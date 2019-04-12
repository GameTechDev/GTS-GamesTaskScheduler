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
 *  Recursively divides a range in half until it grain size is reached.
 */
class StaticPartitioner
{
public:

    //--------------------------------------------------------------------------
    template<typename TObserverTask, typename TPattern, typename TRange>
    GTS_INLINE Task* execute(Task* pThisTask, TaskContext const& ctx, TPattern* pPattern, TRange& range)
    {
        while (range.isDivisible())
        {
            pPattern->offerRange(pThisTask, ctx, range.split());
        }

        // executed the remaining range.
        pPattern->run(ctx, range);
        return nullptr;
    }

    //--------------------------------------------------------------------------
    template<typename TObserverTask>
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
    template<typename TObserverTask, typename TPattern, typename TRange>
    GTS_INLINE Task* execute(Task* pThisTask, TaskContext const& ctx, TPattern* pPattern, TRange& range)
    {
        // Divide up the range until its tile size has not been reached. Queue
        // each division as a new continuation and sibling tasks.
        while (range.isDivisible() && m_initialOfferingDepth > 0)
        {
            pPattern->offerRange(pThisTask, ctx, range.split(), 0);
        }

        if (range.isDivisible())
        {
            // If the sibling to this task was stolen, then there is demand for
            // more work.
            TObserverTask* pObserver = (TObserverTask*)pThisTask->parent()->getData();
            if (pObserver->m_childTaskStolen.load(gts::memory_order::acquire))
            {
                pPattern->offerRange(pThisTask, ctx, range.split(), 0);
            }
        }

        // Execute the remaining range.
        pPattern->run(ctx, range);

        return nullptr;
    }

    //--------------------------------------------------------------------------
    template<typename TObserverTask>
    GTS_INLINE bool adjustIfStolen(Task* pThisTask)
    {
        if (m_initialOfferingDepth == 0)
        {
            TObserverTask* pObserver = (TObserverTask*)pThisTask->parent()->getData();
            if (pObserver->m_childTaskStolen.load(gts::memory_order::acquire))
            {
                pObserver->m_childTaskStolen.store(true, gts::memory_order::release);

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
        // Experimental evidence suggests an initial offering of half the worker
        // count rounded to the next even number with a minimum of 4. 
        // TODO: is there a more precise model?
        m_initialOfferingDepth = (uint8_t)std::max(4, (workerCount / 4) * 2 + 2);
    }

private:

    uint8_t m_initialOfferingDepth;
};

} // namespace gts
