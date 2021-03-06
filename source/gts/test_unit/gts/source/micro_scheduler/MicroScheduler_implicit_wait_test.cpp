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

#include "SchedulerTestsCommon.h"

using namespace gts;

namespace testing {

////////////////////////////////////////////////////////////////////////////////
struct TaskGraphGenerator : public Task
{
    //--------------------------------------------------------------------------
    Task* execute(TaskContext const& ctx)
    {
        uint32_t d = depth++;

        if (d < maxDepth)
        {
            addRef(breadth);

            for (uint32_t ii = 0; ii < breadth; ++ii)
            {
                Task* pChildTask = ctx.pMicroScheduler->allocateTask<TaskGraphGenerator>(*this);
                addChildTaskWithoutRef(pChildTask);
                ctx.pMicroScheduler->spawnTask(pChildTask);
            }

            // NOTE: We don't wait for children so that we test the implicit wait.
        }

        taskCountByThreadIdx[ctx.workerId.localId()].fetch_add(1, memory_order::release);

        return nullptr;
    }

    MicroScheduler* taskScheduler;
    gts::Atomic<uint32_t>* taskCountByThreadIdx;
    uint32_t taskPerGroup;
    uint32_t depth;
    uint32_t breadth;
    uint32_t maxDepth;
    uint32_t threadCount;
    bool testAffinity;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// TASK GRAPH TESTS:

//------------------------------------------------------------------------------
void TestImplicitWait(uint32_t depth, uint32_t breadth, uint32_t threadCount)
{
    uint32_t totalTasks = (uint32_t)((pow(breadth, depth + 1) - 1) / (breadth - 1)); // full k-way tree node count

    WorkerPool workerPool;
    workerPool.initialize(threadCount);

    MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    // Create a counter per thread.
    std::vector<gts::Atomic<uint32_t>> taskCountByThreadIdx(threadCount);
    for (auto& counter : taskCountByThreadIdx)
    {
        counter.store(0, memory_order::release);
    }

    TaskGraphGenerator* pRootTask   = taskScheduler.allocateTask<TaskGraphGenerator>();
    pRootTask->taskScheduler        = &taskScheduler;
    pRootTask->taskCountByThreadIdx = taskCountByThreadIdx.data();
    pRootTask->depth                = 0;
    pRootTask->breadth              = breadth;
    pRootTask->maxDepth             = depth;
    pRootTask->threadCount          = threadCount;

    taskScheduler.spawnTaskAndWait(pRootTask);

    // Total up the counters
    uint32_t taskCount = 0;
    for (auto& counter : taskCountByThreadIdx)
    {
        taskCount += counter.load(memory_order::acquire);
    }

    ASSERT_EQ(totalTasks, taskCount);

    taskScheduler.shutdown();
}

//------------------------------------------------------------------------------
TEST(MicroScheduler, implicitWaitSingleThreaded)
{
    for (uint32_t ii = 0; ii < ITERATIONS; ++ii)
    {
        GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);
        TestImplicitWait(TEST_DEPTH, 3, 1);
    }
}

//------------------------------------------------------------------------------
TEST(MicroScheduler, implicitWaitMultiThreaded)
{
    for (uint32_t ii = 0; ii < ITERATIONS_CONCUR; ++ii)
    {
        GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);
        TestImplicitWait(TEST_DEPTH, 3, gts::Thread::getHardwareThreadCount());
    }
}

//------------------------------------------------------------------------------
TEST(MicroScheduler, implicitWaitMultiThreaded_stress)
{
    for (uint32_t ii = 0; ii < ITERATIONS_STRESS; ++ii)
    {
        GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);
        TestImplicitWait(STRESS_DEPTH, 2, gts::Thread::getHardwareThreadCount());
    }
}

} // namespace testing
