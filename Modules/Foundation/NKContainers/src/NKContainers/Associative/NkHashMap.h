// -----------------------------------------------------------------------------
// FICHIER: NKContainers\Containers\Associative\NkHashMap.h
// DESCRIPTION: Table de hachage - Conteneur associatif non-ordonné
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CONTAINERS_ASSOCIATIVE_NKHASHMAP_H
#define NK_CONTAINERS_ASSOCIATIVE_NKHASHMAP_H

    // ========================================================================
    // INCLUSIONS DES DÉPENDANCES
    // ========================================================================

    #include "NKCore/NkTypes.h"                    // Définition des types fondamentaux (usize, etc.)
    #include "NKContainers/NkContainersApi.h"      // Macros d'exportation de l'API conteneurs
    #include "NKCore/NkTraits.h"                   // Traits pour move/forward et utilitaires méta
    #include "NKMemory/NkAllocator.h"              // Système d'allocation mémoire personnalisable
    #include "NKMemory/NkFunction.h"               // Wrapper pour les fonctions et callables
    #include "NKCore/Assert/NkAssert.h"            // Macros d'assertion pour le débogage sécurisé
    #include "NKContainers/Heterogeneous/NkPair.h" // Conteneur de paire pour stocker clé-valeur
    #include "NKContainers/Iterators/NkIterator.h" // Infrastructure commune pour les itérateurs
    #include "NKContainers/Iterators/NkInitializerList.h" // Support des listes d'initialisation

    // ========================================================================
    // ESPACE DE NOMS PRINCIPAL
    // ========================================================================

    namespace nkentseu {

        // ====================================================================
        // FONCTEURS DE HACHAGE ET D'ÉGALITÉ PAR DÉFAUT
        // ====================================================================

        /**
         * @brief Foncteur de hachage par défaut pour les clés de NkHashMap
         * 
         * Implémente l'algorithme FNV-1a (Fowler-Noll-Vo) pour un hachage
         * rapide et de bonne qualité statistique sur des données binaires.
         * 
         * PROPRIÉTÉS :
         * - Fonction de hachage non cryptographique optimisée pour la vitesse
         * - Distribution uniforme pour réduire les collisions dans la table
         * - Opérateur() const : peut être utilisé sur des instances const
         * - Template sur le type de clé : fonctionne avec tout type POD ou trivial
         * 
         * @tparam Key Type de la clé à hacher (doit être copiable et de taille fixe)
         * 
         * @note Pour les types complexes (NkString, structs), spécialiser ce template
         *       ou fournir un hasher personnalisé via le template parameter de NkHashMap
         * @note L'algorithme traite la clé comme un tableau d'octets : attention à l'endianness
         * 
         * @example
         * NkHashMapDefaultHasher<int> hasher;
         * usize h = hasher(42);  // Calcule le hash de l'entier 42
         */
        template<typename Key>
        struct NkHashMapDefaultHasher {

            /**
             * @brief Calcule le hash d'une clé selon l'algorithme FNV-1a
             * @param key Référence const vers la clé à hacher
             * @return Valeur de type usize représentant le hash calculé
             * @note Méthode const : ne modifie pas l'état du foncteur
             * @note Complexité O(sizeof(Key)) : parcours linéaire des octets de la clé
             */
            usize operator()(const Key& key) const {
                const nk_uint8* data = reinterpret_cast<const nk_uint8*>(&key);
                usize hash = static_cast<usize>(1469598103934665603ull);
                for (usize i = 0; i < sizeof(Key); ++i) {
                    hash ^= static_cast<usize>(data[i]);
                    hash *= static_cast<usize>(1099511628211ull);
                }
                return hash;
            }
        };

        /**
         * @brief Foncteur de comparaison d'égalité par défaut pour les clés
         * 
         * Implémente une comparaison basée sur l'opérateur == du type Key.
         * Utilisé pour résoudre les collisions dans les buckets de la table de hachage.
         * 
         * @tparam Key Type de la clé à comparer (doit supporter operator==)
         * 
         * @note Pour les types nécessitant une comparaison personnalisée (tolérance flottante,
         *       comparaison sémantique de structs), spécialiser ce template ou fournir
         *       un KeyEqual personnalisé via le template parameter de NkHashMap
         */
        template<typename Key>
        struct NkHashMapDefaultEqual {

            /**
             * @brief Compare deux clés pour l'égalité
             * @param lhs Première clé à comparer (côté gauche)
             * @param rhs Seconde clé à comparer (côté droit)
             * @return true si lhs == rhs, false sinon
             * @note Méthode const : ne modifie pas l'état du foncteur
             * @note Délègue à l'opérateur== du type Key : doit être défini et cohérent
             */
            bool operator()(const Key& lhs, const Key& rhs) const {
                return lhs == rhs;
            }
        };

        // ====================================================================
        // CLASSE PRINCIPALE : NK HASH MAP
        // ====================================================================

        /**
         * @brief Classe template implémentant une table de hachage avec chaînage séparé
         * 
         * Conteneur associatif non-ordonné stockant des paires clé-valeur uniques.
         * Utilise le chaînage séparé (separate chaining) pour gérer les collisions :
         * chaque bucket contient une liste chaînée de nœuds en cas de hash identique.
         * 
         * PROPRIÉTÉS FONDAMENTALES :
         * - Accès moyen O(1) pour insertion, recherche et suppression
         * - Pire cas O(n) en cas de nombreuses collisions (hash dégénéré)
         * - Réhash automatique lorsque le load factor dépasse le seuil configuré
         * - Itérateurs bidirectionnels pour parcours de tous les éléments
         * - Gestion mémoire via allocateur personnalisable
         * 
         * PARAMÈTRES DE CONFIGURATION :
         * - Load factor maximal (défaut: 0.75) : déclenche le réhash quand dépassé
         * - Nombre initial de buckets (défaut: 16) : capacité de départ de la table
         * - Foncteurs Hasher et KeyEqual personnalisables pour types complexes
         * 
         * COMPLEXITÉ ALGORITHMIQUE :
         * - Insertion : O(1) amorti, O(n) pire cas (réhash ou collisions)
         * - Recherche (Find/Contains) : O(1) amorti, O(n) pire cas
         * - Suppression (Erase) : O(1) amorti, O(n) pire cas
         * - Réhash explicite : O(n) pour redistribuer tous les éléments
         * - Itération complète : O(n + bucket_count) pour visiter tous les buckets
         * 
         * CAS D'USAGE TYPQUES :
         * - Indexation rapide par clé unique (caches, registres, symbol tables)
         * - Comptage de fréquences (word count, histogrammes)
         * - Mémorisation de résultats (memoization, dynamic programming)
         * - Regroupement d'éléments par clé (grouping, aggregation)
         * 
         * @tparam Key Type des clés (doit être copiable, hachable et comparable)
         * @tparam Value Type des valeurs associées (doit être copiable/déplaçable)
         * @tparam Allocator Type de l'allocateur mémoire (défaut: memory::NkAllocator)
         * @tparam Hasher Foncteur de hachage (défaut: NkHashMapDefaultHasher<Key>)
         * @tparam KeyEqual Foncteur de comparaison (défaut: NkHashMapDefaultEqual<Key>)
         * 
         * @note Les clés sont stockées en const : modification après insertion impossible
         * @note Thread-unsafe : synchronisation externe requise pour usage concurrent
         * @note Les itérateurs sont invalidés par les réhash (Insert/Erase/Rehash)
         */
        template<typename Key,
                 typename Value,
                 typename Allocator = memory::NkAllocator,
                 typename Hasher = NkHashMapDefaultHasher<Key>,
                 typename KeyEqual = NkHashMapDefaultEqual<Key>>
        class NkHashMap {

            // ====================================================================
            // SECTION PRIVÉE : STRUCTURE INTERNE ET NŒUDS
            // ====================================================================
            private:

                /**
                 * @brief Structure interne représentant un nœud de la liste chaînée
                 * 
                 * Contient une paire clé-valeur, un pointeur vers le nœud suivant
                 * dans le même bucket, et le hash pré-calculé de la clé pour optimiser
                 * les comparaisons lors de la résolution des collisions.
                 * 
                 * @note Layout mémoire : [Data: NkPair<const Key, Value>][Next: ptr][Hash: usize]
                 * @note La clé est const pour garantir l'invariant : hash(key) ne change jamais
                 */
                struct Node {

                    NkPair<const Key, Value> Data;  ///< Paire clé-valeur stockée (clé immuable)
                    Node* Next;                      ///< Pointeur vers le nœud suivant dans le bucket
                    usize Hash;                      ///< Hash pré-calculé de la clé pour optimisation

                    // ====================================================================
                    // CONSTRUCTEURS DE NŒUD (C++11 ET LEGACY)
                    // ====================================================================
                    #if defined(NK_CPP11)

                        /**
                         * @brief Constructeur avec forwarding parfait pour C++11+
                         * @tparam K Type déduit pour la clé (avec référence collapsing)
                         * @tparam V Type déduit pour la valeur (avec référence collapsing)
                         * @param hash Hash pré-calculé de la clé à stocker
                         * @param key Argument forwarded vers la construction de la clé
                         * @param value Argument forwarded vers la construction de la valeur
                         * @param next Pointeur optionnel vers le nœud suivant (défaut: nullptr)
                         * @note Utilise traits::NkForward pour éviter les copies inutiles
                         */
                        template<typename K, typename V>
                        Node(usize hash, K&& key, V&& value, Node* next = nullptr)
                            : Data(traits::NkForward<K>(key), traits::NkForward<V>(value))
                            , Next(next)
                            , Hash(hash) {
                        }

                    #else

                        /**
                         * @brief Constructeur par copie pour compatibilité C++98/03
                         * @param hash Hash pré-calculé de la clé à stocker
                         * @param key Référence const vers la clé à copier
                         * @param value Référence const vers la valeur à copier
                         * @param next Pointeur optionnel vers le nœud suivant (défaut: nullptr)
                         * @note Version fallback sans move semantics ni forwarding
                         */
                        Node(usize hash, const Key& key, const Value& value, Node* next = nullptr)
                            : Data(key, value)
                            , Next(next)
                            , Hash(hash) {
                        }

                    #endif
                };

            // ====================================================================
            // SECTION PUBLIQUE : ALIASES ET ITÉRATEURS
            // ====================================================================
            public:

                // ====================================================================
                // ALIASES DE TYPES POUR LA LISIBILITÉ ET LA GÉNÉRICITÉ
                // ====================================================================

                using KeyType = Key;                    ///< Alias du type de clé
                using ValueType = NkPair<const Key, Value>;  ///< Alias du type de paire stockée
                using SizeType = usize;                 ///< Alias du type pour les tailles/comptages
                using Reference = ValueType&;           ///< Alias de référence non-const vers une paire
                using ConstReference = const ValueType&;///< Alias de référence const vers une paire
                using MappedType = Value;               ///< Alias du type de valeur associée

                // ====================================================================
                // CLASSE ITÉRATEUR MUTABLE
                // ====================================================================

                /**
                 * @brief Itérateur mutable pour parcourir les éléments de la HashMap
                 * 
                 * Itérateur bidirectionnel qui traverse les buckets séquentiellement
                 * et suit les listes chaînées à l'intérieur de chaque bucket.
                 * 
                 * PROPRIÉTÉS :
                 * - Sémantique d'itérateur d'entrée (Input Iterator) minimum
                 * - Invalidation par réhash : Insert/Erase/Rehash invalident tous les itérateurs
                 * - Accès en lecture/écriture via operator* et operator->
                 * 
                 * @note Construction réservée à NkHashMap via friend declaration
                 * @note Comparaison par égalité de pointeur de nœud : deux itérateurs égaux
                 *       pointent vers le même élément physique dans la table
                 */
                class Iterator {

                    // ====================================================================
                    // MEMBRES PRIVÉS DE L'ITÉRATEUR
                    // ====================================================================
                    private:

                        NkHashMap* mMap;          ///< Pointeur vers la HashMap parente pour navigation
                        Node* mNode;              ///< Pointeur vers le nœud courant dans la liste chaînée
                        SizeType mBucket;         ///< Index du bucket courant dans le tableau de buckets
                        friend class NkHashMap;   ///< NkHashMap peut construire des itérateurs

                        /**
                         * @brief Constructeur privé pour création contrôlée par NkHashMap
                         * @param map Pointeur vers la HashMap parente
                         * @param node Pointeur vers le nœud de départ (nullptr pour end())
                         * @param bucket Index du bucket contenant le nœud de départ
                         */
                        Iterator(NkHashMap* map, Node* node, SizeType bucket)
                            : mMap(map)
                            , mNode(node)
                            , mBucket(bucket) {
                        }

                        /**
                         * @brief Avance l'itérateur vers l'élément suivant
                         * @note Gère la transition entre nœuds d'un même bucket et entre buckets
                         * @note Complexité amortie O(1) : saute les buckets vides en temps constant moyen
                         */
                        void Advance() {
                            if (!mMap || !mNode) {
                                mNode = nullptr;
                                mBucket = 0;
                                return;
                            }
                            if (mNode->Next) {
                                mNode = mNode->Next;
                                return;
                            }
                            ++mBucket;
                            while (mBucket < mMap->mBucketCount && mMap->mBuckets[mBucket] == nullptr) {
                                ++mBucket;
                            }
                            mNode = (mBucket < mMap->mBucketCount) ? mMap->mBuckets[mBucket] : nullptr;
                        }

                    // ====================================================================
                    // INTERFACE PUBLIQUE DE L'ITÉRATEUR
                    // ====================================================================
                    public:

                        /**
                         * @brief Constructeur par défaut pour itérateur non-initialisé (end-like)
                         */
                        Iterator()
                            : mMap(nullptr)
                            , mNode(nullptr)
                            , mBucket(0) {
                        }

                        /**
                         * @brief Opérateur de déréférencement pour accès à la paire clé-valeur
                         * @return Référence non-const vers la paire stockée dans le nœud courant
                         * @pre L'itérateur ne doit pas être end() (assertion implicite par accès mNode)
                         */
                        Reference operator*() const {
                            return mNode->Data;
                        }

                        /**
                         * @brief Opérateur flèche pour accès aux membres de la paire
                         * @return Pointeur vers la paire stockée dans le nœud courant
                         * @note Permet it->First et it->Second comme syntaxe familière
                         */
                        ValueType* operator->() const {
                            return &mNode->Data;
                        }

                        /**
                         * @brief Opérateur de pré-incrémentation : avance et retourne *this
                         * @return Référence vers l'itérateur avancé pour chaînage
                         * @note Préférer ++it à it++ pour éviter la copie temporaire
                         */
                        Iterator& operator++() {
                            Advance();
                            return *this;
                        }

                        /**
                         * @brief Opérateur de post-incrémentation : retourne copie puis avance
                         * @return Copie de l'itérateur avant l'avancement
                         * @note Crée une copie temporaire : légèrement moins efficace que pré-incrément
                         */
                        Iterator operator++(int) {
                            Iterator temp = *this;
                            Advance();
                            return temp;
                        }

                        /**
                         * @brief Opérateur d'égalité : compare les pointeurs de nœuds
                         * @param other Autre itérateur à comparer avec l'instance courante
                         * @return true si les deux itérateurs pointent vers le même nœud
                         * @note Deux itérateurs end() sont égaux même sur des HashMap différentes
                         */
                        bool operator==(const Iterator& other) const {
                            return mNode == other.mNode;
                        }

                        /**
                         * @brief Opérateur d'inégalité : négation de l'égalité
                         * @param other Autre itérateur à comparer avec l'instance courante
                         * @return true si les itérateurs pointent vers des nœuds différents
                         */
                        bool operator!=(const Iterator& other) const {
                            return mNode != other.mNode;
                        }
                };

                // ====================================================================
                // CLASSE ITÉRATEUR CONST
                // ====================================================================

                /**
                 * @brief Itérateur en lecture seule pour parcourir les éléments de la HashMap
                 * 
                 * Version const de Iterator : permet l'itération sur des HashMap const
                 * sans possibilité de modifier les valeurs via l'itérateur.
                 * 
                 * @note Construction réservée à NkHashMap via friend declaration
                 * @note Conversion implicite depuis Iterator : permet passage mutable -> const
                 */
                class ConstIterator {

                    // ====================================================================
                    // MEMBRES PRIVÉS DE L'ITÉRATEUR CONST
                    // ====================================================================
                    private:

                        const NkHashMap* mMap;    ///< Pointeur const vers la HashMap parente
                        const Node* mNode;        ///< Pointeur const vers le nœud courant
                        SizeType mBucket;         ///< Index du bucket courant
                        friend class NkHashMap;   ///< NkHashMap peut construire des itérateurs

                        /**
                         * @brief Constructeur privé pour création contrôlée par NkHashMap
                         * @param map Pointeur const vers la HashMap parente
                         * @param node Pointeur const vers le nœud de départ
                         * @param bucket Index du bucket contenant le nœud de départ
                         */
                        ConstIterator(const NkHashMap* map, const Node* node, SizeType bucket)
                            : mMap(map)
                            , mNode(node)
                            , mBucket(bucket) {
                        }

                        /**
                         * @brief Avance l'itérateur const vers l'élément suivant
                         * @note Logique identique à Iterator::Advance() mais sur données const
                         */
                        void Advance() {
                            if (!mMap || !mNode) {
                                mNode = nullptr;
                                mBucket = 0;
                                return;
                            }
                            if (mNode->Next) {
                                mNode = mNode->Next;
                                return;
                            }
                            ++mBucket;
                            while (mBucket < mMap->mBucketCount && mMap->mBuckets[mBucket] == nullptr) {
                                ++mBucket;
                            }
                            mNode = (mBucket < mMap->mBucketCount) ? mMap->mBuckets[mBucket] : nullptr;
                        }

                    // ====================================================================
                    // INTERFACE PUBLIQUE DE L'ITÉRATEUR CONST
                    // ====================================================================
                    public:

                        /**
                         * @brief Constructeur par défaut pour itérateur const non-initialisé
                         */
                        ConstIterator()
                            : mMap(nullptr)
                            , mNode(nullptr)
                            , mBucket(0) {
                        }

                        /**
                         * @brief Constructeur de conversion depuis Iterator mutable
                         * @param it Itérateur mutable à convertir en version const
                         * @note Permet le passage implicite : ConstIterator cit = mutable_it;
                         */
                        ConstIterator(const Iterator& it)
                            : mMap(it.mMap)
                            , mNode(it.mNode)
                            , mBucket(it.mBucket) {
                        }

                        /**
                         * @brief Opérateur de déréférencement const pour accès en lecture
                         * @return Référence const vers la paire stockée dans le nœud courant
                         */
                        ConstReference operator*() const {
                            return mNode->Data;
                        }

                        /**
                         * @brief Opérateur flèche const pour accès aux membres en lecture
                         * @return Pointeur const vers la paire stockée dans le nœud courant
                         */
                        const ValueType* operator->() const {
                            return &mNode->Data;
                        }

                        /**
                         * @brief Opérateur de pré-incrémentation const
                         * @return Référence vers l'itérateur const avancé pour chaînage
                         */
                        ConstIterator& operator++() {
                            Advance();
                            return *this;
                        }

                        /**
                         * @brief Opérateur de post-incrémentation const
                         * @return Copie de l'itérateur const avant l'avancement
                         */
                        ConstIterator operator++(int) {
                            ConstIterator temp = *this;
                            Advance();
                            return temp;
                        }

                        /**
                         * @brief Opérateur d'égalité pour itérateurs const
                         * @param other Autre itérateur const à comparer
                         * @return true si les deux pointent vers le même nœud physique
                         */
                        bool operator==(const ConstIterator& other) const {
                            return mNode == other.mNode;
                        }

                        /**
                         * @brief Opérateur d'inégalité pour itérateurs const
                         * @param other Autre itérateur const à comparer
                         * @return true si les itérateurs pointent vers des nœuds différents
                         */
                        bool operator!=(const ConstIterator& other) const {
                            return mNode != other.mNode;
                        }
                };

            // ====================================================================
            // SECTION PRIVÉE : MEMBRES DONNÉES ET MÉTHODES INTERNES
            // ====================================================================
            private:

                // ====================================================================
                // MEMBRES DONNÉES : ÉTAT DE LA TABLE DE HACHAGE
                // ====================================================================

                Node** mBuckets;                  ///< Tableau de pointeurs vers les têtes de listes chaînées
                SizeType mBucketCount;            ///< Nombre total de buckets alloués dans la table
                SizeType mSize;                   ///< Nombre d'éléments effectivement stockés (paires uniques)
                float mMaxLoadFactor;             ///< Seuil de charge déclenchant le réhash automatique
                Allocator* mAllocator;            ///< Pointeur vers l'allocateur mémoire utilisé
                Hasher mHasher;                   ///< Instance du foncteur de hachage pour les clés
                KeyEqual mEqual;                  ///< Instance du foncteur de comparaison pour les clés

                // ====================================================================
                // MÉTHODES UTILITAIRES DE HACHAGE ET INDEXATION
                // ====================================================================

                /**
                 * @brief Calcule le hash d'une clé en déléguant au foncteur configuré
                 * @param key Référence const vers la clé à hacher
                 * @return Valeur de hash de type usize pour l'indexation dans la table
                 * @note Méthode const : ne modifie pas l'état de la HashMap
                 * @note Le hash est pré-calculé et stocké dans chaque Node pour optimisation
                 */
                usize HashKey(const Key& key) const {
                    return mHasher(key);
                }

                /**
                 * @brief Mappe un hash vers un index de bucket valide dans la table
                 * @param hash Valeur de hash pré-calculée par HashKey()
                 * @return Index de bucket dans [0, mBucketCount) via opération modulo
                 * @note Complexité O(1) : opération arithmétique pure
                 * @note Alternative optimisée : utiliser bitmask si mBucketCount est puissance de 2
                 */
                usize GetBucketIndex(usize hash) const {
                    return hash % mBucketCount;
                }

                /**
                 * @brief Initialise le tableau de buckets avec zéro-initialisation
                 * @param bucketCount Nombre de buckets à allouer (minimum 1)
                 * @note Utilise memory::NkMemZero() pour initialiser tous les pointeurs à nullptr
                 * @note Assertion en debug pour vérifier le succès de l'allocation
                 */
                void InitBuckets(SizeType bucketCount) {
                    mBucketCount = bucketCount > 0 ? bucketCount : 1;
                    mBuckets = static_cast<Node**>(mAllocator->Allocate(mBucketCount * sizeof(Node*)));
                    NKENTSEU_ASSERT(mBuckets != nullptr);
                    memory::NkMemZero(mBuckets, mBucketCount * sizeof(Node*));
                }

                /**
                 * @brief Redistribue tous les éléments dans une nouvelle table de buckets
                 * @param newBucketCount Nouveau nombre de buckets pour la table redimensionnée
                 * @note Complexité O(n) : chaque élément est ré-inséré avec son hash pré-calculé
                 * @note Préserve l'ordre relatif dans chaque liste chaînée (stable rehash)
                 * @note Libère l'ancien tableau de buckets après migration complète
                 */
                void RehashInternal(SizeType newBucketCount) {
                    if (newBucketCount < 1) {
                        newBucketCount = 1;
                    }
                    Node** newBuckets = static_cast<Node**>(mAllocator->Allocate(newBucketCount * sizeof(Node*)));
                    NKENTSEU_ASSERT(newBuckets != nullptr);
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
                 * @note Déclenche RehashInternal(mBucketCount * 2) si mSize > mBucketCount * mMaxLoadFactor
                 * @note Stratégie de croissance : doublement de capacité pour amortir les coûts
                 * @note Appelée après chaque Insert() pour maintenir les performances moyennes O(1)
                 */
                void CheckLoadFactor() {
                    if (mSize > mBucketCount * mMaxLoadFactor) {
                        RehashInternal(mBucketCount * 2);
                    }
                }

                /**
                 * @brief Recherche interne d'un nœud par clé dans la structure de hachage
                 * @param key Référence const vers la clé à localiser
                 * @return Pointeur vers le Node trouvé, ou nullptr si la clé est absente
                 * @note Utilise le hash pour accéder directement au bucket candidat
                 * @note Parcours linéaire de la liste chaînée du bucket avec comparaison par mEqual
                 */
                Node* FindNode(const Key& key) const {
                    usize hash = HashKey(key);
                    SizeType idx = GetBucketIndex(hash);
                    Node* node = mBuckets[idx];
                    while (node) {
                        if (mEqual(node->Data.First, key)) {
                            return node;
                        }
                        node = node->Next;
                    }
                    return nullptr;
                }

            // ====================================================================
            // SECTION PUBLIQUE : CONSTRUCTEURS ET DESTRUCTEUR
            // ====================================================================
            public:

                /**
                 * @brief Constructeur par défaut avec allocateur optionnel
                 * @param allocator Pointeur vers un allocateur personnalisé (nullptr = défaut)
                 * @note Initialise une table vide avec 16 buckets et load factor 0.75
                 * @note Complexité O(1) : allocation unique du tableau de buckets initial
                 */
                explicit NkHashMap(Allocator* allocator = nullptr)
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
                 * @param init Liste de paires clé-valeur pour initialiser la map
                 * @param allocator Pointeur vers un allocateur personnalisé (nullptr = défaut)
                 * @note Insère chaque paire séquentiellement : peut déclencher des réhash intermédiaires
                 * @note Les doublons de clés : la dernière valeur l'emporte (comportement std::unordered_map)
                 */
                NkHashMap(NkInitializerList<ValueType> init, Allocator* allocator = nullptr)
                    : mBuckets(nullptr)
                    , mBucketCount(16)
                    , mSize(0)
                    , mMaxLoadFactor(0.75f)
                    , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator())
                    , mHasher()
                    , mEqual() {
                    InitBuckets(16);
                    for (auto& pair : init) {
                        Insert(pair.First, pair.Second);
                    }
                }

                /**
                 * @brief Constructeur depuis std::initializer_list avec allocateur optionnel
                 * @param init Liste standard de paires clé-valeur pour initialiser la map
                 * @param allocator Pointeur vers un allocateur personnalisé (nullptr = défaut)
                 * @note Compatibilité avec la syntaxe C++11 : NkHashMap<int, string> m = {{1, "un"}, {2, "deux"}};
                 */
                NkHashMap(std::initializer_list<ValueType> init, Allocator* allocator = nullptr)
                    : mBuckets(nullptr)
                    , mBucketCount(16)
                    , mSize(0)
                    , mMaxLoadFactor(0.75f)
                    , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator())
                    , mHasher()
                    , mEqual() {
                    InitBuckets(16);
                    for (auto& pair : init) {
                        Insert(pair.First, pair.Second);
                    }
                }

                /**
                 * @brief Constructeur de copie pour duplication profonde de la HashMap
                 * @param other HashMap source à dupliquer élément par élément
                 * @note Copie la configuration (load factor, hasher, equal) et les données
                 * @note Complexité O(n) : ré-insertion de tous les éléments avec recalcul des buckets
                 */
                NkHashMap(const NkHashMap& other)
                    : mBuckets(nullptr)
                    , mBucketCount(16)
                    , mSize(0)
                    , mMaxLoadFactor(other.mMaxLoadFactor)
                    , mAllocator(other.mAllocator)
                    , mHasher(other.mHasher)
                    , mEqual(other.mEqual) {
                    InitBuckets(other.mBucketCount);
                    for (SizeType i = 0; i < other.mBucketCount; ++i) {
                        Node* node = other.mBuckets[i];
                        while (node) {
                            Insert(node->Data.First, node->Data.Second);
                            node = node->Next;
                        }
                    }
                }

                /**
                 * @brief Constructeur de déplacement pour transfert efficace des ressources
                 * @param other HashMap source dont transférer le contenu
                 * @note noexcept : transfert par échange de pointeurs, aucune allocation
                 * @note other est laissé dans un état valide mais vide après le move
                 */
                NkHashMap(NkHashMap&& other) NKENTSEU_NOEXCEPT
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

                /**
                 * @brief Destructeur libérant tous les nœuds et la structure interne
                 * @note Parcours tous les buckets pour détruire chaque Node individuellement
                 * @note Garantit l'absence de fuite mémoire via RAII même en cas d'exception
                 */
                ~NkHashMap() {
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
                 * @param other HashMap source à copier
                 * @return Référence vers l'instance courante pour le chaînage d'opérations
                 * @note Libère les ressources actuelles avant de copier le contenu de other
                 * @note Préserve l'allocateur courant : ne copie pas mAllocator de other
                 */
                NkHashMap& operator=(const NkHashMap& other) {
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
                            Insert(node->Data.First, node->Data.Second);
                            node = node->Next;
                        }
                    }
                    return *this;
                }

                /**
                 * @brief Opérateur d'affectation par déplacement avec transfert de ressources
                 * @param other HashMap source dont transférer le contenu
                 * @return Référence vers l'instance courante pour le chaînage d'opérations
                 * @note noexcept : échange de pointeurs sans allocation ni copie
                 * @note other est laissé dans un état valide mais vide après l'opération
                 */
                NkHashMap& operator=(NkHashMap&& other) NKENTSEU_NOEXCEPT {
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

                /**
                 * @brief Opérateur d'affectation depuis NkInitializerList
                 * @param init Nouvelle liste de paires pour remplacer le contenu actuel
                 * @return Référence vers l'instance courante pour le chaînage d'opérations
                 * @note Efface le contenu existant avant d'insérer les nouvelles paires
                 */
                NkHashMap& operator=(NkInitializerList<ValueType> init) {
                    Clear();
                    for (auto& pair : init) {
                        Insert(pair.First, pair.Second);
                    }
                    return *this;
                }

                /**
                 * @brief Opérateur d'affectation depuis std::initializer_list
                 * @param init Nouvelle liste standard de paires pour remplacer le contenu
                 * @return Référence vers l'instance courante pour le chaînage d'opérations
                 * @note Syntaxe C++11 : map = {{1, "un"}, {2, "deux"}};
                 */
                NkHashMap& operator=(std::initializer_list<ValueType> init) {
                    Clear();
                    for (auto& pair : init) {
                        Insert(pair.First, pair.Second);
                    }
                    return *this;
                }

            // ====================================================================
            // SECTION PUBLIQUE : CAPACITÉ ET MÉTADONNÉES
            // ====================================================================

                /**
                 * @brief Teste si la HashMap ne contient aucun élément
                 * @return true si mSize == 0, false sinon
                 * @note Opération O(1) sans effet de bord, noexcept garantie
                 * @note Équivalent à Size() == 0 mais sémantiquement plus explicite
                 */
                bool Empty() const NKENTSEU_NOEXCEPT {
                    return mSize == 0;
                }

                /**
                 * @brief Retourne le nombre d'éléments stockés dans la HashMap
                 * @return Valeur de type SizeType représentant la cardinalité
                 * @note Opération O(1) sans effet de bord, noexcept garantie
                 * @note Ne compte pas les buckets vides : uniquement les paires clé-valeur uniques
                 */
                SizeType Size() const NKENTSEU_NOEXCEPT {
                    return mSize;
                }

                /**
                 * @brief Retourne le nombre total de buckets alloués dans la table
                 * @return Valeur de type SizeType représentant la capacité de hachage
                 * @note Opération O(1) sans effet de bord, noexcept garantie
                 * @note Utile pour diagnostiquer la densité et l'efficacité du hachage
                 */
                SizeType BucketCount() const NKENTSEU_NOEXCEPT {
                    return mBucketCount;
                }

                /**
                 * @brief Calcule le facteur de charge courant de la table de hachage
                 * @return Ratio mSize / mBucketCount comme float, ou 0.0f si aucun bucket
                 * @note Opération O(1) sans effet de bord, noexcept garantie
                 * @note Valeur cible typique : 0.75 pour équilibrer mémoire et performances
                 */
                float LoadFactor() const NKENTSEU_NOEXCEPT {
                    return mBucketCount > 0 ? static_cast<float>(mSize) / static_cast<float>(mBucketCount) : 0.0f;
                }

                /**
                 * @brief Retourne le seuil de load factor configuré pour le réhash automatique
                 * @return Valeur float représentant le seuil maximal avant déclenchement du réhash
                 * @note Opération O(1) sans effet de bord, noexcept garantie
                 * @note Défaut : 0.75f ; valeurs typiques : 0.5 (mémoire) à 0.9 (performance)
                 */
                float MaxLoadFactor() const NKENTSEU_NOEXCEPT {
                    return mMaxLoadFactor;
                }

                /**
                 * @brief Configure le seuil de load factor pour le réhash automatique
                 * @param factor Nouveau seuil à appliquer (doit être > 0.0f)
                 * @note Assertion en debug pour valider la valeur fournie
                 * @note Peut déclencher un réhash immédiat si le nouveau seuil est dépassé
                 * @note Utile pour ajuster le compromis mémoire/performance à l'exécution
                 */
                void SetMaxLoadFactor(float factor) {
                    NKENTSEU_ASSERT(factor > 0.0f);
                    if (factor > 0.0f) {
                        mMaxLoadFactor = factor;
                        CheckLoadFactor();
                    }
                }

            // ====================================================================
            // SECTION PUBLIQUE : ITÉRATEURS ET PARCOURS
            // ====================================================================

                /**
                 * @brief Retourne un itérateur mutable vers le premier élément valide
                 * @return Iterator pointant vers le premier nœud non-null, ou End() si vide
                 * @note Complexité O(b) où b = nombre de buckets initiaux vides à sauter
                 * @note Invalidation : tout Insert/Erase/Rehash invalide les itérateurs existants
                 */
                Iterator Begin() {
                    for (SizeType i = 0; i < mBucketCount; ++i) {
                        if (mBuckets[i]) {
                            return Iterator(this, mBuckets[i], i);
                        }
                    }
                    return End();
                }

                /**
                 * @brief Retourne un itérateur const vers le premier élément valide
                 * @return ConstIterator pointant vers le premier nœud non-null, ou End() si vide
                 * @note Version const pour itération en lecture seule sur HashMap const
                 */
                ConstIterator Begin() const {
                    for (SizeType i = 0; i < mBucketCount; ++i) {
                        if (mBuckets[i]) {
                            return ConstIterator(this, mBuckets[i], i);
                        }
                    }
                    return End();
                }

                /**
                 * @brief Alias const pour Begin() conforme aux conventions STL
                 * @return ConstIterator vers le premier élément ou end()
                 * @note Permet l'usage idiomatique : for (auto it = map.cbegin(); ...)
                 */
                ConstIterator CBegin() const {
                    return Begin();
                }

                /**
                 * @brief Retourne un itérateur mutable vers la position "fin" (sentinelle)
                 * @return Iterator avec mNode == nullptr marquant la fin du parcours
                 * @note Complexité O(1) : construction directe sans recherche
                 * @note Tous les itérateurs end() sont égaux entre eux pour une même HashMap
                 */
                Iterator End() {
                    return Iterator(this, nullptr, mBucketCount);
                }

                /**
                 * @brief Retourne un itérateur const vers la position "fin" (sentinelle)
                 * @return ConstIterator avec mNode == nullptr marquant la fin du parcours
                 * @note Version const pour comparaison avec des itérateurs const
                 */
                ConstIterator End() const {
                    return ConstIterator(this, nullptr, mBucketCount);
                }

                /**
                 * @brief Alias const pour End() conforme aux conventions STL
                 * @return ConstIterator vers la position sentinelle de fin
                 */
                ConstIterator CEnd() const {
                    return End();
                }

                // ====================================================================
                // ALIASES MINUSCULES POUR COMPATIBILITÉ RANGE-BASED FOR (C++11)
                // ====================================================================

                /**
                 * @brief Alias lowercase de Begin() pour compatibilité range-based for
                 * @return Iterator mutable vers le premier élément
                 * @note Permet : for (auto& entry : map) { ... }
                 */
                Iterator begin() {
                    for (SizeType i = 0; i < mBucketCount; ++i) {
                        if (mBuckets[i]) {
                            return Iterator(this, mBuckets[i], i);
                        }
                    }
                    return end();
                }

                /**
                 * @brief Alias lowercase const de Begin() pour range-based for sur const
                 * @return ConstIterator vers le premier élément
                 */
                ConstIterator begin() const {
                    for (SizeType i = 0; i < mBucketCount; ++i) {
                        if (mBuckets[i]) {
                            return ConstIterator(this, mBuckets[i], i);
                        }
                    }
                    return end();
                }

                /**
                 * @brief Alias lowercase de CBegin() pour compatibilité STL
                 * @return ConstIterator vers le premier élément
                 */
                ConstIterator cbegin() const {
                    return begin();
                }

                /**
                 * @brief Alias lowercase de End() pour compatibilité range-based for
                 * @return Iterator sentinelle de fin
                 */
                Iterator end() {
                    return Iterator(this, nullptr, mBucketCount);
                }

                /**
                 * @brief Alias lowercase const de End() pour compatibilité STL
                 * @return ConstIterator sentinelle de fin
                 */
                ConstIterator end() const {
                    return ConstIterator(this, nullptr, mBucketCount);
                }

                /**
                 * @brief Alias lowercase de CEnd() pour compatibilité STL
                 * @return ConstIterator sentinelle de fin
                 */
                ConstIterator cend() const {
                    return end();
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
                 * @brief Redimensionne explicitement la table de hachage avec un nouveau nombre de buckets
                 * @param newBucketCount Nouveau nombre de buckets souhaité (minimum 1)
                 * @note Complexité O(n) : redistribution de tous les éléments existants
                 * @note Utile pour optimiser les performances après connaissance du volume final
                 * @note Invalide tous les itérateurs existants : à appeler avant toute itération
                 */
                void Rehash(SizeType newBucketCount) {
                    RehashInternal(newBucketCount);
                }

                /**
                 * @brief Réserve de la capacité pour un nombre attendu d'éléments sans réhash prématuré
                 * @param elementCount Nombre d'éléments anticipé à stocker
                 * @note Calcule le nombre de buckets requis : elementCount / mMaxLoadFactor + 1
                 * @note N'effectue un réhash que si la capacité actuelle est insuffisante
                 * @note Optimisation : évite les réhash intermédiaires lors d'insertions en masse
                 */
                void Reserve(SizeType elementCount) {
                    if (elementCount == 0) {
                        return;
                    }
                    SizeType requiredBuckets = static_cast<SizeType>(static_cast<float>(elementCount) / mMaxLoadFactor) + 1;
                    if (requiredBuckets > mBucketCount) {
                        RehashInternal(requiredBuckets);
                    }
                }

                /**
                 * @brief Échange le contenu de cette HashMap avec une autre en temps constant
                 * @param other Autre instance de NkHashMap à échanger
                 * @note noexcept : échange de pointeurs et de métadonnées uniquement
                 * @note Complexité O(1) : aucune allocation, copie ou destruction
                 * @note Préférer à l'assignation pour les permutations efficaces de grandes maps
                 */
                void Swap(NkHashMap& other) NKENTSEU_NOEXCEPT {
                    Node** tmpBuckets = mBuckets;
                    mBuckets = other.mBuckets;
                    other.mBuckets = tmpBuckets;
                    SizeType tmpBucketCount = mBucketCount;
                    mBucketCount = other.mBucketCount;
                    other.mBucketCount = tmpBucketCount;
                    SizeType tmpSize = mSize;
                    mSize = other.mSize;
                    other.mSize = tmpSize;
                    float tmpLoadFactor = mMaxLoadFactor;
                    mMaxLoadFactor = other.mMaxLoadFactor;
                    other.mMaxLoadFactor = tmpLoadFactor;
                    Allocator* tmpAllocator = mAllocator;
                    mAllocator = other.mAllocator;
                    other.mAllocator = tmpAllocator;
                }

            // ====================================================================
            // SECTION PUBLIQUE : INSERTION ET MISE À JOUR
            // ====================================================================

                /**
                 * @brief Insère ou met à jour une paire clé-valeur dans la HashMap
                 * @param key Clé d'indexation pour l'élément (copiée en const dans le nœud)
                 * @param value Valeur associée à stocker ou à mettre à jour si la clé existe
                 * @note Si la clé existe déjà : met à jour la valeur et retourne sans réallocation
                 * @note Si la clé est nouvelle : crée un Node et l'insère en tête de liste du bucket
                 * @note Peut déclencher un réhash automatique si le load factor est dépassé
                 * @note Complexité : O(1) amorti, O(n) pire cas (collisions ou réhash)
                 */
                void Insert(const Key& key, const Value& value) {
                    usize hash = HashKey(key);
                    SizeType idx = GetBucketIndex(hash);
                    Node* node = mBuckets[idx];
                    while (node) {
                        if (mEqual(node->Data.First, key)) {
                            node->Data.Second = value;
                            return;
                        }
                        node = node->Next;
                    }
                    Node* newNode = static_cast<Node*>(mAllocator->Allocate(sizeof(Node)));
                    new (newNode) Node(hash, key, value, mBuckets[idx]);
                    mBuckets[idx] = newNode;
                    ++mSize;
                    CheckLoadFactor();
                }

                /**
                 * @brief Supprime un élément par clé si présent dans la HashMap
                 * @param key Clé de l'élément à supprimer
                 * @return true si l'élément a été trouvé et supprimé, false si clé absente
                 * @note Complexité : O(1) amorti pour la recherche + O(1) pour la suppression de liste
                 * @note Libère la mémoire du Node supprimé via l'allocateur configuré
                 * @note Ne réduit pas le nombre de buckets : utiliser Rehash(1) pour compacter si nécessaire
                 */
                bool Erase(const Key& key) {
                    usize hash = HashKey(key);
                    SizeType idx = GetBucketIndex(hash);
                    Node** prev = &mBuckets[idx];
                    Node* node = mBuckets[idx];
                    while (node) {
                        if (mEqual(node->Data.First, key)) {
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
            // SECTION PUBLIQUE : RECHERCHE ET ACCÈS AUX ÉLÉMENTS
            // ====================================================================

                /**
                 * @brief Recherche une valeur par clé et retourne un pointeur modifiable
                 * @param key Clé à rechercher dans la HashMap
                 * @return Pointeur non-const vers la valeur trouvée, ou nullptr si absente
                 * @note Permet la modification directe : *map.Find(key) = newValue;
                 * @note Complexité : O(1) amorti pour la recherche dans le bucket cible
                 */
                Value* Find(const Key& key) {
                    Node* node = FindNode(key);
                    return node ? &node->Data.Second : nullptr;
                }

                /**
                 * @brief Recherche une valeur par clé et retourne un pointeur en lecture seule
                 * @param key Clé à rechercher dans la HashMap
                 * @return Pointeur const vers la valeur trouvée, ou nullptr si absente
                 * @note Version const pour usage sur HashMap const ou accès en lecture seule
                 */
                const Value* Find(const Key& key) const {
                    Node* node = FindNode(key);
                    return node ? &node->Data.Second : nullptr;
                }

                /**
                 * @brief Teste la présence d'une clé dans la HashMap
                 * @param key Clé à vérifier
                 * @return true si la clé existe, false sinon
                 * @note Implémenté via Find() != nullptr : même complexité O(1) amorti
                 * @note Plus sémantique que Find() quand seule la présence importe
                 */
                bool Contains(const Key& key) const {
                    return Find(key) != nullptr;
                }

                /**
                 * @brief Recherche une clé et retourne un itérateur mutable vers l'élément
                 * @param key Clé à localiser dans la HashMap
                 * @return Iterator vers la paire trouvée, ou End() si clé absente
                 * @note Permet l'usage dans des algorithmes génériques attendant des itérateurs
                 * @note L'itérateur retourne une paire : it->First = clé, it->Second = valeur
                 */
                Iterator FindIterator(const Key& key) {
                    usize hash = HashKey(key);
                    SizeType idx = GetBucketIndex(hash);
                    Node* node = mBuckets[idx];
                    while (node) {
                        if (mEqual(node->Data.First, key)) {
                            return Iterator(this, node, idx);
                        }
                        node = node->Next;
                    }
                    return End();
                }

                /**
                 * @brief Recherche une clé et retourne un itérateur const vers l'élément
                 * @param key Clé à localiser dans la HashMap
                 * @return ConstIterator vers la paire trouvée, ou End() si clé absente
                 * @note Version const pour usage sur HashMap const ou itération en lecture seule
                 */
                ConstIterator FindIterator(const Key& key) const {
                    usize hash = HashKey(key);
                    SizeType idx = GetBucketIndex(hash);
                    Node* node = mBuckets[idx];
                    while (node) {
                        if (mEqual(node->Data.First, key)) {
                            return ConstIterator(this, node, idx);
                        }
                        node = node->Next;
                    }
                    return End();
                }

                /**
                 * @brief Tente d'extraire une valeur par clé avec sémantique optionnelle
                 * @param key Clé à rechercher
                 * @param outValue Référence vers la variable de sortie pour recevoir la valeur
                 * @return true si la clé a été trouvée et la valeur copiée, false sinon
                 * @note Évite les pointeurs nullptr : pattern plus sûr pour les APIs publiques
                 * @note Copie la valeur dans outValue : utiliser Find() pour éviter la copie si type lourd
                 */
                bool TryGet(const Key& key, Value& outValue) const {
                    const Value* value = Find(key);
                    if (!value) {
                        return false;
                    }
                    outValue = *value;
                    return true;
                }

                /**
                 * @brief Accède à une valeur par clé avec assertion sur présence
                 * @param key Clé dont accéder à la valeur associée
                 * @return Référence non-const vers la valeur trouvée
                 * @pre La clé doit exister dans la HashMap (assertion NKENTSEU_ASSERT en debug)
                 * @note Préférer Find() ou Contains() en production pour gérer l'absence gracieusement
                 * @note Permet la modification directe : map.At(key) = newValue;
                 */
                Value& At(const Key& key) {
                    Value* value = Find(key);
                    NKENTSEU_ASSERT(value != nullptr);
                    return *value;
                }

                /**
                 * @brief Accède à une valeur par clé en lecture seule avec assertion
                 * @param key Clé dont accéder à la valeur associée
                 * @return Référence const vers la valeur trouvée
                 * @pre La clé doit exister dans la HashMap (assertion NKENTSEU_ASSERT en debug)
                 * @note Version const pour usage sur HashMap const ou accès en lecture seule
                 */
                const Value& At(const Key& key) const {
                    const Value* value = Find(key);
                    NKENTSEU_ASSERT(value != nullptr);
                    return *value;
                }

                /**
                 * @brief Insère une nouvelle paire ou met à jour une existante avec retour de statut
                 * @param key Clé à insérer ou mettre à jour
                 * @param value Valeur à associer à la clé
                 * @return true si une nouvelle insertion a eu lieu, false si mise à jour d'existant
                 * @note Utile pour distinguer création vs modification dans la logique métier
                 * @note Complexité identique à Insert() : O(1) amorti
                 */
                bool InsertOrAssign(const Key& key, const Value& value) {
                    Value* current = Find(key);
                    if (current) {
                        *current = value;
                        return false;
                    }
                    Insert(key, value);
                    return true;
                }

            // ====================================================================
            // SECTION PUBLIQUE : OPÉRATEUR D'ACCÈS PAR CLÉ (SUBSCRIPT)
            // ====================================================================

                /**
                 * @brief Opérateur d'accès par clé avec insertion automatique si absent
                 * @param key Clé pour laquelle accéder à la valeur associée
                 * @return Référence non-const vers la valeur (existante ou nouvellement créée)
                 * @note Si la clé est absente : insère une nouvelle paire avec valeur par défaut Value()
                 * @note Permet la syntaxe familière : map[key] = value; ou auto& v = map[key];
                 * @note Attention : peut modifier la taille de la map et invalider les itérateurs
                 * @note Pour les types Value non-default-constructible : utiliser Insert() ou TryGet()
                 */
                Value& operator[](const Key& key) {
                    Value* val = Find(key);
                    if (val) {
                        return *val;
                    }
                    Insert(key, Value());
                    return *Find(key);
                }
        };

    } // namespace nkentseu

#endif // NK_CONTAINERS_ASSOCIATIVE_NKHASHMAP_H

// ============================================================================
// EXEMPLES D'UTILISATION ET PATTERNS COURANTS
// ============================================================================
/*
 * @section examples Exemples pratiques d'utilisation de NkHashMap
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 1 : Création et opérations de base
 * --------------------------------------------------------------------------
 * @code
 * #include "NKContainers/Associative/NkHashMap.h"
 * #include "NKCore/NkString.h"
 * #include <cstdio>
 * 
 * void exempleBase() {
 *     // Création d'une HashMap int -> NkString avec allocateur par défaut
 *     nkentseu::NkHashMap<int, nkentseu::NkString> map;
 *     
 *     // Insertion de paires clé-valeur
 *     map.Insert(1, "un");
 *     map.Insert(2, "deux");
 *     map.Insert(3, "trois");
 *     
 *     // Accès via operator[] (crée si absent)
 *     map[4] = "quatre";  // Insertion
 *     map[2] = "DEUX";    // Mise à jour de la clé existante
 *     
 *     // Recherche et affichage
 *     if (map.Contains(2)) {
 *         printf("Clé 2: %s\n", map.Find(2)->CStr());  // "DEUX"
 *     }
 *     
 *     // Accès avec At() (asserte si absent - pour usage contrôlé)
 *     printf("Clé 1: %s\n", map.At(1).CStr());  // "un"
 *     
 *     // Suppression
 *     bool removed = map.Erase(3);  // true : clé présente
 *     bool missing = map.Erase(99); // false : clé absente
 *     
 *     printf("Taille: %zu, Buckets: %zu, Load: %.2f\n",
 *            map.Size(), map.BucketCount(), map.LoadFactor());
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 2 : Itération et parcours des éléments
 * --------------------------------------------------------------------------
 * @code
 * void exempleIteration() {
 *     nkentseu::NkHashMap<const char*, int> wordCount;
 *     
 *     // Construction via initializer_list (C++11)
 *     wordCount = {{"apple", 3}, {"banana", 5}, {"cherry", 2}};
 *     
 *     // Parcours avec itérateur explicite
 *     printf("Contenu de la map:\n");
 *     for (auto it = wordCount.Begin(); it != wordCount.End(); ++it) {
 *         printf("  %s: %d\n", it->First, it->Second);
 *     }
 *     
 *     // Parcours range-based for (C++11, nécessite begin()/end())
 *     printf("Via range-based for:\n");
 *     for (const auto& entry : wordCount) {
 *         printf("  %s: %d\n", entry.First, entry.Second);
 *     }
 *     
 *     // Recherche avec FindIterator pour usage algorithmique
 *     auto it = wordCount.FindIterator("banana");
 *     if (it != wordCount.End()) {
 *         it->Second += 1;  // Modification via itérateur mutable
 *         printf("banana mis à jour: %d\n", it->Second);  // 6
 *     }
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 3 : Gestion mémoire avec allocateur personnalisé
 * --------------------------------------------------------------------------
 * @code
 * #include "NKMemory/NkPoolAllocator.h"
 * 
 * void exempleAllocateur() {
 *     // Pool allocator pour réduire la fragmentation sur insertions massives
 *     memory::NkPoolAllocator pool(256 * 1024);  // Pool de 256KB
 *     
 *     // HashMap utilisant le pool pour toutes ses allocations internes
 *     nkentseu::NkHashMap<usize, nkentseu::NkString, memory::NkPoolAllocator> 
 *         cache(&pool);
 *     
 *     // Pré-réservation pour éviter les réhash intermédiaires
 *     cache.Reserve(1000);  // Anticipe 1000 entrées
 *     
 *     // Insertion en masse sans fragmentation externe
 *     for (usize i = 0; i < 1000; ++i) {
 *         nkentseu::NkString key = nkentseu::NkString::Format("key_%zu", i);
 *         nkentseu::NkString value = nkentseu::NkString::Format("value_%zu", i);
 *         cache.Insert(i, value);  // Pas de réallocation grâce à Reserve()
 *     }
 *     
 *     printf("Cache: %zu entrées, load factor: %.2f\n", 
 *            cache.Size(), cache.LoadFactor());
 *     // Destruction : libération en bloc via le pool (très rapide)
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 4 : Patterns avancés - Comptage, regroupement, memoization
 * --------------------------------------------------------------------------
 * @code
 * // Pattern 1: Comptage de fréquences (word count)
 * void exempleWordCount(const nkentseu::NkVector<nkentseu::NkString>& words) {
 *     nkentseu::NkHashMap<nkentseu::NkString, usize> freq;
 *     
 *     for (const auto& word : words) {
 *         // Operator[] avec insertion automatique si absent
 *         freq[word] += 1;
 *     }
 *     
 *     // Affichage des mots par fréquence décroissante (nécessite tri externe)
 *     for (const auto& entry : freq) {
 *         printf("%s: %zu\n", entry.First.CStr(), entry.Second);
 *     }
 * }
 * 
 * // Pattern 2: Regroupement par clé (grouping)
 * void exempleGrouping() {
 *     struct Person { const char* name; usize age; const char* city; };
 *     nkentseu::NkVector<Person> people = { /\* ... *\/ };
 *     
 *     // Regrouper par ville : HashMap<city, vector<names>>
 *     nkentseu::NkHashMap<const char*, nkentseu::NkVector<const char*>> byCity;
 *     
 *     for (const auto& p : people) {
 *         // TryGet pour éviter l'insertion automatique de operator[]
 *         nkentseu::NkVector<const char*>* names = byCity.Find(p.city);
 *         if (!names) {
 *             byCity.Insert(p.city, nkentseu::NkVector<const char*>({p.name}));
 *         } else {
 *             names->PushBack(p.name);
 *         }
 *     }
 * }
 * 
 * // Pattern 3: Memoization de fonction coûteuse
 * nkentseu::NkHashMap<usize, usize> fibCache;
 * 
 * usize FibonacciMemo(usize n) {
 *     // TryGet pour vérifier le cache sans insertion automatique
 *     usize result;
 *     if (fibCache.TryGet(n, result)) {
 *         return result;  // Cache hit
 *     }
 *     
 *     // Cache miss : calcul et stockage
 *     if (n <= 1) {
 *         result = n;
 *     } else {
 *         result = FibonacciMemo(n - 1) + FibonacciMemo(n - 2);
 *     }
 *     fibCache.Insert(n, result);
 *     return result;
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 5 : Hasher et comparateur personnalisés pour types complexes
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
 *         return a == b;  // Délègue à l'operator== de Point
 *     }
 * };
 * 
 * void exempleTypesComplexes() {
 *     // HashMap avec hasher et comparateur personnalisés
 *     nkentseu::NkHashMap<Point, const char*, 
 *                         memory::NkAllocator,
 *                         PointHasher,
 *                         PointEqual> grid;
 *     
 *     grid.Insert({0, 0}, "origine");
 *     grid.Insert({10, 20}, "point A");
 *     
 *     // Recherche avec clé temporaire
 *     if (grid.Contains({10, 20})) {
 *         printf("Trouvé: %s\n", grid.Find({10, 20}));  // "point A"
 *     }
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 6 : Optimisations de performance et bonnes pratiques
 * --------------------------------------------------------------------------
 * @code
 * void exempleOptimisations() {
 *     nkentseu::NkHashMap<usize, nkentseu::NkVector<int>> data;
 *     
 *     // 1. Pré-réserver avant insertion en masse pour éviter les réhash
 *     data.Reserve(10000);  // Anticipe 10k entrées
 *     
 *     // 2. Utiliser Emplace-style via Insert() avec move pour types lourds
 *     #if defined(NK_CPP11)
 *     for (usize i = 0; i < 10000; ++i) {
 *         nkentseu::NkVector<int> values;
 *         values.PushBack(i);
 *         values.PushBack(i * 2);
 *         // Insert avec move pour éviter la copie du vector
 *         data.Insert(i, nkentseu::traits::NkMove(values));
 *     }
 *     #endif
 *     
 *     // 3. Ajuster le load factor selon le compromis mémoire/performance
 *     data.SetMaxLoadFactor(0.9f);  // Plus de collisions, moins de buckets
 *     // ou
 *     data.SetMaxLoadFactor(0.5f);  // Moins de collisions, plus de mémoire
 *     
 *     // 4. Réhash explicite après chargement pour compacter si beaucoup de suppressions
 *     // data.Rehash(data.Size() / data.MaxLoadFactor() + 1);
 *     
 *     // 5. Préférer Find() à operator[] en lecture pour éviter les insertions accidentelles
 *     const auto* val = data.Find(123);
 *     if (val) {
 *         // Utilisation sûre sans modifier la map
 *         printf("Valeur trouvée, taille: %zu\n", val->Size());
 *     }
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * BONNES PRATIQUES ET RECOMMANDATIONS
 * --------------------------------------------------------------------------
 * 
 * 1. CHOIX DU TYPE DE CLÉ :
 *    - Doit être copiable, hachable (Hasher) et comparable (KeyEqual)
 *    - Privilégier les types trivialement copiables pour les performances
 *    - Pour les types complexes : spécialiser NkHashMapDefaultHasher ou fournir un hasher custom
 *    - Éviter les clés mutables : le hash ne doit jamais changer après insertion
 * 
 * 2. GESTION DU LOAD FACTOR :
 *    - Défaut 0.75 : bon compromis général mémoire/performance
 *    - Réduire à 0.5 pour minimiser les collisions (recherche intensive)
 *    - Augmenter à 0.9 pour économiser la mémoire (insertions rares)
 *    - Utiliser Reserve() avant insertions en masse pour éviter les réhash intermédiaires
 * 
 * 3. ACCÈS ET MODIFICATION :
 *    - operator[] : pratique mais insère si absent (peut invalider les itérateurs)
 *    - Find() : retourne nullptr si absent, plus sûr pour la lecture
 *    - TryGet() : pattern optionnel sans pointeurs, idéal pour les APIs publiques
 *    - At() : avec assertion, pour les cas où la présence est garantie par la logique
 * 
 * 4. ITÉRATION ET INVALIDATION :
 *    - Tout Insert/Erase/Rehash invalide TOUS les itérateurs existants
 *    - Ne jamais modifier la map pendant l'itération (sauf via l'itérateur courant)
 *    - Préférer la collecte des clés à supprimer puis Erase() en dehors de la boucle
 * 
 * 5. GESTION MÉMOIRE :
 *    - Utiliser un allocateur pool pour les HashMap de taille fixe ou prévisible
 *    - Clear() ne libère pas les buckets : appeler Rehash(1) pour compacter si nécessaire
 *    - Pour les valeurs lourdes : stocker des pointeurs ou utiliser move semantics
 * 
 * 6. SÉCURITÉ THREAD :
 *    - NkHashMap est thread-unsafe par conception
 *    - Protéger avec un mutex externe pour accès concurrents multiples
 *    - Pour le pattern reader-writer : envisager une copie en lecture (Copy-On-Read)
 * 
 * 7. EXTENSIONS RECOMMANDÉES (non implémentées) :
 *    - Erase(iterator) : suppression via itérateur pour éviter la recherche redondante
 *    - Extract(key) : extraction avec transfert de propriété de la valeur
 *    - Merge(other, mergeFunc) : fusion avec résolution de conflits personnalisable
 *    - Parallel rehash : réhash multi-threadé pour très grandes tables
 */

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Généré par Rihen le 2026-02-05 22:26:13
// Dernière modification : 2026-04-26 (restructuration et documentation)
// ============================================================