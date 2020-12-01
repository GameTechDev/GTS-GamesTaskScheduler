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

void affinityInit()
{
    printf ("================\n");
    printf ("affinityInit\n");
    printf ("================\n");

    // Explicitly create the workers via a description struct. For this example
    // we will affinitize each worker to a HW thread.
    WorkerPoolDesc workerPoolDesc;

    // Get the CPU topology.
    SystemTopology sysTopo;
    Thread::getSystemTopology(sysTopo);

    // Loop through each processor group.
    for (size_t iGroup = 0; iGroup < sysTopo.groupInfoElementCount; ++iGroup)
    {
        ProcessorGroupInfo const& groupInfo = sysTopo.pGroupInfoArray[iGroup];

        // Loop through each physical core.
        for (size_t iCore = 0; iCore < groupInfo.coreInfoElementCount; ++iCore)
        {
            CpuCoreInfo const& coreInfo = groupInfo.pCoreInfoArray[iCore];

            // Loop through each thread.
            for (size_t iThead = 0; iThead < coreInfo.hardwareThreadIdCount; ++iThead)
            {
                WorkerThreadDesc workerDesc;

                // Set the affinity mask.
                workerDesc.affinity.affinitySet.set(coreInfo.pHardwareThreadIds[iThead]);
                workerDesc.affinity.group = iGroup;

                // Add the worker desc to the pool desc.
                workerPoolDesc.workerDescs.push_back(workerDesc);
            }
        }
    }

    // Affinitize the Master thread here since the Worker Pool does not own it.
    AffinitySet masterAffinity;
    masterAffinity.set(sysTopo.pGroupInfoArray[0].pCoreInfoArray[0].pHardwareThreadIds[0]);
    gts::ThisThread::setAffinity(0, masterAffinity);

    // Create a worker pool and init it with the description.
    WorkerPool workerPool;
    bool result = workerPool.initialize(workerPoolDesc);
    GTS_ASSERT(result);

    // Create a micro scheduler and assign the worker pool to it as before.
    MicroScheduler microScheduler;
    result = microScheduler.initialize(&workerPool);
    GTS_ASSERT(result);

    // schedule stuff ...

    microScheduler.shutdown();
    workerPool.shutdown();
}

} // namespace gts_examples
