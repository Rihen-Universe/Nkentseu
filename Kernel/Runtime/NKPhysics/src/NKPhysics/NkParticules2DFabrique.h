// AUTEUR : Rihen
// =============================================================================
// NkParticules2DFabrique.h — fabriquer les corps de NkParticules2D
//
// Deux choses, et seulement deux :
//   les PRESETS    des parametres physiques MESURES au banc (un blob qui coule
//                  en ~5 s, une eau qui ne s'etale pas en film, un cristal qui
//                  fond a T = 100). Ce sont des chiffres de physique : ils
//                  vivent a cote du solveur, pas dans un jeu.
//   les GEOMETRIES anneau, disque hexagonal, grille, chaine. Chaque fonction
//                  ouvre UN corps, le remplit, le finalise et rend son INDEX
//                  (ou -1). Son `id` stable est dans corps[index].id.
//
// Ce qui n'est PAS ici : les noms, les couleurs, les icones. Ce sont des
// donnees d'affichage : elles appartiennent a l'entite (Unkeny), pas au corps.
//
// Toutes les longueurs sont en metres.
// =============================================================================
#pragma once

#include "NKPhysics/NkParticules2D.h"

namespace nkentseu {
	namespace physics {

		enum class NkPresetP2D : uint8 {
			NK_BALLON = 0,
			NK_BLOB,
			NK_SLIME, ///< un blob plus mou et plus plastique
			NK_GELEE,
			NK_EAU,
			NK_MIEL,
			NK_SABLE,
			NK_CRISTAL,
			NK_TISSU,
			NK_CORDE,
			NK_PONT,
			NK_COUNT
		};

		/// Ecrit dans `c` les parametres mesures du preset (materiau compris).
		void NkAppliquerPresetP2D(NkCorpsP2D &c, NkPresetP2D preset) noexcept;

		/// Ballon : anneau de rayon R gonfle par un gaz.
		int32 NkCreerBallonP2D(NkParticules2D &p, const NkVec2f &centre, float32 R = 0.55f) noexcept;
		/// Blob (ou slime) : disque hexagonal relie par des ressorts plastiques.
		int32 NkCreerBlobP2D(NkParticules2D &p, const NkVec2f &centre, float32 R, NkPresetP2D preset) noexcept;
		/// Gelee : grille nx * ny + appariement de forme.
		int32 NkCreerGeleeP2D(NkParticules2D &p, const NkVec2f &centre, int32 nx = 9, int32 ny = 7,
							  float32 s = 0.13f) noexcept;
		/// Disque de fluide ou de sable (preset EAU, MIEL ou SABLE).
		int32 NkCreerFluideP2D(NkParticules2D &p, const NkVec2f &centre, float32 R, NkPresetP2D preset) noexcept;
		/// Ouvre un corps fluide VIDE pour un pinceau. -1 si le preset n'est pas un fluide.
		int32 NkOuvrirPinceauP2D(NkParticules2D &p, NkPresetP2D preset) noexcept;
		/// Ajoute jusqu'a `n` particules autour de `pos` au corps `corps`, qui doit
		/// etre le DERNIER (contiguite). Refuse de naitre dans une autre particule.
		uint32 NkVerserP2D(NkParticules2D &p, uint32 corps, const NkVec2f &pos, float32 rayon, int32 n,
						   uint32 &graine) noexcept;
		/// Cristal : reseau hexagonal d'atomes lies, en bloc 13 x 8 ou en disque.
		int32 NkCreerCristalP2D(NkParticules2D &p, const NkVec2f &centre, bool disque) noexcept;
		/// Tissu : grille pendante dont la rangee du haut est epinglee de 5 en 5.
		int32 NkCreerTissuP2D(NkParticules2D &p, const NkVec2f &hautCentre, int32 nx = 26, int32 ny = 18,
							  float32 s = 0.11f) noexcept;
		/// Corde lestee, epinglee en haut.
		int32 NkCreerCordeP2D(NkParticules2D &p, const NkVec2f &haut, int32 n = 34, float32 s = 0.09f) noexcept;
		/// Pont de planches entre deux points epingles, avec 6 % de mou.
		int32 NkCreerPontP2D(NkParticules2D &p, const NkVec2f &a, const NkVec2f &b) noexcept;

	} // namespace physics
} // namespace nkentseu
