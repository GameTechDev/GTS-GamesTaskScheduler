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
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <iostream>

#include "gts/micro_scheduler/WorkerPool.h"
#include "gts/micro_scheduler/MicroScheduler.h"

#include "gts/macro_scheduler/Node.h"

#include "gts/macro_scheduler/schedulers/work_stealing/WorkStealing_MacroScheduler.h"
#include "gts/macro_scheduler/compute_resources/CpuPureWorkStealing_ComputeResource.h"
#include "gts/macro_scheduler/compute_resources/CpuPureWorkStealing_Workload.h"

#include "gts/macro_scheduler/schedulers/work_sharing/WorkSharing_MacroScheduler.h"
#include "gts/macro_scheduler/compute_resources/CpuWorkStealing_ComputeResource.h"
#include "gts/macro_scheduler/compute_resources/CpuWorkStealing_Workload.h"

using namespace gts;

namespace testing {

//------------------------------------------------------------------------------
template<typename TComputeResource, typename TWorkload, typename TDagScheduler>
void SimpleTester()
{
    //
    // Setup CpuComputeResource

    WorkerPool workerPool;
    workerPool.initialize(1);

    MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    TComputeResource cpuComputeResource(&taskScheduler);

    //
    // Init MacroScheduler

    MacroSchedulerDesc generalizedDagSchedulerDesc;
    generalizedDagSchedulerDesc.computeResourcesByType[(uint32_t)ComputeResourceType::CPU_WorkStealing].push_back(&cpuComputeResource);

    MacroScheduler* pGenDagScheduler = new TDagScheduler;
    pGenDagScheduler->init(generalizedDagSchedulerDesc);

    //
    // Build DAG Nodes

    Node* pA = pGenDagScheduler->allocateNode();
    pA->setWorkload<TWorkload>(
        [](void*)
        {
            std::cout << "A" << std::endl;
        },
        nullptr);

    Node* pB = pGenDagScheduler->allocateNode();
    pB->setWorkload<TWorkload>(
        [](void*)
        {
            std::cout << "B" << std::endl;
        },
        nullptr);

    Node* pC = pGenDagScheduler->allocateNode();
    pC->setWorkload<TWorkload>(
        [](void*)
        {
            std::cout << "C" << std::endl;
        },
        nullptr);

    Node* pD = pGenDagScheduler->allocateNode();
    pD->setWorkload<TWorkload>(
        [](void*)
        {
            std::cout << "D" << std::endl;
        },
        nullptr);

    //
    // Connect DAG Nodes

    pA->addChild(pB);
    pA->addChild(pC);

    pB->addChild(pD);

    pC->addChild(pD);

    //
    // Build and exectue the schedule

    ISchedule* pSchedule = pGenDagScheduler->buildSchedule(pA, pD);
    pGenDagScheduler->executeSchedule(pSchedule);

    //
    // Cleanup.

    pGenDagScheduler->freeSchedule(pSchedule);

    pGenDagScheduler->freeNode(pD);
    pGenDagScheduler->freeNode(pC);
    pGenDagScheduler->freeNode(pB);
    pGenDagScheduler->freeNode(pA);

    delete pGenDagScheduler;
}

//------------------------------------------------------------------------------
TEST(MacroScheduler, SimpleTest)
{

    SimpleTester<CpuPureWorkStealing_ComputeResource, CpuPureWorkStealing_Workload, WorkStealing_MacroScheduler>();
    SimpleTester<CpuWorkStealing_ComputeResource, CpuWorkStealing_Workload, WorkSharing_MacroScheduler>();
}

} // namespace testing
