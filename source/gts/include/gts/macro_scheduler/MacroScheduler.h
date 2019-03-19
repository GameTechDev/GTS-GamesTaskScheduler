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
#include "gts/containers/Vector.h"
#include "gts/macro_scheduler/ComputeResourceType.h"
#include "gts/macro_scheduler/Node.h"
#include "gts/macro_scheduler/ISchedule.h"

namespace gts {

class ComputeResource;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  The description of a MacroSchedulerDesc to create.
 */
struct MacroSchedulerDesc
{
    //! The ComputeResource per type that a MacroScheduler will schedule to.
    gts::Vector<ComputeResource*> computeResourcesByType[(uint32_t)ComputeResourceType::COUNT];
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A MacroScheduler builds ISchedules for a set of ComputeResources
 *  from a DAG of Nodes.
 */
class MacroScheduler
{
public: // STRUCTORS:

    /**
     * For polymorphic destruction.
     */
    virtual ~MacroScheduler() {}

public: // MUTATORS:

    /**
     * Initializes the IMacroScheduler object base on 'desc'.
     */
    virtual bool init(MacroSchedulerDesc const& desc) = 0;

    /**
     * Allocate a Node that can be scheduled.
     */
    Node* allocateNode() { return new Node; }

    /**
     * Free an existing Node from memory.
     */
    void freeNode(Node* pNode) { delete pNode; }

    /**
     * Builds a schedule from the specified DAG that begins at 'pStart' and ends
     * at 'pEnd'.
     * @returns
     *  An ISchedule or nullptr if the DAG is invalid.
     */
    virtual ISchedule* buildSchedule(Node* pStart, Node* pEnd) = 0;

    /**
     * Free an existing ISchedule from memory.
     */
    void freeSchedule(ISchedule* pSchedule) { delete pSchedule; pSchedule = nullptr; }

    /**
     * Executes the specified 'pSchedule'.
     */
    virtual void executeSchedule(ISchedule* pSchedule) = 0;
};

} // namespace gts