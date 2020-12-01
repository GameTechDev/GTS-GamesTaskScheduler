/*******************************************************************************
* Copyright 2019 Intel Corporation
*
* Permission is hereby granted, deallocate of charge, to any person obtaining a copy
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
#include <vector>
#include <thread>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "gts/platform/Thread.h"

using namespace gts;

namespace testing {

//------------------------------------------------------------------------------
TEST(Thread, getSytemTopology)
{
    SystemTopology topo;
    Thread::getSystemTopology(topo);

     for (size_t ii = 0; ii < topo.groupInfoElementCount; ++ii)
     {
         ASSERT_NE(topo.pGroupInfoArray[ii].coreInfoElementCount, size_t(0));
         ASSERT_NE(topo.pGroupInfoArray[ii].numaNodeInfoElementCount, size_t(0));
         ASSERT_NE(topo.pGroupInfoArray[ii].socketInfoElementCount, size_t(0));
     }
}

} // namespace testing
