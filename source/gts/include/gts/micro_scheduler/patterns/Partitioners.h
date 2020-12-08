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
    template<typename TObserverTask, typename TRange>
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
    template<typename TObserverTask, typename TRange>
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

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  Adaptively subdivides a TRange based on demand from the scheduler.
 * @details
 *  The algorithm seeks to keep the generated task tree balance, and increases
 *  the tree depth based on demand.
 */
class AdaptivePartitioner
{
public:

    using splitter_type = EvenSplitter;

    //--------------------------------------------------------------------------
    GTS_INLINE AdaptivePartitioner()
        : m_initialSplitDepth(0)
        , m_maxBalancedSplitDepth(INIT_DEPTH)
        , m_numSiblingThefts(0)
    {}

    //--------------------------------------------------------------------------
    template<typename TRange>
    GTS_INLINE AdaptivePartitioner(AdaptivePartitioner& partitioner, uint16_t depth, TRange const&)
        : m_initialSplitDepth(getSplit<TRange>(partitioner))
        , m_maxBalancedSplitDepth(partitioner.m_maxBalancedSplitDepth)
        , m_numSiblingThefts(0)
    {
        GTS_ASSERT(depth <= m_maxBalancedSplitDepth);

        // Don't split past the depth of our sibling. This keep the task tree
        // balanced.
        m_maxBalancedSplitDepth -= depth;
    }

    //--------------------------------------------------------------------------
    template<typename TPattern, typename TRange>
    GTS_INLINE Task* execute(TaskContext const& ctx, TPattern* pPattern, TRange& range)
    {
        // Divide up the range while its minimum size has not been reached. Queue
        // each division as new continuation and sibling tasks.
        while (range.isDivisible() && isDivisible())
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
        GTS_TRACE_SCOPED_ZONE_P1(analysis::CaptureMask::MICRO_SCHEDULER_PROFILE, analysis::Color::RoyalBlue1, "ADAPTIVE doExecute", m_maxBalancedSplitDepth);

        // Split the range if there is an remaining initial split.
        if (m_initialSplitDepth == 1)
        {
            m_initialSplitDepth = 0;
            pPattern->balanceAndExecute(ctx, *this, range, splitter);
        }
        else
        {
            // Execute any remaining work.
            pPattern->run(ctx, range, splitter);
        }

        return nullptr;
    }

    //--------------------------------------------------------------------------
    template<typename TObserverTask, typename TPattern, typename TRange>
    GTS_INLINE void balanceAndExecute(
        TaskContext const& ctx,
        TPattern* pPattern,
        TRange& range,
        splitter_type const& splitter)
    {
        // Split the range if there is an remaining initial split.
        if (m_initialSplitDepth == 1)
        {
            m_initialSplitDepth = 0;

            uint16_t splitDepth = 0;

            // Split to depth to balance the Task tree.
            while (splitDepth < m_maxBalancedSplitDepth && range.isDivisible())
            {
                GTS_TRACE_SCOPED_ZONE_P0(analysis::CaptureMask::MICRO_SCHEDULER_PROFILE, analysis::Color::PaleVioletRed4, "ADAPTIVE SPLIT");

                typename TRange::split_result splits;
                range.split(splits, splitter);
                ++splitDepth;

                for (int ii = 0; ii < splits.size; ++ii)
                {
                    pPattern->offerRange(ctx, splits.ranges[ii], splitDepth);
                }
            }
        }

        // Execute any remaining work.
        pPattern->run(ctx, range, splitter);
    }

    //--------------------------------------------------------------------------
    template<typename TObserverTask, typename TRange>
    GTS_INLINE bool isSiblingStolen(Task* pThisTask)
    {
        TObserverTask* pObserver = (TObserverTask*)pThisTask->parent();
        return pObserver->m_childTaskStolen.load(memory_order::acquire);
    }

    //--------------------------------------------------------------------------
    template<typename TObserverTask, typename TRange>
    GTS_INLINE void adjustIfStolen(Task* pThisTask)
    {
        // Only consider Tasks that are not part of the initial split phase.
        if (m_initialSplitDepth == 0)
        {
            // Allow the Task to split to m_maxBalancedSplitDepth.
            m_initialSplitDepth = 1;

            if (pThisTask->isStolen() &&
                // Communicate the theft only if there is another child.
                pThisTask->parent()->refCount() > 2)
            {
                TObserverTask* pObserver = (TObserverTask*)pThisTask->parent();
                pObserver->m_childTaskStolen.exchange(true, memory_order::acq_rel);

                ++m_numSiblingThefts;
                if (m_maxBalancedSplitDepth == 0)
                {
                    ++m_numSiblingThefts;
                };

                adjustDepth<TRange>();
            }
        }
    }

    //--------------------------------------------------------------------------
    template<typename TRange>
    GTS_INLINE void initialize(uint16_t workerCount)
    {
        m_initialSplitDepth = TRange::adjustDivisor(nextPow2(workerCount), false);
        m_maxBalancedSplitDepth = INIT_DEPTH;
    }

    //--------------------------------------------------------------------------
    template<typename TRange>
    GTS_INLINE void split()
    {
        m_initialSplitDepth = TRange::splitInitialDepth(m_initialSplitDepth, false);
    }

    //--------------------------------------------------------------------------
    template<typename TRange>
    static GTS_INLINE uint16_t getSplit(AdaptivePartitioner& partitioner)
    {
        return TRange::splitInitialDepth(partitioner.m_initialSplitDepth, false);
    }

    //--------------------------------------------------------------------------
    template<typename TRange>
    GTS_INLINE void adjustDepth()
    {
        // Increment the depth if the number of sibling thefts is beyond the
        // maximum split threshold. We do this so that a depth increment represents
        // all children for the next depth.
        if (m_numSiblingThefts >= TRange::MAX_SPLITS)
        {
            m_maxBalancedSplitDepth++;
            m_numSiblingThefts = 0;
        }
    }

    //--------------------------------------------------------------------------
    GTS_INLINE bool isDivisible()
    {
        return m_initialSplitDepth > 1;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE uint16_t maxBalancedSplitDepth() const
    {
        return m_maxBalancedSplitDepth;
    }

private:

    enum { INIT_DEPTH = 3 };

    //! The number of initial splits encoded as the number of leaves at the
    //! maximum depth of a (TRange::MAX_SPLITS)-tree. That is, splits equals
    //! log_TRange::MAX_SPLITS(m_initialSplitDepth).
    uint16_t m_initialSplitDepth;

    //! The current maximum depth needed to produce a balanced task tree.
    uint16_t m_maxBalancedSplitDepth;

    //! The number of sibling Task thefts between depth increments.
    uint16_t m_numSiblingThefts;
};

/** @} */ // end of ParallelPatterns
/** @} */ // end of MicroScheduler

} // namespace gts