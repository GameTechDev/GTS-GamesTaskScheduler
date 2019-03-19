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

#include <cassert>
#include <iostream>

#include <gts/micro_scheduler/WorkerPool.h>
#include <gts/micro_scheduler/MicroScheduler.h>

using namespace gts;

namespace gts_examples {

//------------------------------------------------------------------------------
// A parallel Fibonacci number calculator. NOTE: this is a terrible way to calculate
// a Fibonacci number, however it is a simple way to demonstrate the fork-join
// model of the micro-scheduler.
struct ParallelFibTask1
{
    uint32_t fibN;
    uint64_t* sum;

    ParallelFibTask1(
        uint32_t fibN,
        uint64_t* sum)
        : fibN(fibN)
        , sum(sum) {}

    static Task* taskFunc(Task* pThisTask, TaskContext const& ctx)
    {
        // Unpack the data.
        ParallelFibTask1& data = *(ParallelFibTask1*)pThisTask->getData();

        uint32_t fibN = data.fibN;
        uint64_t* sum = data.sum;
        

        // Recursively fork tasks until the base case is reached, then join
        // the task sums to produce the Fibonacci number.

        if (data.fibN <= 2)
        {
            *sum = 1;
        }
        else
        {
            // We are going to fork 2 new tasks, one for f(n-1) and one for
            // f(n-2), so we need to add them as reference to this task. If we
            // forget to do this, this task will exit and be destroyed leaving
            // the new children dangling.
            pThisTask->addRef(2, gts::memory_order::relaxed);
            // Doing this in bulk up front also let's us avoid the expensive 
            // cache synchronization of gts::memory_order::seq_cst.
            
            // Fork f(n-1):

            // Create and init the task
            uint64_t sumLeft;
            Task* pLeftChild = ctx.pMicroScheduler->allocateTask(ParallelFibTask1::taskFunc);
            pLeftChild->emplaceData<ParallelFibTask1>(fibN - 1, &sumLeft);
            // Add the task as a child of this task.
            pThisTask->addChildTask(pLeftChild);
            // Queue it for execution.
            ctx.pMicroScheduler->spawnTask(pLeftChild);

            // Fork f(n-2):

            // Create and init the task
            uint64_t sumRight;
            Task* pRightChild = ctx.pMicroScheduler->allocateTask(ParallelFibTask1::taskFunc);
            pRightChild->emplaceData<ParallelFibTask1>(fibN - 2, &sumRight);
            // Add the task as a child of this task.
            pThisTask->addChildTask(pRightChild);
            // Queue it for execution.
            ctx.pMicroScheduler->spawnTask(pRightChild);

            // Wait for the forked children to finish, i.e. join.
            // NOTE: While this is a simple way to join, it has a latency
            // problem. Since wait will execute other tasks until the children
            // are complete, the child may complete well before this thread
            // finishes helping with other tasks.
            pThisTask->waitForChildren(ctx);

            // Calculate the fib number for this task.
            *sum = sumLeft + sumRight;
        }

        return nullptr;
    }
};

//------------------------------------------------------------------------------
void explicitForkJoin(uint32_t fibN)
{
    // Init boilerplate
    WorkerPool workerPool;
    bool result = workerPool.initialize();
    GTS_ASSERT(result);
    MicroScheduler taskScheduler;
    result = taskScheduler.initialize(&workerPool);
    GTS_ASSERT(result);

    uint64_t fibVal = 0;

    // Create the fib task.
    Task* pTask = taskScheduler.allocateTask(ParallelFibTask1::taskFunc);
    pTask->emplaceData<ParallelFibTask1>(fibN, &fibVal);

    // Queue and wait for the task to complete.
    taskScheduler.spawnTaskAndWait(pTask);

    // NOTE: wait ^^^^ does NOT mean that this thread is blocked. This thread will
    // actively execute any tasks in the scheduler until pTask completes.

    std::cout << "Fib " << fibN << " is: " << fibVal << std::endl;

    taskScheduler.shutdown();
    workerPool.shutdown();
}

} // namespace gts_examples
