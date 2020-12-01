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
#include "gts/macro_scheduler/DagUtils.h"

#include <fstream>
#include <sstream>
#include <queue>
#include <unordered_set>
#include <algorithm>

#include "gts/platform/Assert.h"
#include "gts/macro_scheduler/Node.h"

namespace gts {

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// DagUtils:

void DagUtils::printToDot(const char* filename, Node* pDagRoot, NodePropertyFlags propertiesToPrint)
{
    std::stringstream gvStructs;
    std::stringstream gvNodes;

    std::ofstream file(filename);
    file << "digraph structs {\n";

    std::unordered_set<Node*> visited;

    std::queue<Node*> q;
    q.push(pDagRoot);

    while (!q.empty())
    {
        Node* pCurr = q.front();
        q.pop();
        if (visited.find(pCurr) != visited.end())
        {
            continue;
        }
        visited.insert(pCurr);

        for (uint32_t ii = 0; ii < pCurr->successors().size(); ++ii)
        {
            Node* pChild = pCurr->successors()[ii];
            gvStructs << "n" << pCurr->name() << "->" << "n" << pChild->name() << ";\n";
            q.push(pChild);
        }
    }

    for (auto pNode : visited)
    {
        gvStructs << "n" << pNode->name() << "[label = <"
            "<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\">";

        if (propertiesToPrint & NAME)
        {
            gvStructs << "<TR><TD>name</TD><TD>" << pNode->name() << "</TD></TR>";
        }
        if (propertiesToPrint & COST)
        {
            gvStructs << "<TR><TD>cost</TD><TD>" << pNode->executionCost() << "</TD></TR>";
        }
        if (propertiesToPrint & UPRANK)
        {
            gvStructs << "<TR><TD>rank</TD><TD>" << pNode->rank().load(memory_order::relaxed) << "</TD></TR>";
        }

        gvStructs << "</TABLE>>];\n";
    }

    file << "node [shape=plaintext]\n";
    file << gvStructs.str();
    file << gvNodes.str();

    file << "}\n";
}

//------------------------------------------------------------------------------
void DagUtils::totalWork(uint64_t& outTotalWork, size_t& outNodeCount, Node* pDagRoot)
{
    outTotalWork = 0;
    outNodeCount = 0;

    std::unordered_set<Node*> visited;

    std::queue<Node*> q;
    q.push(pDagRoot);

    while (!q.empty())
    {
        Node* pCurr = q.front();
        q.pop();
        if (visited.find(pCurr) != visited.end())
        {
            continue;
        }
        visited.insert(pCurr);

        outTotalWork += pCurr->executionCost();
        outNodeCount++;

        for (uint32_t ii = 0; ii < pCurr->successors().size(); ++ii)
        {
            Node* pChild = pCurr->successors()[ii];
            q.push(pChild);
        }
    }
}

//-----------------------------------------------------------------------------
void DagUtils::_upRank(Node* pNode, Vector<Node*>& dag)
{
    for (size_t ii = 0; ii < pNode->predecessors().size(); ++ii)
    {
        Node* pPred = pNode->predecessors()[ii];
        if (pPred->rank().load(memory_order::relaxed) < pNode->rank().load(memory_order::relaxed) + 1)
        {
            pPred->rank().store(pNode->rank().load(memory_order::relaxed) + 1, memory_order::relaxed);
            _upRank(pPred, dag);
        }
    }
}

//-----------------------------------------------------------------------------
void DagUtils::getCriticalPath(Vector<Node*>& out, Vector<Node*>& dag)
{
    Vector<uint64_t> rankCopy(dag.size());
    for (size_t ii = 0; ii < dag.size(); ++ii)
    {
        rankCopy[ii] = dag[ii]->rank().load(memory_order::relaxed);
    }

    dag.back()->rank().store(1, memory_order::relaxed);
    _upRank(dag.back(), dag);

    uint64_t maxRank = 0; // set to zero so we add the first node in the DFT.
    Node* pMaxNode = dag.front();

    std::queue<Node*> q;
    q.push(dag.front());

    while (!q.empty())
    {
        Node* pCurr = q.front();
        q.pop();

        // Check if pCurr is a successor of pMaxNode.
        bool isSuccessor = false;
        for (size_t iNode = 0; iNode < pMaxNode->successors().size(); ++iNode)
        {
            if (pMaxNode->successors()[iNode] == pCurr)
            {
                isSuccessor = true;
                break;
            }
        }

        if (pCurr->rank().load(memory_order::relaxed) > maxRank ||
            (pCurr->rank().load(memory_order::relaxed) == maxRank - 1 && isSuccessor))
        {
            maxRank = pCurr->rank().load(memory_order::relaxed);
            pMaxNode = pCurr;
            out.push_back(pCurr);
        }

        for (size_t iNode = 0; iNode < pCurr->successors().size(); ++iNode)
        {
            Node* pSucc = pCurr->successors()[iNode];
            if (pSucc->_removePredecessorRefAndReturnReady())
            {
                q.push(pSucc);
            }
        }
    }

    // Reset numPredecessors and ranks.
    for (uint32_t ii = 0; ii < dag.size(); ++ii)
    {
        dag[ii]->rank().store(rankCopy[ii], memory_order::relaxed);
        dag[ii]->reset();
    }
}

//------------------------------------------------------------------------------
void DagUtils::workOnCriticalPath(uint64_t& outWork, Vector<Node*> const& criticalPath)
{
    for (Node const* pNode : criticalPath)
    {
        outWork += pNode->executionCost();
    }
}

//-----------------------------------------------------------------------------
bool DagUtils::isAcyclic(Node* pDagRoot)
{
    std::unordered_set<Node*> visited;

    std::queue<Node*> q;
    q.push(pDagRoot);

    while (!q.empty())
    {
        Node* pCurr = q.front();
        q.pop();

        pCurr->reset();

        if (visited.find(pCurr) != visited.end())
        {
            return false;
        }

        visited.insert(pCurr);

        for (size_t iNode = 0; iNode < pCurr->successors().size(); ++iNode)
        {
            Node* pSucc = pCurr->successors()[iNode];
            if (pSucc->_removePredecessorRefAndReturnReady())
            {
                q.push(pSucc);
            }
        }
    }
    return true;
}

//-----------------------------------------------------------------------------
void DagUtils::generateRandomDag(
    MacroScheduler* pMacroScheduler,
    uint32_t randSeed,
    uint32_t rank,
    uint32_t minNodesPerRank,
    uint32_t maxNodesPerRank,
    uint32_t precentChanceOfEdge,
    Vector<Node*>& dag)
{
    GTS_ASSERT(rank > 0);

    GTS_ASSERT(minNodesPerRank > 0);
    GTS_ASSERT(minNodesPerRank <= maxNodesPerRank);

    srand(randSeed);

    dag.resize(1); // root node
    dag[0] = pMacroScheduler->allocateNode();
    dag[0]->setName("%d", 0);
    uint32_t prevRankStartIdx = 0;

    for (uint32_t iRank = 0; iRank < rank; ++iRank)
    {
        // Generate the number of Nodes for this rank.
        uint32_t numNewNodes = minNodesPerRank + (rand() % (maxNodesPerRank - minNodesPerRank + 1));
        uint32_t prevNodeCount = (uint32_t)dag.size();
        GTS_ASSERT(prevNodeCount > prevRankStartIdx);
        uint32_t prevRankNodeCount = prevNodeCount - prevRankStartIdx;
        dag.resize(prevNodeCount + numNewNodes);

        // Create the new nodes.
        for (uint32_t iNew = prevNodeCount; iNew < dag.size(); ++iNew)
        {
            dag[iNew] = pMacroScheduler->allocateNode();
            dag[iNew]->setName("%d", iNew);
        }

        // Each node must have one edge to a previous node
        for (uint32_t iNew = prevNodeCount; iNew < dag.size(); ++iNew)
        {
            uint32_t prevIdx = prevRankStartIdx + (rand() % prevRankNodeCount);
            dag[prevIdx]->addSuccessor(dag[iNew]);
        }

        if (precentChanceOfEdge != 0)
        {
            // Possibly make edges from old DAG to new Nodes.
            for (uint32_t iNew = prevNodeCount; iNew < dag.size(); ++iNew)
            {
                if ((rand() % 100u) < precentChanceOfEdge)
                {
                    uint32_t iPrev = rand() % prevNodeCount;

                    // Don't add duplicates.
                    if (std::find(dag[iPrev]->successors().begin(),
                        dag[iPrev]->successors().end(), dag[iNew]) == dag[iPrev]->successors().end())
                    {
                        dag[iPrev]->addSuccessor(dag[iNew]);
                    }
                }
            }
        }

        if (iRank == rank - 1)
        {
            // Join all leaf Nodes to a sink.
            Node* pSink = pMacroScheduler->allocateNode();
            pSink->setName("%d", (uint32_t)dag.size());

            for (uint32_t iNode = 0; iNode < dag.size(); ++iNode)
            {
                if (dag[iNode]->successors().empty())
                {
                    dag[iNode]->addSuccessor(pSink);
                }
            }

            dag.push_back(pSink);
        }

        prevRankStartIdx += prevRankNodeCount;
    }

    GTS_ASSERT(DagUtils::isAcyclic(dag[0]));
}

//-----------------------------------------------------------------------------
bool DagUtils::isATopologicalOrdering(
    Vector<Node*>& dag,
    Vector<uint32_t> const& executionOrderToCheck)
{
    Node::resetGraph(dag[0]);

    std::unordered_set<Node*> visited;
    bool isTopoOrder = true;

    if (dag.size() != executionOrderToCheck.size())
    {
        isTopoOrder = false;
    }
    else
    {
        for (auto nodeIdx : executionOrderToCheck)
        {
            Node* pCurr = dag[nodeIdx];
            if (visited.find(pCurr) != visited.end() || pCurr->currPredecessorCount().load(memory_order::relaxed) != 0)
            {
                isTopoOrder = false;
                break;
            }

            visited.insert(pCurr);
            for (uint32_t ii = 0; ii < pCurr->successors().size(); ++ii)
            {
                pCurr->successors()[ii]->_removePredecessorRefAndReturnReady();
            }
        }
    }

    // Reset numPredecessors.
    for (uint32_t ii = 0; ii < dag.size(); ++ii)
    {
        dag[ii]->reset();
    }

    return isTopoOrder;
}

} // namespace gts
