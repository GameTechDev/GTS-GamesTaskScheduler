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
#include "gts/macro_scheduler/MacroSchedulerTypes.h"

namespace gts {

class ComputeResource;
class Schedule;
class Node;
class DistributedSlabAllocatorBucketed;
class MacroScheduler;

/** 
 * @defgroup MacroScheduler
 *  Coarse-grained, persistent task scheduling framework.
 * @{
 */

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A MacroScheduler builds ISchedules for a set of ComputeResource%s
 *  from a DAG of Node.
 */
class MacroScheduler
{
public: // STRUCTORS:

    MacroScheduler();

    /**
     * For polymorphic destruction.
     */
    virtual ~MacroScheduler();

public: // LIFETIME:

    /**
     * Initializes the IMacroScheduler object based on 'desc'.
     */
    virtual bool init(MacroSchedulerDesc const& desc) = 0;

public: // ACCESSORS:

    /**
     * @return A ComputeResource or nullptr if DNE.
     */
    ComputeResource* findComputeResource(ComputeResourceId id);

    /**
     * @return Get a list of all the ComputeResource.
     */
    Vector<ComputeResource*> const& computeResources() const;

public: // MUTATORS:

    /**
     * Allocate a Node that can be inserted into a DAG.
     * @remark Adding a Node to multiple DAGs is undefined.
     */
    Node* allocateNode();

    /**
     * Free pNode from memory.
     */
    void destroyNode(Node* pNode);

    /**
     * Free an existing Schedule from memory.
     */
    virtual void freeSchedule(Schedule* pSchedule) = 0;

    /**
     * Builds a schedule from the specified DAG that begins at 'pStart' and ends
     * at 'pEnd'.
     * @returns
     *  An Schedule or nullptr if the DAG is invalid.
     */
    virtual Schedule* buildSchedule(Node* pStart, Node* pEnd) = 0;

    /**
     * Executes the specified 'pSchedule' and uses the specified ComputeResource
     * 'id' for the calling thread. Setting 'id' to UNKNOWN_COMP_RESOURCE will
     * not use the calling thread.
     */
    virtual void executeSchedule(Schedule* pSchedule, ComputeResourceId id) = 0;

protected:

    friend class Node;

    void* _allocateWorkload(size_t size);
    void _freeWorkload(void* ptr);

protected:

    Vector<ComputeResource*> m_computeResources;
};

/** @} */ // end of MacroScheduler

} // namespace gts
