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
#include <functional>

#include "gts/platform/Atomic.h"
#include "gts/analysis/Trace.h"

#include "gts/micro_scheduler/WorkerPool.h"
#include "gts/micro_scheduler/MicroScheduler.h"

#include "SchedulerTestsCommon.h"

using namespace gts;

namespace testing {

using WorkFunc = std::function<void(MicroScheduler&)>;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// INIT/SHUTDOWN TESTS:

//------------------------------------------------------------------------------
TEST(MicroScheduler, initializeMultipleSameThread)
{
    WorkerPool workerPool;
    workerPool.initialize();

    constexpr uint32_t NUM_SCHEDULERS = 10;

    MicroScheduler taskScheduler[NUM_SCHEDULERS];

    for (uint32_t ii = 0; ii < NUM_SCHEDULERS; ++ii)
    {
        bool result = taskScheduler[ii].initialize(&workerPool);
        ASSERT_TRUE(result);
    }

    for (uint32_t ii = 0; ii < NUM_SCHEDULERS; ++ii)
    {
        taskScheduler[NUM_SCHEDULERS - ii - 1].shutdown();
    }
}

//------------------------------------------------------------------------------
void TestMultipleSchedulers(
    const uint32_t threadCount,
    const uint32_t schedulerCount,
    WorkFunc& fcnWork)
{
    WorkerPool workerPool;
    workerPool.initialize(threadCount);

    MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    gts::Vector<std::thread*> threads(schedulerCount);
    std::atomic<bool> startInitialize = { false };
    std::atomic<uint32_t> initCount = { 0 };
    std::atomic<bool> startWork = { false };
    std::atomic<uint32_t> shutdownCount = { 0 };

    for (uint32_t ii = 0; ii < schedulerCount; ++ii)
    {
        std::thread* pThread = new std::thread([&]()
        {
            MicroScheduler taskScheduler;

            while (!startInitialize.load(std::memory_order::memory_order_acquire))
            {
                GTS_PAUSE();
            }

            taskScheduler.initialize(&workerPool);
            ++initCount;

            while (!startWork.load(std::memory_order::memory_order_acquire))
            {
                GTS_PAUSE();
            }

            fcnWork(taskScheduler);

            taskScheduler.shutdown();
            ++shutdownCount;
        });

        threads[ii] = pThread;
    }

    startInitialize = true;

    while (initCount.load(std::memory_order::memory_order_acquire) < schedulerCount)
    {
        GTS_PAUSE();
    }

    startWork = true;

    fcnWork(taskScheduler);

    taskScheduler.shutdown();

    while(shutdownCount.load(std::memory_order::memory_order_acquire) < schedulerCount)
    {
        GTS_PAUSE();
    }

    workerPool.shutdown();

    for (uint32_t ii = 0; ii < schedulerCount; ++ii)
    {
        threads[ii]->join();
        delete threads[ii];
    }
}

//------------------------------------------------------------------------------
WorkFunc EmptyWork = [](MicroScheduler&)
{};

//------------------------------------------------------------------------------
TEST(MicroScheduler, testInitShutownWorker1Scheduler)
{
    for (uint32_t ii = 0; ii < ITERATIONS_CONCUR; ++ii)
    {
        GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);
        TestMultipleSchedulers(1, 1, EmptyWork);
        MicroScheduler::resetIdGenerator();
    }
}

//------------------------------------------------------------------------------
TEST(MicroScheduler, testInitShutownNWorker1Schedulers)
{
    for (uint32_t ii = 0; ii < ITERATIONS_CONCUR; ++ii)
    {
        GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);
        TestMultipleSchedulers(gts::Thread::getHardwareThreadCount(), 1, EmptyWork);
        MicroScheduler::resetIdGenerator();
    }
}

//------------------------------------------------------------------------------
TEST(MicroScheduler, testInitShutown1WorkerNSchedulers)
{
    for (uint32_t ii = 0; ii < ITERATIONS_CONCUR; ++ii)
    {
        GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);
        TestMultipleSchedulers(1, gts::Thread::getHardwareThreadCount(), EmptyWork);
        MicroScheduler::resetIdGenerator();
    }
}

//------------------------------------------------------------------------------
TEST(MicroScheduler, testInitShutownNWorkerNSchedulers)
{
    for (uint32_t ii = 0; ii < ITERATIONS_CONCUR; ++ii)
    {
        GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);
        TestMultipleSchedulers(gts::Thread::getHardwareThreadCount(), gts::Thread::getHardwareThreadCount(), EmptyWork);
        MicroScheduler::resetIdGenerator();
    }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// SPAWN TESTS:

struct alignas(GTS_NO_SHARING_CACHE_LINE_SIZE) AlignedCounter
{
    gts::Atomic<uint32_t> counter;
    char pad[GTS_NO_SHARING_CACHE_LINE_SIZE - sizeof(gts::Atomic<uint32_t>)];
};

////////////////////////////////////////////////////////////////////////////////
struct TaskCounter : public Task
{
    TaskCounter(AlignedCounter* taskCountByThreadIdx)
        : taskCountByThreadIdx(taskCountByThreadIdx)
    {}

    //--------------------------------------------------------------------------
    // Count the task when its executed.
    Task* execute(TaskContext const& ctx)
    {
        taskCountByThreadIdx[ctx.workerId.localId()].counter.fetch_add(1, memory_order::release);
        return nullptr;
    }

    AlignedCounter* taskCountByThreadIdx;
};

////////////////////////////////////////////////////////////////////////////////
struct TaskCounterGenerator : public Task
{
    //--------------------------------------------------------------------------
    // Root task for TestSpawnTask
    Task* execute(TaskContext const& ctx)
    {
        addRef(numTasks + 1);

        for (uint32_t ii = 0; ii < numTasks; ++ii)
        {
            Task* pTask = ctx.pMicroScheduler->allocateTask<TaskCounter>(taskCountByThreadIdx);
            addChildTaskWithoutRef(pTask);

            ctx.pMicroScheduler->spawnTask(pTask);
        }

        waitForAll();

        return nullptr;
    }

    uint32_t numTasks;
    AlignedCounter* taskCountByThreadIdx;
};

//------------------------------------------------------------------------------
void SpawnTasks(MicroScheduler& taskScheduler, const uint32_t numTasks, const uint32_t threadCount)
{
    // Create a counter per thread.
    std::vector<AlignedCounter> taskCountByThreadIdx(threadCount);
    for (auto& counter : taskCountByThreadIdx)
    {
        counter.counter.store(0, memory_order::release);
    }

    TaskCounterGenerator* pRootTask = taskScheduler.allocateTask<TaskCounterGenerator>();
    pRootTask->numTasks = numTasks;
    pRootTask->taskCountByThreadIdx = taskCountByThreadIdx.data();

    taskScheduler.spawnTaskAndWait(pRootTask);

    // Total up the counters
    uint32_t numTasksCompleted = 0;
    for (auto& counter : taskCountByThreadIdx)
    {
        numTasksCompleted += counter.counter.load(memory_order::acquire);
    }

    // Verify all the tasks ran.
    ASSERT_EQ(numTasks, numTasksCompleted);
}

//------------------------------------------------------------------------------
TEST(MicroScheduler, testSpawn1Worker1Scheduler)
{
    for (uint32_t ii = 0; ii < ITERATIONS_CONCUR; ++ii)
    {
        GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);
        uint32_t numTasks       = TEST_DEPTH;
        uint32_t threadCount    = 1;
        uint32_t schedulerCount = 1;
        WorkFunc work = [&](MicroScheduler& taskScheduler)
        {
            SpawnTasks(taskScheduler, numTasks, threadCount);
        };
        TestMultipleSchedulers(threadCount, schedulerCount, work);
        MicroScheduler::resetIdGenerator();
    }
}

//------------------------------------------------------------------------------
TEST(MicroScheduler, testSpawnNWorker1Scheduler)
{
    for (uint32_t ii = 0; ii < ITERATIONS_CONCUR; ++ii)
    {
        GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);
        uint32_t numTasks       = TEST_DEPTH;
        uint32_t threadCount    = gts::Thread::getHardwareThreadCount();
        uint32_t schedulerCount = 1;
        WorkFunc work = [&](MicroScheduler& taskScheduler)
        {
            SpawnTasks(taskScheduler, numTasks, threadCount);
        };
        TestMultipleSchedulers(threadCount, schedulerCount, work);
        MicroScheduler::resetIdGenerator();
    }
}

//------------------------------------------------------------------------------
TEST(MicroScheduler, testSpawn1WorkerNSchedulers)
{
    for (uint32_t ii = 0; ii < ITERATIONS_CONCUR; ++ii)
    {
        GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);
        uint32_t numTasks       = TEST_DEPTH;
        uint32_t threadCount    = 1;
        uint32_t schedulerCount = gts::Thread::getHardwareThreadCount();
        WorkFunc work = [&](MicroScheduler& taskScheduler)
        {
            SpawnTasks(taskScheduler, numTasks, threadCount);
        };
        TestMultipleSchedulers(threadCount, schedulerCount, work);
        MicroScheduler::resetIdGenerator();
    }
}

//------------------------------------------------------------------------------
TEST(MicroScheduler, testSpawnNWorkerNSchedulers)
{
    for (uint32_t ii = 0; ii < ITERATIONS_CONCUR; ++ii)
    {
        GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);
        uint32_t numTasks       = TEST_DEPTH;
        uint32_t threadCount    = gts::Thread::getHardwareThreadCount();
        uint32_t schedulerCount = gts::Thread::getHardwareThreadCount();
        WorkFunc work = [&](MicroScheduler& taskScheduler)
        {
            SpawnTasks(taskScheduler, numTasks, threadCount);
        };
        TestMultipleSchedulers(threadCount, schedulerCount, work);
        MicroScheduler::resetIdGenerator();
    }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// REGISTER/UNREGISTER TESTS:

//------------------------------------------------------------------------------
void TestMultipleSchedulersRegistrationAndWork(
    const uint32_t threadCount,
    const uint32_t schedulerCount,
    WorkFunc& fcnWork)
{
    WorkerPool workerPool;
    workerPool.initialize(threadCount);

    gts::Vector<std::thread*> threads(schedulerCount);
    std::atomic<uint32_t> shutdownCount = { 0 };

    constexpr uint32_t iterations = 20;

    for (uint32_t ii = 0; ii < schedulerCount; ++ii)
    {
        std::thread* pThread = new std::thread([&]()
        {
            for (uint32_t iter = 0; iter < iterations; ++iter)
            {
                MicroScheduler taskScheduler;
                taskScheduler.initialize(&workerPool);

                fcnWork(taskScheduler);

                taskScheduler.shutdown();
            }

            ++shutdownCount;
        });

        threads[ii] = pThread;
    }

    for (uint32_t iter = 0; iter < iterations; ++iter)
    {

        MicroScheduler taskScheduler;
        taskScheduler.initialize(&workerPool);

        fcnWork(taskScheduler);

        taskScheduler.shutdown();
    }

    while(shutdownCount.load(std::memory_order::memory_order_acquire) < schedulerCount)
    {
        GTS_PAUSE();
    }

    workerPool.shutdown();

    for (uint32_t ii = 0; ii < schedulerCount; ++ii)
    {
        threads[ii]->join();
        delete threads[ii];
    }
}

//------------------------------------------------------------------------------
TEST(MicroScheduler, testRegisterAndWork1Worker1Scheduler)
{
   for (uint32_t ii = 0; ii < ITERATIONS_CONCUR; ++ii)
   {
       GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);
       uint32_t numTasks       = TEST_DEPTH;
       uint32_t threadCount    = 1;
       uint32_t schedulerCount = 1;
       WorkFunc work = [&](MicroScheduler& taskScheduler)
       {
           SpawnTasks(taskScheduler, numTasks, threadCount);
       };
       TestMultipleSchedulersRegistrationAndWork(threadCount, schedulerCount, work);
       MicroScheduler::resetIdGenerator();
   }
}

//------------------------------------------------------------------------------
TEST(MicroScheduler, testRegisterAndWorkNWorker1Scheduler)
{
   for (uint32_t ii = 0; ii < ITERATIONS_CONCUR; ++ii)
   {
       GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);
       uint32_t numTasks       = TEST_DEPTH;
       uint32_t threadCount    = gts::Thread::getHardwareThreadCount();
       uint32_t schedulerCount = 1;
       WorkFunc work = [&](MicroScheduler& taskScheduler)
       {
           SpawnTasks(taskScheduler, numTasks, threadCount);
       };
       TestMultipleSchedulersRegistrationAndWork(threadCount, schedulerCount, work);
       MicroScheduler::resetIdGenerator();
   }
}

//------------------------------------------------------------------------------
TEST(MicroScheduler, testRegisterAndWork1WorkerNSchedulers)
{
   for (uint32_t ii = 0; ii < ITERATIONS_CONCUR; ++ii)
   {
       GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);
       uint32_t numTasks       = TEST_DEPTH;
       uint32_t threadCount    = 1;
       uint32_t schedulerCount = gts::Thread::getHardwareThreadCount();
       WorkFunc work = [&](MicroScheduler& taskScheduler)
       {
           SpawnTasks(taskScheduler, numTasks, threadCount);
       };
       TestMultipleSchedulersRegistrationAndWork(threadCount, schedulerCount, work);

       MicroScheduler::resetIdGenerator();
   }
}

//------------------------------------------------------------------------------
TEST(MicroScheduler, testRegisterAndWorkNWorkerNSchedulers)
{
   for (uint32_t ii = 0; ii < ITERATIONS_CONCUR; ++ii)
   {
       GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);
       uint32_t numTasks       = TEST_DEPTH;
       uint32_t threadCount    = gts::Thread::getHardwareThreadCount();
       uint32_t schedulerCount = gts::Thread::getHardwareThreadCount();
       WorkFunc work = [&](MicroScheduler& taskScheduler)
       {
           SpawnTasks(taskScheduler, numTasks, threadCount);
       };
       TestMultipleSchedulersRegistrationAndWork(threadCount, schedulerCount, work);

       MicroScheduler::resetIdGenerator();
   }
}

} // namespace testing
