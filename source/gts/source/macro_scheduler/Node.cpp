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

#include <stdarg.h>
#include <queue>
#include <unordered_set>

#include "gts/macro_scheduler/Workload.h"
#include "gts/macro_scheduler/ComputeResource.h"

namespace gts {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Node

// STRUCTORS:

//------------------------------------------------------------------------------
Node::Node(MacroScheduler* pMyScheduler)
    : m_pMyScheduler(pMyScheduler)
    , m_pSchedule(nullptr)
    , m_rank(0)
    , m_executionCost(1)
    , m_predecessorCompleteCount(0)
    , m_currPredecessorCount(0)
    , m_initPredecessorCount(0)
    , m_affinity(ANY_COMP_RESOURCE)
    , m_name{0}
    //, m_isIsolated(false)
{
    for (size_t ii = 0; ii < WorkloadType::COUNT; ++ii)
    {
        m_workloadsByType[ii] = nullptr;
    }
}

//------------------------------------------------------------------------------
Node::~Node()
{
    for (size_t ii = 0; ii < WorkloadType::COUNT; ++ii)
    {
        m_pMyScheduler->_freeWorkload(m_workloadsByType[ii]);
    }
}

// ACCESSORS:

//------------------------------------------------------------------------------
Workload* Node::findWorkload(WorkloadType::Enum type) const
{
    return m_workloadsByType[type];
}

//------------------------------------------------------------------------------
Atomic<uint32_t> const& Node::currPredecessorCount() const
{
    return m_currPredecessorCount;
}

//------------------------------------------------------------------------------
uint32_t Node::initPredecessorCount() const
{
    return m_initPredecessorCount;
}

//------------------------------------------------------------------------------
bool Node::isChild(Node* pChild) const
{
    for (size_t ii = 0; ii < m_successors.size(); ++ii)
    {
        if (pChild == m_successors[ii])
        {
            return true;
        }
    }
    return false;
}

// MUTATORS:

//------------------------------------------------------------------------------
void Node::resetGraph(Node* pSource)
{
    std::unordered_set<Node*> visited;

    std::queue<Node*> q;
    q.push(pSource);

    while(!q.empty())
    {
        Node* pCurr = q.front(); q.pop();
        visited.insert(pCurr);
        pCurr->reset();

        for(Node* pSucc : pCurr->successors())
        {
            if(visited.find(pSucc) == visited.end())
            {
                q.push(pSucc);
            }
        }
    }
}

//------------------------------------------------------------------------------
void Node::removeWorkload(WorkloadType::Enum type)
{
    m_pMyScheduler->_freeWorkload(m_workloadsByType[type]);
    m_workloadsByType[type] = nullptr;
}

//------------------------------------------------------------------------------
void Node::reset()
{
    m_predecessorCompleteCount.store(m_initPredecessorCount, memory_order::relaxed);
    m_currPredecessorCount.store(m_initPredecessorCount, memory_order::relaxed);
}

//------------------------------------------------------------------------------
void Node::addSuccessor(Node* pNode)
{
    m_successors.push_back(pNode);
    pNode->m_predecessors.push_back(this);
    ++pNode->m_initPredecessorCount;
    pNode->m_currPredecessorCount.fetch_add(1, memory_order::acq_rel);
}

//------------------------------------------------------------------------------
void Node::removeSuccessor(Node* pNode)
{
    for (size_t ii = 0; ii < m_successors.size(); ++ii)
    {
        if (pNode == m_successors[ii])
        {
            // Remove pNode from this Node's children.
            std::swap(m_successors[ii], m_successors.back());
            m_successors.pop_back();

            // Remove this from pNode's predecessor list.
            for (size_t jj = 0; jj < pNode->m_predecessors.size(); ++jj)
            {
                if (this == pNode->m_predecessors[jj])
                {
                    std::swap(pNode->m_predecessors[jj], pNode->m_predecessors.back());
                    pNode->m_predecessors.pop_back();
                }
            }

            // Remove pNode from this Node's children.
            --pNode->m_initPredecessorCount;
            pNode->m_currPredecessorCount.fetch_sub(1, memory_order::release);
        }
    }
}

//------------------------------------------------------------------------------
void Node::setName(const char* format, ...)
{
    GTS_ASSERT(format);
    va_list args;
    va_start(args, format);
    #if GTS_MSVC
        vsnprintf_s(m_name, NODE_NAME_MAX, format, args);
    #elif GTS_LINUX
        vsnprintf(m_name, NODE_NAME_MAX, format, args);
    #endif
    va_end(args);
}

} // namespace gts
