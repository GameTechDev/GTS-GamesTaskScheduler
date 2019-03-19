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

//------------------------------------------------------------------------------
void atomic_thread_fence(gts::memory_order order)
{
    std::atomic_thread_fence((std::memory_order)order);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// AtomicCommon:

//------------------------------------------------------------------------------
template<typename T>
AtomicCommon<T>::AtomicCommon(T val)
    : m_atom(val)
{
}

//------------------------------------------------------------------------------
template<typename T>
bool AtomicCommon<T>::is_lock_free() const volatile
{
    return m_atom.is_lock_free();
}

//------------------------------------------------------------------------------
template<typename T>
bool AtomicCommon<T>::is_lock_free() const
{
    return m_atom.is_lock_free();
}

//------------------------------------------------------------------------------
template<typename T>
void AtomicCommon<T>::store(T value, gts::memory_order order) volatile
{
    m_atom.store(value, (std::memory_order)order);
}

//------------------------------------------------------------------------------
template<typename T>
void AtomicCommon<T>::store(T value, gts::memory_order order)
{
    m_atom.store(value, (std::memory_order)order);
}

//------------------------------------------------------------------------------
template<typename T>
T AtomicCommon<T>::load(gts::memory_order order) const volatile
{
    return m_atom.load((std::memory_order)order);
}

//------------------------------------------------------------------------------
template<typename T>
T AtomicCommon<T>::load(gts::memory_order order) const
{
    return m_atom.load((std::memory_order)order);
}

//------------------------------------------------------------------------------
template<typename T>
T AtomicCommon<T>::exchange(T value, gts::memory_order order) volatile
{
    return m_atom.exchange(value, (std::memory_order)order);
}

//------------------------------------------------------------------------------
template<typename T>
T AtomicCommon<T>::exchange(T value, gts::memory_order order)
{
    return m_atom.exchange(value, (std::memory_order)order);
}

//------------------------------------------------------------------------------
template<typename T>
bool AtomicCommon<T>::compare_exchange_weak(T& expected, T value, gts::memory_order order1, gts::memory_order order2) volatile
{
    return m_atom.compare_exchange_weak(expected, value, (std::memory_order)order1, (std::memory_order)order2);
}

template<typename T>
bool AtomicCommon<T>::compare_exchange_weak(T& expected, T value, gts::memory_order order1, gts::memory_order order2)
{
    return m_atom.compare_exchange_weak(expected, value, (std::memory_order)order1, (std::memory_order)order2);
}

//------------------------------------------------------------------------------
template<typename T>
bool AtomicCommon<T>::compare_exchange_weak(T& expected, T value, gts::memory_order order) volatile
{
    return m_atom.compare_exchange_weak(expected, value, (std::memory_order)order, (std::memory_order)order);
}

//------------------------------------------------------------------------------
template<typename T>
bool AtomicCommon<T>::compare_exchange_weak(T& expected, T value, gts::memory_order order)
{
    return m_atom.compare_exchange_weak(expected, value, (std::memory_order)order, (std::memory_order)order);
}

//------------------------------------------------------------------------------
template<typename T>
bool AtomicCommon<T>::compare_exchange_strong(T& expected, T value, gts::memory_order order1, gts::memory_order order2) volatile
{
    return m_atom.compare_exchange_strong(expected, value, (std::memory_order)order1, (std::memory_order)order2);
}

//------------------------------------------------------------------------------
template<typename T>
bool AtomicCommon<T>::compare_exchange_strong(T& expected, T value, gts::memory_order order1, gts::memory_order order2)
{
    return m_atom.compare_exchange_strong(expected, value, (std::memory_order)order1, (std::memory_order)order2);
}

//------------------------------------------------------------------------------
template<typename T>
bool AtomicCommon<T>::compare_exchange_strong(T& expected, T value, gts::memory_order order) volatile
{
    return m_atom.compare_exchange_strong(expected, value, (std::memory_order)order, (std::memory_order)order);
}

//------------------------------------------------------------------------------
template<typename T>
bool AtomicCommon<T>::compare_exchange_strong(T& expected, T value, gts::memory_order order)
{
    return m_atom.compare_exchange_strong(expected, value, (std::memory_order)order, (std::memory_order)order);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

//------------------------------------------------------------------------------
template<typename T>
T AtomicArithmetic<T>::fetch_add(T rhs, gts::memory_order order) volatile
{
    return AtomicCommon<T>::m_atom.fetch_add(rhs, (std::memory_order)order);
}

//------------------------------------------------------------------------------
template<typename T>
T AtomicArithmetic<T>::fetch_add(T rhs, gts::memory_order order)
{
    return AtomicCommon<T>::m_atom.fetch_add(rhs, (std::memory_order)order);
}

//------------------------------------------------------------------------------
template<typename T>
T AtomicArithmetic<T>::fetch_sub(T rhs, gts::memory_order order) volatile
{
    return AtomicCommon<T>::m_atom.fetch_sub(rhs, (std::memory_order)order);
}

//------------------------------------------------------------------------------
template<typename T>
T AtomicArithmetic<T>::fetch_sub(T rhs, gts::memory_order order)
{
    return AtomicCommon<T>::m_atom.fetch_sub(rhs, (std::memory_order)order);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

//------------------------------------------------------------------------------
template<typename T>
T AtomicPointer<T>::fetch_add(ptrdiff_t rhs, gts::memory_order order) volatile
{
    return AtomicCommon<T>::m_atom.fetch_add(rhs, (std::memory_order)order);
}

//------------------------------------------------------------------------------
template<typename T>
T AtomicPointer<T>::fetch_add(ptrdiff_t rhs, gts::memory_order order)
{
    return AtomicCommon<T>::m_atom.fetch_add(rhs, (std::memory_order)order);
}

//------------------------------------------------------------------------------
template<typename T>
T AtomicPointer<T>::fetch_sub(ptrdiff_t rhs, gts::memory_order order) volatile
{
    return AtomicCommon<T>::m_atom.fetch_sub(rhs, (std::memory_order)order);
}

//------------------------------------------------------------------------------
template<typename T>
T AtomicPointer<T>::fetch_sub(ptrdiff_t rhs, gts::memory_order order)
{
    return AtomicCommon<T>::m_atom.fetch_sub(rhs, (std::memory_order)order);
}
