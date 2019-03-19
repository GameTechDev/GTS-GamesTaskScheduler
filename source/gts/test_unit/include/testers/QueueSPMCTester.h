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

#pragma warning( push )
#pragma warning( disable : 4127 ) // conditional expression is constant

#include "TestersCommon.h"

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
* A test suite for concurrent single-producer, multi-consumer queues.
*/
template<typename TQueue>
struct QueueSPMCTester
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
    void popRace()
    {
        for (int testIter = 0; testIter < PARALLEL_ITERATIONS; ++testIter)
        {
            const uint32_t itemCount = ITEM_COUNT;
            const uint32_t threadCount = std::thread::hardware_concurrency();

            TQueue queue;

            // produce all values uncontended
            for (uint32_t ii = 0; ii < itemCount; ++ii)
            {
                while (!queue.tryPush(ValueType(ii)))
                {
                }
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
            for (uint32_t ii = 0; ii < itemCount; ++ii)
            {
                ASSERT_TRUE(values.find(ValueType(ii)) != values.end());
            }
        }
    }

    //--------------------------------------------------------------------------
    void pushPopRace_2Threads()
    {
        for (int testIter = 0; testIter < PARALLEL_ITERATIONS; ++testIter)
        {
            const uint32_t itemCount = ITEM_COUNT;
            std::unordered_set<ValueType> values(itemCount);

            TQueue queue;

            int totalItemValue = 0;

            std::thread theif([itemCount, &queue, &values]()
            {
                while (values.size() < itemCount)
                {
                    ValueType val;
                    if (queue.tryPop(val))
                    {
                        ASSERT_TRUE(values.find(val) == values.end());
                        values.insert(val);
                    }
                }
            });

            for (uint32_t ii = 0; ii < itemCount; ++ii)
            {
                totalItemValue += ii;
                while (!queue.tryPush(ValueType(ii)))
                {
                }
            }

            theif.join();

            // Verify that all values were received
            for (uint32_t ii = 0; ii < itemCount; ++ii)
            {
                ASSERT_TRUE(values.find(ValueType(ii)) != values.end());
            }
        }
    }

    //--------------------------------------------------------------------------
    void pushPopRace_ManyThreads()
    {
        for (int testIter = 0; testIter < PARALLEL_ITERATIONS; ++testIter)
        {
            const uint32_t itemCount = ITEM_COUNT;
            const uint32_t threadCount = std::thread::hardware_concurrency() - 1;

            TQueue queue;

            std::vector<std::thread*> threads(threadCount);
            std::vector<std::unordered_set<ValueType>> stolenValues(threadCount + 1);

            gts::Atomic<bool> startConsumption(false);
            bool done = false;

            for (uint32_t tt = 0; tt < threadCount; ++tt)
            {
                threads[tt] = new std::thread([itemCount, &queue, &stolenValues, &startConsumption, &done, tt]()
                {
                    while (!startConsumption.load())
                    {
                    }

                    std::unordered_set<ValueType>& localValues = stolenValues[tt];

                    while (true)
                    {
                        if (done && queue.empty())
                        {
                            break;
                        }

                        ValueType val;
                        if (queue.tryPop(val))
                        {
                            ASSERT_TRUE(localValues.find(val) == localValues.end());
                            stolenValues[tt].insert(val);
                        }
                    }
                });
            }

            startConsumption.store(true);

            for (uint32_t ii = 0; ii < itemCount; ++ii)
            {
                while (!queue.tryPush(ValueType(ii)))
                {
                }
            }

            done = true; // let consumers know all items are pushed

            for (uint32_t tt = 0; tt < threadCount; ++tt)
            {
                threads[tt]->join();
                delete threads[tt];
            }

            std::unordered_set<ValueType> values;
            for (uint32_t ii = 0; ii < stolenValues.size(); ++ii)
            {
                for (ValueType val : stolenValues[ii])
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
    }
};

#pragma warning( pop )
