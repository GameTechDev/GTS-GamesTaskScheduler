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
#include "gts/micro_scheduler/WorkerPool.h"

#include "gts/platform/Utils.h"
#include "gts/platform/Assert.h"
#include "gts/platform/Memory.h"
#include "gts/analysis/Trace.h"
#include "gts/analysis/Counter.h"
#include "gts/synchronization/Lock.h"

#include "gts/micro_scheduler/MicroScheduler.h"

#include "Worker.h"
#include "LocalScheduler.h"

#include <iostream>

namespace gts {

Atomic<uint16_t> WorkerPool::s_nextWorkerPoolId = { 0 };

//------------------------------------------------------------------------------
WorkerPool::WorkerPool()
    : m_pWorkersByIdx(nullptr)
    , m_pRegisteredSchedulers(nullptr)
    , m_pGetThreadLocalStateFcn(nullptr)
    , m_pSetThreadLocalStateFcn(nullptr)
    , m_poolId(UINT16_MAX)
    , m_workerCount(0)
    , m_sleepingWorkerCount(0)
    , m_haltedWorkerCount(0)
    , m_isRunning(false)
    , m_ishalting(false)
    , m_debugName()
{}

//------------------------------------------------------------------------------
WorkerPool::~WorkerPool()
{
    if (isRunning())
    {
        shutdown();
    }
}

//------------------------------------------------------------------------------
bool WorkerPool::initialize(uint32_t threadCount)
{
    if (threadCount == 0)
    {
        threadCount = gts::Thread::getHardwareThreadCount();
    }

    WorkerPoolDesc desc;
    desc.workerDescs.resize(threadCount);
    return initialize(desc);
}

//------------------------------------------------------------------------------
bool WorkerPool::initialize(WorkerPoolDesc& desc)
{
    m_poolId = s_nextWorkerPoolId.fetch_add(1, memory_order::acquire);
    GTS_ASSERT(m_poolId != UINT16_MAX && "ID overflow"); // just in case anyone gets pool happy.

    GTS_TRACE_SCOPED_ZONE_P1(analysis::CaptureMask::WORKERPOOL_DEBUG, analysis::Color::AntiqueWhite, "WORKERPOOL INIT SHUTDOWN", m_poolId);

    GTS_ASSERT(desc.workerDescs.size() < INT16_MAX);
    const uint16_t workerCount = (uint16_t)desc.workerDescs.size();
    memcpy(m_debugName, desc.name, DESC_NAME_SIZE);

    if (workerCount <= 0)
    {
        GTS_ASSERT(0 && "m_workerCount <= 0");
    }
    m_workerCount = workerCount;
    
    if (!_initWorkers(desc))
    {
        return false;
    }

    m_pRegisteredSchedulers = alignedNew<RegisteredSchedulers, GTS_CACHE_LINE_SIZE>();

    return true;
}

//------------------------------------------------------------------------------
bool WorkerPool::_initWorkers(WorkerPoolDesc& desc)
{
    GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::WORKERPOOL_DEBUG, analysis::Color::AntiqueWhite, "WORKERPOOL INIT WORKERS", this, m_poolId);

    // Create the Workers.
    m_pWorkersByIdx = gts::alignedVectorNew<Worker, GTS_CACHE_LINE_SIZE>(m_workerCount);

    // Mark the scheduler as running.
    m_isRunning.store(true, gts::memory_order::release);

    const uint32_t masterWorkerId = 0;

    // Initialize the TLS functions.
    if (desc.pGetThreadLocalStateFcn != nullptr)
    {
        GTS_ASSERT(desc.pSetThreadLocalStateFcn != nullptr);
        m_pGetThreadLocalStateFcn = desc.pGetThreadLocalStateFcn;
        m_pSetThreadLocalStateFcn = desc.pSetThreadLocalStateFcn;
    }
    else
    {
        Worker::_initTlsGetAndSet(
            m_pGetThreadLocalStateFcn,
            m_pSetThreadLocalStateFcn);
    }   

    if (Worker::s_pGetThreadLocalStateFcn == nullptr)
    {
        Worker::s_pGetThreadLocalStateFcn = m_pGetThreadLocalStateFcn;
    }
    Worker::s_pGetThreadLocalStateFcnRefCount.fetch_add(1, memory_order::release);
    
    // Initialize the MASTER Worker.
    if (!m_pWorkersByIdx[masterWorkerId].initialize(
        this,
        OwnedId(m_poolId, masterWorkerId),
        WorkerThreadDesc::GroupAndAffinity(),
        Thread::Priority::PRIORITY_NORMAL,
        desc.workerDescs[0].name,
        desc.workerDescs[0].pUserData,
        desc.pGetThreadLocalStateFcn,
        desc.pSetThreadLocalStateFcn,
        desc.pVisitor,
        desc.cachableTaskSize,
        desc.initialTaskCountPerWorker,
        true))
    {
        return false;
    }

    // Initialize the Workers
    for (SubIdType workerId = masterWorkerId + 1; workerId < m_workerCount; ++workerId)
    {
        GTS_TRACE_SCOPED_ZONE_P3(analysis::CaptureMask::WORKERPOOL_ALL, analysis::Color::AntiqueWhite, "WORKERPOOL INIT WORKER", this, m_poolId, workerId);

        if (!m_pWorkersByIdx[workerId].initialize(
            this,
            OwnedId(m_poolId, workerId),
            desc.workerDescs[workerId].affinity,
            desc.workerDescs[workerId].priority,
            desc.workerDescs[workerId].name,
            desc.workerDescs[workerId].pUserData,
            desc.pGetThreadLocalStateFcn,
            desc.pSetThreadLocalStateFcn,
            desc.pVisitor,
            desc.cachableTaskSize,
            desc.initialTaskCountPerWorker))
        {
            return false;
        }
    }

    return true;
}

//------------------------------------------------------------------------------
bool WorkerPool::shutdown()
{
    if (isRunning())
    {
        GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::WORKERPOOL_DEBUG, analysis::Color::AntiqueWhite, "WORKERPOOL SHUTDOWN", this, m_poolId);

        _haltAllWorkers(); //-------------------

        // Unregister all the registered MicroSchedulers.
        _unRegisterAllSchedulers();

        alignedDelete(m_pRegisteredSchedulers);
        m_pRegisteredSchedulers = nullptr;

        // Make master Worker unknown.
        uintptr_t state = m_pGetThreadLocalStateFcn();
        GTS_ASSERT(state && "Are you shutting down the WorkerPool on a different threads that it was initialized on?");
        if(((Worker*)state)->m_refCount.fetch_sub(1, memory_order::acq_rel) - 1 == 0)
        {
            m_pSetThreadLocalStateFcn(0);
        }
        
        // Analysis dump. Must happen before the workers are destroyed.
        char filename[128];
        #ifdef GTS_MSVC
            sprintf_s(filename, "WorkerPool_Analysis_%d.txt", m_poolId);
        #else
            snprintf(filename, 128, "WorkerPool_Analysis_%d.txt", m_poolId);
        #endif

        GTS_WP_COUNTER_DUMP_TO(m_poolId, filename);

        // Destroy all the Workers.
        _destroyWorkers();

        if (Worker::s_pGetThreadLocalStateFcnRefCount.fetch_sub(1, memory_order::acq_rel) - 1 == 0)
        {
            Worker::s_pGetThreadLocalStateFcn = nullptr;
        }

        //
        // Reclaim memory

        gts::alignedVectorDelete(m_pWorkersByIdx, m_workerCount);
        m_pWorkersByIdx = nullptr;

        m_sleepingWorkerCount.store(0, memory_order::release);

        GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::WORKERPOOL_DEBUG, analysis::Color::AntiqueWhite, "WORKERPOOL DESTROYED", this, m_poolId);
    }

    return true;
}

//------------------------------------------------------------------------------
OwnedId WorkerPool::thisWorkerId() const
{
    uintptr_t state = m_pGetThreadLocalStateFcn();
    if (state)
    {
        return ((Worker*)state)->m_id;
    }
    else
    {
        return OwnedId();
    }
}

//------------------------------------------------------------------------------
void WorkerPool::_destroyWorkers()
{
    GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::WORKERPOOL_DEBUG, analysis::Color::AntiqueWhite, "WORKERPOOL INIT WORKER", this, m_poolId);

    // Signal all Workers to quit.
    m_isRunning.store(false, gts::memory_order::release);

    // Wake up suspended workers so they exit.
    _resumeAllWorkers();

    // Shutdown all the workers.
    for (uint32_t thread = 0; thread < m_workerCount; ++thread)
    {
        m_pWorkersByIdx[thread].shutdown();
    }
}

//------------------------------------------------------------------------------
MicroScheduler* WorkerPool::currentMicroScheduler() const
{
    uintptr_t state = m_pGetThreadLocalStateFcn();
    if (state == 0)
    {
        GTS_ASSERT(0);
        return nullptr;
    }

    return ((Worker*)state)->currentMicroScheduler();
}

//------------------------------------------------------------------------------
void WorkerPool::enumerateWorkerIds(Vector<OwnedId>& out) const
{
    GTS_ASSERT(out.empty());
    out.clear();
    for (uint32_t ii = 0; ii < m_workerCount; ++ii)
    {
        out.push_back(m_pWorkersByIdx[ii].id());
    }
}

//------------------------------------------------------------------------------
void WorkerPool::enumerateWorkerTids(Vector<ThreadId>& out) const
{
    GTS_ASSERT(out.empty());
    out.clear();
    for (uint32_t ii = 0; ii < m_workerCount; ++ii)
    {
        out.push_back(m_pWorkersByIdx[ii].tid());
    }
}

//------------------------------------------------------------------------------
void WorkerPool::resetIdGenerator()
{
    s_nextWorkerPoolId.store(0, memory_order::release);
}

//------------------------------------------------------------------------------
void WorkerPool::_wakeWorker(Worker* pThisWorker, uint32_t count, bool reset)
{
    GTS_TRACE_SCOPED_ZONE_P3(analysis::CaptureMask::WORKERPOOL_ALL, analysis::Color::Orange2, "WORKERPOOL WAKE WORKER", this, m_poolId, pThisWorker->id().uid());
    GTS_WP_COUNTER_INC(pThisWorker->id(), gts::analysis::WorkerPoolCounters::NUM_WAKE_CALLS);

    GTS_ASSERT(count > 0);

    // Make sure we have suspended threads before entering the more expensive
    // suspend process.
    if (m_sleepingWorkerCount.load(gts::memory_order::acquire) > 0)
    {
        uint32_t startIdx = 1; // <- start at 1 because the master thread #0 does not suspend and will not resume.
        OwnedId thisWorkerId;
        if(pThisWorker)
        {
            startIdx = fastRand(pThisWorker->m_randState) % m_workerCount;
            if(startIdx == 0)
            {
                ++startIdx;
            }
            thisWorkerId = pThisWorker->id();
        }

        // Search through all the Workers for a sleeping Worker.
        if (!_wakeWorkerLoop(startIdx, m_workerCount, thisWorkerId.localId(), count, reset))
        {
            _wakeWorkerLoop(1, startIdx, thisWorkerId.localId(), count, reset);
        }
    }
}

//------------------------------------------------------------------------------
bool WorkerPool::_wakeWorkerLoop(uint32_t startIdx, uint32_t endIdx, SubIdType thisWorkerId, uint32_t count, bool reset)
{
    for (uint32_t ii = startIdx; ii < endIdx; ++ii) 
    {
        if (ii == thisWorkerId)
        {
            continue; // already awake!
        }

        Worker& worker = m_pWorkersByIdx[ii];
        if (worker.wake(count, reset, false))
        {
            return true;
        }
        GTS_SPECULATION_FENCE();
    }
    return false;
}

//------------------------------------------------------------------------------
uint32_t WorkerPool::_haltedWorkerCount() const
{
    // Count the workers in a halted state.
    uint32_t totalhaltedThreads = 0;
    for (uint32_t ii = 1; ii < m_workerCount; ++ii) // ii = 1. ignore master
    {
        // Slow, but needs to be atomic.
        if (m_pWorkersByIdx[ii].isHaulted())
        {
            ++totalhaltedThreads;
        }
    }
    return totalhaltedThreads;
}

//------------------------------------------------------------------------------
void WorkerPool::_haltAllWorkers()
{
    GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::WORKERPOOL_ALL, analysis::Color::AntiqueWhite, "WORKERPOOL HALT WORKERS", this, m_poolId);
    GTS_WP_COUNTER_INC(thisWorkerId(), gts::analysis::WorkerPoolCounters::NUM_HALTS_SIGNALED);

    uintptr_t state = Worker::getLocalState();
    Worker* pWorker = (Worker*)state;

    // halt this pool
    m_ishalting.store(true, gts::memory_order::seq_cst);

    // Count all the workers to be halted.
    uint32_t totalWorkerCount = workerCount();
    totalWorkerCount -= 1; // ignore master thread.

    // Wait for all the Workers to halt.
    uint32_t totalhaltedThreadCount = 0;
    do
    {
        // Get all workers our of their suspended state.
        _wakeWorker(pWorker, m_workerCount, true);

        // Count the workers in a halted state.
        totalhaltedThreadCount = _haltedWorkerCount();

        GTS_PAUSE();

    } while (totalhaltedThreadCount < totalWorkerCount);

    GTS_ASSERT(totalhaltedThreadCount == totalWorkerCount);
}

//------------------------------------------------------------------------------
void WorkerPool::_resumeAllWorkers()
{
    GTS_TRACE_SCOPED_ZONE_P2(analysis::CaptureMask::WORKERPOOL_ALL, analysis::Color::AntiqueWhite, "WORKERPOOL RESUME WORKERS", this, m_poolId);

    m_ishalting.store(false, gts::memory_order::release);

    // Get the total Worker count.
    uint32_t totalhaltedThreadCount = 0;

    // Wait for all the Workers to resume.
    do
    {
        // Get all workers our of their suspend state.
        for (volatile size_t ii = 1; ii < m_workerCount; ++ii)  // ii = 1. ignore master
        {
            GTS_WP_COUNTER_INC(thisWorkerId(), gts::analysis::WorkerPoolCounters::NUM_RESUMES);
            Worker& worker = m_pWorkersByIdx[ii];
            worker.resume();
        }

        // Count the halted workers so far.
        totalhaltedThreadCount = m_haltedWorkerCount.load(gts::memory_order::acquire);

        GTS_PAUSE();

    } while (totalhaltedThreadCount > 0);
}

//------------------------------------------------------------------------------
bool WorkerPool::_unRegisterScheduler(MicroScheduler* pMicroScheduler)
{
    GTS_ASSERT(pMicroScheduler != nullptr);

    bool foundScheduler = false;

    size_t ii = 0;

    for (; ii < m_pRegisteredSchedulers->schedulers.size(); ++ii)
    {
        if (m_pRegisteredSchedulers->schedulers[ii] == pMicroScheduler)
        {
            foundScheduler = true;
            break;
        }
    }

    if (foundScheduler)
    {
        pMicroScheduler->_unRegisterFromWorkers(this);

        if (ii != m_pRegisteredSchedulers->schedulers.size() - 1)
        {
            // swap out
            MicroScheduler* back = m_pRegisteredSchedulers->schedulers.back();
            m_pRegisteredSchedulers->schedulers.pop_back();
            m_pRegisteredSchedulers->schedulers[ii] = back;
        }
        else
        {
            m_pRegisteredSchedulers->schedulers.pop_back();
        }

        return true;
    }
    else
    {
        GTS_ASSERT(0 && "pMicroScheduler is not registered.");
        return false;
    }
}

//------------------------------------------------------------------------------
void WorkerPool::_unRegisterAllSchedulers()
{
    Lock<MutexType> lock(m_pRegisteredSchedulers->mutex);

    while(!m_pRegisteredSchedulers->schedulers.empty())
    {
        MicroScheduler* pMicroScheduler = m_pRegisteredSchedulers->schedulers.back();
        GTS_ASSERT(pMicroScheduler != nullptr);

        pMicroScheduler->_unRegisterFromWorkerPool(this, false);

        //m_pRegisteredSchedulers->schedulers.pop_back();
    }
}

} // namespace gts
