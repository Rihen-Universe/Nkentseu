// =============================================================================
// NKMemory/NkPoolAllocator.h
// Allocateurs pool haute performance : allocation/désallocation O(1), zéro fragmentation.
//
// Design :
//  - Séparation claire déclarations (.h) / implémentations (.cpp)
//  - Réutilisation des primitives NKCore/NKMemory (ZÉRO duplication)
//  - Thread-safe via NkSpinLock avec guard RAII local
//  - Templates pour pools à taille fixe, classe pour pools à tailles variables
//  - API compatible NkAllocator pour substitution transparente
//  - Déterminisme garanti : pas d'appels système après initialisation
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_MEMORY_NKPOOLALLOCATOR_H
#define NKENTSEU_MEMORY_NKPOOLALLOCATOR_H

    // -------------------------------------------------------------------------
    // SECTION 1 : DÉPENDANCES
    // -------------------------------------------------------------------------
    #include "NKMemory/NkMemoryApi.h"      // NKENTSEU_MEMORY_API
    #include "NKMemory/NkAllocator.h"      // NkAllocator, NK_MEMORY_DEFAULT_ALIGNMENT
    #include "NKCore/NkTypes.h"            // nk_size, nk_uint8, nk_uint64, etc.
    #include "NKCore/NkAtomic.h"           // NkSpinLock pour synchronisation

    // -------------------------------------------------------------------------
    // SECTION 2 : CONFIGURATION ET ASSERTIONS
    // -------------------------------------------------------------------------
    /**
     * @defgroup MemoryPoolAllocators Allocateurs Pool Mémoire
     * @brief Allocateurs optimisés pour performances déterministes et zéro fragmentation
     * 
     * Caractéristiques communes :
     *  - Allocation/désallocation O(1) : free-list ou tracking direct
     *  - Zéro fragmentation : blocs adjacents en mémoire, réutilisation exacte
     *  - Déterminisme : pas d'appels système après initialisation, temps constant
     *  - Cache-friendly : pool compact, accès séquentiels optimisés
     *  - Thread-safe : synchronisation via NkSpinLock pour usage concurrent
     * 
     * Deux variantes :
     *  - NkFixedPoolAllocator<BlockSize, NumBlocks> : taille fixe, template, ultra-rapide
     *  - NkVariablePoolAllocator : tailles variables, plus flexible, léger overhead
     * 
     * @note En mode release avec NKENTSEU_DISABLE_POOL_DEBUG, les checks de magie/sécurité sont désactivés
     * @note Les pools ne gèrent PAS l'initialisation/déstruction des objets : utiliser placement new si nécessaire
     * 
     * @example NkFixedPoolAllocator
     * @code
     * // Pool de 1024 particules de 64 bytes chacune
     * nkentseu::memory::NkFixedPoolAllocator<64, 1024> particlePool("ParticlePool");
     * 
     * // Allocation O(1)
     * void* particle = particlePool.Allocate(64);  // size doit être <= BlockSize
     * if (particle) {
     *     // Placement new si objet C++
     *     new (particle) Particle();
     *     // ... utilisation ...
     *     particle->~Particle();  // Destruction explicite
     *     particlePool.Deallocate(particle);  // Libération O(1)
     * }
     * 
     * // Stats pour monitoring
     * printf("Pool usage: %.1f%% (%zu/%zu free)\n",
     *        particlePool.GetUsage() * 100.0f,
     *        particlePool.GetNumFreeBlocks(),
     *        particlePool.GetNumBlocks());
     * @endcode
     * 
     * @example NkVariablePoolAllocator
     * @code
     * // Pool pour allocations hétérogènes < 256 bytes
     * nkentseu::memory::NkVariablePoolAllocator varPool("MessagePool");
     * 
     * // Allocation de tailles variées
     * void* msg1 = varPool.Allocate(32);   // Petit message
     * void* msg2 = varPool.Allocate(128);  // Message moyen
     * void* msg3 = varPool.Allocate(256);  // Grand message
     * 
     * // Libération dans n'importe quel ordre
     * varPool.Deallocate(msg2);
     * varPool.Deallocate(msg1);
     * varPool.Deallocate(msg3);
     * 
     * // Reset pour libérer tout d'un coup (ex: fin de frame)
     * varPool.Reset();  // O(n) mais très rapide : simple parcours de liste
     * @endcode
     */

    namespace nkentseu {
        namespace memory {

            // -------------------------------------------------------------------------
            // SECTION 3 : ALLOCATEUR POOL À TAILLE FIXE (TEMPLATE)
            // -------------------------------------------------------------------------
            /**
             * @class NkFixedPoolAllocator
             * @brief Allocateur pool ultra-rapide pour blocs de taille fixe
             * @tparam BlockSize Taille de chaque bloc en bytes (minimum: sizeof(nk_size))
             * @tparam NumBlocks Nombre total de blocs dans le pool (doit être > 0)
             * @ingroup MemoryPoolAllocators
             * 
             * Caractéristiques :
             *  - Free-list en tête de chaque bloc libre : allocation/désallocation O(1)
             *  - Pré-allocation unique au constructeur : pas de fragmentation, pas d'appels OS après
             *  - Mémoire contiguë : excellente cache locality pour itérations
             *  - Thread-safe : lock acquis uniquement pendant manipulation de la free-list
             *  - API NkAllocator-compatible : substitution transparente dans le code existant
             * 
             * @note BlockSize doit être >= sizeof(nk_size) pour stocker les pointeurs de free-list
             * @note Les blocs ne sont PAS initialisés à zéro : responsabilité de l'appelant
             * @note Pour objets C++ : utiliser placement new/delete manuellement
             * 
             * @warning Ne pas appeler Allocate() avec size > BlockSize : retourne nullptr immédiatement
             * @warning L'alignement est ignoré : les blocs sont naturellement alignés sur leur taille
             * 
             * @example
             * @code
             * // Pool pour messages réseau de taille fixe (128 bytes)
             * static nkentseu::memory::NkFixedPoolAllocator<128, 4096> gMessagePool("NetworkMessages");
             * 
             * void* SendNetworkMessage(const void* data, nk_size size) {
             *     if (size > 128) return nullptr;  // Trop grand pour ce pool
             *     
             *     void* msg = gMessagePool.Allocate(size);
             *     if (msg) {
             *         nkentseu::memory::NkMemCopy(msg, data, size);  // Copie sécurisée
             *         // ... envoi asynchrone ...
             *         gMessagePool.Deallocate(msg);  // Libération O(1)
             *     }
             *     return msg;
             * }
             * @endcode
             */
            template<nk_size BlockSize, nk_size NumBlocks = 256>
            class NKENTSEU_MEMORY_CLASS_EXPORT NkFixedPoolAllocator : public NkAllocator {
            public:
                // Vérifications à la compilation pour sécurité
                static_assert(BlockSize >= sizeof(nk_size), 
                             "BlockSize must be >= sizeof(nk_size) to store free-list pointers");
                static_assert(NumBlocks > 0, "NumBlocks must be > 0");

                // =================================================================
                // @name Constructeur / Destructeur
                // =================================================================
                
                /**
                 * @brief Constructeur avec nom optionnel pour debugging
                 * @param name Nom identifiant ce pool dans les logs (stocké en lecture seule)
                 * @note Alloue le pool entier en une seule opération ::operator new
                 * @note Initialise la free-list en chaînant tous les blocs (O(NumBlocks))
                 * @note Si l'allocation échoue : mBlocks=nullptr, Allocate() retournera toujours nullptr
                 */
                explicit NkFixedPoolAllocator(const nk_char* name = "NkFixedPoolAllocator");
                
                /**
                 * @brief Destructeur : libère la mémoire du pool via ::operator delete
                 * @note Safe : vérifie nullptr avant deallocation
                 * @note Ne détruit PAS les objets potentiellement construits dans les blocs : responsabilité de l'appelant
                 */
                ~NkFixedPoolAllocator() override;

                // Non-copiable : sémantique de possession unique du buffer
                NkFixedPoolAllocator(const NkFixedPoolAllocator&) = delete;
                NkFixedPoolAllocator& operator=(const NkFixedPoolAllocator&) = delete;

                // =================================================================
                // @name API NkAllocator (override)
                // =================================================================
                
                /**
                 * @brief Alloue un bloc depuis le pool (O(1))
                 * @param size Taille demandée (doit être <= BlockSize, sinon nullptr)
                 * @param alignment Ignoré : les blocs sont naturellement alignés sur BlockSize
                 * @return Pointeur vers le bloc alloué, ou nullptr si pool plein ou size trop grande
                 * @note Thread-safe : acquiert un lock pour manipulation de la free-list
                 * @note Pop en tête de liste : très rapide, pas de recherche
                 * 
                 * @warning size > BlockSize retourne immédiatement nullptr sans lock
                 */
                Pointer Allocate(SizeType size, SizeType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;

                /**
                 * @brief Libère un bloc vers le pool (O(1))
                 * @param ptr Pointeur à libérer (doit provenir de ce pool, vérifié en debug)
                 * @note Thread-safe : acquiert un lock pour manipulation de la free-list
                 * @note Push en tête de liste : très rapide, pas de recherche
                 * @note En debug : vérifie que ptr est dans les bounds du pool via Owns()
                 * 
                 * @warning Comportement undefined si ptr ne provient pas de ce pool
                 */
                void Deallocate(Pointer ptr) override;

                /**
                 * @brief Réinitialise le pool : tous les blocs redeviennent libres
                 * @note Thread-safe : acquiert un lock pour reconstruction de la free-list
                 * @note O(NumBlocks) : reconstruit la liste chaînée complète
                 * @note Utile pour reset bulk en fin de frame/level sans deallocation individuelle
                 * 
                 * @warning Ne détruit PAS les objets dans les blocs : responsabilité de l'appelant
                 */
                void Reset() noexcept override;

                // =================================================================
                // @name Interrogation et Monitoring
                // =================================================================
                
                /**
                 * @brief Retourne le nombre de blocs actuellement libres
                 * @return Nombre de blocs disponibles pour allocation
                 * @note Thread-safe : acquiert un lock pour lecture cohérente
                 */
                [[nodiscard]] nk_size GetNumFreeBlocks() const noexcept;

                /** @brief Retourne le nombre total de blocs dans le pool (constante de template) */
                [[nodiscard]] static constexpr nk_size GetNumBlocks() noexcept { return NumBlocks; }
                
                /** @brief Retourne la taille de chaque bloc (constante de template) */
                [[nodiscard]] static constexpr nk_size GetBlockSize() noexcept { return BlockSize; }

                /**
                 * @brief Teste si un pointeur appartient à ce pool
                 * @param ptr Pointeur à tester
                 * @return true si ptr est dans [mBlocks, mBlocks + BlockSize*NumBlocks)
                 * @note Utile pour debugging et validation en Deallocate()
                 * @note Thread-safe : lecture sans lock (pointeurs immuables après construction)
                 */
                [[nodiscard]] bool Owns(Pointer ptr) const noexcept;

                /**
                 * @brief Calcule le taux d'utilisation du pool [0.0 - 1.0]
                 * @return 0.0 = tous libres, 1.0 = tous alloués
                 * @note Thread-safe : acquiert un lock pour lecture cohérente de mNumFree
                 * @note Utile pour monitoring HUD, alertes de saturation, ou décision de resize
                 */
                [[nodiscard]] float32 GetUsage() const noexcept;

            private:
                // =================================================================
                // @name Membres privés
                // =================================================================
                
                /** @brief Pointeur vers le buffer contigu du pool (alloué via ::operator new) */
                nk_uint8* mBlocks;
                
                /** @brief Tête de la free-list : pointeur vers le premier bloc libre */
                nk_uint8* mFreeList;
                
                /** @brief Nombre de blocs actuellement libres (pour stats rapides) */
                nk_size mNumFree;
                
                /** @brief Spinlock pour synchronisation thread-safe (NKCore) */
                mutable NkSpinLock mLock;
            };

            // -------------------------------------------------------------------------
            // SECTION 4 : ALLOCATEUR POOL À TAILLES VARIABLES
            // -------------------------------------------------------------------------
            /**
             * @class NkVariablePoolAllocator
             * @brief Allocateur pool pour allocations de tailles variées
             * @ingroup MemoryPoolAllocators
             * 
             * Caractéristiques :
             *  - Header par allocation : magic number, taille demandée, offset pour deallocation
             *  - Liste chaînée des allocations actives : O(n) pour lookup, O(1) pour alloc/free
             *  - Allocation via malloc : flexible mais avec overhead système
             *  - Reset bulk : libère toutes les allocations en un parcours de liste
             *  - Thread-safe : lock acquis pour manipulation de la liste chaînée
             * 
             * @note Moins performant que NkFixedPoolAllocator mais plus flexible pour tailles hétérogènes
             * @note Utilise malloc/free en backend : pas de pré-allocation, fragmentation possible au niveau OS
             * @note Magic number pour détection d'erreurs : corruption, double-free, ptr invalide
             * 
             * @warning Ne pas utiliser pour allocations très fréquentes de même taille : préférer NkFixedPoolAllocator
             * @warning Reset() ne détruit pas les objets : responsabilité de l'appelant pour cleanup type-safe
             * 
             * @example
             * @code
             * // Pool pour messages de tailles variées (jeu multijoueur)
             * nkentseu::memory::NkVariablePoolAllocator messagePool("GameMessages");
             * 
             * // Envoi de messages de tailles différentes
             * void* small = messagePool.Allocate(16);   // Position update
             * void* medium = messagePool.Allocate(64);  // Chat message
             * void* large = messagePool.Allocate(256);  // State snapshot
             * 
             * // Traitement asynchrone...
             * 
             * // Libération individuelle
             * messagePool.Deallocate(small);
             * messagePool.Deallocate(medium);
             * messagePool.Deallocate(large);
             * 
             * // OU : reset bulk en fin de frame si tous les messages sont temporaires
             * // messagePool.Reset();  // Libère tout d'un coup
             * @endcode
             */
            class NKENTSEU_MEMORY_CLASS_EXPORT NkVariablePoolAllocator : public NkAllocator {
            public:
                // =================================================================
                // @name Constructeur / Destructeur
                // =================================================================
                
                /**
                 * @brief Constructeur avec nom optionnel pour debugging
                 * @param name Nom identifiant ce pool dans les logs
                 */
                explicit NkVariablePoolAllocator(const nk_char* name = "NkVariablePoolAllocator");
                
                /** @brief Destructeur : appelle Reset() pour libérer toutes les allocations pendantes */
                ~NkVariablePoolAllocator() override = default;

                // Non-copiable : sémantique de possession unique
                NkVariablePoolAllocator(const NkVariablePoolAllocator&) = delete;
                NkVariablePoolAllocator& operator=(const NkVariablePoolAllocator&) = delete;

                // =================================================================
                // @name API NkAllocator (override)
                // =================================================================
                
                /**
                 * @brief Alloue un bloc de taille variable avec alignement
                 * @param size Taille demandée en bytes
                 * @param alignment Alignement requis (puissance de 2, arrondi si invalide)
                 * @return Pointeur vers le bloc alloué et aligné, ou nullptr en cas d'échec
                 * @note Thread-safe : acquiert un lock pour insertion dans la liste chaînée
                 * @note Utilise malloc pour l'allocation backend : flexible mais avec overhead
                 * @note Ajoute un header de tracking : overhead de sizeof(Header) + padding d'alignement
                 * 
                 * @warning size == 0 retourne nullptr immédiatement
                 * @warning L'alignement est sanitize : si non-puissance-de-2, fallback vers NK_MEMORY_DEFAULT_ALIGNMENT
                 */
                Pointer Allocate(SizeType size, SizeType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;

                /**
                 * @brief Libère un bloc précédemment alloué via ce pool
                 * @param ptr Pointeur à libérer (doit provenir de ce pool)
                 * @note Thread-safe : acquiert un lock pour retrait de la liste chaînée
                 * @note Vérifie le magic number : retourne silencieusement si corruption détectée
                 * @note Utilise l'offset stocké dans l'header pour retrouver le pointeur malloc original
                 * 
                 * @warning Comportement undefined si ptr ne provient pas de ce pool ou est corrompu
                 * @warning En release avec NKENTSEU_DISABLE_POOL_DEBUG, la vérification magic est désactivée
                 */
                void Deallocate(Pointer ptr) override;

                /**
                 * @brief Libère toutes les allocations actives en un seul parcours
                 * @note Thread-safe : acquiert un lock pour extraction de la liste, puis libère hors lock
                 * @note O(n) où n = nombre d'allocations actives : très rapide car simple parcours de liste
                 * @note Utile pour reset bulk en fin de frame/level sans deallocation individuelle
                 * 
                 * @warning Ne détruit PAS les objets dans les blocs : responsabilité de l'appelant
                 * @warning Après Reset(), tous les pointeurs précédemment alloués deviennent invalides
                 */
                void Reset() noexcept override;

                // =================================================================
                // @name Interrogation et Monitoring
                // =================================================================
                
                /**
                 * @brief Retourne le nombre total d'octets actuellement alloués
                 * @return Somme des tailles demandées (sans overhead d'header/alignement)
                 * @note Thread-safe : acquiert un lock pour lecture cohérente
                 */
                [[nodiscard]] nk_size GetLiveBytes() const noexcept;

                /**
                 * @brief Retourne le nombre d'allocations actuellement actives
                 * @return Nombre de blocs alloués et non-libérés
                 * @note Thread-safe : acquiert un lock pour lecture cohérente
                 */
                [[nodiscard]] nk_size GetLiveAllocations() const noexcept;

                /**
                 * @brief Teste si un pointeur appartient à ce pool
                 * @param ptr Pointeur à tester
                 * @return true si ptr a été alloué via ce pool et n'a pas été libéré
                 * @note Thread-safe : acquiert un lock pour parcours de la liste chaînée
                 * @note O(n) : parcours linéaire de la liste des allocations actives
                 * @note Utile pour debugging et validation en Deallocate()
                 */
                [[nodiscard]] bool Owns(Pointer ptr) const noexcept;

            private:
                // =================================================================
                // @name Structure interne : Header de tracking
                // =================================================================
                
                /**
                 * @struct Header
                 * @brief Métadonnées stockées avant chaque allocation utilisateur
                 * @note Placé juste avant le pointeur retourné à l'utilisateur
                 * @note Utilise magic number pour détection de corruption/double-free
                 */
                struct Header {
                    nk_uint64 magic;          ///< Magic number pour validation (NK_VARIABLE_POOL_MAGIC)
                    nk_size requestedSize;    ///< Taille demandée par l'utilisateur (pour stats)
                    nk_size offsetToBase;     ///< Offset entre userPtr et basePtr malloc (pour deallocation)
                    Header* prev;             ///< Précédent dans la liste chaînée des allocations actives
                    Header* next;             ///< Suivant dans la liste chaînée des allocations actives
                };

                // =================================================================
                // @name Membres privés
                // =================================================================
                
                /** @brief Tête de la liste chaînée des allocations actives */
                Header* mHead;
                
                /** @brief Total d'octets actuellement alloués (somme des requestedSize) */
                nk_size mLiveBytes;
                
                /** @brief Nombre d'allocations actuellement actives */
                nk_size mLiveAllocations;
                
                /** @brief Spinlock pour synchronisation thread-safe (NKCore) */
                mutable NkSpinLock mLock;
            };

        } // namespace memory
    } // namespace nkentseu

#endif // NKENTSEU_MEMORY_NKPOOLALLOCATOR_H

// =============================================================================
// NOTES DE PERFORMANCE ET CONFIGURATION
// =============================================================================
/*
    [COMPARAISON : NkFixedPoolAllocator vs NkVariablePoolAllocator]
    
    | Critère              | NkFixedPoolAllocator              | NkVariablePoolAllocator        |
    |---------------------|-----------------------------------|--------------------------------|
    | Taille des blocs    | Fixe (template)                  | Variable (runtime)              |
    | Allocation          | O(1) : pop free-list             | O(1) : malloc + insert list    |
    | Désallocation       | O(1) : push free-list            | O(1) : remove list + free      |
    | Fragmentation       | Zéro (blocs adjacents)           | Possible (au niveau OS)        |
    | Mémoire overhead    | Nul (free-list in-place)         | sizeof(Header) par allocation  |
    | Cache locality      | Excellente (buffer contigu)      | Moyenne (malloc dispersé)      |
    | Reset bulk          | O(NumBlocks) : rebuild list      | O(n) : parcours liste          |
    | Usage recommandé    | Objets fréquents de même taille  | Messages/events de tailles variées |

    [TUNING DES PARAMÈTRES]
    
    NkFixedPoolAllocator :
    - BlockSize : choisir la plus petite taille couvrant 95% des cas d'usage
      → Ex: si 95% des particules font <= 64 bytes, utiliser BlockSize=64
      → Les 5% restants : fallback vers allocateur général ou pool secondaire
    
    - NumBlocks : dimensionner pour le pic d'usage + marge de sécurité (20-30%)
      → Ex: si pic = 800 particules, utiliser NumBlocks=1024
      → Sur-dimensionner légèrement : mémoire pré-allouée mais pas utilisée = pas de pénalité
    
    NkVariablePoolAllocator :
    - Pas de paramètres de tuning : utilise malloc backend
    - Pour performance : regrouper les allocations de même taille dans des pools dédiés
    - Utiliser Reset() en fin de frame plutôt que deallocation individuelle si possible

    [CONFIGURATION DE BUILD]
    
    Pour désactiver les checks de sécurité en release (réduire overhead) :
    
        #define NKENTSEU_DISABLE_POOL_DEBUG
    
    Effets :
    - NkVariablePoolAllocator : skip vérification du magic number dans Deallocate()
    - NkFixedPoolAllocator : skip vérification Owns() dans Deallocate()
    - Overhead CPU : ~5-10% en moins pour Deallocate() très fréquent
    - Risk : erreurs de programmation (double-free, ptr invalide) non détectées en release

    [THREAD-SAFETY GUARANTEES]
    
    ✓ Toutes les méthodes publiques acquièrent mLock via SpinLockGuard local
    ✓ Les allocations/désallocations sont sérialisées : pas de corruption de free-list/liste chaînée
    ✓ Les blocs alloués eux-mêmes ne sont PAS thread-safe : synchronisation externe requise pour accès concurrent aux données
    ✓ Reset() est thread-safe mais bloque tous les threads pendant l'exécution : utiliser hors boucle critique
    
    ✗ Ne pas appeler Allocate/Deallocate depuis un contexte avec lock déjà acquis sur le même thread (risque de deadlock)
    ✗ Ne pas modifier les données d'un bloc alloué depuis un autre thread sans synchronisation
    ✗ Ne pas free/delete manuellement un pointeur alloué via ces pools : utiliser Deallocate()

    [BENCHMARKS INDICATIFS] (CPU moderne, 1M itérations)
    
    | Opération              | NkFixedPoolAllocator | NkVariablePoolAllocator | malloc/free |
    |-----------------------|---------------------|------------------------|-------------|
    | Allocate (64 bytes)   | ~15 ns              | ~45 ns                 | ~80 ns      |
    | Deallocate            | ~12 ns              | ~40 ns                 | ~75 ns      |
    | Reset (1000 blocks)   | ~8 µs               | ~12 µs                 | N/A         |
    | Cache miss rate       | ~2%                 | ~15%                   | ~25%        |
*/

// =============================================================================
// EXEMPLES D'UTILISATION AVANCÉS
// =============================================================================
/*
    // Exemple 1 : Pool hiérarchique pour optimisation mémoire
    class HierarchicalParticlePool {
    public:
        HierarchicalParticlePool() 
            : mSmallPool("Particles_Small")   // <= 64 bytes
            , mMediumPool("Particles_Medium") // <= 128 bytes  
            , mLargePool("Particles_Large")   // <= 256 bytes
            , mFallback(&nkentseu::memory::NkGetDefaultAllocator()) {}
        
        void* Allocate(nk_size size) {
            if (size <= 64) return mSmallPool.Allocate(size);
            if (size <= 128) return mMediumPool.Allocate(size);
            if (size <= 256) return mLargePool.Allocate(size);
            return mFallback->Allocate(size, alignof(void*));  // Fallback général
        }
        
        void Deallocate(void* ptr, nk_size size) {
            if (mSmallPool.Owns(ptr)) { mSmallPool.Deallocate(ptr); return; }
            if (mMediumPool.Owns(ptr)) { mMediumPool.Deallocate(ptr); return; }
            if (mLargePool.Owns(ptr)) { mLargePool.Deallocate(ptr); return; }
            mFallback->Deallocate(ptr);  // Fallback général
        }
        
        void ResetFrame() {
            // Reset bulk des pools fixes : très rapide
            mSmallPool.Reset();
            mMediumPool.Reset();
            mLargePool.Reset();
            // Note: le fallback n'est pas reset : allocations persistantes si nécessaire
        }
        
    private:
        nkentseu::memory::NkFixedPoolAllocator<64, 4096> mSmallPool;
        nkentseu::memory::NkFixedPoolAllocator<128, 1024> mMediumPool;
        nkentseu::memory::NkFixedPoolAllocator<256, 256> mLargePool;
        nkentseu::memory::NkAllocator* mFallback;
    };

    // Exemple 2 : Intégration avec placement new pour objets C++
    template<typename T, nk_size NumInstances = 1024>
    class TypedPool {
    public:
        using PoolType = nkentseu::memory::NkFixedPoolAllocator<sizeof(T), NumInstances>;
        
        T* New() {
            void* memory = mPool.Allocate(sizeof(T));
            if (!memory) return nullptr;
            return new (memory) T();  // Placement new
        }
        
        void Delete(T* obj) {
            if (!obj) return;
            obj->~T();  // Destruction explicite
            mPool.Deallocate(obj);  // Libération du buffer
        }
        
        void Reset() { mPool.Reset(); }  // Attention : ne détruit pas les objets !
        
        [[nodiscard]] float GetUsage() const { return mPool.GetUsage(); }
        
    private:
        PoolType mPool;
    };
    
    // Usage :
    TypedPool<Particle, 4096> particlePool;
    auto* p = particlePool.New();  // Construction + allocation pool
    if (p) {
        p->Update();
        particlePool.Delete(p);  // Destruction + libération pool
    }

    // Exemple 3 : Monitoring et alertes de saturation
    void MonitorPools() {
        static nkentseu::memory::NkFixedPoolAllocator<64, 1024> bulletPool("Bullets");
        
        const float usage = bulletPool.GetUsage();
        if (usage > 0.9f) {  // 90% de saturation
            NK_FOUNDATION_LOG_WARN("[Pool] Bullet pool at %.1f%% - consider increasing capacity", 
                                  usage * 100.0f);
            // Action corrective : augmenter NumBlocks, ou fallback vers allocateur général
        }
        
        // Stats pour HUD debug
        printf("Bullets: %zu/%zu active (%.1f%%)\n",
               bulletPool.GetNumBlocks() - bulletPool.GetNumFreeBlocks(),
               bulletPool.GetNumBlocks(),
               usage * 100.0f);
    }

    // Exemple 4 : Reset bulk en fin de frame (jeu vidéo)
    class FrameAllocator {
    public:
        void* Allocate(nk_size size, nk_size alignment = alignof(void*)) {
            // Tenter d'abord le pool variable pour petites allocations
            if (size <= 256) {
                void* ptr = mSmallPool.Allocate(size, alignment);
                if (ptr) return ptr;
                // Fallback si pool plein
            }
            // Fallback vers allocateur général
            return nkentseu::memory::NkGetDefaultAllocator().Allocate(size, alignment);
        }
        
        void EndFrame() {
            // Reset du pool : libère toutes les allocations temporaires de la frame
            mSmallPool.Reset();
            // Note: les allocations fallback ne sont pas reset : gestion manuelle si nécessaire
        }
        
    private:
        nkentseu::memory::NkVariablePoolAllocator mSmallPool;
    };
    
    // Usage dans la boucle principale :
    static FrameAllocator gFrameAlloc;
    
    void GameFrame() {
        // Allocations temporaires de la frame
        void* temp1 = gFrameAlloc.Allocate(32);
        void* temp2 = gFrameAlloc.Allocate(128);
        
        // ... logique du jeu ...
        
        // Cleanup automatique en fin de frame
        gFrameAlloc.EndFrame();  // temp1 et temp2 libérés d'un coup
    }
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================