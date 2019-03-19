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

#include <algorithm>

#include "gts/analysis/Instrumenter.h"
#include "gts/analysis/Analyzer.h"

#include "Schedule.h"

namespace gts {

// TODO: Do more experiments so that these values are less adhock.

const uint32_t INITIAL_PAUSE_AVERAGE    = 16;
const uint32_t INITIAL_YEILD_THREASHOLD = 16;
const uint32_t SUSPEND_THREASHOLD       = 300;
const uint64_t MAX_BACKOFF_CYCLES       = 1000;

// An exponentially weighted moving average (EWMA) coefficient used for
// tracking the moving average of iterations with no work per worker.
const float EWMA_COEFFICIENT = 0.2f;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A thread backoff mechanism. Uses exponentially weighted moving averages
 *  to backoff in the following manner:
 *  1) Pause
 *  2) Yeild
 *  3) Suspend
 */
class Backoff
{
public:

    //--------------------------------------------------------------------------
    GTS_INLINE Backoff()
        : m_pauseCount(0)
        , m_spinCount(0)
        , m_yieldCount(0)
        , m_pauseAverage(INITIAL_PAUSE_AVERAGE)
        , m_yieldThreshold(INITIAL_YEILD_THREASHOLD)
    {}

    //--------------------------------------------------------------------------
    GTS_INLINE void reset()
    {
        if (m_pauseCount > 0)
        {
            // calculate the new pause average using an exponentially weighted moving average
            m_pauseAverage = std::max(1U, (uint32_t)(EWMA_COEFFICIENT*m_pauseCount + (1 - EWMA_COEFFICIENT)*m_pauseAverage));
        }

        if (m_spinCount > 0)
        {
            // calculate the new spin estimate using an exponentially weighted moving average
            m_yieldThreshold = std::max(1U, (uint32_t)(EWMA_COEFFICIENT*m_spinCount + (1 - EWMA_COEFFICIENT)*m_yieldThreshold));
        }

        // and reset all the counts.
        m_pauseCount = 0;
        m_spinCount  = 0;
        m_yieldCount = 0;
    }

    //--------------------------------------------------------------------------
    /**
     * Reset the count so that so that the worker has time to find new work.
     */
    GTS_INLINE void resetYield()
    {
        m_yieldCount = 0;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE bool tick(uint32_t workerIndex)
    {
        GTS_UNREFERENCED_PARAM(workerIndex);

        // Always pause first since yield may return immediately.
        ++m_pauseCount;
        if (!gts::SpinWait::spinUntil(m_pauseAverage, MAX_BACKOFF_CYCLES))
        {
            // We exceeded the cycle limit for pause, reduce the spinning.
            m_pauseAverage /= 2;
        }

        // If we are beyond the yield threshold,
        if (++m_spinCount > m_yieldThreshold * 2)
        {
            // and if we are beyond the suspend threshold,
            if (++m_yieldCount > SUSPEND_THREASHOLD)
            {
                // return true to indicate that a suspend is needed.
                return true;
            }
            else
            {
                GTS_ANALYSIS_COUNT(workerIndex, gts::analysis::AnalysisType::NUM_WORKER_YIELDS);
                GTS_ANALYSIS_TIME_SCOPED(workerIndex, gts::analysis::AnalysisType::NUM_WORKER_YIELDS);
                // otherwise yeild.
                gts::ThisThread::yield();
            }
        }

        // return false to indicate that a suspend is NOT needed.
        return false;
    }

    ////--------------------------------------------------------------------------
    //GTS_INLINE void printCounts()
    //{
    //    std::cout << "Pause: " << m_pauseCount << "\n";
    //    std::cout << "Spin:  " << m_spinCount << "\n";
    //    std::cout << "Yeild: " << m_yieldCount << "\n";
    //    std::cout << "PAve:  " << m_pauseAverage << "\n";
    //    std::cout << "YMax:  " << m_yieldThreshold << "\n";
    //}

private:

    uint32_t m_pauseCount;
    uint32_t m_spinCount;
    uint32_t m_yieldCount;
    uint32_t m_pauseAverage;
    uint32_t m_yieldThreshold;
};

} // namespace gts
