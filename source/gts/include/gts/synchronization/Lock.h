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

#include "gts/platform/Assert.h"
#include "gts/platform/Machine.h"
#include "gts/platform/Atomic.h"
#include "gts/analysis/Trace.h"

namespace gts {

/**
 * @addtogroup Synchronization
 * @{
 */

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A scoped lock for a shared mutex concept.
 */
template<typename TMutex>
struct LockShared
{
    LockShared(TMutex& m) : m_mutex(m) { m_mutex.lock_shared(); }
    ~LockShared() { m_mutex.unlock_shared(); }
    TMutex& mutex() { return m_mutex; }

private:

    TMutex& m_mutex;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A scoped lock for a mutex concept.
 */
template<typename TMutex>
struct Lock
{
    Lock(TMutex& m) : m_mutex(m) { m_mutex.lock(); }
    ~Lock() { m_mutex.unlock(); }
    TMutex& mutex() { m_mutex; }

private:

    TMutex& m_mutex;
};

/** @} */ // end of Synchronization

} // namespace gts
