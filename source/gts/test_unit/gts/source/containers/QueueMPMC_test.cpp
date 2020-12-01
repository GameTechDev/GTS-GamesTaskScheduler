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
#include "gts/platform/Machine.h"

#include <algorithm>


#include "gts/containers/parallel/QueueMPMC.h"

#define COMPARE_CONC_QUEUE 0 // rough performance comparison.

#if COMPARE_CONC_QUEUE && GTS_MSVC
#include <concurrent_queue.h>
#endif

#include "testers/QueueTester.h"

 // TODO: verify construction/destruction counts.

namespace testing {

static QueueTester<gts::QueueMPMC<uint32_t>> tester;

//------------------------------------------------------------------------------
TEST(QueueMPMC, emptyConstructor)
{
    tester.emptyConstructor();
}

//------------------------------------------------------------------------------
TEST(QueueMPMC, copyConstructor)
{
    tester.copyConstructor();
}

//------------------------------------------------------------------------------
TEST(QueueMPMC, moveConstructor)
{
    tester.moveConstructor();
}

//------------------------------------------------------------------------------
TEST(QueueMPMC, copyAssign)
{
    tester.copyAssign();
}

//------------------------------------------------------------------------------
TEST(QueueMPMC, moveAssign)
{
    tester.moveAssign();
}

//------------------------------------------------------------------------------
TEST(QueueMPMC, copyPushPop)
{
    tester.copyPushPop();
}

//------------------------------------------------------------------------------
TEST(QueueMPMC, movePushPop)
{
    tester.movePushPop();
}

//------------------------------------------------------------------------------
TEST(QueueMPMC, pushRace)
{
    for(uint32_t ii = 0; ii < PARALLEL_ITERATIONS; ++ii)
    {
        tester.pushRace(gts::Thread::getHardwareThreadCount());
    }
}

#if COMPARE_CONC_QUEUE

//------------------------------------------------------------------------------
TEST(QueueMPMC, concurrent_queue_pushRace)
{
    uint32_t producerThreadCount = gts::Thread::getHardwareThreadCount();

    for(uint32_t iter = 0; iter < PARALLEL_ITERATIONS; ++iter)
    {
        const uint32_t totalItemCount = ITEM_COUNT * producerThreadCount;
        const uint32_t producedItemsPerThread = ITEM_COUNT;
        const uint32_t threadCount = producerThreadCount - 1;

        std::unordered_set<uint32_t, std::hash<uint32_t>> values;
        values.reserve(totalItemCount);

        concurrency::concurrent_queue<uint32_t> queue;

        gts::Atomic<uint32_t> nextItem(0);

        gts::Atomic<bool> startProduction(false);

        // producers with write contention
        std::vector<std::thread*> producers;
        producers.resize(threadCount);
        for (uint32_t tt = 0; tt < threadCount; ++tt)
        {
            const uint32_t itemStart = tt * producedItemsPerThread;
            const uint32_t itemEnd = itemStart + producedItemsPerThread;

            producers[tt] = new std::thread([itemStart, itemEnd, &queue, &startProduction]()
            {
                while (!startProduction.load())
                {
                    GTS_PAUSE();
                }

                for (uint32_t ii = itemStart; ii < itemEnd; ++ii)
                {
                    queue.push(ii);
                }
            });
        }

        startProduction.store(true);

        // This thread helps produce to prevent over subscription.
        {
            const uint32_t itemStart = threadCount * producedItemsPerThread;
            const uint32_t itemEnd = itemStart + producedItemsPerThread;

            for (uint32_t ii = itemStart; ii < itemEnd; ++ii)
            {
                queue.push(ii);
            }
        }

        for (uint32_t tt = 0; tt < threadCount; ++tt)
        {
            producers[tt]->join();
            delete producers[tt];
        }

        // consume without contention
        while (values.size() < totalItemCount)
        {
            uint32_t val;
            if (queue.try_pop(val))
            {
                // Verify that all values are unique
                ASSERT_TRUE(values.find(val) == values.end());
                values.insert(val);
            }
        }

        // Verify that all values were received
        for (uint32_t ii = 0; ii < totalItemCount; ++ii)
        {
            ASSERT_TRUE(values.find(ii) != values.end());
        }
    }
}

#endif

//------------------------------------------------------------------------------
TEST(QueueMPMC, popRace)
{
    for(uint32_t ii = 0; ii < PARALLEL_ITERATIONS; ++ii)
    {
        tester.popRace(gts::Thread::getHardwareThreadCount());
    }
}

#if COMPARE_CONC_QUEUE

//------------------------------------------------------------------------------
TEST(QueueMPMC, concurrent_queue_popRace)
{
    uint32_t consumerThreadCount = gts::Thread::getHardwareThreadCount();

    for(uint32_t iter = 0; iter < PARALLEL_ITERATIONS; ++iter)
    {
        const uint32_t totalItemCount = ITEM_COUNT * consumerThreadCount;
        const uint32_t threadCount = consumerThreadCount - 1;

        concurrency::concurrent_queue<uint32_t> queue;

        // produce all values uncontended
        for (uint32_t ii = 0; ii < totalItemCount; ++ii)
        {
            queue.push(ii);
        }

        std::vector<std::thread*> threads(threadCount);
        std::vector<std::vector<uint32_t>> consumedValues(consumerThreadCount);

        gts::Atomic<bool> startConsumption(false);

        for (uint32_t tt = 0; tt < threadCount; ++tt)
        {
            threads[tt] = new std::thread([&queue, &consumedValues, &startConsumption, tt]()
            {
                while (!startConsumption.load())
                {
                    GTS_PAUSE();
                }

                while (!queue.empty())
                {
                    uint32_t val;
                    if (queue.try_pop(val))
                    {
                        consumedValues[tt].push_back(val);
                    }
                }
            });
        }

        startConsumption.store(true);

        // This thread helps consume to prevent over subscription.
        while (!queue.empty())
        {
            uint32_t val;
            if (queue.try_pop(val))
            {
                consumedValues[threadCount].push_back(val);
            }
        }

        for (uint32_t tt = 0; tt < threadCount; ++tt)
        {
            threads[tt]->join();
            delete threads[tt];
        }

        std::unordered_set<uint32_t> values;
        for (uint32_t ii = 0; ii < consumedValues.size(); ++ii)
        {
            for (uint32_t& val : consumedValues[ii])
            {
                values.insert(val);
            }
        }

        // Verify that all values were received
        for (uint32_t ii = 0; ii < totalItemCount; ++ii)
        {
            ASSERT_TRUE(values.find(ii) != values.end());
        }
    }
}

#endif

//------------------------------------------------------------------------------
TEST(QueueMPMC, pushPopRace)
{
    uint32_t producerCount = std::max(1u, gts::Thread::getHardwareThreadCount() / 2);
    uint32_t consumerCount = std::max(1u, gts::Thread::getHardwareThreadCount() / 2);
    for(uint32_t ii = 0; ii < PARALLEL_ITERATIONS; ++ii)
    {
        tester.pushPopRace(producerCount, consumerCount);
    }
}

#if COMPARE_CONC_QUEUE

//------------------------------------------------------------------------------
TEST(QueueMPMC, concurrent_queue_pushPopRace)
{
    uint32_t producerThreadCount = std::max(1u, gts::Thread::getHardwareThreadCount() / 2);
    uint32_t consumerThreadCount = std::max(1u, gts::Thread::getHardwareThreadCount() / 2);

    for(uint32_t iter = 0; iter < PARALLEL_ITERATIONS; ++iter)
    {
        uint32_t producedItemsPerThread = ITEM_COUNT;
        uint32_t consumedItemsPerThread = ITEM_COUNT;

        concurrency::concurrent_queue<uint32_t> queue;

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
                        queue.push(ii);
                    }
                });
        }

        // consumers
        std::vector<std::vector<uint32_t>> poppedValues(consumerThreadCount);
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
                        while (!queue.try_pop(val))
                        {
                            GTS_PAUSE();
                        }
                        poppedValues[tt].push_back(val);
                    }
                });
        }

        startTest.store(true, memory_order::release);

        // This thread helps consume to prevent over subscription.
        for (uint32_t ii = 0; ii < consumedItemsPerThread; ++ii)
        {
            uint32_t val;
            while (!queue.try_pop(val))
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

        std::unordered_set<uint32_t> values;
        for (uint32_t ii = 0; ii < poppedValues.size(); ++ii)
        {
            for (uint32_t& val : poppedValues[ii])
            {
                values.insert(val);
            }
        }

        // Verify that all values were received
        for (uint32_t ii = 0; ii < ITEM_COUNT; ++ii)
        {
            ASSERT_TRUE(values.find(ii) != values.end());
        }
    }
}

#endif

} // namespace testing
