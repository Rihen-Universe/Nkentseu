#pragma once
// -----------------------------------------------------------------------------
// @File    NKScenaLogo.h
// @Brief   LA MARQUE DE NKSCENA, PROVISOIRE : un CLAP de cinema -- la barre
//          rayee rouge « antenne » et blanc (le rouge de ce qui passe a l'image,
//          design/02 §1.1), le corps clair, le triangle de lecture, et le
//          losange d'ambre de la cle (le temps, la seule surface propre de
//          NKScena). Elle tient le COIN HAUT GAUCHE de la barre de titre, comme
//          le cube de Nogee et l'os de NkAnimaEditor.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// ⚠️ PROVISOIRE : Rihen choisira le logo de NKScena. Il est aussi celui de son
//    lanceur (NkLanceurIdentite : glyphe Camera, accent rouge antenne) : changer
//    le logo, c'est changer CETTE fonction seule (le rappel `NkFamilleTitre::logo`).
//    Dessine en VECTEURS, repere 100 x 100, une version par fond.
// -----------------------------------------------------------------------------

#include "NKGui/Core/NkGuiDrawList.h"

namespace nkentseu {
	namespace nkscena {

		/// L'accent de NKScena (le rouge « antenne » de design/03 §2, `@info.antenne`).
		constexpr uint32 kNkScenaAccent = 0xE5243BFFu;

		inline void NkScenaDessinerLogo(nkgui::NkGuiDrawList &dl, float32 x, float32 y, float32 s, bool fondSombre) {
			const float32 u = s / 100.f;
			auto P = [&](float32 vx, float32 vy) { return nkgui::NkVec2{x + vx * u, y + vy * u}; };
			const nkgui::NkColor rouge = fondSombre ? nkgui::NkColor(0xE5243BFFu) : nkgui::NkColor(0xC81E33FFu);
			const nkgui::NkColor blanc = fondSombre ? nkgui::NkColor(0xEEF1F5FFu) : nkgui::NkColor(0xFFFFFFFFu);
			const nkgui::NkColor corps = fondSombre ? nkgui::NkColor(0xE6EAF0FFu) : nkgui::NkColor(0x2A2D33FFu);
			const nkgui::NkColor fond = fondSombre ? nkgui::NkColor(0x141414FFu) : nkgui::NkColor(0xF5F5F5FFu);
			const nkgui::NkColor cle = fondSombre ? nkgui::NkColor(0xF2980EFFu) : nkgui::NkColor(0xC97A08FFu);
			// Le corps du clap.
			dl.AddRectFilled(nkgui::NkRect{x + 16.f * u, y + 44.f * u, 68.f * u, 40.f * u}, corps, 4.f * u);
			// La barre, inclinee, en quatre bandes alternees (rouge, blanc).
			const float32 hx0 = 16.f, hx1 = 84.f;
			for (int32 k = 0; k < 4; ++k) {
				const float32 a = hx0 + (hx1 - hx0) * static_cast<float32>(k) / 4.f;
				const float32 b = hx0 + (hx1 - hx0) * static_cast<float32>(k + 1) / 4.f;
				// La barre monte de 10 unites de gauche a droite (le clap s'ouvre).
				auto Haut = [&](float32 vx) { return 30.f - (vx - hx0) * 0.18f; };
				const nkgui::NkVec2 q[4] = {P(a, Haut(a)), P(b, Haut(b)), P(b, Haut(b) + 11.f), P(a, Haut(a) + 11.f)};
				dl.AddConvexPolyFilled(q, 4, (k % 2) == 0 ? rouge : blanc);
			}
			// La charniere.
			dl.AddCircleFilled(P(18.f, 38.f), 3.f * u, rouge);
			// Le triangle de lecture, dans le corps.
			dl.AddTriangleFilled(P(42.f, 53.f), P(42.f, 75.f), P(61.f, 64.f), fond);
			// La cle d'ambre, au coin bas droit.
			const nkgui::NkVec2 c = P(80.f, 82.f);
			const float32 r = 10.f * u;
			dl.AddTriangleFilled(nkgui::NkVec2{c.x, c.y - r}, nkgui::NkVec2{c.x + r, c.y}, nkgui::NkVec2{c.x, c.y + r}, cle);
			dl.AddTriangleFilled(nkgui::NkVec2{c.x, c.y - r}, nkgui::NkVec2{c.x, c.y + r}, nkgui::NkVec2{c.x - r, c.y}, cle);
		}

	} // namespace nkscena
} // namespace nkentseu
