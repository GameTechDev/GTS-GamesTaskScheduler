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

#include <gtest/gtest.h>

#include "gts/containers/Vector.h"
#include "gts/containers/parallel/QueueMPMC.h"

#include "gts/macro_scheduler/Node.h"
#include "gts/macro_scheduler/compute_resources/MicroScheduler_Workload.h"
#include "gts/macro_scheduler/compute_resources/MicroScheduler_ComputeResource.h"
#include "gts/macro_scheduler/DagUtils.h"

#include "SchedulerTestsCommon.h"
#include "macro_scheduler/ComputeResourceFactory.h"

using namespace ::gts;
using namespace ::testing;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
template<typename TMacroScheduler>
class MacroSchedulerTester
{
private:

    struct DagWorkload : public MicroScheduler_Workload
    {
        GTS_INLINE DagWorkload(
            uint32_t id,
            gts::QueueMPMC<uint32_t>& executionQueue)
            : id(id)
            , executionQueue(executionQueue)
        {}

        GTS_INLINE virtual void execute(WorkloadContext const&) final
        {
            //printf("%d\n", id);
            if (!executionQueue.tryPush(id))
            {
                GTS_ASSERT(0);
            }
        }

        uint32_t id;
        gts::QueueMPMC<uint32_t>& executionQueue;
    };

public:

    //--------------------------------------------------------------------------
    GTS_INLINE static void makeDiamondDag(MacroScheduler* pMacroScheduler, Vector<Node*>& dag)
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

        //DagUtils::printToDot("outDag.gv", dag[0]); // DEBUG with GraphViz
    }

    //--------------------------------------------------------------------------
    GTS_INLINE static void makeRandomDag(MacroScheduler* pMacroScheduler, Vector<Node*>& dag)
    {
        const uint32_t NUM_RANKS = 100;
        const uint32_t MIN_NODES_PER_RANK = 3;
        const uint32_t MAX_NODES_PER_RANK = 10;
        const uint32_t CHANCE_OF_INCOMING_END = 50;

        DagUtils::generateRandomDag(pMacroScheduler, 1, NUM_RANKS, MIN_NODES_PER_RANK, MAX_NODES_PER_RANK, CHANCE_OF_INCOMING_END, dag);

        //DagUtils::printToDot("outDag.gv", dag[0]); // DEBUG with GraphViz
    }

public:

    //--------------------------------------------------------------------------
    template<typename TDagGenerator>
    GTS_INLINE static void run(TDagGenerator fcnDagGenerator, Vector<ComputeResource*>const& computeResources, uint32_t iterations)
    {
        //
        // Init MacroScheduler

        MacroSchedulerDesc macroSchedulerDesc;
        macroSchedulerDesc.computeResources = computeResources;

        MacroScheduler* pMacroScheduler = new TMacroScheduler;
        pMacroScheduler->init(macroSchedulerDesc);

        //
        // Build DAG Nodes

        Vector<Node*> nodes;
        fcnDagGenerator(pMacroScheduler, nodes);

        gts::QueueMPMC<uint32_t> executionQueue;

        for (uint32_t ii = 0; ii < nodes.size(); ++ii)
        {
            nodes[ii]->addWorkload<DagWorkload>(ii, executionQueue);
            nodes[ii]->setName("%d", ii);
        }

        //
        // Build and execute the schedule

        Schedule* pSchedule = pMacroScheduler->buildSchedule(nodes.front(), nodes.back());

        for (uint32_t iter = 0; iter < iterations; ++iter)
        {
            pMacroScheduler->executeSchedule(pSchedule, computeResources[0]->id());

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
        }

        //
        // Cleanup

        pMacroScheduler->freeSchedule(pSchedule);

        for (uint32_t ii = 0; ii < nodes.size(); ++ii)
        {
            pMacroScheduler->destroyNode(nodes[ii]);
        }

        delete pMacroScheduler;
    }
};

//------------------------------------------------------------------------------
GTS_INLINE ComputeResourceFactory* createMircoSchedulerComputeResourceFactory(uint32_t numResources, uint32_t threadCount)
{
    return new MircoSchedulerComputeResourceFactory(numResources, threadCount);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
struct ExecutionOrderTestParams
{
    ComputeResourceFactory* pComputeResourceFactory;
    uint32_t iterations;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
struct ExecutionOrderTest : public TestWithParam<ExecutionOrderTestParams>
{
    GTS_INLINE void SetUp() final
    {
        m_params = GetParam();
        m_params.pComputeResourceFactory->buildComputeResources(m_pkg);

        GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);
    }

    GTS_INLINE void TearDown() final
    {
        delete m_params.pComputeResourceFactory;
    }

    ExecutionOrderTestParams m_params;
    ComputeResourcePackage m_pkg;
};

