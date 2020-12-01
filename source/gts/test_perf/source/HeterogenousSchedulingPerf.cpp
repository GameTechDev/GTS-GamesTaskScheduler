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
#include <chrono>
#include <inttypes.h>

#include "gts_perf/PerfTests.h"

#include "gts/analysis/ConcurrentLogger.h"
#include "gts/containers/parallel/QueueMPMC.h"

#include "gts/micro_scheduler/WorkerPool.h"
#include "gts/micro_scheduler/MicroScheduler.h"

#include "gts/macro_scheduler/DagUtils.h"
#include "gts/macro_scheduler/Node.h"
#include "gts/macro_scheduler/compute_resources/MicroScheduler_Workload.h"
#include "gts/macro_scheduler/compute_resources/MicroScheduler_ComputeResource.h"

// micro_scheduler
#include "gts/macro_scheduler/schedulers/dynamic/micro_scheduler/DynamicMicroScheduler_ComputeResource.h"
#include "gts/macro_scheduler/schedulers/dynamic/micro_scheduler/DynamicMicroScheduler_MacroScheduler.h"
#include "gts/macro_scheduler/schedulers/dynamic/micro_scheduler/DynamicMicroScheduler_Schedule.h"

// critically_aware_task_scheduling
#include "gts/macro_scheduler/schedulers/dynamic/critically_aware_task_scheduling/CriticallyAware_MacroScheduler.h"
#include "gts/macro_scheduler/schedulers/dynamic/critically_aware_task_scheduling/CriticallyAware_Schedule.h"

#include <Windows.h>

using namespace gts;

namespace {

//------------------------------------------------------------------------------
struct ComputeResourceData
{
    size_t count                       = 0;
    WorkerPool** pWorkerPool           = nullptr;
    MicroScheduler** pMicroScheduler   = nullptr;
    ComputeResource** pComputeResource = nullptr;

    ComputeResourceData(size_t c) : count(c)
    {
        pWorkerPool      = new WorkerPool*[count];
        pMicroScheduler  = new MicroScheduler*[count];
        pComputeResource = new ComputeResource*[count];
    }

    ~ComputeResourceData()
    {
        // Must delete in reverse order!
        for (size_t ii = 0; ii < count; ++ii)
        {
            alignedDelete(pComputeResource[count - ii - 1]);
            alignedDelete(pMicroScheduler[count - ii - 1]);
            alignedDelete(pWorkerPool[count - ii - 1]);
        }

        delete[] pComputeResource;
        delete[] pMicroScheduler;
        delete[] pWorkerPool;
    }
};

//------------------------------------------------------------------------------
void createHomogeneousComputeResources(ComputeResourceData& out)
{
    constexpr uint32_t numResources = 2;
    constexpr uint32_t bigCoreCount = 8;
    constexpr uint32_t littleCoreCount = 8;

    AffinitySet allCoresAffinity;
    for (uint32_t iCore = 0; iCore < bigCoreCount; ++iCore)
    {
        allCoresAffinity.set(iCore);
    }
    for (uint32_t iCore = 16; iCore < 16 + littleCoreCount; ++iCore)
    {
        allCoresAffinity.set(iCore);
    }

    Vector<ComputeResource*> computeResources;

    WorkerPoolDesc workerPoolDesc;
    for (uint32_t ii = 0; ii < bigCoreCount + littleCoreCount; ++ii)
    {
        WorkerThreadDesc desc;
        desc.affinity.affinitySet = allCoresAffinity;
        workerPoolDesc.workerDescs.push_back(desc);
    }

    out.pWorkerPool[0] = alignedNew<WorkerPool, GTS_NO_SHARING_CACHE_LINE_SIZE>();
    out.pWorkerPool[0]->initialize(workerPoolDesc);

    out.pMicroScheduler[0] = alignedNew<MicroScheduler, GTS_NO_SHARING_CACHE_LINE_SIZE>();
    out.pMicroScheduler[0]->initialize(out.pWorkerPool[0]);

    out.pComputeResource[0] = alignedNew<DynamicMicroScheduler_ComputeResource, GTS_NO_SHARING_CACHE_LINE_SIZE>(out.pMicroScheduler[0], 0);
}

//------------------------------------------------------------------------------
void createHeterogeneousComputeResources(ComputeResourceData& out)
{
    constexpr uint32_t numResources = 2;
    constexpr uint32_t bigCoreCount = 8;
    constexpr uint32_t littleCoreCount = 8;

    //
    // Create big cores ComputeResource

    AffinitySet bigCoreAffinity;
    for (uint32_t iCore = 0; iCore < bigCoreCount; ++iCore)
    {
        bigCoreAffinity.set(iCore);
    }

    WorkerPoolDesc bigWorkerPoolDesc;
    for (uint32_t ii = 0; ii < bigCoreCount; ++ii)
    {
        WorkerThreadDesc desc;
        snprintf(desc.name, DESC_NAME_SIZE, "Big Worker %d", ii);
        desc.affinity.affinitySet = bigCoreAffinity;
        bigWorkerPoolDesc.workerDescs.push_back(desc);
    }

    out.pWorkerPool[0] = alignedNew<WorkerPool, GTS_NO_SHARING_CACHE_LINE_SIZE>();
    out.pWorkerPool[0]->initialize(bigWorkerPoolDesc);

    out.pMicroScheduler[0] = alignedNew<MicroScheduler, GTS_NO_SHARING_CACHE_LINE_SIZE>();
    out.pMicroScheduler[0]->initialize(out.pWorkerPool[0]);

    out.pComputeResource[0] = alignedNew<MicroScheduler_ComputeResource, GTS_NO_SHARING_CACHE_LINE_SIZE>(out.pMicroScheduler[0], 0);
    out.pComputeResource[0]->setExecutionNormalizationFactor(1);

    ThisThread::setAffinity(0, bigCoreAffinity);

    //
    // Create little cores ComputeResource

    AffinitySet littleCoreAffinity;
    for (uint32_t iCore = 16; iCore < 16 + littleCoreCount; ++iCore)
    {
        littleCoreAffinity.set(iCore);
    }

    WorkerPoolDesc littleWorkerPoolDesc;
    for (uint32_t ii = 0; ii < littleCoreCount + 1; ++ii) // +1 since the little cores will not used the main thread.
    {
        WorkerThreadDesc desc;
        snprintf(desc.name, DESC_NAME_SIZE, "Little Worker %d", ii);
        desc.affinity.affinitySet = littleCoreAffinity;
        littleWorkerPoolDesc.workerDescs.push_back(desc);
    }

    out.pWorkerPool[1] = alignedNew<WorkerPool, GTS_NO_SHARING_CACHE_LINE_SIZE>();
    out.pWorkerPool[1]->initialize(littleWorkerPoolDesc);

    out.pMicroScheduler[1] = alignedNew<MicroScheduler, GTS_NO_SHARING_CACHE_LINE_SIZE>();
    out.pMicroScheduler[1]->initialize(out.pWorkerPool[1]);

    out.pComputeResource[1] = alignedNew<MicroScheduler_ComputeResource, GTS_NO_SHARING_CACHE_LINE_SIZE>(out.pMicroScheduler[1], 0);
    out.pComputeResource[1]->setExecutionNormalizationFactor(2);

    // 1-way stealing.
    out.pMicroScheduler[0]->addExternalVictim(out.pMicroScheduler[1]);
}

//------------------------------------------------------------------------------
struct SinSpinWorkload : public MicroScheduler_Workload
{
    SinSpinWorkload(uint32_t iterations)
        : iterations(iterations)
    {}

    GTS_INLINE virtual void execute(WorkloadContext const&) final
    {
        GTS_TRACE_SCOPED_ZONE_P0(gts::analysis::CaptureMask::MACRO_SCHEDULER_PROFILE, gts::analysis::Color::Cyan, "SinSpinWorkload");
        //printf("%d\n", GetCurrentProcessorNumber());
        result = (float)iterations;
        for (uint32_t ii = 0; ii < iterations; ++ii)
        {
            result = sin(result);
        }
    }

    uint32_t iterations;
    float result;
};

//------------------------------------------------------------------------------
template<typename TMacroScheduler>
void makeRandomDag(TMacroScheduler* pMacroScheduler, Vector<Node*>& dag)
{
#if 0
    const uint32_t NUM_RANKS = 4;
    const uint32_t MIN_NODES_PER_RANK = 2;
    const uint32_t MAX_NODES_PER_RANK = 3;
    const uint32_t CHANCE_OF_INCOMING_END = 0;
#else
    const uint32_t NUM_RANKS = 500;
    const uint32_t MIN_NODES_PER_RANK = 1;
    const uint32_t MAX_NODES_PER_RANK = 1;
    const uint32_t CHANCE_OF_INCOMING_END = 0;
#endif

    DagUtils::generateRandomDag(pMacroScheduler, 1, NUM_RANKS, MIN_NODES_PER_RANK, MAX_NODES_PER_RANK, CHANCE_OF_INCOMING_END, dag);

    DagUtils::printToDot("outDag.gv", dag); // DEBUG with GraphViz
}

//------------------------------------------------------------------------------
template<typename TMacroScheduler>
void buildSchedulerDagNodes(
    Vector<Node*> const& out,
    TMacroScheduler* pMacroScheduler,
    Vector<Node> const& dag,
    Vector<Node*> const& criticalPath)
{
    for (uint32_t ii = 0; ii < out.size(); ++ii)
    {
        bool isOnCriticalPath = false;

        for (uint32_t iPath = 0; iPath < criticalPath.size(); ++iPath)
        {
            if (criticalPath[iPath]->myIndex == ii)
            {
                isOnCriticalPath = true;
                break;
            }
        }

        Node* pNode = pMacroScheduler->allocateNode();
        pNode->setName("%d", ii);

        if (isOnCriticalPath)
        {
            pNode->addWorkload<SinSpinWorkload>(RAND_MAX);
        }
        else
        {
            pNode->addWorkload<SinSpinWorkload>(RAND_MAX / 2);
        }

        nodes[ii] = pNode;
    }
}

//------------------------------------------------------------------------------
template<typename TMacroScheduler>
Stats heteroRandomDagTest(Vector<ComputeResource*>const& computeResources, uint32_t iterations)
{
    Stats stats(iterations);

#define TEST_GRAPH_WIDTH 1

#if 1

    const uint32_t NUM_RANKS              = 300;
    const uint32_t CHANCE_OF_INCOMING_END = 0;
    const uint32_t MAX_STEPS              = 60;

    const uint32_t MIN_SPIN  = RAND_MAX / 4;
    const uint32_t MAX_SPIN  = RAND_MAX;
#if !TEST_GRAPH_WIDTH
    const uint32_t STEP_SIZE = (MAX_SPIN - MIN_SPIN) / MAX_STEPS;
#endif

    Vector<uint64_t> totalWork(MAX_STEPS);
    Vector<uint64_t> critWork(MAX_STEPS);
    Vector<size_t> totalNodes(MAX_STEPS);
    Vector<size_t> critNodes(MAX_STEPS);
    Vector<double> time(MAX_STEPS);

    for (uint32_t iStep = 0; iStep < MAX_STEPS; ++iStep)
    {
        //
        // Init MacroScheduler

        MacroSchedulerDesc macroSchedulerDesc;
        macroSchedulerDesc.computeResources = computeResources;

        TMacroScheduler* pMacroScheduler = new TMacroScheduler;
        pMacroScheduler->init(macroSchedulerDesc);

        //
        // Build DAG Nodes

        Vector<Node*> nodes;

#if TEST_GRAPH_WIDTH
        DagUtils::generateRandomDag(pMacroScheduler, 1, NUM_RANKS, iStep + 1, iStep + 1, CHANCE_OF_INCOMING_END, nodes);
#else
        DagUtils::generateRandomDag(pMacroScheduler, 1, NUM_RANKS, 15, 15, CHANCE_OF_INCOMING_END, nodes);
#endif

        DagUtils::printToDot("outDag.gv", nodes[0]); // DEBUG with GraphViz

        Vector<Node*> criticalPath;
        DagUtils::getCriticalPath(criticalPath, nodes);

        //for (uint32_t ii = 0; ii < criticalPath.size(); ++ii)
        //{
        //    printf("%zu\n", criticalPath[ii]->myIndex);
        //}

        for (uint32_t ii = 0; ii < nodes.size(); ++ii)
        {
            bool isOnCriticalPath = false;

            for (uint32_t iPath = 0; iPath < criticalPath.size(); ++iPath)
            {
                if (criticalPath[iPath] == nodes[ii])
                {
                    isOnCriticalPath = true;
                    break;
                }
            }

            Node* pNode = nodes[ii];

#if TEST_GRAPH_WIDTH
            if (isOnCriticalPath)
            {
                pNode->addWorkload<SinSpinWorkload>(MAX_SPIN);
            }
            else
            {
                pNode->addWorkload<SinSpinWorkload>(MIN_SPIN);
            }
#else
            if (isOnCriticalPath)
            {
                pNode->addWorkload<SinSpinWorkload>(MAX_SPIN);
            }
            else
            {
                pNode->addWorkload<SinSpinWorkload>(MIN_SPIN + STEP_SIZE * iStep);
            }
#endif
        }

        //
        // Build and execute the schedule

        Schedule* pSchedule = pMacroScheduler->buildSchedule(nodes.front(), nodes.back());


        GTS_TRACE_SET_CAPTURE_MASK(gts::analysis::CaptureMask::MACRO_SCHEDULER_ALL);

        for (uint32_t iter = 0; iter < iterations; ++iter)
        {
            GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);

            auto start = std::chrono::high_resolution_clock::now();

            pMacroScheduler->executeSchedule(pSchedule, computeResources[0]->id());

            auto end = std::chrono::high_resolution_clock::now();

            std::chrono::duration<double> diff = end - start;
            stats.addDataPoint(diff.count());
        }

        DagUtils::totalWork(totalWork[iStep], totalNodes[iStep], nodes.front());
        DagUtils::workOnCriticalPath(critWork[iStep], criticalPath);

        pMacroScheduler->freeSchedule(pSchedule);

        for (uint32_t ii = 0; ii < nodes.size(); ++ii)
        {
            pMacroScheduler->destroyNode(nodes[ii]);
        }

        time[iStep] = stats.mean();
        printf("time: %f\n", stats.mean());
        printf("stddev: %f\n", stats.standardDeviation());
    }

    printf("\nTotal Node Count,");
    for (uint32_t iStep = 0; iStep < MAX_STEPS; ++iStep)
    {
        printf("%zu", totalNodes[iStep]);
    }
    printf("\nSpan Node Count,");
    for (uint32_t iStep = 0; iStep < MAX_STEPS; ++iStep)
    {
        printf("%zu", critNodes[iStep]);
    }
    printf("\nTotal Work,");
    for (uint32_t iStep = 0; iStep < MAX_STEPS; ++iStep)
    {
        printf("%" PRIu64 ",", totalWork[iStep]);
    }
    printf("\nSpan Work,");
    for (uint32_t iStep = 0; iStep < MAX_STEPS; ++iStep)
    {
        printf("%" PRIu64 ",", critWork[iStep]);
    }
    printf("\nSpatial Parallelism,");
    for (uint32_t iStep = 0; iStep < MAX_STEPS; ++iStep)
    {
        printf("%f,", totalNodes[iStep] / double(critNodes[iStep]));
    }
    printf("\nParallelism,");
    for (uint32_t iStep = 0; iStep < MAX_STEPS; ++iStep)
    {
        printf("%f,", totalWork[iStep] / double(critWork[iStep]));
    }
    printf("\nExecution Time,");
    for (uint32_t iStep = 0; iStep < MAX_STEPS; ++iStep)
    {
        printf("%f,", time[iStep]);
    }

    printf("\n");

#else

    Vector<DagUtils::Node> dag;
    makeRandomDag(dag);

    Vector<DagUtils::Node*> criticalPath;
    DagUtils::getCriticalPath(criticalPath, dag);

    //for (uint32_t ii = 0; ii < criticalPath.size(); ++ii)
    //{
    //    printf("%zu\n", criticalPath[ii]->myIndex);
    //}

    //
    // Init MacroScheduler

    MacroSchedulerDesc macroSchedulerDesc;
    macroSchedulerDesc.computeResources = computeResources;

    TMacroScheduler* pMacroScheduler = new TMacroScheduler;
    pMacroScheduler->init(macroSchedulerDesc);

    //
    // Build DAG Nodes

    Vector<Node*> nodes(dag.size());
    for (uint32_t ii = 0; ii < nodes.size(); ++ii)
    {
        bool isOnCriticalPath = false;

        for (uint32_t iPath = 0; iPath < criticalPath.size(); ++iPath)
        {
            if (criticalPath[iPath]->myIndex == ii)
            {
                isOnCriticalPath = true;
                break;
            }
        }

        Node* pNode = pMacroScheduler->allocateNode();
        pNode->setName("%d", ii);

        if (isOnCriticalPath)
        {
            pNode->addWorkload<SinSpinWorkload>(RAND_MAX * 2);
        }
        else
        {
            pNode->addWorkload<SinSpinWorkload>(RAND_MAX / 2);
        }

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

    Schedule* pSchedule = pMacroScheduler->buildSchedule(nodes.front(), nodes.back());


    GTS_TRACE_SET_CAPTURE_MASK(gts::analysis::CaptureMask::MACRO_SCHEDULER_ALL);

    for (uint32_t iter = 0; iter < iterations; ++iter)
    {
        GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);

        auto start = std::chrono::high_resolution_clock::now();

        pMacroScheduler->executeSchedule(pSchedule, computeResources[0]->id());

        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double> diff = end - start;
        stats.addDataPoint(diff.count());
    }

    uint64_t totalWork = 0;
    uint64_t totalNodes = 0;
    Node::totalWork(totalWork, totalNodes, nodes.front());

    uint64_t critWork = 0;
    uint64_t critNodes = 0;
    Node::workOnCriticalPath(critWork, critNodes, nodes.front(), 2);

    if (critWork > 0)
    {
        printf("Total Work          : %" PRIu64 " cycles\n", totalWork);
        printf("Critical Path Work  : %" PRIu64 " cycles\n", critWork);
        printf("Work Parallelism    : %f\n", totalWork / double(critWork));

        printf("Total Nodes         : %" PRIu64 "\n", totalNodes);
        printf("Critical Path Nodes : %" PRIu64 "\n", critNodes);
        printf("Spatial Parallelism : %f\n", totalNodes / double(critNodes));

        totalWork *= totalNodes;
        critWork *= critNodes;
        printf("Parallelism         : %f\n", totalWork / double(critWork));
    }

    pMacroScheduler->freeSchedule(pSchedule);
#endif

    return stats;
}

} // namespace

//------------------------------------------------------------------------------
Stats homoRandomDagWorkStealing(uint32_t iterations)
{
    ComputeResourceData data(1);
    createHomogeneousComputeResources(data);

    Vector<ComputeResource*> computeResources;
    computeResources.push_back(data.pComputeResource[0]);

    Stats stats = heteroRandomDagTest<DynamicMicroScheduler_MacroScheduler>(computeResources, iterations);

    return stats;
}

//------------------------------------------------------------------------------
Stats heteroRandomDagWorkStealing(uint32_t iterations, bool bidirectionalStealing)
{
    constexpr uint32_t numResources    = 2;
    constexpr uint32_t bigCoreCount    = 8;
    constexpr uint32_t littleCoreCount = 8;

    Vector<ComputeResource*> computeResources;

    //
    // Create big cores ComputeResource

    AffinitySet bigCoreAffinity;
    for (uint32_t iCore = 0; iCore < bigCoreCount; ++iCore)
    {
        bigCoreAffinity.set(iCore);
    }

    WorkerPoolDesc bigWorkerPoolDesc;
    for (uint32_t ii = 0; ii < bigCoreCount; ++ii)
    {
        WorkerThreadDesc desc;
        desc.affinity.affinitySet = bigCoreAffinity;
        bigWorkerPoolDesc.workerDescs.push_back(desc);
    }

    WorkerPool* pBigPool = alignedNew<WorkerPool, GTS_NO_SHARING_CACHE_LINE_SIZE>();
    pBigPool->initialize(bigWorkerPoolDesc);

    MicroScheduler* pBigScheduler = alignedNew<MicroScheduler, GTS_NO_SHARING_CACHE_LINE_SIZE>();
    pBigScheduler->initialize(pBigPool);

    DynamicMicroScheduler_ComputeResource* pBigComputeResource = alignedNew<DynamicMicroScheduler_ComputeResource, GTS_NO_SHARING_CACHE_LINE_SIZE>(pBigScheduler, 0);
    computeResources.push_back(pBigComputeResource);

    //
    // Create little cores ComputeResource

    AffinitySet littleCoreAffinity;
    for (uint32_t iCore = 16; iCore < 16 + littleCoreCount; ++iCore)
    {
        littleCoreAffinity.set(iCore);
    }

    WorkerPoolDesc littleWorkerPoolDesc;
    for (uint32_t ii = 0; ii < littleCoreCount + 1; ++ii) // +1 since the little cores will not used the main thread.
    {
        WorkerThreadDesc desc;
        desc.affinity.affinitySet = littleCoreAffinity;
        littleWorkerPoolDesc.workerDescs.push_back(desc);
    }

    WorkerPool* pLittlePool = alignedNew<WorkerPool, GTS_NO_SHARING_CACHE_LINE_SIZE>();
    pLittlePool->initialize(littleWorkerPoolDesc);

    MicroScheduler* pLittleScheduler = alignedNew<MicroScheduler, GTS_NO_SHARING_CACHE_LINE_SIZE>();
    pLittleScheduler->initialize(pLittlePool);

    DynamicMicroScheduler_ComputeResource* pLittleComputeResource = alignedNew<DynamicMicroScheduler_ComputeResource, GTS_NO_SHARING_CACHE_LINE_SIZE>(pLittleScheduler, 0);
    computeResources.push_back(pLittleComputeResource);


    // 1-way stealing.
    pBigScheduler->addExternalVictim(pLittleScheduler);

    if (bidirectionalStealing)
    {
        // 2-way stealing.
        pLittleScheduler->addExternalVictim(pBigScheduler);
    }

    Stats stats = heteroRandomDagTest<DynamicMicroScheduler_MacroScheduler>(computeResources, iterations);

    alignedDelete(pLittleComputeResource);
    alignedDelete(pBigComputeResource);

    alignedDelete(pLittleScheduler);
    alignedDelete(pBigScheduler);

    alignedDelete(pLittlePool);
    alignedDelete(pBigPool);

    return stats;
}

//------------------------------------------------------------------------------
Stats heteroRandomDagCriticallyAware(uint32_t iterations)
{
    constexpr uint32_t numResources    = 2;
    constexpr uint32_t bigCoreCount    = 8;
    constexpr uint32_t littleCoreCount = 8;

    ComputeResourceData data(2);
    createHeterogeneousComputeResources(data);

    Vector<ComputeResource*> computeResources;
    computeResources.push_back(data.pComputeResource[0]);
    computeResources.push_back(data.pComputeResource[1]);

    Stats stats = heteroRandomDagTest<CriticallyAware_MacroScheduler>(computeResources, iterations);

    return stats;
}
