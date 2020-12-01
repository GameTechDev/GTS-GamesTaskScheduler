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
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdlib>
#include <mbstring.h>

#include "gts/containers/parallel/BinnedAllocator.h"
#include "gts/malloc/GtsMalloc.h"

using namespace gts;

namespace testing {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Common methods

//------------------------------------------------------------------------------
TEST(GtsMalloc, gts_malloc)
{
    void* ptr = gts_malloc(4);
    gts_free(ptr);
}

//------------------------------------------------------------------------------
TEST(GtsMalloc, gts_calloc)
{
    void* ptr = gts_calloc(4, 4);
    uint8_t* p = (uint8_t*)ptr;
    for (size_t ii = 0; ii < 4 * 4; ++ii)
    {
        ASSERT_EQ(p[ii], 0);
    }
    gts_free(ptr);
}

//------------------------------------------------------------------------------
TEST(GtsMalloc, gts_realloc)
{
    void* ptr = gts_malloc(4);
    uint8_t* p = (uint8_t*)ptr;
    for (size_t ii = 0; ii < 4; ++ii)
    {
        p[ii] = (uint8_t)ii;
    }

    ptr = gts_realloc(ptr, 8);
    p = (uint8_t*)ptr;
    for (size_t ii = 0; ii < 4; ++ii)
    {
        ASSERT_EQ(p[ii], (uint8_t)ii);
    }

    gts_free(ptr);
}

//------------------------------------------------------------------------------
TEST(GtsMalloc, gts_strdup)
{
    const char* pStr = "Test String";
    char* pDup = gts_strdup(pStr);
    ASSERT_STREQ(pStr, pDup);
    gts_free(pDup);
}

//------------------------------------------------------------------------------
TEST(GtsMalloc, gts_usable_size)
{
    void* ptr = gts_malloc(4);
    ASSERT_EQ(gts_usable_size(ptr), gts::GTS_MALLOC_ALIGNEMNT);
    gts_free(ptr);
}

//------------------------------------------------------------------------------
TEST(GtsMalloc, gts_expand)
{
    void* ptr = gts_malloc(4);

    void* pExp = gts_expand(ptr, gts::GTS_MALLOC_ALIGNEMNT);
    ASSERT_EQ(ptr, pExp);

    void* pExpBad = gts_expand(pExp, gts::GTS_MALLOC_ALIGNEMNT + 1);
    ASSERT_EQ(pExpBad, nullptr);

    gts_free(pExp);
}

//------------------------------------------------------------------------------
TEST(GtsMalloc, gts_aligned_malloc)
{

#ifdef GTS_USE_INTERNAL_ASSERTS
    EXPECT_DEATH(gts_aligned_malloc(4, 7), "");
#else
    ASSERT_EQ(gts_aligned_malloc(4, 7), nullptr);
#endif

    std::vector<void*> blocks;
    for(uint32_t ii = 0; ii < 100; ++ii)
    {
        void* ptr = gts_new_aligned(4, gts::GTS_MALLOC_ALIGNEMNT);
        ASSERT_EQ(uintptr_t(ptr) & (gts::GTS_MALLOC_ALIGNEMNT * 2 - 1), uintptr_t(0));
        blocks.push_back(ptr);
    }

    for(uint32_t ii = 0; ii < 100; ++ii)
    {
        gts_free(blocks[ii]);
    }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// C++

//------------------------------------------------------------------------------
TEST(GtsMalloc, gts_new)
{
#ifdef GTS_USE_INTERNAL_ASSERTS
    EXPECT_DEATH(gts_new(SIZE_MAX), "");
#elif __EXCEPTIONS
    EXPECT_THROW(gts_new(SIZE_MAX), std::bad_alloc);
#endif

    void* ptr = gts_new(4);
    gts_free(ptr);
}

//------------------------------------------------------------------------------
TEST(GtsMalloc, gts_new_nothrow)
{
#ifdef GTS_USE_INTERNAL_ASSERTS
    EXPECT_DEATH(gts_new_nothrow(SIZE_MAX), "");
#else
    ASSERT_EQ(gts_new_nothrow(SIZE_MAX), nullptr);
#endif

    void* ptr = gts_new_nothrow(4);
    gts_free(ptr);
}

//------------------------------------------------------------------------------
TEST(GtsMalloc, gts_free_size)
{
    EXPECT_DEATH({
        void* ptr = gts_new(gts::GTS_MALLOC_ALIGNEMNT - 1);
        gts_free_size(ptr, gts::GTS_MALLOC_ALIGNEMNT + 1);
    }, "");

    void* ptr = gts_new(gts::GTS_MALLOC_ALIGNEMNT - 1);
    gts_free_size(ptr, gts::GTS_MALLOC_ALIGNEMNT);
}

//------------------------------------------------------------------------------
TEST(GtsMalloc, gts_new_aligned)
{
#ifdef GTS_USE_INTERNAL_ASSERTS
    EXPECT_DEATH(gts_new_aligned(SIZE_MAX, 8), "");
    EXPECT_DEATH(gts_new_aligned(4, 7), "");
#elif __EXCEPTIONS
    EXPECT_THROW(gts_new_aligned(SIZE_MAX, 8), std::bad_alloc);
    EXPECT_THROW(gts_new_aligned(4, 7), std::bad_alloc);
#endif

    void* ptr = gts_new_aligned(4, 8);
    gts_free(ptr);
}

//------------------------------------------------------------------------------
TEST(GtsMalloc, gts_new_aligned_nothrow)
{
#ifdef GTS_USE_INTERNAL_ASSERTS
    EXPECT_DEATH(gts_new_aligned_nothrow(SIZE_MAX, 8), "");
    EXPECT_DEATH(gts_new_aligned_nothrow(4, 7), "");
#else
    ASSERT_EQ(gts_new_aligned_nothrow(SIZE_MAX, 8), nullptr);
    ASSERT_EQ(gts_new_aligned_nothrow(4, 7), nullptr);
#endif

    void* ptr = gts_new_aligned_nothrow(4, 8);
    gts_free(ptr);
}

//------------------------------------------------------------------------------
TEST(GtsMalloc, gts_free_aligned)
{
    void* ptr = gts_new_aligned(4, gts::GTS_MALLOC_ALIGNEMNT);

    EXPECT_DEATH({
        gts_free_aligned(ptr, gts::GTS_MALLOC_ALIGNEMNT + 1);
    }, "");

    gts_free_aligned(ptr, gts::GTS_MALLOC_ALIGNEMNT);
}

//------------------------------------------------------------------------------
TEST(GtsMalloc, gts_free_size_aligned)
{
    void* ptr = gts_new_aligned(gts::GTS_MALLOC_ALIGNEMNT - 1, gts::GTS_MALLOC_ALIGNEMNT);

    EXPECT_DEATH({
        gts_free_size_aligned(ptr, gts::GTS_MALLOC_ALIGNEMNT * 2, gts::GTS_MALLOC_ALIGNEMNT + 1);
    }, "");

    EXPECT_DEATH({
        gts_free_size_aligned(ptr, gts::GTS_MALLOC_ALIGNEMNT * 2 + 1, gts::GTS_MALLOC_ALIGNEMNT);
        }, "");

    gts_free_size_aligned(ptr, gts::GTS_MALLOC_ALIGNEMNT * 2, gts::GTS_MALLOC_ALIGNEMNT);
}

#ifdef GTS_WINDOWS

//------------------------------------------------------------------------------
TEST(GtsMalloc, gts_win_recalloc)
{
    {
        void* ptr = gts_malloc(4);
        void* pNew = gts_win_recalloc(ptr, 0, 0);
        ASSERT_EQ(pNew, nullptr);
    }

    {
        void* ptr = gts_malloc(4);
        void* pNew = gts_win_recalloc(ptr, 0, 4);
        ASSERT_EQ(pNew, nullptr);
        gts_free(ptr);
    }

    {
        void* ptr = gts_malloc(4);
        void* pNew = gts_win_recalloc(ptr, SIZE_MAX, SIZE_MAX);
        ASSERT_EQ(pNew, nullptr);
        gts_free(ptr);
    }

    {
        void* ptr = gts_malloc(4);
        void* pNew = gts_win_recalloc(ptr, 4, 4);
        ASSERT_NE(pNew, nullptr);

        uint8_t* p = (uint8_t*)pNew;
        for (size_t ii = 0; ii < 4 * 4; ++ii)
        {
            ASSERT_EQ(p[ii], 0);
        }

        gts_free(pNew);
    }
}

//------------------------------------------------------------------------------
TEST(GtsMalloc, gts_win_wcsdup)
{
    wchar_t const* pStr = L"Test String";
    wchar_t* pDup = gts_win_wcsdup(pStr);
    ASSERT_STREQ(pStr, pDup);
    gts_free(pDup);
}

//------------------------------------------------------------------------------
TEST(GtsMalloc, gts_win_mbsdup)
{
    wchar_t const* pWStr = L"Test String";

    char pStr[32];
    size_t num;
    wcstombs_s(&num, pStr, 32, pWStr, 32);

    unsigned char* pDup = gts_win_mbsdup((unsigned char const*)pStr);
    ASSERT_EQ(_mbscmp((unsigned char const*)pStr, pDup), 0);
    gts_free(pDup);
}

//------------------------------------------------------------------------------
TEST(GtsMalloc, gts_win_dupenv_s)
{
    char* pStr = nullptr;
    size_t size;
    errno_t err = gts_win_dupenv_s(&pStr, &size, "PATH");
    ASSERT_EQ(err, 0);
    gts_free(pStr);
}

//------------------------------------------------------------------------------
TEST(GtsMalloc, gts_win_wdupenv_s)
{
    wchar_t* pStr = nullptr;
    size_t size;
    errno_t err = gts_win_wdupenv_s(&pStr, &size, L"PATH");
    ASSERT_EQ(err, 0);
    gts_free(pStr);
}

//------------------------------------------------------------------------------
TEST(GtsMalloc, gts_win_aligned_realloc)
{
    void* ptr = gts_malloc(4);
    uint8_t* p = (uint8_t*)ptr;
    for (size_t ii = 0; ii < 4; ++ii)
    {
        p[ii] = (uint8_t)ii;
    }

    ptr = gts_win_aligned_realloc(ptr, 8, 8);
    ASSERT_NE(ptr, nullptr);
    ASSERT_EQ(uintptr_t(ptr) & (7), uintptr_t(0));

    p = (uint8_t*)ptr;
    for (size_t ii = 0; ii < 4; ++ii)
    {
        ASSERT_EQ(p[ii], (uint8_t)ii);
    }

    gts_free(ptr);
}

//------------------------------------------------------------------------------
TEST(GtsMalloc, gts_win_aligned_recalloc)
{
    {
        void* ptr = gts_malloc(4);
        void* pNew = gts_win_aligned_recalloc(ptr, 0, 0, 8);
        ASSERT_EQ(pNew, nullptr);
    }

    {
        void* ptr = gts_malloc(4);
        void* pNew = gts_win_aligned_recalloc(ptr, 0, 4, 8);
        ASSERT_EQ(pNew, nullptr);
        gts_free(ptr);
    }

    {
        void* ptr = gts_malloc(4);
        void* pNew = gts_win_aligned_recalloc(ptr, SIZE_MAX, SIZE_MAX, 8);
        ASSERT_EQ(pNew, nullptr);
        gts_free(ptr);
    }

    {
        void* ptr = gts_malloc(4);
        void* pNew = gts_win_aligned_recalloc(ptr, 4, 4, 8);
        ASSERT_EQ(uintptr_t(ptr) & (7), uintptr_t(0));
        ASSERT_NE(pNew, nullptr);

        uint8_t* p = (uint8_t*)pNew;
        for (size_t ii = 0; ii < 4 * 4; ++ii)
        {
            ASSERT_EQ(p[ii], 0);
        }

        gts_free(pNew);
    }
}

//------------------------------------------------------------------------------
TEST(GtsMalloc, gts_win_aligned_offset_malloc)
{
    void* ptr = gts_win_aligned_offset_malloc(8, 8, 3);
    gts_free(ptr);
}

//------------------------------------------------------------------------------
TEST(GtsMalloc, gts_win_aligned_offset_realloc)
{
    void* ptr = gts_malloc(4);
    uint8_t* p = (uint8_t*)ptr;
    for (size_t ii = 0; ii < 4; ++ii)
    {
        p[ii] = (uint8_t)ii;
    }

    ptr = gts_win_aligned_offset_realloc(ptr, 8, 8, 3);
    p = (uint8_t*)ptr;
    for (size_t ii = 0; ii < 4; ++ii)
    {
        ASSERT_EQ(p[ii], (uint8_t)ii);
    }

    gts_free(ptr);
}

//------------------------------------------------------------------------------
TEST(GtsMalloc, gts_win_aligned_offset_recalloc)
{
    void* ptr = gts_malloc(4);
    uint8_t* p = (uint8_t*)ptr;
    for (size_t ii = 0; ii < 4; ++ii)
    {
        p[ii] = (uint8_t)ii;
}

    ptr = gts_win_aligned_offset_recalloc(ptr, 2, 8, 8, 3);
    p = (uint8_t*)ptr;
    for (size_t ii = 0; ii < 8 * 2; ++ii)
    {
        ASSERT_EQ(p[ii], 0);
    }

    gts_free(ptr);
}

#elif GTS_POSIX

#endif

} // namespace testing
