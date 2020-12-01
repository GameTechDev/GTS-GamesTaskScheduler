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

#if GTS_MALLOC_SHARED_LIB
    #if GTS_MALLOC_REDIRECT_LIB_EXPORT
        #define GTS_MALLOC_REDIRECT_EXPORT GTS_SHARED_LIB_EXPORT
    #else
        #define GTS_MALLOC_REDIRECT_EXPORT GTS_SHARED_LIB_IMPORT
    #endif
#else
    #define GTS_MALLOC_REDIRECT_EXPORT
#endif

extern "C" {

#if !defined(GTS_WINDOWS) && !defined(GTS_MALLOC_SHARED_LIB)
#error "Redirection DLL is only used on Windows platforms."
#endif

/** 
 * @addtogroup GtsMalloc
 * @{
 */

/** 
 * @defgroup Redirect
 * @{
 */

/**
 * @brief 
 *  Dummy function that loads an implicitly linked DLL on Windows platforms.
 *  Call in main function or link with /INCLUDE:gts_malloc_redirect (later seems to only work in x64).
 */
GTS_MALLOC_REDIRECT_EXPORT void gts_malloc_redirect();

/** @} */ // end of Redirect
/** @} */ // end of GtsMalloc

}
