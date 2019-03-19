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

//------------------------------------------------------------------------------
TEST(MicroScheduler, addChild)
{
    WorkerPool workerPool;
    workerPool.initialize(1);

    MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    Task* pTask = taskScheduler.allocateTask([](Task*, TaskContext const&)->Task*{ return nullptr; });
    pTask->addRef(1);

    Task* pChild0 = taskScheduler.allocateTask([](Task*, TaskContext const&)->Task* { return nullptr; });
    pTask->addChildTask(pChild0);
    taskScheduler.spawnTask(pChild0);

    TaskContext ctx{ nullptr, 0 };
    pTask->waitForChildren(ctx);
    pTask->destroy(ctx);
}

//------------------------------------------------------------------------------
TEST(MicroScheduler, setContinuation)
{
    WorkerPool workerPool;
    workerPool.initialize(1);

    MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    Task* pTask = taskScheduler.allocateTask([](Task* pThisTask, TaskContext const& ctx)->Task*
    {
        Task* pContinuation = ctx.pMicroScheduler->allocateTask([](Task*, TaskContext const&)->Task* { return nullptr; });
        pContinuation->addRef(1);
        pThisTask->setContinuationTask(pContinuation);

        Task* pChild = ctx.pMicroScheduler->allocateTask([](Task*, TaskContext const&)->Task* { return nullptr; });
        pContinuation->addChildTask(pChild);

        return pChild;

    });

    taskScheduler.spawnTaskAndWait(pTask);
}

//------------------------------------------------------------------------------
TEST(MicroScheduler, queueContinuationBad)
{
    WorkerPool workerPool;
    workerPool.initialize(1);

    MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    EXPECT_DEATH({

        Task* pTask = taskScheduler.allocateTask([](Task*, TaskContext const&)->Task* { return nullptr; });
        Task* pContinuation = taskScheduler.allocateTask([](Task*, TaskContext const&)->Task* { return nullptr; });
        pTask->setContinuationTask(pContinuation);

        taskScheduler.spawnTask(pContinuation);

    }, "");
}

//------------------------------------------------------------------------------
TEST(MicroScheduler, returnContinuationBad)
{
    WorkerPool workerPool;
    workerPool.initialize(1);

    MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    EXPECT_DEATH({

        Task* pTask = taskScheduler.allocateTask([](Task* pThisTask, TaskContext const& ctx)->Task*
        {
            Task* pContinuation = ctx.pMicroScheduler->allocateTask([](Task*, TaskContext const&)->Task* { return nullptr; });
            pThisTask->setContinuationTask(pContinuation);

            return pContinuation;

        });

        taskScheduler.spawnTaskAndWait(pTask);

    }, "");
}


////////////////////////////////////////////////////////////////////////////////
struct SpawnedTaskCounter
{
    //--------------------------------------------------------------------------
    // Count the task when its executed.
    static Task* taskFunc(Task* pThisTask, TaskContext const& ctx)
    {
        SpawnedTaskCounter* data = (SpawnedTaskCounter*)pThisTask->getData();
        data->taskCountByThreadIdx[ctx.workerIndex].fetch_add(1);
        return nullptr;
    }

    //--------------------------------------------------------------------------
    // Root task for TestSpawnTask
    static Task* generatorFunc(Task* pThisTask, TaskContext const& ctx)
    {
        SpawnedTaskCounter& data = *(SpawnedTaskCounter*)pThisTask->getData();

        pThisTask->addRef(data.numTasks);

        for (uint32_t ii = 0; ii < data.numTasks; ++ii)
        {
            Task* pTask = ctx.pMicroScheduler->allocateTask(SpawnedTaskCounter::taskFunc);
            pTask->setData(data);
            pThisTask->addChildTask(pTask);

            ctx.pMicroScheduler->spawnTask(pTask);
        }

        pThisTask->waitForChildren(ctx);

        return nullptr;
    }

    uint32_t numTasks;
    gts::Atomic<uint32_t>* taskCountByThreadIdx;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// SPAWN TASK TESTS:

//------------------------------------------------------------------------------
void TestSpawnTask(const uint32_t numTasks, const uint32_t threadCount)
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

    SpawnedTaskCounter taskData;
    taskData.numTasks = numTasks;
    taskData.taskCountByThreadIdx = taskCountByThreadIdx.data();

    Task* pRootTask = taskScheduler.allocateTask(SpawnedTaskCounter::generatorFunc);
    pRootTask->setData(taskData);

    taskScheduler.spawnTaskAndWait(pRootTask);

    // Total up the counters
    uint32_t numTasksCompleted = 0;
    for (auto& counter : taskCountByThreadIdx)
    {
        numTasksCompleted += counter.load();
    }

    // Verify all the tasks ran.
    ASSERT_EQ(numTasks, numTasksCompleted);

    taskScheduler.shutdown();
}

//------------------------------------------------------------------------------
TEST(MicroScheduler, spawnOneTask)
{
    for (uint32_t ii = 0; ii < ITERATIONS; ++ii)
    {
        GTS_CONCRT_LOGGER_RESET();
        TestSpawnTask(1, 1);
    }
}

//------------------------------------------------------------------------------
TEST(MicroScheduler, spawnTaskSingleThreaded)
{
    for (uint32_t ii = 0; ii < ITERATIONS; ++ii)
    {
        GTS_CONCRT_LOGGER_RESET();
        TestSpawnTask(TEST_DEPTH, 1);
    }
}

//------------------------------------------------------------------------------
TEST(MicroScheduler, spawnTaskMultiThreaded)
{
    for (uint32_t ii = 0; ii < ITERATIONS_CONCUR; ++ii)
    {
        GTS_CONCRT_LOGGER_RESET();
        TestSpawnTask(TEST_DEPTH, gts::Thread::getHardwareThreadCount());
    }
}

} // namespace testing
