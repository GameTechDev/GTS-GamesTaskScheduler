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
#include "gts/containers/parallel/BinnedAllocator.h"

#include <vector>
#include <set>

#include "gts/analysis/Trace.h"

namespace gts {
namespace internal {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// DList

//------------------------------------------------------------------------------
DList::Node* DList::popFront()
{
    if (!m_pHead)
    {
        return nullptr;
    }
    Node* pNode  = m_pHead;
    remove(pNode);
    return pNode;
}

//------------------------------------------------------------------------------
void DList::pushFront(DList::Node* pNode)
{
    GTS_INTERNAL_ASSERT(!containes(pNode));

    if (!m_pHead)
    {
        m_pHead = pNode;
        m_pTail = pNode;
    }
    else
    {
        m_pHead->pPrev = pNode;
        pNode->pNext   = m_pHead;
        m_pHead        = pNode;
    }

    GTS_INTERNAL_ASSERT(size() == 0 ? m_pHead == m_pTail : true);
}

//------------------------------------------------------------------------------
void DList::pushBack(DList::Node* pNode)
{
    GTS_INTERNAL_ASSERT(!containes(pNode));

    if (!m_pTail)
    {
        m_pHead = pNode;
        m_pTail = pNode;
    }
    else
    {
        m_pTail->pNext = pNode;
        pNode->pPrev   = m_pTail;
        m_pTail        = pNode;
    }

    GTS_INTERNAL_ASSERT(size() == 0 ? m_pHead == m_pTail : true);
}

//------------------------------------------------------------------------------
void DList::remove(DList::Node* pNode)
{
    GTS_INTERNAL_ASSERT(containes(pNode));

    if (!pNode->pPrev)
    {
        m_pHead = pNode->pNext;
    }
    else
    {
        pNode->pPrev->pNext = pNode->pNext;
    }

    if (!pNode->pNext)
    {
        m_pTail = pNode->pPrev;
    }
    else
    {
        pNode->pNext->pPrev = pNode->pPrev;
    }

    pNode->pPrev = nullptr;
    pNode->pNext = nullptr;

    GTS_INTERNAL_ASSERT(size() == 0 ? m_pHead == m_pTail : true);
}

//------------------------------------------------------------------------------
void DList::clear()
{
    m_pHead = nullptr;
    m_pTail = nullptr;
}

//------------------------------------------------------------------------------
size_t DList::size() const
{
    uint32_t count = 0;
    Node* pNode = m_pHead;
    while (pNode)
    {
        count++;
        pNode = pNode->pNext;
    }

    uint32_t rcount = 0;
    pNode = m_pTail;
    while (pNode)
    {
        rcount++;
        pNode = pNode->pPrev;
    }

    GTS_INTERNAL_ASSERT(count == rcount);

    return count;
}

//------------------------------------------------------------------------------
bool DList::empty() const
{
    return m_pHead == nullptr;
}

//------------------------------------------------------------------------------
bool DList::containes(Node* pNode) const
{
    Node* pCurr = m_pHead;
    while (pNode)
    {
        if (pCurr == pNode)
        {
            return true;
        }
        pNode = pNode->pNext;
    }
    return false;
}

} // namespace internal

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// SlabHeader

//------------------------------------------------------------------------------
bool SlabHeader::reclaimNonLocalPages()
{
    GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::BINNED_ALLOCATOR_DEBUG, analysis::Color::AntiqueWhite, "SLABHEADER RECLAIM PAGES", this, 0);

    // Reclaim the non-local free list.
    FreeListNode* pReclaimedFreeList = pNonLocalPageFreeList.exchange(nullptr, memory_order::acq_rel);

    if (pReclaimedFreeList != nullptr)
    {
        // Walk to the end of the local free list
        FreeListNode* pLastFreeListBlock = pLocalPageFreeList;
        if (pLastFreeListBlock != nullptr)
        {
            while (pLastFreeListBlock->pNextFree.load(gts::memory_order::relaxed) != nullptr)
            {
                pLastFreeListBlock = pLastFreeListBlock->pNextFree.load(gts::memory_order::relaxed);
            }

            // Added the reclaimed free list to the end of the local free list
            pLastFreeListBlock->pNextFree.store(pReclaimedFreeList, gts::memory_order::relaxed);

            //ALLOCATOR_STATS_COUNT(NUM_MEMORY_RECLAIMS);
        }
        else
        {
            // The reclaimed free list is the new free list
            pLocalPageFreeList = pReclaimedFreeList;
        }

        return true;
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// PageHeader

//------------------------------------------------------------------------------
bool PageHeader::reclaimNonLocalBlocks()
{
    GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::BINNED_ALLOCATOR_DEBUG, analysis::Color::AntiqueWhite, "PAGEHEADER RECLAIM BLOCKS", this, 0);

    // Reclaim the non-local free list.
    FreeListNode* pReclaimedFreeList = pNonLocalFreeList.exchange(nullptr, memory_order::acq_rel);

    if (pReclaimedFreeList != nullptr)
    {
        // Walk to the end of the local free list
        FreeListNode* pLastFreeListBlock = pLocalFreeList;
        if (pLastFreeListBlock != nullptr)
        {
            while (pLastFreeListBlock->pNextFree.load(gts::memory_order::relaxed) != nullptr)
            {
                pLastFreeListBlock = pLastFreeListBlock->pNextFree.load(gts::memory_order::relaxed);
            }

            // Added the reclaimed free list to the end of the local free list
            pLastFreeListBlock->pNextFree.store(pReclaimedFreeList, gts::memory_order::relaxed);

            //ALLOCATOR_STATS_COUNT(NUM_MEMORY_RECLAIMS);
        }
        else
        {
            // The reclaimed free list is the new free list
            pLocalFreeList = pReclaimedFreeList;
        }
    }

    return pLocalFreeList != nullptr;
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// MemoryStore

constexpr uint32_t MemoryStore::SLAB_SIZE;
constexpr uint32_t MemoryStore::PAGE_SIZE_CLASS_0;
constexpr uint32_t MemoryStore::PAGE_SIZE_CLASS_1;
constexpr uint32_t MemoryStore::PAGE_SIZE_CLASS_2;
constexpr uint32_t MemoryStore::PAGE_SIZE_CLASS_3;
constexpr uint32_t MemoryStore::PAGE_FREE_LISTS_COUNT;

const uint32_t MemoryStore::SINGLE_PAGE_HEADER_SIZE =
    (uint32_t)alignUpTo(sizeof(PageHeader) + sizeof(SlabHeader), GTS_GET_OS_PAGE_SIZE());

//------------------------------------------------------------------------------
MemoryStore::~MemoryStore()
{
    clear();
}

//------------------------------------------------------------------------------
bool MemoryStore::empty() const
{
    if(m_freedSlabs.empty() == false)
    {
        return false;
    }

    for(uint32_t ii = 0; ii < PAGE_FREE_LISTS_COUNT; ++ii)
    {
        if(m_abandonedSlabFreeListByClass[ii].empty() == false)
        {
            return false;
        }
    }

    if(m_slabCount.load(memory_order::acquire) != 0)
    {
        return false;
    }

    return true;
}

//------------------------------------------------------------------------------
void MemoryStore::_freeSlabList(Queue& slabList, size_t& slabCount)
{
    SlabHeader* pSlab = nullptr;

    while (!slabList.empty())
    {
        slabList.tryPop(pSlab);

        // Free and committed Pages exist because Slabs can be abandoned
        // while other threads hold their memory, and if no other thread
        // touches an abandoned Slab, it will not be decommitted.
        for (uint32_t iPage = 0, len = pSlab->numPages; iPage < len; ++iPage)
        {
            PageHeader* pPage = pSlab->pPageHeaders + iPage;
            if(pPage->state == MemoryState::STATE_COMMITTED && pPage->numUsedBlocks.load(memory_order::relaxed) == 0)
            {
                deallocatePage(pPage);
            }
        }

        GTS_INTERNAL_ASSERT(pSlab->numCommittedPages == 0 && "Memory Leak!");

        _freeSlab(pSlab);
        --slabCount;
    }
}

//------------------------------------------------------------------------------
void MemoryStore::clear()
{
    size_t slabCount = m_slabCount.load(memory_order::relaxed);

    _freeSlabList(m_freedSlabs, slabCount);

    for(uint32_t iList = 0; iList < PAGE_FREE_LISTS_COUNT; ++iList)
    {
        Queue& list = m_abandonedSlabFreeListByClass[iList];
        _freeSlabList(list, slabCount);
    }

    // Make sure we freed all the slabs that were reserved.
    GTS_INTERNAL_ASSERT(slabCount == 0 && "Slab Leak!");

    m_slabCount.store(0, memory_order::release);
}

//------------------------------------------------------------------------------
SlabHeader* MemoryStore::allocateSlab(size_t pageSize)
{
    if (pageSize > UINT32_MAX || (pageSize & (GTS_GET_OS_PAGE_SIZE() - 1)) != 0)
    {
        GTS_INTERNAL_ASSERT(0);
        return nullptr;
    }

    GTS_TRACE_SCOPED_ZONE_P0(analysis::CaptureMask::BINNED_ALLOCATOR_DEBUG, analysis::Color::AntiqueWhite, "SLAB ALLOC");

    SlabHeader* pSlab = nullptr;

    if (pageSize + SINGLE_PAGE_HEADER_SIZE < SLAB_SIZE)
    {
        pSlab = _getFreeListSlab(pageSize);
        GTS_INTERNAL_ASSERT((pSlab ? pSlab->tid == 0 : true) && "The slab is already in use!");

        if (pSlab)
        {
            _resetSlab(pSlab, (uint32_t)pageSize);
            return pSlab;
        }
        else
        {
            pSlab = _reserveNewSlab((uint32_t)pageSize);
            if (pSlab)
            {
                _initalizeSlab(pSlab, SLAB_SIZE, (uint32_t)pageSize);
            }
        }
    }
    else
    {
        pSlab = _reserveNewSlab((uint32_t)pageSize);
        if (pSlab)
        {
            pSlab->tid = ThisThread::getId();
        }
    }

    GTS_TRACE_ZONE_MARKER_P1(analysis::CaptureMask::BINNED_ALLOCATOR_DEBUG, analysis::Color::AntiqueWhite, "SLAB ALLOCED", pSlab);

    return pSlab;
}


//------------------------------------------------------------------------------
SlabHeader* MemoryStore::_getFreeListSlab(size_t pageSize)
{
    GTS_TRACE_SCOPED_ZONE_P1(analysis::CaptureMask::BINNED_ALLOCATOR_DEBUG, analysis::Color::AntiqueWhite, "TRY GET FREE LIST SLAB", pageSize);

    SlabHeader* pSlab = nullptr;

    m_freedSlabs.tryPop(pSlab);
    if(pSlab)
    {
        GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::BINNED_ALLOCATOR_DEBUG, analysis::Color::AntiqueWhite, "GOT FREE LIST SLAB", pageSize, pSlab);

        return pSlab;
    }

    m_abandonedSlabFreeListByClass[pageSizeToIndex(pageSize)].tryPop(pSlab);

    if(pSlab)
    {
        GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::BINNED_ALLOCATOR_DEBUG, analysis::Color::AntiqueWhite, "GOT ABANDONED SLAB", pageSize, pSlab);
    }

    return pSlab;
}

//------------------------------------------------------------------------------
void MemoryStore::_resetSlab(SlabHeader* pSlab, uint32_t pageSize)
{
    if (pSlab->pageSize == pageSize)
    {
        pSlab->tid = ThisThread::getId();
    }
    else
    {
        GTS_OS_VIRTUAL_DECOMMIT((uint8_t*)pSlab, pSlab->totalHeaderSize);
        _initalizeSlab(pSlab, SLAB_SIZE, pageSize);
    }
}

//------------------------------------------------------------------------------
SlabHeader* MemoryStore::_reserveNewSlab(uint32_t pageSize)
{
    GTS_TRACE_SCOPED_ZONE_P1(analysis::CaptureMask::BINNED_ALLOCATOR_DEBUG, analysis::Color::AntiqueWhite, "SLAB RESERVE", pageSize);

    void* pAlloc = nullptr;
    uint32_t actualSize = SLAB_SIZE;

    if (pageSize + SINGLE_PAGE_HEADER_SIZE > SLAB_SIZE)
    {
        // Adjust for large pages over our slab size.
        actualSize = (uint32_t)alignUpTo(pageSize + SINGLE_PAGE_HEADER_SIZE, GTS_GET_OS_ALLOCATION_GRULARITY());
        GTS_INTERNAL_ASSERT(actualSize % GTS_GET_OS_ALLOCATION_GRULARITY() == 0);
    }

    // Oversize the allocation to account for the alignment.
    uint32_t alignedSize = SLAB_SIZE + actualSize;
    GTS_INTERNAL_ASSERT(alignedSize % GTS_GET_OS_ALLOCATION_GRULARITY() == 0);

#if GTS_WINDOWS

    // Win32 cannot deallocate around sub-regions, so we try to allocate on the exact alignment.
    // Loop because there is a data race between freeing the oversized region and
    // trying to allocate from the aligned address in the region. We retry
    // if we fail the race.
    size_t iTry = 0; GTS_UNREFERENCED_PARAM(iTry);
    while(true)
    {
        GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::BINNED_ALLOCATOR_DEBUG, analysis::Color::AntiqueWhite, "TRY OVERSIZED RESERVE", pageSize, iTry);

        pAlloc = GTS_OS_VIRTUAL_ALLOCATE(nullptr, alignedSize, false, false);
        if (((uintptr_t)pAlloc & (SLAB_SIZE - 1)) == 0)
        {
            GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::BINNED_ALLOCATOR_DEBUG, analysis::Color::Green, "TRY OVERSIZED RESERVE LUCKY", pageSize, iTry);
            // We got lucky and ptr is aligned. Decommit the leftovers.
            GTS_OS_VIRTUAL_DECOMMIT((uint8_t*)pAlloc + actualSize, SLAB_SIZE);
            break;
        }
        else if (uintptr_t(pAlloc) == UINTPTR_MAX)
        {
            GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::BINNED_ALLOCATOR_DEBUG, analysis::Color::Red, "TRY OVERSIZED RESERVE NO MEM", pageSize, iTry);
            // Out of memory.
            GTS_INTERNAL_ASSERT(uintptr_t(pAlloc) != UINTPTR_MAX);
            pAlloc = nullptr;
            break;
        }
        else
        {
            GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::BINNED_ALLOCATOR_DEBUG, analysis::Color::AntiqueWhite, "TRY SUBREGION RESERVE", pageSize, iTry);

            // Free the oversized region and then try to allocate from the
            // aligned address in the region.
            GTS_OS_VIRTUAL_FREE(pAlloc, alignedSize);
            void* pAligned = (void*)alignUpTo((uintptr_t)pAlloc, SLAB_SIZE);

            // Race for this address.
            pAlloc = GTS_OS_VIRTUAL_ALLOCATE(pAligned, actualSize, false, false);

            if (pAlloc == pAligned)
            {
                GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::BINNED_ALLOCATOR_DEBUG, analysis::Color::Green, "TRY SUBREGION RESERVE SUCCESS", pageSize, iTry);
                // We successfully got the region.
                break;
            }

            if ((uintptr_t)pAlloc == UINTPTR_MAX)
            {
                GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::BINNED_ALLOCATOR_DEBUG, analysis::Color::Red, "TRY SUBREGION RESERVE NO MEM", pageSize, iTry);
                // We are out of memory.
                GTS_INTERNAL_ASSERT(uintptr_t(pAlloc) != UINTPTR_MAX);
                pAlloc = nullptr;
                break;
            }

            // Another thread beat us to it. Try again.
            if (pAlloc != nullptr)
            {
                GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::BINNED_ALLOCATOR_DEBUG, analysis::Color::Yellow, "TRY SUBREGION RESERVE LOST RACE", pageSize, iTry);
                GTS_OS_VIRTUAL_FREE(pAlloc, actualSize);
            }

            GTS_PAUSE();
        }

        ++iTry;
    }

#else

    pAlloc = GTS_OS_VIRTUAL_ALLOCATE(nullptr, alignedSize, false, false);

    if (pAlloc == 0)
    {
        GTS_TRACE_ZONE_MARKER_P1(analysis::CaptureMask::BINNED_ALLOCATOR_DEBUG, analysis::Color::Red, "TRY RESERVE FAILED", pageSize);
        GTS_INTERNAL_ASSERT(0);
        return nullptr;
    }

    if((uintptr_t)pAlloc == UINTPTR_MAX)
    {
        GTS_TRACE_ZONE_MARKER_P1(analysis::CaptureMask::BINNED_ALLOCATOR_DEBUG, analysis::Color::Red, "TRY RESERVE NO MEM", pageSize);

        GTS_INTERNAL_ASSERT(0);
        return nullptr;
    }

    // Deallocate around the aligned sub-region.
    void* pAligned = (void*)alignUpTo(uintptr_t(pAlloc), SLAB_SIZE);
    uint32_t preSize = (uint8_t*)pAligned - (uint8_t*)pAlloc;
    uint32_t desiredSize = alignUpTo(actualSize, GTS_GET_OS_ALLOCATION_GRULARITY());
    uint32_t postSize = alignedSize - preSize - desiredSize;
    if (preSize > 0)
    {
        GTS_OS_VIRTUAL_FREE(pAlloc, preSize);
    }
    if (postSize > 0)
    {
        GTS_OS_VIRTUAL_FREE((uint8_t*)pAligned + desiredSize, postSize);
    }
    pAlloc = pAligned;

#endif
    

    GTS_INTERNAL_ASSERT(pAlloc);

    if(pAlloc)
    {
        m_slabCount.fetch_add(1, memory_order::relaxed);
        return _initalizeSlab(pAlloc, actualSize, pageSize);
    }

    return nullptr;
}

//------------------------------------------------------------------------------
SlabHeader* MemoryStore::_initalizeSlab(void* pRawSlab, uint32_t slabSize, uint32_t pageSize)
{
    uint32_t slabHeaderSize  = (uint32_t)sizeof(SlabHeader);
    uint32_t totalHeaderSize = SINGLE_PAGE_HEADER_SIZE;

    SlabHeader* pSlab = nullptr;

    GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::BINNED_ALLOCATOR_DEBUG, analysis::Color::AntiqueWhite, "SLAB INIT", pRawSlab, pageSize);

    uint32_t numPages         = (slabSize - slabHeaderSize) / pageSize;
    uint32_t pageHeadersSize  = sizeof(PageHeader) * numPages;
    totalHeaderSize           = (uint32_t)alignUpTo(slabHeaderSize + pageHeadersSize, GTS_GET_OS_PAGE_SIZE());
    uint32_t pageArea         = slabSize - totalHeaderSize;

    // Reduce page count until we fit.
    while (pageSize * numPages > pageArea)
    {
        if (numPages == 0)
        {
            break;
        }
        numPages--;

        pageHeadersSize  = sizeof(PageHeader) * numPages;
        totalHeaderSize  = (uint32_t)alignUpTo(slabHeaderSize + pageHeadersSize, GTS_GET_OS_PAGE_SIZE());
        pageArea         = slabSize - totalHeaderSize;

        if (numPages == 0)
        {
            GTS_INTERNAL_ASSERT(numPages != 0 && "(blockSize) is too large for (pageSize)");
            return nullptr;
        }
    }

    pSlab = new (GTS_OS_VIRTUAL_COMMIT(pRawSlab, totalHeaderSize)) SlabHeader();
    GTS_INTERNAL_ASSERT(pSlab == pRawSlab);
    if (pSlab != nullptr)
    {
        GTS_INTERNAL_ASSERT(totalHeaderSize < UINT16_MAX);
        pSlab->totalHeaderSize = (uint16_t)totalHeaderSize;
        _initalizeSlabHeader(pSlab, totalHeaderSize, slabSize, pageSize, numPages);
    }

    return pSlab;
}

//------------------------------------------------------------------------------
void MemoryStore::_initalizeSlabHeader(
    SlabHeader* pSlab,
    uint32_t totalHeaderSize,
    uint32_t slabSize,
    uint32_t pageSize,
    uint32_t numPages)
{
    pSlab->pPageHeaders = (PageHeader*)((uint8_t*)pSlab + sizeof(SlabHeader));
    pSlab->pBlocks      = (PageHeader*)((uint8_t*)pSlab + totalHeaderSize);
    pSlab->tid          = ThisThread::getId();
    pSlab->slabSize     = slabSize;
    pSlab->pageSize     = pageSize;
    pSlab->numPages     = (uint16_t)numPages;

    // Initialize all the PageHeaders and add them to the free list.
    for (uint32_t ii = 0; ii < numPages; ++ii)
    {
        PageHeader* pPage = new (pSlab->pPageHeaders + ii) PageHeader();

        pPage->pBlocks = (uint8_t*)pSlab->pBlocks + pageSize * ii;
        pPage->tid = ThisThread::getId();

        FreeListNode* pNode = (FreeListNode*)(pPage);
        pNode->pNextFree.store(pSlab->pLocalPageFreeList, memory_order::relaxed);
        pSlab->pLocalPageFreeList = pNode;
    }
}

//------------------------------------------------------------------------------
void MemoryStore::_freeSlab(SlabHeader* pSlab)
{
    uint32_t actualSize = pSlab->slabSize;
    if(actualSize != SLAB_SIZE)
    {
        actualSize = (uint32_t)alignUpTo(actualSize + SINGLE_PAGE_HEADER_SIZE, GTS_GET_OS_ALLOCATION_GRULARITY());
    }
    GTS_OS_VIRTUAL_FREE(pSlab, actualSize);
}

//------------------------------------------------------------------------------
void MemoryStore::deallocateSlab(SlabHeader* pSlab, bool freeIt)
{
    bool result = true;

    pSlab->tid = 0;
    GTS_INTERNAL_ASSERT(pSlab->myAllocator == nullptr);
    GTS_INTERNAL_ASSERT(pSlab->pNext == nullptr);
    GTS_INTERNAL_ASSERT(pSlab->pPrev == nullptr);

    if (pSlab->slabSize != SLAB_SIZE)
    {
        GTS_TRACE_SCOPED_ZONE_P1(analysis::CaptureMask::BINNED_ALLOCATOR_DEBUG, analysis::Color::AntiqueWhite, "LARGE SLAB FREE", pSlab);
        _freeSlab(pSlab);
        m_slabCount.fetch_sub(1, memory_order::relaxed);
    }
    else if (freeIt)
    {
        GTS_TRACE_SCOPED_ZONE_P1(analysis::CaptureMask::BINNED_ALLOCATOR_DEBUG, analysis::Color::AntiqueWhite, "SLAB FREE", pSlab);
        GTS_INTERNAL_ASSERT(pSlab->numCommittedPages == 0);
        result = m_freedSlabs.tryPush(pSlab);
    }
    else
    {
        GTS_TRACE_SCOPED_ZONE_P1(analysis::CaptureMask::BINNED_ALLOCATOR_DEBUG, analysis::Color::AntiqueWhite, "SLAB ABANDON", pSlab);
        result = m_abandonedSlabFreeListByClass[pageSizeToIndex(pSlab->pageSize)].tryPush(pSlab);
    }

    GTS_UNREFERENCED_PARAM(result);
    GTS_INTERNAL_ASSERT(result);
}

//------------------------------------------------------------------------------
PageHeader* MemoryStore::allocatePage(SlabHeader* pSlab, size_t blockSize)
{
    if (blockSize > UINT32_MAX || blockSize > pSlab->pageSize)
    {
        GTS_INTERNAL_ASSERT(0);
        return nullptr;
    }

    GTS_INTERNAL_ASSERT(pSlab ? pSlab->tid == ThisThread::getId() : true);

    GTS_TRACE_SCOPED_ZONE_P1(analysis::CaptureMask::BINNED_ALLOCATOR_DEBUG, analysis::Color::AntiqueWhite, "PAGE ALLOC", pSlab);

    if (!pSlab)
    {
        return nullptr;
    }

    if (pSlab->numPages == pSlab->numCommittedPages)
    {
        return nullptr;
    }

    PageHeader* pPage = (PageHeader*)pSlab->pLocalPageFreeList;
    if (!pPage)
    {
        pSlab->reclaimNonLocalPages();
        pPage = (PageHeader*)pSlab->pLocalPageFreeList;
        if (!pPage)
        {
            return nullptr;
        }
    }

    pSlab->pLocalPageFreeList = pSlab->pLocalPageFreeList->pNextFree.load(memory_order::relaxed);

    GTS_INTERNAL_ASSERT(pPage->state == MemoryState::STATE_FREE);
    void* pBlocks = GTS_OS_VIRTUAL_COMMIT(pPage->pBlocks, pSlab->pageSize);
    GTS_UNREFERENCED_PARAM(pBlocks);
    GTS_INTERNAL_ASSERT(pBlocks != nullptr);
    GTS_INTERNAL_ASSERT((uintptr_t)((uint8_t*)pBlocks + pSlab->pageSize) - (uintptr_t)pSlab <= pSlab->slabSize);
    pSlab->numCommittedPages++;

    GTS_INTERNAL_ASSERT(ThisThread::getId() == pSlab->tid);

    initPage(pPage, pSlab->pageSize, blockSize);

    GTS_INTERNAL_ASSERT(pPage->pLocalFreeList != nullptr);

    GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::BINNED_ALLOCATOR_DEBUG, analysis::Color::AntiqueWhite, "PAGE ALLOCED", pSlab, pPage);

    return pPage;
}

//------------------------------------------------------------------------------
void MemoryStore::initPage(PageHeader* pPage, size_t pageSize, size_t blockSize)
{
    pPage->state     = MemoryState::STATE_COMMITTED;
    pPage->tid       = ThisThread::getId();
    pPage->blockSize = (uint32_t)blockSize;

    uint32_t numBlocks   = uint32_t(pageSize / blockSize);
    uint8_t* pBlockStart = (uint8_t*)pPage->pBlocks;

    GTS_INTERNAL_ASSERT(numBlocks != 0);

    // Push all the blocks onto the local free list.
    FreeListNode* pBlock = nullptr;
    for (uint32_t ii = 0; ii < numBlocks; ++ii)
    {
        pBlock = (FreeListNode*)(pBlockStart + blockSize * ii);
        GTS_INTERNAL_ASSERT((uintptr_t)((uint8_t*)pBlock + blockSize) - (uintptr_t)pBlockStart <= pageSize);
        pBlock->pNextFree.store(pPage->pLocalFreeList, memory_order::relaxed);
        pPage->pLocalFreeList = pBlock;
    }
}

//------------------------------------------------------------------------------
void MemoryStore::deallocatePage(PageHeader* pPage)
{
    // Must happen under this Page's Slab's mutex.

    GTS_INTERNAL_ASSERT(pPage->numUsedBlocks.load(memory_order::acquire) == 0);
    GTS_INTERNAL_ASSERT(pPage->state == MemoryState::STATE_COMMITTED);

    SlabHeader* pSlab = toSlab(pPage);
    pPage->state = MemoryState::STATE_FREE;
    GTS_OS_VIRTUAL_DECOMMIT(pPage->pBlocks, pSlab->pageSize);

    // Clear header data about the committed memory.
    pPage->pLocalFreeList = nullptr;
    pPage->pNonLocalFreeList.store(nullptr, memory_order::relaxed);
    pPage->hasAlignedBlocks = false;

    // Add the page to the free list.
    FreeListNode* pFreePage = (FreeListNode*)pPage;

    // Local free.
    if (ThisThread::getId() == pSlab->tid)
    {
        GTS_TRACE_SCOPED_ZONE_P1(analysis::CaptureMask::BINNED_ALLOCATOR_DEBUG, analysis::Color::AntiqueWhite, "PAGE LOCAL FREE", pPage);

        pFreePage->pNextFree.store(pSlab->pLocalPageFreeList, memory_order::relaxed);
        pSlab->pLocalPageFreeList = pFreePage;
    }
    // Non-local free.
    else
    {
        GTS_TRACE_SCOPED_ZONE_P1(analysis::CaptureMask::BINNED_ALLOCATOR_DEBUG, analysis::Color::AntiqueWhite, "PAGE NONLOCAL FREE", pPage);

        FreeListNode* pHead = pSlab->pNonLocalPageFreeList.load(gts::memory_order::acquire);
        pFreePage->pNextFree.store(pHead, gts::memory_order::relaxed);

        backoff_type backoff;
        while (!pSlab->pNonLocalPageFreeList.compare_exchange_weak(pHead, pFreePage, memory_order::acq_rel, memory_order::relaxed))
        {
            pFreePage->pNextFree.store(pHead, gts::memory_order::relaxed);
            backoff.tick();
        }
    }

    pSlab->numCommittedPages -= 1;
}

namespace internal {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// BlockAllocator

//------------------------------------------------------------------------------
BlockAllocator::BlockAllocator()
    : m_pBinnedAllocator(nullptr)
    , m_pActivePage(nullptr)
    , m_blockSize(0)
{
}

//------------------------------------------------------------------------------
BlockAllocator::~BlockAllocator()
{
    shutdown();
}

//------------------------------------------------------------------------------
bool BlockAllocator::init(BinnedAllocator* pBinnedAllocator, size_type blockSize, size_type pageSize)
{
    GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::BINNED_ALLOCATOR_DEBUG, analysis::Color::AntiqueWhite, "BLOCKALLOC INIT", this, 0);

    if (!pBinnedAllocator)
    {
        GTS_INTERNAL_ASSERT(pBinnedAllocator != nullptr);
        return false;
    }

    if (pageSize % GTS_GET_OS_PAGE_SIZE() != 0)
    {
        GTS_INTERNAL_ASSERT(pageSize % GTS_GET_OS_PAGE_SIZE() == 0 && "Page size must be a multiple of the OS page size.");
        return false;
    }

    if (blockSize < GTS_MALLOC_ALIGNEMNT)
    {
        GTS_INTERNAL_ASSERT(blockSize > GTS_MALLOC_ALIGNEMNT && "Block size must be greater than or eqaul to GTS_MALLOC_ALIGNEMNT.");
        return false;
    }

    if (blockSize % GTS_MALLOC_ALIGNEMNT != 0)
    {
        GTS_INTERNAL_ASSERT(blockSize % GTS_MALLOC_ALIGNEMNT == 0 && "Block size must be a multiple of GTS_MALLOC_ALIGNEMNT.");
        return false;
    }

    if (blockSize > pageSize)
    {
        GTS_INTERNAL_ASSERT(blockSize + sizeof(PageHeader) <= pageSize && "Page size is too small.");
        return false;
    }

    m_pBinnedAllocator = pBinnedAllocator;
    m_blockSize        = blockSize;
    m_pageSize         = pageSize;

    return true;
}

//------------------------------------------------------------------------------
void* BlockAllocator::allocate()
{
    GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::BINNED_ALLOCATOR_DEBUG, analysis::Color::AntiqueWhite, "BLOCKALLOC ALLOC", this, 0);

    void* pBlock = _getNextFreeBlock();
    if (pBlock == nullptr)
    {
        return nullptr;
    }

    GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::BINNED_ALLOCATOR_DEBUG, analysis::Color::AntiqueWhite, "BLOCKALLOC ALLOCED", this, pBlock);

    GTS_INTERNAL_ASSERT(pBlock);

    return pBlock;
}

//------------------------------------------------------------------------------
void* BlockAllocator::_getNextFreeBlock()
{
    FreeListNode* pFreeBlock = nullptr;

    // 1) Try to get a local free list Block.
    if (m_pActivePage)
    {
        pFreeBlock = m_pActivePage->pLocalFreeList;
    }

    // If the local free list is empty. Try to get another Block from somewhere else.
    if (pFreeBlock == nullptr)
    {
        if (!(
            // 2) Try to reclaim Blocks on the active Page's non-local free list.
            _reclaimNonlocalFreeBlocks() ||
            // 3) Try to get a Page from the Slab list or commit a new Page from the active Slab.
            _getNewPage()))
        {
            // The system in unable to allocate anymore memory.
            return nullptr;
        }

        pFreeBlock = m_pActivePage->pLocalFreeList;
    }

    if (!pFreeBlock)
    {
        GTS_INTERNAL_ASSERT(0 && "Out of memory!");
        return nullptr;
    }

    // Pop the block from the list.
    m_pActivePage->pLocalFreeList = m_pActivePage->pLocalFreeList->pNextFree.load(memory_order::acquire);

    m_pActivePage->numUsedBlocks.fetch_add(1, memory_order::release);

    return pFreeBlock;
}

//------------------------------------------------------------------------------
bool BlockAllocator::_reclaimNonlocalFreeBlocks()
{
    GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::BINNED_ALLOCATOR_DEBUG, analysis::Color::AntiqueWhite, "BLOCKALLOC RECLAIM", this, 0);
    if(m_pActivePage)
    {
        return m_pActivePage->reclaimNonLocalBlocks();
    }
    return false;
}

//------------------------------------------------------------------------------
bool BlockAllocator::_getNewPage()
{
    PageHeader* pPage = m_pBinnedAllocator->_allocatePage(m_pageSize, m_blockSize);

    if (pPage)
    {
        m_pActivePage = pPage;
        GTS_INTERNAL_ASSERT(m_pActivePage->pLocalFreeList);
        return true;
    }

    return false;
}

//------------------------------------------------------------------------------
void BlockAllocator::deallocate(void* ptr)
{
    if (ptr == nullptr)
    {
        return;
    }

    SlabHeader* pSlab = MemoryStore::toSlab(ptr);
    PageHeader* pPage = MemoryStore::toPage(pSlab, ptr);

    // Re-purpose the block memory.
    FreeListNode* pFreeBlock = new (ptr) FreeListNode;

    if (ThisThread::getId() == pPage->tid)
    {
        GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::BINNED_ALLOCATOR_DEBUG, analysis::Color::AntiqueWhite, "BLOCKALLOC LOCAL FREE", this, ptr);

        pFreeBlock->pNextFree.store(pPage->pLocalFreeList, memory_order::relaxed);
        pPage->pLocalFreeList = pFreeBlock;
    }
    else
    {
        GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::BINNED_ALLOCATOR_DEBUG, analysis::Color::AntiqueWhite, "BLOCKALLOC NONLOCAL FREE", this, ptr);

        FreeListNode* pHead = pPage->pNonLocalFreeList.load(gts::memory_order::acquire);
        pFreeBlock->pNextFree.store(pHead, gts::memory_order::relaxed);

        backoff_type backoff;
        while (!pPage->pNonLocalFreeList.compare_exchange_weak(pHead, pFreeBlock, memory_order::acq_rel, memory_order::relaxed))
        {
            pFreeBlock->pNextFree.store(pHead, gts::memory_order::relaxed);
            backoff.tick();
        }

        GTS_INTERNAL_ASSERT(pFreeBlock != pFreeBlock->pNextFree.load(gts::memory_order::relaxed));
    }

    pPage->numUsedBlocks.fetch_sub(1, memory_order::release);

    if (pPage->numUsedBlocks.load(memory_order::relaxed) == 0 &&
        m_pActivePage != pPage &&
        ThisThread::getId() == pPage->tid)
    {
        m_pBinnedAllocator->_deallocatePage(pSlab, pPage);
    }
}

//------------------------------------------------------------------------------
void BlockAllocator::shutdown()
{
    GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::BINNED_ALLOCATOR_DEBUG, analysis::Color::AntiqueWhite, "BLOCKALLOC SHTDWN", this, 0);

    m_pBinnedAllocator = nullptr;
    m_pActivePage      = nullptr;
    m_blockSize        = 0;
    m_pageSize         = 0;
}

} // namespace internal

using namespace internal;

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
// BinnedAllocator

constexpr uint32_t BinnedAllocator::BIN_SIZE_CLASS_0;
constexpr uint32_t BinnedAllocator::BIN_SIZE_CLASS_1;
constexpr uint32_t BinnedAllocator::BIN_SIZE_CLASS_2;
#if GTS_64BIT
constexpr uint32_t BinnedAllocator::BIN_SIZE_CLASS_3;
#endif
constexpr uint32_t BinnedAllocator::BIN_IDX_OVERSIZED;
constexpr uint32_t BinnedAllocator::BIN_DIVISOR;

//------------------------------------------------------------------------------
BinnedAllocator::~BinnedAllocator()
{
    shutdown();
}

//------------------------------------------------------------------------------
bool BinnedAllocator::init(MemoryStore* pMemoryStore)
{
    GTS_INTERNAL_ASSERT(pMemoryStore);
    m_pMemoryStore = pMemoryStore;

    uint32_t numBinsClass0 = BIN_SIZE_CLASS_0 / GTS_MALLOC_ALIGNEMNT;
    uint32_t numBinsClass1 = log2i(BIN_SIZE_CLASS_1) * BIN_DIVISOR - log2i(BIN_SIZE_CLASS_0) * BIN_DIVISOR;
    uint32_t numBinsClass2 = log2i(BIN_SIZE_CLASS_2) * BIN_DIVISOR - numBinsClass1 - log2i(BIN_SIZE_CLASS_0) * BIN_DIVISOR;
    uint32_t numBinsClass3 = 1;

    m_numBins = numBinsClass0 + numBinsClass1 + numBinsClass2 + numBinsClass3;

    m_allocatorsByBin.resize(m_numBins);

    // Initialize each bin.
    uint32_t binIdx = 0;

    // Class 0: [8B, 1024B] with GTS_MALLOC_ALIGNEMNT spacing.
    uint32_t class0Size = GTS_MALLOC_ALIGNEMNT;
    for (uint32_t ii = 0; ii < numBinsClass0; ++ii)
    {
        m_allocatorsByBin[binIdx++].init(this, class0Size, MemoryStore::PAGE_SIZE_CLASS_0);
        class0Size += GTS_MALLOC_ALIGNEMNT;
    }

    GTS_INTERNAL_ASSERT(binIdx == numBinsClass0);

    // Class 1: (1KiB, 8Kib] with BIN_DIVISOR spacing per power-of-2.
    for (uint32_t lowerBound = BIN_SIZE_CLASS_0; lowerBound < BIN_SIZE_CLASS_1; lowerBound *= 2)
    {
        uint32_t upperBound = nextPow2(lowerBound + 1);
        uint32_t intervalSize = (upperBound - lowerBound) / BIN_DIVISOR;

        for (uint32_t jj = 1; jj <= BIN_DIVISOR; ++jj)
        {
            m_allocatorsByBin[binIdx++].init(this, lowerBound + intervalSize * jj, MemoryStore::PAGE_SIZE_CLASS_1);
        }
    }

    GTS_INTERNAL_ASSERT(binIdx == numBinsClass0 + numBinsClass1);

    // Class 2: (8KiB, 32Kib} with BIN_DIVISOR spacing per power-of-2.
    for (uint32_t lowerBound = BIN_SIZE_CLASS_1; lowerBound < BIN_SIZE_CLASS_2; lowerBound *= 2)
    {
        uint32_t upperBound = nextPow2(lowerBound + 1);
        uint32_t intervalSize = (upperBound - lowerBound) / BIN_DIVISOR;

        for (uint32_t jj = 1; jj <= BIN_DIVISOR; ++jj)
        {
            m_allocatorsByBin[binIdx++].init(this, lowerBound + intervalSize * jj, MemoryStore::PAGE_SIZE_CLASS_2);
        }
    }

    GTS_INTERNAL_ASSERT(binIdx == numBinsClass0 + numBinsClass1 + numBinsClass2);

    // Class 3: (32KiB, 512KiB] with just one bin. All sizes are mapped to a 512KiB Page.
    m_allocatorsByBin[binIdx++].init(this, MemoryStore::PAGE_SIZE_CLASS_3, MemoryStore::PAGE_SIZE_CLASS_3);

    GTS_INTERNAL_ASSERT(binIdx == m_numBins);

    m_isInitialized = true;

    return true;
}

//------------------------------------------------------------------------------
void BinnedAllocator::shutdown()
{
    GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::BINNED_ALLOCATOR_DEBUG, analysis::Color::AntiqueWhite, "BUCKETALLOC SHTDWN", this, 0);
    for (uint32_t ii = 0; ii < m_allocatorsByBin.size(); ++ii)
    {
        GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::BINNED_ALLOCATOR_DEBUG, analysis::Color::AntiqueWhite, "MYBLOCKALLOC SHTDWN", this, ii);
        m_allocatorsByBin[ii].shutdown();
    }

    // Iterate over all Slab lists.
    for (uint32_t ii = 0; ii < MemoryStore::PAGE_FREE_LISTS_COUNT; ++ii)
    {
        m_activeSlabs[ii]         = nullptr;
        internal::DList& slabList = m_slabLists[ii];
        size_type& slabCount      = m_slabCounts[ii];

        GTS_INTERNAL_ASSERT(slabCount == slabList.size());

        // Iterate over all Slabs.
        while (!slabList.empty())
        {
            SlabHeader* pCurr = (SlabHeader*)slabList.popFront();
            GTS_INTERNAL_ASSERT(ThisThread::getId() == pCurr->tid);
            slabCount--;

            pCurr->reclaimNonLocalPages();

            // Decommit any free pages.
            for (uint32_t iPage = 0, len = pCurr->numPages; iPage < len; ++iPage)
            {
                PageHeader* pPage = pCurr->pPageHeaders + iPage;
                if(pPage->state == MemoryState::STATE_COMMITTED && pPage->numUsedBlocks.load(memory_order::relaxed) == 0)
                {
                    m_pMemoryStore->deallocatePage(pPage);
                }
            }

            // Iterate over all free Pages.
            FreeListNode* pPageNode = pCurr->pLocalPageFreeList;
            while (pPageNode != nullptr)
            {
                GTS_INTERNAL_ASSERT(((PageHeader*)pPageNode)->numUsedBlocks.load(memory_order::relaxed) == 0 && "Memory Leak!");
                pPageNode = pPageNode->pNextFree.load(memory_order::relaxed);
            }

            m_pMemoryStore->deallocateSlab(pCurr, false);
        }

        GTS_INTERNAL_ASSERT(slabCount == 0 && "Slab Leak!");

        slabList.clear();
        slabCount = 0;

    }

    m_isInitialized = false;
}

//------------------------------------------------------------------------------
void* BinnedAllocator::allocate(size_t size)
{
    GTS_INTERNAL_ASSERT(size < UINT32_MAX);
    if (size >= UINT32_MAX)
    {
        return nullptr;
    }

    uint32_t bin = calculateBin((uint32_t)size);
    GTS_INTERNAL_ASSERT(bin < m_numBins || bin == BIN_IDX_OVERSIZED);

    if(bin == BIN_IDX_OVERSIZED)
    {
        SlabHeader* pSlab = m_pMemoryStore->allocateSlab(
            alignUpTo(size + MemoryStore::SINGLE_PAGE_HEADER_SIZE, GTS_GET_OS_PAGE_SIZE()));

        GTS_INTERNAL_ASSERT(((uintptr_t)pSlab & (MemoryStore::SLAB_SIZE - 1)) == 0);

        size_t blockSize      = alignUpTo(size, GTS_GET_OS_PAGE_SIZE());
        PageHeader* pPage     = m_pMemoryStore->allocatePage(pSlab, blockSize);
        void* pBlock          = pPage->pLocalFreeList;
        pPage->pLocalFreeList = nullptr;
        pPage->numUsedBlocks.fetch_add(1, memory_order::release);

        GTS_INTERNAL_ASSERT(pSlab == MemoryStore::toSlab(pBlock));
        GTS_INTERNAL_ASSERT(pPage == MemoryStore::toPage(pSlab, pBlock));

        return pBlock;
    }
    else
    {
        return m_allocatorsByBin[bin].allocate();
    }
}

//------------------------------------------------------------------------------
void BinnedAllocator::deallocate(void* ptr)
{
    if (ptr == nullptr)
    {
        return;
    }

    uint32_t bin = calculateBin(ptr);
    GTS_INTERNAL_ASSERT(bin < m_numBins || bin == BIN_IDX_OVERSIZED);

    if(bin == BIN_IDX_OVERSIZED)
    {
        SlabHeader* pSlab = MemoryStore::toSlab(ptr);
        PageHeader* pPage = MemoryStore::toPage(pSlab, ptr);
        pPage->numUsedBlocks.store(0, memory_order::relaxed);

        if (pSlab->pageSize < MemoryStore::SLAB_SIZE - MemoryStore::SINGLE_PAGE_HEADER_SIZE)
        {
            m_pMemoryStore->deallocatePage(pPage);
        }
        m_pMemoryStore->deallocateSlab(pSlab, false);
    }
    else
    {
        m_allocatorsByBin[bin].deallocate(ptr);
    }
}

//------------------------------------------------------------------------------
BinnedAllocator::AllocatorVector const& BinnedAllocator::getAllAllocators() const
{
    return m_allocatorsByBin;
}

//------------------------------------------------------------------------------
PageHeader* BinnedAllocator::_allocatePage(size_t pageSize, size_t blockSize)
{
    PageHeader* pPage = nullptr;

    SlabHeader* pActiveSlab = m_activeSlabs[MemoryStore::pageSizeToIndex(pageSize)];

    if (pActiveSlab)
    {
        pPage = m_pMemoryStore->allocatePage(pActiveSlab, blockSize);
    }

    if (!pPage)
    {
        pPage = _walkSlabListForFreePages(pageSize, blockSize);

        if (!pPage)
        {
            pPage = _allocatePageFromNewSlab(pageSize, blockSize);

            if (!pPage)
            {
                GTS_INTERNAL_ASSERT(0 && "Out of memory!");
                return nullptr;
            }
        }
    }

    GTS_INTERNAL_ASSERT(pPage->pLocalFreeList);
    return pPage;
}

//------------------------------------------------------------------------------
PageHeader* BinnedAllocator::_walkSlabListForFreePages(size_t pageSize, size_t blockSize)
{
    GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::BINNED_ALLOCATOR_DEBUG, analysis::Color::AntiqueWhite, "BLOCKALLOC SLAB WALK", this, 0);

    PageHeader* pPage = nullptr;
    SlabHeader* pVisited = nullptr;
    SlabHeader* pSlab = (SlabHeader*)m_slabLists[MemoryStore::pageSizeToIndex(pageSize)].popFront();

    while (pSlab)
    {
        pSlab->pNext = pVisited;
        pVisited = pSlab;
        pPage = _getPageFromSlab(pageSize, blockSize, pSlab);

        if (pPage)
        {
            break;
        }

        pSlab = (SlabHeader*)pSlab->pNext;
    }

    // Put any visited pages back
    while (pVisited != nullptr)
    {
        m_slabLists[MemoryStore::pageSizeToIndex(pageSize)].pushBack(pVisited);
        pVisited = (SlabHeader*)pVisited->pNext;
    }

    return pPage;
}

//------------------------------------------------------------------------------
PageHeader* BinnedAllocator::_getPageFromSlab(size_t pageSize, size_t blockSize, SlabHeader* pSlab)
{
    PageHeader* pPage = nullptr;
    uint32_t numBlocks = uint32_t(pageSize / blockSize);

    //// If fully committed, check for partially allocated Pages.
    //if (pSlab->numCommittedPages == pSlab->numPages)
    //{
        // Walk pages for free blocks.
        for (uint32_t iPage = 0, len = pSlab->numPages; iPage < len; ++iPage)
        {
            pPage = pSlab->pPageHeaders + iPage;
            
            // TODO: bin pages by blockSize.
            if (pPage->blockSize == blockSize &&
                pPage->numUsedBlocks.load(memory_order::acquire) < numBlocks)
            {
                m_activeSlabs[MemoryStore::pageSizeToIndex(pageSize)] = pSlab;
                if(pPage->reclaimNonLocalBlocks())
                {
                    // Done.
                    GTS_INTERNAL_ASSERT(pPage->pLocalFreeList);
                    return pPage;
                }
            }
        }
    //}

    // Allocate a new page.
    m_activeSlabs[MemoryStore::pageSizeToIndex(pageSize)] = pSlab;
    pPage = m_pMemoryStore->allocatePage(pSlab, blockSize);
    return pPage;
}

//------------------------------------------------------------------------------
PageHeader* BinnedAllocator::_allocatePageFromNewSlab(size_t pageSize, size_t blockSize)
{
    GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::BINNED_ALLOCATOR_DEBUG, analysis::Color::AntiqueWhite, "BLOCKALLOC ALLOC NEW SLAB", this, 0);

    PageHeader* pPage = nullptr;

    while(true)
    {
        // Get a new Slab.
        SlabHeader* pSlab = m_pMemoryStore->allocateSlab(pageSize);

        // Failed allocation.
        if (pSlab == nullptr)
        {
            GTS_INTERNAL_ASSERT(0);
            return nullptr;
        }

        size_t index              = MemoryStore::pageSizeToIndex(pageSize);
        size_type& slabCount      = m_slabCounts[index];
        internal::DList& slabList = m_slabLists[index];

        GTS_INTERNAL_ASSERT(slabCount == slabList.size());

        GTS_INTERNAL_ASSERT(pSlab->tid = ThisThread::getId());
        slabList.pushFront(pSlab);
        ++slabCount;

        GTS_INTERNAL_ASSERT(slabCount == slabList.size());

        pPage = _getPageFromSlab(pageSize, blockSize, pSlab);

        if (pPage)
        {
            return pPage;
        }
    }
}

//------------------------------------------------------------------------------
void BinnedAllocator::_deallocatePage(SlabHeader* pSlab, PageHeader* pPage)
{
    GTS_INTERNAL_ASSERT(ThisThread::getId() == pPage->tid && pPage->numUsedBlocks.load(memory_order::relaxed) == 0);

    m_pMemoryStore->deallocatePage(pPage);

    // Free the slab if its not in use.
    if (pSlab->myAllocator && pSlab->numCommittedPages == 0)
    {
        _tryFreeSlab(pSlab);
    }
}

//------------------------------------------------------------------------------
void BinnedAllocator::_tryFreeSlab(SlabHeader* pSlab)
{
    GTS_INTERNAL_ASSERT(pSlab->tid == ThisThread::getId());

    // Don't free if this Slab is active or in use.
    if (m_activeSlabs[MemoryStore::pageSizeToIndex(pSlab->pageSize)] == pSlab ||
        pSlab->numCommittedPages != 0)
    {
        return;
    }

    size_t index              = MemoryStore::pageSizeToIndex(pSlab->pageSize);
    size_type& slabCount      = m_slabCounts[index];
    internal::DList& slabList = m_slabLists[index];

    GTS_INTERNAL_ASSERT(slabCount != 0);
    GTS_INTERNAL_ASSERT(slabCount == slabList.size());

    // Unlink the header so that it is inaccessible.
    slabList.remove(pSlab);
    --slabCount;

    GTS_INTERNAL_ASSERT(slabCount == slabList.size());

    // Free the Slab.
    pSlab->myAllocator = nullptr;
    m_pMemoryStore->deallocateSlab(pSlab, true);
}

} // namespace gts
