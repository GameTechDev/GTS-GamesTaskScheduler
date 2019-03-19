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
#include <chrono>
#include <deque>

#include "micro_scheduler/WorkStealingDeque.h"
#include "testers/WorkerStealingDequeTester.h"

namespace testing {

WorkerStealingDequeTester<gts::WorkStealingDeque<>> spinDequeTester;

//------------------------------------------------------------------------------
TEST(WorkStealingDeque, emptyConstructor)
{
    gts::WorkStealingDeque<> deque;

    ASSERT_EQ(deque.empty(), true);
    ASSERT_EQ(deque.size(), 0u);
    ASSERT_EQ(deque.capacity(), 1u);
}

//------------------------------------------------------------------------------
TEST(WorkStealingDeque, pushPopCopy)
{
    gts::WorkStealingDeque<> deque;
    spinDequeTester.pushPopCopy(deque);
}

//------------------------------------------------------------------------------
TEST(WorkStealingDeque, pushStealCopy)
{
    gts::WorkStealingDeque<> deque;
    spinDequeTester.pushStealCopy(deque);
}

//------------------------------------------------------------------------------
TEST(WorkStealingDeque, stealRace)
{
    gts::WorkStealingDeque<> deque;
    spinDequeTester.stealRace(deque);
}

//------------------------------------------------------------------------------
TEST(WorkStealingDeque, pushStealRace_2Threads)
{
    gts::WorkStealingDeque<> deque;
    spinDequeTester.pushStealRace_2Threads(deque);
}

//------------------------------------------------------------------------------
TEST(WorkStealingDeque, pushStealRace_ManyThreads)
{
    gts::WorkStealingDeque<> deque;
    spinDequeTester.pushStealRace_ManyThreads(deque);
}

//------------------------------------------------------------------------------
TEST(WorkStealingDeque, pushStealPopRace_ManyThreads)
{
    gts::WorkStealingDeque<> deque;
    spinDequeTester.pushStealPopRace_ManyThreads(deque);
}

} // namespace testing
