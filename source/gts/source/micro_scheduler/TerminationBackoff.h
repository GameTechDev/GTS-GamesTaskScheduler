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

#include "gts/platform/Utils.h"
#include "gts/analysis/Trace.h"
#include "gts/analysis/Counter.h"

#include "LocalScheduler.h"

namespace gts {


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A backoff mechanism for the termination phase of a scheduler.
 */
class TerminationBackoffFixed
{
    static constexpr uint32_t QUIT_THREASHOLD = 100;

public:

    //--------------------------------------------------------------------------
    GTS_INLINE TerminationBackoffFixed(uint32_t minQuitThreshold)
        : m_quitThreshold(minQuitThreshold)
        , m_quitCount(0)
    {}

    //--------------------------------------------------------------------------
    GTS_INLINE void resetQuitThreshold(uint32_t minQuitThreshold)
    {
        m_quitThreshold = minQuitThreshold;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE void reset()
    {
        m_quitCount = 0;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE bool tryToQuit(SubIdType workerId)
    {
        GTS_UNREFERENCED_PARAM(workerId);

        GTS_TRACE_SCOPED_ZONE_P1(analysis::CaptureMask::MICRO_SCHEDULER_PROFILE, analysis::Color::Red, "BACKOFF", workerId);
        {
            GTS_TRACE_SCOPED_ZONE_P1(analysis::CaptureMask::MICRO_SCHEDULER_PROFILE, analysis::Color::Red3, "BACKOFF PAUSE", m_quitCount);
            pauseFor(2 * GTS_CONTEXT_SWITCH_CYCLES);
        }

        {
            GTS_TRACE_SCOPED_ZONE_P1(analysis::CaptureMask::MICRO_SCHEDULER_PROFILE, analysis::Color::Red3, "BACKOFF YIELD", m_quitCount);
            ThisThread::yield();
        }

        return m_quitCount++ >= m_quitThreshold;
    }

private:

    uint32_t m_quitThreshold;
    uint32_t m_quitCount;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A thread backoff mechanism that uses adaptive values to define thresholds
 *  for each tier of backoff.
 *  1) Pause
 *  2) Yield
 *  3) Quit
 */
class TerminationBackoffAdaptive
{
    // An exponentially weighted moving average (EWMA) coefficient used for
    // tracking the moving average of iterations with no work per worker.
    static constexpr float EWMA_COEFFICIENT = 0.5f;

public:

    //--------------------------------------------------------------------------
    GTS_INLINE TerminationBackoffAdaptive(int32_t minQuitThreshold)
        : m_minQuitThreshold(minQuitThreshold)
        , m_quitThreshold(minQuitThreshold)
        , m_quitCount(0)
    {
        reset();
    }


    //--------------------------------------------------------------------------
    GTS_INLINE void resetQuitThreshold(uint32_t minQuitThreshold)
    {
        m_minQuitThreshold = minQuitThreshold;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE void reset()
    {
        if (m_quitCount > 0)
        {
            // Calculate the new quit using an exponentially weighted moving average
            m_quitThreshold = (int32_t)(EWMA_COEFFICIENT * m_quitCount + (1 - EWMA_COEFFICIENT) * m_quitThreshold);
            m_quitThreshold = gtsMax(m_quitThreshold, m_minQuitThreshold);

            GTS_TRACE_SCOPED_ZONE_P1(analysis::CaptureMask::MICRO_SCHEDULER_PROFILE, analysis::Color::Red, "BACKOFF RESET", m_quitThreshold);
        }

        m_quitCount = 0;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE bool tryToQuit(SubIdType workerId)
    {
        GTS_UNREFERENCED_PARAM(workerId);

        GTS_TRACE_SCOPED_ZONE_P1(analysis::CaptureMask::MICRO_SCHEDULER_PROFILE, analysis::Color::Red, "BACKOFF", workerId);
        {
            GTS_TRACE_SCOPED_ZONE_P1(analysis::CaptureMask::MICRO_SCHEDULER_PROFILE, analysis::Color::Red3, "BACKOFF PAUSE", m_quitCount);
            pauseFor(GTS_CONTEXT_SWITCH_CYCLES);
        }

        {
            GTS_TRACE_SCOPED_ZONE_P1(analysis::CaptureMask::MICRO_SCHEDULER_PROFILE, analysis::Color::Red3, "BACKOFF YIELD", m_quitCount);
            ThisThread::yield();
        }

        return m_quitCount++ >= m_quitThreshold * 2;
    }

private:

    int32_t m_minQuitThreshold;
    int32_t m_quitThreshold;
    int32_t m_quitCount;
};

} // namespace gts
