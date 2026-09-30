//
// NkUnkenyBancLivraison.h
// =============================================================================
// Description :
//   Le banc de la LIVRAISON (palier U5) : cuire un jeu, le relire comme le
//   joueur autonome, et prouver qu'il est le meme -- sans fenetre ni GPU.
//
// Caracteristiques :
//   - Compte A PART du banc du moteur (NkUnkenyLancerBanc, 37 temoins) : le
//     « 37/37 » garde son sens, et ce banc se lance seul depuis le joueur
//     (`UnkenyPlayer --selftest`) comme depuis l'editeur.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENY_NKUNKENYBANCLIVRAISON_H__
#define __NKENTSEU_UNKENY_NKUNKENYBANCLIVRAISON_H__

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace unkeny {

		/// 0 quand tout tient ; le detail est ecrit sur la sortie standard.
		int32 NkUnkenyLancerBancLivraison();

	} // namespace unkeny
} // namespace nkentseu

#endif // __NKENTSEU_UNKENY_NKUNKENYBANCLIVRAISON_H__
