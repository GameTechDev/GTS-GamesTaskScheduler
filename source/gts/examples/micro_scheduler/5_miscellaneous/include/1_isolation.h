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
 #include <iostream>

#include "gts/platform/Atomic.h"
#include "gts/analysis/Instrumenter.h"
#include "gts/analysis/ConcurrentLogger.h"

#include "gts/micro_scheduler/WorkerPool.h"
#include "gts/micro_scheduler/MicroScheduler.h"
#include "gts/micro_scheduler/patterns/ParallelFor.h"
#include "gts/micro_scheduler/patterns/BlockedRange1d.h"

using namespace gts;

namespace gts_examples {

//------------------------------------------------------------------------------
void noTaskIsolation()
{
    const uint32_t elementCount = 1000;

    WorkerPool workerPool;
    workerPool.initialize(gts::Thread::getHardwareThreadCount());

    MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    // Create a value per thread.
    std::vector<uint32_t> threadLocalValues(gts::Thread::getHardwareThreadCount(), 0);

    ParallelFor parallelFor(taskScheduler);
    parallelFor(
        BlockedRange1d<uint32_t>(0, elementCount, 1),
        [elementCount](BlockedRange1d<uint32_t>& range, void* pData, TaskContext const& ctx)
        {
            std::vector<uint32_t>& threadLocalValues = *(std::vector<uint32_t>*)pData;
            threadLocalValues[ctx.workerIndex] = range.begin();

            // While this thread waits for all inner parallelFor iterations to
            // finish, it is free to start executing iterations from the outer
            // parallelFor.
            ParallelFor parallelFor(*ctx.pMicroScheduler);
            parallelFor(
                BlockedRange1d<uint32_t>(0, elementCount, 1),
                [](BlockedRange1d<uint32_t>&, void*, TaskContext const&)
                {},
                StaticPartitioner(),
                nullptr
            );

            // Check if an outer loop iteration was executed.
            if (threadLocalValues[ctx.workerIndex] != range.begin())
            {
                std::cout << "Non-isolated: Executed an outer loop iteration will waiting on inner loop.\n";
            }
        },
        StaticPartitioner(),
        &threadLocalValues);

    taskScheduler.shutdown();
}

//------------------------------------------------------------------------------
void taskIsolation()
{
    const uint32_t elementCount = 1000;

    WorkerPool workerPool;
    workerPool.initialize(gts::Thread::getHardwareThreadCount());

    MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    // Create a value per thread.
    std::vector<uint32_t> threadLocalValues(gts::Thread::getHardwareThreadCount(), 0);

    ParallelFor parallelFor(taskScheduler);
    parallelFor(
        BlockedRange1d<uint32_t>(0, elementCount, 1),
        [elementCount](BlockedRange1d<uint32_t>& range, void* pData, TaskContext const& ctx)
        {
            std::vector<uint32_t>& threadLocalValues = *(std::vector<uint32_t>*)pData;
            threadLocalValues[ctx.workerIndex] = range.begin();

            // Using the isolate feature, the inner parallelFor cannot execute
            // task spawned outside the scope of the isolation lambda.
            ctx.pMicroScheduler->isolate([&ctx, elementCount]()
            {
                ParallelFor parallelFor(*ctx.pMicroScheduler);
                parallelFor(
                    BlockedRange1d<uint32_t>(0, elementCount, 1),
                    [](BlockedRange1d<uint32_t>&, void*, TaskContext const&)
                    {},
                    StaticPartitioner(),
                    nullptr
                );
            });

            // Will never print.
            if (threadLocalValues[ctx.workerIndex] != range.begin())
            {
                std::cout << "Isolated: Executed an outer loop iteration will waiting on inner loop.\n";
            }
        },
        StaticPartitioner(),
        &threadLocalValues);

    taskScheduler.shutdown();
}

} // gts_examples
