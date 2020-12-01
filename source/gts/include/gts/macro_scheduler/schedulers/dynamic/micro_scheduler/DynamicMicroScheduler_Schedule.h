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

#include "gts/containers/parallel/QueueMPMC.h"
#include "gts/macro_scheduler/Schedule.h"

namespace gts {

class Task;

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
 * @addtogroup DynamicMicroScheduler
 * @{
 */

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A schedule produced by a DynamicMicroScheduler_MacroScheduler.
 */
class DynamicMicroScheduler_Schedule : public Schedule
{
public: // STRUCTORS:

    DynamicMicroScheduler_Schedule(MacroScheduler* pMyScheduler, Node* pBegin, Node* pEnd);

public: // ACCESSORS:

    /**
     * @returns True if the schedule is completed.
     */
    GTS_INLINE virtual bool isDone() const final { return m_isDone.load(memory_order::acquire); }

    /**
     * @returns The first Node.
     */
    virtual Node* popNextNode(ComputeResourceId, ComputeResourceType::Enum) final;

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
     * Unused.
     */
    GTS_INLINE virtual void insertReadyNode(Node*) final {}

    /**
     * Unused.
     */
    GTS_INLINE virtual void rankNode(Node*) final {}

    /**
     * Unused.
     */
    GTS_INLINE virtual void observeExecutionCost(ComputeResourceId, uint64_t) final {}

private:

    friend class DynamicMicroScheduler_MacroScheduler;

    //! The last Node in the Schedule.
    Node* m_pBegin;

    //! The last Node in the Schedule.
    Node* m_pEnd;

    Atomic<bool> m_isDone;
};

/** @} */ // end of DynamicMicroScheduler
/** @} */ // end of Schedulers
/** @} */ // end of Implementations
/** @} */ // end of MacroScheduler

} // namespace gts
