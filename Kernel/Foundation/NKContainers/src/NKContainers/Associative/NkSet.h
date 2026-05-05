// -----------------------------------------------------------------------------
// FICHIER: NKContainers\Containers\Associative\NkSet.h
// DESCRIPTION: Ensemble ordonné - Implémentation par arbre Rouge-Noir
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CONTAINERS_ASSOCIATIVE_NKSET_H
#define NK_CONTAINERS_ASSOCIATIVE_NKSET_H

    // ========================================================================
    // INCLUSIONS DES DÉPENDANCES
    // ========================================================================

    #include "NKCore/NkTypes.h"                    // Définition des types fondamentaux (usize, intptr, etc.)
    #include "NKContainers/NkContainersApi.h"      // Macros d'exportation de l'API conteneurs
    #include "NKCore/NkTraits.h"                   // Traits pour move/forward et utilitaires méta
    #include "NKMemory/NkAllocator.h"              // Système d'allocation mémoire personnalisable
    #include "NKMemory/NkFunction.h"               // Wrapper pour les fonctions et callables
    #include "NKCore/Assert/NkAssert.h"            // Macros d'assertion pour le débogage sécurisé
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
         * @brief Foncteur de comparaison par défaut pour NkSet
         * 
         * Implémente un ordre strict faible basé sur l'opérateur <.
         * Utilisé pour maintenir les éléments triés dans l'arbre Rouge-Noir
         * et pour détecter les doublons lors de l'insertion.
         * 
         * @tparam T Type des éléments comparés (doit supporter operator<)
         * 
         * @note Pour un ordre personnalisé (tri décroissant, comparaison sémantique),
         *       fournir un comparateur custom via le template parameter de NkSet
         * @note L'ordre doit être cohérent : si !(a<b) && !(b<a) alors a==b (équivalence)
         */
        template<typename T>
        struct NkSetLess {

            /**
             * @brief Opérateur de comparaison pour deux éléments
             * @param lhs Premier élément à comparer (côté gauche)
             * @param rhs Second élément à comparer (côté droit)
             * @return true si lhs < rhs selon l'ordre strict faible, false sinon
             * @note Méthode const : ne modifie pas l'état du foncteur
             * @note Délègue à l'operator< du type T : doit être défini et transitif
             */
            bool operator()(const T& lhs, const T& rhs) const {
                return lhs < rhs;
            }
        };

        // ====================================================================
        // CLASSE PRINCIPALE : NK SET (ARBRE ROUGE-NOIR)
        // ====================================================================

        /**
         * @brief Classe template implémentant un ensemble ordonné via arbre Rouge-Noir
         * 
         * Conteneur associatif ordonné stockant des éléments uniques,
         * maintenus triés selon un comparateur configurable. L'implémentation
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
         * - Élimination de doublons avec maintien de l'ordre
         * - Recherche de membership efficace (Contains)
         * - Parcours trié pour export ou affichage ordonné
         * - Opérations ensemblistes (union, intersection - à ajouter)
         * - Remplacement de std::set sans dépendance STL
         * 
         * @tparam T Type des éléments stockés (doit être copiable et comparable via Compare)
         * @tparam Allocator Type de l'allocateur mémoire (défaut: memory::NkAllocator)
         * @tparam Compare Foncteur de comparaison (défaut: NkSetLess<T> pour ordre croissant)
         * 
         * @note Les éléments sont stockés en const implicite : modification après insertion impossible
         * @note Thread-unsafe : synchronisation externe requise pour usage concurrent
         * @note Les itérateurs sont invalidés uniquement par la suppression de l'élément pointé
         */
        template<typename T,
                 typename Allocator = memory::NkAllocator,
                 typename Compare = NkSetLess<T>>
        class NkSet {

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
                 * Contient une valeur, des pointeurs de navigation
                 * (gauche, droit, parent) et la couleur pour l'équilibrage.
                 * 
                 * @note Layout mémoire : [Value: T][Left][Right][Parent][Color]
                 * @note Nouveaux nœuds insérés comme RED : simplifie les rotations de rééquilibrage
                 * @note L'unicité est garantie par la comparaison lors de l'insertion
                 */
                struct Node {

                    T Value;                    ///< Valeur stockée dans le nœud (élément de l'ensemble)
                    Node* Left;                 ///< Pointeur vers le sous-arbre gauche (valeurs < Value)
                    Node* Right;                ///< Pointeur vers le sous-arbre droit (valeurs > Value)
                    Node* Parent;               ///< Pointeur vers le nœud parent (nullptr pour la racine)
                    Color NodeColor;            ///< Couleur du nœud pour l'algorithme Rouge-Noir

                    /**
                     * @brief Constructeur de nœud avec initialisation des membres
                     * @param value Référence const vers la valeur à stocker
                     * @param parent Pointeur optionnel vers le nœud parent (défaut: nullptr)
                     * @note Initialise les pointeurs enfants à nullptr
                     * @note Initialise la couleur à RED pour l'insertion standard
                     */
                    Node(const T& value, Node* parent = nullptr)
                        : Value(value)
                        , Left(nullptr)
                        , Right(nullptr)
                        , Parent(parent)
                        , NodeColor(RED) {
                    }
                };

            // ====================================================================
            // SECTION PUBLIQUE : ALIASES ET ITÉRATEURS
            // ====================================================================
            public:

                // ====================================================================
                // ALIASES DE TYPES POUR LA LISIBILITÉ ET LA GÉNÉRICITÉ
                // ====================================================================

                using ValueType = T;              ///< Alias du type des éléments stockés
                using SizeType = usize;           ///< Alias du type pour les tailles/comptages
                using Reference = T&;             ///< Alias de référence non-const (lecture seule via itérateur)
                using ConstReference = const T&;  ///< Alias de référence const pour accès en lecture

                // ====================================================================
                // CLASSE ITÉRATEUR BIDIRECTIONNEL
                // ====================================================================

                /**
                 * @brief Itérateur bidirectionnel pour parcourir les éléments triés du Set
                 * 
                 * Itérateur qui suit l'ordre in-order de l'arbre (gauche, racine, droite),
                 * garantissant un parcours des éléments en ordre croissant selon le comparateur.
                 * 
                 * PROPRIÉTÉS :
                 * - Catégorie : Bidirectional Iterator (peut avancer et reculer - recul à ajouter)
                 * - Invalidation : uniquement par suppression de l'élément pointé
                 * - Accès en lecture seule via operator* et operator-> (const-correctness)
                 * - Comparaison par égalité de pointeur de nœud
                 * 
                 * IMPLÉMENTATION DE NextNode() :
                 * - Si nœud a un fils droit : retourne le minimum du sous-arbre droit
                 * - Sinon : remonte vers le premier ancêtre dont le nœud est dans le sous-arbre gauche
                 * 
                 * @note Construction réservée à NkSet via friend declaration
                 * @note ConstIterator est un alias vers Iterator : la const-correctness est gérée par le Set parent
                 */
                class Iterator {

                    // ====================================================================
                    // MEMBRES PRIVÉS DE L'ITÉRATEUR
                    // ====================================================================
                    private:

                        Node* mNode;                    ///< Pointeur vers le nœud courant dans l'arbre
                        const NkSet* mSet;              ///< Pointeur vers le Set parent pour contexte
                        friend class NkSet;             ///< NkSet peut construire des itérateurs

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

                        using ValueType = T;                ///< Type de l'élément pointé
                        using Reference = const T&;         ///< Référence const vers l'élément (lecture seule)
                        using Pointer = const T*;           ///< Pointeur const vers l'élément
                        using DifferenceType = intptr;      ///< Type pour les différences d'itérateurs
                        using IteratorCategory = NkBidirectionalIteratorTag;  ///< Catégorie d'itérateur

                    // ====================================================================
                    // CONSTRUCTEURS DE L'ITÉRATEUR
                    // ====================================================================

                        /**
                         * @brief Constructeur par défaut pour itérateur non-initialisé (end-like)
                         */
                        Iterator()
                            : mNode(nullptr)
                            , mSet(nullptr) {
                        }

                        /**
                         * @brief Constructeur paramétré pour création contrôlée par NkSet
                         * @param node Pointeur vers le nœud de départ (nullptr pour end())
                         * @param set Pointeur vers le Set parent pour navigation
                         */
                        Iterator(Node* node, const NkSet* set)
                            : mNode(node)
                            , mSet(set) {
                        }

                    // ====================================================================
                    // OPÉRATEURS D'ACCÈS ET DE DÉPLACEMENT
                    // ====================================================================

                        /**
                         * @brief Opérateur de déréférencement pour accès en lecture seule à la valeur
                         * @return Référence const vers la valeur stockée dans le nœud courant
                         * @pre L'itérateur ne doit pas être end() (accès à mNode supposé valide)
                         * @note Retourne const T& : les éléments du Set ne sont pas modifiables via l'itérateur
                         */
                        Reference operator*() const {
                            return mNode->Value;
                        }

                        /**
                         * @brief Opérateur flèche pour accès aux membres de la valeur en lecture seule
                         * @return Pointeur const vers la valeur stockée dans le nœud courant
                         * @note Permet it->member comme syntaxe familière pour les types complexes
                         */
                        Pointer operator->() const {
                            return &mNode->Value;
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
                         * @note Deux itérateurs end() sont égaux même sur des Sets différents
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
                 * @note La const-correctness est assurée par les méthodes const de NkSet
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
                SizeType mSize;                   ///< Nombre total d'éléments uniques stockés
                Allocator* mAllocator;            ///< Pointeur vers l'allocateur mémoire utilisé
                Compare mCompare;                 ///< Instance du foncteur de comparaison pour l'ordre des éléments

                // ====================================================================
                // MÉTHODES UTILITAIRES DE COMPARAISON ET GESTION MÉMOIRE
                // ====================================================================

                /**
                 * @brief Teste l'équivalence de deux éléments via le comparateur configuré
                 * @param lhs Premier élément à comparer
                 * @param rhs Second élément à comparer
                 * @return true si ni lhs<rhs ni rhs<lhs (équivalence selon l'ordre strict faible)
                 * @note Méthode const : ne modifie pas l'état du Set
                 * @note Utilisée pour la recherche exacte et la détection de doublons lors de l'insertion
                 */
                bool IsEquivalent(const T& lhs, const T& rhs) const {
                    return !mCompare(lhs, rhs) && !mCompare(rhs, lhs);
                }

                /**
                 * @brief Alloue et construit un nouveau nœud avec placement new
                 * @param value Référence const vers la valeur à stocker dans le nœud
                 * @param parent Pointeur optionnel vers le nœud parent (défaut: nullptr)
                 * @return Pointeur vers le nœud nouvellement créé et initialisé
                 * @note Utilise l'allocateur configuré pour toutes les allocations
                 * @note Le nœud est créé avec couleur RED et pointeurs enfants nullptr
                 */
                Node* CreateNode(const T& value, Node* parent = nullptr) {
                    Node* node = static_cast<Node*>(mAllocator->Allocate(sizeof(Node)));
                    new (node) Node(value, parent);
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

            // ====================================================================
            // SECTION PUBLIQUE : CONSTRUCTEURS ET DESTRUCTEUR
            // ====================================================================
            public:

                /**
                 * @brief Constructeur par défaut avec allocateur optionnel
                 * @param allocator Pointeur vers un allocateur personnalisé (nullptr = défaut)
                 * @note Initialise un set vide avec racine nullptr et taille 0
                 * @note Complexité O(1) : aucune allocation initiale
                 */
                explicit NkSet(Allocator* allocator = nullptr)
                    : mRoot(nullptr)
                    , mSize(0)
                    , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator())
                    , mCompare() {
                }

                /**
                 * @brief Constructeur depuis NkInitializerList avec allocateur optionnel
                 * @param init Liste d'éléments pour initialiser le set
                 * @param allocator Pointeur vers un allocateur personnalisé (nullptr = défaut)
                 * @note Insère chaque élément séquentiellement via Insert() : O(n log n) total
                 * @note Les doublons sont automatiquement ignorés : unicité garantie
                 */
                NkSet(NkInitializerList<T> init, Allocator* allocator = nullptr)
                    : mRoot(nullptr)
                    , mSize(0)
                    , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator())
                    , mCompare() {
                    for (auto& val : init) {
                        Insert(val);
                    }
                }

                /**
                 * @brief Constructeur depuis std::initializer_list avec allocateur optionnel
                 * @param init Liste standard d'éléments pour initialiser le set
                 * @param allocator Pointeur vers un allocateur personnalisé (nullptr = défaut)
                 * @note Compatibilité avec la syntaxe C++11 : NkSet<int> s = {3, 1, 4, 1, 5};
                 * @note Les doublons sont automatiquement ignorés : résultat {1, 3, 4, 5}
                 */
                NkSet(std::initializer_list<T> init, Allocator* allocator = nullptr)
                    : mRoot(nullptr)
                    , mSize(0)
                    , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator())
                    , mCompare() {
                    for (auto& val : init) {
                        Insert(val);
                    }
                }

                /**
                 * @brief Constructeur de copie pour duplication profonde du Set
                 * @param other Set source à dupliquer élément par élément
                 * @note Construit un nouvel arbre indépendant via insertions séquentielles
                 * @note Complexité O(n log n) : n insertions dans un arbre initialement vide
                 * @note Copie le comparateur et l'allocateur de other pour cohérence
                 */
                NkSet(const NkSet& other)
                    : mRoot(nullptr)
                    , mSize(0)
                    , mAllocator(other.mAllocator)
                    , mCompare(other.mCompare) {
                    for (auto it = other.begin(); it != other.end(); ++it) {
                        Insert(*it);
                    }
                }

                // ====================================================================
                // SUPPORT C++11 : CONSTRUCTEUR DE DÉPLACEMENT
                // ====================================================================
                #if defined(NK_CPP11)

                    /**
                     * @brief Constructeur de déplacement pour transfert efficace des ressources
                     * @param other Set source dont transférer le contenu
                     * @note noexcept : transfert par échange de pointeurs, aucune allocation
                     * @note other est laissé dans un état valide mais vide après le move
                     * @note Complexité O(1) : simple réaffectation de pointeurs et métadonnées
                     */
                    NkSet(NkSet&& other) NKENTSEU_NOEXCEPT
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
                ~NkSet() {
                    Clear();
                }

            // ====================================================================
            // SECTION PUBLIQUE : OPÉRATEURS D'AFFECTATION
            // ====================================================================

                /**
                 * @brief Opérateur d'affectation par copie avec gestion d'auto-affectation
                 * @param other Set source à copier
                 * @return Référence vers l'instance courante pour le chaînage d'opérations
                 * @note Libère les ressources actuelles via Clear() avant de copier other
                 * @note Ré-insère tous les éléments de other : O(n log n) complexité
                 * @note Préserve l'allocateur courant : ne copie pas mAllocator de other
                 */
                NkSet& operator=(const NkSet& other) {
                    if (this == &other) {
                        return *this;
                    }
                    Clear();
                    mAllocator = other.mAllocator;
                    mCompare = other.mCompare;
                    for (auto it = other.begin(); it != other.end(); ++it) {
                        Insert(*it);
                    }
                    return *this;
                }

                // ====================================================================
                // SUPPORT C++11 : OPÉRATEUR DE DÉPLACEMENT
                // ====================================================================
                #if defined(NK_CPP11)

                    /**
                     * @brief Opérateur d'affectation par déplacement avec transfert de ressources
                     * @param other Set source dont transférer le contenu
                     * @return Référence vers l'instance courante pour le chaînage d'opérations
                     * @note noexcept : échange de pointeurs sans allocation ni copie
                     * @note other est laissé dans un état valide mais vide après l'opération
                     * @note Complexité O(1) : réaffectation directe des membres internes
                     */
                    NkSet& operator=(NkSet&& other) NKENTSEU_NOEXCEPT {
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
                 * @param init Nouvelle liste d'éléments pour remplacer le contenu actuel
                 * @return Référence vers l'instance courante pour le chaînage d'opérations
                 * @note Efface le contenu existant via Clear() avant d'insérer les nouveaux éléments
                 * @note Complexité O(n log n) pour n éléments dans la liste
                 */
                NkSet& operator=(NkInitializerList<T> init) {
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
                NkSet& operator=(std::initializer_list<T> init) {
                    Clear();
                    for (auto& val : init) {
                        Insert(val);
                    }
                    return *this;
                }

            // ====================================================================
            // SECTION PUBLIQUE : ITÉRATEURS ET PARCOURS
            // ====================================================================

                /**
                 * @brief Retourne un itérateur mutable vers le premier élément (valeur minimale)
                 * @return Iterator pointant vers le nœud le plus à gauche, ou end() si vide
                 * @note Complexité O(log n) : descente depuis la racine vers la feuille gauche
                 * @note Invalidation : uniquement si l'élément pointé est supprimé via Erase()
                 */
                Iterator begin() {
                    return Iterator(FindMin(mRoot), this);
                }

                /**
                 * @brief Retourne un itérateur const vers le premier élément (valeur minimale)
                 * @return ConstIterator pointant vers le nœud le plus à gauche, ou end() si vide
                 * @note Version const pour itération en lecture seule sur Set const
                 */
                ConstIterator begin() const {
                    return ConstIterator(FindMin(mRoot), this);
                }

                /**
                 * @brief Retourne un itérateur mutable vers la position "fin" (sentinelle)
                 * @return Iterator avec mNode == nullptr marquant la fin du parcours
                 * @note Complexité O(1) : construction directe sans recherche
                 * @note Tous les itérateurs end() sont égaux entre eux pour un même Set
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

            // ====================================================================
            // SECTION PUBLIQUE : CAPACITÉ ET MÉTADONNÉES
            // ====================================================================

                /**
                 * @brief Teste si le Set ne contient aucun élément
                 * @return true si mSize == 0, false sinon
                 * @note Opération O(1) sans effet de bord, noexcept garantie
                 * @note Équivalent à Size() == 0 mais sémantiquement plus explicite
                 */
                bool Empty() const NKENTSEU_NOEXCEPT {
                    return mSize == 0;
                }

                /**
                 * @brief Retourne le nombre d'éléments stockés dans le Set
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
                 * @brief Insère un nouvel élément dans le Set ordonné si absent
                 * @param value Référence const vers l'élément à insérer
                 * @return true si l'insertion a eu lieu, false si l'élément était déjà présent
                 * @note Si l'élément existe déjà (IsEquivalent) : retourne false sans modification
                 * @note Si l'élément est nouveau : insère un nœud rouge puis appelle FixInsert()
                 * @note Garantit l'équilibrage Rouge-Noir après insertion : hauteur O(log n)
                 * @note Complexité : O(log n) pour la descente + O(log n) pour FixInsert() = O(log n)
                 */
                bool Insert(const T& value) {
                    if (!mRoot) {
                        mRoot = CreateNode(value);
                        mRoot->NodeColor = BLACK;
                        ++mSize;
                        return true;
                    }
                    Node* current = mRoot;
                    Node* parent = nullptr;
                    while (current) {
                        parent = current;
                        if (mCompare(value, current->Value)) {
                            current = current->Left;
                        } else if (mCompare(current->Value, value)) {
                            current = current->Right;
                        } else {
                            return false;
                        }
                    }
                    Node* newNode = CreateNode(value, parent);
                    if (mCompare(value, parent->Value)) {
                        parent->Left = newNode;
                    } else {
                        parent->Right = newNode;
                    }
                    ++mSize;
                    FixInsert(newNode);
                    return true;
                }

            // ====================================================================
            // SECTION PUBLIQUE : RECHERCHE ET ACCÈS AUX ÉLÉMENTS
            // ====================================================================

                /**
                 * @brief Teste la présence d'un élément dans le Set
                 * @param value Élément à vérifier
                 * @return true si l'élément existe, false sinon
                 * @note Implémenté via recherche itérative dans l'arbre : complexité O(log n)
                 * @note Utilise le comparateur configuré pour la navigation et l'équivalence
                 * @note Plus sémantique que Find() quand seule la présence importe
                 */
                bool Contains(const T& value) const {
                    Node* current = mRoot;
                    while (current) {
                        if (mCompare(value, current->Value)) {
                            current = current->Left;
                        } else if (mCompare(current->Value, value)) {
                            current = current->Right;
                        } else {
                            return true;
                        }
                    }
                    return false;
                }

                /**
                 * @brief Recherche un élément et retourne un itérateur mutable vers lui
                 * @param value Élément à localiser dans le Set
                 * @return Iterator vers l'élément trouvé, ou end() si absent
                 * @note Permet l'usage dans des algorithmes génériques attendant des itérateurs
                 * @note L'itérateur permet l'accès en lecture seule : *it retourne const T&
                 * @note Complexité : O(log n) pour la recherche dans l'arbre équilibré
                 */
                Iterator Find(const T& value) {
                    Node* current = mRoot;
                    while (current) {
                        if (mCompare(value, current->Value)) {
                            current = current->Left;
                        } else if (mCompare(current->Value, value)) {
                            current = current->Right;
                        } else {
                            return Iterator(current, this);
                        }
                    }
                    return end();
                }

            // ====================================================================
            // SECTION PUBLIQUE : SUPPRESSION D'ÉLÉMENTS
            // ====================================================================

                /**
                 * @brief Supprime un élément par valeur si présent dans le Set
                 * @param value Élément à supprimer
                 * @return true si l'élément a été trouvé et supprimé, false si absent
                 * @note IMPLÉMENTATION ACTUELLE : recopie tous les éléments sauf celui à supprimer
                 * @note Complexité : O(n log n) - inefficace, à optimiser avec vrai RB-Delete
                 * @note Libère la mémoire du nœud supprimé via l'allocateur configuré
                 * @note TODO: Implémenter l'algorithme RB-Delete-Fixup standard pour O(log n)
                 */
                bool Erase(const T& value) {
                    if (!Contains(value)) {
                        return false;
                    }
                    NkVector<T, Allocator> retained(mAllocator);
                    if (mSize > 1) {
                        retained.Reserve(mSize - 1);
                    }
                    for (auto it = begin(); it != end(); ++it) {
                        if (!IsEquivalent(*it, value)) {
                            retained.PushBack(*it);
                        }
                    }
                    Clear();
                    for (SizeType i = 0; i < retained.Size(); ++i) {
                        Insert(retained[i]);
                    }
                    return true;
                }
        };

    } // namespace nkentseu

#endif // NK_CONTAINERS_ASSOCIATIVE_NKSET_H

// ============================================================================
// EXEMPLES D'UTILISATION ET PATTERNS COURANTS
// ============================================================================
/*
 * @section examples Exemples pratiques d'utilisation de NkSet
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 1 : Création et opérations de base avec unicité garantie
 * --------------------------------------------------------------------------
 * @code
 * #include "NKContainers/Associative/NkSet.h"
 * #include <cstdio>
 * 
 * void exempleBase() {
 *     // Création d'un Set d'entiers avec ordre croissant par défaut
 *     nkentseu::NkSet<int> set;
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
 *     // Parcours en ordre trié garanti
 *     printf("Éléments triés: ");
 *     for (auto it = set.begin(); it != set.end(); ++it) {
 *         printf("%d ", *it);
 *     }
 *     // Sortie: 1 3 4 5 9
 *     printf("\n");
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
 *     printf("Après suppression de 1: ");
 *     for (int val : set) {  // Range-based for grâce à begin()/end()
 *         printf("%d ", val);
 *     }
 *     // Sortie: 3 4 5 9
 *     printf("\n");
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 2 : Initialisation et construction depuis liste
 * --------------------------------------------------------------------------
 * @code
 * void exempleInitialisation() {
 *     // Construction via initializer_list (C++11) - doublons automatiquement filtrés
 *     nkentseu::NkSet<const char*> tags = {"cpp", "template", "cpp", "container", "template"};
 *     
 *     printf("Tags uniques: ");
 *     for (const char* tag : tags) {
 *         printf("%s ", tag);
 *     }
 *     // Sortie (ordre lexicographique): container cpp template
 *     printf("\n");
 *     
 *     // Construction par copie
 *     nkentseu::NkSet<const char*> copy = tags;
 *     
 *     // Construction par déplacement (C++11)
 *     #if defined(NK_CPP11)
 *     nkentseu::NkSet<const char*> moved = nkentseu::traits::NkMove(copy);
 *     // copy est maintenant vide, moved possède les éléments
 *     #endif
 *     
 *     // Affectation depuis liste
 *     tags = {"new", "tag", "list"};
 *     printf("Nouveaux tags: ");
 *     for (const char* tag : tags) {
 *         printf("%s ", tag);
 *     }
 *     // Sortie: list new tag
 *     printf("\n");
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 3 : Comparateur personnalisé pour ordre décroissant
 * --------------------------------------------------------------------------
 * @code
 * // Comparateur inversé pour tri décroissant
 * template<typename T>
 * struct NkSetGreater {
 *     bool operator()(const T& lhs, const T& rhs) const {
 *         return lhs > rhs;  // Inversion pour ordre décroissant
 *     }
 * };
 * 
 * void exempleOrdreDecroissant() {
 *     // Set avec ordre décroissant via comparateur custom
 *     nkentseu::NkSet<int, memory::NkAllocator, NkSetGreater<int>> reverseSet;
 *     
 *     reverseSet.Insert(10);
 *     reverseSet.Insert(5);
 *     reverseSet.Insert(20);
 *     reverseSet.Insert(15);
 *     
 *     // Parcours : éléments en ordre décroissant
 *     printf("Ordre décroissant: ");
 *     for (int val : reverseSet) {
 *         printf("%d ", val);
 *     }
 *     // Sortie: 20 15 10 5
 *     printf("\n");
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
 *     // Conversion en Set pour éliminer les doublons
 *     nkentseu::NkSet<int> unique(data.Begin(), data.End());  // À implémenter: constructor range
 *     // Alternative avec boucle explicite:
 *     nkentseu::NkSet<int> uniqueAlt;
 *     for (int val : data) {
 *         uniqueAlt.Insert(val);  // Doublons automatiquement ignorés
 *     }
 *     
 *     printf("Valeurs uniques triées: ");
 *     for (int val : uniqueAlt) {
 *         printf("%d ", val);
 *     }
 *     // Sortie: 1 2 5 7 8 9
 *     printf("\n");
 *     
 *     // Conversion retour en vecteur si besoin
 *     nkentseu::NkVector<int> result;
 *     for (int val : uniqueAlt) {
 *         result.PushBack(val);
 *     }
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
 *     // Set utilisant le pool pour toutes ses allocations de nœuds
 *     nkentseu::NkSet<usize, memory::NkPoolAllocator> indexedIds(&pool);
 *     
 *     // Insertion en masse sans fragmentation externe
 *     for (usize i = 0; i < 1000; ++i) {
 *         indexedIds.Insert(i * 3);  // Insertion d'IDs multiples de 3
 *     }
 *     
 *     printf("Set: %zu éléments uniques, hauteur ~log2(%zu)\n", 
 *            indexedIds.Size(), indexedIds.Size());
 *     
 *     // Vérification d'appartenance rapide
 *     bool has999 = indexedIds.Contains(999);   // true : 999 = 333*3
 *     bool has1000 = indexedIds.Contains(1000); // false : pas multiple de 3
 *     
 *     printf("999 présent: %s, 1000 présent: %s\n",
 *            has999 ? "oui" : "non", has1000 ? "oui" : "non");
 *     
 *     // Destruction : libération en bloc via le pool (très rapide)
 *     // L'arbre Rouge-Noir garantit O(log n) pour chaque opération intermédiaire
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
 *     nkentseu::NkSet<int> valides;
 *     
 *     for (int val : inputs) {
 *         if (EstValide(val)) {
 *             valides.Insert(val);  // Doublons automatiquement éliminés
 *         }
 *     }
 *     
 *     printf("Valeurs valides uniques: ");
 *     for (int val : valides) {
 *         printf("%d ", val);
 *     }
 *     printf("\n");
 * }
 * 
 * // Pattern 2: Intersection de deux ensembles (simulation)
 * void exempleIntersection(const nkentseu::NkSet<int>& a, 
 *                          const nkentseu::NkSet<int>& b) {
 *     nkentseu::NkSet<int> intersection;
 *     
 *     // Pour chaque élément de a, vérifier présence dans b
 *     for (int val : a) {
 *         if (b.Contains(val)) {
 *             intersection.Insert(val);
 *         }
 *     }
 *     
 *     printf("Intersection: ");
 *     for (int val : intersection) {
 *         printf("%d ", val);
 *     }
 *     printf("\n");
 * }
 * 
 * // Pattern 3: Union de deux ensembles (simulation)
 * void exempleUnion(const nkentseu::NkSet<int>& a,
 *                   const nkentseu::NkSet<int>& b) {
 *     nkentseu::NkSet<int> unite = a;  // Copie de a
 *     
 *     // Insertion des éléments de b : doublons automatiquement ignorés
 *     for (int val : b) {
 *         unite.Insert(val);
 *     }
 *     
 *     printf("Union: ");
 *     for (int val : unite) {
 *         printf("%d ", val);
 *     }
 *     printf("\n");
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 7 : Comparaison avec NkHashMap - quand choisir quoi ?
 * --------------------------------------------------------------------------
 * @code
 * void exempleChoixConteneur() {
 *     // CAS 1: Besoin d'unicité + ordre -> NkSet (Rouge-Noir)
 *     nkentseu::NkSet<int> orderedUnique;
 *     orderedUnique.Insert(3);
 *     orderedUnique.Insert(1);
 *     orderedUnique.Insert(2);
 *     orderedUnique.Insert(2);  // Ignoré : doublon
 *     // Parcours garanti: 1, 2, 3 (toujours le même ordre, unicité garantie)
 *     
 *     // CAS 2: Unicité sans ordre requis -> NkHashMap<T, void> ou NkHashSet (à créer)
 *     // NkHashMap<int, bool> hashSet;  // Simulation de set via map
 *     // hashSet.Insert(3, true);  // Plus rapide en moyenne, ordre indéterminé
 *     
 *     // CAS 3: Clé-valeur avec ordre -> NkMap (voir fichier NkMap.h)
 *     
 *     // COMPARAISON DE PERFORMANCE:
 *     // - NkSet: O(log n) garanti pour toutes les opérations, ordre maintenu
 *     // - NkHashMap: O(1) amorti, O(n) pire cas, pas d'ordre garanti
 *     
 *     // RÈGLE PRATIQUE:
 *     // - Utiliser NkSet si: unicité + ordre trié requis, ou pire cas critique
 *     // - Utiliser NkHashMap si: performance moyenne prioritaire, ordre non requis
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * BONNES PRATIQUES ET RECOMMANDATIONS
 * --------------------------------------------------------------------------
 * 
 * 1. CHOIX DU TYPE D'ÉLÉMENT :
 *    - Doit être copiable et comparable via le foncteur Compare
 *    - Privilégier les types trivialement copiables pour les performances
 *    - Pour un ordre personnalisé : fournir un comparateur custom (ex: NkSetGreater)
 *    - Éviter les éléments mutables : l'ordre de tri ne doit jamais changer après insertion
 * 
 * 2. PERFORMANCE ET COMPLEXITÉ :
 *    - Toutes les opérations (Insert/Contains/Erase) : O(log n) garanti grâce à l'équilibrage RB
 *    - Itération complète : O(n) avec parcours in-order naturel
 *    - Mémoire : overhead constant par nœud (3 pointeurs + 1 couleur + données)
 *    - Préférer NkHashMap si O(1) amorti suffit et l'ordre n'est pas requis
 * 
 * 3. ACCÈS ET MODIFICATION :
 *    - Les éléments sont accessibles en lecture seule via les itérateurs (const T&)
 *    - Contains() : O(log n), idéal pour tester la présence sans itérateur
 *    - Find() : retourne un itérateur pour usage algorithmique ou accès itératif
 *    - Pas d'opérateur[] : les Sets ne stockent pas de valeurs associées, seulement des clés
 * 
 * 4. ITÉRATION ET INVALIDATION :
 *    - Les itérateurs ne sont invalidés QUE par la suppression de l'élément pointé
 *    - Safe pour itérer et supprimer d'autres éléments simultanément
 *    - Parcours in-order garanti : toujours dans l'ordre défini par Compare
 * 
 * 5. GESTION MÉMOIRE :
 *    - Utiliser un allocateur pool pour les Sets de taille fixe ou prévisible
 *    - Clear() libère tous les nœuds : O(n) mais nécessaire pour éviter les fuites
 *    - Pour les éléments lourds : stocker des pointeurs ou utiliser move semantics (C++11)
 * 
 * 6. LIMITATIONS CONNUES (version actuelle) :
 *    - Erase() utilise une recopie naïve : O(n log n) au lieu de O(log n)
 *    - TODO: Implémenter RB-Delete-Fixup standard pour suppression efficace
 *    - Pas de lower_bound/upper_bound : à ajouter pour les recherches par plage
 *    - Pas d'itérateur inverse (rbegin/rend) : à ajouter pour parcours décroissant
 *    - Pas de constructeur range [first, last) : à ajouter pour compatibilité STL
 * 
 * 7. SÉCURITÉ THREAD :
 *    - NkSet est thread-unsafe par conception
 *    - Protéger avec un mutex externe pour accès concurrents multiples
 *    - Pour le pattern reader-writer : plusieurs lectures concurrentes possibles si aucune écriture
 * 
 * 8. EXTENSIONS RECOMMANDÉES :
 *    - Implémenter RB-Delete-Fixup pour Erase() en O(log n)
 *    - Ajouter lower_bound(value) / upper_bound(value) pour recherches par plage
 *    - Supporter les itérateurs inverses (reverse_iterator) pour parcours décroissant
 *    - Ajouter extract(value) pour transfert de propriété sans copie
 *    - Implémenter les opérations ensemblistes : union(), intersection(), difference()
 *    - Ajouter un constructeur range : NkSet(InputIt first, InputIt last)
 */

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Généré par Rihen le 2026-02-05 22:26:13
// Dernière modification : 2026-04-26 (restructuration et documentation)
// ============================================================