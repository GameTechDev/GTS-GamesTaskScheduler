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
#include "gts/containers/Vector.h"

namespace gts {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Typedef blobs to keep OS header from polluting public interface.
#ifdef GTS_WINDOWS

using ThreadHandle       = void*;
using ConditionVarHandle = void*;
using EventHandle        = void*;

#ifdef GTS_ARCH_X64
struct alignas(uintptr_t)MutexHandle
{
    operator void*() { return &data; }
    uint8_t data[40];
};
#else
struct alignas(uintptr_t)MutexHandle
{
    operator void*() { return &data; }
    uint8_t data[24];
};
#endif

using ThreadId           = uint32_t;
using ThreadFunction     = unsigned long (__stdcall*)(void* pArg);
#define GTS_THREAD_DECL(name) unsigned long __stdcall name(void* pArg)
#define GTS_THREAD_DEFN(cls, name) unsigned long cls::name(void* pArg)

#elif GTS_LINUX

#error "Missing Linux thread implementation."

#elif GTS_MAC

#error "Missing mac thread implementation."

#else // CUSTOM

#error "Missing custom thread implementation."

#endif // END OF TYPEDEF BLOBS.

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A description of a CPU core.
 */
struct CpuCoreInfo
{
    //! The affinity mask for each hardware thread in the core.
    gts::Vector<uintptr_t> logicalAffinityMasks;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A description of the CPU topology.
 */
struct CpuTopology
{
    //! A description of each physical CPU core.
    gts::Vector<CpuCoreInfo> coreInfo;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
class Thread
{
public:

    enum PriorityLevel
    {
        PRIORITY_TIME_CRITICAL = 2,
        PRIORITY_HIGH = 1,
        PRIORITY_NORMAL = 0,
        PRIORITY_LOW = -1
    };

    Thread();
    ~Thread();

    //! Starts a thread.
    bool start(ThreadFunction function, void* pArg);

    //! Waits for a thread to complete.
    bool join();

    //! Destroys the internal resource, if any.
    bool destroy();

    //! Returns the handle's unique ID.
    ThreadId getId() const;

    //! Sets the processor affinity for the thread and returns the previous affinity.
    uintptr_t setAffinity(uintptr_t mask);

    //! Sets the priority for the thread.
    bool setPriority(PriorityLevel priority);

    //! Sets the name of the thread.
    void setName(const char* name);

public:

    //! Gets the topology of all the processors in the system.
    static void getCpuTopology(CpuTopology& out);

    //! Gets the number of logical processor on this system.
    static uint32_t getHardwareThreadCount();

private:

    ThreadHandle m_handle;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
struct ThisThread
{
    //! Yields to another thread in this process. (i.e. SwitchToThread on Win32)
    static void yield();

    //! Yields to another on this processor. (i.e. Sleep(...) on Win32)
    static void sleepFor(size_t milliseconds);

    //! Sets the processor affinity for the thread and returns the previous affinity.
    static uintptr_t setAffinity(uintptr_t mask);

    //! Sets the priority for the thread.
    static bool setPriority(Thread::PriorityLevel priority);

    //! Gets the current threads ID.
    static ThreadId getId();

private:

    ThisThread() = delete;
    ~ThisThread() = delete;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
 /**
 * @breif
 *  Spin wait utilities.
 */
struct SpinWait
{
    SpinWait() = delete;
    ~SpinWait() = delete;

    //--------------------------------------------------------------------------
    GTS_INLINE static void spin(uint32_t count)
    {
        for (; count > 0; --count)
        {
            GTS_PAUSE();
        }
    }

    //--------------------------------------------------------------------------
    /**
     * Pause 'pauseCount' times.
     * @return True if the duration did not exceed 'cyclesLimit'.
     */
    GTS_INLINE static bool spinUntil(uint32_t const pauseCount, uint32_t const cyclesLimit)
    {
        uint64_t start = GTS_RDTSC();

        for (uint32_t pp = 0; pp <= pauseCount; ++pp)
        {
            GTS_PAUSE();
        }

        uint64_t end = GTS_RDTSC();

        return start < end && end - start < cyclesLimit;
    }

    //--------------------------------------------------------------------------
    /**
     * Spin 'spinCountState' times. If the number of spins does not exceeded
     * 'cyclesLimit', then spinCountState *= 2.
     * @return True if the duration did not exceed 'cyclesLimit'.
     */
    GTS_INLINE static bool backoffUntilCycles(uint32_t& spinCountState, uint32_t const cyclesLimit)
    {
        GTS_ASSERT(spinCountState >= 1);
        if (spinUntil(spinCountState, cyclesLimit))
        {
            spinCountState *= 2;
            return true;
        }
        return false;
    }

    //--------------------------------------------------------------------------
    /**
     * Spin 'spinCountState' times. If the number of spins does not exceeded
     * 'maxSpinCount', then spinCountState *= 2.
     * @return True if the spinCountState has not exceeded MAX_SPIN_COUNT.
     */
    GTS_INLINE static bool backoffUntilSpinCount(uint32_t& spinCountState, uint32_t const spinLimit)
    {
        GTS_ASSERT(spinCountState >= 1);
        if (spinCountState <= spinLimit)
        {
            spin(spinCountState);
            spinCountState *= 2;
            return true;
        }
        return false;
    }

    //--------------------------------------------------------------------------
    /**
     * Spin 'spinCountState' times. If the number of spins does not exceeded
     * 'limit', then spinCountState *= 2. Otherwise, yield the thread.
     */
    GTS_INLINE static void backoffUntilSpinCountThenYield(uint32_t& spinCountState, uint32_t const limit)
    {
        if (!backoffUntilSpinCount(spinCountState, limit))
        {
            ThisThread::yield();
        }
    }


    //--------------------------------------------------------------------------
    /**
    * Spin 'spinCountState' times. If the number of spins does not exceeded
    * 'limit', then spinCountState *= 2. Otherwise, yield the thread.
    */
    GTS_INLINE static void backoffUntilCyclesThenYield(uint32_t& spinCountState, uint32_t const cyclesLimit)
    {
        if (!backoffUntilCycles(spinCountState, cyclesLimit))
        {
            ThisThread::yield();
        }
    }
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
template<typename TMutex>
struct Lock
{
    Lock(TMutex& m) : m_mutex(m) { m_mutex.lock(); }
    ~Lock() { m_mutex.unlock(); }

private:
    TMutex& m_mutex;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
class Mutex
{
public:

    explicit Mutex(uint32_t spinCount = 0);
    ~Mutex();

    void lock();
    void unlock();

private:

    friend class BinarySemaphore;

    MutexHandle m_criticalSection;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
class BinarySemaphore
{
public:

    BinarySemaphore();
    ~BinarySemaphore();

    bool isWaiting() const; // Slow!
    void wait();
    void signal();

private:

    EventHandle m_event;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A unfair (first-ready-first-serve) spin mutex.
 */
class UnfairSpinMutex
{
public:

    //--------------------------------------------------------------------------
    GTS_INLINE UnfairSpinMutex() : m_lock(AVAILABLE) {}

    //--------------------------------------------------------------------------
    GTS_INLINE void lock()
    {
        uint32_t spinCount = 1;
        while (true)
        {
            // Test and test-and-set.
            if (m_lock.load(gts::memory_order::acquire) == AVAILABLE &&
                // Only perform the expensive exchange if the lock seems available.
                m_lock.exchange(LOCKED) == AVAILABLE)
            {
                return;
            }
            SpinWait::backoffUntilSpinCount(spinCount, 16);
        }
    }

    //--------------------------------------------------------------------------
    GTS_INLINE void unlock()
    {
        m_lock.store(AVAILABLE, gts::memory_order::release);
    }

private:

    enum { AVAILABLE = 0, LOCKED = 1 };

    gts::Atomic<uint8_t> m_lock;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A fair (first-come-first-serve) spin mutex.
 */
class FairSpinMutex
{
public:

    //--------------------------------------------------------------------------
    GTS_INLINE FairSpinMutex() : m_nextTicket(0), m_currTicket(0) {}

    //--------------------------------------------------------------------------
    GTS_INLINE void lock()
    {
        // Get the next ticket.
        uint16_t myTicket = m_nextTicket.fetch_add(1);

        // Wait until my ticket is up.
        uint32_t spinCount = 1;
        while (m_currTicket.load(gts::memory_order::acquire) != myTicket)
        {
            SpinWait::backoffUntilSpinCount(spinCount, 16);
        }
    }

    //--------------------------------------------------------------------------
    GTS_INLINE void unlock()
    {
        // Mark the next tick as ready.
        m_currTicket.fetch_add(1);
    }

private:

    gts::Atomic<uint16_t> m_nextTicket;
    gts::Atomic<uint16_t> m_currTicket;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A mutex that does nothing. Used for traits and testing.
 */
class NullMutex
{
public:

    GTS_INLINE NullMutex() {}

    GTS_INLINE void lock() {}

    GTS_INLINE void unlock() {}
};


} // namespace gts
