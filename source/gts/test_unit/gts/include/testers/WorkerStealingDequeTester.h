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
#include "gts/micro_scheduler/WorkerPool.h"
#include "gts/micro_scheduler/MicroScheduler.h"
#include "TestersCommon.h"

#if GTS_MSVC
#pragma warning( push )
#pragma warning( disable : 4127 ) // conditional expression is constant
#endif

using namespace gts;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
* A test suite for worker stealing deques.
*/
template<typename TQueue>
struct WorkerStealingDequeTester
{
    //--------------------------------------------------------------------------
    void pushPopCopy(TQueue& queue)
    {
        WorkerPool workerPool;
        workerPool.initialize(1);

        MicroScheduler scheduler;
        scheduler.initialize(&workerPool);

        uint32_t itemCount = ITEM_COUNT;
        std::vector<EmptyTask*> tasks(itemCount);
        for (uint32_t ii = 0; ii < itemCount; ++ii)
        {
            tasks[ii] = scheduler.allocateTask<EmptyTask>();
        }

        // Do twice to verify looping around the ring buffer is handled correctly.
        for (size_t iter = 0; iter < 2; ++iter)
        {
            for (uint32_t ii = 0; ii < itemCount; ++ii)
            {
                while (!queue.tryPush(tasks[ii]))
                {
                }
            }

            for (uint32_t ii = 0; ii < itemCount; ++ii)
            {
                Task* val;
                ASSERT_EQ(queue.empty(), false);
                while (!queue.tryPop(val))
                {
                }
                ASSERT_EQ(val, tasks[itemCount - 1 - ii]);
            }
        }

        for (uint32_t ii = 0; ii < itemCount; ++ii)
        {
            scheduler.destoryTask(tasks[ii]);
        }
    }

    //--------------------------------------------------------------------------
    void pushStealCopy(TQueue& queue)
    {
        WorkerPool workerPool;
        workerPool.initialize(1);

        MicroScheduler scheduler;
        scheduler.initialize(&workerPool);

        uint32_t itemCount = ITEM_COUNT;
        std::vector<EmptyTask*> tasks(itemCount);
        for (uint32_t ii = 0; ii < itemCount; ++ii)
        {
            tasks[ii] = scheduler.allocateTask<EmptyTask>();
        }

        // Do twice to verify looping around the ring buffer is handled correctly.
        for (size_t iter = 0; iter < 2; ++iter)
        {
            for (uint32_t ii = 0; ii < itemCount; ++ii)
            {
                while (!queue.tryPush(tasks[ii]))
                {
                }
            }

            for (uint32_t ii = 0; ii < itemCount; ++ii)
            {
                Task* val;
                ASSERT_EQ(queue.empty(), false);
                while (!queue.trySteal(val))
                {
                }
                ASSERT_EQ(val, tasks[ii]);
            }
        }

        for (uint32_t ii = 0; ii < itemCount; ++ii)
        {
            scheduler.destoryTask(tasks[ii]);
        }
    }

    //--------------------------------------------------------------------------
    void stealRace(TQueue& queue)
    {
        WorkerPool workerPool;
        workerPool.initialize(1);

        MicroScheduler scheduler;
        scheduler.initialize(&workerPool);

        for (uint32_t testIter = 0; testIter < PARALLEL_ITERATIONS; ++testIter)
        {
            const uint32_t itemCount = ITEM_COUNT;
            const uint32_t threadCount = std::thread::hardware_concurrency();

            std::vector<EmptyTask*> tasks(itemCount);
            for (uint32_t ii = 0; ii < itemCount; ++ii)
            {
                tasks[ii] = scheduler.allocateTask<EmptyTask>();
            }

            // produce all values uncontended
            for (uint32_t ii = 0; ii < itemCount; ++ii)
            {
                while (!queue.tryPush(tasks[ii]))
                {
                }
            }

            std::vector<std::thread*> threads(threadCount);
            std::vector<std::vector<Task*>> stolenValues(threadCount);

            gts::Atomic<bool> startTest(false);

            for (uint32_t tt = 0; tt < threadCount; ++tt)
            {
                threads[tt] = new std::thread([&queue, &stolenValues, &startTest, tt]()
                {
                    while (!startTest.load(memory_order::acquire))
                    {
                        GTS_PAUSE();
                    }

                    while (!queue.empty())
                    {
                        Task* val;
                        if (queue.trySteal(val))
                        {
                            stolenValues[tt].push_back(val);
                        }
                    }
                });
            }

            startTest.store(true, memory_order::release);

            for (uint32_t tt = 0; tt < threadCount; ++tt)
            {
                threads[tt]->join();
                delete threads[tt];
            }

            std::unordered_set<Task*> values;
            for (uint32_t ii = 0; ii < stolenValues.size(); ++ii)
            {
                for (Task* val : stolenValues[ii])
                {
                    values.insert(val);
                }
            }

            // Verify that all values were received
            for (uint32_t ii = 0; ii < itemCount; ++ii)
            {
                ASSERT_TRUE(values.find(tasks[ii]) != values.end());
            }

            for (uint32_t ii = 0; ii < itemCount; ++ii)
            {
                scheduler.destoryTask(tasks[ii]);
            }
        }
    }

    //--------------------------------------------------------------------------
    void pushStealRace_2Threads(TQueue& queue)
    {
        WorkerPool workerPool;
        workerPool.initialize(1);

        MicroScheduler scheduler;
        scheduler.initialize(&workerPool);

        for (uint32_t testIter = 0; testIter < PARALLEL_ITERATIONS; ++testIter)
        {
            const uint32_t itemCount = ITEM_COUNT;
            std::unordered_set<Task*> values(itemCount);

            std::vector<EmptyTask*> tasks(itemCount);
            for (uint32_t ii = 0; ii < itemCount; ++ii)
            {
                tasks[ii] = scheduler.allocateTask<EmptyTask>();
            }

            uint32_t totalItemValue = 0;

            std::thread theif([itemCount, &queue, &values]()
            {
                while (values.size() < itemCount)
                {
                    Task* val;
                    if (queue.trySteal(val))
                    {
                        ASSERT_TRUE(values.find(val) == values.end());
                        values.insert(val);
                    }
                }
            });

            for (uint32_t ii = 0; ii < itemCount; ++ii)
            {
                totalItemValue += ii;
                while (!queue.tryPush(tasks[ii]))
                {
                }
            }

            theif.join();

            // Verify that all values were received
            for (uint32_t ii = 0; ii < itemCount; ++ii)
            {
                ASSERT_TRUE(values.find(tasks[ii]) != values.end());
            }

            for (uint32_t ii = 0; ii < itemCount; ++ii)
            {
                scheduler.destoryTask(tasks[ii]);
            }
        }
    }

    //--------------------------------------------------------------------------
    void pushStealRace_ManyThreads(TQueue& queue)
    {
        WorkerPool workerPool;
        workerPool.initialize(1);

        MicroScheduler scheduler;
        scheduler.initialize(&workerPool);

        for (uint32_t testIter = 0; testIter < PARALLEL_ITERATIONS; ++testIter)
        {
            const uint32_t itemCount = ITEM_COUNT;
            const uint32_t threadCount = std::thread::hardware_concurrency() - 1;

            std::vector<EmptyTask*> tasks(itemCount);
            for (uint32_t ii = 0; ii < itemCount; ++ii)
            {
                tasks[ii] = scheduler.allocateTask<EmptyTask>();
            }

            std::vector<std::thread*> threads(threadCount);
            std::vector<std::unordered_set<Task*>> stolenValues(threadCount + 1);

            gts::Atomic<bool> startTest(false);
            bool done = false;

            for (uint32_t tt = 0; tt < threadCount; ++tt)
            {
                threads[tt] = new std::thread([&queue, &stolenValues, &startTest, &done, tt]()
                {
                    while (!startTest.load(memory_order::acquire))
                    {
                    }

                    std::unordered_set<Task*>& localValues = stolenValues[tt];

                    while (true)
                    {
                        if (done && queue.empty())
                        {
                            break;
                        }

                        Task* val;
                        if (queue.trySteal(val))
                        {
                            ASSERT_TRUE(localValues.find(val) == localValues.end());
                            stolenValues[tt].insert(val);
                        }
                    }
                });
            }

            startTest.store(true, memory_order::release);

            for (uint32_t ii = 0; ii < itemCount; ++ii)
            {
                while (!queue.tryPush(tasks[ii]))
                {
                }
            }

            done = true; // let consumers know all items are pushed

            for (uint32_t tt = 0; tt < threadCount; ++tt)
            {
                threads[tt]->join();
                delete threads[tt];
            }

            std::unordered_set<Task*> values;
            for (uint32_t ii = 0; ii < stolenValues.size(); ++ii)
            {
                for (Task* val : stolenValues[ii])
                {
                    values.insert(val);
                }
            }

            // Verify that all values were received
            for (uint32_t ii = 0; ii < itemCount; ++ii)
            {
                ASSERT_TRUE(values.find(tasks[ii]) != values.end());
            }

            for (uint32_t ii = 0; ii < itemCount; ++ii)
            {
                scheduler.destoryTask(tasks[ii]);
            }
        }
    }

    //--------------------------------------------------------------------------
    void pushStealPopRace_ManyThreads(TQueue& queue)
    {
        WorkerPool workerPool;
        workerPool.initialize(1);

        MicroScheduler scheduler;
        scheduler.initialize(&workerPool);

        for (uint32_t testIter = 0; testIter < PARALLEL_ITERATIONS; ++testIter)
        {
            const uint32_t itemCount = 4;
            const uint32_t threadCount = std::thread::hardware_concurrency() - 1;

            std::vector<EmptyTask*> tasks(itemCount);
            for (uint32_t ii = 0; ii < itemCount; ++ii)
            {
                tasks[ii] = scheduler.allocateTask<EmptyTask>();
            }

            // steal on consumer threads.
            std::vector<std::thread*> consumers(threadCount);
            std::vector<std::unordered_set<Task*>> stolenValues(threadCount + 1);

            gts::Atomic<bool> startTest(false);
            bool done = false;

            for (uint32_t tt = 0; tt < threadCount; ++tt)
            {
                consumers[tt] = new std::thread([&queue, &stolenValues, &startTest, &done, tt]()
                {
                    while (!startTest.load(memory_order::acquire))
                    {
                    }

                    std::unordered_set<Task*>& localValues = stolenValues[tt];

                    while (true)
                    {
                        if (done && queue.empty())
                        {
                            break;
                        }

                        Task* val;
                        if (queue.trySteal(val))
                        {
                            ASSERT_TRUE(localValues.find(val) == localValues.end());
                            localValues.insert(val);
                        }
                    }
                });
            }

            startTest.store(true, memory_order::release);

            // produce and take on main thread
            std::unordered_set<Task*>& localPopValues = stolenValues[threadCount];
            for (uint32_t ii = 0; ii < itemCount; ++ii)
            {
                // produce
                while (!queue.tryPush(tasks[ii]))
                {
                }

                // try take
                Task* val;
                if (queue.tryPop(val))
                {
                    ASSERT_TRUE(localPopValues.find(val) == localPopValues.end());
                    localPopValues.insert(val);
                }
            }

            done = true;

            for (uint32_t tt = 0; tt < threadCount; ++tt)
            {
                consumers[tt]->join();
                delete consumers[tt];
            }

            std::unordered_set<Task*> values;
            for (uint32_t ii = 0; ii < stolenValues.size(); ++ii)
            {
                for (Task* val : stolenValues[ii])
                {
                    values.insert(val);
                }
            }

            // Verify that all values were received
            for (uint32_t ii = 0; ii < itemCount; ++ii)
            {
                GTS_ASSERT(values.find(tasks[ii]) != values.end());
                //ASSERT_TRUE(values.find(tasks[ii]) != values.end());
            }

            for (uint32_t ii = 0; ii < itemCount; ++ii)
            {
                scheduler.destoryTask(tasks[ii]);
            }
        }
    }
};

#if GTS_MSVC
#pragma warning( pop )
#endif