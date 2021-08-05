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
#include "gts/malloc/GtsMalloc.h"

#include <cstdlib>

#include "gts/containers/parallel/BinnedAllocator.h"

#ifdef GTS_WINDOWS
    #ifdef GTS_MALLOC_SHARED_LIB
        
    #endif
#endif

static thread_local gts::BinnedAllocator tl_binnedAlloc;
gts::MemoryStore g_memoryStore;

namespace gts {
namespace internal {
    
bool verifyMemoryStoreIsFreed()
{
    tl_binnedAlloc.shutdown();
    g_memoryStore.clear();
    return g_memoryStore.empty();
}

} // namespace internal 
} // namespace gts 

namespace {

//------------------------------------------------------------------------------
template<typename T>
size_t strLen(T const& pStr)
{
    size_t len = 0;
    while(pStr[len] != 0)
    {
        ++len;
    }
    ++len;
    return len;
}

//------------------------------------------------------------------------------
bool checkMulOverflow(size_t l, size_t r)
{
    return r > 0 && SIZE_MAX / r > l;
}

} // namespace

using namespace gts;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Common methods

//------------------------------------------------------------------------------
void* gts_malloc(size_t size)
{
    if (!tl_binnedAlloc.isInitialized())
    {
        tl_binnedAlloc.init(&g_memoryStore);
    }
    if(size == 0)
    {
        return nullptr;
    }
    return tl_binnedAlloc.allocate(size);
}

//------------------------------------------------------------------------------
void* gts_calloc(size_t count, size_t size)
{
    if(size == 0 || count == 0 || !checkMulOverflow(count, size))
    {
        return nullptr;
    }

    size_t newSize = count * size;
    void* pResult = gts_malloc(newSize);
    if(pResult)
    {
        return memset(pResult, 0, newSize);
    }
    return pResult;
}

//------------------------------------------------------------------------------
void* gts_realloc(void* ptr, size_t newSize)
{
    if(gts_usable_size(ptr) >= newSize)
    {
        return ptr;
    }

    void* pResult = gts_malloc(newSize);
    if (pResult)
    {
        if(ptr)
        {
            memcpy(pResult, ptr, newSize);
            gts_free(ptr);
        }
    }
    else if(ptr)
    {
        gts_free(ptr);
    }
    return pResult;
}

//------------------------------------------------------------------------------
void gts_free(void* ptr)
{
    if (!tl_binnedAlloc.isInitialized())
    {
        tl_binnedAlloc.init(&g_memoryStore);
    }

    if(ptr)
    {
        tl_binnedAlloc.deallocate(ptr);
    }
}

//------------------------------------------------------------------------------
char* gts_strdup(char const* pStr)
{
    if(pStr == nullptr)
    {
        return nullptr;
    }
    size_t len = strLen(pStr);
    char* pCopy = (char*)gts_malloc(len);
    if(pCopy)
    {
        memcpy(pCopy, pStr, len);
    }
    return pCopy;
}

//------------------------------------------------------------------------------
char* gts_strndup(char const* pStr, size_t size)
{
    if(pStr == nullptr)
    {
        return nullptr;
    }
    size_t len = gts::gtsMin(size, strLen(pStr));
    char* pCopy = (char*)gts_malloc(sizeof(char) * len);
    if(pCopy)
    {
        memcpy(pCopy, pStr, len);
    }
    return pCopy;
}

//------------------------------------------------------------------------------
void* gts_aligned_malloc(size_t size, size_t alignment)
{
    if(!isPow2(alignment))
    {
        GTS_INTERNAL_ASSERT(0);
        return nullptr;
    }

    if (size > SIZE_MAX - alignment + 1)
    {
        GTS_INTERNAL_ASSERT(0);
        return nullptr;
    }

    void* ptr = gts_malloc(size + alignment - 1);
    if (ptr && ((uintptr_t(ptr) & (alignment - 1)) != 0))
    {
        ptr = (void*)alignUpTo(uintptr_t(ptr), alignment);
        SlabHeader* pSlab = MemoryStore::toSlab(ptr);
        PageHeader* pPage = MemoryStore::toPage(pSlab, ptr);
        pPage->hasAlignedBlocks = true;
    }

    GTS_ASSERT((uintptr_t(ptr) & (alignment - 1)) == 0);

    return ptr;
}

//------------------------------------------------------------------------------
size_t gts_usable_size(void* ptr)
{
    if(ptr == nullptr)
    {
        return 0;
    }
    SlabHeader* pSlab = MemoryStore::toSlab(ptr);
    PageHeader* pPage = MemoryStore::toPage(pSlab, ptr);
    if(pPage->hasAlignedBlocks)
    {
        void* pBase     = MemoryStore::toAlignedBlockStart(pPage, ptr);
        uintptr_t delta = uintptr_t(ptr) - uintptr_t(pBase);
        return pPage->blockSize - delta;
    }
    return pPage->blockSize;
}

//------------------------------------------------------------------------------
void* gts_expand(void* ptr, size_t size)
{
    if(size > gts_usable_size(ptr))
    {
        return nullptr;
    }
    return ptr;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Windows

#ifdef GTS_WINDOWS

//------------------------------------------------------------------------------
void* gts_win_recalloc(void* ptr, size_t count, size_t size)
{
    if (size == 0)
    {
        gts_free(ptr);
        return nullptr;
    }

    if(count == 0 || !checkMulOverflow(count, size))
    {
        return nullptr;
    }

    size_t newSize = count * size;
    void* pResult = gts_malloc(newSize);
    if (pResult)
    {
        memset(pResult, 0, newSize);
    }

    if (ptr)
    {
        gts_free(ptr);
    }

    return pResult;
}

//------------------------------------------------------------------------------
wchar_t* gts_win_wcsdup(wchar_t const* pStr)
{
    if(pStr == nullptr)
    {
        return nullptr;
    }

    size_t len = sizeof(wchar_t) * (strLen(pStr) + 1);
    wchar_t* pCopy = (wchar_t*)gts_malloc(len);
    if(pCopy)
    {
        memcpy(pCopy, pStr, len);
    }
    return pCopy;
}

//------------------------------------------------------------------------------
unsigned char * gts_win_mbsdup(unsigned char const* pStr)
{
    if(pStr == nullptr)
    {
        return nullptr;
    }
    size_t len = strLen(pStr);
    unsigned char * pCopy = (unsigned char *)gts_malloc(sizeof(unsigned char) * len);
    memcpy(pCopy, pStr, len);
    return pCopy;
}

//------------------------------------------------------------------------------
errno_t gts_win_dupenv_s(char** ppBuf, size_t* pNumElements, char const* pVarName)
{
    if ( ppBuf == nullptr || pVarName == nullptr )
    {
        return EINVAL;
    }

    if (pNumElements != nullptr)
    {
        *pNumElements = 0;
    }

    #pragma warning(suppress:4996)
    char* ptr = getenv(pVarName);
    if (ptr == nullptr)
    {
        *ppBuf = nullptr;
    }
    else
    {
        *ppBuf = gts_strdup(ptr);
        if (*ppBuf==nullptr)
        {
            return ENOMEM;
        }
        if (pNumElements != nullptr)
        {
            *pNumElements = strLen(ptr);
        }
    }
    return 0;
}

//------------------------------------------------------------------------------
errno_t gts_win_wdupenv_s(wchar_t** ppBuf, size_t* pNumElements, wchar_t const* pVarName)
{
    if ( ppBuf == nullptr || pVarName == nullptr )
    {
        return EINVAL;
    }

    if (pNumElements != nullptr)
    {
        *pNumElements = 0;
    }

    #pragma warning(suppress:4996)
    wchar_t* ptr = _wgetenv(pVarName);
    if (ptr == nullptr)
    {
        *ppBuf = nullptr;
    }
    else
    {
        *ppBuf = gts_win_wcsdup(ptr);
        if (*ppBuf==nullptr)
        {
            return ENOMEM;
        }
        if (pNumElements != nullptr)
        {
            *pNumElements = strLen(ptr);
        }
    }
    return 0;
}

//------------------------------------------------------------------------------
void* gts_win_aligned_realloc(void* ptr, size_t newsize, size_t alignment)
{
    void* pResult = gts_aligned_malloc(newsize, alignment);
    if (pResult && ptr)
    {
        memcpy(pResult, ptr, newsize);
        gts_free(ptr);
    }
    return pResult;
}

//------------------------------------------------------------------------------
void* gts_win_aligned_recalloc(void* ptr, size_t count, size_t size, size_t alignment)
{
    if (size == 0)
    {
        gts_free(ptr);
        return nullptr;
    }

    if(count == 0 || !checkMulOverflow(count, size))
    {
        return nullptr;
    }

    size_t newSize = count * size;
    void* pResult = gts_aligned_malloc(newSize, alignment);
    if (pResult)
    {
        memset(pResult, 0, newSize);
    }

    if (ptr)
    {
        gts_free(ptr);
    }

    return pResult;
}

//------------------------------------------------------------------------------
void* gts_win_aligned_offset_malloc(size_t size, size_t alignment, size_t offset)
{
    if (size == 0 || offset > size)
    {
        return nullptr;
    }

    if(offset == 0 && ((size & (alignment - 1)) == 0))
    {
        void* pResult = gts_malloc(size);
        if(!pResult)
        {
            return nullptr;
        }
        return (void*)((uint8_t*)pResult + offset);
    }

    void* pNew = gts_malloc(size + alignment - 1);
    if(!pNew)
    {
        return nullptr;
    }

    size_t shift = alignment - ((uintptr_t(pNew) + offset) & (alignment - 1));
    void* pAligned = (shift == alignment ? pNew : (void*)((uintptr_t)pNew + shift));
    if (pAligned != pNew)
    {
        SlabHeader* pSlab = MemoryStore::toSlab(pAligned);
        PageHeader* pPage = MemoryStore::toPage(pSlab, pAligned);
        pPage->hasAlignedBlocks = true;
    }

    return pAligned;
}

//------------------------------------------------------------------------------
void* gts_win_aligned_offset_realloc(void* ptr, size_t newsize, size_t alignment, size_t offset)
{
    void* pResult = gts_win_aligned_offset_malloc(newsize, alignment, offset);
    if(pResult)
    {
        memcpy(pResult, ptr, gts_usable_size(ptr));
        gts_free(ptr);
    }
    return pResult;
}

//------------------------------------------------------------------------------
void* gts_win_aligned_offset_recalloc(void* ptr, size_t count, size_t size, size_t alignment, size_t offset)
{
    if (size == 0)
    {
        gts_free(ptr);
        return nullptr;
    }

    if(count == 0 || !checkMulOverflow(count, size))
    {
        return nullptr;
    }

    size_t newSize = count * size;
    void* pResult = gts_win_aligned_offset_malloc(newSize, alignment, offset);
    if (pResult)
    {
        memset(pResult, 0, newSize);
    }

    if (ptr)
    {
        gts_free(ptr);
    }

    return pResult;
}

#elif GTS_POSIX

//------------------------------------------------------------------------------
void* gts_posix_reallocf(void* ptr, size_t size)
{
    return gts_realloc(ptr, size);
}

//------------------------------------------------------------------------------
void* gts_posix_valloc(size_t size)
{
    return gts_aligned_malloc(size, GTS_GET_OS_PAGE_SIZE());
}

//------------------------------------------------------------------------------
void* gts_posix_pvalloc(size_t size)
{
    return gts_aligned_malloc(alignUpTo(size, GTS_GET_OS_PAGE_SIZE()), GTS_GET_OS_PAGE_SIZE());
}

//------------------------------------------------------------------------------
void* gts_posix_reallocarray(void* ptr, size_t count, size_t size)
{
    if (size == 0)
    {
        gts_free(ptr);
        return nullptr;
    }

    if(count == 0 || !checkMulOverflow(count, size))
    {
        return nullptr;
    }

    size_t newSize = count * size;
    void* pResult = gts_malloc(newSize);
    if (pResult)
    {
        memset(pResult, 0, newSize);
    }

    if (ptr)
    {
        gts_free(ptr);
    }

    return pResult;
}

//------------------------------------------------------------------------------
int gts_posix_memalign(void **memptr, size_t alignment, size_t size)
{
    if(!isPow2(alignment))
    {
        return EINVAL;
    }

    *memptr = gts_aligned_malloc(size, alignment);
    if(!*memptr)
    {
        return ENOMEM;
    }
    return 0;
}

//------------------------------------------------------------------------------
void* gts_posix_aligned_alloc(size_t alignment, size_t size)
{
    return gts_aligned_malloc(size, alignment);
}

#endif

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// C++

//------------------------------------------------------------------------------
static bool tryNewHandler(bool noThrow)
{
    std::new_handler h = std::get_new_handler();
    if (h == nullptr)
    {
        if (!noThrow)
        {
        #if __EXCEPTIONS
            throw std::bad_alloc();
        #endif
        }
        return false;
    }
    else
    {
        h();
        return true;
    }
}

//------------------------------------------------------------------------------
void* gts_new(size_t size)
{
    void* ptr = gts_malloc(size);
    while (ptr == nullptr && tryNewHandler(false))
    {
        ptr = gts_malloc(size);
    }
    return ptr;
}

//------------------------------------------------------------------------------
void* gts_new_nothrow(size_t size)
{
    void* ptr = gts_malloc(size);
    while(ptr == nullptr && tryNewHandler(true))
    {
        ptr = gts_malloc(size);
    }
    return ptr;
}

//------------------------------------------------------------------------------
void gts_free_size(void* ptr, size_t size)
{
    GTS_UNREFERENCED_PARAM(size);
    GTS_ASSERT(ptr == nullptr || size <= gts_usable_size(ptr));
    gts_free(ptr);
}

//------------------------------------------------------------------------------
void* gts_new_aligned(size_t size, size_t alignment)
{
    void* ptr = gts_aligned_malloc(size, alignment);
    while (ptr == nullptr && tryNewHandler(false))
    {
        ptr = gts_aligned_malloc(size, alignment);
    }
    return ptr;
}

//------------------------------------------------------------------------------
void* gts_new_aligned_nothrow(size_t size, size_t alignment)
{
    void* ptr = gts_aligned_malloc(size, alignment);
    while (ptr == nullptr && tryNewHandler(true))
    {
        ptr = gts_aligned_malloc(size, alignment);
    }
    return ptr;
}

//------------------------------------------------------------------------------
void  gts_free_aligned(void* ptr, size_t alignment)
{
    if(!isPow2(alignment))
    {
        GTS_ASSERT(0);
    }

    GTS_UNREFERENCED_PARAM(alignment);
    GTS_ASSERT(((uintptr_t)ptr & (alignment - 1)) == 0);
    gts_free(ptr);
}

//------------------------------------------------------------------------------
void  gts_free_size_aligned(void* ptr, size_t size, size_t alignment)
{
    if(!isPow2(alignment))
    {
        GTS_ASSERT(0);
    }

    GTS_UNREFERENCED_PARAM(alignment);
    GTS_ASSERT(((uintptr_t)ptr & (alignment - 1)) == 0);
    gts_free_size(ptr, size);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Library functions:

//------------------------------------------------------------------------------
void attachProcess()
{
    // Do start-up stuff here
}

#if defined(GTS_WINDOWS) && defined(GTS_MALLOC_SHARED_LIB)
#include <Windows.h>

//------------------------------------------------------------------------------
GTS_MALLOC_EXPORT BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID)
{
    switch(reason)
    {
    case DLL_PROCESS_ATTACH:
        attachProcess();
        break;
    }
    
    return TRUE;
}

#else

// Use C++ static initialization to detect process start
static bool gtsMallocInit(void)
{
    attachProcess();
    return g_memoryStore.init();
}

static bool g_gtsMallocIsInitialized = gtsMallocInit();

#endif
