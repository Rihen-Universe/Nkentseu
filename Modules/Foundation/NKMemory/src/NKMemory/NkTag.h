// =============================================================================
// NKMemory/NkTag.h
// Système de tagging et budgétisation mémoire pour le profiling et le debugging.
//
// Design :
//  - Réutilisation des types NKCore (ZÉRO duplication)
//  - API thread-safe pour le tracking mémoire en temps réel
//  - Macros NKENTSEU_MEMORY_API pour la visibilité des symboles
//  - Extensible : nouveaux tags ajoutés sans breaking change
//  - Debug-only en release : overhead minimal en production
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_MEMORY_NKTAG_H
#define NKENTSEU_MEMORY_NKTAG_H

    // -------------------------------------------------------------------------
    // SECTION 1 : DÉPENDANCES
    // -------------------------------------------------------------------------
    #include "NKMemory/NkMemoryApi.h"      // NKENTSEU_MEMORY_API, inline macros
    #include "NKMemory/NkAllocator.h"
    #include "NKCore/NkTypes.h"            // nk_uint8, nk_uint64, float32, etc.

    // -------------------------------------------------------------------------
    // SECTION 2 : CONFIGURATION DU MODULE TAGGING
    // -------------------------------------------------------------------------
    /**
     * @defgroup MemoryTagging Tagging et Budgétisation Mémoire
     * @brief Système de catégorisation et tracking des allocations mémoire
     * 
     * Fonctionnalités :
     *  - Catégorisation des allocations par domaine (Render, Physics, AI, etc.)
     *  - Définition de budgets mémoire par catégorie
     *  - Détection et alerte de dépassement de budget
     *  - Statistiques en temps réel : pic, moyenne, fragmentation
     *  - Dump pour debugging et profiling
     * 
     * @note En mode release avec NKENTSEU_DISABLE_MEMORY_TAGGING, 
     *       la plupart des fonctions deviennent des no-ops pour performance.
     * 
     * @example
     * @code
     * // Définir un budget pour le système de rendu
     * nkentseu::memory::NkMemoryBudget::SetBudget(
     *     nkentseu::memory::NkMemoryTag::NK_MEMORY_RENDER, 
     *     256 * 1024 * 1024  // 256 MB
     * );
     * 
     * // Allouer avec tagging (via wrapper d'allocateur)
     * void* texture = TaggedAlloc(1024*1024, nkentseu::memory::NkMemoryTag::NK_MEMORY_TEXTURE);
     * 
     * // Vérifier l'état du budget
     * if (nkentseu::memory::NkMemoryBudget::IsOverBudget(
     *         nkentseu::memory::NkMemoryTag::NK_MEMORY_TEXTURE)) {
     *     NK_FOUNDATION_LOG_WARN("Texture memory over budget!");
     * }
     * 
     * // Dump des stats pour debugging
     * nkentseu::memory::NkMemoryBudget::DumpStats();
     * @endcode
     */

    namespace nkentseu {
        namespace memory {

            // -------------------------------------------------------------------------
            // SECTION 3 : ÉNUMÉRATION DES TAGS MÉMOIRE
            // -------------------------------------------------------------------------
            /**
             * @enum NkMemoryTag
             * @brief Catégories mémoire pour le tagging et le tracking
             * @ingroup MemoryTagging
             * 
             * Les tags sont organisés par domaine fonctionnel avec des plages dédiées :
             *  - 0-9   : Core engine
             *  - 10-19 : Game systems
             *  - 20-29 : Rendering
             *  - 30-39 : Physics
             *  - 40-49 : Audio
             *  - 50-59 : Streaming/Network
             *  - 255   : Debug/Profiling
             * 
             * @note Valeur nk_uint8 : maximum 256 tags distincts
             * @note Ajouter de nouveaux tags dans la plage appropriée, éviter les collisions
             */
            enum class NkMemoryTag : nk_uint8 {
                // =================================================================
                // Core Engine (0-9)
                // =================================================================
                /** @brief Mémoire interne du moteur (non catégorisée ailleurs) */
                NK_MEMORY_ENGINE      = 0,
                
                /** @brief Conteneurs génériques : vectors, maps, strings */
                NK_MEMORY_CONTAINER   = 1,
                
                /** @brief Overhead des allocateurs : metadata, free-lists */
                NK_MEMORY_ALLOCATOR   = 2,
                
                // Réservé pour extensions futures du core
                NK_MEMORY_CORE_RESERVED_3 = 3,
                NK_MEMORY_CORE_RESERVED_4 = 4,
                NK_MEMORY_CORE_RESERVED_5 = 5,
                NK_MEMORY_CORE_RESERVED_6 = 6,
                NK_MEMORY_CORE_RESERVED_7 = 7,
                NK_MEMORY_CORE_RESERVED_8 = 8,
                NK_MEMORY_CORE_RESERVED_9 = 9,

                // =================================================================
                // Game Systems (10-19)
                // =================================================================
                /** @brief Objets métier du jeu : gameplay logic */
                NK_MEMORY_GAME        = 10,
                
                /** @brief Entités et composants ECS */
                NK_MEMORY_ENTITY      = 11,
                
                /** @brief Machine virtuelle de scripting (Lua, etc.) */
                NK_MEMORY_SCRIPT      = 12,
                
                // Réservé pour extensions futures des systèmes jeu
                NK_MEMORY_GAME_RESERVED_13 = 13,
                NK_MEMORY_GAME_RESERVED_14 = 14,
                NK_MEMORY_GAME_RESERVED_15 = 15,
                NK_MEMORY_GAME_RESERVED_16 = 16,
                NK_MEMORY_GAME_RESERVED_17 = 17,
                NK_MEMORY_GAME_RESERVED_18 = 18,
                NK_MEMORY_GAME_RESERVED_19 = 19,

                // =================================================================
                // Rendering (20-29)
                // =================================================================
                /** @brief Système de rendu : commandes, pipelines, descriptors */
                NK_MEMORY_RENDER      = 20,
                
                /** @brief Données de textures (CPU side, avant upload GPU) */
                NK_MEMORY_TEXTURE     = 21,
                
                /** @brief Données de meshes : vertices, indices, skins */
                NK_MEMORY_MESH        = 22,
                
                /** @brief Shaders compilés, pipelines graphics/compute */
                NK_MEMORY_SHADER      = 23,
                
                // Réservé pour extensions futures du rendu
                NK_MEMORY_RENDER_RESERVED_24 = 24,
                NK_MEMORY_RENDER_RESERVED_25 = 25,
                NK_MEMORY_RENDER_RESERVED_26 = 26,
                NK_MEMORY_RENDER_RESERVED_27 = 27,
                NK_MEMORY_RENDER_RESERVED_28 = 28,
                NK_MEMORY_RENDER_RESERVED_29 = 29,

                // =================================================================
                // Physics (30-39)
                // =================================================================
                /** @brief Moteur physique : broadphase, solvers, constraints */
                NK_MEMORY_PHYSICS     = 30,
                
                /** @brief Shapes de collision : meshes convexes, primitives */
                NK_MEMORY_COLLISION   = 31,
                
                /** @brief Corps rigides : états, transforms, velocities */
                NK_MEMORY_RIGID       = 32,
                
                // Réservé pour extensions futures de la physique
                NK_MEMORY_PHYSICS_RESERVED_33 = 33,
                NK_MEMORY_PHYSICS_RESERVED_34 = 34,
                NK_MEMORY_PHYSICS_RESERVED_35 = 35,
                NK_MEMORY_PHYSICS_RESERVED_36 = 36,
                NK_MEMORY_PHYSICS_RESERVED_37 = 37,
                NK_MEMORY_PHYSICS_RESERVED_38 = 38,
                NK_MEMORY_PHYSICS_RESERVED_39 = 39,

                // =================================================================
                // Audio (40-49)
                // =================================================================
                /** @brief Système audio : mixers, buses, effects */
                NK_MEMORY_AUDIO       = 40,
                
                /** @brief Données de sons : buffers PCM, streaming */
                NK_MEMORY_SOUND       = 41,
                
                // Réservé pour extensions futures de l'audio
                NK_MEMORY_AUDIO_RESERVED_42 = 42,
                NK_MEMORY_AUDIO_RESERVED_43 = 43,
                NK_MEMORY_AUDIO_RESERVED_44 = 44,
                NK_MEMORY_AUDIO_RESERVED_45 = 45,
                NK_MEMORY_AUDIO_RESERVED_46 = 46,
                NK_MEMORY_AUDIO_RESERVED_47 = 47,
                NK_MEMORY_AUDIO_RESERVED_48 = 48,
                NK_MEMORY_AUDIO_RESERVED_49 = 49,

                // =================================================================
                // Streaming & Network (50-59)
                // =================================================================
                /** @brief Cache de streaming : assets chargés à la demande */
                NK_MEMORY_STREAMING   = 50,
                
                /** @brief Buffers réseau : packets, serialization */
                NK_MEMORY_NETWORK     = 51,
                
                // Réservé pour extensions futures
                NK_MEMORY_IO_RESERVED_52 = 52,
                NK_MEMORY_IO_RESERVED_53 = 53,
                NK_MEMORY_IO_RESERVED_54 = 54,
                NK_MEMORY_IO_RESERVED_55 = 55,
                NK_MEMORY_IO_RESERVED_56 = 56,
                NK_MEMORY_IO_RESERVED_57 = 57,
                NK_MEMORY_IO_RESERVED_58 = 58,
                NK_MEMORY_IO_RESERVED_59 = 59,

                // =================================================================
                // Debug & Profiling (250-255)
                // =================================================================
                /** @brief Allocations de debugging : non comptées dans les budgets */
                NK_MEMORY_DEBUG       = 255,
            };

            // -------------------------------------------------------------------------
            // SECTION 4 : STRUCTURE DE STATISTIQUES PAR TAG
            // -------------------------------------------------------------------------
            /**
             * @struct NkMemoryTagStats
             * @brief Statistiques mémoire pour une catégorie donnée
             * @ingroup MemoryTagging
             * 
             * Toutes les valeurs sont en bytes sauf indication contraire.
             * Les statistiques sont mises à jour atomiquement pour thread-safety.
             */
            struct NKENTSEU_MEMORY_CLASS_EXPORT NkMemoryTagStats {
                /** @brief Total d'octets actuellement alloués pour ce tag */
                nk_uint64 totalAllocated;
                
                /** @brief Pic d'allocation enregistré depuis le début */
                nk_uint64 peakAllocated;
                
                /** @brief Nombre total d'allocations (pas de deallocations) */
                nk_uint64 allocationCount;
                
                /** @brief Taille moyenne des allocations (en bytes, float pour précision) */
                float32   averageSize;
                
                /** @brief Ratio de fragmentation [0.0 - 1.0] : 0 = parfait, 1 = très fragmenté */
                float32   fragmentation;

                // =================================================================
                // Constructeurs et helpers
                // =================================================================
                
                /** @brief Constructeur par défaut : toutes valeurs à zéro */
                constexpr NkMemoryTagStats() noexcept 
                    : totalAllocated(0), peakAllocated(0), allocationCount(0)
                    , averageSize(0.0f), fragmentation(0.0f) {}

                /**
                 * @brief Calcule la taille moyenne à partir du total et du count
                 * @note Met à jour le champ averageSize en place
                 * @return Référence vers this pour chaining
                 */
                NkMemoryTagStats& ComputeAverage() noexcept {
                    if (allocationCount > 0) {
                        averageSize = static_cast<float32>(totalAllocated) / 
                                      static_cast<float32>(allocationCount);
                    }
                    return *this;
                }

                /**
                 * @brief Estime la fragmentation (simplifié : ratio free/used)
                 * @note Implémentation réelle nécessite tracking des blocs libres
                 * @param freeBytes Estimation des bytes libres mais non réutilisables
                 * @return Référence vers this pour chaining
                 */
                NkMemoryTagStats& EstimateFragmentation(nk_uint64 freeBytes) noexcept {
                    const nk_uint64 total = totalAllocated + freeBytes;
                    if (total > 0) {
                        fragmentation = static_cast<float32>(freeBytes) / 
                                       static_cast<float32>(total);
                    }
                    return *this;
                }
            };

            // -------------------------------------------------------------------------
            // SECTION 5 : CLASSE PRINCIPALE NkMemoryBudget
            // -------------------------------------------------------------------------
            /**
             * @class NkMemoryBudget
             * @brief Gestionnaire de budgets mémoire par catégorie
             * @ingroup MemoryTagging
             * 
             * Fonctionnalités :
             *  - Définition de limites mémoire par NkMemoryTag
             *  - Tracking en temps réel de l'utilisation
             *  - Détection de dépassement avec alertes configurables
             *  - Statistiques agrégées pour profiling
             *  - Dump formaté pour logs et outils externes
             * 
             * @note Thread-safe : toutes les méthodes peuvent être appelées depuis n'importe quel thread
             * @note Overhead minimal : atomiques légers, pas de locks globaux
             * @note En release avec NKENTSEU_DISABLE_MEMORY_TAGGING : fonctions inline no-op
             */
            class NKENTSEU_MEMORY_CLASS_EXPORT NkMemoryBudget {
                public:
                    // =================================================================
                    // @name Configuration des Budgets
                    // =================================================================
                    
                    /**
                     * @brief Définit le budget maximal pour une catégorie mémoire
                     * @param tag Catégorie à configurer
                     * @param bytes Limite en bytes (0 = illimité)
                     * @note Thread-safe : mise à jour atomique
                     * @note Les allocations existantes ne sont pas affectées rétroactivement
                     * 
                     * @example
                     * @code
                     * // Limiter les textures à 512 MB
                     * NkMemoryBudget::SetBudget(NkMemoryTag::NK_MEMORY_TEXTURE, 512 * 1024 * 1024);
                     * @endcode
                     */
                    static 
                    void SetBudget(NkMemoryTag tag, nk_uint64 bytes) noexcept;

                    /**
                     * @brief Obtient le budget configuré pour une catégorie
                     * @param tag Catégorie à interroger
                     * @return Limite en bytes (0 = illimité)
                     */
                    [[nodiscard]] static 
                    nk_uint64 GetBudget(NkMemoryTag tag) noexcept;

                    // =================================================================
                    // @name Tracking en Temps Réel
                    // =================================================================
                    
                    /**
                     * @brief Notifie une allocation au système de tagging
                     * @param tag Catégorie de l'allocation
                     * @param bytes Taille allouée en bytes
                     * @note Thread-safe : mise à jour atomique des compteurs
                     * @note Appelé automatiquement par les allocateurs taggés
                     * 
                     * @internal Utilisé par les wrappers d'allocation comme TaggedAlloc()
                     */
                    static 
                    void OnAllocate(NkMemoryTag tag, nk_uint64 bytes) noexcept;

                    /**
                     * @brief Notifie une deallocation au système de tagging
                     * @param tag Catégorie de la deallocation
                     * @param bytes Taille libérée en bytes
                     * @note Thread-safe : mise à jour atomique des compteurs
                     */
                    static 
                    void OnDeallocate(NkMemoryTag tag, nk_uint64 bytes) noexcept;

                    /**
                     * @brief Calcule le budget restant pour une catégorie
                     * @param tag Catégorie à interroger
                     * @return Bytes restants avant dépassement (négatif si over budget)
                     * @note Retourne LLONG_MAX si le budget est illimité (0)
                     */
                    [[nodiscard]] static 
                    nk_int64 GetBudgetRemaining(NkMemoryTag tag) noexcept;

                    /**
                     * @brief Teste si une catégorie a dépassé son budget
                     * @param tag Catégorie à vérifier
                     * @return true si l'utilisation actuelle > budget configuré
                     * @note Retourne false si le budget est illimité (0)
                     */
                    [[nodiscard]] static 
                    nk_bool IsOverBudget(NkMemoryTag tag) noexcept;

                    // =================================================================
                    // @name Statistiques et Profiling
                    // =================================================================
                    
                    /**
                     * @brief Obtient les statistiques complètes pour une catégorie
                     * @param tag Catégorie à interroger
                     * @return Structure NkMemoryTagStats avec toutes les métriques
                     * @note Snapshot thread-safe : les valeurs peuvent changer juste après l'appel
                     */
                    [[nodiscard]] static 
                    NkMemoryTagStats GetStats(NkMemoryTag tag) noexcept;

                    /**
                     * @brief Dump formaté de toutes les statistiques mémoire
                     * @note Output vers NK_FOUNDATION_LOG_INFO par défaut
                     * @note Format : tableau aligné avec totaux et pourcentages
                     * 
                     * @example output :
                     * @code
                     * [NKMemory] Tag Budget Report:
                     * | Tag          | Used      | Budget    | % Used | Peak      |
                     * |--------------|-----------|-----------|--------|-----------|
                     * | ENGINE       | 12.5 MB   | 64.0 MB   |  19.5% | 15.2 MB   |
                     * | TEXTURE      | 480.1 MB  | 512.0 MB  |  93.8% | 510.3 MB ⚠️|
                     * | PHYSICS      | 45.2 MB   | 128.0 MB  |  35.3% | 67.8 MB   |
                     * |--------------|-----------|-----------|--------|-----------|
                     * | TOTAL        | 537.8 MB  | 704.0 MB  |  76.4% | 593.3 MB  |
                     * @endcode
                     */
                    static 
                    void DumpStats() noexcept;

                    /**
                     * @brief Reset toutes les statistiques (pour benchmarks isolés)
                     * @note Ne modifie pas les budgets configurés, uniquement les compteurs
                     * @note Thread-safe mais à utiliser avec précaution en production
                     */
                    static 
                    void ResetStats() noexcept;

                    // =================================================================
                    // @name Configuration Avancée
                    // =================================================================
                    
                    /**
                     * @brief Active/désactive les alertes de dépassement de budget
                     * @param enabled true pour logger des warnings sur dépassement
                     * @note Par défaut : enabled = true en debug, false en release
                     */
                    static 
                    void SetBudgetAlertsEnabled(nk_bool enabled) noexcept;

                    /**
                     * @brief Définit le seuil d'alerte préventive (% du budget)
                     * @param threshold Pourcentage [0.0 - 1.0] pour alerte "bientôt plein"
                     * @example 0.9f = alerte à 90% du budget utilisé
                     */
                    static 
                    void SetBudgetWarningThreshold(float32 threshold) noexcept;

                private:
                    // Implémentation interne dans NkTag.cpp
                    // Pas de membres publics pour encapsulation
            };

            // -------------------------------------------------------------------------
            // SECTION 6 : HELPERS D'ALLOCATION TAGGÉE (Optionnels)
            // -------------------------------------------------------------------------
            /**
             * @defgroup MemoryTaggedAlloc Helpers d'Allocation avec Tagging
             * @brief Wrappers pratiques pour allouer avec tagging automatique
             * 
             * Ces fonctions combinent allocation et tagging en un seul appel.
             * En mode release avec NKENTSEU_DISABLE_MEMORY_TAGGING, elles 
             * se réduisent à des appels directs à l'allocateur (zero overhead).
             */

            /**
             * @brief Alloue de la mémoire avec tagging automatique
             * @param bytes Taille à allouer en bytes
             * @param tag Catégorie mémoire pour le tracking
             * @param allocator Allocateur à utiliser (nullptr = défaut)
             * @param alignment Alignement requis (défaut: alignof(std::max_align_t))
             * @return Pointeur vers la mémoire allouée, ou nullptr en cas d'échec
             * @ingroup MemoryTaggedAlloc
             * 
             * @note Appelle automatiquement NkMemoryBudget::OnAllocate() après succès
             * @note Thread-safe : tagging et allocation sont sérialisés si nécessaire
             * 
             * @example
             * @code
             * // Allouer une texture avec tagging
             * void* texData = TaggedAlloc(1024*1024, NkMemoryTag::NK_MEMORY_TEXTURE);
             * if (texData) {
             *     // ... upload vers GPU ...
             *     TaggedFree(texData, NkMemoryTag::NK_MEMORY_TEXTURE);
             * }
             * @endcode
             */
            NKENTSEU_MEMORY_API 
            void* TaggedAlloc(nk_size bytes, NkMemoryTag tag, 
                             NkAllocator* allocator = nullptr,
                             nk_size alignment = NK_MEMORY_DEFAULT_ALIGNMENT);

            /**
             * @brief Libère de la mémoire taggée avec notification automatique
             * @param ptr Pointeur à libérer (retourné par TaggedAlloc)
             * @param tag Catégorie mémoire correspondante
             * @param allocator Allocateur utilisé pour l'allocation (nullptr = défaut)
             * @ingroup MemoryTaggedAlloc
             * 
             * @note Appelle automatiquement NkMemoryBudget::OnDeallocate() avant libération
             * @note Safe : nullptr est accepté (no-op)
             */
            NKENTSEU_MEMORY_API 
            void TaggedFree(void* ptr, NkMemoryTag tag, 
                           NkAllocator* allocator = nullptr);

            /**
             * @brief Alloue et zero-initialise avec tagging
             * @param count Nombre d'éléments
             * @param elementSize Taille de chaque élément
             * @param tag Catégorie mémoire
             * @param allocator Allocateur à utiliser
             * @param alignment Alignement requis
             * @return Pointeur vers mémoire zero-initialisée, ou nullptr
             * @ingroup MemoryTaggedAlloc
             */
            NKENTSEU_MEMORY_API 
            void* TaggedCalloc(nk_size count, nk_size elementSize, NkMemoryTag tag,
                              NkAllocator* allocator = nullptr,
                              nk_size alignment = NK_MEMORY_DEFAULT_ALIGNMENT);

        } // namespace memory
    } // namespace nkentseu

#endif // NKENTSEU_MEMORY_NKTAG_H

// =============================================================================
// NOTES DE CONFIGURATION
// =============================================================================
/*
    [Désactiver le tagging en release pour performance]
    
    Dans votre configuration de build release, définir :
    
        #define NKENTSEU_DISABLE_MEMORY_TAGGING
    
    Effets :
    - NkMemoryBudget::OnAllocate/OnDeallocate deviennent des no-ops inline
    - TaggedAlloc/TaggedFree se réduisent à NkAlloc/NkFree directs
    - Overhead CPU : ~0%, Overhead mémoire : 0 bytes
    - Les budgets et stats ne sont pas trackés (GetStats retourne zéros)
    
    [Activer le tracking détaillé en debug]
    
    En debug, définir en plus :
    
        #define NKENTSEU_MEMORY_TAGGING_VERBOSE
    
    Effets :
    - Log chaque allocation/deallocation avec tag et callstack (si disponible)
    - Dump automatique des stats à la fermeture de l'application
    - Assertions sur dépassement de budget (abort en debug)
    
    [Intégration avec les allocateurs existants]
    
    Pour tagger automatiquement les allocations d'un allocateur personnalisé :
    
        class MyTaggedAllocator : public NkAllocator {
        public:
            MyTaggedAllocator(NkMemoryTag tag) : mTag(tag) {}
            
            Pointer Allocate(SizeType size, AlignType align) override {
                void* ptr = mBacking->Allocate(size, align);
                if (ptr) {
                    NkMemoryBudget::OnAllocate(mTag, size);  // Notification
                }
                return ptr;
            }
            
            void Deallocate(Pointer ptr) override {
                if (ptr) {
                    // Note: besoin de tracker la taille par ptr pour OnDeallocate
                    // Voir NkTaggedAllocator wrapper pour implémentation complète
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
    // Exemple 1 : Gestion de budget avec fallback
    void LoadAssetsWithBudget() {
        constexpr nk_uint64 TEXTURE_BUDGET = 512 * 1024 * 1024;  // 512 MB
        NkMemoryBudget::SetBudget(NkMemoryTag::NK_MEMORY_TEXTURE, TEXTURE_BUDGET);
        
        for (const auto& asset : pendingTextures) {
            if (NkMemoryBudget::IsOverBudget(NkMemoryTag::NK_MEMORY_TEXTURE)) {
                NK_FOUNDATION_LOG_WARN("Texture budget exceeded, skipping %s", asset.name);
                continue;  // Skip pour respecter le budget
            }
            
            void* data = TaggedAlloc(asset.size, NkMemoryTag::NK_MEMORY_TEXTURE);
            if (data) {
                LoadTexture(asset, data);
            }
        }
    }
    
    // Exemple 2 : Profiling par frame
    void EndFrameProfiling() {
        static nk_uint64 lastFrame = 0;
        
        auto stats = NkMemoryBudget::GetStats(NkMemoryTag::NK_MEMORY_RENDER);
        nk_int64 delta = static_cast<nk_int64>(stats.totalAllocated) - static_cast<nk_int64>(lastFrame);
        
        if (delta > 0) {
            NK_FOUNDATION_LOG_DEBUG("Render memory grew by %lld bytes this frame", delta);
        }
        
        lastFrame = stats.totalAllocated;
        
        // Dump toutes les 60 frames
        static int frameCount = 0;
        if (++frameCount >= 60) {
            NkMemoryBudget::DumpStats();
            frameCount = 0;
        }
    }
    
    // Exemple 3 : Alertes configurables
    void ConfigureMemoryAlerts() {
        // Activer les warnings
        NkMemoryBudget::SetBudgetAlertsEnabled(true);
        
        // Alerte préventive à 85% du budget
        NkMemoryBudget::SetBudgetWarningThreshold(0.85f);
        
        // Maintenant, quand un tag atteint 85% de son budget, 
        // un warning sera loggé automatiquement dans OnAllocate
    }
    
    // Exemple 4 : Reset pour benchmark isolé
    void RunMemoryBenchmark() {
        // Setup
        NkMemoryBudget::SetBudget(NkMemoryTag::NK_MEMORY_GAME, 100 * 1024 * 1024);
        NkMemoryBudget::ResetStats();  // Partir de zéro
        
        // Code à benchmarker
        for (int i = 0; i < 1000; ++i) {
            void* ptr = TaggedAlloc(1024, NkMemoryTag::NK_MEMORY_GAME);
            // ... utilisation ...
            TaggedFree(ptr, NkMemoryTag::NK_MEMORY_GAME);
        }
        
        // Résultats
        auto stats = NkMemoryBudget::GetStats(NkMemoryTag::NK_MEMORY_GAME);
        NK_FOUNDATION_LOG_INFO("Benchmark: peak=%llu MB, avg=%.1f bytes", 
                              stats.peakAllocated / (1024*1024), 
                              stats.averageSize);
    }
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================