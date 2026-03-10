// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\Containers\Associative\NkMap.h
// DESCRIPTION: Ordered map - Red-Black Tree key-value pairs
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ASSOCIATIVE_NKMAP_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ASSOCIATIVE_NKMAP_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKContainers/NkContainersExport.h"
#include "NKCore/NkTraits.h"
#include "NKMemory/NkAllocator.h"
#include "NKMemory/NkFunction.h"
#include "NKCore/Assert/NkAssert.h"
#include "NKContainers/Heterogeneous/NkPair.h"
#include "NKContainers/Iterators/NkIterator.h"
#include "NKContainers/Iterators/NkInitializerList.h"

namespace nkentseu {
    
        
        /**
         * @brief Ordered map - std::map equivalent
         * 
         * Map ordonné implémenté avec Red-Black Tree.
         * Les paires key-value sont triées par clé.
         * 
         * Complexité:
         * - Insert/Find/Erase: O(log n)
         * - Iteration: O(n) en ordre trié
         * 
         * @example
         * NkMap<int, NkString> map = {
         *     {3, "three"},
         *     {1, "one"},
         *     {4, "four"}
         * };
         * // Stored sorted by key: {1:"one", 3:"three", 4:"four"}
         * 
         * map[2] = "two";
         * if (map.Contains(3)) { }
         */
        template<typename Key, typename Value, typename Allocator = memory::NkAllocator>
        class NkMap {
        private:
            enum Color { RED, BLACK };
            
            struct Node {
                NkPair<const Key, Value> Data;
                Node* Left;
                Node* Right;
                Node* Parent;
                Color NodeColor;
                
                #if defined(NK_CPP11)
                template<typename K, typename V>
                Node(K&& key, V&& value, Node* parent = nullptr)
                    : Data(traits::NkForward<K>(key), traits::NkForward<V>(value))
                    , Left(nullptr)
                    , Right(nullptr)
                    , Parent(parent)
                    , NodeColor(RED) {
                }
                #else
                Node(const Key& key, const Value& value, Node* parent = nullptr)
                    : Data(key, value)
                    , Left(nullptr)
                    , Right(nullptr)
                    , Parent(parent)
                    , NodeColor(RED) {
                }
                #endif
            };
            
        public:
            using KeyType = Key;
            using MappedType = Value;
            using ValueType = NkPair<const Key, Value>;
            using SizeType = usize;
            using Reference = ValueType&;
            using ConstReference = const ValueType&;
            
            class Iterator {
            private:
                Node* mNode;
                const NkMap* mMap;
                friend class NkMap;
                
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
                using ValueType = NkPair<const Key, Value>;
                using Reference = ValueType&;
                using Pointer = ValueType*;
                using DifferenceType = intptr;
                using IteratorCategory = NkBidirectionalIteratorTag;
                
                Iterator() : mNode(nullptr), mMap(nullptr) {}
                Iterator(Node* node, const NkMap* map) : mNode(node), mMap(map) {}
                
                Reference operator*() const { return mNode->Data; }
                Pointer operator->() const { return &mNode->Data; }
                
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
            
            Node* CreateNode(const Key& key, const Value& value, Node* parent = nullptr) {
                Node* node = static_cast<Node*>(mAllocator->Allocate(sizeof(Node)));
                new (node) Node(key, value, parent);
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

            Node* FindNode(const Key& key) const {
                Node* current = mRoot;
                while (current) {
                    if (key < current->Data.First) {
                        current = current->Left;
                    } else if (current->Data.First < key) {
                        current = current->Right;
                    } else {
                        return current;
                    }
                }
                return nullptr;
            }
            
        public:
            // Constructors
            explicit NkMap(Allocator* allocator = nullptr)
                : mRoot(nullptr), mSize(0)
                , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator()) {
            }
            
            NkMap(NkInitializerList<ValueType> init, Allocator* allocator = nullptr)
                : mRoot(nullptr), mSize(0)
                , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator()) {
                for (auto& pair : init) {
                    Insert(pair.First, pair.Second);
                }
            }
            
            ~NkMap() {
                Clear();
            }
            
            // Iterators
            Iterator begin() { return Iterator(FindMin(mRoot), this); }
            ConstIterator begin() const { return ConstIterator(FindMin(mRoot), this); }
            Iterator end() { return Iterator(nullptr, this); }
            ConstIterator end() const { return ConstIterator(nullptr, this); }
            Iterator Begin() { return begin(); }
            ConstIterator Begin() const { return begin(); }
            Iterator End() { return end(); }
            ConstIterator End() const { return end(); }
            
            // Capacity
            bool Empty() const NK_NOEXCEPT { return mSize == 0; }
            SizeType Size() const NK_NOEXCEPT { return mSize; }
            
            // Modifiers
            void Clear() {
                DestroyTree(mRoot);
                mRoot = nullptr;
                mSize = 0;
            }
            
            void Insert(const Key& key, const Value& value) {
                if (!mRoot) {
                    mRoot = CreateNode(key, value);
                    mRoot->NodeColor = BLACK;
                    ++mSize;
                    return;
                }
                
                Node* current = mRoot;
                Node* parent = nullptr;
                
                while (current) {
                    parent = current;
                    if (key < current->Data.First) {
                        current = current->Left;
                    } else if (current->Data.First < key) {
                        current = current->Right;
                    } else {
                        current->Data.Second = value;  // Update existing
                        return;
                    }
                }
                
                Node* newNode = CreateNode(key, value, parent);
                if (key < parent->Data.First) {
                    parent->Left = newNode;
                } else {
                    parent->Right = newNode;
                }
                
                ++mSize;
                FixInsert(newNode);
            }
            
            bool Contains(const Key& key) const {
                return FindNode(key) != nullptr;
            }
            
            Value* Find(const Key& key) {
                Node* node = FindNode(key);
                return node ? &node->Data.Second : nullptr;
            }
            
            const Value* Find(const Key& key) const {
                Node* node = FindNode(key);
                return node ? &node->Data.Second : nullptr;
            }

            Iterator FindIterator(const Key& key) {
                return Iterator(FindNode(key), this);
            }

            ConstIterator FindIterator(const Key& key) const {
                return ConstIterator(FindNode(key), this);
            }

            Value& At(const Key& key) {
                Node* node = FindNode(key);
                NK_ASSERT(node != nullptr);
                return node->Data.Second;
            }

            const Value& At(const Key& key) const {
                Node* node = FindNode(key);
                NK_ASSERT(node != nullptr);
                return node->Data.Second;
            }

            bool TryGet(const Key& key, Value& outValue) const {
                const Value* value = Find(key);
                if (!value) {
                    return false;
                }
                outValue = *value;
                return true;
            }

            bool InsertOrAssign(const Key& key, const Value& value) {
                Value* current = Find(key);
                if (current) {
                    *current = value;
                    return false;
                }
                Insert(key, value);
                return true;
            }
            
            Value& operator[](const Key& key) {
                Value* val = Find(key);
                if (val) return *val;
                
                Insert(key, Value());
                return *Find(key);
            }
        };
        
    
} // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ASSOCIATIVE_NKMAP_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-05 22:26:13
// Creation Date: 2026-02-05 22:26:13
// ============================================================
