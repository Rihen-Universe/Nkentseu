#include "NKMemory/NkGc.h"

namespace nkentseu {
namespace memory {

void NkGcTracer::Mark(NkGcObject* object) noexcept {
    mGc.Mark(object);
}

NkGarbageCollector& NkGarbageCollector::Instance() noexcept {
    static NkGarbageCollector collector;
    return collector;
}

NkGarbageCollector::NkGarbageCollector(NkAllocator* allocator) noexcept
    : mObjects(nullptr),
      mRoots(nullptr),
      mObjectCount(0u),
      mObjectIndex(nullptr),
      mObjectIndexCapacity(0u),
      mObjectIndexSize(0u),
      mObjectIndexTombstones(0u),
      mAllocator(allocator ? allocator : &NkGetDefaultAllocator()),
      mLock() {
}

NkGarbageCollector::~NkGarbageCollector() {
    NkScopedSpinLock guard(mLock);

    NkGcRoot* root = mRoots;
    while (root) {
        NkGcRoot* next = root->mNext;
        root->mNext = nullptr;
        root = next;
    }
    mRoots = nullptr;

    NkGcObject* current = mObjects;
    while (current) {
        NkGcObject* next = current->mGcNext;
        current->mGcPrev = nullptr;
        current->mGcNext = nullptr;
        DestroyCollectedObject(current);
        current = next;
    }
    mObjects = nullptr;
    mObjectCount = 0u;
    mObjectIndexSize = 0u;
    mObjectIndexTombstones = 0u;

    if (mObjectIndex) {
        mAllocator->Deallocate(mObjectIndex);
        mObjectIndex = nullptr;
    }
    mObjectIndexCapacity = 0u;
}

void NkGarbageCollector::SetAllocator(NkAllocator* allocator) noexcept {
    if (!allocator) {
        allocator = &NkGetDefaultAllocator();
    }

    NkScopedSpinLock guard(mLock);
    if (mObjectCount != 0u || mObjectIndex != nullptr) {
        return;
    }
    mAllocator = allocator;
}

nk_size NkGarbageCollector::HashPointer(const void* pointer) const noexcept {
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

nk_size NkGarbageCollector::NextPow2(nk_size value) const noexcept {
    if (value <= 1u) {
        return 1u;
    }
    --value;
    for (nk_size shift = 1u; shift < (sizeof(nk_size) * 8u); shift <<= 1u) {
        value |= (value >> shift);
    }
    return value + 1u;
}

void NkGarbageCollector::RebuildIndexLocked(nk_size requestedCapacity) noexcept {
    nk_size newCapacity = requestedCapacity < 64u ? 64u : requestedCapacity;
    newCapacity = NextPow2(newCapacity);

    NkGcObject** newTable =
        static_cast<NkGcObject**>(mAllocator->Allocate(newCapacity * sizeof(NkGcObject*),
                                                       alignof(NkGcObject*)));
    if (!newTable) {
        return;
    }

    for (nk_size i = 0u; i < newCapacity; ++i) {
        newTable[i] = nullptr;
    }

    auto insertRaw = [&](NkGcObject* object) {
        nk_size idx = HashPointer(object) & (newCapacity - 1u);
        for (nk_size probes = 0u; probes < newCapacity; ++probes) {
            if (newTable[idx] == nullptr) {
                newTable[idx] = object;
                return;
            }
            idx = (idx + 1u) & (newCapacity - 1u);
        }
    };

    if (mObjectIndex) {
        for (nk_size i = 0u; i < mObjectIndexCapacity; ++i) {
            NkGcObject* slot = mObjectIndex[i];
            if (slot && slot != HashTombstone()) {
                insertRaw(slot);
            }
        }
        mAllocator->Deallocate(mObjectIndex);
    } else {
        NkGcObject* current = mObjects;
        while (current) {
            insertRaw(current);
            current = current->mGcNext;
        }
    }

    mObjectIndex = newTable;
    mObjectIndexCapacity = newCapacity;
    mObjectIndexSize = mObjectCount;
    mObjectIndexTombstones = 0u;
}

void NkGarbageCollector::EnsureIndexCapacityLocked() {
    if (!mObjectIndex) {
        RebuildIndexLocked(64u);
        return;
    }

    const nk_size usedSlots = mObjectIndexSize + mObjectIndexTombstones;
    if ((usedSlots + 1u) * 100u >= mObjectIndexCapacity * 70u) {
        RebuildIndexLocked(mObjectIndexCapacity * 2u);
    } else if (mObjectIndexTombstones * 100u >= mObjectIndexCapacity * 20u) {
        RebuildIndexLocked(mObjectIndexCapacity);
    }
}

nk_bool NkGarbageCollector::ContainsIndexLocked(const NkGcObject* object) const noexcept {
    if (!mObjectIndex || mObjectIndexCapacity == 0u || !object) {
        return false;
    }

    nk_size idx = HashPointer(object) & (mObjectIndexCapacity - 1u);
    for (nk_size probes = 0u; probes < mObjectIndexCapacity; ++probes) {
        const NkGcObject* slot = mObjectIndex[idx];
        if (slot == nullptr) {
            return false;
        }
        if (slot != HashTombstone() && slot == object) {
            return true;
        }
        idx = (idx + 1u) & (mObjectIndexCapacity - 1u);
    }
    return false;
}

nk_bool NkGarbageCollector::InsertIndexLocked(NkGcObject* object) noexcept {
    if (!mObjectIndex || mObjectIndexCapacity == 0u || !object) {
        return false;
    }

    nk_size idx = HashPointer(object) & (mObjectIndexCapacity - 1u);
    nk_size tombstoneIdx = static_cast<nk_size>(-1);

    for (nk_size probes = 0u; probes < mObjectIndexCapacity; ++probes) {
        NkGcObject* slot = mObjectIndex[idx];
        if (slot == nullptr) {
            nk_size target = (tombstoneIdx != static_cast<nk_size>(-1)) ? tombstoneIdx : idx;
            if (mObjectIndex[target] == HashTombstone()) {
                if (mObjectIndexTombstones > 0u) {
                    --mObjectIndexTombstones;
                }
            }
            mObjectIndex[target] = object;
            ++mObjectIndexSize;
            return true;
        }
        if (slot == HashTombstone()) {
            if (tombstoneIdx == static_cast<nk_size>(-1)) {
                tombstoneIdx = idx;
            }
        } else if (slot == object) {
            return true;
        }
        idx = (idx + 1u) & (mObjectIndexCapacity - 1u);
    }

    return false;
}

nk_bool NkGarbageCollector::EraseIndexLocked(const NkGcObject* object) noexcept {
    if (!mObjectIndex || mObjectIndexCapacity == 0u || !object) {
        return false;
    }

    nk_size idx = HashPointer(object) & (mObjectIndexCapacity - 1u);
    for (nk_size probes = 0u; probes < mObjectIndexCapacity; ++probes) {
        NkGcObject* slot = mObjectIndex[idx];
        if (slot == nullptr) {
            return false;
        }
        if (slot != HashTombstone() && slot == object) {
            mObjectIndex[idx] = HashTombstone();
            if (mObjectIndexSize > 0u) {
                --mObjectIndexSize;
            }
            ++mObjectIndexTombstones;
            return true;
        }
        idx = (idx + 1u) & (mObjectIndexCapacity - 1u);
    }

    return false;
}

void NkGarbageCollector::UnlinkObjectLocked(NkGcObject* object) noexcept {
    if (!object) {
        return;
    }

    if (object->mGcPrev) {
        object->mGcPrev->mGcNext = object->mGcNext;
    } else if (mObjects == object) {
        mObjects = object->mGcNext;
    }

    if (object->mGcNext) {
        object->mGcNext->mGcPrev = object->mGcPrev;
    }

    object->mGcPrev = nullptr;
    object->mGcNext = nullptr;
}

void NkGarbageCollector::RegisterObject(NkGcObject* object) noexcept {
    if (!object) {
        return;
    }

    NkScopedSpinLock guard(mLock);

    if (mObjectIndex) {
        if (ContainsIndexLocked(object)) {
            return;
        }
    } else {
        NkGcObject* current = mObjects;
        while (current) {
            if (current == object) {
                return;
            }
            current = current->mGcNext;
        }
    }

    EnsureIndexCapacityLocked();
    (void)InsertIndexLocked(object);

    object->mGcMarked = false;
    object->mGcPrev = nullptr;
    object->mGcNext = mObjects;
    if (mObjects) {
        mObjects->mGcPrev = object;
    }
    mObjects = object;
    ++mObjectCount;
}

void NkGarbageCollector::UnregisterObject(NkGcObject* object) noexcept {
    if (!object) {
        return;
    }

    NkScopedSpinLock guard(mLock);

    if (mObjectIndex) {
        if (!ContainsIndexLocked(object)) {
            return;
        }
    } else {
        NkGcObject* current = mObjects;
        while (current && current != object) {
            current = current->mGcNext;
        }
        if (!current) {
            return;
        }
        object = current;
    }

    UnlinkObjectLocked(object);
    if (mObjectIndex) {
        (void)EraseIndexLocked(object);
    }
    if (mObjectCount > 0u) {
        --mObjectCount;
    }
}

void* NkGarbageCollector::Allocate(nk_size size, nk_size alignment) noexcept {
    if (size == 0u) {
        return nullptr;
    }
    return mAllocator->Allocate(size, alignment);
}

void NkGarbageCollector::Free(void* pointer) noexcept {
    if (!pointer) {
        return;
    }
    mAllocator->Deallocate(pointer);
}

nk_bool NkGarbageCollector::ContainsObject(const NkGcObject* object) const noexcept {
    if (!object) {
        return false;
    }

    NkScopedSpinLock guard(mLock);
    if (mObjectIndex) {
        return ContainsIndexLocked(object);
    }

    const NkGcObject* current = mObjects;
    while (current) {
        if (current == object) {
            return true;
        }
        current = current->mGcNext;
    }
    return false;
}

void NkGarbageCollector::AddRoot(NkGcRoot* root) noexcept {
    if (!root) {
        return;
    }
    NkScopedSpinLock guard(mLock);
    root->mNext = mRoots;
    mRoots = root;
}

void NkGarbageCollector::RemoveRoot(NkGcRoot* root) noexcept {
    if (!root) {
        return;
    }
    NkScopedSpinLock guard(mLock);

    NkGcRoot* previous = nullptr;
    NkGcRoot* current = mRoots;
    while (current) {
        if (current == root) {
            if (previous) {
                previous->mNext = current->mNext;
            } else {
                mRoots = current->mNext;
            }
            current->mNext = nullptr;
            return;
        }
        previous = current;
        current = current->mNext;
    }
}

void NkGarbageCollector::Mark(NkGcObject* object) noexcept {
    if (!object || object->mGcMarked) {
        return;
    }
    object->mGcMarked = true;
    NkGcTracer tracer(*this);
    object->Trace(tracer);
}

void NkGarbageCollector::MarkRoots() noexcept {
    NkGcRoot* root = mRoots;
    while (root) {
        if (root->mSlot && *root->mSlot) {
            Mark(*root->mSlot);
        }
        root = root->mNext;
    }
}

void NkGarbageCollector::Sweep() noexcept {
    NkGcObject* current = mObjects;

    while (current) {
        NkGcObject* next = current->mGcNext;
        if (!current->mGcMarked) {
            UnlinkObjectLocked(current);
            (void)EraseIndexLocked(current);
            DestroyCollectedObject(current);
            if (mObjectCount > 0u) {
                --mObjectCount;
            }
        } else {
            current->mGcMarked = false;
        }
        current = next;
    }
}

void NkGarbageCollector::DestroyCollectedObject(NkGcObject* object) noexcept {
    if (!object) {
        return;
    }

    if (object->mGcOwned && object->mGcAllocator && object->mGcDestroy) {
        object->mGcDestroy(object, object->mGcAllocator);
        return;
    }

    delete object;
}

void NkGarbageCollector::Collect() noexcept {
    NkScopedSpinLock guard(mLock);
    MarkRoots();
    Sweep();
}

} // namespace memory
} // namespace nkentseu
