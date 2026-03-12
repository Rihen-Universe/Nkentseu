// ============================================================
// FILE: NkProfiler.cpp
// DESCRIPTION: Memory profiler implementation
// ============================================================

#include "NKMemory/NkProfiler.h"
#include "NKPlatform/NkFoundationLog.h"

namespace nkentseu {
namespace memory {

    // Initialisation des static members
    AllocCallback NkMemoryProfiler::sAllocCB = nullptr;
    FreeCallback NkMemoryProfiler::sFreeCB = nullptr;
    ReallocCallback NkMemoryProfiler::sReallocCB = nullptr;
    NkMemoryProfiler::GlobalStats NkMemoryProfiler::sStats = {0u, 0u, 0u, 0u, 0u, 0.0f};
    NkSpinLock NkMemoryProfiler::sLock;
    
    namespace {
        static nk_uint64 sTotalAllocatedBytes = 0u;
    }

    void NkMemoryProfiler::SetAllocCallback(AllocCallback cb) noexcept {
        NkScopedSpinLock guard(sLock);
        sAllocCB = cb;
    }

    void NkMemoryProfiler::SetFreeCallback(FreeCallback cb) noexcept {
        NkScopedSpinLock guard(sLock);
        sFreeCB = cb;
    }

    void NkMemoryProfiler::SetReallocCallback(ReallocCallback cb) noexcept {
        NkScopedSpinLock guard(sLock);
        sReallocCB = cb;
    }

    NkMemoryProfiler::GlobalStats NkMemoryProfiler::GetGlobalStats() noexcept {
        NkScopedSpinLock guard(sLock);
        return sStats;
    }
    
    void NkMemoryProfiler::NotifyAlloc(void* ptr, nk_size size, const nk_char* tag) noexcept {
        AllocCallback callback = nullptr;
        {
            NkScopedSpinLock guard(sLock);
            callback = sAllocCB;
            if (ptr && size > 0u) {
                ++sStats.totalAllocations;
                ++sStats.liveAllocations;
                sStats.liveBytes += size;
                sTotalAllocatedBytes += size;
                if (sStats.liveBytes > sStats.peakBytes) {
                    sStats.peakBytes = sStats.liveBytes;
                }
                sStats.avgAllocationSize = sStats.totalAllocations > 0u
                    ? static_cast<float32>(sTotalAllocatedBytes / sStats.totalAllocations)
                    : 0.0f;
            }
        }
        
        if (callback) {
            callback(ptr, size, tag);
        }
    }
    
    void NkMemoryProfiler::NotifyFree(void* ptr, nk_size size) noexcept {
        FreeCallback callback = nullptr;
        {
            NkScopedSpinLock guard(sLock);
            callback = sFreeCB;
            if (ptr && size > 0u) {
                ++sStats.totalFrees;
                if (sStats.liveAllocations > 0u) {
                    --sStats.liveAllocations;
                }
                if (sStats.liveBytes >= size) {
                    sStats.liveBytes -= size;
                } else {
                    sStats.liveBytes = 0u;
                }
            }
        }
        
        if (callback) {
            callback(ptr, size);
        }
    }
    
    void NkMemoryProfiler::NotifyRealloc(void* oldPtr, void* newPtr, nk_size oldSize, nk_size newSize) noexcept {
        ReallocCallback callback = nullptr;
        {
            NkScopedSpinLock guard(sLock);
            callback = sReallocCB;
            if (oldPtr != newPtr) {
                if (sStats.liveBytes >= oldSize) {
                    sStats.liveBytes -= oldSize;
                } else {
                    sStats.liveBytes = 0u;
                }
                sStats.liveBytes += newSize;
                if (sStats.liveBytes > sStats.peakBytes) {
                    sStats.peakBytes = sStats.liveBytes;
                }
            } else if (newSize > oldSize) {
                sStats.liveBytes += (newSize - oldSize);
                if (sStats.liveBytes > sStats.peakBytes) {
                    sStats.peakBytes = sStats.liveBytes;
                }
            } else if (oldSize > newSize) {
                sStats.liveBytes -= (oldSize - newSize);
            }
        }
        
        if (callback) {
            callback(oldPtr, newPtr, oldSize, newSize);
        }
    }

    void NkMemoryProfiler::DumpProfilerStats() noexcept {
        const GlobalStats stats = GetGlobalStats();
        NK_FOUNDATION_LOG_INFO("[NKMemory][Profiler] total allocs=%llu, frees=%llu, live allocs=%llu",
                               static_cast<unsigned long long>(stats.totalAllocations),
                               static_cast<unsigned long long>(stats.totalFrees),
                               static_cast<unsigned long long>(stats.liveAllocations));
        NK_FOUNDATION_LOG_INFO("[NKMemory][Profiler] live bytes=%llu, peak bytes=%llu, avg alloc=%.2f bytes",
                               static_cast<unsigned long long>(stats.liveBytes),
                               static_cast<unsigned long long>(stats.peakBytes),
                               static_cast<double>(stats.avgAllocationSize));
    }

} // namespace memory
} // namespace nkentseu
