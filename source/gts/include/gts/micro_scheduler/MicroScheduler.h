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
#include "gts/synchronization/SpinMutex.h"

#include "gts/containers/AlignedAllocator.h"

#include "gts/micro_scheduler/Task.h"
#include "gts/micro_scheduler/MicroSchedulerTypes.h"

namespace gts {

/** 
 * @defgroup MicroScheduler
 * Fine-grained Task scheduling abstraction.
 * @{
 */

class TaskQueue;
class ScheduleVector;
class MicroSchedulerVector;
class LocalSchedulerBackoff;

class WorkerPool;
class MicroScheduler;

namespace internal {

#ifdef GTS_MSVC
#pragma warning(push)
#pragma warning(disable : 4324) // alignment padding warning
#endif

} // namespace internal

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A work-stealing task scheduler. The scheduler is executed by the WorkerPool
 *  it is initialized with.
 *
 * Supports:
 * -Help-first work-stealing
 * -Fork-join with nested parallelism
 * -Supports arbitrary DAGs
 * -Blocking joins
 * -Continuation joins
 * -Scheduler by-passing and task recycling optimizations
 * -Task affinities. (Force a task to run on a specific thread.)
 * -Task priorities with starvation resistance
 * -Scheduler partitioning
 * -Task execution isolation
 * -Hierarchical work-stealing via external victims.
 *
 * @todo Abstract into interface and make a concrete WorkStealingMicroScheduler. Interface will allow other algorithms to be explored.
 * @todo Explore making priorities global to the scheduler. Currently they are only local to each thread.
 */
class MicroScheduler
{
public: // CREATORS:

    /**
     * @brief
     *  Constructs a MicroScheduler in an uninitialized state. The user must call
     *  MicroScheduler::initialize to initialize the scheduler before use, otherwise
     *  calls to this MicroScheduler are undefined.
     */
    MicroScheduler();

    /**
     * @brief
     *  Implicitly shuts down the MicroScheduler if it's still running.
     */
    virtual ~MicroScheduler();

    // No copying.
    MicroScheduler(MicroScheduler const&) = delete;
    MicroScheduler& operator=(MicroScheduler const&) = delete;

public: // LIFETIME:

    /**
     * @brief
     *  Initializes the MicroScheduler and attaches it to \a pWorkPool, where each
     *  worker in pWorkPool will execute submitted tasks.
     * @param pWorkerPool
     *  The WorkerPool to attach to.
     * @return
     *   True if the initialization succeeded, false otherwise.
     */
    bool initialize(WorkerPool* pWorkerPool);

    /**
     * @brief
     *  Initializes the MicroScheduler for use. It creates the worker thread pool
     *  and allocates all memory to meet the requirements in \a desc.
     * @param desc
     *  The description of the MicroScheduler that describes how to initialize it.
     * @return
     *   True if the initialization succeeded, false otherwise.
     */
    bool initialize(MicroSchedulerDesc const& desc);

    /**
     * @brief
     *  Stops the MicroScheduler and destroys all resources. The TaskSchuduler 
     *  is now in an unusable state. All subsequent calls to this MicroScheduler
     *  are undefined.
     */
    void shutdown();

public: // MUTATORS:

    /**
     * @brief
     *  Allocates a new Task object of type TTask.
     * @param args
     *  The arguments for the TTask constructor.
     * @return
     *  The allocated Task or nullptr if the allocation failed.
     */
    template<typename TTask, typename... TArgs>
    GTS_INLINE TTask* allocateTask(TArgs&&... args)
    {
        return new (_allocateRawTask(sizeof(TTask))) TTask(std::forward<TArgs>(args)...);
    }

    /**
     * @brief
     *  Allocates a new Task object and emplaces the function/lambda data.
     * @param func
     *   A function that takes args as arguments.
     * @param args
     *  The arguments for func.
     * @return
     *  The allocated Task or nullptr if the allocation failed.
     */
    template<typename TFunc, typename... TArgs>
    GTS_INLINE Task* allocateTask(TFunc&& func, TArgs&&... args)
    {
        return new (_allocateRawTask(sizeof(LambdaTaskWrapper<TFunc, TArgs...>)))
            LambdaTaskWrapper<TFunc, TArgs...>(std::forward<TFunc>(func), std::forward<TArgs>(args)...);
    }

    /**
     * @brief
     *  Spawns the specified 'pTask' to be executed by the scheduler. Spawned
     *  tasks are executed in LIFO order, and stolen in FIFO order.
     * @param pTask
     *  The Task to spawn. It will be destroyed after execution refCount == 1.
     *  In the rare situation you want to keep the task alive, add an extra
     *  reference. Doing this will require you to call Task::destroy when you are
     *  done to avoid memory leaks.
     * @param priority
     *  The priority of the Task.
     */
    void spawnTask(Task* pTask, uint32_t priority = 0);

    /**
     * @brief
     *  Spawns the specified 'pTask' to be executed by the scheduler and then
     *  waits for its reference count to become one.
     *
     * There are two blocking behaviors:
     * 1. If spawnTaskAndWait is called from a worker thread, it will execute
     *    other tasks until pTask is complete.
     * 2. If spawnTaskAndWait is called from a non-worker thread, it will block
     *    until pTask is complete.
     *
     * @param pTask
     *  The Task to spawn.
     * @param priority
     *  The priority of the Task, if queued. Queuing occurs if this function is
     *  called on a non-worker thread or if this worker thread has a different
     *  affinity that the Task.
     */
    void spawnTaskAndWait(Task* pTask, uint32_t priority = 0);

    /**
     * @brief
     *  Waits for pTask's reference count to become one.
     *
     * There are two blocking behaviors:
     * 1. If waitFor is called from a worker thread, it will execute
     *    other tasks until pTask is complete.
     * 2. If waitFor is called from a non-worker thread, it will block
     *    until pTask is complete.
     *
     * @param pTask
     *  The Task to wait on. The Task must have an extra reference count
     *  for the wait before spawning, otherwise the MicroSchedule will
     *  destroy the task on completion. It is also up the the caller
     *  to destroy this task once the wait in completed.
     */
    void waitFor(Task* pTask);

    /**
     * @brief
     *  Waits for this MicroScheduler to have executed all it's tasks.
     *
     * There are two blocking behaviors:
     * 1. If waitFor is called from a worker thread, it will execute
     *    other tasks until complete.
     * 2. If waitFor is called from a non-worker thread, this function returns
     *    immediately.
     */
    void waitForAll();

    /**
     * @brief
     *  Manually destroy pTask. Undefined if this Task is executing.
     */
    void destoryTask(Task* pTask);

    /**
     * @brief
     *  Sets the active state of this MicroScheduler. An active MicroScheduler
     *  will be run by the WorkerPool and a inactive MicroScheduler will be
     *  ignored. By default, an initialized MicroScheduler is active.
     * @param isActive
     *  True to mark as active. False to mark as inactive.
     */
    GTS_INLINE void setActiveState(bool isActive)
    {
        m_isActive.store(isActive, memory_order::seq_cst);
    }

    /**
     * @brief
     *  Adds 'pScheduler' as an external victim. All Schedules in this MicroScheduler
     *  will be able to steal from pScheduler when they run out of work.
     * @remark
     *  This operation can be used to build a graph of MicroSchedulers. However,
     *  cycles will cause undefined behavior.
     * @param pScheduler
     *  The victim to add.
     */
    void addExternalVictim(MicroScheduler* pScheduler);

    /**
     * @brief
     *  Removes 'pScheduler' as an external victim.
     */
    void removeExternalVictim(MicroScheduler* pScheduler);

    /**
     * Registers a user callback function.
     */
    template<typename TFunc>
    void registerCallback(MicroSchedulerCallbackType type, TFunc callback, void* pUserData)
    {
        m_pCallbacks[(size_t)type].registerCallback((uintptr_t)callback, pUserData);
    }

    /**
     * Unregisters a user callback function.
     */
    template<typename TFunc>
    void unregisterCallback(MicroSchedulerCallbackType type, TFunc callback, void* pUserData)
    {
        m_pCallbacks[(size_t)type].unregisterCallback((uintptr_t)callback, pUserData);
    }

    /**
     * @brief
     *  Resets the ID generator to 0. This is not safe if there are any
     *  living MicroSchedulers.
     */
    static void resetIdGenerator();

public: // ACCESSORS:

    /**
     * @brief
     *  Checks if the scheduler is running.
     * @return True if the scheduler is running, false otherwise.
     */
    bool isRunning() const;

    /**
     * @brief
     *  Checks if the scheduler has tasks.
     * @return True if there are pending tasks, false otherwise.
     */
    bool hasTasks() const;

    /**
     * @brief
     *  Checks if the scheduler has external tasks.
     * @return True if there are pending tasks in external victims, false otherwise.
     */
    bool hasExternalTasks() const;

    /**
     * @brief
     *  Get the worker count of the WorkerPool this scheduler is attached to.
     * @return The number of Workers in the WorkerPool.
     */
    uint32_t workerCount() const;

    /**
     * @brief
     *  Get the worker the Worker ID of the calling thread.
     * @return The current Worker ID.
     */
    OwnedId thisWorkerId() const;

    /**
     * @brief
     *  Wake a sleeping Worker.
     */
    void wakeWorker();

    /**
     * @brief
     *  Checks if the scheduler is active.
     * @return True if the MicroScheduler is active, false otherwise.
     */ 
    GTS_INLINE bool isActive() const
    {
        return m_isActive.load(memory_order::acquire);
    }

    /**
     * @brief
     *  Get this schedulers ID.
     * @return The unique ID of the scheduler.
     */
    GTS_INLINE bool id() const
    {
        return m_schedulerId;
    }

private: // SCHEDULING:

    void* _allocateRawTask(uint32_t size);
    void _freeTask(Task* pTask);
    void _wait(Worker* pWorker, Task* pTask, Task* pChild);
    void _addTask(Worker* pWorker, Task* pTask, uint32_t priority);
    void _wakeWorkers(Worker* pThisWorker, uint32_t count, bool reset, bool wakeExternalVictims);

    void _registerWithWorkerPool(WorkerPool* pWorkerPool);
    void _unRegisterFromWorkerPool(WorkerPool* pWorkerPool, bool lockWorkerPool);
    void _unRegisterFromWorkers(WorkerPool* pWorkerPool);

    bool _hasDequeTasks() const;
    bool _hasAffinityTasks() const;
    bool _hasQueueTasks() const;

    Task* _getTask(Worker* pWorker, bool considerAffinity, bool callerIsExternal, SubIdType localId, bool& executedexecutedTaskWork);
    Task* _getExternalTask(Worker* pWorker, SubIdType localId, bool& executedTask);

private: // DATA:

    friend class Task;
    friend class Worker;
    friend class WorkerPool;
    friend class LocalScheduler;

    using PriorityTaskQueue = Vector<
        TaskQueue, AlignedAllocator<GTS_NO_SHARING_CACHE_LINE_SIZE>>;

    using BackoffType = Backoff<>;
    using MutexType = UnfairSpinMutex<BackoffType>;

    class GTS_ALIGN(GTS_NO_SHARING_CACHE_LINE_SIZE) ExternalSchedulers
    {
    public:
        void registerVictim(MicroScheduler* pVictim, MicroScheduler* pThief);
        void unregisterVictim(MicroScheduler* pVictim, MicroScheduler* pThief);

        void unregisterAllVictims(MicroScheduler* pThief);
        void unregisterAllThieves(MicroScheduler* pVictim);

        Task* getTask(Worker* pWorker, SubIdType localId, bool& executedTask);
        bool hasDequeTasks() const;
        bool hasVictims() const;
        void wakeThieves(Worker* pThisWorker, uint32_t count, bool reset);

        void addThief(MicroScheduler* pScheduler);
        void removeThief(MicroScheduler* pScheduler);
        void removeVictim(MicroScheduler* pScheduler);

        int32_t _locateVictim(MicroScheduler* pScheduler);
        int32_t _locateThief(MicroScheduler* pScheduler);

        Vector<MicroScheduler*> m_victims;
        Vector<MicroScheduler*> m_thieves;
        Atomic<uint16_t> m_thiefAccessCount = { 0 };
        MutexType m_mutex;
    };

    struct Callback
    {
        uintptr_t func;
        void* pData;
    };

    class GTS_ALIGN(GTS_NO_SHARING_CACHE_LINE_SIZE) Callbacks
    {
    public:
        void registerCallback(uintptr_t func, void* pData);
        void unregisterCallback(uintptr_t func, void* pData);
        Vector<Callback>& getCallbacks();
        MicroSchedulerCallbackType type() const { return m_type; }
        int32_t _locateCallback(uintptr_t func, void* pData);

        Vector<Callback> m_callbacks;
        MicroSchedulerCallbackType m_type;
        using rw_lock_type = UnfairSharedSpinMutex<>;
        rw_lock_type m_mutex;
    };

    PriorityTaskQueue* m_pPriorityTaskQueue;
    WorkerPool* m_pWorkerPool;
    Worker* m_pMyMaster;
    LocalScheduler** m_ppLocalSchedulersByIdx;
    ExternalSchedulers* m_pExternalSchedulers;
    Callbacks* m_pCallbacks;
    ThreadId m_creationThreadId;
    uint32_t m_localSchedulerCount;
    uint16_t m_schedulerId;
    Atomic<bool> m_isAttached;
    GTS_ALIGN(GTS_CACHE_LINE_SIZE) Atomic<bool> m_isActive;
    char m_debugName[DESC_NAME_SIZE];

    static Atomic<uint16_t> s_nextSchedulerId;
};


#ifdef GTS_MSVC
#pragma warning(pop)
#endif

/** @} */ // end of MicroScheduler

} // namespace gts
