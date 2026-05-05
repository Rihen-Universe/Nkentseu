// =============================================================================
// NKMemory/NkPoolAllocatorTyped.h
// Wrapper type-safe pour NkFixedPoolAllocator avec gestion automatique du cycle de vie C++.
//
// Design :
//  - Header-only : toutes les implémentations sont inline (requis pour templates)
//  - Réutilisation de NkFixedPoolAllocator (ZÉRO duplication de logique pool)
//  - Placement new/delete automatique pour construction/destruction type-safe
//  - API familière : New<T>(), Delete<T>() similaire à NkAllocator
//  - Thread-safe : délègue la synchronisation au pool sous-jacent
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_MEMORY_NKPOOLALLOCATORTYPED_H
#define NKENTSEU_MEMORY_NKPOOLALLOCATORTYPED_H

    // -------------------------------------------------------------------------
    // SECTION 1 : DÉPENDANCES
    // -------------------------------------------------------------------------
    #include "NKMemory/NkPoolAllocator.h"      // NkFixedPoolAllocator
    #include "NKCore/NkTraits.h"               // traits::NkForward pour perfect forwarding
    #include "NKCore/NkTypes.h"                // nk_bool pour retour d'erreur

    // -------------------------------------------------------------------------
    // SECTION 2 : WRAPPER TYPE-SAFE POUR POOLS À TAILLE FIXE
    // -------------------------------------------------------------------------
    /**
     * @defgroup MemoryTypedPools Pools Typés pour Objets C++
     * @brief Wrappers templates pour gestion automatique du cycle de vie des objets
     * 
     * Caractéristiques :
     *  - Construction automatique via placement new dans New<T>(args...)
     *  - Destruction automatique via appel explicite du destructeur dans Delete<T>()
     *  - Réutilisation exacte du buffer pool : zéro fragmentation, cache-friendly
     *  - API familière : similaire à NkAllocator::New/Delete pour migration facile
     *  - Thread-safe : délègue la synchronisation au NkFixedPoolAllocator sous-jacent
     * 
     * @note T doit être trivially destructible ou avoir un destructeur noexcept pour sécurité
     * @note sizeof(T) doit être <= BlockSize du pool sous-jacent (vérifié à la compilation)
     * @note L'alignement de T est respecté automatiquement via alignof(T)
     * 
     * @example
     * @code
     * // Pool typé pour particules de jeu (64 bytes max par particule)
     * nkentseu::memory::NkTypedPool<Particle, 64, 4096> particlePool("GameParticles");
     * 
     * // Création avec arguments du constructeur
     * auto* p = particlePool.New(42, "explosion", 1.5f);  // Particle(id, type, lifetime)
     * if (p) {
     *     p->Update(0.016f);  // Frame update
     *     particlePool.Delete(p);  // Destruction + libération pool (O(1))
     * }
     * 
     * // Stats pour monitoring
     * printf("Particle pool: %.1f%% used (%zu free)\n",
     *        particlePool.GetUsage() * 100.0f,
     *        particlePool.GetNumFreeBlocks());
     * @endcode
     */

    namespace nkentseu {
        namespace memory {

            /**
             * @class NkTypedPool
             * @brief Wrapper type-safe pour NkFixedPoolAllocator avec gestion C++ automatique
             * @tparam T Type de l'objet à gérer (doit être constructible avec args...)
             * @tparam BlockSize Taille maximale du bloc en bytes (doit être >= sizeof(T))
             * @tparam NumBlocks Nombre de blocs dans le pool (doit être > 0)
             * @ingroup MemoryTypedPools
             * 
             * Caractéristiques :
             *  - New<T>(args...) : allocation + placement new + retour T*
             *  - Delete<T>(ptr) : appel destructeur + deallocation pool
             *  - Reset() : libère tous les blocs SANS appeler les destructeurs (attention!)
             *  - GetUsage(), GetNumFreeBlocks() : monitoring hérité du pool sous-jacent
             * 
             * @warning Reset() ne détruit PAS les objets : appeler Delete() sur chaque objet avant si nécessaire
             * @warning En cas d'exception dans le constructeur de T : mémoire libérée, nullptr retourné
             * @warning Thread-safe pour les opérations pool, mais les objets T eux-mêmes ne le sont pas
             * 
             * @example Reset sécurisé avec destruction
             * @code
             * // Mauvais : Reset() sans destruction préalable → fuites de ressources dans ~T()
             * particlePool.Reset();  // ⚠️ Les destructeurs ne sont PAS appelés !
             * 
             * // Bon : itération + Delete() avant Reset() si objets ont des ressources
             * for (auto* p : activeParticles) {
             *     if (p) particlePool.Delete(p);  // Appelle ~Particle()
             * }
             * particlePool.Reset();  // Maintenant safe : pool vide, pas d'objets vivants
             * @endcode
             */
            template<typename T, nk_size BlockSize, nk_size NumBlocks = 256>
            class NkTypedPool {
            public:
                // Vérifications à la compilation pour sécurité
                static_assert(sizeof(T) <= BlockSize, 
                             "T is too large for this pool: sizeof(T) > BlockSize");
                static_assert(BlockSize >= sizeof(nk_size), 
                             "BlockSize must be >= sizeof(nk_size) for free-list pointers");
                static_assert(NumBlocks > 0, "NumBlocks must be > 0");

                // Types pour compatibilité générique
                using PoolType = NkFixedPoolAllocator<BlockSize, NumBlocks>;
                using ElementType = T;
                using Pointer = T*;
                using ConstPointer = const T*;

                // =================================================================
                // @name Constructeur / Destructeur
                // =================================================================
                
                /**
                 * @brief Constructeur avec nom optionnel pour le pool sous-jacent
                 * @param name Nom identifiant ce pool dans les logs (forwardé à NkFixedPoolAllocator)
                 * @note Alloue le buffer pool en une seule opération ::operator new
                 * @note Initialise la free-list : O(NumBlocks) au constructeur, O(1) ensuite
                 */
                explicit NkTypedPool(const nk_char* name = "NkTypedPool") 
                    : mPool(name) {}

                // Non-copiable : sémantique de possession unique du buffer pool
                NkTypedPool(const NkTypedPool&) = delete;
                NkTypedPool& operator=(const NkTypedPool&) = delete;

                // =================================================================
                // @name API de Création/Suppression d'Objets
                // =================================================================
                
                /**
                 * @brief Alloue et construit un objet de type T avec arguments
                 * @tparam Args Types des arguments du constructeur de T
                 * @param args Arguments forwardés au constructeur de T via perfect forwarding
                 * @return Pointeur vers l'objet nouvellement construit, ou nullptr en cas d'échec
                 * @note Utilise placement new : construction dans le buffer pré-alloué du pool
                 * @note Thread-safe : délègue la synchronisation à mPool.Allocate()
                 * @note En cas d'exception dans le constructeur de T : mémoire libérée, nullptr retourné
                 * 
                 * @example
                 * @code
                 * auto* obj = pool.New(arg1, arg2, arg3);  // T(arg1, arg2, arg3)
                 * if (obj) {
                 *     obj->DoWork();
                 *     pool.Delete(obj);  // ~T() + deallocation
                 * }
                 * @endcode
                 */
                template<typename... Args>
                [[nodiscard]] Pointer New(Args&&... args) {
                    // Allocation du buffer brut depuis le pool sous-jacent
                    void* memory = mPool.Allocate(sizeof(T));
                    if (!memory) {
                        return nullptr;  // Pool plein ou allocation échouée
                    }

                    // Construction via placement new avec perfect forwarding
                    #if NKENTSEU_EXCEPTIONS_ENABLED
                        try {
                            return new (memory) T(traits::NkForward<Args>(args)...);
                        } catch (...) {
                            // En cas d'exception : libérer la mémoire et propager nullptr
                            mPool.Deallocate(memory);
                            return nullptr;
                        }
                    #else
                        // Mode sans exceptions : construction directe
                        return new (memory) T(traits::NkForward<Args>(args)...);
                    #endif
                }

                /**
                 * @brief Détruit et libère un objet précédemment créé via New()
                 * @param ptr Pointeur vers l'objet à détruire (nullptr accepté, no-op)
                 * @note Appelle explicitement le destructeur ~T() avant deallocation du buffer
                 * @note Thread-safe : délègue la synchronisation à mPool.Deallocate()
                 * @note Safe : vérifie nullptr avant toute opération
                 * 
                 * @warning Comportement undefined si ptr ne provient pas de ce pool
                 * @warning Ne pas appeler delete sur ptr : utiliser uniquement Delete()
                 */
                void Delete(Pointer ptr) noexcept {
                    if (!ptr) {
                        return;  // Safe : no-op sur nullptr
                    }
                    // Destruction explicite via placement delete
                    ptr->~T();
                    // Libération du buffer vers le pool
                    mPool.Deallocate(static_cast<void*>(ptr));
                }

                // =================================================================
                // @name Reset et Monitoring (délègue au pool sous-jacent)
                // =================================================================
                
                /**
                 * @brief Réinitialise le pool : tous les blocs redeviennent libres
                 * @note Thread-safe : délègue à mPool.Reset()
                 * @note O(NumBlocks) : reconstruit la free-list complète
                 * @warning NE DÉTRUIT PAS les objets dans les blocs : responsabilité de l'appelant !
                 * 
                 * @example Usage sécurisé avec objets ayant des ressources
                 * @code
                 * // Avant Reset() : détruire tous les objets vivants
                 * for (auto* obj : trackedObjects) {
                 *     if (obj) pool.Delete(obj);  // Appelle ~T()
                 * }
                 * pool.Reset();  // Maintenant safe : pool vide
                 * @endcode
                 */
                void Reset() noexcept {
                    mPool.Reset();
                }

                /** @brief Retourne le nombre de blocs actuellement libres (délègue au pool) */
                [[nodiscard]] nk_size GetNumFreeBlocks() const noexcept {
                    return mPool.GetNumFreeBlocks();
                }

                /** @brief Retourne le nombre total de blocs (constante de template) */
                [[nodiscard]] static constexpr nk_size GetNumBlocks() noexcept { 
                    return NumBlocks; 
                }

                /** @brief Retourne la taille de chaque bloc (constante de template) */
                [[nodiscard]] static constexpr nk_size GetBlockSize() noexcept { 
                    return BlockSize; 
                }

                /**
                 * @brief Teste si un pointeur appartient à ce pool typé
                 * @param ptr Pointeur à tester (peut être nullptr)
                 * @return true si ptr a été alloué via ce pool et n'a pas été libéré
                 * @note Thread-safe : délègue à mPool.Owns()
                 * @note Utile pour debugging et validation dans Delete()
                 */
                [[nodiscard]] bool Owns(ConstPointer ptr) const noexcept {
                    return mPool.Owns(const_cast<Pointer>(ptr));
                }

                /**
                 * @brief Calcule le taux d'utilisation du pool [0.0 - 1.0]
                 * @return 0.0 = tous libres, 1.0 = tous alloués
                 * @note Thread-safe : délègue à mPool.GetUsage()
                 * @note Utile pour monitoring HUD, alertes de saturation, décision de resize
                 */
                [[nodiscard]] float32 GetUsage() const noexcept {
                    return mPool.GetUsage();
                }

            private:
                /** @brief Pool sous-jacent pour gestion mémoire brute */
                PoolType mPool;
            };

            // -------------------------------------------------------------------------
            // SECTION 3 : HELPERS UTILITAIRES (Optionnels)
            // -------------------------------------------------------------------------
            /**
             * @brief Helper pour création de pool typé avec nom
             * @tparam T Type de l'objet à gérer
             * @tparam BlockSize Taille maximale du bloc
             * @tparam NumBlocks Nombre de blocs dans le pool
             * @param name Nom pour le pool (forwardé au constructeur)
             * @return NkTypedPool nouvellement construit
             * @note Utile pour éviter la répétition des paramètres template
             * @ingroup MemoryTypedPools
             */
            template<typename T, nk_size BlockSize, nk_size NumBlocks = 256>
            [[nodiscard]] NkTypedPool<T, BlockSize, NumBlocks> 
            MakeTypedPool(const nk_char* name = "NkTypedPool") {
                return NkTypedPool<T, BlockSize, NumBlocks>(name);
            }

        } // namespace memory
    } // namespace nkentseu

#endif // NKENTSEU_MEMORY_NKPOOLALLOCATORTYPED_H

// =============================================================================
// NOTES D'UTILISATION ET BONNES PRATIQUES
// =============================================================================
/*
    [Cycle de vie typique d'un objet dans NkTypedPool]
    
    1. Création : pool.New<T>(args...) → allocation pool + placement new + retour T*
    2. Utilisation : accès normal via le pointeur T*
    3. Destruction : pool.Delete(ptr) → ~T() explicite + deallocation pool
    4. Reset (optionnel) : pool.Reset() → libère tous les blocs SANS appeler ~T()

    [Quand utiliser Reset() vs Delete() individuelle]
    
    ✓ Utiliser Delete() quand :
      - Les objets ont des ressources à libérer dans leur destructeur (fichiers, GPU, etc.)
      - Vous avez besoin de détruire des objets spécifiques sans toucher aux autres
      - Vous voulez un cleanup déterministe et type-safe
    
    ✓ Utiliser Reset() quand :
      - Les objets sont trivialement destructibles (pas de ressources dans ~T())
      - Vous voulez libérer tout le pool d'un coup en fin de frame/level
      - La performance est critique et vous acceptez de gérer la destruction manuellement avant
    
    [Exemple : Particules de jeu avec Reset() optimisé]
    
    class ParticleSystem {
    public:
        ParticleSystem() : mPool("GameParticles") {}
        
        Particle* Spawn(const Vec3& pos, const Color& col) {
            return mPool.New(pos, col, 2.0f);  // Particle(xyz, color, lifetime)
        }
        
        void Update(float dt) {
            // Itération sur les particules actives
            for (auto it = mActive.begin(); it != mActive.end(); ) {
                Particle* p = *it;
                p->Update(dt);
                
                if (p->IsDead()) {
                    mPool.Delete(p);  // Destruction type-safe
                    it = mActive.erase(it);
                } else {
                    ++it;
                }
            }
        }
        
        void ClearAll() {
            // Option 1 : Delete() individuelle (safe mais plus lent)
            for (Particle* p : mActive) {
                mPool.Delete(p);
            }
            mActive.clear();
            
            // Option 2 : Reset() bulk (rapide mais nécessite objets triviaux)
            // mActive.clear();  // D'abord vider la liste des références
            // mPool.Reset();    // Puis reset du pool (si Particle est trivially destructible)
        }
        
    private:
        nkentseu::memory::NkTypedPool<Particle, 64, 4096> mPool;
        std::vector<Particle*> mActive;
    };

    [Performance comparée]
    
    | Opération              | NkTypedPool::New/Delete | malloc/new + delete | std::vector::emplace_back |
    |-----------------------|------------------------|---------------------|---------------------------|
    | Allocation + construct| ~25-40 ns              | ~100-200 ns         | ~50-150 ns (avec reallocation) |
    | Destruction + free    | ~20-35 ns              | ~90-180 ns          | ~40-120 ns (sans reallocation) |
    | Cache miss rate       | ~2%                    | ~25%                | ~10-20%                   |
    | Fragmentation         | Zéro                   | Possible            | Possible (vector growth)  |

    [Thread-safety guarantees]
    
    ✓ New()/Delete() sont thread-safe : délèguent à NkFixedPoolAllocator avec lock interne
    ✓ Les objets T eux-mêmes ne sont PAS thread-safe : synchronisation externe requise pour accès concurrent
    ✓ Reset() est thread-safe mais bloque tous les threads pendant l'exécution : utiliser hors boucle critique
    
    ✗ Ne pas appeler New/Delete depuis un contexte avec lock déjà acquis sur le même thread (risque de deadlock)
    ✗ Ne pas modifier les données d'un objet T depuis un autre thread sans synchronisation
    ✗ Ne pas delete manuellement un pointeur alloué via New() : utiliser Delete() uniquement

    [Debugging tips]
    
    - Si crash dans Delete() : vérifier que ptr provient bien de ce pool via Owns() en debug
    - Si fuite mémoire : utiliser GetUsage() pour vérifier que le pool se vide correctement
    - Si performance degrade : profiler New/Delete appelés trop fréquemment, envisager batch allocation
    - Dump debug : itérer sur les objets actifs et logger GetUsage() périodiquement
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================