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

#include <gts/analysis/Trace.h>
#include <gts/micro_scheduler/WorkerPool.h>
#include <gts/micro_scheduler/MicroScheduler.h>
#include <gts/micro_scheduler/patterns/ParallelFor.h>
#include <gts/micro_scheduler/patterns/Range1d.h>

//------------------------------------------------------------------------------
/**
 * Test how long it takes to allocate and spawn K tasks.
 */
Stats spawnTaskOverheadWithoutAllocPerf(gts::MicroScheduler& taskScheduler, uint32_t iterations)
{
    Stats stats(iterations);

    gts::Task* pTask = taskScheduler.allocateTask<gts::EmptyTask>();

    // Do test.
    GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);

    auto start = GTS_RDTSC();

    for (uint32_t iTask = 0; iTask < iterations; ++iTask)
    {
        taskScheduler.spawnTask(pTask);
    }

    auto end = GTS_RDTSC();

    stats.addDataPoint(double(end - start) / iterations);

    return stats;
}

//------------------------------------------------------------------------------
/**
 * Test how long it takes to allocate and spawn K tasks.
 */
Stats spawnTaskOverheadWithAllocCachingPerf(gts::MicroScheduler& taskScheduler, uint32_t iterations)
{
    Stats stats(iterations);

    // Do test.
    GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);

    // Build the task cache.
    gts::Vector<gts::Task*> tasks(iterations);
    for (uint32_t iTask = 0; iTask < iterations; ++iTask)
    {
        tasks[iTask] = taskScheduler.allocateTask<gts::EmptyTask>();
    }
    for (uint32_t iTask = 0; iTask < iterations; ++iTask)
    {
        taskScheduler.destoryTask(tasks[iTask]);
    }

    auto start = GTS_RDTSC();

    for (uint32_t iTask = 0; iTask < iterations; ++iTask)
    {
        gts::Task* pTask = taskScheduler.allocateTask<gts::EmptyTask>();
        taskScheduler.spawnTask(pTask);
    }

    auto end = GTS_RDTSC();

    stats.addDataPoint(double(end - start) / iterations);

    return stats;
}

//------------------------------------------------------------------------------
/**
 * Test how long it takes to spawn K tasks.
 */
Stats spawnTaskOverheadWithAllocPerf(gts::MicroScheduler& taskScheduler, uint32_t iterations)
{
    Stats stats(iterations);

    // Do test.
    GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);

    auto start = GTS_RDTSC();
        
    for (uint32_t iTask = 0; iTask < iterations; ++iTask)
    {
        gts::Task* pTask = taskScheduler.allocateTask<gts::EmptyTask>();
        taskScheduler.spawnTask(pTask);
    }

    auto end = GTS_RDTSC();

    stats.addDataPoint(double(end - start) / iterations);

    return stats;
}
