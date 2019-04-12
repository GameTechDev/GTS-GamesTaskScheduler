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
 *  An iteration range over a dense, 3D set of data. The range is split by
 *  choosing the larger dimension until the minSize is reached for all dimensions.
 */
template<typename TIter0, typename TIter1 = TIter0, typename TIter2 = TIter0>
class BlockedRange3d
{
public: // CREATORS

    BlockedRange3d() = default;
    BlockedRange3d(BlockedRange3d const&) = default;

    //--------------------------------------------------------------------------
    ~BlockedRange3d()
    {
    }

    //--------------------------------------------------------------------------
    /**
     *      range2
     *      ^
     *       \
     *        \
     *         \--------> range0
     *         |
     *         |
     *         |
     *         v
     *         range1
     */
    GTS_INLINE BlockedRange3d(
        TIter0 begin0, TIter0 end0, size_t minSize0,
        TIter1 begin1, TIter1 end1, size_t minSize1,
        TIter2 begin2, TIter2 end2, size_t minSize2)
        : m_range0(begin0, end0, minSize0)
        , m_range1(begin1, end1, minSize1)
        , m_range2(begin2, end2, minSize2)
    {}

public: // ACCESSORS

    //--------------------------------------------------------------------------
    GTS_INLINE BlockedRange1d<TIter0> const& range0() const
    {
        return m_range0;b
    }

    //--------------------------------------------------------------------------
    GTS_INLINE BlockedRange1d<TIter1> const& range1() const
    {
        return m_range1;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE BlockedRange1d<TIter2> const& range2() const
    {
        return m_range2;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE bool isDivisible() const
    {
        return m_range0.isDivisible() || m_range1.isDivisible() || m_range2.isDivisible();
    }

    //--------------------------------------------------------------------------
    GTS_INLINE bool empty() const
    {
        return m_range0.empty() && m_range1.empty() && m_range2.empty();
    }

public: // MUTATORS

    //--------------------------------------------------------------------------
    GTS_INLINE BlockedRange3d split(size_t const divisor = 2)
    {
        GTS_ASSERT(isDivisible());

        // If range2 is relatively smaller than range1,
        if (m_range2.size() * m_range1.minSize() < m_range1.size() * m_range2.minSize())
        {
            // If range1 is relatively smaller than range0,
            if (m_range1.size() * m_range0.minSize() < m_range0.size() * m_range1.minSize())
            {
                // Split range0.
                return BlockedRange3d(m_range0.split(divisor), m_range1, m_range2);
            }
            else
            {
                // Otherwise, split range1.
                return BlockedRange3d(m_range0, m_range1.split(divisor), m_range2);
            }
        }
        // Otherwise range2 is relatively bigger than range1, so
        else
        {
            // If range2 is relatively smaller than range0,
            if (m_range2.size()*double(m_range0.minSize()) < m_range0.size()*double(m_range2.minSize()))
            {
                // Split range0.
                return BlockedRange3d(m_range0.split(divisor), m_range1, m_range2);
            }
            else
            {
                // Otherwise, split range2.
                return BlockedRange3d(m_range0, m_range1, m_range2.split(divisor));
            }
        }
    }

private:

    //--------------------------------------------------------------------------
    GTS_INLINE BlockedRange3d(BlockedRange1d<TIter0> const& range0, BlockedRange1d<TIter1> const& range1, BlockedRange1d<TIter2> const& range2)
        : m_range0(range0)
        , m_range1(range1)
        , m_range2(range2)
    {}

    BlockedRange1d<TIter0> m_range0;
    BlockedRange1d<TIter1> m_range1;
    BlockedRange1d<TIter2> m_range2
};

} // namespace gts
