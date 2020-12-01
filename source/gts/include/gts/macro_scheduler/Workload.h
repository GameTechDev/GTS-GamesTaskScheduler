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
#include <map>

#include "gts/platform/Machine.h"
#include "gts/macro_scheduler/MacroSchedulerTypes.h"


namespace gts {

class ComputeResource;
class Workload;
class Node;

/** 
 * @addtogroup MacroScheduler
 * @{
 */

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A Workload is a base class for describing a task that can be executed by a
 *  set of ComputeResource%s.
 */
class Workload
{
public: // STRUCTORS:

    friend class Node;

    GTS_INLINE explicit Workload(WorkloadType::Enum type)
        : m_pMyNode(nullptr)
        , m_type(type)
    {}

    /**
     * For polymorphic destruction.
     */
    virtual ~Workload() = default;

public: // ACCESSORS:

    /**
     * @returns This Workload's type.
     */
    GTS_INLINE WorkloadType::Enum type() const { return m_type; }

    /**
     * @return The Node this Workload is attached to.
     */
    GTS_INLINE Node* myNode() const { return m_pMyNode; }

public: // MUTATORS:

    /**
     * Executes the Workload in the given context.
     */
    virtual void execute(WorkloadContext const& ctx) = 0;

protected:

    //! The Node this Workload is attached to.
    Node* m_pMyNode;

    //! The Workloads type.
    WorkloadType::Enum m_type;
};

/** @} */ // end of MacroScheduler

} // namespace gts
