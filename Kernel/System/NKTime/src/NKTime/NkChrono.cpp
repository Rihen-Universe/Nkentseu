// =============================================================================
// NKTime/NkChrono.cpp
// Implémentation de NkChrono et NkElapsedTime::ToString.
//
// Design :
//  - Windows : QueryPerformanceCounter avec décomposition pour éviter l'overflow
//  - POSIX   : clock_gettime(CLOCK_MONOTONIC) avec résolution ~1 ns
//  - Emscripten : emscripten_sleep() avec ASYNCIFY pour le Web
//  - Sleep sub-milliseconde : gestion conservative selon la plateforme
//
// Notes de performance :
//  - Windows QPC : décomposition (counter/freq)*1e9 + ((counter%freq)*1e9)/freq
//    garantit que (counter%freq)*1e9 < 1e16 < INT64_MAX (~9.2e18)
//  - POSIX : clock_nanosleep préféré à nanosleep pour précision monotone
//  - Aucune dépendance STL : uniquement <cstdio> pour snprintf
//
// Auteur : TEUGUIA TADJUIDJE Rodolf Séderis
// Date : 2024-2026
// License : Apache-2.0
// =============================================================================

// -------------------------------------------------------------------------
// SECTION 1 : INCLUSIONS (ordre strict requis)
// -------------------------------------------------------------------------
// 1. Precompiled header en premier (obligatoire pour la compilation MSVC/Clang)
// 2. Header correspondant au fichier .cpp
// 3. Headers du projet NKEntseu
// 4. Headers système conditionnels selon la plateforme

#include "pch.h"
#include "NKTime/NkChrono.h"
#include "NKPlatform/NkPlatformDetect.h"

#include <cstdio>   // snprintf — seule dépendance C standard autorisée

#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
    #include <emscripten.h>
#endif

// -------------------------------------------------------------------------
// SECTION 2 : INCLUSIONS SYSTÈME CONDITIONNELLES
// -------------------------------------------------------------------------
// Gestion multiplateforme des APIs de timing et de sommeil.

#if defined(NKENTSEU_PLATFORM_WINDOWS)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#else
    #include <time.h>    // clock_gettime, nanosleep, clock_nanosleep
    #include <errno.h>   // EINTR pour la reprise sur interruption
    #include <sched.h>   // sched_yield pour YieldThread
#endif

// -------------------------------------------------------------------------
// SECTION 3 : NAMESPACE PRINCIPAL
// -------------------------------------------------------------------------
// Implémentation des méthodes de NkChrono dans le namespace nkentseu.

namespace nkentseu {

    // -------------------------------------------------------------------------
    // SECTION 4 : NAMESPACE ANONYME — HELPERS INTERNES
    // -------------------------------------------------------------------------
    // Fonctions utilitaires avec linkage interne (non visibles hors de ce fichier).
    // Organisation par plateforme pour une compilation conditionnelle claire.

    namespace {

        // =====================================================================
        //  SOUS-SECTION 4.1 : Helpers Windows (QueryPerformanceCounter)
        // =====================================================================
        #if defined(NKENTSEU_PLATFORM_WINDOWS)

            /// Obtient la fréquence QPC de manière idempotente et thread-safe.
            /// @return Fréquence en ticks par seconde (typiquement ~3-4 MHz)
            /// @note Lecture unique au premier appel : pas de mutex nécessaire
            /// @note static local : initialisation thread-safe en C++11+
            NKENTSEU_INLINE int64 QPCFrequency() noexcept {
                static int64 freq = 0;
                if (NKENTSEU_UNLIKELY(freq == 0)) {
                    LARGE_INTEGER li;
                    ::QueryPerformanceFrequency(&li);
                    freq = static_cast<int64>(li.QuadPart);
                }
                return freq;
            }

            /**
             * @brief Convertit un compteur QPC en nanosecondes sans overflow.
             * @param counter Valeur brute du compteur QPC
             * @param freq Fréquence de l'horloge QPC en Hz
             * @return Durée en nanosecondes (int64)
             *
             * Formule de décomposition pour éviter l'overflow de (counter * 1e9) :
             *   ns = (counter / freq) * 1e9 + ((counter % freq) * 1e9) / freq
             *
             * Garantie de sécurité :
             *   - freq ~10^7 (10 MHz typique pour QPC)
             *   - counter % freq < freq < 10^7
             *   - (counter % freq) * 1e9 < 10^16 < INT64_MAX (~9.2e18)
             *   - Aucun __int128 ni float80 nécessaire
             */
            NKENTSEU_INLINE int64 QPCToNanoseconds(int64 counter, int64 freq) noexcept {
                // Partie entière : (counter / freq) secondes → nanosecondes
                const int64 whole = (counter / freq) * 1'000'000'000LL;

                // Partie fractionnaire : reste * 1e9 / freq
                // Garanti dans les limites d'int64 grâce à la décomposition
                const int64 frac = (counter % freq) * 1'000'000'000LL / freq;

                // Somme des deux parties : durée totale en nanosecondes
                return whole + frac;
            }

        #endif // NKENTSEU_PLATFORM_WINDOWS

        // =====================================================================
        //  SOUS-SECTION 4.2 : Helpers POSIX (clock_gettime/nanosleep)
        // =====================================================================
        #if !defined(NKENTSEU_PLATFORM_WINDOWS)

            /**
             * @brief Sleep POSIX avec reprise automatique sur EINTR.
             * @param ns Durée en nanosecondes (doit être > 0)
             * @note Utilise clock_nanosleep sur Linux (CLOCK_MONOTONIC)
             * @note Fallback vers nanosleep sur les autres systèmes POSIX
             * @note Gère les interruptions par signaux (EINTR) en bouclant
             */
            NKENTSEU_INLINE void PosixSleepNs(int64 ns) noexcept {
                // Guard : pas de sleep pour durées nulles ou négatives
                if (ns <= 0) {
                    return;
                }

                // Conversion ns → timespec (seconds + nanosecondes résiduelles)
                struct timespec req;
                req.tv_sec = static_cast<time_t>(ns / 1'000'000'000LL);
                req.tv_nsec = static_cast<long>(ns % 1'000'000'000LL);

                #if defined(__linux__)
                    // Linux : clock_nanosleep avec CLOCK_MONOTONIC
                    // Plus précis que nanosleep car indépendant de l'heure système
                    while (::clock_nanosleep(CLOCK_MONOTONIC, 0, &req, &req) == -1
                        && errno == EINTR) {
                        // Reprise automatique sur interruption par signal
                        // req est mis à jour avec le temps restant par clock_nanosleep
                    }
                #else
                    // Autres POSIX : nanosleep avec gestion manuelle du reste
                    struct timespec rem;
                    while (::nanosleep(&req, &rem) == -1 && errno == EINTR) {
                        // Sur EINTR, nanosleep met à jour 'rem' avec le temps restant
                        // On réessaie avec le temps restant jusqu'à complétion
                        req = rem;
                    }
                #endif
            }

        #endif // !NKENTSEU_PLATFORM_WINDOWS

    } // namespace anonymous

    // =============================================================================
    //  NkChrono — Constructeur
    // =============================================================================
    // Initialisation du timestamp de départ via Now() pour démarrer la mesure
    // immédiatement à la construction de l'objet.

    NkChrono::NkChrono() noexcept
        : mStartTime(Now())
    {
        // Initialisation via liste de membres : efficace et clair
        // Now() capture le timestamp monotone courant comme point de référence
    }

    // =============================================================================
    //  NkChrono — Now() : Timestamp absolu monotone
    // =============================================================================
    // Méthode statique retournant le temps absolu courant en nanosecondes.
    // Implémentation multiplateforme avec gestion des spécificités de chaque OS.

    NkElapsedTime NkChrono::Now() noexcept {
        // Variable pour accumuler le résultat en nanosecondes
        int64 ns = 0;

        #if defined(NKENTSEU_PLATFORM_WINDOWS)
            // Windows : QueryPerformanceCounter (QPC) pour haute précision
            LARGE_INTEGER counter;
            ::QueryPerformanceCounter(&counter);

            // Conversion safe sans overflow via la fonction helper
            ns = QPCToNanoseconds(
                static_cast<int64>(counter.QuadPart),
                QPCFrequency()
            );
        #else
            // POSIX : clock_gettime avec CLOCK_MONOTONIC
            // Résolution nominale : 1 nanoseconde
            struct timespec ts;
            ::clock_gettime(CLOCK_MONOTONIC, &ts);

            // Combinaison secondes + nanosecondes en une seule valeur int64
            ns = static_cast<int64>(ts.tv_sec) * 1'000'000'000LL
               + static_cast<int64>(ts.tv_nsec);
        #endif

        // Construction du résultat via la fabrique NkElapsedTime
        // Conversion int64 → float64 pour le stockage interne de NkElapsedTime
        return NkElapsedTime::FromNanoseconds(static_cast<float64>(ns));
    }

    // =============================================================================
    //  NkChrono — Elapsed() : Lecture sans remise à zéro
    // =============================================================================
    // Calcule la durée écoulée depuis le dernier Reset() ou la construction.
    // Ne modifie pas l'état interne : méthode const, thread-safe pour la lecture.

    NkElapsedTime NkChrono::Elapsed() const noexcept {
        // Différence entre le timestamp courant et le timestamp de départ
        // NkElapsedTime supporte l'opérateur - pour la soustraction de durées
        return Now() - mStartTime;
    }

    // =============================================================================
    //  NkChrono — Reset() : Lecture ET remise à zéro atomique
    // =============================================================================
    // Lit la durée écoulée puis met à jour le timestamp de départ.
    // Atomique du point de vue de l'appelant : idéal pour mesurer des intervalles.

    NkElapsedTime NkChrono::Reset() noexcept {
        // Capture du timestamp courant avant toute modification
        const NkElapsedTime now = Now();

        // Calcul de la durée écoulée depuis le dernier point de référence
        const NkElapsedTime elapsed = now - mStartTime;

        // Mise à jour du point de référence pour le prochain intervalle
        mStartTime = now;

        // Retour de la durée mesurée avant remise à zéro
        return elapsed;
    }

    // =============================================================================
    //  NkChrono — GetFrequency() : Fréquence de l'horloge sous-jacente
    // =============================================================================
    // Retourne la fréquence en Hz de l'horloge utilisée pour les mesures.
    // Utile pour la conversion manuelle de ticks ou le debugging de précision.

    int64 NkChrono::GetFrequency() noexcept {
        #if defined(NKENTSEU_PLATFORM_WINDOWS)
            // Windows : fréquence QPC via le helper cached
            return QPCFrequency();
        #else
            // POSIX : CLOCK_MONOTONIC a une résolution nominale de 1 ns
            // Donc fréquence théorique = 1 GHz = 1'000'000'000 Hz
            // Note : la précision réelle dépend du matériel et du noyau
            return 1'000'000'000LL;
        #endif
    }

    // =============================================================================
    //  NkChrono — Sleep : Implémentations multiplateformes
    // =============================================================================
    // Méthodes pour endormir le thread courant avec différentes granularités.
    // Gestion conservative des limitations de chaque plateforme.

    void NkChrono::Sleep(const NkDuration& duration) noexcept {
        // Extraction de la durée en nanosecondes depuis NkDuration
        const int64 ns = duration.ToNanoseconds();

        // Guard : pas de sleep pour durées nulles ou négatives
        if (ns <= 0) {
            return;
        }

        #if defined(NKENTSEU_PLATFORM_WINDOWS)
            // Windows : ::Sleep() prend des millisecondes (DWORD)
            // Granularité typique : ~15.6 ms sans timeBeginPeriod
            const DWORD ms = static_cast<DWORD>(ns / 1'000'000LL);

            // Pour les durées < 1ms, Sleep(1) est l'approximation conservative
            // Une implémentation busy-wait précise pourrait être ajoutée si nécessaire
            ::Sleep(ms > 0 ? ms : 1);

        #elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
            // Emscripten : emscripten_sleep() pour céder au navigateur
            // Nécessite -s ASYNCIFY=1 pour fonctionner correctement
            // Durée minimale : 1 ms (les valeurs < 1 sont arrondies)
            int ms = static_cast<int>(ns / 1'000'000LL);
            if (ms < 1) {
                ms = 1;
            }
            emscripten_sleep(ms);

        #else
            // POSIX : helper avec clock_nanosleep/nanosleep et gestion EINTR
            PosixSleepNs(ns);
        #endif
    }

    void NkChrono::Sleep(int64 milliseconds) noexcept {
        // Guard pour valeurs invalides
        if (milliseconds <= 0) {
            return;
        }
        // Délégation à la version principale via NkDuration
        Sleep(NkDuration::FromMilliseconds(milliseconds));
    }

    void NkChrono::Sleep(float64 milliseconds) noexcept {
        // Guard pour valeurs invalides
        if (milliseconds <= 0.0) {
            return;
        }
        // Délégation à la version principale via NkDuration
        // NkDuration::FromMilliseconds(float64) gère la conversion précise
        Sleep(NkDuration::FromMilliseconds(milliseconds));
    }

    void NkChrono::SleepMilliseconds(int64 ms) noexcept {
        // Alias explicite pour la lisibilité : délègue directement
        Sleep(ms);
    }

    void NkChrono::SleepMicroseconds(int64 us) noexcept {
        // Guard pour valeurs invalides
        if (us <= 0) {
            return;
        }
        // Conversion via NkDuration puis délégation
        Sleep(NkDuration::FromMicroseconds(us));
    }

    void NkChrono::SleepNanoseconds(int64 ns) noexcept {
        // Guard pour valeurs invalides
        if (ns <= 0) {
            return;
        }

        #if defined(NKENTSEU_PLATFORM_WINDOWS)
            // Windows : pas de sleep sub-milliseconde fiable via l'API standard
            // Sleep(1) est l'approximation conservative (~15ms réel sans réglage)
            // Pour une précision sub-ms, une implémentation busy-wait serait nécessaire
            ::Sleep(1);

        #elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
            // Emscripten : arrondi au milliseconde supérieure
            int ms = static_cast<int>(ns / 1'000'000LL);
            if (ms < 1) {
                ms = 1;
            }
            emscripten_sleep(ms);

        #else
            // POSIX : nanosecondes supportées nativement via clock_nanosleep
            PosixSleepNs(ns);
        #endif
    }

    // =============================================================================
    //  NkChrono — YieldThread : Cession du quantum CPU
    // =============================================================================
    // Méthode pour céder volontairement le temps CPU au scheduler du système.
    // Utile dans les boucles d'attente active pour réduire la consommation.

    void NkChrono::YieldThread() noexcept {
        #if defined(NKENTSEU_PLATFORM_WINDOWS)
            // Windows : SwitchToThread() cède aux threads de même priorité
            // Retourne TRUE si un autre thread a été exécuté, FALSE sinon
            ::SwitchToThread();

        #elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
            // Emscripten : emscripten_sleep(0) cède au navigateur sans bloquer
            // Permet au runtime Web de traiter les événements en attente
            emscripten_sleep(0);

        #else
            // POSIX : sched_yield() cède le CPU aux autres threads prêts
            // Ne garantit pas l'ordre d'exécution, juste une opportunité de scheduling
            ::sched_yield();
        #endif
    }

    // =============================================================================
    //  NkElapsedTime — ToString : Formatage adaptatif
    // =============================================================================
    // Implémentation de la méthode de formatage pour NkElapsedTime.
    // Choisit automatiquement l'unité la plus lisible selon la magnitude.

    NkString NkElapsedTime::ToString() const {
        // Buffer local pour le formatage (taille suffisante pour tous les cas)
        // Format max : "-999999999.999 s\0" = ~15 chars, 64 pour marge de sécurité
        char buf[64];

        // Sélection de l'unité adaptative par ordre décroissant de magnitude
        // Utilisation des champs précalculés de NkElapsedTime : zéro calcul à l'accès
        if (seconds >= 1.0) {
            // Durées >= 1 seconde : affichage en secondes avec 3 décimales
            ::snprintf(buf, sizeof(buf), "%.3f s", seconds);
        }
        else if (milliseconds >= 1.0) {
            // Durées >= 1 ms : affichage en millisecondes avec 3 décimales
            ::snprintf(buf, sizeof(buf), "%.3f ms", milliseconds);
        }
        else if (microseconds >= 1.0) {
            // Durées >= 1 µs : affichage en microsecondes avec 3 décimales
            ::snprintf(buf, sizeof(buf), "%.3f us", microseconds);
        }
        else {
            // Durées < 1 µs : affichage en nanosecondes avec 3 décimales
            ::snprintf(buf, sizeof(buf), "%.3f ns", nanoseconds);
        }

        // Construction de la chaîne NKEntseu depuis le buffer C
        return NkString(buf);
    }

} // namespace nkentseu

// =============================================================================
// NOTES D'IMPLÉMENTATION
// =============================================================================
/*
    Précision et limitations par plateforme :
    ----------------------------------------
    Windows (QPC) :
    - Précision typique : ~100 ns selon le matériel et les pilotes
    - Fréquence : variable (3-4 MHz typique), lue une fois au premier appel
    - Limitation Sleep : granularité ~15.6 ms sans timeBeginPeriod(1)
    - Pour Sleep sub-ms : nécessite busy-wait ou APIs temps-réel

    POSIX (clock_gettime) :
    - Précision nominale : 1 ns via CLOCK_MONOTONIC
    - Précision réelle : dépend du hardware (TSC, HPET, etc.) et du noyau
    - clock_nanosleep (Linux) : relatif à CLOCK_MONOTONIC, plus précis
    - nanosleep (autres) : relatif à l'heure réelle, peut être affecté par NTP

    Emscripten (Web) :
    - emscripten_sleep() : nécessite -s ASYNCIFY=1 pour fonctionner
    - Précision : limitée par le scheduler du navigateur (~1-16 ms)
    - emscripten_sleep(0) : yield sans blocage pour les boucles actives

    Gestion des interruptions (EINTR) :
    ----------------------------------
    - Les appels de sleep POSIX peuvent être interrompus par des signaux
    - PosixSleepNs() boucle automatiquement sur EINTR pour reprendre le sleep
    - Comportement conforme aux bonnes pratiques POSIX

    Thread-safety :
    --------------
    - NkChrono::Now() : thread-safe (appels système atomiques)
    - NkChrono instance : non thread-safe pour Elapsed()/Reset()
      (lecture/écriture de mStartTime sans synchronisation)
    - Solution : un NkChrono par thread, ou synchronisation externe

    Performance :
    ------------
    - Now() : 1 appel système (QPC ou clock_gettime) → ~20-100 cycles CPU
    - Elapsed() : Now() + soustraction float64 → négligeable
    - Reset() : Elapsed() + affectation → négligeable
    - Sleep() : appel système bloquant → dépend du scheduler OS

    Overflow protection (Windows QPC) :
    ----------------------------------
    - Formule : (c/f)*1e9 + ((c%f)*1e9)/f
    - Garantie : (c%f) < f ~1e7 → (c%f)*1e9 < 1e16 < INT64_MAX (~9.2e18)
    - Aucun __int128 requis, calcul purement entier en int64

    Évolution future :
    -----------------
    - Ajout de timeBeginPeriod(1) optionnel pour Sleep sub-ms sur Windows
    - Support de CLOCK_BOOTTIME pour les mesures incluant le temps de suspend
    - Intégration avec les timers haute précision (HPET, TSC invariant)
*/

// ============================================================
// Copyright © 2024-2026 TEUGUIA TADJUIDJE Rodolf Séderis.
// Apache-2.0 License - Free to use and modify
// ============================================================