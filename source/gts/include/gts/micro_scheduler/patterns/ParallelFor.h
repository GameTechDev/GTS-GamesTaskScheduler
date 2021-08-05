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

#include "gts/platform/Assert.h"
#include "gts/micro_scheduler/MicroScheduler.h"
#include "gts/micro_scheduler/patterns/Partitioners.h"
#include "gts/micro_scheduler/patterns/Range1d.h"

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
 *  A construct that maps parallel-for behavior to a MicroScheduler.
 */
class ParallelFor
{
public:

    /**
     * Creates a ParallelFor object bound to the specified 'scheduler'. All
     * parallel-for operations will be scheduled with the specified 'priority'.
     */
    GTS_INLINE ParallelFor(MicroScheduler& scheduler, uint32_t priority = 0)
        : m_microScheduler(scheduler)
        , m_priority(priority)
    {}

    /**
     * @brief
     *  Applies the given function 'func' to the specified iteration 'range'.
     * @param range
     *  An iteration range.
     * @param func
     *  The function to apply to each element of range. Signature:
     * @code
     *  void(*)(TRange& range, void* pUserData, TaskContext const&);
     * @endcode
     * @param partitioner
     *  The partitioner object that determines when work is subdivided during
     *  scheduling.
     * @param pUserData
     *  Optional user data that will be passed into func.
     * @param block
     *  Flag true to block until all ParallelFor tasks complete
     */
    template<
        typename TPartitioner,
        typename TRange,
        typename TFunc
    >
    GTS_INLINE void operator()(
        TRange const& range,
        TFunc func,
        TPartitioner partitioner,
        void* pUserData, 
        bool block = true)
    {
        GTS_ASSERT(m_microScheduler.isRunning());

        uint32_t workerCount = m_microScheduler.workerCount();
        partitioner.template initialize<TRange>((uint16_t)workerCount);

        Task* pTask = m_microScheduler.allocateTask<ParallelForTask<TFunc, TRange, TPartitioner>>(
            func, pUserData, range, partitioner, m_priority);

        if (block)
        {
            m_microScheduler.spawnTaskAndWait(pTask, m_priority);
        }
        else
        {
            m_microScheduler.spawnTask(pTask, m_priority);
        }
    }

// 1D CONVENIENCE FUNCTIONS: 

    /**
     * @brief
     *  Applies the given function 'func' to the specified 1D iteration
     *  range [begin, end). Always uses an AdaptivePartitioner with
     *  a minimum block size of 1.
     * @param func
     *  The function to apply to each element of range. Signature:
     * @code
     *  void(*)(TIter iter, void* pUserData, TaskContext const&);
     * @endcode
     */
    template<
        typename TIter,
        typename TFunc,
        typename TPartitioner = AdaptivePartitioner
    >
    GTS_INLINE void operator()(
        TIter begin,
        TIter end,
        TFunc func,
        TPartitioner partitioner = TPartitioner())
    {
        Range1d<TIter> range(begin, end, 1);
        IterationFunc1d<TFunc, Range1d<TIter>> iterFunc(func);

        this->operator()(range, iterFunc, partitioner, nullptr);
    }

private:

    MicroScheduler& m_microScheduler;
    uint32_t m_priority;

private:

    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    template<typename TUserFunc, typename TRange>
    class IterationFunc1d
    {
    public:

        //----------------------------------------------------------------------
        GTS_INLINE IterationFunc1d(TUserFunc& userFunc)
            : m_userFunc(userFunc)
        {}

        //----------------------------------------------------------------------
        GTS_INLINE void operator()(TRange& r, void*, TaskContext const&)
        {
            GTS_SIM_TRACE_MARKER(gts::sim_trace::MARKER_KERNEL_PAR_FOR);
            for (auto ii = r.begin(); ii != r.end(); ++ii)
            {
                m_userFunc(ii);
            }
        }

        TUserFunc& m_userFunc;
    };

    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    /**
     * @details
     *  Derivative of TBB parallel_for. https://github.com/intel/tbb
     */
    template<typename TFunc, typename TRange, typename TPartitioner>
    class ParallelForTask : public Task
    {
    public:

        //----------------------------------------------------------------------
        ParallelForTask(
            TFunc& func,
            void* userData,
            TRange const& range,
            TPartitioner partitioner,
            uint32_t priority)

            : m_func(func)
            , m_pUserData(userData)
            , m_range(range)
            , m_priority(priority)
            , m_partitioner(partitioner)
        {}

        //----------------------------------------------------------------------
        ParallelForTask(ParallelForTask& parent, TRange& range)
            : m_func(parent.m_func)
            , m_pUserData(parent.m_pUserData)
            , m_range(range)
            , m_priority(parent.m_priority)
            , m_partitioner(parent.m_partitioner, 0, TRange())
        {}

        //----------------------------------------------------------------------
        ParallelForTask(ParallelForTask& parent, TRange const& range, uint16_t depth)
            : m_func(parent.m_func)
            , m_pUserData(parent.m_pUserData)
            , m_range(range)
            , m_priority(parent.m_priority)
            , m_partitioner(parent.m_partitioner, depth, TRange())
        {}

        //----------------------------------------------------------------------
        ParallelForTask(ParallelForTask const&) = default;

        //----------------------------------------------------------------------
        ParallelForTask& operator=(ParallelForTask const& other) = default;

        //----------------------------------------------------------------------
        void initialOffer(TaskContext const& ctx, TRange& range, typename TPartitioner::splitter_type const& splitter)
        {
            m_partitioner.initialOffer(ctx, this, range, splitter);
        }

        //----------------------------------------------------------------------
        void offerRange(TaskContext const& ctx, TRange& range, typename TPartitioner::splitter_type const& splitter)
        {
            // Split the range.
            typename TRange::split_result splits;
            range.split(splits, splitter);

            // Allocate the continuation.
            Task* pContinuation = ctx.pMicroScheduler->allocateTask<EmptyTask>();
            pContinuation->addRef(splits.size + 1, gts::memory_order::relaxed);
            setContinuationTask(pContinuation);

            // Add the splits as siblings.
            for (int iSibling = 0; iSibling < splits.size; ++iSibling)
            {
                Task* pSibling = ctx.pMicroScheduler->allocateTask<ParallelForTask>(
                    *this, splits.ranges[iSibling]);

                pContinuation->addChildTaskWithoutRef(pSibling);
                ctx.pMicroScheduler->spawnTask(pSibling);
            }

            // This task becomes a sibling.
            pContinuation->addChildTaskWithoutRef(this);

            m_partitioner.template split<TRange>();
        }

        //----------------------------------------------------------------------
        void offerRange(TaskContext const& ctx, TRange& range, uint16_t depth = 0)
        {
            GTS_ASSERT(!range.empty() && "Bug in partitioner!");

            // Allocate the continuation.
            Task* pContinuation = ctx.pMicroScheduler->allocateTask<EmptyTask>();
            pContinuation->addRef(2, gts::memory_order::relaxed);
            setContinuationTask(pContinuation);

            // The split is the right sibling.
            Task* pSibling = ctx.pMicroScheduler->allocateTask<ParallelForTask>(*this, range, depth);
            pContinuation->addChildTaskWithoutRef(pSibling);
            ctx.pMicroScheduler->spawnTask(pSibling);

            // This task becomes a sibling.
            pContinuation->addChildTaskWithoutRef(this);

            m_partitioner.template split<TRange>();
        }


        //----------------------------------------------------------------------
        void run(TaskContext const& ctx, TRange& range, typename TPartitioner::splitter_type const&)
        {
            GTS_TRACE_SCOPED_ZONE_P0(analysis::CaptureMask::MICRO_SCHEDULER_ALL, analysis::Color::RoyalBlue2, "ParallelFor::run");
            m_func(range, m_pUserData, ctx);
        }

        //----------------------------------------------------------------------
        Task* execute(TaskContext const& ctx)
        {
            GTS_TRACE_SCOPED_ZONE_P0(analysis::CaptureMask::MICRO_SCHEDULER_ALL, analysis::Color::RoyalBlue1, isStolen() ? "S" : "P");

            m_partitioner.template adjustIfStolen<TRange>(this);
            return m_partitioner.execute(ctx, this, m_range);
        }

        //----------------------------------------------------------------------
        void balanceAndExecute(TaskContext const& ctx, TPartitioner& partitioner, TRange& range, typename TPartitioner::splitter_type const& splitter)
        {
            partitioner.balanceAndExecute<>(ctx, this, range, splitter);
        }

    private:

        TFunc&        m_func;
        void*         m_pUserData;
        TRange        m_range;
        uint32_t      m_priority;
        TPartitioner  m_partitioner;
    };
};

/** @} */ // end of ParallelPatterns
/** @} */ // end of MicroScheduler

} // namespace gts
