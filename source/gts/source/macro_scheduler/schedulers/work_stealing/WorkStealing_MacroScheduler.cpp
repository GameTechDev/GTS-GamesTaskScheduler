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
#include "gts/macro_scheduler/schedulers/work_stealing/WorkStealing_MacroScheduler.h"

#include "gts/macro_scheduler/Node.h"
#include "gts/macro_scheduler/compute_resources/CpuPureWorkStealing_ComputeResource.h"
#include "gts/macro_scheduler/schedulers/work_stealing/WorkStealing_Schedule.h"
#include "gts/macro_scheduler/schedulers/work_stealing/WorkStealing_WorkQueue.h"

namespace gts {

class Node_WorkStealing;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// WorkStealing_MacroScheduler:

// STRUCTORS:

//------------------------------------------------------------------------------
WorkStealing_MacroScheduler::~WorkStealing_MacroScheduler()
{
    delete m_pQueue;
}

// MUTATORS:

//------------------------------------------------------------------------------
bool WorkStealing_MacroScheduler::init(MacroSchedulerDesc const& desc)
{
    if (desc.computeResourcesByType[(uint32_t)ComputeResourceType::CPU_WorkStealing].size() != 1)
    {
        GTS_ASSERT(0);
        return false;
    }

    m_pComputeResource = (CpuPureWorkStealing_ComputeResource*)desc.computeResourcesByType[(uint32_t)ComputeResourceType::CPU_WorkStealing][0];

    delete m_pQueue;
    m_pQueue = new WorkStealing_WorkQueue;

    return true;
}

//------------------------------------------------------------------------------
ISchedule* WorkStealing_MacroScheduler::buildSchedule(Node* pStart, Node*)
{
    m_pQueue->push(pStart);
    return new WorkStealing_Schedule(m_pQueue);
}

//------------------------------------------------------------------------------
void WorkStealing_MacroScheduler::executeSchedule(ISchedule* pSchedule)
{
    m_pComputeResource->execute(pSchedule);
}

} // namespace gts
