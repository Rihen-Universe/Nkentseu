//
// NkScopedLock.h
// =============================================================================
// Description :
//   Guard RAII générique pour la gestion automatique des verrous de mutex.
//   Compatible avec tout type implémentant Lock()/Unlock() (duck typing).
//
// Caractéristiques :
//   - Pattern Resource Acquisition Is Initialization (RAII)
//   - Template générique : fonctionne avec NkMutex, NkSpinLock, NkRecursiveMutex
//   - Garantie noexcept sur construction/destruction pour sécurité exceptionnelle
//   - Sémantique move-only : prévention des copies accidentelles de locks
//   - Zero overhead : implémentation inline, optimisée par le compilateur
//
// Algorithmes implémentés :
//   - Acquisition immédiate du lock dans le constructeur
//   - Libération automatique dans le destructeur (même en cas d'exception)
//   - Délégation directe aux méthodes Lock()/Unlock() du mutex sous-jacent
//
// Types disponibles :
//   NkScopedLock<TMutex> : guard générique pour tout mutex compatible
//   NkLockGuard          : alias court pour NkScopedLock<NkMutex> (cas courant)
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_THREADING_SCOPEDLOCK_H__
#define __NKENTSEU_THREADING_SCOPEDLOCK_H__

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion des types fondamentaux et de NkMutex pour l'alias NkLockGuard.
    // Les autres types de mutex (NkSpinLock, etc.) sont supportés via duck typing.
    #include "NKCore/NkTypes.h"
    #include "NKThreading/NkMutex.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : NAMESPACE PRINCIPAL
    // -------------------------------------------------------------------------
    namespace nkentseu {

        // =====================================================================
        // SOUS-NAMESPACE : threading
        // =====================================================================
        namespace threading {

            // =================================================================
            // CLASSE TEMPLATE : NkScopedLock<TMutex>
            // =================================================================
            // Guard RAII générique pour la gestion automatique des verrous.
            //
            // Concept requis sur TMutex (duck typing) :
            //   - void Lock() noexcept : verrouille le mutex (bloquant ou non)
            //   - void Unlock() noexcept : déverrouille le mutex
            //
            // Invariant de production :
            //   - Le mutex est toujours verrouillé après construction réussie
            //   - Le mutex est toujours déverrouillé à la destruction (si encore détenu)
            //   - Aucune exception ne peut être levée (noexcept garanti)
            //
            // Avantages vs gestion manuelle :
            //   - Prévention systématique des deadlocks par oubli d'Unlock()
            //   - Sécurité exceptionnelle : rollback automatique sur throw
            //   - Code plus lisible : la portée du lock est visuellement délimitée
            //   - Compatible avec early returns et multiples points de sortie
            //
            // Sémantique de propriété :
            //   - Non-copiable : un lock ne peut être dupliqué (ressource unique)
            //   - Non-déplaçable : la portée du lock est liée à l'instance (design choice)
            //   - Pour étendre la portée : utiliser un scope plus large, pas un move
            // =================================================================
            template<typename TMutex>
            class NkScopedLock {
                public:

                    // -------------------------------------------------------------
                    // CONSTRUCTEURS ET DESTRUCTEUR
                    // -------------------------------------------------------------

                    /// @brief Constructeur : verrouille immédiatement le mutex fourni.
                    /// @param mutex Référence vers le mutex à protéger.
                    /// @note Garantie noexcept : Lock() ne lève pas d'exception.
                    /// @note Le mutex est détenu dès la fin de cette ligne de construction.
                    /// @tparam TMutex Type du mutex (doit implémenter Lock()/Unlock()).
                    explicit NkScopedLock(TMutex& mutex) noexcept;

                    /// @brief Destructeur : déverrouille le mutex si encore détenu.
                    /// @note Garantie noexcept : Unlock() est toujours sûr.
                    /// @note Appel automatique à la sortie de portée (même sur exception).
                    ~NkScopedLock() noexcept;

                    // -------------------------------------------------------------
                    // RÈGLES DE COPIE ET DÉPLACEMENT (INTERDITES)
                    // -------------------------------------------------------------

                    /// @brief Constructeur de copie supprimé : lock non-copiable.
                    /// @note Un mutex ne peut être détenu qu'une fois par thread.
                    NkScopedLock(const NkScopedLock&) = delete;

                    /// @brief Opérateur d'affectation supprimé : lock non-affectable.
                    NkScopedLock& operator=(const NkScopedLock&) = delete;

                    // Note : Déplacement également interdit par design pour éviter
                    // la confusion sur la portée réelle de la section critique.
                    // Si transfert de propriété nécessaire, utiliser un scope plus large.

                private:

                    // -------------------------------------------------------------
                    // MEMBRES PRIVÉS
                    // -------------------------------------------------------------

                    /// @brief Référence vers le mutex géré (non-null par construction).
                    TMutex& mMutex;
            };

            // =================================================================
            // ALIAS DE TYPE : NkLockGuard
            // =================================================================
            /// @brief Alias court pour NkScopedLock<NkMutex>.
            /// @ingroup ThreadingUtilities
            /// @note Cas d'usage le plus fréquent : protection avec NkMutex standard.
            /// @note Équivalent fonctionnel à std::lock_guard<std::mutex> mais
            ///       avec les garanties noexcept et l'intégration NK entseu.
            using NkLockGuard = NkScopedLock<NkMutex>;

        }
    }

    // -------------------------------------------------------------------------
    // ALIAS GLOBAUX : Compatibilité avec code legacy (namespace nkentseu::)
    // -------------------------------------------------------------------------
    // Ces alias permettent d'utiliser NkScopedLock et NkLockGuard depuis
    // le namespace parent nkentseu:: sans qualification threading::,
    // pour compatibilité avec les modules existants (NkEventBus, etc.).
    //
    // Nouveau code recommandé : utiliser nkentseu::threading:: directement.
    // Code legacy : ces alias assurent la rétrocompatibilité sans duplication.

    /// @brief Alias global pour NkLockGuard (compatibilité legacy).
    /// @deprecated Utiliser nkentseu::threading::NkLockGuard directement.
    using NkLockGuard = nkentseu::threading::NkLockGuard;

    /// @brief Alias global template pour NkScopedLock (compatibilité legacy).
    /// @deprecated Utiliser nkentseu::threading::NkScopedLock<T> directement.
    /// @tparam T Type du mutex à protéger.
    template<typename T>
    using NkScopedLock = nkentseu::threading::NkScopedLock<T>;

#endif

// =============================================================================
// IMPLÉMENTATIONS TEMPLATE (dans le header car templates)
// =============================================================================

namespace nkentseu {
    namespace threading {

        // ---------------------------------------------------------------------
        // Constructeur : acquisition immédiate du lock
        // ---------------------------------------------------------------------
        // Délègue directement à TMutex::Lock() qui doit être noexcept.
        // Le mutex est détenu immédiatement après retour du constructeur.
        //
        // Préconditions :
        //   - mutex référence valide (non-null, non-détruit)
        //   - mutex.implémente Lock() noexcept
        //
        // Postconditions :
        //   - mutex est verrouillé par le thread courant
        //   - tout code suivant s'exécute dans la section critique
        // ---------------------------------------------------------------------
        template<typename TMutex>
        inline NkScopedLock<TMutex>::NkScopedLock(TMutex& mutex) noexcept
            : mMutex(mutex)
        {
            mMutex.Lock();
        }

        // ---------------------------------------------------------------------
        // Destructeur : libération automatique du lock
        // ---------------------------------------------------------------------
        // Délègue directement à TMutex::Unlock() qui doit être noexcept.
        // Appel automatique à la sortie de portée, même en cas d'exception.
        //
        // Préconditions :
        //   - mMutex référence valide vers un mutex précédemment locké
        //   - le thread courant détient effectivement le verrou
        //
        // Postconditions :
        //   - mutex est déverrouillé et disponible pour d'autres threads
        //   - aucune fuite de ressource (garantie RAII)
        // ---------------------------------------------------------------------
        template<typename TMutex>
        inline NkScopedLock<TMutex>::~NkScopedLock() noexcept
        {
            mMutex.Unlock();
        }

    }
}

// =============================================================================
// EXEMPLES D'UTILISATION (documentation - ne pas compiler)
// =============================================================================
/*
    // ---------------------------------------------------------------------
    // Exemple 1 : Usage basique avec NkMutex (alias NkLockGuard)
    // ---------------------------------------------------------------------
    #include <NKThreading/NkScopedLock.h>
    #include <NKThreading/NkMutex.h>
    using namespace nkentseu::threading;

    NkMutex dataMutex;
    int sharedCounter = 0;

    void SafeIncrement()
    {
        NkLockGuard lock(dataMutex);  // Lock acquis ici
        ++sharedCounter;              // Section critique protégée
        // Unlock automatique à la sortie de portée (même si exception)
    }

    // ---------------------------------------------------------------------
    // Exemple 2 : Usage générique avec NkSpinLock (duck typing)
    // ---------------------------------------------------------------------
    #include <NKThreading/NkScopedLock.h>
    #include <NKThreading/NkSpinLock.h>  // Hypothétique, implémente Lock/Unlock

    NkSpinLock fastMutex;  // Mutex léger pour sections très courtes

    void FastOperation()
    {
        // NkScopedLock fonctionne avec tout type ayant Lock()/Unlock()
        NkScopedLock<NkSpinLock> lock(fastMutex);
        PerformQuickUpdate();  // Section critique ultra-courte (<100ns)
    }

    // ---------------------------------------------------------------------
    // Exemple 3 : Protection avec early return (avantage RAII)
    // ---------------------------------------------------------------------
    #include <NKThreading/NkScopedLock.h>

    bool ProcessWithValidation(NkMutex& mutex, Data& data)
    {
        NkLockGuard lock(mutex);  // Lock acquis

        if (!data.IsValid()) {
            return false;  // ✅ Unlock automatique avant return
        }

        if (data.IsExpired()) {
            return false;  // ✅ Unlock automatique avant return
        }

        data.Transform();  // Section critique principale
        return true;       // ✅ Unlock automatique avant return
        // Aucun risque d'oubli d'Unlock() avec RAII
    }

    // ---------------------------------------------------------------------
    // Exemple 4 : Sécurité exceptionnelle garantie
    // ---------------------------------------------------------------------
    #include <NKThreading/NkScopedLock.h>
    #include <stdexcept>

    void RiskyOperation(NkMutex& mutex, std::vector<int>& buffer)
    {
        NkLockGuard lock(mutex);  // Lock acquis

        // Même si une exception est levée ici...
        if (buffer.empty()) {
            throw std::runtime_error("Buffer vide");
        }

        // ... ou ici...
        buffer.at(1000) = 42;  // Peut lancer std::out_of_range

        // ... le mutex sera TOUJOURS déverrouillé à la destruction du guard
        // Évite les deadlocks par exception non gérée
    }

    // ---------------------------------------------------------------------
    // Exemple 5 : Scope imbriqué pour granularité fine
    // ---------------------------------------------------------------------
    #include <NKThreading/NkScopedLock.h>

    void GranularLocking(NkMutex& mutex1, NkMutex& mutex2)
    {
        {
            NkLockGuard lock1(mutex1);  // Scope 1 : mutex1 uniquement
            UpdateResourceA();
        }  // mutex1 libéré ici

        DoUnprotectedWork();  // Section non protégée (performance)

        {
            NkLockGuard lock2(mutex2);  // Scope 2 : mutex2 uniquement
            UpdateResourceB();
        }  // mutex2 libéré ici

        // Évite de garder les locks plus longtemps que nécessaire
    }

    // ---------------------------------------------------------------------
    // Exemple 6 : Template générique acceptant tout mutex compatible
    // ---------------------------------------------------------------------
    #include <NKThreading/NkScopedLock.h>

    // Fonction générique : fonctionne avec NkMutex, NkSpinLock, NkRecursiveMutex...
    template<typename MutexType>
    void ProtectedExecute(MutexType& mutex, std::function<void()> operation)
    {
        // NkScopedLock<T> fonctionne grâce au duck typing sur Lock()/Unlock()
        NkScopedLock<MutexType> lock(mutex);
        operation();  // Exécuté dans la section critique
    }

    // Usage avec différents types de mutex :
    NkMutex standardMutex;
    ProtectedExecute(standardMutex, []() { DoStandardWork(); });

    NkRecursiveMutex recursiveMutex;
    ProtectedExecute(recursiveMutex, []() { DoRecursiveWork(); });

    // ---------------------------------------------------------------------
    // Exemple 7 : Comparaison avec gestion manuelle (anti-pattern)
    // ---------------------------------------------------------------------
    // ❌ MAUVAIS : Gestion manuelle risque d'oubli
    void ManualLocking(NkMutex& mutex)
    {
        mutex.Lock();
        // ... code ...
        if (errorCondition) {
            return;  // ❌ Oubli d'Unlock() = deadlock potentiel !
        }
        mutex.Unlock();  // Peut ne jamais être atteint
    }

    // ✅ BON : RAII avec NkScopedLock
    void SafeLocking(NkMutex& mutex)
    {
        NkLockGuard lock(mutex);  // Acquisition
        // ... code ...
        if (errorCondition) {
            return;  // ✅ Unlock automatique par destructeur
        }
        // ✅ Unlock automatique à la sortie normale aussi
    }

    // ---------------------------------------------------------------------
    // Exemple 8 : Intégration avec NkConditionVariable (pattern classique)
    // ---------------------------------------------------------------------
    #include <NKThreading/NkScopedLock.h>
    #include <NKThreading/NkConditionVariable.h>

    class ThreadSafeQueue {
    public:
        void Push(Item item)
        {
            NkLockGuard lock(mMutex);  // Protection de l'accès
            mQueue.push(std::move(item));
            // Lock libéré avant notification (optimisation)
        }  // Unlock automatique ici

        Item Pop()
        {
            NkLockGuard lock(mMutex);
            mCond.WaitUntil(lock, [this]() { return !mQueue.empty(); });
            Item item = std::move(mQueue.front());
            mQueue.pop();
            return item;  // Lock libéré automatiquement au return
        }

    private:
        NkMutex mMutex;
        NkConditionVariable mCond;
        std::queue<Item> mQueue;
    };

    // ---------------------------------------------------------------------
    // Exemple 9 : Performance : éviter les copies inutiles
    // ---------------------------------------------------------------------
    #include <NKThreading/NkScopedLock.h>

    // ❌ MAUVAIS : Créer un guard dans une boucle (overhead inutile)
    void BadLoopLocking(NkMutex& mutex, const std::vector<int>& items)
    {
        for (int item : items) {
            NkLockGuard lock(mutex);  // Lock/Unlock à chaque itération !
            ProcessItem(item);
        }  // Très coûteux si beaucoup d'itérations
    }

    // ✅ BON : Verrouiller une fois pour toute la boucle si possible
    void GoodLoopLocking(NkMutex& mutex, const std::vector<int>& items)
    {
        NkLockGuard lock(mutex);  // Lock une seule fois
        for (int item : items) {
            ProcessItem(item);  // Toute la boucle dans la section critique
        }
        // Plus efficace, mais attention à ne pas bloquer trop longtemps
    }

    // ✅ ALTERNATIVE : Granularité fine si ProcessItem est long
    void BalancedLoopLocking(NkMutex& mutex, const std::vector<int>& items)
    {
        for (int item : items) {
            {
                NkLockGuard lock(mutex);  // Scope court pour accès partagé
                SharedData& data = GetSharedData();
                data.Update(item);
            }  // Libération immédiate
            ProcessItem(item);  // Travail long hors section critique
        }
    }

    // ---------------------------------------------------------------------
    // Exemple 10 : Bonnes pratiques et anti-patterns récapitulatifs
    // ---------------------------------------------------------------------
    // ✅ BON : Nommer le guard pour clarté et debugging
    void ClearNaming(NkMutex& mutex)
    {
        NkLockGuard dataLock(mutex);  // Nom explicite : "dataLock"
        UpdateData();
    }

    // ❌ MAUVAIS : Variable sans nom ou nom générique
    void UnclearNaming(NkMutex& mutex)
    {
        NkLockGuard _(mutex);  // ❌ Nom "_" peu explicite
        UpdateData();
    }

    // ✅ BON : Scope minimal pour réduire le temps de lock
    void MinimalScope(NkMutex& mutex)
    {
        {
            NkLockGuard lock(mutex);
            criticalValue = ComputeCriticalValue();
        }  // Libération immédiate
        UseNonCriticalValue(criticalValue);  // Hors section critique
    }

    // ❌ MAUVAIS : Scope trop large (bloque inutilement)
    void ExcessiveScope(NkMutex& mutex)
    {
        NkLockGuard lock(mutex);
        criticalValue = ComputeCriticalValue();
        UseNonCriticalValue(criticalValue);  // ❌ Lock maintenu inutilement
    }

    // ✅ BON : Utiliser l'alias NkLockGuard pour NkMutex (plus lisible)
    void PreferAlias(NkMutex& mutex)
    {
        NkLockGuard lock(mutex);  // ✅ Clair et concis
    }

    // ❌ MAUVAIS : Qualification complète inutile pour le cas courant
    void VerboseUsage(NkMutex& mutex)
    {
        nkentseu::threading::NkScopedLock<NkMutex> lock(mutex);  // ❌ Trop verbeux
    }

    // ✅ BON : Template générique quand le type de mutex est paramétrique
    template<typename MutexType>
    void GenericFunction(MutexType& mutex)
    {
        NkScopedLock<MutexType> lock(mutex);  // ✅ Adaptatif au type
    }
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================