#include "NKMemory/NkAllocator.h"

#include <stdlib.h>

#if defined(_WIN32) || defined(_WIN64)
#include <malloc.h>
#include <windows.h>
#elif defined(NKENTSEU_PLATFORM_POSIX)
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace nkentseu {
    namespace memory {

            namespace {

                inline nk_size NkMaxSize(nk_size a, nk_size b) {
                    return a > b ? a : b;
                }

                inline nk_size NkMinSize(nk_size a, nk_size b) {
                    return a < b ? a : b;
                }

                inline nk_size NkNextPow2(nk_size value) {
                    if (value <= 1u) {
                        return 1u;
                    }
                    --value;
                    for (nk_size shift = 1u; shift < sizeof(nk_size) * 8u; shift <<= 1u) {
                        value |= value >> shift;
                    }
                    return value + 1u;
                }

                struct NkAlignedHeader {
                    void* rawPtr;
                    nk_size totalSize; // stored for C++14 sized operator delete
                };

                void* NkAllocateAlignedByNew(nk_size size, nk_size alignment) {
                    if (size == 0u) {
                        return nullptr;
                    }

                    if (!NkIsPowerOfTwo(alignment)) {
                        alignment = NK_MEMORY_DEFAULT_ALIGNMENT;
                    }
                    alignment = NkMaxSize(alignment, NK_MEMORY_DEFAULT_ALIGNMENT);

                    const nk_size total = size + alignment + sizeof(NkAlignedHeader);
                    void* raw = nullptr;
                    try {
                        raw = ::operator new(total);
                    } catch (...) {
                        raw = nullptr;
                    }
                    if (!raw) {
                        return nullptr;
                    }

                    nk_uint8* begin = static_cast<nk_uint8*>(raw) + sizeof(NkAlignedHeader);
                    const nk_size beginAddr = reinterpret_cast<nk_size>(begin);
                    const nk_size alignedAddr = NkAlignUp(beginAddr, alignment);
                    auto* aligned = reinterpret_cast<nk_uint8*>(alignedAddr);

                    auto* header = reinterpret_cast<NkAlignedHeader*>(aligned - sizeof(NkAlignedHeader));
                    header->rawPtr = raw;
                    header->totalSize = total;
                    return aligned;
                }

                void NkFreeAlignedByNew(void* ptr) {
                    if (!ptr) {
                        return;
                    }
                    auto* aligned = static_cast<nk_uint8*>(ptr);
                    auto* header = reinterpret_cast<NkAlignedHeader*>(aligned - sizeof(NkAlignedHeader));
#if __cpp_sized_deallocation >= 201309L
                    ::operator delete(header->rawPtr, header->totalSize);
#else
                    ::operator delete(header->rawPtr);
#endif
                }

                void* NkAllocateAlignedByMalloc(nk_size size, nk_size alignment) {
                    if (size == 0u) {
                        return nullptr;
                    }

                    if (!NkIsPowerOfTwo(alignment)) {
                        alignment = NK_MEMORY_DEFAULT_ALIGNMENT;
                    }
                    alignment = NkMaxSize(alignment, NK_MEMORY_DEFAULT_ALIGNMENT);

                #if defined(_WIN32) || defined(_WIN64)
                    return _aligned_malloc(size, alignment);
                #else
                    void* ptr = nullptr;
                    if (alignment < sizeof(void*)) {
                        alignment = sizeof(void*);
                    }
                    if (posix_memalign(&ptr, alignment, size) != 0) {
                        return nullptr;
                    }
                    return ptr;
                #endif
                }

                void NkFreeAlignedByMalloc(void* ptr) {
                    if (!ptr) {
                        return;
                    }
                #if defined(_WIN32) || defined(_WIN64)
                    _aligned_free(ptr);
                #else
                    free(ptr);
                #endif
                }

                NkMallocAllocator& NkGlobalMallocAllocator() {
                    static NkMallocAllocator* instance = new NkMallocAllocator();
                    return *instance;
                }

                NkNewAllocator& NkGlobalNewAllocator() {
                    static NkNewAllocator* instance = new NkNewAllocator();
                    return *instance;
                }

                NkVirtualAllocator& NkGlobalVirtualAllocator() {
                    static NkVirtualAllocator* instance = new NkVirtualAllocator();
                    return *instance;
                }

                NkAllocator* gDefaultAllocator = nullptr;

            } // namespace

        NkAllocator::Pointer NkAllocator::Reallocate(Pointer ptr, SizeType oldSize, SizeType newSize,
                                                    SizeType alignment) {
            if (!ptr) {
                return Allocate(newSize, alignment);
            }
            if (newSize == 0u) {
                Deallocate(ptr);
                return nullptr;
            }

            Pointer newPtr = Allocate(newSize, alignment);
            if (!newPtr) {
                return nullptr;
            }

            const SizeType copySize = NkMinSize(oldSize, newSize);
            NkMemCopy(newPtr, ptr, copySize);
            Deallocate(ptr);
            return newPtr;
        }

        NkAllocator::Pointer NkAllocator::Calloc(SizeType size, SizeType alignment) {
            Pointer ptr = Allocate(size, alignment);
            if (ptr) {
                NkMemZero(ptr, size);
            }
            return ptr;
        }

        NkAllocator::Pointer NkNewAllocator::Allocate(SizeType size, SizeType alignment) {
            return NkAllocateAlignedByNew(size, alignment);
        }

        void NkNewAllocator::Deallocate(Pointer ptr) {
            NkFreeAlignedByNew(ptr);
        }

        NkAllocator::Pointer NkNewAllocator::Reallocate(Pointer ptr, SizeType oldSize, SizeType newSize,
                                                        SizeType alignment) {
            if (!ptr) {
                return Allocate(newSize, alignment);
            }
            if (newSize == 0u) {
                Deallocate(ptr);
                return nullptr;
            }
            Pointer newPtr = Allocate(newSize, alignment);
            if (!newPtr) {
                return nullptr;
            }
            NkMemCopy(newPtr, ptr, NkMinSize(oldSize, newSize));
            Deallocate(ptr);
            return newPtr;
        }

        NkAllocator::Pointer NkNewAllocator::Calloc(SizeType size, SizeType alignment) {
            Pointer ptr = Allocate(size, alignment);
            if (ptr) {
                NkMemZero(ptr, size);
            }
            return ptr;
        }

        NkAllocator::Pointer NkMallocAllocator::Allocate(SizeType size, SizeType alignment) {
            return NkAllocateAlignedByMalloc(size, alignment);
        }

        void NkMallocAllocator::Deallocate(Pointer ptr) {
            NkFreeAlignedByMalloc(ptr);
        }

        void NkMallocAllocator::Deallocate(Pointer ptr, SizeType size) {
            if (!ptr) {
                return;
            }
#if !defined(_WIN32) && !defined(_WIN64) && defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
            ::free_sized(ptr, size);
#else
            (void)size;
            NkFreeAlignedByMalloc(ptr);
#endif
        }

        NkAllocator::Pointer NkMallocAllocator::Reallocate(Pointer ptr, SizeType oldSize, SizeType newSize,
                                                        SizeType alignment) {
            if (!ptr) {
                return Allocate(newSize, alignment);
            }
            if (newSize == 0u) {
                Deallocate(ptr);
                return nullptr;
            }

        #if defined(_WIN32) || defined(_WIN64)
            if (!NkIsPowerOfTwo(alignment)) {
                alignment = NK_MEMORY_DEFAULT_ALIGNMENT;
            }
            alignment = NkMaxSize(alignment, NK_MEMORY_DEFAULT_ALIGNMENT);
            if (void* resized = _aligned_realloc(ptr, newSize, alignment)) {
                return resized;
            }
        #endif

            Pointer newPtr = Allocate(newSize, alignment);
            if (!newPtr) {
                return nullptr;
            }
            NkMemCopy(newPtr, ptr, NkMinSize(oldSize, newSize));
            Deallocate(ptr);
            return newPtr;
        }

        NkAllocator::Pointer NkMallocAllocator::Calloc(SizeType size, SizeType alignment) {
            if (size == 0u) {
                return nullptr;
            }

            Pointer ptr = Allocate(size, alignment);
            if (ptr) {
                NkMemZero(ptr, size);
            }
            return ptr;
        }

        NkVirtualAllocator::NkVirtualAllocator() noexcept
            : NkAllocator("NkVirtualAllocator"),
            flags(NkMemoryFlag_Read | NkMemoryFlag_Write | NkMemoryFlag_Reserve |
                    NkMemoryFlag_Commit | NkMemoryFlag_Anonymous),
            mBlocks(nullptr) {}

        NkVirtualAllocator::~NkVirtualAllocator() {
            NkVirtualBlock* current = mBlocks;
            while (current) {
                NkVirtualBlock* next = current->next;
        #if defined(_WIN32) || defined(_WIN64)
                VirtualFree(current->ptr, 0, MEM_RELEASE);
        #elif defined(NKENTSEU_PLATFORM_POSIX)
                munmap(current->ptr, current->size);
        #endif
                NkGlobalMallocAllocator().Deallocate(current);
                current = next;
            }
            mBlocks = nullptr;
        }

        nk_size NkVirtualAllocator::PageSize() const noexcept {
        #if defined(_WIN32) || defined(_WIN64)
            SYSTEM_INFO info{};
            GetSystemInfo(&info);
            return static_cast<nk_size>(info.dwPageSize);
        #elif defined(NKENTSEU_PLATFORM_POSIX)
            long page = sysconf(_SC_PAGESIZE);
            return page > 0 ? static_cast<nk_size>(page) : 4096u;
        #else
            return 4096u;
        #endif
        }

        nk_size NkVirtualAllocator::NormalizeSize(SizeType size) const noexcept {
            const nk_size page = PageSize();
            return NkAlignUp(size, page);
        }

        void NkVirtualAllocator::TrackBlock(Pointer ptr, SizeType size) {
            auto* node = static_cast<NkVirtualBlock*>(
                NkGlobalMallocAllocator().Allocate(sizeof(NkVirtualBlock), alignof(NkVirtualBlock)));
            if (!node) {
                return;
            }
            node->ptr = ptr;
            node->size = size;
            node->next = mBlocks;
            mBlocks = node;
        }

        nk_size NkVirtualAllocator::UntrackBlock(Pointer ptr) noexcept {
            NkVirtualBlock* prev = nullptr;
            NkVirtualBlock* cur = mBlocks;
            while (cur) {
                if (cur->ptr == ptr) {
                    const nk_size size = cur->size;
                    if (prev) {
                        prev->next = cur->next;
                    } else {
                        mBlocks = cur->next;
                    }
                    NkGlobalMallocAllocator().Deallocate(cur);
                    return size;
                }
                prev = cur;
                cur = cur->next;
            }
            return 0u;
        }

        NkAllocator::Pointer NkVirtualAllocator::AllocateVirtual(SizeType size, const nk_char* /*tag*/) noexcept {
            if (size == 0u) {
                return nullptr;
            }

            const nk_size normalized = NormalizeSize(size);
            void* address = nullptr;

        #if defined(_WIN32) || defined(_WIN64)
            DWORD protection = PAGE_NOACCESS;
            if ((flags & NkMemoryFlag_Read) && (flags & NkMemoryFlag_Write)) {
                protection = PAGE_READWRITE;
            } else if (flags & NkMemoryFlag_Read) {
                protection = PAGE_READONLY;
            } else if (flags & NkMemoryFlag_Execute) {
                protection = PAGE_EXECUTE_READWRITE;
            }

            DWORD allocFlags = 0;
            allocFlags |= (flags & NkMemoryFlag_Reserve) ? MEM_RESERVE : 0;
            allocFlags |= (flags & NkMemoryFlag_Commit) ? MEM_COMMIT : 0;
            if (allocFlags == 0) {
                allocFlags = MEM_RESERVE | MEM_COMMIT;
            }

            address = VirtualAlloc(nullptr, normalized, allocFlags, protection);
        #elif defined(NKENTSEU_PLATFORM_POSIX)
            int protection = PROT_NONE;
            if (flags & NkMemoryFlag_Read) {
                protection |= PROT_READ;
            }
            if (flags & NkMemoryFlag_Write) {
                protection |= PROT_WRITE;
            }
            if (flags & NkMemoryFlag_Execute) {
                protection |= PROT_EXEC;
            }

            int mapFlags = MAP_PRIVATE;
        #if defined(MAP_ANONYMOUS)
            mapFlags |= MAP_ANONYMOUS;
        #endif
            address = mmap(nullptr, normalized, protection, mapFlags, -1, 0);
            if (address == MAP_FAILED) {
                address = nullptr;
            }
        #endif

            if (address) {
                TrackBlock(address, normalized);
            }
            return address;
        }

        void NkVirtualAllocator::FreeVirtual(Pointer ptr) noexcept {
            if (!ptr) {
                return;
            }
            const nk_size size = UntrackBlock(ptr);
            if (size == 0u) {
                return;
            }
        #if defined(_WIN32) || defined(_WIN64)
            VirtualFree(ptr, 0, MEM_RELEASE);
        #elif defined(NKENTSEU_PLATFORM_POSIX)
            munmap(ptr, size);
        #endif
        }

        NkAllocator::Pointer NkVirtualAllocator::Allocate(SizeType size, SizeType /*alignment*/) {
            return AllocateVirtual(size, Name());
        }

        void NkVirtualAllocator::Deallocate(Pointer ptr) {
            FreeVirtual(ptr);
        }

        NkAllocator::Pointer NkVirtualAllocator::Reallocate(Pointer ptr, SizeType oldSize, SizeType newSize,
                                                            SizeType alignment) {
            if (!ptr) {
                return Allocate(newSize, alignment);
            }
            if (newSize == 0u) {
                Deallocate(ptr);
                return nullptr;
            }
            Pointer newPtr = Allocate(newSize, alignment);
            if (!newPtr) {
                return nullptr;
            }
            NkMemCopy(newPtr, ptr, NkMinSize(oldSize, newSize));
            Deallocate(ptr);
            return newPtr;
        }

        NkAllocator::Pointer NkVirtualAllocator::Calloc(SizeType size, SizeType alignment) {
            Pointer ptr = Allocate(size, alignment);
            if (ptr) {
                NkMemZero(ptr, size);
            }
            return ptr;
        }

        NkLinearAllocator::NkLinearAllocator(SizeType capacityBytes, NkAllocator* backingAllocator)
            : NkAllocator("NkLinearAllocator"),
            mBacking(backingAllocator ? backingAllocator : &NkGetDefaultAllocator()),
            mBuffer(nullptr),
            mCapacity(capacityBytes),
            mOffset(0u),
            mOwnsBuffer(true) {
            mBuffer = static_cast<nk_uint8*>(mBacking->Allocate(capacityBytes, NK_MEMORY_DEFAULT_ALIGNMENT));
            if (!mBuffer) {
                mCapacity = 0u;
                mOwnsBuffer = false;
            }
        }

        NkLinearAllocator::~NkLinearAllocator() {
            if (mOwnsBuffer && mBuffer && mBacking) {
                mBacking->Deallocate(mBuffer);
            }
            mBuffer = nullptr;
            mCapacity = 0u;
            mOffset = 0u;
        }

        NkAllocator::Pointer NkLinearAllocator::Allocate(SizeType size, SizeType alignment) {
            if (!mBuffer || size == 0u) {
                return nullptr;
            }
            if (!NkIsPowerOfTwo(alignment)) {
                alignment = NK_MEMORY_DEFAULT_ALIGNMENT;
            }

            const SizeType base = reinterpret_cast<SizeType>(mBuffer) + mOffset;
            const SizeType aligned = NkAlignUp(base, alignment);
            const SizeType adjustment = aligned - base;
            if (mOffset + adjustment + size > mCapacity) {
                return nullptr;
            }

            mOffset += adjustment;
            Pointer result = mBuffer + mOffset;
            mOffset += size;
            return result;
        }

        void NkLinearAllocator::Deallocate(Pointer /*ptr*/) {
            // Linear allocator frees all memory through Reset.
        }

        void NkLinearAllocator::Deallocate(Pointer ptr, SizeType size) {
            if (!ptr || !mBuffer || size == 0u) {
                return;
            }
            // Top-of-stack optimization: reclaim if ptr is the last allocation.
            auto* p = static_cast<nk_uint8*>(ptr);
            if (p + size == mBuffer + mOffset) {
                mOffset -= size;
            }
            // Otherwise: no-op — linear allocator only supports full Reset().
        }

        NkAllocator::Pointer NkLinearAllocator::Reallocate(Pointer ptr, SizeType oldSize, SizeType newSize,
                                                        SizeType alignment) {
            if (!ptr) {
                return Allocate(newSize, alignment);
            }
            if (newSize <= oldSize) {
                return ptr;
            }
            Pointer newPtr = Allocate(newSize, alignment);
            if (!newPtr) {
                return nullptr;
            }
            NkMemCopy(newPtr, ptr, oldSize);
            return newPtr;
        }

        NkAllocator::Pointer NkLinearAllocator::Calloc(SizeType size, SizeType alignment) {
            Pointer ptr = Allocate(size, alignment);
            if (ptr) {
                NkMemZero(ptr, size);
            }
            return ptr;
        }

        const nk_char* NkLinearAllocator::Name() const noexcept {
            return "NkLinearAllocator";
        }

        void NkLinearAllocator::Reset() noexcept {
            mOffset = 0u;
        }

        NkArenaAllocator::NkArenaAllocator(SizeType capacityBytes, NkAllocator* backingAllocator)
            : NkAllocator("NkArenaAllocator"),
            mBacking(backingAllocator ? backingAllocator : &NkGetDefaultAllocator()),
            mBuffer(nullptr),
            mCapacity(capacityBytes),
            mOffset(0u),
            mOwnsBuffer(true) {
            mBuffer = static_cast<nk_uint8*>(mBacking->Allocate(capacityBytes, NK_MEMORY_DEFAULT_ALIGNMENT));
            if (!mBuffer) {
                mCapacity = 0u;
                mOwnsBuffer = false;
            }
        }

        NkArenaAllocator::~NkArenaAllocator() {
            if (mOwnsBuffer && mBuffer && mBacking) {
                mBacking->Deallocate(mBuffer);
            }
        }

        NkAllocator::Pointer NkArenaAllocator::Allocate(SizeType size, SizeType alignment) {
            if (!mBuffer || size == 0u) {
                return nullptr;
            }
            if (!NkIsPowerOfTwo(alignment)) {
                alignment = NK_MEMORY_DEFAULT_ALIGNMENT;
            }

            const SizeType base = reinterpret_cast<SizeType>(mBuffer) + mOffset;
            const SizeType aligned = NkAlignUp(base, alignment);
            const SizeType adjustment = aligned - base;
            if (mOffset + adjustment + size > mCapacity) {
                return nullptr;
            }
            mOffset += adjustment;
            Pointer result = mBuffer + mOffset;
            mOffset += size;
            return result;
        }

        void NkArenaAllocator::Deallocate(Pointer /*ptr*/) {
            // Arena allocator only supports marker or full reset.
        }

        NkAllocator::Pointer NkArenaAllocator::Reallocate(Pointer ptr, SizeType oldSize, SizeType newSize,
                                                        SizeType alignment) {
            if (!ptr) {
                return Allocate(newSize, alignment);
            }
            if (newSize <= oldSize) {
                return ptr;
            }
            Pointer newPtr = Allocate(newSize, alignment);
            if (!newPtr) {
                return nullptr;
            }
            NkMemCopy(newPtr, ptr, oldSize);
            return newPtr;
        }

        NkAllocator::Pointer NkArenaAllocator::Calloc(SizeType size, SizeType alignment) {
            Pointer ptr = Allocate(size, alignment);
            if (ptr) {
                NkMemZero(ptr, size);
            }
            return ptr;
        }

        const nk_char* NkArenaAllocator::Name() const noexcept {
            return "NkArenaAllocator";
        }

        void NkArenaAllocator::Reset() noexcept {
            mOffset = 0u;
        }

        NkArenaAllocator::Marker NkArenaAllocator::CreateMarker() const noexcept {
            return mOffset;
        }

        void NkArenaAllocator::FreeToMarker(Marker marker) noexcept {
            if (marker <= mOffset) {
                mOffset = marker;
            }
        }

        NkStackAllocator::NkStackAllocator(SizeType capacityBytes, NkAllocator* backingAllocator)
            : NkAllocator("NkStackAllocator"),
            mBacking(backingAllocator ? backingAllocator : &NkGetDefaultAllocator()),
            mBuffer(nullptr),
            mCapacity(capacityBytes),
            mOffset(0u),
            mLastAllocation(nullptr),
            mOwnsBuffer(true) {
            mBuffer = static_cast<nk_uint8*>(mBacking->Allocate(capacityBytes, NK_MEMORY_DEFAULT_ALIGNMENT));
            if (!mBuffer) {
                mCapacity = 0u;
                mOwnsBuffer = false;
            }
        }

        NkStackAllocator::~NkStackAllocator() {
            if (mOwnsBuffer && mBuffer && mBacking) {
                mBacking->Deallocate(mBuffer);
            }
        }

        NkAllocator::Pointer NkStackAllocator::Allocate(SizeType size, SizeType alignment) {
            if (!mBuffer || size == 0u) {
                return nullptr;
            }
            if (!NkIsPowerOfTwo(alignment)) {
                alignment = NK_MEMORY_DEFAULT_ALIGNMENT;
            }

            const SizeType stackBase = reinterpret_cast<SizeType>(mBuffer) + mOffset;
            const SizeType headerEnd = stackBase + sizeof(NkAllocationHeader);
            const SizeType alignedAddr = NkAlignUp(headerEnd, alignment);
            const SizeType used = (alignedAddr + size) - stackBase;

            if (mOffset + used > mCapacity) {
                return nullptr;
            }

            auto* header = reinterpret_cast<NkAllocationHeader*>(alignedAddr - sizeof(NkAllocationHeader));
            header->previousOffset = mOffset;
            header->previousAllocation = mLastAllocation;

            mOffset += used;
            mLastAllocation = reinterpret_cast<Pointer>(alignedAddr);
            return mLastAllocation;
        }

        void NkStackAllocator::Deallocate(Pointer ptr) {
            if (!ptr || ptr != mLastAllocation) {
                return;
            }
            auto* header = reinterpret_cast<NkAllocationHeader*>(
                static_cast<nk_uint8*>(ptr) - sizeof(NkAllocationHeader));
            mOffset = header->previousOffset;
            mLastAllocation = header->previousAllocation;
        }

        NkAllocator::Pointer NkStackAllocator::Reallocate(Pointer ptr, SizeType oldSize, SizeType newSize,
                                                        SizeType alignment) {
            if (!ptr) {
                return Allocate(newSize, alignment);
            }
            if (ptr != mLastAllocation) {
                return nullptr;
            }
            if (newSize == oldSize) {
                return ptr;
            }

            auto* header = reinterpret_cast<NkAllocationHeader*>(
                static_cast<nk_uint8*>(ptr) - sizeof(NkAllocationHeader));
            const SizeType stackBase = reinterpret_cast<SizeType>(mBuffer) + header->previousOffset;
            const SizeType ptrAddr = reinterpret_cast<SizeType>(ptr);
            const SizeType newUsed = (ptrAddr + newSize) - stackBase;
            if (header->previousOffset + newUsed > mCapacity) {
                return nullptr;
            }

            mOffset = header->previousOffset + newUsed;
            return ptr;
        }

        NkAllocator::Pointer NkStackAllocator::Calloc(SizeType size, SizeType alignment) {
            Pointer ptr = Allocate(size, alignment);
            if (ptr) {
                NkMemZero(ptr, size);
            }
            return ptr;
        }

        const nk_char* NkStackAllocator::Name() const noexcept {
            return "NkStackAllocator";
        }

        void NkStackAllocator::Reset() noexcept {
            mOffset = 0u;
            mLastAllocation = nullptr;
        }

        NkPoolAllocator::NkPoolAllocator(SizeType blockSize, SizeType blockCount, NkAllocator* backingAllocator)
            : NkAllocator("NkPoolAllocator"),
            mBacking(backingAllocator ? backingAllocator : &NkGetDefaultAllocator()),
            mBuffer(nullptr),
            mFreeList(nullptr),
            mBlockSize(NkMaxSize(blockSize, static_cast<SizeType>(sizeof(NkFreeNode)))),
            mBlockCount(blockCount),
            mOwnsBuffer(true) {
            if (mBlockSize == 0u || mBlockCount == 0u) {
                mOwnsBuffer = false;
                return;
            }

            const SizeType totalSize = mBlockSize * mBlockCount;
            mBuffer = static_cast<nk_uint8*>(mBacking->Allocate(totalSize, NK_MEMORY_DEFAULT_ALIGNMENT));
            if (!mBuffer) {
                mOwnsBuffer = false;
                return;
            }
            Reset();
        }

        NkPoolAllocator::~NkPoolAllocator() {
            if (mOwnsBuffer && mBuffer && mBacking) {
                mBacking->Deallocate(mBuffer);
            }
        }

        NkAllocator::Pointer NkPoolAllocator::Allocate(SizeType size, SizeType /*alignment*/) {
            if (!mFreeList || size == 0u || size > mBlockSize) {
                return nullptr;
            }
            NkFreeNode* node = mFreeList;
            mFreeList = mFreeList->next;
            return node;
        }

        void NkPoolAllocator::Deallocate(Pointer ptr) {
            if (!ptr) {
                return;
            }
            auto* node = static_cast<NkFreeNode*>(ptr);
            node->next = mFreeList;
            mFreeList = node;
        }

        NkAllocator::Pointer NkPoolAllocator::Reallocate(Pointer ptr, SizeType oldSize, SizeType newSize,
                                                        SizeType alignment) {
            if (!ptr) {
                return Allocate(newSize, alignment);
            }
            if (newSize == 0u) {
                Deallocate(ptr);
                return nullptr;
            }
            if (newSize <= mBlockSize) {
                return ptr;
            }

            Pointer newPtr = Allocate(newSize, alignment);
            if (!newPtr) {
                return nullptr;
            }
            NkMemCopy(newPtr, ptr, NkMinSize(oldSize, newSize));
            Deallocate(ptr);
            return newPtr;
        }

        NkAllocator::Pointer NkPoolAllocator::Calloc(SizeType size, SizeType alignment) {
            Pointer ptr = Allocate(size, alignment);
            if (ptr) {
                NkMemZero(ptr, mBlockSize);
            }
            return ptr;
        }

        const nk_char* NkPoolAllocator::Name() const noexcept {
            return "NkPoolAllocator";
        }

        void NkPoolAllocator::Reset() noexcept {
            mFreeList = nullptr;
            if (!mBuffer) {
                return;
            }

            for (SizeType i = 0u; i < mBlockCount; ++i) {
                auto* node = reinterpret_cast<NkFreeNode*>(mBuffer + i * mBlockSize);
                node->next = mFreeList;
                mFreeList = node;
            }
        }

        NkFreeListAllocator::NkFreeListAllocator(SizeType capacityBytes, NkAllocator* backingAllocator)
            : NkAllocator("NkFreeListAllocator"),
            mBacking(backingAllocator ? backingAllocator : &NkGetDefaultAllocator()),
            mBuffer(nullptr),
            mCapacity(capacityBytes),
            mHead(nullptr),
            mOwnsBuffer(true) {
            mBuffer = static_cast<nk_uint8*>(mBacking->Allocate(capacityBytes, NK_MEMORY_DEFAULT_ALIGNMENT));
            if (!mBuffer) {
                mCapacity = 0u;
                mOwnsBuffer = false;
                return;
            }
            Reset();
        }

        NkFreeListAllocator::~NkFreeListAllocator() {
            if (mOwnsBuffer && mBuffer && mBacking) {
                mBacking->Deallocate(mBuffer);
            }
        }

        NkAllocator::Pointer NkFreeListAllocator::Allocate(SizeType size, SizeType alignment) {
            if (!mHead || size == 0u) {
                return nullptr;
            }
            if (!NkIsPowerOfTwo(alignment)) {
                alignment = NK_MEMORY_DEFAULT_ALIGNMENT;
            }

            NkBlockHeader* block = mHead;
            while (block) {
                if (!block->isFree) {
                    block = block->next;
                    continue;
                }

                const nk_size blockStart = reinterpret_cast<nk_size>(block);
                const nk_size userStart = NkAlignUp(
                    blockStart + sizeof(NkBlockHeader) + sizeof(NkAllocationHeader), alignment);
                const nk_size usedSize = (userStart + size) - blockStart;
                if (usedSize > block->size) {
                    block = block->next;
                    continue;
                }

                const nk_size remaining = block->size - usedSize;
                if (remaining > sizeof(NkBlockHeader) + sizeof(NkAllocationHeader) + 8u) {
                    auto* newBlock = reinterpret_cast<NkBlockHeader*>(blockStart + usedSize);
                    newBlock->size = remaining;
                    newBlock->next = block->next;
                    newBlock->prev = block;
                    newBlock->isFree = true;
                    if (newBlock->next) {
                        newBlock->next->prev = newBlock;
                    }
                    block->next = newBlock;
                    block->size = usedSize;
                }

                block->isFree = false;
                auto* allocationHeader = reinterpret_cast<NkAllocationHeader*>(userStart - sizeof(NkAllocationHeader));
                allocationHeader->block = block;
                return reinterpret_cast<void*>(userStart);
            }

            return nullptr;
        }

        void NkFreeListAllocator::Coalesce(NkBlockHeader* block) noexcept {
            if (!block) {
                return;
            }

            if (block->next && block->next->isFree) {
                nk_uint8* blockEnd = reinterpret_cast<nk_uint8*>(block) + block->size;
                if (blockEnd == reinterpret_cast<nk_uint8*>(block->next)) {
                    NkBlockHeader* next = block->next;
                    block->size += next->size;
                    block->next = next->next;
                    if (block->next) {
                        block->next->prev = block;
                    }
                }
            }

            if (block->prev && block->prev->isFree) {
                nk_uint8* prevEnd = reinterpret_cast<nk_uint8*>(block->prev) + block->prev->size;
                if (prevEnd == reinterpret_cast<nk_uint8*>(block)) {
                    block->prev->size += block->size;
                    block->prev->next = block->next;
                    if (block->next) {
                        block->next->prev = block->prev;
                    }
                }
            }
        }

        void NkFreeListAllocator::Deallocate(Pointer ptr) {
            if (!ptr) {
                return;
            }

            auto* allocationHeader = reinterpret_cast<NkAllocationHeader*>(
                static_cast<nk_uint8*>(ptr) - sizeof(NkAllocationHeader));
            NkBlockHeader* block = allocationHeader->block;
            if (!block) {
                return;
            }
            block->isFree = true;
            Coalesce(block);
        }

        NkAllocator::Pointer NkFreeListAllocator::Reallocate(Pointer ptr, SizeType oldSize, SizeType newSize,
                                                            SizeType alignment) {
            if (!ptr) {
                return Allocate(newSize, alignment);
            }
            if (newSize == 0u) {
                Deallocate(ptr);
                return nullptr;
            }

            Pointer newPtr = Allocate(newSize, alignment);
            if (!newPtr) {
                return nullptr;
            }
            NkMemCopy(newPtr, ptr, NkMinSize(oldSize, newSize));
            Deallocate(ptr);
            return newPtr;
        }

        NkAllocator::Pointer NkFreeListAllocator::Calloc(SizeType size, SizeType alignment) {
            Pointer ptr = Allocate(size, alignment);
            if (ptr) {
                NkMemZero(ptr, size);
            }
            return ptr;
        }

        const nk_char* NkFreeListAllocator::Name() const noexcept {
            return "NkFreeListAllocator";
        }

        void NkFreeListAllocator::Reset() noexcept {
            if (!mBuffer || mCapacity < sizeof(NkBlockHeader)) {
                mHead = nullptr;
                return;
            }

            mHead = reinterpret_cast<NkBlockHeader*>(mBuffer);
            mHead->size = mCapacity;
            mHead->next = nullptr;
            mHead->prev = nullptr;
            mHead->isFree = true;
        }

        NkBuddyAllocator::NkBuddyAllocator(SizeType capacityBytes, SizeType minBlockSize, NkAllocator* backingAllocator)
            : NkAllocator("NkBuddyAllocator"),
            mMinBlockSize(NkMaxSize(minBlockSize, static_cast<SizeType>(16u))),
            mBackend(capacityBytes, backingAllocator) {}

        NkBuddyAllocator::~NkBuddyAllocator() = default;

        NkAllocator::Pointer NkBuddyAllocator::Allocate(SizeType size, SizeType alignment) {
            const SizeType rounded = NkMaxSize(mMinBlockSize, NkNextPow2(size));
            return mBackend.Allocate(rounded, alignment);
        }

        void NkBuddyAllocator::Deallocate(Pointer ptr) {
            mBackend.Deallocate(ptr);
        }

        NkAllocator::Pointer NkBuddyAllocator::Reallocate(Pointer ptr, SizeType oldSize, SizeType newSize,
                                                        SizeType alignment) {
            if (!ptr) {
                return Allocate(newSize, alignment);
            }
            if (newSize == 0u) {
                Deallocate(ptr);
                return nullptr;
            }
            Pointer newPtr = Allocate(newSize, alignment);
            if (!newPtr) {
                return nullptr;
            }
            NkMemCopy(newPtr, ptr, NkMinSize(oldSize, newSize));
            Deallocate(ptr);
            return newPtr;
        }

        NkAllocator::Pointer NkBuddyAllocator::Calloc(SizeType size, SizeType alignment) {
            Pointer ptr = Allocate(size, alignment);
            if (ptr) {
                NkMemZero(ptr, size);
            }
            return ptr;
        }

        const nk_char* NkBuddyAllocator::Name() const noexcept {
            return "NkBuddyAllocator";
        }

        void NkBuddyAllocator::Reset() noexcept {
            mBackend.Reset();
        }

        NkAllocator& NkGetDefaultAllocator() noexcept {
            if (!gDefaultAllocator) {
                gDefaultAllocator = &NkGlobalMallocAllocator();
            }
            return *gDefaultAllocator;
        }

        NkAllocator& NkGetMallocAllocator() noexcept {
            return NkGlobalMallocAllocator();
        }

        NkAllocator& NkGetNewAllocator() noexcept {
            return NkGlobalNewAllocator();
        }

        NkAllocator& NkGetVirtualAllocator() noexcept {
            return NkGlobalVirtualAllocator();
        }

        void NkSetDefaultAllocator(NkAllocator* allocator) noexcept {
            gDefaultAllocator = allocator ? allocator : &NkGlobalMallocAllocator();
        }

        void* NkAlloc(nk_size size, NkAllocator* allocator, nk_size alignment) {
            NkAllocator& alloc = allocator ? *allocator : NkGetDefaultAllocator();
            return alloc.Allocate(size, alignment);
        }

        void* NkAllocZero(nk_size count, nk_size size, NkAllocator* allocator, nk_size alignment) {
            if (count == 0u || size == 0u) {
                return nullptr;
            }
            NkAllocator& alloc = allocator ? *allocator : NkGetDefaultAllocator();
            return alloc.Calloc(count * size, alignment);
        }

        void* NkRealloc(void* ptr, nk_size oldSize, nk_size newSize, NkAllocator* allocator, nk_size alignment) {
            NkAllocator& alloc = allocator ? *allocator : NkGetDefaultAllocator();
            return alloc.Reallocate(ptr, oldSize, newSize, alignment);
        }

        void NkFree(void* ptr, NkAllocator* allocator) {
            if (!ptr) {
                return;
            }
            NkAllocator& alloc = allocator ? *allocator : NkGetDefaultAllocator();
            alloc.Deallocate(ptr);
        }

        void NkFree(void* ptr, nk_size size, NkAllocator* allocator) {
            if (!ptr) {
                return;
            }
            NkAllocator& alloc = allocator ? *allocator : NkGetDefaultAllocator();
            alloc.Deallocate(ptr, size);
        }

    } // namespace memory
} // namespace nkentseu
