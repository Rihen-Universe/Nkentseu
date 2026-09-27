#pragma once
// -----------------------------------------------------------------------------
// @File    SondePont.h
// @Brief   `--sonde-pont` : les deux vues de la toile decrivent-elles LA MEME
//          transformation ?
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  LA QUESTION, ET POURQUOI ELLE SE POSE MAINTENANT
// =============================================================================
//  Faire entrer la toile de NKUIDesign dans le role `Canvas` suppose que
//  `NkCanvasView` et `NkGuiVue` soient deux ECRITURES d'une seule
//  transformation. Cette sonde le verifie -- ou le refute -- AVANT qu'on ouvre
//  `Panels.h`.
//
//  🔴 C2 EST LE CRITERE QUI DECIDE, ET IL N'EST PAS UN ALLER-RETOUR. Un
//     aller-retour (`vue -> pont -> vue`) peut passer avec deux conversions
//     fausses de la meme facon : elles se compensent. C2 compare, pour des points
//     CHOISIS, le pixel que chaque vue rend -- deux chemins de calcul sans rien
//     en commun. C'est la seule facon de prouver que ce n'est pas une
//     compensation.
//
//  ⚠️ ET LES POINTS D'ESSAI NE SONT PAS AU CENTRE. Au centre de la zone, les deux
//     ecritures coincident quel que soit le pan : un jeu d'essai centre passerait
//     avec une conversion fausse. Ils sont donc pris loin, et sur les deux axes.
// -----------------------------------------------------------------------------

#ifndef __NKENTSEU_NKUIDESIGN_SONDEPONT_H__
#define __NKENTSEU_NKUIDESIGN_SONDEPONT_H__

#include "NkToilePont.h"

namespace nkuidesign {

	namespace sondepont {

		using nkentseu::float32;
		using nkentseu::uint32;

		inline bool Proche(float32 a, float32 b, float32 tol) noexcept {
			const float32 d = a - b;
			return (d < 0.f ? -d : d) <= tol;
		}

	} // namespace sondepont

	/// `--sonde-pont` : 0 si tout passe, 1 sinon.
	inline int SondePont() noexcept {
		using namespace sondepont;
		using nkentseu::nkgui::NkGuiVue;
		using nkentseu::nkgui::NkRect;
		using nkentseu::nkgui::NkVec2;

		printf("=== SONDE PONT — les deux vues de la toile decrivent-elles la meme "
			   "transformation ? ===\n");
		uint32 ko = 0u;

		// Une zone qui n'est PAS a l'origine : une conversion qui oublierait
		// `viewport` passerait sur une zone posee en (0,0).
		const NkRect zone{120.f, 60.f, 800.f, 500.f};

		NkCanvasView v;
		v.viewport = {zone.x, zone.y, zone.w, zone.h};
		v.zoom = 2.5f;
		v.panX = -340.f; // un pan FRANC, dans les deux sens
		v.panY = 175.f;

		// ── C1 : ALLER-RETOUR ──────────────────────────────────────────────
		{
			const NkGuiVue g = NkVueDepuisToile(v);
			const NkCanvasView r = NkToileDepuisVue(g, zone);
			const bool ok = Proche(r.zoom, v.zoom, 0.0001f) && Proche(r.panX, v.panX, 0.001f)
							&& Proche(r.panY, v.panY, 0.001f)
							&& Proche(r.viewport.x, v.viewport.x, 0.001f);
			if (!ok) ++ko;
			printf("  C1 aller-retour    pan (%.2f, %.2f) -> (%.2f, %.2f), zoom %.3f -> %.3f  %s\n",
				   (double)v.panX, (double)v.panY, (double)r.panX, (double)r.panY,
				   (double)v.zoom, (double)r.zoom, ok ? "OK" : "<<< KO");
			printf("       ^ NE SUFFIT PAS : deux conversions fausses de la meme facon se\n");
			printf("         compensent et passent ce critere. C'est C2 qui decide.\n");
		}

		// ── C2 : 🔴 LE MEME PIXEL, PAR DEUX CHEMINS SANS CODE COMMUN ───────
		{
			const NkGuiVue g = NkVueDepuisToile(v);
			// Points loin du centre, sur les deux axes, dont un negatif.
			const float32 pts[4][2] = {{0.f, 0.f}, {400.f, 250.f}, {-120.f, 90.f}, {33.5f, -47.25f}};
			uint32 ecarts = 0u;
			float32 pireX = 0.f, pireY = 0.f;
			for (uint32 i = 0; i < 4u; ++i) {
				const float32 sxA = v.ToScreenX(pts[i][0]);
				const float32 syA = v.ToScreenY(pts[i][1]);
				const NkVec2 b = g.VersEcran(NkVec2(pts[i][0], pts[i][1]), zone);
				const float32 dx = sxA - b.x, dy = syA - b.y;
				const float32 ax = dx < 0.f ? -dx : dx, ay = dy < 0.f ? -dy : dy;
				if (ax > pireX) pireX = ax;
				if (ay > pireY) pireY = ay;
				if (ax > 0.01f || ay > 0.01f)
					++ecarts;
			}
			const bool ok = (ecarts == 0u);
			if (!ok) ++ko;
			printf("  C2 meme pixel      4 points, ecart max (%.4f, %.4f) px, ecarts=%u  %s\n",
				   (double)pireX, (double)pireY, ecarts, ok ? "OK" : "<<< KO");
			// ⚠️ LES ACCOLADES NE SONT PAS DU STYLE. Sans elles, la seconde ligne
			//    s'imprimait TOUJOURS : la sonde annonçait « le plan est a revoir »
			//    alors que ses quatre criteres passaient. Un banc qui crie au vert
			//    apprend a ne plus etre lu.
			if (!ok) {
				printf("       ^ LES DEUX VUES NE DECRIVENT PAS LA MEME TRANSFORMATION : le plan\n");
				printf("         de reprise de la toile est a revoir AVANT d'ouvrir Panels.h.\n");
			}
		}

		// ── C3 : LE ZOOM AU CURSEUR, DES DEUX COTES ────────────────────────
		{
			// Les deux vues pretendent laisser immobile le point sous le curseur.
			// On le verifie sur CHACUNE, puis on compare leurs resultats.
			const float32 cx = zone.x + 180.f, cy = zone.y + 120.f; // loin du centre

			NkCanvasView a = v;
			const float32 docAvantX = a.ToDocX(cx), docAvantY = a.ToDocY(cy);
			a.ZoomAt(1.1f, cx, cy);
			const float32 docApresX = a.ToDocX(cx), docApresY = a.ToDocY(cy);
			const bool immobileA = Proche(docAvantX, docApresX, 0.01f)
								   && Proche(docAvantY, docApresY, 0.01f);

			NkGuiVue g = NkVueDepuisToile(v);
			const NkVec2 mAvant = g.VersMonde(NkVec2(cx, cy), zone);
			g.zoom *= 1.1f;
			const NkVec2 mApres0 = g.VersMonde(NkVec2(cx, cy), zone);
			g.centre.x += mAvant.x - mApres0.x;
			g.centre.y += mAvant.y - mApres0.y;
			const NkVec2 mApres = g.VersMonde(NkVec2(cx, cy), zone);
			const bool immobileB = Proche(mAvant.x, mApres.x, 0.01f)
								   && Proche(mAvant.y, mApres.y, 0.01f);

			// ET LES DEUX DOIVENT ARRIVER AU MEME ETAT.
			const NkGuiVue gDepuisA = NkVueDepuisToile(a);
			const bool memeEtat = Proche(gDepuisA.zoom, g.zoom, 0.0001f)
								  && Proche(gDepuisA.centre.x, g.centre.x, 0.01f)
								  && Proche(gDepuisA.centre.y, g.centre.y, 0.01f);
			const bool ok = immobileA && immobileB && memeEtat;
			if (!ok) ++ko;
			printf("  C3 zoom au curseur immobile NKUIDesign=%s, role=%s, meme etat apres=%s  %s\n",
				   immobileA ? "oui" : "NON", immobileB ? "oui" : "NON", memeEtat ? "oui" : "NON",
				   ok ? "OK" : "<<< KO");
		}

		// ── C4 : LE RECTANGLE EST BIEN PORTE ───────────────────────────────
		{
			// Une zone deplacee doit deplacer le pixel rendu. Sans ce critere, une
			// conversion qui perdrait `viewport` passerait C1 et C2 -- tous deux
			// employant LA MEME zone.
			const NkGuiVue g = NkVueDepuisToile(v);
			const NkRect zone2{zone.x + 250.f, zone.y + 90.f, zone.w, zone.h};
			const NkCanvasView v2 = NkToileDepuisVue(g, zone2);
			const float32 sx = v2.ToScreenX(0.f);
			const NkVec2 b = g.VersEcran(NkVec2(0.f, 0.f), zone2);
			const bool suit = Proche(sx, b.x, 0.01f);
			const bool aBouge = !Proche(sx, v.ToScreenX(0.f), 0.5f);
			const bool ok = suit && aBouge;
			if (!ok) ++ko;
			printf("  C4 zone deplacee   les deux suivent=%s, et le pixel a bouge=%s  %s\n",
				   suit ? "oui" : "NON", aBouge ? "oui" : "NON", ok ? "OK" : "<<< KO");
		}

		printf("=== %s (ko = %u) ===\n", ko == 0u ? "TOUT PASSE" : "KO", ko);
		if (ko == 0u) {
			printf("  -> Les deux vues sont DEUX ECRITURES D'UNE SEULE transformation.\n");
			printf("     Le passage de l'une a l'autre est un changement de parametrage,\n");
			printf("     pas un changement de sens. L'integration de la toile peut se\n");
			printf("     faire sans refaire les mathematiques.\n");
		}
		return ko == 0u ? 0 : 1;
	}

} // namespace nkuidesign

#endif // __NKENTSEU_NKUIDESIGN_SONDEPONT_H__

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
