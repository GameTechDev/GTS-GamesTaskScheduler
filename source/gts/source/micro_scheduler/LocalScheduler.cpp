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
#include "LocalScheduler.h"

#include "gts/containers/parallel/BinnedAllocator.h"
#include "gts/platform/Assert.h"
#include "gts/analysis/Trace.h"
#include "gts/analysis/Counter.h"

#include "gts/micro_scheduler/MicroScheduler.h"
#include "gts/micro_scheduler/WorkerPool.h"

#include "Worker.h"

namespace gts {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Task:

//------------------------------------------------------------------------------
void Task::waitForAll()
{
    header().pMyLocalScheduler->runUntilDone(this, nullptr);
}

//------------------------------------------------------------------------------
void Task::spawnAndWaitForAll(Task* pChild)
{
    header().pMyLocalScheduler->runUntilDone(this, pChild);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// LocalScheduler:

// STRUCTORS:

//------------------------------------------------------------------------------
LocalScheduler::LocalScheduler()
    : m_pPriorityTaskQueue(nullptr)
    , m_pMyScheduler(nullptr)
    , m_pWaiterTask(nullptr)
    , m_randState(0)
    , m_proirityBoostAgeStart(INT16_MAX)
    , m_proirityBoostAge(INT16_MAX)
    , m_currentProirityToBoost(1)
{}

//------------------------------------------------------------------------------
LocalScheduler::~LocalScheduler()
{
    m_pMyScheduler->destoryTask(m_pWaiterTask);
}

// ACCESSORS:

//------------------------------------------------------------------------------
bool LocalScheduler::hasDequeTasks() const
{
    for (size_t priority = 0, pCount = m_priorityTaskDeque.size(); priority < pCount; ++priority)
    {
        if (!m_priorityTaskDeque[priority].locallyEmtpy())
        {
            return true;
        }
    }

    return false;
}

//------------------------------------------------------------------------------
bool LocalScheduler::hasAffinityTasks() const
{
    for (size_t priority = 0, pCount = m_affinityTaskQueue.size(); priority < pCount; ++priority)
    {
        if (!m_affinityTaskQueue[priority].empty())
        {
            return true;
        }
    }

    return false;
}

// MUTATORS:

//------------------------------------------------------------------------------
void LocalScheduler::initialize(
    OwnedId scheduleId,
    MicroScheduler* pScheduler,
    uint32_t priorityCount,
    int16_t proirityBoostAge)
{
    GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::MICRO_SCHEDULER_DEBUG, analysis::Color::AntiqueWhite, "LocalScheduler Init", scheduleId.localId(), 0);

    m_pPriorityTaskQueue = pScheduler->m_pPriorityTaskQueue;
    m_pMyScheduler       = pScheduler;

    m_id        = scheduleId;
    m_randState = scheduleId.localId() + 1; // can't be zero.

    m_proirityBoostAgeStart = proirityBoostAge;
    m_proirityBoostAge      = m_proirityBoostAgeStart - 1;

    // Initialize the task deques.
    m_priorityTaskDeque.resize(priorityCount);
    m_affinityTaskQueue.resize(priorityCount);

    m_pWaiterTask = m_pMyScheduler->allocateTask<EmptyTask>();
    m_pWaiterTask->header().executionState = internal::TaskHeader::ALLOCATED;
    m_pWaiterTask->header().flags |= internal::TaskHeader::TASK_IS_WAITER;
}

//------------------------------------------------------------------------------
bool LocalScheduler::spawnTask(Task* pTask, uint32_t priority)
{
    return m_priorityTaskDeque[priority].tryPush(pTask);
}

//------------------------------------------------------------------------------
bool LocalScheduler::queueAffinityTask(Task* pTask, uint32_t priority)
{
    return m_affinityTaskQueue[priority].tryPush(pTask);
}

//------------------------------------------------------------------------------
bool LocalScheduler::run(Task* pInitialTask)
{
    // ref count of 3 -> wait forever.
    m_pWaiterTask->header().refCount.store(3, memory_order::relaxed);
    bool executedTask = runUntilDone(m_pWaiterTask, pInitialTask);
    return executedTask;
}

// SCHEDULING:

//------------------------------------------------------------------------------
void LocalScheduler::_handleContinuation(Task* pParent, Task*& pBypassTask, SubIdType localId)
{
    GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::MICRO_SCHEDULER_PROFILE, analysis::Color::AntiqueWhite, "L_SCHD HANDLE ANY CONTINUATIONS", this, pParent);
    GTS_UNREFERENCED_PARAM(localId);

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

    GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::MICRO_SCHEDULER_DEBUG, analysis::Color::AntiqueWhite, "L_SCHD PARENT IS CONTINUATION", this, pParent);
    GTS_MS_COUNTER_INC(m_id, analysis::MicroSchedulerCounters::NUM_CONTINUATIONS);


    if (pBypassTask == nullptr)
    {
        GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::MICRO_SCHEDULER_DEBUG, analysis::Color::AntiqueWhite, "L_SCHD PARENT IS NEXT", this, pParent);

        // If there is no bypass task, the continuation becomes the next task.
        pBypassTask = pParent;
    }
    else
    {
        GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::MICRO_SCHEDULER_DEBUG, analysis::Color::AntiqueWhite, "L_SCHD PARENT QUEUED", this, pParent);

        // Otherwise queue the continuation.
        spawnTask(pParent, 0);
    }
}

//------------------------------------------------------------------------------
Task* LocalScheduler::_getNonLocalTask(Worker* pThisWorker, bool getAffinity, bool callerIsExternal, SubIdType localId, bool& executedWork)
{
    Task* pTask = _getQueuedTask(localId);

    if(!pTask && getAffinity)
    {
        pTask = _getAffinityTask(localId);
        if (pTask)
        {
            // Reset because the submission of an affinitized task
            // does not reset the semaphore.
            pThisWorker->resetSleepState();
        }
    }

    if (!pTask)
    {
        pTask = _stealTask(localId, callerIsExternal);
    }

    if (!pTask)
    {

        auto& callbacks = m_pMyScheduler->m_pCallbacks[(size_t)MicroSchedulerCallbackType::CHECK_FOR_TASK];
        auto& callbacksVec = callbacks.m_callbacks;

        {
            // Acquire reference to the callback so they cannot be unregistered while calling them.
            LockShared<MicroScheduler::Callbacks::rw_lock_type> lock(callbacks.m_mutex);

            // TODO: make starvation resistant. Round-robin?
            for (size_t ii = 0; ii < callbacksVec.size(); ++ii)
            {
                pTask = ((OnCheckForTasksFcn)(callbacksVec[ii].func))(callbacksVec[ii].pData, m_pMyScheduler, m_id, executedWork);
                if (pTask)
                {
                    break;
                }
            }
        }
    }

    return pTask;
}

//------------------------------------------------------------------------------
Task* LocalScheduler::_getNonLocalTaskLoop(Task* pWaitingTask, Worker* pThisWorker, SubIdType localId, bool& executedTask)
{
    GTS_SIM_TRACE_MARKER(sim_trace::MARKER_NONLOCAL_TASKLOOP_BEGIN);
    GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::MICRO_SCHEDULER_PROFILE, analysis::Color::Orange2, "L_SCHD NON-LOCAL TASK LOOP", this, 0);

    // Mark this Worker as a top level Worker. Top level workers can quit
    // without pWaitingTask being complete, and they can steal externally.
    const bool isTopLevelWorker = pWaitingTask == m_pWaiterTask;

    Task* pTask = nullptr;

    // GET NON-LOCAL TASK LOOP
    while (m_pMyScheduler->m_isAttached.load(memory_order::relaxed))
    {
        // ----------------------------------------------------
        // Get a task from a non-local queue

        if (!pTask)
        {
            pTask = _getNonLocalTask(pThisWorker, true, false, localId, executedTask);
        }

        // ---------------------------------

        if (pTask)
        {
            // Exit non-local loop.
            break;
        }
        else // No work found, try to exit
        {
            GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::MICRO_SCHEDULER_PROFILE, analysis::Color::Red, "L_SCHD TRY EXIT", this, 0);
            GTS_MS_COUNTER_INC(m_id, analysis::MicroSchedulerCounters::NUM_EXIT_ATTEMPTS);

            // If this is a top level worker, try to quit.
            if (isTopLevelWorker)
            {
                // Try to skip expensive _stealExternalTask.
                if (m_id.localId() != 0 && !m_pMyScheduler->m_pExternalSchedulers->hasVictims())
                {
                    GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::MICRO_SCHEDULER_PROFILE, analysis::Color::AntiqueWhite, "L_SCHD SEARCH FOR EXTERNAL TASK", this, 0);

                    // ^^^^^^^^^^^^^^^^^^
                    // Empty check race seems OK. At worst miss a newly
                    // added victim or we step into _stealExternalTask and
                    // learn there are no victims.

                    // Since this scheduler has no demand,
                    // try to steal a task from a victim scheduler.
                    pTask = _stealExternalTask(localId);
                    if (pTask != nullptr)
                    {
                        GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::MICRO_SCHEDULER_PROFILE, analysis::Color::AntiqueWhite, "L_SCHD FOUND EXTERN TASK", this, pTask);
                        // Exit non-local loop.
                        break;
                    }
                }

                GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::MICRO_SCHEDULER_PROFILE, analysis::Color::DarkRed, "L_SCHD VERIFY NO WORK", this, 0);

#if GTS_USE_PRODUCTIVE_EMPTY_CHECKS
                pTask = _getNonLocalTask(pThisWorker, true, false, localId, executedTask);
                if(pTask)
                {
                    break;
                }
                else
#else
                if (!(m_pMyScheduler->_hasDequeTasks() || hasAffinityTasks()))
#endif // GTS_USE_PRODUCTIVE_EMPTY_CHECKS
                {
                    // No tasks. Quit.
                    GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::MICRO_SCHEDULER_PROFILE, analysis::Color::AntiqueWhite, "L_SCHD LOCAL SCHEDULER EXIT", this, 0);
                    GTS_MS_COUNTER_INC(m_id, analysis::MicroSchedulerCounters::NUM_EXITS);
                    break;
                }
            }
        }

        // Are we done?
        if (!pWaitingTask)
        {
            // Stop blocking the caller.
            GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::MICRO_SCHEDULER_DEBUG, analysis::Color::AntiqueWhite, "L_SCHD WAITER DONE", this, pTask);
            break;
        }
        if (pWaitingTask->refCount() <= 2)
        {
            GTS_ASSERT(pWaitingTask->refCount() == 2);
            pWaitingTask->setRef(1, memory_order::relaxed);

            // Stop blocking the caller.
            GTS_TRACE_ZONE_MARKER_P1(analysis::CaptureMask::MICRO_SCHEDULER_DEBUG, analysis::Color::AntiqueWhite, "L_SCHD WAITER DONE", this);
            break;
        }
    }

    GTS_SIM_TRACE_MARKER(sim_trace::MARKER_NONLOCAL_TASKLOOP_END);
    return pTask;
}

//------------------------------------------------------------------------------
bool LocalScheduler::runUntilDone(Task* pWaitingTask, Task* pChild)
{
    GTS_ASSERT(pWaitingTask ? pWaitingTask->refCount() >= (pChild && pChild->parent() == pWaitingTask ? 3 : 2) : true &&
        "pWaitingTask.refCount() is too small");

    GTS_ASSERT(m_id.uid() != UNKNOWN_UID);

    bool executedTask       = false;
    Task* pTask             = pChild;
    const SubIdType localId = m_id.localId();
    Worker* pThisWorker     = (Worker*)Worker::getLocalState();

    GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::MICRO_SCHEDULER_PROFILE, analysis::Color::AntiqueWhite, "L_SCHD WORKER-LOOP", this, 0);

    // WORKER-LOOP
    while (m_pMyScheduler->m_isAttached.load(memory_order::relaxed))
    {
        {
            GTS_SIM_TRACE_MARKER(sim_trace::MARKER_LOCAL_TASKLOOP_BEGIN);
            GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::MICRO_SCHEDULER_PROFILE, analysis::Color::LightGreen, "L_SCHD LOCAL TASK LOOP", this, 0);

            // GET LOCAL TASK LOOP
            while(m_pMyScheduler->m_isAttached.load(memory_order::relaxed))
            {
                GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::MICRO_SCHEDULER_PROFILE, analysis::Color::Green1, "L_SCHD TASK EXECUTION LOOP", this, 0);

                // TASK EXECUTION LOOP
                while (pTask)
                {
                    // ----------------------------------------------------
                    // Execute this task then any continuations or bypasses.

                    Task* pByPassTask = nullptr;
                    {
                        // Execute!
                        GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::MICRO_SCHEDULER_PROFILE, analysis::Color::Cyan, "L_SCHD EXECUTE TASK", this, pTask);
                        GTS_MS_COUNTER_INC(m_id, analysis::MicroSchedulerCounters::NUM_EXECUTED_TASKS);
                        pTask->header().pMyLocalScheduler = this;
                        pTask->header().executionState = internal::TaskHeader::EXECUTING;
                        pByPassTask = pTask->execute(TaskContext{ m_pMyScheduler, m_id, pTask, pThisWorker->m_pUserData });
                        executedTask = true;
                    }

                    GTS_ASSERT((!pByPassTask || (pByPassTask && !(pByPassTask->header().flags & internal::TaskHeader::TASK_IS_CONTINUATION))) &&
                        "A bypass task cannot be a continuation.");

                    Task* pParent = nullptr;

                    switch (pTask->header().executionState)
                    {
                    case internal::TaskHeader::EXECUTING:

                        GTS_SIM_TRACE_MARKER(sim_trace::MARKER_CLEANUP_TASK_BEGIN);

                        pTask->~Task();

                        pParent = pTask->parent();
                        if(pParent != nullptr)
                        {
                            _handleContinuation(pParent, pByPassTask, localId);
                        }

                        GTS_ASSERT(pTask->refCount() <= 1 && "Task still has children after executing.");
                        GTS_ASSERT(pTask != pWaitingTask && "Cannot execute the waiting Task.");

                        // NOTE: allocation id is associated with Workers not Schedules.
                        m_pMyScheduler->_freeTask(pTask); 

                        GTS_SIM_TRACE_MARKER(sim_trace::MARKER_CLEANUP_TASK_END);

                        break;

                    case internal::TaskHeader::ALLOCATED:
                        if(!pByPassTask && (pTask->header().flags & internal::TaskHeader::TASK_IS_CONTINUATION) == 0)
                        {
                            // Spawn any recycled Task that is not a bypass or continuation Task.
                            spawnTask(pTask, 0);
                        }
                        break;
                    }

                    pTask = pByPassTask;

                    --m_proirityBoostAge;

                } // END TASK EXECUTION LOOP

                // ----------------------------------------------------
                // Get a task from the local queue

                // Are we done?
                if (pWaitingTask && pWaitingTask->refCount() <= 2)
                {
                    GTS_ASSERT(pWaitingTask->refCount() == 2);
                    pWaitingTask->setRef(1, memory_order::relaxed);

                    // Stop blocking the caller.
                    GTS_SIM_TRACE_MARKER(sim_trace::MARKER_LOCAL_TASKLOOP_END);
                    GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::MICRO_SCHEDULER_DEBUG, analysis::Color::AntiqueWhite, "L_SCHD WAITER DONE", this, pTask);
                    return executedTask;
                }

                pTask = _getLocalTask(localId);

                if (!pTask)
                {
                    break;
                }

            } // END GET LOCAL TASK LOOP

            GTS_SIM_TRACE_MARKER(sim_trace::MARKER_LOCAL_TASKLOOP_END);
        }

        pTask = _getNonLocalTaskLoop(pWaitingTask, pThisWorker, localId, executedTask);
        if (!pTask)
        {
            break;
        }

        // Make sure any parent still references this task, otherwise it
        // was orphaned from the DAG, which cannot happen.
        GTS_ASSERT((
            (pTask && pTask->parent() != nullptr)
            ? (pTask->parent()->refCount() > 1 && "pTask is orphaned from its parent.")
            : 1));

    } // END WORKER-LOOP

    return executedTask;
}

//------------------------------------------------------------------------------
Task* LocalScheduler::_getLocalTask(SubIdType localId)
{
    GTS_SIM_TRACE_MARKER(sim_trace::MARKER_TAKE_TASK_BEGIN);

    GTS_UNREFERENCED_PARAM(localId);

    GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::MICRO_SCHEDULER_PROFILE, analysis::Color::Orange2, "L_SCHD TRY GET LOCAL TASK", this, 0);

    Task* pTask = nullptr;
    size_t numPriorities = m_priorityTaskDeque.size();

    if (numPriorities > 1 && m_proirityBoostAge <= 0)
    {
        // It's time to boost a priority so that we don't starve a lower priority Task.
        return _getLocalBoostedTask(localId);
    }
    else
    {
        // Get the next task in normal priority order.
        for (size_t priority = 0; priority < numPriorities; ++priority)
        {
            GTS_MS_COUNTER_INC(m_id, analysis::MicroSchedulerCounters::NUM_DEQUE_POP_ATTEMPTS);
            TaskDeque& deque = m_priorityTaskDeque[priority];
            if (deque.tryPop(pTask))
            {
                GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::MICRO_SCHEDULER_PROFILE, analysis::Color::Blue, "L_SCHD GOT LOCAL TASK", this, pTask);
                GTS_MS_COUNTER_INC(m_id, analysis::MicroSchedulerCounters::NUM_DEQUE_POP_SUCCESSES);
                return pTask;
            }
        }
    }

    GTS_SIM_TRACE_MARKER(sim_trace::MARKER_TAKE_TASK_END);
    return nullptr;
}

//------------------------------------------------------------------------------
Task* LocalScheduler::_getLocalBoostedTask(SubIdType localId)
{
    GTS_UNREFERENCED_PARAM(localId);

    GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::MICRO_SCHEDULER_PROFILE, analysis::Color::Orange2, "L_SCHD TRY GET LOCAL BOOSTED TASK", this, 0);

    Task* pTask = nullptr;
    size_t numPriorities = m_priorityTaskDeque.size();

    // Get the next priority.
    size_t startPriority = m_currentProirityToBoost;
    m_currentProirityToBoost = (int16_t)((uint32_t)m_currentProirityToBoost % ((uint32_t)numPriorities - 1)) + 1; // [1, desc.numPriorities]

    // Reset the age counter.
    m_proirityBoostAge = m_proirityBoostAgeStart;

    // Wrap through all the priorities, starting with 'startPriority', looking for the next task.
    size_t priority = startPriority;
    for (size_t ii = 0; ii < numPriorities - 1; ++ii)
    {
        GTS_MS_COUNTER_INC(m_id, analysis::MicroSchedulerCounters::NUM_BOOSTED_DEQUE_POP_ATTEMPTS);
        priority = (size_t)((int32_t)priority % ((int32_t)numPriorities - 1)) + 1;

        TaskDeque& deque = m_priorityTaskDeque[priority];
        if (deque.tryPop(pTask))
        {
            GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::MICRO_SCHEDULER_PROFILE, analysis::Color::Blue, "L_SCHD GOT LOCAL BOOSTED TASK", this, pTask);
            GTS_MS_COUNTER_INC(m_id, analysis::MicroSchedulerCounters::NUM_BOOSTED_DEQUE_POP_SUCCESSES);
            return pTask;
        }
    }
    return nullptr;
}

//------------------------------------------------------------------------------
Task* LocalScheduler::_stealTask(SubIdType localId, bool callerIsExternal)
{
    GTS_SIM_TRACE_MARKER(sim_trace::MARKER_STEAL_TASK_BEGIN);

    GTS_UNREFERENCED_PARAM(localId);

    GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::MICRO_SCHEDULER_PROFILE, analysis::Color::Orange2, "L_SCHD TRY STEAL TASK", this, 0);

    const uint32_t localSchedulerCount = m_pMyScheduler->m_localSchedulerCount;

    if (!callerIsExternal && localSchedulerCount == 1)
    {
        GTS_SIM_TRACE_MARKER(sim_trace::MARKER_STEAL_TASK_END);
        return nullptr;
    }

    LocalScheduler** GTS_NOT_ALIASED ppVictims = m_pMyScheduler->m_ppLocalSchedulersByIdx;

    Task* pTask = nullptr;

#if 0

    uint32_t r = fastRand(m_randState) % (localSchedulerCount);
    if(r >= localSchedulerCount)
    {
        ++r; // avoid stealing from self.
    }

    PriorityTaskDeque& victimDeque = ppVictims[r]->m_priorityTaskDeque;
    const size_t numPriorities     = m_priorityTaskDeque.size();

    for (size_t priority = 0; priority < numPriorities; ++priority)
    {
        GTS_MS_COUNTER_INC(m_id, analysis::MicroSchedulerCounters::NUM_DEQUE_STEAL_ATTEMPTS);

        TaskDeque& deque = victimDeque[priority];
        if (deque.trySteal(pTask))
        {
            GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::MICRO_SCHEDULER_PROFILE, analysis::Color::Blue, "L_SCHD STOLE TASK", this, pTask);
            GTS_MS_COUNTER_INC(m_id, analysis::MicroSchedulerCounters::NUM_DEQUE_STEAL_SUCCESSES);
            pTask->header().flags |= internal::TaskHeader::TASK_IS_STOLEN;
            break;
        }
    }

#else

    uint32_t r = fastRand(m_randState) % (localSchedulerCount);
    pTask = _stealTaskLoop(localId, ppVictims, r, localSchedulerCount);
    if(!pTask)
    {
        pTask = _stealTaskLoop(localId, ppVictims, 0, r);
    }

#endif

    GTS_SIM_TRACE_MARKER(sim_trace::MARKER_STEAL_TASK_END);
    return pTask;
}

//------------------------------------------------------------------------------
Task* LocalScheduler::_stealTaskLoop(SubIdType localId, LocalScheduler** GTS_NOT_ALIASED ppVictims, uint32_t begin, uint32_t end)
{
    GTS_UNREFERENCED_PARAM(localId);

    Task* pTask = nullptr;
    for(uint32_t ii = begin; ii < end; ++ii)
    {
        PriorityTaskDeque& GTS_NOT_ALIASED victimDeque = ppVictims[ii]->m_priorityTaskDeque;
        const size_t numPriorities = victimDeque.size();

        for (size_t priority = 0; priority < numPriorities; ++priority)
        {
            GTS_MS_COUNTER_INC(m_id, analysis::MicroSchedulerCounters::NUM_DEQUE_STEAL_ATTEMPTS);

            TaskDeque& deque = victimDeque[priority];
            if (deque.trySteal(pTask))
            {
                GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::MICRO_SCHEDULER_PROFILE, analysis::Color::Blue, "L_SCHD STOLE TASK", this, pTask);
                GTS_MS_COUNTER_INC(m_id, analysis::MicroSchedulerCounters::NUM_DEQUE_STEAL_SUCCESSES);
                pTask->header().flags |= internal::TaskHeader::TASK_IS_STOLEN;
                return pTask;
            }
            GTS_SPECULATION_FENCE();
        }
    }

    return nullptr;
}

//------------------------------------------------------------------------------
Task* LocalScheduler::_getQueuedTask(SubIdType localId)
{
    GTS_UNREFERENCED_PARAM(localId);

    GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::MICRO_SCHEDULER_PROFILE, analysis::Color::Orange2, "L_SCHD TRY GET QUEUED TASK", this, 0);
    GTS_MS_COUNTER_INC(m_id, analysis::MicroSchedulerCounters::NUM_QUEUE_POP_ATTEMPTS);

    Task* pTask = nullptr;

    for (size_t priority = 0; priority < m_pPriorityTaskQueue->size(); ++priority)
    {
        TaskQueue& queue = (*m_pPriorityTaskQueue)[priority];
        if (!queue.empty())
        {
            if(queue.tryPop(pTask))
            {
                GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::MICRO_SCHEDULER_PROFILE, analysis::Color::Blue, "L_SCHD GOT QUEUED TASK", this, pTask);
                GTS_MS_COUNTER_INC(m_id, analysis::MicroSchedulerCounters::NUM_QUEUE_POP_SUCCESSES);
                return pTask;
            }
        }
    }
    return nullptr;
}

//------------------------------------------------------------------------------
Task* LocalScheduler::_getAffinityTask(SubIdType localId)
{
    GTS_UNREFERENCED_PARAM(localId);

    GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::MICRO_SCHEDULER_PROFILE, analysis::Color::Orange2, "L_SCHD TRY GET AFFINITY TASK", this, 0);

    Task* pTask = nullptr;

    for (size_t priority = 0; priority < m_affinityTaskQueue.size(); ++priority)
    {
        GTS_MS_COUNTER_INC(m_id, analysis::MicroSchedulerCounters::NUM_AFFINITY_POP_ATTEMPTS);
        AffinityTaskQueue& queue = m_affinityTaskQueue[priority];
        if (!queue.empty() && queue.tryPop(pTask))
        {
            GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::MICRO_SCHEDULER_PROFILE, analysis::Color::Blue, "L_SCHD GOT AFFINITY TASK", this, pTask);
            GTS_MS_COUNTER_INC(m_id, analysis::MicroSchedulerCounters::NUM_AFFINITY_POP_SUCCESSES);
            return pTask;
        }
    }
    return nullptr;
}

//------------------------------------------------------------------------------
Task* LocalScheduler::_stealExternalTask(SubIdType localId)
{
    GTS_UNREFERENCED_PARAM(localId);

    GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::MICRO_SCHEDULER_PROFILE, analysis::Color::Orange2, "L_SCHD TRY STEAL EXTERN TASK", this, 0);
    GTS_MS_COUNTER_INC(m_id, analysis::MicroSchedulerCounters::NUM_EXTERNAL_STEAL_ATTEMPTS);

    Task* pTask                = nullptr;
    MicroScheduler* pScheduler = nullptr;
    uint32_t iScheduler        = 0;
    while (true)
    {
        {
            Lock<MicroScheduler::MutexType> lock(m_pMyScheduler->m_pExternalSchedulers->m_mutex);
            if (iScheduler < m_pMyScheduler->m_pExternalSchedulers->m_victims.size())
            {
                pScheduler = m_pMyScheduler->m_pExternalSchedulers->m_victims[iScheduler];
                pScheduler->m_pExternalSchedulers->m_thiefAccessCount.fetch_add(1, memory_order::acquire);
            }
            else
            {
                return nullptr;
            }
        }

        LocalScheduler** GTS_NOT_ALIASED ppVictims = pScheduler->m_ppLocalSchedulersByIdx;
        uint32_t localSchedulerCount = pScheduler->m_localSchedulerCount;
        uint32_t r = fastRand(m_randState) % (localSchedulerCount);


        pTask = _stealTaskLoop(localId, ppVictims, r, localSchedulerCount);
        if(!pTask)
        {
            pTask = _stealTaskLoop(localId, ppVictims, 0, r);
        }

        pScheduler->m_pExternalSchedulers->m_thiefAccessCount.fetch_sub(1, memory_order::release);

        if (pTask)
        {
            GTS_MS_COUNTER_INC(m_id, analysis::MicroSchedulerCounters::NUM_EXTERNAL_STEAL_SUCCESSES);
            return pTask;
        }

        ++iScheduler;

        GTS_SPECULATION_FENCE();
    }
}

} // namespace gts
