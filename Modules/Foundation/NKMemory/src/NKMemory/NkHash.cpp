#include "NKMemory/NkHash.h"

namespace nkentseu {
    namespace memory {

        namespace {

            inline nk_size NkNormalizeCapacity(nk_size requestedCapacity) noexcept {
                return requestedCapacity < 16u ? 16u : requestedCapacity;
            }

            inline nk_size NkCapacityFromLogicalSize(nk_size logicalSize) noexcept {
                const nk_size minCapacity = logicalSize + (logicalSize / 2u) + 1u;
                return NkNormalizeCapacity(minCapacity);
            }

        } // namespace

        NkPointerHashSet::NkPointerHashSet(NkAllocator* allocator) noexcept
            : mKeys(nullptr),
              mCapacity(0u),
              mSize(0u),
              mTombstones(0u),
              mAllocator(allocator) {}

        NkPointerHashSet::~NkPointerHashSet() {
            Shutdown();
        }

        const void* NkPointerHashSet::NkTombstoneKey() noexcept {
            return reinterpret_cast<const void*>(static_cast<nk_uptr>(1u));
        }

        nk_size NkPointerHashSet::NkInvalidIndex() noexcept {
            return static_cast<nk_size>(-1);
        }

        nk_size NkPointerHashSet::NkHashPointer(const void* pointer) noexcept {
            nk_uptr value = reinterpret_cast<nk_uptr>(pointer);
            if (sizeof(nk_uptr) >= 8u) {
                value ^= (value >> 33u);
                value *= static_cast<nk_uptr>(0xff51afd7ed558ccdULL);
                value ^= (value >> 33u);
                value *= static_cast<nk_uptr>(0xc4ceb9fe1a85ec53ULL);
                value ^= (value >> 33u);
            } else {
                value ^= (value >> 16u);
                value *= static_cast<nk_uptr>(0x7feb352du);
                value ^= (value >> 15u);
                value *= static_cast<nk_uptr>(0x846ca68bu);
                value ^= (value >> 16u);
            }
            return static_cast<nk_size>(value);
        }

        nk_size NkPointerHashSet::NkNextPow2(nk_size value) noexcept {
            if (value <= 1u) {
                return 1u;
            }
            --value;
            for (nk_size shift = 1u; shift < sizeof(nk_size) * 8u; shift <<= 1u) {
                value |= value >> shift;
            }
            return value + 1u;
        }

        nk_bool NkPointerHashSet::Initialize(nk_size initialCapacity, NkAllocator* allocator) noexcept {
            if (allocator) {
                mAllocator = allocator;
            }
            if (!mAllocator) {
                mAllocator = &NkGetMallocAllocator();
            }

            if (mKeys) {
                return true;
            }

            return Rehash(initialCapacity);
        }

        void NkPointerHashSet::Shutdown() noexcept {
            if (mKeys && mAllocator) {
                mAllocator->Deallocate(mKeys);
            }
            mKeys = nullptr;
            mCapacity = 0u;
            mSize = 0u;
            mTombstones = 0u;
        }

        nk_bool NkPointerHashSet::Reserve(nk_size requestedCapacity) noexcept {
            if (!mKeys) {
                return Initialize(requestedCapacity, nullptr);
            }
            if (requestedCapacity <= mSize) {
                return true;
            }

            const nk_size target = NkCapacityFromLogicalSize(requestedCapacity);
            if (target <= mCapacity && (mTombstones * 5u) < mCapacity) {
                return true;
            }

            return Rehash(target);
        }

        void NkPointerHashSet::Clear() noexcept {
            if (!mKeys || mCapacity == 0u) {
                return;
            }
            for (nk_size i = 0u; i < mCapacity; ++i) {
                mKeys[i] = nullptr;
            }
            mSize = 0u;
            mTombstones = 0u;
        }

        nk_bool NkPointerHashSet::Rehash(nk_size requestedCapacity) noexcept {
            if (!mAllocator) {
                mAllocator = &NkGetMallocAllocator();
            }

            const nk_size capacity = NkNextPow2(NkNormalizeCapacity(requestedCapacity));
            if (capacity == 0u) {
                return false;
            }

            const nk_size bytes = capacity * sizeof(const void*);
            auto* newKeys = static_cast<const void**>(mAllocator->Allocate(bytes, alignof(const void*)));
            if (!newKeys) {
                return false;
            }

            for (nk_size i = 0u; i < capacity; ++i) {
                newKeys[i] = nullptr;
            }

            const void** oldKeys = mKeys;
            const nk_size oldCapacity = mCapacity;

            mKeys = newKeys;
            mCapacity = capacity;
            mSize = 0u;
            mTombstones = 0u;

            if (oldKeys) {
                for (nk_size i = 0u; i < oldCapacity; ++i) {
                    const void* key = oldKeys[i];
                    if (!key || key == NkTombstoneKey()) {
                        continue;
                    }
                    (void)Insert(key);
                }
                mAllocator->Deallocate(oldKeys);
            }

            return true;
        }

        nk_size NkPointerHashSet::FindSlot(const void* key) const noexcept {
            if (!mKeys || mCapacity == 0u || !key || key == NkTombstoneKey()) {
                return NkInvalidIndex();
            }

            nk_size index = NkHashPointer(key) & (mCapacity - 1u);
            for (nk_size probes = 0u; probes < mCapacity; ++probes) {
                const void* slot = mKeys[index];
                if (slot == nullptr) {
                    return NkInvalidIndex();
                }
                if (slot != NkTombstoneKey() && slot == key) {
                    return index;
                }
                index = (index + 1u) & (mCapacity - 1u);
            }

            return NkInvalidIndex();
        }

        nk_bool NkPointerHashSet::Insert(const void* key) noexcept {
            if (!key || key == NkTombstoneKey()) {
                return false;
            }

            if (!mKeys) {
                if (!Initialize(64u, nullptr)) {
                    return false;
                }
            }

            if (((mSize + mTombstones + 1u) * 100u) >= (mCapacity * 70u)) {
                if (!Rehash(mCapacity * 2u)) {
                    return false;
                }
            }

            nk_size index = NkHashPointer(key) & (mCapacity - 1u);
            nk_size firstTombstone = NkInvalidIndex();

            for (nk_size probes = 0u; probes < mCapacity; ++probes) {
                const void* slot = mKeys[index];

                if (slot == nullptr) {
                    const nk_size target = (firstTombstone != NkInvalidIndex()) ? firstTombstone : index;
                    if (mKeys[target] == NkTombstoneKey() && mTombstones > 0u) {
                        --mTombstones;
                    }
                    mKeys[target] = key;
                    ++mSize;
                    return true;
                }

                if (slot == NkTombstoneKey()) {
                    if (firstTombstone == NkInvalidIndex()) {
                        firstTombstone = index;
                    }
                } else if (slot == key) {
                    return true;
                }

                index = (index + 1u) & (mCapacity - 1u);
            }

            return false;
        }

        nk_bool NkPointerHashSet::Contains(const void* key) const noexcept {
            return FindSlot(key) != NkInvalidIndex();
        }

        nk_bool NkPointerHashSet::Erase(const void* key) noexcept {
            const nk_size slotIndex = FindSlot(key);
            if (slotIndex == NkInvalidIndex()) {
                return false;
            }

            mKeys[slotIndex] = NkTombstoneKey();
            if (mSize > 0u) {
                --mSize;
            }
            ++mTombstones;

            if (mCapacity > 0u && (mTombstones * 100u) >= (mCapacity * 40u)) {
                (void)Rehash(mCapacity);
            }
            return true;
        }

        NkPointerHashMap::NkPointerHashMap(NkAllocator* allocator) noexcept
            : mEntries(nullptr),
              mCapacity(0u),
              mSize(0u),
              mTombstones(0u),
              mAllocator(allocator) {}

        NkPointerHashMap::~NkPointerHashMap() {
            Shutdown();
        }

        const void* NkPointerHashMap::NkTombstoneKey() noexcept {
            return reinterpret_cast<const void*>(static_cast<nk_uptr>(1u));
        }

        nk_size NkPointerHashMap::NkInvalidIndex() noexcept {
            return static_cast<nk_size>(-1);
        }

        nk_size NkPointerHashMap::NkHashPointer(const void* pointer) noexcept {
            nk_uptr value = reinterpret_cast<nk_uptr>(pointer);
            if (sizeof(nk_uptr) >= 8u) {
                value ^= (value >> 33u);
                value *= static_cast<nk_uptr>(0xff51afd7ed558ccdULL);
                value ^= (value >> 33u);
                value *= static_cast<nk_uptr>(0xc4ceb9fe1a85ec53ULL);
                value ^= (value >> 33u);
            } else {
                value ^= (value >> 16u);
                value *= static_cast<nk_uptr>(0x7feb352du);
                value ^= (value >> 15u);
                value *= static_cast<nk_uptr>(0x846ca68bu);
                value ^= (value >> 16u);
            }
            return static_cast<nk_size>(value);
        }

        nk_size NkPointerHashMap::NkNextPow2(nk_size value) noexcept {
            if (value <= 1u) {
                return 1u;
            }
            --value;
            for (nk_size shift = 1u; shift < sizeof(nk_size) * 8u; shift <<= 1u) {
                value |= value >> shift;
            }
            return value + 1u;
        }

        nk_bool NkPointerHashMap::Initialize(nk_size initialCapacity, NkAllocator* allocator) noexcept {
            if (allocator) {
                mAllocator = allocator;
            }
            if (!mAllocator) {
                mAllocator = &NkGetMallocAllocator();
            }

            if (mEntries) {
                return true;
            }

            return Rehash(initialCapacity);
        }

        void NkPointerHashMap::Shutdown() noexcept {
            if (mEntries && mAllocator) {
                mAllocator->Deallocate(mEntries);
            }
            mEntries = nullptr;
            mCapacity = 0u;
            mSize = 0u;
            mTombstones = 0u;
        }

        nk_bool NkPointerHashMap::Reserve(nk_size requestedCapacity) noexcept {
            if (!mEntries) {
                return Initialize(requestedCapacity, nullptr);
            }
            if (requestedCapacity <= mSize) {
                return true;
            }

            const nk_size target = NkCapacityFromLogicalSize(requestedCapacity);
            if (target <= mCapacity && (mTombstones * 5u) < mCapacity) {
                return true;
            }

            return Rehash(target);
        }

        void NkPointerHashMap::Clear() noexcept {
            if (!mEntries || mCapacity == 0u) {
                return;
            }
            for (nk_size i = 0u; i < mCapacity; ++i) {
                mEntries[i].Key = nullptr;
                mEntries[i].Value = nullptr;
            }
            mSize = 0u;
            mTombstones = 0u;
        }

        nk_bool NkPointerHashMap::Rehash(nk_size requestedCapacity) noexcept {
            if (!mAllocator) {
                mAllocator = &NkGetMallocAllocator();
            }

            const nk_size capacity = NkNextPow2(NkNormalizeCapacity(requestedCapacity));
            if (capacity == 0u) {
                return false;
            }

            const nk_size bytes = capacity * sizeof(NkEntry);
            auto* newEntries = static_cast<NkEntry*>(mAllocator->Allocate(bytes, alignof(NkEntry)));
            if (!newEntries) {
                return false;
            }

            for (nk_size i = 0u; i < capacity; ++i) {
                newEntries[i].Key = nullptr;
                newEntries[i].Value = nullptr;
            }

            NkEntry* oldEntries = mEntries;
            const nk_size oldCapacity = mCapacity;

            mEntries = newEntries;
            mCapacity = capacity;
            mSize = 0u;
            mTombstones = 0u;

            if (oldEntries) {
                for (nk_size i = 0u; i < oldCapacity; ++i) {
                    const void* key = oldEntries[i].Key;
                    if (!key || key == NkTombstoneKey()) {
                        continue;
                    }
                    (void)Insert(key, oldEntries[i].Value);
                }
                mAllocator->Deallocate(oldEntries);
            }

            return true;
        }

        nk_size NkPointerHashMap::FindSlot(const void* key) const noexcept {
            if (!mEntries || mCapacity == 0u || !key || key == NkTombstoneKey()) {
                return NkInvalidIndex();
            }

            nk_size index = NkHashPointer(key) & (mCapacity - 1u);
            for (nk_size probes = 0u; probes < mCapacity; ++probes) {
                const void* slotKey = mEntries[index].Key;
                if (slotKey == nullptr) {
                    return NkInvalidIndex();
                }
                if (slotKey != NkTombstoneKey() && slotKey == key) {
                    return index;
                }
                index = (index + 1u) & (mCapacity - 1u);
            }

            return NkInvalidIndex();
        }

        nk_bool NkPointerHashMap::Insert(const void* key, void* value) noexcept {
            if (!key || key == NkTombstoneKey()) {
                return false;
            }

            if (!mEntries) {
                if (!Initialize(64u, nullptr)) {
                    return false;
                }
            }

            if (((mSize + mTombstones + 1u) * 100u) >= (mCapacity * 70u)) {
                if (!Rehash(mCapacity * 2u)) {
                    return false;
                }
            }

            nk_size index = NkHashPointer(key) & (mCapacity - 1u);
            nk_size firstTombstone = NkInvalidIndex();

            for (nk_size probes = 0u; probes < mCapacity; ++probes) {
                const void* slotKey = mEntries[index].Key;

                if (slotKey == nullptr) {
                    const nk_size target = (firstTombstone != NkInvalidIndex()) ? firstTombstone : index;
                    if (mEntries[target].Key == NkTombstoneKey() && mTombstones > 0u) {
                        --mTombstones;
                    }
                    mEntries[target].Key = key;
                    mEntries[target].Value = value;
                    ++mSize;
                    return true;
                }

                if (slotKey == NkTombstoneKey()) {
                    if (firstTombstone == NkInvalidIndex()) {
                        firstTombstone = index;
                    }
                } else if (slotKey == key) {
                    mEntries[index].Value = value;
                    return true;
                }

                index = (index + 1u) & (mCapacity - 1u);
            }

            return false;
        }

        void* NkPointerHashMap::Find(const void* key) const noexcept {
            const nk_size slotIndex = FindSlot(key);
            if (slotIndex == NkInvalidIndex()) {
                return nullptr;
            }
            return mEntries[slotIndex].Value;
        }

        nk_bool NkPointerHashMap::TryGet(const void* key, void** outValue) const noexcept {
            if (!outValue) {
                return false;
            }
            const nk_size slotIndex = FindSlot(key);
            if (slotIndex == NkInvalidIndex()) {
                *outValue = nullptr;
                return false;
            }
            *outValue = mEntries[slotIndex].Value;
            return true;
        }

        nk_bool NkPointerHashMap::Contains(const void* key) const noexcept {
            return FindSlot(key) != NkInvalidIndex();
        }

        nk_bool NkPointerHashMap::Erase(const void* key, void** outValue) noexcept {
            const nk_size slotIndex = FindSlot(key);
            if (slotIndex == NkInvalidIndex()) {
                if (outValue) {
                    *outValue = nullptr;
                }
                return false;
            }

            if (outValue) {
                *outValue = mEntries[slotIndex].Value;
            }
            mEntries[slotIndex].Key = NkTombstoneKey();
            mEntries[slotIndex].Value = nullptr;

            if (mSize > 0u) {
                --mSize;
            }
            ++mTombstones;

            if (mCapacity > 0u && (mTombstones * 100u) >= (mCapacity * 40u)) {
                (void)Rehash(mCapacity);
            }
            return true;
        }

    } // namespace memory
} // namespace nkentseu
