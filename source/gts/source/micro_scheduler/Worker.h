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
#include "gts/micro_scheduler/MicroSchedulerTypes.h"
#include "gts/platform/Thread.h"

namespace gts {

class MicroScheduler;
class WorkerPool;
class Schedule;

#ifdef GTS_MSVC
#pragma warning( push )
#pragma warning( disable : 4324) // alignment padding warning
#endif

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A worker thread used by MicroScheduler.
 */
class GTS_ALIGN(GTS_NO_SHARING_CACHE_LINE_SIZE) Worker
{
public: // STRUCTORS:

    Worker();
    ~Worker();

public: // MUTATORS:

    bool initialize(
        WorkerPool* pMyPool,
        uint32_t workerIdx,
        uintptr_t threadAffinity,
        gts::Thread::PriorityLevel threadPrioity,
        WorkerPoolDesc::GetThreadLocalIdxFcn pGetThreadLocalIdxFcn,
        WorkerPoolDesc::SetThreadLocalIdxFcn pSetThreadLocalIdxFcn,
        bool isMaster = false);

    void shutdown();

    void sleep();
    void wake(uint32_t count);

    void halt();
    void resume();

public: // ACCESSORS:

    uint32_t index() const;

private: // PRIVATE METHODS:

    // no copy
    Worker(Worker const&) {}
    Worker& operator=(Worker const&) { return *this; }

    // Thread routine arguements.
    struct WorkerThreadArgs
    {
        uint32_t workerIdx = UINT32_MAX;
        Worker* pSelf;
        gts::Atomic<bool> isReady = { false };
        fenv_t fenv;
    };

    // The worker thread routine.
    static GTS_THREAD_DECL(_workerThreadRoutine);

    // The Worker's Schedule execution loop.
    void _scheduleExecutionLoop(uint32_t workerIdx);

    // Find the next scheduler with work.
    MicroScheduler* _getNextSchedulerRoundRobin();

private: // DATA:

    friend class MicroScheduler;
    friend class WorkerPool;
    friend class Schedule;

    WorkerPool* m_pMyPool;
    WorkerPoolDesc::GetThreadLocalIdxFcn m_pGetThreadLocalIdxFcn;
    gts::BinarySemaphore m_suspendSemaphore;
    gts::BinarySemaphore m_haltSemaphore;
    gts::Thread m_thread;
    uint32_t m_currentSchedulerIdx;
    uint32_t m_resumeCount;

    WorkerPoolDesc::SetThreadLocalIdxFcn m_pSetThreadLocalIdxFcn;
    bool m_isSuspendedWeak;
    bool m_ishaltedWeak;
};

#ifdef GTS_MSVC
#pragma warning( pop )
#endif

} // namespace gts
