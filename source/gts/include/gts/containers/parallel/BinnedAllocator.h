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

#include "gts/platform/Assert.h"
#include "gts/platform/Memory.h"
#include "gts/platform/Atomic.h"
#include "gts/platform/Utils.h"
#include "gts/platform/Thread.h"
#include "gts/synchronization/SpinMutex.h"
#include "gts/containers/OsHeapAllocator.h"
#include "gts/containers/parallel/QueueMPMC.h"
#include "gts/containers/parallel/ParallelHashTable.h"

#ifdef GTS_MSVC
#pragma warning( push )
#pragma warning( disable : 4324) // alignment padding warning
#endif

namespace gts {

/** 
 * @addtogroup Containers
 * @{
 */

/** 
 * @addtogroup ParallelContainers
 * @{
 */

/**
* The alignment of all allocations.
* @remark
*  To aid the compiler's use of SIMD registers use (may also require a flag,
*  see compiler documentation):
*  - 16 for 128-bit registers (SSE)
*  - 32 for 256-bit registers (AVX/2)
*  - 64 for 512-bit registers (AVX512)
*/
constexpr uint32_t GTS_MALLOC_ALIGNEMNT = 16;

namespace internal {

class BlockAllocator;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  An doubly-linked list that cannot contain duplicates
 */
class DList
{
public:

    //! An intrusive Node.
    struct Node
    {
        //! The previous node in the list.
        Node* pPrev = nullptr;

        //! The next node in the list.
        Node* pNext = nullptr;
    };

    Node* popFront();

    void pushFront(Node* pNode);

    void pushBack(Node* pNode);

    void remove(Node* pNode);

    void clear();

    size_t size() const;

    bool empty() const;

    bool containes(Node* pNode) const;

private:

    Node* m_pHead = nullptr;
    Node* m_pTail = nullptr;
};

} // namespace internal

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  An intrusive singly-linked list node used for free-lists.
 */
struct FreeListNode
{
    //! The next node in the list.
    Atomic<FreeListNode*> pNextFree = { nullptr };
};

struct GTS_ALIGN(GTS_MALLOC_ALIGNEMNT) PageHeader;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  The states of memory object.
 */
enum class MemoryState : uint8_t
{
    STATE_FREE = 0,
    STATE_COMMITTED,
    STATE_IN_USE,
    STATE_ACTIVE,
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  The header for each Slab.
 */
struct GTS_ALIGN(GTS_MALLOC_ALIGNEMNT) SlabHeader
    : public internal::DList::Node
{
    //! The free list used by the thread that owns this Slab.
    FreeListNode* pLocalPageFreeList = nullptr;

    //! The free list used by other threads.
    Atomic<FreeListNode*> pNonLocalPageFreeList = { nullptr };

    //! The start of the PagesHeaders.
    PageHeader* pPageHeaders = nullptr;

    //! The start of the Pages' blocks.
    void* pBlocks = nullptr;

    internal::BlockAllocator* myAllocator = 0;

    //! The thread this Slab belongs to.
    ThreadId tid = 0;

    //! The size in bytes of the slab.
    uint32_t slabSize = 0;

    //! The size in bytes of the pages.
    uint32_t pageSize = 0;

    //! The number of possible committed Pages in this Slab.
    uint16_t numPages = 0;

    //! The number of committed Pages in this Slab.
    uint16_t numCommittedPages = 0;

    //! The total header size.
    uint16_t totalHeaderSize = 0;

    //! Flag true if this slab is active
    MemoryState state = MemoryState::STATE_FREE;

    /**
     * @returns
     *  True if pages from pNonLocalFreeList are move to pLocalFreeList.
     * @remark
     *  Thread-safe.
     */
    bool reclaimNonLocalPages();
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
* @brief
*  The header for each Page.
*/
struct GTS_ALIGN(GTS_MALLOC_ALIGNEMNT) PageHeader
    : public FreeListNode
{
    //! The free list used by the thread that owns this Page.
    FreeListNode* pLocalFreeList = nullptr;

    //! The free list used by other threads.
    Atomic<FreeListNode*> pNonLocalFreeList = { nullptr };

    //! The start of the Page's blocks.
    void* pBlocks = nullptr;

    //! The thread this Page belongs to.
    ThreadId tid = 0;

    //! The size of each block.
    uint32_t blockSize = 0;

    //! The number of used Blocks in this Page.
    Atomic<uint16_t> numUsedBlocks = { 0 };

    //! Flag true if this page has allocated blocks that are aligned to a
    //! boundary other than GTS_MALLOC_ALIGNEMNT.
    bool hasAlignedBlocks = false;

    MemoryState state = MemoryState::STATE_FREE;

    /**
     * @returns
     *  True if block from pNonLocalFreeList are move to pLocalFreeList.
     * @remark
     *  Thread-safe.
     */
    bool reclaimNonLocalBlocks();
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  Creates, destroys, and manages Pages and Slabs.
 */
class MemoryStore
{
public: // STRUCTORS

    ~MemoryStore();

    GTS_INLINE bool init() const { return true; }

public: // ACCESSORS

    /**
     * @returns True if the store has no allocated slabs.
     * @remark
     *  Not thread-safe.
     */
    bool empty() const;

public: // MUTATORS

    /**
     * @brief Deallocates all slabs.
     * @remark
     *  Not thread-safe.
     */
    void clear();

    /**
     * Allocates a region of memory. Slabs are reserved as 4MiB unless 'pageSize'
     * does not fit.
     */
    SlabHeader* allocateSlab(size_t pageSize);

    /**
     * Add a Slab to the free list. Frees Slabs larger than SLAB_SIZE.
     */
    void deallocateSlab(SlabHeader* pSlab, bool freeIt);

    /**
     * Commits a page from the Slab and initializes it.
     */
    PageHeader* allocatePage(SlabHeader* pSlab, size_t blockSize);

    /**
     * Initializes a Page.
     */
    void initPage(PageHeader* pPage, size_t pageSize, size_t blockSize);

    /**
     * Decommits a Page.
     */
    void deallocatePage(PageHeader* pPage);

public: // UTILITIES

    /**
     * @returns This block's Slab.
     * @remark
     *  Thread-safe.
     */
    static GTS_INLINE SlabHeader* toSlab(void* ptr)
    {
        return (SlabHeader*)alignDownTo((uintptr_t)ptr, MemoryStore::SLAB_SIZE);
    }

    /**
     * @returns This block's Page.
     * @remark
     *  Thread-safe.
     */
    static GTS_INLINE PageHeader* toPage(SlabHeader* pSlab, void* ptr)
    {
        uintptr_t index = ((uintptr_t)ptr - (uintptr_t)pSlab->pBlocks) / pSlab->pageSize;
        return pSlab->pPageHeaders + index;
    }

    /**
     * @returns This aligned block actual start address.
     * @remark
     *  Thread-safe.
     */
    static GTS_INLINE void* toAlignedBlockStart(PageHeader* pPage, void* ptr)
    {
        GTS_ASSERT(pPage->hasAlignedBlocks);
        uint8_t* pPageStart = (uint8_t*)pPage->pBlocks;
        size_t dist         = (uint8_t*)ptr - pPageStart;
        size_t offset       = dist % pPage->blockSize;
        return (void*)(uintptr_t(ptr) - offset);
    }

    static GTS_INLINE size_t pageSizeToIndex(size_t pageSize)
    {
        switch (pageSize)
        {
        case PAGE_SIZE_CLASS_0:
            return 0;
        case PAGE_SIZE_CLASS_1:
            return 1;
        case PAGE_SIZE_CLASS_2:
            return 2;
        case PAGE_SIZE_CLASS_3:
            return 3;
        default:
            return 4;
        }
    }

#if GTS_64BIT
    static constexpr uint32_t SIZE_DIVISOR = 1;
#else
    static constexpr uint32_t SIZE_DIVISOR = 2;
#endif

    //! The size of a the Slab used for all Page size classes.
    static constexpr uint32_t SLAB_SIZE = (4 * 1024 * 1024) / SIZE_DIVISOR;

    // The pages sizes for each size class of allocation.
    static constexpr uint32_t PAGE_SIZE_CLASS_0 = (16 * 1024) / SIZE_DIVISOR;
    static constexpr uint32_t PAGE_SIZE_CLASS_1 = (64 * 1024) / SIZE_DIVISOR;
    static constexpr uint32_t PAGE_SIZE_CLASS_2 = (128 * 1024) / SIZE_DIVISOR;
    static constexpr uint32_t PAGE_SIZE_CLASS_3 = (512 * 1024) / SIZE_DIVISOR;
    // PAGE_SIZE_CLASS_4 = max(PAGE_SIZE_CLASS_3, SLAB_SIZE - SINGLE_PAGE_HEADER_SIZE]).

    //! The number of free list, one for each page class size.
    static constexpr uint32_t PAGE_FREE_LISTS_COUNT = 5;

    //! The needed space for headers when allocating a Slab with a single Page.
    static const uint32_t SINGLE_PAGE_HEADER_SIZE;

    static_assert(PAGE_SIZE_CLASS_0 / GTS_MALLOC_ALIGNEMNT != 0,
        "Allocation granularity is too small.");

private:

    using Queue = QueueMPMC<
        SlabHeader*,
        UnfairSpinMutex<>,
        OsHeapAllocator<GTS_NO_SHARING_CACHE_LINE_SIZE>>;

    using backoff_type = Backoff<>;

    void _freeSlabList(Queue& slabList, size_t& slabCount);
    void _freeSlab(SlabHeader* pPage);

    SlabHeader* _getFreeListSlab(size_t pageSize);
    SlabHeader* _reserveNewSlab(uint32_t pageSize);
    void _resetSlab(SlabHeader* pSlab, uint32_t pageSize);
    SlabHeader* _initalizeSlab(void* pRawSlab, uint32_t slabSize, uint32_t pageSize);
    void _initalizeSlabHeader(
        SlabHeader* pSlab,
        uint32_t totalHeaderSize,
        uint32_t slabSize,
        uint32_t pageSize,
        uint32_t numPages);

    //! A list of freed Slabs
    Queue m_freedSlabs;

    //! A list of abandoned Slabs by page size.
    Queue m_abandonedSlabFreeListByClass[PAGE_FREE_LISTS_COUNT];

    //! The total number of allocated slabs.
    Atomic<size_t> m_slabCount = { 0 };
};

class BinnedAllocator;

namespace internal {

    ////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////
    /**
     * @brief
     *  Manages a list of Slab and Pages, and allocates and frees homogeneous
     *  Pages from Slabs and Blocks from Pages.
     * @remark
     *  This class is not thread safe.
     */
    class BlockAllocator
    {
        using backoff_type = Backoff<>;
        using mutex_type = UnfairSharedSpinMutex<uint8_t, backoff_type>;
        using size_type = uint32_t;

    public: // STRUCTORS

        BlockAllocator();
        ~BlockAllocator();

        GTS_INLINE BlockAllocator(BlockAllocator const&) { GTS_ASSERT(0); }

        /**
         * Initialize the BlockAllocator. This must happen before calling allocate.
         * @param pMemoryStore
         *  The source of memory for this allocator.
         * @param blockSize
         *  The size of each Block in a Page.
         * @param pageSize
         *  The size of each Page in a Slab.
         * @remark
         *  Not thread-safe.
         */
        bool init(BinnedAllocator* pBinnedAllocator, size_type blockSize, size_type pageSize);

        /**
         * Returns all allocated memory back to the OS and checks for memory leaks.
         * @remark
         *  Not thread-safe.
         */
        void shutdown();

    public: // MUTATORS

        /**
         * @returns An 'allocationSize' chunk of memory.
         * @remark
         *  Not thread-safe.
         */
        void* allocate();

        /**
         * Free the memory in ptr.
         * @param workerId
         *  The ID of the calling Worker thread.
         * @remark
         *  Thread-safe.
         */
        void deallocate(void* ptr);

    public: // ACCESSORS

        /**
         * @returns The size of each Block in a Page.
         * @remark
         *  Thread-safe.
         */
        GTS_INLINE size_type blockSize() const { return m_blockSize; }

        /**
         * @returns The size of each Page in a Slab.
         * @remark
         *  Thread-safe.
         */
        GTS_INLINE size_type pageSize() const { return m_pageSize; }

    private:

        void* _getNextFreeBlock();

        bool _reclaimNonlocalFreeBlocks();

        bool _getNewPage();

    private:

        //! The source of all memory.
        BinnedAllocator* m_pBinnedAllocator = nullptr;

        //! The current page blocks are being alloted from.
        PageHeader* m_pActivePage = nullptr;

        //! The size of the blocks this allocator returns.
        size_type m_blockSize = 0;

        //! The size of the pages this allocator consumes.
        size_type m_pageSize = 0;
    };

} // namespace internal

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A collection of BlockAllocators binned to size classes.
 *
 * @details
 *  References:
 *  1. https://google.github.io/tcmalloc/design
 *  2. https://www.microsoft.com/en-us/research/uploads/prod/2019/06/mimalloc-tr-v1.pdf
 *  3. https://pdfs.semanticscholar.org/7b55/610961c63b86141f37c68e73e0326fae0693.pdf
 *
 * @todo Add debug mode.
 * @todo Add large page support.
 * @todo Add NUMA support.
 * @todo Add statistics.
 * @todo Optimize.
 */
class BinnedAllocator
{
public:

    using AllocatorVector = Vector<
        internal::BlockAllocator,
        OsHeapAllocator<GTS_NO_SHARING_CACHE_LINE_SIZE>
    >;

    using size_type = uint32_t;

public: // STRUCTORS

    BinnedAllocator() = default;

    /**
     * Destructs the allocator. Frees all BlockAllocator.
     * @remark
     *  Not thread-safe.
     */
    ~BinnedAllocator();

    /**
     * Initialize the BinnedSlabAllocator. This must happen before calling allocate.
     * @remark
     *  Not thread-safe.
     */
    bool init(MemoryStore* pMemoryStore);

    /**
     * Returns all allocated memory back to the OS and checks for memory leaks.
     * @remark
     *  Not thread-safe.
     */
    void shutdown();

public: // MUTATORS

    /**
     * @returns A block memory of size 'size'.
     * @remark
     *  Not thread-safe.
     */
    void* allocate(size_t size);

    /**
     * Frees the memory in ptr.
     * @remark
     *  Not thread-safe.
     */
    void deallocate(void* ptr);

public: // ACCESSORS

    /**
     * @returns A vector of all the binned BlockAllocators.
     * @remark
     *  Thread-safe.
     */
    AllocatorVector const& getAllAllocators() const;

    /**
     * @returns True if BinnedAllocator is initialized.
     */
    GTS_INLINE bool isInitialized() const
    { 
        return m_isInitialized;
    }

    /**
     * @returns The number of bins.
     * @remark
     *  Thread-safe.
     */
    GTS_INLINE size_type binCount() const
    {
        return m_numBins;
    }

    /**
     * @returns The bin 'size' belongs in.
     * @remark
     *  Thread-safe.
     */
    GTS_INLINE size_type calculateBin(size_type size)
    {
        GTS_ASSERT(size > 0);

        size_type bin = 0;

        if (size <= BIN_SIZE_CLASS_0)
        {
            bin = (size - 1) / GTS_MALLOC_ALIGNEMNT;
        }
        else if (size <= BIN_SIZE_CLASS_2)
        {
            size_type upperBound    = nextPow2(size + 1);
            size_type lowerBound    = upperBound / 2;
            size_type intervalSize  = (upperBound - lowerBound) / BIN_DIVISOR;
            size_type localBin      = (size_type)alignUpTo(size, intervalSize);
            size_type numClass0Bins = BIN_SIZE_CLASS_0 / GTS_MALLOC_ALIGNEMNT - 1;
            size_type class0Overlap = log2i(BIN_SIZE_CLASS_0) * BIN_DIVISOR;
            size_type intervalStep  = (localBin - lowerBound) / intervalSize;

            bin = (log2i(size) * BIN_DIVISOR + intervalStep) - class0Overlap + numClass0Bins;
        }
#if GTS_64BIT
        else if (size <= BIN_SIZE_CLASS_3)
        {
            bin = m_numBins - 1;
        }
#endif
        else
        {
            bin = BIN_IDX_OVERSIZED;
        }

        return bin;
    }

    /**
     * @returns The bin 'ptr' belongs to.
     * @remark
     *  Thread-safe.
     */
    GTS_INLINE size_type calculateBin(void* ptr)
    {
        GTS_ASSERT(ptr);
        SlabHeader* pSlab = MemoryStore::toSlab(ptr);
        PageHeader* pPage = MemoryStore::toPage(pSlab, ptr);
        return calculateBin(pPage->blockSize);
    }

    /**
    * @returns The total number of Slabs being used for the given page size.
    * @remark
    *  Not thread-safe.
    */
    GTS_INLINE size_type slabCount(size_t pageSize) const
    {
        return m_slabCounts[MemoryStore::pageSizeToIndex(pageSize)];
    }

public:

    static constexpr size_type BIN_SIZE_CLASS_0  = 1024;
    static constexpr size_type BIN_SIZE_CLASS_1  = 8 * 1024;
    static constexpr size_type BIN_SIZE_CLASS_2  = 32 * 1024;
    static constexpr size_type BIN_SIZE_CLASS_3  = 512 * 1024;
    static constexpr size_type BIN_IDX_OVERSIZED = UINT32_MAX;
    static constexpr size_type BIN_DIVISOR       = 4;

private:

    friend class internal::BlockAllocator;

    PageHeader* _allocatePage(size_t pageSize, size_t blockSize);

    PageHeader* _walkSlabListForFreePages(size_t pageSize, size_t blockSize);

    PageHeader* _getPageFromSlab(size_t pageSize, size_t blockSize, SlabHeader* pSlab);

    PageHeader* _allocatePageFromNewSlab(size_t pageSize, size_t blockSize);

    void _deallocatePage(SlabHeader* pSlab, PageHeader* pPage);

    void _tryFreeSlab(SlabHeader* pSlab);

    //! All the BlockAllocators by bin index.
    AllocatorVector m_allocatorsByBin;

    //! The MemoryStore used by all BlockAllocators.
    MemoryStore* m_pMemoryStore = nullptr;

    //! A list of non empty slabs.
    internal::DList m_slabLists[MemoryStore::PAGE_FREE_LISTS_COUNT];

    //! The current slab pages are being alloted from.
    SlabHeader* m_activeSlabs[MemoryStore::PAGE_FREE_LISTS_COUNT] = { 0 };

    //! The number of slabs in each slab list.
    size_type m_slabCounts[MemoryStore::PAGE_FREE_LISTS_COUNT] = { 0 };

    //! The number of bins in this allocator.
    size_type m_numBins = 0;

    //! Initialization flag.
    bool m_isInitialized = false;
};

/** @} */ // end of ParallelContainers
/** @} */ // end of Containers

} // namespace gts

#ifdef GTS_MSVC
#pragma warning( pop )
#endif
