//
// NkSemaphore.cpp
// =============================================================================
// Description :
//   Implémentation des méthodes de NkSemaphore.
//   Ce fichier contient les définitions séparées pour :
//   - Réduire les dépendances de compilation (inclusions isolées)
//   - Minimiser la taille des objets compilés utilisant uniquement le header
//   - Permettre une compilation incrémentale optimisée
//
// Note architecturale :
//   Toutes les méthodes sont implémentées avec NkScopedLockMutex pour garantir
//   la sécurité exceptionnelle et la clarté du code RAII.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

// =====================================================================
// Inclusion de l'en-tête de déclaration
// =====================================================================
#include "NKThreading/NkSemaphore.h"

// =====================================================================
// Namespace : nkentseu::threading
// =====================================================================
namespace nkentseu {
    namespace threading {

        // =================================================================
        // IMPLÉMENTATION : NkSemaphore
        // =================================================================

        // -----------------------------------------------------------------
        // Constructeur : initialisation du compteur et des limites
        // -----------------------------------------------------------------
        // Gestion du cas edge : si maxCount < initialCount, on ajuste
        // maxCount à initialCount pour garantir l'invariant 0 <= count <= max.
        // Cela évite un état incohérent dès la construction.
        // -----------------------------------------------------------------
        NkSemaphore::NkSemaphore(
            nk_uint32 initialCount,
            nk_uint32 maxCount
        ) noexcept
            : mCount(initialCount)
            , mMaxCount(maxCount >= initialCount ? maxCount : initialCount)
            , mMutex()
            , mCondVar()
        {
            // Initialisation des membres via liste d'initialisation :
            // - mCount : valeur de départ (ressources disponibles)
            // - mMaxCount : capacité maximale (ajustée si nécessaire)
            // - mMutex/mCondVar : constructeurs par défaut noexcept
        }

        // -----------------------------------------------------------------
        // Méthode : Acquire (attente bloquante)
        // -----------------------------------------------------------------
        // Algorithme :
        //   1. Acquérir le mutex interne via NkScopedLockMutex (RAII)
        //   2. Boucler tant que mCount == 0 (spurious wakeup safe)
        //   3. Appeler Wait() sur la condition variable (relâche mutex + suspend)
        //   4. Au réveil, re-vérifier mCount > 0 avant de continuer
        //   5. Décrémenter mCount et retourner (mutex libéré par RAII)
        //
        // Gestion des spurious wakeups :
        //   - while (mCount == 0) au lieu de if : robuste aux réveils intempestifs
        //   - Requis par la sémantique POSIX/pthread et recommandé pour Windows
        //
        // Thread-safety :
        //   - Le mutex protège l'accès à mCount pendant toute l'opération
        //   - La condition variable garantit l'attente efficace sans polling
        // -----------------------------------------------------------------
        void NkSemaphore::Acquire() noexcept
        {
            NkScopedLockMutex lock(mMutex);
            while (mCount == 0u) {
                mCondVar.Wait(lock);
            }
            --mCount;
        }

        // -----------------------------------------------------------------
        // Méthode : TryAcquire (tentative non-bloquante)
        // -----------------------------------------------------------------
        // Comportement :
        //   - Retourne immédiatement avec true si mCount > 0
        //   - Retourne false sans attente si mCount == 0
        //
        // Utilisation typique :
        //   - Algorithmes lock-free avec fallback sur autre stratégie
        //   - Polling avec backoff exponentiel pour éviter la contention
        //   - UI threads qui ne doivent jamais bloquer
        // -----------------------------------------------------------------
        nk_bool NkSemaphore::TryAcquire() noexcept
        {
            NkScopedLockMutex lock(mMutex);
            if (mCount == 0u) {
                return false;
            }
            --mCount;
            return true;
        }

        // -----------------------------------------------------------------
        // Méthode : TryAcquireFor (tentative avec timeout)
        // -----------------------------------------------------------------
        // Algorithme :
        //   1. Vérifier immédiatement si mCount > 0 (fast path)
        //   2. Calculer la deadline absolue : now + milliseconds
        //   3. Boucler avec WaitUntil(deadline) tant que mCount == 0
        //   4. Retourner true si acquis, false si timeout expiré
        //
        // Précision du timeout :
        //   - Relative : comptée depuis l'appel de la méthode
        //   - Non garantie exacte : dépend du scheduler OS et de la charge
        //   - Pour des deadlines précises, préférer les APIs temps-réel OS
        //
        // Gestion du overflow 64-bit :
        //   - nk_uint64 pour deadline : wrap après ~584 ans (pratique infini)
        //   - Comparaison now < deadline safe grâce à l'arithmétique non-signée
        // -----------------------------------------------------------------
        nk_bool NkSemaphore::TryAcquireFor(nk_uint32 milliseconds) noexcept
        {
            NkScopedLockMutex lock(mMutex);
            if (mCount > 0u) {
                --mCount;
                return true;
            }

            const nk_uint64 deadline =
                NkConditionVariable::GetMonotonicTimeMs()
                + static_cast<nk_uint64>(milliseconds);

            while (mCount == 0u) {
                if (!mCondVar.WaitUntil(lock, deadline)) {
                    return false;
                }
            }

            --mCount;
            return true;
        }

        // -----------------------------------------------------------------
        // Méthode : Release (libération d'unités)
        // -----------------------------------------------------------------
        // Algorithme :
        //   1. Vérifier count > 0 (noop si count == 0)
        //   2. Acquérir le mutex pour protéger mCount
        //   3. Vérifier overflow : mCount + count <= mMaxCount
        //   4. Incrémenter mCount de 'count'
        //   5. Notifier 'count' threads en attente via NotifyOne() x count
        //
        // Protection contre overflow :
        //   - Retourne false si mCount + count > mMaxCount
        //   - Évite la corruption de l'état interne du sémaphore
        //   - L'appelant doit gérer l'échec (logging, fallback, assertion)
        //
        // Stratégie de notification :
        //   - NotifyOne() appelé 'count' fois : réveille jusqu'à 'count' waiters
        //   - Plus équitable que NotifyAll() (évite thundering herd)
        //   - Les waiters réveillés re-compétitionnent pour mCount via la boucle while
        // -----------------------------------------------------------------
        nk_bool NkSemaphore::Release(nk_uint32 count) noexcept
        {
            if (count == 0u) {
                return true;
            }

            NkScopedLockMutex lock(mMutex);
            if (mCount > mMaxCount || count > (mMaxCount - mCount)) {
                return false;
            }

            mCount += count;
            for (nk_uint32 i = 0u; i < count; ++i) {
                mCondVar.NotifyOne();
            }
            return true;
        }

        // -----------------------------------------------------------------
        // Méthode : GetCount (requête d'état)
        // -----------------------------------------------------------------
        // Retourne la valeur courante de mCount protégée par mutex.
        //
        // Avertissement de concurrence :
        //   - La valeur retournée peut être obsolète dès le retour de la méthode
        //   - Ne pas utiliser pour prendre des décisions de synchronisation
        //   - Utile pour : debugging, métriques, logging, assertions en test
        //
        // Thread-safety :
        //   - Lecture protégée par mutex : pas de data race
        //   - Mais pas d'atomicité avec d'autres opérations : valeur "instantanée"
        // -----------------------------------------------------------------
        nk_uint32 NkSemaphore::GetCount() const noexcept
        {
            NkScopedLockMutex lock(mMutex);
            return mCount;
        }

        // -----------------------------------------------------------------
        // Méthode : GetMaxCount (requête de capacité)
        // -----------------------------------------------------------------
        // Retourne la capacité maximale fixée à la construction.
        //
        // Optimisation :
        //   - mMaxCount est immutable après construction : pas de mutex requis
        //   - Lecture directe safe car jamais modifiée post-construction
        //
        // Utilisation :
        //   - Valider les paramètres d'appel à Release() côté client
        //   - Calculer le taux d'utilisation : (max - current) / max
        //   - Logging et métriques de monitoring
        // -----------------------------------------------------------------
        nk_uint32 NkSemaphore::GetMaxCount() const noexcept
        {
            return mMaxCount;
        }

    }
}