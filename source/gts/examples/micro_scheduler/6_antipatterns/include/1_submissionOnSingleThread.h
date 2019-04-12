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
#include <iostream>
#include <chrono>

#include "gts/platform/Atomic.h"
#include "gts/analysis/Instrumenter.h"
#include "gts/analysis/ConcurrentLogger.h"

#include "gts/micro_scheduler/WorkerPool.h"
#include "gts/micro_scheduler/MicroScheduler.h"
#include "gts/micro_scheduler/patterns/ParallelFor.h"
#include "gts/micro_scheduler/patterns/BlockedRange1d.h"

using namespace gts;

namespace gts_examples {

// MORAL: Don't submit singular tasks from the same thread. It causes an O(N)
// critical path that limits parallelism, and it causes worst case communication
// overhead because all threads work off a single task deque.
//
// Instead, use divide-and-conquer to recursively subdivide workload, which
// normally (probability-wise) distributes tasks across threads and yields
// O(lgN) critical paths.



//------------------------------------------------------------------------------
// Demonstrates a pattern of task submission that scales poorly.
class BadParallelFor
{
public:

    //--------------------------------------------------------------------------
    inline BadParallelFor(MicroScheduler& scheduler)
        : m_scheduler(scheduler)
    {}

    //--------------------------------------------------------------------------
    template<typename TFunc>
    inline void operator()(uint32_t start, uint32_t end, TFunc func)
    {
        // Create one chunk per thread.
        uint32_t taskCount = m_scheduler.workerCount();
        uint32_t chunkSize = (end - start) / m_scheduler.workerCount();

        Task* pRootTask = m_scheduler.allocateTask<ForTask<TFunc>>(func, start, end);
        pRootTask->addRef(taskCount, gts::memory_order::relaxed);

        // Here we are spawning each task on this thread. BAD!
        for (uint32_t ii = start; ii < end; ii += chunkSize)
        {
            Task* pChildTask = m_scheduler.allocateTask<ForTask<TFunc>>(func, ii, ii + chunkSize);
            pRootTask->addChildTaskWithoutRef(pChildTask);
            m_scheduler.spawnTask(pChildTask);
        }

        m_scheduler.spawnTaskAndWait(pRootTask);
    }

private:

    MicroScheduler& m_scheduler;

    //--------------------------------------------------------------------------
    template<typename TFunc>
    class ForTask
    {
    public:

        //----------------------------------------------------------------------
        ForTask(TFunc& func, uint32_t start, uint32_t end)
            : m_func(func)
            , m_start(start)
            , m_end(end)
        {}

        //----------------------------------------------------------------------
        static GTS_INLINE Task* taskFunc(Task* pThisTask, TaskContext const& ctx)
        {
            ForTask* pData = (ForTask*)pThisTask->getData();
            pData->m_func(pData->m_start, pData->m_end, ctx);
            return nullptr;
        }

    private:

        TFunc&      m_func;
        uint32_t    m_start;
        uint32_t    m_end;
    };
};

//------------------------------------------------------------------------------
void dummyWork(float& f)
{
    // do some dummy work
    for (int jj = 0; jj < 10; ++jj)
    {
        f = sin(f);
    }
}

//------------------------------------------------------------------------------
void submissionOnSingleThread()
{
    const uint32_t elementCount = 1000;

    WorkerPool workerPool;
    workerPool.initialize(gts::Thread::getHardwareThreadCount());

    MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    const size_t sampleCount = 10000;
    uint64_t samples = 0;

    gts::Vector<float> dummyData(taskScheduler.workerCount(), 0.f);

    for(size_t ii = 0; ii < sampleCount; ++ii)
    {
        auto start = std::chrono::high_resolution_clock::now();

        BadParallelFor badParallelFor(taskScheduler);
        badParallelFor(0, elementCount,
            [&dummyData](uint32_t start, uint32_t end, TaskContext const& ctx)
            {
                float& f = dummyData[ctx.workerIndex];
                f = (float)rand();
                for (uint32_t ii = start; ii < end; ++ii)
                {
                    dummyWork(f);
                }
            });

        auto end = std::chrono::high_resolution_clock::now();
        samples += (end - start).count();
    }
    std::cout << "BadParallelFor time: " << samples / sampleCount << std::endl;


    samples = 0;

    for (size_t ii = 0; ii < sampleCount; ++ii)
    {
        auto start = std::chrono::high_resolution_clock::now();

        ParallelFor goodParallelFor(taskScheduler);
        goodParallelFor(
            BlockedRange1d<size_t>(0, elementCount, elementCount / taskScheduler.workerCount()),
            [&dummyData](BlockedRange1d<size_t>& range, void* pData, TaskContext const& ctx)
            {
                float& f = dummyData[ctx.workerIndex];
                f = (float)rand();
                for (size_t ii = range.begin(); ii != range.end(); ++ii)
                {
                    dummyWork(f);
                }
            },
            StaticPartitioner(),
            nullptr);

        auto end = std::chrono::high_resolution_clock::now();
        samples += (end - start).count();
    }
    std::cout << "GoodParallelFor time: " << samples / sampleCount << std::endl;
}

} // gts_examples
