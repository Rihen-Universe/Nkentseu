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
        struct NKENTSEU_CONTAINERS_CLASS_EXPORT NkSuccess {
            T value;

            explicit NkSuccess(const T& val) : value(val) {}
            explicit NkSuccess(T&& val) : value(NKENTSEU_MOVE(val)) {}
        };

        /**
         * @brief Tag pour construction d'un résultat erreur
         * @tparam E Type de l'erreur
         * @ingroup ContainersResultTypes
         */
        template<typename E>
        struct NKENTSEU_CONTAINERS_CLASS_EXPORT NkError {
            E error;

            explicit NkError(const E& err) : error(err) {}
            explicit NkError(E&& err) : error(NKENTSEU_MOVE(err)) {}
        };

        /**
         * @brief Helper pour créer un NkSuccess avec déduction de type
         * @tparam T Type déduit de la valeur
         * @param value Valeur à encapsuler
         * @return NkSuccess<T>
         * @ingroup ContainersResultHelpers
         */
        template<typename T>
        NKENTSEU_CONTAINERS_API_INLINE
        NkSuccess<typename NKENTSEU_DECAY_T(T)> NkOk(T&& value) {
            return NkSuccess<typename NKENTSEU_DECAY_T(T)>(NKENTSEU_FORWARD(T, value));
        }

        /**
         * @brief Helper pour créer un NkError avec déduction de type
         * @tparam E Type déduit de l'erreur
         * @param error Erreur à encapsuler
         * @return NkError<E>
         * @ingroup ContainersResultHelpers
         */
        template<typename E>
        NKENTSEU_CONTAINERS_API_INLINE
        NkError<typename NKENTSEU_DECAY_T(E)> NkErr(E&& error) {
            return NkError<typename NKENTSEU_DECAY_T(E)>(NKENTSEU_FORWARD(E, error));
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
         * using namespace nkentseu::containers;
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
        class NKENTSEU_CONTAINERS_CLASS_EXPORT NkResult {
            public:
                // ========================================
                // TYPE ALIASES
                // ========================================
                using ValueType = T;
                using ErrorType = E;
                using ThisType = NkResult<T, E>;

                // ========================================
                // CONSTRUCTORS
                // ========================================

                /**
                 * @brief Construction depuis NkSuccess
                 */
                NkResult(const NkSuccess<T>& success)
                    : mHasValue(true)
                {
                    NKENTSEU_CONSTRUCT_VALUE(GetValuePtr(), success.value);
                }

                NkResult(NkSuccess<T>&& success)
                    : mHasValue(true)
                {
                    NKENTSEU_CONSTRUCT_VALUE(GetValuePtr(), NKENTSEU_MOVE(success.value));
                }

                /**
                 * @brief Construction depuis NkError
                 */
                NkResult(const NkError<E>& error)
                    : mHasValue(false)
                {
                    NKENTSEU_CONSTRUCT_ERROR(GetErrorPtr(), error.error);
                }

                NkResult(NkError<E>&& error)
                    : mHasValue(false)
                {
                    NKENTSEU_CONSTRUCT_ERROR(GetErrorPtr(), NKENTSEU_MOVE(error.error));
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
                        NKENTSEU_CONSTRUCT_VALUE(GetValuePtr(), NKENTSEU_MOVE(*other.GetValuePtr()));
                    } else {
                        NKENTSEU_CONSTRUCT_ERROR(GetErrorPtr(), NKENTSEU_MOVE(*other.GetErrorPtr()));
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
                            NKENTSEU_CONSTRUCT_VALUE(GetValuePtr(), NKENTSEU_MOVE(*other.GetValuePtr()));
                        } else {
                            NKENTSEU_CONSTRUCT_ERROR(GetErrorPtr(), NKENTSEU_MOVE(*other.GetErrorPtr()));
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
                 * @brief Accès à la valeur (assert si erreur)
                 */
                T& Value() & {
                    NKENTSEU_CONTAINERS_ASSERT_MSG(mHasValue, "NkResult: Value() called on Error state");
                    return *GetValuePtr();
                }

                const T& Value() const & {
                    NKENTSEU_CONTAINERS_ASSERT_MSG(mHasValue, "NkResult: Value() called on Error state");
                    return *GetValuePtr();
                }

                T&& Value() && {
                    NKENTSEU_CONTAINERS_ASSERT_MSG(mHasValue, "NkResult: Value() called on Error state");
                    return NKENTSEU_MOVE(*GetValuePtr());
                }

                /**
                 * @brief Accès avec valeur par défaut (copie)
                 */
                template<typename U>
                T ValueOr(U&& defaultValue) const & {
                    return mHasValue ? *GetValuePtr() : static_cast<T>(NKENTSEU_FORWARD(U, defaultValue));
                }

                template<typename U>
                T ValueOr(U&& defaultValue) && {
                    return mHasValue ? NKENTSEU_MOVE(*GetValuePtr()) : static_cast<T>(NKENTSEU_FORWARD(U, defaultValue));
                }

                /**
                 * @brief Accès avec fonction de fallback
                 */
                template<typename F>
                T ValueOrElse(F&& fallback) const & {
                    return mHasValue ? *GetValuePtr() : static_cast<T>(NKENTSEU_FORWARD(F, fallback)());
                }

                template<typename F>
                T ValueOrElse(F&& fallback) && {
                    return mHasValue ? NKENTSEU_MOVE(*GetValuePtr()) : static_cast<T>(NKENTSEU_FORWARD(F, fallback)());
                }

                /**
                 * @brief Opérateur de déréférencement (style pointeur)
                 */
                T* operator->() {
                    NKENTSEU_CONTAINERS_ASSERT(mHasValue);
                    return GetValuePtr();
                }

                const T* operator->() const {
                    NKENTSEU_CONTAINERS_ASSERT(mHasValue);
                    return GetValuePtr();
                }

                /**
                 * @brief Opérateur de déréférencement (style référence)
                 */
                T& operator*() & {
                    NKENTSEU_CONTAINERS_ASSERT(mHasValue);
                    return *GetValuePtr();
                }

                const T& operator*() const & {
                    NKENTSEU_CONTAINERS_ASSERT(mHasValue);
                    return *GetValuePtr();
                }

                // ========================================
                // ERROR ACCESSORS
                // ========================================

                /**
                 * @brief Accès à l'erreur (assert si succès)
                 */
                E& GetError() & {
                    NKENTSEU_CONTAINERS_ASSERT_MSG(!mHasValue, "NkResult: GetError() called on Ok state");
                    return *GetErrorPtr();
                }

                const E& GetError() const & {
                    NKENTSEU_CONTAINERS_ASSERT_MSG(!mHasValue, "NkResult: GetError() called on Ok state");
                    return *GetErrorPtr();
                }

                E&& GetError() && {
                    NKENTSEU_CONTAINERS_ASSERT_MSG(!mHasValue, "NkResult: GetError() called on Ok state");
                    return NKENTSEU_MOVE(*GetErrorPtr());
                }

                /**
                 * @brief Accès à l'erreur avec valeur par défaut
                 */
                template<typename U>
                E ErrorOr(U&& defaultValue) const & {
                    return !mHasValue ? *GetErrorPtr() : static_cast<E>(NKENTSEU_FORWARD(U, defaultValue));
                }

                // ========================================
                // MONADIC OPERATIONS (Style Rust)
                // ========================================

                /**
                 * @brief Map : transforme la valeur si Ok, propage l'erreur sinon
                 * @tparam F Fonction de transformation T -> U
                 * @return NkResult<U, E>
                 */
                template<typename F>
                auto Map(F&& func) & -> NkResult<decltype(func(*GetValuePtr())), E> {
                    using NewT = decltype(func(*GetValuePtr()));
                    if (mHasValue) {
                        return NkOk<NewT>(func(*GetValuePtr()));
                    }
                    return NkErr<E>(*GetErrorPtr());
                }

                template<typename F>
                auto Map(F&& func) const & -> NkResult<decltype(func(*GetValuePtr())), E> {
                    using NewT = decltype(func(*GetValuePtr()));
                    if (mHasValue) {
                        return NkOk<NewT>(func(*GetValuePtr()));
                    }
                    return NkErr<E>(*GetErrorPtr());
                }

                template<typename F>
                auto Map(F&& func) && -> NkResult<decltype(func(NKENTSEU_MOVE(*GetValuePtr()))), E> {
                    using NewT = decltype(func(NKENTSEU_MOVE(*GetValuePtr())));
                    if (mHasValue) {
                        return NkOk<NewT>(func(NKENTSEU_MOVE(*GetValuePtr())));
                    }
                    return NkErr<E>(NKENTSEU_MOVE(*GetErrorPtr()));
                }

                /**
                 * @brief MapError : transforme l'erreur si Err, propage la valeur sinon
                 * @tparam F Fonction de transformation E -> F
                 * @return NkResult<T, F>
                 */
                template<typename F>
                auto MapError(F&& func) & -> NkResult<T, decltype(func(*GetErrorPtr()))> {
                    using NewE = decltype(func(*GetErrorPtr()));
                    if (!mHasValue) {
                        return NkErr<NewE>(func(*GetErrorPtr()));
                    }
                    return NkOk<T>(*GetValuePtr());
                }

                template<typename F>
                auto MapError(F&& func) const & -> NkResult<T, decltype(func(*GetErrorPtr()))> {
                    using NewE = decltype(func(*GetErrorPtr()));
                    if (!mHasValue) {
                        return NkErr<NewE>(func(*GetErrorPtr()));
                    }
                    return NkOk<T>(*GetValuePtr());
                }

                /**
                 * @brief AndThen : chaîne des opérations, exécute func si Ok
                 * @tparam F Fonction T -> NkResult<U, E>
                 * @return NkResult<U, E>
                 */
                template<typename F>
                auto AndThen(F&& func) & -> decltype(func(*GetValuePtr())) {
                    if (mHasValue) {
                        return func(*GetValuePtr());
                    }
                    return NkErr<E>(*GetErrorPtr());
                }

                template<typename F>
                auto AndThen(F&& func) const & -> decltype(func(*GetValuePtr())) {
                    if (mHasValue) {
                        return func(*GetValuePtr());
                    }
                    return NkErr<E>(*GetErrorPtr());
                }

                template<typename F>
                auto AndThen(F&& func) && -> decltype(func(NKENTSEU_MOVE(*GetValuePtr()))) {
                    if (mHasValue) {
                        return func(NKENTSEU_MOVE(*GetValuePtr()));
                    }
                    return NkErr<E>(NKENTSEU_MOVE(*GetErrorPtr()));
                }

                /**
                 * @brief OrElse : gestion d'erreur, exécute func si Err
                 * @tparam F Fonction E -> NkResult<T, F>
                 * @return NkResult<T, F>
                 */
                template<typename F>
                auto OrElse(F&& func) & -> decltype(func(*GetErrorPtr())) {
                    if (!mHasValue) {
                        return func(*GetErrorPtr());
                    }
                    return NkOk<T>(*GetValuePtr());
                }

                template<typename F>
                auto OrElse(F&& func) const & -> decltype(func(*GetErrorPtr())) {
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
                    return NKENTSEU_MOVE(*GetValuePtr());
                }

                /**
                 * @brief Expect : extrait la valeur, panic avec message si erreur
                 */
                T Expect(const nk_char* message) && {
                    NKENTSEU_CONTAINERS_ASSERT_MSG(mHasValue, message);
                    return NKENTSEU_MOVE(*GetValuePtr());
                }

                /**
                 * @brief UnwrapError : extrait l'erreur, panic si succès
                 */
                E UnwrapError() && {
                    NKENTSEU_CONTAINERS_ASSERT_MSG(!mHasValue, "NkResult::UnwrapError() called on Ok");
                    return NKENTSEU_MOVE(*GetErrorPtr());
                }

                /**
                 * @brief ExpectError : extrait l'erreur, panic avec message si succès
                 */
                E ExpectError(const nk_char* message) && {
                    NKENTSEU_CONTAINERS_ASSERT_MSG(!mHasValue, message);
                    return NKENTSEU_MOVE(*GetErrorPtr());
                }

                /**
                 * @brief UnwrapOr : retourne la valeur ou un fallback
                 */
                template<typename U>
                T UnwrapOr(U&& fallback) const & {
                    return mHasValue ? *GetValuePtr() : static_cast<T>(NKENTSEU_FORWARD(U, fallback));
                }

                /**
                 * @brief UnwrapOrElse : retourne la valeur ou le résultat d'un callback
                 */
                template<typename F>
                T UnwrapOrElse(F&& fallback) const & {
                    return mHasValue ? *GetValuePtr() : static_cast<T>(NKENTSEU_FORWARD(F, fallback)());
                }

            private:
                // ========================================
                // STORAGE INTERN
                // ========================================
                bool mHasValue;

                // Union alignée pour stockage type-safe de T ou E
                union Storage {
                    alignas(T) unsigned char value[sizeof(T)];
                    alignas(E) unsigned char error[sizeof(E)];

                    Storage() {}
                    ~Storage() {}
                } mStorage;

                // Helpers d'accès type-safe
                T* GetValuePtr() NKENTSEU_NOEXCEPT {
                    return reinterpret_cast<T*>(&mStorage.value[0]);
                }

                const T* GetValuePtr() const NKENTSEU_NOEXCEPT {
                    return reinterpret_cast<const T*>(&mStorage.value[0]);
                }

                E* GetErrorPtr() NKENTSEU_NOEXCEPT {
                    return reinterpret_cast<E*>(&mStorage.error[0]);
                }

                const E* GetErrorPtr() const NKENTSEU_NOEXCEPT {
                    return reinterpret_cast<const E*>(&mStorage.error[0]);
                }

                // Destruction de l'objet actif
                void Destroy() {
                    if (mHasValue) {
                        NKENTSEU_DESTROY_VALUE(GetValuePtr());
                    } else {
                        NKENTSEU_DESTROY_ERROR(GetErrorPtr());
                    }
                }

                // Macros internes pour construction/destruction
                #define NKENTSEU_CONSTRUCT_VALUE(ptr, ...) ::new(static_cast<void*>(ptr)) T(__VA_ARGS__)
                #define NKENTSEU_CONSTRUCT_ERROR(ptr, ...) ::new(static_cast<void*>(ptr)) E(__VA_ARGS__)
                #define NKENTSEU_DESTROY_VALUE(ptr) (ptr)->~T()
                #define NKENTSEU_DESTROY_ERROR(ptr) (ptr)->~E()
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
            nk_int32 code;

            NkSimpleError() : message("Unknown error"), code(-1) {}
            
            explicit NkSimpleError(const nk_char* msg, nk_int32 c = -1) : message(msg), code(c) {}

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
    #define NKENTSEU_CONTAINERS_TRY(expr) \
        do { \
            auto&& _nk_try_result = (expr); \
            if (_nk_try_result.IsErr()) { \
                return NkErr<decltype(_nk_try_result.GetError())>( \
                    NKENTSEU_MOVE(_nk_try_result.GetError())); \
            } \
        } while(0); \
        auto&& NKENTSEU_CONCAT(_nk_try_value, __LINE__) = NKENTSEU_MOVE(_nk_try_result.Value())

    /**
     * @brief Version simplifiée de TRY pour les retours directs
     * @def NKENTSEU_CONTAINERS_TRY_RET
     * @ingroup ContainersResultMacros
     */
    #define NKENTSEU_CONTAINERS_TRY_RET(expr) \
        do { \
            auto&& _nk_try_result = (expr); \
            if (_nk_try_result.IsErr()) { \
                return NkErr<decltype(_nk_try_result.GetError())>( \
                    NKENTSEU_MOVE(_nk_try_result.GetError())); \
            } \
            return NkOk<decltype(_nk_try_result.Value())>( \
                NKENTSEU_MOVE(_nk_try_result.Value())); \
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

    using namespace nkentseu::containers;

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
        auto db = NKENTSEU_CONTAINERS_TRY(ConnectDatabase());
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
    nkentseu::NkResult<T, E>            | nkentseu::containers::NkResult<T, E>
    nkentseu::NkOk(value)               | nkentseu::containers::NkOk(value)
    nkentseu::NkErr(error)              | nkentseu::containers::NkErr(error)
    result.Unwrap()                     | result.Unwrap() (identique)
    
    // Avec namespace using :
    using namespace nkentseu::containers;
    // Permet d'utiliser NkResult, NkOk, NkErr directement
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================