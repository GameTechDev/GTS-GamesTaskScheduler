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
#include "gts/macro_scheduler/schedulers/heterogeneous/critical_node_task_scheduling/CriticalNode_MacroScheduler.h"

#include <chrono>

#include "gts/macro_scheduler/DagUtils.h"

#include "gts/micro_scheduler/patterns/ParallelFor.h"

#include "gts/macro_scheduler/Node.h"
#include "gts/macro_scheduler/ComputeResource.h"
#include "gts/macro_scheduler/compute_resources/MicroScheduler_ComputeResource.h"
#include "gts/macro_scheduler/schedulers/heterogeneous/critical_node_task_scheduling/CriticalNode_Schedule.h"

namespace gts {

// LIFETIME:

//------------------------------------------------------------------------------
bool CriticalNode_MacroScheduler::init(MacroSchedulerDesc const& desc)
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

    // Sort ComputeResource by normalization factor (i.e. processing speed)
    insertionSort(m_computeResources.data(), m_computeResources.size(),
        [](ComputeResource* lhs, ComputeResource* rhs)->bool {
            return lhs->executionNormalizationFactor() < rhs->executionNormalizationFactor();
        });


    // Rank the ComputeResources.
    uint32_t nextRank = 0;
    for (size_t ii = 0; ii < m_computeResources.size(); ++ii)
    {
        m_computeResources[ii]->_setMaxRank(nextRank);
        nextRank += m_computeResources[ii]->processorCount();
    }

    return true;
}

// MUTATORS:

//------------------------------------------------------------------------------
Schedule* CriticalNode_MacroScheduler::buildSchedule(Node* pStart, Node* pEnd)
{
    CriticalNode_Schedule* pSchedule = unalignedNew<CriticalNode_Schedule>(this, pStart, pEnd);
    for (size_t ii = 0; ii < m_computeResources.size(); ++ii)
    {
        m_computeResources[ii]->registerSchedule(pSchedule);
    }
    return pSchedule;
}

//--------------------------------------------------------------------------
void CriticalNode_MacroScheduler::freeSchedule(Schedule* pSchedule)
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
void CriticalNode_MacroScheduler::executeSchedule(Schedule* pSchedule, ComputeResourceId id)
{
    GTS_TRACE_SCOPED_ZONE_P0(gts::analysis::CaptureMask::MACRO_SCHEDULER_PROFILE, gts::analysis::Color::Cyan, "Execute Schedule");

    GTS_ASSERT(pSchedule->isDone() && "Trying to execute a running schedule!");

    //
    // Reset the schedule.

    CriticalNode_Schedule* pCritSchedule = (CriticalNode_Schedule*)pSchedule;
    pCritSchedule->m_isDone.exchange(false, memory_order::acq_rel);

    Node::resetGraph(pCritSchedule->m_pSource);

    //DagUtils::printToDot("costedNodes.gv", pCritSchedule->m_pSource, DagUtils::NodePropertyFlags(DagUtils::NAME | DagUtils::COST));

    //{
    //    GTS_TRACE_SCOPED_ZONE_P0(gts::analysis::CaptureMask::MACRO_SCHEDULER_PROFILE, gts::analysis::Color::Cyan, "Preprocess Nodes");
    //    pCritSchedule->_preProcessNodes(pCritSchedule->m_pSink);
    //}

    {
        GTS_TRACE_SCOPED_ZONE_P0(gts::analysis::CaptureMask::MACRO_SCHEDULER_PROFILE, gts::analysis::Color::Cyan, "Rank Nodes");
        pCritSchedule->_rankNodes(m_computeResources, pCritSchedule->m_pSource, pCritSchedule->m_pSink);
    }

    //DagUtils::printToDot("downRankedNodes.gv", pCritSchedule->m_pSource, DagUtils::NodePropertyFlags(DagUtils::NAME | DagUtils::DOWNRANK));

    //
    // Run the schedule.

    //auto start = std::chrono::high_resolution_clock::now();

    pCritSchedule->insertReadyNode(pCritSchedule->m_pSource);

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
    //printf("Execution Time: %f\n", diff.count());
}

} // namespace gts
