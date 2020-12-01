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

// STRUCTORS:

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
Vector<T, TAllocator>::~Vector()
{
    clear();
    allocator_type::deallocate(m_pBegin, m_capacity);
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
Vector<T, TAllocator>::Vector(const allocator_type& allocator)
    : allocator_type(allocator)
    , m_pBegin(nullptr)
    , m_size(0)
    , m_capacity(0)
{}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
Vector<T, TAllocator>::Vector(size_type count, const TAllocator& allocator)
    : allocator_type(allocator)
    , m_pBegin(nullptr)
    , m_size(0)
    , m_capacity(0)
{
    reserve(count);
    _init(count, value_type());
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
Vector<T, TAllocator>::Vector(size_type count, T const& fill, const TAllocator& allocator)
    : allocator_type(allocator)
    , m_pBegin(nullptr)
    , m_size(0)
    , m_capacity(0)
{
    reserve(count);
    _init(count, fill);
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
Vector<T, TAllocator>::Vector(Vector const& other)
    : allocator_type(other.get_allocator())
    , m_pBegin(nullptr)
    , m_size(0)
    , m_capacity(other.m_capacity)
{
    _deepCopy(*this, other);
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
Vector<T, TAllocator>::Vector(Vector&& other)
    : allocator_type(std::move(other.get_allocator()))
    , m_pBegin(std::move(other.m_pBegin))
    , m_size(std::move(other.m_size))
    , m_capacity(std::move(other.m_capacity))
{
    other.m_pBegin   = nullptr;
    other.m_size     = 0;
    other.m_capacity = 0;
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
Vector<T, TAllocator>& Vector<T, TAllocator>::operator=(Vector const& other)
{
    if(this != &other)
    {
        clear();
        allocator_type::deallocate(m_pBegin, m_capacity);

        m_capacity            = other.m_capacity;
        (allocator_type)*this = other.get_allocator();

        _deepCopy(*this, other);
    }
    return *this;
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
Vector<T, TAllocator>& Vector<T, TAllocator>::operator=(Vector&& other)
{
    if(this != &other)
    {
        clear();
        allocator_type::deallocate(m_pBegin, m_capacity);

        m_pBegin              = std::move(other.m_pBegin);
        m_size                = std::move(other.m_size);
        m_capacity            = std::move(other.m_capacity);
        (allocator_type)*this = std::move(other.get_allocator());

        other.m_pBegin   = nullptr;
        other.m_size     = 0;
        other.m_capacity = 0;
    }
    return *this;
}

// CAPACITY:

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
bool Vector<T, TAllocator>::empty() const
{
    return size() == 0;
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
typename Vector<T, TAllocator>::size_type Vector<T, TAllocator>::size() const
{
    return m_size;
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
typename Vector<T, TAllocator>::size_type Vector<T, TAllocator>::capacity() const
{
    return m_capacity;
}

// ACCESSORS:

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
T const* Vector<T, TAllocator>::begin() const
{
    return m_pBegin;
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
T* Vector<T, TAllocator>::begin()
{
    return m_pBegin;
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
T const* Vector<T, TAllocator>::end() const
{
    return m_pBegin + m_size;
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
T* Vector<T, TAllocator>::end()
{
    return m_pBegin + m_size;;
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
T const& Vector<T, TAllocator>::front() const
{
    GTS_ASSERT(!empty());
    return m_pBegin[0];
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
T& Vector<T, TAllocator>::front()
{
    GTS_ASSERT(!empty());
    return m_pBegin[0];
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
T const& Vector<T, TAllocator>::back() const
{
    GTS_ASSERT(!empty());
    return m_pBegin[m_size - 1];
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
T& Vector<T, TAllocator>::back()
{
    GTS_ASSERT(!empty());
    return m_pBegin[m_size - 1];
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
T const& Vector<T, TAllocator>::operator[](size_type pos) const
{
    GTS_ASSERT(pos < size());
    return m_pBegin[pos];
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
T& Vector<T, TAllocator>::operator[](size_type pos)
{
    GTS_ASSERT(pos < size());
    return m_pBegin[pos];
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
T const* Vector<T, TAllocator>::data() const
{
    return m_pBegin;
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
T* Vector<T, TAllocator>::data()
{
    return m_pBegin;
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
typename Vector<T, TAllocator>::allocator_type Vector<T, TAllocator>::get_allocator() const
{
    return *this;
}

// MUTATORS:

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
void Vector<T, TAllocator>::push_back(T const& val)
{
    emplace_back(val);
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
void Vector<T, TAllocator>::push_back(T&& val)
{
    emplace_back(std::move(val));
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
template<typename... TArgs>
void Vector<T, TAllocator>::emplace_back(TArgs&&... args)
{
    if (size() == capacity())
    {
        _grow(m_capacity * 2);
    }
    allocator_type::construct(m_pBegin + m_size, std::forward<TArgs>(args)...);
    m_size += 1;
}


//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
void Vector<T, TAllocator>::pop_back()
{
    GTS_ASSERT(!empty());
    allocator_type::destroy(&back());
    m_size -= 1;
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
void Vector<T, TAllocator>::resize(size_type count)
{
    if (count < m_size)
    {
        _shrink(count);
    }
    else
    {
        if(count > m_capacity)
        {
            _grow(count);
        }
        _init(count - size());
    }
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
void Vector<T, TAllocator>::resize(size_type count, T const& fill)
{
    if (count < m_size)
    {
        _shrink(count);
    }
    else
    {
        if(count > m_capacity)
        {
            _grow(count);
        }
        _init(count - size(), fill);
    }
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
void Vector<T, TAllocator>::reserve(size_type count)
{
    if(count > m_capacity)
    {
        _grow(count);
    }
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
void Vector<T, TAllocator>::shrink_to_fit()
{
    size_type extra = capacity() - size();
    if (extra > 0)
    {
        _shrink(size());
    }
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
void Vector<T, TAllocator>::clear()
{
    for (size_type ii = 0, len = size(); ii < len; ++ii)
    {
        allocator_type::destroy(m_pBegin + ii);
    }
    m_size = 0;
}


// PRIVATES:

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
template<typename... TArgs>
void Vector<T, TAllocator>::_init(size_type count, TArgs&&... args)
{
    for(size_type ii = 0; ii < count; ++ii)
    {
        emplace_back(std::forward<TArgs>(args)...);
    }
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
void Vector<T, TAllocator>::_grow(size_type capacity)
{
    size_type oldCapacity = m_capacity;

    m_capacity = capacity == 0 ? 1 : capacity;

    if (m_capacity <= oldCapacity)
    {
        GTS_ASSERT(0);
        return;
    }

    size_type numElements = size();
    value_type* oldBegin  = m_pBegin;
    m_pBegin              = allocator_type::template allocate<value_type>(m_capacity);
    m_size                = numElements;

    // Copy elements into the new buffer.
    ::memcpy(m_pBegin, oldBegin, sizeof(value_type) * numElements);
    allocator_type::deallocate(oldBegin, oldCapacity);
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
void Vector<T, TAllocator>::_shrink(size_type capacity)
{
    size_type oldCapacity = m_capacity;

    if (capacity >= m_capacity)
    {
        GTS_ASSERT(capacity <= m_capacity);
        return;
    }

    size_type oldSize     = size();
    m_capacity            = capacity;
    value_type* oldBegin  = m_pBegin;
    m_pBegin              = allocator_type::template allocate<value_type>(m_capacity);
    m_size                = m_capacity;

    // Copy unclipped elements into the new buffer.
    ::memcpy(m_pBegin, oldBegin, sizeof(value_type) * m_capacity);

    // Delete the clipped elements from the old buffer.
    for (size_type ii = size(); ii < oldSize; ++ii)
    {
        allocator_type::destroy(oldBegin + ii);
    }

    allocator_type::deallocate(oldBegin, oldCapacity);
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
void Vector<T, TAllocator>::_deepCopy(Vector& dst, Vector const& src)
{
    dst.m_pBegin = allocator_type::template allocate<value_type>(src.size());
    ::memcpy(dst.m_pBegin, src.m_pBegin, sizeof(value_type) * src.size());
    dst.m_size = src.size();
}
