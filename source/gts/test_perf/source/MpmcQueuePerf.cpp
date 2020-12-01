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
#include <atomic>
#include <chrono>
#include <vector>
#include <thread>

#include "gts_perf/Stats.h"

#include <gts/micro_scheduler/WorkerPool.h>
#include <gts/micro_scheduler/MicroScheduler.h>
#include <gts/containers/parallel/QueueMPMC.h>

using namespace gts;

//------------------------------------------------------------------------------
Stats mpmcQueuePerfSerial(const uint32_t itemCount, uint32_t iterations)
{
    Stats stats(iterations);

    // Do test.
    for (uint32_t ii = 0; ii < iterations; ++ii)
    {
        gts::QueueMPMC<uint32_t> queue;

        auto start = std::chrono::high_resolution_clock::now();
        
        for (uint32_t tt = 0; tt < itemCount; ++tt)
        {
            queue.tryPush(tt);
        }

        for (uint32_t tt = 0; tt < itemCount; ++tt)
        {
            uint32_t val;
            queue.tryPop(val);
        }

        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double> diff = end - start;
        stats.addDataPoint(diff.count());
    }

    return stats;
}

//------------------------------------------------------------------------------
Stats mpmcQueuePerfParallel(const uint32_t threadCount, const uint32_t itemCount, uint32_t iterations)
{
    Stats stats(iterations);

    GTS_ASSERT(threadCount % 2 == 0);

    // Do test.
    for (uint32_t iter = 0; iter < iterations; ++iter)
    {
        uint32_t consumerThreadCount = threadCount / 2;
        uint32_t producerThreadCount = threadCount / 2;

        if (producerThreadCount == 0)
        {
            GTS_ASSERT(0);
            return stats;
        }

        uint32_t producedItemsPerThread = itemCount / producerThreadCount;
        uint32_t consumedItemsPerThread = producedItemsPerThread;

        gts::QueueMPMC<uint32_t> queue;

        gts::Atomic<bool> startTest(false);

        // producers
        std::vector<std::thread*> producers(producerThreadCount);
        for (uint32_t tt = 0; tt < producerThreadCount; ++tt)
        {
            uint32_t itemStart = tt * producedItemsPerThread;
            uint32_t itemEnd = itemStart + producedItemsPerThread;
            producers[tt] = new std::thread([itemStart, itemEnd, &queue, &startTest]()
            {
                while (!startTest.load(memory_order::acquire))
                {
                    GTS_PAUSE();
                }

                for (uint32_t ii = itemStart; ii < itemEnd; ++ii)
                {
                    while (!queue.tryPush(ii))
                    {
                        GTS_PAUSE();
                    }
                }
            });
        }

        // consumers
        std::vector<std::vector<int>> poppedValues(consumerThreadCount);
        uint32_t numConsumerThreads = consumerThreadCount - 1;
        std::vector<std::thread*> consumers(numConsumerThreads);
        for (uint32_t tt = 0; tt < numConsumerThreads; ++tt)
        {
            consumers[tt] = new std::thread([consumedItemsPerThread, &queue, &startTest, &poppedValues, tt]()
            {
                while (!startTest.load(memory_order::acquire))
                {
                    GTS_PAUSE();
                }

                for (uint32_t ii = 0; ii < consumedItemsPerThread; ++ii)
                {
                    uint32_t val;
                    while (!queue.tryPop(val))
                    {
                        GTS_PAUSE();
                    }
                    poppedValues[tt].push_back(val);
                }
            });
        }

        auto start = std::chrono::high_resolution_clock::now();

        startTest.store(true, memory_order::release);

        // This thread helps consume to prevent over subscription.
        for (uint32_t ii = 0; ii < consumedItemsPerThread; ++ii)
        {
            uint32_t val;
            while (!queue.tryPop(val))
            {
                GTS_PAUSE();
            }
            poppedValues[numConsumerThreads].push_back(val);
        }

        for (uint32_t tt = 0; tt < producerThreadCount; ++tt)
        {
            producers[tt]->join();
            delete producers[tt];
        }

        for (uint32_t tt = 0; tt < numConsumerThreads; ++tt)
        {
            consumers[tt]->join();
            delete consumers[tt];
        }

        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double> diff = end - start;
        stats.addDataPoint(diff.count());
    }

    return stats;
}
