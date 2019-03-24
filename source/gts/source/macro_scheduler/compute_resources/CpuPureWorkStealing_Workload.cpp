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
#include "gts/macro_scheduler/compute_resources/CpuPureWorkStealing_Workload.h"

#include "gts/containers/Vector.h"
#include "gts/micro_scheduler/MicroScheduler.h"
#include "gts/micro_scheduler/Task.h"
#include "gts/macro_scheduler/Node.h"
#include "gts/macro_scheduler/ComputeResourceType.h"

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
namespace gts {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// PureWorkStealingTask

//------------------------------------------------------------------------------
PureWorkStealingTask::PureWorkStealingTask(
    CpuPureWorkStealingComputeRoutine cpuComputeRoutine,
    void* pUserData)
    : m_cpuComputeRoutine(cpuComputeRoutine)
    , m_pUserData(pUserData)
{}

//------------------------------------------------------------------------------
void PureWorkStealingTask::setOwnerNode(Node* pNode)
{
    m_pMyNode = pNode;
}

//------------------------------------------------------------------------------
Task* PureWorkStealingTask::execute(Task* pThisTask, TaskContext const& ctx)
{
    GTS_ASSERT(pThisTask != nullptr);

    // Execute this node's work.
    PureWorkStealingTask* pNodeTask = (PureWorkStealingTask*)pThisTask->getData();
    pNodeTask->m_cpuComputeRoutine(pNodeTask->m_pUserData);

    GTS_ASSERT(pNodeTask->m_pMyNode != nullptr);

    //
    // Prepass: get all ready children.

    Vector<Node*> const& children = pNodeTask->m_pMyNode->children();

    Node** ppReadyChildNodes = (Node**)alloca(sizeof(Node*) * children.size());
    uint32_t readyChildCount = 0;

    for (size_t ii = 0; ii < children.size(); ++ii)
    {
        Node* pChildNode = children[ii];

        // If the Node is ready, add it to the list.
        if (pChildNode->finishPredecessor())
        {
            ppReadyChildNodes[ii] = pChildNode;
            ++readyChildCount;
        }
        else
        {
            ppReadyChildNodes[ii] = nullptr;
        }
    }

    //
    // Execute the ready child tasks in parallel and wait for them to finish.

    pThisTask->addRef(readyChildCount);

    for (size_t ii = 0; ii < children.size(); ++ii)
    {
        Node* pChild = ppReadyChildNodes[ii];
        if (pChild != nullptr)
        {
            Task* pTask = CpuPureWorkStealing_Workload::buildTask(pChild, ctx.pMicroScheduler);

            // Add and queue the task.
            pThisTask->addChildTaskWithoutRef(pTask);
            ctx.pMicroScheduler->spawnTask(pTask);
        }
    }

    // Wait for all the children to finish.
    pThisTask->waitForChildren(ctx);

    return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// CpuPureWorkStealing_Workload

//------------------------------------------------------------------------------
CpuPureWorkStealing_Workload::CpuPureWorkStealing_Workload(
    CpuPureWorkStealingComputeRoutine cpuComputeRoutine,
    void* pUserData)
    : Workload(ComputeResourceType::CPU_WorkStealing)
    , m_workStealingTask(cpuComputeRoutine, pUserData)
{}

//------------------------------------------------------------------------------
PureWorkStealingTask* CpuPureWorkStealing_Workload::getWork()
{
    return &m_workStealingTask;
}

//------------------------------------------------------------------------------
Task* CpuPureWorkStealing_Workload::buildTask(Node* pNode, MicroScheduler* pMicroScheduler)
{
    // Get the CPU_WorkStealing specific Workload.
    CpuPureWorkStealing_Workload* pWorkload =
        reinterpret_cast<CpuPureWorkStealing_Workload*>(pNode->findWorkload(ComputeResourceType::CPU_WorkStealing));

    // Give the task access to the Node's data.
    pWorkload->getWork()->setOwnerNode(pNode);

    // Build the Task.
    Task* pTask = pMicroScheduler->allocateTask(PureWorkStealingTask::execute);
    pTask->setData(pWorkload->getWork());

    return pTask;
}

} // namespace gts
