// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\Containers\Associative\NkUnorderedMap.h
// DESCRIPTION: Unordered map - Hash map for key-value pairs
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ASSOCIATIVE_NKUNORDEREDMAP_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ASSOCIATIVE_NKUNORDEREDMAP_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKContainers/NkContainersExport.h"
#include "NKCore/NkTraits.h"
#include "NKMemory/NkAllocator.h"
#include "NKMemory/NkFunction.h"
#include "NKCore/Assert/NkAssert.h"
#include "NKContainers/Heterogeneous/NkPair.h"
#include "NKContainers/Iterators/NkIterator.h"
#include "NKContainers/Iterators/NkInitializerList.h"

namespace nkentseu {
    
        template<typename Key>
        struct NkUnorderedMapDefaultHasher {
            usize operator()(const Key& key) const {
                const nk_uint8* data = reinterpret_cast<const nk_uint8*>(&key);
                usize hash = static_cast<usize>(1469598103934665603ull);
                for (usize i = 0; i < sizeof(Key); ++i) {
                    hash ^= static_cast<usize>(data[i]);
                    hash *= static_cast<usize>(1099511628211ull);
                }
                return hash;
            }
        };
        
        template<typename Key>
        struct NkUnorderedMapDefaultEqual {
            bool operator()(const Key& lhs, const Key& rhs) const {
                return lhs == rhs;
            }
        };
        
        
        /**
         * @brief Unordered map - NkUnorderedMap equivalent
         * 
         * Hash map avec separate chaining.
         * Comme NkHashMap mais avec API NkUnorderedMap.
         * 
         * Complexité:
         * - Insert/Find/Erase: O(1) average, O(n) worst
         * 
         * @example
         * NkUnorderedMap<int, NkString> map = {
         *     {1, "one"},
         *     {2, "two"}
         * };
         * map[3] = "three";
         */
        template<typename Key,
                 typename Value,
                 typename Allocator = memory::NkAllocator,
                 typename Hasher = NkUnorderedMapDefaultHasher<Key>,
                 typename KeyEqual = NkUnorderedMapDefaultEqual<Key>>
        class NkUnorderedMap {
        private:
            struct Node {
                NkPair<const Key, Value> Data;
                Node* Next;
                usize Hash;
                
                #if defined(NK_CPP11)
                template<typename K, typename V>
                Node(usize hash, K&& key, V&& value, Node* next = nullptr)
                    : Data(traits::NkForward<K>(key), traits::NkForward<V>(value))
                    , Next(next)
                    , Hash(hash) {
                }
                #else
                Node(usize hash, const Key& key, const Value& value, Node* next = nullptr)
                    : Data(key, value), Next(next), Hash(hash) {
                }
                #endif
            };
            
        public:
            using KeyType = Key;
            using MappedType = Value;
            using ValueType = NkPair<const Key, Value>;
            using SizeType = usize;
            using Reference = ValueType&;
            using ConstReference = const ValueType&;
            
        private:
            Node** mBuckets;
            SizeType mBucketCount;
            SizeType mSize;
            float mMaxLoadFactor;
            Allocator* mAllocator;
            Hasher mHasher;
            KeyEqual mEqual;
            
            void InitBuckets(SizeType bucketCount) {
                mBucketCount = bucketCount > 0 ? bucketCount : 1;
                mBuckets = static_cast<Node**>(mAllocator->Allocate(mBucketCount * sizeof(Node*), alignof(Node*)));
                NK_ASSERT(mBuckets != nullptr);
                memory::NkMemZero(mBuckets, mBucketCount * sizeof(Node*));
            }
            
            usize HashKey(const Key& key) const {
                return mHasher(key);
            }
            
            usize GetBucketIndex(usize hash) const {
                return hash % mBucketCount;
            }
            
            void CheckLoadFactor() {
                if (mSize > mBucketCount * mMaxLoadFactor) {
                    Rehash(mBucketCount * 2);
                }
            }
            
        public:
            // Constructors
            explicit NkUnorderedMap(Allocator* allocator = nullptr)
                : mBuckets(nullptr), mBucketCount(16), mSize(0)
                , mMaxLoadFactor(0.75f)
                , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator())
                , mHasher()
                , mEqual() {
                InitBuckets(16);
            }
            
            NkUnorderedMap(NkInitializerList<ValueType> init, Allocator* allocator = nullptr)
                : mBuckets(nullptr), mBucketCount(16), mSize(0)
                , mMaxLoadFactor(0.75f)
                , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator())
                , mHasher()
                , mEqual() {
                InitBuckets(16);
                for (auto& pair : init) {
                    Insert(pair.First, pair.Second);
                }
            }
            
            NkUnorderedMap(const NkUnorderedMap& other)
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
                        Insert(node->Data.First, node->Data.Second);
                        node = node->Next;
                    }
                }
            }
            
            #if defined(NK_CPP11)
            NkUnorderedMap(NkUnorderedMap&& other) NK_NOEXCEPT
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
            
            ~NkUnorderedMap() {
                Clear();
                if (mBuckets) mAllocator->Deallocate(mBuckets);
            }
            
            NkUnorderedMap& operator=(const NkUnorderedMap& other) {
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
                        Insert(node->Data.First, node->Data.Second);
                        node = node->Next;
                    }
                }
                return *this;
            }
            
            #if defined(NK_CPP11)
            NkUnorderedMap& operator=(NkUnorderedMap&& other) NK_NOEXCEPT {
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
            
            void Rehash(SizeType newBucketCount) {
                if (newBucketCount < 1) {
                    newBucketCount = 1;
                }
                Node** newBuckets = static_cast<Node**>(mAllocator->Allocate(newBucketCount * sizeof(Node*), alignof(Node*)));
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
            
            void Insert(const Key& key, const Value& value) {
                usize hash = HashKey(key);
                SizeType idx = GetBucketIndex(hash);
                
                // Check if exists
                Node* node = mBuckets[idx];
                while (node) {
                    if (mEqual(node->Data.First, key)) {
                        node->Data.Second = value;
                        return;
                    }
                    node = node->Next;
                }
                
                // Insert new
                Node* newNode = static_cast<Node*>(mAllocator->Allocate(sizeof(Node), alignof(Node)));
                new (newNode) Node(hash, key, value, mBuckets[idx]);
                mBuckets[idx] = newNode;
                ++mSize;
                
                CheckLoadFactor();
            }
            
            bool Erase(const Key& key) {
                usize hash = HashKey(key);
                SizeType idx = GetBucketIndex(hash);
                
                Node** prev = &mBuckets[idx];
                Node* node = mBuckets[idx];
                
                while (node) {
                    if (mEqual(node->Data.First, key)) {
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
            
            // Lookup
            Value* Find(const Key& key) {
                usize hash = HashKey(key);
                SizeType idx = GetBucketIndex(hash);
                
                Node* node = mBuckets[idx];
                while (node) {
                    if (mEqual(node->Data.First, key)) {
                        return &node->Data.Second;
                    }
                    node = node->Next;
                }
                
                return nullptr;
            }
            
            const Value* Find(const Key& key) const {
                usize hash = HashKey(key);
                SizeType idx = GetBucketIndex(hash);
                
                Node* node = mBuckets[idx];
                while (node) {
                    if (mEqual(node->Data.First, key)) {
                        return &node->Data.Second;
                    }
                    node = node->Next;
                }
                
                return nullptr;
            }
            
            bool Contains(const Key& key) const {
                return Find(key) != nullptr;
            }
            
            // operator[]
            Value& operator[](const Key& key) {
                Value* val = Find(key);
                if (val) return *val;
                
                Insert(key, Value());
                return *Find(key);
            }

            SizeType BucketCount() const NK_NOEXCEPT { return mBucketCount; }
            float LoadFactor() const NK_NOEXCEPT {
                return mBucketCount > 0 ? (mSize / static_cast<float>(mBucketCount)) : 0.0f;
            }

            // Iteration: ForEach(fn) where fn receives (const Key&, Value&) or (const Key&, const Value&)
            template<typename Fn>
            void ForEach(Fn&& fn) {
                for (SizeType i = 0; i < mBucketCount; ++i) {
                    Node* node = mBuckets[i];
                    while (node) {
                        fn(node->Data.First, node->Data.Second);
                        node = node->Next;
                    }
                }
            }

            template<typename Fn>
            void ForEach(Fn&& fn) const {
                for (SizeType i = 0; i < mBucketCount; ++i) {
                    Node* node = mBuckets[i];
                    while (node) {
                        fn(node->Data.First, node->Data.Second);
                        node = node->Next;
                    }
                }
            }
        };
        
    
} // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ASSOCIATIVE_NKUNORDEREDMAP_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// ============================================================

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-05 22:26:13
// Creation Date: 2026-02-05 22:26:13
// ============================================================
