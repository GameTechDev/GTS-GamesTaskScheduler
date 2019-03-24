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
#include "gts/micro_scheduler/patterns/BlockedRange1d.h"

namespace gts {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
class ParallelReduce
{
public:

    //--------------------------------------------------------------------------
    GTS_INLINE ParallelReduce(MicroScheduler& scheduler, uint32_t priority = 0)
        : m_taskScheduler(scheduler)
        , m_priority(priority)
    {}

    //--------------------------------------------------------------------------
    /**
     * Combines all the values in 'range' via 'reduceFunc'.
     * @param range
     *  The divisible range to iterate over.
     * @param mapReduceFunc
     *  The function to execute on 'range'. Requires the signature:
     *  TValue mapReduceFunc(TRange range, void* userData, TaskContext const&).
     *  NOTE: This can be more that just a reduction operation. For instance one
     *  could fuse map-reduce here.
     * @param joinFunc
     *  The function that joins the results of mapReduceFunc to produce the final
     *  reduction value. Requires the signature:
     *  TValue joinFunc(TValue const& lhs, TValue const& rhs, void* userData, TaskContext const&).
     * @param userData
     *  Optional data to be used in func.
     * @return
     *  The combined result.
     */
    template<
        typename TRange,
        typename TResultValue,
        typename TMapReduceFunc,
        typename TJoinFunc,
        typename TPartition
    >
    GTS_INLINE TResultValue operator()(
        TRange range,
        TMapReduceFunc mapReduceFunc,
        TJoinFunc joinFunc,
        TResultValue identityValue,
        TPartition partitioner,
        void* userData = nullptr)
    {
        GTS_ASSERT(m_taskScheduler.isRunning());

        const uint32_t workerCount = m_taskScheduler.workerCount();

        partitioner.setWorkerCount((uint8_t)workerCount);

        gts::Vector<TResultValue> m_acculumationByThreadIdx(workerCount);

        Task* pTask = m_taskScheduler.allocateTask(DivideAndConquerTask<TRange, TResultValue, TMapReduceFunc, TJoinFunc, TPartition>::taskFunc);
        pTask->emplaceData<DivideAndConquerTask<TRange, TResultValue, TMapReduceFunc, TJoinFunc, TPartition>>(
            mapReduceFunc, joinFunc, userData, m_acculumationByThreadIdx.data(), range, partitioner, m_priority);

        m_taskScheduler.spawnTaskAndWait(pTask);

        TResultValue result = identityValue;
        for (uint32_t ii = 0; ii < workerCount; ++ii)
        {
            result = joinFunc(
                result,
                m_acculumationByThreadIdx[ii],
                userData,
                TaskContext{ &m_taskScheduler, m_taskScheduler.thisWorkerIndex() });
        }

        return result;
    }

private:

    ParallelReduce(ParallelReduce const&) = delete;
    ParallelReduce* operator=(ParallelReduce const&) = delete;

    MicroScheduler& m_taskScheduler;
    uint32_t m_priority;

private:

    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    template<
        typename TRange,
        typename TResultValue,
        typename TMapReduceFunc,
        typename TJoinFunc,
        typename TPartition
        >
    class DivideAndConquerTask
    {
    public:

        //----------------------------------------------------------------------
        DivideAndConquerTask(
            TMapReduceFunc& mapReduceFunc,
            TJoinFunc& joinFunc,
            void* userData,
            TResultValue* acculumationByThreadIdx,
            TRange const& range,
            TPartition partitioner,
            uint32_t priority)

            : m_reduceRangeFunc(mapReduceFunc)
            , m_joinFunc(joinFunc)
            , m_userData(userData)
            , m_acculumationByThreadIdx(acculumationByThreadIdx)
            , m_range(range)
            , m_partitioner(partitioner)
            , m_priority(priority)
        {}

        //----------------------------------------------------------------------
        DivideAndConquerTask(DivideAndConquerTask const&) = default;

        //----------------------------------------------------------------------
        DivideAndConquerTask& operator=(DivideAndConquerTask const& other)
        {
            if (this != &other)
            {

                m_reduceRangeFunc         = other.m_reduceRangeFunc;
                m_joinFunc                = other.m_joinFunc;
                m_userData                = other.m_userData;
                m_acculumationByThreadIdx = other.m_acculumationByThreadIdx;
                m_range                   = other.m_range;
                m_priority                = other.m_priority;
                m_partitioner             = other.m_partitioner;
            }

            return *this;
        }

        //----------------------------------------------------------------------
        Task* offerRange(Task* pThisTask, TaskContext const& ctx, TRange range)
        {
            return offerRange(pThisTask, ctx, range, 0);
        }

        //----------------------------------------------------------------------
        Task* offerRange(Task* pThisTask, TaskContext const& ctx, TRange range, uint8_t depth)
        {
            Task* pContinuation = ctx.pMicroScheduler->allocateTask(TheftObserverTask::taskFunc);
            pContinuation->emplaceData<TheftObserverTask>();
            pContinuation->addRef(2, gts::memory_order::relaxed);

            pThisTask->setContinuationTask(pContinuation);

            Task* pRightChild = ctx.pMicroScheduler->allocateTask(DivideAndConquerTask::taskFunc);

            pRightChild->emplaceData<DivideAndConquerTask<TRange, TResultValue, TMapReduceFunc, TJoinFunc, TPartition>>(
                m_reduceRangeFunc,
                m_joinFunc,
                m_userData,
                m_acculumationByThreadIdx,
                range,
                m_partitioner.split(depth),
                m_priority);

            pContinuation->addChildTaskWithoutRef(pRightChild);

            ctx.pMicroScheduler->spawnTask(pRightChild);

            return pContinuation;
        }

        //----------------------------------------------------------------------
        void run(TaskContext const& ctx, TRange range)
        {
            // map/reduce its range.
            TResultValue result = m_reduceRangeFunc(range, m_userData, ctx);

            // accumulate the result
            m_acculumationByThreadIdx[ctx.workerIndex] = m_joinFunc(
                m_acculumationByThreadIdx[ctx.workerIndex],
                result,
                m_userData,
                ctx);
        }

        //----------------------------------------------------------------------
        TRange& range()
        {
            return m_range;
        }

        //----------------------------------------------------------------------
        TPartition& partitioner()
        {
            return m_partitioner;
        }

        //----------------------------------------------------------------------
        static GTS_INLINE Task* taskFunc(Task* pThisTask, TaskContext const& ctx)
        {
            DivideAndConquerTask* pThisReduceTask = (DivideAndConquerTask*)pThisTask->getData();
            pThisReduceTask->partitioner().adjustIfStolen(pThisTask);
            Task* pBypassTask = pThisReduceTask->partitioner().execute(pThisTask, ctx, pThisReduceTask, pThisReduceTask->range());

            pThisReduceTask->range().destroy();

            return pBypassTask;
        }

    private:

        TMapReduceFunc& m_reduceRangeFunc;
        TJoinFunc&      m_joinFunc;
        void*           m_userData;
        TResultValue*   m_acculumationByThreadIdx;
        TRange          m_range;
        uint32_t        m_priority;
        TPartition      m_partitioner;
    };
};

} // namespace gts
