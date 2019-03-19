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

namespace gts {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// HEAP MEMORY:

namespace hooks {

//------------------------------------------------------------------------------

typedef void*(*allocHook)(size_t);

//! Sets the user defined alloc hook.
void setAllocHook(allocHook hook);

//! Gets the user defined alloc hook;
allocHook getAllocHook();

//------------------------------------------------------------------------------

typedef void(*freeHook)(void*);

//! Sets the user defined free hook.
void setFreeHook(freeHook hook);

//! Gets the user defined free hook;
freeHook getFreeHook();

//------------------------------------------------------------------------------

typedef void*(*alignedAllocHook)(size_t, size_t alignment);

//! Sets the user defined aligned alloc hook.
void setAlignedAllocHook(alignedAllocHook hook);

//! Gets the user defined aligned alloc hook;
alignedAllocHook getAlignedAllocHook();

//------------------------------------------------------------------------------

typedef void(*alignedFreeHook)(void*);

//! Sets the user defined aligned free hook.
void setAlignedFreeHook(alignedFreeHook hook);

//! Gets the user defined aligned free hook;
alignedFreeHook getAlignedFreeHook();

} // namespace hooks


//------------------------------------------------------------------------------
template<typename T, size_t ALIGNEMENT, typename... TArgs>
GTS_INLINE T* unalignedNew(TArgs&&... args)
{
    T* ptr = (T*)hooks::getAllocHook()(sizeof(T));
    return new (ptr) T(std::forward<TArgs>(args)...);
}

//------------------------------------------------------------------------------
template<typename T>
GTS_INLINE T* unalignedDelete(T* ptr)
{
    if (ptr != nullptr)
    {
        ptr->~T();
        hooks::getFreeHook()(ptr);
    }
}

//------------------------------------------------------------------------------
template<typename T, size_t ALIGNEMENT, typename... TArgs>
GTS_INLINE T* alignedNew(TArgs&&... args)
{
    T* ptr = (T*)hooks::getAlignedAllocHook()(sizeof(T), ALIGNEMENT);
    return new (ptr) T(std::forward<TArgs>(args)...);
}

//------------------------------------------------------------------------------
template<typename T, size_t ALIGNEMENT>
GTS_INLINE T* alignedVectorNew(const size_t n)
{
    T* ptr = (T*)hooks::getAlignedAllocHook()(sizeof(T) * n, ALIGNEMENT);
    for (size_t ii = 0; ii < n; ++ii)
    {
        new (ptr + ii) T();
    }
    return ptr;
}

//------------------------------------------------------------------------------
template<typename T>
GTS_INLINE void alignedDelete(T* ptr)
{
    if (ptr != nullptr)
    {
        ptr->~T();
        hooks::getAlignedFreeHook()(ptr);
    }
}

//------------------------------------------------------------------------------
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
    hooks::getAlignedFreeHook()(ptr);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// VIRTUAL MEMORY:

enum class AllocationType
{
    RESERVE_COMMIT,
    COMMIT,
    RESERVE,
    RESET,
    RESET_UNDO
};

enum class FreeType
{
    DECOMMIT,
    RELEASE
};

namespace hooks {

//------------------------------------------------------------------------------

typedef void*(*osAllocHook)(void* ptr, uint32_t length, AllocationType type);

//! Sets the user defined aligned free hook.
void setOsAllocHook(osAllocHook hook);

//! Gets the user defined aligned free hook;
osAllocHook getOsAllocHook();

//------------------------------------------------------------------------------

typedef bool(*osFreeHook)(void* ptr, uint32_t length, FreeType type);

//! Sets the user defined aligned free hook.
void setOsFreeHook(osFreeHook hook);

//! Gets the user defined aligned free hook;
osFreeHook getOsFreeHook();

} // namespace hooks

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 *
 */
struct Memory
{
    static uint32_t getAllocationGranularity();

    static uint32_t getPageSize();

    static void* osAlloc(void* ptr, uint32_t length, AllocationType type = AllocationType::RESERVE_COMMIT);

    static bool osFree(void* ptr, uint32_t length, FreeType type = FreeType::RELEASE);
};

} // namespace gts
