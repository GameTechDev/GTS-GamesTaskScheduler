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
#include "gts/macro_scheduler/MacroSchedulerTypes.h"

namespace gts {

class ComputeResource;
class ISchedule;
class Node;
class DistributedSlabAllocatorBucketed;
class MacroScheduler;

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

    MacroScheduler();

    /**
     * For polymorphic destruction.
     */
    virtual ~MacroScheduler();

public: // MUTATORS:

    /**
     * Initializes the IMacroScheduler object based on 'desc'.
     */
    virtual bool init(MacroSchedulerDesc const& desc) = 0;

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
     * Builds a schedule from the specified DAG that begins at 'pStart' and ends
     * at 'pEnd'.
     * @returns
     *  An ISchedule or nullptr if the DAG is invalid.
     */
    virtual ISchedule* buildSchedule(Node* pStart, Node* pEnd) = 0;

    /**
     * Free an existing ISchedule from memory.
     */
    void freeSchedule(ISchedule* pSchedule);

    /**
     * Executes the specified 'pSchedule'.
     */
    virtual void executeSchedule(ISchedule* pSchedule) = 0;

protected:

    friend class Node;

    template<typename T> 
    void _deleteWorkload(T* ptr)
    {
        if(ptr != nullptr)
        {
            ptr->~T();
        }
        _freeWorkload(ptr);
    }

    void* _allocateWorkload(size_t size, size_t alignment);
    void _freeWorkload(void* ptr);
};

} // namespace gts
