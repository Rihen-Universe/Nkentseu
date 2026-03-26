// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\Containers\Associative\NkHashMap.h
// DESCRIPTION: Hash map - Unordered associative container
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ASSOCIATIVE_NKHASHMAP_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ASSOCIATIVE_NKHASHMAP_H_INCLUDED

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
        struct NkHashMapDefaultHasher {
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
        struct NkHashMapDefaultEqual {
            bool operator()(const Key& lhs, const Key& rhs) const {
                return lhs == rhs;
            }
        };
        
        
        /**
         * @brief Hash map - NkUnorderedMap equivalent
         * 
         * Table de hachage avec chaînage (separate chaining).
         * 
         * Complexité:
         * - Insert/Find/Erase: O(1) average, O(n) worst
         * - Rehash: O(n)
         * 
         * @example
         * NkHashMap<int, NkString> map = {
         *     {1, "one"},
         *     {2, "two"}
         * };
         * map[3] = "three";
         * if (map.Contains(1)) { }
         */
        template<typename Key,
                 typename Value,
                 typename Allocator = memory::NkAllocator,
                 typename Hasher = NkHashMapDefaultHasher<Key>,
                 typename KeyEqual = NkHashMapDefaultEqual<Key>>
        class NkHashMap {
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
                using ValueType = NkPair<const Key, Value>;
                using SizeType = usize;
                using Reference = ValueType&;
                using ConstReference = const ValueType&;
                using MappedType = Value;

                class Iterator {
                    private:
                        NkHashMap* mMap;
                        Node* mNode;
                        SizeType mBucket;
                        friend class NkHashMap;

                        Iterator(NkHashMap* map, Node* node, SizeType bucket)
                            : mMap(map), mNode(node), mBucket(bucket) {}

                        void Advance() {
                            if (!mMap || !mNode) {
                                mNode = nullptr;
                                mBucket = 0;
                                return;
                            }
                            if (mNode->Next) {
                                mNode = mNode->Next;
                                return;
                            }
                            ++mBucket;
                            while (mBucket < mMap->mBucketCount && mMap->mBuckets[mBucket] == nullptr) {
                                ++mBucket;
                            }
                            mNode = (mBucket < mMap->mBucketCount) ? mMap->mBuckets[mBucket] : nullptr;
                        }

                    public:
                        Iterator() : mMap(nullptr), mNode(nullptr), mBucket(0) {}

                        Reference operator*() const { return mNode->Data; }
                        ValueType* operator->() const { return &mNode->Data; }

                        Iterator& operator++() {
                            Advance();
                            return *this;
                        }

                        Iterator operator++(int) {
                            Iterator temp = *this;
                            Advance();
                            return temp;
                        }

                        bool operator==(const Iterator& other) const { return mNode == other.mNode; }
                        bool operator!=(const Iterator& other) const { return mNode != other.mNode; }
                };

                class ConstIterator {
                private:
                    const NkHashMap* mMap;
                    const Node* mNode;
                    SizeType mBucket;
                    friend class NkHashMap;

                    ConstIterator(const NkHashMap* map, const Node* node, SizeType bucket)
                        : mMap(map), mNode(node), mBucket(bucket) {}

                    void Advance() {
                        if (!mMap || !mNode) {
                            mNode = nullptr;
                            mBucket = 0;
                            return;
                        }
                        if (mNode->Next) {
                            mNode = mNode->Next;
                            return;
                        }
                        ++mBucket;
                        while (mBucket < mMap->mBucketCount && mMap->mBuckets[mBucket] == nullptr) {
                            ++mBucket;
                        }
                        mNode = (mBucket < mMap->mBucketCount) ? mMap->mBuckets[mBucket] : nullptr;
                    }

                public:
                    ConstIterator() : mMap(nullptr), mNode(nullptr), mBucket(0) {}
                    ConstIterator(const Iterator& it) : mMap(it.mMap), mNode(it.mNode), mBucket(it.mBucket) {}

                    ConstReference operator*() const { return mNode->Data; }
                    const ValueType* operator->() const { return &mNode->Data; }

                    ConstIterator& operator++() {
                        Advance();
                        return *this;
                    }

                    ConstIterator operator++(int) {
                        ConstIterator temp = *this;
                        Advance();
                        return temp;
                    }

                    bool operator==(const ConstIterator& other) const { return mNode == other.mNode; }
                    bool operator!=(const ConstIterator& other) const { return mNode != other.mNode; }
                };
                
            private:
                Node** mBuckets;
                SizeType mBucketCount;
                SizeType mSize;
                float mMaxLoadFactor;
                Allocator* mAllocator;
                Hasher mHasher;
                KeyEqual mEqual;
                
                // Hash function
                usize HashKey(const Key& key) const {
                    return mHasher(key);
                }
                
                usize GetBucketIndex(usize hash) const {
                    return hash % mBucketCount;
                }
                
                void InitBuckets(SizeType bucketCount) {
                    mBucketCount = bucketCount > 0 ? bucketCount : 1;
                    mBuckets = static_cast<Node**>(mAllocator->Allocate(mBucketCount * sizeof(Node*)));
                    NK_ASSERT(mBuckets != nullptr);
                    memory::NkMemZero(mBuckets, mBucketCount * sizeof(Node*));
                }

                void RehashInternal(SizeType newBucketCount) {
                    if (newBucketCount < 1) {
                        newBucketCount = 1;
                    }
                    Node** newBuckets = static_cast<Node**>(mAllocator->Allocate(newBucketCount * sizeof(Node*)));
                    NK_ASSERT(newBuckets != nullptr);
                    memory::NkMemZero(newBuckets, newBucketCount * sizeof(Node*));
                    
                    // Reinsert all nodes
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
                        RehashInternal(mBucketCount * 2);
                    }
                }

                Node* FindNode(const Key& key) const {
                    usize hash = HashKey(key);
                    SizeType idx = GetBucketIndex(hash);
                    Node* node = mBuckets[idx];
                    while (node) {
                        if (mEqual(node->Data.First, key)) {
                            return node;
                        }
                        node = node->Next;
                    }
                    return nullptr;
                }
                
            public:
                // Constructors
                explicit NkHashMap(Allocator* allocator = nullptr)
                    : mBuckets(nullptr), mBucketCount(16), mSize(0)
                    , mMaxLoadFactor(0.75f)
                    , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator())
                    , mHasher()
                    , mEqual() {
                    InitBuckets(16);
                }
                
                NkHashMap(NkInitializerList<ValueType> init, Allocator* allocator = nullptr)
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

                NkHashMap(std::initializer_list<ValueType> init, Allocator* allocator = nullptr)
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

                NkHashMap(const NkHashMap& other)
                    : mBuckets(nullptr), mBucketCount(16), mSize(0)
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

                NkHashMap(NkHashMap&& other) NK_NOEXCEPT
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

                NkHashMap& operator=(const NkHashMap& other) {
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

                NkHashMap& operator=(NkHashMap&& other) NK_NOEXCEPT {
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

                NkHashMap& operator=(NkInitializerList<ValueType> init) {
                    Clear();
                    for (auto& pair : init) {
                        Insert(pair.First, pair.Second);
                    }
                    return *this;
                }

                NkHashMap& operator=(std::initializer_list<ValueType> init) {
                    Clear();
                    for (auto& pair : init) {
                        Insert(pair.First, pair.Second);
                    }
                    return *this;
                }

                ~NkHashMap() {
                    Clear();
                    if (mBuckets) mAllocator->Deallocate(mBuckets);
                }
                
                // Capacity
                bool Empty() const NK_NOEXCEPT { return mSize == 0; }
                SizeType Size() const NK_NOEXCEPT { return mSize; }
                SizeType BucketCount() const NK_NOEXCEPT { return mBucketCount; }
                float LoadFactor() const NK_NOEXCEPT {
                    return mBucketCount > 0 ? static_cast<float>(mSize) / static_cast<float>(mBucketCount) : 0.0f;
                }
                float MaxLoadFactor() const NK_NOEXCEPT { return mMaxLoadFactor; }
                void SetMaxLoadFactor(float factor) {
                    NK_ASSERT(factor > 0.0f);
                    if (factor > 0.0f) {
                        mMaxLoadFactor = factor;
                        CheckLoadFactor();
                    }
                }

                Iterator Begin() {
                    for (SizeType i = 0; i < mBucketCount; ++i) {
                        if (mBuckets[i]) {
                            return Iterator(this, mBuckets[i], i);
                        }
                    }
                    return End();
                }
                ConstIterator Begin() const {
                    for (SizeType i = 0; i < mBucketCount; ++i) {
                        if (mBuckets[i]) {
                            return ConstIterator(this, mBuckets[i], i);
                        }
                    }
                    return End();
                }
                ConstIterator CBegin() const { return Begin(); }

                Iterator End() { return Iterator(this, nullptr, mBucketCount); }
                ConstIterator End() const { return ConstIterator(this, nullptr, mBucketCount); }
                ConstIterator CEnd() const { return End(); }

                Iterator begin() {
                    for (SizeType i = 0; i < mBucketCount; ++i) {
                        if (mBuckets[i]) {
                            return Iterator(this, mBuckets[i], i);
                        }
                    }
                    return end();
                }
                ConstIterator begin() const {
                    for (SizeType i = 0; i < mBucketCount; ++i) {
                        if (mBuckets[i]) {
                            return ConstIterator(this, mBuckets[i], i);
                        }
                    }
                    return end();
                }
                ConstIterator cbegin() const { return begin(); }

                Iterator end() { return Iterator(this, nullptr, mBucketCount); }
                ConstIterator end() const { return ConstIterator(this, nullptr, mBucketCount); }
                ConstIterator cend() const { return end(); }
                
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

                void Rehash(SizeType newBucketCount) {
                    RehashInternal(newBucketCount);
                }

                void Reserve(SizeType elementCount) {
                    if (elementCount == 0) {
                        return;
                    }
                    SizeType requiredBuckets = static_cast<SizeType>(static_cast<float>(elementCount) / mMaxLoadFactor) + 1;
                    if (requiredBuckets > mBucketCount) {
                        RehashInternal(requiredBuckets);
                    }
                }

                void Swap(NkHashMap& other) NK_NOEXCEPT {
                    Node** tmpBuckets = mBuckets;
                    mBuckets = other.mBuckets;
                    other.mBuckets = tmpBuckets;

                    SizeType tmpBucketCount = mBucketCount;
                    mBucketCount = other.mBucketCount;
                    other.mBucketCount = tmpBucketCount;

                    SizeType tmpSize = mSize;
                    mSize = other.mSize;
                    other.mSize = tmpSize;

                    float tmpLoadFactor = mMaxLoadFactor;
                    mMaxLoadFactor = other.mMaxLoadFactor;
                    other.mMaxLoadFactor = tmpLoadFactor;

                    Allocator* tmpAllocator = mAllocator;
                    mAllocator = other.mAllocator;
                    other.mAllocator = tmpAllocator;
                }
                
                void Insert(const Key& key, const Value& value) {
                    usize hash = HashKey(key);
                    SizeType idx = GetBucketIndex(hash);
                    
                    // Check if key exists
                    Node* node = mBuckets[idx];
                    while (node) {
                        if (mEqual(node->Data.First, key)) {
                            node->Data.Second = value;  // Update
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
                    Node* node = FindNode(key);
                    return node ? &node->Data.Second : nullptr;
                }
                
                const Value* Find(const Key& key) const {
                    Node* node = FindNode(key);
                    return node ? &node->Data.Second : nullptr;
                }
                
                bool Contains(const Key& key) const {
                    return Find(key) != nullptr;
                }

                Iterator FindIterator(const Key& key) {
                    usize hash = HashKey(key);
                    SizeType idx = GetBucketIndex(hash);
                    Node* node = mBuckets[idx];
                    while (node) {
                        if (mEqual(node->Data.First, key)) {
                            return Iterator(this, node, idx);
                        }
                        node = node->Next;
                    }
                    return End();
                }

                ConstIterator FindIterator(const Key& key) const {
                    usize hash = HashKey(key);
                    SizeType idx = GetBucketIndex(hash);
                    Node* node = mBuckets[idx];
                    while (node) {
                        if (mEqual(node->Data.First, key)) {
                            return ConstIterator(this, node, idx);
                        }
                        node = node->Next;
                    }
                    return End();
                }

                bool TryGet(const Key& key, Value& outValue) const {
                    const Value* value = Find(key);
                    if (!value) {
                        return false;
                    }
                    outValue = *value;
                    return true;
                }

                Value& At(const Key& key) {
                    Value* value = Find(key);
                    NK_ASSERT(value != nullptr);
                    return *value;
                }

                const Value& At(const Key& key) const {
                    const Value* value = Find(key);
                    NK_ASSERT(value != nullptr);
                    return *value;
                }

                bool InsertOrAssign(const Key& key, const Value& value) {
                    Value* current = Find(key);
                    if (current) {
                        *current = value;
                        return false;
                    }
                    Insert(key, value);
                    return true;
                }
                
                // Operator[]
                Value& operator[](const Key& key) {
                    Value* val = Find(key);
                    if (val) return *val;
                    
                    Insert(key, Value());
                    return *Find(key);
                }
        };
        
    
} // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ASSOCIATIVE_NKHASHMAP_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-05 22:26:13
// Creation Date: 2026-02-05 22:26:13
// ============================================================
