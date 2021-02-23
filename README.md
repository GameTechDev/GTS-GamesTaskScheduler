[![maintained](https://img.shields.io/maintenance/yes/2021.svg)]()
[![license](https://img.shields.io/badge/License-MIT-blue.svg)]()
[![language count](https://img.shields.io/github/languages/count/GameTechDev/GTS-GamesTaskScheduler.svg)]()
[![top language](https://img.shields.io/github/languages/top/GameTechDev/GTS-GamesTaskScheduler.svg)]()

# Intel Games Task Scheduler (GTS)

> To the [documentation](https://gametechdev.github.io/GTS-GamesTaskScheduler/documentation/html/index.html).

## Introduction

GTS is a C++ task scheduling framework for multi-processor platforms. It is designed to let
developers express their applications in task parallelism and let the scheduling framework
maximize the physical parallelism on the available processors.

### Key Goals

* **Simple expression of task parallelism**
* **Highly efficient scheduling**
* **Easy to configure and extend**

### Target Audience

GTS is designed for the needs of current and future game engine developers. Typical engine developers employ a highly
customized, platform-scalable task system (or job system) that provides dedicated worker threads
executing thousands of concurrent tasks. These threads may also share resources with driver,
networking, middleware, and video streaming threads, all working in synchronized concert to 
deliver glitch-free real-time rendering on 60+ frames per second (FPS) applications. Future game
engines will have to cope with more threads, more tasks, and potentially multiple
instruction-set-architectures (ISAs), all running on an ever-expanding hardware landscape. 

We built GTS to be simple and friendly to game engine task system use cases. We want a framework
that allows the game development community to experiment with and learn from different scheduling
algorithms easily. We also want a framework that allows us to demonstrate state-of-the-art algorithms
on task scheduling. Finally, we want to encourage games to better express parallelism so they can
compute more cool stuff and enable richer PC gaming experiences!

### Features

* Easily integrate GTS into an existing game engine complete with low level platform 
overrides. Game engines support a wide variety of operating systems and hardware platforms,
with varying degrees of custom code and work-arounds. Since GTS cannot possibly
support every work-around and corner case, we have simplified engine integration
by allowing the developer to completely replace the GTS platform layer through
a configuration file (user_config.h). 
* Express high-level program flow with persistent, dynamic task DAGs that can be
executed homogeneously or heterogeneously with the Macro-Scheduler
* Jump right into parallelism with predefined Parallel Patterns
* Easily communicate between threads with Parallel Containers
* Carve up CPU resources as you see fit with the WorkerPool
* Express low-level algorithms and highly efficient execution policies with the Micro-scheduler
and Task constructs.
* Remove bottlenecks around heap access with gts_malloc.
* Avoid contention and kernel-mode synchronization with GTS's user-mode synchronization primitives and contention 
reducing constructs.
* OS-header-free and mostly STL-free interface. GTS won't pollute your engine with unnecessary headers.

<br>
<br>
<br>
<br>

