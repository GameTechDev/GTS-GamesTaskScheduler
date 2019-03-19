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
struct QueueSPSCTester
{
    using ValueType = typename TQueue::value_type;

    //--------------------------------------------------------------------------
    void EmptyConstructor()
    {
        TQueue queue;

        ASSERT_EQ(queue.empty(), true);
        ASSERT_EQ(queue.size(), 0U);
    }

    //--------------------------------------------------------------------------
    void CopyConstructor()
    {
        TQueue queueA;
        for (uint32_t ii = 0; ii < ITEM_COUNT; ++ii)
        {
            ValueType val(ii);
            while (!queueA.tryPush(val))
            {
            }
        }

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

    //--------------------------------------------------------------------------
    void MoveConstructor()
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
    void CopyAssign()
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
    void MoveAssign()
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
    void CopyPushPop()
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
    void MovePushPop()
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
    void PushPopRaceFromEmpty()
    {
        TQueue queue;

        for (int testIter = 0; testIter < PARALLEL_ITERATIONS; ++testIter)
        {
            _PushPopRace(queue, 0, ITEM_COUNT);
        }
    }

    //--------------------------------------------------------------------------
    void PushPopRaceFromFull()
    {
        TQueue queue;

        for (int testIter = 0; testIter < PARALLEL_ITERATIONS; ++testIter)
        {

            for (uint32_t ii = 0; ii < ITEM_COUNT; ++ii)
            {
                while (!queue.tryPush(ii))
                {
                }
            }

            _PushPopRace(queue, ITEM_COUNT, ITEM_COUNT * 2); // double the size since we are starting full.
        }
    }

private:

    //--------------------------------------------------------------------------
    void _PushPopRace(TQueue& queue, const uint32_t startItem, const uint32_t itemCount)
    {
        std::vector<ValueType> values;
        values.reserve(itemCount);

        gts::Atomic<bool> startTest(false);
        gts::Atomic<uint32_t> nextItem(startItem);

        // producer
        std::thread producer([itemCount, &nextItem, &queue, &startTest]()
        {
            while (!startTest.load())
            {
            }

            while (true)
            {
                ValueType currItem = ValueType(nextItem.fetch_add(1));
                if ((uint32_t)currItem >= itemCount)
                {
                    break;
                }

                while (!queue.tryPush(currItem))
                {
                }
            }
        });

        // consumer
        std::thread consumer([itemCount, &queue, &values, &startTest]()
        {
            while (!startTest.load())
            {
            }

            while (values.size() < itemCount)
            {
                ValueType val;
                if (queue.tryPop(val))
                {
                    values.push_back(val);
                }
            }
        });

        startTest.store(true);

        producer.join();
        consumer.join();

        // Verify that all values were received
        for (uint32_t ii = 0; ii < itemCount; ++ii)
        {
            ASSERT_TRUE(values[ii] == ii);
        }
    }
};

#pragma warning( pop )
