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
#pragma once

#include <unordered_set>

#include "gts/platform/Machine.h"

#include "gts/containers/parallel/QueueMPMC.h"
#include "gts/macro_scheduler/Schedule.h"

namespace gts {

class Task;
class MicroScheduler_ComputeResource;

/** 
 * @addtogroup MacroScheduler
 * @{
 */

/** 
 * @addtogroup Implementations
 * @{
 */

/** 
 * @addtogroup Schedulers
 * @{
 */

/** 
 * @addtogroup CriticallyAwareTaskScheduling
 * @{
 */

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A schedule produced by a CriticallyAware_MacroScheduler.
 */
class CriticallyAware_Schedule : public Schedule
{
public: // STRUCTORS:

    CriticallyAware_Schedule(MacroScheduler* pMyScheduler, Node* pBegin, Node* pEnd);
    ~CriticallyAware_Schedule();

public: // ACCESSORS:

    /**
     * @returns True if the schedule is completed.
     */
    GTS_INLINE virtual bool isDone() const final { return m_isDone.load(memory_order::acquire); }

    /**
     * @returns The next Node to execute.
     */
    virtual Node* popNextNode(ComputeResourceId id, ComputeResourceType::Enum type) final;

public: // MUTATORS:

    /**
     * If 'pNode' is the last Node, mark the schedule as done.
     */
    GTS_INLINE virtual void tryMarkDone(Node* pNode) final
    {
        if (pNode == m_pEnd)
        {
            m_isDone.exchange(true, memory_order::acq_rel);
        }
    }

    /**
     * Inserts a ready Node into the schedule.
     */
    virtual void insertReadyNode(Node* pNode) final;

    virtual void observeExecutionCost(ComputeResourceId id, uint64_t cost) final;

private:

    friend class CriticallyAware_MacroScheduler;

    struct ReadyQueue
    {
        //! The ComputeResource associated with this queue.
        ComputeResource* pComputeResource = nullptr;

        //! All the Nodes ready to be executed.
        QueueMPMC<Node*> queue;

        //! The maximum priority encountered during an execution.
        uint64_t maxPriority = 0;

        //! The maximum priority encountered during an execution.
        Atomic<Node*> pMaxPriorityNode = {nullptr};
    };

    struct MaxCost
    {
        ComputeResourceId id;
        uint64_t cost = 0;
        UnfairSpinMutex<> mutex;
        uint8_t pad[GTS_NO_SHARING_CACHE_LINE_SIZE];
    };

    void _insertReadyNode(Node* pNode, ReadyQueue& readyQueue);

    void _notifyQueuesOfReadyNode(size_t lastQueueIdx);

    void _upRankNodes(Node* pEnd, MicroScheduler_ComputeResource* pUpRankCompResource);

    void _preProcessNodes(Node* pEnd);

    //! Queues representing categories of critical paths based on the number
    //! ComputeResources in assigned to the MacroScheduler that created this
    //! Schedule. m_pQueuesByCriticalityIndex[m_numQueues-1] is the non-critical
    //! queue.
    ReadyQueue* m_pQueuesByCriticalityIndex;

    //! Maximum Node cost per ComputeResource. Indices same as m_pQueuesByCriticalityIndex.
    MaxCost* m_pMaxCostByCriticalityIndex;

    //! The last Node in the Schedule.
    Node* m_pBegin;

    //! The last Node in the Schedule.
    Node* m_pEnd;

    Vector<Node*> m_topoSortedNodes;

    //! The number of queues in m_pQueuesByCriticalityIndex.
    uint32_t m_numQueues;

    //! Value that transforms an execution time into a ComputeResoruce rank.
    uint64_t m_rankTransform;

    // Flags the Schedule as executed.
    Atomic<bool> m_isDone;
};

/** @} */ // end of CriticallyAwareTaskScheduling
/** @} */ // end of Schedulers
/** @} */ // end of Implementations
/** @} */ // end of MacroScheduler

} // namespace gts
