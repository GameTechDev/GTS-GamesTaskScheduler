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
#pragma once

#include "gts/containers/parallel/ParallelHashTable.h"

#include "gts/micro_scheduler/MicroSchedulerTypes.h"
#include "gts/macro_scheduler/ComputeResource.h"

#include "gts/containers/parallel/QueueMPSC.h"

namespace gts {

class MicroScheduler;
class MicroScheduler_Workload;

/**
 * @addtogroup MacroScheduler
 * @{
 */

/** 
 * @addtogroup ComputeResources
 * @{
 */

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A ComputeResource that wraps a MicroScheduler.
 */
class MicroScheduler_ComputeResource : public ComputeResource
{
public:

    MicroScheduler_ComputeResource() = default;

    MicroScheduler_ComputeResource(MicroScheduler* pMicroScheduler, uint32_t vectorWidth, uint32_t physicalProcessorCount);

    virtual ~MicroScheduler_ComputeResource();

    bool init(MicroScheduler* pMicroScheduler);

    GTS_INLINE MicroScheduler* microScheduler() { return m_pMicroScheduler; }

    GTS_INLINE Schedule* currentSchedule() { return m_pCurrentSchedule.load(memory_order::acquire); }

    GTS_INLINE virtual ComputeResourceType::Enum type() const final { return ComputeResourceType::CpuMicroScheduler; }

    GTS_INLINE uint32_t vectorWidth() { return m_vectorWidth; }

    virtual bool process(Schedule* pSchedule, bool canBlock);

    virtual bool canExecute(Node* pNode) const final;

    virtual uint32_t processorCount() const final;

    virtual void notify(Schedule* pSchedule) final;

    virtual void registerSchedule(Schedule* pSchedulue) final;

    virtual void unregisterSchedule(Schedule* pSchedulue) final;

    virtual void spawnReadyChildren(WorkloadContext const& workloadContext, Task* pCurrentTask);

protected:

    struct CheckForTasksData
    {
        MicroScheduler_ComputeResource* pSelf = nullptr;
        Schedule* pSchedule = nullptr;
    };

    static Task* onCheckForTask(void* pUserData, MicroScheduler*, OwnedId, bool isCallerExternal, bool& executedTask);

    bool _tryRunNextNode(Schedule* pSchedule, bool myQueuesOnly);

    Task* _tryGetNextTask(CheckForTasksData* pData, bool myQueuesOnly, bool& executedTask);

    bool _buildTaskAndRun(Schedule* pSchedule, Node* pNode);

    Task* _buildTask(Schedule* pSchedule, Node* pNode);

    bool _tryToStealWork();

    MicroScheduler* m_pMicroScheduler = nullptr;

private:

    friend class MicroScheduler_Task;

    MicroScheduler_Workload* _findWorkload(Node* pNode) const;

    Atomic<Schedule*> m_pCurrentSchedule{ nullptr };
    ParallelHashTable<Schedule*, CheckForTasksData>* m_pCheckForTasksDataBySchedule;
    uint32_t m_vectorWidth;
    uint32_t m_physicalProcessorCount;
    uint32_t m_maxRank;
};

/** @} */ // end of ComputeResources
/** @} */ // end of MacroScheduler

} // namespace gts
