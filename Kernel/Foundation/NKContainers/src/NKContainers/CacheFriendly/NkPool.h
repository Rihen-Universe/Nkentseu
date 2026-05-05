// -----------------------------------------------------------------------------
// FICHIER: NKContainers\Containers\CacheFriendly\NkPool.h
// DESCRIPTION: Pool d'objets - Allocation rapide de taille fixe
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CONTAINERS_CACHEFRIENDLY_NKPOOL_H
#define NK_CONTAINERS_CACHEFRIENDLY_NKPOOL_H

    // ========================================================================
    // INCLUSIONS DES DÉPENDANCES
    // ========================================================================

    #include "NKCore/NkTypes.h"                    // Définition des types fondamentaux (usize, etc.)
    #include "NKContainers/NkContainersApi.h"      // Macros d'exportation de l'API conteneurs
    #include "NKCore/NkTraits.h"                   // Traits pour move/forward et utilitaires méta
    #include "NKMemory/NkAllocator.h"              // Système d'allocation mémoire personnalisable
    #include "NKMemory/NkFunction.h"               // Wrapper pour les fonctions et callables
    #include "NKCore/Assert/NkAssert.h"            // Macros d'assertion pour le débogage sécurisé

    // ========================================================================
    // ESPACE DE NOMS PRINCIPAL
    // ========================================================================

    namespace nkentseu {

        // ====================================================================
        // CLASSE PRINCIPALE : NK POOL (POOL D'OBJETS)
        // ====================================================================

        /**
         * @brief Classe template implémentant un pool d'objets pour allocation rapide
         * 
         * Structure de données pré-allouant un bloc de mémoire contigu et gérant
         * des objets de taille fixe via une liste libre (free list) pour des
         * allocations/désallocations en temps constant O(1).
         * 
         * PRINCIPE DE FONCTIONNEMENT :
         * - Allocation unique d'un bloc contigu de N * sizeof(Node) au construction
         * - Gestion via free list chaînée : chaque slot libre pointe vers le suivant
         * - Allocate() : pop du premier élément libre de la free list (O(1))
         * - Deallocate() : push de l'élément libéré en tête de free list (O(1))
         * - Aucune fragmentation : tous les objets sont dans le même bloc mémoire
         * 
         * GARANTIES DE PERFORMANCE :
         * - Allocate() : O(1) constant, pas d'appel à malloc/free après construction
         * - Deallocate() : O(1) constant, simple manipulation de pointeurs
         * - Construction du pool : O(N) pour initialiser la free list
         * - Localité mémoire excellente : objets contigus, prefetching CPU optimal
         * - Pas de surcharge par objet : seul un pointeur Next par slot libre
         * 
         * CAS D'USAGE TYPQUES :
         * - Systèmes de particules et effets visuels (création/destruction fréquente)
         * - Entities dans les moteurs de jeu (allocation par frame)
         * - Buffers de messages et objets de communication réseau
         * - Objets temporaires dans des boucles de traitement intensif
         * - Tout scénario avec allocation/désallocation répétée d'objets de même type
         * 
         * @tparam T Type des objets gérés par le pool (doit avoir taille fixe connue)
         * @tparam Allocator Type de l'allocateur pour le bloc principal (défaut: memory::NkAllocator)
         * 
         * @note Thread-unsafe par défaut : synchronisation externe requise pour usage concurrent
         * @note Le pool ne gère PAS la construction/destruction automatique des objets
         * @note L'utilisateur doit appeler explicitement les destructeurs via placement new
         * @note Non copiable : un pool possède exclusivement son bloc mémoire
         */
        template<typename T, typename Allocator = memory::NkAllocator>
        class NkPool {

            // ====================================================================
            // SECTION PUBLIQUE : ALIASES DE TYPES
            // ====================================================================
            public:

                // ====================================================================
                // ALIASES DE TYPES POUR LA LISIBILITÉ ET LA GÉNÉRICITÉ
                // ====================================================================

                using ValueType = T;              ///< Alias du type des objets gérés par le pool
                using SizeType = usize;           ///< Alias du type pour les tailles/comptages

            // ====================================================================
            // SECTION PRIVÉE : STRUCTURE INTERNE ET MEMBRES
            // ====================================================================
            private:

                /**
                 * @brief Union représentant un nœud dans le pool d'objets
                 * 
                 * Structure polymorphe permettant de réutiliser le même espace mémoire
                 * soit pour stocker un objet T, soit pour maintenir la liste libre
                 * via un pointeur Next vers le prochain slot disponible.
                 * 
                 * @note Layout mémoire : sizeof(Node) = max(sizeof(T), sizeof(Node*))
                 * @note L'union permet d'économiser la mémoire : pas de champ Next dans les objets actifs
                 * @note Constructeur/destructeur vides : la gestion de vie est manuelle via placement new
                 */
                union Node {

                    T Object;           ///< Objet T stocké quand le slot est alloué
                    Node* Next;         ///< Pointeur vers le prochain slot libre (quand slot disponible)

                    /**
                     * @brief Constructeur par défaut (no-op)
                     * @note Ne construit pas T : la construction est manuelle via placement new
                     */
                    Node() {
                    }

                    /**
                     * @brief Destructeur (no-op)
                     * @note Ne détruit pas T : la destruction est manuelle via appel explicite du destructeur
                     */
                    ~Node() {
                    }
                };

                // ====================================================================
                // MEMBRES DONNÉES : ÉTAT DU POOL
                // ====================================================================

                Node* mBlocks;                  ///< Pointeur vers le bloc mémoire contigu alloué
                Node* mFreeList;                ///< Tête de la liste libre des slots disponibles
                SizeType mCapacity;             ///< Nombre total de slots dans le pool (fixe)
                SizeType mAllocated;            ///< Nombre de slots actuellement alloués
                Allocator* mAllocator;          ///< Pointeur vers l'allocateur utilisé pour le bloc principal

            // ====================================================================
            // SECTION PUBLIQUE : CONSTRUCTEURS ET DESTRUCTEUR
            // ====================================================================
            public:

                /**
                 * @brief Constructeur principal avec capacité et allocateur par défaut
                 * @param capacity Nombre d'objets que le pool peut gérer simultanément
                 * @note Alloue un bloc contigu de capacity * sizeof(Node) via l'allocateur par défaut
                 * @note Initialise la free list en chaînant tous les slots : O(capacity)
                 * @note Assertion en debug pour garantir capacity > 0
                 * @note Complexité : O(N) pour l'initialisation de la free list
                 */
                explicit NkPool(SizeType capacity)
                    : mBlocks(nullptr)
                    , mFreeList(nullptr)
                    , mCapacity(capacity)
                    , mAllocated(0)
                    , mAllocator(&memory::NkGetDefaultAllocator()) {
                    NKENTSEU_ASSERT(capacity > 0);
                    mBlocks = static_cast<Node*>(mAllocator->Allocate(capacity * sizeof(Node)));
                    for (SizeType i = 0; i < capacity - 1; ++i) {
                        mBlocks[i].Next = &mBlocks[i + 1];
                    }
                    mBlocks[capacity - 1].Next = nullptr;
                    mFreeList = mBlocks;
                }

                /**
                 * @brief Constructeur avec capacité et allocateur personnalisé
                 * @param capacity Nombre d'objets que le pool peut gérer simultanément
                 * @param allocator Pointeur vers un allocateur personnalisé (nullptr = défaut)
                 * @note Alloue un bloc contigu via l'allocateur spécifié
                 * @note Initialise la free list en chaînant tous les slots : O(capacity)
                 * @note Assertion en debug pour garantir capacity > 0
                 */
                explicit NkPool(SizeType capacity, Allocator* allocator)
                    : mBlocks(nullptr)
                    , mFreeList(nullptr)
                    , mCapacity(capacity)
                    , mAllocated(0)
                    , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator()) {
                    NKENTSEU_ASSERT(capacity > 0);
                    mBlocks = static_cast<Node*>(mAllocator->Allocate(capacity * sizeof(Node)));
                    for (SizeType i = 0; i < capacity - 1; ++i) {
                        mBlocks[i].Next = &mBlocks[i + 1];
                    }
                    mBlocks[capacity - 1].Next = nullptr;
                    mFreeList = mBlocks;
                }

                /**
                 * @brief Destructeur libérant le bloc mémoire alloué
                 * @note Ne détruit PAS automatiquement les objets T encore alloués
                 * @note L'utilisateur doit explicitement détruire tous les objets avant la destruction du pool
                 * @note Libère le bloc via mAllocator->Deallocate(mBlocks)
                 * @note Complexité : O(1) pour la libération du bloc
                 */
                ~NkPool() {
                    if (mBlocks) {
                        mAllocator->Deallocate(mBlocks);
                    }
                }

                // ====================================================================
                // SEMANTIQUE DE COPIE : NON-COPIABLE
                // ====================================================================

                /**
                 * @brief Constructeur de copie supprimé
                 * @note Un pool possède exclusivement son bloc mémoire : pas de copie possible
                 */
                NkPool(const NkPool&) = delete;

                /**
                 * @brief Opérateur d'affectation par copie supprimé
                 * @note Un pool possède exclusivement son bloc mémoire : pas d'assignation possible
                 */
                NkPool& operator=(const NkPool&) = delete;

            // ====================================================================
            // SECTION PUBLIQUE : ALLOCATION ET DÉSALLOCATION BAS NIVEAU
            // ====================================================================
            public:

                /**
                 * @brief Alloue un slot du pool et retourne un pointeur vers l'objet brut
                 * @return Pointeur vers T si un slot est disponible, nullptr si le pool est plein
                 * @note Complexité O(1) : pop du premier élément de la free list
                 * @note Ne construit PAS l'objet T : utiliser placement new pour initialisation
                 * @note Thread-unsafe : synchronisation externe requise pour accès concurrents
                 * @note Pattern typique : T* p = pool.Allocate(); new (p) T(args...);
                 */
                T* Allocate() {
                    if (mFreeList == nullptr) {
                        return nullptr;
                    }
                    Node* node = mFreeList;
                    mFreeList = mFreeList->Next;
                    ++mAllocated;
                    return &node->Object;
                }

                /**
                 * @brief Libère un slot du pool en le retournant à la free list
                 * @param ptr Pointeur vers l'objet T à libérer (doit provenir de ce pool)
                 * @note Complexité O(1) : push en tête de la free list
                 * @note Ne détruit PAS l'objet T : appeler explicitement le destructeur avant
                 * @note Assertions en debug pour valider que ptr appartient au pool et mAllocated > 0
                 * @note Pattern typique : p->~T(); pool.Deallocate(p);
                 */
                void Deallocate(T* ptr) {
                    NKENTSEU_ASSERT(ptr != nullptr);
                    NKENTSEU_ASSERT(mAllocated > 0);
                    NKENTSEU_ASSERT(ptr >= &mBlocks[0].Object && ptr <= &mBlocks[mCapacity - 1].Object);
                    Node* node = reinterpret_cast<Node*>(ptr);
                    node->Next = mFreeList;
                    mFreeList = node;
                    --mAllocated;
                }

            // ====================================================================
            // SECTION PUBLIQUE : HELPERS DE CONSTRUCTION/DESTRUCTION (C++11+)
            // ====================================================================
            public:

                // ====================================================================
                // SUPPORT C++11 : CONSTRUCTION ET DESTRUCTION AVEC FORWARDING
                // ====================================================================
                #if defined(NK_CPP11)

                    /**
                     * @brief Alloue et construit un objet avec forwarding parfait des arguments
                     * @tparam Args Types des arguments de construction de T
                     * @param args Arguments forwarded vers le constructeur de T
                     * @return Pointeur vers l'objet construit, ou nullptr si le pool est plein
                     * @note Combine Allocate() + placement new en une opération pratique
                     * @note Utilise traits::NkForward pour éviter les copies inutiles
                     * @note Pattern typique : T* p = pool.Construct(arg1, arg2);
                     */
                    template<typename... Args>
                    T* Construct(Args&&... args) {
                        T* ptr = Allocate();
                        if (ptr) {
                            new (ptr) T(traits::NkForward<Args>(args)...);
                        }
                        return ptr;
                    }

                    /**
                     * @brief Détruit un objet et le retourne au pool
                     * @param ptr Pointeur vers l'objet T à détruire et libérer
                     * @note Appelle explicitement le destructeur ~T() puis Deallocate()
                     * @note Sécurisé contre nullptr : no-op si ptr == nullptr
                     * @note Pattern typique : pool.Destroy(p);
                     */
                    void Destroy(T* ptr) {
                        if (ptr) {
                            ptr->~T();
                            Deallocate(ptr);
                        }
                    }

                #else

                    /**
                     * @brief Alloue et construit un objet avec le constructeur par défaut (C++98/03 fallback)
                     * @return Pointeur vers l'objet construit, ou nullptr si le pool est plein
                     * @note Version fallback sans forwarding : utilise constructeur par défaut
                     * @note Combine Allocate() + placement new T() en une opération pratique
                     */
                    T* Construct() {
                        T* ptr = Allocate();
                        if (ptr) {
                            new (ptr) T();
                        }
                        return ptr;
                    }

                    /**
                     * @brief Alloue et construit un objet par copie (C++98/03 fallback)
                     * @param value Référence const vers l'objet source à copier
                     * @return Pointeur vers l'objet copié, ou nullptr si le pool est plein
                     * @note Version fallback sans forwarding : utilise constructeur de copie
                     * @note Combine Allocate() + placement new T(value) en une opération pratique
                     */
                    T* Construct(const T& value) {
                        T* ptr = Allocate();
                        if (ptr) {
                            new (ptr) T(value);
                        }
                        return ptr;
                    }

                    /**
                     * @brief Détruit un objet et le retourne au pool (C++98/03 fallback)
                     * @param ptr Pointeur vers l'objet T à détruire et libérer
                     * @note Appelle explicitement le destructeur ~T() puis Deallocate()
                     * @note Sécurisé contre nullptr : no-op si ptr == nullptr
                     */
                    void Destroy(T* ptr) {
                        if (ptr) {
                            ptr->~T();
                            Deallocate(ptr);
                        }
                    }

                #endif // defined(NK_CPP11)

            // ====================================================================
            // SECTION PUBLIQUE : CAPACITÉ ET MÉTADONNÉES
            // ====================================================================
            public:

                /**
                 * @brief Retourne la capacité totale du pool (nombre de slots)
                 * @return Valeur constexpr de type SizeType égale à la capacité configurée
                 * @note Opération O(1) sans effet de bord, noexcept garantie
                 * @note Valeur fixe : déterminée à la construction, ne change jamais
                 */
                SizeType Capacity() const NKENTSEU_NOEXCEPT {
                    return mCapacity;
                }

                /**
                 * @brief Retourne le nombre de slots actuellement alloués
                 * @return Valeur de type SizeType représentant le compteur d'allocations actives
                 * @note Opération O(1) sans effet de bord, noexcept garantie
                 * @note Utile pour le monitoring : pool.Allocated() / pool.Capacity() = taux d'utilisation
                 */
                SizeType Allocated() const NKENTSEU_NOEXCEPT {
                    return mAllocated;
                }

                /**
                 * @brief Retourne le nombre de slots disponibles pour allocation
                 * @return Valeur de type SizeType égale à Capacity() - Allocated()
                 * @note Opération O(1) sans effet de bord, noexcept garantie
                 * @note Utile pour vérifier rapidement si une allocation est possible sans tenter
                 */
                SizeType Available() const NKENTSEU_NOEXCEPT {
                    return mCapacity - mAllocated;
                }

                /**
                 * @brief Teste si le pool est plein (aucun slot disponible)
                 * @return true si mAllocated >= mCapacity, false sinon
                 * @note Opération O(1) sans effet de bord, noexcept garantie
                 * @note Équivalent à Available() == 0 mais sémantiquement plus explicite
                 */
                bool IsFull() const NKENTSEU_NOEXCEPT {
                    return mAllocated >= mCapacity;
                }

                /**
                 * @brief Teste si le pool est vide (aucune allocation active)
                 * @return true si mAllocated == 0, false sinon
                 * @note Opération O(1) sans effet de bord, noexcept garantie
                 * @note Utile pour vérifier si Reset() est sûr sans détruire d'objets actifs
                 */
                bool Empty() const NKENTSEU_NOEXCEPT {
                    return mAllocated == 0;
                }

            // ====================================================================
            // SECTION PUBLIQUE : UTILITAIRES ET GESTION AVANCÉE
            // ====================================================================
            public:

                /**
                 * @brief Vérifie si un pointeur appartient au bloc mémoire de ce pool
                 * @param ptr Pointeur const vers un objet T à vérifier
                 * @return true si ptr est dans l'intervalle [&mBlocks[0].Object, &mBlocks[mCapacity-1].Object]
                 * @note Opération O(1) sans effet de bord, noexcept garantie
                 * @note Utile pour le débogage et la validation d'entrées dans Deallocate()
                 * @note Ne garantit pas que le slot est actuellement alloué, seulement qu'il fait partie du pool
                 */
                bool Owns(const T* ptr) const NKENTSEU_NOEXCEPT {
                    return ptr >= &mBlocks[0].Object && ptr <= &mBlocks[mCapacity - 1].Object;
                }

                /**
                 * @brief Réinitialise le pool en reconstruisant la free list sans détruire les objets
                 * @note Reconstruit la free list en chaînant tous les slots : O(capacity)
                 * @note Réinitialise mAllocated à 0 et mFreeList à mBlocks
                 * @note ATTENTION : n'appelle PAS les destructeurs des objets potentiellement actifs
                 * @note À utiliser uniquement quand on garantit qu'aucun objet n'est en usage, ou pour les types triviaux
                 * @note Pattern typique : après un niveau de jeu, reset du pool de particules sans destruction individuelle
                 */
                void Reset() {
                    for (SizeType i = 0; i < mCapacity - 1; ++i) {
                        mBlocks[i].Next = &mBlocks[i + 1];
                    }
                    mBlocks[mCapacity - 1].Next = nullptr;
                    mFreeList = mBlocks;
                    mAllocated = 0;
                }

                /**
                 * @brief Libère tous les slots sans destruction automatique des objets
                 * @note Alias sémantique pour Reset() : même comportement, nom plus explicite
                 * @note ATTENTION : n'appelle PAS les destructeurs des objets potentiellement actifs
                 * @note L'utilisateur doit explicitement appeler Destroy() sur chaque objet avant Clear()
                 * @note Pattern typique : for (auto* obj : activeObjects) pool.Destroy(obj); pool.Clear();
                 */
                void Clear() {
                    Reset();
                }
        };

    } // namespace nkentseu

#endif // NK_CONTAINERS_CACHEFRIENDLY_NKPOOL_H

// ============================================================================
// EXEMPLES D'UTILISATION ET PATTERNS COURANTS
// ============================================================================
/*
 * @section examples Exemples pratiques d'utilisation de NkPool
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 1 : Allocation manuelle avec placement new
 * --------------------------------------------------------------------------
 * @code
 * #include "NKContainers/CacheFriendly/NkPool.h"
 * #include <cstdio>
 * 
 * struct Particle {
 *     float x, y, z;
 *     float vx, vy, vz;
 *     
 *     Particle(float px, float py, float pz) 
 *         : x(px), y(py), z(pz), vx(0), vy(0), vz(0) {
 *     }
 *     
 *     void Update() {
 *         x += vx; y += vy; z += vz;
 *     }
 * };
 * 
 * void exempleManuel() {
 *     // Création d'un pool de 1000 particules
 *     nkentseu::NkPool<Particle> pool(1000);
 *     
 *     // Allocation manuelle + construction via placement new
 *     Particle* p = pool.Allocate();
 *     if (p) {
 *         new (p) Particle(1.0f, 2.0f, 3.0f);  // Construction in-place
 *         p->Update();
 *         printf("Particle at (%.1f, %.1f, %.1f)\n", p->x, p->y, p->z);
 *         
 *         // Destruction explicite + retour au pool
 *         p->~Particle();  // Appel explicite du destructeur
 *         pool.Deallocate(p);  // Retour du slot à la free list
 *     }
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 2 : Helpers Construct/Destroy (recommandé)
 * --------------------------------------------------------------------------
 * @code
 * void exempleHelpers() {
 *     nkentseu::NkPool<Particle> pool(500);
 *     
 *     // Construction simplifiée avec forwarding des arguments (C++11)
 *     #if defined(NK_CPP11)
 *     Particle* p1 = pool.Construct(10.0f, 20.0f, 30.0f);
 *     Particle* p2 = pool.Construct(40.0f, 50.0f, 60.0f);
 *     #else
 *     // Fallback C++98/03 : constructeur par défaut ou copie
 *     Particle prototype(0, 0, 0);
 *     Particle* p1 = pool.Construct(prototype);
 *     p1->x = 10.0f; p1->y = 20.0f; p1->z = 30.0f;
 *     #endif
 *     
 *     if (p1) {
 *         p1->Update();
 *         printf("P1: (%.1f, %.1f, %.1f)\n", p1->x, p1->y, p1->z);
 *     }
 *     
 *     // Destruction simplifiée : destructeur + deallocate en une étape
 *     pool.Destroy(p1);
 *     pool.Destroy(p2);  // Sécurisé même si p2 == nullptr
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 3 : Système de particules avec gestion de cycle de vie
 * --------------------------------------------------------------------------
 * @code
 * class ParticleSystem {
 * private:
 *     nkentseu::NkPool<Particle> mPool;
 *     nkentseu::NkVector<Particle*> mActive;  // Particules en vie
 *     
 * public:
 *     ParticleSystem(usize maxParticles) 
 *         : mPool(maxParticles) {
 *         mActive.Reserve(maxParticles);  // Pré-allouer pour éviter réallocations
 *     }
 *     
 *     void Emit(float x, float y, float z, usize count) {
 *         for (usize i = 0; i < count; ++i) {
 *             if (!mPool.IsFull()) {
 *                 #if defined(NK_CPP11)
 *                 Particle* p = mPool.Construct(x, y, z);
 *                 #else
 *                 Particle* p = mPool.Construct();
 *                 p->x = x; p->y = y; p->z = z;
 *                 #endif
 *                 if (p) {
 *                     p->vx = (rand() % 100) / 100.0f - 0.5f;  // Vitesse aléatoire
 *                     p->vy = (rand() % 100) / 100.0f - 0.5f;
 *                     p->vz = (rand() % 100) / 100.0f - 0.5f;
 *                     mActive.PushBack(p);
 *                 }
 *             }
 *         }
 *     }
 *     
 *     void Update(float deltaTime) {
 *         for (usize i = 0; i < mActive.Size(); ) {
 *             Particle* p = mActive[i];
 *             p->Update();
 *             
 *             // Supprimer les particules hors limites ou expirées
 *             if (p->y < -10.0f) {  // Exemple: tombées sous le sol
 *                 mPool.Destroy(p);  // Destruction + retour au pool
 *                 mActive[i] = mActive.Back();  // Swap avec dernier pour O(1) removal
 *                 mActive.PopBack();
 *             } else {
 *                 ++i;
 *             }
 *         }
 *     }
 *     
 *     void Clear() {
 *         for (Particle* p : mActive) {
 *             mPool.Destroy(p);
 *         }
 *         mActive.Clear();
 *     }
 * };
 * 
 * void exempleParticleSystem() {
 *     ParticleSystem system(10000);
 *     
 *     // Émission de particules
 *     system.Emit(0, 10, 0, 100);
 *     
 *     // Boucle de simulation
 *     for (int frame = 0; frame < 100; ++frame) {
 *         system.Update(0.016f);  // ~60 FPS
 *         // Rendu des particules actives...
 *     }
 *     
 *     system.Clear();  // Nettoyage final
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 4 : Reset pour types triviaux ou réinitialisation globale
 * --------------------------------------------------------------------------
 * @code
 * struct Transform {
 *     float x, y, z;
 *     float rx, ry, rz;
 *     // Type trivial : pas de destructeur complexe
 * };
 * 
 * void exempleReset() {
 *     nkentseu::NkPool<Transform> pool(1000);
 *     
 *     // Allocation et utilisation
 *     for (usize i = 0; i < 100; ++i) {
 *         Transform* t = pool.Construct();
 *         if (t) {
 *             t->x = static_cast<float>(i);
 *             // ... utilisation
 *         }
 *     }
 *     
 *     // Reset sécurisé pour types triviaux : pas besoin de destructeurs individuels
 *     pool.Reset();  // Tous les slots retournés à la free list en O(N)
 *     
 *     printf("Après reset - Alloués: %zu, Disponibles: %zu\n",
 *            pool.Allocated(), pool.Available());  // 0, 1000
 * }
 * 
 * // ATTENTION: Reset() sur types non-triviaux sans destruction préalable = fuite de ressources
 * void exempleResetDanger() {
 *     nkentseu::NkPool<nkentseu::NkString> pool(100);
 *     
 *     // Allocation de strings avec allocation interne
 *     for (usize i = 0; i < 50; ++i) {
 *         pool.Construct("Donnée %zu", i);  // NkString alloue son buffer interne
 *     }
 *     
 *     // Reset() sans destruction = fuite mémoire des buffers internes de NkString !
 *     // pool.Reset();  // NE PAS FAIRE !
 *     
 *     // Correction : détruire explicitement chaque objet d'abord
 *     // (nécessite de tracker les pointeurs actifs)
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 5 : Allocation avec allocateur personnalisé
 * --------------------------------------------------------------------------
 * @code
 * #include "NKMemory/NkPoolAllocator.h"
 * 
 * void exempleAllocateurCustom() {
 *     // Allocateur pool pour le bloc principal du NkPool
 *     memory::NkPoolAllocator mainAlloc(1024 * 1024);  // 1MB pool
 *     
 *     // NkPool utilisant l'allocateur personnalisé pour son bloc
 *     nkentseu::NkPool<Particle, memory::NkPoolAllocator> pool(1000, &mainAlloc);
 *     
 *     // Avantages :
 *     // - Allocation du bloc principal dans le pool -> pas de fragmentation externe
 *     // - Libération en bloc à la destruction -> très rapide
 *     // - Idéal pour les systèmes avec contraintes mémoire strictes
 *     
 *     // Utilisation normale
 *     Particle* p = pool.Construct(1, 2, 3);
 *     if (p) {
 *         p->Update();
 *         pool.Destroy(p);
 *     }
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 6 : Thread-safety via wrapper mutex (optionnel)
 * --------------------------------------------------------------------------
 * @code
 * #if defined(NK_THREADING)  // Macro projet pour support threading
 * 
 * #include "NKThreading/NkMutex.h"
 * 
 * template<typename T, typename Allocator = memory::NkAllocator>
 * class NkThreadSafePool {
 * private:
 *     NkPool<T, Allocator> mPool;
 *     nkentseu::NkMutex mMutex;
 *     
 * public:
 *     NkThreadSafePool(usize capacity, Allocator* alloc = nullptr)
 *         : mPool(capacity, alloc) {
 *     }
 *     
 *     T* Allocate() {
 *         nkentseu::NkMutexGuard lock(mMutex);
 *         return mPool.Allocate();
 *     }
 *     
 *     void Deallocate(T* ptr) {
 *         nkentseu::NkMutexGuard lock(mMutex);
 *         mPool.Deallocate(ptr);
 *     }
 *     
 *     // ... délégation des autres méthodes avec verrouillage
 * };
 * 
 * #endif // NK_THREADING
 * 
 * void exempleThreadSafe() {
 *     #if defined(NK_THREADING)
 *     // Pool thread-safe pour partage entre threads
 *     NkThreadSafePool<Particle> tsPool(1000);
 *     
 *     // Utilisation depuis plusieurs threads (pseudo-code)
 *     // Thread 1:
 *     // Particle* p1 = tsPool.Construct(...);
 *     // Thread 2:
 *     // Particle* p2 = tsPool.Construct(...);
 *     // Synchronisation automatique via mutex interne
 *     #endif
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * BONNES PRATIQUES ET RECOMMANDATIONS
 * --------------------------------------------------------------------------
 * 
 * 1. GESTION DU CYCLE DE VIE DES OBJETS :
 *    - Toujours appeler le destructeur avant Deallocate() pour les types non-triviaux
 *    - Préférer Construct()/Destroy() qui gèrent automatiquement placement new + destructeur
 *    - Documenter clairement dans l'API si Reset() est sûr pour le type T utilisé
 *    - Tracker les pointeurs actifs si besoin de destruction en masse sécurisée
 * 
 * 2. CHOIX DE LA CAPACITÉ :
 *    - Dimensionner selon le pic d'utilisation attendu, pas la moyenne
 *    - Mémoire allouée = capacity * sizeof(Node) : prévoir l'overhead de l'union
 *    - Pour les types larges : envisager un pool de pointeurs vers heap si mémoire critique
 *    - Utiliser Available() pour monitorer l'utilisation en production
 * 
 * 3. OPTIMISATIONS DE PERFORMANCE :
 *    - Localité mémoire excellente : parcours séquentiel des objets actifs pour prefetching
 *    - Pas de fragmentation : idéal pour les systèmes temps réel avec contraintes mémoire
 *    - O(1) allocation/désallocation : parfait pour les boucles fréquentes (game loops)
 *    - Pré-allouer le pool au démarrage pour éviter les allocations à l'exécution
 * 
 * 4. SÉCURITÉ ET ROBUSTESSE :
 *    - Assertions en debug pour valider que Deallocate() reçoit un pointeur du pool
 *    - Vérifier le retour de Allocate()/Construct() avant utilisation (peut retourner nullptr)
 *    - Thread-unsafe par défaut : protéger avec mutex si accès concurrents
 *    - Pour les types avec ressources : toujours utiliser Destroy() jamais Reset() seul
 * 
 * 5. PATTERNS D'USAGE RECOMMANDÉS :
 *    - Entity Component System : un pool par type de composant pour cache locality
 *    - Object pooling dans les jeux : réutiliser les objets au lieu de créer/détruire
 *    - Buffers de messages : pool de structures de communication pour éviter malloc fréquent
 *    - Allocation par frame : reset du pool à chaque frame pour les objets temporaires
 * 
 * 6. LIMITATIONS CONNUES :
 *    - Taille fixe à la construction : pas d'expansion dynamique
 *    - Pas de tracking automatique des objets actifs : responsabilité de l'utilisateur
 *    - Reset() dangereux pour les types non-triviaux sans destruction préalable
 *    - Pas de support intégré pour la thread-safety : à ajouter via wrapper si besoin
 * 
 * 7. EXTENSIONS RECOMMANDÉES :
 *    - Ajouter un itérateur pour parcourir les objets actifs (nécessite tracking externe)
 *    - Implémenter une méthode TryAllocate(timeout) pour attente bloquante en mode thread-safe
 *    - Ajouter un callback OnAllocate/OnDeallocate pour instrumentation et debugging
 *    - Supporter la réallocation dynamique (agrandissement du pool) si besoin avancé
 *    - Ajouter une méthode Stats() retournant fragmentation, pic d'utilisation, etc.
 * 
 * 8. COMPARAISON AVEC AUTRES STRUCTURES :
 *    - vs new/delete : NkPool évite la fragmentation et offre O(1) garanti
 *    - vs NkVector : NkPool pour objets indépendants, Vector pour séquence ordonnée
 *    - vs std::memory_resource : NkPool plus simple, spécialisé pour objets de même type
 *    - Règle : utiliser NkPool quand allocation/désallocation fréquente d'objets de même type
 */

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Généré par Rihen le 2026-02-05 22:26:13
// Dernière modification : 2026-04-26 (restructuration et documentation)
// ============================================================