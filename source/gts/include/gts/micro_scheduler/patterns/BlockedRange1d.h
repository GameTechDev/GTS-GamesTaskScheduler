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

#include <utility>

#include "gts/platform/Machine.h"
#include "gts/platform/Assert.h"

namespace gts {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  An iteration range over a dense, 1D set of data. The range can be split
 *  until minSize is reached.
 */
template<typename TIter>
class BlockedRange1d
{
public: // CREATORS

    using iter_type = TIter;

    BlockedRange1d() = default;
    BlockedRange1d(BlockedRange1d const&) = default;

    //--------------------------------------------------------------------------
    GTS_INLINE BlockedRange1d(TIter begin, TIter end, size_t minSize)
        :
        m_begin(begin),
        m_end(end),
        m_minSize(minSize)
    {
        GTS_ASSERT(begin != end);
    }

public: // ACCESSORS

    //--------------------------------------------------------------------------
    GTS_INLINE TIter begin() const
    {
        return m_begin;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE TIter end() const
    {
        return m_end;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE bool empty() const
    {
        return size() == 0;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE size_t size() const
    {
        return _distance(m_begin, m_end);
    }

    //--------------------------------------------------------------------------
    GTS_INLINE size_t minSize() const
    {
        return m_minSize;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE bool isDivisible() const
    {
        return size() > m_minSize;
    }

public: // MUTATORS

    //--------------------------------------------------------------------------
    GTS_INLINE BlockedRange1d split(size_t const divisor = 2)
    {
        GTS_ASSERT(isDivisible());

        size_t numDivisions = size() / m_minSize;
        if (numDivisions > 1)
        {
            numDivisions /= divisor;
        }
        TIter oldEnd = m_end;
        m_end = (TIter)(m_begin + numDivisions * m_minSize);

        return BlockedRange1d(
            m_end,
            oldEnd,
            m_minSize);
    }

    //--------------------------------------------------------------------------
    /**
     * For TIter with a destructor like STL debug iterators.
     */
    GTS_INLINE void destroy()
    {
        _destroy(m_end);
        _destroy(m_begin);
    }

private:

    //--------------------------------------------------------------------------
    template<typename T>
    static void _destroy(T& iter, typename std::enable_if<std::is_destructible<T>::value, T>::type* = nullptr)
    {
        iter.~T();
        iter.~T();
        GTS_UNREFERENCED_PARAM(iter); // VS emits warning without this.
    }

    //--------------------------------------------------------------------------
    template<typename T>
    static T _distance(T begin, T end, typename std::enable_if<std::is_integral<T>::value, T>::type* = nullptr)
    {
        return end - begin;
    }

    //--------------------------------------------------------------------------
    template<typename T>
    static ptrdiff_t _distance(T begin, T end, typename std::enable_if<std::is_pointer<T>::value, T>::type* = nullptr)
    {
        return end - begin;
    }

    //--------------------------------------------------------------------------
    template<typename TIter>
    static typename std::iterator_traits<TIter>::difference_type
    _distance(TIter begin, TIter end)
    {
        return std::distance(begin, end);
    }

    TIter m_begin;
    TIter m_end;
    size_t m_minSize;
};

} // namespace gts
