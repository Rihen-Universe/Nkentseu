// =============================================================================
// NKEvent/NkApplicationEvent.h
// Hiérarchie des événements liés au cycle de vie de l'application.
//
// Catégorie : NK_CAT_APPLICATION
//
// Architecture :
//   - NkAppEvent           : classe de base abstraite pour tous les événements application
//   - NkAppLaunchEvent     : émis au démarrage, transporte les arguments CLI
//   - NkAppTickEvent       : tick logique pour la boucle principale (deltaTime)
//   - NkAppUpdateEvent     : mise à jour de la logique métier (pas fixe ou variable)
//   - NkAppRenderEvent     : demande de rendu graphique avec interpolation
//   - NkAppCloseEvent      : demande de fermeture (annulable sauf si forced)
//
// Design :
//   - Héritage polymorphe depuis NkEvent pour intégration au dispatcher central
//   - Macros NK_EVENT_* pour éviter la duplication de code boilerplate
//   - Méthodes Clone() pour copie profonde (replay, tests, sérialisation)
//   - ToString() surchargé pour débogage et logging significatif
//   - Accesseurs inline noexcept pour performance critique (lecture fréquente)
//
// Usage typique :
//   if (auto* closeEvt = event.As<NkAppCloseEvent>()) {
//       if (!closeEvt->IsForced() && HasUnsavedChanges()) {
//           closeEvt->Cancel();
//           ShowSaveDialog();
//       }
//   }
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_EVENT_NKAPPLICATIONEVENT_H
#define NKENTSEU_EVENT_NKAPPLICATIONEVENT_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion des dépendances internes au projet.
    // Toutes les dépendances utilisent les modules NK* du framework.

    #include "NKEvent/NkEvent.h"                        // Classe de base NkEvent + macros
    #include "NKEvent/NkEventApi.h"                     // Macros d'export NKENTSEU_EVENT_API
    #include "NKContainers/String/NkStringUtils.h"      // Conversion types → NkString

    namespace nkentseu {

        // =====================================================================
        // NkAppEvent — Classe de base abstraite pour événements application
        // =====================================================================

        /**
         * @brief Classe de base polymorphe pour tous les événements liés à l'application
         *
         * Catégorie associée : NK_CAT_APPLICATION
         *
         * Cette classe sert de point d'ancrage commun pour tous les événements
         * du cycle de vie applicatif : lancement, tick, update, render, fermeture.
         *
         * @note Les classes dérivées DOIVENT utiliser les macros NK_EVENT_TYPE_FLAGS
         *       et NK_EVENT_CATEGORY_FLAGS pour éviter la duplication de code.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkAppEvent : public NkEvent {
            public:
                /// @brief Implémente le filtrage par catégorie APPLICATION pour tous les dérivés
                NK_EVENT_CATEGORY_FLAGS(NkEventCategory::NK_CAT_APPLICATION)

            protected:
                /// @brief Constructeur protégé — réservé aux classes dérivées
                /// @param windowId Identifiant de la fenêtre source (0 = événement global)
                explicit NkAppEvent(uint64 windowId = 0) noexcept
                    : NkEvent(windowId) {
                }
        };

        // =====================================================================
        // NkAppLaunchEvent — Événement de lancement de l'application
        // =====================================================================

        /**
         * @brief Émis une seule fois au démarrage de l'application, avant la boucle principale
         *
         * Cet événement transporte les arguments de la ligne de commande (argc/argv)
         * permettant à l'application de configurer son comportement initial :
         *   - Mode debug/production
         *   - Chemin de configuration personnalisé
         *   - Options de rendu ou de logging
         *   - Fichiers à charger au démarrage
         *
         * @warning Le tableau argv doit rester valide pendant toute la durée du traitement
         *          de cet événement. Ne pas modifier ni libérer les chaînes pointées.
         *
         * @note Cet événement n'est émis qu'une seule fois par session d'exécution.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkAppLaunchEvent final : public NkAppEvent {
            public:
                /// @brief Associe le type d'événement NK_APP_LAUNCH à cette classe
                NK_EVENT_TYPE_FLAGS(NK_APP_LAUNCH)

                /**
                 * @brief Constructeur avec les arguments de ligne de commande
                 * @param argc Nombre d'arguments CLI (exclu le nom du programme)
                 * @param argv Tableau de chaînes C (durée de vie >= traitement de l'événement)
                 */
                NkAppLaunchEvent(
                    int argc = 0,
                    const char* const* argv = nullptr
                ) noexcept
                    : NkAppEvent()
                    , mArgc(argc)
                    , mArgv(argv) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NkEvent* Clone() const override {
                    return new NkAppLaunchEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant le nombre d'arguments CLI
                NkString ToString() const override {
                    return "AppLaunch(argc=" + string::NkToString(mArgc) + ")";
                }

                /// @brief Retourne le nombre d'arguments de ligne de commande
                /// @return int représentant argc (0 si aucun argument)
                NKENTSEU_EVENT_API_INLINE int GetArgc() const noexcept {
                    return mArgc;
                }

                /// @brief Retourne le tableau des arguments de ligne de commande
                /// @return Pointeur vers tableau de const char* (ne pas libérer)
                NKENTSEU_EVENT_API_INLINE const char* const* GetArgv() const noexcept {
                    return mArgv;
                }

                /// @brief Retourne l'argument à l'index spécifié, ou nullptr si hors limites
                /// @param i Index de l'argument à récupérer (0-based, exclu argv[0])
                /// @return Pointeur vers la chaîne C demandée, ou nullptr si invalide
                /// @note Vérification de bounds intégrée pour sécurité
                NKENTSEU_EVENT_API_INLINE const char* GetArg(int i) const noexcept {
                    if (mArgv && i >= 0 && i < mArgc) {
                        return mArgv[i];
                    }
                    return nullptr;
                }

            private:
                int mArgc;                      ///< Nombre d'arguments CLI
                const char* const* mArgv;       ///< Tableau des arguments (non-owned)
        };

        // =====================================================================
        // NkAppTickEvent — Tick logique de la boucle principale
        // =====================================================================

        /**
         * @brief Émis à chaque itération de la boucle principale pour alimenter la logique temporelle
         *
         * Cet événement est le cœur du timing applicatif :
         *   - Physique : intégration des forces et collisions
         *   - Animations : interpolation et blending
         *   - IA : prise de décision et pathfinding
         *   - Réseau : traitement des paquets entrants
         *
         * @note mDeltaTime représente le temps réel écoulé depuis le tick précédent
         *       À 60 FPS : mDeltaTime ≈ 0.01667 secondes (16.67 ms)
         *       À 120 FPS : mDeltaTime ≈ 0.00833 secondes (8.33 ms)
         *
         * @note mTotalTime permet de calculer des durées cumulées pour les timers
         *       et les événements programmés dans le temps.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkAppTickEvent final : public NkAppEvent {
            public:
                /// @brief Associe le type d'événement NK_APP_TICK à cette classe
                NK_EVENT_TYPE_FLAGS(NK_APP_TICK)

                /**
                 * @brief Constructeur avec les informations de timing
                 * @param deltaTime Temps écoulé depuis le dernier tick [secondes]
                 * @param totalTime Temps cumulé depuis le lancement de l'application [secondes]
                 */
                NkAppTickEvent(
                    float64 deltaTime,
                    float64 totalTime = 0.0
                ) noexcept
                    : NkAppEvent()
                    , mDeltaTime(deltaTime)
                    , mTotalTime(totalTime) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NkEvent* Clone() const override {
                    return new NkAppTickEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant deltaTime et totalTime en secondes
                NkString ToString() const override {
                    return "AppTick(dt=" + string::NkToString(mDeltaTime)
                         + "s total=" + string::NkToString(mTotalTime) + "s)";
                }

                /// @brief Retourne le delta temps depuis le tick précédent
                /// @return float64 représentant la durée en secondes
                NKENTSEU_EVENT_API_INLINE float64 GetDeltaTime() const noexcept {
                    return mDeltaTime;
                }

                /// @brief Retourne le temps total écoulé depuis le lancement
                /// @return float64 représentant la durée cumulée en secondes
                NKENTSEU_EVENT_API_INLINE float64 GetTotalTime() const noexcept {
                    return mTotalTime;
                }

                /// @brief Calcule le FPS instantané à partir du delta temps
                /// @return float64 représentant les frames par seconde (0 si deltaTime == 0)
                /// @note Formule : FPS = 1.0 / deltaTime
                NKENTSEU_EVENT_API_INLINE float64 GetFps() const noexcept {
                    if (mDeltaTime > 0.0) {
                        return 1.0 / mDeltaTime;
                    }
                    return 0.0;
                }

            private:
                float64 mDeltaTime;    ///< Temps écoulé depuis le dernier tick [secondes]
                float64 mTotalTime;    ///< Temps cumulé depuis le lancement [secondes]
        };

        // =====================================================================
        // NkAppUpdateEvent — Mise à jour de la logique applicative
        // =====================================================================

        /**
         * @brief Émis pour déclencher les mises à jour de la logique métier
         *
         * Cet événement orchestre les mises à jour des sous-systèmes :
         *   - État du jeu : règles métier, scores, progression
         *   - Interface utilisateur : réactivité, animations UI
         *   - Intelligence artificielle : décisions, comportements
         *   - Réseau : synchronisation, prédiction, réconciliation
         *
         * Modes de mise à jour :
         *   - Variable (mFixedStep = false) : deltaTime réel, réactif mais non-déterministe
         *   - Fixe (mFixedStep = true) : pas constant, déterministe pour simulation/replay
         *
         * @note En mode pas fixe, l'application peut accumuler le temps réel et exécuter
         *       plusieurs updates de pas fixe pour rattraper le retard (fixed timestep loop).
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkAppUpdateEvent final : public NkAppEvent {
            public:
                /// @brief Associe le type d'événement NK_APP_UPDATE à cette classe
                NK_EVENT_TYPE_FLAGS(NK_APP_UPDATE)

                /**
                 * @brief Constructeur avec les paramètres de mise à jour
                 * @param deltaTime Durée du pas de simulation [secondes]
                 * @param fixedStep true si la mise à jour utilise un pas de temps fixe
                 */
                NkAppUpdateEvent(
                    float64 deltaTime,
                    bool fixedStep = false
                ) noexcept
                    : NkAppEvent()
                    , mDeltaTime(deltaTime)
                    , mFixedStep(fixedStep) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NkEvent* Clone() const override {
                    return new NkAppUpdateEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant deltaTime et le mode (fixed/variable)
                NkString ToString() const override {
                    return NkString("AppUpdate(dt=") + string::NkToString(mDeltaTime)
                         + (mFixedStep ? " fixed" : " variable") + ")";
                }

                /// @brief Retourne la durée du pas de simulation
                /// @return float64 représentant le deltaTime en secondes
                NKENTSEU_EVENT_API_INLINE float64 GetDeltaTime() const noexcept {
                    return mDeltaTime;
                }

                /// @brief Indique si la mise à jour utilise un pas de temps fixe
                /// @return true pour mode déterministe, false pour mode temps réel
                NKENTSEU_EVENT_API_INLINE bool IsFixedStep() const noexcept {
                    return mFixedStep;
                }

            private:
                float64 mDeltaTime;    ///< Durée du pas de simulation [secondes]
                bool mFixedStep;       ///< true = pas fixe (déterministe), false = variable
        };

        // =====================================================================
        // NkAppRenderEvent — Demande de rendu graphique d'une frame
        // =====================================================================

        /**
         * @brief Émis pour déclencher le rendu graphique d'une frame
         *
         * Cet événement synchronise le pipeline de rendu avec la logique :
         *   - Interpolation : lissage entre deux états simulés pour fluidité visuelle
         *   - Culling : élimination des objets hors champ de vue
         *   - Submission : envoi des commandes GPU au driver graphique
         *
         * @note mAlpha est un facteur d'interpolation [0.0, 1.0] :
         *       - 0.0 : rendu de l'état précédent (avant le dernier update)
         *       - 1.0 : rendu de l'état courant (après le dernier update)
         *       - 0.5 : interpolation linéaire entre les deux états
         *
         * @note Utile en mode pas de temps fixe pour découpler fréquence de simulation
         *       et fréquence de rendu (ex: 60 Hz logic / 144 Hz render).
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkAppRenderEvent final : public NkAppEvent {
            public:
                /// @brief Associe le type d'événement NK_APP_RENDER à cette classe
                NK_EVENT_TYPE_FLAGS(NK_APP_RENDER)

                /**
                 * @brief Constructeur avec les paramètres de rendu
                 * @param alpha Facteur d'interpolation [0.0, 1.0] entre états simulés
                 * @param frameIndex Index incrémental de la frame depuis le lancement
                 */
                NkAppRenderEvent(
                    float64 alpha = 1.0,
                    uint64 frameIndex = 0
                ) noexcept
                    : NkAppEvent()
                    , mAlpha(alpha)
                    , mFrameIndex(frameIndex) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NkEvent* Clone() const override {
                    return new NkAppRenderEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant frameIndex et facteur d'interpolation
                NkString ToString() const override {
                    return "AppRender(frame=" + string::NkToString(mFrameIndex)
                         + " alpha=" + string::NkToString(mAlpha) + ")";
                }

                /// @brief Retourne le facteur d'interpolation entre états simulés
                /// @return float64 dans [0.0, 1.0] pour blending temporel
                NKENTSEU_EVENT_API_INLINE float64 GetAlpha() const noexcept {
                    return mAlpha;
                }

                /// @brief Retourne l'index incrémental de la frame courante
                /// @return uint64 représentant le numéro de frame depuis le lancement
                /// @note Utile pour le profiling, le replay et la synchronisation réseau
                NKENTSEU_EVENT_API_INLINE uint64 GetFrameIndex() const noexcept {
                    return mFrameIndex;
                }

            private:
                float64 mAlpha;        ///< Facteur d'interpolation [0.0, 1.0]
                uint64 mFrameIndex;    ///< Index incrémental de la frame courante
        };

        // =====================================================================
        // NkAppCloseEvent — Demande de fermeture de l'application
        // =====================================================================

        /**
         * @brief Émis lorsque l'application reçoit une demande de fermeture
         *
         * Sources typiques de cet événement :
         *   - Clic utilisateur sur le bouton de fermeture de la fenêtre
         *   - Signal système (SIGTERM sur Unix, WM_CLOSE sur Windows)
         *   - Appel programmatique via l'API de l'application
         *   - Déconnexion forcée (session expirée, kick administrateur...)
         *
         * Mécanisme d'annulation :
         *   - Un handler peut appeler Cancel() pour bloquer la fermeture
         *   - Utile pour afficher "Enregistrer les modifications ?" ou confirmer la sortie
         *   - Si IsForced() retourne true, l'annulation est ignorée (fermeture imposée)
         *
         * @code
         *   void OnAppClose(NkAppCloseEvent& event) {
         *       if (event.IsForced()) {
         *           // Fermeture imposée : sauvegarde rapide et sortie
         *           GameState::QuickSave();
         *           return;
         *       }
         *       if (HasUnsavedChanges()) {
         *           event.Cancel();  // Bloque la fermeture
         *           UIManager::ShowSaveConfirmationDialog();
         *       }
         *   }
         * @endcode
         *
         * @note Les handlers sont appelés dans l'ordre d'enregistrement.
         *       Le premier à appeler Cancel() (si allowed) stoppe la fermeture.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkAppCloseEvent final : public NkAppEvent {
            public:
                /// @brief Associe le type d'événement NK_APP_CLOSE à cette classe
                NK_EVENT_TYPE_FLAGS(NK_APP_CLOSE)

                /**
                 * @brief Constructeur avec indicateur de fermeture forcée
                 * @param forced true si la fermeture est imposée par le système (non annulable)
                 */
                NkAppCloseEvent(bool forced = false) noexcept
                    : NkAppEvent()
                    , mForced(forced)
                    , mCancelled(false) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NkEvent* Clone() const override {
                    return new NkAppCloseEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant l'état de la demande de fermeture
                NkString ToString() const override {
                    return NkString("AppClose(")
                         + (mForced ? "forced" : "requested")
                         + (mCancelled ? " cancelled" : "") + ")";
                }

                /// @brief Indique si la fermeture est imposée par le système
                /// @return true si IsForced() — l'annulation sera ignorée
                NKENTSEU_EVENT_API_INLINE bool IsForced() const noexcept {
                    return mForced;
                }

                /// @brief Indique si la fermeture a été annulée par un handler
                /// @return true si Cancel() a été appelé (et autorisé)
                NKENTSEU_EVENT_API_INLINE bool IsCancelled() const noexcept {
                    return mCancelled;
                }

                /// @brief Annule la demande de fermeture (sans effet si IsForced())
                /// @note À appeler dans un handler pour bloquer la fermeture gracieuse
                /// @note Ignoré silencieusement si la fermeture est imposée par le système
                NKENTSEU_EVENT_API_INLINE void Cancel() noexcept {
                    if (!mForced) {
                        mCancelled = true;
                    }
                }

            private:
                bool mForced;        ///< true = fermeture imposée (non annulable)
                bool mCancelled;     ///< true = fermeture annulée par un handler
        };

    } // namespace nkentseu

#endif // NKENTSEU_EVENT_NKAPPLICATIONEVENT_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKAPPLICATIONEVENT.H
// =============================================================================
// Ce fichier définit la hiérarchie des événements application pour le module NKEvent.
// Les exemples ci-dessous illustrent les patterns d'usage recommandés.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Gestion des arguments de ligne de commande au lancement
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkApplicationEvent.h"

    class ApplicationConfig {
    public:
        void OnLaunch(const nkentseu::NkAppLaunchEvent& event) {
            using namespace nkentseu;

            // Parsing des arguments CLI
            for (int i = 0; i < event.GetArgc(); ++i) {
                const char* arg = event.GetArg(i);
                if (!arg) continue;

                if (NkStringUtils::StartsWith(arg, "--config=")) {
                    mConfigPath = arg + 9;  // Skip "--config="
                }
                else if (NkStringUtils::Equals(arg, "--debug")) {
                    Logger::SetLevel(LogLevel::Debug);
                }
                else if (NkStringUtils::Equals(arg, "--fullscreen")) {
                    mLaunchFullscreen = true;
                }
            }

            // Chargement de la configuration
            if (!mConfigPath.Empty()) {
                LoadConfigFromFile(mConfigPath);
            }
        }

    private:
        NkString mConfigPath;
        bool mLaunchFullscreen = false;
    };
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Boucle principale avec gestion des ticks et updates
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkApplicationEvent.h"

    class GameLoop {
    public:
        void Run() {
            using namespace nkentseu;

            float64 accumulator = 0.0;
            const float64 fixedStep = 1.0 / 60.0;  // 60 Hz logic update

            while (!mShouldExit) {
                // 1. Tick : mesure du temps réel écoulé
                float64 frameTime = Platform::GetDeltaTime();
                NkAppTickEvent tickEvent(frameTime, mTotalTime);
                mEventManager.Dispatch(tickEvent);

                // 2. Accumulation pour fixed timestep
                accumulator += frameTime;

                // 3. Updates à pas fixe (peut exécuter plusieurs fois par frame)
                while (accumulator >= fixedStep) {
                    NkAppUpdateEvent updateEvent(fixedStep, true);  // fixedStep = true
                    mEventManager.Dispatch(updateEvent);
                    accumulator -= fixedStep;
                    mLastUpdateTime = mTotalTime;
                }

                // 4. Render avec interpolation
                float64 alpha = accumulator / fixedStep;  // [0.0, 1.0]
                NkAppRenderEvent renderEvent(alpha, mFrameCount++);
                mEventManager.Dispatch(renderEvent);

                // 5. Swap buffers et synchronisation VSync
                Renderer::Present();
            }
        }

    private:
        float64 mTotalTime = 0.0;
        float64 mLastUpdateTime = 0.0;
        uint64 mFrameCount = 0;
        bool mShouldExit = false;
        event::EventManager& mEventManager;
    };
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Gestion élégante de la fermeture avec sauvegarde
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkApplicationEvent.h"

    class SaveManager {
    public:
        void RegisterCloseHandler(nkentseu::event::EventManager& mgr) {
            using namespace nkentseu;

            mgr.Subscribe<NkAppCloseEvent>(
                [this](NkAppCloseEvent& event) {
                    if (event.IsForced()) {
                        // Fermeture imposée : sauvegarde d'urgence minimale
                        NK_FOUNDATION_LOG_WARNING("Forced shutdown: quick save only");
                        GameState::EmergencySave();
                        return;  // Ne pas annuler une fermeture forcée
                    }

                    if (GameState::HasUnsavedChanges()) {
                        // Fermeture gracieuse avec modifications non sauvegardées
                        event.Cancel();  // Bloquer la fermeture
                        ShowSaveConfirmationDialog([this](SaveChoice choice) {
                            switch (choice) {
                                case SaveChoice::Save:
                                    GameState::Save();
                                    Application::RequestExit();  // Re-trigger close
                                    break;
                                case SaveChoice::Discard:
                                    Application::RequestExit();  // Re-trigger close
                                    break;
                                case SaveChoice::Cancel:
                                    // Rien : l'utilisateur reste dans l'application
                                    break;
                            }
                        });
                    }
                    // Si pas de modifications : laisser la fermeture se poursuivre
                }
            );
        }

    private:
        enum class SaveChoice { Save, Discard, Cancel };
        void ShowSaveConfirmationDialog(std::function<void(SaveChoice)> callback);
    };
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Interpolation fluide entre états de simulation
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkApplicationEvent.h"

    class RenderSystem {
    public:
        void OnRender(const nkentseu::NkAppRenderEvent& event) {
            using namespace nkentseu;

            // Récupération des deux états de simulation pour interpolation
            const GameState& prevState = mSimulation.GetState(mLastUpdateIndex);
            const GameState& currState = mSimulation.GetState(mCurrentUpdateIndex);

            // Interpolation linéaire de la position du joueur
            float32 alpha = static_cast<float32>(event.GetAlpha());
            Vec3 interpolatedPos = Lerp(
                prevState.playerPosition,
                currState.playerPosition,
                alpha
            );

            // Rendu avec la position interpolée pour fluidité visuelle
            mPlayerSprite.SetPosition(interpolatedPos);
            mScene.Render(mCamera, alpha);
        }

    private:
        template<typename T>
        T Lerp(const T& a, const T& b, float32 t) const {
            return a + (b - a) * t;
        }

        uint64 mLastUpdateIndex = 0;
        uint64 mCurrentUpdateIndex = 0;
        Simulation& mSimulation;
        PlayerSprite& mPlayerSprite;
        Scene& mScene;
        Camera& mCamera;
    };
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Profiling et statistiques de la boucle principale
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkApplicationEvent.h"
    #include "NKTime/NkChrono.h"

    class FrameProfiler {
    public:
        void OnTick(const nkentseu::NkAppTickEvent& event) {
            using namespace nkentseu;

            // Enregistrement des métriques de timing
            mFrameTimes.PushBack(event.GetDeltaTime());
            mTotalFrames++;

            // Calcul des statistiques glissantes (sur 60 frames)
            if (mFrameTimes.Size() >= 60) {
                float64 avgDt = 0.0;
                float64 minDt = NK_FLOAT64_MAX;
                float64 maxDt = 0.0;

                for (float64 dt : mFrameTimes) {
                    avgDt += dt;
                    minDt = NkMin(minDt, dt);
                    maxDt = NkMax(maxDt, dt);
                }
                avgDt /= static_cast<float64>(mFrameTimes.Size());

                // Logging périodique (toutes les 60 frames ≈ 1 seconde à 60 FPS)
                if (mTotalFrames % 60 == 0) {
                    NK_FOUNDATION_LOG_INFO(
                        "FPS: {:.1f} (avg dt={:.3f}s, min={:.3f}s, max={:.3f}s)",
                        1.0 / avgDt, avgDt, minDt, maxDt
                    );
                }

                // Rotation du buffer circulaire
                mFrameTimes.Erase(0);
            }
        }

    private:
        NkVector<float64> mFrameTimes;  // Buffer circulaire de deltaTimes
        uint64 mTotalFrames = 0;
    };
*/

// =============================================================================
// NOTES DE MAINTENANCE ET BONNES PRATIQUES
// =============================================================================
/*
    1. ORDRE D'ÉMISSION DES ÉVÉNEMENTS APPLICATION :
       - NkAppLaunchEvent : une seule fois, avant la boucle principale
       - NkAppTickEvent   : à chaque frame, en premier (mesure du temps)
       - NkAppUpdateEvent : après les ticks, pour la logique métier
       - NkAppRenderEvent : en dernier, pour le rendu graphique
       - NkAppCloseEvent  : à la demande, peut être émis à tout moment

    2. GESTION DU TEMPS ET PRÉCISION :
       - Utiliser float64 pour deltaTime afin d'éviter l'accumulation d'erreurs
       - En mode fixed timestep, l'interpolation (alpha) compense le décalage render/logic
       - Ne jamais modifier mDeltaTime dans les handlers : il est en lecture seule

    3. SÉCURITÉ DES ARGUMENTS CLI (NkAppLaunchEvent) :
       - Les pointeurs argv sont non-owned : ne pas les libérer ni les modifier
       - Copier les valeurs nécessaires dans des NkString si usage long terme
       - Valider les indices avant d'appeler GetArg(i) pour éviter les out-of-bounds

    4. ANNULATION DE FERMETURE (NkAppCloseEvent) :
       - Cancel() n'a d'effet que si IsForced() == false
       - Plusieurs handlers peuvent appeler Cancel() : le premier suffit
       - Après annulation, l'application doit gérer la ré-émission si nécessaire

    5. PERFORMANCE DES ACCESSEURS :
       - Tous les getters sont inline et noexcept pour éliminer l'overhead d'appel
       - Les calculs simples (GetFps()) sont faits à la volée sans cache
       - Éviter les allocations dans ToString() pour les événements fréquents

    6. EXTENSIBILITÉ :
       - Pour ajouter un nouvel événement application :
         a) Dériver de NkAppEvent (hérite automatiquement de NK_CAT_APPLICATION)
         b) Utiliser NK_EVENT_TYPE_FLAGS avec un nouveau type dans NkEventType
         c) Implémenter Clone() et ToString() pour cohérence avec l'infrastructure
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================