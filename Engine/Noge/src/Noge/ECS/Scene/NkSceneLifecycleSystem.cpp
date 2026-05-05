// =============================================================================
// Scene/NkSceneLifecycleSystem.cpp — Implémentations des systèmes de lifecycle
// =============================================================================
/**
 * @file NkSceneLifecycleSystem.cpp
 * @brief Implémentation des trois systèmes ECS de lifecycle de scène.
 * 
 * Ce fichier contient :
 * - Les constructeurs de NkSceneFixedTickSystem, NkSceneTickSystem, NkSceneLateTickSystem
 * - Les méthodes Describe() configurant l'ordonnancement de chaque système
 * - Les méthodes Execute() appelant les callbacks correspondants sur NkSceneGraph
 * - La fonction helper RegisterSceneLifecycle() pour enregistrement en masse
 * 
 * 🔹 Pourquoi TOUTES les méthodes ici ?
 *    • Respect strict de la consigne : aucune méthode non-template inline dans le header
 *    • Même les méthodes triviales (constructeurs, getters) sont déplacées ici
 *    • Avantage : recompilation locale uniquement si ce .cpp change,
 *      meilleure encapsulation des détails d'implémentation
 * 
 * @note Ce fichier inclut les headers nécessaires pour l'implémentation.
 *       Ces inclusions sont locales au .cpp — pas de pollution du header public.
 */

#include "NkSceneLifecycleSystem.h"

namespace nkentseu {
    namespace ecs {

        // =====================================================================
        // ⚙️ NkSceneFixedTickSystem — Implémentations
        // =====================================================================

        /**
         * @brief Constructeur de NkSceneFixedTickSystem.
         * @param scene Pointeur vers la NkSceneGraph à mettre à jour.
         * 
         * @note Initialise mScene avec la valeur fournie (peut être nullptr).
         *       Aucune allocation ou effet secondaire — constructeur trivial.
         */
        NkSceneFixedTickSystem::NkSceneFixedTickSystem(NkSceneGraph* scene) noexcept
            : mScene(scene) {
        }

        /**
         * @brief Décrit la configuration d'exécution du système.
         * @return NkSystemDesc avec groupe FixedUpdate, nom "NkSceneFixedTick", priorité 100.
         * 
         * @note Cette méthode est appelée une fois par le scheduler à l'initialisation.
         *       Elle ne doit pas modifier l'état du système.
         */
        NkSystemDesc NkSceneFixedTickSystem::Describe() const {
            return NkSystemDesc{}
                .InGroup(NkSystemGroup::FixedUpdate)
                .Named("NkSceneFixedTick")
                .Priority(100);
        }

        /**
         * @brief Exécute la mise à jour fixe de la scène.
         * @param world Référence au monde ECS (non utilisé).
         * @param dt Delta-time FIXE en secondes.
         * 
         * @note Appel conditionnel : si mScene != nullptr, appelle FixedTick(dt).
         *       Sinon, no-op silencieux (permet de désactiver sans unregister).
         */
        void NkSceneFixedTickSystem::Execute(NkWorld& /*world*/, float dt) {
            if (mScene) {
                mScene->FixedTick(dt);
            }
        }

        // =====================================================================
        // ⚡ NkSceneTickSystem — Implémentations
        // =====================================================================

        /**
         * @brief Constructeur de NkSceneTickSystem.
         * @param scene Pointeur vers la NkSceneGraph à mettre à jour.
         */
        NkSceneTickSystem::NkSceneTickSystem(NkSceneGraph* scene) noexcept
            : mScene(scene) {
        }

        /**
         * @brief Décrit la configuration d'exécution du système.
         * @return NkSystemDesc avec groupe Update, nom "NkSceneTick", priorité 100.
         */
        NkSystemDesc NkSceneTickSystem::Describe() const {
            return NkSystemDesc{}
                .InGroup(NkSystemGroup::Update)
                .Named("NkSceneTick")
                .Priority(100);
        }

        /**
         * @brief Exécute la mise à jour principale de la scène.
         * @param world Référence au monde ECS (non utilisé).
         * @param dt Delta-time VARIABLE en secondes.
         */
        void NkSceneTickSystem::Execute(NkWorld& /*world*/, float dt) {
            if (mScene) {
                mScene->Tick(dt);
            }
        }

        // =====================================================================
        // 🌙 NkSceneLateTickSystem — Implémentations
        // =====================================================================

        /**
         * @brief Constructeur de NkSceneLateTickSystem.
         * @param scene Pointeur vers la NkSceneGraph à mettre à jour.
         */
        NkSceneLateTickSystem::NkSceneLateTickSystem(NkSceneGraph* scene) noexcept
            : mScene(scene) {
        }

        /**
         * @brief Décrit la configuration d'exécution du système.
         * @return NkSystemDesc avec groupe PostUpdate, nom "NkSceneLateTick", priorité 200.
         * 
         * @note Priorité 200 > 100 garantit l'exécution en DERNIER dans PostUpdate.
         */
        NkSystemDesc NkSceneLateTickSystem::Describe() const {
            return NkSystemDesc{}
                .InGroup(NkSystemGroup::PostUpdate)
                .Named("NkSceneLateTick")
                .Priority(200);
        }

        /**
         * @brief Exécute la mise à jour tardive de la scène.
         * @param world Référence au monde ECS (non utilisé).
         * @param dt Delta-time VARIABLE en secondes.
         */
        void NkSceneLateTickSystem::Execute(NkWorld& /*world*/, float dt) {
            if (mScene) {
                mScene->LateTick(dt);
            }
        }

        // =====================================================================
        // 🎯 Helper : RegisterSceneLifecycle
        // =====================================================================

        /**
         * @brief Enregistre les trois systèmes de lifecycle d'une scène dans un scheduler.
         * @param scheduler Référence au NkScheduler cible.
         * @param scene Pointeur vers la NkSceneGraph à orchestrer.
         * 
         * 🔹 Étapes :
         *    1. Crée et ajoute NkSceneFixedTickSystem (via new)
         *    2. Crée et ajoute NkSceneTickSystem (via new)
         *    3. Crée et ajoute NkSceneLateTickSystem (via new)
         * 
         * 🔹 Gestion mémoire :
         *    • Les systèmes sont alloués via new — le scheduler en prend ownership
         *    • Le scheduler les détruira automatiquement via delete à sa propre destruction
         *    • Ne pas delete manuellement les pointeurs créés ici
         * 
         * @note Cette fonction est inline dans le header pour commodité, mais son
         *       corps est défini ici pour respecter la consigne "pas d'inline".
         *       En pratique, le linker résoudra la référence sans problème.
         */
        void RegisterSceneLifecycle(NkScheduler& scheduler,
                                     NkSceneGraph* scene) noexcept {
            // Création et enregistrement des trois systèmes de lifecycle
            // L'ordre d'ajout n'affecte pas l'exécution — c'est Describe() qui détermine
            // le groupe et la priorité dans le DAG du scheduler
            scheduler.AddSystem(new NkSceneFixedTickSystem(scene));
            scheduler.AddSystem(new NkSceneTickSystem(scene));
            scheduler.AddSystem(new NkSceneLateTickSystem(scene));
        }

    } // namespace ecs
} // namespace nkentseu

// =====================================================================
// 🎭 SceneLifecycleManager — Implémentations
// =====================================================================

#include "NkSceneLifecycleSystem.h"  // Ou le header contenant la déclaration

namespace nkentseu {
    namespace ecs {

        // ── Enregistrement ───────────────────────────────────────────

        bool SceneLifecycleManager::RegisterScene(NkScheduler& scheduler,
                                                   NkSceneGraph* scene) noexcept {
            // Guard : scène nulle → échec immédiat
            if (!scene) {
                return false;
            }

            // Vérification de doublon : idempotence
            if (IsRegistered(scene)) {
                // Déjà enregistrée → retour succès sans action supplémentaire
                return true;
            }

            // Enregistrement des 3 systèmes de lifecycle dans le scheduler
            // Cette fonction alloue via new — le scheduler en prend ownership
            RegisterSceneLifecycle(scheduler, scene);

            // Ajout au registre local pour suivi et helpers
            mRegisteredScenes.PushBack(scene);

            return true;
        }

        // ── Désenregistrement ────────────────────────────────────────

        bool SceneLifecycleManager::UnregisterScene(NkSceneGraph* scene) noexcept {
            // Guard : scène nulle → échec immédiat
            if (!scene) {
                return false;
            }

            // Recherche linéaire dans le registre
            for (nk_usize i = 0; i < mRegisteredScenes.Size(); ++i) {
                if (mRegisteredScenes[i] == scene) {
                    // Trouvé : suppression par swap-with-last (O(1))
                    // Note : l'ordre des scènes dans le registre n'est pas garanti
                    mRegisteredScenes[i] = mRegisteredScenes[mRegisteredScenes.Size() - 1];
                    mRegisteredScenes.PopBack();
                    return true;
                }
            }

            // Non trouvée → retour échec (idempotence)
            return false;
        }

        // ── Helpers de requête ───────────────────────────────────────

        bool SceneLifecycleManager::IsRegistered(NkSceneGraph* scene) const noexcept {
            if (!scene) {
                return false;
            }
            for (nk_usize i = 0; i < mRegisteredScenes.Size(); ++i) {
                if (mRegisteredScenes[i] == scene) {
                    return true;
                }
            }
            return false;
        }

        nk_usize SceneLifecycleManager::GetRegisteredCount() const noexcept {
            return mRegisteredScenes.Size();
        }

        NkSceneGraph* SceneLifecycleManager::FindSceneByName(const char* name) const noexcept {
            // Guards : nom nul ou vide → retour immédiat
            if (!name || name[0] == '\0') {
                return nullptr;
            }

            // Recherche linéaire avec comparaison de nom
            for (nk_usize i = 0; i < mRegisteredScenes.Size(); ++i) {
                NkSceneGraph* scene = mRegisteredScenes[i];
                if (scene && NkStrEqual(scene->Name(), name)) {
                    return scene;
                }
            }
            return nullptr;
        }

        // ── Cleanup global ───────────────────────────────────────────

        void SceneLifecycleManager::CleanupAll() noexcept {
            // Appel de EndPlay() sur chaque scène started pour nettoyage
            for (nk_usize i = 0; i < mRegisteredScenes.Size(); ++i) {
                NkSceneGraph* scene = mRegisteredScenes[i];
                if (scene) {
                    // EndPlay() est safe à appeler même si déjà appelé
                    // (la scène gère internement le flag mStarted)
                    scene->EndPlay();
                }
            }

            // Clear du registre local — les systèmes dans le scheduler
            // seront détruits automatiquement via l'ownership du scheduler
            mRegisteredScenes.Clear();
        }

        // ── Debug / Profiling ────────────────────────────────────────

        void SceneLifecycleManager::DumpRegisteredScenes() const noexcept {
            // Output basique via logger ou printf — à adapter à votre système
            printf("[SceneLifecycleManager] Scènes enregistrées: %u\n",
                   static_cast<unsigned>(mRegisteredScenes.Size()));

            for (nk_usize i = 0; i < mRegisteredScenes.Size(); ++i) {
                NkSceneGraph* scene = mRegisteredScenes[i];
                if (scene) {
                    const char* name = scene->Name() ? scene->Name() : "<unnamed>";
                    bool paused = scene->IsPaused();
                    bool hasScript = scene->HasScript();
                    
                    printf("  [%u] %s (paused:%s, script:%s)\n",
                           static_cast<unsigned>(i),
                           name,
                           paused ? "yes" : "no",
                           hasScript ? "yes" : "no");
                } else {
                    printf("  [%u] <null pointer>\n", static_cast<unsigned>(i));
                }
            }
        }

    } // namespace ecs
} // namespace nkentseu