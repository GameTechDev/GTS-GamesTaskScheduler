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
#include "gts/micro_scheduler/patterns/Range1d.h"
#include "gts/micro_scheduler/patterns/Partitioners.h"

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
 *  A construct that maps parallel-reduce behavior to a MicroScheduler.
 */
class ParallelReduce
{
public:

    /**
     * Creates a ParallelReduce object bound to the specified 'scheduler'. All
     * parallel-reduce operations will be scheduled with the specified 'priority'.
     */
    GTS_INLINE ParallelReduce(MicroScheduler& scheduler, uint32_t priority = 0)
        : m_microScheduler(scheduler)
        , m_priority(priority)
    {}

    /**
     * Combines all the values in 'range' via 'reduceFunc'.
     * @param range
     *  The divisible range to iterate over.
     * @param mapReduceFunc
     *  The function to execute on 'range'. Requires the signature:
     *  TResultValue mapReduceFunc(TRange range, void* userData, TaskContext const&).
     *  NOTE: This can be more that just a reduction operation. For instance one
     *  could fuse map-reduce here.
     * @param joinFunc
     *  The function that joins the results of mapReduceFunc to produce the final
     *  reduction value. Requires the signature:
     *  TResultValue joinFunc(TResultValue const& lhs, TResultValue const& rhs, void* userData, TaskContext const&).
     * @param identityValue
     *  An identity value for the join function.
     * @param partitioner
     *  The partitioner object that determines when work is subdivided during
     *  scheduling.
     * @param userData
     *  Optional data to be used in the passed in functions.
     * @return
     *  The combined result.
     */
    template<
        typename TRange,
        typename TResultValue,
        typename TMapReduceFunc,
        typename TJoinFunc,
        typename TPartitioner = AdaptivePartitioner
    >
    GTS_INLINE TResultValue operator()(
        TRange const& range,
        TMapReduceFunc mapReduceFunc,
        TJoinFunc joinFunc,
        TResultValue identityValue,
        TPartitioner partitioner = AdaptivePartitioner(),
        void* userData = nullptr)
    {
        GTS_ASSERT(m_microScheduler.isRunning());

        partitioner.template initialize<TRange>((uint16_t)m_microScheduler.workerCount());

        TResultValue result = identityValue;

        Task* pTask = m_microScheduler.allocateTask<ParallelReduceTask<TRange, TResultValue, TMapReduceFunc, TJoinFunc, TPartitioner>>(
            mapReduceFunc, joinFunc, userData, &result, range, partitioner, m_priority);

        m_microScheduler.spawnTaskAndWait(pTask);

        return result;
    }

private:

    ParallelReduce(ParallelReduce const&) = delete;
    ParallelReduce* operator=(ParallelReduce const&) = delete;

    MicroScheduler& m_microScheduler;
    uint32_t m_priority;

private:

    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    template<
        typename TRange,
        typename TResultValue,
        typename TJoinFunc
        >
    class JoinTask : public Task
    {
    public:

        //----------------------------------------------------------------------
        GTS_INLINE JoinTask(
            TJoinFunc& joinFunc,
            TResultValue* pAcculumation,
            uint8_t numPredecessors)
            : m_joinFunc(joinFunc)
            , m_pAcculumation(pAcculumation)
            , m_numPredecessors(numPredecessors)
        {
            GTS_ASSERT(numPredecessors <= MAX_PREDECESSORS);

            for (int ii = 0; ii < m_numPredecessors; ++ii)
            {
                m_prececessorValues[ii] = TResultValue();
            }
        }

        //----------------------------------------------------------------------
        GTS_INLINE virtual Task* execute(TaskContext const& ctx) final
        {
            for (int ii = 0; ii < m_numPredecessors; ++ii)
            {
                *m_pAcculumation = m_joinFunc(
                    *m_pAcculumation,
                    m_prececessorValues[ii],
                    nullptr,
                    ctx);
            }
            return nullptr;
        }

        enum { MAX_PREDECESSORS = TRange::split_result::MAX_RANGES + 1 };

        TJoinFunc&              m_joinFunc;
        TResultValue            m_prececessorValues[MAX_PREDECESSORS];
        TResultValue*           m_pAcculumation;
        uint8_t                 m_numPredecessors;
    };

    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    /**
     * @details
     *  Derivative of TBB parallel_reduce. https://github.com/intel/tbb
     */
    template<
        typename TRange,
        typename TResultValue,
        typename TMapReduceFunc,
        typename TJoinFunc,
        typename TPartitioner
        >
    class ParallelReduceTask : public Task
    {
    public:

        using observer_task_type = JoinTask<TRange, TResultValue, TJoinFunc>;

        //----------------------------------------------------------------------
        ParallelReduceTask(
            TMapReduceFunc& mapReduceFunc,
            TJoinFunc& joinFunc,
            void* userData,
            TResultValue* pAcculumation,
            TRange const& range,
            TPartitioner partitioner,
            uint32_t priority)

            : m_reduceRangeFunc(mapReduceFunc)
            , m_joinFunc(joinFunc)
            , m_userData(userData)
            , m_pAcculumation(pAcculumation)
            , m_range(range)
            , m_priority(priority)
            , m_partitioner(partitioner)
        {
            GTS_ASSERT(!m_range.empty());
        }

        //----------------------------------------------------------------------
        ParallelReduceTask(ParallelReduceTask& parent, TRange& range, TResultValue* pAcculumation)
            : m_reduceRangeFunc(parent.m_reduceRangeFunc)
            , m_joinFunc(parent.m_joinFunc)
            , m_userData(parent.m_userData)
            , m_pAcculumation(pAcculumation)
            , m_range(range)
            , m_priority(parent.m_priority)
            , m_partitioner(parent.m_partitioner, 0, TRange())
        {
            GTS_ASSERT(!m_range.empty());
        }

        //----------------------------------------------------------------------
        ParallelReduceTask(ParallelReduceTask& parent, TRange const& range, uint16_t depth, TResultValue* pAcculumation)
            : m_reduceRangeFunc(parent.m_reduceRangeFunc)
            , m_joinFunc(parent.m_joinFunc)
            , m_userData(parent.m_userData)
            , m_pAcculumation(pAcculumation)
            , m_range(range)
            , m_priority(parent.m_priority)
            , m_partitioner(parent.m_partitioner, depth, TRange())
        {
            GTS_ASSERT(!m_range.empty());
        }

        //----------------------------------------------------------------------
        ~ParallelReduceTask() {}

        //----------------------------------------------------------------------
        ParallelReduceTask(ParallelReduceTask const&) = default;

        //----------------------------------------------------------------------
        ParallelReduceTask& operator=(ParallelReduceTask const& other)
        {
            if (this != &other)
            {
                m_reduceRangeFunc = other.m_reduceRangeFunc;
                m_joinFunc        = other.m_joinFunc;
                m_userData        = other.m_userData;
                m_range           = other.m_range;
                m_priority        = other.m_priority;
                m_partitioner     = other.m_partitioner;
            }

            return *this;
        }

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
            observer_task_type* pContinuation = ctx.pMicroScheduler->allocateTask<observer_task_type>(
                m_joinFunc, m_pAcculumation, uint8_t(splits.size + 1));

            pContinuation->addRef(splits.size + 1, gts::memory_order::relaxed);
            setContinuationTask(pContinuation);

            // Add the splits as siblings.
            for (int iSibling = 0; iSibling < splits.size; ++iSibling)
            {
                Task* pSibling = ctx.pMicroScheduler->allocateTask<ParallelReduceTask>(
                    *this, splits.ranges[iSibling], pContinuation->m_prececessorValues + iSibling);

                pContinuation->addChildTaskWithoutRef(pSibling);
                ctx.pMicroScheduler->spawnTask(pSibling);
            }

            // This task becomes a sibling.
            pContinuation->addChildTaskWithoutRef(this);
            m_pAcculumation = pContinuation->m_prececessorValues + splits.size;

            m_partitioner.template split<TRange>();
        }

        //----------------------------------------------------------------------
        void offerRange(TaskContext const& ctx, TRange const& range, uint16_t depth = 0)
        {
            GTS_ASSERT(!range.empty());

            observer_task_type* pContinuation = ctx.pMicroScheduler->allocateTask<observer_task_type>(
                m_joinFunc, m_pAcculumation, uint8_t(2));
            pContinuation->addRef(2, gts::memory_order::relaxed);

            setContinuationTask(pContinuation);

            Task* pRightChild = ctx.pMicroScheduler->allocateTask<ParallelReduceTask>(
                *this, range, depth, pContinuation->m_prececessorValues);

            pContinuation->addChildTaskWithoutRef(pRightChild);
            ctx.pMicroScheduler->spawnTask(pRightChild);

            // This task becomes the left task.
            pContinuation->addChildTaskWithoutRef(this);
            m_pAcculumation = pContinuation->m_prececessorValues + 1;

            m_partitioner.template split<TRange>();
        }

        //----------------------------------------------------------------------
        void run(TaskContext const& ctx, TRange& range, typename TPartitioner::splitter_type const&)
        {
            TResultValue reduction = m_reduceRangeFunc(range, m_userData, ctx);

            // The Adaptive partitioner may call run on several subranges. We
            // join these here.
            *m_pAcculumation = m_joinFunc(
                *m_pAcculumation,
                reduction,
                nullptr,
                ctx);
        }

        //----------------------------------------------------------------------
        virtual Task* execute(TaskContext const& ctx) final
        {
            m_partitioner.template adjustIfStolen<TRange>(this);
            return m_partitioner.execute(ctx, this, m_range);
        }

        //----------------------------------------------------------------------
        void balanceAndExecute(TaskContext const& ctx, TPartitioner& partitioner, TRange& range, typename TPartitioner::splitter_type const& splitter)
        {
            partitioner.balanceAndExecute(ctx, this, range, splitter);
        }

    private:

        TMapReduceFunc& m_reduceRangeFunc;
        TJoinFunc&      m_joinFunc;
        void*           m_userData;
        TResultValue*   m_pAcculumation;
        TRange          m_range;
        uint32_t        m_priority;
        TPartitioner    m_partitioner;
    };
};

/** @} */ // end of ParallelPatterns
/** @} */ // end of MicroScheduler

} // namespace gts
