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
struct TaskListsGenerator
{   
    //--------------------------------------------------------------------------
    // Create linked list of link lists of tasks. Count this task when its exectuted.
    static Task* taskFunc(Task* pThisTask, TaskContext const& ctx)
    {
        TaskListsGenerator& data = *(TaskListsGenerator*)pThisTask->getData();

        uint32_t d = data.depth++;

        if (d <= data.maxDepth)
        {
            pThisTask->addRef(data.taskPerGroup);

            for (uint32_t ii = 0; ii < data.taskPerGroup; ++ii)
            {
                Task* pChildTask = ctx.pMicroScheduler->allocateTask(TaskListsGenerator::taskFunc);
                pChildTask->setData(data);
                pThisTask->addChildTask(pChildTask);
                ctx.pMicroScheduler->spawnTask(pChildTask);
            }

            pThisTask->waitForChildren(ctx);
        }

        data.taskCountByThreadIdx[ctx.workerIndex].fetch_add(1);

        return nullptr;
    }

    MicroScheduler* taskScheduler;
    gts::Atomic<uint32_t>* taskCountByThreadIdx;
    uint32_t index;
    uint32_t taskPerGroup;
    uint32_t depth;
    uint32_t maxDepth;
    uint32_t threadCount;
    bool testAffinity;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// TASK LISTS TESTS

//------------------------------------------------------------------------------
void TestTasksList(uint32_t tasksPerGroup, uint32_t depth, uint32_t threadCount)
{
    WorkerPool workerPool;
    workerPool.initialize(threadCount);

    MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    uint32_t totalTasks = 0;
    for (uint32_t ii = 0; ii <= depth; ++ii)
    {
        totalTasks += (uint32_t)pow(tasksPerGroup, ii + 1);
    };
    totalTasks += 1;

    // Create a counter per thread.
    std::vector<gts::Atomic<uint32_t>> taskCountByThreadIdx(threadCount);
    for (auto& counter : taskCountByThreadIdx)
    {
        counter.store(0);
    }

    TaskListsGenerator taskData;
    taskData.taskScheduler = &taskScheduler;
    taskData.taskCountByThreadIdx = taskCountByThreadIdx.data();
    taskData.taskPerGroup = tasksPerGroup;
    taskData.depth = 0;
    taskData.maxDepth = depth;
    taskData.threadCount = threadCount;

    Task* pRootTask = taskScheduler.allocateTask(TaskListsGenerator::taskFunc);
    pRootTask->setData(taskData);
    taskScheduler.spawnTaskAndWait(pRootTask);

    // Total up the counters
    uint32_t taskCount = 0;
    for (auto& counter : taskCountByThreadIdx)
    {
        taskCount += counter.load();
    }

    ASSERT_EQ(totalTasks, taskCount);

    taskScheduler.shutdown();
}

//------------------------------------------------------------------------------
TEST(MicroScheduler, taskListsSingleThreaded)
{
    for (uint32_t ii = 0; ii < ITERATIONS; ++ii)
    {
        GTS_CONCRT_LOGGER_RESET();
        TestTasksList(TEST_DEPTH, 3, 1);
    }
}

//------------------------------------------------------------------------------
TEST(MicroScheduler, taskListsMulitThreaded)
{
    for (uint32_t ii = 0; ii < 1; ++ii)
    {
        GTS_CONCRT_LOGGER_RESET();
        TestTasksList(TEST_DEPTH, 3, gts::Thread::getHardwareThreadCount());
    }
}

} // namespace testing
