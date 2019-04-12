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

namespace gts {

#ifdef GTS_MSVC
#pragma warning( push )
#pragma warning( disable : 4324) // alignment padding warning
#endif

struct SlabList;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A thread distributed slab allocator.
 */
class DistributedSlabAllocator
{
public:

    struct HeaderNode;

    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    struct FreeList
    {
        gts::Atomic<HeaderNode*> pHead = { nullptr };
    };

    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    struct GTS_ALIGN(GTS_NO_SHARING_CACHE_LINE_SIZE) LocalAllocator
    {
    public:

        LocalAllocator();
        ~LocalAllocator();

        void* allocate();

    private:

        HeaderNode* _getNextFreeNode();

        void _reclaimDeferredNodes();

        GTS_NO_INLINE void* _slowAlloc();

        void _destroy();

    private:

        friend class DistributedSlabAllocator;

        LocalAllocator(LocalAllocator const&) {}
        LocalAllocator& operator=(LocalAllocator const&) { return *this; }

        FreeList m_deferredFreeList;
        FreeList m_freeList;
        Mutex*   m_pUnknownThreadsMutex = nullptr;
        uint32_t m_threadIdx            = 0;
        uint32_t m_allocationSize       = 0;

        SlabList* m_pSlabList           = nullptr;
        uint32_t m_slabCount            = 0;
        uint32_t m_createdNodeCount     = 0;
        uint32_t m_slabSize             = 0;
        uint32_t m_accessorThreadCount  = 0;

        enum STATS
        {
            NUM_ALLOCATIONS,
            NUM_MEMORY_RECLAIMS,
            NUM_SLOW_PATH_ALLOCATIONS,
            NUM_FREES,
            NUM_DEFERRED_FREES
        };

    #ifdef ENABLE_GTS_SLAB_ALLOCATOR_STATS

        size_t numAllocations = 0;
        size_t numMemoryReclaims = 0;
        size_t numSlowPathAllocations = 0;
        size_t numFrees = 0;
        size_t numDeferredFrees = 0;

        void countStat(STATS stat)
        {
            switch (stat)
            {
            case STATS::NUM_ALLOCATIONS:
                numAllocations++;
                break;
            case STATS::NUM_MEMORY_RECLAIMS:
                numMemoryReclaims++;
                break;
            case STATS::NUM_SLOW_PATH_ALLOCATIONS:
                numSlowPathAllocations++;
                break;
            case STATS::NUM_FREES:
                numFrees++;
                break;
            case STATS::NUM_DEFERRED_FREES:
                numDeferredFrees++;
                break;
        };
    }

    #endif
    };

    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    struct GTS_ALIGN(GTS_NO_SHARING_CACHE_LINE_SIZE) HeaderNode
    {
        gts::Atomic<HeaderNode*> pNext      = { nullptr };
        LocalAllocator* myLocalAllocator    = nullptr;
        uint32_t myThreadIdx                = UINT32_MAX;
        uint32_t accessorThreadCount        = 0;
        uint32_t size                       = 0;
        uint8_t isFree                      = 1;

        void free(uint32_t threadIndex);

    private:

        void addDeferredNode(HeaderNode* pNode);
    };

public: // STRUCTORS

    DistributedSlabAllocator();

    ~DistributedSlabAllocator();

public: // MUTATORS

    bool init(uint32_t allocationSize, uint32_t accessorThreadCount);

    bool init(uint32_t allocationSize, uint32_t accessorThreadCount, uint32_t slabSize);

    void* allocate(uint32_t threadIndex);

    void free(uint32_t threadIndex, void* ptr);

public: // ACCESSORS

    uint32_t slabSize() const;

    uint32_t slabCount(uint32_t threadIndex) const;

    uint32_t nodeCount(uint32_t threadIndex) const;

    static uint32_t getSize(void* ptr);

private:

    LocalAllocator const* _getLocalAllocator(uint32_t threadIndex) const;
    LocalAllocator* _getLocalAllocator(uint32_t threadIndex);

    bool _init();

    void _shutdown();

private: // DATA:

    LocalAllocator* m_pLocalAllocatorsByThreadIdx;
    uint32_t m_accessorThreadCount;

    Mutex m_unknownThreadsMutex;
    LocalAllocator m_unknownThreadAllocator;

    uint32_t m_allocationSize;
    uint32_t m_slabSize;
};

#ifdef GTS_MSVC
#pragma warning( pop )
#endif

} // namespace gts
