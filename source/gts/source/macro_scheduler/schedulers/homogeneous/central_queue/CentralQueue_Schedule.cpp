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
#include "gts/macro_scheduler/schedulers/homogeneous/central_queue/CentralQueue_Schedule.h"

#include "gts/platform/Assert.h"
#include "gts/macro_scheduler/Node.h"
#include "gts/macro_scheduler/compute_resources/MicroScheduler_ComputeResource.h"

namespace gts {

//------------------------------------------------------------------------------
CentralQueue_Schedule::CentralQueue_Schedule(MacroScheduler* pMyScheduler, Node* pSource, Node* pSink)
    : Schedule(pMyScheduler)
    , m_pSource(pSource)
    , m_pSink(pSink)
    , m_isDone(true)
{
    GTS_ASSERT(pMyScheduler != nullptr);
    GTS_ASSERT(pSource != nullptr);
    GTS_ASSERT(pSink != nullptr);
    GTS_ASSERT(pMyScheduler->computeResources().size() > 0);

    // Populate the affinity queues.
    auto computeResources = pMyScheduler->computeResources();
    for (auto* pCompResource : computeResources)
    {
        m_affinityQueues.push_back({ pCompResource, QueueMPMC<Node*>() });
    }
}

//------------------------------------------------------------------------------
Node* CentralQueue_Schedule::popNextNode(ComputeResource* pComputeResource, bool)
{
    GTS_TRACE_SCOPED_ZONE_P0(gts::analysis::CaptureMask::MACRO_SCHEDULER_PROFILE, gts::analysis::Color::Gold1, "Pop Next Node");
    Node* pNode = nullptr;

    for (auto& affinityQueue : m_affinityQueues)
    {
        ComputeResource* pCompResource = affinityQueue.pComputeResource;
        if (pComputeResource->id() == pCompResource->id())
        {
            if (affinityQueue.readyQueue.tryPop(pNode))
            {
                GTS_TRACE_SCOPED_ZONE_P1(gts::analysis::CaptureMask::MACRO_SCHEDULER_PROFILE, gts::analysis::Color::Gold, "Popped Affinity Node at", pCompResource->id());
                return pNode;
            }
            break;
        }
    }

    if (m_readyQueue.tryPop(pNode))
    {
        GTS_TRACE_SCOPED_ZONE_P0(gts::analysis::CaptureMask::MACRO_SCHEDULER_PROFILE, gts::analysis::Color::Gold2, "Popped Node");
        return pNode;
    }

    return pNode;
}

//------------------------------------------------------------------------------
void CentralQueue_Schedule::insertReadyNode(Node* pNode)
{
    GTS_TRACE_SCOPED_ZONE_P0(gts::analysis::CaptureMask::MACRO_SCHEDULER_PROFILE, gts::analysis::Color::Gold, "Insert Ready Node");

    pNode->_setCurrentSchedule(this);

    // Node has affinity so ship it to that queue.
    if (pNode->affinity() != ANY_COMP_RESOURCE)
    {
        for (auto& affinityQueue : m_affinityQueues)
        {
            ComputeResource* pCompResource = affinityQueue.pComputeResource;
            if (pNode->affinity() == pCompResource->id())
            {
                GTS_TRACE_SCOPED_ZONE_P1(gts::analysis::CaptureMask::MACRO_SCHEDULER_PROFILE, gts::analysis::Color::Gold, "Inserting Affinity Node at", pCompResource->id());
                affinityQueue.readyQueue.tryPush(pNode);
                pCompResource->notify(this);
                return;
            }
        }
    }

    m_readyQueue.tryPush(pNode);
}

} // namespace gts
