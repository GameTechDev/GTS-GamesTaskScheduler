Features {#dev_guide_features}
============================

* Easily integrate GTS into an existing game engine complete with low level platform 
overrides. Game engines support a wide variety of operating systems and hardware platforms,
with varying degrees of custom code and work-arounds. Since GTS cannot possibly
support every work-around and corner case, we have simplified engine integration
by allowing the developer to completely replace the GTS platform layer through
a configuration file (user_config.h). 
* Express high-level program flow with persistent, dynamic task DAGs that can be
executed homogeneously or heterogeneously with the [Macro-Scheduler](@ref gts::MacroScheduler).
* Jump right into parallelism with predefined [Parallel Patterns](@ref ParallelPatterns).
* Easily communicate between threads with [Parallel Containers](@ref ParallelContainers).
* Carve up CPU resources as you see fit with the [WorkerPool](@ref gts::WorkerPool).
* Express low-level algorithms and highly efficient execution policies with the [Micro-scheduler](@ref gts::MicroScheduler)
and [Task](@ref gts::Task) constructs.
* Remove bottlenecks around heap access with [gts_malloc](@ref GtsMalloc).
* Avoid contention and kernel-mode synchronization with GTS's user-mode [synchronization primitives and contention 
reducing constructs](@ref SynchronizationPrimitives).
* STL and OS header free interface. GTS won't pollute your engine with unnecessary headers.
