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
#include "gts/micro_scheduler/patterns/BlockedRange1d.h"

namespace gts {

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
        : m_taskScheduler(scheduler)
        , m_priority(priority)
    {}

    /**
     * Applies the given function 'func' to the specified iteration iteration 'range'.
     *
     * @example
     *  parallelFor<AdaptivePartitioner>(BlockedRange1D<int>(0, 1024, 16),
     *      [](BlockedRange1D<int>& r, void* data, TaskContext const& ctx)
     *      {
     *          or(auto ii = r.begin(); ii < r.end(); ++ii) { ... }
     *      },
     *      AdaptivePartitioner(),
     *      pMyData);
     */
    template<typename TPartitioner, typename TRange, typename TFunc>
    GTS_INLINE void operator()(
        TRange const& range,
        TFunc func,
        TPartitioner partitioner,
        void* userData)
    {
        GTS_ASSERT(m_taskScheduler.isRunning());

        partitioner.setWorkerCount((uint8_t)m_taskScheduler.workerCount());

        Task* pTask = m_taskScheduler.allocateTask<DivideAndConquerTask<TFunc, TRange, TPartitioner>>(
            func, userData, range, partitioner, m_priority);

        m_taskScheduler.spawnTaskAndWait(pTask, m_priority);
    }

// BLOCKEDRANGE CONVENIENCE FUNCTIONS:

    /**
     * Applies the given function 'func' to the specified iteration iteration 'range'.
     *
     * @example
     *  parallelFor<AdaptivePartitioner>(BlockedRange1D<int>(0, 1024, 16),
     *      [](BlockedRange1D<int>& r, void* data, TaskContext const& ctx)
     *      {
     *          for(auto ii = r.begin(); ii < r.end(); ++ii) { ... }
     *      });
     *  parallelFor(BlockedRange1D<int>(0, 1024, 16), [](BlockedRange1D<int>& r, void* data, TaskContext const& ctx) {...}).
     */
    template<typename TPartitioner = AdaptivePartitioner, typename TRange, typename TFunc>
    GTS_INLINE void operator()(TRange const& range, TFunc func)
    {
        AdaptivePartitioner partitioner;
        this->operator()(range, func, partitioner, nullptr);
    }

// 1D CONVENIENCE FUNCTIONS:

    /**
     * Applies the given function 'func' to the specified 1D iteration
     * range [begin, end). Always uses an AdaptivePartitioner with
     * a minimum block size of 1.
     *
     * @example
     *  parallelFor<AdaptivePartitioner>(0, 1024, [](int i) {...}).
     *  parallelFor(0, 1024, [](int i) {...}).
     */
    template<typename TIter, typename TFunc>
    GTS_INLINE void operator()(
        TIter begin,
        TIter end,
        TFunc func)
    {
        AdaptivePartitioner partitioner;
        BlockedRange1d<TIter> range(begin, end, 1);
        IterationFunc1d<TFunc, BlockedRange1d<TIter>> iterFunc(func);

        this->operator()(range, iterFunc, partitioner, nullptr);
    }

    /**
     * Applies the given function 'func' to the specified 1D iteration
     * range [begin, end) and constrains range subdivisions to minBlockSize.
     *
     * @example
     *  parallelFor<AdaptivePartitioner>(0, 1024, [](int i) {...}, 16).
     *  parallelFor(0, 1024, [](int i) {...}, 16).
     */
    template<typename TPartitioner = AdaptivePartitioner, typename TIter, typename TFunc>
    GTS_INLINE void operator()(
        TIter begin,
        TIter end,
        TFunc func,
        uint32_t minBlockSize)
    {
        TPartitioner partitioner;
        BlockedRange1d<TIter> range(begin, end, minBlockSize);
        IterationFunc1d<TFunc, BlockedRange1d<TIter>> iterFunc(func);

        this->operator()(range, iterFunc, partitioner, nullptr);
    }


private:

    MicroScheduler& m_taskScheduler;
    uint32_t m_priority;

private:

    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    class TheftObserverTask
    {
    public:

        //----------------------------------------------------------------------
        GTS_INLINE TheftObserverTask()
            : m_childTaskStolen(false)
        {}

        //----------------------------------------------------------------------
        static GTS_INLINE Task* taskFunc(Task*, TaskContext const&)
        {
            return nullptr;
        }

        gts::Atomic<bool> m_childTaskStolen;
    };

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
            for (auto ii = r.begin(); ii != r.end(); ++ii)
            {
                m_userFunc(ii);
            }
        }

        TUserFunc& m_userFunc;
    };

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
                m_func                  = other.m_func;
                m_userData              = other.m_userData;
                m_range                 = other.m_range;
                m_priority              = other.m_priority;
                m_partitioner           = other.m_partitioner;
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
            Task* pContinuation = ctx.pMicroScheduler->allocateTask<TheftObserverTask>();
            pContinuation->addRef(2, gts::memory_order::relaxed);

            pThisTask->setContinuationTask(pContinuation);

            Task* pRightChild = ctx.pMicroScheduler->allocateTask<DivideAndConquerTask>(
                m_func,
                m_userData,
                range,
                m_partitioner.split(depth),
                m_priority);

            pContinuation->addChildTaskWithoutRef(pRightChild);
            ctx.pMicroScheduler->spawnTask(pRightChild);

            // This task becomes the left task.
            pContinuation->addChildTaskWithoutRef(pThisTask);
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
            pThisForTask->partitioner().template adjustIfStolen<TheftObserverTask>(pThisTask);
            Task* pBypassTask = pThisForTask->partitioner().template execute<TheftObserverTask>(pThisTask, ctx, pThisForTask, pThisForTask->range());

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
