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
 *  An iteration range over a 3D data set. Splits divide each dimension in half
 *  for each dimension that is divisible. Successive splits result in a oct-tree
 *  where each leaf represents a unit of work.
 *
 * @todo Add get neighbor functions.
 */
template<typename TIterType>
class OctRange
{
public: // CREATORS

    static constexpr SubRangeIndex::Type X = SubRangeIndex::X;
    static constexpr SubRangeIndex::Type Y = SubRangeIndex::Y;
    static constexpr SubRangeIndex::Type Z = SubRangeIndex::Z;
    static constexpr size_t SPLIT_FACTOR   = 8;
    static constexpr size_t MAX_SPLITS     = SPLIT_FACTOR - 1;
    static constexpr size_t DIMENSIONALITY = 3;

    using iter_type    = TIterType;
    using range_type   = Range1d<iter_type>;
    using split_result = SplitResult<OctRange, MAX_SPLITS, DIMENSIONALITY>;
    using size_type    = typename range_type::size_type;

    OctRange() = default;
    OctRange(OctRange const&) = default;

    //--------------------------------------------------------------------------
    ~OctRange()
    {
    }

    //--------------------------------------------------------------------------
    /**
     * @brief
     *  Constructs a OctRange.
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
    GTS_INLINE OctRange(
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
    /**
     * @brief
     *  Splits the range into eight possible subranges.
     * @param result
     *  The returned results from the split.
     */
    template<typename TSplitter>
    GTS_INLINE void split(split_result& result, TSplitter const&)
    {
        bool xIsDivisible = m_subRanges[X].isDivisible();
        bool yIsDivisible = m_subRanges[Y].isDivisible();
        bool zIsDivisible = m_subRanges[Z].isDivisible();

        result.size = 1;
        if(xIsDivisible) result.size *= 2;
        if(yIsDivisible) result.size *= 2;
        if(zIsDivisible) result.size *= 2;
        result.size -= 1;

        for (int ii = 0; ii < result.size; ++ii)
        {
            result.ranges[ii] = *this;
        }

        if (xIsDivisible && yIsDivisible && zIsDivisible)
        {
            iter_type xMiddle = range_type::splitHelper(m_subRanges[X], EvenSplitter());
            iter_type yMiddle = range_type::splitHelper(m_subRanges[Y], EvenSplitter());
            iter_type zMiddle = range_type::splitHelper(m_subRanges[Z], EvenSplitter());

            // 1st z-plane

            m_subRanges[X].end() = xMiddle;
            m_subRanges[Y].end() = yMiddle;
            m_subRanges[Z].end() = zMiddle;

            result.ranges[0].m_subRanges[X].begin() = xMiddle;
            result.ranges[0].m_subRanges[Y].end() = yMiddle;
            result.ranges[0].m_subRanges[Z].end() = zMiddle;

            result.ranges[1].m_subRanges[X].end() = xMiddle;
            result.ranges[1].m_subRanges[Y].begin() = yMiddle;
            result.ranges[1].m_subRanges[Z].end() = zMiddle;

            result.ranges[2].m_subRanges[X].begin() = xMiddle;
            result.ranges[2].m_subRanges[Y].begin() = yMiddle;
            result.ranges[2].m_subRanges[Z].end() = zMiddle;

            // 2nd z-plane

            result.ranges[3].m_subRanges[X].end()   = xMiddle;
            result.ranges[3].m_subRanges[Y].end()   = yMiddle;
            result.ranges[3].m_subRanges[Z].begin() = zMiddle;

            result.ranges[4].m_subRanges[X].begin() = xMiddle;
            result.ranges[4].m_subRanges[Y].end()   = yMiddle;
            result.ranges[4].m_subRanges[Z].begin() = zMiddle;

            result.ranges[5].m_subRanges[X].end()   = xMiddle;
            result.ranges[5].m_subRanges[Y].begin() = yMiddle;
            result.ranges[5].m_subRanges[Z].begin() = zMiddle;

            result.ranges[6].m_subRanges[X].begin() = xMiddle;
            result.ranges[6].m_subRanges[Y].begin() = yMiddle;
            result.ranges[6].m_subRanges[Z].begin() = zMiddle;
        }
        else if (xIsDivisible)
        {
            iter_type xMiddle = range_type::splitHelper(m_subRanges[X], EvenSplitter());

            if (yIsDivisible)
            {
                iter_type yMiddle = range_type::splitHelper(m_subRanges[Y], EvenSplitter());

                m_subRanges[X].end() = xMiddle;
                m_subRanges[Y].end() = yMiddle;

                result.ranges[0].m_subRanges[X].begin() = xMiddle;
                result.ranges[0].m_subRanges[Y].end()   = yMiddle;

                result.ranges[1].m_subRanges[X].end()   = xMiddle;
                result.ranges[1].m_subRanges[Y].begin() = yMiddle;

                result.ranges[2].m_subRanges[X].begin() = xMiddle;
                result.ranges[2].m_subRanges[Y].begin() = yMiddle;
            }
            else if (zIsDivisible)
            {
                iter_type zMiddle = range_type::splitHelper(m_subRanges[Z], EvenSplitter());

                m_subRanges[X].end() = xMiddle;
                m_subRanges[Z].end() = zMiddle;

                result.ranges[0].m_subRanges[X].begin() = xMiddle;
                result.ranges[0].m_subRanges[Z].end()   = zMiddle;

                result.ranges[1].m_subRanges[X].end()   = xMiddle;
                result.ranges[1].m_subRanges[Z].begin() = zMiddle;

                result.ranges[2].m_subRanges[X].begin() = xMiddle;
                result.ranges[2].m_subRanges[Z].begin() = zMiddle;
            }
            else
            {
                m_subRanges[X].end() = xMiddle;
                result.ranges[0].m_subRanges[X].begin() = xMiddle;
            }
        }
        else if (yIsDivisible)
        {
            iter_type yMiddle = range_type::splitHelper(m_subRanges[Y], EvenSplitter());

            if (zIsDivisible)
            {
                iter_type zMiddle = range_type::splitHelper(m_subRanges[Z], EvenSplitter());

                m_subRanges[Y].end() = yMiddle;
                m_subRanges[Z].end() = zMiddle;

                result.ranges[0].m_subRanges[Y].begin() = yMiddle;
                result.ranges[0].m_subRanges[Z].end()   = zMiddle;

                result.ranges[1].m_subRanges[Y].end()   = yMiddle;
                result.ranges[1].m_subRanges[Z].begin() = zMiddle;

                result.ranges[2].m_subRanges[Y].begin() = yMiddle;
                result.ranges[2].m_subRanges[Z].begin() = zMiddle;
            }
            else
            {
                m_subRanges[Y].end() = yMiddle;
                result.ranges[0].m_subRanges[Y].begin() = yMiddle;
            }
        }
        else if (zIsDivisible)
        {
            iter_type zMiddle = range_type::splitHelper(m_subRanges[Z], EvenSplitter());

            m_subRanges[Z].end() = zMiddle;
            result.ranges[0].m_subRanges[Z].begin() = zMiddle;
        }
    }

    //--------------------------------------------------------------------------
    static GTS_INLINE uint16_t adjustDivisor(uint16_t initialSplitDepth, bool)
    {
        return initialSplitDepth;
    }

    //--------------------------------------------------------------------------
    static GTS_INLINE uint16_t splitInitialDepth(uint16_t initialSplitDepth, bool)
    {
        return initialSplitDepth / SPLIT_FACTOR;
    }


    //--------------------------------------------------------------------------
    static GTS_INLINE uint16_t finalSplitDivisor(uint16_t initialSplitDepth, bool)
    {
        return initialSplitDepth / SPLIT_FACTOR;
    }

private:

    range_type m_subRanges[DIMENSIONALITY];
};

/** @} */ // end of ParallelPatterns
/** @} */ // end of MicroScheduler

} // namespace gts
