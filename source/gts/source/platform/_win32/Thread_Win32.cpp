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

#ifdef GTS_WINDOWS
#ifndef GTS_HAS_CUSTOM_THREAD_WRAPPERS

#include <map>

#include <Windows.h>
#include <processthreadsapi.h>

namespace {

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
GTS_INLINE HANDLE nativeHandle(gts::ThreadHandle& h) { return (HANDLE)h; }


struct ThreadData
{
    gts::ThreadFunction func;
    void* pArg;
};

//------------------------------------------------------------------------------
DWORD WINAPI Win32ThreadFunc(LPVOID lpThreadParameter)
{
    ThreadData* pData = (ThreadData*)lpThreadParameter;
    pData->func(pData->pArg);
    GTS_FREE(pData);
    return 0;
}

struct InternalCoreInfo
{
    uintptr_t processorsBitSet;
    uint8_t efficiencyClass;
    bool visited = false;
};

struct InternalSocketeInfo
{
    uintptr_t processorsBitSet;
};

struct InternalNumaNodeInfo
{
    uintptr_t processorsBitSet;
    uint32_t nodeId;
};

struct InternalGroupInfo
{
    gts::Vector<InternalCoreInfo> coreInfo;
    gts::Vector<InternalNumaNodeInfo> numaInfo;
    gts::Vector<InternalSocketeInfo> socketInfo;
    uintptr_t processorsBitSet;
    uint32_t groupId;
};

struct InternalSystemTopology
{
    gts::Vector<InternalGroupInfo> groupInfo;
};

//------------------------------------------------------------------------------
void getProcInfoBuffer(LOGICAL_PROCESSOR_RELATIONSHIP type, PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX* ppBuffer, DWORD* len)
{
    bool done = false;

    while (!done)
    {
        BOOL result = ::GetLogicalProcessorInformationEx(type, (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)*ppBuffer, len);

        if (result == FALSE)
        {
            GTS_ASSERT(GetLastError() == ERROR_INSUFFICIENT_BUFFER);

            if (*ppBuffer)
            {
                free(*ppBuffer);
            }

            *ppBuffer = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)malloc(*len);
        }
        else
        {
            done = true;
        }
    }
}

//------------------------------------------------------------------------------
size_t countSetBits(uintptr_t bitSet)
{
    size_t count = 0;
    for(size_t ii = 0; ii < sizeof(KAFFINITY) * 8; ++ii)
    {
        uintptr_t affinity = (uintptr_t(1) << ii) & bitSet;
        if(affinity != 0)
        {
            count++;
        }
    }
    return count;
}

//------------------------------------------------------------------------------
void getGroupInfo(InternalSystemTopology& out)
{
    DWORD bufferLen = 0;
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX pBuffer = NULL;
    getProcInfoBuffer(RelationGroup, &pBuffer, &bufferLen);

    DWORD bytesVisited = 0;
    uint8_t* ptr = (uint8_t*)pBuffer;

    do
    {
        PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX pInfo = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)ptr;
        GTS_ASSERT(pInfo->Relationship == RelationGroup);

        out.groupInfo.resize(pInfo->Group.ActiveGroupCount);
        for (WORD iGroup = 0; iGroup < pInfo->Group.ActiveGroupCount; ++iGroup)
        {
            GTS_ASSERT(sizeof(uintptr_t) * 8 >= pInfo->Group.GroupInfo->MaximumProcessorCount && "Cannot used a 64 HW Thread CPU on a 32-bit machine.");
            out.groupInfo[iGroup].groupId = iGroup;
            out.groupInfo[iGroup].processorsBitSet = pInfo->Group.GroupInfo->ActiveProcessorMask;
        }

        bytesVisited += pInfo->Size;
        ptr += pInfo->Size;

    } while(bytesVisited < bufferLen);

    free(pBuffer);
}

//------------------------------------------------------------------------------
void getCoreInfo(
    InternalSystemTopology& out,
    std::map<WORD, std::map<uintptr_t, size_t>>& coreIndexByAffinityByGroup,
    std::map<WORD, gts::Vector<InternalCoreInfo>>& coreInfoByIndexByGroup)
{
    DWORD bufferLen = 0;
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX pBuffer = NULL;
    getProcInfoBuffer(RelationProcessorCore, &pBuffer, &bufferLen);

    DWORD bytesVisited = 0;
    uint8_t* ptr = (uint8_t*)pBuffer;

    do
    {
        PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX pInfo = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)ptr;
        GTS_ASSERT(pInfo->Relationship == RelationProcessorCore);

        InternalCoreInfo coreInfo;
        coreInfo.efficiencyClass = pInfo->Processor.EfficiencyClass;

        GTS_ASSERT(pInfo->Processor.GroupCount == 1);

        coreInfo.processorsBitSet = pInfo->Processor.GroupMask[0].Mask;
        size_t index = coreInfoByIndexByGroup[pInfo->Processor.GroupMask[0].Group].size();
        coreInfoByIndexByGroup[pInfo->Processor.GroupMask[0].Group].push_back(coreInfo);

        for(size_t ii = 0; ii < sizeof(KAFFINITY) * 8; ++ii)
        {
            uintptr_t affinity = (uintptr_t(1) << ii) & pInfo->Processor.GroupMask[0].Mask;
            if(affinity != 0)
            {
                coreIndexByAffinityByGroup[pInfo->Processor.GroupMask[0].Group][affinity] = index;
            }
        }

        bytesVisited += pInfo->Size;
        ptr += pInfo->Size;

    } while(bytesVisited < bufferLen);

    // Map core info to groups.
    for (size_t iGroup = 0; iGroup < out.groupInfo.size(); ++iGroup)
    {
        auto& coreInfos = coreInfoByIndexByGroup[(WORD)iGroup];
        for (size_t ii = 0; ii < coreInfos.size(); ++ii)
        {
            out.groupInfo[iGroup].coreInfo.push_back(coreInfos[ii]);
        }
    }

    free(pBuffer);
}

//------------------------------------------------------------------------------
void getNumaInfo(InternalSystemTopology& out)
{
    std::map<size_t, gts::Vector<InternalNumaNodeInfo>> numaInfoMap;

    DWORD bufferLen = 0;
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX pBuffer = NULL;
    getProcInfoBuffer(RelationNumaNode, &pBuffer, &bufferLen);

    DWORD bytesVisited = 0;
    uint8_t* ptr = (uint8_t*)pBuffer;

    do
    {
        PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX pInfo = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)ptr;
        GTS_ASSERT(pInfo->Relationship == RelationNumaNode);

        InternalNumaNodeInfo numaInfo;
        numaInfo.nodeId = pInfo->NumaNode.NodeNumber;
        numaInfo.processorsBitSet = pInfo->NumaNode.GroupMask.Mask;
        numaInfoMap[pInfo->NumaNode.GroupMask.Group].push_back(numaInfo);

        bytesVisited += pInfo->Size;
        ptr += pInfo->Size;

    } while(bytesVisited < bufferLen);

    for (size_t iGroup = 0; iGroup < out.groupInfo.size(); ++iGroup)
    {
        auto& numaInfos = numaInfoMap[iGroup];
        for (size_t ii = 0; ii < numaInfos.size(); ++ii)
        {
            out.groupInfo[iGroup].numaInfo.push_back(numaInfos[ii]);
        }
    }

    free(pBuffer);
}

//------------------------------------------------------------------------------
void getSocketInfo(InternalSystemTopology& out)
{
    std::map<WORD, gts::Vector<InternalSocketeInfo>> socketInfoMap;

    DWORD bufferLen = 0;
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX pBuffer = NULL;
    getProcInfoBuffer(RelationProcessorPackage, &pBuffer, &bufferLen);

    DWORD bytesVisited = 0;
    uint8_t* ptr = (uint8_t*)pBuffer;

    do
    {
        PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX pInfo = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)ptr;
        GTS_ASSERT(pInfo->Relationship == RelationProcessorPackage);

        InternalSocketeInfo socketInfo;

        for (WORD iGroup = 0; iGroup < pInfo->Processor.GroupCount; ++iGroup)
        {
            socketInfo.processorsBitSet = pInfo->Processor.GroupMask[iGroup].Mask;
            socketInfoMap[pInfo->Processor.GroupMask[iGroup].Group].push_back(socketInfo);
        }

        bytesVisited += pInfo->Size;
        ptr += pInfo->Size;

    } while(bytesVisited < bufferLen);

    for (size_t iGroup = 0; iGroup < out.groupInfo.size(); ++iGroup)
    {
        auto& socketInfos = socketInfoMap[(WORD)iGroup];
        for (size_t ii = 0; ii < socketInfos.size(); ++ii)
        {
            out.groupInfo[iGroup].socketInfo.push_back(socketInfos[ii]);
        }
    }

    free(pBuffer);
}

} // namespace

#endif // GTS_HAS_CUSTOM_THREAD_WRAPPERS

namespace gts {
namespace internal {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Thread:

#ifndef GTS_HAS_CUSTOM_THREAD_WRAPPERS

static_assert(sizeof(ThreadHandle)  == sizeof(HANDLE), "ThreadHandle& size mismatch.");
static_assert(sizeof(ThreadId)      == sizeof(DWORD), "ThreadId size mismatch.");
static_assert(alignof(ThreadHandle) == alignof(HANDLE), "ThreadHandle& alignment mismatch.");

//------------------------------------------------------------------------------
bool Thread::createThread(ThreadHandle& handle, ThreadId&, ThreadFunction function, void* pArg, size_t stackSize)
{
    if (nativeHandle(handle) != NULL)
    {
        GTS_ASSERT(0);
        return false;
    }

    ThreadData* pThreadData = (ThreadData*)GTS_MALLOC(sizeof(ThreadData));
    pThreadData->func = function;
    pThreadData->pArg = pArg;

    handle = ::CreateThread(NULL, stackSize, Win32ThreadFunc, pThreadData, 0, NULL);
    GTS_ASSERT(handle != NULL);
    return handle != NULL;
}

//------------------------------------------------------------------------------
bool Thread::join(ThreadHandle& handle)
{
    GTS_ASSERT(nativeHandle(handle) != NULL);
    return ::WaitForSingleObject(nativeHandle(handle), INFINITE) != WAIT_FAILED;
}

//------------------------------------------------------------------------------
bool Thread::destroy(ThreadHandle& handle)
{
    BOOL result = true;
    if (nativeHandle(handle) != NULL)
    {
        result = ::CloseHandle(nativeHandle(handle));
        GTS_ASSERT(result != 0);
        handle = nullptr;
    }
    return true;
}

//------------------------------------------------------------------------------
bool Thread::setGroupAffinity(ThreadHandle& handle, size_t groupId, AffinitySet const& affinity)
{
    InternalSystemTopology topo;
    getGroupInfo(topo);

    if (groupId > topo.groupInfo.size())
    {
        GTS_ASSERT(0 && "Unknown group ID.");
        return false;
    }

    uintptr_t affinityMask = *(uintptr_t*)&affinity;

    if (affinityMask > topo.groupInfo[groupId].processorsBitSet)
    {
        if (affinityMask == GTS_THREAD_AFFINITY_ALL)
        {
            affinityMask = topo.groupInfo[groupId].processorsBitSet;
        }
        else
        {
            GTS_ASSERT(0 && "Unknown affinity mask.");
            return false;
        }
    }

    GROUP_AFFINITY groupAffinity = {0};
    groupAffinity.Mask = affinityMask;
    groupAffinity.Group = (WORD)groupId;
    BOOL result = ::SetThreadGroupAffinity(nativeHandle(handle), &groupAffinity, NULL);
    GTS_ASSERT(result != 0);
    return result != 0;
}

//------------------------------------------------------------------------------
bool Thread::setPriority(ThreadId&, int32_t priority)
{
    HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, ::GetCurrentThreadId());
    DWORD err = GetLastError(); err;
    GTS_ASSERT(hThread != NULL);
    BOOL result = ::SetThreadPriority(hThread, priority);
    GTS_ASSERT(result != 0);
    CloseHandle(hThread);
    return result;
}

//------------------------------------------------------------------------------
void Thread::setThisThreadName(const char* name)
{
    SetThreadName(GetCurrentThreadId(), name);
}

//------------------------------------------------------------------------------
void Thread::getSytemTopology(SystemTopology& out)
{
    std::map<WORD, std::map<uintptr_t, size_t>> coreIndexByAffinityByGroup;
    std::map<WORD, Vector<InternalCoreInfo>> coreInfoByGroup;
    InternalSystemTopology topo;
    getGroupInfo(topo);
    getCoreInfo(topo, coreIndexByAffinityByGroup, coreInfoByGroup);
    getNumaInfo(topo);
    getSocketInfo(topo);

    out.pGroupInfoArray = new ProcessorGroupInfo[topo.groupInfo.size()];
    out.groupInfoElementCount = topo.groupInfo.size();

    ProcessorGroupInfo* pGroupInfo = out.pGroupInfoArray;
    for (WORD iGroup = 0; iGroup < (WORD)topo.groupInfo.size(); ++iGroup)
    {
        auto& group  = topo.groupInfo[iGroup];
        auto& oGroup = pGroupInfo[iGroup];

        size_t numGroupProcs  = countSetBits(group.processorsBitSet);
        oGroup.pCoreInfoArray = new CpuCoreInfo[numGroupProcs];
        oGroup.groupId        = group.groupId;

        {
            size_t numCores = 0;

            for(size_t iCoreBit = 0, coreIdx = 0; iCoreBit < sizeof(KAFFINITY) * 8; ++iCoreBit)
            {
                uintptr_t affinity = (uintptr_t(1) << iCoreBit) & group.processorsBitSet;
                if(affinity != 0)
                {
                    InternalCoreInfo& coreInfo = coreInfoByGroup[iGroup][coreIndexByAffinityByGroup[iGroup][affinity]];
                    if (coreInfo.visited)
                    {
                        continue;
                    }
                    coreInfo.visited                = true;
                    CpuCoreInfo& oCoreInfo          = oGroup.pCoreInfoArray[coreIdx++];
                    oCoreInfo.efficiencyClass       = coreInfo.efficiencyClass;
                    size_t numHwThreads             = countSetBits(coreInfo.processorsBitSet);
                    oCoreInfo.hardwareThreadIdCount = numHwThreads;
                    oCoreInfo.pHardwareThreadIds    = new uint32_t[numHwThreads];
                    for (uint32_t iThreadBit = 0, threadIdx = 0; iThreadBit < sizeof(KAFFINITY) * 8; ++iThreadBit)
                    {
                        affinity = (uintptr_t(1) << iThreadBit) & coreInfo.processorsBitSet;
                        if(affinity != 0)
                        {
                            oCoreInfo.pHardwareThreadIds[threadIdx++] = iThreadBit;
                        }
                    }
                    numCores++;
                }
            }

            oGroup.coreInfoElementCount = numCores;
        }

        for (size_t ii = 0; ii < coreInfoByGroup[iGroup].size(); ++ii)
        {
            coreInfoByGroup[iGroup][ii].visited = false;
        }

        oGroup.pNumaInfoArray = new NumaNodeInfo[group.numaInfo.size()];
        oGroup.numaNodeInfoElementCount = group.numaInfo.size();

        for (size_t iNuma = 0; iNuma < group.numaInfo.size(); ++iNuma)
        {
            auto& numaInfo   = group.numaInfo[iNuma];
            auto& oNumaInfo  = oGroup.pNumaInfoArray[iNuma];

            size_t numNumaProcs      = countSetBits(numaInfo.processorsBitSet);
            oNumaInfo.pCoreInfoArray = new CpuCoreInfo[numNumaProcs];
            oNumaInfo.nodeId         = numaInfo.nodeId;

            size_t numCores = 0;

            for(size_t iCoreBit = 0, coreIdx = 0; iCoreBit < sizeof(KAFFINITY) * 8; ++iCoreBit)
            {
                uintptr_t affinity = (uintptr_t(1) << iCoreBit) & numaInfo.processorsBitSet;
                if(affinity != 0)
                {
                    InternalCoreInfo& coreInfo = coreInfoByGroup[iGroup][coreIndexByAffinityByGroup[iGroup][affinity]];
                    if (coreInfo.visited)
                    {
                        continue;
                    }
                    coreInfo.visited                = true;
                    CpuCoreInfo& oCoreInfo          = oNumaInfo.pCoreInfoArray[coreIdx++];
                    oCoreInfo.efficiencyClass       = coreInfo.efficiencyClass;
                    size_t numHwThreads             = countSetBits(coreInfo.processorsBitSet);
                    oCoreInfo.hardwareThreadIdCount = numHwThreads;
                    oCoreInfo.pHardwareThreadIds    = new uint32_t[numHwThreads];
                    for (uint32_t iThreadBit = 0, threadIdx = 0; iThreadBit < sizeof(KAFFINITY) * 8; ++iThreadBit)
                    {
                        affinity = (uintptr_t(1) << iThreadBit) & coreInfo.processorsBitSet;
                        if(affinity != 0)
                        {
                            oCoreInfo.pHardwareThreadIds[threadIdx++] = iThreadBit;
                        }
                    }
                    numCores++;
                }
            }
            oNumaInfo.coreInfoElementCount = numCores;
        }

        for (size_t ii = 0; ii < coreInfoByGroup[iGroup].size(); ++ii)
        {
            coreInfoByGroup[iGroup][ii].visited = false;
        }


        oGroup.pSocketInfoArray = new SocketInfo[group.socketInfo.size()];
        oGroup.socketInfoElementCount = group.socketInfo.size();

        for (size_t iSocket = 0; iSocket < group.socketInfo.size(); ++iSocket)
        {
            auto& socketInfo  = group.socketInfo[iSocket];
            auto& oSocketInfo = oGroup.pSocketInfoArray[iSocket];

            size_t numSocketProcs      = countSetBits(socketInfo.processorsBitSet);
            oSocketInfo.pCoreInfoArray = new CpuCoreInfo[numSocketProcs];

            size_t numCores = 0;

            for(size_t iCoreBit = 0, coreIdx = 0; iCoreBit < sizeof(KAFFINITY) * 8; ++iCoreBit)
            {
                uintptr_t affinity = (uintptr_t(1) << iCoreBit) & socketInfo.processorsBitSet;
                if(affinity != 0)
                {
                    InternalCoreInfo& coreInfo = coreInfoByGroup[iGroup][coreIndexByAffinityByGroup[iGroup][affinity]];
                    if (coreInfo.visited)
                    {
                        continue;
                    }
                    coreInfo.visited                = true;
                    CpuCoreInfo& oCoreInfo          = oSocketInfo.pCoreInfoArray[coreIdx++];
                    oCoreInfo.efficiencyClass       = coreInfo.efficiencyClass;
                    size_t numHwThreads             = countSetBits(coreInfo.processorsBitSet);
                    oCoreInfo.hardwareThreadIdCount = numHwThreads;
                    oCoreInfo.pHardwareThreadIds    = new uint32_t[numHwThreads];
                    for (uint32_t iThreadBit = 0, threadIdx = 0; iThreadBit < sizeof(KAFFINITY) * 8; ++iThreadBit)
                    {
                        affinity = (uintptr_t(1) << iThreadBit) & coreInfo.processorsBitSet;
                        if(affinity != 0)
                        {
                            oCoreInfo.pHardwareThreadIds[threadIdx++] = iThreadBit;
                        }
                    }
                    numCores++;
                }
            }
            oSocketInfo.coreInfoElementCount = numCores;
        }
    }
}

//------------------------------------------------------------------------------
uint32_t Thread::getHardwareThreadCount()
{
    ::SYSTEM_INFO info = {};
    ::GetSystemInfo(&info);
    return (uint32_t)info.dwNumberOfProcessors;
}

//------------------------------------------------------------------------------
void Thread::getCurrentProcessorId(uint32_t& groupId, uint32_t& hwTid)
{
    PROCESSOR_NUMBER procNum;
    GetCurrentProcessorNumberEx(&procNum);
    groupId = procNum.Group;
    hwTid = procNum.Number;
}

//------------------------------------------------------------------------------
void Thread::yield()
{
    ::SwitchToThread();
}

//------------------------------------------------------------------------------
void Thread::sleep(uint32_t milliseconds)
{
    ::Sleep((DWORD)milliseconds);
}

//------------------------------------------------------------------------------
ThreadId Thread::getThisThreadId()
{
    return GetCurrentThreadId();
}

//------------------------------------------------------------------------------
bool Thread::setThisGroupAffinity(size_t groupId, AffinitySet const& affinity)
{
    HANDLE hThead = GetCurrentThread();
    return setGroupAffinity(hThead, groupId, affinity);
}

//------------------------------------------------------------------------------
bool Thread::setThisPriority(int32_t priority)
{
    HANDLE hThread = ::GetCurrentThread();
    BOOL result = ::SetThreadPriority(hThread, priority);
    GTS_ASSERT(result != 0);
    return result;
}

#endif // GTS_HAS_CUSTOM_THREAD_WRAPPERS

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Event:

#ifndef GTS_HAS_CUSTOM_EVENT_WRAPPERS

static_assert(sizeof(EventHandle::MutexHandle)  == sizeof(CRITICAL_SECTION), "EventHandle::MutexHandle size mismatch.");
static_assert(alignof(EventHandle::MutexHandle) == alignof(CRITICAL_SECTION), "EventHandle::MutexHandle alignment mismatch.");
static_assert(sizeof(EventHandle::CondVar)      == sizeof(CONDITION_VARIABLE), "EventHandle::MutexHandle size mismatch.");
static_assert(alignof(EventHandle::CondVar)     == alignof(CONDITION_VARIABLE), "EventHandle::MutexHandle alignment mismatch.");

//------------------------------------------------------------------------------
bool Event::createEvent(EventHandle& handle)
{
    InitializeConditionVariable((CONDITION_VARIABLE*)&handle.condVar);
    InitializeCriticalSection((CRITICAL_SECTION*)&handle.mutex);
    return true;
}

//------------------------------------------------------------------------------
bool Event::destroyEvent(EventHandle& handle)
{
    DeleteCriticalSection((CRITICAL_SECTION*)&handle.mutex);
    return true;
}

//------------------------------------------------------------------------------
bool Event::waitForEvent(EventHandle& handle, bool waitForever)
{
    GTS_TRACE_SCOPED_ZONE_P0(analysis::CaptureMask::THREAD_PROFILE, analysis::Color::Purple, "Event waiting");
    if (handle.signaled.load(memory_order::acquire))
    {
        GTS_TRACE_ZONE_MARKER_P0(analysis::CaptureMask::THREAD_PROFILE, analysis::Color::Purple2, "Event is signaled");
        return false;
    }

    EnterCriticalSection((CRITICAL_SECTION*)&handle.mutex);

    if (!waitForever)
    {
        GTS_TRACE_ZONE_MARKER_P0(analysis::CaptureMask::THREAD_PROFILE, analysis::Color::Purple2, "Event waiting check");
        LeaveCriticalSection((CRITICAL_SECTION*)&handle.mutex);
        return handle.waiting;
    }

    handle.waiting = true;

    while (!handle.signaled.load(memory_order::acquire)) // guard against spurious wakes.
    {
        GTS_TRACE_SCOPED_ZONE_P0(analysis::CaptureMask::THREAD_PROFILE, analysis::Color::Purple2, "Event waiting loop");
        BOOL result = SleepConditionVariableCS((CONDITION_VARIABLE*)&handle.condVar, (CRITICAL_SECTION*)&handle.mutex, INFINITE);
        if (result == 0)
        {
            GTS_ASSERT(0);
            LeaveCriticalSection((CRITICAL_SECTION*)&handle.mutex);
            return false;
        }
    }

    handle.waiting = false;

    LeaveCriticalSection((CRITICAL_SECTION*)&handle.mutex);
    return true;
}

//------------------------------------------------------------------------------
bool Event::signalEvent(EventHandle& handle)
{
    GTS_TRACE_SCOPED_ZONE_P0(analysis::CaptureMask::THREAD_PROFILE, analysis::Color::Purple, "Event signaled");
    handle.signaled.exchange(true, memory_order::acq_rel);
    WakeConditionVariable((CONDITION_VARIABLE*)&handle.condVar);
    return true;
}

//------------------------------------------------------------------------------
bool Event::resetEvent(EventHandle& handle)
{
    GTS_TRACE_SCOPED_ZONE_P0(analysis::CaptureMask::THREAD_PROFILE, analysis::Color::Purple, "Event reset");
    handle.signaled.exchange(false, memory_order::acq_rel);
    return true;
}

#endif // GTS_HAS_CUSTOM_EVENT_WRAPPERS

} // namespace internal
} // namespace gts

#endif // GTS_WINDOWS
