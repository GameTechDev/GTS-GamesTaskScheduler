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
struct BasicTask : public Task
{
    // Some data for the task.
    int taskData;

    explicit BasicTask(int val) : taskData(val) {}

    // Override the execute function with the work you want to do.
    virtual Task* execute(gts::TaskContext const&) override
    {
        std::cout << "Hello! My value is: " << taskData << std::endl;

        // This will be explained later.
        return nullptr;
    }
};


//------------------------------------------------------------------------------
void taskBasics()
{
    printf ("================\n");
    printf ("taskBasics\n");
    printf ("================\n");

    // Init boilerplate
    WorkerPool workerPool;
    bool result = workerPool.initialize();
    GTS_ASSERT(result);
    MicroScheduler microScheduler;
    result = microScheduler.initialize(&workerPool);
    GTS_ASSERT(result);

    // Create a new task.
    int val = 1;
    Task* pTask = microScheduler.allocateTask<BasicTask>(val);

    // Tasks start with a refCount of 1.
    GTS_ASSERT(pTask->refCount() == 1);

    // Let's say we only want to run this task on the master worker thread.
    pTask->setAffinity(0);
    // Now this task can only every be run on worker thread #0.

    // Spawn the task for execution and wait for it to complete. The act of
    // spawning a task implicitly reduces its refCount by 1 after execution.
    // If the refCount it zero after execution, the task will be destroyed,
    // which is the desired behavior (in general).
    microScheduler.spawnTaskAndWait(pTask /*, optional priority*/);
    // NOTE: wait ^^^^ does NOT mean that this thread is blocked. This thread will
    // actively execute any tasks in the scheduler until pTask completes.

    microScheduler.shutdown();
    workerPool.shutdown();
}

//------------------------------------------------------------------------------
void lambdaTask()
{
    printf ("================\n");
    printf ("lambdaTask\n");
    printf ("================\n");

    // Init boilerplate
    WorkerPool workerPool;
    bool result = workerPool.initialize();
    GTS_ASSERT(result);
    MicroScheduler microScheduler;
    result = microScheduler.initialize(&workerPool);
    GTS_ASSERT(result);

    uint32_t taskValue = 3;


    // Create a task packaged with a lambda
    Task* pTask = microScheduler.allocateTask([taskValue](TaskContext const&)->Task*
    //                                                    ^^^^^^^^^^^^^^^^^^^^^^^^^^
    // NOTE:                                              Must have this arg and return type.
    {
        std::cout << "Hello! My value is: " << taskValue << std::endl;
        return nullptr;
    });

    microScheduler.spawnTaskAndWait(pTask);


    // Alternatively, like std::thread
    pTask = microScheduler.allocateTask([](uint32_t val, TaskContext const&)->Task*
    //                                                   ^^^^^^^^^^^^^^^^^^^^^^^^^^
    // NOTE:                                             Must have this as the last arg and return type.
    {
        std::cout << "Hello! My value is: " << val << std::endl;
        return nullptr;
    },
    taskValue);

    microScheduler.spawnTaskAndWait(pTask);


    microScheduler.shutdown();
    workerPool.shutdown();
}

} // namespace gts_examples
