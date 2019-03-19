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

#include "gts/platform/Assert.h"
#include "gts/platform/Atomic.h"
#include "gts/containers/Vector.h"
#include "gts/micro_scheduler/MicroSchedulerTypes.h"

namespace gts {

class Worker;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A collection of running Workers that Schedules can be run on. Must be
 *  initialized before use.
 */
class WorkerPool
{
public:

    WorkerPool();

    /**
     * Implicitly calls shutdown.
     */
    ~WorkerPool();

    /**
     * Simple thread count initializer. If threadCount is zero, initializes with
     * hardware thread count.
     */
    bool initialize(uint32_t threadCount = 0);

    /**
     * Explicit initializer.
     */
    bool initialize(WorkerPoolDesc const& desc);

    /**
     * Stops and destroys all workers.
     */
    bool shutdown();

    /**
     * @return True if the scheduler is running, false otherwise.
     */
    GTS_INLINE bool isRunning() const
    {
        return m_isRunning.load(gts::memory_order::acquire);
    }

    /**
     * @return The number of worker threads in the scheduler.
     */
    GTS_INLINE uint32_t workerCount() const
    {
        uint32_t workerCount = 0;
        for (size_t ii = 0; ii < m_totalWorkerCount; ++ii)
        {
            if (m_pWorkerOwnershipByIdx[ii])
            {
                ++workerCount;
            }
        }
        return workerCount;
    }

    /**
     * @return True if this WorkerPool is partitioned from a master WorkerPool.
     */
    GTS_INLINE bool isParition() const
    {
        return m_isPartition;
    }

    /**
     * @return The calling Worker thread's index.
     */
    GTS_INLINE uint32_t thisWorkerIndex() const
    {
        return m_pGetThreadLocalIdxFcn();
    }

    /**
     * @return The current MicroScheduler ID running on this WorkerPool.
     */
    uint32_t thisMicroSchedulerId();

    /**
     * Fills 'out' with the Worker indices assigned to this WorkerPool.
     */
    void getWorkerIndices(Vector<uint32_t>& out);

    /**
     * @return A partition of this WorkerPool with Worker specified by 'workerIndices'.
     */
    WorkerPool* makePartition(Vector<uint32_t> const& workerIndices);

private:

    void _destroyWorkers();

    uint32_t _suspendedWorkerCount() const;
    void _suspendAllWorkers();
    void _wakeWorkers(uint32_t count);

    uint32_t _haltedWorkerCount() const;
    void _haltAllWorkers();
    void _resumeAllWorkers();

    /**
     * Registers a MicroScheduler with the WorkerPool. The WorkerPool will
     * now executed Tasks submitted to this MicroScheduler.
     * @remark
     *  Registering causes all Workers to halt, so do not register on a hot path!
     */
    void _registerScheduler(MicroScheduler* pMicroScheduler);

    /**
     * Unregisters a MicroScheduler from the WorkerPool.
     * @remark
     *  Unregistering causes all Workers to halt, so do not register on a hot path!
     */
    void _unRegisterScheduler(MicroScheduler* pMicroScheduler);


    WorkerPool(WorkerPool const&) {}
    WorkerPool& operator=(WorkerPool const&) { return *this; }

private:

    friend class Worker;
    friend class Schedule;
    friend class MicroScheduler;

    WorkerPoolDesc::GetThreadLocalIdxFcn m_pGetThreadLocalIdxFcn;
    Worker* m_pWorkersByIdx;
    bool* m_pWorkerOwnershipByIdx;
    Vector<MicroScheduler*> m_registeredSchedulers;
    Vector<WorkerPool*> m_partitions;
    uint16_t m_totalWorkerCount;
    Atomic<int16_t> m_runningWorkerCount;
    Atomic<int16_t> m_suspendedThreadCount;
    Atomic<int16_t> m_haltedThreadCount;
    Atomic<bool> m_isRunning;
    Atomic<bool> m_ishalting;
    bool m_isPartition;
};

} // namespace gts
