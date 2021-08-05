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
#include <iostream>
#include <vector>

#include "gts/micro_scheduler/WorkerPool.h"
#include "gts/micro_scheduler/MicroScheduler.h"
#include "gts/micro_scheduler/patterns/ParallelFor.h"

#include "gts/macro_scheduler/DagUtils.h"
#include "gts/macro_scheduler/Node.h"
#include "gts/macro_scheduler/compute_resources/MicroScheduler_Workload.h"
#include "gts/macro_scheduler/compute_resources/MicroScheduler_ComputeResource.h"
#include "gts/macro_scheduler/schedulers/heterogeneous/critical_node_task_scheduling/CriticalNode_MacroScheduler.h"
#include "gts/macro_scheduler/schedulers/heterogeneous/critical_node_task_scheduling/CriticalNode_Schedule.h"

using namespace gts;

namespace gts_examples {

void initCatsForHybridCpu()
{
    printf("================\n");
    printf("Init CATS Macro-scheduler a Hybrid CPU \n");
    printf("================\n");

    // A MacroScheduler is a high level scheduler that maps a persistent task
    // graph (DAG) of Nodes to set of ComputeResources. A CriticalNode_MacroScheduler
    // is a MacroScheduler that executes a DAG on a heterogeneous set of
    // ComputeResources. It uses a heuristic that forces the critical path of
    // a DAG to execute on the most efficient ComputeResource, which minimizes
    // the span of the execution.
    // 
    // Nodes are made available to ComputeResources via Workload types associated
    // with each ComputeResource type. For instance, if a Node can execute on
    // ComputeResouces A, B, and C, then it must have Workloads attached to it that
    // can run on A, B, and C.

    // NOTE: this example assumes a hybrid CPU two types of processors.

    //
    // First, we create the MicroSchedulders and map them to
    // MicroScheduler_ComputeResource that can be consumed by
    // a CriticalNode_MacroScheduler.
    //
    // Each MicroSchedulder cover a homogeneous set of processors on the CPU.

    // Get the CPU topology.
    SystemTopology sysTopo;
    Thread::getSystemTopology(sysTopo);

    // Get the max efficiency.
    uint32_t numEfficiencyClasses = 0;
    for (size_t iGroup = 0; iGroup < sysTopo.groupInfoElementCount; ++iGroup)
    {
        ProcessorGroupInfo const& groupInfo = sysTopo.pGroupInfoArray[iGroup];

        // Loop through each physical core.
        for (size_t iCore = 0; iCore < groupInfo.coreInfoElementCount; ++iCore)
        {
            numEfficiencyClasses = gtsMax(numEfficiencyClasses, (uint32_t)groupInfo.pCoreInfoArray[iCore].efficiencyClass + 1);
        }
    }

    std::vector<WorkerPool*> workerPoolsByEfficiency(numEfficiencyClasses);
    std::vector<MicroScheduler*> microSchedulersByEfficiency(numEfficiencyClasses);
    std::vector<ComputeResource*> computeResourcesByEfficiency(numEfficiencyClasses);

    // Loop through each processor group.
    for (size_t iGroup = 0; iGroup < sysTopo.groupInfoElementCount; ++iGroup)
    {
        ProcessorGroupInfo const& groupInfo = sysTopo.pGroupInfoArray[iGroup];

        std::vector<size_t> numThreadsByEfficiencyClass(numEfficiencyClasses);
        std::vector<AffinitySet> affinityByEfficiencyClass(numEfficiencyClasses);

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

        for (size_t ii = 0; ii < numEfficiencyClasses; ++ii)
        {
            WorkerPoolDesc workerPoolDesc;

            if (iGroup == 0 && ii == numEfficiencyClasses)
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
                workerDesc.affinity = { iGroup, affinityByEfficiencyClass[ii] };
                workerPoolDesc.workerDescs.push_back(workerDesc);
            }

            WorkerPool* pWorkerPool = alignedNew<WorkerPool, GTS_NO_SHARING_CACHE_LINE_SIZE>();
            bool result = pWorkerPool->initialize(workerPoolDesc);
            GTS_ASSERT(result);
            workerPoolsByEfficiency[ii] = pWorkerPool;
        }
    }

    // Create the MicroSchedulers
    for (size_t ii = 0; ii < numEfficiencyClasses; ++ii)
    {
        auto& vec = workerPoolsByEfficiency[ii];

        MicroScheduler* pMicroScheduler = alignedNew<MicroScheduler, GTS_NO_SHARING_CACHE_LINE_SIZE>();
        pMicroScheduler->initialize(workerPoolsByEfficiency[ii]);
        microSchedulersByEfficiency[ii] = pMicroScheduler;

        computeResourcesByEfficiency[ii] = alignedNew<MicroScheduler_ComputeResource, GTS_NO_SHARING_CACHE_LINE_SIZE>(pMicroScheduler, 0,
            workerPoolsByEfficiency[ii]->workerCount() / 2); // Only divide by 2 if CPU has SMT (hyperthreading)
        
        // Assumes each efficiency class is ii-times slower than computeResourcesByEfficiency[0];
        computeResourcesByEfficiency[ii]->setExecutionNormalizationFactor(ii + 1.0);
    }

    // Link such that higher efficiency schedulers victimize lower efficiency schedulers.
    for (int ii = numEfficiencyClasses - 1; ii > 0; --ii)
    {
        auto pHighSched = microSchedulersByEfficiency[ii];

        for (int jj = ii - 1; jj >= 0; --jj)
        {
            pHighSched->addExternalVictim(microSchedulersByEfficiency[jj]);
        }
    }

    //
    // Second, we create a CriticallyNode_MacroScheduler and map the
    // MicroScheduler_ComputeResources to it.

    MacroScheduler* pMacroScheduler = new CriticalNode_MacroScheduler;

    MacroSchedulerDesc generalizedDagSchedulerDesc;
    for (uint32_t ii = 0; ii < numEfficiencyClasses; ++ii)
    {
        generalizedDagSchedulerDesc.computeResources.push_back(computeResourcesByEfficiency[ii]);
    }

    pMacroScheduler->init(generalizedDagSchedulerDesc);

    //
    // For this example let's make a random DAG.

    const uint32_t NUM_RANKS = 10;
    const uint32_t MIN_NODES_PER_RANK = 2;
    const uint32_t MAX_NODES_PER_RANK = 3;
    const uint32_t CHANCE_OF_INCOMING_END = 0;

    Vector<Node*> nodes;

    DagUtils::generateRandomDag(pMacroScheduler, 1, NUM_RANKS, MIN_NODES_PER_RANK, MAX_NODES_PER_RANK, CHANCE_OF_INCOMING_END, nodes);

    for (uint32_t ii = 0; ii < nodes.size(); ++ii)
    {
        nodes[ii]->setName("%d\n", ii);
        nodes[ii]->addWorkload<MicroSchedulerLambda_Workload>([ii](WorkloadContext const&) { printf("%d\n", ii); });
    }

    Schedule* pSchedule = pMacroScheduler->buildSchedule(nodes.front(), nodes.back());
    pMacroScheduler->executeSchedule(pSchedule, computeResourcesByEfficiency[0]->id());
    pMacroScheduler->freeSchedule(pSchedule);

    delete pMacroScheduler;

    for (size_t ii = 0; ii < numEfficiencyClasses; ++ii)
    {
        alignedDelete(computeResourcesByEfficiency[ii]);
        alignedDelete(microSchedulersByEfficiency[ii]);
        alignedDelete(workerPoolsByEfficiency[ii]);
    }

    printf("SUCCESS!\n\n");
}

} // namespace gts_examples
