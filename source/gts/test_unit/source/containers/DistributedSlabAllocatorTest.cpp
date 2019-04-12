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

#include "gts/containers/parallel/DistributedSlabAllocator.h"

using namespace gts;

namespace testing {

//------------------------------------------------------------------------------
TEST(DistributedSlabAllocatorTest, constructor)
{
    const uint32_t threadIdx = 0;

    DistributedSlabAllocator slabAlloc;
    slabAlloc.init(sizeof(size_t), 1);

    // Size should be established.
    ASSERT_NE(0U, slabAlloc.slabSize());

    // No allocation should have happened.
    ASSERT_EQ(0U, slabAlloc.slabCount(threadIdx));
    ASSERT_EQ(0U, slabAlloc.nodeCount(threadIdx));
}

//------------------------------------------------------------------------------
TEST(DistributedSlabAllocatorTest, simpleAllocAndFree)
{
    const uint32_t threadIdx = 0;
    const uint32_t nodeSize = (uint32_t)(sizeof(DistributedSlabAllocator::HeaderNode) + sizeof(size_t));

    DistributedSlabAllocator slabAlloc;
    slabAlloc.init(sizeof(size_t), 1);

    // Allocate an object.
    size_t* pInt = (size_t*)slabAlloc.allocate(threadIdx);
    ASSERT_NE(nullptr, pInt);

    // A single slab should be allocated and broken into object nodes.
    ASSERT_EQ(1U, slabAlloc.slabCount(threadIdx));
    ASSERT_EQ(slabAlloc.slabSize() / nodeSize, slabAlloc.nodeCount(threadIdx));

    // Free the object so no memory is leaked.
    slabAlloc.free(threadIdx, pInt);
}

//------------------------------------------------------------------------------
TEST(DistributedSlabAllocatorTest, simpleMemoryLeak)
{
    const uint32_t threadIdx = 0;

    // No freeing memory will result in an assert.
    EXPECT_DEATH(
        DistributedSlabAllocator slabAlloc;
        slabAlloc.init(sizeof(size_t), 1);
        slabAlloc.allocate(threadIdx);
    , "");
}

//------------------------------------------------------------------------------
TEST(DistributedSlabAllocatorTest, doubleFree)
{
    const uint32_t threadIdx = 0;

    DistributedSlabAllocator slabAlloc;
    slabAlloc.init(sizeof(size_t), 1);

    // Allocate an object.
    size_t* pInt = (size_t*)slabAlloc.allocate(threadIdx);
    ASSERT_NE(nullptr, pInt);

    // Free the object so no memory is leaked.
    slabAlloc.free(threadIdx, pInt);

    // Double free wil result in a assert.
    EXPECT_DEATH(
        slabAlloc.free(threadIdx, pInt);
    , "");
}

//------------------------------------------------------------------------------
TEST(DistributedSlabAllocatorTest, deferredFree)
{
    const uint32_t threadIdx0 = 0;
    const uint32_t threadIdx1 = 1;

    const uint32_t nodeSize = (uint32_t)(sizeof(DistributedSlabAllocator::HeaderNode) + sizeof(size_t));

    // Make two bins with a slab big enough to hold one node.
    DistributedSlabAllocator slabAlloc;
    slabAlloc.init(sizeof(size_t), 2, nodeSize);

    // Allocate one object on thread 0
    size_t* pInt = (size_t*)slabAlloc.allocate(threadIdx0);
    ASSERT_EQ(1U, slabAlloc.slabCount(threadIdx0));

    // Free the object on thread 1.
    slabAlloc.free(threadIdx1, pInt);
}

//------------------------------------------------------------------------------
TEST(DistributedSlabAllocatorTest, multiSlabAllocation)
{
    const uint32_t numAllocs = 2;
    const uint32_t threadIdx = 0;

    const uint32_t nodeSize = (uint32_t)(sizeof(DistributedSlabAllocator::HeaderNode) + sizeof(size_t));

    // Make two bins with a slab big enough to hold one node.
    DistributedSlabAllocator slabAlloc;
    slabAlloc.init(sizeof(size_t), 2, nodeSize);

    std::vector<size_t*> allocs(numAllocs);

    // Allocate several objects.
    for (uint32_t ii = 0; ii < numAllocs; ++ii)
    {
        allocs[ii] = (size_t*)slabAlloc.allocate(threadIdx);
    }
    ASSERT_EQ(numAllocs, slabAlloc.slabCount(threadIdx));

    for (uint32_t ii = 0; ii < numAllocs; ++ii)
    {
        // Free the objects.
        slabAlloc.free(threadIdx, allocs[ii]);
    }
}

//------------------------------------------------------------------------------
TEST(DistributedSlabAllocatorTest, multiSlabAllocationAndDeferredFree)
{
    const uint32_t numAllocs  = 2;
    const uint32_t threadIdx0 = 0;
    const uint32_t threadIdx1 = 1;

    const uint32_t nodeSize = (uint32_t)(sizeof(DistributedSlabAllocator::HeaderNode) + sizeof(size_t));

    // Make two bins with a slab big enough to hold one node.
    DistributedSlabAllocator slabAlloc;
    slabAlloc.init(sizeof(size_t), 2, nodeSize);

    std::vector<size_t*> allocs(numAllocs);

    // Allocate several objects on thread 0
    for (uint32_t ii = 0; ii < numAllocs; ++ii)
    {
        allocs[ii] = (size_t*)slabAlloc.allocate(threadIdx0);
    }
    ASSERT_EQ(numAllocs, slabAlloc.slabCount(threadIdx0));

    for (uint32_t ii = 0; ii < numAllocs; ++ii)
    {
        // Free the object on thread 1.
        slabAlloc.free(threadIdx1, allocs[ii]);
    }
}

//------------------------------------------------------------------------------
TEST(DistributedSlabAllocatorTest, unknownThreadSlabAllocation)
{
    const uint32_t numAllocs = 2;
    const uint32_t numThreads = 1;
    const uint32_t threadIdx = 3;

    const uint32_t nodeSize = (uint32_t)(sizeof(DistributedSlabAllocator::HeaderNode) + sizeof(size_t));

    // Make two bins with a slab big enough to hold one node.
    DistributedSlabAllocator slabAlloc;
    slabAlloc.init(sizeof(size_t), numThreads, nodeSize);

    std::vector<size_t*> allocs(numAllocs);

    // Allocate several objects.
    for (uint32_t ii = 0; ii < numAllocs; ++ii)
    {
        allocs[ii] = (size_t*)slabAlloc.allocate(threadIdx);
    }
    ASSERT_EQ(numAllocs, slabAlloc.slabCount(threadIdx));

    for (uint32_t ii = 0; ii < numAllocs; ++ii)
    {
        // Free the object.
        slabAlloc.free(threadIdx, allocs[ii]);
    }
}

//------------------------------------------------------------------------------
TEST(DistributedSlabAllocatorTest, unknownThreadSlabAllocationDeferredFree)
{
    const uint32_t numAllocs = 2;
    const uint32_t numThreads = 1;
    const uint32_t threadIdx0 = 0;
    const uint32_t threadIdx3 = 3;

    const uint32_t nodeSize = (uint32_t)(sizeof(DistributedSlabAllocator::HeaderNode) + sizeof(size_t));

    // Make two bins with a slab big enough to hold one node.
    DistributedSlabAllocator slabAlloc;
    slabAlloc.init(sizeof(size_t), numThreads, nodeSize);

    std::vector<size_t*> allocs(numAllocs);

    // Allocate several objects on an unknown thread.
    for (uint32_t ii = 0; ii < numAllocs; ++ii)
    {
        allocs[ii] = (size_t*)slabAlloc.allocate(threadIdx3);
    }
    ASSERT_EQ(numAllocs, slabAlloc.slabCount(threadIdx3));

    for (uint32_t ii = 0; ii < numAllocs; ++ii)
    {
        // Free the objects on a known thread.
        slabAlloc.free(threadIdx0, allocs[ii]);
    }
}

} // namespace testing
