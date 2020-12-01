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
#include <chrono>

#include <gts/micro_scheduler/WorkerPool.h>
#include <gts/micro_scheduler/MicroScheduler.h>

using namespace gts;

namespace gts_examples {

// Builds on 4_scheduler_bypassing

//------------------------------------------------------------------------------
// A task that explicitly represent a join.
struct ParallelFibContinuationTask4 : public Task
{
    ParallelFibContinuationTask4(
        uint32_t l,
        uint32_t r,
        uint64_t* sum)
        : l(l)
        , r(r)
        , sum(sum) {}

    uint64_t l;
    uint64_t r;
    uint64_t* sum;

    virtual Task* execute(gts::TaskContext const&) override
    {
        *(sum) = l + r;
        return nullptr;
    }
};

//------------------------------------------------------------------------------
struct ParallelFibTask4 : public Task
{
    uint32_t fibN;
    uint64_t* sum;

    ParallelFibTask4(
        uint32_t fibN,
        uint64_t* sum)
        : fibN(fibN)
        , sum(sum) {}

    virtual Task* execute(gts::TaskContext const& ctx) override
    {
        if (fibN <= 2)
        {
            *sum = 1;
            return nullptr;
        }
        else
        {
            // Create the continuation task with the join function.
            ParallelFibContinuationTask4* pContinuationTask = ctx.pMicroScheduler->allocateTask<ParallelFibContinuationTask4>(0, 0, sum);
            pContinuationTask->addRef(2, gts::memory_order::relaxed);
            setContinuationTask(pContinuationTask);
            
            // Fork f(n-1)
            Task* pLeftChild = ctx.pMicroScheduler->allocateTask<ParallelFibTask4>(fibN - 1, &pContinuationTask->l);
            pContinuationTask->addChildTaskWithoutRef(pLeftChild);
            ctx.pMicroScheduler->spawnTask(pLeftChild);

            // Fork f(n-2)

            // Since the right task has exactly the same form as a this task,
            // we can simply reuse it and skip the allocator!
            recycle();
            pContinuationTask->addChildTaskWithoutRef(this);

            // Reset the values for the right child.
            fibN -= 2;
            sum = &pContinuationTask->r;

            // Now we just bypass with this task!
            return this;
        }
    }
};

//------------------------------------------------------------------------------
void taskRecycling(uint32_t fibN)
{
    printf ("================\n");
    printf ("taskRecycling\n");
    printf ("================\n");

    // Init boilerplate
    WorkerPool workerPool;
    bool result = workerPool.initialize();
    GTS_ASSERT(result);
    MicroScheduler microScheduler;
    result = microScheduler.initialize(&workerPool);
    GTS_ASSERT(result);

    uint64_t fibVal = 0;

    auto start = std::chrono::high_resolution_clock::now();

    // Create the fib task.
    Task* pTask = microScheduler.allocateTask<ParallelFibTask4>(fibN, &fibVal);

    // Queue and wait for the task to complete.
    microScheduler.spawnTaskAndWait(pTask);
    // NOTE: wait ^^^^ does NOT mean that this thread is idle. This thread will
    // actively execute any tasks in the scheduler until pTask completes.

    auto end = std::chrono::high_resolution_clock::now();

    std::cout << "Fib " << fibN << " is: " << fibVal << std::endl;
    std::cout << "Time (ms): " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << std::endl;

    microScheduler.shutdown();
    workerPool.shutdown();
}

} // namespace gts_examples
