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

#include <cmath>

#include "gts/platform/Utils.h"
#include "gts/platform/Thread.h"
#include "gts/synchronization/SpinMutex.h"
#include "gts/synchronization/Lock.h"
#include "gts/containers/AlignedAllocator.h"
#include "gts/containers/Vector.h"

#ifdef GTS_MSVC
#pragma warning( push )
#pragma warning( disable : 4324) // alignment padding warning
#endif

namespace gts {

/** 
 * @addtogroup Containers
 * @{
 */

/** 
 * @addtogroup ParallelContainers
 * @{
 */

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A key-value keyVal.
 */
template<typename TKey, typename TValue>
struct KeyValue
{
    TKey key;
    TValue value;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A parallel hash table. Properties:
 *  - Open addressing with O(log2*log2(N)) probing.
 *  - Grows when insert probing reaches O(log2*log2(N)).
 *  - Unbound.
 *  - Reference Stable.
 *  - Linearizable if Mutexes are fair.
 * @remark
 *  - User is responsible for calling cleanup() to delete stale tables after table doubling.
 *  - Iterators contain spin-locks that are unlocked in the iterator's destructor.
 *    Holding onto them may cause deadlocks.
 *  - Bad hash functions may cause huge tables. Author has only had this occur
 *    with pathologically bad hashers.
 * @tparam TKey
 *  The unique identifier associated with each element in the container.
 * @tparam TValue
 *  The element type stored in the container.
 * @tparam THasher
 *  The hashing function.
 * @tparam TAccessorSharedMutex
 *  The shared mutex type to access individual elements.
 * @tparam TGrowSharedMutex
 *  The shared mutex type used to grow the table.
 * @tparam TAllocator
 *  The allocator used by the storage backing.
 *
 * @todo Try Robin Hood hashing. (Lock cost may be prohibitive.)
 * @todo Try Striped locks to protect slots instead of locks per slot.
 * @todo Try 1-byte per table entry trick and use SSE for compares. (see Skarupke and Kulukundis)
 * @todo Implement table shrinking.
 * @todo Divise a reclaimation system (hazard pointers etc.) the avoid the need for cleanup().
 * @todo Add equals template parameter.
 * @todo Add size() and empty()? requires shared counter :(
 */
template<
    typename TKey,
    typename TValue,
    typename THasher              = MurmurHash<TKey>,
    typename TAccessorSharedMutex = UnfairSharedSpinMutex<>,
    typename TGrowSharedMutex     = UnfairSharedSpinMutex<>,
    typename TAllocator           = AlignedAllocator<GTS_NO_SHARING_CACHE_LINE_SIZE>
    >
class ParallelHashTable : private TAllocator
{
public:

    using key_type       = TKey;
    using mapped_type    = TValue;
    using value_type     = KeyValue<key_type, mapped_type>;
    using size_type      = size_t;
    using allocator_type = TAllocator;
    using hasher_type    = THasher;

private:

    struct slot_type
    {
        using accessor_mutex_type = TAccessorSharedMutex;

        static constexpr uint8_t EMPTY   = 0;
        static constexpr uint8_t USED    = 1;
        static constexpr uint8_t DELETED = 2;

        value_type keyVal;
        accessor_mutex_type valueGuard;
        uint8_t state = EMPTY;
    };

    using accessor_mutex_type = typename slot_type::accessor_mutex_type;
    using grow_mutex_type     = TGrowSharedMutex;

    struct table_type
    {
        slot_type** ppSlots = nullptr;
        size_type capacity  = 0;
        uint16_t maxProbe   = 0;
    };

    friend class base_const_iterator;
    friend class const_iterator;
    friend class iterator;

    class base_const_iterator
    {
    public:
        base_const_iterator() = default;
        explicit base_const_iterator(table_type* pTable, size_type index);
        base_const_iterator(base_const_iterator&& other);

        base_const_iterator& operator=(base_const_iterator&& other);
        base_const_iterator& operator++();
        value_type const* operator->() const;
        value_type const& operator*() const;
        bool operator==(base_const_iterator const& other) const;
        bool operator!=(base_const_iterator const& other) const;

    protected:
        friend class ParallelHashTable;
        base_const_iterator(base_const_iterator const&) = delete;
        base_const_iterator& operator=(base_const_iterator const&) = delete;
        static void _advance(table_type*& pTable, size_type& index);

        mutable table_type* m_pTable = nullptr;
        mutable size_type m_index = ParallelHashTable::BAD_INDEX;
    };

public:

    /**
     * @brief
     *  A constant forward iterator. Read-locks the referenced element until destruction.
     */
    class const_iterator : public base_const_iterator
    {
    public:
        const_iterator() = default;
        const_iterator(const_iterator&& other) = default;
        const_iterator& operator=(const_iterator&& other);

        explicit const_iterator(table_type* pTable, size_type index, bool initProb);
        ~const_iterator();

    protected:
        friend class ParallelHashTable;
    };

    /**
     * @brief
     *  A forward iterator. Write-locks the referenced element until destruction.
     */
    class iterator : public base_const_iterator
    {
    public:

        explicit iterator(table_type* pTable, size_type index, grow_mutex_type* pGrowMutex, bool initProb, bool growLock = true);
        iterator(iterator&& other);
        ~iterator();
        iterator& operator=(iterator&& other);
        iterator& operator++();
        value_type* operator->();
        value_type& operator*();

    private:
        friend class ParallelHashTable;
        iterator(iterator const&) = delete;
        iterator& operator=(iterator const&) = delete;
        static void _advance(table_type*& pTable, size_type& index, grow_mutex_type*& pGrowMutex);

        mutable grow_mutex_type* m_pGrowMutex = nullptr;
    };

public: // STRUCTORS

    /**
     * Constructs an empty container with the given 'allocator'. The number
     * of striped mutexes is equal to the number of HW threads on the machine.
     */
    explicit ParallelHashTable(allocator_type const& allocator = allocator_type());

    /**
     * Constructs an empty container with 'growMutexCount' striped mutexes and
     * with the given 'allocator'.
     */
    explicit ParallelHashTable(uint32_t growMutexCount, allocator_type const& allocator = allocator_type());

    /**
     * Destructs the container. The destructors of the elements are called and the
     * used storage is deallocated.
     */
    ~ParallelHashTable();

    /**
     * Copy constructor. Constructs the container with the copy of the contents
     * of 'other'.
     * @remark
     *  Thread safe.
     */
    ParallelHashTable(ParallelHashTable const& other);
    
    /**
     * Move constructor. Constructs the container with the contents of other
     * using move semantics. After the move, other is invalid.
     * @remark
     *  Thread safe.
     */
    ParallelHashTable(ParallelHashTable&& other);

    /**
     * Copy assignment operator. Replaces the contents with a copy of the
     * contents of 'other'.
     * @remark
     *  Thread safe.
     */
    ParallelHashTable& operator=(ParallelHashTable const& other);

    /**
     * Move assignment operator. Replaces the contents with those of other using
     * move semantics. After the move, other is invalid.
     * @remark
     *  Thread safe.
     */
    ParallelHashTable& operator=(ParallelHashTable&& other);

public: // ITERATORS

    /**
     * @returns
     *  An iterator to the beginning of the current hash table state.
     * @remark
     *  Thread safe.
     */
    const_iterator cbegin() const;

    /**
     * @returns
     *  An iterator to the end of the current hash table state.
     * @remark
     *  Thread safe.
     */
    const_iterator cend() const;

    /**
     * @returns
     *  An iterator to the beginning of the current hash table state.
     * @remark
     *  Thread safe.
     */
    iterator begin() const;

    /**
     * @returns
     *  An iterator to the end of the current hash table state.
     * @remark
     *  Thread safe.
     */
    iterator end() const;

public: // CAPACITY

    /**
     * @returns
     *  The total number of slots in the table.
     * @remark
     *  Thread safe.
     */
    size_type capacity() const;

    /**
     * @returns
     *  Maximum number of slots the container can grow to.
     * @remark
     *  Thread safe.
     */
    size_type max_size() const;

public: // ACCESSORS

    /**
     * Finds an element with key equivalent to 'key'.
     * @returns
     *  A const_terator to an element with key equivalent to key. If no such
     *   element is found, past-the-end (see end()) iterator is returned.
     * @remark
     *  Thread safe.
     */
    const_iterator cfind(key_type const& key) const;

    /**
     * Finds an element with key equivalent to 'key'.
     * @returns
     *  An iterator to an element with key equivalent to key. If no such element
     *   is found, past-the-end (see end()) iterator is returned.
     * @remark
     *  Thread safe.
     */
    iterator find(key_type const& key);

    /**
    * @brief
    *  Get this Vectors allocator.
    * @returns
    *  The allocator.
    */
    allocator_type get_allocator() const;

public: // MUTATORS

    /**
     * Increases the capacity of the hash table. Does nothing if 'sizePow2' < capacity.
     * @remark Thread-safe.
     */
    void reserve(size_type sizePow2);

    /**
     * Inserts an element into the container, if the container doesn't already
     * contain an element with an equivalent key.
     * @returns
     *  An iterator to the element if it's inserted successfully, or
     *  past-the-end (see end()) iterator if:
     *  (1) The key already exists.
     *  (2) The table failed to grow.
     * @remark
     *  Thread safe.
     */
    iterator insert(value_type const& keyVal);

    /**
     * Inserts an element into the container, if the container doesn't already
     * contain an element with an equivalent key.
     * @returns
     *  An iterator to the element if it's inserted successfully, or
     *  past-the-end (see end()) iterator if:
     *  (1) The key already exists.
     *  (2) The table failed to grow.
     * @remark
     *  Thread safe.
     */
    iterator insert(value_type&& keyVal);

    /**
     * Inserts an element into the container or assign the new value if it
     * already exists.
     * @returns
     *  An iterator to the element if it's inserted successfully, or
     *  past-the-end (see end()) iterator if the table failed to grow.
     * @remark
     *  Thread safe.
     */
    iterator insert_or_assign(value_type const& keyVal);

    /**
     * Inserts an element into the container or assign the new value if it
     * already exists.
     * @returns
     *  An iterator to the element if it's inserted successfully, or
     *  past-the-end (see end()) iterator if the table failed to grow.
     * @remark
     *  Thread safe.
     */
    iterator insert_or_assign(value_type&& keyVal);

    /**
     * Tries to erase an element from the table.
     * @returns
     *  The number if element erased.
     * @remark
     *  Thread safe.
     */
    size_type erase(key_type const& key);

    /**
     * Erases the element referenced by 'iter' and invalidates 'iter'.
     @returns
     *  The next iterator.
     * @remark
     *  Thread safe.
     */
    iterator erase(iterator& iter);

    /**
     * Destroys old table memory.
     * @remark
     *  Not thread-safe.
     */
    void cleanup();

    /**
     * Clear the data from the table.
     * @remark
     *  Not thread-safe.
     */
    void clear();

private: // PRIVATES:

    using ProbeResult = uint32_t;
    static constexpr ProbeResult PROBE_FAIL      = 0x0;
    static constexpr ProbeResult PROBE_SUCCESS   = 0x1;
    static constexpr ProbeResult PROBE_EXISTS    = 0x2;
    static constexpr ProbeResult PROBE_TOMBSTONE = 0x4;
    static constexpr ProbeResult PROBE_GROW      = 0x8;

    GTS_NO_INLINE void _init(uint32_t growMutexCount);

    template<bool useLocks>
    GTS_NO_INLINE bool _tryGrowTable(table_type* pTable, size_type sizePow2);

    GTS_NO_INLINE bool _grow(size_type sizePow2);

    void _deepCopy(ParallelHashTable const& src);

    void _probeRead(key_type const& key, table_type*& pTable, size_type& index) const;

    template<bool useLocks, typename... TArgs>
    iterator _tryInsert(bool assignable, TKey const& key, TArgs&&... args);

    template<bool useLocks, typename... TArgs>
    ProbeResult _probeWrite(bool assignable, TKey const& key, table_type*& pTable, size_type& index);

    ProbeResult _probeRemove(key_type const& key);

    void _lockTable() const;

    void _unlockTable() const;

private:

    static constexpr size_type MAX_CAPACITY = numericLimits<size_type>::max() / 2;
    static constexpr size_type BAD_INDEX    = MAX_CAPACITY;
    static constexpr size_type INIT_SIZE    = 2;

    struct GTS_ALIGN(GTS_NO_SHARING_CACHE_LINE_SIZE) PaddedGrowMutex
    {
        grow_mutex_type m;
    };

    //! The current hash table.
    Atomic<table_type*> m_pTable;

    //! Striped mutexes that control table access (read-locked) vs
    //! table growth (write-locked).
    mutable PaddedGrowMutex* m_stripedGrowMutexes;

    //! Data storage decoupled from the table enabling reference stability.
    //! Grows in slabs of size {2, 2, 4, 8, 16,...}
    Vector<slot_type*> m_dataBacking;

    //! Previously sized hash tables, leftover from grow. We keep these around
    //! so they can still be accessed safely by iterators.
    Vector<table_type*> m_oldTables;

    //! The number of m_stripedGrowMutexes.
    uint32_t m_growMutexCount = 0;

    //! The supplied hash function.
    hasher_type m_hasher;
};

/** @} */ // end of ParallelContainers
/** @} */ // end of Containers

#include "ParallelHashTable.inl"

#ifdef GTS_MSVC
#pragma warning( pop )
#endif

} // namespace gts
