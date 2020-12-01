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
#include "gts/containers/parallel/BinnedAllocator.h"
#include "gts/micro_scheduler/MicroSchedulerTypes.h"

namespace gts {

/** 
 * @addtogroup MicroScheduler
 * @{
 */

class Worker;
class AllocatorManager;

#ifdef GTS_MSVC
#pragma warning(push)
#pragma warning(disable : 4324) // alignment padding warning
#endif

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A collection of running Worker threads that a MicroScheduler can be run on.
 * @remark
 *  Must be initialized before use.
 */
class WorkerPool
{
public: // CREATORS:

    /**
     * Constructs a WorkerPool in an uninitialized state. The user must call
     * WorkerPool::initialize to initialize the scheduler before use, otherwise
     * calls to this WorkerPool are undefined.
     */
    WorkerPool();

    /**
     * Implicitly calls shutdown.
     */
    ~WorkerPool();

public: // LIFETIME:

    /**
     * Simple thread count initializer. If threadCount is zero, initializes with
     * hardware thread count.
     */
    bool initialize(uint32_t threadCount = 0);

    /**
     * Explicit initializer.
     */
    bool initialize(WorkerPoolDesc& desc);

    /**
     * Stops and destroys all workers.
     */
    bool shutdown();


public: // ACCESSORS:

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
        return m_workerCount;
    }

    /**
     * @return The calling Worker thread's index.
     */
    OwnedId thisWorkerId() const;

    /**
     * @return This WorkerPool's ID.
     */
    GTS_INLINE SubIdType poolId() const
    {
        return m_poolId;
    }

    /**
     * @return A vector of all Worker IDs in the pool.
     */
    void enumerateWorkerIds(Vector<OwnedId>& out) const;

    /**
     * @return A vector of all Worker OS Thread IDs in the pool, indexed by local ID.
     */
    void enumerateWorkerTids(Vector<ThreadId>& out) const;

    /**
     * @return The current MicroScheduler running on this Worker thread.
     * @remark The return MicroScheduler pointer is not thread safe.
     */
    MicroScheduler* currentMicroScheduler() const;

public: // MUTATORS:

    /**
     * @brief
     *  Resets the ID generator to 0. This is not safe if there are any
     *  living MicroSchedulers.
     */
    static void resetIdGenerator();

private: // PRIVATE METHODS:

    bool _initWorkers(WorkerPoolDesc& desc);
    void _destroyWorkers();

    void _wakeWorker(Worker* pThisWorker, uint32_t count, bool reset);
    bool _wakeWorkerLoop(uint32_t startIdx, uint32_t endIdx, SubIdType thisWorkerId, uint32_t count, bool reset);

    uint32_t _haltedWorkerCount() const;
    void _haltAllWorkers();
    void _resumeAllWorkers();

    /**
     * Unregisters a MicroScheduler from the WorkerPool.
     */
    bool _unRegisterScheduler(MicroScheduler* pMicroScheduler);
    void _unRegisterAllSchedulers();

    WorkerPool(WorkerPool const&) = delete;
    WorkerPool& operator=(WorkerPool const&) = delete;

private:

    friend class Worker;
    friend class LocalScheduler;
    friend class MicroScheduler;

    using MutexType = UnfairSpinMutex<Backoff<BackoffGrowth::Geometric, true>>;

    struct RegisteredSchedulers
    {
        MutexType mutex;
        Vector<MicroScheduler*> schedulers;
    };

    Worker* m_pWorkersByIdx;
    RegisteredSchedulers* m_pRegisteredSchedulers;
    WorkerPoolDesc::GetThreadLocalStateFcn m_pGetThreadLocalStateFcn;
    WorkerPoolDesc::SetThreadLocalStateFcn m_pSetThreadLocalStateFcn;
    uint16_t m_poolId;
    uint16_t m_workerCount;
    Atomic<int16_t> m_sleepingWorkerCount;
    Atomic<int16_t> m_haltedWorkerCount;
    Atomic<bool> m_isRunning;
    Atomic<bool> m_ishalting;
    char m_debugName[DESC_NAME_SIZE];

    static Atomic<uint16_t> s_nextWorkerPoolId;
};

#ifdef GTS_MSVC
#pragma warning(pop)
#endif

/** @} */ // end of MicroScheduler

} // namespace gts
