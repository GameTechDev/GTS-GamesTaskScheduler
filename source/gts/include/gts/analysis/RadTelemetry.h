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

#include <varargs.h>
#include <rad_tm_stub.h> // <------ Replace with <rad_tm.h>

#include "gts/platform/Machine.h"
#include "gts/platform/Atomic.h"
#include "gts/analysis/Instrumenter.h"

namespace gts {
namespace analysis {

class RadTelemetry;
static RadTelemetry* g_ittNotify;

////////////////////////////////////////////////////////////////////////////////
/**
 *  Hooks for Rad Game Tool Telemetry
 */
class RadTelemetry
{
public:

    //--------------------------------------------------------------------------
    static GTS_INLINE RadTelemetry& inst()
    {
        static RadTelemetry instance;
        g_ittNotify = &instance;
        return instance;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE void message(Tag::Type tag, const char* msg, uintptr_t, uintptr_t)
    {
        tmMessage((tm_uint32)tag, TMMF_SEVERITY_LOG, msg);
    }

    //--------------------------------------------------------------------------
    GTS_INLINE void enterZone(Tag::Type tag, const char* msg, uintptr_t, uintptr_t)
    {
        tmEnter((tm_uint32)tag, 0, msg);

    }

    //--------------------------------------------------------------------------
    GTS_INLINE void leaveZone(Tag::Type tag)
    {
        tmLeave((tm_uint32)tag);
    }

    //--------------------------------------------------------------------------
    static GTS_INLINE void nameThreadHook(const char* format, ...)
    {
        // Set the name of this thread so it shows up in the VTUNE as something meaningful
        char buffer[256];

        va_list args;
        va_start(args);
        vsprintf_s(buffer, format, args);
        va_end(args);

        tmThreadName(
            0,        // Capture mask (0 means capture everything)
            0,        // Thread id (0 means use the current thread)
            buffer    // Name of the thread
        );

    }

    //--------------------------------------------------------------------------
    static GTS_INLINE void beginTaskHook(Tag::Type tag, const char* msg, uintptr_t param1, uintptr_t param2)
    {
        RadTelemetry::inst().enterZone(tag, msg, param1, param2);
    }

    //--------------------------------------------------------------------------
    static GTS_INLINE void endTaskHook(Tag::Type tag)
    {
        RadTelemetry::inst().leaveZone(tag);
    }

    //--------------------------------------------------------------------------
    static GTS_INLINE void logHook(Tag::Type tag, const char* msg, uintptr_t param1, uintptr_t param2))
    {
        RadTelemetry::inst().message(tag, msg, param1, param2);
    }
};

} // namespace analysis
} // namespace gts

