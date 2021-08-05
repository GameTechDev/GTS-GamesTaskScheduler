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

#ifdef GTS_POSIX

#include <map>
#include <vector>

#include <pthread.h>
#include <unistd.h>
#include <sched.h>
#include <linux/limits.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/time.h>

#include "gts/platform/Atomic.h"

#ifndef GTS_HAS_CUSTOM_THREAD_WRAPPERS

namespace {

struct ThreadData
{
    gts::ThreadFunction func;
    pid_t* pTid;
    void* pArg;
    gts::Atomic<bool> ready = {false};
};

//------------------------------------------------------------------------------
void* PosixThreadFunc(void* lpThreadParameter)
{
    ThreadData* pData = (ThreadData*)lpThreadParameter;
    *(pData->pTid) = gettid();
    pData->ready.store(true, gts::memory_order::release);
    pData->func(pData->pArg);
    pData->~ThreadData();
    GTS_FREE(pData);
    return 0;
}

struct Core
{
    uint32_t baseFrequency  = UINT32_MAX;
    uint32_t packageId      = UINT32_MAX;
    uint32_t coreId         = UINT32_MAX;
    uint32_t numaId         = UINT32_MAX;
    std::vector<uint32_t> hwThreadsIds;
};

void setPackageId(Core& out, uint32_t coreId)
{
    constexpr size_t MAX_BUFF = PATH_MAX + NAME_MAX;
    char filenameBuffer[MAX_BUFF];
    snprintf(filenameBuffer, MAX_BUFF, "/sys/devices/system/cpu/cpu%d/topology/physical_package_id", coreId);
    FILE* file = fopen(filenameBuffer, "r");
    if(file == nullptr)
    {
        GTS_ASSERT(0 && "No package ID found.");
        return;
    }
    int result = fscanf(file, "%u", &out.packageId);
    GTS_UNREFERENCED_PARAM(result);
    GTS_ASSERT(result > 0);
    fclose(file);
}

void setNumaId(Core& out, uint32_t coreId)
{
    constexpr size_t MAX_BUFF = PATH_MAX + NAME_MAX;
    char filenameBuffer[MAX_BUFF];

    for(uint32_t numaId = 0; numaId < 1024; ++numaId)
    {
        snprintf(filenameBuffer, MAX_BUFF, "/sys/devices/system/cpu/cpu%d/node%d/cpu%d/uevent", coreId, numaId, coreId);
        FILE* file = fopen(filenameBuffer, "r");
        if(file == nullptr)
        {
            continue;
        }
        fclose(file);
        out.numaId = numaId;
        return;
    }

    GTS_ASSERT(0 && "No numa node found.");
}

void setBaseFrequencyId(Core& out, uint32_t coreId)
{
    constexpr size_t MAX_BUFF = PATH_MAX + NAME_MAX;
    char filenameBuffer[MAX_BUFF];
    snprintf(filenameBuffer, MAX_BUFF, "/sys/devices/system/cpu/cpu%d/cpufreq/base_frequency", coreId);
    FILE* file = fopen(filenameBuffer, "r");
    if(file == nullptr)
    {
        GTS_ASSERT(0 && "No package ID found.");
        return;
    }
    int result = fscanf(file, "%u", &out.baseFrequency);
    GTS_UNREFERENCED_PARAM(result);
    GTS_ASSERT(result > 0);
    fclose(file);
}

void setHwThreads(Core& out, uint32_t coreId)
{
    constexpr size_t MAX_BUFF = PATH_MAX + NAME_MAX;
    char filenameBuffer[MAX_BUFF];
    snprintf(filenameBuffer, MAX_BUFF, "/sys/devices/system/cpu/cpu%d/topology/core_cpus_list", coreId);
    FILE* file = fopen(filenameBuffer, "r");
    if(file == nullptr)
    {
        GTS_ASSERT(0 && "No CPUs list found.");
        return;
    }

    while(true)
    {
        uint32_t threadId = 0;
        int result = fscanf(file, "%u", &threadId);
        if(result <= 0)
        {
            break;
        }
        out.hwThreadsIds.push_back(threadId);
        int c = fgetc(file);
        if(c == EOF || c != ',')
        {
            break;
        }
    }

    fclose(file);
}

} // namespace

#endif // GTS_HAS_CUSTOM_THREAD_WRAPPERS

namespace gts {
namespace internal {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Thread:

#ifndef GTS_HAS_CUSTOM_THREAD_WRAPPERS

static_assert(sizeof(ThreadHandle) == sizeof(pthread_t), "ThreadHandle size mismatch.");
static_assert(sizeof(ThreadId) == sizeof(pid_t), "ThreadId size mismatch.");
static_assert(sizeof(AffinitySet) == sizeof(cpu_set_t), "AffinitySet size mismatch.");
static_assert(alignof(ThreadHandle) == alignof(pthread_t), "ThreadHandle alignment mismatch.");
static_assert(alignof(ThreadId) == alignof(pid_t), "ThreadId alignment mismatch.");
static_assert(alignof(AffinitySet) == alignof(cpu_set_t), "AffinitySet alignment mismatch.");

//------------------------------------------------------------------------------
bool Thread::createThread(ThreadHandle& handle, ThreadId& tid, ThreadFunction function, void* pArg, size_t stackSize)
{
    if (pthread_t(handle) != 0)
    {
        GTS_ASSERT(0);
        return false;
    }

    ThreadData* pThreadData = new (GTS_MALLOC(sizeof(ThreadData))) ThreadData();
    pThreadData->func = function;
    pThreadData->pArg = pArg;
    pThreadData->pTid = &tid;

    int result;

    pthread_attr_t attribs;
    pthread_attr_init(&attribs);

    if(stackSize > 0)
    {
        result = pthread_attr_setstacksize(&attribs, stackSize);
        if(result != 0)
        {
            GTS_ASSERT(0);
            return false;
        }
    }

    result = ::pthread_create(&handle, &attribs, PosixThreadFunc, pThreadData);
    GTS_ASSERT(result == 0);

    while(!pThreadData->ready.load(memory_order::acquire))
    {
        GTS_PAUSE();
    }

    return result == 0;
}

//------------------------------------------------------------------------------
bool Thread::join(ThreadHandle& handle)
{
    GTS_ASSERT(pthread_t(handle) != 0);
    int result = ::pthread_join(pthread_t(handle), nullptr);
    return result == 0 || result == ESRCH;
}

//------------------------------------------------------------------------------
bool Thread::destroy(ThreadHandle&)
{
    // NA for Posix.
    return true;
}

//------------------------------------------------------------------------------
bool Thread::setGroupAffinity(ThreadHandle& handle, size_t, AffinitySet const& affinity)
{
    int result = ::pthread_setaffinity_np(pthread_t(handle), sizeof(cpu_set_t), (cpu_set_t*)&affinity);
    GTS_ASSERT(result == 0);
    return result == 0;
}

//------------------------------------------------------------------------------
bool Thread::setPriority(ThreadId& tid, int32_t priority)
{
    // Application including GTS will require CAP_SYS_NICE. Use:
    // sudo setcap 'cap_sys_nice=eip' <application>
    int result = setpriority(PRIO_PROCESS, tid, priority); // NOTE: this is a POSIX violation, so it may not work on non-linux systems.
    GTS_ASSERT(result == 0);
    return result == 0;
}

//------------------------------------------------------------------------------
void Thread::setThisThreadName(const char* name)
{
    if(name != nullptr && name[0] != 0)
    {
        // thread names have a 16 char limit.
        constexpr size_t CHAR_LIMIT = 16;
        char buff[CHAR_LIMIT];
        size_t len = gtsMin(strlen(name), size_t(CHAR_LIMIT - 1));
        memcpy(buff, name, len + 1);

        int result = pthread_setname_np(getThisThread(), buff);
        GTS_UNREFERENCED_PARAM(result);
        GTS_ASSERT(result == 0);
    }
}

//------------------------------------------------------------------------------
void Thread::getSytemTopology(SystemTopology& out)
{
    std::vector<Core> cores;

    constexpr size_t MAX_BUFF = PATH_MAX + NAME_MAX;
    char filenameBuffer[MAX_BUFF];
    uint32_t coreId = 0;
    while(true)
    {
        snprintf(filenameBuffer, MAX_BUFF, "/sys/devices/system/cpu/cpu%d/topology/core_id", coreId);
        FILE* file = fopen(filenameBuffer, "r");
        if(file == nullptr)
        {
            break;
        }

        Core core;

        int result = fscanf(file, "%u", &core.coreId);
        GTS_UNREFERENCED_PARAM(result);
        GTS_ASSERT(result > 0);
        fclose(file);

        setPackageId(core, coreId);
        setNumaId(core, coreId);
        setBaseFrequencyId(core, coreId);
        setHwThreads(core, coreId);

        cores.push_back(core);

        ++coreId;
    }

    // Remove duplicates.
    std::map<uint32_t, Core> coresByFirstHwThread;
    for(size_t ii = 0; ii < cores.size(); ++ii)
    {
        coresByFirstHwThread[cores[ii].hwThreadsIds[0]] = cores[ii];
    }

    // Define the efficiency classes based on frequency.
    std::map<uint32_t, uint32_t> efficiencyByFrequency;
    {
        std::map<uint32_t, uint32_t> orderedFrequencies;
        for(auto iter = coresByFirstHwThread.begin(); iter != coresByFirstHwThread.end(); ++iter)
        {
            orderedFrequencies[iter->second.baseFrequency] = iter->second.baseFrequency;
        }
        std::vector<uint32_t> frequences;
        frequences.reserve(orderedFrequencies.size());
        for(auto iter = orderedFrequencies.begin(); iter != orderedFrequencies.end(); ++iter)
        {
            frequences.push_back(iter->second);
        }
        uint32_t efficiencyClass = 0;
        for(auto iter = frequences.begin(); iter != frequences.end(); ++iter)
        {
            efficiencyByFrequency[efficiencyClass++] = *iter;
        }
    }

    // Map cores to their packages.
    std::map<uint32_t, std::vector<Core>> coresByPackageId;
    for(auto iter = coresByFirstHwThread.begin(); iter != coresByFirstHwThread.end(); ++iter)
    {
        coresByPackageId[iter->second.packageId].push_back(iter->second);
    }

    // Map cores to their NUMA nodes.
    std::map<uint32_t, std::vector<Core>>coresByNumaNode;
    for(auto iter = coresByFirstHwThread.begin(); iter != coresByFirstHwThread.end(); ++iter)
    {
        coresByNumaNode[iter->second.numaId].push_back(iter->second);
    }

    out.pGroupInfoArray = new ProcessorGroupInfo[1];
    out.groupInfoElementCount = 1;
    ProcessorGroupInfo& groupInfo = out.pGroupInfoArray[0];

    // Build socket info.
    groupInfo.pSocketInfoArray = new SocketInfo[coresByPackageId.size()];
    groupInfo.socketInfoElementCount = coresByPackageId.size();
    for(uint32_t iSocket = 0; iSocket < groupInfo.socketInfoElementCount; ++iSocket)
    {
        auto& socket = groupInfo.pSocketInfoArray[iSocket];
        auto& cores = coresByPackageId[iSocket];

        socket.pCoreInfoArray = new CpuCoreInfo[cores.size()];
        socket.coreInfoElementCount = cores.size();
        for(uint32_t iCore = 0; iCore < socket.coreInfoElementCount; ++iCore)
        {
            auto& coreInfo = socket.pCoreInfoArray[iCore];
            coreInfo.efficiencyClass = efficiencyByFrequency[cores[iCore].baseFrequency];
            coreInfo.pHardwareThreadIds = new uint32_t[cores[iCore].hwThreadsIds.size()];
            coreInfo.hardwareThreadIdCount = cores[iCore].hwThreadsIds.size();
            for(uint32_t iThread = 0; iThread < coreInfo.hardwareThreadIdCount; ++iThread)
            {
                coreInfo.pHardwareThreadIds[iThread] = cores[iCore].hwThreadsIds[iThread];
            }
        }
    }

    // Build NUMA info.
    groupInfo.pNumaInfoArray = new NumaNodeInfo[coresByNumaNode.size()];
    groupInfo.numaNodeInfoElementCount = coresByNumaNode.size();
    for(uint32_t iNode = 0; iNode < groupInfo.numaNodeInfoElementCount; ++iNode)
    {
        auto& node = groupInfo.pNumaInfoArray[iNode];
        auto& cores = coresByNumaNode[iNode];

        node.pCoreInfoArray = new CpuCoreInfo[cores.size()];
        node.coreInfoElementCount = cores.size();
        for(uint32_t iCore = 0; iCore < node.coreInfoElementCount; ++iCore)
        {
            auto& coreInfo = node.pCoreInfoArray[iCore];
            coreInfo.efficiencyClass = efficiencyByFrequency[cores[iCore].baseFrequency];
            coreInfo.pHardwareThreadIds = new uint32_t[cores[iCore].hwThreadsIds.size()];
            coreInfo.hardwareThreadIdCount = cores[iCore].hwThreadsIds.size();
            for(uint32_t iThread = 0; iThread < coreInfo.hardwareThreadIdCount; ++iThread)
            {
                coreInfo.pHardwareThreadIds[iThread] = cores[iCore].hwThreadsIds[iThread];
            }
        }
    }

    // Build core info.
    groupInfo.pCoreInfoArray = new CpuCoreInfo[coresByFirstHwThread.size()];
    groupInfo.coreInfoElementCount = coresByFirstHwThread.size();
    for(uint32_t iCore = 0; iCore < groupInfo.coreInfoElementCount; ++iCore)
    {
        auto& coreInfo = groupInfo.pCoreInfoArray[iCore];
        coreInfo.efficiencyClass = efficiencyByFrequency[coresByFirstHwThread[iCore].baseFrequency];
        coreInfo.pHardwareThreadIds = new uint32_t[cores[iCore].hwThreadsIds.size()];
        coreInfo.hardwareThreadIdCount = coresByFirstHwThread[iCore].hwThreadsIds.size();
        for(uint32_t iThread = 0; iThread < coreInfo.hardwareThreadIdCount; ++iThread)
        {
            coreInfo.pHardwareThreadIds[iThread] = coresByFirstHwThread[iCore].hwThreadsIds[iThread];
        }
    }
}

//------------------------------------------------------------------------------
uint32_t Thread::getHardwareThreadCount()
{
    return ::sysconf(_SC_NPROCESSORS_ONLN);
}

//------------------------------------------------------------------------------
void Thread::getCurrentProcessorId(uint32_t& groupId, uint32_t& hwTid)
{
    int result = ::getcpu(&hwTid, &groupId);
    GTS_UNREFERENCED_PARAM(result);
    GTS_ASSERT(0);
}

//------------------------------------------------------------------------------
void Thread::yield()
{
    int result = ::pthread_yield();
    GTS_UNREFERENCED_PARAM(result);
    GTS_ASSERT(result == 0);
}

//------------------------------------------------------------------------------
void Thread::sleep(uint32_t milliseconds)
{
    ::sleep(milliseconds);
}

//------------------------------------------------------------------------------
ThreadId Thread::getThisThreadId()
{
    return ::gettid();
}

//------------------------------------------------------------------------------
ThreadHandle Thread::getThisThread()
{
    return ::pthread_self();
}

//------------------------------------------------------------------------------
bool Thread::setThisGroupAffinity(size_t, AffinitySet const& affinity)
{
    ThreadHandle handle = getThisThread();
    return setGroupAffinity(handle, 0, affinity);
}

//------------------------------------------------------------------------------
bool Thread::setThisPriority(int32_t priority)
{
    ThreadId tid = getThisThreadId();
    return setPriority(tid, priority);
}

#endif // GTS_HAS_CUSTOM_THREAD_WRAPPERS

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Event:

#ifndef GTS_HAS_CUSTOM_EVENT_WRAPPERS

static_assert(sizeof(EventHandle::MutexHandle)  == sizeof(pthread_mutex_t), "EventHandle::Mutex size mismatch.");
static_assert(sizeof(EventHandle::CondVar)      == sizeof(pthread_cond_t), "EventHandle::ConditionVar size mismatch.");
static_assert(alignof(EventHandle::MutexHandle) == alignof(pthread_mutex_t), "EventHandle::Mutex alignment mismatch.");
static_assert(alignof(EventHandle::CondVar)     == alignof(pthread_cond_t), "EventHandle::ConditionVar alignment mismatch.");
static_assert(GTS_WAIT_FOREVER                  == uint32_t(-1), "Value of GTS_WAIT_FOREVER does not match the OS.");

//------------------------------------------------------------------------------
bool Event::createEvent(EventHandle& handle)
{
    int result = pthread_cond_init((pthread_cond_t*)&handle.condVar, 0);
    if(result != 0)
    {
        GTS_ASSERT(0);
        return false;
    }

    result = pthread_mutex_init((pthread_mutex_t*)&handle.mutex, 0);
    GTS_ASSERT(result == 0);
    return result == 0;
}

//------------------------------------------------------------------------------
bool Event::destroyEvent(EventHandle& handle)
{
    int result = pthread_mutex_destroy((pthread_mutex_t*)&handle.mutex);
    if(result != 0)
    {
        GTS_ASSERT(0);
        return false;
    }

    result = pthread_cond_destroy((pthread_cond_t*)&handle.condVar);
    GTS_ASSERT(result == 0);
    return result == 0;
}

//------------------------------------------------------------------------------
bool Event::waitForEvent(EventHandle& handle, bool waitForever)
{
    if (handle.signaled.load(memory_order::acquire))
    {
        return false;
    }

    int result = pthread_mutex_lock((pthread_mutex_t*)&handle.mutex);
    if (result != 0)
    {
        GTS_ASSERT(0);
        return false;
    }

    if (!waitForever)
    {
        result = pthread_mutex_unlock((pthread_mutex_t*)&handle.mutex);
        if (result != 0)
        {
            GTS_ASSERT(0);
            return false;
        }
        return handle.waiting;
    }

    handle.waiting = true;

    while (!handle.signaled.load(memory_order::acquire)) // guard against spurious wakes.
    {
        result = pthread_cond_wait((pthread_cond_t*)&handle.condVar, (pthread_mutex_t*)&handle.mutex);
        if (result != 0)
        {
            GTS_ASSERT(0);
        }

        if (result != 0)
        {
            GTS_ASSERT(0);
            result = pthread_mutex_unlock((pthread_mutex_t*)&handle.mutex);
            if (result != 0)
            {
                GTS_ASSERT(0);
            }
            return false;
        }
    }

    handle.waiting = false;

    result = pthread_mutex_unlock((pthread_mutex_t*)&handle.mutex);
    GTS_ASSERT(result == 0);
    return result == 0;
}

//------------------------------------------------------------------------------
bool Event::signalEvent(EventHandle& handle)
{
    handle.signaled.exchange(true, memory_order::acq_rel);
    int result = pthread_cond_signal((pthread_cond_t*)&handle.condVar);
    GTS_ASSERT(result == 0);
    return result == 0;
}

//------------------------------------------------------------------------------
bool Event::resetEvent(EventHandle& handle)
{
    handle.signaled.exchange(false, memory_order::acq_rel);
    return true;
}

} // namespace internal
} // namespace gts

#endif // GTS_HAS_CUSTOM_EVENT_WRAPPERS
#endif // GTS_POSIX
