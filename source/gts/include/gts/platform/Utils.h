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

#include "gts/platform/Machine.h"

namespace gts {

//------------------------------------------------------------------------------
/**
 * @return True if 'value' is a power of 2.
 */
GTS_INLINE uint32_t isPow2(uint32_t value)
{
    return value && !(value & (value - 1));
}

//------------------------------------------------------------------------------
/**
 * @return The next power of 2 after value.
 */
GTS_INLINE uint32_t nextPow2(uint32_t value)
{
    value--;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    return ++value;
}

//------------------------------------------------------------------------------
/**
 * @return The next power of 2 after value.
 */
GTS_INLINE uint64_t nextPow2(uint64_t value)
{
    value--;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value |= value >> 32;
    return ++value;
}

//------------------------------------------------------------------------------
/**
 * @return A pseudo random number using xor shift algorithm on 'state'.
 */
GTS_INLINE uint32_t fastRand(uint32_t& state)
{
    GTS_ASSERT(state != 0);
    uint32_t x = state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state = x;
    return x;
}

//------------------------------------------------------------------------------
/**
 * @return input % ceil
 * @remark See: CppCon 2015: Chandler Carruth "Tuning C++: Benchmarks, and CPUs, and Compilers! Oh My!"
 */
GTS_INLINE uint32_t fastMod(const uint32_t input, const uint32_t ceil)
{
    // apply the modulo operator only when needed
    // (i.e. when the input is greater than the ceiling)
    return input >= ceil ? input % ceil : input;
}

//------------------------------------------------------------------------------
/**
* @return True if 'ptr' is align to 'alignment', false otherwise.
*/
GTS_INLINE bool isAligned(void* ptr, size_t alignment)
{
    return ((uintptr_t)ptr & (alignment - 1)) == 0;
}

} // namespace gts
