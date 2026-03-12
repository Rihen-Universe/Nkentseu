// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\Containers\Associative\NkPriorityQueue.h
// DESCRIPTION: Priority queue - Binary heap implementation
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ASSOCIATIVE_NKPRIORITYQUEUE_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ASSOCIATIVE_NKPRIORITYQUEUE_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKContainers/NkContainersExport.h"
#include "NKCore/NkTraits.h"
#include "NKCore/Assert/NkAssert.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
    
        template<typename T>
        struct NkPriorityLess {
            bool operator()(const T& lhs, const T& rhs) const {
                return lhs < rhs;
            }
        };
        
        
        /**
         * @brief Priority queue - STL::priority_queue equivalent
         * 
         * Implémentation avec binary heap (max-heap par défaut).
         * L'élément de plus haute priorité est toujours en tête.
         * 
         * Complexité:
         * - Push: O(log n)
         * - Pop: O(log n)
         * - Top: O(1)
         * 
         * @example
         * NkPriorityQueue<int> pq;
         * pq.Push(3);
         * pq.Push(1);
         * pq.Push(4);
         * int top = pq.Top();  // 4 (max)
         * pq.Pop();            // Remove 4
         */
        template<typename T,
                 typename Allocator = memory::NkAllocator,
                 typename Compare = NkPriorityLess<T>>
        class NkPriorityQueue {
        public:
            using ValueType = T;
            using SizeType = usize;
            using Reference = T&;
            using ConstReference = const T&;
            
        private:
            Allocator* mAllocator;
            NkVector<T, Allocator> mHeap;
            Compare mCompare;
            
            SizeType Parent(SizeType i) const { return (i - 1) / 2; }
            SizeType Left(SizeType i) const { return 2 * i + 1; }
            SizeType Right(SizeType i) const { return 2 * i + 2; }
            
            void HeapifyUp(SizeType i) {
                while (i > 0 && mCompare(mHeap[Parent(i)], mHeap[i])) {
                    traits::NkSwap(mHeap[i], mHeap[Parent(i)]);
                    i = Parent(i);
                }
            }
            
            void HeapifyDown(SizeType i) {
                SizeType size = mHeap.Size();
                SizeType largest = i;
                SizeType left = Left(i);
                SizeType right = Right(i);
                
                if (left < size && mCompare(mHeap[largest], mHeap[left])) {
                    largest = left;
                }
                
                if (right < size && mCompare(mHeap[largest], mHeap[right])) {
                    largest = right;
                }
                
                if (largest != i) {
                    traits::NkSwap(mHeap[i], mHeap[largest]);
                    HeapifyDown(largest);
                }
            }
            
        public:
            // Constructors
            NkPriorityQueue()
                : mAllocator(&memory::NkGetDefaultAllocator())
                , mHeap(mAllocator)
                , mCompare() {
            }
            
            explicit NkPriorityQueue(Allocator* allocator)
                : mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator())
                , mHeap(mAllocator)
                , mCompare() {
            }
            
            NkPriorityQueue(NkInitializerList<T> init, Allocator* allocator = nullptr)
                : mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator())
                , mHeap(mAllocator)
                , mCompare() {
                for (auto& val : init) {
                    Push(val);
                }
            }
            
            // Element access
            ConstReference Top() const {
                NK_ASSERT(!Empty());
                return mHeap[0];
            }
            
            // Capacity
            bool Empty() const NK_NOEXCEPT { return mHeap.Empty(); }
            SizeType Size() const NK_NOEXCEPT { return mHeap.Size(); }
            Allocator* GetAllocator() const NK_NOEXCEPT { return mAllocator; }
            
            // Modifiers
            void Push(const T& value) {
                mHeap.PushBack(value);
                HeapifyUp(mHeap.Size() - 1);
            }
            
            #if defined(NK_CPP11)
            void Push(T&& value) {
                mHeap.PushBack(traits::NkMove(value));
                HeapifyUp(mHeap.Size() - 1);
            }
            
            template<typename... Args>
            void Emplace(Args&&... args) {
                mHeap.EmplaceBack(traits::NkForward<Args>(args)...);
                HeapifyUp(mHeap.Size() - 1);
            }
            #endif
            
            void Pop() {
                NK_ASSERT(!Empty());
                
                if (mHeap.Size() == 1) {
                    mHeap.PopBack();
                    return;
                }
                
                mHeap[0] = mHeap[mHeap.Size() - 1];
                mHeap.PopBack();
                HeapifyDown(0);
            }
            
            void Clear() {
                mHeap.Clear();
            }
            
            void Swap(NkPriorityQueue& other) NK_NOEXCEPT {
                mHeap.Swap(other.mHeap);
                traits::NkSwap(mAllocator, other.mAllocator);
                traits::NkSwap(mCompare, other.mCompare);
            }
        };
        
        // Non-member functions
        template<typename T, typename Allocator, typename Compare>
        void NkSwap(NkPriorityQueue<T, Allocator, Compare>& lhs,
                    NkPriorityQueue<T, Allocator, Compare>& rhs) NK_NOEXCEPT {
            lhs.Swap(rhs);
        }
        
    
} // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ASSOCIATIVE_NKPRIORITYQUEUE_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-05 22:26:13
// Creation Date: 2026-02-05 22:26:13
// ============================================================
