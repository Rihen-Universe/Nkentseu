// ============================================================
// FILE: NkMultiLevelAllocator.h
// DESCRIPTION: Multi-tier allocator (UE5-style)
// AUTHOR: Rihen
// DATE: 2026-03-05
// VERSION: 1.0.0
// ============================================================
// Allocateur multi-niveaux: dispatcher vers différents
// allocateurs selon la taille pour optimisation maximale.
//
// Tailles:
// - Tiny (1-64 bytes):   Pool fixe dense
// - Small (65-1KB):      Pool fixe
// - Medium (1KB-1MB):    Pool variable
// - Large (>1MB):        Direct OS malloc
// ============================================================

#pragma once

#ifndef NKENTSEU_MEMORY_NKMULTILLEVELALLOCATOR_H_INCLUDED
#define NKENTSEU_MEMORY_NKMULTILLEVELALLOCATOR_H_INCLUDED

#include "NKMemory/NkAllocator.h"
#include "NKMemory/NkPoolAllocator.h"
#include "NKMemory/NkUniquePtr.h"
#include "NKMemory/NkExport.h"
#include "NKCore/NkTypes.h"
#include "NKCore/NkAtomic.h"

namespace nkentseu {
namespace memory {

    /**
     * @brief Ultra-optimized multi-level allocator (UE5 inspired)
     * 
     * Stratégie:
     * 1. TINY (0-64 bytes):    NkFixedPoolAllocator (extremely optimized)
     * 2. SMALL (64B-1KB):      NkFixedPoolAllocator
     * 3. MEDIUM (1KB-1MB):     NkVariablePoolAllocator
     * 4. LARGE (>1MB):         OS malloc (direct)
     * 
     * Résultats attendus:
     * - 95% des allocations vont en TINY/SMALL (pools)
     * - Zéro fragmentation pour petites allocations
     * - Déterministe (pas de syscall pour <1KB)
     * - Cache-optimizé (objets compacts)
     * 
     * @performance
     * - Tiny allocation:   10-50 ns (pool O(1))
     * - Small allocation: 100-500 ns (pool O(1))
     * - Medium allocation: 1-10 µs (variable pool)
     * - Large allocation: 10-100 µs (OS malloc)
     * 
     * @example
     * ```cpp
     * NkMultiLevelAllocator alloc;
     * 
     * void* p1 = alloc.Allocate(32);   // → TINY pool  (très rapide)
     * void* p2 = alloc.Allocate(512);  // → SMALL pool (très rapide)
     * void* p3 = alloc.Allocate(10*1024*1024);  // → OS malloc
     * 
     * alloc.Deallocate(p1);
     * alloc.Deallocate(p2);
     * alloc.Deallocate(p3);
     * ```
     */
    class NKENTSEU_MEMORY_API NkMultiLevelAllocator : public NkAllocator {
    public:
        // Pool sizes (configurable)
        enum {
            TINY_SIZE = 64,
            TINY_COUNT = 4096,       // 256 KB total
            
            SMALL_SIZE = 1024,
            SMALL_COUNT = 1024,      // 1 MB total
            
            MEDIUM_THRESHOLD = 1 * 1024 * 1024,  // 1 MB
        };
        
        // ==============================================
        // CONSTRUCTEUR/DESTRUCTEUR
        // ==============================================
        
        NkMultiLevelAllocator() noexcept;
        ~NkMultiLevelAllocator() override;
        
        // Non-copiable
        NkMultiLevelAllocator(const NkMultiLevelAllocator&) = delete;
        NkMultiLevelAllocator& operator=(const NkMultiLevelAllocator&) = delete;
        
        // ==============================================
        // ALLOCATOR API
        // ==============================================
        
        /**
         * @brief Allocate - Dispatch vers le bon pool selon taille
         */
        Pointer Allocate(SizeType size, SizeType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
        
        /**
         * @brief Deallocate - Retourne au bon pool
         */
        void Deallocate(Pointer ptr) override;
        
        /**
         * @brief Reallocate - Realloc optimisé
         */
        Pointer Reallocate(Pointer ptr, SizeType oldSize, SizeType newSize,
                          SizeType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
        
        /**
         * @brief Reset tous les pools
         */
        void Reset() noexcept override;
        
        // ==============================================
        // STATISTICS
        // ==============================================
        
        struct NKENTSEU_MEMORY_API Stats {
            struct PoolStats {
                nk_size allocated;
                nk_size capacity;
                float32 usage;   // 0.0 .. 1.0
            };
            
            PoolStats tiny;      // TINY pool
            PoolStats small;     // SMALL pool
            PoolStats medium;    // MEDIUM pool (variable)
            
            nk_size largeAllocations;  // Nombre allocations OS
            nk_size largeBytes;        // Total bytes OS malloc
            
            float32 GetFragmentation() const noexcept;
        };
        
        /**
         * @brief Obtient les statistiques d'utilisation
         */
        [[nodiscard]] Stats GetStats() const noexcept;
        
        /**
         * @brief Dump des stats pour debug
         */
        void DumpStats() const noexcept;
        
    private:
        // Internal pools
        NkUniquePtr<NkFixedPoolAllocator<TINY_SIZE, TINY_COUNT>> mTinyPool;
        NkUniquePtr<NkFixedPoolAllocator<SMALL_SIZE, SMALL_COUNT>> mSmallPool;
        NkUniquePtr<NkVariablePoolAllocator> mMediumPool;
        
        // Large allocations tracker (minimal state in this module).
        struct {
            nk_size totalBytes = 0u;
            nk_size allocationCount = 0u;
        } mLarge;
        
        /**
         * @brief Helper: Trouve le pool pour un pointeur
         */
        NkAllocator* FindAllocatorFor(void* ptr) const noexcept;
        
        NkSpinLock mLock;
    };
    
    /**
     * @brief Obtient l'allocateur multi-niveaux global
     */
    NkMultiLevelAllocator& NkGetMultiLevelAllocator() noexcept;

} // namespace memory
} // namespace nkentseu

#endif // NKENTSEU_MEMORY_NKMULTILLEVELALLOCATOR_H_INCLUDED
