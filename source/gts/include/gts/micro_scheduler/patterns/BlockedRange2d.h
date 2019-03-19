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

namespace gts {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  An iteration range over a dense, 2D set of data. The range is split by
 *  choosing the larger dimension until the minSize is reached for all dimensions.
 */
template<typename TIter0, typename TIter1 = TIter0>
class BlockedRange2d
{
public: // CREATORS

    BlockedRange2d() = default;
    BlockedRange2d(BlockedRange2d const&) = default;

    //--------------------------------------------------------------------------
    ~BlockedRange2d()
    {
    }

    //--------------------------------------------------------------------------
    /**
     *      --------> range0
     *     |
     *     |
     *     |
     *     v
     *     range1
     */
    GTS_INLINE BlockedRange2d(TIter0 begin0, TIter0 end0, size_t minSize0, TIter1 begin1, TIter1 end1, size_t minSize1)
        : m_range0(begin0, end0, minSize0)
        , m_range1(begin1, end1, minSize1)
    {}

public: // ACCESSORS

    //--------------------------------------------------------------------------
    GTS_INLINE BlockedRange1d<TIter0> const& range0() const
    {
        return m_range0;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE BlockedRange1d<TIter1> const& range1() const
    {
        return m_range1;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE bool isDivisible() const
    {
        return m_range0.isDivisible() || m_range1.isDivisible();
    }

    //--------------------------------------------------------------------------
    GTS_INLINE bool empty() const
    {
        return m_range0.empty() && m_range1.empty();
    }

public: // MUTATORS

    //--------------------------------------------------------------------------
    GTS_INLINE BlockedRange2d split(size_t const divisor = 2)
    {
        GTS_ASSERT(isDivisible());

        if (m_range0.size() * m_range1.minSize() < m_range1.size() * m_range0.minSize())
        {
            return BlockedRange2d(m_range0, m_range1.split(divisor));
        }
        else
        {
            return BlockedRange2d(m_range0.split(divisor), m_range1);
        }
    }

    //--------------------------------------------------------------------------
    GTS_INLINE void destroy()
    {
        m_range0.destroy();
        m_range1.destroy();
    }

private:

    //--------------------------------------------------------------------------
    GTS_INLINE BlockedRange2d(BlockedRange1d<TIter0> const& range0, BlockedRange1d<TIter1> const& range1)
        : m_range0(range0)
        , m_range1(range1)
    {}

    BlockedRange1d<TIter0> m_range0;
    BlockedRange1d<TIter1> m_range1;
};

} // namespace gts
