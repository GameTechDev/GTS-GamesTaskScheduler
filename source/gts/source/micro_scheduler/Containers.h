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

#include "gts/containers/parallel/DistributedSlabAllocatorBucketed.h"

#include "gts/micro_scheduler/Task.h"
#include "WorkStealingDeque.h"
#include "AffinityQueue.h"
#include "WorkSharingQueue.h"

namespace gts {

class Schedule;
class MicroScheduler;

class TaskDeque : public WorkStealingDeque<gts::Vector, gts::AlignedAllocator<Task*, GTS_NO_SHARING_CACHE_LINE_SIZE>> {};
class TaskQueue : public gts::WorkSharingQueue<Task*, gts::Vector, gts::AlignedAllocator<Task*, GTS_NO_SHARING_CACHE_LINE_SIZE>> {};
class AffinityTaskQueue : public gts::AffinityQueue<Task*, gts::Vector, gts::AlignedAllocator<Task*, GTS_NO_SHARING_CACHE_LINE_SIZE>> {};

class TaskAllocator : public gts::DistributedSlabAllocatorBucketed
{
public:
    explicit GTS_INLINE TaskAllocator(uint32_t accessorThreadCount)
    {
        bool result = init(accessorThreadCount, GTS_NO_SHARING_CACHE_LINE_SIZE);
        GTS_ASSERT(result);
    }
};

using AlignedAllocTaskAllocator = gts::AlignedAllocator<TaskDeque, GTS_NO_SHARING_CACHE_LINE_SIZE>;
using PriorityTaskDeque = gts::Vector<TaskDeque, AlignedAllocTaskAllocator>;

class ScheduleVector : public gts::Vector<Schedule, gts::AlignedAllocator<Schedule, GTS_NO_SHARING_CACHE_LINE_SIZE>> {};
class MicroSchedulerVector : public gts::Vector<MicroScheduler, gts::AlignedAllocator<MicroScheduler, GTS_NO_SHARING_CACHE_LINE_SIZE>> {};

} // namespace gts
