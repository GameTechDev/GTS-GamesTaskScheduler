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
#include <vector>
#include <iostream>

#include "gts/platform/Thread.h"

#include "gts/micro_scheduler/WorkerPool.h"
#include "gts/micro_scheduler/MicroScheduler.h"
#include "gts/micro_scheduler/patterns/ParallelFor.h"
#include "gts/micro_scheduler/patterns/Range1d.h"

using namespace gts;

namespace gts_examples {

void isolationInit()
{
    printf ("================\n");
    printf ("isolationInit\n");
    printf ("================\n");


    // Another way to view MicroSchedulers are as isolated parallel regions.
    // Each microScheduler runs on the same workerPool, but their tasks are
    // executed in isolation. This is achieved internally using a round robin
    // strategy for each Worker in workerPool. When the currently executing
    // Schedule on the Worker fails to find work, the Worker moves onto the
    // next available Schedule.

    WorkerPool workerPool;
    workerPool.initialize();

    // Let say we need two isolated regions.
    constexpr uint32_t NUM_SCHEDUELRS = 2;
    MicroScheduler microScheduler[NUM_SCHEDUELRS];

    // Attach each scheduler to the same WorkerPool.
    for (uint32_t iScheduler = 0; iScheduler < NUM_SCHEDUELRS; ++iScheduler)
    {
        microScheduler[iScheduler].initialize(&workerPool);
    }

    //
    // Isolated workload:

    const uint32_t elementCount = 1000;

    // Create a value per thread.
    std::vector<uint32_t> threadLocalValues(gts::Thread::getHardwareThreadCount(), 0);

    ParallelFor parallelFor(microScheduler[0]);
    parallelFor(
        Range1d<uint32_t>(0, elementCount, 1),
        [&](Range1d<uint32_t>& range, void* pData, TaskContext const& ctx)
        {
            std::vector<uint32_t>& threadLocalValues = *(std::vector<uint32_t>*)pData;
            threadLocalValues[ctx.workerId.localId()] = range.begin();

            // Using the isolate feature, the inner parallelFor cannot execute
            // task spawned outside the scope of the isolation lambda.
            ParallelFor parallelFor(microScheduler[1]);
            parallelFor(
                Range1d<uint32_t>(0, elementCount, 1),
                [](Range1d<uint32_t>&, void*, TaskContext const&)
                {},
                StaticPartitioner(),
                nullptr
            );

            // Will never print.
            if (threadLocalValues[ctx.workerId.localId()] != range.begin())
            {
                std::cout << "Isolated: Executed an outer loop iteration will waiting on inner loop.\n";
            }
        },
        StaticPartitioner(),
        &threadLocalValues);

    // If many isolated regions are needed, we recommend creating a pool instead
    // of frequently creating and destroying MicroSchedulers.
}

} // namespace gts_examples
