// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\Containers\Associative\NkBinaryTree.h
// DESCRIPTION: Generic binary tree structure
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ASSOCIATIVE_NKBINARYTREE_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ASSOCIATIVE_NKBINARYTREE_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKContainers/NkContainersExport.h"
#include "NKCore/NkTraits.h"
#include "NKMemory/NkAllocator.h"
#include "NKMemory/NkMemoryFn.h"
#include "NKCore/Assert/NkAssert.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
    namespace core {
        
        /**
         * @brief Generic binary tree (non-balanced)
         * 
         * Arbre binaire simple sans équilibrage automatique.
         * Chaque nœud peut avoir jusqu'à 2 enfants (gauche, droit).
         * 
         * Complexité:
         * - Insert: O(h) où h = hauteur
         * - Find: O(h) 
         * - Worst case: O(n) si dégénéré
         * 
         * @example
         * NkBinaryTree<int> tree;
         * tree.Insert(5);
         * tree.Insert(3);
         * tree.Insert(7);
         * 
         * tree.InOrder([](int val) {
         *     printf("%d ", val);  // 3 5 7
         * });
         */
        template<typename T, typename Allocator = memory::NkAllocator>
        class NkBinaryTree {
        public:
            struct Node {
                T Value;
                Node* Left;
                Node* Right;
                
                Node(const T& value)
                    : Value(value), Left(nullptr), Right(nullptr) {
                }
            };
            
            using ValueType = T;
            using SizeType = usize;
            
        private:
            Node* mRoot;
            SizeType mSize;
            Allocator* mAllocator;
            
            Node* CreateNode(const T& value) {
                Node* node = static_cast<Node*>(mAllocator->Allocate(sizeof(Node)));
                new (node) Node(value);
                return node;
            }
            
            void DestroyNode(Node* node) {
                node->~Node();
                mAllocator->Deallocate(node);
            }
            
            void DestroyTree(Node* node) {
                if (!node) return;
                DestroyTree(node->Left);
                DestroyTree(node->Right);
                DestroyNode(node);
            }
            
            Node* InsertRecursive(Node* node, const T& value) {
                if (!node) {
                    ++mSize;
                    return CreateNode(value);
                }
                
                if (value < node->Value) {
                    node->Left = InsertRecursive(node->Left, value);
                } else if (node->Value < value) {
                    node->Right = InsertRecursive(node->Right, value);
                }
                // Duplicate ignored
                
                return node;
            }
            
            Node* FindRecursive(Node* node, const T& value) const {
                if (!node) return nullptr;
                
                if (value < node->Value) {
                    return FindRecursive(node->Left, value);
                } else if (node->Value < value) {
                    return FindRecursive(node->Right, value);
                } else {
                    return node;
                }
            }
            
            template<typename Func>
            void InOrderRecursive(Node* node, Func& visitor) const {
                if (!node) return;
                InOrderRecursive(node->Left, visitor);
                visitor(node->Value);
                InOrderRecursive(node->Right, visitor);
            }
            
            template<typename Func>
            void PreOrderRecursive(Node* node, Func& visitor) const {
                if (!node) return;
                visitor(node->Value);
                PreOrderRecursive(node->Left, visitor);
                PreOrderRecursive(node->Right, visitor);
            }
            
            template<typename Func>
            void PostOrderRecursive(Node* node, Func& visitor) const {
                if (!node) return;
                PostOrderRecursive(node->Left, visitor);
                PostOrderRecursive(node->Right, visitor);
                visitor(node->Value);
            }
            
            usize HeightRecursive(Node* node) const {
                if (!node) return 0;
                usize leftHeight = HeightRecursive(node->Left);
                usize rightHeight = HeightRecursive(node->Right);
                return 1 + (leftHeight > rightHeight ? leftHeight : rightHeight);
            }
            
            Node* CopyTree(Node* node) {
                if (!node) return nullptr;
                
                Node* newNode = CreateNode(node->Value);
                newNode->Left = CopyTree(node->Left);
                newNode->Right = CopyTree(node->Right);
                return newNode;
            }
            
        public:
            // Constructors
            NkBinaryTree()
                : mRoot(nullptr), mSize(0)
                , mAllocator(&memory::NkGetDefaultAllocator()) {
            }
            
            NkBinaryTree(const NkBinaryTree& other)
                : mRoot(nullptr), mSize(other.mSize)
                , mAllocator(other.mAllocator) {
                mRoot = CopyTree(other.mRoot);
            }
            
            ~NkBinaryTree() {
                Clear();
            }
            
            // Assignment
            NkBinaryTree& operator=(const NkBinaryTree& other) {
                if (this != &other) {
                    Clear();
                    mRoot = CopyTree(other.mRoot);
                    mSize = other.mSize;
                }
                return *this;
            }
            
            // Capacity
            bool IsEmpty() const NK_NOEXCEPT { return mSize == 0; }
            SizeType Size() const NK_NOEXCEPT { return mSize; }
            
            usize Height() const {
                return HeightRecursive(mRoot);
            }
            
            // Modifiers
            void Clear() {
                DestroyTree(mRoot);
                mRoot = nullptr;
                mSize = 0;
            }
            
            void Insert(const T& value) {
                mRoot = InsertRecursive(mRoot, value);
            }
            
            // Lookup
            bool Contains(const T& value) const {
                return FindRecursive(mRoot, value) != nullptr;
            }
            
            const T* Find(const T& value) const {
                Node* node = FindRecursive(mRoot, value);
                return node ? &node->Value : nullptr;
            }
            
            // Access
            Node* GetRoot() const NK_NOEXCEPT { return mRoot; }
            
            // Traversal
            template<typename Func>
            void InOrder(Func visitor) const {
                InOrderRecursive(mRoot, visitor);
            }
            
            template<typename Func>
            void PreOrder(Func visitor) const {
                PreOrderRecursive(mRoot, visitor);
            }
            
            template<typename Func>
            void PostOrder(Func visitor) const {
                PostOrderRecursive(mRoot, visitor);
            }
            
            template<typename Func>
            void LevelOrder(Func visitor) const {
                if (!mRoot) return;
                
                NkVector<Node*> queue;
                queue.PushBack(mRoot);
                
                while (!queue.IsEmpty()) {
                    Node* node = queue.Front();
                    queue.Erase(queue.begin());
                    
                    visitor(node->Value);
                    
                    if (node->Left) queue.PushBack(node->Left);
                    if (node->Right) queue.PushBack(node->Right);
                }
            }
            
            // Utility
            T Min() const {
                NK_ASSERT(mRoot);
                Node* node = mRoot;
                while (node->Left) node = node->Left;
                return node->Value;
            }
            
            T Max() const {
                NK_ASSERT(mRoot);
                Node* node = mRoot;
                while (node->Right) node = node->Right;
                return node->Value;
            }
            
            bool IsBalanced() const {
                return IsBalancedRecursive(mRoot) >= 0;
            }
            
        private:
            int IsBalancedRecursive(Node* node) const {
                if (!node) return 0;
                
                int leftHeight = IsBalancedRecursive(node->Left);
                if (leftHeight < 0) return -1;
                
                int rightHeight = IsBalancedRecursive(node->Right);
                if (rightHeight < 0) return -1;
                
                int diff = leftHeight - rightHeight;
                if (diff < -1 || diff > 1) return -1;
                
                return 1 + (leftHeight > rightHeight ? leftHeight : rightHeight);
            }
        };
        
    } // namespace core
} // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ASSOCIATIVE_NKBINARYTREE_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-05 22:26:13
// Creation Date: 2026-02-05 22:26:13
// ============================================================
