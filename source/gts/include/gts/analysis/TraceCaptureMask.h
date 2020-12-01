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

#include <cstdint>

namespace gts {
namespace analysis {
namespace CaptureMask {

/** 
 * @addtogroup Analysis
 * @{
 */

/** 
 * @addtogroup Tracing
 * @{
 */

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A set of flags for filtering on various tracing events.
 */
enum Type : uint64_t
{
    // These are the symbols GTS uses. Feel free to alter the values to fit
    // your system's values.

    ALL                      = (uint64_t)(-1),
    WORKERPOOL_DEBUG         = 1 << 0,
    WORKERPOOL_PROFILE       = 1 << 1,
    WORKERPOOL_ALL           = WORKERPOOL_DEBUG | WORKERPOOL_PROFILE,
    MICRO_SCHEDULER_DEBUG    = 1 << 2,
    MICRO_SCHEDULER_PROFILE  = 1 << 3,
    MICRO_SCHEDULER_ALL      = MICRO_SCHEDULER_DEBUG | MICRO_SCHEDULER_PROFILE,
    THREAD_DEBUG             = 1 << 4,
    THREAD_PROFILE           = 1 << 5,
    THREAD_ALL               = THREAD_DEBUG | THREAD_PROFILE,
    BINNED_ALLOCATOR_DEBUG   = 1 << 6,
    BINNED_ALLOCATOR_PROFILE = 1 << 7,
    BINNED_ALLOCATOR_ALL     = BINNED_ALLOCATOR_DEBUG | BINNED_ALLOCATOR_PROFILE,
    MACRO_SCHEDULER_DEBUG    = 1 << 8,
    MACRO_SCHEDULER_PROFILE  = 1 << 9,
    MACRO_SCHEDULER_ALL      = MACRO_SCHEDULER_DEBUG | MACRO_SCHEDULER_PROFILE,

    USER                     = 1 << 10

};

/** @} */ // end of Tracing
/** @} */ // end of Analysis

} // namespace CaptureMask
} // namespace analysis
} // namespace gts
