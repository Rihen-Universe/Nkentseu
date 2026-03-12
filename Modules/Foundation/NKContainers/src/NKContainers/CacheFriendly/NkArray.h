// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\Containers\CacheFriendly\NkArray.h
// DESCRIPTION: Fixed-size array - Zero-cost wrapper autour de C array
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_CACHEFRIENDLY_NKARRAY_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_CACHEFRIENDLY_NKARRAY_H_INCLUDED

#include "NKContainers/NkCompat.h"
#include "NKCore/NkTypes.h"
#include "NKCore/NkExport.h"
#include "NKCore/NkTraits.h"
#include "NKCore/Assert/NkAssert.h"
#include "NKContainers/Iterators/NkIterator.h"

namespace nkentseu {
    
        
        /**
         * @brief Fixed-size array (STL::array equivalent)
         * 
         * Performance identique à un C array, avec interface moderne.
         * Stack-allocated, très cache-friendly, zero overhead.
         * 
         * @example
         * NkArray<int, 5> arr = {1, 2, 3, 4, 5};
         * arr.Fill(0);
         * for (auto& val : arr) { }
         */
        template<typename T, usize N>
        struct NkArray {
            using ValueType = T;
            using SizeType = usize;
            using Reference = T&;
            using ConstReference = const T&;
            using Pointer = T*;
            using ConstPointer = const T*;
            using Iterator = T*;
            using ConstIterator = const T*;
            using ReverseIterator = NkReverseIterator<Iterator>;
            using ConstReverseIterator = NkReverseIterator<ConstIterator>;
            using value_type = T;
            using size_type = usize;
            using reference = T&;
            using const_reference = const T&;
            using pointer = T*;
            using const_pointer = const T*;
            using iterator = T*;
            using const_iterator = const T*;
            
            // Data (public pour aggregate initialization)
            T mData[N > 0 ? N : 1];
            
            // Element access
            Reference At(SizeType i) {
                NK_ASSERT(i < N);
                return mData[i];
            }
            
            ConstReference At(SizeType i) const {
                NK_ASSERT(i < N);
                return mData[i];
            }
            
            Reference operator[](SizeType i) NK_NOEXCEPT {
                NK_ASSERT(i < N);
                return mData[i];
            }
            
            ConstReference operator[](SizeType i) const NK_NOEXCEPT {
                NK_ASSERT(i < N);
                return mData[i];
            }
            
            NK_CONSTEXPR Reference Front() NK_NOEXCEPT { return mData[0]; }
            NK_CONSTEXPR ConstReference Front() const NK_NOEXCEPT { return mData[0]; }
            NK_CONSTEXPR Reference Back() NK_NOEXCEPT { return mData[N - 1]; }
            NK_CONSTEXPR ConstReference Back() const NK_NOEXCEPT { return mData[N - 1]; }

            NK_CONSTEXPR Pointer Data() NK_NOEXCEPT { return mData; }
            NK_CONSTEXPR ConstPointer Data() const NK_NOEXCEPT { return mData; }
            NK_CONSTEXPR Reference front() NK_NOEXCEPT { return Front(); }
            NK_CONSTEXPR ConstReference front() const NK_NOEXCEPT { return Front(); }
            NK_CONSTEXPR Reference back() NK_NOEXCEPT { return Back(); }
            NK_CONSTEXPR ConstReference back() const NK_NOEXCEPT { return Back(); }
            NK_CONSTEXPR Pointer data() NK_NOEXCEPT { return Data(); }
            NK_CONSTEXPR ConstPointer data() const NK_NOEXCEPT { return Data(); }
            
            // Iterators
            NK_CONSTEXPR Iterator begin() NK_NOEXCEPT { return mData; }
            NK_CONSTEXPR ConstIterator begin() const NK_NOEXCEPT { return mData; }
            NK_CONSTEXPR ConstIterator cbegin() const NK_NOEXCEPT { return mData; }
            
            NK_CONSTEXPR Iterator end() NK_NOEXCEPT { return mData + N; }
            NK_CONSTEXPR ConstIterator end() const NK_NOEXCEPT { return mData + N; }
            NK_CONSTEXPR ConstIterator cend() const NK_NOEXCEPT { return mData + N; }
            
            NK_CONSTEXPR ReverseIterator rbegin() NK_NOEXCEPT { return ReverseIterator(end()); }
            NK_CONSTEXPR ConstReverseIterator rbegin() const NK_NOEXCEPT { return ConstReverseIterator(end()); }
            NK_CONSTEXPR ReverseIterator rend() NK_NOEXCEPT { return ReverseIterator(begin()); }
            NK_CONSTEXPR ConstReverseIterator rend() const NK_NOEXCEPT { return ConstReverseIterator(begin()); }
            
            // Capacity
            NK_CONSTEXPR bool Empty() const NK_NOEXCEPT { return N == 0; }
            NK_CONSTEXPR SizeType Size() const NK_NOEXCEPT { return N; }
            NK_CONSTEXPR SizeType MaxSize() const NK_NOEXCEPT { return N; }
            NK_CONSTEXPR bool empty() const NK_NOEXCEPT { return Empty(); }
            NK_CONSTEXPR SizeType size() const NK_NOEXCEPT { return Size(); }
            NK_CONSTEXPR SizeType max_size() const NK_NOEXCEPT { return MaxSize(); }
            
            // Operations
            void Fill(const T& value) {
                for (SizeType i = 0; i < N; ++i) mData[i] = value;
            }
            void fill(const T& value) { Fill(value); }

            void Swap(NkArray& other) NK_NOEXCEPT {
                for (SizeType i = 0; i < N; ++i) {
                    traits::NkSwap(mData[i], other.mData[i]);
                }
            }
            void swap(NkArray& other) NK_NOEXCEPT { Swap(other); }
        };
        
        // Specialization N=0
        template<typename T>
        struct NkArray<T, 0> {
            using ValueType = T;
            using SizeType = usize;
            using Reference = T&;
            using value_type = T;
            using size_type = usize;
            using reference = T&;
            
            Reference operator[](SizeType) NK_NOEXCEPT {
                NK_ASSERT(false);
                static T dummy;
                return dummy;
            }
            
            NK_CONSTEXPR bool Empty() const NK_NOEXCEPT { return true; }
            NK_CONSTEXPR SizeType Size() const NK_NOEXCEPT { return 0; }
            NK_CONSTEXPR bool empty() const NK_NOEXCEPT { return true; }
            NK_CONSTEXPR SizeType size() const NK_NOEXCEPT { return 0; }
            void Fill(const T&) {}
            void fill(const T&) {}
            void Swap(NkArray&) NK_NOEXCEPT {}
            void swap(NkArray&) NK_NOEXCEPT {}
        };
        
        // Non-member functions
        template<typename T, usize N>
        void NkSwap(NkArray<T, N>& lhs, NkArray<T, N>& rhs) NK_NOEXCEPT {
            lhs.Swap(rhs);
        }
        
        template<typename T, usize N>
        bool operator==(const NkArray<T, N>& lhs, const NkArray<T, N>& rhs) {
            for (usize i = 0; i < N; ++i) {
                if (!(lhs[i] == rhs[i])) return false;
            }
            return true;
        }
        
        template<typename T, usize N>
        bool operator!=(const NkArray<T, N>& lhs, const NkArray<T, N>& rhs) {
            return !(lhs == rhs);
        }
        
    
} // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_CACHEFRIENDLY_NKARRAY_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-05 22:26:13
// Creation Date: 2026-02-05 22:26:13
// ============================================================
