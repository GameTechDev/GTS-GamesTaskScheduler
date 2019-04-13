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
void schedulerOverheadParFor(Output& output)
{
    uint32_t size = 100000;
    uint32_t iterations = 100;

    output << "=== SchedulerOverhead ParFor ===" << std::endl;
    output << "size: " << size << std::endl;

    output << "--- no affinity ---" << std::endl;

    for (uint32_t iThread = 1; iThread <= gts::Thread::getHardwareThreadCount(); ++iThread)
    {
        Stats stats = schedulerOverheadParForPerf(size, iterations, iThread, false);
        output << stats.mean() << ", ";

        iterations += 5;
    }
    output << std::endl;

    output << "--- affinity ---" << std::endl;

    for (uint32_t iThread = 1; iThread <= gts::Thread::getHardwareThreadCount(); ++iThread)
    {
        Stats stats = schedulerOverheadParForPerf(size, iterations, iThread, true);
        output << stats.mean() << ", ";

        iterations += 5;
    }
    output << std::endl;

    //Stats stats = schedulerOverheadParForPerf(size, iterations + 1000, gts::Thread::getHardwareThreadCount());
    //output << fibN << ", " << stats.mean() << ", " << stats.min() << ", " << stats.max() << ", " << stats.standardDeviation() << std::endl;
}

//------------------------------------------------------------------------------
void schedulerOverheadFib(Output& output)
{
    uint32_t fibN = 30;
    uint32_t iterations = 100;

    output << "=== SchedulerOverhead Fib ===" << std::endl;
    output << "Fib#: " << fibN << std::endl;

    output << "--- no affinity ---" << std::endl;

    for (uint32_t iThread = 1; iThread <= gts::Thread::getHardwareThreadCount(); ++iThread)
    {
        Stats stats = schedulerOverheadFibPerf(fibN, iterations, iThread, false);
        output << stats.mean() << ", ";

        iterations += 5;
    }
    output << std::endl;

    output << "--- affinity ---" << std::endl;

    for (uint32_t iThread = 1; iThread <= gts::Thread::getHardwareThreadCount(); ++iThread)
    {
        Stats stats = schedulerOverheadFibPerf(fibN, iterations, iThread, false);
        output << stats.mean() << ", ";

        iterations += 5;
    }
    output << std::endl;

    //Stats stats = schedulerOverheadFibPerf(fibN, iterations + 1000, gts::Thread::getHardwareThreadCount());
    //output << fibN << ", " << stats.mean() << ", " << stats.min() << ", " << stats.max() << ", " << stats.standardDeviation() << std::endl;
}

//------------------------------------------------------------------------------
void poorDistribution(Output& output)
{
    const uint32_t INIT_ITERS = 100;
    uint32_t iterations = INIT_ITERS;

    output << "=== PoorDistribution ===" << std::endl;

    output << "--- no affinity ---" << std::endl;

    for (uint32_t taskCount = 1000; taskCount <= 5000; taskCount += 1000)
    {
        iterations = INIT_ITERS;
        output << "#tasks: " << taskCount << std::endl;
        for (uint32_t iThread = 1; iThread <= gts::Thread::getHardwareThreadCount(); ++iThread)
        {
            Stats stats = poorDistributionPerf(taskCount, iterations, iThread, false);
            output << stats.mean() << ", ";
            iterations += 10;
        }
        output << std::endl;
    }

    output << "--- affinity ---" << std::endl;

    for (uint32_t taskCount = 1000; taskCount <= 5000; taskCount += 1000)
    {
        iterations = INIT_ITERS;
        output << "#tasks: " << taskCount << std::endl;
        for (uint32_t iThread = 1; iThread <= gts::Thread::getHardwareThreadCount(); ++iThread)
        {
            Stats stats = poorDistributionPerf(taskCount, iterations, iThread, true);
            output << stats.mean() << ", ";
            iterations += 10;
        }
        output << std::endl;
    }

    output << std::endl;
}

//------------------------------------------------------------------------------
void mandelbrot(Output& output)
{
    const uint32_t INIT_ITERS = 100;
    uint32_t iterations = INIT_ITERS;

    output << "=== Mandelbrot ===" << std::endl;

    output << "--- serial ---" << std::endl;

    for (uint32_t dim = 256; dim <= 1024; dim *= 2)
    {
        output << "dim: " << dim << std::endl;
        Stats stats = mandelbrotPerfSerial(dim, iterations);
        output << stats.mean() << std::endl;
    }

    output << "--- parallel ---" << std::endl;
    output << "--- no affinity ---" << std::endl;

    for (uint32_t dim = 256; dim <= 1024; dim *= 2)
    {
        iterations = INIT_ITERS;
        output << "dim: " << dim << std::endl;
        for (uint32_t iThread = 1; iThread <= gts::Thread::getHardwareThreadCount(); ++iThread)
        {
            Stats stats = mandelbrotPerfParallel(dim, iterations, iThread, false);
            output << stats.mean() << ", ";
            iterations += 10;
        }
        output << std::endl;
    }

    output << "--- affinity ---" << std::endl;

    for (uint32_t dim = 256; dim <= 1024; dim *= 2)
    {
        iterations = INIT_ITERS;
        output << "dim: " << dim << std::endl;
        for (uint32_t iThread = 1; iThread <= gts::Thread::getHardwareThreadCount(); ++iThread)
        {
            Stats stats = mandelbrotPerfParallel(dim, iterations, iThread, true);
            output << stats.mean() << ", ";
            iterations += 10;
        }
        output << std::endl;
    }

    output << std::endl;
}

//------------------------------------------------------------------------------
void aoBench(Output& output)
{
    const float INIT_ITERS = 2;
    const uint32_t NSUBSAMPLES = 2;

    float iterations = INIT_ITERS;

    output << "=== AO Bench ===" << std::endl;

    output << "--- serial ---" << std::endl;

    for (uint32_t dim = 256; dim <= 1024; dim *= 2)
    {
        output << "dim: " << dim << std::endl;
        Stats stats = aoBenchPerfSerial(dim, dim, NSUBSAMPLES, (uint32_t)iterations);
        output << stats.mean() << std::endl;
    }

    output << "--- parallel ---" << std::endl;
    output << "--- no affinity ---" << std::endl;

    for (uint32_t dim = 256; dim <= 1024; dim *= 2)
    {
        iterations = INIT_ITERS;
        output << "dim: " << dim << std::endl;
        for (uint32_t iThread = 1; iThread <= gts::Thread::getHardwareThreadCount(); ++iThread)
        {
            Stats stats = aoBenchPerfParallel(dim, dim, NSUBSAMPLES, (uint32_t)iterations, iThread, false);
            output << stats.mean() << ", ";
            iterations += 0.25f;
        }
        output << std::endl;
    }

    output << "--- affinity ---" << std::endl;

    for (uint32_t dim = 256; dim <= 1024; dim *= 2)
    {
        iterations = INIT_ITERS;
        output << "dim: " << dim << std::endl;
        for (uint32_t iThread = 1; iThread <= gts::Thread::getHardwareThreadCount(); ++iThread)
        {
            Stats stats = aoBenchPerfParallel(dim, dim, NSUBSAMPLES, (uint32_t)iterations, iThread, true);
            output << stats.mean() << ", ";
            iterations += 0.25f;
        }
        output << std::endl;
    }

    output << std::endl;
}

//------------------------------------------------------------------------------
void matMul(Output& output)
{
    const uint32_t INIT_ITERS = 100;
    uint32_t iterations = INIT_ITERS;

    output << "=== MatMul ===" << std::endl;

    output << "--- serial ---" << std::endl;

    for (uint32_t dim = 256; dim <= 1024; dim *= 2)
    {
        output << "dim: " << dim << std::endl;
        Stats stats = matMulPefSerial(dim, dim, dim, iterations);
        output << stats.mean() << ", ";
    }

    output << "--- parallel ---" << std::endl;
    output << "--- no affinity ---" << std::endl;

    for (uint32_t dim = 256; dim <= 1024; dim *= 2)
    {
        iterations = INIT_ITERS;
        output << "dim: " << dim << std::endl;
        for (uint32_t iThread = 1; iThread <= gts::Thread::getHardwareThreadCount(); ++iThread)
        {
            Stats stats = matMulPefParallel(dim, dim, dim, iterations, iThread, false);
            output << stats.mean() << ", ";
            iterations += 10;
        }
        output << std::endl;
    }

    output << "--- affinity ---" << std::endl;

    for (uint32_t dim = 256; dim <= 1024; dim *= 2)
    {
        iterations = INIT_ITERS;
        output << "dim: " << dim << std::endl;
        for (uint32_t iThread = 1; iThread <= gts::Thread::getHardwareThreadCount(); ++iThread)
        {
            Stats stats = matMulPefParallel(dim, dim, dim, iterations, iThread, true);
            output << stats.mean() << ", ";
            iterations += 10;
        }
        output << std::endl;
    }

    output << std::endl;
}

//------------------------------------------------------------------------------
int main()
{
    Output output("results.txt");

    schedulerOverheadParFor(output);
    schedulerOverheadFib(output);
    poorDistribution(output);
    mandelbrot(output);
    aoBench(output);
    matMul(output);

    return 0;
}

