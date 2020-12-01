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

#include <list>

#include "gts/platform/Machine.h"
#include "gts/platform/Assert.h"
#include "gts/platform/Utils.h"
#include "gts/platform/Atomic.h"
#include "gts/platform/Thread.h"
#include "gts/containers/Vector.h"
#include "gts/containers/AlignedAllocator.h"

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
 *  A single-producer, single-consumer queue. Properties:
    - Wait-free.
    - Unbound.
    - Linearizable.
 * @tparam T
 *  The type stored in the container.
 * @tparam TAllocator
 *  The allocator used by the storage backing.
 */
template<
    typename T,
    typename TAllocator = gts::AlignedAllocator<GTS_NO_SHARING_CACHE_LINE_SIZE>>
class QueueSPSC : private TAllocator
{
public:

    using value_type     = T;
    using size_type      = size_t;
    using allocator_type = TAllocator;

public: // STRUCTORS

    /**
     * Constructs an empty container with the given 'allocator'.
     * @remark
     *  Thread-safe.
     */
    explicit QueueSPSC(allocator_type const& allocator = allocator_type());

    /**
     * Destructs the container. The destructors of the elements are called and the
     * used storage is deallocated.
     * @remark
     *  Not thread-safe.
     */
    ~QueueSPSC();

    /**
    * Copy constructor. Constructs the container with the copy of the contents
    * of 'other'.
    * @remark
    *  Not thread-safe.
    */
    QueueSPSC(QueueSPSC const& other);
    
    /**
     * Move constructor. Constructs the container with the contents of other
     * using move semantics. After the move, other is invalid.
     * @remark
     *  Not thread-safe.
     */
    QueueSPSC(QueueSPSC&& other);

    /**
     * Copy assignment operator. Replaces the contents with a copy of the
     * contents of 'other'.
     * @remark
     *  Not thread-safe.
     */
    QueueSPSC& operator=(QueueSPSC const& other);

    /**
     * Move assignment operator. Replaces the contents with those of other using
     * move semantics. After the move, other is invalid.
     * @remark
     *  Not thread-safe.
     */
    QueueSPSC& operator=(QueueSPSC&& other);

public: // ACCESSORS

    /**
     * @return True of the queue is empty, false otherwise.
     * @remark Not thread-safe. 
     */
    bool empty() const;

    /**
     * @return The number of elements in the queue.
     * @remark Not thread-safe.
     */
    size_type size() const;

    /**
     * @return The capacity of the queue.
     * @remark Not thread-safe.
     */
    size_type capacity() const;

    /**
     * @return This queue's allocator.
     * @remark Thread-safe.
     */
    allocator_type get_allocator() const;

public: // MUTATORS

    /**
     * Increases the capacity of the queue. Does nothing if 'sizePow2' < capacity.
     * @remark Not thread-safe.
     */
    void reserve(size_type sizePow2);

    /**
     * Removes all elements from the queue.
     * @remark Not thread-safe.
     */
    void clear();

    /**
     * Copies 'val' to the back of the queue.
     * @return True if the push, false otherwise.
     * @remark Single producer thread-safe.
     */
    bool tryPush(const value_type& val);

    /**
     * Moves 'val' to the back of the queue.
     * @return True if the push, false otherwise.
     * @remark Single producer thread-safe.
     */
    bool tryPush(value_type&& val);

    /**
     * Pops an element from the queue and copies it into 'out'.
     * @return True if the pop succeeded, false otherwise.
     * @remark Single consumer thread-safe.
     */
    bool tryPop(value_type& out);

private:

    template<typename... TArgs>
    bool _insert(TArgs&&... args);

    void _cleanup();

    void _deepCopy(QueueSPSC const& src);

    GTS_NO_INLINE bool _grow(size_type size, size_type front, size_type back);

    //! The maximum capacity is half the range of the index type, such
    //! that when it wraps around, it will be zero at index zero. This
    //! prevents the dual state of full and empty.
    static constexpr size_type MAX_CAPACITY = numericLimits<size_type>::max() / 2;

    static constexpr size_type MIN_CAPACITY = 2;

    struct RingBuffer
    {
        //! An array of pointers into the data backing array.
        value_type** ppBuff = nullptr;

        //! The size of buff - 1.
        size_type mask = 0;
    };

    struct OldBuffers
    {
        RingBuffer** ppBuff = nullptr;
        size_type size = 0;
    };

    //! The next item to pop.
    Atomic<size_type> m_front;

    //! The last item index.
    Atomic<size_type> m_back;

    //! The storage ring buffer.
    Atomic<RingBuffer*> m_pRingBuffer;

    //! The data backing for m_pRingBuffer.
    value_type** m_ppDataBackingArray;

    //! The size of m_ppDataBackingArray.
    size_t m_dataBackingArraySize;

    //! The old RingBuffers from resizes.
    Atomic<OldBuffers*> m_pOldBuffers;
};

#ifdef GTS_MSVC
#pragma warning( pop )
#endif

/** @} */ // end of ParallelContainers
/** @} */ // end of Containers

#include "QueueSPSC.inl"

} // namespace gts
