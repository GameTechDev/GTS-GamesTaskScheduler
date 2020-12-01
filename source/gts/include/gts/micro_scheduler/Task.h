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
#include <tuple>

#include "gts/analysis/Trace.h"
#include "gts/platform/Assert.h"
#include "gts/platform/Utils.h"
#include "gts/platform/Atomic.h"
#include "gts/micro_scheduler/MicroSchedulerTypes.h"

namespace gts {

/** 
 * @addtogroup MicroScheduler
 * @{
 */

struct TaskContext;
class Task;
class MicroScheduler;
class LocalScheduler;
class Worker;

constexpr uint32_t ANY_WORKER = UNKNOWN_UID;

#ifdef GTS_MSVC
#pragma warning(push)
#pragma warning(disable : 4324) // alignment padding warning
#endif

namespace internal {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  The header for each Task that contains common bookkeeping data.
 */
struct GTS_ALIGN(GTS_CACHE_LINE_SIZE) TaskHeader
{
    // Gets the Task suffix.
    GTS_INLINE Task* _task()
    {
        return (Task*)(this + 1);
    }

    enum ExecutionStates
    {   
        // Allocated or recycled.
        ALLOCATED,
        // In the ready pool.
        READY,
        // A scheduled task. It will be destroyed on completion.
        EXECUTING,
        // A task on the free list.
        FREED
    };

    enum Flags
    {
        TASK_HAS_DATA_SUFFIX = 1 << 0,
        TASK_IS_CONTINUATION = 1 << 1,
        TASK_IS_STOLEN       = 1 << 2,
        TASK_IS_WAITER       = 1 << 3,
        TASK_IS_SMALL        = 1 << 4,
    };

    Task*            pParent           = nullptr;
    Atomic<Task*>    pListNext         = { nullptr };
    LocalScheduler*  pMyLocalScheduler = nullptr;
    Worker*          pMyWorker         = nullptr;
#ifdef GTS_USE_TASK_NAME
    const char*      pName             = nullptr;
#endif
    Atomic<int32_t>  refCount          = { 1 };
    uint32_t         affinity          = ANY_WORKER;
    uint32_t         executionState    = ALLOCATED;
    uint32_t         flags             = 0;
};

} // namespace internal

#ifdef GTS_MSVC
#pragma warning(pop)
#endif

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A Task payload that embeds TFunc and TArgs into the Task's data. It makes
 *  it easy to construct a Task from a lambda or a function plus arguments
 *  similar to std::thread.
 * @details
 *  Derivative of TBB Task. https://github.com/intel/tbb
 */
class Task
{
    friend class MicroScheduler;
    friend class LocalScheduler;
    friend class Worker;

public: // INTERFACE:

    virtual ~Task() {}
    virtual Task* execute(TaskContext const& ctx) = 0;

public: // MUTATORS:

    /**
     * Adds pChild as a child of this task. It DOES NOT increment this task's
     * ref count so the caller must have manually increment the reference count
     * before calling this function. Failing to do so is undefined behavior.
     * pChild cannot already have a parent.
     */
    GTS_INLINE void addChildTaskWithoutRef(Task* pChild);

    /**
     * Adds pChild as a child of this task AND increments this task's ref count.
     * pChild cannot already have a parent.
     */
    GTS_INLINE void addChildTaskWithRef(Task* pChild, gts::memory_order order = gts::memory_order::seq_cst);

    /**
     * Sets pContinuation as a continuation of this task. pContinuation cannot
     * already have a parent.
     */
    GTS_INLINE void setContinuationTask(Task* pContinuation);

    /**
     * Marks the task to be reused after it has finished executing. Makes the
     * Task look like it was just allocated. If the task is not added into
     * a task graph with addChild* or setContinuation or returned as a bypass
     * Task, it will be spawned.
     */
    GTS_INLINE void recycle();

    /**
     * Waits for all this Task's children to complete before continuing. While
     * waiting, this task will execute other available work.
     * @remark
     *  Requires and extra reference be added for the wait. The wait is 
     *  considered complete when this task's reference count == 2. The reference
     *  count will be set to 1 once the wait completes.
     */
    void waitForAll();

    /**
     * Executes pChild immediately and then waits for all this Task's children 
     * to complete before continuing. While waiting, this task will execute
     * other available work.
     * @remark
     *  Requires and extra reference be added for the wait. The wait is 
     *  considered complete when this task's reference count == 2. The reference
     *  count will be set to 1 once the wait completes.
     */
    void spawnAndWaitForAll(Task* pChild);

    /**
     * Force the task to run on a specific Worker thread.
     */
    GTS_INLINE void setAffinity(uint32_t workerIdx);

    /**
     * Adds a reference to the task.
     */
    GTS_INLINE int32_t addRef(int32_t count = 1, gts::memory_order order = gts::memory_order::seq_cst);

    /**
     * Removes a reference from the task.
     */
    GTS_INLINE int32_t removeRef(int32_t count = 1, gts::memory_order order = gts::memory_order::seq_cst);

    /**
     * Sets a reference count.
     */
    GTS_INLINE void setRef(int32_t count, gts::memory_order order = gts::memory_order::seq_cst);

    /**
     * Sets the name of the task.
     */
    GTS_INLINE void setName(const char* name);

public: // ACCCESSORS:

    /**
     * @return The current reference count.
     */
    GTS_INLINE int32_t refCount(gts::memory_order order = gts::memory_order::acquire) const;

    /**
     * @return The current Worker affinity.
     */
    GTS_INLINE uint32_t getAffinity() const;

    /**
     * @return True if stolen.
     */
    GTS_INLINE bool isStolen() const;

    /**
     * @return This task's parent. nullptr is no parent is set.
     */
    GTS_INLINE Task* parent();

    /**
     * @return The name of the task.
     */
    GTS_INLINE const char* name() const;

private:

    // Gets this Task's TaskHeader object.
    GTS_INLINE internal::TaskHeader& header() const
    {
        return *(reinterpret_cast<internal::TaskHeader*>(const_cast<Task*>(this)) + -1);
    }
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A Task payload that embeds a function pointer and data argument into the
 *  the Task's data.
 */
class CStyleTask : public Task
{
public: // STRUCTORS:

    using TaskRoutine = Task* (*)(void* pData, TaskContext const& taskContext);

    /**
     * Construct an empty Task. Needed for some container classes.
     */
    GTS_INLINE CStyleTask();

    /**
     * Construct an Task from a TaskRoutine and data pointer
     */
    template<typename TData>
    GTS_INLINE explicit CStyleTask(TaskRoutine taskRountine, TData* pData = nullptr);

    /**
     * Copy constructor needed for STL containers. Do not use!
     */
    GTS_INLINE CStyleTask(CStyleTask const&);

    GTS_INLINE virtual ~CStyleTask();

public: // MUTATORS:

    GTS_INLINE virtual Task* execute(TaskContext const& ctx) final;

    /**
     * Copies a data pointer into the task.
     */
    template<typename TData>
    GTS_INLINE TData* setData(TData* data);

public: // ACCCESSORS:

    /**
     * @return the const user data from the task.
     */
    GTS_INLINE void const* getData() const;

    /**
     * @return the user data from the task.
     */
    GTS_INLINE void* getData();

private: // HELPERS:

    template<typename TData>
    GTS_INLINE void _resetDataDestructor();

private:

    using DataDestructor = void(*)(void* pData);

    static void emptyDataDestructor(void*)
    {}

    template<typename TData>
    static void dataDestructor(void* pData)
    {
        GTS_UNREFERENCED_PARAM(pData);
        reinterpret_cast<TData*>(pData)->~TData();
    }

    TaskRoutine m_fcnTaskRoutine;
    DataDestructor m_fcnDataDestructor;
    void* m_pData;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
* @brief
*  A Task payload that embeds TFunc and TArgs into the Task's data. It makes
*  it easy to construct a Task from a lambda or a function plus arguments
*  similar to std::thread.
*/
template<typename TFunc, typename...TArgs>
class LambdaTaskWrapper : public Task
{
    using FucnArgsTupleType = typename std::tuple<std::decay_t<TFunc>, std::decay_t<TArgs>... >;

    template<size_t... Idxs>
    GTS_INLINE Task* _invoke(std::integer_sequence<size_t, Idxs...>, TaskContext const& ctx);

public: // STRUCTORS:

    GTS_INLINE LambdaTaskWrapper(TFunc&& func, TArgs&&...args);

public: // MUTATORS:

    GTS_INLINE virtual Task* execute(TaskContext const& ctx) final;

private:

    FucnArgsTupleType m_tuple;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A empty Task that can be used as dummy or placeholder.
 */
class EmptyTask : public Task
{
public: // MUTATORS:

    GTS_INLINE virtual Task* execute(TaskContext const&) final
    {
        return nullptr;
    }
};

/** @} */ // end of MicroScheduler

#include "gts/micro_scheduler/Task.inl"

} // namespace gts
