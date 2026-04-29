// =============================================================================
// NKMemory/NkHash.h
// Conteneurs de hachage performants : HashSet et HashMap à clés pointeurs.
//
// Design :
//  - Séparation claire déclarations (.h) / implémentations (.cpp)
//  - Réutilisation des allocateurs NKMemory (ZÉRO duplication de logique d'alloc)
//  - Hashing optimisé (MurmurHash3 finalizer) pour distribution uniforme
//  - Open addressing avec linear probing + tombstones pour suppression O(1)
//  - Capacité toujours puissance de 2 : masking au lieu de modulo pour performance
//  - Non thread-safe par défaut : responsabilité de l'appelant pour synchronisation
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_MEMORY_NKHASH_H
#define NKENTSEU_MEMORY_NKHASH_H

    // -------------------------------------------------------------------------
    // SECTION 1 : DÉPENDANCES
    // -------------------------------------------------------------------------
    #include "NKMemory/NkMemoryApi.h"      // NKENTSEU_MEMORY_API
    #include "NKMemory/NkAllocator.h"      // NkAllocator pour gestion mémoire
    #include "NKCore/NkTypes.h"            // nk_size, nk_uptr, nk_bool, etc.

    // -------------------------------------------------------------------------
    // SECTION 2 : CONFIGURATION DU MODULE HASH
    // -------------------------------------------------------------------------
    /**
     * @defgroup MemoryHash Conteneurs de Hachage Mémoire
     * @brief HashSet et HashMap optimisés pour clés de type pointeur (void*)
     * 
     * Caractéristiques :
     *  - Clés : pointeurs bruts (const void*), comparaison par égalité de pointeur
     *  - Valeurs (HashMap) : pointeurs bruts (void*) pour flexibilité maximale
     *  - Hashing : fonction MurmurHash3 finalizer pour distribution uniforme
     *  - Résolution de collisions : linear probing avec tombstones pour suppression
     *  - Capacité : toujours puissance de 2 → masking (index & (cap-1)) au lieu de modulo
     *  - Load factor : 70% max avant rehash, 40% de tombstones max avant cleanup
     *  - Allocation : via NkAllocator personnalisé ou malloc par défaut
     * 
     * @note Non thread-safe : synchronisation externe requise pour usage concurrent
     * @note Clés doivent être stables : ne pas modifier l'objet pointé pendant qu'il est dans la table
     * @note Suppression marque avec tombstone : mémoire libérée seulement au rehash
     * 
     * @example HashSet
     * @code
     * nkentseu::memory::NkPointerHashSet trackedPtrs;
     * trackedPtrs.Initialize(128);  // Capacité initiale
     * 
     * void* ptr = malloc(1024);
     * trackedPtrs.Insert(ptr);      // O(1) moyen
     * 
     * if (trackedPtrs.Contains(ptr)) {
     *     // ... utilisation ...
     * }
     * 
     * trackedPtrs.Erase(ptr);       // O(1) moyen (tombstone)
     * free(ptr);
     * @endcode
     * 
     * @example HashMap
     * @code
     * nkentseu::memory::NkPointerHashMap resourceMap;
     * resourceMap.Initialize(64);
     * 
     * // Insertion
     * resourceMap.Insert(texturePtr, metadataPtr);
     * 
     * // Lookup
     * if (void* meta = resourceMap.Find(texturePtr)) {
     *     // Utiliser metadata...
     * }
     * 
     * // Suppression avec récupération de valeur
     * void* oldMeta = nullptr;
     * resourceMap.Erase(texturePtr, &oldMeta);
     * @endcode
     */

    namespace nkentseu {
        namespace memory {

            // -------------------------------------------------------------------------
            // SECTION 3 : UTILITAIRES DE HACHAGE COMMUNS (Internes)
            // -------------------------------------------------------------------------
            /**
             * @brief Fonctions utilitaires de hachage partagées entre HashSet/HashMap
             * @note Implémentation dans NkHash.cpp, déclarées ici pour cohérence
             * @ingroup MemoryHashInternal
             */
            namespace detail {
                /**
                 * @brief Fonction de hachage pour pointeurs (MurmurHash3 finalizer)
                 * @param pointer Pointeur à hasher
                 * @return Hash value dans [0, SIZE_MAX]
                 * @note Distribution uniforme même pour adresses alignées
                 */
                NKENTSEU_MEMORY_API 
                nk_size HashPointer(const void* pointer) noexcept;

                /**
                 * @brief Calcule la prochaine puissance de 2 >= value
                 * @param value Valeur d'entrée
                 * @return Plus petite puissance de 2 >= value
                 * @note Utilisé pour garantir capacité = puissance de 2
                 */
                NKENTSEU_MEMORY_API 
                nk_size NextPow2(nk_size value) noexcept;

                /**
                 * @brief Valeur sentinelle pour marquer une entrée supprimée (tombstone)
                 * @return Pointeur spécial utilisé comme marqueur de suppression
                 * @note Jamais égal à un pointeur utilisateur valide (adresse 0x1)
                 */
                NKENTSEU_MEMORY_API 
                const void* TombstoneKey() noexcept;

                /**
                 * @brief Valeur sentinelle pour index invalide
                 * @return SIZE_MAX (toutes bits à 1)
                 */
                NKENTSEU_MEMORY_API 
                nk_size InvalidIndex() noexcept;
            } // namespace detail

            // -------------------------------------------------------------------------
            // SECTION 4 : CLASSE NkPointerHashSet
            // -------------------------------------------------------------------------
            /**
             * @class NkPointerHashSet
             * @brief HashSet optimisé pour clés de type pointeur (const void*)
             * @ingroup MemoryHash
             * 
             * Caractéristiques :
             *  - Clés : pointeurs bruts, comparaison par égalité d'adresse (==)
             *  - Opérations : Insert/Contains/Erase en O(1) moyen
             *  - Mémoire : tableau contigu de pointeurs, cache-friendly
             *  - Suppression : tombstone marking + rehash périodique pour cleanup
             *  - Capacité : auto-resize à 70% de load factor, toujours puissance de 2
             * 
             * @note Non thread-safe : protéger avec mutex externe si usage concurrent
             * @note Les clés doivent rester valides pendant leur présence dans le set
             * @note Utilise NkAllocator pour toutes les allocations internes
             * 
             * @example
             * @code
             * // Création et initialisation
             * nkentseu::memory::NkPointerHashSet ptrSet;
             * if (!ptrSet.Initialize(128)) {
             *     // Gestion d'erreur...
             * }
             * 
             * // Insertion
             * void* obj = CreateObject();
             * ptrSet.Insert(obj);  // Ignore les doublons
             * 
             * // Lookup
             * if (ptrSet.Contains(obj)) {
             *     ProcessObject(obj);
             * }
             * 
             * // Suppression
             * ptrSet.Erase(obj);  // Marque comme tombstone
             * 
             * // Stats
             * printf("Size: %zu / Capacity: %zu\n", 
             *        ptrSet.Size(), ptrSet.Capacity());
             * 
             * // Cleanup
             * ptrSet.Shutdown();  // Libère la mémoire interne
             * @endcode
             */
            class NKENTSEU_MEMORY_CLASS_EXPORT NkPointerHashSet {
                public:
                    // =================================================================
                    // @name Constructeur / Destructeur
                    // =================================================================
                    
                    /**
                     * @brief Constructeur avec allocateur optionnel
                     * @param allocator Allocateur à utiliser pour les buffers internes
                     * @note L'allocateur est stocké mais Initialize() doit être appelé pour allouer
                     */
                    explicit NkPointerHashSet(NkAllocator* allocator = nullptr) noexcept;
                    
                    /**
                     * @brief Destructeur : appelle Shutdown() pour libérer la mémoire
                     * @note Safe : vérifie nullptr avant deallocation
                     */
                    ~NkPointerHashSet();

                    // Non-copiable : sémantique de possession unique des buffers
                    NkPointerHashSet(const NkPointerHashSet&) = delete;
                    NkPointerHashSet& operator=(const NkPointerHashSet&) = delete;

                    // =================================================================
                    // @name Initialisation et Cycle de Vie
                    // =================================================================
                    
                    /**
                     * @brief Alloue et initialise la table de hachage
                     * @param initialCapacity Capacité initiale souhaitée (arrondie à puissance de 2)
                     * @param allocator Allocateur à utiliser (override du constructeur si fourni)
                     * @return true si succès, false si échec d'allocation
                     * @note Idempotent : appelle répétés sont no-op si déjà initialisé
                     * @note Capacité minimale : 16 entrées
                     */
                    nk_bool Initialize(nk_size initialCapacity = 64u, 
                                    NkAllocator* allocator = nullptr) noexcept;

                    /**
                     * @brief Libère toute la mémoire interne et réinitialise l'état
                     * @note Safe : no-op si déjà shutdown ou non-initialisé
                     * @note Après appel : IsInitialized() retourne false
                     */
                    void Shutdown() noexcept;

                    // =================================================================
                    // @name Gestion de Capacité
                    // =================================================================
                    
                    /**
                     * @brief Réserve de la capacité pour au moins N éléments sans rehash
                     * @param requestedCapacity Nombre minimum d'éléments à supporter
                     * @return true si succès, false si échec d'allocation
                     * @note Calcule la capacité cible avec 50% de marge : target = size + size/2 + 1
                     * @note No-op si capacité actuelle suffisante et peu de tombstones
                     */
                    nk_bool Reserve(nk_size requestedCapacity) noexcept;

                    /**
                     * @brief Supprime toutes les entrées mais conserve la capacité allouée
                     * @note Plus rapide que Shutdown+Initialize si réutilisation prévue
                     * @note Réinitialise Size() à 0 et le compteur de tombstones
                     */
                    void Clear() noexcept;

                    // =================================================================
                    // @name Opérations sur les Éléments
                    // =================================================================
                    
                    /**
                     * @brief Insère une clé dans le set (ignore les doublons)
                     * @param key Pointeur à insérer (ne doit pas être nullptr ni tombstone)
                     * @return true si inséré (nouveau), false si déjà présent ou erreur
                     * @note Déclenche un rehash automatique si load factor > 70%
                     * @note O(1) moyen, O(n) worst-case (très rare avec bon hashing)
                     */
                    nk_bool Insert(const void* key) noexcept;

                    /**
                     * @brief Teste si une clé est présente dans le set
                     * @param key Pointeur à rechercher
                     * @return true si trouvé, false sinon
                     * @note O(1) moyen, ne modifie pas le set
                     */
                    [[nodiscard]] 
                    nk_bool Contains(const void* key) const noexcept;

                    /**
                     * @brief Supprime une clé du set (marquage tombstone)
                     * @param key Pointeur à supprimer
                     * @return true si supprimé (était présent), false si absent
                     * @note Marque l'entrée avec tombstone : mémoire libérée au prochain rehash
                     * @note Déclenche un cleanup rehash si tombstones > 40% de capacity
                     */
                    nk_bool Erase(const void* key) noexcept;

                    // =================================================================
                    // @name Interrogation d'État
                    // =================================================================
                    
                    /** @brief Teste si la structure est initialisée (buffer alloué) */
                    [[nodiscard]] nk_bool IsInitialized() const noexcept { 
                        return mKeys != nullptr; 
                    }
                    
                    /** @brief Nombre d'éléments actuellement stockés */
                    [[nodiscard]] nk_size Size() const noexcept { 
                        return mSize; 
                    }
                    
                    /** @brief Capacité totale de la table (puissance de 2) */
                    [[nodiscard]] nk_size Capacity() const noexcept { 
                        return mCapacity; 
                    }
                    
                    /** @brief Ratio d'occupation actuel : Size() / Capacity() */
                    [[nodiscard]] float LoadFactor() const noexcept {
                        return (mCapacity > 0) ? static_cast<float>(mSize) / static_cast<float>(mCapacity) : 0.0f;
                    }

                private:
                    // =================================================================
                    // @name Implémentation interne (détaillée dans NkHash.cpp)
                    // =================================================================
                    
                    /**
                     * @brief Redimensionne et réhache toute la table
                     * @param requestedCapacity Nouvelle capacité cible
                     * @return true si succès, false si échec d'allocation
                     * @note Copie toutes les entrées valides dans un nouveau buffer
                     * @note Libère l'ancien buffer après migration réussie
                     */
                    nk_bool Rehash(nk_size requestedCapacity) noexcept;

                    /**
                     * @brief Recherche l'index d'une clé ou d'un slot libre
                     * @param key Clé à rechercher
                     * @return Index du slot si trouvé, InvalidIndex() sinon
                     * @note Utilise linear probing avec wrapping (masking)
                     */
                    [[nodiscard]] 
                    nk_size FindSlot(const void* key) const noexcept;

                    // =================================================================
                    // @name Membres privés
                    // =================================================================
                    
                    /** @brief Tableau contigu des clés (pointeurs) */
                    const void** mKeys;
                    
                    /** @brief Capacité du tableau (toujours puissance de 2) */
                    nk_size mCapacity;
                    
                    /** @brief Nombre d'éléments valides actuellement stockés */
                    nk_size mSize;
                    
                    /** @brief Nombre d'entrées marquées comme supprimées (tombstones) */
                    nk_size mTombstones;
                    
                    /** @brief Allocateur utilisé pour les buffers internes */
                    NkAllocator* mAllocator;
            };

            // -------------------------------------------------------------------------
            // SECTION 5 : CLASSE NkPointerHashMap
            // -------------------------------------------------------------------------
            /**
             * @class NkPointerHashMap
             * @brief HashMap optimisé pour clés/valeurs de type pointeur (void* → void*)
             * @ingroup MemoryHash
             * 
             * Caractéristiques :
             *  - Clés : pointeurs bruts (const void*), comparaison par égalité d'adresse
             *  - Valeurs : pointeurs bruts (void*) pour flexibilité maximale
             *  - Entry struct : {Key, Value} contigus en mémoire pour cache locality
             *  - Mêmes performances et garanties que NkPointerHashSet
             * 
             * @note Non thread-safe : synchronisation externe requise pour usage concurrent
             * @note Les clés doivent rester stables pendant leur présence dans la map
             * @note Les valeurs ne sont PAS possédées : responsabilité de l'appelant pour leur cycle de vie
             * 
             * @example
             * @code
             * // Création
             * nkentseu::memory::NkPointerHashMap cache;
             * cache.Initialize(256);
             * 
             * // Insertion
             * cache.Insert(resourceKey, resourceData);
             * 
             * // Lookup avec fallback
             * if (void* data = cache.Find(resourceKey)) {
             *     UseResource(data);
             * } else {
             *     data = LoadResource(resourceKey);  // Cache miss
             *     cache.Insert(resourceKey, data);   // Populate cache
             * }
             * 
             * // Lookup avec output parameter (évite nullptr ambiguity)
             * void* value = nullptr;
             * if (cache.TryGet(resourceKey, &value) && value != nullptr) {
             *     // value est valide et non-null
             * }
             * 
             * // Suppression avec récupération de valeur
             * void* oldData = nullptr;
             * if (cache.Erase(resourceKey, &oldData)) {
             *     FreeResource(oldData);  // Cleanup si nécessaire
             * }
             * @endcode
             */
            class NKENTSEU_MEMORY_CLASS_EXPORT NkPointerHashMap {
                public:
                    // =================================================================
                    // @name Types publics
                    // =================================================================
                    
                    /**
                     * @struct NkEntry
                     * @brief Paire clé/valeur stockée dans la hash table
                     * @ingroup MemoryHash
                     */
                    struct NkEntry {
                        const void* Key;   ///< Clé de l'entrée (pointeur)
                        void* Value;       ///< Valeur associée (pointeur)
                        
                        /** @brief Constructeur par défaut : nullptr pour les deux */
                        constexpr NkEntry() noexcept : Key(nullptr), Value(nullptr) {}
                        
                        /** @brief Constructeur avec valeurs */
                        constexpr NkEntry(const void* key, void* value) noexcept 
                            : Key(key), Value(value) {}
                    };

                    // =================================================================
                    // @name Constructeur / Destructeur
                    // =================================================================
                    
                    explicit NkPointerHashMap(NkAllocator* allocator = nullptr) noexcept;
                    ~NkPointerHashMap();

                    NkPointerHashMap(const NkPointerHashMap&) = delete;
                    NkPointerHashMap& operator=(const NkPointerHashMap&) = delete;

                    // =================================================================
                    // @name Initialisation et Cycle de Vie
                    // =================================================================
                    
                    nk_bool Initialize(nk_size initialCapacity = 64u, NkAllocator* allocator = nullptr) noexcept;
                    
                    void Shutdown() noexcept;

                    // =================================================================
                    // @name Gestion de Capacité
                    // =================================================================
                    
                    nk_bool Reserve(nk_size requestedCapacity) noexcept;
                    
                    void Clear() noexcept;

                    // =================================================================
                    // @name Opérations sur les Éléments
                    // =================================================================
                    
                    /**
                     * @brief Insère ou met à jour une paire clé/valeur
                     * @param key Clé à insérer/mettre à jour
                     * @param value Valeur à associer
                     * @return true si inséré (nouvelle clé), false si mise à jour ou erreur
                     * @note Si la clé existe déjà : met à jour la valeur et retourne false
                     * @note Déclenche rehash automatique si load factor > 70%
                     */
                    nk_bool Insert(const void* key, void* value) noexcept;

                    /**
                     * @brief Recherche une valeur par clé
                     * @param key Clé à rechercher
                     * @return Pointeur vers la valeur si trouvée, nullptr sinon
                     * @note Ambiguïté : nullptr peut signifier "non trouvé" OU "valeur stockée = nullptr"
                     * @note Pour distinguer : utiliser TryGet() avec output parameter
                     * @note O(1) moyen, lecture seule
                     */
                    [[nodiscard]] 
                    void* Find(const void* key) const noexcept;

                    /**
                     * @brief Recherche une valeur avec distinction explicite du résultat
                     * @param key Clé à rechercher
                     * @param outValue Pointeur vers pointeur pour recevoir la valeur
                     * @return true si clé trouvée (outValue contient la valeur, peut être nullptr), false sinon
                     * @note Permet de distinguer "clé absente" de "valeur stockée = nullptr"
                     * @note outValue est toujours écrit : nullptr si non trouvé, valeur sinon
                     */
                    [[nodiscard]] 
                    nk_bool TryGet(const void* key, void** outValue) const noexcept;

                    /**
                     * @brief Teste si une clé est présente (sans récupérer la valeur)
                     * @param key Clé à tester
                     * @return true si présente, false sinon
                     * @note Plus rapide que Find() si seule la présence importe
                     */
                    [[nodiscard]] 
                    nk_bool Contains(const void* key) const noexcept;

                    /**
                     * @brief Supprime une entrée et optionnellement retourne sa valeur
                     * @param key Clé à supprimer
                     * @param outValue Optionnel : pointeur vers pointeur pour recevoir l'ancienne valeur
                     * @return true si supprimé (était présent), false si absent
                     * @note Si outValue != nullptr : écrit la valeur avant suppression
                     * @note Marque l'entrée avec tombstone : cleanup au prochain rehash si >40%
                     */
                    nk_bool Erase(const void* key, void** outValue = nullptr) noexcept;

                    // =================================================================
                    // @name Interrogation d'État
                    // =================================================================
                    
                    [[nodiscard]] nk_bool IsInitialized() const noexcept { 
                        return mEntries != nullptr; 
                    }
                    [[nodiscard]] nk_size Size() const noexcept { 
                        return mSize; 
                    }
                    [[nodiscard]] nk_size Capacity() const noexcept { 
                        return mCapacity; 
                    }
                    [[nodiscard]] float LoadFactor() const noexcept {
                        return (mCapacity > 0) ? static_cast<float>(mSize) / static_cast<float>(mCapacity) : 0.0f;
                    }

                private:
                    // =================================================================
                    // @name Implémentation interne
                    // =================================================================
                    
                    nk_bool Rehash(nk_size requestedCapacity) noexcept;
                    
                    [[nodiscard]] 
                    nk_size FindSlot(const void* key) const noexcept;

                    // =================================================================
                    // @name Membres privés
                    // =================================================================
                    
                    /** @brief Tableau contigu des entrées {Key, Value} */
                    NkEntry* mEntries;
                    
                    /** @brief Capacité du tableau (puissance de 2) */
                    nk_size mCapacity;
                    
                    /** @brief Nombre d'entrées valides */
                    nk_size mSize;
                    
                    /** @brief Nombre d'entrées tombstones */
                    nk_size mTombstones;
                    
                    /** @brief Allocateur pour les buffers internes */
                    NkAllocator* mAllocator;
            };

        } // namespace memory
    } // namespace nkentseu

#endif // NKENTSEU_MEMORY_NKHASH_H

// =============================================================================
// NOTES DE PERFORMANCE
// =============================================================================
/*
    [Complexité Algorithmique]
    - Insert/Contains/Erase/Find : O(1) moyen, O(n) worst-case (très rare)
    - Rehash : O(n) mais amorti sur de nombreuses opérations → O(1) amorti
    - Clear : O(capacity) pour zero-initialization, mais pas de deallocation
    - Shutdown : O(1) pour deallocation du buffer contigu

    [Facteurs de Performance]
    ✓ Capacité puissance de 2 : masking (index & (cap-1)) plus rapide que modulo
    ✓ Tableau contigu : excellente cache locality, prefetching CPU efficace
    ✓ Linear probing : pas de pointeurs indirects, accès mémoire séquentiel
    ✓ Hashing MurmurHash3 : distribution uniforme même pour adresses alignées
    ✓ Tombstones : suppression O(1) sans réorganisation immédiate

    [Tuning des Paramètres]
    - Load factor max (70%) : compromis entre mémoire et performance de lookup
    - Tombstone threshold (40%) : évite la dégradation progressive des lookups
    - Capacité initiale : choisir proche de l'usage attendu pour éviter rehash précoces
    - Minimum capacity (16) : évite les tables trop petites inefficaces

    [Comparaison avec std::unordered_set/map]
    | Critère          | NkPointerHashSet/Map    | std::unordered_*        |
    |-----------------|------------------------|------------------------|
    | Type de clé     | void* uniquement       | Template (n'importe quoi) |
    | Allocation      | NkAllocator personnalisé | std::allocator (moins flexible) |
    | Mémoire         | Tableau contigu        | Buckets + listes chaînées |
    | Cache locality  | Excellente             | Moyenne (indirections) |
    | Suppression     | Tombstone + cleanup    | Suppression directe |
    | Thread-safety   | Non (externe)          | Non (externe) |
    | Dépendances     | NKCore/NKMemory        | STL standard |

    [Quand utiliser ces conteneurs]
    ✓ Clés sont des pointeurs bruts (adresse mémoire comme identifiant)
    ✓ Performance critique : besoin de cache locality et hashing optimisé
    ✓ Allocation mémoire personnalisée requise (pools, arenas, tracking)
    ✓ Valeurs sont aussi des pointeurs (pas de copie/coût de construction)

    [Quand éviter]
    ✗ Clés sont des valeurs (int, string, struct) : utiliser std::unordered_* ou templates génériques
    ✗ Besoin de thread-safety intégrée : ajouter mutex externe ou utiliser conteneur concurrent
    ✗ Valeurs nécessitent gestion de cycle de vie complexe : envisager smart pointers
*/

// =============================================================================
// EXEMPLES D'UTILISATION AVANCÉS
// =============================================================================
/*
    // Exemple 1 : Cache LRU simplifié avec HashMap
    class SimpleCache {
    public:
        SimpleCache(nk_size maxSize) : mMaxSize(maxSize) {
            mMap.Initialize(maxSize * 2);  // Marge pour éviter rehash fréquents
        }
        
        void* Get(const void* key) {
            return mMap.Find(key);  // Lookup O(1)
        }
        
        void Put(const void* key, void* value) {
            // Si plein, supprimer une entrée arbitraire (stratégie simple)
            if (mMap.Size() >= mMaxSize) {
                // Note: pour vrai LRU, utiliser liste double + map vers nodes
                mMap.Erase(mMap.mEntries[0].Key);  // Supprime première entrée
            }
            mMap.Insert(key, value);
        }
        
        void Remove(const void* key) {
            mMap.Erase(key);
        }
        
    private:
        nk_size mMaxSize;
        nkentseu::memory::NkPointerHashMap mMap;
    };

    // Exemple 2 : Tracking d'objets avec HashSet + cleanup périodique
    class ObjectTracker {
    public:
        void Register(void* obj) {
            mTracked.Insert(obj);
        }
        
        void Unregister(void* obj) {
            mTracked.Erase(obj);
        }
        
        void Cleanup() {
            // Supprime les objets "orphelins" selon logique métier
            // Exemple: itération + test de validité + erase si invalid
            // Note: itération directe non supportée → maintenir liste parallèle si besoin
        }
        
        nk_size Count() const { return mTracked.Size(); }
        
    private:
        nkentseu::memory::NkPointerHashSet mTracked;
    };

    // Exemple 3 : Multiplexeur de ressources avec HashMap
    class ResourceMultiplexer {
    public:
        bool RegisterResource(void* id, void* resource) {
            return mResources.Insert(id, resource);
        }
        
        void* GetResource(void* id) const {
            return mResources.Find(id);
        }
        
        bool UnregisterResource(void* id, void** outResource = nullptr) {
            return mResources.Erase(id, outResource);
        }
        
        // Thread-safe wrapper (si usage concurrent)
        void* GetResourceThreadSafe(void* id, NkMutex& lock) const {
            NkLockGuard guard(lock);
            return mResources.Find(id);
        }
        
    private:
        nkentseu::memory::NkPointerHashMap mResources;
    };

    // Exemple 4 : Benchmark de performance
    void HashBenchmark() {
        constexpr nk_size NUM_ITEMS = 100000;
        nkentseu::memory::NkPointerHashSet set;
        set.Initialize(NUM_ITEMS);
        
        // Allocation de clés de test
        void** keys = static_cast<void**>(malloc(NUM_ITEMS * sizeof(void*)));
        for (nk_size i = 0; i < NUM_ITEMS; ++i) {
            keys[i] = malloc(64);  // Clés distinctes
        }
        
        // Benchmark insertion
        nk_uint64 t0 = NkGetTimestamp();
        for (nk_size i = 0; i < NUM_ITEMS; ++i) {
            set.Insert(keys[i]);
        }
        nk_uint64 t1 = NkGetTimestamp();
        
        // Benchmark lookup
        nk_size found = 0;
        for (nk_size i = 0; i < NUM_ITEMS; ++i) {
            if (set.Contains(keys[i])) ++found;
        }
        nk_uint64 t2 = NkGetTimestamp();
        
        printf("Insert: %zu items in %.2f ms (%.0f ns/insert)\n",
               NUM_ITEMS, (t1-t0)/1e6, static_cast<double>(t1-t0)/NUM_ITEMS);
        printf("Lookup: %zu/%zu found in %.2f ms (%.0f ns/lookup)\n",
               found, NUM_ITEMS, (t2-t1)/1e6, static_cast<double>(t2-t1)/NUM_ITEMS);
        
        // Cleanup
        for (nk_size i = 0; i < NUM_ITEMS; ++i) free(keys[i]);
        free(keys);
        set.Shutdown();
    }
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================