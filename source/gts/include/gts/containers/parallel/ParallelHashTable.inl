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


template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
constexpr uint8_t
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::slot_type::EMPTY;

template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
constexpr  uint8_t
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::slot_type::USED;

template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
constexpr uint8_t
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::slot_type::DELETED;


template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
constexpr typename ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::ProbeResult
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::PROBE_FAIL;

template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
constexpr typename ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::ProbeResult
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::PROBE_SUCCESS;

template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
constexpr typename ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::ProbeResult
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::PROBE_EXISTS;

template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
constexpr typename ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::ProbeResult
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::PROBE_TOMBSTONE;

template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
constexpr typename ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::ProbeResult
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::PROBE_GROW;


template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
constexpr typename ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::size_type
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::MAX_CAPACITY;

template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
constexpr typename ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::size_type
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::BAD_INDEX;

template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
constexpr typename ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::size_type
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::INIT_SIZE;

// BASE_CONST_ITERATOR

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::base_const_iterator::base_const_iterator(table_type* pTable, size_type index)
    : m_pTable(pTable)
    , m_index(index)
{}

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::base_const_iterator::base_const_iterator(base_const_iterator&& other)
    : m_pTable(other.m_pTable)
    , m_index(other.m_index)
{
    other.m_pTable = nullptr;
    other.m_index = ParallelHashTable::BAD_INDEX;
}

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
typename ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::base_const_iterator& 
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::base_const_iterator::operator=(base_const_iterator&& other)
{
    if (this != &other)
    {
        m_pTable = other.m_pTable;
        m_index = other.m_index;

        other.m_pTable = nullptr;
        other.m_index = ParallelHashTable::BAD_INDEX;
    }
    return *this;
}

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
typename ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::base_const_iterator& 
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::base_const_iterator::operator++()
{
    _advance(m_pTable, m_index);
    return *this;
}

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
typename ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::value_type const*
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::base_const_iterator::operator->() const
{
    return &(m_pTable->ppSlots[m_index]->keyVal);
}

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
typename ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::value_type const&
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::base_const_iterator::operator*() const
{
    return m_pTable->ppSlots[m_index]->keyVal;
}


//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
bool ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::base_const_iterator::operator==(base_const_iterator const& other) const
{
    return m_pTable == other.m_pTable && m_index == other.m_index;
}

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
bool ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::base_const_iterator::operator!=(base_const_iterator const& other) const
{
    return !(*this == other);
}

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
void ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::base_const_iterator::_advance(table_type*& pTable, size_type& index)
{
    if(pTable)
    {
        pTable->ppSlots[index]->valueGuard.unlock_shared();

        while(index + 1 < pTable->capacity)
        {
            ++index;
            pTable->ppSlots[index]->valueGuard.lock_shared();
            if (pTable->ppSlots[index]->state == slot_type::USED)
            {
                return;
            }
        }
    }

    // End of table.
    pTable->ppSlots[index]->valueGuard.unlock_shared();
    index = ParallelHashTable::BAD_INDEX;
    pTable = nullptr;
}

// CONST_ITERATOR

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::const_iterator::const_iterator(table_type* pTable, size_type index, bool initProb)
    : base_const_iterator(pTable, index)
{
    if(base_const_iterator::m_pTable && initProb)
    {
        while(base_const_iterator::m_index < base_const_iterator::m_pTable->capacity)
        {
            base_const_iterator::m_pTable->ppSlots[base_const_iterator::m_index]->valueGuard.lock_shared();
            if (base_const_iterator::m_pTable->ppSlots[base_const_iterator::m_index]->state == slot_type::USED)
            {
                return;
            }
            base_const_iterator::m_pTable->ppSlots[base_const_iterator::m_index]->valueGuard.unlock_shared();
            ++base_const_iterator::m_index;
        }

        // End of table.
        base_const_iterator::m_index = ParallelHashTable::BAD_INDEX;
        base_const_iterator::m_pTable = nullptr;
    }
}

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::const_iterator::~const_iterator()
{
    if (base_const_iterator::m_pTable && base_const_iterator::m_index < base_const_iterator::m_pTable->capacity)
    {
        base_const_iterator::m_pTable->ppSlots[base_const_iterator::m_index]->valueGuard.unlock_shared();
    }
}

// ITERATOR

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::iterator::iterator(table_type* pTable, size_type index, grow_mutex_type* pGrowMutex, bool initProb, bool growLock)
    : base_const_iterator(pTable, index)
    , m_pGrowMutex(pGrowMutex)
{
    if(base_const_iterator::m_pTable && initProb)
    {
        if(growLock)
        {
            m_pGrowMutex->lock_shared();
        }

        while(base_const_iterator::m_index < base_const_iterator::m_pTable->capacity)
        {
            base_const_iterator::m_pTable->ppSlots[base_const_iterator::m_index]->valueGuard.lock();
            if (base_const_iterator::m_pTable->ppSlots[base_const_iterator::m_index]->state == slot_type::USED)
            {
                return;
            }
            base_const_iterator::m_pTable->ppSlots[base_const_iterator::m_index]->valueGuard.unlock();
            ++base_const_iterator::m_index;
        }

        // End of table.
        base_const_iterator::m_index = ParallelHashTable::BAD_INDEX;
        base_const_iterator::m_pTable = nullptr;
    }
}

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::iterator::iterator(iterator&& other)
    : base_const_iterator(std::move(other))
    , m_pGrowMutex(std::move(other.m_pGrowMutex))
{
    other.m_pGrowMutex = nullptr;
}

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::iterator::~iterator()
{
    if (base_const_iterator::m_pTable && base_const_iterator::m_index < base_const_iterator::m_pTable->capacity)
    {
        base_const_iterator::m_pTable->ppSlots[base_const_iterator::m_index]->valueGuard.unlock();
        base_const_iterator::m_index = ParallelHashTable::BAD_INDEX;
        base_const_iterator::m_pTable = nullptr;
    }

    if (m_pGrowMutex)
    {
        m_pGrowMutex->unlock_shared();
        m_pGrowMutex = nullptr;
    }
}

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
typename ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::iterator&
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::iterator::operator=(iterator&& other)
{
    if (this != &other)
    {
        base_const_iterator::m_pTable     = other.base_const_iterator::m_pTable;
        base_const_iterator::m_index      = other.base_const_iterator::m_index;
        m_pGrowMutex = other.m_pGrowMutex;

        other.base_const_iterator::m_pTable     = nullptr;
        other.base_const_iterator::m_index      = ParallelHashTable::BAD_INDEX;
        other.m_pGrowMutex = nullptr;
    }
    return *this;
}

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
typename ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::iterator&
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::iterator::operator++()
{
    _advance(base_const_iterator::m_pTable, base_const_iterator::m_index, m_pGrowMutex);
    return *this;
}

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
typename ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::value_type*
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::iterator::operator->()
{
    return &(base_const_iterator::m_pTable->ppSlots[base_const_iterator::m_index]->keyVal);
}

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
typename ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::value_type&
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::iterator::operator*()
{
    return base_const_iterator::m_pTable->ppSlots[base_const_iterator::m_index]->keyVal;
}

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
void ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::iterator::_advance(table_type*& pTable, size_type& index, grow_mutex_type*& pGrowMutex)
{
    if(pTable)
    {
        pTable->ppSlots[index]->valueGuard.unlock();

        while(index + 1 < pTable->capacity)
        {
            ++index;
            pTable->ppSlots[index]->valueGuard.lock();
            if (pTable->ppSlots[index]->state == slot_type::USED)
            {
                return;
            }
        }
    }

    // End of table.
    pTable->ppSlots[index]->valueGuard.unlock();
    pGrowMutex->unlock_shared();
    pGrowMutex = nullptr;
    index      = ParallelHashTable::BAD_INDEX;
    pTable     = nullptr;
}

// STRUCTORS

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::ParallelHashTable(allocator_type const& allocator)
    : allocator_type(allocator)
    , m_pTable(nullptr)
    , m_stripedGrowMutexes(nullptr)
    , m_growMutexCount(0)

{
    m_growMutexCount = nextPow2(Thread::getHardwareThreadCount());
    m_stripedGrowMutexes = allocator_type::template vector_new_object<PaddedGrowMutex>(m_growMutexCount);
}

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::ParallelHashTable(uint32_t growMutexCount, allocator_type const& allocator)
    : allocator_type(allocator)
    , m_pTable(nullptr)
    , m_stripedGrowMutexes(nullptr)
    , m_growMutexCount(growMutexCount)
{
    if (m_growMutexCount == 0)
    {
        GTS_ASSERT(m_growMutexCount != 0);
        m_growMutexCount = Thread::getHardwareThreadCount();
    }

    m_growMutexCount = nextPow2(m_growMutexCount);
    m_stripedGrowMutexes = allocator_type::template vector_new_object<PaddedGrowMutex>(m_growMutexCount);
}

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::~ParallelHashTable()
{
    allocator_type::vector_delete_object(m_stripedGrowMutexes, m_growMutexCount);
    cleanup();
    clear();
}

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::ParallelHashTable(ParallelHashTable const& other)
    : allocator_type(other)
    , m_pTable(nullptr)
    , m_stripedGrowMutexes(nullptr)
    , m_growMutexCount(other.m_growMutexCount)
{
    other._lockTable();
    _deepCopy(other);
    other._unlockTable();
}

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::ParallelHashTable(ParallelHashTable&& other)
    : allocator_type(std::move(other.get_allocator()))
    , m_pTable(nullptr)
    , m_stripedGrowMutexes(nullptr)
    , m_growMutexCount(0)
{
    other._lockTable();

    m_pTable.store(std::move(other.m_pTable.load(memory_order::relaxed)), memory_order::relaxed);
    m_stripedGrowMutexes = std::move(other.m_stripedGrowMutexes);
    m_dataBacking        = std::move(other.m_dataBacking);
    m_growMutexCount     = std::move(other.m_growMutexCount);
    m_hasher             = std::move(other.m_hasher);

    other.cleanup();
    other.m_pTable.store(nullptr, gts::memory_order::relaxed);
    other.m_stripedGrowMutexes = nullptr;
    other.m_growMutexCount = 0;

    _unlockTable();
}

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>&
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::operator=(ParallelHashTable const& other)
{
    if (this != &other)
    {
        _lockTable();

        cleanup();
        clear();

        other._lockTable();

        (allocator_type)*this = other.get_allocator();
        _deepCopy(other);

        other._unlockTable();

        _unlockTable();
    }
    return *this;
}

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>&
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::operator=(ParallelHashTable&& other)
{
    if (this != &other)
    {
        other._lockTable();

        PaddedGrowMutex* pOldMutexes = m_stripedGrowMutexes;
        size_type oldMutexCount = m_growMutexCount;

        cleanup();
        clear();

        m_pTable.store(std::move(other.m_pTable.load(memory_order::relaxed)), memory_order::relaxed);
        m_stripedGrowMutexes  = std::move(other.m_stripedGrowMutexes);
        m_dataBacking         = std::move(other.m_dataBacking);
        m_growMutexCount      = std::move(other.m_growMutexCount);
        m_hasher              = std::move(other.m_hasher);
        (allocator_type)*this = std::move(other.get_allocator());

        other.cleanup();
        other.m_pTable.store(nullptr, gts::memory_order::relaxed);
        other.m_stripedGrowMutexes = nullptr;
        other.m_growMutexCount = 0;

        _unlockTable();

        for (size_type ii = 0; ii < oldMutexCount; ++ii)
        {
            pOldMutexes[ii].m.unlock();
        }
        allocator_type::vector_delete_object(pOldMutexes, oldMutexCount);
    }
    return *this;
}

// ITERATORS

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
typename ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::const_iterator
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::cbegin() const
{
    return const_iterator(m_pTable.load(memory_order::acquire), 0, true);
}

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
typename ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::const_iterator
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::cend() const
{
    return const_iterator(nullptr, BAD_INDEX, false);
}

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
typename ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::iterator
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::begin() const
{
    size_type mutexIdx = m_hasher(ThisThread::getId()) & (m_growMutexCount - 1);
    grow_mutex_type& myGrowMutex = m_stripedGrowMutexes[mutexIdx].m;
    myGrowMutex.lock_shared();
    return iterator(m_pTable.load(memory_order::acquire), 0, &myGrowMutex, true);
}

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
typename ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::iterator
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::end() const
{
    return iterator(nullptr, BAD_INDEX, nullptr, false);
}

// CAPACITY

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
typename ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::size_type
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::capacity() const
{
    table_type* pTable = m_pTable.load(memory_order::acquire);
    if (pTable == nullptr)
    {
        return 0;
    }
    return pTable->capacity;
}

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
typename ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::size_type
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::max_size() const
{
    return MAX_CAPACITY;
}

// ACCESSORS

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
typename ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::const_iterator
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::cfind(key_type const& key) const
{
    table_type* pTable;
    size_type index;
    _probeRead(key, pTable, index);
    return const_iterator(pTable, index, false);
}

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
typename ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::iterator
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::find(key_type const& key)
{
    size_type mutexIdx = m_hasher(key) & (m_growMutexCount - 1);
    grow_mutex_type& myGrowMutex = m_stripedGrowMutexes[mutexIdx].m;
    myGrowMutex.lock_shared();

    table_type* pTable;
    size_type index;
    ProbeResult result = _probeWrite<true>(true, key, pTable, index);
    if(result & PROBE_TOMBSTONE)
    {
        // don't find tombstones
        pTable->ppSlots[index]->valueGuard.unlock();
        return iterator(nullptr, BAD_INDEX, nullptr, false);
    }
    return iterator(pTable, index, &myGrowMutex, false);
}

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
typename ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::allocator_type 
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::get_allocator() const
{
    return *this;
}

// MUTATORS

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
void ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::reserve(size_type sizePow2)
{
    size_type newSize = sizePow2 == 0 ? INIT_SIZE: sizePow2;
    _tryGrowTable<true>(m_pTable.load(memory_order::acquire), newSize);
}

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
typename ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::iterator
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::insert(KeyValue<TKey, TValue> const& pair)
{
    return _tryInsert<true>(false, pair.key, pair);
}

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
typename ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::iterator
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::insert(KeyValue<TKey, TValue>&& pair)
{
    return _tryInsert<true>(false, pair.key, std::move(pair));
}

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
typename ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::iterator
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::insert_or_assign(KeyValue<TKey, TValue> const& pair)
{
    return _tryInsert<true>(true, pair.key, pair);
}

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
typename ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::iterator
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::insert_or_assign(KeyValue<TKey, TValue>&& pair)
{
    return _tryInsert<true>(true, pair.key, std::move(pair));
}

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
typename ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::size_type
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::erase(TKey const& key)
{
    size_type mutexIdx       = m_hasher(key) & (m_growMutexCount - 1);
    grow_mutex_type& myMutex = m_stripedGrowMutexes[mutexIdx].m;
    LockShared<grow_mutex_type> lock(myMutex);

    ProbeResult result = _probeRemove(key);

    return result == PROBE_SUCCESS ? 1 : 0;
}

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
typename ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::iterator
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::erase(iterator& iter)
{
    table_type* pTable          = iter.m_pTable;
    size_type index             = iter.base_const_iterator::m_index;
    grow_mutex_type* pGrowMutex = iter.m_pGrowMutex;

    if(pTable)
    {
        slot_type* pSlot = pTable->ppSlots[iter.base_const_iterator::m_index];
        allocator_type::destroy(&pSlot->keyVal);
        pSlot->state = slot_type::DELETED;
    }
    iter.~iterator();
    return iterator(pTable, index, pGrowMutex, true);
}

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
void ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::cleanup()
{
    // TODO: shrink to fit?

    for (size_type ii = 0; ii < m_oldTables.size(); ++ii)
    {
        allocator_type::vector_delete_object(m_oldTables[ii]->ppSlots, m_oldTables[ii]->capacity);
        allocator_type::delete_object(m_oldTables[ii]);
    }
    m_oldTables.clear();
}

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
void ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::clear()
{
    // Free each slab and destroy all keyVals.
    for (size_type ii = 0; ii < m_dataBacking.size(); ++ii)
    {
        size_type prevSize = ii ? (size_type(1) << ii) : 0;
        size_type currSize = (size_type(1) << (ii + 1)) - prevSize;

        for (size_type iSlot = 0; iSlot < currSize; ++iSlot)
        {
            if(m_dataBacking[ii][iSlot].USED)
            {
                allocator_type::destroy(&(m_dataBacking[ii][iSlot].keyVal));
            }
        }
        allocator_type::deallocate(m_dataBacking[ii], currSize);
    }
    m_dataBacking.clear();

    // Destroy the current table.
    table_type* pTable = m_pTable.load(memory_order::relaxed);
    if(pTable)
    {
        allocator_type::vector_delete_object(pTable->ppSlots, pTable->capacity);
        allocator_type::delete_object(pTable);
    }

    m_pTable.store(nullptr, memory_order::release);
}

// PRIVATE

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
void ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::_deepCopy(ParallelHashTable const& src)
{
    table_type* pSrcTable = src.m_pTable.load(memory_order::relaxed);
    for (size_type ii = 0; ii < pSrcTable->capacity; ++ii)
    {
        _tryInsert<false>(false, pSrcTable->ppSlots[ii]->keyVal.key, pSrcTable->ppSlots[ii]->keyVal);
    }
}

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
template<bool useLocks>
bool ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::_tryGrowTable(table_type* pTable, size_type sizePow2)
{
    bool result = false;

    if(useLocks)
    {
        _lockTable();
    }


    if (pTable != m_pTable.load(memory_order::acquire))
    {
        result = true;
        goto done;
    }

    result = _grow(sizePow2);

done:

    if(useLocks)
    {
        _unlockTable();
    }

    return result;
}

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
bool ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::_grow(size_type sizePow2)
{
    table_type* pOldTable  = m_pTable.load(memory_order::relaxed);
    table_type* pNewTable;

    while(true)
    {
        if (pOldTable == nullptr)
        {
            pNewTable              = allocator_type::template new_object<table_type>();
            size_type newCapacity  = INIT_SIZE;
            pNewTable->ppSlots     = allocator_type::template vector_new_object<slot_type*>(newCapacity);
            pNewTable->capacity    = newCapacity;
            pNewTable->maxProbe    = uint16_t(ceil(::log2(pNewTable->capacity) * ::log2(pNewTable->capacity)));
        }
        else
        {
            if (sizePow2 > MAX_CAPACITY)
            {
                // can't grow.
                return false;
            }

            pNewTable              = allocator_type::template new_object<table_type>();
            size_type newCapacity  = sizePow2;
            pNewTable->ppSlots     = allocator_type::template vector_new_object<slot_type*>(newCapacity);
            pNewTable->capacity    = newCapacity;
            pNewTable->maxProbe    = uint16_t(ceil(::log2(pNewTable->capacity) * ::log2(pNewTable->capacity)));

            // Rehash only the used elements from the old table elements into the new table.
            for (size_type ii = 0; ii < pOldTable->capacity; ++ii)
            {
                slot_type* pSlot = pOldTable->ppSlots[ii];
                GTS_ASSERT(pSlot);

                if (pSlot->state == slot_type::USED)
                {
                    // *No threads can modify the tables, so there is no need to lock.

                    typename hasher_type::hashed_value hash = m_hasher(pSlot->keyVal.key);
            
                    // Probe.
                    for (size_type jj = 0; jj < pNewTable->capacity; ++jj)
                    {
                        if (jj > pNewTable->maxProbe)
                        {
                            // Fail! Cleanup and try again with bigger capacity.
                            allocator_type::vector_delete_object(pNewTable->ppSlots, newCapacity);
                            allocator_type::delete_object(pNewTable);
                            sizePow2 *= 2;
                            continue;
                        }

                        size_type idx = (hash + jj) & (pNewTable->capacity - 1);
                        if(pNewTable->ppSlots[idx] == nullptr)
                        {
                            pNewTable->ppSlots[idx] = pSlot;
                            break;
                        }
                    }
                }
            }

            // Fill in the holes with the EMPTY and DELTED slots.
            size_type holeIdx = 0;
            for (size_type ii = 0; ii < pOldTable->capacity; ++ii)
            {
                slot_type* pSlot = pOldTable->ppSlots[ii];
                GTS_ASSERT(pSlot);

                if (pSlot->state != slot_type::USED)
                {
                    slot_type** ppEmptySlot = nullptr;

                    // Find the next hole.
                    while (true)
                    {
                        GTS_ASSERT(holeIdx < pNewTable->capacity);

                        // *No threads can modify the tables, so there is no need to lock.
                        ppEmptySlot = &pNewTable->ppSlots[holeIdx++];
                        if(*ppEmptySlot == nullptr)
                        {
                            *ppEmptySlot = pSlot;
                            break;
                        }
                    }
                }
            }

            // Save since there may still be readers.
            m_oldTables.push_back(pOldTable);
        } 

        break;
    }

    // Add a new slab to the data backing.
    size_type totalCurrentBackingSize = pOldTable ? (size_type(1) << m_dataBacking.size()) : 0;
    size_type newSlabSize             = (size_type(1) << (m_dataBacking.size() + 1)) - totalCurrentBackingSize;
    slot_type* pNewSlab               = allocator_type::template allocate<slot_type>(newSlabSize);
    m_dataBacking.push_back(pNewSlab);

    // Initialize the slot except of the keyVal.
    for (size_type ii = 0; ii < newSlabSize; ++ii)
    {
        new (&(pNewSlab[ii].valueGuard)) accessor_mutex_type();
        pNewSlab[ii].state = slot_type::EMPTY;
    }

    // Insert new slab elements to the new table's holes.
    size_type slabIdx = 0;
    for (size_type ii = 0; ii < pNewTable->capacity; ++ii)
    {
        if(pNewTable->ppSlots[ii] == nullptr)
        {
            pNewTable->ppSlots[ii] = &pNewSlab[slabIdx++];
        }
    }
    GTS_ASSERT(slabIdx == newSlabSize && "Missed data backing to slot assignment.");

    m_pTable.exchange(pNewTable, memory_order::release);

    return true;
}

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
void ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::_probeRead(TKey const& key, table_type*& pOutTable, size_type& outIndex) const
{
    pOutTable = nullptr;
    outIndex  = BAD_INDEX;

    typename hasher_type::hashed_value hash = m_hasher(key);
    table_type* pTable = m_pTable.load(memory_order::acquire);

    if (pTable == nullptr)
    {
        return;
    }

    for (size_type ii = 0, len = pTable->maxProbe; ii < len; ++ii)
    {
        size_type idx = (hash + ii) & (pTable->capacity - 1);
        slot_type* pSlot = pTable->ppSlots[idx];
        GTS_ASSERT(pSlot);

        if (pSlot->state == slot_type::EMPTY)
        {
            // Fail. Slot is empty.
            break;
        }

        if (pSlot->keyVal.key == key)
        {
            if (pSlot->state == slot_type::DELETED)
            {
                break;
            }
            else
            {
                pSlot->valueGuard.lock_shared(); // iterator will unlock on destroy.
                // verify again now that we have ownership
                if(pSlot->keyVal.key == key && pSlot->state != slot_type::DELETED)
                {
                    // Key found!
                    pOutTable = pTable;
                    outIndex= idx;
                    break;
                }
                pSlot->valueGuard.unlock_shared();
            }
        }
        // Keep looking for key...
    }
}

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
template<bool useLocks, typename... TArgs>
typename ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::iterator
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::_tryInsert(bool assignable, TKey const& key, TArgs&&... args)
{
    size_type mutexIdx = m_hasher(key) & (m_growMutexCount - 1);
    grow_mutex_type& myGrowMutex = m_stripedGrowMutexes[mutexIdx].m;

    table_type* pOutTable = nullptr;
    size_type index = BAD_INDEX;

    ProbeResult result = PROBE_FAIL;
    while(true)
    {
        if(useLocks)
        {
            myGrowMutex.lock_shared();
        }

        result = _probeWrite<useLocks>(assignable, key, pOutTable, index);
        if (result & PROBE_SUCCESS)
        {
            if(result & PROBE_EXISTS)
            {
                // destroy the old value.
                allocator_type::destroy(&(pOutTable->ppSlots[index]->keyVal));
            }
            allocator_type::construct(&(pOutTable->ppSlots[index]->keyVal), std::forward<TArgs>(args)...);
            pOutTable->ppSlots[index]->state = slot_type::USED;
            break;
        }

        if(useLocks)
        {
            myGrowMutex.unlock_shared();
        }

        if(result == PROBE_FAIL)
        {
            break;
        }

        table_type* pTable = m_pTable.load(memory_order::acquire);
        size_type newSize = pTable ? pTable->capacity * 2 : INIT_SIZE;
        if (result == PROBE_GROW && !_tryGrowTable<useLocks>(pTable, newSize))
        {
            break;
        }
    }

    return iterator(pOutTable, index, useLocks ? &myGrowMutex : nullptr, false);
}

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
template<bool useLocks, typename... TArgs>
typename ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::ProbeResult
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::_probeWrite(bool assignable, TKey const& key, table_type*& pOutTable, size_type& outIndex)
{
    pOutTable = nullptr;
    outIndex = BAD_INDEX;

    typename hasher_type::hashed_value hash = m_hasher(key);
    table_type* pTable = m_pTable.load(memory_order::acquire);

    if (pTable == nullptr)
    {
        return PROBE_GROW;
    }

    while(true)
    {
        size_type tombstoneIdx = SIZE_MAX;
        for (size_type ii = 0, len = pTable->maxProbe; ii < len; ++ii)
        {
            size_type idx = (hash + ii) & (pTable->capacity - 1);
            slot_type* pSlot = pTable->ppSlots[idx];
            GTS_ASSERT(pSlot);

            if (pSlot->state == slot_type::EMPTY)
            {
                if (tombstoneIdx != SIZE_MAX)
                {
                    // We had a tombstone, so try to use that instead.
                    break;
                }
                else
                {
                    if(useLocks)
                    {
                        pSlot->valueGuard.lock();
                    }

                    // verify again now that we have ownership
                    if (pSlot->state == slot_type::EMPTY) 
                    {
                        // Slot it free. Add our pair.
                        pOutTable = pTable;
                        outIndex  = idx;
                        return PROBE_SUCCESS;
                    }

                    if(useLocks)
                    {
                        pSlot->valueGuard.unlock();
                    }
                }
            }
            else if (pSlot->state == slot_type::DELETED)
            {
                if (tombstoneIdx == SIZE_MAX)
                {
                    tombstoneIdx = idx;
                }
            }
            else if (pSlot->keyVal.key == key)
            {
                if(assignable)
                {
                    if(useLocks)
                    {
                        pSlot->valueGuard.lock();
                    }

                    // verify again now that we have ownership
                    if (pSlot->keyVal.key == key)
                    {
                        pOutTable = pTable;
                        outIndex  = idx;
                        return PROBE_SUCCESS | PROBE_EXISTS;
                    }

                    if(useLocks)
                    {
                        pSlot->valueGuard.unlock();
                    }
                }
                else
                {
                    return PROBE_FAIL;;
                }
            }

            GTS_SPECULATION_FENCE();
        }

        if (tombstoneIdx != SIZE_MAX)
        {
            slot_type* pSlot = pTable->ppSlots[tombstoneIdx];
            GTS_ASSERT(pSlot);

            if(useLocks)
            {
                pSlot->valueGuard.lock();
            }

            if (pSlot->state == slot_type::DELETED)
            {
                // Slot it free. Add our pair.
                pOutTable = pTable;
                outIndex  = tombstoneIdx;
                return PROBE_SUCCESS | PROBE_TOMBSTONE;
            }
            else
            {
                // Another thread took our tombstone. Retry.
                if(useLocks)
                {
                    pSlot->valueGuard.unlock();
                }
                continue;
            }
        }

        break;
    }

    return PROBE_GROW;
}

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
typename ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::ProbeResult
ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::_probeRemove(TKey const& key)
{
    typename hasher_type::hashed_value hash = m_hasher(key);
    table_type* pTable = m_pTable.load(memory_order::acquire);
    if (pTable == nullptr)
    {
        return PROBE_FAIL;
    }

    for (size_type ii = 0, len = pTable->maxProbe; ii < len; ++ii)
    {
        size_type idx = (hash + ii) & (pTable->capacity - 1);
        slot_type* pSlot = pTable->ppSlots[idx];
        GTS_ASSERT(pSlot);

        if (pSlot->state == slot_type::USED && pSlot->keyVal.key == key)
        {
            Lock<accessor_mutex_type> writeLock(pSlot->valueGuard);
            // verify again now that we have ownership
            if (pSlot->state == slot_type::USED && pSlot->keyVal.key == key)
            {
                // Mark as deleted
                pSlot->keyVal.~value_type();
                pSlot->state = slot_type::DELETED;
                return PROBE_SUCCESS;
            }
        }
    }

    return PROBE_FAIL;
}

//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
void ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::_lockTable() const
{
    for (size_type ii = 0; ii < m_growMutexCount; ++ii)
    {
        m_stripedGrowMutexes[ii].m.lock();
    }
}


//------------------------------------------------------------------------------
template<typename TKey, typename TValue, typename THasher, typename TAccessorSharedMutex, typename TGrowSharedMutex, typename TAllocator>
void ParallelHashTable<TKey, TValue, THasher, TAccessorSharedMutex, TGrowSharedMutex, TAllocator>::_unlockTable() const
{
    for (size_type ii = 0; ii < m_growMutexCount; ++ii)
    {
        m_stripedGrowMutexes[ii].m.unlock();
    }
}
