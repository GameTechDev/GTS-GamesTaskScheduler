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

namespace gts {
namespace sim_trace {

/** 
 * @addtogroup Analysis
 * @{
 */

/** 
 * @addtogroup Simulation
 * @{
 */

// Marker for HW simulators.
enum Markers : int
{
    MARKER_KERNEL_PAR_FOR = 500,

    MARKER_ALLOCATE_TASK_BEGIN,
    MARKER_ALLOCATE_TASK_END,

    MARKER_FREE_TASK_BEGIN,
    MARKER_FREE_TASK_END,

    MARKER_LOCAL_TASKLOOP_BEGIN,
    MARKER_LOCAL_TASKLOOP_END,

    MARKER_NONLOCAL_TASKLOOP_BEGIN,
    MARKER_NONLOCAL_TASKLOOP_END,

    MARKER_PUSH_TASK_BEGIN,
    MARKER_PUSH_TASK_END,

    MARKER_TAKE_TASK_BEGIN,
    MARKER_TAKE_TASK_END,

    MARKER_STEAL_TASK_BEGIN,
    MARKER_STEAL_TASK_END,

    MARKER_CLEANUP_TASK_BEGIN,
    MARKER_CLEANUP_TASK_END,

    MARKER_PARTITIONER_BEGIN,
    MARKER_PARTITIONER_END,
};

GTS_OPTIMIZE_OFF_BEGIN
/**
 * @brief
 *  Breadcrumb generator for simulators.
 */
inline void marker(int id)
{
    unsigned int cpu_info[4];
    GTS_CPU_ID(cpu_info, 0x4711 + (id * 65536));
}
GTS_OPTIMIZE_OFF_END

/** @} */ // end of Simulation
/** @} */ // end of Analysis

} // sim_trace
} // namespace gts

#ifdef GTS_ENABLE_SIM_TRACE
#define GTS_SIM_TRACE_MARKER(id) gts::sim_trace::marker(id)
#else
#define GTS_SIM_TRACE_MARKER(id)
#endif

#ifdef GTS_ENABLE_SIM_TRACE
#define GTS_SIM_TRACE_MARKER(id) gts::sim_trace::marker(id)
#else
#define GTS_SIM_TRACE_MARKER(id)
#endif


/**
 * @def GTS_TRACE_INIT()
 * @brief Initializes the trace library.
 */

/**
 * @def GTS_TRACE_SET_CAPTURE_MASK(captureMask)
 * @brief Names the current thread.
 * @param captureMask Defines which bits are allowed in the trace.
 */

/**
 * @def GTS_TRACE_NAME_THREAD(captureMask, tid, fmt, ...)
 * @brief Names the current thread.
 * @param captureMask Used to filter.
 * @param tid The thread's OS ID.
 * @param fmt A printf-like format specification.
 * @param ... Arguments for format specification.
 */

// TODO: Finish Trace Documentation.

#ifndef GTS_HAS_CUSTOM_TRACE_WRAPPER

#include "gts/analysis/TraceCaptureMask.h"
#include "gts/analysis/TraceColors.h"

#if GTS_TRACE_USE_RAD_TELEMETRY == 1

#include "gts/analysis/Trace_RadTelemetry.h"

#elif GTS_TRACE_USE_TRACY == 1

#include "gts/analysis/Trace_Tracy.h"

#elif GTS_TRACE_CONCURRENT_USE_LOGGER == 1

#include "gts/analysis/Trace_ConcurrentLogger.h"

#elif GTS_TRACE_USE_ITT == 1

#include "gts/analysis/Trace_IttNotify.h"

#elif GTS_TRACE_USE_CUSTOM == 1

#include "gts/analysis/Trace_IttNotify.h"

#else // GTS_TRACE_DISABLED

#define GTS_TRACE_INIT

#define GTS_TRACE_DUMP(filename) GTS_UNREFERENCED_PARAM(filename)

#define GTS_TRACE_NAME_THREAD(captureMask, tid, fmt, ...)

#define GTS_TRACE_SET_CAPTURE_MASK(captureMask) GTS_UNREFERENCED_PARAM(captureMask)

#define GTS_TRACE_FRAME_MARK(captureMask)

#define GTS_TRACE_SCOPED_ZONE(captureMask, color, fmt, ...)
#define GTS_TRACE_SCOPED_ZONE_P0(captureMask, color, txt)
#define GTS_TRACE_SCOPED_ZONE_P1(captureMask, color, txt, param1)
#define GTS_TRACE_SCOPED_ZONE_P2(captureMask, color, txt, param1, param2)
#define GTS_TRACE_SCOPED_ZONE_P3(captureMask, color, txt, param1, param2, param3)

#define GTS_TRACE_ZONE_MARKER(captureMask, color, fmt, ...)
#define GTS_TRACE_ZONE_MARKER_P0(captureMask, color, txt)
#define GTS_TRACE_ZONE_MARKER_P1(captureMask, color, txt, param1)
#define GTS_TRACE_ZONE_MARKER_P2(captureMask, color, txt, param1, param2)
#define GTS_TRACE_ZONE_MARKER_P3(captureMask, color, txt, param1, param2, param3)

#define GTS_TRACE_PLOT(captureMask, val, txt)

#define GTS_TRACE_ALLOC(captureMask, ptr, size, fmt, ...)
#define GTS_TRACE_FREE(captureMask, ptr, fmt, ...)

#define GTS_TRACE_MUTEX(type, varname) type varname
#define GTS_TRACE_MUTEX_TYPE(type) type
#define GTS_TRACE_WAIT_FOR_LOCK_START(captureMask, pLock, fmt, ...);
#define GTS_TRACE_WAIT_FOR_END(captureMask)
#define GTS_TRACE_LOCK_ACQUIRED(captureMask, pLock, fmt, ...)
#define GTS_TRACE_LOCK_RELEASED(captureMask, pLock)

#endif // GTS_TRACING

#endif // GTS_HAS_CUSTOM_TRACE_WRAPPER
