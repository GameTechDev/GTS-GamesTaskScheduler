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
#include <ittnotify.h>

#include "gts/platform/Machine.h"
#include "gts/platform/Atomic.h"
#include "gts/analysis/Instrumenter.h"

namespace gts {
namespace analysis {

class IttNotify;
static IttNotify* g_ittNotify;

////////////////////////////////////////////////////////////////////////////////
/**
 *  Hooks for the Instrumentation and Tracing Technology (ITT) API, which
 *  enbles trace data for Intel(R) VTune(TM) Amplifier XE.
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
    static GTS_INLINE void nameThreadHook(const char* format, ...)
    {
        // Set the name of this thread so it shows up in the VTUNE as something meaningful
        char buffer[256];

        va_list args;
        va_start(args);
        vsprintf_s(buffer, format, args);
        va_end(args);

        __itt_thread_set_nameA(buffer);
    }

    //--------------------------------------------------------------------------
    static GTS_INLINE void beginTaskHook(Tag::Type, const char* msg, uintptr_t param1, uintptr_t param2)
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

    __itt_domain* m_domain;
};

} // namespace analysis
} // namespace gts

