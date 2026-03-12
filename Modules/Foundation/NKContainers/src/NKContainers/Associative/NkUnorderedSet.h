// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\Containers\Associative\NkUnorderedSet.h
// DESCRIPTION: Unordered set - Hash set implementation
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ASSOCIATIVE_NKUNORDEREDSET_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ASSOCIATIVE_NKUNORDEREDSET_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKContainers/NkContainersExport.h"
#include "NKCore/NkTraits.h"
#include "NKMemory/NkAllocator.h"
#include "NKMemory/NkFunction.h"
#include "NKCore/Assert/NkAssert.h"
#include "NKContainers/Iterators/NkIterator.h"
#include "NKContainers/Iterators/NkInitializerList.h"

namespace nkentseu {
    
        template<typename T>
        struct NkUnorderedSetDefaultHasher {
            usize operator()(const T& value) const {
                const nk_uint8* data = reinterpret_cast<const nk_uint8*>(&value);
                usize hash = static_cast<usize>(1469598103934665603ull);
                for (usize i = 0; i < sizeof(T); ++i) {
                    hash ^= static_cast<usize>(data[i]);
                    hash *= static_cast<usize>(1099511628211ull);
                }
                return hash;
            }
        };
        
        template<typename T>
        struct NkUnorderedSetDefaultEqual {
            bool operator()(const T& lhs, const T& rhs) const {
                return lhs == rhs;
            }
        };
        
        
        /**
         * @brief Unordered set - STL::unordered_set equivalent
         * 
         * Hash set avec separate chaining.
         * Stocke des éléments uniques sans ordre particulier.
         * 
         * Complexité:
         * - Insert/Find/Erase: O(1) average, O(n) worst
         * - No ordering
         * 
         * @example
         * NkUnorderedSet<int> set = {1, 2, 3, 1, 2};
         * // Stored: {1, 2, 3} (unique, unordered)
         * 
         * set.Insert(4);
         * if (set.Contains(2)) { }
         */
        template<typename T,
                 typename Allocator = memory::NkAllocator,
                 typename Hasher = NkUnorderedSetDefaultHasher<T>,
                 typename Equal = NkUnorderedSetDefaultEqual<T>>
        class NkUnorderedSet {
        private:
            struct Node {
                T Value;
                Node* Next;
                usize Hash;
                
                Node(usize hash, const T& value, Node* next = nullptr)
                    : Value(value), Next(next), Hash(hash) {
                }
            };
            
        public:
            using ValueType = T;
            using SizeType = usize;
            using Reference = const T&;
            using ConstReference = const T&;
            
        private:
            Node** mBuckets;
            SizeType mBucketCount;
            SizeType mSize;
            float mMaxLoadFactor;
            Allocator* mAllocator;
            Hasher mHasher;
            Equal mEqual;
            
            void InitBuckets(SizeType bucketCount) {
                mBucketCount = bucketCount > 0 ? bucketCount : 1;
                mBuckets = static_cast<Node**>(mAllocator->Allocate(mBucketCount * sizeof(Node*), alignof(Node*)));
                NK_ASSERT(mBuckets != nullptr);
                memory::NkMemZero(mBuckets, mBucketCount * sizeof(Node*));
            }
            
            usize HashValue(const T& value) const {
                return mHasher(value);
            }
            
            usize GetBucketIndex(usize hash) const {
                return hash % mBucketCount;
            }
            
            void Rehash(SizeType newBucketCount) {
                Node** newBuckets = static_cast<Node**>(mAllocator->Allocate(newBucketCount * sizeof(Node*)));
                memory::NkMemZero(newBuckets, newBucketCount * sizeof(Node*));
                
                for (SizeType i = 0; i < mBucketCount; ++i) {
                    Node* node = mBuckets[i];
                    while (node) {
                        Node* next = node->Next;
                        SizeType newIdx = node->Hash % newBucketCount;
                        node->Next = newBuckets[newIdx];
                        newBuckets[newIdx] = node;
                        node = next;
                    }
                }
                
                mAllocator->Deallocate(mBuckets);
                mBuckets = newBuckets;
                mBucketCount = newBucketCount;
            }
            
            void CheckLoadFactor() {
                if (mSize > mBucketCount * mMaxLoadFactor) {
                    Rehash(mBucketCount * 2);
                }
            }
            
        public:
            // Constructors
            explicit NkUnorderedSet(Allocator* allocator = nullptr)
                : mBuckets(nullptr), mBucketCount(16), mSize(0)
                , mMaxLoadFactor(0.75f)
                , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator())
                , mHasher()
                , mEqual() {
                InitBuckets(16);
            }
            
            NkUnorderedSet(NkInitializerList<T> init, Allocator* allocator = nullptr)
                : mBuckets(nullptr), mBucketCount(16), mSize(0)
                , mMaxLoadFactor(0.75f)
                , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator())
                , mHasher()
                , mEqual() {
                InitBuckets(16);
                for (auto& val : init) Insert(val);
            }
            
            NkUnorderedSet(const NkUnorderedSet& other)
                : mBuckets(nullptr)
                , mBucketCount(0)
                , mSize(0)
                , mMaxLoadFactor(other.mMaxLoadFactor)
                , mAllocator(other.mAllocator)
                , mHasher(other.mHasher)
                , mEqual(other.mEqual) {
                InitBuckets(other.mBucketCount);
                for (SizeType i = 0; i < other.mBucketCount; ++i) {
                    Node* node = other.mBuckets[i];
                    while (node) {
                        Insert(node->Value);
                        node = node->Next;
                    }
                }
            }
            
            #if defined(NK_CPP11)
            NkUnorderedSet(NkUnorderedSet&& other) NK_NOEXCEPT
                : mBuckets(other.mBuckets)
                , mBucketCount(other.mBucketCount)
                , mSize(other.mSize)
                , mMaxLoadFactor(other.mMaxLoadFactor)
                , mAllocator(other.mAllocator)
                , mHasher(traits::NkMove(other.mHasher))
                , mEqual(traits::NkMove(other.mEqual)) {
                other.mBuckets = nullptr;
                other.mBucketCount = 0;
                other.mSize = 0;
            }
            #endif
            
            ~NkUnorderedSet() {
                Clear();
                if (mBuckets) mAllocator->Deallocate(mBuckets);
            }
            
            NkUnorderedSet& operator=(const NkUnorderedSet& other) {
                if (this == &other) {
                    return *this;
                }
                Clear();
                if (mBuckets) {
                    mAllocator->Deallocate(mBuckets);
                }
                mAllocator = other.mAllocator;
                mMaxLoadFactor = other.mMaxLoadFactor;
                mHasher = other.mHasher;
                mEqual = other.mEqual;
                InitBuckets(other.mBucketCount);
                for (SizeType i = 0; i < other.mBucketCount; ++i) {
                    Node* node = other.mBuckets[i];
                    while (node) {
                        Insert(node->Value);
                        node = node->Next;
                    }
                }
                return *this;
            }
            
            #if defined(NK_CPP11)
            NkUnorderedSet& operator=(NkUnorderedSet&& other) NK_NOEXCEPT {
                if (this == &other) {
                    return *this;
                }
                Clear();
                if (mBuckets) {
                    mAllocator->Deallocate(mBuckets);
                }
                mBuckets = other.mBuckets;
                mBucketCount = other.mBucketCount;
                mSize = other.mSize;
                mMaxLoadFactor = other.mMaxLoadFactor;
                mAllocator = other.mAllocator;
                mHasher = traits::NkMove(other.mHasher);
                mEqual = traits::NkMove(other.mEqual);
                
                other.mBuckets = nullptr;
                other.mBucketCount = 0;
                other.mSize = 0;
                return *this;
            }
            #endif
            
            // Capacity
            bool Empty() const NK_NOEXCEPT { return mSize == 0; }
            SizeType Size() const NK_NOEXCEPT { return mSize; }
            
            // Modifiers
            void Clear() {
                for (SizeType i = 0; i < mBucketCount; ++i) {
                    Node* node = mBuckets[i];
                    while (node) {
                        Node* next = node->Next;
                        node->~Node();
                        mAllocator->Deallocate(node);
                        node = next;
                    }
                    mBuckets[i] = nullptr;
                }
                mSize = 0;
            }
            
            bool Insert(const T& value) {
                usize hash = HashValue(value);
                SizeType idx = GetBucketIndex(hash);
                
                // Check if exists
                Node* node = mBuckets[idx];
                while (node) {
                    if (mEqual(node->Value, value)) {
                        return false;  // Duplicate
                    }
                    node = node->Next;
                }
                
                // Insert new
                Node* newNode = static_cast<Node*>(mAllocator->Allocate(sizeof(Node)));
                new (newNode) Node(hash, value, mBuckets[idx]);
                mBuckets[idx] = newNode;
                ++mSize;
                
                CheckLoadFactor();
                return true;
            }
            
            bool Erase(const T& value) {
                usize hash = HashValue(value);
                SizeType idx = GetBucketIndex(hash);
                
                Node** prev = &mBuckets[idx];
                Node* node = mBuckets[idx];
                
                while (node) {
                    if (mEqual(node->Value, value)) {
                        *prev = node->Next;
                        node->~Node();
                        mAllocator->Deallocate(node);
                        --mSize;
                        return true;
                    }
                    prev = &node->Next;
                    node = node->Next;
                }
                
                return false;
            }
            
            bool Contains(const T& value) const {
                usize hash = HashValue(value);
                SizeType idx = GetBucketIndex(hash);
                
                Node* node = mBuckets[idx];
                while (node) {
                    if (mEqual(node->Value, value)) {
                        return true;
                    }
                    node = node->Next;
                }
                
                return false;
            }
        };
        
    
} // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ASSOCIATIVE_NKUNORDEREDSET_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-05 22:26:13
// Creation Date: 2026-02-05 22:26:13
// ============================================================
