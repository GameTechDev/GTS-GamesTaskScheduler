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

#include <vector>

#include "gts/platform/Utils.h"
#include "gts/platform/Thread.h"
#include "gts/synchronization/SpinMutex.h"
#include "gts/synchronization/Lock.h"
#include "gts/containers/AlignedAllocator.h"
#include "gts/containers/parallel/Guards.h"
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


namespace internal {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  An sub-vector used by ParallelVector.
 * @tparam T
 *  The element type stored in the container.
 * @tparam TSharedMutex
 *  The shared mutex type used to grow the table.
 * @tparam TAllocator
 *  The allocator used by the storage backing.
 */
template<
    typename T,
    typename TSharedMutex = UnfairSharedSpinMutex<>,
    typename TAllocator   = AlignedAllocator<GTS_NO_SHARING_CACHE_LINE_SIZE>
>
class ParallelSubVector : TAllocator
{
public:

    using value_type       = T;
    using size_type        = size_t;
    using mutex_type       = TSharedMutex;
    using backoff_type     = typename mutex_type::backoff_type;
    using allocator_type   = TAllocator;

    struct slot_type
    {
        value_type value;
        mutex_type valueGuard;
        Atomic<bool> isEmpty;
    };

    struct index_buffer_type
    {
        slot_type** ppBegin    = nullptr;
        size_type capacity     = 0;
    };

public:

    explicit ParallelSubVector(size_type numVecs, allocator_type const& allocator = allocator_type());

    // Not thread-safe.
    ParallelSubVector(ParallelSubVector const& other);

    // Not thread-safe.
    ParallelSubVector& operator=(ParallelSubVector const& other);

    // Not thread-safe.
    ParallelSubVector(ParallelSubVector&& other);

    // Not thread-safe.
    ParallelSubVector& operator=(ParallelSubVector&& other);

    // Not thread-safe.
    ~ParallelSubVector();

    template<typename... TArgs>
    void emplaceBack(size_type globalIdx, TArgs&&... args);

    template<bool useLock>
    value_type&& pop_back_and_get(size_type globalIdx);

    slot_type* readAt(size_type globalIdx) const;

    slot_type* writeAt(size_type globalIdx);

    // Not thread-safe.
    void reserve(size_type newCapacity);

    // Not thread-safe.
    void clear();

    // Not thread-safe.
    void shrinkToFit();

    size_type capacity() const;

    allocator_type get_allocator() const;

private:

    size_type _toLocal(size_type globalIdx) const;
    size_type _toSlabIndex(size_type localIdx) const;
    void _toSlabLocation(size_type localIdx, size_type& slabIdx, size_type& slotIdx) const;
    GTS_NO_INLINE void _grow(size_type capacity);
    void _destroy();
    void _deepCopy(ParallelSubVector const& src);

    static constexpr size_type INIT_SIZE = 2;

    // The number of elements combined with an ABA tag. Stacks have a delightful
    // ABA issue around push and pop.
    GTS_ALIGN(GTS_CACHE_LINE_SIZE) Atomic<size_type> m_size;

    // A mask for an incoming index that removes the global index space.
    const size_type m_ticketMask;

    // A shift for an incoming index that moves it into local index space.
    const size_type m_ticketShift;

    // The number of element slots.
    size_type m_capacity;

    // The element slots sorted in slabs. Slab size grows as {2, 2, 4, 8, 16,...}
    slot_type** m_ppSlabs;

    // The number of slabs.
    size_type m_slabCount;
};

} // namespace internal


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A obstruction-free parallel vector. Properties:
 *  - Unbound.
 *  - Reference stable.
 *  - Not linearizable.
 * @remark
 *  Iterators and guards contain spin-locks that are unlocked in the their
 *  destructors. Holding onto them may cause deadlocks.
 * @tparam T
 *  The element type stored in the container.
 * @tparam TAccessorSharedMutex
 *  The shared mutex type to access individual elements.
 * @tparam TGrowSharedMutex
 *  The shared mutex type used to grow the table.
 * @tparam TAllocator
 *  The allocator used by the storage backing.
 *
 * @todo Implement insert()
 * @todo Implement delete()
 * @todo Make more methods thread-safe.
 */
template<
    typename T,
    typename TSharedMutex = UnfairSharedSpinMutex<>,
    typename TAllocator   = AlignedAllocator<GTS_NO_SHARING_CACHE_LINE_SIZE>
    >
class ParallelVector : private TAllocator
{
public:

    using ticket_vec      = internal::ParallelSubVector<T, TSharedMutex, TAllocator>;
    using value_type      = typename ticket_vec::value_type;
    using mutex_type      = typename ticket_vec::mutex_type;
    using backoff_type    = typename ticket_vec::backoff_type;
    using size_type       = typename ticket_vec::size_type;
    using allocator_type  = typename ticket_vec::allocator_type;
    using read_guard      = ReadGuard<value_type, mutex_type, mutex_type>;
    using write_guard     = WriteGuard<value_type, mutex_type, mutex_type>;

    //! The result structure for pop_back_and_get.
    struct pop_back_result
    {
        //! The potentially popped value.
        value_type&& val;
        //! True if 'val' contains the popped value. False if 'val' is garbage.
        bool isValid;
    };

private:

    using slot_type = typename ticket_vec::slot_type;

    friend class base_const_iterator;
    friend class const_iterator;
    friend class iterator;

    class base_const_iterator
    {
    public:
        base_const_iterator() = default;
        base_const_iterator(ParallelVector const* pVec, slot_type* pSlot, size_type index);
        base_const_iterator(base_const_iterator&& other);

        base_const_iterator& operator=(base_const_iterator&& other);
        base_const_iterator& operator++();
        value_type const* operator->() const;
        value_type const& operator*() const;
        bool operator==(base_const_iterator const& other) const;
        bool operator!=(base_const_iterator const& other) const;

    protected:
        friend class ParallelVector;
        base_const_iterator(base_const_iterator const&) = delete;
        base_const_iterator& operator=(base_const_iterator const&) = delete;
        void _invalidate();
        static void _advance(ParallelVector const* pVec, slot_type*& pSlot, size_type& index);

        ParallelVector const* m_pVec = nullptr;
        slot_type* m_pSlot           = nullptr;
        size_type m_index            = ParallelVector::BAD_INDEX;
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
        const_iterator& operator=(const_iterator&& other) = default;

        const_iterator(ParallelVector const* pVec, slot_type* pSlot, size_type index);
        ~const_iterator();

    protected:
        friend class ParallelVector;
    };

    /**
     * @brief
     *  A forward iterator. Write-locks the referenced element until destruction.
     */
    class iterator : public base_const_iterator
    {
    public:

        iterator() = default;
        iterator(iterator&& other) = default;
        iterator& operator=(iterator&& other) = default;

        iterator(ParallelVector* pVec, slot_type* pSlot, size_type index);
        ~iterator();

        iterator& operator++();
        value_type* operator->();
        value_type& operator*();

    private:
        friend class ParallelVector;
        iterator(iterator const&) = delete;
        iterator& operator=(iterator const&) = delete;
        static void _advance(ParallelVector* pVec, slot_type*& pSlot, size_type& index);
    };

public: // STRUCTORS

    /**
     * Constructs an empty container with the given 'allocator'. The number
     * of sub vectors is equal to the number of HW threads on the machine.
     * @remark
     *  Thread-safe.
     */
    explicit ParallelVector(const allocator_type& allocator = allocator_type());

    /**
     * Constructs an empty container with 'subVectorCount' sub vectors and
     * with the given 'allocator'.
     * @remark
     *  Thread-safe.
     */
    explicit ParallelVector(size_type subVectorCountPow2, const allocator_type& allocator = allocator_type());

    /**
     * Destructs the container. The destructors of the elements are called and the
     * used storage is deallocated.
     * @remark
     *  Not thread-safe.
     */
    ~ParallelVector();

    /**
     * Copy constructor. Constructs the container with the copy of the contents
     * of 'other'.
     * @remark
     *  Not thread-safe.
     */
    ParallelVector(ParallelVector const& other);
    
    /**
     * Move constructor. Constructs the container with the contents of other
     * using move semantics. After the move, other is invalid.
     * @remark
     *  Not thread-safe.
     */
    ParallelVector(ParallelVector&& other);

    /**
     * Copy assignment operator. Replaces the contents with a copy of the
     * contents of 'other'.
     * @remark
     *  Not thread-safe.
     */
    ParallelVector& operator=(ParallelVector const& other);

    /**
     * Move assignment operator. Replaces the contents with those of other using
     * move semantics. After the move, other is invalid.
     * @remark
     *  Not thread-safe.
     */
    ParallelVector& operator=(ParallelVector&& other);

public: // ITERATORS

    /**
     * @returns
     *  An iterator to the beginning of the current vector state.
     * @remark
     *  Thread-safe.
     */
    const_iterator cbegin() const;

    /**
     * @returns
     *  An iterator to the end of the current vector state.
     * @remark
     *  Thread-safe.
     */
    const_iterator cend() const;

    /**
     * @returns
     *  An iterator to the beginning of the current vector state.
     * @remark
     *  Thread-safe.
     */
    iterator begin();

    /**
     * @returns
     *  An iterator to the end of the current vector state.
     * @remark
     *  Thread-safe.
     */
    iterator end();

public: // CAPACITY

    /**
     * @returns
     *  The total number of slots in the table.
     * @remark
     *  Not thread-safe. May return old value during a resize.
     */
    size_type capacity() const;

    /**
     * @returns
     *  Maximum number of slots the container can grow to.
     * @remark
     *  Thread-safe.
     */
    size_type max_size() const;

    /**
     * @returns
     *  The number of elements in the vector.
     * @remark
     *  Thread-safe.
     */
    size_type size() const;

    /**
     * @returns
     *  True if there are no element in the vector, false otherwise.
     * @remark
     *  Thread-safe.
     */
    bool empty() const;

    /**
     * @returns
     *  This vector's allocator.
     * @remark
     *  Thread-safe.
     */
    allocator_type get_allocator() const;

public: // ACCESSORS

    /**
     * Access specified element.
     * @returns
     *  A reference to the element wrapped in a read_guard.
     * @remark
     *  Thread-safe.
     */
    read_guard cat(size_type index) const;

    /**
     * Access specified element.
     * @returns
     *  A reference to the element wrapped in a write_guard.
     * @remark
     *  Thread-safe.
     */
    write_guard at(size_type index);

    /**
     * Access specified element.
     * @returns
     *  A reference to the element wrapped in a read_guard.
     * @remark
     *  Thread-safe.
     */
    read_guard operator[](size_type index) const;

    /**
     * Access specified element.
     * @returns
     *  A reference to the element wrapped in a write_guard.
     * @remark
     *  It's likely more efficient to use cat() if the caller only needs read access.
     * @remark
     *  Thread-safe.
     */
    write_guard operator[](size_type index);

    /**
     * Accesses the first element.
     * @returns
     *  A reference to the element wrapped in a read_guard.
     * @remark
     *  Thread-safe.
     */
    read_guard cfront() const;

    /**
     * Accesses the first element.
     * @returns
     *  A reference to the element wrapped in a write_guard.
     * @remark
     *  Thread-safe.
     */
    write_guard front();

    /**
     * Accesses the last element.
     * @returns
     *  A reference to the element wrapped in a read_guard.
     * @remark
     *  Thread-safe.
     */
    read_guard cback() const;

    /**
     * Accesses the last element.
     * @returns
     *  A reference to the element wrapped in a write_guard.
     * @remark
     *  Thread-safe.
     */
    write_guard back();

public: // MUTATORS

    /**
     * Increases the capacity of the vector. Does nothing if
     * 'size' < capacity.
     * @remark 
     *  Not thread-safe.
     */
    void reserve(size_type size);

    /**
     * Changes the number of elements in the vector to 'size'.
     * @remark 
     *  Not thread-safe.
     */
    void resize(size_type size);

    /**
     * Changes the number of elements in the vector to 'size'. New elements
     * will have the value 'fill'.
     * @remark 
     *  Not thread-safe.
     */
    void resize(size_type size, value_type const& fill);

    /**
     * Copies the specified value into the back of the container.
     * @remark 
     *  Thread-safe.
     */
    void push_back(value_type const& val);

    /**
     * Moves the specified value into the back of the container.
     * @remark 
     *  Thread-safe.
     */
    void push_back(value_type&& val);

    /**
     * Removes the last element.
     * @remark 
     *  Thread-safe.
     */
    template<typename... TArgs>
    void emplace_back(TArgs&&... args);

    /**
     * Removes the last element.
     * @remark 
     *  Thread-safe.
     * @remark
     *  If the vector is empty. This function is undefined.
     */
    void pop_back();

    /**
     * Removes the last element and returns it. Basically packages back() and
     * and pop_back() into a single thread-safe transaction.
     * @remark 
     *  Thread-safe.
     * @remark
     *  If the vector is empty. This function is undefined.
     */
    pop_back_result pop_back_and_get();

    /**
     * Removes the specified element and replaces it with the last element.
     * @remark 
     *  Thread-safe.
     * @remark
     *  If index is out of range or the vector is empty, this function is undefined.
     */
    void swap_out(size_type index);

    /**
     * Removes the specified element and replaces it with the last element.
     * @remark 
     *  Thread-safe.
     * @remark
     *  If iter is out of range or the vector is empty, this function is undefined.
     */
    void swap_out(iterator& iter);

    /**
     * Clears the data from the table. Leaves the capacity of the vector unchanged.
     * @remark
     *  Thread-safe.
     */
    void clear();

private: // PRIVATE

    template<typename... TArgs>
    void _emplaceBack(TArgs&&... args);
    void _allocateVectors(allocator_type const& allocator);
    void _deallocateVectors();
    void _deepCopy(ParallelVector const& src);
    void _resize(size_type count, value_type const& fill);
    slot_type* _readAt(size_type index) const;
    slot_type* _writeAt(size_type index);
    void _lock();
    void _unlock();

private:

    static constexpr size_type BAD_INDEX    = numericLimits<size_type>::max();
    static constexpr size_type MAX_CAPACITY = BAD_INDEX - 1;

    struct GTS_ALIGN(GTS_NO_SHARING_CACHE_LINE_SIZE) AlignedMutex
    {
        mutex_type m;
    };

    GTS_ALIGN(GTS_NO_SHARING_CACHE_LINE_SIZE) Atomic<size_type> m_size;
    GTS_ALIGN(GTS_NO_SHARING_CACHE_LINE_SIZE) AlignedMutex* m_pStripedMutexes;
    ticket_vec* m_pVecs;
    size_type m_numVecs;
};

/** @} */ // end of ParallelContainers
/** @} */ // end of Containers

#include "ParallelVector.inl"

#ifdef GTS_MSVC
#pragma warning( pop )
#endif

} // namespace gts
