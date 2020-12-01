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
#include <immintrin.h>
#include <cassert>

#include <gts/containers/Vector.h>
#include <gts/micro_scheduler/MicroScheduler.h>

namespace matmul {

// ***** NOTE: MATRIX DIMENSIONS MUST BE A POWER-OF-2. *****

//------------------------------------------------------------------------------
inline void initMatrix(float* M, size_t rows, size_t cols, float value)
{
    for (size_t r = 0; r < rows; r++)
    {
        for (size_t c = 0; c < cols; c++)
        {
            M[r * cols + c] = value;
        }
    }
}

//------------------------------------------------------------------------------
inline void initMatrixRand(float* M, size_t rows, size_t cols, float /*rangeValue*/)
{
    for (size_t r = 0; r < rows; r++)
    {
        for (size_t c = 0; c < cols; c++)
        {
            float rnd = 1;/*((float)rand() / (float)(RAND_MAX))* rangeValue;*/
            M[r * cols + c] = rnd;
        }
    }
}

//------------------------------------------------------------------------------
inline void sgemmKernel(
    float const* __restrict matrixA, float const* __restrict matrixB, float* __restrict matrixC,
    size_t const N, size_t const K,
    size_t const m, size_t const n, size_t const k)
{
    for (size_t mb = 0; mb < m; ++mb)
    {
        for (size_t nb = 0; nb < n; ++nb)
        {
            for (size_t kb = 0; kb < k; ++mb)
            {
                matrixC[mb * K + kb] += matrixA[mb * N + nb] * matrixB[nb * K + kb];
            }
        }
    }
}

//------------------------------------------------------------------------------
inline void sgemmKernelSimd256(
    float const* __restrict matrixA, float const* __restrict matrixB, float* __restrict matrixC,
    size_t const N, size_t const K,
    size_t const m, size_t const n, size_t const k)
{
    constexpr size_t SIMD_STRIDE   = 8;
    constexpr size_t KERNEL_HEIGHT = 4;
    constexpr size_t KERNEL_WIDTH  = SIMD_STRIDE * 2;

#if 0
    sgemmKernel(matrixA, matrixB, matrixC, N, K, m, n, k);
#else
    for (size_t mb = 0; mb < m; mb += KERNEL_HEIGHT)
    {
        for (size_t kb = 0; kb < k; kb += KERNEL_WIDTH)
        {
            __m256
                c00, c01,
                c10, c11,
                c20, c21,
                c30, c31,
                a00, a10, a20, a30,
                b00, b01;

            c00 = _mm256_setzero_ps(); c01 = _mm256_setzero_ps();
            c10 = _mm256_setzero_ps(); c11 = _mm256_setzero_ps();
            c20 = _mm256_setzero_ps(); c21 = _mm256_setzero_ps();
            c30 = _mm256_setzero_ps(); c31 = _mm256_setzero_ps();

            for (size_t nb = 0; nb < n; ++nb)
            {
                b00 = _mm256_load_ps(matrixB + nb * K + kb);
                b01 = _mm256_load_ps(matrixB + nb * K + kb + SIMD_STRIDE);

                a00 = _mm256_broadcast_ss(matrixA + (mb + 0) * N + nb);
                a10 = _mm256_broadcast_ss(matrixA + (mb + 1) * N + nb);
                a20 = _mm256_broadcast_ss(matrixA + (mb + 2) * N + nb);
                a30 = _mm256_broadcast_ss(matrixA + (mb + 3) * N + nb);

                // first column
                c00 = _mm256_fmadd_ps(a00, b00, c00);
                c10 = _mm256_fmadd_ps(a10, b00, c10);
                c20 = _mm256_fmadd_ps(a20, b00, c20);
                c30 = _mm256_fmadd_ps(a30, b00, c30);

                // second column
                c01 = _mm256_fmadd_ps(a00, b01, c01);
                c11 = _mm256_fmadd_ps(a10, b01, c11);
                c21 = _mm256_fmadd_ps(a20, b01, c21);
                c31 = _mm256_fmadd_ps(a30, b01, c31);
            }

            _mm256_store_ps(matrixC + (mb + 0) * K + kb, c00);
            _mm256_store_ps(matrixC + (mb + 1) * K + kb, c10);
            _mm256_store_ps(matrixC + (mb + 2) * K + kb, c20);
            _mm256_store_ps(matrixC + (mb + 3) * K + kb, c30);

            _mm256_store_ps(matrixC + (mb + 0) * K + kb + SIMD_STRIDE, c01);
            _mm256_store_ps(matrixC + (mb + 1) * K + kb + SIMD_STRIDE, c11);
            _mm256_store_ps(matrixC + (mb + 2) * K + kb + SIMD_STRIDE, c21);
            _mm256_store_ps(matrixC + (mb + 3) * K + kb + SIMD_STRIDE, c31);
        }
    }
#endif
}

//------------------------------------------------------------------------------
inline void sgemmKernelSimd512(
    float const* __restrict matrixA, float const* __restrict matrixB, float* __restrict matrixC,
    size_t const N, size_t const K,
    size_t const m, size_t const n, size_t const k)
{
    constexpr size_t SIMD_STRIDE   = 16;

#if 0
    sgemmKernel(matrixA, matrixB, matrixC, N, K, m, n, k);
#elif 1

    constexpr size_t KERNEL_HEIGHT = 4;
    constexpr size_t KERNEL_WIDTH  = SIMD_STRIDE * 2;

    for (size_t mb = 0; mb < m; mb += KERNEL_HEIGHT)
    {
        for (size_t kb = 0; kb < k; kb += KERNEL_WIDTH)
        {
            __m512
                c00, c01,
                c10, c11,
                c20, c21,
                c30, c31,
                a00, a10, a20, a30,
                b00, b01;

            c00 = _mm512_setzero_ps(); c01 = _mm512_setzero_ps();
            c10 = _mm512_setzero_ps(); c11 = _mm512_setzero_ps();
            c20 = _mm512_setzero_ps(); c21 = _mm512_setzero_ps();
            c30 = _mm512_setzero_ps(); c31 = _mm512_setzero_ps();

            for (size_t nb = 0; nb < n; ++nb)
            {
                b00 = _mm512_load_ps(matrixB + nb * K + kb);
                b01 = _mm512_load_ps(matrixB + nb * K + kb + SIMD_STRIDE);

                a00 = _mm512_set1_ps(matrixA[(mb + 0) * N + nb]);
                a10 = _mm512_set1_ps(matrixA[(mb + 1) * N + nb]);
                a20 = _mm512_set1_ps(matrixA[(mb + 2) * N + nb]);
                a30 = _mm512_set1_ps(matrixA[(mb + 3) * N + nb]);

                // first column
                c00 = _mm512_fmadd_ps(a00, b00, c00);
                c10 = _mm512_fmadd_ps(a10, b00, c10);
                c20 = _mm512_fmadd_ps(a20, b00, c20);
                c30 = _mm512_fmadd_ps(a30, b00, c30);

                // second column
                c01 = _mm512_fmadd_ps(a00, b01, c01);
                c11 = _mm512_fmadd_ps(a10, b01, c11);
                c21 = _mm512_fmadd_ps(a20, b01, c21);
                c31 = _mm512_fmadd_ps(a30, b01, c31);
            }

            _mm512_store_ps(matrixC + (mb + 0) * K + kb, c00);
            _mm512_store_ps(matrixC + (mb + 1) * K + kb, c10);
            _mm512_store_ps(matrixC + (mb + 2) * K + kb, c20);
            _mm512_store_ps(matrixC + (mb + 3) * K + kb, c30);

            _mm512_store_ps(matrixC + (mb + 0) * K + kb + SIMD_STRIDE, c01);
            _mm512_store_ps(matrixC + (mb + 1) * K + kb + SIMD_STRIDE, c11);
            _mm512_store_ps(matrixC + (mb + 2) * K + kb + SIMD_STRIDE, c21);
            _mm512_store_ps(matrixC + (mb + 3) * K + kb + SIMD_STRIDE, c31);
        }
    }
#endif
}


//------------------------------------------------------------------------------
inline void sgemmBlocked(
    float const* __restrict matrixA, float const* __restrict matrixB, float* __restrict matrixC,
    size_t const M, size_t const N, size_t const K,
    size_t const blockSizeM, size_t blockSizeK)
{
    for (size_t m = 0; m < M; m += blockSizeM)
    {
        for (size_t k = 0; k < K; k += blockSizeK)
        {
            for (size_t n = 0; n < N; ++n)
            {
                for (size_t kb = k; kb < k + blockSizeK; ++kb)
                {
                    for (size_t mb = m; mb < m + blockSizeM; ++mb)
                    {
                        matrixC[mb * K + kb] += matrixA[mb * N + n] * matrixB[n * K + kb];
                    }
                }
            }
        }
    }
}

//------------------------------------------------------------------------------
inline void sgemmBlockDivideAndConquerKd(
    float const* __restrict matrixA, float const* __restrict matrixB, float* __restrict matrixC,
    size_t const M, size_t const N, size_t const K,
    size_t const m, size_t const n, size_t const k,
    size_t const blockSizeM, size_t const blockSizeK)
{
    // If we can't split anymore, run the kernel
    if (m <= blockSizeM && k <= blockSizeK)
    {
        assert(m != 0 && n != 0 && k != 0);
        sgemmKernelSimd256(matrixA, matrixB, matrixC, N, K, m, n, k);
    }
    // Split along M
    else if(m > k)
    {
        size_t halfM = m / 2;

        if (n > blockSizeM)
        {
            size_t halfN = n / 2;

            sgemmBlockDivideAndConquerKd(
                matrixA,
                matrixB,
                matrixC,
                M, N, K,
                halfM, halfN, k,
                blockSizeM,
                blockSizeK);

            sgemmBlockDivideAndConquerKd(
                matrixA + halfM * N,
                matrixB,
                matrixC + halfM * K,
                M, N, K,
                halfM, halfN, k,
                blockSizeM,
                blockSizeK);

            // --- N

            sgemmBlockDivideAndConquerKd(
                matrixA + halfN,
                matrixB + halfN * K,
                matrixC,
                M, N, K,
                halfM, halfN, k,
                blockSizeM,
                blockSizeK);

            sgemmBlockDivideAndConquerKd(
                matrixA + halfM * N + halfN,
                matrixB + halfN * K,
                matrixC + halfM * K,
                M, N, K,
                halfM, halfN, k,
                blockSizeM,
                blockSizeK);
        }
        else
        {
            sgemmBlockDivideAndConquerKd(
                matrixA,
                matrixB,
                matrixC,
                M, N, K,
                halfM, n, k,
                blockSizeM,
                blockSizeK);

            sgemmBlockDivideAndConquerKd(
                matrixA + halfM * N,
                matrixB,
                matrixC + halfM * K,
                M, N, K,
                halfM, n, k,
                blockSizeM,
                blockSizeK);
        }
    }
    // Split along K
    else
    {
        size_t halfK = k / 2;

        if (n > blockSizeK)
        {
            size_t halfN = n / 2;

            sgemmBlockDivideAndConquerKd(
                matrixA,
                matrixB,
                matrixC,
                M, N, K,
                m, halfN, halfK,
                blockSizeM,
                blockSizeK);

            sgemmBlockDivideAndConquerKd(
                matrixA,
                matrixB + halfK,
                matrixC + halfK,
                M, N, K,
                m, halfN, halfK,
                blockSizeM,
                blockSizeK);

            // --- N

            sgemmBlockDivideAndConquerKd(
                matrixA + halfN,
                matrixB + halfN * K,
                matrixC,
                M, N, K,
                m, halfN, halfK,
                blockSizeM,
                blockSizeK);

            sgemmBlockDivideAndConquerKd(
                matrixA + halfN,
                matrixB + halfN * K + halfK,
                matrixC + halfK,
                M, N, K,
                m, halfN, halfK,
                blockSizeM,
                blockSizeK);
        }
        else
        {
            sgemmBlockDivideAndConquerKd(
                matrixA,
                matrixB,
                matrixC,
                M, N, K,
                m, n, halfK,
                blockSizeM,
                blockSizeK);

            sgemmBlockDivideAndConquerKd(
                matrixA,
                matrixB + halfK,
                matrixC + halfK,
                M, N, K,
                m, n, halfK,
                blockSizeM,
                blockSizeK);
        }
    }
}

//------------------------------------------------------------------------------
inline void createVaidationMatrix(
    float const* __restrict matrixA, float const* __restrict matrixB, float* __restrict matrixC,
    size_t const M, size_t const N, size_t const K,
    size_t const blockSizeM, size_t blockSizeK)
{
    sgemmBlocked(matrixA, matrixB, matrixC, M, N, K, blockSizeM, blockSizeK);
}

#define EPSILON 0.01f
//------------------------------------------------------------------------------
inline bool validateMatrix(float const* __restrict matrixC, float const* __restrict matrixValid,
    size_t M, size_t K)
{
    for (size_t m = 0; m < M; m++)
    {
        for (size_t k = 0; k < K; k++)
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
 *              N                   K                   K
 *         ---     ---         ---     ---         ---     ---
 *         |         |         |         |         |         |
 *      M  |         |   *   N |         |   =   M |         |
 *         |         |         |         |         |         |
 *         ---     ---         ---     ---         ---     ---
 *              A        *          B        =          C
 */
class ParallelSgemmBlockedKdTask : public gts::Task
{
    float const* __restrict matrixA;
    float const* __restrict matrixB;
    float* __restrict matrixC;
    size_t const M, N, K;
    size_t m, n, k;
    size_t const blockSizeM, blockSizeK;

public:

    inline ParallelSgemmBlockedKdTask(
        float const* __restrict matrixA, float const* __restrict matrixB, float* __restrict matrixC,
        size_t const M, size_t const N, size_t const K,
        size_t const m, size_t const n, size_t const k,
        size_t const blockSizeM, size_t blockSizeK)
        : matrixA(matrixA)
        , matrixB(matrixB)
        , matrixC(matrixC)
        , M(M)
        , N(N)
        , K(K)
        , m(m)
        , n(n)
        , k(k)
        , blockSizeM(blockSizeM)
        , blockSizeK(blockSizeK)
    {}


    inline virtual gts::Task* execute(gts::TaskContext const& ctx) final
    {
        // If we can't split anymore, run the kernel
        if (m <= blockSizeM && k <= blockSizeK)
        {
            assert(m != 0 && n != 0 && k != 0);
            sgemmKernelSimd512(matrixA, matrixB, matrixC, N, K, m, n, k);
            return nullptr;
        }
        // Split along M
        else if (m > k)
        {
            size_t halfM = m / 2;

            if (n > blockSizeM)
            {
                size_t halfN = n / 2;

                gts::Task* pCont = ctx.pMicroScheduler->allocateTask([](
                    float const* __restrict matrixA, float const* __restrict matrixB, float* __restrict matrixC,
                    size_t const M, size_t const N, size_t const K,
                    size_t const m, size_t const n, size_t const k,
                    size_t const blockSizeM, size_t blockSizeK, gts::TaskContext const& ctx) -> gts::Task*
                {
                    gts::Task* pCont = ctx.pMicroScheduler->allocateTask<gts::EmptyTask>();
                    ctx.pThisTask->setContinuationTask(pCont);
                    pCont->addRef(2, gts::memory_order::relaxed);

                    gts::Task* pTask = ctx.pMicroScheduler->allocateTask<ParallelSgemmBlockedKdTask>(
                        matrixA + n,
                        matrixB + n * K,
                        matrixC,
                        M, N, K,
                        m, n, k,
                        blockSizeM,
                        blockSizeK);

                    pCont->addChildTaskWithoutRef(pTask);
                    ctx.pMicroScheduler->spawnTask(pTask);

                    pTask = ctx.pMicroScheduler->allocateTask<ParallelSgemmBlockedKdTask>(
                        matrixA + m * N + n,
                        matrixB + n * K,
                        matrixC + m * K,
                        M, N, K,
                        m, n, k,
                        blockSizeM,
                        blockSizeK);

                    pCont->addChildTaskWithoutRef(pTask);
                    return pTask;
                },
                matrixA,
                matrixB,
                matrixC,
                M, N, K,
                halfM, halfN, k,
                blockSizeM,
                blockSizeK);

                setContinuationTask(pCont);
                pCont->addRef(2, gts::memory_order::relaxed);

                gts::Task* pTask = ctx.pMicroScheduler->allocateTask<ParallelSgemmBlockedKdTask>(
                    matrixA,
                    matrixB,
                    matrixC,
                    M, N, K,
                    halfM, halfN, k,
                    blockSizeM,
                    blockSizeK);

                pCont->addChildTaskWithoutRef(pTask);
                ctx.pMicroScheduler->spawnTask(pTask);

                recycle();

                matrixA += halfM * N;
                matrixC += halfM * K;
                m = halfM;
                n = halfN;

                pCont->addChildTaskWithoutRef(this);
                return this;
            }
            else
            {
                gts::Task* pCont = ctx.pMicroScheduler->allocateTask<gts::EmptyTask>();
                
                setContinuationTask(pCont);
                pCont->addRef(2, gts::memory_order::relaxed);

                gts::Task* pTask = ctx.pMicroScheduler->allocateTask<ParallelSgemmBlockedKdTask>(
                    matrixA,
                    matrixB,
                    matrixC,
                    M, N, K,
                    halfM, n, k,
                    blockSizeM,
                    blockSizeK);

                pCont->addChildTaskWithoutRef(pTask);
                ctx.pMicroScheduler->spawnTask(pTask);

                recycle();

                matrixA += halfM * N;
                matrixC += halfM * K;
                m = halfM;

                pCont->addChildTaskWithoutRef(this);
                return this;
            }
        }
        // Split along K
        else
        {
            size_t halfK = k / 2;

            if (n > blockSizeK)
            {
                size_t halfN = n / 2;

                gts::Task* pCont = ctx.pMicroScheduler->allocateTask([](
                    float const* __restrict matrixA, float const* __restrict matrixB, float* __restrict matrixC,
                    size_t const M, size_t const N, size_t const K,
                    size_t const m, size_t const n, size_t const k,
                    size_t const blockSizeM, size_t blockSizeK, gts::TaskContext const& ctx) -> gts::Task*
                    {
                        gts::Task* pCont = ctx.pMicroScheduler->allocateTask<gts::EmptyTask>();
                        ctx.pThisTask->setContinuationTask(pCont);
                        pCont->addRef(2, gts::memory_order::relaxed);

                        gts::Task* pTask = ctx.pMicroScheduler->allocateTask<ParallelSgemmBlockedKdTask>(
                            matrixA + n,
                            matrixB + n * K,
                            matrixC,
                            M, N, K,
                            m, n, k,
                            blockSizeM,
                            blockSizeK);

                        pCont->addChildTaskWithoutRef(pTask);
                        ctx.pMicroScheduler->spawnTask(pTask);

                        pTask = ctx.pMicroScheduler->allocateTask<ParallelSgemmBlockedKdTask>(
                            matrixA + n,
                            matrixB + n * K + k,
                            matrixC + k,
                            M, N, K,
                            m, n, k,
                            blockSizeM,
                            blockSizeK);

                        pCont->addChildTaskWithoutRef(pTask);
                        return pTask;
                    },
                    matrixA,
                        matrixB,
                        matrixC,
                        M, N, K,
                        m, halfN, halfK,
                        blockSizeM,
                        blockSizeK);

                setContinuationTask(pCont);
                pCont->addRef(2, gts::memory_order::relaxed);

                gts::Task* pTask = ctx.pMicroScheduler->allocateTask<ParallelSgemmBlockedKdTask>(
                    matrixA,
                    matrixB,
                    matrixC,
                    M, N, K,
                    m, halfN, halfK,
                    blockSizeM,
                    blockSizeK);

                pCont->addChildTaskWithoutRef(pTask);
                ctx.pMicroScheduler->spawnTask(pTask);

                recycle();

                matrixB += halfK;
                matrixC += halfK;
                n = halfN;
                k = halfK;

                pCont->addChildTaskWithoutRef(this);
                return this;
            }
            else
            {
                gts::Task* pCont = ctx.pMicroScheduler->allocateTask<gts::EmptyTask>();

                setContinuationTask(pCont);
                pCont->addRef(2, gts::memory_order::relaxed);

                gts::Task* pTask = ctx.pMicroScheduler->allocateTask<ParallelSgemmBlockedKdTask>(
                    matrixA,
                    matrixB,
                    matrixC,
                    M, N, K,
                    m, n, halfK,
                    blockSizeM,
                    blockSizeK);

                pCont->addChildTaskWithoutRef(pTask);
                ctx.pMicroScheduler->spawnTask(pTask);

                recycle();

                matrixB += halfK;
                matrixC += halfK;
                k = halfK;

                pCont->addChildTaskWithoutRef(this);
                return this;
            }
        }
    }
};

//------------------------------------------------------------------------------
inline void sgemmParallelBlockDivideAndConquerKd(
    gts::MicroScheduler& taskScheduler,
    float const* __restrict matrixA, float const* __restrict matrixB, float* __restrict matrixC,
    size_t const M, size_t const N, size_t const K,
    size_t const blockSizeM, size_t const blockSizeK)
{
    gts::Task* pRoot = taskScheduler.allocateTask<ParallelSgemmBlockedKdTask>(
        matrixA,
        matrixB,
        matrixC,
        M, N, K,
        M, N, K,
        blockSizeM,
        blockSizeK);

    taskScheduler.spawnTaskAndWait(pRoot);
}

} // namespace matmul
