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
#include "gts/micro_scheduler/patterns/ParallelWavefront.h"
#include "gts/micro_scheduler/patterns/Partitioners.h"
#include "gts/micro_scheduler/patterns/Range1d.h"
#include "gts/micro_scheduler/patterns/KdRange2d.h"
#include "gts/micro_scheduler/patterns/QuadRange.h"
#include "gts/micro_scheduler/patterns/KdRange3d.h"
#include "gts/micro_scheduler/patterns/OctRange.h"

#include "SchedulerTestsCommon.h"

using namespace gts;

namespace testing {

#include <iostream>
#include <sstream>
#include <vector>

#include <algorithm>


using namespace std;

//------------------------------------------------------------------------------
template<typename TRange, typename TPartitioner>
void ParallelWavefront2D(size_t elementCount, size_t tileSize)
{
    WorkerPool workerPool;
    workerPool.initialize();

    MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    ParallelWavefront parallelWavefront(taskScheduler);

    // Create a 2D matrix of 0s.
    std::vector<std::vector<uint64_t>> matrix;
    matrix.resize(elementCount);
    for (size_t ii = 0; ii < elementCount; ++ii)
    {
        matrix[ii].resize(elementCount, 0);
    }

    // Create a 2D matrix of expected values.
    std::vector<std::vector<uint64_t>> expectedMatrix;
    expectedMatrix.resize(elementCount);
    for (size_t ii = 0; ii < elementCount; ++ii)
    {
        expectedMatrix[ii].resize(elementCount, 0);
    }
    expectedMatrix[0][0] = 1;
    for (size_t x = 0; x < elementCount; ++x)
    {
        for (size_t y = 0; y < elementCount; ++y)
        {
            expectedMatrix[x][y] += (x > 0 ? expectedMatrix[x-1][y] : 0) + (y > 0 ? expectedMatrix[x][y-1] : 0);
        }
    }

    for (uint32_t ii = 0; ii < ITERATIONS_CONCUR; ++ii)
    {
        GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);

        matrix[0][0] = 1; // seed

        TRange range(0, elementCount, tileSize, 0, elementCount, tileSize);

        parallelWavefront(
            range,
            [&matrix](TRange& range, void*, TaskContext const&)
            {
                GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::MICRO_SCEHDULER_ALL, analysis::Color::RoyalBlue2, "parallelWavefront.twoD", range.xRange().begin(), range.yRange().begin());
                for (auto x = range.xRange().begin(); x != range.xRange().end(); ++x)
                {
                    for (auto y = range.yRange().begin(); y != range.yRange().end(); ++y)
                    {
                        matrix[x][y] += (x > 0 ? matrix[x-1][y] : 0) + (y > 0 ? matrix[x][y-1] : 0);
                    }
                }
            },
            TPartitioner(),
            nullptr);

        // Validate the values.
        for (size_t x = 0; x < elementCount; ++x)
        {
            for (size_t y = 0; y < elementCount; ++y)
            {
                //if (expectedMatrix[x][y] != matrix[x][y])
                //{
                //    GTS_TRACE_DUMP("fail.txt");
                //}
                ASSERT_EQ(expectedMatrix[x][y], matrix[x][y]);
                matrix[x][y] = 0; // reset
            }
        }
    }

    taskScheduler.shutdown();
}

//------------------------------------------------------------------------------
TEST(ParallelWavefront, KdSimple2D)
{
    ParallelWavefront2D<KdRange2d<size_t>, SimplePartitioner>(ELEMENT_COUNT, TILE_SIZE);
}

////------------------------------------------------------------------------------
//TEST(ParallelWavefront, KdStatic2D)
//{
//    ParallelWavefront2D<KdRange2d<size_t>, StaticPartitioner>(ELEMENT_COUNT, TILE_SIZE);
//}

//------------------------------------------------------------------------------
TEST(ParallelWavefront, KdAdaptive2D)
{
    ParallelWavefront2D<KdRange2d<size_t>, AdaptivePartitioner>(ELEMENT_COUNT, TILE_SIZE);
}

////------------------------------------------------------------------------------
TEST(ParallelWavefront, QuadSimple2D)
{
    ParallelWavefront2D<QuadRange<size_t>, SimplePartitioner>(ELEMENT_COUNT, TILE_SIZE);
}

//------------------------------------------------------------------------------
TEST(ParallelWavefront, QuadStatic2D)
{
    ParallelWavefront2D<QuadRange<size_t>, StaticPartitioner>(ELEMENT_COUNT, TILE_SIZE);
}

//------------------------------------------------------------------------------
TEST(ParallelWavefront, QuadAdaptive2D)
{
    ParallelWavefront2D<QuadRange<size_t>, AdaptivePartitioner>(ELEMENT_COUNT, TILE_SIZE);
}

////------------------------------------------------------------------------------
//template<typename TRange, typename TPartitioner>
//void ParallelFor3D(size_t elementCount, size_t tileSize)
//{
//    WorkerPool workerPool;
//    workerPool.initialize();
//
//    MicroScheduler taskScheduler;
//    taskScheduler.initialize(&workerPool);
//
//    ParallelWavefront parallelFor(taskScheduler);
//
//    // Create a 3D array of 0s.
//    std::vector<std::vector<std::vector<char>>> volume;
//    volume.resize(elementCount);
//    for (size_t ii = 0; ii < elementCount; ++ii)
//    {
//        volume[ii].resize(elementCount);
//        for (size_t jj = 0; jj < elementCount; ++jj)
//        {
//            volume[ii][jj].resize(elementCount, 0);
//        }
//    }
//
//    for (uint32_t iter = 0; iter < ITERATIONS_CONCUR; ++iter)
//    {
//        GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);
//
//        TRange range(0, elementCount, tileSize, 0, elementCount, tileSize, 0, elementCount, tileSize);
//
//        parallelFor(
//            range,
//            [](TRange& range, void* data, TaskContext const&)
//            {
//                std::vector<std::vector<std::vector<char>>>& volume =
//                    *(std::vector<std::vector<std::vector<char>>>*)data;
//
//                for (auto ii = range.yRange().begin(); ii != range.yRange().end(); ++ii)
//                {
//                    for (auto jj = range.xRange().begin(); jj != range.xRange().end(); ++jj)
//                    {
//                        for (auto kk = range.zRange().begin(); kk != range.zRange().end(); ++kk)
//                        {
//                            volume[ii][jj][kk]++;
//                        }
//                    }
//                }
//            },
//            TPartitioner(),
//            &volume);
//
//        // Validate that all values have been incremented only once.
//        for (auto ii = range.yRange().begin(); ii != range.yRange().end(); ++ii)
//        {
//            for (auto jj = range.xRange().begin(); jj != range.xRange().end(); ++jj)
//            {
//                for (auto kk = range.zRange().begin(); kk != range.zRange().end(); ++kk)
//                {
//                    ASSERT_EQ((size_t)volume[ii][jj][kk], iter + 1);
//                }
//            }
//        }
//    }
//
//    taskScheduler.shutdown();
//}
//
////------------------------------------------------------------------------------
//TEST(ParallelWavefront, KdSimple3D)
//{
//    ParallelFor3D<KdRange3d<size_t, size_t, size_t>, SimplePartitioner>(ELEMENT_COUNT / 2, TILE_SIZE);
//}
//
////------------------------------------------------------------------------------
//TEST(ParallelWavefront, KdSStatic3D)
//{
//    ParallelFor3D<KdRange3d<size_t, size_t, size_t>,StaticPartitioner>(ELEMENT_COUNT / 2, TILE_SIZE);
//}
//
////------------------------------------------------------------------------------
//TEST(ParallelWavefront, KdSAdaptive3D)
//{
//    ParallelFor3D<KdRange3d<size_t, size_t, size_t>,AdaptivePartitioner>(ELEMENT_COUNT / 2, TILE_SIZE);
//}
//
////------------------------------------------------------------------------------
//TEST(ParallelWavefront, OctSimple3D)
//{
//    ParallelFor3D<OctRange<size_t, size_t, size_t>, SimplePartitioner>(ELEMENT_COUNT / 2, TILE_SIZE);
//}
//
////------------------------------------------------------------------------------
//TEST(ParallelWavefront, OctStatic3D)
//{
//    ParallelFor3D<OctRange<size_t, size_t, size_t>, StaticPartitioner>(ELEMENT_COUNT / 2, TILE_SIZE);
//}
//
////------------------------------------------------------------------------------
//TEST(ParallelWavefront, OctAdaptive3D)
//{
//    ParallelFor3D<OctRange<size_t, size_t, size_t>, AdaptivePartitioner>(ELEMENT_COUNT / 2, TILE_SIZE);
//}

} // namespace testing
