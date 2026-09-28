#pragma once
// -----------------------------------------------------------------------------
// @File    CasBandeFixe.h
// @Brief   (b23) Une bande fixe en haut d'une zone défilable : elle NE DÉFILE
//          PAS, et le contenu commence dessous.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  CE QUE ÇA MESURE
// =============================================================================
//  Rodolf, 28/09 : « le panneau n'a pas de header ». Il en avait un — le
//  bandeau de 34 px « Hiérarchie » + œil + loupe — mais dessiné avec
//  `NextItemRect`, donc DANS le cadre de défilement. Sur ses deux captures à
//  34 secondes d'écart, le titre est passé de y = 123 à y = 101, coupé par la
//  barre d'outils : il était parti avec le contenu.
//
//  🔴 UNE FENÊTRE GARDE SON EN-TÊTE, et ce n'est pas une question d'allure :
//     c'est la poignée qui la rend saisissable, donc détachable. Un en-tête
//     qui défile n'est plus une poignée, c'est du contenu qui lui ressemble.
//
// =============================================================================
//  LES CRITÈRES
// =============================================================================
//   (e1) la bande est en haut de la zone, pleine largeur, à la hauteur demandée.
//   (e2) 🔴 LE CRITÈRE QUI COMPTE : après plusieurs crans de molette, la bande
//        est AU MÊME PIXEL. C'est toute la raison d'être de la fonction.
//   (e3) le contenu commence SOUS la bande — sinon la première ligne se
//        peindrait par-dessus l'en-tête.
//   (e4) ET LE CONTENU, LUI, DÉFILE. Une bande fixe obtenue en figeant tout le
//        panneau serait verte en (e2) et parfaitement inutile.
//   (e5) NÉGATIF : sans bande, rien ne bouge pour les appelants existants — le
//        contenu commence au HAUT de la zone, comme avant.
//   (e6) NÉGATIF : une bande plus haute que la zone est REFUSÉE. Elle ne
//        laisserait aucun contenu, et rendre une hauteur négative ferait un
//        dégât silencieux plus loin.
// -----------------------------------------------------------------------------

static float32 gRefSansBande = -1.f;

static void CasBandeFixe() {
	// =====================================================================
	printf("\n-- (b23) une bande fixe en haut d'une zone defilable\n");
	// =====================================================================

	using namespace nkentseu;
	using namespace nkentseu::nkgui;

	const NkRect kZone{0.f, 0.f, 240.f, 300.f};
	const float32 kBande = 34.f;

	/// Une image : bande (ou non), puis douze lignes de contenu.
	/// Rend le rectangle de la bande et celui de la PREMIERE ligne de contenu.
	auto image = [&](NkGuiContext &ctx, bool avecBande, float32 hauteurBande, float32 molette,
					 NkRect &bandeOut, NkRect &premierOut, bool &poseeOut) {
		ctx.input.mousePos = {kZone.x + kZone.w * 0.5f, kZone.y + kZone.h * 0.5f};
		ctx.input.wheel = molette;
		ctx.BeginFrame(1.f / 60.f);
		ctx.BeginLayout({0.f, 0.f, (float32)ctx.viewW, (float32)ctx.viewH});
		ctx.DL().Reset();
		bandeOut = {0.f, 0.f, 0.f, 0.f};
		premierOut = {0.f, 0.f, 0.f, 0.f};
		poseeOut = false;
		if (BeginChild(ctx, "panneau", kZone, true)) {
			if (avecBande) {
				NkRect b;
				if (NkGuiBandeFixeDebut(ctx, hauteurBande, b)) {
					poseeOut = true;
					bandeOut = b;
					NkGuiBandeFixeFin(ctx);
				}
			}
			for (int32 i = 0; i < 12; ++i) {
				const NkRect r = ctx.NextItemRect(-1.f, 22.f);
				if (i == 0)
					premierOut = r;
			}
			EndChild(ctx);
		}
		ctx.EndFrame();
	};

	// ── (e1) LA BANDE EST OU ON L'ATTEND ───────────────────────────────
	NkRect bande0{0.f, 0.f, 0.f, 0.f}, premier0{0.f, 0.f, 0.f, 0.f};
	{
		NkGuiContext ctx;
		ctx.viewW = 400;
		ctx.viewH = 400;
		if (g_fontOk)
			ctx.font = &g_font;
		bool posee = false;
		image(ctx, true, kBande, 0.f, bande0, premier0, posee);
		image(ctx, true, kBande, 0.f, bande0, premier0, posee);
		printf("        bande = %.0f,%.0f %.0fx%.0f ; premiere ligne y = %.0f\n", (double)bande0.x,
			   (double)bande0.y, (double)bande0.w, (double)bande0.h, (double)premier0.y);
		Check(posee, "(e1) la bande est acceptee");
		Check(bande0.y <= kZone.y + 0.5f, "(e1) elle est en HAUT de la zone");
		Check(bande0.h > kBande - 0.5f && bande0.h < kBande + 0.5f,
			  "(e1) a la hauteur demandee");
		Check(bande0.w > kZone.w - 0.5f, "(e1) et sur toute la largeur");

		// ── (e3) LE CONTENU COMMENCE SOUS LA BANDE ──────────────────
		Check(premier0.y >= bande0.y + bande0.h - 0.5f,
			  "(e3) la premiere ligne commence SOUS la bande");

		// ── (e2) 🔴 ELLE NE BOUGE PAS QUAND ON DEFILE ───────────────
		NkRect bandeN{0.f, 0.f, 0.f, 0.f}, premierN{0.f, 0.f, 0.f, 0.f};
		for (uint32 i = 0; i < 10u; ++i)
			image(ctx, true, kBande, -3.f, bandeN, premierN, posee);
		printf("        apres 10 crans : bande y = %.1f (etait %.1f) ; premiere ligne y = %.1f "
			   "(etait %.1f)\n",
			   (double)bandeN.y, (double)bande0.y, (double)premierN.y, (double)premier0.y);
		const float32 derive = (bandeN.y > bande0.y ? bandeN.y - bande0.y : bande0.y - bandeN.y);
		Check(derive <= 0.5f, "(e2) LA BANDE EST AU MEME PIXEL -- elle ne defile pas");
		// ── (e4) ET LE CONTENU, LUI, A DEFILE ───────────────────────
		Check(premierN.y < premier0.y - 1.f,
			  "(e4) ET LE CONTENU A DEFILE -- la bande n'a pas fige le panneau");
	}

	// ── (e5) NEGATIF : sans bande, rien ne change ──────────────────────
	{
		NkGuiContext ctx;
		ctx.viewW = 400;
		ctx.viewH = 400;
		if (g_fontOk)
			ctx.font = &g_font;
		NkRect b{0.f, 0.f, 0.f, 0.f}, pr{0.f, 0.f, 0.f, 0.f};
		bool posee = true;
		image(ctx, false, 0.f, 0.f, b, pr, posee);
		image(ctx, false, 0.f, 0.f, b, pr, posee);
		gRefSansBande = pr.y;
		printf("        [sans bande] premiere ligne y = %.1f -- c'est la REFERENCE\n", (double)pr.y);
		Check(!posee, "(e5) aucune bande n'est posee");
		// ⚠️ LA REFERENCE SE MESURE, ELLE NE SE DEVINE PAS. Une premiere version
		//    de ce critere attendait `y == haut de zone` : faux, la disposition
		//    pose d'elle-meme une marge de 10 px avant le premier element. Le
		//    critere echouait donc sur un code juste. On retient ce que le cadre
		//    SANS bande produit, et on y compare les autres cas.
		Check(pr.y < kZone.y + kBande - 0.5f,
			  "(e5) NEGATIF : sans bande, le contenu n'est PAS repousse d'un en-tete");
	}

	// ── (e6) NEGATIF : une bande plus haute que la zone est REFUSEE ────
	{
		NkGuiContext ctx;
		ctx.viewW = 400;
		ctx.viewH = 400;
		if (g_fontOk)
			ctx.font = &g_font;
		NkRect b{0.f, 0.f, 0.f, 0.f}, pr{0.f, 0.f, 0.f, 0.f};
		bool posee = true;
		image(ctx, true, kZone.h + 10.f, 0.f, b, pr, posee);
		image(ctx, true, kZone.h + 10.f, 0.f, b, pr, posee);
		printf("        [bande de %.0f dans une zone de %.0f] acceptee = %s ; premiere ligne y = "
			   "%.1f\n",
			   (double)(kZone.h + 10.f), (double)kZone.h, posee ? "OUI" : "non", (double)pr.y);
		Check(!posee, "(e6) NEGATIF : une bande qui mange toute la zone est REFUSEE");
		const float32 ecartRef = (pr.y > gRefSansBande ? pr.y - gRefSansBande : gRefSansBande - pr.y);
		Check(ecartRef <= 0.5f,
			  "(e6) et la zone est INTACTE -- au pixel pres de la reference sans bande");
	}
}

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
