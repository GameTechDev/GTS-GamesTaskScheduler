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
struct BasicTask
{
    // Some data for the task.
    int taskData;

    explicit BasicTask(int val) : taskData(val) {}

    // The task function. It MUST have this name and signature.
    static Task* taskFunc(Task* pThisTask, TaskContext const& ctx)
    {
        // Unpack the data.
        BasicTask* pBasicTask = (BasicTask*)pThisTask->getData();

        std::cout << "Hello! My value is: " << pBasicTask->taskData << std::endl;

        // This will be explained later.
        return nullptr;
    }
};


//------------------------------------------------------------------------------
void taskBasics()
{
    // Init boilerplate
    WorkerPool workerPool;
    bool result = workerPool.initialize();
    GTS_ASSERT(result);
    MicroScheduler taskScheduler;
    result = taskScheduler.initialize(&workerPool);
    GTS_ASSERT(result);

    // Create a new task.
    int val = 2;
    Task* pTask = taskScheduler.allocateTask<BasicTask>(val);

    // Tasks start with a refCount of 1.
    GTS_ASSERT(pTask->refCount() == 1);

    // Let's say we only want to run this task on the master worker thread.
    pTask->setAffinity(0);
    // Now this task can only every be run on worker thread #0.

    // Queue the task for execution and wait for it to complete. The act of
    // queueing a task implicitly reduces its refCount by 1.
    taskScheduler.spawnTaskAndWait(pTask /*, optional priority*/);

    // NOTE: wait ^^^^ does NOT mean that this thread is blocked. This thread will
    // actively execute any tasks in the scheduler until pTask completes.

    taskScheduler.shutdown();
    workerPool.shutdown();
}

//------------------------------------------------------------------------------
void manualTask()
{
    // Init boilerplate
    WorkerPool workerPool;
    bool result = workerPool.initialize();
    GTS_ASSERT(result);
    MicroScheduler taskScheduler;
    result = taskScheduler.initialize(&workerPool);
    GTS_ASSERT(result);

    // Create a new task manually.
    Task* pTask = taskScheduler.allocateTask(BasicTask::taskFunc, sizeof(BasicTask));

    // Add the data to the task that BasicTask::taskFunc() expects.
    // ***NOTE: data added to tasks does NOT have its destructor
    // called when the task is freed. If your data requires this,
    // you must call the destructor manually during execution.
    pTask->emplaceData<BasicTask>(1);

    //NOTE: You could also copy in existing data and pointers this way.
    //pTask->setData(...);

    taskScheduler.spawnTaskAndWait(pTask);

    taskScheduler.shutdown();
    workerPool.shutdown();
}

//------------------------------------------------------------------------------
// A non POD task.
struct NonPODTask
{
    // Dyanmic task data.
    int* taskData;

    NonPODTask(int val) : taskData(new int(val)) {}

    ~NonPODTask() { delete taskData; }

    // A task function. It MUST have this signature. It can also be a free function.
    static Task* taskFunc(Task* pThisTask, TaskContext const& ctx)
    {
        // Unpack the data.
        NonPODTask* pNonPODTask = (NonPODTask*)pThisTask->getData();

        std::cout << "Hello! My value is: " << pNonPODTask->taskData << std::endl;

        // Manually clean up the data.
        pNonPODTask->~NonPODTask();

        // This will be explained later.
        return nullptr;
    }
};

//------------------------------------------------------------------------------
void nonPODTask()
{
    // Init boilerplate
    WorkerPool workerPool;
    bool result = workerPool.initialize();
    GTS_ASSERT(result);
    MicroScheduler taskScheduler;
    result = taskScheduler.initialize(&workerPool);
    GTS_ASSERT(result);

    // Create a new task. Tasks start with a refCount of 1.
    Task* pTask = taskScheduler.allocateTask(NonPODTask::taskFunc);

    // NonPODTask requires its destructor to be called to not leak.
    pTask->emplaceData<NonPODTask>(1);

    taskScheduler.spawnTaskAndWait(pTask);

    taskScheduler.shutdown();
    workerPool.shutdown();
}

} // namespace gts_examples
