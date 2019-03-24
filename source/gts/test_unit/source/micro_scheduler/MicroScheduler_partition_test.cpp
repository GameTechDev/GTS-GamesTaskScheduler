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

static uint32_t const TASK_COUNT = 1000;

//------------------------------------------------------------------------------
TEST(MicroScheduler, makePartition)
{
    for(uint32_t iters = 0; iters < ITERATIONS_STRESS; ++iters)
    {
        const uint32_t workerCount = gts::Thread::getHardwareThreadCount();
        ASSERT_TRUE(workerCount > 1);

        WorkerPool workerPool;
        workerPool.initialize(workerCount);

        MicroScheduler taskScheduler;
        taskScheduler.initialize(&workerPool);

        Vector<uint32_t> indices;
        indices.push_back(1);
        WorkerPool* pWorkerPoolParition = workerPool.makePartition(indices);

        MicroScheduler* pMicroSchedulerPartition = taskScheduler.makePartition(pWorkerPoolParition, true);
        ASSERT_TRUE(pMicroSchedulerPartition != nullptr);
        ASSERT_TRUE(pMicroSchedulerPartition->isRunning());

        taskScheduler.shutdown();
        workerPool.shutdown();
    }
}

//------------------------------------------------------------------------------
TEST(MicroScheduler, makeBadDuplicatePartition)
{
    const uint32_t workerCount = gts::Thread::getHardwareThreadCount();
    ASSERT_TRUE(workerCount > 1);

    WorkerPool workerPool;
    workerPool.initialize(workerCount);

    MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    Vector<uint32_t> indices;
    indices.push_back(1);
    WorkerPool* pWorkerPoolParition = workerPool.makePartition(indices);

    MicroScheduler* pMicroSchedulerPartition = taskScheduler.makePartition(pWorkerPoolParition, true);
    ASSERT_TRUE(pMicroSchedulerPartition != nullptr);
    ASSERT_TRUE(pMicroSchedulerPartition->isRunning());

    // Fail to make a duplicate partition. 
    ASSERT_DEATH(
        MicroScheduler* pBadMicroSchedulerPartition = taskScheduler.makePartition(pWorkerPoolParition, true);
        GTS_UNREFERENCED_PARAM(pBadMicroSchedulerPartition);
    ,"");

    taskScheduler.shutdown();
    workerPool.shutdown();
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Test Execution Location

struct ExecutionIndexTracker
{
    //--------------------------------------------------------------------------
    // Count the task when its executed.
    static Task* taskFunc(Task* pThisTask, TaskContext const& ctx)
    {
        ExecutionIndexTracker* data = (ExecutionIndexTracker*)pThisTask->getData();
        data->taskCountByThreadIdx[ctx.workerIndex].fetch_add(1);
        return nullptr;
    }

    //--------------------------------------------------------------------------
    // Root task.
    static Task* generatorTaskFunc(Task* pThisTask, TaskContext const& ctx)
    {
        ExecutionIndexTracker& data = *(ExecutionIndexTracker*)pThisTask->getData();

        pThisTask->addRef(data.numTasks);

        for (uint32_t ii = 0; ii < data.numTasks; ++ii)
        {
            Task* pTask = ctx.pMicroScheduler->allocateTask(ExecutionIndexTracker::taskFunc);
            pTask->setData(data);
            pThisTask->addChildTaskWithoutRef(pTask);

            ctx.pMicroScheduler->spawnTask(pTask);
        }

        if (data.waitForParitionToSteal)
        {
            // block until all tasks are completed by the partition
            while(true)
            {
                uint32_t completedCount = 0;
                for (uint32_t ii = 0; ii < data.numThreads; ++ii)
                {
                    completedCount += data.taskCountByThreadIdx[ii].load(gts::memory_order::relaxed);
                }
                if (completedCount == data.numTasks)
                {
                    break;
                }
                GTS_PAUSE();
            }
        }
        else
        {
            pThisTask->waitForChildren(ctx);
        }

        return nullptr;
    }

    uint32_t numTasks;
    gts::Atomic<uint32_t>* taskCountByThreadIdx;
    uint32_t numThreads;
    bool waitForParitionToSteal;
};

//------------------------------------------------------------------------------
void paritionOnlyTests(uint32_t taskCount, bool paritionCanSteal, bool mainOnly)
{
    if (gts::Thread::getHardwareThreadCount() < 2)
    {
        return;
    }

    uint32_t partitionWorkerIdx = 1;
    uint32_t threadCount = 2;

    WorkerPool workerPool;
    workerPool.initialize(threadCount);

    MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    Vector<uint32_t> indices;
    indices.push_back(partitionWorkerIdx);
    WorkerPool* pWorkerPoolParition = workerPool.makePartition(indices);
    MicroScheduler* pMicroSchedulerPartition = taskScheduler.makePartition(pWorkerPoolParition, true);

    // Create a counter per thread.
    std::vector<gts::Atomic<uint32_t>> taskCountByThreadIdx(threadCount);
    for (auto& counter : taskCountByThreadIdx)
    {
        counter.store(0);
    }

    ExecutionIndexTracker taskData;
    taskData.numTasks = taskCount;
    taskData.taskCountByThreadIdx = taskCountByThreadIdx.data();
    taskData.numThreads = threadCount;
    taskData.waitForParitionToSteal = paritionCanSteal;

    Task* pRootTask = pMicroSchedulerPartition->allocateTask(ExecutionIndexTracker::generatorTaskFunc);
    pRootTask->setData(taskData);

    if (paritionCanSteal)
    {
        // Queue on main scheduler and block until the partition has
        // stole all the tasks.
        taskScheduler.spawnTaskAndWait(pRootTask);
    }
    else
    {
        if (mainOnly)
        {
            // Expect that the partition didn't steal any tasks.
            taskScheduler.spawnTaskAndWait(pRootTask);
        }
        else
        {
            // Queue to the partition and expect only the partition to execute
            // the tasks.
            pMicroSchedulerPartition->spawnTaskAndWait(pRootTask);
        }
    }
    
    // Total up the counters
    uint32_t numTasksCompleted = 0;
    uint32_t idx = 0;
    for (auto& counter : taskCountByThreadIdx)
    {
        numTasksCompleted += counter.load();

        if (mainOnly)
        {
            // Verify no workers ran off the main parition.
            if (idx == partitionWorkerIdx)
            {
                ASSERT_TRUE(counter.load() != 0);
            }
        }
        else
        {
            // Verify no workers ran off the partition.
            ASSERT_TRUE(idx == partitionWorkerIdx || counter.load() == 0);
        }
        idx += 1;
    }

    // Verify all the tasks ran.
    ASSERT_EQ(taskCount, numTasksCompleted);

    taskScheduler.shutdown();
}

//------------------------------------------------------------------------------
TEST(MicroScheduler, scheduleOnPartitionOnly)
{
    paritionOnlyTests(TASK_COUNT, false, false);
}

//------------------------------------------------------------------------------
TEST(MicroScheduler, scheduleOnMainAndPartitionSteals)
{
    paritionOnlyTests(TASK_COUNT, true, false);
}

//------------------------------------------------------------------------------
TEST(MicroScheduler, scheduleMainOnly)
{
    paritionOnlyTests(TASK_COUNT, false, true);
}


} // namespace testing
