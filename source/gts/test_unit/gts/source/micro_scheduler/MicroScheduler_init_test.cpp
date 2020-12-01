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

#include <cstdlib>

#include "gts/platform/Atomic.h"
#include "gts/analysis/Trace.h"

#include "gts/micro_scheduler/WorkerPool.h"
#include "gts/micro_scheduler/MicroScheduler.h"

using namespace gts;

namespace testing {

//------------------------------------------------------------------------------
TEST(MicroScheduler, Constructor)
{
    MicroScheduler taskScheduler;
}

//------------------------------------------------------------------------------
TEST(MicroScheduler, initializeSimpleOneThread)
{
    WorkerPool workerPool;
    workerPool.initialize(1);

    MicroScheduler taskScheduler;
    bool result = taskScheduler.initialize(&workerPool);
    ASSERT_TRUE(result);
    taskScheduler.shutdown();
}

//------------------------------------------------------------------------------
TEST(MicroScheduler, initializeSimpleDefault)
{
    WorkerPool workerPool;
    workerPool.initialize();

    MicroScheduler taskScheduler;
    bool result = taskScheduler.initialize(&workerPool);
    ASSERT_TRUE(result);
    taskScheduler.shutdown();
}

//------------------------------------------------------------------------------
TEST(MicroScheduler, initializePrecacheTasks)
{
    WorkerPoolDesc desc;
    desc.workerDescs.resize(Thread::getHardwareThreadCount());
    desc.initialTaskCountPerWorker = 1000;

    WorkerPool workerPool;
    workerPool.initialize(desc);

    MicroScheduler taskScheduler;
    bool result = taskScheduler.initialize(&workerPool);
    ASSERT_TRUE(result);
    taskScheduler.shutdown();
}

} // namespace testing
