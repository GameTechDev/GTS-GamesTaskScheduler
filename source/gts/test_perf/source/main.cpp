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

#include "gts_perf/Stats.h"
#include "gts_perf/Output.h"
#include "gts_perf/PerfTests.h"

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Tests:

//------------------------------------------------------------------------------
void mandelbrot()
{
    uint32_t iterations = 50;

    Output output("mandelbrot.txt");

    output << "=== Mandelbrot ===" << std::endl;

    output << "--- serial ---" << std::endl;
    output << "Dimensions, " << "Mean Time, " << "Min, " << "Max, " << "Std Dev" << std::endl;

    for (uint32_t dim = 256; dim <= 1024; dim *= 2)
    {
        Stats stats = mandelbrotPerfSerial(dim, iterations);
        output << dim << ", " << stats.mean() << ", " << stats.min() << ", " << stats.max() << ", " << stats.standardDeviation() << std::endl;
    }

    output << "--- parallel ---" << std::endl;

    for (uint32_t iThread = 1; iThread <= gts::Thread::getHardwareThreadCount(); ++iThread)
    {
        output << "Dimensions, " << "Mean Time, " << "Min, " << "Max, " << "Std Dev" << "Thread Count" << std::endl;
        for (uint32_t dim = 256; dim <= 1024; dim *= 2)
        {
            Stats stats = mandelbrotPerfParallel(dim, iterations, iThread);
            output << dim << ", " << stats.mean() << ", " << stats.min() << ", " << stats.max() << ", " << stats.standardDeviation() << ", " << iThread << std::endl;
        }
        iterations += 10;
    }
}

//------------------------------------------------------------------------------
void poorDistribution()
{
    uint32_t iterations = 50;

    Output output("poorDistribution.txt");

    output << "--- PoorDistribution ---" << std::endl;

    for (uint32_t iThread = 1; iThread <= gts::Thread::getHardwareThreadCount(); ++iThread)
    {
        output << "Data Count, " << "Mean Time, " << "Min, " << "Max, " << "Thread Count" << std::endl;
        for (uint32_t taskCount = 1000; taskCount <= 5000; taskCount += 1000)
        {
            Stats stats = poorDistributionPerf(taskCount, iterations, iThread);
            output << taskCount << ", " << stats.mean() << ", " << stats.min() << ", " << stats.max() << ", " << stats.standardDeviation() << ", " << iThread << std::endl;
        }
        iterations += 10;
    }
}

//------------------------------------------------------------------------------
void schedulerOverhead()
{
    uint32_t fibN = 30;
    uint32_t iterations = 20;

    Output output("schedulerOverhead.txt");

    output << "--- SchedulerOverhead ---" << std::endl;

    for (uint32_t iThread = 1; iThread <= gts::Thread::getHardwareThreadCount(); ++iThread)
    {
        output << "Fib #, " << "Mean Time, " << "Min, " << "Max, " << "Thread Count" << std::endl;
        Stats stats = schedulerOverheadPerf(fibN, iterations, iThread);
        output << fibN << ", " << stats.mean() << ", " << stats.min() << ", " << stats.max() << ", " << stats.standardDeviation() << ", " << iThread << std::endl;

        iterations += 5;
    }

    //Stats stats = schedulerOverheadPerf(fibN, iterations + 1000, gts::Thread::getHardwareThreadCount());
    //output << fibN << ", " << stats.mean() << ", " << stats.min() << ", " << stats.max() << ", " << stats.standardDeviation() << ", " << 20 << std::endl;
}

//------------------------------------------------------------------------------
void aoBench()
{
    float iterations = 2;
    const uint32_t NSUBSAMPLES = 2;

    Output output("aoBench.txt");

    output << "=== AO Bench ===" << std::endl;

    output << "--- serial ---" << std::endl;
    output << "Data Count, " << "Mean Time, " << "Min, " << "Max, " << "Std Dev" << std::endl;

    for (uint32_t dim = 256; dim <= 1024; dim *= 2)
    {
        Stats stats = aoBenchPerfSerial(dim, dim, NSUBSAMPLES, (uint32_t)iterations);
        output << dim << ", " << stats.mean() << ", " << stats.min() << ", " << stats.max() << ", " << stats.standardDeviation() << std::endl;
    }

    output << "--- parallel ---" << std::endl;

    for (uint32_t iThread = 1; iThread <= gts::Thread::getHardwareThreadCount(); ++iThread)
    {
        output << "Data Count, " << "Mean Time, " << "Min, " << "Max, " << "Std Dev" << "Thread Count" << std::endl;
        for (uint32_t dim = 256; dim <= 1024; dim *= 2)
        {
            Stats stats = aoBenchPerfParallel(dim, dim, NSUBSAMPLES, (uint32_t)iterations, iThread);
            output << dim << ", " << stats.mean() << ", " << stats.min() << ", " << stats.max() << ", " << stats.standardDeviation() << ", " << iThread << std::endl;
        }

        iterations += 0.25f;
    }
}

//------------------------------------------------------------------------------
void matMul()
{
    uint32_t iterations = 50;

    Output output("matMul.txt");

    output << "=== MatMul ===" << std::endl;

    output << "--- serial ---" << std::endl;
    output << "Data Count, " << "Mean Time, " << "Min, " << "Max, " << "Std Dev" << std::endl;

    for (uint32_t dim = 256; dim <= 1024; dim *= 2)
    {
        Stats stats = matMulPefSerial(dim, dim, dim, iterations);
        output << dim << ", " << stats.mean() << ", " << stats.min() << ", " << stats.max() << ", " << stats.standardDeviation() << std::endl;
    }

    output << "--- parallel ---" << std::endl;

    for (uint32_t iThread = 1; iThread <= gts::Thread::getHardwareThreadCount(); ++iThread)
    {
        output << "Data Count, " << "Mean Time, " << "Min, " << "Max, " << "Std Dev" << "Thread Count" << std::endl;
        for (uint32_t dim = 256; dim <= 1024; dim *= 2)
        {
            Stats stats = matMulPefParallel(dim, dim, dim, iterations, iThread);
            output << dim << ", " << stats.mean() << ", " << stats.min() << ", " << stats.max() << ", " << stats.standardDeviation() << ", " << iThread << std::endl;
        }

        iterations += 10;
    }
}

//------------------------------------------------------------------------------
int main()
{
    mandelbrot();
    poorDistribution();
    schedulerOverhead();
    aoBench();
    matMul();

    return 0;
}

