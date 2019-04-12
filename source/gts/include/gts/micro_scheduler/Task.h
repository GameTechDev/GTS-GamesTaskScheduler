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

#include "gts/analysis/Instrumenter.h"
#include "gts/platform/Assert.h"
#include "gts/platform/Utils.h"
#include "gts/platform/Atomic.h"

namespace gts
{

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
#pragma warning(push)
#pragma warning(disable : 4324) // alignment padding warning
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
    GTS_INLINE Task();

    /**
     * Construct an Task from a TaskRoutine
     */
    GTS_INLINE explicit Task(TaskRoutine taskRountine);

    /**
     * Copy constructor needed for STL containers. Do not use!
     */
    GTS_INLINE Task(Task const&);

    /**
     * Execute the task routine.
     */
    GTS_INLINE Task* execute(Task* thisTask, TaskContext const& taskContext);

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
     * Marks this task to be recycled as a child of pParent.
     */
    GTS_INLINE void recyleAsChildOf(Task* pParent);

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
    GTS_INLINE T* emplaceData(TArgs&&... args);

    /**
     * Copies the data into the task.
     */
    template<typename T>
    GTS_INLINE T* setData(T const& data);

    /**
     * Copies a data pointer into the task.
     */
    template<typename T>
    GTS_INLINE T* setData(T* data);

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

public: // ACCCESSORS:

    /**
     * @return the current reference count.
     */
    GTS_INLINE int32_t refCount(gts::memory_order order = gts::memory_order::acquire) const;

    /**
     * @return the current Worker affinity.
     */
    GTS_INLINE uint32_t getAffinity() const;

    /**
     * @return the const user data from the task.
     */
    GTS_INLINE void const* getData() const;

    /**
     * @return the user data from the task.
     */
    GTS_INLINE void* getData();

    /**
     * @return True if stolen.
     */
    GTS_INLINE bool isStolen() const;

    /**
     * @return This task's parent. nullptr is no parent is set.
     */
    GTS_INLINE Task* parent();

    /**
     * @return This task's isolation tag.
     */
    GTS_INLINE uintptr_t isolationTag() const;

private: // HELPERS:

    GTS_INLINE void*       _dataSuffix();
    GTS_INLINE void const* _dataSuffix() const;

private: // DATA:

    friend class MicroScheduler;
    friend class Schedule;

    using DataDestructor = void(*)(void* pData);

    static void emptyDataDestructor(void*)
    {}

    template<typename T>
    static void dataDestructor(void* pData)
    {
        GTS_UNREFERENCED_PARAM(pData);
        reinterpret_cast<T*>(pData)->~T();
    }

    enum States
    {
        TASK_HAS_DATA_SUFFIX = 0x01,
        TASK_IS_EXECUTING    = 0x02,
        TASK_IS_WAITING      = 0x04,
        TASK_IS_CONTINUATION = 0x08,
        TASK_IS_STOLEN       = 0x10,
        TASK_IS_QUEUED       = 0x20,
        RECYLE               = 0x40
    };

    TaskRoutine          m_fcnTaskRoutine;
    DataDestructor       m_fcnDataDestructor;
    Task*                m_pParent;
    MicroScheduler*      m_pMyScheduler;
    uintptr_t            m_isolationTag;
    uint32_t             m_affinity;
    gts::Atomic<int32_t> m_refCount;
    uint32_t             m_state;

    // vvvvvvvvv TASK DATA IS STORED AS A SUFFIX vvvvvvvvvvvvvvvvvv
};

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
*/
template<typename TFunc, typename...TArgs>
class LambdaTaskWrapper
{
    using FucnArgsTupleType = typename std::tuple<std::decay_t<TFunc>, std::decay_t<TArgs>... >;

    template<size_t... Idxs>
    GTS_INLINE Task* _invoke(std::integer_sequence<size_t, Idxs...>, Task* pThisTask, TaskContext const& ctx);

public:

    GTS_INLINE LambdaTaskWrapper(TFunc&& func, TArgs&&...args);

    GTS_INLINE static gts::Task* taskFunc(gts::Task* pThisTask, gts::TaskContext const& ctx);

private:

    FucnArgsTupleType m_tuple;
};

#include "gts/micro_scheduler/Task.inl"

} // namespace gts
