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
#include "gts/platform/Thread.h"

#include "gts/micro_scheduler/WorkerPool.h"
#include "gts/micro_scheduler/MicroScheduler.h"

using namespace gts;

namespace gts_examples {

void explicitInit()
{
    // Explicitly create the workers via a description struct. For this example
    // we will affinitize each worker to a HW thread.
    WorkerPoolDesc workerPoolDesc;


    // Get the CPU topology.
    CpuTopology cpuTopo;
    Thread::getCpuTopology(cpuTopo);

    // Loop through each physical core.
    for (size_t iCore = 0; iCore < cpuTopo.coreInfo.size(); ++iCore)
    {
        CpuCoreInfo const& coreInfo = cpuTopo.coreInfo[iCore];

        // Loop through each logical core mask.
        for (size_t iMask = 0; iMask < coreInfo.logicalAffinityMasks.size(); ++iMask)
        {
            // NOTE: The order the WorkerThreadDesc are pushed into
            // WorkerThreadDesc::workerDescs is the order they are created and
            // indexed in the worker pool. Worker thread #0 is alway the thread 
            // that the pool was created on and is considered the "master"
            // worker thread.

            WorkerThreadDesc workerDesc;
            // Set the affinity mask.
            workerDesc.affinityMask = coreInfo.logicalAffinityMasks[iMask];
            // We can also set the OS thread priority. We'll keep it at the default.
            workerDesc.priority = Thread::PRIORITY_NORMAL;
            // Add the worker desc to the pool desc.
            workerPoolDesc.workerDescs.push_back(workerDesc);
        }
    }

    // Create a worker pool and init it with the description.
    WorkerPool workerPool;
    bool result = workerPool.initialize(workerPoolDesc);
    GTS_ASSERT(result);

    // Create a micro scheduler and assign the worker pool to it as before.
    MicroScheduler taskScheduler;
    result = taskScheduler.initialize(&workerPool);
    GTS_ASSERT(result);

    // schedule stuff ...

    taskScheduler.shutdown();
    workerPool.shutdown();
}

} // namespace gts_examples
