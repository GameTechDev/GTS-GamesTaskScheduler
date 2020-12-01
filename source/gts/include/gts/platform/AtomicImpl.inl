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

#ifndef GTS_HAS_CUSTOM_ATOMICS_WRAPPERS

//------------------------------------------------------------------------------
template<typename T>
T Atomics::load(GTS_ATOMIC_TYPE<T>const & atomic, int32_t order)
{
    return atomic.load((std::memory_order)order);
}

//------------------------------------------------------------------------------
template<typename T>
void Atomics::store(GTS_ATOMIC_TYPE<T>& atomic, T value, int32_t order)
{
    return atomic.store(value, (std::memory_order)order);
}

//------------------------------------------------------------------------------
template<typename T>
T Atomics::fetch_add(GTS_ATOMIC_TYPE<T>& atomic, T inc, int32_t order)
{
    return atomic.fetch_add(inc, (std::memory_order)order);
}

//------------------------------------------------------------------------------
template<typename T>
T Atomics::fetch_and(GTS_ATOMIC_TYPE<T>& atomic, T rhs, int32_t order)
{
    return atomic.fetch_and(rhs, (std::memory_order)order);
}

//------------------------------------------------------------------------------
template<typename T>
T Atomics::fetch_or(GTS_ATOMIC_TYPE<T>& atomic, T rhs, int32_t order)
{
    return atomic.fetch_or(rhs, (std::memory_order)order);
}

//------------------------------------------------------------------------------
template<typename T>
T Atomics::exchange(GTS_ATOMIC_TYPE<T>& atomic, T value, int32_t order)
{
    return atomic.exchange(value, (std::memory_order)order);
}

//------------------------------------------------------------------------------
template<typename T>
bool Atomics::compare_exchange_weak(GTS_ATOMIC_TYPE<T>& atomic, T& expected, T value, int32_t xchgOrder, int32_t loadOrder)
{
    return atomic.compare_exchange_weak(expected, value, (std::memory_order)xchgOrder, (std::memory_order)loadOrder);
}

//------------------------------------------------------------------------------
template<typename T>
bool Atomics::compare_exchange_strong(GTS_ATOMIC_TYPE<T>& atomic, T& expected, T value, int32_t xchgOrder, int32_t loadOrder)
{
    return atomic.compare_exchange_strong(expected, value, (std::memory_order)xchgOrder, (std::memory_order)loadOrder);
}

#endif

} // namespace internal

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// AtomicCommon:

//------------------------------------------------------------------------------
template<typename T>
constexpr AtomicCommon<T>::AtomicCommon(T val)
    : m_atom(val)
{
}

//------------------------------------------------------------------------------
template<typename T>
void AtomicCommon<T>::store(T value, gts::memory_order order)
{
    GTS_ATOMIC_STORE(this->m_atom, value, (int32_t)order);
}

//------------------------------------------------------------------------------
template<typename T>
T AtomicCommon<T>::load(gts::memory_order order) const
{
    return GTS_ATOMIC_LOAD(this->m_atom, (int32_t)order);
}

//------------------------------------------------------------------------------
template<typename T>
T AtomicCommon<T>::exchange(T value, gts::memory_order order)
{
    return GTS_ATOMIC_EXCHANGE(this->m_atom, value, (int32_t)order);
}

//------------------------------------------------------------------------------
template<typename T>
bool AtomicCommon<T>::compare_exchange_weak(T& expected, T value, gts::memory_order xchgOrder, gts::memory_order loadOrder)
{
    return GTS_ATOMIC_COMPARE_EXCHANGE_WEAK(this->m_atom, expected, value, (int32_t)xchgOrder, (int32_t)loadOrder);
}

//------------------------------------------------------------------------------
template<typename T>
bool AtomicCommon<T>::compare_exchange_strong(T& expected, T value, gts::memory_order xchgOrder, gts::memory_order loadOrder)
{
    return GTS_ATOMIC_COMPARE_EXCHANGE_STRONG(this->m_atom, expected, value, (int32_t)xchgOrder, (int32_t)loadOrder);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

//------------------------------------------------------------------------------
template<typename T>
T AtomicArithmetic<T>::fetch_add(T rhs, gts::memory_order order)
{
    return GTS_ATOMIC_FETCH_ADD(this->m_atom, rhs, (int32_t)order);
}

//------------------------------------------------------------------------------
template<typename T>
T AtomicArithmetic<T>::fetch_sub(T rhs, gts::memory_order order)
{
    return GTS_ATOMIC_FETCH_ADD(this->m_atom, T(0 - typename std::make_unsigned<T>::type(rhs)), (int32_t)order);
}

//------------------------------------------------------------------------------
template<typename T>
T AtomicArithmetic<T>::fetch_and(T rhs, gts::memory_order order)
{
    return GTS_ATOMIC_FETCH_AND(this->m_atom, rhs, (int32_t)order);
}

//------------------------------------------------------------------------------
template<typename T>
T AtomicArithmetic<T>::fetch_or(T rhs, gts::memory_order order)
{
    return GTS_ATOMIC_FETCH_OR(this->m_atom, rhs, (int32_t)order);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

//------------------------------------------------------------------------------
template<typename T>
T AtomicPointer<T>::fetch_add(ptrdiff_t rhs, gts::memory_order order)
{
    return GTS_ATOMIC_FETCH_ADD(this->m_atom, rhs, (int32_t)order);
}

//------------------------------------------------------------------------------
template<typename T>
T AtomicPointer<T>::fetch_sub(ptrdiff_t rhs, gts::memory_order order)
{
    return GTS_ATOMIC_FETCH_ADD(this->m_atom, -rhs, (int32_t)order);
}
