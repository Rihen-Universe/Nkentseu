// =============================================================================
// NKCore/NkTraits.h
// Implémentation portable des traits de type C++ pour le framework NKCore.
//
// Design :
//  - Réimplémentation complète des std::type_traits sans dépendance STL
//  - Support C++11+ avec fallbacks pour les compilateurs anciens
//  - Traits supplémentaires pour les types NKCore (NkIsCharacterType, etc.)
//  - Optimisations via intrinsèques du compilateur (__is_trivially_constructible, etc.)
//  - Compatibilité avec les types NKCore (nk_int8, nk_uint32, etc.)
//  - Namespace nkentseu::traits avec alias nkentseu::traits_alias
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_CORE_NKTRAITS_H
#define NKENTSEU_CORE_NKTRAITS_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion des types NKCore pour la compatibilité avec les traits.
    // <type_traits> est inclus uniquement pour les fallbacks sur les compilateurs modernes.

    #include "NKCore/NkTypes.h"

    // Inclusion conditionnelle de <type_traits> pour les fallbacks
    #if defined(__cplusplus) && __cplusplus >= 201103L
        #include <type_traits>
    #endif

    // -------------------------------------------------------------------------
    // SECTION 2 : ESPACE DE NOMS PRINCIPAL (INDENTATION HIÉRARCHIQUE)
    // -------------------------------------------------------------------------
    // Déclaration de l'espace de noms principal nkentseu::traits.
    // Les traits sont organisés en catégories logiques avec documentation Doxygen.
    //
    // NOTE SUR L'INDENTATION :
    //  - Le contenu de 'nkentseu' est indenté d'un niveau
    //  - Le contenu de 'traits' (dans 'nkentseu') est indenté de deux niveaux

    namespace nkentseu {

        namespace traits {

            // =================================================================
            // SECTION 3 : TRAITS DE BASE (FOUNDATION)
            // =================================================================
            // Implémentation des traits fondamentaux utilisés par tous les autres.

            /**
             * @brief Représentation compile-time d'une constante intégrale
             * @struct NkIntegralConstant
             * @tparam T Type de la valeur (doit être intégral)
             * @tparam V Valeur constante
             * @ingroup BaseTraits
             *
             * Équivalent à std::integral_constant. Sert de base pour les traits booléens.
             *
             * @example
             * @code
             * using TrueType = NkIntegralConstant<bool, true>;
             * static_assert(TrueType::value == true);
             * @endcode
             */
            template <typename T, T V>
            struct NkIntegralConstant {
                /** @brief Valeur constante stockée */
                static constexpr T value = V;

                /** @brief Type de la valeur */
                using value_type = T;

                /** @brief Type de cette instanciation */
                using type = NkIntegralConstant<T, V>;

                /**
                 * @brief Opérateur de conversion implicite vers T
                 * @return La valeur constante
                 */
                constexpr operator value_type() const noexcept {
                    return value;
                }

                /**
                 * @brief Opérateur d'appel pour obtenir la valeur
                 * @return La valeur constante
                 */
                constexpr value_type operator()() const noexcept {
                    return value;
                }
            };

            /**
             * @brief Alias pour les constantes booléennes
             * @typedef NkBoolConstant
             * @tparam V Valeur booléenne (true/false)
             * @ingroup BaseTraits
             */
            template <nk_bool V>
            using NkBoolConstant = NkIntegralConstant<nk_bool, V>;

            /**
             * @brief Type représentant true
             * @typedef NkTrueType
             * @ingroup BaseTraits
             */
            using NkTrueType = NkBoolConstant<true>;

            /**
             * @brief Type représentant false
             * @typedef NkFalseType
             * @ingroup BaseTraits
             */
            using NkFalseType = NkBoolConstant<false>;

            /**
             * @brief Valeur constexpr pour NkTrueType
             * @def NkTrueType_v
             * @ingroup BaseTraits
             */
            inline constexpr bool NkTrueType_v = NkTrueType::value;

            /**
             * @brief Valeur constexpr pour NkFalseType
             * @def NkFalseType_v
             * @ingroup BaseTraits
             */
            inline constexpr bool NkFalseType_v = NkFalseType::value;

            /**
             * @brief Metafonction void pour SFINAE
             * @typedef NkVoidT
             * @tparam ... Types ignorés
             * @ingroup BaseTraits
             *
             * Utilisée dans les expressions SFINAE pour créer des conditions de substitution.
             *
             * @example
             * @code
             * template<typename T, typename = NkVoidT<decltype(std::declval<T>().size())>>
             * void process(T& container) { /\* ... *\/ }
             * @endcode
             */
            template <typename...>
            using NkVoidT = void;

            /**
             * @brief Identité de type
             * @struct NkTypeIdentity
             * @tparam T Type à envelopper
             * @ingroup BaseTraits
             *
             * Permet de forcer l'évaluation d'un type dans un contexte non-déduit.
             *
             * @example
             * @code
             * template<typename T>
             * void foo(NkTypeIdentity_t<T> param);  // T doit être spécifié explicitement
             * @endcode
             */
            template <typename T>
            struct NkTypeIdentity {
                using type = T;
            };

            /**
             * @brief Alias pour NkTypeIdentity
             * @typedef NkTypeIdentity_t
             * @tparam T Type à envelopper
             * @ingroup BaseTraits
             */
            template <typename T>
            using NkTypeIdentity_t = typename NkTypeIdentity<T>::type;

            // =================================================================
            // SECTION 4 : TRAITS CONDITIONNELS
            // =================================================================
            // Implémentation des traits pour la métaprogrammation conditionnelle.

            /**
             * @brief Activation conditionnelle de type (SFINAE)
             * @struct NkEnableIf
             * @tparam B Condition booléenne
             * @tparam T Type à activer si B est true
             * @ingroup ConditionalTraits
             *
             * Équivalent à std::enable_if. Utilisé pour activer/désactiver des templates.
             *
             * @example
             * @code
             * template<typename T>
             * NkEnableIf_t<NkIsIntegral_v<T>, T> add(T a, T b) {
             *     return a + b;  // Seulement pour les types intégraux
             * }
             * @endcode
             */
            template <nk_bool B, typename T = void>
            struct NkEnableIf {};

            /**
             * @brief Spécialisation pour condition true
             * @ingroup ConditionalTraits
             */
            template <typename T>
            struct NkEnableIf<true, T> {
                using type = T;
            };

            /**
             * @brief Alias pour NkEnableIf
             * @typedef NkEnableIf_t
             * @tparam B Condition booléenne
             * @tparam T Type à activer si B est true
             * @ingroup ConditionalTraits
             */
            template <nk_bool B, typename T = void>
            using NkEnableIf_t = typename NkEnableIf<B, T>::type;

            /**
             * @brief Sélection conditionnelle de type
             * @struct NkConditional
             * @tparam B Condition booléenne
             * @tparam T Type si B est true
             * @tparam F Type si B est false
             * @ingroup ConditionalTraits
             *
             * Équivalent à std::conditional. Permet de choisir entre deux types.
             *
             * @example
             * @code
             * using ResultType = NkConditional_t<IsFastMode, FastImpl, SlowImpl>;
             * @endcode
             */
            template <nk_bool B, typename T, typename F>
            struct NkConditional {
                using type = T;
            };

            /**
             * @brief Spécialisation pour condition false
             * @ingroup ConditionalTraits
             */
            template <typename T, typename F>
            struct NkConditional<false, T, F> {
                using type = F;
            };

            /**
             * @brief Alias pour NkConditional
             * @typedef NkConditional_t
             * @tparam B Condition booléenne
             * @tparam T Type si B est true
             * @tparam F Type si B est false
             * @ingroup ConditionalTraits
             */
            template <nk_bool B, typename T, typename F>
            using NkConditional_t = typename NkConditional<B, T, F>::type;

            // =================================================================
            // SECTION 5 : TRAITS DE COMPARAISON DE TYPE
            // =================================================================
            // Traits pour comparer l'égalité et la similarité des types.

            /**
             * @brief Vérification d'égalité de type
             * @struct NkIsSame
             * @tparam T Premier type
             * @tparam U Second type
             * @ingroup ComparisonTraits
             *
             * Équivalent à std::is_same. Retourne true si T et U sont le même type.
             *
             * @note Les qualifications const/volatile sont prises en compte.
             *
             * @example
             * @code
             * static_assert(NkIsSame_v<int, int> == true);
             * static_assert(NkIsSame_v<int, const int> == false);
             * @endcode
             */
            template <typename T, typename U>
            struct NkIsSame : NkFalseType {};

            /**
             * @brief Spécialisation pour types identiques
             * @ingroup ComparisonTraits
             */
            template <typename T>
            struct NkIsSame<T, T> : NkTrueType {};

            /**
             * @brief Valeur constexpr pour NkIsSame
             * @def NkIsSame_v
             * @tparam T Premier type
             * @tparam U Second type
             * @ingroup ComparisonTraits
             */
            template <typename T, typename U>
            inline constexpr nk_bool NkIsSame_v = NkIsSame<T, U>::value;

            // =================================================================
            // SECTION 6 : TRAITS DE MODIFICATION DE TYPE
            // =================================================================
            // Traits pour modifier les qualifications et les références des types.

            /**
             * @brief Suppression de la qualification const
             * @struct NkRemoveConst
             * @tparam T Type à modifier
             * @ingroup ModificationTraits
             *
             * Équivalent à std::remove_const. Supprime la qualification const si présente.
             *
             * @example
             * @code
             * static_assert(NkIsSame_v<NkRemoveConst_t<const int>, int>);
             * static_assert(NkIsSame_v<NkRemoveConst_t<int>, int>);
             * @endcode
             */
            template <typename T>
            struct NkRemoveConst {
                using type = T;
            };

            /**
             * @brief Spécialisation pour types const
             * @ingroup ModificationTraits
             */
            template <typename T>
            struct NkRemoveConst<const T> {
                using type = T;
            };

            /**
             * @brief Alias pour NkRemoveConst
             * @typedef NkRemoveConst_t
             * @tparam T Type à modifier
             * @ingroup ModificationTraits
             */
            template <typename T>
            using NkRemoveConst_t = typename NkRemoveConst<T>::type;

            /**
             * @brief Suppression de la qualification volatile
             * @struct NkRemoveVolatile
             * @tparam T Type à modifier
             * @ingroup ModificationTraits
             *
             * Équivalent à std::remove_volatile. Supprime la qualification volatile si présente.
             */
            template <typename T>
            struct NkRemoveVolatile {
                using type = T;
            };

            /**
             * @brief Spécialisation pour types volatile
             * @ingroup ModificationTraits
             */
            template <typename T>
            struct NkRemoveVolatile<volatile T> {
                using type = T;
            };

            /**
             * @brief Alias pour NkRemoveVolatile
             * @typedef NkRemoveVolatile_t
             * @tparam T Type à modifier
             * @ingroup ModificationTraits
             */
            template <typename T>
            using NkRemoveVolatile_t = typename NkRemoveVolatile<T>::type;

            /**
             * @brief Suppression des qualifications const et volatile
             * @struct NkRemoveCV
             * @tparam T Type à modifier
             * @ingroup ModificationTraits
             *
             * Équivalent à std::remove_cv. Supprime const et volatile.
             */
            template <typename T>
            struct NkRemoveCV {
                using type = NkRemoveVolatile_t<NkRemoveConst_t<T>>;
            };

            /**
             * @brief Alias pour NkRemoveCV
             * @typedef NkRemoveCV_t
             * @tparam T Type à modifier
             * @ingroup ModificationTraits
             */
            template <typename T>
            using NkRemoveCV_t = typename NkRemoveCV<T>::type;

            /**
             * @brief Suppression des références
             * @struct NkRemoveReference
             * @tparam T Type à modifier
             * @ingroup ModificationTraits
             *
             * Équivalent à std::remove_reference. Supprime les références & et &&.
             */
            template <typename T>
            struct NkRemoveReference {
                using type = T;
            };

            /**
             * @brief Spécialisation pour référence lvalue
             * @ingroup ModificationTraits
             */
            template <typename T>
            struct NkRemoveReference<T&> {
                using type = T;
            };

            /**
             * @brief Spécialisation pour référence rvalue
             * @ingroup ModificationTraits
             */
            template <typename T>
            struct NkRemoveReference<T&&> {
                using type = T;
            };

            /**
             * @brief Alias pour NkRemoveReference
             * @typedef NkRemoveReference_t
             * @tparam T Type à modifier
             * @ingroup ModificationTraits
             */
            template <typename T>
            using NkRemoveReference_t = typename NkRemoveReference<T>::type;

            /**
             * @brief Ajout de référence lvalue
             * @struct NkAddLValueReference
             * @tparam T Type de base
             * @ingroup ModificationTraits
             *
             * Équivalent à std::add_lvalue_reference. Ajoute une référence & si possible.
             */
            template <typename T>
            struct NkAddLValueReference {
                using type = T&;
            };

            // Spécialisations pour void (ne peuvent pas avoir de référence)
            template <>
            struct NkAddLValueReference<void> {
                using type = void;
            };

            template <>
            struct NkAddLValueReference<const void> {
                using type = const void;
            };

            template <>
            struct NkAddLValueReference<volatile void> {
                using type = volatile void;
            };

            template <>
            struct NkAddLValueReference<const volatile void> {
                using type = const volatile void;
            };

            /**
             * @brief Alias pour NkAddLValueReference
             * @typedef NkAddLValueReference_t
             * @tparam T Type de base
             * @ingroup ModificationTraits
             */
            template <typename T>
            using NkAddLValueReference_t = typename NkAddLValueReference<T>::type;

            /**
             * @brief Ajout de référence rvalue
             * @struct NkAddRValueReference
             * @tparam T Type de base
             * @ingroup ModificationTraits
             *
             * Équivalent à std::add_rvalue_reference. Ajoute une référence && si possible.
             */
            template <typename T>
            struct NkAddRValueReference {
                using type = T&&;
            };

            // Spécialisations pour void
            template <>
            struct NkAddRValueReference<void> {
                using type = void;
            };

            template <>
            struct NkAddRValueReference<const void> {
                using type = const void;
            };

            template <>
            struct NkAddRValueReference<volatile void> {
                using type = volatile void;
            };

            template <>
            struct NkAddRValueReference<const volatile void> {
                using type = const volatile void;
            };

            /**
             * @brief Alias pour NkAddRValueReference
             * @typedef NkAddRValueReference_t
             * @tparam T Type de base
             * @ingroup ModificationTraits
             */
            template <typename T>
            using NkAddRValueReference_t = typename NkAddRValueReference<T>::type;

            /**
             * @brief Suppression du pointeur
             * @struct NkRemovePointer
             * @tparam T Type à modifier
             * @ingroup ModificationTraits
             *
             * Équivalent à std::remove_pointer. Supprime une indirection * si présente.
             */
            template <typename T>
            struct NkRemovePointer {
                using type = T;
            };

            // Spécialisations pour différents types de pointeurs
            template <typename T>
            struct NkRemovePointer<T*> {
                using type = T;
            };

            template <typename T>
            struct NkRemovePointer<T* const> {
                using type = T;
            };

            template <typename T>
            struct NkRemovePointer<T* volatile> {
                using type = T;
            };

            template <typename T>
            struct NkRemovePointer<T* const volatile> {
                using type = T;
            };

            /**
             * @brief Alias pour NkRemovePointer
             * @typedef NkRemovePointer_t
             * @tparam T Type à modifier
             * @ingroup ModificationTraits
             */
            template <typename T>
            using NkRemovePointer_t = typename NkRemovePointer<T>::type;

            /**
             * @brief Suppression de la première dimension de tableau
             * @struct NkRemoveExtent
             * @tparam T Type à modifier
             * @ingroup ModificationTraits
             *
             * Équivalent à std::remove_extent. Transforme T[N] ou T[] en T.
             */
            template <typename T>
            struct NkRemoveExtent {
                using type = T;
            };

            template <typename T>
            struct NkRemoveExtent<T[]> {
                using type = T;
            };

            template <typename T, nk_size N>
            struct NkRemoveExtent<T[N]> {
                using type = T;
            };

            /**
             * @brief Alias pour NkRemoveExtent
             * @typedef NkRemoveExtent_t
             * @tparam T Type à modifier
             * @ingroup ModificationTraits
             */
            template <typename T>
            using NkRemoveExtent_t = typename NkRemoveExtent<T>::type;

            // =================================================================
            // SECTION 7 : TRAITS DE CLASSIFICATION DE TYPE
            // =================================================================
            // Traits pour classifier les types selon leurs propriétés fondamentales.

            /**
             * @brief Vérification de type void
             * @struct NkIsVoid
             * @tparam T Type à vérifier
             * @ingroup ClassificationTraits
             *
             * Équivalent à std::is_void. Retourne true si T est void (après CV removal).
             */
            template <typename T>
            struct NkIsVoid : NkIsSame<void, NkRemoveCV_t<T>> {};

            /**
             * @brief Valeur constexpr pour NkIsVoid
             * @def NkIsVoid_v
             * @tparam T Type à vérifier
             * @ingroup ClassificationTraits
             */
            template <typename T>
            inline constexpr nk_bool NkIsVoid_v = NkIsVoid<T>::value;

            /**
             * @brief Vérification de pointeur nul (std::nullptr_t)
             * @struct NkIsNullPointer
             * @tparam T Type à vérifier
             * @ingroup ClassificationTraits
             *
             * Équivalent à std::is_null_pointer. Retourne true si T est std::nullptr_t.
             */
            template <typename T>
            struct NkIsNullPointer : NkIsSame<NkRemoveCV_t<T>, decltype(nullptr)> {};

            /**
             * @brief Valeur constexpr pour NkIsNullPointer
             * @def NkIsNullPointer_v
             * @tparam T Type à vérifier
             * @ingroup ClassificationTraits
             */
            template <typename T>
            inline constexpr nk_bool NkIsNullPointer_v = NkIsNullPointer<T>::value;

            /**
             * @brief Vérification de qualification const
             * @struct NkIsConst
             * @tparam T Type à vérifier
             * @ingroup ClassificationTraits
             *
             * Équivalent à std::is_const. Retourne true si T est const-qualified.
             */
            template <typename T>
            struct NkIsConst : NkFalseType {};

            template <typename T>
            struct NkIsConst<const T> : NkTrueType {};

            /**
             * @brief Valeur constexpr pour NkIsConst
             * @def NkIsConst_v
             * @tparam T Type à vérifier
             * @ingroup ClassificationTraits
             */
            template <typename T>
            inline constexpr nk_bool NkIsConst_v = NkIsConst<T>::value;

            /**
             * @brief Vérification de qualification volatile
             * @struct NkIsVolatile
             * @tparam T Type à vérifier
             * @ingroup ClassificationTraits
             *
             * Équivalent à std::is_volatile. Retourne true si T est volatile-qualified.
             */
            template <typename T>
            struct NkIsVolatile : NkFalseType {};

            template <typename T>
            struct NkIsVolatile<volatile T> : NkTrueType {};

            /**
             * @brief Valeur constexpr pour NkIsVolatile
             * @def NkIsVolatile_v
             * @tparam T Type à vérifier
             * @ingroup ClassificationTraits
             */
            template <typename T>
            inline constexpr nk_bool NkIsVolatile_v = NkIsVolatile<T>::value;

            /**
             * @brief Vérification de référence lvalue
             * @struct NkIsLValueReference
             * @tparam T Type à vérifier
             * @ingroup ClassificationTraits
             *
             * Équivalent à std::is_lvalue_reference. Retourne true si T est T&.
             */
            template <typename T>
            struct NkIsLValueReference : NkFalseType {};

            template <typename T>
            struct NkIsLValueReference<T&> : NkTrueType {};

            /**
             * @brief Valeur constexpr pour NkIsLValueReference
             * @def NkIsLValueReference_v
             * @tparam T Type à vérifier
             * @ingroup ClassificationTraits
             */
            template <typename T>
            inline constexpr nk_bool NkIsLValueReference_v = NkIsLValueReference<T>::value;

            /**
             * @brief Vérification de référence rvalue
             * @struct NkIsRValueReference
             * @tparam T Type à vérifier
             * @ingroup ClassificationTraits
             *
             * Équivalent à std::is_rvalue_reference. Retourne true si T est T&&.
             */
            template <typename T>
            struct NkIsRValueReference : NkFalseType {};

            template <typename T>
            struct NkIsRValueReference<T&&> : NkTrueType {};

            /**
             * @brief Valeur constexpr pour NkIsRValueReference
             * @def NkIsRValueReference_v
             * @tparam T Type à vérifier
             * @ingroup ClassificationTraits
             */
            template <typename T>
            inline constexpr nk_bool NkIsRValueReference_v = NkIsRValueReference<T>::value;

            /**
             * @brief Vérification de référence (lvalue ou rvalue)
             * @struct NkIsReference
             * @tparam T Type à vérifier
             * @ingroup ClassificationTraits
             *
             * Équivalent à std::is_reference. Retourne true si T est une référence.
             */
            template <typename T>
            struct NkIsReference
                : NkBoolConstant<NkIsLValueReference_v<T> || NkIsRValueReference_v<T>> {};

            /**
             * @brief Valeur constexpr pour NkIsReference
             * @def NkIsReference_v
             * @tparam T Type à vérifier
             * @ingroup ClassificationTraits
             */
            template <typename T>
            inline constexpr nk_bool NkIsReference_v = NkIsReference<T>::value;

            /**
             * @brief Vérification de pointeur
             * @struct NkIsPointer
             * @tparam T Type à vérifier
             * @ingroup ClassificationTraits
             *
             * Équivalent à std::is_pointer. Retourne true si T est un pointeur.
             */
            template <typename T>
            struct NkIsPointer : NkFalseType {};

            // Spécialisations pour différents types de pointeurs
            template <typename T>
            struct NkIsPointer<T*> : NkTrueType {};

            template <typename T>
            struct NkIsPointer<T* const> : NkTrueType {};

            template <typename T>
            struct NkIsPointer<T* volatile> : NkTrueType {};

            template <typename T>
            struct NkIsPointer<T* const volatile> : NkTrueType {};

            /**
             * @brief Valeur constexpr pour NkIsPointer
             * @def NkIsPointer_v
             * @tparam T Type à vérifier
             * @ingroup ClassificationTraits
             */
            template <typename T>
            inline constexpr nk_bool NkIsPointer_v = NkIsPointer<NkRemoveCV_t<T>>::value;

            /**
             * @brief Vérification de tableau
             * @struct NkIsArray
             * @tparam T Type à vérifier
             * @ingroup ClassificationTraits
             *
             * Équivalent à std::is_array. Retourne true si T est un tableau.
             */
            template <typename T>
            struct NkIsArray : NkFalseType {};

            template <typename T>
            struct NkIsArray<T[]> : NkTrueType {};

            template <typename T, nk_size N>
            struct NkIsArray<T[N]> : NkTrueType {};

            /**
             * @brief Valeur constexpr pour NkIsArray
             * @def NkIsArray_v
             * @tparam T Type à vérifier
             * @ingroup ClassificationTraits
             */
            template <typename T>
            inline constexpr nk_bool NkIsArray_v = NkIsArray<T>::value;

            // =================================================================
            // SECTION 8 : TRAITS ARITHMÉTIQUES
            // =================================================================
            // Traits pour les types arithmétiques (intégraux et flottants).

            /**
             * @brief Vérification de type intégral
             * @struct NkIsIntegralBase
             * @tparam T Type à vérifier
             * @ingroup ArithmeticTraits
             *
             * Implémentation de base pour les types intégraux standards.
             */
            template <typename T>
            struct NkIsIntegralBase : NkFalseType {};

            // Spécialisations pour tous les types intégraux standards
            template <> struct NkIsIntegralBase<bool> : NkTrueType {};
            template <> struct NkIsIntegralBase<char> : NkTrueType {};
            template <> struct NkIsIntegralBase<signed char> : NkTrueType {};
            template <> struct NkIsIntegralBase<unsigned char> : NkTrueType {};
            template <> struct NkIsIntegralBase<wchar_t> : NkTrueType {};
            template <> struct NkIsIntegralBase<char16_t> : NkTrueType {};
            template <> struct NkIsIntegralBase<char32_t> : NkTrueType {};
            template <> struct NkIsIntegralBase<short> : NkTrueType {};
            template <> struct NkIsIntegralBase<unsigned short> : NkTrueType {};
            template <> struct NkIsIntegralBase<int> : NkTrueType {};
            template <> struct NkIsIntegralBase<unsigned int> : NkTrueType {};
            template <> struct NkIsIntegralBase<long> : NkTrueType {};
            template <> struct NkIsIntegralBase<unsigned long> : NkTrueType {};
            template <> struct NkIsIntegralBase<long long> : NkTrueType {};
            template <> struct NkIsIntegralBase<unsigned long long> : NkTrueType {};

            #if defined(__cpp_char8_t)
            template <> struct NkIsIntegralBase<char8_t> : NkTrueType {};
            #endif

            /**
             * @brief Vérification de type intégral (avec CV removal)
             * @struct NkIsIntegral
             * @tparam T Type à vérifier
             * @ingroup ArithmeticTraits
             *
             * Équivalent à std::is_integral. Prend en compte les qualifications CV.
             */
            template <typename T>
            struct NkIsIntegral : NkIsIntegralBase<NkRemoveCV_t<T>> {};

            /**
             * @brief Valeur constexpr pour NkIsIntegral
             * @def NkIsIntegral_v
             * @tparam T Type à vérifier
             * @ingroup ArithmeticTraits
             */
            template <typename T>
            inline constexpr nk_bool NkIsIntegral_v = NkIsIntegral<T>::value;

            /**
             * @brief Vérification de type flottant
             * @struct NkIsFloatingPointBase
             * @tparam T Type à vérifier
             * @ingroup ArithmeticTraits
             *
             * Implémentation de base pour les types flottants standards.
             */
            template <typename T>
            struct NkIsFloatingPointBase : NkFalseType {};

            template <> struct NkIsFloatingPointBase<float> : NkTrueType {};
            template <> struct NkIsFloatingPointBase<double> : NkTrueType {};
            template <> struct NkIsFloatingPointBase<long double> : NkTrueType {};

            /**
             * @brief Vérification de type flottant (avec CV removal)
             * @struct NkIsFloatingPoint
             * @tparam T Type à vérifier
             * @ingroup ArithmeticTraits
             *
             * Équivalent à std::is_floating_point. Prend en compte les qualifications CV.
             */
            template <typename T>
            struct NkIsFloatingPoint : NkIsFloatingPointBase<NkRemoveCV_t<T>> {};

            /**
             * @brief Valeur constexpr pour NkIsFloatingPoint
             * @def NkIsFloatingPoint_v
             * @tparam T Type à vérifier
             * @ingroup ArithmeticTraits
             */
            template <typename T>
            inline constexpr nk_bool NkIsFloatingPoint_v = NkIsFloatingPoint<T>::value;

            /**
             * @brief Vérification de type arithmétique
             * @struct NkIsArithmetic
             * @tparam T Type à vérifier
             * @ingroup ArithmeticTraits
             *
             * Équivalent à std::is_arithmetic. Combine intégral et flottant.
             */
            template <typename T>
            struct NkIsArithmetic
                : NkBoolConstant<NkIsIntegral_v<T> || NkIsFloatingPoint_v<T>> {};

            /**
             * @brief Valeur constexpr pour NkIsArithmetic
             * @def NkIsArithmetic_v
             * @tparam T Type à vérifier
             * @ingroup ArithmeticTraits
             */
            template <typename T>
            inline constexpr nk_bool NkIsArithmetic_v = NkIsArithmetic<T>::value;

            /**
             * @brief Vérification de type signé
             * @struct NkIsSignedImpl
             * @tparam T Type à vérifier
             * @tparam isArithmetic Indicateur si T est arithmétique
             * @ingroup ArithmeticTraits
             *
             * Implémentation conditionnelle basée sur la comparaison -1 < 0.
             */
            template <typename T, nk_bool = NkIsArithmetic_v<T>>
            struct NkIsSignedImpl : NkFalseType {};

            template <typename T>
            struct NkIsSignedImpl<T, true> : NkBoolConstant<(T(-1) < T(0))> {};

            /**
             * @brief Vérification de type signé (avec CV removal)
             * @struct NkIsSigned
             * @tparam T Type à vérifier
             * @ingroup ArithmeticTraits
             *
             * Équivalent à std::is_signed. Fonctionne pour tous les types arithmétiques.
             */
            template <typename T>
            struct NkIsSigned : NkIsSignedImpl<NkRemoveCV_t<T>> {};

            /**
             * @brief Valeur constexpr pour NkIsSigned
             * @def NkIsSigned_v
             * @tparam T Type à vérifier
             * @ingroup ArithmeticTraits
             */
            template <typename T>
            inline constexpr nk_bool NkIsSigned_v = NkIsSigned<T>::value;

            /**
             * @brief Vérification de type non-signé
             * @struct NkIsUnsigned
             * @tparam T Type à vérifier
             * @ingroup ArithmeticTraits
             *
             * Équivalent à std::is_unsigned. Complément de NkIsSigned.
             */
            template <typename T>
            struct NkIsUnsigned
                : NkBoolConstant<NkIsArithmetic_v<T> && !NkIsSigned_v<T>> {};

            /**
             * @brief Valeur constexpr pour NkIsUnsigned
             * @def NkIsUnsigned_v
             * @tparam T Type à vérifier
             * @ingroup ArithmeticTraits
             */
            template <typename T>
            inline constexpr nk_bool NkIsUnsigned_v = NkIsUnsigned<T>::value;

            // =================================================================
            // SECTION 9 : TRAITS DE COMPOSITION DE TYPE
            // =================================================================
            // Traits pour les types composés (classes, unions, enums, fonctions).

            // Détection via intrinsèques du compilateur si disponibles
            #if defined(__clang__) || defined(__GNUC__) || defined(_MSC_VER)
                /**
                 * @brief Vérification de type enum
                 * @struct NkIsEnum
                 * @tparam T Type à vérifier
                 * @ingroup CompositionTraits
                 *
                 * Utilise __is_enum si disponible, sinon fallback.
                 */
                template <typename T>
                struct NkIsEnum : NkBoolConstant<__is_enum(T)> {};

                /**
                 * @brief Vérification de type classe
                 * @struct NkIsClass
                 * @tparam T Type à vérifier
                 * @ingroup CompositionTraits
                 *
                 * Utilise __is_class si disponible, sinon fallback.
                 */
                template <typename T>
                struct NkIsClass : NkBoolConstant<__is_class(T)> {};

                /**
                 * @brief Vérification de type union
                 * @struct NkIsUnion
                 * @tparam T Type à vérifier
                 * @ingroup CompositionTraits
                 *
                 * Utilise __is_union si disponible, sinon fallback.
                 */
                template <typename T>
                struct NkIsUnion : NkBoolConstant<__is_union(T)> {};

                /**
                 * @brief Vérification de type fonction
                 * @struct NkIsFunction
                 * @tparam T Type à vérifier
                 * @ingroup CompositionTraits
                 *
                 * Utilise std::is_function_v comme fallback fiable.
                 */
                template <typename T> struct NkIsFunction : NkFalseType {};
                template <typename Ret, typename... Args> struct NkIsFunction<Ret(Args...)> : NkTrueType {};
                template <typename Ret, typename... Args> struct NkIsFunction<Ret(Args..., ...)> : NkTrueType {};
            #else
                // Fallback minimal pour les compilateurs sans intrinsèques
                template <typename T> struct NkIsEnum : NkFalseType {};
                template <typename T> struct NkIsClass : NkFalseType {};
                template <typename T> struct NkIsUnion : NkFalseType {};
                template <typename T> struct NkIsFunction : NkFalseType {};
            #endif

            /**
             * @brief Valeur constexpr pour NkIsEnum
             * @def NkIsEnum_v
             * @tparam T Type à vérifier
             * @ingroup CompositionTraits
             */
            template <typename T>
            inline constexpr nk_bool NkIsEnum_v = NkIsEnum<T>::value;

            /**
             * @brief Valeur constexpr pour NkIsClass
             * @def NkIsClass_v
             * @tparam T Type à vérifier
             * @ingroup CompositionTraits
             */
            template <typename T>
            inline constexpr nk_bool NkIsClass_v = NkIsClass<T>::value;

            /**
             * @brief Valeur constexpr pour NkIsUnion
             * @def NkIsUnion_v
             * @tparam T Type à vérifier
             * @ingroup CompositionTraits
             */
            template <typename T>
            inline constexpr nk_bool NkIsUnion_v = NkIsUnion<T>::value;

            /**
             * @brief Valeur constexpr pour NkIsFunction
             * @def NkIsFunction_v
             * @tparam T Type à vérifier
             * @ingroup CompositionTraits
             */
            template <typename T>
            inline constexpr nk_bool NkIsFunction_v = NkIsFunction<T>::value;

            // =================================================================
            // SECTION 10 : TRAITS DE TRANSFORMATION AVANCÉE
            // =================================================================
            // Traits pour les transformations complexes de type.

            /**
             * @brief Ajout de pointeur
             * @struct NkAddPointer
             * @tparam T Type de base
             * @ingroup TransformationTraits
             *
             * Équivalent à std::add_pointer. Transforme T en T* (gère les références).
             */
            template <typename T>
            struct NkAddPointer {
                using type = NkRemoveReference_t<T>*;
            };

            /**
             * @brief Alias pour NkAddPointer
             * @typedef NkAddPointer_t
             * @tparam T Type de base
             * @ingroup TransformationTraits
             */
            template <typename T>
            using NkAddPointer_t = typename NkAddPointer<T>::type;

            // =================================================================
            // SECTION 11 : TRAITS LOGIQUES
            // =================================================================
            // Traits pour combiner logiquement d'autres traits.

            /**
             * @brief Conjonction logique (ET)
             * @struct NkConjunction
             * @tparam ... BoolTraits Traits booléens à combiner
             * @ingroup LogicalTraits
             *
             * Équivalent à std::conjunction. Retourne true si tous les traits sont true.
             */
            template <typename...>
            struct NkConjunction : NkTrueType {};

            template <typename B1>
            struct NkConjunction<B1> : B1 {};

            template <typename B1, typename... Bn>
            struct NkConjunction<B1, Bn...>
                : NkConditional_t<static_cast<nk_bool>(B1::value), NkConjunction<Bn...>, B1> {};

            /**
             * @brief Disjonction logique (OU)
             * @struct NkDisjunction
             * @tparam ... BoolTraits Traits booléens à combiner
             * @ingroup LogicalTraits
             *
             * Équivalent à std::disjunction. Retourne true si au moins un trait est true.
             */
            template <typename...>
            struct NkDisjunction : NkFalseType {};

            template <typename B1>
            struct NkDisjunction<B1> : B1 {};

            template <typename B1, typename... Bn>
            struct NkDisjunction<B1, Bn...>
                : NkConditional_t<static_cast<nk_bool>(B1::value), B1, NkDisjunction<Bn...>> {};

            /**
             * @brief Négation logique
             * @struct NkNegation
             * @tparam B Trait booléen à inverser
             * @ingroup LogicalTraits
             *
             * Équivalent à std::negation. Retourne l'opposé du trait donné.
             */
            template <typename B>
            struct NkNegation : NkBoolConstant<!static_cast<nk_bool>(B::value)> {};

            /**
             * @brief Vérification si un type correspond à l'un des types donnés
             * @struct NkIsAnyOf
             * @tparam T Type à vérifier
             * @tparam ... Types Liste de types acceptables
             * @ingroup LogicalTraits
             *
             * Retourne true si T est l'un des types dans la liste Types...
             */
            template <typename T, typename... Types>
            struct NkIsAnyOf : NkDisjunction<NkIsSame<T, Types>...> {};

            /**
             * @brief Valeur constexpr pour NkIsAnyOf
             * @def NkIsAnyOf_v
             * @tparam T Type à vérifier
             * @tparam ... Types Liste de types acceptables
             * @ingroup LogicalTraits
             */
            template <typename T, typename... Types>
            inline constexpr nk_bool NkIsAnyOf_v = NkIsAnyOf<T, Types...>::value;

            // =================================================================
            // SECTION 12 : TRAITS DE DÉGRADATION (DECAY)
            // =================================================================
            // Traits pour simuler la dégradation de type dans les appels de fonction.

            /**
             * @brief Dégénérescence de type (decay)
             * @struct NkDecay
             * @tparam T Type à dégrader
             * @ingroup DecayTraits
             *
             * Équivalent à std::decay. Simule la transformation de type lors du passage par valeur.
             */
            template <typename T>
            struct NkDecay {
            private:
                using U = NkRemoveReference_t<T>;
            public:
                using type = NkConditional_t<
                    NkIsArray_v<U>,
                    NkAddPointer_t<NkRemoveExtent_t<U>>,
                    NkConditional_t<
                        NkIsFunction_v<U>,
                        NkAddPointer_t<U>,
                        NkRemoveCV_t<U>>>;
            };

            /**
             * @brief Alias pour NkDecay
             * @typedef NkDecay_t
             * @tparam T Type à dégrader
             * @ingroup DecayTraits
             */
            template <typename T>
            using NkDecay_t = typename NkDecay<T>::type;

            /**
             * @brief Alias legacy pour compatibilité
             * @typedef NkDecayT
             * @tparam T Type à dégrader
             * @ingroup DecayTraits
             */
            template <typename T>
            using NkDecayT = typename NkDecay<T>::type;

            // =================================================================
            // SECTION 13 : UTILITAIRES DE DÉDUCTION ET MOUVEMENT
            // =================================================================
            // Fonctions et macros pour la déduction de type et le mouvement.

            /**
             * @brief Déclaration fictive pour déduction de type
             * @fn NkDeclVal
             * @tparam T Type à déduire
             * @return Référence rvalue vers T (non définie, juste pour decltype)
             * @ingroup UtilityTraits
             *
             * Équivalent à std::declval. Utilisé dans decltype pour déduire des types.
             */
            template <typename T>
            NkAddRValueReference_t<T> NkDeclval() noexcept;

            template <typename T>
            using NkDeclVal = decltype(NkDeclval<T>());  // ou bien garder l'ancienne fonction ?

            /**
             * @brief Macro pour decltype
             * @def NK_DECL_TYPE
             * @param ... Expression à analyser
             * @ingroup UtilityTraits
             *
             * Wrapper portable autour de decltype pour la compatibilité.
             */
            #define NK_DECL_TYPE(...) decltype(__VA_ARGS__)

            /** @brief Alias de compatibilité (déprécié) — préférer NK_DECL_TYPE */
            #define NkDeclType(...) NK_DECL_TYPE(__VA_ARGS__)

            template <typename T, typename = void>
            struct NkHasSize : NkFalseType {};

            template <typename T>
            struct NkHasSize<T, NkVoidT<
                NK_DECL_TYPE(NkDeclval<T>().Size())
            >> : NkTrueType {};

            // ============================================================
            // @defgroup SfinaeHelperMacros Macros SFINAE (helpers template)
            // @{
            // ============================================================

            /**
             * @brief Paramètre de template SFINAE via NkEnableIf_t
             * @def NK_ENABLE_IF_T
             * @param cond Condition booléenne constexpr
             * @ingroup SfinaeHelperMacros
             *
             * Usage : `template<typename C = Category, NK_ENABLE_IF_T(condition)*>`
             *
             * @example
             * @code
             * template<typename C = Category,
             *          NK_ENABLE_IF_T(NK_IS_BASE_OF_V(NkBidirectionalIteratorTag, C))*>
             * void Decrement() { --mPtr; }
             * @endcode
             */
            #define NK_ENABLE_IF_T(cond) ::nkentseu::traits::NkEnableIf_t<(cond)>

            /**
             * @brief Test d'héritage compile-time (valeur constexpr bool)
             * @def NK_IS_BASE_OF_V
             * @param Base  Classe de base
             * @param Derived Classe dérivée
             * @ingroup SfinaeHelperMacros
             *
             * Équivalent à ::nkentseu::traits::NkIsBaseOf_v<Base, Derived>.
             */
            #define NK_IS_BASE_OF_V(Base, Derived) ::nkentseu::traits::NkIsBaseOf_v<Base, Derived>

            /**
             * @brief Test const compile-time (valeur constexpr bool)
             * @def NK_IS_CONST_V
             * @param T Type à tester
             * @ingroup SfinaeHelperMacros
             *
             * Équivalent à ::nkentseu::traits::NkIsConst_v<T>.
             */
            #define NK_IS_CONST_V(T) ::nkentseu::traits::NkIsConst_v<T>

            /** @} */

            /**
             * @brief Transfert parfait (perfect forwarding)
             * @fn NkForward
             * @tparam T Type déduit
             * @param value Valeur à transférer
             * @return Référence appropriée (lvalue/rvalue)
             * @ingroup UtilityTraits
             *
             * Équivalent à std::forward. Préserve la catégorie de valeur.
             */
            template <typename T>
            constexpr T&& NkForward(NkRemoveReference_t<T>& value) noexcept {
                return static_cast<T&&>(value);
            }

            template <typename T>
            constexpr T&& NkForward(NkRemoveReference_t<T>&& value) noexcept {
                static_assert(!NkIsLValueReference_v<T>, "NkForward<T>(rvalue) with lvalue T");
                return static_cast<T&&>(value);
            }

            /**
             * @brief Mouvement de valeur
             * @fn NkMove
             * @tparam T Type déduit
             * @param value Valeur à déplacer
             * @return Référence rvalue vers la valeur
             * @ingroup UtilityTraits
             *
             * Équivalent à std::move. Convertit en rvalue reference.
             */
            template <typename T>
            constexpr NkRemoveReference_t<T>&& NkMove(T&& value) noexcept {
                return static_cast<NkRemoveReference_t<T>&&>(value);
            }

            // Suppression du conflit avec la macro legacy NkSwap
            #ifdef NkSwap
                #undef NkSwap
            #endif

            /**
             * @brief Échange de valeurs
             * @fn NkSwap
             * @tparam T Type des valeurs à échanger
             * @param a Première valeur
             * @param b Seconde valeur
             * @ingroup UtilityTraits
             *
             * Équivalent à std::swap. Échange les valeurs via move semantics.
             */
            template <typename T>
            constexpr void NkSwap(T& a, T& b) noexcept(noexcept(T(NkMove(a))) &&
                                                    noexcept(a = NkMove(b)) &&
                                                    noexcept(b = NkMove(a))) {
                T tmp = NkMove(a);
                a = NkMove(b);
                b = NkMove(tmp);
            }

            // =================================================================
            // SECTION 14 : TRAITS SPÉCIFIQUES À NKCORE
            // =================================================================
            // Traits personnalisés pour les types et besoins du framework NKCore.

            /**
             * @brief Vérification de type caractère NKCore
             * @struct NkIsCharacterType
             * @tparam T Type à vérifier
             * @ingroup NKCoreTraits
             *
             * Retourne true si T est un type caractère supporté par NKCore :
             * nk_char, nk_wchar, nk_char8, nk_char16, nk_char32, signed/unsigned char.
             */
            template <typename T>
            struct NkIsCharacterType
                : NkIsAnyOf<NkRemoveCV_t<T>, nk_char, nk_wchar, nk_char8, nk_char16, nk_char32,
                            signed char, unsigned char> {};

            /**
             * @brief Valeur constexpr pour NkIsCharacterType
             * @def NkIsCharacterType_v
             * @tparam T Type à vérifier
             * @ingroup NKCoreTraits
             */
            template <typename T>
            inline constexpr nk_bool NkIsCharacterType_v = NkIsCharacterType<T>::value;

            /**
             * @brief Vérification de type caractère valide pour NKCore
             * @struct NkIsValidCharType
             * @tparam T Type à vérifier
             * @ingroup NKCoreTraits
             *
             * Alias vers NkIsCharacterType pour la compatibilité avec l'API existante.
             */
            template <typename T>
            struct NkIsValidCharType : NkIsCharacterType<T> {};

            /**
             * @brief Valeur constexpr pour NkIsValidCharType
             * @def NkIsValidCharType_v
             * @tparam T Type à vérifier
             * @ingroup NKCoreTraits
             */
            template <typename T>
            inline constexpr nk_bool NkIsValidCharType_v = NkIsValidCharType<T>::value;

            /**
             * @brief Vérification de type fondamental
             * @struct NkIsFundamental
             * @tparam T Type à vérifier
             * @ingroup NKCoreTraits
             *
             * Équivalent à std::is_fundamental. Combine arithmétique, void, et nullptr_t.
             */
            template <typename T>
            struct NkIsFundamental
                : NkBoolConstant<NkIsArithmetic_v<T> || NkIsVoid_v<T> || NkIsNullPointer_v<T>> {};

            /**
             * @brief Valeur constexpr pour NkIsFundamental
             * @def NkIsFundamental_v
             * @tparam T Type à vérifier
             * @ingroup NKCoreTraits
             */
            template <typename T>
            inline constexpr nk_bool NkIsFundamental_v = NkIsFundamental<T>::value;

            /**
             * @brief Vérification de type scalaire
             * @struct NkIsScalar
             * @tparam T Type à vérifier
             * @ingroup NKCoreTraits
             *
             * Équivalent à std::is_scalar. Combine arithmétique, enum, pointeur, et nullptr_t.
             */
            template <typename T>
            struct NkIsScalar
                : NkBoolConstant<NkIsArithmetic_v<T> || NkIsEnum_v<T> || NkIsPointer_v<T> ||
                                NkIsNullPointer_v<T>> {};

            /**
             * @brief Valeur constexpr pour NkIsScalar
             * @def NkIsScalar_v
             * @tparam T Type à vérifier
             * @ingroup NKCoreTraits
             */
            template <typename T>
            inline constexpr nk_bool NkIsScalar_v = NkIsScalar<T>::value;

            /**
             * @brief Vérification de type objet
             * @struct NkIsObject
             * @tparam T Type à vérifier
             * @ingroup NKCoreTraits
             *
             * Équivalent à std::is_object. Exclut les références, void, et fonctions.
             */
            template <typename T>
            struct NkIsObject
                : NkBoolConstant<!NkIsReference_v<T> && !NkIsVoid_v<T> && !NkIsFunction_v<T>> {};

            /**
             * @brief Valeur constexpr pour NkIsObject
             * @def NkIsObject_v
             * @tparam T Type à vérifier
             * @ingroup NKCoreTraits
             */
            template <typename T>
            inline constexpr nk_bool NkIsObject_v = NkIsObject<T>::value;

            /**
             * @brief Vérification d'héritage de base
             * @struct NkIsBaseOf
             * @tparam Base Classe de base potentielle
             * @tparam Derived Classe dérivée potentielle
             * @ingroup NKCoreTraits
             *
             * Équivalent à std::is_base_of. Utilise __is_base_of si disponible.
             */
            #if defined(__clang__) || defined(__GNUC__) || defined(_MSC_VER)
                template <typename Base, typename Derived>
                struct NkIsBaseOf : NkBoolConstant<__is_base_of(Base, Derived)> {};
            #else
                template <typename Base, typename Derived>
                struct NkIsBaseOf : NkFalseType {};
            #endif

            /**
             * @brief Valeur constexpr pour NkIsBaseOf
             * @def NkIsBaseOf_v
             * @tparam Base Classe de base potentielle
             * @tparam Derived Classe dérivée potentielle
             * @ingroup NKCoreTraits
             */
            template <typename Base, typename Derived>
            inline constexpr nk_bool NkIsBaseOf_v = NkIsBaseOf<Base, Derived>::value;

            /**
             * @brief Vérification de type composé
             * @struct NkIsCompound
             * @tparam T Type à vérifier
             * @ingroup NKCoreTraits
             *
             * Équivalent à std::is_compound. Opposé de is_fundamental.
             */
            template <typename T>
            struct NkIsCompound : NkBoolConstant<!NkIsFundamental_v<T>> {};

            /**
             * @brief Valeur constexpr pour NkIsCompound
             * @def NkIsCompound_v
             * @tparam T Type à vérifier
             * @ingroup NKCoreTraits
             */
            template <typename T>
            inline constexpr nk_bool NkIsCompound_v = NkIsCompound<T>::value;

            /**
             * @brief Extraction du type nominal (sans CV ni référence)
             * @struct NkNounType
             * @tparam T Type à nettoyer
             * @ingroup NKCoreTraits
             *
             * Retourne le type de base sans qualifications ni références.
             */
            template <typename T>
            struct NkNounType {
                using type = NkRemoveCV_t<NkRemoveReference_t<T>>;
            };

            /**
             * @brief Alias pour NkNounType
             * @typedef NkNounType_t
             * @tparam T Type à nettoyer
             * @ingroup NKCoreTraits
             */
            template <typename T>
            using NkNounType_t = typename NkNounType<T>::type;

            /**
             * @brief Vérification du support de plateforme pour un type
             * @struct NkIsPlatformSupported
             * @tparam T Type à vérifier
             * @ingroup NKCoreTraits
             *
             * Retourne false pour les types 128-bit non supportés sur la plateforme courante.
             */
            template <typename T>
            struct NkIsPlatformSupported : NkTrueType {};

            #if !NKENTSEU_INT128_AVAILABLE
                template <>
                struct NkIsPlatformSupported<nk_int128> : NkFalseType {};
                template <>
                struct NkIsPlatformSupported<nk_uint128> : NkFalseType {};
            #endif

            /**
             * @brief Valeur constexpr pour NkIsPlatformSupported
             * @def NkIsPlatformSupported_v
             * @tparam T Type à vérifier
             * @ingroup NKCoreTraits
             */
            template <typename T>
            inline constexpr nk_bool NkIsPlatformSupported_v = NkIsPlatformSupported<T>::value;

            // =================================================================
            // SECTION 15 : TRAITS DE CONSTRUCTIBILITÉ ET DESTRUCTIBILITÉ
            // =================================================================
            // Traits pour les opérations de construction et destruction.

            /**
             * @brief Vérification de copiabilité triviale
             * @struct NkIsTriviallyCopyable
             * @tparam T Type à vérifier
             * @ingroup ConstructibilityTraits
             *
             * Retourne true si T peut être copié via memcpy (arithmétique ou pointeur).
             */
            template<typename T>
            struct NkIsTriviallyCopyable : NkBoolConstant<
                NkIsArithmetic_v<T> || NkIsPointer_v<T>
            > {};

            /**
             * @brief Valeur constexpr pour NkIsTriviallyCopyable
             * @def NkIsTriviallyCopyable_v
             * @tparam T Type à vérifier
             * @ingroup ConstructibilityTraits
             */
            template<typename T>
            inline constexpr bool NkIsTriviallyCopyable_v = NkIsTriviallyCopyable<T>::value;

            /**
             * @brief Vérification de constructibilité par copie
             * @struct NkIsCopyConstructible
             * @tparam T Type à vérifier
             * @ingroup ConstructibilityTraits
             *
             * Retourne true si T peut être construit par copie (arithmétique ou pointeur).
             */
            template <typename T>
            struct NkIsCopyConstructible : NkBoolConstant<__is_constructible(T, const T&)> {};
            template <typename T, typename = void>
            struct NkIsCopyConstructibleImpl : NkFalseType {};
            template <typename T>
            struct NkIsCopyConstructibleImpl<T, NkVoidT<decltype(T(std::declval<const T&>()))>> : NkTrueType {};

            /**
             * @brief Valeur constexpr pour NkIsCopyConstructible
             * @def NkIsCopyConstructible_v
             * @tparam T Type à vérifier
             * @ingroup ConstructibilityTraits
             */
            template<typename T>
            inline constexpr bool NkIsCopyConstructible_v = NkIsCopyConstructible<T>::value;

            /**
             * @brief Vérification de constructibilité par déplacement
             * @struct NkIsMoveConstructible
             * @tparam T Type à vérifier
             * @ingroup ConstructibilityTraits
             *
             * Alias vers NkIsCopyConstructible (les types copiables sont déplaçables).
             */
            template<typename T>
            struct NkIsMoveConstructible : NkIsCopyConstructible<T> {};

            /**
             * @brief Valeur constexpr pour NkIsMoveConstructible
             * @def NkIsMoveConstructible_v
             * @tparam T Type à vérifier
             * @ingroup ConstructibilityTraits
             */
            template<typename T>
            inline constexpr bool NkIsMoveConstructible_v = NkIsMoveConstructible<T>::value;

            /**
             * @brief Vérification de constructibilité triviale par déplacement
             * @struct NkIsTriviallyMoveConstructible
             * @tparam T Type à vérifier
             * @ingroup ConstructibilityTraits
             *
             * Alias vers NkIsMoveConstructible (constructibilité triviale assumée).
             */
            template<typename T>
            struct NkIsTriviallyMoveConstructible : NkIsMoveConstructible<T> {};

            /**
             * @brief Valeur constexpr pour NkIsTriviallyMoveConstructible
             * @def NkIsTriviallyMoveConstructible_v
             * @tparam T Type à vérifier
             * @ingroup ConstructibilityTraits
             */
            template<typename T>
            inline constexpr bool NkIsTriviallyMoveConstructible_v = NkIsTriviallyMoveConstructible<T>::value;

            /**
             * @brief Vérification de destructibilité triviale
             * @def NkIsTriviallyDestructible
             * @tparam T Type à vérifier
             * @ingroup ConstructibilityTraits
             *
             * Utilise __is_trivially_destructible si disponible, sinon assume true.
             */
            #if defined(__clang__) || defined(__GNUC__) || defined(_MSC_VER)
                template <typename T>
                inline constexpr bool NkIsTriviallyDestructible = __is_trivially_destructible(T);
            #else
                template <typename T>
                inline constexpr bool NkIsTriviallyDestructible = true;
            #endif

            /**
             * @brief Valeur constexpr pour NkIsTriviallyConstructible
             * @def NkIsTriviallyConstructible_v
             * @tparam T Type à vérifier
             * @ingroup ConstructibilityTraits
             *
             * Utilise __is_trivially_constructible si disponible.
             */
            #if defined(__clang__) || defined(__GNUC__) || defined(_MSC_VER)
                template <typename T>
                inline constexpr bool NkIsTriviallyConstructible_v = __is_trivially_constructible(T);
            #else
                template <typename T>
                inline constexpr bool NkIsTriviallyConstructible_v = true;
            #endif

            // =================================================================
            // SECTION 16 : TRAITS MANQUANTS AJOUTÉS (EXTENSIONS)
            // =================================================================
            // Ajout des traits couramment utilisés mais absents du fichier original.

            /**
             * @brief Vérification de constructibilité par défaut
             * @struct NkIsDefaultConstructible
             * @tparam T Type à vérifier
             * @ingroup ExtendedTraits
             *
             * Retourne true si T peut être construit sans arguments.
             */
            #if defined(__clang__) || defined(__GNUC__) || defined(_MSC_VER)
                template <typename T>
                struct NkIsDefaultConstructible : NkBoolConstant<__is_constructible(T)> {};
            #else
                // Fallback conservateur
                template <typename T>
                struct NkIsDefaultConstructible : NkIsTriviallyConstructible<T> {};
            #endif

            /**
             * @brief Valeur constexpr pour NkIsDefaultConstructible
             * @def NkIsDefaultConstructible_v
             * @tparam T Type à vérifier
             * @ingroup ExtendedTraits
             */
            template <typename T>
            inline constexpr bool NkIsDefaultConstructible_v = NkIsDefaultConstructible<T>::value;

            /**
             * @brief Vérification de possibilité d'affectation
             * @struct NkIsAssignable
             * @tparam T Type cible
             * @tparam U Type source
             * @ingroup ExtendedTraits
             *
             * Retourne true si une instance de T peut être assignée depuis U.
             */
            #if defined(__clang__) || defined(__GNUC__) || defined(_MSC_VER)
                template <typename T, typename U>
                struct NkIsAssignable : NkBoolConstant<__is_assignable(T, U)> {};
            #else
                // Fallback pour types arithmétiques/pointeurs
                template <typename T, typename U>
                struct NkIsAssignable : NkBoolConstant<
                    (NkIsArithmetic_v<T> && NkIsArithmetic_v<U>) ||
                    (NkIsPointer_v<T> && NkIsPointer_v<U>)
                > {};
            #endif

            /**
             * @brief Valeur constexpr pour NkIsAssignable
             * @def NkIsAssignable_v
             * @tparam T Type cible
             * @tparam U Type source
             * @ingroup ExtendedTraits
             */
            template <typename T, typename U>
            inline constexpr bool NkIsAssignable_v = NkIsAssignable<T, U>::value;

            /**
             * @brief Vérification de trivialité
             * @struct NkIsTrivial
             * @tparam T Type à vérifier
             * @ingroup ExtendedTraits
             *
             * Équivalent à std::is_trivial. Combine constructibilité, copiabilité, et destructibilité triviales.
             */
            template <typename T>
            struct NkIsTrivial : NkBoolConstant<
                NkIsTriviallyConstructible_v<T> &&
                NkIsTriviallyCopyable_v<T> &&
                NkIsTriviallyDestructible<T>
            > {};

            /**
             * @brief Valeur constexpr pour NkIsTrivial
             * @def NkIsTrivial_v
             * @tparam T Type à vérifier
             * @ingroup ExtendedTraits
             */
            template <typename T>
            inline constexpr bool NkIsTrivial_v = NkIsTrivial<T>::value;

            /**
             * @brief Vérification de standard-layout
             * @struct NkIsStandardLayout
             * @tparam T Type à vérifier
             * @ingroup ExtendedTraits
             *
             * Utilise __is_standard_layout si disponible, sinon assume true pour les types simples.
             */
            #if defined(__clang__) || defined(__GNUC__) || defined(_MSC_VER)
                template <typename T>
                struct NkIsStandardLayout : NkBoolConstant<__is_standard_layout(T)> {};
            #else
                template <typename T>
                struct NkIsStandardLayout : NkBoolConstant<
                    NkIsArithmetic_v<T> || NkIsPointer_v<T> || NkIsEnum_v<T>
                > {};
            #endif

            /**
             * @brief Valeur constexpr pour NkIsStandardLayout
             * @def NkIsStandardLayout_v
             * @tparam T Type à vérifier
             * @ingroup ExtendedTraits
             */
            template <typename T>
            inline constexpr bool NkIsStandardLayout_v = NkIsStandardLayout<T>::value;

            /**
             * @brief Vérification de POD (Plain Old Data)
             * @struct NkIsPOD
             * @tparam T Type à vérifier
             * @ingroup ExtendedTraits
             *
             * Équivalent à std::is_pod. Combine trivialité et standard-layout.
             */
            template <typename T>
            struct NkIsPOD : NkBoolConstant<
                NkIsTrivial_v<T> && NkIsStandardLayout_v<T>
            > {};

            /**
             * @brief Valeur constexpr pour NkIsPOD
             * @def NkIsPOD_v
             * @tparam T Type à vérifier
             * @ingroup ExtendedTraits
             */
            template <typename T>
            inline constexpr bool NkIsPOD_v = NkIsPOD<T>::value;

            /**
             * @brief Taille en bits d'un type
             * @struct NkBitSize
             * @tparam T Type à mesurer
             * @ingroup ExtendedTraits
             *
             * Retourne la taille en bits (sizeof * 8) pour les types arithmétiques.
             */
            template <typename T>
            struct NkBitSize : NkIntegralConstant<size_t, sizeof(T) * 8> {};

            /**
             * @brief Valeur constexpr pour NkBitSize
             * @def NkBitSize_v
             * @tparam T Type à mesurer
             * @ingroup ExtendedTraits
             */
            template <typename T>
            inline constexpr size_t NkBitSize_v = NkBitSize<T>::value;

            /**
             * @brief Vérification de type vide (empty class)
             * @struct NkIsEmpty
             * @tparam T Type à vérifier
             * @ingroup ExtendedTraits
             *
             * Utilise __is_empty si disponible, sinon vérifie sizeof == 1.
             */
            #if defined(__clang__) || defined(__GNUC__) || defined(_MSC_VER)
                template <typename T>
                struct NkIsEmpty : NkBoolConstant<__is_empty(T)> {};
            #else
                template <typename T>
                struct NkIsEmpty : NkBoolConstant<sizeof(T) == 1> {};
            #endif

            /**
             * @brief Valeur constexpr pour NkIsEmpty
             * @def NkIsEmpty_v
             * @tparam T Type à vérifier
             * @ingroup ExtendedTraits
             */
            template <typename T>
            inline constexpr bool NkIsEmpty_v = NkIsEmpty<T>::value;

            /**
             * @brief Vérification de polymorphisme
             * @struct NkIsPolymorphic
             * @tparam T Type à vérifier
             * @ingroup ExtendedTraits
             *
             * Utilise __is_polymorphic si disponible, sinon assume false.
             */
            #if defined(__clang__) || defined(__GNUC__) || defined(_MSC_VER)
                template <typename T>
                struct NkIsPolymorphic : NkBoolConstant<__is_polymorphic(T)> {};
            #else
                template <typename T>
                struct NkIsPolymorphic : NkFalseType {};
            #endif

            /**
             * @brief Valeur constexpr pour NkIsPolymorphic
             * @def NkIsPolymorphic_v
             * @tparam T Type à vérifier
             * @ingroup ExtendedTraits
             */
            template <typename T>
            inline constexpr bool NkIsPolymorphic_v = NkIsPolymorphic<T>::value;

        } // namespace traits

        // =====================================================================
        // SECTION 17 : ALIAS DE NAMESPACE POUR COMPATIBILITÉ
        // =====================================================================
        // Fournit un namespace alias pour faciliter l'utilisation des traits.

        /**
         * @brief Namespace alias pour nkentseu::traits
         * @namespace traits_alias
         * @ingroup TraitUtilities
         *
         * Permet d'utiliser les traits sans préfixe répétitif :
         * @code
         * using namespace nkentseu::traits_alias;
         * static_assert(IsIntegral_v<int>);
         * @endcode
         */
        namespace traits_alias {
            using namespace traits;
        }

    } // namespace nkentseu

#endif // NKENTSEU_CORE_NKTRAITS_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKTRAITS.H
// =============================================================================
// Ce fichier fournit une implémentation complète des traits de type C++
// sans dépendance STL, avec des extensions spécifiques à NKCore.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Utilisation basique des traits de classification
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkTraits.h"

    void BasicTraitUsage() {
        using namespace nkentseu::traits;

        // Vérification de type
        static_assert(NkIsIntegral_v<int> == true);
        static_assert(NkIsFloatingPoint_v<double> == true);
        static_assert(NkIsPointer_v<int*> == true);
        static_assert(NkIsArray_v<int[10]> == true);

        // Comparaison de type
        static_assert(NkIsSame_v<int, int> == true);
        static_assert(NkIsSame_v<int, const int> == false);

        // Suppression de qualifications
        static_assert(NkIsSame_v<NkRemoveConst_t<const int>, int>);
        static_assert(NkIsSame_v<NkRemoveCV_t<const volatile double>, double>);
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Métaprogrammation conditionnelle avec SFINAE
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkTraits.h"

    // Fonction template activée seulement pour les types intégraux
    template<typename T>
    NkEnableIf_t<NkIsIntegral_v<T>, T> AddOne(T value) {
        return value + 1;
    }

    // Fonction template activée seulement pour les types flottants
    template<typename T>
    NkEnableIf_t<NkIsFloatingPoint_v<T>, T> AddOne(T value) {
        return value + 1.0f;
    }

    // Sélection conditionnelle de type
    template<typename T>
    using SmartPointer = NkConditional_t<
        NkIsTriviallyDestructible<T>,
        T*,  // Pour les types triviaux, utiliser un pointeur brut
        std::unique_ptr<T>  // Pour les types complexes, utiliser unique_ptr
    >;
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Traits spécifiques NKCore pour les caractères
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkTraits.h"

    template<typename CharType>
    NkEnableIf_t<NkIsCharacterType_v<CharType>, void> ProcessString(const CharType* str) {
        // Cette fonction ne compile que pour les types caractère NKCore valides
        while (*str) {
            ProcessCharacter(*str++);
        }
    }

    void TestCharacterProcessing() {
        ProcessString("Hello");        // char*
        ProcessString(L"World");       // wchar_t*
        ProcessString(u8"UTF-8");      // char8_t*
        ProcessString(u"UTF-16");      // char16_t*
        ProcessString(U"UTF-32");      // char32_t*

        // ProcessString(123);         // Erreur de compilation : int n'est pas un caractère
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Vérification de support de plateforme pour les types 128-bit
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkTraits.h"

    template<typename T>
    void SafeProcess128Bit(const T& value) {
        if constexpr (NkIsPlatformSupported_v<T>) {
            // Traitement optimisé pour les types 128-bit natifs
            ProcessNative128(value);
        } else {
            // Fallback pour les structures émulées
            ProcessEmulated128(value);
        }
    }

    void Test128BitSupport() {
        nk_uint64 a = 1, b = 2;
        
        #if NKENTSEU_INT128_AVAILABLE
            nk_uint128 result = static_cast<nk_uint128>(a) * b;
            SafeProcess128Bit(result);  // Utilise ProcessNative128
        #else
            // Utilisation de la structure émulée
            nk_uint128 emulated{b, a};
            SafeProcess128Bit(emulated);  // Utilise ProcessEmulated128
        #endif
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Utilisation des nouveaux traits ajoutés (trivial, POD, etc.)
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkTraits.h"

    struct TrivialStruct {
        int x;
        float y;
    };

    struct NonTrivialStruct {
        NonTrivialStruct() = default;
        NonTrivialStruct(const NonTrivialStruct&) { /\* custom copy *\/ }
        int data;
    };

    void TestExtendedTraits() {
        using namespace nkentseu::traits;

        // Vérification de trivialité
        static_assert(NkIsTrivial_v<TrivialStruct> == true);
        static_assert(NkIsTrivial_v<NonTrivialStruct> == false);

        // Vérification de POD
        static_assert(NkIsPOD_v<TrivialStruct> == true);
        static_assert(NkIsPOD_v<int> == true);
        static_assert(NkIsPOD_v<std::string> == false);

        // Vérification de standard layout
        static_assert(NkIsStandardLayout_v<TrivialStruct> == true);

        // Taille en bits
        static_assert(NkBitSize_v<uint32> == 32);
        static_assert(NkBitSize_v<uint64> == 64);

        // Vérification de type vide
        struct Empty {};
        static_assert(NkIsEmpty_v<Empty> == true);
    }
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Combinaison avec d'autres modules NKCore
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkTypes.h"
    #include "NKCore/NkTraits.h"
    #include "NKPlatform/NkPlatformDetect.h"

    // Template sécurisé pour les types NKCore
    template<typename T>
    NkEnableIf_t<
        NkIsArithmetic_v<T> && 
        NkIsPlatformSupported_v<T> &&
        (NkBitSize_v<T> >= 32),
        T
    > SafeMultiply(T a, T b) {
        // Multiplication sécurisée pour les types arithmétiques >= 32 bits
        #ifdef NKENTSEU_PLATFORM_WINDOWS
            // Implémentation Windows-specific si nécessaire
            return _mul128(a, b, nullptr);
        #else
            return a * b;
        #endif
    }

    void TestSafeArithmetic() {
        nk_uint32 result32 = SafeMultiply(nk_uint32(1000), nk_uint32(2000));
        nk_uint64 result64 = SafeMultiply(nk_uint64(1000000), nk_uint64(2000000));
        
        // SafeMultiply(nk_uint8(10), nk_uint8(20));  // Erreur : < 32 bits
        // SafeMultiply("hello", "world");           // Erreur : non arithmétique
    }
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Utilisation de l'alias namespace pour une syntaxe concise
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkTraits.h"

    // Utilisation de l'alias pour une syntaxe plus concise
    using namespace nkentseu::traits_alias;

    template<typename T>
    concept IntegralType = IsIntegral_v<T>;

    template<IntegralType T>
    T Square(T x) {
        return x * x;
    }

    // Ou sans concepts C++20 :
    template<typename T>
    EnableIf_t<IsIntegral_v<T>, T> Cube(T x) {
        return x * x * x;
    }

    void TestAliasUsage() {
        int result1 = Square(5);    // OK
        float result2 = Square(5.0f);  // Erreur de compilation
        
        int result3 = Cube(3);      // OK
        // float result4 = Cube(3.0f);  // Erreur de compilation
    }
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================