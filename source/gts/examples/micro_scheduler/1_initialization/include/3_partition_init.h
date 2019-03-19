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
#include "gts/platform/Thread.h"

#include "gts/micro_scheduler/WorkerPool.h"
#include "gts/micro_scheduler/MicroScheduler.h"

using namespace gts;

namespace gts_examples {

void partitionInit()
{
    // Imagine a scenario where you have long running tasks that need to run
    // along side your normal update loop. You wouldn't want to mix these with
    // your update loop tasks because they would starve them and tanks your
    // framerate. To handle this quality-of-service issue, the micro scheduler
    // allows you to partition the worker pool and scheduler

    // Create a simple "master" worker pool with 4 threads.
    WorkerPool workerPool;
    bool result = workerPool.initialize(4);
    GTS_ASSERT(result);

    // Let's say 2 of the threads will be used for the update loop and 2 will be
    // used for the long running threads. To handle this we partition the pool.

    // To make the partition we give it the indices of the workers that will be
    // the partition. The can be any work but #0, which is the special
    // master(main) thread. Let's give it the last two workers.
    Vector<uint32_t> indicies;
    indicies.push_back(2);
    indicies.push_back(3);
    WorkerPool* pPartitionedWorkerPool = workerPool.makePartition(indicies);
    GTS_ASSERT(pPartitionedWorkerPool != nullptr);

    // NOTE: The "master" workerPool owns pPartitionedWorkerPool. It will destroy
    // the partition when it shuts down.

    // Create a "master" micro scheduler and assign the "master" worker pool to it as before.
    MicroScheduler taskScheduler;
    result = taskScheduler.initialize(&workerPool);
    GTS_ASSERT(result);

    // Now make a partition of the scheduler with the partitioned worker pool.
    // The second parameter controls whether the partition can steal work
    // from its master when it runs out of work. This allows more thread
    // utilization, but may cause a delay for new partition worked to start.
    MicroScheduler* pPartitionedTaskScheduler = taskScheduler.makePartition(pPartitionedWorkerPool, true);
    GTS_ASSERT(pPartitionedTaskScheduler != nullptr);

    // NOTE: The "master" taskScheduler owns pPartitionedTaskScheduler. It will
    // destroy the partition when it shuts down.

    // schedule stuff ...

    taskScheduler.shutdown();
    workerPool.shutdown();
}

} // namespace gts_examples
