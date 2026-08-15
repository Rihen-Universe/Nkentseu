#pragma once
// =============================================================================
// NKRenderer/Tools/Animation/NkAnimationSystem.h
// -----------------------------------------------------------------------------
// FACADE DE RENDU de l'animation. Ce qui reste au renderer apres l'extraction
// du 2026-08-14 : televersement des matrices de skinning, soumission des meshes
// skinnes, pelure d'oignon, compute de morph targets, debug-draw du squelette.
//
// LE MODELE N'EST PLUS ICI. Clips, echantillonnage, lecture, melange 1D/2D et
// machine a etats vivent dans `Kernel/Runtime/NKAnimation` (namespace
// `nkentseu::anim`), qui ne connait que Foundation. Cette classe le CONSOMME.
//
// La frontiere se lit dans les signatures : tout ce qui est prefixe `anim::`
// vient du substrat, tout ce qui ne l'est pas appartient au rendu. C'est le sens
// du bloc de decision « SUBSTRATS ANIMATION ET COMPORTEMENT » (CLAUDE.md du
// repertoire parent) : le renderer redevient consommateur, et son code le dit.
//
// AUTEUR : Rihen — LICENCE : usage regi par le fichier LICENSE a la racine du depot
// =============================================================================
#include "NKAnimation/NkAnimation.h"
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRenderer/Core/NkCamera.h"
#include "NKRenderer/Core/NkRendererConfig.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKContainers/Associative/NkHashMap.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	namespace renderer {

		class NkRender3D;
		class NkMaterialInstance;
		class NkMeshSystem;
		struct NkPostConfig;

		// =========================================================================
		// NkAnimationSystem — gestionnaire central
		// =========================================================================
		class NkAnimationSystem {
			public:
				NkAnimationSystem() = default;
				~NkAnimationSystem();

				bool Init(NkIDevice *device, NkRender3D *r3d);
				void Shutdown();

				// ── Update ────────────────────────────────────────────────────────────
				void Update(float32 dt);

				// ── Clips ─────────────────────────────────────────────────────────────
				anim::NkAnimationClip *CreateClip(const NkString &name);
				anim::NkAnimationClip *FindClip(const NkString &name) const;
				void DestroyClip(anim::NkAnimationClip *&clip);

				// ── Players ───────────────────────────────────────────────────────────
				anim::NkAnimationPlayer *CreatePlayer(const NkString &name = "");
				anim::NkAnimationPlayer *FindPlayer(const NkString &name) const;
				void DestroyPlayer(anim::NkAnimationPlayer *&player);

				// ─────────────────────────────────────────────────────────────────────
				// Application du state au renderer
				// ─────────────────────────────────────────────────────────────────────

				// Skeletal — soumet un mesh skinné
				void ApplySkinnedMesh(NkMeshHandle mesh, NkMatInstHandle mat, const NkMat4f &baseWorld,
									  const anim::NkAnimationPlayer &player);

				// Skeletal — onion skinning autour du frame courant
				void ApplyOnionSkin(NkMeshHandle mesh, NkMatInstHandle mat, const NkMat4f &baseWorld,
									const anim::NkAnimationPlayer &player, const int32 *ghostOffsets, uint32 ghostCount,
									NkVec4f pastColor = {1.f, 0.3f, 0.3f, 0.5f},
									NkVec4f futureColor = {0.3f, 0.3f, 1.f, 0.5f});

				// Transform — calcule la matrice world depuis le state
				NkMat4f ApplyTransform(const anim::NkAnimationPlayer &player, const NkMat4f &base = NkMat4f::Identity());

				// Matériau — écrit les paramètres animés dans une NkMaterialInstance
				void ApplyMaterial(NkMaterialInstance *inst, const anim::NkAnimationPlayer &player);

				// Caméra — applique le state à une NkCamera3D
				void ApplyCamera(NkCamera3D &cam, const anim::NkAnimationPlayer &player);

				// Lumière — applique le state à une NkLightDesc
				void ApplyLight(NkLightDesc &light, const anim::NkAnimationPlayer &player);

				// Post-process — applique le state à un NkPostConfig
				void ApplyPostProcess(NkPostConfig &pp, const anim::NkAnimationPlayer &player);

				// UV — retourne l'UV rect animé {u0,v0,u1,v1}
				NkVec4f GetAnimatedSpriteUV(const anim::NkAnimationPlayer &player);

				// Squelette — dessine le squelette en debug
				void DrawSkeleton(const anim::NkAnimationPlayer &player, const NkMat4f &baseWorld, const int32 *parentIdx,
								  NkVec4f boneColor = {1.f, 0.6f, 0.f, 1.f}, float32 boneSize = 0.05f);

				// Morph targets CPU/GPU
				NkMeshHandle ApplyMorphTargets(NkMeshHandle base, const NkMeshHandle *targets, const float32 *weights,
											   uint32 count, bool useGPU = false);

				uint32 GetPlayerCount() const {
					return (uint32)mPlayers.Size();
				}

			private:
				NkIDevice *mDevice = nullptr;
				NkRender3D *mR3D = nullptr;

				NkVector<anim::NkAnimationClip *> mClips;
				NkVector<anim::NkAnimationPlayer *> mPlayers;
				NkVector<NkMat4f> mTmpBones;
				NkPipelineHandle mMorphCompute;
		};

	} // namespace renderer
} // namespace nkentseu
