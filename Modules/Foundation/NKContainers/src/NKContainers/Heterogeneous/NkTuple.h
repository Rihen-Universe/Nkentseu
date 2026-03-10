// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\Containers\Heterogeneous\NkTuple.h
// DESCRIPTION: Tuple - Fixed-size collection of heterogeneous values
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_HETEROGENEOUS_NKTUPLE_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_HETEROGENEOUS_NKTUPLE_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKContainers/NkContainersExport.h"
#include "NKCore/NkTraits.h"

namespace nkentseu {
    
        
        /**
         * @brief Tuple - Simplified tuple for 2-3 types
         * 
         * Version simplifiée de std::tuple.
         * Supporte jusqu'à 3 types différents.
         * 
         * @example
         * NkTuple3<int, float, const char*> tuple(42, 3.14f, "Hello");
         * int a = tuple.First;
         * float b = tuple.Second;
         * const char* c = tuple.Third;
         */
        
        // Tuple avec 2 éléments (identique à NkPair)
        template<typename T1, typename T2>
        struct NkTuple2 {
            T1 First;
            T2 Second;
            
            NkTuple2() : First(), Second() {}
            
            NkTuple2(const T1& first, const T2& second)
                : First(first), Second(second) {
            }
            
            #if defined(NK_CPP11)
            template<typename U1, typename U2>
            NkTuple2(U1&& first, U2&& second)
                : First(traits::NkForward<U1>(first))
                , Second(traits::NkForward<U2>(second)) {
            }
            #endif
        };
        
        // Tuple avec 3 éléments
        template<typename T1, typename T2, typename T3>
        struct NkTuple3 {
            T1 First;
            T2 Second;
            T3 Third;
            
            NkTuple3() : First(), Second(), Third() {}
            
            NkTuple3(const T1& first, const T2& second, const T3& third)
                : First(first), Second(second), Third(third) {
            }
            
            #if defined(NK_CPP11)
            template<typename U1, typename U2, typename U3>
            NkTuple3(U1&& first, U2&& second, U3&& third)
                : First(traits::NkForward<U1>(first))
                , Second(traits::NkForward<U2>(second))
                , Third(traits::NkForward<U3>(third)) {
            }
            #endif
        };
        
        // Tuple avec 4 éléments
        template<typename T1, typename T2, typename T3, typename T4>
        struct NkTuple4 {
            T1 First;
            T2 Second;
            T3 Third;
            T4 Fourth;
            
            NkTuple4() : First(), Second(), Third(), Fourth() {}
            
            NkTuple4(const T1& first, const T2& second, const T3& third, const T4& fourth)
                : First(first), Second(second), Third(third), Fourth(fourth) {
            }
            
            #if defined(NK_CPP11)
            template<typename U1, typename U2, typename U3, typename U4>
            NkTuple4(U1&& first, U2&& second, U3&& third, U4&& fourth)
                : First(traits::NkForward<U1>(first))
                , Second(traits::NkForward<U2>(second))
                , Third(traits::NkForward<U3>(third))
                , Fourth(traits::NkForward<U4>(fourth)) {
            }
            #endif
        };
        
        // Helper functions
        template<typename T1, typename T2>
        NkTuple2<T1, T2> NkMakeTuple(const T1& first, const T2& second) {
            return NkTuple2<T1, T2>(first, second);
        }
        
        template<typename T1, typename T2, typename T3>
        NkTuple3<T1, T2, T3> NkMakeTuple(const T1& first, const T2& second, const T3& third) {
            return NkTuple3<T1, T2, T3>(first, second, third);
        }
        
        template<typename T1, typename T2, typename T3, typename T4>
        NkTuple4<T1, T2, T3, T4> NkMakeTuple(const T1& first, const T2& second, const T3& third, const T4& fourth) {
            return NkTuple4<T1, T2, T3, T4>(first, second, third, fourth);
        }
        
    
} // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_HETEROGENEOUS_NKTUPLE_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// ============================================================