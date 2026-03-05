// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\Containers\Iterators\NkIterator.h
// DESCRIPTION: Iterator base interface - 100% STL-free
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ITERATORS_NKITERATOR_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ITERATORS_NKITERATOR_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKContainers/NkContainersExport.h"
#include "NKCore/NkTraits.h"

namespace nkentseu {
    namespace core {
        
        // ========================================
        // ITERATOR CATEGORIES
        // ========================================
        
        /**
         * @brief Iterator category tags
         */
        struct NkInputIteratorTag {};
        struct NkOutputIteratorTag {};
        struct NkForwardIteratorTag : public NkInputIteratorTag {};
        struct NkBidirectionalIteratorTag : public NkForwardIteratorTag {};
        struct NkRandomAccessIteratorTag : public NkBidirectionalIteratorTag {};
        struct NkContiguousIteratorTag : public NkRandomAccessIteratorTag {};
        
        // ========================================
        // ITERATOR TRAITS
        // ========================================
        
        /**
         * @brief Iterator traits - extracts iterator properties
         */
        template<typename Iterator>
        struct NkIteratorTraits {
            using DifferenceType = typename Iterator::DifferenceType;
            using ValueType = typename Iterator::ValueType;
            using Pointer = typename Iterator::Pointer;
            using Reference = typename Iterator::Reference;
            using IteratorCategory = typename Iterator::IteratorCategory;
        };
        
        /**
         * @brief Specialization for pointers
         */
        template<typename T>
        struct NkIteratorTraits<T*> {
            using DifferenceType = intptr;
            using ValueType = T;
            using Pointer = T*;
            using Reference = T&;
            using IteratorCategory = NkRandomAccessIteratorTag;
        };
        
        /**
         * @brief Specialization for const pointers
         */
        template<typename T>
        struct NkIteratorTraits<const T*> {
            using DifferenceType = intptr;
            using ValueType = T;
            using Pointer = const T*;
            using Reference = const T&;
            using IteratorCategory = NkRandomAccessIteratorTag;
        };
        
        // ========================================
        // REVERSE ITERATOR
        // ========================================
        
        /**
         * @brief Reverse iterator adapter
         */
        template<typename Iterator>
        class NkReverseIterator {
        public:
            using IteratorType = Iterator;
            using IteratorCategory = typename NkIteratorTraits<Iterator>::IteratorCategory;
            using ValueType = typename NkIteratorTraits<Iterator>::ValueType;
            using DifferenceType = typename NkIteratorTraits<Iterator>::DifferenceType;
            using Pointer = typename NkIteratorTraits<Iterator>::Pointer;
            using Reference = typename NkIteratorTraits<Iterator>::Reference;
            
        private:
            Iterator mCurrent;
            
        public:
            // ========================================
            // CONSTRUCTORS
            // ========================================
            
            NK_CONSTEXPR NkReverseIterator() : mCurrent() {}
            
            explicit NK_CONSTEXPR NkReverseIterator(Iterator it) : mCurrent(it) {}
            
            template<typename U>
            NK_CONSTEXPR NkReverseIterator(const NkReverseIterator<U>& other)
                : mCurrent(other.Base()) {}
            
            // ========================================
            // BASE ACCESS
            // ========================================
            
            NK_CONSTEXPR Iterator Base() const { return mCurrent; }
            
            // ========================================
            // DEREFERENCE
            // ========================================
            
            NK_CONSTEXPR Reference operator*() const {
                Iterator tmp = mCurrent;
                --tmp;
                return *tmp;
            }
            
            NK_CONSTEXPR Pointer operator->() const {
                Iterator tmp = mCurrent;
                --tmp;
                return &(*tmp);
            }
            
            // ========================================
            // INCREMENT/DECREMENT
            // ========================================
            
            NK_CONSTEXPR NkReverseIterator& operator++() {
                --mCurrent;
                return *this;
            }
            
            NK_CONSTEXPR NkReverseIterator operator++(int) {
                NkReverseIterator tmp = *this;
                --mCurrent;
                return tmp;
            }
            
            NK_CONSTEXPR NkReverseIterator& operator--() {
                ++mCurrent;
                return *this;
            }
            
            NK_CONSTEXPR NkReverseIterator operator--(int) {
                NkReverseIterator tmp = *this;
                ++mCurrent;
                return tmp;
            }
            
            // ========================================
            // RANDOM ACCESS
            // ========================================
            
            NK_CONSTEXPR NkReverseIterator operator+(DifferenceType n) const {
                return NkReverseIterator(mCurrent - n);
            }
            
            NK_CONSTEXPR NkReverseIterator operator-(DifferenceType n) const {
                return NkReverseIterator(mCurrent + n);
            }
            
            NK_CONSTEXPR NkReverseIterator& operator+=(DifferenceType n) {
                mCurrent -= n;
                return *this;
            }
            
            NK_CONSTEXPR NkReverseIterator& operator-=(DifferenceType n) {
                mCurrent += n;
                return *this;
            }
            
            NK_CONSTEXPR Reference operator[](DifferenceType n) const {
                return *(*this + n);
            }
        };
        
        // ========================================
        // REVERSE ITERATOR COMPARISON
        // ========================================
        
        template<typename Iterator1, typename Iterator2>
        NK_CONSTEXPR bool operator==(const NkReverseIterator<Iterator1>& lhs,
                                      const NkReverseIterator<Iterator2>& rhs) {
            return lhs.Base() == rhs.Base();
        }
        
        template<typename Iterator1, typename Iterator2>
        NK_CONSTEXPR bool operator!=(const NkReverseIterator<Iterator1>& lhs,
                                      const NkReverseIterator<Iterator2>& rhs) {
            return lhs.Base() != rhs.Base();
        }
        
        template<typename Iterator1, typename Iterator2>
        NK_CONSTEXPR bool operator<(const NkReverseIterator<Iterator1>& lhs,
                                     const NkReverseIterator<Iterator2>& rhs) {
            return lhs.Base() > rhs.Base();
        }
        
        template<typename Iterator1, typename Iterator2>
        NK_CONSTEXPR bool operator<=(const NkReverseIterator<Iterator1>& lhs,
                                      const NkReverseIterator<Iterator2>& rhs) {
            return lhs.Base() >= rhs.Base();
        }
        
        template<typename Iterator1, typename Iterator2>
        NK_CONSTEXPR bool operator>(const NkReverseIterator<Iterator1>& lhs,
                                     const NkReverseIterator<Iterator2>& rhs) {
            return lhs.Base() < rhs.Base();
        }
        
        template<typename Iterator1, typename Iterator2>
        NK_CONSTEXPR bool operator>=(const NkReverseIterator<Iterator1>& lhs,
                                      const NkReverseIterator<Iterator2>& rhs) {
            return lhs.Base() <= rhs.Base();
        }
        
        template<typename Iterator>
        NK_CONSTEXPR typename NkReverseIterator<Iterator>::DifferenceType
        operator-(const NkReverseIterator<Iterator>& lhs,
                 const NkReverseIterator<Iterator>& rhs) {
            return rhs.Base() - lhs.Base();
        }
        
        template<typename Iterator>
        NK_CONSTEXPR NkReverseIterator<Iterator>
        operator+(typename NkReverseIterator<Iterator>::DifferenceType n,
                 const NkReverseIterator<Iterator>& it) {
            return NkReverseIterator<Iterator>(it.Base() - n);
        }
        
        // ========================================
        // MOVE ITERATOR (C++11)
        // ========================================
        
        #if defined(NK_CPP11)
        
        /**
         * @brief Move iterator - converts copy to move
         */
        template<typename Iterator>
        class NkMoveIterator {
        public:
            using IteratorType = Iterator;
            using IteratorCategory = typename NkIteratorTraits<Iterator>::IteratorCategory;
            using ValueType = typename NkIteratorTraits<Iterator>::ValueType;
            using DifferenceType = typename NkIteratorTraits<Iterator>::DifferenceType;
            using Pointer = typename NkIteratorTraits<Iterator>::Pointer;
            using Reference = ValueType&&;
            
        private:
            Iterator mCurrent;
            
        public:
            NK_CONSTEXPR NkMoveIterator() : mCurrent() {}
            
            explicit NK_CONSTEXPR NkMoveIterator(Iterator it) : mCurrent(it) {}
            
            template<typename U>
            NK_CONSTEXPR NkMoveIterator(const NkMoveIterator<U>& other)
                : mCurrent(other.Base()) {}
            
            NK_CONSTEXPR Iterator Base() const { return mCurrent; }
            
            NK_CONSTEXPR Reference operator*() const {
                return static_cast<Reference>(*mCurrent);
            }
            
            NK_CONSTEXPR Pointer operator->() const { return mCurrent; }
            
            NK_CONSTEXPR NkMoveIterator& operator++() {
                ++mCurrent;
                return *this;
            }
            
            NK_CONSTEXPR NkMoveIterator operator++(int) {
                NkMoveIterator tmp = *this;
                ++mCurrent;
                return tmp;
            }
            
            NK_CONSTEXPR NkMoveIterator& operator--() {
                --mCurrent;
                return *this;
            }
            
            NK_CONSTEXPR NkMoveIterator operator--(int) {
                NkMoveIterator tmp = *this;
                --mCurrent;
                return tmp;
            }
            
            NK_CONSTEXPR NkMoveIterator operator+(DifferenceType n) const {
                return NkMoveIterator(mCurrent + n);
            }
            
            NK_CONSTEXPR NkMoveIterator operator-(DifferenceType n) const {
                return NkMoveIterator(mCurrent - n);
            }
            
            NK_CONSTEXPR NkMoveIterator& operator+=(DifferenceType n) {
                mCurrent += n;
                return *this;
            }
            
            NK_CONSTEXPR NkMoveIterator& operator-=(DifferenceType n) {
                mCurrent -= n;
                return *this;
            }
            
            NK_CONSTEXPR Reference operator[](DifferenceType n) const {
                return static_cast<Reference>(mCurrent[n]);
            }
        };
        
        /**
         * @brief Make move iterator
         */
        template<typename Iterator>
        NK_CONSTEXPR NkMoveIterator<Iterator> NkMakeMoveIterator(Iterator it) {
            return NkMoveIterator<Iterator>(it);
        }
        
        #endif // NK_CPP11
        
        // ========================================
        // ITERATOR UTILITIES
        // ========================================
        
        /**
         * @brief Advance iterator by n positions - Random Access
         */
        template<typename Iterator, typename Distance>
        NK_CONSTEXPR void NkAdvanceImpl(Iterator& it, Distance n, NkRandomAccessIteratorTag) {
            it += n;
        }
        
        /**
         * @brief Advance iterator by n positions - Bidirectional
         */
        template<typename Iterator, typename Distance>
        NK_CONSTEXPR void NkAdvanceImpl(Iterator& it, Distance n, NkBidirectionalIteratorTag) {
            if (n > 0) {
                while (n--) ++it;
            } else {
                while (n++) --it;
            }
        }
        
        /**
         * @brief Advance iterator by n positions - Forward
         */
        template<typename Iterator, typename Distance>
        NK_CONSTEXPR void NkAdvanceImpl(Iterator& it, Distance n, NkForwardIteratorTag) {
            while (n--) ++it;
        }
        
        /**
         * @brief Advance iterator by n positions
         */
        template<typename Iterator, typename Distance>
        NK_CONSTEXPR void NkAdvance(Iterator& it, Distance n) {
            using Category = typename NkIteratorTraits<Iterator>::IteratorCategory;
            NkAdvanceImpl(it, n, Category());
        }
        
        /**
         * @brief Distance between iterators - Random Access
         */
        template<typename Iterator>
        NK_CONSTEXPR typename NkIteratorTraits<Iterator>::DifferenceType
        NkDistanceImpl(Iterator first, Iterator last, NkRandomAccessIteratorTag) {
            return last - first;
        }
        
        /**
         * @brief Distance between iterators - Other
         */
        template<typename Iterator, typename Category>
        NK_CONSTEXPR typename NkIteratorTraits<Iterator>::DifferenceType
        NkDistanceImpl(Iterator first, Iterator last, Category) {
            typename NkIteratorTraits<Iterator>::DifferenceType n = 0;
            while (first != last) {
                ++first;
                ++n;
            }
            return n;
        }
        
        /**
         * @brief Distance between two iterators
         */
        template<typename Iterator>
        NK_CONSTEXPR typename NkIteratorTraits<Iterator>::DifferenceType
        NkDistance(Iterator first, Iterator last) {
            using Category = typename NkIteratorTraits<Iterator>::IteratorCategory;
            return NkDistanceImpl(first, last, Category());
        }
        
        /**
         * @brief Next iterator
         */
        template<typename Iterator>
        NK_CONSTEXPR Iterator NkNext(Iterator it,
            typename NkIteratorTraits<Iterator>::DifferenceType n = 1) {
            NkAdvance(it, n);
            return it;
        }
        
        /**
         * @brief Previous iterator
         */
        template<typename Iterator>
        NK_CONSTEXPR Iterator NkPrev(Iterator it,
            typename NkIteratorTraits<Iterator>::DifferenceType n = 1) {
            NkAdvance(it, -n);
            return it;
        }
        
    } // namespace core
} // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ITERATORS_NKITERATOR_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-09
// ============================================================