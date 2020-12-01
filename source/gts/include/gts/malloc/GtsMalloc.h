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

#include <cstdint>
#include "gts/platform/Machine.h"

#if GTS_MALLOC_SHARED_LIB
    #if GTS_MALLOC_SHARED_LIB_EXPORT
        #define GTS_MALLOC_EXPORT GTS_SHARED_LIB_EXPORT
    #else
        #define GTS_MALLOC_EXPORT GTS_SHARED_LIB_IMPORT
    #endif
#else
    #define GTS_MALLOC_EXPORT
#endif

namespace gts {
namespace internal {
    
bool verifyMemoryStoreIsFreed();

}
}

extern "C" {

/** 
 * @defgroup GtsMalloc GtsMalloc
 *  Custom parallel memory allocator.
 * @{
 */

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Common methods.

GTS_MALLOC_EXPORT void*  gts_malloc(size_t size);
GTS_MALLOC_EXPORT void*  gts_calloc(size_t count, size_t size);
GTS_MALLOC_EXPORT void*  gts_realloc(void* ptr, size_t newsize);
GTS_MALLOC_EXPORT void   gts_free(void* ptr);
GTS_MALLOC_EXPORT char*  gts_strdup(char const* pStr);
GTS_MALLOC_EXPORT size_t gts_usable_size(void* ptr);
GTS_MALLOC_EXPORT void*  gts_expand(void* ptr, size_t size);
GTS_MALLOC_EXPORT void*  gts_aligned_malloc(size_t size, size_t alignment);

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// C++
GTS_MALLOC_EXPORT void* gts_new(size_t size);
GTS_MALLOC_EXPORT void* gts_new_nothrow(size_t size);
GTS_MALLOC_EXPORT void  gts_free_size(void* ptr, size_t size);
GTS_MALLOC_EXPORT void* gts_new_aligned(size_t size, size_t alignment);
GTS_MALLOC_EXPORT void* gts_new_aligned_nothrow(size_t size, size_t alignment);
GTS_MALLOC_EXPORT void  gts_free_aligned(void* ptr, size_t alignment);
GTS_MALLOC_EXPORT void  gts_free_size_aligned(void* ptr, size_t size, size_t alignment);

#ifdef GTS_WINDOWS

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Win32

// see crtdbg.h for everything that allocates in WinAPI.

GTS_MALLOC_EXPORT void*          gts_win_recalloc(void* ptr, size_t count, size_t size);
GTS_MALLOC_EXPORT wchar_t*       gts_win_wcsdup(wchar_t const* pStr);
GTS_MALLOC_EXPORT unsigned char* gts_win_mbsdup(unsigned char const* pStr);
GTS_MALLOC_EXPORT errno_t        gts_win_dupenv_s(char** ppBuf, size_t* pNumElements, char const* pVarName);
GTS_MALLOC_EXPORT errno_t        gts_win_wdupenv_s(wchar_t** ppBuf, size_t* pNumElements, wchar_t const* pVarName);
GTS_MALLOC_EXPORT void*          gts_win_aligned_realloc(void* ptr, size_t newsize, size_t alignment);
GTS_MALLOC_EXPORT void*          gts_win_aligned_recalloc(void* ptr, size_t count, size_t size, size_t alignment);
GTS_MALLOC_EXPORT void*          gts_win_aligned_offset_malloc(size_t size, size_t alignment, size_t offset);
GTS_MALLOC_EXPORT void*          gts_win_aligned_offset_realloc(void* ptr, size_t size, size_t alignment, size_t offset);
GTS_MALLOC_EXPORT void*          gts_win_aligned_offset_recalloc(void* ptr, size_t count, size_t size, size_t alignment, size_t offset);

// TODO: add the rest?

#elif GTS_POSIX

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// POSIX

GTS_MALLOC_EXPORT void* gts_posix_reallocf(void* ptr, size_t size);
GTS_MALLOC_EXPORT void* gts_posix_valloc(size_t size);
GTS_MALLOC_EXPORT void* gts_posix_pvalloc(size_t size);
GTS_MALLOC_EXPORT void* gts_posix_reallocarray(void* ptr, size_t count, size_t size);
GTS_MALLOC_EXPORT int   gts_posix_memalign(void **memptr, size_t alignment, size_t size);

#endif

/** @} */ // end of GtsMalloc

}
