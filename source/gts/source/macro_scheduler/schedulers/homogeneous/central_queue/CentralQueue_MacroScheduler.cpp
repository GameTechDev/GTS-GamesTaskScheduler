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
#include "gts/macro_scheduler/schedulers/homogeneous/central_queue/CentralQueue_MacroScheduler.h"

#include "gts/platform/Thread.h"
#include "gts/macro_scheduler/Node.h"
#include "gts/macro_scheduler/compute_resources/MicroScheduler_ComputeResource.h"
#include "gts/macro_scheduler/schedulers/homogeneous/central_queue/CentralQueue_Schedule.h"

namespace gts {

// LIFETIME:

//------------------------------------------------------------------------------
bool CentralQueue_MacroScheduler::init(MacroSchedulerDesc const& desc)
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

    return true;
}

// MUTATORS:

//------------------------------------------------------------------------------
Schedule* CentralQueue_MacroScheduler::buildSchedule(Node* pStart, Node* pEnd)
{
    CentralQueue_Schedule* pSchedule = unalignedNew<CentralQueue_Schedule>(this, pStart, pEnd);
    for (size_t ii = 0; ii < m_computeResources.size(); ++ii)
    {
        m_computeResources[ii]->registerSchedule(pSchedule);
    }
    return pSchedule;
}

//--------------------------------------------------------------------------
void CentralQueue_MacroScheduler::freeSchedule(Schedule* pSchedule)
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
void CentralQueue_MacroScheduler::executeSchedule(Schedule* pSchedule, ComputeResourceId id)
{
    GTS_TRACE_SCOPED_ZONE_P0(gts::analysis::CaptureMask::MACRO_SCHEDULER_PROFILE, gts::analysis::Color::Cyan, "Execute Schedule");

    GTS_ASSERT(pSchedule->isDone() && "Trying to execute a running schedule!");

    //
    // Reset the schedule.

    CentralQueue_Schedule* pCentralQueueSchedule = (CentralQueue_Schedule*)pSchedule;
    pCentralQueueSchedule->m_isDone.exchange(false, memory_order::acq_rel);

    Node::resetGraph(pCentralQueueSchedule->m_pSource);

    //
    // Run the schedule.

    pCentralQueueSchedule->insertReadyNode(pCentralQueueSchedule->m_pSource);

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
}

} // namespace gts
