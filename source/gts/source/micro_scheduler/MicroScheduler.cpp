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
#include "gts/micro_scheduler/MicroScheduler.h"

#ifdef GTS_USE_ITT
#include <ittnotify.h>
#endif

#include "gts/analysis/Instrumenter.h"
#include "gts/analysis/Analyzer.h"

#include "gts/micro_scheduler/WorkerPool.h"

#include "Worker.h"
#include "Schedule.h"
#include "Containers.h"
#include "Backoff.h"

namespace gts {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// MicroScheduler:

//------------------------------------------------------------------------------
Task* MicroScheduler::_allocateEmptyTask(TaskRoutine taskRoutine, uint32_t size)
{
    uint32_t workerIndex = m_pWorkerPool->thisWorkerIndex();

    GTS_ANALYSIS_COUNT(workerIndex, gts::analysis::AnalysisType::NUM_ALLOCATIONS);
    GTS_ANALYSIS_TIME_SCOPED(workerIndex, gts::analysis::AnalysisType::NUM_ALLOCATIONS);

    Task* pTask = nullptr;
    pTask = new (m_pTaskAllocator->allocate(workerIndex, size)) Task(taskRoutine);
    GTS_ASSERT(pTask != nullptr);

    GTS_INSTRUMENTER_SCOPED(analysis::Tag::INTERNAL, "ALLOCATE TASK", pTask, 0);

    pTask->m_pMyScheduler = this;

    // All new tasks belong this schedule's isolation group.
    if(workerIndex != UNKNOWN_WORKER_IDX)
    {
        pTask->m_isolationTag = m_pSchedulesByIdx[workerIndex].m_isolationTag;
    }

    return pTask;
}

//------------------------------------------------------------------------------
void MicroScheduler::spawnTask(Task* pTask, uint32_t priority)
{
    GTS_ASSERT(pTask != nullptr);
    GTS_ASSERT(!(pTask->m_state & Task::TASK_IS_CONTINUATION) &&
        "Cannot queue a continuation.");

    pTask->m_state |= Task::TASK_IS_QUEUED;

    uint32_t myWorkerIndex = m_pWorkerPool->thisWorkerIndex();
    uint32_t workerAffinityIdx = pTask->m_affinity;

    bool result = false;
    GTS_UNREFERENCED_PARAM(result);

    // If this worker is known.
    if (myWorkerIndex != UNKNOWN_WORKER_IDX)
    {
        // If this Worker is on this MicroScheduler.
        if (m_pScheduleOwnershipByIdx[myWorkerIndex])
        {
            Schedule& schedule = m_pSchedulesByIdx[myWorkerIndex];

            // If there is no specified affinity,
            if (workerAffinityIdx == UNKNOWN_WORKER_IDX)
            {
                GTS_INSTRUMENTER_SCOPED(analysis::Tag::INTERNAL, "SPAWN TASK", pTask, workerAffinityIdx);

                // we are good to queue into this Worker's deque.
                result = schedule.spawnTask(pTask, priority);
                GTS_ASSERT(result && "Task queue overflow");

                // Wake any worker.
                m_pWorkerPool->_wakeWorkers(1); // TODO: maybe m_pWorkerPool->workerCount() is better in real workload.
            }
            // Otherwise, ship it to the requested worker.
            else
            {
                GTS_INSTRUMENTER_SCOPED(analysis::Tag::INTERNAL, "SPAWN AFFINITY TASK", pTask, workerAffinityIdx);

                GTS_ASSERT(workerAffinityIdx < m_scheduleCount && "Worker index out of range.");

                result = m_pSchedulesByIdx[workerAffinityIdx].queueAffinityTask(pTask);
                GTS_ASSERT(result && "Affinity queue overflow");

                // Wake the worker.
                m_pSchedulesByIdx[workerAffinityIdx].m_pMyScheduler->m_pWorkerPool->m_pWorkersByIdx[workerAffinityIdx].wake(1);
            }
        }
        // Otherwise, ship it a Worker on this MicroScheduler. 
        else
        {
            GTS_INSTRUMENTER_SCOPED(analysis::Tag::INTERNAL, "SPAWN OFF-SCHEDULER TASK", pTask, workerAffinityIdx);

            // Find the first valid Schedule and give it the Task.
            for (size_t ii = 0; ii < m_scheduleCount; ++ii)
            {
                if (m_pScheduleOwnershipByIdx[ii])
                {
                    result = m_pSchedulesByIdx[ii].queueAffinityTask(pTask);
                    GTS_ASSERT(result && "Affinity queue too small!");

                    m_pSchedulesByIdx[ii].m_pMyScheduler->m_pWorkerPool->m_pWorkersByIdx[ii].wake(1);

                    break;
                }
            }
        }
    }
    // Otherwise, this thread is unknown so there is no deque for it. All we can
    // do is add it to the shared queue.
    else
    {
        GTS_INSTRUMENTER_SCOPED(analysis::Tag::INTERNAL, "QUEUE NON-WORKER TASK", pTask, 0);

        result = m_pTaskQueue->tryPush(pTask);
        GTS_ASSERT(result && "Task queue overflow");

        // Wake any worker.
        m_pWorkerPool->_wakeWorkers(1);
    }
}

//------------------------------------------------------------------------------
void MicroScheduler::spawnTaskAndWait(Task* pTask, uint32_t priority)
{
    GTS_ASSERT(pTask != nullptr);

    Task waiter;
    waiter.m_refCount.store(2, gts::memory_order::relaxed);
    waiter.m_state |= Task::TASK_IS_WAITING;

    waiter.m_pParent = pTask->m_pParent;
    pTask->m_pParent = &waiter;

    spawnTask(pTask, priority);
    _wait(&waiter, nullptr, m_pWorkerPool->thisWorkerIndex());
}

//------------------------------------------------------------------------------
void MicroScheduler::queueTask(Task* pTask)
{
    GTS_INSTRUMENTER_SCOPED(analysis::Tag::INTERNAL, "QUEUE TASK", pTask, 0);

    bool result = m_pTaskQueue->tryPush(pTask);
    GTS_ASSERT(result && "Task queue overflow");
    GTS_UNREFERENCED_PARAM(result);

    // Wake any worker.
    m_pWorkerPool->_wakeWorkers(1);
}

//------------------------------------------------------------------------------
void MicroScheduler::_wait(Task* pWaiterTask, Task* pStartTask, uint32_t workerIndex)
{
    GTS_ANALYSIS_COUNT(workerIndex, gts::analysis::AnalysisType::NUM_WAITS);
    GTS_INSTRUMENTER_SCOPED(analysis::Tag::INTERNAL, "WAIT", pWaiterTask, 0);

    if (workerIndex != UNKNOWN_WORKER_IDX)
    {
        // Working-block until done.
        Backoff backoff;
        while (pWaiterTask->refCount() > 1)
        {
            m_pSchedulesByIdx[workerIndex].wait(pWaiterTask, pStartTask, backoff);
            if(pWaiterTask->refCount() > 1)
            {
                gts::ThisThread::yield();
            }
        }
    }
    else
    {
        GTS_ASSERT(pStartTask == nullptr);

        // Block until done.
        uint32_t spinCount = 1;
        while (pWaiterTask->refCount() > 1)
        {
            gts::SpinWait::backoffUntilSpinCountThenYield(spinCount, 16);
            m_pWorkerPool->_wakeWorkers(1);
        }
    }
}

//--------------------------------------------------------------------------
uintptr_t MicroScheduler::_isolateSchedule(uintptr_t isolationTag)
{
    uint32_t workerIndex = m_pWorkerPool->thisWorkerIndex();
    uintptr_t oldTag = m_pSchedulesByIdx[workerIndex].m_isolationTag;
    m_pSchedulesByIdx[workerIndex].m_isolationTag = isolationTag;
    return oldTag;
}

//--------------------------------------------------------------------------
void Task::waitForChildren(TaskContext const& ctx)
{
    m_pMyScheduler->_wait(this, nullptr, ctx.workerIndex);
}

//--------------------------------------------------------------------------
void Task::destroy(TaskContext const& ctx)
{
    m_pMyScheduler->_freeTask(ctx.workerIndex, this);
}

//------------------------------------------------------------------------------
void MicroScheduler::_freeTask(uint32_t workerIdx, Task* pTask)
{
    GTS_ASSERT(pTask != nullptr);

    GTS_ANALYSIS_COUNT(workerIdx, gts::analysis::AnalysisType::NUM_FREES);
    GTS_ANALYSIS_TIME_SCOPED(workerIdx, gts::analysis::AnalysisType::NUM_FREES);
    GTS_INSTRUMENTER_SCOPED(analysis::Tag::INTERNAL, "FREE TASK", pTask, 0);

    pTask->m_fcnDataDestructor(pTask->_dataSuffix());
    m_pTaskAllocator->free(workerIdx, pTask);
}

// ACCESSORS:

//------------------------------------------------------------------------------
bool MicroScheduler::isRunning() const
{
    return m_isRunning.load(gts::memory_order::acquire);
}

//------------------------------------------------------------------------------
uint32_t MicroScheduler::thisWorkerIndex()
{
    return m_pWorkerPool->thisWorkerIndex();
}

//------------------------------------------------------------------------------
uint32_t MicroScheduler::workerCount() const
{
    return m_scheduleCount;
}

//------------------------------------------------------------------------------
bool MicroScheduler::isPartition() const
{
    return m_pRootMicroScheduler != nullptr;
}
//------------------------------------------------------------------------------
MicroScheduler::MicroScheduler()
    : m_pTaskQueue(nullptr)
    , m_pTaskAllocator(nullptr)
    , m_pWorkerPool(nullptr)
    , m_pSchedulesByIdx(nullptr)
    , m_pScheduleOwnershipByIdx(nullptr)
    , m_pRootMicroScheduler(nullptr)
    , m_scheduleCount(0)
    , m_isRunning(false)
    , m_canStealFromRoot(false)
{}

//------------------------------------------------------------------------------
MicroScheduler::~MicroScheduler()
{
    if (!isPartition())
    {
        shutdown();
    }
}

//------------------------------------------------------------------------------
bool MicroScheduler::initialize(WorkerPool* pWorkerPool, uint32_t priorityLevelCount)
{
    GTS_ASSERT(pWorkerPool != nullptr);

    MicroSchedulerDesc desc;
    desc.pWorkerPool = pWorkerPool;
    desc.priorityCount = priorityLevelCount;

    return initialize(desc);
}

//------------------------------------------------------------------------------
bool MicroScheduler::initialize(MicroSchedulerDesc const& desc)
{
    // If reinitializing, shutdown the previous scheduler.
    shutdown();

    m_isRunning.store(true);

    GTS_ASSERT(desc.pWorkerPool != nullptr);
    m_pWorkerPool = desc.pWorkerPool;
    m_scheduleCount = m_pWorkerPool->m_totalWorkerCount;

    // Size data structures to account for all worker indices.
    if (m_scheduleCount <= 0)
    {
        GTS_ASSERT(m_scheduleCount > 0);
        return false;
    }

    GTS_INSTRUMENTER_SCOPED(analysis::Tag::INTERNAL, "Scheduler Init", 0, 0);

    uint32_t priorityCount = desc.priorityCount;
    if (priorityCount <= 0)
    {
        GTS_ASSERT(priorityCount > 0);
        return false;
    }

    // Initialize non-worker queue per priority.
    m_pTaskQueue = gts::alignedNew<TaskQueue, GTS_NO_SHARING_CACHE_LINE_SIZE>();
    m_pTaskAllocator = gts::alignedNew<TaskAllocator, GTS_NO_SHARING_CACHE_LINE_SIZE>(m_scheduleCount);

    // Create and init all the Schedules.
    m_pSchedulesByIdx = gts::alignedVectorNew<Schedule, GTS_NO_SHARING_CACHE_LINE_SIZE>(m_scheduleCount);
    m_pScheduleOwnershipByIdx = gts::alignedVectorNew<bool, GTS_NO_SHARING_CACHE_LINE_SIZE>(m_scheduleCount);
    for (uint32_t ii = 0; ii < m_scheduleCount; ++ii)
    {
        m_pSchedulesByIdx[ii].initialize(
            ii,
            this,
            desc.priorityCount,
            desc.priorityBoostAge);

        m_pScheduleOwnershipByIdx[ii] = true;
    }

    // Set the Victim pools
    _setupVictimPools(
        m_pSchedulesByIdx,
        m_pScheduleOwnershipByIdx,
        m_pSchedulesByIdx,
        m_pScheduleOwnershipByIdx,
        m_scheduleCount);

    m_pWorkerPool->_registerScheduler(this);

    return true;
}

//------------------------------------------------------------------------------
void MicroScheduler::shutdown()
{
    if (isPartition())
    {
        GTS_ASSERT(0 && "Cannot shutdown a parition.");
        return;
    }

    if (!isRunning())
    {
        return;
    }

    GTS_INSTRUMENTER_SCOPED(analysis::Tag::INTERNAL, "Scheduler Shutdown", 0, 0);

    //
    // Tell this MicroScheduler and its partitions to quit.

    m_isRunning.store(false, gts::memory_order::release);

    for (size_t ii = 0; ii < m_partitions.size(); ++ii)
    {
        MicroScheduler* pMicroScheduler = m_partitions[ii];
        pMicroScheduler->m_isRunning.store(false, gts::memory_order::release);
    }

    //
    // If the MicroScheduler and its partitions are registered, unregister them.

    if (m_pWorkerPool)
    {
        for (size_t ii = 0; ii < m_pWorkerPool->m_partitions.size(); ++ii)
        {
            MicroScheduler* pMicroScheduler = m_partitions[ii];
            if (pMicroScheduler->m_pWorkerPool)
            {
                pMicroScheduler->m_pWorkerPool->_unRegisterScheduler(pMicroScheduler);
                pMicroScheduler->m_pWorkerPool = nullptr;
            }
        }

        m_pWorkerPool->_unRegisterScheduler(this);
        m_pWorkerPool = nullptr;
    }

    //
    // Delete the paritions.

    for (size_t ii = 0; ii < m_partitions.size(); ++ii)
    {
        gts::alignedDelete(m_partitions[ii]->m_pTaskQueue);
        gts::alignedVectorDelete(m_partitions[ii]->m_pScheduleOwnershipByIdx, m_scheduleCount);
        delete m_partitions[ii];
    }
    m_partitions.clear();

    m_pRootMicroScheduler = nullptr;

    gts::alignedVectorDelete(m_pScheduleOwnershipByIdx, m_scheduleCount);
    m_pScheduleOwnershipByIdx = nullptr;

    gts::alignedVectorDelete(m_pSchedulesByIdx, m_scheduleCount);
    m_pSchedulesByIdx = nullptr;

    gts::alignedDelete(m_pTaskAllocator);
    m_pTaskAllocator = nullptr;

    gts::alignedDelete(m_pTaskQueue);
    m_pTaskQueue = nullptr;
}

//------------------------------------------------------------------------------
MicroScheduler* MicroScheduler::makePartition(WorkerPool* pParitionedPool, bool canStealFromMainParition)
{
    if (pParitionedPool == nullptr || !pParitionedPool->isParition())
    {
        GTS_ASSERT(0 && "WorkerPool must be a parition.");
        return nullptr;
    }

    if (!pParitionedPool->isRunning())
    {
        GTS_ASSERT(0 && "The WorkerPool cannot be running.");
        return nullptr;
    }

    if (isPartition())
    {
        GTS_ASSERT(0 && "Cannot partition a partition.");
        return nullptr;
    }

    GTS_INSTRUMENTER_SCOPED(analysis::Tag::INTERNAL, "Scheduler Make Partition", 0, 0);

    // halt all Worker to prevent any races while reassigning Schedules.
    m_pWorkerPool->_haltAllWorkers();

    // Copy data from this MicroScheduler to the new partition.
    MicroScheduler* pPartition            = new MicroScheduler;
    pPartition->m_pRootMicroScheduler     = this;
    pPartition->m_pTaskQueue              = gts::alignedNew<TaskQueue, GTS_CACHE_LINE_SIZE>();
    pPartition->m_pScheduleOwnershipByIdx = gts::alignedVectorNew<bool, GTS_CACHE_LINE_SIZE>(m_scheduleCount);
    pPartition->m_pTaskAllocator          = m_pTaskAllocator;
    pPartition->m_pWorkerPool             = pParitionedPool;
    pPartition->m_canStealFromRoot        = canStealFromMainParition;
    pPartition->m_pSchedulesByIdx         = m_pSchedulesByIdx;
    pPartition->m_scheduleCount           = m_scheduleCount;
    pPartition->m_isRunning.store(true, gts::memory_order::relaxed);


    // For each Worker in the partitioned WorkerPool, assign the associated
    // Schedule to this partition. 1-1 map between Workers and Schedules.
    Vector<uint32_t> workerIndices;
    pParitionedPool->getWorkerIndices(workerIndices);
    for (size_t ii = 0; ii < workerIndices.size(); ++ii)
    {
        uint32_t index = workerIndices[ii];
        if (m_pScheduleOwnershipByIdx[index] == false)
        {
            delete pPartition; // destroy failed partition.

            GTS_ASSERT(0 && "Schedule index belongs to another parition.");
            m_pWorkerPool->_resumeAllWorkers();
            return nullptr;
        }

        // Remove the Schedule from this MicroScheduler and add it to the parition.
        m_pScheduleOwnershipByIdx[index]             = false;
        pPartition->m_pScheduleOwnershipByIdx[index] = true;
    }

    // Point the Parition's Schedules to the Parition.
    for (size_t ii = 0; ii < m_scheduleCount; ++ii)
    {
        if (pPartition->m_pScheduleOwnershipByIdx[ii])
        {
            Schedule& schedule             = m_pSchedulesByIdx[ii];
            schedule.m_pMyScheduler        = pPartition;
            schedule.m_pTaskQueue = pPartition->m_pTaskQueue;
        }
    }

    // Repopulate the victim pools
    if (canStealFromMainParition)
    {
        // If this partition can steal from this scheduler, expose its schedules.
        _setupVictimPools(
            pPartition->m_pSchedulesByIdx,
            pPartition->m_pScheduleOwnershipByIdx,
            m_pSchedulesByIdx,
            m_pScheduleOwnershipByIdx,
            m_scheduleCount);
    }
    else
    {
        // If this partition can only steal from its own schedules.
        _setupVictimPools(
            pPartition->m_pSchedulesByIdx,
            pPartition->m_pScheduleOwnershipByIdx,
            pPartition->m_pSchedulesByIdx,
            pPartition->m_pScheduleOwnershipByIdx,
            m_scheduleCount);
    }

    _setupVictimPools(
        m_pSchedulesByIdx,
        m_pScheduleOwnershipByIdx,
        m_pSchedulesByIdx,
        m_pScheduleOwnershipByIdx,
        m_scheduleCount);

    // Track the parition
    m_partitions.push_back(pPartition);

    // Register the partition with it's worker pool.
    pPartition->m_pWorkerPool->m_registeredSchedulers.push_back(pPartition);

    // Resume the workers.
    pPartition->m_pWorkerPool->_resumeAllWorkers();
    m_pWorkerPool->_resumeAllWorkers();

    return pPartition;
}

//------------------------------------------------------------------------------
bool MicroScheduler::hasTasks() const
{
    for (size_t ii = 0; ii < m_scheduleCount; ++ii)
    {
        if (m_pScheduleOwnershipByIdx[ii] && m_pSchedulesByIdx[ii].hasTasks())
        {
            return true;
        }
    }

    if (!m_pTaskQueue->empty())
    {
        return true;
    }

    if (m_canStealFromRoot)
    {
        for (size_t ii = 0; ii < m_pRootMicroScheduler->m_scheduleCount; ++ii)
        {
            if (m_pRootMicroScheduler->m_pScheduleOwnershipByIdx[ii]
                && m_pRootMicroScheduler->m_pSchedulesByIdx[ii].hasTasks())
            {
                return true;
            }
        }
    }

    return false;
}

//------------------------------------------------------------------------------
void MicroScheduler::_setupVictimPools(Schedule*& pDstSched, bool* pDstOwnership, Schedule* const pSrcSched, bool* pSrcOwnership, uint32_t scheduleCount)
{
    // For each schedule,
    for (uint32_t scheduleIdx = 0; scheduleIdx < scheduleCount; ++scheduleIdx)
    {
        // Is the index owned by the destination?
        if (!pDstOwnership[scheduleIdx])
        {
            // No, skip.
            continue;
        }

        // Create a victim pool from all schedules owned by the source.
        Vector<Schedule*> victimPool;
        for (uint32_t ii = 0; ii < scheduleCount; ++ii)
        {
            // If not the destination schedule and the source owns it.
            if (ii != scheduleIdx && pSrcOwnership[ii])
            {
                // Add it.
                victimPool.push_back(&pSrcSched[ii]);
            }
        }
        // Assign the pool the the desination Schedule.
        pDstSched[scheduleIdx].setVictimPool(victimPool);
    }
}

} // namespace gts
