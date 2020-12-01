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

#include "gts/micro_scheduler/WorkerPool.h"
#include "gts/micro_scheduler/MicroScheduler.h"
#include "gts/micro_scheduler/patterns/ParallelFor.h"
#include "gts/micro_scheduler/patterns/Range1d.h"

using namespace gts;

namespace gts_examples {

// MORAL: Don't submit singular tasks from the same thread. It causes an O(N)
// critical path that limits parallelism, and it causes worst case communication
// overhead because all threads work off a single task deque.
//
// Instead, use divide-and-conquer to recursively subdivide workloads, which
// normally (probability-wise) distributes tasks across threads and yields
// O(lgN) critical paths in expectation (on average).


//------------------------------------------------------------------------------
// Demonstrates a pattern of task submission that scales poorly.
class BadParallelFor
{
public:

    //--------------------------------------------------------------------------
    template<typename TFunc>
    class ForTask : public Task
    {
    public:

        //----------------------------------------------------------------------
        ForTask(TFunc& func, uint32_t start, uint32_t end)
            : m_func(func)
            , m_start(start)
            , m_end(end)
        {}

        //----------------------------------------------------------------------
        virtual GTS_INLINE Task* execute(TaskContext const& ctx) override
        {
            m_func(m_start, m_end, ctx);
            return nullptr;
        }

    private:

        TFunc&   m_func;
        uint32_t m_start;
        uint32_t m_end;
    };

    //--------------------------------------------------------------------------
    inline BadParallelFor(MicroScheduler& scheduler)
        : m_scheduler(scheduler)
    {}

    //--------------------------------------------------------------------------
    template<typename TFunc>
    inline void operator()(uint32_t start, uint32_t end, TFunc func)
    {
        // Create one task per item.
        uint32_t taskCount = end - start;

        Task* pRootTask = m_scheduler.allocateTask<EmptyTask>();
        pRootTask->addRef(taskCount, gts::memory_order::relaxed);

        // Here we are spawning each task on this thread. BAD!
        for (uint32_t ii = 1; ii < end; ++ii)
        {
            Task* pChildTask = m_scheduler.allocateTask<ForTask<TFunc>>(func, ii, ii + 1);
            pRootTask->addChildTaskWithoutRef(pChildTask);
            m_scheduler.spawnTask(pChildTask);
        }

        func(0, 1, TaskContext{nullptr, OwnedId(0, 0)});

        pRootTask->waitForAll();
        m_scheduler.destoryTask(pRootTask);
    }

private:

    MicroScheduler& m_scheduler;
};

//------------------------------------------------------------------------------
GTS_NO_INLINE void dummyWork(float& f)
{
    // do some dummy work
    for (int jj = 0; jj < 1000; ++jj)
    {
        f = sin(f);
    }
}

struct GTS_ALIGN(GTS_NO_SHARING_CACHE_LINE_SIZE) AlignedFloat
{
    explicit AlignedFloat(float f) : f(f) {}
    float f = 0;
};

//------------------------------------------------------------------------------
void loopOverRange(size_t start, size_t end, gts::Vector<AlignedFloat, AlignedAllocator<GTS_NO_SHARING_CACHE_LINE_SIZE>>& dummyData, uint32_t workerId)
{
    float& f = dummyData[workerId].f;
    for (size_t ii = start; ii < end; ++ii)
    {
        dummyWork(f);
    }
}

//------------------------------------------------------------------------------
void submissionOnSingleThread()
{
    printf ("================\n");
    printf ("submissionOnSingleThread\n");
    printf ("================\n");

    const uint32_t threadCount = gts::Thread::getHardwareThreadCount();
    const uint32_t elementCount = threadCount * 100;

    WorkerPool workerPool;
    workerPool.initialize(threadCount);

    MicroScheduler microScheduler;
    microScheduler.initialize(&workerPool);

    const size_t sampleCount = 1000;

    //
    // BAD:
    // Uses a parallel for algorithm that only submits tasks from the main thread.


    uint64_t samples = 0;

    gts::Vector<AlignedFloat, AlignedAllocator<GTS_NO_SHARING_CACHE_LINE_SIZE>> dummyData(microScheduler.workerCount(), AlignedFloat(0.f));

    for(size_t ii = 0; ii < sampleCount; ++ii)
    {
        auto start = std::chrono::high_resolution_clock::now();

        BadParallelFor badParallelFor(microScheduler);
        badParallelFor(0, elementCount,
            [&dummyData](uint32_t start, uint32_t end, TaskContext const& ctx)
            {
                loopOverRange(start, end, dummyData, ctx.workerId.localId());
            });

        auto end = std::chrono::high_resolution_clock::now();
        samples += (end - start).count();
    }
    std::cout << "BadParallelFor time:  " << samples / sampleCount << std::endl;

    //
    // GOOD:
    // Uses GTS's parallel for algorithm that uses divide-and-conquer to
    // recursively subdivide workloads.

    samples = 0;

    for (size_t ii = 0; ii < sampleCount; ++ii)
    {
        auto start = std::chrono::high_resolution_clock::now();

        ParallelFor goodParallelFor(microScheduler);
        goodParallelFor(
            Range1d<size_t>(0, elementCount, 1),
            [&dummyData](Range1d<size_t>& range, void* pData, TaskContext const& ctx)
            {
                loopOverRange(range.begin(), range.end(), dummyData, ctx.workerId.localId());
            },
            SimplePartitioner(),
            nullptr);

        auto end = std::chrono::high_resolution_clock::now();
        samples += (end - start).count();
    }
    std::cout << "GoodParallelFor time: " << samples / sampleCount << std::endl;
}

} // gts_examples
