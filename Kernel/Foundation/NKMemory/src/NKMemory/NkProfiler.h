// =============================================================================
// NKMemory/NkProfiler.h
// Hooks de profiling mémoire runtime pour monitoring et analyse en temps réel.
//
// Design :
//  - Séparation claire déclarations (.h) / implémentations (.cpp)
//  - Réutilisation des primitives NKCore (ZÉRO duplication)
//  - Callbacks thread-safe via NkSpinLock de NKCore
//  - API minimale : installation de hooks + notification + stats globales
//  - Overhead configurable : callbacks optionnels, désactivables en release
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_MEMORY_NKPROFILER_H
#define NKENTSEU_MEMORY_NKPROFILER_H

    // -------------------------------------------------------------------------
    // SECTION 1 : DÉPENDANCES
    // -------------------------------------------------------------------------
    #include "NKMemory/NkMemoryApi.h"      // NKENTSEU_MEMORY_API
    #include "NKCore/NkTypes.h"            // nk_size, nk_uint64, float32, etc.
    #include "NKCore/NkAtomic.h"           // NkSpinLock pour synchronisation

    // -------------------------------------------------------------------------
    // SECTION 2 : CONFIGURATION DU MODULE PROFILING
    // -------------------------------------------------------------------------
    /**
     * @defgroup MemoryProfiling Profiling Mémoire Runtime
     * @brief Système de hooks pour monitoring mémoire en temps réel
     * 
     * Fonctionnalités :
     *  - Installation de callbacks personnalisés pour alloc/free/realloc
     *  - Stats globales thread-safe : total, live, peak, moyenne
     *  - Notifications explicites via Notify*() pour intégration manuelle
     *  - Dump formaté pour debugging et logging
     * 
     * @note En mode release avec NKENTSEU_DISABLE_MEMORY_PROFILING, 
     *       les notifications deviennent des no-ops pour performance.
     * @note Les callbacks sont optionnels : si nullptr, seule la stat interne est mise à jour
     * 
     * @example
     * @code
     * // Callback personnalisé pour logging d'allocations
     * void OnAlloc(void* ptr, nk_size size, const nk_char* tag) {
     *     NK_FOUNDATION_LOG_DEBUG("[Alloc] %p %zu bytes [%s]", ptr, size, tag);
     * }
     * 
     * // Installation au démarrage
     * nkentseu::memory::NkMemoryProfiler::SetAllocCallback(OnAlloc);
     * 
     * // Notification depuis un allocateur personnalisé
     * void* MyAlloc(nk_size size) {
     *     void* ptr = malloc(size);
     *     if (ptr) {
     *         nkentseu::memory::NkMemoryProfiler::NotifyAlloc(ptr, size, "MySystem");
     *     }
     *     return ptr;
     * }
     * 
     * // Dump des stats périodique
     * void FrameEnd() {
     *     static int frameCount = 0;
     *     if (++frameCount >= 60) {  // Toutes les 60 frames
     *         nkentseu::memory::NkMemoryProfiler::DumpProfilerStats();
     *         frameCount = 0;
     *     }
     * }
     * @endcode
     */

    namespace nkentseu {
        namespace memory {

            // -------------------------------------------------------------------------
            // SECTION 3 : TYPES DE CALLBACKS
            // -------------------------------------------------------------------------
            /**
             * @typedef AllocCallback
             * @brief Signature de callback pour notification d'allocation
             * @param ptr Pointeur alloué (non-null si succès)
             * @param size Taille allouée en bytes
             * @param tag Catégorie mémoire optionnelle (peut être nullptr)
             * @ingroup MemoryProfiling
             * 
             * @note Appelé APRÈS l'allocation réussie, avant retour à l'appelant
             * @note Thread-safe : peut être appelé depuis n'importe quel thread
             * @note Doit être noexcept : aucune exception ne doit remonter
             */
            using AllocCallback = void (*)(void* ptr, nk_size size, const nk_char* tag) noexcept;

            /**
             * @typedef FreeCallback
             * @brief Signature de callback pour notification de libération
             * @param ptr Pointeur libéré (peut être nullptr, no-op attendu)
             * @param size Taille libérée en bytes (0 si inconnue)
             * @ingroup MemoryProfiling
             * 
             * @note Appelé APRÈS la libération effective, ou juste avant selon l'usage
             * @note Thread-safe : peut être appelé depuis n'importe quel thread
             * @note Doit être noexcept : aucune exception ne doit remonter
             */
            using FreeCallback = void (*)(void* ptr, nk_size size) noexcept;

            /**
             * @typedef ReallocCallback
             * @brief Signature de callback pour notification de réallocation
             * @param oldPtr Ancien pointeur (peut être nullptr pour allocation initiale)
             * @param newPtr Nouveau pointeur (peut être nullptr si échec)
             * @param oldSize Ancienne taille en bytes
             * @param newSize Nouvelle taille en bytes
             * @ingroup MemoryProfiling
             * 
             * @note Appelé APRÈS la réallocation réussie
             * @note Si oldPtr == newPtr : resize in-place (pas de copie)
             * @note Thread-safe et noexcept requis
             */
            using ReallocCallback = void (*)(void* oldPtr, void* newPtr, nk_size oldSize, nk_size newSize) noexcept;

            // -------------------------------------------------------------------------
            // SECTION 4 : CLASSE PRINCIPALE NkMemoryProfiler
            // -------------------------------------------------------------------------
            /**
             * @class NkMemoryProfiler
             * @brief Système de hooks pour profiling mémoire runtime thread-safe
             * @ingroup MemoryProfiling
             * 
             * Fonctionnalités :
             *  - Installation dynamique de callbacks pour alloc/free/realloc
             *  - Stats globales agrégées : total, live, peak, moyenne
             *  - Notifications explicites via Notify*() pour intégration manuelle
             *  - Thread-safe : toutes les opérations protégées par NkSpinLock
             *  - Overhead minimal : callbacks optionnels, stats mises à jour atomiquement
             * 
             * @note Instance unique recommandée (méthodes statiques)
             * @note En release avec NKENTSEU_DISABLE_MEMORY_PROFILING : Notify*() = no-op
             * @note Les callbacks ne doivent PAS appeler d'allocations mémoire (risque de récursion)
             * 
             * @example
             * @code
             * // Setup initial (au démarrage de l'application)
             * void InitProfiling() {
             *     // Callback pour tracking simple
             *     nkentseu::memory::NkMemoryProfiler::SetAllocCallback(
             *         [](void* ptr, nk_size size, const nk_char* tag) {
             *             // Logging minimal, pas d'allocation ici !
             *             // Ex: écrire dans un buffer pré-alloué, ou incrémenter un compteur
             *         });
             * }
             * 
             * // Intégration avec un allocateur personnalisé
             * class ProfilingAllocator : public NkAllocator {
             * public:
             *     Pointer Allocate(SizeType size, AlignType align) override {
             *         void* ptr = mBacking->Allocate(size, align);
             *         if (ptr) {
             *             NkMemoryProfiler::NotifyAlloc(ptr, size, "CustomPool");
             *         }
             *         return ptr;
             *     }
             *     void Deallocate(Pointer ptr) override {
             *         if (ptr) {
             *             // Note: taille souvent inconnue ici, passer 0
             *             NkMemoryProfiler::NotifyFree(ptr, 0);
             *             mBacking->Deallocate(ptr);
             *         }
             *     }
             * private:
             *     NkAllocator* mBacking;
             * };
             * @endcode
             */
            class NKENTSEU_MEMORY_CLASS_EXPORT NkMemoryProfiler {
                public:
                    // =================================================================
                    // @name Types publics
                    // =================================================================
                    
                    /**
                     * @struct GlobalStats
                     * @brief Snapshot des statistiques globales de mémoire
                     * @ingroup MemoryProfiling
                     * 
                     * Toutes les valeurs sont en bytes sauf indication contraire.
                     * Les stats sont mises à jour atomiquement pour thread-safety.
                     */
                    struct GlobalStats {
                        nk_uint64 totalAllocations;    ///< Nombre total d'allocations (cumulatif)
                        nk_uint64 totalFrees;          ///< Nombre total de libérations (cumulatif)
                        nk_uint64 liveAllocations;     ///< Allocations actuellement actives
                        nk_uint64 liveBytes;           ///< Octets actuellement alloués (live)
                        nk_uint64 peakBytes;           ///< Pic historique d'octets alloués
                        float32   avgAllocationSize;   ///< Taille moyenne des allocations (en bytes)
                        
                        /** @brief Constructeur par défaut : zero-initialisation */
                        constexpr GlobalStats() noexcept 
                            : totalAllocations(0), totalFrees(0), liveAllocations(0)
                            , liveBytes(0), peakBytes(0), avgAllocationSize(0.0f) {}
                    };

                    // =================================================================
                    // @name Installation des Callbacks (thread-safe)
                    // =================================================================
                    
                    /**
                     * @brief Installe un callback pour les notifications d'allocation
                     * @param cb Fonction à appeler après chaque allocation réussie
                     * @note Thread-safe : mise à jour atomique du pointeur de callback
                     * @note Si cb == nullptr : désactive le callback (seules les stats internes sont mises à jour)
                     * @note Le callback sera appelé depuis le thread qui fait l'allocation
                     * 
                     * @example
                     * @code
                     * void MyAllocHook(void* ptr, nk_size size, const nk_char* tag) {
                     *     // Écrire dans un ring buffer pré-alloué, pas de malloc ici !
                     *     gProfilingBuffer.Write(AllocEvent{ptr, size, tag});
                     * }
                     * NkMemoryProfiler::SetAllocCallback(MyAllocHook);
                     * @endcode
                     */
                    static 
                    void SetAllocCallback(AllocCallback cb) noexcept;

                    /**
                     * @brief Installe un callback pour les notifications de libération
                     * @param cb Fonction à appeler après chaque libération
                     * @note Thread-safe et comportement identique à SetAllocCallback
                     */
                    static 
                    void SetFreeCallback(FreeCallback cb) noexcept;

                    /**
                     * @brief Installe un callback pour les notifications de réallocation
                     * @param cb Fonction à appeler après chaque réallocation réussie
                     * @note Thread-safe et comportement identique à SetAllocCallback
                     */
                    static 
                    void SetReallocCallback(ReallocCallback cb) noexcept;

                    // =================================================================
                    // @name Notifications Explicites (à appeler depuis les allocateurs)
                    // =================================================================
                    
                    /**
                     * @brief Notifie le profiler d'une allocation réussie
                     * @param ptr Pointeur alloué (doit être non-null)
                     * @param size Taille allouée en bytes
                     * @param tag Catégorie mémoire optionnelle (peut être nullptr)
                     * @note Thread-safe : mise à jour des stats + appel callback si installé
                     * @note Doit être appelé APRÈS l'allocation réussie, avant retour à l'appelant
                     * 
                     * @example
                     * @code
                     * void* DebugAlloc(nk_size size) {
                     *     void* ptr = malloc(size);
                     *     if (ptr) {
                     *         NkMemoryProfiler::NotifyAlloc(ptr, size, "Debug");
                     *     }
                     *     return ptr;
                     * }
                     * @endcode
                     */
                    static 
                    void NotifyAlloc(void* ptr, nk_size size, const nk_char* tag) noexcept;

                    /**
                     * @brief Notifie le profiler d'une libération
                     * @param ptr Pointeur libéré (nullptr accepté, no-op)
                     * @param size Taille libérée en bytes (0 si inconnue)
                     * @note Thread-safe : mise à jour des stats + appel callback si installé
                     * @note Doit être appelé APRÈS la libération effective (ou juste avant selon l'usage)
                     */
                    static 
                    void NotifyFree(void* ptr, nk_size size) noexcept;

                    /**
                     * @brief Notifie le profiler d'une réallocation réussie
                     * @param oldPtr Ancien pointeur (nullptr pour allocation initiale)
                     * @param newPtr Nouveau pointeur (nullptr si échec, mais alors ne pas appeler)
                     * @param oldSize Ancienne taille en bytes
                     * @param newSize Nouvelle taille en bytes
                     * @note Thread-safe : mise à jour des stats + appel callback si installé
                     * @note Gère les cas : resize in-place (oldPtr==newPtr) et réallocation complète
                     */
                    static 
                    void NotifyRealloc(void* oldPtr, void* newPtr, 
                                    nk_size oldSize, nk_size newSize) noexcept;

                    // =================================================================
                    // @name Interrogation des Statistiques (thread-safe)
                    // =================================================================
                    
                    /**
                     * @brief Obtient un snapshot thread-safe des statistiques globales
                     * @return Structure GlobalStats avec les métriques courantes
                     * @note Thread-safe : acquiert un lock pour lecture cohérente
                     * @note Snapshot : les valeurs peuvent changer juste après l'appel
                     * @note Utile pour affichage HUD, logging périodique, ou prise de décision runtime
                     */
                    [[nodiscard]] static 
                    GlobalStats GetGlobalStats() noexcept;

                    // =================================================================
                    // @name Reporting et Debugging
                    // =================================================================
                    
                    /**
                     * @brief Dump formaté des statistiques du profiler
                     * @note Thread-safe : acquiert un lock pour lecture cohérente
                     * @note Output vers NK_FOUNDATION_LOG_INFO (configurable)
                     * @note Format : résumé compact avec totaux et moyennes
                     * 
                     * @example output :
                     * @code
                     * [NKMemory][Profiler] total allocs=15234, frees=14890, live allocs=344
                     * [NKMemory][Profiler] live bytes=2457600, peak bytes=8912384, avg alloc=128.5 bytes
                     * @endcode
                     */
                    static 
                    void DumpProfilerStats() noexcept;

                private:
                    // =================================================================
                    // @name Membres statiques privés (implémentés dans NkProfiler.cpp)
                    // =================================================================
                    
                    /** @brief Callback installé pour les allocations */
                    static AllocCallback sAllocCB;
                    
                    /** @brief Callback installé pour les libérations */
                    static FreeCallback sFreeCB;
                    
                    /** @brief Callback installé pour les réallocations */
                    static ReallocCallback sReallocCB;
                    
                    /** @brief Stats globales agrégées (mises à jour atomiquement) */
                    static GlobalStats sStats;
                    
                    /** @brief Spinlock pour synchronisation thread-safe (NKCore) */
                    static NkSpinLock sLock;
            };

        } // namespace memory
    } // namespace nkentseu

#endif // NKENTSEU_MEMORY_NKPROFILER_H

// =============================================================================
// NOTES DE CONFIGURATION
// =============================================================================
/*
    [Désactiver le profiling en release pour performance]
    
    Dans votre configuration de build release, définir :
    
        #define NKENTSEU_DISABLE_MEMORY_PROFILING
    
    Effets :
    - NotifyAlloc/NotifyFree/NotifyRealloc deviennent des no-ops inline
    - GetGlobalStats retourne des zéros
    - DumpProfilerStats devient no-op
    - Overhead CPU : ~0%, Overhead mémoire : 0 bytes
    
    [Utilisation en production : bonnes pratiques]
    
    1. Callbacks légers : ne jamais allouer de mémoire dans un callback
       → Utiliser des buffers pré-alloués, des compteurs atomiques, ou du logging asynchrone
    
    2. Sampling optionnel : appeler Notify*() seulement sur un échantillon d'allocations
       → Réduit l'overhead tout en gardant des stats représentatives
    
    3. Tags significatifs : utiliser des tags courts et stables (string literals)
       → Éviter les allocations de string temporaires pour le tag
    
    4. Stats périodiques : appeler GetGlobalStats() toutes les N frames, pas à chaque alloc
       → Réduit la contention sur le lock interne
    
    [Intégration avec les allocateurs NKMemory]
    
    Pour instrumenter automatiquement un allocateur existant :
    
        class InstrumentedAllocator : public NkAllocator {
        public:
            InstrumentedAllocator(const nk_char* tag) : mTag(tag) {}
            
            Pointer Allocate(SizeType size, AlignType align) override {
                Pointer ptr = mBacking->Allocate(size, align);
                if (ptr) {
                    NkMemoryProfiler::NotifyAlloc(ptr, size, mTag);
                }
                return ptr;
            }
            
            void Deallocate(Pointer ptr) override {
                if (ptr) {
                    // Note: taille souvent inconnue, passer 0
                    NkMemoryProfiler::NotifyFree(ptr, 0);
                    mBacking->Deallocate(ptr);
                }
            }
            
        private:
            const nk_char* mTag;
            NkAllocator* mBacking;
        };
*/

// =============================================================================
// EXEMPLES D'UTILISATION AVANCÉS
// =============================================================================
/*
    // Exemple 1 : Callback pour détection de hitches (allocations lentes)
    struct AllocTiming {
        nk_uint64 timestamp;
        nk_size size;
    };
    
    void OnAllocWithTiming(void* ptr, nk_size size, const nk_char* tag) {
        static nk_uint64 lastAllocTime = 0;
        nk_uint64 now = nkentseu::platform::NkGetTimestamp();  // Fonction hypothétique
        
        nk_uint64 delta = now - lastAllocTime;
        if (delta > 1000000) {  // >1ms entre deux allocs : potentiel hitch
            NK_FOUNDATION_LOG_WARN("[Hitch] Alloc gap: %llu us, size=%zu, tag=%s", 
                                  delta / 1000, size, tag ? tag : "unknown");
        }
        lastAllocTime = now;
    }
    
    // Exemple 2 : Sampling pour réduire l'overhead en production
    class SamplingProfiler {
    public:
        SamplingProfiler(nk_uint32 sampleRate) : mRate(sampleRate), mCounter(0) {}
        
        void Notify(void* ptr, nk_size size, const nk_char* tag) {
            // Appeler le vrai profiler seulement 1 fois sur mRate
            if (++mCounter >= mRate) {
                mCounter = 0;
                nkentseu::memory::NkMemoryProfiler::NotifyAlloc(ptr, size, tag);
            }
        }
        
    private:
        nk_uint32 mRate;      // Ex: 100 = 1% des allocs sont profilées
        nk_uint32 mCounter;
    };
    
    // Exemple 3 : Monitoring HUD en temps réel
    void UpdateMemoryHUD() {
        static nk_uint64 lastUpdate = 0;
        static nkentseu::memory::NkMemoryProfiler::GlobalStats lastStats;
        
        auto now = nkentseu::platform::NkGetTimestamp();
        if (now - lastUpdate < 500000) {  // Update max toutes les 500ms
            return;
        }
        lastUpdate = now;
        
        auto stats = nkentseu::memory::NkMemoryProfiler::GetGlobalStats();
        
        // Calcul du delta depuis le dernier update
        nk_int64 bytesDelta = static_cast<nk_int64>(stats.liveBytes) - 
                             static_cast<nk_int64>(lastStats.liveBytes);
        
        // Affichage HUD (pseudo-code)
        HUD_DrawText("Memory: %.1f MB (peak: %.1f MB, Δ: %+d KB)",
                    stats.liveBytes / 1024.0f / 1024.0f,
                    stats.peakBytes / 1024.0f / 1024.0f,
                    static_cast<int>(bytesDelta / 1024));
        
        lastStats = stats;
    }
    
    // Exemple 4 : Alertes de budget en temps réel
    void CheckMemoryBudgets() {
        constexpr nk_uint64 RENDER_BUDGET = 256 * 1024 * 1024;  // 256 MB
        
        auto stats = nkentseu::memory::NkMemoryProfiler::GetGlobalStats();
        if (stats.liveBytes > RENDER_BUDGET) {
            NK_FOUNDATION_LOG_ERROR("[Budget] Memory over limit: %llu/%llu bytes",
                                   static_cast<unsigned long long>(stats.liveBytes),
                                   static_cast<unsigned long long>(RENDER_BUDGET));
            // Action corrective : libérer des caches, réduire la qualité, etc.
        }
    }
*/

// =============================================================================
// COMPARAISON : NkMemoryProfiler vs NkMemoryTracker
// =============================================================================
/*
    | Critère              | NkMemoryProfiler                    | NkMemoryTracker              |
    |---------------------|-------------------------------------|------------------------------|
    | Objectif principal  | Monitoring runtime, callbacks       | Détection de fuites, debug   |
    | Granularité         | Stats globales agrégées             | Métadonnées par allocation   |
    | Overhead mémoire    | ~40 bytes (stats + lock)           | ~100 bytes par allocation    |
    | Overhead CPU        | ~10-20 cycles par Notify*()        | ~50-100 cycles par Register  |
    | Thread-safety       | Oui (NkSpinLock)                   | Oui (NkSpinLock)             |
    | Usage en production | Oui (avec sampling)                | Non (debug uniquement)       |
    | Callbacks personnalisés | Oui (3 types)                  | Non                          |
    | Lookup par pointeur | Non                                | Oui (O(1) hash table)        |
    | Dump des fuites     | Non (stats uniquement)             | Oui (liste détaillée)        |

    Quand utiliser NkMemoryProfiler :
    ✓ Monitoring runtime en production avec overhead minimal
    ✓ Intégration avec des outils externes via callbacks
    ✓ Stats agrégées pour HUD, logging, ou prise de décision
    ✓ Détection de patterns (hitches, pics) en temps réel

    Quand utiliser NkMemoryTracker :
    ✓ Debugging de fuites mémoire en développement
    ✓ Inspection détaillée d'allocations individuelles
    ✓ Validation de cleanup mémoire en fin de test
    ✓ Profiling approfondi avec contexte complet (fichier, ligne, fonction)

    Utilisation conjointe recommandée :
    - En debug : activer les deux pour debugging complet + monitoring
    - En release : désactiver NkMemoryTracker, garder NkMemoryProfiler avec sampling
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================