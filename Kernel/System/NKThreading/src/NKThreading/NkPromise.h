//
// NkPromise.h
// =============================================================================
// Description :
//   Fichier de compatibilité legacy fournissant des alias de types pour
//   rediriger les anciennes références vers les implémentations modernes.
//
// Objectif :
//   - Maintenir la compatibilité avec le code existant utilisant l'ancien
//     namespace nkentseu::entseu pour les primitives async Future/Promise
//   - Faciliter la migration progressive vers nkentseu::threading
//   - Éviter les breaking changes dans les projets dépendants
//
// Architecture :
//   - Simple redirection via alias template (zero overhead à l'exécution)
//   - Inclusion unique de NkFuture.h qui déclare les types cibles
//   - Aucune implémentation supplémentaire : pure couche de compatibilité
//
// Types redirigés :
//   nkentseu::entseu::NkPromise<T> → nkentseu::threading::NkPromise<T>
//   nkentseu::entseu::NkFuture<T>  → nkentseu::threading::NkFuture<T>
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_THREADING_NKPROMISE_H__
#define __NKENTSEU_THREADING_NKPROMISE_H__

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion du header principal qui contient les définitions réelles.
    // Ce fichier ne fait que rediriger via des alias, donc une seule inclusion.
    #include "NKThreading/NkFuture.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : NAMESPACE PRINCIPAL
    // -------------------------------------------------------------------------
    namespace nkentseu {

            // -----------------------------------------------------------------
            // ALIAS LEGACY : NkPromise<T>
            // -----------------------------------------------------------------
            /// @brief Alias de compatibilité pour NkPromise<T>.
            /// @ingroup LegacyCompatibility
            /// @tparam T Type de la valeur produite par le promise.
            /// @deprecated Utiliser directement nkentseu::threading::NkPromise<T>
            /// @note Cet alias est résolu à la compilation : zero runtime overhead.
            /// @note Aucun code supplémentaire n'est généré par cet alias.
            template<typename T>
            using NkPromise = ::nkentseu::threading::NkPromise<T>;

            // -----------------------------------------------------------------
            // ALIAS LEGACY : NkFuture<T>
            // -----------------------------------------------------------------
            /// @brief Alias de compatibilité pour NkFuture<T>.
            /// @ingroup LegacyCompatibility
            /// @tparam T Type de la valeur consommée via le future.
            /// @deprecated Utiliser directement nkentseu::threading::NkFuture<T>
            /// @note Cet alias est résolu à la compilation : zero runtime overhead.
            /// @note La spécialisation NkFuture<void> est également redirigée.
            template<typename T>
            using NkFuture = ::nkentseu::threading::NkFuture<T>;
    }

#endif

// =============================================================================
// EXEMPLES D'UTILISATION (documentation - ne pas compiler)
// =============================================================================
/*
    // ---------------------------------------------------------------------
    // Exemple 1 : Code legacy utilisant les anciens aliases
    // ---------------------------------------------------------------------
    // Ce code fonctionne toujours grâce aux alias de compatibilité,
    // mais générera un warning de dépréciation si configuré.

    #include <NKThreading/NkPromise.h>  // Inclut les aliases legacy
    using namespace nkentseu::entseu;    // Ancien namespace (déprécié)

    void LegacyFunction()
    {
        NkPromise<int> promise;          // Résolu vers threading::NkPromise<int>
        NkFuture<int> future = promise.GetFuture();

        std::thread worker([&promise]() {
            promise.SetValue(42);
        });

        int result = future.Get();
        worker.join();
    }

    // ---------------------------------------------------------------------
    // Exemple 2 : Nouveau code recommandé (sans alias)
    // ---------------------------------------------------------------------
    // Code moderne utilisant directement le namespace cible.

    #include <NKThreading/NkFuture.h>    // Header principal recommandé
    using namespace nkentseu::threading; // Nouveau namespace (recommandé)

    void ModernFunction()
    {
        NkPromise<std::string> promise;  // Usage direct, pas d'alias
        NkFuture<std::string> future = promise.GetFuture();

        std::thread worker([&promise]() {
            promise.SetValue("Hello, World!");
        });

        std::string result = future.Get();
        worker.join();
    }

    // ---------------------------------------------------------------------
    // Exemple 3 : Migration progressive dans un projet existant
    // ---------------------------------------------------------------------
    // Stratégie pour migrer un codebase sans breaking change.

    // Étape 1 : Ajouter un macro de contrôle de migration
    // Dans un header commun du projet :
    #ifndef NKENTSEU_DISABLE_LEGACY_ALIASES
        #define NKENTSEU_USE_LEGACY_ASYNC 1
    #endif

    // Étape 2 : Utiliser un namespace alias conditionnel dans le code client
    #if NKENTSEU_USE_LEGACY_ASYNC
        namespace async = nkentseu::entseu;   // Legacy pendant la transition
    #else
        namespace async = nkentseu::threading;  // Moderne après migration
    #endif

    // Étape 3 : Code agnostique au namespace réel
    void AgnosticFunction()
    {
        async::NkPromise<double> promise;  // Fonctionne dans les deux cas
        auto future = promise.GetFuture();
        // ... reste du code inchangé ...
    }

    // Étape 4 : Une fois toute la codebase migrée, désactiver les aliases
    // Dans CMakeLists.txt ou config du projet :
    // add_definitions(-DNKENTSEU_DISABLE_LEGACY_ALIASES)

    // ---------------------------------------------------------------------
    // Exemple 4 : Vérification de type pour debugging de migration
    // ---------------------------------------------------------------------
    // Utiliser static_assert pour s'assurer que les alias pointent bien
    // vers les types cibles attendus pendant la phase de transition.

    #include <NKThreading/NkPromise.h>
    #include <NKThreading/NkFuture.h>
    #include <type_traits>

    static_assert(
        std::is_same_v<
            nkentseu::entseu::NkPromise<int>,
            nkentseu::threading::NkPromise<int>
        >,
        "Legacy alias NkPromise<int> doit pointer vers threading::NkPromise<int>"
    );

    static_assert(
        std::is_same_v<
            nkentseu::entseu::NkFuture<void>,
            nkentseu::threading::NkFuture<void>
        >,
        "Legacy alias NkFuture<void> doit pointer vers threading::NkFuture<void>"
    );

    // ---------------------------------------------------------------------
    // Exemple 5 : Template générique fonctionnant avec les deux namespaces
    // ---------------------------------------------------------------------
    // Écrire du code template qui accepte les types des deux namespaces
    // pour faciliter la transition progressive.

    template<typename PromiseType>
    void ProcessAsync(PromiseType&& promise, int value)
    {
        // Fonctionne avec nkentseu::entseu::NkPromise<T>
        // ET avec nkentseu::threading::NkPromise<T>
        // grâce à l'interface commune et aux alias compatibles
        promise.SetValue(value);
    }

    // Usage avec legacy alias :
    nkentseu::entseu::NkPromise<int> legacyPromise;
    ProcessAsync(legacyPromise, 100);  // OK

    // Usage avec type moderne :
    nkentseu::threading::NkPromise<int> modernPromise;
    ProcessAsync(modernPromise, 200);  // OK également

    // ---------------------------------------------------------------------
    // Exemple 6 : Configuration CMake pour gérer la dépréciation
    // ---------------------------------------------------------------------
    # CMakeLists.txt - Gestion des warnings de dépréciation

    # Option pour activer/désactiver les warnings de migration
    option(NKENTSEU_WARN_ON_LEGACY_USAGE
           "Emit warnings when using legacy namespace aliases"
           ON)

    if(NKENTSEU_WARN_ON_LEGACY_USAGE)
        # GCC/Clang : warning personnalisé via pragma ou attribute
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(your_target PRIVATE
                -Wno-deprecated-declarations  # Si vous gérez la dépréciation manuellement
            )
        endif()

        # MSVC : warning level personnalisé
        if(MSVC)
            target_compile_options(your_target PRIVATE
                /wd4996  # Désactive warning C4996 si géré autrement
            )
        endif()
    endif()

    # Définir un macro pour marquer l'usage legacy dans le code
    target_compile_definitions(your_target PRIVATE
        NKENTSEU_LEGACY_ASYNC_ALIAS_USED
    )

    // ---------------------------------------------------------------------
    // Exemple 7 : Script de migration automatique (conceptuel)
    // ---------------------------------------------------------------------
    // Pseudo-code pour un outil de refactoring automatique.

    /\*
    # migrate_async_namespace.py --dry-run src/

    Règles de remplacement :
    1. nkentseu::entseu::NkPromise< → nkentseu::threading::NkPromise<
    2. nkentseu::entseu::NkFuture<  → nkentseu::threading::NkFuture<
    3. using namespace nkentseu::entseu; → using namespace nkentseu::threading;
    4. #include <NKThreading/NkPromise.h> → #include <NKThreading/NkFuture.h>

    Exclusions (ne pas modifier) :
    - Commentaires contenant les anciens noms
    - Strings littérales contenant les anciens noms
    - Code dans des blocs #if NKENTSEU_USE_LEGACY_ASYNC

    Exécution :
    $ python migrate_async_namespace.py --apply src/ include/
    [INFO] 42 fichiers analysés
    [INFO] 156 occurrences remplacées
    [INFO] 3 fichiers exclus (guard legacy)
    [SUCCESS] Migration complète. Vérifiez manuellement avant commit.
    *\/

    // ---------------------------------------------------------------------
    // Exemple 8 : Bonnes pratiques pour la gestion des aliases legacy
    // ---------------------------------------------------------------------
    // ✅ BON : Documenter clairement la dépréciation
    /// @deprecated Utiliser nkentseu::threading::NkPromise<T> à la place
    template<typename T> using NkPromise = ...;

    // ✅ BON : Fournir un chemin de migration clair dans la doc
    // Voir guide/migration-async.md pour les étapes de transition

    // ✅ BON : Permettre de désactiver les aliases via macro de build
    #ifndef NKENTSEU_DISABLE_LEGACY_ALIASES
        // ... définitions des aliases ...
    #endif

    // ❌ MAUVAIS : Supprimer brutalement les aliases sans préavis
    // Cela casse la compilation des projets dépendants sans avertissement

    // ❌ MAUVAIS : Dupliquer le code au lieu d'utiliser des alias
    // Cela augmente la maintenance et le risque d'incohérences

    // ❌ MAUVAIS : Oublier de rediriger les spécialisations (ex: void)
    // template<> using NkFuture<void> = ...;  // Nécessaire aussi !

    // ---------------------------------------------------------------------
    // Exemple 9 : Testing de compatibilité legacy
    // ---------------------------------------------------------------------
    // Tests unitaires pour vérifier que les aliases fonctionnent correctement.

    #include <gtest/gtest.h>
    #include <NKThreading/NkPromise.h>

    TEST(LegacyAliasTest, NkPromiseTypeEquivalence)
    {
        // Vérifie que l'alias legacy produit le même type que le target
        using LegacyPromise = nkentseu::entseu::NkPromise<int>;
        using ModernPromise = nkentseu::threading::NkPromise<int>;

        static_assert(std::is_same_v<LegacyPromise, ModernPromise>,
                      "Alias legacy doit être équivalent au type moderne");

        // Test fonctionnel : les deux doivent être utilisables interchangeably
        LegacyPromise legacy;
        ModernPromise modern = std::move(legacy);  // Move doit fonctionner
        EXPECT_TRUE(modern.GetFuture().IsReady() == false);
    }

    TEST(LegacyAliasTest, NkFutureVoidSpecialization)
    {
        // Vérifie que la spécialisation void est également redirigée
        using LegacyFutureVoid = nkentseu::entseu::NkFuture<void>;
        using ModernFutureVoid = nkentseu::threading::NkFuture<void>;

        static_assert(std::is_same_v<LegacyFutureVoid, ModernFutureVoid>,
                      "Alias legacy void doit pointer vers la spécialisation moderne");
    }

    // ---------------------------------------------------------------------
    // Exemple 10 : Documentation de migration pour les utilisateurs
    // ---------------------------------------------------------------------
    /*
    ============================================================================
    GUIDE DE MIGRATION : nkentseu::entseu → nkentseu::threading
    ============================================================================

    Contexte :
    ----------
    Les primitives async (Future/Promise) ont été déplacées du namespace
    nkentseu::entseu vers nkentseu::threading pour une meilleure organisation
    et cohérence avec les autres modules de threading.

    Impact :
    --------
    - Code existant : continue de fonctionner via aliases de compatibilité
    - Nouveaux projets : doivent utiliser directement nkentseu::threading
    - Warning optionnel : configurable via NKENTSEU_WARN_ON_LEGACY_USAGE

    Étapes de migration :
    ---------------------
    1. [Optionnel] Activer les warnings pour identifier les usages legacy :
       add_definitions(-DNKENTSEU_WARN_ON_LEGACY_USAGE)

    2. Remplacer les includes :
       AVANT : #include <NKThreading/NkPromise.h>
       APRÈS : #include <NKThreading/NkFuture.h>

    3. Mettre à jour les using namespace :
       AVANT : using namespace nkentseu::entseu;
       APRÈS : using namespace nkentseu::threading;

    4. Ou utiliser des qualifications complètes :
       AVANT : entseu::NkPromise<T>
       APRÈS : threading::NkPromise<T>

    5. Tester la compilation et l'exécution
    6. [Optionnel] Désactiver les aliases legacy une fois la migration complète :
       add_definitions(-DNKENTSEU_DISABLE_LEGACY_ALIASES)

    Support :
    ---------
    - Voir : docs/migration-async.md pour le guide détaillé
    - Outil : scripts/migrate_async_namespace.py pour l'automatisation
    - Help  : ouvrir un ticket si problème de migration

    Calendrier :
    ------------
    - v2.x : Aliases disponibles avec warnings optionnels
    - v3.0 : Aliases marqués comme deprecated (warnings par défaut)
    - v4.0 : Aliases supprimés (breaking change)

    ============================================================================
    */

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================