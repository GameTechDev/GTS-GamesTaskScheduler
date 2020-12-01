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
#include "gts/macro_scheduler/compute_resources/MicroScheduler_ComputeResource.h"

#include "gts/micro_scheduler/MicroScheduler.h"

#include "gts/macro_scheduler/Node.h"
#include "gts/macro_scheduler/Schedule.h"
#include "gts/macro_scheduler/compute_resources/MicroScheduler_Workload.h"

namespace gts {

//------------------------------------------------------------------------------
MicroScheduler_ComputeResource::MicroScheduler_ComputeResource(MicroScheduler* pMicroScheduler, uint32_t vectorWidth)
    : m_pMicroScheduler(nullptr)
    , m_pCurrentSchedule({nullptr})
    , m_pCheckForTasksDataBySchedule(nullptr)
    , m_vectorWidth(vectorWidth)
{
    init(pMicroScheduler);
}

//------------------------------------------------------------------------------
MicroScheduler_ComputeResource::~MicroScheduler_ComputeResource()
{
    alignedDelete(m_pCheckForTasksDataBySchedule);
}

//------------------------------------------------------------------------------
bool MicroScheduler_ComputeResource::init(MicroScheduler* pMicroScheduler)
{
    GTS_ASSERT(pMicroScheduler);
    if (!pMicroScheduler || m_pMicroScheduler != nullptr)
    {
        return false;
    }

    m_pMicroScheduler = pMicroScheduler;
    m_pCurrentSchedule.store(nullptr, memory_order::relaxed);

    m_pCheckForTasksDataBySchedule = alignedNew<ParallelHashTable<Schedule*, CheckForTasksData>, GTS_NO_SHARING_CACHE_LINE_SIZE>();

    return true;
}

//------------------------------------------------------------------------------
bool MicroScheduler_ComputeResource::process(Schedule* pSchedule, bool canBlock)
{
    // NOTE: Start at 1 to exclude the master thread.
    for (uint32_t ii = 1; ii < m_pMicroScheduler->workerCount(); ++ii)
    {
        notify(pSchedule);
    }

    if (canBlock)
    {
        GTS_TRACE_SCOPED_ZONE_P0(gts::analysis::CaptureMask::MACRO_SCHEDULER_PROFILE, gts::analysis::Color::Cyan, "Process Schedule");

        Backoff<> backoff;

        while (!pSchedule->isDone())
        {
            GTS_TRACE_SCOPED_ZONE_P0(gts::analysis::CaptureMask::MACRO_SCHEDULER_PROFILE, gts::analysis::Color::Cyan1, "Process Schedule Loop");
            Node* pNode = pSchedule->popNextNode(id(), type());
            if (pNode)
            {
                GTS_TRACE_SCOPED_ZONE_P0(gts::analysis::CaptureMask::MACRO_SCHEDULER_PROFILE, gts::analysis::Color::Cyan2, "Execute Node");
                if (!_buildTaskAndRun(pSchedule, pNode))
                {
                    GTS_ASSERT(0);
                    return false;
                }
                backoff.reset();
            }
            else
            {
                GTS_TRACE_SCOPED_ZONE_P0(gts::analysis::CaptureMask::MACRO_SCHEDULER_PROFILE, gts::analysis::Color::Orange, "No Nodes");
                backoff.tick();
            }
        }
    }

    return true;
}

//------------------------------------------------------------------------------
bool MicroScheduler_ComputeResource::canExecute(Node* pNode) const
{
    return _findWorkload(pNode) != nullptr;
}

//------------------------------------------------------------------------------
void MicroScheduler_ComputeResource::notify(Schedule*)
{
    GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::MACRO_SCHEDULER_DEBUG, analysis::Color::Green1, "NOTIFY", this, m_pMicroScheduler->id());
    m_pMicroScheduler->wakeWorker();
}

//------------------------------------------------------------------------------
Task* MicroScheduler_ComputeResource::onCheckForTask(void* pUserData, MicroScheduler*, OwnedId, bool& executedTask)
{
    CheckForTasksData* pData = (CheckForTasksData*)pUserData;

    while (true)
    {
        GTS_TRACE_SCOPED_ZONE_P0(gts::analysis::CaptureMask::MACRO_SCHEDULER_PROFILE, gts::analysis::Color::Cyan1, "Process Schedule Loop");
        Node* pNode = pData->pSchedule->popNextNode(pData->pSelf->id(), pData->pSelf->type());
        if (pNode)
        {
            GTS_TRACE_SCOPED_ZONE_P0(gts::analysis::CaptureMask::MACRO_SCHEDULER_PROFILE, gts::analysis::Color::Cyan2, "Execute Node");
            Task* pTask = pData->pSelf->_buildTask(pData->pSchedule, pNode);
            if (!pTask)
            {
                GTS_ASSERT(0);
                break;
            }
            executedTask = true;
            return pTask;
        }
        else
        {
            break;
        }
    }

    return nullptr;
}

//------------------------------------------------------------------------------
void MicroScheduler_ComputeResource::registerSchedule(Schedule* pSchedule)
{
    auto iter = m_pCheckForTasksDataBySchedule->insert({ pSchedule, {this, pSchedule} });

    m_pMicroScheduler->registerCallback(
        MicroSchedulerCallbackType::CHECK_FOR_TASK,
        onCheckForTask,
        &iter->value);
}

//------------------------------------------------------------------------------
void MicroScheduler_ComputeResource::unregisterSchedule(Schedule* pSchedule)
{
    auto iter = m_pCheckForTasksDataBySchedule->find(pSchedule);

    m_pMicroScheduler->unregisterCallback(
        MicroSchedulerCallbackType::CHECK_FOR_TASK,
        onCheckForTask,
        &iter->value);

    m_pCheckForTasksDataBySchedule->erase(iter);
}

//------------------------------------------------------------------------------
void MicroScheduler_ComputeResource::spawnReadyChildren(WorkloadContext const& workloadContext, Task*)
{
    GTS_TRACE_SCOPED_ZONE_P0(gts::analysis::CaptureMask::MACRO_SCHEDULER_PROFILE, gts::analysis::Color::YellowGreen, "Spawn Ready Successors");

    workloadContext.pNode->_waitUntilComplete();

    Node* pMyNode = workloadContext.pNode;

    Vector<Node*> const& children = pMyNode->successors();

    if(children.size() == 0)
    {
        workloadContext.pSchedule->tryMarkDone(pMyNode);
    }

    for (size_t ii = 0; ii < children.size(); ++ii)
    {
        Node* pChildNode = children[ii];

        // If the Node is ready, add it to the list.
        if (pChildNode->_removePredecessorRefAndReturnReady())
        {
            workloadContext.pSchedule->insertReadyNode(pChildNode);
        }
    }

    for (size_t ii = 0; ii < children.size(); ++ii)
    {
        Node* pChildNode = children[ii];
        pChildNode->_markPredecessorComplete();
    }
}

//------------------------------------------------------------------------------
bool MicroScheduler_ComputeResource::_buildTaskAndRun(Schedule* pSchedule, Node* pNode)
{
    // Find a CPP or ISPC workload.
    MicroScheduler_Workload* pWorkload = _findWorkload(pNode);
    GTS_ASSERT(pWorkload != nullptr && "Invalid WorkloadType");
    if (!pWorkload)
    {
        return false;
    }

    // Build the root task.
    Task* pTask = m_pMicroScheduler->allocateTask<MicroScheduler_Task>(
        pWorkload,
        WorkloadContext{ pNode, pSchedule, this });

    pTask->setAffinity(pWorkload->workerAffinityId());

    // Run it.
    m_pMicroScheduler->spawnTaskAndWait(pTask);

    return true;
}

//------------------------------------------------------------------------------
Task* MicroScheduler_ComputeResource::_buildTask(Schedule* pSchedule, Node* pNode)
{
    // Find a CPP or ISPC workload.
    MicroScheduler_Workload* pWorkload = _findWorkload(pNode);
    GTS_ASSERT(pWorkload != nullptr && "Invalid WorkloadType");
    if (!pWorkload)
    {
        return nullptr;
    }

    // Build the root task.
    Task* pTask = m_pMicroScheduler->allocateTask<MicroScheduler_Task>(
        pWorkload,
        WorkloadContext{ pNode, pSchedule, this });

    pTask->setAffinity(pWorkload->workerAffinityId());

    return pTask;
}

//------------------------------------------------------------------------------
MicroScheduler_Workload* MicroScheduler_ComputeResource::_findWorkload(Node* pNode) const
{
    MicroScheduler_Workload* pWorkload = (MicroScheduler_Workload*)pNode->findWorkload(WorkloadType::ISPC);
    if (!pWorkload)
    {
        pWorkload = (MicroScheduler_Workload*)pNode->findWorkload(WorkloadType::CPP);
    }
    return pWorkload;
}

} // namespace gts
