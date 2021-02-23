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

#include <algorithm>

#include "gts/containers/parallel/QueueMPSC.h"

#include "testers/QueueTester.h"

 // TODO: verify construction/destruction counts.

namespace testing {

static QueueTester<gts::QueueMPSC<uint32_t>> tester;

//------------------------------------------------------------------------------
TEST(QueueMPSC, emptyConstructor)
{
    tester.emptyConstructor();
}

//------------------------------------------------------------------------------
TEST(QueueMPSC, copyConstructor)
{
    tester.copyConstructor();
}

//------------------------------------------------------------------------------
TEST(QueueMPSC, moveConstructor)
{
    tester.moveConstructor();
}

//------------------------------------------------------------------------------
TEST(QueueMPSC, copyAssign)
{
    tester.copyAssign();
}

//------------------------------------------------------------------------------
TEST(QueueMPSC, moveAssign)
{
    tester.moveAssign();
}

//------------------------------------------------------------------------------
TEST(QueueMPSC, clear)
{
    tester.clear();
}


//------------------------------------------------------------------------------
TEST(QueueMPSC, copyPushPop)
{
    tester.copyPushPop();
}

//------------------------------------------------------------------------------
TEST(QueueMPSC, movePushPop)
{
    tester.movePushPop();
}

//------------------------------------------------------------------------------
TEST(QueueMPSC, pushRace)
{
    for(uint32_t ii = 0; ii < PARALLEL_ITERATIONS; ++ii)
    {
        tester.pushRace(gts::Thread::getHardwareThreadCount());
    }
}

//------------------------------------------------------------------------------
TEST(QueueMPSC, pushPopRace)
{
    uint32_t producerCount = std::max(1u, gts::Thread::getHardwareThreadCount() - 1);
    uint32_t consumerCount = 1;
    for(uint32_t ii = 0; ii < PARALLEL_ITERATIONS; ++ii)
    {
        tester.pushPopRace(producerCount, consumerCount);
    }
}

} // namespace testing
