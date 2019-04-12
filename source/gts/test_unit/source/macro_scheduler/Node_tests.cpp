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

#include "DagUtils.h"

// Dynamic
#include "gts/macro_scheduler/schedulers/dynamic/Dynamic_MacroScheduler.h"
#include "gts/macro_scheduler/compute_resources/MicroScheduler_ComputeResource.h"
#include "gts/macro_scheduler/compute_resources/MicroScheduler_Workload.h"

using namespace gts;

namespace testing {

//------------------------------------------------------------------------------
TEST(Node, SetCStyleWorkload)
{
    //
    // Setup CpuComputeResource

    WorkerPool workerPool;
    workerPool.initialize(1);

    MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    MicroScheduler_ComputeResource cpuComputeResource(&taskScheduler);

    //
    // Init MacroScheduler

    MacroSchedulerDesc generalizedDagSchedulerDesc;
    generalizedDagSchedulerDesc.computeResourcesByType[(uint32_t)ComputeResourceType::CPU_MicroScheduler].push_back(&cpuComputeResource);

    MacroScheduler* pMacroScheduler = new Dynamic_MacroScheduler;
    pMacroScheduler->init(generalizedDagSchedulerDesc);

    //
    // Build DAG Node

    uint32_t val = 0;
    Node* pNode = pMacroScheduler->allocateNode();
    MicroScheduler_Workload* pWorkloadA = pNode->setEmptyWorkload<MicroScheduler_Workload>([](Workload* pThisWorkload, WorkloadContext const&)
    {
        MicroScheduler_Workload* pWorkload = (MicroScheduler_Workload*)pThisWorkload;
        uint32_t* pData = (uint32_t*)pWorkload->getData();
        *pData = 1;
    });
    pWorkloadA->setData(&val);


    //
    // Build and execute the schedule

    ISchedule* pSchedule = pMacroScheduler->buildSchedule(pNode, pNode);
    pMacroScheduler->executeSchedule(pSchedule);

    ASSERT_EQ(val, 1u);

    //
    // Cleanup.

    pMacroScheduler->freeSchedule(pSchedule);
    pMacroScheduler->destroyNode(pNode);
    delete pMacroScheduler;
}

//------------------------------------------------------------------------------
struct ObjectWorkload
{
    ObjectWorkload(uint32_t& val) : m_val(val) {}
    static void computeFunc(Workload* pThisWorkload, WorkloadContext const&)
    {
        MicroScheduler_Workload* pWorkload = (MicroScheduler_Workload*)pThisWorkload;
        ObjectWorkload* pData = (ObjectWorkload*)pWorkload->getData();
        pData->m_val = 2;
    }

    uint32_t& m_val;
};

//------------------------------------------------------------------------------
TEST(Node, SetObjectWorkload)
{
    //
    // Setup CpuComputeResource

    WorkerPool workerPool;
    workerPool.initialize(1);

    MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    MicroScheduler_ComputeResource cpuComputeResource(&taskScheduler);

    //
    // Init MacroScheduler

    MacroSchedulerDesc generalizedDagSchedulerDesc;
    generalizedDagSchedulerDesc.computeResourcesByType[(uint32_t)ComputeResourceType::CPU_MicroScheduler].push_back(&cpuComputeResource);

    MacroScheduler* pMacroScheduler = new Dynamic_MacroScheduler;
    pMacroScheduler->init(generalizedDagSchedulerDesc);

    //
    // Build DAG Node

    uint32_t val = 0;
    Node* pNode = pMacroScheduler->allocateNode();
    pNode->setWorkload<MicroScheduler_Workload, ObjectWorkload>(val);

    //
    // Build and execute the schedule

    ISchedule* pSchedule = pMacroScheduler->buildSchedule(pNode, pNode);
    pMacroScheduler->executeSchedule(pSchedule);

    ASSERT_EQ(val, 2u);

    //
    // Cleanup.

    pMacroScheduler->freeSchedule(pSchedule);
    pMacroScheduler->destroyNode(pNode);
    delete pMacroScheduler;
}


//------------------------------------------------------------------------------
TEST(Node, SetLambdaWorkload)
{
    //
    // Setup CpuComputeResource

    WorkerPool workerPool;
    workerPool.initialize(1);

    MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    MicroScheduler_ComputeResource cpuComputeResource(&taskScheduler);

    //
    // Init MacroScheduler

    MacroSchedulerDesc generalizedDagSchedulerDesc;
    generalizedDagSchedulerDesc.computeResourcesByType[(uint32_t)ComputeResourceType::CPU_MicroScheduler].push_back(&cpuComputeResource);

    MacroScheduler* pMacroScheduler = new Dynamic_MacroScheduler;
    pMacroScheduler->init(generalizedDagSchedulerDesc);

    //
    // Build DAG Node

    uint32_t val = 0;
    Node* pNode = pMacroScheduler->allocateNode();
    pNode->setWorkload<MicroScheduler_Workload>([&val](Workload*, WorkloadContext const&)
    {
        val = 3;
    });

    //
    // Build and execute the schedule

    ISchedule* pSchedule = pMacroScheduler->buildSchedule(pNode, pNode);
    pMacroScheduler->executeSchedule(pSchedule);

    ASSERT_EQ(val, 3u);

    //
    // Cleanup.

    pMacroScheduler->freeSchedule(pSchedule);
    pMacroScheduler->destroyNode(pNode);
    delete pMacroScheduler;
}

//------------------------------------------------------------------------------
TEST(Node, SetFunctionalWorkload)
{
    //
    // Setup CpuComputeResource

    WorkerPool workerPool;
    workerPool.initialize(1);

    MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    MicroScheduler_ComputeResource cpuComputeResource(&taskScheduler);

    //
    // Init MacroScheduler

    MacroSchedulerDesc generalizedDagSchedulerDesc;
    generalizedDagSchedulerDesc.computeResourcesByType[(uint32_t)ComputeResourceType::CPU_MicroScheduler].push_back(&cpuComputeResource);

    MacroScheduler* pMacroScheduler = new Dynamic_MacroScheduler;
    pMacroScheduler->init(generalizedDagSchedulerDesc);

    //
    // Build DAG Node

    uint32_t val = 0;
    Node* pNode = pMacroScheduler->allocateNode();
    pNode->setWorkload<MicroScheduler_Workload>([](uint32_t& val, Workload*, WorkloadContext const&)
    {
        val = 4;
    }, std::ref(val));

    //
    // Build and execute the schedule

    ISchedule* pSchedule = pMacroScheduler->buildSchedule(pNode, pNode);
    pMacroScheduler->executeSchedule(pSchedule);

    ASSERT_EQ(val, 4u);

    //
    // Cleanup.

    pMacroScheduler->freeSchedule(pSchedule);
    pMacroScheduler->destroyNode(pNode);
    delete pMacroScheduler;
}

} // namespace testing
