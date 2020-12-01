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

#include <gts/micro_scheduler/WorkerPool.h>
#include <gts/micro_scheduler/MicroScheduler.h>

#include "gts_perf/Stats.h"

Stats spawnTaskOverheadWithoutAllocPerf(gts::MicroScheduler& taskScheduler, uint32_t iterations);
Stats spawnTaskOverheadWithAllocCachingPerf(gts::MicroScheduler& taskScheduler, uint32_t iterations);
Stats spawnTaskOverheadWithAllocPerf(gts::MicroScheduler& taskScheduler, uint32_t iterations);

Stats schedulerOverheadParForPerf(gts::MicroScheduler& taskScheduler, uint32_t size, uint32_t iterations);
Stats schedulerOverheadFibPerf(gts::MicroScheduler& taskScheduler, uint32_t fibN, uint32_t iterations);
Stats poorDistributionPerf(gts::MicroScheduler& taskScheduler, uint32_t taskCount, uint32_t iterations);
Stats poorSystemDistributionPerf(gts::WorkerPool& workerPool, uint32_t iterations);

Stats mandelbrotPerfSerial(uint32_t dimensions, uint32_t iterations);
Stats mandelbrotPerfParallel(gts::MicroScheduler& taskScheduler, uint32_t dimensions, uint32_t iterations);

Stats aoBenchPerfSerial(uint32_t w, uint32_t h, uint32_t nsubsamples, uint32_t iterations);
Stats aoBenchPerfParallel(gts::MicroScheduler& taskScheduler, uint32_t w, uint32_t h, uint32_t nsubsamples, uint32_t iterations);

Stats matMulPefSerial(const size_t M, const size_t N, const size_t K, size_t iterations);
Stats matMulPefParallel(gts::MicroScheduler& taskScheduler, const size_t M, const size_t N, const size_t K, size_t iterations);

Stats mpmcQueuePerfSerial(const uint32_t itemCount, uint32_t iterations);
Stats mpmcQueuePerfParallel(const uint32_t threadCount, const uint32_t itemCount, uint32_t iterations);

Stats homoRandomDagWorkStealing(uint32_t iterations);
Stats heteroRandomDagWorkStealing(uint32_t iterations, bool bidirectionalStealing);
Stats heteroRandomDagCriticallyAware(uint32_t iterations);
