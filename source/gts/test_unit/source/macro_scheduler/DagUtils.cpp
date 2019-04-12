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
 #include "DagUtils.h"

#include <fstream>
#include <queue>
#include "gts/platform/Assert.h"

 //-----------------------------------------------------------------------------
void DagUtils::generateRandomDag(
    uint32_t randSeed,
    uint32_t rank,
    uint32_t minNodesPerRank,
    uint32_t maxNodesPerRank,
    uint32_t precentChanceOfEdge,
    gts::Vector<Node>& dag)
{
    GTS_ASSERT(rank > 0);

    GTS_ASSERT(minNodesPerRank > 0);
    GTS_ASSERT(minNodesPerRank <= maxNodesPerRank);

    srand(randSeed);

    dag.resize(1); // root node
    uint32_t prevRankStartIdx = 0;

    for (uint32_t ii = 0; ii < rank; ++ii)
    {
        // Generate the number of Nodes for this rank.
        uint32_t newNodeCount = minNodesPerRank + (rand() % (maxNodesPerRank - minNodesPerRank + 1));
        uint32_t prevNodeCount = (uint32_t)dag.size();
        dag.resize(prevNodeCount + newNodeCount);

        // Each node must have one edge to a previous node
        for (uint32_t iNew = prevNodeCount; iNew < prevNodeCount + newNodeCount; ++iNew)
        {
            uint32_t prevIdx = rand() % prevNodeCount;
            dag[prevIdx].successorIndices.push_back(iNew);
            dag[iNew].numPredecessors++;
        }

        // Make edges from old DAG to new Nodes.
        for (uint32_t iPrev = prevRankStartIdx; iPrev < prevNodeCount; ++iPrev)
        {
            for (uint32_t iNew = prevNodeCount; iNew < prevNodeCount + newNodeCount; ++iNew)
            {
                if ((rand() % 100u) < precentChanceOfEdge)
                {
                    // Don't add duplicates.
                    if (std::find(dag[iPrev].successorIndices.begin(),
                        dag[iPrev].successorIndices.end(), iNew) == dag[iPrev].successorIndices.end())
                    {
                        dag[iPrev].successorIndices.push_back(iNew);
                        dag[iNew].numPredecessors++;
                    }
                }
            }
        }

        if (ii == rank - 1)
        {
            // Join all dag to a sink.
            Node sink;
            dag.push_back(sink);
            uint32_t lastIndex = (uint32_t)dag.size() - 1;

            for (uint32_t iNew = prevNodeCount; iNew < prevNodeCount + newNodeCount; ++iNew)
            {
                dag[iNew].successorIndices.push_back(lastIndex);
                dag[lastIndex].numPredecessors++;
            }
        }

        prevRankStartIdx += prevNodeCount;
    }
}

//-----------------------------------------------------------------------------
bool DagUtils::isATopologicalOrdering(
    gts::Vector<Node> dag,
    gts::Vector<uint32_t> const& executionOrderToCheck)
{
    for (auto nodeIdx : executionOrderToCheck)
    {
        Node* pCurr = &dag[nodeIdx];
        if (pCurr->visited || pCurr->numPredecessors != 0)
        {
            return false;
        }

        pCurr->visited = true;
        for (uint32_t ii = 0; ii < pCurr->successorIndices.size(); ++ii)
        {
            size_t idx = pCurr->successorIndices[ii];
            dag[idx].numPredecessors--;
        }
    }
    return true;
}

//-----------------------------------------------------------------------------
void DagUtils::dagToDotFile(
    const char* filename,
    gts::Vector<DagUtils::Node> const& dag)
{
    std::ofstream file(filename);
    file << "digraph {\n";
    for (uint32_t ii = 0; ii < dag.size(); ++ii)
    {
        Node const& n = dag[ii];
        for (auto idx : n.successorIndices)
        {
            file << ii << " -> " << idx << '\n';
        }
    }
    file << "}\n";
}
