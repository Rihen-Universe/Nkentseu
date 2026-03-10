#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKContainers/Specialized/NkGraph.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKMemory/NkAllocator.h"


using namespace nkentseu;

TEST_CASE(NKContainersGraph, DirectedAndUndirectedEdges) {
    NkGraph<int> undirected(false);
    undirected.AddEdge(1, 2);
    ASSERT_TRUE(undirected.HasEdge(1, 2));
    ASSERT_TRUE(undirected.HasEdge(2, 1));

    NkGraph<int> directed(true);
    directed.AddEdge(10, 20);
    ASSERT_TRUE(directed.HasEdge(10, 20));
    ASSERT_FALSE(directed.HasEdge(20, 10));
}

TEST_CASE(NKContainersGraph, BfsAndDfsTraversalCoverReachableNodes) {
    NkGraph<int> graph(false);
    graph.AddEdge(1, 2);
    graph.AddEdge(1, 3);
    graph.AddEdge(2, 4);

    NkVector<int> bfsVisited;
    graph.BFS(1, [&](int node) { bfsVisited.PushBack(node); });
    ASSERT_EQUAL(4, static_cast<int>(bfsVisited.Size()));
    ASSERT_EQUAL(1, bfsVisited[0]);
    ASSERT_EQUAL(2, bfsVisited[1]);
    ASSERT_EQUAL(3, bfsVisited[2]);
    ASSERT_EQUAL(4, bfsVisited[3]);

    NkVector<int> dfsVisited;
    graph.DFS(1, [&](int node) { dfsVisited.PushBack(node); });
    ASSERT_EQUAL(4, static_cast<int>(dfsVisited.Size()));
}

TEST_CASE(NKContainersGraph, AllocatorInjection) {
    memory::NkMallocAllocator allocator;
    NkGraph<int, memory::NkAllocator> graph(false, &allocator);

    ASSERT_TRUE(graph.GetAllocator() == &allocator);
    graph.AddEdge(5, 6);
    ASSERT_TRUE(graph.HasEdge(5, 6));
}
