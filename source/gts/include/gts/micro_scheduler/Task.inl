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

 ////////////////////////////////////////////////////////////////////////////////
 ////////////////////////////////////////////////////////////////////////////////
 // Task

// MUTATORS:

//------------------------------------------------------------------------------
void Task::addChildTaskWithoutRef(Task* pChild)
{
    GTS_ASSERT(pChild->header().pParent == nullptr);
    GTS_ASSERT(header().refCount.load(memory_order::acquire) > 1 && "Ref count is 1, did you forget to addRef?");

    GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::MICRO_SCHEDULER_ALL, analysis::Color::SeaGreen4, "ADD CHILD", this, pChild);

    pChild->header().pParent = this;
}

//------------------------------------------------------------------------------
void Task::addChildTaskWithRef(Task* pChild, memory_order order)
{
    addRef(1, order);
    addChildTaskWithoutRef(pChild);
}

//------------------------------------------------------------------------------
void Task::setContinuationTask(Task* pContinuation)
{
    GTS_ASSERT(pContinuation->header().pParent == nullptr);

    GTS_ASSERT(
        (header().executionState == internal::TaskHeader::EXECUTING) &&
        "This task is not executing. Setting a continuation will orphan this task from the DAG.");

    GTS_TRACE_ZONE_MARKER_P2(analysis::CaptureMask::MICRO_SCHEDULER_ALL, analysis::Color::SeaGreen4, "SET CONT", this, pContinuation);

    pContinuation->header().flags |= internal::TaskHeader::TASK_IS_CONTINUATION;

    if(pContinuation != this) // if not recycling.
    {
        // Unlink this task from the DAG
        Task* parent = header().pParent;
        header().pParent = nullptr;

        // Link the continuation to this task's parent to reconnect the DAG.
        pContinuation->header().pParent = parent;
    }
}

//------------------------------------------------------------------------------
void Task::recycle()
{
    header().executionState = internal::TaskHeader::ALLOCATED;
}

//------------------------------------------------------------------------------
void Task::setAffinity(uint32_t workerIdx)
{
    header().affinity = workerIdx;
}

//------------------------------------------------------------------------------
int32_t Task::addRef(int32_t count, memory_order order)
{
    GTS_ASSERT(header().refCount.load(memory_order::relaxed) > 0);

    int32_t refCount;

    switch (order)
    {
    case memory_order::relaxed:
        refCount = header().refCount.load(memory_order::relaxed) + count;
        header().refCount.store(refCount, memory_order::relaxed);
        return refCount;

    default:
        return header().refCount.fetch_add(count, order) + count;
    }
}

//------------------------------------------------------------------------------
int32_t Task::removeRef(int32_t count, memory_order order)
{
    GTS_ASSERT(header().refCount.load(memory_order::relaxed) - count >= 0);
    return addRef(-count, order);
}

//------------------------------------------------------------------------------
void Task::setRef(int32_t count, memory_order order)
{
    GTS_ASSERT(header().refCount.load(memory_order::relaxed) > 0);
    header().refCount.store(count, order);
}

//------------------------------------------------------------------------------
void Task::setName(const char* name)
{
#ifdef GTS_USE_TASK_NAME
    header().pName = name;
#else
    GTS_UNREFERENCED_PARAM(name);
#endif
}

// ACCCESSORS:

//------------------------------------------------------------------------------
int32_t Task::refCount(memory_order order) const
{
    return header().refCount.load(order);
}

//------------------------------------------------------------------------------
uint32_t Task::getAffinity() const
{
    return header().affinity;
}

//------------------------------------------------------------------------------
bool Task::isStolen() const
{
    return (header().flags & internal::TaskHeader::TASK_IS_STOLEN) != 0;
}

//------------------------------------------------------------------------------
Task* Task::parent()
{
    return header().pParent;
}

//------------------------------------------------------------------------------
const char* Task::name() const
{
#ifdef GTS_USE_TASK_NAME
    return header().pName;
#else
    return "define GTS_USE_TASK_NAME";
#endif
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// CStyleTask

//------------------------------------------------------------------------------
CStyleTask::CStyleTask()
    : m_fcnTaskRoutine(nullptr)
    , m_fcnDataDestructor(nullptr)
    , m_pData(nullptr)
{}


//------------------------------------------------------------------------------
CStyleTask::CStyleTask(CStyleTask const&)
    : m_fcnTaskRoutine(nullptr)
    , m_fcnDataDestructor(nullptr)
    , m_pData(nullptr)
{
    // must impl for STL :(
    GTS_ASSERT(0 && "Cannot copy a task");
}

//------------------------------------------------------------------------------
template<typename TData>
CStyleTask::CStyleTask(TaskRoutine taskRountine, TData* pData)
    : m_fcnTaskRoutine(taskRountine)
    , m_fcnDataDestructor(nullptr)
    , m_pData(pData)
{
    _resetDataDestructor<TData*>();
}

//------------------------------------------------------------------------------
CStyleTask::~CStyleTask()
{
    if(m_fcnDataDestructor)
    {
        m_fcnDataDestructor(getData());
    }
}

// MUTATORS:

//------------------------------------------------------------------------------
Task* CStyleTask::execute(TaskContext const& taskContext)
{
    return m_fcnTaskRoutine(getData(), taskContext);
}

//------------------------------------------------------------------------------
template<typename TData>
TData* CStyleTask::setData(TData* pData)
{
    m_pData = pData;
    _resetDataDestructor<TData*>();
}

// ACCCESSORS:

//------------------------------------------------------------------------------
void const* CStyleTask::getData() const
{
    return m_pData;
}

//------------------------------------------------------------------------------
void* CStyleTask::getData()
{
    return m_pData;
}

// HELPERS:

//------------------------------------------------------------------------------
template<typename TData>
void CStyleTask::_resetDataDestructor()
{
    if (m_fcnDataDestructor)
    {
        m_fcnDataDestructor(m_pData);
        m_fcnDataDestructor = nullptr;
    }

    static bool isNotPointerOrTriviallyDestructable = !(std::is_pointer<TData>::value || std::is_trivially_destructible<TData>::value);
    if (isNotPointerOrTriviallyDestructable)
    {
        m_fcnDataDestructor = dataDestructor<TData>;
    }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// LambdaTaskWrapper

//------------------------------------------------------------------------------
template<typename TFunc, typename...TArgs>
template<size_t... Idxs>
Task* LambdaTaskWrapper<TFunc, TArgs...>::_invoke(std::integer_sequence<size_t, Idxs...>, TaskContext const& ctx)
{
    // invoke function object packed in tuple
    return invoke<Task*>(std::move(std::get<Idxs>(m_tuple))..., ctx);
}

//------------------------------------------------------------------------------
template<typename TFunc, typename...TArgs>
LambdaTaskWrapper<TFunc, TArgs...>::LambdaTaskWrapper(TFunc&& func, TArgs&&...args)
    : m_tuple(std::forward<TFunc>(func), std::forward<TArgs>(args)...)
{}

//------------------------------------------------------------------------------
template<typename TFunc, typename...TArgs>
Task* LambdaTaskWrapper<TFunc, TArgs...>::execute(TaskContext const& ctx)
{
    return _invoke(std::make_integer_sequence<size_t, std::tuple_size<FucnArgsTupleType>::value>(), ctx);
}
