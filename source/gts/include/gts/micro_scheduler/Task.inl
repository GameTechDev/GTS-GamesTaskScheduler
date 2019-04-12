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

//------------------------------------------------------------------------------
Task::Task()
    : m_fcnTaskRoutine(nullptr)
    , m_fcnDataDestructor(emptyDataDestructor)
    , m_pParent(nullptr)
    , m_pMyScheduler(nullptr)
    , m_isolationTag(0)
    , m_affinity(UINT32_MAX)
    , m_refCount(1)
    , m_state(0)
{}


//------------------------------------------------------------------------------
Task::Task(TaskRoutine taskRountine)
    : m_fcnTaskRoutine(taskRountine)
    , m_fcnDataDestructor(emptyDataDestructor)
    , m_pParent(nullptr)
    , m_pMyScheduler(nullptr)
    , m_isolationTag(0)
    , m_affinity(UINT32_MAX)
    , m_refCount(1)
    , m_state(0)
{}

//------------------------------------------------------------------------------
Task::Task(Task const&)
{
    // must impl for STL :(
    GTS_ASSERT(0 && "Cannot copy a task");
}

//------------------------------------------------------------------------------
Task* Task::execute(Task* thisTask, TaskContext const& taskContext)
{
    return m_fcnTaskRoutine(thisTask, taskContext);
}

// MUTATORS:

//------------------------------------------------------------------------------
void Task::addChildTaskWithoutRef(Task* pChild)
{
    GTS_ASSERT(pChild->m_pParent == nullptr);
    GTS_ASSERT(m_refCount.load(gts::memory_order::acquire) > 1 && "Ref count is 1, did you forget to addRef?");

    GTS_INSTRUMENTER_MARKER(analysis::Tag::INTERNAL, "ADD CHILD", this, pChild);

    pChild->m_pParent = this;
}

//------------------------------------------------------------------------------
void Task::addChildTaskWithRef(Task* pChild, gts::memory_order order)
{
    addRef(1, order);
    addChildTaskWithoutRef(pChild);
}


//------------------------------------------------------------------------------
void Task::setContinuationTask(Task* pContinuation)
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

//------------------------------------------------------------------------------
void Task::recyleAsChildOf(Task* pParent)
{
    m_state |= RECYLE;
    m_pParent = pParent;
}

//------------------------------------------------------------------------------
template<typename T, typename... TArgs>
T* Task::emplaceData(TArgs&&... args)
{
    m_fcnDataDestructor(_dataSuffix());

    m_state |= TASK_HAS_DATA_SUFFIX;
    m_fcnDataDestructor = dataDestructor<T>;
    return new (_dataSuffix()) T(std::forward<TArgs>(args)...);
}

//------------------------------------------------------------------------------
template<typename T>
T* Task::setData(T const& data)
{
    m_fcnDataDestructor(_dataSuffix());

    m_fcnDataDestructor = dataDestructor<T>;
    m_state |= TASK_HAS_DATA_SUFFIX;
    return new (_dataSuffix()) T(data);
}

//------------------------------------------------------------------------------
template<typename T>
T* Task::setData(T* data)
{
    m_fcnDataDestructor(_dataSuffix());

    m_state &= ~TASK_HAS_DATA_SUFFIX;
    m_fcnDataDestructor = emptyDataDestructor;
    ::memcpy(_dataSuffix(), &data, sizeof(T*)); // stores address number!
    return (T*)((uintptr_t*)_dataSuffix())[0];
}

//------------------------------------------------------------------------------
void Task::setAffinity(uint32_t workerIdx)
{
    m_affinity = workerIdx;
}

//------------------------------------------------------------------------------
int32_t Task::addRef(int32_t count, gts::memory_order order)
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

//------------------------------------------------------------------------------
int32_t Task::removeRef(int32_t count, gts::memory_order order)
{
    GTS_ASSERT(m_refCount.load(gts::memory_order::relaxed) - count >= 0);
    return addRef(-count, order);
}

//------------------------------------------------------------------------------
void Task::setRef(int32_t count, gts::memory_order order)
{
    GTS_ASSERT(m_refCount.load(gts::memory_order::relaxed) > 0);
    m_refCount.store(count, order);
}

// ACCCESSORS:

//------------------------------------------------------------------------------
int32_t Task::refCount(gts::memory_order order) const
{
    return m_refCount.load(order);
}

//------------------------------------------------------------------------------
uint32_t Task::getAffinity() const
{
    return m_affinity;
}

//------------------------------------------------------------------------------
void const* Task::getData() const
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

//------------------------------------------------------------------------------
void* Task::getData()
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

//------------------------------------------------------------------------------
bool Task::isStolen() const
{
    return (m_state & TASK_IS_STOLEN) != 0;
}

//------------------------------------------------------------------------------
Task* Task::parent()
{
    return m_pParent;
}

//------------------------------------------------------------------------------
uintptr_t Task::isolationTag() const
{
    return m_isolationTag;
}

// HELPERS:

//------------------------------------------------------------------------------
void* Task::_dataSuffix()
{
    return this + 1;
}

//------------------------------------------------------------------------------
void const* Task::_dataSuffix() const
{
    return this + 1;
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// LambdaTaskWrapper

//------------------------------------------------------------------------------
template<typename TFunc, typename...TArgs>
template<size_t... Idxs>
Task* LambdaTaskWrapper<TFunc, TArgs...>::_invoke(std::integer_sequence<size_t, Idxs...>, Task* pThisTask, TaskContext const& ctx)
{
    // invoke function object packed in tuple
    return std::invoke(std::move(std::get<Idxs>(m_tuple))..., pThisTask, ctx);
}

//------------------------------------------------------------------------------
template<typename TFunc, typename...TArgs>
LambdaTaskWrapper<TFunc, TArgs...>::LambdaTaskWrapper(TFunc&& func, TArgs&&...args)
    : m_tuple(std::forward<TFunc>(func), std::forward<TArgs>(args)...)
{}

//------------------------------------------------------------------------------
template<typename TFunc, typename...TArgs>
gts::Task* LambdaTaskWrapper<TFunc, TArgs...>::taskFunc(gts::Task* pThisTask, gts::TaskContext const& ctx)
{
    GTS_ASSERT(pThisTask != nullptr);

    // Unpack the data.
    LambdaTaskWrapper<TFunc, TArgs...>* pNodeTask = (LambdaTaskWrapper<TFunc, TArgs...>*)pThisTask->getData();

    return pNodeTask->_invoke(std::make_integer_sequence<size_t, std::tuple_size<FucnArgsTupleType>::value>(), pThisTask, ctx);
}
