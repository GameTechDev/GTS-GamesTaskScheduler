#/*******************************************************************************
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

#ifndef GTS_HAS_CUSTOM_OS_MEMORY_WRAPPERS
#ifdef GTS_WINDOWS

#include "gts/platform/Assert.h"
#include "gts/platform/Utils.h"

#include "Windows.h"

namespace gts {
namespace internal {

static size_t g_allocGranularity = 0;
static size_t g_pageSize = 0;
static size_t g_largePageSize = 0;
static bool g_hasLargePages = false;

//------------------------------------------------------------------------------
size_t Memory::getAllocationGranularity()
{
    if (g_allocGranularity == 0)
    {
        SYSTEM_INFO info = {};
        GetSystemInfo(&info);
        g_allocGranularity = info.dwAllocationGranularity;
    }
    return g_allocGranularity;
}

//------------------------------------------------------------------------------
size_t Memory::getPageSize()
{
    if (g_pageSize == 0)
    {
        SYSTEM_INFO info = {};
        GetSystemInfo(&info);
        g_pageSize = info.dwPageSize;
    }
    return g_pageSize;
}

//------------------------------------------------------------------------------
size_t Memory::getLargePageSize()
{
    if (g_largePageSize == 0)
    {
        g_largePageSize = GetLargePageMinimum();
    }
    return g_largePageSize;
}

//------------------------------------------------------------------------------
bool Memory::enableLargePageSupport()
{
    if (g_hasLargePages)
    {
        return true;
    }

    // https://docs.microsoft.com/en-us/windows/win32/memory/creating-a-file-mapping-using-large-pages?redirectedfrom=MSDN
    HANDLE           hToken;
    TOKEN_PRIVILEGES tp;
    BOOL             result;

    // open process token
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
    {
        return false;
    }

    // get the luid
    if (!LookupPrivilegeValue(NULL, TEXT("SeLockMemoryPrivilege"), &tp.Privileges[0].Luid))
    {
        return false;
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    // enable privilege
    result = AdjustTokenPrivileges(hToken, FALSE, &tp, 0, (PTOKEN_PRIVILEGES)NULL, 0);

    // It is possible for AdjustTokenPrivileges to return TRUE and still not succeed.
    // So always check for the last error value.
    result = (result && (GetLastError() != ERROR_SUCCESS));

    g_hasLargePages = result;

    CloseHandle(hToken);
    return result;
}

//------------------------------------------------------------------------------
void* Memory::osHeapAlloc(size_t size)
{
    HANDLE hHeap = GetProcessHeap();
    return HeapAlloc(hHeap, 0, size);
}

//------------------------------------------------------------------------------
bool Memory::osHeapFree(void* ptr, size_t)
{
    HANDLE hHeap = GetProcessHeap();
    return HeapFree(hHeap, 0, ptr) != FALSE;
}

//------------------------------------------------------------------------------
void* Memory::osVirtualAlloc(void* ptr, size_t size, bool commit, bool largePage)
{
    DWORD flags = MEM_RESERVE;
    if (commit)
    {
        flags |= MEM_COMMIT;
    }
    if (largePage)
    {
        GTS_ASSERT(commit && "Large pages must commit.");

        flags |= MEM_COMMIT;
        flags |= MEM_LARGE_PAGES;
    }

    void* pOut =  VirtualAlloc(ptr, size, flags, PAGE_READWRITE);
    if (pOut == nullptr)
    {
        DWORD lastError = GetLastError();
        if (lastError == ERROR_NOT_ENOUGH_MEMORY || lastError == ERROR_OUTOFMEMORY)
        {
            pOut = (void*)UINTPTR_MAX;
        }
    }

    return pOut;
}

//------------------------------------------------------------------------------
void* Memory::osVirtualCommit(void* ptr, size_t size)
{
    return VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE);
}

//------------------------------------------------------------------------------
bool Memory::osVirtualDecommit(void* ptr, size_t size)
{
    GTS_ASSERT(ptr != nullptr);
    BOOL result = VirtualFree(ptr, size, MEM_DECOMMIT);
    GTS_ASSERT(result != FALSE);
    return result != FALSE;
}

//------------------------------------------------------------------------------
bool Memory::osVirtualFree(void* ptr, size_t)
{
    GTS_ASSERT(ptr != nullptr);
    BOOL result = VirtualFree(ptr, size_t(0), MEM_RELEASE);
    GTS_ASSERT(result != FALSE);
    return result != FALSE;
}

} // namespace internal
} // namespace gts

#endif
#endif
