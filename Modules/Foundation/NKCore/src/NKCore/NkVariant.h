// =============================================================================
// NKCore/NkVariant.h
// Implémentation type-safe de variant (union discriminée) pour types multiples.
//
// Design :
//  - Classe template NkVariant<Ts...> inspirée de std::variant (C++17)
//  - Stockage aligné et dimensionné automatiquement via NkMaxSizeOf/NkMaxAlignOf
//  - Construction, copie, déplacement et assignment avec gestion d'état
//  - Accès type-safe : Get<T>(), GetChecked<T>(), GetIf<T>(), HoldsAlternative<T>()
//  - Pattern visiteur via Visit() intégré avec NkInvoke pour dispatch générique
//  - Emplace<T>() pour construction in-place avec forwarding parfait
//  - Reset() pour destruction explicite et retour à l'état vide
//  - Intégration avec NkAssert pour vérifications debug dans GetChecked()
//  - Compatibilité C++14+ avec if constexpr et détection de types compile-time
//
// Intégration :
//  - Utilise NKCore/NkInvoke.h pour NkInvoke() dans le pattern visiteur
//  - Utilise NKCore/Assert/NkAssert.h pour NK_ASSERT() dans les accès vérifiés
//  - Utilise NKCore/NkTraits.h (via NkInvoke) pour NkEnableIf_t, NkForward, etc.
//  - Header-only : aucune implémentation .cpp requise
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_CORE_NKVARIANT_H_INCLUDED
#define NKENTSEU_CORE_NKVARIANT_H_INCLUDED

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion des modules requis pour l'invocation et les assertions.

    #include "NkInvoke.h"
    #include "NKCore/Assert/NkAssert.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : ESPACE DE NOMS PRINCIPAL
    // -------------------------------------------------------------------------
    // Déclaration de l'espace de noms principal nkentseu.

    namespace nkentseu {

        // ====================================================================
        // SECTION 3 : NAMESPACE DETAIL - UTILITAIRES MÉTAPROGRAMMATION
        // ====================================================================
        // Helpers compile-time pour gestion des types dans NkVariant.

        /**
         * @defgroup VariantDetailHelpers Helpers Internes pour NkVariant
         * @brief Templates utilitaires pour métaprogrammation de types
         * @ingroup VariantUtilities
         * @internal
         *
         * Ces templates fournissent les primitives compile-time nécessaires
         * pour l'implémentation de NkVariant : calcul de taille/alignement,
         * recherche d'index de type, accès par index, vérification d'appartenance.
         *
         * @note
         *   - Ne pas utiliser directement : API interne sujette à changement
         *   - Tous les calculs sont constexpr pour évaluation compile-time
         *   - Basé sur le pattern de récursion template pour itération sur packs
         *
         * @warning
         *   NkTypeIndex<T> sans match dans le pack déclenche un static_assert
         *   avec NkAlwaysFalse : erreur de compilation explicite pour type invalide.
         */
        /** @{ */

        namespace detail {

            /**
             * @brief Helper pour générer des static_assert toujours faux
             * @tparam T Type dummy pour dépendance template
             * @ingroup VariantDetailHelpers
             * @note Utilisé pour messages d'erreur personnalisés dans static_assert
             */
            template <typename T>
            struct NkAlwaysFalse : traits::NkFalseType {};

            /**
             * @brief Calcule la taille maximale parmi un pack de types
             * @tparam Ts Pack de types à analyser
             * @ingroup VariantDetailHelpers
             * @note Résultat accessible via ::value (hérite de NkIntegralConstant)
             *
             * @example
             * @code
             * constexpr auto maxSize = NkMaxSizeOf<int, double, char>::value;
             * // maxSize == sizeof(double) sur la plupart des plateformes
             * @endcode
             */
            template <typename... Ts>
            struct NkMaxSizeOf;

            // Cas de base : un seul type
            template <typename T>
            struct NkMaxSizeOf<T> : traits::NkIntegralConstant<nk_size, sizeof(T)> {};

            // Récursion : comparer le premier avec le max du reste
            template <typename T, typename U, typename... Rest>
            struct NkMaxSizeOf<T, U, Rest...>
                : traits::NkIntegralConstant<
                    nk_size,
                    (sizeof(T) > NkMaxSizeOf<U, Rest...>::value
                        ? sizeof(T)
                        : NkMaxSizeOf<U, Rest...>::value)> {};

            /**
             * @brief Calcule l'alignement maximal requis parmi un pack de types
             * @tparam Ts Pack de types à analyser
             * @ingroup VariantDetailHelpers
             * @note Essentiel pour l'alignement correct du stockage raw de NkVariant
             */
            template <typename... Ts>
            struct NkMaxAlignOf;

            // Cas de base
            template <typename T>
            struct NkMaxAlignOf<T> : traits::NkIntegralConstant<nk_size, alignof(T)> {};

            // Récursion
            template <typename T, typename U, typename... Rest>
            struct NkMaxAlignOf<T, U, Rest...>
                : traits::NkIntegralConstant<
                    nk_size,
                    (alignof(T) > NkMaxAlignOf<U, Rest...>::value
                        ? alignof(T)
                        : NkMaxAlignOf<U, Rest...>::value)> {};

            /**
             * @brief Trouve l'index (0-based) d'un type dans un pack de types
             * @tparam T Type à rechercher
             * @tparam Ts Pack de types dans lequel chercher
             * @ingroup VariantDetailHelpers
             * @note Retourne kNpos (nk_size(-1)) si non trouvé, via static_assert
             *
             * @warning
             *   Si T n'est pas dans Ts..., la spécialisation finale déclenche
             *   un static_assert avec un message explicite "Type is not part...".
             */
            template <typename T, typename... Ts>
            struct NkTypeIndex;

            // Match trouvé en tête de pack
            template <typename T, typename... Rest>
            struct NkTypeIndex<T, T, Rest...> : traits::NkIntegralConstant<nk_size, 0u> {};

            // Récursion : incrémenter l'index si pas en tête
            template <typename T, typename U, typename... Rest>
            struct NkTypeIndex<T, U, Rest...>
                : traits::NkIntegralConstant<
                    nk_size,
                    1u + NkTypeIndex<T, Rest...>::value> {};

            // Cas d'erreur : type non trouvé
            template <typename T>
            struct NkTypeIndex<T> {
                static_assert(
                    NkAlwaysFalse<T>::value,
                    "Type is not part of this NkVariant"
                );
            };

            /**
             * @brief Accède au type à un index donné dans un pack de types
             * @tparam Index Position 0-based du type souhaité
             * @tparam Ts Pack de types source
             * @ingroup VariantDetailHelpers
             * @note Type résultat accessible via ::type
             *
             * @warning
             *   Si Index >= sizeof...(Ts), comportement indéfini à la compilation.
             *   Utiliser uniquement avec des index valides garantis par le contexte.
             */
            template <nk_size Index, typename... Ts>
            struct NkTypeAt;

            // Cas de base : index 0 → premier type
            template <typename T, typename... Rest>
            struct NkTypeAt<0u, T, Rest...> {
                using type = T;
            };

            // Récursion : décrémenter l'index et avancer dans le pack
            template <nk_size Index, typename T, typename... Rest>
            struct NkTypeAt<Index, T, Rest...> {
                using type = typename NkTypeAt<Index - 1u, Rest...>::type;
            };

            /**
             * @brief Vérifie si un type T fait partie d'un pack Ts...
             * @tparam T Type à tester
             * @tparam Ts Pack de types de référence
             * @ingroup VariantDetailHelpers
             * @note Variable template constexpr : valeur connue à la compilation
             *
             * @example
             * @code
             * static_assert(NkContainsType_v<int, int, float, double>, "int is supported");
             * static_assert(!NkContainsType_v<char, int, float, double>, "char not supported");
             * @endcode
             */
            template <typename T, typename... Ts>
            inline constexpr nk_bool NkContainsType_v = traits::NkIsAnyOf<T, Ts...>::value;

        } // namespace detail

        /** @} */ // End of VariantDetailHelpers

        // ====================================================================
        // SECTION 4 : CLASSE TEMPLATE NKVARIANT
        // ====================================================================
        // Union discriminée type-safe pour stockage d'un type parmi plusieurs.

        /**
         * @defgroup VariantClass Classe NkVariant - Union Discriminée Type-Safe
         * @brief Container pour exactement un type parmi un pack de types spécifiés
         * @ingroup CoreContainers
         *
         * NkVariant implémente une union discriminée similaire à std::variant (C++17),
         * permettant de stocker de manière type-safe exactement une valeur parmi
         * plusieurs types possibles, avec gestion automatique de la construction,
         * destruction, copie et déplacement.
         *
         * @tparam Ts Pack de types supportés par cette instance de variant
         *
         * @note
         *   - Stockage raw aligné via alignas + tableau de nk_uint8
         *   - Index du type actif stocké séparément (mIndex) pour dispatch rapide
         *   - État "vide" possible via Reset() ou construction par défaut sans type constructible
         *   - Accès via Get<T>() (non vérifié) ou GetChecked<T>() (avec assertion debug)
         *   - Pattern visiteur via Visit() intégré avec NkInvoke pour polymorphisme
         *
         * @warning
         *   - Get<T>() sans vérification préalable peut causer un comportement indéfini
         *     si le type actif n'est pas T : préférer GetChecked<T>() en debug ou GetIf<T>()
         *   - La copie/déplacement peut lever des exceptions si les constructeurs des types
         *     sous-jacents en lèvent : utiliser avec précaution dans du code noexcept
         *   - NkVariant n'est pas thread-safe : synchronisation externe requise pour accès concurrents
         *
         * @example
         * @code
         * // Variant pour représenter une valeur JSON simplifiée
         * using JsonValue = nkentseu::NkVariant<
         *     nkentseu::nk_nullptr_t,  // null
         *     bool,                     // boolean
         *     double,                   // number
         *     nkentseu::nk_string,      // string
         *     nkentseu::nk_array<JsonValue>,  // array
         *     nkentseu::nk_map<nkentseu::nk_string, JsonValue>  // object
         * >;
         *
         * // Construction et accès
         * JsonValue v1(42.0);  // Stocke un double
         * if (v1.HoldsAlternative<double>()) {
         *     double num = v1.Get<double>();  // Accès vérifié manuellement
         * }
         *
         * // Accès avec vérification automatique (debug)
         * double num = v1.GetChecked<double>();  // Assertion si type incorrect
         *
         * // Pattern visiteur pour traitement polymorphique
         * v1.Visit([](auto&& value) {
         *     using T = std::decay_t<decltype(value)>;
         *     if constexpr (std::is_same_v<T, double>) {
         *         printf("Number: %f\n", value);
         *     } else if constexpr (std::is_same_v<T, bool>) {
         *         printf("Boolean: %s\n", value ? "true" : "false");
         *     }
         *     // ... autres types ...
         * });
         *
         * // Réassignation et Reset
         * v1 = true;  // Change le type actif vers bool
         * v1.Reset();  // Détruit la valeur courante, variant devient "vide"
         * @endcode
         */
        /** @{ */

        template <typename... Ts>
        class NkVariant {
            // Vérification compile-time : au moins un type requis
            static_assert(sizeof...(Ts) > 0u, "NkVariant requires at least one type");

            public:
                // --------------------------------------------------------
                // Constantes et types publics
                // --------------------------------------------------------

                /** @brief Nombre de types supportés par ce variant */
                static constexpr nk_size kTypeCount = sizeof...(Ts);

                /** @brief Valeur sentinelle pour index invalide (équivalent à std::variant::npos) */
                static constexpr nk_size kNpos = static_cast<nk_size>(-1);

                // --------------------------------------------------------
                // Constructeurs
                // --------------------------------------------------------

                /**
                 * @brief Constructeur par défaut : initialise avec le premier type constructible
                 * @note
                 *   - Si le premier type (T0) est constructible sans arguments, l'emplace
                 *   - Sinon, le variant reste dans l'état "vide" (mHasValue = false)
                 *   - Comportement similaire à std::variant pour compatibilité
                 */
                NkVariant()
                    : mIndex(kNpos)
                    , mHasValue(false)
                {
                    using T0 = typename detail::NkTypeAt<0u, Ts...>::type;
                    if constexpr (__is_constructible(T0)) {
                        Emplace<T0>();
                    }
                }

                /**
                 * @brief Destructeur : libère la valeur stockée si présente
                 * @note Appelle Reset() pour destruction propre via le destructeur du type actif
                 */
                ~NkVariant() {
                    Reset();
                }

                /**
                 * @brief Constructeur de copie : copie la valeur d'un autre variant
                 * @param other Variant source à copier
                 * @note
                 *   - Copie profonde : construit une nouvelle instance du type actif
                 *   - Si other est vide, ce variant reste vide
                 *   - Requiert que le type actif soit copiable
                 */
                NkVariant(const NkVariant& other)
                    : mIndex(kNpos)
                    , mHasValue(false)
                {
                    CopyFrom(other);
                }

                /**
                 * @brief Constructeur de déplacement : transfère la valeur d'un autre variant
                 * @param other Variant source à déplacer (laissé dans un état valide mais indéterminé)
                 * @note
                 *   - Déplacement profond : construit via constructeur de mouvement du type actif
                 *   - noexcept si le constructeur de mouvement du type actif est noexcept
                 *   - Plus efficace que la copie pour les types lourds
                 */
                NkVariant(NkVariant&& other) noexcept
                    : mIndex(kNpos)
                    , mHasValue(false)
                {
                    MoveFrom(traits::NkMove(other));
                }

                /**
                 * @brief Constructeur depuis une valeur de type T présent dans le pack
                 * @tparam T Type de la valeur source (déduit automatiquement)
                 * @param value Valeur à encapsuler dans le variant
                 * @note
                 *   - Activation conditionnelle via NkEnableIf : T (après removal de cv/ref) doit être dans Ts...
                 *   - Utilise Emplace<D>() avec forwarding parfait pour construction in-place
                 *   - Évite les copies/movements inutiles grâce à la construction directe
                 */
                template <
                    typename T,
                    typename D = traits::NkRemoveCV_t<traits::NkRemoveReference_t<T>>,
                    typename traits::NkEnableIf_t<detail::NkContainsType_v<D, Ts...>, int> = 0
                >
                NkVariant(T&& value)
                    : mIndex(kNpos)
                    , mHasValue(false)
                {
                    Emplace<D>(traits::NkForward<T>(value));
                }

                // --------------------------------------------------------
                // Opérateurs d'assignment
                // --------------------------------------------------------

                /**
                 * @brief Assignment par copie depuis un autre variant
                 * @param other Variant source à copier
                 * @return Référence vers ce variant pour chaînage
                 * @note
                 *   - Gère l'auto-assignment (this == &other) sans effet
                 *   - Détruit l'ancienne valeur avant de copier la nouvelle
                 *   - Exception-safe : si la copie lève, ce variant reste dans un état valide
                 */
                NkVariant& operator=(const NkVariant& other) {
                    if (this == &other) {
                        return *this;
                    }
                    Reset();
                    CopyFrom(other);
                    return *this;
                }

                /**
                 * @brief Assignment par déplacement depuis un autre variant
                 * @param other Variant source à déplacer
                 * @return Référence vers ce variant pour chaînage
                 * @note
                 *   - noexcept si le mouvement du type actif est noexcept
                 *   - Plus efficace que la copie pour les types avec ressources
                 *   - other est laissé dans un état valide mais indéterminé après le move
                 */
                NkVariant& operator=(NkVariant&& other) noexcept {
                    if (this == &other) {
                        return *this;
                    }
                    Reset();
                    MoveFrom(traits::NkMove(other));
                    return *this;
                }

                /**
                 * @brief Assignment depuis une valeur de type T présent dans le pack
                 * @tparam T Type de la valeur source (déduit automatiquement)
                 * @param value Valeur à assigner au variant
                 * @return Référence vers ce variant pour chaînage
                 * @note
                 *   - Détruit l'ancienne valeur avant d'emplace la nouvelle
                 *   - Utilise forwarding parfait pour éviter les copies inutiles
                 *   - Activation conditionnelle : T doit être l'un des types du pack
                 */
                template <
                    typename T,
                    typename D = traits::NkRemoveCV_t<traits::NkRemoveReference_t<T>>,
                    typename traits::NkEnableIf_t<detail::NkContainsType_v<D, Ts...>, int> = 0
                >
                NkVariant& operator=(T&& value) {
                    Emplace<D>(traits::NkForward<T>(value));
                    return *this;
                }

                // --------------------------------------------------------
                // Méthodes d'inspection d'état
                // --------------------------------------------------------

                /**
                 * @brief Vérifie si le variant est dans un état "vide" (sans valeur active)
                 * @return true si aucune valeur n'est stockée, false sinon
                 * @note
                 *   - État "vide" possible après Reset() ou construction par défaut échouée
                 *   - Nommé ValuelessByException pour compatibilité sémantique avec std::variant
                 *   - Constante : ne modifie pas l'état du variant
                 */
                [[nodiscard]]
                nk_bool ValuelessByException() const noexcept {
                    return !mHasValue;
                }

                /**
                 * @brief Vérifie si le variant contient une valeur active
                 * @return true si une valeur est stockée, false si vide
                 * @note Alias sémantique pour ValuelessByException() avec logique inversée
                 */
                [[nodiscard]]
                nk_bool HasValue() const noexcept {
                    return mHasValue;
                }

                /**
                 * @brief Obtient l'index (0-based) du type actif, ou kNpos si vide
                 * @return Index du type stocké, ou kNpos si aucune valeur
                 * @note
                 *   - Utile pour dispatch manuel via switch sur l'index
                 *   - Plus rapide que HoldAlternative<T>() pour multiples vérifications
                 *   - Constante : lecture seule de l'état interne
                 */
                [[nodiscard]]
                nk_size Index() const noexcept {
                    return mHasValue ? mIndex : kNpos;
                }

                // --------------------------------------------------------
                // Méthodes de gestion du cycle de vie
                // --------------------------------------------------------

                /**
                 * @brief Détruit la valeur courante et retourne à l'état vide
                 * @note
                 *   - Appelle le destructeur du type actif via placement-new inverse
                 *   - Noexcept : ne lève jamais d'exception (destructor devrait être noexcept)
                 *   - Idempotent : appel multiple sans effet si déjà vide
                 */
                void Reset() noexcept {
                    if (!mHasValue) {
                        return;
                    }
                    DestroyByIndex();
                    mIndex = kNpos;
                    mHasValue = false;
                }

                /**
                 * @brief Construit in-place une valeur de type T avec arguments forwarding
                 * @tparam T Type à construire (doit faire partie du pack Ts...)
                 * @tparam Args Types des arguments de construction (déduits)
                 * @param args Arguments à forwarder au constructeur de T
                 * @return Référence vers la nouvelle valeur construite
                 * @note
                 *   - Détruit d'abord l'ancienne valeur via Reset() si présente
                 *   - Utilise placement-new pour construction directe dans le stockage raw
                 *   - Forwarding parfait préserve les catégories de valeur (lvalue/rvalue)
                 * @warning
                 *   Si la construction de T lève une exception, le variant reste vide
                 *   et dans un état sûr (pas de fuite de ressources)
                 */
                template <
                    typename T,
                    typename... Args,
                    typename traits::NkEnableIf_t<detail::NkContainsType_v<T, Ts...>, int> = 0
                >
                T& Emplace(Args&&... args) {
                    Reset();
                    new (mStorage) T(traits::NkForward<Args>(args)...);
                    mIndex = detail::NkTypeIndex<T, Ts...>::value;
                    mHasValue = true;
                    return *reinterpret_cast<T*>(mStorage);
                }

                // --------------------------------------------------------
                // Méthodes d'accès type-safe
                // --------------------------------------------------------

                /**
                 * @brief Vérifie si le type actif est T
                 * @tparam T Type à tester
                 * @return true si le variant contient actuellement une valeur de type T
                 * @note
                 *   - Comparaison d'index : O(1), très efficace
                 *   - Retourne false si le variant est vide (mHasValue == false)
                 *   - Constante : ne modifie pas l'état du variant
                 */
                template <typename T>
                [[nodiscard]]
                nk_bool HoldsAlternative() const noexcept {
                    if (!mHasValue) {
                        return false;
                    }
                    return mIndex == detail::NkTypeIndex<T, Ts...>::value;
                }

                /**
                 * @brief Accès non vérifié à la valeur de type T
                 * @tparam T Type attendu de la valeur stockée
                 * @return Référence non-const vers la valeur
                 * @warning
                 *   - Aucun check runtime : comportement indéfini si le type actif n'est pas T
                 *   - Utiliser uniquement après vérification via HoldsAlternative<T>() ou Index()
                 *   - Préférer GetChecked<T>() en debug pour sécurité accrue
                 */
                template <typename T>
                T& Get() noexcept {
                    return *reinterpret_cast<T*>(mStorage);
                }

                /**
                 * @brief Accès non vérifié à la valeur de type T (version const)
                 * @tparam T Type attendu de la valeur stockée
                 * @return Référence const vers la valeur
                 * @warning Même avertissement que la version non-const
                 */
                template <typename T>
                const T& Get() const noexcept {
                    return *reinterpret_cast<const T*>(mStorage);
                }

                /**
                 * @brief Accès vérifié à la valeur de type T (avec assertion debug)
                 * @tparam T Type attendu de la valeur stockée
                 * @return Référence non-const vers la valeur
                 * @note
                 *   - En mode debug : NK_ASSERT vérifie HoldsAlternative<T>() avant accès
                 *   - En mode release : équivalent à Get<T>() (zéro overhead)
                 *   - Recommandé pour un développement plus sûr sans impact production
                 */
                template <typename T>
                T& GetChecked() {
                    NK_ASSERT(HoldsAlternative<T>());
                    return Get<T>();
                }

                /**
                 * @brief Accès vérifié à la valeur de type T (version const)
                 * @tparam T Type attendu de la valeur stockée
                 * @return Référence const vers la valeur
                 */
                template <typename T>
                const T& GetChecked() const {
                    NK_ASSERT(HoldsAlternative<T>());
                    return Get<T>();
                }

                /**
                 * @brief Accès conditionnel avec retour de pointeur nullptr si type incorrect
                 * @tparam T Type à rechercher
                 * @return Pointeur vers la valeur si type actif == T, nullptr sinon
                 * @note
                 *   - Méthode safe : jamais de comportement indéfini
                 *   - Utile pour les accès optionnels sans exception ni assertion
                 *   - Retourne nullptr aussi si le variant est vide
                 */
                template <typename T>
                T* GetIf() noexcept {
                    return HoldsAlternative<T>()
                        ? reinterpret_cast<T*>(mStorage)
                        : nullptr;
                }

                /**
                 * @brief Accès conditionnel avec retour de pointeur const si type incorrect
                 * @tparam T Type à rechercher
                 * @return Pointeur const vers la valeur si type actif == T, nullptr sinon
                 */
                template <typename T>
                const T* GetIf() const noexcept {
                    return HoldsAlternative<T>()
                        ? reinterpret_cast<const T*>(mStorage)
                        : nullptr;
                }

                // --------------------------------------------------------
                // Pattern visiteur (polymorphisme sans héritage)
                // --------------------------------------------------------

                /**
                 * @brief Applique un visiteur à la valeur stockée (version non-const)
                 * @tparam Visitor Type du callable visiteur (déduit)
                 * @param visitor Callable à invoquer avec la valeur comme argument
                 * @note
                 *   - Utilise NkInvoke pour dispatch générique vers le visiteur
                 *   - Le visiteur doit être surchargé ou utiliser auto/decltype pour gérer tous les types possibles
                 *   - Ne fait rien si le variant est vide (ValuelessByException())
                 * @warning
                 *   Si le visiteur n'est pas invocable avec le type actif, erreur de compilation
                 *   Utiliser if constexpr dans le visiteur pour gérer les types optionnellement
                 */
                template <typename Visitor>
                void Visit(Visitor&& visitor) {
                    if (!mHasValue) {
                        return;
                    }
                    VisitImpl<0u>(visitor);
                }

                /**
                 * @brief Applique un visiteur à la valeur stockée (version const)
                 * @tparam Visitor Type du callable visiteur (déduit)
                 * @param visitor Callable à invoquer avec la valeur const comme argument
                 */
                template <typename Visitor>
                void Visit(Visitor&& visitor) const {
                    if (!mHasValue) {
                        return;
                    }
                    VisitImplConst<0u>(visitor);
                }

                // --------------------------------------------------------
                // Méthodes utilitaires
                // --------------------------------------------------------

                /**
                 * @brief Échange le contenu de ce variant avec un autre
                 * @param other Autre variant à échanger avec celui-ci
                 * @note
                 *   - Implémentation naive via move temporaire : peut être optimisée
                 *   - Gère l'auto-swap (this == &other) sans effet
                 *   - Exception-safe : si un move lève, les variants restent dans un état valide
                 */
                void Swap(NkVariant& other) {
                    if (this == &other) {
                        return;
                    }
                    NkVariant tmp(traits::NkMove(other));
                    other = traits::NkMove(*this);
                    *this = traits::NkMove(tmp);
                }

            private:
                // --------------------------------------------------------
                // Alias et helpers internes
                // --------------------------------------------------------

                /** @brief Alias pour accéder au type à l'index I dans le pack Ts... */
                template <nk_size I>
                using TypeAt = typename detail::NkTypeAt<I, Ts...>::type;

                // --------------------------------------------------------
                // Méthodes privées de gestion interne
                // --------------------------------------------------------

                /**
                 * @brief Détruit la valeur active en fonction de son index (récursif compile-time)
                 * @tparam I Index courant dans la récursion (défaut: 0)
                 * @note
                 *   - Utilise if constexpr pour éliminer les branches non pertinentes à la compilation
                 *   - Appelle explicitement le destructeur via reinterpret_cast + placement-new inverse
                 *   - Noexcept : suppose que les destructeurs des types Ts... sont noexcept
                 */
                template <nk_size I = 0u>
                void DestroyByIndex() noexcept {
                    if constexpr (I < kTypeCount) {
                        if (mIndex == I) {
                            reinterpret_cast<TypeAt<I>*>(mStorage)->~TypeAt<I>();
                            return;
                        }
                        DestroyByIndex<I + 1u>();
                    }
                }

                /**
                 * @brief Copie la valeur d'un autre variant en fonction de son index actif
                 * @tparam I Index courant dans la récursion (défaut: 0)
                 * @param other Variant source à copier
                 * @note
                 *   - Construction par placement-new avec constructeur de copie du type actif
                 *   - Met à jour mIndex et mHasValue après copie réussie
                 *   - Gère le cas other vide en retour immédiat
                 */
                template <nk_size I = 0u>
                void CopyFrom(const NkVariant& other) {
                    if (!other.mHasValue) {
                        return;
                    }
                    if constexpr (I < kTypeCount) {
                        if (other.mIndex == I) {
                            using T = TypeAt<I>;
                            new (mStorage) T(*reinterpret_cast<const T*>(other.mStorage));
                            mIndex = I;
                            mHasValue = true;
                            return;
                        }
                        CopyFrom<I + 1u>(other);
                    }
                }

                /**
                 * @brief Déplace la valeur d'un autre variant en fonction de son index actif
                 * @tparam I Index courant dans la récursion (défaut: 0)
                 * @param other Variant source à déplacer (forwardé comme rvalue)
                 * @note
                 *   - Construction par placement-new avec constructeur de mouvement du type actif
                 *   - other est laissé dans un état valide mais indéterminé après le move
                 *   - noexcept si le constructeur de mouvement du type actif est noexcept
                 */
                template <nk_size I = 0u>
                void MoveFrom(NkVariant&& other) {
                    if (!other.mHasValue) {
                        return;
                    }
                    if constexpr (I < kTypeCount) {
                        if (other.mIndex == I) {
                            using T = TypeAt<I>;
                            new (mStorage) T(traits::NkMove(*reinterpret_cast<T*>(other.mStorage)));
                            mIndex = I;
                            mHasValue = true;
                            return;
                        }
                        MoveFrom<I + 1u>(traits::NkMove(other));
                    }
                }

                /**
                 * @brief Implémentation récursive du visiteur pour variant non-const
                 * @tparam I Index courant dans la récursion (défaut: 0)
                 * @tparam Visitor Type du callable visiteur
                 * @param visitor Référence vers le visiteur à invoquer
                 * @note
                 *   - Compare mIndex avec I pour trouver le type actif
                 *   - Utilise NkInvoke pour appeler le visiteur avec la valeur déréférencée
                 *   - if constexpr élimine les branches non pertinentes à la compilation
                 */
                template <nk_size I = 0u, typename Visitor>
                void VisitImpl(Visitor& visitor) {
                    if constexpr (I < kTypeCount) {
                        if (mIndex == I) {
                            NkInvoke(visitor, *reinterpret_cast<TypeAt<I>*>(mStorage));
                            return;
                        }
                        VisitImpl<I + 1u>(visitor);
                    }
                }

                /**
                 * @brief Implémentation récursive du visiteur pour variant const
                 * @tparam I Index courant dans la récursion (défaut: 0)
                 * @tparam Visitor Type du callable visiteur
                 * @param visitor Référence const vers le visiteur à invoquer
                 */
                template <nk_size I = 0u, typename Visitor>
                void VisitImplConst(Visitor& visitor) const {
                    if constexpr (I < kTypeCount) {
                        if (mIndex == I) {
                            NkInvoke(visitor, *reinterpret_cast<const TypeAt<I>*>(mStorage));
                            return;
                        }
                        VisitImplConst<I + 1u>(visitor);
                    }
                }

                // --------------------------------------------------------
                // Membres de données
                // --------------------------------------------------------

                /**
                 * @brief Stockage raw aligné pour contenir n'importe quel type de Ts...
                 * @note
                 *   - alignas(NkMaxAlignOf<Ts...>::value) garantit l'alignement requis
                 *   - Taille : NkMaxSizeOf<Ts...>::value octets, suffisant pour le plus grand type
                 *   - Accès via reinterpret_cast<T*> : sûr car seul un type est actif à la fois
                 */
                alignas(detail::NkMaxAlignOf<Ts...>::value)
                nk_uint8 mStorage[detail::NkMaxSizeOf<Ts...>::value];

                /** @brief Index du type actif (0-based), ou kNpos si vide */
                nk_size mIndex;

                /** @brief Flag indiquant si une valeur est actuellement stockée */
                nk_bool mHasValue;
        };

        /** @} */ // End of VariantClass

        // ====================================================================
        // SECTION 5 : FONCTIONS LIBRES UTILITAIRES POUR NKVARIANT
        // ====================================================================
        // Helpers non-membres pour usage ergonomique avec déduction de template.

        /**
         * @defgroup VariantFreeFunctions Fonctions Libres pour NkVariant
         * @brief Helpers non-membres pour accès et visite avec déduction de type automatique
         * @ingroup VariantUtilities
         *
         * Ces fonctions fournissent une interface alternative aux méthodes membres
         * de NkVariant, avec l'avantage de la déduction de template pour le type
         * du variant, évitant d'avoir à spécifier explicitement le pack de types.
         *
         * @note
         *   - NkHoldsAlternative<T>(variant) : équivalent à variant.HoldsAlternative<T>()
         *   - NkGetIf<T>(&variant) : accès conditionnel via pointeur, nullptr si type incorrect
         *   - NkVisit(visitor, variant) : applique un visiteur avec forwarding parfait
         *   - Toutes les fonctions sont inline constexpr ou noexcept quand approprié
         *
         * @example
         * @code
         * nkentseu::NkVariant<int, double, std::string> v(42);
         *
         * // Déduction automatique du type du variant
         * if (nkentseu::NkHoldsAlternative<int>(v)) {
         *     int* p = nkentseu::NkGetIf<int>(&v);  // p != nullptr
         *     if (p) { printf("Value: %d\n", *p); }
         * }
         *
         * // Visiteur avec déduction de type du variant
         * nkentseu::NkVisit([](auto&& val) {
         *     using T = std::decay_t<decltype(val)>;
         *     if constexpr (std::is_same_v<T, int>) {
         *         printf("int: %d\n", val);
         *     }
         * }, v);
         * @endcode
         */
        /** @{ */

        /**
         * @brief Vérifie si un variant contient une valeur de type T
         * @tparam T Type à tester
         * @tparam Ts Pack de types du variant (déduit automatiquement)
         * @param variant Référence const vers le variant à inspecter
         * @return true si le type actif est T, false sinon
         * @note
         *   - Équivalent à variant.HoldsAlternative<T>() mais avec déduction de Ts...
         *   - noexcept : opération de comparaison d'index uniquement
         *   - Utile dans du code générique où le type exact du variant n'est pas connu
         */
        template <typename T, typename... Ts>
        [[nodiscard]]
        nk_bool NkHoldsAlternative(const NkVariant<Ts...>& variant) noexcept {
            return variant.template HoldsAlternative<T>();
        }

        /**
         * @brief Accès conditionnel via pointeur (version non-const)
         * @tparam T Type à rechercher
         * @tparam Ts Pack de types du variant (déduit)
         * @param variant Pointeur vers le variant à inspecter (peut être nullptr)
         * @return Pointeur vers la valeur si type actif == T, nullptr sinon
         * @note
         *   - Gère le cas variant == nullptr en retour immédiat de nullptr
         *   - Équivalent à variant->GetIf<T>() mais avec déduction de Ts...
         *   - Méthode safe : jamais de comportement indéfini
         */
        template <typename T, typename... Ts>
        T* NkGetIf(NkVariant<Ts...>* variant) noexcept {
            if (!variant) {
                return nullptr;
            }
            return variant->template GetIf<T>();
        }

        /**
         * @brief Accès conditionnel via pointeur (version const)
         * @tparam T Type à rechercher
         * @tparam Ts Pack de types du variant (déduit)
         * @param variant Pointeur const vers le variant à inspecter (peut être nullptr)
         * @return Pointeur const vers la valeur si type actif == T, nullptr sinon
         */
        template <typename T, typename... Ts>
        const T* NkGetIf(const NkVariant<Ts...>* variant) noexcept {
            if (!variant) {
                return nullptr;
            }
            return variant->template GetIf<T>();
        }

        /**
         * @brief Applique un visiteur à un variant (version non-const)
         * @tparam Visitor Type du callable visiteur (déduit)
         * @tparam Ts Pack de types du variant (déduit)
         * @param visitor Callable à invoquer avec la valeur comme argument
         * @param variant Référence vers le variant à visiter
         * @note
         *   - Forwarding parfait du visiteur pour préserver sa catégorie de valeur
         *   - Équivalent à variant.Visit(forward<Visitor>(visitor)) avec déduction de Ts...
         *   - Utile dans du code générique ou pour éviter la syntaxe membre
         */
        template <typename Visitor, typename... Ts>
        void NkVisit(Visitor&& visitor, NkVariant<Ts...>& variant) {
            variant.Visit(traits::NkForward<Visitor>(visitor));
        }

        /**
         * @brief Applique un visiteur à un variant (version const)
         * @tparam Visitor Type du callable visiteur (déduit)
         * @tparam Ts Pack de types du variant (déduit)
         * @param visitor Callable à invoquer avec la valeur const comme argument
         * @param variant Référence const vers le variant à visiter
         */
        template <typename Visitor, typename... Ts>
        void NkVisit(Visitor&& visitor, const NkVariant<Ts...>& variant) {
            variant.Visit(traits::NkForward<Visitor>(visitor));
        }

        /** @} */ // End of VariantFreeFunctions

    } // namespace nkentseu

#endif // NKENTSEU_CORE_NKVARIANT_H_INCLUDED

// =============================================================================
// EXEMPLES D'UTILISATION DE NKVARIANT.H
// =============================================================================
// Ce fichier fournit une union discriminée type-safe pour stockage polymorphique.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Variant simple pour valeurs hétérogènes
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkVariant.h"
    #include <iostream>

    // Variant pour représenter une configuration pouvant être bool, int ou string
    using ConfigValue = nkentseu::NkVariant<bool, int, nkentseu::nk_string>;

    void PrintConfig(const ConfigValue& config) {
        // Accès vérifié avec GetChecked (safe en debug)
        if (config.HoldsAlternative<bool>()) {
            std::cout << "Boolean: " << config.GetChecked<bool>() << std::endl;
        } else if (config.HoldsAlternative<int>()) {
            std::cout << "Integer: " << config.GetChecked<int>() << std::endl;
        } else if (config.HoldsAlternative<nkentseu::nk_string>()) {
            std::cout << "String: " << config.GetChecked<nkentseu::nk_string>().c_str() << std::endl;
        }
    }

    // Usage
    void Example1() {
        ConfigValue c1(true);           // Stocke un bool
        ConfigValue c2(42);             // Stocke un int
        ConfigValue c3("Hello"_nkstr);  // Stocke un string

        PrintConfig(c1);  // "Boolean: 1"
        PrintConfig(c2);  // "Integer: 42"
        PrintConfig(c3);  // "String: Hello"

        // Réassignation : change le type actif
        c1 = 100;  // c1 contient maintenant un int, plus un bool
        PrintConfig(c1);  // "Integer: 100"

        // Reset : retour à l'état vide
        c1.Reset();
        if (!c1.HasValue()) {
            std::cout << "Config is now empty" << std::endl;
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Pattern visiteur pour traitement polymorphique
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkVariant.h"
    #include <iostream>

    // Variant pour expressions arithmétiques simplifiées
    using Expr = nkentseu::NkVariant<
        double,  // Valeur littérale
        struct Add,  // Addition (déclaration forward)
        struct Multiply  // Multiplication
    >;

    struct Add {
        Expr left, right;
        Add(Expr l, Expr r) : left(std::move(l)), right(std::move(r)) {}
    };

    struct Multiply {
        Expr left, right;
        Multiply(Expr l, Expr r) : left(std::move(l)), right(std::move(r)) {}
    };

    // Visiteur pour évaluer l'expression
    struct EvalVisitor {
        double operator()(double value) const {
            return value;  // Cas de base : littéral
        }

        double operator()(const Add& op) const {
            // Récursion : évaluer les sous-expressions
            return nkentseu::NkVisit(*this, op.left) + nkentseu::NkVisit(*this, op.right);
        }

        double operator()(const Multiply& op) const {
            return nkentseu::NkVisit(*this, op.left) * nkentseu::NkVisit(*this, op.right);
        }
    };

    // Visiteur pour affichage en notation infixée
    struct PrintVisitor {
        void operator()(double value) const {
            std::cout << value;
        }

        void operator()(const Add& op) const {
            std::cout << "(";
            nkentseu::NkVisit(*this, op.left);
            std::cout << " + ";
            nkentseu::NkVisit(*this, op.right);
            std::cout << ")";
        }

        void operator()(const Multiply& op) const {
            std::cout << "(";
            nkentseu::NkVisit(*this, op.left);
            std::cout << " * ";
            nkentseu::NkVisit(*this, op.right);
            std::cout << ")";
        }
    };

    void Example2() {
        // Construire l'expression: (2.0 + 3.0) * 4.0
        Expr expr = Multiply(
            Add(2.0, 3.0),
            4.0
        );

        // Évaluation
        double result = nkentseu::NkVisit(EvalVisitor{}, expr);
        std::cout << "Result: " << result << std::endl;  // "Result: 20"

        // Affichage
        std::cout << "Expression: ";
        nkentseu::NkVisit(PrintVisitor{}, expr);
        std::cout << std::endl;  // "Expression: ((2 + 3) * 4)"
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Accès conditionnel avec GetIf pour code safe
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkVariant.h"
    #include <optional>

    // Variant pour réponse API pouvant être succès (T) ou erreur (string)
    template <typename T>
    using ApiResult = nkentseu::NkVariant<T, nkentseu::nk_string>;

    template <typename T>
    std::optional<T> ExtractSuccess(const ApiResult<T>& result) {
        // Accès safe : retourne std::nullopt si c'est une erreur
        if (const T* value = nkentseu::NkGetIf<T>(&result)) {
            return *value;
        }
        return std::nullopt;  // C'est une erreur
    }

    template <typename T>
    nkentseu::nk_string ExtractError(const ApiResult<T>& result) {
        // Accès safe à l'erreur : retourne chaîne vide si c'est un succès
        if (const nkentseu::nk_string* err = nkentseu::NkGetIf<nkentseu::nk_string>(&result)) {
            return *err;
        }
        return "No error";  // Cas de succès
    }

    void Example3() {
        ApiResult<int> success(42);
        ApiResult<int> failure("Connection timeout"_nkstr);

        // Extraction du succès
        auto val = ExtractSuccess(success);
        if (val.has_value()) {
            std::cout << "Success value: " << *val << std::endl;  // 42
        }

        // Extraction de l'erreur
        auto err = ExtractError(failure);
        std::cout << "Error: " << err.c_str() << std::endl;  // "Connection timeout"

        // Gestion complète
        auto ProcessResult = [](const ApiResult<int>& res) {
            if (auto* value = nkentseu::NkGetIf<int>(&res)) {
                return *value * 2;  // Traitement du succès
            } else if (auto* error = nkentseu::NkGetIf<nkentseu::nk_string>(&res)) {
                std::cerr << "API Error: " << error->c_str() << std::endl;
                return -1;  // Code d'erreur
            }
            return -2;  // État inattendu
        };

        std::cout << ProcessResult(success) << std::endl;  // 84
        std::cout << ProcessResult(failure) << std::endl;  // -1 (avec log d'erreur)
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Emplace pour construction in-place efficace
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkVariant.h"
    #include <string>

    // Type lourd avec constructeur coûteux
    struct ExpensiveObject {
        std::string data;
        std::vector<int> buffer;

        ExpensiveObject(const std::string& s, size_t size)
            : data(s), buffer(size, 42) {
            // Simulation de travail coûteux
        }

        // Désactiver la copie pour forcer l'usage de move/emplace
        ExpensiveObject(const ExpensiveObject&) = delete;
        ExpensiveObject& operator=(const ExpensiveObject&) = delete;

        ExpensiveObject(ExpensiveObject&&) = default;
        ExpensiveObject& operator=(ExpensiveObject&&) = default;
    };

    // Variant pouvant contenir int ou ExpensiveObject
    using DataHolder = nkentseu::NkVariant<int, ExpensiveObject>;

    void Example4() {
        // Construction directe via constructeur : copie/move potentiel
        // DataHolder h1(ExpensiveObject("test", 1000));  // Move depuis temporaire

        // Emplace : construction in-place, évite toute copie/move intermédiaire
        DataHolder h2;
        h2.Emplace<ExpensiveObject>("direct", 2000);  // Construit directement dans le storage

        // Emplace avec forwarding d'arguments multiples
        auto CreateHolder = [](const std::string& prefix, size_t count) -> DataHolder {
            DataHolder h;
            h.Emplace<ExpensiveObject>(prefix + "_data", count * 2);
            return h;  // RVO/move, pas de copie de ExpensiveObject
        };

        DataHolder h3 = CreateHolder("batch", 500);

        // Accès après emplace
        if (auto* obj = h2.GetIf<ExpensiveObject>()) {
            std::cout << "Object data: " << obj->data.c_str()
                      << ", buffer size: " << obj->buffer.size() << std::endl;
        }

        // Ré-emplace : détruit l'ancien, construit le nouveau in-place
        h2.Emplace<int>(999);  // Remplace ExpensiveObject par un int
        std::cout << "Now holds int: " << h2.GetChecked<int>() << std::endl;
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Intégration avec système d'assertions pour développement sûr
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkVariant.h"
    #include "NKCore/Assert/NkAssert.h"
    #include <iostream>

    // Variant pour état de machine avec transitions vérifiées
    using MachineState = nkentseu::NkVariant<
        struct Idle,
        struct Running,
        struct Paused,
        struct ErrorState
    >;

    struct Idle { /\* ... *\/ };
    struct Running { /\* ... *\/ };
    struct Paused { /\* ... *\/ };
    struct ErrorState { nkentseu::nk_string message; };

    class StateMachine {
    public:
        // Transition vérifiée : assertion en debug si transition invalide
        void Start() {
            NK_ASSERT(m_state.HoldsAlternative<Idle>(),
                "Can only start from Idle state");

            m_state.Emplace<Running>();
        }

        void Pause() {
            NK_ASSERT(m_state.HoldsAlternative<Running>(),
                "Can only pause from Running state");

            m_state.Emplace<Paused>();
        }

        void Resume() {
            NK_ASSERT(m_state.HoldsAlternative<Paused>(),
                "Can only resume from Paused state");

            m_state.Emplace<Running>();
        }

        void Stop() {
            // Peut être appelé depuis Running ou Paused
            NK_ASSERT(
                m_state.HoldsAlternative<Running>() ||
                m_state.HoldsAlternative<Paused>(),
                "Can only stop from Running or Paused state"
            );

            m_state.Emplace<Idle>();
        }

        void ReportError(const nkentseu::nk_string& msg) {
            // Peut être appelé depuis n'importe quel état
            m_state.Emplace<ErrorState>(msg);
        }

        // Accès vérifié au state courant (debug-safe)
        const Running& GetRunningState() const {
            return m_state.GetChecked<Running>();  // Assertion si pas Running
        }

        // Accès safe pour code qui gère l'absence
        const ErrorState* GetErrorIfAny() const {
            return m_state.GetIf<ErrorState>();  // nullptr si pas d'erreur
        }

        // Visiteur pour logging d'état
        void LogState() const {
            m_state.Visit([](const auto& state) {
                using StateType = std::decay_t<decltype(state)>;
                if constexpr (std::is_same_v<StateType, Idle>) {
                    std::cout << "[STATE] Idle" << std::endl;
                } else if constexpr (std::is_same_v<StateType, Running>) {
                    std::cout << "[STATE] Running" << std::endl;
                } else if constexpr (std::is_same_v<StateType, Paused>) {
                    std::cout << "[STATE] Paused" << std::endl;
                } else if constexpr (std::is_same_v<StateType, ErrorState>) {
                    std::cout << "[STATE] Error: " << state.message.c_str() << std::endl;
                }
            });
        }

    private:
        MachineState m_state{Idle{}};  // État initial : Idle
    };

    void Example5() {
        StateMachine machine;

        machine.LogState();  // "[STATE] Idle"
        machine.Start();     // Transition: Idle -> Running
        machine.LogState();  // "[STATE] Running"

        // En debug : si on appelle Pause() depuis Idle, NK_ASSERT échoue
        // machine.Pause();  // Assertion failure en debug si m_state != Running

        machine.Pause();     // Running -> Paused
        machine.Resume();    // Paused -> Running
        machine.Stop();      // Running -> Idle

        // Gestion d'erreur
        machine.ReportError("Disk full"_nkstr);
        machine.LogState();  // "[STATE] Error: Disk full"

        // Accès conditionnel à l'erreur
        if (const auto* err = machine.GetErrorIfAny()) {
            std::cout << "Recovering from: " << err->message.c_str() << std::endl;
            // ... logique de récupération ...
        }
    }
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================