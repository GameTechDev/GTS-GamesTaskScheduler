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
#include "gts/macro_scheduler/schedulers/heterogeneous/critical_node_task_scheduling/CriticalNode_Schedule.h"

#include <chrono>
#include <stack>
#include <queue>
#include <algorithm>

#include "gts/platform/Assert.h"

#include "gts/containers/RingDeque.h"

#include "gts/macro_scheduler/Node.h"
#include "gts/macro_scheduler/ComputeResource.h"
#include "gts/macro_scheduler/MacroScheduler.h"
#include "gts/macro_scheduler/compute_resources/MicroScheduler_ComputeResource.h"
#include "gts/macro_scheduler/compute_resources/MicroScheduler_Workload.h"
#include "gts/macro_scheduler/DagUtils.h"

#define GTS_PARALLEL_UPRANKx

#ifdef GTS_MSVC
#pragma warning(push)
#pragma warning(disable : 4324) // alignment padding warning
#endif

namespace {

struct alignas(GTS_NO_SHARING_CACHE_LINE_SIZE) UpRank
{
    gts::Atomic<uint64_t> v = { 0 };
};

}

namespace gts {

//------------------------------------------------------------------------------
CriticalNode_Schedule::CriticalNode_Schedule(MacroScheduler* pMyScheduler, Node* pSource, Node* pSink)
    : Schedule(pMyScheduler)
    , m_pSource(pSource)
    , m_pSink(pSink)
    , m_rankTransform(1)
    , m_isDone(true)
{
    GTS_ASSERT(pMyScheduler != nullptr);
    GTS_ASSERT(pSource != nullptr);
    GTS_ASSERT(pSink != nullptr);
    GTS_ASSERT(pMyScheduler->computeResources().size() > 0);

    auto computeResources = pMyScheduler->computeResources();

    // Create queues for each ComputeResource's processors. Skip the last
    // ComputeResource, which will only have one queue.
    for (size_t ii = 0; ii < computeResources.size() - 1; ++ii)
    {
        auto* pCompResource = computeResources[ii];
        for (uint32_t jj = 0; jj < pCompResource->processorCount(); ++jj)
        {
            m_queuesByRank.push_back({ pCompResource, QueueMPMC<Node*>() });
        }
    }

    // The last ComputeResource's queue.
    m_queuesByRank.push_back({ computeResources.back(), QueueMPMC<Node*>() });

    // Initialize Nodes.
    std::queue<Node*> q;
    q.push(pSource);
    while (!q.empty())
    {
        Node* pCurr = q.front(); q.pop();

        // If pCurr has not been executed, set its cost to 1.
        if (pCurr->executionCost() == 0)
        {
            pCurr->_setExecutionCost(1);
        }

        for (Node* pSucc : pCurr->successors())
        {
            q.push(pSucc);
        }
    }
}

//------------------------------------------------------------------------------
Node* CriticalNode_Schedule::popNextNode(ComputeResource* pComputeResource, bool myQueuesOnly)
{
    GTS_TRACE_SCOPED_ZONE_P1(gts::analysis::CaptureMask::MACRO_SCHEDULER_PROFILE, gts::analysis::Color::Gold, "Pop Next Node", myQueuesOnly);
    Node* pNode = nullptr;
    size_t end;
    if (myQueuesOnly)
    {
        end = gtsMin(size_t(pComputeResource->maxRank() + pComputeResource->processorCount()), m_queuesByRank.size());
    }
    else
    {
        end = m_queuesByRank.size();
    }
    for (size_t iQueue = pComputeResource->maxRank(); iQueue < end; ++iQueue)
    {
        ReadyQueue& GTS_NOT_ALIASED readyQueue = m_queuesByRank[iQueue];
        if(readyQueue.pComputeResource->type() == pComputeResource->type())
        {
            if (readyQueue.queue.tryPop(pNode))
            {
                GTS_TRACE_SCOPED_ZONE_P1(gts::analysis::CaptureMask::MACRO_SCHEDULER_PROFILE, gts::analysis::Color::Gold, "Popped Node From", iQueue);
                return pNode;
            }
        }
    }
    return nullptr;
}

//------------------------------------------------------------------------------
void CriticalNode_Schedule::insertReadyNode(Node* pNode)
{
    GTS_TRACE_SCOPED_ZONE_P0(gts::analysis::CaptureMask::MACRO_SCHEDULER_PROFILE, gts::analysis::Color::Gold, "Insert Ready Node");

    pNode->_setCurrentSchedule(this);

    // Node has affinity so ship it to that queue.
    if (pNode->affinity() != ANY_COMP_RESOURCE)
    {
        for (size_t iQueue = 0; iQueue < m_queuesByRank.size(); ++iQueue)
        {
            ComputeResource* pCompResource = m_queuesByRank[iQueue].pComputeResource;
            if (pNode->affinity() == pCompResource->id())
            {
                GTS_TRACE_SCOPED_ZONE_P1(gts::analysis::CaptureMask::MACRO_SCHEDULER_PROFILE, gts::analysis::Color::Gold, "Inserting Affinity Node At Queue", iQueue);
                _insertReadyNode(pNode, m_queuesByRank[iQueue]);
                pCompResource->notify(this);
                return;
            }
        }
    }

    size_t bestMatchIdx = SIZE_MAX;
    size_t downRank = m_queuesByRank.size() - (size_t)pNode->downRank().load(memory_order::relaxed) - 1;

    // Look through all queues for best match because the rank the Node received
    // my not be associated with an executable queue. 
    for (size_t iQueue = 0; iQueue < m_queuesByRank.size(); ++iQueue)
    {
        ReadyQueue& readyQueue = m_queuesByRank[iQueue];

        if (readyQueue.pComputeResource->canExecute(pNode))
        {
            bestMatchIdx = iQueue;

            if (downRank <= iQueue)
            {
                GTS_TRACE_SCOPED_ZONE_P1(gts::analysis::CaptureMask::MACRO_SCHEDULER_PROFILE, gts::analysis::Color::Gold, "Inserting Node At Queue", iQueue);
                _insertReadyNode(pNode, readyQueue);
                _notifyQueuesOfReadyNode(iQueue);
                return;
            }
        }
    }

    // No ideal queue found so fall back to best match.
    GTS_ASSERT(bestMatchIdx != SIZE_MAX);
    if (bestMatchIdx != SIZE_MAX)
    {
        GTS_TRACE_SCOPED_ZONE_P1(gts::analysis::CaptureMask::MACRO_SCHEDULER_PROFILE, gts::analysis::Color::Orange, "Inserting Best Matched Node At Queue", bestMatchIdx);
        _insertReadyNode(pNode, m_queuesByRank[bestMatchIdx]);
        _notifyQueuesOfReadyNode(bestMatchIdx);
    }
}

//------------------------------------------------------------------------------
void CriticalNode_Schedule::_notifyQueuesOfReadyNode(size_t lastQueueIdx)
{
    GTS_TRACE_SCOPED_ZONE_P0(gts::analysis::CaptureMask::MACRO_SCHEDULER_PROFILE, gts::analysis::Color::Gold, "Notify");
    for (auto pCompResource: getScheduler()->computeResources())
    {
        if (pCompResource->maxRank() <= lastQueueIdx)
        {
            pCompResource->notify(this);
        }
    }
}

//------------------------------------------------------------------------------
void CriticalNode_Schedule::_insertReadyNode(Node* pNode, ReadyQueue& readyQueue)
{
    GTS_TRACE_SCOPED_ZONE_P0(gts::analysis::CaptureMask::MACRO_SCHEDULER_PROFILE, gts::analysis::Color::Gold, "Insert Node");
    readyQueue.queue.tryPush(pNode);
}

//------------------------------------------------------------------------------
void CriticalNode_Schedule::_rankNodes(
    Vector<ComputeResource*> const& computeResources,
    Node* pSource,
    Node* pSink)
{
    bool wasRanked = true;
    uint32_t currRank = (uint32_t)m_queuesByRank.size() - 1;

    // Rank for all but the last ComputeResource, which is always 0.
    for (size_t ii = 0; ii < computeResources.size() - 1 && wasRanked; ++ii)
    {
        // NOTE: Ideally we'd do a ranking pass per processor, however this is
        // a big upfront cost which doesn't seem to add any value in practice
        uint32_t numToRank = computeResources[ii]->processorCount();

        //{
        //    auto start = std::chrono::high_resolution_clock::now();
        _upRankNodes(pSink);
        //    auto end = std::chrono::high_resolution_clock::now();

        //    std::chrono::duration<double> diff = end - start;
        //    printf("UpRank: %f\n", diff.count());
        //}
        //DagUtils::printToDot("upRankedNodes.gv", pSource, DagUtils::NodePropertyFlags(DagUtils::NAME | DagUtils::UPRANK));
        //{
        //    auto start = std::chrono::high_resolution_clock::now();
        wasRanked = _downRankNodes(pSource, currRank, numToRank);
        //    auto end = std::chrono::high_resolution_clock::now();

        //    std::chrono::duration<double> diff = end - start;
        //    printf("DownRank: %f\n", diff.count());
        //}

        currRank -= numToRank;
    }
}

//------------------------------------------------------------------------------
void CriticalNode_Schedule::_upRankNodes(Node* pSink)
{    
    GTS_TRACE_SCOPED_ZONE_P0(gts::analysis::CaptureMask::MACRO_SCHEDULER_PROFILE, gts::analysis::Color::Gold, "Up-rank nodes");

    // Up-downRank this Node's predecessors with its previously calculated execution
    // cost. This heuristic relies on a large amount of temporal coherence
    // between each execution of the Node.

    // Init the sink's downRank.
    if (pSink->downRank().load(memory_order::relaxed) == 0)
    {
        pSink->upRank().store(pSink->executionCost(), memory_order::relaxed);
    }

    std::stack<Node*> q;
    q.push(pSink);

    while (!q.empty())
    {
        Node* pCurr = q.top();
        q.pop();

        Vector<Node*>& predecessors = pCurr->predecessors();
        for (size_t iNode = 0; iNode < predecessors.size(); ++iNode)
        {
            Node* pPred = predecessors[iNode];

            while (true)
            {
                uint64_t pPredOldRank = pPred->upRank().load(memory_order::relaxed);
                uint64_t pCurrRank = pCurr->upRank().load(memory_order::relaxed);
                uint64_t pPredNewRank = 0;

                // If not up-ranked this pass and not previously ranked, add exe cost.
                if (pPredOldRank == 0 && pPred->downRank().load(memory_order::relaxed) == 0)
                {
                    pPredNewRank = pPred->executionCost();
                }

                pPredNewRank += pCurrRank;

                // Does this Node increase its predecessor's downRank?
                if (pPredNewRank > pPredOldRank)
                {
                    if (pPred->upRank().compare_exchange_weak(pPredOldRank, pPredNewRank, memory_order::acq_rel, memory_order::relaxed))
                    {
                        q.push(pPred);
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

//------------------------------------------------------------------------------
bool CriticalNode_Schedule::_downRankNodes(Node* pSource, uint32_t downRank, uint32_t numToRank)
{
    GTS_TRACE_SCOPED_ZONE_P0(gts::analysis::CaptureMask::MACRO_SCHEDULER_PROFILE, gts::analysis::Color::Gold, "Down-rank nodes");

    static auto nodesGreater = [](Node* a, Node* b) {
        return a->upRank().load(memory_order::relaxed) > b->upRank().load(memory_order::relaxed);
    };
    std::priority_queue<Node*, std::vector<Node*>, decltype(nodesGreater)> nodesAtDepth(nodesGreater);

    int currDepth = 0;
    bool wasNodeRanked = false;

    RingDeque<std::pair<Node*, int>> q;
    q.push_back({ pSource, currDepth });

    while (!q.empty())
    {
        auto curr = q.front();
        q.pop_front();

        if (curr.second > currDepth)
        {
            uint32_t currRank = downRank;
            while (!nodesAtDepth.empty())
            {
                Node* pNode = nodesAtDepth.top(); nodesAtDepth.pop();
                pNode->upRank().store(0, memory_order::relaxed);
                pNode->downRank().store(currRank, memory_order::relaxed);
                wasNodeRanked = true;
                --currRank;
            }
            ++currDepth;
        }

        if (curr.first->downRank().load(memory_order::relaxed) == 0)
        {
            nodesAtDepth.push(curr.first);

            // Only save the number of Nodes to rank.
            if (nodesAtDepth.size() > numToRank)
            {
                // Done with Node so clear up-rank for next pass.
                nodesAtDepth.top()->upRank().store(0, memory_order::relaxed);
                nodesAtDepth.pop();
            }
        }

        for (auto pSucc : curr.first->successors())
        {
            q.push_back({ pSucc, curr.second + 1 });
        }
    }

    uint32_t currRank = downRank;
    while (!nodesAtDepth.empty())
    {
        Node* pNode = nodesAtDepth.top(); nodesAtDepth.pop();
        pNode->downRank().store(currRank, memory_order::relaxed);
        wasNodeRanked = true;
        --currRank;
    }
    ++currDepth;

    return wasNodeRanked;
}

} // namespace gts

#ifdef GTS_MSVC
#pragma warning(pop)
#endif
