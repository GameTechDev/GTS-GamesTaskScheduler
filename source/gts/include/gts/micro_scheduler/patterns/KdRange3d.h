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
 *  An iteration range over a 3D data set. Splits occur along the largest
 *  dimension unless the minimum size is reached for all dimensions. Successive
 *  splits result in a Kd-tree where each leaf represents a unit of work.
 * @details
 *  Derivative of TBB blocked_range3d. https://github.com/intel/tbb
 */
template<typename TIterType>
class KdRange3d
{
public: // CREATORS


    static constexpr SubRangeIndex::Type X = SubRangeIndex::X;
    static constexpr SubRangeIndex::Type Y = SubRangeIndex::Y;
    static constexpr SubRangeIndex::Type Z = SubRangeIndex::Z;
    static constexpr size_t SPLIT_FACTOR   = 2;
    static constexpr size_t MAX_SPLITS     = SPLIT_FACTOR - 1;
    static constexpr size_t DIMENSIONALITY = 3;

    using iter_type    = TIterType;
    using range_type   = Range1d<iter_type>;
    using split_result = SplitResult<KdRange3d, MAX_SPLITS, DIMENSIONALITY>;
    using size_type    = typename range_type::size_type;

    KdRange3d() = default;
    KdRange3d(KdRange3d const&) = default;

    //--------------------------------------------------------------------------
    ~KdRange3d()
    {
    }

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
     * @param zBegin
     *  An iterator to the beginning of the z-dimension range.
     * @param zEnd
     *  An iterator to the end of the z-dimension range.
     * @param zMinSize
     *  The smallest sub-range the z-dimension can be divided into. Must be >= splitOnMultiplesOf.
     * @param xSplitOnMultiplesOf
     *  Tries to ensure that each sub-range in the x-dimensions is a multiple
     *  of this value.
     * @param ySplitOnMultiplesOf
     *  Tries to ensure that each sub-range in the y-dimensions is a multiple
     *  of this value.
     * @param zSplitOnMultiplesOf
     *  Tries to ensure that each sub-range in the z-dimensions is a multiple
     *  of this value.
     */
    GTS_INLINE KdRange3d(
        iter_type xBegin, iter_type xEnd, size_type xMinSize,
        iter_type yBegin, iter_type yEnd, size_type yMinSize,
        iter_type zBegin, iter_type zEnd, size_type zMinSize,
        size_type xSplitOnMultiplesOf = 1,
        size_type ySplitOnMultiplesOf = 1,
        size_type zSplitOnMultiplesOf = 1)
        : m_subRanges{{xBegin, xEnd, xMinSize, xSplitOnMultiplesOf},
            {yBegin, yEnd, yMinSize, ySplitOnMultiplesOf},
            {zBegin, zEnd, zMinSize, zSplitOnMultiplesOf}}
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
    GTS_INLINE range_type const& zRange() const
    {
        return m_subRanges[Z];
    }

    //--------------------------------------------------------------------------
    GTS_INLINE range_type& zRange()
    {
        return m_subRanges[Z];
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
    template<typename TSplitter>
    GTS_INLINE void split(split_result& result, TSplitter const& splitter)
    {
        result.size      = MAX_SPLITS;
        result.ranges[0] = *this;
        KdRange3d& r     = result.ranges[0];

        // If xRange is relatively smaller than yRange,
        if (m_subRanges[X].size() * m_subRanges[Y].minSize() < m_subRanges[Y].size() * m_subRanges[X].minSize())
        {
            // If yRange is relatively smaller than zRange,
            if (m_subRanges[Y].size() * m_subRanges[Z].minSize() < m_subRanges[Z].size() * m_subRanges[Y].minSize())
            {
                // Split zRange.
                r.m_subRanges[Z].begin() = range_type::splitHelper(m_subRanges[Z], splitter);
                r.m_subRanges[Z].end()   = m_subRanges[Z].end();
                m_subRanges[Z].end()     = r.m_subRanges[Z].begin();
            }
            else
            {
                // Otherwise, split yRange.
                r.m_subRanges[Y].begin() = range_type::splitHelper(m_subRanges[Y], splitter);
                r.m_subRanges[Y].end()   = m_subRanges[Y].end();
                m_subRanges[Y].end()     = r.m_subRanges[Y].begin();
            }
        }
        // Otherwise xRange is relatively bigger than yRange, so
        else
        {
            // If xRange is relatively smaller than zRange,
            if (m_subRanges[X].size()*m_subRanges[Z].minSize() < m_subRanges[Z].size()*m_subRanges[X].minSize())
            {
                // Split zRange.
                r.m_subRanges[Z].begin() = range_type::splitHelper(m_subRanges[Z], splitter);
                r.m_subRanges[Z].end()   = m_subRanges[Z].end();
                m_subRanges[Z].end()     = r.m_subRanges[Z].begin();
            }
            else
            {
                // Otherwise, split xRange.
                r.m_subRanges[X].begin() = range_type::splitHelper(m_subRanges[X], splitter);
                r.m_subRanges[X].end()   = m_subRanges[X].end();
                m_subRanges[X].end()     = r.m_subRanges[X].begin();
            }
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
        SS_NONE, SS_X, SS_Y, SS_Z
    };

    range_type m_subRanges[DIMENSIONALITY];
    SplitState m_wasSplitOn = SS_NONE;
};

/** @} */ // end of ParallelPatterns
/** @} */ // end of MicroScheduler

} // namespace gts
