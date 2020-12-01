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

////////////////////////////////////////////////////////////////////////////////
struct ThreadData
{
    MicroScheduler* pMicroScheduler;
    uint32_t maxCount;
    gts::Atomic<uint32_t> count;
};

////////////////////////////////////////////////////////////////////////////////
struct PerThreadCounterTask : public Task
{
    //--------------------------------------------------------------------------
    PerThreadCounterTask(gts::Atomic<uint32_t>* count) : count(count)
    {}

    //--------------------------------------------------------------------------
    Task* execute(TaskContext const&)
    {
        count->fetch_add(1, memory_order::release);
        return nullptr;
    }

    gts::Atomic<uint32_t>* count = nullptr;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// NON-WORKER TASK TESTS:

//------------------------------------------------------------------------------
void NonWorkerThreadFunc(ThreadData* data)
{
    for (uint32_t ii = 0; ii < data->maxCount; ++ii)
    {
        Task* pTask = data->pMicroScheduler->allocateTask<PerThreadCounterTask>(&data->count);
        data->pMicroScheduler->spawnTaskAndWait(pTask);
    }
}

//------------------------------------------------------------------------------
void TestNonWorkerThreads(uint32_t numTasks, uint32_t nonWorkerCount, uint32_t threadCount)
{
    WorkerPool workerPool;
    workerPool.initialize(threadCount);

    MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    std::vector<ThreadData> threadData(nonWorkerCount);
    std::vector<std::thread*> threadPool(nonWorkerCount);

    for (uint32_t nn = 0; nn < nonWorkerCount; ++nn)
    {
        threadData[nn].pMicroScheduler = &taskScheduler;
        threadData[nn].maxCount = numTasks;
        threadData[nn].count.store(0, memory_order::release);

        threadPool[nn] = new std::thread(NonWorkerThreadFunc, &threadData[nn]);
    }

    for (uint32_t nn = 0; nn < nonWorkerCount; ++nn)
    {
        threadPool[nn]->join();
    }

    for (uint32_t nn = 0; nn < nonWorkerCount; ++nn)
    {
        delete threadPool[nn];
    }

    // verify all the tasks were executed
    for (uint32_t nn = 0; nn < nonWorkerCount; ++nn)
    {
        ASSERT_EQ(threadData[nn].count.load(memory_order::acquire), numTasks);
    }

    taskScheduler.shutdown();
    workerPool.shutdown();
}

//------------------------------------------------------------------------------
TEST(MicroScheduler_nonWorkerThread, singleThreaded)
{
    for (uint32_t ii = 0; ii < ITERATIONS_CONCUR; ++ii)
    {
        GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);
        TestNonWorkerThreads(1000, 1, 2); // 2 since the main thread will be blocked.

        WorkerPool::resetIdGenerator();
        MicroScheduler::resetIdGenerator();
    }
}

//------------------------------------------------------------------------------
TEST(MicroScheduler_nonWorkerThread, multiThreaded)
{
    for (uint32_t ii = 0; ii < ITERATIONS_CONCUR; ++ii)
    {
        GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);
        TestNonWorkerThreads(1000, 1, gts::Thread::getHardwareThreadCount());

        WorkerPool::resetIdGenerator();
        MicroScheduler::resetIdGenerator();
    }
}

//------------------------------------------------------------------------------
TEST(MicroScheduler_nonWorkerThread, multiThreadedStress)
{
    for (uint32_t ii = 0; ii < ITERATIONS_CONCUR; ++ii)
    {
        GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);
        TestNonWorkerThreads(1000, 10, gts::Thread::getHardwareThreadCount());

        WorkerPool::resetIdGenerator();
        MicroScheduler::resetIdGenerator();
    }
}

} // namespace testing
