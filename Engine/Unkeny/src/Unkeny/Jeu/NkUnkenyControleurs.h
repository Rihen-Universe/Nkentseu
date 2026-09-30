//
// NkUnkenyControleurs.h
// =============================================================================
// Description :
//   Le SYSTEME qui fait agir les controleurs de personnage
//   (Scene/NkUnkenyControles.h) : il lit une table d'ACTIONS, fait marcher et
//   sauter les corps rigides (NkControleRigide2D) et mous (NkControleMou2D).
//
// Caracteristiques :
//   - Un systeme NK_PAS_FIXE de la scene : il tourne AVANT la physique, a
//     chaque pas fixe, pour que ses poussees soient integrees dans le pas.
//   - Il ne lit AUCUNE touche : la table d'actions est remplie par le jeu (ou
//     par un banc). C'est ce qui fait jouer une partie entiere sans fenetre.
//   - Sans allocation par pas en regime etabli (un tampon de contacts garde).
//
// Algorithmes implementes :
//   - Saut au sol seulement, tolerance de bord (coyote) et tampon de saut
//   - Marche a acceleration bornee ; plateforme mobile emportee (rigide)
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENY_NKUNKENYCONTROLEURS_H__
#define __NKENTSEU_UNKENY_NKUNKENYCONTROLEURS_H__

#include "NKECS/World/NkWorld.h"
#include "Unkeny/Entree/NkUnkenyActions.h"
#include "Unkeny/Scene/NkUnkenyControles.h"

namespace nkentseu {
	namespace unkeny {

		class NkScene;

		/// Enregistre le systeme des controleurs sur la scene (NK_PAS_FIXE,
		/// `ordre` -100 : avant les systemes du jeu, qui peuvent corriger).
		/// ⚠️ `actions` doit VIVRE aussi longtemps que la scene : le systeme garde
		/// son adresse. Rend l'id du systeme (0 = refus : actions nulles).
		uint32 NkAjouterControleurs2D(NkScene &scene, const NkActions *actions, int32 ordre = -100);

		/// Un pas des controleurs, hors systeme (un banc, un outil). `dt` = le pas
		/// FIXE de la scene.
		void NkPasControleurs2D(NkScene &scene, const NkActions &actions, float32 dt);

		/// Relie deux entites a corps mou (NkParticules2D::RelierCorps) : chaque
		/// particule de `a` a sa plus proche voisine de `b` a moins de `portee`.
		/// Rend le nombre de liens poses (0 : pas de particules, pas de corps mou).
		uint32 NkRelierCorpsMous(NkScene &scene, ecs::NkEntityId a, ecs::NkEntityId b, float32 portee);

	} // namespace unkeny
} // namespace nkentseu

#endif // __NKENTSEU_UNKENY_NKUNKENYCONTROLEURS_H__
