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
#include <chrono>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "gts/containers/Vector.h"

using namespace gts;


namespace {

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
TEST(Vector, defaultConstructor)
{
    DummyObject::reset();

    Vector<DummyObject> v;
    ASSERT_EQ(v.capacity(), size_t(0));
    ASSERT_EQ(v.size(), size_t(0));
}

//------------------------------------------------------------------------------
TEST(Vector, resizeConstructor)
{
    const size_t size = 32;
    Vector<DummyObject> v(size);
    ASSERT_EQ(v.capacity(), size);
    ASSERT_EQ(v.size(), size);
}

//------------------------------------------------------------------------------
TEST(Vector, fillConstructor)
{
    DummyObject::reset();

    const size_t size = 32;
    const DummyObject fill(2);

    Vector<DummyObject> v(size, fill);
    ASSERT_EQ(v.capacity(), size);
    ASSERT_EQ(v.size(), size);

    for (uint32_t ii = 0; ii < v.size(); ++ii)
    {
        ASSERT_EQ(v[ii], fill);
    }
}

//------------------------------------------------------------------------------
TEST(Vector, copyConstructor)
{
    DummyObject::reset();

    const size_t size = 32;
    const DummyObject fill(2);

    Vector<DummyObject> v(size, fill);
    Vector<DummyObject> vCopy(v);

    // Verify v still has the correct values.
    ASSERT_EQ(v.capacity(), size);
    ASSERT_EQ(v.size(), size);
    for (uint32_t ii = 0; ii < v.size(); ++ii)
    {
        ASSERT_EQ(v[ii], fill);
    }

    // Verify vCopy has the correct values.
    ASSERT_EQ(vCopy.capacity(), size);
    ASSERT_EQ(vCopy.size(), size);
    for (uint32_t ii = 0; ii < v.size(); ++ii)
    {
        ASSERT_EQ(vCopy[ii], fill);
    }

    // Verify underlying data is unique.
    ASSERT_NE(vCopy.data(), v.data());
}

//------------------------------------------------------------------------------
TEST(Vector, moveConstructor)
{
    DummyObject::reset();

    const size_t size = 32;
    const DummyObject fill(2);

    Vector<DummyObject> v(size, fill);
    Vector<DummyObject> vCopy(std::move(v));

    // Verify v has been moved
    ASSERT_EQ(v.capacity(), size_t(0));
    ASSERT_EQ(v.size(), size_t(0));
    ASSERT_EQ(v.data(), nullptr);

    // Verify vCopy has the correct values.
    ASSERT_EQ(vCopy.capacity(), size);
    ASSERT_EQ(vCopy.size(), size);
    for (uint32_t ii = 0; ii < v.size(); ++ii)
    {
        ASSERT_EQ(vCopy[ii], fill);
    }
}

//------------------------------------------------------------------------------
TEST(Vector, copyAssignment)
{
    DummyObject::reset();

    const size_t size = 32;
    const DummyObject fill(2);

    Vector<DummyObject> v(size, fill);
    Vector<DummyObject> vCopy(size, fill);
    vCopy = v;

    // Verify v still has the correct values.
    ASSERT_EQ(v.capacity(), size);
    ASSERT_EQ(v.size(), size);
    for (uint32_t ii = 0; ii < v.size(); ++ii)
    {
        ASSERT_EQ(v[ii], fill);
    }

    // Verify vCopy has the correct values.
    ASSERT_EQ(vCopy.capacity(), size);
    ASSERT_EQ(vCopy.size(), size);
    for (uint32_t ii = 0; ii < v.size(); ++ii)
    {
        ASSERT_EQ(vCopy[ii], fill);
    }

    // Verify underlying data is unique.
    ASSERT_NE(vCopy.data(), v.data());
}

//------------------------------------------------------------------------------
TEST(Vector, moveAssignment)
{
    DummyObject::reset();

    const size_t size = 32;
    const DummyObject fill(2);

    Vector<DummyObject> v(size, fill);
    Vector<DummyObject> vCopy(size, fill);
    vCopy = std::move(v);

    // Verify v has been moved
    ASSERT_EQ(v.capacity(), size_t(0));
    ASSERT_EQ(v.size(), size_t(0));
    ASSERT_EQ(v.data(), nullptr);

    // Verify vCopy has the correct values.
    ASSERT_EQ(vCopy.capacity(), size);
    ASSERT_EQ(vCopy.size(), size);
    for (uint32_t ii = 0; ii < v.size(); ++ii)
    {
        ASSERT_EQ(vCopy[ii], fill);
    }
}

//------------------------------------------------------------------------------
TEST(Vector, push_back_copy)
{
    size_t nextValue = 0;
    const size_t size = 32;

    DummyObject::reset();
    {
        Vector<DummyObject> v;
        const DummyObject fill(nextValue++);
        v.push_back(fill);

        for (size_t ii = 1; ii < size; ii *= 2)
        {
            for (size_t jj = ii; jj < ii * 2; ++jj)
            {
                const DummyObject localFill(nextValue++);
                v.push_back(localFill);

                // Verify size.
                ASSERT_EQ(v.size(), jj + 1);

                // Verify capacity doubling.
                ASSERT_EQ(v.capacity(), ii * 2);
            }
        }

        // Verify elements
        for (size_t ii = 0; ii < size; ++ii)
        {
            ASSERT_EQ(v[ii], ii);
        }
    }
    ASSERT_EQ(DummyObject::s_constructorCount, size * 2);
    ASSERT_EQ(DummyObject::s_destructorCount, size * 2);
}

//------------------------------------------------------------------------------
TEST(Vector, push_back_move)
{
    size_t nextValue = 0;
    const size_t size = 32;

    DummyObject::reset();
    {
        Vector<DummyObject> v;
        v.push_back(DummyObject(nextValue++));

        for (size_t ii = 1; ii < size; ii *= 2)
        {
            for (size_t jj = ii; jj < ii * 2; ++jj)
            {
                v.push_back(DummyObject(nextValue++));

                // Verify size.
                ASSERT_EQ(v.size(), jj + 1);

                // Verify capacity doubling.
                ASSERT_EQ(v.capacity(), ii * 2);
            }
        }

        // Verify elements
        for (size_t ii = 0; ii < size; ++ii)
        {
            ASSERT_EQ(v[ii], ii);
        }
    }
    ASSERT_EQ(DummyObject::s_constructorCount, size);
    ASSERT_EQ(DummyObject::s_destructorCount, size * 2);
}

//------------------------------------------------------------------------------
TEST(Vector, backAndPopBack)
{
    const size_t size = 32;

    DummyObject::reset();
    {
        Vector<DummyObject> v;

        for (size_t ii = 0; ii < size; ++ii)
        {
            v.push_back(DummyObject(ii));
        }

        for (size_t ii = 0; ii < size; ++ii)
        {
            ASSERT_EQ(v.back(), size - 1 - ii);
            v.pop_back();
        }

        ASSERT_EQ(v.empty(), true);
    }
    ASSERT_EQ(DummyObject::s_constructorCount, size);
    ASSERT_EQ(DummyObject::s_destructorCount, size * 2);
}

//------------------------------------------------------------------------------
TEST(Vector, resizeFromEmpty)
{
    const size_t size = 32;

    DummyObject::reset();
    {
        Vector<DummyObject> v;
        v.resize(size);

        ASSERT_EQ(v.capacity(), size);
        ASSERT_EQ(v.size(), size);

        for (uint32_t ii = 0; ii < v.size(); ++ii)
        {
            ASSERT_EQ(v[ii], 0);
        }
    }
    ASSERT_EQ(DummyObject::s_constructorCount, size);
    ASSERT_EQ(DummyObject::s_destructorCount, size);
}

//------------------------------------------------------------------------------
TEST(Vector, resizeFromEmptyWithFill)
{
    const size_t size = 32;
    const DummyObject fill(2);

    DummyObject::reset();
    {
        Vector<DummyObject> v;
        v.resize(size, fill);

        ASSERT_EQ(v.capacity(), size);
        ASSERT_EQ(v.size(), size);

        for (uint32_t ii = 0; ii < v.size(); ++ii)
        {
            ASSERT_EQ(v[ii], fill);
        }
    }
    ASSERT_EQ(DummyObject::s_constructorCount, size);
    ASSERT_EQ(DummyObject::s_destructorCount, size);
}

//------------------------------------------------------------------------------
TEST(Vector, resize)
{
    const size_t size0 = 3;
    const DummyObject fill0(2);

    const size_t size1 = 28;
    const DummyObject fill1(3);

    DummyObject::reset();
    {
        Vector<DummyObject> v(size0, fill0);
        v.resize(size1, fill1);

        ASSERT_EQ(v.capacity(), size1);
        ASSERT_EQ(v.size(), size1);

        for (uint32_t ii = 0; ii < size0; ++ii)
        {
            ASSERT_EQ(v[ii], fill0);
        }

        for (uint32_t ii = size0; ii < size1; ++ii)
        {
            ASSERT_EQ(v[ii], fill1);
        }
    }
    ASSERT_EQ(DummyObject::s_constructorCount, size1);
    ASSERT_EQ(DummyObject::s_destructorCount, size1);
}

//------------------------------------------------------------------------------
TEST(Vector, resizeShrink)
{
    const size_t size0 = 28;
    const DummyObject fill0(2);

    const size_t size1 = 3;

    DummyObject::reset();
    {
        Vector<DummyObject> v(size0, fill0);
        v.resize(size1);

        ASSERT_EQ(v.capacity(), size1);
        ASSERT_EQ(v.size(), size1);

        for (uint32_t ii = 0; ii < size1; ++ii)
        {
            ASSERT_EQ(v[ii], fill0);
        }

        ASSERT_EQ(DummyObject::s_constructorCount, size0);
        ASSERT_EQ(DummyObject::s_destructorCount, size0 - size1);
    }
    ASSERT_EQ(DummyObject::s_destructorCount,size0);
}

//------------------------------------------------------------------------------
TEST(Vector, reserve)
{
    const size_t size = 32;

    DummyObject::reset();
    {
        Vector<DummyObject> v;
        v.reserve(size);

        ASSERT_EQ(v.capacity(), size);
        ASSERT_EQ(v.size(), size_t(0));
    }
    ASSERT_EQ(DummyObject::s_constructorCount, size_t(0));
    ASSERT_EQ(DummyObject::s_destructorCount, size_t(0));
}

//------------------------------------------------------------------------------
TEST(Vector, shrink_to_fit)
{
    const size_t capacity = 32;
    const size_t size = 5;

    DummyObject::reset();
    {
        Vector<DummyObject> v;
        v.reserve(capacity);

        for (uint32_t ii = 0; ii < size; ++ii)
        {
            v.push_back(DummyObject(ii));
        }

        v.shrink_to_fit();
        ASSERT_EQ(v.capacity(), size);
        ASSERT_EQ(v.size(), size);

        for (uint32_t ii = 0; ii < size; ++ii)
        {
            ASSERT_EQ(v[ii], ii);
        }
    }
    ASSERT_EQ(DummyObject::s_constructorCount, size);
    ASSERT_EQ(DummyObject::s_destructorCount, size * 2);
}

} // namespace testing
