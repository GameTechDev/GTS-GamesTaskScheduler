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
 *  A re-sizable array-like ADT. A subset of the STL Vector interface.
 * @tparam T
 *  The element type stored in the container.
 * @tparam TAllocator
 *  The allocator used by the storage backing.
 */
template<
    typename T,
    typename TAllocator = AlignedAllocator<GTS_NO_SHARING_CACHE_LINE_SIZE>
>
class Vector : private TAllocator
{
public:

    using size_type      = size_t;
    using value_type     = T;
    using allocator_type = TAllocator;

public: // STRUCTORS:

    /**
     * @brief
     *  Destructs the container. The destructors of the elements are called and the
     *  used storage is deallocated.
     */
    ~Vector();

    /**
     * @brief
     *  Constructs an empty container with the given 'allocator'.
     */
    explicit Vector(const allocator_type& allocator = allocator_type());

    /**
     * @brief
     *  Constructs the container with 'count' copies of default constructed values.
     */
    explicit Vector(size_type count, const allocator_type& allocator = allocator_type());

    /**
     * @brief
     *  Constructs the container with 'count' copies of elements with value 'fill'.
     */
    Vector(size_type count, value_type const& fill, const allocator_type& alloc = allocator_type());

    /**
     * @brief
     *  Copy constructor. Constructs the container with the copy of the contents
     *  of 'other'.
     */
    Vector(Vector const& other);

    /**
     * @brief
     *  Move constructor. Constructs the container with the contents of other
     *  using move semantics. After the move, other is invalid.
     */
    Vector(Vector&& other);

    /**
     * @brief
     *  Copy assignment operator. Replaces the contents with a copy of the
     *  contents of 'other'.
     */
    Vector& operator=(Vector const& other);

    /**
     * @brief
     * Move assignment operator. Replaces the contents with those of other using
     * move semantics. After the move, other is invalid.
     */
    Vector& operator=(Vector&& other);

public: // CAPACITY:

    /**
     * @brief
     *  Checks if the contains has no elements.
     * @returns
     *  True if empty, false otherwise.
     */
    bool empty() const;

    /**
     * @brief
     *  Gets the number of elements in the container.
     * @returns
     *  The size.
     */
    size_type size() const;

    /**
     * @brief
     *  Gets the number of elements that can be held in the currently
     *  allocated storage.
     * @returns
     *  The capacity.
     */
    size_type capacity() const;

public: // ELEMENT ACCESS:

    /**
     * @brief
     *  Gets a pointer to the first element;
     * @returns
     *  An element pointer.
     */
    value_type const* begin() const;

    /**
     * @brief
     *  Gets a pointer to the first element;
     * @returns
     *  An element pointer.
     */
    value_type* begin();

    /**
     * @brief
     *  Gets a pointer to one past the last element;
     * @returns
     *  An element pointer.
     */
    value_type const* end() const;

    /**
     * @brief
     *  Gets a pointer to one past the last element;
     * @returns
     *  An element pointer.
     */
    value_type* end();

    /**
     * @brief
     *  Gets the first element.
     * @returns
     *  An element.
     */
    value_type const& front() const;

    /**
     * @brief
     *  Gets the first element.
     * @returns
     *  An element.
     */
    value_type& front();

    /**
     * @brief
     *  Gets the last element.
     * @returns
     *  An element.
     */
    value_type const& back() const;

    /**
     * @brief
     *  Gets the last element.
     * @returns
     *  An element.
     */
    value_type& back();

    /**
     * @brief
     *  Gets the element at \a pos.
     * @returns
     *  An element.
     */
    value_type const& operator[](size_type pos) const;

    /**
     * @brief
     *  Gets the element at \a pos.
     * @returns
     *  An element.
     */
    value_type& operator[](size_type pos);

    /**
     * @brief
     *  A pointer to the underlying storage array.
     * @returns
     *  The storage array.
     */
    value_type const* data() const;

    /**
     * @brief
     *  A pointer to the underlying storage array.
     * @returns
     *  The storage array.
     */
    value_type* data();

    /**
     * @brief
     *  Get this Vectors allocator.
     * @returns
     *  The allocator.
     */
    allocator_type get_allocator() const;

public: // MUTATORS:

    /**
     * Copies 'val' to the end.
     */
    void push_back(value_type const& val);

    /**
     * Moves 'val' to the end.
     */
    void push_back(value_type&& val);

    /**
     * Construct a new element at the end.
     */
    template<typename... TArgs>
    void emplace_back(TArgs&&... args);

    /**
     * Removes and destroys the element at the end.
     */
    void pop_back();

    /**
     * Resize the vector to have 'count' elements. If count > size(), new
     * will be default constructed.
     */
    void resize(size_type count);

    /**
     * Resize the vector to have 'count' elements. If count > size(), new
     * will be constructed with the value of 'fill'.
     */
    void resize(size_type count, value_type const& fill);

    /**
     * Grow the underlying array if count > capacity, otherwise it does nothing.
     */
    void reserve(size_type count);

    /**
     * Resize the array to size().
     */
    void shrink_to_fit();

    /**
     * Removes all elements but does not destroy the underlying array.
     */
    void clear();

private:

    template<typename... TArgs>
    void _init(size_type count, TArgs&&... args);
    void _grow(size_type capacity);
    void _shrink(size_type capacity);
    void _deepCopy(Vector& dst, Vector const& src);

    //! A pointer to the beginning of the storage array.
    value_type* m_pBegin;

    //! The number of element in the container.
    size_type m_size;

    //! The size of the storage array.
    size_type m_capacity;
};

/** @} */ // end of Serial
/** @} */ // end of Containers

#include "VectorImpl.inl"

} // namespace gts
