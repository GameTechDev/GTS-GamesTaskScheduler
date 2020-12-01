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
#include "gts/analysis/Trace.h"
#include "gts/analysis/ConcurrentLogger.h"

#include "gts/micro_scheduler/WorkerPool.h"
#include "gts/micro_scheduler/MicroScheduler.h"
#include "gts/micro_scheduler/patterns/ParallelFor.h"
#include "gts/micro_scheduler/patterns/Partitioners.h"
#include "gts/micro_scheduler/patterns/Range1d.h"
#include "gts/micro_scheduler/patterns/KdRange2d.h"
#include "gts/micro_scheduler/patterns/QuadRange.h"
#include "gts/micro_scheduler/patterns/KdRange3d.h"
#include "gts/micro_scheduler/patterns/OctRange.h"

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
    vec.resize(elementCount, 0);

    for (uint32_t ii = 0; ii < ITERATIONS_CONCUR; ++ii)
    {
        GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);

        parallelFor(
            Range1d<size_t>(0, elementCount, tileSize),
            [](Range1d<size_t>& range, void* pData, TaskContext const&)
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
TEST(ParallelFor, Simple1D)
{
    ParallelFor1D<SimplePartitioner>(ELEMENT_COUNT, TILE_SIZE);
}

//------------------------------------------------------------------------------
TEST(ParallelFor, Static1D)
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
    vec.resize(elementCount, 0);

    for (uint32_t ii = 0; ii < ITERATIONS_CONCUR; ++ii)
    {
        GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);

        parallelFor(
            Range1d<size_t>(0, elementCount, tileSize),
            [&vec](Range1d<size_t>& range, void*, TaskContext const&)
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
    ParallelForLambdaClosure1D<SimplePartitioner>(ELEMENT_COUNT, TILE_SIZE);
}

struct IncrementValuesFunctor
{
    IncrementValuesFunctor(std::vector<char>& vec) : vec(vec)
    {}

    void operator()(Range1d<size_t>& range, void*, TaskContext const&)
    {
        for (size_t jj = range.begin(); jj != range.end(); ++jj)
        {
            vec[jj]++;
        }
    }

    std::vector<char>& vec;
};

//------------------------------------------------------------------------------
template<typename TPartitioner>
void ParallelForFunctor1D(size_t elementCount, size_t tileSize)
{
    WorkerPool workerPool;
    workerPool.initialize();

    MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    ParallelFor parallelFor(taskScheduler);

    // Create a 2D array of 0s.
    std::vector<char> vec;
    vec.resize(elementCount, 0);

    for (uint32_t ii = 0; ii < ITERATIONS_CONCUR; ++ii)
    {
        GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);

        parallelFor(
            Range1d<size_t>(0, elementCount, tileSize),
            IncrementValuesFunctor(vec),
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
TEST(ParallelFor, ParallelForFunctor1D)
{
    ParallelForFunctor1D<SimplePartitioner>(ELEMENT_COUNT, TILE_SIZE);
}

//------------------------------------------------------------------------------
TEST(ParallelFor, Convenience1D)
{
    // Convenience

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
        GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);

        parallelFor(0u, ELEMENT_COUNT, [&vec](size_t ii) {
            vec[ii]++;
        });

        // Validate that all values have been incremented only once.
        for (size_t jj = 0; jj < vec.size(); ++jj)
        {
            ASSERT_EQ((size_t)vec[jj], ii + 1);
        }
    }

    taskScheduler.shutdown();
}

//------------------------------------------------------------------------------
template<typename TRange, typename TPartitioner>
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
        GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);

        TRange range(0, elementCount, tileSize, 0, elementCount, tileSize);

        parallelFor(
            range,
            [](TRange& range, void* data, TaskContext const&)
            {
                std::vector<std::vector<char>>& matrix = *(std::vector<std::vector<char>>*)data;
                for (auto ii = range.yRange().begin(); ii != range.yRange().end(); ++ii)
                {
                    std::vector<char>& row = matrix[ii];
                    for (auto jj = range.xRange().begin(); jj != range.xRange().end(); ++jj)
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
TEST(ParallelFor, KdSimple2D)
{
    ParallelFor2D<KdRange2d<size_t>, SimplePartitioner>(ELEMENT_COUNT, TILE_SIZE);
}

//------------------------------------------------------------------------------
TEST(ParallelFor, KdSStatic2D)
{
    ParallelFor2D<KdRange2d<size_t>,StaticPartitioner>(ELEMENT_COUNT, TILE_SIZE);
}

//------------------------------------------------------------------------------
TEST(ParallelFor, KdSAdaptive2D)
{
    ParallelFor2D<KdRange2d<size_t>,AdaptivePartitioner>(ELEMENT_COUNT, TILE_SIZE);
}

//------------------------------------------------------------------------------
TEST(ParallelFor, QuadSimple2D)
{
    ParallelFor2D<QuadRange<size_t>, SimplePartitioner>(ELEMENT_COUNT, TILE_SIZE);
}

//------------------------------------------------------------------------------
TEST(ParallelFor, QuadStatic2D)
{
    ParallelFor2D<QuadRange<size_t>, StaticPartitioner>(ELEMENT_COUNT, TILE_SIZE);
}

//------------------------------------------------------------------------------
TEST(ParallelFor, QuadAdaptive2D)
{
    ParallelFor2D<QuadRange<size_t>, AdaptivePartitioner>(ELEMENT_COUNT, TILE_SIZE);
}

//------------------------------------------------------------------------------
template<typename TRange, typename TPartitioner>
void ParallelFor3D(size_t elementCount, size_t tileSize)
{
    WorkerPool workerPool;
    workerPool.initialize();

    MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    ParallelFor parallelFor(taskScheduler);

    // Create a 3D array of 0s.
    std::vector<std::vector<std::vector<char>>> volume;
    volume.resize(elementCount);
    for (size_t ii = 0; ii < elementCount; ++ii)
    {
        volume[ii].resize(elementCount);
        for (size_t jj = 0; jj < elementCount; ++jj)
        {
            volume[ii][jj].resize(elementCount, 0);
        }
    }

    for (uint32_t iter = 0; iter < ITERATIONS_CONCUR; ++iter)
    {
        GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);

        TRange range(0, elementCount, tileSize, 0, elementCount, tileSize, 0, elementCount, tileSize);

        parallelFor(
            range,
            [](TRange& range, void* data, TaskContext const&)
            {
                std::vector<std::vector<std::vector<char>>>& volume =
                    *(std::vector<std::vector<std::vector<char>>>*)data;

                for (auto ii = range.yRange().begin(); ii != range.yRange().end(); ++ii)
                {
                    for (auto jj = range.xRange().begin(); jj != range.xRange().end(); ++jj)
                    {
                        for (auto kk = range.zRange().begin(); kk != range.zRange().end(); ++kk)
                        {
                            volume[ii][jj][kk]++;
                        }
                    }
                }
            },
            TPartitioner(),
            &volume);

        // Validate that all values have been incremented only once.
        for (auto ii = range.yRange().begin(); ii != range.yRange().end(); ++ii)
        {
            for (auto jj = range.xRange().begin(); jj != range.xRange().end(); ++jj)
            {
                for (auto kk = range.zRange().begin(); kk != range.zRange().end(); ++kk)
                {
                    ASSERT_EQ((size_t)volume[ii][jj][kk], iter + 1);
                }
            }
        }
    }

    taskScheduler.shutdown();
}

//------------------------------------------------------------------------------
TEST(ParallelFor, KdSimple3D)
{
    ParallelFor3D<KdRange3d<size_t>, SimplePartitioner>(ELEMENT_COUNT / 4, TILE_SIZE);
}

//------------------------------------------------------------------------------
TEST(ParallelFor, KdSStatic3D)
{
    ParallelFor3D<KdRange3d<size_t>,StaticPartitioner>(ELEMENT_COUNT / 4, TILE_SIZE);
}

//------------------------------------------------------------------------------
TEST(ParallelFor, KdSAdaptive3D)
{
    ParallelFor3D<KdRange3d<size_t>,AdaptivePartitioner>(ELEMENT_COUNT / 4, TILE_SIZE);
}

//------------------------------------------------------------------------------
TEST(ParallelFor, OctSimple3D)
{
    ParallelFor3D<OctRange<size_t>, SimplePartitioner>(ELEMENT_COUNT / 4, TILE_SIZE);
}

//------------------------------------------------------------------------------
TEST(ParallelFor, OctStatic3D)
{
    ParallelFor3D<OctRange<size_t>, StaticPartitioner>(ELEMENT_COUNT / 4, TILE_SIZE);
}

//------------------------------------------------------------------------------
TEST(ParallelFor, OctAdaptive3D)
{
    ParallelFor3D<OctRange<size_t>, AdaptivePartitioner>(ELEMENT_COUNT / 4, TILE_SIZE);
}

} // namespace testing
