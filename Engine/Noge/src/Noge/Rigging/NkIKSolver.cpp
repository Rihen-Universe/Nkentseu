// =============================================================================
// Noge/Rigging/NkIKSolver.cpp — pont ECS -> renderer::NkIKSystem
// -----------------------------------------------------------------------------
// Voir NkIKSolver.h pour le design (adaptateur fin, zéro réimplémentation des
// algorithmes FABRIK/CCD/TwoBone/Spline). Ce fichier ne contient QUE du
// marshalling ECS <-> NKRenderer (FK local->monde, construction de
// NkIKChainDesc, write-back monde->local) — la résolution numérique elle-même
// vit dans Kernel/Runtime/NKRenderer/src/NKRenderer/Tools/IK/NkIKSystem.cpp.
// =============================================================================
#include "NkIKSolver.h"
#include <cmath>

namespace nkentseu {

	// -------------------------------------------------------------------------
	// Cycle de vie
	// -------------------------------------------------------------------------
	NkIKSolver::NkIKSolver() noexcept {
		// device=nullptr, animSys=nullptr : NkIKSystem::Init se contente de les
		// stocker (aucun accès GPU réel dans les solveurs FABRIK/CCD/TwoBone,
		// cf. NkIKSystem.cpp) -- CPU-only, safe.
		mSystem.Init(nullptr, nullptr);
	}

	// -------------------------------------------------------------------------
	// Clé de rig : un ecs::NkSkeleton n'a pas d'id NKRenderer stable, on utilise
	// son adresse mémoire comme clé opaque (le rig vit tant que le solveur ET
	// le squelette existent -- cohérent avec l'usage attendu : un NkIKSolver
	// membre d'un système, réutilisé chaque frame pour les mêmes entités).
	// -------------------------------------------------------------------------
	uint64 NkIKSolver::SkeletonKey(const ecs::NkSkeleton &sk) noexcept {
		return reinterpret_cast<uint64>(&sk);
	}

	renderer::NkIKRig *NkIKSolver::GetOrCreateRig(const ecs::NkSkeleton &sk) noexcept {
		return mSystem.CreateRig(SkeletonKey(sk));
	}

	// -------------------------------------------------------------------------
	// FK local -> monde. Suppose les os stockés PARENT AVANT ENFANT (convention
	// habituelle des exporteurs glTF/FBX et de NkAnimationClip::jointTopo côté
	// NKRenderer) ; un parent mal ordonné (ou -1) traite l'os comme racine --
	// dégradation silencieuse, pas un crash, documentée en tête de fichier.
	// -------------------------------------------------------------------------
	void NkIKSolver::BuildWorldPose(const ecs::NkSkeleton &sk, NkVector<NkMat4f> &outWorld) noexcept {
		outWorld.Clear();
		const uint32 n = sk.boneCount;
		if (n == 0)
			return;
		outWorld.Reserve(n);
		for (uint32 i = 0; i < n; ++i)
			outWorld.PushBack(NkMat4f::Identity());

		for (uint32 i = 0; i < n; ++i) {
			const ecs::NkBone &b = sk.bones[i];
			const NkMat4f local =
				NkMat4f::Translate(b.localPosition) * b.localRotation.ToMat4() * NkMat4f::Scale(b.localScale);
			const int32 parent = b.parent;
			outWorld[i] = (parent >= 0 && (uint32)parent < i) ? (outWorld[(uint32)parent] * local) : local;
		}
	}

	// -------------------------------------------------------------------------
	// Écrit le résultat MONDE résolu par NkIKSystem dans les os de la chaîne
	// (local = inverse(monde parent) * monde résolu), puis recalcule
	// skinMatrices pour TOUT le squelette (monde résolu × inverseBindPose --
	// worldAfter contient la pose complète, chaîne + reste inchangé en
	// pass-through, donc le skin final reste cohérent même sans re-FK des os
	// hors chaîne -- limitation assumée, voir tête de fichier).
	// -------------------------------------------------------------------------
	void NkIKSolver::WriteBackChain(ecs::NkSkeleton &sk, const NkVector<uint32> &boneIdx, const NkMat4f *worldAfter,
									uint32 worldAfterCount) noexcept {
		if (!worldAfter || worldAfterCount == 0)
			return;

		for (uint32 k = 0; k < (uint32)boneIdx.Size(); ++k) {
			const uint32 bi = boneIdx[k];
			if (bi >= sk.boneCount || bi >= worldAfterCount)
				continue;
			const int32 parent = sk.bones[bi].parent;
			const NkMat4f local = (parent >= 0 && (uint32)parent < worldAfterCount)
									  ? (worldAfter[(uint32)parent].Inverse() * worldAfter[bi])
									  : worldAfter[bi];

			NkVec3f t, s;
			NkMat4f rot;
			local.DecomposeTRS(t, rot, s);
			sk.bones[bi].localPosition = t;
			sk.bones[bi].localRotation = NkQuatf(rot);
			sk.bones[bi].localScale = s;
		}

		const uint32 count = (sk.boneCount < worldAfterCount) ? sk.boneCount : worldAfterCount;
		for (uint32 i = 0; i < count; ++i)
			sk.skinMatrices[i] = worldAfter[i] * sk.bones[i].inverseBindPose;
	}

	// -------------------------------------------------------------------------
	// Coeur partagé : construit la pose monde, (re)crée la chaîne côté
	// NkIKRig, délègue à NkIKSystem::SolveRig, écrit le résultat.
	// -------------------------------------------------------------------------
	uint32 NkIKSolver::SolveByDesc(ecs::NkSkeleton &skeleton, const NkVector<uint32> &boneIndices,
									const NkVec3f &targetPos, const NkQuatf &targetRot, bool matchRotation,
									const NkVec3f &poleVector, bool usePole, float32 weight, uint32 maxIterations,
									float32 tolerance, renderer::NkIKSolver solverKind) noexcept {
		if (boneIndices.Size() < 2)
			return 0;

		NkVector<NkMat4f> world;
		BuildWorldPose(skeleton, world);
		if (world.Empty())
			return 0;

		renderer::NkIKRig *rig = GetOrCreateRig(skeleton);
		if (!rig)
			return 0;
		rig->SetWorldPose(world.Data(), (uint32)world.Size());

		// Nom déterministe (root/tip) : retrouve la même NkIKChainDesc d'un
		// appel à l'autre au lieu d'en empiler une nouvelle par frame. On la
		// SUPPRIME puis la RECRÉE (au lieu d'un simple SetTarget) car la
		// composition de la chaîne (bones/solveur) peut changer d'un appel à
		// l'autre côté appelant -- coût modéré (petites chaînes, 2-4 os), même
		// esprit que le round-trip ToPolygons/BuildFromPolygons de NkEditableMesh.
		const NkString name =
			NkString::Format("chain_%u_%u", boneIndices[0], boneIndices[(uint32)boneIndices.Size() - 1]);
		const renderer::NkIKChainId existing = rig->FindChain(name);
		if (existing.IsValid())
			rig->RemoveChain(existing);

		renderer::NkIKChainDesc desc;
		desc.name = name;
		desc.solver = solverKind;
		desc.maxIterations = maxIterations;
		desc.tolerance = tolerance;
		desc.enabled = true;
		desc.target.position = targetPos;
		desc.target.rotation = targetRot;
		desc.target.weight = weight;
		desc.target.matchRotation = matchRotation;
		desc.target.poleVector = poleVector;
		desc.target.usePole = usePole;
		for (uint32 i = 0; i < (uint32)boneIndices.Size(); ++i) {
			renderer::NkIKBone b;
			b.boneIdx = boneIndices[i];
			b.length = 0.f; // auto-calculée par NkIKSystem depuis la pose monde
			desc.bones.PushBack(b);
		}

		const renderer::NkIKChainId id = rig->AddChain(desc);
		rig->SetWeight(id, weight);

		mSystem.SolveRig(rig, 1.f / 60.f); // dt non utilisé par FABRIK/CCD/TwoBone (position-based)

		WriteBackChain(skeleton, boneIndices, rig->GetBoneMatrices(), rig->GetBoneMatrixCount());

		// NkIKSystem ne remonte pas le nombre d'itérations réellement
		// effectuées (early-exit interne sur la tolérance) -- on renvoie le
		// plafond configuré, cohérent avec l'ancienne signature mais pas une
		// mesure exacte (voir NkIKSolver.h).
		return maxIterations;
	}

	// -------------------------------------------------------------------------
	// API publique — délégation pure
	// -------------------------------------------------------------------------
	uint32 NkIKSolver::SolveFABRIK(ecs::NkSkeleton &skeleton, const Chain &chain) noexcept {
		return SolveByDesc(skeleton, chain.boneIndices, chain.targetPosition, chain.targetRotation,
						   chain.constrainTip, {}, false, chain.weight, chain.maxIterations, chain.tolerance,
						   renderer::NkIKSolver::NK_FABRIK);
	}

	uint32 NkIKSolver::SolveCCD(ecs::NkSkeleton &skeleton, const Chain &chain) noexcept {
		return SolveByDesc(skeleton, chain.boneIndices, chain.targetPosition, chain.targetRotation,
						   chain.constrainTip, {}, false, chain.weight, chain.maxIterations, chain.tolerance,
						   renderer::NkIKSolver::NK_CCD);
	}

	void NkIKSolver::SolveTwoBone(ecs::NkSkeleton &skeleton, const TwoBoneChain &chain) noexcept {
		NkVector<uint32> bones;
		bones.PushBack(chain.rootBone);
		bones.PushBack(chain.midBone);
		bones.PushBack(chain.tipBone);
		SolveByDesc(skeleton, bones, chain.target, NkQuatf::Identity(), false, chain.poleTarget, chain.usePole,
					chain.weight, 10u, 0.0005f, renderer::NkIKSolver::NK_TWO_BONE);
	}

	void NkIKSolver::SolveSpline(ecs::NkSkeleton &skeleton, const SplineChain &chain) noexcept {
		// Délégation honnête vers un chemin encore STUB côté NKRenderer (voir
		// tête de fichier) : on construit quand même la chaîne réelle pour que
		// le comportement suive exactement NkIKSystem, y compris le jour où
		// SolveChain_Spline y sera implémenté -- zéro logique de spline locale.
		if (chain.boneIndices.Empty())
			return;
		const NkVec3f target =
			chain.splinePoints.Empty() ? NkVec3f{} : chain.splinePoints[(uint32)chain.splinePoints.Size() - 1];
		SolveByDesc(skeleton, chain.boneIndices, target, NkQuatf::Identity(), false, {}, false, chain.weight, 10u,
					0.001f, renderer::NkIKSolver::NK_SPLINE);
	}

	// -------------------------------------------------------------------------
	// Utilitaires géométriques (pas de l'IK -- pas de délégation nécessaire)
	// -------------------------------------------------------------------------
	float32 NkIKSolver::ChainLength(const ecs::NkSkeleton &skeleton, const NkVector<uint32> &boneIndices) const
		noexcept {
		NkVector<NkMat4f> world;
		BuildWorldPose(skeleton, world);
		float32 total = 0.f;
		for (uint32 i = 1; i < (uint32)boneIndices.Size(); ++i) {
			const uint32 a = boneIndices[i - 1], b = boneIndices[i];
			if (a >= (uint32)world.Size() || b >= (uint32)world.Size())
				continue;
			const NkVec3f pa = {world[a].position.x, world[a].position.y, world[a].position.z};
			const NkVec3f pb = {world[b].position.x, world[b].position.y, world[b].position.z};
			const NkVec3f d = {pb.x - pa.x, pb.y - pa.y, pb.z - pa.z};
			total += sqrtf(d.x * d.x + d.y * d.y + d.z * d.z);
		}
		return total;
	}

	bool NkIKSolver::IsReachable(const ecs::NkSkeleton &skeleton, const NkVector<uint32> &boneIndices,
								 const NkVec3f &target) const noexcept {
		if (boneIndices.Empty())
			return false;
		NkVector<NkMat4f> world;
		BuildWorldPose(skeleton, world);
		const uint32 root = boneIndices[0];
		if (root >= (uint32)world.Size())
			return false;
		const NkVec3f rp = {world[root].position.x, world[root].position.y, world[root].position.z};
		const NkVec3f d = {target.x - rp.x, target.y - rp.y, target.z - rp.z};
		const float32 dist = sqrtf(d.x * d.x + d.y * d.y + d.z * d.z);
		return dist <= ChainLength(skeleton, boneIndices) + 1e-4f;
	}

} // namespace nkentseu
