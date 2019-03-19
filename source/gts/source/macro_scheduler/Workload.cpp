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
#include "gts/macro_scheduler/Workload.h"

namespace gts {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// IWorkload:

// STRUCTORS:

//------------------------------------------------------------------------------
    Workload::Workload(ComputeResourceType type)
    : m_executionCost(0)
    , m_transferCost(0)
    , m_type(type)
{
}

// ACCESSORS:

//------------------------------------------------------------------------------
uint64_t Workload::executionCost() const
{
    return m_executionCost;
}

//------------------------------------------------------------------------------
uint64_t Workload::transferCost() const
{
    return m_transferCost;
}

//------------------------------------------------------------------------------
ComputeResourceType Workload::type() const
{
    return m_type;
}

// MUTATORS:

//------------------------------------------------------------------------------
void Workload::setExecutionCost(const uint64_t& cost)
{
    m_executionCost = cost;
}

//------------------------------------------------------------------------------
void Workload::setTransferCost(const uint64_t& cost)
{
    m_transferCost = cost;
}

} // namespace gts
