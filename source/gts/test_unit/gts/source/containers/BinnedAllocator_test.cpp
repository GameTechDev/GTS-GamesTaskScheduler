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
TEST(BinnedAllocator, initAndShutdown)
{
    MemoryStore memStore;
    BinnedAllocator binnedAlloc;
    binnedAlloc.init(&memStore);

    // Verify all the bins are the expected size.

    auto const& allocatorsVec = binnedAlloc.getAllAllocators();

    uint32_t numBinsClass0 = BinnedAllocator::BIN_SIZE_CLASS_0 / GTS_MALLOC_ALIGNEMNT;
    uint32_t binIdx = 0;

    // Class 0: [8B, 1024B) with GTS_MALLOC_ALIGNEMNT spacing.
    uint32_t class0Size = GTS_MALLOC_ALIGNEMNT;
    for (size_t ii = 0; ii < numBinsClass0; ++ii)
    {
        ASSERT_EQ(allocatorsVec[binIdx++].blockSize(), class0Size);
        class0Size += GTS_MALLOC_ALIGNEMNT;
    }

    // Class 1: [1KiB, 8Kib) with BIN_DIVISOR spacing per power-of-2.
    for (uint32_t lowerBound = BinnedAllocator::BIN_SIZE_CLASS_0; lowerBound < BinnedAllocator::BIN_SIZE_CLASS_1; lowerBound *= 2)
    {
        uint32_t upperBound = nextPow2(lowerBound + 1);
        uint32_t intervalSize = (upperBound - lowerBound) / BinnedAllocator::BIN_DIVISOR;

        for (uint32_t jj = 1; jj <= BinnedAllocator::BIN_DIVISOR; ++jj)
        {
            ASSERT_EQ(allocatorsVec[binIdx++].blockSize(), lowerBound + intervalSize * jj);
        }
    }

    // Class 2: [8KiB, 32Kib) with BIN_DIVISOR spacing per power-of-2.
    for (uint32_t lowerBound = BinnedAllocator::BIN_SIZE_CLASS_1; lowerBound < BinnedAllocator::BIN_SIZE_CLASS_2; lowerBound *= 2)
    {
        uint32_t upperBound = nextPow2(lowerBound + 1);
        uint32_t intervalSize = (upperBound - lowerBound) / BinnedAllocator::BIN_DIVISOR;

        for (uint32_t jj = 1; jj <= BinnedAllocator::BIN_DIVISOR; ++jj)
        {
            ASSERT_EQ(allocatorsVec[binIdx++].blockSize(), lowerBound + intervalSize * jj);
        }
    }

#if GTS_64BIT
    // Class 3: [32KiB, 512KiB] with just one bin. All sizes are mapped to a 512KiB slab.
    ASSERT_EQ(allocatorsVec[binIdx++].blockSize(), MemoryStore::PAGE_SIZE_CLASS_3);
#endif

    binnedAlloc.shutdown();
}

//------------------------------------------------------------------------------
TEST(BinnedAllocator, calculateBinFromSize)
{
    MemoryStore memStore;
    BinnedAllocator binnedAlloc;
    binnedAlloc.init(&memStore);

    // Verify sizes are assigned to the correct bin.

    uint32_t binIdx = 0;
    uint32_t prevSize = 0;
    uint32_t currSize = 0;

    // Class 0: [8B, 1024B) with 8B spacing.
    for (size_t ii = 0; ii < BinnedAllocator::BIN_SIZE_CLASS_0; ii += GTS_MALLOC_ALIGNEMNT)
    {
        currSize += GTS_MALLOC_ALIGNEMNT;

        // check size and boundaries
        ASSERT_EQ(binnedAlloc.calculateBin(prevSize + 1), binIdx);
        ASSERT_EQ(binnedAlloc.calculateBin(currSize - 1), binIdx);
        ASSERT_EQ(binnedAlloc.calculateBin(currSize), binIdx);

        prevSize = currSize;
        ++binIdx;
    }

    // [1KiB, 32Kib) with BIN_DIVISOR spacing per power-of-2.
    for (uint32_t lowerBound = BinnedAllocator::BIN_SIZE_CLASS_0; lowerBound < BinnedAllocator::BIN_SIZE_CLASS_2; lowerBound *= 2)
    {
        uint32_t upperBound = nextPow2(lowerBound + 1);
        uint32_t intervalSize = (upperBound - lowerBound) / BinnedAllocator::BIN_DIVISOR;

        prevSize = lowerBound;

        for (uint32_t jj = 1; jj <= BinnedAllocator::BIN_DIVISOR; ++jj)
        {
            currSize = lowerBound + intervalSize * jj;

            // check size and boundaries
            ASSERT_EQ(binnedAlloc.calculateBin(prevSize + 1), binIdx);
            ASSERT_EQ(binnedAlloc.calculateBin(currSize - 1), binIdx);
            ASSERT_EQ(binnedAlloc.calculateBin(currSize), binIdx);

            prevSize = currSize;
            ++binIdx;
        }
    }

#if GTS_64BIT
    // Class 3: [32KiB, 512KiB] with just one bin. All sizes are mapped to a 512KiB slab.
    {
        prevSize = BinnedAllocator::BIN_SIZE_CLASS_2;
        currSize = BinnedAllocator::BIN_SIZE_CLASS_3;

        // check size and boundaries
        ASSERT_EQ(binnedAlloc.calculateBin(prevSize + 1), binIdx);
        ASSERT_EQ(binnedAlloc.calculateBin(currSize - 1), binIdx);
        ASSERT_EQ(binnedAlloc.calculateBin(currSize), binIdx);
    }


    // Oversized page.
    ASSERT_EQ(binnedAlloc.calculateBin(BinnedAllocator::BIN_SIZE_CLASS_3 + 1), BinnedAllocator::BIN_IDX_OVERSIZED);
#endif

    // Huge page.
    ASSERT_EQ(binnedAlloc.calculateBin(MemoryStore::SLAB_SIZE), BinnedAllocator::BIN_IDX_OVERSIZED);

    binnedAlloc.shutdown();
}

//------------------------------------------------------------------------------
TEST(BinnedAllocator, calculateBinFromPtr)
{
    MemoryStore memStore;
    BinnedAllocator binnedAlloc;
    binnedAlloc.init(&memStore);

    // Verify sizes are assigned to the correct bin.

    uint32_t binIdx = 0;
    uint32_t currSize = 0;

    // Class 0: [8B, 1024B) with 8B spacing.
    for (size_t ii = 0; ii < BinnedAllocator::BIN_SIZE_CLASS_0; ii += GTS_MALLOC_ALIGNEMNT)
    {
        currSize += GTS_MALLOC_ALIGNEMNT;

        void* ptr = binnedAlloc.allocate(currSize);
        ASSERT_EQ(binnedAlloc.calculateBin(ptr), binIdx);
        binnedAlloc.deallocate(ptr);

        ++binIdx;
    }

    // [1KiB, 32Kib) with BIN_DIVISOR spacing per power-of-2.
    for (uint32_t lowerBound = BinnedAllocator::BIN_SIZE_CLASS_0; lowerBound < BinnedAllocator::BIN_SIZE_CLASS_2; lowerBound *= 2)
    {
        uint32_t upperBound = nextPow2(lowerBound + 1);
        uint32_t intervalSize = (upperBound - lowerBound) / BinnedAllocator::BIN_DIVISOR;

        for (uint32_t jj = 1; jj <= BinnedAllocator::BIN_DIVISOR; ++jj)
        {
            currSize = lowerBound + intervalSize * jj;

            void* ptr = binnedAlloc.allocate(currSize);
            ASSERT_EQ(binnedAlloc.calculateBin(ptr), binIdx);
            binnedAlloc.deallocate(ptr);

            ++binIdx;
        }
    }

#if GTS_64BIT
    // Class 3: [32KiB, 512KiB] with just one bin. All sizes are mapped to a 512KiB slab.
    {
        currSize = BinnedAllocator::BIN_SIZE_CLASS_3;

        void* ptr = binnedAlloc.allocate(currSize);
        ASSERT_EQ(binnedAlloc.calculateBin(ptr), binIdx);
        binnedAlloc.deallocate(ptr);
    }

    // Oversized page.
    {
        void* ptr = binnedAlloc.allocate(BinnedAllocator::BIN_SIZE_CLASS_3 + 1);
        ASSERT_EQ(binnedAlloc.calculateBin(ptr), BinnedAllocator::BIN_IDX_OVERSIZED);
        binnedAlloc.deallocate(ptr);
    }
#else
    // Oversized page.
    {
        void* ptr = binnedAlloc.allocate(BinnedAllocator::BIN_SIZE_CLASS_2 + 1);
        ASSERT_EQ(binnedAlloc.calculateBin(ptr), BinnedAllocator::BIN_IDX_OVERSIZED);
        binnedAlloc.deallocate(ptr);
    }
#endif


    // Huge page.
    {
        void* ptr = binnedAlloc.allocate(MemoryStore::SLAB_SIZE);
        ASSERT_EQ(binnedAlloc.calculateBin(ptr), BinnedAllocator::BIN_IDX_OVERSIZED);
        binnedAlloc.deallocate(ptr);
    }

    binnedAlloc.shutdown();
}

//------------------------------------------------------------------------------
void allocateFree(BinnedAllocator& binnedAlloc, size_t size)
{
    std::vector<void*> allocations;

    void* ptr = binnedAlloc.allocate(size);
    allocations.push_back(ptr);
    SlabHeader* pSlab = MemoryStore::toSlab(ptr);
    PageHeader* pPage = MemoryStore::toPage(pSlab, ptr);
    uint32_t numBlocks = pSlab->pageSize / pPage->blockSize;

    while (pPage->numUsedBlocks.load(memory_order::relaxed) < numBlocks)
    {
        ptr = binnedAlloc.allocate(size);
        allocations.push_back(ptr);
        // Verify all allocations came from this page.
        ASSERT_EQ(pPage, MemoryStore::toPage(pSlab, ptr));
    }

    for (size_t ii = 0; ii < allocations.size(); ++ii)
    {
        binnedAlloc.deallocate(allocations[ii]);
    }
}

//------------------------------------------------------------------------------
void allocateFreeAllBins()
{
    MemoryStore memStore;
    BinnedAllocator binnedAlloc;
    binnedAlloc.init(&memStore);

    // Verify sizes are assigned to the correct bin.

    uint32_t currSize = 0;

    // Class 0: [8B, 1024B) with 8B spacing.
    for (size_t ii = 0; ii < BinnedAllocator::BIN_SIZE_CLASS_0; ii += GTS_MALLOC_ALIGNEMNT)
    {
        currSize += GTS_MALLOC_ALIGNEMNT;
        allocateFree(binnedAlloc, currSize);
    }

    // Class 1 & 2: [1KiB, 32Kib) with BIN_DIVISOR spacing per power-of-2.
    for (uint32_t lowerBound = BinnedAllocator::BIN_SIZE_CLASS_0; lowerBound < BinnedAllocator::BIN_SIZE_CLASS_2; lowerBound *= 2)
    {
        uint32_t upperBound = nextPow2(lowerBound + 1);
        uint32_t intervalSize = (upperBound - lowerBound) / BinnedAllocator::BIN_DIVISOR;

        for (uint32_t jj = 1; jj <= BinnedAllocator::BIN_DIVISOR; ++jj)
        {
            currSize = lowerBound + intervalSize * jj;
            allocateFree(binnedAlloc, currSize);
        }
    }


#if GTS_64BIT
    // Class 3: [32KiB, 512KiB) with just one bin. All sizes are mapped to a 512KiB slab.
    {
        currSize = BinnedAllocator::BIN_SIZE_CLASS_3 - sizeof(PageHeader);
        allocateFree(binnedAlloc, currSize);
    }

    // Large page.
    allocateFree(binnedAlloc, BinnedAllocator::BIN_SIZE_CLASS_3 + 1);
#else
    // Large page.
    allocateFree(binnedAlloc, BinnedAllocator::BIN_SIZE_CLASS_2 + 1);
#endif
    // Huge page.
    allocateFree(binnedAlloc, MemoryStore::SLAB_SIZE);

    binnedAlloc.shutdown();
}

//------------------------------------------------------------------------------
TEST(BinnedAllocator, allocateFree)
{
    allocateFreeAllBins();
}

//------------------------------------------------------------------------------
void allocateFreeAllBins(uint32_t threadCount)
{
    std::vector<std::thread*> threads(threadCount - 1);
    gts::Atomic<bool> startTest(false);

    for (uint32_t tt = 0; tt < threadCount - 1; ++tt)
    {
        threads[tt] = new std::thread([&startTest]()
        {
            while (!startTest.load(memory_order::acquire))
            {
                GTS_PAUSE();
            }

            allocateFreeAllBins();
        });
    }

    startTest.store(true, memory_order::release);
    allocateFreeAllBins();

    for (uint32_t tt = 0; tt < threadCount - 1; ++tt)
    {
        threads[tt]->join();
        delete threads[tt];
    }
}

//------------------------------------------------------------------------------
TEST(BinnedAllocator, parallelAllocateFree)
{
    allocateFreeAllBins(Thread::getHardwareThreadCount());
}

//------------------------------------------------------------------------------
TEST(BinnedAllocator, abandonSlabs)
{
    const uint32_t blockSize = GTS_MALLOC_ALIGNEMNT;
    const uint32_t pageSize  = MemoryStore::PAGE_SIZE_CLASS_0;

    MemoryStore memStore;
    BinnedAllocator binnedAlloc;
    binnedAlloc.init(&memStore);
    gts::internal::BlockAllocator blockAlloc;
    blockAlloc.init(&binnedAlloc, blockSize, pageSize);

    constexpr uint32_t NUM_SLABS = 3;
    std::vector<SlabHeader*> slabs(NUM_SLABS);
    std::vector<std::vector<void*>> blockByPage[NUM_SLABS];

    void* ptr              = blockAlloc.allocate();
    SlabHeader* pSlab      = MemoryStore::toSlab(ptr);
    uint32_t pagesPerSlab  = pSlab->numPages;
    uint32_t blocksPerPage = pSlab->pageSize / blockSize;
    blockAlloc.deallocate(ptr);

    std::vector<void*> blocks;

    // Allocate some slabs on a different thread, and then abandon them.
    std::thread thread([&]
        {
            BinnedAllocator binnedAlloc1;
            binnedAlloc1.init(&memStore);
            gts::internal::BlockAllocator blockAlloc1;
            blockAlloc1.init(&binnedAlloc1, blockSize, pageSize);

            // Allocate several slabs.
            for (uint32_t iSlab = 0; iSlab < NUM_SLABS; ++iSlab)
            {
                for (uint32_t iPage = 0; iPage < pagesPerSlab; ++iPage)
                {
                    for (uint32_t iBlock = 0; iBlock < blocksPerPage; ++iBlock)
                    {
                        ptr = blockAlloc1.allocate();
                        blocks.push_back(ptr);
                    }
                }
            }
        });

    // Block until finished.
    thread.join();

    // Allocate several slabs.
    for (uint32_t iSlab = 0; iSlab < NUM_SLABS; ++iSlab)
    {
        for (uint32_t iPage = 0; iPage < pagesPerSlab; ++iPage)
        {
            for (uint32_t iBlock = 0; iBlock < blocksPerPage; ++iBlock)
            {
                ptr = blockAlloc.allocate();
                blocks.push_back(ptr);
            }
        }
    }

    // Deallocate them on this thread.
    for (void* pBlock : blocks)
    {
        blockAlloc.deallocate(pBlock);
    }
}

//------------------------------------------------------------------------------
TEST(BinnedAllocator, allocateFromAbandonSlabs)
{
    const uint32_t blockSize = MemoryStore::PAGE_SIZE_CLASS_3 / 2;
    const uint32_t pageSize  = MemoryStore::PAGE_SIZE_CLASS_3;

    MemoryStore memStore;
    BinnedAllocator binnedAlloc;
    binnedAlloc.init(&memStore);
    gts::internal::BlockAllocator blockAlloc;
    blockAlloc.init(&binnedAlloc, blockSize, pageSize);

    constexpr uint32_t NUM_SLABS = 3;
    std::vector<SlabHeader*> slabs(NUM_SLABS);
    std::vector<std::vector<void*>> blocksByPage[NUM_SLABS];

    void* ptr              = blockAlloc.allocate();
    SlabHeader* pSlab      = MemoryStore::toSlab(ptr);
    uint32_t pagesPerSlab  = pSlab->numPages;
    uint32_t blocksPerPage = pSlab->pageSize / blockSize;
    blockAlloc.deallocate(ptr);

    // Allocate some slabs on a different thread, and then abandon them.
    std::thread thread([&]
    {
        BinnedAllocator binnedAlloc1;
        binnedAlloc1.init(&memStore);
        gts::internal::BlockAllocator blockAlloc1;
        blockAlloc1.init(&binnedAlloc1, blockSize, pageSize);

        // Allocate several slabs.
        for (uint32_t iSlab = 0; iSlab < NUM_SLABS; ++iSlab)
        {
            for (uint32_t iPage = 0; iPage < pagesPerSlab; ++iPage)
            {
                blocksByPage[iSlab].resize(pagesPerSlab);

                for (uint32_t iBlock = 0; iBlock < blocksPerPage; ++iBlock)
                {
                    ptr = blockAlloc1.allocate();
                    blocksByPage[iSlab][iPage].push_back(ptr);
                }
            }
        }

        // Partially deallocate the 2nd Slab.
        uint32_t slabIdx = NUM_SLABS - 2;
        for (uint32_t iPage = 0; iPage < pagesPerSlab; iPage += 2)
        {
            for (uint32_t iBlock = 0; iBlock < blocksPerPage; ++iBlock)
            {
                blockAlloc1.deallocate(blocksByPage[slabIdx][iPage][iBlock]);
                blocksByPage[slabIdx][iPage][iBlock] = nullptr;
            }
        }

        // Fully deallocate the 3nd Slab.
        slabIdx = NUM_SLABS - 1;
        for (uint32_t iPage = 0; iPage < pagesPerSlab; ++iPage)
        {
            for (uint32_t iBlock = 0; iBlock < blocksPerPage; ++iBlock)
            {
                blockAlloc1.deallocate(blocksByPage[slabIdx][iPage][iBlock]);
                blocksByPage[slabIdx][iPage][iBlock] = nullptr;
            }
        }
    });

    // Block until finished.
    thread.join();

    std::vector<void*> blocks;

    // Allocate several slabs.
    for (uint32_t iSlab = 0; iSlab < NUM_SLABS; ++iSlab)
    {
        for (uint32_t iPage = 0; iPage < pagesPerSlab; ++iPage)
        {
            for (uint32_t iBlock = 0; iBlock < blocksPerPage; ++iBlock)
            {
                ptr = blockAlloc.allocate();
                blocks.push_back(ptr);
            }
        }
    }

    // Allocate several slabs.
    for (uint32_t iSlab = 0; iSlab < NUM_SLABS; ++iSlab)
    {
        for (uint32_t iPage = 0; iPage < pagesPerSlab; ++iPage)
        {
            blocksByPage[iSlab].resize(pagesPerSlab);

            for (uint32_t iBlock = 0; iBlock < blocksPerPage; ++iBlock)
            {
                if (blocksByPage[iSlab][iPage][iBlock] != nullptr)
                {
                    blockAlloc.deallocate(blocksByPage[iSlab][iPage][iBlock]);
                }

            }
        }
    }

    // Deallocate them on this thread.
    for (void* pBlock : blocks)
    {
        blockAlloc.deallocate(pBlock);
    }
}

//------------------------------------------------------------------------------
TEST(BinnedAllocator, mixedAllocations)
{
    const uint32_t blockSize = GTS_MALLOC_ALIGNEMNT;
    const uint32_t pageSize  = MemoryStore::PAGE_SIZE_CLASS_0;

    MemoryStore memStore;
    BinnedAllocator binnedAlloc;
    binnedAlloc.init(&memStore);
    gts::internal::BlockAllocator blockAlloc;
    blockAlloc.init(&binnedAlloc, blockSize, pageSize);

    constexpr uint32_t NUM_THREADS = 3;
    constexpr uint32_t NUM_SLABS = 3;
    std::vector<SlabHeader*> slabs(NUM_SLABS);
    std::vector<std::vector<void*>> blockByPage[NUM_SLABS];

    // Get page and slab data.
    void* ptr = blockAlloc.allocate();
    SlabHeader* pSlab = MemoryStore::toSlab(ptr);
    uint32_t pagesPerSlab = pSlab->numPages;
    uint32_t blocksPerPage = pSlab->pageSize / blockSize;
    blockAlloc.deallocate(ptr);

    // Allocate some slabs on a different thread, and then abandon them.
    std::vector<void*> blocks[NUM_THREADS + 1];
    std::vector<std::thread*> threads(NUM_THREADS);    
    gts::Atomic<bool> startTest(false);
    for (uint32_t tt = 0; tt < NUM_THREADS; ++tt)
    {
        threads[tt] = new std::thread([&memStore, &blocks, &startTest, NUM_SLABS, blockSize, pageSize, pagesPerSlab, blocksPerPage, tt]
        {
            BinnedAllocator binnedAlloc1;
            binnedAlloc1.init(&memStore);
            gts::internal::BlockAllocator blockAlloc1;
            blockAlloc1.init(&binnedAlloc1, blockSize, pageSize);

            std::vector<void*> myBlocks;

            while (!startTest.load(memory_order::acquire))
            {
                GTS_PAUSE();
            }

            // Allocate several slabs.
            for (uint32_t iSlab = 0; iSlab < NUM_SLABS; ++iSlab)
            {
                for (uint32_t iPage = 0; iPage < pagesPerSlab; ++iPage)
                {
                    for (uint32_t iBlock = 0; iBlock < blocksPerPage; ++iBlock)
                    {
                        void* ptr = blockAlloc1.allocate();
                        if (iPage == 0)
                        {
                            myBlocks.push_back(ptr);
                        }
                        else
                        {
                            if ((iBlock & 1) == 0)
                            {
                                myBlocks.push_back(ptr);
                            }
                            else
                            {
                                blocks[tt].push_back(ptr);
                            }
                        }
                    }
                }
            }

            // Deallocate my blocks.
            for (void* pBlock : myBlocks)
            {
                blockAlloc1.deallocate(pBlock);
            }
        });
    }

    startTest.store(true, memory_order::release);

    // Block until finished.
    for (uint32_t tt = 0; tt < NUM_THREADS; ++tt)
    {
        threads[tt]->join();
        delete threads[tt];
    }

    // Allocate several slabs.
    for (uint32_t iSlab = 0; iSlab < NUM_SLABS; ++iSlab)
    {
        for (uint32_t iPage = 0; iPage < pagesPerSlab; ++iPage)
        {
            for (uint32_t iBlock = 0; iBlock < blocksPerPage; ++iBlock)
            {
                ptr = blockAlloc.allocate();
                blocks[NUM_THREADS].push_back(ptr);
            }
        }
    }

    // Deallocate remaining blocks on this thread.
    for(auto& vBlock : blocks)
    {
        for (void* pBlock : vBlock)
        {
            blockAlloc.deallocate(pBlock);
        }
    }
}

} // namespace testing
