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
class GridSumTask : public Task
{
public:

    virtual Task* execute(TaskContext const& ctx) final
    {
        sum = predecessorSum[0] + predecessorSum[1];

        for(int iSucc = 0; iSucc < 2; ++iSucc)
        {
            if (pSuccessors[iSucc])
            {
                pSuccessors[iSucc]->predecessorSum[iSucc] = sum;
                if (pSuccessors[iSucc]->removeRef(1) == 1)
                {
                    ctx.pMicroScheduler->spawnTask(pSuccessors[iSucc]);
                }
            }
        }

        return nullptr;
    }

    GridSumTask* pSuccessors[2];
    uint64_t predecessorSum[2] = {0, 0};
    uint64_t sum = 0;
};

//------------------------------------------------------------------------------
void weaklyDynamicTaskGraph_wavefront(int width, int height)
{
    printf ("================\n");
    printf ("weaklyDynamicTaskGraph_wavefront\n");
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

        This example builds a static representation of the graph.
    */

    // Init boilerplate
    WorkerPool workerPool;
    bool result = workerPool.initialize();
    GTS_ASSERT(result);
    MicroScheduler microScheduler;
    result = microScheduler.initialize(&workerPool);
    GTS_ASSERT(result);

    auto start = std::chrono::high_resolution_clock::now();

    //
    // Pre-build the graph topology.

    std::vector<std::vector<GridSumTask*>> taskGrid(width);

    for (int ii = width - 1; ii >= 0; --ii)
    {
        taskGrid[ii].resize(height);

        for (int jj = height - 1; jj >= 0; --jj)
        {
            GridSumTask* pTask = microScheduler.allocateTask<GridSumTask>();

            // Add a reference for each incoming edge.
            pTask->addRef((ii > 0) + (jj > 0), memory_order::relaxed);

            // Point to each successor so this Task can decrement its reference
            // once it's execute.
            pTask->pSuccessors[0] = ii + 1 < width ? taskGrid[ii+1][jj] : nullptr;
            pTask->pSuccessors[1] = jj + 1 < height ? taskGrid[ii][jj+1] : nullptr;

            taskGrid[ii][jj] = pTask;
        }
    }

    //
    // Execute the graph.

    // Get the first and last tasks.
    GridSumTask* pRoot = taskGrid[0][0];
    GridSumTask* pLast = taskGrid[width-1][height-1];

    // Set the initial value of the sum at the root.
    pRoot->predecessorSum[0] = 1;

    // Add a reference to the last Task so that it's not implicitly destroyed.
    // We want to query the last node for the sum.
    pLast->addRef(1, memory_order::relaxed);

    // Spawn the root task and wait for for the graph to decrement pLast's
    // reference count.
    pLast->spawnAndWaitForAll(pRoot);

    // Execute the task manually since we added a ref.
    pLast->execute(TaskContext());

    printf("Sum: %" PRIu64 "\n", pLast->sum);

    // Destroy the task manually since we added a ref.
    microScheduler.destoryTask(pLast);

    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "Time (ms): " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << std::endl;
}

} // namespace gts_examples
