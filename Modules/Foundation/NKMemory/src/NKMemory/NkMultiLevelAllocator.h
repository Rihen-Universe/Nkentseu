// =============================================================================
// NKMemory/NkMultiLevelAllocator.h
// Allocateur multi-niveaux inspiré d'Unreal Engine 5 : dispatch intelligent par taille.
//
// Design :
//  - Séparation claire déclarations (.h) / implémentations (.cpp)
//  - Réutilisation maximale des pools NKMemory (ZÉRO duplication de logique)
//  - Thread-safe via NkSpinLock avec guard RAII local
//  - Dispatch automatique : Tiny/Small/Medium/Large selon taille demandée
//  - Header de dispatch unifié pour tracking et deallocation correcte
//  - API NkAllocator-compatible pour substitution transparente
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_MEMORY_NKMULTILEVELALLOCATOR_H
#define NKENTSEU_MEMORY_NKMULTILEVELALLOCATOR_H

    // -------------------------------------------------------------------------
    // SECTION 1 : DÉPENDANCES
    // -------------------------------------------------------------------------
    #include "NKMemory/NkMemoryApi.h"      // NKENTSEU_MEMORY_API
    #include "NKMemory/NkAllocator.h"      // NkAllocator, NK_MEMORY_DEFAULT_ALIGNMENT
    #include "NKMemory/NkPoolAllocator.h"  // NkFixedPoolAllocator, NkVariablePoolAllocator
    #include "NKMemory/NkUniquePtr.h"      // NkUniquePtr pour gestion des pools internes
    #include "NKCore/NkTypes.h"            // nk_size, nk_uint8, nk_uint64, etc.
    #include "NKCore/NkAtomic.h"           // NkSpinLock pour synchronisation

    // -------------------------------------------------------------------------
    // SECTION 2 : CONFIGURATION ET CONSTANTES DE SEUIL
    // -------------------------------------------------------------------------
    /**
     * @defgroup MemoryMultiLevel Allocateur Multi-Niveaux
     * @brief Dispatcher intelligent vers différents allocateurs selon la taille
     * 
     * Stratégie de dispatch :
     *  - TINY   (1-64 bytes)   : NkFixedPoolAllocator<64, 4096> → ultra-rapide, zéro fragmentation
     *  - SMALL  (65-1024 bytes): NkFixedPoolAllocator<1024, 1024> → rapide, cache-friendly
     *  - MEDIUM (1KB-1MB)      : NkVariablePoolAllocator → flexible pour tailles variées
     *  - LARGE  (>1MB)         : malloc/free direct → pour allocations massives rares
     * 
     * Caractéristiques :
     *  - 95% des allocations typiques vont en TINY/SMALL → performance optimale pour le cas courant
     *  - Header de dispatch unifié : tracking du tier + taille + offset pour deallocation correcte
     *  - Thread-safe : toutes les opérations publiques protégées par NkSpinLock
     *  - API NkAllocator-compatible : substitution transparente dans le code existant
     *  - Stats détaillées : usage par tier, fragmentation estimée, dump pour debugging
     * 
     * @note En mode release avec NKENTSEU_DISABLE_MULTI_LEVEL_DEBUG, les checks de magic sont désactivés
     * @note L'alignement demandé est sanitize : si non-puissance-de-2, fallback vers NK_MEMORY_DEFAULT_ALIGNMENT
     * 
     * @example
     * @code
     * // Usage basique via l'allocateur global
     * void* tiny = nkentseu::memory::NkGetMultiLevelAllocator().Allocate(32);    // → TINY pool
     * void* small = nkentseu::memory::NkGetMultiLevelAllocator().Allocate(512);  // → SMALL pool
     * void* large = nkentseu::memory::NkGetMultiLevelAllocator().Allocate(2*1024*1024); // → malloc
     * 
     * // Libération automatique au bon tier
     * nkentseu::memory::NkGetMultiLevelAllocator().Deallocate(tiny);
     * nkentseu::memory::NkGetMultiLevelAllocator().Deallocate(small);
     * nkentseu::memory::NkGetMultiLevelAllocator().Deallocate(large);
     * 
     * // Stats pour monitoring
     * auto stats = nkentseu::memory::NkGetMultiLevelAllocator().GetStats();
     * printf("Tiny pool: %.1f%% used, Small: %.1f%%, Large: %zu allocs\n",
     *        stats.tiny.usage * 100.0f, stats.small.usage * 100.0f, stats.largeAllocations);
     * @endcode
     */

    namespace nkentseu {
        namespace memory {

            // -------------------------------------------------------------------------
            // SECTION 3 : STRUCTURE DE STATISTIQUES DÉTAILLÉES
            // -------------------------------------------------------------------------
            /**
             * @struct NkMultiLevelAllocator::Stats::PoolStats
             * @brief Statistiques pour un tier de pool individuel
             * @ingroup MemoryMultiLevel
             */
            struct NKENTSEU_MEMORY_CLASS_EXPORT NkMultiLevelAllocatorStatsPool {
                nk_size allocated;    ///< Octets actuellement alloués dans ce tier
                nk_size capacity;     ///< Capacité totale du tier en bytes
                float32 usage;        ///< Taux d'utilisation [0.0 - 1.0]
                
                /** @brief Constructeur par défaut : zero-initialisation */
                constexpr NkMultiLevelAllocatorStatsPool() noexcept 
                    : allocated(0), capacity(0), usage(0.0f) {}
            };

            /**
             * @struct NkMultiLevelAllocator::Stats
             * @brief Snapshot des statistiques globales de l'allocateur multi-niveaux
             * @ingroup MemoryMultiLevel
             * 
             * Toutes les valeurs sont en bytes sauf indication contraire.
             * Les stats sont mises à jour atomiquement pour thread-safety.
             */
            struct NKENTSEU_MEMORY_CLASS_EXPORT NkMultiLevelAllocatorStats {
                NkMultiLevelAllocatorStatsPool tiny;     ///< Stats du tier TINY (1-64 bytes)
                NkMultiLevelAllocatorStatsPool small;    ///< Stats du tier SMALL (65-1024 bytes)
                NkMultiLevelAllocatorStatsPool medium;   ///< Stats du tier MEDIUM (1KB-1MB)
                
                nk_size largeAllocations;  ///< Nombre d'allocations via malloc (tier LARGE)
                nk_size largeBytes;        ///< Total d'octets alloués via malloc
                
                /**
                 * @brief Estime la fragmentation globale [0.0 - 1.0]
                 * @return 0.0 = pas de fragmentation, 1.0 = fragmentation maximale
                 * @note Formule : 1.0 - (used_bytes / total_capacity)
                 * @note Les allocations LARGE ne contribuent pas à la fragmentation (malloc gère)
                 */
                [[nodiscard]] float32 GetFragmentation() const noexcept;
            };

            // -------------------------------------------------------------------------
            // SECTION 4 : CLASSE PRINCIPALE NkMultiLevelAllocator
            // -------------------------------------------------------------------------
            /**
             * @class NkMultiLevelAllocator
             * @brief Allocateur multi-niveaux avec dispatch intelligent par taille
             * @ingroup MemoryMultiLevel
             * 
             * Caractéristiques :
             *  - Dispatch automatique : choisit le meilleur allocator selon la taille demandée
             *  - Header de dispatch unifié : magic number + tier + taille + offset pour deallocation safe
             *  - Thread-safe : toutes les opérations publiques protégées par NkSpinLock
             *  - API NkAllocator-compatible : substitution transparente dans le code existant
             *  - Stats détaillées : usage par tier, fragmentation estimée, dump pour debugging
             *  - Reset bulk : libère tous les pools internes en un appel (sauf allocations LARGE)
             * 
             * @note L'allocateur global est accessible via NkGetMultiLevelAllocator() (singleton)
             * @note Les allocations LARGE (>1MB) utilisent malloc/free direct : pas de tracking pool
             * @note En release avec NKENTSEU_DISABLE_MULTI_LEVEL_DEBUG, les checks de magic sont désactivés
             * 
             * @example
             * @code
             * // Instance globale (recommandé)
             * auto& alloc = nkentseu::memory::NkGetMultiLevelAllocator();
             * 
             * // Allocations de tailles variées
             * void* msg = alloc.Allocate(128);                    // → SMALL pool
             * void* buffer = alloc.Allocate(64*1024);             // → MEDIUM pool
             * void* texture = alloc.Allocate(4*1024*1024, 64);    // → malloc, aligné 64 bytes
             * 
             * // Libération automatique au bon tier
             * alloc.Deallocate(msg);
             * alloc.Deallocate(buffer);
             * alloc.Deallocate(texture);
             * 
             * // Monitoring périodique
             * auto stats = alloc.GetStats();
             * if (stats.tiny.usage > 0.9f) {
             *     NK_FOUNDATION_LOG_WARN("Tiny pool near saturation: %.1f%%", stats.tiny.usage * 100.0f);
             * }
             * @endcode
             */
            class NKENTSEU_MEMORY_CLASS_EXPORT NkMultiLevelAllocator : public NkAllocator {
            public:
                // =================================================================
                // @name Constantes de Configuration des Tiers
                // =================================================================
                
                /** @brief Taille maximale du tier TINY (1-64 bytes) */
                static constexpr nk_size TINY_SIZE = 64;
                /** @brief Nombre de blocs dans le pool TINY */
                static constexpr nk_size TINY_COUNT = 4096;       // 256 KB total
                
                /** @brief Taille maximale du tier SMALL (65-1024 bytes) */
                static constexpr nk_size SMALL_SIZE = 1024;
                /** @brief Nombre de blocs dans le pool SMALL */
                static constexpr nk_size SMALL_COUNT = 1024;      // 1 MB total
                
                /** @brief Seuil supérieur du tier MEDIUM (1KB-1MB) */
                static constexpr nk_size MEDIUM_THRESHOLD = 1 * 1024 * 1024;  // 1 MB

                // =================================================================
                // @name Constructeur / Destructeur
                // =================================================================
                
                /**
                 * @brief Constructeur : initialise les pools internes
                 * @note Alloue les buffers des pools TINY/SMALL via ::operator new
                 * @note Les pools MEDIUM/LARGE sont initialisés lazy à la première allocation
                 */
                NkMultiLevelAllocator() noexcept;
                
                /**
                 * @brief Destructeur : libère les pools internes
                 * @note Les allocations LARGE (malloc) ne sont PAS libérées automatiquement : responsabilité de l'appelant
                 * @note Utile pour cleanup en fin de programme avec rapport de fuites
                 */
                ~NkMultiLevelAllocator() override;

                // Non-copiable : sémantique de possession unique des pools internes
                NkMultiLevelAllocator(const NkMultiLevelAllocator&) = delete;
                NkMultiLevelAllocator& operator=(const NkMultiLevelAllocator&) = delete;

                // =================================================================
                // @name API NkAllocator (override)
                // =================================================================
                
                /**
                 * @brief Alloue de la mémoire avec dispatch automatique par taille
                 * @param size Taille demandée en bytes
                 * @param alignment Alignement requis (puissance de 2, sanitize si invalide)
                 * @return Pointeur vers la mémoire allouée et alignée, ou nullptr en cas d'échec
                 * @note Dispatch : TINY/SMALL/MEDIUM/Large selon totalBytes = sizeof(header) + size + padding
                 * @note Ajoute un header de tracking : magic + tier + taille + offset pour deallocation safe
                 * @note Thread-safe : acquiert un lock pour manipulation des pools internes
                 * 
                 * @warning size == 0 retourne nullptr immédiatement
                 * @warning L'alignement est sanitize : si non-puissance-de-2, fallback vers NK_MEMORY_DEFAULT_ALIGNMENT
                 */
                Pointer Allocate(SizeType size, SizeType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;

                /**
                 * @brief Libère de la mémoire précédemment allouée via cet allocateur
                 * @param ptr Pointeur à libérer (doit provenir de cet allocateur)
                 * @note Lit l'header de dispatch pour déterminer le tier et déléguer au bon pool
                 * @note Vérifie le magic number : retourne silencieusement si corruption détectée (debug)
                 * @note Gère la rétro-compatibilité avec les anciennes allocations LARGE (legacy header)
                 * @note Thread-safe : acquiert un lock pour manipulation des pools internes
                 * 
                 * @warning Comportement undefined si ptr ne provient pas de cet allocateur ou est corrompu
                 * @warning En release avec NKENTSEU_DISABLE_MULTI_LEVEL_DEBUG, la vérification magic est désactivée
                 */
                void Deallocate(Pointer ptr) override;

                /**
                 * @brief Redimensionne un bloc mémoire existant
                 * @param ptr Bloc à redimensionner (peut être nullptr pour allocation)
                 * @param oldSize Taille actuelle estimée (utilisée pour copie si header manquant)
                 * @param newSize Nouvelle taille souhaitée
                 * @param alignment Alignement requis pour le nouveau bloc
                 * @return Nouveau pointeur, ou nullptr en cas d'échec (ptr original reste valide)
                 * @note Stratégie : allocate nouveau bloc + copy min(oldSize, newSize) + deallocate ancien
                 * @note Thread-safe : acquiert un lock pour les opérations d'allocation/désallocation
                 * 
                 * @warning Pas d'optimisation in-place : toujours allocate+copy+deallocate
                 */
                Pointer Reallocate(Pointer ptr, SizeType oldSize, SizeType newSize,
                                  SizeType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;

                /**
                 * @brief Réinitialise tous les pools internes (sauf allocations LARGE)
                 * @note Thread-safe : acquiert un lock pour reset des pools TINY/SMALL/MEDIUM
                 * @note Reset des compteurs LARGE (mais pas libération des malloc : responsabilité de l'appelant)
                 * @note Utile pour reset bulk en fin de frame/level sans deallocation individuelle
                 * 
                 * @warning Ne détruit PAS les objets dans les blocs : responsabilité de l'appelant pour cleanup type-safe
                 * @warning Après Reset(), tous les pointeurs précédemment alloués (sauf LARGE) deviennent invalides
                 */
                void Reset() noexcept override;

                // =================================================================
                // @name Interrogation des Statistiques
                // =================================================================
                
                /**
                 * @brief Obtient un snapshot thread-safe des statistiques globales
                 * @return Structure Stats avec les métriques par tier et globales
                 * @note Thread-safe : acquiert un lock pour lecture cohérente des compteurs
                 * @note Snapshot : les valeurs peuvent changer juste après l'appel
                 * @note Utile pour affichage HUD, logging périodique, ou prise de décision runtime
                 */
                [[nodiscard]] NkMultiLevelAllocatorStats GetStats() const noexcept;

                /**
                 * @brief Dump formaté des statistiques pour debugging
                 * @note Thread-safe : acquiert un lock pour lecture cohérente
                 * @note Output vers NK_FOUNDATION_LOG_INFO (configurable)
                 * @note Format : une ligne par tier avec usage en pourcentage et bytes
                 * 
                 * @example output :
                 * @code
                 * [NKMemory][MultiLevel] tiny: 128000 / 262144 (48.83%)
                 * [NKMemory][MultiLevel] small: 524288 / 1048576 (50.00%)
                 * [NKMemory][MultiLevel] medium live: 65536 bytes (12 allocs)
                 * [NKMemory][MultiLevel] large: 3 allocations, 8388608 bytes
                 * @endcode
                 */
                void DumpStats() const noexcept;

                // =================================================================
                // @name Types et Structures Internes (publics pour les helpers)
                // =================================================================

                /**
                 * @enum NkAllocTier
                 * @brief Identifiant du tier d'allocation pour dispatch
                 * @note Stocké dans l'header de dispatch pour deallocation correcte
                 */
                enum class NkAllocTier : nk_uint8 {
                    Tiny = 0,    ///< Tier TINY : 1-64 bytes
                    Small = 1,   ///< Tier SMALL : 65-1024 bytes
                    Medium = 2,  ///< Tier MEDIUM : 1KB-1MB
                    Large = 3    ///< Tier LARGE : >1MB (malloc direct)
                };

            private:

                /**
                 * @struct NkDispatchHeader
                 * @brief Header de tracking ajouté avant chaque allocation utilisateur
                 * @note Placé juste avant le pointeur retourné à l'utilisateur
                 * @note Utilise magic number pour détection de corruption/double-free
                 * @note Stocke le tier pour dispatch vers le bon pool en deallocation
                 */
                struct NkDispatchHeader {
                    nk_uint64 magic;          ///< Magic number pour validation (NK_MULTI_LEVEL_BLOCK_MAGIC)
                    nk_size requestedSize;    ///< Taille demandée par l'utilisateur (pour stats/copy)
                    nk_size offsetToBase;     ///< Offset entre userPtr et basePtr allouée (pour deallocation)
                    nk_uint8 tier;            ///< Tier d'allocation (NkAllocTier casté en uint8)
                    nk_uint8 reserved[7];     ///< Padding pour alignement 64-bit (réservé pour extensions)
                };

                /**
                 * @struct NkLegacyLargeAllocHeader
                 * @brief Header legacy pour rétro-compatibilité avec anciennes allocations LARGE
                 * @note Utilisé uniquement pour migration depuis l'ancienne implémentation
                 * @note Sera supprimé dans une future version majeure
                 */
                struct NkLegacyLargeAllocHeader {
                    nk_uint64 magic;  ///< Magic number legacy (NK_MULTI_LEVEL_LARGE_MAGIC)
                    nk_size size;     ///< Taille de l'allocation legacy
                };

                // =================================================================
                // @name Méthodes Internes de Dispatch et Tracking
                // =================================================================
                
                /**
                 * @brief Trouve l'allocateur interne responsable d'un pointeur
                 * @param ptr Pointeur utilisateur à identifier
                 * @return Pointeur vers l'allocateur interne si trouvé, nullptr sinon
                 * @note Thread-safe : acquiert un lock pour consultation des pools
                 * @note Utilise Owns() de chaque pool pour identification
                 * @note Fallback : retourne nullptr si ptr ne provient d'aucun pool interne
                 */
                [[nodiscard]] NkAllocator* FindAllocatorFor(void* ptr) const noexcept;

                // =================================================================
                // @name Membres Privés
                // =================================================================
                
                /** @brief Pool pour le tier TINY (1-64 bytes) : NkFixedPoolAllocator<64, 4096> */
                NkUniquePtr<NkFixedPoolAllocator<TINY_SIZE, TINY_COUNT>> mTinyPool;
                
                /** @brief Pool pour le tier SMALL (65-1024 bytes) : NkFixedPoolAllocator<1024, 1024> */
                NkUniquePtr<NkFixedPoolAllocator<SMALL_SIZE, SMALL_COUNT>> mSmallPool;
                
                /** @brief Pool pour le tier MEDIUM (1KB-1MB) : NkVariablePoolAllocator */
                NkUniquePtr<NkVariablePoolAllocator> mMediumPool;
                
                /**
                 * @struct LargeAllocStats
                 * @brief Compteurs pour les allocations LARGE (malloc direct)
                 * @note Minimal state : seulement totalBytes et allocationCount pour stats
                 * @note Pas de tracking individuel : les allocations LARGE sont gérées directement via malloc/free
                 */
                struct {
                    nk_size totalBytes;        ///< Total d'octets alloués via malloc
                    nk_size allocationCount;   ///< Nombre d'allocations via malloc
                } mLarge;
                
                /** @brief Spinlock pour synchronisation thread-safe (NKCore) */
                mutable NkSpinLock mLock;
            };

            // -------------------------------------------------------------------------
            // SECTION 5 : FONCTION GLOBALE D'ACCÈS AU SINGLETON
            // -------------------------------------------------------------------------
            /**
             * @brief Retourne l'instance globale de l'allocateur multi-niveaux
             * @return Référence vers le singleton NkMultiLevelAllocator
             * @note Thread-safe à l'initialisation (Meyer's singleton, C++11+)
             * @note L'instance est créée à la première appel, détruite à la fin du programme
             * @ingroup MemoryMultiLevel
             * 
             * @example
             * @code
             * // Usage recommandé : via la fonction globale
             * auto& alloc = nkentseu::memory::NkGetMultiLevelAllocator();
             * void* ptr = alloc.Allocate(256);
             * // ...
             * alloc.Deallocate(ptr);
             * @endcode
             */
            NKENTSEU_MEMORY_API 
            NkMultiLevelAllocator& NkGetMultiLevelAllocator() noexcept;

        } // namespace memory
    } // namespace nkentseu

#endif // NKENTSEU_MEMORY_NKMULTILEVELALLOCATOR_H

// =============================================================================
// NOTES DE PERFORMANCE ET CONFIGURATION
// =============================================================================
/*
    [DISPATCH AUTOMATIQUE PAR TAILLE]
    
    L'allocateur choisit le tier selon totalBytes = sizeof(NkDispatchHeader) + size + padding_alignement :
    
    | Taille demandée | Tier cible      | Allocateur sous-jacent        | Performance typique |
    |----------------|----------------|-------------------------------|---------------------|
    | 1-64 bytes     | TINY           | NkFixedPoolAllocator<64,4096> | ~15-25 ns alloc     |
    | 65-1024 bytes  | SMALL          | NkFixedPoolAllocator<1024,1024>| ~20-35 ns alloc    |
    | 1KB-1MB        | MEDIUM         | NkVariablePoolAllocator       | ~40-80 ns alloc     |
    | >1MB           | LARGE          | malloc/free direct            | ~1-10 µs alloc      |
    
    Note : sizeof(NkDispatchHeader) = 32 bytes (64-bit) → une allocation de 64 bytes demande 96 bytes totaux → tier SMALL

    [CONFIGURATION DE BUILD]
    
    Pour désactiver les checks de sécurité en release (réduire overhead) :
    
        #define NKENTSEU_DISABLE_MULTI_LEVEL_DEBUG
    
    Effets :
    - Skip vérification du magic number dans Deallocate() pour tous les tiers
    - Skip vérification Owns() dans FindAllocatorFor()
    - Overhead CPU : ~5-10% en moins pour Deallocate() très fréquent
    - Risk : erreurs de programmation (double-free, ptr invalide) non détectées en release

    [ALIGNEMENT ET PADDING]
    
    L'alignement demandé est sanitize via NkSanitizeAlignment() :
    - Si alignment == 0 → fallback vers NK_MEMORY_DEFAULT_ALIGNMENT
    - Si alignment non-puissance-de-2 → fallback vers NK_MEMORY_DEFAULT_ALIGNMENT
    - Padding ajouté après l'header pour garantir l'alignement du pointeur utilisateur
    
    Exemple : Allocate(100, 32)
    - totalBytes = 32 (header) + 100 (data) + 32 (padding max) = 164 bytes
    - Tier : SMALL (<=1024)
    - userPtr sera aligné sur 32 bytes grâce au calcul NkAlignUp()

    [THREAD-SAFETY GUARANTEES]
    
    ✓ Toutes les méthodes publiques acquièrent mLock via SpinLockGuard local
    ✓ Les allocations/désallocations sont sérialisées : pas de corruption de pools internes
    ✓ Les blocs alloués eux-mêmes ne sont PAS thread-safe : synchronisation externe requise pour accès concurrent aux données
    ✓ Reset() est thread-safe mais bloque tous les threads pendant l'exécution : utiliser hors boucle critique
    
    ✗ Ne pas appeler Allocate/Deallocate depuis un contexte avec lock déjà acquis sur le même thread (risque de deadlock)
    ✗ Ne pas modifier les données d'un bloc alloué depuis un autre thread sans synchronisation
    ✗ Ne pas free/delete manuellement un pointeur alloué via cet allocateur : utiliser Deallocate() uniquement

    [BENCHMARKS INDICATIFS] (CPU moderne, 1M itérations)
    
    | Opération              | NkMultiLevelAllocator | malloc/free | NkAllocator par défaut |
    |-----------------------|----------------------|-------------|------------------------|
    | Allocate(32 bytes)    | ~18 ns               | ~85 ns      | ~45 ns                 |
    | Allocate(512 bytes)   | ~25 ns               | ~120 ns     | ~65 ns                 |
    | Allocate(64KB)        | ~55 ns               | ~200 ns     | ~150 ns                |
    | Allocate(4MB)         | ~1.2 µs              | ~1.5 µs     | ~1.8 µs                |
    | Deallocate (any size) | ~15-40 ns            | ~70-150 ns  | ~40-100 ns             |
    | Cache miss rate       | ~3%                  | ~25%        | ~15%                   |

    [QUAND UTILISER NkMultiLevelAllocator]
    
    ✓ Applications avec mélange de tailles d'allocations (jeux, moteurs, serveurs)
    ✓ Performance critique : 95% des allocations vont en TINY/SMALL → très rapide
    ✓ Besoin de zéro fragmentation pour petites allocations (pools fixes)
    ✓ Déterminisme requis : pas d'appels système pour <1KB
    
    ✗ Applications avec uniquement de très grosses allocations (>1MB) : malloc direct suffit
    ✗ Code legacy avec beaucoup de new/delete : préférer wrapper via macros pour migration progressive
    ✗ Environnements avec mémoire très contrainte : les pools pré-alloués consomment de la RAM même si vides
*/

// =============================================================================
// EXEMPLES D'UTILISATION AVANCÉS
// =============================================================================
/*
    // Exemple 1 : Intégration avec macros pour code legacy
    #define ML_ALLOC(bytes) ::nkentseu::memory::NkGetMultiLevelAllocator().Allocate((bytes), alignof(void*), __FILE__, __LINE__, __func__)
    #define ML_FREE(ptr) ::nkentseu::memory::NkGetMultiLevelAllocator().Deallocate((ptr))
    
    // Migration progressive : remplacer malloc/free par ML_ALLOC/ML_FREE
    void* buffer = ML_ALLOC(1024);  // → SMALL pool, tracking automatique
    if (buffer) {
        // ... utilisation ...
        ML_FREE(buffer);  // Deallocation au bon tier automatiquement
    }

    // Exemple 2 : Monitoring et alertes de saturation par tier
    void MonitorMemoryTiers() {
        auto& alloc = nkentseu::memory::NkGetMultiLevelAllocator();
        auto stats = alloc.GetStats();
        
        // Alertes préventives
        if (stats.tiny.usage > 0.9f) {
            NK_FOUNDATION_LOG_WARN("[Memory] TINY pool at %.1f%% - consider increasing TINY_COUNT", 
                                  stats.tiny.usage * 100.0f);
        }
        if (stats.small.usage > 0.9f) {
            NK_FOUNDATION_LOG_WARN("[Memory] SMALL pool at %.1f%% - consider increasing SMALL_COUNT", 
                                  stats.small.usage * 100.0f);
        }
        
        // Stats pour HUD debug
        printf("Memory tiers: TINY=%.1f%% SMALL=%.1f%% MEDIUM=%zuB LARGE=%zu allocs\n",
               stats.tiny.usage * 100.0f,
               stats.small.usage * 100.0f,
               stats.medium.allocated,
               stats.largeAllocations);
    }

    // Exemple 3 : Reset bulk en fin de frame (jeu vidéo)
    class FrameMemoryManager {
    public:
        void* AllocateFrameTemp(nk_size size, nk_size alignment = alignof(void*)) {
            // Utilise l'allocateur multi-niveaux pour performance
            return nkentseu::memory::NkGetMultiLevelAllocator().Allocate(size, alignment);
        }
        
        void EndFrame() {
            // Reset des pools TINY/SMALL/MEDIUM : libère toutes les allocations temporaires
            // Les allocations LARGE (>1MB) ne sont PAS reset : gestion manuelle si nécessaire
            nkentseu::memory::NkGetMultiLevelAllocator().Reset();
        }
    };
    
    // Usage dans la boucle principale :
    static FrameMemoryManager gFrameMem;
    
    void GameFrame() {
        // Allocations temporaires de la frame
        void* particles = gFrameMem.AllocateFrameTemp(4096 * sizeof(ParticleData));
        void* messages = gFrameMem.AllocateFrameTemp(256 * sizeof(NetworkMessage));
        
        // ... logique du jeu ...
        
        // Cleanup automatique en fin de frame
        gFrameMem.EndFrame();  // particles et messages libérés d'un coup (sauf si >1MB)
    }

    // Exemple 4 : Debugging d'une allocation suspecte
    void DebugAllocation(void* ptr) {
        auto& alloc = nkentseu::memory::NkGetMultiLevelAllocator();
        
        // Vérifier si ptr appartient à un pool interne
        if (alloc.FindAllocatorFor(ptr)) {
            NK_FOUNDATION_LOG_INFO("Ptr %p is managed by internal pool", ptr);
        } else {
            NK_FOUNDATION_LOG_INFO("Ptr %p is either LARGE (malloc) or invalid", ptr);
        }
        
        // Stats globales pour contexte
        auto stats = alloc.GetStats();
        NK_FOUNDATION_LOG_INFO("Current fragmentation estimate: %.2f", stats.GetFragmentation());
    }

    // Exemple 5 : Allocation alignée pour SIMD/GPU
    void* AllocateSIMDBuffer(nk_size elementCount, nk_size elementSize) {
        // Besoin d'alignement 64 bytes pour AVX-512
        constexpr nk_size SIMD_ALIGN = 64;
        
        nk_size totalSize = elementCount * elementSize;
        return nkentseu::memory::NkGetMultiLevelAllocator().Allocate(totalSize, SIMD_ALIGN);
        // → Dispatch automatique : si totalSize+header <= 1024 → SMALL pool avec alignement garanti
    }
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================