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
#pragma once

#include "gts/containers/parallel/QueueMPMC.h"
#include "gts/containers/parallel/QueueMPSC.h"

#include "gts/micro_scheduler/Task.h"
#include "WorkStealingDeque_ChaseLev.h"
#include "TerminationBackoff.h"

namespace gts {

class LocalScheduler;
class MicroScheduler;

class TaskDeque : public WorkStealingDeque {};
class TaskQueue : public QueueMPMC<Task*, UnfairSpinMutex<>, AlignedAllocator<GTS_NO_SHARING_CACHE_LINE_SIZE>> {};
class AffinityTaskQueue : public QueueMPSC<Task*, UnfairSpinMutex<>, AlignedAllocator<GTS_NO_SHARING_CACHE_LINE_SIZE>> {};

class TerminationBackoff : public TerminationBackoffAdaptive
{
public:
    explicit GTS_INLINE TerminationBackoff(uint32_t minFailThreshold) : TerminationBackoffAdaptive(minFailThreshold) {}
};

using PriorityTaskDeque         = Vector<TaskDeque, AlignedAllocator<GTS_NO_SHARING_CACHE_LINE_SIZE>>;
using PriorityAffinityTaskQueue = Vector<AffinityTaskQueue, AlignedAllocator<GTS_NO_SHARING_CACHE_LINE_SIZE>>;
using PriorityTaskQueue         = Vector<TaskQueue, AlignedAllocator<GTS_NO_SHARING_CACHE_LINE_SIZE>>;

class ScheduleVector : public Vector<LocalScheduler, AlignedAllocator<GTS_NO_SHARING_CACHE_LINE_SIZE>> {};
class MicroSchedulerVector : public Vector<MicroScheduler, AlignedAllocator<GTS_NO_SHARING_CACHE_LINE_SIZE>> {};

} // namespace gts
