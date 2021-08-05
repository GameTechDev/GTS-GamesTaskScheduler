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
#include "gts/macro_scheduler/compute_resources/MicroScheduler_Workload.h"

#include "gts/macro_scheduler/compute_resources/MicroScheduler_ComputeResource.h"
#include "gts/macro_scheduler/Schedule.h"

namespace gts {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// MicroScheduler_Task:

//------------------------------------------------------------------------------
MicroScheduler_Task::MicroScheduler_Task(
    MicroScheduler_Workload* pThisWorkload,
    WorkloadContext workloadContext)
    : m_pThisWorkload(pThisWorkload)
    , m_workloadContext(workloadContext)
{}

//------------------------------------------------------------------------------
Task* MicroScheduler_Task::execute(TaskContext const& ctx)
{
    m_pThisWorkload->m_executionContext = ctx;

    ComputeResource* pThisCompResource = findThisComputeResource(ctx.workerId.ownerId());
    GTS_ASSERT(pThisCompResource);
    m_workloadContext.pComputeResource = pThisCompResource;

    uint64_t start = GTS_RDTSC();

    m_pThisWorkload->execute(m_workloadContext);

    uint64_t end = GTS_RDTSC();

    uint64_t cost;
    if (end > start)
    {
        cost = uint64_t(double(end - start) / m_workloadContext.pComputeResource->executionNormalizationFactor());
    }
    else // Something messed up the cost, like a context switch.
    {
        // Try the previous cost.
        cost = m_pThisWorkload->myNode()->executionCost();

        // Or use the Normalization Factor if the previous cost is zero.
        if (cost == 0)
        {
            cost = uint64_t(m_workloadContext.pComputeResource->executionNormalizationFactor());
        }
    }

#if 1
    m_pThisWorkload->myNode()->_setExecutionCost(cost);
    m_workloadContext.pSchedule->observeExecutionCost(m_workloadContext.pComputeResource->id(), cost);
#endif

    static_cast<MicroScheduler_ComputeResource*>(m_workloadContext.pComputeResource)->spawnReadyChildren(m_workloadContext, this);

    return nullptr;
}

//------------------------------------------------------------------------------
ComputeResource* MicroScheduler_Task::findThisComputeResource(SubIdType microSchedulerId)
{
    auto& compResources = m_workloadContext.pSchedule->getScheduler()->computeResources();
    for (uint32_t ii = 0; ii < compResources.size(); ++ii)
    {
        if (compResources[ii]->type() == ComputeResourceType::CpuMicroScheduler
            && ((MicroScheduler_ComputeResource*)compResources[ii])->microScheduler()->id() == microSchedulerId)
        {
            return compResources[ii];
        }
    }
    return nullptr;
}

} // namespace gts
