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

#include "gts/platform/Machine.h"
#include "gts/analysis/ConcurrentLogger.h"

namespace gts {
namespace analysis {

/** 
 * @addtogroup Analysis
 * @{
 */

/** 
 * @addtogroup Tracing
 * @{
 */

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A singleton ConcurrentLogger for tracing.
 */
class TraceConcurrentLogger
{
public:

    //------------------------------------------------------------------------------
    /**
     * @return The singleton instance of a ConcurrentLogger for tracing.
     */
    static GTS_INLINE ConcurrentLogger& inst()
    {
        static ConcurrentLogger instance;
        return instance;
    }

private:

    TraceConcurrentLogger() {}
};

/** @} */ // end of Tracing
/** @} */ // end of Analysis

} // namespace gts
} // namespace analysis

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

#define GTS_TRACE_INIT

#define GTS_TRACE_NAME_THREAD(captureMask, tid, fmt, ...) gts::analysis::TraceConcurrentLogger::inst().nameThread(captureMask, fmt, __VA_ARGS__)

#define GTS_TRACE_SET_CAPTURE_MASK(captureMask) gts::analysis::TraceConcurrentLogger::inst().setCaptureMask(captureMask)

#define GTS_TRACE_DUMP(filename) { \
    size_t eventCount = 0; \
    gts::analysis::TraceConcurrentLogger::inst().lock(); \
    gts::analysis::ConcurrentLoggerEvent* pEvents = gts::analysis::TraceConcurrentLogger::inst().getEvents(&eventCount); \
    gts::analysis::printLogToTextFile(filename, gts::analysis::TraceConcurrentLogger::inst().getThreadNamesMap(), pEvents, eventCount); \
    gts::analysis::TraceConcurrentLogger::inst().unlock(); }

#define GTS_TRACE_FRAME_MARK(captureMask) gts::analysis::TraceConcurrentLogger::inst().reset(captureMask);

#define GTS_TRACE_SCOPED_ZONE_P0(captureMask, hexColor, fmt, ...) gts::analysis::TraceConcurrentLogger::inst().logf(captureMask, fmt, __VA_ARGS__);
#define GTS_TRACE_SCOPED_ZONE_P1(captureMask, hexColor, txt, param1) gts::analysis::TraceConcurrentLogger::inst().logp(captureMask, txt, (uintptr_t)param1);
#define GTS_TRACE_SCOPED_ZONE_P2(captureMask, hexColor, txt, param1, param2) gts::analysis::TraceConcurrentLogger::inst().logp(captureMask, txt, (uintptr_t)param1, (uintptr_t)param2);
#define GTS_TRACE_SCOPED_ZONE_P3(captureMask, hexColor, txt, param1, param2, param3) gts::analysis::TraceConcurrentLogger::inst().logp(captureMask, txt, (uintptr_t)param1, (uintptr_t)param2, (uintptr_t)param3);

#define GTS_TRACE_ZONE_MARKER_P0(captureMask, hexColor, fmt, ...) { GTS_TRACE_SCOPED_ZONE_P0(captureMask, hexColor, fmt, __VA_ARGS__); }
#define GTS_TRACE_ZONE_MARKER_P1(captureMask, hexColor, txt, param1) { GTS_TRACE_SCOPED_ZONE_P1(captureMask, hexColor, txt, param1); }
#define GTS_TRACE_ZONE_MARKER_P2(captureMask, hexColor, txt, param1, param2) { GTS_TRACE_SCOPED_ZONE_P2(captureMask, hexColor, txt, param1, param2); }
#define GTS_TRACE_ZONE_MARKER_P3(captureMask, hexColor, txt, param1, param2, param3) { GTS_TRACE_SCOPED_ZONE_P3(captureMask, hexColor, txt, param1, param2, param3); }

#define GTS_TRACE_PLOT(captureMask, val, txt)

#define GTS_TRACE_ALLOC(captureMask, ptr, size, fmt, ...)
#define GTS_TRACE_FREE(captureMask, ptr, size, fmt, ...)

#define GTS_TRACE_MUTEX(type, varname) type varname
#define GTS_TRACE_WAIT_FOR_LOCK_START(captureMask, pLock, fmt, ...)
#define GTS_TRACE_WAIT_FOR_END(captureMask)
#define GTS_TRACE_LOCK_ACQUIRED(captureMask, pLock, fmt, ...)
#define GTS_TRACE_LOCK_RELEASED(captureMask, pLock)
