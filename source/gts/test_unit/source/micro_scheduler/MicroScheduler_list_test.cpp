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
struct TaskListGenerator
{
    //--------------------------------------------------------------------------
    // Create linked list of tasks. Count this task when its exectuted.
    static Task* taskFunc(Task* pThisTask, TaskContext const& ctx)
    {
        TaskListGenerator* data = (TaskListGenerator*)pThisTask->getData();

        uint32_t d = data->depth++;

        if (d < data->maxDepth)
        {
            pThisTask->addRef(1);

            Task* pChildTask = ctx.pMicroScheduler->allocateTask(TaskListGenerator::taskFunc);
            pChildTask->setData(*data);
            pThisTask->addChildTaskWithoutRef(pChildTask);
            ctx.pMicroScheduler->spawnTask(pChildTask);

            pThisTask->waitForChildren(ctx);

            data->taskCountByThreadIdx[ctx.workerIndex].fetch_add(1);
        }

        return nullptr;
    }

    MicroScheduler* taskScheduler;
    gts::Atomic<uint32_t>* taskCountByThreadIdx;
    uint32_t depth;
    uint32_t maxDepth;
    uint32_t threadCount;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// TASK LIST TESTS:

//------------------------------------------------------------------------------
void TestTaskList(uint32_t depth, uint32_t threadCount)
{
    WorkerPool workerPool;
    workerPool.initialize(threadCount);

    MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    // Create a counter per thread.
    std::vector<gts::Atomic<uint32_t>> taskCountByThreadIdx(threadCount);
    for (auto& counter : taskCountByThreadIdx)
    {
        counter.store(0);
    }

    // Initialize the task list generator data.
    TaskListGenerator taskData;
    taskData.taskScheduler = &taskScheduler;
    taskData.taskCountByThreadIdx = taskCountByThreadIdx.data();
    taskData.depth = 0;
    taskData.maxDepth = depth;
    taskData.threadCount = threadCount;

    // Generate the task list.
    Task* pRootTask = taskScheduler.allocateTask(TaskListGenerator::taskFunc);
    pRootTask->setData(taskData);
    taskScheduler.spawnTaskAndWait(pRootTask);

    // Total up the counters
    uint32_t taskCount = 0;
    for (auto& counter : taskCountByThreadIdx)
    {
        taskCount += counter.load();
    }

    // Was every task executed?
    ASSERT_EQ(depth, taskCount);

    taskScheduler.shutdown();
}

//------------------------------------------------------------------------------
TEST(MicroScheduler, taskListSingleThreaded)
{
    for (uint32_t ii = 0; ii < ITERATIONS; ++ii)
    {
        GTS_CONCRT_LOGGER_RESET();
        TestTaskList(TEST_DEPTH, 1);
    }
}

//------------------------------------------------------------------------------
TEST(MicroScheduler, taskListMultiThreaded)
{
    for (uint32_t ii = 0; ii < ITERATIONS_CONCUR; ++ii)
    {
        GTS_CONCRT_LOGGER_RESET();
        TestTaskList(TEST_DEPTH, gts::Thread::getHardwareThreadCount());
    }
}

} // namespace testing
