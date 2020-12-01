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

#include "gts/micro_scheduler/MicroSchedulerTypes.h"
#include "gts/macro_scheduler/compute_resources/MicroScheduler_ComputeResource.h"

#include "gts/containers/parallel/QueueMPSC.h"

namespace gts {

class MicroScheduler;

/** 
 * @addtogroup MacroScheduler
 * @{
 */

/** 
 * @addtogroup Implementations
 * @{
 */

/** 
 * @addtogroup Schedulers
 * @{
 */

/** 
 * @addtogroup DynamicMicroScheduler
 * @{
 */

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A ComputeResource that wraps a MicroScheduler.
 */
class DynamicMicroScheduler_ComputeResource : public MicroScheduler_ComputeResource
{
public:

    DynamicMicroScheduler_ComputeResource() = default;

    DynamicMicroScheduler_ComputeResource(MicroScheduler* pMicroScheduler, uint32_t vectorWidth)
        : MicroScheduler_ComputeResource(pMicroScheduler, vectorWidth)
    {}

    void receiveAffinitizedNode(Schedule* pSchedule, Node* pNode);

    virtual bool process(Schedule* pSchedule, bool canBlock) final;

    virtual void spawnReadyChildren(WorkloadContext const& workloadContext, Task* pCurrentTask) final;
};

/** @} */ // end of DynamicMicroScheduler
/** @} */ // end of Schedulers
/** @} */ // end of Implementations
/** @} */ // end of MacroScheduler

} // namespace gts
