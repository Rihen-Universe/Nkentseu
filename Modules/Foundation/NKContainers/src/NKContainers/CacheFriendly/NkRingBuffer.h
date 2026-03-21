// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\Containers\CacheFriendly\NkRingBuffer.h
// DESCRIPTION: Circular buffer - FIFO queue with fixed capacity
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_CACHEFRIENDLY_NKRINGBUFFER_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_CACHEFRIENDLY_NKRINGBUFFER_H_INCLUDED

#include "NKContainers/NkCompat.h"
#include "NKCore/NkTypes.h"
#include "NKCore/NkExport.h"
#include "NKCore/NkTraits.h"
#include "NKMemory/NkAllocator.h"
#include "NKMemory/NkFunction.h"
#include "NKCore/Assert/NkAssert.h"
#include "NKContainers/Iterators/NkIterator.h"
#include "NKContainers/Iterators/NkInitializerList.h"

namespace nkentseu {
    
        
        /**
         * @brief Ring buffer (circular buffer)
         * 
         * FIFO queue avec capacité fixe. Quand plein, écrase les anciennes
         * valeurs (comportement circulaire).
         * 
         * Très efficace pour:
         * - Audio/Video buffering
         * - Logging circulaire
         * - Producer-consumer patterns
         * - Lock-free queues (avec atomics)
         * 
         * Complexité:
         * - Push/Pop: O(1)
         * - Accès: O(1)
         * - Pas de reallocation
         * 
         * @example
         * NkRingBuffer<int> ring(5);
         * ring.Push(1);
         * ring.Push(2);
         * int val = ring.Pop();  // 1
         */
        template<typename T, typename Allocator = memory::NkAllocator>
        class NkRingBuffer {
        public:
            using ValueType = T;
            using SizeType = usize;
            using Reference = T&;
            using ConstReference = const T&;
            using Pointer = T*;
            using ConstPointer = const T*;
            
        private:
            T* mData;
            SizeType mCapacity;
            SizeType mHead;  // Position d'écriture
            SizeType mTail;  // Position de lecture
            SizeType mSize;  // Nombre d'éléments
            Allocator* mAllocator;
            
            SizeType NextIndex(SizeType index) const {
                return (index + 1) % mCapacity;
            }
            
        public:
            // ========================================
            // CONSTRUCTORS
            // ========================================
            
            explicit NkRingBuffer(SizeType capacity)
                : mData(nullptr)
                , mCapacity(capacity)
                , mHead(0)
                , mTail(0)
                , mSize(0)
                , mAllocator(&memory::NkGetDefaultAllocator()) {
                NK_ASSERT(capacity > 0);
                mData = static_cast<T*>(mAllocator->Allocate(mCapacity * sizeof(T)));
            }

            NkRingBuffer(NkInitializerList<T> init, Allocator* allocator = nullptr)
                : mData(nullptr)
                , mCapacity(init.Size() > 0 ? init.Size() : 1)
                , mHead(0)
                , mTail(0)
                , mSize(0)
                , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator()) {
                mData = static_cast<T*>(mAllocator->Allocate(mCapacity * sizeof(T)));
                for (auto& val : init) Push(val);
            }

            NkRingBuffer(std::initializer_list<T> init, Allocator* allocator = nullptr)
                : mData(nullptr)
                , mCapacity(init.size() > 0 ? init.size() : 1)
                , mHead(0)
                , mTail(0)
                , mSize(0)
                , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator()) {
                mData = static_cast<T*>(mAllocator->Allocate(mCapacity * sizeof(T)));
                for (auto& val : init) Push(val);
            }

            NkRingBuffer& operator=(NkInitializerList<T> init) {
                Clear();
                for (auto& val : init) Push(val);
                return *this;
            }

            NkRingBuffer& operator=(std::initializer_list<T> init) {
                Clear();
                for (auto& val : init) Push(val);
                return *this;
            }
            
            explicit NkRingBuffer(SizeType capacity, Allocator* allocator)
                : mData(nullptr)
                , mCapacity(capacity)
                , mHead(0)
                , mTail(0)
                , mSize(0)
                , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator()) {
                NK_ASSERT(capacity > 0);
                mData = static_cast<T*>(mAllocator->Allocate(mCapacity * sizeof(T)));
            }
            
            NkRingBuffer(const NkRingBuffer& other)
                : mData(nullptr)
                , mCapacity(other.mCapacity)
                , mHead(other.mHead)
                , mTail(other.mTail)
                , mSize(other.mSize)
                , mAllocator(other.mAllocator) {
                mData = static_cast<T*>(mAllocator->Allocate(mCapacity * sizeof(T)));
                for (SizeType i = 0; i < mSize; ++i) {
                    SizeType idx = (mTail + i) % mCapacity;
                    new (&mData[idx]) T(other.mData[idx]);
                }
            }
            
            #if defined(NK_CPP11)
            NkRingBuffer(NkRingBuffer&& other) NK_NOEXCEPT
                : mData(other.mData)
                , mCapacity(other.mCapacity)
                , mHead(other.mHead)
                , mTail(other.mTail)
                , mSize(other.mSize)
                , mAllocator(other.mAllocator) {
                other.mData = nullptr;
                other.mCapacity = 0;
                other.mHead = 0;
                other.mTail = 0;
                other.mSize = 0;
            }
            #endif
            
            ~NkRingBuffer() {
                Clear();
                if (mData) {
                    mAllocator->Deallocate(mData);
                }
            }
            
            // ========================================
            // ASSIGNMENT
            // ========================================
            
            NkRingBuffer& operator=(const NkRingBuffer& other) {
                if (this != &other) {
                    Clear();
                    if (mData) mAllocator->Deallocate(mData);
                    
                    mCapacity = other.mCapacity;
                    mHead = other.mHead;
                    mTail = other.mTail;
                    mSize = other.mSize;
                    mAllocator = other.mAllocator;
                    
                    mData = static_cast<T*>(mAllocator->Allocate(mCapacity * sizeof(T)));
                    for (SizeType i = 0; i < mSize; ++i) {
                        SizeType idx = (mTail + i) % mCapacity;
                        new (&mData[idx]) T(other.mData[idx]);
                    }
                }
                return *this;
            }
            
            #if defined(NK_CPP11)
            NkRingBuffer& operator=(NkRingBuffer&& other) NK_NOEXCEPT {
                if (this != &other) {
                    Clear();
                    if (mData) mAllocator->Deallocate(mData);
                    
                    mData = other.mData;
                    mCapacity = other.mCapacity;
                    mHead = other.mHead;
                    mTail = other.mTail;
                    mSize = other.mSize;
                    mAllocator = other.mAllocator;
                    
                    other.mData = nullptr;
                    other.mCapacity = 0;
                    other.mHead = 0;
                    other.mTail = 0;
                    other.mSize = 0;
                }
                return *this;
            }
            #endif
            
            // ========================================
            // ELEMENT ACCESS
            // ========================================
            
            Reference Front() {
                NK_ASSERT(!Empty());
                return mData[mTail];
            }
            
            ConstReference Front() const {
                NK_ASSERT(!Empty());
                return mData[mTail];
            }
            
            Reference Back() {
                NK_ASSERT(!Empty());
                SizeType idx = (mHead + mCapacity - 1) % mCapacity;
                return mData[idx];
            }
            
            ConstReference Back() const {
                NK_ASSERT(!Empty());
                SizeType idx = (mHead + mCapacity - 1) % mCapacity;
                return mData[idx];
            }
            
            Reference operator[](SizeType index) {
                NK_ASSERT(index < mSize);
                SizeType idx = (mTail + index) % mCapacity;
                return mData[idx];
            }
            
            ConstReference operator[](SizeType index) const {
                NK_ASSERT(index < mSize);
                SizeType idx = (mTail + index) % mCapacity;
                return mData[idx];
            }
            
            // ========================================
            // CAPACITY
            // ========================================
            
            bool Empty() const NK_NOEXCEPT { return mSize == 0; }
            bool IsFull() const NK_NOEXCEPT { return mSize == mCapacity; }
            SizeType Size() const NK_NOEXCEPT { return mSize; }
            SizeType Capacity() const NK_NOEXCEPT { return mCapacity; }
            
            // ========================================
            // MODIFIERS
            // ========================================
            
            void Clear() {
                while (!Empty()) {
                    Pop();
                }
            }
            
            /**
             * @brief Push (ajoute à la fin)
             * Si plein, écrase le plus ancien élément
             */
            void Push(const T& value) {
                if (IsFull()) {
                    // Écrase l'ancien
                    mData[mHead].~T();
                    new (&mData[mHead]) T(value);
                    mHead = NextIndex(mHead);
                    mTail = NextIndex(mTail);
                } else {
                    new (&mData[mHead]) T(value);
                    mHead = NextIndex(mHead);
                    ++mSize;
                }
            }
            
            #if defined(NK_CPP11)
            void Push(T&& value) {
                if (IsFull()) {
                    mData[mHead].~T();
                    new (&mData[mHead]) T(traits::NkMove(value));
                    mHead = NextIndex(mHead);
                    mTail = NextIndex(mTail);
                } else {
                    new (&mData[mHead]) T(traits::NkMove(value));
                    mHead = NextIndex(mHead);
                    ++mSize;
                }
            }
            
            template<typename... Args>
            void Emplace(Args&&... args) {
                if (IsFull()) {
                    mData[mHead].~T();
                    new (&mData[mHead]) T(traits::NkForward<Args>(args)...);
                    mHead = NextIndex(mHead);
                    mTail = NextIndex(mTail);
                } else {
                    new (&mData[mHead]) T(traits::NkForward<Args>(args)...);
                    mHead = NextIndex(mHead);
                    ++mSize;
                }
            }
            #endif
            
            /**
             * @brief Pop (retire du début)
             * Retourne la valeur retirée
             */
            T Pop() {
                NK_ASSERT(!Empty());
                T value = mData[mTail];
                mData[mTail].~T();
                mTail = NextIndex(mTail);
                --mSize;
                return value;
            }
            
            /**
             * @brief Pop sans retourner la valeur (plus rapide)
             */
            void PopDiscard() {
                NK_ASSERT(!Empty());
                mData[mTail].~T();
                mTail = NextIndex(mTail);
                --mSize;
            }
            
            void Swap(NkRingBuffer& other) NK_NOEXCEPT {
                traits::NkSwap(mData, other.mData);
                traits::NkSwap(mCapacity, other.mCapacity);
                traits::NkSwap(mHead, other.mHead);
                traits::NkSwap(mTail, other.mTail);
                traits::NkSwap(mSize, other.mSize);
                traits::NkSwap(mAllocator, other.mAllocator);
            }
        };
        
        // ========================================
        // NON-MEMBER FUNCTIONS
        // ========================================
        
        template<typename T, typename Allocator>
        void NkSwap(NkRingBuffer<T, Allocator>& lhs, NkRingBuffer<T, Allocator>& rhs) NK_NOEXCEPT {
            lhs.Swap(rhs);
        }
        
    
} // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_CACHEFRIENDLY_NKRINGBUFFER_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-05 22:26:13
// Creation Date: 2026-02-05 22:26:13
// ============================================================
