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
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <thread>

#include "gts/analysis/Trace.h"
#include "gts/platform/Thread.h"
#include "gts/platform/Memory.h"
#include "gts/containers/parallel/BinnedAllocator.h"

#if defined(_MSC_VER)

#include <cstdlib>
#include <crtdbg.h>

#include <Windows.h>

#ifdef _DEBUG
    #define new new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
    // Replace _NORMAL_BLOCK with _CLIENT_BLOCK if you want the
    // allocations to be of _CLIENT_BLOCK type
#else
    #define DBG_NEW new
#endif

//------------------------------------------------------------------------------
LONG WINAPI dumpLogOnSEH(_EXCEPTION_POINTERS *pExceptionInfo)
{
    switch ((size_t)pExceptionInfo->ExceptionRecord->ExceptionCode)
    {
    case EXCEPTION_ACCESS_VIOLATION:
        printf("EXCEPTION_ACCESS_VIOLATION\n");
        printf("Read-write flag: %zu\n", pExceptionInfo->ExceptionRecord->ExceptionInformation[0]);
        printf("At address: %zu\n", pExceptionInfo->ExceptionRecord->ExceptionInformation[1]);
        break;
    case EXCEPTION_DATATYPE_MISALIGNMENT:
        printf("EXCEPTION_DATATYPE_MISALIGNMENT\n");
        break;
    case EXCEPTION_BREAKPOINT:
        printf("EXCEPTION_BREAKPOINT\n");
        break;
    case EXCEPTION_SINGLE_STEP:
        printf("EXCEPTION_SINGLE_STEP\n");
        break;
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        printf("EXCEPTION_ARRAY_BOUNDS_EXCEEDED\n");
        break;
    case EXCEPTION_FLT_DENORMAL_OPERAND:
        printf("EXCEPTION_FLT_DENORMAL_OPERAND\n");
        break;
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        printf("EXCEPTION_FLT_DIVIDE_BY_ZERO\n");
        break;
    case EXCEPTION_FLT_INEXACT_RESULT:
        printf("EXCEPTION_FLT_INEXACT_RESULT\n");
        break;
    case EXCEPTION_FLT_INVALID_OPERATION:
        printf("EXCEPTION_FLT_INVALID_OPERATION\n");
        break;
    case EXCEPTION_FLT_OVERFLOW:
        printf("EXCEPTION_FLT_OVERFLOW\n");
        break;
    case EXCEPTION_FLT_STACK_CHECK:
        printf("EXCEPTION_FLT_STACK_CHECK\n");
        break;
    case EXCEPTION_FLT_UNDERFLOW:
        printf("EXCEPTION_FLT_UNDERFLOW\n");
        break;
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
        printf("EXCEPTION_INT_DIVIDE_BY_ZERO\n");
        break;
    case EXCEPTION_INT_OVERFLOW:
        printf("EXCEPTION_INT_OVERFLOW\n");
        break;
    case EXCEPTION_PRIV_INSTRUCTION:
        printf("EXCEPTION_PRIV_INSTRUCTION\n");
        break;
    case EXCEPTION_IN_PAGE_ERROR:
        printf("EXCEPTION_IN_PAGE_ERROR\n");
        break;
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        printf("EXCEPTION_ILLEGAL_INSTRUCTION\n");
        break;
    case EXCEPTION_NONCONTINUABLE_EXCEPTION:
        printf("EXCEPTION_NONCONTINUABLE_EXCEPTION\n");
        break;
    case EXCEPTION_STACK_OVERFLOW:
        printf("EXCEPTION_STACK_OVERFLOW\n");
        break;
    case EXCEPTION_INVALID_DISPOSITION:
        printf("EXCEPTION_INVALID_DISPOSITION\n");
        break;
    case EXCEPTION_GUARD_PAGE:
        printf("EXCEPTION_GUARD_PAGE\n");
        break;
    case EXCEPTION_INVALID_HANDLE:
        printf("EXCEPTION_INVALID_HANDLE\n");
        break;
    default:
        printf("Unknown Exception: %zu\n", (size_t)pExceptionInfo->ExceptionRecord->ExceptionCode);
    }

    printf("Thread ID: %u\n", gts::ThisThread::getId());

    GTS_TRACE_DUMP("fail.txt");

    return EXCEPTION_EXECUTE_HANDLER;
}

#endif // defined(_MSC_VER)

//------------------------------------------------------------------------------
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);

#if defined(_MSC_VER)
    SetUnhandledExceptionFilter(dumpLogOnSEH);
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    GTS_TRACE_SET_CAPTURE_MASK(gts::analysis::CaptureMask::ALL);

#if 0
    ::testing::GTEST_FLAG(filter) = "CriticalNode_MacroScheduler/ExecutionOrderTest.DiamondDag/0";
    ::testing::GTEST_FLAG(repeat) = 100;
#endif

    return RUN_ALL_TESTS();
}
