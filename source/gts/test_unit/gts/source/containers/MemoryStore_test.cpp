/*******************************************************************************
* Copyright 2019 Intel Corporation
*
* Permission is hereby granted, deallocate of charge, to any person obtaining a copy
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
#include <vector>
#include <thread>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "SchedulerTestsCommon.h"
#include "testers/TestersCommon.h"
#include "gts/containers/parallel/BinnedAllocator.h"

using namespace gts;

namespace testing {

//------------------------------------------------------------------------------
uint16_t calcNumPages(uint32_t slabSize, uint32_t pageSize)
{
    uint32_t slabHeaderSize   = (uint32_t)sizeof(SlabHeader);
    uint32_t numPages         = (slabSize - slabHeaderSize) / pageSize;
    uint32_t pageHeadersSize  = sizeof(PageHeader) * numPages;
    uint32_t totalHeaderSize  = (uint32_t)alignUpTo(slabHeaderSize + pageHeadersSize, GTS_GET_OS_PAGE_SIZE());
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

        GTS_ASSERT(numPages != 0u);
    }

    GTS_ASSERT(numPages != 0u);
    return uint16_t(numPages);
}

//------------------------------------------------------------------------------
void allocFreeSlabTest(uint32_t expectedSlabSize, uint32_t pageSize)
{
    const uint32_t slabHeaderSize    = (uint32_t)sizeof(SlabHeader);
    const uint32_t pageHeaderSize    = (uint32_t)sizeof(PageHeader);
    const uint16_t expectedPageCount = calcNumPages(expectedSlabSize, pageSize);

    MemoryStore memStore;
    SlabHeader* pSlab = memStore.allocateSlab(pageSize);
    ASSERT_TRUE(pSlab != nullptr);

    // alignment check
    ASSERT_EQ((uintptr_t)pSlab % MemoryStore::SLAB_SIZE, size_t(0));

    // Verify page headers and count.
    ASSERT_NE(pSlab->pLocalPageFreeList, nullptr);
    uint32_t pageCount = 0;
    FreeListNode* pNode = pSlab->pLocalPageFreeList;
    uint32_t iNode = 0;
    while (pNode)
    {
        PageHeader* pPage = (PageHeader*)pNode;
        ASSERT_EQ(pPage->pLocalFreeList, nullptr);
        ASSERT_EQ(pPage->pNonLocalFreeList.load(memory_order::relaxed), nullptr);
        ASSERT_EQ(pPage->pBlocks, (uint8_t*)pSlab->pBlocks + pageSize * (expectedPageCount - iNode - 1));
        ASSERT_EQ(pPage->blockSize, 0u);
        ASSERT_EQ(pPage->tid, ThisThread::getId());
        ASSERT_EQ(pPage->numUsedBlocks.load(memory_order::relaxed), 0u);
        ASSERT_EQ(pPage->hasAlignedBlocks, false);

        iNode++;
        pageCount++;
        pNode = pNode->pNextFree.load(memory_order::relaxed);
    }
    ASSERT_EQ(pageCount, expectedPageCount);

    // Verify slab header.
    ASSERT_EQ(pSlab->pNonLocalPageFreeList.load(memory_order::relaxed), nullptr);
    ASSERT_EQ(pSlab->pPageHeaders, (PageHeader*)((uint8_t*)pSlab + slabHeaderSize));
    ASSERT_EQ(pSlab->pBlocks, (uint8_t*)pSlab + alignUpTo(slabHeaderSize + pageHeaderSize* pSlab->numPages, GTS_GET_OS_PAGE_SIZE()));
    ASSERT_EQ(pSlab->tid, ThisThread::getId());
    ASSERT_EQ(pSlab->slabSize, expectedSlabSize);
    ASSERT_EQ(pSlab->pageSize, pageSize);
    ASSERT_EQ(pSlab->numPages, expectedPageCount);
    ASSERT_EQ(pSlab->numCommittedPages, 0u);
    ASSERT_EQ(pSlab->state, MemoryState::STATE_FREE);

    memStore.deallocateSlab(pSlab, false);
}

//------------------------------------------------------------------------------
TEST(MemoryStoreTest, allocateFreeSlab)
{
    allocFreeSlabTest(MemoryStore::SLAB_SIZE, MemoryStore::PAGE_SIZE_CLASS_0);
    allocFreeSlabTest(MemoryStore::SLAB_SIZE, MemoryStore::PAGE_SIZE_CLASS_1);
    allocFreeSlabTest(MemoryStore::SLAB_SIZE, MemoryStore::PAGE_SIZE_CLASS_2);

#if GTS_64BIT
    allocFreeSlabTest(MemoryStore::SLAB_SIZE, MemoryStore::PAGE_SIZE_CLASS_3);
#endif

    uint32_t pageSize = (uint32_t)alignUpTo(MemoryStore::PAGE_SIZE_CLASS_3 + 1, GTS_GET_OS_PAGE_SIZE());
    allocFreeSlabTest(MemoryStore::SLAB_SIZE, pageSize);

    pageSize = (uint32_t)alignDownTo(MemoryStore::SLAB_SIZE - MemoryStore::SINGLE_PAGE_HEADER_SIZE, GTS_GET_OS_PAGE_SIZE());
    allocFreeSlabTest(MemoryStore::SLAB_SIZE, pageSize);
}

//------------------------------------------------------------------------------
TEST(MemoryStoreTest, allocateFreeLargePageSlab)
{
    uint32_t pageSize = MemoryStore::SLAB_SIZE;
    uint32_t slabSize = (uint32_t)alignUpTo(pageSize + MemoryStore::SINGLE_PAGE_HEADER_SIZE, GTS_GET_OS_ALLOCATION_GRULARITY());
    allocFreeSlabTest(slabSize, pageSize);
}

//------------------------------------------------------------------------------
void allocateFreePageTest(uint32_t pageSize, uint32_t blockSize)
{
    MemoryStore memStore;
    SlabHeader* pSlab = memStore.allocateSlab(pageSize);

    PageHeader* pPage = memStore.allocatePage(pSlab, blockSize);
    ASSERT_TRUE(pPage != nullptr);

    const uint32_t expectedBlockCount = pSlab->pageSize / pPage->blockSize;

    // Verify nodes and count.
    ASSERT_NE(pPage->pLocalFreeList, nullptr);
    uint32_t blockCount = 0;
    FreeListNode* pNode = pPage->pLocalFreeList;
    uint32_t iBlock = 0;
    while (pNode)
    {
        ASSERT_EQ((void*)pNode, (uint8_t*)pPage->pBlocks + blockSize * (expectedBlockCount - iBlock - 1));

        iBlock++;
        blockCount++;
        pNode = pNode->pNextFree.load(memory_order::relaxed);
    }
    ASSERT_EQ(blockCount, expectedBlockCount);

    // Verify page header.
    ASSERT_EQ(pPage->pNonLocalFreeList.load(memory_order::relaxed), nullptr);
    ASSERT_EQ(pPage->tid, ThisThread::getId());
    ASSERT_EQ(pPage->numUsedBlocks.load(memory_order::relaxed), 0u);
    ASSERT_EQ(pPage->hasAlignedBlocks, false);
    ASSERT_EQ(pPage->blockSize, blockSize);

    memStore.deallocatePage(pPage);
    memStore.deallocateSlab(pSlab, false);
}

//------------------------------------------------------------------------------
TEST(MemoryStoreTest, allocateFreePage)
{
    allocateFreePageTest(MemoryStore::PAGE_SIZE_CLASS_0, GTS_MALLOC_ALIGNEMNT);
    allocateFreePageTest(MemoryStore::PAGE_SIZE_CLASS_1, GTS_MALLOC_ALIGNEMNT);
    allocateFreePageTest(MemoryStore::PAGE_SIZE_CLASS_2, GTS_MALLOC_ALIGNEMNT);
#if GTS_64BIT
    allocFreeSlabTest(MemoryStore::SLAB_SIZE, MemoryStore::PAGE_SIZE_CLASS_3);
#endif

    uint32_t pageSize = (uint32_t)alignUpTo(MemoryStore::PAGE_SIZE_CLASS_3 + 1, GTS_GET_OS_PAGE_SIZE());
    allocateFreePageTest(pageSize, GTS_MALLOC_ALIGNEMNT);

    pageSize = (uint32_t)alignDownTo(MemoryStore::SLAB_SIZE - MemoryStore::SINGLE_PAGE_HEADER_SIZE, GTS_GET_OS_PAGE_SIZE());
    allocateFreePageTest(pageSize, GTS_MALLOC_ALIGNEMNT);
}

//------------------------------------------------------------------------------
void zeroAllBlocks(std::vector<void*>& blocks, uint32_t blockSize)
{
    for (auto pBlock : blocks)
    {
        for (uint32_t ii = 0; ii < blockSize; ++ii)
        {
            ((uint8_t*)pBlock)[ii] = 0;
        }
    }
}

//------------------------------------------------------------------------------
void incrementAllBlocks(std::vector<void*>& blocks, uint32_t blockSize)
{
    for (auto pBlock : blocks)
    {
        for (uint32_t ii = 0; ii < blockSize; ++ii)
        {
            ((uint8_t*)pBlock)[ii]++;
        }
    }
}

//------------------------------------------------------------------------------
void verifyAllBlocks(std::vector<void*>& blocks, uint32_t blockSize, uint8_t value)
{
    for (auto pBlock : blocks)
    {
        for (uint32_t ii = 0; ii < blockSize; ++ii)
        {
            ASSERT_EQ(((uint8_t*)(pBlock))[ii], value);
        }
    }
}

//------------------------------------------------------------------------------
void allocateAllPagesTest(uint32_t pageSize, uint32_t blockSize)
{
    MemoryStore memStore;
    SlabHeader* pSlab = memStore.allocateSlab(pageSize);

    std::vector<PageHeader*> pages;
    std::vector<std::vector<void*>> blocks(pSlab->numPages);

    // Increment each byte in all blocks.
    for (uint32_t ii = 0; ii < pSlab->numPages; ++ii)
    {
        PageHeader* pPage = memStore.allocatePage(pSlab, blockSize);
        ASSERT_TRUE(pPage != nullptr);
        pages.push_back(pPage);

        FreeListNode* pNode = pPage->pLocalFreeList;
        while (pNode)
        {
            blocks[ii].push_back(pNode);
            pNode = pNode->pNextFree.load(memory_order::relaxed);
        }

        zeroAllBlocks(blocks[ii], pPage->blockSize);
        incrementAllBlocks(blocks[ii], pPage->blockSize);
    }

    // Verify each byte has only been incremented once.
    for (uint32_t ii = 0; ii < pSlab->numPages; ++ii)
    {
        verifyAllBlocks(blocks[ii], blockSize, 1);
    }

    for (uint32_t ii = 0; ii < pSlab->numPages; ++ii)
    {
        memStore.deallocatePage(pages[ii]);
    }

    memStore.deallocateSlab(pSlab, false);
}

//------------------------------------------------------------------------------
TEST(MemoryStoreTest, allocateAllPages)
{
    allocateAllPagesTest(MemoryStore::PAGE_SIZE_CLASS_0, GTS_MALLOC_ALIGNEMNT);
    allocateAllPagesTest(MemoryStore::PAGE_SIZE_CLASS_1, GTS_MALLOC_ALIGNEMNT);
    allocateAllPagesTest(MemoryStore::PAGE_SIZE_CLASS_2, GTS_MALLOC_ALIGNEMNT);
#if GTS_64BIT
    allocFreeSlabTest(MemoryStore::SLAB_SIZE, MemoryStore::PAGE_SIZE_CLASS_3);
#endif

    uint32_t pageSize = (uint32_t)alignUpTo(MemoryStore::PAGE_SIZE_CLASS_3 + 1, GTS_GET_OS_PAGE_SIZE());
    allocateAllPagesTest(pageSize, GTS_MALLOC_ALIGNEMNT);

    pageSize = (uint32_t)alignDownTo(MemoryStore::SLAB_SIZE - MemoryStore::SINGLE_PAGE_HEADER_SIZE, GTS_GET_OS_PAGE_SIZE());
    allocateAllPagesTest(pageSize, GTS_MALLOC_ALIGNEMNT);

    allocateAllPagesTest(MemoryStore::SLAB_SIZE, GTS_MALLOC_ALIGNEMNT);
}

//------------------------------------------------------------------------------
void parallelAllocateDeallocate(uint32_t pageSize, uint32_t numAllocations)
{
    const uint32_t threadCount = Thread::getHardwareThreadCount() - 1;

    MemoryStore memStore;

    gts::Atomic<bool> startTest(false);

    // Contend for slabs through OS virtual allocation
    std::vector<std::thread*> threads;
    threads.resize(threadCount);
    for (uint32_t tt = 0; tt < threadCount; ++tt)
    {
        threads[tt] = new std::thread([numAllocations, pageSize, &memStore, &startTest]()
        {
            while (!startTest.load(memory_order::acquire))
            {
                GTS_PAUSE();
            }

            for (uint32_t ii = 0; ii < numAllocations; ++ii)
            {
                SlabHeader* pSlab = memStore.allocateSlab(pageSize);
                ASSERT_TRUE(pSlab != nullptr);
                memStore.deallocateSlab(pSlab, true);
            }
        });
    }

    startTest.store(true, memory_order::release);

    // This thread works too to prevent over subscription.
    for (uint32_t ii = 0; ii < numAllocations; ++ii)
    {
        SlabHeader* pSlab = memStore.allocateSlab(pageSize);
        ASSERT_TRUE(pSlab != nullptr);
        memStore.deallocateSlab(pSlab, true);
    }

    for (uint32_t tt = 0; tt < threadCount; ++tt)
    {
        threads[tt]->join();
        delete threads[tt];
    }
}

//------------------------------------------------------------------------------
TEST(MemoryStoreTest, allocateDeallocate_ManyThreads)
{
    parallelAllocateDeallocate(MemoryStore::PAGE_SIZE_CLASS_0, ITEM_COUNT);
    parallelAllocateDeallocate(MemoryStore::PAGE_SIZE_CLASS_1, ITEM_COUNT);
    parallelAllocateDeallocate(MemoryStore::PAGE_SIZE_CLASS_2, ITEM_COUNT);
#if GTS_64BIT
    allocFreeSlabTest(MemoryStore::SLAB_SIZE, MemoryStore::PAGE_SIZE_CLASS_3);
#endif

    uint32_t pageSize = (uint32_t)alignUpTo(MemoryStore::PAGE_SIZE_CLASS_3 + 1, GTS_GET_OS_PAGE_SIZE());
    parallelAllocateDeallocate(pageSize, ITEM_COUNT);

    pageSize = (uint32_t)alignDownTo(MemoryStore::SLAB_SIZE - MemoryStore::SINGLE_PAGE_HEADER_SIZE, GTS_GET_OS_PAGE_SIZE());
    parallelAllocateDeallocate(pageSize, 3);
}

//#ifdef GTS_ARCH_X64

//------------------------------------------------------------------------------
TEST(MemoryStoreTest, allocateDeallocateLarge_ManyThreads)
{
    parallelAllocateDeallocate(MemoryStore::SLAB_SIZE, 3);
}

//#endif

} // namespace testing
