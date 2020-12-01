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
#include <atomic>
#include <chrono>

#include "gts_perf/Stats.h"

#include <gts/micro_scheduler/WorkerPool.h>
#include <gts/micro_scheduler/MicroScheduler.h>

 //------------------------------------------------------------------------------
 // A task that explicitly represent a join.
struct ParallelFibContinuationTask : public gts::Task
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

    gts::Task* execute(gts::TaskContext const&)
    {
        *sum = l + r;
        return nullptr;
    }
};

//------------------------------------------------------------------------------
struct ParallelFibTask : public gts::Task
{
    uint32_t fibN;
    uint64_t* sum;

    ParallelFibTask(
        uint32_t fibN,
        uint64_t* sum)
        : fibN(fibN)
        , sum(sum) {}

    gts::Task* execute(gts::TaskContext const& ctx)
    {
        if (fibN <= 2)
        {
            *sum = 1;
            return nullptr;
        }
        else
        {
            // Create the continuation task with the join function.
            ParallelFibContinuationTask* pContinuationTask = ctx.pMicroScheduler->allocateTask<ParallelFibContinuationTask>(0, 0, sum);
            setContinuationTask(pContinuationTask);
            pContinuationTask->addRef(2, gts::memory_order::relaxed);

            // Fork f(n-1)
            ParallelFibTask* pLeftChild = ctx.pMicroScheduler->allocateTask<ParallelFibTask>(fibN - 1, &pContinuationTask->l);
            pContinuationTask->addChildTaskWithoutRef(pLeftChild);
            ctx.pMicroScheduler->spawnTask(pLeftChild);

            // Fork/recycle f(n-2)
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
/**
 * Test the overhead of the Micro-scheduler through the low-level interface.
 */
Stats schedulerOverheadFibPerf(gts::MicroScheduler& taskScheduler, uint32_t fibN, uint32_t iterations)
{
    Stats stats(iterations);

    // Do test.
    for (uint32_t ii = 0; ii < iterations; ++ii)
    {
        GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);

        auto start = std::chrono::high_resolution_clock::now();

        uint64_t fibVal = 0;

        // Create the fib task.
        gts::Task* pTask = taskScheduler.allocateTask<ParallelFibTask>(fibN, &fibVal);

        // Queue and wait for the task to complete.
        taskScheduler.spawnTaskAndWait(pTask);

        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double> diff = end - start;
        stats.addDataPoint(diff.count());
    }

    return stats;
}
