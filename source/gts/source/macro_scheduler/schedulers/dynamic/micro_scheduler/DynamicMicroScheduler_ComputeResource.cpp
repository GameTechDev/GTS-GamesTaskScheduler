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
#include "gts/macro_scheduler/schedulers/dynamic/micro_scheduler/DynamicMicroScheduler_ComputeResource.h"

#include "gts/containers/Vector.h"
 
#include "gts/micro_scheduler/MicroScheduler.h"
#include "gts/micro_scheduler/Task.h"
 
#include "gts/macro_scheduler/Node.h"
#include "gts/macro_scheduler/Schedule.h"
#include "gts/macro_scheduler/schedulers/dynamic/micro_scheduler/DynamicMicroScheduler_MacroScheduler.h"
#include "gts/macro_scheduler/compute_resources/MicroScheduler_Workload.h"

namespace gts {

//------------------------------------------------------------------------------
void DynamicMicroScheduler_ComputeResource::receiveAffinitizedNode(Schedule* pSchedule, Node* pNode)
{
    _buildTaskAndRun(pSchedule, pNode);
}

//------------------------------------------------------------------------------
bool DynamicMicroScheduler_ComputeResource::process(Schedule* pSchedule, bool canBlock)
{
    if (canBlock)
    {
        Backoff<> backoff;
        while (!pSchedule->isDone())
        {
            Node* pNode = pSchedule->popNextNode(id(), type());
            if (pNode)
            {
                if (!_buildTaskAndRun(pSchedule, pNode))
                {
                    GTS_ASSERT(0);
                    return false;
                }
                backoff.reset();
            }
            else
            {
                backoff.tick();
            }
        }
    }
    return true;
}

//------------------------------------------------------------------------------
void DynamicMicroScheduler_ComputeResource::spawnReadyChildren(
    WorkloadContext const& workloadContext,
    Task* pCurrentTask)
{
    GTS_ASSERT(pCurrentTask != nullptr);
    Node* pMyNode = workloadContext.pNode;

    Vector<Node*> const& children = pMyNode->successors();

    // Assume all children are ready. We will adjust this later. (+1 for the wait.)
    pCurrentTask->addRef((int32_t)children.size() + 1, gts::memory_order::relaxed);

    // Count the actual number of spawned children.
    int32_t spawnedChildCount = 0;

    for (size_t ii = 0; ii < children.size(); ++ii)
    {
        Node* pChildNode = children[ii];
        pChildNode->_setCurrentSchedule(pMyNode->currentSchedule());

        // If the Node is ready, add it to the list.
        if (pChildNode->_removePredecessorRefAndReturnReady())
        {
            // Check if we have an affinitized Node.
            if (pChildNode->affinity() != ANY_COMP_RESOURCE && pChildNode->affinity() != workloadContext.pComputeResource->id())
            {
                //
                // Send the affinitized ComputeResource.

                DynamicMicroScheduler_MacroScheduler* pMacroScheduler =
                    (DynamicMicroScheduler_MacroScheduler*)(pMyNode->currentSchedule()->getScheduler());

                DynamicMicroScheduler_ComputeResource* pComputResource =
                    (DynamicMicroScheduler_ComputeResource*)pMacroScheduler->findComputeResource(pChildNode->affinity());

                pComputResource->receiveAffinitizedNode(workloadContext.pSchedule, pChildNode);

                continue;
            }

            ++spawnedChildCount;

            // Find a CPP or ISPC workload.
            MicroScheduler_Workload* pWorkload = (MicroScheduler_Workload*)pChildNode->findWorkload(WorkloadType::CPP);
            if (!pWorkload)
            {
                pWorkload = (MicroScheduler_Workload*)pChildNode->findWorkload(WorkloadType::ISPC);
            }
            GTS_ASSERT(pWorkload != nullptr && "Invalid WorkloadType");

            MicroScheduler_ComputeResource* pComputResource = (MicroScheduler_ComputeResource*)workloadContext.pComputeResource;

            // Build a MicroScheduler Task that wraps the workload.
            Task* pTask = pComputResource->microScheduler()->allocateTask<MicroScheduler_Task>(
                pWorkload,
                WorkloadContext{ pChildNode, workloadContext.pSchedule, pComputResource });

            pTask->setAffinity(pWorkload->workerAffinityId());

            // Add it to the DAG and spawn the task.
            pCurrentTask->addChildTaskWithoutRef(pTask);
            pComputResource->microScheduler()->spawnTask(pTask);

            // Were done with this node, reset it for next time.
            pChildNode->reset();
        }
    }

    // Adjust our assumed ready child count to the actual count.
    pCurrentTask->removeRef((int32_t)children.size() - spawnedChildCount, gts::memory_order::seq_cst);

    // Wait for all the children to finish. // TODO: explore continuations
    pCurrentTask->waitForAll();

    workloadContext.pSchedule->tryMarkDone(pMyNode);
}

} // namespace gts
