//
// NkUnkenyRenduEffets.h
// =============================================================================
// Description :
//   Dessine les particules visuelles d'une scene (NkEffets2D) : disques au
//   bord fondu, traits le long de la vitesse, disques pleins ; couleur et
//   taille interpolees sur la vie.
//
// Caracteristiques :
//   - DEUX PASSES, parce que l'eclairage les separe :
//       NK_EFFETS_ECLAIRES  les particules en melange ALPHA (fumee, pluie,
//                           neige) : dessinees AVANT la carte de lumiere, la
//                           nuit les assombrit comme le reste de la scene ;
//       NK_EFFETS_EMISSIFS  les ADDITIVES (feu, etincelles, explosion) :
//                           dessinees APRES, elles emettent, la nuit ne les
//                           eteint pas.
//     Sans eclairage, les deux passes se suivent, et l'ordre est le meme.
//   - Additif = ONE / ONE (NkGuiBlend::PlusLighter), la couleur PRE-MULTIPLIEE
//     par l'alpha : le bord d'une flamme s'efface vers le noir, qui n'ajoute
//     rien.
//   - Hors champ : non dessinees (test en monde, comme NkDessinerScene).
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENY_NKUNKENYRENDUEFFETS_H__
#define __NKENTSEU_UNKENY_NKUNKENYRENDUEFFETS_H__

#include "NKGui/Core/NkGuiDrawList.h"
#include "Unkeny/Scene/NkUnkenyScene.h"

namespace nkentseu {
	namespace unkeny {

		enum class NkPasseEffets2D : uint8 {
			NK_EFFETS_ECLAIRES = 0, ///< melange alpha : avant la carte de lumiere
			NK_EFFETS_EMISSIFS,		///< additives : apres
			NK_EFFETS_TOUS			///< les deux, dans cet ordre (sans eclairage)
		};

		/// Dessine les particules de la passe demandee. Rend le nombre dessine.
		int32 NkDessinerEffets(nkgui::NkGuiDrawList &dl, NkScene &scene,
							   NkPasseEffets2D passe = NkPasseEffets2D::NK_EFFETS_TOUS);

		/// La couleur d'une particule a la fraction `t` (0..1) de sa vie : debut ->
		/// milieu a 0,5 -> fin, en RGBA. Publique : le banc la verifie.
		uint32 NkCouleurSurLaVie(uint32 debut, uint32 milieu, uint32 fin, float32 t) noexcept;

	} // namespace unkeny
} // namespace nkentseu

#endif // __NKENTSEU_UNKENY_NKUNKENYRENDUEFFETS_H__
