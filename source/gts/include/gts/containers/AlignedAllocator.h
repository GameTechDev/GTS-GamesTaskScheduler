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

#include <stdexcept>
#include <new>

#include "gts/platform/Machine.h"
#include "gts/platform/Memory.h"

namespace gts {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  STL allocator for aligned data.
 *
 * @details
 *  Modified from:
 *  <http://blogs.msdn.com/b/vcblog/archive/2008/08/28/the-mallocator.aspx>
 */
template <typename T, size_t ALIGNMENT>
class AlignedAllocator
{
public: // TYPES:

    // The following will be the same for all allocators.
    typedef T* pointer;
    typedef const T* const_pointer;
    typedef T& reference;
    typedef const T& const_reference;
    typedef T value_type;
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;

    // The following must be the same for all allocators.
    template <typename U>
    struct rebind
    {
        typedef AlignedAllocator<U, ALIGNMENT> other;
    };

public: // STRUCTORS:

    AlignedAllocator() = default;
    AlignedAllocator(const AlignedAllocator&) = default;
    ~AlignedAllocator() = default;

    //--------------------------------------------------------------------------
    template <typename U> explicit AlignedAllocator(const AlignedAllocator<U, ALIGNMENT>&)
    {}

public: // ACCESSORS:

    //--------------------------------------------------------------------------
    T* address(T& r) const
    {
        return &r;
    }

    //--------------------------------------------------------------------------
    const T* address(const T& s) const
    {
        return &s;
    }

    //--------------------------------------------------------------------------
    size_t max_size() const
    {
        // The following has been carefully written to be independent of
        // the definition of size_t and to avoid signed/unsigned warnings.
        return (static_cast<size_t>(0) - static_cast<size_t>(1)) / sizeof(T);
    }

    //--------------------------------------------------------------------------
    bool operator!=(const AlignedAllocator& other) const
    {
        return !(*this == other);
    }

    //--------------------------------------------------------------------------
    void construct(T* const p, const T& t) const
    {
        void * const pv = static_cast<void *>(p);

        new (pv) T(t);
    }

    //--------------------------------------------------------------------------
    void destroy(T* const p) const
    {
        GTS_UNREFERENCED_PARAM(p);
        p->~T();
    }

    //--------------------------------------------------------------------------
    bool operator==(const AlignedAllocator&) const
    {
        // Returns true if and only if storage allocated from *this
        // can be deallocated from other, and vice versa.
        // Always returns true for stateless allocators.
        return true;
    }

    //--------------------------------------------------------------------------
    T* allocate(const size_t n) const
    {
        if (n == 0)
        {
            return nullptr;
        }

        // All allocators should contain an integer overflow check.
        // The Standardization Committee recommends that std::length_error
        // be thrown in the case of integer overflow.
        if (n > max_size())
        {
            throw std::length_error("AlignedAllocator<T>::allocate() - Integer overflow.");
        }

        void * const pv = hooks::getAlignedAllocHook()(n * sizeof(T), ALIGNMENT);

        // Allocators should throw std::bad_alloc in the case of memory allocation failure.
        if (pv == nullptr)
        {
            throw std::bad_alloc();
        }

        return static_cast<T *>(pv);
    }

    //--------------------------------------------------------------------------
    void deallocate(T* const p, const size_t) const
    {
        hooks::getAlignedFreeHook()(p);
    }

    //--------------------------------------------------------------------------
    template <typename U>
    T  allocate(const size_t n, const U* /* const hint */) const
    {
        return allocate(n);
    }

private:

    AlignedAllocator& operator=(const AlignedAllocator&) = delete;
};

} // namespace gts
