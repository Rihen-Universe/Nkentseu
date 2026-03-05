// ============================================================
// FILE: NkFuture.h
// DESCRIPTION: Future/Promise pattern (production-ready)
// AUTHOR: Rihen
// DATE: 2026-03-05
// VERSION: 2.0.0
// ============================================================
// Pattern async/await permettant de:
// - Enqueue du travail sur thread pool
// - Obtenir le résultat plus tard
// - Attendre que le résultat soit prêt
// ============================================================

#pragma once

#ifndef NKENTSEU_THREADING_NKFUTURE_H_INCLUDED
#define NKENTSEU_THREADING_NKFUTURE_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKThreading/NkThreadingExport.h"
#include "NKThreading/NkMutex.h"
#include "NKThreading/NkConditionVariable.h"

#include <functional>
#include <memory>
#include <chrono>

namespace nkentseu {
namespace threading {

    // Forward declaration
    template<typename T>
    class NkPromise;
    
    /**
     * @brief Future - Résultat asynchrone
     * 
     * Représente un résultat qui sera disponible dans le futur.
     * Permet d'attendre le résultat ou d'en connaître la disponibilité.
     * 
     * @example
     * ```cpp
     * NkFuture<int> future = pool.Async([]() {
     *     return 42;
     * });
     * 
     * if (future.IsReady()) {
     *     int result = future.Get();
     * } else {
     *     int result = future.WaitFor(100);
     * }
     * ```
     */
    template<typename T = void>
    class NKTHREADING_API NkFuture {
    public:
        using ValueType = T;
        
        NkFuture() noexcept = default;
        NkFuture(const NkFuture&) = default;
        NkFuture& operator=(const NkFuture&) = default;
        NkFuture(NkFuture&&) noexcept = default;
        NkFuture& operator=(NkFuture&&) noexcept = default;
        ~NkFuture() = default;
        
        /**
         * @brief Récupère le résultat (attend si pas prêt)
         */
        [[nodiscard]] const T& Get() const noexcept;
        
        /**
         * @brief Vérifie si le résultat est prêt
         */
        [[nodiscard]] nk_bool IsReady() const noexcept;
        
        /**
         * @brief Attend que le résultat soit prêt
         */
        void Wait() const noexcept;
        
        /**
         * @brief Attend avec timeout
         */
        [[nodiscard]] nk_bool WaitFor(nk_uint32 milliseconds) const noexcept;
        
    private:
        class State;
        std::shared_ptr<State> mState;
        friend class NkPromise<T>;
    };
    
    /**
     * @brief Promise - Producteur du résultat
     */
    template<typename T>
    class NKTHREADING_API NkPromise {
    public:
        NkPromise() noexcept;
        
        NkPromise(const NkPromise&) = delete;
        NkPromise& operator=(const NkPromise&) = delete;
        NkPromise(NkPromise&&) noexcept = default;
        NkPromise& operator=(NkPromise&&) noexcept = default;
        ~NkPromise() = default;
        
        [[nodiscard]] NkFuture<T> GetFuture() const noexcept;
        
        void SetValue(const T& value) noexcept;
        void SetValue(T&& value) noexcept;
        void SetException(std::exception_ptr ex) noexcept;
        
    private:
        std::shared_ptr<typename NkFuture<T>::State> mState;
    };
    
    // Specialization for void
    template<>
    class NKTHREADING_API NkFuture<void> {
    public:
        NkFuture() noexcept = default;
        nk_bool IsReady() const noexcept;
        void Wait() const noexcept;
        nk_bool WaitFor(nk_uint32 milliseconds) const noexcept;
        
    private:
        class State;
        std::shared_ptr<State> mState;
        friend class NkPromise<void>;
    };

} // namespace threading
} // namespace nkentseu

#endif // NKENTSEU_THREADING_NKFUTURE_H_INCLUDED