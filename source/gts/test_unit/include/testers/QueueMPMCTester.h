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

#pragma warning( push )
#pragma warning( disable : 4127 ) // conditional expression is constant

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * A test suite for concurrent multi-producer multi-consumer.
 */
template<typename TQueue>
struct QueueMPMCTester
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
    void pushPopCopy()
    {
        // Do twice to verify looping aroung the ring buffer is handled correctly.
        for (size_t iter = 0; iter < 2; ++iter)
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
    }

    //--------------------------------------------------------------------------
    void pushPopMove()
    {
        // Do twice to verify looping aroung the ring buffer is handled correctly.
        for (size_t iter = 0; iter < 2; ++iter)
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
    }

    //--------------------------------------------------------------------------
    void pushRace()
    {
        for (uint32_t testIter = 0; testIter < PARALLEL_ITERATIONS; ++testIter)
        {
            const uint32_t producerThreadCount = gts::Thread::getHardwareThreadCount() <= 2
                ? 2
                : gts::Thread::getHardwareThreadCount() - 2;

            const uint32_t itemCount = ITEM_COUNT * producerThreadCount;
            const uint32_t producedItemsPerThread = itemCount / producerThreadCount;

            std::unordered_set<ValueType, std::hash<uint32_t>> values;
            values.reserve(itemCount);

            TQueue queue;

            gts::Atomic<uint32_t> nextItem(0);

            gts::Atomic<bool> startProduction(false);

            // producers with write contention
            std::vector<std::thread*> producers;
            producers.resize(producerThreadCount);
            for (uint32_t tt = 0; tt < producerThreadCount; ++tt)
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
                        while (!queue.tryPush(ii))
                        {
                            GTS_PAUSE();
                        }
                    }
                });
            }

            startProduction.store(true);

            for (uint32_t tt = 0; tt < producerThreadCount; ++tt)
            {
                producers[tt]->join();
                delete producers[tt];
            }

            // consume without contention
            while (values.size() < itemCount)
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
            for (uint32_t ii = 0; ii < ITEM_COUNT; ++ii)
            {
                ASSERT_TRUE(values.find(ValueType(ii)) != values.end());
            }
        }
    }

    //--------------------------------------------------------------------------
    void popRace()
    {
        for (uint32_t testIter = 0; testIter < PARALLEL_ITERATIONS; ++testIter)
        {
            const uint32_t itemCount = ITEM_COUNT;
            const uint32_t threadCount = std::thread::hardware_concurrency();

            TQueue queue;

            // produce all values uncontended
            for (uint32_t ii = 0; ii < itemCount; ++ii)
            {
                while (!queue.tryPush(ValueType(ii)));
            }

            std::vector<std::thread*> threads(threadCount);
            std::vector<std::vector<ValueType>> stolenValues(threadCount);

            gts::Atomic<bool> startConsumption(false);

            for (uint32_t tt = 0; tt < threadCount; ++tt)
            {
                threads[tt] = new std::thread([itemCount, &queue, &stolenValues, &startConsumption, tt]()
                {
                    while (!startConsumption.load())
                    {
                        GTS_PAUSE();
                    }

                    while (!queue.empty())
                    {
                        ValueType val;
                        if (queue.tryPop(val))
                        {
                            stolenValues[tt].push_back(val);
                        }
                    }
                });
            }

            startConsumption.store(true);

            for (uint32_t tt = 0; tt < threadCount; ++tt)
            {
                threads[tt]->join();
                delete threads[tt];
            }

            std::unordered_set<ValueType> values;
            for (uint32_t ii = 0; ii < stolenValues.size(); ++ii)
            {
                for (ValueType& val : stolenValues[ii])
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
    }

    //--------------------------------------------------------------------------
    void pushPopRace_2Threads()
    {
        const uint32_t threadCount = 1;
        const uint32_t itemCount = ITEM_COUNT * threadCount;

        TQueue queue;

        for (uint32_t testIter = 0; testIter < PARALLEL_ITERATIONS; ++testIter)
        {
            _pushPopRace(queue, 0, itemCount, 1, 1);
        }
    }

    //--------------------------------------------------------------------------
    void pushPopRace_ManyThreads()
    {
        const uint32_t threadCount = gts::Thread::getHardwareThreadCount() / 2;
        const uint32_t itemCount = ITEM_COUNT * threadCount;

        TQueue queue;

        for (uint32_t testIter = 0; testIter < PARALLEL_ITERATIONS; ++testIter)
        {
            _pushPopRace(queue, 0, itemCount, threadCount, threadCount);
        }
    }

    //--------------------------------------------------------------------------
    void pushPopRaceOnFull_ManyThreads()
    {
        const uint32_t threadCount = gts::Thread::getHardwareThreadCount() / 2;
        const uint32_t itemCount = ITEM_COUNT * threadCount;

        TQueue queue;

        for (uint32_t testIter = 0; testIter < PARALLEL_ITERATIONS; ++testIter)
        {
            for (uint32_t ii = 0; ii < itemCount; ++ii)
            {
                while (!queue.tryPush(ii))
                {
                }
            }

            _pushPopRace(queue, itemCount, itemCount * 2, threadCount, threadCount);
        }
    }

private:


    //--------------------------------------------------------------------------
    void _pushPopRace(TQueue& queue, const uint32_t startItem, const uint32_t itemCount, const uint32_t producerThreadCount, const uint32_t consumerThreadCount)
    {
        const uint32_t producedItemsPerThread = (itemCount - startItem) / producerThreadCount;
        const uint32_t consumedItemsPerThread = producedItemsPerThread + startItem / producerThreadCount;

        gts::Atomic<bool> startTest(false);

        // producers
        std::vector<std::thread*> producers(producerThreadCount);
        for (uint32_t tt = 0; tt < producerThreadCount; ++tt)
        {
            const uint32_t itemStart = startItem + tt * producedItemsPerThread;
            const uint32_t itemEnd = itemStart + producedItemsPerThread;
            producers[tt] = new std::thread([itemStart, itemEnd, &queue, &startTest]()
            {
                while (!startTest.load())
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

        // consumer
        std::vector<std::thread*> consumers(consumerThreadCount);
        std::vector<std::vector<ValueType>> poppedValues(consumerThreadCount);
        for (uint32_t tt = 0; tt < consumerThreadCount; ++tt)
        {
            consumers[tt] = new std::thread([consumedItemsPerThread, &queue, &startTest, &poppedValues, tt]()
            {
                while (!startTest.load())
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

        startTest.store(true);

        for (uint32_t tt = 0; tt < producerThreadCount; ++tt)
        {
            producers[tt]->join();
            delete producers[tt];
        }

        for (uint32_t tt = 0; tt < consumerThreadCount; ++tt)
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
        for (uint32_t ii = 0; ii < itemCount; ++ii)
        {
            ASSERT_TRUE(values.find(ValueType(ii)) != values.end());
        }
    }
};

#pragma warning( pop )
