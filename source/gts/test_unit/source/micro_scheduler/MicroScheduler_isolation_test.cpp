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
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdlib>

#include "gts/platform/Atomic.h"
#include "gts/analysis/Instrumenter.h"
#include "gts/analysis/ConcurrentLogger.h"

#include "gts/micro_scheduler/WorkerPool.h"
#include "gts/micro_scheduler/MicroScheduler.h"
#include "gts/micro_scheduler/patterns/ParallelFor.h"
#include "gts/micro_scheduler/patterns/BlockedRange1d.h"

#include "SchedulerTestsCommon.h"

using namespace gts;

namespace testing {

const uint32_t ISOLATION_ELEMENT_COUNT = 1000;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// QUEUE TASK TESTS:

//------------------------------------------------------------------------------
void TestTaskIsolation(const uint32_t elementCount, const uint32_t threadCount)
{
    WorkerPool workerPool;
    workerPool.initialize(threadCount);

    MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    // Create a per thread value.
    std::vector<uint32_t> threadLocalNumber(threadCount, 0);

    ParallelFor parallelFor(taskScheduler);
    parallelFor(
        BlockedRange1d<uint32_t>(0, elementCount, 1),
        [](BlockedRange1d<uint32_t>& range, void* pData, TaskContext const& ctx)
        {
            std::vector<uint32_t>& threadLocalNumber = *(std::vector<uint32_t>*)pData;
            threadLocalNumber[ctx.workerIndex] = range.begin();

            ctx.pMicroScheduler->isolate([&ctx]()
            {
                ParallelFor parallelFor(*ctx.pMicroScheduler);
                parallelFor(
                    BlockedRange1d<uint32_t>(0, ISOLATION_ELEMENT_COUNT, 1),
                    [](BlockedRange1d<uint32_t>&, void*, TaskContext const&)
                    {},
                    StaticPartitioner(),
                    nullptr
                );
            });

            ASSERT_EQ(threadLocalNumber[ctx.workerIndex], range.begin());
        },
        StaticPartitioner(),
        &threadLocalNumber);

    taskScheduler.shutdown();
}

//------------------------------------------------------------------------------
TEST(MicroScheduler, testTaskIsolation)
{
    for (uint32_t ii = 0; ii < ITERATIONS_CONCUR; ++ii)
    {
        GTS_CONCRT_LOGGER_RESET();
        TestTaskIsolation(ISOLATION_ELEMENT_COUNT, gts::Thread::getHardwareThreadCount());
    }
}

} // namespace testing
