//
// NkConditionVariable.cpp
// =============================================================================
// Description :
//   Implémentation des méthodes non-template de NkConditionVariable.
//   Ce fichier contient les définitions séparées pour :
//   - Réduire les dépendances de compilation (inclusions système isolées)
//   - Minimiser la taille des objets compilés utilisant uniquement le header
//   - Permettre une compilation incrémentale optimisée
//
// Note architecturale :
//   Les méthodes template WaitUntil() restent inline dans le header car :
//   - Le compilateur doit accéder au code source pour l'instanciation
//   - Leur implémentation est triviale (délégation aux méthodes non-template)
//   - L'inlining permet l'optimisation des boucles d'attente fréquentes
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

// =====================================================================
// Inclusion de l'en-tête de déclaration
// =====================================================================
#include "NKThreading/NkConditionVariable.h"

// =====================================================================
// Inclusions système conditionnelles (isolées dans le .cpp)
// =====================================================================
#if defined(NKENTSEU_PLATFORM_WINDOWS)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#else
    #include <errno.h>
    #include <pthread.h>
    #include <time.h>
#endif

// =====================================================================
// Namespace : nkentseu::threading
// =====================================================================
namespace nkentseu {
    namespace threading {

        // =================================================================
        // IMPLÉMENTATION : NkConditionVariable
        // =================================================================

        // -----------------------------------------------------------------
        // Constructeur : initialisation de la variable de condition
        // -----------------------------------------------------------------
        // Windows : InitializeConditionVariable() est infaillible et légère.
        // POSIX   : pthread_cond_init() peut échouer (mémoire, ressources) ;
        //           l'échec est marqué via mInitialized pour gestion gracieuse.
        // -----------------------------------------------------------------
        NkConditionVariable::NkConditionVariable() noexcept
            : mInitialized(true)
        {
            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                InitializeConditionVariable(&mCondVar);
            #else
                const int result = pthread_cond_init(&mCondVar, nullptr);
                mInitialized = (result == 0);
            #endif
        }

        // -----------------------------------------------------------------
        // Destructeur : libération des ressources système
        // -----------------------------------------------------------------
        // Windows : CONDITION_VARIABLE ne nécessite pas de destruction explicite.
        // POSIX   : pthread_cond_destroy() doit être appelé pour libérer les
        //           ressources ; vérification de mInitialized pour éviter UB.
        // -----------------------------------------------------------------
        NkConditionVariable::~NkConditionVariable() noexcept
        {
            #if !defined(NKENTSEU_PLATFORM_WINDOWS)
                if (mInitialized) {
                    (void)pthread_cond_destroy(&mCondVar);
                }
            #endif
        }

        // -----------------------------------------------------------------
        // Méthode : Wait (attente bloquante sans timeout)
        // -----------------------------------------------------------------
        // Comportement :
        //   1. Vérifie la validité du mutex et de la condition variable
        //   2. Relâche atomiquement le mutex et suspend le thread
        //   3. Au réveil (notification/spurious), ré-acquiert le mutex
        //   4. Retourne au code appelant avec le mutex toujours détenu
        //
        // Propriétés atomiques :
        //   - Le relâchement du mutex et la suspension sont atomiques
        //   - Aucune notification ne peut être perdue entre les deux étapes
        //   - Garantie par les primitives OS natives (SRWLock/pthread)
        //
        // Spurious wakeups :
        //   - Les systèmes peuvent réveiller un thread sans notification explicite
        //   - Toujours appeler Wait() dans une boucle vérifiant la condition
        //   - Pattern : while (!condition) { cond.Wait(lock); }
        // -----------------------------------------------------------------
        void NkConditionVariable::Wait(NkScopedLockMutex& lock) noexcept
        {
            NkMutex* mutex = lock.GetMutex();
            if (mutex == nullptr || !mutex->mInitialized || !mInitialized) {
                return;
            }

            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                (void)SleepConditionVariableSRW(
                    &mCondVar,
                    &mutex->mMutex,
                    INFINITE,
                    0
                );
            #else
                (void)pthread_cond_wait(&mCondVar, &mutex->mMutex);
            #endif
        }

        // -----------------------------------------------------------------
        // Méthode : WaitFor (attente avec timeout relatif)
        // -----------------------------------------------------------------
        // Comportement :
        //   - Timeout relatif : compté depuis l'appel de la méthode
        //   - Retourne true si réveillé par NotifyOne/NotifyAll
        //   - Retourne false si timeout expiré ou erreur système
        //
        // Gestion des erreurs Windows :
        //   - SleepConditionVariableSRW() retourne FALSE pour timeout OU erreur
        //   - GetLastError() == ERROR_TIMEOUT indique un timeout normal
        //   - Toute autre erreur est traitée comme échec (retour false)
        //
        // Gestion des interruptions POSIX :
        //   - pthread_cond_timedwait() peut retourner EINTR (signal reçu)
        //   - Boucle do-while pour ré-essayer automatiquement après EINTR
        //   - Seul retour 0 (succès) ou ETIMEDOUT (timeout) sort de la boucle
        // -----------------------------------------------------------------
        nk_bool NkConditionVariable::WaitFor(
            NkScopedLockMutex& lock,
            nk_uint32 milliseconds
        ) noexcept {
            NkMutex* mutex = lock.GetMutex();
            if (mutex == nullptr || !mutex->mInitialized || !mInitialized) {
                return false;
            }

            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                const BOOL woke = SleepConditionVariableSRW(
                    &mCondVar,
                    &mutex->mMutex,
                    milliseconds,
                    0
                );
                if (woke) {
                    return true;
                }
                // FALSE retourné : timeout ou erreur
                return GetLastError() != ERROR_TIMEOUT;
            #else
                const timespec absDeadline = MakeAbsoluteTimeout(milliseconds);
                int result = 0;
                do {
                    result = pthread_cond_timedwait(
                        &mCondVar,
                        &mutex->mMutex,
                        &absDeadline
                    );
                } while (result == EINTR);
                return result == 0;
            #endif
        }

        // -----------------------------------------------------------------
        // Méthode : WaitUntil (attente avec deadline absolue)
        // -----------------------------------------------------------------
        // Comportement :
        //   - Deadline absolue : timestamp monotone cible (pas un délai)
        //   - Utile pour les timeouts cumulatifs ou coordonnés entre threads
        //   - Boucle interne avec WaitFor() pour gérer les réveils partiels
        //
        // Gestion du overflow 32-bit :
        //   - WaitFor() accepte nk_uint32 milliseconds (max ~49 jours)
        //   - Si remaining > 0xFFFFFFFF, on utilise la valeur max autorisée
        //   - waitMs = 0 est ajusté à 1ms pour éviter une boucle infinie
        //
        // Précision :
        //   - La précision réelle dépend du scheduler OS et de la charge système
        //   - Pour des deadlines précises, préférer les APIs temps-réel OS
        // -----------------------------------------------------------------
        nk_bool NkConditionVariable::WaitUntil(
            NkScopedLockMutex& lock,
            nk_uint64 deadlineMs
        ) noexcept {
            nk_uint64 now = GetMonotonicTimeMs();
            while (now < deadlineMs) {
                nk_uint64 remaining = deadlineMs - now;
                nk_uint32 waitMs = (remaining > 0xFFFFFFFFULL)
                                     ? 0xFFFFFFFFu
                                     : static_cast<nk_uint32>(remaining);
                if (waitMs == 0) {
                    waitMs = 1;
                }

                if (WaitFor(lock, waitMs)) {
                    return true;
                }

                now = GetMonotonicTimeMs();
            }
            return false;
        }

        // -----------------------------------------------------------------
        // Méthode : NotifyOne (réveil d'un thread en attente)
        // -----------------------------------------------------------------
        // Comportement :
        //   - Réveille arbitrairement UN thread bloqué sur Wait()/WaitFor()
        //   - Aucun effet si aucun thread n'est en attente (notification perdue)
        //   - Plus efficace que NotifyAll() quand un seul consommateur peut
        //     traiter l'événement (évite le thundering herd)
        //
        // Sélection du thread :
        //   - Windows : sélection basée sur la priorité et l'ordre d'attente
        //   - POSIX   : sélection non spécifiée (implémentation dépendante)
        //   - Ne pas compter sur un ordre FIFO/LIFO pour la logique métier
        // -----------------------------------------------------------------
        void NkConditionVariable::NotifyOne() noexcept
        {
            if (!mInitialized) {
                return;
            }
            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                WakeConditionVariable(&mCondVar);
            #else
                (void)pthread_cond_signal(&mCondVar);
            #endif
        }

        // -----------------------------------------------------------------
        // Méthode : NotifyAll (broadcast à tous les threads en attente)
        // -----------------------------------------------------------------
        // Comportement :
        //   - Réveille TOUS les threads bloqués sur Wait()/WaitFor()
        //   - Utile pour les conditions de shutdown, reconfiguration globale
        //   - Peut causer du "thundering herd" : tous les threads compétent
        //     pour ré-acquérir le mutex simultanément
        //
        // Optimisation :
        //   - Préférer NotifyOne() quand seul un thread peut progresser
        //   - Utiliser NotifyAll() uniquement pour les broadcast sémantiques
        //   - Combiner avec des prédicats dans WaitUntil() pour filtrer
        // -----------------------------------------------------------------
        void NkConditionVariable::NotifyAll() noexcept
        {
            if (!mInitialized) {
                return;
            }
            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                WakeAllConditionVariable(&mCondVar);
            #else
                (void)pthread_cond_broadcast(&mCondVar);
            #endif
        }

        // -----------------------------------------------------------------
        // Méthode statique : GetMonotonicTimeMs
        // -----------------------------------------------------------------
        // Retourne un timestamp monotone en millisecondes depuis le boot.
        //
        // Propriétés :
        //   - Monotone : ne décroît jamais, même en cas de changement d'heure
        //   - Relatif : la valeur absolue n'a pas de sens, seules les différences comptent
        //   - Wraparound : GetTickCount64() wrap après ~584 jours (géré par
        //                 l'arithmétique non-signée dans les calculs de délai)
        //
        // Alternatives rejetées :
        //   - std::chrono::steady_clock : plus portable mais overhead potentiel
        //   - QueryPerformanceCounter : plus précis mais trop coûteux pour ce cas
        // -----------------------------------------------------------------
        nk_uint64 NkConditionVariable::GetMonotonicTimeMs() noexcept
        {
            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                return static_cast<nk_uint64>(GetTickCount64());
            #else
                timespec ts{};
                if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
                    return 0U;
                }
                return static_cast<nk_uint64>(ts.tv_sec) * 1000ULL +
                       static_cast<nk_uint64>(ts.tv_nsec / 1000000ULL);
            #endif
        }

        // =================================================================
        // IMPLÉMENTATIONS POSIX-ONLY (méthodes privées)
        // =================================================================
        #if !defined(NKENTSEU_PLATFORM_WINDOWS)

            // -----------------------------------------------------------------
            // Méthode privée : MakeAbsoluteTimeout
            // -----------------------------------------------------------------
            // Convertit un timeout relatif (ms) en timespec absolu pour
            // pthread_cond_timedwait().
            //
            // Algorithme :
            //   1. Récupère l'heure courante via clock_gettime(CLOCK_REALTIME)
            //   2. Ajoute les secondes : tv_sec += milliseconds / 1000
            //   3. Ajoute les nanosecondes : tv_nsec += (ms % 1000) * 1000000
            //   4. Normalise si tv_nsec >= 1000000000 (carry vers tv_sec)
            //
            // Gestion d'erreur :
            //   - Si clock_gettime() échoue, retourne timespec nul (timeout immédiat)
            //   - Comportement conservatif : mieux vaut un faux timeout qu'un hang
            // -----------------------------------------------------------------
            timespec NkConditionVariable::MakeAbsoluteTimeout(
                nk_uint32 milliseconds
            ) noexcept {
                timespec deadline{};
                if (clock_gettime(CLOCK_REALTIME, &deadline) != 0) {
                    deadline.tv_sec = 0;
                    deadline.tv_nsec = 0;
                    return deadline;
                }

                deadline.tv_sec += static_cast<time_t>(milliseconds / 1000U);
                deadline.tv_nsec += static_cast<long>(milliseconds % 1000U) * 1000000L;
                if (deadline.tv_nsec >= 1000000000L) {
                    deadline.tv_sec += 1;
                    deadline.tv_nsec -= 1000000000L;
                }
                return deadline;
            }

        #endif  // !defined(NKENTSEU_PLATFORM_WINDOWS)

        // =================================================================
        // Note : Implémentations template dans le header
        // =================================================================
        // Les méthodes template WaitUntil<Predicate>() sont définies inline
        // dans NkConditionVariable.h car :
        //
        // 1. Nature template : le compilateur doit voir le code source pour
        //    générer les spécialisations pour chaque type de prédicat utilisé.
        //
        // 2. Performance : ces méthodes sont dans le chemin critique des
        //    boucles d'attente ; l'inlining permet l'optimisation aggressive.
        //
        // 3. Simplicité : l'implémentation est triviale (délégation aux
        //    méthodes non-template Wait/WaitFor), pas de logique complexe.

    }
}