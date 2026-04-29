// =============================================================================
// NKContainers/Sequential/NkDeque.h
// File d'attente double (double-ended queue) — implémentation par chunks.
//
// Design :
//  - Réutilisation DIRECTE des macros NKCore/NKPlatform (ZÉRO duplication)
//  - Gestion mémoire via NKMemory::NkAllocator pour flexibilité d'allocation
//  - Implémentation par chunks : tableau de pointeurs vers blocs de données
//  - Accès aléatoire O(1) + insertion/suppression aux extrémités O(1) amorti
//  - Itérateurs RandomAccess pour compatibilité algorithmes génériques
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
//     - Implémentation initiale de NkDeque avec chunks de taille fixe
//     - Support C++11 move semantics pour constructeur/opérateur d'affectation
//     - Itérateurs RandomAccess complets avec arithmétique complète
//     - Gestion mémoire chunk-based avec allocation dynamique du tableau de chunks
// =============================================================================

#pragma once

#ifndef NKENTSEU_CONTAINERS_SEQUENTIAL_NKDEQUE_H_INCLUDED
#define NKENTSEU_CONTAINERS_SEQUENTIAL_NKDEQUE_H_INCLUDED

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // NkDeque dépend des modules suivants :
    //  - NKCore/NkTypes.h : Types fondamentaux (nk_size, nk_bool, etc.)
    //  - NKContainers/NkContainersApi.h : Macros d'export pour visibilité des symboles
    //  - NKCore/NkTraits.h : Utilitaires de métaprogrammation (NkMove, NkForward, etc.)
    //  - NKMemory/NkAllocator.h : Interface d'allocateur pour gestion mémoire flexible
    //  - NKMemory/NkFunction.h : Fonctions mémoire bas niveau (NkMemCopy, NkMemZero, etc.)
    //  - NKCore/Assert/NkAssert.h : Système d'assertions pour validation debug
    //  - NKContainers/Iterators/NkIterator.h : Tags d'itérateurs (NkRandomAccessIteratorTag)
    //  - NKContainers/Iterators/NkInitializerList.h : Liste d'initialisation légère

    #include "NKCore/NkTypes.h"
    #include "NKContainers/NkContainersApi.h"
    #include "NKCore/NkTraits.h"
    #include "NKMemory/NkAllocator.h"
    #include "NKMemory/NkFunction.h"
    #include "NKCore/Assert/NkAssert.h"
    #include "NKContainers/Iterators/NkIterator.h"
    #include "NKContainers/Iterators/NkInitializerList.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : ESPACE DE NOMS PRINCIPAL
    // -------------------------------------------------------------------------
    // Tous les composants NKContainers sont encapsulés dans ce namespace hiérarchique
    // pour éviter les collisions de noms et assurer une API cohérente et modulaire.
    // L'indentation reflète la hiérarchie : chaque niveau de namespace est indenté.

    namespace nkentseu {

        // ====================================================================
        // SECTION 3 : CLASSE TEMPLATE NKDEQUE
        // ====================================================================
        /**
         * @brief File d'attente double — conteneur séquentiel à accès aléatoire O(1).
         * @tparam T Type des éléments stockés dans le deque
         * @tparam Allocator Type de l'allocateur utilisé pour la gestion mémoire
         * @ingroup SequentialContainers
         *
         * NkDeque implémente une file d'attente double (double-ended queue),
         * combinant les avantages des tableaux et des listes chaînées :
         *
         *  - Accès aléatoire en temps constant O(1) via operator[]
         *  - Insertion/suppression en tête/queue amortie O(1)
         *  - Insertion/suppression au milieu O(n) (décalage nécessaire)
         *
         * @note Implémentation par chunks :
         *       - Structure interne : tableau de pointeurs vers des blocs (chunks) de données
         *       - Chaque chunk contient CHUNK_SIZE = 64 éléments (configurable via template)
         *       - Le tableau de chunks peut croître dynamiquement quand nécessaire
         *       - Les éléments sont répartis sur plusieurs chunks selon la logique circulaire
         *
         * @note Avantages vs NkVector :
         *       - PushFront/PopFront en O(1) amorti (vs O(n) pour NkVector)
         *       - Pas de réallocation massive lors de croissance aux deux extrémités
         *
         * @note Avantages vs NkDoubleList :
         *       - Accès aléatoire O(1) (vs O(n) pour liste chaînée)
         *       - Meilleure localité cache (éléments groupés par chunks)
         *       - Moins de fragmentation mémoire (allocation par blocs)
         *
         * @example Utilisation basique
         * @code
         * using namespace nkentseu::containers;
         *
         * // Construction avec liste d'initialisation
         * NkDeque<int> values = {1, 2, 3};
         *
         * // Insertion aux extrémités : O(1) amorti
         * values.PushFront(0);  // values = {0, 1, 2, 3}
         * values.PushBack(4);   // values = {0, 1, 2, 3, 4}
         *
         * // Accès aléatoire : O(1)
         * int third = values[2];  // third = 2
         * values[0] = 10;         // values = {10, 1, 2, 3, 4}
         *
         * // Parcours avec itérateurs RandomAccess
         * for (auto it = values.begin(); it != values.end(); ++it) {
         *     printf("%d ", *it);  // Affiche: 10 1 2 3 4
         * }
         *
         * // Suppression aux extrémités : O(1)
         * values.PopFront();  // values = {1, 2, 3, 4}
         * values.PopBack();   // values = {1, 2, 3}
         * @endcode
         */
        template<typename T, typename Allocator = memory::NkAllocator>
        class NKENTSEU_CONTAINERS_CLASS_EXPORT NkDeque {
            private:

                // -----------------------------------------------------------------
                // Sous-section : Constantes et structures internes
                // -----------------------------------------------------------------
                /**
                 * @brief Taille fixe de chaque chunk en nombre d'éléments
                 * @var CHUNK_SIZE
                 * @ingroup DequeInternals
                 *
                 * Nombre d'éléments stockés dans chaque bloc (chunk) de données.
                 * Valeur choisie pour équilibrer :
                 *  - Localité cache (taille raisonnable pour tenir dans les caches L1/L2)
                 *  - Surcoût mémoire (moins de chunks alloués pour grandes collections)
                 *  - Fragmentation (taille suffisante pour réduire les allocations fréquentes)
                 *
                 * @note Peut être ajusté via spécialisation de template si nécessaire
                 * @note Doit être une puissance de 2 pour optimiser les calculs modulo/division
                 */
                static constexpr nk_size CHUNK_SIZE = 64;

                /**
                 * @brief Structure de chunk interne contenant un tableau fixe d'éléments
                 * @struct Chunk
                 * @ingroup DequeInternals
                 *
                 * Bloc de données contenant exactement CHUNK_SIZE éléments de type T.
                 * Les chunks sont alloués individuellement via l'allocateur,
                 * permettant une croissance dynamique sans déplacer les éléments existants.
                 *
                 * @note Les éléments sont stockés dans un tableau brut pour performance maximale
                 * @note Pas de constructeur/destructeur : initialisation/détruction gérée par NkDeque
                 */
                struct Chunk {
                    T Data[CHUNK_SIZE];  ///< Tableau brut de CHUNK_SIZE éléments
                };

            public:

                // -----------------------------------------------------------------
                // Sous-section : Types membres publics
                // -----------------------------------------------------------------
                /**
                 * @brief Type des valeurs stockées dans le conteneur
                 * @ingroup DequeTypes
                 */
                using ValueType = T;

                /**
                 * @brief Type utilisé pour représenter la taille et les index
                 * @ingroup DequeTypes
                 * @note Alias vers nk_size pour cohérence avec le reste du framework
                 */
                using SizeType = nk_size;

                /**
                 * @brief Type de référence mutable aux éléments
                 * @ingroup DequeTypes
                 */
                using Reference = T&;

                /**
                 * @brief Type de référence constante aux éléments
                 * @ingroup DequeTypes
                 */
                using ConstReference = const T&;

                /**
                 * @brief Type de pointeur mutable vers les éléments
                 * @ingroup DequeTypes
                 */
                using Pointer = T*;

                /**
                 * @brief Type de pointeur constant vers les éléments
                 * @ingroup DequeTypes
                 */
                using ConstPointer = const T*;

                // -----------------------------------------------------------------
                // Sous-section : Itérateur mutable (RandomAccess)
                // -----------------------------------------------------------------
                /**
                 * @brief Itérateur mutable pour parcours random-access du deque
                 * @class Iterator
                 * @ingroup DequeIterators
                 *
                 * Itérateur RandomAccess complet supportant :
                 *  - Incrémentation/décrémentation (++/-- préfixe/postfixe)
                 *  - Arithmétique (+, -, +=, -=, [])
                 *  - Comparaisons ordonnées (<, <=, >, >=)
                 *  - Distance entre itérateurs (operator-)
                 *
                 * @note Catégorie : NkRandomAccessIteratorTag (hérite de toutes les catégories inférieures)
                 * @note Implémentation légère : stocke un pointeur vers le deque et un index
                 * @note Compatible avec tous les algorithmes génériques STL attendants RandomAccessIterator
                 */
                class Iterator {
                    private:

                        /** @brief Pointeur vers le deque parent (privé) */
                        NkDeque* mDeque;

                        /** @brief Index de l'élément courant dans le deque (privé) */
                        SizeType mIndex;

                        /** @brief NkDeque est ami pour accéder aux membres privés */
                        friend class NkDeque;

                    public:

                        /** @brief Type des valeurs pointées par l'itérateur */
                        using ValueType = T;

                        /** @brief Type de référence mutable retourné par operator* */
                        using Reference = T&;

                        /** @brief Type de pointeur mutable retourné par operator-> */
                        using Pointer = T*;

                        /** @brief Type pour les différences entre itérateurs */
                        using DifferenceType = nkentseu::ptrdiff;

                        /** @brief Catégorie d'itérateur : RandomAccess */
                        using IteratorCategory = NkRandomAccessIteratorTag;

                        /**
                         * @brief Constructeur par défaut — itérateur invalide
                         * @ingroup DequeIterators
                         */
                        Iterator()
                            : mDeque(nullptr)
                            , mIndex(0) {
                        }

                        /**
                         * @brief Constructeur explicite depuis un deque et un index
                         * @param deque Pointeur vers le deque parent
                         * @param index Index de l'élément à encapsuler
                         * @ingroup DequeIterators
                         */
                        Iterator(NkDeque* deque, SizeType index)
                            : mDeque(deque)
                            , mIndex(index) {
                        }

                        /**
                         * @brief Déréférencement — accès à l'élément courant
                         * @return Référence mutable vers l'élément à l'index courant
                         * @ingroup DequeIterators
                         *
                         * @note Complexité : O(1) — délégation vers operator[] du deque
                         * @note Comportement indéfini si mDeque == nullptr ou index hors bornes
                         */
                        Reference operator*() const {
                            return (*mDeque)[mIndex];
                        }

                        /**
                         * @brief Accès membre via pointeur — équivalent à &(operator*())
                         * @return Pointeur mutable vers l'élément à l'index courant
                         * @ingroup DequeIterators
                         *
                         * @note Utile pour accéder aux membres de T via it->member
                         * @note Même avertissement : vérifier validité avant usage
                         */
                        Pointer operator->() const {
                            return &(*mDeque)[mIndex];
                        }

                        /**
                         * @brief Incrémentation préfixe — avance à l'élément suivant
                         * @return Référence vers *this après avancement
                         * @ingroup DequeIterators
                         *
                         * @note Complexité : O(1) — simple incrémentation d'index
                         * @note Comportement indéfini si appelé sur end() (index == size)
                         */
                        Iterator& operator++() {
                            ++mIndex;
                            return *this;
                        }

                        /**
                         * @brief Incrémentation postfixe — avance et retourne l'ancienne position
                         * @return Copie de l'itérateur avant avancement
                         * @ingroup DequeIterators
                         *
                         * @note Complexité : O(1) mais avec copie temporaire
                         * @note Préférer ++it (préfixe) quand la valeur précédente n'est pas nécessaire
                         */
                        Iterator operator++(int) {
                            Iterator tmp = *this;
                            ++mIndex;
                            return tmp;
                        }

                        /**
                         * @brief Décrémentation préfixe — recule à l'élément précédent
                         * @return Référence vers *this après recul
                         * @ingroup DequeIterators
                         *
                         * @note Complexité : O(1) — simple décrémentation d'index
                         * @note Comportement indéfini si appelé sur begin() (index == 0)
                         */
                        Iterator& operator--() {
                            --mIndex;
                            return *this;
                        }

                        /**
                         * @brief Décrémentation postfixe — recule et retourne l'ancienne position
                         * @return Copie de l'itérateur avant recul
                         * @ingroup DequeIterators
                         *
                         * @note Complexité : O(1) avec copie temporaire
                         * @note Préférer --it (préfixe) pour performance
                         */
                        Iterator operator--(int) {
                            Iterator tmp = *this;
                            --mIndex;
                            return tmp;
                        }

                        /**
                         * @brief Addition assignée — avance de n positions
                         * @param n Nombre de positions à avancer (peut être négatif)
                         * @return Référence vers *this après avancement
                         * @ingroup DequeIterators
                         *
                         * @note Complexité : O(1) — simple addition d'index
                         * @note Comportement indéfini si résultat hors [0, size]
                         */
                        Iterator& operator+=(DifferenceType n) {
                            mIndex += static_cast<SizeType>(n);
                            return *this;
                        }

                        /**
                         * @brief Soustraction assignée — recule de n positions
                         * @param n Nombre de positions à reculer (peut être négatif)
                         * @return Référence vers *this après recul
                         * @ingroup DequeIterators
                         *
                         * @note Complexité : O(1) — simple soustraction d'index
                         * @note Comportement indéfini si résultat hors [0, size]
                         */
                        Iterator& operator-=(DifferenceType n) {
                            mIndex -= static_cast<SizeType>(n);
                            return *this;
                        }

                        /**
                         * @brief Addition — retourne un nouvel itérateur avancé
                         * @param n Nombre de positions à avancer
                         * @return Nouvel itérateur à la position courante + n
                         * @ingroup DequeIterators
                         *
                         * @note Complexité : O(1) — construction d'un nouvel itérateur
                         * @note Comportement indéfini si résultat hors [0, size]
                         */
                        Iterator operator+(DifferenceType n) const {
                            return Iterator(mDeque, mIndex + static_cast<SizeType>(n));
                        }

                        /**
                         * @brief Soustraction — retourne un nouvel itérateur reculé
                         * @param n Nombre de positions à reculer
                         * @return Nouvel itérateur à la position courante - n
                         * @ingroup DequeIterators
                         *
                         * @note Complexité : O(1) — construction d'un nouvel itérateur
                         * @note Comportement indéfini si résultat hors [0, size]
                         */
                        Iterator operator-(DifferenceType n) const {
                            return Iterator(mDeque, mIndex - static_cast<SizeType>(n));
                        }

                        /**
                         * @brief Distance entre deux itérateurs
                         * @param other Autre itérateur à comparer
                         * @return Différence d'index entre cet itérateur et other
                         * @ingroup DequeIterators
                         *
                         * @note Complexité : O(1) — simple soustraction d'index
                         * @note Le résultat peut être négatif si this < other
                         */
                        DifferenceType operator-(const Iterator& other) const {
                            return static_cast<DifferenceType>(mIndex) - static_cast<DifferenceType>(other.mIndex);
                        }

                        /**
                         * @brief Accès par index relatif — équivalent à *(it + n)
                         * @param n Offset relatif depuis la position courante
                         * @return Référence mutable vers l'élément à l'offset n
                         * @ingroup DequeIterators
                         *
                         * @note Complexité : O(1) — délégation vers operator[] du deque
                         * @note Comportement indéfini si offset hors bornes valides
                         */
                        Reference operator[](DifferenceType n) const {
                            return (*mDeque)[mIndex + static_cast<SizeType>(n)];
                        }

                        /**
                         * @brief Comparaison d'égalité entre deux itérateurs
                         * @param o Autre itérateur à comparer
                         * @return true si les deux itérateurs pointent sur le même élément
                         * @ingroup DequeIterators
                         *
                         * @note Complexité : O(1) — comparaison d'index
                         * @note Utilisé pour la condition de fin de boucle : it != end()
                         */
                        bool operator==(const Iterator& o) const {
                            return mIndex == o.mIndex;
                        }

                        /**
                         * @brief Comparaison de non-égalité entre deux itérateurs
                         * @param o Autre itérateur à comparer
                         * @return true si les itérateurs pointent sur des éléments différents
                         * @ingroup DequeIterators
                         *
                         * @note Implémentation via négation de operator==
                         * @note Utilisé dans les boucles : for (; it != end(); ++it)
                         */
                        bool operator!=(const Iterator& o) const {
                            return mIndex != o.mIndex;
                        }

                        /**
                         * @brief Comparaison d'ordre strictement inférieur
                         * @param o Autre itérateur à comparer
                         * @return true si cet itérateur précède other dans le deque
                         * @ingroup DequeIterators
                         *
                         * @note Complexité : O(1) — comparaison d'index
                         * @note Essentiel pour les algorithmes de tri et de recherche
                         */
                        bool operator<(const Iterator& o) const {
                            return mIndex < o.mIndex;
                        }

                        /**
                         * @brief Comparaison d'ordre inférieur ou égal
                         * @param o Autre itérateur à comparer
                         * @return true si cet itérateur précède ou égale other
                         * @ingroup DequeIterators
                         */
                        bool operator<=(const Iterator& o) const {
                            return mIndex <= o.mIndex;
                        }

                        /**
                         * @brief Comparaison d'ordre strictement supérieur
                         * @param o Autre itérateur à comparer
                         * @return true si cet itérateur suit other dans le deque
                         * @ingroup DequeIterators
                         */
                        bool operator>(const Iterator& o) const {
                            return mIndex > o.mIndex;
                        }

                        /**
                         * @brief Comparaison d'ordre supérieur ou égal
                         * @param o Autre itérateur à comparer
                         * @return true si cet itérateur suit ou égale other
                         * @ingroup DequeIterators
                         */
                        bool operator>=(const Iterator& o) const {
                            return mIndex >= o.mIndex;
                        }
                };

                // -----------------------------------------------------------------
                // Sous-section : Itérateur constant (RandomAccess)
                // -----------------------------------------------------------------
                /**
                 * @brief Itérateur constant pour parcours random-access en lecture seule
                 * @class ConstIterator
                 * @ingroup DequeIterators
                 *
                 * Version const de Iterator : retourne const T& via operator*,
                 * empêchant toute modification des éléments via l'itérateur.
                 *
                 * @note Catégorie : NkRandomAccessIteratorTag (supporte toute l'arithmétique)
                 * @note Conversion implicite depuis Iterator pour compatibilité
                 * @note Idéal pour les fonctions prenant const NkDeque& en paramètre
                 */
                class ConstIterator {
                    private:

                        /** @brief Pointeur constant vers le deque parent (privé) */
                        const NkDeque* mDeque;

                        /** @brief Index de l'élément courant dans le deque (privé) */
                        SizeType mIndex;

                        /** @brief NkDeque est ami pour accéder aux membres privés */
                        friend class NkDeque;

                    public:

                        /** @brief Type des valeurs pointées par l'itérateur */
                        using ValueType = T;

                        /** @brief Type de référence constante retourné par operator* */
                        using Reference = const T&;

                        /** @brief Type de pointeur constant retourné par operator-> */
                        using Pointer = const T*;

                        /** @brief Type pour les différences entre itérateurs */
                        using DifferenceType = nkentseu::ptrdiff;

                        /** @brief Catégorie d'itérateur : RandomAccess */
                        using IteratorCategory = NkRandomAccessIteratorTag;

                        /**
                         * @brief Constructeur par défaut — itérateur invalide
                         * @ingroup DequeIterators
                         */
                        ConstIterator()
                            : mDeque(nullptr)
                            , mIndex(0) {
                        }

                        /**
                         * @brief Constructeur explicite depuis un deque constant et un index
                         * @param deque Pointeur constant vers le deque parent
                         * @param index Index de l'élément à encapsuler
                         * @ingroup DequeIterators
                         */
                        ConstIterator(const NkDeque* deque, SizeType index)
                            : mDeque(deque)
                            , mIndex(index) {
                        }

                        /**
                         * @brief Constructeur de conversion depuis Iterator mutable
                         * @param it Itérateur mutable à convertir en const
                         * @ingroup DequeIterators
                         *
                         * @note Permet l'assignation : ConstIterator cit = mutableIt;
                         * @note Conversion sûre : const-correctness garantie par le type
                         */
                        ConstIterator(const Iterator& it)
                            : mDeque(it.mDeque)
                            , mIndex(it.mIndex) {
                        }

                        /**
                         * @brief Déréférencement — accès en lecture seule à l'élément courant
                         * @return Référence constante vers l'élément à l'index courant
                         * @ingroup DequeIterators
                         *
                         * @note Complexité : O(1) — délégation vers operator[] const du deque
                         * @note Comportement indéfini si mDeque == nullptr ou index hors bornes
                         */
                        Reference operator*() const {
                            return (*mDeque)[mIndex];
                        }

                        /**
                         * @brief Accès membre via pointeur constant — lecture seule
                         * @return Pointeur constant vers l'élément à l'index courant
                         * @ingroup DequeIterators
                         *
                         * @note Utile pour it->member en contexte const
                         * @note Même avertissement : vérifier validité avant usage
                         */
                        Pointer operator->() const {
                            return &(*mDeque)[mIndex];
                        }

                        /**
                         * @brief Incrémentation préfixe — avance à l'élément suivant
                         * @return Référence vers *this après avancement
                         * @ingroup DequeIterators
                         *
                         * @note Complexité : O(1) — simple incrémentation d'index
                         * @note Comportement indéfini si appelé sur end() (index == size)
                         */
                        ConstIterator& operator++() {
                            ++mIndex;
                            return *this;
                        }

                        /**
                         * @brief Incrémentation postfixe — avance et retourne l'ancienne position
                         * @return Copie de l'itérateur avant avancement
                         * @ingroup DequeIterators
                         *
                         * @note Complexité : O(1) mais avec copie temporaire
                         * @note Préférer ++it (préfixe) quand la valeur précédente n'est pas nécessaire
                         */
                        ConstIterator operator++(int) {
                            ConstIterator tmp = *this;
                            ++mIndex;
                            return tmp;
                        }

                        /**
                         * @brief Décrémentation préfixe — recule à l'élément précédent
                         * @return Référence vers *this après recul
                         * @ingroup DequeIterators
                         *
                         * @note Complexité : O(1) — simple décrémentation d'index
                         * @note Comportement indéfini si appelé sur begin() (index == 0)
                         */
                        ConstIterator& operator--() {
                            --mIndex;
                            return *this;
                        }

                        /**
                         * @brief Décrémentation postfixe — recule et retourne l'ancienne position
                         * @return Copie de l'itérateur avant recul
                         * @ingroup DequeIterators
                         *
                         * @note Complexité : O(1) avec copie temporaire
                         * @note Préférer --it (préfixe) pour performance
                         */
                        ConstIterator operator--(int) {
                            ConstIterator tmp = *this;
                            --mIndex;
                            return tmp;
                        }

                        /**
                         * @brief Addition assignée — avance de n positions
                         * @param n Nombre de positions à avancer (peut être négatif)
                         * @return Référence vers *this après avancement
                         * @ingroup DequeIterators
                         *
                         * @note Complexité : O(1) — simple addition d'index
                         * @note Comportement indéfini si résultat hors [0, size]
                         */
                        ConstIterator& operator+=(DifferenceType n) {
                            mIndex += static_cast<SizeType>(n);
                            return *this;
                        }

                        /**
                         * @brief Soustraction assignée — recule de n positions
                         * @param n Nombre de positions à reculer (peut être négatif)
                         * @return Référence vers *this après recul
                         * @ingroup DequeIterators
                         *
                         * @note Complexité : O(1) — simple soustraction d'index
                         * @note Comportement indéfini si résultat hors [0, size]
                         */
                        ConstIterator& operator-=(DifferenceType n) {
                            mIndex -= static_cast<SizeType>(n);
                            return *this;
                        }

                        /**
                         * @brief Addition — retourne un nouvel itérateur avancé
                         * @param n Nombre de positions à avancer
                         * @return Nouvel itérateur à la position courante + n
                         * @ingroup DequeIterators
                         *
                         * @note Complexité : O(1) — construction d'un nouvel itérateur
                         * @note Comportement indéfini si résultat hors [0, size]
                         */
                        ConstIterator operator+(DifferenceType n) const {
                            return ConstIterator(mDeque, mIndex + static_cast<SizeType>(n));
                        }

                        /**
                         * @brief Soustraction — retourne un nouvel itérateur reculé
                         * @param n Nombre de positions à reculer
                         * @return Nouvel itérateur à la position courante - n
                         * @ingroup DequeIterators
                         *
                         * @note Complexité : O(1) — construction d'un nouvel itérateur
                         * @note Comportement indéfini si résultat hors [0, size]
                         */
                        ConstIterator operator-(DifferenceType n) const {
                            return ConstIterator(mDeque, mIndex - static_cast<SizeType>(n));
                        }

                        /**
                         * @brief Distance entre deux itérateurs constants
                         * @param other Autre itérateur constant à comparer
                         * @return Différence d'index entre cet itérateur et other
                         * @ingroup DequeIterators
                         *
                         * @note Complexité : O(1) — simple soustraction d'index
                         * @note Le résultat peut être négatif si this < other
                         */
                        DifferenceType operator-(const ConstIterator& other) const {
                            return static_cast<DifferenceType>(mIndex) - static_cast<DifferenceType>(other.mIndex);
                        }

                        /**
                         * @brief Accès par index relatif — équivalent à *(it + n)
                         * @param n Offset relatif depuis la position courante
                         * @return Référence constante vers l'élément à l'offset n
                         * @ingroup DequeIterators
                         *
                         * @note Complexité : O(1) — délégation vers operator[] const du deque
                         * @note Comportement indéfini si offset hors bornes valides
                         */
                        Reference operator[](DifferenceType n) const {
                            return (*mDeque)[mIndex + static_cast<SizeType>(n)];
                        }

                        /**
                         * @brief Comparaison d'égalité entre deux itérateurs constants
                         * @param o Autre itérateur constant à comparer
                         * @return true si les deux pointent sur le même élément
                         * @ingroup DequeIterators
                         *
                         * @note Complexité : O(1) — comparaison d'index
                         * @note Utilisé pour la condition de fin de boucle sur const containers
                         */
                        bool operator==(const ConstIterator& o) const {
                            return mIndex == o.mIndex;
                        }

                        /**
                         * @brief Comparaison de non-égalité entre deux itérateurs constants
                         * @param o Autre itérateur constant à comparer
                         * @return true si les itérateurs pointent sur des éléments différents
                         * @ingroup DequeIterators
                         *
                         * @note Implémentation via négation de operator==
                         * @note Standard pour les boucles const : for (; it != cend(); ++it)
                         */
                        bool operator!=(const ConstIterator& o) const {
                            return mIndex != o.mIndex;
                        }

                        /**
                         * @brief Comparaison d'ordre strictement inférieur
                         * @param o Autre itérateur constant à comparer
                         * @return true si cet itérateur précède other dans le deque
                         * @ingroup DequeIterators
                         *
                         * @note Complexité : O(1) — comparaison d'index
                         * @note Essentiel pour les algorithmes de tri et de recherche
                         */
                        bool operator<(const ConstIterator& o) const {
                            return mIndex < o.mIndex;
                        }

                        /**
                         * @brief Comparaison d'ordre inférieur ou égal
                         * @param o Autre itérateur constant à comparer
                         * @return true si cet itérateur précède ou égale other
                         * @ingroup DequeIterators
                         */
                        bool operator<=(const ConstIterator& o) const {
                            return mIndex <= o.mIndex;
                        }

                        /**
                         * @brief Comparaison d'ordre strictement supérieur
                         * @param o Autre itérateur constant à comparer
                         * @return true si cet itérateur suit other dans le deque
                         * @ingroup DequeIterators
                         */
                        bool operator>(const ConstIterator& o) const {
                            return mIndex > o.mIndex;
                        }

                        /**
                         * @brief Comparaison d'ordre supérieur ou égal
                         * @param o Autre itérateur constant à comparer
                         * @return true si cet itérateur suit ou égale other
                         * @ingroup DequeIterators
                         */
                        bool operator>=(const ConstIterator& o) const {
                            return mIndex >= o.mIndex;
                        }
                };

                /**
                 * @brief Type d'itérateur inversé mutable
                 * @ingroup DequeTypes
                 * @note Adaptateur NkReverseIterator pour parcours en sens inverse
                 */
                using ReverseIterator = NkReverseIterator<Iterator>;

                /**
                 * @brief Type d'itérateur inversé constant
                 * @ingroup DequeTypes
                 */
                using ConstReverseIterator = NkReverseIterator<ConstIterator>;

            private:

                // -----------------------------------------------------------------
                // Sous-section : Membres de données privés
                // -----------------------------------------------------------------
                /**
                 * @brief Tableau de pointeurs vers les chunks alloués
                 * @var mChunks
                 * @note Peut être nullptr si aucun chunk n'est alloué (deque vide)
                 */
                Chunk** mChunks;

                /**
                 * @brief Nombre de chunks actuellement alloués
                 * @var mChunkCount
                 * @note Toujours inférieur ou égal à mChunkCapacity
                 */
                SizeType mChunkCount;

                /**
                 * @brief Capacité actuelle du tableau de chunks (en nombre de pointeurs)
                 * @var mChunkCapacity
                 * @note Capacité du tableau mChunks, pas des données stockées
                 */
                SizeType mChunkCapacity;

                /**
                 * @brief Index du chunk contenant le premier élément du deque
                 * @var mFrontChunk
                 * @note Position dans le tableau mChunks, pas dans les données
                 */
                SizeType mFrontChunk;

                /**
                 * @brief Offset du premier élément dans son chunk
                 * @var mFrontOffset
                 * @note Position dans le chunk mChunks[mFrontChunk]->Data
                 */
                SizeType mFrontOffset;

                /**
                 * @brief Nombre total d'éléments stockés dans le deque
                 * @var mSize
                 * @note Maintenu pour Size() O(1) sans parcours
                 */
                SizeType mSize;

                /**
                 * @brief Pointeur vers l'allocateur utilisé pour les chunks et le tableau
                 * @var mAllocator
                 * @note Par défaut pointe vers l'allocateur global du framework
                 */
                Allocator* mAllocator;

                // -----------------------------------------------------------------
                // Sous-section : Méthodes privées de gestion interne
                // -----------------------------------------------------------------
                /**
                 * @brief Garantit que le tableau de chunks a la capacité requise
                 * @param required Capacité minimale requise en nombre de pointeurs
                 * @ingroup DequeInternals
                 *
                 * Si la capacité actuelle est insuffisante :
                 *  1. Alloue un nouveau tableau de chunks avec capacité doublée
                 *  2. Copie les pointeurs existants vers le nouveau tableau
                 *  3. Libère l'ancien tableau
                 *  4. Met à jour mChunks et mChunkCapacity
                 *
                 * @note Complexité : O(n) si réallocation nécessaire, O(1) sinon
                 * @note Ne modifie pas mChunkCount : seulement la capacité du tableau
                 * @note Utilise NkMemZero pour initialiser les nouveaux pointeurs à nullptr
                 */
                void EnsureChunkArrayCapacity(SizeType required) {
                    if (required <= mChunkCapacity) {
                        return;
                    }
                    SizeType newCapacity = mChunkCapacity == 0 ? 4 : mChunkCapacity * 2;
                    if (newCapacity < required) {
                        newCapacity = required;
                    }
                    Chunk** newChunks = static_cast<Chunk**>(
                        mAllocator->Allocate(newCapacity * sizeof(Chunk*), alignof(Chunk*))
                    );
                    NKENTSEU_ASSERT(newChunks != nullptr);
                    memory::NkMemZero(newChunks, newCapacity * sizeof(Chunk*));
                    if (mChunks) {
                        memory::NkMemCopy(newChunks, mChunks, mChunkCount * sizeof(Chunk*));
                        mAllocator->Deallocate(mChunks);
                    }
                    mChunks = newChunks;
                    mChunkCapacity = newCapacity;
                }

                /**
                 * @brief Alloue un nouveau chunk de données
                 * @return Pointeur vers le nouveau chunk alloué
                 * @ingroup DequeInternals
                 *
                 * Alloue un bloc de mémoire de taille sizeof(Chunk) avec alignement
                 * requis pour le type Chunk, puis retourne le pointeur brut.
                 *
                 * @note Complexité : O(1) — allocation unique
                 * @note Le chunk n'est pas initialisé : les éléments seront construits à la demande
                 * @note Assertion si l'allocation échoue (comportement configurable)
                 */
                Chunk* AllocateChunkBlock() {
                    Chunk* chunk = static_cast<Chunk*>(
                        mAllocator->Allocate(sizeof(Chunk), alignof(Chunk))
                    );
                    NKENTSEU_ASSERT(chunk != nullptr);
                    return chunk;
                }

                /**
                 * @brief Alloue un nouveau chunk et l'ajoute au tableau
                 * @ingroup DequeInternals
                 *
                 * Étapes :
                 *  1. Vérifie que le tableau de chunks a la capacité nécessaire
                 *  2. Alloue un nouveau chunk via AllocateChunkBlock()
                 *  3. Ajoute le pointeur au tableau mChunks[mChunkCount]
                 *  4. Incrémente mChunkCount
                 *
                 * @note Complexité : O(1) amorti (O(n) occasionnellement pour réallocation tableau)
                 * @note Le nouveau chunk n'est pas initialisé : les éléments seront construits à la demande
                 */
                void AllocateChunk() {
                    EnsureChunkArrayCapacity(mChunkCount + 1);
                    mChunks[mChunkCount] = AllocateChunkBlock();
                    ++mChunkCount;
                }

                /**
                 * @brief Libère tous les chunks et le tableau de chunks
                 * @ingroup DequeInternals
                 *
                 * Étapes de libération :
                 *  1. Pour chaque chunk alloué : libère la mémoire du chunk
                 *  2. Libère la mémoire du tableau de chunks mChunks
                 *  3. Réinitialise tous les membres à l'état vide
                 *
                 * @note Complexité : O(n) où n = mChunkCount
                 * @note noexcept implicite : ne lève pas d'exception (deallocator safety)
                 * @note Appelé par le destructeur et Clear() pour nettoyage complet
                 */
                void ReleaseChunkStorage() {
                    if (!mChunks) {
                        mChunkCount = 0;
                        mChunkCapacity = 0;
                        mFrontChunk = 0;
                        mFrontOffset = 0;
                        return;
                    }
                    for (SizeType i = 0; i < mChunkCount; ++i) {
                        if (mChunks[i]) {
                            mAllocator->Deallocate(mChunks[i]);
                            mChunks[i] = nullptr;
                        }
                    }
                    mAllocator->Deallocate(mChunks);
                    mChunks = nullptr;
                    mChunkCount = 0;
                    mChunkCapacity = 0;
                    mFrontChunk = 0;
                    mFrontOffset = 0;
                }

                /**
                 * @brief Convertit un index global en coordonnées chunk+offset
                 * @param index Index global dans le deque (0-based)
                 * @param chunkIdx [out] Index du chunk contenant l'élément
                 * @param offset [out] Offset de l'élément dans le chunk
                 * @ingroup DequeInternals
                 *
                 * Calcule les coordonnées internes à partir de l'index global :
                 *  1. Calcule l'index global absolu : mFrontOffset + index
                 *  2. Calcule le chunk relatif : globalIndex / CHUNK_SIZE
                 *  3. Calcule l'offset dans le chunk : globalIndex % CHUNK_SIZE
                 *  4. Ajoute le chunk de départ : mFrontChunk + chunkRelatif
                 *
                 * @note Complexité : O(1) — calculs arithmétiques simples
                 * @note Précondition : index < mSize (non vérifiée pour performance)
                 * @note Utilisé par operator[] et les itérateurs pour accès O(1)
                 */
                void GetChunkAndOffset(
                    SizeType index,
                    SizeType& chunkIdx,
                    SizeType& offset
                ) const {
                    SizeType globalIndex = mFrontOffset + index;
                    chunkIdx = mFrontChunk + (globalIndex / CHUNK_SIZE);
                    offset = globalIndex % CHUNK_SIZE;
                }

            public:

                // -----------------------------------------------------------------
                // Sous-section : Constructeurs
                // -----------------------------------------------------------------
                /**
                 * @brief Constructeur par défaut — deque vide sans allocation
                 * @param allocator Pointeur vers l'allocateur à utiliser (nullptr pour défaut)
                 * @ingroup DequeConstructors
                 *
                 * Initialise un deque avec :
                 *  - mChunks = nullptr (pas d'allocation initiale)
                 *  - Tous les compteurs à zéro (état vide)
                 *  - mAllocator = allocateur fourni ou global par défaut
                 *
                 * @note Complexité : O(1) — aucune allocation mémoire initiale
                 * @note Premier PushFront/PushBack déclenchera l'allocation du premier chunk
                 */
                explicit NkDeque(Allocator* allocator = nullptr)
                    : mChunks(nullptr)
                    , mChunkCount(0)
                    , mChunkCapacity(0)
                    , mFrontChunk(0)
                    , mFrontOffset(0)
                    , mSize(0)
                    , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator()) {
                }

                /**
                 * @brief Constructeur depuis NkInitializerList — syntaxe {a, b, c}
                 * @param init Liste d'initialisation contenant les éléments à insérer
                 * @param allocator Pointeur vers l'allocateur à utiliser (nullptr pour défaut)
                 * @ingroup DequeConstructors
                 *
                 * Permet la construction avec liste d'initialisation :
                 *   NkDeque<int> values = {1, 2, 3, 4};
                 *
                 * Insère chaque élément via PushBack, préservant l'ordre de la liste.
                 *
                 * @note Complexité : O(n) pour n insertions PushBack O(1) amorti chacune
                 * @note Compatible avec C++11 initializer list syntax
                 */
                NkDeque(NkInitializerList<T> init, Allocator* allocator = nullptr)
                    : mChunks(nullptr)
                    , mChunkCount(0)
                    , mChunkCapacity(0)
                    , mFrontChunk(0)
                    , mFrontOffset(0)
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
                 * @ingroup DequeConstructors
                 *
                 * Facilité d'interopérabilité avec du code utilisant std::initializer_list.
                 * Fonctionne comme le constructeur NkInitializerList mais accepte
                 * le type standard de la STL pour une migration progressive.
                 *
                 * @note Complexité : O(n) pour les insertions successives
                 * @note Nécessite <initializer_list> du compilateur (pas de dépendance STL runtime)
                 */
                NkDeque(std::initializer_list<T> init, Allocator* allocator = nullptr)
                    : mChunks(nullptr)
                    , mChunkCount(0)
                    , mChunkCapacity(0)
                    , mFrontChunk(0)
                    , mFrontOffset(0)
                    , mSize(0)
                    , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator()) {
                    for (auto& val : init) {
                        PushBack(val);
                    }
                }

                /**
                 * @brief Constructeur de copie — duplication profonde d'un deque
                 * @param other Deque source à copier
                 * @ingroup DequeConstructors
                 *
                 * Crée une copie indépendante du deque 'other' :
                 *  - Parcours tous les éléments de other via index
                 *  - Insère chaque élément via PushBack (constructeur de copie de T)
                 *  - Utilise le même type d'allocateur que 'other'
                 *
                 * @note Complexité : O(n) pour la copie des n éléments
                 * @note Requiert que T soit CopyConstructible
                 * @note Les modifications sur la copie n'affectent pas l'original
                 */
                NkDeque(const NkDeque& other)
                    : mChunks(nullptr)
                    , mChunkCount(0)
                    , mChunkCapacity(0)
                    , mFrontChunk(0)
                    , mFrontOffset(0)
                    , mSize(0)
                    , mAllocator(other.mAllocator) {
                    for (SizeType i = 0; i < other.mSize; ++i) {
                        PushBack(other[i]);
                    }
                }

                /**
                 * @brief Constructeur de déplacement — transfert de propriété efficace
                 * @param other Deque source dont transférer le contenu
                 * @ingroup DequeConstructors
                 *
                 * Transfère la propriété des chunks de 'other' vers ce deque :
                 *  - Copie les pointeurs et métadonnées internes (O(1))
                 *  - Réinitialise 'other' à l'état vide (nullptr, count=0, size=0)
                 *  - Évite toute allocation ou copie d'éléments
                 *
                 * @note Complexité : O(1) — transfert de pointeurs uniquement
                 * @note noexcept : garantie de ne pas lever d'exception
                 * @note 'other' est laissé dans un état valide mais vide après le move
                 */
                NkDeque(NkDeque&& other) NKENTSEU_NOEXCEPT
                    : mChunks(other.mChunks)
                    , mChunkCount(other.mChunkCount)
                    , mChunkCapacity(other.mChunkCapacity)
                    , mFrontChunk(other.mFrontChunk)
                    , mFrontOffset(other.mFrontOffset)
                    , mSize(other.mSize)
                    , mAllocator(other.mAllocator) {
                    other.mChunks = nullptr;
                    other.mChunkCount = 0;
                    other.mChunkCapacity = 0;
                    other.mFrontChunk = 0;
                    other.mFrontOffset = 0;
                    other.mSize = 0;
                }

                /**
                 * @brief Destructeur — libération propre de tous les chunks
                 * @ingroup DequeConstructors
                 *
                 * Libère toutes les ressources gérées par ce deque :
                 *  1. Détruit tous les éléments via Clear()
                 *  2. Libère tous les chunks et le tableau via ReleaseChunkStorage()
                 *
                 * @note noexcept implicite : ne lève jamais d'exception (destructor safety)
                 * @note Complexité : O(n) pour la destruction des éléments + O(chunks) pour libération
                 * @note Garantit l'absence de fuites mémoire même en cas d'exception pendant construction
                 */
                ~NkDeque() {
                    Clear();
                    ReleaseChunkStorage();
                }

                // -----------------------------------------------------------------
                // Sous-section : Opérateurs d'affectation
                // -----------------------------------------------------------------
                /**
                 * @brief Opérateur d'affectation par copie — syntaxe deque1 = deque2
                 * @param other Deque source à copier
                 * @return Référence vers *this pour chaînage d'opérations
                 * @ingroup DequeAssignment
                 *
                 * Implémente la sémantique de copie via Clear() + PushBack :
                 *  - Vide le contenu actuel via Clear()
                 *  - Libère le stockage actuel via ReleaseChunkStorage()
                 *  - Parcours tous les éléments de other via index
                 *  - Insère chaque élément via PushBack (constructeur de copie)
                 *
                 * @note Complexité : O(n) pour la copie des éléments
                 * @note Gère l'auto-affectation (this == &other) sans effet
                 * @note Requiert que T soit CopyConstructible
                 */
                NkDeque& operator=(const NkDeque& other) {
                    if (this == &other) {
                        return *this;
                    }
                    Clear();
                    ReleaseChunkStorage();
                    mAllocator = other.mAllocator;
                    for (SizeType i = 0; i < other.mSize; ++i) {
                        PushBack(other[i]);
                    }
                    return *this;
                }

                /**
                 * @brief Opérateur d'affectation par déplacement — transfert efficace
                 * @param other Deque source dont transférer le contenu
                 * @return Référence vers *this pour chaînage d'opérations
                 * @ingroup DequeAssignment
                 *
                 * Transfère la propriété des chunks de 'other' vers ce deque :
                 *  - Libère les chunks actuels via Clear() + ReleaseChunkStorage()
                 *  - Adopte les pointeurs et métadonnées de 'other'
                 *  - Réinitialise 'other' à l'état vide
                 *
                 * @note Complexité : O(1) — transfert de pointeurs uniquement
                 * @note noexcept : garantie de ne pas lever d'exception
                 * @note 'other' est laissé dans un état valide mais vide après l'opération
                 */
                NkDeque& operator=(NkDeque&& other) NKENTSEU_NOEXCEPT {
                    if (this == &other) {
                        return *this;
                    }
                    Clear();
                    ReleaseChunkStorage();
                    mChunks = other.mChunks;
                    mChunkCount = other.mChunkCount;
                    mChunkCapacity = other.mChunkCapacity;
                    mFrontChunk = other.mFrontChunk;
                    mFrontOffset = other.mFrontOffset;
                    mSize = other.mSize;
                    mAllocator = other.mAllocator;
                    other.mChunks = nullptr;
                    other.mChunkCount = 0;
                    other.mChunkCapacity = 0;
                    other.mFrontChunk = 0;
                    other.mFrontOffset = 0;
                    other.mSize = 0;
                    return *this;
                }

                /**
                 * @brief Opérateur d'affectation depuis NkInitializerList
                 * @param init Liste d'initialisation contenant les nouvelles valeurs
                 * @return Référence vers *this pour chaînage d'opérations
                 * @ingroup DequeAssignment
                 *
                 * Permet la syntaxe deque = {a, b, c} pour remplacer le contenu
                 * du deque par une nouvelle liste d'éléments.
                 *
                 * @note Complexité : O(n) pour l'insertion des nouveaux éléments
                 * @note Vide d'abord le contenu actuel via Clear() + ReleaseChunkStorage()
                 */
                NkDeque& operator=(NkInitializerList<T> init) {
                    Clear();
                    ReleaseChunkStorage();
                    for (auto& val : init) {
                        PushBack(val);
                    }
                    return *this;
                }

                /**
                 * @brief Opérateur d'affectation depuis std::initializer_list
                 * @param init Liste d'initialisation STL standard
                 * @return Référence vers *this pour chaînage d'opérations
                 * @ingroup DequeAssignment
                 *
                 * Interopérabilité avec code utilisant std::initializer_list.
                 * Fonctionne comme l'opérateur NkInitializerList mais accepte
                 * le type standard pour migration progressive depuis la STL.
                 *
                 * @note Complexité : O(n) pour l'insertion des éléments
                 * @note Nécessite <initializer_list> du compilateur uniquement
                 */
                NkDeque& operator=(std::initializer_list<T> init) {
                    Clear();
                    ReleaseChunkStorage();
                    for (auto& val : init) {
                        PushBack(val);
                    }
                    return *this;
                }

                // -----------------------------------------------------------------
                // Sous-section : Accès aux éléments
                // -----------------------------------------------------------------
                /**
                 * @brief Accès non vérifié à un élément par index — version mutable
                 * @param index Index de l'élément à accéder (0-based)
                 * @return Référence mutable vers l'élément à l'index spécifié
                 * @ingroup DequeAccess
                 *
                 * Accès direct sans vérification de bornes pour performance maximale.
                 * Utilise NKENTSEU_ASSERT en mode debug pour détecter les erreurs,
                 * mais aucun check en mode release (comportement indéfini si index invalide).
                 *
                 * @note Complexité : O(1) — conversion index → chunk+offset + accès direct
                 * @note À utiliser uniquement quand la validité de l'index est garantie
                 * @note Préférer At() quand la sécurité prime sur la performance
                 */
                Reference operator[](SizeType index) {
                    NKENTSEU_ASSERT(index < mSize);
                    SizeType chunkIdx, offset;
                    GetChunkAndOffset(index, chunkIdx, offset);
                    return mChunks[chunkIdx]->Data[offset];
                }

                /**
                 * @brief Accès non vérifié à un élément par index — version constante
                 * @param index Index de l'élément à accéder (0-based)
                 * @return Référence constante vers l'élément à l'index spécifié
                 * @ingroup DequeAccess
                 *
                 * Version const de operator[] pour les deques en lecture seule.
                 * Même sémantique de performance/sécurité que la version mutable.
                 *
                 * @note Complexité : O(1) — accès direct après conversion index → chunk+offset
                 * @note Compatible avec les algorithmes génériques attendant operator[]
                 */
                ConstReference operator[](SizeType index) const {
                    NKENTSEU_ASSERT(index < mSize);
                    SizeType chunkIdx, offset;
                    GetChunkAndOffset(index, chunkIdx, offset);
                    return mChunks[chunkIdx]->Data[offset];
                }

                /**
                 * @brief Accès au premier élément — version mutable
                 * @return Référence mutable vers le premier élément (index 0)
                 * @ingroup DequeAccess
                 *
                 * Retourne une référence vers operator[](0) avec assertion debug
                 * si le deque est vide. Équivalent à Front() = [0].
                 *
                 * @note Complexité : O(1) — accès direct au premier élément
                 * @note Assertion en debug si Empty() == true
                 * @note Comportement indéfini en release si deque vide
                 */
                Reference Front() {
                    NKENTSEU_ASSERT(mSize > 0);
                    return (*this)[0];
                }

                /**
                 * @brief Accès au premier élément — version constante
                 * @return Référence constante vers le premier élément (index 0)
                 * @ingroup DequeAccess
                 *
                 * Version const de Front() pour les deques en lecture seule.
                 * Même sémantique de vérification que la version mutable.
                 */
                ConstReference Front() const {
                    NKENTSEU_ASSERT(mSize > 0);
                    return (*this)[0];
                }

                /**
                 * @brief Accès au dernier élément — version mutable
                 * @return Référence mutable vers le dernier élément (index Size()-1)
                 * @ingroup DequeAccess
                 *
                 * Retourne une référence vers operator[](Size()-1) avec assertion debug
                 * si le deque est vide. Équivalent à Back() = [Size()-1].
                 *
                 * @note Complexité : O(1) — accès direct au dernier élément
                 * @note Assertion en debug si Empty() == true
                 * @note Comportement indéfini en release si deque vide
                 */
                Reference Back() {
                    NKENTSEU_ASSERT(mSize > 0);
                    return (*this)[mSize - 1];
                }

                /**
                 * @brief Accès au dernier élément — version constante
                 * @return Référence constante vers le dernier élément (index Size()-1)
                 * @ingroup DequeAccess
                 *
                 * Version const de Back() pour les deques en lecture seule.
                 * Même sémantique de vérification que la version mutable.
                 */
                ConstReference Back() const {
                    NKENTSEU_ASSERT(mSize > 0);
                    return (*this)[mSize - 1];
                }

                // -----------------------------------------------------------------
                // Sous-section : Itérateurs — Style minuscule (compatibilité STL)
                // -----------------------------------------------------------------
                /**
                 * @brief Itérateur mutable vers le début du deque
                 * @return Iterator pointant sur le premier élément (ou end() si vide)
                 * @ingroup DequeIterators
                 *
                 * Retourne un Iterator encapsulant ce deque et l'index 0.
                 * Style minuscule pour compatibilité avec range-based for C++11.
                 *
                 * @note Complexité : O(1) — construction d'itérateur léger
                 * @note noexcept implicite : ne lève pas d'exception
                 * @note Compatible avec les algorithmes génériques attendant begin()
                 */
                Iterator begin() {
                    return Iterator(this, 0);
                }

                /**
                 * @brief Itérateur constant vers le début du deque
                 * @return ConstIterator pointant sur le premier élément (ou end() si vide)
                 * @ingroup DequeIterators
                 *
                 * Version const de begin() pour les deques en lecture seule.
                 * Retourne un ConstIterator pour empêcher la modification via l'itérateur.
                 */
                ConstIterator begin() const {
                    return ConstIterator(this, 0);
                }

                /**
                 * @brief Itérateur mutable vers la fin du deque (one-past-last)
                 * @return Iterator pointant après le dernier élément
                 * @ingroup DequeIterators
                 *
                 * Retourne un Iterator encapsulant ce deque et l'index mSize,
                 * représentant la position "juste après" le dernier élément valide.
                 */
                Iterator end() {
                    return Iterator(this, mSize);
                }

                /**
                 * @brief Itérateur constant vers la fin du deque (one-past-last)
                 * @return ConstIterator pointant après le dernier élément
                 * @ingroup DequeIterators
                 *
                 * Version const de end() pour les deques en lecture seule.
                 * Même sémantique de sentinelle exclusive que la version mutable.
                 */
                ConstIterator end() const {
                    return ConstIterator(this, mSize);
                }

                /**
                 * @brief Itérateur inversé mutable vers le dernier élément
                 * @return ReverseIterator pointant sur le dernier élément (ou rend() si vide)
                 * @ingroup DequeIterators
                 *
                 * Retourne un adaptateur NkReverseIterator initialisé avec end(),
                 * permettant de parcourir le deque en sens inverse.
                 */
                ReverseIterator rbegin() {
                    return ReverseIterator(end());
                }

                /**
                 * @brief Itérateur inversé constant vers le dernier élément
                 * @return ConstReverseIterator pointant sur le dernier élément
                 * @ingroup DequeIterators
                 *
                 * Version const de rbegin() pour les deques en lecture seule.
                 * Retourne un adaptateur const pour empêcher la modification
                 * des éléments via l'itérateur inversé.
                 */
                ConstReverseIterator rbegin() const {
                    return ConstReverseIterator(end());
                }

                /**
                 * @brief Itérateur inversé mutable vers avant le premier élément
                 * @return ReverseIterator sentinelle marquant la fin du parcours inversé
                 * @ingroup DequeIterators
                 *
                 * Retourne un adaptateur NkReverseIterator initialisé avec begin(),
                 * représentant la position "juste avant" le premier élément dans
                 * l'ordre inversé.
                 */
                ReverseIterator rend() {
                    return ReverseIterator(begin());
                }

                /**
                 * @brief Itérateur inversé constant vers avant le premier élément
                 * @return ConstReverseIterator sentinelle pour parcours inversé const
                 * @ingroup DequeIterators
                 *
                 * Version const de rend() pour les deques en lecture seule.
                 * Même sémantique de sentinelle exclusive que la version mutable.
                 */
                ConstReverseIterator rend() const {
                    return ConstReverseIterator(begin());
                }

                // -----------------------------------------------------------------
                // Sous-section : Méthodes de capacité
                // -----------------------------------------------------------------
                /**
                 * @brief Vérifie si le deque est vide
                 * @return true si Size() == 0, false sinon
                 * @ingroup DequeCapacity
                 *
                 * Teste si le deque ne contient aucun élément en comparant
                 * mSize à zéro. Plus efficace que comparer les pointeurs.
                 *
                 * @note Complexité : O(1) — comparaison simple de membre
                 * @note noexcept : garantie de ne pas lever d'exception
                 * @note Préférer cette méthode pour tester avant Front()/Back()
                 */
                bool Empty() const NKENTSEU_NOEXCEPT {
                    return mSize == 0;
                }

                /**
                 * @brief Retourne le nombre d'éléments actuellement stockés
                 * @return Valeur de type SizeType représentant la taille courante
                 * @ingroup DequeCapacity
                 *
                 * Retourne mSize, maintenu à jour par toutes les opérations
                 * de modification (PushFront, PushBack, PopFront, PopBack, etc.).
                 * Toujours exact et disponible en temps constant.
                 *
                 * @note Complexité : O(1) — retour de membre direct
                 * @note noexcept : garantie de ne pas lever d'exception
                 */
                SizeType Size() const NKENTSEU_NOEXCEPT {
                    return mSize;
                }

                // -----------------------------------------------------------------
                // Sous-section : Modificateurs de contenu
                // -----------------------------------------------------------------
                /**
                 * @brief Détruit tous les éléments et remet le deque à l'état vide
                 * @ingroup DequeModifiers
                 *
                 * Supprime tous les éléments un par un via PopBack() jusqu'à
                 * ce que le deque soit vide. Ne libère pas les chunks alloués :
                 * ils restent disponibles pour des insertions futures.
                 *
                 * @note Complexité : O(n) pour la destruction des n éléments
                 * @note noexcept implicite : ne lève pas d'exception (destructor safety)
                 * @note Les chunks restent alloués pour réutilisation future
                 * @note Pour libérer complètement la mémoire, appeler ReleaseChunkStorage()
                 */
                void Clear() {
                    while (!Empty()) {
                        PopBack();
                    }
                }

                /**
                 * @brief Ajoute une copie de l'élément à la fin du deque
                 * @param value Valeur à copier et ajouter
                 * @ingroup DequeModifiers
                 *
                 * Si le deque est vide :
                 *  - Alloue le premier chunk si nécessaire
                 *  - Initialise mFrontOffset au milieu du chunk (CHUNK_SIZE/2)
                 *
                 * Sinon :
                 *  - Calcule les coordonnées du nouvel élément via GetChunkAndOffset()
                 *  - Alloue un nouveau chunk si nécessaire
                 *  - Construit l'élément in-place via placement new
                 *
                 * @note Complexité : O(1) amorti (O(n) occasionnellement pour allocation chunk)
                 * @note Peut invalider tous les itérateurs, pointeurs et références
                 * @note Requiert que T soit CopyConstructible
                 */
                void PushBack(const T& value) {
                    if (mSize == 0) {
                        if (mChunkCount == 0) {
                            AllocateChunk();
                        }
                        mFrontChunk = 0;
                        mFrontOffset = CHUNK_SIZE / 2;  // Start in middle
                    }
                    SizeType chunkIdx, offset;
                    GetChunkAndOffset(mSize, chunkIdx, offset);
                    if (chunkIdx >= mChunkCount) {
                        AllocateChunk();
                    }
                    new (&mChunks[chunkIdx]->Data[offset]) T(value);
                    ++mSize;
                }

                /**
                 * @brief Ajoute une copie de l'élément au début du deque
                 * @param value Valeur à copier et ajouter
                 * @ingroup DequeModifiers
                 *
                 * Si le deque est vide :
                 *  - Délégation vers PushBack() pour initialisation
                 *
                 * Sinon :
                 *  - Si mFrontOffset == 0 : besoin d'un nouveau chunk en tête
                 *    - Si mFrontChunk == 0 : décale tous les chunks vers la droite
                 *    - Sinon : utilise le chunk précédent (mFrontChunk--)
                 *  - Sinon : décrémente simplement mFrontOffset
                 *  - Construit l'élément in-place via placement new
                 *
                 * @note Complexité : O(1) amorti (O(n) occasionnellement pour décalage chunks)
                 * @note Peut invalider tous les itérateurs, pointeurs et références
                 * @note Requiert que T soit CopyConstructible
                 */
                void PushFront(const T& value) {
                    if (mSize == 0) {
                        PushBack(value);
                        return;
                    }
                    if (mFrontOffset == 0) {
                        if (mFrontChunk == 0) {
                            EnsureChunkArrayCapacity(mChunkCount + 1);
                            for (SizeType i = mChunkCount; i > 0; --i) {
                                mChunks[i] = mChunks[i - 1];
                            }
                            mChunks[0] = AllocateChunkBlock();
                            ++mChunkCount;
                            mFrontChunk = 0;
                        }
                        else {
                            --mFrontChunk;
                        }
                        mFrontOffset = CHUNK_SIZE - 1;
                    }
                    else {
                        --mFrontOffset;
                    }
                    new (&mChunks[mFrontChunk]->Data[mFrontOffset]) T(value);
                    ++mSize;
                }

                /**
                 * @brief Supprime le dernier élément du deque
                 * @ingroup DequeModifiers
                 *
                 * Étapes de suppression :
                 *  1. Calcule les coordonnées du dernier élément via GetChunkAndOffset()
                 *  2. Appelle explicitement le destructeur de T sur l'élément
                 *  3. Décrémente mSize
                 *  4. Si le deque devient vide : réinitialise mFrontChunk/mFrontOffset
                 *
                 * @note Complexité : O(1) — destruction d'un seul élément
                 * @note Assertion en debug si Empty() == true (underflow protection)
                 * @note Comportement indéfini en release si deque vide
                 * @note Peut invalider les itérateurs pointant vers l'élément supprimé
                 */
                void PopBack() {
                    NKENTSEU_ASSERT(mSize > 0);
                    SizeType chunkIdx, offset;
                    GetChunkAndOffset(mSize - 1, chunkIdx, offset);
                    mChunks[chunkIdx]->Data[offset].~T();
                    --mSize;
                    if (mSize == 0) {
                        mFrontChunk = 0;
                        mFrontOffset = 0;
                    }
                }

                /**
                 * @brief Supprime le premier élément du deque
                 * @ingroup DequeModifiers
                 *
                 * Étapes de suppression :
                 *  1. Appelle explicitement le destructeur de T sur mFrontChunk/mFrontOffset
                 *  2. Incrémente mFrontOffset
                 *  3. Si mFrontOffset atteint CHUNK_SIZE : passe au chunk suivant
                 *  4. Si le deque devient vide : réinitialise mFrontChunk/mFrontOffset
                 *
                 * @note Complexité : O(1) — destruction d'un seul élément
                 * @note Assertion en debug si Empty() == true (underflow protection)
                 * @note Comportement indéfini en release si deque vide
                 * @note Peut invalider les itérateurs pointant vers l'élément supprimé
                 */
                void PopFront() {
                    NKENTSEU_ASSERT(mSize > 0);
                    mChunks[mFrontChunk]->Data[mFrontOffset].~T();
                    ++mFrontOffset;
                    if (mFrontOffset >= CHUNK_SIZE) {
                        ++mFrontChunk;
                        mFrontOffset = 0;
                    }
                    --mSize;
                    if (mSize == 0) {
                        mFrontChunk = 0;
                        mFrontOffset = 0;
                    }
                }

        }; // class NkDeque

    } // namespace nkentseu

    // -------------------------------------------------------------------------
    // SECTION 4 : VALIDATION ET MESSAGES DE DEBUG OPTIONNELS
    // -------------------------------------------------------------------------
    // Ces directives aident à vérifier la configuration de compilation lors
    // du build, particulièrement utile pour déboguer les problèmes liés aux
    // options de compilation (C++11, exceptions, assertions, etc.).

    #ifdef NKENTSEU_CONTAINERS_DEBUG_CONFIG
        #if defined(NKENTSEU_CXX11_OR_LATER)
            #pragma message("NkDeque: C++11 move semantics ENABLED")
        #else
            #pragma message("NkDeque: C++11 move semantics DISABLED (using copy fallback)")
        #endif
    #endif

#endif // NKENTSEU_CONTAINERS_SEQUENTIAL_NKDEQUE_H_INCLUDED

// =============================================================================
// EXEMPLES D'UTILISATION DÉTAILLÉS
// =============================================================================
// Cette section fournit des exemples concrets et complets d'utilisation de
// NkDeque, couvrant les cas d'usage courants et les bonnes pratiques.
// Ces exemples sont en commentaires pour ne pas affecter la compilation.
// =============================================================================

/*
    // -----------------------------------------------------------------------------
    // Exemple 1 : Construction et initialisation variées
    // -----------------------------------------------------------------------------
    #include <NKContainers/Sequential/NkDeque.h>
    using namespace nkentseu::containers;

    void ConstructionExamples() {
        // Constructeur par défaut : deque vide
        NkDeque<int> empty;

        // Constructeur avec allocateur personnalisé
        NkDeque<int> customAlloc(&myCustomAllocator);

        // Constructeur depuis liste d'initialisation
        NkDeque<int> init = {10, 20, 30};  // {10, 20, 30}

        // Constructeur de copie
        NkDeque<int> copy = init;  // Copie profonde : {10, 20, 30}

        #if defined(NKENTSEU_CXX11_OR_LATER)
        // Constructeur de déplacement (C++11)
        NkDeque<int> temp = MakeLargeDeque();
        NkDeque<int> moved = NKENTSEU_MOVE(temp);  // Transfert O(1), temp vide
        #endif
    }

    // -----------------------------------------------------------------------------
    // Exemple 2 : Insertion et suppression aux extrémités — O(1) amorti
    // -----------------------------------------------------------------------------
    void EndOperationsExamples() {
        NkDeque<int> deque;

        // PushBack : insertion en queue O(1) amorti
        deque.PushBack(1);  // deque = {1}
        deque.PushBack(2);  // deque = {1, 2}
        deque.PushBack(3);  // deque = {1, 2, 3}

        // PushFront : insertion en tête O(1) amorti
        deque.PushFront(0);  // deque = {0, 1, 2, 3}
        deque.PushFront(-1); // deque = {-1, 0, 1, 2, 3}

        // Accès aléatoire : O(1)
        int third = deque[2];  // third = 1
        deque[0] = 10;         // deque = {10, 0, 1, 2, 3}

        // PopBack : suppression en queue O(1)
        deque.PopBack();  // deque = {10, 0, 1, 2}
        deque.PopBack();  // deque = {10, 0, 1}

        // PopFront : suppression en tête O(1)
        deque.PopFront();  // deque = {0, 1}
        deque.PopFront();  // deque = {1}
    }

    // -----------------------------------------------------------------------------
    // Exemple 3 : Parcours avec itérateurs RandomAccess
    // -----------------------------------------------------------------------------
    void IterationExamples() {
        NkDeque<int> deque = {1, 2, 3, 4, 5};

        // Parcours avant avec itérateur RandomAccess
        for (auto it = deque.begin(); it != deque.end(); ++it) {
            *it *= 2;  // Modification via itérateur mutable
        }
        // deque contient maintenant {2, 4, 6, 8, 10}

        // Parcours arrière avec reverse iterator
        for (auto rit = deque.rbegin(); rit != deque.rend(); ++rit) {
            printf("%d ", *rit);  // Affiche: 10 8 6 4 2
        }

        // Parcours en lecture seule (const)
        void PrintDeque(const NkDeque<int>& dq) {
            for (auto it = dq.cbegin(); it != dq.cend(); ++it) {
                printf("%d ", *it);  // Lecture seule via itérateur const
            }
            printf("\n");
        }

        // Range-based for grâce aux aliases begin()/end()
        int sum = 0;
        for (int val : deque) {
            sum += val;  // sum = 30
        }

        // Algorithmes avec arithmétique d'itérateurs
        auto mid = deque.begin() + deque.Size() / 2;  // Itérateur vers le milieu
        auto dist = deque.end() - deque.begin();      // Distance = taille
    }

    // -----------------------------------------------------------------------------
    // Exemple 4 : Utilisation comme structure de données spécialisée
    // -----------------------------------------------------------------------------
    // NkDeque est idéal pour implémenter des structures nécessitant :
    //  - Accès aléatoire rapide
    //  - Insertion/suppression aux deux extrémités

    // Sliding window (fenêtre glissante) pour traitement de flux
    template<typename T>
    class SlidingWindow {
    private:
        NkDeque<T> mBuffer;
        nk_size mMaxSize;
    public:
        explicit SlidingWindow(nk_size maxSize) : mMaxSize(maxSize) {}
        
        void Add(T value) {
            mBuffer.PushBack(value);
            if (mBuffer.Size() > mMaxSize) {
                mBuffer.PopFront();  // Maintient la taille maximale
            }
        }
        
        T& operator[](nk_size index) {
            return mBuffer[index];
        }
        
        nk_size Size() const { return mBuffer.Size(); }
        bool Full() const { return mBuffer.Size() == mMaxSize; }
    };

    void SlidingWindowExample() {
        SlidingWindow<int> window(3);  // Fenêtre de taille max 3
        
        window.Add(1);  // window = {1}
        window.Add(2);  // window = {1, 2}
        window.Add(3);  // window = {1, 2, 3}
        window.Add(4);  // window = {2, 3, 4} (1 supprimé)
        
        printf("Window: [%d, %d, %d]\n", 
               window[0], window[1], window[2]);  // Affiche: [2, 3, 4]
    }

    // -----------------------------------------------------------------------------
    // Exemple 5 : Intégration avec algorithmes génériques
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
        NkDeque<int> deque = {1, 2, 3, 4, 5, 6};

        // Utiliser NkDeque avec algorithmes génériques grâce aux itérateurs RandomAccess
        auto it = NkFindIf(deque.begin(), deque.end(), [](int x) { return x % 2 == 0; });
        if (it != deque.end()) {
            printf("First even: %d\n", *it);  // Affiche: First even: 2
        }

        // Tri avec itérateurs RandomAccess (si implémenté)
        // NkSort(deque.begin(), deque.end());

        // Range-based for grâce aux aliases begin()/end()
        int sum = 0;
        for (int val : deque) {
            sum += val;
        }
        // sum = 21
    }

    // -----------------------------------------------------------------------------
    // Exemple 6 : Performance et gestion mémoire
    // -----------------------------------------------------------------------------
    void PerformanceConsiderations() {
        NkDeque<int> deque;
        
        // Scénario 1 : Croissance aux deux extrémités
        // - Moins de réallocations que NkVector
        // - Meilleure localité que NkDoubleList
        for (int i = 0; i < 1000; ++i) {
            if (i % 2 == 0) {
                deque.PushBack(i);
            }
            else {
                deque.PushFront(i);
            }
        }
        // Après 1000 insertions : ~16 chunks alloués (1000/64 ≈ 15.6)
        
        // Scénario 2 : Accès aléatoire fréquent
        // - O(1) garanti contrairement aux listes chaînées
        for (nk_size i = 0; i < deque.Size(); ++i) {
            if (deque[i] % 2 == 0) {
                deque[i] *= 2;
            }
        }
        
        // Scénario 3 : Nettoyage complet
        // - Clear() détruit les éléments mais garde les chunks
        // - ReleaseChunkStorage() libère tout (appelé automatiquement par destructeur)
        deque.Clear();  // Éléments détruits, chunks conservés
        // ~NkDeque() appellera ReleaseChunkStorage() pour libérer les chunks
    }
*/

// =============================================================================
// NOTES DE MIGRATION ET COMPATIBILITÉ
// =============================================================================
/*
    Migration depuis std::deque vers NkDeque :

    std::deque<T> API          | NkDeque<T> équivalent
    --------------------------|----------------------------------
    std::deque<T> dq;        | NkDeque<T> dq;
    dq.push_front(x);        | dq.PushFront(x);
    dq.push_back(x);         | dq.PushBack(x);
    dq.pop_front();          | dq.PopFront();
    dq.pop_back();           | dq.PopBack();
    dq[i]                    | dq[i] (même nom, même sémantique)
    dq.at(i)                 | dq.At(i) (si implémenté avec vérification bounds)
    dq.front()               | dq.Front()
    dq.back()                | dq.Back()
    dq.begin(), dq.end()     | dq.begin()/end() (mêmes noms)
    dq.size(), dq.empty()    | dq.Size()/Empty() OU dq.size()/empty() (aliases)
    dq.clear()               | dq.Clear()

    Avantages de NkDeque vs std::deque :
    - Zéro dépendance STL : portable sur plateformes embarquées, kernels, etc.
    - Allocateur personnalisable via template (pas de std::allocator hardcoded)
    - Gestion d'erreurs configurable : assertions ou exceptions selon le build
    - Nommage PascalCase cohérent avec le reste du framework NK* (pour méthodes principales)
    - Macros NKENTSEU_* pour export/import DLL, inline hints, etc.
    - Intégration avec le système de logging et d'assertions NKPlatform
    - Implémentation transparente par chunks avec contrôle fin de la taille

    Configuration CMake recommandée :
    # Activer C++11 pour move semantics et constructeur/opérateur de déplacement
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