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
#include "gts/macro_scheduler/schedulers/work_sharing/WorkSharing_Schedule.h"

#include "gts/platform/Assert.h"

namespace gts {

//------------------------------------------------------------------------------
WorkSharing_Schedule::WorkSharing_Schedule(WorkSharing_WorkQueue* pQueue, Node* pEnd)
    : m_pQueue(pQueue)
    , m_pEnd(pEnd)
{
    GTS_ASSERT(pQueue != nullptr);
}

//------------------------------------------------------------------------------
IWorkQueue* WorkSharing_Schedule::findQueue(uint32_t) const
{
    return m_pQueue;
}

//------------------------------------------------------------------------------
void WorkSharing_Schedule::tryMarkDone(Node* pNode)
{
    if (pNode == m_pEnd)
    {
        m_done = true;
        GTS_MFENCE();
    }
}

//------------------------------------------------------------------------------
bool WorkSharing_Schedule::done() const
{
    return m_done;
}

} // namespace gts
