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
QueueMPSC<T, TStorage, TAllocator>::QueueMPSC(allocator_type const& allocator)
    : m_queue(allocator)
{}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
QueueMPSC<T, TStorage, TAllocator>::~QueueMPSC()
{}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
QueueMPSC<T, TStorage, TAllocator>::QueueMPSC(QueueMPSC const& other)
    : m_queue(other.m_queue)
{}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
QueueMPSC<T, TStorage, TAllocator>::QueueMPSC(QueueMPSC&& other)
    : m_queue(std::move(other.m_queue))
{}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
QueueMPSC<T, TStorage, TAllocator>& QueueMPSC<T, TStorage, TAllocator>::operator=(QueueMPSC const& other)
{
    if (this != &other)
    {
        m_queue = other.m_queue;
    }
    return *this;
}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
QueueMPSC<T, TStorage, TAllocator>& QueueMPSC<T, TStorage, TAllocator>::operator=(QueueMPSC&& other)
{
    if (this != &other)
    {
        m_queue = std::move(other.m_queue);
    }
    return *this;
}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
bool QueueMPSC<T, TStorage, TAllocator>::empty() const
{
    return size() == 0;
}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
typename QueueMPSC<T, TStorage, TAllocator>::size_type QueueMPSC<T, TStorage, TAllocator>::size() const
{
    return m_queue.size();
}

template<typename T, template <typename, typename> class TStorage, typename TAllocator>
typename QueueMPSC<T, TStorage, TAllocator>::size_type QueueMPSC<T, TStorage, TAllocator>::capacity() const
{
    return m_queue.capacity();
}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
void QueueMPSC<T, TStorage, TAllocator>::clear()
{
    m_queue.clear(m_accessorMutex);
}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
bool QueueMPSC<T, TStorage, TAllocator>::tryPush(const value_type& val)
{
    return m_queue.tryPush(val, m_accessorMutex, m_growMutex);
}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
bool QueueMPSC<T, TStorage, TAllocator>::tryPush(value_type&& val)
{
    return m_queue.tryPush(val, m_accessorMutex, m_growMutex);
}

//------------------------------------------------------------------------------
template<typename T, template <typename, typename> class TStorage, typename TAllocator>
bool QueueMPSC<T, TStorage, TAllocator>::tryPop(value_type& out)
{
    return m_queue.tryPop(out, m_growMutex);
}
