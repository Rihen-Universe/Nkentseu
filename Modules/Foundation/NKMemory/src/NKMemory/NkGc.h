#pragma once

#ifndef NKENTSEU_MEMORY_NKGC_H_INCLUDED
#define NKENTSEU_MEMORY_NKGC_H_INCLUDED

#include "NKMemory/NkAllocator.h"
#include "NKCore/NkAtomic.h"

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
                NkGcObject() noexcept : mGcNext(nullptr), mGcMarked(false) {}
                virtual ~NkGcObject() = default;

                virtual void Trace(NkGcTracer& /*tracer*/) {}

                [[nodiscard]] nk_bool IsMarked() const noexcept { return mGcMarked; }

            private:
                NkGcObject* mGcNext;
                nk_bool mGcMarked;

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

                void RegisterObject(NkGcObject* object) noexcept;
                void UnregisterObject(NkGcObject* object) noexcept;

                void AddRoot(NkGcRoot* root) noexcept;
                void RemoveRoot(NkGcRoot* root) noexcept;

                template <typename T, typename... Args>
                T* New(Args&&... args) {
            #if defined(__clang__) || defined(__GNUC__) || defined(_MSC_VER)
                    static_assert(__is_base_of(NkGcObject, T), "T must derive from NkGcObject");
            #endif
                    T* object = new T(core::traits::NkForward<Args>(args)...);
                    if (object) {
                        RegisterObject(object);
                    }
                    return object;
                }

                template <typename T>
                void Delete(T* object) noexcept {
                    if (!object) {
                        return;
                    }
                    UnregisterObject(object);
                    delete object;
                }

                void Collect() noexcept;
                [[nodiscard]] nk_size ObjectCount() const noexcept { return mObjectCount; }

private:
    friend class NkGcTracer;

    NkGarbageCollector() noexcept;

                void Mark(NkGcObject* object) noexcept;
                void MarkRoots() noexcept;
                void Sweep() noexcept;

                NkGcObject* mObjects;
                NkGcRoot* mRoots;
                nk_size mObjectCount;
                core::NkSpinLock mLock;
        };

    } // namespace memory
} // namespace nkentseu

#endif // NKENTSEU_MEMORY_NKGC_H_INCLUDED
