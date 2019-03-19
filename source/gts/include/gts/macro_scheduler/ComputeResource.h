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

#include "gts/macro_scheduler/ComputeResourceType.h"

namespace gts {

class Node;
class ISchedule;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A ComputeResource represents a compute resource that can execute a Workload
 *  in a Node.
 */
class ComputeResource
{
public: // STRUCTORS:

    /**
     * Assign a resource ID.
     */
    ComputeResource();

    /**
     * For polymorphic destruction.
     */
    virtual ~ComputeResource() = default;

public: // ACCESORS:

    /**
     * Executes a 'pNode'.
     */
    virtual ComputeResourceType type() const = 0;

    /**
     * @returns This ComputeResource's uinque id.
     */
    inline uint32_t resourceId() const { return m_id; }

public: // MUTATORS:

    /**
     * Executes a 'pSchedule'.
     */
    virtual void execute(ISchedule* pSchedule) = 0;

private:

    //! The next ID to assign to a ComputeResource object.
    static uint32_t s_nextId;

    //! This ComputeResource's Type.
    uint32_t m_id;
};

} // namespace gts
