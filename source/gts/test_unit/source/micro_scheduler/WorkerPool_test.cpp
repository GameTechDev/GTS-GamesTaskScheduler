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
#include "gts/platform/Thread.h"
#include "gts/micro_scheduler/WorkerPool.h"

#include "SchedulerTestsCommon.h"

using namespace gts;

namespace testing {

//------------------------------------------------------------------------------
TEST(WorkerPool, Constructor)
{
    WorkerPool workerPool;
}

//------------------------------------------------------------------------------
TEST(WorkerPool, InitAndShutdown)
{
    for (uint32_t iters = 0; iters < ITERATIONS_STRESS; ++iters)
    {
        const uint32_t workerCount = gts::Thread::getHardwareThreadCount();

        WorkerPool workerPool;

        ASSERT_FALSE(workerPool.isRunning());

        workerPool.initialize(workerCount);

        ASSERT_TRUE(workerPool.isRunning());
        ASSERT_EQ(workerCount, workerPool.workerCount());

        workerPool.shutdown();
    }
}

//------------------------------------------------------------------------------
TEST(WorkerPool, makePartition)
{
    for (uint32_t iters = 0; iters < ITERATIONS_STRESS; ++iters)
    {
        const uint32_t workerCount = gts::Thread::getHardwareThreadCount();
        ASSERT_TRUE(workerCount > 1);

        WorkerPool workerPool;

        workerPool.initialize(workerCount);

        Vector<uint32_t> indices;
        indices.push_back(1);
        WorkerPool* pParition = workerPool.makePartition(indices);
        ASSERT_TRUE(pParition->isRunning());
        ASSERT_EQ(pParition->workerCount(), 1U);

        workerPool.shutdown();
    }
}

//------------------------------------------------------------------------------
TEST(WorkerPool, paritionMainThreadError)
{
    const uint32_t workerCount = gts::Thread::getHardwareThreadCount();
    ASSERT_TRUE(workerCount > 1);

    WorkerPool workerPool;

    std::vector<WorkerThreadDesc> descs(workerCount);
    workerPool.initialize(workerCount);

    EXPECT_DEATH(
        Vector<uint32_t> indices;
        indices.push_back(0);
        workerPool.makePartition(indices);
    , "");

    workerPool.shutdown();
}

//------------------------------------------------------------------------------
TEST(WorkerPool, shutdownPartitionError)
{
    const uint32_t workerCount = gts::Thread::getHardwareThreadCount();
    ASSERT_TRUE(workerCount > 1);

    WorkerPool workerPool;

    workerPool.initialize(workerCount);

    Vector<uint32_t> indices;
    indices.push_back(1);
    WorkerPool* pParition = workerPool.makePartition(indices);
    ASSERT_TRUE(pParition->isRunning());
    ASSERT_EQ(pParition->workerCount(), 1U);

    EXPECT_DEATH(
        pParition->shutdown();
    , "");

    workerPool.shutdown();
}

//------------------------------------------------------------------------------
TEST(WorkerPool, overlappedPartitionError)
{
    WorkerPool workerPool;
    workerPool.initialize();

    Vector<uint32_t> indices;
    indices.push_back(1);
    indices.push_back(2);
    WorkerPool* pWorkerPoolPartition1 = workerPool.makePartition(indices);
    GTS_UNREFERENCED_PARAM(pWorkerPoolPartition1);

    EXPECT_DEATH(
        Vector<uint32_t> badIndices;
        badIndices.push_back(2);
        badIndices.push_back(3);
        workerPool.makePartition(badIndices);
    , "");

    workerPool.shutdown();
}

} // namespace testing
