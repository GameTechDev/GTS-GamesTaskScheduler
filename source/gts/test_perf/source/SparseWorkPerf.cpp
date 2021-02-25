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
#include <chrono>

#include "gts_perf/PerfTests.h"

#include <gts/micro_scheduler/WorkerPool.h>
#include <gts/micro_scheduler/MicroScheduler.h>
#include <gts/micro_scheduler/patterns/ParallelFor.h>
#include <gts/micro_scheduler/patterns/Range1d.h>

//------------------------------------------------------------------------------
Stats sparseWorkPerf(gts::MicroScheduler& taskScheduler, uint32_t items, uint32_t iterations)
{
    Stats stats(iterations);

    gts::ParallelFor parallelFor(taskScheduler);


    // Do test.
    for (uint32_t iter = 0; iter < iterations; ++iter)
    {
        GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);

        auto start = std::chrono::high_resolution_clock::now();

        parallelFor(
            gts::Range1d<uint32_t>(0, items, 1),
            [](gts::Range1d<uint32_t>& range, void*, gts::TaskContext const& /*ctx*/)
            {
                GTS_TRACE_SCOPED_ZONE_P1(gts::analysis::CaptureMask::USER, gts::analysis::Color::Cyan, "Irregular ParFor Task", range.size());

                for (uint32_t r = range.begin(); r != range.end(); ++r)
                {
                    float f = 0.45f;
                    for (volatile uint32_t ii = 0; ii < 1000; ++ii)
                    {
                        f = sin(f);
                    }
                }
            },
            gts::AdaptivePartitioner(4),
                nullptr);

        gts::ThisThread::sleepFor(10);

        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double> diff = end - start;
        stats.addDataPoint(diff.count());
    }

    return stats;
}
