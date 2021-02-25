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

#if GTS_ENABLE_COUNTER == 1

#include <unordered_map>
#include <iomanip>
#include <fstream>
#include <string>

#include "gts/platform/Thread.h"
#include "gts/synchronization/SpinMutex.h"
#include "gts/containers/Vector.h"
#include "gts/platform/Assert.h"
#include "gts/platform/Atomic.h"
#include "gts/platform/Utils.h"

#define GTS_WP_COUNTER_INC(ownedId, counter) gts::analysis::WorkerPoolCounter::inst().countOccurance(ownedId, counter)
#define GTS_WP_COUNTER_DUMP_TO(ownerId, filename) gts::analysis::WorkerPoolCounter::inst().print(ownerId, filename)

#define GTS_MS_COUNTER_INC(ownedId, counter) gts::analysis::MicroSchedulerCounter::inst().countOccurance(ownedId, counter)
#define GTS_MS_COUNTER_DUMP_TO(ownerId, filename) gts::analysis::MicroSchedulerCounter::inst().print(ownerId, filename)

#else

#define GTS_WP_COUNTER_INC(ownedId, counter)
#define GTS_WP_COUNTER_DUMP_TO(ownerId, filename)

#define GTS_MS_COUNTER_INC(ownedId, counter)
#define GTS_MS_COUNTER_DUMP_TO(ownerId, filename)

#endif

/** 
 * @addtogroup Analysis
 * @{
 */

/** 
 * @addtogroup Counters
 * @{
 */

#if GTS_ENABLE_COUNTER == 1

namespace gts {
namespace analysis {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
struct WorkerPoolCounters
{
    WorkerPoolCounters();

    enum
    {
        NUM_ALLOCATIONS,
        NUM_SLOW_PATH_ALLOCATIONS,
        NUM_MEMORY_RECLAIMS,

        NUM_FREES,
        NUM_DEFERRED_FREES,

        NUM_WAKE_CALLS,
        NUM_WAKE_CHECKS,
        NUM_WAKE_SUCCESSES,
        NUM_SLEEP_SUCCESSES,

        NUM_RESUMES,
        NUM_HALTS_SIGNALED,
        NUM_HALT_SUCCESSES,

        NUM_SCHEDULER_REGISTERS,
        NUM_SCHEDULER_UNREGISTERS,

        COUNT
    };

    const char* m_counterStringByCounter[COUNT];
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
struct MicroSchedulerCounters
{
    MicroSchedulerCounters();

    enum
    {
        NUM_SPAWNS,
        NUM_QUEUES,

        NUM_DEQUE_POP_ATTEMPTS,
        NUM_DEQUE_POP_SUCCESSES,

        NUM_BOOSTED_DEQUE_POP_ATTEMPTS,
        NUM_BOOSTED_DEQUE_POP_SUCCESSES,

        NUM_DEQUE_STEAL_ATTEMPTS,
        NUM_DEQUE_STEAL_SUCCESSES,

        NUM_FAILED_CAS_IN_DEQUE_STEAL,

        NUM_QUEUE_POP_ATTEMPTS,
        NUM_QUEUE_POP_SUCCESSES,

        NUM_AFFINITY_POP_ATTEMPTS,
        NUM_AFFINITY_POP_SUCCESSES,

        NUM_EXTERNAL_STEAL_ATTEMPTS,
        NUM_EXTERNAL_STEAL_SUCCESSES,

        NUM_WAITS,
        NUM_CONTINUATIONS,
        NUM_EXECUTED_TASKS,
        NUM_SCHEDULER_BYPASSES,
        NUM_EXIT_ATTEMPTS,
        NUM_EXITS,

        NUM_SCHEDULER_REGISTERS,
        NUM_SCHEDULER_UNREGISTERS,

        COUNT
    };

    const char* m_counterStringByCounter[COUNT];
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A set of thread local counters.
 * @tparam TCounters
 *  The definition of the counters and their string mappings.
 */
template<typename TCounters>
class Counter
{
    using mutex_type = UnfairSpinMutex<>;

    struct OwnedIdHash
    { 
        size_t operator()(OwnedId const& id) const
        { 
            return murmur_hash3(id.uid());
        }
    }; 

    struct ThreadInfo
    {
        using array_type = uint64_t[uint32_t(TCounters::COUNT)];
        using map_type = std::unordered_map<OwnedId, array_type, OwnedIdHash>;
        ThreadId tid = 0;
        map_type countsByOwnedId;
    };

    struct ThreadInfoPtr
    {
        GTS_INLINE ThreadInfoPtr() = default;
        GTS_INLINE ~ThreadInfoPtr() { if (ptr) { delete ptr; } }
        ThreadInfo* ptr = nullptr;
    };

public:

    /**
     * @return The singleton instance of a Counter.
     */
    static Counter& inst()
    {
        static Counter instance;
        return instance;
    }

public:

    /**
     * Increment an occurrence of 'type' for this thread.
     */
    void countOccurance(OwnedId const& id, size_t counter)
    {
        ThreadInfo* pInfo = getThreadInfo();
        pInfo->countsByOwnedId[id][counter]++;
    }

    /**
     * Prints the counters from the specified ownerId to the specified file.
     */
    void print(SubIdType const& ownerId, const char* file)
    {
        static const size_t COUNT_WIDTH = 20;

        std::ofstream outFile;
        outFile.open(file, std::ios_base::out);
        outFile << std::setfill('_');

        //
        // Collect all the counters by owner ID.

        std::unordered_map<SubIdType, std::unordered_map<gts::OwnedId, ThreadInfo::array_type, OwnedIdHash>> counterMapsByOwnedId;

        for (size_t iThread = 0; iThread < m_threadInfo.size(); ++iThread)
        {
            ThreadInfo* pInfo = m_threadInfo[iThread];
            for (auto mapIter = pInfo->countsByOwnedId.begin(); mapIter != pInfo->countsByOwnedId.end(); ++mapIter)
            {
                auto& countersArray = counterMapsByOwnedId[mapIter->first.ownerId()][mapIter->first];

                // Accumulate the counts.
                for(size_t ii = 0; ii < TCounters::COUNT; ++ii)
                {
                    countersArray[ii] += mapIter->second[ii];
                }
            }
        }

        //
        // Print counters by owner Id.

        uint64_t totals[TCounters::COUNT] = {0};

        for (auto localMapIter = counterMapsByOwnedId.begin(); localMapIter != counterMapsByOwnedId.end(); ++localMapIter)
        {
            if (localMapIter->first != UNKNOWN_SUBID && localMapIter->first != ownerId)
            {
                continue;
            }

            outFile << "=============\n";
            outFile << "OWNER ID: " << localMapIter->first << "\n";
            outFile << "=============\n";

            for (auto countersIter = localMapIter->second.begin(); countersIter != localMapIter->second.end(); ++countersIter)
            {
                outFile << "=============\n";
                outFile << "Local ID: " << countersIter->first.localId() << "\n";
                outFile << "=============\n";

                for (size_t iCounter = 0; iCounter < TCounters::COUNT; ++iCounter)
                {
                    uint64_t count = countersIter->second[iCounter];
                    outFile << m_counters.m_counterStringByCounter[iCounter] << std::setw(COUNT_WIDTH) << count << "\n";
                    totals[iCounter] += count;
                }
            }
        }

        outFile << "=============\n";
        outFile << "TOTALS\n";
        outFile << "=============\n";
        for (size_t iCounter = 0; iCounter < TCounters::COUNT; ++iCounter)
        {
            uint64_t count = totals[iCounter];
            outFile << m_counters.m_counterStringByCounter[iCounter] << std::setw(COUNT_WIDTH) << count << "\n";
        }

        outFile.close();
    }


private:

    /**
     * Get this thread TL counter info. Creates and tracks the TL info if not present.
     */
    ThreadInfo* getThreadInfo()
    {
        if (s_tlThreadInfo.ptr == nullptr)
        {
            s_tlThreadInfo.ptr = new ThreadInfo;
        }
        if(s_tlThreadInfo.ptr->tid == 0)
        {
            s_tlThreadInfo.ptr->tid = ThisThread::getId();
            Lock<mutex_type> lock(m_threadInfoMutex);
            m_threadInfo.push_back(s_tlThreadInfo.ptr);
        }
        return s_tlThreadInfo.ptr;
    }

    Counter() = default;

private:

    static thread_local ThreadInfoPtr s_tlThreadInfo;
    
    TCounters m_counters;
    UnfairSpinMutex<> m_threadInfoMutex;
    Vector<ThreadInfo*> m_threadInfo;
};

template<typename TCounters>
thread_local typename Counter<TCounters>::ThreadInfoPtr Counter<TCounters>::s_tlThreadInfo;

using WorkerPoolCounter = Counter<WorkerPoolCounters>;
using MicroSchedulerCounter = Counter<MicroSchedulerCounters>;

} // namespace analysis
} // namespace gts

#endif
