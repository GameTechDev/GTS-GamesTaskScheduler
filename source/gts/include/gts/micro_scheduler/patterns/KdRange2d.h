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

/** 
 * @addtogroup MicroScheduler
 * @{
 */

/** 
 * @addtogroup ParallelPatterns
 * @{
 */

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  An iteration range over a 2D data set. Splits occur along the largest
 *  dimension unless the minimum size is reached for all dimensions. Successive
 *  splits result in a Kd-tree where each leaf represents a unit of work.
 * @details
 *  Derivative of TBB blocked_range2d. https://github.com/intel/tbb
 */
template<typename TIterType>
class KdRange2d
{
public: // CREATORS

    static constexpr SubRangeIndex::Type X = SubRangeIndex::X;
    static constexpr SubRangeIndex::Type Y = SubRangeIndex::Y;
    static constexpr size_t SPLIT_FACTOR   = 2;
    static constexpr size_t MAX_SPLITS     = SPLIT_FACTOR - 1;
    static constexpr size_t DIMENSIONALITY = 2;

    using iter_type    = TIterType;
    using range_type   = Range1d<iter_type>;
    using split_result = SplitResult<KdRange2d, MAX_SPLITS, DIMENSIONALITY>;
    using size_type    = typename range_type::size_type;

    KdRange2d() = default;
    KdRange2d(KdRange2d const&) = default;

    //--------------------------------------------------------------------------
    ~KdRange2d()
    {}

    //--------------------------------------------------------------------------
    /**
     * @brief
     *  Constructs a KdRange2d.
     * @param xBegin
     *  An iterator to the beginning of the x-dimension range.
     * @param xEnd
     *  An iterator to the end of the x-dimension range.
     * @param xMinSize
     *  The smallest sub-range the x-dimension can be divided into. Must be >= splitOnMultiplesOf.
     * @param yBegin
     *  An iterator to the beginning of the y-dimension range.
     * @param yEnd
     *  An iterator to the end of the y-dimension range.
     * @param yMinSize
     *  The smallest sub-range the y-dimension can be divided into. Must be >= splitOnMultiplesOf.
     * @param xSplitOnMultiplesOf
     *  Tries to ensure that each sub-range in the x-dimensions is a multiple
     *  of this value.
     * @param ySplitOnMultiplesOf
     *  Tries to ensure that each sub-range in the y-dimensions is a multiple
     *  of this value.
     */
    GTS_INLINE KdRange2d(
        iter_type xBegin, iter_type xEnd, size_type xMinSize,
        iter_type yBegin, iter_type yEnd, size_type yMinSize,
        size_type xSplitOnMultiplesOf = 1,
        size_type ySplitOnMultiplesOf = 1)
        : m_subRanges{{xBegin, xEnd, xMinSize, xSplitOnMultiplesOf}, {yBegin, yEnd, yMinSize, ySplitOnMultiplesOf}}
    {}
    

public: // ACCESSORS

    //--------------------------------------------------------------------------
    GTS_INLINE range_type const& xRange() const
    {
        return m_subRanges[X];
    }

    //--------------------------------------------------------------------------
    GTS_INLINE range_type& xRange()
    {
        return m_subRanges[X];
    }

    //--------------------------------------------------------------------------
    GTS_INLINE range_type const& yRange() const
    {
        return m_subRanges[Y];
    }

    //--------------------------------------------------------------------------
    GTS_INLINE range_type& yRange()
    {
        return m_subRanges[Y];
    }

    //--------------------------------------------------------------------------
    GTS_INLINE range_type const& subRange(SubRangeIndex::Type index) const
    {
        return m_subRanges[index];
    }

    //--------------------------------------------------------------------------
    GTS_INLINE range_type& subRange(SubRangeIndex::Type index)
    {
        return m_subRanges[index];
    }

    //--------------------------------------------------------------------------
    GTS_INLINE KdRange2d xNeighbor() const
    {
        KdRange2d neighbor = *this;

        neighbor.xRange().begin() = m_subRanges[X].end();
        neighbor.xRange().end() = m_subRanges[X].end() + m_subRanges[X].size();
        if (neighbor.xRange().end() > m_subRanges[X].initialSize())
        {
            neighbor.xRange().end() = m_subRanges[X].initialSize();
        }

        if(m_wasSplitOn == SS_X)
        {
            neighbor.yRange().end() = m_subRanges[Y].end() + m_subRanges[Y].size() * 2;
            if (neighbor.yRange().end() > m_subRanges[Y].initialSize())
            {
                neighbor.yRange().end() = m_subRanges[Y].initialSize();
            }
        }

        return neighbor;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE KdRange2d yNeighbor() const
    {
        KdRange2d neighbor = *this;

        if(m_wasSplitOn == SS_Y)
        {
            neighbor.xRange().end() = m_subRanges[X].end() + m_subRanges[X].size() * 2;
            if (neighbor.xRange().end() > m_subRanges[X].initialSize())
            {
                neighbor.xRange().end() = m_subRanges[X].initialSize();
            }
        }

        neighbor.yRange().begin() = m_subRanges[Y].end();
        neighbor.yRange().end() = m_subRanges[Y].end() + m_subRanges[Y].size();
        if (neighbor.yRange().end() > m_subRanges[Y].initialSize())
        {
            neighbor.yRange().end() = m_subRanges[Y].initialSize();
        }
        return neighbor;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE void resetInitialSize(size_type initSize, SubRangeIndex::Type index)
    {
        m_subRanges[index].resetInitialSize(initSize);
    }

    //--------------------------------------------------------------------------
    GTS_INLINE bool isDivisible() const
    {
        for (uint32_t ii = 0; ii < DIMENSIONALITY; ++ii)
        {
            if (m_subRanges[ii].isDivisible())
            {
                return true;
            }
        }
        return false;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE bool empty() const
    {
        for (uint32_t ii = 0; ii < DIMENSIONALITY; ++ii)
        {
            if (!m_subRanges[ii].empty())
            {
                return false;
            }
        }
        return true;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE size_t size() const
    {
        size_t size = 0;
        for (uint32_t ii = 0; ii < DIMENSIONALITY; ++ii)
        {
            size *= m_subRanges[ii].size();
        }
        return size;
    }

    //--------------------------------------------------------------------------
    /**
     * @brief
     *  Splits the range into two possible subranges. There are two cases
     *  based which dimensions are divisible.
     * @param result
     *  The returned results from the split.
     */
    template<typename TSplitter>
    GTS_INLINE void split(split_result& results, TSplitter const& splitter)
    {
        results.size      = MAX_SPLITS;
        results.ranges[0] = *this;
        KdRange2d& r      = results.ranges[0];

        // If xRange is relatively smaller than yRange,
        if (m_subRanges[X].size() * m_subRanges[Y].minSize() < m_subRanges[Y].size() * m_subRanges[X].minSize())
        {
            // split yRange.
            r.m_subRanges[Y].begin() = range_type::splitHelper(m_subRanges[Y], splitter);
            r.m_subRanges[Y].end()   = m_subRanges[Y].end();
            m_subRanges[Y].end()     = r.m_subRanges[Y].begin();
            r.m_wasSplitOn           = SS_Y;
        }
        else
        {
            // Otherwise, split xRange.
            r.m_subRanges[X].begin()     = range_type::splitHelper(m_subRanges[X], splitter);
            r.m_subRanges[X].end()       = m_subRanges[X].end();
            m_subRanges[X].end()         = r.m_subRanges[X].begin();
            r.m_wasSplitOn               = SS_X;
        }
    }

    //--------------------------------------------------------------------------
    static GTS_INLINE uint16_t adjustDivisor(uint16_t initialSplitDepth, bool isStatic)
    {
        if (isStatic)
        {
            return initialSplitDepth - 1;
        }
        return initialSplitDepth;
    }

    //--------------------------------------------------------------------------
    static GTS_INLINE uint16_t splitInitialDepth(uint16_t initialSplitDepth, bool isStatic)
    {
        if (isStatic)
        {
            return initialSplitDepth - 1;
        }
        return initialSplitDepth / SPLIT_FACTOR;
    }

    //--------------------------------------------------------------------------
    static GTS_INLINE uint16_t finalSplitDivisor(uint16_t initialSplitDepth, bool isStatic)
    {
        if (isStatic)
        {
            return 0;
        }
        return initialSplitDepth / SPLIT_FACTOR;
    }

private:

    enum SplitState
    {
        SS_NONE, SS_X, SS_Y
    };

    range_type m_subRanges[DIMENSIONALITY];
    SplitState m_wasSplitOn = SS_NONE;
};

/** @} */ // end of ParallelPatterns
/** @} */ // end of MicroScheduler

} // namespace gts
