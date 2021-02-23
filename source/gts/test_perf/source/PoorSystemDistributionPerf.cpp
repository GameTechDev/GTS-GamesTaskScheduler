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
#include <atomic>
#include <chrono>
#include <thread>

#include "gts_perf/Stats.h"

#include "gts_perf/AOBench.h"
#include "gts_perf/MatMul.h"

#include <gts/micro_scheduler/WorkerPool.h>
#include <gts/micro_scheduler/MicroScheduler.h>
#include <gts/micro_scheduler/patterns/ParallelFor.h>
#include <gts/micro_scheduler/patterns/Range1d.h>

namespace {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
struct alignas(GTS_NO_SHARING_CACHE_LINE_SIZE) RandState
{
    uint32_t state;
    char pad[GTS_NO_SHARING_CACHE_LINE_SIZE - sizeof(uint32_t)];
};

//------------------------------------------------------------------------------
void asyncSubmit(gts::MicroScheduler& taskScheduler, uint32_t taskCount, uint32_t workCount)
{
    GTS_TRACE_SCOPED_ZONE_P0(gts::analysis::CaptureMask::ALL, gts::analysis::Color::Aqua, "Async");

    gts::Task* pRootTask = taskScheduler.allocateTask<gts::EmptyTask>();
    pRootTask->addRef(taskCount + 1);

    for (uint32_t t = 0; t < taskCount; ++t)
    {
        gts::Task* pChildTask = taskScheduler.allocateTask([workCount](gts::TaskContext const&)->gts::Task*
        {
            GTS_TRACE_SCOPED_ZONE_P0(gts::analysis::CaptureMask::MICRO_SCHEDULER_ALL, gts::analysis::Color::RoyalBlue, "Poor Dist Root Task");

            volatile float val = 0.f;
            for (volatile uint32_t ii = 0; ii < workCount; ++ii)
            {
                val = sin(val);
            }
            return nullptr;
        });

        pRootTask->addChildTaskWithoutRef(pChildTask);
        taskScheduler.spawnTask(pChildTask);
    }

    pRootTask->waitForAll();
    taskScheduler.destoryTask(pRootTask);
}

//------------------------------------------------------------------------------
void serialWorker(uint32_t w, uint32_t h, uint32_t nsubsamples)
{
    using namespace ao_bench;

    GTS_TRACE_SCOPED_ZONE_P0(gts::analysis::CaptureMask::ALL, gts::analysis::Color::Aqua, "SerialMaster");
    float* image = new float[w * h * 3];

    Plane plane = { Vec3(0.0f, -0.5f, 0.0f), Vec3(0.f, 1.f, 0.f) };
    Sphere spheres[3] = {
        { Vec3(-2.0f, 0.0f, -3.5f), 0.5f },
        { Vec3(-0.5f, 0.0f, -3.0f), 0.5f },
        { Vec3(1.0f, 0.0f, -2.2f), 0.5f } };

    ao_scanlines(0, w, 0, h, w, h, nsubsamples, image, plane, spheres);

    delete[] image;
}

//------------------------------------------------------------------------------
struct MatMulTaskData
{
    float const* matA;
    float const* matB;
    float* matC;
    float* packedB;
    const uint32_t M;
    const uint32_t N;
    const uint32_t K;
};

//------------------------------------------------------------------------------
void matMulParallel(gts::MicroScheduler& taskScheduler, const uint32_t M, const uint32_t N, const uint32_t K)
{
    GTS_TRACE_SCOPED_ZONE_P0(gts::analysis::CaptureMask::ALL, gts::analysis::Color::Aqua, "Matmul");

    gts::Vector<float, gts::AlignedAllocator<32>> A(M * N);
    matmul::initMatrix(A.data(), M, N, 10.0f);
    gts::Vector<float, gts::AlignedAllocator<32>> B(N * K);
    matmul::initMatrix(B.data(), M, N, 10.0f);
    gts::Vector<float, gts::AlignedAllocator<32>> C(M * K);
    matmul::initMatrix(C.data(), M, K, 0.0f);

    matmul::sgemmParallelBlockDivideAndConquerKd(taskScheduler, A.data(), B.data(), C.data(), M, N, K, 64, 64);
}

//------------------------------------------------------------------------------
void parallelMaster(gts::MicroScheduler& taskScheduler, uint32_t seed)
{
    GTS_TRACE_SCOPED_ZONE_P0(gts::analysis::CaptureMask::ALL, gts::analysis::Color::Aqua, "ParallelMaster");
    RandState myRand;
    myRand.state = seed;

    gts::ThisThread::sleepFor(1);

    for(uint32_t ii = 0; ii < 20; ++ii)
    {
        if(myRand.state % 2 == 0)
        {
            asyncSubmit(taskScheduler, 10 + gts::fastRand(myRand.state) % 50, 1000 * (1 + gts::fastRand(myRand.state) % 9));
            gts::ThisThread::sleepFor(1);
        }
        else
        {
            gts::fastRand(myRand.state);
            matMulParallel(taskScheduler, 128, 128, 128);
            gts::ThisThread::yield();
        }
    }
}

} // namespace


//------------------------------------------------------------------------------
/**
 * Test the scenario where single tasks are submitted from a single thread,
 * which causes worst case load balancing for the scheduler.
 */
Stats poorSystemDistributionPerf(gts::WorkerPool& workerPool, uint32_t iterations)
{
    Stats stats(iterations);

    // Standard rand() is not thread-safe. Use custom per thread rand.
    gts::Vector<RandState, gts::AlignedAllocator<GTS_NO_SHARING_CACHE_LINE_SIZE>> randStatesPerWorker(workerPool.workerCount());
    for (uint32_t ii = 0; ii < workerPool.workerCount(); ++ii)
    {
        randStatesPerWorker[ii].state = ii + 1;
    }

    gts::MicroScheduler taskSchedulerA;
    taskSchedulerA.initialize(&workerPool);

    // Do test.
    for (uint32_t ii = 0; ii < iterations; ++ii)
    {
        GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);

        auto start = std::chrono::high_resolution_clock::now();

        std::thread serialThread([&]()
        {
            serialWorker(64, 64, 2);
        });

        std::thread parallelMasterThread([&]()
        {
            gts::WorkerPool parallelMasterThreadPool;
            parallelMasterThreadPool.initialize(workerPool.workerCount());

            gts::MicroScheduler parallelMasterThreadScheduler;
            parallelMasterThreadScheduler.initialize(&workerPool);

            parallelMaster(parallelMasterThreadScheduler, UINT32_MAX);
        });

        parallelMaster(taskSchedulerA, UINT32_MAX - 1);

        parallelMasterThread.join();
        serialThread.join();

        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double> diff = end - start;
        stats.addDataPoint(diff.count());
    }

    GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);

    return stats;
}
