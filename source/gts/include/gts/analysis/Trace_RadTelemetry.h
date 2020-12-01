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

#include <rad_tm.h>

#include "gts/platform/Assert.h"

/** 
 * @addtogroup Analysis
 * @{
 */

/** 
 * @addtogroup Tracing
 * @{
 */

//------------------------------------------------------------------------------
inline void initTelemetry()
{
    // tmInitialize gets Telemetry started. You can pass a pointer to the memory
    // that you want Telemetry to use, or call tmInitialize(0, 0) and Telemetry
    // will allocate memory from the heap. Increasing memory gives telemetry
    // more internal buffers and helps for captures that are heavily marked up.
    tm_int32 telemetry_memory_size = 8 * 1024 * 1024;
    char* telemetry_memory = new char[telemetry_memory_size];
    tmInitialize(telemetry_memory_size, telemetry_memory);

    // Connect to a telemetry server and start profiling
    tm_error err = tmOpen(
        0,                      // unused
        "GTS",                  // program name, don't use slashes or weird character that will screw up a filename
        __DATE__ " " __TIME__,  // identifier, could be date time, or a build number ... whatever you want
        "localhost",            // telemetry server address
        TMCT_TCP,               // network capture
        4719,                   // telemetry server port
        TMOF_INIT_NETWORKING,   // flags
        100);                   // timeout in milliseconds ... pass -1 for infinite

    if (err == TMERR_DISABLED)
    {
        GTS_ASSERT(0 && "Telemetry is disabled via #define NTELEMETRY");
    }
    else if (err == TMERR_UNINITIALIZED)
    {
        GTS_ASSERT(0 && "tmInitialize failed or was not called");
    }
    else if (err == TMERR_NETWORK_NOT_INITIALIZED)
    {
        GTS_ASSERT(0 && "WSAStartup was not called before tmOpen! Call WSAStartup or pass TMOF_INIT_NETWORKING.");
    }
    else if (err == TMERR_NULL_API)
    {
        GTS_ASSERT(0 && "There is no Telemetry API (the DLL isn't in the EXE's path)!");
    }
    else if (err == TMERR_COULD_NOT_CONNECT)
    {
        GTS_ASSERT(0 && "There is no Telemetry server running");
    }
}

//------------------------------------------------------------------------------
inline void setTelemetryZoneColor(unsigned int hexColor)
{
    int r = hexColor & 0xff;
    int g = (hexColor >> 8) & 0xff;
    int b = (hexColor >> 16) & 0xff;
    tmZoneColor(r, b, g);
}

/** @} */ // end of Tracing
/** @} */ // end of Analysis

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

#define GTS_TRACE_INIT initTelemetry()

#define GTS_TRACE_NAME_THREAD(captureMask, tid, fmt, ...) tmThreadName(captureMask, tid, fmt, ... ) 

#define GTS_TRACE_SET_CAPTURE_MASK(captureMask) tmSetCaptureMask(captureMask)

#define GTS_TRACE_DUMP(filename)

#define GTS_TRACE_FRAME_MARK(captureMask) tmTick(captureMask)

#define GTS_TRACE_SCOPED_ZONE(captureMask, hexColor, fmt, ...) tmZone(captureMask, 0, fmt, __VA_ARGS__); setTelemetryZoneColor(hexColor)
#define GTS_TRACE_SCOPED_ZONE_P2(captureMask, hexColor, txt, param1, param2) tmZone(captureMask, 0, "%s, %d, %d", txt, param1, param2); setTelemetryZoneColor(hexColor)

#define GTS_TRACE_ZONE_MARKER(captureMask, hexColor, fmt, ...) { GTS_TRACE_SCOPED_ZONE(captureMask, hexColor, fmt, __VA_ARGS__); }
#define GTS_TRACE_ZONE_MARKER_P1(captureMask, hexColor, txt, param1) { GTS_TRACE_SCOPED_ZONE_P1(captureMask, hexColor, txt, param1); }
#define GTS_TRACE_ZONE_MARKER_P2(captureMask, hexColor, txt, param1, param2) { GTS_TRACE_SCOPED_ZONE_P2(captureMask, hexColor, txt, param1, param2); }
#define GTS_TRACE_ZONE_MARKER_P3(captureMask, hexColor, txt, param1, param2, param3) { GTS_TRACE_SCOPED_ZONE_P3(captureMask, hexColor, txt, param1, param2, param3); }

#define GTS_TRACE_PLOT(captureMask, val, txt) tmPlot(captureMask, TM_PLOT_UNITS_TIME_US, TM_PLOT_DRAW_LINE, val, txt)

#define GTS_TRACE_ALLOC(captureMask, ptr, size, fmt, ...) tmAlloc(captureMask, ptr, size, fmt, ...)
#define GTS_TRACE_FREE(captureMask, ptr, size, fmt, ...) tmFree(captureMask, ptr)

#define GTS_TRACE_MUTEX(captureMask, type, varname) type varname
#define GTS_TRACE_WAIT_FOR_LOCK_START(captureMask, pLock, fmt, ...) tmStartWaitForLock(captureMask, 0, pLock, fmt, ...)
#define GTS_TRACE_WAIT_FOR_END(captureMask) tmEndWaitForLock(captureMask)
#define GTS_TRACE_LOCK_ACQUIRED(captureMask, pLock, fmt, ...) tmAcquiredLock(captureMask, 0, pLock, fmt, ...)
#define GTS_TRACE_LOCK_RELEASED(captureMask, pLock) tmReleasedLock(captureMask, pLock);
