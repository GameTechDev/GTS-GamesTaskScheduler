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

#include <vector>

#include "gts/analysis/Trace.h"
#include "gts/analysis/ConcurrentLogger.h"
#include "gts/micro_scheduler/WorkerPool.h"
#include "gts/micro_scheduler/MicroScheduler.h"
#include "gts/micro_scheduler/patterns/ParallelFor.h"

using namespace gts;

namespace gts_examples {

// To enable the instrumentation, see the defines in gts/analysis/Trace.h.

// GTS comes with following hooks:
// gts/analysis/Trace_ConcurrentLogger.h
// gts/analysis/Trace_IttNotify.h
// gts/analysis/Trace_RadTelemetry.h
// gts/analysis/Trace_Tracy.h

// Custom hook can be implemented with 

//------------------------------------------------------------------------------
void instrumentationBasics()
{
    // MARKER:

    // An instantaneous marker. P0 means no parameters.
    GTS_TRACE_ZONE_MARKER_P0(analysis::CaptureMask::ALL, analysis::Color::Red, "I happened here!");

    // SCOPED REGIONS:

    // A scoped marker. P0 means no parameters.
    GTS_TRACE_SCOPED_ZONE_P0(analysis::CaptureMask::ALL, analysis::Color::Red, "Scope Zone");
}

//------------------------------------------------------------------------------
void instrumentationGameLoop()
{
    // To enable the macros, define GTS_ENABLE_CONCRT_LOGGER. In this example
    // you can set the configuration to DebugWithInstrument or RelWithInstrument
    // to create a log.

    // Init boilerplate
    WorkerPool workerPool;
    bool result = workerPool.initialize(2);
    GTS_ASSERT(result);
    MicroScheduler taskScheduler;
    result = taskScheduler.initialize(&workerPool);
    GTS_ASSERT(result);
    ParallelFor parallelFor(taskScheduler);

    // In a game, you can dump per frame like so:

    char filename[32];
    int frameId = 0;
    while (true) // update loop.
    {
        // Mark the start of your frame.
        GTS_TRACE_FRAME_MARK(analysis::CaptureMask::ALL);

        // Pretend workload so we can see some tasks.
        uint32_t const elementCount = 1 << 10;
        std::vector<char> vec(elementCount, 0);

        parallelFor(
            BlockedRange1d<std::vector<char>::iterator>(vec.begin(), vec.end(), 1),
            [](BlockedRange1d<std::vector<char>::iterator>& range, void*, TaskContext const&)
            {
                
            },
            AdaptivePartitioner(),
            nullptr,
            nullptr
        );

        // Dump the frame data, where applicable.
        sprintf_s(filename, "frame_%d.txt", frameId);
        GTS_TRACE_DUMP(filename);

        ++frameId;

        break; // just an example, don't really loop.
    }


    // See: ConcurrentLogger.h for finer control over the logger.

    taskScheduler.shutdown();
    workerPool.shutdown();
}

} // namespace gts_examples
