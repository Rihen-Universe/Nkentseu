// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\Containers\Associative\NkSet.h
// DESCRIPTION: Ordered set - Red-Black Tree implementation
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ASSOCIATIVE_NKSET_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ASSOCIATIVE_NKSET_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKContainers/NkContainersExport.h"
#include "NKCore/NkTraits.h"
#include "NKMemory/NkAllocator.h"
#include "NKMemory/NkFunction.h"
#include "NKCore/Assert/NkAssert.h"
#include "NKContainers/Iterators/NkIterator.h"
#include "NKContainers/Iterators/NkInitializerList.h"

namespace nkentseu {
    
        
        /**
         * @brief Ordered set - std::set equivalent
         * 
         * Implémentation avec Red-Black Tree.
         * Les éléments sont toujours triés.
         * 
         * Complexité:
         * - Insert/Find/Erase: O(log n)
         * - Min/Max: O(log n) ou O(1) avec cache
         * - Iteration: O(n) en ordre trié
         * 
         * @example
         * NkSet<int> set = {3, 1, 4, 1, 5};
         * // Stored as: [1, 3, 4, 5] (unique, sorted)
         * 
         * set.Insert(2);
         * if (set.Contains(3)) { }
         * set.Erase(1);
         */
        template<typename T, typename Allocator = memory::NkAllocator>
        class NkSet {
        private:
            enum Color { RED, BLACK };
            
            struct Node {
                T Value;
                Node* Left;
                Node* Right;
                Node* Parent;
                Color NodeColor;
                
                Node(const T& value, Node* parent = nullptr)
                    : Value(value)
                    , Left(nullptr)
                    , Right(nullptr)
                    , Parent(parent)
                    , NodeColor(RED) {
                }
            };
            
        public:
            using ValueType = T;
            using SizeType = usize;
            using Reference = T&;
            using ConstReference = const T&;
            
            // Forward iterator (in-order traversal)
            class Iterator {
            private:
                Node* mNode;
                const NkSet* mSet;
                friend class NkSet;
                
                Node* NextNode(Node* node) {
                    if (!node) return nullptr;
                    
                    if (node->Right) {
                        node = node->Right;
                        while (node->Left) node = node->Left;
                        return node;
                    }
                    
                    Node* parent = node->Parent;
                    while (parent && node == parent->Right) {
                        node = parent;
                        parent = parent->Parent;
                    }
                    return parent;
                }
                
            public:
                using ValueType = T;
                using Reference = const T&;
                using Pointer = const T*;
                using DifferenceType = intptr;
                using IteratorCategory = NkBidirectionalIteratorTag;
                
                Iterator() : mNode(nullptr), mSet(nullptr) {}
                Iterator(Node* node, const NkSet* set) : mNode(node), mSet(set) {}
                
                Reference operator*() const { return mNode->Value; }
                Pointer operator->() const { return &mNode->Value; }
                
                Iterator& operator++() {
                    mNode = NextNode(mNode);
                    return *this;
                }
                
                Iterator operator++(int) {
                    Iterator tmp = *this;
                    ++(*this);
                    return tmp;
                }
                
                bool operator==(const Iterator& o) const { return mNode == o.mNode; }
                bool operator!=(const Iterator& o) const { return mNode != o.mNode; }
            };
            
            using ConstIterator = Iterator;
            
        private:
            Node* mRoot;
            SizeType mSize;
            Allocator* mAllocator;
            
            Node* CreateNode(const T& value, Node* parent = nullptr) {
                Node* node = static_cast<Node*>(mAllocator->Allocate(sizeof(Node)));
                new (node) Node(value, parent);
                return node;
            }
            
            void DestroyNode(Node* node) {
                node->~Node();
                mAllocator->Deallocate(node);
            }
            
            void RotateLeft(Node* node) {
                Node* right = node->Right;
                node->Right = right->Left;
                
                if (right->Left) right->Left->Parent = node;
                
                right->Parent = node->Parent;
                if (!node->Parent) {
                    mRoot = right;
                } else if (node == node->Parent->Left) {
                    node->Parent->Left = right;
                } else {
                    node->Parent->Right = right;
                }
                
                right->Left = node;
                node->Parent = right;
            }
            
            void RotateRight(Node* node) {
                Node* left = node->Left;
                node->Left = left->Right;
                
                if (left->Right) left->Right->Parent = node;
                
                left->Parent = node->Parent;
                if (!node->Parent) {
                    mRoot = left;
                } else if (node == node->Parent->Right) {
                    node->Parent->Right = left;
                } else {
                    node->Parent->Left = left;
                }
                
                left->Right = node;
                node->Parent = left;
            }
            
            void FixInsert(Node* node) {
                while (node->Parent && node->Parent->NodeColor == RED) {
                    if (node->Parent == node->Parent->Parent->Left) {
                        Node* uncle = node->Parent->Parent->Right;
                        
                        if (uncle && uncle->NodeColor == RED) {
                            node->Parent->NodeColor = BLACK;
                            uncle->NodeColor = BLACK;
                            node->Parent->Parent->NodeColor = RED;
                            node = node->Parent->Parent;
                        } else {
                            if (node == node->Parent->Right) {
                                node = node->Parent;
                                RotateLeft(node);
                            }
                            node->Parent->NodeColor = BLACK;
                            node->Parent->Parent->NodeColor = RED;
                            RotateRight(node->Parent->Parent);
                        }
                    } else {
                        Node* uncle = node->Parent->Parent->Left;
                        
                        if (uncle && uncle->NodeColor == RED) {
                            node->Parent->NodeColor = BLACK;
                            uncle->NodeColor = BLACK;
                            node->Parent->Parent->NodeColor = RED;
                            node = node->Parent->Parent;
                        } else {
                            if (node == node->Parent->Left) {
                                node = node->Parent;
                                RotateRight(node);
                            }
                            node->Parent->NodeColor = BLACK;
                            node->Parent->Parent->NodeColor = RED;
                            RotateLeft(node->Parent->Parent);
                        }
                    }
                }
                mRoot->NodeColor = BLACK;
            }
            
            void DestroyTree(Node* node) {
                if (!node) return;
                DestroyTree(node->Left);
                DestroyTree(node->Right);
                DestroyNode(node);
            }
            
            Node* FindMin(Node* node) const {
                while (node && node->Left) node = node->Left;
                return node;
            }
            
        public:
            // Constructors
            explicit NkSet(Allocator* allocator = nullptr)
                : mRoot(nullptr), mSize(0)
                , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator()) {
            }
            
            NkSet(NkInitializerList<T> init, Allocator* allocator = nullptr)
                : mRoot(nullptr), mSize(0)
                , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator()) {
                for (auto& val : init) Insert(val);
            }
            
            ~NkSet() {
                Clear();
            }
            
            // Iterators
            Iterator begin() { return Iterator(FindMin(mRoot), this); }
            ConstIterator begin() const { return ConstIterator(FindMin(mRoot), this); }
            Iterator end() { return Iterator(nullptr, this); }
            ConstIterator end() const { return ConstIterator(nullptr, this); }
            
            // Capacity
            bool Empty() const NK_NOEXCEPT { return mSize == 0; }
            SizeType Size() const NK_NOEXCEPT { return mSize; }
            
            // Modifiers
            void Clear() {
                DestroyTree(mRoot);
                mRoot = nullptr;
                mSize = 0;
            }
            
            bool Insert(const T& value) {
                if (!mRoot) {
                    mRoot = CreateNode(value);
                    mRoot->NodeColor = BLACK;
                    ++mSize;
                    return true;
                }
                
                Node* current = mRoot;
                Node* parent = nullptr;
                
                while (current) {
                    parent = current;
                    if (value < current->Value) {
                        current = current->Left;
                    } else if (current->Value < value) {
                        current = current->Right;
                    } else {
                        return false;  // Duplicate
                    }
                }
                
                Node* newNode = CreateNode(value, parent);
                if (value < parent->Value) {
                    parent->Left = newNode;
                } else {
                    parent->Right = newNode;
                }
                
                ++mSize;
                FixInsert(newNode);
                return true;
            }
            
            bool Contains(const T& value) const {
                Node* current = mRoot;
                while (current) {
                    if (value < current->Value) {
                        current = current->Left;
                    } else if (current->Value < value) {
                        current = current->Right;
                    } else {
                        return true;
                    }
                }
                return false;
            }
            
            Iterator Find(const T& value) {
                Node* current = mRoot;
                while (current) {
                    if (value < current->Value) {
                        current = current->Left;
                    } else if (current->Value < value) {
                        current = current->Right;
                    } else {
                        return Iterator(current, this);
                    }
                }
                return end();
            }
            
            // Note: Erase complet nécessite fix-up RB-Tree (complexe)
            // Version simplifiée ici
            bool Erase(const T& value) {
                // Simplified - full RB-Tree erase is complex
                // TODO: Implement full RB-Tree delete with fixup
                return false;
            }
        };
        
    
} // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ASSOCIATIVE_NKSET_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-05 22:26:13
// Creation Date: 2026-02-05 22:26:13
// ============================================================
