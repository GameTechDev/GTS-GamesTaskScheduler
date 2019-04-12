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

#include "gts/containers/Vector.h"

#include "gts/micro_scheduler/MicroScheduler.h"
#include "gts/micro_scheduler/Task.h"

#include "gts/macro_scheduler/MacroSchedulerTypes.h"
#include "gts/macro_scheduler/Node.h"
#include "gts/macro_scheduler/ComputeResourceType.h"
#include "gts/macro_scheduler/Workload.h"

namespace gts {

class Task;
class MicroScheduler;
struct TaskContext;
class MicroScheduler_Workload;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A Task payload for the MicroScheduler.
 */
class MicroScheduler_Task
{
public:

    MicroScheduler_Task(
        Workload_ComputeRoutine cpuComputeRoutine,
        MicroScheduler_Workload* pMyWorkload,
        uint64_t* pExecutionCost);

    static Task* taskFunc(Task* pThisTask, TaskContext const& ctx);

private:

    void _executeTask();

    Workload_ComputeRoutine m_cpuComputeRoutine;
    MicroScheduler_Workload* m_pMyWorkload;
    uint64_t* m_pExecutionCost;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A Task payload for the MicroScheduler.
 */
template<typename TFunc, typename...TArgs>
class MicroScheduler_LambdaTask
{
    using FuncAndArgsType = typename std::tuple<std::decay_t<TFunc>, std::decay_t<TArgs>...>;

public:

    //--------------------------------------------------------------------------
    GTS_INLINE MicroScheduler_LambdaTask(
        MicroScheduler_Workload* pMyWorkload,
        uint64_t* exeCost,
        TFunc&& computeRoutine,
        TArgs&&...args)
        : m_tuple(std::forward<TFunc>(computeRoutine), std::forward<TArgs>(args)...)
        , m_pMyWorkload(pMyWorkload)
        , m_pExecutionCost(exeCost)
    {}

    //--------------------------------------------------------------------------
    GTS_INLINE static gts::Task* taskFunc(gts::Task* pThisTask, gts::TaskContext const& ctx)
    {
        GTS_ASSERT(pThisTask != nullptr);

        // Unpack the data.
        MicroScheduler_LambdaTask<TFunc, TArgs...>* pNodeTask =
            (MicroScheduler_LambdaTask<TFunc, TArgs...>*)pThisTask->getData();

        pNodeTask->_executeTask();
        pNodeTask->m_pMyWorkload->_spawnReadyChildren(pNodeTask->m_pMyWorkload->node(), pThisTask, ctx);
        return nullptr;
    }

private:

    //--------------------------------------------------------------------------
    template<size_t... Idxs>
    void _invoke(std::integer_sequence<size_t, Idxs...>, Workload* pThisWorkload, WorkloadContext const& ctx)
    {
        std::invoke(std::move(std::get<Idxs>(m_tuple))..., pThisWorkload, ctx);
    }

    //--------------------------------------------------------------------------
    GTS_INLINE void _executeTask()
    {
        // Execute this node's work.
        uint64_t exeCost = GTS_RDTSC();
        _invoke(
            std::make_integer_sequence<size_t, std::tuple_size<FuncAndArgsType>::value>(),
            m_pMyWorkload,
            WorkloadContext{});
        *m_pExecutionCost = GTS_RDTSC() - exeCost;
    }

private:

    FuncAndArgsType m_tuple;
    MicroScheduler_Workload* m_pMyWorkload;
    uint64_t* m_pExecutionCost;
};



#ifdef GTS_MSVC
#pragma warning(push)
#pragma warning(disable : 4324) // alignment padding warning
#endif

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A concrete Workload that maps to the MicroScheduler.
 */
class GTS_ALIGN(GTS_NO_SHARING_CACHE_LINE_SIZE) MicroScheduler_Workload : public Workload
{
    // Type erased creator.
    using BuildTaskFunc = Task*(*)(MicroScheduler* pMicroScheduler, MicroScheduler_Workload* pWorkload);

    //--------------------------------------------------------------------------
    template<typename TFunc, typename...TArgs>
    GTS_INLINE static Task* taskBuilder(MicroScheduler* pMicroScheduler, MicroScheduler_Workload* pWorkload)
    {
        GTS_ASSERT(pMicroScheduler != nullptr);
        Task* pTask = pMicroScheduler->allocateTaskRaw(
            MicroScheduler_LambdaTask<TFunc, TArgs...>::taskFunc,
            sizeof(MicroScheduler_LambdaTask<TFunc, TArgs...>));
        pTask->setData(*(MicroScheduler_LambdaTask<TFunc, TArgs...>*)pWorkload->_dataSuffix());
        return pTask;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE static Task* taskBuilder(MicroScheduler* pMicroScheduler, MicroScheduler_Workload* pWorkload)
    {
        GTS_ASSERT(pMicroScheduler != nullptr);
        Task* pTask = pMicroScheduler->allocateTask<MicroScheduler_Task>(
            pWorkload->m_fcnComputeRoutine,
            pWorkload,
            &pWorkload->m_executionCost);
        return pTask;
    }

public:

    //--------------------------------------------------------------------------
    template<typename TFunc, typename...TArgs>
    GTS_INLINE MicroScheduler_Workload(Node* pOwnerNode, TFunc&& computeRoutine, TArgs&&...args)
        : Workload(ComputeResourceType::CPU_MicroScheduler)
        , m_executionCost(0)
        , m_fcnComputeRoutine(nullptr)
        , m_fcnTaskBuilder(taskBuilder<TFunc, TArgs...>)
        , m_fcnDataDestructor(dataDestructor<MicroScheduler_LambdaTask<TFunc, TArgs...>>)
        , m_pMyNode(pOwnerNode)
        , m_state(0)
    {
        new (_dataSuffix()) MicroScheduler_LambdaTask<TFunc, TArgs...>(
            this,
            &m_executionCost,
            std::forward<TFunc>(computeRoutine),
            std::forward<TArgs>(args)...);
    }

    //--------------------------------------------------------------------------
    GTS_INLINE MicroScheduler_Workload(Node* pOwnerNode, Workload_ComputeRoutine computeRoutine)
        : Workload(ComputeResourceType::CPU_MicroScheduler)
        , m_executionCost(0)
        , m_fcnComputeRoutine(computeRoutine)
        , m_fcnTaskBuilder(taskBuilder)
        , m_fcnDataDestructor(emptyDataDestructor)
        , m_pMyNode(pOwnerNode)
        , m_state(0)
    {}

    //--------------------------------------------------------------------------
    template<typename T, typename... TArgs>
    GTS_INLINE T* emplaceData(TArgs&&... args)
    {
        m_state |= TASK_HAS_DATA_SUFFIX;
        m_fcnDataDestructor = dataDestructor<T>;
        return new (_dataSuffix()) T(std::forward<TArgs>(args)...);
    }

    //--------------------------------------------------------------------------
    template<typename T>
    GTS_INLINE T* setData(T const& data)
    {
        m_fcnDataDestructor = dataDestructor<T>;
        m_state |= TASK_HAS_DATA_SUFFIX;
        return new (_dataSuffix()) T(data);
    }

    //--------------------------------------------------------------------------
    template<typename T>
    GTS_INLINE T* setData(T* data)
    {
        m_state &= ~TASK_HAS_DATA_SUFFIX;

        m_fcnDataDestructor = emptyDataDestructor;
        ::memcpy(_dataSuffix(), &data, sizeof(T*)); // stores address number!
        return (T*)((uintptr_t*)_dataSuffix())[0];
    }

    //--------------------------------------------------------------------------
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

    //--------------------------------------------------------------------------
    template<typename TFunc, typename...TArgs>
    GTS_INLINE static size_t size()
    {
        return sizeof(MicroScheduler_Workload) + sizeof(MicroScheduler_LambdaTask<TFunc, TArgs...>);
    }

    //--------------------------------------------------------------------------
    GTS_INLINE static ComputeResourceType type()
    {
        return ComputeResourceType::CPU_MicroScheduler;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE Node* node()
    {
        return m_pMyNode;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE Task* _buildTask(MicroScheduler* pMicroScheduler)
    {
        GTS_ASSERT(pMicroScheduler != nullptr);
        Task* pTask = m_fcnTaskBuilder(pMicroScheduler, this);
        return pTask;
    }

    void _spawnReadyChildren(Node* pThisNode, Task* pThisTask, TaskContext const& ctx);

private: // HELPERS:

    //--------------------------------------------------------------------------
    GTS_INLINE void* _dataSuffix()
    {
        return this + 1;
    }

    //--------------------------------------------------------------------------
    GTS_INLINE void const* _dataSuffix() const
    {
        return this + 1;
    }

private:

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
    };

    uint64_t m_executionCost;
    Workload_ComputeRoutine m_fcnComputeRoutine;
    BuildTaskFunc m_fcnTaskBuilder;
    DataDestructor m_fcnDataDestructor;
    Node* m_pMyNode;
    uint32_t m_state;

    // vvvvvvvvv TASK DATA IS STORED AS A SUFFIX vvvvvvvvvvvvvvvvvv
};

#ifdef GTS_MSVC
#pragma warning(pop)
#endif

} // namespace gts
