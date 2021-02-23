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
#include <gts/platform/Thread.h>

#include "gts_perf/Stats.h"
#include "gts_perf/Output.h"
#include "gts_perf/PerfTests.h"

#define SANDBOX

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Tests:

//------------------------------------------------------------------------------
void initWorkerPool(gts::WorkerPool& workerPool, uint32_t threadCount, bool affinitize)
{
    if (affinitize)
    {
        gts::WorkerPoolDesc workerPoolDesc;

        // Get the CPU topology.
        gts::SystemTopology sysTopo;
        gts::Thread::getSystemTopology(sysTopo);

        // Loop through each processor group.
        for (size_t iGroup = 0; iGroup < sysTopo.groupInfoElementCount; ++iGroup)
        {
            gts::ProcessorGroupInfo const& groupInfo = sysTopo.pGroupInfoArray[iGroup];

            // Loop through each physical core.
            for (size_t iCore = 0; iCore < groupInfo.coreInfoElementCount; ++iCore)
            {
                gts::CpuCoreInfo const& coreInfo = groupInfo.pCoreInfoArray[iCore];

                // Loop through each thread.
                for (size_t iThead = 0; iThead < coreInfo.hardwareThreadIdCount; ++iThead)
                {
                    gts::WorkerThreadDesc workerDesc;

                    // Set the affinity mask.
                    gts::AffinitySet affinitySet;
                    affinitySet.set(coreInfo.pHardwareThreadIds[iThead]);
                    workerDesc.affinity = {iGroup, affinitySet };

                    // Add the worker desc to the pool desc.
                    workerPoolDesc.workerDescs.push_back(workerDesc);
                }
            }
        }

        // Affinitize the Master thread here since the Worker Pool does not own it.
        gts::AffinitySet affinitySet;
        affinitySet.set(sysTopo.pGroupInfoArray[0].pCoreInfoArray[0].pHardwareThreadIds[0]);
        gts::ThisThread::setAffinity(0, affinitySet);

        workerPool.initialize(workerPoolDesc);
    }
    else
    {
        workerPool.initialize(threadCount);
    }
}

//------------------------------------------------------------------------------
void spawnTaskOverhead(Output& output,
    uint32_t iterations = 100000)
{
    output << "=== Spawn Task Overhead Without Alloc (cycles) ===" << std::endl;
    output << "iterations: " << iterations << std::endl;
    {
        gts::WorkerPool workerPool;
        initWorkerPool(workerPool, 1, false);

        gts::MicroScheduler taskScheduler;
        taskScheduler.initialize(&workerPool);

        Stats stats = spawnTaskOverheadWithoutAllocPerf(taskScheduler, iterations);
        output << stats.mean() << ", ";

        output << std::endl;
    }
    
    output << "=== Spawn Task Overhead With Alloc Caching (cycles) ===" << std::endl;
    output << "iterations: " << iterations << std::endl;
    {
        gts::WorkerPool workerPool;
        initWorkerPool(workerPool, 1, false);

        gts::MicroScheduler taskScheduler;
        taskScheduler.initialize(&workerPool);

        Stats stats = spawnTaskOverheadWithAllocCachingPerf(taskScheduler, iterations);
        output << stats.mean() << ", ";

        output << std::endl;
    }

    output << "=== Spawn Task Overhead With Alloc No Caching (cycles) ===" << std::endl;
    output << "iterations: " << iterations << std::endl;
    {
        gts::WorkerPool workerPool;
        initWorkerPool(workerPool, 1, false);

        gts::MicroScheduler taskScheduler;
        taskScheduler.initialize(&workerPool);

        Stats stats = spawnTaskOverheadWithAllocPerf(taskScheduler, iterations);
        output << stats.mean() << ", ";

        output << std::endl;
    }
}

//------------------------------------------------------------------------------
void schedulerOverheadParFor(Output& output, uint32_t startThreadCount, uint32_t endThreadCount,
    uint32_t size = 1000000,
    uint32_t iterations = 100000)
{
    output << "=== Scheduler Overhead ParFor (s) ===" << std::endl;
    output << "size: " << size << std::endl;
    output << "iterations: " << iterations << std::endl;

    for (uint32_t iThread = startThreadCount; iThread <= endThreadCount; ++iThread)
    {
        gts::WorkerPool workerPool;
        initWorkerPool(workerPool, iThread, false);

        gts::MicroScheduler taskScheduler;
        taskScheduler.initialize(&workerPool);

        Stats stats = schedulerOverheadParForPerf(taskScheduler, size, iterations);
        output << stats.mean() << ", ";
    }

    output << std::endl;
}

//------------------------------------------------------------------------------
void irregularRandParFor(Output& output, uint32_t startThreadCount, uint32_t endThreadCount,
    uint32_t size = 10000,
    uint32_t iterations = 100000)
{
    output << "=== Irregular Random ParFor (s) ===" << std::endl;
    output << "size: " << size << std::endl;
    output << "iterations: " << iterations << std::endl;

    for (uint32_t iThread = startThreadCount; iThread <= endThreadCount; ++iThread)
    {
        gts::WorkerPool workerPool;
        initWorkerPool(workerPool, iThread, false);

        gts::MicroScheduler taskScheduler;
        taskScheduler.initialize(&workerPool);

        Stats stats = irregularRandParallelFor(taskScheduler, size, iterations);
        output << stats.mean() << ", ";
    }

    output << std::endl;
}

//------------------------------------------------------------------------------
void irregularUpfrontParFor(Output& output, uint32_t startThreadCount, uint32_t endThreadCount,
    uint32_t size = 10000,
    uint32_t iterations = 100000)
{
    output << "=== Irregular Random ParFor (s) ===" << std::endl;
    output << "size: " << size << std::endl;
    output << "iterations: " << iterations << std::endl;

    for (uint32_t iThread = startThreadCount; iThread <= endThreadCount; ++iThread)
    {
        gts::WorkerPool workerPool;
        initWorkerPool(workerPool, iThread, false);

        gts::MicroScheduler taskScheduler;
        taskScheduler.initialize(&workerPool);

        Stats stats = irregularUpfrontParallelFor(taskScheduler, size, iterations);
        output << stats.mean() << ", ";
    }

    output << std::endl;
}

//------------------------------------------------------------------------------
void schedulerOverheadFib(Output& output, uint32_t startThreadCount, uint32_t endThreadCount,
    uint32_t fibN = 30,
    uint32_t iterations = 100)
{
    output << "=== Scheduler Overhead Fib (s) ===" << std::endl;
    output << "Fib#: " << fibN << std::endl;
    output << "iterations: " << iterations << std::endl;

    for (uint32_t iThread = startThreadCount; iThread <= endThreadCount; ++iThread)
    {
        gts::WorkerPool workerPool;
        initWorkerPool(workerPool, iThread, false);

        gts::MicroScheduler taskScheduler;
        taskScheduler.initialize(&workerPool);

        Stats stats = schedulerOverheadFibPerf(taskScheduler, fibN, iterations);
        output << stats.mean() << ", ";
    }

    output << std::endl;
}

//------------------------------------------------------------------------------
void poorDistribution(Output& output, uint32_t startThreadCount, uint32_t endThreadCount,
    uint32_t tasks = 5000,
    uint32_t iterations = 30)
{
    output << "=== Poor Distribution ===" << std::endl;
    output << "tasks: " << tasks << std::endl;
    output << "iterations: " << iterations << std::endl;

    for (uint32_t iThread = startThreadCount; iThread <= endThreadCount; ++iThread)
    {
        gts::WorkerPool workerPool;
        initWorkerPool(workerPool, iThread, false);

        gts::MicroScheduler taskScheduler;
        taskScheduler.initialize(&workerPool);

        Stats stats = poorDistributionPerf(taskScheduler, tasks, iterations);
        output << stats.mean() << ", ";
    }

    output << std::endl;
}

//------------------------------------------------------------------------------
void poorSystemDistribution(Output& output, uint32_t startThreadCount, uint32_t endThreadCount,
    uint32_t = 0,
    uint32_t iterations = 40)
{
    output << "=== Poor System Distribution (s) ===" << std::endl;
    output << "iterations: " << iterations << std::endl;

    for (uint32_t iThread = startThreadCount; iThread <= endThreadCount; ++iThread)
    {
        gts::WorkerPool workerPool;
        initWorkerPool(workerPool, iThread, false);

        gts::MicroScheduler taskScheduler;
        taskScheduler.initialize(&workerPool);

        Stats stats = poorSystemDistributionPerf(workerPool, iterations);
        output << stats.mean() << ", ";
    }

    output << std::endl;
}

//------------------------------------------------------------------------------
void mandelbrot(Output& output, uint32_t startThreadCount, uint32_t endThreadCount,
    uint32_t dimensions = 512,
    uint32_t iterations = 100,
    bool serial = false)
{
    output << "=== Mandelbrot (s) ===" << std::endl;
    output << "dimensions: " << dimensions << std::endl;
    output << "iterations: " << iterations << std::endl;

    if(serial)
    {
        output << "--- serial ---" << std::endl;
        Stats stats = mandelbrotPerfSerial(dimensions, iterations);
        output << stats.mean() << std::endl;
    }
    else
    {
        output << "--- parallel ---" << std::endl;

        for (uint32_t iThread = startThreadCount; iThread <= endThreadCount; ++iThread)
        {
            gts::WorkerPool workerPool;
            initWorkerPool(workerPool, iThread, false);

            gts::MicroScheduler taskScheduler;
            taskScheduler.initialize(&workerPool);

            Stats stats = mandelbrotPerfParallel(taskScheduler, dimensions, iterations);
            output << stats.mean() << ", ";
        }
    }
    output << std::endl;
}

//------------------------------------------------------------------------------
void aoBench(Output& output, uint32_t startThreadCount, uint32_t endThreadCount,
    uint32_t dimensions = 512,
    uint32_t iterations = 2,
    bool serial = false)
{
    const uint32_t subSamples = 2;

    output << "=== AO Bench (s) ===" << std::endl;
    output << "dimensions : " << dimensions << std::endl;
    output << "#subsamples: " << subSamples << std::endl;
    output << "iterations : " << iterations << std::endl;

    if(serial)
    {
        output << "--- serial ---" << std::endl;
        Stats stats = aoBenchPerfSerial(dimensions, dimensions, subSamples, iterations);
        output << stats.mean() << std::endl;
    }
    else
    {
        output << "--- parallel ---" << std::endl;
        for (uint32_t iThread = startThreadCount; iThread <= endThreadCount; ++iThread)
        {
            gts::WorkerPool workerPool;
            initWorkerPool(workerPool, iThread, false);

            gts::MicroScheduler taskScheduler;
            taskScheduler.initialize(&workerPool);

            Stats stats = aoBenchPerfParallel(taskScheduler, dimensions, dimensions, subSamples, iterations);
            output << stats.mean() << ", ";
        }
    }
    output << std::endl;
}

//------------------------------------------------------------------------------
void matMul(Output& output, uint32_t startThreadCount, uint32_t endThreadCount,
    uint32_t dimensions = 1024,
    uint32_t iterations = 100,
    bool serial = false,
    bool affinitize = false)
{
    output << "=== Matrix Mul (s) ===" << std::endl;
    output << "dimensions : " << dimensions << std::endl;
    output << "iterations : " << iterations << std::endl;

    if(serial)
    {
        output << "--- serial ---" << std::endl;
        Stats stats = matMulPefSerial(dimensions, dimensions, dimensions, iterations);
        output << stats.mean() << std::endl;
    }
    else
    {
        output << "--- parallel ---" << std::endl;
        for (uint32_t iThread = startThreadCount; iThread <= endThreadCount; ++iThread)
        {
            gts::WorkerPool workerPool;
            initWorkerPool(workerPool, iThread, affinitize);

            gts::MicroScheduler taskScheduler;
            taskScheduler.initialize(&workerPool);

            Stats stats = matMulPefParallel(taskScheduler, dimensions, dimensions, dimensions, iterations);
            output << stats.mean() << ", ";
        }
    }
    output << std::endl;
}

//------------------------------------------------------------------------------
void mpmcQueue(Output& output, uint32_t startThreadCount, uint32_t endThreadCount,
    uint32_t numElements = 1024,
    uint32_t iterations = 100,
    bool serial = false)
{
    output << "=== MPMC Queue push/pop (s) ===" << std::endl;
    output << "numElements : " << numElements << std::endl;
    output << "iterations : " << iterations << std::endl;

    startThreadCount = gts::gtsMax(startThreadCount, 2u);
    endThreadCount = gts::gtsMax(startThreadCount, endThreadCount);

    if(serial)
    {
        output << "--- serial ---" << std::endl;
        Stats stats = mpmcQueuePerfSerial(numElements, iterations);
        output << stats.mean() << std::endl;
    }
    else
    {
        output << "--- parallel ---" << std::endl;
        for (uint32_t iThread = startThreadCount; iThread <= endThreadCount; iThread += 2)
        {
            Stats stats = mpmcQueuePerfParallel(iThread, numElements, iterations);
            output << stats.mean() << ", ";
        }
    }
    output << std::endl;
}

//------------------------------------------------------------------------------
void homoRandomDagWorkStealing(Output& output, uint32_t iterations = 100)
{
    output << "=== Homogeneous Random DAG Work-stealing (s) ===" << std::endl;
    output << "threads : " << 16 << std::endl;
    output << "iterations : " << iterations << std::endl;

    GTS_ASSERT(gts::Thread::getHardwareThreadCount() >= 16 && "Machine must have at least 16 cores.");

    Stats stats = homoRandomDagWorkStealing(iterations);
    output << stats.mean() << ", ";
    output << std::endl;
}

//------------------------------------------------------------------------------
void heteroRandomDagWorkStealing(Output& output, uint32_t iterations = 100)
{
    output << "=== Heterogeneous Random DAG Work-stealing (s) ===" << std::endl;
    output << "threads : " << 16 << std::endl;
    output << "iterations : " << iterations << std::endl;

    GTS_ASSERT(gts::Thread::getHardwareThreadCount() >= 16 && "Machine must have at least 16 cores.");

    Stats stats = heteroRandomDagWorkStealing(iterations, false);
    output << stats.mean() << ", ";
    output << std::endl;
}

//------------------------------------------------------------------------------
void heteroRandomDagCriticallyAware(Output& output, uint32_t iterations = 100)
{
    output << "=== Heterogeneous Random DAG Critically Aware (s) ===" << std::endl;
    output << "threads : " << 16 << std::endl;
    output << "iterations : " << iterations << std::endl;

    GTS_ASSERT(gts::Thread::getHardwareThreadCount() >= 16 && "Machine must have at least 16 cores.");

    Stats stats = heteroRandomDagCriticallyAware(iterations);
    output << stats.mean() << ", ";
    output << std::endl;
}

constexpr char* TEST_TYPE_SPAWN_TASK        = "spawn_task";
constexpr char* TEST_TYPE_OVERHEAD          = "empty_for";
constexpr char* TEST_TYPE_FIBONACCI         = "fibonacci";
constexpr char* TEST_TYPE_POOR_DIST         = "poor_dist";
constexpr char* TEST_TYPE_POOR_SYS_DIST     = "poor_sys_dist";
constexpr char* TEST_TYPE_MANDELBROT        = "mandelbrot";
constexpr char* TEST_TYPE_AO_BENCH          = "ao_bench";
constexpr char* TEST_TYPE_MAT_MUL           = "mat_mul";
constexpr char* TEST_TYPE_MPMC_QUEUE        = "mpmc_queue";

//------------------------------------------------------------------------------
void runTests(
    Output& output,
    std::string const& testType,
    uint32_t testSize,
    uint32_t testIterations,
    uint32_t startThreadCount = 1,
    uint32_t endThreadCount = gts::Thread::getHardwareThreadCount())
{
    if(TEST_TYPE_SPAWN_TASK == testType)
    {
        spawnTaskOverhead(output, testIterations);
    }
    else if(TEST_TYPE_OVERHEAD == testType)
    {
        schedulerOverheadParFor(output, startThreadCount, endThreadCount, testSize, testIterations);
    }
    else if(TEST_TYPE_FIBONACCI == testType)
    {
        schedulerOverheadFib(output, startThreadCount, endThreadCount, testSize, testIterations);
    }
    else if(TEST_TYPE_POOR_DIST == testType)
    {
        poorDistribution(output, startThreadCount, endThreadCount, testSize, testIterations);
    }
    else if(TEST_TYPE_POOR_SYS_DIST == testType)
    {
        poorSystemDistribution(output, startThreadCount, endThreadCount, testSize, testIterations);
    }
    else if(TEST_TYPE_MANDELBROT == testType)
    {
        mandelbrot(output, startThreadCount, endThreadCount, testSize, testIterations);
    }
    else if(TEST_TYPE_AO_BENCH == testType)
    {
        aoBench(output, startThreadCount, endThreadCount, testSize, testIterations);
    }
    else if(TEST_TYPE_MAT_MUL == testType)
    {
        matMul(output, startThreadCount, endThreadCount, testSize, testIterations);
    }
    else if(TEST_TYPE_MPMC_QUEUE == testType)
    {
        mpmcQueue(output, startThreadCount, endThreadCount, testSize, testIterations);
    }
}

//------------------------------------------------------------------------------
void printArgRequirements()
{
    std::cout << "\nRequired Args:\n[spawn_task|empty_for|fibonacci|poor_dist|poor_sys_dist|mandelbrot|ao_bench|mat_mul] [size] [iterations] [startThreadCount] [endThreadCount]\n\n";
}

//------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    gts::analysis::CaptureMask::Type captureMask = gts::analysis::CaptureMask::Type(
        gts::analysis::CaptureMask::MICRO_SCHEDULER_PROFILE |
        gts::analysis::CaptureMask::WORKERPOOL_PROFILE | 
        gts::analysis::CaptureMask::THREAD_PROFILE |
        gts::analysis::CaptureMask::USER);

    GTS_TRACE_SET_CAPTURE_MASK(captureMask);

    Output output("results.txt");

#ifndef SANDBOX

    if (argc > 3)
    {
        std::string testType    = argv[1];
        uint32_t testSize       = atoi(argv[2]);
        uint32_t testIterations = atoi(argv[3]);

        uint32_t startThreadCount = 0;
        uint32_t endThreadCount   = 0;

        if (argc > 4)
        {
            startThreadCount = atoi(argv[4]);
            endThreadCount   = atoi(argv[5]);
        }

        if(startThreadCount == 0)
        {
            startThreadCount = 1;
        }
        if(endThreadCount == 0)
        {
            endThreadCount = gts::Thread::getHardwareThreadCount();
        }

        runTests(output, testType, testSize, testIterations, startThreadCount, endThreadCount);
    }
    else
    {
        printArgRequirements();

        uint32_t startThreadCount = 1;
        uint32_t endThreadCount = gts::Thread::getHardwareThreadCount();

        spawnTaskOverhead(output);
        schedulerOverheadParFor(output, startThreadCount, endThreadCount);
        schedulerOverheadFib(output, startThreadCount, endThreadCount);
        poorDistribution(output, startThreadCount, endThreadCount);
        poorSystemDistribution(output, startThreadCount, endThreadCount);
        mandelbrot(output, startThreadCount, endThreadCount);
        aoBench(output, startThreadCount, endThreadCount);
        matMul(output, startThreadCount, endThreadCount);
        mpmcQueue(output, startThreadCount, endThreadCount);
    }

#else

    GTS_UNREFERENCED_PARAM(argc);
    GTS_UNREFERENCED_PARAM(argv);

    //spawnTaskOverhead(output);
    //schedulerOverheadParFor(output, gts::Thread::getHardwareThreadCount(), gts::Thread::getHardwareThreadCount(), 10000000, 1000);
    //mandelbrot(output, gts::Thread::getHardwareThreadCount(), gts::Thread::getHardwareThreadCount(), 2048, 50);
    poorSystemDistribution(output, gts::Thread::getHardwareThreadCount(), gts::Thread::getHardwareThreadCount(), 0, 100);

    //matMul(output, gts::Thread::getHardwareThreadCount(), gts::Thread::getHardwareThreadCount(), 2048, 100, false, false);

    //aoBench(output, gts::Thread::getHardwareThreadCount(), gts::Thread::getHardwareThreadCount(), 1024, 10);

    //mpmcQueue(output, gts::Thread::getHardwareThreadCount(), gts::Thread::getHardwareThreadCount(), 100000, 100);

    //irregularRandParFor(output, gts::Thread::getHardwareThreadCount(), gts::Thread::getHardwareThreadCount(), 10000, 100);
    //irregularUpfrontParFor(output, gts::Thread::getHardwareThreadCount(), gts::Thread::getHardwareThreadCount(), 10000, 100);

    //homoRandomDagWorkStealing(output, 10);
    //heteroRandomDagCriticallyAware(output, 10);

#endif

    return 0;
}

