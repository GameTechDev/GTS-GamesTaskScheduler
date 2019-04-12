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
#include <atomic>
#include <chrono>

#include "gts_perf/Stats.h"

#include <gts/micro_scheduler/WorkerPool.h>
#include <gts/micro_scheduler/MicroScheduler.h>

//------------------------------------------------------------------------------
// A small, dummy compute workload.
gts::Task* poorDistributionTask(gts::Task*, gts::TaskContext const&)
{
    volatile float sum = 0.f;
    for (volatile uint32_t ii = 0; ii < 1000; ++ii)
    {
        sum += ii * ii;
    }
    return nullptr;
}

//------------------------------------------------------------------------------
/**
 * Test the scenario where single tasks are submitted from a single thread,
 * which causes worst case load balancing for the scheduler.
 */
Stats poorDistributionPerf(uint32_t taskCount, uint32_t iterations, uint32_t threadCount)
{
    Stats stats(iterations);

    gts::WorkerPool workerPool;
    workerPool.initialize(threadCount);

    gts::MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    // Do test.
    for (uint32_t ii = 0; ii < iterations; ++ii)
    {
        auto start = std::chrono::high_resolution_clock::now();

        gts::Task* pRootTask = taskScheduler.allocateTaskRaw([](gts::Task*, gts::TaskContext const&)->gts::Task* { return nullptr; });
        pRootTask->addRef(taskCount);

        for (uint32_t t = 0; t < taskCount; ++t)
        {
            gts::Task* pChildTask = taskScheduler.allocateTaskRaw(poorDistributionTask);
            pRootTask->addChildTaskWithoutRef(pChildTask);
            taskScheduler.queueTask(pChildTask);
        }

        taskScheduler.spawnTaskAndWait(pRootTask);

        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double> diff = end - start;
        stats.addDataPoint(diff.count());
    }

    taskScheduler.shutdown();

    return stats;
}
