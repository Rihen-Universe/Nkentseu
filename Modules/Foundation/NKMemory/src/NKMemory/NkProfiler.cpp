// =============================================================================
// NKMemory/NkProfiler.cpp
// Implémentation du système de profiling mémoire runtime.
//
// Design :
//  - Inclusion systématique de "pch.h" en premier pour la précompilation
//  - Réutilisation des primitives NKCore (ZÉRO duplication de logique)
//  - Thread-safety via NkSpinLock avec guard RAII local (pas de dépendance externe)
//  - Callbacks optionnels : overhead minimal si nullptr
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

// -------------------------------------------------------------------------
// PRÉ-COMPILED HEADER - TOUJOURS EN PREMIER
// -------------------------------------------------------------------------
#include "pch.h"

// -------------------------------------------------------------------------
// EN-TÊTES DU MODULE
// -------------------------------------------------------------------------
#include "NKMemory/NkProfiler.h"

// -------------------------------------------------------------------------
// DÉPENDANCES OPTIONNELLES (debug/verbose)
// -------------------------------------------------------------------------
#include "NKPlatform/NkFoundationLog.h"    // NK_FOUNDATION_LOG_*

// -------------------------------------------------------------------------
// CONFIGURATION CONDITIONNELLE
// -------------------------------------------------------------------------
// En release avec NKENTSEU_DISABLE_MEMORY_PROFILING, toutes les fonctions
// deviennent des no-ops compilés en inline pour zero overhead.

#if !defined(NKENTSEU_DISABLE_MEMORY_PROFILING)

    // -------------------------------------------------------------------------
    // NAMESPACE ANONYME : Helpers internes (non-exportés)
    // -------------------------------------------------------------------------
    namespace {

        // Variable locale pour le calcul de la moyenne (évite overflow sur cumulatif)
        nkentseu::nk_uint64 gTotalAllocatedBytes = 0u;

    } // namespace anonyme


    // -------------------------------------------------------------------------
    // NAMESPACE PUBLIC : Initialisation des membres statiques
    // -------------------------------------------------------------------------
    namespace nkentseu {
        namespace memory {

            // Initialisation des membres statiques de NkMemoryProfiler
            AllocCallback NkMemoryProfiler::sAllocCB = nullptr;
            FreeCallback NkMemoryProfiler::sFreeCB = nullptr;
            ReallocCallback NkMemoryProfiler::sReallocCB = nullptr;
            NkMemoryProfiler::GlobalStats NkMemoryProfiler::sStats{};  // Zero-initialization
            NkSpinLock NkMemoryProfiler::sLock;

            // ====================================================================
            // Installation des Callbacks (thread-safe)
            // ====================================================================

            void NkMemoryProfiler::SetAllocCallback(AllocCallback cb) noexcept {
                NkSpinLockGuard guard(sLock);
                sAllocCB = cb;
            }

            void NkMemoryProfiler::SetFreeCallback(FreeCallback cb) noexcept {
                NkSpinLockGuard guard(sLock);
                sFreeCB = cb;
            }

            void NkMemoryProfiler::SetReallocCallback(ReallocCallback cb) noexcept {
                NkSpinLockGuard guard(sLock);
                sReallocCB = cb;
            }

            // ====================================================================
            // Notifications Explicites (thread-safe + callbacks)
            // ====================================================================

            void NkMemoryProfiler::NotifyAlloc(void* ptr, nk_size size, const nk_char* tag) noexcept {
                // Guards : ignorer les paramètres invalides
                if (!ptr || size == 0) {
                    return;
                }

                // Copie locale du callback pour appel hors section critique
                AllocCallback callback = nullptr;
                
                {
                    NkSpinLockGuard guard(sLock);
                    
                    // Capture du callback avant release du lock
                    callback = sAllocCB;
                    
                    // Mise à jour des stats globales
                    ++sStats.totalAllocations;
                    ++sStats.liveAllocations;
                    sStats.liveBytes += size;
                    ::gTotalAllocatedBytes += size;
                    
                    // Mise à jour du peak si nécessaire
                    if (sStats.liveBytes > sStats.peakBytes) {
                        sStats.peakBytes = sStats.liveBytes;
                    }
                    
                    // Recalcul de la moyenne (évite division par zéro)
                    if (sStats.totalAllocations > 0) {
                        sStats.avgAllocationSize = 
                            static_cast<float32>(::gTotalAllocatedBytes) / 
                            static_cast<float32>(sStats.totalAllocations);
                    }
                }
                // Lock relâché ici

                // Appel du callback HORS section critique pour minimiser la contention
                // Note : le callback ne doit PAS allouer de mémoire (risque de récursion)
                if (callback) {
                    callback(ptr, size, tag);
                }
            }

            void NkMemoryProfiler::NotifyFree(void* ptr, nk_size size) noexcept {
                // Safe : nullptr est accepté (no-op)
                if (!ptr) {
                    return;
                }

                FreeCallback callback = nullptr;
                
                {
                    NkSpinLockGuard guard(sLock);
                    callback = sFreeCB;
                    
                    // Mise à jour des stats seulement si size valide
                    if (size > 0) {
                        ++sStats.totalFrees;
                        
                        if (sStats.liveAllocations > 0) {
                            --sStats.liveAllocations;
                        }
                        
                        // Sous-traction sécurisée (évite underflow)
                        if (sStats.liveBytes >= size) {
                            sStats.liveBytes -= size;
                        } else {
                            sStats.liveBytes = 0;
                        }
                    }
                }

                if (callback) {
                    callback(ptr, size);
                }
            }

            void NkMemoryProfiler::NotifyRealloc(void* oldPtr, void* newPtr, 
                                                nk_size oldSize, nk_size newSize) noexcept {
                // Si newPtr est nullptr : réallocation échouée, ne pas notifier
                if (!newPtr) {
                    return;
                }

                ReallocCallback callback = nullptr;
                
                {
                    NkSpinLockGuard guard(sLock);
                    callback = sReallocCB;
                    
                    // Cas 1 : réallocation avec changement d'adresse (oldPtr != newPtr)
                    if (oldPtr != newPtr) {
                        // Déduire l'ancienne allocation
                        if (oldPtr && oldSize > 0 && sStats.liveBytes >= oldSize) {
                            sStats.liveBytes -= oldSize;
                        }
                        // Ajouter la nouvelle allocation
                        if (newSize > 0) {
                            sStats.liveBytes += newSize;
                            if (sStats.liveBytes > sStats.peakBytes) {
                                sStats.peakBytes = sStats.liveBytes;
                            }
                        }
                    }
                    // Cas 2 : resize in-place (oldPtr == newPtr)
                    else if (newSize != oldSize) {
                        if (newSize > oldSize) {
                            // Grow in-place
                            sStats.liveBytes += (newSize - oldSize);
                            if (sStats.liveBytes > sStats.peakBytes) {
                                sStats.peakBytes = sStats.liveBytes;
                            }
                        } else {
                            // Shrink in-place
                            if (sStats.liveBytes >= (oldSize - newSize)) {
                                sStats.liveBytes -= (oldSize - newSize);
                            } else {
                                sStats.liveBytes = 0;
                            }
                        }
                    }
                    // Cas 3 : newSize == oldSize : no-op pour les stats
                }

                if (callback) {
                    callback(oldPtr, newPtr, oldSize, newSize);
                }
            }

            // ====================================================================
            // Interrogation des Statistiques (thread-safe)
            // ====================================================================

            NkMemoryProfiler::GlobalStats NkMemoryProfiler::GetGlobalStats() noexcept {
                NkSpinLockGuard guard(sLock);
                return sStats;  // Copie de la structure (8+8+8+8+8+4 = ~44 bytes)
            }

            // ====================================================================
            // Reporting et Debugging
            // ====================================================================

            void NkMemoryProfiler::DumpProfilerStats() noexcept {
                // Acquisition du lock pour lecture cohérente
                const GlobalStats stats = GetGlobalStats();
                
                NK_FOUNDATION_LOG_INFO("=== NKMemory Profiler Stats ===");
                NK_FOUNDATION_LOG_INFO("Allocations: total=%llu, frees=%llu, live=%llu",
                                      static_cast<unsigned long long>(stats.totalAllocations),
                                      static_cast<unsigned long long>(stats.totalFrees),
                                      static_cast<unsigned long long>(stats.liveAllocations));
                
                // Formatage des tailles en unités humaines
                auto FormatBytes = [](nk_uint64 bytes) -> const char* {
                    static char buffer[32];
                    if (bytes >= (1024ULL * 1024 * 1024)) {
                        snprintf(buffer, sizeof(buffer), "%.2f GB", 
                                static_cast<float>(bytes) / (1024.0f * 1024.0f * 1024.0f));
                    } else if (bytes >= (1024ULL * 1024)) {
                        snprintf(buffer, sizeof(buffer), "%.2f MB", 
                                static_cast<float>(bytes) / (1024.0f * 1024.0f));
                    } else if (bytes >= 1024) {
                        snprintf(buffer, sizeof(buffer), "%.2f KB", 
                                static_cast<float>(bytes) / 1024.0f);
                    } else {
                        snprintf(buffer, sizeof(buffer), "%llu B", 
                                static_cast<unsigned long long>(bytes));
                    }
                    return buffer;
                };
                
                NK_FOUNDATION_LOG_INFO("Memory: live=%s, peak=%s, avg=%.2f bytes",
                                      FormatBytes(stats.liveBytes),
                                      FormatBytes(stats.peakBytes),
                                      static_cast<double>(stats.avgAllocationSize));
                NK_FOUNDATION_LOG_INFO("=================================");
            }

        } // namespace memory
    } // namespace nkentseu

#else // NKENTSEU_DISABLE_MEMORY_PROFILING défini

    // -------------------------------------------------------------------------
    // VERSION NO-OP POUR RELEASE (zero overhead)
    // -------------------------------------------------------------------------
    namespace nkentseu {
        namespace memory {

            // Initialisation minimale des membres statiques
            AllocCallback NkMemoryProfiler::sAllocCB = nullptr;
            FreeCallback NkMemoryProfiler::sFreeCB = nullptr;
            ReallocCallback NkMemoryProfiler::sReallocCB = nullptr;
            NkMemoryProfiler::GlobalStats NkMemoryProfiler::sStats{};
            NkSpinLock NkMemoryProfiler::sLock;

            // Toutes les méthodes publiques deviennent des no-ops
            void NkMemoryProfiler::SetAllocCallback(AllocCallback) noexcept {}
            void NkMemoryProfiler::SetFreeCallback(FreeCallback) noexcept {}
            void NkMemoryProfiler::SetReallocCallback(ReallocCallback) noexcept {}
            
            void NkMemoryProfiler::NotifyAlloc(void*, nk_size, const nk_char*) noexcept {}
            void NkMemoryProfiler::NotifyFree(void*, nk_size) noexcept {}
            void NkMemoryProfiler::NotifyRealloc(void*, void*, nk_size, nk_size) noexcept {}
            
            NkMemoryProfiler::GlobalStats NkMemoryProfiler::GetGlobalStats() noexcept {
                return GlobalStats{};  // Retourne des zéros
            }
            
            void NkMemoryProfiler::DumpProfilerStats() noexcept {}

        } // namespace memory
    } // namespace nkentseu

#endif // NKENTSEU_DISABLE_MEMORY_PROFILING

// =============================================================================
// NOTES DE MAINTENANCE
// =============================================================================
/*
    [THREAD-SAFETY]
    ✓ Toutes les méthodes publiques acquièrent sLock via NkSpinLockGuard local
    ✓ Les callbacks sont capturés AVANT release du lock pour éviter les races
    ✓ Les callbacks sont appelés HORS section critique pour minimiser la contention
    ✓ Les stats sont copiées atomiquement dans GetGlobalStats() (structure petite)

    [PERFORMANCE]
    - NotifyAlloc/Free/Realloc : ~10-20 cycles CPU + overhead du lock (~20-50 cycles selon contention)
    - GetGlobalStats : copie de ~44 bytes + overhead du lock
    - Callbacks : overhead variable selon l'implémentation (responsabilité de l'utilisateur)
    - En release avec NKENTSEU_DISABLE_MEMORY_PROFILING : overhead = 0 (fonctions inline no-op)

    [MÉMOIRE]
    - Membres statiques : ~44 bytes (GlobalStats) + sizeof(NkSpinLock) (~4-8 bytes)
    - Pas d'allocation dynamique : tout est statique ou sur la pile
    - Overhead négligeable même en debug

    [EXTENSIONS POSSIBLES]
    1. Sampling intégré : ajouter un paramètre sampleRate à Notify*() pour filtrer automatiquement
    2. Timestamps : ajouter nk_uint64 timestamp aux stats pour calcul de débits (bytes/sec)
    3. Tags agrégés : maintenir des stats par tag dans une petite table hash (ex: 16 tags max)
    4. Export asynchrone : buffer ring pour écrire les events et les exporter dans un thread dédié

    [WARNINGS IMPORTANTS]
    ⚠️ Les callbacks NE DOIVENT PAS allouer de mémoire :
       - Risque de récursion infinie si l'allocateur appelle NotifyAlloc
       - Solution : utiliser des buffers pré-alloués, des compteurs atomiques, ou du logging asynchrone
    
    ⚠️ Les callbacks doivent être noexcept :
       - Une exception dans un callback peut corrompre l'état du profiler
       - En debug : ajouter NK_ASSERT pour catcher les exceptions
    
    ⚠️ Thread-safety des tags :
       - Le paramètre `tag` est un pointeur non-possédé : doit pointer vers une string valide
       - Préférer les string literals ou les constantes globales, pas les temporaires locaux
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================