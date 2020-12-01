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
#include <Tracy.hpp>
#include <common/TracySystem.hpp>

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

 ////////////////////////////////////////////////////////////////////////////////
 /**
 * @brief
 *  A state singleton for Tracy.
 */
class TracyState
{
public:

    enum { MAX_BUFFER_SIZE = 128 };

    //--------------------------------------------------------------------------
    /**
     * Gets the singleton instance of the TracyState.
     */
    static GTS_INLINE TracyState& inst()
    {
        static TracyState instance;
        return instance;
    }

    //--------------------------------------------------------------------------
    /**
     * Names a thread.
     */
    static GTS_INLINE void nameThread(const char* fmt, ...)
    {
        char buff[MAX_BUFFER_SIZE];
        va_list args;
        va_start(args, fmt);
        vsprintf_s(buff, fmt, args);
        va_end(args);

        tracy::SetThreadName(buff);
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

private:

    //--------------------------------------------------------------------------
    GTS_INLINE TracyState() : m_captureMask((uint64_t)-1)
    {
    }

    //! A mask used to filter events.
    uint64_t m_captureMask;
};

/** @} */ // end of Tracing
/** @} */ // end of Analysis

} // namespace analysis
} // namespace gts

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

#define GTS_TRACE_INIT

#define GTS_TRACE_NAME_THREAD(captureMask, tid, fmt, ...) if(gts::analysis::TracyState::inst().isEnabled(captureMask)) { gts::analysis::TracyState::nameThread(fmt, __VA_ARGS__); }

#define GTS_TRACE_SET_CAPTURE_MASK(captureMask) gts::analysis::TracyState::inst().setCaptureMask(captureMask)

#define GTS_TRACE_DUMP(filename)

#define GTS_TRACE_FRAME_MARK(captureMask) if(gts::analysis::TracyState::inst().isEnabled(captureMask)) { FrameMark; }

#define GTS_TRACE_SCOPED_ZONE(captureMask, hexColor, fmt, ...)                                            \
    ZoneNamedC(___tracy_scoped_zone, hexColor, gts::analysis::TracyState::inst().isEnabled(captureMask)); \
    {                                                                                                     \
        char buff[gts::analysis::TracyState::MAX_BUFFER_SIZE];                                            \
        va_list args;                                                                                     \
        va_start(args, fmt);                                                                              \
        vsprintf_s(buff, fmt, __VA_ARGS__);                                                               \
        va_end(args);                                                                                     \
        ZoneName(buff, gts::analysis::TracyState::MAX_BUFFER_SIZE);                                       \
    }

#define GTS_TRACE_SCOPED_ZONE_P0(captureMask, hexColor, txt)                                                    \
    ZoneNamedC(___tracy_scoped_zone, hexColor, gts::analysis::TracyState::inst().isEnabled(captureMask));       \
    {                                                                                                           \
        ZoneName(txt, strlen(txt));                                                                             \
    }

#define GTS_TRACE_SCOPED_ZONE_P1(captureMask, hexColor, txt, param1)                                            \
    ZoneNamedC(___tracy_scoped_zone, hexColor, gts::analysis::TracyState::inst().isEnabled(captureMask));       \
    {                                                                                                           \
        char buff[gts::analysis::TracyState::MAX_BUFFER_SIZE];                                                  \
        sprintf_s(buff, "%s, %llu", txt, (uintptr_t)param1);                                                    \
        ZoneName(buff, gts::analysis::TracyState::MAX_BUFFER_SIZE);                                             \
    }

#define GTS_TRACE_SCOPED_ZONE_P2(captureMask, hexColor, txt, param1, param2)                                    \
    ZoneNamedC(___tracy_scoped_zone, hexColor, gts::analysis::TracyState::inst().isEnabled(captureMask));       \
    {                                                                                                           \
        char buff[gts::analysis::TracyState::MAX_BUFFER_SIZE];                                                  \
        sprintf_s(buff, "%s, %llu, %llu", txt, (uintptr_t)param1, (uintptr_t)param2);                           \
        ZoneName(buff, gts::analysis::TracyState::MAX_BUFFER_SIZE);                                             \
    }

#define GTS_TRACE_SCOPED_ZONE_P3(captureMask, hexColor, txt, param1, param2, param3)                            \
    ZoneNamedC(___tracy_scoped_zone, hexColor, gts::analysis::TracyState::inst().isEnabled(captureMask));       \
    {                                                                                                           \
        char buff[gts::analysis::TracyState::MAX_BUFFER_SIZE];                                                  \
        sprintf_s(buff, "%s, %llu, %llu, %llu", txt, (uintptr_t)param1, (uintptr_t)param2, (uintptr_t)param3);  \
        ZoneName(buff, gts::analysis::TracyState::MAX_BUFFER_SIZE);                                             \
    }

#define GTS_TRACE_ZONE_MARKER(captureMask, hexColor, fmt, ...) { GTS_TRACE_SCOPED_ZONE(captureMask, hexColor, fmt, __VA_ARGS__); }
#define GTS_TRACE_ZONE_MARKER_P0(captureMask, hexColor, txt) { GTS_TRACE_SCOPED_ZONE_P0(captureMask, hexColor, txt); }
#define GTS_TRACE_ZONE_MARKER_P1(captureMask, hexColor, txt, param1) { GTS_TRACE_SCOPED_ZONE_P1(captureMask, hexColor, txt, param1); }
#define GTS_TRACE_ZONE_MARKER_P2(captureMask, hexColor, txt, param1, param2) { GTS_TRACE_SCOPED_ZONE_P2(captureMask, hexColor, txt, param1, param2); }
#define GTS_TRACE_ZONE_MARKER_P3(captureMask, hexColor, txt, param1, param2, param3) { GTS_TRACE_SCOPED_ZONE_P3(captureMask, hexColor, txt, param1, param2, param3); }

#define GTS_TRACE_PLOT(captureMask, val, txt) if(gts::analysis::TracyState::inst().isEnabled(captureMask)) { TracyPlot(txt, val); }

#define GTS_TRACE_ALLOC(captureMask, ptr, size, fmt, ...) if(gts::analysis::TracyState::inst().isEnabled(captureMask)) { TracyAlloc(ptr, size); }
#define GTS_TRACE_FREE(captureMask, ptr, size, fmt, ...) if(gts::analysis::TracyState::inst().isEnabled(captureMask)) { TracyFree(ptr); }

#define GTS_TRACE_MUTEX(type, varname) TracyLockable(type, varname)
#define GTS_TRACE_MUTEX_TYPE(type) LockableBase(type)

#define GTS_TRACE_WAIT_FOR_LOCK_START(captureMask, pLock, fmt, ...)
#define GTS_TRACE_WAIT_FOR_END(captureMask)
#define GTS_TRACE_LOCK_ACQUIRED(captureMask, pLock, fmt, ...)
#define GTS_TRACE_LOCK_RELEASED(captureMask, pLock)

