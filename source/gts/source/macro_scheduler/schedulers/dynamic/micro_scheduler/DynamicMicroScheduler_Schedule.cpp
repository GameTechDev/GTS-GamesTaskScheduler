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
#include "gts/macro_scheduler/schedulers/dynamic/micro_scheduler/DynamicMicroScheduler_Schedule.h"

#include <queue>
#include <unordered_set>

#include "gts/platform/Assert.h"
#include "gts/macro_scheduler/Node.h"

namespace gts {

//------------------------------------------------------------------------------
DynamicMicroScheduler_Schedule::DynamicMicroScheduler_Schedule(MacroScheduler* pMyScheduler, Node* pBegin, Node* pEnd)
    : Schedule(pMyScheduler)
    , m_pBegin(pBegin)
    , m_pEnd(pEnd)
    , m_isDone(true)
{
    GTS_ASSERT(pMyScheduler != nullptr);
    GTS_ASSERT(pBegin != nullptr);
    GTS_ASSERT(pEnd != nullptr);
}

//------------------------------------------------------------------------------
Node* DynamicMicroScheduler_Schedule::popNextNode(ComputeResourceId, ComputeResourceType::Enum)
{
    m_pBegin->_setCurrentSchedule(this);
    return m_pBegin;
}

} // namespace gts
