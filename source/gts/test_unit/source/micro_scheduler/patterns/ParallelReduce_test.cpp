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

#include "gts/analysis/Instrumenter.h"
#include "gts/analysis/ConcurrentLogger.h"

#include "gts/platform/Thread.h"

#include "gts/micro_scheduler/WorkerPool.h"
#include "gts/micro_scheduler/MicroScheduler.h"
#include "gts/micro_scheduler/patterns/ParallelReduce.h"
#include "gts/micro_scheduler/patterns/Partitioners.h"

#include "SchedulerTestsCommon.h"

using namespace gts;

namespace testing {

//------------------------------------------------------------------------------
TEST(ParallelReduce, parallelReduce)
{
    WorkerPool workerPool;
    workerPool.initialize();

    MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    ParallelReduce reduce(taskScheduler);

    // Create a array of 1s.
    std::vector<uint32_t> onesArray;
    onesArray.resize(ELEMENT_COUNT, 1);

    for (uint32_t ii = 0; ii < ITERATIONS_CONCUR; ++ii)
    {
        GTS_CONCRT_LOGGER_RESET();

        uint32_t reduction = reduce(
            BlockedRange1d<std::vector<uint32_t>::iterator>(onesArray.begin(), onesArray.end(), TILE_SIZE),
            [](BlockedRange1d<std::vector<uint32_t>::iterator>& range, void*, TaskContext const&) -> uint32_t
            {
                uint32_t result = 0;
                for (auto ii = range.begin(); ii != range.end(); ++ii)
                {
                    result += *ii;
                }
                return result;
            },
            [](uint32_t const& lhs, uint32_t const& rhs, void*, TaskContext const&) -> uint32_t
            {
                return lhs + rhs;
            },
            0,
            AdaptivePartitioner());

        ASSERT_EQ(ELEMENT_COUNT, reduction);
    }

    taskScheduler.shutdown();
}

//------------------------------------------------------------------------------
TEST(ParallelReduce, parallelMapReduce)
{
    WorkerPool workerPool;
    workerPool.initialize();

    MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    ParallelReduce reduce(taskScheduler);

    // Create a array of 0s.
    std::vector<uint32_t> onesArray;
    onesArray.resize(ELEMENT_COUNT, 0);

    for (uint32_t ii = 0; ii < ITERATIONS_CONCUR; ++ii)
    {
        GTS_CONCRT_LOGGER_RESET();

        uint32_t reduction = reduce(
            BlockedRange1d<std::vector<uint32_t>::iterator>(onesArray.begin(), onesArray.end(), TILE_SIZE),
            [](BlockedRange1d<std::vector<uint32_t>::iterator>& range, void*, TaskContext const&) -> uint32_t
            {
                uint32_t result = 0;
                for (auto ii = range.begin(); ii != range.end(); ++ii)
                {
                    *ii = 1;        // map
                    result += *ii;  // reduce
                }
                return result;
            },
            [](uint32_t const& lhs, uint32_t const& rhs, void*, TaskContext const&) -> uint32_t
            {
                return lhs + rhs;
            },
            0,
            AdaptivePartitioner());

        ASSERT_EQ(ELEMENT_COUNT, reduction);
    }

    taskScheduler.shutdown();
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
struct AABB
{
    float min[3];
    float max[3];

    AABB()
    {
        min[0] = FLT_MAX;
        min[1] = FLT_MAX;
        min[2] = FLT_MAX;

        max[0] = FLT_MIN;
        max[1] = FLT_MIN;
        max[2] = FLT_MIN;
    }

    AABB(AABB const& other)
    {
        min[0] = other.min[0];
        min[1] = other.min[1];
        min[2] = other.min[2];

        max[0] = other.max[0];
        max[1] = other.max[1];
        max[2] = other.max[2];
    }

    bool approxEq(float a, float b, float tolerance) const
    {
        return abs(a - b) < tolerance;
    }

    bool operator==(AABB const& other) const
    {
        const float absErr = 0.001f;
        return  approxEq(min[0], other.min[0], absErr) &&
                approxEq(min[1], other.min[1], absErr) &&
                approxEq(min[2], other.min[2], absErr) &&
                approxEq(max[0], other.max[0], absErr) &&
                approxEq(max[1], other.max[1], absErr) &&
                approxEq(max[2], other.max[2], absErr);
    }

    void inflate(AABB const& other)
    {
        min[0] = std::min(min[0], other.min[0]);
        min[1] = std::min(min[1], other.min[1]);
        min[2] = std::min(min[2], other.min[2]);

        max[0] = std::max(max[0], other.max[0]);
        max[1] = std::max(max[1], other.max[1]);
        max[2] = std::max(max[2], other.max[2]);
    }

    static AABB inflate(AABB const& lhs, AABB const& rhs)
    {
        AABB result(lhs);
        result.inflate(rhs);
        return result;
    }
};

//------------------------------------------------------------------------------
TEST(ParallelReduce, parallelReduce_AABB)
{
    WorkerPool workerPool;
    workerPool.initialize();

    MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    ParallelReduce reduce(taskScheduler);

    const uint32_t subdivisions = 100;
    const uint32_t regionCount = subdivisions * subdivisions * subdivisions;
    std::vector<AABB> regions(regionCount);

    AABB regionExtents;
    regionExtents.min[0] = -1.1f;
    regionExtents.min[1] = -1.2f;
    regionExtents.min[2] = -1.3f;
    regionExtents.max[0] = 1.1f;
    regionExtents.max[1] = 1.2f;
    regionExtents.max[2] = 1.3f;

    float stepX = (regionExtents.max[0] - regionExtents.min[0]) / subdivisions;
    float stepY = (regionExtents.max[1] - regionExtents.min[1]) / subdivisions;
    float stepZ = (regionExtents.max[2] - regionExtents.min[2]) / subdivisions;

    uint32_t iRegion = 0;
    for (uint32_t ix = 0; ix < subdivisions - 1; ++ix)
    {
        for (uint32_t iy = 0; iy < subdivisions - 1; ++iy)
        {
            for (uint32_t iz = 0; iz < subdivisions - 1; ++iz)
            {
                AABB& region = regions[iRegion++];
                region.min[0] = regionExtents.min[0] + stepX * ix;
                region.min[1] = regionExtents.min[1] + stepY * iy;
                region.min[2] = regionExtents.min[2] + stepZ * iz;
                region.max[0] = std::min(regionExtents.min[0] + stepX * ix + 1, regionExtents.max[0]);
                region.max[1] = std::min(regionExtents.min[1] + stepY * iy + 1, regionExtents.max[1]);
                region.max[2] = std::min(regionExtents.min[2] + stepZ * iz + 1, regionExtents.max[2]);
            }
        }
    }

    for (uint32_t ii = 0; ii < ITERATIONS_CONCUR; ++ii)
    {
        GTS_CONCRT_LOGGER_RESET();

        // Reduce all the AABBs to a single AABB.
        AABB reduction = reduce(
            BlockedRange1d<std::vector<AABB>::iterator>(regions.begin(), regions.end(), TILE_SIZE),
            [](BlockedRange1d<std::vector<AABB>::iterator>& range, void*, TaskContext const&) -> AABB
            {
                AABB result;
                for (auto iAABB = range.begin(); iAABB != range.end(); ++iAABB)
                {
                    result.inflate(*iAABB);
                }
                return result;
            },
            [](AABB const& lhs, AABB const& rhs, void*, TaskContext const&) -> AABB
            {
                return AABB::inflate(lhs, rhs);
            },
            AABB(),
            AdaptivePartitioner());

        ASSERT_EQ(regionExtents, reduction);
    }

    taskScheduler.shutdown();
}

} // namespace testing
