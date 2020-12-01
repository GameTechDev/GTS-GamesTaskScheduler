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
#include <thread>

#include "gts/platform/Atomic.h"
#include "gts/analysis/Trace.h"

#include "gts/micro_scheduler/WorkerPool.h"
#include "gts/micro_scheduler/MicroScheduler.h"

#include "SchedulerTestsCommon.h"

using namespace gts;

namespace testing {

static uint32_t const TASK_COUNT = 1000;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Test Execution Location

enum class PartitionSharingMode
{
    Schedule1Only,
    Schedule2Only,
    Share
};

////////////////////////////////////////////////////////////////////////////////
struct ExecutionIndexTracker : public Task
{
    ExecutionIndexTracker(Atomic<uint32_t>* taskCountByScheduleIdx)
        : taskCountByScheduleIdx(taskCountByScheduleIdx)
    {}

    // Count the task when its executed.
    Task* execute(TaskContext const& ctx)
    {
        uint32_t ownerId = ctx.workerId.ownerId();
        GTS_ASSERT(ownerId < 2 && "You forgot to reset the owner ID generator.");
        taskCountByScheduleIdx[ownerId].fetch_add(1, memory_order::release);
        return nullptr;
    }

    Atomic<uint32_t>* taskCountByScheduleIdx;
};

////////////////////////////////////////////////////////////////////////////////
struct ExecutionIndexTrackerGenerator : public Task
{
    Task* execute(TaskContext const& ctx)
    {
        int32_t startRefCount = refCount();

        addRef(numTasks + 1);

        for (uint32_t ii = 0; ii < numTasks; ++ii)
        {
            Task* pTask = ctx.pMicroScheduler->allocateTask<ExecutionIndexTracker>(taskCountByScheduleIdx);
            addChildTaskWithoutRef(pTask);

            ctx.pMicroScheduler->spawnTask(pTask);
        }

        if (partitionMode == PartitionSharingMode::Share)
        {
            // block until all tasks are completed by the partition
            while(refCount() > startRefCount + 1)
            {
                ctx.pMicroScheduler->wakeWorker();
                GTS_PAUSE();
            }
            addRef(-1);
        }
        else
        {
            waitForAll();
        }

        return nullptr;
    }

    uint32_t numTasks;
    Atomic<uint32_t>* taskCountByScheduleIdx;
    uint32_t numSchedulers;
    PartitionSharingMode partitionMode;
};

//------------------------------------------------------------------------------
void paritionOnlyTests(uint32_t taskCount, PartitionSharingMode mode)
{
    // reset so the test buckets start at zero.
    MicroScheduler::resetIdGenerator();

    if (gts::Thread::getHardwareThreadCount() < 2)
    {
        return;
    }

    WorkerPool workerPool1;
    WorkerPool workerPool2;
    MicroScheduler taskScheduler1;
    MicroScheduler taskScheduler2;

    if (mode == PartitionSharingMode::Share)
    {
        workerPool1.initialize(1);
        taskScheduler1.initialize(&workerPool1);

        workerPool2.initialize(2);
        taskScheduler2.initialize(&workerPool2);

        taskScheduler2.addExternalVictim(&taskScheduler1);
    }
    else
    {
        workerPool1.initialize(2);
        taskScheduler1.initialize(&workerPool1);

        workerPool2.initialize(2);
        taskScheduler2.initialize(&workerPool2);
    }

    // Create a counter per thread.
    constexpr uint32_t numSchedulers = 2;
    Atomic<uint32_t> taskCountByScheduleIdx[numSchedulers];
    for (auto& counter : taskCountByScheduleIdx)
    {
        counter.store(0, memory_order::release);
    }

    ExecutionIndexTrackerGenerator* pRootTask = taskScheduler2.allocateTask<ExecutionIndexTrackerGenerator>();
    pRootTask->numTasks                       = taskCount;
    pRootTask->taskCountByScheduleIdx         = taskCountByScheduleIdx;
    pRootTask->numSchedulers                  = numSchedulers;
    pRootTask->partitionMode                  = mode;

    switch (mode)
    {
    case PartitionSharingMode::Schedule1Only:
        // Expect that the scheduler2 didn't steal any tasks.
        taskScheduler1.spawnTaskAndWait(pRootTask);
        break;

    case PartitionSharingMode::Schedule2Only:
        // Expect that the scheduler1 didn't steal any tasks.
        taskScheduler2.spawnTaskAndWait(pRootTask);
        break;

    case PartitionSharingMode::Share:
        // Queue on scheduler1 and block until the scheduler2 has
        // stolen all the tasks.
        pRootTask->setAffinity(0);
        taskScheduler1.spawnTaskAndWait(pRootTask);
        break;
    }

    switch (mode)
    {
    case PartitionSharingMode::Schedule1Only:
        ASSERT_TRUE(taskCountByScheduleIdx[1].load(memory_order::acquire) == 0);
        break;

    case PartitionSharingMode::Schedule2Only:
        ASSERT_TRUE(taskCountByScheduleIdx[0].load(memory_order::acquire) == 0);
        break;

    case PartitionSharingMode::Share:
        ASSERT_TRUE(taskCountByScheduleIdx[0].load(memory_order::acquire) == 0);
        break;
    }

    // Total up the counters
    uint32_t numTasksCompleted = 0;
    for (auto& counter : taskCountByScheduleIdx)
    {
        numTasksCompleted += counter.load(memory_order::acquire);
    }

    // Verify all the tasks ran.
    ASSERT_EQ(taskCount, numTasksCompleted);

    taskScheduler2.shutdown();
    taskScheduler1.shutdown();
}

//------------------------------------------------------------------------------
TEST(MicroScheduler, scheduleOnPartitionOnly)
{
    for (uint32_t ii = 0; ii < ITERATIONS_CONCUR; ++ii)
    {
        paritionOnlyTests(TASK_COUNT, PartitionSharingMode::Schedule1Only);
    }
}

//------------------------------------------------------------------------------
TEST(MicroScheduler, scheduleOnMainAndPartitionSteals)
{
    for (uint32_t ii = 0; ii < ITERATIONS_CONCUR; ++ii)
    {
        paritionOnlyTests(TASK_COUNT, PartitionSharingMode::Schedule2Only);
    }
}

//------------------------------------------------------------------------------
TEST(MicroScheduler, scheduleMainOnly)
{
    for (uint32_t ii = 0; ii < ITERATIONS_CONCUR; ++ii)
    {
        paritionOnlyTests(TASK_COUNT, PartitionSharingMode::Share);
    }
}


} // namespace testing
