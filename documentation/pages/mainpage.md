Games Task Scheduler (GTS)
============================

> Get the [code](https://github.com/GameTechDev/GTS-GamesTaskScheduler)!

> To the [docs](@ref developer_guide).

# Introduction

GTS is a C++ task scheduling framework for multi-processor platforms. It is designed to let
developers express their applications in task parallelism and let the scheduling framework
maximize the physical parallelism on the available processors.

## Key Goals

* **Simple expression of task parallelism**
* **Highly efficient scheduling**
* **Easy to configure and extend**

## Target Audience

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

## Features ##

* **Clean engine integration**
* **Expressive high-level dependency layout**
* **Efficient low-level scheduling primitives**
* **Parallel patterns and containers**
* **Scalable memory allocator**
* [more](@ref dev_guide_features)

<br>
<br>
<br>
<br>

