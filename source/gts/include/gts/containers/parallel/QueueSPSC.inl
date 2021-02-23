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

template<typename T, typename TAllocator>
constexpr typename QueueSPSC<T, TAllocator>::size_type QueueSPSC<T, TAllocator>::MAX_CAPACITY;

template<typename T, typename TAllocator>
constexpr typename QueueSPSC<T, TAllocator>::size_type QueueSPSC<T, TAllocator>::MIN_CAPACITY;

 //------------------------------------------------------------------------------
template<typename T, typename TAllocator>
QueueSPSC<T, TAllocator>::QueueSPSC(allocator_type const& allocator)
    : allocator_type(allocator)
    , m_front(0)
    , m_back(0)
    , m_pRingBuffer(nullptr)
    , m_ppDataBackingArray(nullptr)
    , m_dataBackingArraySize(0)
    , m_pOldBuffers(nullptr)
{}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
QueueSPSC<T, TAllocator>::~QueueSPSC()
{
    clear();
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
QueueSPSC<T, TAllocator>::QueueSPSC(QueueSPSC const& other)
    : allocator_type(other.get_allocator())
    , m_front(other.m_front.load(memory_order::relaxed))
    , m_back(other.m_back.load(memory_order::relaxed))
    , m_pRingBuffer(nullptr)
    , m_ppDataBackingArray(nullptr)
    , m_dataBackingArraySize(other.m_dataBackingArraySize)
    , m_pOldBuffers(nullptr)
{
    _deepCopy(other);
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
QueueSPSC<T, TAllocator>::QueueSPSC(QueueSPSC&& other)
    : allocator_type(std::move(other.get_allocator()))
    , m_front(other.m_front.load(memory_order::relaxed))
    , m_back(other.m_back.load(memory_order::relaxed))
    , m_pRingBuffer(other.m_pRingBuffer.load(memory_order::relaxed))
    , m_ppDataBackingArray(other.m_ppDataBackingArray)
    , m_dataBackingArraySize(other.m_dataBackingArraySize)
    , m_pOldBuffers(nullptr)
{
    other.m_back.store(0, memory_order::relaxed);
    other.m_front.store(0, memory_order::relaxed);
    other.m_pRingBuffer.store(nullptr, memory_order::relaxed);
    other.m_ppDataBackingArray   = nullptr;
    other.m_dataBackingArraySize = 0;
    other._cleanup();
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
QueueSPSC<T, TAllocator>& QueueSPSC<T, TAllocator>::operator=(QueueSPSC const& other)
{
    if (this != &other)
    {
        clear();
        (allocator_type)*this = other.get_allocator();
        _deepCopy(other);
    }
    return *this;
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
QueueSPSC<T, TAllocator>& QueueSPSC<T, TAllocator>::operator=(QueueSPSC&& other)
{
    if (this != &other)
    {
        m_front.store(other.m_front.load(memory_order::relaxed), memory_order::relaxed);
        m_back.store(other.m_back.load(memory_order::relaxed), memory_order::relaxed);
        m_pRingBuffer.store(other.m_pRingBuffer.load(memory_order::relaxed), memory_order::relaxed);
        m_ppDataBackingArray   = other.m_ppDataBackingArray;
        m_dataBackingArraySize = other.m_dataBackingArraySize;
        (allocator_type)*this  = std::move(other.get_allocator());

        other.m_back.store(0, memory_order::relaxed);
        other.m_front.store(0, memory_order::relaxed);
        other.m_pRingBuffer.store(nullptr, memory_order::relaxed);
        other.m_ppDataBackingArray   = nullptr;
        other.m_dataBackingArraySize = 0;
        other._cleanup();
    }
    return *this;
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
bool QueueSPSC<T, TAllocator>::empty() const
{
    return size() == 0;
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
typename QueueSPSC<T, TAllocator>::size_type QueueSPSC<T, TAllocator>::size() const
{
    return m_back.load(memory_order::acquire) - m_front.load(memory_order::acquire);
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
typename QueueSPSC<T, TAllocator>::size_type QueueSPSC<T, TAllocator>::capacity() const
{
    RingBuffer* pBuffer = m_pRingBuffer.load(memory_order::acquire);
    if (pBuffer)
    {
        return pBuffer->mask + 1;
    }
    return 0;
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
typename QueueSPSC<T, TAllocator>::allocator_type 
QueueSPSC<T, TAllocator>::get_allocator() const
{
    return *this;
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
void QueueSPSC<T, TAllocator>::clear()
{
    _cleanup();

    // Delete data backing.
    for (size_type ii = 0; ii < m_dataBackingArraySize; ++ii)
    {
        size_type prevSize = ii ? (size_type(1) << ii) : 0;
        size_type currSize = (size_type(1) << (ii + 1)) - prevSize;

        for (size_type jj = 0; jj < currSize; ++jj)
        {
            allocator_type::destroy(m_ppDataBackingArray[ii] + jj);
        }
        allocator_type::deallocate(m_ppDataBackingArray[ii], currSize);
    }
    allocator_type::vector_delete_object(m_ppDataBackingArray, m_dataBackingArraySize);
    m_ppDataBackingArray = nullptr;
    m_dataBackingArraySize = 0;

    m_front.store(0, memory_order::relaxed);
    m_back.store(0, memory_order::relaxed);

    // Delete current table.
    RingBuffer* pBuffer = m_pRingBuffer.exchange(nullptr, memory_order::acq_rel);
    if (pBuffer)
    {
        allocator_type::vector_delete_object(pBuffer->ppBuff, pBuffer->mask + 1);
        allocator_type::delete_object(pBuffer);
    }
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
bool QueueSPSC<T, TAllocator>::tryPush(const value_type& val)
{
    return _insert(val);
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
bool QueueSPSC<T, TAllocator>::tryPush(value_type&& val)
{
    return _insert(std::move(val));
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
bool QueueSPSC<T, TAllocator>::tryPop(value_type& out)
{
    size_type b = m_back.load(memory_order::relaxed);
    size_type f = m_front.load(memory_order::acquire);
    bool result = true;

    if (b > f)
    {
        RingBuffer* pBuffer = m_pRingBuffer.load(memory_order::relaxed);
        out                 = *(pBuffer->ppBuff[f & pBuffer->mask]);
        allocator_type::destroy(pBuffer->ppBuff[f & pBuffer->mask]);

        m_front.store(f + 1, memory_order::release);
    }
    else
    {
        result = false;
    }

    _cleanup();
    return result;
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
template<typename... TArgs>
bool QueueSPSC<T, TAllocator>::_insert(TArgs&&... args)
{
    size_type b = m_back.load(memory_order::relaxed);
    size_type f = m_front.load(memory_order::acquire);

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
        if (!_grow(gtsMax(MIN_CAPACITY, cap * 2), f, b))
        {
            return false;
        }
    }

    RingBuffer* pBuffer = m_pRingBuffer.load(memory_order::relaxed);
    allocator_type::construct(pBuffer->ppBuff[b & pBuffer->mask], std::forward<TArgs>(args)...);

    m_back.store(b + 1, memory_order::release);

    return true;
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
void QueueSPSC<T, TAllocator>::_cleanup()
{
    OldBuffers* pOldBuffers = m_pOldBuffers.load(memory_order::acquire);
    if (pOldBuffers != nullptr)
    {
        // Take the old buffers so we don't race with _grow();
        pOldBuffers = m_pOldBuffers.exchange(nullptr, memory_order::release);

        if(pOldBuffers != nullptr)
        {
            // Delete all the old buffers.
            for (size_type ii = 0; ii < pOldBuffers->size; ++ii)
            {
                allocator_type::vector_delete_object(pOldBuffers->ppBuff[ii]->ppBuff, pOldBuffers->ppBuff[ii]->mask + 1);
                allocator_type::delete_object(pOldBuffers->ppBuff[ii]);
            }
            allocator_type::vector_delete_object(pOldBuffers->ppBuff, pOldBuffers->size);
            allocator_type::delete_object(pOldBuffers);
        }
    }
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
bool QueueSPSC<T, TAllocator>::_grow(size_type size, size_type front, size_type back)
{
    if (size < MIN_CAPACITY || size > MAX_CAPACITY)
    {
        // can't grow anymore.
        return false;
    }

    RingBuffer* pOldBuffer = m_pRingBuffer.load(memory_order::relaxed);
    size_type oldMask      = pOldBuffer ? pOldBuffer->mask : 0;

    size_type newMask      = size - 1;
    RingBuffer* pNewBuffer = allocator_type::template new_object<RingBuffer>();
    pNewBuffer->ppBuff     = allocator_type::template vector_new_object<value_type*>(size);
    pNewBuffer->mask       = newMask;
    memset(pNewBuffer->ppBuff, 0, sizeof(value_type*) * size);

    // Copy the elements into the new buffer.
    for (size_type ii = front; ii != back; ++ii)
    {
        pNewBuffer->ppBuff[ii & newMask] = pOldBuffer->ppBuff[ii & oldMask];
    }

    // Grow data backing array.
    value_type** ppNewDataBackingArray = allocator_type::template vector_new_object<value_type*>(m_dataBackingArraySize + 1); // TODO: consider table doubling
    for (size_type ii = 0; ii < m_dataBackingArraySize; ++ii)
    {
        ppNewDataBackingArray[ii] = m_ppDataBackingArray[ii];
    }

    // Add a new slab to the data backing.
    size_type totalCurrentBackingSize             = pOldBuffer ? (size_type(1) << m_dataBackingArraySize) : 0;
    size_type newSlabSize                         = (size_type(1) << (m_dataBackingArraySize + 1)) - totalCurrentBackingSize;
    value_type* pNewSlab                          = allocator_type::template allocate<value_type>(newSlabSize);
    ppNewDataBackingArray[m_dataBackingArraySize] = pNewSlab;

    // Replace the old data backing array.
    allocator_type::vector_delete_object(m_ppDataBackingArray, m_dataBackingArraySize);
    m_ppDataBackingArray = ppNewDataBackingArray;
    m_dataBackingArraySize++;

    // Insert new slab's elements to the new buffer's holes.
    size_type slabIdx = 0;
    for (size_type ii = 0; ii < pNewBuffer->mask + 1; ++ii)
    {
        if(pNewBuffer->ppBuff[ii] == nullptr)
        {
            pNewBuffer->ppBuff[ii] = &pNewSlab[slabIdx++];
        }
    }

    // Publish the new buffer.
    m_pRingBuffer.store(pNewBuffer, memory_order::release);

    // Save the old buffer since the consumer might still be using it.
    if(pOldBuffer)
    {
        // Take the old buffers so we don't race with cleanup().
        OldBuffers* pOldBuffers = m_pOldBuffers.exchange(nullptr, memory_order::release);
        if (pOldBuffers == nullptr)
        {
            // Already cleaned-up. Make a new one.
            pOldBuffers         = allocator_type::template new_object<OldBuffers>();
            pOldBuffers->ppBuff = allocator_type::template vector_new_object<RingBuffer*>(1);
            pOldBuffers->size   = 1;

            // Add the new old buffer.
            pOldBuffers->ppBuff[0] = pOldBuffer;
        }
        else
        {
            // Grow the old table buffer.
            RingBuffer** ppNewOldBuffersArray = allocator_type::template vector_new_object<RingBuffer*>(pOldBuffers->size + 1); // TODO: consider table doubling
            for (size_type ii = 0; ii < pOldBuffers->size; ++ii)
            {
                ppNewOldBuffersArray[ii] = pOldBuffers->ppBuff[ii];
            }

            // Add the new old buffer.
            ppNewOldBuffersArray[pOldBuffers->size] = pOldBuffer;

            // Replace the old table buffer.
            allocator_type::vector_delete_object(pOldBuffers->ppBuff, pOldBuffers->size);
            pOldBuffers->ppBuff = ppNewOldBuffersArray;
            pOldBuffers->size++;
        }

        // Publish the old buffer for cleanup.
        m_pOldBuffers.store(pOldBuffers, memory_order::release);
    }

    return true;
}

//------------------------------------------------------------------------------
template<typename T, typename TAllocator>
void QueueSPSC<T, TAllocator>::_deepCopy(QueueSPSC const& src)
{
    RingBuffer* pSrcRingBuffer = src.m_pRingBuffer.load(memory_order::relaxed);
    if (pSrcRingBuffer)
    {
        // Create a RingBuffer;
        RingBuffer* pRingBuffer = allocator_type::template new_object<RingBuffer>();
        pRingBuffer->ppBuff     = allocator_type::template vector_new_object<value_type*>(pSrcRingBuffer->mask + 1);
        pRingBuffer->mask       = pSrcRingBuffer->mask;

        //
        // Copy data backing.

        GTS_ASSERT(src.m_dataBackingArraySize > 0 && src.m_ppDataBackingArray != nullptr);
        m_dataBackingArraySize = src.m_dataBackingArraySize;
        m_ppDataBackingArray   = allocator_type::template vector_new_object<value_type*>(m_dataBackingArraySize);
        
        // Copy each slab from src and assign its elements to the ring buffer.
        size_type ringIdx = 0;
        for (size_type ii = 0; ii < m_dataBackingArraySize; ++ii)
        {
            size_type prevSize = ii ? (size_type(1) << ii) : 0;
            size_type currSize = (size_type(1) << (ii + 1)) - prevSize;

            value_type* pNewSlab = allocator_type::template allocate<value_type>(currSize);
            m_ppDataBackingArray[ii] = pNewSlab;

            for (size_type jj = 0; jj < currSize; ++jj)
            {
                pRingBuffer->ppBuff[ringIdx++] = pNewSlab + jj;
            }
        }

        //
        // Copy RingBuffer elements.

        size_type f = src.m_front.load(memory_order::relaxed);
        size_type b = src.m_back.load(memory_order::relaxed);

        for (size_type ii = f; ii != b; ++ii)
        {
            allocator_type::construct(pRingBuffer->ppBuff[ii & pRingBuffer->mask], *(pSrcRingBuffer->ppBuff[ii & pSrcRingBuffer->mask])); 
        }

        m_front.store(f, memory_order::relaxed);
        m_back.store(b, memory_order::relaxed);
        m_pRingBuffer.store(pRingBuffer, memory_order::relaxed);
    }
}
