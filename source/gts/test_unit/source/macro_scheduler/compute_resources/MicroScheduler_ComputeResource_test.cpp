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
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <iostream>

#include "gts/micro_scheduler/WorkerPool.h"
#include "gts/micro_scheduler/MicroScheduler.h"

#include "gts/macro_scheduler/Node.h"

#include "DagUtils.h"

// Dynamic
#include "gts/macro_scheduler/schedulers/dynamic/Dynamic_MacroScheduler.h"
#include "gts/macro_scheduler/compute_resources/MicroScheduler_ComputeResource.h"
#include "gts/macro_scheduler/compute_resources/MicroScheduler_Workload.h"

using namespace gts;

namespace testing {

//------------------------------------------------------------------------------
TEST(MicroScheduler_ComputeResource, Constructor)
{
	WorkerPool workerPool;
	workerPool.initialize();

	MicroScheduler microScheduler;
	microScheduler.initialize(&workerPool);

	MicroScheduler_ComputeResource computeResource(&microScheduler);
	ASSERT_EQ(computeResource.type(), ComputeResourceType::CPU_MicroScheduler);

	EXPECT_DEATH({
		MicroScheduler_ComputeResource computeResource_bad(nullptr);
	}, "");
}

} // namespace testing
