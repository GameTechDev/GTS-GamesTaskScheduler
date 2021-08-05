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

#if defined(GTS_USER_CONFIG)
#include // Point to your own header location.
#else
#include "../../user_config.h"
#endif

/**
 * @defgroup Platform
 *  Platform specific functionality
 * @{
 */

/**
 * @defgroup Machine
 *  Machine specific macros.
 * @{
 */

// TODO: Update this with CI
#define GTS_VERSION_MAJOR    0
#define GTS_VERSION_MINOR    1
#define GTS_VERSION_REVISION 0

/** @} */ // end of Machine
/** @} */ // end of Platform

 ////////////////////////////////////////////////////////////////////////////////
 // COMPILER:

#define GTS_TOKENPASTE(x, y) x ## y
#define GTS_TOKENPASTE2(x, y) GTS_TOKENPASTE(x, y)

#define GTS_UNREFERENCED_PARAM(x) (void)(x)

#if defined(_MSC_VER)

    #if _MSC_VER < 1900
        #error "Requires at least Visual C++ compiler 18 (VS 2015)"
    #endif

    #define GTS_MSVC 1

    #if defined(_M_AMD64)
        #define GTS_64BIT 1
        #define GTS_ARCH_X64 1
        #define GTS_ARCH_X86 1
    #else
        #define GTS_32BIT 1
        #define GTS_ARCH_X86 1
    #endif

#define GTS_DEPRECATE __declspec(deprecated)

#elif defined(__clang__)

    // C++11 support
    #if __clang_major__ < 3 || (__clang_major__ == 3 && ___clang_minor__ < 3)
        #error "Requires at least clang compiler 3.3"
    #endif

    #define GTS_CLANG 1

    #if defined(__x86_64__)
        #define GTS_64BIT 1
        #define GTS_ARCH_X64 1
        #define GTS_ARCH_X86 1
    #elif defined(__i386__)
        #define GTS_32BIT 1
        #define GTS_ARCH_X86 1
    #elif defined(__aarch64__)
        #define GTS_64BIT 1
        #define GTS_ARCH_ARM 1
        #define GTS_ARCH_ARM64 1
    #elif defined(__arm__)
        #define GTS_32BIT 1
        #define GTS_ARCH_ARM 1
        #define GTS_ARCH_ARM32 1
    #endif

#define GTS_DEPRECATE __attribute__(deprecated)

#elif defined(__GNUG__)

    // C++11 support
    #if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 8)
        #error "Requires at least GCC compiler 4.8"
    #endif

    #define GTS_GCC 1

    #if defined(__x86_64__)
        #define GTS_64BIT 1
        #define GTS_ARCH_X64 1
        #define GTS_ARCH_X86 1
    #elif defined(__i386__)
        #define GTS_32BIT 1
        #define GTS_ARCH_X86 1
    #elif defined(__aarch64__)
        #define GTS_64BIT 1
        #define GTS_ARCH_ARM 1
        #define GTS_ARCH_ARM64 1
    #elif defined(__arm__)
        #define GTS_32BIT 1
        #define GTS_ARCH_ARM 1
        #define GTS_ARCH_ARM32 1
    #endif

#define GTS_DEPRECATE __attribute__(deprecated)

#else

    #error "Compiler unsupported."

#endif


////////////////////////////////////////////////////////////////////////////////
// COMPILER LANGUAGE LEVEL FIXES:

#if GTS_MSVC
    #include <cstdint>
    #define GTS_ALIGN(alignment) alignas(alignment)
    #define GTS_ALIGN_OF(expr) alignof(expr)
    #define GTS_THREAD_LOCAL thread_local
#elif GTS_GCC || GTS_CLANG
    #include <cstdint>
    #define GTS_ALIGN(alignment) alignas(alignment)
    #define GTS_ALIGN_OF(expr) __alignof__(expr)
    #define GTS_THREAD_LOCAL thread_local
#else
    #define GTS_ALIGN(alignment) #error "not implemented"
    #define GTS_ALIGN_OF(expr) #error "not implemented"
    #define GTS_THREAD_LOCAL #error "not implemented"
#endif

////////////////////////////////////////////////////////////////////////////////
// ARCH:

#ifdef GTS_ARCH_X86

#define GTS_CACHE_LINE_SIZE 64

// Account for adjacent cache-line prefetch
#define GTS_NO_SHARING_CACHE_LINE_SIZE (GTS_CACHE_LINE_SIZE * 4)

#else

#define GTS_CACHE_LINE_SIZE 64
#define GTS_NO_SHARING_CACHE_LINE_SIZE 64

#endif

////////////////////////////////////////////////////////////////////////////////
// INTRINSICS:

#ifdef GTS_MSVC

#include <intrin.h>

#define GTS_INLINE inline
#define GTS_FORCE_INLINE __forceinline
#define GTS_NO_INLINE __declspec(noinline)
#define GTS_DEBUG_BREAK() __debugbreak()
#define GTS_NOT_ALIASED __restrict
#define GTS_SHARED_LIB_EXPORT __declspec(dllexport)
#define GTS_SHARED_LIB_IMPORT __declspec(dllimport)
#define GTS_CPU_ID(registers, leaf) __cpuid((int*)registers, leaf);
#define GTS_OPTIMIZE_OFF_BEGIN __pragma(optimize("", off))
#define GTS_OPTIMIZE_OFF_END __pragma(optimize( "", on ))

#elif GTS_GCC || GTS_CLANG

#include <x86intrin.h>
#include <cpuid.h>

#define GTS_INLINE inline
#define GTS_FORCE_INLINE __attribute__((always_inline))
#define GTS_NO_INLINE __attribute__ ((noinline))
#define GTS_DEBUG_BREAK() __builtin_trap()
#define GTS_NOT_ALIASED __restrict__
#define GTS_SHARED_LIB_EXPORT __attribute__((visibility("default")))
#define GTS_SHARED_LIB_IMPORT
#define GTS_CPU_ID(registers, leaf) __get_cpuid(leaf, registers, registers + 1, registers + 2, registers + 3);
#define GTS_OPTIMIZE_OFF_BEGIN \
    _Pragma("GCC push_options") \
    _Pragma("GCC optimize (\"O0\")")
#define GTS_OPTIMIZE_OFF_END _Pragma("GCC pop_options")

#else

#define GTS_INLINE inline
#define GTS_FORECE_INLINE
#define GTS_NO_INLINE
#define GTS_DEBUG_BREAK() #error "not implemented"
#define GTS_NOT_ALIASED
#define GTS_SHARED_LIB_EXPORT #error "not implemented"
#define GTS_SHARED_LIB_IMPORT #error "not implemented"
#define GTS_OPTIMIZE_OFF_BEGIN #error "not implemented"
#define GTS_OPTIMIZE_OFF_END #error "not implemented"

#endif


#ifndef GTS_HAS_CUSTOM_CPU_INTRINSICS_WRAPPERS
namespace gts {

constexpr uint32_t WAITPKG_OPTIMIZE_POWER = 0;
constexpr uint32_t WAITPKG_OPTIMIZE_SPEED = 1;

//------------------------------------------------------------------------------
GTS_INLINE void tpause(uint32_t control, uint64_t cycles)
{
#ifdef GTS_ARCH_X86
    _tpause(control, cycles);
#else
    #error "not implemented"
#endif
}
#define GTS_TPAUSE(control, cycles) gts::tpause(control, cycles)

//------------------------------------------------------------------------------
GTS_INLINE void pause()
{
#ifdef GTS_ARCH_X86
    _mm_pause();
#elif GTS_ARCH_ARM
    __yield();
#else
    #error "not implemented"
#endif
}
#define GTS_PAUSE() gts::pause()

//------------------------------------------------------------------------------
GTS_INLINE uint64_t rdtsc()
{
#ifdef GTS_ARCH_X86
    return __rdtsc();
#elif GTS_ARCH_ARM
    // https://github.com/google/benchmark/blob/v1.1.0/src/cycleclock.h#L116
    uint32_t pmccntr;
    uint32_t pmuseren;
    uint32_t pmcntenset;
    // Read the user mode perf monitor counter access permissions.
    asm volatile("mrc p15, 0, %0, c9, c14, 0" : "=r"(pmuseren));
    if (pmuseren & 1) {  // Allows reading perfmon counters for user mode code.
        asm volatile("mrc p15, 0, %0, c9, c12, 1" : "=r"(pmcntenset));
        if (pmcntenset & 0x80000000ul) {  // Is it counting?
            asm volatile("mrc p15, 0, %0, c9, c13, 0" : "=r"(pmccntr));
            // The counter is set up to count every 64th cycle
            return static_cast<int64_t>(pmccntr) * 64;  // Should optimize to << 6
        }
    }
#else
    #error "not implemented"
#endif
}
#define GTS_RDTSC() gts::rdtsc()

//------------------------------------------------------------------------------
GTS_INLINE void memoryFence()
{
#ifdef GTS_ARCH_X86
    _mm_mfence();
#elif GTS_ARCH_ARM
    __dmb();
#else
    #error "not implemented"
#endif
}
#define GTS_MFENCE() gts::memoryFence()

//------------------------------------------------------------------------------
GTS_INLINE void speculationFence()
{
#ifdef GTS_ARCH_X86
    _mm_lfence();
#elif GTS_ARCH_ARM
    __isb(); // or CSDB?
#else
    #error "not implemented"
#endif
}
#define GTS_SPECULATION_FENCE() gts::speculationFence()

//------------------------------------------------------------------------------
GTS_INLINE uint32_t msbScan(uint32_t bitSet)
{
#ifdef GTS_MSVC
    unsigned long result = 0;
    _BitScanReverse(&result, (unsigned long)bitSet);
    return (uint32_t)result;
#elif (GTS_CLANG || GTS_GCC)
    return 31 - __builtin_clz(bitSet);
#else
    return (uint32_t)::floor(::log2((float)bitSet));
#endif
}
#define GTS_MSB_SCAN(bitSet) gts::msbScan(bitSet)

//------------------------------------------------------------------------------
GTS_INLINE uint64_t msbScan64(uint64_t bitSet)
{
#ifdef GTS_MSVC
    #ifdef GTS_ARCH_X64
        unsigned long result = 0;
        _BitScanReverse64(&result, (unsigned __int64)bitSet);
        return (uint64_t)result;
    #else
        unsigned long result = 0;

        // Check high word.
        _BitScanReverse(&result, (unsigned long)((bitSet & ~UINT32_MAX) >> 32));
        if (result == 0)
        {
            // If no high word, check low word.
            _BitScanReverse(&result, (unsigned long)(bitSet & UINT32_MAX));
        }
        return (uint64_t)result;
    #endif // GTS_ARCH_X64
#elif (GTS_CLANG || GTS_GCC)
    return 63 - __builtin_clzll(bitSet);
#else
    return (uint64_t)::floor(::log2((double)bitSet));
#endif
}
#define GTS_MSB_SCAN64(bitSet) gts::msbScan64(bitSet)


} // namespace gts
#endif // GTS_HAS_CUSTOM_CPU_INTRINSICS_WRAPPERS


////////////////////////////////////////////////////////////////////////////////
// OS:

#if defined(_WIN32) || defined(_WIN64)
    #define GTS_WINDOWS 1
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
#elif defined(__linux__)
    #define GTS_LINUX 1
    #define GTS_POSIX 1
#elif defined(__APPLE__)
    #define GTS_MAC 1
    #define GTS_POSIX 1
#endif
