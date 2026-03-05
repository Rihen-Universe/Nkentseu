// ============================================================
// FILE: NkMemoryTag.h
// DESCRIPTION: Memory budgeting and tagging system
// AUTHOR: Rihen
// DATE: 2026-03-05
// VERSION: 1.0.0
// ============================================================
// Système de tagging mémoire permettant:
// - Catégoriser allocations par type (Render, Physics, AI, etc.)
// - Tracker budgets par catégorie
// - Alert si dépassement de budget
// - Debug allocation patterns
// ============================================================

#pragma once

#ifndef NKENTSEU_MEMORY_NKMEMORY_TAG_H_INCLUDED
#define NKENTSEU_MEMORY_NKMEMORY_TAG_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKMemory/NkMemoryExport.h"

namespace nkentseu {
namespace memory {

    /**
     * @brief Memory budget category
     * 
     * Permet de catégoriser et tracker les allocations
     * dans différentes zones (render, physics, AI, streaming, etc.)
     */
    enum class NkMemoryTag : nk_uint8 {
        // Core engine
        NK_MEMORY_ENGINE      = 0,    // Engine internals
        NK_MEMORY_CONTAINER   = 1,    // Vectors, Maps, etc.
        NK_MEMORY_ALLOCATOR   = 2,    // Allocator overhead
        
        // Game systems
        NK_MEMORY_GAME        = 10,   // Game objects
        NK_MEMORY_ENTITY      = 11,   // Entities/Components
        NK_MEMORY_SCRIPT      = 12,   // Script VM
        
        // Rendering
        NK_MEMORY_RENDER      = 20,   // Render system
        NK_MEMORY_TEXTURE     = 21,   // Textures (GPU mem)
        NK_MEMORY_MESH        = 22,   // Mesh data
        NK_MEMORY_SHADER      = 23,   // Shaders
        
        // Physics
        NK_MEMORY_PHYSICS     = 30,   // Physics engine
        NK_MEMORY_COLLISION   = 31,   // Collision shapes
        NK_MEMORY_RIGID       = 32,   // Rigid bodies
        
        // Audio
        NK_MEMORY_AUDIO       = 40,   // Audio system
        NK_MEMORY_SOUND       = 41,   // Sound data
        
        // Streaming
        NK_MEMORY_STREAMING   = 50,   // Streaming cache
        NK_MEMORY_NETWORK     = 51,   // Network buffers
        
        // Debug/Profiling
        NK_MEMORY_DEBUG       = 255,  // Debug allocations
    };
    
    /**
     * @brief Memory tag statistics
     */
    struct NKMEMORY_API NkMemoryTagStats {
        nk_uint64 totalAllocated;      // Total bytes allocated
        nk_uint64 peakAllocated;       // Peak bytes
        nk_uint64 allocationCount;     // Number of allocations
        float32 averageSize;           // Average allocation size
        float32 fragmentation;         // Fragmentation ratio
    };
    
    /**
     * @brief Memory budget tracker
     * 
     * Permet de définir et tracker les budgets mémoire
     * par catégorie.
     */
    class NKMEMORY_API NkMemoryBudget {
    public:
        /**
         * @brief Définit le budget pour une catégorie
         * @param tag Catégorie mémoire
         * @param bytes Limite de budget
         */
        static void SetBudget(NkMemoryTag tag, nk_uint64 bytes) noexcept;
        
        /**
         * @brief Obtient le budget restant
         * @return Bytes restants avant dépassement
         */
        static nk_int64 GetBudgetRemaining(NkMemoryTag tag) noexcept;
        
        /**
         * @brief Obtient les stats pour une catégorie
         */
        static NkMemoryTagStats GetStats(NkMemoryTag tag) noexcept;
        
        /**
         * @brief Dump toutes les stats pour debug
         */
        static void DumpStats() noexcept;
        
        /**
         * @brief Check si un tag est en dépassement
         */
        static nk_bool IsOverBudget(NkMemoryTag tag) noexcept;
    };

} // namespace memory
} // namespace nkentseu

#endif // NKENTSEU_MEMORY_NKMEMORY_TAG_H_INCLUDED
