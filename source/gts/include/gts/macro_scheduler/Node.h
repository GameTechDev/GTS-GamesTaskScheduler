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

#include "gts/platform/Atomic.h"
#include "gts/containers/Vector.h"
#include "gts/macro_scheduler/ComputeResourceType.h"

namespace gts {

class Workload;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
 * @brief
 *  A Node represents a coarse grained task in a generalized task DAG. Nodes
 *  are scheduled by a MacroScheduler.
 */
class Node
{
public: // STRUCTORS:

    Node();
    ~Node();

public: // ACCESSORS

    Workload* findWorkload(ComputeResourceType type) const;
    uint32_t priority() const;
    Atomic<uint32_t> const& currPredecessorCount() const;
    uint32_t const& initPredecessorCount() const;
    Vector<Node*> const& children() const;
    bool isReady() const;

public: // MUTATORS

    template<typename TWorkload, typename... TArgs>
    GTS_INLINE TWorkload* setWorkload(TArgs... args)
    {
        if (m_workloadsByType[(size_t)TWorkload::type()] != nullptr)
        {
            delete m_workloadsByType[(size_t)TWorkload::type()];
        }

        TWorkload* pWorkload = new TWorkload(std::forward<TArgs>(args)...);
        m_workloadsByType[(size_t)TWorkload::type()] = pWorkload;
        return pWorkload;
    }

    void setPriority(uint32_t priority);
    Atomic<uint32_t>& currPredecessorCount();

    /**
     * @remark Not threadsafe.
     */
    void addChild(Node* pNode);

    /**
     * @remark Not threadsafe.
     */
    void removeChild(Node* pNode);

    bool finishPredecessor();

private:

    Vector<Node*> m_children;
    Workload* m_workloadsByType[(uint32_t)ComputeResourceType::COUNT];
    uint32_t m_priority;
    Atomic<uint32_t> m_currPredecessorCount;
    uint32_t m_initPredecessorCount;

    // TODO:
    // uint32_t m_computeUnitRequestId;
};

} // namespace gts
