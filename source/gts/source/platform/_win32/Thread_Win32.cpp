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
#include "gts/platform/Thread.h"

#include <Windows.h>
#include <processthreadsapi.h>

namespace gts {

static_assert(sizeof(ThreadHandle)        == sizeof(HANDLE),              "ThreadHandle size mismatch.");
static_assert(sizeof(MutexHandle)         == sizeof(CRITICAL_SECTION),    "MutexHandle size mismatch.");
static_assert(sizeof(ConditionVarHandle)  == sizeof(CONDITION_VARIABLE),  "ConditionVarHandle size mismatch.");
static_assert(sizeof(ThreadId)            == sizeof(DWORD),               "ThreadId size mismatch.");

static_assert(alignof(ThreadHandle)       == alignof(HANDLE),             "ThreadHandle alignment mismatch.");
static_assert(alignof(MutexHandle)        == alignof(CRITICAL_SECTION),   "MutexHandle alignment mismatch.");
static_assert(alignof(ConditionVarHandle) == alignof(ConditionVarHandle), "ThreadHandle alignment mismatch.");

//------------------------------------------------------------------------------
inline HANDLE nativeHandle(ThreadHandle const& h) { return (HANDLE)h; }

// Yuck Windows!
static const DWORD MS_VC_EXCEPTION = 0x406D1388;
#pragma pack(push,8)  
struct tagTHREADNAME_INFO
{
    DWORD dwType;       // Must be 0x1000.  
    LPCSTR szName;      // Pointer to name (in user addr space).  
    DWORD dwThreadID;   // Thread ID (-1=caller thread).  
    DWORD dwFlags;      // Reserved for future use, must be zero.  
};
#pragma pack(pop)  
static void SetThreadName(DWORD dwThreadID, const char* threadName) {
    tagTHREADNAME_INFO info;
    info.dwType = 0x1000;
    info.szName = threadName;
    info.dwThreadID = dwThreadID;
    info.dwFlags = 0;
#pragma warning(push)  
#pragma warning(disable: 6320 6322)  
    __try {
        RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
    }
#pragma warning(pop)  
}

//------------------------------------------------------------------------------
static uintptr_t setAffinity(HANDLE hThread, uintptr_t mask)
{
    return ::SetThreadAffinityMask(hThread, mask);
}

//------------------------------------------------------------------------------
bool setPriority(HANDLE hThread, Thread::PriorityLevel priority)
{
    int translatedPriority = 0;

    switch (priority)
    {
    case Thread::PRIORITY_TIME_CRITICAL:
        translatedPriority = THREAD_PRIORITY_TIME_CRITICAL;
        break;

    case Thread::PRIORITY_HIGH:
        translatedPriority = THREAD_PRIORITY_HIGHEST;
        break;

    case Thread::PRIORITY_NORMAL:
        translatedPriority = THREAD_PRIORITY_NORMAL;
        break;

    case Thread::PRIORITY_LOW:
        translatedPriority = THREAD_PRIORITY_LOWEST;
        break;
    }
    return ::SetThreadPriority(hThread, translatedPriority) == TRUE;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Thread:

//------------------------------------------------------------------------------
Thread::Thread()
{
    m_handle = NULL;
}

//------------------------------------------------------------------------------
Thread::~Thread()
{
    destroy();
}

//------------------------------------------------------------------------------
bool Thread::start(ThreadFunction function, void* pArg)
{
    if (nativeHandle(m_handle) != NULL)
    {
        GTS_ASSERT(0);
        return false;
    }

    m_handle = ::CreateThread(NULL, 0, function, pArg, 0, NULL);
    GTS_ASSERT(m_handle != NULL);
    return m_handle != NULL;
}

//------------------------------------------------------------------------------
bool Thread::join()
{
    GTS_ASSERT(nativeHandle(m_handle) != NULL);
    return ::WaitForSingleObject(nativeHandle(m_handle), INFINITE) != WAIT_FAILED;
}

//------------------------------------------------------------------------------
bool Thread::destroy()
{
    BOOL result = true;
    if (nativeHandle(m_handle) != NULL)
    {
        result = ::CloseHandle(nativeHandle(m_handle));
        GTS_ASSERT(result != 0);
        m_handle = nullptr;
    }
    return true;
}

//------------------------------------------------------------------------------
ThreadId Thread::getId() const
{
    GTS_ASSERT(nativeHandle(m_handle) != NULL);
    return ::GetThreadId(nativeHandle(m_handle));
}

//------------------------------------------------------------------------------
uintptr_t Thread::setAffinity(uintptr_t mask)
{
    return gts::setAffinity(nativeHandle(m_handle), mask);
}

//------------------------------------------------------------------------------
bool Thread::setPriority(PriorityLevel priority)
{
    GTS_ASSERT(nativeHandle(m_handle) != NULL);
    return gts::setPriority(nativeHandle(m_handle), priority);
}

//------------------------------------------------------------------------------
void Thread::setName(const char* name)
{
    SetThreadName(getId(), name);
}

//------------------------------------------------------------------------------
void Thread::getCpuTopology(CpuTopology& out)
{
    out.coreInfo.clear();

    BOOL done = FALSE;
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer = NULL;
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION ptr = NULL;
    DWORD returnLength = 0;

    while (!done)
    {
        const DWORD rc = ::GetLogicalProcessorInformation(buffer, &returnLength);

        if (FALSE == rc)
        {
            GTS_ASSERT(GetLastError() == ERROR_INSUFFICIENT_BUFFER);

            if (buffer)
            {
                free(buffer);
            }

            buffer = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)malloc(returnLength);
        }
        else
        {
            done = TRUE;
        }
    }

    DWORD byteOffset = 0;
    ptr = buffer;

    while ((ptr != NULL) && (byteOffset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) <= returnLength))
    {
        if (ptr->Relationship == RelationProcessorCore)
        {
            gts::Vector<uintptr_t> affinityMasks;
            for (size_t ii = 0; ii < sizeof(ULONG_PTR) * 8; ++ii)
            {
                ULONG_PTR bit = 1;
                ULONG_PTR mask = bit << ii;
                if (mask & ptr->ProcessorMask)
                {
                    affinityMasks.push_back(mask);
                }
            }
            affinityMasks.shrink_to_fit();
            out.coreInfo.push_back(CpuCoreInfo{ affinityMasks });
        }
        byteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
        ptr++;
    }

    if (buffer)
    {
        free(buffer);
    }
}

//------------------------------------------------------------------------------
uint32_t Thread::getHardwareThreadCount()
{
    ::SYSTEM_INFO info = {};
    ::GetSystemInfo(&info);
    return (uint32_t)info.dwNumberOfProcessors;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// ThisThread:

//------------------------------------------------------------------------------
void ThisThread::yield()
{
    ::SwitchToThread();
}

//------------------------------------------------------------------------------
void ThisThread::sleepFor(size_t milliseconds)
{
    ::Sleep((DWORD)milliseconds);
}

//------------------------------------------------------------------------------
uintptr_t ThisThread::setAffinity(uintptr_t mask)
{
    HANDLE hThread = ::GetCurrentThread();
    GTS_ASSERT(hThread != NULL);
    return gts::setAffinity(hThread, mask);
}

//------------------------------------------------------------------------------
bool ThisThread::setPriority(Thread::PriorityLevel priority)
{
    HANDLE hThread = ::GetCurrentThread();
    GTS_ASSERT(hThread != NULL);
    return gts::setPriority(hThread, priority);
}

//------------------------------------------------------------------------------
ThreadId ThisThread::getId()
{
    return GetCurrentThreadId();
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Mutex:

//------------------------------------------------------------------------------
Mutex::Mutex(uint32_t spinCount)
{
    ::InitializeCriticalSectionAndSpinCount((CRITICAL_SECTION*)&m_criticalSection, spinCount);
}

//------------------------------------------------------------------------------
Mutex::~Mutex()
{
    ::DeleteCriticalSection((CRITICAL_SECTION*)&m_criticalSection);
}

//------------------------------------------------------------------------------
void Mutex::lock()
{
    ::EnterCriticalSection((CRITICAL_SECTION*)&m_criticalSection);
}

//------------------------------------------------------------------------------
void Mutex::unlock()
{
    ::LeaveCriticalSection((CRITICAL_SECTION*)&m_criticalSection);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// BinarySemaphore:

//------------------------------------------------------------------------------
BinarySemaphore::BinarySemaphore()
{
    m_event = CreateEventEx(NULL, NULL,
        CREATE_EVENT_INITIAL_SET | CREATE_EVENT_MANUAL_RESET, // manual reset for isWaiting.
        EVENT_ALL_ACCESS);

    GTS_ASSERT(m_event != NULL);
}

//------------------------------------------------------------------------------
BinarySemaphore::~BinarySemaphore()
{
    ::CloseHandle(m_event);
}

//------------------------------------------------------------------------------
bool BinarySemaphore::isWaiting() const
{
    DWORD result = WaitForSingleObjectEx(m_event, 0, FALSE);
    return WAIT_TIMEOUT == result;
}

//------------------------------------------------------------------------------
void BinarySemaphore::wait()
{
    BOOL success = ResetEvent(m_event);
    GTS_UNREFERENCED_PARAM(success);
    GTS_ASSERT(success != 0);

    DWORD result = WaitForSingleObjectEx(m_event, INFINITE, FALSE);
    GTS_UNREFERENCED_PARAM(result);
    GTS_ASSERT(result != WAIT_FAILED);
}

//------------------------------------------------------------------------------
void BinarySemaphore::signal()
{
    BOOL success = SetEvent(m_event);
    GTS_UNREFERENCED_PARAM(success);
    GTS_ASSERT(success != 0);
}

} // namespace gts
