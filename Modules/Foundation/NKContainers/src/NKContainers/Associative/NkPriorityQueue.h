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
    namespace core {
        
        /**
         * @brief Priority queue - std::priority_queue equivalent
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
        template<typename T, typename Allocator = memory::NkAllocator>
        class NkPriorityQueue {
        public:
            using ValueType = T;
            using SizeType = usize;
            using Reference = T&;
            using ConstReference = const T&;
            
        private:
            NkVector<T, Allocator> mHeap;
            
            SizeType Parent(SizeType i) const { return (i - 1) / 2; }
            SizeType Left(SizeType i) const { return 2 * i + 1; }
            SizeType Right(SizeType i) const { return 2 * i + 2; }
            
            void HeapifyUp(SizeType i) {
                while (i > 0 && mHeap[Parent(i)] < mHeap[i]) {
                    traits::NkSwap(mHeap[i], mHeap[Parent(i)]);
                    i = Parent(i);
                }
            }
            
            void HeapifyDown(SizeType i) {
                SizeType size = mHeap.Size();
                SizeType largest = i;
                SizeType left = Left(i);
                SizeType right = Right(i);
                
                if (left < size && mHeap[largest] < mHeap[left]) {
                    largest = left;
                }
                
                if (right < size && mHeap[largest] < mHeap[right]) {
                    largest = right;
                }
                
                if (largest != i) {
                    traits::NkSwap(mHeap[i], mHeap[largest]);
                    HeapifyDown(largest);
                }
            }
            
        public:
            // Constructors
            NkPriorityQueue() {}
            
            NkPriorityQueue(NkInitializerList<T> init) {
                for (auto& val : init) {
                    Push(val);
                }
            }
            
            // Element access
            ConstReference Top() const {
                NK_ASSERT(!IsEmpty());
                return mHeap[0];
            }
            
            // Capacity
            bool IsEmpty() const NK_NOEXCEPT { return mHeap.IsEmpty(); }
            SizeType Size() const NK_NOEXCEPT { return mHeap.Size(); }
            
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
                NK_ASSERT(!IsEmpty());
                
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
            }
        };
        
        // Non-member functions
        template<typename T, typename Allocator>
        void NkSwap(NkPriorityQueue<T, Allocator>& lhs, NkPriorityQueue<T, Allocator>& rhs) NK_NOEXCEPT {
            lhs.Swap(rhs);
        }
        
    } // namespace core
} // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ASSOCIATIVE_NKPRIORITYQUEUE_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-05 22:26:13
// Creation Date: 2026-02-05 22:26:13
// ============================================================
