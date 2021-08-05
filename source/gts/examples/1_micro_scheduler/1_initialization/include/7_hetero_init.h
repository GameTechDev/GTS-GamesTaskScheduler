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
#include <vector>

#include "gts/platform/Thread.h"

#include "gts/micro_scheduler/WorkerPool.h"
#include "gts/micro_scheduler/MicroScheduler.h"

using namespace gts;

namespace gts_examples {

void heteroInit()
{
    printf ("================\n");
    printf ("heteroInit\n");
    printf ("================\n");

    // This example demonstrates hierarchical Task scheduling over two Efficiency Classes.  Each Efficiency
    // class gets it's own Worker Pool and Micro-scheduler. The goal of this configuration is to keep Tasks
    // local to their Efficiency Class as long as possible. The only non-local communication that happens
    // is when the more efficient Micro-scheduler victimizes the less efficient Micro-scheduler. Victimization
    // is one-way, because we don't want a more computationally intensive Task to run on a less efficient processor,
    // which could dramatically increase the overall makespan of the schedule.

    // Get the CPU topology.
    SystemTopology sysTopo;
    Thread::getSystemTopology(sysTopo);

    // Get the max efficiency.
    uint32_t maxEfficiency = 0;
    for (size_t iGroup = 0; iGroup < sysTopo.groupInfoElementCount; ++iGroup)
    {
        ProcessorGroupInfo const& groupInfo = sysTopo.pGroupInfoArray[iGroup];

        // Loop through each physical core.
        for (size_t iCore = 0; iCore < groupInfo.coreInfoElementCount; ++iCore)
        {
            maxEfficiency = gtsMax(maxEfficiency, (uint32_t)groupInfo.pCoreInfoArray[iCore].efficiencyClass);
        }
    }

    std::vector<std::vector<WorkerPool*>> workerPoolsByEfficiency(maxEfficiency + 1);
    std::vector<std::vector<MicroScheduler*>> microSchedulersByEfficiency(maxEfficiency + 1);

    // Loop through each processor group.
    for (size_t iGroup = 0; iGroup < sysTopo.groupInfoElementCount; ++iGroup)
    {
        ProcessorGroupInfo const& groupInfo = sysTopo.pGroupInfoArray[iGroup];

        std::vector<size_t> numThreadsByEfficiencyClass(maxEfficiency + 1);
        std::vector<AffinitySet> affinityByEfficiencyClass(maxEfficiency + 1);

        // Loop through each physical core.
        for (size_t iCore = 0; iCore < groupInfo.coreInfoElementCount; ++iCore)
        {
            CpuCoreInfo const& coreInfo = groupInfo.pCoreInfoArray[iCore];

            // Loop through each thread.
            for (size_t iThead = 0; iThead < coreInfo.hardwareThreadIdCount; ++iThead)
            {
                numThreadsByEfficiencyClass[coreInfo.efficiencyClass]++;
                affinityByEfficiencyClass[coreInfo.efficiencyClass].set(coreInfo.pHardwareThreadIds[iThead]);
            }
        }

        //
        // Create the WorkerPool for each efficiency class.

        for (size_t ii = 0; ii <= maxEfficiency; ++ii)
        {
            WorkerPoolDesc workerPoolDesc;

            if (iGroup == 0 && ii == maxEfficiency)
            {
                // The Mater thread IS part of this class.

                // Affinitize the Master thread here since the Worker Pool does not own it.
                AffinitySet masterAffinity;
                masterAffinity.set(sysTopo.pGroupInfoArray[0].pCoreInfoArray[0].pHardwareThreadIds[0]);
                gts::ThisThread::setAffinity(0, masterAffinity);
            }
            else
            {
                // The Mater thread IS NOT part of this node.

                // Add filler desc for the master so that Worker threads
                // are created for all the threads in the class.
                workerPoolDesc.workerDescs.push_back(WorkerThreadDesc());
            }

            size_t numThreads = numThreadsByEfficiencyClass[ii];
            for (size_t iWorker = 0; iWorker < numThreads; ++iWorker)
            {
                WorkerThreadDesc workerDesc;
                workerDesc.affinity = {iGroup, affinityByEfficiencyClass[ii]};
                workerPoolDesc.workerDescs.push_back(workerDesc);
            }

            WorkerPool* pWorkerPool = new WorkerPool;
            bool result = pWorkerPool->initialize(workerPoolDesc);
            GTS_ASSERT(result);
            workerPoolsByEfficiency[ii].push_back(pWorkerPool);
        }
    }

    // Create the MicroSchedulers
    for (size_t ii = 0; ii <= maxEfficiency; ++ii)
    {
        auto& vec = workerPoolsByEfficiency[ii];

        for (auto& wp : vec)
        {
            MicroScheduler* pMicroScheduler = alignedNew<MicroScheduler, GTS_NO_SHARING_CACHE_LINE_SIZE>();
            pMicroScheduler->initialize(wp);
            microSchedulersByEfficiency[ii].push_back(pMicroScheduler);
        }
    }

    // Link such that higher efficiency schedulers victimize lower efficiency schedulers.
    for (int ii = maxEfficiency; ii > 0; --ii)
    {
        auto highVec = microSchedulersByEfficiency[ii];

        for (auto highSched : highVec)
        {
            for (int jj = ii - 1; jj >= 0; --jj)
            {
                auto lowVec = microSchedulersByEfficiency[jj];

                for (auto lowSched : lowVec)
                {
                    highSched->addExternalVictim(lowSched);
                }
            }
        }
    }

    // schedule stuff ...

}

} // namespace gts_examples
