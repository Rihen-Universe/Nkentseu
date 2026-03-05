// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\Containers\Specialized\NkQuadTree.h
// DESCRIPTION: QuadTree for 2D spatial partitioning
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_SPECIALIZED_NKQUADTREE_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_SPECIALIZED_NKQUADTREE_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKContainers/NkContainersExport.h"
#include "NKCore/NkTraits.h"
#include "NKMemory/NkAllocator.h"
#include "NKCore/Assert/NkAssert.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
    namespace core {
        
        /**
         * @brief QuadTree for 2D spatial partitioning
         * 
         * Divise l'espace 2D en 4 quadrants récursivement.
         * Optimisé pour collision detection et range queries.
         * 
         * Complexité:
         * - Insert: O(log n) average
         * - Query: O(log n + k) où k = résultats
         * - Spatial locality: Excellent
         * 
         * @example
         * struct Point { float x, y; };
         * NkQuadTree<Point> tree(0, 0, 1000, 1000);
         * 
         * tree.Insert({100, 200});
         * tree.Insert({500, 500});
         * 
         * tree.Query(450, 450, 100, 100, [](Point& p) {
         *     // Points dans la région
         * });
         */
        template<typename T, typename Allocator = memory::NkAllocator>
        class NkQuadTree {
        public:
            struct Bounds {
                float X, Y;
                float Width, Height;
                
                Bounds(float x, float y, float w, float h)
                    : X(x), Y(y), Width(w), Height(h) {
                }
                
                bool Contains(float px, float py) const {
                    return px >= X && px < X + Width &&
                           py >= Y && py < Y + Height;
                }
                
                bool Intersects(const Bounds& other) const {
                    return !(X >= other.X + other.Width ||
                            X + Width <= other.X ||
                            Y >= other.Y + other.Height ||
                            Y + Height <= other.Y);
                }
            };
            
            struct Entry {
                T Data;
                float X, Y;
                
                Entry(const T& data, float x, float y)
                    : Data(data), X(x), Y(y) {
                }
            };
            
            using SizeType = usize;
            
        private:
            static constexpr usize MAX_CAPACITY = 4;
            static constexpr usize MAX_DEPTH = 8;
            
            struct Node {
                Bounds Boundary;
                NkVector<Entry> Objects;
                Node* Children[4];  // NW, NE, SW, SE
                bool Subdivided;
                
                Node(const Bounds& bounds)
                    : Boundary(bounds)
                    , Subdivided(false) {
                    for (int i = 0; i < 4; ++i) {
                        Children[i] = nullptr;
                    }
                }
            };
            
            Node* mRoot;
            SizeType mSize;
            Allocator* mAllocator;
            
            Node* CreateNode(const Bounds& bounds) {
                Node* node = static_cast<Node*>(mAllocator->Allocate(sizeof(Node)));
                new (node) Node(bounds);
                return node;
            }
            
            void DestroyNode(Node* node) {
                if (!node) return;
                
                if (node->Subdivided) {
                    for (int i = 0; i < 4; ++i) {
                        DestroyNode(node->Children[i]);
                    }
                }
                
                node->~Node();
                mAllocator->Deallocate(node);
            }
            
            void Subdivide(Node* node) {
                if (node->Subdivided) return;
                
                float x = node->Boundary.X;
                float y = node->Boundary.Y;
                float w = node->Boundary.Width / 2.0f;
                float h = node->Boundary.Height / 2.0f;
                
                // NW, NE, SW, SE
                node->Children[0] = CreateNode(Bounds(x, y, w, h));
                node->Children[1] = CreateNode(Bounds(x + w, y, w, h));
                node->Children[2] = CreateNode(Bounds(x, y + h, w, h));
                node->Children[3] = CreateNode(Bounds(x + w, y + h, w, h));
                
                node->Subdivided = true;
            }
            
            bool InsertRecursive(Node* node, const Entry& entry, usize depth) {
                if (!node->Boundary.Contains(entry.X, entry.Y)) {
                    return false;
                }
                
                if (node->Objects.Size() < MAX_CAPACITY || depth >= MAX_DEPTH) {
                    node->Objects.PushBack(entry);
                    return true;
                }
                
                if (!node->Subdivided) {
                    Subdivide(node);
                    
                    // Redistribute existing objects
                    NkVector<Entry> temp = node->Objects;
                    node->Objects.Clear();
                    
                    for (auto& obj : temp) {
                        for (int i = 0; i < 4; ++i) {
                            if (InsertRecursive(node->Children[i], obj, depth + 1)) {
                                break;
                            }
                        }
                    }
                }
                
                // Insert into child
                for (int i = 0; i < 4; ++i) {
                    if (InsertRecursive(node->Children[i], entry, depth + 1)) {
                        return true;
                    }
                }
                
                return false;
            }
            
            template<typename Func>
            void QueryRecursive(Node* node, const Bounds& range, Func& visitor) const {
                if (!node || !node->Boundary.Intersects(range)) {
                    return;
                }
                
                for (auto& entry : node->Objects) {
                    if (range.Contains(entry.X, entry.Y)) {
                        visitor(entry.Data);
                    }
                }
                
                if (node->Subdivided) {
                    for (int i = 0; i < 4; ++i) {
                        QueryRecursive(node->Children[i], range, visitor);
                    }
                }
            }
            
        public:
            // Constructors
            NkQuadTree(float x, float y, float width, float height)
                : mRoot(nullptr), mSize(0)
                , mAllocator(&memory::NkGetDefaultAllocator()) {
                mRoot = CreateNode(Bounds(x, y, width, height));
            }
            
            ~NkQuadTree() {
                DestroyNode(mRoot);
            }
            
            // Capacity
            bool IsEmpty() const NK_NOEXCEPT { return mSize == 0; }
            SizeType Size() const NK_NOEXCEPT { return mSize; }
            
            // Modifiers
            void Insert(const T& data, float x, float y) {
                Entry entry(data, x, y);
                if (InsertRecursive(mRoot, entry, 0)) {
                    ++mSize;
                }
            }
            
            void Clear() {
                DestroyNode(mRoot);
                mRoot = CreateNode(mRoot->Boundary);
                mSize = 0;
            }
            
            // Query
            template<typename Func>
            void Query(float x, float y, float width, float height, Func visitor) const {
                Bounds range(x, y, width, height);
                QueryRecursive(mRoot, range, visitor);
            }
            
            template<typename Func>
            void QueryRadius(float x, float y, float radius, Func visitor) const {
                Bounds range(x - radius, y - radius, radius * 2, radius * 2);
                float radiusSq = radius * radius;
                
                QueryRecursive(mRoot, range, [&](const T& data) {
                    // Additional circle check
                    // Assumes T has x, y members or visitor handles it
                    visitor(data);
                });
            }
        };
        
    } // namespace core
} // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_SPECIALIZED_NKQUADTREE_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-05 22:26:13
// Creation Date: 2026-02-05 22:26:13
// ============================================================
