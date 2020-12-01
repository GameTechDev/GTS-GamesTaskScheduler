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

// Builds on 2_blocking_join

//------------------------------------------------------------------------------
// A task that explicitly represent a join.
struct ParallelFibContinuationTask : public Task
{
    ParallelFibContinuationTask(
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
struct ParallelFibTask2 : public Task
{
    uint32_t fibN;
    uint64_t* sum;

    ParallelFibTask2(
        uint32_t fibN,
        uint64_t* sum)
        : fibN(fibN)
        , sum(sum) {}

    virtual Task* execute(gts::TaskContext const& ctx) override
    {
        if (fibN <= 2)
        {
            *sum = 1;
        }
        else
        {
            // Create the continuation task with the join function.
            ParallelFibContinuationTask* pContinuationTask =
                ctx.pMicroScheduler->allocateTask<ParallelFibContinuationTask>(0, 0, sum);

            // We now replace pThisTask in the task graph with the continuation task.
            /*
                              ----------       Child_R
                             /          \    /
                <---Cont <--  pThisTask   ---
                                             \
                                               Child_L
            */
            setContinuationTask(pContinuationTask);

            // This time add the ref count to the continuation.
            pContinuationTask->addRef(2, gts::memory_order::relaxed);
            
            // Fork f(n-1)
            Task* pLeftChild = ctx.pMicroScheduler->allocateTask<ParallelFibTask2>(fibN - 1, &pContinuationTask->l);
            // Add the task to the continuation.
            pContinuationTask->addChildTaskWithoutRef(pLeftChild);
            ctx.pMicroScheduler->spawnTask(pLeftChild);

            // Fork f(n-2)
            Task* pRightChild = ctx.pMicroScheduler->allocateTask<ParallelFibTask2>(fibN - 2, &pContinuationTask->r);
            // Add the task to the continuation.
            pContinuationTask->addChildTaskWithoutRef(pRightChild);
            ctx.pMicroScheduler->spawnTask(pRightChild);

            // We don't have to explicitly wait for the left and right children
            // to join as before. Now the last child to complete will automatically
            // join by executing the continuation task. 
        }

        return nullptr;
    }
};

//------------------------------------------------------------------------------
void continuationJoin(uint32_t fibN)
{
    printf ("================\n");
    printf ("continuationJoin\n");
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
    Task* pTask = microScheduler.allocateTask<ParallelFibTask2>(fibN, &fibVal);

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
