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
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <queue>
#include <vector>

#include "gts/micro_scheduler/patterns/Range1d.h"

using namespace gts;

namespace testing {

//------------------------------------------------------------------------------
TEST(Range1d, construct)
{
    const uint32_t begin = 0;
    const uint32_t end = 10;
    const size_t minSize = 1;

    Range1d<uint32_t> range1(begin, end, minSize);

    ASSERT_EQ(range1.begin(), begin);
    ASSERT_EQ(range1.end(), end);
    ASSERT_EQ(range1.minSize(), minSize);
    ASSERT_EQ(range1.splitOnMultiplesOf(), size_t(1));

    const size_t splitOnMultiplesOf = 4;

    Range1d<uint32_t> range2(begin, end, splitOnMultiplesOf, splitOnMultiplesOf);

    ASSERT_EQ(range2.begin(), begin);
    ASSERT_EQ(range2.end(), end);
    ASSERT_EQ(range2.minSize(), splitOnMultiplesOf);
    ASSERT_EQ(range2.splitOnMultiplesOf(), splitOnMultiplesOf);
}

//------------------------------------------------------------------------------
TEST(Range1d, constructFail)
{
    EXPECT_DEATH({

        Range1d<uint32_t> range(1, 1, 1);

    }, "");

    EXPECT_DEATH({

        Range1d<uint32_t> range(2, 1, 1);

    }, "");

    EXPECT_DEATH({

        Range1d<uint32_t> range(0, 2, 0);

    }, "");

    EXPECT_DEATH({

        Range1d<uint32_t> range(0, 2, 1, 2);

    }, "");
}

//------------------------------------------------------------------------------
TEST(Range1d, evenSplit)
{
    const uint32_t begin = 0;
    const uint32_t end = 10;
    const size_t minSize = 1;

    Range1d<uint32_t> range1(begin, end, minSize);
    Range1d<uint32_t>::split_result result;
    range1.split(result, EvenSplitter());

    ASSERT_EQ(result.size, 1);
    Range1d<uint32_t> range2 = result.ranges[0];

    ASSERT_EQ(range1.begin(), begin);
    ASSERT_EQ(range1.end(), (begin + end) / 2);

    ASSERT_EQ(range2.begin(), (begin + end) / 2);
    ASSERT_EQ(range2.end(), end);
}

//------------------------------------------------------------------------------
TEST(Range1d, proportionalSplit)
{
    const uint32_t begin = 0;
    const uint32_t end = 10;
    const size_t minSize = 1;

    Range1d<uint32_t> range1(begin, end, minSize);
    Range1d<uint32_t>::split_result result;
    range1.split(result, ProportionalSplitter(3, 7));

    ASSERT_EQ(result.size, 1);
    Range1d<uint32_t> range2 = result.ranges[0];

    ASSERT_EQ(range1.begin(), begin);
    ASSERT_EQ(range1.end(), 3u);

    ASSERT_EQ(range2.begin(), 3u);
    ASSERT_EQ(range2.end(), 10u);
}

//------------------------------------------------------------------------------
TEST(Range1d, isDivisible)
{
    const uint32_t begin = 0;
    const uint32_t end = 10;
    const size_t minSize = 5;

    Range1d<uint32_t> range1(begin, end, minSize);
    ASSERT_EQ(range1.isDivisible(), true);

    Range1d<uint32_t>::split_result result;
    range1.split(result, EvenSplitter());
    Range1d<uint32_t> range2 = result.ranges[0];

    ASSERT_EQ(range1.isDivisible(), false);
    ASSERT_EQ(range2.isDivisible(), false);
}

//------------------------------------------------------------------------------
TEST(Range1d, splitOnMultiplesOfEvenly)
{
    const uint32_t begin = 0;
    const uint32_t end = 10;
    const size_t minSize = 2;
    const size_t splitOnMultiplesOf = 2;

    std::queue<Range1d<uint32_t>> toSplitQueue;
    std::vector<Range1d<uint32_t>> leafRanges;

    Range1d<uint32_t> range(begin, end, minSize, splitOnMultiplesOf);
    toSplitQueue.push(range);

    while (!toSplitQueue.empty())
    {
        Range1d<uint32_t>& currRange = toSplitQueue.front();

        while(currRange.isDivisible())
        {
            Range1d<uint32_t>::split_result result;
            currRange.split(result, EvenSplitter());
            Range1d<uint32_t> newSplit = result.ranges[0];

            if (newSplit.isDivisible())
            {
                toSplitQueue.push(newSplit);
            }
            else
            {
                leafRanges.push_back(newSplit);
            }
        }

        leafRanges.push_back(currRange);
        toSplitQueue.pop();
    }

    for (auto& r : leafRanges)
    {
        ASSERT_EQ(r.size(), splitOnMultiplesOf);
    }
}

//------------------------------------------------------------------------------
TEST(Range1d, splitOnMultiplesOfUnevenly)
{
    const uint32_t begin = 0;
    const uint32_t end = 11;
    const size_t minSize = 2;
    const size_t splitOnMultiplesOf = 2;

    std::queue<Range1d<uint32_t>> toSplitQueue;
    std::vector<Range1d<uint32_t>> leafRanges;

    Range1d<uint32_t> range(begin, end, minSize, splitOnMultiplesOf);
    toSplitQueue.push(range);

    while (!toSplitQueue.empty())
    {
        Range1d<uint32_t>& currRange = toSplitQueue.front();

        while(currRange.isDivisible())
        {
            Range1d<uint32_t>::split_result result;
            currRange.split(result, EvenSplitter());
            Range1d<uint32_t> newSplit = result.ranges[0];

            if (newSplit.isDivisible())
            {
                toSplitQueue.push(newSplit);
            }
            else
            {
                leafRanges.push_back(newSplit);
            }
        }

        leafRanges.push_back(currRange);
        toSplitQueue.pop();
    }

    int32_t oddLeafCount = 0;

    for (auto& r : leafRanges)
    {
        if (r.size() != splitOnMultiplesOf)
        {
            ++oddLeafCount;
        }
    }

    ASSERT_EQ(oddLeafCount, 1);
}

} // namespace testing
