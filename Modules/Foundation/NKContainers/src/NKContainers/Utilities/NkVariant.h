// =============================================================================
// NKContainers/Utilities/NkVariant.h
// Forwarding header vers l'implémentation canonique NKCore::NkVariant
//
// Design :
//  - Forwarding unique vers NKCore (ZÉRO duplication de code)
//  - Alias de namespace pour intégration nkentseu::containers
//  - Documentation Doxygen pour visibilité dans l'API publique
//  - Compatibilité avec les modes de build indépendants
//  - Réutilisation des macros NKPlatform via NKCore
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_CONTAINERS_UTILITIES_NKVARIANT_H_INCLUDED
#define NKENTSEU_CONTAINERS_UTILITIES_NKVARIANT_H_INCLUDED

    // -------------------------------------------------------------------------
    // EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    #include "NKContainers/NkContainersApi.h"   // Pour la cohérence d'inclusion
    #include "NKCore/NkVariant.h"     // Implémentation canonique NKCore

    // -------------------------------------------------------------------------
    // ALIAS DE NAMESPACE POUR NKCONTAINERS
    // -------------------------------------------------------------------------
    /**
     * @namespace nkentseu::containers::variant
     * @brief Point d'accès à NkVariant depuis le namespace containers
     * @ingroup ContainersUtilities
     *
     * Fournit des alias vers l'implémentation NKCore pour une API cohérente.
     * Tous les types et fonctions sont forwardés depuis nkentseu::.
     */
    namespace nkentseu {
        namespace containers {

            // ====================================================================
            // TYPES PRINCIPAUX
            // ====================================================================

            /** @brief Alias vers nkentseu::NkVariant */
            using ::nkentseu::NkVariant;

            // ====================================================================
            // UTILITAIRES DE VISITE
            // ====================================================================

            /** @brief Alias vers nkentseu::NkVisit (fonction libre) */
            using ::nkentseu::NkVisit;

            // ====================================================================
            // HELPERS DE CONSTRUCTION
            // ====================================================================

            /** @brief Alias vers nkentseu::detail (pour accès avancé, usage interne) */
            using namespace ::nkentseu::detail;

            // ====================================================================
            // TRAITS ET MÉTAPROGRAMMATION (via detail namespace NKCore)
            // ====================================================================
            // Note: Les helpers de détail sont généralement utilisés en interne.
            // Ils sont exposés ici uniquement pour compatibilité avec du code
            // générique avancé. Préférer l'API publique de NkVariant.

        } // namespace variant

        // -------------------------------------------------------------------------
        // ALIAS AU NIVEAU CONTAINERS POUR USAGE DIRECT
        // -------------------------------------------------------------------------
        // Ces alias permettent d'utiliser les types sans le sous-namespace 'variant'
        // pour une ergonomie améliorée, tout en restant dans nkentseu::containers.

        using variant::NkVariant;
        using variant::NkVisit;

    } // namespace nkentseu

    // -------------------------------------------------------------------------
    // DOCUMENTATION RAPIDE (Doxygen)
    // -------------------------------------------------------------------------
    /**
     * @brief Type variant type-safe pour conteneurs polymorphes
     * @tparam Ts... Pack de types alternatifs supportés
     * @ingroup ContainersUtilities
     *
     * @note Implémentation canonique dans NKCore/Utilities/NkVariant.h.
     *       Ce header fournit uniquement des alias de namespace.
     *
     * @example
     * @code
     * using namespace nkentseu::containers;
     *
     * // Variant pour valeurs JSON simplifiées
     * using JsonValue = NkVariant<
     *     nk_nullptr_t, bool, double, nk_string,
     *     nk_array<JsonValue>, nk_map<nk_string, JsonValue>
     * >;
     *
     * JsonValue v = 42.0;  // Stocke un double
     * 
     * // Accès vérifié
     * if (v.HoldsAlternative<double>()) {
     *     double num = v.GetChecked<double>();
     * }
     *
     * // Pattern visiteur
     * NkVisit(v, [](auto&& value) {
     *     using T = std::decay_t<decltype(value)>;
     *     if constexpr (std::is_same_v<T, double>) {
     *         printf("Number: %f\n", value);
     *     }
     * });
     * @endcode
     *
     * @see nkentseu::NkVariant (implémentation complète dans NKCore)
     * @see nkentseu::NkVisit (fonction libre pour visiteur)
     */

    // -------------------------------------------------------------------------
    // VALIDATION DE CONFIGURATION
    // -------------------------------------------------------------------------
    #ifndef NKENTSEU_CORE_NKVARIANT_H
        #error "NKContainers/NkVariant.h requires NKCore/NkVariant.h to be included first"
    #endif

    // -------------------------------------------------------------------------
    // MESSAGES DE DEBUG OPTIONNELS
    // -------------------------------------------------------------------------
    #ifdef NKENTSEU_CONTAINERS_DEBUG_CONFIG
        #pragma message("NKContainers NkVariant: Forwarding to NKCore implementation")
    #endif

#endif // NKENTSEU_CONTAINERS_UTILITIES_NKVARIANT_H_INCLUDED

// =============================================================================
// EXEMPLES D'UTILISATION
// =============================================================================
/*
    // Exemple 1 : Variant simple pour valeurs hétérogènes
    #include <NKContainers/Utilities/NkVariant.h>

    using namespace nkentseu::containers;

    // Variant pour configuration : bool, int ou string
    using ConfigValue = NkVariant<bool, int, nk_string>;

    void PrintConfig(const ConfigValue& config) {
        if (config.HoldsAlternative<bool>()) {
            printf("Boolean: %s\n", config.GetChecked<bool>() ? "true" : "false");
        } else if (config.HoldsAlternative<int>()) {
            printf("Integer: %d\n", config.GetChecked<int>());
        } else if (config.HoldsAlternative<nk_string>()) {
            printf("String: %s\n", config.GetChecked<nk_string>().c_str());
        }
    }

    // Exemple 2 : Pattern visiteur pour traitement polymorphique
    void ProcessValue(const NkVariant<int, float, nk_string>& value) {
        NkVisit(value, [](auto&& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, int>) {
                printf("int: %d\n", v);
            } else if constexpr (std::is_same_v<T, float>) {
                printf("float: %.2f\n", v);
            } else if constexpr (std::is_same_v<T, nk_string>) {
                printf("string: %s\n", v.c_str());
            }
        });
    }

    // Exemple 3 : Accès conditionnel avec GetIf (safe)
    template<typename T>
    T* ExtractIf(NkVariant<int, float, nk_string>& variant) {
        return variant.template GetIf<T>();  // nullptr si type incorrect
    }

    // Exemple 4 : Emplace pour construction in-place efficace
    struct Expensive {
        Expensive(int x, int y) { /\* ... *\/ }
    };

    void EfficientConstruction() {
        NkVariant<int, Expensive> holder;
        holder.Emplace<Expensive>(10, 20);  // Construction directe, pas de copie
    }
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================