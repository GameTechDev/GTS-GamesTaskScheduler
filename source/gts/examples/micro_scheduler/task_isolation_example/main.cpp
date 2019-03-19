/*******************************************************************************
 * Copyright 2017 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License.  You may obtain a copy
 * of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 ******************************************************************************/
#pragma once

#include <atomic>
#include <chrono>

#include <gts/micro_scheduler/WorkerPool.h>
#include <gts/micro_scheduler/MicroScheduler.h>
#include <gts/micro_scheduler/patterns/BlockedRange1d.h>
#include <gts/micro_scheduler/patterns/ParallelFor.h>

const uint32_t ISOLATION_ELEMENT_COUNT = 1000;

//------------------------------------------------------------------------------
void NoTaskIsolation(uint32_t threadCount)
{
    const uint32_t outerElementCount = ISOLATION_ELEMENT_COUNT;

    gts::WorkerPool workerPool;
    workerPool.initialize(threadCount);

    gts::MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    // Create a set of per thread values.
    std::vector<uint32_t> threadLocalNumber(threadCount, 0);

    gts::ParallelFor parallelFor(taskScheduler);
    parallelFor(
        gts::BlockedRange1d<uint32_t>(0, outerElementCount, 1),
        [](gts::BlockedRange1d<uint32_t>& range, void* pData, gts::TaskContext const& ctx)
    {
        std::vector<uint32_t>& threadLocalNumber = *(std::vector<uint32_t>*)pData;

        // Set the per thread value for this iteration of the outer loop.
        threadLocalNumber[ctx.workerIndex] = range.begin();

        // Inner loop is not isolated. If this thread wait, it can pick up an
        // out loop task.
        gts::ParallelFor parallelFor(*ctx.pMicroScheduler);
        parallelFor(
            gts::BlockedRange1d<uint32_t>(0, ISOLATION_ELEMENT_COUNT, 1),
            [](gts::BlockedRange1d<uint32_t>&, void*, gts::TaskContext const&)
            {
                // do cool stuff!
            },
            gts::StaticPartitioner()
            );

        // **** Uncomment to assert when outer loop task got run during the inner loop. ****
        //GTS_FATAL_ASSERT(threadLocalNumber[ctx.workerIndex] == range.begin());
    },
        gts::StaticPartitioner(),
        &threadLocalNumber);

    taskScheduler.shutdown();
}

//------------------------------------------------------------------------------
void TaskIsolation(uint32_t threadCount)
{
    const uint32_t outerElementCount = ISOLATION_ELEMENT_COUNT;

    gts::WorkerPool workerPool;
    workerPool.initialize(threadCount);

    gts::MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    // Create a set of per thread values.
    std::vector<uint32_t> threadLocalNumber(threadCount, 0);

    gts::ParallelFor parallelFor(taskScheduler);
    parallelFor(
        gts::BlockedRange1d<uint32_t>(0, outerElementCount, 1),
        [](gts::BlockedRange1d<uint32_t>& range, void* pData, gts::TaskContext const& ctx)
        {
            std::vector<uint32_t>& threadLocalNumber = *(std::vector<uint32_t>*)pData;

            // Set the per thread value for this iteration of the outer loop.
            threadLocalNumber[ctx.workerIndex] = range.begin();

            ctx.pMicroScheduler->isolate([&ctx]()
            {
                // Run the second parallel loop in an isolated region to prevent
                // the current thread from taking tasks related to the outer parallel loop.
                gts::ParallelFor parallelFor(*ctx.pMicroScheduler);
                parallelFor(
                    gts::BlockedRange1d<uint32_t>(0, ISOLATION_ELEMENT_COUNT, 1),
                    [](gts::BlockedRange1d<uint32_t>&, void*, gts::TaskContext const&)
                    {
                        // do cool stuff!
                    },
                    gts::StaticPartitioner()
                );
            });

            // Verify that no outer loop tasks were executed on this thread
            // while the inner parallel for was executing.
            GTS_FATAL_ASSERT(threadLocalNumber[ctx.workerIndex] == range.begin());
        },
        gts::StaticPartitioner(),
        &threadLocalNumber);

    taskScheduler.shutdown();
}

//------------------------------------------------------------------------------
int main()
{
    NoTaskIsolation(gts::Thread::getHardwareThreadCount());
    TaskIsolation(gts::Thread::getHardwareThreadCount());
    return 0;
}
