#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// Rig/ — le rig 3D : armature editable (tete/queue/roulis), rig automatique, document du rig.
// -----------------------------------------------------------------------------
// FICHIER: NKAnima/Rig/NkRigMannequin.h
// DESCRIPTION: des MANNEQUINS DE TEST, fabriques par le calcul : un maillage
//   SANS squelette, ferme et d'un seul tenant, sur lequel le rig automatique
//   et les poids automatiques s'eprouvent (bancs, captures, premier essai dans
//   NkAnimaEditor).
//
//   Un mannequin d'atelier, pas un personnage : des volumes lisses (capsules et
//   ellipsoides fondus), une tete en oeuf SANS visage -- aucun trait, donc aucun
//   trait « ethnique ». La surface est extraite d'une fonction de distance
//   (« surface nets ») : un seul maillage etanche, ce qu'aucune union de
//   primitives separees ne donnerait.
// -----------------------------------------------------------------------------

#include "NKAnima/Skin/NkSkinMesh.h"

namespace nkentseu {
	namespace anim {

		enum class NkMannequinKind : uint8 {
			NK_HUMANOIDE_T = 0, ///< debout, bras a l'horizontale (pose en T), 1,75 de haut
			NK_HUMANOIDE_A,		///< debout, bras a 40 deg sous l'horizontale (pose en A)
			NK_QUADRUPEDE,		///< un quadrupede (chien d'atelier), tete vers +Z
			NK_COUNT
		};

		/// Fabrique le mannequin : positions, triangles, normales, topologie.
		/// `resolution` : cellules sur la hauteur (64 a 160).
		bool NkMakeMannequin(NkMannequinKind kind, NkSkinMesh &out, uint32 resolution = 96);

	} // namespace anim
} // namespace nkentseu
