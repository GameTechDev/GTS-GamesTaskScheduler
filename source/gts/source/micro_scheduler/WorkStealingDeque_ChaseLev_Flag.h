/*******************************************************************************
 * Copyright 2017 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License.  You may obtain a copy
 * of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 ******************************************************************************/
#pragma once

#include "gts/platform/Assert.h"
#include "gts/platform/Utils.h"
#include "gts/platform/Atomic.h"
#include "gts/platform/Thread.h"
#include "gts/containers/Vector.h"
#include "gts/containers/AlignedAllocator.h"
#include "gts/micro_scheduler/Task.h"

namespace gts {

class Task;

#ifdef GTS_MSVC
#pragma warning( push )
#pragma warning( disable : 4324) // alignment padding warning
#endif

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A unbounded, wait-free, single-producer multi-consumer queue. Before using,
 *  client must call WorkStealingDeque::init to set the bounds.
 * @details
 *  Derivative of:
 *  Nhat Minh Le, Antoniu Pop, Albert Cohen, and Francesco Zappa Nardelli. 2013.
 *  Correct and efficient work-stealing for weak memory models. In Proceedings of
 *  the 18th ACM SIGPLAN symposium on Principles and practice of parallel
 *  programming (PPoPP ’13). Association for Computing Machinery, New York, NY,
 *  USA, 69–80. DOI: https://doi.org/10.1145/2442516.2442524.
 */
class WorkStealingDeque
{
private:

    struct RingBuffer
    {
        Task** vec;
        size_t mask;

        RingBuffer(size_t size)
        {
            vec = alignedVectorNew<Task*, GTS_NO_SHARING_CACHE_LINE_SIZE>(size);
            mask = size - 1;
        }

        ~RingBuffer()
        {
            alignedVectorDelete(vec, mask + 1);
        }

        Task*& operator[](size_t index)
        {
            return vec[index & mask];
        }
    };

public: // STRUCTORS

    //--------------------------------------------------------------------------
    GTS_NO_INLINE WorkStealingDeque()
        : m_isEmpty(true)
        // Start at 1 to avoid the integer wrap-around on initial tryTake.
        , m_back(1)
        , m_front(1)
        , m_ringBuffer(nullptr)
    {
        size_t b = m_back.load(memory_order::acquire);
        size_t f = m_front.load(memory_order::acquire);
        _grow(1, b, f);
    }

    //--------------------------------------------------------------------------
    GTS_INLINE WorkStealingDeque(WorkStealingDeque const& other)
        : m_isEmpty(other.m_isEmpty.load(memory_order::relaxed)) 
        , m_back(other.m_back.load(memory_order::relaxed))
        , m_front(other.m_front.load(memory_order::relaxed))
        , m_ringBuffer(std::move(other.m_ringBuffer.load(memory_order::relaxed)))
    {}

    //--------------------------------------------------------------------------
    GTS_INLINE ~WorkStealingDeque()
    {
        _cleanupOldBuffers(m_oldRingBuffers);

        if (m_ringBuffer.load(memory_order::relaxed) != nullptr)
        {
            alignedDelete(m_ringBuffer.load(memory_order::relaxed));
        }
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
        return m_isEmpty.load(memory_order::relaxed);
    }

    //--------------------------------------------------------------------------
    /**
     * @return True of the queue is empty, false otherwise.
     * @remark
     *  Thread-safe.
     */
    GTS_INLINE bool empty() const
    {
        return size() == 0;
    }

    //--------------------------------------------------------------------------
    /**
     * @return The number of elements in the queue.
     * @remark
     *  Thread-safe.
     */
    GTS_INLINE size_t size() const
    {
        if(m_isEmpty.load(memory_order::relaxed))
        {
            return 0;
        }

        size_t f = m_front.load(memory_order::acquire);
        size_t b = m_back.load(memory_order::acquire);
        return b - f;
    }

    //--------------------------------------------------------------------------
    /**
     * @return The capacity of the queue.
     * @remark
     *  Thread-safe.
     */
    GTS_INLINE size_t capacity() const
    {
        RingBuffer* pRingBuffer = m_ringBuffer.load(memory_order::relaxed);
        return (size_t)pRingBuffer->mask + 1;
    }

public: // MUTATORS

    //--------------------------------------------------------------------------
    /**
     * Removes all elements from the queue.
     * @remark
     *  Not thread-safe.
     */
    GTS_INLINE void clear()
    {
        m_front.store(1, memory_order::relaxed);
        m_back.store(1, memory_order::relaxed);
    }

    //--------------------------------------------------------------------------
    /**
     * A single-producer-safe push operation. Pushes pTask to the back.
     * @param pTask
     *  The Task to push.
     * @return
     *  True if pTask was pushed, false otherwise.
     * @remark
     *  Thread-safe.
     */
    GTS_NO_INLINE bool tryPush(Task* pTask)
    {
        // Sync with front from other consumers. Back does not matter since there
        // is only one producer.
        size_t b       = m_back.load(memory_order::relaxed);
        size_t f       = m_front.load(memory_order::acquire);
        bool isEmpty   = m_isEmpty.load(memory_order::relaxed);
        RingBuffer* pRingBuffer = m_ringBuffer.load(memory_order::relaxed);

        // If the buffer is at capacity, try to grow it.
        if (!isEmpty && std::make_signed<size_t>::type(b - f) >= std::make_signed<size_t>::type(pRingBuffer->mask + 1))
        {
            if (!_grow(capacity() * 2, f, b))
            {
                return false;
            }
            pRingBuffer = m_ringBuffer.load(memory_order::relaxed);
        }

        // Store the new value in the buffer.
        (*pRingBuffer)[b] = pTask;

        if (isEmpty)
        {
            // Let consumers know we are no longer empty.
            m_isEmpty.store(false, memory_order::relaxed);
        }

        // Notify the consumers that the new value exists.
        m_back.store(b + 1, memory_order::release);

        return true;
    }

    //--------------------------------------------------------------------------
    /**
     * A single-producer-safe pop operation. Pops pTask from the back.
     * @param pTask
     *  The popped task.
     * @return True if the pop succeeded, false otherwise.
     * @remark
     *  Thread-safe.
     */
    GTS_NO_INLINE bool tryPop(Task*& pTask)
    {
        bool result = true;

        size_t b                = m_back.load(memory_order::relaxed) - 1;
        RingBuffer* pRingBuffer = m_ringBuffer.load(memory_order::relaxed);

        m_back.store(b, memory_order::relaxed);
        GTS_MFENCE(); // notify thieves of back position.

        size_t f = m_front.load(memory_order::relaxed);

        if (b < f)
        {
            // Mark as empty.
            m_isEmpty.store(true, memory_order::relaxed);

            pTask  = nullptr;
            result = false;
            m_back.store(b + 1, memory_order::relaxed);
        }
        else
        {
            pTask = (*pRingBuffer)[b];
            if (b == f)
            {
                if (!m_front.compare_exchange_strong(f, f + 1, memory_order::seq_cst, memory_order::relaxed))
                {
                    pTask = nullptr;
                    result = false;
                }

                // Mark as empty.
                m_isEmpty.store(true, memory_order::relaxed);

                m_back.store(b + 1, memory_order::relaxed);
            }
        }

        return result;
    }

    //--------------------------------------------------------------------------
    /**
     * A multi-consumer-safe pop operation. Pops pTask from the front.
     * @param pTask
     *  The stolen task.
     * @return True if the steal succeeded, false otherwise.
     * @remark
     *  Thread-safe.
     */
    GTS_NO_INLINE bool trySteal(Task*& out)
    {
        while (true)
        {
            if (m_isEmpty.load(memory_order::acquire))
            {
                out = nullptr;
                return false;
            }

            size_t f = m_front.load(memory_order::acquire);

            GTS_MFENCE(); // sync with take.

            size_t b = m_back.load(memory_order::acquire);

            // (1) If the queue is empty, quit.
            if (b <= f)
            {
                out = nullptr;
                return false;
            }

            // Get element before releasing change to front.
            RingBuffer* pRingBuffer = m_ringBuffer.load(memory_order::consume);
            out = (*pRingBuffer)[f];

            // (2) Race against tryPop with other consumers
            if (!m_front.compare_exchange_strong(f, f + 1, memory_order::seq_cst, memory_order::relaxed))
            {
                GTS_SPECULATION_FENCE();

                // race lost, try again
                continue;
            }

            // Race won. Steal successful
            return true;
        }
    }

private:

    //--------------------------------------------------------------------------
    GTS_NO_INLINE bool _grow(size_t size, size_t front, size_t back)
    {
        if (size > MAX_CAPACITY)
        {
            // can't grow anymore.
            return false;
        }

        RingBuffer* pNewBuffer = alignedNew<RingBuffer, GTS_NO_SHARING_CACHE_LINE_SIZE>(size);
        RingBuffer* pOldBuffer = m_ringBuffer.load(memory_order::relaxed);

        // Copy the elements into the new buffer.
        for (size_t ii = front; ii < back; ++ii)
        {
            (*pNewBuffer)[ii] = (*pOldBuffer)[ii];
        }

        // Swap in the new buffer.
        m_oldRingBuffers.push_back(m_ringBuffer.load(memory_order::relaxed));
        m_ringBuffer.store(pNewBuffer, memory_order::relaxed);

        return true;
    }


    //--------------------------------------------------------------------------
    static GTS_INLINE void _cleanupOldBuffers(Vector<RingBuffer*>& oldBuffers)
    {
        for (size_t ii = 0, len = oldBuffers.size(); ii < len; ++ii)
        {
            alignedDelete(oldBuffers[ii]);
        }
        oldBuffers.clear();
    }

    enum : size_t
    {
        //! The maximum capacity is half the range of the index type, such
        //! that when it wraps around, it will be zero at index zero. This
        //! prevents the dual state of full and empty.
        MAX_CAPACITY = (size_t)(numericLimits<size_t>::max / 2),
    };

private:

    //! Flag the deque as empty.
    GTS_ALIGN(GTS_NO_SHARING_CACHE_LINE_SIZE) Atomic<bool> m_isEmpty;

    //! The last item index.
    GTS_ALIGN(GTS_NO_SHARING_CACHE_LINE_SIZE) Atomic<size_t> m_back;

    //! The next item to pop.
    Atomic<size_t> m_front;

    //! The storage ring buffer.
    Atomic<RingBuffer*> m_ringBuffer;

    //! All previous ring buffers. Must save in-case they are still in use.
    Vector<RingBuffer*> m_oldRingBuffers;
};

#ifdef GTS_MSVC
#pragma warning( pop )
#endif

} // namespace gts
