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
#include "gts/containers/AlignedAllocator.h"

#include <initializer_list>

namespace gts {

/** 
 * @addtogroup Containers
 * @{
 */

/** 
 * @addtogroup Serial
 * @{
 */

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A re-sizable ring buffer ADT.
 * @tparam T
 *  The element type stored in the container.
 * @tparam TAllocator
 *  The allocator used by the storage backing.
 */
template<
    typename T,
    typename TAllocator = AlignedAllocator<GTS_NO_SHARING_CACHE_LINE_SIZE>
>
class RingDeque : private TAllocator
{
public:

    using size_type = int64_t;
    using value_type = T;
    using allocator_type = TAllocator;

public: // STRUCTORS:

    /**
     * @brief
     *  Destructs the container. The destructors of the elements are called and the
     *  used storage is deallocated.
     */
    ~RingDeque()
    {
        clear();
        allocator_type::deallocate(m_pRingBuffer, (size_t)m_capacity);
    }

    /**
     * @brief
     *  Constructs an empty container with the given 'allocator'.
     */
    explicit RingDeque(const allocator_type& allocator = allocator_type())
        : allocator_type(allocator)
        , m_front(0)
        , m_back(0)
        , m_pRingBuffer(nullptr)
        , m_capacity(0)
    {}

    /**
     * @brief
     *  Copy constructor. Constructs the container with the copy of the contents
     *  of 'other'.
     */
    RingDeque(RingDeque const& other)
        : allocator_type(other.get_allocator())
        , m_front(other.m_front)
        , m_back(other.m_back)
        , m_pRingBuffer(nullptr)
        , m_capacity(0)
    {
        _deepCopy(other);
    }

    /**
     * @brief
     *  Move constructor. Constructs the container with the contents of other
     *  using move semantics. After the move, other is invalid.
     */
    RingDeque(RingDeque&& other)
        : allocator_type(std::move(other.get_allocator()))
        , m_front(other.m_front)
        , m_back(other.m_back)
        , m_pRingBuffer(other.m_pRingBuffer)
        , m_capacity(other.m_capacity)
    {
        other.m_back = 0;
        other.m_front = 0;
        other.m_pRingBuffer = nullptr;
        other.m_capacity = 0;
        other._cleanup();
    }

    /**
     * @brief
     *  Copy assignment operator. Replaces the contents with a copy of the
     *  contents of 'other'.
     */
    RingDeque& operator=(RingDeque const& other)
    {
        if (this != &other)
        {
            clear();
            allocator_type::deallocate(m_pRingBuffer, m_capacity);
            (allocator_type)*this = other.get_allocator();
            _deepCopy(other);
        }
        return *this;
    }

    /**
     * @brief
     * Move assignment operator. Replaces the contents with those of other using
     * move semantics. After the move, other is invalid.
     */
    RingDeque& operator=(RingDeque&& other)
    {
        if (this != &other)
        {
            clear();
            allocator_type::deallocate(m_pRingBuffer, m_capacity);

            (allocator_type)*this = std::move(other.get_allocator());
            m_front = other.m_front;
            m_back = other.m_back;
            m_pRingBuffer = other.m_pRingBuffer;
            m_capacity = other.m_capacity;

            other.m_back = 0;
            other.m_front = 0;
            other.m_pRingBuffer = nullptr;
            other.m_capacity = 0;
        }
    }

public: // CAPACITY:

    /**
     * @brief
     *  Checks if the contains has no elements.
     * @returns
     *  True if empty, false otherwise.
     */
    bool empty() const
    {
        return size() == 0;
    }

    /**
     * @brief
     *  Gets the number of elements in the container.
     * @returns
     *  The size.
     */
    size_type size() const
    {
        return m_back - m_front;
    }

    /**
     * @brief
     *  Gets the number of elements that can be held in the currently
     *  allocated storage.
     * @returns
     *  The capacity.
     */
    size_type capacity() const
    {
        return m_capacity;
    }

public: // ELEMENT ACCESS:

    /**
     * @brief
     *  Gets the first element.
     * @returns
     *  An element.
     */
    value_type const& front() const
    {
        GTS_ASSERT(!empty());
        return m_pRingBuffer[m_front & (m_capacity - 1)];
    }

    /**
     * @brief
     *  Gets the first element.
     * @returns
     *  An element.
     */
    value_type& front()
    {
        GTS_ASSERT(!empty());
        return m_pRingBuffer[m_front & (m_capacity - 1)];
    }

    /**
     * @brief
     *  Gets the last element.
     * @returns
     *  An element.
     */
    value_type const& back() const
    {
        GTS_ASSERT(!empty());
        return m_pRingBuffer[(m_back - 1) & (m_capacity - 1)];
    }

    /**
     * @brief
     *  Gets the last element.
     * @returns
     *  An element.
     */
    value_type& back()
    {
        GTS_ASSERT(!empty());
        return m_pRingBuffer[(m_back - 1) & (m_capacity - 1)];
    }

    /**
     * @brief
     *  Gets the element at \a pos.
     * @returns
     *  An element.
     */
    value_type const& operator[](size_type pos) const
    {
        GTS_ASSERT(!empty());
        return m_pRingBuffer[pos & (m_capacity - 1)];
    }

    /**
     * @brief
     *  Gets the element at \a pos.
     * @returns
     *  An element.
     */
    value_type& operator[](size_type pos)
    {
        GTS_ASSERT(!empty());
        return m_pRingBuffer[pos & (m_capacity - 1)];
    }

    /**
     * @brief
     *  Get this RingDeque's allocator.
     * @returns
     *  The allocator.
     */
    allocator_type get_allocator() const
    {
        return (allocator_type)*this;
    }

public: // MUTATORS:

    /**
     * Inserts a copy of 'val' at the back.
     */
    void push_back(value_type const& val)
    {
        emplace_back(val);
    }

    /**
     * Inserts 'val' at the back.
     */
    void push_back(value_type&& val)
    {
        emplace_back(std::move(val));
    }

    /**
     * Inserts a copy of 'val' at the front.
     */
    void push_front(value_type const& val)
    {
        emplace_front(val);
    }

    /**
     * Inserts 'val' at the front.
     */
    void push_front(value_type&& val)
    {
        emplace_front(std::move(val));
    }

    /**
     * Construct a new element at the back.
     */
    template<typename... TArgs>
    void emplace_back(TArgs&&... args)
    {
        if (size() == capacity())
        {
            _grow(gtsMax(MIN_CAPACITY, capacity() * 2));
        }
        size_type idx = m_back & (m_capacity - 1);
        allocator_type::construct(m_pRingBuffer + idx, std::forward<TArgs>(args)...);
        ++m_back;
    }

    /**
     * Construct a new element at the front.
     */
    template<typename... TArgs>
    void emplace_front(TArgs&&... args)
    {
        if (size() == capacity())
        {
            _grow(gtsMax(MIN_CAPACITY, capacity() * 2));
        }
        --m_front;
        size_type idx = m_front & mask;
        allocator_type::construct(m_pRingBuffer + idx, std::forward<TArgs>(args)...);
    }

    /**
     * Removes and destroys the element at the end.
     */
    void pop_back()
    {
        GTS_ASSERT(!empty());
        allocator_type::destroy(&back());
        --m_back;
    }

    /**
     * Removes and destroys the element at the front.
     */
    void pop_front()
    {
        GTS_ASSERT(!empty());
        allocator_type::destroy(&front());
        ++m_front;
    }

    /**
     * Increases the capacity of the queue. Does nothing if 'sizePow2' < capacity.
     * @remark Not thread-safe.
     */
    void reserve(size_type sizePow2)
    {
        if (sizePow2 > capacity())
        {
            _grow(nextPow2(uint64_t(sizePow2)));
        }
    }

    /**
     * Removes all elements but does not destroy the underlying array.
     */
    void clear()
    {
        // Copy the elements into the new buffer.
        for (size_type ii = m_front; ii != m_back; ++ii)
        {
            allocator_type::destroy(m_pRingBuffer + (ii & (m_capacity - 1)));
        }
        m_front = m_back = 0;
    }

private:

    bool _grow(size_type size)
    {
        if (size < MIN_CAPACITY || size > MAX_CAPACITY)
        {
            // can't grow anymore.
            return false;
        }

        value_type* pOldBuffer = m_pRingBuffer;
        size_type oldMask = (m_capacity - 1);

        m_pRingBuffer = allocator_type::template allocate<value_type>((size_t)size);
        m_capacity = size;

        // Copy the elements into the new buffer.
        for (size_type ii = m_front; ii != m_back; ++ii)
        {
            allocator_type::construct(m_pRingBuffer + (ii & (m_capacity - 1)), pOldBuffer[ii & oldMask]);
        }

        allocator_type::deallocate(pOldBuffer, (size_t)oldMask + 1);

        return true;
    }

    void _deepCopy(RingDeque const& src)
    {
        RingDeque* pSrcRingBuffer = src.m_pRingBuffer;
        if (pSrcRingBuffer)
        {
            // Create a RingDeque
            m_pRingBuffer = allocator_type::template allocate<value_type>(pSrcRingBuffer->m_capacity);
            m_capacity = pSrcRingBuffer->m_capacity;

            //
            // Copy RingDeque elements.

            size_type f = src.m_front.load(memory_order::relaxed);
            size_type b = src.m_back.load(memory_order::relaxed);

            for (size_type ii = f; ii != b; ++ii)
            {
                allocator_type::construct(pRingBuffer + (ii & (m_capacity - 1)), pSrcRingBuffer[ii & (pSrcRingBuffer->m_capacity - 1)]);
            }

            m_front = f;
            m_back = b;
        }
    }

    //! The maximum capacity is half the range of the index type, such
    //! that when it wraps around, it will be zero at index zero. This
    //! prevents the dual state of full and empty.
    static constexpr size_type MAX_CAPACITY = numericLimits<size_type>::max() / 2;

    static constexpr size_type MIN_CAPACITY = 2;

    //! The front of the buffer.
    size_type m_front;

    //! The back of the buffer.
    size_type m_back;

    //! A storage buffer.
    value_type* m_pRingBuffer;

    //! The size of m_pRingBuffer.
    size_type m_capacity;
};

/** @} */ // end of Serial
/** @} */ // end of Containers

} // namespace gts
