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

#include "gts/platform/Machine.h"
#include "gts/synchronization/SpinMutex.h"
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
 * @addtogroup CriticalNodeTaskScheduling
 * @{
 */

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A schedule produced by a CriticalNode_MacroScheduler.
 */
class CriticalNode_Schedule : public Schedule
{
public: // STRUCTORS:

    CriticalNode_Schedule(MacroScheduler* pMyScheduler, Node* pSource, Node* pSink);

public: // ACCESSORS:

    /**
     * @returns True if the schedule is completed.
     */
    GTS_INLINE virtual bool isDone() const final { return m_isDone.load(memory_order::acquire); }

    /**
     * @returns The next Node to execute.
     */
    virtual Node* popNextNode(ComputeResource* pCompResource, bool myQueuesOnly) final;

public: // MUTATORS:

    /**
     * If 'pNode' is the last Node, mark the schedule as done.
     */
    GTS_INLINE virtual void tryMarkDone(Node* pNode) final
    {
        if (pNode == m_pSink)
        {
            m_isDone.exchange(true, memory_order::acq_rel);
        }
    }

    /**
     * Inserts a ready Node into the schedule.
     */
    virtual void insertReadyNode(Node* pNode) final;

private:

    friend class CriticalNode_MacroScheduler;

    struct ReadyQueue
    {
        //! The ComputeResource associated with this queue.
        ComputeResource* pComputeResource = nullptr;

        //! All the Nodes ready to be executed.
        QueueMPMC<Node*> queue;
    };

    void _insertReadyNode(Node* pNode, ReadyQueue& readyQueue);

    void _notifyQueuesOfReadyNode(size_t lastQueueIdx);

    void _rankNodes(
        Vector<ComputeResource*> const& computeResources,
        Node* pSource,
        Node* pSink);

    void _upRankNodes(Node* pSink);

    bool _downRankNodes(Node* pSource, uint32_t rank, uint32_t numToRank);

    //! Ready Node queues ordered by rank.
    Vector<ReadyQueue> m_queuesByRank;

    //! The first Node in the Schedule.
    Node* m_pSource;

    //! The last Node in the Schedule.
    Node* m_pSink;

    //! Value that transforms an execution time into a ComputeResoruce rank.
    uint64_t m_rankTransform;

    // Flags the Schedule as executed.
    Atomic<bool> m_isDone;
};

/** @} */ // end of CriticalNodeTaskScheduling
/** @} */ // end of Schedulers
/** @} */ // end of Implementations
/** @} */ // end of MacroScheduler

} // namespace gts
