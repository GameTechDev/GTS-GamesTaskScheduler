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

//------------------------------------------------------------------------------
void basicParallelFor()
{
    printf ("================\n");
    printf ("basicParallelFor\n");
    printf ("================\n");

    // Create a worker pool for the MicroScheduler to run on.
    WorkerPool workerPool;
    bool result = workerPool.initialize();
    GTS_ASSERT(result);

    // Create a micro scheduler and assign the worker pool to it.
    MicroScheduler microScheduler;
    result = microScheduler.initialize(&workerPool);
    GTS_ASSERT(result);

    // Create a ParallelFor object attached to the scheduler.
    ParallelFor parFor(microScheduler);

    // Increment a vector of items in parallel
    std::vector<int> vec(1000000, 0);
    parFor(vec.begin(), vec.end(), [](std::vector<int>::iterator iter) { (*iter)++; });

    // Verify results.
    for (auto const& v : vec)
    {
        GTS_ASSERT(v == 1);
    }

    // These resources can be shutdown explicitly or their destructor will
    // shut them down implicitly.
    microScheduler.shutdown();
    workerPool.shutdown();

    printf("SUCCESS!\n\n");
}

//------------------------------------------------------------------------------
void basicMacroSchedulerNodeGraph()
{
    printf ("================\n");
    printf ("basicMacroSchedulerNodeGraph\n");
    printf ("================\n");

    // A MacroScheduler is a high level scheduler that maps a persistent task
    // graph (DAG) of Nodes to set of ComputeResources. A CentralQueue_MacroScheduler
    // is a MacroScheduler that executes a DAG exclusively on one or more
    // MicroSchedulers. Each Node is converted to a Task when it is ready to be
    // executed.

    //
    // First, we create a MicroSchedulder and map it to a
    // MicroScheduler_ComputeResource that can be consumed by
    // a CentralQueue_MacroScheduler.

    WorkerPool workerPool;
    workerPool.initialize();

    MicroScheduler microScheduler;
    microScheduler.initialize(&workerPool);

    MicroScheduler_ComputeResource microSchedulerCompResource(&microScheduler, 0, 0);

    //
    // Second, we create a CentralQueue_MacroScheduler and map the
    // MicroScheduler_ComputeResource to it.

    MacroSchedulerDesc generalizedDagSchedulerDesc;
    generalizedDagSchedulerDesc.computeResources.push_back(&microSchedulerCompResource);

    MacroScheduler* pMacroScheduler = new CentralQueue_MacroScheduler;
    pMacroScheduler->init(generalizedDagSchedulerDesc);

    //
    // Build a DAG of Nodes
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
    pA->addWorkload<MicroSchedulerLambda_Workload>([](WorkloadContext const&){ printf("A\n"); });

    Node* pB = pMacroScheduler->allocateNode();
    pB->addWorkload<MicroSchedulerLambda_Workload>([](WorkloadContext const&){ printf("B\n"); });

    Node* pC = pMacroScheduler->allocateNode();
    pC->addWorkload<MicroSchedulerLambda_Workload>([](WorkloadContext const&){ printf("C\n"); });

    Node* pD = pMacroScheduler->allocateNode();
    pD->addWorkload<MicroSchedulerLambda_Workload>([](WorkloadContext const&){ printf("D\n"); });

    pA->addSuccessor(pB);
    pA->addSuccessor(pC);
    pB->addSuccessor(pD);
    pC->addSuccessor(pD);

    //
    // Build and execute the schedule

    Schedule* pSchedule = pMacroScheduler->buildSchedule(pA, pD);

    // Let's execute the DAG as if it were in a game loop.
    for(size_t iLoop = 0; iLoop < 10; ++iLoop)
    {
        printf("--- Frame ---\n");
        pMacroScheduler->executeSchedule(pSchedule, microSchedulerCompResource.id());
    }

    printf("SUCCESS!\n\n");
}

//------------------------------------------------------------------------------
void basicMacroSchedulerNodeGraphWithParallelFor()
{
    printf ("================\n");
    printf ("basicMacroSchedulerNodeGraphWithParallelFor\n");
    printf ("================\n");

    //
    // Setup CpuComputeResource

    WorkerPool workerPool;
    workerPool.initialize();

    MicroScheduler microScheduler;
    microScheduler.initialize(&workerPool);

    MicroScheduler_ComputeResource microSchedulerCompResource(&microScheduler, 0, 0);

    //
    // Init MacroScheduler

    MacroSchedulerDesc generalizedDagSchedulerDesc;
    generalizedDagSchedulerDesc.computeResources.push_back(&microSchedulerCompResource);

    MacroScheduler* pMacroScheduler = new CentralQueue_MacroScheduler;
    pMacroScheduler->init(generalizedDagSchedulerDesc);

    //
    // Build a DAG of Nodes
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

      A: Increments all elements in a vector by 1.
      B: Increments elements [0, n/2) by 2.
      C: Increments elements [n/2, n) by 3.
      D: Increments all elements by by 1.

      Result: { 4, 4, ..., 4, 5, 5, ..., 5}.
    */

    ParallelFor parFor(microScheduler);
    std::vector<int> vec(1000000, 0);

    Node* pA = pMacroScheduler->allocateNode();
    pA->addWorkload<MicroSchedulerLambda_Workload>([&parFor, &vec](WorkloadContext const& ctx)
    {
        parFor(vec.begin(), vec.end(), [](std::vector<int>::iterator iter) { (*iter)++; });
        printf("A\n");
    });

    Node* pB = pMacroScheduler->allocateNode();
    pB->addWorkload<MicroSchedulerLambda_Workload>([&parFor, &vec](WorkloadContext const&)
    {
        parFor(vec.begin(), vec.begin() + vec.size() / 2, [](std::vector<int>::iterator iter) { (*iter) += 2; });
        printf("B\n");
    });

    Node* pC = pMacroScheduler->allocateNode();
    pC->addWorkload<MicroSchedulerLambda_Workload>([&parFor, &vec](WorkloadContext const&)
    {
        parFor(vec.begin() + vec.size() / 2, vec.end(), [](std::vector<int>::iterator iter) { (*iter) += 3; });
        printf("C\n");
    });

    Node* pD = pMacroScheduler->allocateNode();
    pD->addWorkload<MicroSchedulerLambda_Workload>([&parFor, &vec](WorkloadContext const&)
    {
        parFor(vec.begin(), vec.end(), [](std::vector<int>::iterator iter) { (*iter)++; });
        printf("D\n");

    });

    pA->addSuccessor(pB);
    pA->addSuccessor(pC);
    pB->addSuccessor(pD);
    pC->addSuccessor(pD);

    // Build and execute the schedule
    Schedule* pSchedule = pMacroScheduler->buildSchedule(pA, pD);
    pMacroScheduler->executeSchedule(pSchedule, microSchedulerCompResource.id());

    // Validate
    for (auto iter = vec.begin(); iter != vec.begin() + vec.size() / 2; ++iter)
    {
        GTS_ASSERT(*iter == 4);
    }
    for (auto iter = vec.begin() + vec.size() / 2; iter != vec.end(); ++iter)
    {
        GTS_ASSERT(*iter == 5);
    }


    printf("SUCCESS!\n\n");
}

} // namespace gts_examples
