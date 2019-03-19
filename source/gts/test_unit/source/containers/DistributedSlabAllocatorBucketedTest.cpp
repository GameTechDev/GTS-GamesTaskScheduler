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
#include <chrono>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "gts/containers/parallel/DistributedSlabAllocatorBucketed.h"

using namespace gts;

namespace testing {

//------------------------------------------------------------------------------
TEST(DistributedSlabAllocatorBucketedTest, bucketCountTest)
{
    {
        const uint32_t sizeBoundary = 8; // power of 2

        DistributedSlabAllocatorBucketed slabAlloc;
        slabAlloc.init(1, sizeBoundary);

        ASSERT_EQ(Memory::getAllocationGranularity() / sizeBoundary, slabAlloc.bucketCount());
    }

    EXPECT_DEATH({
        const uint32_t sizeBoundary = 9; // not power of 2

        DistributedSlabAllocatorBucketed slabAlloc;
        slabAlloc.init(1, sizeBoundary);

        ASSERT_EQ(Memory::getAllocationGranularity() / sizeBoundary, slabAlloc.bucketCount());
    }, "");
}

//------------------------------------------------------------------------------
TEST(DistributedSlabAllocatorBucketedTest, sizeBucketTest)
{
    const uint32_t sizeBoundary = GTS_NO_SHARING_CACHE_LINE_SIZE;

    DistributedSlabAllocatorBucketed slabAlloc;
    slabAlloc.init(1, sizeBoundary);

    EXPECT_DEATH({ slabAlloc.calculateBucket(0u); }, "");
    ASSERT_EQ(0u, slabAlloc.calculateBucket(1));

    ASSERT_EQ(0u, slabAlloc.calculateBucket(sizeBoundary - 1));
    ASSERT_EQ(0u, slabAlloc.calculateBucket(sizeBoundary));
    ASSERT_EQ(1u, slabAlloc.calculateBucket(sizeBoundary + 1));

    uint32_t multiple = 5;
    
    ASSERT_EQ(multiple - 1, slabAlloc.calculateBucket(sizeBoundary * multiple - 1));
    ASSERT_EQ(multiple - 1, slabAlloc.calculateBucket(sizeBoundary * multiple));
    ASSERT_EQ(multiple, slabAlloc.calculateBucket(sizeBoundary * multiple + 1));
}

//------------------------------------------------------------------------------
TEST(DistributedSlabAllocatorBucketedTest, allocBucketTest)
{
    const uint32_t threadIdx = 0;
    const uint32_t sizeBoundary = GTS_NO_SHARING_CACHE_LINE_SIZE;

    DistributedSlabAllocatorBucketed slabAlloc;
    slabAlloc.init(1, sizeBoundary);

    EXPECT_DEATH({ slabAlloc.allocate(threadIdx, 0u); }, "");

    void* ptr = slabAlloc.allocate(threadIdx, 1u);
    ASSERT_EQ(0u, slabAlloc.calculateBucket(ptr));
    slabAlloc.free(threadIdx, ptr);


    ptr = slabAlloc.allocate(threadIdx, sizeBoundary - 1);
    ASSERT_EQ(0u, slabAlloc.calculateBucket(ptr));
    slabAlloc.free(threadIdx, ptr);

    ptr = slabAlloc.allocate(threadIdx, sizeBoundary);
    ASSERT_EQ(0u, slabAlloc.calculateBucket(ptr));
    slabAlloc.free(threadIdx, ptr);

    ptr = slabAlloc.allocate(threadIdx, sizeBoundary + 1);
    ASSERT_EQ(1u, slabAlloc.calculateBucket(ptr));
    slabAlloc.free(threadIdx, ptr);


    uint32_t multiple = 5;

    ptr = slabAlloc.allocate(threadIdx, sizeBoundary * multiple - 1);
    ASSERT_EQ(multiple - 1, slabAlloc.calculateBucket(ptr));
    slabAlloc.free(threadIdx, ptr);

    ptr = slabAlloc.allocate(threadIdx, sizeBoundary * multiple);
    ASSERT_EQ(multiple - 1, slabAlloc.calculateBucket(ptr));
    slabAlloc.free(threadIdx, ptr);

    ptr = slabAlloc.allocate(threadIdx, sizeBoundary * multiple + 1);
    ASSERT_EQ(multiple, slabAlloc.calculateBucket(ptr));
    slabAlloc.free(threadIdx, ptr);
}

} // namespace testing
