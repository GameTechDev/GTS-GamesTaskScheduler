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
#include "gts/platform/Machine.h"

#include <chrono>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "gts/containers/AlignedAllocator.h"

namespace testing {

#pragma warning( push )
#pragma warning( disable : 4324) // alignment padding warning

class GTS_ALIGN(GTS_CACHE_LINE_SIZE) AlignedType
{
    enum {SIZE = 5};

public:

    AlignedType(int val)
    {
        for (int ii = 0; ii < SIZE; ++ii)
        {
            data[ii] = val;
        }
    }

    bool operator==(int val)
    {
        for (int ii = 0; ii < SIZE; ++ii)
        {
            if (data[ii] != val)
            {
                return false;
            }
        }
        return true;
    }

private:

    int data[SIZE];
};

#pragma warning( pop )

//------------------------------------------------------------------------------
TEST(AlignedAllocator, vectorPOD)
{
    std::vector<uint32_t, gts::AlignedAllocator<uint32_t, GTS_CACHE_LINE_SIZE>> alignedVec;

    for (uint32_t ii = 0; ii < 100; ++ii)
    {
        alignedVec.push_back(ii);
    }

    for (uint32_t ii = 0; ii < 100; ++ii)
    {
        ASSERT_EQ(alignedVec[ii], ii);
    }
}

//------------------------------------------------------------------------------
TEST(AlignedAllocator, vectorAlignedType)
{
    std::vector<AlignedType, gts::AlignedAllocator<AlignedType, GTS_CACHE_LINE_SIZE>> alignedVec;

    for (uint32_t ii = 0; ii < 100; ++ii)
    {
        alignedVec.push_back(AlignedType(ii));
    }

    for (uint32_t ii = 0; ii < 100; ++ii)
    {
        ASSERT_TRUE(alignedVec[ii] == ii);
    }
}

} // namespace testing
