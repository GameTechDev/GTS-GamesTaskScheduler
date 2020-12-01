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
#include "gts/macro_scheduler/schedulers/dynamic/micro_scheduler/DynamicMicroScheduler_MacroScheduler.h"

#include "gts/platform/Thread.h"
#include "gts/macro_scheduler/Node.h"
#include "gts/macro_scheduler/compute_resources/MicroScheduler_ComputeResource.h"
#include "gts/macro_scheduler/schedulers/dynamic/micro_scheduler/DynamicMicroScheduler_Schedule.h"

namespace gts {

// LIFETIME:

//------------------------------------------------------------------------------
bool DynamicMicroScheduler_MacroScheduler::init(MacroSchedulerDesc const& desc)
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
Schedule* DynamicMicroScheduler_MacroScheduler::buildSchedule(Node* pStart, Node* pEnd)
{
    DynamicMicroScheduler_Schedule* pSchedule = unalignedNew<DynamicMicroScheduler_Schedule>(this, pStart, pEnd);
    return pSchedule;
}

//--------------------------------------------------------------------------
void DynamicMicroScheduler_MacroScheduler::freeSchedule(Schedule* pSchedule)
{
    unalignedDelete(pSchedule);
    pSchedule = nullptr;
}

//------------------------------------------------------------------------------
void DynamicMicroScheduler_MacroScheduler::executeSchedule(Schedule* pSchedule, ComputeResourceId id)
{
    DynamicMicroScheduler_Schedule* pDynSchedule = static_cast<DynamicMicroScheduler_Schedule*>(pSchedule);
    Node::resetGraph(pDynSchedule->m_pBegin);
    pDynSchedule->m_isDone.exchange(false, memory_order::acq_rel);
    ComputeResource* m_computeResource = findComputeResource(id);
    m_computeResource->process(pSchedule, true);
}

} // namespace gts
