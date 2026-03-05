// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\Containers\Sequential\NkDoubleList.h
// DESCRIPTION: Doubly linked list - 100% STL-free
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_SEQUENTIAL_NKDOUBLELIST_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_SEQUENTIAL_NKDOUBLELIST_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKContainers/NkContainersExport.h"
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
         * @brief Doubly linked list (std::list equivalent)
         * 
         * Liste doublement chaînée. Chaque nœud pointe vers le précédent et le suivant.
         * Optimisée pour insertion/suppression O(1) en toute position.
         * Support iteration bidirectionnelle.
         * 
         * Complexité:
         * - PushFront/PushBack: O(1)
         * - PopFront/PopBack: O(1)
         * - Insert (à position): O(1)
         * - Erase (à position): O(1)
         * - Accès par index: O(n)
         * - Search: O(n)
         * 
         * @example
         * NkDoubleList<int> list = {1, 2, 3, 4, 5};
         * list.PushFront(0);
         * list.PushBack(6);
         * for (auto it = list.rbegin(); it != list.rend(); ++it) {
         *     printf("%d ", *it);
         * }
         */
        template<typename T, typename Allocator = memory::NkAllocator>
        class NkDoubleList {
        private:
            // ========================================
            // NODE STRUCTURE
            // ========================================
            
            struct Node {
                T value;
                Node* prev;
                Node* next;
                
                Node(const T& val) : value(val), prev(nullptr), next(nullptr) {}
                
                #if defined(NK_CPP11)
                template<typename... Args>
                Node(Args&&... args) 
                    : value(traits::NkForward<Args>(args)...)
                    , prev(nullptr)
                    , next(nullptr) {}
                #endif
            };
            
        public:
            // ========================================
            // ITERATOR
            // ========================================
            
            class Iterator {
            private:
                Node* mNode;
                friend class NkDoubleList;
                
            public:
                using DifferenceType = intptr;
                using ValueType = T;
                using Pointer = T*;
                using Reference = T&;
                using IteratorCategory = NkBidirectionalIteratorTag;
                
                Iterator() : mNode(nullptr) {}
                explicit Iterator(Node* node) : mNode(node) {}
                
                Reference operator*() const {
                    NK_ASSERT(mNode != nullptr);
                    return mNode->value;
                }
                
                Pointer operator->() const {
                    NK_ASSERT(mNode != nullptr);
                    return &(mNode->value);
                }
                
                Iterator& operator++() {
                    NK_ASSERT(mNode != nullptr);
                    mNode = mNode->next;
                    return *this;
                }
                
                Iterator operator++(int) {
                    Iterator tmp = *this;
                    ++(*this);
                    return tmp;
                }
                
                Iterator& operator--() {
                    NK_ASSERT(mNode != nullptr);
                    mNode = mNode->prev;
                    return *this;
                }
                
                Iterator operator--(int) {
                    Iterator tmp = *this;
                    --(*this);
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
                friend class NkDoubleList;
                
            public:
                using DifferenceType = intptr;
                using ValueType = T;
                using Pointer = const T*;
                using Reference = const T&;
                using IteratorCategory = NkBidirectionalIteratorTag;
                
                ConstIterator() : mNode(nullptr) {}
                explicit ConstIterator(const Node* node) : mNode(node) {}
                ConstIterator(const Iterator& it) : mNode(it.mNode) {}
                
                Reference operator*() const {
                    NK_ASSERT(mNode != nullptr);
                    return mNode->value;
                }
                
                Pointer operator->() const {
                    NK_ASSERT(mNode != nullptr);
                    return &(mNode->value);
                }
                
                ConstIterator& operator++() {
                    NK_ASSERT(mNode != nullptr);
                    mNode = mNode->next;
                    return *this;
                }
                
                ConstIterator operator++(int) {
                    ConstIterator tmp = *this;
                    ++(*this);
                    return tmp;
                }
                
                ConstIterator& operator--() {
                    NK_ASSERT(mNode != nullptr);
                    mNode = mNode->prev;
                    return *this;
                }
                
                ConstIterator operator--(int) {
                    ConstIterator tmp = *this;
                    --(*this);
                    return tmp;
                }
                
                bool operator==(const ConstIterator& other) const {
                    return mNode == other.mNode;
                }
                
                bool operator!=(const ConstIterator& other) const {
                    return mNode != other.mNode;
                }
            };
            
            using ValueType = T;
            using AllocatorType = Allocator;
            using SizeType = usize;
            using Reference = T&;
            using ConstReference = const T&;
            using ReverseIterator = NkReverseIterator<Iterator>;
            using ConstReverseIterator = NkReverseIterator<ConstIterator>;
            
        private:
            Node* mHead;
            Node* mTail;
            SizeType mSize;
            Allocator* mAllocator;
            
            // ========================================
            // INTERNAL HELPERS
            // ========================================
            
            Node* AllocateNode(const T& value) {
                Node* node = static_cast<Node*>(mAllocator->Allocate(sizeof(Node)));
                if (node) {
                    new (node) Node(value);
                }
                return node;
            }
            
            #if defined(NK_CPP11)
            template<typename... Args>
            Node* AllocateNode(Args&&... args) {
                Node* node = static_cast<Node*>(mAllocator->Allocate(sizeof(Node)));
                if (node) {
                    new (node) Node(traits::NkForward<Args>(args)...);
                }
                return node;
            }
            #endif
            
            void DeallocateNode(Node* node) {
                if (node) {
                    node->~Node();
                    mAllocator->Deallocate(node);
                }
            }
            
        public:
            // ========================================
            // CONSTRUCTORS
            // ========================================
            
            NkDoubleList()
                : mHead(nullptr)
                , mTail(nullptr)
                , mSize(0)
                , mAllocator(&memory::NkGetDefaultAllocator()) {
            }
            
            explicit NkDoubleList(Allocator* allocator)
                : mHead(nullptr)
                , mTail(nullptr)
                , mSize(0)
                , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator()) {
            }
            
            NkDoubleList(NkInitializerList<T> init)
                : mHead(nullptr)
                , mTail(nullptr)
                , mSize(0)
                , mAllocator(&memory::NkGetDefaultAllocator()) {
                for (auto& val : init) {
                    PushBack(val);
                }
            }
            
            NkDoubleList(const NkDoubleList& other)
                : mHead(nullptr)
                , mTail(nullptr)
                , mSize(0)
                , mAllocator(other.mAllocator) {
                for (auto& val : other) {
                    PushBack(val);
                }
            }
            
            #if defined(NK_CPP11)
            NkDoubleList(NkDoubleList&& other) NK_NOEXCEPT
                : mHead(other.mHead)
                , mTail(other.mTail)
                , mSize(other.mSize)
                , mAllocator(other.mAllocator) {
                other.mHead = nullptr;
                other.mTail = nullptr;
                other.mSize = 0;
            }
            #endif
            
            ~NkDoubleList() {
                Clear();
            }
            
            // ========================================
            // ASSIGNMENT
            // ========================================
            
            NkDoubleList& operator=(const NkDoubleList& other) {
                if (this != &other) {
                    Clear();
                    for (auto& val : other) {
                        PushBack(val);
                    }
                }
                return *this;
            }
            
            #if defined(NK_CPP11)
            NkDoubleList& operator=(NkDoubleList&& other) NK_NOEXCEPT {
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
            
            NkDoubleList& operator=(NkInitializerList<T> init) {
                Clear();
                for (auto& val : init) {
                    PushBack(val);
                }
                return *this;
            }
            
            // ========================================
            // ITERATORS
            // ========================================
            
            Iterator begin() { return Iterator(mHead); }
            ConstIterator begin() const { return ConstIterator(mHead); }
            ConstIterator cbegin() const { return ConstIterator(mHead); }
            
            Iterator end() { return Iterator(nullptr); }
            ConstIterator end() const { return ConstIterator(nullptr); }
            ConstIterator cend() const { return ConstIterator(nullptr); }
            
            ReverseIterator rbegin() { return ReverseIterator(Iterator(nullptr)); }
            ConstReverseIterator rbegin() const { return ConstReverseIterator(ConstIterator(nullptr)); }
            ConstReverseIterator crbegin() const { return ConstReverseIterator(ConstIterator(nullptr)); }
            
            ReverseIterator rend() { return ReverseIterator(Iterator(mHead)); }
            ConstReverseIterator rend() const { return ConstReverseIterator(ConstIterator(mHead)); }
            ConstReverseIterator crend() const { return ConstReverseIterator(ConstIterator(mHead)); }
            
            // ========================================
            // CAPACITY
            // ========================================
            
            bool IsEmpty() const NK_NOEXCEPT { return mSize == 0; }
            bool empty() const NK_NOEXCEPT { return mSize == 0; }
            
            SizeType Size() const NK_NOEXCEPT { return mSize; }
            SizeType size() const NK_NOEXCEPT { return mSize; }
            
            // ========================================
            // ELEMENT ACCESS
            // ========================================
            
            Reference Front() {
                NK_ASSERT(mHead != nullptr);
                return mHead->value;
            }
            
            ConstReference Front() const {
                NK_ASSERT(mHead != nullptr);
                return mHead->value;
            }
            
            Reference Back() {
                NK_ASSERT(mTail != nullptr);
                return mTail->value;
            }
            
            ConstReference Back() const {
                NK_ASSERT(mTail != nullptr);
                return mTail->value;
            }
            
            // ========================================
            // MODIFIERS
            // ========================================
            
            void Clear() {
                Node* current = mHead;
                while (current) {
                    Node* next = current->next;
                    DeallocateNode(current);
                    current = next;
                }
                mHead = nullptr;
                mTail = nullptr;
                mSize = 0;
            }
            
            void PushFront(const T& value) {
                Node* newNode = AllocateNode(value);
                if (!newNode) return;
                
                newNode->next = mHead;
                newNode->prev = nullptr;
                
                if (mHead) {
                    mHead->prev = newNode;
                }
                
                mHead = newNode;
                
                if (!mTail) {
                    mTail = newNode;
                }
                
                ++mSize;
            }
            
            void PushBack(const T& value) {
                Node* newNode = AllocateNode(value);
                if (!newNode) return;
                
                newNode->prev = mTail;
                newNode->next = nullptr;
                
                if (mTail) {
                    mTail->next = newNode;
                }
                
                mTail = newNode;
                
                if (!mHead) {
                    mHead = newNode;
                }
                
                ++mSize;
            }
            
            #if defined(NK_CPP11)
            void PushFront(T&& value) {
                Node* newNode = AllocateNode(traits::NkMove(value));
                if (!newNode) return;
                
                newNode->next = mHead;
                newNode->prev = nullptr;
                
                if (mHead) {
                    mHead->prev = newNode;
                }
                
                mHead = newNode;
                
                if (!mTail) {
                    mTail = newNode;
                }
                
                ++mSize;
            }
            
            void PushBack(T&& value) {
                Node* newNode = AllocateNode(traits::NkMove(value));
                if (!newNode) return;
                
                newNode->prev = mTail;
                newNode->next = nullptr;
                
                if (mTail) {
                    mTail->next = newNode;
                }
                
                mTail = newNode;
                
                if (!mHead) {
                    mHead = newNode;
                }
                
                ++mSize;
            }
            
            template<typename... Args>
            void EmplaceFront(Args&&... args) {
                Node* newNode = AllocateNode(traits::NkForward<Args>(args)...);
                if (!newNode) return;
                
                newNode->next = mHead;
                newNode->prev = nullptr;
                
                if (mHead) {
                    mHead->prev = newNode;
                }
                
                mHead = newNode;
                
                if (!mTail) {
                    mTail = newNode;
                }
                
                ++mSize;
            }
            
            template<typename... Args>
            void EmplaceBack(Args&&... args) {
                Node* newNode = AllocateNode(traits::NkForward<Args>(args)...);
                if (!newNode) return;
                
                newNode->prev = mTail;
                newNode->next = nullptr;
                
                if (mTail) {
                    mTail->next = newNode;
                }
                
                mTail = newNode;
                
                if (!mHead) {
                    mHead = newNode;
                }
                
                ++mSize;
            }
            #endif
            
            void PopFront() {
                NK_ASSERT(mHead != nullptr);
                
                Node* oldHead = mHead;
                mHead = mHead->next;
                
                if (mHead) {
                    mHead->prev = nullptr;
                } else {
                    mTail = nullptr;
                }
                
                DeallocateNode(oldHead);
                --mSize;
            }
            
            void PopBack() {
                NK_ASSERT(mTail != nullptr);
                
                Node* oldTail = mTail;
                mTail = mTail->prev;
                
                if (mTail) {
                    mTail->next = nullptr;
                } else {
                    mHead = nullptr;
                }
                
                DeallocateNode(oldTail);
                --mSize;
            }
            
            Iterator Insert(ConstIterator pos, const T& value) {
                if (!pos.mNode || pos.mNode == mHead) {
                    PushFront(value);
                    return begin();
                }
                
                Node* newNode = AllocateNode(value);
                if (!newNode) return end();
                
                Node* posNode = const_cast<Node*>(pos.mNode);
                Node* prevNode = posNode->prev;
                
                newNode->next = posNode;
                newNode->prev = prevNode;
                posNode->prev = newNode;
                
                if (prevNode) {
                    prevNode->next = newNode;
                }
                
                ++mSize;
                return Iterator(newNode);
            }
            
            Iterator Erase(ConstIterator pos) {
                NK_ASSERT(pos.mNode != nullptr);
                
                Node* toDelete = const_cast<Node*>(pos.mNode);
                Node* prevNode = toDelete->prev;
                Node* nextNode = toDelete->next;
                
                if (prevNode) {
                    prevNode->next = nextNode;
                } else {
                    mHead = nextNode;
                }
                
                if (nextNode) {
                    nextNode->prev = prevNode;
                } else {
                    mTail = prevNode;
                }
                
                DeallocateNode(toDelete);
                --mSize;
                
                return Iterator(nextNode);
            }
            
            void Reverse() {
                Node* current = mHead;
                mTail = mHead;
                
                while (current) {
                    Node* temp = current->next;
                    current->next = current->prev;
                    current->prev = temp;
                    
                    if (!temp) {
                        mHead = current;
                    }
                    
                    current = temp;
                }
            }
            
            void Swap(NkDoubleList& other) NK_NOEXCEPT {
                traits::NkSwap(mHead, other.mHead);
                traits::NkSwap(mTail, other.mTail);
                traits::NkSwap(mSize, other.mSize);
                traits::NkSwap(mAllocator, other.mAllocator);
            }
        };
        
        // ========================================
        // NON-MEMBER FUNCTIONS
        // ========================================
        
        template<typename T, typename Allocator>
        void NkSwap(NkDoubleList<T, Allocator>& lhs, NkDoubleList<T, Allocator>& rhs) NK_NOEXCEPT {
            lhs.Swap(rhs);
        }
        
    } // namespace core
} // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_SEQUENTIAL_NKDOUBLELIST_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-05 22:26:13
// Creation Date: 2026-02-05 22:26:13
// ============================================================