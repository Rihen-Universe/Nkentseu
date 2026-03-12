// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\Containers\Views\NkSpan.h
// DESCRIPTION: Span - Non-owning view over contiguous sequence (STL::span)
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_VIEWS_NKSPAN_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_VIEWS_NKSPAN_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKContainers/NkContainersExport.h"
#include "NKCore/NkTraits.h"
#include "NKCore/Assert/NkAssert.h"

namespace nkentseu {
    
        
        /**
         * @brief Span - Non-owning view over contiguous sequence
         * 
         * Vue légère sur une séquence contiguë d'éléments.
         * Ne possède pas les données, juste un pointeur + taille.
         * 
         * Complexité:
         * - Toutes opérations: O(1)
         * - Pas d'allocation
         * 
         * @example
         * void Process(NkSpan<int> data) {
         *     for (auto& val : data) {
         *         printf("%d ", val);
         *     }
         * }
         * 
         * NkVector<int> vec = {1, 2, 3};
         * int arr[] = {4, 5, 6};
         * Process(vec);  // Works!
         * Process(arr);  // Works!
         */
        template<typename T>
        class NkSpan {
        public:
            using ElementType = T;
            using ValueType = traits::NkRemoveCV<T>;
            using SizeType = usize;
            using Reference = T&;
            using ConstReference = const T&;
            using Pointer = T*;
            using ConstPointer = const T*;
            using Iterator = T*;
            using ConstIterator = const T*;
            
        private:
            T* mData;
            SizeType mSize;
            
        public:
            // Constructors
            NK_CONSTEXPR NkSpan() NK_NOEXCEPT
                : mData(nullptr), mSize(0) {
            }
            
            NK_CONSTEXPR NkSpan(T* data, SizeType size) NK_NOEXCEPT
                : mData(data), mSize(size) {
            }
            
            template<SizeType N>
            NK_CONSTEXPR NkSpan(T (&arr)[N]) NK_NOEXCEPT
                : mData(arr), mSize(N) {
            }
            
            // From Vector-like containers
            template<typename Container>
            NK_CONSTEXPR NkSpan(Container& container) NK_NOEXCEPT
                : mData(container.Data()), mSize(container.Size()) {
            }
            
            // Copy constructor
            NK_CONSTEXPR NkSpan(const NkSpan& other) NK_NOEXCEPT
                : mData(other.mData), mSize(other.mSize) {
            }
            
            // Assignment
            NK_CONSTEXPR NkSpan& operator=(const NkSpan& other) NK_NOEXCEPT {
                mData = other.mData;
                mSize = other.mSize;
                return *this;
            }
            
            // Element access
            Reference operator[](SizeType index) const NK_NOEXCEPT {
                NK_ASSERT(index < mSize);
                return mData[index];
            }
            
            Reference Front() const NK_NOEXCEPT {
                NK_ASSERT(mSize > 0);
                return mData[0];
            }
            
            Reference Back() const NK_NOEXCEPT {
                NK_ASSERT(mSize > 0);
                return mData[mSize - 1];
            }
            
            NK_CONSTEXPR Pointer Data() const NK_NOEXCEPT {
                return mData;
            }
            
            // Iterators
            NK_CONSTEXPR Iterator begin() const NK_NOEXCEPT { return mData; }
            NK_CONSTEXPR Iterator end() const NK_NOEXCEPT { return mData + mSize; }
            
            // Capacity
            NK_CONSTEXPR SizeType Size() const NK_NOEXCEPT { return mSize; }
            NK_CONSTEXPR SizeType SizeBytes() const NK_NOEXCEPT { return mSize * sizeof(T); }
            NK_CONSTEXPR bool Empty() const NK_NOEXCEPT { return mSize == 0; }
            
            // Subviews
            NkSpan First(SizeType count) const {
                NK_ASSERT(count <= mSize);
                return NkSpan(mData, count);
            }
            
            NkSpan Last(SizeType count) const {
                NK_ASSERT(count <= mSize);
                return NkSpan(mData + (mSize - count), count);
            }
            
            NkSpan Subspan(SizeType offset, SizeType count) const {
                NK_ASSERT(offset <= mSize);
                NK_ASSERT(count <= mSize - offset);
                return NkSpan(mData + offset, count);
            }
        };
        
        // Deduction guides (C++17)
        #if defined(NK_CPP17)
        template<typename T, usize N>
        NkSpan(T (&)[N]) -> NkSpan<T>;
        
        template<typename Container>
        NkSpan(Container&) -> NkSpan<typename Container::ValueType>;
        #endif
        
        // Helper functions
        template<typename T>
        NK_CONSTEXPR NkSpan<T> NkMakeSpan(T* data, usize size) NK_NOEXCEPT {
            return NkSpan<T>(data, size);
        }
        
        template<typename T, usize N>
        NK_CONSTEXPR NkSpan<T> NkMakeSpan(T (&arr)[N]) NK_NOEXCEPT {
            return NkSpan<T>(arr);
        }
        
        template<typename Container>
        NK_CONSTEXPR NkSpan<typename Container::ValueType> NkMakeSpan(Container& cont) NK_NOEXCEPT {
            return NkSpan<typename Container::ValueType>(cont);
        }
        
    
} // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_VIEWS_NKSPAN_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// ============================================================
