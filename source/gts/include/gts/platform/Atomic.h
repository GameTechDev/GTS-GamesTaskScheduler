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

#include "gts/platform/Assert.h"

#ifndef GTS_USE_CUSTOM_ATOMICS
#include <atomic>
#endif

namespace gts {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A enumeration of atomic memory orders. See C++ memory model.
 */
enum class memory_order
{
    relaxed,
    consume,
    acquire,
    release,
    acq_rel,
    seq_cst
};

//------------------------------------------------------------------------------
/**
 * A wrapper for memory fences. Uses the STL by default. Define
 * GTS_USE_CUSTOM_ATOMICS to add your own backend, or replace everything
 * completely as long as this interface exists.
 */
GTS_INLINE void atomic_thread_fence(gts::memory_order order = gts::memory_order::seq_cst);

//------------------------------------------------------------------------------
/**
 * A wrapper for signal fences. Uses the STL by default. Define
 * GTS_USE_CUSTOM_ATOMICS to add your own backend, or replace everything
 * completely as long as this interface exists.
 */
GTS_INLINE void atomic_signal_fence(gts::memory_order order = gts::memory_order::seq_cst);


#pragma warning( push )
#pragma warning( disable : 4522) // multiple assignment ops

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

    AtomicCommon(T val);

    bool is_lock_free() const volatile;
    bool is_lock_free() const;
    void store(T value, gts::memory_order order = gts::memory_order::seq_cst) volatile;
    void store(T value, gts::memory_order order = gts::memory_order::seq_cst);
    T load(gts::memory_order order = gts::memory_order::seq_cst) const volatile;
    T load(gts::memory_order order = gts::memory_order::seq_cst) const;
    T exchange(T value, gts::memory_order order = gts::memory_order::seq_cst) volatile;
    T exchange(T value, gts::memory_order order = gts::memory_order::seq_cst);
    bool compare_exchange_weak(T& expected, T value, gts::memory_order order1, gts::memory_order order2) volatile;
    bool compare_exchange_weak(T& expected, T value, gts::memory_order order1, gts::memory_order order2);
    bool compare_exchange_weak(T& expected, T value, gts::memory_order order = gts::memory_order::seq_cst) volatile;
    bool compare_exchange_weak(T& expected, T value, gts::memory_order order = gts::memory_order::seq_cst);
    bool compare_exchange_strong(T& expected, T value, gts::memory_order order1, gts::memory_order order2) volatile;
    bool compare_exchange_strong(T& expected, T value, gts::memory_order order1, gts::memory_order order2);
    bool compare_exchange_strong(T& expected, T value, gts::memory_order order = gts::memory_order::seq_cst) volatile;
    bool compare_exchange_strong(T& expected, T value, gts::memory_order order = gts::memory_order::seq_cst);

protected:

#ifndef GTS_USE_CUSTOM_ATOMICS
    std::atomic<T> m_atom;
#else
    alignas(T) T m_atom;
#endif
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

    AtomicArithmetic(T val) : AtomicCommon<T>(val) {}

    T fetch_add(T rhs, gts::memory_order order = gts::memory_order::seq_cst) volatile;
    T fetch_add(T rhs, gts::memory_order order = gts::memory_order::seq_cst);
    T fetch_sub(T rhs, gts::memory_order order = gts::memory_order::seq_cst) volatile;
    T fetch_sub(T rhs, gts::memory_order order = gts::memory_order::seq_cst);
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

    AtomicPointer(T val) : AtomicCommon<T>(val) {}

    T fetch_add(ptrdiff_t rhs, gts::memory_order order = gts::memory_order::seq_cst) volatile;
    T fetch_add(ptrdiff_t rhs, gts::memory_order order = gts::memory_order::seq_cst);
    T fetch_sub(ptrdiff_t rhs, gts::memory_order order = gts::memory_order::seq_cst) volatile;
    T fetch_sub(ptrdiff_t rhs, gts::memory_order order = gts::memory_order::seq_cst);
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

    Atomic(T val) : AtomicArithmetic<T>(val) {}
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

    Atomic(T* val) : AtomicPointer<T*>(val) {}
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

    Atomic(bool val) : AtomicCommon<bool>(val) {}
};

#ifndef GTS_USE_CUSTOM_ATOMICS
#include "AtomicImpl_STL.inl"
#else
#error "Missing custom atomics implementation."
#endif

#pragma warning( pop )

} // namespace gts

