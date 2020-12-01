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

#include "gts/macro_scheduler/MacroScheduler.h"

namespace gts {

class MicroScheduler_ComputeResource;

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
 *  A generalized DAG scheduler utilizing work stealing. This scheduler delegates
 *  its responsibilities to a MicroScheduler.
 */
class DynamicMicroScheduler_MacroScheduler : public MacroScheduler
{
public: // LIFETIME:

    virtual bool init(MacroSchedulerDesc const& desc) final;

public: // MUTATORS:

    virtual Schedule* buildSchedule(Node* pStart, Node* pEnd) final;

    virtual void freeSchedule(Schedule* pSchedule) final;

    virtual void executeSchedule(Schedule* pSchedule, ComputeResourceId id) final;
};

/** @} */ // end of DynamicMicroScheduler
/** @} */ // end of Schedulers
/** @} */ // end of Implementations
/** @} */ // end of MacroScheduler

} // namespace gts
