// =============================================================================
// NkUnkenyBanc.h — le banc du MOTEUR, sans fenetre ni GPU
//
// Unkeny n'avait aucun banc a lui : ses garanties n'etaient eprouvees que par
// les applications (Physic2D --selftest, UnkenyEditor --selftest), c'est-a-dire
// melangees a ce que ces applications ajoutent. Ce banc vit DANS le moteur pour
// que toute application puisse le lancer :
//
//     if (args[i] == "--selftest") return NkUnkenyLancerBanc() | MonBanc();
//
// Il rend 0 quand tout tient ; le detail est ecrit sur la sortie standard.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace unkeny {
		int32 NkUnkenyLancerBanc();
		/// La STRUCTURE de scene (hierarchie, identites, sauvegarde v2, prefabs,
		/// 2026-09-29) : NkUnkenyBancStructure.cpp. NkUnkenyLancerBanc la lance
		/// APRES ses propres temoins, et rend un echec si elle en a un.
		int32 NkUnkenyLancerBancStructure();
		/// Le banc du JEU (2026-09-29, NkUnkenyBancJeu.cpp) : corps mous dans les
		/// contacts, controleurs de personnage, jalon « Gelee ». Lance a la fin de
		/// NkUnkenyLancerBanc, qui rend 1 si l'un des deux rougit.
		int32 NkUnkenyLancerBancJeu();
		/// L'ECRAN du jeu et sa ZONE SURE (2026-10-01, NkUnkenyBancEcran.cpp) :
		/// lance A PART par l'editeur ET par le joueur (le meme code des deux
		/// cotes). Rend 0 quand tout tient.
		int32 NkUnkenyLancerBancEcran();
	} // namespace unkeny
} // namespace nkentseu
