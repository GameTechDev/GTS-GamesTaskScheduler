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

namespace gts {
namespace internal {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// TicketQueueMPSC

template<typename T, typename TMutex, typename TAllocator>
constexpr typename TicketQueueMPSC<T, TMutex, TAllocator>::size_type TicketQueueMPSC<T, TMutex, TAllocator>::MAX_CAPACITY;

//------------------------------------------------------------------------------
template<typename T, typename TMutex, typename TAllocator>
TicketQueueMPSC<T, TMutex, TAllocator>::TicketQueueMPSC(size_t numQueues, allocator_type const& allocator)
    : allocator_type(allocator)
    , m_ticketMask(~numQueues + 1)
    , m_ticketShift(size_type(log2i(numQueues)))
    , m_front(0)
    , m_back(0)
    , m_pRingBuffer(nullptr)
    , m_mask(SIZE_MAX)
{}

//------------------------------------------------------------------------------
template<typename T, typename TMutex, typename TAllocator>
TicketQueueMPSC<T, TMutex, TAllocator>::TicketQueueMPSC(size_t numQueues, size_type sizePow2, allocator_type const& allocator)
    : allocator_type(allocator)
    , m_ticketMask(~numQueues + 1)
    , m_ticketShift(size_type(log2i(numQueues)))
    , m_front(0)
    , m_back(0)
    , m_pRingBuffer(nullptr)
    , m_mask(SIZE_MAX)
{
    reserve(sizePow2);
}

//------------------------------------------------------------------------------
template<typename T, typename TMutex, typename TAllocator>
TicketQueueMPSC<T, TMutex, TAllocator>::~TicketQueueMPSC()
{
    for (size_type ii = m_front; ii < m_back; ++ii)
    {
        allocator_type::destroy(m_pRingBuffer + (ii & m_mask));
    }
    allocator_type::deallocate(m_pRingBuffer, m_mask + 1);
}

//------------------------------------------------------------------------------
template<typename T, typename TMutex, typename TAllocator>
TicketQueueMPSC<T, TMutex, TAllocator>::TicketQueueMPSC(TicketQueueMPSC const& other)
    : allocator_type(other.get_allocator())
    , m_ticketMask(other.m_ticketMask)
    , m_ticketShift(other.m_ticketShift)
    , m_front(other.m_front)
    , m_back(other.m_back)
    , m_pRingBuffer(nullptr)
    , m_mask(other.m_mask)
{
    if (other.m_pRingBuffer != nullptr)
    {
        _deepCopy(other);
    }
}

//------------------------------------------------------------------------------
template<typename T, typename TMutex, typename TAllocator>
TicketQueueMPSC<T, TMutex, TAllocator>::TicketQueueMPSC(TicketQueueMPSC&& other)
    : allocator_type(std::move(other.get_allocator()))
    , m_ticketShift(std::move(other.m_ticketShift))
    , m_front(std::move(other.m_front))
    , m_back(std::move(other.m_back))
    , m_pRingBuffer(std::move(other.m_pRingBuffer))
    , m_mask(std::move(other.m_mask))
{
    other.m_back        = 0;
    other.m_front       = 0;
    other.m_pRingBuffer = nullptr;
    other.m_mask        = 0;
}

//------------------------------------------------------------------------------
template<typename T, typename TMutex, typename TAllocator>
TicketQueueMPSC<T, TMutex, TAllocator>& TicketQueueMPSC<T, TMutex, TAllocator>::operator=(TicketQueueMPSC const& other)
{
    if (this != &other)
    {
        clear();

        GTS_ASSERT(m_ticketMask == other.m_ticketMask && m_ticketShift == other.m_ticketShift);

        m_front               = other.m_front;
        m_back                = other.m_back;
        m_pRingBuffer         = other.m_pRingBuffer;
        m_mask                = other.m_mask;
        (allocator_type)*this = other.get_allocator();

        _deepCopy(other);
    }
    return *this;
}

//------------------------------------------------------------------------------
template<typename T, typename TMutex, typename TAllocator>
TicketQueueMPSC<T, TMutex, TAllocator>& TicketQueueMPSC<T, TMutex, TAllocator>::operator=(TicketQueueMPSC&& other)
{
    if (this != &other)
    {
        clear();

        GTS_ASSERT(m_ticketMask == other.m_ticketMask && m_ticketShift == other.m_ticketShift);

        m_front                = std::move(other.m_front);
        m_back                 = std::move(other.m_back);
        m_pRingBuffer          = std::move(other.m_pRingBuffer);
        m_mask                 = std::move(other.m_mask);
        (allocator_type)*this  = std::move(other.get_allocator());

        other.m_back        = 0;
        other.m_front       = 0;
        other.m_pRingBuffer = nullptr;
        other.m_mask        = 0;
    }
    return *this;
}

//------------------------------------------------------------------------------
template<typename T, typename TMutex, typename TAllocator>
bool TicketQueueMPSC<T, TMutex, TAllocator>::empty() const
{
    return size() == 0;
}

//------------------------------------------------------------------------------
template<typename T, typename TMutex, typename TAllocator>
typename TicketQueueMPSC<T, TMutex, TAllocator>::size_type TicketQueueMPSC<T, TMutex, TAllocator>::size() const
{
    gts::Lock<mutex_type> lock(m_mutex);
    return m_back - m_front;
}

//------------------------------------------------------------------------------
template<typename T, typename TMutex, typename TAllocator>
typename TicketQueueMPSC<T, TMutex, TAllocator>::size_type TicketQueueMPSC<T, TMutex, TAllocator>::capacity() const
{
    return m_mask + 1;
}

//------------------------------------------------------------------------------
template<typename T, typename TMutex, typename TAllocator>
typename TicketQueueMPSC<T, TMutex, TAllocator>::allocator_type
TicketQueueMPSC<T, TMutex, TAllocator>::get_allocator() const
{
    return *this;
}

//------------------------------------------------------------------------------
template<typename T, typename TMutex, typename TAllocator>
void TicketQueueMPSC<T, TMutex, TAllocator>::reserve(size_type sizePow2)
{
    if (!isPow2(sizePow2))
    {
        GTS_ASSERT(isPow2(sizePow2));
        sizePow2 = gtsMax(size_type(1), nextPow2(sizePow2));
    }

    _grow(sizePow2, m_front, m_back);
}

//------------------------------------------------------------------------------
template<typename T, typename TMutex, typename TAllocator>
void TicketQueueMPSC<T, TMutex, TAllocator>::clear()
{
    gts::Lock<mutex_type> lock(m_mutex);

    for (size_type ii = m_front; ii < m_back; ++ii)
    {
        allocator_type::destroy(m_pRingBuffer + (ii & m_mask));
    }
    allocator_type::deallocate(m_pRingBuffer, m_mask + 1);

    m_pRingBuffer = nullptr;
    m_front = 0;
    m_back  = 0;
}

//------------------------------------------------------------------------------
template<typename T, typename TMutex, typename TAllocator>
bool TicketQueueMPSC<T, TMutex, TAllocator>::tryPush(size_type ticket, const value_type& val)
{
    return _insert(ticket, val);
}

//------------------------------------------------------------------------------
template<typename T, typename TMutex, typename TAllocator>
bool TicketQueueMPSC<T, TMutex, TAllocator>::tryPush(size_type ticket, value_type&& val)
{
    return _insert(ticket, std::move(val));
}

//------------------------------------------------------------------------------
template<typename T, typename TMutex, typename TAllocator>
bool TicketQueueMPSC<T, TMutex, TAllocator>::tryPop(size_type ticket, value_type& out)
{
    // Transform the ticket into the local queue index space.
    ticket &= m_ticketMask;
    ticket >>= m_ticketShift;

    // Wait for back item to be published
    while (ticket == m_back)
    {
        GTS_SPECULATION_FENCE();
    }

    gts::Lock<mutex_type> lock(m_mutex);

    // Copy the value to the output variable.
    out = m_pRingBuffer[ticket & m_mask];

    // Destroy the value from in the buffer.
    allocator_type::destroy(m_pRingBuffer + (ticket & m_mask));

    // Notify the producer of the consumption.
    m_front += 1;

    return true;
}

//------------------------------------------------------------------------------
template<typename T, typename TMutex, typename TAllocator>
template<typename... TArgs>
bool TicketQueueMPSC<T, TMutex, TAllocator>::_insert(size_type ticket, TArgs&&... args)
{
    // Transform the ticket into the local queue index space.
    ticket &= m_ticketMask;
    ticket >>= m_ticketShift;

    // Wait for our turn.
    while (ticket != m_back)
    {
        GTS_ASSERT(ticket >= m_back);
        GTS_SPECULATION_FENCE();
    }

    gts::Lock<mutex_type> lock(m_mutex);

    size_type f = m_front;
    size_type b = m_back;

    // If the buffer is at capacity, try to grow it.
    size_type cap = capacity();
    size_type size = 0;
    if (b + 1 < f)
    {
        size = cap - (f - (b + 1));
    }
    else
    {
        size = (b + 1) - f;
    }

    if (size > cap)
    {
        if (!_grow(gtsMax(size_type(1), cap * 2), f, b))
        {
            return false;
        }
    }

    // Store the new value in the buffer.
    allocator_type::construct(m_pRingBuffer + (b & m_mask), std::forward<TArgs>(args)...);

    // Notify the consumers that the new value exists.
    m_back += 1;

    return true;
}

//------------------------------------------------------------------------------
template<typename T, typename TMutex, typename TAllocator>
void TicketQueueMPSC<T, TMutex, TAllocator>::_deepCopy(TicketQueueMPSC const& src)
{
    m_pRingBuffer = allocator_type::template allocate<value_type>(src.size());
    ::memcpy(m_pRingBuffer, src.m_pRingBuffer, sizeof(value_type) * src.capacity());
}

//------------------------------------------------------------------------------
template<typename T, typename TMutex, typename TAllocator>
bool TicketQueueMPSC<T, TMutex, TAllocator>::_grow(size_type size, size_type front, size_type back)
{
    if (size > MAX_CAPACITY)
    {
        // can't grow anymore.
        return false;
    }

    size_type newMask      = size - 1;
    value_type* pNewBuffer = allocator_type::template allocate<value_type>(size);
    value_type* pOldBuffer = m_pRingBuffer;
    size_type oldMask      = m_mask;

    // Copy the elements into the new buffer.
    m_mask = newMask;
    for (size_type ii = front; ii != back; ++ii)
    {
        allocator_type::construct(pNewBuffer + (ii & m_mask), std::move(pOldBuffer[ii & oldMask]));
    }

    m_pRingBuffer = pNewBuffer;

    if(pOldBuffer)
    {
        allocator_type::deallocate(pOldBuffer, oldMask + 1);
    }

    return true;
}

} // namespace internal

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// QueueMPSC

//------------------------------------------------------------------------------
template<typename T, typename TMutex, typename TAllocator>
QueueMPSC<T, TMutex, TAllocator>::QueueMPSC(size_t numSubQueuesPow2, allocator_type const& allocator)
    : allocator_type(allocator)
    , m_front(0)
    , m_back(0)
    , m_pSubQueues(nullptr)
    , m_numSubQueues(numSubQueuesPow2)
{
    if (m_numSubQueues == 0)
    {
        m_numSubQueues = 1;
    }
    else if (!isPow2(m_numSubQueues))
    {
        m_numSubQueues = nextPow2(m_numSubQueues);
    }

    _constructQueues(m_numSubQueues, allocator);
}

//------------------------------------------------------------------------------
template<typename T, typename TMutex, typename TAllocator>
QueueMPSC<T, TMutex, TAllocator>::~QueueMPSC()
{
    for (size_type ii = 0; ii < m_numSubQueues; ++ii)
    {
        allocator_type::destroy(m_pSubQueues + ii);
    }
    allocator_type::deallocate(m_pSubQueues, m_numSubQueues);
}

//------------------------------------------------------------------------------
template<typename T, typename TMutex, typename TAllocator>
QueueMPSC<T, TMutex, TAllocator>::QueueMPSC(QueueMPSC const& other)
    : allocator_type(other.get_allocator())
    , m_front(other.m_front.load(memory_order::relaxed))
    , m_back(other.m_back.load(memory_order::relaxed))
    , m_pSubQueues(nullptr)
    , m_numSubQueues(other.m_numSubQueues)
{
    m_pSubQueues = (queue_type*)GTS_ALIGNED_MALLOC(m_numSubQueues * sizeof(queue_type), GTS_NO_SHARING_CACHE_LINE_SIZE);
    for (size_type ii = 0; ii < m_numSubQueues; ++ii)
    {
        new (m_pSubQueues + ii) queue_type(other.m_pSubQueues[ii]);
    }
}

//------------------------------------------------------------------------------
template<typename T, typename TMutex, typename TAllocator>
QueueMPSC<T, TMutex, TAllocator>::QueueMPSC(QueueMPSC&& other)
    : allocator_type(std::move(other.get_allocator()))
    , m_front(std::move(other.m_front.load(memory_order::relaxed)))
    , m_back(std::move(other.m_back.load(memory_order::relaxed)))
    , m_pSubQueues(std::move(other.m_pSubQueues))
    , m_numSubQueues(std::move(other.m_numSubQueues))
{
    other.m_back.store(0, memory_order::relaxed);
    other.m_front.store(0, memory_order::relaxed);
    other.m_pSubQueues  = nullptr;
    other.m_numSubQueues = 0;
}

//------------------------------------------------------------------------------
template<typename T, typename TMutex, typename TAllocator>
QueueMPSC<T, TMutex, TAllocator>& QueueMPSC<T, TMutex, TAllocator>::operator=(QueueMPSC const& other)
{
    if (this != &other)
    {
        m_front.store(other.m_front.load(memory_order::relaxed), memory_order::relaxed);
        m_back.store(other.m_back.load(memory_order::relaxed), memory_order::relaxed);

        for (size_type ii = 0; ii < m_numSubQueues; ++ii)
        {
            m_pSubQueues[ii] = other.m_pSubQueues[ii];
        }
    }
    return *this;
}

//------------------------------------------------------------------------------
template<typename T, typename TMutex, typename TAllocator>
QueueMPSC<T, TMutex, TAllocator>& QueueMPSC<T, TMutex, TAllocator>::operator=(QueueMPSC&& other)
{
    if (this != &other)
    {
        for (size_type ii = 0; ii < m_numSubQueues; ++ii)
        {
        allocator_type::destroy(m_pSubQueues + ii);
        }
        allocator_type::deallocate(m_pSubQueues, m_numSubQueues);

        m_front.store(std::move(other.m_front.load(memory_order::relaxed)), memory_order::relaxed);
        m_back.store(std::move(other.m_back.load(memory_order::relaxed)), memory_order::relaxed);
        m_pSubQueues = std::move(other.m_pSubQueues);
        m_numSubQueues = std::move(other.m_numSubQueues);

        other.m_back.store(0, memory_order::relaxed);
        other.m_front.store(0, memory_order::relaxed);
        other.m_pSubQueues  = nullptr;
        other.m_numSubQueues = 0;
    }
    return *this;
}

//------------------------------------------------------------------------------
template<typename T, typename TMutex, typename TAllocator>
bool QueueMPSC<T, TMutex, TAllocator>::empty() const
{
    return size() == 0;
}

//------------------------------------------------------------------------------
template<typename T, typename TMutex, typename TAllocator>
typename QueueMPSC<T, TMutex, TAllocator>::size_type QueueMPSC<T, TMutex, TAllocator>::size() const
{
    return m_back.load(memory_order::acquire) - m_front.load(memory_order::acquire);
}

//------------------------------------------------------------------------------
template<typename T, typename TMutex, typename TAllocator>
typename QueueMPSC<T, TMutex, TAllocator>::size_type QueueMPSC<T, TMutex, TAllocator>::capacity() const
{
    size_type cap = 0;
    for (size_type ii = 0; ii < m_numSubQueues; ++ii)
    {
        cap += m_pSubQueues[ii].capacity();
    }
    return cap;
}

//------------------------------------------------------------------------------
template<typename T, typename TMutex, typename TAllocator>
typename QueueMPSC<T, TMutex, TAllocator>::allocator_type
QueueMPSC<T, TMutex, TAllocator>::get_allocator() const
{
    return *this;
}

//------------------------------------------------------------------------------
template<typename T, typename TMutex, typename TAllocator>
void QueueMPSC<T, TMutex, TAllocator>::reserve(size_type sizePow2)
{
    if (!isPow2(sizePow2))
    {
        sizePow2 = nextPow2(sizePow2);
    }

    // distribute the reserve.
    sizePow2 = gtsMax(m_numSubQueues, sizePow2) / m_numSubQueues;
    for (size_type ii = 0; ii < m_numSubQueues; ++ii)
    {
        m_pSubQueues[ii].reserve(sizePow2);
    }
}

//------------------------------------------------------------------------------
template<typename T, typename TMutex, typename TAllocator>
void QueueMPSC<T, TMutex, TAllocator>::clear()
{
    for (size_type ii = 0; ii < m_numSubQueues; ++ii)
    {
        m_pSubQueues[ii].clear();
    }

    m_front.store(0, memory_order::relaxed);
    m_back.store(0, memory_order::relaxed);
}

//------------------------------------------------------------------------------
template<typename T, typename TMutex, typename TAllocator>
bool QueueMPSC<T, TMutex, TAllocator>::tryPush(const value_type& val)
{
    size_type b = m_back.fetch_add(1, memory_order::acq_rel);
    return m_pSubQueues[index(b)].tryPush(b, val);
}

//------------------------------------------------------------------------------
template<typename T, typename TMutex, typename TAllocator>
bool QueueMPSC<T, TMutex, TAllocator>::tryPush(value_type&& val)
{
    size_type b = m_back.fetch_add(1, memory_order::acq_rel);
    return m_pSubQueues[index(b)].tryPush(b, std::move(val));
}

//------------------------------------------------------------------------------
template<typename T, typename TMutex, typename TAllocator>
bool QueueMPSC<T, TMutex, TAllocator>::tryPop(value_type& out)
{
    size_type f = m_front.load(memory_order::relaxed);
    if (f == m_back.load(memory_order::acquire))
    {
        return false;
    }

    m_front.store(f + 1, memory_order::relaxed);

    return m_pSubQueues[index(f)].tryPop(f, out);
}

//------------------------------------------------------------------------------
template<typename T, typename TMutex, typename TAllocator>
template<typename... TArgs>
void QueueMPSC<T, TMutex, TAllocator>::_constructQueues(TArgs&&... args)
{
    m_pSubQueues = (queue_type*)GTS_ALIGNED_MALLOC(m_numSubQueues * sizeof(queue_type), GTS_NO_SHARING_CACHE_LINE_SIZE);
    for (size_type ii = 0; ii < m_numSubQueues; ++ii)
    {
        new (m_pSubQueues + ii) queue_type(std::forward<TArgs>(args)...);
    }
}

} // namespace gts
