# Intel Game Task Scheduler (GTS)

## What it is?

GTS is a light-weight, C++, experimental task scheduling framework designed with a "Bring Your Own Engine/Framework"
style. It consists of a work-stealing micro-scheduler and a persistent DAG macro-scheduler. It also incorporates
parallel containers, threading constructs, and debugging utilities.

## Why?

1. The need for a simple, light-weight, and engine friendly task scheduler.
2. The need for a framework that allows the game development community to experiment with different scheduling algorithms - easily.
3. A place to house state-of-the-art algorithms on task scheduling for games from both Intel and the community.
4. A place to learn about different task scheduling algorithms and parallel computing constructs.
5. Encourage games to become more parallel so they can compute more cool stuff!

## How to build

1. Download premake 5.0 https://premake.github.io/ and place it in "premake/\_scripts\_"
2. In the "premake" folder, run the "gts.bat" file.
3. Move back to the root "gts" directory and there should be an "_build..." folder with the solution in it.

## Tutorials/Examples

1. Download premake 5.0 https://premake.github.io/ and place it in "premake/\_scripts\_"
2. In the "premake" folder, run the "gts_examples.bat" file.
3. Move back to the root "gts" directory and there should be an "_build..." folder with the solution in it.
4. Example projects are numbered and should be read through in-order. Please issue anything that needs more explaination.

(*Doxygen and more example comming soon!)

## Features

NOTE: * indicates you can replace the implementation with your own.

### Micro-scheduler

1. Help-first work-stealing
2. Fork-join with nested parallelism
3. Supports arbitrary DAGs
4. Blocking joins
5. Continuation joins
6. Scheduler by-passing and task recycling optimizations
7. Task affinities. (Force a task to run on a specific thread.)
8. Task priorities with starvation resistance
9. Scheduler partitioning
10. Task execution isolation

#### Patterns

1. Parallel for
2. Parallel reduce
3. (Parallel sort) - coming soon!
4. (Parallel radix sort) - coming soon!
4. (Parallel scan) - coming soon!
3. Data partitioners
4. 1D/2D/3D iteration ranges

### Macro-scheduler (*** Work-in-progress ***)

1. Persistent, DAG structures
2. Extendible for custom scheduler implementations/experimentations
3. Allows multiple workloads to be attached to a task for heterogeneous scheduling

### Containers

#### Parallel
1. Queue single-producer single-consumer
2. Queue single-producer multi-consumer
3. Queue multi-producer single-consumer
4. Queue multi-producer multi-consumer
5. Distributed allocator
6. (Hash table) - coming soon!

#### Serial

1. Aligned allocator
2. Ring buffer
3. Vector*

### Platform

1. Assert*
2. Atomics*
3. Threads and synchronization*
4. OS memory*
5. Machine specific definitions/wrappers*

### Analysis

1. Analyzer to gather stats for scheduling algorithm
2. Concurrent logger for per thread logging (output redirection*)
3. Instrumenter*

## Known issues

1. Code comments may be incomplete, this is a work-in-progress
2. Testing is on-going and mostly complete. If you encounter issues please let us know.
3. Comments may contain spelling and grammar errors.
4. Only works on Windows with VS2015 and VS2017 (MSVC and Clang). Cross platform/compiler code is in the works!

## Example

A simple parallel-for that increments each element in an array.

```
size_t const elementCount = 1 << 16;

// Create an array of 0s.
gts::Vector<char> vec(0, elementCount);

// Make a parallel-for object for this scheduler.
ParallelFor parallelFor(taskScheduler);

// The partitioner determines how the data in BlockedRange1d is divided
// up amongst all the worker threads. The AdaptivePartitioner type only
// divides when other worker threads need more work.
auto partitionerType = AdaptivePartitioner();

// Since the partitioner is adaptive, a block size of 1 gives the paritioner
// full control over division.
size_t const blockSize = 1;

parallelFor(

    // The 1-D iteration range parallel-for will iterate over.
    BlockedRange1d<gts::Vector<char>::iterator>(vec.begin(), vec.end(), blockSize),

    // The lambda that parallel-for will map to each block of the range.
    [](BlockedRange1d<gts::Vector<char>::iterator>& range, void* pData, TaskContext const&)
    {
        // For each item in the block, increment the value.
        for (auto iter = range.begin(); iter != range.end(); ++iter)
        {
            (*iter)++;
        }
    },

    // The partitioner object.
    partitionerType
);
```

## References
1. https://www.threadingbuildingblocks.org/
2. http://supertech.csail.mit.edu/papers/steal.pdf
3. https://www.cs.cmu.edu/~guyb/papers/locality2000.pdf
4. https://arxiv.org/abs/1806.11128
5. https://www.cse.wustl.edu/~kunal/resources/Papers/nabbit.pdf


## Contribute

### Test Build

1. Download premake 5.0 https://premake.github.io/ and place it in "premake/\_scripts\_".
2. Run "git submodules update --init" to pull in googletest.
3. In the "premake" folder, run the "gts_unit_test.bat" file.
4. Move back to the root "gts" directory and there should be an "_build..." folder with the test solution in it.

### Running Test

All tests run as a post-build step. Any failing tests will fail the build.

### Licencing

Contributors of new files should add a copyright header at the top of every new source code
file with their copyright along with the MIT licensing stub.

### Formatting

Please format like the existing code. Clang format will likely be added at some point.

