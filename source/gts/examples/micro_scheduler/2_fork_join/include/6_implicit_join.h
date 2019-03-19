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
 #pragma once

#include <iostream>

#include "gts/micro_scheduler/WorkerPool.h"
#include "gts/micro_scheduler/MicroScheduler.h"

using namespace gts;

namespace gts_examples {

//------------------------------------------------------------------------------
// A basic task structure.
struct BasicForkJoinTask
{
    int taskNum;

    BasicForkJoinTask(int val) : taskNum(val) {}

    static Task* taskPrintFunc(Task* pThisTask, TaskContext const& ctx)
    {
        // Unpack the data.
        BasicForkJoinTask* pData = (BasicForkJoinTask*)pThisTask->getData();
        std::cout << "Hello! I'm task #" << pData->taskNum << std::endl;
        return nullptr;
    }

    static Task* taskSpawnFunc(Task* pThisTask, TaskContext const& ctx)
    {
        // Unpack the data.
        BasicForkJoinTask* pData = (BasicForkJoinTask*)pThisTask->getData();

        std::cout << "Hello! I'm the root task" << std::endl;

        int const childCount = 5;

        // We are going to fork 'childCount' new tasks, so add their reference
        // to this task.
        pThisTask->addRef(childCount, gts::memory_order::relaxed);

        for (int ii = 0; ii < childCount; ++ii)
        {
            Task* pChild = ctx.pMicroScheduler->allocateTask(BasicForkJoinTask::taskPrintFunc);
            pChild->emplaceData<BasicForkJoinTask>(ii);
            pThisTask->addChildTask(pChild);
            ctx.pMicroScheduler->spawnTask(pChild);
        }

        // NOTE: we don't wait for the children to complete before finishing
        // this task. This is fine since this task will have extra references
        // until the children complete, so the scheduler will implicitly wait on 
        // this reference count. This still has the latency issue of waiting!

        return nullptr;
    }
};

//------------------------------------------------------------------------------
void implicitJoin()
{
    // Init boilerplate
    WorkerPool workerPool;
    bool result = workerPool.initialize();
    GTS_ASSERT(result);
    MicroScheduler taskScheduler;
    result = taskScheduler.initialize(&workerPool);
    GTS_ASSERT(result);

    // Create a new task.
    Task* pTask = taskScheduler.allocateTask(BasicForkJoinTask::taskSpawnFunc);

    taskScheduler.spawnTaskAndWait(pTask);

    // NOTE: wait ^^^^ does NOT mean that this thread is blocked. This thread will
    // actively execute any tasks in the scheduler until pTask completes.

    taskScheduler.shutdown();
    workerPool.shutdown();
}

} // namespace gts_examples
