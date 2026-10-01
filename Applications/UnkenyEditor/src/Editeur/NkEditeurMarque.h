//
// NkEditeurMarque.h
// =============================================================================
// Description :
//   LA MARQUE D'UNKENY, dessinee en VECTEURS par la liste de dessin de
//   l'editeur : « Rebond sur les tuiles », la gelee souriante dans le U en
//   tuiles, SANS la trajectoire en pointilles (le logo valide le 01/10/2026).
//   Elle tient le COIN HAUT GAUCHE de l'editeur, un carre qui couvre la ligne
//   des menus ET celle des onglets, comme le logo rond d'Unreal (`ue58_a.png`).
//
// Caracteristiques :
//   - MEME DESSIN PARTOUT : les coordonnees sont celles des SVG maitres
//     (data/brand/unkeny_marque_{sombre,clair}.svg, repere 100 x 100) -- huit
//     tuiles bleues 16 x 16 en deux colonnes (x = 15 et 69 ; y = 15, 33, 51,
//     69), deux tuiles ambre au sol (x = 33 et 51, y = 69), la gelee
//     « M34,67 C34,52 66,52 66,67 Z », deux yeux et un sourire. Changer le
//     logo, c'est changer ces fichiers ENSEMBLE (comme NkModelerBrand.h pour
//     l'araignee de NKCraft).
//   - SOMBRE ou CLAIR selon le fond : les couleurs de MARQUE de chaque version
//     (un bleu, un ambre, la gelee et son visage) sont celles des SVG -- elles
//     sont l'identite d'Unkeny, pas des roles du theme.
//   - Les courbes (le dome de la gelee, le sourire) sont echantillonnees :
//     aucune image, net a toute taille.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENYEDITOR_NKEDITEURMARQUE_H__
#define __NKENTSEU_UNKENYEDITOR_NKEDITEURMARQUE_H__

#include "NKGui/Core/NkGuiDrawList.h"

namespace nkentseu {
	namespace editeur {

		/// Les couleurs d'une version de la marque (celles des SVG maitres).
		struct NkCouleursMarque {
				nkgui::NkColor tuile;
				nkgui::NkColor sol;
				nkgui::NkColor gelee;
				nkgui::NkColor visage;
		};

		/// La version SOMBRE (pour un fond sombre) ou CLAIRE (pour un fond clair).
		inline NkCouleursMarque NkMarqueCouleurs(bool sombre) noexcept {
			NkCouleursMarque k;
			if (sombre) {
				k.tuile = nkgui::NkColor(0x1177D1FFu);
				k.sol = nkgui::NkColor(0xF2980EFFu);
				k.gelee = nkgui::NkColor(0xF5F5F5FFu);
				k.visage = nkgui::NkColor(0x141414FFu);
			} else {
				k.tuile = nkgui::NkColor(0x0E5FA6FFu);
				k.sol = nkgui::NkColor(0xC97A08FFu);
				k.gelee = nkgui::NkColor(0x141414FFu);
				k.visage = nkgui::NkColor(0xF5F5F5FFu);
			}
			return k;
		}

		/// Un fond est-il SOMBRE (luminance percue < 0,5) ? La version de la
		/// marque suit le fond sous elle.
		inline bool NkFondSombre(const nkgui::NkColor &fond) noexcept {
			const float32 l = (0.299f * static_cast<float32>(fond.r) + 0.587f * static_cast<float32>(fond.g) +
							   0.114f * static_cast<float32>(fond.b)) /
							  255.f;
			return l < 0.5f;
		}

		/// Dessine la marque dans le carre (x, y, s) -- s = cote en pixels.
		inline void NkDessinerMarque(nkgui::NkGuiDrawList &dl, float32 x, float32 y, float32 s, bool sombre) {
			const NkCouleursMarque k = NkMarqueCouleurs(sombre);
			const float32 u = s / 100.f;
			auto P = [&](float32 vx, float32 vy) { return nkgui::NkVec2{x + vx * u, y + vy * u}; };
			auto Tuile = [&](float32 tx, float32 ty, const nkgui::NkColor &c) {
				dl.AddRectFilled(nkgui::NkRect{x + tx * u, y + ty * u, 16.f * u, 16.f * u}, c, 2.5f * u);
			};
			// 1. Le U : deux colonnes de quatre tuiles bleues.
			static const float32 kRangs[4] = {15.f, 33.f, 51.f, 69.f};
			for (float32 ty : kRangs) {
				Tuile(15.f, ty, k.tuile);
				Tuile(69.f, ty, k.tuile);
			}
			// 2. Le sol : deux tuiles ambre entre les colonnes.
			Tuile(33.f, 69.f, k.sol);
			Tuile(51.f, 69.f, k.sol);
			// 3. La gelee : « M34,67 C34,52 66,52 66,67 Z », une cubique fermee
			//    par sa base -- convexe : un eventail suffit.
			constexpr int32 N = 24;
			nkgui::NkVec2 dome[N + 1];
			for (int32 i = 0; i <= N; ++i) {
				const float32 t = static_cast<float32>(i) / static_cast<float32>(N);
				const float32 a = (1.f - t) * (1.f - t) * (1.f - t), b = 3.f * (1.f - t) * (1.f - t) * t;
				const float32 c = 3.f * (1.f - t) * t * t, d = t * t * t;
				dome[i] = P(a * 34.f + b * 34.f + c * 66.f + d * 66.f, a * 67.f + b * 52.f + c * 52.f + d * 67.f);
			}
			dl.AddConvexPolyFilled(dome, N + 1, k.gelee);
			// 4. Les yeux.
			dl.AddCircleFilled(P(45.f, 60.6f), 2.3f * u, k.visage);
			dl.AddCircleFilled(P(55.f, 60.6f), 2.3f * u, k.visage);
			// 5. Le sourire : « M45.6,63.8 Q50,66.8 54.4,63.8 », trait de 1,8.
			constexpr int32 M = 10;
			nkgui::NkVec2 sourire[M + 1];
			for (int32 i = 0; i <= M; ++i) {
				const float32 t = static_cast<float32>(i) / static_cast<float32>(M);
				const float32 a = (1.f - t) * (1.f - t), b = 2.f * (1.f - t) * t, c = t * t;
				sourire[i] = P(a * 45.6f + b * 50.f + c * 54.4f, a * 63.8f + b * 66.8f + c * 63.8f);
			}
			dl.AddPolyline(sourire, M + 1, k.visage, 1.8f * u, false);
		}

	} // namespace editeur
} // namespace nkentseu

#endif // __NKENTSEU_UNKENYEDITOR_NKEDITEURMARQUE_H__
