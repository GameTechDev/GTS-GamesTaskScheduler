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

#include "gts/analysis/Trace.h"
#include "gts/analysis/Counter.h"
#include "gts/synchronization/Lock.h"
#include "gts/micro_scheduler/WorkerPool.h"

#include "Worker.h"
#include "LocalScheduler.h"
#include "Containers.h"

namespace gts {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// MicroScheduler:

Atomic<SubIdType> MicroScheduler::s_nextSchedulerId = { 0 };

// STRUCTORS:

//------------------------------------------------------------------------------
MicroScheduler::MicroScheduler()
    : m_pPriorityTaskQueue(nullptr)
    , m_pWorkerPool(nullptr)
    , m_pMyMaster(nullptr)
    , m_ppLocalSchedulersByIdx(nullptr)
    , m_pExternalSchedulers(nullptr)
    , m_pCallbacks(nullptr)
    , m_creationThreadId(0)
    , m_localSchedulerCount(0)
    , m_schedulerId(UINT16_MAX)
    , m_isAttached(false)
    , m_canStealExternally(true)
    , m_isActive(true)
{}

//------------------------------------------------------------------------------
MicroScheduler::~MicroScheduler()
{
    shutdown();
}

//------------------------------------------------------------------------------
bool MicroScheduler::initialize(WorkerPool* pWorkerPool)
{
    GTS_ASSERT(pWorkerPool != nullptr);

    MicroSchedulerDesc desc;
    desc.pWorkerPool = pWorkerPool;

    return initialize(desc);
}

//------------------------------------------------------------------------------
bool MicroScheduler::initialize(MicroSchedulerDesc const& desc)
{
    // If reinitializing, shutdown the previous scheduler.
    shutdown();

    m_schedulerId = s_nextSchedulerId.fetch_add(1, memory_order::acq_rel);
    GTS_ASSERT(m_schedulerId != UINT16_MAX && "ID overflow"); // just in case anyone gets scheduler happy.

    m_isAttached.store(true, memory_order::release);

    memcpy(m_debugName, desc.name, DESC_NAME_SIZE);
    m_creationThreadId = ThisThread::getId();

    GTS_ASSERT(desc.pWorkerPool != nullptr);
    m_pWorkerPool = desc.pWorkerPool;
    m_localSchedulerCount = m_pWorkerPool->m_workerCount;
    m_canStealExternally = desc.canStealExternalTasks;

    if (m_localSchedulerCount <= 0)
    {
        GTS_ASSERT(m_localSchedulerCount > 0);
        return false;
    }

    uint32_t priorityCount = desc.priorityCount;
    if (priorityCount <= 0)
    {
        GTS_ASSERT(priorityCount > 0);
        return false;
    }

    GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::MICRO_SCHEDULER_DEBUG, analysis::Color::AntiqueWhite, "MIRCOSCHED INIT", this, 0);

    m_pExternalSchedulers = alignedNew<ExternalSchedulers, GTS_NO_SHARING_CACHE_LINE_SIZE>();
    m_pCallbacks          = alignedVectorNew<Callbacks, GTS_NO_SHARING_CACHE_LINE_SIZE>((size_t)MicroSchedulerCallbackType::COUNT);

    // Make sure this Worker is registered and is referenced as our Master.
    uintptr_t workerState = m_pWorkerPool->m_pGetThreadLocalStateFcn();
    if (workerState == 0)
    {
        m_pMyMaster = alignedNew<Worker, GTS_NO_SHARING_CACHE_LINE_SIZE>();
        m_pMyMaster->initialize(
            m_pWorkerPool,
            OwnedId(m_schedulerId, 0),
            WorkerThreadDesc::GroupAndAffinity(),
            Thread::Priority::PRIORITY_NORMAL,
            nullptr,
            nullptr,
            m_pWorkerPool->m_pGetThreadLocalStateFcn,
            m_pWorkerPool->m_pSetThreadLocalStateFcn,
            nullptr,
            m_pWorkerPool->m_pWorkersByIdx[0].m_cachableTaskSize,
            0,
            true);

        m_pWorkerPool->m_pSetThreadLocalStateFcn((uintptr_t)m_pMyMaster);
    }
    else
    {
        m_pMyMaster = (Worker*)workerState;
        m_pMyMaster->m_refCount.fetch_add(1, memory_order::release);
        fflush(stdout);
    }

    // Initialize non-worker queue per priority.
    m_pPriorityTaskQueue = alignedNew<PriorityTaskQueue, GTS_NO_SHARING_CACHE_LINE_SIZE>();
    m_pPriorityTaskQueue->resize(priorityCount);

    // Create and init all the Schedulers.
    m_ppLocalSchedulersByIdx = alignedVectorNew<LocalScheduler*, GTS_NO_SHARING_CACHE_LINE_SIZE>(m_localSchedulerCount);
    for (uint16_t ii = 0; ii < m_localSchedulerCount; ++ii)
    {
        m_ppLocalSchedulersByIdx[ii] = alignedNew<LocalScheduler, GTS_NO_SHARING_CACHE_LINE_SIZE>();
        m_ppLocalSchedulersByIdx[ii]->initialize(
            OwnedId(m_schedulerId, ii),
            this,
            desc.priorityCount,
            desc.priorityBoostAge,
            desc.canStealBackTasks);
    }

    // Attach the Schedulers to Workers.
    _registerWithWorkerPool(m_pWorkerPool);

    return true;
}

//------------------------------------------------------------------------------
void MicroScheduler::shutdown()
{
    if (!isRunning())
    {
        return;
    }

    GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::MICRO_SCHEDULER_DEBUG, analysis::Color::AntiqueWhite, "MIRCOSCHED SHUTDOWN", this, 0);

    // Tell this MicroScheduler to quit.
    m_isAttached.store(false, memory_order::release);

    GTS_ASSERT(m_pWorkerPool != nullptr);

    // Unregister from the WorkerPool
    _unRegisterFromWorkerPool(m_pWorkerPool, true);

    // WARNING! External unregisters must happen after _unRegisterFromWorkerPool
    m_pExternalSchedulers->shutdown(this);

    // Must delete before the WorkerPool to cleanup internal Task resources.
    for (uint16_t ii = 0; ii < m_localSchedulerCount; ++ii)
    {
        alignedDelete(m_ppLocalSchedulersByIdx[ii]);
    }
    alignedVectorDelete(m_ppLocalSchedulersByIdx, m_localSchedulerCount);
    m_ppLocalSchedulersByIdx = nullptr;

    // A MicroScheduler must be shutdown on the thread that created it, or 
    // it may clear the wrong TL storage.
    GTS_ASSERT(m_pMyMaster->m_threadId == ThisThread::getId() && "A MicroScheduler must be shutdown on the thread that created it.");
    if (m_pMyMaster->m_refCount.fetch_sub(1, memory_order::acq_rel) - 1 == 0)
    {
        // Delete our Master Worker if only we referenced it. This occurs
        // when a MicroScheduler is initialized on a thread outside the
        // WorkerPool.
        m_pMyMaster->shutdown();
        alignedDelete(m_pMyMaster);

        // Clear TL storage.
        m_pWorkerPool->m_pSetThreadLocalStateFcn(0);
    }
    m_pWorkerPool = nullptr;

    alignedVectorDelete(m_pCallbacks, (size_t)MicroSchedulerCallbackType::COUNT);
    m_pCallbacks = nullptr;

    alignedDelete(m_pExternalSchedulers);
    m_pExternalSchedulers = nullptr;

    alignedDelete(m_pPriorityTaskQueue);
    m_pPriorityTaskQueue = nullptr;

    char filename[128];
#ifdef GTS_MSVC
    sprintf_s(filename, "MicroScheduler_Analysis_%d.txt", m_schedulerId);
#else
    snprintf(filename, 128, "MicroScheduler_Analysis_%d.txt", m_schedulerId);
#endif
    GTS_MS_COUNTER_DUMP_TO(m_schedulerId, filename);
}

//------------------------------------------------------------------------------
void MicroScheduler::resetIdGenerator()
{
    s_nextSchedulerId.exchange(0, memory_order::seq_cst);
}

//------------------------------------------------------------------------------
void* MicroScheduler::_allocateRawTask(uint32_t size)
{
    GTS_SIM_TRACE_MARKER(sim_trace::MARKER_ALLOCATE_TASK_BEGIN);

    GTS_TRACE_SCOPED_ZONE_P1(analysis::CaptureMask::MICRO_SCHEDULER_ALL, analysis::Color::Magenta, "MIRCOSCHED ALLOC TASK", this);

    uintptr_t state = Worker::getLocalState();
    Task* pTask;

    if(state)
    {
        GTS_TRACE_SCOPED_ZONE_P1(analysis::CaptureMask::MICRO_SCHEDULER_ALL, analysis::Color::Magenta, "MIRCOSCHED ALLOC KNOWN WORKER", this);

        Worker* pWorker = (Worker*)state;
        pTask = pWorker->allocateTask(size);
        GTS_ASSERT(pTask != nullptr);

        pTask->header().pMyLocalScheduler = m_ppLocalSchedulersByIdx[pWorker->id().localId()];
    }
    else
    {
        GTS_TRACE_SCOPED_ZONE_P1(analysis::CaptureMask::MICRO_SCHEDULER_ALL, analysis::Color::DarkMagenta, "MIRCOSCHED ALLOC UNKNOWN WORKER", this);

        internal::TaskHeader* pTaskHeader = 
            new (GTS_ALIGNED_MALLOC(size + sizeof(internal::TaskHeader), GTS_NO_SHARING_CACHE_LINE_SIZE)) internal::TaskHeader();
        GTS_ASSERT(pTaskHeader != nullptr);
        pTask = pTaskHeader->_task();
    }


    GTS_SIM_TRACE_MARKER(sim_trace::MARKER_ALLOCATE_TASK_END);
    return pTask;
}

//------------------------------------------------------------------------------
void MicroScheduler::_wakeWorkers(Worker* pThisWorker, uint32_t count, bool reset, bool wakeExternalVictims)
{
    GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::MICRO_SCHEDULER_ALL, analysis::Color::Orange2, "MIRCOSCHED WAKE WORKERS", this, count);

    m_pWorkerPool->_wakeWorker(pThisWorker, count, reset);

    if (wakeExternalVictims)
    {
        m_pExternalSchedulers->wakeThieves(pThisWorker, 1, reset);
    }
}

//------------------------------------------------------------------------------
void MicroScheduler::_addTask(Worker* pWorker, Task* pTask, uint32_t priority)
{
    GTS_SIM_TRACE_MARKER(sim_trace::MARKER_PUSH_TASK_BEGIN);

    GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::MICRO_SCHEDULER_ALL, analysis::Color::SeaGreen, "MIRCOSCHED ADD TASK", this, pTask);

    GTS_ASSERT(pTask != nullptr);
    GTS_ASSERT(!(pTask->header().flags & internal::TaskHeader::TASK_IS_CONTINUATION) &&
        "Cannot queue a continuation.");

    pTask->header().executionState = internal::TaskHeader::READY;

    uint32_t mandatoryAffinity = pTask->header().affinity;

    GTS_MS_COUNTER_INC(pWorker ? pWorker->id() : OwnedId(), analysis::MicroSchedulerCounters::NUM_SPAWNS);

    bool result = false;
    GTS_UNREFERENCED_PARAM(result);

    // If this Task has a mandatory affinity.
    if (mandatoryAffinity != ANY_WORKER)
    {
        GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::MICRO_SCHEDULER_DEBUG, analysis::Color::SeaGreen, "MIRCOSCHED SPAWN AFFINITY TASK", this, pTask);

        GTS_ASSERT(mandatoryAffinity < m_localSchedulerCount && "Worker index out of range.");

        result = m_ppLocalSchedulersByIdx[mandatoryAffinity]->queueAffinityTask(pTask, priority);
        GTS_ASSERT(result && "Affinity queue overflow");

        // Wake the worker.
        m_pWorkerPool->m_pWorkersByIdx[mandatoryAffinity].wake(1, false, true);
    }
    // This thread is an actual WorkerPool thread or Master thread and
    // it executes work on this scheduler.
    else if (pWorker != nullptr && pWorker->id().ownerId() == m_pWorkerPool->poolId())
    {
        GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::MICRO_SCHEDULER_DEBUG, analysis::Color::SeaGreen, "MIRCOSCHED SPAWN TASK", this, pTask);

        SubIdType localId = pWorker->id().localId();

        // we are good to queue into this Worker's deque.
        LocalScheduler* pLocalScheduler = m_ppLocalSchedulersByIdx[localId];
        result = pLocalScheduler->spawnTask(pTask, priority);
        GTS_ASSERT(result && "Task queue overflow");

        // Try to wake a workers.
        _wakeWorkers(pWorker, 1, true, true);
    }
    // This Worker is does not execute work on this MicroScheduler so there is
    // no storage for it and we must use the MPMC Task queue.
    else
    {
        GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::MICRO_SCHEDULER_DEBUG, analysis::Color::SeaGreen, "MIRCOSCHED QUEUE OFF-SCHED TASK", this, pTask);

        result = (*m_pPriorityTaskQueue)[priority].tryPush(pTask);
        GTS_ASSERT(result && "Task queue overflow");

        // Wake any worker.
        _wakeWorkers(pWorker, 1, true, true);
    }

    GTS_SIM_TRACE_MARKER(sim_trace::MARKER_PUSH_TASK_END);
}

//------------------------------------------------------------------------------
void MicroScheduler::spawnTask(Task* pTask, uint32_t priority)
{
    uintptr_t state = Worker::getLocalState();
    Worker* pWorker = (Worker*)state;
    _addTask(pWorker, pTask, priority);
}

//------------------------------------------------------------------------------
void MicroScheduler::spawnTaskAndWait(Task* pTask, uint32_t priority)
{
    EmptyTask* pWaiter = allocateTask<EmptyTask>();
    pWaiter->header().executionState = internal::TaskHeader::ALLOCATED;
    pWaiter->header().flags |= internal::TaskHeader::TASK_IS_WAITER;
    // Ref count is self + pTask + wait. Wait ref distinguishes pWaiter from a continuation.
    pWaiter->header().refCount.store(3, memory_order::relaxed);
    pWaiter->addChildTaskWithoutRef(pTask);

    // Queue the task if we cannot execute it
    uintptr_t state = Worker::getLocalState();
    Worker* pWorker = (Worker*)state;
    if(!pWorker || (pTask->header().affinity != ANY_WORKER && pTask->header().affinity != pWorker->id().localId()))
    {
        _addTask(pWorker, pTask, priority);
        _wait(pWorker, pWaiter, nullptr);
    }
    else
    {
        _wait(pWorker, pWaiter, pTask);
    }

    destoryTask(pWaiter);
}

//------------------------------------------------------------------------------
void MicroScheduler::waitFor(Task* pTask)
{
    uintptr_t state = Worker::getLocalState();
    Worker* pWorker = (Worker*)state;
    _wait(pWorker, pTask, nullptr);
}

//------------------------------------------------------------------------------
void MicroScheduler::waitForAll()
{
    uintptr_t state = Worker::getLocalState();
    Worker* pWorker = (Worker*)state;
    _wait(pWorker, nullptr, nullptr);
}

//------------------------------------------------------------------------------
void MicroScheduler::_wait(Worker* pWorker, Task* pWaiterTask, Task* pChild)
{
    GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::MICRO_SCHEDULER_DEBUG, analysis::Color::AntiqueWhite, "MIRCOSCHED RUN UNTIL DONE", this, pWaiterTask);

    if (pWorker != nullptr)
    {
        m_ppLocalSchedulersByIdx[pWorker->id().localId()]->runUntilDone(pWaiterTask, pChild);
        GTS_ASSERT(pWaiterTask ? pWaiterTask->refCount() == 1 : true);
    }
    else if(pWaiterTask)
    {
        _wakeWorkers(pWorker, 1, true, true);

        // TODO: use Event

        // Block until done.
        BackoffType backoff;
        while (pWaiterTask->refCount() > 2)
        {
            backoff.tick();
            _wakeWorkers(pWorker, 1, true, true);
        }
    }
}

//------------------------------------------------------------------------------
void MicroScheduler::destoryTask(Task* pTask)
{
    GTS_TRACE_SCOPED_ZONE_P1(analysis::CaptureMask::MICRO_SCHEDULER_DEBUG, analysis::Color::Magenta, "MIRCOSCHED DESTORY TASK", this);

    pTask->~Task();
    Task* pParent = pTask->parent();
    _freeTask(pTask);

    if(pParent)
    {
        if(pParent->refCount() == 2)
        {
            pParent->setRef(1, memory_order::relaxed);
        }
        else
        {
            GTS_SPECULATION_FENCE();
            if(pParent->removeRef(1) > 1)
            {
                return;
            }
        }
        spawnTask(pParent);
    }
}

//--------------------------------------------------------------------------
void MicroScheduler::addExternalVictim(MicroScheduler* pScheduler)
{
    if (pScheduler == this)
    {
        GTS_ASSERT(0 && "Self victimization is not allowed.");
        return;
    }

    GTS_MS_COUNTER_INC(OwnedId(m_schedulerId, 0), gts::analysis::MicroSchedulerCounters::NUM_SCHEDULER_REGISTERS);

    m_pExternalSchedulers->registerVictim(pScheduler, this);
}

//--------------------------------------------------------------------------
void MicroScheduler::removeExternalVictim(MicroScheduler* pScheduler)
{
    GTS_MS_COUNTER_INC(OwnedId(m_schedulerId, 0), gts::analysis::MicroSchedulerCounters::NUM_SCHEDULER_UNREGISTERS);

    m_pExternalSchedulers->unregisterVictim(pScheduler, this);
}

//------------------------------------------------------------------------------
void MicroScheduler::_freeTask(Task* pTask)
{
    GTS_ASSERT(pTask != nullptr);

    GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::MICRO_SCHEDULER_ALL, analysis::Color::Magenta, "MIRCOSCHED FREE TASK", this, pTask);

    uintptr_t state = Worker::getLocalState();
    if(state)
    {
        GTS_TRACE_SCOPED_ZONE_P1(analysis::CaptureMask::MICRO_SCHEDULER_DEBUG, analysis::Color::Magenta, "MIRCOSCHED FREE KNOWN TASK", this);
        ((Worker*)state)->freeTask(pTask);
    }
    else
    {
        GTS_TRACE_SCOPED_ZONE_P1(analysis::CaptureMask::MICRO_SCHEDULER_DEBUG, analysis::Color::DarkMagenta, "MIRCOSCHED FREE UNKNOWN TASK", this);
        GTS_ALIGNED_FREE(&pTask->header());
    }
}

//------------------------------------------------------------------------------
void MicroScheduler::wakeWorker()
{
    uintptr_t state = Worker::getLocalState();
    Worker* pWorker = (Worker*)state;
    _wakeWorkers(pWorker, 1, true, false);
}

//------------------------------------------------------------------------------
bool MicroScheduler::stealAndExecuteTask()
{
    uintptr_t state = Worker::getLocalState();
    Worker* pWorker = (Worker*)state;

    if (pWorker != nullptr)
    {
        LocalScheduler* pScheduler = m_ppLocalSchedulersByIdx[pWorker->id().localId()];
        uint16_t localId = pScheduler->m_id.localId();
        bool unused;

        Task* pTask = pScheduler->_getNonLocalTask(pWorker, true, false, false, localId, unused);
        if (!pTask)
        {
            pTask = pScheduler->_stealExternalTask(localId);
        }

        if (pTask)
        {
            pScheduler->_executeTaskLoop(pTask, localId, pWorker, unused);
            return true;
        }
    }

    return false;
}

// ACCESSORS:

//------------------------------------------------------------------------------
bool MicroScheduler::hasDemand(bool clear) const
{
    uintptr_t state = Worker::getLocalState();
    Worker* pWorker = (Worker*)state;

    if (pWorker == nullptr)
    {
        return false;
    }

    LocalScheduler* pScheduler = m_ppLocalSchedulersByIdx[pWorker->id().localId()];
    if (clear)
    {
        return pScheduler->m_hasDemand.exchange(false, memory_order::acq_rel);
    }
    else
    {
        return pScheduler->m_hasDemand.load(memory_order::acquire);
    }
}

//------------------------------------------------------------------------------
bool MicroScheduler::isRunning() const
{
    return m_isAttached.load(memory_order::acquire);
}

//------------------------------------------------------------------------------
OwnedId MicroScheduler::thisWorkerId() const
{
    return m_pWorkerPool->thisWorkerId();
}

//------------------------------------------------------------------------------
uint32_t MicroScheduler::workerCount() const
{
    return m_localSchedulerCount;
}

//------------------------------------------------------------------------------
bool MicroScheduler::_hasDequeTasks() const
{
    for (size_t ii = 0, len = m_localSchedulerCount; ii < len; ++ii)
    {
        if(m_ppLocalSchedulersByIdx[ii]->hasDequeTasks())
        {
            GTS_TRACE_ZONE_MARKER_P1(analysis::CaptureMask::MICRO_SCHEDULER_ALL, analysis::Color::AntiqueWhite, "HAS DEQUE TASK", ii);
            return true;
        }
    }
    return false; 
}

//------------------------------------------------------------------------------
bool MicroScheduler::_hasAffinityTasks() const
{
    uintptr_t state = Worker::getLocalState();
    Worker* pWorker = (Worker*)state;

    if (!pWorker || pWorker->id().localId() >= m_localSchedulerCount)
    {
        return false;
    }

    if (m_ppLocalSchedulersByIdx[pWorker->id().localId()]->hasAffinityTasks())
    {
        GTS_TRACE_ZONE_MARKER_P0(analysis::CaptureMask::MICRO_SCHEDULER_ALL, analysis::Color::AntiqueWhite, "HAS AFFINITY TASK");
        return true;
    }
    return false;
}

//------------------------------------------------------------------------------
bool MicroScheduler::_hasQueueTasks() const
{
    for (size_t iPriority = 0, numPrios = m_pPriorityTaskQueue->size(); iPriority < numPrios; ++iPriority)
    {
        if (!(*m_pPriorityTaskQueue)[iPriority].empty())
        {
            GTS_TRACE_ZONE_MARKER_P1(analysis::CaptureMask::MICRO_SCHEDULER_ALL, analysis::Color::AntiqueWhite, "HAS QUEUE TASK", iPriority);
            return true;
        }
    }
    return false;
}

//------------------------------------------------------------------------------
bool MicroScheduler::hasTasks() const
{
    if (_hasDequeTasks())
    {
        GTS_TRACE_ZONE_MARKER_P0(analysis::CaptureMask::MICRO_SCHEDULER_ALL, analysis::Color::YellowGreen, "HAS DEQUE TASK");
        return true;
    }

    if (_hasAffinityTasks())
    {
        GTS_TRACE_ZONE_MARKER_P0(analysis::CaptureMask::MICRO_SCHEDULER_ALL, analysis::Color::YellowGreen, "HAS AFFINITY TASK");
        return true;
    }

    if (_hasQueueTasks())
    {
        GTS_TRACE_ZONE_MARKER_P0(analysis::CaptureMask::MICRO_SCHEDULER_ALL, analysis::Color::YellowGreen, "HAS QUEUE TASK");
        return true;
    }

    GTS_TRACE_ZONE_MARKER_P0(analysis::CaptureMask::MICRO_SCHEDULER_ALL, analysis::Color::Orange, "NO TASKS FOUND");
    return false;
}

//------------------------------------------------------------------------------
Task* MicroScheduler::_getTask(Worker* pWorker, bool considerAffinity, bool callerIsExternal, SubIdType localId, bool& executedTask)
{
    Task* pTask = nullptr;
    for (size_t ii = 0, len = m_localSchedulerCount; ii < len; ++ii)
    {
        bool checkForMyAffinityTask = considerAffinity && ii == localId;
        pTask = m_ppLocalSchedulersByIdx[ii]->_getNonLocalTask(pWorker, checkForMyAffinityTask, callerIsExternal, true, localId, executedTask);
        if (pTask)
        {
            break;
        }
    }
    return pTask;
}

//------------------------------------------------------------------------------
Task* MicroScheduler::_getExternalTask(Worker* pWorker, SubIdType localId, bool& executedTask)
{
    return m_pExternalSchedulers->getTask(pWorker, localId, executedTask);
}

//------------------------------------------------------------------------------
bool MicroScheduler::hasExternalTasks() const
{
    return m_pExternalSchedulers->hasDequeTasks();
}

//------------------------------------------------------------------------------
void MicroScheduler::_registerWithWorkerPool(WorkerPool* pWorkerPool)
{
    constexpr uint32_t startIdx = 1;

    {
        Lock<WorkerPool::MutexType> lock(pWorkerPool->m_pRegisteredSchedulers->mutex);

        m_isAttached.store(true, memory_order::release);

        // Register the MicroScheduler.
        pWorkerPool->m_pRegisteredSchedulers->schedulers.push_back(this);
    }

    // Register each localScheduler with a Worker.
    for (uint32_t ii = startIdx; ii < m_localSchedulerCount; ++ii)
    {
        pWorkerPool->m_pWorkersByIdx[ii].registerLocalScheduler(m_ppLocalSchedulersByIdx[ii]);
    }
}

//------------------------------------------------------------------------------
void MicroScheduler::_unRegisterFromWorkerPool(WorkerPool* pWorkerPool, bool lockWorkerPool)
{
    if (lockWorkerPool)
    {
        m_pWorkerPool->m_pRegisteredSchedulers->mutex.lock();
    }

    bool wasUnregistered = pWorkerPool->_unRegisterScheduler(this);

    if (lockWorkerPool)
    {
        m_pWorkerPool->m_pRegisteredSchedulers->mutex.unlock();
    }

    if (wasUnregistered)
    {
        m_isAttached.store(false, memory_order::release);
    }
}

//------------------------------------------------------------------------------
void MicroScheduler::_unRegisterFromWorkers(WorkerPool* pWorkerPool)
{
    constexpr uint32_t startIdx = 1;

    for (uint32_t iSchedule = startIdx; iSchedule < m_localSchedulerCount; ++iSchedule)
    {
        pWorkerPool->m_pWorkersByIdx[iSchedule].unregisterLocalScheduler(m_ppLocalSchedulersByIdx[iSchedule]);
    }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// class ExternalSchedulers

//------------------------------------------------------------------------------
void MicroScheduler::ExternalSchedulers::shutdown(MicroScheduler* pScheduler)
{
    m_mutex.lock();
    _unregisterAllVictims(pScheduler);
    _unregisterAllThieves(pScheduler);
    m_mutex.unlock();
}

//------------------------------------------------------------------------------
void MicroScheduler::ExternalSchedulers::registerVictim(MicroScheduler* pVictim, MicroScheduler* pThief)
{
    {
        Lock<MutexType> lock(m_mutex);

        GTS_ASSERT(_locateVictim(pVictim) == -1 && "MicroScheduler is already a victim.");
        m_victims.push_back(pVictim);
    }

    // Let the scheduler know that we are a thief
    pVictim->m_pExternalSchedulers->addThief(pThief);
}

//------------------------------------------------------------------------------
void MicroScheduler::ExternalSchedulers::unregisterVictim(MicroScheduler* pVictim, MicroScheduler* pThief, bool useLock)
{
    // Let the victim know that there is no longer a thief referencing it.
    pVictim->m_pExternalSchedulers->removeThief(pThief);

    removeVictim(pVictim, useLock);
}

//------------------------------------------------------------------------------
Task* MicroScheduler::ExternalSchedulers::getTask(Worker* pWorker, SubIdType localId, bool& executedTask)
{
    Task* pTask = nullptr;
    if (m_mutex.try_lock_shared())
    {
        for (size_t iVictim = 0; iVictim < m_victims.size(); ++iVictim)
        {
            MicroScheduler* pVictim = m_victims[iVictim];
            if (pVictim->isActive())
            {
                pTask = m_victims[iVictim]->_getTask(pWorker, false, true, localId, executedTask);
                if (pTask)
                {
                    break;
                }
            }
        }
        m_mutex.unlock_shared();
    }
    return pTask;
}

//------------------------------------------------------------------------------
bool MicroScheduler::ExternalSchedulers::hasDequeTasks() const
{
    bool result = false;
    if (m_mutex.try_lock_shared())
    {
        for (size_t iVictim = 0; iVictim < m_victims.size(); ++iVictim)
        {
            if (m_victims[iVictim]->_hasDequeTasks())
            {
                result = true;
                break;
            }
        }
        m_mutex.unlock_shared();
    }
    return result;
}

//------------------------------------------------------------------------------
bool MicroScheduler::ExternalSchedulers::hasVictimsUnsafe() const
{
    return m_victims.size() == 0;
}

//------------------------------------------------------------------------------
void MicroScheduler::ExternalSchedulers::wakeThieves(Worker* pThisWorker, uint32_t count, bool reset)
{
    if (m_mutex.try_lock_shared())
    {
        for (uint32_t ii = 0; ii < m_thieves.size(); ++ii)
        {
            m_thieves[ii]->_wakeWorkers(pThisWorker, count, reset, false);
        }
        m_mutex.unlock_shared();
    }
}

//------------------------------------------------------------------------------
void MicroScheduler::ExternalSchedulers::addThief(MicroScheduler* pScheduler)
{
    Lock<MutexType> lock(m_mutex);

    GTS_ASSERT(_locateThief(pScheduler) == -1 && "MicroScheduler is already a thief.");
    m_thieves.push_back(pScheduler);
}

//------------------------------------------------------------------------------
void MicroScheduler::ExternalSchedulers::removeThief(MicroScheduler* pScheduler, bool useLock)
{
    if (useLock)
    {
        m_mutex.lock();
    }

    int32_t slotIdx = _locateThief(pScheduler);
    GTS_ASSERT(slotIdx != -1);

    if (slotIdx != (int32_t)m_thieves.size() - 1)
    {
        // swap out
        MicroScheduler* back = m_thieves.back();
        m_thieves.pop_back();
        m_thieves[slotIdx] = back;
    }
    else
    {
        m_thieves.pop_back();
    }

    if (useLock)
    {
        m_mutex.unlock();
    }
}

//------------------------------------------------------------------------------
void MicroScheduler::ExternalSchedulers::removeVictim(MicroScheduler* pScheduler, bool useLock)
{
    if (useLock)
    {
        m_mutex.lock();
    }

    int32_t slotIdx = _locateVictim(pScheduler);
    GTS_ASSERT(slotIdx != -1);

    // Wait until it's not in use by any thieves.
    BackoffType backoff;
    while (m_thiefAccessCount.load(memory_order::acquire) > 0)
    {
        backoff.tick();
    }

    if (slotIdx != (int32_t)m_victims.size() - 1)
    {
        // swap out
        MicroScheduler* back = m_victims.back();
        m_victims.pop_back();
        m_victims[slotIdx] = back;
    }
    else
    {
        m_victims.pop_back();
    }

    if (useLock)
    {
        m_mutex.unlock();
    }
}

//------------------------------------------------------------------------------
void MicroScheduler::ExternalSchedulers::_unregisterAllVictims(MicroScheduler* pThief)
{// Must be called under locked mutex
    while (!m_victims.empty())
    {
        unregisterVictim(m_victims[0], pThief, false);
    }
}

//------------------------------------------------------------------------------
void MicroScheduler::ExternalSchedulers::_unregisterAllThieves(MicroScheduler* pVictim)
{// Must be called under locked mutex
    while (!m_thieves.empty())
    {
        // Let the victim know that there is no longer a thief referencing it.
        m_thieves[0]->m_pExternalSchedulers->removeVictim(pVictim);

        removeThief(m_thieves[0], false);
    }
}

//------------------------------------------------------------------------------
int32_t MicroScheduler::ExternalSchedulers::_locateVictim(MicroScheduler* pScheduler)
{// Must be called under locked mutex
    for (uint32_t ii = 0; ii < m_victims.size(); ++ii)
    {
        if (m_victims[ii] == pScheduler)
        {
            return ii;
        }
    }
    return -1;
}

//------------------------------------------------------------------------------
int32_t MicroScheduler::ExternalSchedulers::_locateThief(MicroScheduler* pScheduler)
{// Must be called under locked mutex
    for (uint32_t ii = 0; ii < m_thieves.size(); ++ii)
    {
        if (m_thieves[ii] == pScheduler)
        {
            return ii;
        }
    }
    return -1;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// class Callbacks

//------------------------------------------------------------------------------
void MicroScheduler::Callbacks::registerCallback(uintptr_t func, void* pData)
{
    Lock<rw_lock_type> lock(m_mutex);

    GTS_ASSERT(_locateCallback(func, pData) == -1 && "Callback is already registered.");
    m_callbacks.push_back(Callback{ func, pData });
}

//------------------------------------------------------------------------------
void MicroScheduler::Callbacks::unregisterCallback(uintptr_t func, void* pData)
{
    Lock<rw_lock_type> lock(m_mutex);

    int32_t slotIdx = _locateCallback(func, pData);
    GTS_ASSERT(slotIdx != -1);

    // swap out
    std::swap(m_callbacks[slotIdx], m_callbacks.back());
    m_callbacks.pop_back();
}

//------------------------------------------------------------------------------
int32_t MicroScheduler::Callbacks::_locateCallback(uintptr_t func, void* pData)
{
    for (uint32_t ii = 0; ii < m_callbacks.size(); ++ii)
    {
        if (m_callbacks[ii].func == func && m_callbacks[ii].pData == pData)
        {
            return ii;
        }
    }
    return -1;
}


} // namespace gts
