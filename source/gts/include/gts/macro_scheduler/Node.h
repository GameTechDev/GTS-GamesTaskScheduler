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
#include <unordered_map>

#include "gts/platform/Atomic.h"
#include "gts/platform/Thread.h"
#include "gts/containers/Vector.h"
#include "gts/macro_scheduler/MacroScheduler.h"
#include "gts/macro_scheduler/Workload.h"

namespace gts {

class Workload;

/** 
 * @addtogroup MacroScheduler
 * @{
 */

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A Node represents a task in a generalized task DAG. It contains Workload%s
 *  that are scheduled onto a ComputeResource by a MacroScheduler.
 */
class Node
{
public: // STRUCTORS:

    enum { NODE_NAME_MAX = 64 };

    Node(MacroScheduler* pMyScheduler);
    ~Node();

public: // ACCESSORS:

    /**
     * @return The Workload associated with 'type' or nullptr is it does not exist.
     * @remark Not thread-safe.
     */ 
    Workload* findWorkload(WorkloadType::Enum type) const;

    /**
     * @return The number of predecessors Nodes that must complete before this
     *  Node can execute.
     */ 
    Atomic<uint32_t> const& currPredecessorCount() const;

    /**
     * @return The number of predecessors Nodes this Node starts with.
     */ 
    uint32_t initPredecessorCount() const;

    /**
     * @return The predecessor nodes of this Node.
     */ 
    GTS_INLINE Vector<Node*> const& predecessors() const
    {
        return m_predecessors;
    }

    /**
     * @return The successor nodes of this Node.
     */ 
    GTS_INLINE Vector<Node*> const& successors() const
    {
        return m_successors;
    }

    /**
     * @return True if pChild is a child of this Node.
     */
    bool isChild(Node* pChild) const;

    /**
     * @return The MacroScheduler that created this Node.
     */ 
    GTS_INLINE MacroScheduler* myScheduler() { return m_pMyScheduler; }

    /**
     * @return The Schedule that this Node currently belongs to. nullptr if not
     *  scheduled.
     */ 
    GTS_INLINE Schedule* currentSchedule() { return m_pSchedule; }

    /**
     * @return The ID of the ComputeResource that must execute this Node.
     */
    GTS_INLINE ComputeResourceId affinity() const { return m_affinity; }

    /**
     * @return The rank of this Node.
     */
    GTS_INLINE const char* name() const { return m_name; }

    /**
     * @return The rank of this Node.
     */
    GTS_INLINE Atomic<uint64_t> const& rank() const { return m_rank;  }

    /**
     * @return The rank of this Node.
     */
    GTS_INLINE uint64_t executionCost() const { return m_executionCost; }

    /**
     * @return True if the Node must be executed in isolation.
     */
    //GTS_INLINE bool requiresIsolation() const { return m_isIsolated; }

public: // MUTATORS:

    /**
     * @brief
     *  Resets all Nodes in the graph.
     * @remark Not thread-safe.
     */ 
    static void resetGraph(Node* pSource);

    /**
     * @return The predecessor nodes of this Node.
     */ 
    GTS_INLINE Vector<Node*>& predecessors()
    {
        return m_predecessors;
    }

    /**
     * @return The successor nodes of this Node.
     */ 
    GTS_INLINE Vector<Node*>& successors()
    {
        return m_successors;
    }

    /**
     * @brief
     *  Allocates a new Workload object of type TWorkload.
     * @param args
     *  The arguments for the TWorkload constructor.
     * @return
     *  The allocated TWorkload or nullptr if the allocation failed.
     * @remark Not thread-safe.
     */
    template<typename TWorkload, typename... TArgs>
    GTS_INLINE TWorkload* addWorkload(TArgs&&... args)
    {
        TWorkload* pWorkload = new (m_pMyScheduler->_allocateWorkload(sizeof(TWorkload))) TWorkload(std::forward<TArgs>(args)...);
        if (!pWorkload)
        {
            GTS_ASSERT(0);
            return nullptr;
        }
        GTS_ASSERT(m_workloadsByType[pWorkload->type()] == nullptr);
        m_workloadsByType[pWorkload->type()] = pWorkload;
        pWorkload->m_pMyNode = this;
        return pWorkload;
    }

    /**
     * @brief
     *  Allocates a new Workload object of type TLambdaWorkload.
     * @param args
     *  The arguments for the TLambdaWorkload constructor.
     * @return
     *  The allocated TLambdaWorkload or nullptr if the allocation failed.
     * @remark Not thread-safe.
     */
    template<typename TLambdaWorkload, typename TFunc, typename... TArgs>
    GTS_INLINE TLambdaWorkload* addWorkload(TFunc&& func, TArgs&&... args)
    {
        // Size of TLambdaWorkload plus space for the lambda.
        constexpr size_t size = sizeof(TLambdaWorkload) + sizeof(std::tuple<TFunc, TArgs...>);

        TLambdaWorkload* pWorkload = new (m_pMyScheduler->_allocateWorkload(size))
            TLambdaWorkload(std::forward<TFunc>(func), std::forward<TArgs>(args)...);

        if (!pWorkload)
        {
            GTS_ASSERT(0);
            return nullptr;
        }
        GTS_ASSERT(m_workloadsByType[pWorkload->type()] == nullptr);
        m_workloadsByType[pWorkload->type()] = pWorkload;
        pWorkload->m_pMyNode = this;
        return pWorkload;
    }

    /**
     * @brief
     *  Removed a workload by its WorkloadType.
     * @remark Not thread-safe.
     */
    void removeWorkload(WorkloadType::Enum type);

    /**
     * @brief Reset the state of this Node for another execution.
     */
    void reset();

    /**
     * @brief 
     *  Add the sucessor Node 'pNode'.
     * @remark Thread-safe if:
     *  (1) The DAG this Node belongs is not executing, OR
     *  (2) pNode is being added to this Node during this Node's executing Workload.
     */
    void addSuccessor(Node* pNode);

    /**
     * @brief 
     *  Remove the sucessor Node 'pNode'.
     * @remark Thread-safe if:
     *  (1) The DAG this Node belongs is not executing, OR
     *  (2) pNode is being removed from this Node during this Node's executing Workload.
     */
    void removeSuccessor(Node* pNode);

    /**
     * @brief Set the name of this Node.
     */
    void setName(const char* format, ...);

    /**
     * @return The rank of this Node.
     */
    GTS_INLINE Atomic<uint64_t>& rank() { return m_rank;  }

    /**
     * @brief
     *  Sets the ID of the ComputeResource that must execute this Node.
     * @remark Not thread-safe.
     */
    GTS_INLINE void setAffinity(ComputeResourceId affinity) { m_affinity = affinity; }

    /**
     * @brief
     *  Specifies the ComputeResource that must execute this Workload.
     */
    //GTS_INLINE void setRequiresIsolation(bool requiresIsolation) { m_isIsolated = requiresIsolation; }

public: // INTERNAL USE

    /**
     * @brief
     *  Remove a predecessor reference and check if the Node is ready to run.
     * @return True of the Node's predecessor count is zero and ready to run.
     * @remark Internal use only.
     * @remark Thread-safe.
     */
    GTS_INLINE bool _removePredecessorRefAndReturnReady()
    {
        uint32_t prevCount = m_currPredecessorCount.fetch_sub(1, memory_order::acq_rel);
        GTS_ASSERT(prevCount != 0);
        return prevCount - 1 == 0;
    }

    /**
     * @brief
     *  Mark one predecessor as complete.
     * @remark Internal use only.
     * @remark Thread-safe.
     */
    GTS_INLINE void _markPredecessorComplete()
    {
        uint32_t prevCount = m_predecessorCompleteCount.fetch_sub(1, memory_order::acq_rel);
        GTS_UNREFERENCED_PARAM(prevCount);
        GTS_ASSERT(prevCount != 0);
    }

    /**
     * @brief
     *  Waits for all predecessors to tally their completion.
     * @remark The wait should happen rarely.
     * @remark Internal use only.
     * @remark Thread-safe.
     */ 
    GTS_INLINE void _waitUntilComplete() const
    {
        while(m_predecessorCompleteCount.load(memory_order::acquire) != 0)
        {
            GTS_PAUSE();
        }
    }

    /**
     * @brief
     *  Sets this Node's current Schedule.
     * @remark Internal use only.
     */
    GTS_INLINE void _setCurrentSchedule(Schedule* pSchedule)
    {
        m_pSchedule = pSchedule;
    }

    /**
     * @brief
     *  Sets the execution cost of this Node.
     * @remark Internal use only.
     * @remark Not thread-safe.
     */
    GTS_INLINE void _setExecutionCost(uint64_t exeCost) { m_executionCost = exeCost; }

private:

    friend class CriticiallyAware_Schedule;

    //! The Schedule this node belongs to.
    MacroScheduler* m_pMyScheduler;

    //! The Schedule this node belongs to.
    Schedule* m_pSchedule;

    //! The Workloads for the ComputeResources this Node can be scheduled on.
    Workload* m_workloadsByType[WorkloadType::COUNT];

    //! The predecessors nodes of this Node.
    Vector<Node*> m_predecessors;

    //! The successor nodes of this Node.
    Vector<Node*> m_successors;

    //! The priority rank of this Node.
    Atomic<uint64_t> m_rank;

    //! The cost of executing this Node.
    uint64_t m_executionCost;

    //! The number of predecessor nodes that must complete before this Node can be considered complete.
    Atomic<uint32_t> m_predecessorCompleteCount;

    //! The number of predecessor that must complete before this Node can execute.
    Atomic<uint32_t> m_currPredecessorCount;

    //! The number of predecessors this Node started with. Used for resetting persistent DAGs.
    uint32_t m_initPredecessorCount;

    //! The ID of the ComputeResource that must execute this Node.
    ComputeResourceId m_affinity;

    char m_name[NODE_NAME_MAX];

    //! Flag true if the Node must be executed in isolation.
    //bool m_isIsolated;
};

/** @} */ // end of MacroScheduler

} // namespace gts
