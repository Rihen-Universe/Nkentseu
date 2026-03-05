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

NkGarbageCollector::NkGarbageCollector() noexcept
    : mObjects(nullptr), mRoots(nullptr), mObjectCount(0u), mLock() {}

void NkGarbageCollector::RegisterObject(NkGcObject* object) noexcept {
    if (!object) {
        return;
    }
    core::NkScopedSpinLock guard(mLock);
    object->mGcMarked = false;
    object->mGcNext = mObjects;
    mObjects = object;
    ++mObjectCount;
}

void NkGarbageCollector::UnregisterObject(NkGcObject* object) noexcept {
    if (!object) {
        return;
    }

    core::NkScopedSpinLock guard(mLock);
    NkGcObject* previous = nullptr;
    NkGcObject* current = mObjects;
    while (current) {
        if (current == object) {
            if (previous) {
                previous->mGcNext = current->mGcNext;
            } else {
                mObjects = current->mGcNext;
            }
            current->mGcNext = nullptr;
            if (mObjectCount > 0u) {
                --mObjectCount;
            }
            return;
        }
        previous = current;
        current = current->mGcNext;
    }
}

void NkGarbageCollector::AddRoot(NkGcRoot* root) noexcept {
    if (!root) {
        return;
    }
    core::NkScopedSpinLock guard(mLock);
    root->mNext = mRoots;
    mRoots = root;
}

void NkGarbageCollector::RemoveRoot(NkGcRoot* root) noexcept {
    if (!root) {
        return;
    }
    core::NkScopedSpinLock guard(mLock);

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
    NkGcObject* previous = nullptr;
    NkGcObject* current = mObjects;

    while (current) {
        if (!current->mGcMarked) {
            NkGcObject* toDelete = current;
            current = current->mGcNext;

            if (previous) {
                previous->mGcNext = current;
            } else {
                mObjects = current;
            }

            toDelete->mGcNext = nullptr;
            delete toDelete;
            if (mObjectCount > 0u) {
                --mObjectCount;
            }
        } else {
            current->mGcMarked = false;
            previous = current;
            current = current->mGcNext;
        }
    }
}

void NkGarbageCollector::Collect() noexcept {
    core::NkScopedSpinLock guard(mLock);
    MarkRoots();
    Sweep();
}

} // namespace memory
} // namespace nkentseu
