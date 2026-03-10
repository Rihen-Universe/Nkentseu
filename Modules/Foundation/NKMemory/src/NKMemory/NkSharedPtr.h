#pragma once

#ifndef NKENTSEU_MEMORY_NKSHAREDPTR_H_INCLUDED
#define NKENTSEU_MEMORY_NKSHAREDPTR_H_INCLUDED

#include "NKMemory/NkAllocator.h"
#include "NKCore/NkAtomic.h"

namespace nkentseu {
    namespace memory {

        class NkSharedControlBlockBase {
            public:
                NkSharedControlBlockBase() : mStrong(1), mWeak(1) {}
                virtual ~NkSharedControlBlockBase() = default;

                void AddStrongRef() noexcept { mStrong.FetchAdd(1); }
                void AddWeakRef() noexcept { mWeak.FetchAdd(1); }

                void ReleaseStrongRef() noexcept {
                    if (mStrong.FetchSub(1) == 1) {
                        DestroyObject();
                        ReleaseWeakRef();
                    }
                }

                void ReleaseWeakRef() noexcept {
                    if (mWeak.FetchSub(1) == 1) {
                        DestroyControlBlock();
                    }
                }

                [[nodiscard]] nk_int32 StrongCount() const noexcept { return mStrong.Load(); }
                [[nodiscard]] nk_int32 WeakCount() const noexcept { return mWeak.Load() - 1; }
                [[nodiscard]] nk_bool Expired() const noexcept { return mStrong.Load() == 0; }

                nk_bool TryAddStrongRef() noexcept {
                    nk_int32 expected = mStrong.Load();
                    while (expected > 0) {
                        nk_int32 desired = expected + 1;
                        if (mStrong.CompareExchangeWeak(expected, desired)) {
                            return true;
                        }
                    }
                    return false;
                }

            protected:
                virtual void DestroyObject() noexcept = 0;
                virtual void DestroyControlBlock() noexcept = 0;

            private:
                NkAtomicInt32 mStrong;
                NkAtomicInt32 mWeak;
        };

        template <typename T>
        class NkSharedControlBlock final : public NkSharedControlBlockBase {
            public:
                NkSharedControlBlock(T* ptr, NkAllocator* allocator, nk_bool useAllocatorForObject) noexcept
                    : mPtr(ptr)
                    , mAllocator(allocator ? allocator : &NkGetDefaultAllocator())
                    , mUseAllocatorForObject(useAllocatorForObject) {}

            protected:
                void DestroyObject() noexcept override {
                    if (!mPtr) {
                        return;
                    }
                    if (mUseAllocatorForObject && mAllocator) {
                        mAllocator->Delete(mPtr);
                    } else {
                        delete mPtr;
                    }
                    mPtr = nullptr;
                }

                void DestroyControlBlock() noexcept override {
                    NkAllocator* alloc = mAllocator ? mAllocator : &NkGetDefaultAllocator();
                    this->~NkSharedControlBlock();
                    alloc->Deallocate(this);
                }

            private:
                T* mPtr;
                NkAllocator* mAllocator;
                nk_bool mUseAllocatorForObject;
        };

        template <typename T>
        class NkWeakPtr;

        template <typename T>
        class NkSharedPtr {
            public:
                using element_type = T;

                constexpr NkSharedPtr() noexcept : mPtr(nullptr), mControl(nullptr) {}
                constexpr NkSharedPtr(decltype(nullptr)) noexcept : mPtr(nullptr), mControl(nullptr) {}

                explicit NkSharedPtr(T* ptr, NkAllocator* allocator = nullptr)
                    : mPtr(ptr), mControl(nullptr) {
                    if (!ptr) {
                        return;
                    }
                    NkAllocator* controlAllocator = allocator ? allocator : &NkGetDefaultAllocator();
                    void* mem = controlAllocator->Allocate(sizeof(NkSharedControlBlock<T>), alignof(NkSharedControlBlock<T>));
                    if (!mem) {
                        if (allocator) {
                            allocator->Delete(ptr);
                        } else {
                            delete ptr;
                        }
                        mPtr = nullptr;
                        return;
                    }
                    const nk_bool useAllocatorForObject = allocator != nullptr;
                    mControl = new (mem) NkSharedControlBlock<T>(ptr, controlAllocator, useAllocatorForObject);
                }

                NkSharedPtr(const NkSharedPtr& other) noexcept
                    : mPtr(other.mPtr), mControl(other.mControl) {
                    if (mControl) {
                        mControl->AddStrongRef();
                    }
                }

                NkSharedPtr(NkSharedPtr&& other) noexcept
                    : mPtr(other.mPtr), mControl(other.mControl) {
                    other.mPtr = nullptr;
                    other.mControl = nullptr;
                }

                explicit NkSharedPtr(const NkWeakPtr<T>& weak) noexcept
                    : mPtr(nullptr), mControl(nullptr) {
                    if (weak.mControl && weak.mControl->TryAddStrongRef()) {
                        mControl = weak.mControl;
                        mPtr = weak.mPtr;
                    }
                }

                ~NkSharedPtr() {
                    if (mControl) {
                        mControl->ReleaseStrongRef();
                    }
                }

                NkSharedPtr& operator=(const NkSharedPtr& other) noexcept {
                    if (this != &other) {
                        NkSharedPtr(other).Swap(*this);
                    }
                    return *this;
                }

                NkSharedPtr& operator=(NkSharedPtr&& other) noexcept {
                    if (this != &other) {
                        NkSharedPtr(traits::NkMove(other)).Swap(*this);
                    }
                    return *this;
                }

                NkSharedPtr& operator=(decltype(nullptr)) noexcept {
                    Reset();
                    return *this;
                }

                void Reset() noexcept { NkSharedPtr().Swap(*this); }
                void Reset(T* ptr, NkAllocator* allocator = nullptr) { NkSharedPtr(ptr, allocator).Swap(*this); }

                void Swap(NkSharedPtr& other) noexcept {
                    T* tmpPtr = mPtr;
                    mPtr = other.mPtr;
                    other.mPtr = tmpPtr;

                    NkSharedControlBlockBase* tmpCtrl = mControl;
                    mControl = other.mControl;
                    other.mControl = tmpCtrl;
                }

                [[nodiscard]] T* Get() const noexcept { return mPtr; }
                [[nodiscard]] T& operator*() const noexcept { return *mPtr; }
                [[nodiscard]] T* operator->() const noexcept { return mPtr; }
                [[nodiscard]] nk_int32 UseCount() const noexcept {
                    return mControl ? mControl->StrongCount() : 0;
                }
                [[nodiscard]] nk_bool Unique() const noexcept { return UseCount() == 1; }
                [[nodiscard]] nk_bool IsValid() const noexcept { return mPtr != nullptr; }
                explicit operator nk_bool() const noexcept { return mPtr != nullptr; }

            private:
                T* mPtr;
                NkSharedControlBlockBase* mControl;

                NkSharedPtr(T* ptr, NkSharedControlBlockBase* control, nk_bool addRef) noexcept
                    : mPtr(ptr), mControl(control) {
                    if (addRef && mControl) {
                        mControl->AddStrongRef();
                    }
                }

                friend class NkWeakPtr<T>;
        };

        template <typename T>
        class NkWeakPtr {
            public:
                constexpr NkWeakPtr() noexcept : mPtr(nullptr), mControl(nullptr) {}

                NkWeakPtr(const NkSharedPtr<T>& shared) noexcept
                    : mPtr(shared.mPtr), mControl(shared.mControl) {
                    if (mControl) {
                        mControl->AddWeakRef();
                    }
                }

                NkWeakPtr(const NkWeakPtr& other) noexcept
                    : mPtr(other.mPtr), mControl(other.mControl) {
                    if (mControl) {
                        mControl->AddWeakRef();
                    }
                }

                NkWeakPtr(NkWeakPtr&& other) noexcept
                    : mPtr(other.mPtr), mControl(other.mControl) {
                    other.mPtr = nullptr;
                    other.mControl = nullptr;
                }

                ~NkWeakPtr() {
                    if (mControl) {
                        mControl->ReleaseWeakRef();
                    }
                }

                NkWeakPtr& operator=(const NkWeakPtr& other) noexcept {
                    if (this != &other) {
                        NkWeakPtr(other).Swap(*this);
                    }
                    return *this;
                }

                NkWeakPtr& operator=(const NkSharedPtr<T>& shared) noexcept {
                    NkWeakPtr(shared).Swap(*this);
                    return *this;
                }

                NkWeakPtr& operator=(NkWeakPtr&& other) noexcept {
                    if (this != &other) {
                        NkWeakPtr(traits::NkMove(other)).Swap(*this);
                    }
                    return *this;
                }

                void Reset() noexcept { NkWeakPtr().Swap(*this); }

                void Swap(NkWeakPtr& other) noexcept {
                    T* tmpPtr = mPtr;
                    mPtr = other.mPtr;
                    other.mPtr = tmpPtr;

                    NkSharedControlBlockBase* tmpCtrl = mControl;
                    mControl = other.mControl;
                    other.mControl = tmpCtrl;
                }

                [[nodiscard]] nk_int32 UseCount() const noexcept {
                    return mControl ? mControl->StrongCount() : 0;
                }
                [[nodiscard]] nk_bool Expired() const noexcept {
                    return !mControl || mControl->Expired();
                }
                [[nodiscard]] nk_bool IsValid() const noexcept {
                    return !Expired();
                }
                [[nodiscard]] NkSharedPtr<T> Lock() const noexcept {
                    if (!mControl) {
                        return NkSharedPtr<T>();
                    }
                    if (!mControl->TryAddStrongRef()) {
                        return NkSharedPtr<T>();
                    }
                    return NkSharedPtr<T>(mPtr, mControl, false);
                }

            private:
                T* mPtr;
                NkSharedControlBlockBase* mControl;

                friend class NkSharedPtr<T>;
        };

        template <typename T, typename... Args>
        NkSharedPtr<T> NkMakeShared(Args&&... args) {
            NkAllocator& alloc = NkGetDefaultAllocator();
            T* object = alloc.New<T>(traits::NkForward<Args>(args)...);
            return NkSharedPtr<T>(object, &alloc);
        }

        template <typename T, typename... Args>
        NkSharedPtr<T> NkMakeSharedWithAllocator(NkAllocator& allocator, Args&&... args) {
            T* object = allocator.New<T>(traits::NkForward<Args>(args)...);
            return NkSharedPtr<T>(object, &allocator);
        }

    } // namespace memory
} // namespace nkentseu

#endif // NKENTSEU_MEMORY_NKSHAREDPTR_H_INCLUDED
