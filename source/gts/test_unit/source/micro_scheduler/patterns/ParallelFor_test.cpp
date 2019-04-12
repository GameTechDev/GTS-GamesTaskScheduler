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

#include "gts/platform/Thread.h"
#include "gts/analysis/Instrumenter.h"
#include "gts/analysis/ConcurrentLogger.h"

#include "gts/micro_scheduler/WorkerPool.h"
#include "gts/micro_scheduler/MicroScheduler.h"
#include "gts/micro_scheduler/patterns/ParallelFor.h"
#include "gts/micro_scheduler/patterns/Partitioners.h"
#include "gts/micro_scheduler/patterns/BlockedRange1d.h"
#include "gts/micro_scheduler/patterns/BlockedRange2d.h"

#include "SchedulerTestsCommon.h"

using namespace gts;

namespace testing {

//------------------------------------------------------------------------------
template<typename TPartitioner>
void ParallelFor1D(size_t elementCount, size_t tileSize)
{
    WorkerPool workerPool;
    workerPool.initialize();

    MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    ParallelFor parallelFor(taskScheduler);

    // Create a 2D array of 0s.
    std::vector<char> vec;
    vec.resize(ELEMENT_COUNT, 0);

    for (uint32_t ii = 0; ii < ITERATIONS_CONCUR; ++ii)
    {
        GTS_CONCRT_LOGGER_RESET();

        parallelFor(
            BlockedRange1d<size_t>(0, elementCount, tileSize),
            [](BlockedRange1d<size_t>& range, void* pData, TaskContext const&)
            {
                std::vector<char>& mtx = *(std::vector<char>*)pData;
                for (size_t jj = range.begin(); jj != range.end(); ++jj)
                {
                    mtx[jj]++;
                }
            },
            TPartitioner(),
            &vec);

        // Validate that all values have been incremented only once.
        for (size_t jj = 0; jj < vec.size(); ++jj)
        {
            ASSERT_EQ((size_t)vec[jj], ii + 1);
        }
    }

    taskScheduler.shutdown();
}

//------------------------------------------------------------------------------
TEST(ParallelFor, Explicit1D)
{
    ParallelFor1D<StaticPartitioner>(ELEMENT_COUNT, TILE_SIZE);
}

//------------------------------------------------------------------------------
TEST(ParallelFor, Adaptive1D)
{
    ParallelFor1D<AdaptivePartitioner>(ELEMENT_COUNT, 1);
}

//------------------------------------------------------------------------------
template<typename TPartitioner>
void ParallelForLambdaClosure1D(size_t elementCount, size_t tileSize)
{
    WorkerPool workerPool;
    workerPool.initialize();

    MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    ParallelFor parallelFor(taskScheduler);

    // Create a 2D array of 0s.
    std::vector<char> vec;
    vec.resize(ELEMENT_COUNT, 0);

    for (uint32_t ii = 0; ii < ITERATIONS_CONCUR; ++ii)
    {
        GTS_CONCRT_LOGGER_RESET();

        parallelFor(
            BlockedRange1d<size_t>(0, elementCount, tileSize),
            [&vec](BlockedRange1d<size_t>& range, void*, TaskContext const&)
            {
                for (size_t jj = range.begin(); jj != range.end(); ++jj)
                {
                    vec[jj]++;
                }
            },
            TPartitioner(),
            nullptr);

        // Validate that all values have been incremented only once.
        for (size_t jj = 0; jj < vec.size(); ++jj)
        {
            ASSERT_EQ((size_t)vec[jj], ii + 1);
        }
    }

    taskScheduler.shutdown();
}

//------------------------------------------------------------------------------
TEST(ParallelFor, ParallelForLambdaClosure)
{
    ParallelForLambdaClosure1D<StaticPartitioner>(ELEMENT_COUNT, TILE_SIZE);
}

//------------------------------------------------------------------------------
void ParallelForSimple1D(size_t elementCount)
{
    WorkerPool workerPool;
    workerPool.initialize();

    MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    ParallelFor parallelFor(taskScheduler);

    // Create a 2D array of 0s.
    std::vector<char> vec;
    vec.resize(ELEMENT_COUNT, 0);

    for (uint32_t ii = 0; ii < ITERATIONS_CONCUR; ++ii)
    {
        GTS_CONCRT_LOGGER_RESET();

        parallelFor(size_t(0), elementCount, [&vec](size_t it) { vec[it]++; });

        // Validate that all values have been incremented only once.
        for (size_t jj = 0; jj < vec.size(); ++jj)
        {
            ASSERT_EQ((size_t)vec[jj], ii + 1);
        }
    }

    taskScheduler.shutdown();
}

//------------------------------------------------------------------------------
TEST(ParallelFor, ParallelForSimple)
{
    ParallelForSimple1D(ELEMENT_COUNT);
}

//------------------------------------------------------------------------------
template<typename TPartitioner>
void ParallelFor2D(size_t elementCount, size_t tileSize)
{
    WorkerPool workerPool;
    workerPool.initialize();

    MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    ParallelFor parallelFor(taskScheduler);

    // Create a 2D array of 0s.
    std::vector<std::vector<char>> matrix;
    matrix.resize(elementCount);
    for (size_t ii = 0; ii < elementCount; ++ii)
    {
        matrix[ii].resize(elementCount, 0);
    }

    for (uint32_t ii = 0; ii < ITERATIONS_CONCUR; ++ii)
    {
        GTS_CONCRT_LOGGER_RESET();

        BlockedRange2d<size_t, size_t> range(0, elementCount, tileSize, 0, elementCount, tileSize);

        parallelFor(
            range,
            [](BlockedRange2d<size_t, size_t>& range, void* data, TaskContext const&)
            {
                std::vector<std::vector<char>>& matrix = *(std::vector<std::vector<char>>*)data;
                for (auto ii = range.range0().begin(); ii != range.range0().end(); ++ii)
                {
                    std::vector<char>& row = matrix[ii];
                    for (auto jj = range.range1().begin(); jj != range.range1().end(); ++jj)
                    {
                        row[jj]++;
                    }
                }
            },
            TPartitioner(),
            &matrix);

        // Validate that all values have been incremented only once.
        for (size_t jj = 0; jj < matrix.size(); ++jj)
        {
            for (size_t kk = 0; kk < matrix[jj].size(); ++kk)
            {
                ASSERT_EQ((size_t)matrix[jj][kk], ii + 1);
            }
        }
    }

    taskScheduler.shutdown();
}

//------------------------------------------------------------------------------
TEST(ParallelFor, Explicit2D)
{
    ParallelFor2D<StaticPartitioner>(ELEMENT_COUNT, TILE_SIZE);
}

//------------------------------------------------------------------------------
TEST(ParallelFor, Adaptive2D)
{
    ParallelFor2D<AdaptivePartitioner>(ELEMENT_COUNT, 1);
}


//------------------------------------------------------------------------------
// Discover if there are any issues around tasking having non-uniform durations.
template<typename TPartitioner>
void ParallelForRandomized1D(size_t elementCount, size_t tileSize)
{
    WorkerPool workerPool;
    workerPool.initialize();

    MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    ParallelFor parallelFor(taskScheduler);

    for (uint32_t ii = 0; ii < ITERATIONS_CONCUR; ++ii)
    {
        GTS_CONCRT_LOGGER_RESET();

        std::vector<uint32_t> countPerThread(taskScheduler.workerCount(), 0);

        parallelFor(
            BlockedRange1d<size_t>(0, elementCount, tileSize),
            [](BlockedRange1d<size_t>& r, void* data, TaskContext const& ctx)
            {
                std::vector<uint32_t>& countPerThread = *(std::vector<uint32_t>*)data;
                for (auto ii = r.begin(); ii != r.end(); ++ii)
                {
                    randomSleep();
                    countPerThread[ctx.workerIndex]++;
                }
            },
            TPartitioner(),
            &countPerThread);

        // Validate that all values have been incremented only once.
        uint32_t total = 0;
        for (size_t jj = 0; jj < taskScheduler.workerCount(); ++jj)
        {
            total += countPerThread[jj];
        }

        ASSERT_EQ(total, elementCount);
    }

    taskScheduler.shutdown();
}

//------------------------------------------------------------------------------
TEST(ParallelFor, ExplicitRandomized1D)
{
    ParallelForRandomized1D<StaticPartitioner>(ELEMENT_COUNT / 2, TILE_SIZE);
}

//------------------------------------------------------------------------------
TEST(ParallelFor, AdaptiveRandomized1D)
{
    ParallelForRandomized1D<AdaptivePartitioner>(ELEMENT_COUNT / 2, 1);
}

} // namespace testing
