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
#include <gts/micro_scheduler/patterns/ParallelFor.h>
#include <gts/micro_scheduler/patterns/BlockedRange1d.h>


//------------------------------------------------------------------------------
/**
 * Test the overhead of the Micro-scheduler through an empty parallel-for.
 */
Stats schedulerOverheadParForPerf(uint32_t size, uint32_t iterations, uint32_t threadCount, bool affinitize)
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

    gts::ParallelFor parallelFor(taskScheduler);

    // Do test.
    for (uint32_t ii = 0; ii < iterations; ++ii)
    {
        auto start = std::chrono::high_resolution_clock::now();

        parallelFor.operator()<gts::StaticPartitioner>(0u, size, [](uint32_t) { return nullptr; }, 1);

        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double> diff = end - start;
        stats.addDataPoint(diff.count());
    }

    taskScheduler.shutdown();

    return stats;
}
