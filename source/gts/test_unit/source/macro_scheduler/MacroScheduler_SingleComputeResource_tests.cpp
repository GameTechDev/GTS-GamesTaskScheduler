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

#include "gts/analysis/ConcurrentLogger.h"
#include "gts/containers/parallel/QueueMPMC.h"

#include "gts/micro_scheduler/WorkerPool.h"
#include "gts/micro_scheduler/MicroScheduler.h"

#include "gts/macro_scheduler/Node.h"

#include "DagUtils.h"

// Dynamic
#include "gts/macro_scheduler/schedulers/dynamic/Dynamic_MacroScheduler.h"
#include "gts/macro_scheduler/compute_resources/MicroScheduler_ComputeResource.h"
#include "gts/macro_scheduler/compute_resources/MicroScheduler_Workload.h"

#include "SchedulerTestsCommon.h"

using namespace gts;

namespace testing {

//------------------------------------------------------------------------------
struct DagWorkload
{
    DagWorkload(
        uint32_t id,
        gts::QueueMPMC<uint32_t>& executionQueue)
        : id(id)
        , executionQueue(executionQueue)
    {}

    GTS_INLINE static void computeFunc(Workload* pThisWorkload, WorkloadContext const&)
    {
        MicroScheduler_Workload* pWorkload = (MicroScheduler_Workload*)pThisWorkload;
        DagWorkload* pData = (DagWorkload*)pWorkload->getData();

        if (!pData->executionQueue.tryPush(pData->id))
        {
            GTS_ASSERT(0);
        }
    }

    uint32_t id;
    gts::QueueMPMC<uint32_t>& executionQueue;
};

//------------------------------------------------------------------------------
template<typename TComputeResource, typename TWorkload, typename TDagScheduler>
void DagTester(gts::Vector<DagUtils::Node>const& dag, uint32_t threadCount)
{
    //
    // Setup CpuComputeResource

    WorkerPool workerPool;
    workerPool.initialize(threadCount);

    MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    TComputeResource cpuComputeResource(&taskScheduler);

    //
    // Init MacroScheduler

    MacroSchedulerDesc generalizedDagSchedulerDesc;
    generalizedDagSchedulerDesc.computeResourcesByType[(uint32_t)ComputeResourceType::CPU_MicroScheduler].push_back(&cpuComputeResource);

    MacroScheduler* pMacroScheduler = new TDagScheduler;
    pMacroScheduler->init(generalizedDagSchedulerDesc);

    //
    // Build DAG Nodes

    gts::QueueMPMC<uint32_t> executionQueue;

    gts::Vector<Node*> nodes(dag.size());
    for (uint32_t ii = 0; ii < nodes.size(); ++ii)
    {
        //Node* pNode = pMacroScheduler->allocateNode();
        //pNode->setWorkload<TWorkload>([](const uint32_t id, gts::QueueMPMC<uint32_t>& executionQueue)
        //{
        //    if (!executionQueue.tryPush(id))
        //    {
        //        GTS_ASSERT(0);
        //    }
        //},
        //ii, std::ref(executionQueue));

        Node* pNode = pMacroScheduler->allocateNode();
        pNode->setWorkload<TWorkload, DagWorkload>(ii, executionQueue);

        nodes[ii] = pNode;
    }

    //
    // Connect DAG Nodes

    for (uint32_t ii = 0; ii < dag.size(); ++ii)
    {
        for (uint32_t jj = 0; jj < dag[ii].successorIndices.size(); ++jj)
        {
            nodes[ii]->addChild(nodes[dag[ii].successorIndices[jj]]);
        }
    }

    //
    // Build and execute the schedule

    ISchedule* pSchedule = pMacroScheduler->buildSchedule(nodes.front(), nodes.back());
    pMacroScheduler->executeSchedule(pSchedule);

    gts::Vector<uint32_t> executionOrder;
    executionOrder.reserve(nodes.size());
    while (!executionQueue.empty())
    {
        uint32_t id = 0;
        if (!executionQueue.tryPop(id))
        {
            GTS_ASSERT(0);
        }

        executionOrder.push_back(id);
    }

    // Verify execution order.
    ASSERT_TRUE(DagUtils::isATopologicalOrdering(dag, executionOrder));

    //
    // Cleanup.

    pMacroScheduler->freeSchedule(pSchedule);

    for (uint32_t ii = 0; ii < nodes.size(); ++ii)
    {
        pMacroScheduler->destroyNode(nodes[ii]);
    }

    delete pMacroScheduler;
}

//------------------------------------------------------------------------------
void makeDiamondDag(gts::Vector<DagUtils::Node>& dag)
{
    dag.resize(4);

    dag[0].successorIndices.push_back(1);
    dag[1].numPredecessors++;

    dag[0].successorIndices.push_back(2);
    dag[2].numPredecessors++;

    dag[1].successorIndices.push_back(3);
    dag[3].numPredecessors++;

    dag[2].successorIndices.push_back(3);
    dag[3].numPredecessors++;

    //DagUtils::dagToDotFile("outDag.gv", dag); // DEBUG with GraphViz
}

//------------------------------------------------------------------------------
TEST(MacroScheduler, DiamondTest_OneThread)
{
    gts::Vector<DagUtils::Node> dag;
    makeDiamondDag(dag);
    DagTester<MicroScheduler_ComputeResource, MicroScheduler_Workload, Dynamic_MacroScheduler>(dag, 1);
}

//------------------------------------------------------------------------------
TEST(MacroScheduler, DiamondTest_ManyThreads)
{
    gts::Vector<DagUtils::Node> dag;
    makeDiamondDag(dag);

    for (uint32_t ii = 0; ii < ITERATIONS_CONCUR; ++ii)
    {
        GTS_CONCRT_LOGGER_RESET();
        DagTester<MicroScheduler_ComputeResource, MicroScheduler_Workload, Dynamic_MacroScheduler>(dag, gts::Thread::getHardwareThreadCount());
    }
}


//------------------------------------------------------------------------------
void makeRandomDag(gts::Vector<DagUtils::Node>& dag)
{
    const uint32_t NUM_RANKS              = 1000;
    const uint32_t MIN_NODES_PER_RANK     = 10;
    const uint32_t MAX_NODES_PER_RANK     = 100;
    const uint32_t CHANCE_OF_INCOMING_END = 50;

    DagUtils::generateRandomDag(1, NUM_RANKS, MIN_NODES_PER_RANK, MAX_NODES_PER_RANK, CHANCE_OF_INCOMING_END, dag);

    //DagUtils::dagToDotFile("outDag.gv", dag); // DEBUG with GraphViz
}

//------------------------------------------------------------------------------
TEST(MacroScheduler, RandomDagTest_OneThread)
{
    gts::Vector<DagUtils::Node> dag;
    makeRandomDag(dag);
    DagTester<MicroScheduler_ComputeResource, MicroScheduler_Workload, Dynamic_MacroScheduler>(dag, 1);
}

//------------------------------------------------------------------------------
TEST(MacroScheduler, RandomDagTest_ManyThreads)
{
    gts::Vector<DagUtils::Node> dag;
    makeRandomDag(dag);

    for (uint32_t ii = 0; ii < ITERATIONS_CONCUR; ++ii)
    {
        GTS_CONCRT_LOGGER_RESET();
        DagTester<MicroScheduler_ComputeResource, MicroScheduler_Workload, Dynamic_MacroScheduler>(dag, gts::Thread::getHardwareThreadCount());
    }
}

} // namespace testing
