// =============================================================================
// Scene/NkSceneScript.cpp — Implémentations non-template de NkSceneScript
// =============================================================================
/**
 * @file NkSceneScript.cpp
 * @brief Implémentation des méthodes non-template de NkSceneScript.
 * 
 * Ce fichier contient :
 * - Le destructeur virtuel (définition pour vtable)
 * - Les implémentations par défaut des callbacks lifecycle
 * - L'injection du contexte (SetContext) et les getters
 * - Les helpers utilitaires : FindGameObjectsWithTag, FindGameObjectByName,
 *   SpawnPrefab, DestroyGameObject
 * 
 * 🔹 Pourquoi TOUTES les méthodes non-template ici ?
 *    • Respect de la consigne : aucune méthode non-template inline dans le header
 *    • Même les méthodes triviales (GetWorld, GetScene) sont déplacées ici
 *    • Avantage : réduction du code généré, meilleure encapsulation,
 *      recompilation locale uniquement si ce .cpp change
 * 
 * 🔹 Exception : les méthodes template (GetComponentInWorld<T>)
 *    • Doivent rester dans le header pour l'instantiation à chaque site d'appel
 *    • C'est une contrainte du langage C++, pas un choix de conception
 * 
 * @note Ce fichier inclut les headers nécessaires pour l'implémentation :
 *       NkWorld.h, NkSceneGraph.h, NkGameObject.h, NkPrefab.h, etc.
 *       Ces inclusions sont locales au .cpp — pas de pollution du header public.
 */

#include "NkSceneScript.h"
#include "NKECS/World/NkWorld.h"
#include "NKECS/Scene/NkSceneGraph.h"
#include "NKECS/GameObject/NkGameObject.h"
#include "NKECS/Components/Core/NkTag.h"
#include "NKECS/Components/Core/NkName.h"
#include "NKECS/Prefabs/NkPrefab.h"
#include "NKMath/NkMath.h"

namespace nkentseu {
    namespace ecs {
        // =====================================================================
        // 🔧 Destructeur virtuel
        // =====================================================================

        /**
         * @brief Destructeur virtuel de NkSceneScript.
         * 
         * @note Définition "out-of-line" pour :
         *    • Émettre la vtable dans cette unité de compilation uniquement
         *    • Éviter la duplication de la vtable dans chaque TU utilisant le header
         *    • Permettre une suppression polymorphique correcte via pointeur de base
         * 
         * @warning noexcept : essentiel pour la compatibilité avec les systèmes
         *          de cleanup et la garantie de non-propagation d'exceptions
         *          lors de la destruction d'un script de scène.
         */
        NkSceneScript::~NkSceneScript() noexcept = default;

        // =====================================================================
        // 🔄 Callbacks lifecycle (implémentations par défaut)
        // =====================================================================

        /**
         * @brief Implémentation par défaut de OnBeginPlay().
         * @note Corps vide — les classes dérivées override si besoin.
         */
        void NkSceneScript::OnBeginPlay() noexcept {
            // Implémentation par défaut : no-op
        }

        /**
         * @brief Implémentation par défaut de OnTick().
         * @param dt Delta-time en secondes (non utilisé par défaut).
         */
        void NkSceneScript::OnTick(float dt) noexcept {
            // Supprime warning unused parameter dans les builds avec warnings élevés
            (void)dt;
        }

        /**
         * @brief Implémentation par défaut de OnLateTick().
         * @param dt Delta-time en secondes (non utilisé par défaut).
         */
        void NkSceneScript::OnLateTick(float dt) noexcept {
            (void)dt;
        }

        /**
         * @brief Implémentation par défaut de OnFixedTick().
         * @param fixedDt Delta-time fixe (non utilisé par défaut).
         */
        void NkSceneScript::OnFixedTick(float fixedDt) noexcept {
            (void)fixedDt;
        }

        /**
         * @brief Implémentation par défaut de OnEndPlay().
         * @note Corps vide — les classes dérivées override pour le nettoyage.
         */
        void NkSceneScript::OnEndPlay() noexcept {
            // Implémentation par défaut : no-op
        }

        /**
         * @brief Implémentation par défaut de OnPause().
         */
        void NkSceneScript::OnPause() noexcept {
            // Implémentation par défaut : no-op
        }

        /**
         * @brief Implémentation par défaut de OnResume().
         */
        void NkSceneScript::OnResume() noexcept {
            // Implémentation par défaut : no-op
        }

        // =====================================================================
        // 🎯 Injection et accès au contexte
        // =====================================================================

        /**
         * @brief Injecte les pointeurs vers le monde et la scène.
         * @param world Pointeur vers le NkWorld contenant cette scène.
         * @param scene Pointeur vers le NkSceneGraph gérant cette scène.
         * 
         * @note Méthode appelée automatiquement par NkSceneGraph::SetScript<T>().
         *       Ne pas appeler manuellement — risque de corruption d'état.
         */
        void NkSceneScript::SetContext(NkWorld* world,
                                        NkSceneGraph* scene) noexcept {
            mWorld = world;
            mScene = scene;
        }

        /**
         * @brief Retourne un pointeur vers le monde ECS associé.
         * @return NkWorld* ou nullptr si le contexte n'a pas été injecté.
         */
        NkWorld* NkSceneScript::GetWorld() const noexcept {
            return mWorld;
        }

        /**
         * @brief Retourne un pointeur vers la scène graphique associée.
         * @return NkSceneGraph* ou nullptr si le contexte n'a pas été injecté.
         */
        NkSceneGraph* NkSceneScript::GetScene() const noexcept {
            return mScene;
        }

        // =====================================================================
        // 🔍 Helpers de recherche d'entités
        // =====================================================================

        /**
         * @brief Trouve tous les GameObjects ayant un tag spécifique.
         * @param tag Nom du tag à rechercher (comparaison case-sensitive).
         * @return NkVector<NkGameObject> contenant les entités trouvées.
         * 
         * @note Retourne un vecteur vide si le contexte n'est pas injecté
         *       ou si aucune entité ne correspond au tag.
         */
        NkVector<NkGameObject> NkSceneScript::FindGameObjectsWithTag(const char* tag) const {
            NkVector<NkGameObject> result;
            
            // Guard : contexte non injecté → retour vide immédiat
            if (!mWorld || !tag) {
                return result;
            }
            
            // Query sur toutes les entités avec un composant NkTag
            // Note : const_cast nécessaire car Query() n'est pas const dans NkWorld
            const_cast<NkWorld*>(mWorld)
                ->Query<NkTag>()
                .ForEach([&](NkEntityId id, NkTag& t) {
                    // Comparaison exacte case-sensitive via NkTag::Has()
                    if (t.Has(tag)) {
                        // Construction du handle GameObject et ajout au résultat
                        result.PushBack(NkGameObject(id, mWorld));
                    }
                });
            
            return result;
        }

        /**
         * @brief Trouve le premier GameObject dont le nom correspond exactement.
         * @param name Nom à rechercher (comparaison case-sensitive via strcmp).
         * @return NkGameObject trouvé, ou handle invalide si non trouvé.
         * 
         * @note Retourne un handle invalide (IsValid() == false) si :
         *       • Le contexte n'est pas injecté
         *       • Aucune entité ne correspond au nom recherché
         */
        NkGameObject NkSceneScript::FindGameObjectByName(const char* name) const {
            NkGameObject result;  // Handle invalide par défaut
            
            // Guard : contexte non injecté ou nom nul → retour invalide immédiat
            if (!mWorld || !name) {
                return result;
            }
            
            // Query sur toutes les entités avec un composant NkName
            const_cast<NkWorld*>(mWorld)
                ->Query<NkName>()
                .ForEach([&](NkEntityId id, NkName& n) {
                    // Comparaison exacte via strcmp (case-sensitive)
                    // Optimisation : arrêt dès le premier match trouvé
                    if (!result.IsValid() && NkStrEqual(n.Get(), name)) {
                        result = NkGameObject(id, mWorld);
                    }
                });
            
            return result;
        }

        // =====================================================================
        // 🎮 Helpers de spawn et destruction
        // =====================================================================

        /**
         * @brief Instancie un prefab dans la scène avec position optionnelle.
         * @param prefab Référence au prefab à instancier.
         * @param position Position mondiale initiale (par défaut : origine).
         * @param name Nom optionnel pour l'instance créée (par défaut : "Spawned").
         * @return NkGameObject de l'instance créée.
         * 
         * @note Retourne un handle invalide si :
         *       • Le contexte n'est pas injecté
         *       • Le prefab est invalide ou non enregistré
         *       • L'instanciation échoue pour une raison interne
         */
        NkGameObject NkSceneScript::SpawnPrefab(const NkPrefab& prefab,
                                                    const NkMath::NkVec3f& position,
                                                    const char* name) {
            // Handle de résultat invalide par défaut (en cas d'erreur)
            NkGameObject result;
            
            // Guards : contexte ou prefab invalide → retour immédiat
            if (!mWorld || !prefab.IsValid()) {
                return result;
            }
            
            // Nom par défaut si non spécifié
            const char* instanceName = name ? name : "Spawned";
            
            // Instanciation via la méthode du prefab
            result = prefab.Instantiate(*mWorld, instanceName);
            
            // Application de la position si l'instance est valide et a un root component
            if (result.IsValid()) {
                // Accès au transform via l'API GameObject (plus haut niveau)
                result.SetPosition(position.x, position.y, position.z);
            }
            
            return result;
        }

        /**
         * @brief Détruit un GameObject de façon sécurisée.
         * @param go Référence au GameObject à détruire.
         * 
         * @note Appel direct à go.Destroy() — destruction immédiate.
         *       Ne pas utiliser pendant l'itération sur une query ECS.
         */
        void NkSceneScript::DestroyGameObject(NkGameObject& go) {
            // Délègue à la méthode Destroy() du GameObject
            // La responsabilité de vérifier IsValid() incombe à l'appelant
            go.Destroy();
        }
    }
} // namespace nkentseu