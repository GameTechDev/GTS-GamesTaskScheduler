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
#include "gts/micro_scheduler/patterns/Partitioners.h"

namespace gts {

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
        : m_taskScheduler(scheduler)
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
     *  The partitioner object that determines how work is subdivided during
     *  scheduling.
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
        typename TPartition = AdaptivePartitioner
    >
    GTS_INLINE TResultValue operator()(
        TRange range,
        TMapReduceFunc mapReduceFunc,
        TJoinFunc joinFunc,
        TResultValue identityValue,
        TPartition partitioner = AdaptivePartitioner(),
        void* userData = nullptr)
    {
        GTS_ASSERT(m_taskScheduler.isRunning());

        partitioner.setWorkerCount((uint8_t)m_taskScheduler.workerCount());

        TResultValue result = identityValue;

        Task* pTask = m_taskScheduler.allocateTask<DivideAndConquerTask<TRange, TResultValue, TMapReduceFunc, TJoinFunc, TPartition>>(
            mapReduceFunc, joinFunc, userData, &result, range, partitioner, m_priority);

        m_taskScheduler.spawnTaskAndWait(pTask);

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
        typename TJoinFunc
        >
    class TheftObserverAndJoinTask
    {
    public:

        //----------------------------------------------------------------------
        GTS_INLINE TheftObserverAndJoinTask(
            TJoinFunc& joinFunc,
            TResultValue leftValue,
            TResultValue rightValue,
            TResultValue* pAcculumation)
            : m_joinFunc(joinFunc)
            , m_leftValue(leftValue)
            , m_rightValue(rightValue)
            , m_pAcculumation(pAcculumation)
            , m_childTaskStolen(false)
        {}

        //----------------------------------------------------------------------
        static GTS_INLINE Task* taskFunc(Task* pThisTask, TaskContext const& ctx)
        {
            TheftObserverAndJoinTask* pData= (TheftObserverAndJoinTask*)pThisTask->getData();
            *pData->m_pAcculumation = pData->m_joinFunc(
                pData->m_leftValue,
                pData->m_rightValue,
                nullptr,
                ctx);
            return nullptr;
        }

        TJoinFunc&        m_joinFunc;
        TResultValue      m_leftValue;
        TResultValue      m_rightValue;
        TResultValue*     m_pAcculumation;
        gts::Atomic<bool> m_childTaskStolen;
    };

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

        using ObserverTaskType = TheftObserverAndJoinTask<TRange, TResultValue, TJoinFunc>;

        //----------------------------------------------------------------------
        DivideAndConquerTask(
            TMapReduceFunc& mapReduceFunc,
            TJoinFunc& joinFunc,
            void* userData,
            TResultValue* pAcculumation,
            TRange const& range,
            TPartition partitioner,
            uint32_t priority)

            : m_reduceRangeFunc(mapReduceFunc)
            , m_joinFunc(joinFunc)
            , m_userData(userData)
            , m_pAcculumation(pAcculumation)
            , m_range(range)
            , m_priority(priority)
            , m_partitioner(partitioner)
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
                m_range                   = other.m_range;
                m_priority                = other.m_priority;
                m_partitioner             = other.m_partitioner;
            }

            return *this;
        }

        //----------------------------------------------------------------------
        void offerRange(Task* pThisTask, TaskContext const& ctx, TRange range)
        {
            offerRange(pThisTask, ctx, range, 0);
        }

        //----------------------------------------------------------------------
        void offerRange(Task* pThisTask, TaskContext const& ctx, TRange range, uint8_t depth)
        {
            Task* pContinuation = ctx.pMicroScheduler->allocateTaskRaw(ObserverTaskType::taskFunc, sizeof(ObserverTaskType));
            ObserverTaskType* pContinuationData = pContinuation->emplaceData<ObserverTaskType>(
                m_joinFunc, TResultValue(), TResultValue(), m_pAcculumation);
            pContinuation->addRef(2, gts::memory_order::relaxed);

            pThisTask->setContinuationTask(pContinuation);

            Task* pRightChild = ctx.pMicroScheduler->allocateTask<DivideAndConquerTask>(
                m_reduceRangeFunc,
                m_joinFunc,
                m_userData,
                &pContinuationData->m_rightValue,
                range,
                m_partitioner.split(depth),
                m_priority);

            pContinuation->addChildTaskWithoutRef(pRightChild);
            ctx.pMicroScheduler->spawnTask(pRightChild);

            // This task becomes the left task.
            pContinuation->addChildTaskWithoutRef(pThisTask);
            m_pAcculumation = &pContinuationData->m_leftValue;
        }

        //----------------------------------------------------------------------
        void run(TaskContext const& ctx, TRange range)
        {
            // map/reduce its range
            *m_pAcculumation = m_reduceRangeFunc(range, m_userData, ctx);
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
            pThisReduceTask->partitioner().template adjustIfStolen<ObserverTaskType>(pThisTask);
            Task* pBypassTask = pThisReduceTask->partitioner().template execute<ObserverTaskType>(pThisTask, ctx, pThisReduceTask, pThisReduceTask->range());

            return pBypassTask;
        }

    private:

        TMapReduceFunc& m_reduceRangeFunc;
        TJoinFunc&      m_joinFunc;
        void*           m_userData;
        TResultValue*   m_pAcculumation;
        TRange          m_range;
        uint32_t        m_priority;
        TPartition      m_partitioner;
    };
};

} // namespace gts
