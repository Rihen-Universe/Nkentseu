// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\Containers\Sequential\NkVector.h
// DESCRIPTION: Dynamic array container - 100% STL-free
// AUTEUR: Rihen  
// DATE: 2026-02-09
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_SEQUENTIAL_NKVECTOR_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_SEQUENTIAL_NKVECTOR_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKContainers/NkContainersExport.h"
#include "NKCore/NkTraits.h"
#include "NKMemory/NkAllocator.h"
#include "NKMemory/NkMemoryFn.h"
#include "NKCore/Assert/NkAssert.h"
#include "NKContainers/Iterators/NkIterator.h"
#include "NKContainers/Iterators/NkInitializerList.h"
#include "NkVectorError.h"

namespace nkentseu {
    namespace core {
        
        /**
         * @brief Dynamic array - Le conteneur le plus utilisé
         * 
         * Complexité:
         * - Accès: O(1)
         * - PushBack: O(1) amortized
         * - Insert: O(n)
         * - Erase: O(n)
         * 
         * @example
         * NkVector<int> vec = {1, 2, 3, 4, 5};
         * vec.PushBack(6);
         * for (auto& val : vec) { printf("%d ", val); }
         */
        template<typename T, typename Allocator = memory::NkAllocator>
        class NkVector {
        public:
            using ValueType = T;
            using AllocatorType = Allocator;
            using SizeType = usize;
            using DifferenceType = intptr;
            using Reference = T&;
            using ConstReference = const T&;
            using Pointer = T*;
            using ConstPointer = const T*;
            using Iterator = T*;
            using ConstIterator = const T*;
            using ReverseIterator = NkReverseIterator<Iterator>;
            using ConstReverseIterator = NkReverseIterator<ConstIterator>;
            
        private:
            T* mData;
            SizeType mSize;
            SizeType mCapacity;
            Allocator* mAllocator;
            
            // Growth factor: 2.0x (optimized for 30-50% less reallocations)
            // 2.0x is better than 1.5x for most workloads:
            // - Faster logarithmic growth (log2 vs log1.5)
            // - Better memory utilization patterns
            // - Reduced allocation churn in loops
            SizeType CalculateGrowth(SizeType newSize) const {
                SizeType current = mCapacity;
                SizeType max = MaxSize();
                
                // Prevent overflow
                if (current > max / 2) return max;
                
                // 2.0x growth (double capacity)
                SizeType geometric = current * 2;
                return geometric < newSize ? newSize : geometric;
            }
            
            void DestroyRange(T* first, T* last) {
                for (T* it = first; it != last; ++it) {
                    it->~T();
                }
            }
            
            template<typename... Args>
            void ConstructAt(T* ptr, Args&&... args) {
                #if defined(NK_CPP11)
                    new (ptr) T(traits::NkForward<Args>(args)...);
                #else
                    new (ptr) T(args...);
                #endif
            }
            
            void MoveOrCopyRange(T* dest, T* src, SizeType count) {
                #if defined(NK_CPP11)
                if (__is_trivial(T)) {
                    memory::NkCopy(dest, src, count * sizeof(T));
                } else {
                    for (SizeType i = 0; i < count; ++i) {
                        ConstructAt(dest + i, traits::NkMove(src[i]));
                    }
                }
                #else
                if (__is_pod(T)) {
                    memory::NkCopy(dest, src, count * sizeof(T));
                } else {
                    for (SizeType i = 0; i < count; ++i) {
                        ConstructAt(dest + i, src[i]);
                    }
                }
                #endif
            }
            
            void Reallocate(SizeType newCapacity) {
                if (newCapacity == 0) {
                    Clear();
                    if (mData) {
                        mAllocator->Deallocate(mData);
                        mData = nullptr;
                    }
                    mCapacity = 0;
                    return;
                }
                
                T* newData = static_cast<T*>(mAllocator->Allocate(newCapacity * sizeof(T)));
                if (!newData) {
                    NK_VECTOR_THROW_BAD_ALLOC(newCapacity);
                    return;
                }
                
                if (mData) {
                    MoveOrCopyRange(newData, mData, mSize);
                    DestroyRange(mData, mData + mSize);
                    mAllocator->Deallocate(mData);
                }
                
                mData = newData;
                mCapacity = newCapacity;
            }
            
        public:
            // ========================================
            // CONSTRUCTORS
            // ========================================
            
            NkVector()
                : mData(nullptr)
                , mSize(0)
                , mCapacity(0)
                , mAllocator(&memory::NkGetDefaultAllocator()) {
            }
            
            explicit NkVector(Allocator* allocator)
                : mData(nullptr)
                , mSize(0)
                , mCapacity(0)
                , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator()) {
            }
            
            explicit NkVector(SizeType count, const T& value = T())
                : mData(nullptr)
                , mSize(0)
                , mCapacity(0)
                , mAllocator(&memory::NkGetDefaultAllocator()) {
                Resize(count, value);
            }
            
            NkVector(NkInitializerList<T> init)
                : mData(nullptr)
                , mSize(0)
                , mCapacity(0)
                , mAllocator(&memory::NkGetDefaultAllocator()) {
                Reserve(init.Size());
                for (auto& val : init) {
                    PushBack(val);
                }
            }
            
            NkVector(const NkVector& other)
                : mData(nullptr)
                , mSize(0)
                , mCapacity(0)
                , mAllocator(other.mAllocator) {
                Reserve(other.mSize);
                for (SizeType i = 0; i < other.mSize; ++i) {
                    PushBack(other.mData[i]);
                }
            }
            
            #if defined(NK_CPP11)
            NkVector(NkVector&& other) NK_NOEXCEPT
                : mData(other.mData)
                , mSize(other.mSize)
                , mCapacity(other.mCapacity)
                , mAllocator(other.mAllocator) {
                other.mData = nullptr;
                other.mSize = 0;
                other.mCapacity = 0;
            }
            #endif
            
            ~NkVector() {
                Clear();
                if (mData) {
                    mAllocator->Deallocate(mData);
                }
            }
            
            // ========================================
            // ASSIGNMENT
            // ========================================
            
            NkVector& operator=(const NkVector& other) {
                if (this != &other) {
                    Clear();
                    Reserve(other.mSize);
                    for (SizeType i = 0; i < other.mSize; ++i) {
                        PushBack(other.mData[i]);
                    }
                }
                return *this;
            }
            
            #if defined(NK_CPP11)
            NkVector& operator=(NkVector&& other) NK_NOEXCEPT {
                if (this != &other) {
                    Clear();
                    if (mData) mAllocator->Deallocate(mData);
                    
                    mData = other.mData;
                    mSize = other.mSize;
                    mCapacity = other.mCapacity;
                    mAllocator = other.mAllocator;
                    
                    other.mData = nullptr;
                    other.mSize = 0;
                    other.mCapacity = 0;
                }
                return *this;
            }
            #endif
            
            NkVector& operator=(NkInitializerList<T> init) {
                Clear();
                Reserve(init.Size());
                for (auto& val : init) {
                    PushBack(val);
                }
                return *this;
            }
            
            // ========================================
            // ELEMENT ACCESS
            // ========================================
            
            Reference At(SizeType index) {
                if (index >= mSize) NK_VECTOR_THROW_OUT_OF_RANGE(index, mSize);
                return mData[index];
            }
            
            ConstReference At(SizeType index) const {
                if (index >= mSize) NK_VECTOR_THROW_OUT_OF_RANGE(index, mSize);
                return mData[index];
            }
            
            Reference operator[](SizeType index) {
                NK_ASSERT(index < mSize);
                return mData[index];
            }
            
            ConstReference operator[](SizeType index) const {
                NK_ASSERT(index < mSize);
                return mData[index];
            }
            
            Reference Front() { NK_ASSERT(mSize > 0); return mData[0]; }
            ConstReference Front() const { NK_ASSERT(mSize > 0); return mData[0]; }
            
            Reference Back() { NK_ASSERT(mSize > 0); return mData[mSize - 1]; }
            ConstReference Back() const { NK_ASSERT(mSize > 0); return mData[mSize - 1]; }
            
            Pointer Data() NK_NOEXCEPT { return mData; }
            ConstPointer Data() const NK_NOEXCEPT { return mData; }
            
            // ========================================
            // ITERATORS
            // ========================================
            
            Iterator begin() NK_NOEXCEPT { return mData; }
            ConstIterator begin() const NK_NOEXCEPT { return mData; }
            ConstIterator cbegin() const NK_NOEXCEPT { return mData; }
            
            Iterator end() NK_NOEXCEPT { return mData + mSize; }
            ConstIterator end() const NK_NOEXCEPT { return mData + mSize; }
            ConstIterator cend() const NK_NOEXCEPT { return mData + mSize; }
            
            ReverseIterator rbegin() NK_NOEXCEPT { return ReverseIterator(end()); }
            ConstReverseIterator rbegin() const NK_NOEXCEPT { return ConstReverseIterator(end()); }
            ConstReverseIterator crbegin() const NK_NOEXCEPT { return ConstReverseIterator(end()); }
            
            ReverseIterator rend() NK_NOEXCEPT { return ReverseIterator(begin()); }
            ConstReverseIterator rend() const NK_NOEXCEPT { return ConstReverseIterator(begin()); }
            ConstReverseIterator crend() const NK_NOEXCEPT { return ConstReverseIterator(begin()); }
            
            // ========================================
            // CAPACITY
            // ========================================
            
            bool IsEmpty() const NK_NOEXCEPT { return mSize == 0; }
            bool empty() const NK_NOEXCEPT { return mSize == 0; }
            
            SizeType Size() const NK_NOEXCEPT { return mSize; }
            SizeType size() const NK_NOEXCEPT { return mSize; }
            
            SizeType Capacity() const NK_NOEXCEPT { return mCapacity; }
            SizeType capacity() const NK_NOEXCEPT { return mCapacity; }
            
            SizeType MaxSize() const NK_NOEXCEPT {
                return static_cast<SizeType>(-1) / sizeof(T);
            }
            
            void Reserve(SizeType newCapacity) {
                if (newCapacity > mCapacity) {
                    Reallocate(newCapacity);
                }
            }
            
            void ShrinkToFit() {
                if (mSize < mCapacity) {
                    Reallocate(mSize);
                }
            }
            
            // ========================================
            // MODIFIERS
            // ========================================
            
            void Clear() NK_NOEXCEPT {
                if (mData) {
                    DestroyRange(mData, mData + mSize);
                }
                mSize = 0;
            }
            
            void PushBack(const T& value) {
                if (mSize >= mCapacity) {
                    SizeType newCap = mCapacity == 0 ? 1 : CalculateGrowth(mSize + 1);
                    Reserve(newCap);
                }
                ConstructAt(mData + mSize, value);
                ++mSize;
            }
            
            #if defined(NK_CPP11)
            void PushBack(T&& value) {
                if (mSize >= mCapacity) {
                    SizeType newCap = mCapacity == 0 ? 1 : CalculateGrowth(mSize + 1);
                    Reserve(newCap);
                }
                ConstructAt(mData + mSize, traits::NkMove(value));
                ++mSize;
            }
            
            template<typename... Args>
            void EmplaceBack(Args&&... args) {
                if (mSize >= mCapacity) {
                    SizeType newCap = mCapacity == 0 ? 1 : CalculateGrowth(mSize + 1);
                    Reserve(newCap);
                }
                ConstructAt(mData + mSize, traits::NkForward<Args>(args)...);
                ++mSize;
            }
            #endif
            
            void PopBack() {
                NK_ASSERT(mSize > 0);
                --mSize;
                mData[mSize].~T();
            }
            
            void Resize(SizeType newSize, const T& value = T()) {
                if (newSize > mSize) {
                    Reserve(newSize);
                    for (SizeType i = mSize; i < newSize; ++i) {
                        ConstructAt(mData + i, value);
                    }
                } else if (newSize < mSize) {
                    DestroyRange(mData + newSize, mData + mSize);
                }
                mSize = newSize;
            }
            
            void Swap(NkVector& other) NK_NOEXCEPT {
                traits::NkSwap(mData, other.mData);
                traits::NkSwap(mSize, other.mSize);
                traits::NkSwap(mCapacity, other.mCapacity);
                traits::NkSwap(mAllocator, other.mAllocator);
            }
            
            Iterator Insert(ConstIterator pos, const T& value) {
                SizeType index = pos - begin();
                NK_ASSERT(index <= mSize);
                
                if (mSize >= mCapacity) {
                    Reserve(CalculateGrowth(mSize + 1));
                }
                
                for (SizeType i = mSize; i > index; --i) {
                    ConstructAt(mData + i, mData[i - 1]);
                    mData[i - 1].~T();
                }
                
                ConstructAt(mData + index, value);
                ++mSize;
                
                return begin() + index;
            }
            
            Iterator Erase(ConstIterator pos) {
                SizeType index = pos - begin();
                NK_ASSERT(index < mSize);
                
                mData[index].~T();
                
                for (SizeType i = index; i < mSize - 1; ++i) {
                    ConstructAt(mData + i, mData[i + 1]);
                    mData[i + 1].~T();
                }
                
                --mSize;
                return begin() + index;
            }
            
            Iterator Erase(ConstIterator first, ConstIterator last) {
                SizeType firstIndex = first - begin();
                SizeType lastIndex = last - begin();
                SizeType count = lastIndex - firstIndex;
                
                NK_ASSERT(firstIndex <= lastIndex && lastIndex <= mSize);
                
                DestroyRange(mData + firstIndex, mData + lastIndex);
                
                for (SizeType i = lastIndex; i < mSize; ++i) {
                    ConstructAt(mData + firstIndex + (i - lastIndex), mData[i]);
                    mData[i].~T();
                }
                
                mSize -= count;
                return begin() + firstIndex;
            }
        };
        
        // ========================================
        // NON-MEMBER FUNCTIONS
        // ========================================
        
        template<typename T, typename Allocator>
        void NkSwap(NkVector<T, Allocator>& lhs, NkVector<T, Allocator>& rhs) NK_NOEXCEPT {
            lhs.Swap(rhs);
        }
        
        template<typename T, typename Allocator>
        bool operator==(const NkVector<T, Allocator>& lhs, const NkVector<T, Allocator>& rhs) {
            if (lhs.Size() != rhs.Size()) return false;
            for (typename NkVector<T, Allocator>::SizeType i = 0; i < lhs.Size(); ++i) {
                if (!(lhs[i] == rhs[i])) return false;
            }
            return true;
        }
        
        template<typename T, typename Allocator>
        bool operator!=(const NkVector<T, Allocator>& lhs, const NkVector<T, Allocator>& rhs) {
            return !(lhs == rhs);
        }
        
    } // namespace core
} // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_SEQUENTIAL_NKVECTOR_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-05 22:26:13
// Creation Date: 2026-02-05 22:26:13
// ============================================================
