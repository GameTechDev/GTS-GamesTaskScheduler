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

namespace internal {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// PARALLEL_SUB_VECTOR

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
ParallelSubVector<T, TSharedMutex, TAllocator>::ParallelSubVector(size_type numVecs, allocator_type const& allocator)
    : allocator_type(allocator)
    , m_size(0)
    , m_ticketMask(~numVecs + 1)
    , m_ticketShift(size_type(log2i(numVecs)))
    , m_capacity(0)
    , m_ppSlabs(nullptr)
    , m_slabCount(0)
{
    size_type numSlabs = log2i(numericLimits<size_type>::max()) + 1;
    m_ppSlabs = allocator_type::template vector_new_object<slot_type*>(numSlabs);
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
ParallelSubVector<T, TSharedMutex, TAllocator>::ParallelSubVector(ParallelSubVector const& other)
    : allocator_type(other.get_allocator())
    , m_size(other.m_size.load(memory_order::acquire))
    , m_ticketMask(other.m_ticketMask)
    , m_ticketShift(other.m_ticketShift)
    , m_capacity(0)
    , m_ppSlabs(nullptr)
    , m_slabCount(0)
{
    size_type numSlabs = log2i(numericLimits<size_type>::max()) + 1;
    m_ppSlabs = allocator_type::template vector_new_object<slot_type*>(numSlabs);

    size_type size = other.m_size.load(memory_order::relaxed);
    reserve(nextPow2(size));

    _deepCopy(other);
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
ParallelSubVector<T, TSharedMutex, TAllocator>::ParallelSubVector(ParallelSubVector&& other)
    : allocator_type(std::move(other.get_allocator()))
    , m_size(std::move(m_size.load(memory_order::acquire)))
    , m_ticketMask(std::move(other.m_ticketMask))
    , m_ticketShift(std::move(other.m_ticketShift))
    , m_capacity(std::move(other.m_capacity))
    , m_ppSlabs(std::move(other.m_ppSlabs))
    , m_slabCount(std::move(other.m_slabCount))
{
    other.m_ticketMask  = 0;
    other.m_ticketShift = 0;
    other.m_ppSlabs     = nullptr;
    other.m_slabCount   = 0;
    other.m_size.store(0, memory_order::relaxed);
    other.m_capacity = 0;
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
ParallelSubVector<T, TSharedMutex, TAllocator>&
ParallelSubVector<T, TSharedMutex, TAllocator>::operator=(ParallelSubVector const& other)
{
    if (this != &other)
    {
        GTS_ASSERT(m_ticketMask == other.m_ticketMask && m_ticketShift == other.m_ticketShift);

        clear();
        _destroy();

        size_type size = other.m_size.load(memory_order::acquire);
        reserve(nextPow2(size));

        (allocator_type)*this = other.get_allocator();
        _deepCopy(other);
    }
    return *this;
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
ParallelSubVector<T, TSharedMutex, TAllocator>&
ParallelSubVector<T, TSharedMutex, TAllocator>::operator=(ParallelSubVector&& other)
{
    if (this != &other)
    {
        GTS_ASSERT(m_ticketMask == other.m_ticketMask && m_ticketShift == other.m_ticketShift);

        m_ppSlabs         = std::move(other.m_ppSlabs);
        m_slabCount       = std::move(other.m_slabCount);
        m_size.store(std::move(other.m_size.load(memory_order::acquire)), memory_order::relaxed);
        m_capacity        = 0;
        (allocator_type)*this = std::move(other.get_allocator());

        other.m_ticketMask  = 0;
        other.m_ticketShift = 0;
        other.m_ppSlabs     = 0;
        other.m_slabCount   = 0;
        other.m_size.store(0, memory_order::relaxed);
        other.m_capacity    = 0;
    }
    return *this;
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
ParallelSubVector<T, TSharedMutex, TAllocator>::~ParallelSubVector()
{
    clear();
    _destroy();
    size_type numSlabs = log2i(numericLimits<size_type>::max()) + 1;
    allocator_type::vector_delete_object(m_ppSlabs, numSlabs);
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
template<typename... TArgs>
void ParallelSubVector<T, TSharedMutex, TAllocator>::emplaceBack(size_type globalIdx, TArgs&&... args)
{
    size_type localIdx = _toLocal(globalIdx);

    // Wait for our turn.
    backoff_type backoff;
    while (localIdx != m_size.load(memory_order::acquire))
    {
        backoff.tick();
    }
     
    if(localIdx == m_capacity)
    {
        // Only the first element per slab grows the vector.
        _grow(m_capacity * 2);
    }

    size_type slabIdx = 0;
    size_type idx     = 0;
    _toSlabLocation(localIdx, slabIdx, idx);

    slot_type& mySlot = m_ppSlabs[slabIdx][idx];
    allocator_type::construct(&mySlot.value, std::forward<TArgs>(args)...);
    mySlot.isEmpty.store(false, memory_order::relaxed);

    m_size.fetch_add(1, memory_order::release);
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
template<bool useLock>
typename ParallelSubVector<T, TSharedMutex, TAllocator>::value_type&&
ParallelSubVector<T, TSharedMutex, TAllocator>::pop_back_and_get(size_type globalIdx)
{
    size_type localIdx = _toLocal(globalIdx);
    size_type slabIdx  = 0;
    size_type idx      = 0;
    _toSlabLocation(localIdx, slabIdx, idx);

    slot_type& mySlot = m_ppSlabs[slabIdx][idx];

    if (useLock)
    {
        mySlot.valueGuard.lock();
    }

    value_type&& poppedVal = std::move(m_ppSlabs[slabIdx][idx].value);
    mySlot.isEmpty.store(true, memory_order::release);

    if (useLock)
    {
        mySlot.valueGuard.unlock();
    }

    m_size.store(m_size.load(memory_order::relaxed) - 1, memory_order::release);
    return std::move(poppedVal);
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
typename ParallelSubVector<T, TSharedMutex, TAllocator>::slot_type*
ParallelSubVector<T, TSharedMutex, TAllocator>::readAt(size_type globalIdx) const
{
    size_type localIdx = _toLocal(globalIdx);
    size_type slabIdx  = 0;
    size_type idx      = 0;
    _toSlabLocation(localIdx, slabIdx, idx);

    slot_type* pSlab = m_ppSlabs[slabIdx];
    if (!pSlab)
    {
        return nullptr;
    }

    slot_type& mySlot = pSlab[idx];
    mySlot.valueGuard.lock_shared();

    if (mySlot.isEmpty.load(memory_order::acquire))
    {
        mySlot.valueGuard.unlock_shared();
        return nullptr;
    }

    return &mySlot;
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
typename ParallelSubVector<T, TSharedMutex, TAllocator>::slot_type*
ParallelSubVector<T, TSharedMutex, TAllocator>::writeAt(size_type globalIdx)
{
    size_type localIdx = _toLocal(globalIdx);
    size_type slabIdx  = 0;
    size_type idx      = 0;
    _toSlabLocation(localIdx, slabIdx, idx);

    slot_type* pSlab = m_ppSlabs[slabIdx];
    if (!pSlab)
    {
        return nullptr;
    }

    slot_type& mySlot = pSlab[idx];
    mySlot.valueGuard.lock();

    if (mySlot.isEmpty.load(memory_order::acquire))
    {
        mySlot.valueGuard.unlock();
        return nullptr;
    }

    return &mySlot;
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
void ParallelSubVector<T, TSharedMutex, TAllocator>::reserve(size_type newCapacity)
{
    if(newCapacity > capacity())
    {
        size_type startSlab = _toSlabIndex(capacity());
        size_type endSlab   = _toSlabIndex(newCapacity);

        if (m_slabCount == 0)
        {
            size_type newSlabSize    = size_type(1) << (m_slabCount + 1);
            slot_type* pNewSlab      = allocator_type::template allocate<slot_type>(newSlabSize);
            m_ppSlabs[m_slabCount++] = pNewSlab;
            ++startSlab;
        }

        for (size_type ii = startSlab; ii < endSlab; ++ii)
        {
            // Add a new slab to the data backing.
            size_type totalSlotsSize = size_type(1) << m_slabCount;
            size_type newSlabSize    = (size_type(1) << (m_slabCount + 1)) - totalSlotsSize;
            slot_type* pNewSlab      = allocator_type::template  allocate<slot_type>(newSlabSize);
            m_ppSlabs[m_slabCount++] = pNewSlab;
        }

        m_capacity = newCapacity;
    }
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
void ParallelSubVector<T, TSharedMutex, TAllocator>::clear()
{
    size_type size = m_size.load(memory_order::acquire);

    while (size > 0)
    {
        size_type localIdx = --size;
        size_type slabIdx = 0;
        size_type idx = 0;
        _toSlabLocation(localIdx, slabIdx, idx);

        Lock<mutex_type> lock(m_ppSlabs[slabIdx][idx].valueGuard);
        allocator_type::destroy(&(m_ppSlabs[slabIdx][idx].value));
    }
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
void ParallelSubVector<T, TSharedMutex, TAllocator>::shrinkToFit()
{
    size_type size = m_size.load(memory_order::acquire);

    if(size > 0)
    {
        size_type startSlab = _toSlabIndex(size - 1);
        size_type endSlab   = _toSlabIndex(capacity() - 1);

        for (size_type iSlab = startSlab + 1; iSlab <= endSlab; ++iSlab)
        {
            size_type prevSlabSize = iSlab == 0 ? 0 : (size_type(1) << (iSlab - 1));
            size_type slabSize     = (size_type(1) << iSlab) - prevSlabSize;

            allocator_type::deallocate(m_ppSlabs[iSlab], slabSize);
            m_slabCount--;
        }
    }
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
typename ParallelSubVector<T, TSharedMutex, TAllocator>::size_type
ParallelSubVector<T, TSharedMutex, TAllocator>::capacity() const
{
    return m_capacity;
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
typename ParallelSubVector<T, TSharedMutex, TAllocator>::allocator_type 
ParallelSubVector<T, TSharedMutex, TAllocator>::get_allocator() const
{
    return *this;
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
typename ParallelSubVector<T, TSharedMutex, TAllocator>::size_type
ParallelSubVector<T, TSharedMutex, TAllocator>::_toLocal(size_type globalIdx) const
{
    // Transform the globalIdx into the local index space.
    return (globalIdx & m_ticketMask) >> m_ticketShift;
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
typename ParallelSubVector<T, TSharedMutex, TAllocator>::size_type
ParallelSubVector<T, TSharedMutex, TAllocator>::_toSlabIndex(size_type localIdx) const
{
    // The slab where the localIdx is located.
    return log2i(localIdx | 1); // (| 1) to map {0,1} -> {0}.
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
void ParallelSubVector<T, TSharedMutex, TAllocator>::_toSlabLocation(size_type localIdx, size_type& slabIdx, size_type& slotIdx) const
{
    // The slab where the localIdx is located.
    slabIdx = _toSlabIndex(localIdx);

    // Map the base index of the slab to local space.
    size_type slabBaseIdx = (size_type(1) << slabIdx)
        & ~size_type(1); // (~1) to map {0,1} -> {0}.

    // Map the localIdx to slab space.
    slotIdx = localIdx - slabBaseIdx;
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
void ParallelSubVector<T, TSharedMutex, TAllocator>::_grow(size_type capacity)
{
    size_type newCapacity = capacity == 0 ? INIT_SIZE : capacity;

    if (newCapacity <= m_capacity)
    {
        GTS_ASSERT(0);
        return;
    }


    size_type startSlab = _toSlabIndex(m_capacity);
    size_type endSlab  = _toSlabIndex(newCapacity);

    if (m_slabCount == 0)
    {
        size_type newSlabSize    = size_type(1) << (m_slabCount + 1);
        slot_type* pNewSlab      = allocator_type::template allocate<slot_type>(newSlabSize);
        m_ppSlabs[m_slabCount++] = pNewSlab;
        ++startSlab;

        for (size_type iSlot = 0; iSlot < newSlabSize; ++iSlot)
        {
            new (&pNewSlab[iSlot].valueGuard) mutex_type();
            pNewSlab[iSlot].isEmpty.store(true, memory_order::relaxed);
        }
    }

    for (size_type ii = startSlab; ii < endSlab; ++ii)
    {
        // Add a new slab to the data backing.
        size_type totalSlotsSize = size_type(1) << m_slabCount;
        size_type newSlabSize    = (size_type(1) << (m_slabCount + 1)) - totalSlotsSize;
        slot_type* pNewSlab      = allocator_type::template allocate<slot_type>(newSlabSize);
        m_ppSlabs[m_slabCount++] = pNewSlab;

        for (size_type iSlot = 0; iSlot < newSlabSize; ++iSlot)
        {
            new (&pNewSlab[iSlot].valueGuard) mutex_type();
            pNewSlab[iSlot].isEmpty.store(true, memory_order::relaxed);
        }
    }

    m_capacity = newCapacity;
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
void ParallelSubVector<T, TSharedMutex, TAllocator>::_destroy()
{
    for (size_type ii = 0; ii < m_slabCount; ++ii)
    {
        size_type prevSlabSize = ii == 0 ? 0 : (size_type(1) << (ii - 1));
        size_type slabSize     = (size_type(1) << (ii + 1)) - prevSlabSize;

        allocator_type::destroy(m_ppSlabs[ii]);
        allocator_type::deallocate(m_ppSlabs[ii], slabSize);
    }
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
void ParallelSubVector<T, TSharedMutex, TAllocator>::_deepCopy(ParallelSubVector const& src)
{
    if (src.m_slabCount == 0)
    {
        return;
    }

    // First slab.
    ::memcpy(m_ppSlabs[0], src.m_ppSlabs[0], sizeof(slot_type) * INIT_SIZE);

    // Subsequent slabs.
    for (size_type ii = 1; ii < m_slabCount; ++ii)
    {
        size_type prevSize = (ii - 1) == 0 ? 0 : (size_type(1) << (ii - 1));
        size_type slabSize = (size_type(1) << ii) - prevSize;
        ::memcpy(m_ppSlabs[ii], src.m_ppSlabs[ii], sizeof(slot_type) * slabSize);
    }
}

} // namespace internal

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// ParallelVector::base_const_iterator

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
ParallelVector<T, TSharedMutex, TAllocator>::base_const_iterator::base_const_iterator(ParallelVector const* pVec, slot_type* pSlot, size_type index)
    : m_pVec(pVec)
    , m_pSlot(pSlot)
    , m_index(index)
{}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
ParallelVector<T, TSharedMutex, TAllocator>::base_const_iterator::base_const_iterator(base_const_iterator&& other)
    : m_pVec(std::move(other.m_pVec))
    , m_pSlot(std::move(other.m_pSlot))
    , m_index(std::move(other.m_index))
{
    other._invalidate();
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
typename ParallelVector<T, TSharedMutex, TAllocator>::base_const_iterator& 
ParallelVector<T, TSharedMutex, TAllocator>::base_const_iterator::operator=(base_const_iterator&& other)
{
    if (this != &other)
    {
        m_pVec   = std::move(other.m_pVec);
        m_pSlot  = std::move(other.m_pSlot);
        m_index  = std::move(other.m_index);

        other._invalidate();
    }
    return *this;
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
typename ParallelVector<T, TSharedMutex, TAllocator>::base_const_iterator& 
ParallelVector<T, TSharedMutex, TAllocator>::base_const_iterator::operator++()
{
    _advance(m_pVec, m_pSlot, m_index);
    return *this;
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
typename ParallelVector<T, TSharedMutex, TAllocator>::value_type const*
ParallelVector<T, TSharedMutex, TAllocator>::base_const_iterator::operator->() const
{
    return &(m_pSlot->value);
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
typename ParallelVector<T, TSharedMutex, TAllocator>::value_type const&
ParallelVector<T, TSharedMutex, TAllocator>::base_const_iterator::operator*() const
{
    return m_pSlot->value;
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
bool ParallelVector<T, TSharedMutex, TAllocator>::base_const_iterator::operator==(base_const_iterator const& other) const
{
    return m_pSlot == other.m_pSlot && m_index == other.m_index;
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
bool ParallelVector<T, TSharedMutex, TAllocator>::base_const_iterator::operator!=(base_const_iterator const& other) const
{
    return !(*this == other);
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
void ParallelVector<T, TSharedMutex, TAllocator>::base_const_iterator::_invalidate()
{
    m_pVec   = nullptr;
    m_pSlot  = nullptr;
    m_index  = ParallelVector::BAD_INDEX;
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
void ParallelVector<T, TSharedMutex, TAllocator>::base_const_iterator::_advance(ParallelVector const* pVec, slot_type*& pSlot, size_type& index)
{
    if(pVec && pSlot)
    {
        // Unlock this slot.
        pSlot->valueGuard.unlock_shared();

        if(index + 1 < pVec->size())
        {
            ++index;

            // Try to get the next slot.
            pSlot = pVec->_readAt(index);

            if (pSlot)
            {
                // Success.
                return;
            }
        }
    }

    // Fail. End of vector.
    pSlot  = nullptr;
    index  = BAD_INDEX;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// ParallelVector::const_iterator

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
ParallelVector<T, TSharedMutex, TAllocator>::const_iterator::const_iterator(ParallelVector const* pVec, slot_type* pSlot, size_type index)
    : base_const_iterator(pVec, pSlot, index)
{
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
ParallelVector<T, TSharedMutex, TAllocator>::const_iterator::~const_iterator()
{
    if (base_const_iterator::m_pSlot)
    {
        base_const_iterator::m_pSlot->valueGuard.unlock_shared();
    }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// ParallelVector::iterator

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
ParallelVector<T, TSharedMutex, TAllocator>::iterator::iterator(ParallelVector* pVec, slot_type* pSlot, size_type index)
    : base_const_iterator(pVec, pSlot, index)
{
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
ParallelVector<T, TSharedMutex, TAllocator>::iterator::~iterator()
{
    if (base_const_iterator::m_pSlot)
    {
        base_const_iterator::m_pSlot->valueGuard.unlock();
    }
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
typename ParallelVector<T, TSharedMutex, TAllocator>::iterator&
ParallelVector<T, TSharedMutex, TAllocator>::iterator::operator++()
{
    _advance(const_cast<ParallelVector*>(base_const_iterator::m_pVec), base_const_iterator::m_pSlot, base_const_iterator::m_index);
    return *this;
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
typename ParallelVector<T, TSharedMutex, TAllocator>::value_type*
ParallelVector<T, TSharedMutex, TAllocator>::iterator::operator->()
{
    return &(base_const_iterator::m_pSlot->value);
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
typename ParallelVector<T, TSharedMutex, TAllocator>::value_type&
ParallelVector<T, TSharedMutex, TAllocator>::iterator::operator*()
{
    return base_const_iterator::m_pSlot->value;
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
void ParallelVector<T, TSharedMutex, TAllocator>::iterator::_advance(ParallelVector* pVec, slot_type*& pSlot, size_type& index)
{
    if(pVec && pSlot)
    {
        // Unlock this slot.
        pSlot->valueGuard.unlock();

        if(index + 1 < pVec->size())
        {
            ++index;

            // Try to get the next slot.
            pSlot = pVec->_writeAt(index);

            if (pSlot)
            {
                // Success.
                return;
            }
        }
    }

    // Fail. End of vector.
    pSlot = nullptr;
    index = BAD_INDEX;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// ParallelVector

// STRUCTORS

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
ParallelVector<T, TSharedMutex, TAllocator>::ParallelVector(allocator_type const& allocator)
    : allocator_type(allocator)
    , m_size(0)
    , m_pStripedMutexes(nullptr)
    , m_pVecs(nullptr)
    , m_numVecs(0)
{
    m_numVecs = nextPow2(Thread::getHardwareThreadCount());
    _allocateVectors(allocator);
    m_pStripedMutexes = allocator_type::template vector_new_object<AlignedMutex>(m_numVecs);
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
ParallelVector<T, TSharedMutex, TAllocator>::ParallelVector(size_type subVectorCount, allocator_type const& allocator)
    : allocator_type(allocator)
    , m_size(0)
    , m_pStripedMutexes(nullptr)
    , m_pVecs(nullptr)
    , m_numVecs((uint32_t)subVectorCount)
{
    GTS_ASSERT(subVectorCount < UINT32_MAX);

    if (m_numVecs == 0)
    {
        GTS_ASSERT(0);
        m_numVecs = Thread::getHardwareThreadCount();
    }
    m_numVecs = nextPow2(m_numVecs);

    _allocateVectors(allocator);
    m_pStripedMutexes = allocator_type::template vector_new_object<AlignedMutex>(m_numVecs);
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
ParallelVector<T, TSharedMutex, TAllocator>::~ParallelVector()
{    
    allocator_type::vector_delete_object(m_pStripedMutexes, m_numVecs);
    _deallocateVectors();
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
ParallelVector<T, TSharedMutex, TAllocator>::ParallelVector(ParallelVector const& other)
    : allocator_type(other.get_allocator())
    , m_size(other.m_size.load(memory_order::relaxed))
    , m_pStripedMutexes(nullptr)
    , m_pVecs(nullptr)
    , m_numVecs(other.m_numVecs)
{
    m_size.store(other.m_size.load(memory_order::relaxed), memory_order::relaxed);
    m_numVecs = other.m_numVecs;
    _deepCopy(other);
    m_pStripedMutexes = allocator_type::template vector_new_object<AlignedMutex>(m_numVecs);
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
ParallelVector<T, TSharedMutex, TAllocator>::ParallelVector(ParallelVector&& other)
    : allocator_type(std::move(other.get_allocator()))
    , m_size(std::move(other.m_size.load(memory_order::relaxed)))
    , m_pStripedMutexes(std::move(other.m_pStripedMutexes))
    , m_pVecs(std::move(other.m_pVecs))
    , m_numVecs(std::move(other.m_numVecs))
{
    other.m_pStripedMutexes = nullptr;
    other.m_size.store(0, memory_order::relaxed);
    other.m_numVecs = 0;
    other.m_pVecs = nullptr;
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
ParallelVector<T, TSharedMutex, TAllocator>& ParallelVector<T, TSharedMutex, TAllocator>::operator=(ParallelVector const& other)
{
    if (this != &other)
    {
        if(m_pVecs)
        {
            _deallocateVectors();
        }

        m_size.store(other.m_size.load(memory_order::relaxed), memory_order::relaxed);
        m_numVecs             = other.m_numVecs;
        (allocator_type)*this = other.get_allocator();
        _deepCopy(other);
    }
    return *this;
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
ParallelVector<T, TSharedMutex, TAllocator>& ParallelVector<T, TSharedMutex, TAllocator>::operator=(ParallelVector&& other)
{
    if (this != &other)
    {
        if(m_pVecs)
        {
            _deallocateVectors();
        }

        m_size.store(std::move(other.m_size.load(memory_order::relaxed)), memory_order::relaxed);
        m_numVecs              = std::move(other.m_numVecs);
        m_pVecs                = std::move(other.m_pVecs);
        (allocator_type)*this  = std::move(other.get_allocator());

        other.m_size.store(0, memory_order::relaxed);
        other.m_numVecs = 0;
        other.m_pVecs = nullptr;
    }
    return *this;
}

// ITERATORS

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
typename ParallelVector<T, TSharedMutex, TAllocator>::const_iterator
ParallelVector<T, TSharedMutex, TAllocator>::cbegin() const
{
    slot_type* pSlot = _readAt(0);
    if(pSlot)
    {
        return const_iterator(this, pSlot, 0);
    }
    return const_iterator(this, nullptr, BAD_INDEX);
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
typename ParallelVector<T, TSharedMutex, TAllocator>::const_iterator
ParallelVector<T, TSharedMutex, TAllocator>::cend() const
{
    return const_iterator(this, nullptr, BAD_INDEX);
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
typename ParallelVector<T, TSharedMutex, TAllocator>::iterator
ParallelVector<T, TSharedMutex, TAllocator>::begin()
{
    slot_type* pSlot = _writeAt(0);
    if(pSlot)
    {
        return iterator(this, pSlot, 0);
    }
    return iterator(this, nullptr, BAD_INDEX);
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
typename ParallelVector<T, TSharedMutex, TAllocator>::iterator
ParallelVector<T, TSharedMutex, TAllocator>::end()
{
    return iterator(this, nullptr, BAD_INDEX);
}

// CAPACITY

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
typename ParallelVector<T, TSharedMutex, TAllocator>::size_type
ParallelVector<T, TSharedMutex, TAllocator>::capacity() const
{
    size_type cap = 0;
    for (size_type ii = 0; ii < m_numVecs; ++ii)
    {
        cap += m_pVecs[0].capacity() ;
    }
    return cap;
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
typename ParallelVector<T, TSharedMutex, TAllocator>::size_type
ParallelVector<T, TSharedMutex, TAllocator>::max_size() const
{
    return MAX_CAPACITY;
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
typename ParallelVector<T, TSharedMutex, TAllocator>::size_type
ParallelVector<T, TSharedMutex, TAllocator>::size() const
{
    return m_size.load(memory_order::acquire);
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
bool ParallelVector<T, TSharedMutex, TAllocator>::empty() const
{
    return size() == 0;
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
typename ParallelVector<T, TSharedMutex, TAllocator>::allocator_type 
ParallelVector<T, TSharedMutex, TAllocator>::get_allocator() const
{
    return *this;
}

// ACCESSORS

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
typename ParallelVector<T, TSharedMutex, TAllocator>::read_guard
ParallelVector<T, TSharedMutex, TAllocator>::cat(size_type index) const
{
    size_type mutexIdx = ThisThread::getId() & (m_numVecs - 1);
    LockShared<mutex_type> lock(m_pStripedMutexes[mutexIdx].m);

    slot_type* pSlot = _readAt(index);
    if(pSlot)
    {
        return read_guard(&pSlot->value, &pSlot->valueGuard, nullptr);
    }
    return read_guard(nullptr, nullptr, nullptr);
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
typename ParallelVector<T, TSharedMutex, TAllocator>::write_guard
ParallelVector<T, TSharedMutex, TAllocator>::at(size_type index)
{
    size_type mutexIdx = ThisThread::getId() & (m_numVecs - 1);
    LockShared<mutex_type> lock(m_pStripedMutexes[mutexIdx].m);

    slot_type* pSlot = _writeAt(index);
    if(pSlot)
    {
        return write_guard(&pSlot->value, &pSlot->valueGuard, nullptr);
    }
    return write_guard(nullptr, nullptr, nullptr);
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
typename ParallelVector<T, TSharedMutex, TAllocator>::read_guard
ParallelVector<T, TSharedMutex, TAllocator>::operator[](size_type index) const
{
    return cat(index);
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
typename ParallelVector<T, TSharedMutex, TAllocator>::write_guard
ParallelVector<T, TSharedMutex, TAllocator>::operator[](size_type index)
{
    return at(index);
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
typename ParallelVector<T, TSharedMutex, TAllocator>::read_guard
ParallelVector<T, TSharedMutex, TAllocator>::cfront() const
{
    return cat(0);
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
typename ParallelVector<T, TSharedMutex, TAllocator>::write_guard
ParallelVector<T, TSharedMutex, TAllocator>::front()
{
    return at(0);
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
typename ParallelVector<T, TSharedMutex, TAllocator>::read_guard
ParallelVector<T, TSharedMutex, TAllocator>::cback() const
{
    size_type mutexIdx = ThisThread::getId() & (m_numVecs - 1);
    LockShared<mutex_type> lock(m_pStripedMutexes[mutexIdx].m);
    size_type size = m_size.load(memory_order::acquire);
    return cat(size - 1);
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
typename ParallelVector<T, TSharedMutex, TAllocator>::write_guard
ParallelVector<T, TSharedMutex, TAllocator>::back()
{
    size_type mutexIdx = ThisThread::getId() & (m_numVecs - 1);
    LockShared<mutex_type> lock(m_pStripedMutexes[mutexIdx].m);
    size_type size = m_size.load(memory_order::acquire);
    return at(size - 1);
}

// MUTATORS

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
void ParallelVector<T, TSharedMutex, TAllocator>::reserve(size_type newCapacity)
{
    // Distribute the capacity over the sub vectors.
    size_type oldCap = m_pVecs[0].capacity();
    size_type newCap = nextPow2(newCapacity / m_numVecs + newCapacity % m_numVecs);
    if(newCap > oldCap)
    {
        for(size_type ii = oldCap; ii < newCap; ++ii)
        {
            m_pVecs[ii].reserve(newCap);
        }
    }
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
void ParallelVector<T, TSharedMutex, TAllocator>::resize(size_type size)
{
    _resize(size, value_type());
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
void ParallelVector<T, TSharedMutex, TAllocator>::resize(size_type size, value_type const& fill)
{
    _resize(size, fill);
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
void ParallelVector<T, TSharedMutex, TAllocator>::push_back(value_type const& value)
{
    _emplaceBack(value);
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
void ParallelVector<T, TSharedMutex, TAllocator>::push_back(value_type&& value)
{
    _emplaceBack(std::move(value));
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
template<typename... TArgs>
void ParallelVector<T, TSharedMutex, TAllocator>::emplace_back(TArgs&&... args)
{
    _emplaceBack(std::forward<TArgs>(args)...);
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
void ParallelVector<T, TSharedMutex, TAllocator>::pop_back()
{
    pop_back_result result = pop_back_and_get();
    if (result.isValid)
    {
        result.val.~value_type();
    }
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
typename ParallelVector<T, TSharedMutex, TAllocator>::pop_back_result
ParallelVector<T, TSharedMutex, TAllocator>::pop_back_and_get()
{
    _lock();

    size_type size = m_size.load(memory_order::relaxed);
    if (size == 0)
    {
        _unlock();
        return pop_back_result{ value_type(), false };
    }

    size--;
    m_size.store(size, memory_order::relaxed);
    value_type&& value = m_pVecs[size & (m_numVecs - 1)].template pop_back_and_get<true>(size);

    _unlock();

    return pop_back_result { std::move(value), true };
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
void ParallelVector<T, TSharedMutex, TAllocator>::swap_out(size_type index)
{
    pop_back_result backVal = pop_back_and_get();
    if (!backVal.isValid)
    {
        return;
    }

    slot_type* pSlot = m_pVecs[index & (m_numVecs - 1)].writeAt(index);
    if(pSlot)
    {
        pSlot->value = std::move(backVal.val);
        pSlot->valueGuard.unlock();
    }
    else
    {
        // The item at index no longer exists, and neither does the previous back.
        backVal.val.~value_type();
    }
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
void ParallelVector<T, TSharedMutex, TAllocator>::swap_out(iterator& iter)
{
    if (iter.m_index == BAD_INDEX)
    {
        GTS_ASSERT(0 && "Iterator is invalid.");
        return;
    }

    _lock();

    size_type size = m_size.load(memory_order::relaxed);
    if (size == 0)
    {
        GTS_ASSERT(0 && "ParallelVector is empty.");
        _unlock();
        return;
    }

    size--;
    m_size.store(size, memory_order::relaxed);
    value_type&& value = m_pVecs[size & (m_numVecs - 1)].template pop_back_and_get<false>(size);

    // If we are swapping out the back element,
    if (iter.m_index == size)
    {
        // just invalidate the iterator.
        iter.m_pSlot->valueGuard.unlock();
        iter._invalidate();
    }
    else
    {
        // Otherwise, swap in the back element value.
        iter.m_pSlot->value = std::move(value);
    }

    _unlock();
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
void ParallelVector<T, TSharedMutex, TAllocator>::clear()
{
    _lock();

    for (size_type ii = 0; ii < m_numVecs; ++ii)
    {
        m_pVecs[ii].clear();
    }
    m_size.store(0, memory_order::relaxed);

    _unlock();
}

// PRIVATE

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
template<typename... TArgs>
void ParallelVector<T, TSharedMutex, TAllocator>::_emplaceBack(TArgs&&... args)
{
    size_type mutexIdx = ThisThread::getId() & (m_numVecs - 1);
    LockShared<mutex_type> lock(m_pStripedMutexes[mutexIdx].m);
    size_type size = m_size.fetch_add(1, memory_order::acq_rel);
    m_pVecs[size & (m_numVecs - 1)].emplaceBack(size, std::forward<TArgs>(args)...);
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
void ParallelVector<T, TSharedMutex, TAllocator>::_allocateVectors(allocator_type const& allocator)
{
    m_pVecs = allocator_type::template allocate<ticket_vec>(m_numVecs);
    for (size_type ii = 0; ii < m_numVecs; ++ii)
    {
        allocator_type::construct(m_pVecs + ii, m_numVecs, allocator);
    }
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
void ParallelVector<T, TSharedMutex, TAllocator>::_deallocateVectors()
{
    for (size_type ii = 0; ii < m_numVecs; ++ii)
    {
        allocator_type::destroy(m_pVecs + ii);
    }
    allocator_type::deallocate(m_pVecs, m_numVecs);
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
void ParallelVector<T, TSharedMutex, TAllocator>::_deepCopy(ParallelVector const& src)
{
    m_pVecs = allocator_type::template allocate<ticket_vec>(m_numVecs);
    for (size_type ii = 0; ii < m_numVecs; ++ii)
    {
        allocator_type::construct(m_pVecs + ii, src.m_pVecs[ii]);
    }
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
void ParallelVector<T, TSharedMutex, TAllocator>::_resize(size_type count, value_type const& fill)
{
    if(count > 0)
    {
        size_type sz = m_size.load(memory_order::relaxed);
        if(count > sz)
        {
            while(count > sz)
            {
                _emplaceBack(fill);
                sz++;
            }
        }
        else if(count < sz)
        {
            while(count < sz)
            {
                pop_back();
                sz--;
            }

            for (size_type ii = 0; ii < m_numVecs; ++ii)
            {
                m_pVecs[ii].shrinkToFit();
            }
        }
    }
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
typename ParallelVector<T, TSharedMutex, TAllocator>::slot_type*
ParallelVector<T, TSharedMutex, TAllocator>::_readAt(size_type index) const
{
    return m_pVecs[index & (m_numVecs - 1)].readAt(index);
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
typename ParallelVector<T, TSharedMutex, TAllocator>::slot_type*
ParallelVector<T, TSharedMutex, TAllocator>::_writeAt(size_type index)
{
    return m_pVecs[index & (m_numVecs - 1)].writeAt(index);
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
void ParallelVector<T, TSharedMutex, TAllocator>::_lock()
{
    for (size_type ii = 0; ii < m_numVecs; ++ii)
    {
        m_pStripedMutexes[ii].m.lock();
    }
}

//------------------------------------------------------------------------------
template<typename T, typename TSharedMutex, typename TAllocator>
void ParallelVector<T, TSharedMutex, TAllocator>::_unlock()
{
    for (size_type ii = 0; ii < m_numVecs; ++ii)
    {
        m_pStripedMutexes[m_numVecs - ii - 1].m.unlock();
    }
}
