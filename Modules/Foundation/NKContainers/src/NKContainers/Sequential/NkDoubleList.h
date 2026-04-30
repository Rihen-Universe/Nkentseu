// =============================================================================
// NKContainers/Sequential/NkDoubleList.h
// Liste doublement chaînée — conteneur séquentiel 100% indépendant de la STL.
//
// Design :
//  - Réutilisation DIRECTE des macros NKCore/NKPlatform (ZÉRO duplication)
//  - Gestion mémoire via NKMemory::NkAllocator pour flexibilité d'allocation
//  - Itérateurs Bidirectional pour parcours avant/arrière O(1)
//  - Insertion/Suppression à toute position en O(1) via liens doubles
//  - Gestion d'erreurs unifiée via NKENTSEU_CONTAINERS_THROW_* macros
//  - Indentation stricte : visibilité, blocs conditionnels, namespaces tous indentés
//  - Une instruction par ligne pour lisibilité et maintenance
//  - Compatibilité multiplateforme via NKPlatform
//
// Auteur : Rihen
// Date : 2024-2026
// License : Propriétaire - Free to use and modify
//
// Historique des versions :
//   v1.0.0 (2026-02-09) :
//     - Implémentation initiale de NkDoubleList avec itérateurs Bidirectional
//     - Support C++11 move semantics pour PushFront/PushBack/EmplaceFront/EmplaceBack
//     - Méthodes Insert/Erase à position arbitraire en O(1)
//     - Reverse() pour inversion in-place de la liste
// =============================================================================

#pragma once

#ifndef NKENTSEU_CONTAINERS_SEQUENTIAL_NKDOUBLELIST_H_INCLUDED
#define NKENTSEU_CONTAINERS_SEQUENTIAL_NKDOUBLELIST_H_INCLUDED

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // NkDoubleList dépend des modules suivants :
    //  - NKCore/NkTypes.h : Types fondamentaux (nk_size, nk_bool, etc.)
    //  - NKContainers/NkContainersApi.h : Macros d'export pour visibilité des symboles
    //  - NKCore/NkTraits.h : Utilitaires de métaprogrammation (NkMove, NkForward, etc.)
    //  - NKMemory/NkAllocator.h : Interface d'allocateur pour gestion mémoire flexible
    //  - NKMemory/NkFunction.h : Fonctions mémoire bas niveau (NkCopy, etc.)
    //  - NKCore/Assert/NkAssert.h : Système d'assertions pour validation debug
    //  - NKContainers/Iterators/NkIterator.h : Tags d'itérateurs (NkBidirectionalIteratorTag)
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
        // SECTION 3 : CLASSE TEMPLATE NKDOUBLELIST
        // ====================================================================
        /**
         * @brief Liste doublement chaînée — conteneur séquentiel à insertion/suppression O(1) partout.
         * @tparam T Type des éléments stockés dans la liste
         * @tparam Allocator Type de l'allocateur utilisé pour la gestion mémoire des nœuds
         * @ingroup SequentialContainers
         *
         * NkDoubleList implémente une liste doublement chaînée (doubly linked list),
         * offrant des performances optimales pour les opérations de modification :
         *
         *  - PushFront/PushBack : O(1) constant — insertion aux extrémités
         *  - PopFront/PopBack : O(1) constant — suppression aux extrémités
         *  - Insert à position : O(1) — grâce aux liens prev/next bidirectionnels
         *  - Erase à position : O(1) — réarrangement des pointeurs uniquement
         *  - Accès par index : O(n) — parcours séquentiel requis (pas d'accès aléatoire)
         *  - Recherche : O(n) — parcours linéaire nécessaire
         *
         * @note Avantages vs NkVector :
         *       - Insertions/suppressions à toute position sans décalage d'éléments
         *       - Itérateurs non invalidés par insertions/suppressions (sauf élément supprimé)
         *       - Support des itérateurs bidirectionnels (--it possible)
         *       - Mémoire fragmentée mais allocation par nœud (utile pour objets lourds)
         *
         * @note Avantages vs NkList (singly linked) :
         *       - Parcours inversé natif via ReverseIterator ou operator--
         *       - Suppression en queue O(1) via PopBack() (impossible en O(1) avec singly linked)
         *       - Insertion avant une position connue O(1) grâce au lien prev
         *
         * @note Inconvénients vs NkVector :
         *       - Pas d'accès aléatoire O(1) — parcours séquentiel obligatoire
         *       - Surcoût mémoire par nœud (deux pointeurs + alignment)
         *       - Moins cache-friendly (nœuds potentiellement dispersés en mémoire)
         *
         * @example Utilisation basique
         * @code
         * using namespace nkentseu::containers;
         *
         * // Construction avec liste d'initialisation
         * NkDoubleList<int> values = {1, 2, 3, 4, 5};
         *
         * // Insertion en tête : O(1)
         * values.PushFront(0);  // values = {0, 1, 2, 3, 4, 5}
         *
         * // Insertion en queue : O(1)
         * values.PushBack(6);   // values = {0, 1, 2, 3, 4, 5, 6}
         *
         * // Parcours avant avec itérateur bidirectionnel
         * for (auto it = values.begin(); it != values.end(); ++it) {
         *     printf("%d ", *it);  // Affiche: 0 1 2 3 4 5 6
         * }
         *
         * // Parcours arrière avec reverse iterator
         * for (auto rit = values.rbegin(); rit != values.rend(); ++rit) {
         *     printf("%d ", *rit);  // Affiche: 6 5 4 3 2 1 0
         * }
         *
         * // Insertion à position arbitraire : O(1) si itérateur déjà obtenu
         * auto it = values.begin();
         * ++it; ++it;  // Position index 2
         * values.Insert(it, 99);  // Insère 99 avant l'élément à index 2
         *
         * // Suppression à position : O(1)
         * values.Erase(it);  // Supprime l'élément à la position courante
         *
         * // Inversion de la liste : O(n)
         * values.Reverse();  // values = {6, 5, 4, 3, 2, 1, 0} (ordre inversé)
         * @endcode
         */
        template<typename T, typename Allocator = memory::NkAllocator>
        class NKENTSEU_CONTAINERS_CLASS_EXPORT NkDoubleList {
            private:

                // -----------------------------------------------------------------
                // Sous-section : Structure interne Node (privée)
                // -----------------------------------------------------------------
                /**
                 * @brief Nœud interne de la liste doublement chaînée
                 * @struct Node
                 * @ingroup DoubleListInternals
                 *
                 * Structure privée contenant :
                 *  - value : l'élément stocké de type T
                 *  - prev : pointeur vers le nœud précédent (nullptr pour premier)
                 *  - next : pointeur vers le nœud suivant (nullptr pour dernier)
                 *
                 * @note Allocation via l'allocateur de la liste, pas new/delete standard
                 * @note Constructeurs conditionnels selon disponibilité C++11
                 * @note Les pointeurs prev/next permettent le parcours bidirectionnel O(1)
                 */
                struct Node {
                    T value;        ///< Élément stocké dans ce nœud
                    Node* prev;     ///< Pointeur vers le nœud précédent (nullptr si premier)
                    Node* next;     ///< Pointeur vers le nœud suivant (nullptr si dernier)

                    /**
                     * @brief Constructeur par copie pour compatibilité pré-C++11
                     * @param val Valeur à copier dans le nœud
                     *
                     * @note Initialise prev et next à nullptr — liaison gérée par la liste
                     * @note Requiert que T soit CopyConstructible
                     */
                    Node(const T& val)
                        : value(val)
                        , prev(nullptr)
                        , next(nullptr) {
                    }

                    /**
                     * @brief Constructeur C++11 avec forwarding parfait
                     * @tparam Args Types des arguments de construction (déduits)
                     * @param args Arguments forwardés au constructeur de T
                     *
                     * @note Utilise placement new pour construction in-place dans le buffer alloué
                     * @note Forwarding parfait préserve les catégories de valeur (lvalue/rvalue)
                     * @note Idéal pour éviter les copies temporaires des types lourds
                     */
                    template<typename... Args>
                    Node(Args&&... args)
                        : value(traits::NkForward<Args>(args)...)
                        , prev(nullptr)
                        , next(nullptr) {
                    }
                };

            public:

                // -----------------------------------------------------------------
                // Sous-section : Itérateur mutable (Bidirectional)
                // -----------------------------------------------------------------
                /**
                 * @brief Itérateur mutable pour parcours bidirectionnel de la liste
                 * @class Iterator
                 * @ingroup DoubleListIterators
                 *
                 * Itérateur Bidirectional : supporte ++ et -- pour avancer/reculer,
                 * ainsi que la déréférenciation * et ->. Catégorie : NkBidirectionalIteratorTag.
                 *
                 * @note Implémentation légère : simple wrapper autour de Node*
                 * @note Compatible avec les algorithmes génériques attendant BidirectionalIterator
                 * @note Les itérateurs restent valides après insertions/suppressions (sauf élément supprimé)
                 */
                class Iterator {
                    private:

                        /** @brief Pointeur vers le nœud courant (privé) */
                        Node* mNode;

                        /** @brief NkDoubleList est ami pour accéder à mNode */
                        friend class NkDoubleList;

                    public:

                        /** @brief Type pour les différences entre itérateurs */
                        using DifferenceType = nkentseu::ptrdiff;

                        /** @brief Type des valeurs pointées par l'itérateur */
                        using ValueType = T;

                        /** @brief Type de pointeur mutable retourné par operator-> */
                        using Pointer = T*;

                        /** @brief Type de référence mutable retourné par operator* */
                        using Reference = T&;

                        /** @brief Catégorie d'itérateur : Bidirectional */
                        using IteratorCategory = NkBidirectionalIteratorTag;

                        /**
                         * @brief Constructeur par défaut — itérateur nul (end sentinel)
                         * @ingroup DoubleListIterators
                         */
                        Iterator()
                            : mNode(nullptr) {
                        }

                        /**
                         * @brief Constructeur explicite depuis un pointeur de nœud
                         * @param node Pointeur vers le nœud à encapsuler
                         * @ingroup DoubleListIterators
                         */
                        explicit Iterator(Node* node)
                            : mNode(node) {
                        }

                        /**
                         * @brief Déréférencement — accès à l'élément courant
                         * @return Référence mutable vers value du nœud courant
                         * @ingroup DoubleListIterators
                         *
                         * @note Assertion en debug si mNode == nullptr (déréférencement invalide)
                         * @note Comportement indéfini en release si itérateur end() est déréférencé
                         * @note L'utilisateur doit vérifier it != end() avant déréférencement
                         */
                        Reference operator*() const {
                            NKENTSEU_ASSERT(mNode != nullptr);
                            return mNode->value;
                        }

                        /**
                         * @brief Accès membre via pointeur — équivalent à &(operator*())
                         * @return Pointeur mutable vers value du nœud courant
                         * @ingroup DoubleListIterators
                         *
                         * @note Utile pour accéder aux membres de T via it->member
                         * @note Assertion en debug si mNode == nullptr
                         * @note Même avertissement : vérifier validité avant usage
                         */
                        Pointer operator->() const {
                            NKENTSEU_ASSERT(mNode != nullptr);
                            return &(mNode->value);
                        }

                        /**
                         * @brief Incrémentation préfixe — avance au nœud suivant
                         * @return Référence vers *this après avancement
                         * @ingroup DoubleListIterators
                         *
                         * @note Complexité : O(1) — simple affectation via lien next
                         * @note Assertion en debug si mNode == nullptr avant avancement
                         * @note Comportement indéfini en release si appelé sur end()
                         *
                         * @example
                         * @code
                         * auto it = list.begin();
                         * ++it;  // Avance au deuxième élément via mNode = mNode->next
                         * @endcode
                         */
                        Iterator& operator++() {
                            NKENTSEU_ASSERT(mNode != nullptr);
                            mNode = mNode->next;
                            return *this;
                        }

                        /**
                         * @brief Incrémentation postfixe — avance et retourne l'ancienne position
                         * @return Copie de l'itérateur avant avancement
                         * @ingroup DoubleListIterators
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
                         * @brief Décrémentation préfixe — recule au nœud précédent
                         * @return Référence vers *this après recul
                         * @ingroup DoubleListIterators
                         *
                         * @note Complexité : O(1) — simple affectation via lien prev (bidirectional)
                         * @note Assertion en debug si mNode == nullptr avant recul
                         * @note Avantage clé vs NkList (singly) : permet parcours arrière natif
                         *
                         * @example
                         * @code
                         * auto it = list.end();
                         * --it;  // Recule au dernier élément via mNode = mNode->prev
                         * @endcode
                         */
                        Iterator& operator--() {
                            NKENTSEU_ASSERT(mNode != nullptr);
                            mNode = mNode->prev;
                            return *this;
                        }

                        /**
                         * @brief Décrémentation postfixe — recule et retourne l'ancienne position
                         * @return Copie de l'itérateur avant recul
                         * @ingroup DoubleListIterators
                         *
                         * @note Complexité : O(1) avec copie temporaire
                         * @note Préférer --it (préfixe) pour performance
                         */
                        Iterator operator--(int) {
                            Iterator tmp = *this;
                            --(*this);
                            return tmp;
                        }

                        /**
                         * @brief Comparaison d'égalité entre deux itérateurs
                         * @param other Autre itérateur à comparer
                         * @return true si les deux itérateurs pointent sur le même nœud
                         * @ingroup DoubleListIterators
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
                         * @ingroup DoubleListIterators
                         *
                         * @note Implémentation via négation de operator== pour cohérence
                         * @note Utilisé dans les boucles : for (; it != end(); ++it)
                         */
                        bool operator!=(const Iterator& other) const {
                            return mNode != other.mNode;
                        }
                };

                // -----------------------------------------------------------------
                // Sous-section : Itérateur constant (Bidirectional)
                // -----------------------------------------------------------------
                /**
                 * @brief Itérateur constant pour parcours bidirectionnel en lecture seule
                 * @class ConstIterator
                 * @ingroup DoubleListIterators
                 *
                 * Version const de Iterator : retourne const T& via operator*,
                 * empêchant toute modification des éléments via l'itérateur.
                 *
                 * @note Catégorie : NkBidirectionalIteratorTag (supporte ++ et --)
                 * @note Conversion implicite depuis Iterator pour compatibilité
                 * @note Idéal pour les fonctions prenant const NkDoubleList& en paramètre
                 */
                class ConstIterator {
                    private:

                        /** @brief Pointeur constant vers le nœud courant (privé) */
                        const Node* mNode;

                        /** @brief NkDoubleList est ami pour accéder à mNode */
                        friend class NkDoubleList;

                    public:

                        /** @brief Type pour les différences entre itérateurs */
                        using DifferenceType = nkentseu::ptrdiff;

                        /** @brief Type des valeurs pointées par l'itérateur */
                        using ValueType = T;

                        /** @brief Type de pointeur constant retourné par operator-> */
                        using Pointer = const T*;

                        /** @brief Type de référence constante retourné par operator* */
                        using Reference = const T&;

                        /** @brief Catégorie d'itérateur : Bidirectional */
                        using IteratorCategory = NkBidirectionalIteratorTag;

                        /**
                         * @brief Constructeur par défaut — itérateur nul (end sentinel)
                         * @ingroup DoubleListIterators
                         */
                        ConstIterator()
                            : mNode(nullptr) {
                        }

                        /**
                         * @brief Constructeur explicite depuis un pointeur de nœud constant
                         * @param node Pointeur constant vers le nœud à encapsuler
                         * @ingroup DoubleListIterators
                         */
                        explicit ConstIterator(const Node* node)
                            : mNode(node) {
                        }

                        /**
                         * @brief Constructeur de conversion depuis Iterator mutable
                         * @param it Itérateur mutable à convertir en const
                         * @ingroup DoubleListIterators
                         *
                         * @note Permet l'assignation : ConstIterator cit = mutableIt;
                         * @note Conversion sûre : const-correctness garantie par le type
                         */
                        ConstIterator(const Iterator& it)
                            : mNode(it.mNode) {
                        }

                        /**
                         * @brief Déréférencement — accès en lecture seule à l'élément courant
                         * @return Référence constante vers value du nœud courant
                         * @ingroup DoubleListIterators
                         *
                         * @note Assertion en debug si mNode == nullptr
                         * @note Retourne const T& : impossible de modifier via cet itérateur
                         */
                        Reference operator*() const {
                            NKENTSEU_ASSERT(mNode != nullptr);
                            return mNode->value;
                        }

                        /**
                         * @brief Accès membre via pointeur constant — lecture seule
                         * @return Pointeur constant vers value du nœud courant
                         * @ingroup DoubleListIterators
                         *
                         * @note Utile pour it->member en contexte const
                         * @note Assertion en debug si mNode == nullptr
                         */
                        Pointer operator->() const {
                            NKENTSEU_ASSERT(mNode != nullptr);
                            return &(mNode->value);
                        }

                        /**
                         * @brief Incrémentation préfixe — avance au nœud suivant
                         * @return Référence vers *this après avancement
                         * @ingroup DoubleListIterators
                         *
                         * @note Complexité : O(1) — simple affectation via lien next
                         * @note Assertion en debug si mNode == nullptr avant avancement
                         */
                        ConstIterator& operator++() {
                            NKENTSEU_ASSERT(mNode != nullptr);
                            mNode = mNode->next;
                            return *this;
                        }

                        /**
                         * @brief Incrémentation postfixe — avance et retourne l'ancienne position
                         * @return Copie de l'itérateur avant avancement
                         * @ingroup DoubleListIterators
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
                         * @brief Décrémentation préfixe — recule au nœud précédent
                         * @return Référence vers *this après recul
                         * @ingroup DoubleListIterators
                         *
                         * @note Complexité : O(1) — simple affectation via lien prev
                         * @note Assertion en debug si mNode == nullptr avant recul
                         * @note Avantage clé : permet parcours arrière sur const containers
                         */
                        ConstIterator& operator--() {
                            NKENTSEU_ASSERT(mNode != nullptr);
                            mNode = mNode->prev;
                            return *this;
                        }

                        /**
                         * @brief Décrémentation postfixe — recule et retourne l'ancienne position
                         * @return Copie de l'itérateur avant recul
                         * @ingroup DoubleListIterators
                         *
                         * @note Complexité : O(1) avec copie temporaire
                         * @note Préférer --it (préfixe) pour performance
                         */
                        ConstIterator operator--(int) {
                            ConstIterator tmp = *this;
                            --(*this);
                            return tmp;
                        }

                        /**
                         * @brief Comparaison d'égalité entre deux itérateurs constants
                         * @param other Autre itérateur constant à comparer
                         * @return true si les deux pointent sur le même nœud
                         * @ingroup DoubleListIterators
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
                         * @ingroup DoubleListIterators
                         *
                         * @note Implémentation via négation de operator==
                         * @note Standard pour les boucles const : for (; it != cend(); ++it)
                         */
                        bool operator!=(const ConstIterator& other) const {
                            return mNode != other.mNode;
                        }
                };

                // -----------------------------------------------------------------
                // Sous-section : Types membres publics
                // -----------------------------------------------------------------
                /**
                 * @brief Type des valeurs stockées dans le conteneur
                 * @ingroup DoubleListTypes
                 */
                using ValueType = T;

                /**
                 * @brief Type de l'allocateur utilisé pour la gestion mémoire
                 * @ingroup DoubleListTypes
                 */
                using AllocatorType = Allocator;

                /**
                 * @brief Type utilisé pour représenter la taille et les index
                 * @ingroup DoubleListTypes
                 * @note Alias vers nk_size pour cohérence avec le reste du framework
                 */
                using SizeType = nk_size;

                /**
                 * @brief Type de référence mutable aux éléments
                 * @ingroup DoubleListTypes
                 */
                using Reference = T&;

                /**
                 * @brief Type de référence constante aux éléments
                 * @ingroup DoubleListTypes
                 */
                using ConstReference = const T&;

                /**
                 * @brief Type d'itérateur inversé mutable
                 * @ingroup DoubleListTypes
                 * @note Adaptateur NkReverseIterator pour parcours en sens inverse
                 */
                using ReverseIterator = NkReverseIterator<Iterator>;

                /**
                 * @brief Type d'itérateur inversé constant
                 * @ingroup DoubleListTypes
                 */
                using ConstReverseIterator = NkReverseIterator<ConstIterator>;

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
                 * @note nullptr si la liste est vide ; maintenu pour PushBack/PopBack O(1)
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
                 * @brief Alloue et construit un nouveau nœud par copie
                 * @param value Valeur à copier dans le nouveau nœud
                 * @return Pointeur vers le nouveau nœud alloué, ou nullptr si échec
                 * @ingroup DoubleListInternals
                 *
                 * Étapes :
                 *  1. Alloue la mémoire pour Node via mAllocator->Allocate()
                 *  2. Si allocation réussie : placement new pour construire Node in-place
                 *  3. Retourne le pointeur vers le nœud construit
                 *
                 * @note Gestion d'erreur : retourne nullptr si Allocate() échoue
                 * @note Complexité : O(1) pour l'allocation + construction
                 * @note Requiert que T soit CopyConstructible
                 */
                Node* AllocateNode(const T& value) {
                    Node* node = static_cast<Node*>(
                        mAllocator->Allocate(sizeof(Node))
                    );
                    if (node) {
                        new (node) Node(value);
                    }
                    return node;
                }

                /**
                 * @brief Alloue et construit un nouveau nœud avec forwarding parfait
                 * @tparam Args Types des arguments de construction (déduits)
                 * @param args Arguments forwardés au constructeur de Node
                 * @return Pointeur vers le nouveau nœud alloué, ou nullptr si échec
                 * @ingroup DoubleListInternals
                 *
                 * Version C++11 de AllocateNode utilisant traits::NkForward
                 * pour préserver les catégories de valeur et éviter les copies.
                 *
                 * @note Utilise placement new pour construction in-place
                 * @note Gestion d'erreur : retourne nullptr si Allocate() échoue
                 * @note Idéal pour les types lourds ou non copiables
                 */
                template<typename... Args>
                Node* AllocateNode(Args&&... args) {
                    Node* node = static_cast<Node*>(
                        mAllocator->Allocate(sizeof(Node))
                    );
                    if (node) {
                        new (node) Node(traits::NkForward<Args>(args)...);
                    }
                    return node;
                }

                /**
                 * @brief Détruit explicitement un nœud et libère sa mémoire
                 * @param node Pointeur vers le nœud à détruire
                 * @ingroup DoubleListInternals
                 *
                 * Étapes de destruction :
                 *  1. Appelle explicitement le destructeur de Node (et donc de T)
                 *  2. Libère la mémoire via mAllocator->Deallocate()
                 *
                 * @note noexcept implicite : ne lève pas d'exception (destructor safety)
                 * @note Complexité : O(1) — destruction d'un seul nœud
                 * @note Ne modifie pas mHead/mTail/mSize : responsabilité du caller
                 */
                void DeallocateNode(Node* node) {
                    if (node) {
                        node->~Node();
                        mAllocator->Deallocate(node);
                    }
                }

            public:

                // -----------------------------------------------------------------
                // Sous-section : Constructeurs
                // -----------------------------------------------------------------
                /**
                 * @brief Constructeur par défaut — liste vide sans allocation
                 * @ingroup DoubleListConstructors
                 *
                 * Initialise une liste avec :
                 *  - mHead = nullptr, mTail = nullptr (état vide)
                 *  - mSize = 0 (aucun élément)
                 *  - mAllocator = allocateur global par défaut
                 *
                 * @note Complexité : O(1) — aucune allocation mémoire initiale
                 * @note Premier PushFront/PushBack déclenchera l'allocation du premier nœud
                 *
                 * @example
                 * @code
                 * NkDoubleList<int> empty;  // Liste vide, aucun nœud alloué
                 * empty.PushBack(42);       // Alloue et insère le premier nœud
                 * @endcode
                 */
                NkDoubleList()
                    : mHead(nullptr)
                    , mTail(nullptr)
                    , mSize(0)
                    , mAllocator(&memory::NkGetDefaultAllocator()) {
                }

                /**
                 * @brief Constructeur avec allocateur personnalisé
                 * @param allocator Pointeur vers l'allocateur à utiliser (nullptr pour défaut)
                 * @ingroup DoubleListConstructors
                 *
                 * Permet d'utiliser un allocateur spécifique pour cette liste,
                 * utile pour :
                 *  - Pool d'allocations pour performance prédictible
                 *  - Mémoire alignée pour SIMD/GPU
                 *  - Tracking mémoire pour debugging/profiling
                 *
                 * @note Si allocator == nullptr, utilise l'allocateur global par défaut
                 * @note Complexité : O(1) — aucune allocation de données
                 */
                explicit NkDoubleList(Allocator* allocator)
                    : mHead(nullptr)
                    , mTail(nullptr)
                    , mSize(0)
                    , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator()) {
                }

                /**
                 * @brief Constructeur depuis NkInitializerList — syntaxe {a, b, c}
                 * @param init Liste d'initialisation contenant les éléments à insérer
                 * @ingroup DoubleListConstructors
                 *
                 * Permet la construction avec liste d'initialisation :
                 *   NkDoubleList<int> values = {1, 2, 3, 4};
                 *
                 * Insère chaque élément via PushBack, préservant l'ordre de la liste.
                 *
                 * @note Complexité : O(n) pour n insertions PushBack O(1) chacune
                 * @note Compatible avec C++11 initializer list syntax
                 *
                 * @example
                 * @code
                 * auto coords = NkDoubleList<float>{1.0f, 2.0f, 3.0f};
                 * auto names = NkDoubleList<const char*>{"Alice", "Bob", "Charlie"};
                 * @endcode
                 */
                NkDoubleList(NkInitializerList<T> init)
                    : mHead(nullptr)
                    , mTail(nullptr)
                    , mSize(0)
                    , mAllocator(&memory::NkGetDefaultAllocator()) {
                    for (auto& val : init) {
                        PushBack(val);
                    }
                }

                /**
                 * @brief Constructeur depuis std::initializer_list — interop STL
                 * @param init Liste d'initialisation STL standard
                 * @ingroup DoubleListConstructors
                 *
                 * Facilité d'interopérabilité avec du code utilisant std::initializer_list.
                 * Fonctionne comme le constructeur NkInitializerList mais accepte
                 * le type standard de la STL pour une migration progressive.
                 *
                 * @note Complexité : O(n) pour les insertions successives
                 * @note Nécessite <initializer_list> du compilateur (pas de dépendance STL runtime)
                 */
                NkDoubleList(std::initializer_list<T> init)
                    : mHead(nullptr)
                    , mTail(nullptr)
                    , mSize(0)
                    , mAllocator(&memory::NkGetDefaultAllocator()) {
                    for (auto& val : init) {
                        PushBack(val);
                    }
                }

                /**
                 * @brief Constructeur de copie — duplication profonde d'une liste
                 * @param other Liste source à copier
                 * @ingroup DoubleListConstructors
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
                 * NkDoubleList<int> original = {1, 2, 3};
                 * NkDoubleList<int> copy = original;  // Copie indépendante
                 * copy.PushBack(4);                   // original reste {1,2,3}
                 * @endcode
                 */
                NkDoubleList(const NkDoubleList& other)
                    : mHead(nullptr)
                    , mTail(nullptr)
                    , mSize(0)
                    , mAllocator(other.mAllocator) {
                    for (auto& val : other) {
                        PushBack(val);
                    }
                }

                /**
                 * @brief Constructeur de déplacement — transfert de propriété efficace
                 * @param other Liste source dont transférer le contenu
                 * @ingroup DoubleListConstructors
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
                 * NkDoubleList<int> source = MakeLargeList();
                 * NkDoubleList<int> dest = NKENTSEU_MOVE(source);  // Transfert O(1)
                 * // source est maintenant vide, dest contient les nœuds
                 * @endcode
                 */
                NkDoubleList(NkDoubleList&& other) NKENTSEU_NOEXCEPT
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
                 * @ingroup DoubleListConstructors
                 *
                 * Libère toutes les ressources gérées par cette liste :
                 *  1. Parcours tous les nœuds de mHead à nullptr via les liens next
                 *  2. Pour chaque nœud : appelle DeallocateNode() (destructor + deallocate)
                 *  3. Réinitialise mHead, mTail, mSize à l'état vide
                 *
                 * @note noexcept implicite : ne lève jamais d'exception (destructor safety)
                 * @note Complexité : O(n) pour la destruction des n nœuds
                 * @note Garantit l'absence de fuites mémoire même en cas d'exception pendant construction
                 */
                ~NkDoubleList() {
                    Clear();
                }

                // -----------------------------------------------------------------
                // Sous-section : Opérateurs d'affectation
                // -----------------------------------------------------------------
                /**
                 * @brief Opérateur d'affectation par copie — syntaxe list1 = list2
                 * @param other Liste source à copier
                 * @return Référence vers *this pour chaînage d'opérations
                 * @ingroup DoubleListAssignment
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
                 * NkDoubleList<int> a = {1, 2}, b = {3, 4}, c;
                 * c = a;        // c contient {1, 2}
                 * c = b = a;    // Chaînage : c et b contiennent {1, 2}
                 * @endcode
                 */
                NkDoubleList& operator=(const NkDoubleList& other) {
                    if (this != &other) {
                        Clear();
                        for (auto& val : other) {
                            PushBack(val);
                        }
                    }
                    return *this;
                }

                /**
                 * @brief Opérateur d'affectation par déplacement — transfert efficace
                 * @param other Liste source dont transférer le contenu
                 * @return Référence vers *this pour chaînage d'opérations
                 * @ingroup DoubleListAssignment
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
                 * NkDoubleList<int> temp = ComputeExpensiveList();
                 * result = NKENTSEU_MOVE(temp);  // Transfert O(1), pas de copie
                 * // temp est maintenant vide, result contient les nœuds
                 * @endcode
                 */
                NkDoubleList& operator=(NkDoubleList&& other) NKENTSEU_NOEXCEPT {
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
                 * @ingroup DoubleListAssignment
                 *
                 * Permet la syntaxe list = {a, b, c} pour remplacer le contenu
                 * de la liste par une nouvelle liste d'éléments.
                 *
                 * @note Complexité : O(n) pour l'insertion des nouveaux éléments
                 * @note Vide d'abord le contenu actuel via Clear()
                 *
                 * @example
                 * @code
                 * NkDoubleList<int> list = {1, 2, 3};
                 * list = {4, 5, 6};  // list contient maintenant {4, 5, 6}
                 * @endcode
                 */
                NkDoubleList& operator=(NkInitializerList<T> init) {
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
                 * @ingroup DoubleListAssignment
                 *
                 * Interopérabilité avec code utilisant std::initializer_list.
                 * Fonctionne comme l'opérateur NkInitializerList mais accepte
                 * le type standard pour migration progressive depuis la STL.
                 *
                 * @note Complexité : O(n) pour l'insertion des éléments
                 * @note Nécessite <initializer_list> du compilateur uniquement
                 */
                NkDoubleList& operator=(std::initializer_list<T> init) {
                    Clear();
                    for (auto& val : init) {
                        PushBack(val);
                    }
                    return *this;
                }

                // -----------------------------------------------------------------
                // Sous-section : Itérateurs — Style minuscule (compatibilité STL)
                // -----------------------------------------------------------------
                /**
                 * @brief Itérateur mutable vers le début de la liste
                 * @return Iterator pointant sur le premier nœud (ou end() si vide)
                 * @ingroup DoubleListIterators
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
                 * @ingroup DoubleListIterators
                 *
                 * Version const de begin() pour les listes en lecture seule.
                 * Retourne un ConstIterator pour empêcher la modification via l'itérateur.
                 *
                 * @note Complexité : O(1) — construction d'itérateur léger
                 * @note noexcept implicite : ne lève pas d'exception
                 * @note Permet de parcourir un const NkDoubleList& sans lever la const
                 *
                 * @example
                 * @code
                 * void PrintAll(const NkDoubleList<int>& list) {
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
                 * @ingroup DoubleListIterators
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
                 * @ingroup DoubleListIterators
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
                 * @ingroup DoubleListIterators
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
                 * @ingroup DoubleListIterators
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

                /**
                 * @brief Itérateur inversé mutable vers le dernier élément
                 * @return ReverseIterator pointant sur le dernier nœud (ou rend() si vide)
                 * @ingroup DoubleListIterators
                 *
                 * Retourne un adaptateur NkReverseIterator initialisé avec end(),
                 * permettant de parcourir la liste en sens inverse. L'opération
                 * ++ sur un ReverseIterator avance vers le début de la liste.
                 *
                 * @note Complexité : O(1) — construction d'adaptateur léger
                 * @note noexcept implicite : ne lève pas d'exception
                 * @note Nécessite que l'itérateur sous-jacent supporte l'opération --
                 *
                 * @example
                 * @code
                 * // Parcours à l'envers : dernier -> premier
                 * for (auto rit = list.rbegin(); rit != list.rend(); ++rit) {
                 *     printf("%d ", *rit);
                 * }
                 * @endcode
                 */
                ReverseIterator rbegin() {
                    return ReverseIterator(Iterator(nullptr));
                }

                /**
                 * @brief Itérateur inversé constant vers le dernier élément
                 * @return ConstReverseIterator pointant sur le dernier nœud
                 * @ingroup DoubleListIterators
                 *
                 * Version const de rbegin() pour les listes en lecture seule.
                 * Retourne un adaptateur const pour empêcher la modification
                 * des éléments via l'itérateur inversé.
                 *
                 * @note Complexité : O(1) — construction d'adaptateur léger
                 * @note noexcept implicite : ne lève pas d'exception
                 * @note Permet le parcours inversé d'un const NkDoubleList&
                 *
                 * @example
                 * @code
                 * void PrintReverse(const NkDoubleList<int>& list) {
                 *     for (auto rit = list.rbegin(); rit != list.rend(); ++rit) {
                 *         printf("%d ", *rit);  // Lecture seule en ordre inverse
                 *     }
                 * }
                 * @endcode
                 */
                ConstReverseIterator rbegin() const {
                    return ConstReverseIterator(ConstIterator(nullptr));
                }

                /**
                 * @brief Itérateur inversé constant explicite (compatibilité STL)
                 * @return ConstReverseIterator pointant sur le dernier élément
                 * @ingroup DoubleListIterators
                 *
                 * Alias vers rbegin() const pour respecter la convention STL
                 * où crbegin() retourne toujours un const_reverse_iterator.
                 *
                 * @note Complexité : O(1) — délégation vers rbegin() const
                 * @note noexcept implicite : ne lève pas d'exception
                 * @note Utile pour les templates génériques attendant crbegin()
                 */
                ConstReverseIterator crbegin() const {
                    return ConstReverseIterator(ConstIterator(nullptr));
                }

                /**
                 * @brief Itérateur inversé mutable vers avant le premier élément
                 * @return ReverseIterator sentinelle marquant la fin du parcours inversé
                 * @ingroup DoubleListIterators
                 *
                 * Retourne un adaptateur NkReverseIterator initialisé avec begin(),
                 * représentant la position "juste avant" le premier élément dans
                 * l'ordre inversé. Utilisé comme borne supérieure exclusive pour
                 * les boucles de parcours inversé.
                 *
                 * @note Complexité : O(1) — construction d'adaptateur léger
                 * @note noexcept implicite : ne lève pas d'exception
                 * @note Ne jamais déréférencer : c'est une sentinelle, pas un nœud valide
                 *
                 * @example
                 * @code
                 * // Boucle inversée avec sentinelle rend()
                 * for (auto rit = list.rbegin(); rit != list.rend(); ++rit) {
                 *     Process(*rit);  // Traite du dernier au premier
                 * }
                 * @endcode
                 */
                ReverseIterator rend() {
                    return ReverseIterator(Iterator(mHead));
                }

                /**
                 * @brief Itérateur inversé constant vers avant le premier élément
                 * @return ConstReverseIterator sentinelle pour parcours inversé const
                 * @ingroup DoubleListIterators
                 *
                 * Version const de rend() pour les listes en lecture seule.
                 * Même sémantique de sentinelle exclusive que la version mutable.
                 *
                 * @note Complexité : O(1) — construction d'adaptateur léger
                 * @note noexcept implicite : ne lève pas d'exception
                 * @note Compatible avec les algorithmes inversés sur const containers
                 */
                ConstReverseIterator rend() const {
                    return ConstReverseIterator(ConstIterator(mHead));
                }

                /**
                 * @brief Itérateur inversé constant explicite (compatibilité STL)
                 * @return ConstReverseIterator sentinelle pour parcours inversé const
                 * @ingroup DoubleListIterators
                 *
                 * Alias vers rend() const pour respecter la convention STL
                 * où crend() retourne toujours un const_reverse_iterator.
                 *
                 * @note Complexité : O(1) — délégation vers rend() const
                 * @note noexcept implicite : ne lève pas d'exception
                 * @note Utile pour les templates génériques attendant crend()
                 */
                ConstReverseIterator crend() const {
                    return ConstReverseIterator(ConstIterator(mHead));
                }

                // -----------------------------------------------------------------
                // Sous-section : Méthodes de capacité
                // -----------------------------------------------------------------
                /**
                 * @brief Vérifie si la liste est vide — style PascalCase
                 * @return true si Size() == 0, false sinon
                 * @ingroup DoubleListCapacity
                 *
                 * Méthode standard interne du framework (PascalCase) pour
                 * tester si la liste ne contient aucun élément.
                 *
                 * @note Complexité : O(1) — comparaison simple de membre
                 * @note noexcept : garantie de ne pas lever d'exception
                 * @note Préférer cette version dans le code applicatif NKContainers
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
                 * @brief Vérifie si la liste est vide — compatibilité STL empty()
                 * @return true si Size() == 0, false sinon
                 * @ingroup DoubleListCapacity
                 *
                 * Alias vers Empty() pour compatibilité avec la STL et les
                 * algorithmes génériques attendant la méthode empty().
                 *
                 * @note Complexité : O(1) — délégation inline
                 * @note noexcept : garantie de ne pas lever d'exception
                 * @note Essentiel pour les templates génériques STL-compatible
                 *
                 * @example
                 * @code
                 * template<typename Container>
                 * void ProcessIfNotEmpty(const Container& c) {
                 *     if (!c.empty()) {  // Fonctionne avec NkDoubleList grâce à cet alias
                 *         DoSomething(c);
                 *     }
                 * }
                 * @endcode
                 */
                bool empty() const NKENTSEU_NOEXCEPT {
                    return mSize == 0;
                }

                /**
                 * @brief Retourne le nombre d'éléments actuellement stockés
                 * @return Valeur de type SizeType représentant la taille courante
                 * @ingroup DoubleListCapacity
                 *
                 * Méthode standard interne (PascalCase) pour obtenir le nombre
                 * d'éléments valides dans la liste. Toujours exact grâce au
                 * compteur mSize maintenu à jour par toutes les opérations.
                 *
                 * @note Complexité : O(1) — retour de membre direct
                 * @note noexcept : garantie de ne pas lever d'exception
                 * @note Préférer cette version dans le code applicatif NKContainers
                 *
                 * @example
                 * @code
                 * for (SizeType i = 0; i < list.Size(); ++i) {
                 *     // Note : accès par index nécessite parcours O(n) pour liste chaînée
                 * }
                 * @endcode
                 */
                SizeType Size() const NKENTSEU_NOEXCEPT {
                    return mSize;
                }

                /**
                 * @brief Retourne le nombre d'éléments — alias de Size() compatibilité STL
                 * @return Valeur de type SizeType représentant la taille courante
                 * @ingroup DoubleListCapacity
                 *
                 * Alias vers Size() pour compatibilité avec la STL et les
                 * algorithmes génériques attendant la méthode size().
                 *
                 * @note Complexité : O(1) — délégation inline
                 * @note noexcept : garantie de ne pas lever d'exception
                 * @note Essentiel pour les templates génériques STL-compatible
                 */
                SizeType size() const NKENTSEU_NOEXCEPT {
                    return mSize;
                }

                // -----------------------------------------------------------------
                // Sous-section : Accès aux éléments (extrémités uniquement)
                // -----------------------------------------------------------------
                /**
                 * @brief Accès au premier élément — version mutable
                 * @return Référence mutable vers le value du premier nœud
                 * @ingroup DoubleListAccess
                 *
                 * Retourne une référence vers mHead->value avec assertion debug
                 * si la liste est vide. Équivalent à Front() = *begin()
                 * mais avec sémantique explicite "premier élément".
                 *
                 * @note Complexité : O(1) — accès direct via pointeur mHead
                 * @note Assertion en debug si Empty() == true
                 * @note Comportement indéfini en release si liste vide
                 *
                 * @example
                 * @code
                 * NkDoubleList<int> list = {10, 20, 30};
                 * list.Front() = 100;  // list contient maintenant {100, 20, 30}
                 * @endcode
                 */
                Reference Front() {
                    NKENTSEU_ASSERT(mHead != nullptr);
                    return mHead->value;
                }

                /**
                 * @brief Accès au premier élément — version constante
                 * @return Référence constante vers le value du premier nœud
                 * @ingroup DoubleListAccess
                 *
                 * Version const de Front() pour les listes en lecture seule.
                 * Même sémantique de vérification que la version mutable.
                 *
                 * @note Complexité : O(1) — accès direct
                 * @note Utile pour lire la première valeur sans modification
                 *
                 * @example
                 * @code
                 * int GetFirst(const NkDoubleList<int>& list) {
                 *     return list.Front();  // Lecture du premier élément
                 * }
                 * @endcode
                 */
                ConstReference Front() const {
                    NKENTSEU_ASSERT(mHead != nullptr);
                    return mHead->value;
                }

                /**
                 * @brief Accès au dernier élément — version mutable
                 * @return Référence mutable vers le value du dernier nœud
                 * @ingroup DoubleListAccess
                 *
                 * Retourne une référence vers mTail->value avec assertion debug
                 * si la liste est vide. Rendu possible en O(1) grâce au pointeur
                 * tail mémorisé (contrairement à une liste sans tail qui nécessiterait O(n)).
                 *
                 * @note Complexité : O(1) — accès direct via pointeur mTail
                 * @note Assertion en debug si Empty() == true
                 * @note Comportement indéfini en release si liste vide
                 *
                 * @example
                 * @code
                 * NkDoubleList<int> list = {10, 20, 30};
                 * list.Back() = 300;  // list contient maintenant {10, 20, 300}
                 * @endcode
                 */
                Reference Back() {
                    NKENTSEU_ASSERT(mTail != nullptr);
                    return mTail->value;
                }

                /**
                 * @brief Accès au dernier élément — version constante
                 * @return Référence constante vers le value du dernier nœud
                 * @ingroup DoubleListAccess
                 *
                 * Version const de Back() pour les listes en lecture seule.
                 * Même sémantique de vérification que la version mutable.
                 *
                 * @note Complexité : O(1) — accès direct via mTail
                 * @note Très utile pour les algorithmes de type "queue" (FIFO)
                 *
                 * @example
                 * @code
                 * int PeekBack(const NkDoubleList<int>& queue) {
                 *     return queue.Back();  // Lire le dernier sans retirer
                 * }
                 * @endcode
                 */
                ConstReference Back() const {
                    NKENTSEU_ASSERT(mTail != nullptr);
                    return mTail->value;
                }

                // -----------------------------------------------------------------
                // Sous-section : Modificateurs de contenu
                // -----------------------------------------------------------------
                /**
                 * @brief Détruit tous les nœuds et remet la liste à l'état vide
                 * @ingroup DoubleListModifiers
                 *
                 * Parcours tous les nœuds de mHead à nullptr via les liens next :
                 *  1. Pour chaque nœud : sauvegarde next, appelle DeallocateNode(), avance
                 *  2. Après boucle : réinitialise mTail et mSize à l'état vide
                 *
                 * @note Complexité : O(n) pour la destruction des n nœuds
                 * @note noexcept implicite : ne lève pas d'exception (destructor safety)
                 * @note Tous les itérateurs deviennent invalides après Clear()
                 * @note Ne libère pas l'allocateur lui-même, seulement les nœuds alloués
                 *
                 * @example
                 * @code
                 * NkDoubleList<std::string> list = {"a", "b", "c"};
                 * list.Clear();  // Détruit les 3 strings, list.Size() == 0
                 * list.PushBack("new");  // Réalloue un nouveau nœud
                 * @endcode
                 */
                void Clear() {
                    Node* current = mHead;
                    while (current) {
                        Node* next = current->next;
                        DeallocateNode(current);
                        current = next;
                    }
                    mHead = nullptr;
                    mTail = nullptr;
                    mSize = 0;
                }

                /**
                 * @brief Ajoute une copie de l'élément au début de la liste
                 * @param value Valeur à copier et insérer en tête
                 * @ingroup DoubleListModifiers
                 *
                 * Crée un nouveau nœud avec AllocateNode(), puis le lie en tête :
                 *  1. newNode->next = mHead, newNode->prev = nullptr
                 *  2. Si mHead existe : mHead->prev = newNode
                 *  3. mHead = newNode
                 *  4. Si liste vide : mTail = newNode aussi
                 *
                 * @note Complexité : O(1) — allocation et liaison de nœud constant
                 * @note Peut invalider les itérateurs pointant vers l'ancienne tête
                 * @note Requiert que T soit CopyConstructible
                 * @note mSize est incrémenté pour maintenir Size() exact
                 *
                 * @example
                 * @code
                 * NkDoubleList<int> list = {2, 3};
                 * list.PushFront(1);  // list contient maintenant {1, 2, 3}
                 * @endcode
                 */
                void PushFront(const T& value) {
                    Node* newNode = AllocateNode(value);
                    if (!newNode) {
                        return;
                    }
                    newNode->next = mHead;
                    newNode->prev = nullptr;
                    if (mHead) {
                        mHead->prev = newNode;
                    }
                    mHead = newNode;
                    if (!mTail) {
                        mTail = newNode;
                    }
                    ++mSize;
                }

                /**
                 * @brief Ajoute une copie de l'élément à la fin de la liste
                 * @param value Valeur à copier et insérer en queue
                 * @ingroup DoubleListModifiers
                 *
                 * Crée un nouveau nœud avec AllocateNode(), puis le lie en queue :
                 *  1. newNode->prev = mTail, newNode->next = nullptr
                 *  2. Si mTail existe : mTail->next = newNode
                 *  3. mTail = newNode
                 *  4. Si liste vide : mHead = newNode aussi
                 *
                 * @note Complexité : O(1) — grâce au pointeur tail mémorisé
                 * @note Peut invalider les itérateurs end() (sentinelle nullptr)
                 * @note Requiert que T soit CopyConstructible
                 * @note mSize est incrémenté pour maintenir Size() exact
                 *
                 * @example
                 * @code
                 * NkDoubleList<int> list = {1, 2};
                 * list.PushBack(3);  // list contient maintenant {1, 2, 3}
                 * @endcode
                 */
                void PushBack(const T& value) {
                    Node* newNode = AllocateNode(value);
                    if (!newNode) {
                        return;
                    }
                    newNode->prev = mTail;
                    newNode->next = nullptr;
                    if (mTail) {
                        mTail->next = newNode;
                    }
                    mTail = newNode;
                    if (!mHead) {
                        mHead = newNode;
                    }
                    ++mSize;
                }

                /**
                 * @brief Ajoute un élément par déplacement au début de la liste
                 * @param value Valeur à déplacer et insérer en tête
                 * @ingroup DoubleListModifiers
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
                    Node* newNode = AllocateNode(traits::NkMove(value));
                    if (!newNode) {
                        return;
                    }
                    newNode->next = mHead;
                    newNode->prev = nullptr;
                    if (mHead) {
                        mHead->prev = newNode;
                    }
                    mHead = newNode;
                    if (!mTail) {
                        mTail = newNode;
                    }
                    ++mSize;
                }

                /**
                 * @brief Ajoute un élément par déplacement à la fin de la liste
                 * @param value Valeur à déplacer et insérer en queue
                 * @ingroup DoubleListModifiers
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
                    Node* newNode = AllocateNode(traits::NkMove(value));
                    if (!newNode) {
                        return;
                    }
                    newNode->prev = mTail;
                    newNode->next = nullptr;
                    if (mTail) {
                        mTail->next = newNode;
                    }
                    mTail = newNode;
                    if (!mHead) {
                        mHead = newNode;
                    }
                    ++mSize;
                }

                /**
                 * @brief Construit un élément in-place au début de la liste
                 * @tparam Args Types des arguments de construction (déduits)
                 * @param args Arguments forwardés au constructeur de T
                 * @ingroup DoubleListModifiers
                 *
                 * Évite toute copie ou déplacement temporaire en construisant
                 * directement l'objet dans le nœud alloué via placement new
                 * et forwarding parfait des arguments.
                 *
                 * @note Complexité : O(1) — comme PushFront
                 * @note Peut invalider les itérateurs pointant vers l'ancienne tête
                 * @note Idéal pour les types coûteux à copier/move ou non copyable
                 *
                 * @example
                 * @code
                 * // Sans EmplaceFront : copie temporaire
                 * list.PushFront(std::string(1000, 'x'));
                 *
                 * // Avec EmplaceFront : construction directe, zéro copie
                 * list.EmplaceFront(1000, 'x');  // Construit string(1000, 'x') in-place
                 * @endcode
                 */
                template<typename... Args>
                void EmplaceFront(Args&&... args) {
                    Node* newNode = AllocateNode(traits::NkForward<Args>(args)...);
                    if (!newNode) {
                        return;
                    }
                    newNode->next = mHead;
                    newNode->prev = nullptr;
                    if (mHead) {
                        mHead->prev = newNode;
                    }
                    mHead = newNode;
                    if (!mTail) {
                        mTail = newNode;
                    }
                    ++mSize;
                }

                /**
                 * @brief Construit un élément in-place à la fin de la liste
                 * @tparam Args Types des arguments de construction (déduits)
                 * @param args Arguments forwardés au constructeur de T
                 * @ingroup DoubleListModifiers
                 *
                 * Version Emplace pour PushBack : construction directe dans
                 * le nœud alloué sans copie/move temporaire.
                 *
                 * @note Complexité : O(1) — comme PushBack
                 * @note Peut invalider les itérateurs end() (sentinelle nullptr)
                 * @note Idéal pour les types lourds ou non copyable
                 *
                 * @example
                 * @code
                 * list.EmplaceBack(1000, 'y');  // Construit string(1000, 'y') in-place en queue
                 * @endcode
                 */
                template<typename... Args>
                void EmplaceBack(Args&&... args) {
                    Node* newNode = AllocateNode(traits::NkForward<Args>(args)...);
                    if (!newNode) {
                        return;
                    }
                    newNode->prev = mTail;
                    newNode->next = nullptr;
                    if (mTail) {
                        mTail->next = newNode;
                    }
                    mTail = newNode;
                    if (!mHead) {
                        mHead = newNode;
                    }
                    ++mSize;
                }

                /**
                 * @brief Supprime le premier élément de la liste
                 * @ingroup DoubleListModifiers
                 *
                 * Détache et détruit le nœud de tête :
                 *  1. Sauvegarde oldHead = mHead
                 *  2. mHead = mHead->next
                 *  3. Si nouveau mHead existe : mHead->prev = nullptr
                 *  4. Sinon (liste devenue vide) : mTail = nullptr aussi
                 *  5. DeallocateNode(oldHead)
                 *
                 * @note Complexité : O(1) — suppression d'un seul nœud
                 * @note Assertion en debug si Empty() == true (underflow protection)
                 * @note Comportement indéfini en release si liste vide
                 * @note Peut invalider les itérateurs pointant vers l'élément supprimé
                 * @note mSize est décrémenté pour maintenir Size() exact
                 *
                 * @example
                 * @code
                 * NkDoubleList<int> list = {1, 2, 3};
                 * list.PopFront();  // list contient maintenant {2, 3}
                 * list.PopFront();  // list contient maintenant {3}
                 * @endcode
                 */
                void PopFront() {
                    NKENTSEU_ASSERT(mHead != nullptr);
                    Node* oldHead = mHead;
                    mHead = mHead->next;
                    if (mHead) {
                        mHead->prev = nullptr;
                    }
                    else {
                        mTail = nullptr;
                    }
                    DeallocateNode(oldHead);
                    --mSize;
                }

                /**
                 * @brief Supprime le dernier élément de la liste
                 * @ingroup DoubleListModifiers
                 *
                 * Détache et détruit le nœud de queue :
                 *  1. Sauvegarde oldTail = mTail
                 *  2. mTail = mTail->prev
                 *  3. Si nouveau mTail existe : mTail->next = nullptr
                 *  4. Sinon (liste devenue vide) : mHead = nullptr aussi
                 *  5. DeallocateNode(oldTail)
                 *
                 * @note Complexité : O(1) — grâce au pointeur tail et lien prev bidirectionnel
                 * @note Assertion en debug si Empty() == true (underflow protection)
                 * @note Comportement indéfini en release si liste vide
                 * @note Avantage clé vs NkList (singly) : PopBack O(1) possible ici
                 * @note mSize est décrémenté pour maintenir Size() exact
                 *
                 * @example
                 * @code
                 * NkDoubleList<int> list = {1, 2, 3};
                 * list.PopBack();  // list contient maintenant {1, 2}
                 * list.PopBack();  // list contient maintenant {1}
                 * @endcode
                 */
                void PopBack() {
                    NKENTSEU_ASSERT(mTail != nullptr);
                    Node* oldTail = mTail;
                    mTail = mTail->prev;
                    if (mTail) {
                        mTail->next = nullptr;
                    }
                    else {
                        mHead = nullptr;
                    }
                    DeallocateNode(oldTail);
                    --mSize;
                }

                /**
                 * @brief Insère une copie d'un élément avant une position donnée
                 * @param pos Itérateur constant vers la position avant laquelle insérer
                 * @param value Valeur à copier et insérer
                 * @return Iterator pointant vers le nouvel élément inséré
                 * @ingroup DoubleListModifiers
                 *
                 * Grâce aux liens bidirectionnels, l'insertion avant une position
                 * connue est O(1) :
                 *  1. Si pos == begin() : délégation vers PushFront (optimisation)
                 *  2. Sinon : alloue newNode, lie prev/next des nœuds adjacents
                 *  3. Met à jour les pointeurs prev/next pour intégrer newNode
                 *
                 * @note Complexité : O(1) si itérateur déjà obtenu, O(n) pour atteindre la position
                 * @note Peut invalider les itérateurs pointant vers les nœuds modifiés
                 * @note Requiert que T soit CopyConstructible
                 * @note mSize est incrémenté pour maintenir Size() exact
                 *
                 * @example
                 * @code
                 * NkDoubleList<int> list = {1, 3, 4};
                 * auto it = list.begin();
                 * ++it;  // Position index 1 (valeur 3)
                 * list.Insert(it, 2);  // Insère 2 avant 3
                 * // list contient maintenant {1, 2, 3, 4}
                 * @endcode
                 */
                Iterator Insert(ConstIterator pos, const T& value) {
                    if (!pos.mNode || pos.mNode == mHead) {
                        PushFront(value);
                        return begin();
                    }
                    Node* newNode = AllocateNode(value);
                    if (!newNode) {
                        return end();
                    }
                    Node* posNode = const_cast<Node*>(pos.mNode);
                    Node* prevNode = posNode->prev;
                    newNode->next = posNode;
                    newNode->prev = prevNode;
                    posNode->prev = newNode;
                    if (prevNode) {
                        prevNode->next = newNode;
                    }
                    ++mSize;
                    return Iterator(newNode);
                }

                /**
                 * @brief Supprime un élément à une position donnée
                 * @param pos Itérateur constant vers l'élément à supprimer
                 * @return Iterator pointant vers l'élément suivant la suppression
                 * @ingroup DoubleListModifiers
                 *
                 * Grâce aux liens bidirectionnels, la suppression à une position
                 * connue est O(1) :
                 *  1. Sauvegarde prevNode et nextNode de l'élément à supprimer
                 *  2. Relie prevNode->next = nextNode et nextNode->prev = prevNode
                 *  3. Met à jour mHead/mTail si l'élément supprimé était une extrémité
                 *  4. DeallocateNode(toDelete)
                 *
                 * @note Complexité : O(1) si itérateur déjà obtenu, O(n) pour atteindre la position
                 * @note Peut invalider les itérateurs pointant vers l'élément supprimé
                 * @note Assertion en debug si pos.mNode == nullptr
                 * @note mSize est décrémenté pour maintenir Size() exact
                 *
                 * @example
                 * @code
                 * NkDoubleList<int> list = {1, 2, 3, 4};
                 * auto it = list.begin();
                 * ++it; ++it;  // Position index 2 (valeur 3)
                 * auto next = list.Erase(it);  // Supprime 3, retourne itérateur vers 4
                 * // list contient maintenant {1, 2, 4}, 'next' pointe vers 4
                 * @endcode
                 */
                Iterator Erase(ConstIterator pos) {
                    NKENTSEU_ASSERT(pos.mNode != nullptr);
                    Node* toDelete = const_cast<Node*>(pos.mNode);
                    Node* prevNode = toDelete->prev;
                    Node* nextNode = toDelete->next;
                    if (prevNode) {
                        prevNode->next = nextNode;
                    }
                    else {
                        mHead = nextNode;
                    }
                    if (nextNode) {
                        nextNode->prev = prevNode;
                    }
                    else {
                        mTail = prevNode;
                    }
                    DeallocateNode(toDelete);
                    --mSize;
                    return Iterator(nextNode);
                }

                /**
                 * @brief Inverse l'ordre des nœuds dans la liste in-place
                 * @ingroup DoubleListModifiers
                 *
                 * Algorithme de reversal itératif en échangeant les liens prev/next :
                 *  1. Sauvegarde old head comme new tail
                 *  2. Pour chaque nœud : échange current->next et current->prev
                 *  3. Avance via l'ancien next (devenu prev après échange)
                 *  4. Après boucle : mHead pointe vers l'ancienne queue
                 *
                 * @note Complexité : O(n) — parcours unique de tous les nœuds
                 * @note noexcept implicite : ne lève pas d'exception
                 * @note Tous les itérateurs deviennent invalides après Reverse()
                 * @note Utile pour transformer une stack en queue ou vice-versa
                 *
                 * @example
                 * @code
                 * NkDoubleList<int> list = {1, 2, 3, 4};
                 * list.Reverse();  // list contient maintenant {4, 3, 2, 1}
                 * @endcode
                 */
                void Reverse() {
                    Node* current = mHead;
                    mTail = mHead;
                    while (current) {
                        Node* temp = current->next;
                        current->next = current->prev;
                        current->prev = temp;
                        if (!temp) {
                            mHead = current;
                        }
                        current = temp;
                    }
                }

                /**
                 * @brief Échange le contenu de cette liste avec une autre en O(1)
                 * @param other Autre liste avec laquelle échanger le contenu
                 * @ingroup DoubleListModifiers
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
                 * NkDoubleList<int> a = {1, 2}, b = {3, 4, 5};
                 * a.Swap(b);  // a = {3,4,5}, b = {1,2} en O(1)
                 *
                 * // Idiom copy-and-swap pour assignment exception-safe
                 * NkDoubleList& operator=(NkDoubleList other) {  // other est une copie
                 *     Swap(other);  // Échange O(1) avec la copie
                 *     return *this; // other détruit libère l'ancien contenu
                 * }
                 * @endcode
                 */
                void Swap(NkDoubleList& other) NKENTSEU_NOEXCEPT {
                    traits::NkSwap(mHead, other.mHead);
                    traits::NkSwap(mTail, other.mTail);
                    traits::NkSwap(mSize, other.mSize);
                    traits::NkSwap(mAllocator, other.mAllocator);
                }

        }; // class NkDoubleList

        // -------------------------------------------------------------------------
        // SECTION 4 : FONCTIONS NON-MEMBRES ET OPÉRATEURS
        // -------------------------------------------------------------------------
        // Ces fonctions libres fournissent une interface additionnelle pour
        // NkDoubleList, compatible avec les algorithmes génériques et les idiomes C++.

        /**
         * @brief Échange le contenu de deux listes via leur méthode membre Swap
         * @tparam T Type des éléments stockés
         * @tparam Allocator Type de l'allocateur utilisé
         * @param lhs Première liste à échanger
         * @param rhs Seconde liste à échanger
         * @ingroup DoubleListAlgorithms
         *
         * Fonction libre permettant l'usage de std::swap-like avec NkDoubleList.
         * Délègue simplement à la méthode membre Swap() pour une implémentation
         * O(1) efficace.
         *
         * @note Complexité : O(1) — délégation vers NkDoubleList::Swap
         * @note noexcept : garantie de ne pas lever d'exception
         * @note Utile pour les algorithmes de tri et l'idiome copy-and-swap
         *
         * @example
         * @code
         * NkDoubleList<int> a = {1, 2}, b = {3, 4};
         * NkSwap(a, b);  // a = {3,4}, b = {1,2} via délégation vers a.Swap(b)
         * @endcode
         */
        template<typename T, typename Allocator>
        void NkSwap(
            NkDoubleList<T, Allocator>& lhs,
            NkDoubleList<T, Allocator>& rhs
        ) NKENTSEU_NOEXCEPT {
            lhs.Swap(rhs);
        }

        // template<typename T>
        // NkString NkToString(const NkDoubleList<T>& c, const NkFormatProps& props = {}) {
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
            #pragma message("NkDoubleList: C++11 move semantics ENABLED")
        #else
            #pragma message("NkDoubleList: C++11 move semantics DISABLED (using copy fallback)")
        #endif
    #endif

#endif // NKENTSEU_CONTAINERS_SEQUENTIAL_NKDOUBLELIST_H_INCLUDED

// =============================================================================
// EXEMPLES D'UTILISATION DÉTAILLÉS
// =============================================================================
// Cette section fournit des exemples concrets et complets d'utilisation de
// NkDoubleList, couvrant les cas d'usage courants et les bonnes pratiques.
// Ces exemples sont en commentaires pour ne pas affecter la compilation.
// =============================================================================

/*
    // -----------------------------------------------------------------------------
    // Exemple 1 : Construction et initialisation variées
    // -----------------------------------------------------------------------------
    #include <NKContainers/Sequential/NkDoubleList.h>
    using namespace nkentseu::containers;

    void ConstructionExamples() {
        // Constructeur par défaut : liste vide
        NkDoubleList<int> empty;

        // Constructeur avec allocateur personnalisé
        NkDoubleList<int> customAlloc(&myCustomAllocator);

        // Constructeur depuis liste d'initialisation
        NkDoubleList<int> init = {10, 20, 30};  // {10, 20, 30}

        // Constructeur de copie
        NkDoubleList<int> copy = init;  // Copie profonde : {10, 20, 30}

        #if defined(NKENTSEU_CXX11_OR_LATER)
        // Constructeur de déplacement (C++11)
        NkDoubleList<int> temp = MakeLargeList();
        NkDoubleList<int> moved = NKENTSEU_MOVE(temp);  // Transfert O(1), temp vide
        #endif
    }

    // -----------------------------------------------------------------------------
    // Exemple 2 : Insertion et suppression aux extrémités — O(1)
    // -----------------------------------------------------------------------------
    void EndOperationsExamples() {
        NkDoubleList<int> list;

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

        // PopBack : suppression en queue O(1) — avantage vs NkList (singly)
        list.PopBack();   // list = {3, 4}
        list.PopBack();   // list = {3}
    }

    // -----------------------------------------------------------------------------
    // Exemple 3 : Parcours avec itérateurs bidirectionnels
    // -----------------------------------------------------------------------------
    void IterationExamples() {
        NkDoubleList<int> list = {1, 2, 3, 4, 5};

        // Parcours avant avec itérateur bidirectionnel
        for (auto it = list.begin(); it != list.end(); ++it) {
            *it *= 2;  // Modification via itérateur mutable
        }
        // list contient maintenant {2, 4, 6, 8, 10}

        // Parcours arrière avec operator-- (bidirectional)
        for (auto it = list.end(); it != list.begin(); ) {
            --it;  // Recule d'un élément
            printf("%d ", *it);  // Affiche: 10 8 6 4 2
        }

        // Parcours arrière avec reverse iterator (plus idiomatique)
        for (auto rit = list.rbegin(); rit != list.rend(); ++rit) {
            printf("%d ", *rit);  // Affiche: 10 8 6 4 2
        }

        // Parcours en lecture seule (const)
        void PrintList(const NkDoubleList<int>& lst) {
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
    }

    // -----------------------------------------------------------------------------
    // Exemple 4 : Insertion/Suppression à position arbitraire — O(1) si itérateur connu
    // -----------------------------------------------------------------------------
    void PositionOperationsExamples() {
        NkDoubleList<int> list = {1, 2, 3, 4, 5};

        // Obtention d'un itérateur vers une position (O(n) pour atteindre)
        auto it = list.begin();
        ++it; ++it;  // Position index 2 (valeur 3)

        // Insertion avant cette position : O(1) une fois l'itérateur obtenu
        list.Insert(it, 99);  // Insère 99 avant 3
        // list contient maintenant {1, 2, 99, 3, 4, 5}

        // Suppression à cette position : O(1) une fois l'itérateur obtenu
        auto next = list.Erase(it);  // Supprime 3, retourne itérateur vers 4
        // list contient maintenant {1, 2, 99, 4, 5}, 'next' pointe vers 4

        // Note : Le coût O(n) est uniquement pour atteindre la position via ++it
        // Une fois l'itérateur obtenu, Insert/Erase sont O(1) grâce aux liens doubles
    }

    // -----------------------------------------------------------------------------
    // Exemple 5 : Modification avancée — Reverse, Swap, Clear
    // -----------------------------------------------------------------------------
    void AdvancedModificationExamples() {
        NkDoubleList<int> list = {1, 2, 3, 4, 5};

        // Inversion in-place de la liste : O(n)
        list.Reverse();  // list contient maintenant {5, 4, 3, 2, 1}

        // Échange O(1) avec une autre liste
        NkDoubleList<int> other = {10, 20};
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
    // Exemple 6 : Utilisation comme structure de données spécialisée
    // -----------------------------------------------------------------------------
    // NkDoubleList est idéal pour implémenter des structures LIFO/FIFO/deque :

    // Deque (double-ended queue) via PushFront/PushBack/PopFront/PopBack
    template<typename T>
    class Deque {
    private:
        NkDoubleList<T> mStorage;
    public:
        void PushFront(const T& value) { mStorage.PushFront(value); }  // O(1)
        void PushBack(const T& value) { mStorage.PushBack(value); }    // O(1)
        void PopFront() { mStorage.PopFront(); }                       // O(1)
        void PopBack() { mStorage.PopBack(); }                         // O(1)
        T& Front() { return mStorage.Front(); }                        // O(1)
        T& Back() { return mStorage.Back(); }                          // O(1)
        bool Empty() const { return mStorage.Empty(); }                // O(1)
    };

    void DequeUsage() {
        Deque<int> dq;
        dq.PushFront(1);   // dq = {1}
        dq.PushBack(2);    // dq = {1, 2}
        dq.PushFront(0);   // dq = {0, 1, 2}
        dq.PushBack(3);    // dq = {0, 1, 2, 3}

        int front = dq.Front();  // front = 0
        int back = dq.Back();    // back = 3

        dq.PopFront();   // dq = {1, 2, 3}
        dq.PopBack();    // dq = {1, 2}
    }

    // -----------------------------------------------------------------------------
    // Exemple 7 : Intégration avec algorithmes génériques
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
        NkDoubleList<int> list = {1, 2, 3, 4, 5, 6};

        // Utiliser NkDoubleList avec algorithmes génériques grâce aux itérateurs Bidirectional
        auto it = NkFindIf(list.begin(), list.end(), [](int x) { return x % 2 == 0; });
        if (it != list.end()) {
            printf("First even: %d\n", *it);  // Affiche: First even: 2
        }

        // Parcours inversé avec algorithme générique
        auto rit = NkFindIf(list.rbegin(), list.rend(), [](int x) { return x > 3; });
        if (rit != list.rend()) {
            printf("First from back > 3: %d\n", *rit);  // Affiche: First from back > 3: 6
        }

        // Range-based for grâce aux aliases begin()/end()
        int sum = 0;
        for (int val : list) {
            sum += val;
        }
        // sum = 21
    }

    // -----------------------------------------------------------------------------
    // Exemple 8 : Optimisations avec move semantics et emplace (C++11)
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
        NkDoubleList<Expensive> list;

        // EmplaceFront : construction directe in-place, zéro copie/move temporaire
        list.EmplaceFront(1024);  // Construit Expensive(1024) directement dans le nœud

        // PushBack avec move : évite la copie grâce à move semantics
        Expensive temp(2048);
        list.PushBack(NKENTSEU_MOVE(temp));  // Déplace temp dans list

        // Swap O(1) pour réorganiser sans copier les données lourdes
        NkDoubleList<Expensive> other;
        other.EmplaceBack(512);
        list.Swap(other);  // Échange les chaînes de nœuds en O(1)
    }
    #endif
*/

// =============================================================================
// NOTES DE MIGRATION ET COMPATIBILITÉ
// =============================================================================
/*
    Migration depuis std::list vers NkDoubleList :

    std::list<T> API          | NkDoubleList<T> équivalent
    --------------------------|----------------------------------
    std::list<T> lst;        | NkDoubleList<T> lst;
    lst.push_front(x);       | lst.PushFront(x);
    lst.push_back(x);        | lst.PushBack(x);
    lst.emplace_front(args); | lst.EmplaceFront(args...);
    lst.emplace_back(args);  | lst.EmplaceBack(args...);
    lst.pop_front();         | lst.PopFront();
    lst.pop_back();          | lst.PopBack();
    lst.front()              | lst.Front()
    lst.back()               | lst.Back()
    lst.begin(), lst.end()   | lst.begin()/end() (mêmes noms)
    lst.rbegin(), lst.rend() | lst.rbegin()/rend() (mêmes noms)
    lst.size(), lst.empty()  | lst.Size()/Empty() OU lst.size()/empty() (aliases)
    lst.clear()              | lst.Clear()
    lst.reverse()            | lst.Reverse()
    lst.insert(pos, val)     | lst.Insert(pos, val)
    lst.erase(pos)           | lst.Erase(pos)
    std::swap(a, b)          | NkSwap(a, b) ou a.Swap(b)

    Avantages de NkDoubleList vs std::list :
    - Zéro dépendance STL : portable sur plateformes embarquées, kernels, etc.
    - Allocateur personnalisable via template (pas de std::allocator hardcoded)
    - Gestion d'erreurs configurable : assertions ou exceptions selon le build
    - Nommage PascalCase cohérent avec le reste du framework NK* (pour méthodes principales)
    - Macros NKENTSEU_* pour export/import DLL, inline hints, etc.
    - Intégration avec le système de logging et d'assertions NKPlatform
    - Itérateurs Bidirectional natifs avec operator-- (pas besoin d'adaptateur)

    Configuration CMake recommandée :
    # Activer C++11 pour move semantics et EmplaceFront/EmplaceBack
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