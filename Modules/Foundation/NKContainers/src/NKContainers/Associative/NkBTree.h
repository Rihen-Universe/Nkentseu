// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\Containers\Associative\NkBTree.h
// DESCRIPTION: B-Tree - Self-balancing tree for databases
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ASSOCIATIVE_NKBTREE_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ASSOCIATIVE_NKBTREE_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKContainers/NkContainersExport.h"
#include "NKCore/NkTraits.h"
#include "NKMemory/NkAllocator.h"
#include "NKCore/Assert/NkAssert.h"

namespace nkentseu {
    
        
        /**
         * @brief B-Tree - Self-balancing multi-way tree
         * 
         * Arbre B optimisé pour systèmes avec accès disque.
         * Chaque nœud contient plusieurs clés (order).
         * 
         * Parfait pour:
         * - Bases de données
         * - Systèmes de fichiers
         * - Index sur disque
         * 
         * Complexité:
         * - Insert/Search/Delete: O(log n)
         * - Mais avec factor log_m(n) où m = order
         * 
         * @example
         * NkBTree<int> btree(5);  // Order 5 (4-8 keys per node)
         * btree.Insert(10);
         * btree.Insert(20);
         * bool found = btree.Search(10);
         */
        template<typename T, typename Allocator = memory::NkAllocator>
        class NkBTree {
        private:
            struct BTreeNode {
                T* Keys;              // Array of keys
                BTreeNode** Children; // Array of child pointers
                usize NumKeys;        // Current number of keys
                bool IsLeaf;          // Is leaf node?
                usize Order;          // Order of tree (max children)
                
                BTreeNode(usize order, bool isLeaf, Allocator* alloc)
                    : NumKeys(0), IsLeaf(isLeaf), Order(order) {
                    const usize maxKeys = 2 * order - 1;
                    const usize maxChildren = 2 * order;
                    Keys = static_cast<T*>(alloc->Allocate(maxKeys * sizeof(T)));
                    Children = static_cast<BTreeNode**>(alloc->Allocate(maxChildren * sizeof(BTreeNode*)));
                    
                    for (usize i = 0; i < maxChildren; ++i) {
                        Children[i] = nullptr;
                    }
                }
            };
            
            BTreeNode* mRoot;
            usize mOrder;      // Minimum degree (minimum children = order, max = 2*order)
            usize mSize;
            Allocator* mAllocator;

            usize MaxKeysPerNode() const NK_NOEXCEPT {
                return 2 * mOrder - 1;
            }
            
            BTreeNode* CreateNode(bool isLeaf) {
                BTreeNode* node = static_cast<BTreeNode*>(mAllocator->Allocate(sizeof(BTreeNode)));
                new (node) BTreeNode(mOrder, isLeaf, mAllocator);
                return node;
            }
            
            void DestroyNode(BTreeNode* node) {
                if (!node) return;
                
                // Destroy children
                if (!node->IsLeaf) {
                    for (usize i = 0; i <= node->NumKeys; ++i) {
                        DestroyNode(node->Children[i]);
                    }
                }
                
                // Destroy keys
                for (usize i = 0; i < node->NumKeys; ++i) {
                    node->Keys[i].~T();
                }
                
                mAllocator->Deallocate(node->Keys);
                mAllocator->Deallocate(node->Children);
                node->~BTreeNode();
                mAllocator->Deallocate(node);
            }
            
            BTreeNode* SearchRecursive(BTreeNode* node, const T& key) const {
                if (!node) return nullptr;
                
                // Find first key >= key
                usize i = 0;
                while (i < node->NumKeys && key > node->Keys[i]) {
                    ++i;
                }
                
                // Key found
                if (i < node->NumKeys && key == node->Keys[i]) {
                    return node;
                }
                
                // Not found and leaf
                if (node->IsLeaf) {
                    return nullptr;
                }
                
                // Recurse to child
                return SearchRecursive(node->Children[i], key);
            }
            
            void SplitChild(BTreeNode* parent, usize index) {
                BTreeNode* fullChild = parent->Children[index];
                BTreeNode* newChild = CreateNode(fullChild->IsLeaf);

                // Standard B-Tree split for minimum degree t = mOrder.
                const usize t = mOrder;
                newChild->NumKeys = t - 1;

                // Move upper half keys [t, 2t-2] to the new child.
                for (usize i = 0; i < t - 1; ++i) {
                    new (&newChild->Keys[i]) T(traits::NkMove(fullChild->Keys[i + t]));
                    fullChild->Keys[i + t].~T();
                }

                // Move upper half children [t, 2t-1] for internal nodes.
                if (!fullChild->IsLeaf) {
                    for (usize i = 0; i < t; ++i) {
                        newChild->Children[i] = fullChild->Children[i + t];
                        fullChild->Children[i + t] = nullptr;
                    }
                }

                T middleKey = traits::NkMove(fullChild->Keys[t - 1]);
                fullChild->Keys[t - 1].~T();
                fullChild->NumKeys = t - 1;

                // Shift children in parent to make room for newChild.
                for (usize i = parent->NumKeys + 1; i > index + 1; --i) {
                    parent->Children[i] = parent->Children[i - 1];
                }
                parent->Children[index + 1] = newChild;

                // Shift keys in parent and insert middle key.
                for (usize i = parent->NumKeys; i > index; --i) {
                    new (&parent->Keys[i]) T(traits::NkMove(parent->Keys[i - 1]));
                    parent->Keys[i - 1].~T();
                }
                new (&parent->Keys[index]) T(traits::NkMove(middleKey));
                parent->NumKeys++;
            }
            
            void InsertNonFull(BTreeNode* node, const T& key) {
                if (node->IsLeaf) {
                    usize insertPos = node->NumKeys;
                    while (insertPos > 0 && key < node->Keys[insertPos - 1]) {
                        --insertPos;
                    }

                    for (usize i = node->NumKeys; i > insertPos; --i) {
                        new (&node->Keys[i]) T(traits::NkMove(node->Keys[i - 1]));
                        node->Keys[i - 1].~T();
                    }

                    new (&node->Keys[insertPos]) T(key);
                    node->NumKeys++;
                } else {
                    usize childIndex = node->NumKeys;
                    while (childIndex > 0 && key < node->Keys[childIndex - 1]) {
                        --childIndex;
                    }
                    
                    // Split child if full
                    if (node->Children[childIndex]->NumKeys == MaxKeysPerNode()) {
                        SplitChild(node, childIndex);
                        if (key > node->Keys[childIndex]) {
                            ++childIndex;
                        }
                    }
                    
                    InsertNonFull(node->Children[childIndex], key);
                }
            }
            
        public:
            // Constructors
            explicit NkBTree(usize order = 3, Allocator* allocator = nullptr)
                : mRoot(nullptr)
                , mOrder(order < 3 ? 3 : order)
                , mSize(0)
                , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator()) {
                NK_ASSERT(order >= 3);
                mRoot = CreateNode(true);
            }
            
            ~NkBTree() {
                DestroyNode(mRoot);
            }
            
            // Operations
            void Insert(const T& key) {
                // If root is full, split it
                if (mRoot->NumKeys == MaxKeysPerNode()) {
                    BTreeNode* newRoot = CreateNode(false);
                    newRoot->Children[0] = mRoot;
                    SplitChild(newRoot, 0);
                    mRoot = newRoot;
                }
                
                InsertNonFull(mRoot, key);
                ++mSize;
            }
            
            bool Search(const T& key) const {
                return SearchRecursive(mRoot, key) != nullptr;
            }
            
            // Capacity
            usize Size() const NK_NOEXCEPT { return mSize; }
            bool Empty() const NK_NOEXCEPT { return mSize == 0; }
        };
        
    
} // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ASSOCIATIVE_NKBTREE_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// 
// USAGE:
// 
// NkBTree<int> btree(5);  // Order 5
// btree.Insert(10);
// btree.Insert(20);
// btree.Insert(5);
// btree.Insert(15);
// 
// bool found = btree.Search(10);  // true
// 
// B-TREE PROPERTIES:
// - All leaves at same level
// - Node with k keys has k+1 children
// - Keys in node sorted
// - Min keys per node: order-1
// - Max keys per node: 2*order-1
// 
// ADVANTAGES:
// - Optimized for disk access
// - Fewer disk reads than binary trees
// - Good cache performance
// - Used in databases (MySQL, PostgreSQL)
// - Used in filesystems (ext4, NTFS, HFS+)
// 
// ============================================================

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-05 22:26:13
// Creation Date: 2026-02-05 22:26:13
// ============================================================
