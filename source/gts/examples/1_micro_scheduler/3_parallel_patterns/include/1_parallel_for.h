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
#pragma once

#include <vector>

#include "gts/micro_scheduler/WorkerPool.h"
#include "gts/micro_scheduler/MicroScheduler.h"
#include "gts/micro_scheduler/patterns/ParallelFor.h"
#include "gts/micro_scheduler/patterns/Partitioners.h"
#include "gts/micro_scheduler/patterns/Range1d.h"

using namespace gts;

namespace gts_examples {

//------------------------------------------------------------------------------
void simplestIndexedParallelFor()
{
    printf ("================\n");
    printf ("simplestIndexedParallelFor\n");
    printf ("================\n");

    // Demonstrates the simplest ParallelFor interface with 
    // iteration over a container using an index.

    // Init boilerplate
    WorkerPool workerPool;
    bool result = workerPool.initialize();
    GTS_ASSERT(result);
    MicroScheduler microScheduler;
    result = microScheduler.initialize(&workerPool);
    GTS_ASSERT(result);

    size_t const elementCount = 1 << 16;

    // Make a ParallelFor object for this scheduler. We do this because
    // there can be multiple scheduler objects.
    ParallelFor parFor(microScheduler);

    // Increment a vector of items in parallel
    std::vector<int> vec(elementCount, 0);
    parFor(size_t(0), size_t(vec.size()), [&vec](size_t idx) { vec[idx]++; });

    // Verify results.
    for (auto const& v : vec)
    {
        GTS_ASSERT(v == 1);
    }
}

//------------------------------------------------------------------------------
void simplerIteratorParallelFor()
{
    printf ("================\n");
    printf ("simplerIteratorParallelFor\n");
    printf ("================\n");

    // Demonstrates the simpler ParallelFor interface with 
    // iteration over a container using an iterator.

    // Init boilerplate
    WorkerPool workerPool;
    bool result = workerPool.initialize();
    GTS_ASSERT(result);
    MicroScheduler microScheduler;
    result = microScheduler.initialize(&workerPool);
    GTS_ASSERT(result);

    size_t const elementCount = 1 << 16;

    ParallelFor parFor(microScheduler);

    // The partitioner determines how the data in Range1d is divided
    // up over all the works. AdaptivePartitioner type only divides when
    // other worker threads need more work.
    auto partitionerType = AdaptivePartitioner();

    // Since the partitioner is adaptive, a block size of 1 gives the partitioner
    // full control over division.
    size_t const blockSize = 1;

    // Increment a vector of items in parallel
    std::vector<int> vec(elementCount, 0);
    parFor(
        vec.begin(),
        vec.end(),
        [](std::vector<int>::iterator iter) { (*iter)++; });

    // Verify results.
    for (auto const& v : vec)
    {
        GTS_ASSERT(v == 1);
    }
}

//------------------------------------------------------------------------------
void fullParallelFor()
{
    printf ("================\n");
    printf ("fullParallelFor\n");
    printf ("================\n");

    // Demonstrates the full ParallelFor interface. Note that the ParallelFor
    // execution function gives full access to the iteration range.

    // Init boilerplate
    WorkerPool workerPool;
    bool result = workerPool.initialize();
    GTS_ASSERT(result);
    MicroScheduler microScheduler;
    result = microScheduler.initialize(&workerPool);
    GTS_ASSERT(result);

    size_t const elementCount = 1 << 16;
    std::vector<int> vec(elementCount, 0);

    ParallelFor parallelFor(microScheduler);
    auto partitionerType = AdaptivePartitioner();
    size_t const blockSize = 1;

    parallelFor(

        // The 1D index range parallel-for will iterate over.
        Range1d<size_t>(0, elementCount, blockSize),

        // The function parallel for will execute on each block of the range.
        [](Range1d<size_t>& range, void* pData, TaskContext const&)
        {
            // unpack the user data
            std::vector<int>& vec = *(std::vector<int>*)pData;

            // For each index in the block, increment the element.
            for (size_t idx = range.begin(); idx != range.end(); ++idx)
            {
                vec[idx]++;
            }
        },

        // The partitioner object.
        partitionerType,

        // The vec user data.
        &vec
    );

    // Verify results.
    for (auto const& v : vec)
    {
        GTS_ASSERT(v == 1);
    }
}

} // namespace gts_examples
