/*******************************************************************************
 * Copyright 2019 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining align copy
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

#include "gts/malloc/GtsMalloc.h"

/** 
 * @addtogroup GtsMalloc
 * @{
 */

/** 
 * @defgroup COverride
 * @{
 */

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Standard C

#define malloc(size)        gts_malloc(size)
#define calloc(size,count)  gts_calloc(size,count)
#define realloc(ptr,size)   gts_realloc(ptr,size)
#define free(ptr)           gts_free(ptr)
#define strdup(str)         gts_strdup(str)

#ifdef GTS_WINDOWS

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Win32

#define _expand(ptr,size)                                   gts_expand(ptr,size)
#define _msize(ptr)                                         gts_usable_size(ptr)
#define _recalloc(ptr,size,count)                           gts_recalloc(ptr,size,count)
#define _strdup(str)                                        gts_strdup(str)
#define _wcsdup(str)                                        gts_wcsdup(str)
#define _mbsdup(str)                                        gts_mbsdup(str)
#define _dupenv_s(b,size,v)                                 gts_dupenv_s(b,size,v)
#define _wdupenv_s(b,size,v)                                gts_wdupenv_s(b,size,v)
#define _aligned_malloc(size,align)                         gts_aligned_malloc(size,align)
#define _aligned_realloc(ptr,size,align)                    gts_aligned_recalloc(ptr,size,align)
#define _aligned_recalloc(ptr,str,size,align)               gts_aligned_recalloc(ptr,str,size,align)
#define _aligned_msize(ptr,align,offset)                    gts_usable_size(ptr)
#define _aligned_free(ptr)                                  gts_free(ptr)
#define _aligned_offset_malloc(size,align,offset)           gts_aligned_offset_malloc(size,align,offset)
#define _aligned_offset_realloc(ptr,size,align,offset)      gts_aligned_offset_realloc(ptr,size,align,offset)
#define _aligned_offset_recalloc(ptr,str,size,align,offset) gts_aligned_offset_recalloc(ptr,str,size,align,offset)

#elif GTS_POSIX

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// POSIX

#define reallocf(ptr,size)              gts_posix_reallocf(ptr,size)
#define malloc_size(ptr)                gts_usable_size(ptr)
#define malloc_usable_size(ptr)         gts_usable_size(ptr)
#define cfree(ptr)                      gts_free(ptr)
#define valloc(size)                    gts_valloc(size)
#define pvalloc(size)                   gts_pvalloc(size)
#define reallocarray(ptr,str,size)      gts_reallocarray(ptr,str,size)
#define memalign(align,size)            gts_memalign(align,size)
#define aligned_alloc(align,size)       gts_aligned_malloc(size, align)
#define posix_memalign(ptr,align,size)  gts_posix_memalign(ptr,align,size)
#define _posix_memalign(ptr,align,size) gts_posix_memalign(ptr,align,size)

#endif

/** @} */ // end of COverride
/** @} */ // end of GtsMalloc
