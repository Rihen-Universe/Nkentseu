// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\Containers\Adapters\NkQueue.h
// DESCRIPTION: Queue adapter - FIFO (First In First Out)
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ADAPTERS_NKQUEUE_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ADAPTERS_NKQUEUE_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKContainers/NkContainersExport.h"
#include "NKCore/NkTraits.h"
#include "NKCore/Assert/NkAssert.h"
#include "NKContainers/Sequential/NkDeque.h"

namespace nkentseu {
    namespace core {
        
        /**
         * @brief Queue adapter - std::queue equivalent
         * 
         * FIFO (First In First Out).
         * Wrapper autour d'un conteneur (Deque par défaut).
         * 
         * Complexité:
         * - Push: O(1)
         * - Pop: O(1)
         * - Front/Back: O(1)
         * 
         * @example
         * NkQueue<int> queue;
         * queue.Push(1);
         * queue.Push(2);
         * queue.Push(3);
         * int front = queue.Front();  // 1
         * queue.Pop();                // Remove 1
         */
        template<typename T, typename Container = NkDeque<T>>
        class NkQueue {
        public:
            using ValueType = T;
            using SizeType = usize;
            using Reference = T&;
            using ConstReference = const T&;
            using ContainerType = Container;
            
        private:
            Container mContainer;
            
        public:
            // Constructors
            NkQueue() {}
            
            explicit NkQueue(const Container& cont) : mContainer(cont) {}
            
            #if defined(NK_CPP11)
            explicit NkQueue(Container&& cont) : mContainer(traits::NkMove(cont)) {}
            #endif
            
            // Element access
            Reference Front() {
                NK_ASSERT(!IsEmpty());
                return mContainer.Front();
            }
            
            ConstReference Front() const {
                NK_ASSERT(!IsEmpty());
                return mContainer.Front();
            }
            
            Reference Back() {
                NK_ASSERT(!IsEmpty());
                return mContainer.Back();
            }
            
            ConstReference Back() const {
                NK_ASSERT(!IsEmpty());
                return mContainer.Back();
            }
            
            // Capacity
            bool IsEmpty() const NK_NOEXCEPT { return mContainer.IsEmpty(); }
            SizeType Size() const NK_NOEXCEPT { return mContainer.Size(); }
            
            // Modifiers
            void Push(const T& value) {
                mContainer.PushBack(value);
            }
            
            #if defined(NK_CPP11)
            void Push(T&& value) {
                mContainer.PushBack(traits::NkMove(value));
            }
            
            template<typename... Args>
            void Emplace(Args&&... args) {
                mContainer.EmplaceBack(traits::NkForward<Args>(args)...);
            }
            #endif
            
            void Pop() {
                NK_ASSERT(!IsEmpty());
                mContainer.PopFront();
            }
            
            void Swap(NkQueue& other) NK_NOEXCEPT {
                mContainer.Swap(other.mContainer);
            }
        };
        
        // Non-member functions
        template<typename T, typename Container>
        void NkSwap(NkQueue<T, Container>& lhs, NkQueue<T, Container>& rhs) NK_NOEXCEPT {
            lhs.Swap(rhs);
        }
        
    } // namespace core
} // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ADAPTERS_NKQUEUE_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// ============================================================
