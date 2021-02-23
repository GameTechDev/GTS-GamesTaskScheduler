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

#ifdef GTS_MSVC
#pragma warning(push)
#pragma warning(disable : 4324) // alignment padding warning
#endif

struct alignas(256) PaddedInt
{
    uint32_t i;
};

#ifdef GTS_MSVC
#pragma warning(pop)
#endif

//------------------------------------------------------------------------------
Stats irregularRandParallelFor(gts::MicroScheduler& taskScheduler, uint32_t items, uint32_t iterations)
{
    Stats stats(iterations);

    gts::ParallelFor parallelFor(taskScheduler);

    constexpr uint32_t TOTAL_SPINS = 7;

    uint32_t spins[TOTAL_SPINS] = { 10, 10, 10, 100, 100, 100, 100000 };
    gts::Vector<PaddedInt> randStates(taskScheduler.workerCount());
    for (uint32_t ii = 0; ii < randStates.size(); ++ii)
    {
        randStates[ii].i = ii + 1;
    }

    // Do test.
    for (uint32_t ii = 0; ii < iterations; ++ii)
    {
        GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);

        auto start = std::chrono::high_resolution_clock::now();

        parallelFor(
            gts::Range1d<uint32_t>(0, items, 1),
            [spins, TOTAL_SPINS, &randStates](gts::Range1d<uint32_t>& range, void*, gts::TaskContext const& ctx)
            {
                GTS_TRACE_SCOPED_ZONE_P1(gts::analysis::CaptureMask::USER, gts::analysis::Color::DarkBlue, "Irregular ParFor Task", range.size());

                uint32_t& randState = randStates[ctx.workerId.localId()].i;

                for (uint32_t r = range.begin(); r != range.end(); ++r)
                {
                    uint32_t s = spins[gts::fastRand(randState) % TOTAL_SPINS];
                    float f = 0.45f;
                    for (volatile uint32_t ii = 0; ii < s; ++ii)
                    {
                        f = sin(f);
                    }
                }
            },
            gts::AdaptivePartitioner(4),
            nullptr);

        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double> diff = end - start;
        stats.addDataPoint(diff.count());
    }

    return stats;
}

//------------------------------------------------------------------------------
Stats irregularUpfrontParallelFor(gts::MicroScheduler& taskScheduler, uint32_t items, uint32_t iterations)
{
    Stats stats(iterations);

    gts::ParallelFor parallelFor(taskScheduler);

    constexpr float precentageofRange = 0.1f;
    uint32_t rangeOfHighCostEnd       = uint32_t(items * precentageofRange);

    // Do test.
    for (uint32_t ii = 0; ii < iterations; ++ii)
    {
        GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);

        auto start = std::chrono::high_resolution_clock::now();

        parallelFor(
            gts::Range1d<uint32_t>(0, items, 1),
            [rangeOfHighCostEnd](gts::Range1d<uint32_t>& range, void*, gts::TaskContext const&)
            {
                GTS_TRACE_SCOPED_ZONE_P1(gts::analysis::CaptureMask::USER, gts::analysis::Color::DarkBlue, "Irregular ParFor Task", range.size());

                for (uint32_t r = range.begin(); r != range.end(); ++r)
                {
                    uint32_t numSpins = 10;
                    if (r > rangeOfHighCostEnd)
                    {
                        numSpins = 100000;
                    }

                    float f = 0.45f;
                    for (volatile uint32_t ii = 0; ii < numSpins; ++ii)
                    {
                        f = sin(f);
                    }
                }
            },
            gts::AdaptivePartitioner(4),
                nullptr);

        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double> diff = end - start;
        stats.addDataPoint(diff.count());
    }

    return stats;
}
