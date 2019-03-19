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
#include "gts/containers/parallel/DistributedSlabAllocator.h"

#include <forward_list>

#ifdef ENABLE_GTS_SLAB_ALLOCATOR_STATS

#define ALLOCATOR_STATS_COUNT(stat) countStat(stat);
#define ALLOCATOR_STATS_COUNT_NODE(stat) myLocalAllocator->countStat(stat);

#else

#define ALLOCATOR_STATS_COUNT(stat)
#define ALLOCATOR_STATS_COUNT_NODE(stat)

#endif

namespace gts {

struct SlabList : public std::forward_list<void*> {};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// LocalAllocator

//------------------------------------------------------------------------------
DistributedSlabAllocator::LocalAllocator::LocalAllocator()
    : m_pSlabList(nullptr)
{
    m_pSlabList = new SlabList;
}

//------------------------------------------------------------------------------
DistributedSlabAllocator::LocalAllocator::~LocalAllocator()
{
    delete m_pSlabList;
}

//------------------------------------------------------------------------------
void* DistributedSlabAllocator::LocalAllocator::allocate()
{
    HeaderNode* pAlloc = _getNextFreeNode();

    if (pAlloc == nullptr)
    {
        _reclaimDeferredNodes();
        pAlloc = _getNextFreeNode();
        if (pAlloc == nullptr)
        {
            return (void*)_slowAlloc();
        }
    }

#ifndef NDEBUG
    ::memset((uint8_t*)pAlloc + sizeof(HeaderNode), 0xCD, m_allocationSize);
#endif

    ALLOCATOR_STATS_COUNT(NUM_ALLOCATIONS);

    return (uint8_t*)pAlloc + sizeof(HeaderNode);
}

//------------------------------------------------------------------------------
DistributedSlabAllocator::HeaderNode* DistributedSlabAllocator::LocalAllocator::_getNextFreeNode()
{
    HeaderNode* pNode = m_freeList.pHead.load(gts::memory_order::relaxed);
    if (pNode)
    {
        GTS_ASSERT(pNode->isFree == 1 && "double alloc!");
        pNode->isFree = 0;

        HeaderNode* pNewHead = pNode->pNext.load(gts::memory_order::relaxed);
        m_freeList.pHead.store(pNewHead, gts::memory_order::relaxed);
        return pNode;
    }

    return nullptr;
}

//------------------------------------------------------------------------------
void DistributedSlabAllocator::LocalAllocator::_reclaimDeferredNodes()
{
    // Take deferred list.
    HeaderNode* pMyDeferredNodes = m_deferredFreeList.pHead.exchange(nullptr);

    if (pMyDeferredNodes != nullptr)
    {
        // Walk to the end of the free list
        HeaderNode* pLastFreeListNode = m_freeList.pHead.load(gts::memory_order::relaxed);
        if (pLastFreeListNode != nullptr)
        {
            while (pLastFreeListNode->pNext.load(gts::memory_order::relaxed) != nullptr)
            {
                pLastFreeListNode = pLastFreeListNode->pNext.load(gts::memory_order::relaxed);
            }

            // Added the deferred list to the end of the free list
            pLastFreeListNode->pNext.store(pMyDeferredNodes, gts::memory_order::relaxed);

            ALLOCATOR_STATS_COUNT(NUM_MEMORY_RECLAIMS);
        }
        else
        {
            // Added the deferred list to the end of the free list
            m_freeList.pHead.store(pMyDeferredNodes, gts::memory_order::relaxed);
        }
    }
}

//------------------------------------------------------------------------------
void* DistributedSlabAllocator::LocalAllocator::_slowAlloc()
{
    // Allocate a new slab.
    void* pSlab = Memory::osAlloc(0, m_slabSize, AllocationType::RESERVE_COMMIT);

    // Make sure the slab is grain aligned.
    if (!isAligned(pSlab, m_slabSize))
    {
        GTS_ASSERT(0);
        return nullptr;
    }

    HeaderNode* pMyFreeList = nullptr;

    // Save the page for destruction.
    m_pSlabList->push_front(pSlab);
    m_slabCount++;

    pMyFreeList = m_freeList.pHead.load(gts::memory_order::relaxed);

    //
    // Subdivide the new slab and add the divisions to the free list.

    uint8_t* pMem = (uint8_t*)pSlab;
    uint32_t objectCount = (m_slabSize / (uint32_t)(sizeof(HeaderNode) + m_allocationSize));

    for (uint32_t ii = 0; ii < objectCount; ++ii)
    {
        HeaderNode* pNode          = (HeaderNode*)pMem;
        pNode->myThreadIdx         = m_threadIdx;
        pNode->accessorThreadCount = m_accessorThreadCount;
        pNode->size                = m_allocationSize;
        pNode->isFree              = 1;
        pNode->myLocalAllocator    = this;

        pNode->pNext.store(pMyFreeList, gts::memory_order::relaxed);

        pMyFreeList = pNode;
        pMem += sizeof(HeaderNode) + m_allocationSize;
    }

    m_freeList.pHead.store(pMyFreeList, gts::memory_order::relaxed);
    m_createdNodeCount += objectCount;

    ALLOCATOR_STATS_COUNT(NUM_SLOW_PATH_ALLOCATIONS);

    return allocate();
}

//------------------------------------------------------------------------------
void DistributedSlabAllocator::LocalAllocator::_destroy()
{
    _reclaimDeferredNodes();

    // Verify that there are no leaked nodes.
    uint32_t nodeCount = 0;
    HeaderNode* pNode = m_freeList.pHead.load(gts::memory_order::relaxed);
    while (pNode != nullptr)
    {
        nodeCount++;
        pNode = pNode->pNext.load();
    }
    GTS_ASSERT(nodeCount == m_createdNodeCount && "Memory Leak!");

    // Free the slabs.
    for (auto slab : *m_pSlabList)
    {
        Memory::osFree(slab, m_slabSize, FreeType::RELEASE);
    }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// HeaderNode

//------------------------------------------------------------------------------
void DistributedSlabAllocator::HeaderNode::free(uint32_t threadIndex)
{
    GTS_ASSERT(isFree == 0 && "double free!");
    isFree = 1;

    ALLOCATOR_STATS_COUNT_NODE(LocalAllocator::NUM_FREES);

    if (threadIndex < accessorThreadCount && myThreadIdx == threadIndex)
    {
        // Free the current node
        pNext.store(myLocalAllocator->m_freeList.pHead.load(), gts::memory_order::relaxed);
        myLocalAllocator->m_freeList.pHead.store(this, gts::memory_order::relaxed);

        GTS_ASSERT(this != pNext.load(gts::memory_order::relaxed));
    }
    else
    {
        ALLOCATOR_STATS_COUNT_NODE(LocalAllocator::NUM_DEFERRED_FREES);

        addDeferredNode(this);
    }
}

//------------------------------------------------------------------------------
void DistributedSlabAllocator::HeaderNode::addDeferredNode(HeaderNode* pNode)
{
    HeaderNode* pHead = myLocalAllocator->m_deferredFreeList.pHead.load(gts::memory_order::acquire);
    pNode->pNext.store(pHead, gts::memory_order::relaxed);

    uint32_t spinCount = 1;
    while (!myLocalAllocator->m_deferredFreeList.pHead.compare_exchange_strong(pHead, pNode))
    {
        pNode->pNext.store(pHead, gts::memory_order::relaxed);
        SpinWait::backoffUntilSpinCount(spinCount, 16);
    }

    GTS_ASSERT(pNode != pNode->pNext.load(gts::memory_order::relaxed));
}

//------------------------------------------------------------------------------
DistributedSlabAllocator::DistributedSlabAllocator()
    : m_pLocalAllocatorsByThreadIdx(nullptr)
    , m_accessorThreadCount(0)
    , m_allocationSize(0)
    , m_slabSize(0)
{}

//------------------------------------------------------------------------------
bool DistributedSlabAllocator::init(uint32_t allocationSize, uint32_t accessorThreadCount)
{
    m_accessorThreadCount = accessorThreadCount;
    m_allocationSize = allocationSize;
    m_slabSize = Memory::getAllocationGranularity();

    if (accessorThreadCount <= 0)
    {
        GTS_ASSERT(0 && "accessorThreadCount is zero");
        return false;
    }
    
    if (m_slabSize < sizeof(HeaderNode))
    {
        GTS_ASSERT(0 && "slabSize < sizeof(HeaderNode)");
        return false;
    }

    return _init();
}

//------------------------------------------------------------------------------
bool DistributedSlabAllocator::init(uint32_t allocationSize, uint32_t accessorThreadCount, uint32_t slabSize)
{
    m_accessorThreadCount =accessorThreadCount;
    m_allocationSize = allocationSize;
    m_slabSize = nextPow2(slabSize);

    if (accessorThreadCount <= 0)
    {
        GTS_ASSERT(0 && "accessorThreadCount is zero");
        return false;
    }

    if (m_slabSize < sizeof(HeaderNode))
    {
        GTS_ASSERT(0 && "slabSize < sizeof(HeaderNode)");
        return false;
    }

    if (m_slabSize > Memory::getAllocationGranularity())
    {
        GTS_ASSERT(0 && "m_slabSize > Memory::getAllocationGranularity()");
        return false;
    }

    return _init();
}

//------------------------------------------------------------------------------
DistributedSlabAllocator::~DistributedSlabAllocator()
{
    _shutdown();
}

//------------------------------------------------------------------------------
void* DistributedSlabAllocator::allocate(uint32_t threadIndex)
{
    if (threadIndex < m_accessorThreadCount)
    {
        return m_pLocalAllocatorsByThreadIdx[threadIndex].allocate();
    }
    else
    {
        Lock<Mutex> lock(m_unknownThreadsMutex);
        return m_unknownThreadAllocator.allocate();
    }
}

//------------------------------------------------------------------------------
void DistributedSlabAllocator::free(uint32_t threadIndex, void* ptr)
{
#ifndef NDEBUG
    ::memset(ptr, 0xDD, m_allocationSize);
#endif
    HeaderNode* pNode = (HeaderNode*)((uint8_t*)ptr - sizeof(HeaderNode));

    if (threadIndex < m_accessorThreadCount)
    {
        pNode->free(threadIndex);
    }
    else
    {
        Lock<Mutex> lock(m_unknownThreadsMutex);
        pNode->free(threadIndex);
    }
}

//------------------------------------------------------------------------------
uint32_t DistributedSlabAllocator::slabSize() const
{
    return m_slabSize;
}

//------------------------------------------------------------------------------
uint32_t DistributedSlabAllocator::slabCount(uint32_t threadIndex) const
{
    return _getLocalAllocator(threadIndex)->m_slabCount;
}

//------------------------------------------------------------------------------
uint32_t DistributedSlabAllocator::nodeCount(uint32_t threadIndex) const
{
    return _getLocalAllocator(threadIndex)->m_createdNodeCount;
}

//------------------------------------------------------------------------------
uint32_t DistributedSlabAllocator::getSize(void* ptr)
{
    HeaderNode* pNode = (HeaderNode*)((uint8_t*)ptr - sizeof(HeaderNode));
    return pNode->size;
}

//------------------------------------------------------------------------------
DistributedSlabAllocator::LocalAllocator const* DistributedSlabAllocator::_getLocalAllocator(uint32_t threadIndex) const
{
    if (threadIndex < m_accessorThreadCount)
    {
        return &m_pLocalAllocatorsByThreadIdx[threadIndex];
    }
    else
    {
        return &m_unknownThreadAllocator;
    }
}

//------------------------------------------------------------------------------
DistributedSlabAllocator::LocalAllocator* DistributedSlabAllocator::_getLocalAllocator(uint32_t threadIndex)
{
    if (threadIndex < m_accessorThreadCount)
    {
        return &m_pLocalAllocatorsByThreadIdx[threadIndex];
    }
    else
    {
        return &m_unknownThreadAllocator;
    }
}

//------------------------------------------------------------------------------
bool DistributedSlabAllocator::_init()
{
    m_pLocalAllocatorsByThreadIdx = alignedVectorNew<LocalAllocator, GTS_NO_SHARING_CACHE_LINE_SIZE>(m_accessorThreadCount);
    if (m_pLocalAllocatorsByThreadIdx == nullptr)
    {
        return false;
    }

    for (uint32_t ii = 0; ii < m_accessorThreadCount; ++ii)
    {
        m_pLocalAllocatorsByThreadIdx[ii].m_allocationSize          = m_allocationSize;
        m_pLocalAllocatorsByThreadIdx[ii].m_threadIdx               = ii;
        m_pLocalAllocatorsByThreadIdx[ii].m_slabSize                = m_slabSize;
        m_pLocalAllocatorsByThreadIdx[ii].m_accessorThreadCount     = m_accessorThreadCount;
        m_pLocalAllocatorsByThreadIdx[ii].m_pUnknownThreadsMutex    = &m_unknownThreadsMutex;
    }

    m_unknownThreadAllocator.m_allocationSize       = m_allocationSize;
    m_unknownThreadAllocator.m_threadIdx            = UINT32_MAX;
    m_unknownThreadAllocator.m_slabSize             = m_slabSize;
    m_unknownThreadAllocator.m_accessorThreadCount  = m_accessorThreadCount;
    m_unknownThreadAllocator.m_pUnknownThreadsMutex = &m_unknownThreadsMutex;

    return true;
}

//------------------------------------------------------------------------------
void DistributedSlabAllocator::_shutdown()
{
    m_unknownThreadAllocator._destroy();
    for (uint32_t ii = 0; ii < m_accessorThreadCount; ++ii)
    {
        m_pLocalAllocatorsByThreadIdx[ii]._destroy();
    }

    alignedVectorDelete(m_pLocalAllocatorsByThreadIdx, m_accessorThreadCount);
}

} // namespace gts
