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

#include "gts/platform/Machine.h"

namespace gts {

/** 
 * @addtogroup MicroScheduler
 * @{
 */

/** 
 * @addtogroup ParallelPatterns
 * @{
 */

using range_size_t = uint8_t;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  An enumeration of sub range indices.
 */
namespace SubRangeIndex
{
    enum Type : uint32_t
    {
        X = 0,
        Y = 1,
        Z = 2
    };
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  The resulting Ranges of a Range split.
 */
template<typename TRange, uint32_t MaxSplits, uint32_t Dimesionality>
struct SplitResult;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
* @brief
*  The resulting Ranges of a 1D Range split.
*/
template<typename TRange, uint32_t MaxSplits>
struct SplitResult<TRange, MaxSplits, 1>
{
    //! The dimensionality of the Ranges.
    static constexpr size_t DIMENSIONALITY = 1;

    //! The capacity of 'ranges'.
    static constexpr size_t MAX_RANGES = MaxSplits;

    //! An array of resulting Ranges.
    TRange ranges[MAX_RANGES];

    // The number of valid ranges in 'ranges'.
    range_size_t size = 0;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  The resulting Ranges of a 2D Range split.
 */
template<typename TRange, uint32_t MaxSplits>
struct SplitResult<TRange, MaxSplits, 2>
{
    //! The dimensionality of the Ranges.
    static constexpr size_t DIMENSIONALITY = 2;

    //! The capacity of 'ranges'.
    static constexpr size_t MAX_RANGES = MaxSplits;

    //! An array of resulting Ranges.
    TRange ranges[MAX_RANGES];

    // The number of valid ranges in 'ranges'.
    range_size_t size = 0;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  The resulting Ranges of a 3D Range split.
 */
template<typename TRange, uint32_t MaxSplits>
struct SplitResult<TRange, MaxSplits, 3>
{
    //! The dimensionality of the Ranges.
    static constexpr size_t DIMENSIONALITY = 3;

    //! The capacity of 'ranges'.
    static constexpr size_t MAX_RANGES = MaxSplits;

    //! An array of resulting Ranges.
    TRange ranges[MAX_RANGES];

    // The number of valid ranges in 'ranges'.
    range_size_t size = 0;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  Indicates that a Range is to be split in half.
 */
struct EvenSplitter {};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  Indicates that a Range is to be split in proportionally.
 */
struct ProportionalSplitter
{
    GTS_INLINE ProportionalSplitter(uint32_t left, uint32_t right)
        : left(left), right(right)
    {}

    uint32_t left  = 0;
    uint32_t right = 0;
};

/** @} */ // end of ParallelPatterns
/** @} */ // end of MicroScheduler

} // namespace gts
