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

#include <memory>

#include "gts/platform/Assert.h"
#include "gts/platform/Memory.h"
#include "gts/platform/Utils.h"

namespace gts {

/**
 * @addtogroup Containers
 * @{
 */

/**
 * @addtogroup Allocators
 * @{
 */

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  An aligned allocator for GTS containers.
 */
template <size_t ALIGNMENT>
class AlignedAllocator
{
    static_assert(ALIGNMENT >= sizeof(uintptr_t), "ALIGNMENT to small");
    static_assert(isPow2(ALIGNMENT), "ALIGNMENT must be a power of 2.");

public: // STRUCTORS:

    AlignedAllocator()                        = default;
    AlignedAllocator(const AlignedAllocator&) = default;
    ~AlignedAllocator()                       = default;


    //--------------------------------------------------------------------------
    template<typename T>
    size_t max_size() const
    {
        return SIZE_MAX / sizeof(T);
    }

    //--------------------------------------------------------------------------
    template<typename T, typename... TArgs>
    void construct(T* ptr, TArgs&&... args)
    {
        void* const pv = static_cast<void*>(ptr);
        new (pv) T(std::forward<TArgs>(args)...);
    }

    //--------------------------------------------------------------------------
    template<typename T>
    void destroy(T* const ptr)
    {
        ptr->~T();
    }

    //--------------------------------------------------------------------------
    template<typename T>
    T* allocate(const size_t n, size_t align = ALIGNMENT) const
    {
        if (n == 0)
        {
            return nullptr;
        }

        if (n > max_size<T>())
        {
            GTS_ASSERT(0 && "Integer overflow.");
            return nullptr;
        }

        void * const pv = GTS_ALIGNED_MALLOC(n * sizeof(T), align);
        if (pv == nullptr)
        {
            GTS_ASSERT(0 && "Failed Allocation.");
            return nullptr;
        }

        return static_cast<T *>(pv);
    }

    //--------------------------------------------------------------------------
    template<typename T>
    void deallocate(T* const ptr, size_t) const
    {
        if(ptr)
        {
            GTS_ALIGNED_FREE(ptr);
        }
    }

    //--------------------------------------------------------------------------
    template<typename T, typename... TArgs>
    T* new_object(TArgs&&... args)
    {
        T* ptr = allocate<T>(1);
        construct(ptr, std::forward<TArgs>(args)...);
        return ptr;
    }

    //--------------------------------------------------------------------------
    template<typename T>
    void delete_object(T* ptr)
    {
        destroy(ptr);
        deallocate(ptr, sizeof(T));
    }

    //--------------------------------------------------------------------------
    template<typename T>
    T* vector_new_object(size_t count)
    {
        T* ptr = allocate<T>(count);
        for (size_t ii = 0; ii < count; ++ii)
        {
            construct(ptr + ii);
        }
        return ptr;
    }

    //--------------------------------------------------------------------------
    template<typename T>
    void vector_delete_object(T* ptr, size_t count)
    {
        for (size_t ii = 0; ii < count; ++ii)
        {
            destroy(ptr + ii);
        }
        deallocate(ptr, sizeof(T) * count);
    }
};

/** @} */ // end of Allocators
/** @} */ // end of Containers

} // namespace gts
