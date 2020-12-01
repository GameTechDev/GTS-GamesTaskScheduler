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
#include <chrono>

#include "gts_perf/MatMul.h"

#include "gts_perf/Stats.h"

#include <gts/micro_scheduler/WorkerPool.h>
#include <gts/micro_scheduler/MicroScheduler.h>
#include <gts/micro_scheduler/patterns/ParallelFor.h>
#include <gts/micro_scheduler/patterns/ParallelReduce.h>
#include <gts/micro_scheduler/patterns/Range1d.h>


using namespace matmul;

constexpr size_t BLOCK_SIZE = 32;

#define VALIDATE_MATRIX 0

//------------------------------------------------------------------------------
struct MatMulTaskData
{
    float const* matA;
    float const* matB;
    float* matC;
    float* packedB;
    const size_t M;
    const size_t N;
    const size_t K;
};


//------------------------------------------------------------------------------
Stats matMulPefSerial(const size_t M, const size_t N, const size_t K, size_t iterations)
{
    Stats stats(iterations);

    gts::Vector<float> A(M * N);
    initMatrixRand(A.data(), M, N, 10.0f);
    gts::Vector<float> B(N * K);
    initMatrixRand(B.data(), M, N, 10.0f);
    gts::Vector<float> C(M * K);
    initMatrix(C.data(), M, K, 0.0f);

#if VALIDATE_MATRIX == 1
    gts::Vector<float> matrixValid(M * K);
    initMatrix(matrixValid.data(), M, K, 0.0f);
    createVaidationMatrix(A.data(), B.data(), matrixValid.data(), M, N, K, M, K);
#endif

    // Do test.
    for (size_t ii = 0; ii < iterations; ++ii)
    {
        auto start = std::chrono::high_resolution_clock::now();

        sgemmBlockDivideAndConquerKd(A.data(), B.data(), C.data(), M, N, K, M, N, K, BLOCK_SIZE, BLOCK_SIZE);

        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double> diff = end - start;
        stats.addDataPoint(diff.count());

#if VALIDATE_MATRIX == 1
        if (!validateMatrix(C.data(), matrixValid.data(), M, K))
        {
            __debugbreak();
        }
#endif
    }

    return stats;
}

void test(const size_t M, const size_t N, const size_t K)
{
    gts::Vector<float> A(M * N);
    initMatrixRand(A.data(), M, N, 10.0f);
    gts::Vector<float> B(N * K);
    initMatrixRand(B.data(), M, N, 10.0f);
    gts::Vector<float> C(M * K);
    initMatrix(C.data(), M, K, 0.0f);

    gts::Vector<float> matrixValid(M * K);
    initMatrix(matrixValid.data(), M, K, 0.0f);
    sgemmBlocked(A.data(), B.data(), matrixValid.data(), M, N, K, M, K);

    sgemmKernelSimd256(A.data(), B.data(), C.data(), N, K, M, N, K);
    //sgemmBlockDivideAndConquerKd(A.data(), B.data(), C.data(), M, N, K, M, N, K, 64, 64);

    if (!validateMatrix(C.data(), matrixValid.data(), M, K))
    {
        __debugbreak();
    }
}

//------------------------------------------------------------------------------
// Naive matrix multiplication. A test of uniform workloads.
Stats matMulPefParallel(gts::MicroScheduler& taskScheduler, const size_t M, const size_t N, const size_t K, size_t iterations)
{
    Stats stats(iterations);

    gts::Vector<float, gts::AlignedAllocator<64>> A(M * N);
    initMatrixRand(A.data(), M, N, 10.0f);
    gts::Vector<float, gts::AlignedAllocator<64>> B(N * K);
    initMatrixRand(B.data(), M, N, 10.0f);
    gts::Vector<float, gts::AlignedAllocator<64>> C(M * K);
    initMatrix(C.data(), M, K, 0.0f);

#if VALIDATE_MATRIX == 1
    gts::Vector<float, gts::AlignedAllocator<64>> matrixValid(M * K);
    initMatrix(matrixValid.data(), M, K, 0.0f);
    createVaidationMatrix(A.data(), B.data(), matrixValid.data(), M, N, K, M, K);
#endif

    // Do test.
    for (size_t ii = 0; ii < iterations; ++ii)
    {
        GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);

        auto start = std::chrono::high_resolution_clock::now();

        sgemmParallelBlockDivideAndConquerKd(taskScheduler, A.data(), B.data(), C.data(), M, N, K, BLOCK_SIZE, BLOCK_SIZE);

        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double> diff = end - start;
        stats.addDataPoint(diff.count());
    }

    GTS_TRACE_FRAME_MARK(gts::analysis::CaptureMask::ALL);

#if VALIDATE_MATRIX == 1
    if (!validateMatrix(C.data(), matrixValid.data(), M, K))
    {
        __debugbreak();
    }
#endif

    return stats;
}
