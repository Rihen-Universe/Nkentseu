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
#include "NKMemory/NkMemoryFn.h"
#include "NKCore/Assert/NkAssert.h"
#include "NKContainers/Heterogeneous/NkPair.h"
#include "NKContainers/Iterators/NkIterator.h"
#include "NKContainers/Iterators/NkInitializerList.h"

namespace nkentseu {
    namespace core {
        
        /**
         * @brief Unordered map - std::unordered_map equivalent
         * 
         * Hash map avec separate chaining.
         * Comme NkHashMap mais avec API std::unordered_map.
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
        template<typename Key, typename Value, typename Allocator = memory::NkAllocator>
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
            
            usize HashKey(const Key& key) const {
                // FNV-1a hash
                usize hash = 2166136261u;
                const unsigned char* data = reinterpret_cast<const unsigned char*>(&key);
                for (usize i = 0; i < sizeof(Key); ++i) {
                    hash ^= data[i];
                    hash *= 16777619u;
                }
                return hash;
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
            NkUnorderedMap()
                : mBuckets(nullptr), mBucketCount(16), mSize(0)
                , mMaxLoadFactor(0.75f)
                , mAllocator(&memory::NkGetDefaultAllocator()) {
                mBuckets = static_cast<Node**>(mAllocator->Allocate(mBucketCount * sizeof(Node*)));
                memory::NkMemZero(mBuckets, mBucketCount * sizeof(Node*));
            }
            
            NkUnorderedMap(NkInitializerList<ValueType> init)
                : mBuckets(nullptr), mBucketCount(16), mSize(0)
                , mMaxLoadFactor(0.75f)
                , mAllocator(&memory::NkGetDefaultAllocator()) {
                mBuckets = static_cast<Node**>(mAllocator->Allocate(mBucketCount * sizeof(Node*)));
                memory::NkMemZero(mBuckets, mBucketCount * sizeof(Node*));
                for (auto& pair : init) {
                    Insert(pair.First, pair.Second);
                }
            }
            
            ~NkUnorderedMap() {
                Clear();
                if (mBuckets) mAllocator->Deallocate(mBuckets);
            }
            
            // Capacity
            bool IsEmpty() const NK_NOEXCEPT { return mSize == 0; }
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
                    if (node->Data.First == key) {
                        node->Data.Second = value;
                        return;
                    }
                    node = node->Next;
                }
                
                // Insert new
                Node* newNode = static_cast<Node*>(mAllocator->Allocate(sizeof(Node)));
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
                    if (node->Data.First == key) {
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
                    if (node->Data.First == key) {
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
                    if (node->Data.First == key) {
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
        };
        
    } // namespace core
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