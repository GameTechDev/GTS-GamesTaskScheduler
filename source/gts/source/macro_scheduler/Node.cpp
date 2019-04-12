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
#include "gts/macro_scheduler/Node.h"

#include "gts/macro_scheduler/Workload.h"

namespace gts {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Node

// STRUCTORS:

//------------------------------------------------------------------------------
Node::Node(MacroScheduler* pMyScheduler)
    : m_pMyScheduler(pMyScheduler)
    , m_priority(0)
    , m_currPredecessorCount(0)
    , m_initPredecessorCount(0)
{
    for (size_t ii = 0; ii < (size_t)ComputeResourceType::COUNT; ++ii)
    {
        m_workloadsByType[ii] = nullptr;
    }
}

//------------------------------------------------------------------------------
Node::~Node()
{
    for (size_t ii = 0; ii < (size_t)ComputeResourceType::COUNT; ++ii)
    {
        m_pMyScheduler->_freeWorkload(m_workloadsByType[ii]);
    }
}

// ACCESSORS:

//------------------------------------------------------------------------------
Workload* Node::findWorkload(ComputeResourceType type) const
{
    return m_workloadsByType[(size_t)type];
}

//------------------------------------------------------------------------------
uint32_t Node::priority() const
{
    return m_priority;
}

//------------------------------------------------------------------------------
Atomic<uint32_t> const& Node::currPredecessorCount() const
{
    Lock<UnfairSpinMutex> lock(m_accessorMutex);
    return m_currPredecessorCount;
}

//------------------------------------------------------------------------------
uint32_t const& Node::initPredecessorCount() const
{
    Lock<UnfairSpinMutex> lock(m_accessorMutex);
    return m_initPredecessorCount;
}

//------------------------------------------------------------------------------
Vector<Node*> const Node::children() const
{
    Lock<UnfairSpinMutex> lock(m_accessorMutex);
    return m_children; // return a copy.
}

// MUTATORS:

//------------------------------------------------------------------------------
void Node::addChild(Node* pNode)
{
    Lock<UnfairSpinMutex> lock(m_accessorMutex);
    m_children.push_back(pNode);
    ++pNode->m_initPredecessorCount;
    pNode->m_currPredecessorCount.fetch_add(1, gts::memory_order::relaxed);
}

//------------------------------------------------------------------------------
void Node::removeChild(Node* pNode)
{
    Lock<UnfairSpinMutex> lock(m_accessorMutex);
    for (size_t ii = 0, len = m_children.size(); ii < len; ++ii)
    {
        if (pNode == m_children[ii])
        {
            std::swap(m_children[ii], m_children.back());
            m_children.pop_back();
            --pNode->m_initPredecessorCount;
            pNode->m_currPredecessorCount.fetch_sub(1, gts::memory_order::relaxed);
        }
    }
}

//------------------------------------------------------------------------------
void Node::setPriority(uint32_t priority)
{
    m_priority = priority;
}

//------------------------------------------------------------------------------
void Node::reset()
{
    Lock<UnfairSpinMutex> lock(m_accessorMutex);
    m_currPredecessorCount.store(m_initPredecessorCount, gts::memory_order::relaxed);
}

//------------------------------------------------------------------------------
bool Node::finishPredecessor()
{
    Lock<UnfairSpinMutex> lock(m_accessorMutex);
    return(
        // No Race Case: T0 observes refcount > 1 and completes removeRef(1) before T1
        // observes refcount. In this case T1 cannot race and it will see
        // refcount == 1, short circuiting the atomic.
        m_currPredecessorCount.load(gts::memory_order::acquire) == 1 ||
        // Race Case: T0 and T1 can both observe refcount > 1, so they must use
        // removeRef(1) to not race. Since both threads took this path, the thread
        // that sees refcount == 0 is the winner.
        m_currPredecessorCount.fetch_sub(1) == 0);
}

} // namespace gts
