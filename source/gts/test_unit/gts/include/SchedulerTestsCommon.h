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

#include <gts/platform/Thread.h>

static const uint32_t ELEMENT_COUNT = 512;
static const uint32_t TILE_SIZE = 8;

static const uint32_t ITERATIONS = 1;
static const uint32_t TEST_DEPTH = 3;
static const uint32_t STRESS_DEPTH = 18;

static const uint32_t ITERATIONS_SERIAL = 2;

#if NDEBUG
static const uint32_t ITERATIONS_CONCUR = 100;
static const uint32_t ITERATIONS_STRESS = 100;
#else
static const uint32_t ITERATIONS_CONCUR = 2;
static const uint32_t ITERATIONS_STRESS = 2;
#endif

inline void randomSleep()
{
    if(rand() % 2 == 0)
    {
        gts::ThisThread::sleepFor(1);
    }
}

#if GTS_USE_GTS_MALLOC
#define GTS_MALLOC_VERIFY_MEM_STORE gts::internal::verifyMemoryStoreIsFreed()
#else
#define GTS_MALLOC_VERIFY_MEM_STORE true
#endif