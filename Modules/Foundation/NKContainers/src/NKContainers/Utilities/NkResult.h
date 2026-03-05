// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\Utilities\NkResult.h
// DESCRIPTION: Result type (Success/Error) - like Rust Result or std::expected
// AUTEUR: Rihen
// DATE: 2026-02-07
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NKENTSEU_CORE_NKCORE_SRC_NKCORE_UTILITIES_NKRESULT_H_INCLUDED
#define NKENTSEU_CORE_NKCORE_SRC_NKCORE_UTILITIES_NKRESULT_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKCore/NkTraits.h"
#include "NKMemory/NkMemoryFn.h"
#include "NKCore/Assert/NkAssert.h"

namespace nkentseu {
    namespace core {
        
        // ========================================
        // SUCCESS/ERROR TAGS
        // ========================================
        
        /**
         * @brief Tag pour construction Success
         */
        template<typename T>
        struct NkSuccess {
            T value;
            
            explicit NkSuccess(const T& val) : value(val) {}
            explicit NkSuccess(T&& val) : value(traits::Move(val)) {}
        };
        
        /**
         * @brief Tag pour construction Error
         */
        template<typename E>
        struct NkError {
            E error;
            
            explicit NkError(const E& err) : error(err) {}
            explicit NkError(E&& err) : error(traits::Move(err)) {}
        };
        
        // Helper functions
        template<typename T>
        NkSuccess<typename traits::NkDecayT<T>> NkOk(T&& value) {
            return NkSuccess<typename traits::NkDecayT<T>>(traits::NkForward<T>(value));
        }
        
        template<typename E>
        NkError<typename traits::NkDecayT<E>> NkErr(E&& error) {
            return NkError<typename traits::NkDecayT<E>>(traits::NkForward<E>(error));
        }
        
        // ========================================
        // RESULT
        // ========================================
        
        /**
         * @brief Result type (Success ou Error)
         * 
         * Similaire à:
         * - Rust: Result<T, E>
         * - C++23: std::expected<T, E>
         * 
         * Usage:
         * @code
         * Result<nk_int32, Error> Divide(nk_int32 a, nk_int32 b) {
         *     if (b == 0) return Err("Division by zero");
         *     return Ok(a / b);
         * }
         * 
         * auto result = Divide(10, 2);
         * if (result.IsOk()) {
         *     printf("Result: %d\n", result.Value());
         * } else {
         *     printf("Error: %s\n", result.Error().message);
         * }
         * @endcode
         * 
         * @tparam T Type du succès
         * @tparam E Type de l'erreur
         */
        template<typename T, typename E>
        class NkResult {
        public:
            using ValueType = T;
            using ErrorType = E;
            
            // ========================================
            // CONSTRUCTORS
            // ========================================
            
            /**
             * @brief Constructeur depuis Success
             */
            NkResult(const NkSuccess<T>& success)
                : mHasValue(true)
            {
                memory::NkConstruct(GetValuePtr(), success.value);
            }
            
            NkResult(NkSuccess<T>&& success)
                : mHasValue(true)
            {
                memory::NkConstruct(GetValuePtr(), traits::NkMove(success.value));
            }
            
            /**
             * @brief Constructeur depuis Error
             */
            NkResult(const NkError<E>& error)
                : mHasValue(false)
            {
                memory::NkConstruct(GetErrorPtr(), error.error);
            }
            
            NkResult(NkError<E>&& error)
                : mHasValue(false)
            {
                memory::NkConstruct(GetErrorPtr(), traits::NkMove(error.error));
            }
            
            /**
             * @brief Copy constructor
             */
            NkResult(const NkResult& other)
                : mHasValue(other.mHasValue)
            {
                if (mHasValue) {
                    memory::NkConstruct(GetValuePtr(), *other.GetValuePtr());
                } else {
                    memory::NkConstruct(GetErrorPtr(), *other.GetErrorPtr());
                }
            }
            
            /**
             * @brief Move constructor
             */
            NkResult(NkResult&& other) NKENTSEU_NOEXCEPT
                : mHasValue(other.mHasValue)
            {
                if (mHasValue) {
                    memory::NkConstruct(GetValuePtr(), traits::NkMove(*other.GetValuePtr()));
                } else {
                    memory::NkConstruct(GetErrorPtr(), traits::NkMove(*other.GetErrorPtr()));
                }
            }
            
            /**
             * @brief Destructeur
             */
            ~NkResult() {
                Destroy();
            }
            
            // ========================================
            // ASSIGNMENT
            // ========================================
            
            NkResult& operator=(const NkResult& other) {
                if (this != &other) {
                    Destroy();
                    mHasValue = other.mHasValue;
                    if (mHasValue) {
                        memory::NkConstruct(GetValuePtr(), *other.GetValuePtr());
                    } else {
                        memory::NkConstruct(GetErrorPtr(), *other.GetErrorPtr());
                    }
                }
                return *this;
            }
            
            NkResult& operator=(NkResult&& other) NKENTSEU_NOEXCEPT {
                if (this != &other) {
                    Destroy();
                    mHasValue = other.mHasValue;
                    if (mHasValue) {
                        memory::NkConstruct(GetValuePtr(), traits::NkMove(*other.GetValuePtr()));
                    } else {
                        memory::NkConstruct(GetErrorPtr(), traits::NkMove(*other.GetErrorPtr()));
                    }
                }
                return *this;
            }
            
            // ========================================
            // OBSERVERS
            // ========================================
            
            /**
             * @brief Vérifie si succès
             */
            bool IsOk() const NKENTSEU_NOEXCEPT {
                return mHasValue;
            }
            
            /**
             * @brief Vérifie si erreur
             */
            bool IsErr() const NKENTSEU_NOEXCEPT {
                return !mHasValue;
            }
            
            explicit operator bool() const NKENTSEU_NOEXCEPT {
                return mHasValue;
            }
            
            // ========================================
            // VALUE ACCESS
            // ========================================
            
            /**
             * @brief Accès à la valeur (assert si erreur)
             */
            T& Value() & {
                NKENTSEU_ASSERT_MSG(mHasValue, "Result is Error");
                return *GetValuePtr();
            }
            
            const T& Value() const & {
                NKENTSEU_ASSERT_MSG(mHasValue, "Result is Error");
                return *GetValuePtr();
            }
            
            T&& Value() && {
                NKENTSEU_ASSERT_MSG(mHasValue, "Result is Error");
                return traits::Move(*GetValuePtr());
            }
            
            /**
             * @brief Accès avec valeur par défaut
             */
            template<typename U>
            T ValueOr(U&& defaultValue) const & {
                return mHasValue ? *GetValuePtr() : static_cast<T>(traits::Forward<U>(defaultValue));
            }
            
            template<typename U>
            T ValueOr(U&& defaultValue) && {
                return mHasValue ? traits::Move(*GetValuePtr()) : static_cast<T>(traits::Forward<U>(defaultValue));
            }
            
            /**
             * @brief Déréférencement (comme Optional)
             */
            T* operator->() {
                NKENTSEU_ASSERT(mHasValue);
                return GetValuePtr();
            }
            
            const T* operator->() const {
                NKENTSEU_ASSERT(mHasValue);
                return GetValuePtr();
            }
            
            T& operator*() & {
                NKENTSEU_ASSERT(mHasValue);
                return *GetValuePtr();
            }
            
            const T& operator*() const & {
                NKENTSEU_ASSERT(mHasValue);
                return *GetValuePtr();
            }
            
            // ========================================
            // ERROR ACCESS
            // ========================================
            
            /**
             * @brief Accès à l'erreur (assert si succès)
             */
            E& GetError() & {
                NKENTSEU_ASSERT_MSG(!mHasValue, "Result is Ok");
                return *GetErrorPtr();
            }
            
            const E& GetError() const & {
                NKENTSEU_ASSERT_MSG(!mHasValue, "Result is Ok");
                return *GetErrorPtr();
            }
            
            E&& GetError() && {
                NKENTSEU_ASSERT_MSG(!mHasValue, "Result is Ok");
                return traits::Move(*GetErrorPtr());
            }
            
            // ========================================
            // MONADIC OPERATIONS (Rust-like)
            // ========================================
            
            /**
             * @brief Map (transforme la valeur si Ok)
             */
            template<typename F>
            auto Map(F&& func) -> Result<decltype(func(*GetValuePtr())), E> {
                using NewT = decltype(func(*GetValuePtr()));
                
                if (mHasValue) {
                    return Ok(func(*GetValuePtr()));
                }
                return Err(*GetErrorPtr());
            }
            
            /**
             * @brief MapError (transforme l'erreur si Err)
             */
            template<typename F>
            auto MapError(F&& func) -> Result<T, decltype(func(*GetErrorPtr()))> {
                using NewE = decltype(func(*GetErrorPtr()));
                
                if (!mHasValue) {
                    return Err(func(*GetErrorPtr()));
                }
                return Ok(*GetValuePtr());
            }
            
            /**
             * @brief AndThen (chain operations)
             */
            template<typename F>
            auto AndThen(F&& func) -> decltype(func(*GetValuePtr())) {
                if (mHasValue) {
                    return func(*GetValuePtr());
                }
                return Err(*GetErrorPtr());
            }
            
            /**
             * @brief OrElse (handle error)
             */
            template<typename F>
            auto OrElse(F&& func) -> decltype(func(*GetErrorPtr())) {
                if (!mHasValue) {
                    return func(*GetErrorPtr());
                }
                return Ok(*GetValuePtr());
            }
            
            /**
             * @brief Unwrap (panic si erreur)
             */
            T Unwrap() && {
                NKENTSEU_ASSERT_MSG(mHasValue, "Called Unwrap on Error Result");
                return traits::Move(*GetValuePtr());
            }
            
            /**
             * @brief Expect (panic avec message si erreur)
             */
            T Expect(const nk_char* message) && {
                NKENTSEU_ASSERT_MSG(mHasValue, message);
                return traits::Move(*GetValuePtr());
            }
            
        private:
            bool mHasValue;
            
            // Storage aligné
            union {
                typename NKENTSEU_ALIGN(alignof(T)) unsigned nk_char mValueStorage[sizeof(T)];
                typename NKENTSEU_ALIGN(alignof(E)) unsigned nk_char mErrorStorage[sizeof(E)];
            };
            
            T* GetValuePtr() NKENTSEU_NOEXCEPT {
                return reinterpret_cast<T*>(&mValueStorage[0]);
            }
            
            const T* GetValuePtr() const NKENTSEU_NOEXCEPT {
                return reinterpret_cast<const T*>(&mValueStorage[0]);
            }
            
            E* GetErrorPtr() NKENTSEU_NOEXCEPT {
                return reinterpret_cast<E*>(&mErrorStorage[0]);
            }
            
            const E* GetErrorPtr() const NKENTSEU_NOEXCEPT {
                return reinterpret_cast<const E*>(&mErrorStorage[0]);
            }
            
            void Destroy() {
                if (mHasValue) {
                    memory::NkDestroy(GetValuePtr());
                } else {
                    memory::NkDestroy(GetErrorPtr());
                }
            }
        };
        
        // ========================================
        // SIMPLE ERROR TYPE
        // ========================================
        
        /**
         * @brief Type d'erreur simple (string)
         */
        struct NkSimpleError {
            const nk_char* message;
            nk_int32 code;
            
            NkSimpleError() : message("Unknown error"), code(-1) {}
            explicit NkSimpleError(const nk_char* msg, nk_int32 c = -1) : message(msg), code(c) {}
        };
        
        /**
         * @brief Alias pratique pour Result avec SimpleError
         */
        template<typename T>
        using NkSimpleResult = NkResult<T, NkSimpleError>;
        
    } // namespace core
} // namespace nkentseu

#endif // NKENTSEU_CORE_NKCORE_SRC_NKCORE_UTILITIES_NKRESULT_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-07
// Creation Date: 2026-02-07
// ============================================================