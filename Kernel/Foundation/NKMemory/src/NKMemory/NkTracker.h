// =============================================================================
// NKMemory/NkTracker.h
// Détection de fuites mémoire O(1) via table de hachage.
//
// Design :
//  - Séparation claire déclarations (.h) / implémentations (.cpp)
//  - Réutilisation des primitives NKCore (ZÉRO duplication)
//  - Thread-safe via NkSpinLock de NKCore
//  - Intégration avec NkTag.h pour le tagging des allocations
//  - Overhead minimal en release via macros conditionnelles
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_MEMORY_NKTRACKER_H
#define NKENTSEU_MEMORY_NKTRACKER_H

    // -------------------------------------------------------------------------
    // SECTION 1 : DÉPENDANCES
    // -------------------------------------------------------------------------
    #include "NKMemory/NkMemoryApi.h"      // NKENTSEU_MEMORY_API
    #include "NKMemory/NkTag.h"            // NkMemoryTag pour catégorisation
    #include "NKCore/NkTypes.h"            // nk_size, nk_uintptr, nk_uint64, etc.
    #include "NKCore/NkAtomic.h"           // NkSpinLock pour synchronisation

    // -------------------------------------------------------------------------
    // SECTION 2 : CONFIGURATION DU MODULE TRACKING
    // -------------------------------------------------------------------------
    /**
     * @defgroup MemoryLeakTracking Tracking et Détection de Fuites Mémoire
     * @brief Système de détection de fuites mémoire O(1) pour le debugging
     * 
     * Caractéristiques :
     *  - Table de hachage pour lookup O(1) moyen (vs O(n) liste chaînée)
     *  - Thread-safe via spinlock léger (NkSpinLock de NKCore)
     *  - Intégration avec NkMemoryTag pour catégorisation des fuites
     *  - Stats en temps réel : allocations live, peak, total
     *  - Dump formaté des fuites avec contexte (fichier, ligne, fonction)
     * 
     * @note En mode release avec NKENTSEU_DISABLE_MEMORY_TRACKING, 
     *       la plupart des fonctions deviennent des no-ops pour performance.
     * 
     * @example
     * @code
     * // Initialisation (généralement au démarrage de l'application)
     * static nkentseu::memory::NkMemoryTracker gTracker;
     * 
     * // Enregistrement d'une allocation (appelé par les wrappers d'alloc)
     * nkentseu::memory::NkAllocationInfo info{};
     * info.userPtr = ptr;
     * info.size = size;
     * info.tag = static_cast<nk_uint8>(nkentseu::memory::NkMemoryTag::NK_MEMORY_RENDER);
     * info.file = __FILE__;
     * info.line = __LINE__;
     * info.function = __func__;
     * gTracker.Register(info);
     * 
     * // Désenregistrement à la libération
     * gTracker.Unregister(ptr);
     * 
     * // Dump des fuites à la fermeture
     * #if defined(NKENTSEU_DEBUG)
     *     gTracker.DumpLeaks();
     * #endif
     * @endcode
     */

    namespace nkentseu {
        namespace memory {

            // -------------------------------------------------------------------------
            // SECTION 3 : STRUCTURE D'INFORMATION D'ALLOCATION
            // -------------------------------------------------------------------------
            /**
             * @struct NkAllocationInfo
             * @brief Métadonnées d'une allocation pour le tracking
             * @ingroup MemoryLeakTracking
             * 
             * Stocke toutes les informations nécessaires pour déboguer une allocation :
             *  - Pointeurs utilisateur et base (pour les allocations alignées)
             *  - Taille, alignement, count (pour tableaux)
             *  - Tag mémoire pour catégorisation
             *  - Contexte d'appel : fichier, ligne, fonction, nom optionnel
             *  - Metadata système : timestamp, thread ID
             * 
             * @note Cette structure est copiée dans la table de hachage : garder compacte
             * @note Les pointeurs de string (file, function, name) sont non-possédés
             */
            struct NKENTSEU_MEMORY_CLASS_EXPORT NkAllocationInfo {
                // =================================================================
                // @name Identifiants de l'allocation
                // =================================================================
                
                /** @brief Pointeur retourné à l'utilisateur (peut être aligné) */
                void* userPtr;
                
                /** @brief Pointeur brut retourné par l'allocateur (base réelle) */
                void* basePtr;
                
                /** @brief Taille de l'allocation en bytes */
                nk_size size;
                
                /** @brief Nombre d'éléments (pour les allocations de type tableau) */
                nk_size count;
                
                /** @brief Alignement requis pour cette allocation */
                nk_size alignment;
                
                /** @brief Tag mémoire pour catégorisation (NkMemoryTag casté en uint8) */
                nk_uint8 tag;

                // =================================================================
                // @name Contexte de debug (disponible seulement en debug builds)
                // =================================================================
                
                /** @brief Fichier source de l'appel d'allocation (non-possédé) */
                const nk_char* file;
                
                /** @brief Numéro de ligne de l'appel d'allocation */
                nk_int32 line;
                
                /** @brief Fonction contenant l'appel d'allocation (non-possédé) */
                const nk_char* function;
                
                /** @brief Nom optionnel pour l'allocation (ex: "TextureCache", non-possédé) */
                const nk_char* name;

                // =================================================================
                // @name Metadata système (pour profiling avancé)
                // =================================================================
                
                /** @brief Timestamp de l'allocation (ticks système ou temps relatif) */
                nk_uint64 timestamp;
                
                /** @brief ID du thread ayant effectué l'allocation */
                nk_uint32 threadId;

                // =================================================================
                // @name Constructeurs et helpers
                // =================================================================
                
                /** @brief Constructeur par défaut : zero-initialisation */
                constexpr NkAllocationInfo() noexcept 
                    : userPtr(nullptr), basePtr(nullptr), size(0), count(0)
                    , alignment(0), tag(0), file(nullptr), line(0)
                    , function(nullptr), name(nullptr), timestamp(0), threadId(0) {}

                /**
                 * @brief Helper de construction rapide pour usage courant
                 * @param ptr Pointeur utilisateur
                 * @param sz Taille en bytes
                 * @param memoryTag Catégorie mémoire
                 * @param srcFile Fichier source (__FILE__)
                 * @param srcLine Ligne source (__LINE__)
                 * @param srcFunc Fonction source (__func__)
                 * @return NkAllocationInfo initialisé
                 */
                static 
                NkAllocationInfo Make(void* ptr, nk_size sz, NkMemoryTag memoryTag,
                                     const nk_char* srcFile, nk_int32 srcLine, 
                                     const nk_char* srcFunc) noexcept {
                    NkAllocationInfo info;
                    info.userPtr = ptr;
                    info.basePtr = ptr;  // Par défaut, même si allocation alignée
                    info.size = sz;
                    info.count = 1;
                    info.alignment = 0;
                    info.tag = static_cast<nk_uint8>(memoryTag);
                    info.file = srcFile;
                    info.line = srcLine;
                    info.function = srcFunc;
                    info.name = nullptr;
                    // timestamp et threadId initialisés à 0 (remplis par le tracker si besoin)
                    return info;
                }
            };

            // -------------------------------------------------------------------------
            // SECTION 4 : CLASSE PRINCIPALE NkMemoryTracker
            // -------------------------------------------------------------------------
            /**
             * @class NkMemoryTracker
             * @brief Tracker de fuites mémoire O(1) via table de hachage
             * @ingroup MemoryLeakTracking
             * 
             * Fonctionnalités :
             *  - Enregistrement/désenregistrement d'allocations en O(1) moyen
             *  - Lookup de métadonnées par pointeur en O(1) moyen
             *  - Détection et dump des fuites à la fermeture
             *  - Statistiques en temps réel : live bytes, peak, total allocations
             *  - Thread-safe via NkSpinLock (NKCore) pour usage multi-thread
             * 
             * @note Instance unique recommandée par application (singleton statique)
             * @note En release avec NKENTSEU_DISABLE_MEMORY_TRACKING : méthodes no-op
             * @note Overhead mémoire : ~4096 * sizeof(Entry*) + entries actives
             * 
             * @example
             * @code
             * // Déclaration globale (fichier de startup)
             * static nkentseu::memory::NkMemoryTracker gMemoryTracker;
             * 
             * // Wrapper d'allocation avec tracking
             * void* DebugAlloc(nk_size size, const char* file, int line) {
             *     void* ptr = malloc(size);
             *     if (ptr) {
             *         auto info = nkentseu::memory::NkAllocationInfo::Make(
             *             ptr, size, nkentseu::memory::NkMemoryTag::NK_MEMORY_ENGINE,
             *             file, line, __func__);
             *         gMemoryTracker.Register(info);
             *     }
             *     return ptr;
             * }
             * 
             * // Wrapper de free avec tracking
             * void DebugFree(void* ptr, const char* file, int line) {
             *     if (ptr) {
             *         gMemoryTracker.Unregister(ptr);
             *         free(ptr);
             *     }
             * }
             * 
             * // À la fermeture de l'application
             * void Shutdown() {
             *     #if defined(NKENTSEU_DEBUG)
             *         gMemoryTracker.DumpLeaks();  // Liste toutes les allocations non-libérées
             *     #endif
             * }
             * @endcode
             */
            class NKENTSEU_MEMORY_CLASS_EXPORT NkMemoryTracker {
            public:
                // =================================================================
                // @name Constructeur / Destructeur
                // =================================================================
                
                /** @brief Constructeur par défaut : initialise la table de hachage */
                NkMemoryTracker() noexcept;
                
                /**
                 * @brief Destructeur : libère toutes les entrées restantes
                 * @note Appel automatique de ClearInternal() pour éviter les fuites du tracker lui-même
                 * @note Thread-safe : acquiert le lock avant cleanup
                 */
                ~NkMemoryTracker();

                // Non-copiable : singleton ou instance globale recommandée
                NkMemoryTracker(const NkMemoryTracker&) = delete;
                NkMemoryTracker& operator=(const NkMemoryTracker&) = delete;

                // =================================================================
                // @name API de Tracking (thread-safe)
                // =================================================================
                
                /**
                 * @brief Enregistre une nouvelle allocation dans le tracker
                 * @param info Métadonnées de l'allocation à tracker
                 * @note Thread-safe : acquiert un spinlock pour la durée de l'opération
                 * @note O(1) moyen : insertion en tête de liste dans le bucket hashé
                 * @note Si le pointeur existe déjà : met à jour l'entrée existante
                 * 
                 * @example
                 * @code
                 * void* ptr = malloc(1024);
                 * NkAllocationInfo info = NkAllocationInfo::Make(ptr, 1024, 
                 *     NkMemoryTag::NK_MEMORY_ENGINE, __FILE__, __LINE__, __func__);
                 * tracker.Register(info);
                 * @endcode
                 */
                void Register(const NkAllocationInfo& info) noexcept;

                /**
                 * @brief Désenregistre une allocation précédemment trackée
                 * @param ptr Pointeur utilisateur à désenregistrer
                 * @note Thread-safe : acquiert un spinlock pour la durée de l'opération
                 * @note O(1) moyen : recherche dans le bucket hashé + suppression
                 * @note Safe : si ptr non trouvé, opération no-op (pas d'erreur)
                 * 
                 * @example
                 * @code
                 * free(ptr);  // Libération réelle d'abord
                 * tracker.Unregister(ptr);  // Puis retrait du tracking
                 * @endcode
                 */
                void Unregister(void* ptr) noexcept;

                /**
                 * @brief Recherche les métadonnées d'une allocation par pointeur
                 * @param ptr Pointeur utilisateur à rechercher
                 * @return Pointeur vers NkAllocationInfo si trouvé, nullptr sinon
                 * @note Thread-safe : acquiert un spinlock pour la durée de la recherche
                 * @note O(1) moyen : lookup dans le bucket hashé
                 * @note Retourne nullptr si l'allocation n'est pas trackée
                 * 
                 * @example
                 * @code
                 * if (const auto* info = tracker.Find(ptr)) {
                 *     printf("Allocated at %s:%d (%s), size=%zu\n",
                 *            info->file, info->line, info->function, info->size);
                 * }
                 * @endcode
                 */
                [[nodiscard]] 
                const NkAllocationInfo* Find(void* ptr) const noexcept;

                // =================================================================
                // @name Reporting et Debugging
                // =================================================================
                
                /**
                 * @brief Dump formaté de toutes les allocations non-libérées (fuites)
                 * @note Thread-safe : acquiert un spinlock pour la durée du dump
                 * @note Output vers NK_FOUNDATION_LOG_INFO (configurable)
                 * @note Format : une ligne par fuite avec contexte complet
                 * 
                 * @example output :
                 * @code
                 * [NKMemory] Leak summary:
                 *   [LEAK] 0x7f8a1c001000 size=1024 tag=20 at RenderSystem.cpp:142 (LoadTexture)
                 *   [LEAK] 0x7f8a1c002000 size=4096 tag=22 at MeshLoader.cpp:89 (CreateVertexBuffer)
                 * [NKMemory] Total: 2 leaks, 5120 bytes
                 * @endcode
                 */
                void DumpLeaks() const noexcept;

                // =================================================================
                // @name Statistiques
                // =================================================================
                
                /**
                 * @struct Stats
                 * @brief Snapshot des statistiques du tracker
                 * @ingroup MemoryLeakTracking
                 */
                struct Stats {
                    nk_size liveBytes;          ///< Octets actuellement alloués et trackés
                    nk_size peakBytes;          ///< Pic historique d'octets trackés
                    nk_size liveAllocations;    ///< Nombre d'allocations actuellement live
                    nk_uint64 totalAllocations; ///< Nombre total d'allocations enregistrées (cumulatif)
                };

                /**
                 * @brief Obtient un snapshot thread-safe des statistiques
                 * @return Structure Stats avec les métriques courantes
                 * @note Thread-safe : acquiert un spinlock pour la lecture cohérente
                 * @note Snapshot : les valeurs peuvent changer juste après l'appel
                 */
                [[nodiscard]] 
                Stats GetStats() const noexcept;

                // =================================================================
                // @name Gestion du cycle de vie
                // =================================================================
                
                /**
                 * @brief Réinitialise complètement le tracker (libère toutes les entrées)
                 * @note Thread-safe : acquiert un spinlock pour la durée de l'opération
                 * @note Utile avant shutdown pour éviter les faux positifs de fuites
                 * @note Ne modifie pas les allocations réelles, seulement le tracking
                 */
                void Clear() noexcept;

            private:
                // =================================================================
                // @name Implémentation interne (détaillée dans NkTracker.cpp)
                // =================================================================
                
                /** @brief Nombre de buckets dans la table de hachage (puissance de 2 recommandée) */
                static constexpr nk_size NK_TRACKER_BUCKET_COUNT = 4096u;

                /**
                 * @struct Entry
                 * @brief Noeud de la liste chaînée dans un bucket de la hash table
                 * @note Structure interne, non exposée dans l'API publique
                 */
                struct Entry {
                    NkAllocationInfo Info;  ///< Métadonnées de l'allocation
                    Entry* Next;            ///< Prochain noeud dans le même bucket (chaining)
                };

                /**
                 * @brief Fonction de hachage pour pointeurs
                 * @param ptr Pointeur à hasher
                 * @return Index de bucket dans [0, NK_TRACKER_BUCKET_COUNT)
                 * @note Mix des bits du pointeur pour distribution uniforme
                 */
                [[nodiscard]] static nk_size HashPtr(const void* ptr) noexcept;

                /**
                 * @brief Recherche une entrée par pointeur (version mutable)
                 * @param ptr Pointeur à rechercher
                 * @return Pointeur vers Entry si trouvé, nullptr sinon
                 * @note Précondition : lock déjà acquis par l'appelant
                 */
                [[nodiscard]] Entry* FindEntry(void* ptr) noexcept;

                /**
                 * @brief Recherche une entrée par pointeur (version const)
                 * @param ptr Pointeur à rechercher
                 * @return Pointeur vers Entry const si trouvé, nullptr sinon
                 * @note Précondition : lock déjà acquis par l'appelant
                 */
                [[nodiscard]] const Entry* FindEntryConst(const void* ptr) const noexcept;

                /**
                 * @brief Libère toutes les entrées de la table de hachage
                 * @note Précondition : lock déjà acquis par l'appelant
                 * @note Reset également les compteurs statistiques
                 */
                void ClearInternal() noexcept;

                // =================================================================
                // @name Membres privés
                // =================================================================
                
                /** @brief Table de hachage : tableau de têtes de listes chaînées */
                Entry* mBuckets[NK_TRACKER_BUCKET_COUNT];
                
                /** @brief Nombre d'allocations actuellement trackées (live) */
                nk_size mLiveAllocations;
                
                /** @brief Total d'octets actuellement alloués et trackés */
                nk_size mTotalLiveBytes;
                
                /** @brief Pic historique d'octets trackés */
                nk_size mPeakBytes;
                
                /** @brief Compteur cumulatif de toutes les allocations enregistrées */
                nk_uint64 mTotalAllocations;
                
                /** @brief Spinlock pour synchronisation thread-safe (NKCore) */
                mutable NkSpinLock mLock;
            };

            // -------------------------------------------------------------------------
            // SECTION 5 : HELPERS GLOBAUX (Optionnels, pour intégration facile)
            // -------------------------------------------------------------------------
            /**
             * @defgroup MemoryTrackerHelpers Helpers d'Intégration du Tracker
             * @brief Fonctions utilitaires pour intégrer le tracking dans les allocateurs
             */

            /**
             * @brief Wrapper macro pour enregistrer une allocation avec contexte automatique
             * @param ptr Pointeur alloué
             * @param size Taille en bytes
             * @param tag Catégorie mémoire (NkMemoryTag)
             * @ingroup MemoryTrackerHelpers
             * 
             * @note Utilise __FILE__, __LINE__, __func__ automatiquement pour le contexte
             * @note En release avec NKENTSEU_DISABLE_MEMORY_TRACKING : se réduit à ((void)0)
             * 
             * @example
             * @code
             * void* ptr = malloc(1024);
             * NK_TRACK_ALLOC(ptr, 1024, NkMemoryTag::NK_MEMORY_ENGINE);
             * @endcode
             */
            #if !defined(NKENTSEU_DISABLE_MEMORY_TRACKING)
                #define NK_TRACK_ALLOC(ptr, size, tag) \
                    do { \
                        if (ptr) { \
                            auto info = nkentseu::memory::NkAllocationInfo::Make( \
                                ptr, size, tag, __FILE__, __LINE__, __func__); \
                            nkentseu::memory::NkMemoryTracker::GetGlobal().Register(info); \
                        } \
                    } while(0)
            #else
                #define NK_TRACK_ALLOC(ptr, size, tag) ((void)0)
            #endif

            /**
             * @brief Wrapper macro pour désenregistrer une allocation
             * @param ptr Pointeur à désenregistrer
             * @ingroup MemoryTrackerHelpers
             * 
             * @note En release avec NKENTSEU_DISABLE_MEMORY_TRACKING : se réduit à ((void)0)
             * 
             * @example
             * @code
             * free(ptr);
             * NK_UNTRACK_ALLOC(ptr);
             * @endcode
             */
            #if !defined(NKENTSEU_DISABLE_MEMORY_TRACKING)
                #define NK_UNTRACK_ALLOC(ptr) \
                    do { \
                        if (ptr) { \
                            nkentseu::memory::NkMemoryTracker::GetGlobal().Unregister(ptr); \
                        } \
                    } while(0)
            #else
                #define NK_UNTRACK_ALLOC(ptr) ((void)0)
            #endif

            /**
             * @brief Accès à l'instance globale du tracker (singleton)
             * @return Référence vers l'instance unique de NkMemoryTracker
             * @note Thread-safe à l'initialisation (Meyer's singleton, C++11+)
             * @ingroup MemoryTrackerHelpers
             */
            NKENTSEU_MEMORY_API 
            NkMemoryTracker& GetGlobalMemoryTracker() noexcept;

        } // namespace memory
    } // namespace nkentseu

#endif // NKENTSEU_MEMORY_NKTRACKER_H

// =============================================================================
// NOTES DE CONFIGURATION
// =============================================================================
/*
    [Désactiver le tracking en release pour performance]
    
    Dans votre configuration de build release, définir :
    
        #define NKENTSEU_DISABLE_MEMORY_TRACKING
    
    Effets :
    - Register/Unregister/Find deviennent des no-ops inline
    - DumpLeaks/GetStats retournent des valeurs vides/zero
    - Overhead CPU : ~0%, Overhead mémoire : 0 bytes
    - Les macros NK_TRACK_ALLOC/NK_UNTRACK_ALLOC se réduisent à ((void)0)
    
    [Activer le tracking détaillé en debug]
    
    En debug, définir en plus :
    
        #define NKENTSEU_MEMORY_TRACKING_VERBOSE
    
    Effets :
    - Log chaque Register/Unregister avec contexte complet
    - Dump automatique des stats à la fermeture de l'application
    - Assertions sur incohérences (double-register, unregister inconnu)
    
    [Intégration avec les allocateurs existants]
    
    Pour tracker automatiquement les allocations d'un allocateur personnalisé :
    
        class TrackedAllocator : public NkAllocator {
        public:
            Pointer Allocate(SizeType size, AlignType align) override {
                void* ptr = mBacking->Allocate(size, align);
                if (ptr) {
                    NK_TRACK_ALLOC(ptr, size, mTag);  // Macro d'intégration
                }
                return ptr;
            }
            
            void Deallocate(Pointer ptr) override {
                if (ptr) {
                    NK_UNTRACK_ALLOC(ptr);  // Macro d'intégration
                    mBacking->Deallocate(ptr);
                }
            }
            
        private:
            NkMemoryTag mTag;
            NkAllocator* mBacking;
        };
*/

// =============================================================================
// EXEMPLES D'UTILISATION AVANCÉS
// =============================================================================
/*
    // Exemple 1 : Intégration avec malloc/free pour debugging global
    #if defined(NKENTSEU_DEBUG)
        #define malloc(size) DebugMalloc(size, __FILE__, __LINE__)
        #define free(ptr) DebugFree(ptr, __FILE__, __LINE__)
        
        void* DebugMalloc(size_t size, const char* file, int line) {
            void* ptr = ::malloc(size);
            if (ptr) {
                auto info = nkentseu::memory::NkAllocationInfo::Make(
                    ptr, size, nkentseu::memory::NkMemoryTag::NK_MEMORY_DEBUG,
                    file, line, "DebugMalloc");
                nkentseu::memory::GetGlobalMemoryTracker().Register(info);
            }
            return ptr;
        }
        
        void DebugFree(void* ptr, const char* file, int line) {
            if (ptr) {
                nkentseu::memory::GetGlobalMemoryTracker().Unregister(ptr);
                ::free(ptr);
            }
        }
    #endif
    
    // Exemple 2 : Tracking par scope pour détecter les fuites locales
    class ScopedMemoryTracker {
    public:
        ScopedMemoryTracker(const char* scopeName) 
            : mName(scopeName), mStartStats(GetGlobalMemoryTracker().GetStats()) {}
        
        ~ScopedMemoryTracker() {
            auto endStats = GetGlobalMemoryTracker().GetStats();
            nk_size leaked = endStats.liveBytes - mStartStats.liveBytes;
            if (leaked > 0) {
                NK_FOUNDATION_LOG_WARN("[MemoryScope] %s: leaked %zu bytes", 
                                      mName, leaked);
                GetGlobalMemoryTracker().DumpLeaks();  // Dump détaillé
            }
        }
        
    private:
        const char* mName;
        NkMemoryTracker::Stats mStartStats;
    };
    
    void TestFunction() {
        ScopedMemoryTracker scope("TestFunction");
        // ... code à tester ...
        // Si des allocations ne sont pas libérées, warning à la sortie du scope
    }
    
    // Exemple 3 : Filtrage des fuites par tag pour debugging ciblé
    void DumpLeaksByTag(NkMemoryTag tag) {
        auto& tracker = GetGlobalMemoryTracker();
        // Note: nécessite une extension de DumpLeaks pour filtrer par tag
        // Ou itération manuelle via Find() sur les pointeurs connus
        NK_FOUNDATION_LOG_INFO("Dumping leaks for tag %d...", static_cast<int>(tag));
        tracker.DumpLeaks();  // Dump complet en attendant un filtre
    }
    
    // Exemple 4 : Monitoring en temps réel pour profiling
    void FrameEndMemoryCheck() {
        static nk_size lastFrameBytes = 0;
        
        auto stats = GetGlobalMemoryTracker().GetStats();
        nk_int64 delta = static_cast<nk_int64>(stats.liveBytes) - static_cast<nk_int64>(lastFrameBytes);
        
        if (delta > 0) {
            NK_FOUNDATION_LOG_DEBUG("[Memory] Frame growth: +%lld bytes (total: %zu)", 
                                   delta, stats.liveBytes);
        } else if (delta < 0) {
            NK_FOUNDATION_LOG_DEBUG("[Memory] Frame shrink: %lld bytes (total: %zu)", 
                                   delta, stats.liveBytes);
        }
        
        lastFrameBytes = stats.liveBytes;
    }
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================