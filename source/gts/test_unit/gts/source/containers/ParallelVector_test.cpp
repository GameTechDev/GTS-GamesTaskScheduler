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
#include <thread>
#include <unordered_set>
#include <unordered_map>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "gts/containers/parallel/ParallelVector.h"

#define COMPARE_CONC_VECTOR 0 // rough performance comparison.

#if COMPARE_CONC_VECTOR && GTS_MSVC
#include <concurrent_vector.h>
#endif

#include "testers/TestersCommon.h"

using namespace gts;

// TODO: verify construction/destruction counts.

namespace {

//------------------------------------------------------------------------------
struct DummyObject
{
    DummyObject()
        :
        m_value(0)
    {
        s_constructorCount++;
    }

    explicit DummyObject(size_t val)
        :
        m_value(val)
    {
        s_constructorCount++;
    }

    DummyObject(DummyObject const& other)
        :
        m_value(other.m_value)
    {
        s_constructorCount++;
    }

    DummyObject(DummyObject&& other)
        :
        m_value(other.m_value)
    {
    }

    DummyObject& operator=(DummyObject const& rhs) = default;

    void operator=(size_t val)
    {
        m_value = val;
    }

    ~DummyObject()
    {
        s_destructorCount++;
    }

    bool operator==(DummyObject const& rhs) const
    {
        return m_value == rhs.m_value;
    }

    bool operator==(size_t rhs) const
    {
        return m_value == rhs;
    }

    operator size_t()
    {
        return m_value;
    }

    operator size_t() const
    {
        return m_value;
    }

    static void reset()
    {
        s_constructorCount = 0;
        s_destructorCount = 0;
    }

    static size_t s_constructorCount;
    static size_t s_destructorCount;

private:

    size_t m_value;
};

size_t DummyObject::s_constructorCount = 0;
size_t DummyObject::s_destructorCount = 0;

} // namespace

namespace testing {

static const size_t PARALLEL_VECTOR_TEST_SIZE = Thread::getHardwareThreadCount() * 3 + 1;

//------------------------------------------------------------------------------
TEST(ParallelVector, defaultConstructor)
{
    ParallelVector<DummyObject> vec;
    ASSERT_EQ(vec.capacity(), 0u);
    ASSERT_EQ(vec.size(), 0u);
}

//------------------------------------------------------------------------------
TEST(ParallelVector, copyConstructor)
{
    const size_t size = PARALLEL_VECTOR_TEST_SIZE;

    DummyObject::reset();
    {
        ParallelVector<DummyObject> vec1;
        for (size_t ii = 0; ii < size; ++ii)
        {
            DummyObject obj(ii);
            vec1.push_back(obj);
        }

        ParallelVector<DummyObject> vec2(vec1);

        // Verify elements
        for (size_t ii = 0; ii < size; ++ii)
        {
            auto reader1 = vec1.cat(ii);
            ASSERT_EQ(reader1.get(), ii);

            auto reader2 = vec2.cat(ii);
            ASSERT_EQ(reader2.get(), ii);
        }
    }
}

//------------------------------------------------------------------------------
TEST(ParallelVector, moveConstructor)
{
    const size_t size = PARALLEL_VECTOR_TEST_SIZE;

    DummyObject::reset();
    {
        ParallelVector<DummyObject> vec1;
        for (size_t ii = 0; ii < size; ++ii)
        {
            DummyObject obj(ii);
            vec1.push_back(obj);
        }

        ParallelVector<DummyObject> vec2(std::move(vec1));

        ASSERT_TRUE(vec1.empty());

        // Verify elements
        for (size_t ii = 0; ii < size; ++ii)
        {
            auto reader2 = vec2.cat(ii);
            ASSERT_EQ(reader2.get(), ii);
        }
    }
}

//------------------------------------------------------------------------------
TEST(ParallelVector, copyAssign)
{
    const size_t size = PARALLEL_VECTOR_TEST_SIZE;

    DummyObject::reset();
    {
        ParallelVector<DummyObject> vec1;
        for (size_t ii = 0; ii < size; ++ii)
        {
            DummyObject obj(ii);
            vec1.push_back(obj);
        }

        // Give vec2 some initial data to make sure it gets destroyed.
        ParallelVector<DummyObject> vec2;
        for (size_t ii = size; ii < size * 2; ++ii)
        {
            DummyObject obj(ii);
            vec2.push_back(obj);
        }

        vec2 = vec1;

        // Verify elements
        for (size_t ii = 0; ii < size; ++ii)
        {
            auto reader1 = vec1.cat(ii);
            ASSERT_EQ(reader1.get(), ii);

            auto reader2 = vec2.cat(ii);
            ASSERT_EQ(reader2.get(), ii);
        }
    }
}

//------------------------------------------------------------------------------
TEST(ParallelVector, moveAssign)
{
    const size_t size = PARALLEL_VECTOR_TEST_SIZE;

    DummyObject::reset();
    {
        ParallelVector<DummyObject> vec1;
        for (size_t ii = 0; ii < size; ++ii)
        {
            DummyObject obj(ii);
            vec1.push_back(obj);
        }

        // Give vec2 some initial data to make sure it gets destroyed.
        ParallelVector<DummyObject> vec2;
        for (size_t ii = size; ii < size * 2; ++ii)
        {
            DummyObject obj(ii);
            vec2.push_back(obj);
        }

        vec2 = std::move(vec1);

        ASSERT_TRUE(vec1.empty());

        // Verify elements
        for (size_t ii = 0; ii < size; ++ii)
        {
            auto reader2 = vec2.cat(ii);
            ASSERT_EQ(reader2.get(), ii);
        }
    }
}

//------------------------------------------------------------------------------
TEST(ParallelVector, copyPushBack)
{
    const size_t size = PARALLEL_VECTOR_TEST_SIZE;

    DummyObject::reset();
    {
        ParallelVector<DummyObject> vec;

        for (size_t ii = 0; ii < size; ++ii)
        {
            DummyObject obj(ii);
            vec.push_back(obj);
        }

        // Verify elements
        for (size_t ii = 0; ii < size; ++ii)
        {
            auto reader = vec.cat(ii);
            ASSERT_EQ(reader.get(), ii);
        }
    }
}

//------------------------------------------------------------------------------
TEST(ParallelVector, movePushBack)
{
    const size_t size = PARALLEL_VECTOR_TEST_SIZE;

    DummyObject::reset();
    {
        ParallelVector<DummyObject> vec;

        for (size_t ii = 0; ii < size; ++ii)
        {
            vec.push_back(DummyObject(ii));
        }

        // Verify elements
        for (size_t ii = 0; ii < size; ++ii)
        {
            auto reader = vec.cat(ii);
            ASSERT_EQ(reader.get(), ii);
        }
    }
}

//------------------------------------------------------------------------------
TEST(ParallelVector, atAndBrackets)
{
    const size_t frontVal = 1;
    ParallelVector<size_t> vec;
    vec.push_back(frontVal);

    ASSERT_EQ(vec.cat(0).get(), frontVal);
    ASSERT_EQ(vec.at(0).get(), frontVal);
    ASSERT_EQ(vec[0].get(), frontVal);

    vec.at(0).get() += 1;
    ASSERT_EQ(vec.at(0).get(), frontVal + 1);
    vec[0].get() += 1;
    ASSERT_EQ(vec[0].get(), frontVal + 2);

    ASSERT_FALSE(vec.cat(1).isValid());
    ASSERT_FALSE(vec.at(1).isValid());
    ASSERT_FALSE(vec[1].isValid());
}

//------------------------------------------------------------------------------
TEST(ParallelVector, frontAndBack)
{
    DummyObject::reset();
    {
        const size_t frontVal = 1;
        const size_t backVal = 2;
        ParallelVector<DummyObject> vec;
        vec.push_back(DummyObject(frontVal));
        vec.push_back(DummyObject(backVal));

        ASSERT_EQ(vec.cfront().get(), frontVal);
        ASSERT_EQ(vec.front().get(), frontVal);
        ASSERT_EQ(vec.cback().get(), backVal);
        ASSERT_EQ(vec.back().get(), backVal);
    }
}

//------------------------------------------------------------------------------
TEST(ParallelVector, popBack)
{
    const size_t size = PARALLEL_VECTOR_TEST_SIZE;

    DummyObject::reset();
    {
        ParallelVector<DummyObject> vec;

        for (size_t ii = 0; ii < size; ++ii)
        {
            vec.push_back(DummyObject(ii));
        }

        // Verify elements
        for (size_t ii = 0; ii < size; ++ii)
        {
            {
                auto reader = vec.cback();
                ASSERT_EQ(reader.get(), size_t(size - 1 - ii));
            }
            vec.pop_back();
        }

        ASSERT_TRUE(vec.empty());
    }
}

//------------------------------------------------------------------------------
TEST(ParallelVector, popBackAndGet)
{
    const size_t size = PARALLEL_VECTOR_TEST_SIZE;

    DummyObject::reset();
    {
        ParallelVector<DummyObject> vec;

        for (size_t ii = 0; ii < size; ++ii)
        {
            vec.push_back(DummyObject(ii));
        }

        // Verify elements
        for (size_t ii = 0; ii < size; ++ii)
        {
            auto result = vec.pop_back_and_get();
            ASSERT_TRUE(result.isValid);
            ASSERT_EQ(result.val, size_t(size - 1 - ii));
        }

        ASSERT_TRUE(vec.empty());

        auto result = vec.pop_back_and_get();
        ASSERT_FALSE(result.isValid);
    }
}

//------------------------------------------------------------------------------
TEST(ParallelVector, beginAndEnd)
{
    DummyObject::reset();
    {
        const size_t frontVal = 1;
        ParallelVector<DummyObject> vec;
        vec.push_back(DummyObject(frontVal));

        ASSERT_EQ(*vec.cbegin(), frontVal);
        ASSERT_EQ(*vec.begin(), frontVal);
    }
}

//------------------------------------------------------------------------------
TEST(ParallelVector, constIteration)
{
    const size_t size = PARALLEL_VECTOR_TEST_SIZE;

    DummyObject::reset();
    {
        ParallelVector<DummyObject> vec;

        for (size_t ii = 0; ii < size; ++ii)
        {
            vec.push_back(DummyObject(ii));
        }

        // Verify elements
        size_t val = 0;
        for (auto iter = vec.cbegin(); iter != vec.cend(); ++iter)
        {
            ASSERT_EQ(*iter, DummyObject(val++));
        }

        // Verify element were unlocked during the previous iteration.
        for (auto iter = vec.cbegin(); iter != vec.cend(); ++iter)
        {
        }
    }
}

//------------------------------------------------------------------------------
TEST(ParallelVector, iteration)
{
    const size_t size = PARALLEL_VECTOR_TEST_SIZE;

    DummyObject::reset();
    {
        ParallelVector<DummyObject> vec;

        for (size_t ii = 0; ii < size; ++ii)
        {
            vec.push_back(DummyObject(ii));
        }

        // Verify elements
        size_t val = 0;
        for (auto iter = vec.begin(); iter != vec.end(); ++iter)
        {
            ASSERT_EQ(*iter, DummyObject(val++));
        }

        // Verify element were unlocked during the previous iteration.
        for (auto iter = vec.begin(); iter != vec.end(); ++iter)
        {
        }
    }
}

//------------------------------------------------------------------------------
TEST(ParallelVector, indexSwapOut)
{
    const size_t size = Thread::getHardwareThreadCount() + 1;

    DummyObject::reset();
    {
        ParallelVector<DummyObject> vec;

        for (size_t ii = 0; ii < size; ++ii)
        {
            vec.push_back(DummyObject(ii));
        }

        size_t val = size - 1;
        for (size_t ii = 0; ii < size - 1; ++ii)
        {
            vec.swap_out(0);
            // Verify front is the old back.
            ASSERT_EQ(vec.cfront().get(), val--);
        }

        vec.swap_out(0);
        ASSERT_TRUE(vec.empty());

        // Make sure swap_out on empty does not explode.
        vec.swap_out(0);
    }
}

//------------------------------------------------------------------------------
TEST(ParallelVector, iteratorSwapOut)
{
    const size_t size = Thread::getHardwareThreadCount() + 1;

    DummyObject::reset();
    {
        ParallelVector<DummyObject> vec;

        for (size_t ii = 0; ii < size; ++ii)
        {
            vec.push_back(DummyObject(ii));
        }

        size_t val = size - 1;
        auto iter = vec.begin();
        while (true)
        {
            vec.swap_out(iter);
            if (iter == vec.end())
            {
                break;
            }
            // Verify front is the old back.
            ASSERT_EQ(*iter, val--);
        }

        EXPECT_DEATH( { vec.swap_out(iter); }, "");
    }
}

//------------------------------------------------------------------------------
TEST(ParallelVector, clear)
{
    const size_t size = Thread::getHardwareThreadCount() + 1;

    DummyObject::reset();
    {
        ParallelVector<DummyObject> vec;

        for (size_t ii = 0; ii < size; ++ii)
        {
            vec.push_back(DummyObject(ii));
        }

        size_t capacity = vec.capacity();
        vec.clear();
        ASSERT_EQ(vec.size(), 0u);
        ASSERT_EQ(vec.capacity(), capacity);
    }
}

//------------------------------------------------------------------------------
TEST(ParallelVector, reserve)
{
    DummyObject::reset();
    {
        size_t newCapacity = 33;
        size_t numThreads = Thread::getHardwareThreadCount();
        size_t expectedCap = nextPow2(newCapacity / numThreads + newCapacity % numThreads) * numThreads;

        ParallelVector<DummyObject> vec;
        vec.reserve(newCapacity);
        ASSERT_EQ(vec.capacity(), expectedCap);
        ASSERT_TRUE(vec.empty());

        vec.reserve(3);
        ASSERT_EQ(vec.capacity(), expectedCap);
        ASSERT_TRUE(vec.empty());
    }
}

//------------------------------------------------------------------------------
TEST(ParallelVector, resize)
{
    DummyObject::reset();
    {
        DummyObject fill(1);

        ParallelVector<DummyObject> vec;
        vec.resize(33u, fill);
        ASSERT_GE(vec.capacity(), 33u);
        ASSERT_TRUE(vec.size() == 33);

        // Verify elements
        for (auto iter = vec.cbegin(); iter != vec.cend(); ++iter)
        {
            ASSERT_EQ(*iter, fill);
        }
    }
}

//------------------------------------------------------------------------------
void parallelAt(size_t threadCount, size_t itemCount)
{
    ParallelVector<DummyObject> vec;
    for (size_t ii = 0; ii < itemCount; ++ii)
    {
        vec.push_back(DummyObject(ii));
    }

    const size_t numReaderThreads = threadCount - 1;

    gts::Atomic<bool> startTest(false);

    // concurrently reading threads
    std::vector<std::thread*> threads;
    threads.resize(numReaderThreads);
    for (size_t tt = 0; tt < numReaderThreads; ++tt)
    {
        threads[tt] = new std::thread([tt, itemCount, &vec, &startTest]()
        {
            uint32_t randState = (uint32_t)(tt + 1);

            while (!startTest.load(memory_order::acquire))
            {
                GTS_PAUSE();
            }

            for (size_t ii = 0; ii < itemCount; ++ii)
            {
                size_t idx = fastRand(randState) % itemCount;
                auto reader = vec.cat(idx);
                ASSERT_EQ(reader.get(), idx);
            }
        });
    }

    startTest.store(true, memory_order::release);

    for (size_t ii = 0; ii < itemCount; ++ii)
    {
        auto reader = vec.cat(ii);
        ASSERT_EQ(reader.get(), ii);
    }

    for (size_t tt = 0; tt < numReaderThreads; ++tt)
    {
        threads[tt]->join();
        delete threads[tt];
    }
}

//------------------------------------------------------------------------------
TEST(ParallelVector, parallelAt_2Threads)
{
    for (size_t ii = 0; ii < PARALLEL_ITERATIONS; ++ii)
    {
        parallelAt(2, ITEM_COUNT);
    }
}

//------------------------------------------------------------------------------
TEST(ParallelVector, parallelAt_ManyThreads)
{
    for (size_t ii = 0; ii < PARALLEL_ITERATIONS; ++ii)
    {
        parallelAt(Thread::getHardwareThreadCount(), ITEM_COUNT);
    }
}

//------------------------------------------------------------------------------
void parallelPushBack(size_t threadCount, size_t itemCount)
{
    gts::ParallelVector<DummyObject> vec;

    gts::Atomic<bool> startTest(false);

    const size_t numWriterThreads = threadCount - 1;

    // concurrently writing threads
    std::vector<std::thread*> threads;
    threads.resize(numWriterThreads);
    for (size_t tt = 0; tt < numWriterThreads; ++tt)
    {
        threads[tt] = new std::thread([tt, itemCount, &vec, &startTest]()
        {
            while (!startTest.load(memory_order::acquire))
            {
                GTS_PAUSE();
            }

            size_t val = itemCount * tt;
            for (size_t ii = 0; ii < itemCount; ++ii)
            {
                vec.push_back(DummyObject(val++));
            }
        });
    }

    startTest.store(true, memory_order::release);

    // Work on main thread too so we don't oversubscribe.
    size_t val = itemCount * numWriterThreads;
    for (size_t ii = 0; ii < itemCount; ++ii)
    {
        vec.push_back(DummyObject(val++));
    }

    for (size_t tt = 0; tt < numWriterThreads; ++tt)
    {
        threads[tt]->join();
        delete threads[tt];
    }

    std::unordered_set<size_t> values;

    // Make sure all the items exist.
    for (size_t ii = 0; ii < itemCount * threadCount; ++ii)
    {
        values.insert(vec.cat(ii).get());
    }
    for (size_t ii = 0; ii < itemCount * threadCount; ++ii)
    {
        ASSERT_TRUE(values.find(ii) != values.end());
    }
}

//------------------------------------------------------------------------------
TEST(ParallelVector, parallelPushBack_2Threads)
{
    for (size_t ii = 0; ii < PARALLEL_ITERATIONS; ++ii)
    {
        parallelPushBack(2, ITEM_COUNT);
    }
}

//------------------------------------------------------------------------------
TEST(ParallelVector, parallelPushBack_ManyThreads)
{
    for (size_t ii = 0; ii < PARALLEL_ITERATIONS; ++ii)
    {
        parallelPushBack(Thread::getHardwareThreadCount(), ITEM_COUNT);
    }
}

//------------------------------------------------------------------------------
void parallelPopBack(size_t threadCount, size_t itemCount)
{
    gts::ParallelVector<size_t> vec;
    for (size_t ii = 0; ii < itemCount * threadCount; ++ii)
    {
        vec.push_back(ii);
    }

    gts::Atomic<bool> startTest(false);

    const size_t numWriterThreads = threadCount - 1;

    // concurrently writing threads
    std::vector<std::thread*> threads;
    threads.resize(numWriterThreads);
    for (size_t tt = 0; tt < numWriterThreads; ++tt)
    {
        threads[tt] = new std::thread([tt, itemCount, &vec, &startTest]()
        {
            while (!startTest.load(memory_order::acquire))
            {
                GTS_PAUSE();
            }

            for (size_t ii = 0; ii < itemCount; ++ii)
            {
                vec.pop_back();
            }
        });
    }

    startTest.store(true, memory_order::release);

    // Work on main thread too so we don't oversubscribe.
    for (size_t ii = 0; ii < itemCount; ++ii)
    {
        vec.pop_back();
    }

    for (size_t tt = 0; tt < numWriterThreads; ++tt)
    {
        threads[tt]->join();
        delete threads[tt];
    }

    ASSERT_EQ(vec.size(), 0u);
}

//------------------------------------------------------------------------------
TEST(ParallelVector, parallelPopBack_2Threads)
{
    for (size_t ii = 0; ii < PARALLEL_ITERATIONS; ++ii)
    {
        parallelPopBack(2, ITEM_COUNT);
    }
}

//------------------------------------------------------------------------------
TEST(ParallelVector, parallelPopBack_ManyThreads)
{
    for (size_t ii = 0; ii < PARALLEL_ITERATIONS; ++ii)
    {
        parallelPopBack(Thread::getHardwareThreadCount(), ITEM_COUNT);
    }
}

//------------------------------------------------------------------------------
void parallelSwapOut(size_t swapperCount, size_t itemCount)
{
    gts::ParallelVector<size_t> vec;
    for (size_t ii = 0; ii < itemCount * swapperCount; ++ii)
    {
        vec.push_back(ii);
    }

    gts::Atomic<bool> startTest(false);

    const size_t numSwapperThreads = swapperCount - 1;

    // concurrently writing threads
    std::vector<std::thread*> threads;
    threads.resize(numSwapperThreads);
    for (size_t tt = 0; tt < numSwapperThreads; ++tt)
    {
        threads[tt] = new std::thread([tt, itemCount, &vec, &startTest]()
        {
            while (!startTest.load(memory_order::acquire))
            {
                GTS_PAUSE();
            }

            for (size_t ii = 0; ii < itemCount; ++ii)
            {
                vec.swap_out(0);
            }
        });
    }

    startTest.store(true, memory_order::release);

    // Work on main thread too so we don't oversubscribe.
    for (size_t ii = 0; ii < itemCount; ++ii)
    {
        vec.swap_out(0);
    }

    for (size_t tt = 0; tt < numSwapperThreads; ++tt)
    {
        threads[tt]->join();
        delete threads[tt];
    }

    ASSERT_EQ(vec.size(), 0u);
}

//------------------------------------------------------------------------------
TEST(ParallelVector, parallelSwapOut_2Threads)
{
    for (size_t ii = 0; ii < PARALLEL_ITERATIONS; ++ii)
    {
        parallelSwapOut(2, ITEM_COUNT);
    }
}

//------------------------------------------------------------------------------
TEST(ParallelVector, parallelSwapOut_ManyThreads)
{
    for (size_t ii = 0; ii < PARALLEL_ITERATIONS; ++ii)
    {
        parallelSwapOut(Thread::getHardwareThreadCount(), ITEM_COUNT);
    }
}

//------------------------------------------------------------------------------
void parallelPushPop(size_t threadCount, size_t itemCount)
{
    gts::ParallelVector<size_t> vec(threadCount);

    gts::Atomic<bool> startTest(false);

    const size_t numSwapperThreads = threadCount - 1;

    // concurrently writing threads
    std::vector<std::thread*> threads;
    threads.resize(numSwapperThreads);
    for (size_t tt = 0; tt < numSwapperThreads; ++tt)
    {
        threads[tt] = new std::thread([tt, itemCount, &vec, &startTest]()
        {
            while (!startTest.load(memory_order::acquire))
            {
                GTS_PAUSE();
            }

            for (size_t ii = 0; ii < itemCount; ++ii)
            {
                vec.push_back(0);
                vec.pop_back();
            }
        });
    }

    startTest.store(true, memory_order::release);

    // Work on main thread too so we don't oversubscribe.
    for (size_t ii = 0; ii < itemCount; ++ii)
    {
        vec.push_back(0);
        vec.pop_back();
    }

    for (size_t tt = 0; tt < numSwapperThreads; ++tt)
    {
        threads[tt]->join();
        delete threads[tt];
    }

    ASSERT_EQ(vec.size(), 0u);
}

//------------------------------------------------------------------------------
TEST(ParallelVector, parallelPushPop_2Threads)
{
    for (size_t ii = 0; ii < PARALLEL_ITERATIONS; ++ii)
    {
        parallelPushPop(2, ITEM_COUNT);
    }
}

//------------------------------------------------------------------------------
TEST(ParallelVector, parallelPushPop_ManyThreads)
{
    for (size_t ii = 0; ii < PARALLEL_ITERATIONS; ++ii)
    {
        parallelPushPop(Thread::getHardwareThreadCount(), ITEM_COUNT);
    }
}

//------------------------------------------------------------------------------
void parallelPushAndPop(size_t popperCount, size_t pusherCount, size_t itemCount)
{
    gts::ParallelVector<size_t> vec(2);
    for (size_t ii = 0; ii < itemCount * popperCount; ++ii)
    {
        vec.push_back(ii);
    }

    size_t expectedSize = itemCount * pusherCount;

    gts::Atomic<bool> startTest(false);

    // concurrently reading threads
    std::vector<std::thread*> readers;
    readers.resize(pusherCount);
    for (size_t tt = 0; tt < pusherCount; ++tt)
    {
        readers[tt] = new std::thread([tt, itemCount, &vec, &startTest]()
        {
            while (!startTest.load(memory_order::acquire))
            {
                GTS_PAUSE();
            }

            for (size_t ii = 0; ii < itemCount; ++ii)
            {
                vec.push_back(ii);
            }
        });
    }

    const size_t numPopperThreads = popperCount - 1;

    // concurrently writing threads
    std::vector<std::thread*> writers;
    writers.resize(numPopperThreads);
    for (size_t tt = 0; tt < numPopperThreads; ++tt)
    {
        writers[tt] = new std::thread([tt, itemCount, &vec, &startTest]()
        {
            while (!startTest.load(memory_order::acquire))
            {
                GTS_PAUSE();
            }

            for (size_t ii = 0; ii < itemCount; ++ii)
            {
                vec.pop_back();
            }
        });
    }

    startTest.store(true, memory_order::release);

    // Write on main thread too so we don't oversubscribe.
    for (size_t ii = 0; ii < itemCount; ++ii)
    {
        vec.pop_back();
    }

    for (size_t tt = 0; tt < pusherCount; ++tt)
    {
        readers[tt]->join();
        delete readers[tt];
    }
    for (size_t tt = 0; tt < numPopperThreads; ++tt)
    {
        writers[tt]->join();
        delete writers[tt];
    }

    ASSERT_EQ(vec.size(), expectedSize);
}

//------------------------------------------------------------------------------
TEST(ParallelVector, parallelPushAndPop_1Popper_1Pusher)
{
    for (size_t ii = 0; ii < PARALLEL_ITERATIONS; ++ii)
    {
        parallelPushAndPop(1, 1, ITEM_COUNT);
    }
}

//------------------------------------------------------------------------------
TEST(ParallelVector, parallelPushAndPop_ManyPoppers_1Pusher)
{
    for (size_t ii = 0; ii < PARALLEL_ITERATIONS; ++ii)
    {
        parallelPushAndPop(gtsMax(1u, Thread::getHardwareThreadCount() - 1), 1, ITEM_COUNT);
    }
}

//------------------------------------------------------------------------------
TEST(ParallelVector, parallelPushAndPop_1Popper_ManyPushers)
{
    for (size_t ii = 0; ii < PARALLEL_ITERATIONS; ++ii)
    {
        parallelPushAndPop(1, gtsMax(1u, Thread::getHardwareThreadCount() - 1), ITEM_COUNT);
    }
}

//------------------------------------------------------------------------------
TEST(ParallelVector, parallelPushAndPop_ManyPoppers_ManyPushers)
{
    for (size_t ii = 0; ii < PARALLEL_ITERATIONS; ++ii)
    {
        parallelPushAndPop(gtsMax(1u, Thread::getHardwareThreadCount() / 2), gtsMax(1u, Thread::getHardwareThreadCount() / 2), ITEM_COUNT);
    }
}

//------------------------------------------------------------------------------
void parallelPushAndSwapOut(size_t swapperCount, size_t pusherCount, size_t itemCount)
{
    gts::ParallelVector<size_t> vec(2);
    for (size_t ii = 0; ii < itemCount * swapperCount; ++ii)
    {
        vec.push_back(ii);
    }

    size_t expectedSize = itemCount * pusherCount;

    gts::Atomic<bool> startTest(false);

    // concurrently reading threads
    std::vector<std::thread*> readers;
    readers.resize(pusherCount);
    for (size_t tt = 0; tt < pusherCount; ++tt)
    {
        readers[tt] = new std::thread([tt, itemCount, &vec, &startTest]()
            {
                while (!startTest.load(memory_order::acquire))
                {
                    GTS_PAUSE();
                }

                for (size_t ii = 0; ii < itemCount; ++ii)
                {
                    vec.push_back(ii);
                }
            });
    }

    const size_t numSwapperThreads = swapperCount - 1;

    // concurrently writing threads
    std::vector<std::thread*> writers;
    writers.resize(numSwapperThreads);
    for (size_t tt = 0; tt < numSwapperThreads; ++tt)
    {
        writers[tt] = new std::thread([tt, itemCount, &vec, &startTest]()
        {
            while (!startTest.load(memory_order::acquire))
            {
                GTS_PAUSE();
            }

            for (size_t ii = 0; ii < itemCount; ++ii)
            {
                vec.swap_out(0);
            }
        });
    }

    startTest.store(true, memory_order::release);

    // Write on main thread too so we don't oversubscribe.
    for (size_t ii = 0; ii < itemCount; ++ii)
    {
        vec.swap_out(0);
    }

    for (size_t tt = 0; tt < pusherCount; ++tt)
    {
        readers[tt]->join();
        delete readers[tt];
    }
    for (size_t tt = 0; tt < numSwapperThreads; ++tt)
    {
        writers[tt]->join();
        delete writers[tt];
    }

    ASSERT_EQ(vec.size(), expectedSize);
}

//------------------------------------------------------------------------------
TEST(ParallelVector, parallelPushAndSwapOut_1Popper_1Pusher)
{
    for (size_t ii = 0; ii < PARALLEL_ITERATIONS; ++ii)
    {
        parallelPushAndSwapOut(1, 1, ITEM_COUNT);
    }
}

//------------------------------------------------------------------------------
TEST(ParallelVector, parallelPushAndSwapOut_ManyPoppers_1Pusher)
{
    for (size_t ii = 0; ii < PARALLEL_ITERATIONS; ++ii)
    {
        parallelPushAndSwapOut(gtsMax(1u, Thread::getHardwareThreadCount() - 1), 1, ITEM_COUNT);
    }
}

//------------------------------------------------------------------------------
TEST(ParallelVector, parallelPushAndSwapOut_1Popper_ManyPushers)
{
    for (size_t ii = 0; ii < PARALLEL_ITERATIONS; ++ii)
    {
        parallelPushAndSwapOut(1, gtsMax(1u, Thread::getHardwareThreadCount() - 1), ITEM_COUNT);
    }
}

//------------------------------------------------------------------------------
TEST(ParallelVector, parallelPushAndSwapOut_ManyPoppers_ManyPushers)
{
    for (size_t ii = 0; ii < PARALLEL_ITERATIONS; ++ii)
    {
        parallelPushAndSwapOut(gtsMax(1u, Thread::getHardwareThreadCount() / 2), gtsMax(1u, Thread::getHardwareThreadCount() / 2), ITEM_COUNT);
    }
}

//------------------------------------------------------------------------------
void parallelPushAndSwapOutAndIterate(size_t swapperCount, size_t pusherCount, size_t iteratorCount, size_t itemCount)
{
    gts::ParallelVector<size_t> vec(2);
    for (size_t ii = 0; ii < itemCount * swapperCount; ++ii)
    {
        vec.push_back(ii);
    }

    size_t expectedSize = itemCount * pusherCount;

    gts::Atomic<bool> startTest(false);

    // concurrently pushing threads
    std::vector<std::thread*> pushers;
    pushers.resize(pusherCount);
    for (size_t tt = 0; tt < pusherCount; ++tt)
    {
        pushers[tt] = new std::thread([tt, itemCount, &vec, &startTest]()
        {
            while (!startTest.load(memory_order::acquire))
            {
                GTS_PAUSE();
            }

            for (size_t ii = 0; ii < itemCount; ++ii)
            {
                vec.push_back(ii);
            }
        });
    }

    Atomic<bool> stopIteration = { false };

    // concurrently iterating threads
    std::vector<std::thread*> iterators;
    iterators.resize(iteratorCount);
    for (size_t tt = 0; tt < iteratorCount; ++tt)
    {
        iterators[tt] = new std::thread([tt, itemCount, &vec, &startTest, &stopIteration]()
        {
            while (!startTest.load(memory_order::acquire))
            {
                GTS_PAUSE();
            }

            while(stopIteration.load(memory_order::acquire) != true)
            {
                for (auto iter = vec.begin(); iter != vec.end(); ++iter)
                {}
            }
        });
    }

    const size_t numSwapperThreads = swapperCount - 1;

    // concurrently swapping threads
    std::vector<std::thread*> swappers;
    swappers.resize(numSwapperThreads);
    for (size_t tt = 0; tt < numSwapperThreads; ++tt)
    {
        swappers[tt] = new std::thread([tt, itemCount, &vec, &startTest]()
        {
            while (!startTest.load(memory_order::acquire))
            {
                GTS_PAUSE();
            }

            for (size_t ii = 0; ii < itemCount; ++ii)
            {
                vec.swap_out(0);
            }
        });
    }

    startTest.store(true, memory_order::release);

    // Write on main thread too so we don't oversubscribe.
    for (size_t ii = 0; ii < itemCount; ++ii)
    {
        vec.swap_out(0);
    }

    for (size_t tt = 0; tt < pusherCount; ++tt)
    {
        pushers[tt]->join();
        delete pushers[tt];
    }

    for (size_t tt = 0; tt < numSwapperThreads; ++tt)
    {
        swappers[tt]->join();
        delete swappers[tt];
    }

    stopIteration.store(true, memory_order::release);
    for (size_t tt = 0; tt < iteratorCount; ++tt)
    {
        iterators[tt]->join();
        delete iterators[tt];
    }

    ASSERT_EQ(vec.size(), expectedSize);
}

//------------------------------------------------------------------------------
TEST(ParallelVector, parallelPushAndSwapOutAndIterate_1Each)
{
    for (size_t ii = 0; ii < PARALLEL_ITERATIONS; ++ii)
    {
        parallelPushAndSwapOutAndIterate(1, 1, 1, ITEM_COUNT);
    }
}

//------------------------------------------------------------------------------
TEST(ParallelVector, parallelPushAndSwapOut_ManyEach)
{
    for (size_t ii = 0; ii < PARALLEL_ITERATIONS; ++ii)
    {
        size_t numThreads = gtsMax(1u, Thread::getHardwareThreadCount() / 3);
        parallelPushAndSwapOutAndIterate(numThreads, numThreads, numThreads, ITEM_COUNT);
    }
}

} // namespace testing
