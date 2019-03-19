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

#include "gts/platform/Machine.h"
#include "gts/platform/Memory.h"
#include "gts/platform/Atomic.h"
#include "gts/platform/Thread.h"
#include "gts/analysis/Instrumenter.h"

namespace gts {
namespace analysis {

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
    ThreadId tid        = 0;                //!> Thread ID
    Tag::Type tag       = Tag::INTERNAL;    //!> Tag for filtering.
    const char* msg     = " ";              //!> Message string
    uintptr_t param1    = 0;                //!> A parameter which can mean anything you want
    uintptr_t param2    = 0;                //!> A parameter which can mean anything you want
};

#ifdef GTS_MSVC
#pragma warning( pop )
#endif

////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A serialized log of per-thread messages.
 */
class ConcurrentLogger
{
public:

    /**
     * Gets the singleton instance of the logger.
     */
    static GTS_INLINE ConcurrentLogger& inst()
    {
        static ConcurrentLogger instance;
        return instance;
    }

    /**
     * Adds an entry to the log.
     */
    GTS_INLINE void log(Tag::Type tag, const char* msg, uintptr_t param1, uintptr_t param2)
    {
        gts::Lock<FairSpinMutex> lock(m_accessorMutex);

        if (m_pos + 1 >= m_capacity)
        {
            _resize();
        }

        // Write an event at this index
        ConcurrentLoggerEvent* e = &m_eventsStack[m_pos++];

        e->tid    = ThisThread::getId();
        e->tag    = tag;
        e->msg    = msg;
        e->param1 = param1;
        e->param2 = param2;
    }

    /**
     * Resets the log to empty.
     */
    GTS_INLINE void reset()
    {
        gts::Lock<FairSpinMutex> lock(m_accessorMutex);
        m_pos = 0;
    }

    /** 
     * Get all the events from the logger.
     * @param outEventCount
     *  Outputs number of events being returns.
     * @return
     *  An array of ConcurrentLoggerEvent.
     */
    ConcurrentLoggerEvent* getEvents(size_t* outEventCount)
    {
        GTS_ASSERT(outEventCount != nullptr);

        gts::Lock<FairSpinMutex> lock(m_accessorMutex);

        *outEventCount = m_pos;
        return m_eventsStack;
    }

    /**
     * A hook for Instrumenter.
     */
    static GTS_INLINE void logHook(Tag::Type tag, const char* msg, uintptr_t param1, uintptr_t param2)
    {
        ConcurrentLogger::inst().log(tag, msg, param1, param2);
    }

private:

    // Initializes the events array.
    ConcurrentLogger() : m_eventsStack(nullptr), m_capacity(1), m_pos(0)
    {
        m_eventsStack = gts::alignedVectorNew<ConcurrentLoggerEvent, GTS_NO_SHARING_CACHE_LINE_SIZE>(m_capacity);
    }

    // Destroys the events array.
    ~ConcurrentLogger()
    {
        gts::alignedVectorDelete(m_eventsStack, m_capacity);
    }

    // Resizes the events array.
    GTS_INLINE void _resize()
    {
        // double the buffer capacity
        size_t newCapacity = m_capacity * 2;
        ConcurrentLoggerEvent* pNewStack = gts::alignedVectorNew<ConcurrentLoggerEvent, GTS_NO_SHARING_CACHE_LINE_SIZE>(newCapacity);

        for (size_t ii = 0; ii < m_pos; ++ii)
        {
            pNewStack[ii] = m_eventsStack[ii];
        }

        gts::alignedVectorDelete(m_eventsStack, m_capacity);

        m_capacity = newCapacity;
        m_eventsStack = pNewStack;
    }

    //! Events stack
    ConcurrentLoggerEvent* m_eventsStack;

    //! The capacity of the events stack;
    size_t m_capacity;

    //! The current position in the ring buffer.
    size_t m_pos;

    //! Sync access. Must be fair to order events properly!
    FairSpinMutex m_accessorMutex;
};

/**
 * Print all the events to a text file.
 * @param filename
 *  The location and name of the file.
 * @param pEvents
 *  An array of ConcurrentLoggerEvents to print.
 * @param eventCount
 *  The number of events in pEvents.
 * @param tagsToInclude
 *  The set of event tags to include.
 */
void printLogToTextFile(
    char const* filename,
    ConcurrentLoggerEvent const* pEvents,
    size_t const eventCount,
    Tag::Type tagsToInclude = gts::analysis::Tag::ANY);

/**
 * Print all the events to a CSV file.
 * @param filename
 *  The location and name of the file.
 * @param pEvents
 *  An array of ConcurrentLoggerEvents to print.
 * @param eventCount
 *  The number of events in pEvents.
 * @param tagsToInclude
 *  The set of event tags to include.
 */
void printLogToCsvFile(
    char const* filename,
    ConcurrentLoggerEvent const* pEvents,
    size_t const eventCount,
    Tag::Type tagsToInclude = gts::analysis::Tag::ANY);


} // namespace analysis
} // namespace gts

#if GTS_ENABLE_CONCRT_LOGGER
#define GTS_CONCRT_LOGGER_RESET() gts::analysis::ConcurrentLogger::inst().reset()
#define GTS_CONCRT_LOGGER_DUMP(filename) { \
    size_t eventCount = 0; \
    gts::analysis::ConcurrentLoggerEvent* pEvents = gts::analysis::ConcurrentLogger::inst().getEvents(&eventCount); \
    gts::analysis::printLogToTextFile(filename, pEvents, eventCount); }
#define GTS_CONCRT_LOGGER_LOG(tag, message, param1, param2) gts::analysis::ConcurrentLogger::inst().log(tag, message, param1, param2)
#else
#define GTS_CONCRT_LOGGER_RESET()
#define GTS_CONCRT_LOGGER_DUMP(filename)
#define GTS_CONCRT_LOGGER_LOG(tag, message, param1, param2)
#endif
