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

#include "gts_perf/Stats.h"

Stats schedulerOverheadParForPerf(uint32_t size, uint32_t iterations, uint32_t threadCount, bool affinitize);
Stats schedulerOverheadFibPerf(uint32_t fibN, uint32_t iterations, uint32_t threadCount, bool affinitize);
Stats poorDistributionPerf(uint32_t taskCount, uint32_t iterations, uint32_t threadCount, bool affinitize);

Stats mandelbrotPerfSerial(uint32_t dimensions, uint32_t iterations);
Stats mandelbrotPerfParallel(uint32_t dimensions, uint32_t iterations, uint32_t threadCount, bool affinitize);

Stats aoBenchPerfSerial(uint32_t w, uint32_t h, uint32_t nsubsamples, uint32_t iterations);
Stats aoBenchPerfParallel(uint32_t w, uint32_t h, uint32_t nsubsamples, uint32_t iterations, uint32_t threadCount, bool affinitize);

Stats matMulPefSerial(const uint32_t M, const uint32_t N, const uint32_t K, uint32_t iterations);
Stats matMulPefParallel(const uint32_t M, const uint32_t N, const uint32_t K, uint32_t iterations, uint32_t threadCount, bool affinitize);
