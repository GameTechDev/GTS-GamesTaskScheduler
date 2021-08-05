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

#include "gts/macro_scheduler/Node.h"
#include "gts/macro_scheduler/compute_resources/MicroScheduler_Workload.h"
#include "gts/macro_scheduler/compute_resources/MicroScheduler_ComputeResource.h"
#include "gts/macro_scheduler/schedulers/homogeneous/central_queue/CentralQueue_MacroScheduler.h"

using namespace gts;

namespace gts_examples {

void initDynamicMicroScheduler()
{
    printf("================\n");
    printf("Init Dynamic Micro-scheduler\n");
    printf("================\n");

    // A MacroScheduler is a high level scheduler that maps a persistent task
    // graph (DAG) of Nodes to set of ComputeResources. A DynamicMicroScheduler
    // is a MacroScheduler that executes a DAG exclusively on one or more
    // MicroSchedulers. Each Node is converted to a Task when it is ready to be
    // executed.

    // For this example, let's say we have two schedulers, one with higher 
    // priority than the other.

    //
    // First, we create the MicroSchedulders and map them to
    // MicroScheduler_ComputeResources that can be consumed by
    // a CentralQueue_MacroScheduler.

    constexpr uint32_t PRIORITY_COUNT = 2;
    WorkerPool workerPool[PRIORITY_COUNT];
    MicroScheduler microScheduler[PRIORITY_COUNT];
    MicroScheduler_ComputeResource microSchedulerCompResource[PRIORITY_COUNT];

    for (uint32_t iPriority = 0; iPriority < PRIORITY_COUNT; ++iPriority)
    {
        WorkerPoolDesc workerPoolDesc;

        for (uint32_t iWorker = 0; iWorker < Thread::getHardwareThreadCount(); iWorker++)
        {
            WorkerThreadDesc workerDesc;
            workerDesc.priority = (Thread::Priority)((uint32_t)Thread::Priority::PRIORITY_NORMAL + iPriority);
            workerPoolDesc.workerDescs.push_back(workerDesc);
        }

        workerPool[iPriority].initialize(workerPoolDesc);
        microScheduler[iPriority].initialize(&workerPool[iPriority]);

        microSchedulerCompResource[iPriority].init(microScheduler + iPriority);
    }

    //
    // Second, we create a MicroScheduler_ComputeResources and map the
    // MicroScheduler_ComputeResources to it.

    MacroSchedulerDesc generalizedDagSchedulerDesc;
    for (uint32_t iPriority = 0; iPriority < PRIORITY_COUNT; ++iPriority)
    {
        generalizedDagSchedulerDesc.computeResources.push_back(microSchedulerCompResource + iPriority);
    }

    MacroScheduler* pMacroScheduler = new CentralQueue_MacroScheduler;
    pMacroScheduler->init(generalizedDagSchedulerDesc);

    //
    // For this example let's revisit the simple DAG from the Quick Start
    // example. However, this time we want A, B, and D to run on the high priority
    // scheduler and C to run on the low priority scheduler. We do this with 
    // Node affinity.
    /*
                +---+
          +---->| B |-----+
          |     +---+     |
          |               v
        +---+           +---+
        | A |           | D |
        +---+           +---+
          |               ^
          |     +---+     |
          +---->| C |-----+
                +---+
    */


    Node* pA = pMacroScheduler->allocateNode();
    pA->addWorkload<MicroSchedulerLambda_Workload>([](WorkloadContext const&) { printf("A\n"); });
    pA->setAffinity(microSchedulerCompResource[1].id());

    Node* pB = pMacroScheduler->allocateNode();
    pB->addWorkload<MicroSchedulerLambda_Workload>([](WorkloadContext const&) { printf("B\n"); });
    pB->setAffinity(microSchedulerCompResource[1].id());

    Node* pC = pMacroScheduler->allocateNode();
    pC->addWorkload<MicroSchedulerLambda_Workload>([](WorkloadContext const&) { printf("C\n"); });
    pC->setAffinity(microSchedulerCompResource[0].id());

    Node* pD = pMacroScheduler->allocateNode();
    pD->addWorkload<MicroSchedulerLambda_Workload>([](WorkloadContext const&) { printf("D\n"); });
    pD->setAffinity(microSchedulerCompResource[1].id());

    pA->addSuccessor(pB);
    pA->addSuccessor(pC);
    pB->addSuccessor(pD);
    pC->addSuccessor(pD);

    // Build and execute the schedule
    Schedule* pSchedule = pMacroScheduler->buildSchedule(pA, pD);
    pMacroScheduler->executeSchedule(pSchedule, microSchedulerCompResource[1].id());

    pMacroScheduler->freeSchedule(pSchedule);
    delete pMacroScheduler;

    printf("SUCCESS!\n\n");
}

} // namespace gts_examples
