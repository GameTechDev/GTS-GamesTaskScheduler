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

#include <algorithm>
#include "gts/micro_scheduler/Task.h"

namespace gts {


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A continuation task that marks if a child has been stolen.
 */
class TheftObserverTask
{
public:

    //--------------------------------------------------------------------------
    GTS_INLINE TheftObserverTask()
        : childTaskStolen(false)
    {}

    //--------------------------------------------------------------------------
    static GTS_INLINE Task* taskFunc(Task*, TaskContext const&)
    {
        return nullptr;
    }

    gts::Atomic<bool> childTaskStolen;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  Recursively divides a range in half until it grain size is reached.
 */
class StaticPartitioner
{
public:

    //--------------------------------------------------------------------------
    template<typename TPattern, typename TRange>
    GTS_INLINE Task* execute(Task* pThisTask, TaskContext const& ctx, TPattern* pPattern, TRange& range)
    {
        while (range.isDivisible())
        {
            Task* pContinuation = pPattern->offerRange(pThisTask, ctx, range.split());
            pContinuation->addChildTask(pThisTask);
        }

        // executed the remaining range.
        pPattern->run(ctx, range);
        return nullptr;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE bool adjustIfStolen(Task*) { return false; }

    //--------------------------------------------------------------------------
    GTS_INLINE void setWorkerCount(uint8_t) {}

    //--------------------------------------------------------------------------
    GTS_INLINE StaticPartitioner split(uint8_t) { return *this; }
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  Recursively divides a range based on demand from the scheduler.
 */
class AdaptivePartitioner
{
public:

    //--------------------------------------------------------------------------
    GTS_INLINE AdaptivePartitioner()
        : m_initialOfferingDepth(0)
    {}

    //--------------------------------------------------------------------------
    GTS_INLINE explicit AdaptivePartitioner(uint8_t initialDepth, uint8_t)
        : m_initialOfferingDepth(initialDepth)
    {}

    //--------------------------------------------------------------------------
    template<typename TPattern, typename TRange>
    GTS_INLINE Task* execute(Task* pThisTask, TaskContext const& ctx, TPattern* pPattern, TRange& range)
    {
        // Divide up the range until its tile size has not been reached. Queue
        // each division as a new continuation and sibling tasks.
        while (range.isDivisible() && m_initialOfferingDepth > 0)
        {
            Task* pContinuation = pPattern->offerRange(pThisTask, ctx, range.split(), 0);
            pContinuation->addChildTask(pThisTask);
        }

        if (range.isDivisible())
        {
            // If the sibling to this task was stolen, then there is demand for
            // more work.
            TheftObserverTask* pObserver = (TheftObserverTask*)pThisTask->parent()->getData();
            if (pObserver->childTaskStolen.load(gts::memory_order::acquire))
            {
                Task* pContinuation = pPattern->offerRange(
                    pThisTask,
                    ctx,
                    range.split(),
                    0);

                pContinuation->addChildTask(pThisTask);
            }
        }

        // Execute the remaining range.
        pPattern->run(ctx, range);

        return nullptr;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE bool adjustIfStolen(Task* pThisTask)
    {
        if (m_initialOfferingDepth == 0)
        {
            TheftObserverTask* pObserver = (TheftObserverTask*)pThisTask->parent()->getData();
            if (pObserver->childTaskStolen.load(gts::memory_order::acquire))
            {
                pObserver->childTaskStolen.store(true, gts::memory_order::release);

                return true;
            }
        }
        return false;
    }


    //--------------------------------------------------------------------------
    GTS_INLINE AdaptivePartitioner split(uint8_t depth)
    {
        --m_initialOfferingDepth;
        return AdaptivePartitioner(m_initialOfferingDepth, depth);
    }

    //--------------------------------------------------------------------------
    GTS_INLINE void setWorkerCount(uint8_t workerCount)
    {
        m_initialOfferingDepth = workerCount / 2;
    }

private:

    uint8_t m_initialOfferingDepth;
};

} // namespace gts
