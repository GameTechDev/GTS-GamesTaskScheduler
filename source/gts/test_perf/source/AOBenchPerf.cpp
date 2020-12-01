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
#include "gts_perf/PerfTests.h"

#include <chrono>

#include "gts_perf/AOBench.h"

#include <gts/micro_scheduler/WorkerPool.h>
#include <gts/micro_scheduler/MicroScheduler.h>
#include <gts/micro_scheduler/patterns/ParallelFor.h>
#include <gts/micro_scheduler/patterns/KdRange2d.h>

using namespace ao_bench;

//------------------------------------------------------------------------------
struct AoUniformData
{
    Plane plane;
    Sphere* spheres;
    uint32_t w;
    uint32_t h;
    uint32_t nsubsamples;
    float* image;
};

//------------------------------------------------------------------------------
Stats aoBenchPerfSerial(uint32_t w, uint32_t h, uint32_t nsubsamples, uint32_t iterations)
{
    Stats stats(iterations);

    float* image = new float[w * h * 3];

    Plane plane = { Vec3(0.0f, -0.5f, 0.0f), Vec3(0.f, 1.f, 0.f) };
    Sphere spheres[3] = {
        { Vec3(-2.0f, 0.0f, -3.5f), 0.5f },
        { Vec3(-0.5f, 0.0f, -3.0f), 0.5f },
        { Vec3(1.0f, 0.0f, -2.2f), 0.5f } };

    // Do test.
    for (uint32_t ii = 0; ii < iterations; ++ii)
    {
        auto start = std::chrono::high_resolution_clock::now();

        ao_scanlines(0, w, 0, h, w, h, nsubsamples, image, plane, spheres);

        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double> diff = end - start;
        stats.addDataPoint(diff.count());
    }

    delete[] image;

    return stats;
}

//------------------------------------------------------------------------------
Stats aoBenchPerfParallel(gts::MicroScheduler& taskScheduler, uint32_t w, uint32_t h, uint32_t nsubsamples, uint32_t iterations)
{
    Stats stats(iterations);

    gts::ParallelFor parallelFor(taskScheduler);

    float* image = new float[w * h * 3];

    Plane plane = { Vec3(0.0f, -0.5f, 0.0f), Vec3(0.f, 1.f, 0.f) };
    Sphere spheres[3] = {
        { Vec3(-2.0f, 0.0f, -3.5f), 0.5f },
        { Vec3(-0.5f, 0.0f, -3.0f), 0.5f },
        { Vec3(1.0f, 0.0f, -2.2f), 0.5f } };


    // Do test.
    for (uint32_t ii = 0; ii < iterations; ++ii)
    {
        GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);

        auto start = std::chrono::high_resolution_clock::now();

        AoUniformData taskData;
        taskData.w = w;
        taskData.h = h;
        taskData.nsubsamples = nsubsamples;
        taskData.image = image;
        taskData.plane = plane;
        taskData.spheres = spheres;

        parallelFor(
            gts::KdRange2d<int>(0, w, 1, 0, h, 16),
            [](gts::KdRange2d<int>& range, void* param, gts::TaskContext const&)
            {
                GTS_TRACE_SCOPED_ZONE_P1(gts::analysis::CaptureMask::MICRO_SCEHDULER_ALL, gts::analysis::Color::DarkBlue, "AOBench Task", range.size());

                AoUniformData& data = *(AoUniformData*)param;
                
                ao_scanlines(
                    range.xRange().begin(),
                    range.xRange().end(),
                    range.yRange().begin(),
                    range.yRange().end(),
                    data.w,
                    data.h,
                    data.nsubsamples,
                    data.image,
                    data.plane,
                    data.spheres);
            },
            gts::AdaptivePartitioner(),
            &taskData);

        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double> diff = end - start;
        stats.addDataPoint(diff.count());
    }

    GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);

    delete[] image;

    return stats;
}
