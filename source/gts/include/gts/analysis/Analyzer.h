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

#include <array>
#include <unordered_map>
#include <string>

#include "gts/platform/Machine.h"
#include "gts/containers/Vector.h"
#include "gts/platform/Assert.h"
#include "gts/platform/Atomic.h"

#ifdef GTS_ANALYZE

#define GTS_ANALYSIS_START(threadCount) gts::analysis::Analyzer::inst().start(threadCount)
#define GTS_ANALYSIS_STOP(filename) gts::analysis::Analyzer::inst().stop(filename)
#define GTS_ANALYSIS_COUNT(threadIndex, type) gts::analysis::Analyzer::inst().countOccurance(threadIndex, type)
#define GTS_ANALYSIS_TIME_START(threadIndex, type) gts::analysis::Analyzer::inst().beginTimedAnalysis(threadIndex, type)
#define GTS_ANALYSIS_TIME_END(threadIndex, type) gts::analysis::Analyzer::inst().endTimedAnalysis(threadIndex, type)
#define GTS_ANALYSIS_TIME_SCOPED(threadIndex, type) gts::analysis::ScopedAnalysisTime GTS_TOKENPASTE2(scopedAnalysisTime, __LINE__)(threadIndex, type)
#define GTS_ANALYSIS_TIME_START_NO_ID(type) gts::analysis::Analyzer::inst().beginTimedAnalysis(type)
#define GTS_ANALYSIS_TIME_END_NO_ID(type) gts::analysis::Analyzer::inst().endTimedAnalysis(type)
#define GTS_ANALYSIS_TIME_SCOPED_NO_ID(type) gts::analysis::ScopedAnalysisTimeAuto(type)

#else

#define GTS_ANALYSIS_START(threadCount)
#define GTS_ANALYSIS_STOP(filename)
#define GTS_ANALYSIS_COUNT(threadIndex, type)
#define GTS_ANALYSIS_TIME_START(threadIndex, type)
#define GTS_ANALYSIS_TIME_END(threadIndex, type)
#define GTS_ANALYSIS_TIME_SCOPED(threadIndex, type)
#define GTS_ANALYSIS_TIME_START_NO_ID(type)
#define GTS_ANALYSIS_TIME_END_NO_ID(type)
#define GTS_ANALYSIS_TIME_SCOPED_NO_ID(type)

#endif

namespace gts {
namespace analysis {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
enum class AnalysisType : uint32_t
{
    NUM_ALLOCATIONS,
    NUM_SLOW_PATH_ALLOCATIONS,
    NUM_MEMORY_RECLAIMS,

    NUM_FREES,
    NUM_DEFERRED_FREES,

    NUM_NONWORKER_POP_ATTEMPTS,
    NUM_NONWORKER_POP_SUCCESSES,

    NUM_AFFINITY_POP_ATTEMPTS,
    NUM_AFFINITY_POP_SUCCESSES,

    NUM_TAKE_ATTEMPTS,
    NUM_TAKE_SUCCESSES,

    NUM_STEAL_ATTEMPTS,
    NUM_STEAL_SUCCESSES,

    NUM_WORKER_YIELDS,
    NUM_WAIT_YIELDS,

    NUM_WAITS,
    NUM_EXECUTED_TASKS,
    NUM_SHEDULER_BYPASSES,

    NUM_WAKE_CALLS,
    NUM_WAKE_CHECKS,
    NUM_WAKE_SUCCESSES,
    NUM_SLEEP_ATTEMPTS,
    NUM_SLEEP_SUCCESSES,

    NUM_RESUME_CHECKS,
    NUM_RESUME_SUCCESSES,
    NUM_haltS_SIGNALED,
    NUM_halt_SUCCESSES,

    ANALYSIS_TYPE_COUNT
};

} // namespace gts
} // namespace analysis

namespace std {

template<>
struct hash<gts::analysis::AnalysisType>
{
    size_t operator()(const gts::analysis::AnalysisType& x) const
    {
        return std::hash<uint32_t>{}(static_cast<uint32_t>(x));
    }
};

} // namespace std

namespace gts {
namespace analysis {

class Analyzer;
static Analyzer* g_analyzer;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A task scheduler analyzer.
 */
class Analyzer
{
public:

    //--------------------------------------------------------------------------
    GTS_INLINE static Analyzer& inst()
    {
        static Analyzer instance;
        g_analyzer = &instance;
        return instance;
    }

    void start(size_t threadCount);
    void stop(const char* file);

    //--------------------------------------------------------------------------
    GTS_INLINE void countOccurance(uint32_t threadIndex, AnalysisType type)
    {
        m_analysisDataByThreadIndex[threadIndex][(size_t)type].count++;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE void countOccurance(AnalysisType type)
    {
        countOccurance(getThreadIndex(), type);
    }

    //--------------------------------------------------------------------------
    GTS_INLINE void beginTimedAnalysis(uint32_t threadIndex, AnalysisType type)
    {
        GTS_ASSERT(m_currentTimingsByThreadIndex[threadIndex][(size_t)type] == 0);

        GTS_SERIALIZE_CPU();
        m_currentTimingsByThreadIndex[threadIndex][(size_t)type] = GTS_RDTSC();
        gts::atomic_signal_fence();
        GTS_SERIALIZE_CPU();
    }

    //--------------------------------------------------------------------------
    GTS_INLINE void beginTimedAnalysis(AnalysisType type)
    {
        beginTimedAnalysis(getThreadIndex(), type);
    }

    //--------------------------------------------------------------------------
    GTS_INLINE void endTimedAnalysis(uint32_t threadIndex, AnalysisType type)
    {
        GTS_SERIALIZE_CPU();
        gts::atomic_signal_fence();
        const uint64_t endCount = GTS_RDTSC();
        GTS_SERIALIZE_CPU();

        const uint64_t startCount = m_currentTimingsByThreadIndex[threadIndex][(size_t)type];
        uint64_t deltaCount = endCount - startCount;
        if (deltaCount > 10000 || startCount > endCount)
        {
            deltaCount = 0;
        }
        m_analysisDataByThreadIndex[threadIndex][(size_t)type].cycles += deltaCount;

        m_currentTimingsByThreadIndex[threadIndex][(size_t)type] = 0;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE void endTimedAnalysis(AnalysisType type)
    {
        endTimedAnalysis(getThreadIndex(), type);
    }

private:

    Analyzer();
    uint32_t getThreadIndex();

    struct AnalysisData
    {
        uint64_t count = 0;
        uint64_t cycles = 0;
    };

    std::unordered_map<AnalysisType, std::string> m_typeStringByType;
    gts::Vector<std::array<uint64_t, (size_t)AnalysisType::ANALYSIS_TYPE_COUNT>> m_currentTimingsByThreadIndex;
    gts::Vector<std::array<AnalysisData, (size_t)AnalysisType::ANALYSIS_TYPE_COUNT>> m_analysisDataByThreadIndex;
    gts::Vector<size_t> m_threadIdThreadIndex;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
struct ScopedAnalysisTime
{
    ScopedAnalysisTime(uint32_t threadIndex, AnalysisType type)
        :
        m_threadIndex(threadIndex),
        m_type(type)
    {
        GTS_ANALYSIS_TIME_START(m_threadIndex, m_type);
    }

    ~ScopedAnalysisTime()
    {
        GTS_ANALYSIS_TIME_END(m_threadIndex, m_type);
    }

    uint32_t m_threadIndex;
    AnalysisType m_type;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
struct ScopedAnalysisTimeNoId
{
    explicit ScopedAnalysisTimeNoId(AnalysisType type)
        :
        m_type(type)
    {
        gts::analysis::Analyzer::inst().countOccurance(1, type);

        GTS_ANALYSIS_TIME_START_NO_ID(m_type);
    }

    ~ScopedAnalysisTimeNoId()
    {
        GTS_ANALYSIS_TIME_END_NO_ID(m_type);
    }

    AnalysisType m_type;
};

} // namespace analysis
} // namespace gts
