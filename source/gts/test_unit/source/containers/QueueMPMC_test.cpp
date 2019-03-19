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
#include <deque>

#include "gts/containers/parallel/QueueMPMC.h"

#include "testers/QueueMPMCTester.h"

namespace testing {

QueueMPMCTester<gts::QueueMPMC<uint32_t>> queueOfInt;

//------------------------------------------------------------------------------
TEST(QueueMPMC, emptyConstructor)
{
    queueOfInt.emptyConstructor();
}

//------------------------------------------------------------------------------
TEST(QueueMPMC, pushPopCopy)
{
    queueOfInt.pushPopCopy();
}

//------------------------------------------------------------------------------
TEST(QueueMPMC, pushPopMove)
{
    queueOfInt.pushPopMove();
}

//------------------------------------------------------------------------------
TEST(QueueMPMC, pushRace)
{
    queueOfInt.pushRace();
}

//------------------------------------------------------------------------------
TEST(QueueMPMC, pushPopRace_2Threads)
{
    queueOfInt.pushPopRace_2Threads();
}

//------------------------------------------------------------------------------
TEST(QueueMPMC, pushPopRace_ManyThreads)
{
    queueOfInt.pushPopRace_ManyThreads();
}

//------------------------------------------------------------------------------
TEST(QueueMPMC, pushPopRaceOnFull_ManyThreads)
{
    queueOfInt.pushPopRaceOnFull_ManyThreads();
}

} // namespace testing
