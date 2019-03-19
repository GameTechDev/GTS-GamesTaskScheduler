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
#include "Worker.h"

#include "gts/containers/parallel/DistributedSlabAllocator.h"
#include "gts/platform/Assert.h"
#include "gts/analysis/Instrumenter.h"
#include "gts/analysis/Analyzer.h"

#include "gts/micro_scheduler/MicroScheduler.h"
#include "gts/micro_scheduler/WorkerPool.h"

#include "Schedule.h"
#include "Backoff.h"

static GTS_THREAD_LOCAL uint32_t tl_threadIdx = gts::UNKNOWN_WORKER_IDX;

//------------------------------------------------------------------------------
uint32_t gtsGetThreadIdx()
{
    return tl_threadIdx;
}

//------------------------------------------------------------------------------
void gtsSetThreadIdx(uint32_t idx)
{
    tl_threadIdx = idx;
}

namespace gts {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Worker:

// STRUCTORS:

//------------------------------------------------------------------------------
Worker::Worker()
    : m_pMyPool(nullptr)
    , m_currentSchedulerIdx(0)
    , m_pGetThreadLocalIdxFcn(nullptr)
    , m_pSetThreadLocalIdxFcn(nullptr)
    , m_resumeCount(0)
    , m_isSuspendedWeak(true)
    , m_ishaltedWeak(false)
{}

//------------------------------------------------------------------------------
Worker::~Worker()
{}

//------------------------------------------------------------------------------
bool Worker::initialize(
    WorkerPool* pMyPool,
    uint32_t workerIdx,
    uintptr_t threadAffinity,
    gts::Thread::PriorityLevel threadPrioity,
    WorkerPoolDesc::GetThreadLocalIdxFcn pGetThreadLocalIdxFcn,
    WorkerPoolDesc::SetThreadLocalIdxFcn pSetThreadLocalIdxFcn,
    bool isMaster)
{
    GTS_INSTRUMENTER_SCOPED(analysis::Tag::INTERNAL, "Worker Init", workerIdx, 0);

    GTS_ASSERT(pMyPool != nullptr);
    m_pMyPool = pMyPool;

    m_pGetThreadLocalIdxFcn = pGetThreadLocalIdxFcn != nullptr
        ? pGetThreadLocalIdxFcn
        : gtsGetThreadIdx;

    m_pSetThreadLocalIdxFcn = pSetThreadLocalIdxFcn != nullptr
        ? pSetThreadLocalIdxFcn
        : gtsSetThreadIdx;

    if (isMaster)
    {
        gts::ThisThread::setAffinity(threadAffinity);
        gts::ThisThread::setPriority(threadPrioity);
        m_pSetThreadLocalIdxFcn(workerIdx);
        pMyPool->m_pGetThreadLocalIdxFcn = m_pGetThreadLocalIdxFcn;
        return true;
    }

    // Get the current floating-point environment state.
    fenv_t fenv;
    if (fegetenv(&fenv) != 0)
    {
        GTS_ASSERT(0 && "Failed to get the floating point environment.");
        return false;
    }

    // Thread args
    WorkerThreadArgs workerArgs;
    workerArgs.pSelf = this;
    workerArgs.workerIdx = workerIdx;
    workerArgs.fenv = fenv;

    GTS_INSTRUMENTER_MARKER(analysis::Tag::INTERNAL, "Worker Thread Starting", workerIdx, 0);

    // Create the thread.
    for(uint32_t ii = 0; ii < 16; ++ii)
    {
        if(m_thread.start(_workerThreadRoutine, &workerArgs))
        {
            break;
        }
        GTS_INSTRUMENTER_MARKER(analysis::Tag::INTERNAL, "Worker Thread Fail", workerIdx, 0);
        printf("Failed to create Thread.\n");
        GTS_PAUSE();
    }

    GTS_INSTRUMENTER_MARKER(analysis::Tag::INTERNAL, "Worker Thread Started", workerIdx, m_thread.getId());

    // Keep 'workerArgs' alive until thread is running.
    while (!workerArgs.isReady.load())
    {
        GTS_PAUSE();
    }
    GTS_INSTRUMENTER_MARKER(analysis::Tag::INTERNAL, "Worker isReady", workerIdx, 0);

    m_thread.setAffinity(threadAffinity);
    GTS_INSTRUMENTER_MARKER(analysis::Tag::INTERNAL, "Worker setAffinity", workerIdx, 0);

    m_thread.setPriority(threadPrioity);
    GTS_INSTRUMENTER_MARKER(analysis::Tag::INTERNAL, "Worker setPriority", workerIdx, 0);

    GTS_INSTRUMENTER_MARKER(analysis::Tag::INTERNAL, "Worker Init Done", workerIdx, 0);

    return true;
}

//------------------------------------------------------------------------------
void Worker::shutdown()
{
    GTS_INSTRUMENTER_SCOPED(analysis::Tag::INTERNAL, "Worker shutdown", m_pGetThreadLocalIdxFcn(), 0);

    m_thread.join();
    m_thread.destroy();
}

//------------------------------------------------------------------------------
uint32_t Worker::index() const
{
    return m_pGetThreadLocalIdxFcn();
}

// PRIVATE METHODS:

//------------------------------------------------------------------------------
GTS_THREAD_DEFN(Worker, _workerThreadRoutine)
{
    WorkerThreadArgs& args   = *(WorkerThreadArgs*)pArg;
    const uint32_t workerIdx = args.workerIdx;
    Worker* pSelf            = args.pSelf;
    pSelf->m_isSuspendedWeak = false;

    pSelf->m_pSetThreadLocalIdxFcn(workerIdx);

    GTS_INSTRUMENTER_SCOPED(analysis::Tag::INTERNAL, "Worker Routine", pSelf->index(), 0);

    GTS_INSTRUMENTER_NAME_THREAD("Worker Thread %d", workerIdx);

    // Set the main thread's floating-point environment state.
    if (fesetenv(&args.fenv))
    {
        GTS_ASSERT(0);
    }

    // Inform startThreads() that we are ready.
    args.isReady.store(true);

    // Signal that a thread is running.
    pSelf->m_pMyPool->m_runningWorkerCount.fetch_add(1);

    //vvvvvvvvvvvvvvvvvvvvvvv 'pArgs' is now invalid vvvvvvvvvvvvvvvvvvvvvvvvvvv

    // Execute tasks until done.
    pSelf->_scheduleExecutionLoop(pSelf->m_pGetThreadLocalIdxFcn());

    // Signal that a thread as quit.
    pSelf->m_pMyPool->m_runningWorkerCount.fetch_sub(1);

    // Make worker unknown.
    pSelf->m_pSetThreadLocalIdxFcn(UNKNOWN_WORKER_IDX);

    return 0;
}

//------------------------------------------------------------------------------
void Worker::sleep()
{
    GTS_ANALYSIS_COUNT(m_pGetThreadLocalIdxFcn(), gts::analysis::AnalysisType::NUM_SLEEP_SUCCESSES);
    GTS_INSTRUMENTER_SCOPED(analysis::Tag::INTERNAL, "Worker Sleep", m_pGetThreadLocalIdxFcn(), 0);

    // Notify the WorkPool of the suspension.
    m_pMyPool->m_suspendedThreadCount.fetch_add(1);

    // Suspend the thread.
    m_isSuspendedWeak = true;
    m_suspendSemaphore.wait();

    // Thead woke up.
    m_isSuspendedWeak = false;

    // Resume more Workers if needed.
    if (m_resumeCount > 0)
    {
        // Wake up the next Worker. This distributes the resume.
        for (size_t ii = m_pGetThreadLocalIdxFcn() + 1; ii < m_pMyPool->workerCount(); ++ii)
        {
            Worker& worker = m_pMyPool->m_pWorkersByIdx[ii];

            if (worker.m_isSuspendedWeak)
            {
                worker.wake(m_resumeCount);

                // We resumed another thread so clear our resume count.
                m_resumeCount = 0;
                break;
            }
        }
    }

    // Notify the WorkPool of the un-suspension.
    m_pMyPool->m_suspendedThreadCount.fetch_sub(1);

    GTS_INSTRUMENTER_MARKER(analysis::Tag::INTERNAL, "Worker Woke", m_pGetThreadLocalIdxFcn(), 0);
}

//------------------------------------------------------------------------------
void Worker::wake(uint32_t count)
{
    GTS_ASSERT(count <= m_pMyPool->m_totalWorkerCount);
    GTS_ANALYSIS_COUNT(index(), gts::analysis::AnalysisType::NUM_WAKE_CHECKS);
    GTS_ANALYSIS_TIME_SCOPED(index(), gts::analysis::AnalysisType::NUM_WAKE_CHECKS);

    if (count > 0 && m_isSuspendedWeak)
    {
        m_resumeCount = count - 1;

        GTS_ANALYSIS_COUNT(index(), gts::analysis::AnalysisType::NUM_WAKE_SUCCESSES);
        GTS_ANALYSIS_TIME_SCOPED(index(), gts::analysis::AnalysisType::NUM_WAKE_SUCCESSES);
        m_suspendSemaphore.signal();
    }
}

//------------------------------------------------------------------------------
void Worker::halt()
{
    // enter halt.
    GTS_ANALYSIS_COUNT(index(), gts::analysis::AnalysisType::NUM_halt_SUCCESSES);
    GTS_INSTRUMENTER_SCOPED(analysis::Tag::INTERNAL, "Worker halt", m_pGetThreadLocalIdxFcn(), 0);

    m_pMyPool->m_haltedThreadCount.fetch_add(1);

    m_ishaltedWeak = true;
    m_haltSemaphore.wait();
    m_ishaltedWeak = false;

    // resume.
    m_pMyPool->m_haltedThreadCount.fetch_sub(1);

    GTS_INSTRUMENTER_MARKER(analysis::Tag::INTERNAL, "Worker Resumed", m_pGetThreadLocalIdxFcn(), 0);
}

//------------------------------------------------------------------------------
void Worker::resume()
{
    GTS_ANALYSIS_COUNT(index(), gts::analysis::AnalysisType::NUM_RESUME_CHECKS);
    GTS_ANALYSIS_TIME_SCOPED(index(), gts::analysis::AnalysisType::NUM_RESUME_CHECKS);

    if (m_isSuspendedWeak)
    {
        GTS_ANALYSIS_COUNT(index(), gts::analysis::AnalysisType::NUM_RESUME_SUCCESSES);
        GTS_ANALYSIS_TIME_SCOPED(index(), gts::analysis::AnalysisType::NUM_RESUME_SUCCESSES);
        m_haltSemaphore.signal();
    }
}

//------------------------------------------------------------------------------
void Worker::_scheduleExecutionLoop(uint32_t workerIdx)
{
    sleep();

    Backoff backoff;

    GTS_INSTRUMENTER_SCOPED(analysis::Tag::INTERNAL, "Worker Enter Loop", workerIdx, 0);

    while (m_pMyPool->isRunning())
    {
        while (!m_pMyPool->m_ishalting.load(gts::memory_order::acquire) && m_pMyPool->isRunning())
        {
            MicroScheduler* pCurrentSchedule = _getNextSchedulerRoundRobin();

            if (pCurrentSchedule != nullptr && pCurrentSchedule->hasTasks())
            {
                pCurrentSchedule->m_pSchedulesByIdx[workerIdx].run(backoff);

                // Reset for next time.
                backoff.reset();
            }

            sleep();
        }

        halt();
    }
}

//------------------------------------------------------------------------------
MicroScheduler* Worker::_getNextSchedulerRoundRobin()
{
    MicroScheduler* pResult = nullptr;

    if (!m_pMyPool->m_registeredSchedulers.empty())
    {
        uint32_t index = m_currentSchedulerIdx;
        const uint32_t numSchedulers = (uint32_t)m_pMyPool->m_registeredSchedulers.size();

        for (uint32_t ii = 0; ii < numSchedulers; ++ii)
        {
            MicroScheduler* pMicroScheduler = m_pMyPool->m_registeredSchedulers[index % numSchedulers];

            if (pMicroScheduler->hasTasks())
            {
                m_currentSchedulerIdx = index;
                pResult = pMicroScheduler;
                break;
            }

            ++index;
        }
    }

    return pResult;
}

} // namespace gts
