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
TEST(MicroScheduler, allocateCStyleTask)
{
    WorkerPool workerPool;
    workerPool.initialize(1);

    MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    uint32_t val = 0;
    Task* pTask = taskScheduler.allocateTaskRaw([](Task* pThisTask, TaskContext const&)->Task*
    {
        uint32_t* pData = (uint32_t*)pThisTask->getData();
        *pData = 1;
        return nullptr;
    },
    sizeof(val));
    pTask->setData(&val);

    taskScheduler.spawnTaskAndWait(pTask);

    ASSERT_EQ(val, 1u);
}

//------------------------------------------------------------------------------
struct ObjectTask
{
    ObjectTask(uint32_t& val) : m_val(val) {}
    static Task* taskFunc(Task* pThisTask, TaskContext const&)
    {
        ObjectTask* pObj = (ObjectTask*)pThisTask->getData();
        pObj->m_val = 2;
        return nullptr;
    }

    uint32_t& m_val;
};

//------------------------------------------------------------------------------
TEST(Task, allocateObjectTask)
{
    WorkerPool workerPool;
    workerPool.initialize(1);

    MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    uint32_t val = 0;
    Task* pTask = taskScheduler.allocateTask<ObjectTask>(val);

    taskScheduler.spawnTaskAndWait(pTask);

    ASSERT_EQ(val, 2u);
}

//------------------------------------------------------------------------------
TEST(Task, allocateLambdaTask)
{
    WorkerPool workerPool;
    workerPool.initialize(1);

    MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    uint32_t val = 0;
    Task* pTask = taskScheduler.allocateTask([&val](Task*, TaskContext const&)->Task*
    {
        val = 3;
        return nullptr;
    });

    taskScheduler.spawnTaskAndWait(pTask);

    ASSERT_EQ(val, 3u);
}

//------------------------------------------------------------------------------
TEST(Task, allocateFunctionalTask)
{
    WorkerPool workerPool;
    workerPool.initialize(1);

    MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    uint32_t val = 0;
    Task* pTask = taskScheduler.allocateTask([](uint32_t& val, Task*, TaskContext const&)->Task*
    {
        val = 4;
        return nullptr;
    },
    std::ref(val));

    taskScheduler.spawnTaskAndWait(pTask);

    ASSERT_EQ(val, 4u);
}

//------------------------------------------------------------------------------
TEST(Task, addChild)
{
    WorkerPool workerPool;
    workerPool.initialize(1);

    MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    Task* pTask = taskScheduler.allocateTaskRaw([](Task*, TaskContext const&)->Task*{ return nullptr; });
    pTask->addRef(1);

    Task* pChild0 = taskScheduler.allocateTaskRaw([](Task*, TaskContext const&)->Task* { return nullptr; });
    pTask->addChildTaskWithoutRef(pChild0);
    taskScheduler.spawnTask(pChild0);

    TaskContext ctx{ nullptr, 0 };
    pTask->waitForChildren(ctx);
    pTask->destroy(ctx);
}

//------------------------------------------------------------------------------
TEST(Task, setContinuation)
{
    WorkerPool workerPool;
    workerPool.initialize(1);

    MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    Task* pTask = taskScheduler.allocateTaskRaw([](Task* pThisTask, TaskContext const& ctx)->Task*
    {
        Task* pContinuation = ctx.pMicroScheduler->allocateTaskRaw([](Task*, TaskContext const&)->Task* { return nullptr; });
        pContinuation->addRef(1);
        pThisTask->setContinuationTask(pContinuation);

        Task* pChild = ctx.pMicroScheduler->allocateTaskRaw([](Task*, TaskContext const&)->Task* { return nullptr; });
        pContinuation->addChildTaskWithoutRef(pChild);

        return pChild;

    });

    taskScheduler.spawnTaskAndWait(pTask);
}

//------------------------------------------------------------------------------
TEST(Task, queueContinuationBad)
{
    WorkerPool workerPool;
    workerPool.initialize(1);

    MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    EXPECT_DEATH({

        Task* pTask = taskScheduler.allocateTaskRaw([](Task*, TaskContext const&)->Task* { return nullptr; });
        Task* pContinuation = taskScheduler.allocateTaskRaw([](Task*, TaskContext const&)->Task* { return nullptr; });
        pTask->setContinuationTask(pContinuation);

        taskScheduler.spawnTask(pContinuation);

    }, "");
}

//------------------------------------------------------------------------------
TEST(Task, returnContinuationBad)
{
    WorkerPool workerPool;
    workerPool.initialize(1);

    MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    EXPECT_DEATH({

        Task* pTask = taskScheduler.allocateTaskRaw([](Task* pThisTask, TaskContext const& ctx)->Task*
        {
            Task* pContinuation = ctx.pMicroScheduler->allocateTaskRaw([](Task*, TaskContext const&)->Task* { return nullptr; });
            pThisTask->setContinuationTask(pContinuation);

            return pContinuation;

        });

        taskScheduler.spawnTaskAndWait(pTask);

    }, "");
}

} // namespace testing
