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
#ifdef _MSC_VER

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdlib>

#include <Windows.h>
#include <Shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

namespace testing {

//------------------------------------------------------------------------------
TEST(GtsMallocRedirect, testDynamicRedirection)
{
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;

    ZeroMemory( &si, sizeof(si) );
    si.cb = sizeof(si);
    ZeroMemory( &pi, sizeof(pi) );

    // Set the working directory to be the same as this executable.
    char buff[MAX_PATH];
    GetModuleFileNameA(0, buff, MAX_PATH);
    PathRemoveFileSpecA(buff);
    SetCurrentDirectoryA(buff);

    if (!CreateProcessA(
        "test_malloc_proc.exe", // No module name (use command line)
        0,                      // Command line
        NULL,                   // Process handle not inheritable
        NULL,                   // Thread handle not inheritable
        FALSE,                  // Set handle inheritance to FALSE
        0,                      // No creation flags
        NULL,                   // Use parent's environment block
        NULL,                   // Use parent's starting directory 
        &si,                    // Pointer to STARTUPINFO structure
        &pi                     // Pointer to PROCESS_INFORMATION structure
    ))
    {
        DWORD error = GetLastError();
        printf("%d\n", error);
        ASSERT_TRUE(false);
    }

    // Wait until child process exits.
    WaitForSingleObject( pi.hProcess, INFINITE );

    // Close process and thread handles. 
    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );
}

} // namespace testing

#endif
