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

#include "gts/micro_scheduler/Task.h"
#include "gts/micro_scheduler/patterns/RangeSplitters.h"

namespace gts {

/** 
 * @addtogroup MicroScheduler
 * @{
 */

/** 
 * @addtogroup ParallelPatterns
 * @{
 */

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  Recursively splits a range until it is no longer divisible.
 */
class SimplePartitioner
{
public:

    using splitter_type = EvenSplitter;

    //--------------------------------------------------------------------------
    SimplePartitioner() = default;

    //--------------------------------------------------------------------------
    template<typename TRange>
    GTS_INLINE SimplePartitioner(SimplePartitioner&, uint16_t, TRange const&) {}

    //--------------------------------------------------------------------------
    template<typename TPattern, typename TRange>
    GTS_INLINE Task* execute(TaskContext const& ctx, TPattern* pPattern, TRange& range)
    {
        // Divide up the range and offer each split as a new task to the scheduler.
        while (range.isDivisible())
        {
            pPattern->initialOffer(ctx, range, splitter_type());
        }
        return doExecute(ctx, pPattern, range, splitter_type());
    }

    //--------------------------------------------------------------------------
    template<typename TPattern, typename TRange>
    GTS_INLINE void initialOffer(TaskContext const& ctx, TPattern* pPattern, TRange& range, splitter_type const& splitter)
    {
        pPattern->offerRange(ctx, range, splitter);
    }

    //--------------------------------------------------------------------------
    template<typename TPattern, typename TRange>
    GTS_INLINE Task* doExecute(TaskContext const& ctx, TPattern* pPattern, TRange& range, splitter_type const& splitter)
    {
        // execute the remaining range.
        pPattern->run(ctx, range, splitter);
        return nullptr;
    }

    //--------------------------------------------------------------------------
    template<typename TRange>
    GTS_INLINE void adjustIfStolen(Task*) {}

    //--------------------------------------------------------------------------
    template<typename TRange>
    GTS_INLINE void initialize(uint16_t) {}

    //--------------------------------------------------------------------------
    template<typename TRange>
    static GTS_INLINE void split() {}

    //--------------------------------------------------------------------------
    template<typename TRange>
    static GTS_INLINE uint16_t getSplit(SimplePartitioner&) { return 0; }

    //--------------------------------------------------------------------------
    GTS_INLINE bool isDivisible() { return true; }
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  Recursively splits a range and tries to limit number of splits
 *  to the number of workers in the executing scheduler.
 */
class StaticPartitioner
{
public:

    using splitter_type = ProportionalSplitter;

    //--------------------------------------------------------------------------
    GTS_INLINE StaticPartitioner()
        : m_initialSplitDepth(1)
    {}

    //--------------------------------------------------------------------------
    template<typename TRange>
    GTS_INLINE StaticPartitioner(StaticPartitioner& partitioner, uint16_t, TRange const&)
        : m_initialSplitDepth(getSplit<TRange>(partitioner))
    {}

    //--------------------------------------------------------------------------
    template< typename TPattern, typename TRange>
    GTS_INLINE Task* execute(TaskContext const& ctx, TPattern* pPattern, TRange& range)
    {
        splitter_type splitter(1, m_initialSplitDepth);

        // Divide up the range proportionally and offer the larger split as a
        // new task to the scheduler.
        while (range.isDivisible() && isDivisible())
        {
            pPattern->initialOffer(ctx, range, splitter);
        }
        return doExecute(ctx, pPattern, range, splitter);
    }

    //--------------------------------------------------------------------------
    template<typename TPattern, typename TRange>
    GTS_INLINE void initialOffer(TaskContext const& ctx, TPattern* pPattern, TRange& range, splitter_type const& splitter)
    {
        pPattern->offerRange(ctx, range, splitter);
    }

    //--------------------------------------------------------------------------
    template<typename TPattern, typename TRange>
    GTS_INLINE Task* doExecute(TaskContext const& ctx, TPattern* pPattern, TRange& range, splitter_type const& splitter)
    {
        // execute the remaining range.
        pPattern->run(ctx, range, splitter);
        return nullptr;
    }

    //--------------------------------------------------------------------------
    template<typename TRange>
    GTS_INLINE void adjustIfStolen(Task*) {}

    //--------------------------------------------------------------------------
    template<typename TRange>
    GTS_INLINE void initialize(uint16_t workerCount)
    {
        m_initialSplitDepth = TRange::adjustDivisor(workerCount, true);
    }

    //--------------------------------------------------------------------------
    template<typename TRange>
    GTS_INLINE void split()
    {
        m_initialSplitDepth = TRange::finalSplitDivisor(m_initialSplitDepth, true);
    }

    //--------------------------------------------------------------------------
    template<typename TRange>
    static GTS_INLINE uint16_t getSplit(StaticPartitioner& partitioner)
    {
        return TRange::splitInitialDepth(partitioner.m_initialSplitDepth, true);
    }

    //--------------------------------------------------------------------------
    GTS_INLINE bool isDivisible()
    {
        return m_initialSplitDepth >= 1;
    }

private:

    //! The number of initial splits encoded as the number of leaves at the
    //! maximum depth of a (TRange::MAX_SPLITS)-tree. That is, splits equals
    //! log_TRange::MAX_SPLITS(m_initialSplitDepth) + 1.
    uint16_t m_initialSplitDepth;
};


#include "AdaptivePartitioner.h"


/** @} */ // end of ParallelPatterns
/** @} */ // end of MicroScheduler

} // namespace gts
