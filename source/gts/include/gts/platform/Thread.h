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
#include "gts/containers/Vector.h"

namespace gts {

/**
 * @addtogroup Platform
 * @{
 */

/**
 * @defgroup Thread Thread
 *  Wrappers around OS threading libraries.
 * @{
 */

using ThreadFunction = void(*)(void* pArg);

#ifndef GTS_HAS_CUSTOM_THREAD_WRAPPERS

#ifndef DOXYGEN_SHOULD_SKIP_THIS

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
#ifdef GTS_WINDOWS
    #define GTS_CONTEXT_SWITCH_CYCLES 1000
    #define GTS_THREAD_AFFINITY_ALL UINTPTR_MAX

    #define GTS_THREAD_PRIORITY_HIGHEST 2
    #define GTS_THREAD_PRIORITY_ABOVE_NORMAL 1
    #define GTS_THREAD_PRIORITY_NORMAL 0
    #define GTS_THREAD_PRIORITY_BELOW_NORMAL -1
    #define GTS_THREAD_PRIORITY_LOWEST -2

    #define GTS_THREAD_FUNCTION_DECL(name) unsigned long __stdcall name(void* pArg)
    #define GTS_THREAD_FUNCTION_DEFN(cls, name) unsigned long cls::name(void* pArg)

    using ThreadHandle = void*;
    using ThreadId     = uint32_t;

    class AffinitySet
    {
    public:
        GTS_INLINE void set(uint32_t cpuId)
        {
            GTS_ASSERT(cpuId < 64);
            m_bitset |= uintptr_t(1) << cpuId;
        }
        GTS_INLINE void combine(AffinitySet const& other)
        {
            m_bitset |= other.m_bitset;
        }
        GTS_INLINE bool empty() const
        {
            return m_bitset == 0;
        }
    private:
        uintptr_t m_bitset = 0;
    };

#elif GTS_POSIX
    #define GTS_CONTEXT_SWITCH_CYCLES 1000
    #define GTS_THREAD_AFFINITY_ALL UINTPTR_MAX

    // Priorities correspond to "nice" values.
    #define GTS_THREAD_PRIORITY_HIGHEST -2
    #define GTS_THREAD_PRIORITY_ABOVE_NORMAL -1
    #define GTS_THREAD_PRIORITY_NORMAL 0
    #define GTS_THREAD_PRIORITY_BELOW_NORMAL 1
    #define GTS_THREAD_PRIORITY_LOWEST 0

    #define GTS_THREAD_FUNCTION_DECL(name) void* name(void* pArg)
    #define GTS_THREAD_FUNCTION_DEFN(cls, name) void* cls::name(void* pArg)

    using ThreadHandle = unsigned long;
    using ThreadId     = int;

    class AffinitySet
    {
    #if GTS_64BIT
        static constexpr uint32_t MAX_SETS = 16;
    #else
        static constexpr uint32_t MAX_SETS = 32;
    #endif

    public:
        GTS_INLINE void set(uint32_t cpuId)
        {
            GTS_ASSERT(cpuId < sizeof(unsigned long) * MAX_SETS * 8);
            m_bitset[cpuId / MAX_SETS] |= 1 << (cpuId % MAX_SETS);
        }
        GTS_INLINE void combine(AffinitySet const& other)
        {
            for(uint32_t ii = 0; ii < MAX_SETS; ++ii)
            {
                m_bitset[ii] |= other.m_bitset[ii];
            }
        }
        GTS_INLINE bool empty() const
        {
            for(uint32_t ii = 0; ii < MAX_SETS; ++ii)
            {
                if(m_bitset[ii] != 0)
                {
                    return false;
                }
            }
            return true;
        }
    private:
        GTS_ALIGN(sizeof(void*)) size_t m_bitset[MAX_SETS] = {0};
    };

#endif // GTS_WINDOWS

#endif // DOXYGEN_SHOULD_SKIP_THIS

/**
 * @brief
 *  A description of a CPU core.
 */
struct CpuCoreInfo
{
    GTS_INLINE ~CpuCoreInfo()
    {
        delete[] pHardwareThreadIds;
    }

    //! The ID for each hardware thread in the core.
    uint32_t* pHardwareThreadIds = nullptr;
    //! The number of elements in pHardwareThreadIds.
    size_t hardwareThreadIdCount = 0;
    //! The efficiency of this core. {0,.., n} => {min,.., max}.
    uint8_t efficiencyClass = 0;
};

/**
 * @brief
 *  A description of a socket.
 */
struct SocketInfo
{
    GTS_INLINE ~SocketInfo()
    {
        delete[] pCoreInfoArray;
    }
    //! A array of descriptions for each processor core in the socket.
    CpuCoreInfo* pCoreInfoArray = nullptr;
    //! The number of elements in pCoreInfoArray.
    size_t coreInfoElementCount = 0;
};

/**
 * @brief
 *  A description of a NUMA node.
 */
struct NumaNodeInfo
{
    GTS_INLINE ~NumaNodeInfo()
    {
        delete[] pCoreInfoArray;
    }
    //! A array of descriptions for each processor core in the node.
    CpuCoreInfo* pCoreInfoArray = nullptr;
    //! The number of elements in pCoreInfoArray.
    size_t coreInfoElementCount = 0;
    //! The ID of the node.
    size_t nodeId = SIZE_MAX;
};

/**
 * @brief
 *  A description of a processor group.
 */
struct ProcessorGroupInfo
{
    GTS_INLINE ~ProcessorGroupInfo()
    {
        delete[] pCoreInfoArray;
        delete[] pNumaInfoArray;
        delete[] pSocketInfoArray;
    }

    //! A array of descriptions for each processor core in the group.
    CpuCoreInfo* pCoreInfoArray = nullptr;
    //! The number of elements in pCoreInfoArray.
    size_t coreInfoElementCount = 0;
    //! A array of descriptions for each NUMA node.
    NumaNodeInfo* pNumaInfoArray = nullptr;
    //! The number of elements in pNumaInfoArray.
    size_t numaNodeInfoElementCount = 0;
    //! A array of descriptions for each socket.
    SocketInfo* pSocketInfoArray = nullptr;
    //! The number of elements in pSocketInfoArray.
    size_t socketInfoElementCount = 0;
    //! The ID of the group.
    uint32_t groupId = 0;
};

/**
 * @brief
 *  A description of the system's processor topology.
 */
struct SystemTopology
{
    GTS_INLINE ~SystemTopology()
    {
        delete[] pGroupInfoArray;
    }

    //! A array of descriptions for each processor group.
    ProcessorGroupInfo* pGroupInfoArray = nullptr;
    //! The number of elements in pGroupInfoArray.
    size_t groupInfoElementCount = 0;
};

#ifndef DOXYGEN_SHOULD_SKIP_THIS

namespace internal {
struct Thread
{
    Thread() = delete;
    ~Thread() = delete;

    static bool createThread(ThreadHandle& handle, ThreadId& tid, ThreadFunction func, void* arg, size_t stackSize);
    static bool join(ThreadHandle& handle);
    static bool destroy(ThreadHandle& handle);
    static bool setGroupAffinity(ThreadHandle& handle, size_t groupId, AffinitySet const& affinity);
    static bool setPriority(ThreadId& tid, int32_t priority);
    static void setThisThreadName(const char* name);
    static ThreadId getThisThreadId();
    static ThreadHandle getThisThread();
    static bool setThisGroupAffinity(size_t groupId, AffinitySet const& affinity);
    static bool setThisPriority(int32_t priority);
    static void yield();
    static void sleep(uint32_t milliseconds);
    static uint32_t getHardwareThreadCount();
    static void getSytemTopology(SystemTopology& out);
    static void getCurrentProcessorId(uint32_t& socketId, uint32_t& hwTid);
};
} // namespace internal

#define GTS_CREATE_THREAD(threadHandle, tid, func, arg, stackSize) internal::Thread::createThread(threadHandle, tid, func, arg, stackSize)
#define GTS_JOIN_THREAD(threadHandle) internal::Thread::join(threadHandle)
#define GTS_DESTROY_THREAD(threadHandle) internal::Thread::destroy(threadHandle)
#define GTS_SET_THREAD_GROUP_AFFINITY(threadHandle, groupId, affinity) internal::Thread::setGroupAffinity(threadHandle, groupId, affinity)
#define GTS_SET_THREAD_PRIORITY(threadId, priority) internal::Thread::setPriority(threadId, priority)
#define GTS_SET_THIS_THREAD_NAME(name) internal::Thread::setThisThreadName(name)
#define GTS_GET_THIS_THREAD_ID() internal::Thread::getThisThreadId()
#define GTS_GET_THIS_THREAD() internal::Thread::getThisThread()
#define GTS_SET_THIS_THREAD_GROUP_AFFINITY(groupId, affinity) internal::Thread::setThisGroupAffinity(groupId, affinity)
#define GTS_SET_THIS_THREAD_PRIORITY(priority) internal::Thread::setThisPriority(priority)
#define GTS_YIELD_THREAD() internal::Thread::yield()
#define GTS_THREAD_SLEEP(milliseconds) internal::Thread::sleep(milliseconds)
#define GTS_GET_HARDWARE_THREAD_COUNT() internal::Thread::getHardwareThreadCount()
#define GTS_GET_SYSTEM_TOPOLOGY(topology) internal::Thread::getSytemTopology(topology)
#define GTS_GET_CURRENT_PROCESSOR_ID(groupId, hwTid) internal::Thread::getCurrentProcessorId(groupId, hwTid)

#else

    using ThreadHandle       = GTS_THREAD_HANDLE_TYPE;
    using ThreadId           = GTS_THREAD_ID_TYPE;
    using ThreadAffinityType = GTS_THREAD_AFFINITY_TYPE;
    using CpuCoreInfo        = GTS_CPU_CORE_INFO;
    using SocketInfo         = GTS_SOCKET_CORE_INFO;
    using NumaNodeInfo       = GTS_NUMA_NODE_INFO;
    using ProcessorGroupInfo = GTS_PROCESSOR_GROUP_INFO;
    using SystemTopology     = GTS_SYSTEM_TOPOLOGY;

#endif // DOXYGEN_SHOULD_SKIP_THIS

#endif // GTS_HAS_CUSTOM_THREAD_WRAPPERS

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
#ifndef GTS_HAS_CUSTOM_EVENT_WRAPPERS

#ifndef DOXYGEN_SHOULD_SKIP_THIS

#ifdef GTS_WINDOWS
    #define GTS_WAIT_FOREVER uint32_t(-1)
    struct EventHandle
    {
    #ifdef GTS_64BIT
        struct MutexHandle { GTS_ALIGN(uintptr_t) uint8_t v[40]; } mutex;
        struct CondVar { uintptr_t v; } condVar;
    #else
        struct MutexHandle { GTS_ALIGN(uintptr_t) uint8_t v[24]; } mutex;
        struct CondVar { uintptr_t v; } condVar;
    #endif
        Atomic<bool> signaled = { false };
        bool waiting = false;
    };
#elif GTS_LINUX
    #define GTS_WAIT_FOREVER uint32_t(-1)
    struct EventHandle
    {
    #ifdef GTS_64BIT
        struct MutexHandle { GTS_ALIGN(uintptr_t) uint8_t v[40]; } mutex;
        struct CondVar { GTS_ALIGN(uintptr_t) uint8_t v[48]; } condVar;
    #else
        struct MutexHandle { GTS_ALIGN(uintptr_t) uint8_t v[24]; } mutex;
        struct CondVar { GTS_ALIGN(uintptr_t) uint8_t v[48]; } condVar;
    #endif
        Atomic<bool> signaled = { false };
        bool waiting = false;
    };
#elif GTS_MAC
    #error "Missing mac thread implementation."
#else
    #error "Missing custom thread implementation."
#endif // GTS_WINDOWS

namespace internal {
struct Event
{
    Event() = delete;
    ~Event() = delete;

    static bool createEvent(EventHandle& handle);
    static bool destroyEvent(EventHandle& handle);
    static bool waitForEvent(EventHandle& handle, bool waitForever);
    static bool signalEvent(EventHandle& handle);
    static bool resetEvent(EventHandle& handle);
};
} // namespace internal

#define GTS_CREATE_EVENT(eventHandle) internal::Event::createEvent(eventHandle);
#define GTS_DESTROY_EVENT(eventHandle) internal::Event::destroyEvent(eventHandle);
#define GTS_WAIT_FOR_EVENT(eventHandle, waitForever) internal::Event::waitForEvent(eventHandle, waitForever);
#define GTS_SIGNAL_EVENT(eventHandle) internal::Event::signalEvent(eventHandle);
#define GTS_RESET_EVENT(eventHandle) internal::Event::resetEvent(eventHandle);

#else

    using EventHandle = GTS_EVENT_HANDLE_TYPE;

#endif // DOXYGEN_SHOULD_SKIP_THIS

#endif // GTS_HAS_CUSTOM_EVENT_WRAPPERS

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A wrapper around the platform's threading system.
 */
class Thread
{
public:

    enum class Priority : int32_t
    {
        PRIORITY_HIGHEST      = GTS_THREAD_PRIORITY_HIGHEST,
        PRIORITY_ABOVE_NORMAL = GTS_THREAD_PRIORITY_ABOVE_NORMAL,
        PRIORITY_NORMAL       = GTS_THREAD_PRIORITY_NORMAL,
        PRIORITY_BELOW_NORMAL = GTS_THREAD_PRIORITY_BELOW_NORMAL,
        PRIORITY_LOWEST       = GTS_THREAD_PRIORITY_LOWEST,
    };

    enum
    {
        DEFAULT_STACK_SIZE = 0
    };

    GTS_INLINE Thread()
        : m_handle()
    {}

    GTS_INLINE ~Thread()
    {
        GTS_DESTROY_THREAD(m_handle);
    }

    //! Starts a thread.
    GTS_INLINE bool start(ThreadFunction function, void* pArg, uint32_t stackSize = DEFAULT_STACK_SIZE)
    {
        return GTS_CREATE_THREAD(m_handle, m_tid, function, pArg, stackSize);
    }

    GTS_INLINE bool join()
    {
        bool result = GTS_JOIN_THREAD(m_handle);
        return result;
    }

    //! Destroys the internal resource, if any.
    GTS_INLINE bool destroy()
    {
        return GTS_DESTROY_THREAD(m_handle);
    }

    //! Returns the handle's unique ID.
    GTS_INLINE ThreadId getId()
    {
        return m_tid;
    }

    //! Sets the processor affinity for the thread and returns the previous affinity.
    GTS_INLINE bool setAffinity(size_t groupId, AffinitySet const& affinity)
    {
        return GTS_SET_THREAD_GROUP_AFFINITY(m_handle, groupId, affinity);
    }

    //! Sets the priority for the thread.
    GTS_INLINE bool setPriority(Priority priority)
    {
        return GTS_SET_THREAD_PRIORITY(m_tid, (int32_t)priority);
    }

public:

    //! Gets the topology of all the processors in the system.
    GTS_INLINE static void getSystemTopology(SystemTopology& out)
    {
        GTS_GET_SYSTEM_TOPOLOGY(out);
    }

    //! Gets the number of logical processor on this system.
    GTS_INLINE static uint32_t getHardwareThreadCount()
    {
        return GTS_GET_HARDWARE_THREAD_COUNT();
    }

    //! Gets the current group and hardware thread IDs.
    GTS_INLINE static void getCurrentProcessorId(uint32_t& groupId, uint32_t& hwTid)
    {
        return GTS_GET_CURRENT_PROCESSOR_ID(groupId, hwTid);
    }

private:

    ThreadHandle m_handle;
    ThreadId m_tid;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A set of utilities for the calling thread.
 */
struct ThisThread
{
    //! Yields to another thread on this CPU. (i.e. SwitchToThread on Win32)
    GTS_INLINE static void yield()
    {
        GTS_YIELD_THREAD();
    }

    //! Yields to another thread in this process. (i.e. Sleep(...) on Win32)
    GTS_INLINE static void sleepFor(uint32_t milliseconds)
    {
        GTS_THREAD_SLEEP(milliseconds);
    }

    //! Gets the current threads ID.
    GTS_INLINE static ThreadId getId()
    {
        return GTS_GET_THIS_THREAD_ID();
    }

    //! Sets the name of the thread.
    GTS_INLINE static void setName(const char* name)
    {
        GTS_SET_THIS_THREAD_NAME(name);
    }

    //! Sets the processor affinity for the thread and returns the previous affinity.
    GTS_INLINE static bool setAffinity(size_t groupId, AffinitySet const& affinity)
    {
        return GTS_SET_THIS_THREAD_GROUP_AFFINITY(groupId, affinity);
    }

    //! Sets the priority for the thread.
    GTS_INLINE static bool setPriority(Thread::Priority priority)
    {
        return GTS_SET_THIS_THREAD_PRIORITY((int32_t)priority);
    }

private:

    ThisThread() = delete;
    ~ThisThread() = delete;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A semaphore that only allows access to one object.
 */
class BinarySemaphore
{
public:

    BinarySemaphore()
    {
        GTS_CREATE_EVENT(m_event);
    }

    ~BinarySemaphore()
    {
        GTS_DESTROY_EVENT(m_event);
    }

    bool isWaiting()
    {
        return GTS_WAIT_FOR_EVENT(m_event, false);
    }

    /**
     *  @return True if the event was waited on, false otherwise.
     */
    bool wait()
    {
        return GTS_WAIT_FOR_EVENT(m_event, true);
    }

    void signal()
    {
        GTS_SIGNAL_EVENT(m_event);
    }

    void reset()
    {
        GTS_RESET_EVENT(m_event);
    }

private:

    EventHandle m_event;
};

/** @} */ // end of Thread

} // namespace gts
