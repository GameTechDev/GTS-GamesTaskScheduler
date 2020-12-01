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

#include "gts/containers/parallel/ParallelHashTable.h"

#define COMPARE_CONC_UNORDERED_MAP 0 // rough performance comparison.

#if COMPARE_CONC_UNORDERED_MAP && GTS_MSVC
#include <concurrent_unordered_map.h>
#endif

#include "testers/TestersCommon.h"
#include "SchedulerTestsCommon.h"

using namespace gts;

// TODO: verify construction/destruction counts.

namespace {

//------------------------------------------------------------------------------
// Maps to the keys to zero.
template<class TKey>
struct ZeroHash
{
    using hashed_value = size_t;

    GTS_INLINE hashed_value operator()(const TKey&) const
    {
        return hashed_value(0);
    }
};

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

    operator uintptr_t()
    {
        return (uintptr_t)m_value;
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

//------------------------------------------------------------------------------
TEST(ParallelHashTable, defaultConstructor)
{
    ParallelHashTable<size_t, DummyObject> ht;
    ASSERT_EQ(ht.capacity(), size_t(0));
}

//------------------------------------------------------------------------------
TEST(ParallelHashTable, copyConstructor)
{
    const size_t size = 32;

    DummyObject::reset();
    {
        ParallelHashTable<size_t, DummyObject> ht1;

        for (size_t ii = 0; ii < size; ++ii)
        {
            ParallelHashTable<size_t, DummyObject>::value_type obj{ii, DummyObject(ii)};
            auto iter = ht1.insert(obj);
        }

        ParallelHashTable<size_t, DummyObject> ht2(ht1);

        // Verify elements
        for (size_t ii = 0; ii < size; ++ii)
        {
            auto iter1 = ht1.cfind(ii);
            ASSERT_TRUE(iter1 != ht1.end());
            ASSERT_EQ(iter1->value, ii);

            auto iter2 = ht2.cfind(ii);
            ASSERT_TRUE(iter2 != ht2.end());
            ASSERT_EQ(iter2->value, ii);
        }
    }
}

//------------------------------------------------------------------------------
TEST(ParallelHashTable, moveConstructor)
{
    const size_t size = 32;

    DummyObject::reset();
    {
        ParallelHashTable<size_t, DummyObject> ht1;

        for (size_t ii = 0; ii < size; ++ii)
        {
            ParallelHashTable<size_t, DummyObject>::value_type obj{ii, DummyObject(ii)};
            auto iter = ht1.insert(obj);
        }

        ParallelHashTable<size_t, DummyObject> ht2(std::move(ht1));

        // Verify elements
        for (size_t ii = 0; ii < size; ++ii)
        {
            auto iter1 = ht1.cfind(ii);
            ASSERT_TRUE(iter1 == ht1.end());

            auto iter2 = ht2.cfind(ii);
            ASSERT_TRUE(iter2 != ht2.end());
            ASSERT_EQ(iter2->value, ii);
        }
    }
}

//------------------------------------------------------------------------------
TEST(ParallelHashTable, copyAssignment)
{
    const size_t size = 32;

    DummyObject::reset();
    {
        ParallelHashTable<size_t, DummyObject> ht1;

        for (size_t ii = 0; ii < size; ++ii)
        {
            ParallelHashTable<size_t, DummyObject>::value_type obj{ii, DummyObject(ii)};
            auto iter = ht1.insert(obj);
        }

        ParallelHashTable<size_t, DummyObject> ht2;

        for (size_t ii = size; ii < size * 2; ++ii)
        {
            ParallelHashTable<size_t, DummyObject>::value_type obj{ii, DummyObject(ii)};
            auto iter = ht2.insert(obj);
        }

        ht2 = ht1;

        // Verify elements
        for (size_t ii = 0; ii < size; ++ii)
        {
            auto iter1 = ht1.cfind(ii);
            ASSERT_TRUE(iter1 != ht1.end());
            ASSERT_EQ(iter1->value, ii);

            auto iter2 = ht2.cfind(ii);
            ASSERT_TRUE(iter2 != ht2.end());
            ASSERT_EQ(iter2->value, ii);
        }
    }
}

//------------------------------------------------------------------------------
TEST(ParallelHashTable, moveAssignment)
{
    const size_t size = 1;

    DummyObject::reset();
    {
        ParallelHashTable<size_t, DummyObject> ht1;

        for (size_t ii = 0; ii < size; ++ii)
        {
            ParallelHashTable<size_t, DummyObject>::value_type obj{ii, DummyObject(ii)};
            auto iter = ht1.insert(obj);
        }

        ParallelHashTable<size_t, DummyObject> ht2;

        for (size_t ii = size; ii < size * 2; ++ii)
        {
            ParallelHashTable<size_t, DummyObject>::value_type obj{ii, DummyObject(ii)};
            auto iter = ht2.insert(obj);
        }

        ht2 = std::move(ht1);

        // Verify elements
        for (size_t ii = 0; ii < size; ++ii)
        {
            auto iter1 = ht1.cfind(ii);
            ASSERT_TRUE(iter1 == ht1.end());

            auto iter2 = ht2.cfind(ii);
            ASSERT_TRUE(iter2 != ht2.end());
            ASSERT_EQ(iter2->value, ii);
        }
    }
}

//------------------------------------------------------------------------------
TEST(ParallelHashTable, copyInsert)
{
    const size_t size = 32;

    DummyObject::reset();
    {
        ParallelHashTable<size_t, DummyObject> ht;

        for (size_t ii = 0; ii < size; ++ii)
        {
            ParallelHashTable<size_t, DummyObject>::value_type obj{ii, DummyObject(ii)};
            auto iter = ht.insert(obj);
            ASSERT_TRUE(iter != ht.end());
        }

        // Verify elements
        for (size_t ii = 0; ii < size; ++ii)
        {
            auto iter = ht.cfind(ii);
            ASSERT_TRUE(iter != ht.end());
            ASSERT_EQ(iter->value, ii);
        }
    }
}

//------------------------------------------------------------------------------
TEST(ParallelHashTable, moveInsert)
{
    const size_t size = 32;

    DummyObject::reset();
    {
        ParallelHashTable<size_t, DummyObject> ht;

        for (size_t ii = 0; ii < size; ++ii)
        {
            auto iter = ht.insert({ii, DummyObject(ii)});
            ASSERT_TRUE(iter != ht.end());
        }

        // Verify elements
        for (size_t ii = 0; ii < size; ++ii)
        {
            auto iter = ht.cfind(ii);
            ASSERT_TRUE(iter != ht.end());
            ASSERT_EQ(iter->value, ii);
        }
    }
}

//------------------------------------------------------------------------------
TEST(ParallelHashTable, constIteration)
{
    const size_t size = 1;

    DummyObject::reset();
    {
        ParallelHashTable<size_t, DummyObject> ht;

        for (size_t ii = 0; ii < size; ++ii)
        {
            auto iter = ht.insert({ii, DummyObject(ii)});
            ASSERT_TRUE(iter != ht.end());
        }

        // Collect elements through iteration
        std::unordered_set<size_t> foundValues;
        for(auto iter = ht.cbegin(); iter != ht.cend(); ++iter)
        {
            foundValues.insert(iter->key);
        }

        // Verify iterators unlocked their mutexes.
        for(auto iter = ht.cbegin(); iter != ht.cend(); ++iter)
        {
        }

        // Verify elements
        for (size_t ii = 0; ii < size; ++ii)
        {
            ASSERT_TRUE(foundValues.find(ii) != foundValues.end());
        }
    }
}

//------------------------------------------------------------------------------
TEST(ParallelHashTable, iteration)
{
    const size_t size = 1;

    DummyObject::reset();
    {
        ParallelHashTable<size_t, DummyObject> ht;

        for (size_t ii = 0; ii < size; ++ii)
        {
            auto iter = ht.insert({ii, DummyObject(ii)});
            ASSERT_TRUE(iter != ht.end());
        }

        // Collect elements through iteration
        std::unordered_set<size_t> foundValues;
        for (auto iter = ht.begin(); iter != ht.end(); ++iter)
        {
            foundValues.insert(iter->key);
        }

        // Verify iterators unlocked their mutexes.
        for(auto iter = ht.begin(); iter != ht.end(); ++iter)
        {
        }

        // Verify elements
        for (size_t ii = 0; ii < size; ++ii)
        {
            ASSERT_TRUE(foundValues.find(ii) != foundValues.end());
        }
    }
}

//------------------------------------------------------------------------------
TEST(ParallelHashTable, insertDuplicate)
{
    const size_t size = 32;

    DummyObject::reset();
    {
        ParallelHashTable<size_t, DummyObject> ht;

        for (size_t ii = 0; ii < size; ++ii)
        {
            auto iter = ht.insert({ii, DummyObject(ii)});
            ASSERT_TRUE(iter != ht.end());
        }

        // Verify elements
        for (size_t ii = 0; ii < size; ++ii)
        {
            auto iter = ht.cfind(ii);
            ASSERT_TRUE(iter != ht.end());
            ASSERT_EQ(iter->value, ii);
        }

        // Verify cannot duplicate
        for (size_t ii = 0; ii < size; ++ii)
        {
            auto iter = ht.insert({ii, DummyObject(ii)});
            ASSERT_TRUE(iter == ht.end());
        }

        // Verify elements
        for (size_t ii = 0; ii < size; ++ii)
        {
            auto iter = ht.cfind(ii);
            ASSERT_TRUE(iter != ht.end());
            ASSERT_EQ(iter->value, ii);
        }
    }
}

//------------------------------------------------------------------------------
TEST(ParallelHashTable, insert_or_assign)
{
    const size_t size = 32;

    DummyObject::reset();
    {
        ParallelHashTable<size_t, DummyObject> ht;

        for (size_t ii = 0; ii < size; ++ii)
        {
            auto iter = ht.insert({ii, DummyObject(ii)});
            ASSERT_TRUE(iter != ht.end());
        }

        // Verify elements
        for (size_t ii = 0; ii < size; ++ii)
        {
            auto iter = ht.cfind(ii);
            ASSERT_TRUE(iter != ht.end());
            ASSERT_EQ(iter->value, ii);
        }

        // Verify cannot duplicate
        for (size_t ii = 0; ii < size; ++ii)
        {
            auto iter = ht.insert_or_assign({ii, DummyObject(ii)});
            ASSERT_TRUE(iter != ht.end());
        }

        // Verify elements
        for (size_t ii = 0; ii < size; ++ii)
        {
            auto iter = ht.cfind(ii);
            ASSERT_TRUE(iter != ht.end());
            ASSERT_EQ(iter->value, ii);
        }
    }
}

//------------------------------------------------------------------------------
TEST(ParallelHashTable, find)
{
    const size_t size = 32;

    DummyObject::reset();
    {
        ParallelHashTable<size_t, DummyObject> ht;

        for (size_t ii = 0; ii < size; ++ii)
        {
            auto iter = ht.insert({ii, DummyObject(ii)});
            ASSERT_TRUE(iter != ht.end());
        }

        // Zero out elements
        for (size_t ii = 0; ii < size; ++ii)
        {
            auto iter = ht.find(ii);
            iter->value = 0;
        }

        // Verify elements
        for (size_t ii = 0; ii < size; ++ii)
        {
            auto iter = ht.cfind(ii);
            ASSERT_TRUE(iter != ht.end());
            ASSERT_EQ(iter->value, 0);
        }
    }
}

//------------------------------------------------------------------------------
TEST(ParallelHashTable, eraseKey)
{
    DummyObject::reset();

    const size_t size = 32;

    ParallelHashTable<size_t, DummyObject> ht;

    for (size_t ii = 0; ii < size; ++ii)
    {
        auto iter = ht.insert({ii, DummyObject(ii)});
        ASSERT_TRUE(iter != ht.end());
    }

    size_t capacity = ht.capacity();

    // Remove all elements
    for (size_t ii = 0; ii < size; ++ii)
    {
        size_t numErased = ht.erase(ii);
        ASSERT_TRUE(numErased == 1);
    }

    // Verify elements no longer exist.
    for (size_t ii = 0; ii < size; ++ii)
    {
        auto iter = ht.cfind(ii);
        ASSERT_TRUE(iter == ht.end());
    }

    // Capacity is unchanged.
    ASSERT_EQ(ht.capacity(), capacity);
}

//------------------------------------------------------------------------------
TEST(ParallelHashTable, eraseIterator)
{
    DummyObject::reset();

    const size_t size = 32;

    ParallelHashTable<size_t, DummyObject> ht;

    for (size_t ii = 0; ii < size; ++ii)
    {
        auto iter = ht.insert({ii, DummyObject(ii)});
        ASSERT_TRUE(iter != ht.end());
    }

    size_t capacity = ht.capacity();

    // Remove all elements
    {
        auto iter = ht.begin();
        for (size_t ii = 0; ii < size; ++ii)
        {
            iter = ht.erase(iter);
        }
        ASSERT_TRUE(iter == ht.end());
    }

    // Verify elements no longer exist.
    for (size_t ii = 0; ii < size; ++ii)
    {
        auto iter = ht.cfind(ii);
        ASSERT_TRUE(iter == ht.end());
    }

    // Capacity is unchanged.
    ASSERT_EQ(ht.capacity(), capacity);
}

//------------------------------------------------------------------------------
TEST(ParallelHashTable, tombstones)
{
    DummyObject::reset();

    const size_t size = 3;
    ParallelHashTable<size_t, DummyObject, ZeroHash<size_t>> ht;

    // Insert: { 0, 1, 2 }
    for (size_t ii = 0; ii < size; ++ii)
    {
        ht.insert({ii, DummyObject(ii)});
    }

    size_t capacity = ht.capacity();

    // Remove 1: { 0, x, 2 }
    size_t numErased = ht.erase(1);
    ASSERT_TRUE(numErased == 1);

    // Re-insert 1: { 0, 1, 2 }
    auto iter = ht.insert({1, DummyObject(1)});
    ASSERT_TRUE(iter != ht.end());

    // Capacity is unchanged.
    ASSERT_EQ(ht.capacity(), capacity);

    // Insert 3: { 0, 1, 2. 3 }
    iter = ht.insert({3, DummyObject(3)});
    ASSERT_TRUE(iter != ht.end());
}

//------------------------------------------------------------------------------
void parallelTryFind(size_t threadCount, size_t itemCount)
{
    ParallelHashTable<size_t, DummyObject> ht;
    for (size_t ii = 0; ii < itemCount; ++ii)
    {
        ht.insert({ii, DummyObject(ii)});
    }

    const size_t numReaderThreads = threadCount - 1;

    gts::Atomic<bool> startProduction(false);

    // concurrently reading threads
    std::vector<std::thread*> readers;
    readers.resize(numReaderThreads);
    for (size_t tt = 0; tt < numReaderThreads; ++tt)
    {
        readers[tt] = new std::thread([tt, itemCount, &ht, &startProduction]()
        {
            uint32_t randState = (uint32_t)(tt + 1);

            while (!startProduction.load(memory_order::acquire))
            {
                GTS_PAUSE();
            }

            for (size_t ii = 0; ii < itemCount; ++ii)
            {
                size_t idx = fastRand(randState) % itemCount;
                auto iter = ht.cfind(idx);
                ASSERT_TRUE(iter != ht.end());
                ASSERT_EQ(iter->value, idx);
            }
        });
    }

    startProduction.store(true, memory_order::release);

    ParallelHashTable<size_t, DummyObject>::value_type out;
    uint32_t randState = (uint32_t)numReaderThreads;

    for (size_t ii = 0; ii < itemCount; ++ii)
    {
        size_t idx = fastRand(randState) % itemCount;
        auto iter = ht.cfind(idx);
        ASSERT_TRUE(iter != ht.end());
        ASSERT_EQ(iter->value, idx);
    }

    for (size_t tt = 0; tt < numReaderThreads; ++tt)
    {
        readers[tt]->join();
        delete readers[tt];
    }
}

//------------------------------------------------------------------------------
TEST(ParallelHashTable, parallelTryFind_2Threads)
{
    for (size_t ii = 0; ii < PARALLEL_ITERATIONS; ++ii)
    {
        parallelTryFind(2, ITEM_COUNT);
    }
}

//------------------------------------------------------------------------------
TEST(ParallelHashTable, parallelTryFind_ManyThreads)
{
    for (size_t ii = 0; ii < PARALLEL_ITERATIONS; ++ii)
    {
        parallelTryFind(Thread::getHardwareThreadCount() - 1, ITEM_COUNT);
    }
}

#if COMPARE_CONC_UNORDERED_MAP
//------------------------------------------------------------------------------
void parallelUnorderedMapFind(size_t threadCount, size_t itemCount)
{
    concurrency::concurrent_unordered_map<size_t, DummyObject> ht;
    for (size_t ii = 0; ii < itemCount; ++ii)
    {
        ht.insert({ii, DummyObject(ii)});
    }

    const size_t numReaderThreads = threadCount - 1;

    gts::Atomic<bool> startProduction(false);

    // producers with write contention
    std::vector<std::thread*> readers;
    readers.resize(numReaderThreads);
    for (size_t tt = 0; tt < numReaderThreads; ++tt)
    {
        readers[tt] = new std::thread([tt, itemCount, &ht, &startProduction]()
        {
            uint32_t randState = (uint32_t)(tt + 1);

            while (!startProduction.load())
            {
                GTS_PAUSE();
            }

            for (size_t ii = 0; ii < itemCount; ++ii)
            {
                size_t idx = fastRand(randState) % itemCount;
                DummyObject out = ht.at(idx);
                ASSERT_EQ(out, idx);
            }
        });
    }

    startProduction.store(true);

    uint32_t randState = (uint32_t)umReaderThreads;
    for (size_t ii = 0; ii < itemCount; ++ii)
    {
        size_t idx = fastRand(randState) % itemCount;
        DummyObject out = ht.at(idx);
        ASSERT_EQ(out, idx);
    }

    for (size_t tt = 0; tt < numReaderThreads; ++tt)
    {
        readers[tt]->join();
        delete readers[tt];
    }
}

//------------------------------------------------------------------------------
TEST(ParallelHashTable, parallelUnorderedMapFind_ManyThreads)
{
    for (size_t ii = 0; ii < PARALLEL_ITERATIONS; ++ii)
    {
        parallelUnorderedMapFind(Thread::getHardwareThreadCount(), ITEM_COUNT);
    }
}

#endif

//------------------------------------------------------------------------------
void parallelTryInsert(size_t writerCount, size_t itemCount)
{
    ParallelHashTable<size_t, DummyObject> ht;

    gts::Atomic<bool> startProduction(false);

    const size_t numWriterThreads = writerCount - 1;

    // concurrently writing threads
    std::vector<std::thread*> writers;
    writers.resize(numWriterThreads);
    for (size_t tt = 0; tt < numWriterThreads; ++tt)
    {
        writers[tt] = new std::thread([tt, itemCount, &ht, &startProduction]()
        {
            while (!startProduction.load(memory_order::acquire))
            {
                GTS_PAUSE();
            }

            size_t val = (size_t)itemCount * tt;
            for (size_t ii = 0; ii < itemCount; ++ii)
            {
                auto iter = ht.insert({val, DummyObject(val)});
                ASSERT_TRUE(iter != ht.end());
                val++;
            }
        });
    }

    startProduction.store(true, memory_order::release);
    
    // Work on main thread too so we don't oversubscribe.
    size_t val = (size_t)(itemCount * numWriterThreads);
    for (size_t ii = 0; ii < itemCount; ++ii)
    {
        auto iter = ht.insert({val, DummyObject(val)});
        ASSERT_TRUE(iter != ht.end());
        val++;
    }

    for (size_t tt = 0; tt < numWriterThreads; ++tt)
    {
        writers[tt]->join();
        delete writers[tt];
    }

    // Make sure all the items exist.
    for (size_t ii = 0; ii < itemCount * writerCount; ++ii)
    {
        ParallelHashTable<size_t, DummyObject>::value_type out;
        auto iter = ht.cfind(ii);
        ASSERT_TRUE(iter != ht.end());
        ASSERT_EQ(iter->value, ii);
    }
}

//------------------------------------------------------------------------------
TEST(ParallelHashTable, parallelTryInsert_2Threads)
{
    for (size_t ii = 0; ii < PARALLEL_ITERATIONS; ++ii)
    {
        parallelTryInsert(2, ITEM_COUNT);
    }
}

//------------------------------------------------------------------------------
TEST(ParallelHashTable, parallelTryInsert_ManyThreads)
{
    for (size_t ii = 0; ii < PARALLEL_ITERATIONS; ++ii)
    {
        parallelTryInsert(Thread::getHardwareThreadCount(), ITEM_COUNT);
    }
}

#if COMPARE_CONC_UNORDERED_MAP
//------------------------------------------------------------------------------
void parallelUnorderedMapTryInsert(size_t writerCount, size_t itemCount)
{
    concurrency::concurrent_unordered_map<size_t, DummyObject> ht;

    gts::Atomic<bool> startProduction(false);

    const size_t numWriterThreads = writerCount - 1;

    // concurrently writing threads
    std::vector<std::thread*> writers;
    writers.resize(numWriterThreads);
    for (size_t tt = 0; tt < numWriterThreads; ++tt)
    {
        writers[tt] = new std::thread([tt, itemCount, &ht, &startProduction]()
        {
            while (!startProduction.load())
            {
                GTS_PAUSE();
            }

            size_t val = (size_t)itemCount * tt;
            for (size_t ii = 0; ii < itemCount; ++ii)
            {
                auto iter = ht.insert({val, DummyObject(val)});
                ASSERT_TRUE(iter.second);
                val++;
            }
        });
    }

    startProduction.store(true);
    
    // Work on main thread too so we don't oversubscribe.
    size_t val = (size_t)(itemCount * numWriterThreads);
    for (size_t ii = 0; ii < itemCount; ++ii)
    {
        auto iter = ht.insert({val, DummyObject(val)});
        ASSERT_TRUE(iter.second);
        val++;
    }

    for (size_t tt = 0; tt < numWriterThreads; ++tt)
    {
        writers[tt]->join();
        delete writers[tt];
    }

    // Make sure all the items exist.
    for (size_t ii = 0; ii < itemCount * writerCount; ++ii)
    {
        DummyObject out = ht.at(ii);
        ASSERT_EQ(out, ii);
    }
}

//------------------------------------------------------------------------------
TEST(ParallelHashTable, parallelUnorderedMapTryInsert_ManyThreads)
{
    for (size_t ii = 0; ii < PARALLEL_ITERATIONS; ++ii)
    {
        parallelUnorderedMapTryInsert(Thread::getHardwareThreadCount(), ITEM_COUNT);
    }
}

#endif

//------------------------------------------------------------------------------
void parallelTryErase(size_t writerCount, size_t itemCount)
{
    ParallelHashTable<size_t, DummyObject> ht;

    for (size_t ii = 0; ii < itemCount * writerCount; ++ii)
    {
        ht.insert({ii, DummyObject(ii)});
    }

    gts::Atomic<bool> startProduction(false);

    const size_t numWriterThreads = writerCount - 1;

    // concurrently writing threads
    std::vector<std::thread*> writers;
    writers.resize(numWriterThreads);
    for (size_t tt = 0; tt < numWriterThreads; ++tt)
    {
        writers[tt] = new std::thread([tt, itemCount, &ht, &startProduction]()
        {
            while (!startProduction.load(memory_order::acquire))
            {
                GTS_PAUSE();
            }

            size_t val = (size_t)itemCount * tt;
            for (size_t ii = 0; ii < itemCount; ++ii)
            {
                size_t numErased = ht.erase(val);
                ASSERT_TRUE(numErased == 1);
                val++;
            }
        });
    }

    startProduction.store(true, memory_order::release);

    // Work on main thread too so we don't oversubscribe.
    size_t val = (size_t)(itemCount * numWriterThreads);
    for (size_t ii = 0; ii < itemCount; ++ii)
    {
        size_t numErased = ht.erase(val);
        ASSERT_TRUE(numErased == 1);
        val++;
    }

    for (size_t tt = 0; tt < numWriterThreads; ++tt)
    {
        writers[tt]->join();
        delete writers[tt];
    }

    // Make sure all the items are erased.
    for (size_t ii = 0; ii < itemCount * writerCount; ++ii)
    {
        ParallelHashTable<size_t, DummyObject>::value_type out;
        auto iter = ht.cfind(ii);
        ASSERT_TRUE(iter == ht.end());
    }
}

//------------------------------------------------------------------------------
TEST(ParallelHashTable, parallelTryErase_2Threads)
{
    for (size_t ii = 0; ii < PARALLEL_ITERATIONS; ++ii)
    {
        parallelTryErase(2, ITEM_COUNT);
    }
}

//------------------------------------------------------------------------------
TEST(ParallelHashTable, parallelTryErase_ManyThreads)
{
    for (size_t ii = 0; ii < PARALLEL_ITERATIONS; ++ii)
    {
        parallelTryErase(Thread::getHardwareThreadCount(), ITEM_COUNT);
    }
}

//------------------------------------------------------------------------------
void parallelTryFindAndInsert(size_t readerCount, size_t writerCount, size_t itemCount)
{
    ParallelHashTable<size_t, DummyObject> ht;
    for (size_t ii = 0; ii < itemCount; ++ii)
    {
        ht.insert({ii, DummyObject(ii)});
    }

    gts::Atomic<bool> startProduction(false);

    // concurrently reading threads
    std::vector<std::thread*> readers;
    readers.resize(readerCount);
    for (size_t tt = 0; tt < readerCount; ++tt)
    {
        readers[tt] = new std::thread([tt, itemCount, &ht, &startProduction]()
        {
            uint32_t randState = (uint32_t)(tt + 1);

            while (!startProduction.load(memory_order::acquire))
            {
                GTS_PAUSE();
            }

            for (size_t ii = 0; ii < itemCount; ++ii)
            {
                size_t idx = fastRand(randState) % itemCount;
                ParallelHashTable<size_t, DummyObject>::value_type out;
                auto iter = ht.cfind(idx);
                ASSERT_TRUE(iter != ht.end());
                ASSERT_EQ(iter->value, idx);
            }
        });
    }

    const size_t numWriterThreads = writerCount - 1;

    // concurrently writing threads
    std::vector<std::thread*> writers;
    writers.resize(numWriterThreads);
    for (size_t tt = 0; tt < numWriterThreads; ++tt)
    {
        writers[tt] = new std::thread([tt, itemCount, &ht, &startProduction]()
        {
            while (!startProduction.load(memory_order::acquire))
            {
                GTS_PAUSE();
            }

            size_t val = (size_t)itemCount * (tt + 1);
            for (size_t ii = 0; ii < itemCount; ++ii)
            {
                auto iter = ht.insert({val, DummyObject(val)});
                ASSERT_TRUE(iter != ht.end());
                val++;
            }
        });
    }

    startProduction.store(true, memory_order::release);

    // Write on main thread too so we don't oversubscribe.
    size_t val = (size_t)(itemCount * (numWriterThreads + 1));
    for (size_t ii = 0; ii < itemCount; ++ii)
    {
        auto iter = ht.insert({val, DummyObject(val)});
        ASSERT_TRUE(iter != ht.end());
        val++;
    }

    for (size_t tt = 0; tt < readerCount; ++tt)
    {
        readers[tt]->join();
        delete readers[tt];
    }
    for (size_t tt = 0; tt < numWriterThreads; ++tt)
    {
        writers[tt]->join();
        delete writers[tt];
    }
    
    // Make sure all the items exist.
    for (size_t ii = 0; ii < itemCount * (writerCount + 1); ++ii)
    {
        ParallelHashTable<size_t, DummyObject>::value_type out;
        auto iter = ht.cfind(ii);
        ASSERT_TRUE(iter != ht.end());
        ASSERT_EQ(iter->value, ii);
    }
}

//------------------------------------------------------------------------------
TEST(ParallelHashTable, parallelTryFindAndInsert_1Reader_1Writer)
{
    for (size_t ii = 0; ii < PARALLEL_ITERATIONS; ++ii)
    {
        parallelTryFindAndInsert(1, 1, ITEM_COUNT);
    }
}

//------------------------------------------------------------------------------
TEST(ParallelHashTable, parallelTryFindAndInsert_ManyReaders_1Writer)
{
    for (size_t ii = 0; ii < PARALLEL_ITERATIONS; ++ii)
    {
        parallelTryFindAndInsert(gtsMax(1u, Thread::getHardwareThreadCount() - 1), 1, ITEM_COUNT);
    }
}

//------------------------------------------------------------------------------
TEST(ParallelHashTable, parallelTryFindAndInsert_1Reader_ManyWriters)
{
    for (size_t ii = 0; ii < PARALLEL_ITERATIONS; ++ii)
    {
        parallelTryFindAndInsert(1, gtsMax(1u, Thread::getHardwareThreadCount() - 1), ITEM_COUNT);
    }
}

//------------------------------------------------------------------------------
TEST(ParallelHashTable, parallelTryFindAndInsert_ManyReaders_ManyWriters)
{
    for (size_t ii = 0; ii < PARALLEL_ITERATIONS; ++ii)
    {
        parallelTryFindAndInsert(gtsMax(1u, Thread::getHardwareThreadCount() / 2), gtsMax(1u, Thread::getHardwareThreadCount() / 2), ITEM_COUNT);
    }
}

#if COMPARE_CONC_UNORDERED_MAP
//------------------------------------------------------------------------------
void parallelUnorderedMapTryFindAndInsert(size_t readerCount, size_t writerCount, size_t itemCount)
{
    concurrency::concurrent_unordered_map<size_t, DummyObject> ht;
    for (size_t ii = 0; ii < itemCount; ++ii)
    {
        ht.insert({ii, DummyObject(ii)});
    }

    gts::Atomic<bool> startProduction(false);

    // concurrently reading threads
    std::vector<std::thread*> readers;
    readers.resize(readerCount);
    for (size_t tt = 0; tt < readerCount; ++tt)
    {
        readers[tt] = new std::thread([tt, itemCount, &ht, &startProduction]()
            {
                uint32_t randState = (uint32_t)(tt + 1);

                while (!startProduction.load())
                {
                    GTS_PAUSE();
                }

                for (size_t ii = 0; ii < itemCount; ++ii)
                {
                    size_t idx = fastRand(randState) % itemCount;
                    DummyObject out;
                    auto iter = ht.cfind(idx);
                    ASSERT_TRUE(iter != ht.end());
                    ASSERT_EQ(iter->second, idx);
                }
            });
    }

    const size_t numWriterThreads = writerCount - 1;

    // concurrently writing threads
    std::vector<std::thread*> writers;
    writers.resize(numWriterThreads);
    for (size_t tt = 0; tt < numWriterThreads; ++tt)
    {
        writers[tt] = new std::thread([tt, itemCount, &ht, &startProduction]()
            {
                while (!startProduction.load())
                {
                    GTS_PAUSE();
                }

                size_t val = (size_t)itemCount * (tt + 1);
                for (size_t ii = 0; ii < itemCount; ++ii)
                {
                    auto iter = ht.insert({val, DummyObject(val)});
                    ASSERT_TRUE(iter.second);
                    val++;
                }
            });
    }

    startProduction.store(true);

    // Write on main thread too so we don't oversubscribe.
    size_t val = (size_t)(itemCount * (numWriterThreads + 1));
    for (size_t ii = 0; ii < itemCount; ++ii)
    {
        auto iter = ht.insert({val, DummyObject(val)});
        ASSERT_TRUE(iter.second);
        val++;
    }

    for (size_t tt = 0; tt < readerCount; ++tt)
    {
        readers[tt]->join();
        delete readers[tt];
    }
    for (size_t tt = 0; tt < numWriterThreads; ++tt)
    {
        writers[tt]->join();
        delete writers[tt];
    }

    // Make sure all the items exist.
    for (size_t ii = 0; ii < itemCount * (writerCount + 1); ++ii)
    {
        DummyObject out;
        auto iter = ht.cfind(ii);
        ASSERT_TRUE(iter != ht.end());
        ASSERT_EQ(iter->second, ii);
    }
}

//------------------------------------------------------------------------------
TEST(ParallelHashTable, parallelUnorderedMapTryFindAndInsert_ManyReaders_ManyWriters)
{
    for (size_t ii = 0; ii < PARALLEL_ITERATIONS; ++ii)
    {
        parallelUnorderedMapTryFindAndInsert(gtsMax(1u, Thread::getHardwareThreadCount() / 2), gtsMax(1u, Thread::getHardwareThreadCount() / 2), ITEM_COUNT);
    }
}

#endif

//------------------------------------------------------------------------------
void parallelFindInsertAndErase(size_t readerCount, size_t writerCount, size_t itemCount)
{
    ParallelHashTable<size_t, DummyObject> ht;
    for (size_t ii = 0; ii < itemCount; ++ii)
    {
        ht.insert({ii, DummyObject(ii)});
    }

    gts::Atomic<bool> startProduction(false);

    // concurrently reading threads
    std::vector<std::thread*> readers;
    readers.resize(readerCount);
    for (size_t tt = 0; tt < readerCount; ++tt)
    {
        readers[tt] = new std::thread([tt, itemCount, &ht, &startProduction]()
        {
            uint32_t randState = (uint32_t)(tt + 1);

            while (!startProduction.load(memory_order::acquire))
            {
                GTS_PAUSE();
            }

            for (size_t ii = 0; ii < itemCount; ++ii)
            {
                size_t idx = fastRand(randState) % itemCount;
                ParallelHashTable<size_t, DummyObject>::value_type out;
                auto iter = ht.cfind(idx);
                ASSERT_TRUE(iter != ht.end());
                ASSERT_EQ(iter->value, idx);
            }
        });
    }

    const size_t numWriterThreads = writerCount - 1;

    // concurrently writing threads
    std::vector<std::thread*> writers;
    writers.resize(numWriterThreads);
    for (size_t tt = 0; tt < numWriterThreads; ++tt)
    {
        writers[tt] = new std::thread([tt, itemCount, &ht, &startProduction]()
        {
            while (!startProduction.load(memory_order::acquire))
            {
                GTS_PAUSE();
            }

            size_t val = (size_t)itemCount * (tt + 1);
            for (size_t ii = 0; ii < itemCount; ++ii)
            {
                auto iter = ht.insert({val, DummyObject(val)});
                ASSERT_TRUE(iter != ht.end());
                ht.erase(iter);
                val++;
            }
        });
    }

    startProduction.store(true, memory_order::release);

    // Write on main thread too so we don't oversubscribe.
    size_t val = (size_t)(itemCount * (numWriterThreads + 1));
    for (size_t ii = 0; ii < itemCount; ++ii)
    {
        auto iter = ht.insert({val, DummyObject(val)});
        ASSERT_TRUE(iter != ht.end());
        ht.erase(iter);
        val++;
    }

    for (size_t tt = 0; tt < readerCount; ++tt)
    {
        readers[tt]->join();
        delete readers[tt];
    }
    for (size_t tt = 0; tt < numWriterThreads; ++tt)
    {
        writers[tt]->join();
        delete writers[tt];
    }

    // Make sure only the initial items exist.
    for (size_t ii = 0; ii < itemCount; ++ii)
    {
        ParallelHashTable<size_t, DummyObject>::value_type out;
        auto iter = ht.cfind(ii);
        ASSERT_TRUE(iter != ht.end());
        ASSERT_EQ(iter->value, ii);
    }
}

//------------------------------------------------------------------------------
TEST(ParallelHashTable, parallelFindInsertAndErase_1Reader_1Writer)
{
    for (size_t ii = 0; ii < PARALLEL_ITERATIONS; ++ii)
    {
        parallelFindInsertAndErase(1, 1, ITEM_COUNT);
    }
}

//------------------------------------------------------------------------------
TEST(ParallelHashTable, parallelFindInsertAndErase_ManyReaders_1Writer)
{
    for (size_t ii = 0; ii < PARALLEL_ITERATIONS; ++ii)
    {
        parallelFindInsertAndErase(gtsMax(1u, Thread::getHardwareThreadCount() - 1), 1, ITEM_COUNT);
    }
}

//------------------------------------------------------------------------------
TEST(ParallelHashTable, parallelFindInsertAndErase_1Reader_ManyWriters)
{
    for (size_t ii = 0; ii < PARALLEL_ITERATIONS; ++ii)
    {
        parallelFindInsertAndErase(1, gtsMax(1u, Thread::getHardwareThreadCount() - 1), ITEM_COUNT);
    }
}

//------------------------------------------------------------------------------
TEST(ParallelHashTable, parallelFindInsertAndErase_ManyReaders_ManyWriters)
{
    for (size_t ii = 0; ii < PARALLEL_ITERATIONS; ++ii)
    {
        parallelFindInsertAndErase(gtsMax(1u, Thread::getHardwareThreadCount() / 2), gtsMax(1u, Thread::getHardwareThreadCount() / 2), ITEM_COUNT);
    }
}

} // namespace testing
