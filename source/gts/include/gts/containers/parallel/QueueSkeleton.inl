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

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
QueueSkeleton<T, TStorage, TAllocator>::QueueSkeleton(allocator_type const& allocator)
    : m_front(0)
    , m_back(0)
    , m_pRingBuffer(nullptr)
    , m_pQuienscentRingBuffer(nullptr)
    , m_allocator(allocator)
{
    _grow(1, 0, 0);
}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
QueueSkeleton<T, TStorage, TAllocator>::~QueueSkeleton()
{
    gts::alignedDelete(m_pQuienscentRingBuffer);
    gts::alignedDelete(m_pRingBuffer.load(gts::memory_order::relaxed));
}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
QueueSkeleton<T, TStorage, TAllocator>::QueueSkeleton(QueueSkeleton const& other)
    : m_front(other.m_front.load())
    , m_back(other.m_back.load())
    , m_pRingBuffer(nullptr)
    , m_pQuienscentRingBuffer(nullptr)
    , m_allocator(other.m_allocator)
{
    RingBuffer* pOtherRingBuffer = other.m_pRingBuffer.load(gts::memory_order::acquire);
    if (pOtherRingBuffer != nullptr)
    {
        RingBuffer* pRingBuffer = gts::alignedNew<RingBuffer, GTS_NO_SHARING_CACHE_LINE_SIZE>(pOtherRingBuffer->mask + 1, m_allocator);
        pRingBuffer->vec        = pOtherRingBuffer->vec;
        pRingBuffer->mask       = pOtherRingBuffer->mask;

        m_pRingBuffer.store(pRingBuffer, gts::memory_order::relaxed);
    }
}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
QueueSkeleton<T, TStorage, TAllocator>::QueueSkeleton(QueueSkeleton&& other)
    : m_front(other.m_front.load())
    , m_back(other.m_back.load())
    , m_pRingBuffer(other.m_pRingBuffer.load())
    , m_pQuienscentRingBuffer(other.m_pQuienscentRingBuffer)
    , m_allocator(other.m_allocator)
{
    other.m_back.store(0, gts::memory_order::relaxed);
    other.m_front.store(0, gts::memory_order::relaxed);
    other.m_pRingBuffer.store(nullptr, gts::memory_order::relaxed);
    other.m_pQuienscentRingBuffer = nullptr;
}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
QueueSkeleton<T, TStorage, TAllocator>& QueueSkeleton<T, TStorage, TAllocator>::operator=(QueueSkeleton const& other)
{
    if (this != &other)
    {
        gts::alignedDelete(m_pQuienscentRingBuffer);
        gts::alignedDelete(m_pRingBuffer.load(gts::memory_order::relaxed));

        m_back.store(other.m_back.load(), gts::memory_order::relaxed);
        m_front.store(other.m_front.load(), gts::memory_order::relaxed);
        m_pRingBuffer.store(nullptr, gts::memory_order::relaxed);
        m_pQuienscentRingBuffer = nullptr;

        RingBuffer* pOtherRingBuffer = other.m_pRingBuffer.load(gts::memory_order::acquire);
        if (pOtherRingBuffer != nullptr)
        {
            RingBuffer* pRingBuffer = gts::alignedNew<RingBuffer, GTS_NO_SHARING_CACHE_LINE_SIZE>(pOtherRingBuffer->mask + 1, m_allocator);
            pRingBuffer->vec        = pOtherRingBuffer->vec;
            pRingBuffer->mask       = pOtherRingBuffer->mask;

            m_pRingBuffer.store(pRingBuffer, gts::memory_order::relaxed);
        }
    }
    return *this;
}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
QueueSkeleton<T, TStorage, TAllocator>& QueueSkeleton<T, TStorage, TAllocator>::operator=(QueueSkeleton&& other)
{
    if (this != &other)
    {
        m_back.store(other.m_back.load(), gts::memory_order::relaxed);
        m_front.store(other.m_front.load(), gts::memory_order::relaxed);
        m_pRingBuffer.store(other.m_pRingBuffer.load(), gts::memory_order::relaxed);
        m_pQuienscentRingBuffer = other.m_pQuienscentRingBuffer;

        other.m_back.store(0, gts::memory_order::relaxed);
        other.m_front.store(0, gts::memory_order::relaxed);
        other.m_pRingBuffer.store(nullptr, gts::memory_order::relaxed);
        other.m_pQuienscentRingBuffer = nullptr;
    }
    return *this;
}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
bool QueueSkeleton<T, TStorage, TAllocator>::empty() const
{
    return size() == 0;
}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
typename QueueSkeleton<T, TStorage, TAllocator>::size_type QueueSkeleton<T, TStorage, TAllocator>::size() const
{
    index_size_type b = m_back.load(gts::memory_order::acquire);
    index_size_type f = m_front.load(gts::memory_order::acquire);

    return b - f;
}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
typename QueueSkeleton<T, TStorage, TAllocator>::size_type QueueSkeleton<T, TStorage, TAllocator>::capacity() const
{
    return (size_type)m_pRingBuffer.load(gts::memory_order::acquire)->vec.size();
}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
template<typename TMutex>
void QueueSkeleton<T, TStorage, TAllocator>::clear(TMutex& mutex)
{
    gts::Lock<TMutex> lock(mutex);
    m_front.store(0);
    m_back.store(0);
}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
template<typename TMutex, typename TGrowMutex>
bool QueueSkeleton<T, TStorage, TAllocator>::tryPush(const value_type& val, TMutex& mutex, TGrowMutex& growMutex)
{
    gts::Lock<TMutex> lock(mutex);

    index_size_type f = m_front.load(gts::memory_order::acquire);
    index_size_type b = m_back.load(gts::memory_order::relaxed);
    RingBuffer* pRingBuffer = m_pRingBuffer.load(gts::memory_order::relaxed);

    // If the buffer is at capacity, try to grow it.
    if (std::make_signed<size_type>::type(b - f) >= std::make_signed<size_type>::type(capacity()))
    {
        gts::Lock<TGrowMutex> growLock(growMutex);

        if (!_grow(capacity() * 2, f, b))
        {
            return false;
        }
        pRingBuffer = m_pRingBuffer.load(gts::memory_order::relaxed);
    }

    // Store the new value in the buffer.
    pRingBuffer->vec[b & pRingBuffer->mask] = val;

    // Notify the consumers that the new value exists.
    m_back.store(b + 1, gts::memory_order::release);

    return true;
}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
template<typename TMutex, typename TGrowMutex>
bool QueueSkeleton<T, TStorage, TAllocator>::tryPush(value_type&& val, TMutex& mutex, TGrowMutex& growMutex)
{
    gts::Lock<TMutex> lock(mutex);

    index_size_type f = m_front.load(gts::memory_order::acquire);
    index_size_type b = m_back.load(gts::memory_order::relaxed);
    RingBuffer* pRingBuffer = m_pRingBuffer.load(gts::memory_order::relaxed);

    // If the buffer is at capacity, try to grow it.
    if (std::make_signed<size_type>::type(b - f) >= std::make_signed<size_type>::type(capacity()))
    {
        gts::Lock<TGrowMutex> growLock(growMutex);

        if (!_grow(capacity() * 2, f, b))
        {
            return false;
        }
        pRingBuffer = m_pRingBuffer.load(gts::memory_order::relaxed);
    }

    // Store the new value in the buffer.
    pRingBuffer->vec[b & pRingBuffer->mask] = std::move(val);

    // Notify the consumers that the new value exists.
    m_back.store(b + 1, gts::memory_order::release);

    return true;
}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
template<typename TMutex>
bool QueueSkeleton<T, TStorage, TAllocator>::tryPop(value_type& out, TMutex& mutex)
{
    gts::Lock<TMutex> lock(mutex);

    index_size_type b = m_back.load(gts::memory_order::acquire);
    index_size_type f = m_front.load(gts::memory_order::relaxed);

    std::make_signed<size_type>::type size = b - f;

    // If the queue is empty, quit.
    if (size <= 0)
    {
        return false;
    }

    RingBuffer* pRingBuffer = m_pRingBuffer.load(gts::memory_order::relaxed);
    out = pRingBuffer->vec[f & pRingBuffer->mask];

    // Notify the producer of the consumption.
    m_front.store(f + 1, gts::memory_order::release);

    return true;
}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
template<typename TMutex, typename TTag>
bool QueueSkeleton<T, TStorage, TAllocator>::tryPop(value_type& out, TMutex& mutex, TTag const& tag)
{
    gts::Lock<TMutex> lock(mutex);

    index_size_type b = m_back.load(gts::memory_order::acquire);
    index_size_type f = m_front.load(gts::memory_order::relaxed);

    std::make_signed<size_type>::type size = b - f;

    // If the queue is empty, quit.
    if (size <= 0)
    {
        return false;
    }

    RingBuffer* pRingBuffer = m_pRingBuffer.load(gts::memory_order::relaxed);
    out = pRingBuffer->vec[f & pRingBuffer->mask];

    if (out->isolationTag() != tag)
    {
        return false;
    }

    // Notify the producer of the consumption.
    m_front.store(f + 1, gts::memory_order::release);

    return true;
}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
bool QueueSkeleton<T, TStorage, TAllocator>::_grow(size_type size, index_size_type front, index_size_type back)
{
    if (size > MAX_CAPACITY)
    {
        // can't grow anymore.
        return false;
    }

    size_type newMask      = size - 1;
    RingBuffer* pNewBuffer = gts::alignedNew<RingBuffer, GTS_NO_SHARING_CACHE_LINE_SIZE>(size, m_allocator);
    RingBuffer* pOldBuffer = m_pRingBuffer.load(gts::memory_order::relaxed);
    size_type oldMask      = pOldBuffer ? pOldBuffer->mask : 0;

    // Copy the elements into the new buffer.
    pNewBuffer->mask    = newMask;
    for (index_size_type ii = front; ii < back; ++ii)
    {
        (*pNewBuffer).vec[ii & newMask] = (*pOldBuffer).vec[ii & oldMask];
    }

    // Save the previous buffer since a consumer could still be using it.
    if (m_pQuienscentRingBuffer)
    {
        gts::alignedDelete(m_pQuienscentRingBuffer);
    }

    // Swap in the new buffer.
    m_pQuienscentRingBuffer = m_pRingBuffer.exchange(pNewBuffer);

    return true;
}
