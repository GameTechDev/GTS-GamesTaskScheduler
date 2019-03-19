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
#include "gts/containers/parallel/DistributedSlabAllocatorBucketed.h"

namespace gts {

//------------------------------------------------------------------------------
DistributedSlabAllocatorBucketed::DistributedSlabAllocatorBucketed()
    : m_slabSize(0)
    , m_numBuckets(0)
    , m_fastCacheLineDiv(0)
{}

//------------------------------------------------------------------------------
bool DistributedSlabAllocatorBucketed::init(uint32_t accessorThreadCount, uint32_t sizeBoundaryPow2)
{
    m_sizeBoundaryPow2 = sizeBoundaryPow2;
    m_slabSize         = 0;
    m_numBuckets       = 0;
    m_fastCacheLineDiv = (uint32_t)log2(sizeBoundaryPow2);

    if (!isPow2(sizeBoundaryPow2))
    {
        GTS_ASSERT(0 && "sizeBoundaryPow2 is not a power of 2");
        return false;
    }

    uint32_t slabSizePow2 = Memory::getAllocationGranularity();
    return _init(accessorThreadCount, slabSizePow2);
}

//------------------------------------------------------------------------------
bool DistributedSlabAllocatorBucketed::init(uint32_t accessorThreadCount, uint32_t sizeBoundaryPow2, uint32_t slabSize)
{
    m_sizeBoundaryPow2 = sizeBoundaryPow2;
    m_slabSize         = nextPow2(slabSize);
    m_numBuckets       = 0;
    m_fastCacheLineDiv = (uint32_t)log2(sizeBoundaryPow2);

    if (!isPow2(sizeBoundaryPow2))
    {
        GTS_ASSERT(0 && "sizeBoundaryPow2 is not a power of 2");
        return false;
    }

    uint32_t slabSizePow2 = nextPow2(slabSize);
    return _init(accessorThreadCount, slabSizePow2);
}

//------------------------------------------------------------------------------
bool DistributedSlabAllocatorBucketed::_init(uint32_t accessorThreadCount, uint32_t slabSize)
{
    // Number of cacheline size buckets available.
    m_numBuckets = slabSize / m_sizeBoundaryPow2;

    // Make num buckets a power of 2 for cheaper bucket calcs.
    m_numBuckets = nextPow2(m_numBuckets);

    // Make a bucket per allocator.
    m_allocatorsByBucket.resize(m_numBuckets);
    uint32_t nextAllocSize = m_sizeBoundaryPow2;
    for (size_t ii = 0; ii < m_numBuckets; ++ii)
    {
        bool result = m_allocatorsByBucket[ii].init(nextAllocSize, accessorThreadCount, slabSize);
        if (!result)
        {
            return false;
        }
        nextAllocSize += m_sizeBoundaryPow2;
    }

    return true;
}

//------------------------------------------------------------------------------
DistributedSlabAllocatorBucketed::~DistributedSlabAllocatorBucketed()
{
}

//------------------------------------------------------------------------------
void* DistributedSlabAllocatorBucketed::allocate(uint32_t threadIndex, uint32_t size)
{
    uint32_t bucket = calculateBucket(size);
    return m_allocatorsByBucket[bucket].allocate(threadIndex);
}

//------------------------------------------------------------------------------
void DistributedSlabAllocatorBucketed::free(uint32_t threadIndex, void* ptr)
{
    uint32_t bucket = calculateBucket(ptr);
    m_allocatorsByBucket[bucket].free(threadIndex, ptr);
}

//------------------------------------------------------------------------------
DistributedSlabAllocatorBucketed::AllocatorVector const& DistributedSlabAllocatorBucketed::getAllAllocators() const
{
    return m_allocatorsByBucket;
}

} // namespace gts
