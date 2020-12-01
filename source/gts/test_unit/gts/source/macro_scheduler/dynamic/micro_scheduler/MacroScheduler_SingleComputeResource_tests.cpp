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
#include "gts/macro_scheduler/compute_resources/MicroScheduler_Workload.h"
#include "gts/macro_scheduler/DagUtils.h"

// Dynamic
#include "gts/macro_scheduler/schedulers/dynamic/micro_scheduler/DynamicMicroScheduler_MacroScheduler.h"
#include "gts/macro_scheduler/schedulers/dynamic/micro_scheduler/DynamicMicroScheduler_ComputeResource.h"

#include "SchedulerTestsCommon.h"

using namespace gts;

namespace testing {

//------------------------------------------------------------------------------
struct DagWorkload : public MicroScheduler_Workload
{
    DagWorkload(
        uint32_t id,
        gts::QueueMPMC<uint32_t>& executionQueue)
        : id(id)
        , executionQueue(executionQueue)
    {}

    GTS_INLINE virtual void execute(WorkloadContext const&) final
    {
        if (!executionQueue.tryPush(id))
        {
            GTS_ASSERT(0);
        }
    }

    uint32_t id;
    gts::QueueMPMC<uint32_t>& executionQueue;
};

//------------------------------------------------------------------------------
template<typename TDagGenerator>
void dagTester(TDagGenerator fcnDagGenerator, uint32_t threadCount)
{
    //
    // Setup CpuComputeResource

    WorkerPool workerPool;
    workerPool.initialize(threadCount);

    MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    DynamicMicroScheduler_ComputeResource microSchedulerCompResource(&taskScheduler, 0);

    //
    // Init MacroScheduler

    MacroSchedulerDesc generalizedDagSchedulerDesc;
    generalizedDagSchedulerDesc.computeResources.push_back(&microSchedulerCompResource);

    MacroScheduler* pMacroScheduler = new DynamicMicroScheduler_MacroScheduler;
    pMacroScheduler->init(generalizedDagSchedulerDesc);

    //
    // Build DAG Nodes

    Vector<Node*> nodes;
    fcnDagGenerator(pMacroScheduler, nodes);

    gts::QueueMPMC<uint32_t> executionQueue;

    for (uint32_t ii = 0; ii < nodes.size(); ++ii)
    {
        nodes[ii]->addWorkload<DagWorkload>(ii, executionQueue);
    }

    //
    // Build and execute the schedule

    Schedule* pSchedule = pMacroScheduler->buildSchedule(nodes.front(), nodes.back());
    pMacroScheduler->executeSchedule(pSchedule, microSchedulerCompResource.id());

    Vector<uint32_t> executionOrder;
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
    ASSERT_TRUE(DagUtils::isATopologicalOrdering(nodes, executionOrder));

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
void makeDiamondDag(MacroScheduler* pMacroScheduler, Vector<Node*>& dag)
{
    dag.resize(4);
    for (uint32_t ii = 0; ii < 4; ++ii)
    {
        dag[ii] = pMacroScheduler->allocateNode();
    }

    dag[0]->addSuccessor(dag[1]);
    dag[0]->addSuccessor(dag[2]);

    dag[1]->addSuccessor(dag[3]);
    dag[2]->addSuccessor(dag[3]);

    DagUtils::printToDot("outDag.gv", dag[0]); // DEBUG with GraphViz
}

//------------------------------------------------------------------------------
TEST(MacroScheduler, DiamondTest_OneThread)
{
    dagTester(makeDiamondDag, 1);
}

//------------------------------------------------------------------------------
TEST(MacroScheduler, DiamondTest_ManyThreads)
{
    for (uint32_t ii = 0; ii < ITERATIONS_CONCUR; ++ii)
    {
        GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);
        dagTester(makeDiamondDag, gts::Thread::getHardwareThreadCount());
    }
}

//------------------------------------------------------------------------------
void makeRandomDag(MacroScheduler* pMacroScheduler, Vector<Node*>& dag)
{
    const uint32_t NUM_RANKS              = 100;
    const uint32_t MIN_NODES_PER_RANK     = 3;
    const uint32_t MAX_NODES_PER_RANK     = 10;
    const uint32_t CHANCE_OF_INCOMING_END = 50;

    DagUtils::generateRandomDag(pMacroScheduler, 1, NUM_RANKS, MIN_NODES_PER_RANK, MAX_NODES_PER_RANK, CHANCE_OF_INCOMING_END, dag);

    //DagUtils::dagToDotFile("outDag.gv", dag); // DEBUG with GraphViz
}

//------------------------------------------------------------------------------
TEST(MacroScheduler, RandomDagTest_OneThread)
{
    dagTester(makeRandomDag, 1);
}

//------------------------------------------------------------------------------
TEST(MacroScheduler, RandomDagTest_ManyThreads)
{
    for (uint32_t ii = 0; ii < ITERATIONS_CONCUR; ++ii)
    {
        GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);
        dagTester(makeRandomDag, gts::Thread::getHardwareThreadCount());
    }
}

} // namespace testing
