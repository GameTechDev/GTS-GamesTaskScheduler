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

#include "gts/platform/Machine.h"
#include "gts/macro_scheduler/MacroSchedulerTypes.h"

namespace gts {

class Node;
class MacroScheduler;

/** 
 * @addtogroup MacroScheduler
 * @{
 */

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  The execution schedule for all ComputeResource%s.
 */
class Schedule
{
public: // STRUCTORS:

    GTS_INLINE Schedule(MacroScheduler* pMyScheduler)
        : m_pMyScheduler(pMyScheduler)
        , m_refCount{0} {}

    /**
     * For polymorphic destruction.
     */
    virtual ~Schedule() = default;

public: // ACCESSORS:

    /**
     * @returns The MacroScheduler that created this Schedule.
     */
    GTS_INLINE MacroScheduler* getScheduler() { return m_pMyScheduler; }

    /**
     * @returns The current reference count.
     */
    GTS_INLINE int32_t refCount() const
    {
        return m_refCount.load(memory_order::acquire);
    }

    /**
     * @returns The next Node for the ComputeResource to execute.
     */
    virtual Node* popNextNode(ComputeResourceId id, ComputeResourceType::Enum type) = 0;

    /**
     * @returns True if the schedule is completed.
     */
    virtual bool isDone() const = 0;

public: // MUTATORS:

    /**
     * Adds refCount to the current reference count.
     * @returns The new reference count.
     */
    GTS_INLINE int32_t addRef(uint32_t refCount)
    {
        return m_refCount.fetch_add(refCount, memory_order::acq_rel) + refCount;
    }

    /**
     * Removes refCount from the current reference count.
     * @returns The new reference count.
     */
    GTS_INLINE uint32_t removeRef(uint32_t refCount)
    {
        return m_refCount.fetch_sub(refCount, memory_order::acq_rel) + refCount;
    }

    /**
     * If 'pNode' is the last Node, mark the schedule as done.
     */
    virtual void tryMarkDone(Node* pNode) = 0;

    /**
     * Insert 'pNode' into the schedule.
     */
    virtual void insertReadyNode(Node* pNode) = 0;

    /**
     * Share the current Node's execution cost on the specified compute resource.
     */
    virtual void observeExecutionCost(ComputeResourceId id, uint64_t cost) = 0;

private:

    MacroScheduler* m_pMyScheduler;
    Atomic<int32_t> m_refCount;
};


class ProtectedSchedule
{
    class Accessor
    {
    public:

        GTS_INLINE Accessor(Schedule* pSchedule, UnfairSharedSpinMutex<>& rwLock)
            : m_pSchedule(pSchedule)
            , m_rwLock(rwLock)
        { rwLock.lock_shared(); }

        GTS_INLINE ~Accessor() { m_rwLock.unlock_shared(); }

        GTS_INLINE Schedule* get() { return m_pSchedule; }

    private:

        Schedule* m_pSchedule;
        UnfairSharedSpinMutex<>& m_rwLock;
    };

public:

    GTS_INLINE Accessor getAccessor() { return Accessor(m_pSchedule, m_rwLock); }

    GTS_INLINE Accessor getDeletor() { return Accessor(m_pSchedule, m_rwLock); }

private:

    Schedule* m_pSchedule;
    UnfairSharedSpinMutex<> m_rwLock;
};

/** @} */ // end of MacroScheduler

} // namespace gts
