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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <unordered_set>
#include <thread>

#include "gts/platform/Atomic.h"
#include "gts/platform/Thread.h"

#include "TestersCommon.h"

#if GTS_MSVC
#pragma warning( push )
#pragma warning( disable : 4127 ) // conditional expression is constant
#endif

using namespace gts;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * A test suite for concurrent multi-producer multi-consumer.
 */
template<typename TQueue>
struct QueueTester
{
    using ValueType = typename TQueue::value_type;

    //--------------------------------------------------------------------------
    void emptyConstructor()
    {
        TQueue queue;

        ASSERT_EQ(queue.empty(), true);
        ASSERT_EQ(queue.size(), 0U);
    }

    //--------------------------------------------------------------------------
    void copyConstructor()
    {
        TQueue queueA;
        for (uint32_t ii = 0; ii < ITEM_COUNT; ++ii)
        {
            ValueType val(ii);
            while (!queueA.tryPush(val))
            {
            }
        }

        {
            TQueue queueB = queueA;

            for (uint32_t ii = 0; ii < ITEM_COUNT; ++ii)
            {
                ValueType val;
                ASSERT_EQ(queueA.empty(), false);
                while (!queueA.tryPop(val))
                {
                }
                ASSERT_EQ(val, ValueType(ii));
            }

            for (uint32_t ii = 0; ii < ITEM_COUNT; ++ii)
            {
                ValueType val;
                ASSERT_EQ(queueB.empty(), false);
                while (!queueB.tryPop(val))
                {
                }
                ASSERT_EQ(val, ValueType(ii));
            }
        }
    }

    //--------------------------------------------------------------------------
    void moveConstructor()
    {
        TQueue queueA;
        for (uint32_t ii = 0; ii < ITEM_COUNT; ++ii)
        {
            ValueType val(ii);
            while (!queueA.tryPush(val))
            {
            }
        }

        TQueue queueB = std::move(queueA);

        ASSERT_EQ(queueA.empty(), true);
        ASSERT_EQ(queueA.size(), 0U);

        for (uint32_t ii = 0; ii < ITEM_COUNT; ++ii)
        {
            ValueType val;
            ASSERT_EQ(queueB.empty(), false);
            while (!queueB.tryPop(val))
            {
            }
            ASSERT_EQ(val, ValueType(ii));
        }
    }

    //--------------------------------------------------------------------------
    void copyAssign()
    {
        TQueue queueA;
        for (uint32_t ii = 0; ii < ITEM_COUNT; ++ii)
        {
            ValueType val(ii);
            while (!queueA.tryPush(val))
            {
            }
        }

        TQueue queueB;
        queueB = queueA;

        for (uint32_t ii = 0; ii < ITEM_COUNT; ++ii)
        {
            ValueType val;
            ASSERT_EQ(queueA.empty(), false);
            while (!queueA.tryPop(val))
            {
            }
            ASSERT_EQ(val, ValueType(ii));
        }

        for (uint32_t ii = 0; ii < ITEM_COUNT; ++ii)
        {
            ValueType val;
            ASSERT_EQ(queueB.empty(), false);
            while (!queueB.tryPop(val))
            {
            }
            ASSERT_EQ(val, ValueType(ii));
        }
    }

    //--------------------------------------------------------------------------
    void moveAssign()
    {
        TQueue queueA;
        for (uint32_t ii = 0; ii < ITEM_COUNT; ++ii)
        {
            ValueType val(ii);
            while (!queueA.tryPush(val))
            {
            }
        }

        TQueue queueB;
        queueB = std::move(queueA);

        ASSERT_EQ(queueA.empty(), true);
        ASSERT_EQ(queueA.size(), 0U);

        for (uint32_t ii = 0; ii < ITEM_COUNT; ++ii)
        {
            ValueType val;
            ASSERT_EQ(queueB.empty(), false);
            while (!queueB.tryPop(val))
            {
            }
            ASSERT_EQ(val, ValueType(ii));
        }
    }

    //--------------------------------------------------------------------------
    void copyPushPop()
    {
        TQueue queue;

        for (uint32_t ii = 0; ii < ITEM_COUNT; ++ii)
        {
            ValueType val(ii);
            while (!queue.tryPush(val))
            {
            }
        }

        for (uint32_t ii = 0; ii < ITEM_COUNT; ++ii)
        {
            ValueType val;
            ASSERT_EQ(queue.empty(), false);
            while (!queue.tryPop(val))
            {
            }
            ASSERT_EQ(val, ValueType(ii));
        }
    }

    //--------------------------------------------------------------------------
    void movePushPop()
    {
        TQueue queue;

        for (uint32_t ii = 0; ii < ITEM_COUNT; ++ii)
        {
            while (!queue.tryPush(ValueType(ii)))
            {
            }
        }

        for (uint32_t ii = 0; ii < ITEM_COUNT; ++ii)
        {
            ValueType val;
            ASSERT_EQ(queue.empty(), false);
            while (!queue.tryPop(val))
            {
            }
            ASSERT_EQ(val, ValueType(ii));
        }
    }

    //--------------------------------------------------------------------------
    void pushRace(uint32_t producerThreadCount)
    {
        const uint32_t totalItemCount = ITEM_COUNT * producerThreadCount;
        const uint32_t producedItemsPerThread = ITEM_COUNT;
        const uint32_t threadCount = producerThreadCount - 1;

        std::unordered_set<ValueType, std::hash<uint32_t>> values;
        values.reserve(totalItemCount);

        TQueue queue;

        gts::Atomic<uint32_t> nextItem(0);

        gts::Atomic<bool> startTest(false);

        // producers with write contention
        std::vector<std::thread*> producers;
        producers.resize(threadCount);
        for (uint32_t tt = 0; tt < threadCount; ++tt)
        {
            const uint32_t itemStart = tt * producedItemsPerThread;
            const uint32_t itemEnd = itemStart + producedItemsPerThread;

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

        startTest.store(true, memory_order::release);

        // This thread helps produce to prevent over subscription.
        {
            const uint32_t itemStart = threadCount * producedItemsPerThread;
            const uint32_t itemEnd = itemStart + producedItemsPerThread;

            for (uint32_t ii = itemStart; ii < itemEnd; ++ii)
            {
                while (!queue.tryPush(ii))
                {
                    GTS_PAUSE();
                }
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
            ValueType val;
            if (queue.tryPop(val))
            {
                // Verify that all values are unique
                ASSERT_TRUE(values.find(val) == values.end());
                values.insert(val);
            }
        }

        // Verify that all values were received
        for (uint32_t ii = 0; ii < totalItemCount; ++ii)
        {
            ASSERT_TRUE(values.find(ValueType(ii)) != values.end());
        }
    }

    //--------------------------------------------------------------------------
    void popRace(uint32_t consumerThreadCount)
    {
        const uint32_t totalItemCount = ITEM_COUNT * consumerThreadCount;
        const uint32_t threadCount = consumerThreadCount - 1;

        TQueue queue;

        // produce all values uncontended
        for (uint32_t ii = 0; ii < totalItemCount; ++ii)
        {
            while (!queue.tryPush(ValueType(ii)));
        }

        std::vector<std::thread*> threads(threadCount);
        std::vector<std::vector<ValueType>> consumedValues(consumerThreadCount);

        gts::Atomic<bool> startTest(false);

        for (uint32_t tt = 0; tt < threadCount; ++tt)
        {
            threads[tt] = new std::thread([&queue, &consumedValues, &startTest, tt]()
            {
                while (!startTest.load(memory_order::acquire))
                {
                    GTS_PAUSE();
                }

                while (!queue.empty())
                {
                    ValueType val;
                    if (queue.tryPop(val))
                    {
                        consumedValues[tt].push_back(val);
                    }
                }
            });
        }

        startTest.store(true, memory_order::release);

        // This thread helps consume to prevent over subscription.
        while (!queue.empty())
        {
            ValueType val;
            if (queue.tryPop(val))
            {
                consumedValues[threadCount].push_back(val);
            }
        }

        for (uint32_t tt = 0; tt < threadCount; ++tt)
        {
            threads[tt]->join();
            delete threads[tt];
        }

        std::unordered_set<ValueType> values;
        for (uint32_t ii = 0; ii < consumedValues.size(); ++ii)
        {
            for (ValueType& val : consumedValues[ii])
            {
                values.insert(val);
            }
        }

        // Verify that all values were received
        for (uint32_t ii = 0; ii < totalItemCount; ++ii)
        {
            ASSERT_TRUE(values.find(ValueType(ii)) != values.end());
        }
    }

    //--------------------------------------------------------------------------
    void pushPopRace(const uint32_t producerThreadCount, const uint32_t consumerThreadCount)
    {
        GTS_ASSERT(producerThreadCount == consumerThreadCount || producerThreadCount == 1 || consumerThreadCount == 1);

        uint32_t producedItemsPerThread = producerThreadCount == 1 ? ITEM_COUNT * consumerThreadCount : ITEM_COUNT;
        uint32_t consumedItemsPerThread = consumerThreadCount == 1 ? ITEM_COUNT * producerThreadCount : ITEM_COUNT;

        TQueue queue;

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
        std::vector<std::vector<ValueType>> poppedValues(consumerThreadCount);
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
                    ValueType val;
                    while (!queue.tryPop(val))
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
            ValueType val;
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

        std::unordered_set<ValueType> values;
        for (uint32_t ii = 0; ii < poppedValues.size(); ++ii)
        {
            for (ValueType& val : poppedValues[ii])
            {
                values.insert(val);
            }
        }

        // Verify that all values were received
        for (uint32_t ii = 0; ii < ITEM_COUNT; ++ii)
        {
            ASSERT_TRUE(values.find(ValueType(ii)) != values.end());
        }
    }
};

#if GTS_MSVC
#pragma warning( pop )
#endif