/*******************************************************************************
 * Copyright 2019 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files(the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions :
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/
#include "gts/macro_scheduler/schedulers/dynamic/critically_aware_task_scheduling/CriticallyAware_Schedule.h"

#include <queue>
#include <deque>
#include <unordered_set>

#include "gts/platform/Assert.h"

#include "gts/macro_scheduler/Node.h"
#include "gts/macro_scheduler/ComputeResource.h"
#include "gts/macro_scheduler/MacroScheduler.h"
#include "gts/macro_scheduler/compute_resources/MicroScheduler_ComputeResource.h"
#include "gts/macro_scheduler/compute_resources/MicroScheduler_Workload.h"

#define GTS_PARALLEL_UPRANK

namespace {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
struct UpRankTask : gts::Task
{
    //--------------------------------------------------------------------------
    UpRankTask(gts::Node* pNode, uint64_t rankTransform, uint32_t numQueues, uint8_t depth, uint8_t maxDepth)
        : m_pNode(pNode)
        , m_rankTransform(rankTransform)
        , m_numQueues(numQueues)
        , m_depth(depth)
        , m_maxDepth(maxDepth)
    {}

    //--------------------------------------------------------------------------
    virtual gts::Task* execute(gts::TaskContext const& ctx) final
    {
        GTS_TRACE_SCOPED_ZONE_P0(gts::analysis::CaptureMask::MACRO_SCHEDULER_PROFILE, gts::analysis::Color::Gold, "Up-rank Task");

        using namespace gts;

        if (m_depth >= m_maxDepth)
        {
            serialUpRank();
        }
        else
        {
            Vector<Node*>& predecessors = m_pNode->predecessors();

            addRef((uint32_t)predecessors.size() + 1);
            uint32_t spawnedTaskCount = 0;

            for (size_t iNode = 0; iNode < predecessors.size(); ++iNode)
            {
                Node* pPred = predecessors[iNode];

                // It not normalized, normalize the execution cost.
                if (pPred->executionCost() > m_numQueues)
                {
                    pPred->_setExecutionCost(pPred->executionCost() / m_rankTransform + 1);  // +1 map to [1, m_numQueues]

                    // Is first run?
                    if (pPred->executionCost() == 0)
                    {
                        pPred->_setExecutionCost(1);
                    }

                    GTS_ASSERT(pPred->executionCost() <= m_numQueues);
                }

                while (true)
                {
                    uint64_t pCurrRank = m_pNode->rank().load(memory_order::acquire);
                    uint64_t oldRank = pPred->rank().load(memory_order::acquire);
                    uint64_t newRank = pCurrRank + pPred->executionCost();

                    // Does this Node increase its predecessor's rank?
                    if (newRank >= oldRank)
                    {
                        if (pPred->rank().compare_exchange_weak(oldRank, newRank, memory_order::acq_rel, memory_order::relaxed))
                        {
                            Task* pTask = ctx.pMicroScheduler->allocateTask<UpRankTask>(
                                pPred, m_rankTransform, m_numQueues, uint8_t(m_depth + 1), m_maxDepth);

                            addChildTaskWithoutRef(pTask);
                            ctx.pMicroScheduler->spawnTask(pTask);

                            ++spawnedTaskCount;
                            break;
                        }
                    }
                    else
                    {
                        break;
                    }
                }
            }

            removeRef((uint32_t)predecessors.size() - spawnedTaskCount);
            ctx.pThisTask->waitForAll();
        }

        return nullptr;
    }

    //--------------------------------------------------------------------------
    void serialUpRank()
    {
        using namespace gts;

        std::deque<Node*> q;
        q.push_back(m_pNode);

        while (!q.empty())
        {
            Node* pCurr = q.front();
            q.pop_front();

            Vector<Node*>& predecessors = pCurr->predecessors();
            for (size_t iNode = 0; iNode < predecessors.size(); ++iNode)
            {
                Node* pPred = predecessors[iNode];

                // It not normalized, normalize the execution cost.
                if (pPred->executionCost() > m_numQueues)
                {
                    pPred->_setExecutionCost(pPred->executionCost() / m_rankTransform + 1);

                    // Is first run?
                    if (pPred->executionCost() == 0)
                    {
                        pPred->_setExecutionCost(1);
                    }

                    GTS_ASSERT(pPred->executionCost() <= m_numQueues);
                }

                while (true)
                {
                    uint64_t pCurrRank = pCurr->rank().load(memory_order::acquire);
                    uint64_t oldRank = pPred->rank().load(memory_order::acquire);
                    uint64_t newRank = pCurrRank + pPred->executionCost();

                    // Does this Node increase its predecessor's rank?
                    if (newRank >= oldRank)
                    {
                        if (pPred->rank().compare_exchange_weak(oldRank, newRank, memory_order::acq_rel, memory_order::relaxed))
                        {
                            q.push_back(pPred);
                            break;
                        }
                    }
                    else
                    {
                        break;
                    }
                }
            }
        }
    }

    gts::Node* m_pNode;
    const uint64_t m_rankTransform;
    const uint32_t m_numQueues;
    const uint8_t m_depth;
    const uint8_t m_maxDepth;
};

} // namespace

namespace gts {

//------------------------------------------------------------------------------
CriticallyAware_Schedule::CriticallyAware_Schedule(MacroScheduler* pMyScheduler, Node* pBegin, Node* pEnd)
    : Schedule(pMyScheduler)
    , m_pQueuesByCriticalityIndex(nullptr)
    , m_pMaxCostByCriticalityIndex(nullptr)
    , m_pBegin(pBegin)
    , m_pEnd(pEnd)
    , m_numQueues(0)
    , m_rankTransform(1)
    , m_isDone(true)
{
    GTS_ASSERT(pMyScheduler != nullptr);
    GTS_ASSERT(pBegin != nullptr);
    GTS_ASSERT(pEnd != nullptr);

    m_numQueues = (uint32_t)pMyScheduler->computeResources().size();
    GTS_ASSERT(m_numQueues > 0);

    m_pQueuesByCriticalityIndex = alignedVectorNew<ReadyQueue, GTS_NO_SHARING_CACHE_LINE_SIZE>(m_numQueues);
    m_pMaxCostByCriticalityIndex = alignedVectorNew<MaxCost, GTS_NO_SHARING_CACHE_LINE_SIZE>(m_numQueues);

    // Associate respective ComputeResources.
    for (size_t iQueue = 0; iQueue < m_numQueues; ++iQueue)
    {
        m_pQueuesByCriticalityIndex[iQueue].pComputeResource = pMyScheduler->computeResources()[iQueue];
        m_pMaxCostByCriticalityIndex[iQueue].id = pMyScheduler->computeResources()[iQueue]->id();
    }
}

//------------------------------------------------------------------------------
CriticallyAware_Schedule::~CriticallyAware_Schedule()
{
    alignedVectorDelete(m_pMaxCostByCriticalityIndex, m_numQueues);
    alignedVectorDelete(m_pQueuesByCriticalityIndex, m_numQueues);
}

//------------------------------------------------------------------------------
Node* CriticallyAware_Schedule::popNextNode(ComputeResourceId id, ComputeResourceType::Enum type)
{
    Node* pNode = nullptr;
    bool checkedMyQueue = false;
    for (size_t iQueue = 0; iQueue < m_numQueues; ++iQueue)
    {
        ReadyQueue& GTS_NOT_ALIASED readyQueue = m_pQueuesByCriticalityIndex[iQueue];
        if (readyQueue.pComputeResource->id() == id)
        { 
            if (readyQueue.queue.tryPop(pNode))
            {
                return pNode;
            }
            checkedMyQueue = true;
        }
        else if(checkedMyQueue && readyQueue.pComputeResource->type() == type)
        {
            if (readyQueue.queue.tryPop(pNode))
            {
                return pNode;
            }
        }
    }
    return nullptr;
}

//------------------------------------------------------------------------------
void CriticallyAware_Schedule::insertReadyNode(Node* pNode)
{
    GTS_TRACE_SCOPED_ZONE_P0(gts::analysis::CaptureMask::MACRO_SCHEDULER_PROFILE, gts::analysis::Color::Gold, "Insert Ready Node");

    GTS_ASSERT(pNode->rank().load(memory_order::relaxed) != 0);

    pNode->_setCurrentSchedule(this);

    if (pNode->affinity() != ANY_COMP_RESOURCE)
    {
        for (size_t iQueue = 0; iQueue < m_numQueues; ++iQueue)
        {
            ComputeResource* pCompResource = m_pQueuesByCriticalityIndex[iQueue].pComputeResource;
            if (pNode->affinity() == pCompResource->id())
            {
                _insertReadyNode(pNode, m_pQueuesByCriticalityIndex[iQueue]);
                pCompResource->notify(this);
                return;
            }
        }
    }

    size_t bestMatchIdx = SIZE_MAX;

    size_t iQueue = 0;
    for (; iQueue < m_numQueues; ++iQueue)
    {
        ReadyQueue& readyQueue = m_pQueuesByCriticalityIndex[iQueue];
        Node* pMaxPriorityNode = readyQueue.pMaxPriorityNode.load(memory_order::acquire);

        if (readyQueue.pComputeResource->canExecute(pNode))
        {
            bestMatchIdx = iQueue;

            if (pNode->rank().load(memory_order::relaxed) >= readyQueue.maxPriority - m_numQueues &&
                (pMaxPriorityNode == nullptr || pMaxPriorityNode->isChild(pNode)))
            {
                readyQueue.maxPriority = pNode->rank().load(memory_order::relaxed);
                readyQueue.pMaxPriorityNode.store(pNode, memory_order::release);

                _insertReadyNode(pNode, readyQueue);
                _notifyQueuesOfReadyNode(iQueue);
                return;
            }
        }
    }

    GTS_ASSERT(bestMatchIdx != SIZE_MAX);
    if (bestMatchIdx != SIZE_MAX)
    {
        _insertReadyNode(pNode, m_pQueuesByCriticalityIndex[bestMatchIdx]);
        _notifyQueuesOfReadyNode(bestMatchIdx);
    }
}

//------------------------------------------------------------------------------
void CriticallyAware_Schedule::_notifyQueuesOfReadyNode(size_t lastQueueIdx)
{
    GTS_TRACE_SCOPED_ZONE_P0(gts::analysis::CaptureMask::MACRO_SCHEDULER_PROFILE, gts::analysis::Color::Gold, "Notify");
    for (size_t iQueue = 0; iQueue <= lastQueueIdx; ++iQueue)
    {
        m_pQueuesByCriticalityIndex[iQueue].pComputeResource->notify(this);
    }
}

//------------------------------------------------------------------------------
void CriticallyAware_Schedule::observeExecutionCost(ComputeResourceId id, uint64_t cost)
{
    for (size_t ii = 0; ii < m_numQueues; ++ii)
    {
        if (m_pMaxCostByCriticalityIndex[ii].id == id)
        {
            Lock<UnfairSpinMutex<>> lock(m_pMaxCostByCriticalityIndex[ii].mutex);
            if (cost > m_pMaxCostByCriticalityIndex[ii].cost)
            {
                m_pMaxCostByCriticalityIndex[ii].cost = cost;
            }
            break;
        }
    }
}

//------------------------------------------------------------------------------
void CriticallyAware_Schedule::_insertReadyNode(Node* pNode, ReadyQueue& readyQueue)
{
    GTS_TRACE_SCOPED_ZONE_P0(gts::analysis::CaptureMask::MACRO_SCHEDULER_PROFILE, gts::analysis::Color::Gold, "Insert Node");
    readyQueue.maxPriority = pNode->rank().load(memory_order::relaxed);
    readyQueue.pMaxPriorityNode.store(pNode, memory_order::release);
    readyQueue.queue.tryPush(pNode);
}

//------------------------------------------------------------------------------
void CriticallyAware_Schedule::_upRankNodes(Node* pEnd, MicroScheduler_ComputeResource* pUpRankCompResource)
{    
    // Up-rank this Node's predecessors with its previously calculated execution
    // cost. This heuristic relies on a large amount of temporal coherence
    // between each execution of the Node.

#ifdef GTS_PARALLEL_UPRANK
    Task* pTask = pUpRankCompResource->microScheduler()->allocateTask<UpRankTask>(
        pEnd, m_rankTransform, m_numQueues, uint8_t(0), uint8_t(pUpRankCompResource->microScheduler()->workerCount()));

    pUpRankCompResource->microScheduler()->spawnTaskAndWait(pTask);
#else

    GTS_UNREFERENCED_PARAM(pUpRankCompResource);

    std::deque<Node*> q;
    q.push_back(pEnd);

    while (!q.empty())
    {
        Node* pCurr = q.front();
        q.pop_front();

        Vector<Node*>& predecessors = pCurr->predecessors();
        for (size_t iNode = 0; iNode < predecessors.size(); ++iNode)
        {
            Node* pPred = predecessors[iNode];

            // It not normalized, normalize the execution cost.
            if (pPred->executionCost() > m_numQueues)
            {
                pPred->_setExecutionCost(pPred->executionCost() / m_rankTransform + 1);  // +1 map to [1, m_numQueues]
                GTS_ASSERT(pEnd->executionCost() <= m_numQueues);

                // Is first run?
                if (pPred->executionCost() == 0)
                {
                    pPred->_setExecutionCost(1);
                }

                GTS_ASSERT(pPred->executionCost() <= m_numQueues);
            }

            while (true)
            {
                uint64_t pCurrRank = pCurr->rank().load(memory_order::acquire);
                uint64_t oldRank = pPred->rank().load(memory_order::acquire);
                uint64_t newRank = pCurrRank + pPred->executionCost();

                // Does this Node increase its predecessor's rank?
                if (newRank >= oldRank)
                {
                    if (pPred->rank().compare_exchange_weak(oldRank, newRank, memory_order::acq_rel, memory_order::relaxed))
                    {
                        q.push_back(pPred);
                        break;
                    }
                }
                else
                {
                    break;
                }
            }
        }
    }
#endif
}

//------------------------------------------------------------------------------
void CriticallyAware_Schedule::_preProcessNodes(Node* pEnd)
{
    //
    // Find the maximum cost.

    uint64_t maxCost = 0;

    // Max costs are stored per queue from the previous run.
    for (size_t ii = 0; ii < m_numQueues; ++ii)
    {
        if (m_pMaxCostByCriticalityIndex[ii].cost > maxCost)
        {
            maxCost = m_pMaxCostByCriticalityIndex[ii].cost;
        }

        // clear max cost
        m_pMaxCostByCriticalityIndex[ii].cost = 0;
    }

    if (maxCost == 0)
    {
        maxCost = 1;
    }

    //
    // Calculate the rank transform.

    m_rankTransform = gtsMax(uint64_t(1), maxCost / m_numQueues + 1);

    //
    // Set the end Node's transformed cost and rank.

    if (pEnd->executionCost() != 0)
    {
        pEnd->_setExecutionCost(pEnd->executionCost() / m_rankTransform + 1); // +1 map to [1, m_numQueues]
        GTS_ASSERT(pEnd->executionCost() <= m_numQueues);
        pEnd->rank().store(pEnd->executionCost(), memory_order::relaxed);
    }

    // Is first run?
    if (pEnd->rank().load(memory_order::relaxed) == 0)
    {
        pEnd->rank().store(1, memory_order::relaxed);
    }
}

} // namespace gts
