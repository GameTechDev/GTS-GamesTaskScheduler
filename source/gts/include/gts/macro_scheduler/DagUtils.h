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

#include "gts/containers/Vector.h"

namespace gts {

class Node;
class MacroScheduler;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
struct DagUtils
{
    enum NodePropertyFlags : uint32_t
    {
        NAME   = 0x01,
        COST   = 0x02,
        UPRANK = 0x04,
        ALL    = uint32_t(-1)
    };

    static void printToDot(const char* filename, Node* pDagRoot, NodePropertyFlags propertiesToPrint = NAME);

    static void totalWork(uint64_t& outTotalWork, size_t& outNodeCountNode, Node* pDagRoot);

    static void getCriticalPath(Vector<Node*>& out, Vector<Node*>& dag);

    static void workOnCriticalPath(uint64_t& outWork, Vector<Node*> const& criticalPath);

    static bool isAcyclic(Node* pDagRoot);

    static void generateRandomDag(
        MacroScheduler* pMacroScheduler,
        uint32_t randSeed,
        uint32_t rank,
        uint32_t minNodesPerRank,
        uint32_t maxNodesPerRank,
        uint32_t precentChanceOfEdge,
        Vector<Node*>& dag);

    static bool isATopologicalOrdering(
        Vector<Node*>& dag,
        Vector<uint32_t> const& executionOrderToCheck);

private:

    static void _upRank(Node* pNode, Vector<Node*>& dag);
};

} // namespace gts
