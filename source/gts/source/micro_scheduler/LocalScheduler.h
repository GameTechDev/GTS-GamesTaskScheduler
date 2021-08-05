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
#include "gts/micro_scheduler/MicroSchedulerTypes.h"
#include "Containers.h"

namespace gts {

class MicroScheduler;
class Task;
class Worker;

#ifdef GTS_MSVC
#pragma warning( push )
#pragma warning( disable : 4324) // alignment padding warning
#endif

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A LocalScheduler that executes tasks on a Worker.
 */
class GTS_ALIGN(GTS_NO_SHARING_CACHE_LINE_SIZE) LocalScheduler
{
public: // STRUCTORS:

    LocalScheduler();
    ~LocalScheduler();

public: // ACCESSORS:

    bool hasDequeTasks() const;
    bool hasAffinityTasks() const;

    GTS_INLINE OwnedId index() const
    {
        return m_id;
    }

    GTS_INLINE bool isMaster() const
    {
        return m_id.localId() == 0;
    }

public: // MUTATORS:

    void initialize(
        OwnedId scheduleId,
        MicroScheduler* pScheduler,
        uint32_t priorityCount,
        int16_t proirityToBoostAge,
        bool canStealBack);

    bool run(Task* pInitialTask);

    bool spawnTask(Task* pTask, uint32_t priority);
    bool queueAffinityTask(Task* pTask, uint32_t priority);
    bool runUntilDone(Task* pWaitingTask, Task* pChild);

private: // PRIVATE METHODS:

    void _executeTaskLoop(Task* pTask, SubIdType localId, Worker* pThisWorker, bool& executedTask);
    GTS_NO_INLINE void _handleContinuation(Task* pParent, Task*& pBypassTask, SubIdType localId);
    Task* _getLocalTask(SubIdType localId);
    GTS_NO_INLINE Task* _getLocalBoostedTask(SubIdType localId);
    Task* _getNonLocalTaskLoop(Task* pWaitingTask, Worker* pThisWorker, SubIdType localId, bool canStealExternal, bool& executedTask);
    Task* _getQueuedTask(SubIdType localId);
    Task* _getAffinityTask(SubIdType localId);
    Task* _stealTask(SubIdType localId, bool callerIsExternal);
    Task* _stealTaskLoop(SubIdType localId, LocalScheduler** ppVictims, uint32_t begin, uint32_t end);
    Task* _stealTaskFrom(SubIdType localId, SubIdType victimId, LocalScheduler** ppVictims, bool notifyStealBack);
    Task* _getNonLocalTask(Worker* pThisWorker, bool getAffinity, bool callerIsExternal, bool isLastChance, SubIdType localId, bool& executedTask);
    GTS_NO_INLINE Task* _stealExternalTask(SubIdType localId);
    Task* LocalScheduler::_stealBackExternalTask(SubIdType localId, OwnedId const& stealbackId);

private: // DATA:

    friend class MicroScheduler;
    friend class Task;
    friend class Worker;
    friend class WorkerPool;

    static constexpr uint16_t TASKS_STATE_EMPTY = 0;
    static constexpr uint16_t TASKS_STATE_FULL  = 1;
    static constexpr uint16_t TASKS_STATE_BUSY  = 2;

    using MutexType = UnfairSpinMutex<>;

    PriorityTaskDeque m_priorityTaskDeque;
    PriorityAffinityTaskQueue m_affinityTaskQueue;
    PriorityTaskQueue* m_pPriorityTaskQueue;
    MicroScheduler* m_pMyScheduler;
    OwnedId m_id;

    struct GTS_ALIGN(GTS_NO_SHARING_CACHE_LINE_SIZE) 
    {
        Atomic<OwnedId> lastThief;
        bool enabled;
    } m_stealBack;
    
    Atomic<bool> m_hasDemand;

    GTS_ALIGN(GTS_NO_SHARING_CACHE_LINE_SIZE) Task* m_pWaiterTask;

    uint32_t m_randState;
    int16_t m_proirityBoostAgeStart;
    int16_t m_proirityBoostAge;
    int16_t m_currentProirityToBoost;

    GTS_ALIGN(GTS_NO_SHARING_CACHE_LINE_SIZE) MutexType m_workerAccessMutex;
};

#ifdef GTS_MSVC
#pragma warning( pop )
#endif

} // namespace gts