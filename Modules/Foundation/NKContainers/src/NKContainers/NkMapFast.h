// ============================================================
// FILE: NkMapFast.h
// DESCRIPTION: Ultra-optimized hash map with cache-friendly layout
// AUTHOR: Rihen
// DATE: 2026-03-05
// VERSION: 1.0.0
// ============================================================
// Conteneur hasmap optimisé pour:
// - Cache locality (compact layout)
// - Small bucket overhead
// - Death flag (no tombstone)
// - Robin Hood hashing
// ============================================================

#pragma once

#ifndef NKENTSEU_NKCONTAINERS_NKMAPFAST_H_INCLUDED
#define NKENTSEU_NKCONTAINERS_NKMAPFAST_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
namespace containers {

    /**
     * @brief Cache-optimized hash map with Robin Hood hashing
     * 
     * Avantages:
     * - Compact layout (all keys/values in flat vectors) = cache-lineaire
     * - Robin Hood hashing = minimal probe sequences
     * - No tombstones = no memory waste
     * - O(1) average lookup, O(n) worst case
     * 
     * Memory layout:
     * [hashes][keys][values] - all contiguous for L1/L2 cache hits
     */
    template<typename Key, typename Value, typename HashFn = std::hash<Key>>
    class NkMapFast {
    public:
        using SizeType = nk_size;
        using KeyType = Key;
        using ValueType = Value;
        using PairType = std::pair<const Key, Value>;
        
        static constexpr nk_float LOAD_FACTOR = 0.75f;
        static constexpr SizeType INITIAL_CAPACITY = 16;
        static constexpr nk_uint32 EMPTY_HASH = 0;
        static constexpr nk_uint32 TOMBSTONE = 0xDEADBEEF;
        
        // == Constructors ==
        
        NkMapFast() : mSize(0), mCapacity(0) {}
        
        ~NkMapFast() {
            Clear();
        }
        
        NkMapFast(const NkMapFast& other) : NkMapFast() {
            for (const auto& pair : other) {
                Insert(pair.first, pair.second);
            }
        }
        
        NkMapFast(NkMapFast&& other) noexcept
            : mHashes(std::move(other.mHashes))
            , mKeys(std::move(other.mKeys))
            , mValues(std::move(other.mValues))
            , mSize(other.mSize)
            , mCapacity(other.mCapacity)
        {
            other.mSize = 0;
            other.mCapacity = 0;
        }
        
        // == Operators ==
        
        NkMapFast& operator=(const NkMapFast& other) {
            if (this != &other) {
                Clear();
                for (const auto& pair : other) {
                    Insert(pair.first, pair.second);
                }
            }
            return *this;
        }
        
        NkMapFast& operator=(NkMapFast&& other) noexcept {
            mHashes = std::move(other.mHashes);
            mKeys = std::move(other.mKeys);
            mValues = std::move(other.mValues);
            mSize = other.mSize;
            mCapacity = other.mCapacity;
            other.mSize = 0;
            other.mCapacity = 0;
            return *this;
        }
        
        // == Insert/Find ==
        
        std::pair<Value*, nk_bool> Insert(const Key& key, const Value& value) {
            EnsureCapacity(mSize + 1);
            
            nk_uint32 hash = HashKey(key);
            SizeType pos = FindPosition(hash, key);
            
            if (pos != NK_NPOS) {
                // Already exists
                return {&mValues[pos], nk_false};
            }
            
            // Insert new entry (Robin Hood insertion)
            InsertNewEntry(hash, key, value);
            return {&mValues[mSize - 1], nk_true};
        }
        
        Value* Find(const Key& key) {
            if (mCapacity == 0) return nullptr;
            
            nk_uint32 hash = HashKey(key);
            SizeType pos = FindPosition(hash, key);
            return pos != NK_NPOS ? &mValues[pos] : nullptr;
        }
        
        const Value* Find(const Key& key) const {
            if (mCapacity == 0) return nullptr;
            
            nk_uint32 hash = HashKey(key);
            SizeType pos = FindPosition(hash, key);
            return pos != NK_NPOS ? &mValues[pos] : nullptr;
        }
        
        // == Remove ==
        
        nk_bool Remove(const Key& key) {
            if (mCapacity == 0) return nk_false;
            
            nk_uint32 hash = HashKey(key);
            SizeType pos = FindPosition(hash, key);
            
            if (pos == NK_NPOS) return nk_false;
            
            // Mark as dead (Robin Hood requires proper removal)
            mHashes[pos] = TOMBSTONE;
            mSize--;
            
            // Rehash if too many tombstones
            nk_float deadRatio = (nk_float)(mCapacity - mSize) / (nk_float)mCapacity;
            if (deadRatio > 0.3f) {
                Rehash();
            }
            
            return nk_true;
        }
        
        // == Size queries ==
        
        NK_FORCE_INLINE SizeType Size() const { return mSize; }
        NK_FORCE_INLINE SizeType Capacity() const { return mCapacity; }
        NK_FORCE_INLINE nk_bool IsEmpty() const { return mSize == 0; }
        
        // == Iteration ==
        
        struct Iterator {
            NkMapFast* map;
            SizeType index;
            
            iterator& operator++() {
                for (++index; index < map->mCapacity; ++index) {
                    if (map->mHashes[index] != EMPTY_HASH && map->mHashes[index] != TOMBSTONE) {
                        break;
                    }
                }
                return *this;
            }
            
            PairType operator*() const {
                return {map->mKeys[index], map->mValues[index]};
            }
            
            nk_bool operator==(const Iterator& other) const {
                return index == other.index;
            }
            
            nk_bool operator!=(const Iterator& other) const {
                return index != other.index;
            }
        };
        
        Iterator begin() {
            SizeType idx = 0;
            for (; idx < mCapacity; ++idx) {
                if (mHashes[idx] != EMPTY_HASH && mHashes[idx] != TOMBSTONE) {
                    break;
                }
            }
            return {this, idx};
        }
        
        Iterator end() {
            return {this, mCapacity};
        }
        
        // == Capacity ==
        
        void Clear() {
            mHashes.Clear();
            mKeys.Clear();
            mValues.Clear();
            mSize = 0;
            mCapacity = 0;
        }
        
        void Reserve(SizeType capacity) {
            if (capacity > mCapacity) {
                mCapacity = capacity;
                Rehash();
            }
        }
        
    private:
        NkVector<nk_uint32> mHashes;
        NkVector<Key> mKeys;
        NkVector<Value> mValues;
        SizeType mSize;
        SizeType mCapacity;
        
        // == Internal helpers ==
        
        nk_uint32 HashKey(const Key& key) const {
            nk_uint32 h = HashFn()(key);
            return h == EMPTY_HASH ? 0xDEADBEEF : h;
        }
        
        SizeType FindPosition(nk_uint32 hash, const Key& key) const {
            if (mCapacity == 0) return NK_NPOS;
            
            SizeType idx = hash % mCapacity;
            SizeType probe = 0;
            
            while (probe < mCapacity) {
                if (mHashes[idx] == EMPTY_HASH) {
                    return NK_NPOS;  // Not found
                }
                
                if (mHashes[idx] != TOMBSTONE && mHashes[idx] == hash && mKeys[idx] == key) {
                    return idx;  // Found!
                }
                
                idx = (idx + 1) % mCapacity;
                probe++;
            }
            
            return NK_NPOS;
        }
        
        void InsertNewEntry(nk_uint32 hash, const Key& key, const Value& value) {
            if (mSize >= mCapacity) {
                EnsureCapacity(mSize + 1);
            }
            
            SizeType idx = hash % mCapacity;
            nk_uint32 probeLen = 0;
            
            // Robin Hood insertion: swap if incoming has longer probe distance
            while (mHashes[idx] != EMPTY_HASH && mHashes[idx] != TOMBSTONE) {
                nk_uint32 existingHash = mHashes[idx];
                nk_uint32 existingProbe = (idx - (existingHash % mCapacity) + mCapacity) % mCapacity;
                
                if (probeLen > existingProbe) {
                    // Swap
                    std::swap(hash, existingHash);
                    std::swap(mHashes[idx], hash);
                    std::swap(key, mKeys[idx]);
                    std::swap(value, mValues[idx]);
                    probeLen = existingProbe;
                }
                
                idx = (idx + 1) % mCapacity;
                probeLen++;
            }
            
            mHashes[idx] = hash;
            mKeys[idx] = key;
            mValues[idx] = value;
            mSize++;
        }
        
        void EnsureCapacity(SizeType requiredSize) {
            nk_float loadFactor = (nk_float)requiredSize / (nk_float)(mCapacity ? mCapacity : 1);
            
            if (loadFactor > LOAD_FACTOR) {
                SizeType newCapacity = mCapacity == 0 ? INITIAL_CAPACITY : mCapacity * 2;
                while (newCapacity < requiredSize / LOAD_FACTOR) {
                    newCapacity *= 2;
                }
                Reserve(newCapacity);
            }
        }
        
        void Rehash() {
            NkVector<nk_uint32> oldHashes = mHashes;
            NkVector<Key> oldKeys = mKeys;
            NkVector<Value> oldValues = mValues;
            
            SizeType oldSize = mSize;
            
            Clear();
            mCapacity = std::max(mCapacity, INITIAL_CAPACITY);
            mHashes.Resize(mCapacity, EMPTY_HASH);
            mKeys.Resize(mCapacity);
            mValues.Resize(mCapacity);
            mSize = 0;
            
            for (SizeType i = 0; i < oldHashes.Size(); ++i) {
                if (oldHashes[i] != EMPTY_HASH && oldHashes[i] != TOMBSTONE) {
                    InsertNewEntry(oldHashes[i], oldKeys[i], oldValues[i]);
                }
            }
        }
    };

} // namespace containers
} // namespace nkentseu

#endif // NKENTSEU_NKCONTAINERS_NKMAPFAST_H_INCLUDED
