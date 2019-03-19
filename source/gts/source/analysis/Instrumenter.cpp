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
#include "gts/analysis/Instrumenter.h"

#include <unordered_map>

#if defined (GTS_USE_ITT)
#include "gts/analysis/IttNotify.h"
#elif defined (GTS_USE_TELEMETRY)
#include "gts/analysis/Telemetry.h"
#else
#include "gts/analysis/ConcurrentLogger.h"
#endif

namespace gts {
namespace analysis {

#if defined (GTS_USE_ITT)

static InstrumenterNameThreadFunc g_instrumenterNameThreadHook   = IttNotify::nameThreadHook;
static InstrumenterBeginFunc g_instrumenterBeginHook             = IttNotify::beginTaskHook;
static InstrumenterEndFunc g_instrumenterEndHook                 = IttNotify::endTaskHook;
static InstrumenterDebugMarkerFunc g_instrumenterDebugMarkerHook = [](Tag::Type, const char*, uintptr_t, uintptr_t) {};

#elif defined (GTS_USE_TELEMETRY)

static InstrumenterNameThreadFunc g_instrumenterNameThreadHook   = RadTelemetry::nameThreadHook;
static InstrumenterBeginFunc g_instrumenterBeginHook             = RadTelemetry::beginTaskHook;
static InstrumenterEndFunc g_instrumenterEndHook                 = RadTelemetry::endTaskHook;
static InstrumenterDebugMarkerFunc g_instrumenterDebugMarkerHook = RadTelemetry::logHook;

#else

static InstrumenterNameThreadFunc g_instrumenterNameThreadHook   = [](const char*, ...) {};
static InstrumenterBeginFunc g_instrumenterBeginHook             = ConcurrentLogger::logHook;
static InstrumenterEndFunc g_instrumenterEndHook                 = [](Tag::Type) {};
static InstrumenterDebugMarkerFunc g_instrumenterDebugMarkerHook = ConcurrentLogger::logHook;

#endif

//------------------------------------------------------------------------------
void setInstrumenterNameThreadFunc(InstrumenterNameThreadFunc hook)
{
    g_instrumenterNameThreadHook = hook;
}

//------------------------------------------------------------------------------
void setInstrumenterBeginFunc(InstrumenterBeginFunc hook)
{
    g_instrumenterBeginHook = hook;
}

//------------------------------------------------------------------------------
void setInstrumenterEndFunc(InstrumenterEndFunc hook)
{
    g_instrumenterEndHook = hook;
}

//------------------------------------------------------------------------------
void setInstrumenterDebugMarkerFunc(InstrumenterDebugMarkerFunc hook)
{
    g_instrumenterDebugMarkerHook = hook;
}

//------------------------------------------------------------------------------
InstrumenterNameThreadFunc getInstrumenterNameThreadFunc()
{
    return g_instrumenterNameThreadHook;
}

//------------------------------------------------------------------------------
InstrumenterBeginFunc getInstrumenterBeginFunc()
{
    return g_instrumenterBeginHook;
}

//------------------------------------------------------------------------------
InstrumenterEndFunc getInstrumenterEndFunc()
{
    return g_instrumenterEndHook;
}

//------------------------------------------------------------------------------
InstrumenterDebugMarkerFunc getInstrumenterDebugMarkerFunc()
{
    return g_instrumenterDebugMarkerHook;
}

} // namespace analysis
} // namespace gts
