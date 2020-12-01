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

namespace internal {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
struct alignas(GTS_NO_SHARING_CACHE_LINE_SIZE) Front
{
    using BackoffType = Backoff<>;

    enum class LockState : uint32_t
    {
        READY,
        LOCKED,
        EMPTY
    };

    //! The next item to pop.
    Atomic<size_t> m_front = 1;

    //! Synchronized access.
    Atomic<LockState> m_lock = LockState::EMPTY;

    //--------------------------------------------------------------------------
    GTS_INLINE bool empty() const
    {
        return m_lock.load(memory_order::acquire) == LockState::EMPTY;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE void lockProducer()
    {
        GTS_TRACE_SCOPED_ZONE_P1(analysis::CaptureMask::THREAD_PROFILE, analysis::Color::DarkRed, "LOCKING", this);

        BackoffType backoff;
        while (true)
        {
            // Test and test-and-set.
            if (m_lock.load(gts::memory_order::acquire) != LockState::LOCKED &&
                // Only perform the expensive exchange if the lock seems available.
                m_lock.exchange(LockState::LOCKED, memory_order::acq_rel) != LockState::LOCKED)
            {
                return;
            }
            GTS_SPECULATION_FENCE();
        }
    }

    //--------------------------------------------------------------------------
    GTS_INLINE bool lockConsumer()
    {
        GTS_TRACE_SCOPED_ZONE_P1(analysis::CaptureMask::THREAD_PROFILE, analysis::Color::DarkRed, "LOCKING", this);

        BackoffType backoff;
        while (true)
        {
            if (m_lock.load(gts::memory_order::acquire) == LockState::EMPTY)
            {
                // Quit on empty so that there is no race for the last element.
                return false;
            }

            // Test and test-and-set.
            LockState exectedLockState = LockState::READY;
            if (m_lock.load(gts::memory_order::acquire) == exectedLockState &&
                // Only perform the expensive compare exchange if the lock seems available.
                m_lock.compare_exchange_weak(exectedLockState, LockState::LOCKED, memory_order::acq_rel, memory_order::relaxed))
            {
                break;
            }
            GTS_SPECULATION_FENCE();
        }

        return true;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE void unlockAsEmpty()
    {
        m_lock.store(LockState::EMPTY, memory_order::release);
    }

    //--------------------------------------------------------------------------
    GTS_INLINE void unlock()
    {
        m_lock.store(LockState::READY, memory_order::release);
    }

    //--------------------------------------------------------------------------
    GTS_INLINE void notifyIfEmpty()
    {
        if(m_lock.load(gts::memory_order::acquire) == LockState::EMPTY)
        {
            m_lock.store(LockState::READY, memory_order::release);
        }
    }
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
struct alignas(GTS_NO_SHARING_CACHE_LINE_SIZE) Back
{
    //! The last item index.
    Atomic<size_t> m_back = 1;

    //! The number of elements in the ring buffer.
    size_t m_ringBufferSize = 0;

    //! The storage ring buffer.
    Task** m_pRingBuffer = nullptr;
};

} // namespace internal

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A unbound, spin-locked, single-producer multi-consumer queue with stealing.
 * @details
 *  Derivative of:
 *  - Matteo Frigo, Charles E. Leiserson, and Keith H. Randall. 1998. The
 *    implementation of the Cilk-5 multithreaded language. In Proceedings of the
 *    ACM SIGPLAN 1998 conference on Programming language design and implementation
 *    (PLDI ’98). Association for Computing Machinery, New York, NY, USA,
 *    212–223. DOI: https://doi.org/10.1145/277650.277725
 *  - https://github.com/intel/tbb
 */
class WorkStealingDeque : public internal::Front, public internal::Back
{
public: // STRUCTORS

    //--------------------------------------------------------------------------
    GTS_INLINE WorkStealingDeque()
    {
        _grow(1, 1, 1);
    }

    //--------------------------------------------------------------------------
    GTS_INLINE ~WorkStealingDeque()
    {
        alignedVectorDelete(this->m_pRingBuffer, this->m_ringBufferSize);
    }

    //--------------------------------------------------------------------------
    /**
     * Non-thread-safe copy.
     */
    GTS_INLINE WorkStealingDeque(WorkStealingDeque const& other)
    {
        this->m_front.store(other.m_front.load(memory_order::relaxed), memory_order::relaxed);
        this->m_lock.store(other.m_lock.load(memory_order::relaxed), memory_order::relaxed);
        this->m_back.store(other.m_back.load(memory_order::relaxed), memory_order::relaxed);
        this->m_ringBufferSize = other.m_ringBufferSize;
        this->m_pRingBuffer    = other.m_pRingBuffer;
    }

public: // ACCESSORS

    //--------------------------------------------------------------------------
    /**
    * @return True of the queue has been locally marked as empty, false otherwise.
    * @remark
    *  Thread-safe.
    */
    GTS_INLINE bool locallyEmtpy() const
    {
        return m_lock.load(memory_order::acquire) == LockState::EMPTY;
    }

    //--------------------------------------------------------------------------
    /**
     * @return True if the queue is empty, false otherwise.
     */
    GTS_INLINE bool empty() const
    {
        if(locallyEmtpy())
        {
            return true;
        }
        else
        {
            return size() == 0;
        }
    }

    //--------------------------------------------------------------------------
    /**
     * @return The number of elements in the queue.
     */
    GTS_INLINE size_t size() const
    {
        size_t b = this->m_back.load(memory_order::acquire);
        size_t f = this->m_front.load(memory_order::acquire);

        return (size_t)(b - f);
    }

    //--------------------------------------------------------------------------
    /**
     * @return The capacity of the queue.
     */
    GTS_INLINE size_t capacity() const
    {
        return this->m_ringBufferSize;
    }

public: // MUTATORS

    //--------------------------------------------------------------------------
    /**
     * Removes all elements from the queue.
     */
    GTS_INLINE void clear()
    {
        this->lockProducer();
        this->m_front.store(1, memory_order::relaxed);
        this->m_back.store(1, memory_order::relaxed);
        this->unlockAsEmpty();
    }

    //--------------------------------------------------------------------------
    /**
     * A single-producer-safe push operation.
     */
    GTS_NO_INLINE bool tryPush(Task* const& pTask)
    {
        size_t f = this->m_front.load(memory_order::acquire);
        size_t b = this->m_back.load(memory_order::relaxed);

        // If the buffer is at capacity, try to grow it.
        size_t cap = capacity();
        size_t size = 0;
        if (b + 1 < f)
        {
            size = cap - (f - (b + 1));
        }
        else
        {
            size = (b + 1) - f;
        }

        if (size > cap)
        {
            // Growth needs to be protected on push, so that thieves can't 
            // reference a stale ring buffer.
            this->lockProducer();

            if (!_grow(cap * 2, f, b))
            {
                this->unlock();
                return false;
            }

            this->unlock();
        }

        // Store the new value in the buffer.
        this->m_pRingBuffer[b & (this->m_ringBufferSize - 1)] = pTask;

        // Notify the consumers that the new value exists.
        this->m_back.store(b + 1, memory_order::release);

        this->notifyIfEmpty();

        return true;
    }

    //--------------------------------------------------------------------------
    /**
     * A single-producer-safe pop operation.
     * @return True if the pop succeeded, false otherwise.
     */
    GTS_NO_INLINE bool tryPop(Task*& out)
    {
        // Mark the item as consumed.
        size_t b = this->m_back.load(memory_order::relaxed) - 1;
        this->m_back.store(b, memory_order::relaxed);

        this->lockProducer(); // lock also syncs new m_back value.

        bool hasTask = true;

        size_t f = this->m_front.load(memory_order::relaxed);

        // Get element.
        out = this->m_pRingBuffer[b & (this->m_ringBufferSize - 1)];

        // If the queue is empty, quit.
        if(b < f)
        {
            // restore back index.
            this->m_back.store(b + 1, memory_order::relaxed);
            this->unlockAsEmpty();
            out = nullptr;
            hasTask = false;
        }
        else if (b == f)
        {
            this->unlockAsEmpty();
        }
        else
        {
            this->unlock();
        }

        return hasTask;
    }

    //--------------------------------------------------------------------------
    /**
     * A multi-consumer-safe steal operation.
     * @return True if the pop succeeded, false otherwise.
     */
    GTS_NO_INLINE bool trySteal(Task*& out)
    {
        if (!this->lockConsumer())
        {
            return false;
        }

        // Sync with front from other consumers. It's ok if back is stale because
        // it will just look like the queue is empty.
        size_t f = this->m_front.load(memory_order::acquire);
        size_t b = this->m_back.load(memory_order::acquire);

        // If the queue is empty, quit.
        if (b <= f)
        {
            this->unlock();
            return false;
        }

        // Get element.
        out = this->m_pRingBuffer[f & (this->m_ringBufferSize - 1)];

        // Consume the item.
        this->m_front.store(f + 1, memory_order::relaxed);

        this->unlock();
        return true;
    }

private:

    //------------------------------------------------------------------------------
    GTS_NO_INLINE bool _grow(size_t size, size_t front, size_t back)
    {
        if (size > MAX_CAPACITY)
        {
            // can't grow anymore.
            return false;
        }

        size_t newMask     = size - 1;
        Task** pNewBuffer  = alignedVectorNew<Task*, GTS_NO_SHARING_CACHE_LINE_SIZE>(size);
        Task** pOldBuffer  = this->m_pRingBuffer;
        size_t oldMask     = pOldBuffer ? (this->m_ringBufferSize - 1) : 0;

        // Copy the elements into the new buffer.
        for (size_t ii = front; ii < back; ++ii)
        {
            size_t newIdx      = ii & newMask;
            size_t oldIdx      = ii & oldMask;
            pNewBuffer[newIdx] = pOldBuffer[oldIdx];
        }

        // Save the new elements.
        this->m_ringBufferSize = size;
        this->m_pRingBuffer    = pNewBuffer;

        // Delete the old buffer.
        alignedVectorDelete(pOldBuffer, oldMask + 1);

        return true;
    }

    enum : size_t
    {
        //! The maximum capacity is half the range of the index type, such
        //! that when it wraps around, it will be zero at index zero. This
        //! prevents the dual state of full and empty.
        MAX_CAPACITY = (size_t)(numericLimits<size_t>::max / 2),
    };
};

#ifdef GTS_MSVC
#pragma warning( pop )
#endif

} // namespace gts
