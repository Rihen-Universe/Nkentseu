// =============================================================================
// NkUE5Theme.h — la palette de l'editeur, facon Unreal Engine 5
//
// LA SEULE SOURCE DES COULEURS. Aucun autre fichier de l'editeur n'ecrit un
// triplet RGB en dur : changer le theme, c'est changer ce fichier.
//
// Les valeurs sont RELEVEES sur le theme sombre par defaut d'UE5 (5.x) :
// fond quasi noir entre les panneaux, panneaux gris #242424, champs de saisie
// #0F0F0F, accent bleu #0070E0, selection orange dans la vue.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#pragma once

#include "NKGui/Core/NkGuiTypes.h"

namespace nkentseu {
	namespace physic2d {
		namespace ue5 {

			using nkgui::NkColor;

			// --- Structure -------------------------------------------------------
			const NkColor kFond(10, 10, 10);			///< l'espace ENTRE les panneaux
			const NkColor kBarreTitre(21, 21, 21);
			const NkColor kBarreOutils(26, 26, 26);
			const NkColor kPanneau(36, 36, 36);
			const NkColor kPanneauSombre(28, 28, 28);
			const NkColor kEnteteOnglets(21, 21, 21);
			const NkColor kOngletActif(36, 36, 36);
			const NkColor kOngletInactif(26, 26, 26);
			const NkColor kCategorie(47, 47, 47);		///< bandeaux repliables du panneau Details
			const NkColor kCategorieSurvol(56, 56, 56);
			const NkColor kLigneImpaire(40, 40, 40);
			const NkColor kSeparateur(18, 18, 18);

			// --- Widgets ---------------------------------------------------------
			const NkColor kChamp(15, 15, 15);
			const NkColor kChampSurvol(24, 24, 24);
			const NkColor kChampBord(56, 56, 56);
			const NkColor kChampRemplissage(34, 58, 96);
			const NkColor kBouton(56, 56, 56);
			const NkColor kBoutonSurvol(74, 74, 74);
			const NkColor kBoutonPresse(38, 38, 38);
			const NkColor kBoutonPlat(36, 36, 36);

			// --- Accents ---------------------------------------------------------
			const NkColor kAccent(0, 112, 224);			///< le bleu UE5
			const NkColor kAccentClair(38, 145, 255);
			const NkColor kAccentSombre(0, 70, 150);
			const NkColor kSelectionLigne(0, 90, 190, 200);
			const NkColor kSelectionVue(255, 162, 18);	///< contour orange de l'acteur selectionne
			const NkColor kJouer(64, 176, 84);
			const NkColor kArret(228, 72, 64);
			const NkColor kPauseCouleur(236, 184, 52);

			// --- Texte -----------------------------------------------------------
			const NkColor kTexte(196, 196, 196);
			const NkColor kTexteClair(240, 240, 240);
			const NkColor kTexteFaible(128, 128, 128);
			const NkColor kTexteTresFaible(88, 88, 88);

			// --- Journal ---------------------------------------------------------
			const NkColor kLogNormal(190, 190, 190);
			const NkColor kLogAvert(255, 200, 64);
			const NkColor kLogErreur(255, 96, 96);
			const NkColor kLogSucces(120, 214, 120);
			const NkColor kLogCategorie(110, 160, 230);

			// --- Vue ---------------------------------------------------------------
			const NkColor kVueHaut(62, 66, 72);
			const NkColor kVueBas(34, 36, 40);
			const NkColor kVueHorsMonde(0, 0, 0, 90);
			const NkColor kGrilleFine(255, 255, 255, 14);
			const NkColor kGrilleForte(255, 255, 255, 34);
			const NkColor kAxeX(210, 64, 64);
			const NkColor kAxeY(96, 196, 84);
			const NkColor kSol(30, 31, 34);
			const NkColor kSolBord(124, 128, 136);
			const NkColor kObstacle(84, 88, 98);
			const NkColor kObstacleBord(24, 25, 28);
			const NkColor kObstacleReflet(128, 134, 146);
			const NkColor kEpingle(236, 60, 60);

			/// Ton reel d'une couleur : alpha remplace.
			inline NkColor NkAvecAlpha(const NkColor &c, uint8 a) noexcept {
				return NkColor(c.r, c.g, c.b, a);
			}
			/// Melange lineaire de deux couleurs, t dans [0,1].
			inline NkColor NkMelange(const NkColor &a, const NkColor &b, float32 t) noexcept {
				t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
				auto m = [t](uint8 x, uint8 y) {
					return static_cast<uint8>(static_cast<float32>(x) + (static_cast<float32>(y) - static_cast<float32>(x)) * t);
				};
				return NkColor(m(a.r, b.r), m(a.g, b.g), m(a.b, b.b), m(a.a, b.a));
			}
			inline NkColor NkEclaircir(const NkColor &c, float32 t) noexcept {
				return NkMelange(c, NkColor(255, 255, 255, c.a), t);
			}
			inline NkColor NkAssombrir(const NkColor &c, float32 t) noexcept {
				return NkMelange(c, NkColor(0, 0, 0, c.a), t);
			}

		} // namespace ue5
	} // namespace physic2d
} // namespace nkentseu
