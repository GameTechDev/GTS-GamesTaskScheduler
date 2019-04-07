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

#include <type_traits>

#include "gts/analysis/Instrumenter.h"
#include "gts/platform/Assert.h"
#include "gts/platform/Utils.h"
#include "gts/platform/Atomic.h"

namespace gts {

struct TaskContext;
class Task;
class MicroScheduler;
class Schedule;

/**
 * The task function signature.
 * @param pThisTask
 *  The Task currently being executed.
 * @param taskContext
 *  The context in which the task is being executed.
 * @return
 *  A optional Task to be executed immediately upon returning.
 */
using TaskRoutine = gts::Task* (*)(Task* pThisTask, TaskContext const& taskContext);

#ifdef GTS_MSVC
#pragma warning( push )
#pragma warning( disable : 4324) // alignment padding warning
#endif

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  The task type to be executed by MicroScheduler.
 */
class GTS_ALIGN(GTS_NO_SHARING_CACHE_LINE_SIZE) Task
{
public: // CREATORS:

    /**
     * Construct an empty Task. Needed for some container classes.
     */
    GTS_INLINE Task()
        : m_fcnTaskRoutine(nullptr)
        , m_pParent(nullptr)
        , m_pMyScheduler(nullptr)
        , m_isolationTag(0)
        , m_affinity(UINT32_MAX)
        , m_refCount(1)
        , m_state(0)
    {}

private: // CREATORS:

    /**
     * Construct an Task from a TaskRoutine
     */
    GTS_INLINE explicit Task(TaskRoutine taskRountine)
        : m_fcnTaskRoutine(taskRountine)
        , m_pParent(nullptr)
        , m_pMyScheduler(nullptr)
        , m_isolationTag(0)
        , m_affinity(UINT32_MAX)
        , m_refCount(1)
        , m_state(0)
    {}

    /**
     * Copy constructor needed for STL containers. Do not use!
     */
    GTS_INLINE Task(Task const&)
    {
        // must impl for STL :(
        GTS_ASSERT(0 && "Cannot copy a task");
    }

    /**
     * Execute the task routine.
     */
    GTS_INLINE Task* execute(Task* thisTask, TaskContext const& taskContext)
    {
        return m_fcnTaskRoutine(thisTask, taskContext);
    }

public: // MUTATORS:

    /**
     * Adds pChild as a child of this task. It DOES NOT increment this task's
     * ref count so the caller must have manually increment the reference count
     * before calling this function. Failing to do so is undefined behavior.
     * pChild cannot already have a parent.
     */
    GTS_INLINE void addChildTaskWithoutRef(Task* pChild)
    {
        GTS_ASSERT(pChild->m_pParent == nullptr);
        GTS_ASSERT(m_refCount.load(gts::memory_order::acquire) > 1 && "Ref count is 1, did you forget to addRef?");

        GTS_INSTRUMENTER_MARKER(analysis::Tag::INTERNAL, "ADD CHILD", this, pChild);

        pChild->m_pParent = this;
    }

    /**
     * Adds pChild as a child of this task AND increment this task's ref count.
     * pChild cannot already have a parent.
     */
    GTS_INLINE void addChildTaskWithRef(Task* pChild, gts::memory_order order = gts::memory_order::seq_cst)
    {
        addRef(1, order);
        addChildTaskWithoutRef(pChild);
    }

    /**
     * Sets pContinuation as a continuation of this task. pContinuation cannot
     * already have a parent.
     */
    GTS_INLINE void setContinuationTask(Task* pContinuation)
    {
        GTS_ASSERT(pContinuation->m_pParent == nullptr);

        GTS_ASSERT(
            (m_state & TASK_IS_EXECUTING) &&
            "This task is not executing. Setting a continuation will orphan this task from the DAG.");

        GTS_INSTRUMENTER_MARKER(analysis::Tag::INTERNAL, "SET CONT", this, pContinuation);

        pContinuation->m_state |= TASK_IS_CONTINUATION;

        // Unlink this task from the DAG
        Task* parent = m_pParent;
        m_pParent    = nullptr;

        // Link the continuation to this task's parent to reconnect the DAG.
        pContinuation->m_pParent = parent;
    }

    /**
     * Marks this task to be recycled as a child of pParent.
     */
    GTS_INLINE void recyleAsChildOf(Task* pParent)
    {
        m_state |= RECYLE;
        m_pParent = pParent;
    }

    /**
     * Waits for all this Task's children to complete before continuing. While
     * waiting, this task will execute other available work.
     */
    void waitForChildren(TaskContext const& ctx);

    /**
     * Manually destroy this task that are never executed. Useful for dummy tasks.
     */
    void destroy(TaskContext const& ctx);

    /**
     * Emplaces the data into the task.
     */
    template<typename T, typename... TArgs>
    GTS_INLINE T* emplaceData(TArgs&&... args)
    {
#if defined(GTS_USE_ASSERTS) || !defined(NDEBUG)
        GTS_ASSERT((m_state & TASK_NEED_DATA_DESTRUCTOR) == 0);
        m_state |= !std::is_trivially_destructible<T>::value ? TASK_NEED_DATA_DESTRUCTOR : 0;
#endif

        m_state |= TASK_HAS_DATA_SUFFIX;
        return new (_dataSuffix()) T(std::forward<TArgs>(args)...);
    }

    /**
     * Copies the data into the task.
     */
    template<typename T>
    GTS_INLINE T* setData(T const& data)
    {
#if defined(GTS_USE_ASSERTS) || !defined(NDEBUG)
        GTS_ASSERT((m_state & TASK_NEED_DATA_DESTRUCTOR) == 0);
        m_state |= !std::is_trivially_destructible<T>::value ? TASK_NEED_DATA_DESTRUCTOR : 0;
#endif

        m_state |= TASK_HAS_DATA_SUFFIX;
        return new (_dataSuffix()) T(data);
    }

    /**
     * Copies a data pointer into the task.
     */
    template<typename T>
    GTS_INLINE T* setData(T* data)
    {
        GTS_ASSERT((m_state & TASK_NEED_DATA_DESTRUCTOR) == 0);
        m_state &= ~TASK_HAS_DATA_SUFFIX;

        ::memcpy(_dataSuffix(), &data, sizeof(T*)); // stores address number!
        return (T*)((uintptr_t*)_dataSuffix())[0];
    }

    /**
     * Call destructor for the data in the Task.
     */
    template<typename T>
    GTS_INLINE void destroyData()
    {
#if defined(GTS_USE_ASSERTS) || !defined(NDEBUG)
        m_state &= ~TASK_NEED_DATA_DESTRUCTOR;
#endif
        ((T*)_dataSuffix())->~T();
    }

    /**
     * Force the task to run on a specific Worker thread.
     */
    GTS_INLINE void setAffinity(uint32_t workerIdx)
    {
        m_affinity = workerIdx;
    }

    /**
     * Adds a reference to the task.
     */
    GTS_INLINE int32_t addRef(int32_t count = 1, gts::memory_order order = gts::memory_order::seq_cst)
    {
        GTS_ASSERT(m_refCount.load(gts::memory_order::relaxed) > 0);

        int32_t refCount;

        switch (order)
        {
        case gts::memory_order::relaxed:
            refCount = m_refCount.load(gts::memory_order::relaxed) + count;
            m_refCount.store(refCount, gts::memory_order::relaxed);
            return refCount;

        default:
            return m_refCount.fetch_add(count, order) + count;
        }
    }

    /**
     * Removes a reference from the task.
     */
    GTS_INLINE int32_t removeRef(int32_t count = 1, gts::memory_order order = gts::memory_order::seq_cst)
    {
        GTS_ASSERT(m_refCount.load(gts::memory_order::relaxed) - count >= 0);
        return addRef(-count, order);
    }

    /**
     * Sets a reference count.
     */
    GTS_INLINE void setRef(int32_t count, gts::memory_order order = gts::memory_order::seq_cst)
    {
        GTS_ASSERT(m_refCount.load(gts::memory_order::relaxed) > 0);
        m_refCount.store(count, order);
    }

public: // ACCCESSORS:

    /**
     * @return the current reference count.
     */
    GTS_INLINE int32_t refCount(gts::memory_order order = gts::memory_order::acquire) const
    {
        return m_refCount.load(order);
    }

    /**
     * @return the current Worker affinity.
     */
    GTS_INLINE uint32_t getAffinity() const
    {
        return m_affinity;
    }

    /**
     * @return the const user data from the task.
     */
    GTS_INLINE void const* getData() const
    {
        if (m_state & TASK_HAS_DATA_SUFFIX)
        {
            return _dataSuffix();
        }
        else
        {
            return (void*)((uintptr_t*)_dataSuffix())[0];
        }
    }

    /**
     * @return the user data from the task.
     */
    GTS_INLINE void* getData()
    {
        if (m_state & TASK_HAS_DATA_SUFFIX)
        {
            return _dataSuffix();
        }
        else
        {
            return (void*)((uintptr_t*)_dataSuffix())[0];
        }
    }

    /**
     * @return True if stolen.
     */
    GTS_INLINE bool isStolen() const
    {
        return (m_state & TASK_IS_STOLEN) != 0;
    }

    /**
     * @return This task's parent. nullptr is no parent is set.
     */
    GTS_INLINE Task* parent()
    {
        return m_pParent;
    }

    /**
     * @return This task's isolation tag.
     */
    GTS_INLINE uintptr_t isolationTag() const
    {
        return m_isolationTag;
    }

private: // HELPERS:

    GTS_INLINE void* _dataSuffix()
    {
        return this + 1;
    }

    GTS_INLINE void const* _dataSuffix() const
    {
        return this + 1;
    }

private: // DATA:

    friend class MicroScheduler;
    friend class Schedule;

    enum States
    {
        TASK_HAS_DATA_SUFFIX = 0x01,
        TASK_IS_EXECUTING    = 0x02,
        TASK_IS_WAITING      = 0x04,
        TASK_IS_CONTINUATION = 0x08,
        TASK_IS_STOLEN       = 0x10,
        TASK_IS_QUEUED       = 0x20,
        RECYLE               = 0x40,
#if defined(GTS_USE_ASSERTS) || !defined(NDEBUG)
        TASK_NEED_DATA_DESTRUCTOR = 0x80,
#endif
    };

    TaskRoutine m_fcnTaskRoutine;
    Task* m_pParent;
    MicroScheduler* m_pMyScheduler;
    uintptr_t m_isolationTag;
    uint32_t m_affinity;
    gts::Atomic<int32_t> m_refCount;
    uint32_t m_state;

    // vvvvvvvvv TASK DATA IS STORED AS A SUFFIX vvvvvvvvvvvvvvvvvv
};

#ifdef GTS_MSVC
#pragma warning( pop )
#endif

} // namespace gts
