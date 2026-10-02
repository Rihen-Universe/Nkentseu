#pragma once
// -----------------------------------------------------------------------------
// @File    NogeeLogo.h
// @Brief   LA MARQUE DE NOGEE, PROVISOIRE : un cube en perspective isometrique
//          (le moteur 3D), ses trois faces dans les bleus de la famille, et
//          l'arete d'ambre de la selection. Elle tient le COIN HAUT GAUCHE,
//          le carre qui couvre la ligne des menus ET celle des onglets (la place
//          que le logo rond d'Unreal occupe, et la gelee d'Unkeny chez
//          UnkenyEditor).
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// ⚠️ PROVISOIRE : Rihen choisira le logo de Nogee (feuille de route R32). Changer
//    le logo, c'est changer CETTE fonction seule : le cadre de la famille
//    (NkFamilleCadre.h) l'appelle par le rappel `NkFamilleTitre::logo`.
//    Dessinee en VECTEURS (aucune image), repere 100 x 100, nette a toute taille ;
//    une version pour fond sombre, une pour fond clair.
// -----------------------------------------------------------------------------

#include "NKGui/Core/NkGuiDrawList.h"

namespace nkentseu {
	namespace nogee {

		inline void NogeeDessinerLogo(nkgui::NkGuiDrawList &dl, float32 x, float32 y, float32 s, bool fondSombre, void *) {
			const float32 u = s / 100.f;
			auto P = [&](float32 vx, float32 vy) { return nkgui::NkVec2{x + vx * u, y + vy * u}; };
			const nkgui::NkColor dessus = fondSombre ? nkgui::NkColor(0x5BA8EEFFu) : nkgui::NkColor(0x2F86D6FFu);
			const nkgui::NkColor gauche = fondSombre ? nkgui::NkColor(0x1177D1FFu) : nkgui::NkColor(0x0E5FA6FFu);
			const nkgui::NkColor droite = fondSombre ? nkgui::NkColor(0x0A4A86FFu) : nkgui::NkColor(0x0A3F70FFu);
			const nkgui::NkColor arete = fondSombre ? nkgui::NkColor(0xF2980EFFu) : nkgui::NkColor(0xC97A08FFu);
			const nkgui::NkColor trait = fondSombre ? nkgui::NkColor(0x141414FFu) : nkgui::NkColor(0xF5F5F5FFu);
			// Le cube : un hexagone (centre 50,52), trois losanges.
			const nkgui::NkVec2 haut = P(50.f, 14.f), hg = P(18.f, 32.f), hd = P(82.f, 32.f);
			const nkgui::NkVec2 c = P(50.f, 50.f), bg = P(18.f, 70.f), bd = P(82.f, 70.f), bas = P(50.f, 88.f);
			dl.AddTriangleFilled(haut, hd, c, dessus);
			dl.AddTriangleFilled(haut, c, hg, dessus);
			dl.AddTriangleFilled(hg, c, bas, gauche);
			dl.AddTriangleFilled(hg, bas, bg, gauche);
			dl.AddTriangleFilled(c, hd, bd, droite);
			dl.AddTriangleFilled(c, bd, bas, droite);
			// Les aretes interieures, fines, pour lire le volume.
			dl.AddLine(c, hg, trait, 1.2f * u);
			dl.AddLine(c, hd, trait, 1.2f * u);
			// L'arete d'ambre : celle de la selection (Unreal la trace en orange).
			dl.AddLine(c, bas, arete, 4.f * u);
			dl.AddCircleFilled(c, 3.2f * u, arete);
		}

	} // namespace nogee
} // namespace nkentseu
