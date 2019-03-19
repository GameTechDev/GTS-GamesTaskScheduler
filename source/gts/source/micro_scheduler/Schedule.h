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

#include "Containers.h"
#include "gts/containers/Vector.h"

namespace gts {

class TaskDeque;
class TaskAllocator;
class MicroScheduler;
class Task;
class Backoff;

#ifdef GTS_MSVC
#pragma warning( push )
#pragma warning( disable : 4324) // alignment padding warning
#endif

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A Schedule that executes tasks on a Worker.
 */
class GTS_ALIGN(GTS_NO_SHARING_CACHE_LINE_SIZE) Schedule
{
public: // STRUCTORS:

    Schedule();
    virtual ~Schedule();

public: // ACCESSORS:

    bool hasTasks() const;

    GTS_INLINE uint32_t index() const
    {
        return m_index;
    }

public: // MUTATORS:

    void initialize(
        uint32_t scheduleIdx,
        MicroScheduler* pScheduler,
        uint32_t priorityCount,
        int16_t proirityToBoostAge);

    void run(Backoff& backoff);

    bool spawnTask(Task* pTask, uint32_t priority);
    bool queueAffinityTask(Task* pTask);
    void wait(Task* pTask, Task* pStartTask, Backoff& backoff);
    void setVictimPool(Vector<Schedule*> const& victimPool);

private: // SCHEDULING:

    void _handleContinuation(Task* pTask, Task*& pNextTask);
    //void _executeTask(Task* pTask);
    Task* _getLocalTask(uint32_t workerIdx);
    GTS_NO_INLINE Task* _getLocalBoostedTask(uint32_t workerIdx);
    Task* _getNonWorkerTask(uint32_t workerIdx);
    Task* _getAffinityTask(uint32_t workerIdx);
    Task* _stealTask(uint32_t workerIdx);

private: // DATA:

    friend class MicroScheduler;
    friend class Backoff;

    PriorityTaskDeque m_priorityTaskDeque;
    AffinityTaskQueue* m_pAffinityTaskQueue;
    TaskQueue* m_pTaskQueue;
    Vector<Schedule*> m_victimPool;

    TaskAllocator* m_pTaskAllocator;
    MicroScheduler* m_pMyScheduler;
    uintptr_t m_isolationTag;

    uint32_t m_index;
    uint32_t m_randState;
    int16_t m_proirityBoostAgeStart;
    int16_t m_proirityBoostAge;
    int16_t m_currentProirityToBoost;
};

#ifdef GTS_MSVC
#pragma warning( pop )
#endif

} // namespace gts
