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
#include <gts/platform/Machine.h>

#if GTS_WINDOWS

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdlib>

#include <TestDll.h>

#include <gts/micro_scheduler/WorkerPool.h>

using namespace gts;

thread_local uintptr_t tl_state;

//------------------------------------------------------------------------------
uintptr_t GetThreadState()
{
    return tl_state;
}

//------------------------------------------------------------------------------
void SetThreadState(uintptr_t state)
{
    tl_state = state;
}

namespace testing {

//------------------------------------------------------------------------------
TEST(MicroScheduler, testCrossDllThreadIdx)
{
    WorkerPoolDesc desc;
    desc.workerDescs.resize(1);
    desc.pGetThreadLocalStateFcn = GetThreadState;
    desc.pSetThreadLocalStateFcn = SetThreadState;

    WorkerPool workerPool;
    workerPool.initialize(desc);

    MicroScheduler scheduler;
    scheduler.initialize(&workerPool);

    ASSERT_TRUE(gotCorrectTid(&scheduler));
}

} // namespace testing

#endif
