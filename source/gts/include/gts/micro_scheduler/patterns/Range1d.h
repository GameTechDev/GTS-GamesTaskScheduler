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

#include "gts/platform/Utils.h"
#include "gts/platform/Machine.h"
#include "gts/platform/Assert.h"
#include "gts/micro_scheduler/patterns/RangeSplitters.h"

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
 *  An iteration range over a 1D data set. Splits divide the range in two based
 *  unless the minimum size is reached. Splits are determined by a splitter
 *  object. Successive splits result in a binary tree where each leaf represents
 *  a unit of work.
 * @details
 *  Derivative of TBB blocked_range. https://github.com/intel/tbb
 */
template<typename Iter>
class Range1d
{
public: // CREATORS

    static constexpr size_t SPLIT_FACTOR   = 2;
    static constexpr size_t MAX_SPLITS     = SPLIT_FACTOR - 1;
    static constexpr size_t DIMENSIONALITY = 1;

    using iter_type    = Iter;
    using split_result = SplitResult<Range1d, MAX_SPLITS, DIMENSIONALITY>;
    using size_type    = size_t;

    Range1d() = default;
    Range1d(Range1d const&) = default;

    //--------------------------------------------------------------------------
    /**
     * @brief
     *  Constructs a Range1d.
     * @param xBegin
     *  An iterator to the beginning of the range.
     * @param xEnd
     *  An iterator to the xEnd of the range.
     * @param minSize
     *  The smallest sub-range the range can be divided into. Must be >= splitOnMultiplesOf.
     * @param splitOnMultiplesOf
     *  Tries to ensure that each sub-range is a multiple of this value. Useful
     *  for things like iterating items to be packed into SIMD vectors.
     */
    GTS_INLINE Range1d(iter_type xBegin, iter_type xEnd, size_type minSize, size_type splitOnMultiplesOf = 1)
        : m_origin(xBegin)
        , m_begin(xBegin)
        , m_end(xEnd)
        , m_initialSize(size())
        , m_minSize(minSize)
        , m_splitOnMultiplesOf(splitOnMultiplesOf)
    {
        GTS_ASSERT(_distance(m_begin, m_end) > 0);
        GTS_ASSERT(m_minSize > 0);
        GTS_ASSERT(minSize >= splitOnMultiplesOf);
        GTS_ASSERT(isPow2(splitOnMultiplesOf));
    }

public: // ACCESSORS

    //--------------------------------------------------------------------------
    GTS_INLINE iter_type begin() const
    {
        return m_begin;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE iter_type end() const
    {
        return m_end;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE iter_type& begin()
    {
        return m_begin;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE iter_type& end()
    {
        return m_end;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE bool empty() const
    {
        return size() == 0;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE size_type size() const
    {
        return _distance(m_begin, m_end);
    }

    //--------------------------------------------------------------------------
    GTS_INLINE size_type initialSize() const
    {
        return m_initialSize;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE void resetInitialSize(size_type initSize)
    {
        m_initialSize = initSize;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE size_type minSize() const
    {
        return m_minSize;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE size_type splitOnMultiplesOf() const
    {
        return m_splitOnMultiplesOf;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE bool isDivisible() const
    {
        return size() > m_minSize;
    }

    //--------------------------------------------------------------------------
    /**
     * @brief
     *  Divides the range based on the splitter.
     * @param result
     *  The result of the split.
     * @param splitter
     *  Defines the split behavior.
     */
    template<typename TSplitter>
    GTS_INLINE void split(split_result& result, TSplitter const& splitter)
    {
        result.size      = MAX_SPLITS;
        result.ranges[0] = *this;
        Range1d& r       = result.ranges[0];

        iter_type middle = splitHelper(*this, splitter);

        r.m_begin = middle;
        r.m_end   = m_end;
        m_end     = middle;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE static iter_type splitHelper(Range1d& range, EvenSplitter const&)
    {
        GTS_ASSERT(range.isDivisible());

        // Calculate the middle.
        iter_type middle = (iter_type)(range.m_begin + (range.size() >> 1));
        // Adjust base of the specified multiple.
        middle = (iter_type)(middle + _calOffset(_distance(range.m_origin, middle), range.m_splitOnMultiplesOf));

        return middle;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE static iter_type splitHelper(Range1d& range, ProportionalSplitter const& proportions)
    {
        GTS_ASSERT(range.isDivisible());
        GTS_ASSERT(proportions.left > 0 && proportions.right > 0);

        // Calculate the right size based on the proportions.
        size_type rightSize = size_type(float(range.size()) * float(proportions.right)
            / float(proportions.left + proportions.right) + 0.5f);

        // Calculate the middle.
        iter_type middle = (iter_type)(range.m_end - rightSize);
        // Adjust base of the specified multiple.
        middle = (iter_type)(middle + _calOffset(_distance(range.m_origin, middle), range.m_splitOnMultiplesOf));

        return middle;
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

    //--------------------------------------------------------------------------
    template<typename T>
    GTS_INLINE static size_type _calOffset(T value, size_type boundary)
    {
        size_type offset = value & (boundary - 1);
        if (offset == 0)
        {
            return 0;
        }
        return boundary - offset;
    }

    //--------------------------------------------------------------------------
    template<
        typename T,
        typename std::enable_if<std::is_integral<T>::value, T>::type = 0
    >
        GTS_INLINE static typename std::make_signed<T>::type _distance(T begin, T end)
    {
        return end - begin;
    }

    //--------------------------------------------------------------------------
    template<
        typename T,
        typename std::enable_if<std::is_pointer<T>::value, T>::type = 0
    >
        GTS_INLINE static ptrdiff_t _distance(T begin, T end)
    {
        return end - begin;
    }

    //--------------------------------------------------------------------------
    template<typename iter_type>
    GTS_INLINE static typename std::iterator_traits<iter_type>::difference_type
        _distance(iter_type begin, iter_type end)
    {
        return std::distance(begin, end);
    }

    iter_type m_origin;
    iter_type m_begin;
    iter_type m_end;
    size_type m_initialSize;
    size_type m_minSize;
    size_type m_splitOnMultiplesOf;
};
/** @} */ // xEnd of ParallelPatterns
/** @} */ // xEnd of MicroScheduler

} // namespace gts

