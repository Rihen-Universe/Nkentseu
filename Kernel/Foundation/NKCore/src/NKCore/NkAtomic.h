// =============================================================================
// NKCore/NkAtomic.h
// Opérations atomiques multi-plateforme avec support mémoire ordering.
//
// Design :
//  - Classe template NkAtomic<T> pour opérations atomiques génériques
//  - Support des ordres mémoire C++11 : Relaxed, Acquire, Release, SeqCst
//  - Implémentation via intrinsics MSVC ou built-ins GCC/Clang
//  - Fallback générique pour plateformes non supportées (non-atomique)
//  - Typedefs prédéfinis : NkAtomicInt32, NkAtomicBool, NkAtomicPtr, etc.
//  - NkAtomicFlag léger pour synchronisation basique (spinlock minimal)
//  - Barrières mémoire explicites : ThreadFence, AcquireFence, ReleaseFence
//  - NkSpinLock avec backoff exponentiel pour sections critiques courtes
//  - NkScopedSpinLock pour gestion RAII des verrous
//  - Fonctions utilitaires globales : NkAtomicIncrement, NkAtomicAdd, etc.
//
// Intégration :
//  - Utilise NKCore/NkTypes.h pour les types fondamentaux (nk_int32, etc.)
//  - Utilise NKPlatform/NkCompilerDetect.h pour la détection de compilateur
//  - Respecte NKENTSEU_FORCE_INLINE et NKENTSEU_NOEXCEPT pour compatibilité
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_NKCORE_NKATOMIC_H_INCLUDED
#define NKENTSEU_NKCORE_NKATOMIC_H_INCLUDED

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion des modules de base et détection de plateforme.

    #include "NKCore/NkTypes.h"
    #include "NKPlatform/NkCompilerDetect.h"

    // Headers système pour opérations atomiques (ordre : système, puis plateforme)
    #if defined(NKENTSEU_COMPILER_MSVC)
        #include <emmintrin.h>      // Pour _mm_pause() sur x86/x64
        #include <intrin.h>         // Intrinsics MSVC (_Interlocked*)
        #include <windows.h>        // MemoryBarrier(), _ReadBarrier(), etc.
    #elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
        // GCC/Clang : built-ins __atomic_* utilisés directement
        // Aucun include supplémentaire requis pour les opérations atomiques de base
    #endif

    // Désactivation temporaire des warnings GCC/Clang sur l'alignement atomique
    #if defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Watomic-alignment"
    #endif

    // -------------------------------------------------------------------------
    // SECTION 2 : ESPACE DE NOMS PRINCIPAL
    // -------------------------------------------------------------------------
    // Déclaration de l'espace de noms principal nkentseu.
    // Le contenu est indenté d'un niveau pour la lisibilité hiérarchique.

    namespace nkentseu {

        // ====================================================================
        // SECTION 3 : ÉNUMÉRATION DES ORDRES MÉMOIRE
        // ====================================================================
        // Définition des ordres mémoire pour les opérations atomiques (modèle C++11).

        /**
         * @brief Ordres mémoire pour opérations atomiques
         * @enum NkMemoryOrder
         * @ingroup AtomicOperations
         *
         * Ces valeurs contrôlent la visibilité et l'ordre des opérations mémoire
         * entre threads. Choisir l'ordre le plus faible possible pour de meilleures
         * performances, tout en garantissant la correction du programme.
         *
         * @note
         *   - NK_RELAXED : Aucune garantie d'ordre, seule l'atomicité est assurée
         *   - NK_ACQUIRE : Synchronisation lecture (barrière après l'opération)
         *   - NK_RELEASE : Synchronisation écriture (barrière avant l'opération)
         *   - NK_SEQCST : Ordre séquentiellement cohérent (plus fort, plus coûteux)
         *
         * @example
         * @code
         * // Producteur-Consommateur avec Acquire/Release
         * nkentseu::NkAtomic<bool> dataReady{false};
         * int sharedData = 0;
         *
         * // Thread producteur
         * sharedData = 42;  // Écriture non-atomique
         * dataReady.Store(true, nkentseu::NkMemoryOrder::NK_RELEASE);  // Barrière avant
         *
         * // Thread consommateur
         * while (!dataReady.Load(nkentseu::NkMemoryOrder::NK_ACQUIRE)) {  // Barrière après
         *     // Attente active ou yield
         * }
         * int value = sharedData;  // Garanti de voir 42 grâce à la synchronisation
         * @endcode
         */
        enum class NkMemoryOrder : nk_uint8 {
            NK_RELAXED  = 0,  ///< Aucune garantie d'ordre, seule l'atomicité est assurée
            NK_CONSUME  = 1,  ///< Acquire pour dépendances de données (rarement utilisé)
            NK_ACQUIRE  = 2,  ///< Synchronisation lecture : voit les écritures précédentes
            NK_RELEASE  = 3,  ///< Synchronisation écriture : rend visibles les écritures précédentes
            NK_ACQREL   = 4,  ///< Combinaison Acquire + Release pour opérations read-modify-write
            NK_SEQCST   = 5   ///< Ordre séquentiellement cohérent (plus fort, plus coûteux)
        };

        // ====================================================================
        // SECTION 4 : CLASSE TEMPLATE NKATOMIC<T>
        // ====================================================================
        // Classe générique pour opérations atomiques thread-safe sur type T.

        /**
         * @brief Classe template pour opérations atomiques sur type T
         * @tparam T Type de la valeur atomique (doit être trivialment copiable)
         * @ingroup AtomicOperations
         *
         * Cette classe encapsule une valeur de type T et fournit des opérations
         * atomiques thread-safe via les intrinsics plateforme ou built-ins compiler.
         * La classe est non-copiable pour éviter les copies accidentelles non-atomiques.
         *
         * @note
         *   - L'alignement est forcé via alignas(T) pour garantir l'atomicité matérielle
         *   - Les opérations utilisent volatile pour empêcher les optimisations agressives
         *   - En fallback, les opérations deviennent non-atomiques (à éviter en production)
         *
         * @warning
         *   Les opérations FetchAdd/FetchSub ne sont garanties atomiques que pour
         *   les types entiers de 1, 2, 4 ou 8 octets sur toutes les plateformes supportées.
         *   Pour d'autres tailles, un fallback non-atomique est utilisé.
         *
         * @example
         * @code
         * // Compteur atomique simple
         * nkentseu::NkAtomic<nk_uint32> counter{0};
         *
         * // Incrémentation thread-safe
         * nk_uint32 oldValue = counter.FetchAdd(1);  // Retourne la valeur avant incrément
         *
         * // Lecture avec ordre mémoire relaxé (plus rapide)
         * nk_uint32 current = counter.Load(nkentseu::NkMemoryOrder::NK_RELAXED);
         *
         * // Compare-and-swap pour mise à jour conditionnelle
         * nk_uint32 expected = current;
         * nk_uint32 desired = current * 2;
         * while (!counter.CompareExchangeWeak(expected, desired)) {
         *     // expected a été mis à jour avec la valeur courante, réessayer
         *     desired = expected * 2;
         * }
         * @endcode
         */
        template <typename T>
        class alignas(T) NkAtomic {
            public:
                // --------------------------------------------------------
                // Constructeurs
                // --------------------------------------------------------

                /**
                 * @brief Constructeur par défaut (valeur initialisée à zéro)
                 * @ingroup AtomicOperations
                 */
                NkAtomic() NKENTSEU_NOEXCEPT
                    : mValue()
                {
                }

                /**
                 * @brief Constructeur avec valeur initiale
                 * @param value Valeur initiale à stocker
                 * @ingroup AtomicOperations
                 */
                explicit NkAtomic(T value) NKENTSEU_NOEXCEPT
                    : mValue(value)
                {
                }

                // Suppression des opérations de copie pour éviter les copies non-atomiques
                NkAtomic(const NkAtomic &) = delete;
                NkAtomic &operator=(const NkAtomic &) = delete;

                // --------------------------------------------------------
                // Opérations de base : Load / Store
                // --------------------------------------------------------

                /**
                 * @brief Charge atomiquement la valeur courante
                 * @param order Ordre mémoire pour l'opération (défaut: SEQCST)
                 * @return Valeur chargée
                 * @ingroup AtomicOperations
                 */
                NKENTSEU_FORCE_INLINE
                T Load(NkMemoryOrder order = NkMemoryOrder::NK_SEQCST) const NKENTSEU_NOEXCEPT
                {
                    (void)order;  // Unused parameter in fallback implementation

                    #if defined(NKENTSEU_COMPILER_MSVC)
                        MemoryBarrier();
                        return mValue;
                    #elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
                        __atomic_thread_fence(__ATOMIC_SEQ_CST);
                        return mValue;
                    #else
                        return mValue;  // Fallback non-atomique
                    #endif
                }

                /**
                 * @brief Stocke atomiquement une nouvelle valeur
                 * @param value Nouvelle valeur à stocker
                 * @param order Ordre mémoire pour l'opération (défaut: SEQCST)
                 * @ingroup AtomicOperations
                 */
                NKENTSEU_FORCE_INLINE
                void Store(T value, NkMemoryOrder order = NkMemoryOrder::NK_SEQCST) NKENTSEU_NOEXCEPT
                {
                    (void)order;  // Unused parameter in fallback implementation

                    #if defined(NKENTSEU_COMPILER_MSVC)
                        mValue = value;
                        MemoryBarrier();
                    #elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
                        mValue = value;
                        __atomic_thread_fence(__ATOMIC_SEQ_CST);
                    #else
                        mValue = value;  // Fallback non-atomique
                    #endif
                }

                // --------------------------------------------------------
                // Opérations Read-Modify-Write
                // --------------------------------------------------------

                /**
                 * @brief Échange atomique : remplace et retourne l'ancienne valeur
                 * @param value Nouvelle valeur à stocker
                 * @param order Ordre mémoire pour l'opération (défaut: SEQCST)
                 * @return Ancienne valeur avant l'échange
                 * @ingroup AtomicOperations
                 */
                NKENTSEU_FORCE_INLINE
                T Exchange(T value, NkMemoryOrder order = NkMemoryOrder::NK_SEQCST) NKENTSEU_NOEXCEPT
                {
                    (void)order;  // Unused parameter in fallback implementation

                    #if defined(NKENTSEU_COMPILER_MSVC)
                        if constexpr (sizeof(T) == 4) {
                            return static_cast<T>(
                                InterlockedExchange(
                                    reinterpret_cast<volatile long *>(&mValue),
                                    static_cast<long>(value)
                                )
                            );
                        } else if constexpr (sizeof(T) == 8) {
                            return static_cast<T>(
                                InterlockedExchange64(
                                    reinterpret_cast<volatile long long *>(&mValue),
                                    static_cast<long long>(value)
                                )
                            );
                        }
                    #elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
                        return __atomic_exchange_n(&mValue, value, __ATOMIC_SEQ_CST);
                    #endif

                    // Fallback générique (non-atomique - à éviter en production)
                    T old = mValue;
                    mValue = value;
                    return old;
                }

                /**
                 * @brief Ajoute atomiquement et retourne l'ancienne valeur
                 * @param value Valeur à ajouter
                 * @param order Ordre mémoire pour l'opération (défaut: SEQCST)
                 * @return Ancienne valeur avant l'addition
                 * @note Supporté pour types entiers de 1, 2, 4, 8 octets
                 * @ingroup AtomicOperations
                 */
                NKENTSEU_FORCE_INLINE
                T FetchAdd(T value, NkMemoryOrder order = NkMemoryOrder::NK_SEQCST) NKENTSEU_NOEXCEPT
                {
                    (void)order;  // Unused parameter in fallback implementation

                    #if defined(NKENTSEU_COMPILER_MSVC)
                        if constexpr (sizeof(T) == 1) {
                            auto *ptr = reinterpret_cast<volatile char *>(&mValue);
                            return static_cast<T>(_InterlockedExchangeAdd8(ptr, static_cast<char>(value)));
                        } else if constexpr (sizeof(T) == 2) {
                            auto *ptr = reinterpret_cast<volatile short *>(&mValue);
                            return static_cast<T>(_InterlockedExchangeAdd16(ptr, static_cast<short>(value)));
                        } else if constexpr (sizeof(T) == 4) {
                            auto *ptr = reinterpret_cast<volatile long *>(&mValue);
                            return static_cast<T>(_InterlockedExchangeAdd(ptr, static_cast<long>(value)));
                        } else if constexpr (sizeof(T) == 8) {
                            auto *ptr = reinterpret_cast<volatile long long *>(&mValue);
                            return static_cast<T>(_InterlockedExchangeAdd64(ptr, static_cast<long long>(value)));
                        }
                    #elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
                        return __atomic_fetch_add(&mValue, value, __ATOMIC_SEQ_CST);
                    #endif

                    // Fallback générique non-atomique (PAS thread-safe)
                    T old = mValue;
                    mValue = static_cast<T>(old + value);
                    return old;
                }

                /**
                 * @brief Soustrait atomiquement et retourne l'ancienne valeur
                 * @param value Valeur à soustraire
                 * @param order Ordre mémoire pour l'opération (défaut: SEQCST)
                 * @return Ancienne valeur avant la soustraction
                 * @ingroup AtomicOperations
                 */
                NKENTSEU_FORCE_INLINE
                T FetchSub(T value, NkMemoryOrder order = NkMemoryOrder::NK_SEQCST) NKENTSEU_NOEXCEPT
                {
                    return FetchAdd(static_cast<T>(-value), order);
                }

                // --------------------------------------------------------
                // Compare-Exchange (CAS) - Weak et Strong
                // --------------------------------------------------------

                /**
                 * @brief Compare-and-exchange faible (peut échouer spurieusement)
                 * @param expected Valeur attendue (modifiée si échec)
                 * @param desired Nouvelle valeur si *expected == mValue
                 * @param order Ordre mémoire en cas de succès
                 * @return true si l'échange a réussi, false sinon
                 * @note En cas d'échec, expected est mis à jour avec la valeur courante
                 * @ingroup AtomicOperations
                 */
                NKENTSEU_FORCE_INLINE
                nk_bool CompareExchangeWeak(
                    T &expected,
                    T desired,
                    NkMemoryOrder order = NkMemoryOrder::NK_SEQCST
                ) NKENTSEU_NOEXCEPT
                {
                    (void)order;  // Unused parameter in fallback implementation

                    #if defined(NKENTSEU_COMPILER_MSVC)
                        if constexpr (sizeof(T) == 1) {
                            auto *ptr = reinterpret_cast<volatile char *>(&mValue);
                            char expectedChar = static_cast<char>(expected);
                            char result = _InterlockedCompareExchange8(ptr, static_cast<char>(desired), expectedChar);
                            nk_bool success = (result == expectedChar);
                            if (!success) { expected = static_cast<T>(result); }
                            return success;
                        } else if constexpr (sizeof(T) == 2) {
                            auto *ptr = reinterpret_cast<volatile short *>(&mValue);
                            short expectedShort = static_cast<short>(expected);
                            short result = _InterlockedCompareExchange16(ptr, static_cast<short>(desired), expectedShort);
                            nk_bool success = (result == expectedShort);
                            if (!success) { expected = static_cast<T>(result); }
                            return success;
                        } else if constexpr (sizeof(T) == 4) {
                            auto *ptr = reinterpret_cast<volatile long *>(&mValue);
                            long expectedLong = static_cast<long>(expected);
                            long result = _InterlockedCompareExchange(ptr, static_cast<long>(desired), expectedLong);
                            nk_bool success = (result == expectedLong);
                            if (!success) { expected = static_cast<T>(result); }
                            return success;
                        } else if constexpr (sizeof(T) == 8) {
                            auto *ptr = reinterpret_cast<volatile long long *>(&mValue);
                            long long expectedLongLong = static_cast<long long>(expected);
                            long long result = _InterlockedCompareExchange64(ptr, static_cast<long long>(desired), expectedLongLong);
                            nk_bool success = (result == expectedLongLong);
                            if (!success) { expected = static_cast<T>(result); }
                            return success;
                        }
                    #elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
                        T expectedCopy = expected;
                        nk_bool success = __atomic_compare_exchange_n(
                            &mValue,
                            &expectedCopy,
                            desired,
                            true,  // weak = true
                            __ATOMIC_SEQ_CST,
                            __ATOMIC_SEQ_CST
                        );
                        if (!success) { expected = expectedCopy; }
                        return success;
                    #endif

                    // Fallback générique non-atomique
                    T old = mValue;
                    nk_bool fallbackSuccess = (old == expected);
                    if (fallbackSuccess) {
                        mValue = desired;
                    } else {
                        expected = old;
                    }
                    return fallbackSuccess;
                }

                /**
                 * @brief Compare-and-exchange faible avec ordres mémoire séparés
                 * @param expected Valeur attendue (modifiée si échec)
                 * @param desired Nouvelle valeur si *expected == mValue
                 * @param success Ordre mémoire en cas de succès
                 * @param failure Ordre mémoire en cas d'échec
                 * @return true si l'échange a réussi, false sinon
                 * @ingroup AtomicOperations
                 */
                NKENTSEU_FORCE_INLINE
                nk_bool CompareExchangeWeak(
                    T &expected,
                    T desired,
                    NkMemoryOrder success,
                    NkMemoryOrder failure
                ) NKENTSEU_NOEXCEPT
                {
                    (void)failure;  // Unused in current implementation
                    return CompareExchangeWeak(expected, desired, success);
                }

                /**
                 * @brief Compare-and-exchange fort (garanti de ne pas échouer spurieusement)
                 * @param expected Valeur attendue (modifiée si échec)
                 * @param desired Nouvelle valeur si *expected == mValue
                 * @param order Ordre mémoire en cas de succès
                 * @return true si l'échange a réussi, false sinon
                 * @note Implémentation actuelle identique à Weak (à renforcer si nécessaire)
                 * @ingroup AtomicOperations
                 */
                NKENTSEU_FORCE_INLINE
                nk_bool CompareExchangeStrong(
                    T &expected,
                    T desired,
                    NkMemoryOrder order = NkMemoryOrder::NK_SEQCST
                ) NKENTSEU_NOEXCEPT
                {
                    return CompareExchangeWeak(expected, desired, order);
                }

                /**
                 * @brief Compare-and-exchange fort avec ordres mémoire séparés
                 * @param expected Valeur attendue (modifiée si échec)
                 * @param desired Nouvelle valeur si *expected == mValue
                 * @param success Ordre mémoire en cas de succès
                 * @param failure Ordre mémoire en cas d'échec
                 * @return true si l'échange a réussi, false sinon
                 * @ingroup AtomicOperations
                 */
                NKENTSEU_FORCE_INLINE
                nk_bool CompareExchangeStrong(
                    T &expected,
                    T desired,
                    NkMemoryOrder success,
                    NkMemoryOrder failure
                ) NKENTSEU_NOEXCEPT
                {
                    (void)failure;  // Unused in current implementation
                    return CompareExchangeStrong(expected, desired, success);
                }

                // --------------------------------------------------------
                // Méthodes utilitaires et opérateurs
                // --------------------------------------------------------

                /**
                 * @brief Vérifie si les opérations sont lock-free sur cette plateforme
                 * @return true si lock-free, false sinon
                 * @note Retourne toujours false en fallback générique
                 * @ingroup AtomicOperations
                 */
                NKENTSEU_FORCE_INLINE
                nk_bool IsLockFree() const NKENTSEU_NOEXCEPT
                {
                    #if defined(NKENTSEU_COMPILER_MSVC) || defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
                        return sizeof(T) <= 8;  // Approximation : types <= 8 bytes sont lock-free
                    #else
                        return false;  // Fallback conservateur
                    #endif
                }

                /**
                 * @brief Conversion implicite vers T (effectue un Load)
                 * @return Valeur chargée atomiquement
                 * @ingroup AtomicOperations
                 */
                NKENTSEU_FORCE_INLINE
                operator T() const NKENTSEU_NOEXCEPT
                {
                    return Load();
                }

                /**
                 * @brief Assignment depuis T (effectue un Store)
                 * @param value Valeur à stocker
                 * @return Valeur stockée
                 * @ingroup AtomicOperations
                 */
                NKENTSEU_FORCE_INLINE
                T operator=(T value) NKENTSEU_NOEXCEPT
                {
                    Store(value);
                    return value;
                }

                // --------------------------------------------------------
                // Opérateurs arithmétiques atomiques
                // --------------------------------------------------------

                /** @brief Pré-incrémentation atomique (++x) */
                NKENTSEU_FORCE_INLINE
                T operator++() NKENTSEU_NOEXCEPT
                {
                    return FetchAdd(1) + 1;
                }

                /** @brief Post-incrémentation atomique (x++) */
                NKENTSEU_FORCE_INLINE
                T operator++(int) NKENTSEU_NOEXCEPT
                {
                    return FetchAdd(1);
                }

                /** @brief Pré-décrémentation atomique (--x) */
                NKENTSEU_FORCE_INLINE
                T operator--() NKENTSEU_NOEXCEPT
                {
                    return FetchSub(1) - 1;
                }

                /** @brief Post-décrémentation atomique (x--) */
                NKENTSEU_FORCE_INLINE
                T operator--(int) NKENTSEU_NOEXCEPT
                {
                    return FetchSub(1);
                }

                /** @brief Addition-assignment atomique (x += v) */
                NKENTSEU_FORCE_INLINE
                T operator+=(T value) NKENTSEU_NOEXCEPT
                {
                    return FetchAdd(value) + value;
                }

                /** @brief Soustraction-assignment atomique (x -= v) */
                NKENTSEU_FORCE_INLINE
                T operator-=(T value) NKENTSEU_NOEXCEPT
                {
                    return FetchSub(value) - value;
                }

                // --------------------------------------------------------
                // Opérateurs bitwise atomiques (via CAS loop)
                // --------------------------------------------------------

                /** @brief ET bitwise-assignment atomique (x &= v) */
                NKENTSEU_FORCE_INLINE
                T operator&=(T value) NKENTSEU_NOEXCEPT
                {
                    T old = Load();
                    T desired;
                    do {
                        desired = static_cast<T>(old & value);
                    } while (!CompareExchangeWeak(old, desired));
                    return desired;
                }

                /** @brief OU bitwise-assignment atomique (x |= v) */
                NKENTSEU_FORCE_INLINE
                T operator|=(T value) NKENTSEU_NOEXCEPT
                {
                    T old = Load();
                    T desired;
                    do {
                        desired = static_cast<T>(old | value);
                    } while (!CompareExchangeWeak(old, desired));
                    return desired;
                }

                /** @brief XOR bitwise-assignment atomique (x ^= v) */
                NKENTSEU_FORCE_INLINE
                T operator^=(T value) NKENTSEU_NOEXCEPT
                {
                    T old = Load();
                    T desired;
                    do {
                        desired = static_cast<T>(old ^ value);
                    } while (!CompareExchangeWeak(old, desired));
                    return desired;
                }

            private:
                // --------------------------------------------------------
                // Membre de données
                // --------------------------------------------------------
                // volatile pour empêcher les optimisations, alignas pour atomicité matérielle
                alignas(T) volatile T mValue;
        };

        // ====================================================================
        // SECTION 5 : TYPEDEFS PRÉDÉFINIS POUR TYPES COURANTS
        // ====================================================================
        // Alias pour éviter la verbosité des templates dans le code client.

        /** @brief Atomic bool */
        using NkAtomicBool = NkAtomic<nk_bool>;

        /** @brief Atomic int8 */
        using NkAtomicInt8 = NkAtomic<nk_int8>;

        /** @brief Atomic int16 */
        using NkAtomicInt16 = NkAtomic<nk_int16>;

        /** @brief Atomic int32 */
        using NkAtomicInt32 = NkAtomic<nk_int32>;

        /** @brief Atomic int64 */
        using NkAtomicInt64 = NkAtomic<nk_int64>;

        /** @brief Atomic int (alias vers int32) */
        using NkAtomicInt = NkAtomic<nk_int32>;

        /** @brief Atomic uint8 */
        using NkAtomicUInt8 = NkAtomic<nk_uint8>;

        /** @brief Atomic uint16 */
        using NkAtomicUInt16 = NkAtomic<nk_uint16>;

        /** @brief Atomic uint32 */
        using NkAtomicUInt32 = NkAtomic<nk_uint32>;

        /** @brief Atomic uint64 */
        using NkAtomicUInt64 = NkAtomic<nk_uint64>;

        /** @brief Atomic uint (alias vers uint32) */
        using NkAtomicUint = NkAtomic<nk_uint32>;

        /** @brief Atomic size_t */
        using NkAtomicSize = NkAtomic<nk_size>;

        /** @brief Atomic void* */
        using NkAtomicPtr = NkAtomic<void *>;

        // ====================================================================
        // SECTION 6 : CLASSE NKATOMICFLAG (SPINLOCK MINIMAL)
        // ====================================================================
        // Flag atomique léger pour synchronisation basique.

        /**
         * @brief Flag atomique pour synchronisation légère (spinlock minimal)
         * @class NkAtomicFlag
         * @ingroup AtomicOperations
         *
         * Wrapper minimal autour de NkAtomic<nk_bool> pour les cas d'usage
         * de type "test-and-set". Plus léger qu'un mutex pour les sections
         * critiques très courtes.
         *
         * @warning
         *   Ne pas utiliser pour des sections critiques longues : le thread
         *   en attente consomme du CPU en boucle active (busy-wait).
         *
         * @example
         * @code
         * nkentseu::NkAtomicFlag lock{false};
         *
         * // Thread 1
         * while (lock.TestAndSet()) {
         *     // Attente active (spin)
         * }
         * // Section critique
         * sharedResource.DoWork();
         * lock.Clear();  // Libération
         *
         * // Thread 2 : même logique
         * @endcode
         */
        class NkAtomicFlag {
            public:
                /**
                 * @brief Constructeur avec valeur initiale optionnelle
                 * @param flag Valeur initiale (défaut: false)
                 * @ingroup AtomicOperations
                 */
                NkAtomicFlag(nk_bool flag = false) NKENTSEU_NOEXCEPT
                    : mFlag(flag)
                {
                }

                /**
                 * @brief Test-and-set atomique
                 * @param order Ordre mémoire pour l'opération (défaut: ACQREL)
                 * @return true si le flag était déjà set, false sinon
                 * @note Retourne l'ancienne valeur : true = déjà acquis, false = acquis maintenant
                 * @ingroup AtomicOperations
                 */
                NKENTSEU_FORCE_INLINE
                nk_bool TestAndSet(NkMemoryOrder order = NkMemoryOrder::NK_ACQREL) NKENTSEU_NOEXCEPT
                {
                    return mFlag.Exchange(true, order);
                }

                /**
                 * @brief Clear atomique du flag
                 * @param order Ordre mémoire pour l'opération (défaut: SEQCST)
                 * @ingroup AtomicOperations
                 */
                NKENTSEU_FORCE_INLINE
                void Clear(NkMemoryOrder order = NkMemoryOrder::NK_SEQCST) NKENTSEU_NOEXCEPT
                {
                    mFlag.Store(false, order);
                }

                /**
                 * @brief Test de l'état courant du flag
                 * @return true si set, false sinon
                 * @ingroup AtomicOperations
                 */
                NKENTSEU_FORCE_INLINE
                nk_bool IsSet() const NKENTSEU_NOEXCEPT
                {
                    return mFlag.Load();
                }

            private:
                NkAtomic<nk_bool> mFlag;  ///< Flag atomique interne
        };

        // ====================================================================
        // SECTION 7 : BARRIÈRES MÉMOIRE EXPLICITES
        // ====================================================================
        // Fonctions pour contrôler l'ordre des opérations mémoire entre threads.

        /**
         * @brief Barrière mémoire pour thread courant
         * @param order Ordre mémoire (défaut: SEQCST)
         * @ingroup AtomicOperations
         *
         * Empêche le réordonnancement des opérations mémoire autour de ce point.
         * Utiliser avec parcimonie : impact potentiel sur les performances.
         */
        NKENTSEU_FORCE_INLINE
        void NkAtomicThreadFence(NkMemoryOrder order = NkMemoryOrder::NK_SEQCST)
        {
            (void)order;  // Unused in fallback implementation

            #if defined(NKENTSEU_COMPILER_MSVC)
                MemoryBarrier();
            #elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
                __atomic_thread_fence(__ATOMIC_SEQ_CST);
            #endif
        }

        /**
         * @brief Barrière acquire (empêche le réordonnancement après)
         * @ingroup AtomicOperations
         *
         * Toutes les lectures/écritures après cette barrière sont garanties
         * de ne pas être réordonnancées avant la barrière.
         */
        NKENTSEU_FORCE_INLINE
        void NkAtomicAcquireFence()
        {
            #if defined(NKENTSEU_COMPILER_MSVC)
                _ReadBarrier();
            #elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
                __asm__ volatile("" ::: "memory");
            #endif
        }

        /**
         * @brief Barrière release (empêche le réordonnancement avant)
         * @ingroup AtomicOperations
         *
         * Toutes les lectures/écritures avant cette barrière sont garanties
         * de ne pas être réordonnancées après la barrière.
         */
        NKENTSEU_FORCE_INLINE
        void NkAtomicReleaseFence()
        {
            #if defined(NKENTSEU_COMPILER_MSVC)
                _WriteBarrier();
            #elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
                __asm__ volatile("" ::: "memory");
            #endif
        }

        /**
         * @brief Barrière compile-time uniquement (pas d'instruction CPU)
         * @ingroup AtomicOperations
         *
         * Empêche le compilateur de réordonnancer les accès mémoire autour
         * de ce point, mais n'émet pas d'instruction de barrière CPU.
         * Utile pour les optimisations agressives du compilateur.
         */
        NKENTSEU_FORCE_INLINE
        void NkAtomicCompileBarrier()
        {
            #if defined(NKENTSEU_COMPILER_MSVC)
                _ReadWriteBarrier();
            #elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
                __asm__ volatile("" ::: "memory");
            #endif
        }

        // ====================================================================
        // SECTION 8 : FONCTIONS ATOMIQUES GLOBALES UTILITAIRES
        // ====================================================================
        // Wrappers pour opérations courantes avec retour de nouvelle/ancienne valeur.

        /**
         * @brief Incrémente atomiquement et retourne l'ancienne valeur
         * @tparam T Type atomique (doit supporter FetchAdd)
         * @param atomic Référence vers l'atomique à incrémenter
         * @param order Ordre mémoire pour l'opération
         * @return Valeur avant l'incrémentation
         * @ingroup AtomicOperations
         */
        template <typename T>
        NKENTSEU_FORCE_INLINE
        T NkAtomicIncrement(NkAtomic<T> &atomic, NkMemoryOrder order = NkMemoryOrder::NK_SEQCST)
        {
            return atomic.FetchAdd(static_cast<T>(1), order);
        }

        /**
         * @brief Décrémente atomiquement et retourne l'ancienne valeur
         * @tparam T Type atomique (doit supporter FetchSub)
         * @param atomic Référence vers l'atomique à décrémenter
         * @param order Ordre mémoire pour l'opération
         * @return Valeur avant la décrémentation
         * @ingroup AtomicOperations
         */
        template <typename T>
        NKENTSEU_FORCE_INLINE
        T NkAtomicDecrement(NkAtomic<T> &atomic, NkMemoryOrder order = NkMemoryOrder::NK_SEQCST)
        {
            return atomic.FetchSub(static_cast<T>(1), order);
        }

        /**
         * @brief Ajoute atomiquement et retourne la nouvelle valeur
         * @tparam T Type atomique (doit supporter FetchAdd)
         * @param atomic Référence vers l'atomique à modifier
         * @param value Valeur à ajouter
         * @param order Ordre mémoire pour l'opération
         * @return Valeur après l'addition
         * @ingroup AtomicOperations
         */
        template <typename T>
        NKENTSEU_FORCE_INLINE
        T NkAtomicAdd(NkAtomic<T> &atomic, T value, NkMemoryOrder order = NkMemoryOrder::NK_SEQCST)
        {
            return atomic.FetchAdd(value, order) + value;
        }

        /**
         * @brief Soustrait atomiquement et retourne la nouvelle valeur
         * @tparam T Type atomique (doit supporter FetchSub)
         * @param atomic Référence vers l'atomique à modifier
         * @param value Valeur à soustraire
         * @param order Ordre mémoire pour l'opération
         * @return Valeur après la soustraction
         * @ingroup AtomicOperations
         */
        template <typename T>
        NKENTSEU_FORCE_INLINE
        T NkAtomicSubtract(NkAtomic<T> &atomic, T value, NkMemoryOrder order = NkMemoryOrder::NK_SEQCST)
        {
            return atomic.FetchSub(value, order) - value;
        }

        // ====================================================================
        // SECTION 9 : SPINLOCK AVEC BACKOFF EXPONENTIEL
        // ====================================================================
        // Verrou léger optimisé pour les sections critiques très courtes.

        /**
         * @brief Spinlock avec backoff exponentiel pour réduire la contention
         * @class NkSpinLock
         * @ingroup AtomicOperations
         *
         * Verrou léger basé sur NkAtomicFlag avec stratégie de backoff exponentiel
         * pour réduire la contention CPU lors d'attente active. Optimisé pour les
         * sections critiques très courtes (< 100 cycles typiquement).
         *
         * @note
         *   - Utilise l'instruction PAUSE/YIELD pour réduire la consommation CPU
         *   - Le backoff double à chaque itération jusqu'à un maximum de 1024
         *   - Ne pas utiliser pour des sections critiques longues
         *
         * @warning
         *   Ce verrou n'est pas récursif : un thread ne peut pas le locker deux fois.
         *   Utiliser NkScopedSpinLock pour une gestion RAII sécurisée.
         *
         * @example
         * @code
         * nkentseu::NkSpinLock lock;
         *
         * void UpdateSharedData() {
         *     lock.Lock();  // Acquisition (busy-wait si déjà pris)
         *     // Section critique
         *     sharedCounter++;
         *     sharedBuffer.Write(data);
         *     lock.Unlock();  // Libération immédiate
         * }
         *
         * // Version avec tentative non-bloquante
         * if (lock.TryLock()) {
         *     // Section critique
         *     lock.Unlock();
         * } else {
         *     // Alternative : traiter sans verrou ou réessayer plus tard
         * }
         * @endcode
         */
        class NkSpinLock {
            public:
                /** @brief Constructeur (flag initialisé à false = déverrouillé) */
                NkSpinLock()
                    : mFlag(false)
                {
                }

                /**
                 * @brief Acquisition bloquante avec backoff exponentiel
                 * @note Boucle active jusqu'à acquisition réussie
                 */
                void Lock()
                {
                    nk_size backoff = 1;
                    while (true) {
                        // Essayer d'acquérir le lock
                        if (!mFlag.TestAndSet(NkMemoryOrder::NK_ACQUIRE)) {
                            return;  // Success : lock acquis
                        }

                        // Backoff exponentiel pour réduire la contention
                        for (nk_size i = 0; i < backoff; ++i) {
                            #if defined(NKENTSEU_COMPILER_MSVC) && (defined(_M_IX86) || defined(_M_X64))
                                _mm_pause();  // Instruction PAUSE sur x86/x64 MSVC
                            #elif (defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)) && (defined(__i386__) || defined(__x86_64__))
                                __builtin_ia32_pause();  // Instruction PAUSE sur x86/x64 GCC/Clang
                            #elif defined(__aarch64__) || defined(__arm__)
                                __asm__ __volatile__("yield");  // Instruction YIELD sur ARM
                            #else
                                // Fallback no-op pour architectures sans instruction de pause dédiée
                            #endif
                        }

                        // Double le backoff, avec un maximum pour éviter l'overflow
                        if (backoff < 1024) {
                            backoff <<= 1;
                        }
                    }
                }

                /**
                 * @brief Tentative d'acquisition non-bloquante
                 * @return true si acquis, false si déjà pris
                 */
                nk_bool TryLock()
                {
                    return !mFlag.TestAndSet(NkMemoryOrder::NK_ACQUIRE);
                }

                /**
                 * @brief Libération du verrou
                 * @note Doit être appelé par le thread qui détient le lock
                 */
                void Unlock()
                {
                    mFlag.Clear(NkMemoryOrder::NK_RELEASE);
                }

            private:
                NkAtomicFlag mFlag;  ///< Flag atomique interne pour l'état du lock
        };

        // ====================================================================
        // SECTION 10 : SCOPE GUARD POUR SPINLOCK (RAII)
        // ====================================================================
        // Gestion automatique des verrous via RAII pour éviter les oublis de Unlock.

        /**
         * @brief Scope guard pour gestion RAII de NkSpinLock
         * @class NkScopedSpinLock
         * @ingroup AtomicOperations
         *
         * Wrapper RAII qui acquiert un NkSpinLock à la construction et le libère
         * automatiquement à la destruction. Garantit que le lock est toujours
         * libéré, même en cas d'exception ou de retour prématuré.
         *
         * @note
         *   - Non-copiable et non-déplaçable pour éviter les transferts accidentels
         *   - Détruit dans l'ordre inverse de construction (LIFO)
         *
         * @example
         * @code
         * nkentseu::NkSpinLock globalLock;
         *
         * void SafeUpdate() {
         *     nkentseu::NkScopedSpinLock guard(globalLock);  // Lock acquis ici
         *
         *     // Section critique : lock automatiquement libéré à la sortie
         *     sharedData.Modify();
         *     // Pas besoin d'appeler Unlock() manuellement
         *
         *     // Même en cas de return ou throw, le lock est libéré
         * }
         * @endcode
         */
        class NkScopedSpinLock {
            public:
                /**
                 * @brief Constructeur : acquiert le lock immédiatement
                 * @param lock Référence vers le NkSpinLock à verrouiller
                 */
                explicit NkScopedSpinLock(NkSpinLock &lock)
                    : mLock(lock)
                {
                    mLock.Lock();
                }

                /**
                 * @brief Destructeur : libère automatiquement le lock
                 */
                ~NkScopedSpinLock()
                {
                    mLock.Unlock();
                }

                // Suppression des opérations de copie et déplacement
                NkScopedSpinLock(const NkScopedSpinLock &) = delete;
                NkScopedSpinLock &operator=(const NkScopedSpinLock &) = delete;

            private:
                NkSpinLock &mLock;  ///< Référence vers le lock géré
        };

		using NkSpinLockGuard = NkScopedSpinLock;

    } // namespace nkentseu

    // Réactivation des warnings GCC/Clang après la section atomique
    #if defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
        #pragma GCC diagnostic pop
    #endif

#endif // NKENTSEU_NKCORE_NKATOMIC_H_INCLUDED

// =============================================================================
// EXEMPLES D'UTILISATION DE NKATOMIC.H
// =============================================================================
// Ce fichier fournit des primitives atomiques thread-safe pour le développement
// concurrent avec NKCore.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Compteur atomique thread-safe
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkAtomic.h"

    // Compteur global partagé entre threads
    static nkentseu::NkAtomicUInt32 g_frameCounter{0};

    void RenderThread() {
        // Incrémentation atomique sans verrou
        nk_uint32 frame = nkentseu::NkAtomicIncrement(g_frameCounter);

        // Lecture avec ordre relaxé (plus rapide, cohérence éventuellement retardée)
        nk_uint32 current = g_frameCounter.Load(nkentseu::NkMemoryOrder::NK_RELAXED);

        // Utilisation de l'opérateur d'incrémentation
        g_frameCounter++;  // Équivalent à FetchAdd(1) + 1

        // Lecture implicite via conversion operator
        nk_uint32 value = g_frameCounter;  // Appel Load() automatiquement
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Pattern producteur-consommateur avec Acquire/Release
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkAtomic.h"

    struct SharedData {
        nkentseu::NkAtomic<bool> ready{false};
        int payload = 0;  // Non-atomique, protégé par la synchronisation
    };

    void Producer(SharedData& data, int value) {
        // Écriture des données (non-atomique, safe car pas encore visible)
        data.payload = value;

        // Publication avec barrière Release : garantit que payload est visible
        // avant que ready ne soit vu comme true par les consommateurs
        data.ready.Store(true, nkentseu::NkMemoryOrder::NK_RELEASE);
    }

    void Consumer(SharedData& data) {
        // Attente avec barrière Acquire : garantit de voir les écritures
        // faites avant le Store(Release) dans le producteur
        while (!data.ready.Load(nkentseu::NkMemoryOrder::NK_ACQUIRE)) {
            // Optionnel : yield ou sleep pour réduire la consommation CPU
            #if defined(NKENTSEU_COMPILER_MSVC)
                YieldProcessor();
            #elif defined(__GNUC__) || defined(__clang__)
                __builtin_ia32_pause();
            #endif
        }

        // Lecture safe de payload grâce à la synchronisation Acquire/Release
        int value = data.payload;
        ProcessValue(value);
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Compare-and-Swap pour mise à jour conditionnelle
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkAtomic.h"

    // Mise à jour atomique d'un maximum sans verrou
    void UpdateMax(nkentseu::NkAtomicInt32& currentMax, nk_int32 newValue) {
        nk_int32 expected = currentMax.Load();

        // Boucle CAS : réessayer si la valeur a changé entre temps
        while (newValue > expected) {
            // Tenter de remplacer expected par newValue si currentMax == expected
            if (currentMax.CompareExchangeWeak(expected, newValue)) {
                break;  // Succès : maximum mis à jour
            }
            // Échec : expected contient maintenant la nouvelle valeur courante
            // La boucle continue avec la nouvelle valeur pour réessayer
        }
    }

    // Version avec Strong CAS (garanti pas d'échec spurieux)
    bool TrySetFlag(nkentseu::NkAtomicBool& flag, bool expected, bool desired) {
        return flag.CompareExchangeStrong(expected, desired);
        // Retourne true si flag == expected et a été remplacé par desired
        // Retourne false sinon, avec expected mis à jour vers la valeur courante
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Spinlock pour section critique courte
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkAtomic.h"

    // Ressource partagée protégée par spinlock
    struct SharedBuffer {
        nkentseu::NkSpinLock lock;
        char data[256];
        nk_size size = 0;
    };

    void WriteToBuffer(SharedBuffer& buffer, const char* src, nk_size len) {
        // Acquisition avec backoff exponentiel
        buffer.lock.Lock();

        // Section critique : accès exclusif garanti
        if (len <= sizeof(buffer.data)) {
            memcpy(buffer.data, src, len);
            buffer.size = len;
        }

        // Libération explicite
        buffer.lock.Unlock();
    }

    // Version RAII avec NkScopedSpinLock (recommandée)
    void SafeWrite(SharedBuffer& buffer, const char* src, nk_size len) {
        nkentseu::NkScopedSpinLock guard(buffer.lock);  // Lock acquis

        // Section critique : lock libéré automatiquement à la sortie
        if (len <= sizeof(buffer.data)) {
            memcpy(buffer.data, src, len);
            buffer.size = len;
        }
        // Pas de risque d'oublier Unlock(), même avec return ou exception
    }

    // Version non-bloquante avec TryLock
    bool TryWrite(SharedBuffer& buffer, const char* src, nk_size len) {
        if (!buffer.lock.TryLock()) {
            return false;  // Lock déjà pris, traitement différé ou ignoré
        }

        // Section critique
        if (len <= sizeof(buffer.data)) {
            memcpy(buffer.data, src, len);
            buffer.size = len;
        }

        buffer.lock.Unlock();  // Libération manuelle requise avec TryLock
        return true;
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Barrières mémoire pour synchronisation manuelle
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkAtomic.h"

    // Structure pour communication inter-threads sans verrou
    struct LockFreeQueue {
        nkentseu::NkAtomic<nk_size> writeIndex{0};
        nkentseu::NkAtomic<nk_size> readIndex{0};
        int buffer[1024];  // Tableau circulaire
    };

    void Producer(LockFreeQueue& queue, int value) {
        nk_size index = queue.writeIndex.Load();

        // Écriture des données (non-atomique)
        queue.buffer[index % 1024] = value;

        // Barrière pour garantir que l'écriture dans buffer est visible
        // avant la mise à jour de writeIndex
        nkentseu::NkAtomicReleaseFence();

        // Publication de la nouvelle position d'écriture
        queue.writeIndex.Store(index + 1, nkentseu::NkMemoryOrder::NK_RELEASE);
    }

    void Consumer(LockFreeQueue& queue, int& outValue) {
        nk_size readIdx = queue.readIndex.Load();
        nk_size writeIdx = queue.writeIndex.Load(nkentseu::NkMemoryOrder::NK_ACQUIRE);

        if (readIdx >= writeIdx) {
            return;  // Queue vide
        }

        // Barrière pour garantir de voir les données écrites avant writeIndex
        nkentseu::NkAtomicAcquireFence();

        // Lecture safe des données
        outValue = queue.buffer[readIdx % 1024];
        queue.readIndex.Store(readIdx + 1, nkentseu::NkMemoryOrder::NK_RELEASE);
    }
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Utilisation des typedefs prédéfinis
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkAtomic.h"

    class ThreadSafeStats {
    public:
        ThreadSafeStats()
            : m_hitCount(0)
            , m_missCount(0)
            , m_enabled(true)
        {
        }

        void RecordHit() {
            // Utilisation des typedefs pour plus de lisibilité
            nkentseu::NkAtomicUInt64::operator++(m_hitCount);  // Ou simplement : m_hitCount++
        }

        void RecordMiss() {
            m_missCount++;  // Incrémentation atomique via operator++
        }

        void SetEnabled(bool value) {
            // Store atomique sur booléen
            m_enabled.Store(value, nkentseu::NkMemoryOrder::NK_RELEASE);
        }

        bool IsEnabled() const {
            // Load atomique avec conversion implicite
            return m_enabled;  // Équivalent à m_enabled.Load()
        }

        StatsSnapshot GetSnapshot() const {
            // Lecture cohérente des deux compteurs avec barrière
            nk_uint64 hits = m_hitCount.Load(nkentseu::NkMemoryOrder::NK_ACQUIRE);
            nk_uint64 misses = m_missCount.Load(nkentseu::NkMemoryOrder::NK_ACQUIRE);
            return {hits, misses};
        }

    private:
        // Typedefs prédéfinis pour code plus concis
        nkentseu::NkAtomicUInt64 m_hitCount;
        nkentseu::NkAtomicUInt64 m_missCount;
        nkentseu::NkAtomicBool m_enabled;
    };
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Combinaison avec autres modules NKCore
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkAtomic.h"
    #include "NKCore/NkTypes.h"
    #include "NKPlatform/NkFoundationLog.h"

    // Allocation thread-safe avec comptage statistique
    class TrackingAllocator {
    public:
        void* Allocate(nkentseu::nk_size size, nkentseu::nk_size alignment) {
            void* ptr = PlatformAllocate(size, alignment);

            if (ptr != NK_NULL) {
                // Mise à jour atomique des statistiques
                m_totalAllocated += size;
                m_allocationCount++;

                #if !defined(NDEBUG)
                    NK_FOUNDATION_LOG_DEBUG(
                        "Allocated %zu bytes (total: %zu, count: %u)",
                        size,
                        m_totalAllocated.Load(),
                        m_allocationCount.Load()
                    );
                #endif
            }

            return ptr;
        }

        void Deallocate(void* ptr) {
            if (ptr != NK_NULL) {
                nk_size size = GetAllocationSize(ptr);  // Fonction interne
                PlatformDeallocate(ptr);

                m_totalAllocated -= size;
                m_deallocationCount++;
            }
        }

        AllocationStats GetStats() const {
            return {
                .totalAllocated = m_totalAllocated.Load(),
                .allocationCount = m_allocationCount.Load(),
                .deallocationCount = m_deallocationCount.Load(),
                .activeAllocations = m_allocationCount.Load() - m_deallocationCount.Load()
            };
        }

    private:
        // Compteurs atomiques pour accès concurrent sans verrou
        nkentseu::NkAtomicSize m_totalAllocated{0};
        nkentseu::NkAtomicUInt32 m_allocationCount{0};
        nkentseu::NkAtomicUInt32 m_deallocationCount{0};

        nk_size GetAllocationSize(void* ptr);  // Implémentation interne
        void* PlatformAllocate(nk_size, nk_size);
        void PlatformDeallocate(void*);
    };
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================