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

#include "gts/micro_scheduler/WorkerPool.h"
#include "gts/micro_scheduler/MicroScheduler.h"
#include "gts/micro_scheduler/patterns/ParallelFor.h"
#include "gts/micro_scheduler/patterns/Partitioners.h"
#include "gts/micro_scheduler/patterns/BlockedRange1d.h"
#include "gts/micro_scheduler/patterns/BlockedRange2d.h"

using namespace gts;

namespace gts_examples {

//------------------------------------------------------------------------------
void convenience1dParallelFor()
{
    // Init boilerplate
    WorkerPool workerPool;
    bool result = workerPool.initialize();
    GTS_ASSERT(result);
    MicroScheduler taskScheduler;
    result = taskScheduler.initialize(&workerPool);
    GTS_ASSERT(result);

    // Create an array of 0s.
    uint32_t const elementCount = 1 << 16;
    gts::Vector<char> vec(elementCount, 0);

    // Make a parallel-for object for this scheduler. We do this because
    // there can be multiple scheduler objects.
    ParallelFor parallelFor(taskScheduler);

    // Similar to std::for_each.
    parallelFor(vec.begin(), vec.end(), [](auto iter) { (*iter)++; });

    taskScheduler.shutdown();
    workerPool.shutdown();
}

//------------------------------------------------------------------------------
void simpleIndexedParallelFor()
{
    // Init boilerplate
    WorkerPool workerPool;
    bool result = workerPool.initialize();
    GTS_ASSERT(result);
    MicroScheduler taskScheduler;
    result = taskScheduler.initialize(&workerPool);
    GTS_ASSERT(result);

    size_t const elementCount = 1 << 16;

    // Create an array of 0s.
    gts::Vector<char> vec(elementCount, 0);

    // Make a parallel-for object for this scheduler. We do this because
    // there can be multiple scheduler objects.
    ParallelFor parallelFor(taskScheduler);

    // The partitioner determines how the data in BlockedRange1d is divided
    // up over all the works. AdaptivePartitioner type only divides when
    // other worker threads need more work.
    auto partitionerType = AdaptivePartitioner();

    // Since the partitioner is adaptive, a block size of 1 gives the partitioner
    // full control over division. 1 is not always the best value, so experiment
    // as needed.
    size_t const blockSize = 1;

    parallelFor(

        // The 1D index range parallel-for will iterate over.
        BlockedRange1d<size_t>(0, elementCount, blockSize),

        // The function parallel for will execute on each block of the range.
        [](BlockedRange1d<size_t>& range, void* pData, TaskContext const&)
        {
            // unpack the user data
            gts::Vector<char>& vec = *(gts::Vector<char>*)pData;

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

    taskScheduler.shutdown();
    workerPool.shutdown();
}

//------------------------------------------------------------------------------
void simpleIteratorParallelFor()
{
    // Init boilerplate
    WorkerPool workerPool;
    bool result = workerPool.initialize();
    GTS_ASSERT(result);
    MicroScheduler taskScheduler;
    result = taskScheduler.initialize(&workerPool);
    GTS_ASSERT(result);

    size_t const elementCount = 1 << 16;

    // Create an array of 0s.
    gts::Vector<char> vec(elementCount, 0);

    ParallelFor parallelFor(taskScheduler);

    // The StaticPartitioner type recursively divides the only divides
    // BlockedRange1d until the blockSize is reached.
    auto paritionerType = StaticPartitioner();
    size_t const blockSize = 128;

    parallelFor(

        // The 1D iterator range parallel-for will iterate over.
        BlockedRange1d<gts::Vector<char>::iterator>(vec.begin(), vec.end(), blockSize),

        // The function parallel-for will execute on each block of the range.
        [](BlockedRange1d<gts::Vector<char>::iterator>& range, void*, TaskContext const&)
        {
            // Increment each element in the block.
            for (auto iter = range.begin(); iter != range.end(); ++iter)
            {
                (*iter)++;
            }
        },

        // The partitioner object.
        paritionerType,

        // No user data.
        nullptr
    );

    taskScheduler.shutdown();
    workerPool.shutdown();
}

} // namespace gts_examples
