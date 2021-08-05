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

#include "gts/containers/Vector.h"

namespace gts {

/**
 * @addtogroup Platform
 * @{
 */

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 */
class InstructionSet
{
private:

    template<typename T, size_t SIZE>
    struct Array
    {
        T data[SIZE];
    };

    // https://docs.microsoft.com/en-us/cpp/intrinsics/cpuid-cpuidex?view=msvc-160
    class CpuId
    {
    public:
        CpuId()
            : m_f_1_ECX{ 0 }
            , m_f_1_EDX{ 0 }
            , m_f_7_EBX{ 0 }
            , m_f_7_ECX{ 0 }
            , m_f_81_ECX{ 0 }
            , m_f_81_EDX{ 0 }
        {
#if GTS_ARCH_X86

            Vector<Array<int, 4>> data;
            Array<int, 4> cpuInfo;

            // Calling __cpuid with 0x0 as the function_id argument
            // gets the number of the highest valid function ID.
            __cpuid(cpuInfo.data, 0);
            int numIds = cpuInfo.data[0];

            for (int i = 0; i <= numIds; ++i)
            {
                __cpuid(cpuInfo.data, i);
                data.push_back(cpuInfo);
            }

            // load bitset with flags for function 0x00000001
            if (numIds >= 1)
            {
                m_f_1_ECX = data[1].data[2];
                m_f_1_EDX = data[1].data[3];
            }

            // load bitset with flags for function 0x00000007
            if (numIds >= 7)
            {
                m_f_7_EBX = data[7].data[1];
                m_f_7_ECX = data[7].data[2];
            }

            data.clear();

            // Calling __cpuid with 0x80000000 as the function_id argument
            // gets the number of the highest valid extended ID.
            __cpuid(cpuInfo.data, 0x80000000);
            int  numExIds = cpuInfo.data[0];

            for (int i = 0x80000000; i <= numExIds; ++i)
            {
                __cpuidex(cpuInfo.data, i, 0);
                data.push_back(cpuInfo);
            }

            // load bitset with flags for function 0x80000001
            if (numExIds >= 0x80000001)
            {
                m_f_81_ECX = data[1].data[2];
                m_f_81_EDX = data[1].data[3];
            }
#endif
        };

        uint32_t m_f_1_ECX;
        uint32_t m_f_1_EDX;
        uint32_t m_f_7_EBX;
        uint32_t m_f_7_ECX;
        uint32_t m_f_81_ECX;
        uint32_t m_f_81_EDX;
    };

public:

    static bool cmpxchg16B(void) { return m_cpuId.m_f_1_EDX & (1 << 13); }
    static bool waitPackage(void) { return m_cpuId.m_f_7_ECX & (1 << 5); }

private:

    static const CpuId m_cpuId;
};

/** @} */ // end of Platform

} // namespace gts
