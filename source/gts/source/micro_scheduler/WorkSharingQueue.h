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

#include "gts/containers/parallel/QueueSkeleton.h"

#ifdef GTS_MSVC
#pragma warning( push )
#pragma warning( disable : 4324) // alignment padding warning
#endif

namespace gts {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A unbound, lock-free multi-producer multi-consumer queue.
 * @tparam T
 *  The type stored in the container.
 * @tparam TStorage
 *  The storage backing for the container. Must be a random access container,
 *  such as std::vector or std::deque.
 * @tparam TAllocator
 *  The allocator used by the storage backing.
 */
template<
    typename T,
    template <typename, typename> class TStorage = gts::Vector,
    typename TAllocator = gts::AlignedAllocator<T, GTS_NO_SHARING_CACHE_LINE_SIZE>>
class WorkSharingQueue
{
public:

    using interal_queue   = QueueSkeleton<T, TStorage, TAllocator>;

    using value_type      = typename interal_queue::value_type;
    using size_type       = typename interal_queue::size_type;
    using index_size_type = typename interal_queue::index_size_type;
    using allocator_type  = typename interal_queue::allocator_type;
    using storage_type    = typename interal_queue::storage_type;

public: // STRUCTORS

    WorkSharingQueue(allocator_type const& allocator = allocator_type());
    ~WorkSharingQueue();

    /**
     * Copy constructor.
     * @remark Not thread-safe.
     */
    WorkSharingQueue(WorkSharingQueue const& other);
    
    /**
     * Move constructor.
     * @remark Not thread-safe.
     */
    WorkSharingQueue(WorkSharingQueue&& other);

    /**
     * Copy assignment.
     * @remark Not thread-safe.
     */
    WorkSharingQueue& operator=(WorkSharingQueue const& other);

    /**
     * Move assignment.
     * @remark Not thread-safe.
     */
    WorkSharingQueue& operator=(WorkSharingQueue&& other);

public: // ACCESSORS

    /**
     * @return True of the queue is empty, false otherwise.
     */
    bool empty() const;

    /**
     * @return The number of elements in the queue.
     */
    size_type size() const;

    /**
     * @return The capacity of the queue.
     */
    size_type capacity() const;

public: // MUTATORS

    /**
     * Removes all elements from the queue.
     * @remark Not thread-safe.
     */
    void clear();

    /**
     * A multi-producer-safe push operation.
     */
    bool tryPush(const value_type& val);

    /**
     * A multi-producer-safe push operation.
     */
    bool tryPush(value_type&& val);

    /**
     * A multi-consumer-safe pop operation.
     * @return True if the pop succeeded, false otherwise.
     */
    bool tryPop(value_type& out, uintptr_t isolationTag = 0);

private:

    GTS_INLINE bool _grow(size_type size, index_size_type front, index_size_type back);

    enum : index_size_type
    {
        //! The maximum capacity is half the range of the index type, such
        //! that when it wraps around, it will be zero at index zero. This
        //! prevents the dual state of full and empty.
        MAX_CAPACITY = (index_size_type)(UINT32_MAX / 2)
    };

private:

    interal_queue m_queue;
    UnfairSpinMutex m_accessorMutex;
};

#include "WorkSharingQueue.inl"

#ifdef GTS_MSVC
#pragma warning( pop )
#endif

} // namespace gts
