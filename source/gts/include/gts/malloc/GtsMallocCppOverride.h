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

#include <new>
#include "gts/malloc/GtsMalloc.h"

// https://en.cppreference.com/w/cpp/memory/new/operator_new
// https://en.cppreference.com/w/cpp/memory/new/operator_delete
// https://docs.microsoft.com/en-us/cpp/build/reference/zc-cplusplus?view=vs-2019

/** 
 * @addtogroup GtsMalloc
 * @{
 */

/** 
 * @defgroup CppOverride
 * @{
 */

void* operator new  (std::size_t n) noexcept(false) { return gts_new(n); }
void* operator new[](std::size_t n) noexcept(false) { return gts_new(n); }

void* operator new  (std::size_t n, const std::nothrow_t& tag) noexcept { (void)(tag); return gts_new_nothrow(n); }
void* operator new[](std::size_t n, const std::nothrow_t& tag) noexcept { (void)(tag); return gts_new_nothrow(n); }

void operator delete  (void* ptr) noexcept { gts_free(ptr); };
void operator delete[](void* ptr) noexcept { gts_free(ptr); };

#if (__cplusplus >= 201402L)
void operator delete  (void* ptr, std::size_t n) noexcept { gts_free_size(ptr,n); };
void operator delete[](void* ptr, std::size_t n) noexcept { gts_free_size(ptr,n); };
#endif

#if (__cplusplus > 201402L)
void* operator new  ( std::size_t n, std::align_val_t align) noexcept(false) { return gts_new_aligned(n, static_cast<size_t>(align)); }
void* operator new[]( std::size_t n, std::align_val_t align) noexcept(false) { return gts_new_aligned(n, static_cast<size_t>(align)); }
void* operator new  (std::size_t n, std::align_val_t align, const std::nothrow_t&) noexcept { return gts_new_aligned_nothrow(n, static_cast<size_t>(align)); }
void* operator new[](std::size_t n, std::align_val_t align, const std::nothrow_t&) noexcept { return gts_new_aligned_nothrow(n, static_cast<size_t>(align)); }

void operator delete  (void* ptr, std::align_val_t align) noexcept { gts_free_aligned(ptr, static_cast<size_t>(align)); }
void operator delete[](void* ptr, std::align_val_t align) noexcept { gts_free_aligned(ptr, static_cast<size_t>(align)); }
void operator delete  (void* ptr, std::size_t n, std::align_val_t align) noexcept { gts_free_size_aligned(ptr, n, static_cast<size_t>(align)); };
void operator delete[](void* ptr, std::size_t n, std::align_val_t align) noexcept { gts_free_size_aligned(ptr, n, static_cast<size_t>(align)); };
#endif

/** @} */ // end of CppOverride
/** @} */ // end of GtsMalloc
