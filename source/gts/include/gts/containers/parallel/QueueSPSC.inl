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
QueueSPSC<T, TStorage, TAllocator>::QueueSPSC(allocator_type const& allocator)
    : m_queue(allocator)
{}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
QueueSPSC<T, TStorage, TAllocator>::~QueueSPSC()
{}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
QueueSPSC<T, TStorage, TAllocator>::QueueSPSC(QueueSPSC const& other)
    : m_queue(other.m_queue)
{}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
QueueSPSC<T, TStorage, TAllocator>::QueueSPSC(QueueSPSC&& other)
    : m_queue(std::move(other.m_queue))
{}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
QueueSPSC<T, TStorage, TAllocator>& QueueSPSC<T, TStorage, TAllocator>::operator=(QueueSPSC const& other)
{
    if (this != &other)
    {
        m_queue = other.m_queue;
    }
    return *this;
}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
QueueSPSC<T, TStorage, TAllocator>& QueueSPSC<T, TStorage, TAllocator>::operator=(QueueSPSC&& other)
{
    if (this != &other)
    {
        m_queue = std::move(other.m_queue);
    }
    return *this;
}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
bool QueueSPSC<T, TStorage, TAllocator>::empty() const
{
    return size() == 0;
}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
typename QueueSPSC<T, TStorage, TAllocator>::size_type QueueSPSC<T, TStorage, TAllocator>::size() const
{
    return m_queue.size();
}

template<typename T, template <typename, typename> class TStorage, typename TAllocator>
typename QueueSPSC<T, TStorage, TAllocator>::size_type QueueSPSC<T, TStorage, TAllocator>::capacity() const
{
    return m_queue.capacity();
}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
void QueueSPSC<T, TStorage, TAllocator>::clear()
{
    NullMutex m_mutex;
    m_queue.clear(m_mutex);
}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
bool QueueSPSC<T, TStorage, TAllocator>::tryPush(const value_type& val)
{
    NullMutex m_mutex;
    return m_queue.tryPush(val, m_mutex, m_mutex);
}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
bool QueueSPSC<T, TStorage, TAllocator>::tryPush(value_type&& val)
{
    NullMutex m_mutex;
    return m_queue.tryPush(val, m_mutex, m_mutex);
}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
bool QueueSPSC<T, TStorage, TAllocator>::tryPop(value_type& out)
{
    NullMutex m_mutex;
    return m_queue.tryPop(out, m_mutex);
}
