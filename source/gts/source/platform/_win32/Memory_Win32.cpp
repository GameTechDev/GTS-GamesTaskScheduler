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
#include "gts/platform/Memory.h"
#include "gts/platform/Assert.h"

#include "Windows.h"

namespace gts {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// HEAP MEMORY:

namespace hooks {

static allocHook g_allocHook = [](size_t size)->void*{ return malloc(size); };

//------------------------------------------------------------------------------
void setAllocHook(allocHook hook)
{
    g_allocHook = hook;
}

//------------------------------------------------------------------------------
allocHook getAllocHook()
{
    return g_allocHook;
}

static freeHook g_freeHook = [](void* ptr){ free(ptr); };

//------------------------------------------------------------------------------
void setFreeHook(freeHook hook)
{
    g_freeHook = hook;
}

//------------------------------------------------------------------------------
freeHook getFreeHook()
{
    return g_freeHook;
}

alignedAllocHook g_alignedAllocHook = [](size_t size, size_t alignment)->void* { return GTS_ALIGNED_MALLOC(size, alignment); };

//------------------------------------------------------------------------------
void setAlignedAllocHook(alignedAllocHook hook)
{
    g_alignedAllocHook = hook;
}

//------------------------------------------------------------------------------
alignedAllocHook getAlignedAllocHook()
{
    return g_alignedAllocHook;
}

alignedFreeHook g_alignedFreeHook = [](void* ptr){ GTS_ALIGNED_FREE(ptr); };

//! Sets the user defined aligned free hook.
void setAlignedFreeHook(alignedFreeHook hook)
{
    g_alignedFreeHook = hook;
}

//! Gets the user defined aligned free hook;
alignedFreeHook getAlignedFreeHook()
{
    return g_alignedFreeHook;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// VIRTUAL MEMORY:

//------------------------------------------------------------------------------

osAllocHook g_osAllocHook = [](void* ptr, uint32_t length, AllocationType type)->void*
{
    switch (type)
    {
    case AllocationType::RESERVE_COMMIT:
        return VirtualAlloc(ptr, length, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

    case AllocationType::COMMIT:
        return VirtualAlloc(ptr, length, MEM_COMMIT, PAGE_READWRITE);

    case AllocationType::RESERVE:
        return VirtualAlloc(ptr, length, MEM_RESERVE, PAGE_READWRITE);

    case AllocationType::RESET:
        return VirtualAlloc(ptr, length, MEM_RESET, PAGE_READWRITE);

    case AllocationType::RESET_UNDO:
        return VirtualAlloc(ptr, length, MEM_RESET_UNDO, PAGE_READWRITE);

    default:
        return nullptr;
    }
};

//------------------------------------------------------------------------------
void setOsAllocHook(osAllocHook hook)
{
    g_osAllocHook = hook;
}

//------------------------------------------------------------------------------
osAllocHook getOsAllocHook()
{
    return g_osAllocHook;
}

osFreeHook g_osFreeHook = [](void* ptr, uint32_t length, FreeType type)->bool
{
    switch (type)
    {
    case FreeType::DECOMMIT:
        return VirtualFree(ptr, length, MEM_DECOMMIT) == TRUE;

    case FreeType::RELEASE:
        return VirtualFree(ptr, 0, MEM_RELEASE) == TRUE;

    default:
        return false;
    }
};

//------------------------------------------------------------------------------
void setOsFreeHook(osFreeHook hook)
{
    g_osFreeHook = hook;
}

//------------------------------------------------------------------------------
osFreeHook getOsFreeHook()
{
    return g_osFreeHook;
}

} // namespace hooks

//------------------------------------------------------------------------------
uint32_t Memory::getAllocationGranularity()
{
    SYSTEM_INFO info = {};
    GetSystemInfo(&info);
    return (uint32_t)info.dwAllocationGranularity;
}

//------------------------------------------------------------------------------
uint32_t Memory::getPageSize()
{
    SYSTEM_INFO info = {};
    GetSystemInfo(&info);
    return (uint32_t)info.dwPageSize;
}

//------------------------------------------------------------------------------
void* Memory::osAlloc(void* ptr, uint32_t length, AllocationType type)
{
    return hooks::getOsAllocHook()(ptr, length, type);
}

//------------------------------------------------------------------------------
bool Memory::osFree(void* ptr, uint32_t length, FreeType type)
{
    GTS_ASSERT(ptr != nullptr);

    return hooks::getOsFreeHook()(ptr, length, type);
}

} // namespace gts
