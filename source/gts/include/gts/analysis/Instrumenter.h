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

/**
 * @def GTS_INSTRUMENTER_NAME_THREAD(format, ...)
 * @brief Assigns a name to the current thread.
 * @param format A printf-like format specification.
 * @param ... Arguments for format specification.
 */

/**
 * @def GTS_INSTRUMENTER_BEGIN(tag, message, pParam1, pParam2)
 * @brief Mark the beginning of a region. Must call end to close.
 * @param tag The filter tag this marker belongs to.
 * @param message A message to store in the marker.
 * @param pParam1 A uintptr_t value to store with the message.
 * @param pParam2 A uintptr_t value to store with the message.
 */

/**
 * @def GTS_INSTRUMENTER_END(tag)
 * @brief Mark the end of a region. Must pair with a begin.
 * @param tag The filter tag this marker belongs to.
 */

/**
 * @def GTS_INSTRUMENTER_MARKER(tag)
 * @brief An instantaneous marker.
 * @param tag The filter tag this marker belongs to.
 * @param message A message to store in the marker.
 * @param pParam1 A uintptr_t value to store with the message.
 * @param pParam2 A uintptr_t value to store with the message.
 */

/**
 * @def GTS_INSTRUMENTER_SCOPED(tag)
 * @brief Uses RAII delimit the region in scope.
 * @param tag The filter tag this marker belongs to.
 * @param message A message to store in the marker.
 * @param pParam1 A uintptr_t value to store with the message.
 * @param pParam2 A uintptr_t value to store with the message.
 */


#ifdef GTS_ENABLE_INSTRUMENTER

#define GTS_INSTRUMENTER_NAME_THREAD(format, ...) gts::analysis::getInstrumenterNameThreadFunc()(format, __VA_ARGS__)
#define GTS_INSTRUMENTER_BEGIN(tag, message, pParam1, pParam2) gts::analysis::getInstrumenterBeginFunc()(tag, message, (uintptr_t)pParam1, (uintptr_t)pParam2)
#define GTS_INSTRUMENTER_END(tag) gts::analysis::getInstrumenterEndFunc()(tag)
#define GTS_INSTRUMENTER_MARKER(tag, message, pParam1, pParam2) gts::analysis::getInstrumenterDebugMarkerFunc()(tag, message, (uintptr_t)pParam1, (uintptr_t)pParam2)
#define GTS_INSTRUMENTER_SCOPED(tag, message, pParam1, pParam2) gts::analysis::ScopedInstrumenter GTS_TOKENPASTE2(scopedInstrumenter, __LINE__)(tag, message, (uintptr_t)pParam1, (uintptr_t)pParam2)

#else

#define GTS_INSTRUMENTER_NAME_THREAD(format, ...)
#define GTS_INSTRUMENTER_BEGIN(tag, message, pParam1, pParam2)
#define GTS_INSTRUMENTER_END(tag)
#define GTS_INSTRUMENTER_MARKER(tag, message, pParam1, pParam2)
#define GTS_INSTRUMENTER_SCOPED(tag, message, pParam1, pParam2)

#endif // GTS_ENABLE_INSTRUMENTER

namespace gts {
namespace analysis {

////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A set of flags for filtering on various instrumentation events.
 */
namespace Tag {
    enum Type : uint32_t
    {
        ANY      = 0,
        INTERNAL = 0x0001,

        // vvvvvvvvvvv Add user flags below vvvvvvvvvvvvvv

    };
} // namespace Tag

typedef void (*InstrumenterNameThreadFunc)(const char* format, ...);
typedef void (*InstrumenterBeginFunc)(Tag::Type tag, const char* msg, uintptr_t pParam1, uintptr_t pParam2);
typedef void (*InstrumenterEndFunc)(Tag::Type tag);
typedef void (*InstrumenterDebugMarkerFunc)(Tag::Type tag, const char* msg, uintptr_t pParam1, uintptr_t pParam2);

void setInstrumenterNameThreadFunc(InstrumenterNameThreadFunc hook);
void setInstrumenterBeginFunc(InstrumenterBeginFunc hook);
void setInstrumenterEndFunc(InstrumenterEndFunc hook);
void setInstrumenterDebugMarkerFunc(InstrumenterDebugMarkerFunc hook);

InstrumenterNameThreadFunc getInstrumenterNameThreadFunc();
InstrumenterBeginFunc getInstrumenterBeginFunc();
InstrumenterEndFunc getInstrumenterEndFunc();
InstrumenterDebugMarkerFunc getInstrumenterDebugMarkerFunc();

struct ScopedInstrumenter
{
    ScopedInstrumenter(Tag::Type tag, const char* msg, uintptr_t pParam1, uintptr_t pParam2)
        : m_tag(tag)
        , m_msg(msg)
        , m_pParam1(pParam1)
        , m_pParam2(pParam2)
    {
        GTS_INSTRUMENTER_BEGIN(tag, msg, m_pParam1, m_pParam2);
    }

    ~ScopedInstrumenter()
    {
        GTS_INSTRUMENTER_END(m_tag);
    }

    Tag::Type m_tag;
    const char* m_msg;
    uintptr_t m_pParam1;
    uintptr_t m_pParam2;
};

} // namespace analysis
} // namespace gts
