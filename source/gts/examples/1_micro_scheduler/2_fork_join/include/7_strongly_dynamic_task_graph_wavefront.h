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

#include <inttypes.h>
#include <vector>
#include <chrono>

#include <gts/micro_scheduler/WorkerPool.h>
#include <gts/micro_scheduler/MicroScheduler.h>

using namespace gts;

namespace gts_examples {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
struct GridCell
{
    uint64_t sum = 0;
    Atomic<uint8_t> dependecies = { 0 };
};

//------------------------------------------------------------------------------
Task* calcuateSum(GridCell** grid, int ii, int jj, int width, int height, TaskContext const& ctx)
{
    GridCell& myCell = grid[ii][jj];
    myCell.sum += (ii != 0 ? grid[ii-1][jj].sum : 0) + (jj != 0 ? grid[ii][jj-1].sum : 0);

    bool hasBottomSuccessor = (ii + 1 < width) && (grid[ii+1][jj].dependecies.fetch_sub(1, memory_order::acq_rel) - 1 == 0);
    bool hasRightSuccessor = (jj + 1 < height) && (grid[ii][jj+1].dependecies.fetch_sub(1, memory_order::acq_rel) - 1 == 0);

    // If there are ready successors,
    if(hasBottomSuccessor || hasRightSuccessor)
    {
        // We have to use continuations because blocking will cause a stack overflow.
        // Since there is nothing to continue to, we use an empty task.
        Task* pContinuation = ctx.pMicroScheduler->allocateTask<EmptyTask>();
        ctx.pThisTask->setContinuationTask(pContinuation);

        // Add references for the successors.
        uint32_t refCount = (hasBottomSuccessor ? 1 : 0) + (hasRightSuccessor ? 1 : 0);
        pContinuation->addRef(refCount, memory_order::relaxed);

        if (hasBottomSuccessor)
        {
            Task* pSuccessor = ctx.pMicroScheduler->allocateTask(calcuateSum, grid, ii + 1, jj, width, height);
            pContinuation->addChildTaskWithoutRef(pSuccessor);
            ctx.pMicroScheduler->spawnTask(pSuccessor);
        }

        if (hasRightSuccessor)
        {
            Task* pSuccessor = ctx.pMicroScheduler->allocateTask(calcuateSum, grid, ii, jj + 1, width, height);
            pContinuation->addChildTaskWithoutRef(pSuccessor);
            ctx.pMicroScheduler->spawnTask(pSuccessor);
        }
    }

    return nullptr;
}

//------------------------------------------------------------------------------
void stronglyDynamicTaskGraph_wavefront(int width, int height)
{
    printf ("================\n");
    printf ("stronglyDynamicTaskGraph_wavefront\n");
    printf ("================\n");

    /*
        The following examples generate a task graph over a 2D grid, were each
        task computes a 2D prefix sum S(i,j) = S(i-1,j) + S(i,j-1).
        
        (0,0) --------> (1,0) --------> ... -> (width-1,0)
          |               |                            |
          V               V                            V
        (0,1) --------> (1,1) --------> ... -> (width-1,1)
          |               |                            |
          V               V                            V
          .               .                            .
          .               .                            .
          .               .                            .
          |               |                            |
          V               V                            V
        (0,height-1) -> (1,height-1) -> ... -> (width-1,height-1)

        This example generates the graph as the execution unfolds. It moves the
        data out of the Tasks and into a memoization table. The benefits are:
        1. Task creation is distributed.
        2. all computed data is available after execution.
        3. No special treatment is require for the last Task like in
           weaklyDynamicTaskGraph_wavefront.
    */


    // Init boilerplate
    WorkerPool workerPool;
    bool result = workerPool.initialize();
    GTS_ASSERT(result);
    MicroScheduler microScheduler;
    result = microScheduler.initialize(&workerPool);
    GTS_ASSERT(result);

    auto start = std::chrono::high_resolution_clock::now();

    // Build the grid.
    GridCell** grid = new GridCell*[width];
    for (int ii = width - 1; ii >= 0; --ii)
    {
        grid[ii] = new GridCell[height];

        for (int jj = height - 1; jj >= 0; --jj)
        {
            grid[ii][jj].dependecies.store((ii > 0) + (jj > 0), memory_order::relaxed);
        }
    }

    // Set the initial value of the sum at the root.
    grid[0][0].sum = 1;

    Task* pTask = microScheduler.allocateTask(calcuateSum, grid, 0, 0, width, height);
    microScheduler.spawnTaskAndWait(pTask);

    printf("Sum: %" PRIu64 "\n", grid[width-1][height-1].sum);

    for (int ii = width - 1; ii >= 0; --ii)
    {
        delete[] grid[ii];
    }
    delete[] grid;

    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "Time (ms): " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << std::endl;
}

//------------------------------------------------------------------------------
void wavefront(uint64_t** grid, int ii, int jj, int dimensions)
{
    for (int x = ii; x < ii + dimensions; ++x)
    {
        for (int y = jj; y < jj + dimensions; ++y)
        {
            grid[x][y] += (x > 0 ? grid[x-1][y] : 0) + (y > 0 ? grid[x][y-1] : 0);
        }
    }
}

//------------------------------------------------------------------------------
Task* wavefrontDivAndConq(uint64_t** grid, int ii, int jj, int dimensions, TaskContext const& ctx)
{
    if (dimensions <= 16)
    {
        wavefront(grid, ii, jj, dimensions);
    }
    else
    {
        int halfDim = dimensions / 2;

        // Do this block's recursive work.
        Task* pChild = ctx.pMicroScheduler->allocateTask(wavefrontDivAndConq, grid, ii, jj, halfDim);
        ctx.pThisTask->addRef(2, memory_order::relaxed);
        ctx.pThisTask->addChildTaskWithoutRef(pChild);
        ctx.pThisTask->spawnAndWaitForAll(pChild);

        // Create the continuation for the 2 child blocks.
        Task* pContinuation = ctx.pMicroScheduler->allocateTask(wavefrontDivAndConq, grid, ii + halfDim, jj + halfDim, halfDim);
        ctx.pThisTask->setContinuationTask(pContinuation);
        pContinuation->addRef(2, memory_order::relaxed);

        //
        // Create and spawn the 2 child blocks.

        pChild = ctx.pMicroScheduler->allocateTask(wavefrontDivAndConq, grid, ii + halfDim, jj, halfDim);
        pContinuation->addChildTaskWithoutRef(pChild);
        ctx.pMicroScheduler->spawnTask(pChild);

        pChild = ctx.pMicroScheduler->allocateTask(wavefrontDivAndConq, grid, ii, jj + halfDim, halfDim);
        pContinuation->addChildTaskWithoutRef(pChild);
        ctx.pMicroScheduler->spawnTask(pChild);
    }

    return nullptr;
}

//------------------------------------------------------------------------------
void stronglyDynamicTaskGraph_divideAndConquerWavefront(int dim)
{
    printf ("================\n");
    printf ("stronglyDynamicTaskGraph_divideAndConquerWavefront\n");
    printf ("================\n");

    /*
        The following examples generate a task graph over a 2D grid, were each
        task computes a 2D prefix sum S(i,j) = S(i-1,j) + S(i,j-1).
        
        (0,0) --------> (1,0) --------> ... -> (dim-1,0)
          |               |                          |
          V               V                          V
        (0,1) --------> (1,1) --------> ... -> (dim-1,1)
          |               |                          |
          V               V                          V
          .               .                          .
          .               .                          .
          .               .                          .
          |               |                          |
          V               V                          V
        (0,dim-1) ----> (1,dim-1) ----> ... -> (dim-1,dim-1)
    */

    // Init boilerplate
    WorkerPool workerPool;
    bool result = workerPool.initialize();
    GTS_ASSERT(result);
    MicroScheduler microScheduler;
    result = microScheduler.initialize(&workerPool);
    GTS_ASSERT(result);

    auto start = std::chrono::high_resolution_clock::now();

    uint64_t* buffer = new uint64_t[dim*dim]; 
    uint64_t** grid = new uint64_t*[dim];
    for (int ii = 0; ii < dim; ++ii)
    {
        grid[ii] = buffer + dim * ii;
        memset(grid[ii], 0, sizeof(uint64_t) * dim);
    }

    grid[0][0] = 1;

    Task* pTask = microScheduler.allocateTask(wavefrontDivAndConq, grid, 0, 0, dim);
    microScheduler.spawnTaskAndWait(pTask);

    printf("Sum: %" PRIu64 "\n", grid[dim-1][dim-1]);

    delete[] grid;
    delete[] buffer;

    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "Time (ms): " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << std::endl;
}

} // namespace gts_examples
