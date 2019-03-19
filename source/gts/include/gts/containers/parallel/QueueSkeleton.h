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

#include <algorithm>

#include "gts/platform/Machine.h"
#include "gts/platform/Assert.h"
#include "gts/platform/Utils.h"
#include "gts/platform/Atomic.h"
#include "gts/platform/Thread.h"
#include "gts/containers/Vector.h"
#include "gts/containers/AlignedAllocator.h"

#ifdef GTS_MSVC
#pragma warning( push )
#pragma warning( disable : 4324) // alignment padding warning
#endif

namespace gts {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A skeleton for all parallel queue implementations.
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
    template <typename, typename> class TStorage,
    typename TAllocator>
class QueueSkeleton
{
public:

    using value_type      = T;
    using size_type       = uint32_t;
    using index_size_type = uint32_t;
    using allocator_type  = typename TAllocator::template rebind<value_type>::other;
    using storage_type    = TStorage<value_type, allocator_type>;

    struct RingBuffer
    {
        RingBuffer(uint32_t n, allocator_type const& allocator)
            : vec(n, allocator)
        {}

        storage_type vec;
        size_type mask;
    };

public: // STRUCTORS

    explicit QueueSkeleton(allocator_type const& allocator = allocator_type());
    ~QueueSkeleton();

    /**
     * Copy constructor.
     * @remark Not thread-safe.
     */
    QueueSkeleton(QueueSkeleton const& other);
    
    /**
     * Move constructor.
     * @remark Not thread-safe.
     */
    QueueSkeleton(QueueSkeleton&& other);

    /**
     * Copy assignment.
     * @remark Not thread-safe.
     */
    QueueSkeleton& operator=(QueueSkeleton const& other);

    /**
     * Move assignment.
     * @remark Not thread-safe.
     */
    QueueSkeleton& operator=(QueueSkeleton&& other);

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
    template<typename TMutex>
    void clear(TMutex& mutex);

    /**
     * A multi-producer-safe push operation.
     */
    template<typename TMutex, typename TGrowMutex>
    bool tryPush(const value_type& val, TMutex& mutex, TGrowMutex& growMutex);

    /**
     * A multi-producer-safe push operation.
     */
    template<typename TMutex, typename TGrowMutex>
    bool tryPush(value_type&& val, TMutex& mutex, TGrowMutex& growMutex);

    /**
     * A multi-consumer-safe pop operation.
     * @return True if the pop succeeded, false otherwise.
     */
    template<typename TMutex>
    bool tryPop(value_type& out, TMutex& mutex);

    /**
     * A multi-consumer-safe pop operation.
     * @return True if the pop succeeded, false otherwise.
     */
    template<typename TMutex, typename TTag>
    bool tryPop(value_type& out, TMutex& mutex, TTag const& tag);

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

    //! The next item to pop.
    GTS_ALIGN(GTS_NO_SHARING_CACHE_LINE_SIZE) gts::Atomic<index_size_type> m_front;

    //! The last item index.
    GTS_ALIGN(GTS_NO_SHARING_CACHE_LINE_SIZE) gts::Atomic<index_size_type> m_back;

    //! The storage ring buffers.
    GTS_ALIGN(GTS_NO_SHARING_CACHE_LINE_SIZE) gts::Atomic<RingBuffer*> m_pRingBuffer;
    RingBuffer* m_pQuienscentRingBuffer;
    allocator_type m_allocator;
};

#include "QueueSkeleton.inl"

#ifdef GTS_MSVC
#pragma warning( pop )
#endif

} // namespace gts
