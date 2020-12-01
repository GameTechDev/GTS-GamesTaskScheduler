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
#include "gts/macro_scheduler/schedulers/dynamic/critically_aware_task_scheduling/CriticallyAware_MacroScheduler.h"

#include <chrono>

#include "gts/macro_scheduler/DagUtils.h"

#include "gts/micro_scheduler/patterns/ParallelFor.h"

#include "gts/macro_scheduler/Node.h"
#include "gts/macro_scheduler/ComputeResource.h"
#include "gts/macro_scheduler/compute_resources/MicroScheduler_ComputeResource.h"
#include "gts/macro_scheduler/schedulers/dynamic/critically_aware_task_scheduling/CriticallyAware_Schedule.h"

namespace gts {

// LIFETIME:

//------------------------------------------------------------------------------
bool CriticallyAware_MacroScheduler::init(MacroSchedulerDesc const& desc)
{
    if (desc.computeResources.empty())
    {
        GTS_ASSERT(0 && "Must have at least one compute resource.");
        return false;
    }

    m_computeResources.resize(desc.computeResources.size());
    for (size_t ii = 0; ii < desc.computeResources.size(); ++ii)
    {
        m_computeResources[ii] = desc.computeResources[ii];
    }

    // Sort ComputeResource from smallest to largest weight.
    insertionSort(m_computeResources.data(), m_computeResources.size(),
        [](ComputeResource* lhs, ComputeResource* rhs)->bool {
            return lhs->executionNormalizationFactor() < rhs->executionNormalizationFactor();
        });

    return true;
}

// MUTATORS:

//------------------------------------------------------------------------------
Schedule* CriticallyAware_MacroScheduler::buildSchedule(Node* pStart, Node* pEnd)
{
    CriticallyAware_Schedule* pSchedule = unalignedNew<CriticallyAware_Schedule>(this, pStart, pEnd);
    for (size_t ii = 0; ii < m_computeResources.size(); ++ii)
    {
        m_computeResources[ii]->registerSchedule(pSchedule);
    }
    return pSchedule;
}

//--------------------------------------------------------------------------
void CriticallyAware_MacroScheduler::freeSchedule(Schedule* pSchedule)
{
    while (pSchedule->refCount() > 0)
    {
        GTS_PAUSE();
    }

    for (size_t ii = 0; ii < m_computeResources.size(); ++ii)
    {
        m_computeResources[ii]->unregisterSchedule(pSchedule);
    }

    unalignedDelete(pSchedule);
    pSchedule = nullptr;
}

//------------------------------------------------------------------------------
void CriticallyAware_MacroScheduler::executeSchedule(Schedule* pSchedule, ComputeResourceId id)
{
    GTS_TRACE_SCOPED_ZONE_P0(gts::analysis::CaptureMask::MACRO_SCHEDULER_PROFILE, gts::analysis::Color::Cyan, "Execute Schedule");

    GTS_ASSERT(pSchedule->isDone() && "Trying to execute a running schedule!");

    //
    // Reset the schedule.

    CriticallyAware_Schedule* pCritSchedule = (CriticallyAware_Schedule*)pSchedule;
    GTS_ASSERT(pCritSchedule->m_numQueues == m_computeResources.size());
    pCritSchedule->m_isDone.exchange(false, memory_order::acq_rel);
    for (size_t ii = 0; ii < pCritSchedule->m_numQueues; ++ii)
    {
        pCritSchedule->m_pQueuesByCriticalityIndex[ii].maxPriority = pCritSchedule->m_numQueues;
        pCritSchedule->m_pQueuesByCriticalityIndex[ii].pMaxPriorityNode.store(nullptr, memory_order::release);
    }

    Node::resetGraph(pCritSchedule->m_pBegin);

    //DagUtils::printToDot("costedNodes.gv", pCritSchedule->m_pBegin, DagUtils::NodePropertyFlags(DagUtils::NAME | DagUtils::COST));

    {
        GTS_TRACE_SCOPED_ZONE_P0(gts::analysis::CaptureMask::MACRO_SCHEDULER_PROFILE, gts::analysis::Color::Cyan, "Preprocess Nodes");
        pCritSchedule->_preProcessNodes(pCritSchedule->m_pEnd);
    }

    {
        GTS_TRACE_SCOPED_ZONE_P0(gts::analysis::CaptureMask::MACRO_SCHEDULER_PROFILE, gts::analysis::Color::Cyan, "Up-rank Nodes");

        MicroScheduler_ComputeResource* pUpRankCompResource = nullptr;
        for (size_t ii = 0; ii < m_computeResources.size(); ++ii)
        {
            if (m_computeResources[ii]->id() == id)
            {
                pUpRankCompResource = (MicroScheduler_ComputeResource*)(m_computeResources[ii]);
                GTS_ASSERT(pUpRankCompResource->type() == ComputeResourceType::CpuMicroScheduler);
                break;
            }
        }

        pCritSchedule->_upRankNodes(pCritSchedule->m_pEnd, pUpRankCompResource);
    }


    //DagUtils::printToDot("rankedNodes.gv", pCritSchedule->m_pBegin, DagUtils::NodePropertyFlags(DagUtils::NAME | DagUtils::COST));
    //DagUtils::printToDot("upRankedNodes.gv", pCritSchedule->m_pBegin, DagUtils::NodePropertyFlags(DagUtils::NAME | DagUtils::UPRANK));

    //
    // Run the schedule.

    //auto start = std::chrono::high_resolution_clock::now();

    pCritSchedule->insertReadyNode(pCritSchedule->m_pBegin);

    ComputeResource* pBlockingCompResource = nullptr;
    for (size_t ii = 0; ii < m_computeResources.size(); ++ii)
    {
        if (m_computeResources[ii]->id() != id)
        {
            m_computeResources[ii]->process(pSchedule, false);
        }
        else
        {
            pBlockingCompResource = m_computeResources[ii];
            GTS_ASSERT(pBlockingCompResource->type() == ComputeResourceType::CpuMicroScheduler);
        }
    }

    if (pBlockingCompResource)
    {
        pBlockingCompResource->process(pSchedule, true);
    }

    //while (!pCritSchedule->m_crticalPath.empty())
    //{
    //    Node* pNode;
    //    pCritSchedule->m_crticalPath.tryPop(pNode);
    //    printf("%s\n", pNode->name());
    //}
    //pCritSchedule->m_crticalPath.clear();

    //auto end = std::chrono::high_resolution_clock::now();

    //std::chrono::duration<double> diff = end - start;
    //printf("%f\n", diff.count());
}

} // namespace gts
