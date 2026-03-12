// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\Containers\Adapters\NkStack.h
// DESCRIPTION: Stack adapter - LIFO (Last In First Out)
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ADAPTERS_NKSTACK_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ADAPTERS_NKSTACK_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKContainers/NkContainersExport.h"
#include "NKCore/NkTraits.h"
#include "NKCore/Assert/NkAssert.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
    
        
        /**
         * @brief Stack adapter - STL::stack equivalent
         * 
         * LIFO (Last In First Out).
         * Wrapper autour d'un conteneur (Vector par défaut).
         * 
         * Complexité:
         * - Push: O(1) amortized
         * - Pop: O(1)
         * - Top: O(1)
         * 
         * @example
         * NkStack<int> stack;
         * stack.Push(1);
         * stack.Push(2);
         * stack.Push(3);
         * int top = stack.Top();  // 3
         * stack.Pop();            // Remove 3
         */
        template<typename T, typename Container = NkVector<T>>
        class NkStack {
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
            NkStack() {}
            
            explicit NkStack(const Container& cont) : mContainer(cont) {}
            
            #if defined(NK_CPP11)
            explicit NkStack(Container&& cont) : mContainer(traits::NkMove(cont)) {}
            #endif
            
            // Element access
            Reference Top() {
                NK_ASSERT(!Empty());
                return mContainer.Back();
            }
            
            ConstReference Top() const {
                NK_ASSERT(!Empty());
                return mContainer.Back();
            }
            
            // Capacity
            bool Empty() const NK_NOEXCEPT { return mContainer.Empty(); }
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
                NK_ASSERT(!Empty());
                mContainer.PopBack();
            }
            
            void Swap(NkStack& other) NK_NOEXCEPT {
                mContainer.Swap(other.mContainer);
            }
        };
        
        // Non-member functions
        template<typename T, typename Container>
        void NkSwap(NkStack<T, Container>& lhs, NkStack<T, Container>& rhs) NK_NOEXCEPT {
            lhs.Swap(rhs);
        }
        
        template<typename T, typename Container>
        bool operator==(const NkStack<T, Container>& lhs, const NkStack<T, Container>& rhs) {
            return lhs.mContainer == rhs.mContainer;
        }
        
        template<typename T, typename Container>
        bool operator!=(const NkStack<T, Container>& lhs, const NkStack<T, Container>& rhs) {
            return !(lhs == rhs);
        }
        
    
} // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ADAPTERS_NKSTACK_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// ============================================================
