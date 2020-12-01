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

#include "gts/containers/parallel/QueueSPMC.h"

#include "testers/QueueTester.h"

 // TODO: verify construction/destruction counts.

namespace testing {

static QueueTester<gts::QueueSPMC<uint32_t>> tester;

//------------------------------------------------------------------------------
TEST(QueueSPMC, emptyConstructor)
{
    tester.emptyConstructor();
}

//------------------------------------------------------------------------------
TEST(QueueSPMC, copyConstructor)
{
    tester.copyConstructor();
}

//------------------------------------------------------------------------------
TEST(QueueSPMC, moveConstructor)
{
    tester.moveConstructor();
}

//------------------------------------------------------------------------------
TEST(QueueSPMC, copyAssign)
{
    tester.copyAssign();
}

//------------------------------------------------------------------------------
TEST(QueueSPMC, moveAssign)
{
    tester.moveAssign();
}

//------------------------------------------------------------------------------
TEST(QueueSPMC, copyPushPop)
{
    tester.copyPushPop();
}

//------------------------------------------------------------------------------
TEST(QueueSPMC, movePushPop)
{
    tester.movePushPop();
}

//------------------------------------------------------------------------------
TEST(QueueSPMC, popRace)
{
    for(uint32_t ii = 0; ii < PARALLEL_ITERATIONS; ++ii)
    {
        tester.popRace(gts::Thread::getHardwareThreadCount());
    }
}

//------------------------------------------------------------------------------
TEST(QueueSPMC, pushPopRace)
{
    uint32_t producerCount = 1;
    uint32_t consumerCount = std::max(1u, gts::Thread::getHardwareThreadCount() - 1);
    for(uint32_t ii = 0; ii < PARALLEL_ITERATIONS; ++ii)
    {
        tester.pushPopRace(producerCount, consumerCount);
    }
}

} // namespace testing
