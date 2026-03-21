// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\Containers\Sequential\NkDeque.h
// DESCRIPTION: Double-ended queue (deque) - Chunk-based implementation
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_SEQUENTIAL_NKDEQUE_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_SEQUENTIAL_NKDEQUE_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKContainers/NkContainersExport.h"
#include "NKCore/NkTraits.h"
#include "NKMemory/NkAllocator.h"
#include "NKMemory/NkFunction.h"
#include "NKCore/Assert/NkAssert.h"
#include "NKContainers/Iterators/NkIterator.h"
#include "NKContainers/Iterators/NkInitializerList.h"

namespace nkentseu {
    
        
        /**
         * @brief Double-ended queue (STL::deque equivalent)
         * 
         * Implémentation par chunks (blocks). Combine les avantages de:
         * - Vector: Accès random O(1)
         * - List: Insert/Erase en tête/queue O(1)
         * 
         * Structure: Array de pointeurs vers des chunks de données.
         * Chaque chunk contient CHUNK_SIZE éléments.
         * 
         * Complexité:
         * - PushFront/PushBack: O(1) amortized
         * - PopFront/PopBack: O(1)
         * - Accès: O(1)
         * - Insert/Erase au milieu: O(n)
         * 
         * @example
         * NkDeque<int> deque = {1, 2, 3};
         * deque.PushFront(0);
         * deque.PushBack(4);
         * int val = deque[2];  // O(1) random access
         */
        template<typename T, typename Allocator = memory::NkAllocator>
        class NkDeque {
        private:
            static constexpr usize CHUNK_SIZE = 64;  // Elements per chunk
            
            struct Chunk {
                T Data[CHUNK_SIZE];
            };
            
        public:
            using ValueType = T;
            using SizeType = usize;
            using Reference = T&;
            using ConstReference = const T&;
            using Pointer = T*;
            using ConstPointer = const T*;
            
            // Random access iterator
            class Iterator {
            private:
                NkDeque* mDeque;
                SizeType mIndex;
                friend class NkDeque;
                
            public:
                using ValueType = T;
                using Reference = T&;
                using Pointer = T*;
                using DifferenceType = intptr;
                using IteratorCategory = NkRandomAccessIteratorTag;
                
                Iterator() : mDeque(nullptr), mIndex(0) {}
                Iterator(NkDeque* deque, SizeType index) : mDeque(deque), mIndex(index) {}
                
                Reference operator*() const { return (*mDeque)[mIndex]; }
                Pointer operator->() const { return &(*mDeque)[mIndex]; }
                
                Iterator& operator++() { ++mIndex; return *this; }
                Iterator operator++(int) { Iterator tmp = *this; ++mIndex; return tmp; }
                Iterator& operator--() { --mIndex; return *this; }
                Iterator operator--(int) { Iterator tmp = *this; --mIndex; return tmp; }
                
                Iterator& operator+=(DifferenceType n) { mIndex += n; return *this; }
                Iterator& operator-=(DifferenceType n) { mIndex -= n; return *this; }
                
                Iterator operator+(DifferenceType n) const { return Iterator(mDeque, mIndex + n); }
                Iterator operator-(DifferenceType n) const { return Iterator(mDeque, mIndex - n); }
                
                DifferenceType operator-(const Iterator& other) const { return mIndex - other.mIndex; }
                
                Reference operator[](DifferenceType n) const { return (*mDeque)[mIndex + n]; }
                
                bool operator==(const Iterator& o) const { return mIndex == o.mIndex; }
                bool operator!=(const Iterator& o) const { return mIndex != o.mIndex; }
                bool operator<(const Iterator& o) const { return mIndex < o.mIndex; }
                bool operator<=(const Iterator& o) const { return mIndex <= o.mIndex; }
                bool operator>(const Iterator& o) const { return mIndex > o.mIndex; }
                bool operator>=(const Iterator& o) const { return mIndex >= o.mIndex; }
            };
            
            class ConstIterator {
            private:
                const NkDeque* mDeque;
                SizeType mIndex;
                friend class NkDeque;
                
            public:
                using ValueType = T;
                using Reference = const T&;
                using Pointer = const T*;
                using DifferenceType = intptr;
                using IteratorCategory = NkRandomAccessIteratorTag;
                
                ConstIterator() : mDeque(nullptr), mIndex(0) {}
                ConstIterator(const NkDeque* deque, SizeType index) : mDeque(deque), mIndex(index) {}
                ConstIterator(const Iterator& it) : mDeque(it.mDeque), mIndex(it.mIndex) {}
                
                Reference operator*() const { return (*mDeque)[mIndex]; }
                Pointer operator->() const { return &(*mDeque)[mIndex]; }
                
                ConstIterator& operator++() { ++mIndex; return *this; }
                ConstIterator operator++(int) { ConstIterator tmp = *this; ++mIndex; return tmp; }
                ConstIterator& operator--() { --mIndex; return *this; }
                ConstIterator operator--(int) { ConstIterator tmp = *this; --mIndex; return tmp; }
                
                ConstIterator& operator+=(DifferenceType n) { mIndex += n; return *this; }
                ConstIterator& operator-=(DifferenceType n) { mIndex -= n; return *this; }
                
                ConstIterator operator+(DifferenceType n) const { return ConstIterator(mDeque, mIndex + n); }
                ConstIterator operator-(DifferenceType n) const { return ConstIterator(mDeque, mIndex - n); }
                
                DifferenceType operator-(const ConstIterator& other) const { return mIndex - other.mIndex; }
                
                Reference operator[](DifferenceType n) const { return (*mDeque)[mIndex + n]; }
                
                bool operator==(const ConstIterator& o) const { return mIndex == o.mIndex; }
                bool operator!=(const ConstIterator& o) const { return mIndex != o.mIndex; }
                bool operator<(const ConstIterator& o) const { return mIndex < o.mIndex; }
                bool operator<=(const ConstIterator& o) const { return mIndex <= o.mIndex; }
                bool operator>(const ConstIterator& o) const { return mIndex > o.mIndex; }
                bool operator>=(const ConstIterator& o) const { return mIndex >= o.mIndex; }
            };
            
            using ReverseIterator = NkReverseIterator<Iterator>;
            using ConstReverseIterator = NkReverseIterator<ConstIterator>;
            
        private:
            Chunk** mChunks;        // Array de pointeurs vers chunks
            SizeType mChunkCount;   // Nombre de chunks alloués
            SizeType mChunkCapacity; // Capacité du array de chunks
            SizeType mFrontChunk;   // Index du premier chunk utilisé
            SizeType mFrontOffset;  // Offset dans le premier chunk
            SizeType mSize;
            Allocator* mAllocator;
            
            void EnsureChunkArrayCapacity(SizeType required) {
                if (required <= mChunkCapacity) {
                    return;
                }
                
                SizeType newCapacity = mChunkCapacity == 0 ? 4 : mChunkCapacity * 2;
                if (newCapacity < required) {
                    newCapacity = required;
                }
                
                Chunk** newChunks = static_cast<Chunk**>(mAllocator->Allocate(newCapacity * sizeof(Chunk*), alignof(Chunk*)));
                NK_ASSERT(newChunks != nullptr);
                memory::NkMemZero(newChunks, newCapacity * sizeof(Chunk*));
                
                if (mChunks) {
                    memory::NkMemCopy(newChunks, mChunks, mChunkCount * sizeof(Chunk*));
                    mAllocator->Deallocate(mChunks);
                }
                
                mChunks = newChunks;
                mChunkCapacity = newCapacity;
            }
            
            Chunk* AllocateChunkBlock() {
                Chunk* chunk = static_cast<Chunk*>(mAllocator->Allocate(sizeof(Chunk), alignof(Chunk)));
                NK_ASSERT(chunk != nullptr);
                return chunk;
            }
            
            void AllocateChunk() {
                EnsureChunkArrayCapacity(mChunkCount + 1);
                mChunks[mChunkCount] = AllocateChunkBlock();
                ++mChunkCount;
            }
            
            void ReleaseChunkStorage() {
                if (!mChunks) {
                    mChunkCount = 0;
                    mChunkCapacity = 0;
                    mFrontChunk = 0;
                    mFrontOffset = 0;
                    return;
                }
                
                for (SizeType i = 0; i < mChunkCount; ++i) {
                    if (mChunks[i]) {
                        mAllocator->Deallocate(mChunks[i]);
                        mChunks[i] = nullptr;
                    }
                }
                mAllocator->Deallocate(mChunks);
                mChunks = nullptr;
                mChunkCount = 0;
                mChunkCapacity = 0;
                mFrontChunk = 0;
                mFrontOffset = 0;
            }
            
            void GetChunkAndOffset(SizeType index, SizeType& chunkIdx, SizeType& offset) const {
                SizeType globalIndex = mFrontOffset + index;
                chunkIdx = mFrontChunk + (globalIndex / CHUNK_SIZE);
                offset = globalIndex % CHUNK_SIZE;
            }
            
        public:
            // Constructors
            explicit NkDeque(Allocator* allocator = nullptr)
                : mChunks(nullptr), mChunkCount(0), mChunkCapacity(0)
                , mFrontChunk(0), mFrontOffset(0), mSize(0)
                , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator()) {
            }
            
            NkDeque(NkInitializerList<T> init, Allocator* allocator = nullptr)
                : mChunks(nullptr), mChunkCount(0), mChunkCapacity(0)
                , mFrontChunk(0), mFrontOffset(0), mSize(0)
                , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator()) {
                for (auto& val : init) PushBack(val);
            }

            NkDeque(std::initializer_list<T> init, Allocator* allocator = nullptr)
                : mChunks(nullptr), mChunkCount(0), mChunkCapacity(0)
                , mFrontChunk(0), mFrontOffset(0), mSize(0)
                , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator()) {
                for (auto& val : init) PushBack(val);
            }
            
            NkDeque(const NkDeque& other)
                : mChunks(nullptr)
                , mChunkCount(0)
                , mChunkCapacity(0)
                , mFrontChunk(0)
                , mFrontOffset(0)
                , mSize(0)
                , mAllocator(other.mAllocator) {
                for (SizeType i = 0; i < other.mSize; ++i) {
                    PushBack(other[i]);
                }
            }
            
            #if defined(NK_CPP11)
            NkDeque(NkDeque&& other) NK_NOEXCEPT
                : mChunks(other.mChunks)
                , mChunkCount(other.mChunkCount)
                , mChunkCapacity(other.mChunkCapacity)
                , mFrontChunk(other.mFrontChunk)
                , mFrontOffset(other.mFrontOffset)
                , mSize(other.mSize)
                , mAllocator(other.mAllocator) {
                other.mChunks = nullptr;
                other.mChunkCount = 0;
                other.mChunkCapacity = 0;
                other.mFrontChunk = 0;
                other.mFrontOffset = 0;
                other.mSize = 0;
            }
            #endif
            
            ~NkDeque() {
                Clear();
                ReleaseChunkStorage();
            }
            
            NkDeque& operator=(const NkDeque& other) {
                if (this == &other) {
                    return *this;
                }
                Clear();
                ReleaseChunkStorage();
                mAllocator = other.mAllocator;
                for (SizeType i = 0; i < other.mSize; ++i) {
                    PushBack(other[i]);
                }
                return *this;
            }
            
            #if defined(NK_CPP11)
            NkDeque& operator=(NkDeque&& other) NK_NOEXCEPT {
                if (this == &other) {
                    return *this;
                }
                Clear();
                ReleaseChunkStorage();

                mChunks = other.mChunks;
                mChunkCount = other.mChunkCount;
                mChunkCapacity = other.mChunkCapacity;
                mFrontChunk = other.mFrontChunk;
                mFrontOffset = other.mFrontOffset;
                mSize = other.mSize;
                mAllocator = other.mAllocator;

                other.mChunks = nullptr;
                other.mChunkCount = 0;
                other.mChunkCapacity = 0;
                other.mFrontChunk = 0;
                other.mFrontOffset = 0;
                other.mSize = 0;
                return *this;
            }
            #endif

            NkDeque& operator=(NkInitializerList<T> init) {
                Clear();
                ReleaseChunkStorage();
                for (auto& val : init) PushBack(val);
                return *this;
            }

            NkDeque& operator=(std::initializer_list<T> init) {
                Clear();
                ReleaseChunkStorage();
                for (auto& val : init) PushBack(val);
                return *this;
            }

            // Element access
            Reference operator[](SizeType index) {
                NK_ASSERT(index < mSize);
                SizeType chunkIdx, offset;
                GetChunkAndOffset(index, chunkIdx, offset);
                return mChunks[chunkIdx]->Data[offset];
            }
            
            ConstReference operator[](SizeType index) const {
                NK_ASSERT(index < mSize);
                SizeType chunkIdx, offset;
                GetChunkAndOffset(index, chunkIdx, offset);
                return mChunks[chunkIdx]->Data[offset];
            }
            
            Reference Front() { NK_ASSERT(mSize > 0); return (*this)[0]; }
            ConstReference Front() const { NK_ASSERT(mSize > 0); return (*this)[0]; }
            Reference Back() { NK_ASSERT(mSize > 0); return (*this)[mSize - 1]; }
            ConstReference Back() const { NK_ASSERT(mSize > 0); return (*this)[mSize - 1]; }
            
            // Iterators
            Iterator begin() { return Iterator(this, 0); }
            ConstIterator begin() const { return ConstIterator(this, 0); }
            Iterator end() { return Iterator(this, mSize); }
            ConstIterator end() const { return ConstIterator(this, mSize); }
            
            ReverseIterator rbegin() { return ReverseIterator(end()); }
            ConstReverseIterator rbegin() const { return ConstReverseIterator(end()); }
            ReverseIterator rend() { return ReverseIterator(begin()); }
            ConstReverseIterator rend() const { return ConstReverseIterator(begin()); }
            
            // Capacity
            bool Empty() const NK_NOEXCEPT { return mSize == 0; }
            SizeType Size() const NK_NOEXCEPT { return mSize; }
            
            // Modifiers
            void Clear() {
                while (!Empty()) PopBack();
            }
            
            void PushBack(const T& value) {
                if (mSize == 0) {
                    if (mChunkCount == 0) AllocateChunk();
                    mFrontChunk = 0;
                    mFrontOffset = CHUNK_SIZE / 2;  // Start in middle
                }
                
                SizeType chunkIdx, offset;
                GetChunkAndOffset(mSize, chunkIdx, offset);
                
                if (chunkIdx >= mChunkCount) AllocateChunk();
                
                new (&mChunks[chunkIdx]->Data[offset]) T(value);
                ++mSize;
            }
            
            void PushFront(const T& value) {
                if (mSize == 0) {
                    PushBack(value);
                    return;
                }
                
                if (mFrontOffset == 0) {
                    if (mFrontChunk == 0) {
                        EnsureChunkArrayCapacity(mChunkCount + 1);
                        for (SizeType i = mChunkCount; i > 0; --i) {
                            mChunks[i] = mChunks[i - 1];
                        }
                        mChunks[0] = AllocateChunkBlock();
                        ++mChunkCount;
                        mFrontChunk = 0;
                    } else {
                        --mFrontChunk;
                    }
                    mFrontOffset = CHUNK_SIZE - 1;
                } else {
                    --mFrontOffset;
                }
                
                new (&mChunks[mFrontChunk]->Data[mFrontOffset]) T(value);
                ++mSize;
            }
            
            void PopBack() {
                NK_ASSERT(mSize > 0);
                SizeType chunkIdx, offset;
                GetChunkAndOffset(mSize - 1, chunkIdx, offset);
                mChunks[chunkIdx]->Data[offset].~T();
                --mSize;
                if (mSize == 0) {
                    mFrontChunk = 0;
                    mFrontOffset = 0;
                }
            }
            
            void PopFront() {
                NK_ASSERT(mSize > 0);
                mChunks[mFrontChunk]->Data[mFrontOffset].~T();
                ++mFrontOffset;
                if (mFrontOffset >= CHUNK_SIZE) {
                    ++mFrontChunk;
                    mFrontOffset = 0;
                }
                --mSize;
                if (mSize == 0) {
                    mFrontChunk = 0;
                    mFrontOffset = 0;
                }
            }
        };
        
    
} // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_SEQUENTIAL_NKDEQUE_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-05 22:26:13
// Creation Date: 2026-02-05 22:26:13
// ============================================================
