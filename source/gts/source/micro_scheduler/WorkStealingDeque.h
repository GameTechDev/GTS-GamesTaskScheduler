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

#include <numeric>

#include "gts/platform/Assert.h"
#include "gts/platform/Utils.h"
#include "gts/platform/Atomic.h"
#include "gts/platform/Thread.h"
#include "gts/containers/Vector.h"
#include "gts/containers/AlignedAllocator.h"
#include "gts/micro_scheduler/Task.h"

namespace gts {

#ifdef GTS_MSVC
#pragma warning( push )
#pragma warning( disable : 4324) // alignment padding warning
#endif

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A unbound, spin-locked, single-producer multi-consumer queue with stealing.
 * @tparam TStorage
 *  The storage backing for the container. Must be a random access container,
 *  such as std::vector or std::deque.
 * @tparam TAllocator
 *  The allocator used by the storage backing.
 */
template<
    template <typename, typename> class TStorage = gts::Vector,
    typename TAllocator = gts::AlignedAllocator<Task*, GTS_NO_SHARING_CACHE_LINE_SIZE>>
class WorkStealingDeque
{
public:

    using size_type       = uint32_t;
    using index_size_type = uint32_t;
    using allocator_type  = typename TAllocator::template rebind<Task*>::other;
    using storage_type    = TStorage<Task*, allocator_type>;

    struct RingBuffer
    {
        RingBuffer(uint32_t n, allocator_type const& allocator)
            : vec(n, allocator)
        {}

        storage_type vec;
        size_type mask;
    };

public: // STRUCTORS

    //--------------------------------------------------------------------------
    GTS_INLINE WorkStealingDeque(allocator_type const& allocator = allocator_type())
        // Start at 1 to avoid the integer wrap-around on initial tryTake.
        : m_back(1)
        , m_front(1)
        , m_pRingBuffer(nullptr)
        , m_pQuienscentRingBuffer(nullptr)
        , m_allocator(allocator)
    {
        _grow(1, 0, 0);
    }

    //--------------------------------------------------------------------------
    GTS_INLINE ~WorkStealingDeque()
    {
        gts::alignedDelete(m_pQuienscentRingBuffer);
        gts::alignedDelete(m_pRingBuffer.load(gts::memory_order::relaxed));
    }

    //--------------------------------------------------------------------------
    /**
     * Non-thread-safe copy.
     */
    GTS_INLINE WorkStealingDeque(WorkStealingDeque const& other)
        : m_back(other.m_back.load())
        , m_front(other.m_front.load())
        , m_pRingBuffer(other.m_pRingBuffer.load())
        , m_pQuienscentRingBuffer(nullptr)
        , m_allocator(other.m_allocator)
    {}

public: // ACCESSORS

    //--------------------------------------------------------------------------
    /**
     * @return True of the queue is empty, false otherwise.
     */
    GTS_INLINE bool empty() const
    {
        return size() == 0;
    }

    //--------------------------------------------------------------------------
    /**
     * @return The number of elements in the queue.
     */
    GTS_INLINE size_type size() const
    {
        index_size_type b = m_back.load(gts::memory_order::acquire);
        index_size_type f = m_front.load(gts::memory_order::acquire);

        return (size_type)(b - f);
    }

    //--------------------------------------------------------------------------
    /**
     * @return The capacity of the queue.
     */
    GTS_INLINE size_type capacity() const
    {
        return (size_type)m_pRingBuffer.load(gts::memory_order::acquire)->vec.size();
    }

public: // MUTATORS

    //--------------------------------------------------------------------------
    /**
     * Removes all elements from the queue.
     */
    GTS_INLINE void clear()
    {
        m_front.store(0);
        m_back.store(0);
    }

    //--------------------------------------------------------------------------
    /**
     * A single-producer-safe push operation.
     */
    GTS_FORCE_INLINE bool tryPush(Task* pTask)
    {
        index_size_type f = m_front.load(gts::memory_order::acquire);
        index_size_type b = m_back.load(gts::memory_order::relaxed);
        RingBuffer* pRingBuffer = m_pRingBuffer.load(gts::memory_order::relaxed);

        // If the buffer is at capacity, try to grow it.
        if (std::make_signed<size_type>::type(b - f) >= std::make_signed<size_type>::type(capacity()))
        {
            // Growth needs to be protected on push, so that thiefs can't 
            // reference a stale ring buffer.
            gts::Lock<gts::UnfairSpinMutex> lock(m_spinLock);

            if (!_grow(capacity() * 2, f, b))
            {
                return false;
            }
            pRingBuffer = m_pRingBuffer.load(gts::memory_order::relaxed);
        }

        // Store the new value in the buffer.
        pRingBuffer->vec[b & pRingBuffer->mask] = pTask;

        // Notify the consumers that the new value exists.
        m_back.store(b + 1, gts::memory_order::release);

        return true;
    }

    //--------------------------------------------------------------------------
    /**
     * A single-producer-safe pop operation.
     * @return True if the pop succeeded, false otherwise.
     */
    GTS_FORCE_INLINE bool tryPop(Task*& out, uintptr_t isolationTag = 0)
    {
        gts::Lock<gts::UnfairSpinMutex> lock(m_spinLock);

        // Mark the item as consumed.
        index_size_type b = m_back.load(gts::memory_order::relaxed) - 1;
        m_back.store(b, gts::memory_order::relaxed);

        index_size_type f = m_front.load(gts::memory_order::relaxed);

        // If the queue is empty, quit.
        if (b < f)
        {
            // restore back index.
            m_back.store(b + 1, gts::memory_order::relaxed);
            return false;
        }

        // Get element.
        RingBuffer* pRingBuffer = m_pRingBuffer.load(gts::memory_order::relaxed);
        out = pRingBuffer->vec[b & pRingBuffer->mask];

        if (out->isolationTag() != isolationTag)
        {
            // restore back index.
            m_back.store(b + 1, gts::memory_order::relaxed);
            return false;
        }
        return true;
    }

    //--------------------------------------------------------------------------
    /**
     * A multi-consumer-safe steal operation.
     * @return True if the pop succeeded, false otherwise.
     */
    GTS_FORCE_INLINE bool trySteal(Task*& out, uintptr_t isolationTag = 0)
    {
        gts::Lock<gts::UnfairSpinMutex> lock(m_spinLock);

        // Sync with front from other consumers. It's ok if back is stale because
        // it will just look like the queue is empty.
        index_size_type f = m_front.load(gts::memory_order::acquire);
        index_size_type b = m_back.load(gts::memory_order::acquire);

        // If the queue is empty, quit.
        if (b <= f)
        {
            return false;
        }

        // Get element.
        RingBuffer* pRingBuffer = m_pRingBuffer.load(gts::memory_order::relaxed);
        out = pRingBuffer->vec[f & pRingBuffer->mask];

        if (out->isolationTag() != isolationTag)
        {
            return false;
        }

        // Consume the item.
        m_front.store(f + 1, gts::memory_order::relaxed);
        return true;
    }

private:

    //------------------------------------------------------------------------------
    GTS_INLINE bool _grow(size_type size, index_size_type front, index_size_type back)
    {
        if (size > MAX_CAPACITY)
        {
            // can't grow anymore.
            return false;
        }

        size_type newMask = size - 1;
        RingBuffer* pNewBuffer = gts::alignedNew<RingBuffer, GTS_NO_SHARING_CACHE_LINE_SIZE>(size, m_allocator);
        RingBuffer* pOldBuffer = m_pRingBuffer.load(gts::memory_order::relaxed);
        size_type oldMask = pOldBuffer ? pOldBuffer->mask : 0;

        // Copy the elements into the new buffer.
        pNewBuffer->mask = newMask;
        for (index_size_type ii = front; ii < back; ++ii)
        {
            (*pNewBuffer).vec[ii & newMask] = (*pOldBuffer).vec[ii & oldMask];
        }

        // Save the previous buffer since the consumer could still be using it.
        if (m_pQuienscentRingBuffer)
        {
            gts::alignedDelete(m_pQuienscentRingBuffer);
        }

        // Swap in the new buffer.
        m_pQuienscentRingBuffer = m_pRingBuffer.exchange(pNewBuffer);

        return true;
    }

    enum : index_size_type
    {
        //! The maximum capacity is half the range of the index type, such
        //! that when it wraps around, it will be zero at index zero. This
        //! prevents the dual state of full and empty.
        MAX_CAPACITY = (index_size_type)(std::numeric_limits<index_size_type>::max() / 2),
    };

private:

    //! The next item to pop.
    GTS_ALIGN(GTS_NO_SHARING_CACHE_LINE_SIZE) gts::Atomic<index_size_type> m_front;

    //! The last item index.
    GTS_ALIGN(GTS_NO_SHARING_CACHE_LINE_SIZE) gts::Atomic<index_size_type> m_back;

    //! The storage ring buffer.
    GTS_ALIGN(GTS_NO_SHARING_CACHE_LINE_SIZE) gts::Atomic<RingBuffer*> m_pRingBuffer;

    //! Synchronized access.
    gts::UnfairSpinMutex m_spinLock;

    //! Previous buffer for any threads 
    RingBuffer* m_pQuienscentRingBuffer; // todo: unused. remove.

    //! The allocator for the buffers.
    allocator_type m_allocator;
};

#ifdef GTS_MSVC
#pragma warning( pop )
#endif

} // namespace gts
