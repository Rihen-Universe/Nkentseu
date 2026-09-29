//
// NkUnkenyBancEntrees.h
// =============================================================================
// Description :
//   Le banc des ENTREES du jeu : actions, liaisons, sources (clavier, manette,
//   doigts), et la composition IME d'un champ NKGui. Sans fenetre, sans
//   manette branchee, sans doigt : tout est POSE (evenements construits a la
//   main, manette de banc derriere un vrai NkGamepadSystem).
//
// Caracteristiques :
//   - Separe de NkUnkenyLancerBanc : les comptes de ce banc-la (37/37) restent
//     ceux qu'on connait, et celui-ci se lit a part.
//   - `NK_UNKENY_BANC_CAPTURES=<dossier>` : ecrit aussi l'image du champ en
//     composition (entrees_composition.png), pour la regarder.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENY_NKUNKENYBANCENTREES_H__
#define __NKENTSEU_UNKENY_NKUNKENYBANCENTREES_H__

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace unkeny {
		/// 0 quand tout tient ; le detail est ecrit sur la sortie standard.
		int32 NkUnkenyLancerBancEntrees();
	} // namespace unkeny
} // namespace nkentseu

#endif // __NKENTSEU_UNKENY_NKUNKENYBANCENTREES_H__
