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

#include "gts/macro_scheduler/ISchedule.h"
#include "gts/macro_scheduler/schedulers/work_sharing/WorkSharing_WorkQueue.h"

namespace gts {

class IWorkQueue;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  The execution schedule for all ComputeResources.
 */
class WorkSharing_Schedule : public ISchedule
{
public: // ACCESSORS:

    WorkSharing_Schedule(WorkSharing_WorkQueue* pQueue, Node* pEnd);

    /**
     * Find the schedule queue for the specified resourceId.
     * @returns
     *  An IWorkQueue or nullptr if the resourceId is unknown.
     */
    virtual IWorkQueue* findQueue(uint32_t resourceId) const final;

    virtual void tryMarkDone(Node* pNode) final;

    virtual bool done() const final;

private:

    WorkSharing_WorkQueue* m_pQueue = nullptr;
    Node* m_pEnd = nullptr;
    bool m_done = false;
};

} // namespace gts
