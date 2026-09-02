#pragma once
// -----------------------------------------------------------------------------
// FICHIER: NKAnima/NkSkeletonDef.h
// DESCRIPTION: L'ACTIF SQUELETTE PARTAGE — la definition, sans le par-instance.
//
//   NkBoneDef      — la definition d'un os, invariante entre les instances
//   NkSkeletonDef  — l'actif partage (l'equivalent d'un USkeleton)
//
// =============================================================================
//  POURQUOI CE TYPE VIT ICI, ET PAS DANS NOGE (descendu le 2026-09-02)
// =============================================================================
//  Il vivait dans `Engine/Noge/.../Animation/NkAnimation.h`, en `nkentseu::ecs`.
//  Descendu dans NKAnima sur decision de Rodolf : c'est un ACTIF D'ANIMATION,
//  pas un composant d'ECS. Sa place est aupres du reciblage
//  (`NkAnimRetarget`) et du melange (`NkBlendTree1D/2D`), qui vivaient deja ici.
//
//  Consequence utile : NKAnima est **pur Foundation, sans GPU**. Un outil 2D,
//  PV3DE ou NKScena peuvent donc decrire un squelette sans tirer le renderer
//  ni l'ECS.
//
//  ⚠️ CE QUI RESTE DANS NOGE, ET POURQUOI. `NkBonePose` (la pose locale, animee
//  chaque image) et `NkSkeleton` (le composant, copiable par l'ECS) restent
//  cote Noge : ce sont du PAR-INSTANCE et un composant, pas un actif. La
//  frontiere est celle d'Unreal, et c'est elle qui a fait passer le composant
//  de 77 064 a 88 octets — la separer autrement la reintroduirait.
//
//  ⚠️ ET ON N'UNIFIE PAS AVEC `NkRetargetSkeleton`, qui vit dans ce meme
//  espace de noms. Mesure du 2026-09-02 : il stocke `parent` + `bindLocal`
//  (LOCAL, relatif au parent) + `names` + `topo` ; celui-ci stocke des matrices
//  bind / inverse-bind. **Ce ne sont pas deux versions d'une meme chose, ce
//  sont deux structures differentes** — l'une decrit un squelette A RECIBLER,
//  l'autre un squelette A PEAUFINER POUR LE GPU. Chacune est dans la forme qui
//  sert son usage. La question de les unifier est posee a Rodolf (bloc 11 de
//  `Engine/Noge/DECISIONS_RODOLF.md`) ; tant qu'elle n'est pas tranchee, elles
//  coexistent, voisines et distinctes.
// -----------------------------------------------------------------------------

#include "NKContainers/Sequential/NkVector.h"
#include "NKMath/NKMath.h"

#include <cstring>

namespace nkentseu {
	namespace anim {

		// La DEFINITION d'un os — invariante entre toutes les instances.
		// ⚠️ Aucune pose ici : c'est elle qui rendait le partage impossible.
		struct NkBoneDef {
				static constexpr uint32 kMaxBoneNameLen = 64u;
				char name[kMaxBoneNameLen] = {};
				int32 parent = -1;					  // index du parent (-1 = racine)
				math::NkMat4f bindPose = math::NkMat4f::Identity(); // pose de repos (bind pose)
				math::NkMat4f inverseBindPose = math::NkMat4f::Identity();
		};

		// L'ACTIF PARTAGE. Les instances le referencent par NkSharedPtr ; le
		// modifier apres creation modifierait TOUTES les instances — c'est le
		// contrat d'un USkeleton, on ne le cache pas.
		struct NkSkeletonDef {
				NkVector<NkBoneDef> bones; // dimensionne au reel, aucun plafond
				char path[256] = {};	   // provenance (ex-skeletonPath)

				[[nodiscard]] int32 FindBone(const char *name) const noexcept {
					for (uint32 i = 0; i < (uint32)bones.Size(); ++i)
						if (std::strcmp(bones[(NkVector<NkBoneDef>::SizeType)i].name, name) == 0)
							return static_cast<int32>(i);
					return -1;
				}
		};

	} // namespace anim
} // namespace nkentseu
