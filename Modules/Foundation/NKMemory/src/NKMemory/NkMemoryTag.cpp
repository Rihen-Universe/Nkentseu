// NkMemoryTag.cpp
#include "NkMemoryTag.h"
#include <cstdio>

namespace nkentseu::memory {
    static nk_uint64 gMemoryBudgets[256] = {0};
    
    void NkMemoryBudget::SetBudget(NkMemoryTag tag, nk_uint64 bytes) noexcept {
        gMemoryBudgets[static_cast<nk_uint8>(tag)] = bytes;
    }
    
    nk_int64 NkMemoryBudget::GetBudgetRemaining(NkMemoryTag tag) noexcept {
        // Simplified: would track actual allocations per tag
        return static_cast<nk_int64>(gMemoryBudgets[static_cast<nk_uint8>(tag)]);
    }
    
    NkMemoryTagStats NkMemoryBudget::GetStats(NkMemoryTag tag) noexcept {
        return NkMemoryTagStats();
    }
    
    void NkMemoryBudget::DumpStats() noexcept {
        printf("[NKMemory] Memory tag stats:\n");
    }
    
    nk_bool NkMemoryBudget::IsOverBudget(NkMemoryTag tag) noexcept {
        return false;
    }
}
