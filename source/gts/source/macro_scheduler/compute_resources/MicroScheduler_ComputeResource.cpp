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
#include "gts/macro_scheduler/compute_resources/MicroScheduler_ComputeResource.h"

#include "gts/micro_scheduler/MicroScheduler.h"

#include "gts/macro_scheduler/Node.h"
#include "gts/macro_scheduler/ISchedule.h"
#include "gts/macro_scheduler/IWorkQueue.h"
#include "gts/macro_scheduler/compute_resources/MicroScheduler_Workload.h"

namespace gts {

//------------------------------------------------------------------------------
MicroScheduler_ComputeResource::MicroScheduler_ComputeResource(MicroScheduler* pMicroScheduler)
    : m_pMicroScheduler(pMicroScheduler)
{
    GTS_ASSERT(pMicroScheduler != nullptr);
}

//------------------------------------------------------------------------------
ComputeResourceType MicroScheduler_ComputeResource::type() const
{
    return ComputeResourceType::CPU_MicroScheduler;
}

//------------------------------------------------------------------------------
void MicroScheduler_ComputeResource::execute(ISchedule* pSchedule)
{
    Node* pNode = pSchedule->findQueue(0)->pop();

    GTS_ASSERT(pNode != nullptr);

    MicroScheduler_Workload* pWorkload = (MicroScheduler_Workload*)pNode->findWorkload(ComputeResourceType::CPU_MicroScheduler);
    GTS_ASSERT(pWorkload != nullptr);
    Task* pTask = pWorkload->_buildTask(m_pMicroScheduler);

    // Run it.
    m_pMicroScheduler->spawnTaskAndWait(pTask, pNode->priority());

    // We're done with this Node, reset it.
    pNode->reset();
}

} // namespace gts
