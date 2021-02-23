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

#include "gts/platform/Machine.h"
#include "gts/platform/Assert.h"
#include "gts/platform/Utils.h"
#include "gts/platform/Atomic.h"
#include "gts/platform/Thread.h"
#include "gts/synchronization/SpinMutex.h"
#include "gts/synchronization/Lock.h"
#include "gts/containers/AlignedAllocator.h"

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

namespace internal {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A multi-producer, single-consumer queue. Properties:
    - Unbound.
    - Linearizable.
    - Contiguous memory.
 * @tparam T
 *  The type stored in the container.
 * @tparam TMutex
 *  The mutex type that guards access to the container.
 * @tparam TAllocator
 *  The allocator used for T.
 * @tparam TSize
 *  The integral type used for the container size.
 */
template<
    typename T,
    typename TMutex     = UnfairSpinMutex<>,
    typename TAllocator = AlignedAllocator<GTS_NO_SHARING_CACHE_LINE_SIZE>>
class TicketQueueMPSC : private TAllocator
{
public:

    using value_type     = T;
    using mutex_type     = TMutex;
    using size_type      = size_t;
    using allocator_type = TAllocator;

public: // STRUCTORS

    explicit TicketQueueMPSC(size_t numQueues, allocator_type const& allocator = allocator_type());
    TicketQueueMPSC(size_t numQueues, size_type sizePow2, allocator_type const& allocator = allocator_type());
    ~TicketQueueMPSC();

    /**
     * Copy constructor.
     * @remark Not thread-safe.
     */
    TicketQueueMPSC(TicketQueueMPSC const& other);
    
    /**
     * Move constructor.
     * @remark Not thread-safe.
     */
    TicketQueueMPSC(TicketQueueMPSC&& other);

    /**
     * Copy assignment.
     * @remark Not thread-safe.
     */
    TicketQueueMPSC& operator=(TicketQueueMPSC const& other);

    /**
     * Move assignment.
     * @remark Not thread-safe.
     */
    TicketQueueMPSC& operator=(TicketQueueMPSC&& other);

public: // ACCESSORS

    /**
     * @return True of the queue is empty, false otherwise.
     * @remark Thread-safe. 
     */
    bool empty() const;

    /**
     * @return The number of elements in the queue.
     * @remark Thread-safe.
     */
    size_type size() const;

    /**
     * @return The capacity of the queue.
     * @remark Thread-safe, but may return a previous value of capacity.
     */
    size_type capacity() const;

    /**
     * @return This queue's allocator.
     * @remark Thread-safe.
     */
    allocator_type get_allocator() const;

public: // MUTATORS

    /**
     * Increases the capacity of the queue. Does nothing if 'sizePow2' < capacity.
     * @remark Thread-safe.
     */
    void reserve(size_type sizePow2);

    /**
     * Removes all elements from the queue.
     * @remark Thread-safe.
     */
    void clear();

    /**
     * Copies 'val' into the queue.
     * @return True if the pop push, false otherwise.
     * @remark Thread-safe.
     */
    bool tryPush(size_type ticket, const value_type& val);

    /**
     * Moves 'val' into the queue.
     * @return True if the pop push, false otherwise.
     * @remark Thread-safe.
     */
    bool tryPush(size_type ticket, value_type&& val);

    /**
     * Pops an element from the queue and copies it into 'out'.
     * @return True if the pop succeeded, false otherwise.
     * @remark Thread-safe.
     */
    bool tryPop(size_type ticket, value_type& out);

private:

    template<typename... TArgs>
    bool _insert(size_t ticket, TArgs&&... args);

    void _deepCopy(TicketQueueMPSC const& src);

    GTS_NO_INLINE bool _grow(size_type size, size_type front, size_type back);


    //! The maximum capacity is half the range of the index type, such
    //! that when it wraps around, it will be zero at index zero. This
    //! prevents the dual state of full and empty.
    static constexpr size_type MAX_CAPACITY = numericLimits<size_type>::max()  / 2;

private:

    //! A mask for an incoming ticket that removes the global index space.
    const size_type m_ticketMask;

    //! A shift for an incoming ticket that moves it into local index space.
    const size_type m_ticketShift;

    //! The next item to pop.
    size_type m_front;

    //! The last item index.
    size_type m_back;

    //! The mutex guard.
    mutable mutex_type m_mutex;

    //! The storage ring buffers.
    value_type* m_pRingBuffer;

    //! The size of m_pRingBuffer - 1.
    size_type m_mask;
};

} // namespace internal


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A multi-producer, multi-consumer queue. Properties:
    - Unbound.
    - Distributed to reduce contention.
 * @tparam T
 *  The type stored in the container.
 * @tparam TMutex
 *  The mutex type that guards access to the container.
 * @tparam TAllocator
 *  The allocator used for T.
 * @tparam TSize
 *  The integral type used for the container size.
 */
template<
    typename T,
    typename TMutex     = UnfairSpinMutex<>,
    typename TAllocator = AlignedAllocator<GTS_NO_SHARING_CACHE_LINE_SIZE>>
class QueueMPSC : private TAllocator
{
public:

    using queue_type     = internal::TicketQueueMPSC<T, TMutex, TAllocator>;
    using value_type     = typename queue_type::value_type;
    using mutex_type     = typename queue_type::mutex_type;
    using size_type      = typename queue_type::size_type;
    using allocator_type = typename queue_type::allocator_type;

public: // STRUCTORS

    /**
     * Constructs an empty container with 'numSubQueuesPow2' sub vectors and
     * with the given 'allocator'.
     * @remark
     *  Thread-safe.
     */
    explicit QueueMPSC(
        size_t numSubQueuesPow2 = Thread::getHardwareThreadCount(),
        allocator_type const& allocator = allocator_type());

    /**
     * Destructs the container. The destructors of the elements are called and the
     * used storage is deallocated.
     * @remark
     *  Not thread-safe.
     */
    ~QueueMPSC();

    /**
     * Copy constructor. Constructs the container with the copy of the contents
     * of 'other'.
     * @remark
     *  Not thread-safe.
     */
    QueueMPSC(QueueMPSC const& other);
    
    /**
     * Move constructor. Constructs the container with the contents of other
     * using move semantics. After the move, other is invalid.
     * @remark
     *  Not thread-safe.
     */
    QueueMPSC(QueueMPSC&& other);

    /**
     * Copy assignment operator. Replaces the contents with a copy of the
     * contents of 'other'.
     * @remark
     *  Not thread-safe.
     */
    QueueMPSC& operator=(QueueMPSC const& other);

    /**
     * Move assignment operator. Replaces the contents with those of other using
     * move semantics. After the move, other is invalid.
     * @remark
     *  Not thread-safe.
     */
    QueueMPSC& operator=(QueueMPSC&& other);

public: // ACCESSORS

    /**
     * @return True of the queue is empty, false otherwise.
     * @remark Not thread-safe. 
     */
    bool empty() const;

    /**
     * @return The number of elements in the queue.
     * @remark Not thread-safe.
     */
    size_type size() const;

    /**
     * @return The capacity of the queue.
     * @remark Not thread-safe.
     */
    size_type capacity() const;

    /**
     * @return This queue's allocator.
     * @remark Thread-safe.
     */
    allocator_type get_allocator() const;

public: // MUTATORS

    /**
     * Increases the capacity of the queue. Does nothing if 'sizePow2' < capacity.
     * @remark Not thread-safe.
     */
    void reserve(size_type sizePow2);

    /**
     * Removes all elements from the queue.
     * @remark Not thread-safe.
     */
    void clear();

    /**
     * Copies 'val' into the queue.
     * @return True if the pop push, false otherwise.
     * @remark Thread-safe.
     */
    bool tryPush(const value_type& val);

    /**
     * Moves 'val' into the queue.
     * @return True if the pop push, false otherwise.
     * @remark Thread-safe.
     */
    bool tryPush(value_type&& val);

    /**
     * Pops an element from the queue and copies it into 'out'.
     * @return True if the pop succeeded, false otherwise.
     * @remark Thread-safe.
     */
    bool tryPop(value_type& out);

private:

    template<typename... TArgs>
    void _constructQueues(TArgs&&... args);

    //! The maximum capacity is half the range of the index type, such
    //! that when it wraps around, it will be zero at index zero. This
    //! prevents the dual state of full and empty.
    static constexpr size_type MAX_CAPACITY = numericLimits<size_type>::max()  / 2;

    size_type index(size_type t)
    {
        return t & (m_numSubQueues - 1);
    }

private:

    //! The next item to pop.
    GTS_ALIGN(GTS_NO_SHARING_CACHE_LINE_SIZE) Atomic<size_type> m_front;

    //! The next item to push.
    GTS_ALIGN(GTS_NO_SHARING_CACHE_LINE_SIZE) Atomic<size_type> m_back;

    //! Distributed sub queues.
    queue_type* m_pSubQueues;

    //! The number of distributed sub queues.
    size_t m_numSubQueues;
};

#ifdef GTS_MSVC
#pragma warning( pop )
#endif

/** @} */ // end of ParallelContainers
/** @} */ // end of Containers

} // namespace gts

#include "QueueMPSC.inl"
