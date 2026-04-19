#pragma once
// =============================================================================
// Nkentseu/ECS/Entities/NkActor.h — Hiérarchie d'acteurs style Unreal
// =============================================================================
/**
 * @file NkActor.h
 * @brief Classes d'acteurs avec cycle de vie complet (BeginPlay/Tick/EndPlay).
 *
 * 🔹 HIÉRARCHIE :
 *   NkGameObject (handle ECS léger)
 *     └── NkActor (+ lifecycle BeginPlay/Tick/EndPlay)
 *           └── NkPawn (+ possession par controller)
 *                 └── NkCharacter (+ CharacterController/Animator/Audio)
 *
 * 🔹 COMPOSANTS SPÉCIALISÉS :
 *   NkCharacter ajoute automatiquement dans BeginPlay() :
 *   - NkCharacterController (via HasComponent/AddComponent)
 *   - NkSkeletalMesh
 *   - NkAnimator
 *   - NkAudioSource
 *
 * 🔹 CYCLE DE VIE :
 *   Appelé automatiquement par NkSceneLifecycleSystem (via NkScheduler).
 *   L'utilisateur ne doit PAS appeler ces méthodes directement.
 */

#include "NkGameObject.h"
#include "Nkentseu/ECS/Components/Physics/NkPhysicsComponents.h"
#include "Nkentseu/ECS/Components/Animation/NkAnimation.h"
#include "Nkentseu/ECS/Components/Audio/NkAudioComponents.h"

namespace nkentseu {
    namespace ecs {

        // =====================================================================
        // NkActor — Acteur de base avec cycle de vie
        // =====================================================================
        class NkActor : public NkGameObject {
            public:
                using NkGameObject::NkGameObject;
                virtual ~NkActor() noexcept = default;

                // ── Cycle de vie (géré par NkSceneLifecycleSystem) ────────
                /**
                 * @brief Appelé une fois avant la première frame.
                 * Surcharger pour initialiser les composants et la logique.
                 */
                virtual void BeginPlay() noexcept {}

                /**
                 * @brief Appelé chaque frame (delta-time variable).
                 * Surcharger pour la logique gameplay principale.
                 */
                virtual void Tick(float32 dt) noexcept { (void)dt; }

                /**
                 * @brief Appelé après Tick(), avant le rendu.
                 * Surcharger pour les caméras, l'UI dépendant du mouvement.
                 */
                virtual void LateTick(float32 dt) noexcept { (void)dt; }

                /**
                 * @brief Appelé à intervalle fixe (physique déterministe).
                 */
                virtual void FixedTick(float32 fixedDt) noexcept { (void)fixedDt; }

                /**
                 * @brief Appelé au déchargement de la scène.
                 */
                virtual void EndPlay() noexcept {}

                // ── Initialisation des composants de base ─────────────────
                /**
                 * @brief Garantit que les composants minimaux sont présents.
                 * Appelé automatiquement par NkGameObjectFactory.
                 * Peut être surchargé pour ajouter des composants spécifiques.
                 */
                virtual void SpawnDefaultComponents() noexcept {
                    if (!HasComponent<NkTransform>())  AddComponent<NkTransform>();
                    if (!HasComponent<NkTag>())         AddComponent<NkTag>();
                    if (!HasComponent<NkName>())        AddComponent<NkName>("UnnamedActor");
                }
        };

        // =====================================================================
        // NkPawn — Acteur contrôlable (joueur, IA, véhicule)
        // =====================================================================
        class NkPawn : public NkActor {
            public:
                using NkActor::NkActor;
                virtual ~NkPawn() noexcept = default;

                [[nodiscard]] bool   IsPossessed()    const noexcept { return mPossessed; }
                [[nodiscard]] uint32 GetControllerId() const noexcept { return mControllerId; }

                void Possess(uint32 controllerId = 0) noexcept {
                    mPossessed = true;
                    mControllerId = controllerId;
                }

                void Unpossess() noexcept {
                    mPossessed = false;
                    mControllerId = 0;
                }

            protected:
                bool   mPossessed     = false;
                uint32 mControllerId  = 0;
        };

        // =====================================================================
        // NkCharacter — Personnage bipède (spécialisation de NkPawn)
        // =====================================================================
        /**
         * @class NkCharacter
         * @brief Acteur personnage avec controller, squelette, animation et audio.
         *
         * 🔹 Composants auto-ajoutés dans BeginPlay() :
         *   - NkCharacterController  (mouvement physique du personnage)
         *   - NkSkeletalMesh         (mesh avec squelette pour l'animation)
         *   - NkAnimator             (machine à états d'animation)
         *   - NkAudioSource          (source audio du personnage)
         *
         * 🔹 Utilisation de NkComponentHandle :
         *   Les composants sont ajoutés via AddComponent<T>() qui retourne
         *   un NkComponentHandle<T>. L'accès ultérieur se fait via GetComponent<T>().
         *
         * @code
         * class MyHero : public NkCharacter {
         * public:
         *     void BeginPlay() noexcept override {
         *         NkCharacter::BeginPlay();  // Ajoute controller/mesh/animator/audio
         *         // Configurer l'animator
         *         if (auto anim = GetComponent<NkAnimator>()) {
         *             anim->SetDefaultClip("Idle");
         *         }
         *     }
         *     void Tick(float32 dt) noexcept override {
         *         // Mouvement via le controller
         *         if (auto ctrl = GetComponent<NkCharacterController>()) {
         *             ctrl->Move(GetInputDirection() * mSpeed * dt);
         *         }
         *     }
         * };
         * @endcode
         */
        class NkCharacter : public NkPawn {
            public:
                using NkPawn::NkPawn;
                virtual ~NkCharacter() noexcept = default;

                void BeginPlay() noexcept override {
                    NkPawn::BeginPlay();

                    // Ajoute les composants spécialisés si absents
                    if (!HasComponent<NkCharacterController>())
                        AddComponent<NkCharacterController>();
                    if (!HasComponent<NkSkeletalMesh>())
                        AddComponent<NkSkeletalMesh>();
                    if (!HasComponent<NkAnimator>())
                        AddComponent<NkAnimator>();
                    if (!HasComponent<NkAudioSource>())
                        AddComponent<NkAudioSource>();
                }
        };

    } // namespace ecs
} // namespace nkentseu

// =============================================================================
// EXEMPLES D'UTILISATION
// =============================================================================
/*
// Exemple 1 : Acteur générique
void Example_Actor(nkentseu::ecs::NkWorld& world) {
    // Via NkGameObjectFactory (la bonne façon)
    auto crate = nkentseu::NkGameObjectFactory::Create<nkentseu::ecs::NkActor>(world, "Prop_Crate");
    crate.BeginPlay();
    crate.SetPosition({0, 1, 0});
    crate.GetComponent<nkentseu::ecs::NkTag>()->Add(NkTagBit::Static);
}

// Exemple 2 : Personnage
class MyPlayer : public nkentseu::ecs::NkCharacter {
public:
    float32 mSpeed = 5.f;

    void BeginPlay() noexcept override {
        NkCharacter::BeginPlay();  // Ajoute controller/mesh/animator/audio
        SetName("Player_01");
    }

    void Tick(float32 dt) noexcept override {
        // Utilise NkComponentHandle pour accéder au controller
        if (auto ctrl = GetComponent<nkentseu::ecs::NkCharacterController>()) {
            // ctrl->Move(inputDir * mSpeed * dt);
        }

        // Utilise NkRequiredComponent pour les invariants (pas de nullptr check)
        auto& transform = RequireComponent<nkentseu::ecs::NkTransform>().Get();
        // transform.localPosition.y += 0.f;  // toujours valide

        // Utilise Optional pour les composants vraiment optionnels
        Optional<nkentseu::ecs::NkAudioSource>()->SetVolume(0.8f);  // no-op si absent
    }
};
*/
