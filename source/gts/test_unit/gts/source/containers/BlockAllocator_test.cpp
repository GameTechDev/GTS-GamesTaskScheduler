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
#include "gts/containers/parallel/QueueSPMC.h"

using namespace gts;

namespace testing {

//------------------------------------------------------------------------------
TEST(BlockAllocatorTest, initAndShutdown)
{
    const uint32_t blockSize = GTS_MALLOC_ALIGNEMNT;
    const uint32_t pageSize  = MemoryStore::PAGE_SIZE_CLASS_0;

    MemoryStore memStore;
    BinnedAllocator binnedAlloc;
    binnedAlloc.init(&memStore);
    gts::internal::BlockAllocator blockAlloc;
    blockAlloc.init(&binnedAlloc, blockSize, pageSize);

    ASSERT_EQ(blockAlloc.blockSize(), blockSize);
    ASSERT_EQ(blockAlloc.pageSize(), pageSize);

    blockAlloc.shutdown();
}

//------------------------------------------------------------------------------
TEST(BlockAllocatorTest, badInit)
{
    const uint32_t goodBlockSize = GTS_MALLOC_ALIGNEMNT;
    const uint32_t goodPageSize  = MemoryStore::PAGE_SIZE_CLASS_0;

    MemoryStore memStore;
    BinnedAllocator binnedAlloc;
    binnedAlloc.init(&memStore);
    gts::internal::BlockAllocator blockAlloc;

    EXPECT_DEATH({
        blockAlloc.init(nullptr, GTS_MALLOC_ALIGNEMNT - 1, goodPageSize);
    }, "");

    EXPECT_DEATH({
        blockAlloc.init(&binnedAlloc, GTS_MALLOC_ALIGNEMNT - 1, goodPageSize);
    }, "");

    EXPECT_DEATH({
        blockAlloc.init(&binnedAlloc, GTS_MALLOC_ALIGNEMNT + 1, goodPageSize);
    }, "");

    EXPECT_DEATH({
        blockAlloc.init(&binnedAlloc, goodBlockSize, goodPageSize + 1);
    }, "");

    EXPECT_DEATH({
        blockAlloc.init(&binnedAlloc, goodPageSize + 1, goodPageSize);
    }, "");
}

//------------------------------------------------------------------------------
void allocateFreeTest(uint32_t pageSize, uint32_t blockSize)
{
    MemoryStore memStore;
    BinnedAllocator binnedAlloc;
    binnedAlloc.init(&memStore);
    gts::internal::BlockAllocator blockAlloc;
    blockAlloc.init(&binnedAlloc, blockSize, pageSize);

    std::vector<void*> allocations;

    void* ptr = blockAlloc.allocate();
    allocations.push_back(ptr);

    SlabHeader* pSlab = MemoryStore::toSlab(ptr);
    PageHeader* pPage = MemoryStore::toPage(pSlab, ptr);

    uint32_t numBlocks = pSlab->pageSize / pPage->blockSize;

    while (pPage->numUsedBlocks.load(memory_order::relaxed) < numBlocks)
    {
        ptr = blockAlloc.allocate();
        allocations.push_back(ptr);
        // Verify all allocations came from this page.
        ASSERT_EQ(pPage, MemoryStore::toPage(pSlab, ptr));
    }

    for (size_t ii = 0; ii < allocations.size(); ++ii)
    {
        blockAlloc.deallocate(allocations[ii]);
    }

    ASSERT_EQ(pPage->numUsedBlocks.load(memory_order::relaxed), 0u);

    blockAlloc.shutdown();
}

//------------------------------------------------------------------------------
TEST(BlockAllocatorTest, allocateFree)
{
    allocateFreeTest(MemoryStore::PAGE_SIZE_CLASS_0, GTS_MALLOC_ALIGNEMNT);
    allocateFreeTest(MemoryStore::PAGE_SIZE_CLASS_0, MemoryStore::PAGE_SIZE_CLASS_0);
    allocateFreeTest(MemoryStore::PAGE_SIZE_CLASS_1, GTS_MALLOC_ALIGNEMNT);
    allocateFreeTest(MemoryStore::PAGE_SIZE_CLASS_1, MemoryStore::PAGE_SIZE_CLASS_1);
    allocateFreeTest(MemoryStore::PAGE_SIZE_CLASS_2, GTS_MALLOC_ALIGNEMNT);
    allocateFreeTest(MemoryStore::PAGE_SIZE_CLASS_2, MemoryStore::PAGE_SIZE_CLASS_2);
#if GTS_64BIT
    allocateFreeTest(MemoryStore::PAGE_SIZE_CLASS_3, GTS_MALLOC_ALIGNEMNT);
    allocateFreeTest(MemoryStore::PAGE_SIZE_CLASS_3, MemoryStore::PAGE_SIZE_CLASS_3);
#endif
}

//------------------------------------------------------------------------------
void allocateNonLocalBlockFreeTest(uint32_t pageSize, uint32_t blockSize)
{
    MemoryStore memStore;
    BinnedAllocator binnedAlloc;
    binnedAlloc.init(&memStore);
    gts::internal::BlockAllocator blockAlloc;
    blockAlloc.init(&binnedAlloc, blockSize, pageSize);

    std::vector<void*> allocations;

    void* ptr = blockAlloc.allocate();
    allocations.push_back(ptr);

    SlabHeader* pSlab = MemoryStore::toSlab(ptr);
    PageHeader* pPage = MemoryStore::toPage(pSlab, ptr);

    uint32_t numBlocks = pSlab->pageSize / pPage->blockSize;

    while (pPage->numUsedBlocks.load(memory_order::relaxed) < numBlocks)
    {
        ptr = blockAlloc.allocate();
        allocations.push_back(ptr);
        // Verify all allocations came from this page.
        ASSERT_EQ(pPage, MemoryStore::toPage(pSlab, ptr));
    }

    std::atomic<bool> finshedDealloc = { false };

    // Deallocate on a different thread.
    std::thread deallocThread([&]
    {
        for (size_t ii = 0; ii < allocations.size(); ++ii)
        {
            blockAlloc.deallocate(allocations[ii]);
        }

        finshedDealloc.store(true, std::memory_order_release);
    });

    // Block until finished.
    while(!finshedDealloc.load(std::memory_order_acquire))
    {
        GTS_PAUSE();
    }

    deallocThread.join();

    // Expect all allocations to be reallocations of the freed memory.
    for (uint32_t iBlock = 0; iBlock < allocations.size(); ++iBlock)
    {
        ptr = blockAlloc.allocate();
        void* p = allocations[allocations.size() - iBlock - 1];
        ASSERT_EQ(ptr, p);
    }

    // Deallocate all.
    for (uint32_t iBlock = 0; iBlock < allocations.size(); ++iBlock)
    {
        blockAlloc.deallocate(allocations[iBlock]);
    }

    ASSERT_EQ(pPage->numUsedBlocks.load(memory_order::relaxed), 0u);

    blockAlloc.shutdown();
}

//------------------------------------------------------------------------------
TEST(BlockAllocatorTest, allocateNonLocalBlockFree)
{
    allocateNonLocalBlockFreeTest(MemoryStore::PAGE_SIZE_CLASS_0, GTS_MALLOC_ALIGNEMNT);
    allocateNonLocalBlockFreeTest(MemoryStore::PAGE_SIZE_CLASS_0, MemoryStore::PAGE_SIZE_CLASS_0);
    allocateNonLocalBlockFreeTest(MemoryStore::PAGE_SIZE_CLASS_1, GTS_MALLOC_ALIGNEMNT);
    allocateNonLocalBlockFreeTest(MemoryStore::PAGE_SIZE_CLASS_1, MemoryStore::PAGE_SIZE_CLASS_1);
    allocateNonLocalBlockFreeTest(MemoryStore::PAGE_SIZE_CLASS_2, GTS_MALLOC_ALIGNEMNT);
    allocateNonLocalBlockFreeTest(MemoryStore::PAGE_SIZE_CLASS_2, MemoryStore::PAGE_SIZE_CLASS_2);
#if GTS_64BIT
    allocateNonLocalBlockFreeTest(MemoryStore::PAGE_SIZE_CLASS_3, GTS_MALLOC_ALIGNEMNT);
    allocateNonLocalBlockFreeTest(MemoryStore::PAGE_SIZE_CLASS_3, MemoryStore::PAGE_SIZE_CLASS_3);
#endif
}

//------------------------------------------------------------------------------
TEST(BlockAllocatorTest, blockLeak)
{
    const uint32_t blockSize = GTS_MALLOC_ALIGNEMNT;
    const uint32_t pageSize  = MemoryStore::PAGE_SIZE_CLASS_0;

    EXPECT_DEATH({
        MemoryStore memStore;
        BinnedAllocator binnedAlloc;
        binnedAlloc.init(&memStore);
        gts::internal::BlockAllocator blockAlloc;
        blockAlloc.init(&binnedAlloc, blockSize, pageSize);
        blockAlloc.allocate();
        blockAlloc.shutdown();
    },"");
}

////------------------------------------------------------------------------------
//TEST(BlockAllocatorTest, allocatePageWalk)
//{
//    const uint32_t blockSize = GTS_MALLOC_ALIGNEMNT;
//    const uint32_t pageSize  = MemoryStore::PAGE_SIZE_CLASS_0;
//
//    MemoryStore memStore;
//    gts::internal::BlockAllocator blockAlloc;
//    blockAlloc.init(&memStore, blockSize, pageSize);
//
//    constexpr uint32_t NUM_PAGES = 3;
//    std::vector<void*> allocations[3];
//
//    // Allocated three pages.
//    for (uint32_t iPage = 0; iPage < NUM_PAGES; ++iPage)
//    {
//        void* ptr = blockAlloc.allocate();
//        allocations[iPage].push_back(ptr);
//
//        SlabHeader* pSlab = MemoryStore::toSlab(ptr);
//        PageHeader* pPage = MemoryStore::toPage(pSlab, ptr);
//
//        uint32_t numBlocks = pSlab->pageSize / pSlab->blockSize;
//
//        while (pPage->numUsedBlocks.load(memory_order::relaxed) < numBlocks)
//        {
//            ptr = blockAlloc.allocate();
//            allocations[iPage].push_back(ptr);
//            // Verify all allocations came from this page.
//            ASSERT_EQ(pPage, MemoryStore::toPage(pSlab, ptr));
//        }
//    }
//
//    // Deallocate some blocks from the second page.
//    {
//        auto& blocks = allocations[1];
//        for (uint32_t iBlock = 0; iBlock < blocks.size(); iBlock += 2)
//        {
//            blockAlloc.deallocate(blocks[iBlock]);
//        }
//    }
//
//    // Allocate blocks. Verify they are the blocks we just deallocated.
//    {
//        auto& blocks = allocations[1];
//        size_t size = (blocks.size() & 1) == 0 ? blocks.size() : blocks.size() + 1;
//        for (uint32_t iBlock = 0; iBlock < size; iBlock += 2)
//        {
//            void* ptr = blockAlloc.allocate();
//            void* p = blocks[size - iBlock - 2];
//            ASSERT_EQ(ptr, p);
//        }
//    }
//
//    // Deallocate all.
//    for (uint32_t iPage = 0; iPage < NUM_PAGES; ++iPage)
//    {
//        auto& blocks = allocations[iPage];
//        for (uint32_t iBlock = 0; iBlock < blocks.size(); ++iBlock)
//        {
//            blockAlloc.deallocate(blocks[iBlock]);
//        }
//    }
//
//    blockAlloc.shutdown();
//}
//
////------------------------------------------------------------------------------
//TEST(BlockAllocatorTest, allocateNonLocaPageFree)
//{
//    const uint32_t blockSize = GTS_MALLOC_ALIGNEMNT;
//    const uint32_t pageSize  = MemoryStore::PAGE_SIZE_CLASS_0;
//
//    MemoryStore memStore;
//    gts::internal::BlockAllocator blockAlloc;
//    blockAlloc.init(&memStore, blockSize, pageSize);
//
//    constexpr uint32_t NUM_PAGES = 3;
//    std::vector<void*> allocations[3];
//
//    // Allocated three pages.
//    for (uint32_t iPage = 0; iPage < NUM_PAGES; ++iPage)
//    {
//        void* ptr = blockAlloc.allocate();
//        allocations[iPage].push_back(ptr);
//
//        SlabHeader* pSlab = MemoryStore::toSlab(ptr);
//        PageHeader* pPage = MemoryStore::toPage(pSlab, ptr);
//
//        uint32_t numBlocks = pSlab->pageSize / pSlab->blockSize;
//
//        while (pPage->numUsedBlocks.load(memory_order::relaxed) < numBlocks)
//        {
//            ptr = blockAlloc.allocate();
//            allocations[iPage].push_back(ptr);
//            // Verify all allocations came from this page.
//            ASSERT_EQ(pPage, MemoryStore::toPage(pSlab, ptr));
//        }
//    }
//
//    // Deallocate some blocks from the second page.
//    std::thread thread([&allocations, &blockAlloc]()
//    {
//        auto& blocks = allocations[1];
//        for (uint32_t iBlock = 0; iBlock < blocks.size(); iBlock += 2)
//        {
//            blockAlloc.deallocate(blocks[iBlock]);
//        }
//    });
//
//    thread.join();
//
//    // Allocate blocks. Verify they are the blocks we just deallocated.
//    {
//        auto& blocks = allocations[1];
//        size_t size = (blocks.size() & 1) == 0 ? blocks.size() : blocks.size() + 1;
//        for (uint32_t iBlock = 0; iBlock < size; iBlock += 2)
//        {
//            void* ptr = blockAlloc.allocate();
//            void* p = blocks[size - iBlock - 2];
//            ASSERT_EQ(ptr, p);
//        }
//    }
//
//    // Deallocate all.
//    for (uint32_t iPage = 0; iPage < NUM_PAGES; ++iPage)
//    {
//        auto& blocks = allocations[iPage];
//        for (uint32_t iBlock = 0; iBlock < blocks.size(); ++iBlock)
//        {
//            blockAlloc.deallocate(blocks[iBlock]);
//        }
//    }
//
//    blockAlloc.shutdown();
//}
//
////------------------------------------------------------------------------------
//TEST(BlockAllocatorTest, allocateSlabWalk)
//{
//    const uint32_t blockSize = GTS_MALLOC_ALIGNEMNT;
//    const uint32_t pageSize  = MemoryStore::PAGE_SIZE_CLASS_0;
//
//    MemoryStore memStore;
//    gts::internal::BlockAllocator blockAlloc;
//    blockAlloc.init(&memStore, blockSize, pageSize);
//
//    constexpr uint32_t NUM_SLABS = 3;
//    std::vector<SlabHeader*> slabs(NUM_SLABS);
//    std::vector<std::vector<void*>> blockByPage[NUM_SLABS];
//
//    void* ptr = blockAlloc.allocate();
//
//    SlabHeader* pSlab = MemoryStore::toSlab(ptr);
//    uint32_t pagesPerSlab = pSlab->numPages;
//
//    PageHeader* pPage = MemoryStore::toPage(pSlab, ptr);
//    uint32_t blocksPerPage = pSlab->pageSize / pSlab->blockSize;
//
//    blockAlloc.deallocate(ptr);
//
//    // Fully allocate several slabs.
//    for (uint32_t iSlab = 0; iSlab < NUM_SLABS; ++iSlab)
//    {
//        ptr = blockAlloc.allocate();
//        pSlab = MemoryStore::toSlab(ptr);
//        pPage = MemoryStore::toPage(pSlab, ptr);
//        slabs[iSlab] = pSlab;
//        blockAlloc.deallocate(ptr);
//
//        for (uint32_t iPage = 0; iPage < pagesPerSlab; ++iPage)
//        {
//            blockByPage[iSlab].push_back(std::vector<void*>());
//
//            for (uint32_t iBlock = 0; iBlock < blocksPerPage; ++iBlock)
//            {
//                ptr = blockAlloc.allocate();
//                blockByPage[iSlab][iPage].push_back(ptr);
//            }
//        }
//    }
//
//    // Deallocate some pages from the second slab.
//    std::vector<std::vector<void*>>& slab = blockByPage[1];
//    for (uint32_t iPage = 0; iPage < pagesPerSlab; iPage += 2)
//    {
//        for (uint32_t iBlock = 0; iBlock < blocksPerPage; ++iBlock)
//        {
//            blockAlloc.deallocate(slab[iPage][iBlock]);
//        }
//    }
//
//    // Reallocate deallocated pages. Expect all allocations come from previously
//    // freed pages in LIFO.
//    for (uint32_t iPage = 0; iPage < pagesPerSlab; iPage += 2)
//    {
//        uint32_t dec = pagesPerSlab & 1 ? 1 : 2;
//        for (uint32_t iBlock = 0; iBlock < blocksPerPage; ++iBlock)
//        {
//            ptr = blockAlloc.allocate();
//            void* p = slab[pagesPerSlab - iPage - dec][iBlock];
//            if(p != ptr) __debugbreak();
//            ASSERT_EQ(ptr, p);
//        }
//    }
//
//    // Deallocate all.
//    for (uint32_t iSlab = 0; iSlab < NUM_SLABS; ++iSlab)
//    {
//        for (uint32_t iPage = 0; iPage < pagesPerSlab; ++iPage)
//        {
//            for (uint32_t iBlock = 0; iBlock < blocksPerPage; ++iBlock)
//            {
//                blockAlloc.deallocate(blockByPage[iSlab][iPage][iBlock]);
//            }
//        }
//    }
//
//    blockAlloc.shutdown();
//}

//------------------------------------------------------------------------------
void parallelFree(uint32_t blockSize, uint32_t pageSize, uint32_t numThreads)
{
    MemoryStore memStore;
    BinnedAllocator binnedAlloc;
    binnedAlloc.init(&memStore);
    gts::internal::BlockAllocator blockAlloc;
    blockAlloc.init(&binnedAlloc, blockSize, pageSize);

    constexpr uint32_t NUM_SLABS = 3;
    std::vector<SlabHeader*> slabs(NUM_SLABS);
    std::vector<std::vector<void*>> blockByPage[NUM_SLABS];

    void* ptr = blockAlloc.allocate();

    SlabHeader* pSlab = MemoryStore::toSlab(ptr);
    uint32_t pagesPerSlab = pSlab->numPages;
    uint32_t blocksPerPage = pSlab->pageSize / blockSize;

    blockAlloc.deallocate(ptr);

    QueueSPMC<void*> blockQueue;

    // Fully allocate several slabs.
    for (uint32_t iSlab = 0; iSlab < NUM_SLABS; ++iSlab)
    {
        for (uint32_t iPage = 0; iPage < pagesPerSlab; ++iPage)
        {
            for (uint32_t iBlock = 0; iBlock < blocksPerPage; ++iBlock)
            {
                ptr = blockAlloc.allocate();
                ASSERT_TRUE(blockQueue.tryPush(ptr));
            }
        }
    }

    gts::Atomic<bool> startTest(false);

    // Concurrently deallocate all blocks
    std::vector<std::thread*> threads;
    threads.resize(numThreads);
    for (uint32_t tt = 0; tt < numThreads; ++tt)
    {
        threads[tt] = new std::thread([&]()
        {
            while (!startTest.load(memory_order::acquire))
            {
                GTS_PAUSE();
            }

            while (!blockQueue.empty())
            {
                void* pBlock = nullptr;
                if (blockQueue.tryPop(pBlock))
                {
                    blockAlloc.deallocate(pBlock);
                }
            }
        });
    }

    startTest.store(true, memory_order::release);

    for (uint32_t tt = 0; tt < numThreads; ++tt)
    {
        threads[tt]->join();
        delete threads[tt];
    }
}

//------------------------------------------------------------------------------
TEST(BlockAllocatorTest, parallelFree_1Thread)
{
    parallelFree(GTS_MALLOC_ALIGNEMNT, MemoryStore::PAGE_SIZE_CLASS_0, 1);
}

//------------------------------------------------------------------------------
TEST(BlockAllocatorTest, parallelFree_ManyThreads)
{
    parallelFree(GTS_MALLOC_ALIGNEMNT, MemoryStore::PAGE_SIZE_CLASS_0, Thread::getHardwareThreadCount());
}



} // namespace testing
