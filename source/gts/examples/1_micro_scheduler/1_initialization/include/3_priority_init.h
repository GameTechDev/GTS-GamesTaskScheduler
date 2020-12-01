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

void priorityInit()
{
    printf ("================\n");
    printf ("priorityInit\n");
    printf ("================\n");

    // The GTS micro-schedule implements task priorities, yet it does not
    // context switch when a higher priority task is spawned. To achieve
    // context switching behavior, one needs to use the OS's preemptive scheduler.

    // Let's say we need two priorities hi and low.
    constexpr uint32_t PRIORITY_COUNT = 2;
    WorkerPool workerPool[PRIORITY_COUNT];
    MicroScheduler microScheduler[PRIORITY_COUNT];

    // For each priority, build a pool with a different priority and attach
    // a scheduler to it.
    for (uint32_t iPriority = 0; iPriority < PRIORITY_COUNT; ++iPriority)
    {
        WorkerPoolDesc workerPoolDesc;

        for (uint32_t iWorker = 0; iWorker < Thread::getHardwareThreadCount(); iWorker++)
        {
            WorkerThreadDesc workerDesc;
            workerDesc.priority = (Thread::Priority)((uint32_t)Thread::Priority::PRIORITY_NORMAL + iPriority);
            workerPoolDesc.workerDescs.push_back(workerDesc);
        }

        workerPool[iPriority].initialize(workerPoolDesc);
        microScheduler[iPriority].initialize(&workerPool[iPriority]);
    }

    // Task submitted from microScheduler[1] running on threads from
    // workerPool[1] will preempt any running tasks from microScheduler[0].
}

} // namespace gts_examples
