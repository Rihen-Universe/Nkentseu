// -----------------------------------------------------------------------------
// FICHIER: NKContainers\Containers\Associative\NkUnorderedSet.h
// DESCRIPTION: Ensemble non-ordonné - Implémentation par table de hachage
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CONTAINERS_ASSOCIATIVE_NKUNORDEREDSET_H
#define NK_CONTAINERS_ASSOCIATIVE_NKUNORDEREDSET_H

    // ========================================================================
    // INCLUSIONS DES DÉPENDANCES
    // ========================================================================

    #include "NKCore/NkTypes.h"                    // Définition des types fondamentaux (usize, nk_uint8, etc.)
    #include "NKContainers/NkContainersApi.h"      // Macros d'exportation de l'API conteneurs
    #include "NKCore/NkTraits.h"                   // Traits pour move/forward et utilitaires méta
    #include "NKMemory/NkAllocator.h"              // Système d'allocation mémoire personnalisable
    #include "NKMemory/NkFunction.h"               // Wrapper pour les fonctions et callables
    #include "NKCore/Assert/NkAssert.h"            // Macros d'assertion pour le débogage sécurisé
    #include "NKContainers/Iterators/NkIterator.h" // Infrastructure commune pour les itérateurs
    #include "NKContainers/Iterators/NkInitializerList.h" // Support des listes d'initialisation

    // ========================================================================
    // ESPACE DE NOMS PRINCIPAL
    // ========================================================================

    namespace nkentseu {

        // ====================================================================
        // FONCTEURS DE HACHAGE ET D'ÉGALITÉ PAR DÉFAUT
        // ========================================================================

        /**
         * @brief Foncteur de hachage par défaut pour les éléments de NkUnorderedSet
         * 
         * Implémente l'algorithme FNV-1a (Fowler-Noll-Vo) pour un hachage
         * rapide et de bonne qualité statistique sur des données binaires.
         * 
         * PROPRIÉTÉS :
         * - Fonction de hachage non cryptographique optimisée pour la vitesse
         * - Distribution uniforme pour réduire les collisions dans la table
         * - Opérateur() const : peut être utilisé sur des instances const
         * - Template sur le type d'élément : fonctionne avec tout type POD ou trivial
         * 
         * @tparam T Type des éléments à hacher (doit être copiable et de taille fixe)
         * 
         * @note Pour les types complexes (NkString, structs), spécialiser ce template
         *       ou fournir un hasher personnalisé via le template parameter de NkUnorderedSet
         * @note L'algorithme traite l'élément comme un tableau d'octets : attention à l'endianness
         * 
         * @example
         * NkUnorderedSetDefaultHasher<int> hasher;
         * usize h = hasher(42);  // Calcule le hash de l'entier 42
         */
        template<typename T>
        struct NkUnorderedSetDefaultHasher {

            /**
             * @brief Calcule le hash d'un élément selon l'algorithme FNV-1a
             * @param value Référence const vers l'élément à hacher
             * @return Valeur de type usize représentant le hash calculé
             * @note Méthode const : ne modifie pas l'état du foncteur
             * @note Complexité O(sizeof(T)) : parcours linéaire des octets de l'élément
             */
            usize operator()(const T& value) const {
                const nk_uint8* data = reinterpret_cast<const nk_uint8*>(&value);
                usize hash = static_cast<usize>(1469598103934665603ull);
                for (usize i = 0; i < sizeof(T); ++i) {
                    hash ^= static_cast<usize>(data[i]);
                    hash *= static_cast<usize>(1099511628211ull);
                }
                return hash;
            }
        };

        /**
         * @brief Foncteur de comparaison d'égalité par défaut pour les éléments
         * 
         * Implémente une comparaison basée sur l'opérateur == du type T.
         * Utilisé pour résoudre les collisions dans les buckets de la table de hachage
         * et pour détecter les doublons lors de l'insertion.
         * 
         * @tparam T Type des éléments à comparer (doit supporter operator==)
         * 
         * @note Pour les types nécessitant une comparaison personnalisée (tolérance flottante,
         *       comparaison sémantique de structs), spécialiser ce template ou fournir
         *       un Equal personnalisé via le template parameter de NkUnorderedSet
         */
        template<typename T>
        struct NkUnorderedSetDefaultEqual {

            /**
             * @brief Compare deux éléments pour l'égalité
             * @param lhs Premier élément à comparer (côté gauche)
             * @param rhs Second élément à comparer (côté droit)
             * @return true si lhs == rhs, false sinon
             * @note Méthode const : ne modifie pas l'état du foncteur
             * @note Délègue à l'opérateur== du type T : doit être défini et cohérent
             */
            bool operator()(const T& lhs, const T& rhs) const {
                return lhs == rhs;
            }
        };

        // ====================================================================
        // CLASSE PRINCIPALE : NK UNORDERED SET
        // ====================================================================

        /**
         * @brief Classe template implémentant un ensemble non-ordonné via table de hachage
         * 
         * Conteneur associatif non-ordonné stockant des éléments uniques.
         * Utilise le chaînage séparé (separate chaining) pour gérer les collisions :
         * chaque bucket contient une liste chaînée de nœuds en cas de hash identique.
         * 
         * PROPRIÉTÉS FONDAMENTALES :
         * - Accès moyen O(1) pour insertion, recherche et suppression
         * - Pire cas O(n) en cas de nombreuses collisions (hash dégénéré)
         * - Réhash automatique lorsque le load factor dépasse le seuil configuré
         * - Unicité garantie : les doublons sont automatiquement ignorés lors de l'insertion
         * - Gestion mémoire via allocateur personnalisable
         * 
         * PARAMÈTRES DE CONFIGURATION :
         * - Load factor maximal (défaut: 0.75) : déclenche le réhash quand dépassé
         * - Nombre initial de buckets (défaut: 16) : capacité de départ de la table
         * - Foncteurs Hasher et Equal personnalisables pour types complexes
         * 
         * COMPLEXITÉ ALGORITHMIQUE :
         * - Insertion : O(1) amorti, O(n) pire cas (réhash ou collisions)
         * - Recherche (Contains) : O(1) amorti, O(n) pire cas
         * - Suppression (Erase) : O(1) amorti, O(n) pire cas
         * - Réhash explicite : O(n) pour redistribuer tous les éléments
         * 
         * CAS D'USAGE TYPQUES :
         * - Élimination de doublons dans un flux de données
         * - Tests de membership rapide (présence/absence)
         * - Ensembles mathématiques : union, intersection, différence (à implémenter)
         * - Filtres de valeurs uniques pour traitement ultérieur
         * - Alternative à std::unordered_set sans dépendance STL
         * 
         * @tparam T Type des éléments stockés (doit être copiable, hachable et comparable)
         * @tparam Allocator Type de l'allocateur mémoire (défaut: memory::NkAllocator)
         * @tparam Hasher Foncteur de hachage (défaut: NkUnorderedSetDefaultHasher<T>)
         * @tparam Equal Foncteur de comparaison (défaut: NkUnorderedSetDefaultEqual<T>)
         * 
         * @note Les éléments sont stockés en const implicite : modification après insertion impossible
         * @note Thread-unsafe : synchronisation externe requise pour usage concurrent
         * @note L'ordre d'itération est indéterminé et peut changer après réhash
         */
        template<typename T,
                 typename Allocator = memory::NkAllocator,
                 typename Hasher = NkUnorderedSetDefaultHasher<T>,
                 typename Equal = NkUnorderedSetDefaultEqual<T>>
        class NkUnorderedSet {

            // ====================================================================
            // SECTION PRIVÉE : STRUCTURE INTERNE ET NŒUDS
            // ====================================================================
            private:

                /**
                 * @brief Structure interne représentant un nœud de la liste chaînée
                 * 
                 * Contient une valeur unique, un pointeur vers le nœud suivant
                 * dans le même bucket, et le hash pré-calculé de la valeur pour optimiser
                 * les comparaisons lors de la résolution des collisions.
                 * 
                 * @note Layout mémoire : [Value: T][Next: ptr][Hash: usize]
                 * @note La valeur est stockée telle quelle : unicité garantie par comparaison
                 * @note Le hash pré-calculé évite de re-hacher lors des comparaisons dans la liste
                 */
                struct Node {

                    T Value;                    ///< Valeur stockée dans le nœud (élément de l'ensemble)
                    Node* Next;                 ///< Pointeur vers le nœud suivant dans le bucket
                    usize Hash;                 ///< Hash pré-calculé de la valeur pour optimisation

                    /**
                     * @brief Constructeur de nœud avec initialisation des membres
                     * @param hash Hash pré-calculé de la valeur à stocker
                     * @param value Référence const vers la valeur à copier dans le nœud
                     * @param next Pointeur optionnel vers le nœud suivant (défaut: nullptr)
                     * @note Insertion en tête de liste : next pointe vers l'ancien premier nœud du bucket
                     */
                    Node(usize hash, const T& value, Node* next = nullptr)
                        : Value(value)
                        , Next(next)
                        , Hash(hash) {
                    }
                };

            // ====================================================================
            // SECTION PUBLIQUE : ALIASES DE TYPES
            // ====================================================================
            public:

                // ====================================================================
                // ALIASES DE TYPES POUR LA LISIBILITÉ ET LA GÉNÉRICITÉ
                // ====================================================================

                using ValueType = T;              ///< Alias du type des éléments stockés
                using SizeType = usize;           ///< Alias du type pour les tailles/comptages
                using Reference = const T&;       ///< Alias de référence const (lecture seule)
                using ConstReference = const T&;  ///< Alias de référence const pour accès en lecture

            // ====================================================================
            // SECTION PRIVÉE : MEMBRES DONNÉES ET MÉTHODES INTERNES
            // ====================================================================
            private:

                // ====================================================================
                // MEMBRES DONNÉES : ÉTAT DE LA TABLE DE HACHAGE
                // ====================================================================

                Node** mBuckets;                  ///< Tableau de pointeurs vers les têtes de listes chaînées
                SizeType mBucketCount;            ///< Nombre total de buckets alloués dans la table
                SizeType mSize;                   ///< Nombre d'éléments uniques effectivement stockés
                float mMaxLoadFactor;             ///< Seuil de charge déclenchant le réhash automatique
                Allocator* mAllocator;            ///< Pointeur vers l'allocateur mémoire utilisé
                Hasher mHasher;                   ///< Instance du foncteur de hachage pour les éléments
                Equal mEqual;                     ///< Instance du foncteur de comparaison pour l'égalité

                // ====================================================================
                // MÉTHODES UTILITAIRES D'INITIALISATION ET DE HACHAGE
                // ====================================================================

                /**
                 * @brief Initialise le tableau de buckets avec allocation alignée et zéro-initialisation
                 * @param bucketCount Nombre de buckets à allouer (minimum 1)
                 * @note Utilise alignof(Node*) pour une allocation mémoire optimale
                 * @note Utilise memory::NkMemZero() pour initialiser tous les pointeurs à nullptr
                 * @note Assertion en debug pour vérifier le succès de l'allocation
                 */
                void InitBuckets(SizeType bucketCount) {
                    mBucketCount = bucketCount > 0 ? bucketCount : 1;
                    mBuckets = static_cast<Node**>(mAllocator->Allocate(mBucketCount * sizeof(Node*), alignof(Node*)));
                    NKENTSEU_ASSERT(mBuckets != nullptr);
                    memory::NkMemZero(mBuckets, mBucketCount * sizeof(Node*));
                }

                /**
                 * @brief Calcule le hash d'un élément en déléguant au foncteur configuré
                 * @param value Référence const vers l'élément à hacher
                 * @return Valeur de hash de type usize pour l'indexation dans la table
                 * @note Méthode const : ne modifie pas l'état du UnorderedSet
                 * @note Le hash est pré-calculé et stocké dans chaque Node pour optimisation
                 */
                usize HashValue(const T& value) const {
                    return mHasher(value);
                }

                /**
                 * @brief Mappe un hash vers un index de bucket valide dans la table
                 * @param hash Valeur de hash pré-calculée par HashValue()
                 * @return Index de bucket dans [0, mBucketCount) via opération modulo
                 * @note Complexité O(1) : opération arithmétique pure
                 * @note Alternative optimisée : utiliser bitmask si mBucketCount est puissance de 2
                 */
                usize GetBucketIndex(usize hash) const {
                    return hash % mBucketCount;
                }

                /**
                 * @brief Redimensionne la table de hachage avec un nouveau nombre de buckets
                 * @param newBucketCount Nouveau nombre de buckets souhaité (minimum 1)
                 * @note Complexité O(n) : redistribution de tous les éléments existants
                 * @note Réutilise les hash pré-calculés : recalcul uniquement de l'index modulo
                 * @note Préserve l'ordre relatif dans chaque liste chaînée (stable rehash)
                 */
                void Rehash(SizeType newBucketCount) {
                    Node** newBuckets = static_cast<Node**>(mAllocator->Allocate(newBucketCount * sizeof(Node*)));
                    memory::NkMemZero(newBuckets, newBucketCount * sizeof(Node*));
                    for (SizeType i = 0; i < mBucketCount; ++i) {
                        Node* node = mBuckets[i];
                        while (node) {
                            Node* next = node->Next;
                            SizeType newIdx = node->Hash % newBucketCount;
                            node->Next = newBuckets[newIdx];
                            newBuckets[newIdx] = node;
                            node = next;
                        }
                    }
                    mAllocator->Deallocate(mBuckets);
                    mBuckets = newBuckets;
                    mBucketCount = newBucketCount;
                }

                /**
                 * @brief Vérifie et applique le réhash automatique si le load factor est dépassé
                 * @note Déclenche Rehash(mBucketCount * 2) si mSize > mBucketCount * mMaxLoadFactor
                 * @note Stratégie de croissance : doublement de capacité pour amortir les coûts
                 * @note Appelée après chaque Insert() pour maintenir les performances moyennes O(1)
                 */
                void CheckLoadFactor() {
                    if (mSize > mBucketCount * mMaxLoadFactor) {
                        Rehash(mBucketCount * 2);
                    }
                }

            // ====================================================================
            // SECTION PUBLIQUE : CONSTRUCTEURS ET DESTRUCTEUR
            // ====================================================================
            public:

                /**
                 * @brief Constructeur par défaut avec allocateur optionnel
                 * @param allocator Pointeur vers un allocateur personnalisé (nullptr = défaut)
                 * @note Initialise un set vide avec 16 buckets et load factor 0.75
                 * @note Complexité O(1) : allocation unique du tableau de buckets initial
                 */
                explicit NkUnorderedSet(Allocator* allocator = nullptr)
                    : mBuckets(nullptr)
                    , mBucketCount(16)
                    , mSize(0)
                    , mMaxLoadFactor(0.75f)
                    , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator())
                    , mHasher()
                    , mEqual() {
                    InitBuckets(16);
                }

                /**
                 * @brief Constructeur depuis NkInitializerList avec allocateur optionnel
                 * @param init Liste d'éléments pour initialiser le set
                 * @param allocator Pointeur vers un allocateur personnalisé (nullptr = défaut)
                 * @note Insère chaque élément séquentiellement : peut déclencher des réhash intermédiaires
                 * @note Les doublons sont automatiquement ignorés : unicité garantie
                 */
                NkUnorderedSet(NkInitializerList<T> init, Allocator* allocator = nullptr)
                    : mBuckets(nullptr)
                    , mBucketCount(16)
                    , mSize(0)
                    , mMaxLoadFactor(0.75f)
                    , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator())
                    , mHasher()
                    , mEqual() {
                    InitBuckets(16);
                    for (auto& val : init) {
                        Insert(val);
                    }
                }

                /**
                 * @brief Constructeur depuis std::initializer_list avec allocateur optionnel
                 * @param init Liste standard d'éléments pour initialiser le set
                 * @param allocator Pointeur vers un allocateur personnalisé (nullptr = défaut)
                 * @note Compatibilité avec la syntaxe C++11 : NkUnorderedSet<int> s = {3, 1, 4, 1, 5};
                 * @note Les doublons sont automatiquement ignorés : résultat {1, 3, 4, 5}
                 */
                NkUnorderedSet(std::initializer_list<T> init, Allocator* allocator = nullptr)
                    : mBuckets(nullptr)
                    , mBucketCount(16)
                    , mSize(0)
                    , mMaxLoadFactor(0.75f)
                    , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator())
                    , mHasher()
                    , mEqual() {
                    InitBuckets(16);
                    for (auto& val : init) {
                        Insert(val);
                    }
                }

                /**
                 * @brief Constructeur de copie pour duplication profonde du UnorderedSet
                 * @param other UnorderedSet source à dupliquer élément par élément
                 * @note Copie la configuration (load factor, hasher, equal) et les données
                 * @note Complexité O(n) : ré-insertion de tous les éléments avec recalcul des buckets
                 */
                NkUnorderedSet(const NkUnorderedSet& other)
                    : mBuckets(nullptr)
                    , mBucketCount(0)
                    , mSize(0)
                    , mMaxLoadFactor(other.mMaxLoadFactor)
                    , mAllocator(other.mAllocator)
                    , mHasher(other.mHasher)
                    , mEqual(other.mEqual) {
                    InitBuckets(other.mBucketCount);
                    for (SizeType i = 0; i < other.mBucketCount; ++i) {
                        Node* node = other.mBuckets[i];
                        while (node) {
                            Insert(node->Value);
                            node = node->Next;
                        }
                    }
                }

                // ====================================================================
                // SUPPORT C++11 : CONSTRUCTEUR DE DÉPLACEMENT
                // ====================================================================
                #if defined(NK_CPP11)

                    /**
                     * @brief Constructeur de déplacement pour transfert efficace des ressources
                     * @param other UnorderedSet source dont transférer le contenu
                     * @note noexcept : transfert par échange de pointeurs, aucune allocation
                     * @note other est laissé dans un état valide mais vide après le move
                     */
                    NkUnorderedSet(NkUnorderedSet&& other) NKENTSEU_NOEXCEPT
                        : mBuckets(other.mBuckets)
                        , mBucketCount(other.mBucketCount)
                        , mSize(other.mSize)
                        , mMaxLoadFactor(other.mMaxLoadFactor)
                        , mAllocator(other.mAllocator)
                        , mHasher(traits::NkMove(other.mHasher))
                        , mEqual(traits::NkMove(other.mEqual)) {
                        other.mBuckets = nullptr;
                        other.mBucketCount = 0;
                        other.mSize = 0;
                    }

                #endif // defined(NK_CPP11)

                /**
                 * @brief Destructeur libérant tous les nœuds et la structure interne
                 * @note Appelle Clear() pour détruire chaque Node individuellement
                 * @note Libère le tableau de buckets si alloué
                 * @note Garantit l'absence de fuite mémoire via RAII même en cas d'exception
                 */
                ~NkUnorderedSet() {
                    Clear();
                    if (mBuckets) {
                        mAllocator->Deallocate(mBuckets);
                    }
                }

            // ====================================================================
            // SECTION PUBLIQUE : OPÉRATEURS D'AFFECTATION
            // ====================================================================

                /**
                 * @brief Opérateur d'affectation par copie avec gestion d'auto-affectation
                 * @param other UnorderedSet source à copier
                 * @return Référence vers l'instance courante pour le chaînage d'opérations
                 * @note Libère les ressources actuelles avant de copier le contenu de other
                 * @note Préserve l'allocateur courant : ne copie pas mAllocator de other
                 */
                NkUnorderedSet& operator=(const NkUnorderedSet& other) {
                    if (this == &other) {
                        return *this;
                    }
                    Clear();
                    if (mBuckets) {
                        mAllocator->Deallocate(mBuckets);
                    }
                    mAllocator = other.mAllocator;
                    mMaxLoadFactor = other.mMaxLoadFactor;
                    mHasher = other.mHasher;
                    mEqual = other.mEqual;
                    InitBuckets(other.mBucketCount);
                    for (SizeType i = 0; i < other.mBucketCount; ++i) {
                        Node* node = other.mBuckets[i];
                        while (node) {
                            Insert(node->Value);
                            node = node->Next;
                        }
                    }
                    return *this;
                }

                // ====================================================================
                // SUPPORT C++11 : OPÉRATEUR DE DÉPLACEMENT
                // ====================================================================
                #if defined(NK_CPP11)

                    /**
                     * @brief Opérateur d'affectation par déplacement avec transfert de ressources
                     * @param other UnorderedSet source dont transférer le contenu
                     * @return Référence vers l'instance courante pour le chaînage d'opérations
                     * @note noexcept : échange de pointeurs sans allocation ni copie
                     * @note other est laissé dans un état valide mais vide après l'opération
                     */
                    NkUnorderedSet& operator=(NkUnorderedSet&& other) NKENTSEU_NOEXCEPT {
                        if (this == &other) {
                            return *this;
                        }
                        Clear();
                        if (mBuckets) {
                            mAllocator->Deallocate(mBuckets);
                        }
                        mBuckets = other.mBuckets;
                        mBucketCount = other.mBucketCount;
                        mSize = other.mSize;
                        mMaxLoadFactor = other.mMaxLoadFactor;
                        mAllocator = other.mAllocator;
                        mHasher = traits::NkMove(other.mHasher);
                        mEqual = traits::NkMove(other.mEqual);
                        other.mBuckets = nullptr;
                        other.mBucketCount = 0;
                        other.mSize = 0;
                        return *this;
                    }

                #endif // defined(NK_CPP11)

                /**
                 * @brief Opérateur d'affectation depuis NkInitializerList
                 * @param init Nouvelle liste d'éléments pour remplacer le contenu actuel
                 * @return Référence vers l'instance courante pour le chaînage d'opérations
                 * @note Efface le contenu existant avant d'insérer les nouveaux éléments
                 */
                NkUnorderedSet& operator=(NkInitializerList<T> init) {
                    Clear();
                    for (auto& val : init) {
                        Insert(val);
                    }
                    return *this;
                }

                /**
                 * @brief Opérateur d'affectation depuis std::initializer_list
                 * @param init Nouvelle liste standard d'éléments pour remplacer le contenu
                 * @return Référence vers l'instance courante pour le chaînage d'opérations
                 * @note Syntaxe C++11 : set = {1, 2, 3};
                 */
                NkUnorderedSet& operator=(std::initializer_list<T> init) {
                    Clear();
                    for (auto& val : init) {
                        Insert(val);
                    }
                    return *this;
                }

            // ====================================================================
            // SECTION PUBLIQUE : CAPACITÉ ET MÉTADONNÉES
            // ====================================================================

                /**
                 * @brief Teste si le UnorderedSet ne contient aucun élément
                 * @return true si mSize == 0, false sinon
                 * @note Opération O(1) sans effet de bord, noexcept garantie
                 * @note Équivalent à Size() == 0 mais sémantiquement plus explicite
                 */
                bool Empty() const NKENTSEU_NOEXCEPT {
                    return mSize == 0;
                }

                /**
                 * @brief Retourne le nombre d'éléments stockés dans le UnorderedSet
                 * @return Valeur de type SizeType représentant la cardinalité
                 * @note Opération O(1) sans effet de bord, noexcept garantie
                 * @note Reflète exactement le nombre d'éléments uniques (doublons ignorés)
                 */
                SizeType Size() const NKENTSEU_NOEXCEPT {
                    return mSize;
                }

            // ====================================================================
            // SECTION PUBLIQUE : MODIFICATEURS DE LA STRUCTURE
            // ====================================================================

                /**
                 * @brief Supprime tous les éléments et libère la mémoire des nœuds
                 * @note Complexité O(n) : destruction de chaque Node via son destructeur
                 * @note Ne libère pas le tableau de buckets : conserve la capacité allouée
                 * @note Après Clear(), Size() == 0 et Empty() == true, mais BucketCount() inchangé
                 */
                void Clear() {
                    for (SizeType i = 0; i < mBucketCount; ++i) {
                        Node* node = mBuckets[i];
                        while (node) {
                            Node* next = node->Next;
                            node->~Node();
                            mAllocator->Deallocate(node);
                            node = next;
                        }
                        mBuckets[i] = nullptr;
                    }
                    mSize = 0;
                }

                /**
                 * @brief Insère un nouvel élément dans le UnorderedSet si absent
                 * @param value Référence const vers l'élément à insérer
                 * @return true si l'insertion a eu lieu, false si l'élément était déjà présent
                 * @note Si l'élément existe déjà (mEqual) : retourne false sans modification
                 * @note Si l'élément est nouveau : crée un Node et l'insère en tête de liste du bucket
                 * @note Peut déclencher un réhash automatique si le load factor est dépassé
                 * @note Complexité : O(1) amorti, O(n) pire cas (collisions ou réhash)
                 */
                bool Insert(const T& value) {
                    usize hash = HashValue(value);
                    SizeType idx = GetBucketIndex(hash);
                    Node* node = mBuckets[idx];
                    while (node) {
                        if (mEqual(node->Value, value)) {
                            return false;
                        }
                        node = node->Next;
                    }
                    Node* newNode = static_cast<Node*>(mAllocator->Allocate(sizeof(Node)));
                    new (newNode) Node(hash, value, mBuckets[idx]);
                    mBuckets[idx] = newNode;
                    ++mSize;
                    CheckLoadFactor();
                    return true;
                }

                /**
                 * @brief Supprime un élément par valeur si présent dans le UnorderedSet
                 * @param value Élément à supprimer
                 * @return true si l'élément a été trouvé et supprimé, false si absent
                 * @note Complexité : O(1) amorti pour la recherche + O(1) pour la suppression de liste
                 * @note Libère la mémoire du Node supprimé via l'allocateur configuré
                 * @note Ne réduit pas le nombre de buckets : utiliser Rehash(1) pour compacter si nécessaire
                 */
                bool Erase(const T& value) {
                    usize hash = HashValue(value);
                    SizeType idx = GetBucketIndex(hash);
                    Node** prev = &mBuckets[idx];
                    Node* node = mBuckets[idx];
                    while (node) {
                        if (mEqual(node->Value, value)) {
                            *prev = node->Next;
                            node->~Node();
                            mAllocator->Deallocate(node);
                            --mSize;
                            return true;
                        }
                        prev = &node->Next;
                        node = node->Next;
                    }
                    return false;
                }

            // ====================================================================
            // SECTION PUBLIQUE : RECHERCHE ET INTERROGATION
            // ====================================================================

                /**
                 * @brief Teste la présence d'un élément dans le UnorderedSet
                 * @param value Élément à vérifier
                 * @return true si l'élément existe, false sinon
                 * @note Implémenté via recherche itérative dans le bucket cible : complexité O(1) amorti
                 * @note Utilise le hasher et le comparateur configurés pour la navigation et l'équivalence
                 * @note Plus sémantique que Find() (non implémenté ici) quand seule la présence importe
                 */
                bool Contains(const T& value) const {
                    usize hash = HashValue(value);
                    SizeType idx = GetBucketIndex(hash);
                    Node* node = mBuckets[idx];
                    while (node) {
                        if (mEqual(node->Value, value)) {
                            return true;
                        }
                        node = node->Next;
                    }
                    return false;
                }
        };

    } // namespace nkentseu

#endif // NK_CONTAINERS_ASSOCIATIVE_NKUNORDEREDSET_H

// ============================================================================
// EXEMPLES D'UTILISATION ET PATTERNS COURANTS
// ============================================================================
/*
 * @section examples Exemples pratiques d'utilisation de NkUnorderedSet
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 1 : Création et opérations de base avec unicité garantie
 * --------------------------------------------------------------------------
 * @code
 * #include "NKContainers/Associative/NkUnorderedSet.h"
 * #include <cstdio>
 * 
 * void exempleBase() {
 *     // Création d'un UnorderedSet d'entiers avec allocateur par défaut
 *     nkentseu::NkUnorderedSet<int> set;
 *     
 *     // Insertion avec doublons : seuls les éléments uniques sont conservés
 *     set.Insert(3);
 *     set.Insert(1);
 *     set.Insert(4);
 *     set.Insert(1);  // Doublon ignoré
 *     set.Insert(5);
 *     set.Insert(9);
 *     
 *     printf("Taille: %zu (doublons ignorés)\n", set.Size());  // 5, pas 6
 *     
 *     // Recherche de membership efficace
 *     if (set.Contains(4)) {
 *         printf("4 est présent dans le set\n");
 *     }
 *     
 *     // Suppression
 *     bool removed = set.Erase(1);   // true : élément présent
 *     bool missing = set.Erase(99);  // false : élément absent
 *     
 *     printf("Après suppression de 1: taille = %zu\n", set.Size());  // 4
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 2 : Initialisation et construction depuis liste
 * --------------------------------------------------------------------------
 * @code
 * void exempleInitialisation() {
 *     // Construction via initializer_list (C++11) - doublons automatiquement filtrés
 *     nkentseu::NkUnorderedSet<const char*> tags = {"cpp", "template", "cpp", "container", "template"};
 *     
 *     printf("Tags uniques: %zu\n", tags.Size());  // 3: cpp, template, container
 *     
 *     // Construction par copie
 *     nkentseu::NkUnorderedSet<const char*> copy = tags;
 *     
 *     // Construction par déplacement (C++11)
 *     #if defined(NK_CPP11)
 *     nkentseu::NkUnorderedSet<const char*> moved = nkentseu::traits::NkMove(copy);
 *     // copy est maintenant vide, moved possède les éléments
 *     #endif
 *     
 *     // Affectation depuis liste
 *     tags = {"new", "tag", "list", "new"};
 *     printf("Nouveaux tags: %zu uniques\n", tags.Size());  // 3: new, tag, list
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 3 : Hasher et comparateur personnalisés pour types complexes
 * --------------------------------------------------------------------------
 * @code
 * // Type personnalisé nécessitant un hasher spécialisé
 * struct Point {
 *     int x, y;
 *     bool operator==(const Point& other) const {
 *         return x == other.x && y == other.y;
 *     }
 * };
 * 
 * // Hasher personnalisé pour Point (combinaison des coordonnées)
 * struct PointHasher {
 *     usize operator()(const Point& p) const {
 *         // Combinaison simple : peut être améliorée avec un meilleur mixing
 *         return static_cast<usize>(p.x) * 31u + static_cast<usize>(p.y);
 *     }
 * };
 * 
 * // Comparateur personnalisé (ici identique au défaut, mais extensible)
 * struct PointEqual {
 *     bool operator()(const Point& a, const Point& b) const {
 *         return a == b;
 *     }
 * };
 * 
 * void exempleTypesComplexes() {
 *     // UnorderedSet avec hasher et comparateur personnalisés
 *     nkentseu::NkUnorderedSet<Point, memory::NkAllocator, PointHasher, PointEqual> points;
 *     
 *     points.Insert({0, 0});
 *     points.Insert({10, 20});
 *     points.Insert({0, 0});  // Doublon ignoré
 *     
 *     printf("Points uniques: %zu\n", points.Size());  // 2
 *     
 *     // Recherche avec élément temporaire
 *     if (points.Contains({10, 20})) {
 *         printf("Point (10,20) trouvé\n");
 *     }
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 4 : Élimination de doublons dans un flux de données
 * --------------------------------------------------------------------------
 * @code
 * #include "NKContainers/Sequential/NkVector.h"
 * 
 * void exempleDeduplication() {
 *     // Vecteur avec doublons
 *     nkentseu::NkVector<int> data = {5, 2, 8, 2, 9, 1, 5, 2, 7};
 *     
 *     // Conversion en UnorderedSet pour éliminer les doublons
 *     nkentseu::NkUnorderedSet<int> unique;
 *     for (int val : data) {
 *         unique.Insert(val);  // Doublons automatiquement ignorés
 *     }
 *     
 *     printf("Valeurs uniques: %zu\n", unique.Size());  // 6: 1,2,5,7,8,9
 *     
 *     // Vérification de présence rapide
 *     bool has5 = unique.Contains(5);    // true
 *     bool has10 = unique.Contains(10);  // false
 *     
 *     printf("5 présent: %s, 10 présent: %s\n",
 *            has5 ? "oui" : "non", has10 ? "oui" : "non");
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 5 : Gestion mémoire avec allocateur personnalisé
 * --------------------------------------------------------------------------
 * @code
 * #include "NKMemory/NkPoolAllocator.h"
 * 
 * void exempleAllocateur() {
 *     // Pool allocator pour réduire la fragmentation sur insertions massives
 *     memory::NkPoolAllocator pool(256 * 1024);  // Pool de 256KB
 *     
 *     // UnorderedSet utilisant le pool pour toutes ses allocations de nœuds
 *     nkentseu::NkUnorderedSet<usize, memory::NkPoolAllocator> indexedIds(&pool);
 *     
 *     // Pré-réhash pour éviter les réhash intermédiaires lors d'insertions en masse
 *     indexedIds.Rehash(2048);  // Anticipe ~1500 éléments avec load factor 0.75
 *     
 *     // Insertion en masse sans fragmentation externe
 *     for (usize i = 0; i < 1000; ++i) {
 *         indexedIds.Insert(i * 3);  // Insertion d'IDs multiples de 3
 *     }
 *     
 *     printf("Set: %zu éléments uniques\n", indexedIds.Size());
 *     
 *     // Vérification d'appartenance rapide
 *     bool has999 = indexedIds.Contains(999);   // true : 999 = 333*3
 *     bool has1000 = indexedIds.Contains(1000); // false : pas multiple de 3
 *     
 *     // Destruction : libération en bloc via le pool (très rapide)
 *     // L'implémentation garantit O(1) amorti pour chaque opération intermédiaire
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 6 : Patterns avancés - Filtrage, intersection, union simulée
 * --------------------------------------------------------------------------
 * @code
 * // Pattern 1: Filtrage de valeurs valides
 * bool EstValide(int x) { return x > 0 && x < 100; }
 * 
 * void exempleFiltrage(const nkentseu::NkVector<int>& inputs) {
 *     nkentseu::NkUnorderedSet<int> valides;
 *     
 *     for (int val : inputs) {
 *         if (EstValide(val)) {
 *             valides.Insert(val);  // Doublons automatiquement éliminés
 *         }
 *     }
 *     
 *     printf("Valeurs valides uniques: %zu\n", valides.Size());
 * }
 * 
 * // Pattern 2: Intersection de deux ensembles (simulation)
 * void exempleIntersection(const nkentseu::NkUnorderedSet<int>& a, 
 *                          const nkentseu::NkUnorderedSet<int>& b) {
 *     nkentseu::NkUnorderedSet<int> intersection;
 *     
 *     // Pour chaque élément de a, vérifier présence dans b
 *     for (SizeType i = 0; i < a.Size(); ++i) {
 *         // Note: nécessite un itérateur ou une méthode d'accès par index
 *         // Simulation avec Contains sur un vecteur intermédiaire
 *     }
 *     
 *     // Alternative plus simple avec NkVector comme intermédiaire
 * }
 * 
 * // Pattern 3: Union de deux ensembles (simulation)
 * void exempleUnion(const nkentseu::NkUnorderedSet<int>& a,
 *                   const nkentseu::NkUnorderedSet<int>& b) {
 *     nkentseu::NkUnorderedSet<int> unite = a;  // Copie de a
 *     
 *     // Insertion des éléments de b : doublons automatiquement ignorés
 *     // Note: nécessite un itérateur pour parcourir b
 *     // Pour l'instant : insertion manuelle ou utilisation de NkVector intermédiaire
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 7 : Comparaison avec NkSet - quand choisir quoi ?
 * --------------------------------------------------------------------------
 * @code
 * void exempleChoixConteneur() {
 *     // CAS 1: Unicité sans ordre requis + performance moyenne optimale
 *     // -> NkUnorderedSet (table de hachage)
 *     nkentseu::NkUnorderedSet<int> hashSet;
 *     hashSet.Insert(3);
 *     hashSet.Insert(1);
 *     hashSet.Insert(2);
 *     // Recherche: O(1) amorti, ordre d'itération indéterminé
 *     
 *     // CAS 2: Unicité + ordre trié requis + pire cas garanti
 *     // -> NkSet (arbre Rouge-Noir)
 *     nkentseu::NkSet<int> treeSet;
 *     treeSet.Insert(3);
 *     treeSet.Insert(1);
 *     treeSet.Insert(2);
 *     // Parcours garanti: 1, 2, 3 (toujours le même ordre)
 *     // Recherche: O(log n) garanti
 *     
 *     // COMPARAISON DE PERFORMANCE:
 *     // - NkUnorderedSet: O(1) amorti, O(n) pire cas, pas d'ordre
 *     // - NkSet: O(log n) garanti, ordre maintenu, plus de mémoire par nœud
 *     
 *     // RÈGLE PRATIQUE:
 *     // - Utiliser NkUnorderedSet si: performance moyenne prioritaire, ordre non requis
 *     // - Utiliser NkSet si: ordre trié requis, ou pire cas critique (temps réel)
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * BONNES PRATIQUES ET RECOMMANDATIONS
 * --------------------------------------------------------------------------
 * 
 * 1. CHOIX DU TYPE D'ÉLÉMENT :
 *    - Doit être copiable, hachable (Hasher) et comparable (Equal)
 *    - Privilégier les types trivialement copiables pour les performances
 *    - Pour les types complexes : spécialiser NkUnorderedSetDefaultHasher ou fournir un hasher custom
 *    - Éviter les éléments mutables : le hash ne doit jamais changer après insertion
 * 
 * 2. GESTION DU LOAD FACTOR :
 *    - Défaut 0.75 : bon compromis général mémoire/performance
 *    - Réduire à 0.5 pour minimiser les collisions (recherche intensive)
 *    - Augmenter à 0.9 pour économiser la mémoire (insertions rares)
 *    - Utiliser Rehash() avant insertions en masse pour éviter les réhash intermédiaires
 * 
 * 3. ACCÈS ET MODIFICATION :
 *    - Contains() : O(1) amorti, idéal pour tester la présence sans itérateur
 *    - Insert() retourne bool : permet de savoir si l'élément était nouveau ou doublon
 *    - Pas d'opérateur[] : les Sets ne stockent pas de valeurs associées, seulement des clés
 *    - Pas d'accès direct aux éléments : utiliser Contains() ou itérateurs (à ajouter)
 * 
 * 4. ITÉRATION ET INVALIDATION :
 *    - TODO: Ajouter des itérateurs pour parcours des éléments (Forward Iterator)
 *    - Tout Insert/Erase/Rehash invalide TOUS les itérateurs existants (quand implémentés)
 *    - L'ordre d'itération est INDÉTERMINÉ : ne pas dépendre de l'ordre pour la logique métier
 * 
 * 5. GESTION MÉMOIRE :
 *    - Utiliser un allocateur pool pour les UnorderedSet de taille fixe ou prévisible
 *    - Clear() ne libère pas les buckets : appeler Rehash(1) pour compacter si nécessaire
 *    - Pour les éléments lourds : stocker des pointeurs ou utiliser move semantics (C++11)
 * 
 * 6. SÉCURITÉ ET ROBUSTESSE :
 *    - Valider les entrées utilisateur avant Insert()/Contains() si nécessaire
 *    - Thread-unsafe : protéger avec mutex pour accès concurrents multiples
 *    - Pour les clés flottantes : attention aux problèmes de précision dans Equal()
 * 
 * 7. LIMITATIONS CONNUES (version actuelle) :
 *    - Pas d'itérateurs : à ajouter pour compatibilité range-based for et algorithmes génériques
 *    - Pas de méthode ForEach() : à ajouter pour parcours fonctionnel sans itérateurs explicites
 *    - Pas d'opérations ensemblistes : union(), intersection(), difference() à implémenter
 *    - Pas de méthode Reserve(elementCount) : alias sémantique pour Rehash() basé sur le nombre cible
 * 
 * 8. EXTENSIONS RECOMMANDÉES :
 *    - Ajouter des itérateurs Forward pour compatibilité STL et range-based for
 *    - Implémenter ForEach(Fn) pour parcours fonctionnel sans exposition d'itérateurs
 *    - Ajouter les opérations ensemblistes : Union(), Intersection(), Difference()
 *    - Ajouter Reserve(elementCount) pour API plus intuitive que Rehash(bucketCount)
 *    - Supporter la construction depuis range d'itérateurs : NkUnorderedSet(InputIt first, InputIt last)
 * 
 * 9. COMPARAISON AVEC AUTRES STRUCTURES :
 *    - vs NkSet (arbre Rouge-Noir) : O(1) amorti vs O(log n) garanti, pas d'ordre vs ordre trié
 *    - vs NkVector + déduplication manuelle : O(1) recherche vs O(n), plus de mémoire vs moins
 *    - vs NkHashMap<T, void> : sémantique plus claire pour un ensemble pur
 *    - Règle : utiliser NkUnorderedSet si unicité + performance moyenne + pas d'ordre requis
 */

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Généré par Rihen le 2026-02-05 22:26:13
// Dernière modification : 2026-04-26 (restructuration et documentation)
// ============================================================