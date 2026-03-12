/**
 * @File NkIterator.h
 * @Description Fournit des classes d'itérateurs génériques pour le framework Nkentseu, sans dépendances STL.
 * @Author TEUGUIA TADJUIDJE Rodolf
 * @Date 2025-06-10
 * @License Rihen
 */

#pragma once

#include "NKPlatform/NkPlatformDetect.h"
#include "NKContainers/NkContainersExport.h"
#include "NKCore/NkTypes.h"
#include "NKCore/NkTraits.h"
#include "NKCore/NkInline.h"
#include "NKMemory/NkFunction.h"

#include <iterator>

/**
 * - nkentseu : Regroupe les composants de base du framework Nkentseu.
 *
 * @Description :
 * Ce namespace contient les classes, fonctions et macros fondamentales pour la gestion des itérateurs,
 * des conteneurs, et des boucles dans le framework Nkentseu. Il est conçu pour être portable et indépendant
 * de la STL, avec un accent sur la performance et la flexibilité.
 */
namespace nkentseu {

    // Bring required traits into nkentseu scope for unqualified use in iterator templates.
    using traits::NkBoolConstant;
    using traits::NkTrueType;
    using traits::NkFalseType;
    using traits::NkEnableIf_t;
    using traits::NkIsBaseOf;
    using traits::NkIsBaseOf_v;
    using traits::NkIsConst;
    using traits::NkIsConst_v;

    /**
     * - NkInputIteratorTag : Marqueur pour les itérateurs d'entrée.
     *
     * @Description :
     * Cette structure sert de tag pour identifier les itérateurs qui supportent uniquement la lecture
     * et l'avancement unidirectionnel (par exemple, pour les flux d'entrée).
     */
    struct NKENTSEU_CONTAINERS_API NkInputIteratorTag {};

    /**
     * - NkOutputIteratorTag : Marqueur pour les itérateurs de sortie.
     *
     * @Description :
     * Cette structure sert de tag pour identifier les itérateurs qui supportent uniquement l'écriture
     * et l'avancement unidirectionnel (par exemple, pour les flux de sortie).
     */
    struct NKENTSEU_CONTAINERS_API NkOutputIteratorTag {};

    /**
     * - NkForwardIteratorTag : Marqueur pour les itérateurs avant.
     *
     * @Description :
     * Cette structure dérive de NkInputIteratorTag et identifie les itérateurs qui supportent
     * l'avancement unidirectionnel avec plusieurs passages sur les mêmes éléments.
     */
    struct NKENTSEU_CONTAINERS_API NkForwardIteratorTag : NkInputIteratorTag {};

    /**
     * - NkBidirectionalIteratorTag : Marqueur pour les itérateurs bidirectionnels.
     *
     * @Description :
     * Cette structure dérive de NkForwardIteratorTag et identifie les itérateurs qui supportent
     * l'avancement et le recul (par exemple, pour les listes chaînées).
     */
    struct NKENTSEU_CONTAINERS_API NkBidirectionalIteratorTag : NkForwardIteratorTag {};

    /**
     * - NkRandomAccessIteratorTag : Marqueur pour les itérateurs à accès aléatoire.
     *
     * @Description :
     * Cette structure dérive de NkBidirectionalIteratorTag et identifie les itérateurs qui supportent
     * l'accès direct à n'importe quel élément via des opérations comme l'indexation ou l'arithmétique
     * (par exemple, pour les tableaux).
     */
    struct NKENTSEU_CONTAINERS_API NkRandomAccessIteratorTag : NkBidirectionalIteratorTag {};

    template<typename T, typename Category = NkRandomAccessIteratorTag>
    class NkConstIterator;

    template<typename Iterator>
    struct NkIteratorTraits {
        using ValueType = typename Iterator::ValueType;
        using Pointer = typename Iterator::Pointer;
        using Reference = typename Iterator::Reference;
        using DifferenceType = typename Iterator::DifferenceType;
        using IteratorCategory = typename Iterator::IteratorCategory;
    };

    // Spécialisation pour les pointeurs bruts
    template<typename T>
    struct NkIteratorTraits<T*> {
        using ValueType = T;
        using Pointer = T*;
        using Reference = T&;
        using DifferenceType = ptrdiff;
        using IteratorCategory = void; // À définir selon le type d'itérateur
    };

    // Spécialisation pour les pointeurs const
    template<typename T>
    struct NkIteratorTraits<const T*> {
        using ValueType = T;
        using Pointer = const T*;
        using Reference = const T&;
        using DifferenceType = ptrdiff;
        using IteratorCategory = void; // À définir
    };

    /**
     * - NkIterator : Itérateur générique mutable pour les conteneurs Nkentseu.
     *
     * @Description :
     * Cette classe template fournit un itérateur mutable permettant de parcourir et de modifier
     * les éléments d'un conteneur. Elle supporte différentes catégories d'itérateurs
     * (Forward, Bidirectional, RandomAccess) via un paramètre de catégorie.
     * Optimisée pour la performance avec des opérations inline et sans allocations dynamiques.
     *
     * @Template :
     * - (typename T) : Type des éléments pointés par l'itérateur.
     * - (typename Category) : Catégorie d'itérateur (par défaut NkRandomAccessIteratorTag).
     *
     * @Members :
     * - (Pointer) mPtr : Pointeur interne vers l'élément courant.
     */
    template<typename T, typename Category = NkRandomAccessIteratorTag>
    class NkIterator {
        public:
            using ValueType = T;
            using Pointer = T*;
            using Reference = T&;
            using DifferenceType = ptrdiff;
            using IteratorCategory = Category;

            /**
             * - NkIterator : Constructeur par défaut.
             *
             * @Description :
             * Initialise un itérateur avec un pointeur nul, représentant un état non défini.
             *
             * @return (void) : Aucune valeur retournée.
             */
            constexpr NkIterator() noexcept : mPtr(nullptr) {}

            /**
             * - NkIterator : Constructeur avec un pointeur.
             *
             * @Description :
             * Initialise un itérateur pointant vers l'élément spécifié par le pointeur.
             *
             * @param (Pointer) ptr : Pointeur vers l'élément à itérer.
             * @return (void) : Aucune valeur retournée.
             *
             * Exemple :
             * int32_t arr[] = {1, 2, 3};
             * NkIterator<int32_t> it(arr); // Pointe sur arr[0]
             */
            explicit constexpr NkIterator(Pointer ptr) noexcept : mPtr(ptr) {}

            /**
             * - operator* : Accède à l'élément courant.
             *
             * @Description :
             * Retourne une référence mutable à l'élément pointé par l'itérateur.
             * Ne vérifie pas la validité du pointeur pour des raisons de performance.
             *
             * @return (Reference) : Référence à l'élément courant.
             *
             * Exemple :
             * NkIterator<int32_t> it(arr);
             * *it = 5; // Modifie arr[0] à 5
             */
            [[nodiscard]] Reference operator*() const noexcept { return *mPtr; }

            /**
             * - operator-> : Accède à l'élément courant via un pointeur.
             *
             * @Description :
             * Retourne le pointeur brut vers l'élément courant, permettant l'accès aux membres.
             *
             * @return (Pointer) : Pointeur vers l'élément courant.
             *
             * Exemple :
             * struct Data { int value; };
             * Data arr[] = {{1}, {2}};
             * NkIterator<Data> it(arr);
             * it->value = 3; // Modifie arr[0].value
             */
            [[nodiscard]] Pointer operator->() const noexcept { return mPtr; }

            /**
             * - operator++ : Incrémente l'itérateur (préfixe).
             *
             * @Description :
             * Avance l'itérateur à l'élément suivant et retourne une référence à l'itérateur.
             *
             * @return (NkIterator&) : Référence à l'itérateur après incrémentation.
             *
             * Exemple :
             * NkIterator<int32_t> it(arr);
             * ++it; // Pointe sur arr[1]
             */
            NkIterator& operator++() noexcept { ++mPtr; return *this; }

            /**
             * - operator++ : Incrémente l'itérateur (postfixe).
             *
             * @Description :
             * Avance l'itérateur à l'élément suivant et retourne une copie de l'itérateur avant incrémentation.
             * Moins efficace que la version préfixe en raison de la copie.
             *
             * @return (NkIterator) : Copie de l'itérateur avant incrémentation.
             *
             * Exemple :
             * NkIterator<int32_t> it(arr);
             * NkIterator<int32_t> old = it++;
             * // old pointe sur arr[0], it pointe sur arr[1]
             */
            [[nodiscard]] NkIterator operator++(int) noexcept { NkIterator tmp = *this; ++mPtr; return tmp; }

            /**
             * - operator-- : Décrémente l'itérateur (préfixe).
             *
             * @Description :
             * Recule l'itérateur à l'élément précédent et retourne une référence à l'itérateur.
             * Disponible uniquement pour les catégories NkBidirectionalIteratorTag ou supérieures.
             *
             * @return (NkIterator&) : Référence à l'itérateur après décrémentation.
             *
             * Exemple :
             * NkIterator<int32_t> it(arr + 1);
             * --it; // Pointe sur arr[0]
             */
            template<typename C = Category, NkEnableIf_t<NkIsBaseOf_v<NkBidirectionalIteratorTag, C>>* = nullptr>
            NkIterator& operator--() noexcept { --mPtr; return *this; }

            /**
             * - operator-- : Décrémente l'itérateur (postfixe).
             *
             * @Description :
             * Recule l'itérateur à l'élément précédent et retourne une copie de l'itérateur avant décrémentation.
             * Disponible uniquement pour les catégories NkBidirectionalIteratorTag ou supérieures.
             *
             * @return (NkIterator) : Copie de l'itérateur avant décrémentation.
             *
             * Exemple :
             * NkIterator<int32_t> it(arr + 1);
             * NkIterator<int32_t> old = it--;
             * // old pointe sur arr[1], it pointe sur arr[0]
             */
            template<typename C = Category, NkEnableIf_t<NkIsBaseOf_v<NkBidirectionalIteratorTag, C>>* = nullptr>
            [[nodiscard]] NkIterator operator--(int) noexcept { NkIterator tmp = *this; --mPtr; return tmp; }

            /**
             * - operator+ : Ajoute un décalage à l'itérateur.
             *
             * @Description :
             * Retourne un nouvel itérateur avancé du nombre d'éléments spécifié.
             * Disponible uniquement pour NkRandomAccessIteratorTag.
             *
             * @param (DifferenceType) offset : Nombre d'éléments à avancer.
             * @return (NkIterator) : Nouvel itérateur avancé.
             *
             * Exemple :
             * NkIterator<int32_t> it(arr);
             * auto it2 = it + 2; // Pointe sur arr[2]
             */
            template<typename C = Category, NkEnableIf_t<NkIsBaseOf_v<NkRandomAccessIteratorTag, C>>* = nullptr>
            [[nodiscard]] NkIterator operator+(DifferenceType offset) const noexcept { return NkIterator(mPtr + offset); }

            /**
             * - operator- : Soustrait un décalage à l'itérateur.
             *
             * @Description :
             * Retourne un nouvel itérateur reculé du nombre d'éléments spécifié.
             * Disponible uniquement pour NkRandomAccessIteratorTag.
             *
             * @param (DifferenceType) offset : Nombre d'éléments à reculer.
             * @return (NkIterator) : Nouvel itérateur reculé.
             */
            template<typename C = Category, NkEnableIf_t<NkIsBaseOf_v<NkRandomAccessIteratorTag, C>>* = nullptr>
            [[nodiscard]] NkIterator operator-(DifferenceType offset) const noexcept { return NkIterator(mPtr - offset); }

            /**
             * - operator- : Calcule la distance entre deux itérateurs.
             *
             * @Description :
             * Retourne le nombre d'éléments entre l'itérateur courant et un autre itérateur.
             * Disponible uniquement pour NkRandomAccessIteratorTag.
             *
             * @param (const NkIterator&) other : Autre itérateur pour le calcul.
             * @return (DifferenceType) : Distance entre les itérateurs.
             *
             * Exemple :
             * NkIterator<int32_t> it1(arr);
             * NkIterator<int32_t> it2(arr + 3);
             * auto dist = it2 - it1; // dist = 3
             */
            template<typename C = Category, NkEnableIf_t<NkIsBaseOf_v<NkRandomAccessIteratorTag, C>>* = nullptr>
            DifferenceType operator-(const NkIterator& other) const noexcept { return mPtr - other.mPtr; }

            /**
             * - operator+= : Avance l'itérateur d'un décalage.
             *
             * @Description :
             * Modifie l'itérateur en l'avançant du nombre d'éléments spécifié.
             * Disponible uniquement pour NkRandomAccessIteratorTag.
             *
             * @param (DifferenceType) offset : Nombre d'éléments à avancer.
             * @return (NkIterator&) : Référence à l'itérateur modifié.
             */
            template<typename C = Category, NkEnableIf_t<NkIsBaseOf_v<NkRandomAccessIteratorTag, C>>* = nullptr>
            NkIterator& operator+=(DifferenceType offset) noexcept { mPtr += offset; return *this; }

            /**
             * - operator-= : Recule l'itérateur d'un décalage.
             *
             * @Description :
             * Modifie l'itérateur en le reculant du nombre d'éléments spécifié.
             * Disponible uniquement pour NkRandomAccessIteratorTag.
             *
             * @param (DifferenceType) offset : Nombre d'éléments à reculer.
             * @return (NkIterator&) : Référence à l'itérateur modifié.
             */
            template<typename C = Category, NkEnableIf_t<NkIsBaseOf_v<NkRandomAccessIteratorTag, C>>* = nullptr>
            NkIterator& operator-=(DifferenceType offset) noexcept { mPtr -= offset; return *this; }

            /**
             * - operator[] : Accède à un élément par index.
             *
             * @Description :
             * Retourne une référence à l'élément situé à l'offset spécifié.
             * Disponible uniquement pour NkRandomAccessIteratorTag.
             *
             * @param (DifferenceType) offset : Index de l'élément à accéder.
             * @return (Reference) : Référence à l'élément.
             *
             * Exemple :
             * NkIterator<int32_t> it(arr);
             * it[1] = 10; // Modifie arr[1] à 10
             */
            template<typename C = Category, NkEnableIf_t<NkIsBaseOf_v<NkRandomAccessIteratorTag, C>>* = nullptr>
            [[nodiscard]] Reference operator[](DifferenceType offset) const noexcept { return *(mPtr + offset); }

            /**
             * - operator== : Compare l'égalité avec un autre itérateur.
             *
             * @Description :
             * Vérifie si l'itérateur pointe sur le même élément qu'un autre itérateur.
             *
             * @param (const NkIterator&) other : Itérateur à comparer.
             * @return (bool) : `true` si les itérateurs sont égaux, `false` sinon.
             */
            [[nodiscard]] bool operator==(const NkIterator& other) const noexcept { return mPtr == other.mPtr; }

            /**
             * - operator!= : Compare la non-égalité avec un autre itérateur.
             *
             * @Description :
             * Vérifie si l'itérateur pointe sur un élément différent d'un autre itérateur.
             *
             * @param (const NkIterator&) other : Itérateur à comparer.
             * @return (bool) : `true` si les itérateurs sont différents, `false` sinon.
             */
            [[nodiscard]] bool operator!=(const NkIterator& other) const noexcept { return mPtr != other.mPtr; }

            /**
             * - operator!= : Compare l'itérateur avec un pointeur nul.
             *
             * @Description :
             * Vérifie si l'itérateur pointe sur un élément différent d'un pointeur nul.
             *
             * @param (mem::nknullptr) : Pointeur nul à comparer.
             * @return (bool) : `true` si l'itérateur n'est pas nul, `false` sinon.
             */
            [[nodiscard]] bool operator==(mem::nknullptr) const noexcept { return mPtr == nullptr; }

            /**
             * - operator!= : Compare l'itérateur avec un pointeur nul.
             *
             * @Description :
             * Vérifie si l'itérateur pointe sur un élément différent d'un pointeur nul.
             *
             * @param (mem::nknullptr) : Pointeur nul à comparer.
             * @return (bool) : `true` si l'itérateur n'est pas nul, `false` sinon.
             */
            [[nodiscard]] bool operator!=(mem::nknullptr) const noexcept { return mPtr != nullptr; }

            /**
             * - operator< : Compare si l'itérateur est avant un autre.
             *
             * @Description :
             * Vérifie si l'itérateur pointe sur un élément avant un autre itérateur.
             * Disponible uniquement pour NkRandomAccessIteratorTag.
             *
             * @param (const NkIterator&) other : Itérateur à comparer.
             * @return (bool) : `true` si l'itérateur est avant, `false` sinon.
             */
            template<typename C = Category, NkEnableIf_t<NkIsBaseOf_v<NkRandomAccessIteratorTag, C>>* = nullptr>
            [[nodiscard]] bool operator<(const NkIterator& other) const noexcept { return mPtr < other.mPtr; }

            /**
             * - operator<= : Compare si l'itérateur est avant ou égal à un autre.
             *
             * @Description :
             * Vérifie si l'itérateur pointe sur un élément avant ou au même endroit qu'un autre.
             * Disponible uniquement pour NkRandomAccessIteratorTag.
             *
             * @param (const NkIterator&) other : Itérateur à comparer.
             * @return (bool) : `true` si l'itérateur est avant ou égal, `false` sinon.
             */
            template<typename C = Category, NkEnableIf_t<NkIsBaseOf_v<NkRandomAccessIteratorTag, C>>* = nullptr>
            [[nodiscard]] bool operator<=(const NkIterator& other) const noexcept { return mPtr <= other.mPtr; }

            /**
             * - operator> : Compare si l'itérateur est après un autre.
             *
             * @Description :
             * Vérifie si l'itérateur pointe sur un élément après un autre itérateur.
             * Disponible uniquement pour NkRandomAccessIteratorTag.
             *
             * @param (const NkIterator&) other : Itérateur à comparer.
             * @return (bool) : `true` si l'itérateur est après, `false` sinon.
             */
            template<typename C = Category, NkEnableIf_t<NkIsBaseOf_v<NkRandomAccessIteratorTag, C>>* = nullptr>
            [[nodiscard]] bool operator>(const NkIterator& other) const noexcept { return mPtr > other.mPtr; }

            /**
             * - operator>= : Compare si l'itérateur est après ou égal à un autre.
             *
             * @Description :
             * Vérifie si l'itérateur pointe sur un élément après ou au même endroit qu'un autre.
             * Disponible uniquement pour NkRandomAccessIteratorTag.
             *
             * @param (const NkIterator&) other : Itérateur à comparer.
             * @return (bool) : `true` si l'itérateur est après ou égal, `false` sinon.
             */
            template<typename C = Category, NkEnableIf_t<NkIsBaseOf_v<NkRandomAccessIteratorTag, C>>* = nullptr>
            [[nodiscard]] bool operator>=(const NkIterator& other) const noexcept { return mPtr >= other.mPtr; }

            /**
             * - operator== : Compare l'égalité avec un NkConstIterator.
             *
             * @Description :
             * Vérifie si l'itérateur pointe sur le même élément qu'un NkConstIterator.
             * Permet la comparaison hétérogène entre itérateurs mutables et const.
             *
             * @param (const NkConstIterator<T>&) other : Itérateur const à comparer.
             * @return (bool) : `true` si les itérateurs sont égaux, `false` sinon.
             *
             * Exemple :
             * NkIterator<int32_t> it(arr);
             * NkConstIterator<int32_t> cit(arr);
             * bool equal = (it == cit); // true
             */
            [[nodiscard]] bool operator==(const NkConstIterator<T, Category>& other) const noexcept { return mPtr == other.operator->(); }

            /**
             * - operator!= : Compare la non-égalité avec un NkConstIterator.
             *
             * @Description :
             * Vérifie si l'itérateur pointe sur un élément différent d'un NkConstIterator.
             *
             * @param (const NkConstIterator<T>&) other : Itérateur const à comparer.
             * @return (bool) : `true` si les itérateurs sont différents, `false` sinon.
             */
            [[nodiscard]] bool operator!=(const NkConstIterator<T, Category>& other) const noexcept { return mPtr != other.operator->(); }

        private:
            Pointer mPtr;
    };

    /**
     * - NkConstIterator : Itérateur générique constant pour les conteneurs Nkentseu.
     *
     * @Description :
     * Cette classe template fournit un itérateur constant permettant de parcourir les éléments
     * d'un conteneur en lecture seule. Elle supporte différentes catégories d'itérateurs
     * et peut être construite à partir d'un NkIterator mutable pour une conversion sécurisée.
     *
     * @Template :
     * - (typename T) : Type des éléments pointés par l'itérateur.
     * - (typename Category) : Catégorie d'itérateur (par défaut NkRandomAccessIteratorTag).
     *
     * @Members :
     * - (Pointer) mPtr : Pointeur interne vers l'élément courant.
     */
    template<typename T, typename Category>
    class NkConstIterator {
    public:
        using ValueType = T;
        using Pointer = const T*;
        using Reference = const T&;
        using DifferenceType = ptrdiff;
        using IteratorCategory = Category;

        /**
         * - NkConstIterator : Constructeur par défaut.
         *
         * @Description :
         * Initialise un itérateur constant avec un pointeur nul.
         *
         * @return (void) : Aucune valeur retournée.
         */
        constexpr NkConstIterator() noexcept : mPtr(nullptr) {}

        /**
         * - NkConstIterator : Constructeur avec un pointeur.
         *
         * @Description :
         * Initialise un itérateur constant pointant vers l'élément spécifié.
         *
         * @param (Pointer) ptr : Pointeur vers l'élément à itérer.
         * @return (void) : Aucune valeur retournée.
         *
         * Exemple :
         * const int32_t arr[] = {1, 2, 3};
         * NkConstIterator<int32_t> it(arr); // Pointe sur arr[0]
         */
        explicit constexpr NkConstIterator(Pointer ptr) noexcept : mPtr(ptr) {}

        /**
         * - NkConstIterator : Constructeur de conversion depuis NkIterator.
         *
         * @Description :
         * Construit un itérateur constant à partir d'un itérateur mutable, permettant
         * une conversion sécurisée vers un accès en lecture seule.
         * Uniquement disponible pour les types non-const.
         *
         * @param (const NkIterator<U, Category>&) other : Itérateur mutable source.
         * @return (void) : Aucune valeur retournée.
         *
         * Exemple :
         * int32_t arr[] = {1, 2, 3};
         * NkIterator<int32_t> it(arr);
         * NkConstIterator<int32_t> cit(it); // Conversion sécurisée
         */
        template<typename U = T, typename = NkEnableIf_t<!NkIsConst_v<U>>>
        constexpr NkConstIterator(const NkIterator<U, Category>& other) noexcept : mPtr(other.operator->()) {}

        /**
         * - operator* : Accède à l'élément courant.
         *
         * @Description :
         * Retourne une référence constante à l'élément pointé par l'itérateur.
         *
         * @return (Reference) : Référence constante à l'élément courant.
         *
         * Exemple :
         * NkConstIterator<int32_t> it(arr);
         * printf("%d\n", *it); // Affiche arr[0]
         */
        [[nodiscard]] Reference operator*() const noexcept { return *mPtr; }

        /**
         * - operator-> : Accède à l'élément courant via un pointeur.
         *
         * @Description :
         * Retourne le pointeur constant vers l'élément courant.
         *
         * @return (Pointer) : Pointeur constant vers l'élément courant.
         */
        [[nodiscard]] Pointer operator->() const noexcept { return mPtr; }

        /**
         * - operator++ : Incrémente l'itérateur (préfixe).
         *
         * @Description :
         * Avance l'itérateur à l'élément suivant.
         *
         * @return (NkConstIterator&) : Référence à l'itérateur après incrémentation.
         */
        NkConstIterator& operator++() noexcept { ++mPtr; return *this; }

        /**
         * - operator++ : Incrémente l'itérateur (postfixe).
         *
         * @Description :
         * Avance l'itérateur à l'élément suivant et retourne une copie avant incrémentation.
         *
         * @return (NkConstIterator) : Copie de l'itérateur avant incrémentation.
         */
        [[nodiscard]] NkConstIterator operator++(int) noexcept { NkConstIterator tmp = *this; ++mPtr; return tmp; }

        /**
         * - operator-- : Décrémente l'itérateur (préfixe).
         *
         * @Description :
         * Recule l'itérateur à l'élément précédent.
         * Disponible pour NkBidirectionalIteratorTag ou supérieures.
         *
         * @return (NkConstIterator&) : Référence à l'itérateur après décrémentation.
         */
        template<typename C = Category, NkEnableIf_t<NkIsBaseOf_v<NkBidirectionalIteratorTag, C>>* = nullptr>
        NkConstIterator& operator--() noexcept { --mPtr; return *this; }

        /**
         * - operator-- : Décrémente l'itérateur (postfixe).
         *
         * @Description :
         * Recule l'itérateur à l'élément précédent et retourne une copie avant décrémentation.
         * Disponible pour NkBidirectionalIteratorTag ou supérieures.
         *
         * @return (NkConstIterator) : Copie de l'itérateur avant décrémentation.
         */
        template<typename C = Category, NkEnableIf_t<NkIsBaseOf_v<NkBidirectionalIteratorTag, C>>* = nullptr>
        [[nodiscard]] NkConstIterator operator--(int) noexcept { NkConstIterator tmp = *this; --mPtr; return tmp; }

        /**
         * - operator+ : Ajoute un décalage à l'itérateur.
         *
         * @Description :
         * Retourne un nouvel itérateur avancé du nombre d'éléments spécifié.
         * Disponible pour NkRandomAccessIteratorTag.
         *
         * @param (DifferenceType) offset : Nombre d'éléments à avancer.
         * @return (NkConstIterator) : Nouvel itérateur avancé.
         */
        template<typename C = Category, NkEnableIf_t<NkIsBaseOf_v<NkRandomAccessIteratorTag, C>>* = nullptr>
        [[nodiscard]] NkConstIterator operator+(DifferenceType offset) const noexcept { return NkConstIterator(mPtr + offset); }

        /**
         * - operator- : Soustrait un décalage à l'itérateur.
         *
         * @Description :
         * Retourne un nouvel itérateur reculé du nombre d'éléments spécifié.
         * Disponible pour NkRandomAccessIteratorTag.
         *
         * @param (DifferenceType) offset : Nombre d'éléments à reculer.
         * @return (NkConstIterator) : Nouvel itérateur reculé.
         */
        template<typename C = Category, NkEnableIf_t<NkIsBaseOf_v<NkRandomAccessIteratorTag, C>>* = nullptr>
        [[nodiscard]] NkConstIterator operator-(DifferenceType offset) const noexcept { return NkConstIterator(mPtr - offset); }

        /**
         * - operator- : Calcule la distance entre deux itérateurs.
         *
         * @Description :
         * Retourne le nombre d'éléments entre l'itérateur courant et un autre.
         * Disponible pour NkRandomAccessIteratorTag.
         *
         * @param (const NkConstIterator&) other : Autre itérateur pour le calcul.
         * @return (DifferenceType) : Distance entre les itérateurs.
         */
        template<typename C = Category, NkEnableIf_t<NkIsBaseOf_v<NkRandomAccessIteratorTag, C>>* = nullptr>
        DifferenceType operator-(const NkConstIterator& other) const noexcept { return mPtr - other.mPtr; }

        /**
         * - operator+= : Avance l'itérateur d'un décalage.
         *
         * @Description :
         * Modifie l'itérateur en l'avançant du nombre d'éléments spécifié.
         * Disponible pour NkRandomAccessIteratorTag.
         *
         * @param (DifferenceType) offset : Nombre d'éléments à avancer.
         * @return (NkConstIterator&) : Référence à l'itérateur modifié.
         */
        template<typename C = Category, NkEnableIf_t<NkIsBaseOf_v<NkRandomAccessIteratorTag, C>>* = nullptr>
        NkConstIterator& operator+=(DifferenceType offset) noexcept { mPtr += offset; return *this; }

        /**
         * - operator-= : Recule l'itérateur d'un décalage.
         *
         * @Description :
         * Modifie l'itérateur en le reculant du nombre d'éléments spécifié.
         * Disponible pour NkRandomAccessIteratorTag.
         *
         * @param (DifferenceType) offset : Nombre d'éléments à reculer.
         * @return (NkConstIterator&) : Référence à l'itérateur modifié.
         */
        template<typename C = Category, NkEnableIf_t<NkIsBaseOf_v<NkRandomAccessIteratorTag, C>>* = nullptr>
        NkConstIterator& operator-=(DifferenceType offset) noexcept { mPtr -= offset; return *this; }

        /**
         * - operator[] : Accède à un élément par index.
         *
         * @Description :
         * Retourne une référence constante à l'élément situé à l'offset spécifié.
         * Disponible pour NkRandomAccessIteratorTag.
         *
         * @param (DifferenceType) offset : Index de l'élément à accéder.
         * @return (Reference) : Référence constante à l'élément.
         */
        template<typename C = Category, NkEnableIf_t<NkIsBaseOf_v<NkRandomAccessIteratorTag, C>>* = nullptr>
        [[nodiscard]] Reference operator[](DifferenceType offset) const noexcept { return *(mPtr + offset); }

        /**
         * - operator== : Compare l'égalité avec un autre itérateur constant.
         *
         * @Description :
         * Vérifie si l'itérateur pointe sur le même élément qu'un autre itérateur constant.
         *
         * @param (const NkConstIterator&) other : Itérateur à comparer.
         * @return (bool) : `true` si les itérateurs sont égaux, `false` sinon.
         */
        [[nodiscard]] bool operator==(const NkConstIterator& other) const noexcept { return mPtr == other.mPtr; }

        /**
         * - operator!= : Compare la non-égalité avec un autre itérateur constant.
         *
         * @Description :
         * Vérifie si l'itérateur pointe sur un élément différent d'un autre itérateur constant.
         *
         * @param (const NkConstIterator&) other : Itérateur à comparer.
         * @return (bool) : `true` si les itérateurs sont différents, `false` sinon.
         */
        [[nodiscard]] bool operator!=(const NkConstIterator& other) const noexcept { return mPtr != other.mPtr; }

        /**
         * - operator!= : Compare l'itérateur avec un pointeur nul.
         *
         * @Description :
         * Vérifie si l'itérateur pointe sur un élément différent d'un pointeur nul.
         *
         * @param (mem::nknullptr) : Pointeur nul à comparer.
         * @return (bool) : `true` si l'itérateur n'est pas nul, `false` sinon.
         */
        [[nodiscard]] bool operator!=(mem::nknullptr) const noexcept { return mPtr != nullptr; }

        /**
         * - operator== : Compare l'itérateur avec un pointeur nul.
         *
         * @Description :
         * Vérifie si l'itérateur pointe sur un élément nul.
         *
         * @param (mem::nknullptr) : Pointeur nul à comparer.
         * @return (bool) : `true` si l'itérateur est nul, `false` sinon.
         */
        [[nodiscard]] bool operator==(mem::nknullptr) const noexcept { return mPtr == nullptr; }

        /**
         * - operator< : Compare si l'itérateur est avant un autre.
         *
         * @Description :
         * Vérifie si l'itérateur pointe sur un élément avant un autre itérateur constant.
         * Disponible pour NkRandomAccessIteratorTag.
         *
         * @param (const NkConstIterator&) other : Itérateur à comparer.
         * @return (bool) : `true` si l'itérateur est avant, `false` sinon.
         */
        template<typename C = Category, NkEnableIf_t<NkIsBaseOf_v<NkRandomAccessIteratorTag, C>>* = nullptr>
        [[nodiscard]] bool operator<(const NkConstIterator& other) const noexcept { return mPtr < other.mPtr; }

        /**
         * - operator<= : Compare si l'itérateur est avant ou égal à un autre.
         *
         * @Description :
         * Vérifie si l'itérateur pointe sur un élément avant ou au même endroit qu'un autre.
         * Disponible pour NkRandomAccessIteratorTag.
         *
         * @param (const NkConstIterator&) other : Itérateur à comparer.
         * @return (bool) : `true` si l'itérateur est avant ou égal, `false` sinon.
         */
        template<typename C = Category, NkEnableIf_t<NkIsBaseOf_v<NkRandomAccessIteratorTag, C>>* = nullptr>
        [[nodiscard]] bool operator<=(const NkConstIterator& other) const noexcept { return mPtr <= other.mPtr; }

        /**
         * - operator> : Compare si l'itérateur est après un autre.
         *
         * @Description :
         * Vérifie si l'itérateur pointe sur un élément après un autre itérateur constant.
         * Disponible pour NkRandomAccessIteratorTag.
         *
         * @param (const NkConstIterator&) other : Itérateur à comparer.
         * @return (bool) : `true` si l'itérateur est après, `false` sinon.
         */
        template<typename C = Category, NkEnableIf_t<NkIsBaseOf_v<NkRandomAccessIteratorTag, C>>* = nullptr>
        [[nodiscard]] bool operator>(const NkConstIterator& other) const noexcept { return mPtr > other.mPtr; }

        /**
         * - operator>= : Compare si l'itérateur est après ou égal à un autre.
         *
         * @Description :
         * Vérifie si l'itérateur pointe sur un élément après ou au même endroit qu'un autre.
         * Disponible pour NkRandomAccessIteratorTag.
         *
         * @param (const NkConstIterator&) other : Itérateur à comparer.
         * @return (bool) : `true` si l'itérateur est après ou égal, `false` sinon.
         */
        template<typename C = Category, NkEnableIf_t<NkIsBaseOf_v<NkRandomAccessIteratorTag, C>>* = nullptr>
        [[nodiscard]] bool operator>=(const NkConstIterator& other) const noexcept { return mPtr >= other.mPtr; }

        /**
         * - operator== : Compare l'égalité avec un NkIterator.
         *
         * @Description :
         * Vérifie si l'itérateur constant pointe sur le même élément qu'un itérateur mutable.
         *
         * @param (const NkIterator<T, Category>&) other : Itérateur mutable à comparer.
         * @return (bool) : `true` si les itérateurs sont égaux, `false` sinon.
         */
        [[nodiscard]] bool operator==(const NkIterator<T, Category>& other) const noexcept { return mPtr == other.operator->(); }

        /**
         * - operator!= : Compare la non-égalité avec un NkIterator.
         *
         * @Description :
         * Vérifie si l'itérateur constant pointe sur un élément différent d'un itérateur mutable.
         *
         * @param (const NkIterator<T, Category>&) other : Itérateur mutable à comparer.
         * @return (bool) : `true` si les itérateurs sont différents, `false` sinon.
         */
        [[nodiscard]] bool operator!=(const NkIterator<T, Category>& other) const noexcept { return mPtr != other.operator->(); }

    private:
        Pointer mPtr;
    };

    // Définitions existantes : NkInputIteratorTag, NkOutputIteratorTag, NkForwardIteratorTag, 
    // NkBidirectionalIteratorTag, NkRandomAccessIteratorTag, NkIteratorTraits, etc.

    // Déclarations anticipées
    template<typename T, typename Category = NkBidirectionalIteratorTag>
    class NkBidirectionalIterator;

    template<typename T, typename Category = NkBidirectionalIteratorTag>
    class NkConstBidirectionalIterator;

    /**
     * - NkBidirectionalIterator : Itérateur bidirectionnel mutable pour les conteneurs Nkentseu.
     *
     * @Description :
     * Cette classe template fournit un itérateur bidirectionnel mutable permettant de parcourir 
     * et de modifier les éléments d'un conteneur dans les deux sens (avant et arrière). Elle 
     * supporte la catégorie NkBidirectionalIteratorTag, avec des opérations d'incrémentation,
     * de décrémentation, d'accès et de comparaison. Optimisée pour la performance avec des 
     * opérations inline et sans allocations dynamiques.
     *
     * @Template :
     * - (typename T) : Type des éléments pointés par l'itérateur.
     * - (typename Category) : Catégorie d'itérateur (par défaut NkBidirectionalIteratorTag).
     *
     * @Members :
     * - (Pointer) mPtr : Pointeur interne vers l'élément courant (ou nœud pour des conteneurs complexes).
     */
    template<typename T, typename Category>
    class NkBidirectionalIterator {
    public:
        using ValueType = T;
        using Pointer = T*;
        using Reference = T&;
        using DifferenceType = ptrdiff;
        using IteratorCategory = Category;

        /**
         * - NkBidirectionalIterator : Constructeur par défaut.
         *
         * @Description :
         * Initialise un itérateur avec un pointeur nul, représentant un état non défini.
         */
        constexpr NkBidirectionalIterator() noexcept : mPtr(nullptr) {}

        /**
         * - NkBidirectionalIterator : Constructeur avec un pointeur.
         *
         * @Description :
         * Initialise un itérateur pointant vers l'élément spécifié par le pointeur.
         *
         * @param (Pointer) ptr : Pointeur vers l'élément à itérer.
         */
        explicit constexpr NkBidirectionalIterator(Pointer ptr) noexcept : mPtr(ptr) {}

        /**
         * - operator* : Accède à l'élément courant.
         *
         * @Description :
         * Retourne une référence mutable à l'élément pointé par l'itérateur.
         * Ne vérifie pas la validité du pointeur pour des raisons de performance.
         *
         * @return (Reference) : Référence à l'élément courant.
         */
        [[nodiscard]] Reference operator*() const noexcept { 
            #ifdef NKENTSEU_LOG_ENABLED
            if (mPtr == nullptr) {
                NKENTSEU_LOG_ERROR("NkBidirectionalIterator: Dereferencing null pointer.");
            }
            #endif
            return *mPtr; 
        }

        /**
         * - operator-> : Accède à l'élément courant via un pointeur.
         *
         * @Description :
         * Retourne le pointeur brut vers l'élément courant, permettant l'accès aux membres.
         *
         * @return (Pointer) : Pointeur vers l'élément courant.
         */
        [[nodiscard]] Pointer operator->() const noexcept { 
            #ifdef NKENTSEU_LOG_ENABLED
            if (mPtr == nullptr) {
                NKENTSEU_LOG_ERROR("NkBidirectionalIterator: Accessing null pointer.");
            }
            #endif
            return mPtr; 
        }

        /**
         * - operator++ : Incrémente l'itérateur (préfixe).
         *
         * @Description :
         * Avance l'itérateur à l'élément suivant et retourne une référence à l'itérateur.
         *
         * @return (NkBidirectionalIterator&) : Référence à l'itérateur après incrémentation.
         */
        NkBidirectionalIterator& operator++() noexcept { 
            ++mPtr; 
            return *this; 
        }

        /**
         * - operator++ : Incrémente l'itérateur (postfixe).
         *
         * @Description :
         * Avance l'itérateur à l'élément suivant et retourne une copie de l'itérateur avant incrémentation.
         *
         * @return (NkBidirectionalIterator) : Copie de l'itérateur avant incrémentation.
         */
        [[nodiscard]] NkBidirectionalIterator operator++(int) noexcept { 
            NkBidirectionalIterator tmp = *this; 
            ++mPtr; 
            return tmp; 
        }

        /**
         * - operator-- : Décrémente l'itérateur (préfixe).
         *
         * @Description :
         * Recule l'itérateur à l'élément précédent et retourne une référence à l'itérateur.
         *
         * @return (NkBidirectionalIterator&) : Référence à l'itérateur après décrémentation.
         */
        template<typename C = Category, NkEnableIf_t<NkIsBaseOf_v<NkBidirectionalIteratorTag, C>>* = nullptr>
        NkBidirectionalIterator& operator--() noexcept { 
            --mPtr; 
            return *this; 
        }

        /**
         * - operator-- : Décrémente l'itérateur (postfixe).
         *
         * @Description :
         * Recule l'itérateur à l'élément précédent et retourne une copie de l'itérateur avant décrémentation.
         *
         * @return (NkBidirectionalIterator) : Copie de l'itérateur avant décrémentation.
         */
        template<typename C = Category, NkEnableIf_t<NkIsBaseOf_v<NkBidirectionalIteratorTag, C>>* = nullptr>
        [[nodiscard]] NkBidirectionalIterator operator--(int) noexcept { 
            NkBidirectionalIterator tmp = *this; 
            --mPtr; 
            return tmp; 
        }

        /**
         * - operator== : Compare l'égalité avec un autre itérateur.
         *
         * @Description :
         * Vérifie si l'itérateur pointe sur le même élément qu'un autre itérateur.
         *
         * @param (const NkBidirectionalIterator&) other : Itérateur à comparer.
         * @return (bool) : `true` si les itérateurs sont égaux, `false` sinon.
         */
        [[nodiscard]] bool operator==(const NkBidirectionalIterator& other) const noexcept { 
            return mPtr == other.mPtr; 
        }

        /**
         * - operator!= : Compare la non-égalité avec un autre itérateur.
         *
         * @Description :
         * Vérifie si l'itérateur pointe sur un élément différent d'un autre itérateur.
         *
         * @param (const NkBidirectionalIterator&) other : Itérateur à comparer.
         * @return (bool) : `true` si les itérateurs sont différents, `false` sinon.
         */
        [[nodiscard]] bool operator!=(const NkBidirectionalIterator& other) const noexcept { 
            return mPtr != other.mPtr; 
        }

        /**
         * - operator== : Compare l'égalité avec un pointeur nul.
         *
         * @Description :
         * Vérifie si l'itérateur pointe sur un élément nul.
         *
         * @param (mem::nknullptr) : Pointeur nul à comparer.
         * @return (bool) : `true` si l'itérateur est nul, `false` sinon.
         */
        [[nodiscard]] bool operator==(mem::nknullptr) const noexcept { 
            return mPtr == nullptr; 
        }

        /**
         * - operator!= : Compare la non-égalité avec un pointeur nul.
         *
         * @Description :
         * Vérifie si l'itérateur pointe sur un élément différent d'un pointeur nul.
         *
         * @param (mem::nknullptr) : Pointeur nul à comparer.
         * @return (bool) : `true` si l'itérateur n'est pas nul, `false` sinon.
         */
        [[nodiscard]] bool operator!=(mem::nknullptr) const noexcept { 
            return mPtr != nullptr; 
        }

        /**
         * - operator== : Compare l'égalité avec un NkConstBidirectionalIterator.
         *
         * @Description :
         * Vérifie si l'itérateur pointe sur le même élément qu'un itérateur constant.
         *
         * @param (const NkConstBidirectionalIterator<T, Category>&) other : Itérateur constant à comparer.
         * @return (bool) : `true` si les itérateurs sont égaux, `false` sinon.
         */
        [[nodiscard]] bool operator==(const NkConstBidirectionalIterator<T, Category>& other) const noexcept { 
            return mPtr == other.operator->(); 
        }

        /**
         * - operator!= : Compare la non-égalité avec un NkConstBidirectionalIterator.
         *
         * @Description :
         * Vérifie si l'itérateur pointe sur un élément différent d'un itérateur constant.
         *
         * @param (const NkConstBidirectionalIterator<T, Category>&) other : Itérateur constant à comparer.
         * @return (bool) : `true` si les itérateurs sont différents, `false` sinon.
         */
        [[nodiscard]] bool operator!=(const NkConstBidirectionalIterator<T, Category>& other) const noexcept { 
            return mPtr != other.operator->(); 
        }

    private:
        Pointer mPtr;
    };

    /**
     * - NkConstBidirectionalIterator : Itérateur bidirectionnel constant pour les conteneurs Nkentseu.
     *
     * @Description :
     * Cette classe template fournit un itérateur bidirectionnel constant permettant de parcourir 
     * les éléments d'un conteneur en lecture seule dans les deux sens. Elle peut être construite 
     * à partir d'un NkBidirectionalIterator pour une conversion sécurisée.
     *
     * @Template :
     * - (typename T) : Type des éléments pointés par l'itérateur.
     * - (typename Category) : Catégorie d'itérateur (par défaut NkBidirectionalIteratorTag).
     *
     * @Members :
     * - (Pointer) mPtr : Pointeur interne vers l'élément courant.
     */
    template<typename T, typename Category>
    class NkConstBidirectionalIterator {
    public:
        using ValueType = T;
        using Pointer = const T*;
        using Reference = const T&;
        using DifferenceType = ptrdiff;
        using IteratorCategory = Category;

        /**
         * - NkConstBidirectionalIterator : Constructeur par défaut.
         *
         * @Description :
         * Initialise un itérateur constant avec un pointeur nul.
         */
        constexpr NkConstBidirectionalIterator() noexcept : mPtr(nullptr) {}

        /**
         * - NkConstBidirectionalIterator : Constructeur avec un pointeur.
         *
         * @Description :
         * Initialise un itérateur constant pointant vers l'élément spécifié.
         *
         * @param (Pointer) ptr : Pointeur vers l'élément à itérer.
         */
        explicit constexpr NkConstBidirectionalIterator(Pointer ptr) noexcept : mPtr(ptr) {}

        /**
         * - NkConstBidirectionalIterator : Constructeur de conversion depuis NkBidirectionalIterator.
         *
         * @Description :
         * Construit un itérateur constant à partir d'un itérateur mutable, permettant
         * une conversion sécurisée vers un accès en lecture seule.
         *
         * @param (const NkBidirectionalIterator<U, Category>&) other : Itérateur mutable source.
         */
        template<typename U = T, typename = NkEnableIf_t<!NkIsConst_v<U>>>
        constexpr NkConstBidirectionalIterator(const NkBidirectionalIterator<U, Category>& other) noexcept 
            : mPtr(other.operator->()) {}

        /**
         * - operator* : Accède à l'élément courant.
         *
         * @Description :
         * Retourne une référence constante à l'élément pointé par l'itérateur.
         *
         * @return (Reference) : Référence constante à l'élément courant.
         */
        [[nodiscard]] Reference operator*() const noexcept { 
            #ifdef NKENTSEU_LOG_ENABLED
            if (mPtr == nullptr) {
                NKENTSEU_LOG_ERROR("NkConstBidirectionalIterator: Dereferencing null pointer.");
            }
            #endif
            return *mPtr; 
        }

        /**
         * - operator-> : Accède à l'élément courant via un pointeur.
         *
         * @Description :
         * Retourne le pointeur constant vers l'élément courant.
         *
         * @return (Pointer) : Pointeur constant vers l'élément courant.
         */
        [[nodiscard]] Pointer operator->() const noexcept { 
            #ifdef NKENTSEU_LOG_ENABLED
            if (mPtr == nullptr) {
                NKENTSEU_LOG_ERROR("NkConstBidirectionalIterator: Accessing null pointer.");
            }
            #endif
            return mPtr; 
        }

        /**
         * - operator++ : Incrémente l'itérateur (préfixe).
         *
         * @Description :
         * Avance l'itérateur à l'élément suivant.
         *
         * @return (NkConstBidirectionalIterator&) : Référence à l'itérateur après incrémentation.
         */
        NkConstBidirectionalIterator& operator++() noexcept { 
            ++mPtr; 
            return *this; 
        }

        /**
         * - operator++ : Incrémente l'itérateur (postfixe).
         *
         * @Description :
         * Avance l'itérateur à l'élément suivant et retourne une copie avant incrémentation.
         *
         * @return (NkConstBidirectionalIterator) : Copie de l'itérateur avant incrémentation.
         */
        [[nodiscard]] NkConstBidirectionalIterator operator++(int) noexcept { 
            NkConstBidirectionalIterator tmp = *this; 
            ++mPtr; 
            return tmp; 
        }

        /**
         * - operator-- : Décrémente l'itérateur (préfixe).
         *
         * @Description :
         * Recule l'itérateur à l'élément précédent.
         *
         * @return (NkConstBidirectionalIterator&) : Référence à l'itérateur après décrémentation.
         */
        template<typename C = Category, NkEnableIf_t<NkIsBaseOf_v<NkBidirectionalIteratorTag, C>>* = nullptr>
        NkConstBidirectionalIterator& operator--() noexcept { 
            --mPtr; 
            return *this; 
        }

        /**
         * - operator-- : Décrémente l'itérateur (postfixe).
         *
         * @Description :
         * Recule l'itérateur à l'élément précédent et retourne une copie avant décrémentation.
         *
         * @return (NkConstBidirectionalIterator) : Copie de l'itérateur avant décrémentation.
         */
        template<typename C = Category, NkEnableIf_t<NkIsBaseOf_v<NkBidirectionalIteratorTag, C>>* = nullptr>
        [[nodiscard]] NkConstBidirectionalIterator operator--(int) noexcept { 
            NkConstBidirectionalIterator tmp = *this; 
            --mPtr; 
            return tmp; 
        }

        /**
         * - operator== : Compare l'égalité avec un autre itérateur constant.
         *
         * @Description :
         * Vérifie si l'itérateur pointe sur le même élément qu'un autre itérateur constant.
         *
         * @param (const NkConstBidirectionalIterator&) other : Itérateur à comparer.
         * @return (bool) : `true` si les itérateurs sont égaux, `false` sinon.
         */
        [[nodiscard]] bool operator==(const NkConstBidirectionalIterator& other) const noexcept { 
            return mPtr == other.mPtr; 
        }

        /**
         * - operator!= : Compare la non-égalité avec un autre itérateur constant.
         *
         * @Description :
         * Vérifie si l'itérateur pointe sur un élément différent d'un autre itérateur constant.
         *
         * @param (const NkConstBidirectionalIterator&) other : Itérateur à comparer.
         * @return (bool) : `true` si les itérateurs sont différents, `false` sinon.
         */
        [[nodiscard]] bool operator!=(const NkConstBidirectionalIterator& other) const noexcept { 
            return mPtr != other.mPtr; 
        }

        /**
         * - operator== : Compare l'égalité avec un pointeur nul.
         *
         * @Description :
         * Vérifie si l'itérateur pointe sur un élément nul.
         *
         * @param (mem::nknullptr) : Pointeur nul à comparer.
         * @return (bool) : `true` si l'itérateur est nul, `false` sinon.
         */
        [[nodiscard]] bool operator==(mem::nknullptr) const noexcept { 
            return mPtr == nullptr; 
        }

        /**
         * - operator!= : Compare la non-égalité avec un pointeur nul.
         *
         * @Description :
         * Vérifie si l'itérateur pointe sur un élément différent d'un pointeur nul.
         *
         * @param (mem::nknullptr) : Pointeur nul à comparer.
         * @return (bool) : `true` si l'itérateur n'est pas nul, `false` sinon.
         */
        [[nodiscard]] bool operator!=(mem::nknullptr) const noexcept { 
            return mPtr != nullptr; 
        }

        /**
         * - operator== : Compare l'égalité avec un NkBidirectionalIterator.
         *
         * @Description :
         * Vérifie si l'itérateur constant pointe sur le même élément qu'un itérateur mutable.
         *
         * @param (const NkBidirectionalIterator<T, Category>&) other : Itérateur mutable à comparer.
         * @return (bool) : `true` si les itérateurs sont égaux, `false` sinon.
         */
        [[nodiscard]] bool operator==(const NkBidirectionalIterator<T, Category>& other) const noexcept { 
            return mPtr == other.operator->(); 
        }

        /**
         * - operator!= : Compare la non-égalité avec un NkBidirectionalIterator.
         *
         * @Description :
         * Vérifie si l'itérateur constant pointe sur un élément différent d'un itérateur mutable.
         *
         * @param (const NkBidirectionalIterator<T, Category>&) other : Itérateur mutable à comparer.
         * @return (bool) : `true` si les itérateurs sont différents, `false` sinon.
         */
        [[nodiscard]] bool operator!=(const NkBidirectionalIterator<T, Category>& other) const noexcept { 
            return mPtr != other.operator->(); 
        }

    private:
        Pointer mPtr;
    };

    // Mise à jour de NkIteratorTraits pour NkBidirectionalIterator
    template<typename T, typename Category>
    struct NkIteratorTraits<NkBidirectionalIterator<T, Category>> {
        using ValueType = typename NkBidirectionalIterator<T, Category>::ValueType;
        using Pointer = typename NkBidirectionalIterator<T, Category>::Pointer;
        using Reference = typename NkBidirectionalIterator<T, Category>::Reference;
        using DifferenceType = typename NkBidirectionalIterator<T, Category>::DifferenceType;
        using IteratorCategory = typename NkBidirectionalIterator<T, Category>::IteratorCategory;
    };

    // Mise à jour de NkIteratorTraits pour NkConstBidirectionalIterator
    template<typename T, typename Category>
    struct NkIteratorTraits<NkConstBidirectionalIterator<T, Category>> {
        using ValueType = typename NkConstBidirectionalIterator<T, Category>::ValueType;
        using Pointer = typename NkConstBidirectionalIterator<T, Category>::Pointer;
        using Reference = typename NkConstBidirectionalIterator<T, Category>::Reference;
        using DifferenceType = typename NkConstBidirectionalIterator<T, Category>::DifferenceType;
        using IteratorCategory = typename NkConstBidirectionalIterator<T, Category>::IteratorCategory;
    };

    /**
     * - NkReverseIterator : Adaptateur pour itérer à l'envers.
     *
     * @Description :
     * Cette classe template adapte un itérateur existant (NkIterator ou NkConstIterator)
     * pour parcourir les éléments dans l'ordre inverse. Elle inverse les opérations
     * d'incrémentation et de décrémentation pour simuler une itération à rebours.
     *
     * @Template :
     * - (typename Iterator) : Type de l'itérateur sous-jacent (NkIterator ou NkConstIterator).
     *
     * @Members :
     * - (Iterator) mIt : Itérateur sous-jacent.
     */
    template<typename Iterator>
    class NkReverseIterator {
    public:
        using ValueType = typename NkIteratorTraits<Iterator>::ValueType;
        using Pointer = typename NkIteratorTraits<Iterator>::Pointer;
        using Reference = typename NkIteratorTraits<Iterator>::Reference;
        using DifferenceType = typename NkIteratorTraits<Iterator>::DifferenceType;
        using IteratorCategory = typename NkIteratorTraits<Iterator>::IteratorCategory;

        /**
         * - NkReverseIterator : Constructeur par défaut.
         *
         * @Description :
         * Initialise un itérateur inversé avec un itérateur sous-jacent par défaut.
         *
         * @return (void) : Aucune valeur retournée.
         */
        constexpr NkReverseIterator() noexcept : mIt() {}

        /**
         * - NkReverseIterator : Constructeur avec un itérateur.
         *
         * @Description :
         * Initialise un itérateur inversé encapsulant l'itérateur donné.
         *
         * @param (Iterator) it : Itérateur sous-jacent à inverser.
         * @return (void) : Aucune valeur retournée.
         *
         * Exemple :
         * int32_t arr[] = {1, 2, 3};
         * NkIterator<int32_t> it(arr + 3);
         * NkReverseIterator<NkIterator<int32_t>> rit(it); // Pointe sur arr[2]
         */
        explicit constexpr NkReverseIterator(Iterator it) noexcept : mIt(it) {}

        /**
         * - operator* : Accède à l'élément précédent.
         *
         * @Description :
         * Retourne une référence à l'élément juste avant l'itérateur sous-jacent,
         * simulant une itération à l'envers.
         *
         * @return (Reference) : Référence à l'élément précédent.
         *
         * Exemple :
         * NkReverseIterator<NkIterator<int32_t>> rit(it);
         * printf("%d\n", *rit); // Affiche arr[2]
         */
        [[nodiscard]] Reference operator*() const noexcept {
            auto tmp = mIt;
            return *--tmp;
        }

        /**
         * - operator-> : Accède à l'élément précédent via un pointeur.
         *
         * @Description :
         * Retourne un pointeur vers l'élément juste avant l'itérateur sous-jacent.
         *
         * @return (Pointer) : Pointeur vers l'élément précédent.
         */
        [[nodiscard]] Pointer operator->() const noexcept {
            auto tmp = mIt;
            return (--tmp).operator->();
        }

        /**
         * - operator++ : Incrémente l'itérateur inversé (préfixe).
         *
         * @Description :
         * Décrémente l'itérateur sous-jacent pour avancer dans l'itération inversée.
         *
         * @return (NkReverseIterator&) : Référence à l'itérateur après incrémentation.
         */
        NkReverseIterator& operator++() noexcept { --mIt; return *this; }

        /**
         * - operator++ : Incrémente l'itérateur inversé (postfixe).
         *
         * @Description :
         * Décrémente l'itérateur sous-jacent et retourne une copie avant incrémentation.
         *
         * @return (NkReverseIterator) : Copie de l’itérateur avant incrémentation.
         */
        [[nodiscard]] NkReverseIterator operator++(int) noexcept { NkReverseIterator tmp = *this; --mIt; return tmp; }

        /**
         * - operator-- : Décrémente l'itérateur inversé (préfixe).
         *
         * @Description :
         * Incrémente l'itérateur sous-jacent pour reculer dans l'itération inversée.
         * Disponible pour NkBidirectionalIteratorTag.
         *
         * @return (NkReverseIterator&) : Référence à l’itérateur après décrémentation.
         */
        template<typename C = IteratorCategory, NkEnableIf_t<NkIsBaseOf_v<NkBidirectionalIteratorTag, C>>* = nullptr>
        NkReverseIterator& operator--() noexcept { ++mIt; return *this; }

        /**
         * - operator-- : Décrémente l’itérateur inversé (postfixe).
         *
         * @Description :
         * Incrémente l’itérateur sous-jacent et retourne une copie avant décrémentation.
         * Disponible pour NkBidirectionalIteratorTag.
         *
         * @return (NkReverseIterator) : Copie de l’itérateur avant décrémentation.
         */
        template<typename C = IteratorCategory, NkEnableIf_t<NkIsBaseOf_v<NkBidirectionalIteratorTag, C>>* = nullptr>
        [[nodiscard]] NkReverseIterator operator--(int) noexcept { NkReverseIterator tmp = *this; ++mIt; return tmp; }

        /**
         * - operator+ : Ajoute un décalage à l’itérateur inversé.
         *
         * @Description :
         * Retourne un nouvel itérateur inversé reculé du nombre d’éléments spécifié.
         * Disponible pour NkRandomAccessIteratorTag.
         *
         * @param (DifferenceType) offset : Nombre d’éléments à reculer.
         * @return (NkReverseIterator) : Nouvel itérateur inversé.
         */
        template<typename C = IteratorCategory, NkEnableIf_t<NkIsBaseOf_v<NkRandomAccessIteratorTag, C>>* = nullptr>
        [[nodiscard]] NkReverseIterator operator+(DifferenceType offset) const noexcept { return NkReverseIterator(mIt - offset); }

        /**
         * - operator- : Soustrait un décalage à l’itérateur inversé.
         *
         * @Description :
         * Retourne un nouvel itérateur inversé avancé du nombre d’éléments spécifié.
         * Disponible pour NkRandomAccessIteratorTag.
         *
         * @param (DifferenceType) offset : Nombre d’éléments à avancer.
         * @return (NkReverseIterator) : Nouvel itérateur inversé.
         */
        template<typename C = IteratorCategory, NkEnableIf_t<NkIsBaseOf_v<NkRandomAccessIteratorTag, C>>* = nullptr>
        [[nodiscard]] NkReverseIterator operator-(DifferenceType offset) const noexcept { return NkReverseIterator(mIt + offset); }

        /**
         * - operator- : Calcule la distance entre deux itérateurs inversés.
         *
         * @Description :
         * Retourne le nombre d’éléments entre l’itérateur courant et un autre inversé.
         * Disponible pour NkRandomAccessIteratorTag.
         *
         * @param (const NkReverseIterator&) other : Autre itérateur inversé pour le calcul.
         * @return (DifferenceType) : Distance entre les itérateurs inversés.
         */
        template<typename C = IteratorCategory, NkEnableIf_t<NkIsBaseOf_v<NkRandomAccessIteratorTag, C>>* = nullptr>
        DifferenceType operator-(const NkReverseIterator& other) const noexcept { return other.mIt - mIt; }

        /**
         * - operator+= : Avance l’itérateur inversé d’un décalage.
         *
         * @Description :
         * Modifie l’itérateur en le reculant du nombre d’éléments spécifié.
         * Disponible pour NkRandomAccessIteratorTag.
         *
         * @param (DifferenceType) offset : Nombre d’éléments à reculer.
         * @return (NkReverseIterator&) : Référence à l’itérateur modifié.
         */
        template<typename C = IteratorCategory, NkEnableIf_t<NkIsBaseOf_v<NkRandomAccessIteratorTag, C>>* = nullptr>
        NkReverseIterator& operator+=(DifferenceType offset) noexcept { mIt -= offset; return *this; }

        /**
         * - operator-= : Recule l’itérateur inversé d’un décalage.
         *
         * @Description :
         * Modifie l’itérateur en l’avançant du nombre d’éléments spécifié.
         * Disponible pour NkRandomAccessIteratorTag.
         *
         * @param (DifferenceType) offset : Nombre d’éléments à avancer.
         * @return (NkReverseIterator&) : Référence à l’itérateur modifié.
         */
        template<typename C = IteratorCategory, NkEnableIf_t<NkIsBaseOf_v<NkRandomAccessIteratorTag, C>>* = nullptr>
        NkReverseIterator& operator-=(DifferenceType offset) noexcept { mIt += offset; return *this; }

        /**
         * - operator[] : Accède à un élément par index.
         *
         * @Description :
         * Retourne une référence à l’élément situé à l’offset spécifié dans l’ordre inversé.
         * Disponible uniquement pour NkRandomAccessIteratorTag.
         *
         * @param (DifferenceType) offset : Index de l’élément à accéder.
         * @return (Reference) : Référence à l’élément.
         *
         * Exemple :
         * NkReverseIterator<NkIterator<int32_t>> rit(NkIterator<int32_t>(arr + 3));
         * printf("%d\n", rit[1]); // Affiche arr[1]
         */
        template<typename C = IteratorCategory, NkEnableIf_t<NkIsBaseOf_v<NkRandomAccessIteratorTag, C>>* = nullptr>
        [[nodiscard]] Reference operator[](DifferenceType offset) const noexcept { return *(mIt - (offset + 1)); }

        /**
     * - operator== : Compare l’égalité avec un autre itérateur inversé.
     *
     * @Description :
     * Vérifie si l’itérateur inversé pointe sur le même élément qu’un autre itérateur inversé.
     *
     * @param (const NkReverseIterator&) other : Itérateur inversé à comparer.
     * @return (bool) : `true` si les itérateurs sont égaux, `false`.
     */
    [[nodiscard]] bool operator==(const NkReverseIterator& other) const noexcept { return mIt == other.mIt; }

    /**
         * - operator!= : Compare la non-égalité avec un autre itérateur inversé.
         *
         * @Description :
         * Vérifie si l’itérateur inversé pointe sur un élément différent.
         *
         * @param (const NkReverseIterator&) other : Itérateur inversé à comparer.
         * @return (bool) : `true` si les itérateurs sont différents, `false` sinon.
         */
        [[nodiscard]] bool operator!=(const NkReverseIterator& other) const noexcept { return mIt != other.mIt; }

        /**
         * - operator< : Compare si l’itérateur inversé est avant un autre.
         *
         * @Description :
         * Vérifie si l’itérateur pointe sur un élément après un autre dans l’ordre inversé.
         * Disponible pour NkRandomAccessIteratorTag.
         *
         * @param (const NkReverseIterator&) other : Itérateur inversé à comparer.
         * @return (bool]) : `true` si l’itérateur est avant, `false` sinon.
         */
        template<typename C = IteratorCategory, NkEnableIf_t<NkIsBaseOf_v<NkRandomAccessIteratorTag, C>>* = nullptr>
        [[nodiscard]] bool operator<(const NkReverseIterator& other) const noexcept { return mIt > other.mIt; }

        /**
         * - operator<= : Compare si l’itérateur inversé est avant ou égal à un autre.
         *
         * @Description :
         * Vérifie si l’itérateur pointe sur un élément avant ou égal dans l’ordre inversé.
         * Disponible pour NkRandomAccessIteratorTag.
         *
         * @param (const NkReverseIterator&) other : Itérateur inversé à comparer.
         * @return (bool) : `true` si l’itérateur est avant ou égal, `false` sinon.
         */
        template<typename C = IteratorCategory, NkEnableIf_t<NkIsBaseOf_v<NkRandomAccessIteratorTag, C>>* = nullptr>
        [[nodiscard]] bool operator<=(const NkReverseIterator& other) const noexcept { return mIt >= other.mIt; }

        /**
         * - operator> : Compare si l’itérateur inversé est après un autre.
         *
         * @Description :
         * Vérifie si l’itérateur pointe sur un élément avant un autre dans l’ordre inversé.
         * Disponible pour NkRandomAccessIteratorTag.
         *
         * @param (const NkReverseIterator&) other : Itérateur inversé à comparer.
         * @return (bool) : `true` si l’itérateur est après, `false` sinon.
         */
        template<typename C = IteratorCategory, NkEnableIf_t<NkIsBaseOf_v<NkRandomAccessIteratorTag, C>>* = nullptr>
        [[nodiscard]] bool operator>(const NkReverseIterator& other) const noexcept { return mIt < other.mIt; }

        /**
         * - operator>= : Compare si l’itérateur inversé est après ou égal à un autre.
         *
         * @Description :
         * Vérifie si l’itérateur pointe sur un élément avant ou égal dans l’ordre inversé.
         * Disponible pour NkRandomAccessIteratorTag.
         *
         * @param (const NkReverseIterator&) other : Itérateur inversé à comparer.
         * @return (bool) : `true` si l’itérateur est après ou égal, `false` sinon
         */
        template<typename C = IteratorCategory, NkEnableIf_t<NkIsBaseOf_v<NkRandomAccessIteratorTag, C>>* = nullptr>
        [[nodiscard]] bool operator>=(const NkReverseIterator& other) const noexcept { return mIt <= other.mIt; }

    private:
        Iterator mIt;
    };

    // Forward declarations
    template<typename T, typename Category = NkForwardIteratorTag>
    class NkForwardIterator;

    template<typename T, typename Category = NkForwardIteratorTag>
    class NkConstForwardIterator;

    /**
     * - NkForwardIterator : Forward iterator for Nkentseu containers.
     *
     * @Description :
     * This class template provides a mutable forward iterator for unidirectional traversal
     * of a container, supporting read/write access and multiple passes. It uses
     * NkForwardIteratorTag and is suitable for structures like linked lists or buckets in
     * unordered maps. Optimized for performance with inline operations and no dynamic allocations.
     *
     * @Template :
     * - (typename T) : Type of elements pointed to by the iterator.
     * - (typename Category) : Iterator category (defaults to NkForwardIteratorTag).
     *
     * @Members :
     * - (Pointer) mPtr : Internal pointer to the current element or node.
     */
    template<typename T, typename Category>
    class NkForwardIterator {
    public:
        using ValueType = T;
        using Pointer = T*;
        using Reference = T&;
        using DifferenceType = ptrdiff;
        using IteratorCategory = Category;

        /**
         * - NkForwardIterator : Default constructor.
         *
         * @Description :
         * Initializes an iterator with a null pointer, representing an undefined state.
         */
        constexpr NkForwardIterator() noexcept : mPtr(nullptr) {}

        /**
         * - NkForwardIterator : Constructor with a pointer.
         *
         * @Description :
         * Initializes an iterator pointing to the specified element.
         *
         * @param (Pointer) ptr : Pointer to the element to iterate.
         */
        explicit constexpr NkForwardIterator(Pointer ptr) noexcept : mPtr(ptr) {}

        /**
         * - operator* : Accesses the current element.
         *
         * @Description :
         * Returns a mutable reference to the element pointed to by the iterator.
         * No validity check for performance; debug mode logs errors.
         *
         * @return (Reference) : Reference to the current element.
         */
        [[nodiscard]] Reference operator*() const noexcept {
            #ifdef NKENTSEU_LOG_ENABLED
            if (mPtr == nullptr) {
                NKENTSEU_LOG_ERROR("NkForwardIterator: Dereferencing null pointer.");
            }
            #endif
            return *mPtr;
        }

        /**
         * - operator-> : Accesses the current element via pointer.
         *
         * @Description :
         * Returns the raw pointer to the current element for member access.
         *
         * @return (Pointer) : Pointer to the current element.
         */
        [[nodiscard]] Pointer operator->() const noexcept {
            #ifdef NKENTSEU_LOG_ENABLED
            if (mPtr == nullptr) {
                NKENTSEU_LOG_ERROR("NkForwardIterator: Accessing null pointer.");
            }
            #endif
            return mPtr;
        }

        /**
         * - operator++ : Increments the iterator (prefix).
         *
         * @Description :
         * Advances the iterator to the next element and returns a reference to itself.
         *
         * @return (NkForwardIterator&) : Reference to the iterator after increment.
         */
        NkForwardIterator& operator++() noexcept {
            ++mPtr;
            return *this;
        }

        /**
         * - operator++ : Increments the iterator (postfix).
         *
         * @Description :
         * Advances the iterator to the next element and returns a copy of the iterator before increment.
         *
         * @return (NkForwardIterator) : Copy of the iterator before increment.
         */
        [[nodiscard]] NkForwardIterator operator++(int) noexcept {
            NkForwardIterator tmp = *this;
            ++mPtr;
            return tmp;
        }

        /**
         * - operator== : Compares equality with another iterator.
         *
         * @Description :
         * Checks if the iterator points to the same element as another iterator.
         *
         * @param (const NkForwardIterator&) other : Iterator to compare.
         * @return (bool) : True if iterators are equal, false otherwise.
         */
        [[nodiscard]] bool operator==(const NkForwardIterator& other) const noexcept {
            return mPtr == other.mPtr;
        }

        /**
         * - operator!= : Compares inequality with another iterator.
         *
         * @Description :
         * Checks if the iterator points to a different element than another iterator.
         *
         * @param (const NkForwardIterator&) other : Iterator to compare.
         * @return (bool) : True if iterators are different, false otherwise.
         */
        [[nodiscard]] bool operator!=(const NkForwardIterator& other) const noexcept {
            return mPtr != other.mPtr;
        }

        /**
         * - operator== : Compares equality with a null pointer.
         *
         * @Description :
         * Checks if the iterator points to a null element.
         *
         * @param (mem::nknullptr) : Null pointer to compare.
         * @return (bool) : True if the iterator is null, false otherwise.
         */
        [[nodiscard]] bool operator==(mem::nknullptr) const noexcept {
            return mPtr == nullptr;
        }

        /**
         * - operator!= : Compares inequality with a null pointer.
         *
         * @Description :
         * Checks if the iterator points to a non-null element.
         *
         * @param (mem::nknullptr) : Null pointer to compare.
         * @return (bool) : True if the iterator is non-null, false otherwise.
         */
        [[nodiscard]] bool operator!=(mem::nknullptr) const noexcept {
            return mPtr != nullptr;
        }

        /**
         * - operator== : Compares equality with a const iterator.
         *
         * @Description :
         * Checks if the iterator points to the same element as a const iterator.
         *
         * @param (const NkConstForwardIterator<T, Category>&) other : Const iterator to compare.
         * @return (bool) : True if iterators are equal, false otherwise.
         */
        [[nodiscard]] bool operator==(const NkConstForwardIterator<T, Category>& other) const noexcept {
            return mPtr == other.operator->();
        }

        /**
         * - operator!= : Compares inequality with a const iterator.
         *
         * @Description :
         * Checks if the iterator points to a different element than a const iterator.
         *
         * @param (const NkConstForwardIterator<T, Category>&) other : Const iterator to compare.
         * @return (bool) : True if iterators are different, false otherwise.
         */
        [[nodiscard]] bool operator!=(const NkConstForwardIterator<T, Category>& other) const noexcept {
            return mPtr != other.operator->();
        }

    private:
        Pointer mPtr;
    };

    /**
     * - NkConstForwardIterator : Const forward iterator for Nkentseu containers.
     *
     * @Description :
     * This class template provides a const forward iterator for unidirectional traversal
     * of a container in read-only mode. It supports conversion from NkForwardIterator
     * for safe const access and is optimized for performance.
     *
     * @Template :
     * - (typename T) : Type of elements pointed to by the iterator.
     * - (typename Category) : Iterator category (defaults to NkForwardIteratorTag).
     *
     * @Members :
     * - (Pointer) mPtr : Internal pointer to the current element or node.
     */
    template<typename T, typename Category>
    class NkConstForwardIterator {
    public:
        using ValueType = T;
        using Pointer = const T*;
        using Reference = const T&;
        using DifferenceType = ptrdiff;
        using IteratorCategory = Category;

        /**
         * - NkConstForwardIterator : Default constructor.
         *
         * @Description :
         * Initializes an iterator with a null pointer.
         */
        constexpr NkConstForwardIterator() noexcept : mPtr(nullptr) {}

        /**
         * - NkConstForwardIterator : Constructor with a pointer.
         *
         * @Description :
         * Initializes an iterator pointing to the specified element.
         *
         * @param (Pointer) ptr : Pointer to the element to iterate.
         */
        explicit constexpr NkConstForwardIterator(Pointer ptr) noexcept : mPtr(ptr) {}

        /**
         * - NkConstForwardIterator : Conversion constructor from NkForwardIterator.
         *
         * @Description :
         * Constructs a const iterator from a mutable iterator for safe read-only access.
         *
         * @param (const NkForwardIterator<U, Category>&) other : Mutable iterator source.
         */
        template<typename U = T, typename = NkEnableIf_t<!NkIsConst_v<U>>>
        constexpr NkConstForwardIterator(const NkForwardIterator<U, Category>& other) noexcept
            : mPtr(other.operator->()) {}

        /**
         * - operator* : Accesses the current element.
         *
         * @Description :
         * Returns a const reference to the element pointed to by the iterator.
         *
         * @return (Reference) : Const reference to the current element.
         */
        [[nodiscard]] Reference operator*() const noexcept {
            #ifdef NKENTSEU_LOG_ENABLED
            if (mPtr == nullptr) {
                NKENTSEU_LOG_ERROR("NkConstForwardIterator: Dereferencing null pointer.");
            }
            #endif
            return *mPtr;
        }

        /**
         * - operator-> : Accesses the current element via pointer.
         *
         * @Description :
         * Returns the const pointer to the current element.
         *
         * @return (Pointer) : Const pointer to the current element.
         */
        [[nodiscard]] Pointer operator->() const noexcept {
            #ifdef NKENTSEU_LOG_ENABLED
            if (mPtr == nullptr) {
                NKENTSEU_LOG_ERROR("NkConstForwardIterator: Accessing null pointer.");
            }
            #endif
            return mPtr;
        }

        /**
         * - operator++ : Increments the iterator (prefix).
         *
         * @Description :
         * Advances the iterator to the next element.
         *
         * @return (NkConstForwardIterator&) : Reference to the iterator after increment.
         */
        NkConstForwardIterator& operator++() noexcept {
            ++mPtr;
            return *this;
        }

        /**
         * - operator++ : Increments the iterator (postfix).
         *
         * @Description :
         * Advances the iterator and returns a copy before increment.
         *
         * @return (NkConstForwardIterator) : Copy of the iterator before increment.
         */
        [[nodiscard]] NkConstForwardIterator operator++(int) noexcept {
            NkConstForwardIterator tmp = *this;
            ++mPtr;
            return tmp;
        }

        /**
         * - operator== : Compares equality with another const iterator.
         *
         * @Description :
         * Checks if the iterator points to the same element as another const iterator.
         *
         * @param (const NkConstForwardIterator&) other : Iterator to compare.
         * @return (bool) : True if iterators are equal, false otherwise.
         */
        [[nodiscard]] bool operator==(const NkConstForwardIterator& other) const noexcept {
            return mPtr == other.mPtr;
        }

        /**
         * - operator!= : Compares inequality with another const iterator.
         *
         * @Description :
         * Checks if the iterator points to a different element than another const iterator.
         *
         * @param (const NkConstForwardIterator&) other : Iterator to compare.
         * @return (bool) : True if iterators are different, false otherwise.
         */
        [[nodiscard]] bool operator!=(const NkConstForwardIterator& other) const noexcept {
            return mPtr != other.mPtr;
        }

        /**
         * - operator== : Compares equality with a null pointer.
         *
         * @Description :
         * Checks if the iterator points to a null element.
         *
         * @param (mem::nknullptr) : Null pointer to compare.
         * @return (bool) : True if the iterator is null, false otherwise.
         */
        [[nodiscard]] bool operator==(mem::nknullptr) const noexcept {
            return mPtr == nullptr;
        }

        /**
         * - operator!= : Compares inequality with a null pointer.
         *
         * @Description :
         * Checks if the iterator points to a non-null element.
         *
         * @param (mem::nknullptr) : Null pointer to compare.
         * @return (bool) : True if the iterator is non-null, false otherwise.
         */
        [[nodiscard]] bool operator!=(mem::nknullptr) const noexcept {
            return mPtr != nullptr;
        }

        /**
         * - operator== : Compares equality with a mutable iterator.
         *
         * @Description :
         * Checks if the const iterator points to the same element as a mutable iterator.
         *
         * @param (const NkForwardIterator<T, Category>&) other : Mutable iterator to compare.
         * @return (bool) : True if iterators are equal, false otherwise.
         */
        [[nodiscard]] bool operator==(const NkForwardIterator<T, Category>& other) const noexcept {
            return mPtr == other.operator->();
        }

        /**
         * - operator!= : Compares inequality with a mutable iterator.
         *
         * @Description :
         * Checks if the const iterator points to a different element than a mutable iterator.
         *
         * @param (const NkForwardIterator<T, Category>&) other : Mutable iterator to compare.
         * @return (bool) : True if iterators are different, false otherwise.
         */
        [[nodiscard]] bool operator!=(const NkForwardIterator<T, Category>& other) const noexcept {
            return mPtr != other.operator->();
        }

    private:
        Pointer mPtr;
    };

    // Update NkIteratorTraits for NkForwardIterator
    template<typename T, typename Category>
    struct NkIteratorTraits<NkForwardIterator<T, Category>> {
        using ValueType = typename NkForwardIterator<T, Category>::ValueType;
        using Pointer = typename NkForwardIterator<T, Category>::Pointer;
        using Reference = typename NkForwardIterator<T, Category>::Reference;
        using DifferenceType = typename NkForwardIterator<T, Category>::DifferenceType;
        using IteratorCategory = typename NkForwardIterator<T, Category>::IteratorCategory;
    };

    // Update NkIteratorTraits for NkConstForwardIterator
    template<typename T, typename Category>
    struct NkIteratorTraits<NkConstForwardIterator<T, Category>> {
        using ValueType = typename NkConstForwardIterator<T, Category>::ValueType;
        using Pointer = typename NkConstForwardIterator<T, Category>::Pointer;
        using Reference = typename NkConstForwardIterator<T, Category>::Reference;
        using DifferenceType = typename NkConstForwardIterator<T, Category>::DifferenceType;
        using IteratorCategory = typename NkConstForwardIterator<T, Category>::IteratorCategory;
    };

    // Existing NkBidirectionalIterator, NkConstBidirectionalIterator, NkIterator, 
    // NkConstIterator, NkReverseIterator, and utility functions (GetBegin, GetEnd, NkForeach, etc.)

    // Update GetBegin/GetEnd for forward iterators
    template<typename T, typename Category>
    NkForwardIterator<T, Category> GetBegin(T* array) noexcept {
        return NkForwardIterator<T, Category>(array);
    }

    template<typename T, typename Category>
    NkForwardIterator<T, Category> GetEnd(T* array, usize size) noexcept {
        return NkForwardIterator<T, Category>(array + size);
    }

    /**
     * - NkIterator : Itérateur générique pour parcourir des conteneurs.
     *
     * @Description :
     * Cette classe template permet de créer des itérateurs pour différents types de conteneurs,
     * en fournissant des opérations d'accès, d'incrémentation et de comparaison.
     *
     * @Template :
     * - (typename T) : Type des éléments pointés par l'itérateur.
     * - (typename Category) : Catégorie d'itérateur (par défaut NkRandomAccessIteratorTag).
     *
     * @Members :
     * - (Pointer) mPtr : Pointeur interne vers l'élément courant.
     */
    template<typename Container>
    auto GetBegin(Container& container) -> decltype(container.Begin()) {
        return container.Begin();
    }
    
    /*
    * - GetBegin : Obtient l'itérateur de début d'un tableau.

    * @Description :
    * Cette fonction template retourne un itérateur pointant sur le premier élément d'un tableau.
    * Elle est optimisée pour les tableaux de taille fixe et ne nécessite pas de vérification de validité.
    * 
    * @Template :
    * - (typename T, usize N) : Type des éléments du tableau et sa taille.
    * 
    * @param (T (&array)[N]) array : Tableau dont on veut l'itérateur de début.
    * @return (NkIterator<T>) : Itérateur pointant sur le premier élément du tableau.
    */
    template<typename T, usize N>
    NkIterator<T> GetBegin(T (&array)[N]) noexcept {
        return NkIterator<T>(array);
    }

    /**
     * - GetEnd : Obtient l'itérateur de fin d'un conteneur.
     *
     * @Description :
     * Cette fonction template retourne un itérateur pointant sur la fin d'un conteneur.
     * Elle est compatible avec tout conteneur ayant une méthode `End()`.
     *
     * @Template :
     * - (typename Container) : Type du conteneur.
     *
     * @param (Container&) container : Conteneur dont on veut l'itérateur de fin.
     * @return (decltype(container.End())) : Itérateur pointant sur la fin du conteneur.
     */
    template<typename Container>
    auto GetEnd(Container& container) -> decltype(container.End()) {
        return container.End();
    }

    /**
     * - GetEnd : Obtient l'itérateur de fin d'un tableau.
     *
     * @Description :
     * Cette fonction template retourne un itérateur pointant sur la fin d'un tableau.
     * Elle est optimisée pour les tableaux de taille fixe et ne nécessite pas de vérification de validité.
     *
     * @Template :
     * - (typename T, usize N) : Type des éléments du tableau et sa taille.
     *
     * @param (T (&array)[N]) array : Tableau dont on veut l'itérateur de fin.
     * @return (NkIterator<T>) : Itérateur pointant sur la fin du tableau.
     */
    template<typename T, usize N>
    NkIterator<T> GetEnd(T (&array)[N]) noexcept {
        return NkIterator<T>(array + N);
    }

    // Mise à jour des fonctions utilitaires pour supporter NkBidirectionalIterator
    template<typename T, typename Category>
    NkBidirectionalIterator<T, Category> GetBegin(T* array, usize) noexcept {
        return NkBidirectionalIterator<T, Category>(array);
    }

    template<typename T, typename Category>
    NkBidirectionalIterator<T, Category> GetEnd(T* array, usize size) noexcept {
        return NkBidirectionalIterator<T, Category>(array + size);
    }

    template<typename Container>
    auto GetBegin(const Container& container) -> decltype(container.Begin()) {
        return container.Begin();
    }

    template<typename Container>
    auto GetEnd(const Container& container) -> decltype(container.End()) {
        return container.End();
    }

    /**
     * - NkForeach : Applique une opération à chaque élément d’une plage.
     *
     * @Description :
     * Cette fonction template parcourt une plage d’itérateurs et applique l’opération donnée à chaque élément.
     * Elle est optimisée pour éviter les copies inutiles et est compatible avec tout itérateur supportant l’incrémentation.
     * En mode débogage, vérifie la validité des itérateurs.
     *
     * @Template :
     * - (typename Iterator) : Type de l’itérateur (par exemple, `NkIterator` ou `NkConstIterator`).
     * - (typename Operation) : Type de l’opération (par exemple, lambda ou fonction).
     *
     * @param (Iterator) begin : Itérateur de début de la plage.
     * @param (Iterator) end : Itérateur de fin de la plage.
     * @param (Operation) op : Opération à appliquer à chaque élément.
     * @return (void) : Aucune valeur retournée.
     *
     * @Lambda print_value :
     * @Description Affiche la valeur de l’élément courant.
     * @Parameters (T&) value : Élément courant de la plage.
     * @Return (void) : Aucune valeur.
     *
     * Exemple :
     * int32_t arr[] = {1, 2, 3};
     * NkForeach(NkIterator<int32_t>(arr), NkIterator<int32_t>(arr + 3),
     *             [](int32_t& value) { printf("%d\n", value); }); // Affiche : 1, 2, 3
     */
    template<typename Iterator, typename Operation>
    NKENTSEU_FORCE_INLINE void NkForeach(Iterator begin, Iterator end, Operation op) noexcept {
        #ifdef NKENTSEU_LOG_ENABLED
        if (begin == nullptr || end == nullptr) {
            NKENTSEU_LOG_ERROR("NkForeach: Invalid iterators provided.");
            return;
        }
        #endif

        for (; begin != end; ++begin) {
            op(*begin);
        }
    }

    /**
     * - NkForeach : Applique une opération à chaque élément d’un conteneur.
     *
     * @Description :
     * Cette surcharge appelle NkForeach sur les itérateurs `Begin()` et `End()` du conteneur.
     * Compatible avec tout conteneur ayant ces méthodes (par exemple, NkInitializerList).
     *
     * @Template :
     * - (typename Container) : Type du conteneur.
     * - (typename Operation) : Type de l’opération.
     *
     * @param (const Container&) container : Conteneur à parcourir.
     * @param (Operation) op : Opération à appliquer.
     * @return (void) : Aucune valeur retournée.
     *
     * Exemple :
     * auto list = MakeInitializerList<int32_t>({1, 2, 3}, 3);
     * NkForeach(list, [](int32_t value) { printf("%d\n", value); }); // Affiche : 1, 2, 3
     */
    template<typename Container, typename Operation>
    NKENTSEU_FORCE_INLINE void NkForeach(const Container& container, Operation op) noexcept {
        NkForeach(GetBegin(container), GetEnd(container), op);
    }

    /**
     * - NkForeachIndexed : Applique une opération à chaque élément avec son index.
     *
     * @Description :
     * Parcourt une plage d’itérateurs et applique l’opération donnée à chaque élément,
     * en passant également l’index de l’élément. Non marqué `noexcept` car l’indexation peut lever des exceptions.
     *
     * @Template :
     * - (typename Iterator) : Type de l’itérateur.
     * - (typename Operation) : Type de l’opération.
     *
     * @param (Iterator) begin : Itérateur de début de la plage.
     * @param (Iterator) end : Itérateur de fin de la plage.
     * @param (Operation) op : Opération à appliquer, prenant l’élément et l’index.
     * @return (void) : Aucune valeur retournée.
     *
     * Exemple :
     * int32_t arr[] = {1, 2, 3};
     * NkForeachIndexed(NkIterator<int32_t>(arr), NkIterator<int32_t>(arr + 3),
     *                    [](int32_t& value, usize i) { printf("%zu: %d\n", i, value); });
     * // Affiche : 0:1, 1:2, 2:3
     */
    template<typename Iterator, typename Operation>
    void NkForeachIndexed(Iterator begin, Iterator end, Operation op) {
        for (usize i = 0; begin != end; ++begin, ++i) {
            op(*begin, i);
        }
    }

    /**
     * - NkForeachWithBreak : Applique une opération avec possibilité d’arrêt.
     *
     * @Description :
     * Parcourt une plage et applique une opération jusqu’à ce qu’une condition d’arrêt soit satisfaite.
     * Non marqué `noexcept` car la condition peut lever des exceptions.
     *
     * @Template :
     * - (typename Iterator) : Type de l’itérateur.
     * - (typename Operation) : Type de l’opération.
     * - (typename BreakCondition) : Type de la condition d’arrêt.
     *
     * @param (Iterator) begin : Itérateur de début de la plage.
     * @param (Iterator) end : Itérateur de fin de la plage.
     * @param (Operation) op : Opération à appliquer.
     * @param (BreakCondition) breakCond : Condition pour arrêter l’itération.
     * @return (void) : Aucune valeur retournée.
     *
     * Exemple :
     * int32_t arr[] = {1, 2, 3, 4};
     * NkForeachWithBreak(NkIterator<int32_t>(arr), NkIterator<int32_t>(arr + 4),
     *                    [](int32_t& value) { printf("%d\n", value); },
     *                    [](int32_t value) { return value == 3; });
     * // Affiche : 1, 2, 3
     */
    template<typename Iterator, typename Operation, typename BreakCondition>
    void NkForeachWithBreak(Iterator begin, Iterator end, Operation op, BreakCondition breakCond) {
        for (; begin != end; ++begin) {
            op(*begin);
            if (breakCond(*begin)) break;
        }
    }

    /**
     * - NkForeachReverse : Applique une opération à chaque élément en ordre inverse.
     *
     * @Description :
     * Parcourt un conteneur en utilisant ses itérateurs inversés (`RBegin`, `REnd`).
     * Compatible avec tout conteneur ayant ces méthodes.
     *
     * @Template :
     * - (typename Container) : Type du conteneur.
     * - (typename Operation) : Type de l’opération.
     *
     * @param (const Container&) container : Conteneur à parcourir.
     * @param (Operation) op : Opération à appliquer.
     * @return (void) : Aucune valeur retournée.
     *
     * Exemple :
     * auto list = MakeInitializerList<int32_t>({1, 2, 3}, 3);
     * NkForeachReverse(list, [](int32_t value) { printf("%d\n", value); });
     * // Affiche : 3, 2, 1
     */
    template<typename Container, typename Operation>
    void NkForeachReverse(const Container& container, Operation op) {
        NkForeach(GetEnd(container), GetBegin(container), op);
        // NkForeach(container.RBegin(), container.REnd(), op);
    }

    /**
     * - NkWhileach : Itère avec une condition, applique une opération.
     *
     * @Description :
     * Parcourt une plage d’itérateurs tant qu’une condition est vraie, appliquant une opération.
     * Optimisée avec `noexcept` pour les cas où la condition et l’opération sont sécurisées.
     *
     * @Template :
     * - (typename Iterator) : Type de l’itérateur.
     * - (typename Condition) : Type de la condition.
     * - (typename Operation) : Type de l’opération.
     *
     * @param (Iterator) begin : Itérateur de début de la plage.
     * @param (Iterator) end : Itérateur de fin de la plage.
     * @param (Condition) cond : Condition pour continuer l’itération.
     * @param (Operation) op : Opération à appliquer.
     * @return (void) : Aucune valeur retournée.
     *
     * Exemple :
     * int32_t arr[] = {1, 2, 3, 4};
     * NkWhileach(NkIterator<int32_t>(arr), NkIterator<int32_t>(arr + 4),
     *                 [](int32_t value) { return value < 3; },
     *                 [](int32_t& value) { printf("%d\n", value); });
     * // Affiche : 1, 2
     */
    template<typename Iterator, typename Condition, typename Operation>
    NKENTSEU_FORCE_INLINE void NkWhileach(Iterator begin, Iterator end, Condition cond, Operation op) noexcept {
        #ifdef NKENTSEU_LOG_ENABLED
        if (begin == nullptr || end == nullptr) {
            NKENTSEU_LOG_ERROR("NkWhileach: Invalid iterators provided.");
            return;
        }
        #endif

        for (; begin != end && cond(*begin); ++begin) {
            op(*begin);
        }
    }

    /**
     * - NkWhileach : Itère avec une condition sur un conteneur.
     *
     * @Description :
     * Surcharge pour appliquer NkWhileach sur les itérateurs d’un conteneur.
     *
     * @Template :
     * - (typename Container) : Type du conteneur.
     * - (typename Condition) : Type de la condition.
     * - (typename Operation) : Type de l’opération.
     *
     * @param (const Container&) container : Conteneur à parcourir.
     * @param (Condition) cond : Condition pour continuer l’itération.
     * @param (Operation) op : Opération à appliquer.
     * @return (void) : Aucune valeur retournée.
     *
     * Exemple :
     * auto list = MakeInitializerList({1, 2, 3, 4}, 4);
     * NkWhileach(list, [](int32_t value) { return value < 3; },
     *                    [](int32_t value) { printf("%d\n", value); });
     * // Affiche : 1, 2
     */
    template<typename Container, typename Condition, typename Operation>
    NKENTSEU_FORCE_INLINE void NkWhileach(const Container& container, Condition cond, Operation op) noexcept {
        NkWhileach(GetBegin(container), GetEnd(container), cond, op);
    }

    // ============================================================================
    // Macros de comptage d'arguments et sélecteurs pour les boucles foreach
    // ============================================================================
    #define NK_NARG(...)  NK_NARG_(__VA_ARGS__, 8,7,6,5,4,3,2,1,0)
    #define NK_NARG_(_1,_2,_3,_4,_5,_6,_7,_8,N,...) N
    #define NK_SELECT(_1,_2,_3,NAME,...) NAME

    /**
     * @Description : Macro pour une boucle Foreach intuitive.
     * Permet une syntaxe proche de `for (auto& value : container)` pour itérer sur un conteneur.
     * Utilisation:
     *   - nkforeach(conteneur, variable)                 -> auto&& variable
     *   - nkforeach(conteneur, qualificateur, variable)  -> qualificateur variable
     */
    #define nkforeach(...) \
        NK_SELECT(__VA_ARGS__, NK_FOREACH_3, NK_FOREACH_2,)(__VA_ARGS__)

    #define NK_FOREACH_2(container, value) \
        for (auto it = GetBegin(container), end = GetEnd(container); it != end; ++it) \
            for (bool first = true; first; first = false) \
                for (auto&& value = *it; first; first = false)

    #define NK_FOREACH_3(container, qualifier, value) \
        for (auto it = GetBegin(container), end = GetEnd(container); it != end; ++it) \
            for (bool first = true; first; first = false) \
                for (qualifier value = *it; first; first = false)

    #define nkforeachbase(...) \
        NK_SELECT(__VA_ARGS__, NK_FOREACHBASE_3, NK_FOREACHBASE_2,)(__VA_ARGS__)

    #define NK_FOREACHBASE_2(container, value) \
        for (auto it = (container).Begin(), end = (container).End(); it != end; ++it) \
            for (bool first = true; first; first = false) \
                for (auto&& value = *it; first; first = false)

    #define NK_FOREACHBASE_3(container, qualifier, value) \
        for (auto it = (container).Begin(), end = (container).End(); it != end; ++it) \
            for (bool first = true; first; first = false) \
                for (qualifier value = *it; first; first = false)

    #define nkwhileach(...) \
        NK_SELECT(__VA_ARGS__, NK_WHILEACH_4, NK_WHILEACH_3,)(__VA_ARGS__)

    #define NK_WHILEACH_3(container, value, condition) \
        for (auto it = GetBegin(container), end = GetEnd(container); it != end && (condition(*it)); ++it) \
            for (bool first = true; first; first = false) \
                for (auto&& value = *it; first; first = false)

    #define NK_WHILEACH_4(container, qualifier, value, condition) \
        for (auto it = GetBegin(container), end = GetEnd(container); it != end && (condition(*it)); ++it) \
            for (bool first = true; first; first = false) \
                for (qualifier value = *it; first; first = false)

    /**
     * @Description : Macro pour une boucle Whileach avec condition.
     * Permet une syntaxe conditionnelle pour itérer tant qu’une condition est vraie.
     */
    #define nkwhileachbase(...) \
        NK_SELECT(__VA_ARGS__, NK_WHILEACHBASE_4, NK_WHILEACHBASE_3,)(__VA_ARGS__)

    #define NK_WHILEACHBASE_3(container, value, condition) \
        for (auto it = (container).Begin(), end = (container).End(); it != end && (condition(*it)); ++it) \
            for (bool first = true; first; first = false) \
                for (auto&& value = *it; first; first = false)

    #define NK_WHILEACHBASE_4(container, qualifier, value, condition) \
        for (auto it = (container).Begin(), end = (container).End(); it != end && (condition(*it)); ++it) \
            for (bool first = true; first; first = false) \
                for (qualifier value = *it; first; first = false)

                
    template<typename T>
    struct NkIsIterator : NkFalseType {};

    template<typename T>
    struct NkIsIterator<T*> : NkTrueType {};

    template<typename T>
    struct NkIsIterator<const T*> : NkTrueType {};

    template<typename T>
    struct NkIsIterator<NkIterator<T>> : NkTrueType {};

    template<typename T>
    inline constexpr bool NkIsIterator_v = NkIsIterator<T>::value;

        /**
     * - NkAdvance : Avance un itérateur de n positions.
     *
     * @Description :
     * Cette fonction template avance l'itérateur donné du nombre spécifié de positions.
     * Utilise l'opérateur += si disponible (random access) ou une boucle sinon.
     *
     * @Template :
     * - (typename Iterator) : Type de l'itérateur.
     *
     * @param (Iterator&) it : Itérateur à avancer.
     * @param (typename NkIteratorTraits<Iterator>::DifferenceType) n : Nombre de positions à avancer.
     * @return (void) : Aucune valeur retournée.
     */
    template<typename Iterator>
    NKENTSEU_FORCE_INLINE void NkAdvance(Iterator& it, typename NkIteratorTraits<Iterator>::DifferenceType n) noexcept {
        it += n;
    }

    /**
     * - NkDistance : Calcule la distance entre deux itérateurs.
     *
     * @Description :
     * Retourne le nombre d'éléments entre first et last.
     *
     * @Template :
     * - (typename Iterator) : Type de l'itérateur.
     *
     * @param (Iterator) first : Itérateur de début.
     * @param (Iterator) last : Itérateur de fin.
     * @return (typename NkIteratorTraits<Iterator>::DifferenceType) : Distance entre les itérateurs.
     */
    template<typename Iterator>
    NKENTSEU_FORCE_INLINE typename NkIteratorTraits<Iterator>::DifferenceType NkDistance(Iterator first, Iterator last) noexcept {
        return last - first;
    }

    /**
     * - NkNext : Retourne un itérateur avancé de n positions.
     *
     * @Description :
     * Crée une copie de l'itérateur et l'avance de n positions.
     *
     * @Template :
     * - (typename Iterator) : Type de l'itérateur.
     *
     * @param (Iterator) it : Itérateur de base.
     * @param (typename NkIteratorTraits<Iterator>::DifferenceType) n : Nombre de positions à avancer (défaut: 1).
     * @return (Iterator) : Itérateur avancé.
     */
    template<typename Iterator>
    NKENTSEU_FORCE_INLINE Iterator NkNext(Iterator it, typename NkIteratorTraits<Iterator>::DifferenceType n = 1) noexcept {
        return it + n;
    }

    /**
     * - NkPrev : Retourne un itérateur reculé de n positions.
     *
     * @Description :
     * Crée une copie de l'itérateur et le recule de n positions.
     *
     * @Template :
     * - (typename Iterator) : Type de l'itérateur.
     *
     * @param (Iterator) it : Itérateur de base.
     * @param (typename NkIteratorTraits<Iterator>::DifferenceType) n : Nombre de positions à reculer (défaut: 1).
     * @return (Iterator) : Itérateur reculé.
     */
    template<typename Iterator>
    NKENTSEU_FORCE_INLINE Iterator NkPrev(Iterator it, typename NkIteratorTraits<Iterator>::DifferenceType n = 1) noexcept {
        return it - n;
    }

} // namespace nkentseu
