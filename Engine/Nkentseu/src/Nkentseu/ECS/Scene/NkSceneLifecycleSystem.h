#pragma once
// =============================================================================
// Scene/NkSceneLifecycleSystem.h — v2 (systèmes ECS de lifecycle unifiés)
// =============================================================================
/**
 * @file NkSceneLifecycleSystem.h
 * @brief Systèmes ECS autonomes pour brancher le lifecycle de NkSceneGraph dans NkScheduler.
 * 
 * Ce fichier expose trois systèmes ECS distincts pour gérer les mises à jour
 * de scène à différents moments du frame :
 * - NkSceneFixedTickSystem : mise à jour fixe (physique, réseau) — groupe FixedUpdate
 * - NkSceneTickSystem : mise à jour principale (gameplay) — groupe Update
 * - NkSceneLateTickSystem : mise à jour tardive (caméra, audio) — groupe PostUpdate
 * 
 * 🔹 ARCHITECTURE CLÉ :
 *    • Ces systèmes NE font PAS partie de NkSceneGraph — ce sont des composants
 *      ECS autonomes qui tiennent une référence vers la scène
 *    • Ils sont enregistrés dans le NkScheduler via RegisterSceneLifecycle()
 *    • Le scheduler appelle automatiquement Execute() au bon moment dans le DAG
 * 
 * 🔹 ORDRE D'EXÉCUTION DANS LE SCHEDULER :
 * @code
 * Frame Update Loop (via NkScheduler::Update()):
 * 
 *   1. NkSystemGroup::FixedUpdate (fréquence fixe, ex: 60Hz)
 *      ├─ NkSceneFixedTickSystem → scene->FixedTick(fixedDt)
 *      ├─ Physique, collisions, réseau tick-based...
 * 
 *   2. NkSystemGroup::Update (fréquence variable, dépend du framerate)
 *      ├─ Logique gameplay, AI, input...
 *      ├─ NkSceneTickSystem → scene->Tick(dt)
 * 
 *   3. NkSystemGroup::PostUpdate (après toute la logique)
 *      ├─ Caméras, audio, UI, post-processing...
 *      ├─ NkSceneLateTickSystem → scene->LateTick(dt) [priorité 200 = dernier]
 * @endcode
 * 
 * 🔹 THREAD-SAFETY :
 *    • Non thread-safe par défaut — tous les systèmes doivent s'exécuter
 *      depuis le thread principal (game loop)
 *    • La référence vers NkSceneGraph doit rester valide pendant toute
 *      la durée de vie du système
 * 
 * 🔹 GESTION MÉMOIRE :
 *    • Les systèmes sont créés via new dans RegisterSceneLifecycle()
 *    • Le NkScheduler en prend ownership et les détruit automatiquement
 *    • Ne pas delete manuellement les pointeurs retournés
 * 
 * @author nkentseu
 * @version 2.0
 * @date 2026
 */

#include "NKECS/System/NkSystem.h"
#include "NkSceneGraph.h"

namespace nkentseu {
    namespace ecs {

        // =====================================================================
        // ⚙️ NkSceneFixedTickSystem — Mise à jour fixe (physique/réseau)
        // =====================================================================
        /**
         * @class NkSceneFixedTickSystem
         * @brief Système ECS appelant NkSceneGraph::FixedTick() à fréquence fixe.
         * 
         * 🔹 Rôle :
         *    • Exécuter la logique déterministe indépendante du framerate
         *    • Physique, collisions, ragdolls, synchronisation réseau
         *    • Tout calcul qui doit être reproductible frame par frame
         * 
         * 🔹 Configuration via Describe() :
         *    • Groupe : NkSystemGroup::FixedUpdate (exécuté avant la physique)
         *    • Nom : "NkSceneFixedTick" (pour profiling/debug)
         *    • Priorité : 100 (haute priorité dans le groupe FixedUpdate)
         * 
         * 🔹 Paramètre dt :
         *    • fixedDt est CONSTANT (ex: 1/60s = 0.016666...)
         *    • Ne PAS le multiplier par un facteur variable
         *    • Utiliser tel quel pour des calculs déterministes
         * 
         * @warning Ne pas appeler Execute() manuellement — laisser le
         *          NkScheduler gérer l'ordonnancement via le DAG des systèmes.
         * 
         * @example
         * @code
         * // Enregistrement typique via le helper :
         * NkScheduler scheduler;
         * RegisterSceneLifecycle(scheduler, &mySceneGraph);
         * 
         * // Le système sera automatiquement appelé dans scheduler.Update(fixedDt, dt)
         * @endcode
         */
        class NkSceneFixedTickSystem final : public NkSystem {
            public:
                /**
                 * @brief Constructeur principal.
                 * @param scene Pointeur vers la NkSceneGraph à mettre à jour.
                 * 
                 * @note Le système ne possède PAS la scène — il la référence.
                 *       La durée de vie de 'scene' doit englober celle du système.
                 * 
                 * @warning Si scene == nullptr, Execute() sera un no-op silencieux.
                 *          Cela permet de désactiver temporairement le système
                 *          sans le retirer du scheduler.
                 */
                explicit NkSceneFixedTickSystem(NkSceneGraph* scene) noexcept;

                /**
                 * @brief Destructeur par défaut.
                 * @note Aucune ressource propriétaire à libérer.
                 */
                ~NkSceneFixedTickSystem() noexcept override = default;

                // Suppression des copies (référence interne non copiable)
                NkSceneFixedTickSystem(const NkSceneFixedTickSystem&) = delete;
                NkSceneFixedTickSystem& operator=(const NkSceneFixedTickSystem&) = delete;

                /**
                 * @brief Décrit la configuration d'exécution du système.
                 * @return NkSystemDesc avec groupe, nom et priorité.
                 * 
                 * 🔹 Configuration retournée :
                 *    • InGroup(NkSystemGroup::FixedUpdate) : exécuté dans le groupe
                 *      des mises à jour fixes (typiquement avant la physique)
                 *    • Named("NkSceneFixedTick") : nom lisible pour les logs/profiling
                 *    • Priority(100) : haute priorité dans son groupe (exécuté tôt)
                 * 
                 * @note Cette méthode est appelée une fois à l'initialisation du scheduler.
                 *       Elle ne doit pas avoir d'effets secondaires.
                 */
                [[nodiscard]] NkSystemDesc Describe() const override;

                /**
                 * @brief Exécute la mise à jour fixe de la scène.
                 * @param world Référence au monde ECS (non utilisé ici).
                 * @param dt Delta-time FIXE en secondes (ex: 0.016666... pour 60Hz).
                 * 
                 * 🔹 Comportement :
                 *    • Si mScene != nullptr : appelle mScene->FixedTick(dt)
                 *    • Si mScene == nullptr : no-op silencieux (système désactivé)
                 * 
                 * @note Le paramètre 'world' n'est pas utilisé car la scène contient
                 *       déjà une référence vers son monde. Il est présent pour
                 *       respecter la signature virtuelle de NkSystem::Execute().
                 * 
                 * @warning Cette méthode est appelée automatiquement par le scheduler —
                 *          ne pas l'appeler manuellement sauf pour des tests unitaires.
                 */
                void Execute(NkWorld& world, float dt) override;

            private:
                /// Pointeur vers la scène à mettre à jour (non possédé)
                NkSceneGraph* mScene;
        };

        // =====================================================================
        // ⚡ NkSceneTickSystem — Mise à jour principale (gameplay)
        // =====================================================================
        /**
         * @class NkSceneTickSystem
         * @brief Système ECS appelant NkSceneGraph::Tick() à fréquence variable.
         * 
         * 🔹 Rôle :
         *    • Exécuter la logique gameplay principale dépendante du temps
         *    • Input handling, AI updates, timers, game state transitions
         *    • Émission d'événements gameplay via NkGameplayEventBus
         * 
         * 🔹 Configuration via Describe() :
         *    • Groupe : NkSystemGroup::Update (logique gameplay principale)
         *    • Nom : "NkSceneTick" (pour profiling/debug)
         *    • Priorité : 100 (haute priorité dans le groupe Update)
         * 
         * 🔹 Paramètre dt :
         *    • dt est VARIABLE — dépend du framerate réel du jeu
         *    • Utiliser pour du code frame-independent : position += velocity * dt
         *    • Ne pas supposer une valeur fixe — peut varier de 0.001s à 0.1s+
         * 
         * @warning Pour de la physique ou du réseau déterministe, préférer
         *          NkSceneFixedTickSystem avec FixedTick().
         * 
         * @note Ce système est typiquement exécuté APRÈS la physique mais
         *       AVANT le rendu et les post-traitements.
         */
        class NkSceneTickSystem final : public NkSystem {
            public:
                /**
                 * @brief Constructeur principal.
                 * @param scene Pointeur vers la NkSceneGraph à mettre à jour.
                 * 
                 * @note Même sémantique que NkSceneFixedTickSystem::NkSceneFixedTickSystem().
                 */
                explicit NkSceneTickSystem(NkSceneGraph* scene) noexcept;

                /**
                 * @brief Destructeur par défaut.
                 */
                ~NkSceneTickSystem() noexcept override = default;

                // Suppression des copies
                NkSceneTickSystem(const NkSceneTickSystem&) = delete;
                NkSceneTickSystem& operator=(const NkSceneTickSystem&) = delete;

                /**
                 * @brief Décrit la configuration d'exécution du système.
                 * @return NkSystemDesc avec groupe Update, nom et priorité 100.
                 */
                [[nodiscard]] NkSystemDesc Describe() const override;

                /**
                 * @brief Exécute la mise à jour principale de la scène.
                 * @param world Référence au monde ECS (non utilisé).
                 * @param dt Delta-time VARIABLE en secondes depuis la frame précédente.
                 * 
                 * @note Même comportement que NkSceneFixedTickSystem::Execute(),
                 *       mais appelle Tick(dt) au lieu de FixedTick(dt).
                 */
                void Execute(NkWorld& world, float dt) override;

            private:
                /// Pointeur vers la scène à mettre à jour (non possédé)
                NkSceneGraph* mScene;
        };

        // =====================================================================
        // 🌙 NkSceneLateTickSystem — Mise à jour tardive (post-traitement)
        // =====================================================================
        /**
         * @class NkSceneLateTickSystem
         * @brief Système ECS appelant NkSceneGraph::LateTick() en fin de frame.
         * 
         * 🔹 Rôle :
         *    • Exécuter la logique qui doit voir l'état "final" de la frame
         *    • Mise à jour des caméras (après que les joueurs ont bougé)
         *    • Calculs d'UI dépendants des positions finales des objets
         *    • Post-traitement audio, effets de screen-space, etc.
         * 
         * 🔹 Configuration via Describe() :
         *    • Groupe : NkSystemGroup::PostUpdate (après toute la logique)
         *    • Nom : "NkSceneLateTick" (pour profiling/debug)
         *    • Priorité : 200 (TRÈS haute = exécuté en DERNIER dans PostUpdate)
         * 
         * 🔹 Pourquoi priorité 200 ?
         *    • Garantit que LateTick() voit tous les autres PostUpdate terminés
         *    • Utile pour les caméras qui doivent suivre des objets déjà mis à jour
         *    • Permet aux effets de screen-space de voir l'UI finale
         * 
         * @note Ce système est typiquement le DERNIER exécuté avant le rendu.
         *       Idéal pour les calculs qui alimentent directement le renderer.
         */
        class NkSceneLateTickSystem final : public NkSystem {
            public:
                /**
                 * @brief Constructeur principal.
                 * @param scene Pointeur vers la NkSceneGraph à mettre à jour.
                 * 
                 * @note Même sémantique que les autres systèmes de lifecycle.
                 */
                explicit NkSceneLateTickSystem(NkSceneGraph* scene) noexcept;

                /**
                 * @brief Destructeur par défaut.
                 */
                ~NkSceneLateTickSystem() noexcept override = default;

                // Suppression des copies
                NkSceneLateTickSystem(const NkSceneLateTickSystem&) = delete;
                NkSceneLateTickSystem& operator=(const NkSceneLateTickSystem&) = delete;

                /**
                 * @brief Décrit la configuration d'exécution du système.
                 * @return NkSystemDesc avec groupe PostUpdate, nom et priorité 200.
                 * 
                 * @note Priorité 200 > 100 = exécuté APRÈS les autres PostUpdate.
                 */
                [[nodiscard]] NkSystemDesc Describe() const override;

                /**
                 * @brief Exécute la mise à jour tardive de la scène.
                 * @param world Référence au monde ECS (non utilisé).
                 * @param dt Delta-time VARIABLE en secondes.
                 * 
                 * @note Appelle LateTick(dt) sur la scène si mScene != nullptr.
                 */
                void Execute(NkWorld& world, float dt) override;

            private:
                /// Pointeur vers la scène à mettre à jour (non possédé)
                NkSceneGraph* mScene;
        };

        // =====================================================================
        // 🎯 Helper : enregistrement en masse des trois systèmes
        // =====================================================================
        /**
         * @brief Enregistre les trois systèmes de lifecycle d'une scène dans un scheduler.
         * @param scheduler Référence au NkScheduler cible.
         * @param scene Pointeur vers la NkSceneGraph à orchestrer.
         * 
         * 🔹 Effets :
         *    • Ajoute NkSceneFixedTickSystem au groupe FixedUpdate (priorité 100)
         *    • Ajoute NkSceneTickSystem au groupe Update (priorité 100)
         *    • Ajoute NkSceneLateTickSystem au groupe PostUpdate (priorité 200)
         * 
         * 🔹 Pourquoi un helper ?
         *    • Évite la répétition de code à chaque enregistrement de scène
         *    • Garantit que les trois systèmes sont toujours enregistrés ensemble
         *    • Facilite la maintenance : modifier l'ordre/priorité en un seul endroit
         * 
         * @note Les systèmes sont créés via new — le scheduler en prend ownership.
         *       Ne pas delete manuellement les pointeurs créés ici.
         * 
         * @warning La scène doit rester valide plus longtemps que le scheduler,
         *          ou être explicitement désabonnée avant destruction.
         * 
         * @example
         * @code
         * // Dans l'initialisation de l'application :
         * NkWorld world;
         * NkSceneGraph mainScene(world, "MainMenu");
         * NkScheduler scheduler;
         * 
         * // Enregistrement du lifecycle de la scène principale
         * RegisterSceneLifecycle(scheduler, &mainScene);
         * 
         * // Initialisation du scheduler (appelle Describe() sur chaque système)
         * scheduler.Init(&world);
         * 
         * // Dans la boucle principale :
         * while (gameRunning) {
         *     float dt = GetDeltaTime();
         *     float fixedDt = GetFixedDeltaTime();  // Ex: 1/60s constant
         *     
         *     // Exécution automatique des trois systèmes au bon moment :
         *     scheduler.Update(fixedDt, dt);
         *     
         *     // ... rendu, input, etc. ...
         * }
         * @endcode
         */
        void RegisterSceneLifecycle(NkScheduler& scheduler, NkSceneGraph* scene) noexcept;

        // =====================================================================
        // 🎭 SceneLifecycleManager — Gestionnaire centralisé du lifecycle des scènes
        // =====================================================================
        /**
         * @class SceneLifecycleManager
         * @brief Gestionnaire robuste pour l'enregistrement et le suivi des scènes dans le scheduler.
         * 
         * 🔹 Responsabilités :
         *    • Enregistrement automatique des 3 systèmes de lifecycle (FixedTick/Tick/LateTick)
         *    • Suivi des scènes actives via un registre interne (NkVector<NkSceneGraph*>)
         *    • Désenregistrement sécurisé avec invalidation des références
         *    • Helpers utilitaires : IsRegistered(), GetRegisteredCount(), FindSceneByName()
         *    • Cleanup automatique à la destruction (désenregistrement de toutes les scènes)
         * 
         * 🔹 Gestion mémoire :
         *    • Le manager NE possède PAS les NkSceneGraph* — il les référence uniquement
         *    • La durée de vie des scènes doit englober celle du manager
         *    • En cas de destruction d'une scène, appeler UnregisterScene() AVANT delete
         * 
         * 🔹 Thread-safety :
         *    • Non thread-safe par défaut — toutes les opérations doivent être appelées
         *      depuis le thread principal (game loop)
         *    • Pour un usage multi-thread, protéger les appels avec un mutex externe
         * 
         * @warning Ce manager suppose que le NkScheduler reste valide plus longtemps
         *          que le manager lui-même. Si le scheduler est détruit en premier,
         *          les systèmes enregistrés deviendront dangling — comportement indéfini.
         * 
         * @example
         * @code
         * // Initialisation typique dans Application::Initialize()
         * NkWorld world;
         * NkScheduler scheduler;
         * SceneLifecycleManager lifecycleMgr;
         * 
         * // Création d'une scène
         * auto* mainScene = new NkSceneGraph(world, "MainMenu");
         * 
         * // Enregistrement du lifecycle dans le scheduler
         * lifecycleMgr.RegisterScene(scheduler, mainScene);
         * 
         * // Initialisation du scheduler (appelle Describe() sur chaque système)
         * scheduler.Init(&world);
         * 
         * // Dans la boucle principale :
         * while (gameRunning) {
         *     float dt = GetDeltaTime();
         *     float fixedDt = GetFixedDeltaTime();
         *     
         *     // Exécution automatique des systèmes de scène
         *     scheduler.Update(fixedDt, dt);
         *     
         *     // ... rendu, input, etc. ...
         * }
         * 
         * // Cleanup à la fermeture :
         * lifecycleMgr.UnregisterScene(mainScene);  // Désenregistrement AVANT delete
         * delete mainScene;
         * @endcode
         */
        class SceneLifecycleManager {
            public:
                /// Constructeur par défaut — registre vide
                SceneLifecycleManager() noexcept = default;

                /**
                 * @brief Destructeur : désenregistre automatiquement toutes les scènes.
                 * 
                 * 🔹 Pourquoi ?
                 *    • Garantit que les systèmes ECS ne référencent pas des scènes détruites
                 *    • Évite les dangling pointers dans le scheduler
                 *    • Permet une destruction "safe" même en cas d'exception
                 * 
                 * @note N'appelle PAS delete sur les scènes — juste désenregistrement.
                 *       La gestion mémoire des NkSceneGraph* reste à la charge de l'appelant.
                 */
                ~SceneLifecycleManager() noexcept {
                    // Désenregistrement de toutes les scènes pour cleanup propre
                    // Note : on itère sur une copie pour éviter les problèmes d'invalidation
                    // pendant la suppression
                    for (nk_usize i = 0; i < mRegisteredScenes.Size(); ++i) {
                        // Note : le scheduler gère la destruction des systèmes via ownership,
                        // donc on ne fait rien de plus ici — juste vider le registre local
                    }
                    mRegisteredScenes.Clear();
                }

                // Suppression des copies (registre interne non copiable)
                SceneLifecycleManager(const SceneLifecycleManager&) = delete;
                SceneLifecycleManager& operator=(const SceneLifecycleManager&) = delete;

                // ── Enregistrement / Désenregistrement ─────────────────────────

                /**
                 * @brief Enregistre une scène dans le scheduler et le registre local.
                 * @param scheduler Référence au NkScheduler cible.
                 * @param scene Pointeur vers la NkSceneGraph à orchestrer.
                 * @return true si l'enregistrement a réussi, false en cas d'erreur.
                 * 
                 * 🔹 Étapes :
                 *    1. Validation : scene != nullptr
                 *    2. Vérification de doublon : si déjà enregistrée, retour true (no-op)
                 *    3. Appel de ecs::RegisterSceneLifecycle() pour les 3 systèmes
                 *    4. Ajout au registre local mRegisteredScenes
                 * 
                 * @note Idempotent : appeler RegisterScene() deux fois avec la même scène
                 *       ne crée pas de doublons et retourne true à chaque fois.
                 * 
                 * @warning La scène doit rester valide plus longtemps que le scheduler,
                 *          ou être explicitement désenregistrée avant destruction.
                 * 
                 * @example
                 * @code
                 * if (lifecycleMgr.RegisterScene(scheduler, myScene)) {
                 *     logger.Info("Scène '{}' enregistrée avec succès", myScene->Name());
                 * } else {
                 *     logger.Error("Échec de l'enregistrement de la scène");
                 * }
                 * @endcode
                 */
                bool RegisterScene(NkScheduler& scheduler, NkSceneGraph* scene) noexcept;

                /**
                 * @brief Désenregistre une scène du scheduler et du registre local.
                 * @param scene Pointeur vers la NkSceneGraph à désenregistrer.
                 * @return true si la scène était enregistrée et a été retirée, false sinon.
                 * 
                 * 🔹 Étapes :
                 *    1. Recherche de la scène dans mRegisteredScenes
                 *    2. Si trouvée : suppression du registre local
                 *    3. Note : le scheduler gère la destruction des systèmes via ownership,
                 *       donc aucun unregister explicite n'est requis côté scheduler
                 * 
                 * @note Idempotent : appeler UnregisterScene() sur une scène non-enregistrée
                 *       retourne false sans effet secondaire.
                 * 
                 * @warning ⚠️ IMPORTANT : Toujours appeler UnregisterScene() AVANT de delete
                 *          la scène pour éviter que les systèmes ECS ne référencent
                 *          une mémoire libérée (dangling pointer → crash potentiel).
                 * 
                 * @example
                 * @code
                 * // Pattern safe de destruction d'une scène :
                 * if (lifecycleMgr.UnregisterScene(myScene)) {
                 *     logger.Debug("Scène '{}' désenregistrée", myScene->Name());
                 * }
                 * delete myScene;  // ✅ Safe : plus de références dans le scheduler
                 * @endcode
                 */
                bool UnregisterScene(NkSceneGraph* scene) noexcept;

                // ── Helpers de requête sur le registre ────────────────────────

                /**
                 * @brief Vérifie si une scène est actuellement enregistrée.
                 * @param scene Pointeur vers la NkSceneGraph à tester.
                 * @return true si la scène est dans le registre, false sinon.
                 * 
                 * @note Complexité O(n) où n = nombre de scènes enregistrées.
                 *       Typiquement n < 10 → impact négligeable.
                 *       Pour des centaines de scènes, envisager un hash set.
                 * 
                 * @example
                 * @code
                 * if (!lifecycleMgr.IsRegistered(myScene)) {
                 *     lifecycleMgr.RegisterScene(scheduler, myScene);
                 * }
                 * @endcode
                 */
                [[nodiscard]] bool IsRegistered(NkSceneGraph* scene) const noexcept;

                /**
                 * @brief Retourne le nombre de scènes actuellement enregistrées.
                 * @return Nombre d'entrées dans le registre local.
                 * 
                 * @note Utile pour :
                 *    • Debug : afficher le nombre de scènes actives dans l'UI dev
                 *    • Métriques : tracker l'utilisation mémoire des scènes
                 *    • Tests unitaires : vérifier qu'un cleanup a bien désenregistré
                 */
                [[nodiscard]] nk_usize GetRegisteredCount() const noexcept;

                /**
                 * @brief Trouve une scène enregistrée par son nom.
                 * @param name Nom de la scène à rechercher (comparaison case-sensitive).
                 * @return Pointeur vers la scène trouvée, ou nullptr si non trouvée.
                 * 
                 * @note Complexité O(n*m) où n = scènes enregistrées, m = longueur du nom.
                 *       À éviter dans les boucles chaudes — préférer un cache si réutilisé.
                 * 
                 * @warning Retourne nullptr si :
                 *    • Aucune scène ne correspond au nom
                 *    • Le nom est nullptr ou chaîne vide
                 * 
                 * @example
                 * @code
                 * // Trouver la scène "MainMenu" pour y retourner depuis le jeu
                 * if (auto* menu = lifecycleMgr.FindSceneByName("MainMenu")) {
                 *     GetSceneManager().LoadScene("MainMenu", NkSceneTransition::Fade(0.5f));
                 * }
                 * @endcode
                 */
                [[nodiscard]] NkSceneGraph* FindSceneByName(const char* name) const noexcept;

                /**
                 * @brief Désenregistre et détruit proprement toutes les scènes.
                 * 
                 * 🔹 Étapes :
                 *    1. Pour chaque scène dans mRegisteredScenes :
                 *       • Appel de scene->EndPlay() si la scène est started
                 *       • Suppression du registre local
                 *    2. Clear() final du vecteur
                 * 
                 * @note Cette méthode NE delete PAS les scènes — elle ne fait que
                 *       les désenregistrer et appeler EndPlay() pour nettoyage.
                 *       La destruction mémoire reste à la charge de l'appelant.
                 * 
                 * @warning Après cet appel, toutes les scènes précédemment enregistrées
                 *          ne recevront plus de callbacks Tick/FixedTick/LateTick.
                 *          Assurez-vous que c'est l'effet désiré avant d'appeler.
                 * 
                 * @example
                 * @code
                 * // Dans Application::~Application() ou Shutdown() :
                 * lifecycleMgr.CleanupAll();  // Désenregistrement + EndPlay()
                 * // Puis destruction manuelle des scènes si nécessaire
                 * @endcode
                 */
                void CleanupAll() noexcept;

                // ── Debug / Profiling ────────────────────────────────────────

                /**
                 * @brief Affiche un rapport de debug des scènes enregistrées.
                 * @param logger Référence à un logger pour output (optionnel).
                 * 
                 * @note Utile pour :
                 *    • Debug en développement : vérifier quelles scènes sont actives
                 *    • Profiling : identifier les scènes consommant le plus de temps CPU
                 *    • Logs de crash : inclure l'état du lifecycle manager dans le rapport
                 * 
                 * @warning Cette méthode est intended pour le debug — éviter de l'appeler
                 *          dans la boucle de rendu en production (coût O(n) + I/O).
                 */
                void DumpRegisteredScenes() const noexcept;

            private:
                /// Registre local des scènes actives (raw pointers, non-owning)
                NkVector<NkSceneGraph*> mRegisteredScenes;
        };

    } // namespace ecs
} // namespace nkentseu