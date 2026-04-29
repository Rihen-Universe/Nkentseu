// =============================================================================
// NKCore/Assert/NkAssert.h
// Macros d'assertions runtime et compile-time pour débogage et validation.
//
// Design :
//  - Macro NKENTSEU_ASSERT() pour assertions conditionnelles en debug
//  - Macro NKENTSEU_ASSERT_MSG() avec message personnalisé pour contexte
//  - Macro NKENTSEU_VERIFY() pour validations toujours exécutées (même en release)
//  - Macro NKENTSEU_STATIC_ASSERT() portable pour assertions compile-time
//  - Intégration avec NkAssertHandler pour gestion centralisée des échecs
//  - Support multi-compilateur : C++11 static_assert, C11 _Static_assert, fallback C99
//  - Zéro overhead en production : assertions désactivées via NKENTSEU_ENABLE_ASSERTS
//
// Intégration :
//  - Utilise NKCore/Assert/NkAssertion.h pour NkAssertionInfo et NkAssertAction
//  - Utilise NKCore/Assert/NkAssertHandler.h pour gestion centralisée
//  - Utilise NKCore/Assert/NkDebugBreak.h pour points d'arrêt debugger
//  - Utilise NKCore/NkConfig.h pour NKENTSEU_ENABLE_ASSERTS et autres flags
//  - Utilise NKCore/NkPlatform.h pour NKENTSEU_FILE_NAME, NKENTSEU_LINE_NUMBER, etc.
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_CORE_NKASSERT_H_INCLUDED
#define NKENTSEU_CORE_NKASSERT_H_INCLUDED

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion des modules requis pour le système d'assertions.

    #include "NkAssertion.h"
    #include "NkAssertHandler.h"
    #include "NkDebugBreak.h"
    #include "NKCore/NkConfig.h"
    #include "NKCore/NkPlatform.h"

    // Inclusion standard pour ::abort()
    #include <stdlib.h>

    // -------------------------------------------------------------------------
    // SECTION 2 : MACROS D'ASSERTION RUNTIME
    // -------------------------------------------------------------------------
    // Définition conditionnelle selon NKENTSEU_ENABLE_ASSERTS.

    /**
     * @defgroup AssertMacros Macros d'Assertions Runtime
     * @brief Macros pour validation conditionnelle avec gestion centralisée des échecs
     * @ingroup AssertUtilities
     *
     * Ces macros fournissent un système d'assertions flexible pour :
     *   - Valider des invariants et préconditions en développement
     *   - Capturer le contexte complet (expression, fichier, ligne, fonction)
     *   - Délégation à NkAssertHandler pour politique de gestion configurable
     *   - Désactivation totale en production via NKENTSEU_ENABLE_ASSERTS
     *
     * @note
     *   - NKENTSEU_ASSERT() : assertion basique sans message
     *   - NKENTSEU_ASSERT_MSG() : assertion avec message personnalisé
     *   - NKENTSEU_VERIFY() : validation toujours évaluée, même en release
     *   - En mode release sans assertions : les macros s'expandent en ((void)0)
     *
     * @warning
     *   Ne pas placer d'effets de bord dans l'expression d'une NKENTSEU_ASSERT() :
     *   en release, l'expression ne sera pas évaluée si NKENTSEU_ENABLE_ASSERTS est faux.
     *   Préférer NKENTSEU_VERIFY() pour les expressions avec effets de bord.
     *
     * @example
     * @code
     * void ProcessBuffer(void* data, size_t size) {
     *     // Assertions basiques (désactivées en release)
     *     NKENTSEU_ASSERT(data != nullptr);
     *     NKENTSEU_ASSERT_MSG(size > 0, "Buffer size must be positive");
     *
     *     // Validation avec effet de bord (toujours évaluée)
     *     NKENTSEU_VERIFY(InitializeSubsystem() == RESULT_OK);
     *
     *     // ... traitement ...
     * }
     * @endcode
     */
    /** @{ */

    // ============================================================
    // MODE ASSERTIONS ACTIVÉES : NKENTSEU_ENABLE_ASSERTS défini
    // ============================================================

    #if defined(NKENTSEU_ENABLE_ASSERTS)

        /**
         * @brief Implémentation interne des macros d'assertion
         * @def NKENTSEU_ASSERT_IMPL(condition, message, file, line, func)
         * @param condition Expression à évaluer
         * @param message Message personnalisé (peut être chaîne vide)
         * @param file Nom du fichier source (via macro plateforme)
         * @param line Numéro de ligne (via macro plateforme)
         * @param func Nom de la fonction (via macro plateforme)
         * @ingroup AssertMacros
         * @internal
         *
         * @note
         *   - Utilise do-while(0) pour sécurité syntaxique dans tous les contextes
         *   - Capture le contexte dans NkAssertionInfo pour le handler
         *   - Délègue à NkAssertHandler::HandleAssertion() pour décision
         *   - Exécute NKENTSEU_DEBUG_BREAK() ou ::abort() selon l'action retournée
         */
        #define NKENTSEU_ASSERT_IMPL(condition, message, file, line, func) \
        do { \
            if (!(condition)) { \
                nkentseu::NkAssertionInfo info = { \
                    #condition, \
                    message, \
                    file, \
                    static_cast<nkentseu::nk_int32>(line), \
                    func \
                }; \
                nkentseu::NkAssertAction action = \
                    nkentseu::NkAssertHandler::HandleAssertion(info); \
                if (action == nkentseu::NkAssertAction::NK_BREAK) { \
                    NKENTSEU_DEBUG_BREAK(); \
                } else if (action == nkentseu::NkAssertAction::NK_ABORT) { \
                    ::abort(); \
                } \
            } \
        } while (0)

        /**
         * @brief Assertion avec message personnalisé
         * @def NKENTSEU_ASSERT_MSG(condition, message)
         * @param condition Expression booléenne à valider
         * @param message Chaîne littérale décrivant l'échec attendu
         * @ingroup AssertMacros
         *
         * @note
         *   - Utilise les macros plateforme NKENTSEU_FILE_NAME, etc. pour le contexte
         *   - Le message est optionnel : passer "" pour un message vide
         *   - En release avec assertions désactivées : s'expand en ((void)0)
         *
         * @example
         * @code
         * NKENTSEU_ASSERT_MSG(ptr != nullptr, "Pointer must be initialized before use");
         * NKENTSEU_ASSERT_MSG(index < arraySize, "Index out of bounds");
         * @endcode
         */
        #define NKENTSEU_ASSERT_MSG(condition, message) \
        NKENTSEU_ASSERT_IMPL( \
            condition, \
            message, \
            NKENTSEU_FILE_NAME, \
            NKENTSEU_LINE_NUMBER, \
            NKENTSEU_FUNCTION_NAME)

        /**
         * @brief Assertion basique sans message personnalisé
         * @def NKENTSEU_ASSERT(condition)
         * @param condition Expression booléenne à valider
         * @ingroup AssertMacros
         *
         * @note
         *   - Équivalent à NKENTSEU_ASSERT_MSG(condition, "")
         *   - Plus concis pour les assertions simples et évidentes
         *
         * @example
         * @code
         * NKENTSEU_ASSERT(result == EXPECTED_VALUE);
         * NKENTSEU_ASSERT(object.IsValid());
         * @endcode
         */
        #define NKENTSEU_ASSERT(condition) \
        NKENTSEU_ASSERT_IMPL( \
            condition, \
            "", \
            NKENTSEU_FILE_NAME, \
            NKENTSEU_LINE_NUMBER, \
            NKENTSEU_FUNCTION_NAME)

        /**
         * @brief Validation toujours exécutée, même en mode release
         * @def NKENTSEU_VERIFY(expression)
         * @param expression Expression à évaluer (peut avoir des effets de bord)
         * @ingroup AssertMacros
         *
         * @note
         *   - Contrairement à NKENTSEU_ASSERT(), l'expression est TOUJOURS évaluée
         *   - En release : évalue l'expression mais ignore l'échec (pas de break/abort)
         *   - Utile pour les appels de fonction avec effets de bord nécessaires
         *
         * @warning
         *   Le retour de l'expression est ignoré en release : ne pas dépendre
         *   de la valeur de retour pour la logique métier.
         *
         * @example
         * @code
         * // Initialisation critique : toujours exécutée
         * NKENTSEU_VERIFY(InitializeHardware() == HW_SUCCESS);
         *
         * // En debug : échec → breakpoint/abort
         * // En release : échec → log ignoré, exécution continue
         * @endcode
         */
        #define NKENTSEU_VERIFY(expression) \
        NKENTSEU_ASSERT(expression)

        /**
         * @brief Validation toujours exécutée avec message personnalisé
         * @def NKENTSEU_VERIFY_MSG(expression, message)
         * @param expression Expression à évaluer
         * @param message Message décrivant l'échec attendu
         * @ingroup AssertMacros
         */
        #define NKENTSEU_VERIFY_MSG(expression, message) \
        NKENTSEU_ASSERT_MSG(expression, message)

    // ============================================================
    // MODE ASSERTIONS DÉSACTIVÉES : NKENTSEU_ENABLE_ASSERTS non défini
    // ============================================================

    #else

        /**
         * @brief Version no-op de NKENTSEU_ASSERT pour mode release
         * @def NKENTSEU_ASSERT(condition)
         * @param condition Expression ignorée (non évaluée)
         * @ingroup AssertMacros
         * @note S'expand en ((void)0) : zéro code généré, zéro overhead
         */
        #define NKENTSEU_ASSERT(condition) ((void)0)

        /**
         * @brief Version no-op de NKENTSEU_ASSERT_MSG pour mode release
         * @def NKENTSEU_ASSERT_MSG(condition, message)
         * @param condition Expression ignorée
         * @param message Message ignoré
         * @ingroup AssertMacros
         */
        #define NKENTSEU_ASSERT_MSG(condition, message) ((void)0)

        /**
         * @brief Version "evaluate-only" de NKENTSEU_VERIFY pour mode release
         * @def NKENTSEU_VERIFY(expression)
         * @param expression Expression évaluée mais résultat ignoré
         * @ingroup AssertMacros
         * @note S'expand en ((void)(expression)) : évaluation sans effet
         */
        #define NKENTSEU_VERIFY(expression) ((void)(expression))

        /**
         * @brief Version "evaluate-only" de NKENTSEU_VERIFY_MSG pour mode release
         * @def NKENTSEU_VERIFY_MSG(expression, message)
         * @param expression Expression évaluée mais résultat ignoré
         * @param message Message ignoré
         * @ingroup AssertMacros
         */
        #define NKENTSEU_VERIFY_MSG(expression, message) ((void)(expression))

    #endif

    /** @} */ // End of AssertMacros

    // -------------------------------------------------------------------------
    // SECTION 3 : MACROS D'ASSERTION COMPILE-TIME (STATIC ASSERT)
    // -------------------------------------------------------------------------
    // Assertions évaluées à la compilation pour validation de types/constantes.

    /**
     * @defgroup StaticAssertMacros Macros d'Assertions Compile-Time
     * @brief Macros portables pour assertions évaluées à la compilation
     * @ingroup CompileTimeUtilities
     *
     * Ces macros fournissent une interface uniforme pour les assertions
     * statiques, avec fallback automatique selon les capacités du compilateur :
     *   - C++11+ : utilise static_assert(condition, message)
     *   - C11+ : utilise _Static_assert(condition, message)
     *   - Fallback : technique du tableau de taille conditionnelle
     *
     * @note
     *   - Les assertions statiques sont TOUJOURS actives, même en release
     *   - L'échec cause une erreur de compilation, pas un runtime error
     *   - Utile pour valider des invariants de types, tailles, constantes
     *
     * @example
     * @code
     * // Validation de taille de type
     * NKENTSEU_STATIC_ASSERT(sizeof(nk_int32) == 4, "nk_int32 must be 4 bytes");
     *
     * // Validation de propriété de type
     * NKENTSEU_STATIC_ASSERT(std::is_trivially_copyable<MyType>::value,
     *                        "MyType must be trivially copyable");
     *
     * // Validation de constante
     * NKENTSEU_STATIC_ASSERT(MAX_BUFFER_SIZE <= 65536,
     *                        "Buffer size exceeds 16-bit limit");
     * @endcode
     */
    /** @{ */

    /**
     * @brief Macro portable pour assertion compile-time
     * @def NKENTSEU_STATIC_ASSERT(condition, message)
     * @param condition Expression constexpr à valider
     * @param message Chaîne littérale affichée si l'assertion échoue
     * @ingroup StaticAssertMacros
     *
     * @note
     *   La macro sélectionne automatiquement l'implémentation disponible :
     *   - C++11/14/17/20 : static_assert natif (meilleurs messages d'erreur)
     *   - C11 : _Static_assert natif
     *   - Fallback C99/C++98 : astuce de tableau de taille -1 en cas d'échec
     *
     * @warning
     *   Le fallback utilise __LINE__ pour l'unicité du nom : deux assertions
     *   sur la même ligne causeront une collision de noms. Éviter ou utiliser
     *   des macros d'encapsulation avec concaténation de tokens.
     */
    #ifndef NKENTSEU_STATIC_ASSERT
        #if defined(__cplusplus) && (__cplusplus >= 201103L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201103L))
            // C++11 et supérieur : static_assert natif
            #define NKENTSEU_STATIC_ASSERT(name, condition, message) \
            static_assert(condition, #name ": " message)
        #elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
            // C11 et supérieur : _Static_assert natif
            #define NKENTSEU_STATIC_ASSERT(name, condition, message) \
            _Static_assert(condition, #name ": " message)
        #else
            // Fallback pour compilateurs anciens : astuce de tableau
            #define NKENTSEU_STATIC_ASSERT(name, condition, message) \
            typedef char NKENTSEU_STATIC_ASSERT_JOIN(static_assert_#name, __LINE__)[(condition) ? 1 : -1] NKENTSEU_UNUSED
            // Macro d'aide pour concaténation de tokens
            #define NKENTSEU_STATIC_ASSERT_JOIN(a, b) NKENTSEU_STATIC_ASSERT_JOIN_IMPL(a, b)
            #define NKENTSEU_STATIC_ASSERT_JOIN_IMPL(a, b) a##b
        #endif
    #endif

    /** @} */ // End of StaticAssertMacros

#endif // NKENTSEU_CORE_NKASSERT_H_INCLUDED

// =============================================================================
// EXEMPLES D'UTILISATION DE NKASSERT.H
// =============================================================================
// Ce fichier fournit les macros principales pour assertions runtime et compile-time.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Assertions basiques pour validation de préconditions
// -----------------------------------------------------------------------------
/*
    #include "NKCore/Assert/NkAssert.h"

    void ProcessData(void* buffer, size_t size, int flags) {
        // Préconditions de base
        NKENTSEU_ASSERT(buffer != nullptr);
        NKENTSEU_ASSERT_MSG(size > 0, "Buffer size must be positive");
        NKENTSEU_ASSERT_MSG(size <= MAX_BUFFER_SIZE, "Buffer exceeds maximum allowed size");

        // Validation de flags avec message explicite
        NKENTSEU_ASSERT_MSG(
            (flags & ~(VALID_FLAG_MASK)) == 0,
            "Invalid flags provided: only bits 0-3 are supported"
        );

        // ... traitement des données ...
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : NKENTSEU_VERIFY pour expressions avec effets de bord
// -----------------------------------------------------------------------------
/*
    #include "NKCore/Assert/NkAssert.h"
    #include "NKCore/NkResult.h"

    bool InitializeSubsystem() {
        // Initialisation critique : toujours exécutée, même en release
        // En debug : échec → breakpoint pour inspection
        // En release : échec → log ignoré, retourne false pour gestion gracieuse
        NKENTSEU_VERIFY(ConfigureHardware() == HW_SUCCESS);
        NKENTSEU_VERIFY(AllocateResources() == RES_SUCCESS);

        // Assertion normale : désactivée en release
        NKENTSEU_ASSERT(m_resourceCount > 0);

        return true;
    }

    void CleanupSubsystem() {
        // Libération avec vérification : toujours exécutée
        NKENTSEU_VERIFY(ReleaseResources() == RES_SUCCESS);
        NKENTSEU_VERIFY(DeconfigureHardware() == HW_SUCCESS);

        // Post-condition : seulement vérifiée en debug
        NKENTSEU_ASSERT(m_resourceCount == 0);
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Assertions compile-time pour validation de types
// -----------------------------------------------------------------------------
/*
    #include "NKCore/Assert/NkAssert.h"
    #include "NKCore/NkTypes.h"
    #include <type_traits>

    // Template avec contraintes de type vérifiées à la compilation
    template <typename T>
    class SerializedBuffer {
        // Validation compile-time des propriétés requises
        NKENTSEU_STATIC_ASSERT(std::is_trivially_copyable<T>::value,
            "T must be trivially copyable for serialization");
        NKENTSEU_STATIC_ASSERT(!std::is_pointer<T>::value,
            "T cannot be a pointer type (use smart pointers or values)");
        NKENTSEU_STATIC_ASSERT(sizeof(T) <= MAX_SERIALIZED_SIZE,
            "T exceeds maximum serialized size limit");

    public:
        void Serialize(uint8_t* output) const {
            // Safe memcpy grâce aux assertions statiques ci-dessus
            memcpy(output, &m_value, sizeof(T));
        }

    private:
        T m_value;
    };

    // Usage : ces lignes compileront ou échoueront selon les contraintes
    SerializedBuffer<int> intBuffer;        // ✓ OK : int est trivially copyable
    SerializedBuffer<float> floatBuffer;    // ✓ OK
    // SerializedBuffer<std::string> strBuffer; // ✗ Échec : std::string n'est pas trivially copyable
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Invariants de classe avec assertions debug-only
// -----------------------------------------------------------------------------
/*
    #include "NKCore/Assert/NkAssert.h"

    class CircularQueue {
    public:
        void Push(int value) {
            // Préconditions
            NKENTSEU_ASSERT(!IsFull());
            NKENTSEU_ASSERT(m_writeIndex < m_capacity);

            // Opération
            m_buffer[m_writeIndex] = value;
            m_writeIndex = (m_writeIndex + 1) % m_capacity;
            ++m_size;

            // Invariants post-opération (debug only)
            NKENTSEU_ASSERT(m_size <= m_capacity);
            NKENTSEU_ASSERT(m_writeIndex < m_capacity);
            NKENTSEU_ASSERT(m_readIndex < m_capacity);
        }

        int Pop() {
            NKENTSEU_ASSERT(!IsEmpty());

            int value = m_buffer[m_readIndex];
            m_readIndex = (m_readIndex + 1) % m_capacity;
            --m_size;

            // Invariants post-opération
            NKENTSEU_ASSERT(m_size >= 0);
            NKENTSEU_ASSERT(m_readIndex < m_capacity);

            return value;
        }

        // Méthode de vérification d'invariants (appelable manuellement en debug)
        void ValidateInvariants() const {
            NKENTSEU_ASSERT(m_buffer != nullptr);
            NKENTSEU_ASSERT(m_capacity > 0);
            NKENTSEU_ASSERT(m_size >= 0 && m_size <= static_cast<int>(m_capacity));
            NKENTSEU_ASSERT(m_readIndex < m_capacity);
            NKENTSEU_ASSERT(m_writeIndex < m_capacity);
        }

    private:
        int* m_buffer = nullptr;
        size_t m_capacity = 0;
        int m_size = 0;
        size_t m_readIndex = 0;
        size_t m_writeIndex = 0;
    };
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Configuration du système d'assertions au démarrage
// -----------------------------------------------------------------------------
/*
    #include "NKCore/Assert/NkAssert.h"
    #include "NKCore/Assert/NkAssertHandler.h"
    #include "NKCore/NkFoundationLog.h"

    // Handler personnalisé pour environnement de développement
    nkentseu::NkAssertAction DevEnvironmentCallback(const nkentseu::NkAssertionInfo& info) {
        // Logging structuré avec contexte complet
        NK_FOUNDATION_LOG_ERROR(
            "ASSERT: %s\n  Location: %s:%d in %s\n  Details: %s",
            info.expression,
            info.file,
            info.line,
            info.function,
            info.message && info.message[0] ? info.message : "(no details)"
        );

        // En développement : breakpoint pour inspection interactive
        return nkentseu::NkAssertAction::NK_BREAK;
    }

    // Handler pour tests automatisés : capture sans interruption
    nkentseu::NkAssertAction TestEnvironmentCallback(const nkentseu::NkAssertionInfo& info) {
        // Enregistrer l'assertion pour rapport de test
        TestReporter::RecordAssertionFailure(info);

        // Continuer l'exécution du test
        return nkentseu::NkAssertAction::NK_CONTINUE;
    }

    // Configuration conditionnelle au démarrage
    void ConfigureAssertionSystem() {
        #if defined(NKENTSEU_DEBUG)
            #if defined(RUNNING_TESTS)
                nkentseu::NkAssertHandler::SetCallback(TestEnvironmentCallback);
                NK_FOUNDATION_LOG_INFO("Assertions: Test mode with capture handler");
            #else
                nkentseu::NkAssertHandler::SetCallback(DevEnvironmentCallback);
                NK_FOUNDATION_LOG_INFO("Assertions: Dev mode with breakpoint handler");
            #endif
        #else
            // En release : utiliser le handler par défaut (abort sur échec)
            nkentseu::NkAssertHandler::SetCallback(nullptr);
            NK_FOUNDATION_LOG_INFO("Assertions: Release mode with default abort handler");
        #endif
    }
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================