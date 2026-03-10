#pragma once

#ifndef NKENTSEU_MEMORY_NKGC_H_INCLUDED
#define NKENTSEU_MEMORY_NKGC_H_INCLUDED

#include "NKMemory/NkAllocator.h"
#include "NKCore/NkAtomic.h"

#ifndef NKENTSEU_STATIC_ASSERT
#define NKENTSEU_STATIC_ASSERT(condition, message) static_assert((condition), message)
#endif

namespace nkentseu {
    namespace memory {

        class NkGcObject;
        class NkGarbageCollector;

        class NkGcTracer {
            public:
                explicit NkGcTracer(NkGarbageCollector& gc) noexcept : mGc(gc) {}
                void Mark(NkGcObject* object) noexcept;

            private:
                NkGarbageCollector& mGc;
        };

        class NkGcObject {
            public:
                using NkGcDestroyFn = void (*)(NkGcObject* object, NkAllocator* allocator) noexcept;

                NkGcObject() noexcept
                    : mGcNext(nullptr),
                      mGcPrev(nullptr),
                      mGcMarked(false),
                      mGcOwned(false),
                      mGcAllocator(nullptr),
                      mGcDestroy(nullptr) {}
                virtual ~NkGcObject() = default;

                virtual void Trace(NkGcTracer& /*tracer*/) {}

                [[nodiscard]] nk_bool IsMarked() const noexcept { return mGcMarked; }

            private:
                NkGcObject* mGcNext;
                NkGcObject* mGcPrev;
                nk_bool mGcMarked;
                nk_bool mGcOwned;
                NkAllocator* mGcAllocator;
                NkGcDestroyFn mGcDestroy;

                friend class NkGarbageCollector;
                friend class NkGcTracer;
        };

        class NkGcRoot {
            public:
                explicit NkGcRoot(NkGcObject** slot = nullptr) noexcept : mSlot(slot), mNext(nullptr) {}

                void Bind(NkGcObject** slot) noexcept { mSlot = slot; }
                [[nodiscard]] NkGcObject** Slot() const noexcept { return mSlot; }

            private:
                NkGcObject** mSlot;
                NkGcRoot* mNext;

                friend class NkGarbageCollector;
        };

        class NkGarbageCollector {
            public:
                static NkGarbageCollector& Instance() noexcept;

                explicit NkGarbageCollector(NkAllocator* allocator = nullptr) noexcept;
                ~NkGarbageCollector();

                NkGarbageCollector(const NkGarbageCollector&) = delete;
                NkGarbageCollector& operator=(const NkGarbageCollector&) = delete;
                NkGarbageCollector(NkGarbageCollector&&) = delete;
                NkGarbageCollector& operator=(NkGarbageCollector&&) = delete;

                void SetAllocator(NkAllocator* allocator) noexcept;
                [[nodiscard]] NkAllocator* GetAllocator() const noexcept { return mAllocator; }

                void RegisterObject(NkGcObject* object) noexcept;
                void UnregisterObject(NkGcObject* object) noexcept;

                void AddRoot(NkGcRoot* root) noexcept;
                void RemoveRoot(NkGcRoot* root) noexcept;

                template <typename T, typename... Args>
                T* New(Args&&... args) {
            #if defined(__clang__) || defined(__GNUC__) || defined(_MSC_VER)
                    NKENTSEU_STATIC_ASSERT(__is_base_of(NkGcObject, T), "T must derive from NkGcObject");
            #endif
                    void* storage = mAllocator ? mAllocator->Allocate(sizeof(T), alignof(T)) : nullptr;
                    if (!storage) {
                        return nullptr;
                    }

                    T* object = nullptr;
                    try {
                        object = new (storage) T(traits::NkForward<Args>(args)...);
                    } catch (...) {
                        mAllocator->Deallocate(storage);
                        return nullptr;
                    }

                    object->mGcOwned = true;
                    object->mGcAllocator = mAllocator;
                    object->mGcDestroy = &DestroyOwnedObject<T>;
                    RegisterObject(object);
                    return object;
                }

                template <typename T>
                void Delete(T* object) noexcept {
                    if (!object) {
                        return;
                    }
                    UnregisterObject(object);
                    if (object->mGcOwned && object->mGcAllocator && object->mGcDestroy) {
                        object->mGcDestroy(static_cast<NkGcObject*>(object), object->mGcAllocator);
                        return;
                    }
                    delete object;
                }

                template <typename T>
                T* NewArray(nk_size count) {
                    if (count == 0u) {
                        return nullptr;
                    }
#if defined(__clang__) || defined(__GNUC__) || defined(_MSC_VER)
                    NKENTSEU_STATIC_ASSERT(__is_base_of(NkGcObject, T), "T must derive from NkGcObject");
#endif
                    T* objects = nullptr;
                    try {
                        objects = new T[count];
                    } catch (...) {
                        return nullptr;
                    }
                    for (nk_size i = 0u; i < count; ++i) {
                        RegisterObject(objects + i);
                    }
                    return objects;
                }

                template <typename T>
                void DeleteArray(T* objects, nk_size count) noexcept {
                    if (!objects || count == 0u) {
                        return;
                    }
                    for (nk_size i = 0u; i < count; ++i) {
                        UnregisterObject(objects + i);
                    }
                    delete[] objects;
                }

                void* Allocate(nk_size size, nk_size alignment = alignof(void*)) noexcept;
                void Free(void* pointer) noexcept;

                [[nodiscard]] nk_bool ContainsObject(const NkGcObject* object) const noexcept;

                void Collect() noexcept;
                [[nodiscard]] nk_size ObjectCount() const noexcept { return mObjectCount; }

            private:
                friend class NkGcTracer;

                void Mark(NkGcObject* object) noexcept;
                void MarkRoots() noexcept;
                void Sweep() noexcept;
                void UnlinkObjectLocked(NkGcObject* object) noexcept;
                void DestroyCollectedObject(NkGcObject* object) noexcept;

                template <typename T>
                static void DestroyOwnedObject(NkGcObject* object, NkAllocator* allocator) noexcept {
                    if (!object || !allocator) {
                        return;
                    }
                    T* typedObject = static_cast<T*>(object);
                    typedObject->~T();
                    allocator->Deallocate(typedObject);
                }

                static NkGcObject* HashTombstone() noexcept {
                    return reinterpret_cast<NkGcObject*>(static_cast<nk_uptr>(1u));
                }
                [[nodiscard]] nk_size HashPointer(const void* pointer) const noexcept;
                [[nodiscard]] nk_size NextPow2(nk_size value) const noexcept;
                void EnsureIndexCapacityLocked();
                void RebuildIndexLocked(nk_size requestedCapacity) noexcept;
                [[nodiscard]] nk_bool ContainsIndexLocked(const NkGcObject* object) const noexcept;
                [[nodiscard]] nk_bool InsertIndexLocked(NkGcObject* object) noexcept;
                [[nodiscard]] nk_bool EraseIndexLocked(const NkGcObject* object) noexcept;

                NkGcObject* mObjects;
                NkGcRoot* mRoots;
                nk_size mObjectCount;
                NkGcObject** mObjectIndex;
                nk_size mObjectIndexCapacity;
                nk_size mObjectIndexSize;
                nk_size mObjectIndexTombstones;
                NkAllocator* mAllocator;
                mutable NkSpinLock mLock;
        };

    } // namespace memory
} // namespace nkentseu

#endif // NKENTSEU_MEMORY_NKGC_H_INCLUDED
