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

#include <cstdint>
#include <atomic>
#include <vector>
#include <memory>

#include "gts/platform/Assert.h"
#include "gts/platform/Utils.h"

namespace gts {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A bounded, single-threaded ring buffer. Before using, client must call RingBuffer::init
 *  to set the bounds.
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
    template <typename, typename> class TStorage = std::vector,
    typename TAllocator = std::allocator<T>>
class RingBuffer
{
public:

    using value_type      = T;
    using size_type       = uint32_t;
    using index_size_type = uint32_t;
    using allocator_type  = TAllocator;
    using storage_type    = TStorage<value_type, allocator_type>;

public: // STRUCTORS

    RingBuffer();
    ~RingBuffer();

    /**
     * Non-thread-safe copy.
     */
    RingBuffer(RingBuffer const& other);
    
    /**
     * Non-thread-safe move.
     */
    RingBuffer(RingBuffer&& other);

    /**
     * Non-thread-safe move assign.
     */
    RingBuffer& operator=(RingBuffer&& other);

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
     * Clear all the data in the queue and resizes its storage backing.
     * @param powOf2
     *  The size of the storage backing. It must be a power of 2. If not a
     *  power of 2, the queue will resize the value to the next power of 2.
     */
    void init(size_type powOf2);

    /**
     * Removes all elements from the queue.
     */
    void clear();

    /**
     * A non-thread-safe push operation.
     * @return True if the push succeeded, false if the buffer is at capacity.
     */
    bool tryPush(const value_type& val);

    /**
     * A non-thread-safe operation.
     * @return True if the push succeeded, false if the buffer is at capacity.
     */
    bool tryPush(value_type&& val);

    /**
     * A non-thread-safe pop operation.
     * @return True if the pop succeeded, false if the buffer is empty.
     */
    bool tryPop(value_type& out);

private:

    enum : index_size_type
    {
        //! The maximum capacity is half the range of the index type, such
        //! that when it wraps around, it will be zero at index zero. This
        //! prevents the dual state of full and empty.
        MAX_CAPACITY = (index_size_type)(UINT32_MAX / 2)
    };

private:

    //! The next item to pop.
    index_size_type m_front;

    //! The last item index.
    index_size_type m_back;

    //! The circular storage buffer.
    std::vector<value_type, allocator_type> m_ringBuffer;
    size_type m_mask;
};

#include "RingBuffer.inl"

} // namespace gts
