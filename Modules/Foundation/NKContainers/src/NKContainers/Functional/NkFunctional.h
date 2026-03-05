// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\Containers\Functional\NkFunctional.h
// DESCRIPTION: Functional programming algorithms - map, filter, reduce, etc.
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_FUNCTIONAL_NKFUNCTIONAL_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_FUNCTIONAL_NKFUNCTIONAL_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKContainers/NkContainersExport.h"
#include "NKCore/NkTraits.h"
#include "NKContainers/Heterogeneous/NkPair.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
    namespace core {
        namespace functional {
            
            /**
             * @brief Map - Transform each element
             * 
             * Applique une fonction à chaque élément et retourne un nouveau conteneur.
             * 
             * @example
             * NkVector<int> vec = {1, 2, 3};
             * auto result = Map(vec, [](int x) { return x * 2; });
             * // result = {2, 4, 6}
             */
            template<typename Container, typename Func>
            Container NkMapFunc(const Container& container, Func func) {
                Container result;
                for (auto it = container.begin(); it != container.end(); ++it) {
                    result.PushBack(func(*it));
                }
                return result;
            }
            
            /**
             * @brief Filter - Keep only elements matching predicate
             * 
             * Garde seulement les éléments pour lesquels le prédicat est vrai.
             * 
             * @example
             * NkVector<int> vec = {1, 2, 3, 4, 5};
             * auto result = Filter(vec, [](int x) { return x % 2 == 0; });
             * // result = {2, 4}
             */
            template<typename Container, typename Predicate>
            Container NkFilter(const Container& container, Predicate pred) {
                Container result;
                for (auto it = container.begin(); it != container.end(); ++it) {
                    if (pred(*it)) {
                        result.PushBack(*it);
                    }
                }
                return result;
            }
            
            /**
             * @brief Reduce - Fold elements into single value
             * 
             * Réduit tous les éléments à une seule valeur avec une fonction.
             * 
             * @example
             * NkVector<int> vec = {1, 2, 3, 4};
             * int sum = Reduce(vec, 0, [](int acc, int x) { return acc + x; });
             * // sum = 10
             */
            template<typename Container, typename T, typename Func>
            T NkReduce(const Container& container, T init, Func func) {
                T result = init;
                for (auto it = container.begin(); it != container.end(); ++it) {
                    result = func(result, *it);
                }
                return result;
            }
            
            /**
             * @brief ForEach - Apply function to each element
             * 
             * Applique une fonction à chaque élément (pour side effects).
             * 
             * @example
             * NkVector<int> vec = {1, 2, 3};
             * ForEach(vec, [](int x) { printf("%d ", x); });
             */
            template<typename Container, typename Func>
            void NkForEach(const Container& container, Func func) {
                for (auto it = container.begin(); it != container.end(); ++it) {
                    func(*it);
                }
            }
            
            /**
             * @brief Any - Check if any element matches predicate
             * 
             * @example
             * NkVector<int> vec = {1, 2, 3};
             * bool hasEven = Any(vec, [](int x) { return x % 2 == 0; });
             * // hasEven = true
             */
            template<typename Container, typename Predicate>
            bool NkAny(const Container& container, Predicate pred) {
                for (auto it = container.begin(); it != container.end(); ++it) {
                    if (pred(*it)) return true;
                }
                return false;
            }
            
            /**
             * @brief All - Check if all elements match predicate
             * 
             * @example
             * NkVector<int> vec = {2, 4, 6};
             * bool allEven = All(vec, [](int x) { return x % 2 == 0; });
             * // allEven = true
             */
            template<typename Container, typename Predicate>
            bool NkAll(const Container& container, Predicate pred) {
                for (auto it = container.begin(); it != container.end(); ++it) {
                    if (!pred(*it)) return false;
                }
                return true;
            }
            
            /**
             * @brief Count - Count elements matching predicate
             * 
             * @example
             * NkVector<int> vec = {1, 2, 3, 4, 5};
             * usize count = Count(vec, [](int x) { return x > 3; });
             * // count = 2
             */
            template<typename Container, typename Predicate>
            usize NkCount(const Container& container, Predicate pred) {
                usize count = 0;
                for (auto it = container.begin(); it != container.end(); ++it) {
                    if (pred(*it)) ++count;
                }
                return count;
            }
            
            /**
             * @brief Find - Find first element matching predicate
             * 
             * @return Pointer to element, or nullptr if not found
             */
            template<typename Container, typename Predicate>
            typename Container::ValueType* NkFind(Container& container, Predicate pred) {
                for (auto it = container.begin(); it != container.end(); ++it) {
                    if (pred(*it)) return &(*it);
                }
                return nullptr;
            }
            
            /**
             * @brief Partition - Separate into two groups
             * 
             * @return Pair of containers (matches, non-matches)
             * 
             * @example
             * NkVector<int> vec = {1, 2, 3, 4, 5};
             * auto parts = Partition(vec, [](int x) { return x % 2 == 0; });
             * // parts.First = {2, 4}
             * // parts.Second = {1, 3, 5}
             */
            template<typename Container, typename Predicate>
            NkPair<Container, Container> NkPartition(const Container& container, Predicate pred) {
                Container matches, nonMatches;
                for (auto it = container.begin(); it != container.end(); ++it) {
                    if (pred(*it)) {
                        matches.PushBack(*it);
                    } else {
                        nonMatches.PushBack(*it);
                    }
                }
                return NkPair<Container, Container>(matches, nonMatches);
            }
            
            /**
             * @brief Take - Take first N elements
             * 
             * @example
             * NkVector<int> vec = {1, 2, 3, 4, 5};
             * auto result = Take(vec, 3);
             * // result = {1, 2, 3}
             */
            template<typename Container>
            Container NkTake(const Container& container, usize n) {
                Container result;
                auto it = container.begin();
                for (usize i = 0; i < n && it != container.end(); ++i, ++it) {
                    result.PushBack(*it);
                }
                return result;
            }
            
            /**
             * @brief Skip - Skip first N elements
             * 
             * @example
             * NkVector<int> vec = {1, 2, 3, 4, 5};
             * auto result = Skip(vec, 2);
             * // result = {3, 4, 5}
             */
            template<typename Container>
            Container NkSkip(const Container& container, usize n) {
                Container result;
                auto it = container.begin();
                for (usize i = 0; i < n && it != container.end(); ++i, ++it) {
                    // Skip
                }
                for (; it != container.end(); ++it) {
                    result.PushBack(*it);
                }
                return result;
            }
            
            /**
             * @brief Zip - Combine two containers element-wise
             * 
             * @example
             * NkVector<int> a = {1, 2, 3};
             * NkVector<float> b = {1.1f, 2.2f, 3.3f};
             * auto result = Zip(a, b);
             * // result = {(1, 1.1), (2, 2.2), (3, 3.3)}
             */
            template<typename Container1, typename Container2>
            NkVector<NkPair<typename Container1::ValueType, typename Container2::ValueType>>
            NkZip(const Container1& c1, const Container2& c2) {
                NkVector<NkPair<typename Container1::ValueType, typename Container2::ValueType>> result;
                
                auto it1 = c1.begin();
                auto it2 = c2.begin();
                
                while (it1 != c1.end() && it2 != c2.end()) {
                    result.PushBack(NkPair<typename Container1::ValueType, typename Container2::ValueType>(*it1, *it2));
                    ++it1;
                    ++it2;
                }
                
                return result;
            }
            
            /**
             * @brief Sort - Sort container (selection sort - simple)
             * 
             * @example
             * NkVector<int> vec = {3, 1, 4, 1, 5};
             * Sort(vec);
             * // vec = {1, 1, 3, 4, 5}
             */
            template<typename Container>
            void NkSort(Container& container) {
                for (auto i = container.begin(); i != container.end(); ++i) {
                    auto minIt = i;
                    for (auto j = i; j != container.end(); ++j) {
                        if (*j < *minIt) {
                            minIt = j;
                        }
                    }
                    if (minIt != i) {
                        traits::NkSwap(*i, *minIt);
                    }
                }
            }
            
            /**
             * @brief Reverse - Reverse container in-place
             */
            template<typename Container>
            void NkReverse(Container& container) {
                auto begin = container.begin();
                auto end = container.end();
                if (begin == end) return;
                --end;
                
                while (begin != end) {
                    traits::NkSwap(*begin, *end);
                    ++begin;
                    if (begin == end) break;
                    --end;
                }
            }
            
        } // namespace functional
    } // namespace core
} // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_FUNCTIONAL_NKFUNCTIONAL_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// 
// USAGE EXAMPLES:
// 
// NkVector<int> vec = {1, 2, 3, 4, 5};
// 
// // Map
// auto doubled = Map(vec, [](int x) { return x * 2; });
// // {2, 4, 6, 8, 10}
// 
// // Filter
// auto evens = Filter(vec, [](int x) { return x % 2 == 0; });
// // {2, 4}
// 
// // Reduce
// int sum = Reduce(vec, 0, [](int acc, int x) { return acc + x; });
// // 15
// 
// // Chaining
// auto result = Map(
//     Filter(vec, [](int x) { return x > 2; }),
//     [](int x) { return x * x; }
// );
// // {9, 16, 25}
// 
// ============================================================

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-05 22:26:13
// Creation Date: 2026-02-05 22:26:13
// ============================================================
