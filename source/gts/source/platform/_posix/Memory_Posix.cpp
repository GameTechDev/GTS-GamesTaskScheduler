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

#ifdef GTS_POSIX
#ifndef GTS_HAS_CUSTOM_OS_MEMORY_WRAPPERS

#include "gts/platform/Assert.h"
#include "gts/platform/Utils.h"

#include <fstream>
#include <string>
#include <iostream>
#include <ios>
#include <regex>

#include <sys/mman.h>
#include <unistd.h>

#if !defined(MAP_ANONYMOUS)
  #define MAP_ANONYMOUS  MAP_ANON
#endif

#if !defined(MAP_NORESERVE)
  #define MAP_NORESERVE  0
#endif

namespace gts {
namespace internal {

static size_t g_largePageSize = 0;

//------------------------------------------------------------------------------
size_t Memory::getAllocationGranularity()
{
    size_t granularity = sysconf(_SC_PAGESIZE);
    return granularity;
}

//------------------------------------------------------------------------------
size_t Memory::getPageSize()
{
    size_t size = sysconf(_SC_PAGESIZE);
    return size;
}

//------------------------------------------------------------------------------
size_t Memory::getLargePageSize()
{
    if (g_largePageSize == 0)
    {
        const char* seachStr = "Hugepagesize:";
        std::string line;
        std::ifstream fileInput;
        fileInput.open("/proc/meminfo", std::ios_base::in);

        while(std::getline(fileInput, line))
        {
            if (line.find(seachStr, 0) != std::string::npos)
            {
                std::regex exp("Hugepagesize: *(\\w+) kB");
                std::smatch sm;
                std::regex_match ( line, sm, exp, std::regex_constants::match_default );
                g_largePageSize = std::stoi(sm[1]);
                break;
            }
        }
    }

    return g_largePageSize;
}

//------------------------------------------------------------------------------
bool Memory::enableLargePageSupport()
{
    return getLargePageSize() != 0;
}

//------------------------------------------------------------------------------
void* Memory::osHeapAlloc(size_t size)
{
    void* pOut = osVirtualAlloc(nullptr, alignUpTo(size, getPageSize()), true, false);
    if(pOut == (void*)UINTPTR_MAX)
    {
        return nullptr;
    }
    return pOut;
}

//------------------------------------------------------------------------------
bool Memory::osHeapFree(void* ptr, size_t size)
{
    return osVirtualFree(ptr, alignUpTo(size, getPageSize()));
}

//------------------------------------------------------------------------------
void* Memory::osVirtualAlloc(void* pHint, size_t size, bool commit, bool largePage)
{
    int protectFlags = 0;
    int flags        = 0;
    int fd           = -1;

    if (largePage && getLargePageSize() != 0)
    {
        GTS_ASSERT(size % g_largePageSize == 0 && "Large allocations be multiples of the large page size.");
        GTS_ASSERT(commit && "Large allocations must commit.");

        protectFlags = PROT_WRITE | PROT_READ;
        flags        = MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE;

    #ifdef MAP_ALIGNED_SUPER
        flags |= MAP_ALIGNED_SUPER;
    #endif

    #ifdef MAP_HUGETLB
        flags |= MAP_HUGETLB;
    #endif

    #ifdef MAP_HUGE_1GB
        if(size % g_largePageSize == 0)
        {
            flags |= MAP_HUGE_1GB;
        }
        else
    #endif
        {
        #ifdef MAP_HUGE_2MB
            llags |= MAP_HUGE_2MB;
        #endif
        }

        #ifdef VM_FLAGS_SUPERPAGE_SIZE_2MB
            fd |= VM_FLAGS_SUPERPAGE_SIZE_2MB;
        #endif
    }
    else
    {
        GTS_ASSERT(size % getPageSize() == 0 && "Allocation must be multiples of the page size.");

        protectFlags = (commit ? (PROT_WRITE | PROT_READ) : PROT_NONE);
        flags        = MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE;
    }

    void* pOut = mmap(pHint, size, protectFlags, flags, fd, 0);
    if(pOut == MAP_FAILED)
    {
        GTS_INTERNAL_ASSERT(0);
        if(errno == ENOMEM || errno == EOVERFLOW)
        {
            pOut = (void*)UINTPTR_MAX;
        }
        else
        {
            pOut = nullptr;
        }
    }

    return pOut;
}

//------------------------------------------------------------------------------
void* Memory::osVirtualCommit(void* ptr, size_t size)
{
    GTS_ASSERT(isAligned(ptr, getPageSize()) && "Commit pointer must be multiples of the page size.");
    GTS_ASSERT(size % getPageSize() == 0 && "Commit size must be multiples of the page size.");

    int result = mprotect(ptr, size, (PROT_READ | PROT_WRITE));
    if (result != 0)
    {
        GTS_INTERNAL_ASSERT(0);
        if(errno == ENOMEM || errno == EOVERFLOW)
        {
            return (void*)UINTPTR_MAX;
        }
        else
        {
            return nullptr;
        }
    }
    return ptr;
}

//------------------------------------------------------------------------------
bool Memory::osVirtualDecommit(void* ptr, size_t size)
{
    GTS_ASSERT(isAligned(ptr, getPageSize()) && "Commit pointer must be multiples of the page size.");
    GTS_ASSERT(size % getPageSize() == 0 && "Commit size must be multiples of the page size.");
    int result = 0;

#if defined(MAP_FIXED)
    void* p = mmap(ptr, size, PROT_NONE, (MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE), -1, 0);
    if(p != ptr) result = -1;
#else
    int result = mprotect(ptr, size, PROT_NONE);
#endif

    GTS_INTERNAL_ASSERT(result == 0);
    return result == 0;
}

//------------------------------------------------------------------------------
bool Memory::osVirtualFree(void* ptr, size_t size)
{
    int result = munmap(ptr, size);
    if(result != 0)
    {
        printf("%d", errno);
    }
    GTS_INTERNAL_ASSERT(result == 0);
    return result == 0;
}

} // namespace internal
} // namespace gts

#endif
#endif
