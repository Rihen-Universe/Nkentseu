// =============================================================================
// NKContainers/Utilities/NkResult.h
// Type Result (Succès/Erreur) - compatible Rust Result / std::expected
//
// Design :
//  - Réutilisation DIRECTE de l'implémentation NKCore (ZÉRO duplication)
//  - Macros NKENTSEU_CONTAINERS_API pour l'export des symboles publics
//  - Alias de namespace pour intégration transparente dans nkentseu::containers
//  - Fallback standalone si NKCore non disponible (mode header-only isolé)
//  - Compatibilité multiplateforme via NKPlatform
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_CONTAINERS_UTILITIES_NKRESULT_H_INCLUDED
#define NKENTSEU_CONTAINERS_UTILITIES_NKRESULT_H_INCLUDED

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // NKContainers dépend de NKCore pour l'implémentation canonique de Result.
    // Nous utilisons un forwarding header avec alias de namespace.

    #include "NKContainers/NkContainersApi.h"   // NKENTSEU_CONTAINERS_API
    #include "NKContainers/NkCompat.h"          // Compatibilité C++

    // -------------------------------------------------------------------------
    // SECTION 2 : MODE D'INTÉGRATION NKCORE
    // -------------------------------------------------------------------------
    /**
     * @defgroup ContainersResultIntegration Intégration NKCore::NkResult
     * @brief Stratégie de réutilisation de l'implémentation NKCore
     *
     * Deux modes possibles :
     *  - NKENTSEU_CONTAINERS_USE_CORE_RESULT (défaut) : forwarding vers NKCore
     *  - NKENTSEU_CONTAINERS_RESULT_STANDALONE : implémentation isolée
     *
     * @note Le mode standalone est déconseillé sauf pour builds minimalistes.
     */

    #if !defined(NKENTSEU_CONTAINERS_RESULT_STANDALONE)
        #ifndef NKENTSEU_CONTAINERS_USE_CORE_RESULT
            #define NKENTSEU_CONTAINERS_USE_CORE_RESULT 1
        #endif
    #endif

    // -------------------------------------------------------------------------
    // SECTION 3 : FORWARDING VERS NKCORE (MODE RECOMMANDÉ)
    // -------------------------------------------------------------------------

    #include "NKCore/NkTypes.h"
    #include "NKCore/NkTraits.h"
    #include "NKCore/Assert/NkAssert.h"
    #include "NKMemory/NKMemory.h"

    namespace nkentseu {

        // ========================================
        // SUCCESS/ERROR TAGS
        // ========================================

        /**
         * @brief Tag pour construction d'un résultat succès
         * @tparam T Type de la valeur de succès
         * @ingroup ContainersResultTypes
         */
        template<typename T>
        struct NkSuccess {
            T value;

            explicit NkSuccess(const T& val)
                : value(val)
            {}

            explicit NkSuccess(T&& val)
                : value(traits::NkMove(val))
            {}
        };

        /**
         * @brief Tag pour construction d'un résultat erreur
         * @tparam E Type de l'erreur
         * @ingroup ContainersResultTypes
         */
        template<typename E>
        struct NkError {
            E error;

            explicit NkError(const E& err)
                : error(err)
            {}

            explicit NkError(E&& err)
                : error(traits::NkMove(err))
            {}
        };

        /**
         * @brief Helper pour créer un NkSuccess avec déduction de type
         * @tparam T Type déduit de la valeur
         * @param value Valeur à encapsuler
         * @return NkSuccess<T>
         * @ingroup ContainersResultHelpers
         */
        template<typename T>
        NkSuccess<typename traits::NkDecayT<T>> NkOk(T&& value) {
            return NkSuccess<typename traits::NkDecayT<T>>(traits::NkForward<T>(value));
        }

        /**
         * @brief Helper pour créer un NkError avec déduction de type
         * @tparam E Type déduit de l'erreur
         * @param error Erreur à encapsuler
         * @return NkError<E>
         * @ingroup ContainersResultHelpers
         */
        template<typename E>
        NkError<typename traits::NkDecayT<E>> NkErr(E&& error) {
            return NkError<typename traits::NkDecayT<E>>(traits::NkForward<E>(error));
        }

        // ========================================
        // NkResult CLASS TEMPLATE
        // ========================================

        /**
         * @brief Type Result représentant un succès (T) ou une erreur (E)
         *
         * Inspiré de :
         *  - Rust: std::result::Result<T, E>
         *  - C++23: std::expected<T, E>
         *  - Haskell: Either e a
         *
         * @tparam T Type de la valeur en cas de succès
         * @tparam E Type de l'erreur en cas d'échec
         *
         * @ingroup ContainersResultTypes
         *
         * @example
         * @code
         * using namespace nkentseu;
         *
         * NkResult<nk_int32, nk_string> Divide(nk_int32 a, nk_int32 b) {
         *     if (b == 0) return NkErr(nk_string("Division by zero"));
         *     return NkOk(a / b);
         * }
         *
         * auto result = Divide(10, 2);
         * if (result.IsOk()) {
         *     printf("Result: %d\n", result.Value());
         * } else {
         *     printf("Error: %s\n", result.GetError().c_str());
         * }
         * @endcode
         */
        template<typename T, typename E>
        class NkResult {
        public:
            // ========================================
            // TYPE ALIASES
            // ========================================

            using ValueType = T;
            using ErrorType = E;
            using ThisType  = NkResult<T, E>;

            // ========================================
            // CONSTRUCTORS
            // ========================================

            /**
             * @brief Construction depuis NkSuccess (copie)
             */
            NkResult(const NkSuccess<T>& success)
                : mHasValue(true)
            {
                NKENTSEU_CONSTRUCT_VALUE(GetValuePtr(), success.value);
            }

            /**
             * @brief Construction depuis NkSuccess (déplacement)
             */
            NkResult(NkSuccess<T>&& success)
                : mHasValue(true)
            {
                NKENTSEU_CONSTRUCT_VALUE(GetValuePtr(), traits::NkMove(success.value));
            }

            /**
             * @brief Construction depuis NkError (copie)
             */
            NkResult(const NkError<E>& error)
                : mHasValue(false)
            {
                NKENTSEU_CONSTRUCT_ERROR(GetErrorPtr(), error.error);
            }

            /**
             * @brief Construction depuis NkError (déplacement)
             */
            NkResult(NkError<E>&& error)
                : mHasValue(false)
            {
                NKENTSEU_CONSTRUCT_ERROR(GetErrorPtr(), traits::NkMove(error.error));
            }

            /**
             * @brief Copy constructor
             */
            NkResult(const ThisType& other)
                : mHasValue(other.mHasValue)
            {
                if (mHasValue) {
                    NKENTSEU_CONSTRUCT_VALUE(GetValuePtr(), *other.GetValuePtr());
                } else {
                    NKENTSEU_CONSTRUCT_ERROR(GetErrorPtr(), *other.GetErrorPtr());
                }
            }

            /**
             * @brief Move constructor
             */
            NkResult(ThisType&& other) NKENTSEU_NOEXCEPT
                : mHasValue(other.mHasValue)
            {
                if (mHasValue) {
                    NKENTSEU_CONSTRUCT_VALUE(GetValuePtr(), traits::NkMove(*other.GetValuePtr()));
                } else {
                    NKENTSEU_CONSTRUCT_ERROR(GetErrorPtr(), traits::NkMove(*other.GetErrorPtr()));
                }
            }

            /**
             * @brief Destructeur
             */
            ~NkResult() {
                Destroy();
            }

            // ========================================
            // ASSIGNMENT OPERATORS
            // ========================================

            ThisType& operator=(const ThisType& other) {
                if (this != &other) {
                    Destroy();
                    mHasValue = other.mHasValue;
                    if (mHasValue) {
                        NKENTSEU_CONSTRUCT_VALUE(GetValuePtr(), *other.GetValuePtr());
                    } else {
                        NKENTSEU_CONSTRUCT_ERROR(GetErrorPtr(), *other.GetErrorPtr());
                    }
                }
                return *this;
            }

            ThisType& operator=(ThisType&& other) NKENTSEU_NOEXCEPT {
                if (this != &other) {
                    Destroy();
                    mHasValue = other.mHasValue;
                    if (mHasValue) {
                        NKENTSEU_CONSTRUCT_VALUE(GetValuePtr(), traits::NkMove(*other.GetValuePtr()));
                    } else {
                        NKENTSEU_CONSTRUCT_ERROR(GetErrorPtr(), traits::NkMove(*other.GetErrorPtr()));
                    }
                }
                return *this;
            }

            // ========================================
            // OBSERVERS
            // ========================================

            /**
             * @brief Retourne true si le résultat est un succès
             */
            bool IsOk() const NKENTSEU_NOEXCEPT {
                return mHasValue;
            }

            /**
             * @brief Retourne true si le résultat est une erreur
             */
            bool IsErr() const NKENTSEU_NOEXCEPT {
                return !mHasValue;
            }

            /**
             * @brief Conversion explicite en bool (true = Ok)
             */
            explicit operator bool() const NKENTSEU_NOEXCEPT {
                return mHasValue;
            }

            // ========================================
            // VALUE ACCESSORS
            // ========================================

            /**
             * @brief Accès à la valeur lvalue (assert si erreur)
             */
            T& Value() & {
                NKENTSEU_CONTAINERS_ASSERT_MSG(mHasValue, "NkResult: Value() called on Error state");
                return *GetValuePtr();
            }

            /**
             * @brief Accès à la valeur const lvalue (assert si erreur)
             */
            const T& Value() const & {
                NKENTSEU_CONTAINERS_ASSERT_MSG(mHasValue, "NkResult: Value() called on Error state");
                return *GetValuePtr();
            }

            /**
             * @brief Accès à la valeur rvalue (assert si erreur)
             */
            T&& Value() && {
                NKENTSEU_CONTAINERS_ASSERT_MSG(mHasValue, "NkResult: Value() called on Error state");
                return traits::NkMove(*GetValuePtr());
            }

            /**
             * @brief Accès avec valeur par défaut (const lvalue)
             * @tparam U Type du fallback
             * @param defaultValue Valeur retournée si erreur
             */
            template<typename U>
            T ValueOr(U&& defaultValue) const & {
                return mHasValue
                    ? *GetValuePtr()
                    : static_cast<T>(traits::NkForward<U>(defaultValue));
            }

            /**
             * @brief Accès avec valeur par défaut (rvalue)
             * @tparam U Type du fallback
             * @param defaultValue Valeur retournée si erreur
             */
            template<typename U>
            T ValueOr(U&& defaultValue) && {
                return mHasValue
                    ? traits::NkMove(*GetValuePtr())
                    : static_cast<T>(traits::NkForward<U>(defaultValue));
            }

            /**
             * @brief Accès avec fonction de fallback (const lvalue)
             * @tparam F Type du callable retournant T
             * @param fallback Appelé si erreur, retourne T
             */
            template<typename F>
            T ValueOrElse(F&& fallback) const & {
                return mHasValue
                    ? *GetValuePtr()
                    : static_cast<T>(traits::NkForward<F>(fallback)());
            }

            /**
             * @brief Accès avec fonction de fallback (rvalue)
             * @tparam F Type du callable retournant T
             * @param fallback Appelé si erreur, retourne T
             */
            template<typename F>
            T ValueOrElse(F&& fallback) && {
                return mHasValue
                    ? traits::NkMove(*GetValuePtr())
                    : static_cast<T>(traits::NkForward<F>(fallback)());
            }

            /**
             * @brief Opérateur -> (assert si erreur)
             */
            T* operator->() {
                NKENTSEU_CONTAINERS_ASSERT(mHasValue);
                return GetValuePtr();
            }

            /**
             * @brief Opérateur -> const (assert si erreur)
             */
            const T* operator->() const {
                NKENTSEU_CONTAINERS_ASSERT(mHasValue);
                return GetValuePtr();
            }

            /**
             * @brief Opérateur * lvalue (assert si erreur)
             */
            T& operator*() & {
                NKENTSEU_CONTAINERS_ASSERT(mHasValue);
                return *GetValuePtr();
            }

            /**
             * @brief Opérateur * const lvalue (assert si erreur)
             */
            const T& operator*() const & {
                NKENTSEU_CONTAINERS_ASSERT(mHasValue);
                return *GetValuePtr();
            }

            // ========================================
            // ERROR ACCESSORS
            // ========================================

            /**
             * @brief Accès à l'erreur lvalue (assert si succès)
             */
            E& GetError() & {
                NKENTSEU_CONTAINERS_ASSERT_MSG(!mHasValue, "NkResult: GetError() called on Ok state");
                return *GetErrorPtr();
            }

            /**
             * @brief Accès à l'erreur const lvalue (assert si succès)
             */
            const E& GetError() const & {
                NKENTSEU_CONTAINERS_ASSERT_MSG(!mHasValue, "NkResult: GetError() called on Ok state");
                return *GetErrorPtr();
            }

            /**
             * @brief Accès à l'erreur rvalue (assert si succès)
             */
            E&& GetError() && {
                NKENTSEU_CONTAINERS_ASSERT_MSG(!mHasValue, "NkResult: GetError() called on Ok state");
                return traits::NkMove(*GetErrorPtr());
            }

            /**
             * @brief Accès à l'erreur avec valeur par défaut (const lvalue)
             * @tparam U Type du fallback
             * @param defaultValue Valeur retournée si succès
             */
            template<typename U>
            E ErrorOr(U&& defaultValue) const & {
                return !mHasValue
                    ? *GetErrorPtr()
                    : static_cast<E>(traits::NkForward<U>(defaultValue));
            }

            // ========================================
            // MONADIC OPERATIONS (Style Rust)
            // ========================================
            //
            // CORRECTION CLANG :
            // Les méthodes GetValuePtr() et GetErrorPtr() sont privées.
            // Les appeler dans les trailing return types (decltype(...)) provoque
            // une erreur sur Clang car le contexte 'this' n'est pas encore établi
            // lors de l'évaluation du type de retour.
            //
            // Solution : utiliser traits::NkDeclVal<T&>(), NkDeclVal<const T&>()
            // et NkDeclVal<T&&>() dans les decltype des trailing return types.
            // Ces expressions produisent le même type sans appeler de méthode membre.
            // Le corps de la fonction peut continuer à appeler GetValuePtr() normalement.

            /**
             * @brief Map : transforme la valeur si Ok, propage l'erreur sinon (lvalue)
             *
             * @tparam F Fonction de transformation T& -> U
             * @param func Fonction à appliquer sur la valeur
             * @return NkResult<U, E>
             *
             * @note Utilise NkDeclVal<T&>() dans le trailing return type
             *       pour éviter l'appel à GetValuePtr() hors contexte this.
             */
            template<typename F>
            auto Map(F&& func) &
                -> NkResult<decltype(func(traits::NkDeclVal<T&>())), E>
            {
                using NewT = decltype(func(traits::NkDeclVal<T&>()));
                if (mHasValue) {
                    return NkOk<NewT>(func(*GetValuePtr()));
                }
                return NkErr<E>(*GetErrorPtr());
            }

            /**
             * @brief Map : transforme la valeur si Ok, propage l'erreur sinon (const lvalue)
             *
             * @tparam F Fonction de transformation const T& -> U
             * @param func Fonction à appliquer sur la valeur
             * @return NkResult<U, E>
             *
             * @note Utilise NkDeclVal<const T&>() dans le trailing return type.
             */
            template<typename F>
            auto Map(F&& func) const &
                -> NkResult<decltype(func(traits::NkDeclVal<const T&>())), E>
            {
                using NewT = decltype(func(traits::NkDeclVal<const T&>()));
                if (mHasValue) {
                    return NkOk<NewT>(func(*GetValuePtr()));
                }
                return NkErr<E>(*GetErrorPtr());
            }

            /**
             * @brief Map : transforme la valeur si Ok, propage l'erreur sinon (rvalue)
             *
             * @tparam F Fonction de transformation T&& -> U
             * @param func Fonction à appliquer sur la valeur
             * @return NkResult<U, E>
             *
             * @note Utilise NkDeclVal<T&&>() dans le trailing return type.
             */
            template<typename F>
            auto Map(F&& func) &&
                -> NkResult<decltype(func(traits::NkDeclVal<T&&>())), E>
            {
                using NewT = decltype(func(traits::NkDeclVal<T&&>()));
                if (mHasValue) {
                    return NkOk<NewT>(func(traits::NkMove(*GetValuePtr())));
                }
                return NkErr<E>(traits::NkMove(*GetErrorPtr()));
            }

            /**
             * @brief MapError : transforme l'erreur si Err, propage la valeur sinon (lvalue)
             *
             * @tparam F Fonction de transformation E& -> NewE
             * @param func Fonction à appliquer sur l'erreur
             * @return NkResult<T, NewE>
             *
             * @note Utilise NkDeclVal<E&>() dans le trailing return type.
             */
            template<typename F>
            auto MapError(F&& func) &
                -> NkResult<T, decltype(func(traits::NkDeclVal<E&>()))>
            {
                using NewE = decltype(func(traits::NkDeclVal<E&>()));
                if (!mHasValue) {
                    return NkErr<NewE>(func(*GetErrorPtr()));
                }
                return NkOk<T>(*GetValuePtr());
            }

            /**
             * @brief MapError : transforme l'erreur si Err, propage la valeur sinon (const lvalue)
             *
             * @tparam F Fonction de transformation const E& -> NewE
             * @param func Fonction à appliquer sur l'erreur
             * @return NkResult<T, NewE>
             *
             * @note Utilise NkDeclVal<const E&>() dans le trailing return type.
             */
            template<typename F>
            auto MapError(F&& func) const &
                -> NkResult<T, decltype(func(traits::NkDeclVal<const E&>()))>
            {
                using NewE = decltype(func(traits::NkDeclVal<const E&>()));
                if (!mHasValue) {
                    return NkErr<NewE>(func(*GetErrorPtr()));
                }
                return NkOk<T>(*GetValuePtr());
            }

            /**
             * @brief AndThen : chaîne des opérations, exécute func si Ok (lvalue)
             *
             * @tparam F Fonction T& -> NkResult<U, E>
             * @param func Fonction à appliquer sur la valeur
             * @return NkResult<U, E> (type déduit de func)
             *
             * @note Utilise NkDeclVal<T&>() dans le trailing return type.
             */
            template<typename F>
            auto AndThen(F&& func) &
                -> decltype(func(traits::NkDeclVal<T&>()))
            {
                if (mHasValue) {
                    return func(*GetValuePtr());
                }
                return NkErr<E>(*GetErrorPtr());
            }

            /**
             * @brief AndThen : chaîne des opérations, exécute func si Ok (const lvalue)
             *
             * @tparam F Fonction const T& -> NkResult<U, E>
             * @param func Fonction à appliquer sur la valeur
             * @return NkResult<U, E> (type déduit de func)
             *
             * @note Utilise NkDeclVal<const T&>() dans le trailing return type.
             */
            template<typename F>
            auto AndThen(F&& func) const &
                -> decltype(func(traits::NkDeclVal<const T&>()))
            {
                if (mHasValue) {
                    return func(*GetValuePtr());
                }
                return NkErr<E>(*GetErrorPtr());
            }

            /**
             * @brief AndThen : chaîne des opérations, exécute func si Ok (rvalue)
             *
             * @tparam F Fonction T&& -> NkResult<U, E>
             * @param func Fonction à appliquer sur la valeur
             * @return NkResult<U, E> (type déduit de func)
             *
             * @note Utilise NkDeclVal<T&&>() dans le trailing return type.
             */
            template<typename F>
            auto AndThen(F&& func) &&
                -> decltype(func(traits::NkDeclVal<T&&>()))
            {
                if (mHasValue) {
                    return func(traits::NkMove(*GetValuePtr()));
                }
                return NkErr<E>(traits::NkMove(*GetErrorPtr()));
            }

            /**
             * @brief OrElse : gestion d'erreur, exécute func si Err (lvalue)
             *
             * @tparam F Fonction E& -> NkResult<T, NewE>
             * @param func Fonction à appliquer sur l'erreur
             * @return NkResult<T, NewE> (type déduit de func)
             *
             * @note Utilise NkDeclVal<E&>() dans le trailing return type.
             */
            template<typename F>
            auto OrElse(F&& func) &
                -> decltype(func(traits::NkDeclVal<E&>()))
            {
                if (!mHasValue) {
                    return func(*GetErrorPtr());
                }
                return NkOk<T>(*GetValuePtr());
            }

            /**
             * @brief OrElse : gestion d'erreur, exécute func si Err (const lvalue)
             *
             * @tparam F Fonction const E& -> NkResult<T, NewE>
             * @param func Fonction à appliquer sur l'erreur
             * @return NkResult<T, NewE> (type déduit de func)
             *
             * @note Utilise NkDeclVal<const E&>() dans le trailing return type.
             */
            template<typename F>
            auto OrElse(F&& func) const &
                -> decltype(func(traits::NkDeclVal<const E&>()))
            {
                if (!mHasValue) {
                    return func(*GetErrorPtr());
                }
                return NkOk<T>(*GetValuePtr());
            }

            // ========================================
            // CONSUMPTION OPERATIONS
            // ========================================

            /**
             * @brief Unwrap : extrait la valeur, panic si erreur
             */
            T Unwrap() && {
                NKENTSEU_CONTAINERS_ASSERT_MSG(mHasValue, "NkResult::Unwrap() called on Error");
                return traits::NkMove(*GetValuePtr());
            }

            /**
             * @brief Expect : extrait la valeur, panic avec message si erreur
             * @param message Message d'erreur affiché lors du panic
             */
            T Expect(const nk_char* message) && {
                NKENTSEU_CONTAINERS_ASSERT_MSG(mHasValue, message);
                return traits::NkMove(*GetValuePtr());
            }

            /**
             * @brief UnwrapError : extrait l'erreur, panic si succès
             */
            E UnwrapError() && {
                NKENTSEU_CONTAINERS_ASSERT_MSG(!mHasValue, "NkResult::UnwrapError() called on Ok");
                return traits::NkMove(*GetErrorPtr());
            }

            /**
             * @brief ExpectError : extrait l'erreur, panic avec message si succès
             * @param message Message d'erreur affiché lors du panic
             */
            E ExpectError(const nk_char* message) && {
                NKENTSEU_CONTAINERS_ASSERT_MSG(!mHasValue, message);
                return traits::NkMove(*GetErrorPtr());
            }

            /**
             * @brief UnwrapOr : retourne la valeur ou un fallback (const lvalue)
             * @tparam U Type du fallback
             * @param fallback Valeur retournée si erreur
             */
            template<typename U>
            T UnwrapOr(U&& fallback) const & {
                return mHasValue
                    ? *GetValuePtr()
                    : static_cast<T>(traits::NkForward<U>(fallback));
            }

            /**
             * @brief UnwrapOrElse : retourne la valeur ou le résultat d'un callback (const lvalue)
             * @tparam F Type du callable retournant T
             * @param fallback Appelé si erreur, retourne T
             */
            template<typename F>
            T UnwrapOrElse(F&& fallback) const & {
                return mHasValue
                    ? *GetValuePtr()
                    : static_cast<T>(traits::NkForward<F>(fallback)());
            }

        private:
            // ========================================
            // STORAGE INTERN
            // ========================================

            // Indicateur d'état : true = valeur Ok, false = erreur
            bool mHasValue;

            // Union alignée pour stockage type-safe de T ou E sans allocation
            union Storage {
                // Stockage brut aligné pour le type valeur T
                alignas(T) unsigned char value[sizeof(T)];
                // Stockage brut aligné pour le type erreur E
                alignas(E) unsigned char error[sizeof(E)];

                // Constructeur et destructeur triviaux (les objets sont construits manuellement)
                Storage()  {}
                ~Storage() {}
            } mStorage;

            // ========================================
            // HELPERS D'ACCÈS TYPE-SAFE (PRIVÉS)
            // ========================================
            //
            // Ces méthodes sont UNIQUEMENT appelées dans le corps des fonctions membres,
            // jamais dans les trailing return types (voir correction Clang plus haut).

            /**
             * @brief Retourne un pointeur non-const vers la valeur stockée
             */
            T* GetValuePtr() NKENTSEU_NOEXCEPT {
                return reinterpret_cast<T*>(&mStorage.value[0]);
            }

            /**
             * @brief Retourne un pointeur const vers la valeur stockée
             */
            const T* GetValuePtr() const NKENTSEU_NOEXCEPT {
                return reinterpret_cast<const T*>(&mStorage.value[0]);
            }

            /**
             * @brief Retourne un pointeur non-const vers l'erreur stockée
             */
            E* GetErrorPtr() NKENTSEU_NOEXCEPT {
                return reinterpret_cast<E*>(&mStorage.error[0]);
            }

            /**
             * @brief Retourne un pointeur const vers l'erreur stockée
             */
            const E* GetErrorPtr() const NKENTSEU_NOEXCEPT {
                return reinterpret_cast<const E*>(&mStorage.error[0]);
            }

            /**
             * @brief Détruit l'objet actif (valeur ou erreur) via le destructeur approprié
             */
            void Destroy() {
                if (mHasValue) {
                    NKENTSEU_DESTROY_VALUE(GetValuePtr());
                } else {
                    NKENTSEU_DESTROY_ERROR(GetErrorPtr());
                }
            }

            // ========================================
            // MACROS INTERNES DE CONSTRUCTION/DESTRUCTION
            // ========================================
            //
            // Ces macros encapsulent le placement new et les appels de destructeur
            // pour la construction et destruction des objets dans le stockage union.

            // Construit T dans le stockage via placement new
            #define NKENTSEU_CONSTRUCT_VALUE(ptr, ...) \
                ::new(static_cast<void*>(ptr)) T(__VA_ARGS__)

            // Construit E dans le stockage via placement new
            #define NKENTSEU_CONSTRUCT_ERROR(ptr, ...) \
                ::new(static_cast<void*>(ptr)) E(__VA_ARGS__)

            // Détruit T en appelant explicitement son destructeur
            #define NKENTSEU_DESTROY_VALUE(ptr) \
                (ptr)->~T()

            // Détruit E en appelant explicitement son destructeur
            #define NKENTSEU_DESTROY_ERROR(ptr) \
                (ptr)->~E()
        };

        // ========================================
        // SIMPLE ERROR TYPE
        // ========================================

        /**
         * @brief Type d'erreur simple avec message et code
         * @ingroup ContainersResultTypes
         */
        struct NKENTSEU_CONTAINERS_CLASS_EXPORT NkSimpleError {
            const nk_char* message;
            nk_int32       code;

            NkSimpleError()
                : message("Unknown error")
                , code(-1)
            {}

            explicit NkSimpleError(const nk_char* msg, nk_int32 c = -1)
                : message(msg)
                , code(c)
            {}

            bool operator==(const NkSimpleError& other) const {
                return code == other.code;
            }
        };

        /**
         * @brief Alias pratique pour Result avec NkSimpleError
         * @tparam T Type de la valeur de succès
         * @ingroup ContainersResultAliases
         */
        template<typename T>
        using NkSimpleResult = NkResult<T, NkSimpleError>;

    } // namespace nkentseu

    // -------------------------------------------------------------------------
    // SECTION 5 : MACROS DE CONFORT ET UTILITAIRES
    // -------------------------------------------------------------------------
    /**
     * @brief Macro pour retour rapide en cas d'erreur (style ? operator Rust)
     * @def NKENTSEU_CONTAINERS_TRY
     * @ingroup ContainersResultMacros
     *
     * Propage l'erreur si le Result est en état Err.
     *
     * @example
     * @code
     * NkResult<int, string> ParseAndCompute(const string& input) {
     *     auto value = NKENTSEU_CONTAINERS_TRY(ParseInt(input));
     *     auto result = NKENTSEU_CONTAINERS_TRY(Compute(value));
     *     return NkOk(result * 2);
     * }
     * @endcode
     */
    #define NKENTSEU_CONTAINERS_TRY(expr)                                               \
        do {                                                                             \
            auto&& _nk_try_result = (expr);                                             \
            if (_nk_try_result.IsErr()) {                                               \
                return NkErr<decltype(_nk_try_result.GetError())>(                      \
                    traits::NkMove(_nk_try_result.GetError()));                          \
            }                                                                            \
        } while(0);                                                                      \
        auto&& NKENTSEU_CONCAT(_nk_try_value, __LINE__) =                              \
            traits::NkMove(_nk_try_result.Value())

    /**
     * @brief Version simplifiée de TRY pour les retours directs
     * @def NKENTSEU_CONTAINERS_TRY_RET
     * @ingroup ContainersResultMacros
     */
    #define NKENTSEU_CONTAINERS_TRY_RET(expr)                                           \
        do {                                                                             \
            auto&& _nk_try_result = (expr);                                             \
            if (_nk_try_result.IsErr()) {                                               \
                return NkErr<decltype(_nk_try_result.GetError())>(                      \
                    traits::NkMove(_nk_try_result.GetError()));                          \
            }                                                                            \
            return NkOk<decltype(_nk_try_result.Value())>(                              \
                traits::NkMove(_nk_try_result.Value()));                                 \
        } while(0)

    // -------------------------------------------------------------------------
    // SECTION 6 : VALIDATION DE CONFIGURATION
    // -------------------------------------------------------------------------
    #if defined(NKENTSEU_CONTAINERS_USE_CORE_RESULT) && defined(NKENTSEU_CONTAINERS_RESULT_STANDALONE)
        #error "NKContainers: Ne pas définir à la fois NKENTSEU_CONTAINERS_USE_CORE_RESULT et NKENTSEU_CONTAINERS_RESULT_STANDALONE"
    #endif

    // -------------------------------------------------------------------------
    // SECTION 7 : MESSAGES DE DEBUG OPTIONNELS
    // -------------------------------------------------------------------------
    #ifdef NKENTSEU_CONTAINERS_DEBUG_CONFIG
        #if defined(NKENTSEU_CONTAINERS_USE_CORE_RESULT)
            #pragma message("NKContainers NkResult: Using NKCore implementation (forwarding)")
        #elif defined(NKENTSEU_CONTAINERS_RESULT_STANDALONE)
            #pragma message("NKContainers NkResult: Using standalone implementation")
        #endif
    #endif

#endif // NKENTSEU_CONTAINERS_UTILITIES_NKRESULT_H_INCLUDED

// =============================================================================
// EXEMPLES D'UTILISATION
// =============================================================================
/*
    // Exemple 1 : Usage basique avec forwarding vers NKCore (mode par défaut)
    #include <NKContainers/Utilities/NkResult.h>

    using namespace nkentseu;

    NkResult<nk_int32, nk_string> SafeDivide(nk_int32 a, nk_int32 b) {
        if (b == 0) {
            return NkErr(nk_string("Division by zero"));
        }
        return NkOk(a / b);
    }

    void Example() {
        auto result = SafeDivide(10, 2);

        // Pattern 1 : Check explicite
        if (result.IsOk()) {
            printf("Result: %d\n", result.Value());
        } else {
            printf("Error: %s\n", result.GetError().c_str());
        }

        // Pattern 2 : ValueOr avec fallback
        nk_int32 value = result.ValueOr(0);

        // Pattern 3 : Conversion booléenne
        if (result) {
            printf("Success: %d\n", *result);  // operator*
        }
    }

    // Exemple 2 : Chaînage monadique (style Rust)
    NkResult<nk_string, nk_string> ParseConfig(const nk_string& path) {
        return NkOk(path)
            .Map([](const nk_string& p) { return LoadFile(p); })
            .AndThen([](const nk_string& content) { return ParseJson(content); })
            .MapError([](const nk_string& err) {
                return nk_string("Config error: ") + err;
            });
    }

    // Exemple 3 : Macro TRY pour propagation d'erreur
    NkResult<User, nk_string> LoadUser(nk_uint64 id) {
        auto db    = NKENTSEU_CONTAINERS_TRY(ConnectDatabase());
        auto query = NKENTSEU_CONTAINERS_TRY(db.Prepare("SELECT * FROM users WHERE id = ?"));
        return query.Execute(id);
    }

    // Exemple 4 : Mode standalone (sans dépendance NKCore)
    // Définir avant inclusion :
    // #define NKENTSEU_CONTAINERS_RESULT_STANDALONE
    // #include <NKContainers/Utilities/NkResult.h>
*/

// =============================================================================
// NOTES DE MIGRATION
// =============================================================================
/*
    Pour migrer depuis l'ancienne API NKCore::NkResult vers NKContainers :

    Ancien code                          | Nouveau code
    -------------------------------------|----------------------------------
    nkentseu::NkResult<T, E>            | nkentseu::NkResult<T, E>
    nkentseu::NkOk(value)               | nkentseu::NkOk(value)
    nkentseu::NkErr(error)              | nkentseu::NkErr(error)
    result.Unwrap()                     | result.Unwrap() (identique)

    // Avec namespace using :
    using namespace nkentseu;
    // Permet d'utiliser NkResult, NkOk, NkErr directement
*/

// =============================================================================
// NOTES TECHNIQUES — CORRECTION CLANG (TRAILING RETURN TYPES)
// =============================================================================
/*
    PROBLÈME :
    Les méthodes Map, MapError, AndThen, OrElse utilisaient GetValuePtr() et
    GetErrorPtr() dans leurs trailing return types :

        auto Map(F&& func) & -> NkResult<decltype(func(*GetValuePtr())), E>

    Sous Clang, cela génère l'erreur :
        "use of undeclared identifier 'GetValuePtr'"

    Car les trailing return types sont évalués AVANT l'établissement du contexte
    'this', rendant les méthodes membres privées inaccessibles à ce stade.
    GCC et MSVC sont plus permissifs sur ce point.

    SOLUTION :
    Remplacer GetValuePtr() par traits::NkDeclVal<T&>() dans les trailing return
    types uniquement. NkDeclVal<T&>() produit exactement le même type (T&) que
    *GetValuePtr() sans avoir besoin d'appeler une méthode membre :

        auto Map(F&& func) & -> NkResult<decltype(func(traits::NkDeclVal<T&>())), E>

    Le corps de la fonction continue d'utiliser GetValuePtr() normalement.

    CORRESPONDANCE DES QUALIFICATIONS :
        lvalue     :  GetValuePtr()  →  NkDeclVal<T&>()
        const lvalue : GetValuePtr() (const) → NkDeclVal<const T&>()
        rvalue     :  NkMove(*GetValuePtr()) → NkDeclVal<T&&>()
        error lvalue : GetErrorPtr() → NkDeclVal<E&>()
        error const  : GetErrorPtr() (const) → NkDeclVal<const E&>()
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================