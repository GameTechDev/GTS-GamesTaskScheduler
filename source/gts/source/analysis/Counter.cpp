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
#if GTS_ENABLE_COUNTER == 1

#include "gts/analysis/Counter.h"

namespace gts {
namespace analysis {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// WorkerPoolCounters

WorkerPoolCounters::WorkerPoolCounters()
{
    WorkerPoolCounters::m_counterStringByCounter[NUM_ALLOCATIONS]           = "Task Allocations           :";
    WorkerPoolCounters::m_counterStringByCounter[NUM_SLOW_PATH_ALLOCATIONS] = "Task Slow Allocations      :";
    WorkerPoolCounters::m_counterStringByCounter[NUM_MEMORY_RECLAIMS]       = "Task Reclaims              :";

    WorkerPoolCounters::m_counterStringByCounter[NUM_FREES]                 = "Task Frees                 :";
    WorkerPoolCounters::m_counterStringByCounter[NUM_DEFERRED_FREES]        = "Task Deferred Frees        :";

    WorkerPoolCounters::m_counterStringByCounter[NUM_WAKE_CALLS]            = "Wake Workers Calls         :";
    WorkerPoolCounters::m_counterStringByCounter[NUM_WAKE_CHECKS]           = "Wake Workers Checks        :";
    WorkerPoolCounters::m_counterStringByCounter[NUM_WAKE_SUCCESSES]        = "Wake Workers               :";
    WorkerPoolCounters::m_counterStringByCounter[NUM_SLEEP_SUCCESSES]       = "Worker Sleeps              :";

    WorkerPoolCounters::m_counterStringByCounter[NUM_RESUMES]               = "Worker Resumes             :";
    WorkerPoolCounters::m_counterStringByCounter[NUM_HALTS_SIGNALED]        = "Worker Halts Signaled      :";
    WorkerPoolCounters::m_counterStringByCounter[NUM_HALT_SUCCESSES]        = "Worker Halts               :";

    WorkerPoolCounters::m_counterStringByCounter[NUM_SCHEDULER_REGISTERS]   = "MicroScheduler Registers   :";
    WorkerPoolCounters::m_counterStringByCounter[NUM_SCHEDULER_UNREGISTERS] = "MicorScheduler Unregisters :";
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// MicroSchedulerCounters

MicroSchedulerCounters::MicroSchedulerCounters()
{
    MicroSchedulerCounters::m_counterStringByCounter[NUM_SPAWNS]                      = "Task Spawns                    :";
    MicroSchedulerCounters::m_counterStringByCounter[NUM_QUEUES]                      = "Task Queues                    :";

    MicroSchedulerCounters::m_counterStringByCounter[NUM_DEQUE_POP_ATTEMPTS]          = "Deque Pop Attempts             :";
    MicroSchedulerCounters::m_counterStringByCounter[NUM_DEQUE_POP_SUCCESSES]         = "Deque Pops                     :";

    MicroSchedulerCounters::m_counterStringByCounter[NUM_BOOSTED_DEQUE_POP_ATTEMPTS]  = "Boosted Deque Pop Attempts     :";
    MicroSchedulerCounters::m_counterStringByCounter[NUM_BOOSTED_DEQUE_POP_SUCCESSES] = "Boosted Deque Pops             :";

    MicroSchedulerCounters::m_counterStringByCounter[NUM_DEQUE_STEAL_ATTEMPTS]        = "Deque Steal Attempts           :";
    MicroSchedulerCounters::m_counterStringByCounter[NUM_DEQUE_STEAL_SUCCESSES]       = "Deque Steals                   :";

    MicroSchedulerCounters::m_counterStringByCounter[NUM_FAILED_CAS_IN_DEQUE_STEAL]   = "Deque Steals CAS Fails         :";

    MicroSchedulerCounters::m_counterStringByCounter[NUM_EXTERNAL_STEAL_ATTEMPTS]     = "External Steal Attempts        :";
    MicroSchedulerCounters::m_counterStringByCounter[NUM_EXTERNAL_STEAL_SUCCESSES]    = "External Steals                :";

    MicroSchedulerCounters::m_counterStringByCounter[NUM_QUEUE_POP_ATTEMPTS]          = "Queue Pop Attempts             :";
    MicroSchedulerCounters::m_counterStringByCounter[NUM_QUEUE_POP_SUCCESSES]         = "Queue Pops                     :";

    MicroSchedulerCounters::m_counterStringByCounter[NUM_AFFINITY_POP_ATTEMPTS]       = "Affinity Pop Attempts          :";
    MicroSchedulerCounters::m_counterStringByCounter[NUM_AFFINITY_POP_SUCCESSES]      = "Affinity Pops                  :";

    MicroSchedulerCounters::m_counterStringByCounter[NUM_WAITS]                       = "Waits                          :";
    MicroSchedulerCounters::m_counterStringByCounter[NUM_CONTINUATIONS]               = "Continuations                  :";
    MicroSchedulerCounters::m_counterStringByCounter[NUM_EXECUTED_TASKS]              = "Executed Tasks                 :";
    MicroSchedulerCounters::m_counterStringByCounter[NUM_SCHEDULER_BYPASSES]          = "Scheduler Bypasses             :";
    MicroSchedulerCounters::m_counterStringByCounter[NUM_EXIT_ATTEMPTS]               = "Scheduler Exit Attempts        :";
    MicroSchedulerCounters::m_counterStringByCounter[NUM_EXITS]                       = "Scheduler Exits                :";

    MicroSchedulerCounters::m_counterStringByCounter[NUM_SCHEDULER_REGISTERS]         = "External Scheduler Registers   :";
    MicroSchedulerCounters::m_counterStringByCounter[NUM_SCHEDULER_UNREGISTERS]       = "External Scheduler Unregisters :";
}

} // namespace analysis
} // namespace gts

#endif
