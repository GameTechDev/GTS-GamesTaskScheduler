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
#include "Schedule.h"

#include "gts/containers/parallel/DistributedSlabAllocator.h"
#include "gts/platform/Assert.h"
#include "gts/analysis/Instrumenter.h"
#include "gts/analysis/Analyzer.h"

#include "gts/micro_scheduler/MicroScheduler.h"
#include "gts/micro_scheduler/WorkerPool.h"
#include "Worker.h"

#include "Backoff.h"

namespace gts {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Schedule:

// STRUCTORS:

//------------------------------------------------------------------------------
Schedule::Schedule()
    : m_pTaskQueue(nullptr)
    , m_pAffinityTaskQueue(nullptr)
    , m_pTaskAllocator(nullptr)
    , m_pMyScheduler(nullptr)
    , m_isolationTag(0)
    , m_proirityBoostAgeStart(INT16_MAX)
    , m_proirityBoostAge(INT16_MAX)
    , m_currentProirityToBoost(1)
{}

//------------------------------------------------------------------------------
Schedule::~Schedule()
{
    gts::alignedDelete(m_pAffinityTaskQueue);
}

// ACCESSORS:

//------------------------------------------------------------------------------
bool Schedule::hasTasks() const
{
    for (size_t priority = 0, pCount = m_priorityTaskDeque.size(); priority < pCount; ++priority)
    {
        if (!m_priorityTaskDeque[priority].empty())
        {
            return true;
        }
    }

    if (!m_pAffinityTaskQueue->empty())
    {
        return true;
    }

    return false;
}

// MUTATORS:

//------------------------------------------------------------------------------
void Schedule::initialize(
    uint32_t scheduleIdx,
    MicroScheduler* pScheduler,
    uint32_t priorityCount,
    int16_t proirityBoostAge)
{
    GTS_INSTRUMENTER_SCOPED(analysis::Tag::INTERNAL, "Schedule Init", scheduleIdx, 0);

    m_pTaskQueue        = pScheduler->m_pTaskQueue;
    m_pTaskAllocator    = pScheduler->m_pTaskAllocator;
    m_pMyScheduler      = pScheduler;

    m_index     = scheduleIdx;
    m_randState = scheduleIdx + 1; // can't be zero.

    m_proirityBoostAgeStart = proirityBoostAge;
    m_proirityBoostAge = m_proirityBoostAgeStart - 1;

    // Initialize the task deques.
    m_priorityTaskDeque.resize(priorityCount);

    m_pAffinityTaskQueue = gts::alignedNew<AffinityTaskQueue, GTS_NO_SHARING_CACHE_LINE_SIZE>();
}

//------------------------------------------------------------------------------
bool Schedule::spawnTask(Task* pTask, uint32_t priority)
{
    return m_priorityTaskDeque[priority].tryPush(pTask);
}

//------------------------------------------------------------------------------
bool Schedule::queueAffinityTask(Task* pTask)
{
    return m_pAffinityTaskQueue->tryPush(pTask);
}

//------------------------------------------------------------------------------
void Schedule::setVictimPool(Vector<Schedule*> const& victimPool)
{
    m_victimPool = victimPool;
}

// SCHEDULING:

//------------------------------------------------------------------------------
void Schedule::run(Backoff& backoff)
{
    Task waitForever;
    waitForever.m_refCount.store(2, gts::memory_order::relaxed); // ref count of 2 -> wait forever.

    wait(&waitForever, nullptr, backoff);
}

//------------------------------------------------------------------------------
void Schedule::wait(Task* pWaitingTask, Task* pStartTask, Backoff& backoff)
{
    GTS_ASSERT(m_index != UNKNOWN_WORKER_IDX);

    Task* pTask = pStartTask;
    const uint32_t workerIdx = m_index;

    // WORKER-LOOP
    while (m_pMyScheduler->m_isRunning.load(gts::memory_order::relaxed))
    {
        // GET LOCAL TASK LOOP
        while(m_pMyScheduler->m_isRunning.load(gts::memory_order::relaxed))
        {
            // TASK EXECUTION LOOP
            while (pTask)
            {
                // ----------------------------------------------------
                // Execute this task then any continuations or bypasses.

                Task* pNextTask = nullptr;

                GTS_ANALYSIS_COUNT(workerIdx, gts::analysis::AnalysisType::NUM_EXECUTED_TASKS);
                GTS_INSTRUMENTER_MARKER(analysis::Tag::INTERNAL, "EXECUTE TASK", pTask, 0);

                // Execute!
                pTask->m_state |= Task::TASK_IS_EXECUTING;
                pTask->m_state &= ~Task::TASK_IS_QUEUED;
                pNextTask = pTask->execute(pTask, TaskContext{ m_pMyScheduler, workerIdx });

                GTS_ASSERT((!pNextTask || (pNextTask && !(pNextTask->m_state & Task::TASK_IS_CONTINUATION))) &&
                    "A bypass task cannot be a continuation.");

                if (pTask->m_state & Task::RECYLE)
                {
                    // Use the task that was recyled!
                    pTask->m_state &= ~Task::RECYLE; // don't recycle again.
                    GTS_INSTRUMENTER_MARKER(analysis::Tag::INTERNAL, "RECYCLING TASK", pTask, 0);
                    continue;
                }

                // Check if the task's dependents are done
                if (pTask->refCount() > 1)
                {
                    GTS_INSTRUMENTER_MARKER(analysis::Tag::INTERNAL, "IMPLICIT WAIT START", pTask, 0);

                    // If not, implicitly wait.
                    wait(pTask, nullptr, backoff);

                    GTS_INSTRUMENTER_MARKER(analysis::Tag::INTERNAL, "IMPLICIT WAIT END", pTask, 0);
                }

                // Check for ready continuations.
                _handleContinuation(pTask, pNextTask);

                // Free this task.
                m_pMyScheduler->_freeTask(workerIdx, pTask);

                pTask = pNextTask;

                --m_proirityBoostAge;

            } // END TASK EXECUTION LOOP

            // ----------------------------------------------------
            // Get a task from the local queue

            // Are we done?
            if (pWaitingTask->refCount() <= 1)
            {
                // Stop blocking the caller.
                return;
            }

            pTask = _getLocalTask(workerIdx);

            if (!pTask)
            {
                break;
            }

        } // END GET LOCAL TASK LOOP

        // GET NON-LOCAL TASK LOOP
        while (m_pMyScheduler->m_isRunning.load(gts::memory_order::relaxed))
        {
            // Are we done?
            if (pWaitingTask->refCount() <= 1)
            {
                // Stop blocking the caller.
                return;
            }

            // ----------------------------------------------------
            // Get a task from a non-local queue

            pTask = _getNonWorkerTask(workerIdx);

            if (!pTask)
            {
                pTask = _getAffinityTask(workerIdx);
            }

            if (!pTask)
            {
                pTask = _stealTask(workerIdx);
            }

            // ---------------------------------
            // No work found so back off.

            if (!pTask)
            {
                // Back off threshold reached.
                if (backoff.tick(workerIdx))
                {
                    GTS_ANALYSIS_COUNT(workerIdx, gts::analysis::AnalysisType::NUM_SLEEP_ATTEMPTS);
                    GTS_ANALYSIS_TIME_SCOPED(workerIdx, gts::analysis::AnalysisType::NUM_SLEEP_ATTEMPTS);

                    // Stop scheduling if their are no tasks in the Schedule.
                    if (!m_pMyScheduler->hasTasks())
                    {
                        return;
                    }

                    backoff.resetYield();
                }
            }
            else
            {
                if (pTask && pTask->m_pParent != nullptr)
                {
                    GTS_ASSERT(pTask->m_pParent->refCount() > 1 && "pTask is orphaned from its parent.");
                }

                backoff.reset();
                break;
            }
        } // END GET NON-LOCAL TASK LOOP

    } // END WORKER-LOOP
}

//------------------------------------------------------------------------------
void Schedule::_handleContinuation(Task* pTask, Task*& pNextTask)
{
    // If the task has a parent,
    Task* pParent = pTask->m_pParent;
    if (pParent == nullptr)
    {
        GTS_INSTRUMENTER_MARKER(analysis::Tag::INTERNAL, "DONE NO PARENT", pTask, pParent);
        return;
    }

    // If the parent is a waiting dummy task,
    if ((pParent->m_state & Task::TASK_IS_WAITING) != 0)
    {
        GTS_INSTRUMENTER_MARKER(analysis::Tag::INTERNAL, "DONE WAITING TASK", pTask, pParent);

        // mark waitng as over.
        pParent->m_refCount.store(0, gts::memory_order::relaxed);
        return;
    }

    // If the parent is a ready continuation,
    if (
        // No Race Case: T0 observes parent.refcount > 2 and completes removeRef(1) before T1
        // observes parent.refcount.In this case T1 cannot race and it will see
        // parent.refcount == 2, short circuiting the atomic.
        pParent->refCount() == 2 ||
        // Race Case: T0 and T1 can both observe parent.refcount > 2, so they must use
        // removeRef(1) to not race. Since both threads took this path, the thread
        // that sees refcount == 1 is the winner.
        pParent->removeRef(1) == 1)
    {
        pParent->m_refCount.store(1, gts::memory_order::relaxed);

        GTS_INSTRUMENTER_MARKER(analysis::Tag::INTERNAL, "DONE PARENT COMPLETE", pTask, pParent);

        if (pParent->m_state & Task::TASK_IS_CONTINUATION)
        {
            GTS_INSTRUMENTER_MARKER(analysis::Tag::INTERNAL, "DONE PARENT IS CONT", pTask, pParent);

            if (pNextTask == nullptr)
            {
                GTS_INSTRUMENTER_MARKER(analysis::Tag::INTERNAL, "DONE PARENT IS NEXT", pTask, pParent);

                // If there is no bypass task, the continuation becomes the next task.
                pNextTask = pParent;
            }
            else
            {
                GTS_INSTRUMENTER_MARKER(analysis::Tag::INTERNAL, "DONE PARENT QUEUED", pTask, pParent);

                // Otherwise queue the continuation.
                spawnTask(pParent, 0);
            }
        }
    }
}

//------------------------------------------------------------------------------
Task* Schedule::_getLocalTask(uint32_t workerIdx)
{
    GTS_UNREFERENCED_PARAM(workerIdx);

    GTS_ANALYSIS_COUNT(workerIdx, gts::analysis::AnalysisType::NUM_TAKE_ATTEMPTS);
    GTS_ANALYSIS_TIME_SCOPED(workerIdx, gts::analysis::AnalysisType::NUM_TAKE_ATTEMPTS);

    Task* pTask = nullptr;
    size_t numPriorities = m_priorityTaskDeque.size();

    if (numPriorities > 1 && m_proirityBoostAge <= 0)
    {
        // It's time to boost a priority so that we don't starve a lower priority Task.
        return _getLocalBoostedTask(workerIdx);
    }
    else
    {
        // Get the next task in normal priority order.
        for (size_t priority = 0; priority < numPriorities; ++priority)
        {
            TaskDeque& deque = m_priorityTaskDeque[priority];
            if (!deque.empty() && deque.tryPop(pTask, m_isolationTag))
            {
                GTS_ANALYSIS_COUNT(workerIdx, gts::analysis::AnalysisType::NUM_TAKE_SUCCESSES);
                GTS_INSTRUMENTER_MARKER(analysis::Tag::INTERNAL, "GOT LOCAL TASK", pTask, 0);
                return pTask;
            }
        }
    }

    return nullptr;
}

//------------------------------------------------------------------------------
Task* Schedule::_getLocalBoostedTask(uint32_t workerIdx)
{
    GTS_UNREFERENCED_PARAM(workerIdx);

    Task* pTask = nullptr;
    size_t numPriorities = m_priorityTaskDeque.size();

    // Get the next priority.
    size_t startPriority = m_currentProirityToBoost;
    m_currentProirityToBoost = (int16_t)gts::fastMod((uint32_t)m_currentProirityToBoost, (uint32_t)numPriorities - 1) + 1; // [1, desc.numPriorities]

    // Reset the age counter.
    m_proirityBoostAge = m_proirityBoostAgeStart;

    // Wrap through all the priorities, starting with 'startPriority', looking for the next task.
    size_t priority = startPriority;
    for (size_t ii = 0; ii < numPriorities - 1; ++ii)
    {
        priority = gts::fastMod((uint32_t)priority, (uint32_t)numPriorities - 1) + 1;

        TaskDeque& deque = m_priorityTaskDeque[priority];
        if (!deque.empty() && deque.tryPop(pTask, m_isolationTag))
        {
            GTS_ANALYSIS_COUNT(workerIdx, gts::analysis::AnalysisType::NUM_TAKE_SUCCESSES);
            GTS_INSTRUMENTER_MARKER(analysis::Tag::INTERNAL, "GOT LOCAL TASK", pTask, 0);
            return pTask;
        }
    }
    return nullptr;
}

//------------------------------------------------------------------------------
Task* Schedule::_stealTask(uint32_t workerIdx)
{
    GTS_UNREFERENCED_PARAM(workerIdx);

    GTS_ANALYSIS_COUNT(workerIdx, gts::analysis::AnalysisType::NUM_STEAL_ATTEMPTS);
    GTS_ANALYSIS_TIME_SCOPED(workerIdx, gts::analysis::AnalysisType::NUM_STEAL_ATTEMPTS);

    if (m_victimPool.empty())
    {
        return nullptr;
    }

    Task* pTask = nullptr;

    uint32_t r = gts::fastMod(gts::fastRand(m_randState), (uint32_t)m_victimPool.size());
    PriorityTaskDeque& victimDeque = m_victimPool[r]->m_priorityTaskDeque;

    for (size_t priority = 0; priority < m_priorityTaskDeque.size(); ++priority)
    {
        TaskDeque& deque = victimDeque[priority];
        if (!deque.empty() && deque.trySteal(pTask, m_isolationTag))
        {
            GTS_ANALYSIS_COUNT(workerIdx, gts::analysis::AnalysisType::NUM_STEAL_SUCCESSES);
            GTS_INSTRUMENTER_MARKER(analysis::Tag::INTERNAL, "STOLE TASK", pTask, 0);
            pTask->m_state |= Task::TASK_IS_STOLEN;
            return pTask;
        }
    }
    return nullptr;
}

//------------------------------------------------------------------------------
Task* Schedule::_getNonWorkerTask(uint32_t workerIdx)
{
    GTS_UNREFERENCED_PARAM(workerIdx);

    GTS_ANALYSIS_COUNT(workerIdx, gts::analysis::AnalysisType::NUM_NONWORKER_POP_ATTEMPTS);
    GTS_ANALYSIS_TIME_SCOPED(workerIdx, gts::analysis::AnalysisType::NUM_NONWORKER_POP_ATTEMPTS);

    Task* pTask = nullptr;

    if (!m_pTaskQueue->empty() && m_pTaskQueue->tryPop(pTask, m_isolationTag))
    {
        GTS_ANALYSIS_COUNT(workerIdx, gts::analysis::AnalysisType::NUM_NONWORKER_POP_SUCCESSES);
        GTS_INSTRUMENTER_MARKER(analysis::Tag::INTERNAL, "GOT NON-WORKER TASK", pTask, 0);
        return pTask;
    }
    return nullptr;
}

//------------------------------------------------------------------------------
Task* Schedule::_getAffinityTask(uint32_t workerIdx)
{
    GTS_UNREFERENCED_PARAM(workerIdx);

    GTS_ANALYSIS_COUNT(workerIdx, gts::analysis::AnalysisType::NUM_AFFINITY_POP_ATTEMPTS);
    GTS_ANALYSIS_TIME_SCOPED(workerIdx, gts::analysis::AnalysisType::NUM_AFFINITY_POP_ATTEMPTS);

    Task* pTask = nullptr;

    if (!m_pAffinityTaskQueue->empty() && m_pAffinityTaskQueue->tryPop(pTask, m_isolationTag))
    {
        GTS_ANALYSIS_COUNT(workerIdx, gts::analysis::AnalysisType::NUM_AFFINITY_POP_SUCCESSES);
        GTS_INSTRUMENTER_MARKER(analysis::Tag::INTERNAL, "GOT AFFINITY TASK", pTask, 0);
        return pTask;
    }
    return nullptr;
}

} // namespace gts
