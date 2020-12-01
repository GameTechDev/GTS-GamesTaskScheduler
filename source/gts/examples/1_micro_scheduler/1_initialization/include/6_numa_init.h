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

void numaInit()
{
    printf ("================\n");
    printf ("numaInit\n");
    printf ("================\n");

    // This example demonstrates hierarchical Task scheduling over two NUMA nodes. Each node gets it's own
    // Worker Pool and Micro-scheduler. The goal of this configuration is to keep Tasks local to each
    // NUMA node as long as possible. Only when they run out of Tasks do they communicate with each other
    // through victimization.

    // Get the CPU topology.
    SystemTopology sysTopo;
    Thread::getSystemTopology(sysTopo);

    std::vector<WorkerPool*> workerPoolsByNumaNode;
    std::vector<MicroScheduler*> microSchedulersByNumaNode;

    // Loop through each processor group.
    for (size_t iGroup = 0; iGroup < sysTopo.groupInfoElementCount; ++iGroup)
    {
        ProcessorGroupInfo const& groupInfo = sysTopo.pGroupInfoArray[iGroup];

        // Loop through each NUMA node.
        for (size_t iNode = 0; iNode < groupInfo.numaNodeInfoElementCount; ++iNode)
        {
            NumaNodeInfo const& nodeInfo = groupInfo.pNumaInfoArray[iNode];

            // Get the thread count for the node.
            size_t threadCount = 0;
            // Get the affinity set for node.
            AffinitySet nodeAffinity;

            // Loop through each Core.
            for (size_t iCore = 0; iCore < nodeInfo.coreInfoElementCount; ++iCore)
            {
                CpuCoreInfo const& coreInfo = nodeInfo.pCoreInfoArray[iCore];

                // Loop through each HW thread.
                for (size_t iThead = 0; iThead < coreInfo.hardwareThreadIdCount; ++iThead)
                {
                    threadCount++;
                    nodeAffinity.set(coreInfo.pHardwareThreadIds[iThead]);
                }
            }

            //
            // Create the WorkerPool for this node.

            WorkerPoolDesc workerPoolDesc;

            if (iGroup == 0 && iNode == 0)
            {
                // The Mater thread IS part of this node.

                // Affinitize the Master thread here since the Worker Pool does not own it.
                AffinitySet masterAffinity;
                masterAffinity.set(sysTopo.pGroupInfoArray[0].pCoreInfoArray[0].pHardwareThreadIds[0]);
                gts::ThisThread::setAffinity(0, masterAffinity);
            }
            else
            {
                // The Mater thread IS NOT part of this node.

                // Add filler desc for the master so that Worker threads
                // are created for all the threads in the node.
                workerPoolDesc.workerDescs.push_back(WorkerThreadDesc());
            }

            for (size_t iWorker = 0; iWorker < threadCount; ++iWorker)
            {
                WorkerThreadDesc workerDesc;
                workerDesc.affinity = {iGroup, nodeAffinity};
                workerPoolDesc.workerDescs.push_back(workerDesc);
            }

            WorkerPool* pWorkerPool = new WorkerPool;
            bool result = pWorkerPool->initialize(workerPoolDesc);
            GTS_ASSERT(result);
            workerPoolsByNumaNode.push_back(pWorkerPool);
        }
    }

    // Create the MicroSchedulers
    for (auto wp : workerPoolsByNumaNode)
    {
        MicroScheduler* pMicroScheduler = alignedNew<MicroScheduler, GTS_NO_SHARING_CACHE_LINE_SIZE>();
        pMicroScheduler->initialize(wp);
        microSchedulersByNumaNode.push_back(pMicroScheduler);
    }

    // Link the external victims
    for (auto ms : microSchedulersByNumaNode)
    {
        for (auto victim : microSchedulersByNumaNode)
        {
            if (ms == victim)
            {
                // Don't self victimize.
                continue;
            }

            ms->addExternalVictim(victim);
        }
    }

    // schedule stuff ...

}

} // namespace gts_examples
