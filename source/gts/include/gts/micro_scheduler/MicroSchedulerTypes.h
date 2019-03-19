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

#include "gts/containers/Vector.h"
#include "gts/platform/Thread.h"

namespace gts {

class MicroScheduler;
class WorkerPool;

const uint32_t UNKNOWN_WORKER_IDX = UINT32_MAX;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  The context associated with the task being executed.
 */
struct TaskContext
{
    MicroScheduler* pMicroScheduler;
    uint32_t workerIndex;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  The description of a worker Thread.
 */
struct WorkerThreadDesc
{
    //! The CPU(s) where the OS will schedule the thread.
    uintptr_t affinityMask = UINTPTR_MAX;

    //! The priority at with the thread is scheduled by the OS.
    gts::Thread::PriorityLevel priority = gts::Thread::PRIORITY_NORMAL;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  The description of a WorkerPool.
 */
struct WorkerPoolDesc
{
    using SetThreadLocalIdxFcn = void (*)(uint32_t tid);
    using GetThreadLocalIdxFcn = uint32_t(*)();

    //! The description of each worker thread.
    gts::Vector<WorkerThreadDesc> workerDescs;

    //! The setter for the thread local value that holds each worker thread's
    //! index. Set to handle cross DLL thread local storage. Uses default if nullptr.
    SetThreadLocalIdxFcn pSetThreadLocalIdxFcn = nullptr;

    //! The getter for the thread local value that holds each worker thread's
    //! index. Set to handle cross DLL thread local storage. Uses default if nullptr.
    GetThreadLocalIdxFcn pGetThreadLocalIdxFcn = nullptr;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  The description of a MicroScheduler.
 */
struct MicroSchedulerDesc
{
    //! The WorkerPool to the MicroScheduler will run on.
    WorkerPool* pWorkerPool = nullptr;

    //! The number of Task priorities.
    //! Priorities are indexed [0, priorityCount), with zero being the highest
    //! priority.
    uint32_t priorityCount = 1;

    //! The age for each Worker when a lower priority Task will try to be
    //! executed. Age is measured in number of Task executions since the last boost.
    int16_t priorityBoostAge = INT16_MAX;
};

} // namespace gts
