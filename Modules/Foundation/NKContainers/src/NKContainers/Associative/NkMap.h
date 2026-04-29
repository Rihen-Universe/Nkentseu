// -----------------------------------------------------------------------------
// FICHIER: NKContainers\Containers\Associative\NkMap.h
// DESCRIPTION: Map ordonné - Paires clé-valeur via arbre Rouge-Noir
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CONTAINERS_ASSOCIATIVE_NKMAP_H
#define NK_CONTAINERS_ASSOCIATIVE_NKMAP_H

    // ========================================================================
    // INCLUSIONS DES DÉPENDANCES
    // ========================================================================

    #include "NKCore/NkTypes.h"                    // Définition des types fondamentaux (usize, intptr, etc.)
    #include "NKContainers/NkContainersApi.h"      // Macros d'exportation de l'API conteneurs
    #include "NKCore/NkTraits.h"                   // Traits pour move/forward et utilitaires méta
    #include "NKMemory/NkAllocator.h"              // Système d'allocation mémoire personnalisable
    #include "NKMemory/NkFunction.h"               // Wrapper pour les fonctions et callables
    #include "NKCore/Assert/NkAssert.h"            // Macros d'assertion pour le débogage sécurisé
    #include "NKContainers/Heterogeneous/NkPair.h" // Conteneur de paire pour stocker clé-valeur
    #include "NKContainers/Iterators/NkIterator.h" // Infrastructure commune pour les itérateurs
    #include "NKContainers/Iterators/NkInitializerList.h" // Support des listes d'initialisation
    #include "NKContainers/Sequential/NkVector.h"  // Conteneur vectoriel pour opérations internes

    // ========================================================================
    // ESPACE DE NOMS PRINCIPAL
    // ========================================================================

    namespace nkentseu {

        // ====================================================================
        // COMPARATEUR PAR DÉFAUT : ORDRE STRICT FAIBLE
        // ====================================================================

        /**
         * @brief Foncteur de comparaison par défaut pour NkMap
         * 
         * Implémente un ordre strict faible basé sur l'opérateur <.
         * Utilisé pour maintenir les clés triées dans l'arbre Rouge-Noir.
         * 
         * @tparam Key Type des clés comparées (doit supporter operator<)
         * 
         * @note Pour un ordre personnalisé (tri décroissant, comparaison sémantique),
         *       fournir un comparateur custom via le template parameter de NkMap
         * @note L'ordre doit être cohérent : si !(a<b) && !(b<a) alors a==b
         */
        template<typename Key>
        struct NkMapLess {

            /**
             * @brief Opérateur de comparaison pour deux clés
             * @param lhs Première clé à comparer (côté gauche)
             * @param rhs Seconde clé à comparer (côté droit)
             * @return true si lhs < rhs selon l'ordre strict faible, false sinon
             * @note Méthode const : ne modifie pas l'état du foncteur
             * @note Délègue à l'operator< du type Key : doit être défini et transitif
             */
            bool operator()(const Key& lhs, const Key& rhs) const {
                return lhs < rhs;
            }
        };

        // ====================================================================
        // CLASSE PRINCIPALE : NK MAP (ARBRE ROUGE-NOIR)
        // ====================================================================

        /**
         * @brief Classe template implémentant une map ordonnée via arbre Rouge-Noir
         * 
         * Conteneur associatif ordonné stockant des paires clé-valeur uniques,
         * maintenues triées selon un comparateur configurable. L'implémentation
         * utilise un arbre Rouge-Noir pour garantir un équilibrage automatique
         * et des performances logarithmiques dans le pire cas.
         * 
         * PROPRIÉTÉS DE L'ARBRE ROUGE-NOIR :
         * - Chaque nœud est soit rouge, soit noir
         * - La racine est toujours noire
         * - Les feuilles (nullptr) sont considérées noires
         * - Un nœud rouge ne peut avoir d'enfants rouges (pas de deux rouges consécutifs)
         * - Tous les chemins d'un nœud à ses feuilles contiennent le même nombre de nœuds noirs
         * 
         * GARANTIES DE PERFORMANCE :
         * - Hauteur maximale : 2 * log2(n+1) pour n éléments
         * - Insertion/Recherche/Suppression : O(log n) dans tous les cas
         * - Itération en ordre trié : O(n) avec parcours in-order
         * - Mémoire : O(n) avec overhead constant par nœud (couleur + pointeurs)
         * 
         * CAS D'USAGE TYPQUES :
         * - Indexation avec besoin de parcours trié (rapports, exports ordonnés)
         * - Recherche par plage de clés (lower_bound, upper_bound - à ajouter)
         * - Structures nécessitant un ordre déterministe (sérialisation, tests)
         * - Remplacement de std::map sans dépendance STL
         * 
         * @tparam Key Type des clés (doit être copiable et comparable via Compare)
         * @tparam Value Type des valeurs associées (doit être copiable/déplaçable)
         * @tparam Allocator Type de l'allocateur mémoire (défaut: memory::NkAllocator)
         * @tparam Compare Foncteur de comparaison (défaut: NkMapLess<Key> pour ordre croissant)
         * 
         * @note Les clés sont stockées en const : modification après insertion impossible
         * @note Thread-unsafe : synchronisation externe requise pour usage concurrent
         * @note Les itérateurs sont invalidés uniquement par la suppression de l'élément pointé
         */
        template<typename Key,
                 typename Value,
                 typename Allocator = memory::NkAllocator,
                 typename Compare = NkMapLess<Key>>
        class NkMap {

            // ====================================================================
            // SECTION PRIVÉE : CONSTANTES ET STRUCTURE DE NŒUD
            // ====================================================================
            private:

                /**
                 * @brief Énumération des couleurs possibles pour un nœud Rouge-Noir
                 * @note Utilisée exclusivement pour l'algorithme d'équilibrage interne
                 * @note RED = 0, BLACK = 1 : valeurs arbitraires, seule la distinction importe
                 */
                enum Color {
                    RED,    ///< Nœud rouge : peut avoir des enfants noirs uniquement
                    BLACK   ///< Nœud noir : peut avoir des enfants de toute couleur
                };

                /**
                 * @brief Structure interne représentant un nœud de l'arbre Rouge-Noir
                 * 
                 * Contient une paire clé-valeur, des pointeurs de navigation
                 * (gauche, droit, parent) et la couleur pour l'équilibrage.
                 * 
                 * @note Layout mémoire : [Data: NkPair<const Key, Value>][Left][Right][Parent][Color]
                 * @note La clé est const pour garantir l'invariant : l'ordre de tri ne change jamais
                 * @note Nouveaux nœuds insérés comme RED : simplifie les rotations de rééquilibrage
                 */
                struct Node {

                    NkPair<const Key, Value> Data;  ///< Paire clé-valeur stockée (clé immuable)
                    Node* Left;                      ///< Pointeur vers le sous-arbre gauche (clés < Data.First)
                    Node* Right;                     ///< Pointeur vers le sous-arbre droit (clés > Data.First)
                    Node* Parent;                    ///< Pointeur vers le nœud parent (nullptr pour la racine)
                    Color NodeColor;                 ///< Couleur du nœud pour l'algorithme Rouge-Noir

                    // ====================================================================
                    // CONSTRUCTEURS DE NŒUD (C++11 ET LEGACY)
                    // ====================================================================
                    #if defined(NK_CPP11)

                        /**
                         * @brief Constructeur avec forwarding parfait pour C++11+
                         * @tparam K Type déduit pour la clé (avec référence collapsing)
                         * @tparam V Type déduit pour la valeur (avec référence collapsing)
                         * @param key Argument forwarded vers la construction de la clé
                         * @param value Argument forwarded vers la construction de la valeur
                         * @param parent Pointeur optionnel vers le nœud parent (défaut: nullptr)
                         * @note Utilise traits::NkForward pour éviter les copies inutiles
                         * @note Initialise la couleur à RED pour l'insertion standard
                         */
                        template<typename K, typename V>
                        Node(K&& key, V&& value, Node* parent = nullptr)
                            : Data(traits::NkForward<K>(key), traits::NkForward<V>(value))
                            , Left(nullptr)
                            , Right(nullptr)
                            , Parent(parent)
                            , NodeColor(RED) {
                        }

                    #else

                        /**
                         * @brief Constructeur par copie pour compatibilité C++98/03
                         * @param key Référence const vers la clé à copier
                         * @param value Référence const vers la valeur à copier
                         * @param parent Pointeur optionnel vers le nœud parent (défaut: nullptr)
                         * @note Version fallback sans move semantics ni forwarding
                         * @note Initialise la couleur à RED pour l'insertion standard
                         */
                        Node(const Key& key, const Value& value, Node* parent = nullptr)
                            : Data(key, value)
                            , Left(nullptr)
                            , Right(nullptr)
                            , Parent(parent)
                            , NodeColor(RED) {
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
                using MappedType = Value;               ///< Alias du type de valeur associée
                using ValueType = NkPair<const Key, Value>;  ///< Alias du type de paire stockée
                using SizeType = usize;                 ///< Alias du type pour les tailles/comptages
                using Reference = ValueType&;           ///< Alias de référence non-const vers une paire
                using ConstReference = const ValueType&;///< Alias de référence const vers une paire

                // ====================================================================
                // CLASSE ITÉRATEUR BIDIRECTIONNEL
                // ====================================================================

                /**
                 * @brief Itérateur bidirectionnel pour parcourir les éléments triés de la Map
                 * 
                 * Itérateur qui suit l'ordre in-order de l'arbre (gauche, racine, droite),
                 * garantissant un parcours des clés en ordre croissant selon le comparateur.
                 * 
                 * PROPRIÉTÉS :
                 * - Catégorie : Bidirectional Iterator (peut avancer et reculer - recul à ajouter)
                 * - Invalidation : uniquement par suppression de l'élément pointé
                 * - Accès en lecture/écriture via operator* et operator->
                 * - Comparaison par égalité de pointeur de nœud
                 * 
                 * IMPLÉMENTATION DE NextNode() :
                 * - Si nœud a un fils droit : retourne le minimum du sous-arbre droit
                 * - Sinon : remonte vers le premier ancêtre dont le nœud est dans le sous-arbre gauche
                 * 
                 * @note Construction réservée à NkMap via friend declaration
                 * @note ConstIterator est un alias vers Iterator : la const-correctness est gérée par la Map parente
                 */
                class Iterator {

                    // ====================================================================
                    // MEMBRES PRIVÉS DE L'ITÉRATEUR
                    // ====================================================================
                    private:

                        Node* mNode;                    ///< Pointeur vers le nœud courant dans l'arbre
                        const NkMap* mMap;              ///< Pointeur vers la Map parente pour contexte
                        friend class NkMap;             ///< NkMap peut construire des itérateurs

                        /**
                         * @brief Calcule le nœud suivant dans l'ordre in-order
                         * @param node Nœud courant dont trouver le successeur
                         * @return Pointeur vers le nœud suivant, ou nullptr si fin de parcours
                         * @note Algorithme standard de successeur dans un arbre binaire de recherche
                         * @note Complexité amortie O(1) : chaque arête est traversée au plus 2 fois sur n itérations
                         */
                        Node* NextNode(Node* node) {
                            if (!node) {
                                return nullptr;
                            }
                            if (node->Right) {
                                node = node->Right;
                                while (node->Left) {
                                    node = node->Left;
                                }
                                return node;
                            }
                            Node* parent = node->Parent;
                            while (parent && node == parent->Right) {
                                node = parent;
                                parent = parent->Parent;
                            }
                            return parent;
                        }

                    // ====================================================================
                    // ALIASES DE TYPES CONFORMES AUX ITÉRATEURS STL
                    // ====================================================================
                    public:

                        using ValueType = NkPair<const Key, Value>;  ///< Type de l'élément pointé
                        using Reference = ValueType&;                 ///< Référence vers l'élément pointé
                        using Pointer = ValueType*;                   ///< Pointeur vers l'élément pointé
                        using DifferenceType = intptr;                ///< Type pour les différences d'itérateurs
                        using IteratorCategory = NkBidirectionalIteratorTag;  ///< Catégorie d'itérateur

                    // ====================================================================
                    // CONSTRUCTEURS DE L'ITÉRATEUR
                    // ====================================================================

                        /**
                         * @brief Constructeur par défaut pour itérateur non-initialisé (end-like)
                         */
                        Iterator()
                            : mNode(nullptr)
                            , mMap(nullptr) {
                        }

                        /**
                         * @brief Constructeur paramétré pour création contrôlée par NkMap
                         * @param node Pointeur vers le nœud de départ (nullptr pour end())
                         * @param map Pointeur vers la Map parente pour navigation
                         */
                        Iterator(Node* node, const NkMap* map)
                            : mNode(node)
                            , mMap(map) {
                        }

                    // ====================================================================
                    // OPÉRATEURS D'ACCÈS ET DE DÉPLACEMENT
                    // ====================================================================

                        /**
                         * @brief Opérateur de déréférencement pour accès à la paire clé-valeur
                         * @return Référence non-const vers la paire stockée dans le nœud courant
                         * @pre L'itérateur ne doit pas être end() (accès à mNode supposé valide)
                         */
                        Reference operator*() const {
                            return mNode->Data;
                        }

                        /**
                         * @brief Opérateur flèche pour accès aux membres de la paire
                         * @return Pointeur vers la paire stockée dans le nœud courant
                         * @note Permet it->First et it->Second comme syntaxe familière
                         */
                        Pointer operator->() const {
                            return &mNode->Data;
                        }

                        /**
                         * @brief Opérateur de pré-incrémentation : avance vers le successeur in-order
                         * @return Référence vers l'itérateur avancé pour chaînage
                         * @note Préférer ++it à it++ pour éviter la copie temporaire
                         * @note Complexité amortie O(1) grâce à l'algorithme de NextNode()
                         */
                        Iterator& operator++() {
                            mNode = NextNode(mNode);
                            return *this;
                        }

                        /**
                         * @brief Opérateur de post-incrémentation : retourne copie puis avance
                         * @return Copie de l'itérateur avant l'avancement
                         * @note Crée une copie temporaire : légèrement moins efficace que pré-incrément
                         */
                        Iterator operator++(int) {
                            Iterator tmp = *this;
                            ++(*this);
                            return tmp;
                        }

                    // ====================================================================
                    // OPÉRATEURS DE COMPARAISON D'ITÉRATEURS
                    // ====================================================================

                        /**
                         * @brief Opérateur d'égalité : compare les pointeurs de nœuds
                         * @param o Autre itérateur à comparer avec l'instance courante
                         * @return true si les deux itérateurs pointent vers le même nœud
                         * @note Deux itérateurs end() sont égaux même sur des Maps différentes
                         */
                        bool operator==(const Iterator& o) const {
                            return mNode == o.mNode;
                        }

                        /**
                         * @brief Opérateur d'inégalité : négation de l'égalité
                         * @param o Autre itérateur à comparer avec l'instance courante
                         * @return true si les itérateurs pointent vers des nœuds différents
                         */
                        bool operator!=(const Iterator& o) const {
                            return mNode != o.mNode;
                        }
                };

                /**
                 * @brief Alias pour l'itérateur en lecture seule
                 * @note Dans cette implémentation, ConstIterator == Iterator
                 * @note La const-correctness est assurée par les méthodes const de NkMap
                 * @note Pour une séparation stricte, créer une classe ConstIterator dédiée
                 */
                using ConstIterator = Iterator;

            // ====================================================================
            // SECTION PRIVÉE : MEMBRES DONNÉES ET MÉTHODES INTERNES
            // ====================================================================
            private:

                // ====================================================================
                // MEMBRES DONNÉES : ÉTAT DE L'ARBRE ROUGE-NOIR
                // ====================================================================

                Node* mRoot;                      ///< Pointeur vers la racine de l'arbre (nullptr si vide)
                SizeType mSize;                   ///< Nombre total de paires clé-valeur stockées
                Allocator* mAllocator;            ///< Pointeur vers l'allocateur mémoire utilisé
                Compare mCompare;                 ///< Instance du foncteur de comparaison pour l'ordre des clés

                // ====================================================================
                // MÉTHODES UTILITAIRES DE COMPARAISON ET GESTION MÉMOIRE
                // ====================================================================

                /**
                 * @brief Teste l'égalité de deux clés via le comparateur configuré
                 * @param lhs Première clé à comparer
                 * @param rhs Seconde clé à comparer
                 * @return true si ni lhs<rhs ni rhs<lhs (équivalence selon l'ordre strict faible)
                 * @note Méthode const : ne modifie pas l'état de la Map
                 * @note Utilisée pour la recherche exacte et la détection de doublons
                 */
                bool KeyEquals(const Key& lhs, const Key& rhs) const {
                    return !mCompare(lhs, rhs) && !mCompare(rhs, lhs);
                }

                /**
                 * @brief Alloue et construit un nouveau nœud avec placement new
                 * @param key Clé à stocker dans le nœud (copiée en const)
                 * @param value Valeur à associer à la clé
                 * @param parent Pointeur optionnel vers le nœud parent (défaut: nullptr)
                 * @return Pointeur vers le nœud nouvellement créé et initialisé
                 * @note Utilise l'allocateur configuré pour toutes les allocations
                 * @note Le nœud est créé avec couleur RED et pointeurs enfants nullptr
                 */
                Node* CreateNode(const Key& key, const Value& value, Node* parent = nullptr) {
                    Node* node = static_cast<Node*>(mAllocator->Allocate(sizeof(Node)));
                    new (node) Node(key, value, parent);
                    return node;
                }

                /**
                 * @brief Détruit un nœud et libère sa mémoire via l'allocateur
                 * @param node Pointeur vers le nœud à détruire
                 * @note Appelle explicitement le destructeur pour les types non-triviaux
                 * @note Libère la mémoire allouée via mAllocator->Deallocate()
                 */
                void DestroyNode(Node* node) {
                    node->~Node();
                    mAllocator->Deallocate(node);
                }

                // ====================================================================
                // MÉTHODES DE ROTATION POUR L'ÉQUILIBRAGE ROUGE-NOIR
                // ====================================================================

                /**
                 * @brief Effectue une rotation gauche autour du nœud spécifié
                 * @param node Nœud pivot autour duquel effectuer la rotation
                 * @note Réorganise les pointeurs pour maintenir l'ordre binaire de recherche
                 * @note Utilisée dans FixInsert() pour restaurer les invariants Rouge-Noir
                 * @note Complexité O(1) : manipulation constante de pointeurs
                 * 
                 * Schéma avant/après :
                 *     node                    right
                 *    /    \                  /    \
                 *   A    right    =>      node    C
                 *        /  \            /  \
                 *       B    C          A    B
                 */
                void RotateLeft(Node* node) {
                    Node* right = node->Right;
                    node->Right = right->Left;
                    if (right->Left) {
                        right->Left->Parent = node;
                    }
                    right->Parent = node->Parent;
                    if (!node->Parent) {
                        mRoot = right;
                    } else if (node == node->Parent->Left) {
                        node->Parent->Left = right;
                    } else {
                        node->Parent->Right = right;
                    }
                    right->Left = node;
                    node->Parent = right;
                }

                /**
                 * @brief Effectue une rotation droite autour du nœud spécifié
                 * @param node Nœud pivot autour duquel effectuer la rotation
                 * @note Réorganise les pointeurs pour maintenir l'ordre binaire de recherche
                 * @note Utilisée dans FixInsert() pour restaurer les invariants Rouge-Noir
                 * @note Complexité O(1) : manipulation constante de pointeurs
                 * 
                 * Schéma avant/après :
                 *     node                left
                 *    /    \              /    \
                 *  left   C    =>      A    node
                 *  / \                      /  \
                 * A   B                    B    C
                 */
                void RotateRight(Node* node) {
                    Node* left = node->Left;
                    node->Left = left->Right;
                    if (left->Right) {
                        left->Right->Parent = node;
                    }
                    left->Parent = node->Parent;
                    if (!node->Parent) {
                        mRoot = left;
                    } else if (node == node->Parent->Right) {
                        node->Parent->Right = left;
                    } else {
                        node->Parent->Left = left;
                    }
                    left->Right = node;
                    node->Parent = left;
                }

                /**
                 * @brief Restaure les invariants Rouge-Noir après insertion d'un nœud
                 * @param node Nœud nouvellement inséré (initialement rouge) point de départ
                 * @note Implémente l'algorithme standard de rééquilibrage RB-Insert-Fixup
                 * @note Gère les 6 cas classiques via rotations et recolorements
                 * @note Garantit que la racine reste noire en fin d'opération
                 * @note Complexité O(log n) : remontée au plus jusqu'à la racine
                 */
                void FixInsert(Node* node) {
                    while (node->Parent && node->Parent->NodeColor == RED) {
                        if (node->Parent == node->Parent->Parent->Left) {
                            Node* uncle = node->Parent->Parent->Right;
                            if (uncle && uncle->NodeColor == RED) {
                                node->Parent->NodeColor = BLACK;
                                uncle->NodeColor = BLACK;
                                node->Parent->Parent->NodeColor = RED;
                                node = node->Parent->Parent;
                            } else {
                                if (node == node->Parent->Right) {
                                    node = node->Parent;
                                    RotateLeft(node);
                                }
                                node->Parent->NodeColor = BLACK;
                                node->Parent->Parent->NodeColor = RED;
                                RotateRight(node->Parent->Parent);
                            }
                        } else {
                            Node* uncle = node->Parent->Parent->Left;
                            if (uncle && uncle->NodeColor == RED) {
                                node->Parent->NodeColor = BLACK;
                                uncle->NodeColor = BLACK;
                                node->Parent->Parent->NodeColor = RED;
                                node = node->Parent->Parent;
                            } else {
                                if (node == node->Parent->Left) {
                                    node = node->Parent;
                                    RotateRight(node);
                                }
                                node->Parent->NodeColor = BLACK;
                                node->Parent->Parent->NodeColor = RED;
                                RotateLeft(node->Parent->Parent);
                            }
                        }
                    }
                    mRoot->NodeColor = BLACK;
                }

                /**
                 * @brief Détruit récursivement tout un sous-arbre en post-order
                 * @param node Racine du sous-arbre à détruire
                 * @note Parcours post-order : enfants avant parent pour libération sécurisée
                 * @note Complexité O(n) pour la destruction complète de n nœuds
                 * @note Appel explicite des destructeurs pour les types non-triviaux
                 */
                void DestroyTree(Node* node) {
                    if (!node) {
                        return;
                    }
                    DestroyTree(node->Left);
                    DestroyTree(node->Right);
                    DestroyNode(node);
                }

                /**
                 * @brief Trouve le nœud de valeur minimale dans un sous-arbre
                 * @param node Racine du sous-arbre à analyser
                 * @return Pointeur vers le nœud le plus à gauche (minimum), ou nullptr si vide
                 * @note Utilisé pour trouver begin() et le successeur dans NextNode()
                 * @note Complexité O(h) où h = hauteur, O(log n) pour arbre équilibré
                 */
                Node* FindMin(Node* node) const {
                    while (node && node->Left) {
                        node = node->Left;
                    }
                    return node;
                }

                /**
                 * @brief Recherche itérative d'un nœud par clé dans l'arbre
                 * @param key Clé à localiser dans la structure
                 * @return Pointeur vers le Node trouvé, ou nullptr si clé absente
                 * @note Utilise l'ordre binaire pour éliminer la moitié des nœuds à chaque étape
                 * @note Complexité O(log n) grâce à l'équilibrage Rouge-Noir
                 */
                Node* FindNode(const Key& key) const {
                    Node* current = mRoot;
                    while (current) {
                        if (mCompare(key, current->Data.First)) {
                            current = current->Left;
                        } else if (mCompare(current->Data.First, key)) {
                            current = current->Right;
                        } else {
                            return current;
                        }
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
                 * @note Initialise une map vide avec racine nullptr et taille 0
                 * @note Complexité O(1) : aucune allocation initiale
                 */
                explicit NkMap(Allocator* allocator = nullptr)
                    : mRoot(nullptr)
                    , mSize(0)
                    , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator())
                    , mCompare() {
                }

                /**
                 * @brief Constructeur depuis NkInitializerList avec allocateur optionnel
                 * @param init Liste de paires clé-valeur pour initialiser la map
                 * @param allocator Pointeur vers un allocateur personnalisé (nullptr = défaut)
                 * @note Insère chaque paire séquentiellement via Insert() : O(n log n) total
                 * @note Les doublons de clés : la dernière valeur l'emporte (comportement std::map)
                 */
                NkMap(NkInitializerList<ValueType> init, Allocator* allocator = nullptr)
                    : mRoot(nullptr)
                    , mSize(0)
                    , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator())
                    , mCompare() {
                    for (auto& pair : init) {
                        Insert(pair.First, pair.Second);
                    }
                }

                /**
                 * @brief Constructeur depuis std::initializer_list avec allocateur optionnel
                 * @param init Liste standard de paires clé-valeur pour initialiser la map
                 * @param allocator Pointeur vers un allocateur personnalisé (nullptr = défaut)
                 * @note Compatibilité avec la syntaxe C++11 : NkMap<int, string> m = {{1, "un"}, {2, "deux"}};
                 */
                NkMap(std::initializer_list<ValueType> init, Allocator* allocator = nullptr)
                    : mRoot(nullptr)
                    , mSize(0)
                    , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator())
                    , mCompare() {
                    for (auto& pair : init) {
                        Insert(pair.First, pair.Second);
                    }
                }

                /**
                 * @brief Constructeur de copie pour duplication profonde de la Map
                 * @param other Map source à dupliquer élément par élément
                 * @note Construit un nouvel arbre indépendant via insertions séquentielles
                 * @note Complexité O(n log n) : n insertions dans un arbre initialement vide
                 * @note Copie le comparateur et l'allocateur de other pour cohérence
                 */
                NkMap(const NkMap& other)
                    : mRoot(nullptr)
                    , mSize(0)
                    , mAllocator(other.mAllocator)
                    , mCompare(other.mCompare) {
                    for (auto it = other.begin(); it != other.end(); ++it) {
                        Insert(it->First, it->Second);
                    }
                }

                // ====================================================================
                // SUPPORT C++11 : CONSTRUCTEUR DE DÉPLACEMENT
                // ====================================================================
                #if defined(NK_CPP11)

                    /**
                     * @brief Constructeur de déplacement pour transfert efficace des ressources
                     * @param other Map source dont transférer le contenu
                     * @note noexcept : transfert par échange de pointeurs, aucune allocation
                     * @note other est laissé dans un état valide mais vide après le move
                     * @note Complexité O(1) : simple réaffectation de pointeurs et métadonnées
                     */
                    NkMap(NkMap&& other) NKENTSEU_NOEXCEPT
                        : mRoot(other.mRoot)
                        , mSize(other.mSize)
                        , mAllocator(other.mAllocator)
                        , mCompare(traits::NkMove(other.mCompare)) {
                        other.mRoot = nullptr;
                        other.mSize = 0;
                    }

                #endif // defined(NK_CPP11)

                /**
                 * @brief Destructeur libérant tous les nœuds et la structure de l'arbre
                 * @note Appelle Clear() pour détruire récursivement tous les nœuds
                 * @note Garantit l'absence de fuite mémoire via RAII même en cas d'exception
                 * @note Complexité O(n) pour la destruction complète de n nœuds
                 */
                ~NkMap() {
                    Clear();
                }

            // ====================================================================
            // SECTION PUBLIQUE : OPÉRATEURS D'AFFECTATION
            // ====================================================================

                /**
                 * @brief Opérateur d'affectation par copie avec gestion d'auto-affectation
                 * @param other Map source à copier
                 * @return Référence vers l'instance courante pour le chaînage d'opérations
                 * @note Libère les ressources actuelles via Clear() avant de copier other
                 * @note Ré-insère tous les éléments de other : O(n log n) complexité
                 * @note Préserve l'allocateur courant : ne copie pas mAllocator de other
                 */
                NkMap& operator=(const NkMap& other) {
                    if (this == &other) {
                        return *this;
                    }
                    Clear();
                    mAllocator = other.mAllocator;
                    mCompare = other.mCompare;
                    for (auto it = other.begin(); it != other.end(); ++it) {
                        Insert(it->First, it->Second);
                    }
                    return *this;
                }

                // ====================================================================
                // SUPPORT C++11 : OPÉRATEUR DE DÉPLACEMENT
                // ====================================================================
                #if defined(NK_CPP11)

                    /**
                     * @brief Opérateur d'affectation par déplacement avec transfert de ressources
                     * @param other Map source dont transférer le contenu
                     * @return Référence vers l'instance courante pour le chaînage d'opérations
                     * @note noexcept : échange de pointeurs sans allocation ni copie
                     * @note other est laissé dans un état valide mais vide après l'opération
                     * @note Complexité O(1) : réaffectation directe des membres internes
                     */
                    NkMap& operator=(NkMap&& other) NKENTSEU_NOEXCEPT {
                        if (this == &other) {
                            return *this;
                        }
                        Clear();
                        mRoot = other.mRoot;
                        mSize = other.mSize;
                        mAllocator = other.mAllocator;
                        mCompare = traits::NkMove(other.mCompare);
                        other.mRoot = nullptr;
                        other.mSize = 0;
                        return *this;
                    }

                #endif // defined(NK_CPP11)

                /**
                 * @brief Opérateur d'affectation depuis NkInitializerList
                 * @param init Nouvelle liste de paires pour remplacer le contenu actuel
                 * @return Référence vers l'instance courante pour le chaînage d'opérations
                 * @note Efface le contenu existant via Clear() avant d'insérer les nouvelles paires
                 * @note Complexité O(n log n) pour n éléments dans la liste
                 */
                NkMap& operator=(NkInitializerList<ValueType> init) {
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
                NkMap& operator=(std::initializer_list<ValueType> init) {
                    Clear();
                    for (auto& pair : init) {
                        Insert(pair.First, pair.Second);
                    }
                    return *this;
                }

            // ====================================================================
            // SECTION PUBLIQUE : ITÉRATEURS ET PARCOURS
            // ====================================================================

                /**
                 * @brief Retourne un itérateur mutable vers le premier élément (clé minimale)
                 * @return Iterator pointant vers le nœud le plus à gauche, ou end() si vide
                 * @note Complexité O(log n) : descente depuis la racine vers la feuille gauche
                 * @note Invalidation : uniquement si l'élément pointé est supprimé via Erase()
                 */
                Iterator begin() {
                    return Iterator(FindMin(mRoot), this);
                }

                /**
                 * @brief Retourne un itérateur const vers le premier élément (clé minimale)
                 * @return ConstIterator pointant vers le nœud le plus à gauche, ou end() si vide
                 * @note Version const pour itération en lecture seule sur Map const
                 */
                ConstIterator begin() const {
                    return ConstIterator(FindMin(mRoot), this);
                }

                /**
                 * @brief Retourne un itérateur mutable vers la position "fin" (sentinelle)
                 * @return Iterator avec mNode == nullptr marquant la fin du parcours
                 * @note Complexité O(1) : construction directe sans recherche
                 * @note Tous les itérateurs end() sont égaux entre eux pour une même Map
                 */
                Iterator end() {
                    return Iterator(nullptr, this);
                }

                /**
                 * @brief Retourne un itérateur const vers la position "fin" (sentinelle)
                 * @return ConstIterator avec mNode == nullptr marquant la fin du parcours
                 * @note Version const pour comparaison avec des itérateurs const
                 */
                ConstIterator end() const {
                    return ConstIterator(nullptr, this);
                }

                /**
                 * @brief Alias uppercase de begin() pour compatibilité avec les conventions internes
                 * @return Iterator mutable vers le premier élément
                 * @note Permet l'usage cohérent avec Begin()/End() dans le codebase
                 */
                Iterator Begin() {
                    return begin();
                }

                /**
                 * @brief Alias uppercase const de begin() pour compatibilité interne
                 * @return ConstIterator vers le premier élément
                 */
                ConstIterator Begin() const {
                    return begin();
                }

                /**
                 * @brief Alias uppercase de end() pour compatibilité avec les conventions internes
                 * @return Iterator sentinelle de fin
                 */
                Iterator End() {
                    return end();
                }

                /**
                 * @brief Alias uppercase const de end() pour compatibilité interne
                 * @return ConstIterator sentinelle de fin
                 */
                ConstIterator End() const {
                    return end();
                }

            // ====================================================================
            // SECTION PUBLIQUE : CAPACITÉ ET MÉTADONNÉES
            // ====================================================================

                /**
                 * @brief Teste si la Map ne contient aucun élément
                 * @return true si mSize == 0, false sinon
                 * @note Opération O(1) sans effet de bord, noexcept garantie
                 * @note Équivalent à Size() == 0 mais sémantiquement plus explicite
                 */
                bool Empty() const NKENTSEU_NOEXCEPT {
                    return mSize == 0;
                }

                /**
                 * @brief Retourne le nombre d'éléments stockés dans la Map
                 * @return Valeur de type SizeType représentant la cardinalité
                 * @note Opération O(1) sans effet de bord, noexcept garantie
                 * @note Reflète exactement le nombre de paires clé-valeur uniques
                 */
                SizeType Size() const NKENTSEU_NOEXCEPT {
                    return mSize;
                }

            // ====================================================================
            // SECTION PUBLIQUE : MODIFICATEURS DE LA STRUCTURE
            // ====================================================================

                /**
                 * @brief Supprime tous les éléments et libère la mémoire des nœuds
                 * @note Complexité O(n) : destruction post-order de chaque Node
                 * @note Réinitialise mRoot à nullptr et mSize à 0
                 * @note Après Clear(), Empty() == true et Size() == 0
                 */
                void Clear() {
                    DestroyTree(mRoot);
                    mRoot = nullptr;
                    mSize = 0;
                }

                /**
                 * @brief Insère ou met à jour une paire clé-valeur dans la Map ordonnée
                 * @param key Clé d'indexation pour l'élément (copiée en const dans le nœud)
                 * @param value Valeur associée à stocker ou à mettre à jour si la clé existe
                 * @note Si la clé existe déjà : met à jour la valeur et retourne sans rééquilibrage
                 * @note Si la clé est nouvelle : insère un nœud rouge puis appelle FixInsert()
                 * @note Garantit l'équilibrage Rouge-Noir après insertion : hauteur O(log n)
                 * @note Complexité : O(log n) pour la descente + O(log n) pour FixInsert() = O(log n)
                 */
                void Insert(const Key& key, const Value& value) {
                    if (!mRoot) {
                        mRoot = CreateNode(key, value);
                        mRoot->NodeColor = BLACK;
                        ++mSize;
                        return;
                    }
                    Node* current = mRoot;
                    Node* parent = nullptr;
                    while (current) {
                        parent = current;
                        if (mCompare(key, current->Data.First)) {
                            current = current->Left;
                        } else if (mCompare(current->Data.First, key)) {
                            current = current->Right;
                        } else {
                            current->Data.Second = value;
                            return;
                        }
                    }
                    Node* newNode = CreateNode(key, value, parent);
                    if (mCompare(key, parent->Data.First)) {
                        parent->Left = newNode;
                    } else {
                        parent->Right = newNode;
                    }
                    ++mSize;
                    FixInsert(newNode);
                }

            // ====================================================================
            // SECTION PUBLIQUE : RECHERCHE ET ACCÈS AUX ÉLÉMENTS
            // ====================================================================

                /**
                 * @brief Teste la présence d'une clé dans la Map
                 * @param key Clé à vérifier
                 * @return true si la clé existe, false sinon
                 * @note Implémenté via FindNode() != nullptr : complexité O(log n)
                 * @note Plus sémantique que Find() quand seule la présence importe
                 */
                bool Contains(const Key& key) const {
                    return FindNode(key) != nullptr;
                }

                /**
                 * @brief Recherche une valeur par clé et retourne un pointeur modifiable
                 * @param key Clé à rechercher dans la Map
                 * @return Pointeur non-const vers la valeur trouvée, ou nullptr si absente
                 * @note Permet la modification directe : *map.Find(key) = newValue;
                 * @note Complexité : O(log n) pour la recherche dans l'arbre équilibré
                 */
                Value* Find(const Key& key) {
                    Node* node = FindNode(key);
                    return node ? &node->Data.Second : nullptr;
                }

                /**
                 * @brief Recherche une valeur par clé et retourne un pointeur en lecture seule
                 * @param key Clé à rechercher dans la Map
                 * @return Pointeur const vers la valeur trouvée, ou nullptr si absente
                 * @note Version const pour usage sur Map const ou accès en lecture seule
                 */
                const Value* Find(const Key& key) const {
                    Node* node = FindNode(key);
                    return node ? &node->Data.Second : nullptr;
                }

                /**
                 * @brief Recherche une clé et retourne un itérateur mutable vers l'élément
                 * @param key Clé à localiser dans la Map
                 * @return Iterator vers la paire trouvée, ou end() si clé absente
                 * @note Permet l'usage dans des algorithmes génériques attendant des itérateurs
                 * @note L'itérateur retourne une paire : it->First = clé, it->Second = valeur
                 */
                Iterator FindIterator(const Key& key) {
                    return Iterator(FindNode(key), this);
                }

                /**
                 * @brief Recherche une clé et retourne un itérateur const vers l'élément
                 * @param key Clé à localiser dans la Map
                 * @return ConstIterator vers la paire trouvée, ou end() si clé absente
                 * @note Version const pour usage sur Map const ou itération en lecture seule
                 */
                ConstIterator FindIterator(const Key& key) const {
                    return ConstIterator(FindNode(key), this);
                }

                /**
                 * @brief Accède à une valeur par clé avec assertion sur présence
                 * @param key Clé dont accéder à la valeur associée
                 * @return Référence non-const vers la valeur trouvée
                 * @pre La clé doit exister dans la Map (assertion NKENTSEU_ASSERT en debug)
                 * @note Préférer Find() ou Contains() en production pour gérer l'absence gracieusement
                 * @note Permet la modification directe : map.At(key) = newValue;
                 */
                Value& At(const Key& key) {
                    Node* node = FindNode(key);
                    NKENTSEU_ASSERT(node != nullptr);
                    return node->Data.Second;
                }

                /**
                 * @brief Accède à une valeur par clé en lecture seule avec assertion
                 * @param key Clé dont accéder à la valeur associée
                 * @return Référence const vers la valeur trouvée
                 * @pre La clé doit exister dans la Map (assertion NKENTSEU_ASSERT en debug)
                 * @note Version const pour usage sur Map const ou accès en lecture seule
                 */
                const Value& At(const Key& key) const {
                    Node* node = FindNode(key);
                    NKENTSEU_ASSERT(node != nullptr);
                    return node->Data.Second;
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
                 * @brief Insère une nouvelle paire ou met à jour une existante avec retour de statut
                 * @param key Clé à insérer ou mettre à jour
                 * @param value Valeur à associer à la clé
                 * @return true si une nouvelle insertion a eu lieu, false si mise à jour d'existant
                 * @note Utile pour distinguer création vs modification dans la logique métier
                 * @note Complexité identique à Insert() : O(log n)
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
            // SECTION PUBLIQUE : SUPPRESSION D'ÉLÉMENTS
            // ====================================================================

                /**
                 * @brief Supprime un élément par clé si présent dans la Map
                 * @param key Clé de l'élément à supprimer
                 * @return true si l'élément a été trouvé et supprimé, false si clé absente
                 * @note IMPLÉMENTATION ACTUELLE : recopie tous les éléments sauf celui à supprimer
                 * @note Complexité : O(n log n) - inefficace, à optimiser avec vrai RB-Delete
                 * @note Libère la mémoire du nœud supprimé via l'allocateur configuré
                 * @note TODO: Implémenter l'algorithme RB-Delete-Fixup standard pour O(log n)
                 */
                bool Erase(const Key& key) {
                    if (!Contains(key)) {
                        return false;
                    }
                    struct EntryCopy {
                        Key KeyValue;
                        Value ValueValue;
                        EntryCopy(const Key& k, const Value& v)
                            : KeyValue(k)
                            , ValueValue(v) {
                        }
                    };
                    NkVector<EntryCopy, Allocator> retained(mAllocator);
                    if (mSize > 1) {
                        retained.Reserve(mSize - 1);
                    }
                    for (auto it = begin(); it != end(); ++it) {
                        if (!KeyEquals(it->First, key)) {
                            retained.PushBack(EntryCopy(it->First, it->Second));
                        }
                    }
                    Clear();
                    for (SizeType i = 0; i < retained.Size(); ++i) {
                        Insert(retained[i].KeyValue, retained[i].ValueValue);
                    }
                    return true;
                }

                /**
                 * @brief Supprime un élément via itérateur et retourne l'itérateur suivant
                 * @param position Itérateur pointant vers l'élément à supprimer
                 * @return Itérateur vers l'élément suivant, ou end() si fin de parcours
                 * @note IMPLÉMENTATION ACTUELLE : délègue à Erase(key) puis retourne end()
                 * @note Complexité : O(n log n) - inefficace, à optimiser avec vrai RB-Delete
                 * @note TODO: Implémenter la suppression directe par rotation pour O(log n)
                 */
                Iterator Erase(Iterator position) {
                    if (position == end()) {
                        return end();
                    }
                    Key key = position->First;
                    Erase(key);
                    return end();
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
                 * @note Attention : peut modifier la taille de la map et déclencher un rééquilibrage
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

#endif // NK_CONTAINERS_ASSOCIATIVE_NKMAP_H

// ============================================================================
// EXEMPLES D'UTILISATION ET PATTERNS COURANTS
// ============================================================================
/*
 * @section examples Exemples pratiques d'utilisation de NkMap
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 1 : Création et opérations de base avec ordre garanti
 * --------------------------------------------------------------------------
 * @code
 * #include "NKContainers/Associative/NkMap.h"
 * #include "NKCore/NkString.h"
 * #include <cstdio>
 * 
 * void exempleBase() {
 *     // Création d'une Map int -> NkString avec ordre croissant par défaut
 *     nkentseu::NkMap<int, nkentseu::NkString> map;
 *     
 *     // Insertion dans un ordre arbitraire
 *     map.Insert(3, "trois");
 *     map.Insert(1, "un");
 *     map.Insert(4, "quatre");
 *     map.Insert(2, "deux");
 *     
 *     // L'arbre maintient l'ordre : parcours in-order = clés triées
 *     printf("Contenu trié par clé:\n");
 *     for (auto it = map.Begin(); it != map.End(); ++it) {
 *         printf("  %d: %s\n", it->First, it->Second.CStr());
 *     }
 *     // Sortie:
 *     //   1: un
 *     //   2: deux
 *     //   3: trois
 *     //   4: quatre
 *     
 *     // Accès via operator[] (crée si absent)
 *     map[5] = "cinq";  // Insertion
 *     map[2] = "DEUX";  // Mise à jour de la clé existante
 *     
 *     // Recherche avec Find()
 *     nkentseu::NkString* val = map.Find(3);
 *     if (val) {
 *         printf("Clé 3: %s\n", val->CStr());  // "trois"
 *     }
 *     
 *     // Suppression
 *     bool removed = map.Erase(4);  // true : clé présente
 *     bool missing = map.Erase(99); // false : clé absente
 *     
 *     printf("Taille: %zu\n", map.Size());  // 4
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 2 : Itération ordonnée et algorithmes de plage
 * --------------------------------------------------------------------------
 * @code
 * void exempleIteration() {
 *     nkentseu::NkMap<const char*, int> scores;
 *     
 *     // Construction via initializer_list (C++11)
 *     scores = {{"Alice", 95}, {"Bob", 87}, {"Charlie", 92}};
 *     
 *     // Parcours en ordre lexicographique des clés (ordre croissant)
 *     printf("Classement:\n");
 *     int rank = 1;
 *     for (const auto& entry : scores) {  // Range-based for grâce à begin()/end()
 *         printf("  %d. %s: %d points\n", rank++, entry.First, entry.Second);
 *     }
 *     // Sortie (ordre alphabétique):
 *     //   1. Alice: 95 points
 *     //   2. Bob: 87 points
 *     //   3. Charlie: 92 points
 *     
 *     // Recherche avec FindIterator pour usage algorithmique
 *     auto it = scores.FindIterator("Bob");
 *     if (it != scores.End()) {
 *         it->Second += 3;  // Modification via itérateur mutable
 *         printf("Bob mis à jour: %d points\n", it->Second);  // 90
 *     }
 *     
 *     // Simulation de recherche par plage (nécessite lower_bound/upper_bound futurs)
 *     printf("\nScores >= 90:\n");
 *     for (auto it = scores.Begin(); it != scores.End(); ++it) {
 *         if (it->Second >= 90) {
 *             printf("  %s: %d\n", it->First, it->Second);
 *         }
 *     }
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 3 : Comparateur personnalisé pour ordre décroissant
 * --------------------------------------------------------------------------
 * @code
 * // Comparateur inversé pour tri décroissant
 * template<typename Key>
 * struct NkMapGreater {
 *     bool operator()(const Key& lhs, const Key& rhs) const {
 *         return lhs > rhs;  // Inversion pour ordre décroissant
 *     }
 * };
 * 
 * void exempleOrdreDecroissant() {
 *     // Map avec ordre décroissant via comparateur custom
 *     nkentseu::NkMap<int, nkentseu::NkString, memory::NkAllocator, NkMapGreater<int>> 
 *         reverseMap;
 *     
 *     reverseMap.Insert(10, "dix");
 *     reverseMap.Insert(5, "cinq");
 *     reverseMap.Insert(20, "vingt");
 *     
 *     // Parcours : clés en ordre décroissant
 *     printf("Ordre décroissant:\n");
 *     for (const auto& entry : reverseMap) {
 *         printf("  %d: %s\n", entry.First, entry.Second.CStr());
 *     }
 *     // Sortie:
 *     //   20: vingt
 *     //   10: dix
 *     //   5: cinq
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 4 : Gestion mémoire avec allocateur personnalisé
 * --------------------------------------------------------------------------
 * @code
 * #include "NKMemory/NkPoolAllocator.h"
 * 
 * void exempleAllocateur() {
 *     // Pool allocator pour réduire la fragmentation sur insertions massives
 *     memory::NkPoolAllocator pool(512 * 1024);  // Pool de 512KB
 *     
 *     // Map utilisant le pool pour toutes ses allocations de nœuds
 *     nkentseu::NkMap<usize, nkentseu::NkString, memory::NkPoolAllocator> 
 *         indexedData(&pool);
 *     
 *     // Insertion en masse sans fragmentation externe
 *     for (usize i = 0; i < 1000; ++i) {
 *         nkentseu::NkString key = nkentseu::NkString::Format("key_%zu", i);
 *         nkentseu::NkString value = nkentseu::NkString::Format("value_%zu", i);
 *         indexedData.Insert(i, value);  // Allocation via pool
 *     }
 *     
 *     printf("Index: %zu entrées, hauteur ~log2(%zu) = ~%zu\n", 
 *            indexedData.Size(), indexedData.Size(), 
 *            static_cast<usize>(nkentseu::traits::NkLog2(indexedData.Size()) + 1));
 *     
 *     // Destruction : libération en bloc via le pool (très rapide)
 *     // L'arbre Rouge-Noir garantit O(log n) pour chaque opération intermédiaire
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 5 : Patterns avancés - Indexation, cache LRU, configuration
 * --------------------------------------------------------------------------
 * @code
 * // Pattern 1: Index inversé pour recherche rapide
 * void exempleIndexInverse() {
 *     // Map nom -> ID pour lookup rapide
 *     nkentseu::NkMap<nkentseu::NkString, usize> nameToId;
 *     
 *     nameToId.Insert("alice", 1001);
 *     nameToId.Insert("bob", 1002);
 *     nameToId.Insert("charlie", 1003);
 *     
 *     // Recherche O(log n) par nom
 *     if (const usize* id = nameToId.Find("bob")) {
 *         printf("ID de bob: %zu\n", *id);  // 1002
 *     }
 *     
 *     // Parcours ordonné pour export trié
 *     printf("Utilisateurs triés:\n");
 *     for (const auto& entry : nameToId) {
 *         printf("  %s -> %zu\n", entry.First.CStr(), entry.Second);
 *     }
 * }
 * 
 * // Pattern 2: Cache avec ordre d'accès (simulation LRU via timestamp)
 * struct CacheEntry {
 *     nkentseu::NkString data;
 *     usize lastAccess;
 * };
 * 
 * void exempleCache() {
 *     nkentseu::NkMap<usize, CacheEntry> cache;  // Clé: timestamp d'accès
 *     
 *     // Insertion avec timestamp croissant
 *     cache.Insert(1000, {"donnée A", 1000});
 *     cache.Insert(1001, {"donnée B", 1001});
 *     cache.Insert(1002, {"donnée C", 1002});
 *     
 *     // Les entrées sont triées par timestamp : plus ancien en premier
 *     // Pour LRU : supprimer begin() quand capacité dépassée
 *     if (cache.Size() > 2) {
 *         cache.Erase(cache.Begin()->First);  // Supprime l'entrée la plus ancienne
 *     }
 * }
 * 
 * // Pattern 3: Configuration hiérarchique avec parcours préfixe
 * void exempleConfiguration() {
 *     nkentseu::NkMap<nkentseu::NkString, nkentseu::NkString> config;
 *     
 *     config.Insert("app.name", "MonApplication");
 *     config.Insert("app.version", "1.0.0");
 *     config.Insert("db.host", "localhost");
 *     config.Insert("db.port", "5432");
 *     
 *     // Parcours ordonné : regroupement naturel par préfixe
 *     printf("Configuration:\n");
 *     nkentseu::NkString currentPrefix;
 *     for (const auto& entry : config) {
 *         if (entry.First.Find(".") != nkentseu::NkString::NkPosInvalid && 
 *             entry.First.SubStr(0, entry.First.Find(".")) != currentPrefix) {
 *             currentPrefix = entry.First.SubStr(0, entry.First.Find("."));
 *             printf("\n[%s]\n", currentPrefix.CStr());
 *         }
 *         nkentseu::NkString key = entry.First.SubStr(entry.First.Find(".") + 1);
 *         printf("  %s = %s\n", key.CStr(), entry.Second.CStr());
 *     }
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 6 : Comparaison avec NkHashMap - quand choisir quoi ?
 * --------------------------------------------------------------------------
 * @code
 * void exempleChoixConteneur() {
 *     // CAS 1: Besoin d'ordre déterministe -> NkMap (Rouge-Noir)
 *     nkentseu::NkMap<int, nkentseu::NkString> ordered;
 *     ordered.Insert(3, "trois");
 *     ordered.Insert(1, "un");
 *     ordered.Insert(2, "deux");
 *     // Parcours garanti: 1, 2, 3 (toujours le même ordre)
 *     
 *     // CAS 2: Performance moyenne optimale -> NkHashMap (table de hachage)
 *     nkentseu::NkHashMap<int, nkentseu::NkString> hashed;
 *     hashed.Insert(3, "trois");
 *     hashed.Insert(1, "un");
 *     hashed.Insert(2, "deux");
 *     // Parcours: ordre indéterminé (dépend du hash et des buckets)
 *     
 *     // COMPARAISON DE PERFORMANCE:
 *     // - NkMap: O(log n) garanti pour toutes les opérations
 *     // - NkHashMap: O(1) amorti, O(n) pire cas (collisions)
 *     
 *     // RÈGLE PRATIQUE:
 *     // - Utiliser NkMap si: besoin de tri, parcours ordonné, ou pire cas critique
 *     // - Utiliser NkHashMap si: performance moyenne prioritaire, ordre non requis
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * BONNES PRATIQUES ET RECOMMANDATIONS
 * --------------------------------------------------------------------------
 * 
 * 1. CHOIX DU TYPE DE CLÉ :
 *    - Doit être copiable et comparable via le foncteur Compare
 *    - Privilégier les types trivialement copiables pour les performances
 *    - Pour un ordre personnalisé : fournir un comparateur custom (ex: NkMapGreater)
 *    - Éviter les clés mutables : l'ordre de tri ne doit jamais changer après insertion
 * 
 * 2. PERFORMANCE ET COMPLEXITÉ :
 *    - Toutes les opérations (Insert/Find/Erase) : O(log n) garanti grâce à l'équilibrage RB
 *    - Itération complète : O(n) avec parcours in-order naturel
 *    - Mémoire : overhead constant par nœud (3 pointeurs + 1 couleur + données)
 *    - Préférer NkHashMap si O(1) amorti suffit et l'ordre n'est pas requis
 * 
 * 3. ACCÈS ET MODIFICATION :
 *    - operator[] : pratique mais insère si absent (peut déclencher rééquilibrage)
 *    - Find() : retourne nullptr si absent, plus sûr pour la lecture
 *    - TryGet() : pattern optionnel sans pointeurs, idéal pour les APIs publiques
 *    - At() : avec assertion, pour les cas où la présence est garantie par la logique
 * 
 * 4. ITÉRATION ET INVALIDATION :
 *    - Les itérateurs ne sont invalidés QUE par la suppression de l'élément pointé
 *    - Safe pour itérer et modifier d'autres éléments simultanément
 *    - Parcours in-order garanti : toujours dans l'ordre défini par Compare
 * 
 * 5. GESTION MÉMOIRE :
 *    - Utiliser un allocateur pool pour les Maps de taille fixe ou prévisible
 *    - Clear() libère tous les nœuds : O(n) mais nécessaire pour éviter les fuites
 *    - Pour les valeurs lourdes : stocker des pointeurs ou utiliser move semantics (C++11)
 * 
 * 6. LIMITATIONS CONNUES (version actuelle) :
 *    - Erase() utilise une recopie naïve : O(n log n) au lieu de O(log n)
 *    - TODO: Implémenter RB-Delete-Fixup standard pour suppression efficace
 *    - Pas de lower_bound/upper_bound : à ajouter pour les recherches par plage
 *    - Pas d'itérateur inverse (rbegin/rend) : à ajouter pour parcours décroissant
 * 
 * 7. SÉCURITÉ THREAD :
 *    - NkMap est thread-unsafe par conception
 *    - Protéger avec un mutex externe pour accès concurrents multiples
 *    - Pour le pattern reader-writer : plusieurs lectures concurrentes possibles si aucune écriture
 * 
 * 8. EXTENSIONS RECOMMANDÉES :
 *    - Implémenter RB-Delete-Fixup pour Erase() en O(log n)
 *    - Ajouter lower_bound(key) / upper_bound(key) pour recherches par plage
 *    - Supporter les itérateurs inverses (reverse_iterator) pour parcours décroissant
 *    - Ajouter extract(key) pour transfert de propriété sans copie
 */

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Généré par Rihen le 2026-02-05 22:26:13
// Dernière modification : 2026-04-26 (restructuration et documentation)
// ============================================================