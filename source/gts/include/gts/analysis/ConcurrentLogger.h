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

#include <stdarg.h>

// STL used so there are no circular dependencies with GTS implementations of the same.
#include <atomic>
#include <string>
#include <map>
#include <thread>
#include <cassert>

#include "gts/platform/Memory.h"
#include "gts/analysis/TraceCaptureMask.h"

namespace gts {
namespace analysis {

/** 
 * @addtogroup Analysis
 * @{
 */

/** 
 * @addtogroup Logging
 * @{
 */


#ifdef GTS_MSVC
#pragma warning( push )
#pragma warning( disable : 4324) // alignment padding warning
#endif

////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  The log event payload for ConcurrentLogger.
 */
struct alignas(GTS_NO_SHARING_CACHE_LINE_SIZE) ConcurrentLoggerEvent
{
    // Size to pack into GTS_NO_SHARING_CACHE_LINE_SIZE.
    static constexpr size_t MSG_BUFF_SIZE = GTS_NO_SHARING_CACHE_LINE_SIZE
        - sizeof(std::thread::id) // threadid
        - sizeof(bool);           // isMessageBuffer

    //! Thread ID
    std::thread::id tid;
    //! Message data.
    union {
        //! Buffer for sprintf message.
        char buff[MSG_BUFF_SIZE] = {0};
        //! Message and params.
        struct
        {
            const char* msg;
            uintptr_t params[4];
            uint8_t numParams;
        } msgAndParams;
    } message;
    //! Flag 'message' as being stored in 'buff'.
    bool isMessageBuffer = false;
};

////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A serialized log of per-thread messages.
 */
class ConcurrentLogger
{

    template<typename TMutex>
    struct Lock
    {
        Lock(TMutex& m) : m_mutex(m) { m_mutex.lock(); }
        ~Lock() { m_mutex.unlock(); }

    private:

        TMutex& m_mutex;
    };

    class FairSpinMutex
    {
    public:

        //----------------------------------------------------------------------
        GTS_INLINE FairSpinMutex() : m_nextTicket(0), m_currTicket(0) {}

        //----------------------------------------------------------------------
        GTS_INLINE void lock()
        {
            // Get the next ticket.
            uint16_t myTicket = m_nextTicket.fetch_add(1, std::memory_order_acquire);

            // Wait until my ticket is up.
            while (m_currTicket.load(std::memory_order_acquire) != myTicket)
            {
                GTS_PAUSE();
            }
        }

        //----------------------------------------------------------------------
        GTS_INLINE void unlock()
        {
            // Mark the next ticket as ready.
            m_currTicket.fetch_add(1, std::memory_order_release);
        }

    private:

        std::atomic<uint16_t> m_nextTicket;
        std::atomic<uint16_t> m_currTicket;
    };

public:

    GTS_INLINE ConcurrentLogger()
        : m_captureMask((uint64_t)-1) 
        , m_pEventsStack(nullptr)
        , m_pQuiescentEventsStack(nullptr)
        , m_capacity(1)
        , m_pos(0)
    {
        m_pEventsStack = (ConcurrentLoggerEvent*)GTS_ALIGNED_MALLOC(sizeof(ConcurrentLoggerEvent) * m_capacity, GTS_NO_SHARING_CACHE_LINE_SIZE);
    }

    GTS_INLINE ~ConcurrentLogger()
    {
        GTS_ALIGNED_FREE(m_pEventsStack);
        if (m_pQuiescentEventsStack)
        {
            GTS_ALIGNED_FREE(m_pQuiescentEventsStack);
        }
    }

    /**
     * Names a thread.
     */
    GTS_INLINE void nameThread(CaptureMask::Type captureMask, const char* fmt, ...)
    {
        if(captureMask & m_captureMask)
        {
            constexpr size_t BUFF_SIZE = 256;
            char buff[BUFF_SIZE];
            va_list args;
            va_start(args, fmt);
        #if GTS_MSVC
            vsprintf_s(buff, fmt, args);
        #elif GTS_LINUX
            vsnprintf(buff, BUFF_SIZE, fmt, args);
        #endif
            va_end(args);

            Lock<FairSpinMutex> lock(m_accessorMutex);
            if(m_threadNamesById.find(std::this_thread::get_id()) == m_threadNamesById.end())
            {   // don't overwrite existing.
                m_threadNamesById[std::this_thread::get_id()] = buff;
            }
        }
    }

    /**
     * Adds an entry to the log.
     */
    GTS_INLINE void logf(CaptureMask::Type captureMask, const char* fmt, ...)
    {
        if(captureMask & m_captureMask)
        {
            Lock<FairSpinMutex> lock(m_accessorMutex);

            if (m_pos + 1 >= m_capacity)
            {
                _resize();
            }

            // Write an event at this index
            ConcurrentLoggerEvent* e = &m_pEventsStack[m_pos++];

            e->tid             = std::this_thread::get_id();
            e->isMessageBuffer = true;

            va_list args;
            va_start(args, fmt);
        #if GTS_MSVC
            vsprintf_s(e->message.buff, fmt, args);
        #elif GTS_LINUX
            vsnprintf(e->message.buff, ConcurrentLoggerEvent::MSG_BUFF_SIZE, fmt, args);
        #endif
            va_end(args);
        }
    }

    /**
     * Adds an entry to the log.
     */
    GTS_INLINE void logp(CaptureMask::Type captureMask, const char* txt)
    {
        if(captureMask & m_captureMask)
        {
            Lock<FairSpinMutex> lock(m_accessorMutex);

            if (m_pos + 1 >= m_capacity)
            {
                _resize();
            }

            // Write an event at this index
            ConcurrentLoggerEvent* e = &m_pEventsStack[m_pos++];

            e->tid = std::this_thread::get_id();
            e->message.msgAndParams.msg       = txt;
            e->message.msgAndParams.numParams = 0;
            e->isMessageBuffer                = false;
        }
    }

    /**
    * Adds an entry to the log.
    */
    GTS_INLINE void logp(CaptureMask::Type captureMask, const char* txt, uintptr_t param1)
    {
        if(captureMask & m_captureMask)
        {
            Lock<FairSpinMutex> lock(m_accessorMutex);

            if (m_pos + 1 >= m_capacity)
            {
                _resize();
            }

            // Write an event at this index
            ConcurrentLoggerEvent* e = &m_pEventsStack[m_pos++];

            e->tid = std::this_thread::get_id();
            e->message.msgAndParams.msg       = txt;
            e->message.msgAndParams.params[0] = param1;
            e->message.msgAndParams.numParams = 1;
            e->isMessageBuffer                = false;
        }
    }

    /**
    * Adds an entry to the log.
    */
    GTS_INLINE void logp(CaptureMask::Type captureMask, const char* txt, uintptr_t param1, uintptr_t param2)
    {
        if(captureMask & m_captureMask)
        {
            Lock<FairSpinMutex> lock(m_accessorMutex);

            if (m_pos + 1 >= m_capacity)
            {
                _resize();
            }

            // Write an event at this index
            ConcurrentLoggerEvent* e = &m_pEventsStack[m_pos++];

            e->tid = std::this_thread::get_id();
            e->message.msgAndParams.msg       = txt;
            e->message.msgAndParams.params[0] = param1;
            e->message.msgAndParams.params[1] = param2;
            e->message.msgAndParams.numParams = 2;
            e->isMessageBuffer                = false;
        }
    }

    /**
    * Adds an entry to the log.
    */
    GTS_INLINE void logp(CaptureMask::Type captureMask, const char* txt, uintptr_t param1, uintptr_t param2, uintptr_t param3)
    {
        if(captureMask & m_captureMask)
        {
            Lock<FairSpinMutex> lock(m_accessorMutex);

            if (m_pos + 1 >= m_capacity)
            {
                _resize();
            }

            // Write an event at this index
            ConcurrentLoggerEvent* e = &m_pEventsStack[m_pos++];

            e->tid = std::this_thread::get_id();
            e->message.msgAndParams.msg       = txt;
            e->message.msgAndParams.params[0] = param1;
            e->message.msgAndParams.params[1] = param2;
            e->message.msgAndParams.params[2] = param3;
            e->message.msgAndParams.numParams = 3;
            e->isMessageBuffer                = false;
        }
    }

    /**
     * Resets the log to empty.
     * @remark Not thread-safe.
     */
    GTS_INLINE void setCaptureMask(uint64_t captureMask)
    {
        m_captureMask = captureMask;
    }

    /**
     * Resets the log to empty.
     */
    GTS_INLINE void reset(CaptureMask::Type captureMask)
    {
        if(captureMask & m_captureMask)
        {
            Lock<FairSpinMutex> lock(m_accessorMutex);
            m_pos = 0;
        }
    }

    GTS_INLINE void lock()
    {
        m_accessorMutex.lock();
    }

    GTS_INLINE void unlock()
    {
        m_accessorMutex.unlock();
    }

    /** 
     * Get all the events from the logger.
     * @param outEventCount
     *  Outputs number of events being returns.
     * @return
     *  An array of ConcurrentLoggerEvent.
     */
    GTS_INLINE ConcurrentLoggerEvent* getEvents(size_t* outEventCount)
    {
        assert(outEventCount != nullptr);

        *outEventCount = m_pos;
        return m_pEventsStack;
    }

    /** 
     * @return The thread names mapped by TID.
     */
    GTS_INLINE std::map<std::thread::id, std::string> const& getThreadNamesMap() const
    {
        return m_threadNamesById;
    }

private:

    // Resizes the events array.
    GTS_INLINE void _resize()
    {
        // double the buffer capacity
        size_t newCapacity = m_capacity * 2;
        ConcurrentLoggerEvent* pNewStack = (ConcurrentLoggerEvent*)GTS_ALIGNED_MALLOC(sizeof(ConcurrentLoggerEvent) * newCapacity, GTS_NO_SHARING_CACHE_LINE_SIZE);
        assert(pNewStack);

        for (size_t ii = 0; ii < m_pos; ++ii)
        {
            pNewStack[ii] = m_pEventsStack[ii];
        }

        if (m_pQuiescentEventsStack)
        {
            GTS_ALIGNED_FREE(m_pQuiescentEventsStack);
        }
        m_pQuiescentEventsStack = m_pEventsStack;

        m_capacity = newCapacity;
        m_pEventsStack = pNewStack;
    }

    //! Sync access. Must be fair to order events FIFO!
    FairSpinMutex m_accessorMutex;

    //! A mask used to filter events.
    uint64_t m_captureMask;

    // Cacheline ------------------------------

    //! Events stack
    alignas(GTS_CACHE_LINE_SIZE) ConcurrentLoggerEvent* m_pEventsStack;

    //! Old Events stack
    ConcurrentLoggerEvent* m_pQuiescentEventsStack;

    //! The capacity of the events stack.
    size_t m_capacity;

    //! The current position in the ring buffer.
    size_t m_pos;

    std::map<std::thread::id, std::string> m_threadNamesById;
};

#ifdef GTS_MSVC
#pragma warning( pop )
#endif

/**
 * Print all the events to a text file.
 * @param filename
 *  The location and name of the file.
 * @param threadNamesMap
 *  Thread names mapped by TID.
 * @param pEvents
 *  An array of ConcurrentLoggerEvents to print.
 * @param eventCount
 *  The number of events in pEvents.
 */
void printLogToTextFile(
    char const* filename,
    std::map<std::thread::id, std::string> const& threadNamesMap,
    ConcurrentLoggerEvent const* pEvents,
    size_t const eventCount);

/**
 * Print all the events to a CSV file.
 * @param filename
 *  The location and name of the file.
 * @param threadNamesMap
 *  Thread names mapped by TID.
 * @param pEvents
 *  An array of ConcurrentLoggerEvents to print.
 * @param eventCount
 *  The number of events in pEvents.
 */
void printLogToCsvFile(
    char const* filename,
    std::map<std::thread::id, std::string> const& threadNamesMap,
    ConcurrentLoggerEvent const* pEvents,
    size_t const eventCount);


} // namespace analysis
} // namespace gts
