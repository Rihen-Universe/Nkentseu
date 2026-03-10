#pragma once

#ifndef NKENTSEU_MEMORY_NKHASH_H_INCLUDED
#define NKENTSEU_MEMORY_NKHASH_H_INCLUDED

#include "NKMemory/NkAllocator.h"
#include "NKMemory/NkExport.h"

namespace nkentseu {
    namespace memory {

        class NKENTSEU_MEMORY_API NkPointerHashSet final {
            public:
                explicit NkPointerHashSet(NkAllocator* allocator = nullptr) noexcept;
                ~NkPointerHashSet();

                NkPointerHashSet(const NkPointerHashSet&) = delete;
                NkPointerHashSet& operator=(const NkPointerHashSet&) = delete;

                nk_bool Initialize(nk_size initialCapacity = 64u, NkAllocator* allocator = nullptr) noexcept;
                void Shutdown() noexcept;

                nk_bool Reserve(nk_size requestedCapacity) noexcept;
                void Clear() noexcept;

                nk_bool Insert(const void* key) noexcept;
                nk_bool Contains(const void* key) const noexcept;
                nk_bool Erase(const void* key) noexcept;

                [[nodiscard]] nk_bool IsInitialized() const noexcept { return mKeys != nullptr; }
                [[nodiscard]] nk_size Size() const noexcept { return mSize; }
                [[nodiscard]] nk_size Capacity() const noexcept { return mCapacity; }

            private:
                static const void* NkTombstoneKey() noexcept;
                static nk_size NkInvalidIndex() noexcept;
                static nk_size NkHashPointer(const void* pointer) noexcept;
                static nk_size NkNextPow2(nk_size value) noexcept;

                nk_bool Rehash(nk_size requestedCapacity) noexcept;
                nk_size FindSlot(const void* key) const noexcept;

                const void** mKeys;
                nk_size mCapacity;
                nk_size mSize;
                nk_size mTombstones;
                NkAllocator* mAllocator;
        };

        class NKENTSEU_MEMORY_API NkPointerHashMap final {
            public:
                struct NkEntry {
                    const void* Key;
                    void* Value;
                };

                explicit NkPointerHashMap(NkAllocator* allocator = nullptr) noexcept;
                ~NkPointerHashMap();

                NkPointerHashMap(const NkPointerHashMap&) = delete;
                NkPointerHashMap& operator=(const NkPointerHashMap&) = delete;

                nk_bool Initialize(nk_size initialCapacity = 64u, NkAllocator* allocator = nullptr) noexcept;
                void Shutdown() noexcept;

                nk_bool Reserve(nk_size requestedCapacity) noexcept;
                void Clear() noexcept;

                nk_bool Insert(const void* key, void* value) noexcept;
                [[nodiscard]] void* Find(const void* key) const noexcept;
                nk_bool TryGet(const void* key, void** outValue) const noexcept;
                nk_bool Contains(const void* key) const noexcept;
                nk_bool Erase(const void* key, void** outValue = nullptr) noexcept;

                [[nodiscard]] nk_bool IsInitialized() const noexcept { return mEntries != nullptr; }
                [[nodiscard]] nk_size Size() const noexcept { return mSize; }
                [[nodiscard]] nk_size Capacity() const noexcept { return mCapacity; }

            private:
                static const void* NkTombstoneKey() noexcept;
                static nk_size NkInvalidIndex() noexcept;
                static nk_size NkHashPointer(const void* pointer) noexcept;
                static nk_size NkNextPow2(nk_size value) noexcept;

                nk_bool Rehash(nk_size requestedCapacity) noexcept;
                nk_size FindSlot(const void* key) const noexcept;

                NkEntry* mEntries;
                nk_size mCapacity;
                nk_size mSize;
                nk_size mTombstones;
                NkAllocator* mAllocator;
        };

    } // namespace memory
} // namespace nkentseu

#endif // NKENTSEU_MEMORY_NKHASH_H_INCLUDED
