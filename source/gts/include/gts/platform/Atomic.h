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

#include <cstddef>
#include "gts/platform/Assert.h"

/** 
 * @addtogroup Platform
 * @{
 */

/** 
 * @defgroup Atomics
 *  Wrappers around atomic types.
 * @{
 */

#ifndef GTS_HAS_CUSTOM_ATOMICS_WRAPPERS
#include <atomic>
#endif

namespace gts {
namespace internal {

#ifndef DOXYGEN_SHOULD_SKIP_THIS

#ifndef GTS_HAS_CUSTOM_ATOMICS_WRAPPERS

#define GTS_ATOMIC_TYPE ::std::atomic

#define GTS_MEMORY_ORDER_RELAXED ::std::memory_order_relaxed
#define GTS_MEMORY_ORDER_CONSUME ::std::memory_order_consume
#define GTS_MEMORY_ORDER_ACQUIRE ::std::memory_order_acquire
#define GTS_MEMORY_ORDER_RELEASE ::std::memory_order_release
#define GTS_MEMORY_ORDER_ACQ_REL ::std::memory_order_acq_rel
#define GTS_MEMORY_ORDER_SEQ_CST ::std::memory_order_seq_cst


struct Atomics
{
    Atomics() = delete;
    ~Atomics() = delete;

    template<typename T>
    GTS_INLINE static T load(GTS_ATOMIC_TYPE<T> const& atomic, int32_t order);

    template<typename T>
    GTS_INLINE static void store(GTS_ATOMIC_TYPE<T>& atomic, T value, int32_t order);

    template<typename T>
    GTS_INLINE static T fetch_add(GTS_ATOMIC_TYPE<T>& atomic, T inc, int32_t order);

    template<typename T>
    GTS_INLINE static T fetch_and(GTS_ATOMIC_TYPE<T>& atomic, T rhs, int32_t order);

    template<typename T>
    GTS_INLINE static T fetch_or(GTS_ATOMIC_TYPE<T>& atomic, T rhs, int32_t order);

    template<typename T>
    GTS_INLINE static T exchange(GTS_ATOMIC_TYPE<T>& atomic, T value, int32_t order);

    template<typename T>
    GTS_INLINE static bool compare_exchange_weak(GTS_ATOMIC_TYPE<T>& atomic, T& expected, T value, int32_t xchgOrder, int32_t loadOrder);

    template<typename T>
    GTS_INLINE static bool compare_exchange_strong(GTS_ATOMIC_TYPE<T>& atomic, T& expected, T value, int32_t xchgOrder, int32_t loadOrder);
};

#define GTS_ATOMIC_LOAD(a, memoryOrder) internal::Atomics::load(a, memoryOrder);
#define GTS_ATOMIC_STORE(a, value, memoryOrder) internal::Atomics::store(a, value, memoryOrder);
#define GTS_ATOMIC_FETCH_ADD(a, inc, memoryOrder) internal::Atomics::fetch_add(a, inc, memoryOrder);
#define GTS_ATOMIC_FETCH_AND(a, rhs, memoryOrder) internal::Atomics::fetch_and(a, rhs, memoryOrder);
#define GTS_ATOMIC_FETCH_OR(a, rhs, memoryOrder) internal::Atomics::fetch_or(a, rhs, memoryOrder);
#define GTS_ATOMIC_EXCHANGE(a, value, memoryOrder) internal::Atomics::exchange(a, value, memoryOrder);
#define GTS_ATOMIC_COMPARE_EXCHANGE_WEAK(a, expected, value, xchgMemoryOrder, loadMemoryOrder) internal::Atomics::compare_exchange_weak(a, expected, value, xchgMemoryOrder, loadMemoryOrder);
#define GTS_ATOMIC_COMPARE_EXCHANGE_STRONG(a, expected, value, xchgMemoryOrder, loadMemoryOrder) internal::Atomics::compare_exchange_strong(a, expected, value, xchgMemoryOrder, loadMemoryOrder);

#endif // GTS_HAS_CUSTOM_ATOMICS_WRAPPERS

#endif // DOXYGEN_SHOULD_SKIP_THIS

} // namespace internal

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A enumeration of atomic memory orders. See C++ memory model.
 */
enum class memory_order : int32_t
{
    relaxed = int32_t(GTS_MEMORY_ORDER_RELAXED),
    consume = int32_t(GTS_MEMORY_ORDER_CONSUME),
    acquire = int32_t(GTS_MEMORY_ORDER_ACQUIRE),
    release = int32_t(GTS_MEMORY_ORDER_RELEASE),
    acq_rel = int32_t(GTS_MEMORY_ORDER_ACQ_REL),
    seq_cst = int32_t(GTS_MEMORY_ORDER_SEQ_CST),
};

#ifdef GTS_MSVC
#pragma warning( push )
#pragma warning( disable : 4522) // multiple assignment ops
#endif

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A wrapper class for a stl-like atomic. Uses the STL by default. Define
 *  GTS_USE_CUSTOM_ATOMICS to add your own backend, or replace everything
 *  completely as long as this interface exists.
 */
template<typename T>
struct AtomicCommon
{
    ~AtomicCommon() = default;
    AtomicCommon() = default;
    AtomicCommon(const AtomicCommon&) = delete;
    AtomicCommon& operator=(const AtomicCommon&) = delete;
    AtomicCommon& operator=(const AtomicCommon&) volatile = delete;

    constexpr AtomicCommon(T val);

    void store(T value, gts::memory_order order);
    T load(gts::memory_order order) const;
    T exchange(T value, gts::memory_order order);
    bool compare_exchange_weak(T& expected, T value, gts::memory_order xchgOrder, gts::memory_order loadOrder);
    bool compare_exchange_strong(T& expected, T value, gts::memory_order xchgOrder, gts::memory_order loadOrder);

protected:

    GTS_ATOMIC_TYPE<T> m_atom;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A wrapper class for a stl-like atomic arithmetic specialization. Uses the
 *  STL by default. Define GTS_USE_CUSTOM_ATOMICS to add your own
 *  backend, or replace everything completely as long as this interface exists.
 */
template<typename T>
struct AtomicArithmetic : public AtomicCommon<T>
{
    ~AtomicArithmetic() = default;
    AtomicArithmetic() = default;
    AtomicArithmetic(const AtomicArithmetic&) = delete;
    AtomicArithmetic& operator=(const AtomicArithmetic&) = delete;
    AtomicArithmetic& operator=(const AtomicArithmetic&) volatile = delete;

    constexpr AtomicArithmetic(T val) : AtomicCommon<T>(val) {}

    T fetch_add(T rhs, gts::memory_order order);
    T fetch_sub(T rhs, gts::memory_order order);
    T fetch_and(T rhs, gts::memory_order order);
    T fetch_or(T rhs, gts::memory_order order);
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A wrapper class for a stl-like atomic pointer specialization. Uses the
 *  STL by default. Define GTS_USE_CUSTOM_ATOMICS to add your own
 *  backend, or replace everything completely as long as this interface exists.
 */
template<typename T>
struct AtomicPointer : public AtomicCommon<T>
{
    ~AtomicPointer() = default;
    AtomicPointer() = default;
    AtomicPointer(const AtomicPointer&) = delete;
    AtomicPointer& operator=(const AtomicPointer&) = delete;
    AtomicPointer& operator=(const AtomicPointer&) volatile = delete;

    constexpr AtomicPointer(T val) : AtomicCommon<T>(val) {}

    T fetch_add(ptrdiff_t rhs, gts::memory_order order);
    T fetch_sub(ptrdiff_t rhs, gts::memory_order order);
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  Arithmetic atomic specialization.
 */
template<typename T>
class Atomic : public AtomicArithmetic<T>
{
public:

    ~Atomic() = default;
    Atomic() = default;
    Atomic(const Atomic&) = delete;
    Atomic& operator=(const Atomic&) = delete;
    Atomic& operator=(const Atomic&) volatile = delete;

    constexpr Atomic(T val) : AtomicArithmetic<T>(val) {}
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  Atomic pointer specialization.
 */
template<typename T>
class Atomic<T*> : public AtomicPointer<T*>
{
public:

    ~Atomic() = default;
    Atomic() = default;
    Atomic(const Atomic&) = delete;
    Atomic& operator=(const Atomic&) = delete;
    Atomic& operator=(const Atomic&) volatile = delete;

    constexpr Atomic(T* val) : AtomicPointer<T*>(val) {}
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  Atomic bool specialization.
 */
template<>
class Atomic<bool> : public AtomicCommon<bool>
{
public:

    ~Atomic() = default;
    Atomic() = default;
    Atomic(const Atomic&) = delete;
    Atomic& operator=(const Atomic&) = delete;
    Atomic& operator=(const Atomic&) volatile = delete;

    constexpr Atomic(bool val) : AtomicCommon<bool>(val) {}
};

/** @} */ // end of Atomics
/** @} */ // end of Platform

#include "AtomicImpl.inl"

#ifdef GTS_MSVC
#pragma warning( pop )
#endif

} // namespace gts

