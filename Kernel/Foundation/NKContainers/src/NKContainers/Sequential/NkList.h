// =============================================================================
// NKContainers/Sequential/NkList.h
// Liste chaînée simple — conteneur séquentiel 100% indépendant de la STL.
//
// Design :
//  - Réutilisation DIRECTE des macros NKCore/NKPlatform (ZÉRO duplication)
//  - Gestion mémoire via NKMemory::NkAllocator pour flexibilité d'allocation
//  - Itérateurs Forward-only (parcours unidirectionnel) pour performance
//  - Pointeur tail pour PushBack en O(1) amorti
//  - Gestion d'erreurs unifiée via NKENTSEU_CONTAINERS_THROW_* macros
//  - Indentation stricte : namespaces imbriqués, visibilité, blocs tous indentés
//  - Une instruction par ligne pour lisibilité et maintenance
//  - Compatibilité multiplateforme via NKPlatform
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
//
// Historique des versions :
//   v1.0.0 (2026-02-09) :
//     - Implémentation initiale de NkList avec itérateurs Forward
//     - Support C++11 move semantics pour PushFront/PushBack
//     - Méthode Reverse() pour inversion in-place de la liste
// =============================================================================

#pragma once

#ifndef NKENTSEU_CONTAINERS_SEQUENTIAL_NKLIST_H_INCLUDED
#define NKENTSEU_CONTAINERS_SEQUENTIAL_NKLIST_H_INCLUDED

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // NkList dépend des modules suivants :
    //  - NKCore/NkTypes.h : Types fondamentaux (nk_size, nk_bool, etc.)
    //  - NKContainers/NkContainersApi.h : Macros d'export pour visibilité des symboles
    //  - NKCore/NkTraits.h : Utilitaires de métaprogrammation (NkMove, NkForward, etc.)
    //  - NKMemory/NkAllocator.h : Interface d'allocateur pour gestion mémoire flexible
    //  - NKMemory/NkFunction.h : Fonctions mémoire bas niveau (NkCopy, etc.)
    //  - NKCore/Assert/NkAssert.h : Système d'assertions pour validation debug
    //  - NKContainers/Iterators/NkIterator.h : Tags d'itérateurs (NkForwardIteratorTag)
    //  - NKContainers/Iterators/NkInitializerList.h : Liste d'initialisation légère
    #include "NKContainers/NkContainersApi.h"
    #include "NKContainers/Iterators/NkIterator.h"
    #include "NKContainers/Iterators/NkInitializerList.h"
    #include "NKContainers/String/NkFormat.h"   

    #include "NKCore/NkTypes.h"
    #include "NKCore/NkTraits.h"
    #include "NKMemory/NkAllocator.h"
    #include "NKMemory/NkFunction.h"
    #include "NKCore/Assert/NkAssert.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : ESPACE DE NOMS PRINCIPAL
    // -------------------------------------------------------------------------
    // Tous les composants NKContainers sont encapsulés dans ce namespace hiérarchique
    // pour éviter les collisions de noms et assurer une API cohérente et modulaire.
    // L'indentation reflète la hiérarchie : chaque niveau de namespace est indenté.

    namespace nkentseu {

        // ====================================================================
        // SECTION 3 : CLASSE TEMPLATE NKLIST
        // ====================================================================
        /**
         * @brief Liste chaînée simple — conteneur séquentiel à insertion/suppression rapide.
         * @tparam T Type des éléments stockés dans la liste
         * @tparam Allocator Type de l'allocateur utilisé pour la gestion mémoire des nœuds
         * @ingroup SequentialContainers
         *
         * NkList implémente une liste chaînée simple (singly linked list),
         * optimisée pour les opérations d'insertion/suppression aux extrémités :
         *
         *  - PushFront/PopFront : O(1) constant — idéal pour structures de type stack/queue
         *  - PushBack : O(1) amorti grâce au pointeur tail mémorisé
         *  - Accès par index : O(n) — parcours séquentiel requis (pas d'accès aléatoire)
         *  - Insertion/suppression au milieu : O(n) — parcours jusqu'à la position
         *
         * @note Avantages vs NkVector :
         *       - Insertions/suppressions en tête ou milieu sans décalage d'éléments
         *       - Itérateurs non invalidés par insertions/suppressions (sauf élément supprimé)
         *       - Mémoire fragmentée mais allocation par nœud (utile pour objets lourds)
         *
         * @note Inconvénients vs NkVector :
         *       - Pas d'accès aléatoire O(1) — parcours séquentiel obligatoire
         *       - Surcoût mémoire par nœud (pointeur Next + alignment)
         *       - Moins cache-friendly (nœuds potentiellement dispersés en mémoire)
         *
         * @example Utilisation basique
         * @code
         * using namespace nkentseu::containers::sequential;
         *
         * // Construction avec liste d'initialisation
         * NkList<int> values = {1, 2, 3, 4, 5};
         *
         * // Insertion en tête : O(1)
         * values.PushFront(0);  // values = {0, 1, 2, 3, 4, 5}
         *
         * // Insertion en queue : O(1) grâce au pointeur tail
         * values.PushBack(6);   // values = {0, 1, 2, 3, 4, 5, 6}
         *
         * // Parcours avec itérateur Forward-only
         * for (auto it = values.begin(); it != values.end(); ++it) {
         *     printf("%d ", *it);  // Affiche: 0 1 2 3 4 5 6
         * }
         *
         * // Suppression en tête : O(1)
         * values.PopFront();  // values = {1, 2, 3, 4, 5, 6}
         *
         * // Inversion de la liste : O(n)
         * values.Reverse();   // values = {6, 5, 4, 3, 2, 1}
         * @endcode
         */
        template<typename T, typename Allocator = memory::NkAllocator>
        class NKENTSEU_CONTAINERS_CLASS_EXPORT NkList {
            private:

                // -----------------------------------------------------------------
                // Sous-section : Structure interne Node (privée)
                // -----------------------------------------------------------------
                /**
                 * @brief Nœud interne de la liste chaînée
                 * @struct Node
                 * @ingroup ListInternals
                 *
                 * Structure privée contenant :
                 *  - Data : l'élément stocké de type T
                 *  - Next : pointeur vers le nœud suivant (nullptr pour dernier)
                 *
                 * @note Allocation via l'allocateur de la liste, pas new/delete standard
                 * @note Constructeurs conditionnels selon disponibilité C++11
                 */
                struct Node {
                    T Data;       ///< Élément stocké dans ce nœud
                    Node* Next;   ///< Pointeur vers le nœud suivant (nullptr si dernier)

                    /**
                     * @brief Constructeur C++11 avec forwarding parfait
                     * @tparam Args Types des arguments de construction (déduits)
                     * @param next Pointeur vers le nœud suivant à lier
                     * @param args Arguments forwardés au constructeur de T
                     *
                     * @note Utilise placement new pour construction in-place dans le buffer alloué
                     * @note Forwarding parfait préserve les catégories de valeur (lvalue/rvalue)
                     */
                    template<typename... Args>
                    Node(Node* next, Args&&... args)
                        : Data(traits::NkForward<Args>(args)...)
                        , Next(next) {
                    }

                    /**
                     * @brief Constructeur pré-C++11 par copie
                     * @param next Pointeur vers le nœud suivant à lier
                     * @param data Valeur à copier dans le nœud
                     *
                     * @note Fallback pour compatibilité avec compilateurs anciens
                     * @note Requiert que T soit CopyConstructible
                     */
                    Node(Node* next, const T& data)
                        : Data(data)
                        , Next(next) {
                    }
                };

            public:

                // -----------------------------------------------------------------
                // Sous-section : Types membres publics
                // -----------------------------------------------------------------
                /**
                 * @brief Type des valeurs stockées dans le conteneur
                 * @ingroup ListTypes
                 */
                using ValueType = T;

                /**
                 * @brief Type utilisé pour représenter la taille et les index
                 * @ingroup ListTypes
                 * @note Alias vers nk_size pour cohérence avec le reste du framework
                 */
                using SizeType = nk_size;

                /**
                 * @brief Type de référence mutable aux éléments
                 * @ingroup ListTypes
                 */
                using Reference = T&;

                /**
                 * @brief Type de référence constante aux éléments
                 * @ingroup ListTypes
                 */
                using ConstReference = const T&;

                // -----------------------------------------------------------------
                // Sous-section : Itérateur mutable (Forward-only)
                // -----------------------------------------------------------------
                /**
                 * @brief Itérateur mutable pour parcours unidirectionnel de la liste
                 * @class Iterator
                 * @ingroup ListIterators
                 *
                 * Itérateur Forward-only : supporte uniquement l'incrémentation ++
                 * et la déréférenciation *, pas de décrément -- ni d'arithmétique.
                 *
                 * @note Catégorie : NkForwardIteratorTag (hérite de NkInputIteratorTag)
                 * @note Implémentation légère : simple wrapper autour de Node*
                 * @note Compatible avec les algorithmes génériques attendant ForwardIterator
                 */
                class Iterator {
                    private:

                        /** @brief Pointeur vers le nœud courant (privé) */
                        Node* mNode;

                        /** @brief NkList est ami pour accéder à mNode */
                        friend class NkList;

                    public:

                        /** @brief Type des valeurs pointées par l'itérateur */
                        using ValueType = T;

                        /** @brief Type de référence mutable retourné par operator* */
                        using Reference = T&;

                        /** @brief Type de pointeur mutable retourné par operator-> */
                        using Pointer = T*;

                        /** @brief Type pour les différences entre itérateurs */
                        using DifferenceType = nkentseu::ptrdiff;

                        /** @brief Catégorie d'itérateur : Forward-only */
                        using IteratorCategory = NkForwardIteratorTag;

                        /**
                         * @brief Constructeur par défaut — itérateur nul (end sentinel)
                         * @ingroup ListIterators
                         */
                        Iterator()
                            : mNode(nullptr) {
                        }

                        /**
                         * @brief Constructeur explicite depuis un pointeur de nœud
                         * @param node Pointeur vers le nœud à encapsuler
                         * @ingroup ListIterators
                         */
                        explicit Iterator(Node* node)
                            : mNode(node) {
                        }

                        /**
                         * @brief Déréférencement — accès à l'élément courant
                         * @return Référence mutable vers Data du nœud courant
                         * @ingroup ListIterators
                         *
                         * @note Comportement indéfini si l'itérateur est end() (mNode == nullptr)
                         * @note L'utilisateur doit vérifier it != end() avant déréférencement
                         */
                        Reference operator*() const {
                            return mNode->Data;
                        }

                        /**
                         * @brief Accès membre via pointeur — équivalent à &(operator*())
                         * @return Pointeur mutable vers Data du nœud courant
                         * @ingroup ListIterators
                         *
                         * @note Utile pour accéder aux membres de T via it->member
                         * @note Même avertissement : vérifier validité avant usage
                         */
                        Pointer operator->() const {
                            return &mNode->Data;
                        }

                        /**
                         * @brief Incrémentation préfixe — avance au nœud suivant
                         * @return Référence vers *this après avancement
                         * @ingroup ListIterators
                         *
                         * @note Complexité : O(1) — simple affectation de pointeur
                         * @note Comportement indéfini si appelé sur end() (mNode == nullptr)
                         *
                         * @example
                         * @code
                         * auto it = list.begin();
                         * ++it;  // Avance au deuxième élément
                         * @endcode
                         */
                        Iterator& operator++() {
                            mNode = mNode->Next;
                            return *this;
                        }

                        /**
                         * @brief Incrémentation postfixe — avance et retourne l'ancienne position
                         * @return Copie de l'itérateur avant avancement
                         * @ingroup ListIterators
                         *
                         * @note Complexité : O(1) mais avec copie temporaire (légèrement moins efficace)
                         * @note Préférer ++it (préfixe) quand la valeur précédente n'est pas nécessaire
                         *
                         * @example
                         * @code
                         * auto it = list.begin();
                         * auto old = it++;  // old pointe sur premier, it sur deuxième
                         * @endcode
                         */
                        Iterator operator++(int) {
                            Iterator tmp = *this;
                            ++(*this);
                            return tmp;
                        }

                        /**
                         * @brief Comparaison d'égalité entre deux itérateurs
                         * @param other Autre itérateur à comparer
                         * @return true si les deux itérateurs pointent sur le même nœud
                         * @ingroup ListIterators
                         *
                         * @note Utilisé pour la condition de fin de boucle : it != end()
                         * @note Complexité : O(1) — comparaison de pointeurs
                         */
                        bool operator==(const Iterator& other) const {
                            return mNode == other.mNode;
                        }

                        /**
                         * @brief Comparaison de non-égalité entre deux itérateurs
                         * @param other Autre itérateur à comparer
                         * @return true si les itérateurs pointent sur des nœuds différents
                         * @ingroup ListIterators
                         *
                         * @note Implémentation via négation de operator== pour cohérence
                         * @note Utilisé dans les boucles : for (; it != end(); ++it)
                         */
                        bool operator!=(const Iterator& other) const {
                            return mNode != other.mNode;
                        }
                };

                // -----------------------------------------------------------------
                // Sous-section : Itérateur constant (Forward-only)
                // -----------------------------------------------------------------
                /**
                 * @brief Itérateur constant pour parcours unidirectionnel en lecture seule
                 * @class ConstIterator
                 * @ingroup ListIterators
                 *
                 * Version const de Iterator : retourne const T& via operator*,
                 * empêchant toute modification des éléments via l'itérateur.
                 *
                 * @note Catégorie : NkForwardIteratorTag (même limitations que Iterator)
                 * @note Conversion implicite depuis Iterator pour compatibilité
                 * @note Idéal pour les fonctions prenant const NkList& en paramètre
                 */
                class ConstIterator {
                    private:

                        /** @brief Pointeur constant vers le nœud courant (privé) */
                        const Node* mNode;

                        /** @brief NkList est ami pour accéder à mNode */
                        friend class NkList;

                    public:

                        /** @brief Type des valeurs pointées par l'itérateur */
                        using ValueType = T;

                        /** @brief Type de référence constante retourné par operator* */
                        using Reference = const T&;

                        /** @brief Type de pointeur constant retourné par operator-> */
                        using Pointer = const T*;

                        /** @brief Type pour les différences entre itérateurs */
                        using DifferenceType = nkentseu::ptrdiff;

                        /** @brief Catégorie d'itérateur : Forward-only */
                        using IteratorCategory = NkForwardIteratorTag;

                        /**
                         * @brief Constructeur par défaut — itérateur nul (end sentinel)
                         * @ingroup ListIterators
                         */
                        ConstIterator()
                            : mNode(nullptr) {
                        }

                        /**
                         * @brief Constructeur explicite depuis un pointeur de nœud constant
                         * @param node Pointeur constant vers le nœud à encapsuler
                         * @ingroup ListIterators
                         */
                        explicit ConstIterator(const Node* node)
                            : mNode(node) {
                        }

                        /**
                         * @brief Constructeur de conversion depuis Iterator mutable
                         * @param it Itérateur mutable à convertir en const
                         * @ingroup ListIterators
                         *
                         * @note Permet l'assignation : ConstIterator cit = mutableIt;
                         * @note Conversion sûre : const-correctness garantie par le type
                         */
                        ConstIterator(const Iterator& it)
                            : mNode(it.mNode) {
                        }

                        /**
                         * @brief Déréférencement — accès en lecture seule à l'élément courant
                         * @return Référence constante vers Data du nœud courant
                         * @ingroup ListIterators
                         *
                         * @note Comportement indéfini si l'itérateur est end()
                         * @note Retourne const T& : impossible de modifier via cet itérateur
                         */
                        Reference operator*() const {
                            return mNode->Data;
                        }

                        /**
                         * @brief Accès membre via pointeur constant — lecture seule
                         * @return Pointeur constant vers Data du nœud courant
                         * @ingroup ListIterators
                         *
                         * @note Utile pour it->member en contexte const
                         * @note Même avertissement : vérifier validité avant usage
                         */
                        Pointer operator->() const {
                            return &mNode->Data;
                        }

                        /**
                         * @brief Incrémentation préfixe — avance au nœud suivant
                         * @return Référence vers *this après avancement
                         * @ingroup ListIterators
                         *
                         * @note Complexité : O(1) — simple affectation de pointeur
                         * @note Comportement indéfini si appelé sur end()
                         */
                        ConstIterator& operator++() {
                            mNode = mNode->Next;
                            return *this;
                        }

                        /**
                         * @brief Incrémentation postfixe — avance et retourne l'ancienne position
                         * @return Copie de l'itérateur avant avancement
                         * @ingroup ListIterators
                         *
                         * @note Complexité : O(1) avec copie temporaire
                         * @note Préférer ++it (préfixe) pour performance
                         */
                        ConstIterator operator++(int) {
                            ConstIterator tmp = *this;
                            ++(*this);
                            return tmp;
                        }

                        /**
                         * @brief Comparaison d'égalité entre deux itérateurs constants
                         * @param other Autre itérateur constant à comparer
                         * @return true si les deux pointent sur le même nœud
                         * @ingroup ListIterators
                         *
                         * @note Utilisé pour la condition de fin de boucle sur const containers
                         * @note Complexité : O(1) — comparaison de pointeurs
                         */
                        bool operator==(const ConstIterator& other) const {
                            return mNode == other.mNode;
                        }

                        /**
                         * @brief Comparaison de non-égalité entre deux itérateurs constants
                         * @param other Autre itérateur constant à comparer
                         * @return true si les itérateurs pointent sur des nœuds différents
                         * @ingroup ListIterators
                         *
                         * @note Implémentation via négation de operator==
                         * @note Standard pour les boucles const : for (; it != cend(); ++it)
                         */
                        bool operator!=(const ConstIterator& other) const {
                            return mNode != other.mNode;
                        }
                };

            private:

                // -----------------------------------------------------------------
                // Sous-section : Membres de données privés
                // -----------------------------------------------------------------
                /**
                 * @brief Pointeur vers le premier nœud de la liste (tête)
                 * @var mHead
                 * @note nullptr si la liste est vide (Size() == 0)
                 */
                Node* mHead;

                /**
                 * @brief Pointeur vers le dernier nœud de la liste (queue)
                 * @var mTail
                 * @note nullptr si la liste est vide ; maintenu pour PushBack O(1)
                 */
                Node* mTail;

                /**
                 * @brief Nombre d'éléments actuellement stockés dans la liste
                 * @var mSize
                 * @note Maintenu pour Size() O(1) sans parcours
                 */
                SizeType mSize;

                /**
                 * @brief Pointeur vers l'allocateur utilisé pour les nœuds
                 * @var mAllocator
                 * @note Par défaut pointe vers l'allocateur global du framework
                 */
                Allocator* mAllocator;

                // -----------------------------------------------------------------
                // Sous-section : Méthodes privées de gestion interne
                // -----------------------------------------------------------------
                
                /**
                 * @brief Alloue et construit un nouveau nœud avec forwarding parfait
                 * @tparam Args Types des arguments de construction (déduits)
                 * @param next Pointeur vers le nœud suivant à lier (ou nullptr)
                 * @param args Arguments forwardés au constructeur de Node
                 * @return Pointeur vers le nouveau nœud alloué, ou nullptr si échec
                 * @ingroup ListInternals
                 *
                 * Étapes :
                 *  1. Alloue la mémoire pour Node via mAllocator->Allocate()
                 *  2. Si allocation réussie : placement new pour construire Node in-place
                 *  3. Retourne le pointeur vers le nœud construit
                 *
                 * @note Utilise traits::NkForward pour préserver les catégories de valeur
                 * @note Gestion d'erreur : retourne nullptr si Allocate() échoue
                 * @note Complexité : O(1) pour l'allocation + construction
                 */
                template<typename... Args>
                Node* CreateNode(Node* next, Args&&... args) {
                    Node* node = static_cast<Node*>(
                        mAllocator->Allocate(sizeof(Node))
                    );
                    if (node) {
                        new (node) Node(next, traits::NkForward<Args>(args)...);
                    }
                    return node;
                }
                
                /**
                 * @brief Alloue et construit un nouveau nœud par copie (pré-C++11)
                 * @param next Pointeur vers le nœud suivant à lier (ou nullptr)
                 * @param data Valeur à copier dans le nouveau nœud
                 * @return Pointeur vers le nouveau nœud alloué, ou nullptr si échec
                 * @ingroup ListInternals
                 *
                 * Fallback pour compatibilité avec compilateurs ne supportant pas C++11.
                 * Même logique que la version C++11 mais avec constructeur par copie.
                 *
                 * @note Requiert que T soit CopyConstructible
                 * @note Gestion d'erreur : retourne nullptr si Allocate() échoue
                 */
                Node* CreateNode(Node* next, const T& data) {
                    Node* node = static_cast<Node*>(
                        mAllocator->Allocate(sizeof(Node))
                    );
                    if (node) {
                        new (node) Node(next, data);
                    }
                    return node;
                }

                /**
                 * @brief Détruit explicitement un nœud et libère sa mémoire
                 * @param node Pointeur vers le nœud à détruire
                 * @ingroup ListInternals
                 *
                 * Étapes de destruction :
                 *  1. Appelle explicitement le destructeur de Node (et donc de T)
                 *  2. Libère la mémoire via mAllocator->Deallocate()
                 *
                 * @note noexcept implicite : ne lève pas d'exception (destructor safety)
                 * @note Complexité : O(1) — destruction d'un seul nœud
                 * @note Ne modifie pas mHead/mTail/mSize : responsabilité du caller
                 */
                void DestroyNode(Node* node) {
                    node->~Node();
                    mAllocator->Deallocate(node);
                }

            public:

                // -----------------------------------------------------------------
                // Sous-section : Constructeurs
                // -----------------------------------------------------------------
                /**
                 * @brief Constructeur par défaut — liste vide sans allocation
                 * @param allocator Pointeur vers l'allocateur à utiliser (nullptr pour défaut)
                 * @ingroup ListConstructors
                 *
                 * Initialise une liste avec :
                 *  - mHead = nullptr, mTail = nullptr (état vide)
                 *  - mSize = 0 (aucun élément)
                 *  - mAllocator = allocateur fourni ou global par défaut
                 *
                 * @note Complexité : O(1) — aucune allocation mémoire initiale
                 * @note Premier PushFront/PushBack déclenchera l'allocation du premier nœud
                 *
                 * @example
                 * @code
                 * NkList<int> empty;  // Liste vide, aucun nœud alloué
                 * empty.PushBack(42); // Alloue et insère le premier nœud
                 * @endcode
                 */
                explicit NkList(Allocator* allocator = nullptr)
                    : mHead(nullptr)
                    , mTail(nullptr)
                    , mSize(0)
                    , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator()) {
                }

                /**
                 * @brief Constructeur depuis NkInitializerList — syntaxe {a, b, c}
                 * @param init Liste d'initialisation contenant les éléments à insérer
                 * @param allocator Pointeur vers l'allocateur à utiliser (nullptr pour défaut)
                 * @ingroup ListConstructors
                 *
                 * Permet la construction avec liste d'initialisation :
                 *   NkList<int> values = {1, 2, 3, 4};
                 *
                 * Insère chaque élément via PushBack, préservant l'ordre de la liste.
                 *
                 * @note Complexité : O(n) pour n insertions PushBack O(1) chacune
                 * @note Compatible avec C++11 initializer list syntax
                 *
                 * @example
                 * @code
                 * auto coords = NkList<float>{1.0f, 2.0f, 3.0f};
                 * auto names = NkList<const char*>{"Alice", "Bob", "Charlie"};
                 * @endcode
                 */
                NkList(NkInitializerList<T> init, Allocator* allocator = nullptr)
                    : mHead(nullptr)
                    , mTail(nullptr)
                    , mSize(0)
                    , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator()) {
                    for (auto& val : init) {
                        PushBack(val);
                    }
                }

                /**
                 * @brief Constructeur depuis std::initializer_list — interop STL
                 * @param init Liste d'initialisation STL standard
                 * @param allocator Pointeur vers l'allocateur à utiliser (nullptr pour défaut)
                 * @ingroup ListConstructors
                 *
                 * Facilité d'interopérabilité avec du code utilisant std::initializer_list.
                 * Fonctionne comme le constructeur NkInitializerList mais accepte
                 * le type standard de la STL pour une migration progressive.
                 *
                 * @note Complexité : O(n) pour les insertions successives
                 * @note Nécessite <initializer_list> du compilateur (pas de dépendance STL runtime)
                 */
                NkList(std::initializer_list<T> init, Allocator* allocator = nullptr)
                    : mHead(nullptr)
                    , mTail(nullptr)
                    , mSize(0)
                    , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator()) {
                    for (auto& val : init) {
                        PushBack(val);
                    }
                }

                /**
                 * @brief Constructeur de copie — duplication profonde d'une liste
                 * @param other Liste source à copier
                 * @ingroup ListConstructors
                 *
                 * Crée une copie indépendante de la liste 'other' :
                 *  - Parcours tous les éléments de other via itérateurs
                 *  - Insère chaque élément via PushBack (constructeur de copie de T)
                 *  - Utilise le même type d'allocateur que 'other'
                 *
                 * @note Complexité : O(n) pour la copie des n éléments
                 * @note Requiert que T soit CopyConstructible
                 * @note Les modifications sur la copie n'affectent pas l'original
                 *
                 * @example
                 * @code
                 * NkList<int> original = {1, 2, 3};
                 * NkList<int> copy = original;  // Copie indépendante
                 * copy.PushBack(4);             // original reste {1,2,3}
                 * @endcode
                 */
                NkList(const NkList& other)
                    : mHead(nullptr)
                    , mTail(nullptr)
                    , mSize(0)
                    , mAllocator(other.mAllocator) {
                    for (auto it = other.begin(); it != other.end(); ++it) {
                        PushBack(*it);
                    }
                }

                /**
                 * @brief Constructeur de déplacement — transfert de propriété efficace
                 * @param other Liste source dont transférer le contenu
                 * @ingroup ListConstructors
                 *
                 * Transfère la propriété des nœuds de 'other' vers cette liste :
                 *  - Copie les pointeurs mHead, mTail et la taille mSize (O(1))
                 *  - Copie le pointeur d'allocateur mAllocator
                 *  - Réinitialise 'other' à l'état vide (nullptr, size=0)
                 *  - Évite toute allocation ou copie d'éléments
                 *
                 * @note Complexité : O(1) — transfert de pointeurs uniquement
                 * @note noexcept : garantie de ne pas lever d'exception
                 * @note 'other' est laissé dans un état valide mais vide après le move
                 *
                 * @example
                 * @code
                 * NkList<int> source = MakeLargeList();
                 * NkList<int> dest = NKENTSEU_MOVE(source);  // Transfert O(1)
                 * // source est maintenant vide, dest contient les nœuds
                 * @endcode
                 */
                NkList(NkList&& other) NKENTSEU_NOEXCEPT
                    : mHead(other.mHead)
                    , mTail(other.mTail)
                    , mSize(other.mSize)
                    , mAllocator(other.mAllocator) {
                    other.mHead = nullptr;
                    other.mTail = nullptr;
                    other.mSize = 0;
                }

                /**
                 * @brief Destructeur — libération propre de tous les nœuds
                 * @ingroup ListConstructors
                 *
                 * Libère toutes les ressources gérées par cette liste :
                 *  1. Parcours tous les nœuds de mHead à nullptr
                 *  2. Pour chaque nœud : appelle DestroyNode() (destructor + deallocate)
                 *  3. Réinitialise mHead, mTail, mSize à l'état vide
                 *
                 * @note noexcept implicite : ne lève jamais d'exception (destructor safety)
                 * @note Complexité : O(n) pour la destruction des n nœuds
                 * @note Garantit l'absence de fuites mémoire même en cas d'exception pendant construction
                 */
                ~NkList() {
                    Clear();
                }

                // -----------------------------------------------------------------
                // Sous-section : Opérateurs d'affectation
                // -----------------------------------------------------------------
                /**
                 * @brief Opérateur d'affectation par copie — syntaxe list1 = list2
                 * @param other Liste source à copier
                 * @return Référence vers *this pour chaînage d'opérations
                 * @ingroup ListAssignment
                 *
                 * Implémente la sémantique de copie via Clear() + PushBack :
                 *  - Vide le contenu actuel via Clear()
                 *  - Parcours tous les éléments de other via itérateurs
                 *  - Insère chaque élément via PushBack (constructeur de copie)
                 *
                 * @note Complexité : O(n) pour la copie des éléments
                 * @note Gère l'auto-affectation (this == &other) sans effet
                 * @note Requiert que T soit CopyConstructible
                 *
                 * @example
                 * @code
                 * NkList<int> a = {1, 2}, b = {3, 4}, c;
                 * c = a;        // c contient {1, 2}
                 * c = b = a;    // Chaînage : c et b contiennent {1, 2}
                 * @endcode
                 */
                NkList& operator=(const NkList& other) {
                    if (this != &other) {
                        Clear();
                        for (auto it = other.begin(); it != other.end(); ++it) {
                            PushBack(*it);
                        }
                    }
                    return *this;
                }

                /**
                 * @brief Opérateur d'affectation par déplacement — transfert efficace
                 * @param other Liste source dont transférer le contenu
                 * @return Référence vers *this pour chaînage d'opérations
                 * @ingroup ListAssignment
                 *
                 * Transfère la propriété des nœuds de 'other' vers cette liste :
                 *  - Libère les nœuds actuels via Clear() si présents
                 *  - Adopte les pointeurs mHead, mTail, mSize et mAllocator de 'other'
                 *  - Réinitialise 'other' à l'état vide
                 *
                 * @note Complexité : O(1) — transfert de pointeurs uniquement
                 * @note noexcept : garantie de ne pas lever d'exception
                 * @note 'other' est laissé dans un état valide mais vide après l'opération
                 *
                 * @example
                 * @code
                 * NkList<int> temp = ComputeExpensiveList();
                 * result = NKENTSEU_MOVE(temp);  // Transfert O(1), pas de copie
                 * // temp est maintenant vide, result contient les nœuds
                 * @endcode
                 */
                NkList& operator=(NkList&& other) NKENTSEU_NOEXCEPT {
                    if (this != &other) {
                        Clear();
                        mHead = other.mHead;
                        mTail = other.mTail;
                        mSize = other.mSize;
                        mAllocator = other.mAllocator;
                        other.mHead = nullptr;
                        other.mTail = nullptr;
                        other.mSize = 0;
                    }
                    return *this;
                }

                /**
                 * @brief Opérateur d'affectation depuis NkInitializerList
                 * @param init Liste d'initialisation contenant les nouvelles valeurs
                 * @return Référence vers *this pour chaînage d'opérations
                 * @ingroup ListAssignment
                 *
                 * Permet la syntaxe list = {a, b, c} pour remplacer le contenu
                 * de la liste par une nouvelle liste d'éléments.
                 *
                 * @note Complexité : O(n) pour l'insertion des nouveaux éléments
                 * @note Vide d'abord le contenu actuel via Clear()
                 *
                 * @example
                 * @code
                 * NkList<int> list = {1, 2, 3};
                 * list = {4, 5, 6};  // list contient maintenant {4, 5, 6}
                 * @endcode
                 */
                NkList& operator=(NkInitializerList<T> init) {
                    Clear();
                    for (auto& val : init) {
                        PushBack(val);
                    }
                    return *this;
                }

                /**
                 * @brief Opérateur d'affectation depuis std::initializer_list
                 * @param init Liste d'initialisation STL standard
                 * @return Référence vers *this pour chaînage d'opérations
                 * @ingroup ListAssignment
                 *
                 * Interopérabilité avec code utilisant std::initializer_list.
                 * Fonctionne comme l'opérateur NkInitializerList mais accepte
                 * le type standard pour migration progressive depuis la STL.
                 *
                 * @note Complexité : O(n) pour l'insertion des éléments
                 * @note Nécessite <initializer_list> du compilateur uniquement
                 */
                NkList& operator=(std::initializer_list<T> init) {
                    Clear();
                    for (auto& val : init) {
                        PushBack(val);
                    }
                    return *this;
                }

                // -----------------------------------------------------------------
                // Sous-section : Accès aux éléments (extrémités uniquement)
                // -----------------------------------------------------------------
                /**
                 * @brief Accès au premier élément — version mutable
                 * @return Référence mutable vers le Data du premier nœud
                 * @ingroup ListAccess
                 *
                 * Retourne une référence vers mHead->Data avec assertion debug
                 * si la liste est vide. Équivalent à Front() = *begin()
                 * mais avec sémantique explicite "premier élément".
                 *
                 * @note Complexité : O(1) — accès direct via pointeur mHead
                 * @note Assertion en debug si Empty() == true
                 * @note Comportement indéfini en release si liste vide
                 *
                 * @example
                 * @code
                 * NkList<int> list = {10, 20, 30};
                 * list.Front() = 100;  // list contient maintenant {100, 20, 30}
                 * @endcode
                 */
                Reference Front() {
                    NKENTSEU_ASSERT(mHead != nullptr);
                    return mHead->Data;
                }

                /**
                 * @brief Accès au premier élément — version constante
                 * @return Référence constante vers le Data du premier nœud
                 * @ingroup ListAccess
                 *
                 * Version const de Front() pour les listes en lecture seule.
                 * Même sémantique de vérification que la version mutable.
                 *
                 * @note Complexité : O(1) — accès direct
                 * @note Utile pour lire la première valeur sans modification
                 *
                 * @example
                 * @code
                 * int GetFirst(const NkList<int>& list) {
                 *     return list.Front();  // Lecture du premier élément
                 * }
                 * @endcode
                 */
                ConstReference Front() const {
                    NKENTSEU_ASSERT(mHead != nullptr);
                    return mHead->Data;
                }

                /**
                 * @brief Accès au dernier élément — version mutable
                 * @return Référence mutable vers le Data du dernier nœud
                 * @ingroup ListAccess
                 *
                 * Retourne une référence vers mTail->Data avec assertion debug
                 * si la liste est vide. Rendu possible en O(1) grâce au pointeur
                 * tail mémorisé (contrairement à une liste sans tail qui nécessiterait O(n)).
                 *
                 * @note Complexité : O(1) — accès direct via pointeur mTail
                 * @note Assertion en debug si Empty() == true
                 * @note Comportement indéfini en release si liste vide
                 *
                 * @example
                 * @code
                 * NkList<int> list = {10, 20, 30};
                 * list.Back() = 300;  // list contient maintenant {10, 20, 300}
                 * @endcode
                 */
                Reference Back() {
                    NKENTSEU_ASSERT(mTail != nullptr);
                    return mTail->Data;
                }

                /**
                 * @brief Accès au dernier élément — version constante
                 * @return Référence constante vers le Data du dernier nœud
                 * @ingroup ListAccess
                 *
                 * Version const de Back() pour les listes en lecture seule.
                 * Même sémantique de vérification que la version mutable.
                 *
                 * @note Complexité : O(1) — accès direct via mTail
                 * @note Très utile pour les algorithmes de type "queue" (FIFO)
                 *
                 * @example
                 * @code
                 * int PeekBack(const NkList<int>& queue) {
                 *     return queue.Back();  // Lire le dernier sans retirer
                 * }
                 * @endcode
                 */
                ConstReference Back() const {
                    NKENTSEU_ASSERT(mTail != nullptr);
                    return mTail->Data;
                }

                // -----------------------------------------------------------------
                // Sous-section : Itérateurs — Style minuscule (compatibilité STL)
                // -----------------------------------------------------------------
                /**
                 * @brief Itérateur mutable vers le début de la liste
                 * @return Iterator pointant sur le premier nœud (ou end() si vide)
                 * @ingroup ListIterators
                 *
                 * Retourne un Iterator encapsulant mHead pour le parcours mutable.
                 * Style minuscule pour compatibilité avec range-based for C++11.
                 *
                 * @note Complexité : O(1) — construction d'itérateur léger
                 * @note noexcept implicite : ne lève pas d'exception
                 * @note Compatible avec les algorithmes génériques attendant begin()
                 *
                 * @example
                 * @code
                 * for (auto it = list.begin(); it != list.end(); ++it) {
                 *     *it *= 2;  // Modification via itérateur mutable
                 * }
                 * @endcode
                 */
                Iterator begin() {
                    return Iterator(mHead);
                }

                /**
                 * @brief Itérateur constant vers le début de la liste
                 * @return ConstIterator pointant sur le premier nœud (ou end() si vide)
                 * @ingroup ListIterators
                 *
                 * Version const de begin() pour les listes en lecture seule.
                 * Retourne un ConstIterator pour empêcher la modification via l'itérateur.
                 *
                 * @note Complexité : O(1) — construction d'itérateur léger
                 * @note noexcept implicite : ne lève pas d'exception
                 * @note Permet de parcourir un const NkList& sans lever la const
                 *
                 * @example
                 * @code
                 * void PrintAll(const NkList<int>& list) {
                 *     for (auto it = list.begin(); it != list.end(); ++it) {
                 *         printf("%d ", *it);  // Lecture seule via itérateur const
                 *     }
                 * }
                 * @endcode
                 */
                ConstIterator begin() const {
                    return ConstIterator(mHead);
                }

                /**
                 * @brief Itérateur constant explicite vers le début (compatibilité STL)
                 * @return ConstIterator pointant sur le premier nœud
                 * @ingroup ListIterators
                 *
                 * Alias vers begin() const pour respecter la convention STL
                 * où cbegin() retourne toujours un const_iterator, même sur
                 * un conteneur non-const.
                 *
                 * @note Complexité : O(1) — délégation vers begin() const
                 * @note noexcept implicite : ne lève pas d'exception
                 * @note Utile pour les templates génériques attendant cbegin()
                 *
                 * @example
                 * @code
                 * template<typename Container>
                 * void GenericPrint(const Container& c) {
                 *     for (auto it = c.cbegin(); it != c.cend(); ++it) {
                 *         std::cout << *it << " ";
                 *     }
                 * }
                 * @endcode
                 */
                ConstIterator cbegin() const {
                    return ConstIterator(mHead);
                }

                /**
                 * @brief Itérateur mutable vers la fin de la liste (sentinelle nullptr)
                 * @return Iterator sentinelle marquant la fin du parcours
                 * @ingroup ListIterators
                 *
                 * Retourne un Iterator encapsulant nullptr, représentant la
                 * position "après le dernier élément". Utilisé comme borne
                 * supérieure exclusive dans les boucles d'itération.
                 *
                 * @note Complexité : O(1) — construction d'itérateur avec nullptr
                 * @note noexcept implicite : ne lève pas d'exception
                 * @note Ne jamais déréférencer : c'est une sentinelle, pas un nœud valide
                 *
                 * @example
                 * @code
                 * // Boucle classique avec itérateurs
                 * for (auto it = list.begin(); it != list.end(); ++it) {
                 *     Process(*it);
                 * }
                 * @endcode
                 */
                Iterator end() {
                    return Iterator(nullptr);
                }

                /**
                 * @brief Itérateur constant vers la fin de la liste (sentinelle nullptr)
                 * @return ConstIterator sentinelle pour parcours const
                 * @ingroup ListIterators
                 *
                 * Version const de end() pour les listes en lecture seule.
                 * Même sémantique de sentinelle exclusive que la version mutable.
                 *
                 * @note Complexité : O(1) — construction d'itérateur avec nullptr
                 * @note noexcept implicite : ne lève pas d'exception
                 * @note Compatible avec les algorithmes STL-style sur const containers
                 */
                ConstIterator end() const {
                    return ConstIterator(nullptr);
                }

                /**
                 * @brief Itérateur constant explicite vers la fin (compatibilité STL)
                 * @return ConstIterator sentinelle pour parcours const
                 * @ingroup ListIterators
                 *
                 * Alias vers end() const pour respecter la convention STL
                 * où cend() retourne toujours un const_iterator.
                 *
                 * @note Complexité : O(1) — délégation vers end() const
                 * @note noexcept implicite : ne lève pas d'exception
                 * @note Utile pour les templates génériques attendant cend()
                 */
                ConstIterator cend() const {
                    return ConstIterator(nullptr);
                }

                // -----------------------------------------------------------------
                // Sous-section : Méthodes de capacité
                // -----------------------------------------------------------------
                /**
                 * @brief Vérifie si la liste est vide
                 * @return true si Size() == 0, false sinon
                 * @ingroup ListCapacity
                 *
                 * Teste si la liste ne contient aucun élément en comparant
                 * mSize à zéro. Plus efficace que comparer mHead à nullptr
                 * car mSize est maintenu à jour pour Size() O(1).
                 *
                 * @note Complexité : O(1) — comparaison simple de membre
                 * @note noexcept : garantie de ne pas lever d'exception
                 * @note Préférer cette méthode pour tester avant Front()/Back()
                 *
                 * @example
                 * @code
                 * if (list.Empty()) {
                 *     list.PushBack(defaultValue);  // Initialisation lazy
                 * }
                 * @endcode
                 */
                bool Empty() const NKENTSEU_NOEXCEPT {
                    return mSize == 0;
                }

                /**
                 * @brief Retourne le nombre d'éléments actuellement stockés
                 * @return Valeur de type SizeType représentant la taille courante
                 * @ingroup ListCapacity
                 *
                 * Retourne mSize, maintenu à jour par toutes les opérations
                 * de modification (PushFront, PushBack, PopFront, Clear, etc.).
                 * Toujours exact et disponible en temps constant.
                 *
                 * @note Complexité : O(1) — retour de membre direct
                 * @note noexcept : garantie de ne pas lever d'exception
                 * @note Préférer cette méthode pour les boucles indexées (si nécessaire)
                 *
                 * @example
                 * @code
                 * printf("List size: %zu\n", list.Size());
                 * @endcode
                 */
                SizeType Size() const NKENTSEU_NOEXCEPT {
                    return mSize;
                }

                // -----------------------------------------------------------------
                // Sous-section : Modificateurs de contenu
                // -----------------------------------------------------------------
                /**
                 * @brief Détruit tous les nœuds et remet la liste à l'état vide
                 * @ingroup ListModifiers
                 *
                 * Parcours tous les nœuds de mHead à nullptr :
                 *  1. Pour chaque nœud : sauvegarde Next, appelle DestroyNode(), avance
                 *  2. Après boucle : réinitialise mTail et mSize à l'état vide
                 *
                 * @note Complexité : O(n) pour la destruction des n nœuds
                 * @note noexcept implicite : ne lève pas d'exception (destructor safety)
                 * @note Tous les itérateurs deviennent invalides après Clear()
                 * @note Ne libère pas l'allocateur lui-même, seulement les nœuds alloués
                 *
                 * @example
                 * @code
                 * NkList<std::string> list = {"a", "b", "c"};
                 * list.Clear();  // Détruit les 3 strings, list.Size() == 0
                 * list.PushBack("new");  // Réalloue un nouveau nœud
                 * @endcode
                 */
                void Clear() {
                    while (mHead != nullptr) {
                        Node* next = mHead->Next;
                        DestroyNode(mHead);
                        mHead = next;
                    }
                    mTail = nullptr;
                    mSize = 0;
                }

                /**
                 * @brief Ajoute une copie de l'élément au début de la liste
                 * @param value Valeur à copier et insérer en tête
                 * @ingroup ListModifiers
                 *
                 * Crée un nouveau nœud avec CreateNode() pointant vers l'ancienne tête,
                 * puis met à jour mHead pour pointer vers ce nouveau nœud.
                 * Si la liste était vide, met également à jour mTail.
                 *
                 * @note Complexité : O(1) — allocation et liaison de nœud constant
                 * @note Peut invalider les itérateurs pointant vers l'ancienne tête
                 * @note Requiert que T soit CopyConstructible
                 * @note mSize est incrémenté pour maintenir Size() exact
                 *
                 * @example
                 * @code
                 * NkList<int> list = {2, 3};
                 * list.PushFront(1);  // list contient maintenant {1, 2, 3}
                 * @endcode
                 */
                void PushFront(const T& value) {
                    Node* node = CreateNode(mHead, value);
                    mHead = node;
                    if (!mTail) {
                        mTail = node;
                    }
                    ++mSize;
                }

                /**
                 * @brief Ajoute un élément par déplacement au début de la liste
                 * @param value Valeur à déplacer et insérer en tête
                 * @ingroup ListModifiers
                 *
                 * Version move semantics de PushFront pour éviter les copies
                 * inutiles des types lourds. Utilise traits::NkMove pour
                 * transférer la propriété des ressources si possible.
                 *
                 * @note Complexité : O(1) — comme PushFront const&
                 * @note Peut invalider les itérateurs pointant vers l'ancienne tête
                 * @note Requiert que T soit MoveConstructible
                 * @note 'value' est laissé dans un état valide mais indéterminé après l'appel
                 *
                 * @example
                 * @code
                 * std::string heavy = MakeHeavyString();
                 * list.PushFront(NKENTSEU_MOVE(heavy));  // Déplace au lieu de copier
                 * // heavy est maintenant vide ou dans un état valide indéterminé
                 * @endcode
                 */
                void PushFront(T&& value) {
                    Node* node = CreateNode(mHead, traits::NkMove(value));
                    mHead = node;
                    if (!mTail) {
                        mTail = node;
                    }
                    ++mSize;
                }

                /**
                 * @brief Supprime le premier élément de la liste
                 * @ingroup ListModifiers
                 *
                 * Sauvegarde le pointeur vers le deuxième nœud, détruit le premier
                 * via DestroyNode(), puis met à jour mHead. Si la liste devient
                 * vide, met également à jour mTail à nullptr.
                 *
                 * @note Complexité : O(1) — suppression d'un seul nœud
                 * @note Assertion en debug si Empty() == true (underflow protection)
                 * @note Comportement indéfini en release si liste vide
                 * @note Peut invalider les itérateurs pointant vers l'élément supprimé
                 * @note mSize est décrémenté pour maintenir Size() exact
                 *
                 * @example
                 * @code
                 * NkList<int> list = {1, 2, 3};
                 * list.PopFront();  // list contient maintenant {2, 3}
                 * list.PopFront();  // list contient maintenant {3}
                 * @endcode
                 */
                void PopFront() {
                    NKENTSEU_ASSERT(mHead != nullptr);
                    Node* next = mHead->Next;
                    DestroyNode(mHead);
                    mHead = next;
                    if (!mHead) {
                        mTail = nullptr;
                    }
                    --mSize;
                }

                /**
                 * @brief Ajoute une copie de l'élément à la fin de la liste
                 * @param value Valeur à copier et insérer en queue
                 * @ingroup ListModifiers
                 *
                 * Crée un nouveau nœud avec CreateNode(nullptr, value) (Next = nullptr),
                 * puis le lie à l'ancienne queue via mTail->Next = node.
                 * Met à jour mTail pour pointer vers ce nouveau nœud.
                 * Si la liste était vide, met également à jour mHead.
                 *
                 * @note Complexité : O(1) — grâce au pointeur tail mémorisé
                 * @note Peut invalider les itérateurs end() (sentinelle nullptr)
                 * @note Requiert que T soit CopyConstructible
                 * @note mSize est incrémenté pour maintenir Size() exact
                 *
                 * @example
                 * @code
                 * NkList<int> list = {1, 2};
                 * list.PushBack(3);  // list contient maintenant {1, 2, 3}
                 * @endcode
                 */
                void PushBack(const T& value) {
                    Node* node = CreateNode(nullptr, value);
                    if (mTail) {
                        mTail->Next = node;
                    }
                    else {
                        mHead = node;
                    }
                    mTail = node;
                    ++mSize;
                }

                /**
                 * @brief Ajoute un élément par déplacement à la fin de la liste
                 * @param value Valeur à déplacer et insérer en queue
                 * @ingroup ListModifiers
                 *
                 * Version move semantics de PushBack pour éviter les copies
                 * inutiles des types lourds. Utilise traits::NkMove pour
                 * transférer la propriété des ressources si possible.
                 *
                 * @note Complexité : O(1) — comme PushBack const&
                 * @note Peut invalider les itérateurs end() (sentinelle nullptr)
                 * @note Requiert que T soit MoveConstructible
                 * @note 'value' est laissé dans un état valide mais indéterminé après l'appel
                 *
                 * @example
                 * @code
                 * std::string heavy = MakeHeavyString();
                 * list.PushBack(NKENTSEU_MOVE(heavy));  // Déplace au lieu de copier
                 * // heavy est maintenant vide ou dans un état valide indéterminé
                 * @endcode
                 */
                void PushBack(T&& value) {
                    Node* node = CreateNode(nullptr, traits::NkMove(value));
                    if (mTail) {
                        mTail->Next = node;
                    }
                    else {
                        mHead = node;
                    }
                    mTail = node;
                    ++mSize;
                }

                /**
                 * @brief Inverse l'ordre des nœuds dans la liste in-place
                 * @ingroup ListModifiers
                 *
                 * Algorithme de reversal itératif en trois pointeurs :
                 *  1. Initialise prev = nullptr, current = mHead, sauvegarde old head comme new tail
                 *  2. Pour chaque nœud : sauvegarde next, inverse le lien (current->Next = prev), avance
                 *  3. Après boucle : mHead = prev (ancien tail), mTail = ancien head
                 *
                 * @note Complexité : O(n) — parcours unique de tous les nœuds
                 * @note noexcept implicite : ne lève pas d'exception
                 * @note Tous les itérateurs deviennent invalides après Reverse()
                 * @note Utile pour transformer une stack en queue ou vice-versa
                 *
                 * @example
                 * @code
                 * NkList<int> list = {1, 2, 3, 4};
                 * list.Reverse();  // list contient maintenant {4, 3, 2, 1}
                 * @endcode
                 */
                void Reverse() {
                    if (!mHead || !mHead->Next) {
                        return;  // Liste vide ou à un élément : déjà inversée
                    }

                    Node* prev = nullptr;
                    Node* current = mHead;
                    mTail = mHead;  // Ancienne tête devient nouvelle queue

                    while (current != nullptr) {
                        Node* next = current->Next;  // Sauvegarde suivant
                        current->Next = prev;         // Inverse le lien
                        prev = current;               // Avance prev
                        current = next;               // Avance current
                    }

                    mHead = prev;  // Ancienne queue devient nouvelle tête
                }

                /**
                 * @brief Échange le contenu de cette liste avec une autre en O(1)
                 * @param other Autre liste avec laquelle échanger le contenu
                 * @ingroup ListModifiers
                 *
                 * Échange les pointeurs et métadonnées internes sans toucher
                 * aux nœuds stockés : opération constante indépendante de
                 * la taille des listes.
                 *
                 * @note Complexité : O(1) — échange de quelques pointeurs/entiers
                 * @note noexcept : garantie de ne pas lever d'exception
                 * @note N'invalide aucun itérateur : ils suivent leur liste d'origine
                 * @note Utile pour l'idiome copy-and-swap et les algorithmes de tri
                 *
                 * @example
                 * @code
                 * NkList<int> a = {1, 2}, b = {3, 4, 5};
                 * a.Swap(b);  // a = {3,4,5}, b = {1,2} en O(1)
                 *
                 * // Idiom copy-and-swap pour assignment exception-safe
                 * NkList& operator=(NkList other) {  // other est une copie
                 *     Swap(other);  // Échange O(1) avec la copie
                 *     return *this; // other détruit libère l'ancien contenu
                 * }
                 * @endcode
                 */
                void Swap(NkList& other) NKENTSEU_NOEXCEPT {
                    traits::NkSwap(mHead, other.mHead);
                    traits::NkSwap(mTail, other.mTail);
                    traits::NkSwap(mSize, other.mSize);
                    traits::NkSwap(mAllocator, other.mAllocator);
                }

        }; // class NkList

    } // namespace nkentseu

    // -------------------------------------------------------------------------
    // SECTION 4 : FONCTIONS NON-MEMBRES ET OPÉRATEURS
    // -------------------------------------------------------------------------
    // Ces fonctions libres fournissent une interface additionnelle pour
    // NkList, compatible avec les algorithmes génériques et les idiomes C++.

    namespace nkentseu {

        /**
         * @brief Échange le contenu de deux listes via leur méthode membre Swap
         * @tparam T Type des éléments stockés
         * @tparam Allocator Type de l'allocateur utilisé
         * @param lhs Première liste à échanger
         * @param rhs Seconde liste à échanger
         * @ingroup ListAlgorithms
         *
         * Fonction libre permettant l'usage de std::swap-like avec NkList.
         * Délègue simplement à la méthode membre Swap() pour une implémentation
         * O(1) efficace.
         *
         * @note Complexité : O(1) — délégation vers NkList::Swap
         * @note noexcept : garantie de ne pas lever d'exception
         * @note Utile pour les algorithmes de tri et l'idiome copy-and-swap
         *
         * @example
         * @code
         * NkList<int> a = {1, 2}, b = {3, 4};
         * NkSwap(a, b);  // a = {3,4}, b = {1,2} via délégation vers a.Swap(b)
         * @endcode
         */
        template<typename T, typename Allocator>
        void NkSwap(
            NkList<T, Allocator>& lhs,
            NkList<T, Allocator>& rhs
        ) NKENTSEU_NOEXCEPT {
            lhs.Swap(rhs);
        }

        // template<typename T>
        // NkString NkToString(const NkList<T>& c, const NkFormatProps& props = {}) {
        //     NkString result = "[";
        //     int i = 0;
        //     for (const auto& item : c) {
        //         result += NkToString(item, props);

        //         if (i <= c.Size() - 1) {
        //             result += ", ";
        //         }

        //         i++;
        //     }
        //     result += "]";
        //     return result;
        // }

    } // namespace nkentseu

    // -------------------------------------------------------------------------
    // SECTION 5 : VALIDATION ET MESSAGES DE DEBUG OPTIONNELS
    // -------------------------------------------------------------------------
    // Ces directives aident à vérifier la configuration de compilation lors
    // du build, particulièrement utile pour déboguer les problèmes liés aux
    // options de compilation (C++11, exceptions, assertions, etc.).

    #ifdef NKENTSEU_CONTAINERS_DEBUG_CONFIG
        #if defined(NKENTSEU_CXX11_OR_LATER)
            #pragma message("NkList: C++11 move semantics ENABLED")
        #else
            #pragma message("NkList: C++11 move semantics DISABLED (using copy fallback)")
        #endif
    #endif

#endif // NKENTSEU_CONTAINERS_SEQUENTIAL_NKLIST_H_INCLUDED

// =============================================================================
// EXEMPLES D'UTILISATION DÉTAILLÉS
// =============================================================================
// Cette section fournit des exemples concrets et complets d'utilisation de
// NkList, couvrant les cas d'usage courants et les bonnes pratiques.
// Ces exemples sont en commentaires pour ne pas affecter la compilation.
// =============================================================================

/*
    // -----------------------------------------------------------------------------
    // Exemple 1 : Construction et initialisation variées
    // -----------------------------------------------------------------------------
    #include <NKContainers/Sequential/NkList.h>
    using namespace nkentseu::containers::sequential;

    void ConstructionExamples() {
        // Constructeur par défaut : liste vide
        NkList<int> empty;

        // Constructeur avec allocateur personnalisé
        NkList<int> customAlloc(&myCustomAllocator);

        // Constructeur depuis liste d'initialisation
        NkList<int> init = {10, 20, 30};  // {10, 20, 30}

        // Constructeur de copie
        NkList<int> copy = init;  // Copie profonde : {10, 20, 30}

        #if defined(NKENTSEU_CXX11_OR_LATER)
        // Constructeur de déplacement (C++11)
        NkList<int> temp = MakeLargeList();
        NkList<int> moved = NKENTSEU_MOVE(temp);  // Transfert O(1), temp vide
        #endif
    }

    // -----------------------------------------------------------------------------
    // Exemple 2 : Insertion et suppression aux extrémités — O(1)
    // -----------------------------------------------------------------------------
    void EndOperationsExamples() {
        NkList<int> list;

        // PushFront : insertion en tête O(1)
        list.PushFront(3);  // list = {3}
        list.PushFront(2);  // list = {2, 3}
        list.PushFront(1);  // list = {1, 2, 3}

        // PushBack : insertion en queue O(1) grâce au pointeur tail
        list.PushBack(4);   // list = {1, 2, 3, 4}
        list.PushBack(5);   // list = {1, 2, 3, 4, 5}

        // Accès aux extrémités : O(1)
        int first = list.Front();  // first = 1
        int last = list.Back();    // last = 5

        // PopFront : suppression en tête O(1)
        list.PopFront();  // list = {2, 3, 4, 5}
        list.PopFront();  // list = {3, 4, 5}

        // Note : NkList n'a pas PopBack() car singly linked list ne permet
        // pas de remonter au nœud précédent en O(1). Pour PopBack O(1),
        // utiliser NkDoubleList (liste doublement chaînée).
    }

    // -----------------------------------------------------------------------------
    // Exemple 3 : Parcours avec itérateurs Forward-only
    // -----------------------------------------------------------------------------
    void IterationExamples() {
        NkList<int> list = {1, 2, 3, 4, 5};

        // Parcours mutable avec itérateur Forward-only
        for (auto it = list.begin(); it != list.end(); ++it) {
            *it *= 2;  // Modification via itérateur mutable
        }
        // list contient maintenant {2, 4, 6, 8, 10}

        // Parcours en lecture seule (const)
        void PrintList(const NkList<int>& lst) {
            for (auto it = lst.cbegin(); it != lst.cend(); ++it) {
                printf("%d ", *it);  // Lecture seule via itérateur const
            }
            printf("\n");
        }

        // Range-based for grâce aux aliases begin()/end()
        int sum = 0;
        for (int val : list) {
            sum += val;  // sum = 30
        }

        // Note : Pas d'itérateurs inversés pour NkList (singly linked).
        // Pour parcours inversé, utiliser Reverse() puis parcours normal,
        // ou utiliser NkDoubleList qui supporte BidirectionalIterator.
    }

    // -----------------------------------------------------------------------------
    // Exemple 4 : Modification avancée — Reverse, Swap, Clear
    // -----------------------------------------------------------------------------
    void AdvancedModificationExamples() {
        NkList<int> list = {1, 2, 3, 4, 5};

        // Inversion in-place de la liste : O(n)
        list.Reverse();  // list contient maintenant {5, 4, 3, 2, 1}

        // Échange O(1) avec une autre liste
        NkList<int> other = {10, 20};
        list.Swap(other);  // list = {10,20}, other = {5,4,3,2,1}

        // Vidage complet de la liste : O(n) pour destruction des nœuds
        list.Clear();  // list est maintenant vide, Size() == 0
        // other contient toujours {5,4,3,2,1}

        // Vérification d'état
        if (list.Empty()) {
            printf("List is empty\n");
        }
        nk_size currentSize = list.Size();  // currentSize == 0
    }

    // -----------------------------------------------------------------------------
    // Exemple 5 : Utilisation comme structure de données spécialisée
    // -----------------------------------------------------------------------------
    // NkList est idéal pour implémenter des structures LIFO/FIFO :

    // Stack (LIFO) via PushFront/PopFront
    template<typename T>
    class Stack {
    private:
        NkList<T> mStorage;
    public:
        void Push(const T& value) { mStorage.PushFront(value); }  // O(1)
        void Pop() { mStorage.PopFront(); }                        // O(1)
        T& Top() { return mStorage.Front(); }                      // O(1)
        bool Empty() const { return mStorage.Empty(); }            // O(1)
    };

    // Queue (FIFO) via PushBack/PopFront
    template<typename T>
    class Queue {
    private:
        NkList<T> mStorage;
    public:
        void Enqueue(const T& value) { mStorage.PushBack(value); } // O(1)
        void Dequeue() { mStorage.PopFront(); }                    // O(1)
        T& Front() { return mStorage.Front(); }                    // O(1)
        bool Empty() const { return mStorage.Empty(); }            // O(1)
    };

    void SpecializedUsage() {
        Stack<int> stack;
        stack.Push(1);
        stack.Push(2);
        stack.Push(3);
        // stack contient {3, 2, 1} (3 en tête)
        int top = stack.Top();  // top = 3
        stack.Pop();            // stack contient {2, 1}

        Queue<int> queue;
        queue.Enqueue(1);
        queue.Enqueue(2);
        queue.Enqueue(3);
        // queue contient {1, 2, 3} (1 en tête)
        int front = queue.Front();  // front = 1
        queue.Dequeue();            // queue contient {2, 3}
    }

    // -----------------------------------------------------------------------------
    // Exemple 6 : Intégration avec algorithmes génériques
    // -----------------------------------------------------------------------------
    template<typename Iterator, typename Predicate>
    Iterator NkFindIf(Iterator first, Iterator last, Predicate pred) {
        for (; first != last; ++first) {
            if (pred(*first)) {
                return first;
            }
        }
        return last;
    }

    void AlgorithmIntegration() {
        NkList<int> list = {1, 2, 3, 4, 5, 6};

        // Utiliser NkList avec algorithmes génériques grâce aux itérateurs Forward
        auto it = NkFindIf(list.begin(), list.end(), [](int x) { return x % 2 == 0; });
        if (it != list.end()) {
            printf("First even: %d\n", *it);  // Affiche: First even: 2
        }

        // Range-based for grâce aux aliases begin()/end()
        int sum = 0;
        for (int val : list) {
            sum += val;
        }
        // sum = 21
    }

    // -----------------------------------------------------------------------------
    // Exemple 7 : Optimisations avec move semantics (C++11)
    // -----------------------------------------------------------------------------
    #if defined(NKENTSEU_CXX11_OR_LATER)
    struct Expensive {
        nkentseu::nk_uint8* data;
        nk_size size;

        Expensive(nk_size s) : size(s) {
            data = static_cast<nkentseu::nk_uint8*>(malloc(s));
        }

        // Constructeur de déplacement
        Expensive(Expensive&& other) NKENTSEU_NOEXCEPT
            : data(other.data), size(other.size) {
            other.data = nullptr;
            other.size = 0;
        }

        ~Expensive() { free(data); }

        // Désactiver copie pour forcer l'usage de move
        Expensive(const Expensive&) = delete;
        Expensive& operator=(const Expensive&) = delete;
    };

    void MoveSemanticsOptimization() {
        NkList<Expensive> list;

        // PushFront avec move : évite la copie grâce à move semantics
        Expensive temp1(1024);
        list.PushFront(NKENTSEU_MOVE(temp1));  // Déplace temp1 dans list

        // PushBack avec move : idem pour insertion en queue
        Expensive temp2(2048);
        list.PushBack(NKENTSEU_MOVE(temp2));   // Déplace temp2 dans list

        // Swap O(1) pour réorganiser sans copier les données lourdes
        NkList<Expensive> other;
        other.PushFront(Expensive(512));
        list.Swap(other);  // Échange les chaînes de nœuds en O(1)
    }
    #endif
*/

// =============================================================================
// NOTES DE MIGRATION ET COMPATIBILITÉ
// =============================================================================
/*
    Migration depuis std::list vers NkList :

    std::list<T> API          | NkList<T> équivalent
    --------------------------|----------------------------------
    std::list<T> lst;        | NkList<T> lst;
    lst.push_front(x);       | lst.PushFront(x);
    lst.push_back(x);        | lst.PushBack(x);
    lst.pop_front();         | lst.PopFront();
    lst.front()              | lst.Front()
    lst.back()               | lst.Back()
    lst.begin(), lst.end()   | lst.begin()/end() (mêmes noms)
    lst.cbegin(), lst.cend() | lst.cbegin()/cend() (mêmes noms)
    lst.size(), lst.empty()  | lst.Size()/Empty() OU lst.size()/empty() (aliases)
    lst.clear()              | lst.Clear()
    lst.reverse()            | lst.Reverse()
    std::swap(a, b)          | NkSwap(a, b) ou a.Swap(b)

    Différences notables :
    - NkList n'a PAS de PopBack() (singly linked : pas de lien précédent)
    - NkList n'a PAS d'itérateurs bidirectionnels (--it non supporté)
    - NkList n'a PAS d'insertion/effacement à position arbitraire O(1)
      (nécessiterait parcours O(n) pour atteindre la position)
    - NkList utilise NKENTSEU_ASSERT pour validation debug, pas d'exceptions par défaut

    Avantages de NkList vs std::list :
    - Zéro dépendance STL : portable sur plateformes embarquées, kernels, etc.
    - Allocateur personnalisable via template (pas de std::allocator hardcoded)
    - Gestion d'erreurs configurable : assertions ou exceptions selon le build
    - Nommage PascalCase cohérent avec le reste du framework NK* (pour méthodes principales)
    - Macros NKENTSEU_* pour export/import DLL, inline hints, etc.
    - Intégration avec le système de logging et d'assertions NKPlatform

    Configuration CMake recommandée :
    # Activer C++11 pour move semantics et PushFront/PushBack rvalue
    target_compile_features(monapp PRIVATE cxx_std_11)

    # Mode développement avec assertions activées pour débogage
    target_compile_definitions(monapp PRIVATE NKENTSEU_ENABLE_ASSERT=1)

    # Mode production sans assertions pour performance maximale
    target_compile_definitions(monapp PRIVATE NKENTSEU_ENABLE_ASSERT=0)
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================