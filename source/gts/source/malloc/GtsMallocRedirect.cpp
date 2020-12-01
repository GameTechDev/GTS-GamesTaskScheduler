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
#if GTS_MALLOC_REDIRECT

#include <cstdio>

#include "gts/malloc/GtsMalloc.h"
#include "gts/malloc/GtsMallocRedirect.h"

// We have to override new and delete since Detours calls delete in
// DetourTransactionCommit, which tries to free non-gts_malloc memory
// with gts_malloc in DLL_PROCESS_DETACH.
#include "gts/malloc/GtsMallocCppOverride.h"

#if !defined(GTS_WINDOWS) && !defined(GTS_MALLOC_SHARED_LIB)
#error "Redirection DLL is only used on Windows platforms."
#endif

#include <Windows.h>
#include <mbstring.h>
#include <malloc.h>
#include "detours.h" // Must be included after Windows.h!

void*           (__cdecl * std_malloc)(size_t size)                                                                             = malloc;
void*           (__cdecl * std_calloc)(size_t count, size_t size)                                                               = calloc;
void*           (__cdecl * std_realloc)(void* p, size_t newsize)                                                                = realloc;
void            (__cdecl * std_free)(void* p)                                                                                   = free;

#pragma warning(push)
#pragma warning(disable : 4996)
char*           (__cdecl * std_strdup)(char const* pStr)                                                                        = strdup;
#pragma warning(pop)

void*           (__cdecl * std_expand)(void* ptr, size_t size)                                                                  = _expand;
size_t          (__cdecl * std_usable_size)(void* ptr)                                                                          = _msize;
void*           (__cdecl * std_recalloc)(void* ptr, size_t count, size_t size)                                                  = _recalloc;
char*           (__cdecl * std__strdup)(char const* pStr)                                                                       = _strdup;
wchar_t*        (__cdecl * std_wcsdup)(wchar_t const* pStr)                                                                     = _wcsdup;
unsigned char*  (__cdecl * std_mbsdup)(unsigned char const* pStr)                                                               = _mbsdup;
errno_t         (__cdecl * std_dupenv_s)(char** ppBuf, size_t* pNumElements, char const* pVarName)                              = _dupenv_s;
errno_t         (__cdecl * std_wdupenv_s)(wchar_t** ppBuf, size_t* pNumElements, wchar_t const* pVarName)                       = _wdupenv_s;
void*           (__cdecl * std_aligned_malloc)(size_t size, size_t alignment)                                                   = _aligned_malloc;
void*           (__cdecl * std_aligned_realloc)(void* ptr, size_t newsize, size_t alignment)                                    = _aligned_realloc;
void*           (__cdecl * std_aligned_recalloc)(void* ptr, size_t count, size_t size, size_t alignment)                        = _aligned_recalloc;
void*           (__cdecl * std_aligned_offset_malloc)(size_t size, size_t alignment, size_t offset)                             = _aligned_offset_malloc;
void*           (__cdecl * std_aligned_offset_realloc)(void* ptr, size_t size, size_t alignment, size_t offset)                 = _aligned_offset_realloc;
void*           (__cdecl * std_aligned_offset_recalloc)(void* ptr, size_t count, size_t size, size_t alignment, size_t offset)  = _aligned_offset_recalloc;

//------------------------------------------------------------------------------
void gts_malloc_redirect()
{
}

//------------------------------------------------------------------------------
GTS_SHARED_LIB_EXPORT BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID)
{
    if (DetourIsHelperProcess())
    {
        printf("DetourIsHelperProcess\n");
        return TRUE;
    }

    switch(reason)
    {
    case DLL_PROCESS_ATTACH:

        printf("Attach\n");

        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());

        DetourAttach(&(PVOID&)std_malloc, gts_malloc);
        DetourAttach(&(PVOID&)std_calloc, gts_calloc);
        DetourAttach(&(PVOID&)std_realloc, gts_realloc);
        DetourAttach(&(PVOID&)std_free, gts_free);
        DetourAttach(&(PVOID&)std_strdup, gts_strdup);
        DetourAttach(&(PVOID&)std_expand, gts_expand);
        DetourAttach(&(PVOID&)std_usable_size, gts_usable_size);
        DetourAttach(&(PVOID&)std_recalloc, gts_win_recalloc);
        DetourAttach(&(PVOID&)std__strdup, gts_strdup);
        DetourAttach(&(PVOID&)std_wcsdup, gts_win_wcsdup);
        DetourAttach(&(PVOID&)std_mbsdup, gts_win_mbsdup);
        DetourAttach(&(PVOID&)std_dupenv_s, gts_win_dupenv_s);
        DetourAttach(&(PVOID&)std_wdupenv_s, gts_win_wdupenv_s);
        DetourAttach(&(PVOID&)std_aligned_malloc, gts_aligned_malloc);
        DetourAttach(&(PVOID&)std_aligned_realloc, gts_win_aligned_realloc);
        DetourAttach(&(PVOID&)std_aligned_recalloc, gts_win_aligned_recalloc);
        DetourAttach(&(PVOID&)std_aligned_offset_malloc, gts_win_aligned_offset_malloc);
        DetourAttach(&(PVOID&)std_aligned_offset_realloc, gts_win_aligned_offset_realloc);
        DetourAttach(&(PVOID&)std_aligned_offset_recalloc, gts_win_aligned_offset_recalloc);

        DetourTransactionCommit();

        break;

    case DLL_PROCESS_DETACH:

        printf("Detach\n");

        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());

        DetourDetach(&(PVOID&)std_malloc, gts_malloc);
        DetourDetach(&(PVOID&)std_calloc, gts_calloc);
        DetourDetach(&(PVOID&)std_realloc, gts_realloc);
        DetourDetach(&(PVOID&)std_free, gts_free);
        DetourDetach(&(PVOID&)std_strdup, gts_strdup);
        DetourDetach(&(PVOID&)std_expand, gts_expand);
        DetourDetach(&(PVOID&)std_usable_size, gts_usable_size);
        DetourDetach(&(PVOID&)std_recalloc, gts_win_recalloc);
        DetourDetach(&(PVOID&)std__strdup, gts_strdup);
        DetourDetach(&(PVOID&)std_wcsdup, gts_win_wcsdup);
        DetourDetach(&(PVOID&)std_mbsdup, gts_win_mbsdup);
        DetourDetach(&(PVOID&)std_dupenv_s, gts_win_dupenv_s);
        DetourDetach(&(PVOID&)std_wdupenv_s, gts_win_wdupenv_s);
        DetourDetach(&(PVOID&)std_aligned_malloc, gts_aligned_malloc);
        DetourDetach(&(PVOID&)std_aligned_realloc, gts_win_aligned_realloc);
        DetourDetach(&(PVOID&)std_aligned_recalloc, gts_win_aligned_recalloc);
        DetourDetach(&(PVOID&)std_aligned_offset_malloc, gts_win_aligned_offset_malloc);
        DetourDetach(&(PVOID&)std_aligned_offset_realloc, gts_win_aligned_offset_realloc);
        DetourDetach(&(PVOID&)std_aligned_offset_recalloc, gts_win_aligned_offset_recalloc);

        DetourTransactionCommit();

        break;
    }
   
    return TRUE;
}

#endif
