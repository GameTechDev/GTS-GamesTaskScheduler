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

#include "gts_perf/Mandelbrot.h"

#include "gts_perf/PerfTests.h"

#include <gts/micro_scheduler/WorkerPool.h>
#include <gts/micro_scheduler/MicroScheduler.h>
#include <gts/micro_scheduler/patterns/ParallelFor.h>
#include <gts/micro_scheduler/patterns/Range1d.h>

using namespace mandelbrot;

static const int MAX_DEPTH = 128;

//------------------------------------------------------------------------------
/**
 * Uniform data for the Mandelbrot set calculation.
 */
struct MandelbrotUniformData
{
    std::vector<uint32_t>* output;
    uint32_t maxDepth;
    uint32_t dimensions;
    float dx;
    float dy;
    float x0;
    float y0;
};

//------------------------------------------------------------------------------
Stats mandelbrotPerfSerial(uint32_t dimensions, uint32_t iterations)
{
    Stats stats(iterations);

    std::vector<uint32_t> mandelOut;
    mandelOut.resize(dimensions * dimensions);

    float x0 = -2;
    float x1 = 1;
    float y0 = -1;
    float y1 = 1;
    float dx = (x1 - x0) / dimensions;
    float dy = (y1 - y0) / dimensions;

    // Do test.
    for (uint32_t ii = 0; ii < iterations; ++ii)
    {
        auto start = std::chrono::high_resolution_clock::now();

        for (uint32_t r = 0; r < dimensions; ++r)
        {
            for (uint32_t c = 0; c < dimensions; ++c)
            {
                float x = x0 + c * dx;
                float y = y0 + r * dy;

                uint32_t index = (r * dimensions + c);

                mandelOut[index] = calcMandelbrot(x, y, MAX_DEPTH);
            }
        }

        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double> diff = end - start;
        stats.addDataPoint(diff.count());
    }


    return stats;
}

//------------------------------------------------------------------------------
Stats mandelbrotPerfParallel(gts::MicroScheduler& taskScheduler, uint32_t dimensions, uint32_t iterations)
{
    Stats stats(iterations);

    gts::ParallelFor parallelFor(taskScheduler);

    std::vector<uint32_t> mandelOut;
    mandelOut.resize(dimensions * dimensions);

    float x0 = -2;
    float x1 = 1;
    float y0 = -1;
    float y1 = 1;

    // Do test.
    for (uint32_t ii = 0; ii < iterations; ++ii)
    {
        GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);

        auto start = std::chrono::high_resolution_clock::now();

        MandelbrotUniformData taskData;
        taskData.output = &mandelOut;
        taskData.maxDepth = MAX_DEPTH;
        taskData.dimensions = dimensions;
        taskData.dx = (x1 - x0) / dimensions;
        taskData.dy = (y1 - y0) / dimensions;
        taskData.x0 = x0;
        taskData.y0 = y0;


        parallelFor(
            gts::Range1d<uint32_t>(0, dimensions, 1),
            [](gts::Range1d<uint32_t>& range, void* param, gts::TaskContext const&)
        {
            GTS_TRACE_SCOPED_ZONE_P1(gts::analysis::CaptureMask::USER, gts::analysis::Color::DarkBlue, "Mandelbrot Task", range.size());

            MandelbrotUniformData& data = *(MandelbrotUniformData*)param;

            for (uint32_t r = range.begin(); r != range.end(); ++r)
            {
                for (uint32_t c = 0, csize = data.dimensions; c < csize; ++c)
                {
                    float x = data.x0 + c * data.dx;
                    float y = data.y0 + r * data.dy;

                    uint32_t index = (r * csize + c);

                    (*data.output)[index] = calcMandelbrot(x, y, data.maxDepth);
                }
            }
        },
        gts::AdaptivePartitioner(3),
        &taskData);

        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double> diff = end - start;
        stats.addDataPoint(diff.count());
    }

    return stats;
}
