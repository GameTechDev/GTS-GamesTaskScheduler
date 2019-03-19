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
#include "gts/analysis/Analyzer.h"

#include <iomanip>
#include <fstream>
#include <string>
#include <thread>

#include "gts/platform/Atomic.h"

namespace gts {
namespace analysis {

static const size_t COUNT_WIDTH = 20;
static const size_t CYCLES_WIDTH = 20;

static gts::Atomic<uint32_t> g_nextThreadIndex = { 0 };
static GTS_THREAD_LOCAL uint32_t g_threadIndex = UINT32_MAX;

//------------------------------------------------------------------------------
uint32_t Analyzer::getThreadIndex()
{
    if (g_threadIndex == UINT32_MAX)
    {
        g_threadIndex = g_nextThreadIndex.fetch_add(1);
        m_threadIdThreadIndex[g_threadIndex] = std::hash<std::thread::id>{}(std::this_thread::get_id());
    }
    return g_threadIndex;
}

//------------------------------------------------------------------------------
void Analyzer::start(size_t threadCount)
{
    m_analysisDataByThreadIndex.clear();
    m_analysisDataByThreadIndex.resize(threadCount);
    m_currentTimingsByThreadIndex.resize(threadCount);
    m_threadIdThreadIndex.resize(threadCount);
}

//------------------------------------------------------------------------------
void Analyzer::stop(const char* file)
{
    std::ofstream outFile;
    outFile.open(file, std::ios_base::out);
    outFile << std::setfill('_');

    AnalysisData totals[(uint64_t)AnalysisType::ANALYSIS_TYPE_COUNT];

    uint32_t threadIndex = 0;
    for (size_t ii = 0; ii < m_analysisDataByThreadIndex.size(); ++ii)
    {
        outFile << "=============\n";
        outFile << "THREAD INDEX: " << threadIndex++ << "\n";
        outFile << "=============\n";
        for (size_t analysisType = 0; analysisType < (size_t)AnalysisType::ANALYSIS_TYPE_COUNT; ++analysisType)
        {
            const uint64_t count = m_analysisDataByThreadIndex[ii][analysisType].count;
            const uint64_t cycles = m_analysisDataByThreadIndex[ii][analysisType].cycles;
            outFile << m_typeStringByType[(AnalysisType)analysisType] << std::setw(COUNT_WIDTH) << count;
            outFile << "\tCost:" << std::setw(CYCLES_WIDTH) << cycles << " cylces\n";

            totals[analysisType].count += count;
            totals[analysisType].cycles += cycles;
        }
    }

    outFile << "=============\n";
    outFile << "THREAD TOTALS\n";
    outFile << "=============\n";
    for (size_t analysisType = 0; analysisType < (size_t)AnalysisType::ANALYSIS_TYPE_COUNT; ++analysisType)
    {
        const uint64_t count = totals[analysisType].count;
        const uint64_t cycles = totals[analysisType].cycles;
        outFile << m_typeStringByType[(AnalysisType)analysisType] << std::setw(COUNT_WIDTH) << count;
        outFile << "\tCost:" << std::setw(CYCLES_WIDTH) << cycles << " cylces\n";
    }

    outFile.close();
}

//------------------------------------------------------------------------------
Analyzer::Analyzer()
{
    m_typeStringByType[AnalysisType::NUM_ALLOCATIONS]               = "Task Allocations       :";
    m_typeStringByType[AnalysisType::NUM_SLOW_PATH_ALLOCATIONS]     = "Task Slow Allocations  :";
    m_typeStringByType[AnalysisType::NUM_MEMORY_RECLAIMS]           = "Task Reclaims          :";

    m_typeStringByType[AnalysisType::NUM_FREES]                     = "Task Frees             :";
    m_typeStringByType[AnalysisType::NUM_DEFERRED_FREES]            = "Task Deferred Frees    :";

    m_typeStringByType[AnalysisType::NUM_NONWORKER_POP_ATTEMPTS]    = "Non-worker Pop Attempts:";
    m_typeStringByType[AnalysisType::NUM_NONWORKER_POP_SUCCESSES]   = "Non-worker Pops        :";

    m_typeStringByType[AnalysisType::NUM_AFFINITY_POP_ATTEMPTS]     = "Affinity Pop Attempts  :";
    m_typeStringByType[AnalysisType::NUM_AFFINITY_POP_SUCCESSES]    = "Affinity Pops          :";

    m_typeStringByType[AnalysisType::NUM_TAKE_ATTEMPTS]             = "Worker Take Attempts   :";
    m_typeStringByType[AnalysisType::NUM_TAKE_SUCCESSES]            = "Worker Takes           :";

    m_typeStringByType[AnalysisType::NUM_STEAL_ATTEMPTS]            = "Worker Steal Attempts  :";
    m_typeStringByType[AnalysisType::NUM_STEAL_SUCCESSES]           = "Worker Steals          :";

    m_typeStringByType[AnalysisType::NUM_WORKER_YIELDS]             = "Main Loop Yields       :";
    m_typeStringByType[AnalysisType::NUM_WAIT_YIELDS]               = "Wait Loop Yields       :";

    m_typeStringByType[AnalysisType::NUM_WAITS]                     = "Waits                  :";
    m_typeStringByType[AnalysisType::NUM_EXECUTED_TASKS]            = "Executed Tasks         :";
    m_typeStringByType[AnalysisType::NUM_SHEDULER_BYPASSES]         = "Scheduler Bypasses     :";

    m_typeStringByType[AnalysisType::NUM_WAKE_CALLS]                = "Wake Workers Calls     :";
    m_typeStringByType[AnalysisType::NUM_WAKE_CHECKS]               = "Wake Workers Checks    :";
    m_typeStringByType[AnalysisType::NUM_WAKE_SUCCESSES]            = "Wake Workers           :";
    m_typeStringByType[AnalysisType::NUM_SLEEP_ATTEMPTS]            = "Worker Sleep Attempts  :";
    m_typeStringByType[AnalysisType::NUM_SLEEP_SUCCESSES]           = "Worker Sleeps          :";

    m_typeStringByType[AnalysisType::NUM_RESUME_CHECKS]             = "Resume Workers Checks  :";
    m_typeStringByType[AnalysisType::NUM_RESUME_SUCCESSES]          = "Resume Workers         :";
    m_typeStringByType[AnalysisType::NUM_haltS_SIGNALED]           = "Worker halt Signaled  :";
    m_typeStringByType[AnalysisType::NUM_halt_SUCCESSES]           = "Worker halts          :";
}

} // namespace analysis
} // namespace gts
