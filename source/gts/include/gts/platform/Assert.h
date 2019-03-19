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

#include <cstdio>

#include "gts/platform/Machine.h"

namespace gts {

typedef void (*assertHook)(const char*);

//! Sets the user defined assert hook.
void setAssertHook(assertHook hook);

//! Gets the user defined assert hook;
assertHook getAssertHook();

namespace internal {

GTS_INLINE void _assert(bool expression, const char *exp, const char* file, int line)
{
    if (!expression)
    {
        assertHook hook = getAssertHook();
        if (hook)
        {
            static char LOG_BUFFER[2048];
            sprintf_s(LOG_BUFFER, "%s, FILE: %s file (%d)", exp, file, line);
            hook(LOG_BUFFER);
        }
        else
        {
            printf("%s, FILE: %s file (%d)", exp, file, line);
        }

        GTS_DEBUG_BREAK();
    }
}

} // namespace internal
} // namespace gts

#if defined(GTS_USE_ASSERTS) || !defined(NDEBUG)

#define GTS_ASSERT(expr) gts::internal::_assert((expr), #expr, __FILE__, __LINE__)

#else

#define GTS_ASSERT(expr)

#endif // defined(GTS_USE_ASSERTS) || !defined(NDEBUG)
