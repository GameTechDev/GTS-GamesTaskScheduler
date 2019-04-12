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

namespace gts {

//------------------------------------------------------------------------------
MicroScheduler_Task::MicroScheduler_Task(
    Workload_ComputeRoutine cpuComputeRoutine,
    MicroScheduler_Workload* pMyWorkload,
    uint64_t* pExecutionCost)
    : m_cpuComputeRoutine(cpuComputeRoutine)
    , m_pMyWorkload(pMyWorkload)
    , m_pExecutionCost(pExecutionCost)
{}

//------------------------------------------------------------------------------
Task* MicroScheduler_Task::taskFunc(Task* pThisTask, TaskContext const& ctx)
{
    GTS_ASSERT(pThisTask != nullptr);
        
    // Unpack the data.
    MicroScheduler_Task* pNodeTask = (MicroScheduler_Task*)pThisTask->getData();
        
    pNodeTask->_executeTask();
    pNodeTask->m_pMyWorkload->_spawnReadyChildren(pNodeTask->m_pMyWorkload->node(), pThisTask, ctx);
        
    return nullptr;
}

//------------------------------------------------------------------------------
void MicroScheduler_Task::_executeTask()
{
    // Execute this node's work.
    uint64_t exeCost = GTS_RDTSC();
    m_cpuComputeRoutine(m_pMyWorkload, WorkloadContext{});
    *m_pExecutionCost = GTS_RDTSC() - exeCost;
}

//------------------------------------------------------------------------------
void MicroScheduler_Workload::_spawnReadyChildren(Node* pThisNode, Task* pThisTask, TaskContext const& ctx)
{
    GTS_ASSERT(pThisNode != nullptr);
        
    Vector<Node*> const& children = pThisNode->children();
        
    // Assume all children are ready. We will adjust this later.
    pThisTask->addRef((int32_t)children.size(), gts::memory_order::relaxed);
        
    // Count the actual number of spawned children.
    int32_t spawnedChildCount = 0;

    // TODO: parallel-for?
    for (size_t ii = 0; ii < children.size(); ++ii)
    {
        Node* pChildNode = children[ii];
        
        // If the Node is ready, add it to the list.
        if (pChildNode->finishPredecessor())
        {
            ++spawnedChildCount;
        
            MicroScheduler_Workload* pWorkload = (MicroScheduler_Workload*)pChildNode->findWorkload(ComputeResourceType::CPU_MicroScheduler);
            GTS_ASSERT(pWorkload != nullptr);
            Task* pTask = pWorkload->_buildTask(ctx.pMicroScheduler);
        
            // Add and queue the task.
            pThisTask->addChildTaskWithoutRef(pTask);
            ctx.pMicroScheduler->spawnTask(pTask);
        }
    }
        
    // Adjust our assumed ready child count to the actual count.
    pThisTask->removeRef((int32_t)children.size() - spawnedChildCount, gts::memory_order::seq_cst);
        
    // Wait for all the children to finish.
    pThisTask->waitForChildren(ctx);
}

} // namespace gts
