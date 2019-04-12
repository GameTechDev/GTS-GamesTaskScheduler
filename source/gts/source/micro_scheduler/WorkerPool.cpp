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
#include "gts/analysis/Instrumenter.h"
#include "gts/analysis/Analyzer.h"

#include "gts/micro_scheduler/MicroScheduler.h"

#include "Worker.h"

#include <iostream>

namespace gts {

//------------------------------------------------------------------------------
WorkerPool::WorkerPool()
    : m_pGetThreadLocalIdxFcn(nullptr)
    , m_pWorkersByIdx(nullptr)
    , m_pWorkerOwnershipByIdx(nullptr)
    , m_totalWorkerCount(0)
    , m_runningWorkerCount(0)
    , m_suspendedThreadCount(0)
    , m_haltedThreadCount(0)
    , m_isRunning(false)
    , m_ishalting(false)
    , m_isPartition(false)
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
bool WorkerPool::initialize(WorkerPoolDesc const& desc)
{
    GTS_INSTRUMENTER_SCOPED(analysis::Tag::INTERNAL, "WorkerPool Init", 0, 0);

    GTS_ASSERT(desc.workerDescs.size() < INT16_MAX);
    const uint16_t workerCount = (uint16_t)desc.workerDescs.size();

    GTS_ANALYSIS_START(workerCount);

    if (workerCount <= 0)
    {
        GTS_ASSERT(0 && "m_totalWorkerCount <= 0");
    }

    m_totalWorkerCount = workerCount;

    // Create the Workers.
    m_pWorkersByIdx = gts::alignedVectorNew<Worker, GTS_CACHE_LINE_SIZE>(workerCount);
    m_pWorkerOwnershipByIdx = gts::alignedVectorNew<bool, GTS_NO_SHARING_CACHE_LINE_SIZE>(m_totalWorkerCount);

    // Mark the scheduler as running.
    m_isRunning.store(true, gts::memory_order::seq_cst);

    const uint32_t masterWorkerIdx = 0;

    // Initialize the MASTER Worker.
    if (!m_pWorkersByIdx[masterWorkerIdx].initialize(
        this,
        masterWorkerIdx,
        desc.workerDescs[masterWorkerIdx].affinityMask,
        desc.workerDescs[masterWorkerIdx].priority,
        desc.pGetThreadLocalIdxFcn,
        desc.pSetThreadLocalIdxFcn,
        true))
    {
        return false;
    }

    m_pWorkerOwnershipByIdx[masterWorkerIdx] = true;

    // Initialize the Workers
    for (uint32_t workerIdx = masterWorkerIdx + 1; workerIdx < workerCount; ++workerIdx)
    {
        GTS_INSTRUMENTER_SCOPED(analysis::Tag::INTERNAL, "WorkerPool InitWorker", workerIdx, 0);
        if (!m_pWorkersByIdx[workerIdx].initialize(
            this,
            workerIdx,
            desc.workerDescs[workerIdx].affinityMask,
            desc.workerDescs[workerIdx].priority,
            desc.pGetThreadLocalIdxFcn,
            desc.pSetThreadLocalIdxFcn))
        {
            return false;
        }

        m_pWorkerOwnershipByIdx[workerIdx] = true;
    }

    return true;
}

//------------------------------------------------------------------------------
bool WorkerPool::shutdown()
{
    if (m_isPartition)
    {
        GTS_ASSERT(0 && "Cannot shutdown a parition.");
        return false;
    }

    if (isRunning())
    {
        GTS_INSTRUMENTER_SCOPED(analysis::Tag::INTERNAL, "WorkerPool Shutdown", m_pGetThreadLocalIdxFcn(), 0);

        //
        // Shutdown all the registered MicroSchedulers.

        for (size_t shedulerIdx = 0; shedulerIdx < m_registeredSchedulers.size(); ++shedulerIdx)
        {
            MicroScheduler* pMicroScheduler = m_registeredSchedulers[shedulerIdx];
            if (pMicroScheduler)
            {
                pMicroScheduler->shutdown();
            }
        }

        _haltAllWorkers(); //-------------------

        //
        // Reclaim WorkerPool partitions. Must happen before destroying threads.

        for (size_t partitionIdx = 0; partitionIdx < m_partitions.size(); ++partitionIdx)
        {
            WorkerPool* pPartition = m_partitions[partitionIdx];

            // Stop running becuase we don't want shutdown to be called on delete.
            pPartition->m_isRunning.store(false, gts::memory_order::release);

            for (size_t workerIdx = 0; workerIdx < m_totalWorkerCount; ++workerIdx)
            {
                if (pPartition->m_pWorkerOwnershipByIdx[workerIdx])
                {
                    m_haltedThreadCount.fetch_add(1);
                    m_pWorkerOwnershipByIdx[workerIdx] = true;
                    m_pWorkersByIdx[workerIdx].m_pMyPool = this;
                }
            }

            gts::alignedVectorDelete(pPartition->m_pWorkerOwnershipByIdx, m_totalWorkerCount);
            delete pPartition;
        }

        //
        // After reclaim

        // Stop tracking WorkerPool partitions
        m_partitions.clear();

        // Destroy all the Workers.
        _destroyWorkers();

        //
        // Reclaim memory

        gts::alignedVectorDelete(m_pWorkerOwnershipByIdx, m_totalWorkerCount);
        m_pWorkerOwnershipByIdx = nullptr;

        gts::alignedVectorDelete(m_pWorkersByIdx, m_totalWorkerCount);
        m_pWorkersByIdx = nullptr;

        m_suspendedThreadCount.store(0);
        m_runningWorkerCount.store(0);
    }

    GTS_ANALYSIS_STOP("analysis_dump.txt");

    GTS_INSTRUMENTER_MARKER(analysis::Tag::INTERNAL, "WorkerPool Destroyed", m_pGetThreadLocalIdxFcn(), 0);

    return true;
}

//------------------------------------------------------------------------------
void WorkerPool::_destroyWorkers()
{
    GTS_INSTRUMENTER_SCOPED(analysis::Tag::INTERNAL, "WorkerPool DestroyWorkers", m_pGetThreadLocalIdxFcn(), 0);

    // Signal all Workers to quit.
    m_isRunning.store(false, gts::memory_order::release);

    // Wake up suspended workers so they exit.
    _resumeAllWorkers();

    // Close all threads.
    for (uint32_t thread = 1; thread < m_totalWorkerCount; ++thread)
    {
        m_pWorkersByIdx[thread].shutdown();
    }
}

//------------------------------------------------------------------------------
uint32_t WorkerPool::thisMicroSchedulerId()
{
    uint32_t workerIdx = m_pGetThreadLocalIdxFcn();
    GTS_ASSERT(workerIdx != gts::UNKNOWN_WORKER_IDX);

    uint32_t schedulerIdx = m_pWorkersByIdx[workerIdx].m_currentSchedulerIdx % m_registeredSchedulers.size();
    return schedulerIdx;
}

//------------------------------------------------------------------------------
void WorkerPool::getWorkerIndices(Vector<uint32_t>& out)
{
    out.clear();

    for (size_t ii = 0; ii < m_totalWorkerCount; ++ii)
    {
        if (m_pWorkerOwnershipByIdx[ii])
        {
            out.push_back((uint32_t)ii);
        }
    }
}

//------------------------------------------------------------------------------
WorkerPool* WorkerPool::makePartition(Vector<uint32_t> const& workerIndices)
{
    if (!isRunning())
    {
        GTS_ASSERT(0 && "The WorkerPool cannot be running.");
        return nullptr;
    }

    if (m_isPartition)
    {
        GTS_ASSERT(0 && "Cannot partition a partition.");
        return nullptr;
    }

    GTS_INSTRUMENTER_SCOPED(analysis::Tag::INTERNAL, "WorkerPool Make Partition", 0, 0);

    _haltAllWorkers();

    WorkerPool* pPartition = new WorkerPool;
    m_partitions.push_back(pPartition);

    pPartition->m_pGetThreadLocalIdxFcn = m_pGetThreadLocalIdxFcn;
    pPartition->m_pWorkersByIdx         = m_pWorkersByIdx;
    pPartition->m_pWorkerOwnershipByIdx = gts::alignedVectorNew<bool, GTS_NO_SHARING_CACHE_LINE_SIZE>(m_totalWorkerCount);
    pPartition->m_totalWorkerCount      = m_totalWorkerCount;
    pPartition->m_registeredSchedulers  = m_registeredSchedulers;

    pPartition->m_suspendedThreadCount.store(0, gts::memory_order::relaxed);
    pPartition->m_haltedThreadCount.store(0, gts::memory_order::relaxed);

    pPartition->m_isRunning.store(true, gts::memory_order::relaxed);
    pPartition->m_ishalting.store(true, gts::memory_order::relaxed);

    pPartition->m_isPartition = true;

    for (size_t ii = 0; ii < workerIndices.size(); ++ii)
    {
        size_t index = workerIndices[ii];
        if (index == 0)
        {
            GTS_ASSERT(0 && "Cannot use main thread in a partition.");
            goto fail;
        }
        if (index >= m_totalWorkerCount)
        {
            GTS_ASSERT(0 && "Worker index out of range.");
            goto fail;
        }
        if (!m_pWorkerOwnershipByIdx[ii])
        {
            GTS_ASSERT(0 && "Worker index belongs to another parition.");
            goto fail;
        }

        pPartition->m_pWorkerOwnershipByIdx[index] = true;

        // Tally this halted Worker.
        pPartition->m_haltedThreadCount.fetch_add(1);
    }

    // Remove the Worker form the schduler. Do this after setting up the partition
    // in case it fails.
    for (size_t ii = 0; ii < workerIndices.size(); ++ii)
    {
        // Remove the Worker form the schduler.
        size_t index = workerIndices[ii];
        m_pWorkerOwnershipByIdx[index] = false;

        // Untally this halted Worker.
        m_haltedThreadCount.fetch_sub(1);
    }

    // Point the Parition's Workers to the Parition.
    for (size_t ii = 0; ii < m_totalWorkerCount; ++ii)
    {
        if (pPartition->m_pWorkerOwnershipByIdx[ii])
        {
            pPartition->m_pWorkersByIdx[ii].m_pMyPool = pPartition;
        }
    }

    _resumeAllWorkers();
    return pPartition;

fail:

    delete pPartition;
    _resumeAllWorkers();
    return nullptr;
}

//------------------------------------------------------------------------------
uint32_t WorkerPool::_suspendedWorkerCount() const
{
    // Count the workers in a suspended state.
    uint32_t totalSuspendedThreads = 0;
    for (uint32_t ii = 1; ii < m_totalWorkerCount; ++ii) // ii = 1. ignore master
    {
        // Slow, but needs to be atomic.
        if (m_pWorkerOwnershipByIdx[ii] && m_pWorkersByIdx[ii].m_suspendSemaphore.isWaiting())
        {
            ++totalSuspendedThreads;
        }
    }
    return totalSuspendedThreads;
}

//------------------------------------------------------------------------------
void WorkerPool::_suspendAllWorkers()
{
    //
    // Count all the workers to be suspended.

    uint32_t totalWorkerCount = workerCount();
    if (!m_isPartition)
    {
        totalWorkerCount -= 1; // ignore master thread.
    }
    for (size_t ii = 0; ii < m_partitions.size(); ++ii)
    {
        totalWorkerCount += m_partitions[ii]->workerCount();
    }

    //
    // Wait for all the Workers to suspend.

    uint32_t totalSuspendedThreadCount = 0;
    do
    {
        // Count the workers in a suspended state.
        totalSuspendedThreadCount = _suspendedWorkerCount();
        for (size_t ii = 0; ii < m_partitions.size(); ++ii)
        {
            totalSuspendedThreadCount += m_partitions[ii]->_suspendedWorkerCount();
        }

        GTS_PAUSE();

    } while (totalSuspendedThreadCount < totalWorkerCount);
}

//------------------------------------------------------------------------------
void WorkerPool::_wakeWorkers(uint32_t count)
{
    GTS_ANALYSIS_COUNT(thisWorkerIndex(), gts::analysis::AnalysisType::NUM_WAKE_CALLS);
    GTS_ANALYSIS_TIME_SCOPED(thisWorkerIndex(), gts::analysis::AnalysisType::NUM_WAKE_CALLS);
    GTS_INSTRUMENTER_SCOPED(analysis::Tag::INTERNAL, "Wake Workers", thisWorkerIndex(), 0);

    // Make sure we have suspeneded threads before entering the more expensive
    // suspend process.
    if (m_suspendedThreadCount.load(gts::memory_order::acquire) > 0)
    {
        // Only wake up the first Worker. Each resumed Worker will wake up the
        // next. This distribute the resume load over the Workers.
        for (uint32_t ii = 1; ii < m_totalWorkerCount; ++ii) // <- start at 1 because the main thread #0 does not suspend and will not resume.
        {
            if (m_pWorkerOwnershipByIdx[ii])
            {
                Worker& worker = m_pWorkersByIdx[ii];
                if(worker.m_isSuspendedWeak)
                {
                    worker.wake(count);
                    break;
                }
            }
        }
    }

    // Wake any partitions.
    for (size_t ii = 0; ii < m_partitions.size(); ++ii)
    {
        m_partitions[ii]->_wakeWorkers(count);
    }
}

//------------------------------------------------------------------------------
uint32_t WorkerPool::_haltedWorkerCount() const
{
    // Count the workers in a halted state.
    uint32_t totalhaltedThreads = 0;
    for (uint32_t ii = 1; ii < m_totalWorkerCount; ++ii) // ii = 1. ignore master
    {
        // Slow, but needs to be atomic.
        if (m_pWorkerOwnershipByIdx[ii] && m_pWorkersByIdx[ii].m_haltSemaphore.isWaiting())
        {
            ++totalhaltedThreads;
        }
    }
    return totalhaltedThreads;
}

//------------------------------------------------------------------------------
void WorkerPool::_haltAllWorkers()
{
    GTS_ANALYSIS_COUNT(thisWorkerIndex(), gts::analysis::AnalysisType::NUM_haltS_SIGNALED);
    GTS_ANALYSIS_TIME_SCOPED(thisWorkerIndex(), gts::analysis::AnalysisType::NUM_haltS_SIGNALED);

    //
    // halt this pool and all its partitions.

    m_ishalting.store(true, gts::memory_order::seq_cst);
    for (size_t ii = 0; ii < m_partitions.size(); ++ii)
    {
        m_partitions[ii]->m_ishalting.store(true, gts::memory_order::seq_cst);
    }

    //
    // Count all the workers to be halted.

    uint32_t totalWorkerCount = workerCount();
    if (!m_isPartition)
    {
        totalWorkerCount -= 1; // ignore master thread.
    }
    for (size_t ii = 0; ii < m_partitions.size(); ++ii)
    {
        totalWorkerCount += m_partitions[ii]->workerCount();
    }

    //
    // Wait for all the Workers to halt.

    uint32_t totalhaltedThreadCount = 0;
    do
    {
        // Get all workers our of their suspended state.
        _wakeWorkers(1);
        for (size_t ii = 0; ii < m_partitions.size(); ++ii)
        {
            m_partitions[ii]->_wakeWorkers(1);
        }

        // Count the workers in a halted state.
        totalhaltedThreadCount = _haltedWorkerCount();
        for (size_t ii = 0; ii < m_partitions.size(); ++ii)
        {
            totalhaltedThreadCount += m_partitions[ii]->_haltedWorkerCount();
        }

        GTS_PAUSE();

    } while (totalhaltedThreadCount < totalWorkerCount);

    GTS_ASSERT(totalhaltedThreadCount == totalWorkerCount);
}

//------------------------------------------------------------------------------
void WorkerPool::_resumeAllWorkers()
{
    m_ishalting.store(false, gts::memory_order::release);
    for (size_t ii = 0; ii < m_partitions.size(); ++ii)
    {
        m_partitions[ii]->m_ishalting.store(false, gts::memory_order::release);
    }

    // Get the total Worker count.
    uint32_t totalhaltedThreadCount = 0;

    // Wait for all the Workers to resume.
    do
    {
        // Get all workers our of their suspend state.
        for (volatile size_t ii = 1; ii < m_totalWorkerCount; ++ii)
        {
            GTS_ANALYSIS_COUNT(thisWorkerIndex(), gts::analysis::AnalysisType::NUM_RESUME_CHECKS);
            GTS_ANALYSIS_TIME_SCOPED(thisWorkerIndex(), gts::analysis::AnalysisType::NUM_RESUME_CHECKS);

            if (m_pWorkerOwnershipByIdx[ii])
            {
                Worker& worker = m_pWorkersByIdx[ii];
                GTS_ANALYSIS_COUNT(thisWorkerIndex(), gts::analysis::AnalysisType::NUM_RESUME_SUCCESSES);
                GTS_ANALYSIS_TIME_SCOPED(thisWorkerIndex(), gts::analysis::AnalysisType::NUM_RESUME_SUCCESSES);
                worker.m_haltSemaphore.signal();
            }
        }

        // Count the halted workers so far.
        totalhaltedThreadCount = m_haltedThreadCount.load(gts::memory_order::acquire);

        GTS_PAUSE();

    } while (totalhaltedThreadCount > 0);
}

//------------------------------------------------------------------------------
void WorkerPool::_registerScheduler(MicroScheduler* pMicroScheduler)
{
    _haltAllWorkers();

    m_registeredSchedulers.push_back(pMicroScheduler);

    _resumeAllWorkers();
}

//------------------------------------------------------------------------------
void WorkerPool::_unRegisterScheduler(MicroScheduler* pMicroScheduler)
{
    _haltAllWorkers();

    bool foundScheduler = false;

    size_t ii = 0;
    for (; ii < m_registeredSchedulers.size(); ++ii)
    {
        if (m_registeredSchedulers[ii] == pMicroScheduler)
        {
            foundScheduler = true;
            break;
        }
    }

    if (foundScheduler)
    {
        if (ii != m_registeredSchedulers.size() - 1)
        {
            // swap out
            MicroScheduler* back = m_registeredSchedulers.back();
            m_registeredSchedulers.pop_back();
            m_registeredSchedulers[ii] = back;
        }
        else
        {
            m_registeredSchedulers.pop_back();
        }
    }

    _resumeAllWorkers();
}

} // namespace gts
