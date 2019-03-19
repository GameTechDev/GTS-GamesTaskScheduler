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

namespace gts {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
class ParallelFor
{
public:

    //--------------------------------------------------------------------------
    GTS_INLINE ParallelFor(MicroScheduler& scheduler, uint32_t priority = 0)
        : m_taskScheduler(scheduler)
        , m_priority(priority)
    {}

    //--------------------------------------------------------------------------
    /**
     * Applies the given function to the specified range [begin, end).
     * @param range
     *  The range to iterator over.
     * @param func
     *  The function to execute on the range. Requires the signature
     *  void func(TRange&, void* userData, TaskContext&).
     * @param userData
     *  Optional extra to be used in func.
     */
    template<typename TFunc, typename TRange, typename TPartition>
    GTS_INLINE void operator()(TRange const& range, TFunc func, TPartition partitioner, void* userData = nullptr)
    {
        GTS_ASSERT(m_taskScheduler.isRunning());

        partitioner.setWorkerCount((uint8_t)m_taskScheduler.workerCount());

        Task* pTask = m_taskScheduler.allocateTask(DivideAndConquerTask<TFunc, TRange, TPartition>::taskFunc);
        pTask->emplaceData<DivideAndConquerTask<TFunc, TRange, TPartition>>(func, userData, range, partitioner, m_priority);

        m_taskScheduler.spawnTaskAndWait(pTask, m_priority);
    }

private:

    ParallelFor(ParallelFor const&) = delete;
    ParallelFor* operator=(ParallelFor const&) = delete;

    MicroScheduler& m_taskScheduler;
    uint32_t m_priority;

private:

    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    template<typename TFunc, typename TRange, typename TPartition>
    class DivideAndConquerTask
    {
    public:

        //----------------------------------------------------------------------
        DivideAndConquerTask(
            TFunc& func,
            void* userData,
            TRange const& range,
            TPartition partitioner,
            uint32_t priority)

            : m_func(func)
            , m_userData(userData)
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
                m_func                  = other.m_func;
                m_userData              = other.m_userData;
                m_range                 = other.m_range;
                m_priority              = other.m_priority;
                m_partitioner           = other.m_partitioner;
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

            pRightChild->emplaceData<DivideAndConquerTask<TFunc, TRange, TPartition>>(
                m_func,
                m_userData,
                range,
                m_partitioner.split(depth),
                m_priority);

            pContinuation->addChildTask(pRightChild);

            ctx.pMicroScheduler->spawnTask(pRightChild);

            return pContinuation;
        }

        //----------------------------------------------------------------------
        void run(TaskContext const& ctx, TRange range)
        {
            m_func(range, m_userData, ctx);
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
            DivideAndConquerTask* pThisForTask = (DivideAndConquerTask*)pThisTask->getData();
            pThisForTask->partitioner().adjustIfStolen(pThisTask);
            Task* pBypassTask = pThisForTask->partitioner().execute(pThisTask, ctx, pThisForTask, pThisForTask->range());

            pThisForTask->range().destroy();

            return pBypassTask;
        }

    private:

        TFunc&      m_func;
        void*       m_userData;
        TRange      m_range;
        uint32_t    m_priority;
        TPartition  m_partitioner;
    };
};

} // namespace gts
