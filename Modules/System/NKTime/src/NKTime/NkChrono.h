// =============================================================================
// NKTime/NkChrono.h
// Chronomètre haute précision — sans dépendance STL.
//
// Design :
//  - NkChrono mesure des durées relatives via un timestamp de départ
//  - Précision plateforme :
//    * Windows  : QueryPerformanceCounter (~100 ns selon matériel)
//    * Linux    : clock_gettime(CLOCK_MONOTONIC) (~1 ns)
//    * macOS    : clock_gettime(CLOCK_MONOTONIC) (~1 ns)
//  - Séparation claire des responsabilités :
//    * NkElapsedTime : résultat de mesure (float64, 4 unités précalculées)
//    * NkDuration    : durée à spécifier (int64 ns, API mutable)
//    * NkChrono      : orchestrateur de mesure + sleep/yield
//
// Règles d'application des macros :
//  - NKENTSEU_TIME_CLASS_EXPORT sur la classe uniquement
//  - PAS de NKENTSEU_TIME_API sur les méthodes (l'export de classe suffit)
//  - PAS de macro sur les fonctions inline/constexpr définies dans le header
//
// Auteur : TEUGUIA TADJUIDJE Rodolf Séderis
// Date : 2024-2026
// License : Apache-2.0
// =============================================================================

#pragma once

#ifndef NKENTSEU_TIME_NKCHRONO_H
#define NKENTSEU_TIME_NKCHRONO_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion de l'API d'export NKTime (remplace l'ancien NkTimeExport.h)
    // Inclusion de NkElapsedTime pour les résultats de mesure
    // Inclusion de NkDuration pour les paramètres de sleep
    // Inclusion des types de base NKCore

    #include "NKTime/NkTimeApi.h"
    #include "NKTime/NkElapsedTime.h"
    #include "NKTime/NkDuration.h"
    #include "NKCore/NkTypes.h"

    // -------------------------------------------------------------------------
    // SECTION 3 : NAMESPACE PRINCIPAL ET DÉCLARATION DE LA CLASSE
    // -------------------------------------------------------------------------
    // La classe NkChrono fournit des primitives de mesure temporelle haute
    // précision et des utilitaires de synchronisation (sleep, yield).

    namespace nkentseu {

        // Classe exportée : les méthodes n'ont PAS besoin de macro d'export
        class NKENTSEU_TIME_CLASS_EXPORT NkChrono {

        public:

            // -------------------------------------------------------------
            // SOUS-SECTION 3.1 : Constructeurs
            // -------------------------------------------------------------

            /// Démarre le chronomètre immédiatement à la construction.
            /// @note Capture le timestamp courant via Now() en interne
            /// @note noexcept : aucune allocation, aucun appel système bloquant
            NkChrono() noexcept;

            /// Destructeur par défaut : rien à libérer (RAII simple).
            ~NkChrono() = default;

            // Copie autorisée : NkChrono est triviallement copiable
            // La copie capture l'état courant du timestamp de départ
            NkChrono(const NkChrono&) = default;
            NkChrono& operator=(const NkChrono&) = default;

            // -------------------------------------------------------------
            // SOUS-SECTION 3.2 : Mesure de durée
            // -------------------------------------------------------------
            // Méthodes principales pour obtenir des durées écoulées.

            /**
             * @brief Temps écoulé depuis la dernière construction ou Reset().
             * @return NkElapsedTime représentant la durée écoulée
             * @note Ne remet PAS le chrono à zéro : lecture seule
             * @note Thread-safe : lecture atomique du timestamp interne
             */
            NKTIME_NODISCARD NkElapsedTime Elapsed() const noexcept;

            /**
             * @brief Lit le temps écoulé PUIS remet le chrono à zéro.
             * @return Durée écoulée avant la remise à zéro
             * @note Opération atomique du point de vue de l'appelant
             * @note Utile pour mesurer des intervalles successifs (ex: frame time)
             */
            NKTIME_NODISCARD NkElapsedTime Reset() noexcept;

            // -------------------------------------------------------------
            // SOUS-SECTION 3.3 : Horloge de base
            // -------------------------------------------------------------
            // Méthodes statiques pour l'accès direct à l'horloge système.

            /**
             * @brief Temps absolu monotonique en nanosecondes.
             * @return NkElapsedTime représentant le timestamp courant
             * @note L'origine est arbitraire (boot, epoch, etc.) mais stable
             * @note Monotone : ne recule jamais, même en cas de changement d'heure
             * @note Utilisez Elapsed() pour des mesures relatives préférablement
             */
            NKTIME_NODISCARD static NkElapsedTime Now() noexcept;

            /**
             * @brief Fréquence de l'horloge sous-jacente en Hz.
             * @return Fréquence en Hz (ticks par seconde)
             * @note Windows : fréquence QPC (typiquement ~3-4 MHz)
             * @note POSIX   : 1'000'000'000 (résolution nominale 1 ns)
             * @note Utile pour convertir manuellement des ticks en durée
             */
            NKTIME_NODISCARD static int64 GetFrequency() noexcept;

            // -------------------------------------------------------------
            // SOUS-SECTION 3.4 : Sommeil (Sleep)
            // -------------------------------------------------------------
            // Méthodes statiques pour endormir le thread courant.
            // Toutes délèguent à l'implémentation plateforme appropriée.

            /// Endort le thread courant pour la durée spécifiée.
            /// @param duration Durée à attendre (NkDuration)
            /// @note Précision dépendante de la plateforme et du scheduler OS
            /// @note Peut retourner avant le délai si un signal interrompt le sleep
            static void Sleep(const NkDuration& duration) noexcept;

            /// Endort le thread courant pour un nombre de millisecondes (entier).
            /// @param milliseconds Durée en millisecondes
            /// @note Commodité : évite de construire un NkDuration manuellement
            static void Sleep(int64 milliseconds) noexcept;

            /// Endort le thread courant pour un nombre de millisecondes (flottant).
            /// @param milliseconds Durée en millisecondes (permet les sous-ms)
            /// @note Utile pour des délais précis comme 16.67ms (60 FPS)
            static void Sleep(float64 milliseconds) noexcept;

            /// Endort le thread courant pour un nombre de millisecondes (nom explicite).
            /// @param ms Durée en millisecondes
            static void SleepMilliseconds(int64 ms) noexcept;

            /// Endort le thread courant pour un nombre de microsecondes.
            /// @param us Durée en microsecondes
            /// @note Précision réelle dépend de la résolution du timer système
            static void SleepMicroseconds(int64 us) noexcept;

            /// Endort le thread courant pour un nombre de nanosecondes.
            /// @param ns Durée en nanosecondes
            /// @note Sur Windows, les durées < 1ms sont arrondies à 1ms minimum
            static void SleepNanoseconds(int64 ns) noexcept;

            // -------------------------------------------------------------
            // SOUS-SECTION 3.5 : Scheduling (Yield)
            // -------------------------------------------------------------

            /// Cède le quantum restant du thread au scheduler OS.
            /// @note Utile dans les boucles d'attente active pour réduire la charge CPU
            /// @note Ne garantit pas l'ordre d'exécution des threads
            /// @note Retourne immédiatement : pas de blocage
            static void YieldThread() noexcept;

        private:

            // -------------------------------------------------------------
            // SOUS-SECTION 3.6 : Membre privé
            // -------------------------------------------------------------
            // Timestamp absolu du dernier Reset() ou de la construction.
            // Source de vérité pour tous les calculs de durée relative.

            NkElapsedTime mStartTime;

        }; // class NkChrono

    } // namespace nkentseu

#endif // NKENTSEU_TIME_NKCHRONO_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKCHRONO.H
// =============================================================================
// NkChrono fournit des primitives de chronométrage haute précision et des
// utilitaires de synchronisation pour les applications temps-réel.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Mesure simple d'une opération
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkChrono.h"
    #include "NKCore/Logger/NkLogger.h"

    void MeasureOperationExample() {
        using namespace nkentseu;

        // Création du chrono : démarre immédiatement
        NkChrono chrono;

        // ... code à mesurer ...
        // PerformExpensiveCalculation();

        // Lecture de la durée écoulée sans remise à zéro
        NkElapsedTime elapsed = chrono.Elapsed();

        // Logging avec formatage automatique
        NK_LOG_INFO("Operation took: {}", elapsed.ToString());
        // Output: "Operation took: 45.678 ms"

        // Accès précis aux différentes unités (zéro calcul)
        float64 ms = elapsed.ToMilliseconds();   // 45.678
        float64 us = elapsed.ToMicroseconds();   // 45678.0
        float64 ns = elapsed.ToNanoseconds();    // 45678000.0
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Mesure de frames successives avec Reset()
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkChrono.h"
    #include "NKCore/Logger/NkLogger.h"

    void FrameTimingExample() {
        using namespace nkentseu;

        NkChrono frameTimer;

        while (IsRunning()) {
            // Reset() lit ET remet à zéro : parfait pour delta time
            NkElapsedTime frameTime = frameTimer.Reset();

            // Mise à jour de la logique avec le delta time
            // UpdateGameLogic(frameTime.ToSeconds());

            // Rendu
            // RenderFrame();

            // Logging périodique des performances
            static uint64 frameCount = 0;
            if (++frameCount % 60 == 0) {
                NK_LOG_INFO("Frame {}: {} ms ({} FPS)",
                            frameCount,
                            frameTime.ToMilliseconds(),
                            1000.0 / frameTime.ToMilliseconds());
            }
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Sleep avec différentes unités et précision
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkChrono.h"
    #include "NKTime/NkDuration.h"

    void SleepExamples() {
        using namespace nkentseu;

        // Sleep avec NkDuration (recommandé pour la clarté)
        NkChrono::Sleep(NkDuration::FromMilliseconds(16));  // ~60 FPS cap

        // Sleep avec durée flottante pour précision sub-milliseconde
        NkChrono::Sleep(16.67);  // 16.67ms exact pour 60 FPS

        // Sleep avec durée entière (commodité)
        NkChrono::Sleep(100);  // 100ms

        // Sleep avec noms explicites pour la lisibilité
        NkChrono::SleepMilliseconds(50);    // 50ms
        NkChrono::SleepMicroseconds(500);   // 500µs
        NkChrono::SleepNanoseconds(500000); // 500µs (équivalent)

        // Pattern : cap de FPS avec sleep adaptatif
        void CapFrameRate(float targetFPS) {
            static NkChrono frameClock;
            const float targetFrameTime = 1000.f / targetFPS;  // en ms

            NkElapsedTime frameTime = frameClock.Reset();
            float elapsedMs = static_cast<float>(frameTime.ToMilliseconds());

            if (elapsedMs < targetFrameTime) {
                float sleepMs = targetFrameTime - elapsedMs;
                NkChrono::Sleep(sleepMs);  // Sleep du temps restant
            }
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : YieldThread pour boucles d'attente active
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkChrono.h"
    #include "NKCore/Atomic/NkAtomic.h"  // Hypothétique

    void ActiveWaitExample() {
        using namespace nkentseu;

        // Attente sur un flag atomique sans bloquer le thread
        void WaitForFlag(const NkAtomicBool& flag, int timeoutMs) {
            NkChrono timeout;

            while (!flag.Load() && timeout.Elapsed().ToMilliseconds() < timeoutMs) {
                // Yield pour céder le CPU aux autres threads
                // Évite le busy-wait à 100% CPU
                NkChrono::YieldThread();
            }
        }

        // Pattern : polling avec backoff exponentiel
        void PollWithBackoff(std::function<bool()> condition, int maxAttempts) {
            int attempt = 0;
            int sleepMs = 1;  // Commence avec 1ms

            while (attempt < maxAttempts && !condition()) {
                ++attempt;
                NkChrono::SleepMilliseconds(sleepMs);
                sleepMs = std::min(sleepMs * 2, 100);  // Backoff jusqu'à 100ms
            }
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Now() pour timestamps absolus et synchronisation
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkChrono.h"
    #include "NKCore/Logger/NkLogger.h"

    void AbsoluteTimestampExample() {
        using namespace nkentseu;

        // Capture d'un timestamp absolu pour logging ou synchronisation
        NkElapsedTime startupTime = NkChrono::Now();

        // ... application runtime ...

        // Calcul du temps de fonctionnement total
        NkElapsedTime uptime = NkChrono::Now() - startupTime;
        NK_LOG_INFO("Uptime: {}", uptime.ToString());

        // Pattern : timeout absolu pour opérations asynchrones
        bool WaitForResponseWithTimeout(
            std::function<bool()> hasResponse,
            float timeoutSeconds
        ) {
            NkElapsedTime deadline = NkChrono::Now();
            // Ajout du timeout en secondes (conversion via NkElapsedTime)
            // Note : NkElapsedTime supporte l'addition de float64 secondes
            // via ses opérateurs arithmétiques

            while (!hasResponse()) {
                if ((NkChrono::Now() - deadline).ToSeconds() >= timeoutSeconds) {
                    return false;  // Timeout expiré
                }
                NkChrono::YieldThread();  // Cède le CPU en attendant
            }
            return true;  // Réponse reçue avant timeout
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 6 : GetFrequency() pour conversion manuelle de ticks
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkChrono.h"
    #include "NKCore/Logger/NkLogger.h"

    void FrequencyExample() {
        using namespace nkentseu;

        // Obtention de la fréquence de l'horloge sous-jacente
        int64 freq = NkChrono::GetFrequency();
        NK_LOG_INFO("Timer frequency: {} Hz", freq);
        // Windows: typiquement 3'579'545 Hz (~3.58 MHz)
        // POSIX:   toujours 1'000'000'000 Hz (1 GHz nominal)

        // Conversion manuelle de ticks en durée (si nécessaire)
        // Note : NkChrono gère déjà cette conversion en interne,
        // mais GetFrequency() est utile pour l'interop avec des APIs bas-niveau.

        void LogRawTicks() {
        #if defined(NKENTSEU_PLATFORM_WINDOWS)
            LARGE_INTEGER ticks;
            ::QueryPerformanceCounter(&ticks);
            int64 freq = NkChrono::GetFrequency();

            // Conversion manuelle : ticks → nanosecondes
            int64 ns = (ticks.QuadPart / freq) * 1'000'000'000LL
                     + (ticks.QuadPart % freq) * 1'000'000'000LL / freq;

            NK_LOG_INFO("Raw ticks: {} -> {} ns", ticks.QuadPart, ns);
        #endif
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Copie et réutilisation de NkChrono
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkChrono.h"

    void ChronoCopyExample() {
        using namespace nkentseu;

        // NkChrono est triviallement copiable : la copie capture l'état courant
        NkChrono mainTimer;

        // ... du temps passe ...

        // Copie du chrono : les deux partagent le même point de départ
        NkChrono backupTimer = mainTimer;

        // Les deux retournent la même durée écoulée
        NkElapsedTime t1 = mainTimer.Elapsed();
        NkElapsedTime t2 = backupTimer.Elapsed();
        // t1 == t2 (aux imprécisions de mesure près)

        // Reset() sur l'un n'affecte pas l'autre
        NkElapsedTime delta = mainTimer.Reset();  // Remet mainTimer à zéro
        // backupTimer.Elapsed() retourne toujours la durée depuis l'origine

        // Pattern : checkpoint temporel pour mesures imbriquées
        void NestedTimingExample() {
            NkChrono totalTimer;  // Timer pour la durée totale

            // ... setup ...

            NkChrono phaseTimer;  // Timer pour une phase spécifique
            // DoPhase1();
            NkElapsedTime phase1Time = phaseTimer.Reset();

            // DoPhase2();
            NkElapsedTime phase2Time = phaseTimer.Elapsed();

            // Durée totale incluant les deux phases
            NkElapsedTime totalTime = totalTimer.Elapsed();
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 8 : Intégration avec NkClock pour game loop
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkChrono.h"
    #include "NKTime/NkClock.h"  // Module séparé utilisant NkChrono

    namespace game {

        // NkClock utilise NkChrono en interne pour la mesure des frames
        // Exemple d'intégration manuelle si NkClock n'est pas utilisé

        class SimpleLoop {
        public:
            void Run() {
                nkentseu::NkChrono frameChrono;
                nkentseu::NkChrono totalChrono;

                const float targetFrameTime = 1.f / 60.f;  // 60 FPS cible

                while (mRunning) {
                    // Mesure du delta time
                    nkentseu::NkElapsedTime frameTime = frameChrono.Reset();
                    float delta = static_cast<float>(frameTime.ToSeconds());

                    // Mise à jour avec time scale optionnel
                    // UpdateLogic(delta * mTimeScale);

                    // Rendu
                    // RenderFrame(delta);

                    // Cap de FPS : sleep du temps restant
                    float elapsed = static_cast<float>(frameTime.ToSeconds());
                    if (elapsed < targetFrameTime) {
                        float sleepSec = targetFrameTime - elapsed;
                        nkentseu::NkChrono::Sleep(sleepSec * 1000.f);  // en ms
                    }
                }
            }

        private:
            bool mRunning = true;
            float mTimeScale = 1.f;
        };

    } // namespace game
*/

// ============================================================
// Copyright © 2024-2026 TEUGUIA TADJUIDJE Rodolf Séderis.
// Apache-2.0 License - Free to use and modify
// ============================================================