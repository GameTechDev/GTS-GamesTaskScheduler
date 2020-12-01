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

#include <functional>
#include <thread>

#include "gts/platform/Machine.h"
#include "gts/containers/Vector.h"
#include "gts/platform/Atomic.h"

using namespace gts;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
class SynchronizedTests
{
public:

    using TestFunc = std::function<void(uint16_t threadIdx)>;

    //--------------------------------------------------------------------------
    GTS_INLINE void generateTests(
        uint32_t const threadCount,
        TestFunc&& startFunc,
        TestFunc&& testFunc)
    {
        for (uint16_t localId = 0; localId < threadCount; ++localId)
        {
            std::thread* pThread = new std::thread([&, startFunc, testFunc, localId]()
            {
                while (!m_startInitialize.load(gts::memory_order::acquire))
                {
                    GTS_PAUSE();
                }

                m_initCount.fetch_add(1, memory_order::acq_rel);

                startFunc(localId);

                while (!m_startWork.load(gts::memory_order::acquire))
                {
                    GTS_PAUSE();
                }

                testFunc(localId);
            });

            m_threads.push_back(pThread);
        }
    }

    //--------------------------------------------------------------------------
    GTS_INLINE void initialize()
    {
        m_startInitialize.store(true, memory_order::relaxed);
    }

    //--------------------------------------------------------------------------
    GTS_INLINE uint32_t initCount() const
    {
        m_initCount.load(gts::memory_order::acquire);
    }

    //--------------------------------------------------------------------------
    GTS_INLINE void start()
    {
        m_startWork.store(true, memory_order::release);
    }

    //--------------------------------------------------------------------------
    GTS_INLINE void wait()
    {
        for (size_t ii = 0; ii < m_threads.size(); ++ii)
        {
            m_threads[ii]->join();
            delete m_threads[ii];
        }
    }

private:

    gts::Atomic<bool> m_startInitialize = false;
    gts::Atomic<uint32_t> m_initCount = 0;
    gts::Atomic<bool> m_startWork = false;
    gts::Vector<std::thread*> m_threads;
};
