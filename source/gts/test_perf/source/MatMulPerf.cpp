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
#include <immintrin.h>

#include "gts_perf/Stats.h"

#include <gts/micro_scheduler/WorkerPool.h>
#include <gts/micro_scheduler/MicroScheduler.h>
#include <gts/micro_scheduler/patterns/ParallelFor.h>
#include <gts/micro_scheduler/patterns/ParallelReduce.h>
#include <gts/micro_scheduler/patterns/BlockedRange1d.h>
#include <gts/micro_scheduler/patterns/BlockedRange2d.h>

//------------------------------------------------------------------------------
void initMatrix(float* M, uint32_t rows, uint32_t cols, float value)
{
    for (uint32_t r = 0; r < rows; r++)
    {
        for (uint32_t c = 0; c < cols; c++)
        {
            M[r * cols + c] = value;
        }
    }
}

//------------------------------------------------------------------------------
void initMatrixRand(float* M, uint32_t rows, uint32_t cols, float rangeValue)
{
    for (uint32_t r = 0; r < rows; r++)
    {
        for (uint32_t c = 0; c < cols; c++)
        {
            float rnd = ((float)rand() / (float)(RAND_MAX)) * rangeValue;
            M[r * cols + c] = rnd;
        }
    }
}

//------------------------------------------------------------------------------
void createVaidationMatrix(float const* matrixA, float const* matrixB, float* matrixC,
    uint32_t M, uint32_t N, uint32_t K)
{
    for (uint32_t m = 0; m < M; m++)
    {
        float sum;
        for (uint32_t k = 0; k < K; k++)
        {
            sum = 0.0f;
            for (uint32_t n = 0; n < N; n++)
            {
                sum += matrixA[m * N + n] * matrixB[n * K + k];
            }

            matrixC[m * K + k] = sum;
        }
    }
}

#define EPSILON 0.01f
//------------------------------------------------------------------------------
bool validateMatrix(float const* matrixC, float const* matrixValid,
    uint32_t M, uint32_t K)
{
    for (uint32_t m = 0; m < M; m++)
    {
        for (uint32_t k = 0; k < K; k++)
        {
            float delta = (float)fabs(matrixC[m * K + k] - matrixValid[m * K + k]);
            if (delta > EPSILON)
            {
                return false;
            }
        }
    }
    return true;
}

//==============================================================================
/**
* Matrix layout, rows x col, row major storage:
*              N                     K                     K
*         ---     ---           ---     ---           ---     ---
*         |         |           |         |           |         |
*      M  |         |   *     N |         |   =     M |         |
*         |         |           |         |           |         |
*         ---     ---           ---     ---           ---     ---
*              A        *            B        =            C
*/

#define TILE_HEIGHT 8
#define TILE_WIDTH 128

#define MIRCO_HEIGHT 8
#define MICRO_WIDTH  8

//------------------------------------------------------------------------------
void matMul_no_simd(float const* matrixA, float const* matrixB, float* matrixC,
    const uint32_t N, const uint32_t K,
    const uint32_t uRowStart, const uint32_t uRowEnd)
{
    alignas(32) float packedResult[TILE_HEIGHT * TILE_WIDTH];

    // Loop over the sub matrix M dimension by tile. 
    for (uint32_t m = uRowStart; m < uRowEnd; m += TILE_HEIGHT)
    {
        float const* mA = matrixA + m*N;

        // Loop over the sub matrix K dimension by tile. 
        for (uint32_t k = 0; k < K; k += TILE_WIDTH)
        {
            // Load packedResult into cache.
            for (uint32_t ii = 0, len = TILE_HEIGHT * TILE_WIDTH; ii < len; ++ii)
            {
                packedResult[ii] = 0.0f;
            }

            float const* mB = matrixB + k*N;

            // Loop over the matrix N dimension:
            for (uint32_t n = 0; n < N; n++)
            {
                //
                // Inner-kernel loops over tiles:

                for (int kt = 0; kt < TILE_WIDTH; kt += MICRO_WIDTH)
                {
                    //
                    // Micro-kernel loops over blocks:

                    for (int kb = kt; kb < kt + MICRO_WIDTH; ++kb)
                    {
                        for (int mb = 0; mb < MIRCO_HEIGHT; ++mb)
                        {
                            packedResult[mb * TILE_WIDTH + kb] += mA[mb * N + n] * mB[kb];
                        }
                    }
                }

                mB += TILE_WIDTH;
            }

            // Loop horizontally again over TILE_WIDTH dimension:
            float* mC = matrixC + m*K + k;
            for (int mi = 0; mi < TILE_HEIGHT; ++mi)
            {
                for (int ki = 0; ki < TILE_WIDTH; ++ki)
                {
                    mC[mi*K + ki] = packedResult[mi * TILE_WIDTH + ki];
                }
            }
        }
    }
}

//------------------------------------------------------------------------------
void packB(float const* matrixB,
    const uint32_t N, const uint32_t K,
    const uint32_t uColStart, const uint32_t uColEnd,
    float*& packedB)
{
    // Loop over the sub matrix K dimension by tile. 
    for (uint32_t k = uColStart; k < uColEnd; k += TILE_WIDTH)
    {
        uint32_t bi = N*k;

        // Loop over the matrix N dimension:
        for (uint32_t n = 0; n < N; n++)
        {
            // Loop over each element in the tile.
            for (uint32_t kt = 0; kt < TILE_WIDTH; ++kt)
            {
                packedB[bi++] = matrixB[n*K + k + kt];
            }
        }
    }
}

//------------------------------------------------------------------------------
void matMul(float const* matrixA, float const* matrixB, float* matrixC,
    const uint32_t N, const uint32_t K,
    const uint32_t uRowStart, const uint32_t uRowEnd)
{
    alignas(32) float packedResult[TILE_HEIGHT * TILE_WIDTH];

    // Loop over the sub matrix M dimension by tile. 
    for (uint32_t m = uRowStart; m < uRowEnd; m += TILE_HEIGHT)
    {
        float const* mA = matrixA + m*N;

        // Loop over the sub matrix K dimension by tile. 
        for (uint32_t k = 0; k < K; k += TILE_WIDTH)
        {
            // Load packedResult into cache.
            for (uint32_t ii = 0, len = TILE_HEIGHT * TILE_WIDTH; ii < len; ++ii)
            {
                packedResult[ii] = 0.0f;
            }

            float const* mB = matrixB + k*N;

            // Loop over the matrix N dimension:
            for (uint32_t n = 0; n < N; n++)
            {
                // Pack A into registers.
                __m256 a0 = _mm256_broadcast_ss(mA + 0 * N + n);
                __m256 a1 = _mm256_broadcast_ss(mA + 1 * N + n);
                __m256 a2 = _mm256_broadcast_ss(mA + 2 * N + n);
                __m256 a3 = _mm256_broadcast_ss(mA + 3 * N + n);
                __m256 a4 = _mm256_broadcast_ss(mA + 4 * N + n);
                __m256 a5 = _mm256_broadcast_ss(mA + 5 * N + n);
                __m256 a6 = _mm256_broadcast_ss(mA + 6 * N + n);
                __m256 a7 = _mm256_broadcast_ss(mA + 7 * N + n);

                __m256 c0 = _mm256_load_ps(packedResult + 0 * TILE_WIDTH);
                __m256 c1, c2, c3, c4, c5, c6, c7;

                __m256 b;

                for (uint32_t kt = 0; kt < TILE_WIDTH; kt += MICRO_WIDTH)
                {
                    // Pack B into a register.
                    b = _mm256_load_ps(mB + kt);

                    //
                    // Micro-kernel loops over blocks 8x8:
                    c1 = _mm256_load_ps(packedResult + 1 * TILE_WIDTH + kt);
                    c0 = _mm256_fmadd_ps(a0, b, c0);
                    _mm256_store_ps(packedResult + 0 * TILE_WIDTH + kt, c0);

                    c2 = _mm256_load_ps(packedResult + 2 * TILE_WIDTH + kt);
                    c1 = _mm256_fmadd_ps(a1, b, c1);
                    _mm256_store_ps(packedResult + 1 * TILE_WIDTH + kt, c1);

                    c3 = _mm256_load_ps(packedResult + 3 * TILE_WIDTH + kt);
                    c2 = _mm256_fmadd_ps(a2, b, c2);
                    _mm256_store_ps(packedResult + 2 * TILE_WIDTH + kt, c2);

                    c4 = _mm256_load_ps(packedResult + 4 * TILE_WIDTH + kt);
                    c3 = _mm256_fmadd_ps(a3, b, c3);
                    _mm256_store_ps(packedResult + 3 * TILE_WIDTH + kt, c3);

                    c5 = _mm256_load_ps(packedResult + 5 * TILE_WIDTH + kt);
                    c4 = _mm256_fmadd_ps(a4, b, c4);
                    _mm256_store_ps(packedResult + 4 * TILE_WIDTH + kt, c4);

                    c6 = _mm256_load_ps(packedResult + 6 * TILE_WIDTH + kt);
                    c5 = _mm256_fmadd_ps(a5, b, c5);
                    _mm256_store_ps(packedResult + 5 * TILE_WIDTH + kt, c5);

                    c7 = _mm256_load_ps(packedResult + 7 * TILE_WIDTH + kt);
                    c6 = _mm256_fmadd_ps(a6, b, c6);
                    _mm256_store_ps(packedResult + 6 * TILE_WIDTH + kt, c6);

                    c0 = _mm256_load_ps(packedResult + 0 * TILE_WIDTH + kt + 8);
                    c7 = _mm256_fmadd_ps(a7, b, c7);
                    _mm256_store_ps(packedResult + 7 * TILE_WIDTH + kt, c7);
                }

                mB += TILE_WIDTH;
            }

            // Loop horizontally again over TILE_WIDTH dimension:
            float* mC = matrixC + m*K + k;
            for (uint32_t mi = 0; mi < TILE_HEIGHT; ++mi)
            {
                for (uint32_t ki = 0; ki < TILE_WIDTH; ki += 8)
                {
                    __m256 c = _mm256_loadu_ps(packedResult + mi*TILE_WIDTH + ki);
                    _mm256_stream_ps(mC + mi*K + ki, c);
                }
            }
        }
    }
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
Stats matMulPefSerial(const uint32_t M, const uint32_t N, const uint32_t K, uint32_t iterations)
{
    Stats stats(iterations);

    gts::Vector<float> A(M * N);
    initMatrixRand(A.data(), M, N, 10.0f);
    gts::Vector<float> B(N * K);
    initMatrixRand(B.data(), M, N, 10.0f);
    gts::Vector<float> C(M * K);
    initMatrix(C.data(), M, K, 0.0f);
    gts::Vector<float> matrixValid(M * K);
    initMatrix(matrixValid.data(), M, K, 0.0f);
    createVaidationMatrix(A.data(), B.data(), matrixValid.data(), M, N, K);

    gts::Vector<float> packedB(N * K);

    // Do test.
    for (uint32_t ii = 0; ii < iterations; ++ii)
    {
        auto start = std::chrono::high_resolution_clock::now();

        float* pB = packedB.data();
        packB(B.data(), N, K, 0, K, pB);
        matMul_no_simd(A.data(), pB, C.data(), N, K, 0, M);

        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double> diff = end - start;
        stats.addDataPoint(diff.count());

        if (!validateMatrix(C.data(), matrixValid.data(), M, K))
        {
            __debugbreak();
        }
    }

    return stats;
}

//------------------------------------------------------------------------------
Stats matMulPefParallel(const uint32_t M, const uint32_t N, const uint32_t K, uint32_t iterations, uint32_t threadCount, bool affinitize)
{
    Stats stats(iterations);

    gts::WorkerPool workerPool;
    if (affinitize)
    {
        gts::WorkerPoolDesc desc;

        gts::CpuTopology topology;
        gts::Thread::getCpuTopology(topology);

        gts::Vector<uintptr_t> affinityMasks;

        for (size_t iThread = 0, threadsPerCore = topology.coreInfo[0].logicalAffinityMasks.size(); iThread < threadsPerCore; ++iThread)
        {
            for (auto& core : topology.coreInfo)
            {
                affinityMasks.push_back(core.logicalAffinityMasks[iThread]);
            }
        }

        for (uint32_t ii = 0; ii < threadCount; ++ii)
        {
            gts::WorkerThreadDesc d;
            d.affinityMask = affinityMasks[ii];
            desc.workerDescs.push_back(d);
        }

        workerPool.initialize(desc);
    }
    else
    {
        workerPool.initialize(threadCount);
    }

    gts::MicroScheduler taskScheduler;
    taskScheduler.initialize(&workerPool);

    gts::ParallelFor parallelFor(taskScheduler);

    gts::Vector<float> A(M * N);
    initMatrixRand(A.data(), M, N, 10.0f);
    gts::Vector<float> B(N * K);
    initMatrixRand(B.data(), M, N, 10.0f);
    gts::Vector<float> C(M * K);
    initMatrix(C.data(), M, K, 0.0f);
    gts::Vector<float> matrixValid(M * K);
    initMatrix(matrixValid.data(), M, K, 0.0f);
    createVaidationMatrix(A.data(), B.data(), matrixValid.data(), M, N, K);

    gts::Vector<float> packedB(N * K);
    MatMulTaskData taskData{ A.data(), B.data(), C.data(), packedB.data(), M, N, K };

    // Do test.
    for (uint32_t ii = 0; ii < iterations; ++ii)
    {
        auto start = std::chrono::high_resolution_clock::now();

        // Pack B in parallel.
        parallelFor(
            gts::BlockedRange1d<uint32_t>(0, M, TILE_WIDTH),
            [](gts::BlockedRange1d<uint32_t> r, void* userData, gts::TaskContext const&)
        {
            MatMulTaskData& data = *(MatMulTaskData*)userData;

            uint32_t uColStart = r.begin();
            uint32_t uColEnd = r.end();

            packB(data.matB, data.N, data.K, uColStart, uColEnd, data.packedB);
        },
            gts::StaticPartitioner(),
            &taskData);

        // Matmul in parallel.
        parallelFor(
            gts::BlockedRange1d<uint32_t>(0, M, TILE_HEIGHT),
            [](gts::BlockedRange1d<uint32_t> r, void* userData, gts::TaskContext const&)
        {
            MatMulTaskData& data = *(MatMulTaskData*)userData;

            uint32_t uRowStart = r.begin();
            uint32_t uRowEnd = r.end();

            matMul_no_simd(data.matA, data.packedB, data.matC,
                data.N, data.K, uRowStart, uRowEnd);
        },
            gts::StaticPartitioner(),
            &taskData);

        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double> diff = end - start;
        stats.addDataPoint(diff.count());
    }

    if (!validateMatrix(C.data(), matrixValid.data(), M, K))
    {
        __debugbreak();
    }

    return stats;
}
