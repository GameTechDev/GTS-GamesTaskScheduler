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

#include "gts/platform/Thread.h"
#include "gts/containers/parallel/BinnedAllocator.h"
#include "gts/micro_scheduler/MicroSchedulerTypes.h"
#include "gts/micro_scheduler/Task.h"

namespace gts {

class MicroScheduler;
class WorkerPool;
class LocalScheduler;

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
        OwnedId workerId,
        WorkerThreadDesc::GroupAndAffinity affinityByGroup,
        Thread::Priority threadPrioity,
        const char* threadName,
        void* pUserData,
        WorkerPoolDesc::GetThreadLocalStateFcn pGetThreadLocalStateFcn,
        WorkerPoolDesc::SetThreadLocalStateFcn pSetThreadLocalStateFcn,
        WorkerPoolVisitor* pVisitor,
        uint32_t cachableTaskSize,
        uint32_t initialTaskCountPerWorker,
        bool isMaster = false);

    void shutdown();

    void sleep(bool force);
    void resetSleepState();
    bool wake(uint32_t count, bool reset, bool force);

    void halt();
    void resume();

    void registerLocalScheduler(LocalScheduler* pLocalScheduler);
    void unregisterLocalScheduler(LocalScheduler* pLocalScheduler);

    Task* allocateTask(uint32_t size);
    void freeTask(Task* pTask);

public: // ACCESSORS:

    MicroScheduler* currentMicroScheduler() const;

    GTS_INLINE bool isHaulted() const
    {
        return m_pHaltSemaphore->isWaiting();
    }

    GTS_INLINE OwnedId id() const
    {
        return m_id;
    }

    GTS_INLINE ThreadId tid()
    {
        return m_threadId;
    }

    GTS_INLINE static uintptr_t getLocalState()
    {
        return s_pGetThreadLocalStateFcn();
    }

private: // PRIVATE METHODS:

    static void _initTlsGetAndSet(
        WorkerPoolDesc::GetThreadLocalStateFcn& getter,
        WorkerPoolDesc::SetThreadLocalStateFcn& setter);

    // no copy
    Worker(Worker const&) = delete;
    Worker& operator=(Worker const&) = delete;

    // Thread routine arguments.
    struct WorkerThreadArgs
    {
        Worker* pSelf = nullptr;
        const char* name = nullptr;
        OwnedId workerId;
        fenv_t fenv;
        WorkerPoolVisitor* pVisitor = nullptr;
        uint32_t initialTaskCountPerWorker = 0;
        Atomic<bool> isReady = { false };
    };

    // The worker thread routine.
    static void _workerThreadRoutine(void* pArg);

    int32_t _locateScheduler(LocalScheduler* pLocalScheduler);

    // The Worker's LocalScheduler execution loop.
    void _schedulerExecutionLoop(SubIdType localWorkerId);

    // Find the next LocalScheduler with work.
    LocalScheduler* _getNextSchedulerRoundRobin(SubIdType localWorkerId, bool reset, Task*& pFoundTask, bool& executedTask);
    LocalScheduler* _getNextSchedulerRoundRobinLoop(SubIdType localWorkerId, bool reset, uint32_t begin, uint32_t end, Task*& pFoundTask, bool& executedTask);

    // Check if there is work to do.
    bool _hasWork();

    void _freeTasks();

private: // DATA:

    friend class MicroScheduler;
    friend class WorkerPool;
    friend class LocalScheduler;

    struct TaskPool
    {
        GTS_ALIGN(GTS_NO_SHARING_CACHE_LINE_SIZE) Atomic<Task*> pDeferredFreeList = { nullptr };
        GTS_ALIGN(GTS_NO_SHARING_CACHE_LINE_SIZE) Task* pFreeList = nullptr;
    };

    struct ThreadBlocker
    {
        static constexpr uint32_t IS_UNBLOCKED = 0x00;
        static constexpr uint32_t IS_BLOCKED   = 0x01;

        GTS_ALIGN(GTS_NO_SHARING_CACHE_LINE_SIZE) Atomic<uint32_t> state = { IS_UNBLOCKED };
        BinarySemaphore semaphore;
        Atomic<bool> resetOnWake = { false };
        Atomic<bool> doneWaking = { false };
    };

    using BackoffType = Backoff<BackoffGrowth::Geometric, true>;
    using MutexType   = UnfairSharedSpinMutex<uint8_t, BackoffType>;

    WorkerPool* m_pMyPool;
    TaskPool* m_pTaskPool;
    ThreadBlocker* m_pSleepBlocker;
    Vector<LocalScheduler*> m_registeredSchedulers;
    LocalScheduler* m_pCurrentScheduler;
    void* m_pUserData;
    BinarySemaphore* m_pHaltSemaphore;
    MutexType* m_pLocalSchedulersMutex;
    WorkerPoolDesc::GetThreadLocalStateFcn m_pGetThreadLocalStateFcn;
    WorkerPoolDesc::SetThreadLocalStateFcn m_pSetThreadLocalStateFcn;
    Thread m_thread;
    ThreadId m_threadId;
    uint32_t m_randState;
    uint32_t m_currentScheduleIdx;
    uint32_t m_resumeCount;
    Atomic<uint32_t> m_refCount;
    OwnedId m_id;
    uint32_t m_cachableTaskSize;

    char pad[GTS_CACHE_LINE_SIZE * 2];

    static WorkerPoolDesc::GetThreadLocalStateFcn s_pGetThreadLocalStateFcn;
    static Atomic<size_t> s_pGetThreadLocalStateFcnRefCount;
};

#ifdef GTS_MSVC
#pragma warning(pop)
#endif


} // namespace gts
