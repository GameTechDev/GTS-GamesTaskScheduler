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

 ////////////////////////////////////////////////////////////////////////////////
 // COMPILER:

#define GTS_TOKENPASTE(x, y) x ## y
#define GTS_TOKENPASTE2(x, y) GTS_TOKENPASTE(x, y)

#define GTS_UNREFERENCED_PARAM(x) (void) ((const volatile void *) &(x))

#if defined(_MSC_VER)

    #if _MSC_VER < 1800
        #error "Requires at least Visual C++ compiler 18 (VS 2013)"
    #endif

    #define GTS_MSVC 1

    #if defined(_M_AMD64)
        #define GTS_ARCH_X64 1
        #define GTS_ARCH_X86 1
    #else
        #define GTS_ARCH_X86 1
    #endif 

#elif defined(__GNUG__)

    // C++11 support
    #if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 8)
        #error "Requires at least GCC compiler 4.8"
    #endif

    #define GTS_GCC 1

    #if defined(__x86_64__)
        #define GTS_ARCH_X64 1
        #define GTS_ARCH_X86 1
    #elif defined(__i386__)
        #define GTS_ARCH_X86 1
    #elif defined(__aarch64__)
        #define GTS_ARCH_ARM64 1
    #elif defined(__arm__)
        #define GTS_ARCH_ARM32 1
    #endif

#elif defined(__clang__)

    // C++11 support
    #if __clang_major__ < 3 || (__clang_major__ == 3 && ___clang_minor__ < 3)
        #error "Requires at least clang compiler 3.3"
    #endif

    #define GTS_CLANG 1

    #if defined(__x86_64__)
        #define GTS_ARCH_X64 1
        #define GTS_ARCH_X86 1
    #elif defined(__i386__)
        #define GTS_ARCH_X86 1
    #elif defined(__aarch64__)
        #define GTS_ARCH_ARM 1
        #define GTS_ARCH_ARM64 1
    #elif defined(__arm__)
        #define GTS_ARCH_ARM 1
        #define GTS_ARCH_ARM32 1
    #endif

#else

    #error "Compiler unsupported."

#endif


////////////////////////////////////////////////////////////////////////////////
// COMPILER LANGUAGE LEVEL FIXES:

#define MULITCORELIB_USE_STD

#if GTS_MSVC && _MSC_VER < 1900

    typedef signed char         int8_t;
    typedef unsigned char       uint8_t;
    typedef signed short        int16_t;
    typedef unsigned short      uint16_t;
    typedef signed int          int32_t;
    typedef unsigned int        uint32_t;
    typedef signed long long    int64_t;
    typedef unsigned long long  uint64_t;

    #define GTS_ALIGN(alignment) __declspec(align(alignment))
    #define GTS_THREAD_LOCAL __declspec(thread)

#else

    #include <cstdint>

    #define GTS_ALIGN(alignment) alignas(alignment)
    #define GTS_THREAD_LOCAL thread_local

#endif

////////////////////////////////////////////////////////////////////////////////
// ARCH:

#ifdef GTS_ARCH_X86

#define GTS_CACHE_LINE_SIZE 64

// Account for adjacent cache-line prefetch
#define GTS_NO_SHARING_CACHE_LINE_SIZE (GTS_CACHE_LINE_SIZE * 2)

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

#elif GTS_GCC

#include <intrin.h>

#define GTS_INLINE inline
#define GTS_FORCE_INLINE __attribute__((always_inline))
#define GTS_NO_INLINE __attribute__ ((noinline))
#define GTS_DEBUG_BREAK() __builtin_trap()

#elif GTS_CLANG

#include <intrin.h>

#define GTS_INLINE inline
#define GTS_FORCE_INLINE __attribute__((always_inline))
#define GTS_NO_INLINE __attribute__ ((noinline))
#define GTS_DEBUG_BREAK() __builtin_trap()

#else

#define GTS_INLINE inline
#define GTS_FORECE_INLINE
#define GTS_NO_INLINE
#define GTS_DEBUG_BREAK() #error "not implemented"

#endif

namespace gts {

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

//------------------------------------------------------------------------------
GTS_INLINE void serializeCPU()
{
#ifdef GTS_ARCH_X86
    _mm_lfence();
#elif GTS_ARCH_ARM
    __isb();
#else
    #error "not implemented"
#endif
}

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

} // namespace gts


#define GTS_PAUSE() gts::pause()
#define GTS_RDTSC() gts::rdtsc()
#define GTS_SERIALIZE_CPU() gts::serializeCPU()
#define GTS_MFENCE() gts::memoryFence()


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

#ifdef GTS_WINDOWS

    #define GTS_ALIGNED_MALLOC(size, alignment) _aligned_malloc(size, alignment)
    #define GTS_ALIGNED_FREE(ptr) _aligned_free(ptr)

#elif GTS_LINUX || GTS_APPLE

    #define GTS_ALIGNED_MALLOC(size, alignment) posix_memalign(size, alignment)
    #define GTS_ALIGNED_FREE(ptr) free(ptr)

#else

    #define GTS_ALIGNED_MALLOC #error "not implemented"
    #define GTS_ALIGNED_FREE #error "not implemented"

#endif
