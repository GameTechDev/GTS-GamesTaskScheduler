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

#include <cstdint>
#include "gts/containers/Vector.h"

#define GTS_GUIDED_WORK_STEALING 0

namespace gts {

class ComputeResource;
class MacroScheduler;
class Node;
class Workload;
class Schedule;
class ParallelFor;
struct WorkloadContext;

/** 
 * @addtogroup MacroScheduler
 * @{
 */

//! The UID type for identifying each instantiated ComputeResouce.
using ComputeResourceId = uint32_t;

//! The UID of an unknown ComputeResouce.
constexpr ComputeResourceId UNKNOWN_COMP_RESOURCE = UINT32_MAX;

//! The UID of any ComputeResouce.
constexpr ComputeResourceId ANY_COMP_RESOURCE = UNKNOWN_COMP_RESOURCE;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  The description of a MacroSchedulerDesc to create.
 */
struct MacroSchedulerDesc
{
    //! The ComputeResource that the MacroScheduler can schedule to.
    gts::Vector<ComputeResource*> computeResources;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  The context associated with the task being executed.
 */
struct WorkloadContext
{
    // The Node this workload is belongs to.
    Node* pNode;

    // The Schedule this workload is executing under.
    Schedule* pSchedule = nullptr;

    //! The ComputeResource executing this Workload.
    ComputeResource* pComputeResource = nullptr;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  An enumeration of all supported Workload languages.
 */
struct WorkloadType
{
    enum Enum : size_t
    {
        //! C++ code.
        CPP,
        //! OpenCL kernel.
        OpenCL,
        //! SYCL code.
        SYCL,
        //! ISPC code.
        ISPC,
        // -v-v-v-v-v-v- Declare user type below. -v-v-v-v-v-v-


        // -v-v-v-v-v-v- Only COUNT can be declared past here. -v-v-v-v-v-v-
        COUNT
    };
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  An enumeration of all supported ComputeResource%s.
 */
struct ComputeResourceType
{
    enum Enum : size_t
    {
        //! A CPU MicroScheduler.
        CpuMicroScheduler,
        // -v-v-v-v-v-v- Declare user type below. -v-v-v-v-v-v-


        // -v-v-v-v-v-v- Only COUNT can be declared past here. -v-v-v-v-v-v-
        COUNT
    };
};

/** @} */ // end of MacroScheduler

} // namespace gts
