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
#include <vector>

#include "gts/platform/Atomic.h"
#include "gts/analysis/Instrumenter.h"
#include "gts/analysis/ConcurrentLogger.h"

#include "gts/micro_scheduler/WorkerPool.h"
#include "gts/micro_scheduler/MicroScheduler.h"

#include "SchedulerTestsCommon.h"

using namespace gts;

namespace testing {

////////////////////////////////////////////////////////////////////////////////
struct PriorityGrabber
{
    //--------------------------------------------------------------------------
    static Task* taskFunc(Task* pThisTask, TaskContext const&)
    {
        PriorityGrabber* data = (PriorityGrabber*)pThisTask->getData();
        data->vec->push_back(data->taskIndex);
        return nullptr;
    }

    //--------------------------------------------------------------------------
    static Task* generatorFunc(Task* pThisTask, TaskContext const& ctx)
    {
        PriorityGrabber* data = (PriorityGrabber*)pThisTask->getData();

        pThisTask->addRef(data->numTasks);

        // Add tasks in reverse priority order
        for (int32_t ii = (int32_t)data->numTasks - 1; ii >= 0; --ii)
        {
            Task* pChildTask = ctx.pMicroScheduler->allocateTask(PriorityGrabber::taskFunc);

            PriorityGrabber taskData{ data->vec, 0, (uint32_t)ii };
            pChildTask->setData(taskData);

            pThisTask->addChildTaskWithoutRef(pChildTask);

            ctx.pMicroScheduler->spawnTask(pChildTask, ii);
        }

        pThisTask->waitForChildren(ctx);

        return nullptr;
    }

    std::vector<int>* vec;
    uint32_t numTasks;
    uint32_t taskIndex;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// PRIORITY ORDER TESTS:

// NOTE: there are no multithreaded priority tests here because, while the tasks can be
// de-queued in priority order, there is no guarantee how their execution will
// be interleaved.

//------------------------------------------------------------------------------
void PriorityOrderTest(uint32_t priorityCount, uint32_t threadCount)
{
    WorkerPool workerPool;
    workerPool.initialize(threadCount);

    MicroSchedulerDesc desc;
    desc.pWorkerPool = &workerPool;
    desc.priorityCount = priorityCount;

    MicroScheduler taskScheduler;
    taskScheduler.initialize(desc);

    std::vector<int> vec;

    Task* pRoot = taskScheduler.allocateTask(PriorityGrabber::generatorFunc);
    PriorityGrabber taskData{ &vec, (uint32_t)priorityCount };
    pRoot->setData(taskData);
    taskScheduler.spawnTaskAndWait(pRoot);

    // Verify that task were executed in priority order
    for (int32_t ii = (int32_t)priorityCount - 1; ii >= 0; --ii)
    {
        int result = vec.back();
        vec.pop_back();

        ASSERT_EQ(ii, result);
    }

    taskScheduler.shutdown();
}

//------------------------------------------------------------------------------
TEST(MicroScheduler, priorityOrderSingleThreaded)
{
    for (uint32_t ii = 0; ii < ITERATIONS; ++ii)
    {
        GTS_CONCRT_LOGGER_RESET();
        PriorityOrderTest(TEST_DEPTH, 1);
    }
}

//------------------------------------------------------------------------------
TEST(MicroScheduler, priorityOrderWithAffinitySingleThreaded)
{
    for (uint32_t ii = 0; ii < ITERATIONS; ++ii)
    {
        GTS_CONCRT_LOGGER_RESET();
        PriorityOrderTest(TEST_DEPTH, 1);
    }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// PRIORITY STARVATION TESTS:

struct PriorityStarvationGrabber
{
    //------------------------------------------------------------------------------
    static Task* taskFunc(Task* pThisTask, TaskContext const&)
    {
        PriorityStarvationGrabber* data = (PriorityStarvationGrabber*)pThisTask->getData();
        data->vec->push_back(data->taskIndex);
        return nullptr;
    }

    //------------------------------------------------------------------------------
    static Task* generatorFunc(Task* pThisTask, TaskContext const& ctx)
    {
        PriorityStarvationGrabber* data = (PriorityStarvationGrabber*)pThisTask->getData();

        pThisTask->addRef(data->numTasks);

        uint32_t nextPriority = 1;

        for (uint32_t ii = 0; ii < data->numTasks; ++ii)
        {
            Task* pChildTask = ctx.pMicroScheduler->allocateTask(PriorityStarvationGrabber::taskFunc);

            if (ii % data->boostAge == 0)
            {
                PriorityStarvationGrabber taskData{ data->vec, 0, 0, nextPriority, 0 };
                pChildTask->setData(taskData);

                pThisTask->addChildTaskWithoutRef(pChildTask);

                ctx.pMicroScheduler->spawnTask(pChildTask, nextPriority);

                nextPriority = nextPriority % (data->maxPriority - 1) + 1; // [1, data->maxPriority]
            }
            else
            {
                PriorityStarvationGrabber taskData{ data->vec, 0, 0, 0, 0 };
                pChildTask->setData(taskData);

                pThisTask->addChildTaskWithoutRef(pChildTask);

                ctx.pMicroScheduler->spawnTask(pChildTask, 0);
            }
        }

        pThisTask->waitForChildren(ctx);

        return nullptr;
    }

    std::vector<int>* vec;
    uint32_t numTasks;
    uint32_t boostAge;
    uint32_t taskIndex;
    uint32_t maxPriority;
};

//------------------------------------------------------------------------------
void PriorityOrderTest()
{    WorkerPool workerPool;
    workerPool.initialize(1);

    MicroSchedulerDesc desc;
    desc.pWorkerPool = &workerPool;
    desc.priorityCount = 3;
    desc.priorityBoostAge = (int16_t)desc.priorityCount;

    MicroScheduler taskScheduler;
    taskScheduler.initialize(desc);

    std::vector<int> vec;

    uint32_t numTasks = desc.priorityCount * desc.priorityBoostAge;

    Task* pRoot = taskScheduler.allocateTask(PriorityStarvationGrabber::generatorFunc);
    PriorityStarvationGrabber taskData{ &vec, numTasks, (uint32_t)desc.priorityBoostAge, 0, desc.priorityCount };
    pRoot->setData(taskData);
    taskScheduler.spawnTaskAndWait(pRoot);

    // Verify that non-zero priority tasks were executed at a boost age.
    uint32_t nextPriority = 1;
    for (uint32_t ii = 0; ii < numTasks; ++ii)
    {
        int result = vec.back();
        vec.pop_back();

        if (ii % desc.priorityBoostAge == 0)
        {
            ASSERT_NE(result, 0);
            nextPriority = nextPriority % (desc.priorityCount - 1) + 1; // [1, desc.priorityCount]
        }
        else
        {
            ASSERT_EQ(result, 0);
        }
    }

    taskScheduler.shutdown();
}

//------------------------------------------------------------------------------
TEST(MicroScheduler, priorityStarvationSingleThreaded)
{
    for (uint32_t ii = 0; ii < ITERATIONS; ++ii)
    {
        GTS_CONCRT_LOGGER_RESET();
        PriorityOrderTest();
    }
}

} // namespace testing
