// ============================================================
// FILE: NkMemoryProfiler.cpp
// DESCRIPTION: Memory profiler implementation
// ============================================================

#include "NKMemory/NkMemoryProfiler.h"

namespace nkentseu {
namespace memory {

    // Initialisation des static members
    AllocCallback NkMemoryProfiler::sAllocCB = nullptr;
    FreeCallback NkMemoryProfiler::sFreeCB = nullptr;
    ReallocCallback NkMemoryProfiler::sReallocCB = nullptr;

    void NkMemoryProfiler::SetAllocCallback(AllocCallback cb) noexcept {
        sAllocCB = cb;
    }

    void NkMemoryProfiler::SetFreeCallback(FreeCallback cb) noexcept {
        sFreeCB = cb;
    }

    void NkMemoryProfiler::SetReallocCallback(ReallocCallback cb) noexcept {
        sReallocCB = cb;
    }

    NkMemoryProfiler::GlobalStats NkMemoryProfiler::GetGlobalStats() noexcept {
        // TODO: Return actual stats from global tracker
        return GlobalStats{0, 0, 0, 0, 0, 0.0f};
    }

    void NkMemoryProfiler::DumpProfilerStats() noexcept {
        // TODO: Dump profiler statistics
    }

} // namespace memory
} // namespace nkentseu
