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
#include "gts/macro_scheduler/Workload.h"
#include "gts/macro_scheduler/ComputeResource.h"

namespace gts {

class Task;
class MicroScheduler;
struct TaskContext;
class MicroScheduler_Workload;

/**
 * @addtogroup MacroScheduler
 * @{
 */

/** 
 * @addtogroup ComputeResources
 * @{
 */

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A Task payload for the MicroScheduler_Workload.
 */
class MicroScheduler_Task : public Task
{
public:

    MicroScheduler_Task(
        MicroScheduler_Workload* pThisWorkload,
        WorkloadContext workloadContext);

    virtual Task* execute(TaskContext const& ctx) final;

private:

    ComputeResource* findThisComputeResource(SubIdType microSchedulerId);

    MicroScheduler_Workload* m_pThisWorkload;
    WorkloadContext m_workloadContext;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A abstract Workload that maps to the MicroScheduler. Subclass to create
 *  concrete MicroScheduler Workloads.
 */
class MicroScheduler_Workload : public Workload
{
public:

    GTS_INLINE MicroScheduler_Workload()
        : Workload(WorkloadType::CPP)
        , m_workerId(ANY_WORKER)
    {}

    GTS_INLINE uint32_t workerAffinityId() const
    {
        return m_workerId;
    }

    GTS_INLINE void setWorkerAffinityId(uint32_t workerId)
    {
        m_workerId = workerId;
    }

protected:

    friend class MicroScheduler_Task;

    TaskContext m_executionContext;
    uint32_t m_workerId;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A concrete lambda Workload that maps to the MicroScheduler.
 */
class MicroSchedulerLambda_Workload : public MicroScheduler_Workload
{
    template<typename TFunc, typename... TArgs>
    static GTS_INLINE void _invoker(void* pRawTuple, WorkloadContext const& ctx)
    {
        // decode the tuple
        using TupleType = std::tuple<TFunc, TArgs... >;
        TupleType& tuple = *(TupleType*)pRawTuple;

        _invoke(tuple, std::make_integer_sequence<size_t, std::tuple_size<TupleType>::value>(), ctx);
    }

    template<typename TTuple, size_t... Idxs>
    static GTS_INLINE void _invoke(TTuple& tuple, std::integer_sequence<size_t, Idxs...>, WorkloadContext const& ctx)
    {
        // invoke function object packed in tuple
        invoke<void>(std::move(std::get<Idxs>(tuple))..., ctx);
    }

    using invoke_fn = void(*)(void*, WorkloadContext const&);

public: // STRUCTORS:

    template<typename TFunc, typename... TArgs>
    GTS_INLINE MicroSchedulerLambda_Workload(TFunc&& func, TArgs&&... args)
        : m_invoker(&_invoker<TFunc, TArgs...>)
    {
        // Place the tuple at the end of this object.
        new (this + 1) std::tuple<TFunc, TArgs... >(
            std::forward<TFunc>(func), std::forward<TArgs>(args)...
            );
    }

public: // MUTATORS:

    GTS_INLINE virtual void execute(WorkloadContext const& ctx) final
    {
        m_invoker(getTuple(), ctx);
    }

private:

    GTS_INLINE void* getTuple()
    {
        return this + 1;
    }

    invoke_fn m_invoker;
};

/** @} */ // end of ComputeResources
/** @} */ // end of MacroScheduler

} // namespace gts
