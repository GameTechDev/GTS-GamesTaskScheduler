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
#include "gts/analysis/Trace.h"

#include "gts/micro_scheduler/WorkerPool.h"
#include "gts/micro_scheduler/MicroScheduler.h"

#include "SchedulerTestsCommon.h"

using namespace gts;

namespace testing {

////////////////////////////////////////////////////////////////////////////////
struct PriorityGrabber : public Task
{
    PriorityGrabber(std::vector<int>* vec, uint32_t taskIndex) : vec(vec), taskIndex(taskIndex) {}

    //--------------------------------------------------------------------------
    Task* execute(TaskContext const&)
    {
        vec->push_back(taskIndex);
        return nullptr;
    }

    std::vector<int>* vec;
    uint32_t taskIndex;
};

////////////////////////////////////////////////////////////////////////////////
struct PriorityGrabberGenerator : public Task
{
    //--------------------------------------------------------------------------
    Task* execute(TaskContext const& ctx)
    {
        addRef(numTasks + 1);

        // Add tasks in reverse priority order
        for (int32_t ii = (int32_t)numTasks - 1; ii >= 0; --ii)
        {
            Task* pChildTask = ctx.pMicroScheduler->allocateTask<PriorityGrabber>(vec, ii);
            addChildTaskWithoutRef(pChildTask);
            ctx.pMicroScheduler->spawnTask(pChildTask, ii);
        }
        waitForAll();

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

    PriorityGrabberGenerator* pRoot = taskScheduler.allocateTask<PriorityGrabberGenerator>();
    pRoot->vec                      = &vec;
    pRoot->numTasks                 = priorityCount;
    pRoot->taskIndex                = 0;

    taskScheduler.spawnTaskAndWait(pRoot);

    // Verify that tasks were executed in priority order
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
        GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);
        PriorityOrderTest(TEST_DEPTH, 1);
    }
}

//------------------------------------------------------------------------------
TEST(MicroScheduler, priorityOrderWithAffinitySingleThreaded)
{
    for (uint32_t ii = 0; ii < ITERATIONS; ++ii)
    {
        GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);
        PriorityOrderTest(TEST_DEPTH, 1);
    }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// PRIORITY STARVATION TESTS:

////////////////////////////////////////////////////////////////////////////////
struct PriorityStarvationGrabberGenerator : public Task
{
    //--------------------------------------------------------------------------
    Task* execute(TaskContext const& ctx)
    {
        addRef(numTasks + 1);

        uint32_t nextPriority = 1;

        for (uint32_t ii = 0; ii < numTasks; ++ii)
        {
            if (ii % boostAge == 0)
            {
                Task* pChildTask = ctx.pMicroScheduler->allocateTask<PriorityGrabber>(vec, nextPriority);
                addChildTaskWithoutRef(pChildTask);
                ctx.pMicroScheduler->spawnTask(pChildTask, nextPriority);

                nextPriority = nextPriority % (maxPriority - 1) + 1; // [1, data->maxPriority]
            }
            else
            {
                Task* pChildTask = ctx.pMicroScheduler->allocateTask<PriorityGrabber>(vec, 0);
                addChildTaskWithoutRef(pChildTask);

                ctx.pMicroScheduler->spawnTask(pChildTask, 0);
            }
        }
        waitForAll();
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
{    
    WorkerPool workerPool;
    workerPool.initialize(1);

    MicroSchedulerDesc desc;
    desc.pWorkerPool = &workerPool;
    desc.priorityCount = 3;
    desc.priorityBoostAge = (int16_t)desc.priorityCount;

    MicroScheduler taskScheduler;
    taskScheduler.initialize(desc);

    std::vector<int> vec;

    uint32_t numTasks = desc.priorityCount * desc.priorityBoostAge;

    PriorityStarvationGrabberGenerator* pRoot = taskScheduler.allocateTask<PriorityStarvationGrabberGenerator>();
    pRoot->vec                                = &vec;
    pRoot->numTasks                           = numTasks;
    pRoot->boostAge                           = (uint32_t)desc.priorityBoostAge;
    pRoot->taskIndex                          = 0;
    pRoot->maxPriority                        = desc.priorityCount;

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
        GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);
        PriorityOrderTest();
    }
}

} // namespace testing
