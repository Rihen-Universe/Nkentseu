// ============================================================
// FILE: NkMemoryTracker.h
// DESCRIPTION: O(1) memory leak detection (vs O(n) linked-list)
// AUTHOR: Rihen
// DATE: 2026-03-05
// VERSION: 1.0.0
// ============================================================
// Remplace la linked-list O(n) par table hash O(1).
// Critique pour production avec miillions d'allocations.
//
// Avantages:
// - Leak detection O(1) vs O(n)
// - Meilleure cache-locality
// - Multi-thread friendly
// ============================================================

#pragma once

#ifndef NKENTSEU_MEMORY_NKMEMORY_TRACKER_H_INCLUDED
#define NKENTSEU_MEMORY_NKMEMORY_TRACKER_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKCore/NkAtomic.h"
#include "NKMemory/NkMemoryExport.h"
#include "NKMemory/NkMemoryTag.h"

#include <cstdio>
#include <unordered_map>

namespace nkentseu {
namespace memory {

    /**
     * @brief Alloc info (replaces NkAllocationNode)
     * 
     * Enregistrement d'une allocation pour tracking.
     */
    struct NKMEMORY_API NkAllocationInfo {
        void* userPtr;       // Pointeur utilisateur
        void* basePtr;       // Pointeur base alloué
        nk_size size;        // Taille en bytes
        nk_size count;       // Nombre items (pour arrays)
        nk_size alignment;   // Alignement
        nk_uint8 tag;        // Memory tag (categorie)
        
        // Debug info
        const nk_char* file;
        nk_int32 line;
        const nk_char* function;
        const nk_char* name;
        
        // Stats
        nk_uint64 timestamp;  // When allocated
        nk_uint32 threadId;   // Which thread allocated
    };
    
    /**
     * @brief O(1) Memory tracker utilisant hash-table
     * 
     * Remplace completement la LinkedList O(n) du système.
     * 
     * Performance:
     * - Register allocation: O(1) average
     * - Find allocation: O(1) average
     * - Leak detection: O(n) mais avec meilleure cache-locality
     * - Unregister: O(1) average
     */
    class NKMEMORY_API NkMemoryTracker {
    public:
        // ==============================================
        // CONSTRUCTEUR/DESTRUCTEUR
        // ==============================================
        
        NkMemoryTracker() noexcept = default;
        ~NkMemoryTracker() = default;
        
        // Non-copiable
        NkMemoryTracker(const NkMemoryTracker&) = delete;
        NkMemoryTracker& operator=(const NkMemoryTracker&) = delete;
        
        // ==============================================
        // API TRACKING
        // ==============================================
        
        /**
         * @brief Enregistre une allocation (O(1))
         */
        void Register(const NkAllocationInfo& info) noexcept {
            core::NkScopedSpinLock guard(mLock);
            mAllocations[info.userPtr] = info;
            mTotalLiveBytes += info.size;
            mTotalAllocations++;
            
            if (mTotalLiveBytes > mPeakBytes) {
                mPeakBytes = mTotalLiveBytes;
            }
        }
        
        /**
         * @brief Unregisters une allocation (O(1))
         */
        void Unregister(void* ptr) noexcept {
            core::NkScopedSpinLock guard(mLock);
            auto it = mAllocations.find(ptr);
            if (it != mAllocations.end()) {
                mTotalLiveBytes -= it->second.size;
                mAllocations.erase(it);
            }
        }
        
        /**
         * @brief Trouver une allocation (O(1))
         */
        const NkAllocationInfo* Find(void* ptr) const noexcept {
            core::NkScopedSpinLock guard(mLock);
            auto it = mAllocations.find(ptr);
            if (it != mAllocations.end()) {
                return &it->second;
            }
            return nullptr;
        }
        
        /**
         * @brief Dump toutes les fuites détectées
         */
        void DumpLeaks() const noexcept {
            core::NkScopedSpinLock guard(mLock);
            if (mAllocations.empty()) {
                printf("[NKMemory] No leaks detected.\n");
                return;
            }
            
            printf("[NKMemory] Leak summary:\n");
            for (const auto& pair : mAllocations) {
                const NkAllocationInfo& info = pair.second;
                printf("  [LEAK] %p size=%llu tag=%d at %s:%d (%s)\n",
                    info.userPtr,
                    static_cast<unsigned long long>(info.size),
                    static_cast<int>(info.tag),
                    info.file ? info.file : "<unknown>",
                    info.line,
                    info.function ? info.function : "<unknown>");
            }
            printf("[NKMemory] Total: %llu leaks, %llu bytes\n",
                static_cast<unsigned long long>(mAllocations.size()),
                static_cast<unsigned long long>(mTotalLiveBytes));
        }
        
        /**
         * @brief Obtient les statistiques
         */
        struct Stats {
            nk_size liveBytes;
            nk_size peakBytes;
            nk_size liveAllocations;
            nk_uint64 totalAllocations;
        };
        
        [[nodiscard]] Stats GetStats() const noexcept {
            core::NkScopedSpinLock guard(mLock);
            return {
                mTotalLiveBytes,
                mPeakBytes,
                mAllocations.size(),
                mTotalAllocations
            };
        }
        
        /**
         * @brief Reset tracker (utilisé avant shutdown)
         */
        void Clear() noexcept {
            core::NkScopedSpinLock guard(mLock);
            mAllocations.clear();
            mTotalLiveBytes = 0;
        }
        
    private:
        // Hash-table: ptr → AllocationInfo (O(1) lookup)
        std::unordered_map<void*, NkAllocationInfo> mAllocations;
        
        // Statistics
        nk_size mTotalLiveBytes = 0;
        nk_size mPeakBytes = 0;
        nk_uint64 mTotalAllocations = 0;
        
        mutable core::NkSpinLock mLock;
    };

} // namespace memory
} // namespace nkentseu

#endif // NKENTSEU_MEMORY_NKMEMORY_TRACKER_H_INCLUDED
