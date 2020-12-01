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

void partitionInit()
{
    printf ("================\n");
    printf ("partitionInit\n");
    printf ("================\n");

    // Imagine a scenario where there are long-running, multi-frame Tasks that need to run
    // along side a normal update loop. If the long-running Tasks are mixed with
    // the update-loop, the update-loop Tasks could starve. To handle this quality-of-service
    // issue, processors can be partitioned into two WorkerPools, one to handle the update-loop
    // Tasks and one to handle the long-running Tasks. The goal of this configuration is to keep Tasks
    // isolated. The only time it may be ok to break this isolation is when the long-running Micro-scheduler
    // is out of Tasks, where it can victimize the update-loop Tasks.

    // Aside: One could also solve this problem similar to the 3_priority_init.h
    // example with the update loop running higher priority, however issues
    // could arise with cache thrashing when the tasks are context switched.

    // Scheduler for update loop tasks.
    WorkerPoolDesc mainPoolDesc;
    WorkerPool mainWorkerPool;
    MicroScheduler mainMicroScheduler;

    // Scheduler for multi-frame tasks.
    WorkerPoolDesc slowPoolDesc;
    WorkerPool slowWorkerPool;
    MicroScheduler slowMicroScheduler;

    // Lets split the CPU so that the first N-1 cores execute update loop tasks
    // and the last core updates the multi-frame tasks.

    AffinitySet mainPoolAffinity;
    AffinitySet slowPoolAffinity;

    int numMainThreads = 0;
    int numSlowThreads = 0;

    // Get the CPU topology.
    SystemTopology sysTopo;
    Thread::getSystemTopology(sysTopo);

    // Lets assume we only have one group for this example.
    ProcessorGroupInfo const& groupInfo = sysTopo.pGroupInfoArray[0];

    // Loop through the first N-1 cores and consolidate the affinities.
    for (size_t iCore = 0; iCore < groupInfo.coreInfoElementCount - 1; ++iCore)
    {
        CpuCoreInfo const& coreInfo = groupInfo.pCoreInfoArray[iCore];

        for (size_t iThead = 0; iThead < coreInfo.hardwareThreadIdCount; ++iThead)
        {
            // Consolidate.
            mainPoolAffinity.set(coreInfo.pHardwareThreadIds[iThead]);
            ++numMainThreads;
        }
    }

    // Get the affinities of the last core.
    {
        CpuCoreInfo const& coreInfo = groupInfo.pCoreInfoArray[groupInfo.coreInfoElementCount - 1];

        for (size_t iThead = 0; iThead < coreInfo.hardwareThreadIdCount; ++iThead)
        {
            // Consolidate.
            slowPoolAffinity.set(coreInfo.pHardwareThreadIds[iThead]);
            ++numSlowThreads;
        }
    }

    WorkerPoolDesc workerPoolDesc;

    //
    // Build the mainWorkerPool description 

    // The Mater thread IS part of this partition. Affinitize the Master thread here
    // since the Worker Pool does not own it.
    AffinitySet masterAffinity;
    masterAffinity.set(sysTopo.pGroupInfoArray[0].pCoreInfoArray[0].pHardwareThreadIds[0]);
    gts::ThisThread::setAffinity(0, masterAffinity);

    for (int iWorker = 0; iWorker < numMainThreads; ++iWorker)
    {
        WorkerThreadDesc workerDesc;
        // Set the affinity.
        workerDesc.affinity = {0, mainPoolAffinity};
        // Set the thread's name.
        sprintf(workerDesc.name, "MainWorker %d", iWorker);
        // Add the worker desc to the pool desc.
        mainPoolDesc.workerDescs.push_back(workerDesc);
    }
    sprintf(mainPoolDesc.name, "MainWorkerPool");
    mainWorkerPool.initialize(mainPoolDesc);
    mainMicroScheduler.initialize(&mainWorkerPool);

    //
    // Build the slowWorkerPool description 

    // The Mater thread IS NOT part of this partition. Add filler desc for the Master.
    slowPoolDesc.workerDescs.push_back(WorkerThreadDesc());

    for (int iWorker = 0; iWorker < numSlowThreads; ++iWorker)
    {
        WorkerThreadDesc workerDesc;
        // Set the affinity.
        workerDesc.affinity = {0, slowPoolAffinity};
        // Set the thread's name.
        sprintf(workerDesc.name, "SlowWorker %d", iWorker);
        // Add the worker desc to the pool desc.
        slowPoolDesc.workerDescs.push_back(workerDesc);
    }
    sprintf(slowPoolDesc.name, "SlowWorkerPool");
    slowWorkerPool.initialize(slowPoolDesc);
    slowMicroScheduler.initialize(&slowWorkerPool);

    // Further, let say that the slowMicroScheduler can become idle. Instead of
    // letting CPU resources go to waste, we can have the slowMicroScheduler
    // help out the mainMicroScheduler. To do this the mainMicroScheduler becomes
    // a victim of the slowTaskScehduler.

    slowMicroScheduler.addExternalVictim(&mainMicroScheduler);

    // Now, once all tasks local to slowMicroScheduler are complete,
    // slowMicroScheduler will try to steal work from mainMicroScheduler.
}

} // namespace gts_examples
