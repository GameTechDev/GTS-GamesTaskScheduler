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
#include "gts/analysis/Instrumenter.h"
#include "gts/analysis/ConcurrentLogger.h"

#include "gts/micro_scheduler/WorkerPool.h"
#include "gts/micro_scheduler/MicroScheduler.h"

#include "SchedulerTestsCommon.h"

using namespace gts;

namespace testing {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// AFFINITY TESTS:

//------------------------------------------------------------------------------
Task* AffinityTestTask(Task* thisTask, TaskContext const& ctx)
{
    uint32_t affinityIdx = thisTask->getAffinity();
    uint32_t workerIdx = ctx.workerIndex;
    GTS_ASSERT(affinityIdx == workerIdx);
    return nullptr;
}

//------------------------------------------------------------------------------
Task* RootAffinityTestTask(Task* thisTask, TaskContext const& ctx)
{
    thisTask->addRef(ctx.pMicroScheduler->workerCount());

    for (uint32_t ii = 0; ii < ctx.pMicroScheduler->workerCount(); ++ii)
    {
        Task* pChildTask = ctx.pMicroScheduler->allocateTask(AffinityTestTask);
        pChildTask->setAffinity(ii);

        thisTask->addChildTask(pChildTask);

        ctx.pMicroScheduler->spawnTask(pChildTask);
    }

    thisTask->waitForChildren(ctx);

    return nullptr;
}

//------------------------------------------------------------------------------
void AffinityTest(uint32_t threadCount)
{
    WorkerPool workerPool;
    workerPool.initialize(threadCount);

    MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    Task* pRoot = taskScheduler.allocateTask(RootAffinityTestTask);
    taskScheduler.spawnTaskAndWait(pRoot);

    taskScheduler.shutdown();
}

//------------------------------------------------------------------------------
TEST(MicroScheduler, affinityTestSingleThreaded)
{
    for (uint32_t ii = 0; ii < ITERATIONS; ++ii)
    {
        GTS_CONCRT_LOGGER_RESET();
        AffinityTest(1);
    }
}

//------------------------------------------------------------------------------
TEST(MicroScheduler, affinityTestMultiThreaded)
{
    for (uint32_t ii = 0; ii < ITERATIONS; ++ii)
    {
        GTS_CONCRT_LOGGER_RESET();
        AffinityTest(gts::Thread::getHardwareThreadCount());
    }
}

} // namespace testing
