#pragma once
// =============================================================================
// Scene/NkSceneScript.h — v2 (interface unifiée, STL-free)
// =============================================================================
/**
 * @file NkSceneScript.h
 * @brief Interface de base pour les scripts de scène (logique de haut niveau).
 * 
 * Ce fichier définit l'interface NkSceneScript pour :
 * - Recevoir les callbacks du lifecycle de scène (BeginPlay, Tick, EndPlay, etc.)
 * - Accéder au contexte (NkWorld, NkSceneGraph) pour manipuler l'ECS
 * - Fournir des helpers utilitaires pour la recherche et le spawn d'entités
 * 
 * 🔹 PRINCIPE FONDAMENTAL : 0 ou 1 script par scène
 *    • 0 script : scène purement data-driven (assemblage d'entités/systèmes)
 *    • 1 script : logique métier spécifique au niveau (règles, victoire, spawns)
 * 
 * 🔹 POURQUOI UN SEUL SCRIPT ?
 *    • Évite les ambiguïtés d'ordre d'exécution entre plusieurs scripts
 *    • Clarifie la propriété : un seul point d'entrée pour la logique de scène
 *    • Si besoin de plusieurs comportements : utiliser des NkScriptComponent
 *      sur des entités dédiées (approche ECS-correcte)
 * 
 * 🔹 ACTIVATION AUTOMATIQUE :
 *    • NkSceneLifecycleSystem (enregistré dans NkScheduler) appelle
 *      automatiquement OnBeginPlay/OnTick/OnFixedTick/OnLateTick/OnEndPlay
 *    • L'utilisateur NE doit JAMAIS appeler ces méthodes manuellement
 * 
 * 🔹 Thread-safety : Non thread-safe. Toutes les méthodes doivent être
 *    appelées depuis le thread principal (game loop).
 * 
 * @author nkentseu
 * @version 2.0
 * @date 2026
 */

#include "NKECS/NkECSDefines.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {

    // Forward declarations pour éviter les inclusions circulaires
    namespace ecs {

        class NkWorld;
        class NkSceneGraph;
        class NkGameObject;
        struct NkTag;
        struct NkName;
        class NkPrefab;

        // =====================================================================
        // 🎭 NkSceneScript — Interface de base pour les scripts de scène
        // =====================================================================
        /**
         * @class NkSceneScript
         * @brief Interface à dériver pour implémenter la logique de haut niveau d'une scène.
         * 
         * 🔹 Rôle :
         *    • Point d'entrée unique pour la logique métier d'un niveau
         *    • Réception automatique des callbacks lifecycle via NkSceneLifecycleSystem
         *    • Accès contrôlé au contexte ECS (monde + scène) via getters protégés
         * 
         * 🔹 Pattern d'usage :
         * @code
         * class MyLevelScript : public NkSceneScript {
         * public:
         *     void OnBeginPlay() override {
         *         // Initialisation : spawn des ennemis, configuration UI, etc.
         *         SpawnEnemyWave(5);
         *     }
         * 
         *     void OnTick(float dt) override {
         *         // Logique gameplay principale : timers, événements, IA
         *         mElapsedTime += dt;
         *         if (mElapsedTime >= 30.f) {
         *             SpawnEnemyWave(3);
         *             mElapsedTime = 0.f;
         *         }
         *     }
         * 
         *     void OnEndPlay() override {
         *         // Nettoyage : libération de ressources, sauvegarde, etc.
         *         SavePlayerProgress();
         *     }
         * 
         * private:
         *     float mElapsedTime = 0.f;
         * 
         *     void SpawnEnemyWave(int count) {
         *         // Utilisation des helpers utilitaires
         *         for (int i = 0; i < count; ++i) {
         *             auto enemy = SpawnPrefab(mEnemyPrefab, GetRandomSpawnPoint());
         *             enemy.SetName(NkString::Format("Enemy_%d", i));
         *         }
         *     }
         * };
         * 
         * // Attachement dans la scène :
         * sceneGraph.SetScript<MyLevelScript>();
         * @endcode
         * 
         * 🔹 Helpers utilitaires fournis :
         *    • FindGameObjectsWithTag() : recherche par tag (NkTag component)
         *    • FindGameObjectByName() : recherche par nom exact (NkName component)
         *    • GetComponentInWorld<T>() : accès aux composants globaux du monde
         *    • SpawnPrefab() : instanciation d'un prefab avec position optionnelle
         *    • DestroyGameObject() : destruction sécurisée d'un GameObject
         * 
         * @warning Les méthodes de recherche (Find*) effectuent des queries ECS —
         *          éviter de les appeler dans des boucles chaudes (OnTick fréquent).
         *          Préférer mettre en cache les résultats si réutilisés.
         * 
         * @note Toutes les méthodes non-template sont implémentées dans NkSceneScript.cpp.
         *       Seules les méthodes template (GetComponentInWorld) restent dans le header.
         */
        class NkSceneScript {
            public:
                /// Destructeur virtuel pour suppression polymorphique sûre
                virtual ~NkSceneScript() noexcept;

                // ── Cycle de vie de la scène (appelés automatiquement) ─────────

                /**
                 * @brief Point d'entrée unique — appelé une fois au démarrage de la scène.
                 * 
                 * 🔹 Quand est-ce appelé ?
                 *    • Par NkSceneManager::LoadScene() après que tous les acteurs
                 *      sont spawnés et la hiérarchie établie
                 *    • Jamais plus d'une fois par instance de script
                 * 
                 * 🔹 Usage typique :
                 *    • Spawn initial des ennemis/NPCs
                 *    • Configuration de l'UI du niveau
                 *    • Initialisation des timers et états de jeu
                 *    • Chargement de données spécifiques au niveau
                 * 
                 * @note Méthode virtuelle avec implémentation vide — override optionnel.
                 */
                virtual void OnBeginPlay() noexcept;

                /**
                 * @brief Mise à jour principale — appelée chaque frame variable.
                 * @param dt Delta-time en secondes depuis la frame précédente.
                 * 
                 * 🔹 Quand est-ce appelé ?
                 *    • Par NkSceneLifecycleSystem::Update() dans la boucle principale
                 *    • Fréquence variable : dépend du framerate du jeu
                 * 
                 * 🔹 Usage typique :
                 *    • Logique gameplay dépendante du temps (mouvements, timers)
                 *    • Input handling, AI updates, game state transitions
                 *    • Émission d'événements gameplay via NkGameplayEventBus
                 * 
                 * @note Le paramètre dt permet d'écrire du code frame-independent :
                 *       position += velocity * dt;  // ✅ Correct
                 *       position += velocity;        // ❌ Dépend du framerate
                 */
                virtual void OnTick(float dt) noexcept;

                /**
                 * @brief Mise à jour tardive — appelée après OnTick(), avant le rendu.
                 * @param dt Delta-time en secondes.
                 * 
                 * 🔹 Quand est-ce appelé ?
                 *    • Par NkSceneLifecycleSystem::LateUpdate() après tous les OnTick()
                 * 
                 * 🔹 Usage typique :
                 *    • Mise à jour des caméras (après que les joueurs ont bougé)
                 *    • Calculs d'UI dépendants des positions finales des objets
                 *    • Post-traitement des données de OnTick()
                 * 
                 * @note Idéal pour les calculs qui doivent voir l'état "final" de la frame.
                 */
                virtual void OnLateTick(float dt) noexcept;

                /**
                 * @brief Mise à jour fixe — appelée à intervalle régulier pour la physique.
                 * @param fixedDt Delta-time fixe (ex: 1/60s) indépendant du framerate.
                 * 
                 * 🔹 Quand est-ce appelé ?
                 *    • Par NkSceneLifecycleSystem::FixedUpdate() à fréquence constante
                 *    • Peut être appelé 0, 1 ou plusieurs fois par frame selon le lag
                 * 
                 * 🔹 Usage typique :
                 *    • Physique déterministe (collisions, forces, ragdolls)
                 *    • Logique réseau tick-based pour la synchronisation
                 *    • Tout calcul qui doit être frame-independent
                 * 
                 * @warning Le paramètre fixedDt est CONSTANT — ne pas le multiplier
                 *          par un facteur variable sous peine de casser la physique.
                 */
                virtual void OnFixedTick(float fixedDt) noexcept;

                /**
                 * @brief Point de sortie — appelé avant le déchargement de la scène.
                 * 
                 * 🔹 Quand est-ce appelé ?
                 *    • Par NkSceneManager::UnloadScene() avant destruction
                 *    • Par le destructeur de NkSceneGraph en fallback
                 * 
                 * 🔹 Usage typique :
                 *    • Libération de ressources acquises dans OnBeginPlay()
                 *    • Sauvegarde de l'état de la scène/progression joueur
                 *    • Nettoyage des listeners/events souscrits
                 *    • Logging de statistiques de fin de niveau
                 * 
                 * @note Garantir que cette méthode est noexcept pour éviter
                 *       les terminaisons brutales lors du déchargement.
                 */
                virtual void OnEndPlay() noexcept;

                /**
                 * @brief Notification de pause — appelé quand la scène est mise en pause.
                 * 
                 * 🔹 Usage typique :
                 *    • Stopper les timers gameplay (mais pas l'UI)
                 *    • Mettre en sourdine l'audio gameplay (garder l'UI audible)
                 *    • Sauvegarder l'état temporaire pour reprise
                 * 
                 * @note La pause n'affecte PAS le rendu — la scène reste visible
                 *       sauf si explicitement cachée via NkSceneGraph::SetVisible().
                 */
                virtual void OnPause() noexcept;

                /**
                 * @brief Notification de reprise — appelé quand la scène est reprise.
                 * 
                 * 🔹 Usage typique :
                 *    • Reprendre les timers mis en pause
                 *    • Re-synchroniser les états après une pause longue
                 *    • Rejouer des effets audio/visuels interrompus
                 * 
                 * @note OnResume() peut être appelé même si OnPause() ne l'a pas été
                 *       (ex: reprise après chargement d'une sauvegarde).
                 */
                virtual void OnResume() noexcept;

                // ── Injection du contexte (appelé par NkSceneGraph::SetScript) ─

                /**
                 * @brief Injecte les pointeurs vers le monde et la scène.
                 * @param world Pointeur vers le NkWorld contenant cette scène.
                 * @param scene Pointeur vers le NkSceneGraph gérant cette scène.
                 * 
                 * @note Méthode appelée automatiquement par NkSceneGraph::SetScript<T>().
                 *       Ne pas appeler manuellement — risque de corruption d'état.
                 * 
                 * @warning Les pointeurs sont stockés en membre — la durée de vie
                 *          du script doit être inférieure ou égale à celle du monde.
                 */
                void SetContext(NkWorld* world, NkSceneGraph* scene) noexcept;

                // ── Accès au contexte (getters pour les classes dérivées) ─────

                /**
                 * @brief Retourne un pointeur vers le monde ECS associé.
                 * @return NkWorld* ou nullptr si le contexte n'a pas été injecté.
                 * 
                 * @note Utile pour :
                 *    • Accès direct à l'API ECS bas-niveau (Query, Add, Remove...)
                 *    • Émission d'événements via le bus du monde
                 *    • Manipulation d'entités en dehors des helpers utilitaires
                 * 
                 * @warning Retourne nullptr si SetContext() n'a pas encore été appelé.
                 *          Toujours vérifier le résultat avant déréférencement.
                 */
                [[nodiscard]] NkWorld* GetWorld() const noexcept;

                /**
                 * @brief Retourne un pointeur vers la scène graphique associée.
                 * @return NkSceneGraph* ou nullptr si le contexte n'a pas été injecté.
                 * 
                 * @note Utile pour :
                 *    • Spawn d'acteurs via la sémantique "scène" (SpawnActor, etc.)
                 *    • Recherche hiérarchique (FindByName, FindByLayer)
                 *    • Contrôle de la hiérarchie (SetParent, DestroyRecursive)
                 * 
                 * @warning Même avertissement que GetWorld() — vérifier nullptr.
                 */
                [[nodiscard]] NkSceneGraph* GetScene() const noexcept;

                // ── Helpers utilitaires pour la manipulation d'entités ────────

                /**
                 * @brief Trouve tous les GameObjects ayant un tag spécifique.
                 * @param tag Nom du tag à rechercher (comparaison case-sensitive).
                 * @return NkVector<NkGameObject> contenant les entités trouvées.
                 * 
                 * 🔹 Complexité : O(n) où n = nombre d'entités avec NkTag.
                 *          À éviter dans les boucles chaudes — préférer un cache.
                 * 
                 * 🔹 Usage typique :
                 *    • Trouver tous les ennemis : FindGameObjectsWithTag("Enemy")
                 *    • Trouver tous les checkpoints : FindGameObjectsWithTag("Checkpoint")
                 *    • Appliquer un effet de zone à un groupe d'entités taguées
                 * 
                 * @note Retourne un NkVector vide si :
                 *    • Le contexte n'est pas injecté (GetWorld() == nullptr)
                 *    • Aucune entité ne possède le tag recherché
                 * 
                 * @example
                 * @code
                 * // Dans OnTick : appliquer des dégâts de zone aux ennemis proches
                 * auto enemies = FindGameObjectsWithTag("Enemy");
                 * for (const auto& enemy : enemies) {
                 *     if (Distance(enemy.GetPosition(), mExplosionPos) < mRadius) {
                 *         enemy.TakeDamage(mDamageAmount);
                 *     }
                 * }
                 * @endcode
                 */
                [[nodiscard]] NkVector<NkGameObject>
                FindGameObjectsWithTag(const char* tag) const;

                /**
                 * @brief Trouve le premier GameObject dont le nom correspond exactement.
                 * @param name Nom à rechercher (comparaison case-sensitive via strcmp).
                 * @return NkGameObject trouvé, ou handle invalide si non trouvé.
                 * 
                 * 🔹 Complexité : O(n) où n = nombre d'entités avec NkName.
                 *          À éviter dans les boucles chaudes — préférer un cache.
                 * 
                 * 🔹 Comportement :
                 *    • Retourne le PREMIER match trouvé (ordre d'itération ECS non garanti)
                 *    • Si plusieurs entités ont le même nom, seule la première est retournée
                 *    • Pour des noms uniques, envisager une contrainte au niveau métier
                 * 
                 * @note Retourne un handle invalide (IsValid() == false) si :
                 *    • Le contexte n'est pas injecté
                 *    • Aucune entité ne correspond au nom recherché
                 * 
                 * @example
                 * @code
                 * // Trouver un point de spawn spécifique
                 * auto spawnPoint = FindGameObjectByName("Boss_SpawnPoint");
                 * if (spawnPoint.IsValid()) {
                 *     mBoss.SetPosition(spawnPoint.GetPosition());
                 * }
                 * @endcode
                 */
                [[nodiscard]] NkGameObject
                FindGameObjectByName(const char* name) const;

                /**
                 * @brief Récupère un composant global du monde (singleton ECS).
                 * @tparam T Type du composant global à récupérer.
                 * @return Pointeur vers T, ou nullptr si absent ou contexte invalide.
                 * 
                 * 🔹 Qu'est-ce qu'un composant global ?
                 *    • Un composant attaché à une entité "racine" du monde
                 *    • Typiquement utilisé pour : GameManager, AudioManager, SaveSystem
                 *    • Accès via NkWorld::GetGlobalComponent<T>() en interne
                 * 
                 * @note Cette méthode est template — l'implémentation reste dans le header
                 *       car requise pour l'instantiation à chaque site d'appel.
                 * 
                 * @example
                 * @code
                 * // Accéder au GameManager depuis un script de scène
                 * if (auto* gameMgr = GetComponentInWorld<GameManager>()) {
                 *     gameMgr->AddScore(100);
                 * }
                 * @endcode
                 */
                template<typename T>
                [[nodiscard]] T* GetComponentInWorld() const;

                /**
                 * @brief Instancie un prefab dans la scène avec position optionnelle.
                 * @param prefab Référence au prefab à instancier.
                 * @param position Position mondiale initiale (par défaut : origine).
                 * @param name Nom optionnel pour l'instance créée (par défaut : "Spawned").
                 * @return NkGameObject de l'instance créée.
                 * 
                 * 🔹 Étapes :
                 *    1. Appelle prefab.Instantiate(*mWorld, name)
                 *    2. Si l'instance a un root component : applique la position
                 *    3. Retourne le GameObject pour configuration immédiate
                 * 
                 * @note Le prefab doit être valide et enregistré dans NkPrefabRegistry.
                 *       En cas d'erreur, retourne un GameObject invalide (IsValid() == false).
                 * 
                 * @example
                 * @code
                 * // Spawn d'un ennemi à une position aléatoire
                 * NkVec3 spawnPos = GetRandomSpawnPosition();
                 * auto enemy = SpawnPrefab(mOrcPrefab, spawnPos, "Orc_Warrior_01");
                 * if (enemy.IsValid()) {
                 *     enemy.AddComponent<Health>(100);  // Configuration post-spawn
                 * }
                 * @endcode
                 */
                [[nodiscard]] NkGameObject
                SpawnPrefab(const NkPrefab& prefab,
                            const NkMath::NkVec3f& position = {0.f, 0.f, 0.f},
                            const char* name = nullptr);

                /**
                 * @brief Détruit un GameObject de façon sécurisée.
                 * @param go Référence au GameObject à détruire.
                 * 
                 * 🔹 Comportement :
                 *    • Appelle go.Destroy() en interne
                 *    • La destruction est IMMÉDIATE — ne pas appeler pendant
                 *      l'itération sur une query ECS (préférer DestroyDeferred)
                 * 
                 * @note Après cet appel, le handle 'go' devient invalide.
                 *       Ne plus l'utiliser sans vérification IsValid().
                 * 
                 * @warning Pour une destruction sécurisée pendant l'itération,
                 *          utiliser directement world.DestroyDeferred(id) via GetWorld().
                 */
                void DestroyGameObject(NkGameObject& go);

            protected:
                // ── Membres protégés pour accès dans les classes dérivées ─────

                /// Pointeur vers le monde ECS (injecté par SetContext)
                NkWorld* mWorld;

                /// Pointeur vers la scène graphique (injecté par SetContext)
                NkSceneGraph* mScene;

            private:
                // ── Amitié pour injection du contexte ─────────────────────────
                friend class NkSceneGraph;  ///< NkSceneGraph appelle SetContext()
        };

        // =====================================================================
        // 📦 Implémentations inline des templates (requis pour instantiation)
        // =====================================================================

        /**
         * @brief Implémentation de GetComponentInWorld<T>().
         * @note Doit rester dans le header car méthode template.
         */
        template<typename T>
        [[nodiscard]] T* NkSceneScript::GetComponentInWorld() const {
            return mWorld ? mWorld->GetGlobalComponent<T>() : nullptr;
        }

        // =====================================================================
        // 📚 EXEMPLES D'UTILISATION COMPLETS ET DÉTAILLÉS
        // =====================================================================
        /**
         * @addtogroup NkSceneScript_Examples
         * @{
         */

        /**
         * @example 01_basic_lifecycle.cpp
         * @brief Implémentation basique des callbacks lifecycle.
         * 
         * @code
         * #include "NKECS/Scene/NkSceneScript.h"
         * #include "MyGame/Components/Health.h"
         * 
         * using namespace nkentseu;
         * using namespace nkentseu::ecs;
         * 
         * class SimpleLevelScript : public NkSceneScript {
         * public:
         *     void OnBeginPlay() noexcept override {
         *         // Initialisation : afficher un message, spawn d'ennemis
         *         printf("Niveau démarré !\n");
         *         SpawnInitialEnemies();
         *     }
         * 
         *     void OnTick(float dt) noexcept override {
         *         // Logique principale : timer de survie
         *         mSurvivalTime += dt;
         *         
         *         // Vérifier condition de victoire toutes les secondes
         *         if (mSurvivalTime >= 60.f && !mVictoryTriggered) {
         *             TriggerVictory();
         *             mVictoryTriggered = true;
         *         }
         *     }
         * 
         *     void OnEndPlay() noexcept override {
         *         // Nettoyage : sauvegarder la progression
         *         printf("Niveau terminé - Temps de survie : %.1fs\n", mSurvivalTime);
         *         SaveProgress(mSurvivalTime);
         *     }
         * 
         * private:
         *     float mSurvivalTime = 0.f;
         *     bool mVictoryTriggered = false;
         * 
         *     void SpawnInitialEnemies() {
         *         // Utilisation de GetWorld() pour accès ECS bas-niveau
         *         if (auto* world = GetWorld()) {
         *             for (int i = 0; i < 5; ++i) {
         *                 auto enemy = world->CreateGameObject("BasicEnemy");
         *                 enemy.Add<Health>(50);
         *                 // ... configuration supplémentaire ...
         *             }
         *         }
         *     }
         * 
         *     void TriggerVictory() {
         *         // Utilisation de GetScene() pour opérations "scène"
         *         if (auto* scene = GetScene()) {
         *             // Afficher un message de victoire via UI
         *             auto victoryText = scene->SpawnActor<UIText>(
         *                 "VictoryMessage", 0, 200, 0
         *             );
         *             victoryText.SetText("VICTOIRE !");
         *         }
         *     }
         * 
         *     void SaveProgress(float time) {
         *         // Logique de sauvegarde simplifiée
         *         // En production : utiliser un SaveSystem global
         *     }
         * };
         * 
         * // Attachement dans la scène :
         * // sceneGraph.SetScript<SimpleLevelScript>();
         * @endcode
         */

        /**
         * @example 02_utility_helpers.cpp
         * @brief Utilisation des helpers utilitaires (Find*, SpawnPrefab, etc.).
         * 
         * @code
         * #include "NKECS/Scene/NkSceneScript.h"
         * #include "MyGame/Prefabs/EnemyPrefabs.h"
         * 
         * using namespace nkentseu;
         * using namespace nkentseu::ecs;
         * 
         * class WaveManagerScript : public NkSceneScript {
         * public:
         *     WaveManagerScript(int maxWaves)
         *         : mMaxWaves(maxWaves) {}
         * 
         *     void OnBeginPlay() noexcept override {
         *         // Charger les prefabs au démarrage
         *         mEnemyPrefab = LoadPrefab("Enemies/Orc_Warrior");
         *         mBossPrefab = LoadPrefab("Enemies/Boss_Dragon");
         *         
         *         // Démarrer la première vague après 3 secondes
         *         mWaveTimer = 3.f;
         *     }
         * 
         *     void OnTick(float dt) noexcept override {
         *         // Gestion du timer de vagues
         *         if (mCurrentWave <= mMaxWaves) {
         *             mWaveTimer -= dt;
         *             if (mWaveTimer <= 0.f) {
         *                 StartWave(mCurrentWave++);
         *                 mWaveTimer = mWaveInterval;
         *             }
         *         }
         *         
         *         // Vérifier si tous les ennemis de la vague sont morts
         *         CheckWaveCompletion();
         *     }
         * 
         * private:
         *     int mMaxWaves;
         *     int mCurrentWave = 1;
         *     float mWaveTimer = 0.f;
         *     float mWaveInterval = 30.f;  // 30 secondes entre les vagues
         *     NkPrefab mEnemyPrefab;
         *     NkPrefab mBossPrefab;
         * 
         *     void StartWave(int waveIndex) {
         *         printf("Vague %d démarrée !\n", waveIndex);
         *         
         *         // Spawn d'ennemis via SpawnPrefab helper
         *         int enemyCount = 3 + waveIndex * 2;  // Plus d'ennemis à chaque vague
         *         for (int i = 0; i < enemyCount; ++i) {
         *             NkVec3 spawnPos = GetRandomSpawnPosition();
         *             auto enemy = SpawnPrefab(
         *                 mEnemyPrefab,
         *                 spawnPos,
         *                 NkString::Format("Wave%d_Enemy%d", waveIndex, i).CStr()
         *             );
         *             
         *             // Configuration post-spawn via composants
         *             if (enemy.IsValid() && enemy.Has<Health>()) {
         *                 auto* health = enemy.Get<Health>();
         *                 health->maxValue += waveIndex * 10;  // Plus de PV à chaque vague
         *                 health->value = health->maxValue;
         *             }
         *         }
         *         
         *         // Boss à la dernière vague
         *         if (waveIndex == mMaxWaves) {
         *             SpawnBoss();
         *         }
         *     }
         * 
         *     void CheckWaveCompletion() {
         *         // Utilisation de FindGameObjectsWithTag pour vérifier les ennemis restants
         *         auto enemies = FindGameObjectsWithTag("Enemy");
         *         bool allDead = true;
         *         
         *         for (const auto& enemy : enemies) {
         *             if (enemy.IsValid() && enemy.Has<Health>()) {
         *                 if (enemy.Get<Health>()->value > 0) {
         *                     allDead = false;
         *                     break;
         *                 }
         *             }
         *         }
         *         
         *         if (allDead && mCurrentWave <= mMaxWaves) {
         *             // Toutes les vagues terminées → victoire
         *             if (mCurrentWave > mMaxWaves) {
         *                 OnVictory();
         *             }
         *         }
         *     }
         * 
         *     void SpawnBoss() {
         *         // Trouver le point de spawn du boss par nom
         *         auto spawnPoint = FindGameObjectByName("Boss_SpawnPoint");
         *         if (spawnPoint.IsValid()) {
         *             auto boss = SpawnPrefab(
         *                 mBossPrefab,
         *                 spawnPoint.GetPosition(),
         *                 "FinalBoss"
         *             );
         *             // Configuration spéciale du boss...
         *         }
         *     }
         * 
         *     NkVec3 GetRandomSpawnPosition() {
         *         // Logique simplifiée de position aléatoire
         *         return {
         *             NkRandom::Range(-10.f, 10.f),
         *             0.f,
         *             NkRandom::Range(-10.f, 10.f)
         *         };
         *     }
         * 
         *     void OnVictory() {
         *         printf("Toutes les vagues terminées — Victoire !\n");
         *         // Trigger victoire, récompenses, etc.
         *     }
         * 
         *     NkPrefab LoadPrefab(const char* path) {
         *         // Wrapper simplifié pour NkPrefabRegistry::Get()
         *         return NkPrefabRegistry::Global().Get(path);
         *     }
         * };
         * @endcode
         */

        /**
         * @example 03_pause_resume_handling.cpp
         * @brief Gestion de la pause/reprise dans un script de scène.
         * 
         * @code
         * #include "NKECS/Scene/NkSceneScript.h"
         * #include "MyGame/Systems/AudioManager.h"
         * 
         * using namespace nkentseu;
         * using namespace nkentseu::ecs;
         * 
         * class PauseAwareScript : public NkSceneScript {
         * public:
         *     void OnBeginPlay() noexcept override {
         *         // Initialiser le timer principal
         *         mGameTimer.Start();
         *         
         *         // Souscrire aux événements de pause globaux (optionnel)
         *         // via NkGameplayEventBus si nécessaire
         *     }
         * 
         *     void OnTick(float dt) noexcept override {
         *         // Ce code n'est JAMAIS exécuté quand la scène est en pause
         *         // grâce à NkSceneGraph::Pause() qui skip OnTick
         *         
         *         mGameTimer.Update(dt);  // Timer gameplay normal
         *         UpdateEnemies(dt);
         *         CheckTriggers();
         *     }
         * 
         *     void OnPause() noexcept override {
         *         // La scène vient d'être mise en pause
         *         printf("Jeu en pause\n");
         *         
         *         // 1. Stopper le timer gameplay (mais garder le temps écoulé)
         *         mGameTimer.Pause();
         *         
         *         // 2. Baisser le volume de l'audio gameplay (garder l'UI audible)
         *         AudioManager::SetCategoryVolume("Gameplay", 0.2f);
         *         AudioManager::SetCategoryVolume("UI", 1.0f);
         *         
         *         // 3. Afficher le menu de pause via UI
         *         ShowPauseMenu();
         *         
         *         // 4. Optionnel : sauvegarder l'état temporaire pour reprise rapide
         *         CacheGameState();
         *     }
         * 
         *     void OnResume() noexcept override {
         *         // La scène vient d'être reprise
         *         printf("Jeu repris\n");
         *         
         *         // 1. Reprendre le timer gameplay
         *         mGameTimer.Resume();
         *         
         *         // 2. Restaurer le volume audio
         *         AudioManager::SetCategoryVolume("Gameplay", 1.0f);
         *         
         *         // 3. Cacher le menu de pause
         *         HidePauseMenu();
         *         
         *         // 4. Optionnel : re-synchroniser les états après une pause longue
         *         ResyncGameState();
         *         
         *         // 5. Optionnel : effet de transition (fade-in audio, etc.)
         *         AudioManager::FadeIn("Gameplay", 0.3f);
         *     }
         * 
         *     void OnEndPlay() noexcept override {
         *         // Nettoyage garanti même si la scène est déchargée en pause
         *         mGameTimer.Stop();
         *         AudioManager::ResetCategoryVolumes();
         *     }
         * 
         * private:
         *     GameTimer mGameTimer;  // Timer avec support pause/resume
         * 
         *     void ShowPauseMenu() {
         *         // Spawn d'UI de pause via GetScene()
         *         if (auto* scene = GetScene()) {
         *             scene->SpawnActor<PauseMenuUI>("PauseOverlay");
         *         }
         *     }
         * 
         *     void HidePauseMenu() {
         *         // Trouver et désactiver l'UI de pause
         *         auto pauseUI = FindGameObjectByName("PauseOverlay");
         *         if (pauseUI.IsValid()) {
         *             pauseUI.SetActive(false);
         *         }
         *     }
         * 
         *     void CacheGameState() {
         *         // Sauvegarder positions des ennemis, état des triggers, etc.
         *         // Pour une reprise instantanée sans recalcul
         *     }
         * 
         *     void ResyncGameState() {
         *         // Re-synchroniser avec les systèmes externes (réseau, physique)
         *         // Après une pause potentiellement longue
         *     }
         * 
         *     void UpdateEnemies(float dt) { / * ... * / }
        *     void CheckTriggers() { / * ... * / }
        * };
        * @endcode
        */

        /**
         * @example 04_best_practices_pitfalls.cpp
         * @brief Bonnes pratiques et pièges courants avec NkSceneScript.
         * 
         * @code
         * // ✅ BONNES PRATIQUES
         * 
         * // 1. Toujours vérifier les pointeurs de contexte avant utilisation
         * void SafeContextUsage() {
         *     if (auto* world = GetWorld()) {  // ✅ Guard explicite
         *         // Utiliser world en toute sécurité
         *         world->Query<Health>().ForEach([](auto& h) { / * ... * / });
        *     }
        *     // else : contexte non injecté — ignorer ou logger
        * }
        * 
        * // 2. Mettre en cache les résultats de Find* pour éviter les queries répétées
        * class OptimizedScript : public NkSceneScript {
        *     NkVector<NkGameObject> mCachedEnemies;  // Cache membre
        *     bool mEnemiesCached = false;
        * 
        * public:
        *     void OnTick(float dt) noexcept override {
        *         if (!mEnemiesCached) {
        *             mCachedEnemies = FindGameObjectsWithTag("Enemy");  // Query O(n)
        *             mEnemiesCached = true;
        *         }
        *         // Utiliser le cache : O(1) d'accès
        *         for (const auto& enemy : mCachedEnemies) {
        *             UpdateEnemy(enemy, dt);
        *         }
        *     }
        * 
        *     void OnEnemySpawned() {
        *         // Invalider le cache quand un nouvel ennemi apparaît
        *         mEnemiesCached = false;
        *     }
        * };
        * 
        * // 3. Utiliser OnEndPlay() pour le nettoyage garanti
        * class ResourceManagingScript : public NkSceneScript {
        *     AssetHandle mLoadedAsset;
        * 
        * public:
        *     void OnBeginPlay() noexcept override {
        *         mLoadedAsset = LoadHeavyAsset("level_data");  // Acquisition
        *     }
        * 
        *     void OnEndPlay() noexcept override {
        *         UnloadAsset(mLoadedAsset);  // ✅ Libération garantie
        *         mLoadedAsset = AssetHandle::Null();
        *     }
        * };
        * 
        * // 4. Préférer DestroyGameObject() ou world.DestroyDeferred() selon le contexte
        * void SafeDestruction() {
        *     // ❌ DANS une query ECS : utiliser DestroyDeferred
        *     for (auto [health] : GetWorld()->Query<Health>()) {
        *         if (health->value <= 0) {
        *             GetWorld()->DestroyDeferred(health->ownerEntity);  // ✅ Sécurisé
        *         }
        *     }
        *     
        *     // ✅ HORS d'une query : DestroyGameObject est OK
        *     auto target = FindGameObjectByName("Boss");
        *     if (target.IsValid()) {
        *         DestroyGameObject(target);  // ✅ Immédiat et sûr ici
        *     }
        * }
        * 
        * // 5. Documenter l'ordre d'appel attendu des callbacks
        * /// @lifecycle Order: BeginPlay → (FixedTick)* → Tick → LateTick → EndPlay
        * class WellDocumentedScript : public NkSceneScript { /* ... */ };
        * 
        * 
        * // ❌ PIÈGES À ÉVITER
        * 
        * // 1. Appeler les méthodes lifecycle manuellement
        * // ❌ myScript.OnTick(dt);  // Interdit !
        * // ✅ Laisser NkSceneLifecycleSystem gérer automatiquement
        * 
        * // 2. Stocker des références aux résultats de Find* au-delà de leur validité
        * // ❌ auto& enemies = FindGameObjectsWithTag("Enemy");
        * //    // ... plus tard, après un Spawn/Destroy ...
        * //    enemies[0].DoSomething();  // ❌ Référence potentiellement dangling
        * // ✅ Toujours re-résoudre via Find* ou utiliser un cache avec invalidation
        * 
        * // 3. Oublier de vérifier IsValid() sur les GameObjects retournés
        * // ❌ auto obj = FindGameObjectByName("Missing");
        * //    obj.DoSomething();  // ❌ Crash si obj est invalide
        * // ✅ if (obj.IsValid()) { obj.DoSomething(); }
        * 
        * // 4. Utiliser SpawnPrefab avec un prefab invalide
        * // ❌ NkPrefab nullPrefab;
        * //    auto go = SpawnPrefab(nullPrefab);  // Retourne un handle invalide
        * // ✅ Toujours vérifier que le prefab est chargé avant spawn :
        * //    if (myPrefab.IsValid()) { SpawnPrefab(myPrefab, ...); }
        * 
        * // 5. Modifier l'état du monde dans OnEndPlay() sans FlushDeferred
        * // ❌ void OnEndPlay() override {
        * //        GetWorld()->Destroy(someEntity);  // Destruction immédiate
        * //        // ... mais d'autres systèmes peuvent encore itérer !
        * //    }
        * // ✅ Utiliser DestroyDeferred() dans OnEndPlay() si itérations possibles :
        * //    void OnEndPlay() override {
        * //        GetWorld()->DestroyDeferred(someEntity);  // Sécurisé
        * //        GetWorld()->FlushDeferred();  // Appliquer avant retour
        * //    }
        * @endcode
        */

        /** @} */ // end of NkSceneScript_Examples group
    } // namespace ecs
} // namespace nkentseu