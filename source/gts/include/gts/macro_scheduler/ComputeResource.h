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

#include "gts/platform/Machine.h"
#include "gts/containers/parallel/QueueMPSC.h"
#include "gts/macro_scheduler/MacroSchedulerTypes.h"

namespace gts {

class Node;
class Schedule;
class Workload;

/** 
 * @addtogroup MacroScheduler
 * @{
 */

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A ComputeResource represents a entity that can execute a Workload in a Node.
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
     * @returns This ComputeResource's type.
     */
    virtual ComputeResourceType::Enum type() const = 0;

    /**
     * @returns True if this ComputeResource can execute the specified Node.
     */
    virtual bool canExecute(Node* pNode) const = 0;

    /**
     * @returns The normalization factor applied to execution times produced by
     *  this compute resource.
     */
    GTS_INLINE double executionNormalizationFactor() const { return m_exeWeight; }

    /**
     * @returns This ComputeResource's unique id.
     */
    GTS_INLINE ComputeResourceId id() const { return m_id; }

public: // MUTATORS:

    /**
     * @brief
     *  Sets the normalization factor applied to each Nodes execution time. This
     *  factor normalizes all execution times into a single ComputeResrouce's
     *  execution space.
     * @todo Have the MicroScheduler set this during initialization instead of the user.
     */
    void setExecutionNormalizationFactor(double exeWeight) { GTS_ASSERT(exeWeight != 0); m_exeWeight = exeWeight; }

    /**
     * Process pSchedule until it is complete. If 'canBlock' is true,
     * this ComputeResouces can use the calling thread; if false, it
     * must fork its own thread.
     * @returns True if a node was processed.
     */
    virtual bool process(Schedule* pSchedule, bool canBlock) = 0;

    /**
     * Notify this ComputeResrouce of work available.
     */
    virtual void notify(Schedule* pSchedule) = 0;

    /**
     * Register a Schedule with this ComputeResrouce.
     */
    virtual void registerSchedule(Schedule*) {};

    /**
     * Unregister a Schedule from this ComputeResrouce.
     */
    virtual void unregisterSchedule(Schedule*) {};

protected:

    //! Nodes that require this ComputeResource. They are send from other
    //! ComputeResouce while processing a Schedule. These Node must be processed
    //! first when looking for work.
    QueueMPSC<Node*> m_affinityQueue;

private:

    //! The next ID to assign to a ComputeResource object.
    static ComputeResourceId s_nextId;

    //! The normalization weight for execution times produces by this ComputeResource.
    double m_exeWeight;

    //! This ComputeResource's Type.
    ComputeResourceId m_id;
};

/** @} */ // end of MacroScheduler

} // namespace gts
