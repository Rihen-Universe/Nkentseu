// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\Containers\Specialized\NkGraph.h
// DESCRIPTION: Graph data structure - Vertices and edges
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_SPECIALIZED_NKGRAPH_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_SPECIALIZED_NKGRAPH_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKContainers/NkContainersExport.h"
#include "NKCore/NkTraits.h"
#include "NKCore/Assert/NkAssert.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/Associative/NkHashMap.h"
#include "NKContainers/Associative/NkUnorderedSet.h"

namespace nkentseu {
    
        
        /**
         * @brief Graph structure - Vertices and edges
         * 
         * Graphe générique (dirigé ou non-dirigé).
         * Stockage: Adjacency list (efficace pour graphes sparse).
         * 
         * Complexité:
         * - AddVertex: O(1)
         * - AddEdge: O(1)
         * - HasEdge: O(degree)
         * - GetNeighbors: O(1)
         * 
         * @example
         * NkGraph<int> graph(false);  // Undirected
         * graph.AddVertex(1);
         * graph.AddVertex(2);
         * graph.AddEdge(1, 2, 1.0f);  // Edge with weight
         */
        template<typename VertexType, typename Allocator = memory::NkAllocator>
        class NkGraph {
        public:
            struct Edge {
                VertexType From;
                VertexType To;
                float Weight;
                
                Edge(const VertexType& from, const VertexType& to, float weight = 1.0f)
                    : From(from), To(to), Weight(weight) {
                }
            };
            
            using SizeType = usize;
            using VertexList = NkVector<VertexType, Allocator>;
            using EdgeList = NkVector<Edge, Allocator>;
            using AdjacencyList = NkHashMap<VertexType, VertexList, Allocator>;
            
        private:
            AdjacencyList mAdjacency;
            bool mDirected;
            SizeType mVertexCount;
            SizeType mEdgeCount;
            Allocator* mAllocator;
            
        public:
            // Constructors
            explicit NkGraph(bool directed = false, Allocator* allocator = nullptr)
                : mAdjacency(allocator ? allocator : &memory::NkGetDefaultAllocator())
                , mDirected(directed)
                , mVertexCount(0)
                , mEdgeCount(0)
                , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator()) {
            }
            
            // Vertex operations
            void AddVertex(const VertexType& vertex) {
                if (!mAdjacency.Contains(vertex)) {
                    mAdjacency.Insert(vertex, VertexList(mAllocator));
                    ++mVertexCount;
                }
            }
            
            bool HasVertex(const VertexType& vertex) const {
                return mAdjacency.Contains(vertex);
            }
            
            void RemoveVertex(const VertexType& vertex) {
                if (!mAdjacency.Contains(vertex)) return;
                
                // Remove all edges to this vertex
                for (auto& pair : mAdjacency) {
                    VertexList* neighbors = mAdjacency.Find(pair.First);
                    if (neighbors) {
                        // Remove vertex from neighbor lists
                        for (SizeType i = 0; i < neighbors->Size(); ) {
                            if ((*neighbors)[i] == vertex) {
                                neighbors->Erase(neighbors->begin() + i);
                                --mEdgeCount;
                            } else {
                                ++i;
                            }
                        }
                    }
                }
                
                // Remove vertex's adjacency list
                mAdjacency.Erase(vertex);
                --mVertexCount;
            }
            
            // Edge operations
            void AddEdge(const VertexType& from, const VertexType& to, float weight = 1.0f) {
                AddVertex(from);
                AddVertex(to);
                
                VertexList* neighbors = mAdjacency.Find(from);
                if (neighbors) {
                    neighbors->PushBack(to);
                    ++mEdgeCount;
                }
                
                if (!mDirected && from != to) {
                    neighbors = mAdjacency.Find(to);
                    if (neighbors) {
                        neighbors->PushBack(from);
                    }
                }
            }
            
            bool HasEdge(const VertexType& from, const VertexType& to) const {
                const VertexList* neighbors = mAdjacency.Find(from);
                if (!neighbors) return false;
                
                for (SizeType i = 0; i < neighbors->Size(); ++i) {
                    if ((*neighbors)[i] == to) return true;
                }
                return false;
            }
            
            void RemoveEdge(const VertexType& from, const VertexType& to) {
                VertexList* neighbors = mAdjacency.Find(from);
                if (neighbors) {
                    for (SizeType i = 0; i < neighbors->Size(); ) {
                        if ((*neighbors)[i] == to) {
                            neighbors->Erase(neighbors->begin() + i);
                            --mEdgeCount;
                        } else {
                            ++i;
                        }
                    }
                }
                
                if (!mDirected) {
                    neighbors = mAdjacency.Find(to);
                    if (neighbors) {
                        for (SizeType i = 0; i < neighbors->Size(); ) {
                            if ((*neighbors)[i] == from) {
                                neighbors->Erase(neighbors->begin() + i);
                            } else {
                                ++i;
                            }
                        }
                    }
                }
            }
            
            // Queries
            const VertexList* GetNeighbors(const VertexType& vertex) const {
                return mAdjacency.Find(vertex);
            }
            
            SizeType GetDegree(const VertexType& vertex) const {
                const VertexList* neighbors = mAdjacency.Find(vertex);
                return neighbors ? neighbors->Size() : 0;
            }
            
            SizeType VertexCount() const NK_NOEXCEPT { return mVertexCount; }
            SizeType EdgeCount() const NK_NOEXCEPT { return mEdgeCount; }
            bool IsDirected() const NK_NOEXCEPT { return mDirected; }
            bool Empty() const NK_NOEXCEPT { return mVertexCount == 0; }
            Allocator* GetAllocator() const NK_NOEXCEPT { return mAllocator; }
            
            // Traversal helpers
            template<typename Func>
            void DFS(const VertexType& start, Func visitor) {
                NkUnorderedSet<VertexType, Allocator> visited(mAllocator);
                DFSRecursive(start, visited, visitor);
            }
            
            template<typename Func>
            void BFS(const VertexType& start, Func visitor) {
                NkUnorderedSet<VertexType, Allocator> visited(mAllocator);
                NkVector<VertexType, Allocator> queue(mAllocator);
                
                queue.PushBack(start);
                visited.Insert(start);
                
                while (!queue.Empty()) {
                    VertexType vertex = queue.Front();
                    queue.Erase(queue.begin());
                    
                    visitor(vertex);
                    
                    const VertexList* neighbors = GetNeighbors(vertex);
                    if (neighbors) {
                        for (SizeType i = 0; i < neighbors->Size(); ++i) {
                            if (!visited.Contains((*neighbors)[i])) {
                                queue.PushBack((*neighbors)[i]);
                                visited.Insert((*neighbors)[i]);
                            }
                        }
                    }
                }
            }
            
        private:
            template<typename Func>
            void DFSRecursive(const VertexType& vertex,
                              NkUnorderedSet<VertexType, Allocator>& visited,
                              Func& visitor) {
                visited.Insert(vertex);
                visitor(vertex);
                
                const VertexList* neighbors = GetNeighbors(vertex);
                if (neighbors) {
                    for (SizeType i = 0; i < neighbors->Size(); ++i) {
                        if (!visited.Contains((*neighbors)[i])) {
                            DFSRecursive((*neighbors)[i], visited, visitor);
                        }
                    }
                }
            }
        };
        
    
} // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_SPECIALIZED_NKGRAPH_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-05 22:26:13
// Creation Date: 2026-02-05 22:26:13
// ============================================================
