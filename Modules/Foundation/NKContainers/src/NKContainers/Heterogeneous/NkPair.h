// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\Containers\Heterogeneous\NkPair.h
// DESCRIPTION: Pair container - 100% STL-free
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_HETEROGENEOUS_NKPAIR_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_HETEROGENEOUS_NKPAIR_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKContainers/NkContainersExport.h"
#include "NKCore/NkTraits.h"

namespace nkentseu {
    namespace core {
        
        /**
         * @brief Pair container - holds two values
         * 
         * Implémentation personnalisée de std::pair sans aucune dépendance STL.
         * Utilisé massivement dans Map, HashMap, etc.
         * 
         * @example
         * NkPair<int, NkString> p1(42, "Hello");
         * auto p2 = NkMakePair(10, 3.14f);
         * 
         * int key = p1.First;
         * NkString value = p1.Second;
         */
        template<typename T1, typename T2>
        struct NkPair {
            using FirstType = T1;
            using SecondType = T2;
            
            T1 First;
            T2 Second;
            
            // ========================================
            // CONSTRUCTORS
            // ========================================
            
            /**
             * @brief Default constructor
             */
            NK_CONSTEXPR NkPair()
                : First()
                , Second() {
            }
            
            /**
             * @brief Constructor with values
             */
            NK_CONSTEXPR NkPair(const T1& first, const T2& second)
                : First(first)
                , Second(second) {
            }
            
            /**
             * @brief Copy constructor
             */
            NK_CONSTEXPR NkPair(const NkPair& other)
                : First(other.First)
                , Second(other.Second) {
            }
            
            /**
             * @brief Template copy constructor (conversion)
             */
            template<typename U1, typename U2>
            NK_CONSTEXPR NkPair(const NkPair<U1, U2>& other)
                : First(other.First)
                , Second(other.Second) {
            }
            
            #if defined(NK_CPP11)
            /**
             * @brief Move constructor (C++11)
             */
            NK_CONSTEXPR NkPair(NkPair&& other) NK_NOEXCEPT
                : First(traits::NkMove(other.First))
                , Second(traits::NkMove(other.Second)) {
            }
            
            /**
             * @brief Template move constructor (C++11)
             */
            template<typename U1, typename U2>
            NK_CONSTEXPR NkPair(NkPair<U1, U2>&& other) NK_NOEXCEPT
                : First(traits::NkForward<U1>(other.First))
                , Second(traits::NkForward<U2>(other.Second)) {
            }
            
            /**
             * @brief Perfect forwarding constructor (C++11)
             */
            template<typename U1, typename U2>
            NK_CONSTEXPR NkPair(U1&& first, U2&& second)
                : First(traits::NkForward<U1>(first))
                , Second(traits::NkForward<U2>(second)) {
            }
            #endif
            
            // ========================================
            // ASSIGNMENT OPERATORS
            // ========================================
            
            /**
             * @brief Copy assignment
             */
            NkPair& operator=(const NkPair& other) {
                if (this != &other) {
                    First = other.First;
                    Second = other.Second;
                }
                return *this;
            }
            
            /**
             * @brief Template copy assignment
             */
            template<typename U1, typename U2>
            NkPair& operator=(const NkPair<U1, U2>& other) {
                First = other.First;
                Second = other.Second;
                return *this;
            }
            
            #if defined(NK_CPP11)
            /**
             * @brief Move assignment (C++11)
             */
            NkPair& operator=(NkPair&& other) NK_NOEXCEPT {
                First = traits::NkMove(other.First);
                Second = traits::NkMove(other.Second);
                return *this;
            }
            
            /**
             * @brief Template move assignment (C++11)
             */
            template<typename U1, typename U2>
            NkPair& operator=(NkPair<U1, U2>&& other) NK_NOEXCEPT {
                First = traits::NkForward<U1>(other.First);
                Second = traits::NkForward<U2>(other.Second);
                return *this;
            }
            #endif
            
            // ========================================
            // UTILITIES
            // ========================================
            
            /**
             * @brief Swap with another pair
             */
            void Swap(NkPair& other) NK_NOEXCEPT {
                traits::NkSwap(First, other.First);
                traits::NkSwap(Second, other.Second);
            }
        };
        
        // ========================================
        // COMPARISON OPERATORS
        // ========================================
        
        /**
         * @brief Equality comparison
         */
        template<typename T1, typename T2>
        NK_CONSTEXPR bool operator==(const NkPair<T1, T2>& lhs, const NkPair<T1, T2>& rhs) {
            return lhs.First == rhs.First && lhs.Second == rhs.Second;
        }
        
        /**
         * @brief Inequality comparison
         */
        template<typename T1, typename T2>
        NK_CONSTEXPR bool operator!=(const NkPair<T1, T2>& lhs, const NkPair<T1, T2>& rhs) {
            return !(lhs == rhs);
        }
        
        /**
         * @brief Less than comparison (lexicographic)
         */
        template<typename T1, typename T2>
        NK_CONSTEXPR bool operator<(const NkPair<T1, T2>& lhs, const NkPair<T1, T2>& rhs) {
            return lhs.First < rhs.First || 
                   (!(rhs.First < lhs.First) && lhs.Second < rhs.Second);
        }
        
        /**
         * @brief Less than or equal comparison
         */
        template<typename T1, typename T2>
        NK_CONSTEXPR bool operator<=(const NkPair<T1, T2>& lhs, const NkPair<T1, T2>& rhs) {
            return !(rhs < lhs);
        }
        
        /**
         * @brief Greater than comparison
         */
        template<typename T1, typename T2>
        NK_CONSTEXPR bool operator>(const NkPair<T1, T2>& lhs, const NkPair<T1, T2>& rhs) {
            return rhs < lhs;
        }
        
        /**
         * @brief Greater than or equal comparison
         */
        template<typename T1, typename T2>
        NK_CONSTEXPR bool operator>=(const NkPair<T1, T2>& lhs, const NkPair<T1, T2>& rhs) {
            return !(lhs < rhs);
        }
        
        // ========================================
        // HELPER FUNCTIONS
        // ========================================
        
        /**
         * @brief Make pair - template type deduction
         */
        template<typename T1, typename T2>
        NK_CONSTEXPR NkPair<T1, T2> NkMakePair(const T1& first, const T2& second) {
            return NkPair<T1, T2>(first, second);
        }
        
        #if defined(NK_CPP11)
        /**
         * @brief Make pair with perfect forwarding (C++11)
         */
        template<typename T1, typename T2>
        NK_CONSTEXPR NkPair<traits::NkDecay_t<T1>, 
                           traits::NkDecay_t<T2>>
        NkMakePair(T1&& first, T2&& second) {
            using FirstType = traits::NkDecay_t<T1>;
            using SecondType = traits::NkDecay_t<T2>;
            return NkPair<FirstType, SecondType>(
                traits::NkForward<T1>(first),
                traits::NkForward<T2>(second)
            );
        }
        #endif
        
        /**
         * @brief Swap two pairs
         */
        template<typename T1, typename T2>
        void NkSwap(NkPair<T1, T2>& lhs, NkPair<T1, T2>& rhs) NK_NOEXCEPT {
            lhs.Swap(rhs);
        }
        
        // ========================================
        // ELEMENT ACCESS BY INDEX
        // ========================================
        
        /**
         * @brief Get first element
         */
        template<typename T1, typename T2>
        NK_CONSTEXPR T1& NkGetFirst(NkPair<T1, T2>& p) NK_NOEXCEPT {
            return p.First;
        }
        
        template<typename T1, typename T2>
        NK_CONSTEXPR const T1& NkGetFirst(const NkPair<T1, T2>& p) NK_NOEXCEPT {
            return p.First;
        }
        
        /**
         * @brief Get second element
         */
        template<typename T1, typename T2>
        NK_CONSTEXPR T2& NkGetSecond(NkPair<T1, T2>& p) NK_NOEXCEPT {
            return p.Second;
        }
        
        template<typename T1, typename T2>
        NK_CONSTEXPR const T2& NkGetSecond(const NkPair<T1, T2>& p) NK_NOEXCEPT {
            return p.Second;
        }
        
        #if defined(NK_CPP11)
        /**
         * @brief Get by index (template version)
         */
        template<usize Index, typename T1, typename T2>
        struct NkPairElement;
        
        template<typename T1, typename T2>
        struct NkPairElement<0, T1, T2> {
            using Type = T1;
            static NK_CONSTEXPR T1& Get(NkPair<T1, T2>& p) NK_NOEXCEPT { return p.First; }
            static NK_CONSTEXPR const T1& Get(const NkPair<T1, T2>& p) NK_NOEXCEPT { return p.First; }
        };
        
        template<typename T1, typename T2>
        struct NkPairElement<1, T1, T2> {
            using Type = T2;
            static NK_CONSTEXPR T2& Get(NkPair<T1, T2>& p) NK_NOEXCEPT { return p.Second; }
            static NK_CONSTEXPR const T2& Get(const NkPair<T1, T2>& p) NK_NOEXCEPT { return p.Second; }
        };
        
        /**
         * @brief Get element by index
         */
        template<usize Index, typename T1, typename T2>
        NK_CONSTEXPR typename NkPairElement<Index, T1, T2>::Type&
        NkGet(NkPair<T1, T2>& p) NK_NOEXCEPT {
            static_assert(Index < 2, "Index out of bounds for NkPair");
            return NkPairElement<Index, T1, T2>::Get(p);
        }
        
        template<usize Index, typename T1, typename T2>
        NK_CONSTEXPR const typename NkPairElement<Index, T1, T2>::Type&
        NkGet(const NkPair<T1, T2>& p) NK_NOEXCEPT {
            static_assert(Index < 2, "Index out of bounds for NkPair");
            return NkPairElement<Index, T1, T2>::Get(p);
        }
        
        template<usize Index, typename T1, typename T2>
        NK_CONSTEXPR typename NkPairElement<Index, T1, T2>::Type&&
        NkGet(NkPair<T1, T2>&& p) NK_NOEXCEPT {
            static_assert(Index < 2, "Index out of bounds for NkPair");
            return traits::NkMove(NkPairElement<Index, T1, T2>::Get(p));
        }
        #endif
        
    } // namespace core
} // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_HETEROGENEOUS_NKPAIR_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-09
// ============================================================
