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

#include <stdarg.h>
#include <cstdio>
#include <ittnotify.h>

#include "gts/platform/Machine.h"

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

class IttNotify;
static IttNotify* g_ittNotify;

////////////////////////////////////////////////////////////////////////////////
/**
 *  Hooks for the Instrumentation and Tracing Technology (ITT) API, which
 *  enables trace data for Intel(R) VTune(TM) Amplifier XE.
 */
class IttNotify
{
public:

    //--------------------------------------------------------------------------
    static GTS_INLINE IttNotify& inst()
    {
        static IttNotify instance;
        g_ittNotify = &instance;
        return instance;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE void beginTask(const char* msg, uintptr_t, uintptr_t)
    {
        __itt_string_handle* handle_Task = __itt_string_handle_createA(msg);
        __itt_task_begin(m_domain, __itt_null, __itt_null, handle_Task);
    }

    //--------------------------------------------------------------------------
    GTS_INLINE void endTask()
    {
        __itt_task_end(m_domain);
    }

    //--------------------------------------------------------------------------
    /**
     * Check the capture state.
     * @remark Not thread-safe with setCaptureMask
     */
    GTS_INLINE bool isEnabled(uint64_t captureMask)
    {
        return m_captureMask & captureMask;
    }

    //--------------------------------------------------------------------------
    /**
     * Sets the capture filter mask.
     * @remark Not thread-safe with isEnabled
     */
    GTS_INLINE void setCaptureMask(uint64_t captureMask)
    {
        m_captureMask = captureMask;
    }

    //--------------------------------------------------------------------------
    static GTS_INLINE void nameThreadHook(const char* fmt, ...)
    {
        // Set the name of this thread so it shows up in the VTUNE as something meaningful
        char buffer[256];

        va_list args;
        va_start(args, fmt);
        vsprintf_s(buffer, fmt, args);
        va_end(args);

        __itt_thread_set_nameA(buffer);
    }

    //--------------------------------------------------------------------------
    static GTS_INLINE void beginTaskHook(CaptureMask::Type, const char* msg, uintptr_t param1, uintptr_t param2)
    {
        IttNotify::inst().beginTask(msg, param1, param2);
    }

    //--------------------------------------------------------------------------
    static GTS_INLINE void endTaskHook()
    {
        IttNotify::inst().endTask();
    }

private:

    IttNotify()
    {
        m_domain = __itt_domain_createA("Intel GTS");
    }

    //! An ITT Domain.
    __itt_domain* m_domain;

    //! A mask used to filter events.
    uint64_t m_captureMask;

};

/** @} */ // end of Tracing
/** @} */ // end of Analysis

} // namespace analysis
} // namespace gts

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

#define GTS_TRACE_NAME_THREAD(captureMask, tid, fmt, ...) gts::analysis::traceConcurrentLogger().nameThread(captureMask, tid, fmt, __VA_ARGS__)

#define GTS_TRACE_SET_CAPTURE_MASK(captureMask) gts::analysis::traceConcurrentLogger().setCaptureMask(captureMask)

#define GTS_TRACE_DUMP(filename)

#define GTS_TRACE_FRAME_MARK(captureMask) gts::analysis::traceConcurrentLogger().reset(captureMask);

#define GTS_TRACE_SCOPED_ZONE(captureMask, hexColor, fmt, ...) gts::analysis::traceConcurrentLogger().logf(captureMask, fmt, __VA_ARGS__);
#define GTS_TRACE_SCOPED_ZONE_P2(captureMask, hexColor, txt, param1, param2) gts::analysis::traceConcurrentLogger().logf(captureMask, txt, param1, param2);

#define GTS_TRACE_ZONE_MARKER(captureMask, hexColor, fmt, ...) { GTS_TRACE_SCOPED_ZONE(captureMask, hexColor, fmt, __VA_ARGS__) }
#define GTS_TRACE_ZONE_MARKER_P1(captureMask, hexColor, txt, param1) { GTS_TRACE_SCOPED_ZONE_P1(captureMask, hexColor, txt, param1); }
#define GTS_TRACE_ZONE_MARKER_P2(captureMask, hexColor, txt, param1, param2) { GTS_TRACE_SCOPED_ZONE_P2(captureMask, hexColor, txt, param1, param2); }
#define GTS_TRACE_ZONE_MARKER_P3(captureMask, hexColor, txt, param1, param2, param3) { GTS_TRACE_SCOPED_ZONE_P3(captureMask, hexColor, txt, param1, param2, param3); }
#define GTS_TRACE_PLOT(captureMask, val, txt)

#define GTS_TRACE_ALLOC(captureMask, ptr, size, fmt, ...)
#define GTS_TRACE_FREE(captureMask, ptr, size, fmt, ...)

#define GTS_TRACE_MUTEX(captureMask, type, varname) type varname
#define GTS_TRACE_WAIT_FOR_LOCK_START(captureMask, pLock, fmt, ...)
#define GTS_TRACE_WAIT_FOR_END(captureMask)
#define GTS_TRACE_LOCK_ACQUIRED(captureMask, pLock, fmt, ...)
#define GTS_TRACE_LOCK_RELEASED(captureMask, pLock)

