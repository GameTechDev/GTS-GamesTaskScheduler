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

#include "gts/platform/Utils.h"
#include "gts/platform/Thread.h"
#include "gts/containers/AlignedAllocator.h"
#include "gts/containers/Vector.h"

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
 *  The base class for all Guards.
 */
template<typename T, typename TAccessorSharedMutex, typename TGrowSharedMutex>
class GuardBase
{
public:

    ~GuardBase() = default;
    GuardBase() = delete;
    GuardBase(GuardBase const&) = delete;
    GuardBase& operator=(GuardBase const&) = delete;
    GuardBase(GuardBase&& other);
    GuardBase& operator=(GuardBase&& other);
    GuardBase(T* pItem, TAccessorSharedMutex* pAccessorMutex, TGrowSharedMutex* pGrowMutex);

    // TODO: implicit conversion operator?

    /**
     * @brief
     *  Gets the contained value.
     * @returns The contained value.
     */
    T const& get() const;

    /**
     * @brief
     *  Checks if the contained value is valid.
     * @returns True if valid.
     */
    bool isValid() const;

protected:

    T* m_pItem;
    TAccessorSharedMutex* m_pAccessorMutex;
    TGrowSharedMutex* m_pGrowMutex;
};

} // namespace internal

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A read-only item guarded by a mutex. The mutex is released on destruction.
 * @remark
 *  Keeping this object alive will block other threads from writing.
 */
template<typename T, typename TAccessorSharedMutex, typename TGrowSharedMutex>
class ReadGuard : public internal::GuardBase<T, TAccessorSharedMutex, TGrowSharedMutex>
{
    using guard_base_type = internal::GuardBase<T, TAccessorSharedMutex, TGrowSharedMutex>;

public:

    ReadGuard() = delete;
    ReadGuard(ReadGuard const&) = delete;
    ReadGuard(ReadGuard&&) = default;
    ReadGuard& operator=(ReadGuard const&) = delete;
    ReadGuard& operator=(ReadGuard&&) = default;
    ReadGuard(T* pItem, TAccessorSharedMutex* pAccessorMutex, TGrowSharedMutex* pGrowMutex);

    /**
     * @brief
     *  Unlocks the guarded element.
     */
    ~ReadGuard();
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
* @brief
*  A read-write item guarded by a mutex. The mutex is released on destruction.
* @remark
*  Keeping this object alive will block other threads.
*/
template<typename T, typename TAccessorSharedMutex, typename TGrowSharedMutex>
class WriteGuard : public internal::GuardBase<T, TAccessorSharedMutex, TGrowSharedMutex>
{
    using guard_base_type = internal::GuardBase<T, TAccessorSharedMutex, TGrowSharedMutex>;

public:

    WriteGuard() = delete;
    WriteGuard(WriteGuard const&) = delete;
    WriteGuard(WriteGuard&&) = default;
    WriteGuard& operator=(WriteGuard const&) = delete;
    WriteGuard& operator=(WriteGuard&&) = default;
    WriteGuard(T* pItem, TAccessorSharedMutex* pAccessorMutex, TGrowSharedMutex* pGrowMutex);
    WriteGuard& operator=(T const& val);
    WriteGuard& operator=(T&& val);

    // TODO: implicit conversion operator?

    /**
     * @brief
     *  Unlocks the guarded element.
     */
    ~WriteGuard();

    /**
     * @brief
     *  Gets the contained value.
     * @returns The contained value.
     */
    T& get();
};

/** @} */ // end of ParallelContainers
/** @} */ // end of Containers

#include "Guards.inl"

} // namespace gts
