#pragma once

#ifndef NKENTSEU_MEMORY_NKINTRUSIVEPTR_H_INCLUDED
#define NKENTSEU_MEMORY_NKINTRUSIVEPTR_H_INCLUDED

#include "NKCore/NkAtomic.h"
#include "NKCore/NkTraits.h"

namespace nkentseu {
    namespace memory {

        class NkIntrusiveRefCounted {
            public:
                NkIntrusiveRefCounted() noexcept : mRefCount(0) {}
                NkIntrusiveRefCounted(const NkIntrusiveRefCounted&) noexcept : mRefCount(0) {}
                NkIntrusiveRefCounted& operator=(const NkIntrusiveRefCounted&) noexcept { return *this; }

                virtual ~NkIntrusiveRefCounted() = default;

                void AddRef() const noexcept { mRefCount.FetchAdd(1); }
                void ReleaseRef() const noexcept {
                    if (mRefCount.FetchSub(1) == 1) {
                        delete this;
                    }
                }

                [[nodiscard]] nk_int32 RefCount() const noexcept { return mRefCount.Load(); }

            private:
                mutable NkAtomicInt32 mRefCount;
        };

        template <typename T>
        class NkIntrusivePtr {
            public:
                constexpr NkIntrusivePtr() noexcept : mPtr(nullptr) {}
                constexpr NkIntrusivePtr(decltype(nullptr)) noexcept : mPtr(nullptr) {}

                explicit NkIntrusivePtr(T* ptr, nk_bool addRef = true) noexcept : mPtr(ptr) {
                    if (mPtr && addRef) {
                        mPtr->AddRef();
                    }
                }

                NkIntrusivePtr(const NkIntrusivePtr& other) noexcept : mPtr(other.mPtr) {
                    if (mPtr) {
                        mPtr->AddRef();
                    }
                }

                NkIntrusivePtr(NkIntrusivePtr&& other) noexcept : mPtr(other.mPtr) {
                    other.mPtr = nullptr;
                }

                ~NkIntrusivePtr() {
                    if (mPtr) {
                        mPtr->ReleaseRef();
                    }
                }

                NkIntrusivePtr& operator=(const NkIntrusivePtr& other) noexcept {
                    if (this != &other) {
                        NkIntrusivePtr(other).Swap(*this);
                    }
                    return *this;
                }

                NkIntrusivePtr& operator=(NkIntrusivePtr&& other) noexcept {
                    if (this != &other) {
                        NkIntrusivePtr(traits::NkMove(other)).Swap(*this);
                    }
                    return *this;
                }

                NkIntrusivePtr& operator=(T* ptr) noexcept {
                    NkIntrusivePtr(ptr).Swap(*this);
                    return *this;
                }

                NkIntrusivePtr& operator=(decltype(nullptr)) noexcept {
                    Reset();
                    return *this;
                }

                void Reset(T* ptr = nullptr, nk_bool addRef = true) noexcept {
                    NkIntrusivePtr(ptr, addRef).Swap(*this);
                }

                [[nodiscard]] T* Release() noexcept {
                    T* ptr = mPtr;
                    mPtr = nullptr;
                    return ptr;
                }

                void Swap(NkIntrusivePtr& other) noexcept {
                    T* tmp = mPtr;
                    mPtr = other.mPtr;
                    other.mPtr = tmp;
                }

                [[nodiscard]] T* Get() const noexcept { return mPtr; }
                [[nodiscard]] T& operator*() const noexcept { return *mPtr; }
                [[nodiscard]] T* operator->() const noexcept { return mPtr; }
                explicit operator nk_bool() const noexcept { return mPtr != nullptr; }
                [[nodiscard]] nk_int32 UseCount() const noexcept { return mPtr ? mPtr->RefCount() : 0; }

            private:
                T* mPtr;
        };

        template <typename T, typename... Args>
        NkIntrusivePtr<T> NkMakeIntrusive(Args&&... args) {
            T* object = new T(traits::NkForward<Args>(args)...);
            if (object) {
                object->AddRef();
            }
            return NkIntrusivePtr<T>(object, false);
        }

    } // namespace memory
} // namespace nkentseu

#endif // NKENTSEU_MEMORY_NKINTRUSIVEPTR_H_INCLUDED
