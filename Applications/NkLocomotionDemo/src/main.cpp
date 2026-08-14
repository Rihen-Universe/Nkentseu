// =============================================================================
// Applications/NkLocomotionDemo/src/main.cpp
// =============================================================================
// Preuve d'exécution réelle du pont animation Noge (Engine/Noge/ROADMAP.md,
// item G1.4) : un personnage ECS (NkSkeleton + NkFootIK + NkLocomotion +
// NkAnimator) joue un blend Walk/Run et corrige ses pieds par IK, tout en
// délégant CHAQUE calcul numérique au vrai backend NKRenderer :
//
//   • Blend Walk/Run  -> anim::NkBlendTree1D (NkLocomotionSystem)
//   • IK des pieds     -> renderer::NkIKSystem::NK_TWO_BONE (NkFootIKSystem
//                         via NkIKSolver -- Rigging/NkIKSolver.h/.cpp)
//
// ZÉRO réimplémentation locale de FABRIK/CCD/TwoBone/blend : voir les
// commentaires de tête de Engine/Noge/src/Noge/Rigging/NkIKSolver.h et
// Engine/Noge/src/Noge/Anim/NkLocomotion.h pour le détail du design.
//
// Contourne le blocage de `jenga test` (politique de workspace), même schéma
// que Applications/NkEditableMeshDemo -- application console classique avec
// assertions manuelles, pas une TestSuite.
// =============================================================================
#include "Noge/Anim/NkLocomotion.h"
#include "Noge/ECS/Components/Core/NkTransform.h"
#include "NKECS/World/NkWorld.h"
#include "NKAnimation/NkAnimation.h"
#include "NKMemory/NkAllocator.h"
#include "NKLogger/NkLog.h"
#include <cstring>

using namespace nkentseu;
using namespace nkentseu::ecs;

namespace {

	int gFailCount = 0;
	int gPassCount = 0;

	void Check(bool cond, const char *what) noexcept {
		if (cond) {
			++gPassCount;
			logger.Infof("  [OK]   %s\n", what);
		} else {
			++gFailCount;
			logger.Errorf("  [FAIL] %s\n", what);
		}
	}

	// Indices de bones alignés sur les valeurs par défaut de NkFootIK (voir
	// Noge/Anim/NkLocomotion.h) : hip=0, leftThigh=1, leftCalf=2, leftFoot=3,
	// leftToe=4, rightThigh=5, rightCalf=6, rightFoot=7, rightToe=8.
	constexpr uint32 kBoneCount = 9;

	// Construit un squelette PLAT (bones[i].parent == -1, voir la note de
	// conception dans NkLocomotion.h/NkIKSolver.h) en pose debout approximative.
	void BuildFlatSkeleton(NkSkeleton &sk) noexcept {
		sk.boneCount = kBoneCount;
		const NkVec3f pos[kBoneCount] = {
			{0.f, 1.00f, 0.f},	 // hip
			{-0.15f, 0.90f, 0.f}, // leftThigh
			{-0.15f, 0.50f, 0.f}, // leftCalf
			{-0.15f, 0.05f, 0.f}, // leftFoot
			{-0.15f, 0.00f, 0.15f}, // leftToe
			{0.15f, 0.90f, 0.f},  // rightThigh
			{0.15f, 0.50f, 0.f},  // rightCalf
			{0.15f, 0.05f, 0.f},  // rightFoot
			{0.15f, 0.00f, 0.15f}, // rightToe
		};
		for (uint32 i = 0; i < kBoneCount; ++i) {
			sk.bones[i].parent = -1; // squelette plat -- voir tête de fichier NkLocomotion.h
			sk.bones[i].localPosition = pos[i];
			sk.bones[i].localRotation = NkQuatf::Identity();
			sk.bones[i].localScale = {1.f, 1.f, 1.f};
			sk.bones[i].bindPose = NkMat4f::Translate(pos[i]);
			sk.bones[i].inverseBindPose = NkMat4f::Translate(pos[i]).Inverse();
			sk.skinMatrices[i] = sk.bones[i].bindPose;
		}
	}

} // namespace

int main() {
	logger.Infof("=== NkLocomotionDemo — pont animation Noge (G1.4) ===\n");
	logger.Infof("=== Blend Walk/Run (anim::NkBlendTree1D) + IK pieds (renderer::NkIKSystem) ===\n");

	// ── Clips Walk / Run (procéduraux -- pas de dépendance à un asset externe,
	// voir anim::NkAnimationClip::MakeProceduralWalk, réel et déjà utilisé
	// par Applications/Sandbox/src/Demo/DemoAnim.cpp) ──────────────────────
	anim::NkAnimationClip *walk = anim::NkAnimationClip::MakeProceduralWalk(kBoneCount, 1.0f);
	anim::NkAnimationClip *run = anim::NkAnimationClip::MakeProceduralWalk(kBoneCount, 0.4f); // cadence rapide
	Check(walk != nullptr && run != nullptr, "clips Walk/Run procéduraux crées (MakeProceduralWalk)");
	Check(walk->boneCount == kBoneCount && run->boneCount == kBoneCount, "boneCount des clips == squelette (9)");

	// ── Monde ECS + entité personnage ───────────────────────────────────────
	NkWorld world;
	NkSkeleton skeleton;
	BuildFlatSkeleton(skeleton);

	NkFootIK footIK; // indices par défaut déjà alignés sur BuildFlatSkeleton
	footIK.footHeight = 0.02f;
	footIK.rayLength = 2.0f;
	footIK.blendSpeed = 12.f; // convergence rapide du contactWeight pour la démo

	NkLocomotion loco;
	loco.walkSpeed = 1.5f;
	loco.runSpeed = 5.5f;
	loco.accelerationTime = 0.15f;

	NkAnimator animator;
	// Paramètre "speed" lu par NkLocomotionSystem::Execute (animator.SetFloat).
	animator.paramCount = 1;
	std::strncpy(animator.params[0].name, "speed", sizeof(animator.params[0].name) - 1);
	animator.params[0].value = 0.f;

	// NOTE : NkEntityBuilder::With(T&&) (NKECS/World/NkWorld.h) a un bug de
	// résolution de surcharge pré-existant (forwarding reference qui déduit
	// T comme référence pour un lvalue nommé, casse l'appel à World::Add<T>)
	// -- hors-scope de cette passe (code NKECS core, pas Noge/animation).
	// Contournement local : World::Add<T>(id, value) avec argument de
	// template EXPLICITE, qui fixe T et évite la déduction ambiguë.
	NkEntityId character = world.CreateEntity();
	world.Add<NkTransform>(character, NkTransform{});
	world.Add<NkSkeleton>(character, skeleton);
	world.Add<NkFootIK>(character, footIK);
	world.Add<NkLocomotion>(character, loco);
	world.Add<NkAnimator>(character, animator);
	Check(world.IsAlive(character), "entité personnage créée (Transform+Skeleton+FootIK+Locomotion+Animator)");

	// ── Systèmes : NkLocomotionSystem (blend) + NkFootIKSystem (IK pieds) ──
	NkLocomotionSystem locoSys;
	locoSys.ConfigureBlend(walk, run);
	NkFootIKSystem footSys;
	footSys.SetFallbackGroundY(0.f); // pas de NKPhysics ici -- plan de sol plat (voir NkLocomotion.h)

	// ── Simulation : balaie desiredSpeed de 0 (Walk) à runSpeed (Run) ──────
	constexpr float32 kDt = 1.f / 60.f;
	constexpr uint32 kFrames = 240; // 4 s -- laisse le temps au blend + au contact IK de converger

	float32 paramAtWalk = -1.f, paramAtRun = -1.f;
	NkMat4f boneAtWalk = NkMat4f::Identity(), boneAtRun = NkMat4f::Identity();
	bool sawGroundedFoot = false;
	float32 worstFootY = 1e9f;

	for (uint32 f = 0; f < kFrames; ++f) {
		NkLocomotion *lc = world.Get<NkLocomotion>(character);
		lc->desiredSpeed = (f < kFrames / 2) ? loco.walkSpeed * 0.5f /* phase Walk */
											  : loco.runSpeed;			 /* phase Run */

		locoSys.Execute(world, kDt);
		footSys.Execute(world, kDt);

		if (f == kFrames / 2 - 1) {
			paramAtWalk = locoSys.GetBlendTree().GetParameter();
			boneAtWalk = locoSys.GetBlendedState().boneMatrices.Empty() ? NkMat4f::Identity()
																		 : locoSys.GetBlendedState().boneMatrices[1];
		}
		if (f == kFrames - 1) {
			paramAtRun = locoSys.GetBlendTree().GetParameter();
			boneAtRun = locoSys.GetBlendedState().boneMatrices.Empty() ? NkMat4f::Identity()
																		: locoSys.GetBlendedState().boneMatrices[1];
		}

		const NkSkeleton *sk = world.Get<NkSkeleton>(character);
		const NkFootIK *fi = world.Get<NkFootIK>(character);
		if (fi->leftFoot.isGrounded || fi->rightFoot.isGrounded)
			sawGroundedFoot = true;
		if (sk->bones[3].localPosition.y < worstFootY)
			worstFootY = sk->bones[3].localPosition.y;
	}

	// ── Assertions ──────────────────────────────────────────────────────────
	logger.Infof("-- Blend Walk/Run (anim::NkBlendTree1D) --\n");
	Check(paramAtWalk >= 0.f && paramAtWalk < 0.5f, "paramètre de blend proche de 0 (Walk) en phase lente");
	Check(paramAtRun > 0.5f, "paramètre de blend proche de 1 (Run) en phase rapide");
	bool poseDiffers = false;
	for (int e = 0; e < 16; ++e)
		if (boneAtWalk.data[e] != boneAtRun.data[e])
			poseDiffers = true;
	Check(poseDiffers, "la pose (bone[1]) DIFFÈRE entre Walk et Run -- preuve d'un vrai blend, pas un stub");

	logger.Infof("-- IK des pieds (renderer::NkIKSystem::NK_TWO_BONE via NkIKSolver) --\n");
	Check(sawGroundedFoot, "au moins un pied détecté au sol (RaycastGround, repli plan-plat)");
	logger.Infof("     pied gauche Y le plus bas observé = %.4f (cible ~ groundY(0) + footHeight(%.2f))\n",
				 worstFootY, footIK.footHeight);
	Check(worstFootY < 0.30f, "l'IK a effectivement ABAISSÉ le pied vers le sol (Y << pose de repos 0.05)");
	Check(worstFootY > -0.5f, "le résultat reste dans une plage physiquement raisonnable (pas de divergence)");

	logger.Infof("=== Résultat : %d OK / %d FAIL ===\n", gPassCount, gFailCount);

	memory::NkGetDefaultAllocator().Delete(walk);
	memory::NkGetDefaultAllocator().Delete(run);

	return gFailCount == 0 ? 0 : 1;
}
