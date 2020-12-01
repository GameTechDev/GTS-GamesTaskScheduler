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

#include <cstddef>
#include <utility>

#include "gts/platform/Machine.h"

#ifndef DOXYGEN_SHOULD_SKIP_THIS

#ifndef GTS_HAS_CUSTOM_COMPILER_MEMORY_WRAPPERS

    #ifdef GTS_USE_GTS_MALLOC

        #include "gts/malloc/GtsMalloc.h"

        #define GTS_MALLOC(size) gts_malloc(size)
        #define GTS_FREE(ptr) gts_free(ptr)

        #ifdef GTS_WINDOWS
            #define GTS_ALIGNED_MALLOC(size, alignment) gts_aligned_malloc(size, alignment)
            #define GTS_ALIGNED_FREE(ptr) gts_free(ptr)
        #elif GTS_POSIX
            #define GTS_ALIGNED_MALLOC(size, alignment) gts_aligned_malloc(size, alignment)
            #define GTS_ALIGNED_FREE(ptr) gts_free(ptr)
        #else
            #define GTS_ALIGNED_MALLOC #error "not implemented"
            #define GTS_ALIGNED_FREE #error "not implemented"
        #endif

    #else

        #define GTS_MALLOC(size) ::malloc(size)
        #define GTS_FREE(ptr) ::free(ptr)

        #ifdef GTS_WINDOWS
            #define GTS_ALIGNED_MALLOC(size, alignment) ::_aligned_malloc(size, alignment)
            #define GTS_ALIGNED_FREE(ptr) ::_aligned_free(ptr)
        #elif GTS_POSIX
            #define GTS_ALIGNED_MALLOC(size, alignment) ::aligned_alloc(alignment, size)
            #define GTS_ALIGNED_FREE(ptr) ::free(ptr)
        #else
            #define GTS_ALIGNED_MALLOC #error "not implemented"
            #define GTS_ALIGNED_FREE #error "not implemented"
        #endif

    #endif // GTS_USE_GTS_MALLOC

#endif // GTS_HAS_CUSTOM_COMPILER_MEMORY_WRAPPERS

#endif // DOXYGEN_SHOULD_SKIP_THIS

namespace gts {

/**
 * @addtogroup Platform
 * @{
 */

 /**
 * @defgroup Memory Memory
 *  Wrappers around standard lib and OS memory libraries.
 * @{
 */

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// HEAP MEMORY:

//------------------------------------------------------------------------------
/**
 * @brief
 * Allocates and construct a object of T at T's natural alignment. Allocates
 * with GTS_MALLOC.
 */
template<typename T, typename... TArgs>
GTS_INLINE T* unalignedNew(TArgs&&... args)
{
    T* ptr = (T*)GTS_MALLOC(sizeof(T));
    return new (ptr) T(std::forward<TArgs>(args)...);
}

//------------------------------------------------------------------------------
/**
 * @brief
 * Allocates and construct an array of T. Allocates with GTS_MALLOC.
 */
template<typename T>
GTS_INLINE T* unalignedVectorNew(const size_t n)
{
    T* ptr = (T*)GTS_MALLOC(sizeof(T) * n);
    for (size_t ii = 0; ii < n; ++ii)
    {
        new (ptr + ii) T();
    }
    return ptr;
}

//------------------------------------------------------------------------------
/**
 * @brief
 * Frees and destructs an object T. Memory is freed with GTS_FREE.
 */
template<typename T>
GTS_INLINE void unalignedDelete(T* ptr)
{
    if (ptr != nullptr)
    {
        ptr->~T();
        GTS_FREE(ptr);
    }
}

//------------------------------------------------------------------------------
/**
 * @brief
 * Frees and destructs an array of T objects. Memory is freed with GTS_FREE.
 */
template<typename T>
GTS_INLINE void unalignedVectorDelete(T* ptr, const size_t n)
{
    for (size_t ii = 0; ii < n; ++ii)
    {
        if (ptr != nullptr)
        {
            (ptr + ii)->~T();
        }
    }
    GTS_FREE(ptr);
}

//------------------------------------------------------------------------------
/**
 * @brief
 * Allocates and construct a object of T with the specified alignment. Allocates
 * with GTS_ALIGNED_MALLOC.
 */
template<typename T, size_t ALIGNEMENT, typename... TArgs>
GTS_INLINE T* alignedNew(TArgs&&... args)
{
    T* ptr = (T*)GTS_ALIGNED_MALLOC(sizeof(T), ALIGNEMENT);
    return new (ptr) T(std::forward<TArgs>(args)...);
}

//------------------------------------------------------------------------------
/**
 * @brief
 * Allocates and construct an array of T with the specified alignment. Allocates
 * with GTS_ALIGNED_MALLOC.
 */
template<typename T, size_t ALIGNEMENT>
GTS_INLINE T* alignedVectorNew(const size_t n)
{
    T* ptr = (T*)GTS_ALIGNED_MALLOC(sizeof(T) * n, ALIGNEMENT);
    for (size_t ii = 0; ii < n; ++ii)
    {
        new (ptr + ii) T();
    }
    return ptr;
}

//------------------------------------------------------------------------------
/**
 * @brief
 * Frees and destructs an object T. Memory is freed with GTS_ALIGNED_FREE.
 */
template<typename T>
GTS_INLINE void alignedDelete(T* ptr)
{
    if (ptr != nullptr)
    {
        ptr->~T();
        GTS_ALIGNED_FREE(ptr);
    }
}

//------------------------------------------------------------------------------
/**
 * @brief
 * Frees and destructs the array of T. Memory is freed with GTS_ALIGNED_FREE.
 */
template<typename T>
GTS_INLINE void alignedVectorDelete(T* ptr, const size_t n)
{
    for (size_t ii = 0; ii < n; ++ii)
    {
        if (ptr != nullptr)
        {
            (ptr + ii)->~T();
        }
    }
    GTS_ALIGNED_FREE(ptr);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// VIRTUAL MEMORY:

#ifndef DOXYGEN_SHOULD_SKIP_THIS

#ifndef GTS_HAS_CUSTOM_OS_MEMORY_WRAPPERS

namespace internal {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *	Encapsulates OS memory utilities.
 */
struct Memory
{
    static size_t getAllocationGranularity();
    static size_t getPageSize();
    static size_t getLargePageSize();
    static bool enableLargePageSupport();
    static void* osHeapAlloc(size_t size);
    static bool osHeapFree(void* ptr, size_t size);
    static void* osVirtualAlloc(void* pHint, size_t size, bool commit, bool largePage);
    static void* osVirtualCommit(void* ptr, size_t size);
    static bool osVirtualDecommit(void* ptr, size_t size);
    static bool osVirtualFree(void* ptr, size_t size = 0);

};
} // namespace internal

#define GTS_GET_OS_ALLOCATION_GRULARITY() gts::internal::Memory::getAllocationGranularity()
#define GTS_GET_OS_PAGE_SIZE() gts::internal::Memory::getPageSize()
#define GTS_GET_OS_LARGE_PAGE_SIZE() gts::internal::Memory::getLargePageSize()
#define GTS_ENABLE_LARGE_PAGE_SUPPORT() gts::internal::Memory::enableLargePageSupport()
#define GTS_OS_HEAP_ALLOCATE(size) gts::internal::Memory::osHeapAlloc(size)
#define GTS_OS_HEAP_FREE(ptr, size) gts::internal::Memory::osHeapFree(ptr, size)
#define GTS_OS_VIRTUAL_ALLOCATE(ptr, size, commit, largePage) gts::internal::Memory::osVirtualAlloc(ptr, size, commit, largePage)
#define GTS_OS_VIRTUAL_COMMIT(ptr, size) gts::internal::Memory::osVirtualCommit(ptr, size)
#define GTS_OS_VIRTUAL_DECOMMIT(ptr, size) gts::internal::Memory::osVirtualDecommit(ptr, size)
#define GTS_OS_VIRTUAL_FREE(ptr, size) gts::internal::Memory::osVirtualFree(ptr, size)

#endif // GTS_HAS_CUSTOM_OS_MEMORY_WRAPPERS

#endif // DOXYGEN_SHOULD_SKIP_THIS

/** @} */ // end of Utilities
/** @} */ // end of Platform

} // namespace gts
