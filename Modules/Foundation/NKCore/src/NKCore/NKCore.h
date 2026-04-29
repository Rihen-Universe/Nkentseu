// =============================================================================
// NKCore/NKCore.h
// Point d'entrée principal du module NKCore.
//
// Design :
//  - Fichier "umbrella header" regroupant tous les exports publics du module
//  - Inclut les types fondamentaux, utilitaires, conteneurs et algorithmes
//  - Fournit une interface unifiée pour les composants de base du framework
//  - Conçu pour être le seul fichier à inclure pour utiliser NKCore
//
// Intégration :
//  - Inclut tous les headers publics de NKCore dans l'ordre de dépendance
//  - Inclut automatiquement NKPlatform/NKPlatform.h pour les dépendances système
//  - Peut être inclus seul si NKPlatform n'est pas requis
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_CORE_H_INCLUDED
#define NKENTSEU_CORE_H_INCLUDED

    // -------------------------------------------------------------------------
    // SECTION 1 : DÉPENDANCES EXTERNES REQUISES
    // -------------------------------------------------------------------------
    // Inclusion du module Platform pour les dépendances système fondamentales.

    /**
     * @defgroup CoreModule Module NKCore
     * @brief Module de base du framework avec types, utilitaires et conteneurs
     * @ingroup FrameworkModules
     *
     * NKCore fournit les composants fondamentaux pour le développement :
     *   - Types portables (nk_int32, nk_float64, nk_string, etc.)
     *   - Utilitaires génériques (macros, traits, invoke, variant, etc.)
     *   - Conteneurs et algorithmes (vector, hashmap, sort, etc.)
     *   - Système d'assertions et de logging intégré
     *
     * @note
     *   - Ce fichier est le seul à inclure pour utiliser le module NKCore
     *   - NKPlatform/NKPlatform.h est inclus automatiquement pour les dépendances
     *   - L'ordre d'inclusion est géré automatiquement pour éviter les dépendances circulaires
     *
     * @example
     * @code
     * // Inclusion unique du module Core
     * #include "NKCore/NKCore.h"
     *
     * void Example() {
     *     // Types portables
     *     nkentseu::nk_int32 count = 42;
     *     nkentseu::nk_string name = "Example"_nkstr;
     *
     *     // Conteneurs
     *     nkentseu::NkVector<nkentseu::nk_int32> numbers;
     *     numbers.PushBack(1);
     *     numbers.PushBack(2);
     *
     *     // Utilitaires
     *     if (nkentseu::NkIsPowerOfTwo(count)) {
     *         NK_FOUNDATION_LOG_INFO("Count is power of two");
     *     }
     * }
     * @endcode
     */
    /** @{ */

    // ============================================================
    // MODULE PLATFORM (DÉPENDANCE FONDAMENTALE)
    // ============================================================

    /**
     * @brief Module NKPlatform requis pour les types et détections de base
     * @ingroup CoreDependencies
     * @note Inclut NkPlatformDetect, NkTypes, NkPlatform runtime info, etc.
     */
    #include "NKCore/NKPlatform.h"

    // ============================================================
    // TYPES FONDAMENTAUX ET MÉTAPROGRAMMATION
    // ============================================================

    /**
     * @brief Types fondamentaux portables et alias
     * @ingroup CoreTypes
     * @note Définit nk_int8, nk_uint32, nk_float64, nk_string, etc.
     */
    #include "NKCore/NkTypes.h"

    /**
     * @brief Traits et utilitaires de métaprogrammation
     * @ingroup CoreTraits
     * @note Fournit NkRemoveReference, NkEnableIf, NkIsSame, etc.
     */
    #include "NKCore/NkTraits.h"

    /**
     * @brief Limites numériques portables sans <limits>
     * @ingroup CoreTypes
     * @note Fournit NkNumericLimits<T> pour min/max/epsilon des types
     */
    #include "NKCore/NkLimits.h"

    /**
     * @brief Utilitaires de types et conversions
     * @ingroup CoreTypes
     * @note Fournit NkClamp, NkBit, NkArraySize, littéraux, etc.
     */
    #include "NKCore/NkTypeUtils.h"

    // ============================================================
    // UTILITAIRES GÉNÉRIQUES ET ALGORITHMES
    // ============================================================

    /**
     * @brief Macros de préprocesseur et utilitaires compile-time
     * @ingroup CoreMacros
     * @note Fournit NKENTSEU_STRINGIFY, NKENTSEU_CONCAT, etc.
     */
    #include "NKCore/NkMacros.h"

    /**
     * @brief Fonction invoke portable inspirée de std::invoke
     * @ingroup CoreUtilities
     * @note Fournit NkInvoke() pour appel uniforme de callables
     */
    #include "NKCore/NkInvoke.h"

    /**
     * @brief Union discriminée type-safe inspirée de std::variant
     * @ingroup CoreContainers
     * @note Fournit NkVariant<Ts...> pour stockage polymorphique
     */
    #include "NKCore/NkVariant.h"

    /**
     * @brief Manipulation de bits avancée avec optimisations
     * @ingroup CoreUtilities
     * @note Fournit NkBits::CountBits, RotateLeft, ByteSwap, etc.
     */
    #include "NKCore/NkBits.h"

    // ============================================================
    // ASSERTIONS, LOGGING ET GESTION D'ERREURS
    // ============================================================

    /**
     * @brief Structure d'information pour assertions échouées
     * @ingroup CoreAssert
     * @note Fournit NkAssertionInfo pour contexte d'erreur
     */
    #include "NKCore/Assert/NkAssertion.h"

    /**
     * @brief Gestionnaire centralisé d'assertions
     * @ingroup CoreAssert
     * @note Fournit NkAssertHandler pour configuration des callbacks
     */
    #include "NKCore/Assert/NkAssertHandler.h"

    /**
     * @brief Macros d'assertions runtime et compile-time
     * @ingroup CoreAssert
     * @note Fournit NKENTSEU_ASSERT, NKENTSEU_STATIC_ASSERT, etc.
     */
    #include "NKCore/Assert/NkAssert.h"

    /**
     * @brief Point d'arrêt debugger portable
     * @ingroup CoreAssert
     * @note Fournit NKENTSEU_DEBUGBREAK() avec intrinsics platform-specific
     */
    #include "NKCore/Assert/NkDebugBreak.h"

    // ============================================================
    // CONFIGURATION GLOBALE DU FRAMEWORK
    // ============================================================

    /**
     * @brief Configuration globale du framework NKCore
     * @ingroup CoreConfig
     * @note Définit NKENTSEU_DEBUG, NKENTSEU_ENABLE_ASSERTS, etc.
     */
    #include "NKCore/NkConfig.h"

    /**
     * @brief Macros d'export et visibilité des symboles
     * @ingroup CoreExport
     * @note Définit NKENTSEU_CORE_API pour DLL/.so/.dylib
     */
    #include "NKCore/NkCoreExport.h"

    /**
     * @brief Versionnement du framework
     * @ingroup CoreConfig
     * @note Définit NKENTSEU_VERSION_MAJOR, MINOR, PATCH, etc.
     */
    #include "NKCore/NkVersion.h"

    /** @} */ // End of CoreModule

#endif // NKENTSEU_CORE_H_INCLUDED

// =============================================================================
// EXEMPLES D'UTILISATION DE NKCORE.H
// =============================================================================
// Ce fichier est le point d'entrée unique pour le module NKCore.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Inclusion minimale pour utilisation des types et utilitaires
// -----------------------------------------------------------------------------
/*
    // Fichier : app.cpp
    #include "NKCore/NKCore.h"  // Seul include nécessaire pour NKCore

    int main() {
        // Types portables
        nkentseu::nk_int32 count = 100;
        nkentseu::nk_float64 ratio = 3.14159;
        nkentseu::nk_string message = "Hello, NKCore!"_nkstr;

        // Utilitaires
        if (nkentseu::NkIsPowerOfTwo(count)) {
            NK_FOUNDATION_LOG_INFO("Count is power of two");
        }

        // Conteneurs
        nkentseu::NkVector<nkentseu::nk_string> items;
        items.Reserve(10);
        items.PushBack("Item 1"_nkstr);
        items.PushBack("Item 2"_nkstr);

        // Variant pour valeurs hétérogènes
        nkentseu::NkVariant<nkentseu::nk_int32, nkentseu::nk_float64, nkentseu::nk_string> value;
        value = 42;  // Stocke un int
        value.Visit([](auto&& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, nkentseu::nk_int32>) {
                printf("Integer: %d\n", v);
            }
        });

        return 0;
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Assertions et logging avec contexte automatique
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NKCore.h"

    bool ProcessData(nkentseu::nk_ptr buffer, nkentseu::nk_size size) {
        // Assertions avec message personnalisé
        NKENTSEU_ASSERT_MSG(buffer != nullptr, "Buffer pointer cannot be null");
        NKENTSEU_ASSERT_MSG(size > 0, "Buffer size must be positive");
        NKENTSEU_ASSERT_MSG(size <= MAX_BUFFER_SIZE, "Buffer exceeds maximum size");

        // Logging avec niveau et contexte source automatique
        NK_FOUNDATION_LOG_DEBUG("Processing %zu bytes at %p", size, buffer);

        // Vérification conditionnelle avec retour automatique
        NKENTSEU_CHECK_RETURN(InitializeProcessor(), false);

        // ... traitement des données ...

        NK_FOUNDATION_LOG_INFO("Processing completed successfully");
        return true;
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Utilisation de NkVariant pour configuration polymorphique
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NKCore.h"

    // Configuration pouvant être bool, int, float ou string
    using ConfigValue = nkentseu::NkVariant<
        nkentseu::nk_bool,
        nkentseu::nk_int32,
        nkentseu::nk_float64,
        nkentseu::nk_string
    >;

    class ConfigManager {
    public:
        void Set(const nkentseu::nk_string& key, ConfigValue value) {
            m_config[key] = std::move(value);
        }

        template <typename T>
        nkentseu::nk_bool Get(const nkentseu::nk_string& key, T& out) const {
            auto it = m_config.find(key);
            if (it == m_config.end()) {
                return false;
            }

            // Accès type-safe avec vérification
            if (const T* value = it->second.template GetIf<T>()) {
                out = *value;
                return true;
            }
            return false;
        }

        // Visiteur pour affichage/debug
        void Print(const nkentseu::nk_string& key) const {
            auto it = m_config.find(key);
            if (it != m_config.end()) {
                it->second.Visit([&](auto&& value) {
                    using ValueType = std::decay_t<decltype(value)>;
                    printf("%s = ", key.c_str());
                    if constexpr (std::is_same_v<ValueType, nkentseu::nk_string>) {
                        printf("\"%s\"", value.c_str());
                    } else {
                        printf("%s", nkentseu::NkToString(value).c_str());
                    }
                    printf("\n");
                });
            }
        }

    private:
        nkentseu::NkHashMap<nkentseu::nk_string, ConfigValue> m_config;
    };
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Algorithmes et manipulation de bits
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NKCore.h"

    void BitManipulationExample() {
        // Comptage de bits
        nkentseu::nk_uint32 flags = 0b10110100;
        nkentseu::nk_int32 activeBits = nkentseu::NkBits::CountBits(flags);
        printf("Active flags: %d\n", activeBits);  // 3

        // Rotation de bits
        nkentseu::nk_uint32 value = 0x12345678;
        nkentseu::nk_uint32 rotated = nkentseu::NkBits::RotateLeft(value, 8);
        printf("Rotated: 0x%08X\n", rotated);  // 0x34567812

        // Extraction de champ de bits
        nkentseu::nk_uint32 packet = 0xABCD1234;
        nkentseu::nk_uint32 header = nkentseu::NkBits::ExtractBits(packet, 28, 4);
        printf("Header: 0x%X\n", header);  // 0xA (bits 28-31)

        // Changement d'endianness pour réseau
        nkentseu::nk_uint32 hostValue = 0x12345678;
        nkentseu::nk_uint32 netValue = nkentseu::NkBits::ByteSwap32(hostValue);
        printf("Network order: 0x%08X\n", netValue);  // 0x78563412
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Configuration du framework via NKCore/NkConfig.h
// -----------------------------------------------------------------------------
/*
    // Avant d'inclure NKCore.h, configurer les options du framework
    #define NKENTSEU_DEBUG              // Activer le mode debug
    #define NKENTSEU_ENABLE_PROFILING   // Activer le profiling
    #define NKENTSEU_MATH_USE_DOUBLE    // Utiliser double précision pour les maths

    #include "NKCore/NKCore.h"  // Les options ci-dessus seront prises en compte

    void ConfiguredApplication() {
        // Vérification du mode de build
        #if NKENTSEU_IS_DEBUG
            NK_FOUNDATION_LOG_INFO("Running in Debug mode");
            NKENTSEU_DEBUG_ONLY(EnableDebugOverlays());
        #else
            NK_FOUNDATION_LOG_INFO("Running in Release mode");
        #endif

        // Précision mathématique configurée
        #if defined(NKENTSEU_MATH_PRECISION_DOUBLE)
            using Real = nkentseu::nk_float64;
        #else
            using Real = nkentseu::nk_float32;
        #endif

        Real pi = NKENTSEU_PI_DOUBLE;  // Constante avec précision adaptée
        printf("Pi = %.15f\n", static_cast<double>(pi));

        // Profiling scope (actif seulement si NKENTSEU_ENABLE_PROFILING)
        NKENTSEU_PROFILE_SCOPE("ConfiguredApplication");
        // ... code à profiler ...
    }
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================