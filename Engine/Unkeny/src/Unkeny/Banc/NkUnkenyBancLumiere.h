//
// NkUnkenyBancLumiere.h
// =============================================================================
// Description :
//   Le banc de l'ECLAIRAGE 2D et des EFFETS (2026-09-30), sans fenetre ni GPU :
//   la carte de lumiere est rasterisee par NkGuiDrawListRaster, avec les
//   facteurs de melange du dorsal OpenGL.
//
//   Compte A PART du banc du moteur (NkUnkenyLancerBanc) : les 51 temoins
//   d'avant gardent leur compte, et un echec ici se lit a sa source.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENY_NKUNKENYBANCLUMIERE_H__
#define __NKENTSEU_UNKENY_NKUNKENYBANCLUMIERE_H__

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace unkeny {
		/// 0 quand tout tient ; le detail est ecrit sur la sortie standard.
		///
		/// OUTIL DE CONTRE-EPREUVE, lu seulement si la variable est posee :
		/// NK_BANC_SCENE_ANCIENNE=<fichier .nkscene ecrit par un moteur d'avant>
		/// le relit, et NK_BANC_SCENE_REECRITE=<sortie> y reecrit ce qui a ete lu
		/// (pour le comparer a l'original, hors du banc).
		int32 NkUnkenyLancerBancLumiere();
	} // namespace unkeny
} // namespace nkentseu

#endif // __NKENTSEU_UNKENY_NKUNKENYBANCLUMIERE_H__
