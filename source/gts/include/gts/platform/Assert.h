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

/** 
 * @addtogroup Platform
 * @{
 */

/** 
 * @defgroup Assert Assert
 *  Assert functionality.
 * @{
 */

//! The assert hook type. Consumes a string of error information.
typedef void (*assertHook)(const char*);

/**
 * @brief
 *  Sets the user defined assert hook.
 */
void setAssertHook(assertHook hook);

/**
 * @brief
 *  Gets the user defined assert hook
 */
assertHook getAssertHook();

namespace internal {

GTS_INLINE void _assert(
    bool expression,
    const char* exp,
    const char* file,
    int line)
{
    if (!expression)
    {
        assertHook hook = getAssertHook();
        if (hook)
        {
            constexpr size_t BUFF_SIZE = 2048;
            static char LOG_BUFFER[BUFF_SIZE];
        #ifdef GTS_MSVC
            sprintf_s(LOG_BUFFER, "%s, FILE: %s file (%d)", exp, file, line);
        #else
            snprintf(LOG_BUFFER, BUFF_SIZE, "%s, FILE: %s file (%d)", exp, file, line);
        #endif
            hook(LOG_BUFFER);
        }
        else
        {
            printf("MISSING ASSERT HOOK!\n");
        }

        GTS_DEBUG_BREAK();
    }
}

} // namespace internal


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
#if defined(GTS_USE_INTERNAL_ASSERTS)

/**
 * @def GTS_INTERNAL_ASSERT(expr)
 * @brief
 *  Causes execution to break when \a expr is false.
 * @param expression
 *  The boolean expression.
 * @remark
 *  Force enable by defining GTS_USE_INTERNAL_ASSERTS
 */

#if defined(GTS_HAS_CUSTOM_ASSERT_WRAPPER)

#define GTS_INTERNAL_ASSERT(expr) \
    GTS_CUSTOM_ASSERT((expr), #expr, __FILE__, __LINE__)

#else

#define GTS_INTERNAL_ASSERT(expr) \
    gts::internal::_assert((expr), #expr, __FILE__, __LINE__)

#endif // GTS_HAS_CUSTOM_ASSERT_WRAPPER

#else

#define GTS_INTERNAL_ASSERT(expr)

#endif // defined(GTS_USE_INTERNAL_ASSERTS)


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
#if defined(GTS_USE_ASSERTS) || !defined(NDEBUG)

/**
 * @def GTS_ASSERT(expr)
 * @brief
 *  Causes execution to break when \a expr is false.
 * @param expression
 *  The boolean expression.
 * @remark
 *  Force enable by defining GTS_USE_ASSERTS
 */

#if defined(GTS_HAS_CUSTOM_ASSERT_WRAPPER)

#define GTS_ASSERT(expr) \
    GTS_CUSTOM_ASSERT((expr), #expr, __FILE__, __LINE__)

#else

#define GTS_ASSERT(expr) \
    gts::internal::_assert((expr), #expr, __FILE__, __LINE__)

#endif // GTS_HAS_CUSTOM_ASSERT_WRAPPER

#else

#define GTS_ASSERT(expr)

#endif // defined(GTS_USE_ASSERTS) || !defined(NDEBUG)

/** @} */ // end of Assert
/** @} */ // end of Platform

} // namespace gts
