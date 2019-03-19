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

#include "gts/containers/parallel/DistributedSlabAllocator.h"
#include "gts/containers/AlignedAllocator.h"

namespace gts {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A set of thread distributed slab allocator bucketed by cacheline size.
 */
class DistributedSlabAllocatorBucketed
{
public:

    using AllocatorVector = gts::Vector<DistributedSlabAllocator, AlignedAllocator<DistributedSlabAllocator, GTS_NO_SHARING_CACHE_LINE_SIZE>>;

public: // STRUCTORS

    DistributedSlabAllocatorBucketed();

    ~DistributedSlabAllocatorBucketed();

public: // MUTATORS

    bool init(uint32_t accessorThreadCount, uint32_t sizeBoundaryPow2);

    bool init(uint32_t accessorThreadCount, uint32_t sizeBoundaryPow2, uint32_t slabSize);

    void* allocate(uint32_t threadInde, uint32_t size);

    void free(uint32_t threadIndex, void* ptr);

public: // ACCESSORS

    AllocatorVector const& getAllAllocators() const;

    GTS_INLINE uint32_t calculateBucket(void* ptr) const
    {
        uint32_t size = DistributedSlabAllocator::getSize(ptr);
        return calculateBucket(size);
    }

    GTS_INLINE uint32_t calculateBucket(uint32_t size) const
    {
        GTS_ASSERT(size > 0);
        // -1 so that size == n*m_sizeBoundaryPow2 falls in bucket n-1.
        return (size - 1) >> m_fastCacheLineDiv;
    }

    GTS_INLINE uint32_t bucketCount() const
    {
        return m_numBuckets;
    }

private:

    bool _init(uint32_t accessorThreadCount, uint32_t slabSize);

    AllocatorVector m_allocatorsByBucket;
    uint32_t m_sizeBoundaryPow2;
    uint32_t m_slabSize;
    uint32_t m_numBuckets;
    uint32_t m_fastCacheLineDiv;
};

} // namespace gts
