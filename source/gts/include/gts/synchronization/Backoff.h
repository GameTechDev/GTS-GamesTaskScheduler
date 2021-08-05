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
#include "gts/platform/Machine.h"
#include "gts/platform/Atomic.h"
#include "gts/platform/Thread.h"
#include "gts/platform/InstructionSet.h"
#include "gts/analysis/Trace.h"

namespace gts {

constexpr uint32_t WAITPKG_OPTIMIZATION_STATE = WAITPKG_OPTIMIZE_POWER;

/**
 * @defgroup Synchronization
 *  A library of spinlocks and contention reducing algorithms.
 * @{
 */

//------------------------------------------------------------------------------
/**
 * @brief
 * Spin on pause 'count' times.
 */
GTS_INLINE void spin(uint32_t count)
{
    GTS_TRACE_SCOPED_ZONE_P1(analysis::CaptureMask::THREAD_PROFILE, analysis::Color::DarkRed, "SPIN", count);
    if (!InstructionSet::waitPackage())
    {
        for (uint32_t pp = 0; pp <= count; ++pp)
        {
            GTS_PAUSE();
        }
    }
    else
    {
        constexpr uint64_t cycles = 40;
        for (uint32_t pp = 0; pp <= count; ++pp)
        {
            tpause(WAITPKG_OPTIMIZATION_STATE, cycles);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  The growths rates for a back-off algorithm.
 */
enum class BackoffGrowth
{
    //! Backoff at an arithmetic rate.
    Arithmetic,
    //! Backoff at an geometric rate.
    Geometric
};

//------------------------------------------------------------------------------
/**
* @brief
*  Spin wait for the specified number of cycles.
*/
GTS_INLINE void pauseFor(uint32_t cycleCount)
{
    if (!InstructionSet::waitPackage())
    {
        int32_t count = 0;

        uint64_t prev = GTS_RDTSC();
        const uint64_t finish = prev + cycleCount;
        do
        {
            spin(count);
            count *= 2;

            uint64_t curr = GTS_RDTSC();
            if (curr <= prev)
            {
                // Quit on context switch or overflow.
                break;
            }
            prev = curr;

        } while (prev < finish);
    }
    else
    {
        tpause(WAITPKG_OPTIMIZATION_STATE, cycleCount);
    }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  An object that backs-off a thread to reduce contention with other threads.
 * @tparam GROWTH_RATE
 *  The growth rate of the backoff.
 * @tparam YIELD
 *  A flag to indicate that the backoff will yield the thread once a threshold
 *  has been reached.
 */
template<BackoffGrowth GROWTH_RATE = BackoffGrowth::Geometric, bool YIELD = true>
class Backoff
{
public:

    //--------------------------------------------------------------------------
    /**
     * @brief
     *  Take the next backoff step.
     */
    GTS_INLINE bool tick()
    {
        if(!YIELD || m_cyclesSoFar < GTS_CONTEXT_SWITCH_CYCLES)
        {
            uint64_t begin = GTS_RDTSC();
            spin(m_count);
            uint64_t end = GTS_RDTSC();

            if (end <= begin)
            {
                // Context switch or overflow occurred. Assume we are ready to yield.
                m_cyclesSoFar = GTS_CONTEXT_SWITCH_CYCLES;
            }
            else
            {
                m_cyclesSoFar += end - begin;
            }

            switch (GROWTH_RATE)
            {
            case BackoffGrowth::Arithmetic:
                m_count += 1;
                break;
            case BackoffGrowth::Geometric:
                m_count *= 2;
                break;
            }
            return false;
        }
        else if(YIELD)
        {
            GTS_TRACE_SCOPED_ZONE_P0(analysis::CaptureMask::THREAD_PROFILE, analysis::Color::DarkMagenta, "YIELD");
            ThisThread::yield();
        }

        return true;
    }

    //--------------------------------------------------------------------------
    /**
     * @brief
     *  Reset the backoff state.
     */
    GTS_INLINE void reset()
    {
        m_count       = 1;
        m_cyclesSoFar = 0;
    }

    //--------------------------------------------------------------------------
    /**
     * @brief
     *  Get the number of spins to perform next tick.
     * @return
     *  The spin count.
     */
    GTS_INLINE int32_t spinCount() const
    {
        return m_count;
    }

private:

    uint64_t m_cyclesSoFar = 0;
    int32_t m_count        = 1;
};

/** @} */ // end of SynchronizationPrimitives
/** @} */ // end of Platform

} // namespace gts
