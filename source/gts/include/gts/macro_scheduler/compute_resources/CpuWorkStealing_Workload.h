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

#include "gts/macro_scheduler/Workload.h"

namespace gts {

class IWorkQueue;
class Task;
class MicroScheduler;
struct TaskContext;

using CpuWorkStealingComputeRoutine = void (*)(void* pUserData);

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 */
class WorkStealingTask
{
public:

    WorkStealingTask(
        CpuWorkStealingComputeRoutine cpuComputeRoutine,
        void* pUserData = nullptr);

    static Task* execute(Task* pThisTask, TaskContext const& taskContext);

private:

    CpuWorkStealingComputeRoutine m_cpuComputeRoutine;
    void* m_pUserData;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 */
class CpuWorkStealing_Workload : public Workload
{
public:

    CpuWorkStealing_Workload(
        CpuWorkStealingComputeRoutine cpuComputeRoutine,
        void* pUserData);

    WorkStealingTask* getWork();

    static Task* buildTask(Node* pNode, MicroScheduler* pMicroScheduler);

    static ComputeResourceType type() { return ComputeResourceType::CPU_WorkStealing; }

private:

    WorkStealingTask m_workStealingTask;
};

} // namespace gts
