#pragma once

#ifndef NKENTSEU_MEMORY_NKALLOCATOR_H_INCLUDED
#define NKENTSEU_MEMORY_NKALLOCATOR_H_INCLUDED

#include "NKMemory/NkMemoryExport.h"
#include "NKMemory/NkUtils.h"
#include "NKCore/NkTraits.h"
#include "NKPlatform/NkPlatformDetect.h"

namespace nkentseu {
    namespace memory {

        inline constexpr nk_size NK_MEMORY_DEFAULT_ALIGNMENT = sizeof(void*);

        enum NkMemoryFlag : nk_uint32 {
            NkMemoryFlag_None      = 0u,
            NkMemoryFlag_Read      = 1u << 0u,
            NkMemoryFlag_Write     = 1u << 1u,
            NkMemoryFlag_Execute   = 1u << 2u,
            NkMemoryFlag_Reserve   = 1u << 3u,
            NkMemoryFlag_Commit    = 1u << 4u,
            NkMemoryFlag_Anonymous = 1u << 5u
        };

        class NKMEMORY_API NkAllocatorBase {
            public:
                explicit NkAllocatorBase(const nk_char* name = "NkAllocatorBase") noexcept : mName(name) {}
                virtual ~NkAllocatorBase() = default;
                [[nodiscard]] const nk_char* GetName() const noexcept { return mName; }

            protected:
                const nk_char* mName;
        };

        class NKMEMORY_API NkAllocator : public NkAllocatorBase {
            public:
                using Pointer = void*;
                using SizeType = nk_size;

                explicit NkAllocator(const nk_char* name = "NkAllocator") noexcept : NkAllocatorBase(name) {}
                ~NkAllocator() override = default;

                virtual Pointer Allocate(SizeType size, SizeType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) = 0;
                virtual void Deallocate(Pointer ptr) = 0;
                virtual Pointer Reallocate(Pointer ptr, SizeType oldSize, SizeType newSize,
                                        SizeType alignment = NK_MEMORY_DEFAULT_ALIGNMENT);
                virtual Pointer Calloc(SizeType size, SizeType alignment = NK_MEMORY_DEFAULT_ALIGNMENT);
                virtual void Reset() noexcept {}
                [[nodiscard]] virtual const nk_char* Name() const noexcept { return mName; }

                template <typename T, typename... Args>
                T* New(Args&&... args) {
                    void* memory = Allocate(sizeof(T), alignof(T));
                    if (!memory) {
                        return nullptr;
                    }
                    return new (memory) T(core::traits::NkForward<Args>(args)...);
                }

                template <typename T>
                void Delete(T* ptr) noexcept {
                    if (!ptr) {
                        return;
                    }
                    ptr->~T();
                    Deallocate(ptr);
                }

                template <typename T, typename... Args>
                T* NewArray(SizeType count, Args&&... args) {
                    if (count == 0u) {
                        return nullptr;
                    }

                                struct NkArrayHeader {
                                    SizeType count;
                                    SizeType offset;
                                };

                                const SizeType headerSize = sizeof(NkArrayHeader);
                                const SizeType elementAlignment = alignof(T);
                                const SizeType baseOffset = NkAlignUp(headerSize, elementAlignment);
                                const SizeType totalBytes = baseOffset + sizeof(T) * count;

                                void* base = Allocate(totalBytes, elementAlignment);
                                if (!base) {
                                    return nullptr;
                                }

                                auto* header = static_cast<NkArrayHeader*>(base);
                                header->count = count;
                                header->offset = baseOffset;

                                T* array = reinterpret_cast<T*>(static_cast<nk_uint8*>(base) + baseOffset);
                                SizeType i = 0u;
                                for (; i < count; ++i) {
                                    new (array + i) T(core::traits::NkForward<Args>(args)...);
                                }

                                return array;
                            }

                template <typename T>
                void DeleteArray(T* ptr) noexcept {
                    if (!ptr) {
                        return;
                    }

                    struct NkArrayHeader {
                        SizeType count;
                        SizeType offset;
                    };

                    const SizeType headerSize = sizeof(NkArrayHeader);
                    const SizeType baseOffset = NkAlignUp(headerSize, alignof(T));
                    auto* base = reinterpret_cast<nk_uint8*>(ptr) - baseOffset;
                    auto* header = reinterpret_cast<NkArrayHeader*>(base);

                    for (SizeType i = 0u; i < header->count; ++i) {
                        ptr[i].~T();
                    }

                    Deallocate(base);
                }

                template <typename T>
                T* As(Pointer ptr) const noexcept {
                    if (!ptr) {
                        return nullptr;
                    }
                    return reinterpret_cast<T*>(ptr);
                }
        };

        // Backward-compat alias used by legacy modules.
        using NkIAllocator = NkAllocator;

        class NKMEMORY_API NkNewAllocator final : public NkAllocator {
            public:
                NkNewAllocator() noexcept : NkAllocator("NkNewAllocator") {}
                ~NkNewAllocator() override = default;

                Pointer Allocate(SizeType size, SizeType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                void Deallocate(Pointer ptr) override;
                Pointer Reallocate(Pointer ptr, SizeType oldSize, SizeType newSize,
                                SizeType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                Pointer Calloc(SizeType size, SizeType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
        };

        class NKMEMORY_API NkMallocAllocator final : public NkAllocator {
            public:
                NkMallocAllocator() noexcept : NkAllocator("NkMallocAllocator") {}
                ~NkMallocAllocator() override = default;

                Pointer Allocate(SizeType size, SizeType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                void Deallocate(Pointer ptr) override;
                Pointer Reallocate(Pointer ptr, SizeType oldSize, SizeType newSize,
                                SizeType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                Pointer Calloc(SizeType size, SizeType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
        };

        class NKMEMORY_API NkVirtualAllocator final : public NkAllocator {
            public:
                NkVirtualAllocator() noexcept;
                ~NkVirtualAllocator() override;

                Pointer Allocate(SizeType size, SizeType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                void Deallocate(Pointer ptr) override;
                Pointer Reallocate(Pointer ptr, SizeType oldSize, SizeType newSize,
                                SizeType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                Pointer Calloc(SizeType size, SizeType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;

                Pointer AllocateVirtual(SizeType size, const nk_char* tag = "VirtualAllocation") noexcept;
                void FreeVirtual(Pointer ptr) noexcept;

                nk_uint32 flags;

            private:
                struct NkVirtualBlock {
                    Pointer ptr;
                    SizeType size;
                    NkVirtualBlock* next;
                };

                NkVirtualBlock* mBlocks;

                SizeType PageSize() const noexcept;
                SizeType NormalizeSize(SizeType size) const noexcept;
                void TrackBlock(Pointer ptr, SizeType size);
                SizeType UntrackBlock(Pointer ptr) noexcept;
        };

        class NKMEMORY_API NkLinearAllocator final : public NkAllocator {
            public:
                explicit NkLinearAllocator(SizeType capacityBytes,
                                        NkAllocator* backingAllocator = nullptr);
                ~NkLinearAllocator() override;

                Pointer Allocate(SizeType size, SizeType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                void Deallocate(Pointer ptr) override;
                Pointer Reallocate(Pointer ptr, SizeType oldSize, SizeType newSize,
                                SizeType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                Pointer Calloc(SizeType size, SizeType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                const nk_char* Name() const noexcept override;
                void Reset() noexcept override;

                [[nodiscard]] SizeType Capacity() const noexcept { return mCapacity; }
                [[nodiscard]] SizeType Used() const noexcept { return mOffset; }
                [[nodiscard]] SizeType Available() const noexcept { return mCapacity - mOffset; }

            private:
                NkAllocator* mBacking;
                nk_uint8* mBuffer;
                SizeType mCapacity;
                SizeType mOffset;
                nk_bool mOwnsBuffer;
        };

        class NKMEMORY_API NkArenaAllocator final : public NkAllocator {
            public:
                using Marker = SizeType;

                explicit NkArenaAllocator(SizeType capacityBytes,
                                        NkAllocator* backingAllocator = nullptr);
                ~NkArenaAllocator() override;

                Pointer Allocate(SizeType size, SizeType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                void Deallocate(Pointer ptr) override;
                Pointer Reallocate(Pointer ptr, SizeType oldSize, SizeType newSize,
                                SizeType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                Pointer Calloc(SizeType size, SizeType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                const nk_char* Name() const noexcept override;
                void Reset() noexcept override;

                Marker CreateMarker() const noexcept;
                void FreeToMarker(Marker marker) noexcept;

            private:
                NkAllocator* mBacking;
                nk_uint8* mBuffer;
                SizeType mCapacity;
                SizeType mOffset;
                nk_bool mOwnsBuffer;
        };

        class NKMEMORY_API NkStackAllocator final : public NkAllocator {
            public:
                explicit NkStackAllocator(SizeType capacityBytes,
                                        NkAllocator* backingAllocator = nullptr);
                ~NkStackAllocator() override;

                Pointer Allocate(SizeType size, SizeType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                void Deallocate(Pointer ptr) override;
                Pointer Reallocate(Pointer ptr, SizeType oldSize, SizeType newSize,
                                SizeType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                Pointer Calloc(SizeType size, SizeType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                const nk_char* Name() const noexcept override;
                void Reset() noexcept override;

                [[nodiscard]] SizeType Capacity() const noexcept { return mCapacity; }
                [[nodiscard]] SizeType Used() const noexcept { return mOffset; }

            private:
                struct NkAllocationHeader {
                    SizeType previousOffset;
                    Pointer previousAllocation;
                };

                NkAllocator* mBacking;
                nk_uint8* mBuffer;
                SizeType mCapacity;
                SizeType mOffset;
                Pointer mLastAllocation;
                nk_bool mOwnsBuffer;
        };

        class NKMEMORY_API NkPoolAllocator final : public NkAllocator {
            public:
                NkPoolAllocator(SizeType blockSize,
                                SizeType blockCount,
                                NkAllocator* backingAllocator = nullptr);
                ~NkPoolAllocator() override;

                Pointer Allocate(SizeType size, SizeType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                void Deallocate(Pointer ptr) override;
                Pointer Reallocate(Pointer ptr, SizeType oldSize, SizeType newSize,
                                SizeType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                Pointer Calloc(SizeType size, SizeType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                const nk_char* Name() const noexcept override;
                void Reset() noexcept override;

                [[nodiscard]] SizeType BlockSize() const noexcept { return mBlockSize; }
                [[nodiscard]] SizeType BlockCount() const noexcept { return mBlockCount; }

            private:
                struct NkFreeNode {
                    NkFreeNode* next;
                };

                NkAllocator* mBacking;
                nk_uint8* mBuffer;
                NkFreeNode* mFreeList;
                SizeType mBlockSize;
                SizeType mBlockCount;
                nk_bool mOwnsBuffer;
        };

        class NKMEMORY_API NkFreeListAllocator final : public NkAllocator {
            public:
                explicit NkFreeListAllocator(SizeType capacityBytes,
                                            NkAllocator* backingAllocator = nullptr);
                ~NkFreeListAllocator() override;

                Pointer Allocate(SizeType size, SizeType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                void Deallocate(Pointer ptr) override;
                Pointer Reallocate(Pointer ptr, SizeType oldSize, SizeType newSize,
                                SizeType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                Pointer Calloc(SizeType size, SizeType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                const nk_char* Name() const noexcept override;
                void Reset() noexcept override;

                [[nodiscard]] SizeType Capacity() const noexcept { return mCapacity; }

            private:
                struct NkBlockHeader {
                    SizeType size;
                    NkBlockHeader* next;
                    NkBlockHeader* prev;
                    nk_bool isFree;
                };

                struct NkAllocationHeader {
                    NkBlockHeader* block;
                };

                NkAllocator* mBacking;
                nk_uint8* mBuffer;
                SizeType mCapacity;
                NkBlockHeader* mHead;
                nk_bool mOwnsBuffer;

                void Coalesce(NkBlockHeader* block) noexcept;
        };

        class NKMEMORY_API NkBuddyAllocator final : public NkAllocator {
            public:
                explicit NkBuddyAllocator(SizeType capacityBytes,
                                        SizeType minBlockSize = 32u,
                                        NkAllocator* backingAllocator = nullptr);
                ~NkBuddyAllocator() override;

                Pointer Allocate(SizeType size, SizeType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                void Deallocate(Pointer ptr) override;
                Pointer Reallocate(Pointer ptr, SizeType oldSize, SizeType newSize,
                                SizeType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                Pointer Calloc(SizeType size, SizeType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                const nk_char* Name() const noexcept override;
                void Reset() noexcept override;

            private:
                SizeType mMinBlockSize;
                NkFreeListAllocator mBackend;
        };

        NKMEMORY_API NkAllocator& NkGetDefaultAllocator() noexcept;
        NKMEMORY_API NkAllocator& NkGetMallocAllocator() noexcept;
        NKMEMORY_API NkAllocator& NkGetNewAllocator() noexcept;
        NKMEMORY_API NkAllocator& NkGetVirtualAllocator() noexcept;
        NKMEMORY_API void NkSetDefaultAllocator(NkAllocator* allocator) noexcept;

        NKMEMORY_API void* NkAlloc(nk_size size, NkAllocator* allocator = nullptr,
                                nk_size alignment = NK_MEMORY_DEFAULT_ALIGNMENT);
        NKMEMORY_API void* NkAllocZero(nk_size count, nk_size size, NkAllocator* allocator = nullptr,
                                    nk_size alignment = NK_MEMORY_DEFAULT_ALIGNMENT);
        NKMEMORY_API void* NkRealloc(void* ptr, nk_size oldSize, nk_size newSize,
                                    NkAllocator* allocator = nullptr,
                                    nk_size alignment = NK_MEMORY_DEFAULT_ALIGNMENT);
        NKMEMORY_API void NkFree(void* ptr, NkAllocator* allocator = nullptr);

    } // namespace memory
} // namespace nkentseu

#endif // NKENTSEU_MEMORY_NKALLOCATOR_H_INCLUDED
