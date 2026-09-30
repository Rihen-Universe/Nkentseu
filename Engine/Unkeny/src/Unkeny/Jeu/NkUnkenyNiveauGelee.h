//
// NkUnkenyNiveauGelee.h
// =============================================================================
// Description :
//   Le niveau du jalon « Gelee » : trois plates-formes, deux trous, une zone
//   de fin, et le heros (mou ou rigide) avec son controleur de personnage.
//   Une fonction, pour que le banc du jeu (G1, G2), l'editeur (« Espace fait
//   sauter le heros en Jouer ») et le joueur autonome jouent le MEME niveau.
//
// Caracteristiques :
//   - Le niveau est pose dans une scene DEJA initialisee avec la physique et
//     les particules (NkSceneConfig::physique et ::particules).
//   - Le heros lit les actions STANDARD (Entree/NkUnkenyActionsStandard.h) :
//     ses reglages gardent les indices par defaut.
//
// ⚠️ POURQUOI CE FICHIER EXISTE (2026-09-30)
//   Le niveau vivait dans le banc du jeu (NkUnkenyBancJeu.cpp, espace anonyme).
//   L'editeur et le joueur devaient le rejouer : une copie aurait diverge au
//   premier reglage du banc, et le temoin de l'editeur aurait joue un AUTRE
//   niveau que le jalon. Le banc appelle desormais cette fonction.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENY_NKUNKENYNIVEAUGELEE_H__
#define __NKENTSEU_UNKENY_NKUNKENYNIVEAUGELEE_H__

#include "NKECS/World/NkWorld.h"
#include "NKMath/NKMath.h"
#include "Unkeny/Scene/NkUnkenyControles.h"

namespace nkentseu {
	namespace unkeny {

		class NkScene;

		struct NkNiveauGelee {
				ecs::NkEntityId plateformes[3];
				NkVec2f mn[3]; ///< coin bas-gauche de chaque plate-forme
				NkVec2f mx[3]; ///< coin haut-droit
				ecs::NkEntityId fin;   ///< la zone de fin (collisionneur declencheur)
				ecs::NkEntityId heros; ///< porte NkControleMou2D (mou) ou NkControleRigide2D
				bool mou = true;
		};

		/// Pose le niveau et son heros. false si la scene n'a pas de physique (ou
		/// pas de particules pour un heros mou) : rien n'est alors cree.
		bool NkConstruireNiveauGelee(NkScene &scene, NkNiveauGelee &niveau, bool herosMou = true);

		/// Le centre du heros : celui de ses particules (mou) ou sa position.
		NkVec2f NkPositionHerosGelee(NkScene &scene, const NkNiveauGelee &niveau);

		/// L'etat du controleur du heros, nullptr s'il n'en a pas.
		const NkEtatControle2D *NkEtatHerosGelee(NkScene &scene, const NkNiveauGelee &niveau);

	} // namespace unkeny
} // namespace nkentseu

#endif // __NKENTSEU_UNKENY_NKUNKENYNIVEAUGELEE_H__
