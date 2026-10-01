//
// NkUnkenyZoneSure.h
// =============================================================================
// Description :
//   La zone sure OFFERTE AU JEU (document 03 d'UnkenyEditor, §2.5) : l'ecran
//   du jeu depuis ce que NKCanvas lit dans NKWindow (ou ce que l'editeur
//   simule), le rectangle sur dans le monde, l'ancrage des entites d'interface,
//   la surimpression de debogage.
//
// Caracteristiques :
//   - NkEcranDepuisLayout : LA conversion NkLayoutInfo -> NkEcranDuJeu, la meme
//     dans le joueur (Layout() de la coquille, donc NKWindow) et dans l'editeur
//     (NkLayoutSimule de l'appareil simule). Deux conversions divergeraient.
//   - NkAppliquerAncrages : pose le transform des entites NkAncrageEcran2D.
//     Le joueur l'appelle avant de dessiner ; l'editeur aussi (et rend les
//     positions en EDITION, que la scene editee ne bouge pas).
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
#pragma once
#ifndef __NKENTSEU_UNKENY_NKUNKENYZONESURE_H__
#define __NKENTSEU_UNKENY_NKUNKENYZONESURE_H__

#include "NKCanvas/App/NkCanvasApp.h"
#include "NKGui/Core/NkGuiDrawList.h"
#include "Unkeny/Scene/NkUnkenyEcran.h"
#include "Unkeny/Scene/NkUnkenyScene.h"

namespace nkentseu {
	namespace unkeny {

		/// L'ecran du jeu depuis un NkLayoutInfo affiche dans `rect` (pixels de
		/// la fenetre). Quand `rect` est plus petit que le layout (l'appareil
		/// simule, reduit dans le viseur), les marges et la densite suivent
		/// l'echelle. `rect` vide : la fenetre du layout, a l'origine.
		NkEcranDuJeu NkEcranDepuisLayout(const renderer::NkLayoutInfo &layout, const nkgui::NkRect &rect, bool simule) noexcept;

		/// Le rectangle sur de l'ecran de la scene, dans le MONDE (Y vers le
		/// haut), a travers sa camera. Rend false si la scene n'a pas d'ecran.
		bool NkZoneSureMonde(const NkScene &scene, NkVec2f &minimum, NkVec2f &maximum) noexcept;

		/// Ce que NkAppliquerAncrages a deplace, pour le rendre (l'editeur, en
		/// EDITION).
		struct NkPositionAncree {
				ecs::NkEntityId id;
				NkVec2f avant{0.f, 0.f};
		};

		/// Pose le transform de chaque entite NkAncrageEcran2D active a son
		/// ancre, sur l'ecran de la scene (scene.Ecran()). Rend le nombre
		/// d'entites posees ; `avant`, facultatif, recoit leurs positions
		/// d'avant. Sans ecran valide : rien.
		int32 NkAppliquerAncrages(NkScene &scene, NkVector<NkPositionAncree> *avant = nullptr);
		void NkRendreAncrages(NkScene &scene, const NkVector<NkPositionAncree> &avant);

		/// Surimpression de DEBOGAGE : les bandes hors zone sure, hachurees, et
		/// le contour du rectangle sur. Ne dessine rien d'un ecran invalide.
		void NkDessinerZoneSure(nkgui::NkGuiDrawList &dl, const NkEcranDuJeu &e);

	} // namespace unkeny
} // namespace nkentseu

#endif // __NKENTSEU_UNKENY_NKUNKENYZONESURE_H__
