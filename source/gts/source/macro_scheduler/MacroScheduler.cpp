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
#include "gts/macro_scheduler/MacroScheduler.h"
#include "gts/macro_scheduler/Node.h"
#include "gts/macro_scheduler/Schedule.h"
#include "gts/macro_scheduler/ComputeResource.h"

namespace gts {

//--------------------------------------------------------------------------
MacroScheduler::MacroScheduler()
{}

//--------------------------------------------------------------------------
MacroScheduler::~MacroScheduler()
{}

// ACCESSORS:

//------------------------------------------------------------------------------
ComputeResource* MacroScheduler::findComputeResource(ComputeResourceId id)
{
    for (size_t ii = 0; ii < m_computeResources.size(); ++ii)
    {
        if (m_computeResources[ii]->id() == id)
        {
            return m_computeResources[ii];
        }
    }
    return nullptr;
}

//--------------------------------------------------------------------------
Vector<ComputeResource*> const& MacroScheduler::computeResources() const
{
    return m_computeResources;
}

// MUTATORS:

//--------------------------------------------------------------------------
Vector<ComputeResource*>& MacroScheduler::computeResources()
{
    return m_computeResources;
}

//--------------------------------------------------------------------------
Node* MacroScheduler::allocateNode()
{
    // TODO: parallel allocator if needed.
    return alignedNew<Node, GTS_NO_SHARING_CACHE_LINE_SIZE>(this);
}

//--------------------------------------------------------------------------
void MacroScheduler::destroyNode(Node* pNode)
{
    // TODO: parallel allocator if needed.
    alignedDelete(pNode);
}

//--------------------------------------------------------------------------
void* MacroScheduler::_allocateWorkload(size_t size)
{
    // TODO: parallel allocator if needed.
    return GTS_ALIGNED_MALLOC(size, GTS_NO_SHARING_CACHE_LINE_SIZE);
}

//--------------------------------------------------------------------------
void MacroScheduler::_freeWorkload(void* ptr)
{
    // TODO: parallel allocator if needed.
    GTS_ALIGNED_FREE(ptr);
}

} // namespace gts
