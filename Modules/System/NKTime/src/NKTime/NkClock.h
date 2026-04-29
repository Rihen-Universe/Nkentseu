// =============================================================================
// NKTime/NkClock.h
// Orchestrateur de frame — gère NkTime pour la game loop.
//
// Design :
//  - NkClock::NkTime est distinct de ::nkentseu::NkTime (heure du jour HH:MM:SS)
//  - NkClock::NkTime est un snapshot de frame : delta, fps, frameCount, timeScale
//  - Deux NkChrono internes :
//    * mFrameChrono — resetté à chaque Tick() → delta de frame
//    * mTotalChrono — jamais resetté          → temps depuis construction/Reset()
//  - Support du pause/resume avec préservation du temps total
//  - Time scale pour ralentir/accélérer la logique métier sans affecter le rendu
//
// Règles d'application des macros :
//  - NKENTSEU_TIME_CLASS_EXPORT sur la classe uniquement
//  - PAS de NKENTSEU_TIME_API sur les méthodes (l'export de classe suffit)
//  - PAS de macro sur les fonctions inline/constexpr définies dans le header
//  - Struct interne NkTime : exporté via NKENTSEU_TIME_CLASS_EXPORT, méthodes sans macro
//
// Auteur : TEUGUIA TADJUIDJE Rodolf Séderis
// Date : 2024-2026
// License : Apache-2.0
// =============================================================================

#pragma once

#ifndef NKENTSEU_TIME_NKCLOCK_H
#define NKENTSEU_TIME_NKCLOCK_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion de l'API d'export NKTime (remplace l'ancien NkTimeExport.h)
    // Inclusion de NkChrono pour la mesure du temps haute précision
    // Inclusion de NkDuration pour les méthodes de sleep déléguées
    // Inclusion de NkElapsedTime pour les snapshots de durée mesurée

    #include "NKTime/NkTimeApi.h"
    #include "NKTime/NkChrono.h"
    #include "NKTime/NkDuration.h"
    #include "NKTime/NkElapsedTime.h"

    // -------------------------------------------------------------------------
    // SECTION 3 : NAMESPACE PRINCIPAL ET DÉCLARATION DE LA CLASSE
    // -------------------------------------------------------------------------
    // La classe NkClock orchestre la boucle de jeu en fournissant des snapshots
    // temporels cohérents à chaque frame. Elle gère delta time, FPS, time scale.

    namespace nkentseu {

        // Classe exportée : les méthodes n'ont PAS besoin de macro d'export
        class NKENTSEU_TIME_CLASS_EXPORT NkClock {

        public:

            // -------------------------------------------------------------
            // SOUS-SECTION 3.1 : Struct NkTime (snapshot de frame)
            // -------------------------------------------------------------
            // NkTime représente l'état temporel d'une frame unique.
            // Exporté via NKENTSEU_TIME_CLASS_EXPORT, méthodes sans macro.

            struct NKENTSEU_TIME_CLASS_EXPORT NkTime {

                // ---------------------------------------------------------
                // Champs publics — lecture directe, zéro calcul à l'accès
                // ---------------------------------------------------------
                // Tous les champs sont publics pour performance et simplicité.
                // Initialisés avec des valeurs par défaut sensées.

                float32 delta;          ///< Durée de la dernière frame en secondes
                float32 total;          ///< Temps total écoulé depuis Reset() en secondes
                uint64  frameCount;     ///< Nombre total de frames depuis Reset()
                float32 fps;            ///< Frames par seconde estimées (moyenne glissante)
                float32 fixedDelta;     ///< Delta fixe pour la logique déterministe (ex: physique)
                float32 timeScale;      ///< Facteur de ralentissement/accélération (1.0 = normal)

                // ---------------------------------------------------------
                // Méthodes inline — calculs simples, constexpr si possible
                // ---------------------------------------------------------
                // Ces méthodes sont définies inline dans le header pour performance.
                // PAS de macro NKENTSEU_TIME_API sur les méthodes de classe.

                /// Retourne le delta time appliqué du time scale.
                /// @return delta * timeScale pour la logique métier
                NKTIME_NODISCARD NKENTSEU_INLINE constexpr float32 Scaled() const noexcept {
                    return delta * timeScale;
                }

                /// Retourne le fixed delta appliqué du time scale.
                /// @return fixedDelta * timeScale pour la physique déterministe
                NKTIME_NODISCARD NKENTSEU_INLINE constexpr float32 FixedScaled() const noexcept {
                    return fixedDelta * timeScale;
                }

                // ---------------------------------------------------------
                // Fabrique statique — construction depuis composants bruts
                // ---------------------------------------------------------

                /// Construit un snapshot NkTime depuis des valeurs brutes.
                /// @param delta Durée de la frame courante (NkElapsedTime)
                /// @param total Temps total écoulé (NkElapsedTime)
                /// @param frame Numéro de frame courante
                /// @param fixedDt Valeur de fixedDelta à utiliser
                /// @param scale Valeur de timeScale à appliquer
                /// @return Nouvelle instance NkTime initialisée
                NKTIME_NODISCARD static NkTime From(
                    const NkElapsedTime& delta,
                    const NkElapsedTime& total,
                    uint64 frame,
                    float32 fixedDt,
                    float32 scale
                ) noexcept;

            }; // struct NkTime

            // -------------------------------------------------------------
            // SOUS-SECTION 3.2 : Constructeur / Destructeur
            // -------------------------------------------------------------

            /// Constructeur par défaut : initialise les chronos et l'état.
            /// @note noexcept : aucune allocation, aucun appel système
            NkClock() noexcept;

            /// Destructeur par défaut : rien à libérer (RAII simple).
            ~NkClock() = default;

            // Suppression des copies : NkClock gère un état mutable interne
            NkClock(const NkClock&) = delete;
            NkClock& operator=(const NkClock&) = delete;

            // -------------------------------------------------------------
            // SOUS-SECTION 3.3 : Cycle de vie
            // -------------------------------------------------------------

            /// Remet à zéro les chronos et le compteur de frames.
            /// @note Conserve timeScale et fixedDelta configurés avant l'appel
            /// @note Thread-safe : à appeler depuis le thread principal uniquement
            void Reset() noexcept;

            // -------------------------------------------------------------
            // SOUS-SECTION 3.4 : Gestion des frames
            // -------------------------------------------------------------

            /**
             * @brief Avance d'une frame et retourne le snapshot mis à jour.
             * @return Référence constante vers le snapshot NkTime courant
             * @note En pause : delta = fps = 0, total continue, frameCount s'incrémente
             * @note La référence retournée est valide jusqu'au prochain Tick() ou Reset()
             */
            const NkTime& Tick() noexcept;

            /// Obtient le snapshot temporel courant sans avancer la frame.
            /// @return Référence constante vers le snapshot NkTime courant
            /// @note Utile pour lire l'état sans déclencher de nouvelle frame
            NKTIME_NODISCARD const NkTime& GetTime() const noexcept {
                return mTime;
            }

            // -------------------------------------------------------------
            // SOUS-SECTION 3.5 : Paramètres de contrôle
            // -------------------------------------------------------------
            // Méthodes pour configurer le comportement temporel du clock.

            /// Définit le facteur de time scale (ralentissement/accélération).
            /// @param scale Facteur multiplicatif (1.0 = normal, 0.5 = ralenti 2x)
            /// @note Affecte Scaled() et FixedScaled(), pas delta brut
            void SetTimeScale(float32 scale) noexcept;

            /// Définit le delta fixe pour la logique déterministe.
            /// @param seconds Durée fixe en secondes (ex: 1/60 pour 60 FPS)
            /// @note Utilisé par FixedScaled() pour la physique pas-à-pas
            void SetFixedDelta(float32 seconds) noexcept;

            /// Met le clock en pause : delta et fps deviennent 0.
            /// @note Le temps total continue de s'écouler (mTotalChrono non affecté)
            /// @note frameCount continue de s'incrémenter pour le debugging
            void Pause() noexcept;

            /// Reprend le clock après une pause.
            /// @note Restaure le calcul normal de delta et fps au prochain Tick()
            void Resume() noexcept;

            /// Vérifie si le clock est actuellement en pause.
            /// @return true si pause active, false sinon
            NKTIME_NODISCARD bool IsPaused() const noexcept {
                return mPaused;
            }

            // -------------------------------------------------------------
            // SOUS-SECTION 3.6 : Sleep / Yield (délégation à NkChrono)
            // -------------------------------------------------------------
            // Méthodes statiques deleguant directement à NkChrono pour commodité.
            // Permettent d'utiliser NkClock::Sleep() sans inclure NkChrono séparément.

            /// Endort le thread courant pour une durée spécifiée.
            /// @param d Durée à attendre (NkDuration)
            static void Sleep(const NkDuration& d) noexcept {
                NkChrono::Sleep(d);
            }

            /// Endort le thread courant pour un nombre de millisecondes.
            /// @param ms Durée en millisecondes (int64)
            static void Sleep(int64 ms) noexcept {
                NkChrono::Sleep(ms);
            }

            /// Endort le thread courant pour un nombre de millisecondes (flottant).
            /// @param ms Durée en millisecondes (float64, permet les sous-ms)
            static void Sleep(float64 ms) noexcept {
                NkChrono::Sleep(ms);
            }

            /// Endort le thread courant pour un nombre de millisecondes (nom explicite).
            /// @param ms Durée en millisecondes
            static void SleepMilliseconds(int64 ms) noexcept {
                NkChrono::SleepMilliseconds(ms);
            }

            /// Endort le thread courant pour un nombre de microsecondes.
            /// @param us Durée en microsecondes
            static void SleepMicroseconds(int64 us) noexcept {
                NkChrono::SleepMicroseconds(us);
            }

            /// Endort le thread courant pour un nombre de nanosecondes.
            /// @param ns Durée en nanosecondes
            static void SleepNanoseconds(int64 ns) noexcept {
                NkChrono::SleepNanoseconds(ns);
            }

            /// Cède le temps CPU au scheduler (yield).
            /// @note Utile pour les boucles d'attente actives sans bloquer le système
            static void YieldThread() noexcept {
                NkChrono::YieldThread();
            }

        private:

            // -------------------------------------------------------------
            // SOUS-SECTION 3.7 : Membres privés
            // -------------------------------------------------------------
            // État interne du clock : chronos, snapshot courant, flags.
            // L'ordre est optimisé pour l'alignement mémoire et le cache.

            NkChrono mFrameChrono;    ///< Chrono pour la durée de frame (reset à chaque Tick)
            NkChrono mTotalChrono;    ///< Chrono pour le temps total (jamais reset sauf Reset())
            NkTime mTime;             ///< Snapshot courant exposé via GetTime()/Tick()
            bool mPaused = false;     ///< Flag de pause : true = delta/fps = 0

        }; // class NkClock

    } // namespace nkentseu

#endif // NKENTSEU_TIME_NKCLOCK_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKCLOCK.H
// =============================================================================
// NkClock est l'orchestrateur temporel principal pour les boucles de jeu.
// Il fournit des snapshots cohérents de delta time, FPS, et temps total.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Boucle de jeu basique avec NkClock
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkClock.h"
    #include "NKCore/Logger/NkLogger.h"

    void GameLoopExample() {
        using namespace nkentseu;

        // Création du clock avec valeurs par défaut (60 FPS, time scale 1.0)
        NkClock clock;

        bool running = true;
        while (running) {
            // Avance d'une frame et récupère le snapshot temporel
            const NkClock::NkTime& t = clock.Tick();

            // Logging périodique des performances (toutes les 60 frames)
            if (t.frameCount % 60 == 0) {
                NK_LOG_INFO("Frame {}: {} ms, FPS: {:.2f}",
                            t.frameCount,
                            t.delta * 1000.f,  // Conversion en ms
                            t.fps);
            }

            // Mise à jour de la logique métier avec time scale appliqué
            // UpdateGameLogic(t.Scaled());

            // Rendu avec delta brut (indépendant du time scale)
            // RenderFrame(t.delta);

            // Condition de sortie (exemple)
            // if (ShouldExit()) running = false;
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Physique déterministe avec fixed delta
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkClock.h"

    void DeterministicPhysicsExample() {
        using namespace nkentseu;

        NkClock clock;
        // Configuration pour une physique à 50 Hz (20ms par pas)
        clock.SetFixedDelta(1.f / 50.f);

        float accumulator = 0.f;

        while (true) {
            const NkClock::NkTime& t = clock.Tick();

            // Accumulation du temps écoulé
            accumulator += t.delta;

            // Exécution de la physique par pas fixes
            while (accumulator >= t.fixedDelta) {
                // PhysicsStep utilise FixedScaled() pour respecter le time scale
                // PhysicsSystem::Step(t.FixedScaled());
                accumulator -= t.fixedDelta;
            }

            // Interpolation visuelle avec le temps restant
            // float alpha = accumulator / t.fixedDelta;
            // RenderFrame(alpha);

            // Limitation de FPS pour éviter la surchauffe CPU
            if (t.fps > 120.f) {
                NkClock::SleepMilliseconds(1);  // Yield léger
            }
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Pause/Resume et time scale dynamique
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkClock.h"
    #include "NKCore/Input/NkInput.h"  // Hypothétique

    void PauseAndTimeScaleExample() {
        using namespace nkentseu;

        NkClock clock;
        bool isPaused = false;

        while (true) {
            // Gestion des entrées pour pause et slow-mo
            if (Input::KeyPressed(Input::Key::P)) {
                isPaused = !isPaused;
                if (isPaused) {
                    clock.Pause();
                } else {
                    clock.Resume();
                }
            }

            // Slow-motion avec la touche M
            if (Input::KeyHeld(Input::Key::M)) {
                clock.SetTimeScale(0.25f);  // 4x plus lent
            } else if (!isPaused) {
                clock.SetTimeScale(1.0f);   // Retour à la normale
            }

            const NkClock::NkTime& t = clock.Tick();

            // En pause : t.delta == 0, mais t.total continue d'avancer
            if (isPaused) {
                // Afficher un overlay de pause
                // RenderPauseOverlay();
            } else {
                // Mise à jour normale avec time scale appliqué
                // UpdateGame(t.Scaled());
            }

            // RenderFrame(t.delta);  // Rendu toujours avec delta brut
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Reset partiel sans perdre la configuration
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkClock.h"

    void LevelTransitionExample() {
        using namespace nkentseu;

        NkClock clock;
        // Configuration persistante entre les niveaux
        clock.SetFixedDelta(1.f / 60.f);
        clock.SetTimeScale(1.0f);

        // Niveau 1
        // LoadLevel(1);
        while (!LevelComplete()) {
            const NkClock::NkTime& t = clock.Tick();
            // UpdateLevel(t);
        }

        // Transition : reset du clock pour le nouveau niveau
        // Préserve fixedDelta et timeScale configurés précédemment
        clock.Reset();

        // Niveau 2 : le temps repart de zéro, mais la config est conservée
        // LoadLevel(2);
        while (!LevelComplete()) {
            const NkClock::NkTime& t = clock.Tick();
            // t.total recommence à 0, t.frameCount aussi
            // UpdateLevel(t);
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Mesure de performance avec NkClock
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkClock.h"
    #include "NKCore/Logger/NkLogger.h"
    #include <vector>

    void PerformanceProfilingExample() {
        using namespace nkentseu;

        NkClock clock;
        std::vector<float> frameTimes;
        const uint64 sampleCount = 300;  // ~5 secondes à 60 FPS

        while (frameTimes.size() < sampleCount) {
            const NkClock::NkTime& t = clock.Tick();

            // Collecte des delta times pour analyse
            frameTimes.push_back(t.delta);

            // Simulation de travail
            // DoWork();

            // Yield pour éviter de saturer le CPU en mode benchmark
            if (t.fps > 1000.f) {
                NkClock::YieldThread();
            }
        }

        // Calcul des statistiques
        float sum = 0.f;
        float min = frameTimes[0];
        float max = frameTimes[0];
        for (float dt : frameTimes) {
            sum += dt;
            if (dt < min) min = dt;
            if (dt > max) max = dt;
        }
        float avg = sum / static_cast<float>(frameTimes.size());

        NK_LOG_INFO("Performance over {} frames:", sampleCount);
        NK_LOG_INFO("  Avg: {:.3f} ms ({:.1f} FPS)", avg * 1000.f, 1.f / avg);
        NK_LOG_INFO("  Min: {:.3f} ms, Max: {:.3f} ms", min * 1000.f, max * 1000.f);
    }
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Utilisation des méthodes Sleep déléguées
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkClock.h"
    #include "NKCore/Network/NkSocket.h"  // Hypothétique

    void NetworkWaitExample() {
        using namespace nkentseu;

        // Attente avec timeout sur une opération réseau
        bool WaitForData(NkSocket& socket, float timeoutSeconds) {
            const NkDuration timeout = NkDuration::FromSeconds(timeoutSeconds);
            NkDuration elapsed = NkDuration::Zero();

            while (elapsed < timeout) {
                if (socket.HasData()) {
                    return true;
                }

                // Sleep court pour éviter le busy-wait
                // Utilisation de NkClock::Sleep pour commodité (pas besoin d'inclure NkChrono)
                NkClock::SleepMilliseconds(5);  // 5ms

                // Mise à jour du temps écoulé
                elapsed += NkDuration::FromMilliseconds(5);
            }
            return false;  // Timeout expiré
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Intégration avec un moteur de jeu complet
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkClock.h"
    #include "NKCore/Logger/NkLogger.h"

    namespace game {

        class Engine {
        public:
            void Run() {
                Initialize();

                nkentseu::NkClock clock;
                clock.SetFixedDelta(1.f / 60.f);  // Physique à 60 Hz

                while (mRunning) {
                    // 1. Avance temporelle
                    const nkentseu::NkClock::NkTime& t = clock.Tick();

                    // 2. Gestion des entrées (indépendante du time scale)
                    Input::PollEvents();

                    // 3. Mise à jour logique avec time scale
                    if (!clock.IsPaused()) {
                        mWorld.Update(t.Scaled());
                        mPhysics.Step(t.FixedScaled());
                    }

                    // 4. Rendu avec delta brut (pour interpolation fluide)
                    mRenderer.BeginFrame();
                    mWorld.Render(t.delta);
                    mRenderer.EndFrame();

                    // 5. Synchronisation optionnelle (VSync ou cap FPS)
                    if (mTargetFPS > 0 && t.fps > static_cast<float>(mTargetFPS)) {
                        float sleepMs = (1000.f / mTargetFPS) - (t.delta * 1000.f);
                        if (sleepMs > 0.f) {
                            nkentseu::NkClock::Sleep(sleepMs);
                        }
                    }
                }

                Shutdown();
            }

        private:
            bool mRunning = true;
            int mTargetFPS = 0;  // 0 = illimité
            // ... autres membres ...
        };

    } // namespace game
*/

// -----------------------------------------------------------------------------
// Exemple 8 : Tests unitaires de NkClock
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkClock.h"
    #include "NKCore/Assert/NkAssert.h"

    void NkClockUnitTest() {
        using namespace nkentseu;

        // Test 1 : Construction et valeurs par défaut
        {
            NkClock clock;
            const auto& t = clock.GetTime();
            NKENTSEU_ASSERT(t.delta == 0.f);
            NKENTSEU_ASSERT(t.total == 0.f);
            NKENTSEU_ASSERT(t.frameCount == 0);
            NKENTSEU_ASSERT(t.timeScale == 1.f);
            NKENTSEU_ASSERT(t.fixedDelta == 1.f / 60.f);
        }

        // Test 2 : Tick incrémente frameCount
        {
            NkClock clock;
            for (uint64 i = 1; i <= 10; ++i) {
                const auto& t = clock.Tick();
                NKENTSEU_ASSERT(t.frameCount == i);
            }
        }

        // Test 3 : Pause met delta à 0 mais continue total
        {
            NkClock clock;
            clock.Tick();  // Frame 1
            const float totalBefore = clock.GetTime().total;

            clock.Pause();
            NkClock::SleepMilliseconds(10);  // Attendre 10ms réelles
            const auto& tPaused = clock.Tick();

            NKENTSEU_ASSERT(tPaused.delta == 0.f);  // Delta nul en pause
            NKENTSEU_ASSERT(tPaused.total >= totalBefore);  // Total continue
            NKENTSEU_ASSERT(clock.IsPaused() == true);
        }

        // Test 4 : Time scale affecte Scaled() mais pas delta brut
        {
            NkClock clock;
            clock.SetTimeScale(0.5f);

            // Simuler un tick avec delta ~16ms (60 FPS)
            // Note : en test unitaire, on ne peut pas forcer le delta réel,
            // donc on vérifie la relation mathématique
            const auto& t = clock.GetTime();
            NKENTSEU_ASSERT(t.Scaled() == t.delta * 0.5f);
            NKENTSEU_ASSERT(t.FixedScaled() == t.fixedDelta * 0.5f);
        }

        // Test 5 : Reset préserve la configuration
        {
            NkClock clock;
            clock.SetTimeScale(2.f);
            clock.SetFixedDelta(1.f / 30.f);
            clock.Tick();  // Avancer d'une frame

            clock.Reset();
            const auto& t = clock.GetTime();

            NKENTSEU_ASSERT(t.frameCount == 0);  // Reseté
            NKENTSEU_ASSERT(t.total == 0.f);      // Reseté
            NKENTSEU_ASSERT(t.timeScale == 2.f);  // Conservé
            NKENTSEU_ASSERT(t.fixedDelta == 1.f / 30.f);  // Conservé
        }
    }
*/

// ============================================================
// Copyright © 2024-2026 TEUGUIA TADJUIDJE Rodolf Séderis.
// Apache-2.0 License - Free to use and modify
// ============================================================