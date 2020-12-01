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
 
/** 
 * @defgroup Config Config
 * @brief
 *  Custom plaform overrides.
 * @par
 *  Define any of the below with a custom implementation. If no custom
 *  implementation exists, GTS may provide a default one. These customization
 *  take presedence over all other overrides.
 * @par
 *  NOTE: You can place this file anywhere in your project by updating the
 *  reference location at the top of Machine.h.
 * @{
 */

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// CPU INTRINSICS

/** 
 * @defgroup CpuIntrinsicsWrapper
 *  User defined wrappers for the CPU intrinsics API.
 * @{
 */

#if defined(GTS_HAS_CUSTOM_CPU_INTRINSICS_WRAPPERS) || defined(DOXYGEN_DOCUMENTING)

/**
 * @def GTS_PAUSE()
 * @brief
 *  Pauses the CPU for a few cycles. (i.e. PAUSE on x86.) @endcode
 * @remark
 *  Signature: @code void pause()
 */
#define GTS_PAUSE() #error "Replace with custom definition"

/**
 * @def GTS_RDTSC()
 * @brief
 *  Reads the current time-stamp count.
 * @returns
 *  The time-stamp value. (i.e. RDTSC on x86.) @endcode
 * @remark
 *  Signature: @code uint64_t rdtsc()
 */
#define GTS_RDTSC() #error "Replace with custom definition"

/**
 * @def GTS_MEMORY_FENCE()
 * @brief
 *  Inserts a full memory fence. (i.e. MFENCE on x86.) @endcode
 * @remark
 *  Signature: @code void memoryFence()
 */
#define GTS_MEMORY_FENCE() #error "Replace with custom definition"

/**
 * @def GTS_SPECULATION_FENCE()
 * @brief
 *  Inserts a speculation memory fence. (i.e. LFENCE on x86.) @endcode
 * @remark
 *  Signature: @code void speculationFence()
 */
#define GTS_SPECULATION_FENCE() #error "Replace with custom definition"

/**
 * @def GTS_MSB_SCAN(bitSet)
 * @brief
 *  Gets the most significant bit from the bit set.
 * @remark
 *  Signature: @code uint32_t msbScan(uint32_t bitSet) @endcode
 * @param bitSet
 *  Gets the most significant bit from the bit set.
 */
#define GTS_MSB_SCAN(bitSet) #error "Replace with custom definition"

/**
 * @def GTS_MSB_SCAN64(bitSet)
 * @brief
 *  Gets the most significant bit from the bit set.
 * @remark
 *  Signature: @code uint64_t msbScan64(uint64_t bitSet) @endcode
 * @param bitSet
 *  Gets the most significant bit from the bit set.
 */
#define GTS_MSB_SCAN64(bitSet) #error "Replace with custom definition"

#endif

/** @} */ // end of CpuIntrinsicsWrapper

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// C MEMORY WRAPPERS

/** 
 * @defgroup CMemoryWrapper
 *  User defined wrappers for the C memory management API.
 * @{
 */

#if defined(GTS_HAS_CUSTOM_DYNAMIC_MEMORY_WRAPPERS) || defined(DOXYGEN_DOCUMENTING)

/**
 * @def GTS_MALLOC(size)
 * @brief
 *  Override malloc.
 * @remark
 *  Signature: @code void* malloc(size_t size) @endcode
 * @param size
 *  The size of the memory to allocate.
 * @returns
 *  A pointer to the memory or nullptr on fail.
 */
#define GTS_MALLOC(size) #error "Replace with custom definition"

/**
 * @def GTS_FREE(ptr)
 * @brief
 *  Override free.
 * @remark
 *  Signature: void free(void* ptr) @endcode
 * @param ptr
 *  A pointer to the memory to free that was allocated with GTS_MALLOC.
 */
#define GTS_FREE(ptr) #error "Replace with custom definition"

/**
 * @def GTS_ALIGNED_MALLOC(size, alignment)
 * @brief
 *  Override aligned_malloc.
 * @remark
 *  Signature: @code void* alignedMalloc(size_t size, size_t alignment) @endcode
 * @param size
 *  The size of the memory to allocate.
 * @param alignment
 *  The address alignment of the allocated memory.
 * @returns
 *  A pointer to the memory or nullptr on fail.
 */
#define GTS_ALIGNED_MALLOC(size, alignment) #error "Replace with custom definition"

/**
 * @def GTS_ALIGNED_FREE(ptr)
 * @brief
 *  Override aligned_free.
 * @remark
 *  Signature: @code void alignedfree(void* ptr) @endcode
 * @param ptr
 *  A pointer to the memory to free that was allocated with GTS_ALIGNED_MALLOC.
 */
#define GTS_ALIGNED_FREE(ptr) #error "Replace with custom definition"

#endif

/** @} */ // end of CMemoryWrapper

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// OS MEMORY WRAPPERS

/** 
 * @defgroup OsMemoryWrapper
 *  User defined wrappers for the OS memory API.
 * @{
 */

#if defined(GTS_HAS_CUSTOM_OS_MEMORY_WRAPPERS) || defined(DOXYGEN_DOCUMENTING)

/**
 * @def GTS_GET_OS_ALLOCATION_GRULARITY()
 * @remark
 *  Signature: @code size_t getAllocationGranularity() @endcode
 * @returns
 *  The allocation granularity for GTS_OS_VIRTUAL_ALLOCATE.
 */
#define GTS_GET_OS_ALLOCATION_GRULARITY() #error "Replace with custom definition"

/**
 * @def GTS_GET_OS_PAGE_SIZE()
 * @remark
 *  Signature: @code size_t getPageSize() @endcode
 * @returns
 *  The commit granularity for GTS_OS_VIRTUAL_COMMIT.
 */
#define GTS_GET_OS_PAGE_SIZE() #error "Replace with custom definition"

/**
 * @def GTS_GET_OS_LARGE_PAGE_SIZE()
 * @remark
 *  Signature: @code size_t getLargePageSize() @endcode
 * @returns
 *  The allocation granularity for Large Pages. Zero if large pages are not
 *  supported.
 */
#define GTS_GET_OS_LARGE_PAGE_SIZE() #error "Replace with custom definition"

/**
 * @def GTS_ENABLE_LARGE_PAGE_SUPPORT()
 * @remark
 *  Signature: @code bool enableLargePageSupport() @endcode
 * @returns
 *  True if large pages are enabled.
 */
#define GTS_ENABLE_LARGE_PAGE_SUPPORT() #error "Replace with custom definition"

/**
 * @def GTS_OS_HEAP_ALLOCATE(size)
 * @brief
 *  Allocates a block of memory from the OS Heap. (i.e. HeapAlloc on Win32)
 * @remark
 *  Signature: @code void* osHeapAlloc(size_t size) @endcode
 * @param size
 *  The size of the memory to allocate.
 * @returns
 *  A pointer to the memory or nullptr on fail.
 */
#define GTS_OS_HEAP_ALLOCATE(size) #error "Replace with custom definition"

/**
 * @def GTS_OS_HEAP_ALLOCATE(ptr)
 * @brief
 *  Deallocate a block of memory to the OS Heap. (i.e. HeapFree on Win32)
 * @remark
 *  Signature: @code bool osHeapFree(void* ptr) @endcode
 * @param ptr
 *  A pointer to the memory to free that was allocated with GTS_OS_HEAP_ALLOCATE.
 */
#define GTS_OS_HEAP_FREE(ptr) #error "Replace with custom definition"

/**
 * @def GTS_OS_VIRTUAL_ALLOCATE(ptr, size, commit, largePage)
 * @brief
 *  Creates a new mapping in the virtual address space of the calling process.
 * @remark
 *  Signature: @code void* osVirtualAlloc(void* ptr, size_t size, bool commit, bool largePage) @endcode
 * @ptr
 *  Address hint for the mapping.
 * @param size
 *  The size of the memory to reserve or commit. Must be a multiple of
 *  GTS_GET_OS_ALLOCATION_GRULARITY.
 * @param commit
 *  True to reserve \a and commit the memory.
 * @param largePage
 *  True to use Large Pages. Large pages always commit.
 * @returns
 *  A valid pointer, UINTPTR_MAX if out of memory, or nullptr for other errors.
 */
#define GTS_OS_VIRTUAL_ALLOCATE(ptr, size, commit, largePage) #error "Replace with custom definition"

/**
 * @def GTS_OS_VIRTUAL_COMMIT(ptr, size)
 * @brief
 *  Creates a mapping from a range of virtual address space to physical address
 *  space.
 * @remark
 *  Signature: @code void* osVirtualCommit(void* ptr, size_t size) @endcode
 * @ptr
 *  The starting address of the commit.
 * @param size
 *  The size of the address range. Must be a multiple of GTS_GET_OS_PAGE_SIZE.
 * @returns
 *  A to the committed memory or nullptr on fail.
 */
#define GTS_OS_VIRTUAL_COMMIT(ptr, size) #error "Replace with custom definition"

/**
 * @def GTS_OS_VIRTUAL_DECOMMIT(ptr, size)
 * @brief
 *  Unmaps a range of physical address space.
 * @remark
 *  Signature: @code bool osVirtualDecommit(void* ptr, size_t size) @endcode
 * @ptr
 *  The starting address of the commit.
 * @param size
 *  The size of the address range. Must be a multiple of GTS_GET_OS_PAGE_SIZE.
 * @returns
 *  True on success, false on fail.
 */
#define GTS_OS_VIRTUAL_DECOMMIT(ptr, size) #error "Replace with custom definition"

/**
 * @def GTS_OS_VIRTUAL_DECOMMIT(ptr)
 * @brief
 *  Unmaps a range of virtual address space, decommitting any committed page.
 * @remark
 *  Signature: @code bool osVirtualFree(void* ptr) @endcode
 * @ptr
 *  The starting address of range to free.
 * @param size
 *  The size of the address range. Must be a multiple of GTS_GET_OS_PAGE_SIZE.
 * @returns
 *  True on success, false on fail.
 */
#define GTS_OS_VIRTUAL_FREE(ptr, size)

#endif

/** @} */ // end of OsMemoryWrapper

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// OS THREAD WRAPPERS

/** 
 * @defgroup OsThreadWrapper
 *  User defined wrappers for the OS thread API.
 * @{
 */

#if defined(GTS_HAS_CUSTOM_THREAD_WRAPPERS) || defined(DOXYGEN_DOCUMENTING)

#define GTS_CONTEXT_SWITCH_CYCLES #error "Replace with custom definition"
#define GTS_THREAD_AFFINITY_ALL #error "Replace with custom definition"

#define GTS_THREAD_PRIORITY_HIGHEST #error "Replace with custom definition"
#define GTS_THREAD_PRIORITY_ABOVE_NORMAL #error "Replace with custom definition"
#define GTS_THREAD_PRIORITY_NORMAL #error "Replace with custom definition"
#define GTS_THREAD_PRIORITY_BELOW_NORMAL #error "Replace with custom definition"
#define GTS_THREAD_PRIORITY_BELOW_LOWEST #error "Replace with custom definition"

#define GTS_THREAD_HANDLE_TYPE #error "Replace with custom definition"
#define GTS_THREAD_ID_TYPE #error "Replace with custom definition"

#define GTS_THREAD_FUNCTION_DECL(name) #error "Replace with custom definition"
#define GTS_THREAD_FUNCTION_DEFN(cls, name) #error "Replace with custom definition"

/**
 * @def GTS_THREAD_AFFINITY_SET_TYPE
 * @brief
 *  A description of an CPU affinity set for a threa.
 * @remark
 *  Required struct layout:
 * @code
 *  struct AffinitySet
 *  {
 *      //! Add a cpu ID to the affinity set.
 *      void set(uint32_t cpuId);
 *
 *      //! A union operation with 'other'.
 *      void combine(AffinitySet const& other);
 *
 *      //! Checks the AffinitySet has any IDs set.
 *      bool empty() const;
 *  };
 * @endcode
 */
#define GTS_THREAD_AFFINITY_SET_TYPE #error "Replace with custom definition"

/**
 * @def GTS_CREATE_THREAD()
 * @brief
 *  Creates and launches a new thread of execution.
 * @remark
 *  Signature: @code bool createThread(ThreadHandle& handle, ThreadId& tid, ThreadFunction func, void* arg, size_t stackSize) @endcode
 * @param[out] threadHandle
 *  The handle to the created thread.
  * @param[out] tid
 *  The ID of the created thread.
 * @param func
 *  The function the thread will execute. Must have the signature: void (*func)(void* pArg).
 * @param arg
 *  The user defined argument that will be passed into func.
 * @param stackSize
 *  The stack size of the threads. 0 => default.
 * @returns
 *  True on success, false on fail.
 */
#define GTS_CREATE_THREAD(threadHandle, tid, func, arg, stackSize) #error "Replace with custom definition"

/**
 * @def GTS_JOIN_THREAD()
 * @brief
 *  Blocks until the thread exits.
 * @remark
 *  Signature: @code bool join(ThreadHandle& handle) @endcode
 * @param threadHandle
 *  The thread to wait on.
 * @returns
 *  True on success, false on fail.
 */
#define GTS_JOIN_THREAD(threadHandle) #error "Replace with custom definition"

/**
 * @def GTS_DESTROY_THREAD()
 * @brief
 *  Frees a ThreadHandle.
 * @remark
 *  Signature: @code bool destroy(ThreadHandle& handle) @endcode
 * @param threadHandle
 *  The thread to destroy.
 * @returns
 *  True on success, false on fail.
 */
#define GTS_DESTROY_THREAD(threadHandle) #error "Replace with custom definition"

/**
 * @def GTS_SET_THREAD_AFFINITY()
 * @brief
 *  Sets the processor affinity for the thread.
 * @remark
 *  Signature: @code bool setAffinity(ThreadHandle& handle, uint32_t groupId, GTS_THREAD_AFFINITY_SET_TYPE affinitySet) @endcode
 * @param threadHandle
 *  The thread to apply the affinity mask to.
 * @param groupId
 *  The processor group ID. (Ignored on non-Windows platforms.)
 * @param affinitySet
 *  The affinitySet.
 * @returns
 *  The true if the affinitySet was applied successfully.
 */
#define GTS_SET_THREAD_GROUP_AFFINITY(threadHandle, groupId, affinitySet) #error "Replace with custom definition"

/**
 * @def GTS_SET_THREAD_PRIORITY()
 * @brief
 *  Sets the priority for the thread.
 * @remark
 *  Signature: @code bool setPriority(ThreadId& tid, int32_t priority) @endcode
 * @param threadHandle
 *  The thread to apply the priority to.
 * @param priority
 *  The priority of the thread.
 * @returns
 *  True on success, false on fail.
 */
#define GTS_SET_THREAD_PRIORITY(tid, priority) #error "Replace with custom definition"

/**
 * @def GTS_SET_THIS_THREAD_NAME()
 * @brief
 *  Sets a thread's name.
 * @remark
 *  Signature: @code void setThisThreadName(const char* name) @endcode
 * @param name
 *  The name to set.
 */
#define GTS_SET_THIS_THREAD_NAME(name) #error "Replace with custom definition"

/**
 * @def GTS_GET_THIS_THREAD_ID()
 * @remark
 *  Signature: @code ThreadId getThisThreadId() @endcode
 * @returns
 *  The calling thread's ID.
 */
#define GTS_GET_THIS_THREAD_ID() #error "Replace with custom definition"

/**
 * @def GTS_GET_THIS_THREAD()
 * @remark
 *  Signature: @code ThreadHandle getThisThread() @endcode
 * @returns
 *  The calling thread's handle.
 */
#define GTS_GET_THIS_THREAD() #error "Replace with custom definition"

/**
 * @def GTS_SET_THIS_THREAD_AFFINITY()
 * @brief
 *  Sets the processor affinity for the calling thread.
 * @remark
 *  Signature: @code GTS_THREAD_AFFINITY_TYPE setAffinity(GTS_THREAD_AFFINITY_SET_TYPE affinitySet) @endcode
 * @param groupId
 *  The processor group ID. (Ignored on non-Windows platforms.)
 * @param affinitySet
 *  The affinitySet.
 * @returns
 *  The true if the affinitySet was applied successfully.
 */
#define GTS_SET_THIS_THREAD_GROUP_AFFINITY(groupId, affinitySet) #error "Replace with custom definition"

/**
 * @def GTS_SET_THIS_THREAD_PRIORITY()
 * @brief
 *  Sets the priority for the calling thread.
 * @remark
 *  Signature: @code bool setPriority(int32_t priority) @endcode
 * @param priority
 *  The priority of the thread.
 * @returns
 *  True on success, false on fail.
 */
#define GTS_SET_THREAD_PRIORITY(priority) #error "Replace with custom definition"

/**
 * @def GTS_YIELD_THREAD()
 * @brief
 *  Yields the current thread to another thread.
 * @remark
 *  Signature: @code void yield() @endcode
 */
#define GTS_YIELD_THREAD() #error "Replace with custom definition"

/**
 * @def GTS_THREAD_SLEEP()
 * @brief
 *  Suspends the current thread for the specified time.
 * @remark
 *  Signature: @code void sleep(uint32_t milliseconds) @endcode
 * @param milliseconds
 *  The amount of time to suspend for.
 */
#define GTS_THREAD_SLEEP(milliseconds) #error "Replace with custom definition"

/**
 * @def GTS_GET_HARDWARE_THREAD_COUNT()
 * @remark
 *  Signature: @code uint32_t getHardwareThreadCount() @endcode
 * @returns
 *  The number of HW threads in the system.
 */
#define GTS_GET_HARDWARE_THREAD_COUNT() #error "Replace with custom definition"

/**
 * @def GTS_GET_HARDWARE_THREAD_COUNT()
 * @brief
 *  Get the hardware thread ID and socked ID the calling thread is running on.
 * @remark
 *  Signature: @code void getCurrentProcessorId(uint32_t& socketId, uint32_t& hwTid) @endcode
 * @param[out] socketId
 *  The returned socket ID.
 * @param[out] priority
 *  The returned hardware thread ID.
 */
#define GTS_GET_CURRENT_PROCESSOR_ID(socketId, hwTid) #error "Replace with custom definition"

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// CPU TOPOLOGY

/**
 * @def GTS_CPU_CORE_INFO
 * @brief
 *  A description of a CPU core.
 * @remark
 *  Required struct layout:
 * @code
 *  struct CpuCoreInfo
 *  {
 *      inline ~CpuCoreInfo()
 *      {
 *          delete[] pLogicalAffinityMasks;
 *      }
 *
 *      //! The ID for each hardware thread in the core.
 *      uint32_t* pHardwareThreadIds = nullptr;
 *
 *      //! The number of elements in pHardwareThreadIds.
 *      size_t hardwareThreadIdCount = 0;
 *
 *      //! The efficiency of this core. {0,.., n} => {min,.., max}.
 *      uint8_t efficiencyClass = 0;
 *  };
 * @endcode
 */
#define GTS_CPU_CORE_INFO #error "Replace with custom definition"

/**
 * @def GTS_SOCKET_CORE_INFO
 * @brief
 *  A description of a socket.
 * @remark
 *  Required struct layout:
 * @code
 *  struct CpuSocketInfo
 *  {
 *      ~CpuSocketInfo()
 *      {
 *          delete[] pCoreInfoArray;
 *      }
 *
 *      //! A array of descriptions for each processor in the socket.
 *      CpuCoreInfo* pCoreInfoArray = nullptr;
 *
 *      //! The number of elements in pCoreInfoArray.
 *      size_t coreInfoElementCount = 0;
 *  };
 * @endcode
 */
#define GTS_SOCKET_CORE_INFO #error "Replace with custom definition"

/**
 * @def GTS_NUMA_NODE_CORE_INFO
 * @brief
 *  A description of a NUMA node.
 * @remark
 *  Required struct layout:
 * @code
 *  struct NumaNodeInfo
 *  {
 *      ~NumaNodeInfo()
 *      {
 *          delete[] pCoreInfoArray;
 *      }
 *  
 *      //! A array of descriptions for each processor core in the node.
 *      CpuCoreInfo* pCoreInfoArray = nullptr;
 *  
 *      //! The number of elements in pCoreInfoArray.
 *      size_t coreInfoElementCount = 0;
 *  
 *      //! The ID of the node.
 *      size_t nodeId = SIZE_MAX;
 *  };
 * @endcode
 */
#define GTS_NUMA_NODE_CORE_INFO #error "Replace with custom definition"

/**
 * @def GTS_PROCESSOR_GROUP_INFO
 * @brief
 *  A description of a processor group.
 * @remark
 *  Required struct layout:
 * @code
 *  struct ProcessorGroupInfo
 *  {
 *      ~ProcessorGroupInfo()
 *      {
 *          delete[] pCoreInfoArray;
 *          delete[] pNumaInfoArray;
 *          delete[] pSocketInfoArray;
 *      }
 *
 *      //! A array of descriptions for each processor core in the group.
 *      CpuCoreInfo* pCoreInfoArray = nullptr;
 *
 *      //! The number of elements in pCoreInfoArray.
 *      size_t coreInfoElementCount = 0;
 *
 *      //! A array of descriptions for each NUMA node.
 *      NumaNodeInfo* pNumaInfoArray = nullptr;
 *
 *      //! The number of elements in pNumaInfoArray.
 *      size_t numaNodeInfoElementCount = 0;
 *
 *      //! A array of descriptions for each socket.
 *      SocketInfo* pSocketInfoArray = nullptr;
 *
 *      //! The number of elements in pSocketInfoArray.
 *      size_t socketInfoElementCount = 0;
 *
 *      //! The ID of the group.
 *      uint32_t groupId;
 *  };
 * @endcode
 */
#define GTS_PROCESSOR_GROUP_INFO #error "Replace with custom definition"

/**
 * @def GTS_SYSTEM_TOPOLOGY
 * @brief
 *  A description of the system's processor topology.
 * @remark
 *  Required struct layout:
 * @code
 *  struct SystemTopology
 *  {
 *      ~SystemTopology()
 *      {
 *          delete[] pGroupInfoArray;
 *      }
 *
 *      //! A array of descriptions for each processor group.
 *      ProcessorGroupInfo* pGroupInfoArray = nullptr;
 *
 *      //! The number of elements in pGroupInfoArray.
 *      size_t groupInfoElementCount = 0;
 *  };
 * @endcode
 */
#define GTS_SYSTEM_TOPOLOGY #error "Replace with custom definition"

#endif

/** @} */ // end of OsThreadWrapper

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// OS EVENT WRAPPERS

/** 
 * @defgroup OsEventWrapper
 *  User defined wrappers for the OS event API.
 * @{
 */

#if defined(GTS_HAS_CUSTOM_EVENT_WRAPPERS) || defined(DOXYGEN_DOCUMENTING)

#define GTS_WAIT_FOREVER #error "Replace with custom definition"
#define GTS_EVENT_HANDLE_TYPE #error "Replace with custom definition"

/**
 * @def GTS_CREATE_EVENT(mutexHandle)
 * @brief
 *  Creates an OS event object.
 * @remark
 *  Signature: @code bool createEvent(EventHandle& handle) @endcode
 * @param[out] eventHandle
 *  The returned handle to the event.
 * @returns
 *  True on success, false on fail.
 */
#define GTS_CREATE_EVENT(eventHandle) #error "Replace with custom definition"

/**
 * @def GTS_DESTROY_EVENT(eventHandle)
 * @brief
 *  Destroys an OS event object.
 * @remark
 *  Signature: @code bool destroyEvent(EventHandle& handle) @endcode
 * @param eventHandle
 *  The event to destroy.
 * @returns
 *  True on success, false on fail.
 */
#define GTS_DESTROY_EVENT(eventHandle) #error "Replace with custom definition"

/**
 * @def GTS_WAIT_FOR_EVENT(eventHandle)
 * @brief
 *  Blocks until an event is signaled.
 * @remark
 *  Signature: @code bool waitForEvent(EventHandle& handle, bool waitForever) @endcode
 * @param eventHandle
 *  The event to wait for.
* @param waitForever
 *  Flag true to wait until an event is signaled, false to return immediate.
 * @returns
 *  True if then event was set to unsignaled, false otherwise.
 */
#define GTS_WAIT_FOR_EVENT(eventHandle, waitForever) #error "Replace with custom definition"

/**
 * @def GTS_SIGNAL_EVENT(eventHandle)
 * @brief
 *  Signals an event.
 * @remark
 *  Signature: @code bool signalEvent(EventHandle& handle) @endcode
 * @param eventHandle
 *  The to signal.
 * @returns
 *  True on success, false on fail.
 */
#define GTS_SIGNAL_EVENT(eventHandle) #error "Replace with custom definition"

/**
 * @def GTS_RESET_EVENT(eventHandle)
 * @brief
 *  Resets an event to be unsignaled.
 * @remark
 *  Signature: @code bool resetEvent(EventHandle& handle) @endcode
 * @param eventHandle
 *  The to signal.
 * @returns
 *  True on success, false on fail.
 */
#define GTS_RESET_EVENT(eventHandle) #error "Replace with custom definition"

#endif

/** @} */ // end of OsEventWrapper

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// ATOMICS

/** 
 * @defgroup AtomicsWrapper
 *  User defined wrappers for the atomics API.
 * @{
 */

#if defined(GTS_HAS_CUSTOM_ATOMICS_WRAPPERS) || defined(DOXYGEN_DOCUMENTING)

// See: https://en.cppreference.com/w/cpp/atomic/memory_order
#define GTS_MEMORY_ORDER_RELAXED #error "Replace with custom definition"
#define GTS_MEMORY_ORDER_CONSUME #error "Replace with custom definition"
#define GTS_MEMORY_ORDER_ACQUIRE #error "Replace with custom definition"
#define GTS_MEMORY_ORDER_RELEASE #error "Replace with custom definition"
#define GTS_MEMORY_ORDER_ACQ_REL #error "Replace with custom definition"
#define GTS_MEMORY_ORDER_SEQ_CST #error "Replace with custom definition"

//! An atomic type to be used as GTS_ATOMIC_TYPE<T>
#define GTS_ATOMIC_TYPE #error "Replace with custom definition"

/**
 * @def GTS_ATOMIC_LOAD(a, memoryOrder)
 * @brief
 *  Atomically loads the contents of a with the specified memory order.
 * @remark
 *  Signature: @code
 *  template<typename T>
 *  T load(GTS_ATOMIC_TYPE<T> const& a, int32_t order) @endcode
 * @returns
 *  The loaded value.
 */
#define GTS_ATOMIC_LOAD(a, memoryOrder) #error "Replace with custom definition"

/**
 * @def GTS_ATOMIC_LOAD(a, memoryOrder)
 * @brief
 *  Atomically stores value into a with the specified memory order.
 * @remark
 *  Signature: @code
 *  template<typename T>
 *  void store(GTS_ATOMIC_TYPE<T>& a, T value, int32_t memoryOrder) @endcode
 */
#define GTS_ATOMIC_STORE(a, value, memoryOrder) #error "Replace with custom definition"

/**
 * @def GTS_ATOMIC_FETCH_ADD(a, addend, memoryOrder)
 * @brief
 *  Atomically adds addend to a with the specified memory order.
 * @remark
 *  Signature: @code
 *  template<typename T>
 *  T fetch_add(GTS_ATOMIC_TYPE<T>& a, T inc, int32_t order) @endcode
 * @returns
 *  The value of a before the ADD.
 */
#define GTS_ATOMIC_FETCH_ADD(a, inc, memoryOrder) #error "Replace with custom definition"

/**
 * @def GTS_ATOMIC_FETCH_AND(a, addend, memoryOrder)
 * @brief
 *  Atomically ANDs rhs to a with the specified memory order.
 * @remark
 *  Signature: @code
 *  template<typename T>
 *  T fetch_and(GTS_ATOMIC_TYPE<T>& a, T rhs, int32_t order) @endcode
 * @returns
 *  The value of a before the AND.
 */
#define GTS_ATOMIC_FETCH_AND(a, rhs, memoryOrder) #error "Replace with custom definition"

/**
 * @def GTS_ATOMIC_FETCH_AND(a, addend, memoryOrder)
 * @brief
 *  Atomically ORs rhs and a with the specified memory order.
 * @remark
 *  Signature: @code
 *  template<typename T>
 *  T exchange(GTS_ATOMIC_TYPE<T>& a, T value, int32_t order) @endcode
 * @returns
 *  The value of a before the OR.
 */
#define GTS_ATOMIC_FETCH_OR(a, rhs, memoryOrder) #error "Replace with custom definition"

/**
 * @def GTS_ATOMIC_FETCH_AND(a, addend, memoryOrder)
 * @brief
 *  Atomically exchanges rhs and a with the specified memory order.
 * @remark
 *  Signature: @code
 *  template<typename T>
 *  T exchange(GTS_ATOMIC_TYPE<T>& a, T value, int32_t order) @endcode
 * @returns
 *  The value of a before the exchange.
 */
#define GTS_ATOMIC_EXCHANGE(a, value, memoryOrder) #error "Replace with custom definition"

/**
 * @def GTS_ATOMIC_COMPARE_EXCHANGE_WEAK(a, addend, memoryOrder)
 * @brief
 *  Atomically compares a with expected and stores value in a if
 *  a==expected with xchgMemoryOrder, otheriwse stores value in expected with
 *  loadMemoryOrder. Weak implies the compare may spuriously fail.
 * @remark
 *  Signature: @code
 *  template<typename T>
 *  bool compare_exchange_weak(GTS_ATOMIC_TYPE<T>& atomic, T& expected, T value, int32_t xchgOrder, int32_t loadOrder) @endcode
 * @returns
 *  True if a==expected.
 */
#define GTS_ATOMIC_COMPARE_EXCHANGE_WEAK(a, expected, value, xchgMemoryOrder, loadMemoryOrder) #error "Replace with custom definition"

/**
 * @def GTS_ATOMIC_COMPARE_EXCHANGE_WEAK(a, addend, memoryOrder)
 * @brief
 *  Atomically compares a with expected and stores value in a if
 *  a==expected with xchgMemoryOrder, otheriwse stores value in expected with
 *  loadMemoryOrder.
 * @remark
 *  Signature: @code
 *  template<typename T>
 *  bool compare_exchange_strong(GTS_ATOMIC_TYPE<T>& atomic, T& expected, T value, int32_t xchgOrder, int32_t loadOrder) @endcode
 * @returns
 *  True if a==expected.
 */
#define GTS_ATOMIC_COMPARE_EXCHANGE_STRONG(a, expected, value, xchgMemoryOrder, loadMemoryOrder) #error "Replace with custom definition"

#endif

/** @} */ // end of AtomicsWrapper

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// ASSERT

/** 
 * @defgroup AssertWrapper
 *  User defined wrappers for the assert API.
 * @{
 */

#if defined(GTS_HAS_CUSTOM_ASSERT_WRAPPER) || defined(DOXYGEN_DOCUMENTING)

/**
 * @def GTS_CUSTOM_ASSERT()
 * @brief
 *  Overrides GTS_ASSERT.
  * @remark
 *  Signature: @code
 *  void assert(bool expression, const char* file, int line) @endcode
 * @param expression
 *  The boolean assert expression.
 * @param file
 *  The name of the file where the assertion occurred.
 * @param line
 *  The line number in the file where the assertion occurred.
 */
#GTS_CUSTOM_ASSERT(expression, file, line) #error "Replace with custom definition or nothing."

#endif

/** @} */ // end of AssertWrapper

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// TRACING

/** 
 * @defgroup TracingWrapper
 *  User defined wrappers for the tracing API.
 * @{
 */

#if defined(GTS_HAS_CUSTOM_TRACE_WRAPPER) || defined(DOXYGEN_DOCUMENTING)

#include "gts/analysis/TraceCaptureMask.h"
#include "gts/analysis/TraceColors.h"

#define GTS_TRACE_INIT #error "Replace with custom definition"

#define GTS_TRACE_DUMP(filename) #error "Replace with custom definition or nothing."

#define GTS_TRACE_NAME_THREAD(captureMask, tid, fmt, ...) #error "Replace with custom definition or nothing."

#define GTS_TRACE_SET_CAPTURE_MASK(captureMask) #error "Replace with custom definition or nothing."

#define GTS_TRACE_FRAME_MARK(captureMask) #error "Replace with custom definition or nothing."

#define GTS_TRACE_SCOPED_ZONE(captureMask, color, fmt, ...) #error "Replace with custom definition or nothing."
#define GTS_TRACE_SCOPED_ZONE_P0(captureMask, color, txt) #error "Replace with custom definition or nothing."
#define GTS_TRACE_SCOPED_ZONE_P1(captureMask, color, txt, param1) #error "Replace with custom definition or nothing."
#define GTS_TRACE_SCOPED_ZONE_P2(captureMask, color, txt, param1, param2) #error "Replace with custom definition or nothing."
#define GTS_TRACE_SCOPED_ZONE_P3(captureMask, color, txt, param1, param2, param3) #error "Replace with custom definition or nothing."

#define GTS_TRACE_ZONE_MARKER(captureMask, color, fmt, ...) #error "Replace with custom definition or nothing."
#define GTS_TRACE_ZONE_MARKER_P0(captureMask, color, txt) #error "Replace with custom definition or nothing."
#define GTS_TRACE_ZONE_MARKER_P1(captureMask, color, txt, param1) #error "Replace with custom definition or nothing."
#define GTS_TRACE_ZONE_MARKER_P2(captureMask, color, txt, param1, param2) #error "Replace with custom definition or nothing."
#define GTS_TRACE_ZONE_MARKER_P3(captureMask, color, txt, param1, param2, param3) #error "Replace with custom definition or nothing."

#define GTS_TRACE_PLOT(captureMask, val, txt) #error "Replace with custom definition or nothing."

#define GTS_TRACE_ALLOC(captureMask, ptr, size, fmt, ...) #error "Replace with custom definition or nothing."
#define GTS_TRACE_FREE(captureMask, ptr, fmt, ...) #error "Replace with custom definition or nothing."

#define GTS_TRACE_MUTEX(type, varname) #error "Replace with custom definition or nothing."
#define GTS_TRACE_MUTEX_TYPE(type) #error "Replace with custom definition or nothing."
#define GTS_TRACE_WAIT_FOR_LOCK_START(captureMask, pLock, fmt, ...) #error "Replace with custom definition or nothing."
#define GTS_TRACE_WAIT_FOR_END(captureMask) #error "Replace with custom definition or nothing."
#define GTS_TRACE_LOCK_ACQUIRED(captureMask, pLock, fmt, ...) #error "Replace with custom definition or nothing."
#define GTS_TRACE_LOCK_RELEASED(captureMask, pLock) #error "Replace with custom definition or nothing."

#endif

/** @} */ // end of TracingWrapper

/** @} */ // end of Config
