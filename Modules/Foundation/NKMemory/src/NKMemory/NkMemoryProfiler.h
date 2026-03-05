// ============================================================
// FILE: NkMemoryProfiler.h
// DESCRIPTION: Runtime memory profiling hooks
// AUTHOR: Rihen
// DATE: 2026-03-05
// VERSION: 1.0.0
// ============================================================
// Système de profiling mémoire permettant:
// - Sampling périodique d'allocations
// - Graphing patterns
// - Budget tracking
// - Real-time hitches detection
// ============================================================

#pragma once

#ifndef NKENTSEU_MEMORY_NKMEMORY_PROFILER_H_INCLUDED
#define NKENTSEU_MEMORY_NKMEMORY_PROFILER_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKMemory/NkMemoryExport.h"

#include <functional>

namespace nkentseu {
namespace memory {

    /**
     * @brief Callback pour les allocations
     */
    using AllocCallback = std::function<void(void* ptr, nk_size size, const nk_char* tag)>;
    using FreeCallback = std::function<void(void* ptr, nk_size size)>;
    using ReallocCallback = std::function<void(void* oldPtr, void* newPtr, nk_size oldSize, nk_size newSize)>;
    
    /**
     * @brief Memory profiler hooks
     * 
     * Permet d'installer des callbacks personnalisés pour
     * tracker allocations en temps réel (production-safe).
     */
    class NKMEMORY_API NkMemoryProfiler {
    public:
        /**
         * @brief Installe un callback d'allocation
         */
        static void SetAllocCallback(AllocCallback cb) noexcept;
        
        /**
         * @brief Installe un callback de free
         */
        static void SetFreeCallback(FreeCallback cb) noexcept;
        
        /**
         * @brief Installe un callback de realloc
         */
        static void SetReallocCallback(ReallocCallback cb) noexcept;
        
        /**
         * @brief Obtient les stats globales
         */
        struct GlobalStats {
            nk_uint64 totalAllocations;
            nk_uint64 totalFrees;
            nk_uint64 liveAllocations;
            nk_uint64 liveBytes;
            nk_uint64 peakBytes;
            float32 avgAllocationSize;
        };
        
        static GlobalStats GetGlobalStats() noexcept;
        
        /**
         * @brief Dump pour debug
         */
        static void DumpProfilerStats() noexcept;
        
    private:
        static AllocCallback sAllocCB;
        static FreeCallback sFreeCB;
        static ReallocCallback sReallocCB;
    };

} // namespace memory
} // namespace nkentseu

#endif // NKENTSEU_MEMORY_NKMEMORY_PROFILER_H_INCLUDED
