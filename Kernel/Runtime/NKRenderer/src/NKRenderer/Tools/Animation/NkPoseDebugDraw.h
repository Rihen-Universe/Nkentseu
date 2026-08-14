// =============================================================================
// NKRenderer/Tools/Animation/NkPoseDebugDraw.h
// -----------------------------------------------------------------------------
// VISUALISATION DEBUG de la physique d'animation M3 (NkAnima). Dessine, via les
// primitives debug de NkRender3D, l'état d'équilibre d'une pose :
//   • le CENTRE DE MASSE (COM, M3.1) — sphère colorée VERT (équilibré) / ROUGE (non),
//   • le POLYGONE DE SUPPORT (M3.2/M3.3) — arêtes + coins,
//   • la PROJECTION du COM sur le sol (fil d'aplomb + cercle),
//   • (option) la direction de bascule.
// Réutilisable : éditeur NkAnima, démos, debug jeu (Noge). Le module de calcul
// (NkPoseMass/NkBalance) reste pur ; SEUL ce helper touche au rendu.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKMath/NKMath.h"
#include "NKAnimPhysics/NkPoseMass.h"

namespace nkentseu {
	namespace renderer {

		class NkRender3D; // fwd

		// Options d'affichage du debug M3.
		struct NkPoseDebugVizOptions {
				bool drawCOM = true;		 // sphère au centre de masse
				bool drawSupport = true;	 // arêtes + coins du polygone de support
				bool drawProjection = true;	 // fil d'aplomb COM→sol + cercle au sol
				bool drawTipArrow = false;	 // flèche de direction de bascule (si comVelocity fourni)
				float32 comRadius = 0.05f;	 // rayon de la sphère COM (mètres)
				float32 supportRadius = 0.02f; // rayon des sphères de coin
		};

		struct NkPoseDebugDraw {
			public:
				// Dessine le debug M3 pour une pose. `jointWorld` = positions monde des joints (count),
				// `mass` = modèle de masse associé, `supportPts` = coins d'appui au sol (supportCount).
				// `comVelocity` (option, pour la flèche de bascule) = vitesse monde du COM.
				static void Draw(NkRender3D &r3d, const math::NkVec3f *jointWorld, int32 count, const NkPoseMass &mass,
								 const math::NkVec3f *supportPts, int32 supportCount,
								 const math::NkVec3f &groundNormal = math::NkVec3f{0.f, 1.f, 0.f},
								 const NkPoseDebugVizOptions &opt = NkPoseDebugVizOptions{},
								 const math::NkVec3f &comVelocity = math::NkVec3f{0.f, 0.f, 0.f});
		};

	} // namespace renderer
} // namespace nkentseu
