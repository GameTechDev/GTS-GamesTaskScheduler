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

#include <fenv.h>

#include "gts/platform/Assert.h"
#include "gts/platform/Atomic.h"
#include "gts/platform/Thread.h"

#include "gts/micro_scheduler/Task.h"
#include "gts/micro_scheduler/MicroSchedulerTypes.h"

namespace gts {

class TaskDeque;
class TaskQueue;
class ScheduleVector;
class MicroSchedulerVector;

class WorkerPool;
class TaskAllocator;
class MicroScheduler;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A scheduler for fine grained tasks. It supports work-stealing through
 *  task spawning, and work-sharing through task queuing.
 */
class MicroScheduler
{
public: // CREATORS:

    /**
     * Creates a MicroScheduler in an uninitialized state. The user must call
     * MicroScheduler::initialize to initialize the scheduler before use, otherwise
     * calls to this MicroScheduler are undefined.
     */
    MicroScheduler();

    /**
     * Implicitly shuts down the MicroScheduler if it's still running.
     */
    ~MicroScheduler();

    // No copying.
    MicroScheduler(MicroScheduler const&) = delete;
    MicroScheduler& operator=(MicroScheduler const&) = delete;

public: // LIFETIME:

    /**
     * Initializes the MicroScheduler with 'pWorkPool', where each worker in
     * pWorkPool will execute submitted tasks.
     * @param pWorkerPool
     *  The WorkerPool that this MicroScheduler will run on.
     * @param priorityLevelCount
     *  The number of priority levels. Must be >= 1.
     * @return
     *   True if the initialization succeeded, false otherwise.
     */
    bool initialize(WorkerPool* pWorkerPool, uint32_t priorityLevelCount = 1);

    /**
    * Initializes the MicroScheduler for use. It creates the worker thread pool
    * and allocates all memory to meet the requirements in 'desc'.
    * @param desc
    *  The description of the MicroScheduler that describes how to initialize it.
    * @return
    *   True if the initialization succeeded, false otherwise.
    */
    bool initialize(MicroSchedulerDesc const& desc);

    /**
     * Stops the MicroScheduler and destroys all resources. The TaskSchuduler 
     * is now in an unusable state. All subsequent calls to this MicroScheduler
     * are undefined.
     */
    void shutdown();

    /**
     * Creates a partition of this MicroScheduler form a partitioned WorkerPool.
     * @param pParitionedPool
     *  The partitioned WorkerPool.
     * @param canStealFromThis
     *  If true, the partition can steal from from this MicroScheduler.
     * @returns The partitioned MicroScheduler, or nullptr if the partition failed.
     */
    MicroScheduler* makePartition(WorkerPool* pParitionedPool, bool canStealFromThis);

public: // MUTATORS:

    /**
     * Allocates a new Task object of the specified size.
     * @param taskRoutine
     *  The function the Task will execute.
     * @param dataSize
     *  The size of the Task data. Always rounds up to the next multiple of
     *  GTS_NO_SHARING_CACHE_LINE_SIZE.
     * @return
     *  The allocated Task or nullptr if the allocation failed.
     */
    GTS_INLINE Task* allocateTask(TaskRoutine taskRoutine, uint32_t dataSize = GTS_NO_SHARING_CACHE_LINE_SIZE)
    {
        Task* pTask = _allocateEmptyTask(taskRoutine, sizeof(Task) + dataSize);
        return pTask;
    }

    /**
     * Allocates a new Task object and emplaces the TTask data.
     * @tparam TTask
     *   The Task type. It must have a static member function:
     *   Task* taskFunc(Task* pThisTask, TaskContext const& taskContext);
     * @param args
     *  The constructor arguments for TTask.
     * @return
     *  The allocated Task or nullptr if the allocation failed.
     */
    template<typename TTask, typename... TArgs>
    GTS_INLINE Task* allocateTask(TArgs&&... args)
    {
        Task* pTask = _allocateEmptyTask(TTask::taskFunc, sizeof(Task) + sizeof(TTask));
        
        if (pTask != nullptr)
        {
            pTask->emplaceData<TTask>(std::forward<TArgs>(args)...);
        }
        return pTask;
    }

    /**
     * Spawns the specified 'pTask' to be executed by the scheduler. Spawned
     * tasks are executed in LIFO order, and stolen in FIFO order.
     * @param pTask
     *  The Task to execute.
     * @param priority
     *  The priority of the Task.
     */
    void spawnTask(Task* pTask, uint32_t priority = 0);

    /**
     * Spawns the specified 'pTask' to be executed by the scheduler and then
     * waits for it's reference count to become zero. Spawned tasks are executed
     * in LIFO order, and stolen in FIFO order.
     *
     * There are two behaviors:
     * (1) If spawnTaskAndWait is called from a worker thread, it will execute
     *     other tasks until all the tasks in the scheduler are complete.
     * (2) If spawnTaskAndWait is called from a non-worker thread, it will block
     *     until all the tasks in the scheduler are complete.
     *
     * @param pTask
     *  The Task to execute.
     * @param priority
     *  The priority of the Task.
     */
    void spawnTaskAndWait(Task* pTask, uint32_t priority = 0);

    /**
     * Queues the specified 'pTask' to be executed by the scheduler. Queued
     * tasks are executed in FIFO order, and may be more suitable for algorithms
     * that explore a solution space in bread-first order. It's generally faster
     * to spawn a task than it is to queue it.
     * @param pTask
     *  The Task to queue.
     */
    void queueTask(Task* pTask);

    /**
     * Runs any DAGs created by 'fcn' in isolation of other Tasks. That is,
     * if a thread begins executing an isolated Task, it can only execute
     * other Tasks from the same isolation group until complete.
     */
    template<typename TFunction>
    void isolate(TFunction fcn)
    {
        uint32_t* pIsolationTag = new uint32_t; // make unique address.

        if (UNKNOWN_WORKER_IDX == thisWorkerIndex())
        {
            GTS_ASSERT(0 && "Can't isolate an on an unknown thread.");
            return;
        }

        uintptr_t oldTag = _isolateSchedule((uintptr_t)pIsolationTag);
        fcn();
        _isolateSchedule(oldTag); // restore old tag.

        delete pIsolationTag;
    }

public: // ACCESSORS:

    /**
     * @return True if the scheduler is running, false otherwise.
     */
    bool isRunning() const;

    /**
     * @return True if there are pending tasks, false otherwise.
     */
    bool hasTasks() const;

    /**
     * @return The number of Workers in the WorkerPool.
     */
    uint32_t workerCount() const;

    /**
     * @return The current Worker thread index.
     */
    uint32_t thisWorkerIndex();

    /**
     * @return True if this MicroScheduler is partitioned from a master MicroScheduler.
     */
    bool isPartition() const;

private: // SCHEDULING:

    Task* _allocateEmptyTask(TaskRoutine taskRoutine, uint32_t size);
    void _freeTask(uint32_t workerIdx, Task* pTask);
    void _wait(Task* pTask, Task* pStartTask, uint32_t workerIndex);
    uintptr_t _isolateSchedule(uintptr_t isolationTag);
    static void _setupVictimPools(Schedule*& pDstSched, bool* pDstOwnership, Schedule* const pSrcSched, bool* pSrcOwnership, uint32_t scheduleCount);

private: // DATA:

    friend class Task;
    friend class Worker;
    friend class WorkerPool;
    friend class Schedule;

    TaskQueue* m_pTaskQueue;
    TaskAllocator* m_pTaskAllocator;
    WorkerPool* m_pWorkerPool;
    Schedule* m_pSchedulesByIdx;
    bool* m_pScheduleOwnershipByIdx;
    Vector<MicroScheduler*> m_partitions;
    MicroScheduler* m_pRootMicroScheduler;
    uint32_t m_scheduleCount;
    Atomic<bool> m_isRunning;
    bool m_canStealFromRoot;
};

} // namespace gts
