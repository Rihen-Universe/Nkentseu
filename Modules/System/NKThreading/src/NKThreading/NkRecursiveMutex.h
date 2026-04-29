//
// NkRecursiveMutex.h
// =============================================================================
// Description :
//   Fichier de compatibilité legacy fournissant un alias de type pour
//   rediriger les anciennes références vers l'implémentation moderne.
//
// Objectif :
//   - Maintenir la compatibilité avec le code existant utilisant l'ancien
//     namespace nkentseu::entseu pour la primitive NkRecursiveMutex
//   - Faciliter la migration progressive vers nkentseu::threading
//   - Éviter les breaking changes dans les projets dépendants
//
// Architecture :
//   - Simple redirection via alias using (zero overhead à l'exécution)
//   - Inclusion unique de NkMutex.h qui contient la définition réelle
//   - Aucune implémentation supplémentaire : pure couche de compatibilité
//
// Type redirigé :
//   nkentseu::entseu::NkRecursiveMutex → nkentseu::threading::NkRecursiveMutex
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_THREADING_NKRECURSIVEMUTEX_H__
#define __NKENTSEU_THREADING_NKRECURSIVEMUTEX_H__

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion du header principal qui contient la définition réelle.
    // Ce fichier ne fait que rediriger via un alias, donc une seule inclusion.
    #include "NKThreading/NkMutex.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : NAMESPACE PRINCIPAL
    // -------------------------------------------------------------------------
    namespace nkentseu {

        // -----------------------------------------------------------------
        // ALIAS LEGACY : NkRecursiveMutex
        // -----------------------------------------------------------------
        /// @brief Alias de compatibilité pour NkRecursiveMutex.
        /// @ingroup LegacyCompatibility
        /// @deprecated Utiliser directement nkentseu::threading::NkRecursiveMutex
        /// @note Cet alias est résolu à la compilation : zero runtime overhead.
        /// @note Aucune duplication de code : pointe vers l'implémentation moderne.
        /// @warning Les mutex réentrants ont un overhead de performance ;
        ///          privilégier NkMutex quand la récursivité n'est pas requise.
        using NkRecursiveMutex = ::nkentseu::threading::NkRecursiveMutex;
    }

#endif

// =============================================================================
// EXEMPLES D'UTILISATION (documentation - ne pas compiler)
// =============================================================================
/*
    // ---------------------------------------------------------------------
    // Exemple 1 : Code legacy utilisant l'ancien alias
    // ---------------------------------------------------------------------
    // Ce code fonctionne toujours grâce à l'alias de compatibilité,
    // mais générera un warning de dépréciation si configuré.

    #include <NKThreading/NkRecursiveMutex.h>  // Inclut l'alias legacy
    using namespace nkentseu::entseu;           // Ancien namespace (déprécié)

    void LegacyRecursiveFunction(int depth);

    void LegacyRecursiveFunction(int depth)
    {
        static NkRecursiveMutex mutex;  // Résolu vers threading::NkRecursiveMutex

        NkScopedLock lock(mutex);       // Lock récursif autorisé

        if (depth > 0) {
            // Appel récursif : le même thread peut re-locker sans deadlock
            LegacyRecursiveFunction(depth - 1);
        }

        // Traitement protégé...
    }

    // ---------------------------------------------------------------------
    // Exemple 2 : Nouveau code recommandé (sans alias)
    // ---------------------------------------------------------------------
    // Code moderne utilisant directement le namespace cible.

    #include <NKThreading/NkMutex.h>     // Header principal recommandé
    using namespace nkentseu::threading; // Nouveau namespace (recommandé)

    void ModernRecursiveFunction(int depth)
    {
        static NkRecursiveMutex mutex;   // Usage direct, pas d'alias

        NkScopedLock lock(mutex);

        if (depth > 0) {
            ModernRecursiveFunction(depth - 1);  // OK : récursivité gérée
        }

        // Traitement protégé...
    }

    // ---------------------------------------------------------------------
    // Exemple 3 : Quand utiliser NkRecursiveMutex vs NkMutex
    // ---------------------------------------------------------------------
    #include <NKThreading/NkMutex.h>

    // ✅ CAS LÉGITIME : API publique appelant des méthodes internes protégées
    class DataProcessor {
    public:
        void ProcessBatch(const std::vector<Item>& items)
        {
            NkScopedLock lock(mMutex);  // Lock externe
            for (const auto& item : items) {
                ProcessItem(item);      // Appel interne : re-lock autorisé
            }
        }

    private:
        void ProcessItem(const Item& item)
        {
            NkScopedLock lock(mMutex);  // OK avec NkRecursiveMutex
            // Transformation de l'item...
        }

        NkRecursiveMutex mMutex;  // Justifié par l'API imbriquée
    };

    // ❌ CAS À ÉVITER : Sections critiques simples (préférer NkMutex)
    class SimpleCounter {
    public:
        void Increment()
        {
            NkScopedLock lock(mMutex);  // NkMutex suffit ici !
            ++mValue;
        }

        int Get() const
        {
            NkScopedLock lock(mMutex);
            return mValue;
        }

    private:
        mutable NkMutex mMutex;  // Plus rapide que NkRecursiveMutex
        int mValue;
    };

    // ---------------------------------------------------------------------
    // Exemple 4 : Migration progressive avec macro conditionnelle
    // ---------------------------------------------------------------------
    // Stratégie pour migrer un codebase sans breaking change.

    // Dans un header commun du projet :
    #ifndef NKENTSEU_DISABLE_LEGACY_ALIASES
        #define NKENTSEU_USE_LEGACY_THREADING 1
    #endif

    // Dans le code client :
    #if NKENTSEU_USE_LEGACY_THREADING
        namespace sync = nkentseu::entseu;      // Legacy pendant transition
    #else
        namespace sync = nkentseu::threading;   // Moderne après migration
    #endif

    // Code agnostique au namespace réel :
    void ThreadSafeOperation()
    {
        static sync::NkRecursiveMutex mutex;  // Fonctionne dans les deux cas
        NkScopedLock lock(mutex);
        // ... opération protégée ...
    }

    // Une fois la migration complète, désactiver les aliases :
    // add_definitions(-DNKENTSEU_DISABLE_LEGACY_ALIASES) dans CMake

    // ---------------------------------------------------------------------
    // Exemple 5 : Vérification de type pour debugging de migration
    // ---------------------------------------------------------------------
    // Utiliser static_assert pour s'assurer que l'alias pointe bien
    // vers le type cible attendu pendant la phase de transition.

    #include <NKThreading/NkRecursiveMutex.h>
    #include <NKThreading/NkMutex.h>
    #include <type_traits>

    static_assert(
        std::is_same_v<
            nkentseu::entseu::NkRecursiveMutex,
            nkentseu::threading::NkRecursiveMutex
        >,
        "Legacy alias NkRecursiveMutex doit pointer vers threading::NkRecursiveMutex"
    );

    // ---------------------------------------------------------------------
    // Exemple 6 : Template générique acceptant les deux namespaces
    // ---------------------------------------------------------------------
    // Écrire du code template qui fonctionne avec les types des deux
    // namespaces pour faciliter la transition progressive.

    template<typename MutexType>
    void ProtectedCall(MutexType& mutex, std::function<void()> operation)
    {
        // Fonctionne avec entseu::NkRecursiveMutex
        // ET avec threading::NkRecursiveMutex
        // grâce à l'interface commune et aux aliases compatibles
        NkScopedLock lock(mutex);
        operation();
    }

    // Usage avec legacy alias :
    nkentseu::entseu::NkRecursiveMutex legacyMutex;
    ProtectedCall(legacyMutex, []() { DoWork(); });  // OK

    // Usage avec type moderne :
    nkentseu::threading::NkRecursiveMutex modernMutex;
    ProtectedCall(modernMutex, []() { DoWork(); });  // OK également

    // ---------------------------------------------------------------------
    // Exemple 7 : Configuration CMake pour gérer la dépréciation
    // ---------------------------------------------------------------------
    # CMakeLists.txt - Gestion des warnings de dépréciation

    # Option pour activer/désactiver les warnings de migration
    option(NKENTSEU_WARN_ON_LEGACY_USAGE
           "Emit warnings when using legacy namespace aliases"
           ON)

    if(NKENTSEU_WARN_ON_LEGACY_USAGE)
        # GCC/Clang : utiliser pragma pour marquer la dépréciation
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(your_target PRIVATE
                -Wdeprecated-declarations
            )
        endif()

        # MSVC : warning level personnalisé
        if(MSVC)
            target_compile_options(your_target PRIVATE
                /wd4996  # Si vous gérez la dépréciation manuellement
            )
        endif()
    endif()

    # Définir un macro pour tracer l'usage legacy dans le code
    target_compile_definitions(your_target PRIVATE
        NKENTSEU_LEGACY_MUTEX_ALIAS_USED
    )

    // ---------------------------------------------------------------------
    // Exemple 8 : Bonnes pratiques vs anti-patterns
    // ---------------------------------------------------------------------
    // ✅ BON : Documenter clairement la dépréciation
    /// @deprecated Utiliser nkentseu::threading::NkRecursiveMutex à la place
    using NkRecursiveMutex = ...;

    // ✅ BON : Fournir un chemin de migration clair dans la doc
    // Voir guide/migration-threading.md pour les étapes de transition

    // ✅ BON : Permettre de désactiver les aliases via macro de build
    #ifndef NKENTSEU_DISABLE_LEGACY_ALIASES
        // ... définitions des aliases ...
    #endif

    // ❌ MAUVAIS : Supprimer brutalement les aliases sans préavis
    // Cela casse la compilation des projets dépendants sans avertissement

    // ❌ MAUVAIS : Dupliquer le code au lieu d'utiliser des alias
    // Cela augmente la maintenance et le risque d'incohérences

    // ❌ MAUVAIS : Utiliser NkRecursiveMutex quand NkMutex suffit
    // Overhead inutile de 2-3x sur les opérations de lock/unlock simples

    // ---------------------------------------------------------------------
    // Exemple 9 : Testing de compatibilité legacy
    // ---------------------------------------------------------------------
    // Tests unitaires pour vérifier que l'alias fonctionne correctement.

    #include <gtest/gtest.h>
    #include <NKThreading/NkRecursiveMutex.h>

    TEST(LegacyAliasTest, NkRecursiveMutexTypeEquivalence)
    {
        // Vérifie que l'alias legacy produit le même type que le target
        using LegacyMutex = nkentseu::entseu::NkRecursiveMutex;
        using ModernMutex = nkentseu::threading::NkRecursiveMutex;

        static_assert(std::is_same_v<LegacyMutex, ModernMutex>,
                      "Alias legacy doit être équivalent au type moderne");

        // Test fonctionnel : comportement récursif identique
        LegacyMutex legacy;
        legacy.Lock();      // Premier lock
        legacy.Lock();      // Re-lock par le même thread : OK
        legacy.Unlock();    // Décrémente compteur
        legacy.Unlock();    // Libère réellement le mutex

        EXPECT_TRUE(legacy.TryLock());  // Devrait réussir maintenant
        legacy.Unlock();
    }

    // ---------------------------------------------------------------------
    // Exemple 10 : Documentation de migration pour les utilisateurs
    // ---------------------------------------------------------------------
    /\*
    ============================================================================
    GUIDE DE MIGRATION : nkentseu::entseu → nkentseu::threading
    ============================================================================

    Contexte :
    ----------
    Les primitives de synchronisation (Mutex, RecursiveMutex, etc.) ont été
    déplacées du namespace nkentseu::entseu vers nkentseu::threading pour
    une meilleure organisation et cohérence avec les autres modules.

    Impact :
    --------
    - Code existant : continue de fonctionner via alias de compatibilité
    - Nouveaux projets : doivent utiliser directement nkentseu::threading
    - Warning optionnel : configurable via NKENTSEU_WARN_ON_LEGACY_USAGE

    Étapes de migration :
    ---------------------
    1. [Optionnel] Activer les warnings pour identifier les usages legacy :
       add_definitions(-DNKENTSEU_WARN_ON_LEGACY_USAGE)

    2. Remplacer les includes :
       AVANT : #include <NKThreading/NkRecursiveMutex.h>
       APRÈS : #include <NKThreading/NkMutex.h>  // Contient NkRecursiveMutex

    3. Mettre à jour les using namespace :
       AVANT : using namespace nkentseu::entseu;
       APRÈS : using namespace nkentseu::threading;

    4. Ou utiliser des qualifications complètes :
       AVANT : entseu::NkRecursiveMutex
       APRÈS : threading::NkRecursiveMutex

    5. Tester la compilation et l'exécution (tests de récursivité)
    6. [Optionnel] Désactiver les aliases legacy une fois la migration complète :
       add_definitions(-DNKENTSEU_DISABLE_LEGACY_ALIASES)

    Support :
    ---------
    - Voir : docs/migration-threading.md pour le guide détaillé
    - Outil : scripts/migrate_threading_namespace.py pour l'automatisation
    - Help  : ouvrir un ticket si problème de migration

    Calendrier :
    ------------
    - v2.x : Aliases disponibles avec warnings optionnels
    - v3.0 : Aliases marqués comme deprecated (warnings par défaut)
    - v4.0 : Aliases supprimés (breaking change)

    ============================================================================
    *\/
*/
// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================