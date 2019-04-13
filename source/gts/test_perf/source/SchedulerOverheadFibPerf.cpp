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
struct ParallelFibContinuationTask
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

    static gts::Task* taskFunc(gts::Task* thisTask, gts::TaskContext const&)
    {
        ParallelFibContinuationTask& data = *(ParallelFibContinuationTask*)thisTask->getData();
        *(data.sum) = data.l + data.r;
        return nullptr;
    }
};

//------------------------------------------------------------------------------
struct ParallelFibTask
{
    uint32_t fibN;
    uint64_t* sum;

    ParallelFibTask(
        uint32_t fibN,
        uint64_t* sum)
        : fibN(fibN)
        , sum(sum) {}

    static gts::Task* taskFunc(gts::Task* pThisTask, gts::TaskContext const& ctx)
    {
        // Unpack the data.
        ParallelFibTask& data = *(ParallelFibTask*)pThisTask->getData();

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
            gts::Task* pContinuationTask = ctx.pMicroScheduler->allocateTaskRaw(ParallelFibContinuationTask::taskFunc, sizeof(ParallelFibContinuationTask));
            ParallelFibContinuationTask* pContinuationData = pContinuationTask->emplaceData<ParallelFibContinuationTask>(0, 0, sum);
            pThisTask->setContinuationTask(pContinuationTask);
            pContinuationTask->addRef(2, gts::memory_order::relaxed);

            // Fork f(n-1)
            gts::Task* pLeftChild = ctx.pMicroScheduler->allocateTask<ParallelFibTask>(fibN - 1, &pContinuationData->l);
            pContinuationTask->addChildTaskWithoutRef(pLeftChild);
            ctx.pMicroScheduler->spawnTask(pLeftChild);

            // Fork/recycle f(n-2)

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
/**
 * Test the overhead of the Micro-scheduler through the low-level interface.
 */
Stats schedulerOverheadFibPerf(uint32_t fibN, uint32_t iterations, uint32_t threadCount, bool affinitize)
{
    Stats stats(iterations);

    gts::WorkerPool workerPool;
    if (affinitize)
    {
        gts::WorkerPoolDesc desc;

        gts::CpuTopology topology;
        gts::Thread::getCpuTopology(topology);

        gts::Vector<uintptr_t> affinityMasks;

        for (size_t iThread = 0, threadsPerCore = topology.coreInfo[0].logicalAffinityMasks.size(); iThread < threadsPerCore; ++iThread)
        {
            for (auto& core : topology.coreInfo)
            {
                affinityMasks.push_back(core.logicalAffinityMasks[iThread]);
            }
        }

        for (uint32_t ii = 0; ii < threadCount; ++ii)
        {
            gts::WorkerThreadDesc d;
            d.affinityMask = affinityMasks[ii];
            desc.workerDescs.push_back(d);
        }

        workerPool.initialize(desc);
    }
    else
    {
        workerPool.initialize(threadCount);
    }

    gts::MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    // Do test.
    for (uint32_t ii = 0; ii < iterations; ++ii)
    {
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

    taskScheduler.shutdown();

    return stats;
}
