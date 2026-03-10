#pragma once

#ifndef NKENTSEU_MEMORY_NKUNIQUEPTR_H_INCLUDED
#define NKENTSEU_MEMORY_NKUNIQUEPTR_H_INCLUDED

#include "NKMemory/NkAllocator.h"

namespace nkentseu {
    namespace memory {

        template <typename T>
        struct NkDefaultDelete {
            NkAllocator* allocator;

            explicit NkDefaultDelete(NkAllocator* alloc = nullptr) noexcept : allocator(alloc) {}

            void operator()(T* ptr) const noexcept {
                if (!ptr) {
                    return;
                }
                NkAllocator* alloc = allocator ? allocator : &NkGetDefaultAllocator();
                alloc->Delete(ptr);
            }
        };

        template <typename T>
        struct NkDefaultDelete<T[]> {
            NkAllocator* allocator;

            explicit NkDefaultDelete(NkAllocator* alloc = nullptr) noexcept : allocator(alloc) {}

            void operator()(T* ptr) const noexcept {
                if (!ptr) {
                    return;
                }
                NkAllocator* alloc = allocator ? allocator : &NkGetDefaultAllocator();
                alloc->DeleteArray(ptr);
            }
        };

        template <typename T, typename Deleter = NkDefaultDelete<T>>
        class NkUniquePtr {
            public:
                using element_type = T;
                using pointer = T*;
                using deleter_type = Deleter;

                constexpr NkUniquePtr() noexcept : mPtr(nullptr), mDeleter() {}
                explicit NkUniquePtr(pointer ptr) noexcept : mPtr(ptr), mDeleter() {}
                NkUniquePtr(pointer ptr, const Deleter& deleter) noexcept : mPtr(ptr), mDeleter(deleter) {}

                NkUniquePtr(NkUniquePtr&& other) noexcept
                    : mPtr(other.Release()), mDeleter(other.mDeleter) {}

                NkUniquePtr& operator=(NkUniquePtr&& other) noexcept {
                    if (this != &other) {
                        Reset(other.Release());
                        mDeleter = other.mDeleter;
                    }
                    return *this;
                }

                ~NkUniquePtr() { Reset(); }

                NkUniquePtr(const NkUniquePtr&) = delete;
                NkUniquePtr& operator=(const NkUniquePtr&) = delete;

                [[nodiscard]] pointer Get() const noexcept { return mPtr; }
                [[nodiscard]] Deleter& GetDeleter() noexcept { return mDeleter; }
                [[nodiscard]] const Deleter& GetDeleter() const noexcept { return mDeleter; }
                [[nodiscard]] nk_bool IsValid() const noexcept { return mPtr != nullptr; }

                explicit operator nk_bool() const noexcept { return mPtr != nullptr; }

                [[nodiscard]] element_type& operator*() const noexcept { return *mPtr; }
                [[nodiscard]] pointer operator->() const noexcept { return mPtr; }

                pointer Release() noexcept {
                    pointer old = mPtr;
                    mPtr = nullptr;
                    return old;
                }

                void Reset(pointer ptr = nullptr) noexcept {
                    pointer old = mPtr;
                    mPtr = ptr;
                    if (old) {
                        mDeleter(old);
                    }
                }

                void Swap(NkUniquePtr& other) noexcept {
                    pointer tmp = mPtr;
                    mPtr = other.mPtr;
                    other.mPtr = tmp;

                    Deleter tmpDel = mDeleter;
                    mDeleter = other.mDeleter;
                    other.mDeleter = tmpDel;
                }

            private:
                pointer mPtr;
                Deleter mDeleter;
        };

        template <typename T, typename Deleter>
        class NkUniquePtr<T[], Deleter> {
            public:
                using element_type = T;
                using pointer = T*;
                using deleter_type = Deleter;

                constexpr NkUniquePtr() noexcept : mPtr(nullptr), mDeleter() {}
                explicit NkUniquePtr(pointer ptr) noexcept : mPtr(ptr), mDeleter() {}
                NkUniquePtr(pointer ptr, const Deleter& deleter) noexcept : mPtr(ptr), mDeleter(deleter) {}

                NkUniquePtr(NkUniquePtr&& other) noexcept
                    : mPtr(other.Release()), mDeleter(other.mDeleter) {}

                NkUniquePtr& operator=(NkUniquePtr&& other) noexcept {
                    if (this != &other) {
                        Reset(other.Release());
                        mDeleter = other.mDeleter;
                    }
                    return *this;
                }

                ~NkUniquePtr() { Reset(); }

                NkUniquePtr(const NkUniquePtr&) = delete;
                NkUniquePtr& operator=(const NkUniquePtr&) = delete;

                [[nodiscard]] pointer Get() const noexcept { return mPtr; }
                [[nodiscard]] Deleter& GetDeleter() noexcept { return mDeleter; }
                [[nodiscard]] const Deleter& GetDeleter() const noexcept { return mDeleter; }
                [[nodiscard]] nk_bool IsValid() const noexcept { return mPtr != nullptr; }
                explicit operator nk_bool() const noexcept { return mPtr != nullptr; }

                [[nodiscard]] element_type& operator[](nk_size index) const noexcept { return mPtr[index]; }

                pointer Release() noexcept {
                    pointer old = mPtr;
                    mPtr = nullptr;
                    return old;
                }

                void Reset(pointer ptr = nullptr) noexcept {
                    pointer old = mPtr;
                    mPtr = ptr;
                    if (old) {
                        mDeleter(old);
                    }
                }

                void Swap(NkUniquePtr& other) noexcept {
                    pointer tmp = mPtr;
                    mPtr = other.mPtr;
                    other.mPtr = tmp;

                    Deleter tmpDel = mDeleter;
                    mDeleter = other.mDeleter;
                    other.mDeleter = tmpDel;
                }

            private:
                pointer mPtr;
                Deleter mDeleter;
        };

        template <typename T, typename... Args>
        NkUniquePtr<T> NkMakeUnique(Args&&... args) {
            NkAllocator& alloc = NkGetDefaultAllocator();
            T* object = alloc.New<T>(traits::NkForward<Args>(args)...);
            return NkUniquePtr<T>(object, NkDefaultDelete<T>(&alloc));
        }

        template <typename T, typename... Args>
        NkUniquePtr<T> NkMakeUniqueWithAllocator(NkAllocator& allocator, Args&&... args) {
            T* object = allocator.New<T>(traits::NkForward<Args>(args)...);
            return NkUniquePtr<T>(object, NkDefaultDelete<T>(&allocator));
        }

        template <typename T>
        NkUniquePtr<T[]> NkMakeUniqueArray(nk_size count) {
            NkAllocator& alloc = NkGetDefaultAllocator();
            T* object = alloc.NewArray<T>(count);
            return NkUniquePtr<T[]>(object, NkDefaultDelete<T[]>(&alloc));
        }

        template <typename T>
        NkUniquePtr<T[]> NkMakeUniqueArrayWithAllocator(NkAllocator& allocator, nk_size count) {
            T* object = allocator.NewArray<T>(count);
            return NkUniquePtr<T[]>(object, NkDefaultDelete<T[]>(&allocator));
        }

    } // namespace memory
} // namespace nkentseu

#endif // NKENTSEU_MEMORY_NKUNIQUEPTR_H_INCLUDED
