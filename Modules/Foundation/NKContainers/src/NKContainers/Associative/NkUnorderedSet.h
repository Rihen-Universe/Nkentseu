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
#include "NKMemory/NkMemoryFn.h"
#include "NKCore/Assert/NkAssert.h"
#include "NKContainers/Iterators/NkIterator.h"
#include "NKContainers/Iterators/NkInitializerList.h"

namespace nkentseu {
    namespace core {
        
        /**
         * @brief Unordered set - std::unordered_set equivalent
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
        template<typename T, typename Allocator = memory::NkAllocator>
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
            
            usize HashValue(const T& value) const {
                // FNV-1a hash
                usize hash = 2166136261u;
                const unsigned char* data = reinterpret_cast<const unsigned char*>(&value);
                for (usize i = 0; i < sizeof(T); ++i) {
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
            NkUnorderedSet()
                : mBuckets(nullptr), mBucketCount(16), mSize(0)
                , mMaxLoadFactor(0.75f)
                , mAllocator(&memory::NkGetDefaultAllocator()) {
                mBuckets = static_cast<Node**>(mAllocator->Allocate(mBucketCount * sizeof(Node*)));
                memory::NkMemZero(mBuckets, mBucketCount * sizeof(Node*));
            }
            
            NkUnorderedSet(NkInitializerList<T> init)
                : mBuckets(nullptr), mBucketCount(16), mSize(0)
                , mMaxLoadFactor(0.75f)
                , mAllocator(&memory::NkGetDefaultAllocator()) {
                mBuckets = static_cast<Node**>(mAllocator->Allocate(mBucketCount * sizeof(Node*)));
                memory::NkMemZero(mBuckets, mBucketCount * sizeof(Node*));
                for (auto& val : init) Insert(val);
            }
            
            ~NkUnorderedSet() {
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
            
            bool Insert(const T& value) {
                usize hash = HashValue(value);
                SizeType idx = GetBucketIndex(hash);
                
                // Check if exists
                Node* node = mBuckets[idx];
                while (node) {
                    if (node->Value == value) {
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
                    if (node->Value == value) {
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
                    if (node->Value == value) {
                        return true;
                    }
                    node = node->Next;
                }
                
                return false;
            }
        };
        
    } // namespace core
} // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_ASSOCIATIVE_NKUNORDEREDSET_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-05 22:26:13
// Creation Date: 2026-02-05 22:26:13
// ============================================================