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

#include <functional>

#include "gts/containers/Vector.h"
#include "gts/platform//Utils.h"
#include "gts/platform/Thread.h"

#define GTS_USE_BACKOFF 1
#define GTS_USE_PRODUCTIVE_EMPTY_CHECKS 1

namespace gts {

/** 
 * @addtogroup MicroScheduler
 * @{
 */

class MicroScheduler;
class WorkerPool;
class Task;

//! The fixed size for descriptive names.
constexpr size_t DESC_NAME_SIZE = 64;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  The context associated with the task being executed.
 */
struct TaskContext
{
    /**
     * @brief
     *  The MicroScheduler executing the Task.
     */
    MicroScheduler* pMicroScheduler = nullptr;

    /**
     * @brief
     * The ID of the current Worker.
     */
    OwnedId workerId;

    /**
     * @brief
     * The current task.
     */
    Task* pThisTask = nullptr;

    /**
     * @brief
     * The Worker local data from WorkerThreadDesc.
     */
    void* pUserData = nullptr;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  The description of a worker Thread.
 */
struct WorkerThreadDesc
{
    struct GroupAndAffinity
    {
        /**
         * @brief
         *  The group the the affinities in affinitySet belong too.
         * Groups seem to only be relevant for Windows. All other platforms will ignore this value.
         */
        size_t group = 0;
        /**
         * @brief
         *  A set of processor affinities.
         */
        AffinitySet affinitySet;
    };

    /**
     * @brief
     *  Worker local user data to be passed to executing Tasks via TaskContext.
     */
    void* pUserData = nullptr;

    /**
     * @brief
     *  Specifies the thread affinity for each processor group. See
     *  gts::Thread::getSystemTopology to identify affinities within a group.
     */
    WorkerThreadDesc::GroupAndAffinity affinity;

    /**
     * @brief
     *  The priority at with the thread is scheduled by the OS.
     */
    Thread::Priority priority = Thread::Priority::PRIORITY_NORMAL;

    /**
     * @brief
     *  The thread's stack frame size.
     */
    uint32_t stackSize = Thread::DEFAULT_STACK_SIZE;

    /**
     * @brief
     *  The thread's name.
     */
    char name[DESC_NAME_SIZE] = { 0 };
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A visitor object for the worker pool. Allows the user to be notified
 *  at particular points during the WorkerPool's lifetime.
 */
struct WorkerPoolVisitor
{
    GTS_INLINE virtual ~WorkerPoolVisitor() {}

    /**
     * @brief
     *  Called when a Worker thread is created.
     * @param workerId
     *  A combination of the local Worker ID and the ID of the WorkerPool
     *  that owns it.
     */
    virtual void onThreadStart(OwnedId workerId) { GTS_UNREFERENCED_PARAM(workerId); }

    /**
     * @brief
     *  Called when a Worker thread is destroyed.
     * @param workerId
     *  A combination of the local Worker ID and the ID of the WorkerPool
     *  that owns it.
     */
    virtual void onThreadExit(OwnedId workerId) { GTS_UNREFERENCED_PARAM(workerId); }
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  The description of a WorkerPool.
 */
struct WorkerPoolDesc
{
    using SetThreadLocalStateFcn = void (*)(uintptr_t state);
    using GetThreadLocalStateFcn = uintptr_t(*)();

    /**
     * @brief
     *  The description of each worker thread.
     * @remark
     *   The "master" thread calling WorkerPool::Initialize will use index zero,
     *   BUT it will not be altered. The user should set these properties
     *   manually on the master thread if they are desired.
     */
    gts::Vector<WorkerThreadDesc> workerDescs;

    /**
     * @brief
     *  The setter for the thread local value that holds each worker thread's
     *  index. Useful for cross DLL thread local storage. Uses default if nullptr.
     */
    SetThreadLocalStateFcn pSetThreadLocalStateFcn = nullptr;

    /**
     * @brief
     *  The getter for the thread local value that holds each worker thread's
     *  index. Useful for cross DLL thread local storage. Uses default if nullptr.
     */
    GetThreadLocalStateFcn pGetThreadLocalStateFcn = nullptr;

    /**
     * @brief
     *  A user-defined visitor object for the WorkerPool. The object must
     *  stay alive for the lifetime of the WorkerPool.
     */
    WorkerPoolVisitor* pVisitor = nullptr;

    /**
     * @brief
     *  The maximum size of Tasks that are cached and reused. Sizes larger than
     *  this will be allocated from the heap.
     */
    uint32_t cachableTaskSize = GTS_NO_SHARING_CACHE_LINE_SIZE * 2;

    /**
     * @brief
     *  The number of Tasks to create and cache per Worker during initialization.
     */
    uint32_t initialTaskCountPerWorker = 0;

    /**
     * @brief
     *  A name to help with debugging.
     */
    char name[DESC_NAME_SIZE] = { 0 };
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  The description of a MicroScheduler.
 */
struct MicroSchedulerDesc
{
    /**
     * @brief
     * The WorkerPool to the MicroScheduler will run on.
     */
    WorkerPool* pWorkerPool = nullptr;

    /**
     * @brief
     * The number of Task priorities.
     * Priorities are indexed [0, priorityCount), with zero being the highest
     * priority. Must be >= 1.
     */

    uint32_t priorityCount = 1;

    /**
     * @brief
     * The age for each Worker when a lower priority Task will try to be
     * executed. Age is measured in number of Task executions since the last
     * boost.
     */
    int16_t priorityBoostAge = INT16_MAX;

    /**
     * @brief
     * A name to help with debugging.
     */
    char name[DESC_NAME_SIZE] = { 0 };
};

enum class MicroSchedulerCallbackType
{
    CHECK_FOR_TASK,

    //---------------
    COUNT
};

using OnCheckForTasksFcn = Task* (*)(void* pUserData, MicroScheduler* pThisScheduler, OwnedId localSchedulerId, bool& executedTask);


/** @} */ // end of MicroScheduler

} // namespace gts
