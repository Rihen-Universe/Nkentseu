// =============================================================================
// NKContainers/Utilities/NkOptional.h
// Forwarding header vers l'implémentation canonique NKCore::NkOptional
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

#ifndef NKENTSEU_CONTAINERS_UTILITIES_NKOPTIONAL_H_INCLUDED
#define NKENTSEU_CONTAINERS_UTILITIES_NKOPTIONAL_H_INCLUDED

    // -------------------------------------------------------------------------
    // EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    #include "NKContainers/NkContainersApi.h"   // Pour la cohérence d'inclusion
    #include "NKCore/NkOptional.h"    // Implémentation canonique NKCore

    // -------------------------------------------------------------------------
    // ALIAS DE NAMESPACE POUR NKCONTAINERS
    // -------------------------------------------------------------------------
    /**
     * @namespace nkentseu::containers::optional
     * @brief Point d'accès à NkOptional depuis le namespace containers
     * @ingroup ContainersUtilities
     *
     * Fournit des alias vers l'implémentation NKCore pour une API cohérente.
     * Tous les types et fonctions sont forwardés depuis nkentseu::.
     */
    namespace nkentseu {
        namespace optional {

            // ====================================================================
            // TYPES PRINCIPAUX
            // ====================================================================

            /** @brief Alias vers nkentseu::NkOptional */
            using ::nkentseu::NkOptional;

            // ====================================================================
            // TAGS DE CONSTRUCTION
            // ====================================================================

            /** @brief Alias vers nkentseu::NkNullOpt_t (type sentinelle) */
            using ::nkentseu::NkNullOpt_t;

            /** @brief Alias vers nkentseu::NkNullOpt (instance globale constexpr) */
            using ::nkentseu::NkNullOpt;

            // ====================================================================
            // FONCTIONS LIBRES UTILITAIRES (si présentes dans NKCore)
            // ====================================================================
            // Note: NkOptional dans NKCore n'expose pas de fonctions libres
            // supplémentaires au-delà des méthodes membres. Les alias ci-dessus
            // suffisent pour l'usage standard.

        } // namespace optional

        // -------------------------------------------------------------------------
        // ALIAS AU NIVEAU CONTAINERS POUR USAGE DIRECT
        // -------------------------------------------------------------------------
        // Ces alias permettent d'utiliser les types sans le sous-namespace 'optional'
        // pour une ergonomie améliorée, tout en restant dans nkentseu::containers.

        using optional::NkOptional;
        using optional::NkNullOpt_t;
        using optional::NkNullOpt;

    } // namespace nkentseu

    // -------------------------------------------------------------------------
    // DOCUMENTATION RAPIDE (Doxygen)
    // -------------------------------------------------------------------------
    /**
     * @brief Type optional pour valeurs potentiellement absentes
     * @tparam T Type de la valeur optionnelle
     * @ingroup ContainersUtilities
     *
     * @note Implémentation canonique dans NKCore/Utilities/NkOptional.h.
     *       Ce header fournit uniquement des alias de namespace.
     *
     * @example
     * @code
     * using namespace nkentseu::containers;
     *
     * // Fonction qui peut échouer
     * NkOptional<nk_string> FindUser(nk_uint64 id) {
     *     // ... recherche en base ...
     *     if (found) return NkOptional<nk_string>(name);
     *     return NkNullOpt;  // Retourne l'état "vide"
     * }
     *
     * // Utilisation
     * auto user = FindUser(123);
     * if (user.HasValue()) {
     *     printf("User: %s\n", user.Value().c_str());
     * }
     *
     * // Accès sécurisé avec fallback
     * nk_string username = user.ValueOr("Anonymous");
     *
     * // Style idiomatique avec conversion booléenne
     * if (user) {
     *     Process(*user);  // operator*
     * }
     *
     * // Construction in-place efficace
     * NkOptional<std::vector<int>> vec;
     * vec.Emplace(10, 42);  // Construit vector(10, 42) directement
     * @endcode
     *
     * @see nkentseu::NkOptional (implémentation complète dans NKCore)
     * @see nkentseu::NkNullOpt (constante pour état vide)
     */

    // -------------------------------------------------------------------------
    // VALIDATION DE CONFIGURATION
    // -------------------------------------------------------------------------
    #ifndef NKENTSEU_CORE_NKOPTIONAL_H
        #error "NKContainers/NkOptional.h requires NKCore/NkOptional.h to be included first"
    #endif

    // -------------------------------------------------------------------------
    // MESSAGES DE DEBUG OPTIONNELS
    // -------------------------------------------------------------------------
    #ifdef NKENTSEU_CONTAINERS_DEBUG_CONFIG
        #pragma message("NKContainers NkOptional: Forwarding to NKCore implementation")
    #endif

#endif // NKENTSEU_CONTAINERS_UTILITIES_NKOPTIONAL_H_INCLUDED

// =============================================================================
// EXEMPLES D'UTILISATION
// =============================================================================
/*
    // Exemple 1 : Création et utilisation basique
    #include <NKContainers/Utilities/NkOptional.h>

    using namespace nkentseu::containers;

    void BasicUsage() {
        // Optionnel vide par défaut
        NkOptional<int> maybeInt;

        // Vérification d'état
        if (maybeInt.Empty()) {
            printf("maybeInt est vide\n");
        }

        // Assignation d'une valeur
        maybeInt.Emplace(42);

        // Accès après vérification
        if (maybeInt.HasValue()) {
            printf("Valeur : %d\n", maybeInt.Value());
        }

        // Conversion booléenne idiomatique
        if (maybeInt) {
            printf("Via operator bool : %d\n", *maybeInt);
        }

        // Réinitialisation
        maybeInt = NkNullOpt;
    }

    // Exemple 2 : Accès sécurisé avec ValueOr et GetIf
    void SafeAccess() {
        NkOptional<nk_string> username;

        // ValueOr : fallback par copie
        nk_string name = username.ValueOr("Anonymous");

        // GetIf : accès via pointeur sécurisé
        if (nk_string* ptr = username.GetIf()) {
            printf("Username: %s\n", ptr->c_str());
        } else {
            printf("Username non défini\n");
        }

        // Après assignation
        username.Emplace("Alice");
        if (auto* ptr = username.GetIf()) {
            printf("Maintenant : %s\n", ptr->c_str());
        }
    }

    // Exemple 3 : Sémantique de copie et déplacement
    void CopyMoveSemantics() {
        // Copie
        NkOptional<int> opt1;
        opt1.Emplace(100);
        NkOptional<int> opt2 = opt1;  // Copie de la valeur

        // Déplacement
        NkOptional<nk_string> src;
        src.Emplace("HeavyData...");
        NkOptional<nk_string> dst = NKENTSEU_MOVE(src);
        // dst contient "HeavyData...", src est vide

        // Swap
        NkOptional<int> a, b;
        a.Emplace(1);
        b.Emplace(2);
        a.Swap(b);  // a=2, b=1
    }

    // Exemple 4 : Construction in-place avec Emplace
    void InPlaceConstruction() {
        // Évite une copie temporaire
        NkOptional<nk_string> opt;
        opt.Emplace(10, 'x');  // Construit nk_string(10, 'x') in-place

        // Avec struct personnalisé
        struct Point { int x, y; Point(int x_, int y_) : x(x_), y(y_) {} };
        NkOptional<Point> maybePoint;
        maybePoint.Emplace(10, 20);  // Appelle Point(10, 20) directement
    }

    // Exemple 5 : Gestion d'erreurs avec optionnels
    NkOptional<int> ParseInteger(const char* str) {
        if (!str || !*str) return NkNullOpt;
        
        int value = 0;
        while (*str >= '0' && *str <= '9') {
            value = value * 10 + (*str - '0');
            ++str;
        }
        return (*str == '\0') ? NkOptional<int>(value) : NkNullOpt;
    }

    void ErrorHandlingExample() {
        const char* inputs[] = {"123", "abc", "", "456"};
        for (const char* input : inputs) {
            auto result = ParseInteger(input);
            printf("'%s' -> %s\n", input, 
                   result.HasValue() ? "succès" : "échec");
        }
    }
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================