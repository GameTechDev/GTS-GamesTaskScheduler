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
#include "gts/macro_scheduler/compute_resources/CpuWorkStealing_Workload.h"

#include "gts/containers/Vector.h"
#include "gts/micro_scheduler/MicroScheduler.h"
#include "gts/micro_scheduler/Task.h"
#include "gts/macro_scheduler/Node.h"
#include "gts/macro_scheduler/ComputeResourceType.h"

namespace gts {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// WorkStealingTask

//------------------------------------------------------------------------------
WorkStealingTask::WorkStealingTask(
    CpuWorkStealingComputeRoutine cpuComputeRoutine,
    void* pUserData)
    : m_cpuComputeRoutine(cpuComputeRoutine)
    , m_pUserData(pUserData)
{}

//------------------------------------------------------------------------------
Task* WorkStealingTask::execute(Task* pThisTask, TaskContext const&)
{
    GTS_ASSERT(pThisTask != nullptr);

    // Execute this node's work.
    WorkStealingTask* pNodeTask = (WorkStealingTask*)pThisTask->getData();
    pNodeTask->m_cpuComputeRoutine(pNodeTask->m_pUserData);

    return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// CpuWorkStealing_Workload

//------------------------------------------------------------------------------
CpuWorkStealing_Workload::CpuWorkStealing_Workload(
    CpuWorkStealingComputeRoutine cpuComputeRoutine,
    void* pUserData)
    : Workload(ComputeResourceType::CPU_WorkStealing)
    , m_workStealingTask(cpuComputeRoutine, pUserData)
{}

//------------------------------------------------------------------------------
WorkStealingTask* CpuWorkStealing_Workload::getWork()
{
    return &m_workStealingTask;
}

//------------------------------------------------------------------------------
Task* CpuWorkStealing_Workload::buildTask(Node* pNode, MicroScheduler* pMicroScheduler)
{
    // Get the CPU_WorkStealing specific Workload.
    CpuWorkStealing_Workload* pWorkload =
        reinterpret_cast<CpuWorkStealing_Workload*>(pNode->findWorkload(ComputeResourceType::CPU_WorkStealing));

    // Build the Task.
    Task* pTask = pMicroScheduler->allocateTask(WorkStealingTask::execute);
    pTask->setData(pWorkload->getWork());

    return pTask;
}

} // namespace gts
