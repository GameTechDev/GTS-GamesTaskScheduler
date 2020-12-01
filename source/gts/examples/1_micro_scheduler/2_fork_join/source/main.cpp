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
#include "1_task_basics.h"
#include "2_blocking_join.h"
#include "3_continuation_join.h"
#include "4_scheduler_bypassing.h"
#include "5_task_recycling.h"
#include "6_weakly_dynamic_task_graph_wavefront.h"
#include "7_strongly_dynamic_task_graph_wavefront.h"

using namespace gts_examples;

int main()
{
    taskBasics();
    lambdaTask();

    const int FIB = 40;

    blockingJoin(FIB);
    continuationJoin(FIB);
    schedulerBypassing(FIB);
    taskRecycling(FIB);

    const int DIM = 2048;

    weaklyDynamicTaskGraph_wavefront(DIM, DIM);
    stronglyDynamicTaskGraph_wavefront(DIM, DIM);
    stronglyDynamicTaskGraph_divideAndConquerWavefront(DIM);

    return 0;
}