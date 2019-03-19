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

#include "gts/analysis/Instrumenter.h"
#include "gts/analysis/ConcurrentLogger.h"

using namespace gts;

namespace gts_examples {

// To enable the instrumentation, define GTS_ENABLE_INSTRUMENTER.

// The GTS instrumenter is a wrapper around a concrete instrumentation framework
// that give GTS the ability to output debugging and performance markers.
// By default, GTS uses a concurrent logger to store the markers, however
// it can be overridden to output to any frame work. To override call
// the following functions:
//
// void setInstrumenterNameThreadFunc(InstrumenterNameThreadFunc hook);
// void setInstrumenterBeginFunc(InstrumenterBeginFunc hook);
// void setInstrumenterEndFunc(InstrumenterEndFunc hook);
// void setInstrumenterDebugMarkerFunc(InstrumenterDebugMarkerFunc hook);


// GTS comes with hooks Intel ITT and RAD Game Tool Telemetry. See:
//
// analysis/IttNotify.h
// analysis/RadTelemetry.h

//------------------------------------------------------------------------------
void instrumentationBasics()
{
    // MARKER:

    // An instantaneous marker.
    GTS_INSTRUMENTER_MARKER(gts::analysis::Tag::ANY, "I happened here!", 0, 0);

    // REGIONS:

    // Mark the beginning of a region. Must call end to close.
    GTS_INSTRUMENTER_BEGIN(gts::analysis::Tag::ANY, "Mark Begin", 0, 0);

    // Mark the end of a region. Must pair with a begin.
    GTS_INSTRUMENTER_END(gts::analysis::Tag::ANY);

    {
        // Uses RAII delimit the region in scope.
        GTS_INSTRUMENTER_SCOPED(gts::analysis::Tag::ANY, "Mark Scope", 0, 0);
    }
}

//------------------------------------------------------------------------------
void instrumentationConcurrentLoggerUsage()
{
    // To enable the macros, define GTS_ENABLE_CONCRT_LOGGER.


    // If using the default ConcurrentLogger, you can dump the log to a file like so:
    GTS_CONCRT_LOGGER_DUMP("LoggerDump.txt");


    // In a game, you can dump per frame like so:

    char filename[32];
    int frameId = 0;
    while (true)
    {
        // ... update simulation ...

        // Dump the frame data.
        sprintf_s(filename, "frame_%d.txt", frameId);
        GTS_CONCRT_LOGGER_DUMP(filename);

        // Clear the log for the next frame
        GTS_CONCRT_LOGGER_RESET();

        ++frameId;

        break; // just an example, don't really loop.
    }


    // See: ConcurrentLogger.h for finer control over the logger.
}

//------------------------------------------------------------------------------
void instrumentationITTUsage()
{
    // To enable ITT, define GTS_USE_ITT.
}

//------------------------------------------------------------------------------
void instrumentationTelemetryUsage()
{
    // To enable Telemetry, define GTS_USE_TELEMETRY.

    // NOTE: Telemetry is expected to be initialized outside of the GTS
    // framework. See Telemetry documentation for instructions.
}

} // namespace gts_examples
