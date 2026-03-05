#pragma once

#ifndef NKENTSEU_MEMORY_NKMEMORY_H_INCLUDED
#define NKENTSEU_MEMORY_NKMEMORY_H_INCLUDED

#include "NKMemory/NkAllocator.h"
#include "NKMemory/NkGc.h"
#include "NKMemory/NkIntrusivePtr.h"
#include "NKMemory/NkMemoryExport.h"
#include "NKMemory/NkMemoryFn.h"
#include "NKMemory/NkSharedPtr.h"
#include "NKMemory/NkUniquePtr.h"
#include "NKCore/NkAtomic.h"

namespace nkentseu {
    namespace memory {

        struct NkMemoryStats {
            nk_size liveBytes;
            nk_size peakBytes;
            nk_size liveAllocations;
            nk_size totalAllocations;
        };

        class NKMEMORY_API NkMemorySystem {
            public:
                using NkLogCallback = void (*)(const nk_char*);

                static NkMemorySystem& Instance() noexcept;

                void Initialize(NkAllocator* allocator = nullptr) noexcept;
                void Shutdown(nk_bool reportLeaks = true) noexcept;
                [[nodiscard]] nk_bool IsInitialized() const noexcept { return mInitialized; }

                void SetLogCallback(NkLogCallback callback) noexcept;

                void* Allocate(nk_size size,
                            nk_size alignment,
                            const nk_char* file,
                            nk_int32 line,
                            const nk_char* function,
                            const nk_char* tag = nullptr);

                void Free(void* pointer) noexcept;

                template <typename T, typename... Args>
                T* New(const nk_char* file,
                    nk_int32 line,
                    const nk_char* function,
                    const nk_char* tag,
                    Args&&... args) {
                    void* memory = mAllocator->Allocate(sizeof(T), alignof(T));
                    if (!memory) {
                        return nullptr;
                    }

                    T* object = new (memory) T(core::traits::NkForward<Args>(args)...);
                    RegisterAllocation(object, memory, sizeof(T), 1u, alignof(T),
                                    &DestroyObject<T>, file, line, function, tag);
                    return object;
                }

                template <typename T>
                void Delete(T* pointer) noexcept {
                    Free(static_cast<void*>(pointer));
                }

                template <typename T>
                T* NewArray(nk_size count,
                            const nk_char* file,
                            nk_int32 line,
                            const nk_char* function,
                            const nk_char* tag) {
                    if (count == 0u) {
                        return nullptr;
                    }

                    void* memory = mAllocator->Allocate(sizeof(T) * count, alignof(T));
                    if (!memory) {
                        return nullptr;
                    }

                    T* array = static_cast<T*>(memory);
                    nk_size i = 0u;
                    for (; i < count; ++i) {
                        new (array + i) T();
                    }

                    RegisterAllocation(array, memory, sizeof(T) * count, count, alignof(T),
                                    &DestroyArray<T>, file, line, function, tag);
                    return array;
                }

                template <typename T>
                void DeleteArray(T* pointer) noexcept {
                    Free(static_cast<void*>(pointer));
                }

                [[nodiscard]] NkMemoryStats GetStats() const noexcept;
                void DumpLeaks() noexcept;

                [[nodiscard]] NkGarbageCollector& Gc() noexcept { return mGc; }

            private:
                using NkDestroyFn = void (*)(void* userPtr, void* basePtr, NkAllocator* allocator, nk_size count);

                struct NkAllocationNode {
                    void* userPtr;
                    void* basePtr;
                    nk_size size;
                    nk_size count;
                    nk_size alignment;
                    NkDestroyFn destroy;
                    const nk_char* file;
                    const nk_char* function;
                    const nk_char* tag;
                    nk_int32 line;
                    NkAllocationNode* next;
                };

                NkMemorySystem() noexcept;
                ~NkMemorySystem() = default;

                NkMemorySystem(const NkMemorySystem&) = delete;
                NkMemorySystem& operator=(const NkMemorySystem&) = delete;

                static void DestroyRaw(void* userPtr, void* basePtr, NkAllocator* allocator, nk_size count);

                template <typename T>
                static void DestroyObject(void* userPtr, void* basePtr, NkAllocator* allocator, nk_size /*count*/) {
                    if (!userPtr || !allocator) {
                        return;
                    }
                    T* object = static_cast<T*>(userPtr);
                    object->~T();
                    allocator->Deallocate(basePtr);
                }

                template <typename T>
                static void DestroyArray(void* userPtr, void* basePtr, NkAllocator* allocator, nk_size count) {
                    if (!userPtr || !allocator) {
                        return;
                    }
                    T* array = static_cast<T*>(userPtr);
                    for (nk_size i = 0u; i < count; ++i) {
                        array[i].~T();
                    }
                    allocator->Deallocate(basePtr);
                }

                void RegisterAllocation(void* userPtr,
                                        void* basePtr,
                                        nk_size size,
                                        nk_size count,
                                        nk_size alignment,
                                        NkDestroyFn destroy,
                                        const nk_char* file,
                                        nk_int32 line,
                                        const nk_char* function,
                                        const nk_char* tag);

                NkAllocationNode* FindAndDetach(void* userPtr) noexcept;
                void ReleaseNode(NkAllocationNode* node) noexcept;
                void LogLine(const nk_char* message) noexcept;

                NkAllocator* mAllocator;
                NkAllocationNode* mHead;
                nk_size mLiveBytes;
                nk_size mPeakBytes;
                nk_size mLiveAllocations;
                nk_size mTotalAllocations;
                NkLogCallback mLogCallback;
                nk_bool mInitialized;
                mutable core::NkSpinLock mLock;
                NkGarbageCollector& mGc;
        };

    } // namespace memory
} // namespace nkentseu

#define NK_MEMORY_SYSTEM (::nkentseu::memory::NkMemorySystem::Instance())
#define NK_MEM_ALLOC(bytes)                                                                            \
    NK_MEMORY_SYSTEM.Allocate((bytes), alignof(void*), __FILE__, __LINE__, __func__, "raw")
#define NK_MEM_FREE(ptr) NK_MEMORY_SYSTEM.Free((ptr))
#define NK_MEM_NEW(T, ...)                                                                            \
    NK_MEMORY_SYSTEM.New<T>(__FILE__, __LINE__, __func__, #T, ##__VA_ARGS__)
#define NK_MEM_DELETE(ptr) NK_MEMORY_SYSTEM.Delete((ptr))
#define NK_MEM_NEW_ARRAY(T, count)                                                                    \
    NK_MEMORY_SYSTEM.NewArray<T>((count), __FILE__, __LINE__, __func__, #T "[]")
#define NK_MEM_DELETE_ARRAY(ptr) NK_MEMORY_SYSTEM.DeleteArray((ptr))

#endif // NKENTSEU_MEMORY_NKMEMORY_H_INCLUDED
