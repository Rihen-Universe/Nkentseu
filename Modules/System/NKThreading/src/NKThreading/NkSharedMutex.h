//
// NkSharedMutex.h
// =============================================================================
// Description :
//   Fichier d'alias fournissant des noms familiers pour le verrou lecteur/rédacteur.
//   Redirige vers l'implémentation principale NkReaderWriterLock pour cohérence.
//
// Objectif :
//   - Fournir des alias std::like (SharedMutex, SharedLock, UniqueLock)
//   - Faciliter la migration depuis std::shared_mutex et code legacy
//   - Maintenir une API intuitive tout en utilisant l'implémentation optimisée NK
//
// Architecture :
//   - Pure couche d'alias : zero overhead à l'exécution
//   - Inclusion unique de NkReaderWriterLock.h qui contient l'implémentation
//   - Support dual-namespace : nkentseu::threading (moderne) + nkentseu::entseu (legacy)
//
// Alias exposés :
//   nkentseu::threading::NkSharedMutex → NkReaderWriterLock
//   nkentseu::threading::NkSharedLock  → NkReadLock
//   nkentseu::threading::NkUniqueLock  → NkWriteLock
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_THREADING_SHAREDMUTEX_H__
#define __NKENTSEU_THREADING_SHAREDMUTEX_H__

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion du header principal contenant l'implémentation réelle.
    // Ce fichier ne fait que définir des alias, donc une seule inclusion suffit.
    #include "NKThreading/Synchronization/NkReaderWriterLock.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : NAMESPACE PRINCIPAL
    // -------------------------------------------------------------------------
    namespace nkentseu {

        // =====================================================================
        // SOUS-NAMESPACE : threading (namespace moderne recommandé)
        // =====================================================================
        namespace threading {

            // -----------------------------------------------------------------
            // ALIAS : NkSharedMutex
            // -----------------------------------------------------------------
            /// @brief Alias pour NkReaderWriterLock avec sémantique shared_mutex.
            /// @ingroup ThreadingPrimitives
            /// @note Permet un accès concurrent en lecture, exclusif en écriture.
            /// @note Équivalent fonctionnel à std::shared_mutex mais avec garanties NK.
            /// @see NkReaderWriterLock pour l'implémentation détaillée.
            using NkSharedMutex = NkReaderWriterLock;

            // -----------------------------------------------------------------
            // ALIAS : NkSharedLock
            // -----------------------------------------------------------------
            /// @brief Alias pour NkReadLock avec sémantique shared_lock.
            /// @ingroup ThreadingUtilities
            /// @note Guard RAII pour verrouillage en lecture (mode partagé).
            /// @note Multiple NkSharedLock peuvent coexister sur le même mutex.
            /// @see NkReadLock pour l'implémentation détaillée.
            using NkSharedLock = NkReadLock;

            // -----------------------------------------------------------------
            // ALIAS : NkUniqueLock
            // -----------------------------------------------------------------
            /// @brief Alias pour NkWriteLock avec sémantique unique_lock.
            /// @ingroup ThreadingUtilities
            /// @note Guard RAII pour verrouillage en écriture (mode exclusif).
            /// @note Un seul NkUniqueLock peut être détenu à la fois (exclusion mutuelle).
            /// @see NkWriteLock pour l'implémentation détaillée.
            using NkUniqueLock = NkWriteLock;

        }
    }

    // -------------------------------------------------------------------------
    // SECTION 3 : NAMESPACE LEGACY (compatibilité rétroactive)
    // -------------------------------------------------------------------------
    // Ces alias dans nkentseu::entseu sont conservés uniquement pour la
    // rétrocompatibilité avec le code existant. Tout nouveau code doit
    // utiliser directement le namespace nkentseu::threading.
    // =====================================================================
    namespace nkentseu {

            // -----------------------------------------------------------------
            // ALIAS LEGACY : NkSharedMutex
            // -----------------------------------------------------------------
            /// @brief Alias de compatibilité legacy pour NkSharedMutex.
            /// @ingroup LegacyCompatibility
            /// @deprecated Utiliser nkentseu::threading::NkSharedMutex directement.
            /// @note Résolu à la compilation : zero runtime overhead.
            using NkSharedMutex = ::nkentseu::threading::NkSharedMutex;

            // -----------------------------------------------------------------
            // ALIAS LEGACY : NkSharedLock
            // -----------------------------------------------------------------
            /// @brief Alias de compatibilité legacy pour NkSharedLock.
            /// @ingroup LegacyCompatibility
            /// @deprecated Utiliser nkentseu::threading::NkSharedLock directement.
            /// @note Résolu à la compilation : zero runtime overhead.
            using NkSharedLock = ::nkentseu::threading::NkSharedLock;

            // -----------------------------------------------------------------
            // ALIAS LEGACY : NkUniqueLock
            // -----------------------------------------------------------------
            /// @brief Alias de compatibilité legacy pour NkUniqueLock.
            /// @ingroup LegacyCompatibility
            /// @deprecated Utiliser nkentseu::threading::NkUniqueLock directement.
            /// @note Résolu à la compilation : zero runtime overhead.
            using NkUniqueLock = ::nkentseu::threading::NkUniqueLock;
    }

#endif

// =============================================================================
// EXEMPLES D'UTILISATION (documentation - ne pas compiler)
// =============================================================================
/*
    // ---------------------------------------------------------------------
    // Exemple 1 : Usage basique avec NkSharedMutex (lecture/écriture)
    // ---------------------------------------------------------------------
    #include <NKThreading/NkSharedMutex.h>
    using namespace nkentseu::threading;

    NkSharedMutex dataMutex;
    std::map<std::string, int> sharedData;

    // Lecture concurrente : multiple threads peuvent lire simultanément
    int LookupValue(const std::string& key)
    {
        NkSharedLock lock(dataMutex);  // Verrouillage partagé (lecture)
        auto it = sharedData.find(key);
        return (it != sharedData.end()) ? it->second : -1;
        // Déverrouillage automatique à la sortie de portée
    }

    // Écriture exclusive : un seul thread peut modifier à la fois
    void UpdateValue(const std::string& key, int value)
    {
        NkUniqueLock lock(dataMutex);  // Verrouillage exclusif (écriture)
        sharedData[key] = value;
        // Déverrouillage automatique à la sortie de portée
    }

    // ---------------------------------------------------------------------
    // Exemple 2 : Pattern copy-on-write avec upgrade de lock
    // ---------------------------------------------------------------------
    #include <NKThreading/NkSharedMutex.h>

    class CachedConfig {
    public:
        std::string GetValue(const std::string& key) const
        {
            NkSharedLock readLock(mMutex);  // Lecture partagée
            auto it = mCache.find(key);
            if (it != mCache.end()) {
                return it->second;  // Cache hit : retour rapide
            }

            // Cache miss : besoin d'écrire → upgrade vers write lock
            readLock.Unlock();  // Libérer le read lock avant d'acquérir write

            NkUniqueLock writeLock(mMutex);  // Verrouillage exclusif
            // Re-vérifier après acquisition (autre thread a pu écrire)
            it = mCache.find(key);
            if (it != mCache.end()) {
                return it->second;
            }

            // Calculer et insérer la nouvelle valeur
            std::string computed = ComputeExpensiveValue(key);
            mCache[key] = computed;
            return computed;
        }

    private:
        mutable NkSharedMutex mMutex;
        mutable std::map<std::string, std::string> mCache;
    };

    // ---------------------------------------------------------------------
    // Exemple 3 : Migration depuis std::shared_mutex (code legacy)
    // ---------------------------------------------------------------------
    // Avant (code utilisant la STL) :
    // #include <shared_mutex>
    // std::shared_mutex legacyMutex;
    // std::shared_lock<std::shared_mutex> readLock(legacyMutex);
    // std::unique_lock<std::shared_mutex> writeLock(legacyMutex);

    // Après (migration vers NKThreading) :
    #include <NKThreading/NkSharedMutex.h>
    nkentseu::threading::NkSharedMutex nkMutex;  // Remplace std::shared_mutex
    nkentseu::threading::NkSharedLock readLock(nkMutex);   // Remplace std::shared_lock
    nkentseu::threading::NkUniqueLock writeLock(nkMutex);  // Remplace std::unique_lock

    // Avantages de la migration :
    // - Meilleures performances sur certaines plateformes (implémentation optimisée)
    // - Garantie noexcept sur toutes les opérations
    // - Intégration avec le reste de l'écosystème NK (logging, debugging, etc.)
    // - Support uniforme multiplateforme sans dépendance à la STL

    // ---------------------------------------------------------------------
    // Exemple 4 : Lecture préférentielle vs écriture préférentielle
    // ---------------------------------------------------------------------
    #include <NKThreading/NkSharedMutex.h>

    // NkReaderWriterLock (sous-jacent à NkSharedMutex) implémente
    // une politique de lecture préférentielle par défaut :
    // - Les nouveaux lecteurs peuvent acquérir même si des writers attendent
    // - Évite la starvation des lecteurs dans les workloads read-heavy
    //
    // Pour un comportement écriture-préférentiel (si nécessaire) :
    // → Utiliser directement NkReaderWriterLock avec paramètres de politique
    // → Ou implémenter une file d'attente personnalisée côté application

    void ReadHeavyWorkload()
    {
        static NkSharedMutex mutex;

        // Thread lecteur (nombreux) :
        auto reader = []() {
            while (running) {
                NkSharedLock lock(mutex);  // Acquisition rapide si pas de writer actif
                ReadSharedData();
            }
        };

        // Thread writer (rare) :
        auto writer = []() {
            while (running) {
                NkUniqueLock lock(mutex);  // Attend que tous les readers terminent
                UpdateSharedData();
            }
        };
    }

    // ---------------------------------------------------------------------
    // Exemple 5 : Timeout sur acquisition de lock
    // ---------------------------------------------------------------------
    #include <NKThreading/NkSharedMutex.h>

    bool TryUpdateWithTimeout(NkSharedMutex& mutex, nk_uint32 timeoutMs)
    {
        // NkUniqueLock (alias de NkWriteLock) supporte TryLockFor
        NkUniqueLock lock;
        if (!mutex.TryLockFor(lock, timeoutMs)) {
            // Timeout : fallback ou erreur
            LogWarning("Failed to acquire write lock within {}ms", timeoutMs);
            return false;
        }

        // Section critique protégée
        PerformUpdate();
        return true;
        // Déverrouillage automatique par destructeur
    }

    // ---------------------------------------------------------------------
    // Exemple 6 : Transfert de propriété de lock (move semantics)
    // ---------------------------------------------------------------------
    #include <NKThreading/NkSharedMutex.h>

    void ExtendedCriticalSection(NkSharedMutex& mutex)
    {
        NkUniqueLock lock(mutex);  // Acquisition initiale

        if (NeedsExtendedProcessing()) {
            // Transférer le lock à une fonction externe
            ContinueProcessing(std::move(lock));  // 'lock' devient invalide ici
            return;
        }

        // Traitement standard...
    }

    void ContinueProcessing(NkUniqueLock&& lock)
    {
        // 'lock' détient maintenant le mutex transféré
        PerformExtendedOperation();
        // Destruction automatique : Unlock() appelé à la sortie
    }

    // ---------------------------------------------------------------------
    // Exemple 7 : Adoption de lock existant (deferred locking)
    // ---------------------------------------------------------------------
    #include <NKThreading/NkSharedMutex.h>

    void ManualLockingPattern(NkSharedMutex& mutex)
    {
        // Acquisition manuelle (pour logique complexe)
        mutex.LockShared();  // ou LockExclusive() pour écriture

        // Création d'un guard qui "adopte" le lock déjà acquis
        NkSharedLock guard(mutex, nkentseu::threading::AdoptLockTag{});

        // Maintenant RAII s'applique : déverrouillage automatique
        DoProtectedWork();
        // guard.Unlock() appelé automatiquement à la sortie
    }

    // ---------------------------------------------------------------------
    // Exemple 8 : Bonnes pratiques et anti-patterns
    // ---------------------------------------------------------------------
    // ✅ BON : Utiliser NkSharedLock pour les lectures (concurrence maximale)
    int SafeRead(NkSharedMutex& mutex, const Data& key)
    {
        NkSharedLock lock(mutex);  // Multiple readers OK
        return Lookup(key);
    }

    // ❌ MAUVAIS : Utiliser NkUniqueLock pour une lecture (exclusion inutile)
    int InefficientRead(NkSharedMutex& mutex, const Data& key)
    {
        NkUniqueLock lock(mutex);  // ❌ Bloque tous les autres readers !
        return Lookup(key);
    }

    // ✅ BON : Minimiser la durée des write locks
    void EfficientWrite(NkSharedMutex& mutex)
    {
        // Pré-calculer hors section critique
        auto newData = ExpensiveComputation();

        {
            NkUniqueLock lock(mutex);  // Scope minimal pour write lock
            ApplyUpdate(newData);      // Opération rapide protégée
        }  // Libération immédiate

        ContinueWithUnprotectedWork();  // Hors section critique
    }

    // ❌ MAUVAIS : Garder un write lock pendant un traitement long
    void InefficientWrite(NkSharedMutex& mutex)
    {
        NkUniqueLock lock(mutex);  // ❌ Lock acquis trop tôt
        auto newData = ExpensiveComputation();  // Lock maintenu inutilement !
        ApplyUpdate(newData);
    }

    // ✅ BON : Vérifier IsLocked() pour debugging (pas pour logique métier)
    void DebugLockState(const NkSharedMutex& mutex)
    {
        if (mutex.HasReaders()) {
            LogDebug("Mutex has {} active readers", mutex.GetReaderCount());
        }
        if (mutex.HasWriter()) {
            LogDebug("Mutex has active writer");
        }
    }

    // ❌ MAUVAIS : Utiliser GetCount() pour prendre des décisions de sync
    void RiskyLogic(NkSharedMutex& mutex)
    {
        if (mutex.GetReaderCount() == 0) {  // ❌ Valeur peut changer immédiatement !
            // Race condition : un reader peut acquérir entre le check et l'action
            PerformExclusiveAction();
        }
    }

    // ---------------------------------------------------------------------
    // Exemple 9 : Intégration avec conteneurs thread-safe
    // ---------------------------------------------------------------------
    #include <NKThreading/NkSharedMutex.h>

    template<typename Key, typename Value>
    class ConcurrentMap {
    public:
        Optional<Value> Get(const Key& key) const
        {
            NkSharedLock lock(mMutex);  // Lecture partagée
            auto it = mMap.find(key);
            return (it != mMap.end()) ? Optional<Value>(it->second) : nullopt;
        }

        bool Insert(const Key& key, const Value& value)
        {
            NkUniqueLock lock(mMutex);  // Écriture exclusive
            return mMap.emplace(key, value).second;
        }

        size_t Size() const
        {
            NkSharedLock lock(mMutex);
            return mMap.size();
        }

    private:
        mutable NkSharedMutex mMutex;
        std::unordered_map<Key, Value> mMap;
    };

    // ---------------------------------------------------------------------
    // Exemple 10 : Configuration CMake pour migration progressive
    // ---------------------------------------------------------------------
    # CMakeLists.txt - Gestion des aliases legacy

    # Option pour activer/désactiver les warnings de dépréciation
    option(NKENTSEU_WARN_ON_LEGACY_THREADING
           "Emit warnings when using nkentseu::entseu namespace"
           ON)

    if(NKENTSEU_WARN_ON_LEGACY_THREADING)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(your_target PRIVATE
                -Wdeprecated-declarations
            )
        endif()
        if(MSVC)
            target_compile_options(your_target PRIVATE
                /wd4996
            )
        endif()
    endif()

    # Macro pour tracer l'usage legacy dans le code
    target_compile_definitions(your_target PRIVATE
        NKENTSEU_LEGACY_SHARED_MUTEX_USED
    )

    # Guide de migration dans les commentaires du code :
    // AVANT : #include <NKThreading/NkSharedMutex.h> + using namespace nkentseu::entseu;
    // APRÈS : #include <NKThreading/NkSharedMutex.h> + using namespace nkentseu::threading;

    // ---------------------------------------------------------------------
    // Exemple 11 : Testing de compatibilité des aliases
    // ---------------------------------------------------------------------
    #include <gtest/gtest.h>
    #include <NKThreading/NkSharedMutex.h>
    #include <type_traits>

    TEST(SharedMutexAliasTest, TypeEquivalence)
    {
        // Vérifie que les aliases pointent vers les types cibles
        static_assert(std::is_same_v<
            nkentseu::threading::NkSharedMutex,
            nkentseu::threading::NkReaderWriterLock
        >, "NkSharedMutex doit être alias de NkReaderWriterLock");

        static_assert(std::is_same_v<
            nkentseu::threading::NkSharedLock,
            nkentseu::threading::NkReadLock
        >, "NkSharedLock doit être alias de NkReadLock");

        static_assert(std::is_same_v<
            nkentseu::threading::NkUniqueLock,
            nkentseu::threading::NkWriteLock
        >, "NkUniqueLock doit être alias de NkWriteLock");
    }

    TEST(SharedMutexAliasTest, LegacyCompatibility)
    {
        // Vérifie que les aliases legacy fonctionnent aussi
        static_assert(std::is_same_v<
            nkentseu::entseu::NkSharedMutex,
            nkentseu::threading::NkSharedMutex
        >, "Legacy alias doit pointer vers moderne");

        // Test fonctionnel : les deux namespaces sont interchangeables
        nkentseu::entseu::NkSharedMutex legacyMutex;
        nkentseu::threading::NkSharedLock readLock(legacyMutex);  // OK
    }

    // ---------------------------------------------------------------------
    // Exemple 12 : Documentation de migration pour les utilisateurs
    // ---------------------------------------------------------------------
    /\*
    ============================================================================
    GUIDE DE MIGRATION : NkSharedMutex Aliases
    ============================================================================

    Contexte :
    ----------
    Les aliases NkSharedMutex/NkSharedLock/NkUniqueLock ont été introduits
    pour faciliter la migration depuis std::shared_mutex et fournir une API
    familière tout en utilisant l'implémentation optimisée NkReaderWriterLock.

    Impact :
    --------
    - Code legacy nkentseu::entseu : continue via aliases de compatibilité
    - Nouveau code : utiliser directement nkentseu::threading
    - Warning optionnel : configurable via NKENTSEU_WARN_ON_LEGACY_THREADING

    Étapes de migration :
    ---------------------
    1. Remplacer les includes si nécessaire :
       // Aucun changement requis : NkSharedMutex.h inclut déjà l'implémentation

    2. Mettre à jour les using namespace :
       AVANT : using namespace nkentseu::entseu;
       APRÈS : using namespace nkentseu::threading;

    3. Ou utiliser des qualifications complètes :
       AVANT : entseu::NkSharedMutex
       APRÈS : threading::NkSharedMutex

    4. Tester la compilation et les tests de concurrence

    5. [Optionnel] Désactiver les aliases legacy une fois la migration complète :
       add_definitions(-DNKENTSEU_DISABLE_LEGACY_ALIASES)

    Avantages de l'implémentation NK vs std::shared_mutex :
    -------------------------------------------------------
    ✓ Garantie noexcept sur toutes les opérations publiques
    ✓ Meilleure intégration avec le logging/debugging NK
    ✓ Comportement uniforme multiplateforme (pas de dépendance STL)
    ✓ Politiques de scheduling configurables (lecture/écriture préférentielle)
    ✓ Métriques et instrumentation natives pour le profiling

    Support :
    ---------
    - Voir : docs/migration-threading.md pour le guide complet
    - Outil : scripts/migrate_threading_aliases.py pour l'automatisation
    - Help  : ouvrir un ticket si problème de migration

    Calendrier :
    ------------
    - v2.x : Aliases disponibles avec warnings optionnels
    - v3.0 : Aliases legacy marqués deprecated (warnings par défaut)
    - v4.0 : Aliases legacy supprimés (breaking change planifié)

    ============================================================================
    *\/
*/
// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================