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
/**
 * Test the scenario where single tasks are submitted from a single thread,
 * which causes worst case load balancing for the scheduler.
 */
Stats poorDistributionPerf(gts::MicroScheduler& taskScheduler, uint32_t taskCount, uint32_t iterations)
{
    Stats stats(iterations);

    // Do test.
    for (uint32_t ii = 0; ii < iterations; ++ii)
    {
        GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);

        auto start = std::chrono::high_resolution_clock::now();

        gts::Task* pRootTask = taskScheduler.allocateTask<gts::EmptyTask>();
        pRootTask->addRef(taskCount + 1);

        for (uint32_t t = 0; t < taskCount; ++t)
        {
            GTS_TRACE_SCOPED_ZONE_P0(gts::analysis::CaptureMask::MICRO_SCEHDULER_ALL, gts::analysis::Color::RoyalBlue, "Poor Dist Root Task");

            gts::Task* pChildTask = taskScheduler.allocateTask([](gts::TaskContext const&)->gts::Task*
            {
                GTS_TRACE_SCOPED_ZONE_P0(gts::analysis::CaptureMask::MICRO_SCEHDULER_ALL, gts::analysis::Color::RoyalBlue, "Poor Dist Root Task");

                volatile float val = 0.f;
                for (volatile uint32_t ii = 0; ii < 10000; ++ii)
                {
                    val = sinf(val);
                }
                return nullptr;
            });

            pRootTask->addChildTaskWithoutRef(pChildTask);
            taskScheduler.spawnTask(pChildTask);
        }

        pRootTask->waitForAll();
        taskScheduler.destoryTask(pRootTask);

        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double> diff = end - start;
        stats.addDataPoint(diff.count());
    }

    return stats;
}
