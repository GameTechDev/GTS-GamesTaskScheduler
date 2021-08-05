Quick Start Guide {#quick_start}
============================

This section is a brief overview of how to get started with GTS. See @ref dev_guide_programming_model for the gritty details.

# Worker Pool

A [Worker Pool](@ref gts::WorkerPool) is the fundamental ***executor*** of CPU work. It determines how SW Worker threads are
mapped to a processor's HW threads. By default, GTS creates one SW thread per HW thread to avoid oversubscription.

![Figure 1: A simple Worker Pool mapping.](WorkerPoolMapping.svg)

~~~~~~~~~~~~~~~{.cpp}
// Create an unintilialized WorkerPool.
gts::WorkerPool workerPool;

// Initialize it. Empty means #workers == #HW threads.
workerPool.initialize(/* you can also explicitly specify the number of workers. */);
~~~~~~~~~~~~~~~

<br />

# Micro-scheduler

A [Micro-scheduler](@ref gts::MicroScheduler) defines the scheduling policy of work onto a Worker
Pool. It is the fundamental ***scheduler*** of work onto the CPU. It creates and consumes
[Tasks](@ref gts::Task) as a fundamental unit of work. A Micro-scheduler is always
mapped to a Worker Pool with a 1:1 mapping between Local Schedulers and Workers.

![Figure 2: A simple Micro-scheduler mapping.](MicroSchedulerMapping.svg)

~~~~~~~~~~~~~~~{.cpp}
// Create an unintilialized MicroScheduler.
gts::MicroScheduler microScheduler;

// Initialize it with a WorkerPool.
microScheduler.initialize(&workerPool);
~~~~~~~~~~~~~~~

<br />

## Example

This program increments the values of all elements in a vector using a [parallel-for](@ref gts::ParallelFor) pattern.

~~~~~~~~~~~~~~~{.cpp}
#include "gts/micro_scheduler/WorkerPool.h"
#include "gts/micro_scheduler/MicroScheduler.h"
#include "gts/micro_scheduler/patterns/ParallelFor.h"

...

// Create an unintilialized WorkerPool.
gts::WorkerPool workerPool;

// Initialize it.
bool result = workerPool.initialize();
GTS_ASSERT(result);

// Create an unintilialized MicroScheduler.
gts::MicroScheduler microScheduler;

// Initialize it with a WorkerPool.
result = microScheduler.initialize(&workerPool);
GTS_ASSERT(result);

// Create a ParallelFor object attached to the scheduler.
gts::ParallelFor parFor(taskScheduler);

// Increment a vector of items in parallel.
std::vector<int> vec(1000000, 0);
parFor(vec.begin(), vec.end(), [](std::vector<int>::iterator iter) { (*iter)++; });

// Verify results.
for (auto const& v : vec)
{
    GTS_ASSERT(v == 1);
}

printf("Success!\n");

// These resources can be shutdown explicitly or their destructor will shut them down implicitly.
taskScheduler.shutdown();
workerPool.shutdown();
~~~~~~~~~~~~~~~

Ouput:
~~~~~~~~~~~~~~~
SUCCESS!
~~~~~~~~~~~~~~~

<br />

# Macro-scheduler

A [Macro-scheduler](@ref gts::MacroScheduler) is a high level scheduler of persistent task DAGs. It produces
[Schedules](@ref gts::Schedule) onto [Compute Resources](@ref gts::ComputeResource), like the Micro-scheduler. The
Task DAGs are formed by [Nodes](@ref gts::Node), with each Node containing [Workloads](@ref gts::Workload) that
define what Compute Resource it can be scheduled to.

![Figure 3: A simple Macro-scheduler mapping.](MacroScheduler.svg)

~~~~~~~~~~~~~~~{.cpp}
// Create a MicroScheduler_ComputeResource that will execute Workloads through the MicroScheduler.
MicroScheduler_ComputeResource microSchedulerCompResource(&microScheduler, 0, 0);

// Add the microSchedulerComputeResource to the description struct for the MacroScheduler.
gts::MacroSchedulerDesc macroSchedulerDesc;
macroSchedulerDesc.computeResources.push_back(&microSchedulerCompResource);

// Create and initialize the MacroScheduler with the macroSchedulerDesc.
gts::MacroScheduler* pMacroScheduler = new gts::CentralQueue_MacroScheduler;
pMacroScheduler->init(macroSchedulerDesc);
~~~~~~~~~~~~~~~

<br />

## Example 1

This program builds a DAG of Nodes, where each Node prints its name. For this example
we use the [Dynamic Micro-scheduler](@ref DynamicMicroScheduler) implementation of the Macro-scheduler.

~~~~~~~~~~~~~~~{.cpp}
#include "gts/micro_scheduler/WorkerPool.h"
#include "gts/micro_scheduler/MicroScheduler.h"

#include "gts/macro_scheduler/Node.h"
#include "gts/macro_scheduler/compute_resources/MicroScheduler_Workload.h"
#include "gts/macro_scheduler/compute_resources/MicroScheduler_ComputeResource.h"
#include "gts/macro_scheduler/schedulers/homogeneous/central_queue/CentralQueue_MacroScheduler.h"

...

// Create an unintilialized WorkerPool.
gts::WorkerPool workerPool;

// Initialize it.
bool result = workerPool.initialize();
GTS_ASSERT(result);

// Create an unintilialized MicroScheduler.
gts::MicroScheduler microScheduler;

// Initialize it with a WorkerPool.
result = microScheduler.initialize(&workerPool);
GTS_ASSERT(result);

// Create a MicroScheduler_ComputeResource that will execute Workloads through the MicroScheduler.
gts::MicroScheduler_ComputeResource microSchedulerComputeResource(&microScheduler);

// Add the microSchedulerComputeResource to the description struct for the MacroScheduler.
gts::MacroSchedulerDesc macroSchedulerDesc;
macroSchedulerDesc.computeResources.push_back(&microSchedulerComputeResource);

// Create and initialize the MacroScheduler with the macroSchedulerDesc.
gts::MacroScheduler* pMacroScheduler = new gts::CentralQueue_MacroScheduler;
GTS_ASSERT(pMacroScheduler);
pMacroScheduler->init(macroSchedulerDesc);


    //
    // Build a DAG of Nodes
    /*
                +---+
          +---->| B |-----+
          |     +---+     |
          |               v
        +---+           +---+ 
        | A |           | D |
        +---+           +---+
          |               ^
          |     +---+     |
          +---->| C |-----+
                +---+
    */

Node* pA = pMacroScheduler->allocateNode();
pA->addWorkload<gts::MicroSchedulerLambda_Workload>([](WorkloadContext const&){ printf("A "); });

Node* pB = pMacroScheduler->allocateNode();
pB->addWorkload<gts::MicroSchedulerLambda_Workload>([](WorkloadContext const&){ printf("B "); });

Node* pC = pMacroScheduler->allocateNode();
pC->addWorkload<gts::MicroSchedulerLambda_Workload>([](WorkloadContext const&){ printf("C "); });

Node* pD = pMacroScheduler->allocateNode();
pD->addWorkload<gts::MicroSchedulerLambda_Workload>([](WorkloadContext const&){ printf("D "); });

pA->addChild(pB);
pA->addChild(pC);
pB->addChild(pD);
pC->addChild(pD);

// Build and execute the schedule
gts::Schedule* pSchedule = pMacroScheduler->buildSchedule(pA, pD);
pMacroScheduler->executeSchedule(pSchedule, microSchedulerCompResource.id(), true);
~~~~~~~~~~~~~~~

Ouput:
~~~~~~~~~~~~~~~
A B C D
~~~~~~~~~~~~~~~
*or*
~~~~~~~~~~~~~~~
A C B D
~~~~~~~~~~~~~~~

<br />

## Example 2

This program builds a DAG of Nodes, where Node runs a more fine-grained computation using a
Micro-scheduler. The DAG can be seen as an execution flow that organizes the lower level
computation. In this example we will divide up execution on a vector over the Nodes.

~~~~~~~~~~~~~~~{.cpp}

//... Init Boilerplate as seen in the previous examples.

//
// Build a DAG of Nodes
/*
            +---+
      +---->| B |-----+
      |     +---+     |
      |               v
    +---+           +---+
    | A |           | D |
    +---+           +---+
      |               ^
      |     +---+     |
      +---->| C |-----+
            +---+
            
  A: Increments all elements in a vector by 1.
  B: Increments elements [0, n/2) by 2.
  C: Increments elements [n/2, n) by 3.
  D: Increments all elements by by 1.
  
  Result: { 4, 4, ..., 4, 5, 5, ..., 5}.
  
*/

gts::ParallelFor parFor(microScheduler);
std::vector<int> vec(1000000, 0);

// Increments all elements in a vector by 1.
Node* pA = pMacroScheduler->allocateNode();
pA->addWorkload<gts::MicroSchedulerLambda_Workload>([&parFor, &vec](WorkloadContext const& ctx)
{
    parFor(vec.begin(), vec.end(), [](std::vector<int>::iterator iter) { (*iter)++; });
});

// Increments elements [0, n/2) by 2.
Node* pB = pMacroScheduler->allocateNode();
pB->addWorkload<gts::MicroSchedulerLambda_Workload>([&parFor, &vec](WorkloadContext const&)
{
    parFor(vec.begin(), vec.begin() + vec.size() / 2, [](std::vector<int>::iterator iter) { (*iter) += 2; });
});

// Increments elements [n/2, n) by 3.
Node* pC = pMacroScheduler->allocateNode();
pC->addWorkload<gts::MicroSchedulerLambda_Workload>([&parFor, &vec](WorkloadContext const&)
{
    parFor(vec.begin() + vec.size() / 2, vec.end(), [](std::vector<int>::iterator iter) { (*iter) += 3; });
});

// Increments all elements by by 1.
Node* pD = pMacroScheduler->allocateNode();
pD->addWorkload<gts::MicroSchedulerLambda_Workload>([&parFor, &vec](WorkloadContext const&)
{
    parFor(vec.begin(), vec.end(), [](std::vector<int>::iterator iter) { (*iter)++; });
});

pA->addChild(pB);
pA->addChild(pC);
pB->addChild(pD);
pC->addChild(pD);

// Build and execute the schedule
gts::Schedule* pSchedule = pMacroScheduler->buildSchedule(pA, pD);
pMacroScheduler->executeSchedule(pSchedule, microSchedulerCompResource.id(), true);

// Validate result = { 4, 4, ..., 4, 5, 5, ..., 5}.
for (auto iter = vec.begin(); iter != vec.begin() + vec.size() / 2; ++iter)
{
    GTS_ASSERT(*iter == 4);
}
for (auto iter = vec.begin() + vec.size() / 2; iter != vec.end(); ++iter)
{
    GTS_ASSERT(*iter == 5);
}

printf ("SUCCESS!\n");
~~~~~~~~~~~~~~~

Ouput:
~~~~~~~~~~~~~~~
SUCCESS!
~~~~~~~~~~~~~~~

[Examples Source](@ref quick_start.h)

<br>
<br>
<br>
<br>
