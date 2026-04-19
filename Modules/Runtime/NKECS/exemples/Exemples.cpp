// =============================================================================
// Documentation/NkScene_Examples.h
// =============================================================================
/**
 * @file NkScene_Examples.h
 * @brief Guide complet d'utilisation du système de scènes NKRenderer.
 * 
 * 🔹 OBJECTIF DE CE FICHIER :
 *    • Servir de référence documentaire pour les développeurs
 *    • Illustrer les patterns d'usage recommandés avec NkSceneGraph/Script/Manager
 *    • Fournir des exemples copiables-collables pour démarrer rapidement
 *    • Documenter les pièges courants et comment les éviter
 * 
 * 🔹 AVERTISSEMENT IMPORTANT :
 *    • Ce fichier est DOCUMENTAIRE UNIQUEMENT — ne pas l'inclure dans le build
 *    • Les exemples sont conçus pour être lus, pas compilés tels quels
 *    • Adaptez les snippets à votre architecture spécifique
 * 
 * 🔹 STRUCTURE DU GUIDE :
 *    1. Définition des scripts de scène (NkSceneScript)
 *    2. Enregistrement des scènes dans NkSceneManager
 *    3. Configuration du scheduler avec NkSceneLifecycleSystem
 *    4. Chargement et transitions entre scènes
 *    5. Accès et manipulation des scripts depuis le code métier
 *    6. Intégration dans un éditeur (EditorLayer pattern)
 *    7. Bonnes pratiques et anti-patterns à éviter
 * 
 * @author nkentseu
 * @version 1.0
 * @date 2026
 */

// =============================================================================
// PRÉREQUIS : Inclusions et namespaces
// =============================================================================
/**
 * @brief Inclusions nécessaires pour utiliser le système de scènes.
 * 
 * @note Ces includes sont donnés à titre indicatif — adaptez les chemins
 *       à votre structure de projet réelle.
 */
/*
#include "NKECS/Scene/NkSceneGraph.h"           // NkSceneGraph, composants hiérarchie
#include "NKECS/Scene/NkSceneScript.h"          // NkSceneScript (interface de base)
#include "NKECS/Scene/NkSceneManager.h"         // NkSceneManager (chargement/transitions)
#include "NKECS/Scene/NkSceneLifecycleSystem.h" // Systèmes ECS de lifecycle
#include "NKECS/World/NkWorld.h"                // NkWorld (monde ECS sous-jacent)
#include "NKECS/Scheduler/NkScheduler.h"        // NkScheduler (ordonnancement systèmes)
#include "NKCore/NkLogger.h"                    // Système de logging
#include "NKInput/NkInputSystem.h"              // Gestion des inputs (optionnel)

// Namespace pour simplifier l'écriture des exemples
using namespace nkentseu;
using namespace nkentseu::ecs;
using namespace nkentseu::logger;
*/

// =============================================================================
// SECTION 1 : DÉFINITION DES SCRIPTS DE SCÈNE
// =============================================================================
/**
 * @addtogroup SceneScript_Examples
 * @{
 */

/**
 * @example 01_main_menu_script.cpp
 * @brief Script de scène pour un menu principal interactif.
 * 
 * 🔹 Rôle de MainMenuScript :
 *    • Initialiser l'UI du menu au démarrage (OnBeginPlay)
 *    • Gérer les inputs joueur pour la navigation (OnTick)
 *    • Nettoyer les ressources à la fermeture (OnEndPlay)
 *    • Réagir aux événements de pause/reprise si le menu reste actif
 * 
 * 🔹 Architecture recommandée :
 *    • État privé dans la classe (mSelectedIndex, mAnimations, etc.)
 *    • Méthodes privées pour la logique métier (SpawnButtons, HandleInput, etc.)
 *    • Callbacks lifecycle override uniquement si nécessaire
 * 
 * @note Ce script est attaché à la scène via scene->SetScript<MainMenuScript>().
 *       Il reçoit automatiquement les callbacks via NkSceneLifecycleSystem.
 */
class MainMenuScript : public NkSceneScript {
    public:
        /**
         * @brief Point d'entrée unique — appelé une fois au chargement du menu.
         * 
         * 🔹 Usage typique :
         *    • Spawn des éléments d'UI (boutons, logo, fond)
         *    • Chargement de la musique/ambiance sonore
         *    • Initialisation des variables d'état du menu
         *    • Souscription aux événements globaux (ex: achievements unlock)
         * 
         * @warning Ne pas faire de chargements lourds ici — préférer une scène
         *          de chargement dédiée si nécessaire.
         */
        void OnBeginPlay() noexcept override {
            logger.Infof("[MainMenu] Initialisation du menu principal\n");

            // ── Spawn des éléments d'UI statiques ───────────────────────
            // Utilisation de GetScene() pour accéder à l'API "scène"
            if (auto* scene = GetScene()) {
                // Bouton "Jouer" — déclenche le chargement du niveau
                mButtonStart = scene->SpawnNode("UI_Button_Start");
                ConfigureButton(mButtonStart, "JOUER", [this]() {
                    OnStartGameClicked();
                });

                // Bouton "Options" — ouvre le sous-menu des paramètres
                mButtonOptions = scene->SpawnNode("UI_Button_Options");
                ConfigureButton(mButtonOptions, "OPTIONS", [this]() {
                    OnOptionsClicked();
                });

                // Bouton "Quitter" — ferme l'application
                mButtonQuit = scene->SpawnNode("UI_Button_Quit");
                ConfigureButton(mButtonQuit, "QUITTER", [this]() {
                    OnQuitClicked();
                });

                // Logo du jeu — élément décoratif
                auto logo = scene->SpawnNode("UI_Logo");
                SetLogoAnimation(logo, AnimationType::Pulse);
            }

            // ── Initialisation de l'état interne ────────────────────────
            mSelectedIndex = 0;  // Premier bouton sélectionné par défaut
            mNavigationTimer = 0.f;
            mAllowInput = true;

            // ── Chargement de l'ambiance sonore ─────────────────────────
            // AudioManager::PlayMusic("Music_MainMenu", /*loop=*/true);
        }

        /**
         * @brief Mise à jour principale — appelée chaque frame variable.
         * @param dt Delta-time en secondes depuis la frame précédente.
         * 
         * 🔹 Usage typique :
         *    • Gestion des inputs joueur (navigation clavier/manette)
         *    • Animations UI dépendantes du temps (fade, pulse, slide)
         *    • Mise à jour des timers et conditions de transition
         * 
         * @note Le paramètre dt permet d'écrire du code frame-independent :
         *       animationProgress += speed * dt;  // ✅ Correct
         *       animationProgress += speed;        // ❌ Dépend du framerate
         */
        void OnTick(float dt) noexcept override {
            // Guard : désactiver les inputs si nécessaire (ex: transition en cours)
            if (!mAllowInput) {
                return;
            }

            // ── Gestion de la navigation au clavier/manette ─────────────
            mNavigationTimer += dt;
            const float NAV_COOLDOWN = 0.15f;  // 150ms entre deux changements

            if (mNavigationTimer >= NAV_COOLDOWN) {
                // Flèche haut / D-pad haut : remonter dans la liste
                if (InputSystem::IsPressed(NkKey::ArrowUp) ||
                    InputSystem::IsPressed(NkKey::Gamepad_DPadUp)) {
                    mSelectedIndex = (mSelectedIndex - 1 + mButtonCount) % mButtonCount;
                    UpdateSelectionHighlight();
                    mNavigationTimer = 0.f;
                }
                // Flèche bas / D-pad bas : descendre dans la liste
                else if (InputSystem::IsPressed(NkKey::ArrowDown) ||
                         InputSystem::IsPressed(NkKey::Gamepad_DPadDown)) {
                    mSelectedIndex = (mSelectedIndex + 1) % mButtonCount;
                    UpdateSelectionHighlight();
                    mNavigationTimer = 0.f;
                }
                // Entrée / Bouton A : valider la sélection
                else if (InputSystem::IsPressed(NkKey::Enter) ||
                         InputSystem::IsPressed(NkKey::Gamepad_A)) {
                    TriggerSelectedAction();
                    mNavigationTimer = 0.f;
                }
            }

            // ── Animations UI continues (indépendantes des inputs) ─────
            UpdateLogoPulse(dt);  // Effet de respiration sur le logo
            UpdateButtonHoverEffects(dt);  // Feedback visuel au survol
        }

        /**
         * @brief Mise à jour tardive — appelée après OnTick(), avant le rendu.
         * @param dt Delta-time en secondes.
         * 
         * 🔹 Usage typique :
         *    • Mise à jour des caméras UI (après que les éléments ont bougé)
         *    • Calculs de layout dépendants des positions finales
         *    • Post-traitement des effets de screen-space
         * 
         * @note Dans un menu, LateTick est souvent utilisé pour :
         *       • Ajuster le focus de la caméra sur le bouton sélectionné
         *       • Mettre à jour les particules d'UI en fonction des positions
         */
        void OnLateTick(float dt) noexcept override {
            // Ajustement de la caméra UI pour suivre la sélection
            if (auto* scene = GetScene()) {
                auto selectedButton = GetButtonByIndex(mSelectedIndex);
                if (selectedButton.IsValid()) {
                    // Smooth follow : interpolation vers la position cible
                    NkVec3f targetPos = selectedButton.GetPosition();
                    NkVec3f currentPos = mUICamera->GetPosition();
                    NkVec3f newPos = NkLerp(currentPos, targetPos, 0.1f);
                    mUICamera->SetPosition(newPos);
                }
            }
            (void)dt;
        }

        /**
         * @brief Notification de pause — appelé quand la scène est mise en pause.
         * 
         * 🔹 Usage typique :
         *    • Stopper les animations non-essentielles pour économiser du CPU
         *    • Mettre en sourdine l'ambiance sonore du menu
         *    • Afficher un overlay "PAUSE" si le menu reste visible
         * 
         * @note La pause n'affecte PAS le rendu — le menu reste visible sauf
         *       si explicitement caché via SetVisible(false).
         */
        void OnPause() noexcept override {
            logger.Debug("[MainMenu] Menu en pause\n");
            mAllowInput = false;
            // AudioManager::SetCategoryVolume("Menu", 0.3f);  // Baisser le volume
            ShowPauseOverlay(true);
        }

        /**
         * @brief Notification de reprise — appelé quand la scène est reprise.
         * 
         * 🔹 Usage typique :
         *    • Reprendre les animations et les timers
         *    • Restaurer le volume audio
         *    • Cacher l'overlay de pause
         * 
         * @note OnResume() peut être appelé même si OnPause() ne l'a pas été
         *       (ex: reprise après chargement d'une sauvegarde).
         */
        void OnResume() noexcept override {
            logger.Debug("[MainMenu] Menu repris\n");
            mAllowInput = true;
            // AudioManager::SetCategoryVolume("Menu", 1.0f);  // Restaurer le volume
            ShowPauseOverlay(false);
        }

        /**
         * @brief Point de sortie — appelé avant le déchargement du menu.
         * 
         * 🔹 Usage typique :
         *    • Sauvegarder les préférences utilisateur (volume, contrôles)
         *    • Libérer les ressources spécifiques au menu (textures UI)
         *    • Annuler les souscriptions aux événements globaux
         * 
         * @note Garantir que cette méthode est noexcept pour éviter
         *       les terminaisons brutales lors du déchargement.
         */
        void OnEndPlay() noexcept override {
            logger.Infof("[MainMenu] Fermeture du menu principal\n");

            // ── Sauvegarde des préférences utilisateur ─────────────────
            SaveUserPreferences();

            // ── Libération des ressources spécifiques ──────────────────
            // AssetManager::UnloadCategory("UI_MainMenu");

            // ── Reset de l'état interne pour un prochain chargement ────
            mSelectedIndex = 0;
            mNavigationTimer = 0.f;
            mAllowInput = true;
        }

    private:
        // ── État privé du script ───────────────────────────────────────
        NkEntityId mButtonStart = NkEntityId::Invalid();  ///< Handle du bouton "Jouer"
        NkEntityId mButtonOptions = NkEntityId::Invalid();///< Handle du bouton "Options"
        NkEntityId mButtonQuit = NkEntityId::Invalid();   ///< Handle du bouton "Quitter"
        
        nk_int32 mSelectedIndex = 0;      ///< Index du bouton actuellement sélectionné
        nk_int32 mButtonCount = 3;        ///< Nombre total de boutons navigables
        float mNavigationTimer = 0.f;     ///< Timer pour le cooldown de navigation
        bool mAllowInput = true;          ///< Flag pour activer/désactiver les inputs

        NkCameraComponent* mUICamera = nullptr;  ///< Caméra dédiée à l'UI (optionnel)

        // ── Méthodes utilitaires privées ───────────────────────────────
        /**
         * @brief Configure un bouton UI avec texte et callback de clic.
         * @param buttonId Handle du bouton à configurer.
         * @param text Texte à afficher sur le bouton.
         * @param onClick Callback à exécuter au clic.
         */
        void ConfigureButton(NkEntityId buttonId, const char* text,
                             NkFunction<void()> onClick) {
            // Implémentation simplifiée — à adapter à votre système d'UI
            if (auto* world = GetWorld()) {
                // Ajout du composant UIText pour l'affichage
                world->Add<UIText>(buttonId, UIText{text});
                // Ajout du composant UIButton pour l'interactivité
                world->Add<UIButton>(buttonId, UIButton{onClick});
            }
        }

        /**
         * @brief Met à jour le visuel de surbrillance du bouton sélectionné.
         */
        void UpdateSelectionHighlight() {
            // Désactiver la surbrillance sur tous les boutons
            // Activer uniquement sur le bouton à mSelectedIndex
            // Jouer un son de feedback si nécessaire
        }

        /**
         * @brief Déclenche l'action associée au bouton actuellement sélectionné.
         */
        void TriggerSelectedAction() {
            switch (mSelectedIndex) {
                case 0: OnStartGameClicked(); break;
                case 1: OnOptionsClicked(); break;
                case 2: OnQuitClicked(); break;
                default: break;
            }
        }

        /**
         * @brief Callback : l'utilisateur a cliqué sur "Jouer".
         */
        void OnStartGameClicked() {
            logger.Info("[MainMenu] Démarrage d'une nouvelle partie\n");
            mAllowInput = false;  // Désactiver les inputs pendant la transition

            // Chargement du niveau avec transition en fondu
            if (auto* scene = GetScene()) {
                if (auto* mgr = scene->GetManager()) {
                    mgr->LoadScene("GameLevel",
                                   NkSceneTransition::Fade(0.5f));
                }
            }
        }

        /**
         * @brief Callback : l'utilisateur a cliqué sur "Options".
         */
        void OnOptionsClicked() {
            logger.Info("[MainMenu] Ouverture du menu des options\n");
            // Ouvrir un sous-menu ou une overlay UI
            // ShowOptionsOverlay();
        }

        /**
         * @brief Callback : l'utilisateur a cliqué sur "Quitter".
         */
        void OnQuitClicked() {
            logger.Info("[MainMenu] Demande de fermeture de l'application\n");
            // Application::Get().RequestQuit();  // Demande de fermeture propre
        }

        /**
         * @brief Met à jour l'animation de pulse du logo.
         * @param dt Delta-time pour l'animation frame-independent.
         */
        void UpdateLogoPulse(float dt) {
            // Animation de respiration : scale oscillant entre 1.0 et 1.05
            // mLogoScale = 1.0f + 0.05f * sinf(mPulseTimer);
            // mPulseTimer += dt * 2.f;  // 2 cycles par seconde
            (void)dt;
        }

        /**
         * @brief Met à jour les effets de hover sur les boutons.
         * @param dt Delta-time pour l'animation.
         */
        void UpdateButtonHoverEffects(float dt) {
            // Détection du bouton sous la souris + feedback visuel
            // (changement de couleur, scale, particules)
            (void)dt;
        }

        /**
         * @brief Affiche ou cache l'overlay de pause.
         * @param show true pour afficher, false pour cacher.
         */
        void ShowPauseOverlay(bool show) {
            // Spawn/Hide d'une entité UI "PauseOverlay"
            // Animation de fade-in/fade-out optionnelle
        }

        /**
         * @brief Sauvegarde les préférences utilisateur sur disque.
         */
        void SaveUserPreferences() {
            // Serialisation des settings (volume, contrôles, etc.)
            // via votre système de sauvegarde préféré
        }

        /**
         * @brief Retourne le handle du bouton à l'index donné.
         * @param index Index du bouton (0 à mButtonCount-1).
         * @return NkEntityId du bouton, ou Invalid si hors limites.
         */
        [[nodiscard]] NkEntityId GetButtonByIndex(nk_int32 index) const noexcept {
            if (index < 0 || index >= mButtonCount) {
                return NkEntityId::Invalid();
            }
            const NkEntityId buttons[] = {mButtonStart, mButtonOptions, mButtonQuit};
            return buttons[index];
        }
};


/**
 * @example 02_game_level_script.cpp
 * @brief Script de scène pour un niveau de jeu avec logique gameplay.
 * 
 * 🔹 Rôle de GameLevelScript :
 *    • Initialiser le niveau (spawn ennemis, config caméra, setup triggers)
 *    • Gérer la boucle de gameplay principale (ennemis, objectifs, timers)
 *    • Gérer la physique déterministe via OnFixedTick()
 *    • Gérer les transitions de victoire/défaite
 * 
 * 🔹 Différence avec MainMenuScript :
 *    • Plus de logique métier complexe (AI, collisions, progression)
 *    • Utilisation intensive de OnFixedTick() pour la physique
 *    • Interaction avec d'autres systèmes ECS via GetWorld()
 * 
 * @note Ce script illustre l'usage avancé des callbacks lifecycle
 *       et l'intégration avec le reste de l'architecture ECS.
 */
class GameLevelScript : public NkSceneScript {
    public:
        /**
         * @brief Constructeur avec paramètres de configuration du niveau.
         * @param difficulty Niveau de difficulté (1=facile, 3=difficile).
         * @param levelIndex Index du niveau pour chargement des assets spécifiques.
         * 
         * @note Les constructeurs avec paramètres sont supportés via
         *       scene->SetScript<GameLevelScript>(diff, index).
         */
        explicit GameLevelScript(nk_int32 difficulty = 1, nk_int32 levelIndex = 0) noexcept
            : mDifficulty(difficulty)
            , mLevelIndex(levelIndex) {
        }

        /**
         * @brief Initialisation du niveau — appelé une fois au chargement.
         * 
         * 🔹 Étapes typiques :
         *    1. Chargement des assets spécifiques au niveau (meshes, textures)
         *    2. Spawn des entités statiques (décor, triggers, checkpoints)
         *    3. Spawn des entités dynamiques (ennemis, items, projectiles)
         *    4. Configuration de la caméra et de l'éclairage
         *    5. Initialisation des variables d'état gameplay
         * 
         * @warning Éviter les chargements bloquants ici — préférer un
         *          système de streaming asynchrone si les assets sont lourds.
         */
        void OnBeginPlay() noexcept override {
            logger.Infof("[Level%d] Démarrage du niveau (difficulté: %d)\n",
                         mLevelIndex, mDifficulty);

            if (auto* scene = GetScene()) {
                // ── Chargement du décor statique ───────────────────────
                LoadLevelGeometry(mLevelIndex);
                SetupLightingAndFog();
                SetupAudioAmbiance();

                // ── Spawn du joueur et configuration initiale ──────────
                mPlayer = SpawnPlayerAtCheckpoint("SpawnPoint_Start");
                ConfigurePlayerForDifficulty(mPlayer, mDifficulty);

                // ── Spawn des ennemis selon la difficulté ─────────────
                SpawnEnemyWaves(CalculateEnemyCountForDifficulty());

                // ── Configuration de la caméra gameplay ────────────────
                SetupGameCamera();

                // ── Initialisation des systèmes de progression ─────────
                mObjectiveManager = std::make_unique<ObjectiveManager>();
                mObjectiveManager->LoadObjectivesForLevel(mLevelIndex);

                // ── Démarrage du timer de niveau ───────────────────────
                mLevelTimer.Start();
                mIsVictoryAchieved = false;
                mIsDefeatTriggered = false;
            }
        }

        /**
         * @brief Mise à jour physique déterministe — appelée à fréquence fixe.
         * @param fixedDt Delta-time CONSTANT (ex: 0.016666... pour 60Hz).
         * 
         * 🔹 Usage typique :
         *    • Physique des projectiles et des ragdolls
         *    • Détection des collisions gameplay (hitboxes, triggers)
         *    • Synchronisation réseau tick-based (si multijoueur)
         *    • Tout calcul qui doit être reproductible frame par frame
         * 
         * @warning ⚠️ Le paramètre fixedDt est CONSTANT — ne pas le multiplier
         *          par un facteur variable sous peine de casser la physique.
         *          Utiliser tel quel : velocity += acceleration * fixedDt;
         */
        void OnFixedTick(float fixedDt) noexcept override {
            // Guard : skip si le niveau est en pause ou terminé
            if (!IsLevelActive()) {
                return;
            }

            // ── Mise à jour de la physique des projectiles ─────────────
            UpdateProjectilesPhysics(fixedDt);

            // ── Détection des collisions gameplay ──────────────────────
            CheckPlayerEnemyCollisions();
            CheckTriggerZoneActivations();

            // ── Mise à jour de l'IA des ennemis (décisions tick-based) ─
            UpdateEnemyAI(fixedDt);

            // ── Synchronisation réseau (si multijoueur) ────────────────
            // if (IsMultiplayer()) {
            //     NetworkSync::Update(fixedDt);
            // }

            (void)fixedDt;  // Supprime warning si implémentation temporaire
        }

        /**
         * @brief Mise à jour gameplay principale — appelée chaque frame variable.
         * @param dt Delta-time VARIABLE depuis la frame précédente.
         * 
         * 🔹 Usage typique :
         *    • Input handling du joueur (mouvement, actions, UI)
         *    • Mise à jour des timers et conditions de victoire/défaite
         *    • Émission d'événements gameplay (dégâts, achievements, etc.)
         *    • Logique métier dépendante du temps mais non-déterministe
         * 
         * @note Utiliser dt pour du code frame-independent :
         *       playerPosition += playerVelocity * dt;
         *       cooldownTimer -= dt;
         */
        void OnTick(float dt) noexcept override {
            // Guard : skip si le niveau est en pause ou terminé
            if (!IsLevelActive()) {
                return;
            }

            // ── Mise à jour du timer de niveau ─────────────────────────
            mLevelTimer.Update(dt);

            // ── Gestion des inputs du joueur ───────────────────────────
            HandlePlayerInput(dt);

            // ── Mise à jour des objectifs et progression ───────────────
            if (mObjectiveManager) {
                mObjectiveManager->Update(dt);
                CheckVictoryConditions();
                CheckDefeatConditions();
            }

            // ── Gestion des événements gameplay ────────────────────────
            ProcessQueuedGameplayEvents();

            // ── Mise à jour des effets visuels/sonores ─────────────────
            UpdateVisualEffects(dt);
            UpdateAudioCues(dt);
        }

        /**
         * @brief Mise à jour tardive — après toute la logique gameplay.
         * @param dt Delta-time en secondes.
         * 
         * 🔹 Usage typique :
         *    • Mise à jour de la caméra gameplay (après que le joueur a bougé)
         *    • Calculs d'UI dépendants des positions finales (minimap, health bars)
         *    • Post-traitement des effets de screen-space (bloom, motion blur)
         * 
         * @note LateTick est idéal pour les calculs qui doivent voir l'état
         *       "final" de la frame avant le rendu.
         */
        void OnLateTick(float dt) noexcept override {
            if (!IsLevelActive()) {
                return;
            }

            // ── Mise à jour de la caméra gameplay ──────────────────────
            UpdateGameCamera(dt);

            // ── Mise à jour de l'UI dépendante du monde ────────────────
            UpdateWorldSpaceUI(dt);  // Health bars au-dessus des ennemis
            UpdateMinimap(dt);        // Position des marqueurs sur la carte

            // ── Post-traitement des effets visuels ─────────────────────
            // renderer.UpdateScreenSpaceEffects(dt);

            (void)dt;
        }

        /**
         * @brief Notification de pause — niveau mis en pause.
         * 
         * 🔹 Usage typique :
         *    • Stopper la logique gameplay (ennemis, timers, physique)
         *    • Mettre en sourdine l'audio gameplay (garder l'UI audible)
         *    • Afficher le menu de pause avec options (reprendre, quitter, etc.)
         * 
         * @note La pause n'affecte PAS le rendu — le niveau reste visible
         *       sauf si explicitement caché.
         */
        void OnPause() noexcept override {
            logger.Info("[Level%d] Niveau en pause\n", mLevelIndex);
            mIsPaused = true;

            // Stopper les timers gameplay (mais garder le temps écoulé)
            mLevelTimer.Pause();

            // Baisser le volume de l'audio gameplay
            // AudioManager::SetCategoryVolume("Gameplay", 0.2f);

            // Afficher le menu de pause
            ShowPauseMenu(true);
        }

        /**
         * @brief Notification de reprise — niveau repris après pause.
         * 
         * 🔹 Usage typique :
         *    • Reprendre les timers et la logique gameplay
         *    • Restaurer le volume audio
         *    • Cacher le menu de pause avec transition
         * 
         * @note OnResume() peut être appelé même sans OnPause() préalable
         *       (ex: reprise après chargement d'une sauvegarde).
         */
        void OnResume() noexcept override {
            logger.Info("[Level%d] Niveau repris\n", mLevelIndex);
            mIsPaused = false;

            // Reprendre le timer de niveau
            mLevelTimer.Resume();

            // Restaurer le volume audio
            // AudioManager::SetCategoryVolume("Gameplay", 1.0f);

            // Cacher le menu de pause avec fade-out
            ShowPauseMenu(false);
        }

        /**
         * @brief Point de sortie — nettoyage avant déchargement du niveau.
         * 
         * 🔹 Usage typique :
         *    • Sauvegarde de la progression joueur (checkpoints, stats)
         *    • Libération des assets spécifiques au niveau
         *    • Nettoyage des listeners/events souscrits
         *    • Logging des statistiques de fin de niveau (temps, score, etc.)
         * 
         * @note Garantir noexcept pour éviter les crashes au déchargement.
         */
        void OnEndPlay() noexcept override {
            logger.Infof("[Level%d] Fin du niveau\n", mLevelIndex);

            // ── Sauvegarde de la progression ───────────────────────────
            SavePlayerProgress();

            // ── Logging des statistiques ───────────────────────────────
            LogLevelStatistics();

            // ── Libération des assets spécifiques ──────────────────────
            // AssetManager::UnloadCategory(NkString::Format("Level%d", mLevelIndex));

            // ── Reset de l'état interne ────────────────────────────────
            mPlayer = NkGameObject();  // Reset handle
            mIsPaused = false;
            mIsVictoryAchieved = false;
            mIsDefeatTriggered = false;
            mObjectiveManager.reset();
        }

    private:
        // ── Configuration du niveau ────────────────────────────────────
        nk_int32 mDifficulty = 1;   ///< Niveau de difficulté (1-3)
        nk_int32 mLevelIndex = 0;   ///< Index du niveau pour chargement assets

        // ── État gameplay ──────────────────────────────────────────────
        NkGameObject mPlayer;       ///< Handle du joueur (peut être invalide)
        GameTimer mLevelTimer;      ///< Timer du niveau avec support pause/resume
        bool mIsPaused = false;     ///< Flag de pause du niveau
        bool mIsVictoryAchieved = false;  ///< Flag de victoire
        bool mIsDefeatTriggered = false;  ///< Flag de défaite

        // ── Systèmes métier ────────────────────────────────────────────
        std::unique_ptr<ObjectiveManager> mObjectiveManager;  ///< Gestion des objectifs

        // ── Helpers de gameplay ────────────────────────────────────────
        /**
         * @brief Vérifie si le niveau est actif (non-pausé, non-terminé).
         * @return true si la logique gameplay doit s'exécuter.
         */
        [[nodiscard]] bool IsLevelActive() const noexcept {
            return !mIsPaused && !mIsVictoryAchieved && !mIsDefeatTriggered;
        }

        /**
         * @brief Charge la géométrie du niveau depuis les assets.
         * @param levelIndex Index du niveau à charger.
         */
        void LoadLevelGeometry(nk_int32 levelIndex) {
            // Chargement des meshes, collision meshes, navmesh, etc.
            // Via votre système d'asset management
        }

        /**
         * @brief Configure l'éclairage et le brouillard du niveau.
         */
        void SetupLightingAndFog() {
            // Configuration des lumières directionnelles, ambient, fog
            // renderer.SetLightingPreset(levelIndex);
        }

        /**
         * @brief Configure l'ambiance sonore du niveau.
         */
        void SetupAudioAmbiance() {
            // Lancement de la musique de fond et des ambiances 3D
            // AudioManager::PlayAmbience("Forest_Ambience", /*loop=*/true);
        }

        /**
         * @brief Spawn le joueur au checkpoint spécifié.
         * @param checkpointName Nom du checkpoint dans la scène.
         * @return NkGameObject du joueur spawné.
         */
        [[nodiscard]] NkGameObject SpawnPlayerAtCheckpoint(const char* checkpointName) {
            if (auto* scene = GetScene()) {
                // Trouver le point de spawn par nom
                auto spawnPoint = scene->FindByName(checkpointName);
                if (spawnPoint.IsValid()) {
                    // Spawn du prefab joueur à la position du checkpoint
                    return scene->SpawnPrefab(
                        GetPlayerPrefab(),
                        spawnPoint.GetPosition(),
                        "Player"
                    );
                }
            }
            // Fallback : spawn à l'origine si checkpoint non trouvé
            return NkGameObject();
        }

        /**
         * @brief Configure le joueur selon la difficulté sélectionnée.
         * @param player Handle du joueur à configurer.
         * @param difficulty Niveau de difficulté (1-3).
         */
        void ConfigurePlayerForDifficulty(NkGameObject& player, nk_int32 difficulty) {
            if (!player.IsValid()) {
                return;
            }

            // Ajustement des stats selon la difficulté
            if (auto* health = player.Get<HealthComponent>()) {
                health->maxValue = 100 + (difficulty - 1) * 25;  // +25 PV par niveau
                health->value = health->maxValue;
            }
            // Autres ajustements : dégâts, vitesse, cooldowns, etc.
        }

        /**
         * @brief Calcule le nombre d'ennemis à spawn selon la difficulté.
         * @return Nombre d'ennemis pour la première vague.
         */
        [[nodiscard]] nk_int32 CalculateEnemyCountForDifficulty() const noexcept {
            // Formule simple : base + multiplicateur par difficulté
            return 5 + (mDifficulty - 1) * 3;  // 5, 8, ou 11 ennemis
        }

        /**
         * @brief Spawn les vagues d'ennemis initiales.
         * @param count Nombre d'ennemis à spawn.
         */
        void SpawnEnemyWaves(nk_int32 count) {
            if (auto* scene = GetScene()) {
                for (nk_int32 i = 0; i < count; ++i) {
                    NkVec3f spawnPos = GetRandomEnemySpawnPosition();
                    auto enemy = scene->SpawnPrefab(
                        GetEnemyPrefabForLevel(),
                        spawnPos,
                        NkString::Format("Enemy_%d", i).CStr()
                    );
                    if (enemy.IsValid()) {
                        ConfigureEnemyForDifficulty(enemy, mDifficulty);
                    }
                }
            }
        }

        /**
         * @brief Configure la caméra gameplay (follow player, cinématique, etc.).
         */
        void SetupGameCamera() {
            // Configuration de la caméra principale pour suivre le joueur
            // mGameCamera->SetFollowTarget(mPlayer);
            // mGameCamera->SetMode(CameraMode::ThirdPerson);
        }

        /**
         * @brief Gère les inputs du joueur pour le mouvement et les actions.
         * @param dt Delta-time pour les mouvements frame-independent.
         */
        void HandlePlayerInput(float dt) {
            if (!mPlayer.IsValid()) {
                return;
            }

            // Exemple simplifié : mouvement directionnel
            NkVec2f inputAxis = {
                InputSystem::GetAxis(NkAxis::Horizontal),
                InputSystem::GetAxis(NkAxis::Vertical)
            };

            if (inputAxis.LengthSquared() > 0.01f) {
                // Déplacement du joueur selon l'input
                NkVec3f moveDir = {inputAxis.x, 0.f, inputAxis.y};
                mPlayer.SetPosition(
                    mPlayer.GetPosition() + moveDir * 5.f * dt  // 5 units/sec
                );
            }

            // Action : saut si espace appuyé
            if (InputSystem::IsPressed(NkKey::Space)) {
                // player.Jump();  // Méthode spécifique au composant Player
            }
        }

        /**
         * @brief Met à jour la physique des projectiles (mouvement, collisions).
         * @param fixedDt Delta-time fixe pour la physique déterministe.
         */
        void UpdateProjectilesPhysics(float fixedDt) {
            // Query sur tous les projectiles actifs
            if (auto* world = GetWorld()) {
                world->Query<ProjectileComponent, NkTransform>()
                    .ForEach([=](NkEntityId id, ProjectileComponent& proj, NkTransform& transform) {
                        // Mise à jour de la position selon la vélocité
                        transform.position += proj.velocity * fixedDt;
                        // Durée de vie du projectile
                        proj.lifetime -= fixedDt;
                        if (proj.lifetime <= 0.f) {
                            world->DestroyDeferred(id);  // Destruction sécurisée
                        }
                    });
            }
        }

        /**
         * @brief Vérifie les collisions joueur-ennemis pour appliquer les dégâts.
         */
        void CheckPlayerEnemyCollisions() {
            if (!mPlayer.IsValid()) {
                return;
            }

            // Query sur les ennemis avec hitbox
            if (auto* world = GetWorld()) {
                world->Query<EnemyComponent, HitboxComponent, NkTransform>()
                    .ForEach([this](NkEntityId, EnemyComponent&, HitboxComponent& hitbox, NkTransform& enemyTransform) {
                        // Test de collision simplifié (sphere vs sphere)
                        NkVec3f playerPos = mPlayer.GetPosition();
                        NkVec3f enemyPos = enemyTransform.position;
                        float distance = NkDistance(playerPos, enemyPos);
                        
                        if (distance < hitbox.radius) {
                            // Collision détectée : appliquer dégâts au joueur
                            ApplyDamageToPlayer(hitbox.damageAmount);
                        }
                    });
            }
        }

        /**
         * @brief Vérifie les conditions de victoire du niveau.
         */
        void CheckVictoryConditions() {
            if (mIsVictoryAchieved || mIsDefeatTriggered) {
                return;  // Déjà terminé
            }

            // Exemple : victoire si tous les ennemis sont morts
            if (auto* world = GetWorld()) {
                nk_int32 aliveEnemies = 0;
                world->Query<EnemyComponent, HealthComponent>()
                    .ForEach([&](NkEntityId, EnemyComponent&, HealthComponent& health) {
                        if (health.value > 0) {
                            ++aliveEnemies;
                        }
                    });
                
                if (aliveEnemies == 0) {
                    TriggerVictory();
                }
            }
        }

        /**
         * @brief Vérifie les conditions de défaite du niveau.
         */
        void CheckDefeatConditions() {
            if (mIsVictoryAchieved || mIsDefeatTriggered) {
                return;
            }

            // Exemple : défaite si le joueur n'a plus de PV
            if (mPlayer.IsValid()) {
                if (auto* health = mPlayer.Get<HealthComponent>()) {
                    if (health->value <= 0) {
                        TriggerDefeat();
                    }
                }
            }
        }

        /**
         * @brief Déclenche la séquence de victoire du niveau.
         */
        void TriggerVictory() {
            logger.Info("[Level%d] Victoire !\n", mLevelIndex);
            mIsVictoryAchieved = true;

            // Afficher l'UI de victoire, jouer le son, etc.
            // ShowVictoryScreen();
            // AudioManager::PlaySound("Victory_Fanfare");

            // Optionnel : charger le niveau suivant après délai
            // NkTimer::After(3.0f, [this]() { LoadNextLevel(); });
        }

        /**
         * @brief Déclenche la séquence de défaite du niveau.
         */
        void TriggerDefeat() {
            logger.Info("[Level%d] Défaite\n", mLevelIndex);
            mIsDefeatTriggered = true;

            // Afficher l'UI de défaite, proposer de recommencer
            // ShowDefeatScreen();
        }

        /**
         * @brief Sauvegarde la progression du joueur sur disque.
         */
        void SavePlayerProgress() {
            // Serialisation des stats, checkpoints débloqués, etc.
            // SaveSystem::SavePlayerData(playerId, progressData);
        }

        /**
         * @brief Log les statistiques de fin de niveau pour analytics.
         */
        void LogLevelStatistics() {
            float totalTime = mLevelTimer.GetElapsedTime();
            logger.Info("[Level%d] Stats: temps=%.1fs, difficulté=%d\n",
                        mLevelIndex, totalTime, mDifficulty);
            // Envoyer aux analytics si nécessaire
        }

        // ── Helpers d'assets (simplifiés pour l'exemple) ───────────────
        [[nodiscard]] NkPrefab GetPlayerPrefab() const noexcept {
            return NkPrefabRegistry::Global().Get("Prefabs/Player_Default");
        }

        [[nodiscard]] NkPrefab GetEnemyPrefabForLevel() const noexcept {
            const char* prefabs[] = {"Prefabs/Enemy_Basic", "Prefabs/Enemy_Advanced", "Prefabs/Enemy_Elite"};
            nk_int32 index = NkClamp(mDifficulty - 1, 0, 2);
            return NkPrefabRegistry::Global().Get(prefabs[index]);
        }

        [[nodiscard]] NkVec3f GetRandomEnemySpawnPosition() const noexcept {
            // Retourne une position aléatoire dans la zone de spawn
            return {
                NkRandom::Range(-10.f, 10.f),
                0.f,
                NkRandom::Range(-10.f, 10.f)
            };
        }

        void ConfigureEnemyForDifficulty(NkGameObject& enemy, nk_int32 difficulty) {
            if (!enemy.IsValid()) {
                return;
            }
            // Augmenter les PV et les dégâts selon la difficulté
            if (auto* health = enemy.Get<HealthComponent>()) {
                health->maxValue *= (1.f + 0.2f * (difficulty - 1));
                health->value = health->maxValue;
            }
            // Autres ajustements : vitesse, IA aggressiveness, etc.
        }

        void UpdateGameCamera(float dt) {
            // Logique de follow camera : interpolation vers le joueur
            if (mPlayer.IsValid() /* && mGameCamera */) {
                NkVec3f targetPos = mPlayer.GetPosition() + NkVec3f{0, 5, -10};
                // mGameCamera->SetPosition(NkLerp(current, target, 0.05f));
            }
            (void)dt;
        }

        void UpdateWorldSpaceUI(float dt) {
            // Mise à jour des health bars au-dessus des ennemis
            // Positionnement basé sur NkWorldTransform des entités
            (void)dt;
        }

        void UpdateMinimap(float dt) {
            // Mise à jour des marqueurs sur la minimap
            // Filtrage par distance au joueur, culling hors écran
            (void)dt;
        }

        void UpdateVisualEffects(float dt) {
            // Mise à jour des particules, post-processing, etc.
            (void)dt;
        }

        void UpdateAudioCues(float dt) {
            // Mise à jour des sources audio 3D, fade selon distance
            (void)dt;
        }

        void ProcessQueuedGameplayEvents() {
            // Traitement des événements gameplay en file d'attente
            // (dégâts, achievements, dialogues, etc.)
        }

        void ShowPauseMenu(bool show) {
            // Spawn/Hide de l'UI de pause avec animation
        }

        void ApplyDamageToPlayer(float amount) {
            if (mPlayer.IsValid()) {
                if (auto* health = mPlayer.Get<HealthComponent>()) {
                    health->value -= amount;
                    // Trigger hit feedback : son, particules, screen shake
                }
            }
        }
};

/** @} */ // end of SceneScript_Examples group


// =============================================================================
// SECTION 2 : ENREGISTREMENT DES SCÈNES DANS NKSCENEMANAGER
// =============================================================================
/**
 * @addtogroup SceneManager_Examples
 * @{
 */

/**
 * @example 03_register_scenes.cpp
 * @brief Enregistrement des scènes dans NkSceneManager au démarrage.
 * 
 * 🔹 Pourquoi enregistrer les scènes ?
 *    • Permet un chargement dynamique par nom ("MainMenu", "Level_1", etc.)
 *    • Centralise la logique de création dans des factories
 *    • Facilite les transitions et le debugging (liste des scènes disponibles)
 * 
 * 🔹 Bonnes pratiques :
 *    • Enregistrer TOUTES les scènes au démarrage, avant tout LoadScene()
 *    • Utiliser des lambdas pour les factories simples, des fonctions pour les complexes
 *    • Documenter le nom attendu de chaque scène pour éviter les typos
 * 
 * @note Les factories retournent un NkSceneGraph* via new — le SceneManager
 *       en prend ownership et appelle delete au déchargement.
 */
void Example_RegisterScenes(NkSceneManager& mgr) {
    // ── Scène : Menu Principal ───────────────────────────────────────
    /**
     * @brief Factory pour la scène de menu principal.
     * @param world Référence au monde ECS dans lequel créer la scène.
     * @return Pointeur vers la NkSceneGraph nouvellement créée.
     * 
     * 🔹 Étapes de création :
     *    1. new NkSceneGraph(world, "MainMenu") — création de base
     *    2. SetScript<MainMenuScript>() — attachement du script métier
     *    3. Spawn des éléments statiques (décor UI, logo, fond)
     *    4. Configuration initiale (caméra UI, ambiance sonore)
     *    5. Retour du pointeur — ownership transféré au SceneManager
     */
    mgr.Register("MainMenu", [](NkWorld& world) -> NkSceneGraph* {
        // Création de la scène avec nom lisible pour debugging
        auto* scene = new NkSceneGraph(world, "MainMenu");
        
        // Attachement du script de scène — gère la logique interactive
        scene->SetScript<MainMenuScript>();
        
        // ── Spawn des éléments UI statiques ─────────────────────────
        // Ces éléments ne nécessitent pas de logique complexe — purement visuels
        scene->SpawnNode("UI_Background");      // Fond décoratif
        scene->SpawnNode("UI_Logo");            // Logo du jeu
        scene->SpawnNode("UI_VersionText");     // Texte de version (ex: "v1.2.3")
        
        // ── Configuration optionnelle ───────────────────────────────
        // Si vous avez un système de caméra UI dédié :
        // auto* cam = scene->SpawnActor<Camera>("UI_Camera");
        // cam->SetOrthographic(true);
        // cam->SetSize(1920, 1080);  // Résolution UI
        
        return scene;  // Ownership transféré — ne pas delete manuellement
    });

    // ── Scène : Niveau de Jeu ────────────────────────────────────────
    /**
     * @brief Factory pour la scène de niveau de jeu.
     * @note Utilise un script avec paramètres de construction.
     * 
     * 🔹 Pourquoi des paramètres au script ?
     *    • Permet de configurer la difficulté, l'index de niveau, etc.
     *    • Évite d'avoir à créer des scènes différentes pour chaque variante
     *    • Centralise la logique de configuration dans le script lui-même
     */
    mgr.Register("GameLevel", [](NkWorld& world) -> NkSceneGraph* {
        auto* scene = new NkSceneGraph(world, "GameLevel");
        
        // Script avec paramètres : difficulté=2, niveau=1
        scene->SetScript<GameLevelScript>(/*difficulty=*/2, /*levelIndex=*/1);
        
        // Note : le spawn des ennemis/décor est géré dans OnBeginPlay()
        // du script — pas besoin de le faire ici dans la factory
        // Cela garde la factory légère et la logique métier dans le script
        
        return scene;
    });

    // ── Scène : Écran de Chargement (optionnel mais recommandé) ─────
    /**
     * @brief Factory pour la scène de chargement intermédiaire.
     * 
     * 🔹 Rôle de LoadingScreen :
     *    • Masquer le temps de chargement des assets lourds
     *    • Afficher une barre de progression ou des conseils gameplay
     *    • Maintenir l'immersion pendant les transitions longues
     * 
     * 🔹 Pourquoi pas de script ?
     *    • La scène de chargement est purement visuelle — pas de logique métier
     *    • La progression est gérée par le système de streaming d'assets
     *    • Évite la complexité inutile pour un cas d'usage simple
     */
    mgr.Register("LoadingScreen", [](NkWorld& world) -> NkSceneGraph* {
        auto* scene = new NkSceneGraph(world, "LoadingScreen");
        
        // Pas de script — juste des éléments UI statiques
        scene->SpawnNode("Loading_Background");  // Fond sombre/blur
        scene->SpawnNode("Loading_Bar");         // Barre de progression
        scene->SpawnNode("Loading_Text");        // Texte "Chargement..."
        scene->SpawnNode("Loading_Tip");         // Conseil gameplay aléatoire
        
        // Optionnel : animation de la barre via un composant simple
        // auto* bar = scene->SpawnNode("Loading_Bar_Animation");
        // world.Add<ProgressBarComponent>(bar, ProgressBarComponent{0.f, 1.f});
        
        return scene;
    });

    // ── Configuration de la scène de chargement par défaut ───────────
    /**
     * @brief Définit quelle scène utiliser pour les transitions avec chargement.
     * 
     * 🔹 Usage :
     *    • LoadScene("Level_2", NkSceneTransition::WithLoading("LoadingScreen"))
     *    • Affiche automatiquement "LoadingScreen" pendant le chargement
     * 
     * @note Si non défini, les transitions WithLoading() ignoreront l'écran
     *       de chargement et feront juste un fondu — moins immersif mais fonctionnel.
     */
    mgr.SetLoadingScene("LoadingScreen");

    // ── Logging de confirmation (optionnel pour le debug) ────────────
    logger.Info("[SceneManager] %u scènes enregistrées\n", 3);
    // Pour lister les noms : itérer sur le registre interne si exposé
}

/** @} */ // end of SceneManager_Examples group


// =============================================================================
// SECTION 3 : CONFIGURATION DU SCHEDULER AVEC LIFECYCLE SYSTEMS
// =============================================================================
/**
 * @addtogroup Scheduler_Examples
 * @{
 */

/**
 * @example 04_setup_scheduler.cpp
 * @brief Enregistrement des systèmes de lifecycle dans NkScheduler.
 * 
 * 🔹 Pourquoi trois systèmes distincts ?
 *    • FixedTick : physique/réseau déterministe (fréquence fixe, ex: 60Hz)
 *    • Tick : logique gameplay principale (fréquence variable, dépend du framerate)
 *    • LateTick : post-traitement (caméra, UI, effets) après toute la logique
 * 
 * 🔹 Groupes d'exécution dans le scheduler :
 *    • NkSystemGroup::FixedUpdate → exécuté avant la physique
 *    • NkSystemGroup::Update → exécuté après la physique, avant le rendu
 *    • NkSystemGroup::PostUpdate → exécuté en dernier, juste avant le rendu
 * 
 * @note Les priorités (100 pour Tick/FixedTick, 200 pour LateTick) garantissent
 *       l'ordre d'exécution au sein de chaque groupe.
 */
void Example_SetupScheduler(NkScheduler& scheduler, NkSceneManager& mgr) {
    // ── Approche recommandée : lifecycle sur la scène COURANTE ───────
    /**
     * @brief Stratégie : les systèmes référencent mgr.GetCurrent() à chaque frame.
     * 
     * 🔹 Avantages :
     *    • Pas besoin de ré-enregistrer les systèmes quand la scène change
     *    • Les systèmes appellent toujours la scène active via le manager
     *    • Code plus simple et moins sujet aux erreurs de synchronisation
     * 
     * 🔹 Inconvénient mineur :
     *    • Un indirection supplémentaire (mgr.GetCurrent()) à chaque appel
     *    • Impact négligeable : un pointeur dereferenced, pas d'allocation
     * 
     * @warning Cette approche suppose que NkSceneManager::GetCurrent()
     *          retourne rapidement et de façon thread-safe (si applicable).
     */
    
    // ── Création des systèmes avec référence au manager ─────────────
    // Note : on pourrait créer un système wrapper qui forward vers mgr.GetCurrent()
    // mais pour la simplicité, on utilise les systèmes fournis directement
    
    // Alternative plus robuste : créer un système "SceneLifecycleForwarder"
    // qui tient une référence au manager et forward vers GetCurrent()
    // (implémentation optionnelle — voir bonus ci-dessous)
    
    // Enregistrement direct avec la scène courante (simple mais à ré-enregistrer si scène change)
    if (mgr.HasScene()) {
        scheduler.AddSystem(new NkSceneFixedTickSystem(mgr.GetCurrent()));
        scheduler.AddSystem(new NkSceneTickSystem(mgr.GetCurrent()));
        scheduler.AddSystem(new NkSceneLateTickSystem(mgr.GetCurrent()));
    }

    // ── Bonus : Système forwarder pour gestion automatique ──────────
    /**
     * @brief Système optionnel qui forward automatiquement vers la scène courante.
     * 
     * 🔹 Pourquoi ce wrapper ?
     *    • Évite de devoir ré-enregistrer les systèmes à chaque changement de scène
     *    • Centralise la logique de forward dans un seul endroit
     *    • Plus robuste face aux changements dynamiques de scène
     * 
     * @note Ce système n'est pas fourni par défaut — à implémenter si besoin.
     */
    /*
    class SceneLifecycleForwarder : public NkSystem {
    public:
        explicit SceneLifecycleForwarder(NkSceneManager* mgr) noexcept
            : mManager(mgr) {}
        
        NkSystemDesc Describe() const override {
            return NkSystemDesc{}
                .InGroup(NkSystemGroup::Update)
                .Named("SceneLifecycleForwarder")
                .Priority(50);  // Exécuté tôt dans Update
        }
        
        void Execute(NkWorld&, float dt) override {
            if (mManager && mManager->HasScene()) {
                // Forward vers les trois callbacks de la scène courante
                // Note : on pourrait séparer en trois systèmes distincts
                // pour respecter les groupes FixedUpdate/Update/PostUpdate
                auto* scene = mManager->GetCurrent();
                scene->FixedTick(mFixedDt);  // mFixedDt à mettre à jour ailleurs
                scene->Tick(dt);
                // LateTick dans un autre système PostUpdate si nécessaire
            }
        }
        
        void SetFixedDt(float dt) noexcept { mFixedDt = dt; }
        
    private:
        NkSceneManager* mManager = nullptr;
        float mFixedDt = 1.f / 60.f;
    };
    
    // Usage :
    // scheduler.AddSystem(new SceneLifecycleForwarder(&mgr));
    */
}

/** @} */ // end of Scheduler_Examples group


// =============================================================================
// SECTION 4 : CHARGEMENT ET TRANSITIONS ENTRE SCÈNES
// =============================================================================
/**
 * @addtogroup SceneTransitions_Examples
 * @{
 */

/**
 * @example 05_scene_transitions.cpp
 * @brief Exemples de chargement et transitions entre scènes.
 * 
 * 🔹 Types de transitions supportés :
 *    • Instant() : changement immédiat, aucune animation (debug, tests)
 *    • Fade(dur) : fondu vers le noir puis vers la nouvelle scène
 *    • FadeWhite(dur) : fondu vers le blanc (effets spéciaux, rêves)
 *    • WithLoading(name, fadeDur) : fondu + écran de chargement intermédiaire
 * 
 * 🔹 Bonnes pratiques :
 *    • Utiliser Instant() pour le debug et les tests rapides
 *    • Utiliser Fade(0.5f) pour l'expérience joueur normale
 *    • Utiliser WithLoading() pour les niveaux lourds avec streaming d'assets
 * 
 * @warning LoadScene() est synchrone et bloquant — le thread appelant
 *          est suspendu pendant toute la durée du chargement.
 *          Pour du chargement asynchrone, utiliser une scène de chargement
 *          avec un système de background loading.
 */
void Example_Transitions(NkSceneManager& mgr) {
    // ── Transition instantanée (debug, tests rapides) ───────────────
    /**
     * @brief Changement immédiat sans animation.
     * 
     * 🔹 Usage recommandé :
     *    • Débogage : tester un niveau sans attendre les animations
     *    • Tests automatisés : vitesse d'exécution prioritaire
     *    • Transitions internes très rapides (ex: sous-menus UI)
     * 
     * @note Le paramètre de transition est optionnel — Instant() est le défaut.
     */
    mgr.LoadScene("MainMenu");  // Équivalent à LoadScene("MainMenu", NkSceneTransition::Instant())

    // ── Fondu noir standard (expérience joueur normale) ─────────────
    /**
     * @brief Transition par fondu vers le noir de durée spécifiée.
     * @param duration Durée du fondu en secondes (0.5f = demi-seconde).
     * 
     * 🔹 Flux visuel :
     *    1. Fondu vers noir sur 'duration' secondes
     *    2. Chargement de la nouvelle scène (masqué par le noir)
     *    3. Fondu depuis noir vers la nouvelle scène sur 'duration' secondes
     * 
     * @note La durée totale est donc ~2*duration + temps de chargement réel.
     */
    mgr.LoadScene("GameLevel", NkSceneTransition::Fade(0.5f));

    // ── Fondu blanc pour effet spécial ─────────────────────────────
    /**
     * @brief Transition par fondu vers le blanc.
     * 
     * 🔹 Usage typique :
     *    • Flash de lumière intense (explosion, sort magique)
     *    • Transition onirique (rêve, souvenir, flashback)
     *    • Effet de téléportation ou changement de réalité
     * 
     * @note Fonctionnellement identique à Fade() mais avec une couleur différente.
     *       L'implémentation du rendu doit respecter NkTransitionType::FadeWhite.
     */
    mgr.LoadScene("DreamSequence", NkSceneTransition::FadeWhite(0.8f));

    // ── Transition avec écran de chargement intermédiaire ───────────
    /**
     * @brief Fondu + scène de chargement affichée pendant le chargement réel.
     * @param loadingScene Nom de la scène à afficher pendant le chargement.
     * @param fadeDur Durée des fondus d'entrée/sortie (par défaut 0.3f).
     * 
     * 🔹 Flux détaillé :
     *    1. Fondu vers noir sur fadeDur secondes
     *    2. Affichage de loadingScene (ex: barre de progression + conseils)
     *    3. Chargement asynchrone de la scène cible en arrière-plan
     *    4. Une fois chargée : fondu depuis noir vers la nouvelle scène
     * 
     * 🔹 Pourquoi cette complexité ?
     *    • Masque le temps de chargement des assets lourds (textures, meshes)
     *    • Maintient l'immersion avec des conseils gameplay ou du lore
     *    • Permet un feedback visuel de progression (barre qui avance)
     * 
     * @warning La scène de chargement doit être LÉGÈRE — son rôle est de
     *          masquer le chargement, pas de le prolonger. Éviter d'y mettre
     *          des assets lourds ou de la logique complexe.
     */
    mgr.LoadScene("Level_3",
                NkSceneTransition::WithLoading("LoadingScreen", 0.3f));

    // ── Chargement depuis un script de scène (pattern courant) ──────
    /**
     * @brief Exemple : changer de niveau depuis la logique gameplay.
     * 
     * 🔹 Accès au SceneManager depuis un script :
     *    1. GetScene() → NkSceneGraph*
     *    2. scene->GetManager() → NkSceneManager*
     *    3. mgr->LoadScene(...) → chargement effectif
     * 
     * @note Toujours vérifier les pointeurs intermédiaires pour éviter les crashes.
     */
    /*
    // Dans un NkSceneScript dérivé :
    void MyLevelScript::OnVictory() {
        if (auto* scene = GetScene()) {
            if (auto* mgr = scene->GetManager()) {
                // Afficher un message de victoire pendant 2s, puis charger le suivant
                ShowVictoryUI();
                NkTimer::After(2.0f, [mgr]() {
                    mgr->LoadScene("Level_Next", NkSceneTransition::Fade(1.0f));
                });
            }
        }
    }
    */

    // ── Rechargement de la scène courante (restart de niveau) ───────
    /**
     * @brief Recharge la scène actuellement active avec transition Instant().
     * @return true si le rechargement a réussi, false si aucune scène active.
     * 
     * 🔹 Usage typique :
     *    • Joueur mort → restart du niveau
     *    • Debug : tester un niveau depuis le début rapidement
     *    • Speedrun : reset rapide sans retour au menu principal
     * 
     * @note Utilise une transition Instant() par défaut pour un restart rapide.
     *       Pour une transition animée, appeler LoadScene() directement avec
     *       le NkSceneTransition souhaité.
     */
    mgr.ReloadCurrent();  // Équivalent à LoadScene(mgr.GetCurrentName(), Instant())

    // ── Pause / Resume de la scène courante ─────────────────────────
    /**
     * @brief Met en pause ou reprend la scène active.
     * 
     * 🔹 Effets de Pause() :
     *    • Skip des callbacks Tick/FixedTick/LateTick sur la scène
     *    • Appel de OnPause() sur le script de scène si présent
     *    • La scène reste visible sauf si explicitement cachée
     * 
     * 🔹 Effets de Resume() :
     *    • Reprise des callbacks Tick/FixedTick/LateTick
     *    • Appel de OnResume() sur le script de scène si présent
     * 
     * @note Ces méthodes sont des wrappers vers NkSceneGraph::Pause()/Resume().
     *       Elles sont silencieusement ignorées si aucune scène n'est chargée.
     */
    mgr.Pause();   // Met la scène en pause
    mgr.Resume();  // Reprend la scène après pause
}

/** @} */ // end of SceneTransitions_Examples group


// =============================================================================
// SECTION 5 : ACCÈS ET MANIPULATION DES SCRIPTS DE SCÈNE
// =============================================================================
/**
 * @addtogroup ScriptAccess_Examples
 * @{
 */

/**
 * @example 06_access_script.cpp
 * @brief Accéder et manipuler un script de scène depuis le code métier.
 * 
 * 🔹 Pourquoi accéder au script ?
 *    • Déclencher des événements gameplay depuis l'UI ou d'autres systèmes
 *    • Modifier des paramètres de difficulté en temps réel (debug, cheats)
 *    • Synchroniser l'état du script avec d'autres composants (audio, sauvegarde)
 * 
 * 🔹 Bonnes pratiques :
 *    • Toujours vérifier HasScene() avant d'accéder à GetCurrent()
 *    • Utiliser GetScript<T>() avec vérification nullptr pour le type-safe
 *    • Éviter de stocker des pointeurs vers le script au-delà de la frame
 * 
 * @warning Les scripts de scène sont des singletons par scène — ne pas
 *          créer plusieurs instances du même type de script pour une scène.
 */
void Example_AccessScript(NkSceneManager& mgr) {
    // ── Vérification de base : une scène est-elle chargée ? ─────────
    /**
     * @brief Guard essentiel avant toute opération sur la scène.
     * 
     * 🔹 Pourquoi vérifier ?
     *    • GetCurrent() retourne nullptr si aucune scène n'est chargée
     *    • Évite les crashes par déréférencement de pointeur nul
     *    • Permet un fallback ou un logging d'erreur gracieux
     * 
     * @note HasScene() est plus rapide que GetCurrent() != nullptr car
     *       il évite le retour de pointeur — à privilégier dans les guards.
     */
    if (!mgr.HasScene()) {
        logger.Warning("[AccessScript] Aucune scène active — opération ignorée\n");
        return;
    }

    // ── Accès typé au script de scène ───────────────────────────────
    /**
     * @brief Récupère un pointeur vers le script avec type spécifique.
     * @tparam T Type du script attendu (doit dériver de NkSceneScript).
     * @return Pointeur vers T si le script existe et correspond, nullptr sinon.
     * 
     * 🔹 Comportement :
     *    • Retourne nullptr si la scène n'a pas de script attaché
     *    • Retourne nullptr si le script attaché n'est PAS de type T
     *    • Effectue un static_cast — aucun runtime check de type supplémentaire
     * 
     * @warning Si le type ne correspond pas, le comportement est indéfini.
     *          Toujours vérifier le résultat != nullptr avant utilisation.
     * 
     * @example
     * @code
     * if (auto* gm = mgr.GetCurrent()->GetScript<GameLevelScript>()) {
     *     gm->SetDifficulty(3);  // Modification safe via pointeur valide
     * } else {
     *     logger.Warning("Script GameLevelScript non trouvé sur la scène courante\n");
     * }
     * @endcode
     */
    GameLevelScript* gm = mgr.GetCurrent()->GetScript<GameLevelScript>();
    if (gm) {
        // Le script est présent et du bon type — interaction safe
        // gm->SetDifficulty(3);  // Exemple : augmenter la difficulté en debug
        // gm->TriggerEvent("Debug_SpawnBoss");  // Exemple : déclencher un événement
    } else {
        // Script absent ou de type différent — logging pour debug
        logger.Warning("[AccessScript] GameLevelScript non trouvé sur la scène '%s'\n",
                       mgr.GetCurrentName().CStr());
    }

    // ── Accès non-typé : juste vérifier la présence d'un script ─────
    /**
     * @brief Vérifie si la scène a un script attaché, quel que soit son type.
     * @return true si un script est présent, false sinon.
     * 
     * 🔹 Usage typique :
     *    • Logging/debug : afficher l'état de la scène dans l'UI dev
     *    • Conditions générales : "si la scène a un script, faire X"
     *    • Éviter d'appeler des méthodes sur un script potentiellement absent
     * 
     * @note Ne donne aucune information sur le type du script — utiliser
     *       GetScript<T>() si vous avez besoin d'interagir avec des méthodes
     *       spécifiques au type.
     */
    if (mgr.GetCurrent()->HasScript()) {
        logger.Infof("[AccessScript] Scène '%s' a un script attaché\n",
                     mgr.GetCurrentName().CStr());
        // Interaction générique possible via l'interface de base NkSceneScript
        // (mais limitée aux méthodes virtuelles de l'interface)
    }

    // ── Pattern avancé : accès via Application singleton ────────────
    /**
     * @brief Accéder au script depuis n'importe où dans l'application.
     * 
     * 🔹 Pourquoi un singleton Application ?
     *    • Point d'accès global cohérent au SceneManager
     *    • Évite de passer des références partout dans le code
     *    • Facilite les tests via injection de mock
     * 
     * @warning Les singletons peuvent compliquer les tests et créer
     *          des dépendances cachées — à utiliser avec discernement.
     */
    /*
    // Dans n'importe quelle fonction, même hors des systèmes ECS :
    auto& app = Application::Get();
    auto& mgr = app.GetSceneManager();
    
    if (mgr.HasScene()) {
        if (auto* script = mgr.GetCurrent()->GetScript<GameLevelScript>()) {
            script->SpawnEnemyWave(5);  // Interaction directe avec la logique métier
        }
    }
    */

    // ── Pattern sécurisé : wrapper avec fallback ────────────────────
    /**
     * @brief Fonction helper pour accéder au script avec gestion d'erreur.
     * @tparam T Type du script attendu.
     * @param mgr Référence au SceneManager.
     * @param onFound Callback à exécuter si le script est trouvé.
     * @param onNotFound Callback optionnel si le script n'est pas trouvé.
     * 
     * 🔹 Avantages :
     *    • Centralise la logique de vérification nullptr
     *    • Permet un fallback personnalisé (logging, erreur UI, etc.)
     *    • Réduit la duplication de code dans l'application
     * 
     * @example
     * @code
     * WithSceneScript<GameLevelScript>(mgr,
     *     [](GameLevelScript* script) {
     *         script->TriggerVictory();  // Callback si trouvé
     *     },
     *     []() {
     *         logger.Error("Impossible de déclencher la victoire — script non trouvé\n");
     *     }
     * );
     * @endcode
     */
    /*
    template<typename T>
    void WithSceneScript(NkSceneManager& mgr,
                         NkFunction<void(T*)> onFound,
                         NkFunction<void()> onNotFound = nullptr) {
        if (!mgr.HasScene()) {
            if (onNotFound) onNotFound();
            return;
        }
        if (auto* script = mgr.GetCurrent()->GetScript<T>()) {
            onFound(script);
        } else if (onNotFound) {
            onNotFound();
        }
    }
    */
}

/** @} */ // end of ScriptAccess_Examples group


// =============================================================================
// SECTION 6 : INTÉGRATION DANS UN ÉDITEUR (EDITORLAYER PATTERN)
// =============================================================================
/**
 * @addtogroup EditorIntegration_Examples
 * @{
 */

/**
 * @example 07_editor_layer_integration.cpp
 * @brief Intégration du système de scènes dans un éditeur de niveau.
 * 
 * 🔹 Objectifs d'un EditorLayer :
 *    • Afficher la scène courante dans un viewport éditable
 *    • Permettre de lancer/arrêter le mode "Play" (BeginPlay/EndPlay)
 *    • Changer de scène via un asset browser ou un dropdown
 *    • Modifier les propriétés des entités en temps réel (inspecteur)
 * 
 * 🔹 Défis spécifiques :
 *    • Le scheduler ECS est conçu pour le runtime — en mode édition,
 *      on veut souvent un contrôle manuel des mises à jour
 *    • Les modifications en mode édition ne doivent pas persister
 *      dans la scène runtime sauf explicitement sauvegardées
 *    • L'UI de l'éditeur et la scène de jeu peuvent coexister —
 *      gestion des inputs et du rendu en couches
 * 
 * @note Cet exemple illustre un pattern courant mais n'est pas une
 *       implémentation complète — à adapter à votre architecture d'éditeur.
 */
class EditorLayer /* : public Layer */ {
    public:
        /**
         * @brief Initialisation de l'EditorLayer.
         * @param world Référence au monde ECS partagé.
         * @param scheduler Référence au scheduler pour enregistrement des systèmes.
         */
        EditorLayer(NkWorld& world, NkScheduler& scheduler)
            : mWorld(world)
            , mScheduler(scheduler)
            , mSceneManager(world)
            , mPlaying(false)
            , mCurrentSceneName("MainMenu") {
            
            // Enregistrement des scènes disponibles dans l'éditeur
            SetupSceneRegistry();
            
            // Configuration initiale : charger le menu en mode édition
            LoadSceneInEditor("MainMenu");
        }

        /**
         * @brief Mise à jour de l'EditorLayer — appelée chaque frame.
         * @param dt Delta-time en secondes.
         * 
         * 🔹 Comportement selon le mode :
         *    • Mode Édition (mPlaying=false) : pas de mise à jour gameplay,
         *      juste l'UI de l'éditeur et le viewport interactif
         *    • Mode Play (mPlaying=true) : mise à jour manuelle de la scène
         *      car le scheduler ECS peut être désactivé en mode édition
         * 
         * @note En mode Play, on appelle directement Tick() sur la scène
         *       plutôt que de passer par le scheduler — plus de contrôle,
         *       possibilité de pause/step frame par frame pour le debug.
         */
        void OnUpdate(float dt) override {
            if (mPlaying && mSceneManager.HasScene()) {
                // ── Mode Play : mise à jour manuelle de la scène ─────
                // Alternative : réactiver le scheduler et le laisser faire
                // Mais le contrôle manuel permet le frame-stepping pour debug
                
                auto* scene = mSceneManager.GetCurrent();
                
                // Mise à jour dans l'ordre logique : Fixed → Tick → Late
                scene->FixedTick(mFixedDeltaTime);  // Physique déterministe
                scene->Tick(dt);                    // Logique gameplay
                scene->LateTick(dt);                // Post-traitement
                
                // Mise à jour du rendu de la scène dans le viewport
                RenderSceneInViewport(scene);
                
            } else {
                // ── Mode Édition : pas de mise à jour gameplay ───────
                // Juste l'UI de l'éditeur et le viewport interactif
                
                UpdateEditorUI();          // Inspecteur, hiérarchie, assets
                UpdateViewportInteraction();  // Sélection, déplacement d'entités
                RenderEditorOverlay();     // Grid, gizmos, labels
            }
        }

        /**
         * @brief Bouton "Play" : lance la scène courante en mode runtime.
         * 
         * 🔹 Étapes :
         *    1. Sauvegarder l'état actuel de la scène (pour restauration au Stop)
         *    2. Charger la scène via SceneManager (appelle BeginPlay)
         *    3. Activer le flag mPlaying pour basculer en mode runtime
         *    4. Optionnel : cacher l'UI de l'éditeur pour immersion
         * 
         * @note La scène est chargée avec une transition Instant() car
         *       on est déjà dans l'éditeur — pas besoin d'animation.
         */
        void OnPlay() {
            if (mPlaying) {
                return;  // Déjà en mode play
            }
            
            // Sauvegarde de l'état édition pour restauration au Stop
            SaveEditorState();
            
            // Chargement de la scène sélectionnée
            if (mSceneManager.LoadScene(mCurrentSceneName,
                                       NkSceneTransition::Instant())) {
                mPlaying = true;
                logger.Info("[Editor] Mode Play activé — scène: %s\n",
                            mCurrentSceneName.CStr());
                
                // Optionnel : cacher l'UI de l'éditeur
                // SetEditorUIVisible(false);
            } else {
                logger.Error("[Editor] Échec du chargement de la scène '%s'\n",
                             mCurrentSceneName.CStr());
                ShowErrorDialog("Impossible de charger la scène");
            }
        }

        /**
         * @brief Bouton "Stop" : arrête le mode runtime et restaure l'édition.
         * 
         * 🔹 Étapes :
         *    1. Appeler EndPlay() sur la scène courante (nettoyage script)
         *    2. Désactiver le flag mPlaying pour revenir en mode édition
         *    3. Restaurer l'état de la scène tel qu'en édition
         *    4. Optionnel : réafficher l'UI de l'éditeur
         * 
         * @warning Ne pas delete la scène — elle est gérée par le SceneManager.
         *          On veut juste revenir à l'état d'édition, pas détruire.
         */
        void OnStop() {
            if (!mPlaying) {
                return;  // Déjà en mode édition
            }
            
            // Nettoyage de la scène runtime
            if (mSceneManager.HasScene()) {
                mSceneManager.GetCurrent()->EndPlay();
                // Note : on ne décharge pas la scène, on la "gèle" en l'état
            }
            
            mPlaying = false;
            logger.Info("[Editor] Mode Play désactivé — retour à l'édition\n");
            
            // Restauration de l'état édition
            RestoreEditorState();
            
            // Optionnel : réafficher l'UI de l'éditeur
            // SetEditorUIVisible(true);
        }

        /**
         * @brief Change la scène courante en mode édition (sans Play).
         * @param sceneName Nom de la scène à charger dans l'éditeur.
         * 
         * 🔹 Différence avec LoadScene() runtime :
         *    • Pas d'appel à BeginPlay() — on veut juste visualiser la scène
         *    • Pas de script de scène actif — éviter la logique gameplay
         *    • Les entités sont spawnées mais "gelées" pour édition
         * 
         * @note Cette méthode est spécifique à l'éditeur — ne pas l'utiliser
         *       dans le code runtime du jeu.
         */
        void LoadSceneInEditor(const NkString& sceneName) {
            // Désactiver le mode play si actif
            if (mPlaying) {
                OnStop();
            }
            
            // Chargement de la scène via le manager (sans BeginPlay)
            // Note : implémentation spécifique à votre éditeur
            // Ici on suppose une méthode EditorLoadScene qui skip BeginPlay
            if (EditorLoadScene(sceneName)) {
                mCurrentSceneName = sceneName;
                logger.Info("[Editor] Scène chargée en mode édition: %s\n",
                            sceneName.CStr());
            } else {
                logger.Error("[Editor] Échec du chargement édition de '%s'\n",
                             sceneName.CStr());
            }
        }

        /**
         * @brief Affiche la liste des scènes disponibles dans l'UI de l'éditeur.
         * 
         * 🔹 Usage typique :
         *    • Dropdown dans la toolbar de l'éditeur pour changer de scène
         *    • Asset browser avec preview des scènes
         *    • Recherche/filtrage des scènes par nom ou tag
         * 
         * @note La liste est peuplée depuis le registre interne du SceneManager.
         *       Si ce registre n'est pas exposé, maintenir une copie locale.
         */
        void RenderSceneSelector() {
            // Exemple pseudo-code pour une UI ImGui-like :
            /*
            if (ImGui::BeginCombo("Scène courante", mCurrentSceneName.CStr())) {
                for (const auto& entry : mSceneManager.GetRegisteredScenes()) {
                    bool isSelected = (entry.name == mCurrentSceneName);
                    if (ImGui::Selectable(entry.name.CStr(), isSelected)) {
                        LoadSceneInEditor(entry.name);
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            */
        }

    private:
        // ── Références aux systèmes principaux ───────────────────────
        NkWorld& mWorld;              ///< Monde ECS partagé
        NkScheduler& mScheduler;      ///< Scheduler pour enregistrement systèmes
        NkSceneManager mSceneManager; ///< Gestionnaire de scènes (possédé)

        // ── État de l'éditeur ────────────────────────────────────────
        bool mPlaying;                      ///< Flag mode Play/Édition
        NkString mCurrentSceneName;         ///< Nom de la scène sélectionnée
        float mFixedDeltaTime = 1.f / 60.f; ///< Delta-time fixe pour FixedTick

        // ── Cache pour l'état édition (sauvegarde/restauration) ─────
        // À implémenter selon votre système de sérialisation
        struct EditorState {
            NkVector<EntityEditData> entities;  ///< Données éditables des entités
            NkString activeScene;               ///< Nom de la scène active
            // ... autres données d'état ...
        };
        EditorState mSavedEditorState;

        // ── Méthodes utilitaires privées ────────────────────────────
        void SetupSceneRegistry() {
            // Enregistrement des scènes disponibles dans l'éditeur
            // Identique à Example_RegisterScenes() mais avec des factories
            // qui peuvent inclure des éléments d'édition (gizmos, labels)
            Example_RegisterScenes(mSceneManager);
        }

        bool EditorLoadScene(const NkString& sceneName) {
            // Implémentation spécifique à l'éditeur :
            // • Charge la scène sans appeler BeginPlay()
            // • Spawn des entités avec des composants d'édition (Selectible, Gizmo)
            // • Configure le viewport pour afficher la scène
            // Retourne true si succès, false sinon
            return true;  // Placeholder
        }

        void SaveEditorState() {
            // Serialisation de l'état actuel de la scène en mode édition
            // Pour restauration exacte au retour du mode Play
            // mSavedEditorState = SerializeCurrentScene();
        }

        void RestoreEditorState() {
            // Désérialisation et application de l'état sauvegardé
            // Pour revenir exactement à l'état d'édition pré-Play
            // DeserializeScene(mSavedEditorState);
        }

        void RenderSceneInViewport(NkSceneGraph* scene) {
            // Rendu de la scène dans le viewport de l'éditeur
            // Avec overlays d'édition : grid, sélection, gizmos de transformation
            // renderer.RenderSceneWithEditorOverlays(scene);
        }

        void UpdateEditorUI() {
            // Mise à jour de l'UI de l'éditeur : inspecteur, hiérarchie, assets
            // inspectorPanel.Update();
            // hierarchyPanel.Update();
            // assetBrowser.Update();
        }

        void UpdateViewportInteraction() {
            // Gestion des inputs dans le viewport : sélection, déplacement, rotation
            // if (viewport.IsHovered()) {
            //     HandleSelectionInput();
            //     HandleTransformGizmos();
            // }
        }

        void RenderEditorOverlay() {
            // Rendu des overlays d'édition : grid, axes, labels, bounds
            // editorRenderer.DrawGrid();
            // editorRenderer.DrawSelectionBounds();
            // editorRenderer.DrawEntityLabels();
        }

        void ShowErrorDialog(const char* message) {
            // Affichage d'une modale d'erreur dans l'UI de l'éditeur
            // ImGui::OpenPopup("Erreur");
            // if (ImGui::BeginPopupModal("Erreur")) {
            //     ImGui::Text("%s", message);
            //     if (ImGui::Button("OK")) { ImGui::CloseCurrentPopup(); }
            //     ImGui::EndPopup();
            // }
        }
};

/** @} */ // end of EditorIntegration_Examples group


// =============================================================================
// SECTION 7 : BONNES PRATIQUES ET ANTI-PATTERNS
// =============================================================================
/**
 * @addtogroup BestPractices_Examples
 * @{
 */

/**
 * @example 08_best_practices_and_pitfalls.cpp
 * @brief Bonnes pratiques et pièges courants avec le système de scènes.
 * 
 * 🔹 Objectif de cette section :
 *    • Documenter les patterns qui fonctionnent bien en production
 *    • Identifier les anti-patterns qui causent des bugs ou des perf issues
 *    • Fournir des alternatives robustes pour les cas problématiques
 * 
 * @note Ces conseils sont basés sur l'expérience — adaptez-les à votre
 *       contexte spécifique (taille de l'équipe, type de jeu, contraintes).
 */

// =====================================================================
// ✅ BONNES PRATIQUES
// =====================================================================

// ── 1. Toujours vérifier les pointeurs avant utilisation ───────────
/**
 * @brief Pattern de guard explicite pour éviter les crashes.
 * 
 * 🔹 Pourquoi ?
 *    • GetWorld()/GetScene() peuvent retourner nullptr si le contexte
 *      n'a pas été injecté (ex: script utilisé hors d'une scène)
 *    • Un guard explicite rend le code plus robuste et plus lisible
 *    • Facilite le debugging : on sait exactement où l'échec se produit
 * 
 * @example
 * @code
 * void SafeContextUsage(NkSceneScript* script) {
 *     if (!script) {
 *         logger.Error("Script null — opération ignorée\n");
 *         return;
 *     }
 *     
 *     if (auto* world = script->GetWorld()) {  // ✅ Guard explicite
 *         // Utiliser world en toute sécurité
 *         world->Query<Health>().ForEach([](auto& h) { /* ... */ });
 *     } else {
 *         logger.Warning("Contexte monde non injecté — skip\n");
 *     }
 * }
 * @endcode
 */
void Example_SafePointerChecks() {
    // Implémentation illustrative — voir commentaire ci-dessus
}

// ── 2. Mettre en cache les résultats de Find* pour éviter les queries répétées ─
/**
 * @brief Pattern de caching avec invalidation pour les recherches coûteuses.
 * 
 * 🔹 Pourquoi ?
 *    • FindGameObjectsWithTag/ByName effectuent des queries ECS O(n)
 *    • Les appeler dans OnTick() chaque frame peut devenir très coûteux
 *    • Un cache avec invalidation réduit le coût à O(1) la plupart du temps
 * 
 * 🔹 Quand invalider le cache ?
 *    • Quand une entité avec le tag recherché est spawnée/détruite
 *    • Quand le nom d'une entité est modifié
 *    • Périodiquement si la scène est très dynamique (ex: toutes les 30 frames)
 * 
 * @example
 * @code
 * class CachedScript : public NkSceneScript {
 *     NkVector<NkGameObject> mCachedEnemies;  // Cache membre
 *     bool mEnemiesCacheValid = false;
 * 
 * public:
 *     void OnTick(float dt) noexcept override {
 *         // Utiliser le cache si valide, sinon rebuild
 *         const auto& enemies = GetEnemiesCached();
 *         for (const auto& enemy : enemies) {
 *             UpdateEnemy(enemy, dt);
 *         }
 *     }
 * 
 *     void OnEnemySpawned() {
 *         // Invalider le cache quand un nouvel ennemi apparaît
 *         mEnemiesCacheValid = false;
 *     }
 * 
 * private:
 *     const NkVector<NkGameObject>& GetEnemiesCached() {
 *         if (!mEnemiesCacheValid) {
 *             mCachedEnemies = FindGameObjectsWithTag("Enemy");  // Query O(n)
 *             mEnemiesCacheValid = true;
 *         }
 *         return mCachedEnemies;  // Retour par const ref — pas de copie
 *     }
 * };
 * @endcode
 */
void Example_CachingPattern() {
    // Implémentation illustrative — voir commentaire ci-dessus
}

// ── 3. Utiliser OnEndPlay() pour le nettoyage garanti ──────────────
/**
 * @brief Pattern RAII pour les ressources acquises dans OnBeginPlay().
 * 
 * 🔹 Pourquoi ?
 *    • OnEndPlay() est TOUJOURS appelé avant la destruction de la scène
 *    • Même en cas d'exception, de crash contrôlé, ou de changement de scène
 *    • Garantit que les ressources ne fuient pas et que l'état est propre
 * 
 * 🔹 Quoi nettoyer ?
 *    • Assets chargés spécifiquement pour la scène (textures, sons)
 *    • Souscriptions aux événements globaux (pour éviter des callbacks dangling)
 *    • Timers, coroutines, ou autres systèmes asynchrones
 *    • Pointeurs vers des entités qui vont être détruites
 * 
 * @example
 * @code
 * class ResourceManagingScript : public NkSceneScript {
 *     AssetHandle mLoadedAsset;
 *     EventSubscriptionId mGlobalEventSub;
 * 
 * public:
 *     void OnBeginPlay() noexcept override {
 *         mLoadedAsset = LoadHeavyAsset("level_data");  // Acquisition
 *         mGlobalEventSub = EventBus::Subscribe<GlobalEvent>(/* ... */);
 *     }
 * 
 *     void OnEndPlay() noexcept override {
 *         UnloadAsset(mLoadedAsset);  // ✅ Libération garantie
 *         EventBus::Unsubscribe<GlobalEvent>(mGlobalEventSub);
 *         mLoadedAsset = AssetHandle::Null();
 *         mGlobalEventSub = EventSubscriptionId::Invalid();
 *     }
 * };
 * @endcode
 */
void Example_ResourceCleanup() {
    // Implémentation illustrative — voir commentaire ci-dessus
}

// ── 4. Préférer DestroyGameObject() ou world.DestroyDeferred() selon le contexte ─
/**
 * @brief Choisir la bonne méthode de destruction selon le contexte d'appel.
 * 
 * 🔹 Règle générale :
 *    • HORS d'une query ECS : DestroyGameObject() ou world.Destroy() est OK
 *    • DANS une query ECS : TOUJOURS utiliser world.DestroyDeferred()
 * 
 * 🔹 Pourquoi DestroyDeferred() dans une query ?
 *    • Une query itère sur les entités via des pointeurs internes
 *    • Détruire une entité pendant l'itération invalide ces pointeurs
 *    • DestroyDeferred() marque l'entité pour destruction AFTER la query
 * 
 * @example
 * @code
 * void SafeDestructionExample(NkSceneScript* script) {
 *     if (!script || !script->GetWorld()) return;
 *     auto* world = script->GetWorld();
 * 
 *     // ❌ DANS une query : utiliser DestroyDeferred
 *     world->Query<HealthComponent>().ForEach([world](NkEntityId id, HealthComponent& h) {
 *         if (h.value <= 0) {
 *             world->DestroyDeferred(id);  // ✅ Sécurisé — destruction après la query
 *         }
 *     });
 *     
 *     // ✅ HORS d'une query : DestroyGameObject est OK
 *     auto target = script->FindGameObjectByName("Boss");
 *     if (target.IsValid()) {
 *         script->DestroyGameObject(target);  // ✅ Immédiat et sûr ici
 *     }
 * }
 * @endcode
 */
void Example_SafeDestruction() {
    // Implémentation illustrative — voir commentaire ci-dessus
}

// ── 5. Documenter l'ordre d'appel attendu des callbacks lifecycle ─
/**
 * @brief Utiliser des commentaires Doxygen pour clarifier le lifecycle.
 * 
 * 🔹 Pourquoi documenter l'ordre ?
 *    • Les nouveaux développeurs comprennent rapidement le flux d'exécution
 *    • Facilite le debugging : on sait dans quel callback chercher un bug
 *    • Aide à décider où placer quelle logique (ex: physique → FixedTick)
 * 
 * 🔹 Ordre typique à documenter :
 *    • BeginPlay → (FixedTick)* → Tick → LateTick → EndPlay
 *    • Pause/Resume peuvent survenir à tout moment après BeginPlay
 * 
 * @example
 * @code
 * /// @lifecycle Order: BeginPlay → (FixedTick)* → Tick → LateTick → EndPlay
 * /// @thread MainThread — tous les callbacks sont appelés depuis le thread principal
 * class WellDocumentedScript : public NkSceneScript {
 *     // ... implémentation ...
 * };
 * @endcode
 */
void Example_LifecycleDocumentation() {
    // Implémentation illustrative — voir commentaire ci-dessus
}


// =====================================================================
// ❌ ANTI-PATTERNS À ÉVITER
// =====================================================================

// ── 1. Appeler les méthodes lifecycle manuellement ─────────────────
/**
 * @brief Anti-pattern : appeler OnTick() directement au lieu de laisser le scheduler gérer.
 * 
 * 🔹 Pourquoi c'est mauvais ?
 *    • Brise l'encapsulation du système de scènes
 *    • Risque d'appels multiples ou hors ordre (ex: Tick avant BeginPlay)
 *    • Rend le code plus difficile à tester et à maintenir
 * 
 * ✅ Alternative : laisser NkSceneLifecycleSystem appeler les callbacks
 *    via le scheduler — c'est son rôle exclusif.
 * 
 * @code
 * // ❌ À ÉVITER :
 * myScript->OnTick(dt);  // Appel manuel — risque d'incohérence d'état
 * 
 * // ✅ FAIRE :
 * // Laisser le scheduler appeler OnTick() automatiquement
 * // Si besoin de forcer une mise à jour (debug), utiliser une méthode dédiée :
 * // myScript->DebugForceTick(dt);  // Méthode explicite, documentée, contrôlée
 * @endcode
 */
void Example_AvoidManualLifecycleCalls() {
    // Placeholder — voir commentaire ci-dessus
}

// ── 2. Stocker des références aux résultats de Find* au-delà de leur validité ─
/**
 * @brief Anti-pattern : conserver un pointeur vers une entité trouvée sans vérifier sa validité.
 * 
 * 🔹 Pourquoi c'est dangereux ?
 *    • Une entité peut être détruite entre le Find* et l'utilisation du pointeur
 *    • Le pointeur devient dangling — déréférencement = crash ou corruption
 *    • Difficile à debugger car le bug peut apparaître loin du Find* original
 * 
 * ✅ Alternatives :
 *    • Toujours vérifier IsValid() avant d'utiliser un NkGameObject
 *    • Utiliser un cache avec invalidation (voir bonne pratique #2)
 *    • Stocker l'ID (NkEntityId) plutôt qu'un pointeur — plus robuste
 * 
 * @code
 * // ❌ À ÉVITER :
 * auto& enemies = FindGameObjectsWithTag("Enemy");  // Référence locale
 * // ... plus tard, après un Spawn/Destroy potentiel ...
 * enemies[0].DoSomething();  // ❌ enemies[0] peut être dangling !
 * 
 * // ✅ FAIRE :
 * auto enemies = FindGameObjectsWithTag("Enemy");  // Copie du vecteur
 * for (auto& enemy : enemies) {
 *     if (enemy.IsValid()) {  // ✅ Guard avant utilisation
 *         enemy.DoSomething();
 *     }
 * }
 * @endcode
 */
void Example_AvoidDanglingReferences() {
    // Placeholder — voir commentaire ci-dessus
}

// ── 3. Oublier de vérifier IsValid() sur les GameObjects retournés ─
/**
 * @brief Anti-pattern : utiliser un NkGameObject sans vérifier sa validité.
 * 
 * 🔹 Pourquoi c'est risqué ?
 *    • FindGameObjectByName/SpawnPrefab peuvent retourner un handle invalide
 *    • Appeler des méthodes sur un handle invalide = comportement indéfini
 *    • Peut causer des crashes silencieux ou des corruptions de mémoire
 * 
 * ✅ Alternative : toujours guarder avec IsValid() avant utilisation.
 * 
 * @code
 * // ❌ À ÉVITER :
 * auto obj = FindGameObjectByName("Missing");
 * obj.DoSomething();  // ❌ Crash si obj est invalide
 * 
 * // ✅ FAIRE :
 * auto obj = FindGameObjectByName("Missing");
 * if (obj.IsValid()) {  // ✅ Vérification explicite
 *     obj.DoSomething();
 * } else {
 *     logger.Warning("Objet 'Missing' non trouvé — opération ignorée\n");
 * }
 * @endcode
 */
void Example_AlwaysCheckIsValid() {
    // Placeholder — voir commentaire ci-dessus
}

// ── 4. Utiliser SpawnPrefab avec un prefab invalide ───────────────
/**
 * @brief Anti-pattern : spawn sans vérifier que le prefab est chargé.
 * 
 * 🔹 Pourquoi c'est problématique ?
 *    • SpawnPrefab() retourne un handle invalide si le prefab est null
 *    • Utiliser ce handle sans vérification = bug silencieux
 *    • Difficile à tracer car l'erreur se produit loin du SpawnPrefab
 * 
 * ✅ Alternative : toujours vérifier prefab.IsValid() avant spawn.
 * 
 * @code
 * // ❌ À ÉVITER :
 * NkPrefab nullPrefab;  // Non initialisé → invalide
 * auto go = SpawnPrefab(nullPrefab);  // Retourne un handle invalide
 * go.SetPosition(...);  // ❌ Comportement indéfini
 * 
 * // ✅ FAIRE :
 * if (myPrefab.IsValid()) {  // ✅ Vérification avant spawn
 *     auto go = SpawnPrefab(myPrefab, position);
 *     if (go.IsValid()) {  // ✅ Vérification après spawn (double guard)
 *         go.SetPosition(...);
 *     }
 * } else {
 *     logger.Error("Prefab non chargé — spawn ignoré\n");
 * }
 * @endcode
 */
void Example_ValidatePrefabBeforeSpawn() {
    // Placeholder — voir commentaire ci-dessus
}

// ── 5. Modifier l'état du monde dans OnEndPlay() sans FlushDeferred ─
/**
 * @brief Anti-pattern : destruction immédiate d'entités dans OnEndPlay().
 * 
 * 🔹 Pourquoi c'est risqué ?
 *    • D'autres systèmes peuvent encore itérer sur le monde après OnEndPlay()
 *    • Une destruction immédiate invalide ces itérations = crash potentiel
 *    • L'ordre d'exécution des systèmes n'est pas toujours évident
 * 
 * ✅ Alternative : utiliser DestroyDeferred() + FlushDeferred() dans OnEndPlay().
 * 
 * @code
 * // ❌ À ÉVITER :
 * void OnEndPlay() override {
 *     GetWorld()->Destroy(someEntity);  // ❌ Destruction immédiate — risque d'itération pendante
 * }
 * 
 * // ✅ FAIRE :
 * void OnEndPlay() override {
 *     GetWorld()->DestroyDeferred(someEntity);  // ✅ Marqué pour destruction après les queries
 *     GetWorld()->FlushDeferred();  // ✅ Applique les destructions avant retour — safe
 * }
 * @endcode
 */
void Example_SafeCleanupInEndPlay() {
    // Placeholder — voir commentaire ci-dessus
}

/** @} */ // end of BestPractices_Examples group


// =============================================================================
// CONCLUSION ET RESSOURCES SUPPLÉMENTAIRES
// =============================================================================
/**
 * @mainpage Guide d'Utilisation du Système de Scènes NKRenderer
 * 
 * 🔹 Ce document couvre :
 *    • La définition et l'usage de NkSceneScript pour la logique métier
 *    • L'enregistrement et le chargement dynamique via NkSceneManager
 *    • L'intégration avec le scheduler ECS via NkSceneLifecycleSystem
 *    • Les patterns d'usage recommandés et les pièges à éviter
 * 
 * 🔹 Ressources complémentaires :
 *    • @ref NkSceneGraph — Documentation de la classe de scène de base
 *    • @ref NkSceneManager — Documentation du gestionnaire de chargement
 *    • @ref NkSceneLifecycleSystem — Documentation des systèmes ECS de lifecycle
 *    • @ref NkScheduler — Documentation du scheduler ECS pour l'ordonnancement
 * 
 * 🔹 Support et contribution :
 *    • Bugs/feature requests : ouvrir un ticket sur le tracker du projet
 *    • Questions d'usage : consulter le wiki ou poser une question sur le forum
 *    • Contributions : suivre les guidelines de contribution du dépôt
 * 
 * @version 1.0
 * @date 2026
 * @author nkentseu
 */