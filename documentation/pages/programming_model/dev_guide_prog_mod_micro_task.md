Micro-scheduler Task Interface {#dev_guide_prog_mod_micro_task}
-----------------

This section covers the Micro-scheduler's low-level Task interface. It is the interface used to build all
the [Parallel Patterns](@ref ParallelPatterns).

@warning The Task interface is for low-level, high performance algorithm design. It is recommended to
only use this interface if there is not an available Parallel Pattern. For a thorough understanding of
the pitfalls, please read this entire section especial (@ref dev_guide_pitfalls).

# Tasks

In GTS a Task is modeled by the [Task](@ref gts::Task) class. Tasks can be syntactically represented in three ways:

* **By Subclassing Task**

~~~~~~~~~~~~~~~{.cpp}
// The extended Task class.
struct IncTask : public gts::Task
{
    IncTask(int& v) : val(v) {}

    // Override execute to define your work.
    virtual Task* execute(gts::TaskContext const&) final
    {
        ++val;
        return nullptr;
    }
    
    int val;
};

// ...

// The value to increment.
int val = 3;

// Create the Task.
gts::Task* pTask = microScheduler.allocateTask<IncTask>(val);

// Spawn it with a Micro-scheduler and wait for it to complete.
microScheduler.spawnTaskAndWait(pTask);
GTS_ASSERT(val == 4);
~~~~~~~~~~~~~~~

* **By use of Lambdas**

~~~~~~~~~~~~~~~{.cpp}
// The value to increment.
int val = 3;

// Create the Task with a lambda.
gts::Task* pTask = microScheduler.allocateTask([&val](gts::TaskContext const&)
{
    ++val;
    return nullptr;
});

// Spawn it with a Micro-scheduler and wait for it to complete.
microScheduler.spawnTaskAndWait(pTask);
GTS_ASSERT(val == 4);
~~~~~~~~~~~~~~~

* **By use of C-style functions**

~~~~~~~~~~~~~~~{.cpp}
// The Task function.
gts::Task* incrementFunction(void* pData, gts::TaskContext const& ctx)
{
    int& val = *(int*)pData;
    ++val;
    return nullptr;
}

// ...

// The value to increment.
int val = 3;

// Create the Task with a function and pointer.
gts::Task* pTask = microScheduler.allocateTask<CStyleTask>(incrementFunction, &val);

// Spawn it with a Micro-scheduler and wait for it to complete.
microScheduler.spawnTaskAndWait(pTask);
GTS_ASSERT(val == 4);
~~~~~~~~~~~~~~~

@note Much of the Task interface for GTS is inspired by TBB. If you already know TBB, GTS will be familiar to you.

## Activation Frame

The activation frame of a Task is the gts::Task execute method. It is were a Task's work is done, and it is
where dependenies can be dynaimcally added to executing Task.

~~~~~~~~~~~~~~~{.cpp}
virtual gts::Task* myExecute(gts::TaskContext const& ctx) final
{
    ...
    return nullptr;
}
~~~~~~~~~~~~~~~

### Task Context

The gts::TaskContext is a parameter that is passed into the activation frame. It contains contextual information
about the Task like
* The Task itself, which is useful for labmda Task
* A reference to the Micro-scheduler that created the Task, which is needed for added dependecies.
* A OwnedId of the Worker thread executing the Task, which is useful for binning resources.

### Return Task

The Task returned from the Activation Frame is executed immediately, completely bypassing the scheduler. We'll
see how to used this as an optimization [later](@ref task_bypassing).

## Spawning

* **Fire-and-forget:** Spawns a Task that does not need to be waited on for completion.
~~~~~~~~~~~~~~~{.cpp}
gts::Task* pTask = microScheduler.allocateTask([](gts::TaskContext const& ctx) { ... });
microScheduler.spawnTask(pTask);
~~~~~~~~~~~~~~~

* **Fire-and-wait:** Spawns a Task and waits for it to complete.
~~~~~~~~~~~~~~~{.cpp}
gts::Task* pTask = microScheduler.allocateTask([](gts::TaskContext const& ctx) { ... });
microScheduler.spawnTaskAndWait(pTask);
~~~~~~~~~~~~~~~

* **Affinity:** Forces a Task to run on a specific Worker thread. It must be set before spawning.
~~~~~~~~~~~~~~~{.cpp}
// Let's say we only want to run this Task on Worker thread 3
// of the WorkerPool attached to the Micro-scheduler.
pTask->setAffinity(3);
// Now this Task can only every be run on Worker thread 3.
~~~~~~~~~~~~~~~

* **Priority:** Spawns a Task the specified [Task priority](@ref dev_guide_micro_desc).
~~~~~~~~~~~~~~~{.cpp}
uin32_t priority = 2
microScheduler.spawnTask(pTask, priority);
~~~~~~~~~~~~~~~

## Lifetime
Task are dynamic, reference counted objects. They are created by a Micro-scheduler with a reference count of one
and they lose a reference when executed. If their reference count becomes zero after execution, they are destroyed.

~~~~~~~~~~~~~~~{.cpp}
microScheduler.spawnTaskAndWait(pTask);
pTask->doSomething(); // Error! pTask no longer exists after execution.
~~~~~~~~~~~~~~~

[Later](@ref dev_guide_prebuilt_topo) we'll explore how to manipulate the reference count.

## Dependencies

Task dependencies are modeled by making a Task a child of another Task, or by making a Task a continuation of another Task.

### Children

Child Tasks model forks. Children refer to their parent Task directly, and a parent Task refers to its children
in reference count only. When a child Task has finished executing, it decrements its parent’s reference count.

![](ChildTask.svg)

The safest way to add a child is with a reference count, as it automatically adds the reference to the parent.

~~~~~~~~~~~~~~~{.cpp}
virtual gts::Task* myExecute(gts::TaskContext const& ctx) final
{
    gts::Task* pChild = ctx.pMicroScheduler->allocateTask([](gts::TaskContext const& ctx) { ... });
    addChildWithRef(pChild);
    ctx.pMicroScheduler->spawnTask(pChild); // fork.
    ...
~~~~~~~~~~~~~~~

Yet, this can be inefficient when adding multiple children. In this case, it is better to manually adjust the
reference count and then add the children without reference increment

~~~~~~~~~~~~~~~{.cpp}
virtual gts::Task* myExecute(gts::TaskContext const& ctx) final
{
    // If the child count is known up-front, we can added the references without incuring an
    // expensive XADD per child.
    addRef(2, memory_order::relaxed);

    gts::Task* pChild1 = ctx.pMicroScheduler->allocateTask([](gts::TaskContext const& ctx) { ... });
    addChildWithoutRef(pChild1);
    ctx.pMicroScheduler->spawnTask(pChild1);
    
    gts::Task* pChild2 = ctx.pMicroScheduler->allocateTask([](gts::TaskContext const& ctx) { ... });
    addChildWithoutRef(pChild2);
    ctx.pMicroScheduler->spawnTask(pChild2);
    ...
~~~~~~~~~~~~~~~

@warning Forgetting to add reference counts will cause undefined behavior.

### Continuations

Continuations are Tasks that join two or more child Tasks. The child Tasks refer directly to the
continuation and the continuation Task refers to its children in reference count only. When creating
a continuation, the current Task is unlinked from the task graph and the continuation in link in its place.

![](ContinuationTask.svg)

When a child finishes executing, it decrements its continuations’s reference count. If the reference
count is one, meaning that it is the last child, it executes the continuation.

Here is an example of inserting a continuation Task into a Task graph.

~~~~~~~~~~~~~~~{.cpp}
virtual gts::Task* myExecute(gts::TaskContext const& ctx) final
{
    gts::Task* pContinuation = ctx.pMicroScheduler->allocateTask([](gts::TaskContext const& ctx) { ... });
    setContinuation(pContinuation);

    // We now add the children to the continuation, so increase its ref count.
    pContinuation->addRef(2, memory_order::relaxed);

    gts::Task* pChild1 = ctx.pMicroScheduler->allocateTask([](gts::TaskContext const& ctx) { ... });
    pContinuation->addChildWithoutRef(pChild1);
    ctx.pMicroScheduler->spawnTask(pChild1);
    
    gts::Task* pChild2 = ctx.pMicroScheduler->allocateTask([](gts::TaskContext const& ctx) { ... });
    pContinuation->addChildWithoutRef(pChild2);
    ctx.pMicroScheduler->spawnTask(pChild2);
    ...
~~~~~~~~~~~~~~~

@warning Forgetting to add reference counts will cause undefined behavior.

# Joining

GTS offer two options for joins: blocking joins and continuations.

## Blocking

Blocking joins are joins that wait for a Task to complete. Blocking joins block the current execution stream, however
they do not block the executing thread. Instead the executing thread executes work in the Micro-scheduler.

@note Blocking joins are the simplest form of joining in the Task model, yet they can also introduce the most latency.
The latency comes from executing Tasks in the Micro-scheduler while waiting, which can cause the blocked execution
stream to not immediately resume when ready.

Here is an example of blocking with gts::Task::waitForAll. Note the extra reference count added for the wait. /(This is done
to distinguish a waiting parent from a waiting continuation./)
~~~~~~~~~~~~~~~{.cpp}
virtual gts::Task* myExecute(gts::TaskContext const& ctx) final
{
    // Add references for the children plus 1 for the wait.
    addRef(2 + 1, memory_order::relaxed);

    gts::Task* pChild1 = ctx.pMicroScheduler->allocateTask([](gts::TaskContext const& ctx) { ... });
    addChildWithoutRef(pChild1);
    ctx.pMicroScheduler->spawnTask(pChild1);
    
    gts::Task* pChild2 = ctx.pMicroScheduler->allocateTask([](gts::TaskContext const& ctx) { ... });
    addChildWithoutRef(pChild2);
    ctx.pMicroScheduler->spawnTask(pChild2);
    
    waitForAll(); // blocking join.
    
    // do more stuff ...
    
    return nullptr;
}
~~~~~~~~~~~~~~~

We can also used gts::Task::spwandWaitForAll, which combines spawning and blocking. It is slightly more
efficient for the last child Task.
~~~~~~~~~~~~~~~{.cpp}
virtual gts::Task* myExecute(gts::TaskContext const& ctx) final
{
    // Add references for the children plus 1 for the wait.
    addRef(2 + 1, memory_order::relaxed);

    gts::Task* pChild1 = ctx.pMicroScheduler->allocateTask([](gts::TaskContext const& ctx) { ... });
    addChildWithoutRef(pChild1);
    ctx.pMicroScheduler->spawnTask(pChild1);
    
    gts::Task* pChild2 = ctx.pMicroScheduler->allocateTask([](gts::TaskContext const& ctx) { ... });
    addChildWithoutRef(pChild2);
    
    // Combines spawning and blocking. Slightly more efficient for the last Task.
    spawnTaskAndWaitForAll(pChild2);// blocking join.
    
    // do more stuff ...
    
    return nullptr;
}
~~~~~~~~~~~~~~~

## Continuations

Continuations are explicit join Tasks that execute immediately when their dependencies have completed. Generally,
continuations do not suffer the latency of blocking joins, but they do require explicit linking into a Task graph.

~~~~~~~~~~~~~~~{.cpp}
virtual gts::Task* myExecute(gts::TaskContext const& ctx) final
{
    gts::Task* pContinuation = ctx.pMicroScheduler->allocateTask([](gts::TaskContext const& ctx) { ... });
    setContinuation(pContinuation);

    pContinuation->addRef(2, memory_order::relaxed);

    gts::Task* pChild1 = ctx.pMicroScheduler->allocateTask([](gts::TaskContext const& ctx) { ... });
    pContinuation->addChildWithoutRef(pChild1);
    ctx.pMicroScheduler->spawnTask(pChild1);
    
    gts::Task* pChild2 = ctx.pMicroScheduler->allocateTask([](gts::TaskContext const& ctx) { ... });
    pContinuation->addChildWithoutRef(pChild2);
    ctx.pMicroScheduler->spawnTask(pChild2);
    
    // No waiting. The last child executed will execute the continuation.
    
    return nullptr;
}
~~~~~~~~~~~~~~~

## Optimizations

### Bypassing ## {#task_bypassing}

Bypassing is an optimization where a Task is returned from the activation frame instead of being spawned. It is
an optimization because the returned Task is executed immediately instead of being store and retrieved from
from the Micro-scheduler’s internal Task storage. Bypassing with a child Tasks requires the use of continuations
becuase there is no longer an option of waiting. Bypassing fire-and-forget Tasks have no constraints.

~~~~~~~~~~~~~~~{.cpp}
virtual gts::Task* myExecute(gts::TaskContext const& ctx) final
{
    gts::Task* pContinuation = ctx.pMicroScheduler->allocateTask([](gts::TaskContext const& ctx) { ... });
    setContinuation(pContinuation);
    
    pContinuation->addRef(2, memory_order::relaxed);

    gts::Task* pChild1 = ctx.pMicroScheduler->allocateTask([](gts::TaskContext const& ctx) { ... });
    pContinuation->addChildWithoutRef(pChild1);
    ctx.pMicroScheduler->spawnTask(pChild1);
    
    gts::Task* pChild2 = ctx.pMicroScheduler->allocateTask([](gts::TaskContext const& ctx) { ... });
    pContinuation->addChildWithoutRef(pChild2);
    
    // Return the Task instead of spawning to bypass the scheduler.
    //ctx.pMicroScheduler->spawnTask(pChild2);
    return pChild2;
}
~~~~~~~~~~~~~~~


### Task Recycling

Task recycling is an optimization that reuses the currently executing Task as a new Tasks. Once the current
Task finished excuting it is not destroyed and instead treaded as a newly spawned Task.

Here are the safe ways to recycle a Task:

* **As a child with bypassing**:
~~~~~~~~~~~~~~~{.cpp}
virtual gts::Task* myExecute(gts::TaskContext const& ctx) final
{
    gts::Task* pContinuation = ctx.pMicroScheduler->allocateTask([](gts::TaskContext const& ctx) { ... });
    setContinuation(pContinuation);

    pContinuation->addRef(2, memory_order::relaxed);

    gts::Task* pChild1 = ctx.pMicroScheduler->allocateTask([](gts::TaskContext const& ctx) { ... });
    pContinuation->addChildWithoutRef(pChild1);
    ctx.pMicroScheduler->spawnTask(pChild1);
    
    // Recyle this Task instead of spawning a new one.
    //gts::Task* pChild2 = ctx.pMicroScheduler->allocateTask([](gts::TaskContext const& ctx) { ... });
    //pContinuation->addChildWithoutRef(pChild2);
    //ctx.pMicroScheduler->spawnTask(pChild2);
    recyleAsChildOf(pContinuationTask);
    
    // Reset some data on this Task...
    
    return this;
}
~~~~~~~~~~~~~~~

* **As a continuation**:
~~~~~~~~~~~~~~~{.cpp}
virtual gts::Task* myExecute(gts::TaskContext const& ctx) final
{
    recycle();
    setContinuation(this);

    // We now add the children to the continuation, so increase its ref count.
    addRef(2, memory_order::relaxed);

    gts::Task* pChild1 = ctx.pMicroScheduler->allocateTask([](gts::TaskContext const& ctx) { ... });
    pContinuation->addChildWithoutRef(pChild1);
    ctx.pMicroScheduler->spawnTask(pChild1);
    
    gts::Task* pChild2 = ctx.pMicroScheduler->allocateTask([](gts::TaskContext const& ctx) { ... });
    pContinuation->addChildWithoutRef(pChild2);
    
    // Return the Task instead of spawning to bypass the scheduler.
    //ctx.pMicroScheduler->spawnTask(pChild2);
    return pChild2;
}
~~~~~~~~~~~~~~~

* **As automatic fire-and-forget**:
~~~~~~~~~~~~~~~{.cpp}
virtual gts::Task* myExecute(gts::TaskContext const& ctx) final
{
    // This task will be respawned once this function returns.
    recycle();
    return nullptr;
}
~~~~~~~~~~~~~~~

* **As bypass fire-and-forget**:
~~~~~~~~~~~~~~~{.cpp}
virtual gts::Task* myExecute(gts::TaskContext const& ctx) final
{
    // This task will be executed immediately once this function returns.
    recycle();
    return return;
}
~~~~~~~~~~~~~~~

## Full Examples

1. [Blocking](@ref 2_blocking_join.h)
2. [Continuations](@ref 3_continuation_join.h)
3. [Bypassing](@ref 4_scheduler_bypassing.h)
4. [Recycling](@ref 5_task_recycling.h)

# General Task Graphs

The following examples demonstrate the creation of [weakly dynamic](@ref dev_guide_online_sched_cat) and
[strongly dynamic](@ref dev_guide_online_sched_cat) graphs in GTS. The examples create graphs for a 2D
prefix sum using a naïve wavefront algorithm.

![](Wavefront.svg)

@note Examples of static Task graphs are omitted because execution time is not considered in the Micro-scheduler’s
greedy scheduling algorithm.

## Weakly Dynamic Task Graph

This [example](@ref 6_weakly_dynamic_task_graph_wavefront.h) prebuilds a Task graph and then executes its. It also demonstrates
manipulating a root Task's reference count.

## Strongly Dynamic Task Graph

This [example](@ref 7_strongly_dynamic_task_graph_wavefront.h) generates the graph as the execution unfolds. It moves the data out of the Tasks and into
a memoization table. It does not require special treatment if the last Task like the previous example did. Further,
it is more efficient since parallelizes the creation of the graph.

@note GTS includes a production version of a wavefront algorithm gts::ParallelWavefront.

# Pitfalls {#dev_guide_pitfalls}

This section will cover various race condition that can occur when setting up dependencies

## Accessing a Spawned Task

~~~~~~~~~~~~~~~{.cpp}
microScheduler.spawn(pTask);
pTask->doSomething(); // Race condition! pTask may or may not be executed and destroyed.
~~~~~~~~~~~~~~~

## Children and Reference Counting

~~~~~~~~~~~~~~~{.cpp}
virtual gts::Task* myExecute(gts::TaskContext const& ctx) final
{
    gts::Task* pChild1 = ctx.pMicroScheduler->allocateTask([](gts::TaskContext const& ctx) { ... });
    ctx.pMicroScheduler->spawnTask(pChild1);

    gts::Task* pChild2 = ctx.pMicroScheduler->allocateTask([](gts::TaskContext const& ctx) { ... });
    ctx.pMicroScheduler->spawnTask(pChild2);

    waitForAll(); // Undefined! No references were added for the children.
    ...
~~~~~~~~~~~~~~~

## Blocking and Reference Counting

~~~~~~~~~~~~~~~{.cpp}
virtual gts::Task* myExecute(gts::TaskContext const& ctx) final
{
    gts::Task* pChild1 = ctx.pMicroScheduler->allocateTask([](gts::TaskContext const& ctx) { ... });
    addChildWithRef(pChild1);
    ctx.pMicroScheduler->spawnTask(pChild1);

    gts::Task* pChild2 = ctx.pMicroScheduler->allocateTask([](gts::TaskContext const& ctx) { ... });
    addChildWithRef(pChild2);
    ctx.pMicroScheduler->spawnTask(pChild2);

    waitForAll(); // Undefined! No reference was added to this task for the wait.
    ...
~~~~~~~~~~~~~~~

## Continuations and Reference Counting

~~~~~~~~~~~~~~~{.cpp}
virtual gts::Task* myExecute(gts::TaskContext const& ctx) final
{
    gts::Task* pContinuation = ctx.pMicroScheduler->allocateTask([](gts::TaskContext const& ctx) { ... });
    setContinuation(pContinuation);
    
    // Undefined! No references were added for the continuation's children.

    gts::Task* pChild1 = ctx.pMicroScheduler->allocateTask([](gts::TaskContext const& ctx) { ... });
    pContinuation->addChildWithoutRef(pChild1);
    ctx.pMicroScheduler->spawnTask(pChild1);
    
    gts::Task* pChild2 = ctx.pMicroScheduler->allocateTask([](gts::TaskContext const& ctx) { ... });
    pContinuation->addChildWithoutRef(pChild2);
    ctx.pMicroScheduler->spawnTask(pChild2);
    
    return nullptr;
}
~~~~~~~~~~~~~~~

## Blocking and Stack Overflow

Prefer continuations to blocking, as blocking can overflow the stack.
~~~~~~~~~~~~~~~{.cpp}
gts::Task* stackOverflowTask(TaskContext const& ctx)
{
    // Infinite recursion!
    addRef(2, memory_order::relaxed);
    gts::Task* pTask = ctx.pMicroScheduler->allocateTask(taskFunc);
    addChildWithoutRef(pTask);
    ctx.pMicroScheduler->spawnTask(pTask);
    waitForAll();
}
~~~~~~~~~~~~~~~

## Cycles

~~~~~~~~~~~~~~~{.cpp}
gts::Task* pTaskA = ctx.pMicroScheduler->allocateTask([](gts::TaskContext const& ctx) { ... });
gts::Task* pTaskB = ctx.pMicroScheduler->allocateTask([](gts::TaskContext const& ctx) { ... });
gts::Task* pTaskC = ctx.pMicroScheduler->allocateTask([](gts::TaskContext const& ctx) { ... });

pTaskA->addChildWithRef(pTaskB);
pTaskB->addChildWithRef(pTaskC);
pTaskC->addChildWithRef(pTaskA);

// Undefined! Cyclic graph.
ctx.pMicroScheduler->spawnTaskAndWait(pTaskA);
~~~~~~~~~~~~~~~

<br>
<br>
<br>
<br>

