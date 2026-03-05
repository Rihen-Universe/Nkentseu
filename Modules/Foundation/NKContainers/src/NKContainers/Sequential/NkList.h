// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\Containers\Sequential\NkList.h
// DESCRIPTION: Singly linked list - 100% STL-free
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_SEQUENTIAL_NKLIST_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_SEQUENTIAL_NKLIST_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKContainers/NkContainersExport.h"
#include "NKCore/NkTraits.h"
#include "NKMemory/NkAllocator.h"
#include "NKMemory/NkMemoryFn.h"
#include "NKCore/Assert/NkAssert.h"
#include "NKContainers/Iterators/NkIterator.h"
#include "NKContainers/Iterators/NkInitializerList.h"

namespace nkentseu {
    namespace core {
        
        /**
         * @brief Singly linked list
         * 
         * Complexité:
         * - PushFront/PopFront: O(1)
         * - PushBack: O(1) avec tail pointer
         * - Accès: O(n)
         * 
         * @example
         * NkList<int> list = {1, 2, 3};
         * list.PushFront(0);
         * list.Reverse();
         */
        template<typename T, typename Allocator = memory::NkAllocator>
        class NkList {
        private:
            struct Node {
                T Data;
                Node* Next;
                
                #if defined(NK_CPP11)
                template<typename... Args>
                Node(Node* next, Args&&... args)
                    : Data(traits::NkForward<Args>(args)...)
                    , Next(next) {
                }
                #else
                Node(Node* next, const T& data)
                    : Data(data), Next(next) {
                }
                #endif
            };
            
        public:
            using ValueType = T;
            using SizeType = usize;
            using Reference = T&;
            using ConstReference = const T&;
            
            // Iterator
            class Iterator {
            private:
                Node* mNode;
                friend class NkList;
                
            public:
                using ValueType = T;
                using Reference = T&;
                using Pointer = T*;
                using DifferenceType = intptr;
                using IteratorCategory = NkForwardIteratorTag;
                
                Iterator() : mNode(nullptr) {}
                explicit Iterator(Node* node) : mNode(node) {}
                
                Reference operator*() const { return mNode->Data; }
                Pointer operator->() const { return &mNode->Data; }
                
                Iterator& operator++() {
                    mNode = mNode->Next;
                    return *this;
                }
                
                Iterator operator++(int) {
                    Iterator tmp = *this;
                    ++(*this);
                    return tmp;
                }
                
                bool operator==(const Iterator& other) const {
                    return mNode == other.mNode;
                }
                
                bool operator!=(const Iterator& other) const {
                    return mNode != other.mNode;
                }
            };
            
            class ConstIterator {
            private:
                const Node* mNode;
                friend class NkList;
                
            public:
                using ValueType = T;
                using Reference = const T&;
                using Pointer = const T*;
                using DifferenceType = intptr;
                using IteratorCategory = NkForwardIteratorTag;
                
                ConstIterator() : mNode(nullptr) {}
                explicit ConstIterator(const Node* node) : mNode(node) {}
                ConstIterator(const Iterator& it) : mNode(it.mNode) {}
                
                Reference operator*() const { return mNode->Data; }
                Pointer operator->() const { return &mNode->Data; }
                
                ConstIterator& operator++() {
                    mNode = mNode->Next;
                    return *this;
                }
                
                ConstIterator operator++(int) {
                    ConstIterator tmp = *this;
                    ++(*this);
                    return tmp;
                }
                
                bool operator==(const ConstIterator& other) const {
                    return mNode == other.mNode;
                }
                
                bool operator!=(const ConstIterator& other) const {
                    return mNode != other.mNode;
                }
            };
            
        private:
            Node* mHead;
            Node* mTail;
            SizeType mSize;
            Allocator* mAllocator;
            
            #if defined(NK_CPP11)
            template<typename... Args>
            Node* CreateNode(Node* next, Args&&... args) {
                Node* node = static_cast<Node*>(mAllocator->Allocate(sizeof(Node)));
                if (node) {
                    new (node) Node(next, traits::NkForward<Args>(args)...);
                }
                return node;
            }
            #else
            Node* CreateNode(Node* next, const T& data) {
                Node* node = static_cast<Node*>(mAllocator->Allocate(sizeof(Node)));
                if (node) {
                    new (node) Node(next, data);
                }
                return node;
            }
            #endif
            
            void DestroyNode(Node* node) {
                node->~Node();
                mAllocator->Deallocate(node);
            }
            
        public:
            // Constructors
            NkList()
                : mHead(nullptr), mTail(nullptr), mSize(0)
                , mAllocator(&memory::NkGetDefaultAllocator()) {
            }
            
            NkList(NkInitializerList<T> init)
                : mHead(nullptr), mTail(nullptr), mSize(0)
                , mAllocator(&memory::NkGetDefaultAllocator()) {
                for (auto& val : init) {
                    PushBack(val);
                }
            }
            
            NkList(const NkList& other)
                : mHead(nullptr), mTail(nullptr), mSize(0)
                , mAllocator(other.mAllocator) {
                for (auto it = other.begin(); it != other.end(); ++it) {
                    PushBack(*it);
                }
            }
            
            #if defined(NK_CPP11)
            NkList(NkList&& other) NK_NOEXCEPT
                : mHead(other.mHead), mTail(other.mTail)
                , mSize(other.mSize), mAllocator(other.mAllocator) {
                other.mHead = nullptr;
                other.mTail = nullptr;
                other.mSize = 0;
            }
            #endif
            
            ~NkList() {
                Clear();
            }
            
            // Assignment
            NkList& operator=(const NkList& other) {
                if (this != &other) {
                    Clear();
                    for (auto it = other.begin(); it != other.end(); ++it) {
                        PushBack(*it);
                    }
                }
                return *this;
            }
            
            #if defined(NK_CPP11)
            NkList& operator=(NkList&& other) NK_NOEXCEPT {
                if (this != &other) {
                    Clear();
                    mHead = other.mHead;
                    mTail = other.mTail;
                    mSize = other.mSize;
                    mAllocator = other.mAllocator;
                    other.mHead = nullptr;
                    other.mTail = nullptr;
                    other.mSize = 0;
                }
                return *this;
            }
            #endif
            
            // Element access
            Reference Front() { NK_ASSERT(mHead); return mHead->Data; }
            ConstReference Front() const { NK_ASSERT(mHead); return mHead->Data; }
            Reference Back() { NK_ASSERT(mTail); return mTail->Data; }
            ConstReference Back() const { NK_ASSERT(mTail); return mTail->Data; }
            
            // Iterators
            Iterator begin() { return Iterator(mHead); }
            ConstIterator begin() const { return ConstIterator(mHead); }
            ConstIterator cbegin() const { return ConstIterator(mHead); }
            
            Iterator end() { return Iterator(nullptr); }
            ConstIterator end() const { return ConstIterator(nullptr); }
            ConstIterator cend() const { return ConstIterator(nullptr); }
            
            // Capacity
            bool IsEmpty() const NK_NOEXCEPT { return mSize == 0; }
            SizeType Size() const NK_NOEXCEPT { return mSize; }
            
            // Modifiers
            void Clear() {
                while (mHead) {
                    Node* next = mHead->Next;
                    DestroyNode(mHead);
                    mHead = next;
                }
                mTail = nullptr;
                mSize = 0;
            }
            
            void PushFront(const T& value) {
                Node* node = CreateNode(mHead, value);
                mHead = node;
                if (!mTail) mTail = node;
                ++mSize;
            }
            
            #if defined(NK_CPP11)
            void PushFront(T&& value) {
                Node* node = CreateNode(mHead, traits::NkMove(value));
                mHead = node;
                if (!mTail) mTail = node;
                ++mSize;
            }
            #endif
            
            void PopFront() {
                NK_ASSERT(mHead);
                Node* next = mHead->Next;
                DestroyNode(mHead);
                mHead = next;
                if (!mHead) mTail = nullptr;
                --mSize;
            }
            
            void PushBack(const T& value) {
                Node* node = CreateNode(nullptr, value);
                if (mTail) {
                    mTail->Next = node;
                } else {
                    mHead = node;
                }
                mTail = node;
                ++mSize;
            }
            
            #if defined(NK_CPP11)
            void PushBack(T&& value) {
                Node* node = CreateNode(nullptr, traits::NkMove(value));
                if (mTail) {
                    mTail->Next = node;
                } else {
                    mHead = node;
                }
                mTail = node;
                ++mSize;
            }
            #endif
            
            void Reverse() {
                if (!mHead || !mHead->Next) return;
                
                Node* prev = nullptr;
                Node* current = mHead;
                mTail = mHead;
                
                while (current) {
                    Node* next = current->Next;
                    current->Next = prev;
                    prev = current;
                    current = next;
                }
                
                mHead = prev;
            }
            
            void Swap(NkList& other) NK_NOEXCEPT {
                traits::NkSwap(mHead, other.mHead);
                traits::NkSwap(mTail, other.mTail);
                traits::NkSwap(mSize, other.mSize);
                traits::NkSwap(mAllocator, other.mAllocator);
            }
        };
        
        template<typename T, typename Allocator>
        void NkSwap(NkList<T, Allocator>& lhs, NkList<T, Allocator>& rhs) NK_NOEXCEPT {
            lhs.Swap(rhs);
        }
        
    } // namespace core
} // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_SEQUENTIAL_NKLIST_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-05 22:26:13
// Creation Date: 2026-02-05 22:26:13
// ============================================================