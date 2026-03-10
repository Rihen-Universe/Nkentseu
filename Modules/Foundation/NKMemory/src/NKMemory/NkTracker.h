// ============================================================
// FILE: NkTracker.h
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

#ifndef NKENTSEU_MEMORY_NKTRACKER_H_INCLUDED
#define NKENTSEU_MEMORY_NKTRACKER_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKCore/NkAtomic.h"
#include "NKMemory/NkExport.h"
#include "NKMemory/NkTag.h"
#include "NKPlatform/NkFoundationLog.h"

#include <cstdio>
#include <cstdlib>

namespace nkentseu {
namespace memory {

    /**
     * @brief Alloc info (replaces NkAllocationNode)
     * 
     * Enregistrement d'une allocation pour tracking.
     */
    struct NKENTSEU_MEMORY_API NkAllocationInfo {
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
    class NKENTSEU_MEMORY_API NkMemoryTracker {
    public:
        // ==============================================
        // CONSTRUCTEUR/DESTRUCTEUR
        // ==============================================
        
        NkMemoryTracker() noexcept = default;
        ~NkMemoryTracker() {
            NkScopedSpinLock guard(mLock);
            ClearInternal();
        }
        
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
            NkScopedSpinLock guard(mLock);
            Entry* entry = FindEntry(info.userPtr);
            if (entry) {
                if (mTotalLiveBytes >= entry->Info.size) {
                    mTotalLiveBytes -= entry->Info.size;
                } else {
                    mTotalLiveBytes = 0;
                }
                entry->Info = info;
            } else {
                const nk_size bucket = HashPtr(info.userPtr);
                Entry* newEntry = static_cast<Entry*>(::malloc(sizeof(Entry)));
                if (!newEntry) {
                    return;
                }
                newEntry->Info = info;
                newEntry->Next = mBuckets[bucket];
                mBuckets[bucket] = newEntry;
                ++mLiveAllocations;
            }

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
            NkScopedSpinLock guard(mLock);
            const nk_size bucket = HashPtr(ptr);
            Entry* prev = nullptr;
            Entry* node = mBuckets[bucket];
            while (node) {
                if (node->Info.userPtr == ptr) {
                    if (mTotalLiveBytes >= node->Info.size) {
                        mTotalLiveBytes -= node->Info.size;
                    } else {
                        mTotalLiveBytes = 0;
                    }

                    if (prev) {
                        prev->Next = node->Next;
                    } else {
                        mBuckets[bucket] = node->Next;
                    }
                    ::free(node);
                    if (mLiveAllocations > 0) {
                        --mLiveAllocations;
                    }
                    return;
                }
                prev = node;
                node = node->Next;
            }
        }
        
        /**
         * @brief Trouver une allocation (O(1))
         */
        const NkAllocationInfo* Find(void* ptr) const noexcept {
            NkScopedSpinLock guard(mLock);
            const Entry* entry = FindEntryConst(ptr);
            return entry ? &entry->Info : nullptr;
        }
        
        /**
         * @brief Dump toutes les fuites détectées
         */
        void DumpLeaks() const noexcept {
            NkScopedSpinLock guard(mLock);
            if (mLiveAllocations == 0) {
                NK_FOUNDATION_LOG_INFO("[NKMemory] No leaks detected.");
                return;
            }
            
            NK_FOUNDATION_LOG_INFO("[NKMemory] Leak summary:");
            for (nk_size i = 0; i < NK_TRACKER_BUCKET_COUNT; ++i) {
                const Entry* node = mBuckets[i];
                while (node) {
                    const NkAllocationInfo& info = node->Info;
                    NK_FOUNDATION_LOG_INFO("  [LEAK] %p size=%llu tag=%d at %s:%d (%s)",
                                           info.userPtr,
                                           static_cast<unsigned long long>(info.size),
                                           static_cast<int>(info.tag),
                                           info.file ? info.file : "<unknown>",
                                           info.line,
                                           info.function ? info.function : "<unknown>");
                    node = node->Next;
                }
            }
            NK_FOUNDATION_LOG_INFO("[NKMemory] Total: %llu leaks, %llu bytes",
                                   static_cast<unsigned long long>(mLiveAllocations),
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
            NkScopedSpinLock guard(mLock);
            return {
                mTotalLiveBytes,
                mPeakBytes,
                mLiveAllocations,
                mTotalAllocations
            };
        }
        
        /**
         * @brief Reset tracker (utilisé avant shutdown)
         */
        void Clear() noexcept {
            NkScopedSpinLock guard(mLock);
            ClearInternal();
        }
        
    private:
        static constexpr nk_size NK_TRACKER_BUCKET_COUNT = 4096u;

        struct Entry {
            NkAllocationInfo Info;
            Entry* Next;
        };

        static nk_size HashPtr(const void* ptr) noexcept {
            const nk_size v = static_cast<nk_size>(reinterpret_cast<nk_uintptr>(ptr));
            return ((v >> 4u) ^ (v >> 9u) ^ (v >> 16u)) % NK_TRACKER_BUCKET_COUNT;
        }

        Entry* FindEntry(const void* ptr) noexcept {
            Entry* node = mBuckets[HashPtr(ptr)];
            while (node) {
                if (node->Info.userPtr == ptr) {
                    return node;
                }
                node = node->Next;
            }
            return nullptr;
        }

        const Entry* FindEntryConst(const void* ptr) const noexcept {
            const Entry* node = mBuckets[HashPtr(ptr)];
            while (node) {
                if (node->Info.userPtr == ptr) {
                    return node;
                }
                node = node->Next;
            }
            return nullptr;
        }

        void ClearInternal() noexcept {
            for (nk_size i = 0; i < NK_TRACKER_BUCKET_COUNT; ++i) {
                Entry* node = mBuckets[i];
                while (node) {
                    Entry* next = node->Next;
                    ::free(node);
                    node = next;
                }
                mBuckets[i] = nullptr;
            }
            mLiveAllocations = 0;
            mTotalLiveBytes = 0;
        }

        Entry* mBuckets[NK_TRACKER_BUCKET_COUNT] = {};
        nk_size mLiveAllocations = 0;
        
        // Statistics
        nk_size mTotalLiveBytes = 0;
        nk_size mPeakBytes = 0;
        nk_uint64 mTotalAllocations = 0;
        
        mutable NkSpinLock mLock;
    };

} // namespace memory
} // namespace nkentseu

#endif // NKENTSEU_MEMORY_NKTRACKER_H_INCLUDED
