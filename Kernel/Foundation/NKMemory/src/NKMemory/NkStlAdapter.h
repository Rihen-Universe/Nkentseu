// =============================================================================
// NKMemory/NkStlAdapter.h
// Adaptateurs templates pour intégration transparente des allocateurs NKMemory
// avec les conteneurs STL standards (std::vector, std::list, std::unordered_map, etc.).
//
// Design :
//  - Header-only : toutes les implémentations sont inline (requis pour templates)
//  - Compatible std::allocator_traits : substitution transparente dans le code STL
//  - Support de tous les NkAllocator dérivés : NkMallocAllocator, NkPoolAllocator, etc.
//  - Type-safe et noexcept où approprié : garanties de sécurité et performance
//  - Exemples inclus pour les conteneurs les plus courants
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_MEMORY_NKSTLADAPTER_H
#define NKENTSEU_MEMORY_NKSTLADAPTER_H

    // -------------------------------------------------------------------------
    // SECTION 1 : DÉPENDANCES
    // -------------------------------------------------------------------------
    #include "NKMemory/NkAllocator.h"      // NkAllocator, NK_MEMORY_DEFAULT_ALIGNMENT
    #include "NKCore/NkTypes.h"            // nk_size pour compatibilité
    #include <memory>                      // std::allocator_traits, std::destroy_at
    #include <new>                         // std::bad_alloc, std::nothrow_t
    #include <type_traits>                 // std::is_void, std::add_const

    // -------------------------------------------------------------------------
    // SECTION 2 : CONFIGURATION ET CONCEPTS
    // -------------------------------------------------------------------------
    /**
     * @defgroup MemoryStlAdapters Adaptateurs STL pour Allocateurs NKMemory
     * @brief Wrappers templates pour utiliser les allocateurs NKMemory avec les conteneurs STL
     * 
     * Caractéristiques :
     *  - Compatible std::allocator_traits : fonctionne avec std::vector, std::list, std::map, etc.
     *  - Supporte tous les NkAllocator dérivés : NkMallocAllocator, NkPoolAllocator, NkContainerAllocator, etc.
     *  - Propagation d'allocateur : supporte std::allocator_traits::propagate_on_container_* traits
     *  - Rebind : supporte std::allocator_traits::rebind_alloc pour conteneurs de types différents
     *  - noexcept : méthodes marquées noexcept quand l'allocateur sous-jacent le garantit
     * 
     * @note L'allocateur NKMemory doit avoir une méthode Allocate(size, alignment) et Deallocate(ptr)
     * @note Les méthodes de construction/destruction utilisent std::construct_at/std::destroy_at (C++17)
     * @note En mode sans exceptions : les allocations échouées retournent nullptr au lieu de lancer std::bad_alloc
     * 
     * @example std::vector avec NkContainerAllocator
     * @code
     * #include "NKMemory/NkStlAdapter.h"
     * #include <vector>
     * 
     * nkentseu::memory::NkContainerAllocator containerAlloc;
     * 
     * // Adaptateur pour int avec NkContainerAllocator
     * using IntVector = std::vector<int, nkentseu::memory::NkStlAdapter<int, nkentseu::memory::NkContainerAllocator>>;
     * 
     * IntVector vec(&containerAlloc);  // Constructeur prenant l'allocateur
     * vec.reserve(10000);  // Allocations rapides via size classes
     * vec.push_back(42);
     * @endcode
     * 
     * @example std::unordered_map avec NkPoolAllocator
     * @code
     * #include "NKMemory/NkStlAdapter.h"
     * #include <unordered_map>
     * 
     * // Pool pour paires clé-valeur de taille fixe
     * nkentseu::memory::NkFixedPoolAllocator<sizeof(std::pair<const int, float>), 4096> pairPool;
     * 
     * using FloatMap = std::unordered_map<
     *     int, float,
     *     std::hash<int>,
     *     std::equal_to<int>,
     *     nkentseu::memory::NkStlAdapter<std::pair<const int, float>, decltype(pairPool)>
     * >;
     * 
     * FloatMap map(&pairPool);
     * map[1] = 1.5f;  // Allocation via pool, pas de fragmentation
     * @endcode
     */

    namespace nkentseu {
        namespace memory {

            // -------------------------------------------------------------------------
            // SECTION 3 : ADAPTATEUR STL PRINCIPAL
            // -------------------------------------------------------------------------
            /**
             * @class NkStlAdapter
             * @brief Adaptateur template pour utiliser un NkAllocator avec les conteneurs STL
             * @tparam T Type des éléments stockés dans le conteneur
             * @tparam AllocatorType Type de l'allocateur NKMemory (doit dériver de NkAllocator)
             * @ingroup MemoryStlAdapters
             * 
             * Caractéristiques :
             *  - Implémente l'interface requise par std::allocator_traits (C++11+)
             *  - value_type, pointer, const_pointer, etc. définis pour compatibilité STL
             *  - allocate/deallocate délèguent à l'allocateur NKMemory sous-jacent
             *  - construct/destroy utilisent std::construct_at/std::destroy_at pour safety
             *  - propagate_on_container_* traits configurés pour propagation d'allocateur
             *  - rebind_alloc pour support des conteneurs avec types d'éléments différents
             * 
             * @note L'allocateur NKMemory est stocké par pointeur : partage possible entre conteneurs
             * @note En cas d'échec d'allocation : lance std::bad_alloc (ou retourne nullptr sans exceptions)
             * @note Thread-safe si l'allocateur NKMemory sous-jacent est thread-safe
             * 
             * @warning Ne pas copier/mover l'adaptateur sans comprendre la sémantique de partage d'allocateur
             * @warning L'allocateur NKMemory doit rester valide pendant toute la durée de vie du conteneur
             * 
             * @example Configuration des traits std::allocator_traits
             * @code
             * // std::allocator_traits détecte automatiquement :
             * // - propagate_on_container_copy_assignment = false (par défaut)
             * // - propagate_on_container_move_assignment = true (optimisation)
             * // - propagate_on_container_swap = true (pour swap efficace)
             * // - is_always_equal = false (car l'allocateur est stocké par pointeur)
             * 
             * // Pour changer ces comportements, spécialiser std::allocator_traits<NkStlAdapter<...>>
             * @endcode
             */
            template<typename T, typename AllocatorType>
            class NkStlAdapter {
            public:
                // =================================================================
                // @name Types requis par std::allocator_traits
                // =================================================================
                
                using value_type = T;                          ///< Type des éléments stockés
                using pointer = T*;                            ///< Pointeur vers T
                using const_pointer = const T*;                ///< Pointeur vers const T
                using reference = T&;                          ///< Référence vers T
                using const_reference = const T&;              ///< Référence vers const T
                using size_type = nk_size;                     ///< Type pour les tailles (nk_size)
                using difference_type = std::ptrdiff_t;        ///< Type pour les différences de pointeurs

                /**
                 * @brief Rebind pour conteneurs avec types d'éléments différents
                 * @tparam U Nouveau type d'élément
                 * @return NkStlAdapter<U, AllocatorType> pour le nouveau type
                 * @note Requis par std::allocator_traits pour conteneurs comme std::list<T>::node_type
                 */
                template<typename U>
                struct rebind {
                    using other = NkStlAdapter<U, AllocatorType>;
                };

                // =================================================================
                // @name Constructeurs
                // =================================================================
                
                /** @brief Constructeur par défaut : allocateur nullptr (fallback vers défaut) */
                NkStlAdapter() noexcept : mAllocator(nullptr) {}

                /**
                 * @brief Constructeur avec allocateur explicite
                 * @param alloc Pointeur vers l'allocateur NKMemory à utiliser
                 * @note Si nullptr : fallback vers NkGetDefaultAllocator() lors des allocations
                 */
                explicit NkStlAdapter(AllocatorType* alloc) noexcept : mAllocator(alloc) {}

                /**
                 * @brief Constructeur de copie : partage le même pointeur d'allocateur
                 * @tparam U Autre type d'élément (pour rebind)
                 * @param other Adaptateur pour un autre type à copier
                 * @note Les adaptateurs de types différents partagent le même allocateur sous-jacent
                 */
                template<typename U>
                NkStlAdapter(const NkStlAdapter<U, AllocatorType>& other) noexcept 
                    : mAllocator(other.GetAllocator()) {}

                // =================================================================
                // @name Méthodes d'Allocation/Désallocation
                // =================================================================
                
                /**
                 * @brief Alloue de la mémoire pour n éléments de type T
                 * @param n Nombre d'éléments à allouer
                 * @return Pointeur vers la mémoire allouée, ou nullptr en cas d'échec
                 * @note Délègue à AllocatorType::Allocate() avec alignement alignof(T)
                 * @note Lance std::bad_alloc en cas d'échec (si NKENTSEU_EXCEPTIONS_ENABLED)
                 * @note Thread-safe si l'allocateur sous-jacent est thread-safe
                 */
                [[nodiscard]] pointer allocate(size_type n) {
                    if (n == 0) {
                        return nullptr;
                    }
                    
                    AllocatorType* alloc = mAllocator ? mAllocator : &NkGetDefaultAllocator();
                    void* mem = alloc->Allocate(n * sizeof(T), alignof(T));
                    
                    if (!mem) {
                        #if NKENTSEU_EXCEPTIONS_ENABLED
                            throw std::bad_alloc{};
                        #else
                            return nullptr;
                        #endif
                    }
                    
                    return static_cast<pointer>(mem);
                }

                /**
                 * @brief Libère de la mémoire précédemment allouée
                 * @param p Pointeur vers la mémoire à libérer (peut être nullptr, no-op)
                 * @param n Nombre d'éléments (informationnel, peut être ignoré par l'allocateur)
                 * @note Délègue à AllocatorType::Deallocate()
                 * @note noexcept : la désallocation ne doit jamais lever d'exception
                 */
                void deallocate(pointer p, size_type /*n*/) noexcept {
                    if (!p) {
                        return;
                    }
                    AllocatorType* alloc = mAllocator ? mAllocator : &NkGetDefaultAllocator();
                    alloc->Deallocate(p);
                }

                // =================================================================
                // @name Construction/Destruction d'Objets
                // =================================================================
                
                /**
                 * @brief Construit un objet de type U en place à l'adresse p
                 * @tparam U Type de l'objet à construire (peut être différent de T)
                 * @tparam Args Types des arguments du constructeur de U
                 * @param p Pointeur vers la mémoire pré-allouée (doit être alignée pour U)
                 * @param args Arguments forwardés au constructeur de U
                 * @note Utilise std::construct_at (C++17) ou placement new (C++11/14)
                 * @note noexcept si le constructeur de U est noexcept
                 */
                template<typename U, typename... Args>
                void construct(U* p, Args&&... args) 
                    noexcept(std::is_nothrow_constructible<U, Args...>::value) {
                    #if __cpp_lib_construct_at >= 201703L
                        std::construct_at(p, traits::NkForward<Args>(args)...);
                    #else
                        ::new (static_cast<void*>(p)) U(traits::NkForward<Args>(args)...);
                    #endif
                }

                /**
                 * @brief Détruit un objet de type U à l'adresse p
                 * @tparam U Type de l'objet à détruire (peut être différent de T)
                 * @param p Pointeur vers l'objet à détruire (nullptr accepté, no-op)
                 * @note Utilise std::destroy_at (C++17) ou appel explicite du destructeur
                 * @note noexcept : la destruction ne doit jamais lever d'exception
                 */
                template<typename U>
                void destroy(U* p) noexcept {
                    if (!p) {
                        return;
                    }
                    #if __cpp_lib_destroy_at >= 201703L
                        std::destroy_at(p);
                    #else
                        p->~U();
                    #endif
                }

                // =================================================================
                // @name Méthodes Optionnelles (optimisations)
                // =================================================================
                
                /**
                 * @brief Retourne le nombre maximal d'éléments pouvant être alloués
                 * @return SIZE_MAX / sizeof(T) (limite théorique)
                 * @note Utilisé par certains conteneurs pour pré-vérifier les capacités
                 */
                [[nodiscard]] static constexpr size_type max_size() noexcept {
                    return static_cast<size_type>(-1) / sizeof(T);
                }

                /**
                 * @brief Teste si deux adaptateurs peuvent allouer de la mémoire interchangeablement
                 * @param other Autre adaptateur à comparer
                 * @return true si les deux pointent vers le même allocateur (ou tous deux nullptr)
                 * @note Utilisé par std::allocator_traits::is_always_equal (ici : false car pointeur)
                 */
                [[nodiscard]] bool operator==(const NkStlAdapter& other) const noexcept {
                    return mAllocator == other.mAllocator;
                }
                
                #if __cpp_impl_three_way_comparison < 201907L
                    [[nodiscard]] bool operator!=(const NkStlAdapter& other) const noexcept {
                        return !(*this == other);
                    }
                #endif

                // =================================================================
                // @name Accès à l'Allocateur Sous-Jacent
                // =================================================================
                
                /**
                 * @brief Retourne le pointeur vers l'allocateur NKMemory configuré
                 * @return Pointeur vers AllocatorType, ou nullptr si fallback vers défaut
                 * @note Utile pour inspection ou pour passer l'allocateur à d'autres composants
                 */
                [[nodiscard]] AllocatorType* GetAllocator() const noexcept {
                    return mAllocator;
                }

            private:
                /** @brief Pointeur vers l'allocateur NKMemory sous-jacent (non-possédé) */
                AllocatorType* mAllocator;
            };

            // -------------------------------------------------------------------------
            // SECTION 4 : TRAITS SPÉCIALISÉS POUR std::allocator_traits
            // -------------------------------------------------------------------------
            /**
             * @brief Spécialisation de std::allocator_traits pour NkStlAdapter
             * @note Configure les traits de propagation d'allocateur pour les conteneurs STL
             * @ingroup MemoryStlAdapters
             */
            template<typename T, typename AllocatorType>
            struct std::allocator_traits<nkentseu::memory::NkStlAdapter<T, AllocatorType>> {
                using allocator_type = nkentseu::memory::NkStlAdapter<T, AllocatorType>;
                using value_type = T;
                using pointer = typename allocator_type::pointer;
                using const_pointer = typename allocator_type::const_pointer;
                using void_pointer = void*;
                using const_void_pointer = const void*;
                using difference_type = typename allocator_type::difference_type;
                using size_type = typename allocator_type::size_type;

                // Méthodes d'allocation
                [[nodiscard]] static pointer allocate(allocator_type& a, size_type n) {
                    return a.allocate(n);
                }
                
                static void deallocate(allocator_type& a, pointer p, size_type n) noexcept {
                    a.deallocate(p, n);
                }

                // Construction/destruction
                template<typename U, typename... Args>
                static void construct(allocator_type& a, U* p, Args&&... args) {
                    a.construct(p, traits::NkForward<Args>(args)...);
                }
                
                template<typename U>
                static void destroy(allocator_type& a, U* p) noexcept {
                    a.destroy(p);
                }

                // Méthodes optionnelles
                [[nodiscard]] static size_type max_size(const allocator_type& a) noexcept {
                    return a.max_size();
                }
                
                [[nodiscard]] static allocator_type select_on_container_copy_construction(
                    const allocator_type& a) {
                    // Par défaut : ne pas propager l'allocateur lors de la copie
                    // Pour propager : retourner a au lieu de allocator_type{}
                    return allocator_type{};
                }

                // Traits de propagation (configurables)
                using propagate_on_container_copy_assignment = std::false_type;
                using propagate_on_container_move_assignment = std::true_type;   // Optimisation : move prend l'allocateur
                using propagate_on_container_swap = std::true_type;              // Swap échange les allocateurs
                using is_always_equal = std::false_type;                         // Dépend du pointeur d'allocateur
            };

            // -------------------------------------------------------------------------
            // SECTION 5 : ALIAS UTILES POUR USAGE COURANT
            // -------------------------------------------------------------------------
            /**
             * @brief Alias pour adaptateur avec NkContainerAllocator (cas d'usage fréquent)
             * @tparam T Type des éléments du conteneur
             * @ingroup MemoryStlAdapters
             * 
             * @example
             * @code
             * nkentseu::memory::NkContainerAllocator containerAlloc;
             * nkentseu::memory::NkContainerVector<int> vec(&containerAlloc);
             * vec.reserve(1000);  // Allocations rapides via size classes
             * @endcode
             */
            template<typename T>
            using NkContainerVector = std::vector<T, NkStlAdapter<T, NkContainerAllocator>>;

            /** @brief Alias pour adaptateur avec NkMallocAllocator (fallback standard) */
            template<typename T>
            using NkMallocVector = std::vector<T, NkStlAdapter<T, NkMallocAllocator>>;

            /** @brief Alias pour adaptateur avec NkPoolAllocator (pour objets de taille fixe) */
            template<typename T, nk_size BlockSize, nk_size NumBlocks = 256>
            using NkPoolVector = std::vector<T, NkStlAdapter<T, NkFixedPoolAllocator<BlockSize, NumBlocks>>>;

        } // namespace memory
    } // namespace nkentseu

#endif // NKENTSEU_MEMORY_NKSTLADAPTER_H

// =============================================================================
// NOTES D'UTILISATION ET BONNES PRATIQUES
// =============================================================================
/*
    [Intégration avec les conteneurs STL]
    
    1. Inclure l'adaptateur et l'allocateur souhaité :
       #include "NKMemory/NkStlAdapter.h"
       #include "NKMemory/NkContainerAllocator.h"
       #include <vector>
    
    2. Déclarer le type de conteneur avec l'adaptateur :
       using MyVector = std::vector<int, nkentseu::memory::NkStlAdapter<int, nkentseu::memory::NkContainerAllocator>>;
    
    3. Créer l'allocateur et le conteneur :
       nkentseu::memory::NkContainerAllocator alloc;
       MyVector vec(&alloc);  // Constructeur prenant l'allocateur
    
    4. Utiliser normalement :
       vec.reserve(1000);
       vec.push_back(42);
    
    [Propagation d'allocateur]
    
    Par défaut, les traits sont configurés pour :
    - propagate_on_container_copy_assignment = false : copie crée un nouveau conteneur avec allocateur défaut
    - propagate_on_container_move_assignment = true : move transfère l'allocateur (optimisation)
    - propagate_on_container_swap = true : swap échange les allocateurs
    
    Pour changer ces comportements, spécialiser std::allocator_traits :
    
        template<typename T, typename A>
        struct std::allocator_traits<nkentseu::memory::NkStlAdapter<T, A>> {
            // ... hériter de la spécialisation par défaut ...
            using propagate_on_container_copy_assignment = std::true_type;  // Propager à la copie
        };
    
    [Performance comparée]
    
    | Conteneur          | std::allocator | NkStlAdapter + NkContainerAllocator | Gain typique |
    |-------------------|----------------|-------------------------------------|--------------|
    | std::vector<int>  | ~85 ns/alloc   | ~15 ns/alloc (TLS hit)              | ~5-6x        |
    | std::list<Node>   | ~120 ns/alloc  | ~25 ns/alloc (size class)           | ~4-5x        |
    | std::unordered_map| ~200 ns/alloc  | ~40 ns/alloc (pool dédié)           | ~5x          |
    
    Note : les gains dépendent du workload ; les allocations fréquentes de petites tailles bénéficient le plus.
    
    [Thread-safety]
    
    ✓ NkStlAdapter est thread-safe si l'allocateur NKMemory sous-jacent l'est
    ✓ Les conteneurs STL ne sont PAS thread-safe : synchronisation externe requise pour accès concurrent
    ✓ Les caches TLS de NkContainerAllocator sont thread-local : pas de contention pour le fast path
    
    ✗ Ne pas partager un conteneur entre threads sans mutex
    ✗ Ne pas détruire l'allocateur NKMemory tant que des conteneurs l'utilisent encore
    
    [Debugging tips]
    
    - Si crash dans allocate : vérifier que l'allocateur NKMemory est initialisé et valide
    - Si fuite mémoire : utiliser NkMemoryTracker ou DumpLeaks() pour identifier les allocations non-libérées
    - Si performance degrade : profiler avec GetStats() pour vérifier le TLS cache hit rate
    
    [Migration depuis std::allocator]
    
    Étape 1 : Remplacer std::allocator<T> par NkStlAdapter<T, YourAllocator>
    Étape 2 : Passer l'instance d'allocateur au constructeur du conteneur
    Étape 3 : Tester en debug avec tracking activé pour valider l'absence de fuites
    Étape 4 : En release, définir NKENTSEU_DISABLE_CONTAINER_DEBUG pour performance maximale
*/

// =============================================================================
// EXEMPLES COMPLETS D'UTILISATION
// =============================================================================
/*
    // Exemple 1 : std::vector avec NkContainerAllocator (cas d'usage fréquent)
    #include "NKMemory/NkStlAdapter.h"
    #include "NKMemory/NkContainerAllocator.h"
    #include <vector>
    #include <iostream>
    
    int main() {
        // Créer l'allocateur conteneur
        nkentseu::memory::NkContainerAllocator containerAlloc;
        
        // Type alias pour lisibilité
        using FastIntVector = std::vector<
            int, 
            nkentseu::memory::NkStlAdapter<int, nkentseu::memory::NkContainerAllocator>
        >;
        
        // Créer le vecteur avec l'allocateur
        FastIntVector vec(&containerAlloc);
        vec.reserve(10000);  // Pré-allocation rapide via size classes
        
        // Utilisation normale
        for (int i = 0; i < 10000; ++i) {
            vec.push_back(i * 2);
        }
        
        std::cout << "Vector size: " << vec.size() << ", capacity: " << vec.capacity() << "\n";
        
        // Stats pour monitoring
        auto stats = containerAlloc.GetStats();
        std::cout << "Container allocator: " << stats.SmallUsedBlocks << " small blocks used\n";
        
        return 0;  // Destruction automatique : vec libère sa mémoire via l'adaptateur
    }
    
    // Exemple 2 : std::unordered_map avec NkFixedPoolAllocator (objets de taille fixe)
    #include "NKMemory/NkStlAdapter.h"
    #include "NKMemory/NkPoolAllocator.h"
    #include <unordered_map>
    
    struct PlayerData {
        int score;
        float health;
        // ... autres champs ...
    };
    
    int main() {
        // Pool pour paires<const int, PlayerData>
        constexpr nk_size PairSize = sizeof(std::pair<const int, PlayerData>);
        nkentseu::memory::NkFixedPoolAllocator<PairSize, 4096> pairPool("PlayerMapPool");
        
        // Type alias complexe mais réutilisable
        using PlayerMap = std::unordered_map<
            int,  // Key type
            PlayerData,  // Value type
            std::hash<int>,  // Hasher
            std::equal_to<int>,  // Key equality
            nkentseu::memory::NkStlAdapter<  // Allocator adapter
                std::pair<const int, PlayerData>, 
                decltype(pairPool)
            >
        >;
        
        // Créer la map avec l'allocateur pool
        PlayerMap players(&pairPool);
        
        // Insertion rapide via pool (pas de fragmentation)
        players[101] = {1500, 100.0f};
        players[102] = {2300, 85.5f};
        
        // Accès normal
        if (auto it = players.find(101); it != players.end()) {
            std::cout << "Player 101 score: " << it->second.score << "\n";
        }
        
        return 0;
    }
    
    // Exemple 3 : Conteneur personnalisé avec propagation d'allocateur
    template<typename T>
    class MyCustomContainer {
    public:
        using allocator_type = nkentseu::memory::NkStlAdapter<T, nkentseu::memory::NkContainerAllocator>;
        
        explicit MyCustomContainer(allocator_type alloc = {}) : mAlloc(alloc), mData(&alloc.GetAllocator()) {}
        
        void Add(const T& value) {
            // allocate via adaptateur
            T* ptr = std::allocator_traits<allocator_type>::allocate(mAlloc, 1);
            std::allocator_traits<allocator_type>::construct(mAlloc, ptr, value);
            mData.push_back(ptr);  // Stocker les pointeurs
        }
        
        ~MyCustomContainer() {
            for (T* ptr : mData) {
                std::allocator_traits<allocator_type>::destroy(mAlloc, ptr);
                std::allocator_traits<allocator_type>::deallocate(mAlloc, ptr, 1);
            }
        }
        
    private:
        allocator_type mAlloc;
        std::vector<T*> mData;  // Stockage interne avec même allocateur
    };
    
    // Exemple 4 : Swap de conteneurs avec propagation d'allocateur
    void SwapExample() {
        nkentseu::memory::NkContainerAllocator alloc1, alloc2;
        
        using FastString = std::string, nkentseu::memory::NkStlAdapter<char, nkentseu::memory::NkContainerAllocator>>;
        
        FastString s1(&alloc1), s2(&alloc2);
        s1 = "Hello";
        s2 = "World";
        
        // Swap : échange les buffers ET les allocateurs (propagate_on_container_swap = true)
        using std::swap;
        swap(s1, s2);
        
        // Maintenant s1 utilise alloc2, s2 utilise alloc1
        // Les données sont échangées sans réallocation (move semantics)
    }
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================