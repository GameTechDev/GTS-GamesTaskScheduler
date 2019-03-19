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
RingBuffer<T, TStorage, TAllocator>::RingBuffer()
    :
    m_front(0),
    m_back(0)
{}
    
//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
RingBuffer<T, TStorage, TAllocator>::~RingBuffer()
{}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
RingBuffer<T, TStorage, TAllocator>::RingBuffer(RingBuffer const& other)
    : m_back(other.m_back)
    , m_front(other.m_front)
    , m_ringBuffer(other.m_ringBuffer)
    , m_mask(other.m_mask)
{
    assert(m_ringBuffer.size() == other.m_ringBuffer.size());
}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
RingBuffer<T, TStorage, TAllocator>::RingBuffer(RingBuffer&& other)
    : m_back(std::move(other.m_back))
    , m_front(std::move(other.m_front))
    , m_ringBuffer(std::move(other.m_ringBuffer))
    , m_mask(std::move(other.m_mask))
{
    other.m_back  = 0;
    other.m_front = 0;
}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
RingBuffer<T, TStorage, TAllocator>& RingBuffer<T, TStorage, TAllocator>::operator=(RingBuffer&& other)
{
    if (this != &other)
    {
        GTS_ASSERT(m_ringBuffer.size() == 0);

        m_back       = std::move(other.m_back);
        m_front      = std::move(other.m_front);
        m_ringBuffer = std::move(other.m_ringBuffer);

        other.m_back  = 0;
        other.m_front = 0;
    }
    return *this;
}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
bool RingBuffer<T, TStorage, TAllocator>::empty() const
{
    return size() == 0;
}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
typename RingBuffer<T, TStorage, TAllocator>::size_type RingBuffer<T, TStorage, TAllocator>::size() const
{
    return m_back - m_front;
}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
void RingBuffer<T, TStorage, TAllocator>::init(size_type powOf2)
{
    size_type capacity = next_pow2((size_type)powOf2);
    GTS_ASSERT(capacity < MAX_CAPACITY);

    m_ringBuffer.resize(capacity);
    m_mask = capacity - 1;
}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
void RingBuffer<T, TStorage, TAllocator>::clear()
{
    m_front = 0;
    m_back = 0;
}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
typename RingBuffer<T, TStorage, TAllocator>::size_type RingBuffer<T, TStorage, TAllocator>::capacity() const
{
    return (size_type)m_ringBuffer.size();
}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
bool RingBuffer<T, TStorage, TAllocator>::tryPush(const value_type& val)
{
    // If the buffer is at capacity, fail.
    if (std::make_signed<size_type>::type(m_back - m_front) >= std::make_signed<size_type>::type(m_ringBuffer.size()))
    {
        return false;
    }

    // Store the new value in the buffer.
    m_ringBuffer[m_back & m_mask] = val;

    // Notify the consumers that the new value exists.
    m_back++;

    return true;
}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
bool RingBuffer<T, TStorage, TAllocator>::tryPush(value_type&& val)
{
    // If the buffer is at capacity, fail.
    if (std::make_signed<size_type>::type(m_back - m_front) >= std::make_signed<size_type>::type(m_ringBuffer.size()))
    {
        return false;
    }

    // Store the new value in the buffer.
    m_ringBuffer[m_back & m_mask] = std::move(val);

    // Notify the consumers that the new value exists.
    m_back++;

    return true;
}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
bool RingBuffer<T, TStorage, TAllocator>::tryPop(value_type& out)
{
    std::make_signed<size_type>::type size = m_back - m_front;

    // If the queue is empty, quit.
    if (size <= 0)
    {
        return false;
    }

    out = m_ringBuffer[m_front & m_mask];

    m_front++;

    return true;
}
