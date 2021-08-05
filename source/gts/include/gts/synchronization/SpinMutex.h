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
#include "gts/analysis/Trace.h"
#include "gts/synchronization/Backoff.h"

namespace gts {

/**
 * @addtogroup Synchronization
 * @{
 */

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A unfair (first-ready-first-serve) spin mutex.
 */
template<typename TBackoff = Backoff<BackoffGrowth::Geometric, true>>
class UnfairSpinMutex
{
    // TTAS + Backoff spinlock
public:

    using backoff_type = TBackoff;

public:

    //--------------------------------------------------------------------------
    GTS_INLINE UnfairSpinMutex() : m_lock(AVAILABLE) {}

    //--------------------------------------------------------------------------
    GTS_INLINE UnfairSpinMutex(UnfairSpinMutex&& other)
        : m_lock(std::move(other.m_lock.load(memory_order::acquire)))
    {}

    //--------------------------------------------------------------------------
    GTS_INLINE UnfairSpinMutex& operator=(UnfairSpinMutex&& other)
    {
        if (this != &other)
        {
            m_lock.store(std::move(other.m_lock.load(memory_order::acquire)), memory_order::relaxed);
        }
        return *this;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE bool try_lock()
    {
        GTS_TRACE_SCOPED_ZONE_P1(analysis::CaptureMask::THREAD_PROFILE, analysis::Color::DarkRed, "TRY LOCK", this);

        // Test and test-and-set.
        return m_lock.load(memory_order::relaxed) == AVAILABLE &&
            // Only perform the expensive exchange if the lock seems available.
            m_lock.exchange(LOCKED, memory_order::acq_rel) == AVAILABLE;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE void lock()
    {
        GTS_TRACE_SCOPED_ZONE_P1(analysis::CaptureMask::THREAD_PROFILE, analysis::Color::DarkRed, "LOCKING", this);

        backoff_type backoff;
        while (!try_lock())
        {
            backoff.tick();
        }
    }

    //--------------------------------------------------------------------------
    GTS_INLINE void unlock()
    {
        m_lock.store(AVAILABLE, memory_order::release);
    }

    //--------------------------------------------------------------------------
    GTS_INLINE bool isLocked() const
    {
        return m_lock.load(memory_order::acquire) == LOCKED;
    }

private:

    enum { AVAILABLE = 0, LOCKED = 1 };

    Atomic<uint8_t> m_lock;
};


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A fair (first-come-first-serve) spin mutex.
 */
template<typename TBackoff = Backoff<BackoffGrowth::Geometric, true>>
class FairSpinMutex
{
    // Ticket spinlock
public:

    using backoff_type = TBackoff;

public:

    //--------------------------------------------------------------------------
    GTS_INLINE FairSpinMutex() : m_nextTicket(0), m_currTicket(0) {}

    //--------------------------------------------------------------------------
    GTS_INLINE FairSpinMutex(FairSpinMutex&& other)
        : m_nextTicket(std::move(other.m_nextTicket.load(memory_order::acquire)))
        , m_currTicket(std::move(other.m_currTicket.load(memory_order::acquire)))
    {}

    //--------------------------------------------------------------------------
    GTS_INLINE FairSpinMutex& operator=(FairSpinMutex&& other)
    {
        if (this != &other)
        {
            m_nextTicket.store(std::move(other.m_nextTicket.load(memory_order::acquire)), memory_order::relaxed);
            m_currTicket.store(std::move(other.m_currTicket.load(memory_order::acquire)), memory_order::relaxed);
        }
        return *this;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE bool try_lock()
    {
        GTS_TRACE_SCOPED_ZONE_P1(analysis::CaptureMask::THREAD_PROFILE, analysis::Color::DarkRed, "TRY LOCK", this);

        uint16_t currTicket = m_currTicket.load(memory_order::acquire);
        uint16_t myTicket = m_nextTicket.load(memory_order::acquire);

        if (currTicket == myTicket)
        {
            return m_nextTicket.compare_exchange_weak(myTicket, myTicket + 1, memory_order::acq_rel, memory_order::relaxed);
        }

        return false;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE void lock()
    {
        GTS_TRACE_SCOPED_ZONE_P1(analysis::CaptureMask::THREAD_PROFILE, analysis::Color::DarkRed, "LOCKING", this);

        // Get the next ticket.
        uint16_t myTicket = m_nextTicket.fetch_add(1, memory_order::acquire);

        // Wait until my ticket is up.
        backoff_type backoff;
        while (m_currTicket.load(memory_order::acquire) != myTicket)
        {
            backoff.tick();
        }
    }

    //--------------------------------------------------------------------------
    GTS_INLINE void unlock()
    {
        // Mark the next ticket as ready.
        m_currTicket.fetch_add(1, memory_order::release);
    }

    //--------------------------------------------------------------------------
    GTS_INLINE bool isLocked() const
    {
        return m_currTicket.load(memory_order::acquire) == m_nextTicket.load(memory_order::acquire);
    }

private:

    Atomic<uint16_t> m_nextTicket;
    Atomic<uint16_t> m_currTicket;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A unfair reader-writer spin mutex.
 */
template<typename TReaders = uint8_t, typename TBackoff = Backoff<BackoffGrowth::Geometric, true>>
class UnfairSharedSpinMutex
{
    static_assert(std::is_integral<TReaders>::value, "TReaders must be an integral type.");
public:

    using backoff_type = TBackoff;
    using mutex_type   = UnfairSpinMutex<backoff_type>;

    //--------------------------------------------------------------------------
    GTS_INLINE UnfairSharedSpinMutex() : m_readersBarrier(0) {}

    //--------------------------------------------------------------------------
    GTS_INLINE UnfairSharedSpinMutex(UnfairSharedSpinMutex&& other)
        : m_writerMutex(std::move(other.m_writerMutex))
        , m_readersBarrier(std::move(other.m_readersBarrier.load(memory_order::acquire)))
    {}

    //--------------------------------------------------------------------------
    GTS_INLINE UnfairSharedSpinMutex& operator=(UnfairSharedSpinMutex&& other)
    {
        if (this != &other)
        {
            m_writerMutex = std::move(other.m_writerMutex);
            m_readersBarrier.store(std::move(other.m_readersBarrier.load(memory_order::acquire)), memory_order::relaxed);
        }
        return *this;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE bool try_lock()
    {
        GTS_TRACE_SCOPED_ZONE_P1(analysis::CaptureMask::THREAD_PROFILE, analysis::Color::DarkRed, "TRY LOCK EXCLUSIVE", this);

        if (m_readersBarrier.load(memory_order::acquire) > 0)
        {
            return false;
        }

        if (!m_writerMutex.try_lock())
        {
            return false;
        }

        if (m_readersBarrier.load(memory_order::relaxed) > 0)
        {
            // A reader got in!
            m_writerMutex.unlock();
            return false;
        }

        return true;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE void lock()
    {
        GTS_TRACE_SCOPED_ZONE_P1(analysis::CaptureMask::THREAD_PROFILE, analysis::Color::DarkRed, "LOCKING EXCLUSIVE", this);

        m_writerMutex.lock();

        // Wait for readers to finish.
        TBackoff backoff;
        while (m_readersBarrier.load(memory_order::relaxed) > 0)
        {
            backoff.tick();
        }
    }

    //--------------------------------------------------------------------------
    GTS_INLINE void downgrade()
    {
        m_readersBarrier.fetch_add(1, memory_order::release);
        m_writerMutex.unlock();
    }

    //--------------------------------------------------------------------------
    GTS_INLINE void unlock()
    {
        m_writerMutex.unlock();
    }

    //--------------------------------------------------------------------------
    GTS_INLINE bool try_lock_shared()
    {
        GTS_TRACE_SCOPED_ZONE_P1(analysis::CaptureMask::THREAD_PROFILE, analysis::Color::DarkRed, "TRY LOCK SHARED", this);

        if (m_writerMutex.isLocked())
        {
            return false;
        }

        // Mark our intent to read.
        m_readersBarrier.fetch_add(1, memory_order::acquire);

        if (m_writerMutex.isLocked())
        {
            // Can't read yet. Remove our mark.
            m_readersBarrier.fetch_sub(1, memory_order::release);
            return false;
        }

        return true;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE void lock_shared()
    {
        GTS_TRACE_SCOPED_ZONE_P1(analysis::CaptureMask::THREAD_PROFILE, analysis::Color::DarkRed, "LOCKING SHARED", this);

        while (true)
        {
            // Mark our intent to read.
            m_readersBarrier.fetch_add(1, memory_order::acquire);

            if (!m_writerMutex.isLocked())
            {
                return; // success
            }

            // Can't read yet. Remove our mark.
            m_readersBarrier.fetch_sub(1, memory_order::release);

            // Wait for writer to finish.
            TBackoff backoff;
            while (m_writerMutex.isLocked())
            {
                backoff.tick();
            }
        }
    }

    //--------------------------------------------------------------------------
    GTS_INLINE void unlock_shared()
    {
        m_readersBarrier.fetch_sub(1, memory_order::release);
    }

    //--------------------------------------------------------------------------
    GTS_INLINE bool isLocked() const
    {
        return m_writerMutex.isLocked();
    }

private:

    mutex_type m_writerMutex;
    Atomic<TReaders> m_readersBarrier;
};

/** @} */ // end of Synchronization

} // namespace gts
