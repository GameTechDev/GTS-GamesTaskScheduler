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

#include "gts/platform/Assert.h"
#include "gts/analysis/Trace.h"
#include "gts/analysis/Counter.h"
#include "gts/synchronization/Lock.h"

#include "gts/micro_scheduler/MicroScheduler.h"
#include "gts/micro_scheduler/WorkerPool.h"

#include "LocalScheduler.h"

static GTS_THREAD_LOCAL uintptr_t tl_threadState;
static constexpr uint64_t LOWER_BOUND_SLEEP_CYCLES = 1024 * 10;

//------------------------------------------------------------------------------
static uintptr_t gtsGetThreadState()
{
    return tl_threadState;
}

//------------------------------------------------------------------------------
static void gtsSetThreadState(uintptr_t state)
{
    tl_threadState = state;
}

namespace gts {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Worker:

WorkerPoolDesc::GetThreadLocalStateFcn Worker::s_pGetThreadLocalStateFcn = nullptr;
Atomic<size_t> Worker::s_pGetThreadLocalStateFcnRefCount = { 0 };

// STRUCTORS:

//------------------------------------------------------------------------------
Worker::Worker()
    : m_pMyPool(nullptr)
    , m_pTaskPool(nullptr)
    , m_pSleepBlocker(nullptr)
    , m_pCurrentScheduler(nullptr)
    , m_pUserData(nullptr)
    , m_minSleepCycles(UINT64_MAX)
    , m_pHaltSemaphore(nullptr)
    , m_pLocalSchedulersMutex(nullptr)
    , m_pGetThreadLocalStateFcn(nullptr)
    , m_pSetThreadLocalStateFcn(nullptr)
    , m_threadId(0)
    , m_randState(0)
    , m_currentScheduleIdx(0)
    , m_resumeCount(0)
    , m_refCount(1)
    , m_cachableTaskSize(0)

{}

//------------------------------------------------------------------------------
Worker::~Worker()
{}

//------------------------------------------------------------------------------
bool Worker::initialize(
    WorkerPool* pMyPool,
    OwnedId workerId,
    WorkerThreadDesc::GroupAndAffinity  affinity,
    Thread::Priority threadPrioity,
    const char* threadName,
    void* pUserData,
    WorkerPoolDesc::GetThreadLocalStateFcn pGetThreadLocalStateFcn,
    WorkerPoolDesc::SetThreadLocalStateFcn pSetThreadLocalStateFcn,
    WorkerPoolVisitor* pVisitor,
    uint32_t cachableTaskSize,
    uint32_t initialTaskCountPerWorker,
    bool isMaster)
{
    GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::WORKERPOOL_DEBUG, analysis::Color::AntiqueWhite, "WORKER INIT", this, workerId.localId());

    m_pUserData = pUserData;

    m_pGetThreadLocalStateFcn = pGetThreadLocalStateFcn != nullptr
        ? pGetThreadLocalStateFcn
        : gtsGetThreadState;

    m_pSetThreadLocalStateFcn = pSetThreadLocalStateFcn != nullptr
        ? pSetThreadLocalStateFcn
        : gtsSetThreadState;

    m_id = workerId;
    m_randState = m_id.localId() + 1;

    GTS_ASSERT(pMyPool != nullptr);
    m_pMyPool = pMyPool;

    m_pTaskPool = alignedNew<TaskPool, GTS_NO_SHARING_CACHE_LINE_SIZE>();
    m_pSleepBlocker = alignedNew<ThreadBlocker, GTS_NO_SHARING_CACHE_LINE_SIZE>();
    m_pHaltSemaphore = alignedNew<BinarySemaphore, GTS_NO_SHARING_CACHE_LINE_SIZE>();
    m_pLocalSchedulersMutex = alignedNew<MutexType, GTS_NO_SHARING_CACHE_LINE_SIZE>();

    if(cachableTaskSize < GTS_CACHE_LINE_SIZE)
    {
        GTS_ASSERT(cachableTaskSize >= GTS_CACHE_LINE_SIZE);
        cachableTaskSize = GTS_CACHE_LINE_SIZE;
    }
    m_cachableTaskSize = cachableTaskSize;

    if (isMaster)
    {
        if(initialTaskCountPerWorker > 0)
        {
            Vector<Task*, AlignedAllocator<GTS_NO_SHARING_CACHE_LINE_SIZE>> tasks(initialTaskCountPerWorker);
            for(uint32_t ii = 0; ii < initialTaskCountPerWorker; ++ii)
            {
                tasks[ii] = allocateTask(m_cachableTaskSize - sizeof(internal::TaskHeader));
            }

            for(uint32_t ii = 0; ii < initialTaskCountPerWorker; ++ii)
            {
                freeTask(tasks[initialTaskCountPerWorker - ii - 1]);
            }
        }

        m_threadId = ThisThread::getId();

        // NOTE: We don't own this thread so don't alter it's affinity or priority.

        uintptr_t state = m_pGetThreadLocalStateFcn();
        if(state == 0)
        {
            m_pSetThreadLocalStateFcn((uintptr_t)this);
        }
        else
        {
            ((Worker*)state)->m_refCount.fetch_add(1, memory_order::acq_rel);
        }
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
    workerArgs.pSelf                     = this;
    workerArgs.workerId                  = workerId;
    workerArgs.fenv                      = fenv;
    workerArgs.pVisitor                  = pVisitor;
    workerArgs.name                      = threadName;
    workerArgs.initialTaskCountPerWorker = initialTaskCountPerWorker;

    // Create the thread.
    for(uint32_t ii = 0; ii < 16; ++ii)
    {
        if(m_thread.start(_workerThreadRoutine, &workerArgs))
        {
            break;
        }
        GTS_ASSERT(0 && "Failed to create Thread");
        GTS_PAUSE();
    }

    // Keep 'workerArgs' alive until thread is running.
    while (!workerArgs.isReady.load(memory_order::acquire))
    {
        GTS_PAUSE();
    }

    if(!affinity.affinitySet.empty())
    {
        m_thread.setAffinity(affinity.group, affinity.affinitySet);
    }

    m_thread.setPriority(threadPrioity);

    m_registeredSchedulers.reserve(1024);

    return true;
}

//------------------------------------------------------------------------------
void Worker::shutdown()
{
    GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::WORKERPOOL_DEBUG, analysis::Color::AntiqueWhite, "WORKER SHUTDOWN", this, id().localId());

    _freeTasks();

    if (id().localId() != 0) // Skip master. It has no thread.
    {
        m_thread.join();
        m_thread.destroy();
    }

    alignedDelete(m_pLocalSchedulersMutex);
    alignedDelete(m_pHaltSemaphore);
    alignedDelete(m_pSleepBlocker);
    alignedDelete(m_pTaskPool);
}

//------------------------------------------------------------------------------
Task* Worker::allocateTask(uint32_t size)
{
    uint32_t totalSize = gtsMax(m_cachableTaskSize, (uint32_t)sizeof(internal::TaskHeader) + size);
    Task* pTask = nullptr;

    if (totalSize == m_cachableTaskSize)
    {
        if((pTask = m_pTaskPool->pFreeList) != nullptr)
        {
            m_pTaskPool->pFreeList = pTask->header().pListNext.load(memory_order::relaxed);
        }
        else if(m_pTaskPool->pDeferredFreeList.load(memory_order::relaxed) != nullptr)
        {
            GTS_SPECULATION_FENCE();

            pTask = m_pTaskPool->pDeferredFreeList.exchange(nullptr, memory_order::acq_rel);
            m_pTaskPool->pFreeList = pTask->header().pListNext.load(memory_order::relaxed);
        }
    }
    
    if(!pTask)
    {
        GTS_SPECULATION_FENCE();
        pTask = ((internal::TaskHeader*)GTS_ALIGNED_MALLOC(totalSize, GTS_NO_SHARING_CACHE_LINE_SIZE))->_task();
    }

    internal::TaskHeader& header = pTask->header();
    header.pParent               = nullptr;
    header.pMyLocalScheduler     = nullptr;
    header.pMyWorker             = this;
    header.pListNext.store(nullptr, memory_order::relaxed);
    header.affinity              = ANY_WORKER;
    header.refCount.store(1, memory_order::relaxed);
    header.executionState        = internal::TaskHeader::ALLOCATED;
    header.flags                 = totalSize <= m_cachableTaskSize ? internal::TaskHeader::TASK_IS_SMALL : 0;

    return pTask;
}

//------------------------------------------------------------------------------
void Worker::freeTask(Task* pTask)
{
    GTS_SIM_TRACE_MARKER(sim_trace::MARKER_FREE_TASK_BEGIN);

    GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::MICRO_SCHEDULER_ALL, analysis::Color::Magenta, "WORKER FREE TASK", this, pTask);
    //GTS_COUNTER_INC(m_pMyPool->m_pAnalyzer, localId(), analysis::AnalysisType::NUM_FREES);

    internal::TaskHeader& taskHeader = pTask->header();

    if(taskHeader.flags & internal::TaskHeader::TASK_IS_SMALL && taskHeader.pMyWorker != nullptr)
    {
        GTS_ASSERT(taskHeader.executionState != internal::TaskHeader::FREED && "double free!");
        taskHeader.executionState = internal::TaskHeader::FREED;

        if(taskHeader.pMyWorker == this)
        {
            taskHeader.pListNext.store(m_pTaskPool->pFreeList, memory_order::relaxed);
            m_pTaskPool->pFreeList = pTask;
        }
        else
        {
            Task* pHead = taskHeader.pMyWorker->m_pTaskPool->pDeferredFreeList.load(memory_order::acquire);
            taskHeader.pListNext.store(pHead, memory_order::relaxed);
            
            while (!taskHeader.pMyWorker->m_pTaskPool->pDeferredFreeList.compare_exchange_strong(pHead, pTask, memory_order::acq_rel, memory_order::relaxed))
            {
                taskHeader.pListNext.store(pHead, memory_order::relaxed);
                GTS_SPECULATION_FENCE();
            }
        }
    }
    else
    {
        GTS_ALIGNED_FREE(&pTask->header());
    }

    GTS_SIM_TRACE_MARKER(sim_trace::MARKER_FREE_TASK_END);
}

//------------------------------------------------------------------------------
MicroScheduler* Worker::currentMicroScheduler() const
{
    if (m_pCurrentScheduler != nullptr)
    {
        return m_pCurrentScheduler->m_pMyScheduler;
    }
    return nullptr;
}

// PRIVATE METHODS:

//------------------------------------------------------------------------------
void Worker::_workerThreadRoutine(void* pArg)
{
    WorkerThreadArgs& args              = *(WorkerThreadArgs*)pArg;
    const OwnedId workerId              = args.workerId;
    Worker* pSelf                       = args.pSelf;
    WorkerPoolVisitor* pVisitor         = args.pVisitor;
    uint32_t initialTaskCountPerWorker  = args.initialTaskCountPerWorker;

    pSelf->m_threadId = ThisThread::getId();

    if (args.name)
    {
        ThisThread::setName(args.name);
    }

    GTS_ASSERT(pSelf->m_pGetThreadLocalStateFcn() == 0);
    pSelf->m_pSetThreadLocalStateFcn((uintptr_t)pSelf);

    uintptr_t state = pSelf->m_pGetThreadLocalStateFcn();
    if(state == 0)
    {
        pSelf->m_pSetThreadLocalStateFcn((uintptr_t)pSelf);
    }
    else
    {
        ((Worker*)state)->m_refCount.fetch_add(1, memory_order::acq_rel);
    }

    if(pVisitor)
    {
        pVisitor->onThreadStart(workerId);
    }

    GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::WORKERPOOL_DEBUG, analysis::Color::AntiqueWhite, "WORKER ENTER THREAD", pSelf, pSelf->id().localId());

    if (args.name && args.name[0])
    {
        GTS_TRACE_NAME_THREAD(analysis::CaptureMask::ALL, ThisThread::getId(), "%s", args.name);
    }
    else
    {
        GTS_TRACE_NAME_THREAD(analysis::CaptureMask::ALL, ThisThread::getId(), "WorkerThread_%d_%d", workerId.ownerId(), workerId.localId());
    }

    // Set the main thread's floating-point environment state.
    if (fesetenv(&args.fenv))
    {
        GTS_ASSERT(0);
    }

    // Inform startThreads() that we are ready.
    args.isReady.store(true, memory_order::release);

    // -v-v-v-v-v-v-v-v-v-v- 'pArgs' is now invalid -v-v-v-v-v-v-v-v-v-v-

    // Precache Tasks.
    if(initialTaskCountPerWorker > 0)
    {
        Vector<Task*, AlignedAllocator<alignof(Task*)>> tasks(initialTaskCountPerWorker);
        for(uint32_t ii = 0; ii < initialTaskCountPerWorker; ++ii)
        {
            tasks[ii] = pSelf->allocateTask(pSelf->m_cachableTaskSize - sizeof(internal::TaskHeader));
        }

        for(uint32_t ii = 0; ii < initialTaskCountPerWorker; ++ii)
        {
            pSelf->freeTask(tasks[initialTaskCountPerWorker - ii - 1]);
        }
    }

    // Execute tasks until done.
    pSelf->_schedulerExecutionLoop(workerId.localId());

    if(pVisitor)
    {
        pVisitor->onThreadExit(workerId);
    }

    // Make worker unknown.
    state = pSelf->m_pGetThreadLocalStateFcn();
    if(((Worker*)state)->m_refCount.fetch_sub(1, memory_order::acq_rel) - 1 == 0)
    {
        pSelf->m_pSetThreadLocalStateFcn(0);
    }
}

//------------------------------------------------------------------------------
void Worker::_freeTasks()
{
    while (m_pTaskPool->pFreeList != nullptr)
    {
        Task* pTask = m_pTaskPool->pFreeList;
        m_pTaskPool->pFreeList = m_pTaskPool->pFreeList->header().pListNext.load(memory_order::relaxed);
        GTS_ALIGNED_FREE(&pTask->header());
    }

    while (m_pTaskPool->pDeferredFreeList.load(memory_order::relaxed) != nullptr)
    {
        Task* pTask = m_pTaskPool->pDeferredFreeList.load(memory_order::relaxed);
        m_pTaskPool->pDeferredFreeList.store(pTask->header().pListNext.load(memory_order::relaxed), memory_order::relaxed);
        GTS_ALIGNED_FREE(&pTask->header());
    }
}

//------------------------------------------------------------------------------
void Worker::sleep(bool force)
{
    GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::WORKERPOOL_ALL, analysis::Color::DarkRed, "WORKER SLEEP", this, id().localId());
    GTS_WP_COUNTER_INC(id(), gts::analysis::WorkerPoolCounters::NUM_SLEEP_SUCCESSES);

    uint64_t startSleepTime = GTS_RDTSC();

    // Notify the WorkPool of the suspension.
    m_pMyPool->m_sleepingWorkerCount.fetch_add(1, memory_order::acquire);

    if (force)
    {
        GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::WORKERPOOL_DEBUG, analysis::Color::Yellow, "WORKER FORCE SLEEP RESET", this, id().localId());
        // For the Worker to sleep by reseting the semaphore.
        m_pSleepBlocker->semaphore.reset();
    }

    // Mark as asleep.
    m_pSleepBlocker->state.store(ThreadBlocker::IS_BLOCKED, memory_order::relaxed);

    bool didSleep = m_pSleepBlocker->semaphore.wait(); // Zzzz.....................

    GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::WORKERPOOL_DEBUG, analysis::Color::Yellow, "WORKER OUT OF SEMAPHORE", this, id().localId());

    // Mark as awake.
    m_pSleepBlocker->state.exchange(ThreadBlocker::IS_UNBLOCKED, memory_order::release);

    // Wait for the waker to exit its wake loop.
    while (m_pSleepBlocker->state.load(memory_order::acquire) != ThreadBlocker::IS_OUT_OF_SIGNAL_LOOP)
    {
        GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::WORKERPOOL_DEBUG, analysis::Color::Yellow, "WORKER WAITING FOR IS_OUT_OF_SIGNAL_LOOP", this, id().localId());
        GTS_PAUSE();
    }

    if (m_pSleepBlocker->resetOnWake.load(memory_order::acquire))
    {
        GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::WORKERPOOL_DEBUG, analysis::Color::Yellow, "WORKER RESET SEMAPHORE", this, id().localId());
        m_pSleepBlocker->resetOnWake.store(false, memory_order::relaxed);
        m_pSleepBlocker->semaphore.reset();
    }

    m_pSleepBlocker->state.exchange(ThreadBlocker::IS_AWAKE, memory_order::release);

    // Resume more Workers if needed.
    if (m_resumeCount > 0)
    {
        // Wake up the next Worker. This distributes the resume.
        m_pMyPool->_wakeWorker(this, m_resumeCount, true);
    }

    // Clear our resume count.
    m_resumeCount = 0;

    // Notify the WorkPool of the resume.
    m_pMyPool->m_sleepingWorkerCount.fetch_sub(1, memory_order::release);

    // Wait for wakers to exit.
    while (m_pSleepBlocker->numWakers.load(memory_order::acquire) > 0)
    {
        GTS_PAUSE();
    }

    uint64_t endSleepTime = GTS_RDTSC();

    if (didSleep && endSleepTime > startSleepTime)
    {
        uint64_t dt = endSleepTime - startSleepTime;
        GTS_TRACE_ZONE_MARKER_P1(analysis::CaptureMask::WORKERPOOL_DEBUG, analysis::Color::DarkRed, "WORKER SLEEP TIME", dt);
        if (dt < m_minSleepCycles && dt > LOWER_BOUND_SLEEP_CYCLES)
        {
            GTS_TRACE_ZONE_MARKER_P1(analysis::CaptureMask::WORKERPOOL_DEBUG, analysis::Color::Green, "WORKER NEW SLEEP TIME", dt);
            m_minSleepCycles = dt;
        }
    }

    GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::WORKERPOOL_DEBUG, analysis::Color::Green, "WORKER WOKE", this, id().localId());
}

//------------------------------------------------------------------------------
void Worker::resetSleepState()
{
    m_pSleepBlocker->semaphore.reset();
}

//------------------------------------------------------------------------------
bool Worker::wake(uint32_t count, bool reset, bool force)
{
    if(count == 0)
    {
        GTS_ASSERT(count != 0);
        return false;
    }

    GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::WORKERPOOL_ALL, analysis::Color::Green1, "ATTEMPTY WAKE WORKER", this, id().localId());
    GTS_WP_COUNTER_INC(id(), gts::analysis::WorkerPoolCounters::NUM_WAKE_CHECKS);

    if (force || m_pSleepBlocker->state.load(memory_order::acquire) == ThreadBlocker::IS_BLOCKED)
    {
        // Only allow one waker.
        if (m_pSleepBlocker->numWakers.fetch_add(1, memory_order::acq_rel) != 0)
        {
            m_pSleepBlocker->numWakers.fetch_sub(1, memory_order::acq_rel);
            return false;
        }

        // Quit if the Worker became unblock.
        if (m_pSleepBlocker->state.load(memory_order::acquire) != ThreadBlocker::IS_BLOCKED)
        {
            m_pSleepBlocker->numWakers.fetch_sub(1, memory_order::acq_rel);
            return false;
        }

        GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::WORKERPOOL_DEBUG, analysis::Color::Green2, "WAKING WORKER", this, id().localId());
        do
        {
            GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::WORKERPOOL_DEBUG, analysis::Color::Green3, "SIGNAL SEMAPHORE", this, id().localId());
            m_pSleepBlocker->resetOnWake.store(reset, memory_order::relaxed);
            m_pSleepBlocker->semaphore.signal();
            GTS_PAUSE();
        }
        while (m_pSleepBlocker->state.load(memory_order::acquire) == ThreadBlocker::IS_BLOCKED);

        // Let the sleeper know we are out of the wake loop.
        m_pSleepBlocker->state.exchange(ThreadBlocker::IS_OUT_OF_SIGNAL_LOOP, memory_order::acq_rel);

        GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::WORKERPOOL_DEBUG, analysis::Color::Yellow, "WORKER IS_OUT_OF_SIGNAL_LOOP", this, id().localId());

        // Wait until the Worker if fully awake before allowing new wakers to try.
        while (m_pSleepBlocker->state.load(memory_order::acquire) != ThreadBlocker::IS_AWAKE)
        {
            GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::WORKERPOOL_DEBUG, analysis::Color::Yellow, "WORKER WAITING FOR IS_AWAKE", this, id().localId());
            GTS_PAUSE();
        }

        m_pSleepBlocker->numWakers.fetch_sub(1, memory_order::acq_rel);

        GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::WORKERPOOL_DEBUG, analysis::Color::Yellow, "WORKER DONE WAKING", this, id().localId());

        m_resumeCount = count - 1;
        GTS_WP_COUNTER_INC(id(), gts::analysis::WorkerPoolCounters::NUM_WAKE_SUCCESSES);
        return true;
    }

    return false;
}

//------------------------------------------------------------------------------
void Worker::halt()
{
    // enter halt.
    GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::WORKERPOOL_ALL, analysis::Color::DarkRed, "WORKER HALT", this, id().localId());
    GTS_WP_COUNTER_INC(id(), gts::analysis::WorkerPoolCounters::NUM_HALT_SUCCESSES);

    m_pMyPool->m_haltedWorkerCount.fetch_add(1, memory_order::acquire);
    m_pHaltSemaphore->reset();

    m_pHaltSemaphore->wait();  // Zzzz.....................

    // resume.
    m_pMyPool->m_haltedWorkerCount.fetch_sub(1, memory_order::release);

    GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::WORKERPOOL_DEBUG, analysis::Color::AntiqueWhite, "WORKER RESUMED", this, id().localId());
}

//------------------------------------------------------------------------------
void Worker::resume()
{
    GTS_WP_COUNTER_INC(id(), gts::analysis::WorkerPoolCounters::NUM_RESUMES);
    m_pHaltSemaphore->signal();
}

//------------------------------------------------------------------------------
void Worker::registerLocalScheduler(LocalScheduler* pLocalScheduler)
{
    GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::WORKERPOOL_DEBUG, analysis::Color::AntiqueWhite, "WORKER REG LOCAL SCHED", this, id().localId());
    GTS_WP_COUNTER_INC(id(), gts::analysis::WorkerPoolCounters::NUM_SCHEDULER_REGISTERS);

    Lock<MutexType> lock(*m_pLocalSchedulersMutex);

    GTS_ASSERT(_locateScheduler(pLocalScheduler) == -1 && "LocalScheduler is already registered.");

    m_registeredSchedulers.push_back(pLocalScheduler);
}

//------------------------------------------------------------------------------
void Worker::unregisterLocalScheduler(LocalScheduler* pLocalScheduler)
{
    GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::WORKERPOOL_DEBUG, analysis::Color::AntiqueWhite, "WORKER UNREG LOCAL SCHED", this, id().localId());
    GTS_WP_COUNTER_INC(id(), gts::analysis::WorkerPoolCounters::NUM_SCHEDULER_UNREGISTERS);

    Lock<MutexType> lock(*m_pLocalSchedulersMutex);

    // Wait until it's not in use by the Worker.
    pLocalScheduler->m_workerAccessMutex.lock();

    int32_t slotIdx = _locateScheduler(pLocalScheduler);
    GTS_ASSERT(slotIdx != -1);

    if (slotIdx != (int32_t)m_registeredSchedulers.size() - 1)
    {
        // swap out
        LocalScheduler* back = m_registeredSchedulers.back();
        back->m_workerAccessMutex.lock();
        m_registeredSchedulers[slotIdx] = back;
        back->m_workerAccessMutex.unlock();
        m_registeredSchedulers.pop_back();
    }
    else
    {
        m_registeredSchedulers.pop_back();
    }

    pLocalScheduler->m_workerAccessMutex.unlock();
}

//------------------------------------------------------------------------------
void Worker::_initTlsGetAndSet(
    WorkerPoolDesc::GetThreadLocalStateFcn& getter,
    WorkerPoolDesc::SetThreadLocalStateFcn& setter)
{
    getter = gtsGetThreadState;
    setter = gtsSetThreadState;
}

//------------------------------------------------------------------------------
int32_t Worker::_locateScheduler(LocalScheduler* pLocalScheduler)
{
    for (uint32_t ii = 0; ii < m_registeredSchedulers.size(); ++ii)
    {
        if (m_registeredSchedulers[ii] == pLocalScheduler)
        {
            return ii;
        }
    }
    return -1;
}

//------------------------------------------------------------------------------
void Worker::_schedulerExecutionLoop(SubIdType localWorkerId)
{
    sleep(true);

    bool resetSearchIndex = true;

    GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::WORKERPOOL_DEBUG, analysis::Color::AntiqueWhite, "WORKER EXE LOOP", this, localWorkerId);

    constexpr uint32_t minQuitThreshold = 1;
    TerminationBackoff backoff(m_minSleepCycles, minQuitThreshold);

    Task* pFoundTask = nullptr;

    while (m_pMyPool->isRunning())
    {
        while (!m_pMyPool->m_ishalting.load(memory_order::acquire) && m_pMyPool->isRunning())
        {
            bool exexecutedTask = false;
            bool processScheduler = true;
            bool hasWork = true;
            pFoundTask = nullptr;
            {
                GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::WORKERPOOL_ALL, analysis::Color::AntiqueWhite, "WORKER FIND SCHED", this, localWorkerId);
                m_pCurrentScheduler = _getNextSchedulerRoundRobin(localWorkerId, resetSearchIndex, pFoundTask, exexecutedTask);

                // If no scheduler was found,
                if (m_pCurrentScheduler == nullptr)
                {
                    GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::WORKERPOOL_DEBUG, analysis::Color::Yellow, "WORKER NO SCHED FOUND", this, localWorkerId);
                    processScheduler = false;

#if GTS_USE_BACKOFF

                    // Quit or backoff.
                    if(backoff.tryToQuit(localWorkerId))
                    {
                        // Quit threshold reached. Final check for no work.
                        GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::WORKERPOOL_ALL, analysis::Color::DarkGoldenrod, "LAST CHANCE WORKER FIND SCHED", this, localWorkerId);
                        m_pCurrentScheduler = _getNextSchedulerRoundRobin(localWorkerId, resetSearchIndex, pFoundTask, exexecutedTask);
                        if (m_pCurrentScheduler != nullptr)
                        {
                            processScheduler = true;
                        }
                        else
                        {
                            hasWork = false;
                        }
                    }
#else
                    hasWork = _hasWork();
#endif
                }
            }

            if (exexecutedTask)
            {
                // If a task was executed, reset the backoff.
                backoff.reset();
            }

            if (processScheduler)
            {
                GTS_TRACE_ZONE_MARKER_P3(analysis::CaptureMask::WORKERPOOL_DEBUG, analysis::Color::DarkGreen, "WORKER ENTER SCHED", this, localWorkerId, m_pCurrentScheduler);

                // RUN THE SCHEDULE
#if GTS_USE_BACKOFF
                if (m_pCurrentScheduler->run(pFoundTask))
                {
                    // If a task was executed, reset the backoff.
                    backoff.reset();
                }
#else
                m_pCurrentScheduler->run(pFoundTask);
#endif
                GTS_TRACE_ZONE_MARKER_P3(analysis::CaptureMask::WORKERPOOL_DEBUG, analysis::Color::DarkGreen, "WORKER EXIT SCHED", this, localWorkerId, m_pCurrentScheduler);

                // Mark the scheduler as out-of-use by this thread.
                m_pCurrentScheduler->m_workerAccessMutex.unlock();
                resetSearchIndex = false;
            }
            else
            {
                GTS_ASSERT(m_pCurrentScheduler == nullptr);

                // There are no schedulers with work, so sleep.
                if(!hasWork)
                {
                    GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::WORKERPOOL_DEBUG, analysis::Color::OrangeRed, "WORKER ENTER SLEEP", this, localWorkerId);
                    sleep(false);
                    backoff.resetQuitThreshold(m_minSleepCycles * 2, minQuitThreshold);
                }
                else
                {
                    GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::WORKERPOOL_DEBUG, analysis::Color::YellowGreen, "WORKER STILL HAS WORK", this, localWorkerId);
                }

                resetSearchIndex = true;
            }
        }

        halt();
    }
}

//------------------------------------------------------------------------------
LocalScheduler* Worker::_getNextSchedulerRoundRobin(SubIdType localWorkerId, bool reset, Task*& pFoundTask, bool& executedTask)
{
    GTS_UNREFERENCED_PARAM(localWorkerId);

    if (!m_registeredSchedulers.empty())
    {
        const uint32_t numSchedulers = (uint32_t)m_registeredSchedulers.size();

        uint32_t index = m_currentScheduleIdx + 1;
        if (index >= numSchedulers)
        {
            index = 0;
        }

        GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::WORKERPOOL_DEBUG, analysis::Color::AntiqueWhite, "WORKER SCHED RROBIN", this, localWorkerId);

        LocalScheduler* pLocalScheduler = nullptr;
        if ((pLocalScheduler = _getNextSchedulerRoundRobinLoop(localWorkerId, reset, index, numSchedulers, pFoundTask, executedTask)) != nullptr)
        {
            return pLocalScheduler;
        }

        return _getNextSchedulerRoundRobinLoop(localWorkerId, reset, 0, index, pFoundTask, executedTask);
    }

    return nullptr;
}

//------------------------------------------------------------------------------
LocalScheduler* Worker::_getNextSchedulerRoundRobinLoop(SubIdType localWorkerId, bool reset, uint32_t begin, uint32_t end, Task*& pFoundTask, bool& executedTask)
{
    GTS_UNREFERENCED_PARAM(localWorkerId);

    for (uint32_t ii = begin; ii < end; ++ii)
    {
        GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::WORKERPOOL_DEBUG, analysis::Color::AntiqueWhite, "WORKER SCHED RROBIN IDX", this, ii);

        if (!reset && ii == m_currentScheduleIdx)
        {
            GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::WORKERPOOL_DEBUG, analysis::Color::AntiqueWhite, "WORKER SCHED RROBIN EXHAUSTED", this, ii);
            // We've exhausted the search space.
            return nullptr;
        }

        LocalScheduler* pLocalScheduler = nullptr;
        {
            LockShared<MutexType> lock(*m_pLocalSchedulersMutex);
            if(ii < m_registeredSchedulers.size())
            {
                pLocalScheduler = m_registeredSchedulers[ii];
                pLocalScheduler->m_workerAccessMutex.lock();
            }
            else
            {
                break;
            }
        }

        GTS_TRACE_SCOPED_ZONE_P3(analysis::CaptureMask::WORKERPOOL_DEBUG, analysis::Color::AntiqueWhite, "WORKER CHECK SCHED", this, localWorkerId, pLocalScheduler);

        if (pLocalScheduler != nullptr)
        {
            MicroScheduler* pMicroScheduler = pLocalScheduler->m_pMyScheduler;
            pFoundTask = nullptr;

            if (pMicroScheduler->isActive())
            {
                pFoundTask = pMicroScheduler->_getTask(this, true, false, localWorkerId, executedTask);
                if (!pFoundTask)
                {
                    pFoundTask = pMicroScheduler->_getExternalTask(this, localWorkerId, executedTask);
                }
            }

            if (pFoundTask)
            {
                m_currentScheduleIdx = ii;
                return pLocalScheduler;
            }

            pLocalScheduler->m_workerAccessMutex.unlock();
        }
    }

    return nullptr;
}

//------------------------------------------------------------------------------
bool Worker::_hasWork()
{
    if (!m_registeredSchedulers.empty())
    {
        const uint32_t numSchedulers = (uint32_t)m_registeredSchedulers.size();

        for (uint32_t ii = 0; ii < numSchedulers; ++ii)
        {
            LocalScheduler* pLocalScheduler = nullptr;
            {
                LockShared<MutexType> lock(*m_pLocalSchedulersMutex);
                if(ii < m_registeredSchedulers.size())
                {
                    pLocalScheduler = m_registeredSchedulers[ii];
                    pLocalScheduler->m_workerAccessMutex.lock();
                }
                else
                {
                    break;
                }
            }

            if (pLocalScheduler != nullptr)
            {
                MicroScheduler* pMicroScheduler = pLocalScheduler->m_pMyScheduler;
                if (pMicroScheduler->isActive())
                {
                    if (pMicroScheduler->hasTasks() || pMicroScheduler->hasExternalTasks())
                    {
                        pLocalScheduler->m_workerAccessMutex.unlock();
                        return true;
                    }
                }

                pLocalScheduler->m_workerAccessMutex.unlock();
            }
        }
    }
    return false;
}

} // namespace gts
