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

// Builds on 4_scheduler_bypassing

//------------------------------------------------------------------------------
// A task that explicitly represent a join.
struct ParallelFibContinuationTask4
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

    static Task* taskFunc(gts::Task* thisTask, gts::TaskContext const&)
    {
        ParallelFibContinuationTask4& data = *(ParallelFibContinuationTask4*)thisTask->getData();
        *(data.sum) = data.l + data.r;
        return nullptr;
    }
};

//------------------------------------------------------------------------------
struct ParallelFibTask4
{
    uint32_t fibN;
    uint64_t* sum;

    ParallelFibTask4(
        uint32_t fibN,
        uint64_t* sum)
        : fibN(fibN)
        , sum(sum) {}

    static Task* taskFunc(Task* pThisTask, TaskContext const& ctx)
    {
        // Unpack the data.
        ParallelFibTask4& data = *(ParallelFibTask4*)pThisTask->getData();

        uint32_t fibN = data.fibN;
        uint64_t* sum = data.sum;

        if (data.fibN <= 2)
        {
            *sum = 1;
            return nullptr;
        }
        else
        {
            // Create the continuation task with the join function.
            Task* pContinuationTask = ctx.pMicroScheduler->allocateTaskRaw(ParallelFibContinuationTask4::taskFunc, sizeof(ParallelFibContinuationTask4));
            ParallelFibContinuationTask4* pContinuationData = pContinuationTask->emplaceData<ParallelFibContinuationTask4>(0, 0, sum);
            pThisTask->setContinuationTask(pContinuationTask);
            pContinuationTask->addRef(2, gts::memory_order::relaxed);
            
            // Fork f(n-1)
            Task* pLeftChild = ctx.pMicroScheduler->allocateTask<ParallelFibTask4>(fibN - 1, &pContinuationData->l);
            pContinuationTask->addChildTaskWithoutRef(pLeftChild);
            ctx.pMicroScheduler->spawnTask(pLeftChild);

            // Fork f(n-2)

            // Since the right task is exactly the same form as a this task,
            // we can simply reuse it and skip the allocator!
            pThisTask->recyleAsChildOf(pContinuationTask);
            // Reset the values for the right child.
            data.fibN -= 2;
            data.sum = &pContinuationData->r;

            // Now we just bypass with this task!
            return pThisTask;
        }
    }
};

//------------------------------------------------------------------------------
void recylingForkJoin(uint32_t fibN)
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
    Task* pTask = taskScheduler.allocateTask<ParallelFibTask4>(fibN, &fibVal);

    // Queue and wait for the task to complete.
    taskScheduler.spawnTaskAndWait(pTask);

    // NOTE: wait ^^^^ does NOT mean that this thread is blocked. This thread will
    // actively execute any tasks in the scheduler until pTask completes.

    std::cout << "Fib " << fibN << " is: " << fibVal << std::endl;

    taskScheduler.shutdown();
    workerPool.shutdown();
}

} // namespace gts_examples
