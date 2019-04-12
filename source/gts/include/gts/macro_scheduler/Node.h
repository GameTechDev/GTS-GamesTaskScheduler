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

#include <utility>

#include "gts/platform/Atomic.h"
#include "gts/platform/Thread.h"
#include "gts/containers/Vector.h"
#include "gts/macro_scheduler/ComputeResourceType.h"
#include "gts/macro_scheduler/MacroScheduler.h"
#include "gts/macro_scheduler/Workload.h"

namespace gts {

class Workload;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A Node represents a task in a generalized task DAG. It contains Workloads
 *  that are scheduled onto a ComputeResource by a MacroScheduler.
 */
class Node
{
public: // STRUCTORS:

    Node(MacroScheduler* pMyScheduler);
    ~Node();

public: // ACCESSORS

    /**
     * @return The Workload associated with 'type' or nullptr is it does not exist.
     */ 
    Workload* findWorkload(ComputeResourceType type) const;

    /**
     * @return The execution priority of this Node.
     */ 
    uint32_t priority() const;

    /**
     * @return The number of predecessors Nodes that must complete before this
     * Node can execute.
     */ 
    Atomic<uint32_t> const& currPredecessorCount() const;

    /**
     * @return The number of predecessors Nodes this Node starts with.
     */ 
    uint32_t const& initPredecessorCount() const;

    /**
     * @return The successor nodes of this Node.
     */ 
    Vector<Node*> const children() const;

public: // MUTATORS

    /**
     * Allocates a new TWorkload object of the specified size and adds it to
     * the Node. Useful if you are not ready to set the task data on Workload.
     * @param computeFunc
     *  The function the Workload will execute.
     * @param dataSize
     *  The size of the Workload data region.
     * @return
     *  The allocated Workload or nullptr if the allocation failed.
     */
    template<typename TWorkload>
    GTS_INLINE TWorkload* setEmptyWorkload(
        Workload_ComputeRoutine computeFunc,
        uint32_t dataSize = GTS_NO_SHARING_CACHE_LINE_SIZE)
    {
        m_pMyScheduler->_deleteWorkload(m_workloadsByType[(size_t)TWorkload::type()]);

        void* pBuffer = m_pMyScheduler->_allocateWorkload(
            sizeof(TWorkload) + dataSize, 
            alignof(TWorkload));

        TWorkload* pWorkload = new (pBuffer) TWorkload(this, computeFunc);
        m_workloadsByType[(size_t)TWorkload::type()] = pWorkload;
        return pWorkload;
    }

    /**
     * Allocates a new TWorkload object and emplaces the TData object.
     * TData must have a static function computeFunc.
     * @param args
     *  The constructor arguments for TData.
     * @return
     *  The allocated Task or nullptr if the allocation failed.
     */
    template<
        typename TWorkload,
        typename TData,
        void (*TaskFunc)(Workload*, WorkloadContext const&) = TData::computeFunc,
        typename... TArgs>
    GTS_INLINE TWorkload* setWorkload(TArgs&&... args)
    {
        m_pMyScheduler->_deleteWorkload(m_workloadsByType[(size_t)TWorkload::type()]);

        void* pBuffer = m_pMyScheduler->_allocateWorkload(
            sizeof(TWorkload) + sizeof(TData),
            alignof(TWorkload));

        TWorkload* pWorkload = new (pBuffer) TWorkload(this, TData::computeFunc);

        if (pWorkload != nullptr)
        {
            pWorkload->template emplaceData<TData>(std::forward<TArgs>(args)...);
        }
        m_workloadsByType[(size_t)TWorkload::type()] = pWorkload;
        return pWorkload;
    }

    /**
     * Allocates a new TWorkload object and emplaces the function/lambda data.
     * @param func
     *   A function that takes args as arguments.
     * @param args
     *  The arguments for func.
     * @return
     *  The allocated TWorkload or nullptr if the allocation failed.
     */
    template<typename TWorkload, typename TFunc, typename... TArgs>
    GTS_INLINE TWorkload* setWorkload(TFunc&& func, TArgs&&... args)
    {
        m_pMyScheduler->_deleteWorkload(m_workloadsByType[(size_t)TWorkload::type()]);

        void* pBuffer = m_pMyScheduler->_allocateWorkload(
            sizeof(TWorkload) +
                sizeof(std::decay_t<TFunc>) +
                sizeof(std::tuple<std::decay_t<TArgs>...>),
            alignof(TWorkload));

        TWorkload* pWorkload = new (pBuffer) TWorkload(
            this,
            std::forward<TFunc>(func),
            std::forward<TArgs>(args)...);

        m_workloadsByType[(size_t)TWorkload::type()] = pWorkload;
        return pWorkload;
    }

    /**
     * Set this Node's execution priority. 0 is the lowest.
     */
    void setPriority(uint32_t priority);

    /**
     * Reset the state of this Node another execution.
     */
    void reset();

    /**
     * Add the child Node 'pNode'.
     * @remark Thread-safe if:
     *  (1) The DAG this Node belongs is not executing, OR
     *  (2) pNode is being added to this Node from this Node's executing Workload.
     */
    void addChild(Node* pNode);

    /**
     * Remove the child Node 'pNode'.
     * @remark Thread-safe if:
     *  (1) The DAG this Node belongs is not executing, OR
     *  (2) pNode is being removed from this Node from this Node's executing Workload.
     */
    void removeChild(Node* pNode);

    /**
     * Remove a predecessor and check if the Node is ready to run.
     * @return True of the Node's predecessor count is zero.
     */
    bool finishPredecessor();

private:

    MacroScheduler* m_pMyScheduler;
    // The Workloads for the ComputeResources this Node can be scheduled on.
    Workload* m_workloadsByType[(uint32_t)ComputeResourceType::COUNT];
    // The successor nodes of this Node.
    Vector<Node*> m_children;
    // A spin mutex for shared access to this Node.
    mutable UnfairSpinMutex m_accessorMutex;
    // The priority of this Node.
    uint32_t m_priority;
    // The number of nodes that must complete before this Node can execute.
    Atomic<uint32_t> m_currPredecessorCount;
    // The number of predecessors this Node started with. Used for resetting
    // persistent DAGs.
    uint32_t m_initPredecessorCount;

    // TODO:
    // The requested compute resource workload to use.
    // uint32_t m_computeResourceRequestId;
};

} // namespace gts
