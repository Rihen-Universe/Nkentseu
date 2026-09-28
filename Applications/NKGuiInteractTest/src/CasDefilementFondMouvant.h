#pragma once
// -----------------------------------------------------------------------------
// @File    CasDefilementFondMouvant.h
// @Brief   (b21) Un contenu qui « remplit jusqu'en bas » dans une zone
//          défilable : sa limite de défilement doit rester FIXE.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  CE QUE ÇA MESURE
// =============================================================================
//  Rodolf, 28/09 : « les scrollbar dans les panneaux ne fonctionnent pas, non
//  seulement pas bien adaptées à la hauteur, mais en plus le scroll dépasse
//  tout le contenu de la zone ».
//
//  🔴 LA CAUSE EST CIRCULAIRE, ET C'EST POUR ÇA QU'ELLE PART À L'INFINI.
//
//  Un panneau ancré demande sa hauteur restante au BAS VISIBLE de son cadre de
//  défilement (`area.y + area.h`, une ordonnée ÉCRAN). Mais dans ce cadre, le
//  curseur de disposition est décalé par le défilement : le contenu commence à
//  `area.y - S`. Donc :
//
//      défilement = S
//        -> le curseur est S px plus haut
//        -> « bas visible - curseur » GRANDIT de S
//        -> le contenu mesure `vue + S`
//        -> maxScroll = contenu - vue = S
//
//  LA LIMITE DEVIENT ÉGALE AU DÉFILEMENT. Elle ne l'arrête donc jamais : chaque
//  cran de molette en autorise un de plus, sans fin. Et le pouce vaut
//  `vue / (vue + S)`, donc il rétrécit à mesure qu'on descend — le trait
//  minuscule collé en haut de la capture du 28/09.
//
// =============================================================================
//  LES CRITÈRES
// =============================================================================
//   (d1) AU REPOS : un contenu qui remplit exactement la vue ne laisse RIEN à
//        défiler. `maxY == 0`.
//   (d2) 🔴 LE CRITÈRE QUI COMPTE : après plusieurs crans de molette, `maxY`
//        doit TOUJOURS valoir 0. S'il suit le défilement, la boucle est là.
//        ⚠️ ON MESURE `maxY`, PAS `scrollY`. Un banc qui vérifierait « le
//           défilement est revenu à 0 » serait vert avec la boucle : la valeur
//           EST bornée à chaque image — simplement par une borne qui vient de
//           monter. C'est la BORNE qui ment, pas la position.
//   (d3) NÉGATIF, ET IL EST INDISPENSABLE : un contenu RÉELLEMENT plus haut que
//        la vue garde un `maxY` strictement positif ET CONSTANT d'une image à
//        l'autre. Sans lui, « maxY = 0 partout » passerait (d1) et (d2) les
//        yeux fermés — c'est-à-dire en supprimant le défilement.
// -----------------------------------------------------------------------------

static void CasDefilementFondMouvant() {
	// =====================================================================
	printf("\n-- (b21) un contenu qui remplit jusqu'en bas : sa limite reste FIXE\n");
	// =====================================================================

	using namespace nkentseu;
	using namespace nkentseu::nkgui;

	/// ⚠️ ON APPELLE LA VRAIE FONCTION DE NKGui, pas une copie. Une copie
	///    locale aurait mesure le banc lui-meme : elle resterait juste le jour
	///    ou `NkGuiBasVisible` deviendrait fausse, et ce cas passerait au vert
	///    sur un noyau casse.
	auto basVisible = [](NkGuiContext &c) -> float32 {
		float32 bas = 0.f;
		if (NkGuiBasVisible(c, bas))
			return bas;
		return c.layout.cursor.y + 600.f;
	};

	/// La borne de defilement retenue pour cette zone, telle que la frame
	/// PRECEDENTE l'a ecrite. C'est elle qui autorise ou refuse le cran suivant.
	auto borne = [](NkGuiContext &c, const char *idStr) -> float32 {
		const NkGuiId id = c.GetId(idStr);
		for (uint32 i = 0; i < (uint32)c.scrollKeys.Size(); ++i)
			if (c.scrollKeys[i] == id)
				return c.scrollVals[i].maxY;
		return -1.f; // jamais vue
	};

	auto position = [](NkGuiContext &c, const char *idStr) -> float32 {
		const NkGuiId id = c.GetId(idStr);
		for (uint32 i = 0; i < (uint32)c.scrollKeys.Size(); ++i)
			if (c.scrollKeys[i] == id)
				return c.scrollVals[i].y;
		return -1.f;
	};

	const NkRect kZone{0.f, 0.f, 240.f, 300.f};

	// `remplit` : le contenu se cale sur le bas visible (le cas de Rodolf).
	// sinon    : le contenu a une hauteur FIXE de `hFixe` (le negatif).
	// `apres`  : hauteur d'un item dessine APRES la zone qui remplit. C'est lui
	//            qui donne l'amorce -- voir (d4).
	auto tournerAvecSuite = [&](NkGuiContext &ctx, bool remplit, float32 hFixe, float32 molette,
								float32 apres) {
		ctx.input.mousePos = {kZone.x + kZone.w * 0.5f, kZone.y + kZone.h * 0.5f};
		ctx.input.wheel = molette;
		ctx.BeginFrame(1.f / 60.f);
		ctx.BeginLayout({0.f, 0.f, (float32)ctx.viewW, (float32)ctx.viewH});
		ctx.DL().Reset();
		if (BeginChild(ctx, "panneau", kZone, true)) {
			Text(ctx, "BANDE");
			float32 h;
			if (remplit) {
				h = basVisible(ctx) - ctx.layout.cursor.y - 2.f * ctx.layout.itemSpacingY;
				if (h < 60.f)
					h = 60.f;
			} else {
				h = hFixe;
			}
			(void)ctx.NextItemRect(-1.f, h);
			if (apres > 0.f)
				(void)ctx.NextItemRect(-1.f, apres);
			EndChild(ctx);
		}
		ctx.EndFrame();
	};

	auto tourner = [&](NkGuiContext &ctx, bool remplit, float32 hFixe, float32 molette) {
		ctx.input.mousePos = {kZone.x + kZone.w * 0.5f, kZone.y + kZone.h * 0.5f};
		ctx.input.wheel = molette;
		ctx.BeginFrame(1.f / 60.f);
		ctx.BeginLayout({0.f, 0.f, (float32)ctx.viewW, (float32)ctx.viewH});
		ctx.DL().Reset();
		if (BeginChild(ctx, "panneau", kZone, true)) {
			// Une bande de titre, comme « PAGES » dans la Hierarchie.
			Text(ctx, "BANDE");
			float32 h;
			if (remplit) {
				h = basVisible(ctx) - ctx.layout.cursor.y - 2.f * ctx.layout.itemSpacingY;
				if (h < 60.f)
					h = 60.f;
			} else {
				h = hFixe;
			}
			// La zone que l'arbre occupe. `NextItemRect` la RESERVE : c'est elle
			// qui fait avancer le curseur, donc qui decide la hauteur du contenu.
			(void)ctx.NextItemRect(-1.f, h);
			EndChild(ctx);
		}
		ctx.EndFrame();
	};

	// ── (d1) AU REPOS ──────────────────────────────────────────────────
	{
		NkGuiContext ctx;
		ctx.viewW = 400;
		ctx.viewH = 400;
		if (g_fontOk)
			ctx.font = &g_font;
		tourner(ctx, true, 0.f, 0.f);
		tourner(ctx, true, 0.f, 0.f);
		const float32 m = borne(ctx, "panneau");
		printf("        [repos]  maxY = %.1f  (attendu 0)\n", (double)m);
		Check(m >= 0.f, "(d1) la zone a bien un etat de defilement");
		Check(m <= 0.5f, "(d1) un contenu qui remplit la vue ne laisse RIEN a defiler");
	}

	// ── (d2) 🔴 APRES PLUSIEURS CRANS ──────────────────────────────────
	{
		NkGuiContext ctx;
		ctx.viewW = 400;
		ctx.viewH = 400;
		if (g_fontOk)
			ctx.font = &g_font;
		tourner(ctx, true, 0.f, 0.f);
		float32 pire = 0.f;
		for (uint32 i = 0; i < 12u; ++i) {
			tourner(ctx, true, -3.f, 0.f); // molette vers le BAS
			const float32 m = borne(ctx, "panneau");
			if (m > pire)
				pire = m;
		}
		const float32 mFin = borne(ctx, "panneau");
		const float32 sFin = position(ctx, "panneau");
		printf("        [12 crans] maxY = %.1f  (pire vue : %.1f) ; defilement = %.1f\n",
			   (double)mFin, (double)pire, (double)sFin);
		// ⚠️ LA BORNE, PAS LA POSITION. Avec la boucle, `scrollY` est borne a
		//    chaque image -- par une borne qui vient de monter. Seul `maxY`
		//    trahit le fond qui recule.
		Check(pire <= 0.5f,
			  "(d2) LA LIMITE NE SUIT PAS LE DEFILEMENT -- le fond ne recule pas");
	}

	// ── (d3) NEGATIF : un vrai debordement defile, et sa borne est STABLE ──
	{
		NkGuiContext ctx;
		ctx.viewW = 400;
		ctx.viewH = 400;
		if (g_fontOk)
			ctx.font = &g_font;
		const float32 hFixe = 900.f; // trois fois la vue : il DOIT defiler
		tourner(ctx, false, hFixe, 0.f);
		tourner(ctx, false, hFixe, 0.f);
		const float32 m0 = borne(ctx, "panneau");
		for (uint32 i = 0; i < 8u; ++i)
			tourner(ctx, false, hFixe, -3.f);
		const float32 m1 = borne(ctx, "panneau");
		const float32 s1 = position(ctx, "panneau");
		printf("        [hauteur fixe %.0f] maxY avant = %.1f, apres 8 crans = %.1f ; "
			   "defilement = %.1f\n",
			   (double)hFixe, (double)m0, (double)m1, (double)s1);
		Check(m0 > 1.f, "(d3) un contenu plus haut que la vue DEFILE encore");
		const float32 ecart = (m1 > m0 ? m1 - m0 : m0 - m1);
		Check(ecart <= 1.f, "(d3) ET SA BORNE NE BOUGE PAS d'une image a l'autre");
		Check(s1 > 1.f, "(d3) le defilement a reellement avance");
	}

	// ── (d4) 🔴 LE CAS REEL : « remplit jusqu'en bas » PUIS du contenu ──
	// =====================================================================
	//  (d1) et (d2) sont verts sur le code NON corrige, et c'est instructif :
	//  le defilement est borne par le `maxY` de l'image PRECEDENTE **avant**
	//  la disposition. Avec `maxY = 0`, la molette est ramenee a 0 et la
	//  boucle ne s'amorce jamais. Mon raisonnement etait juste sur le
	//  mecanisme, faux sur les conditions de son declenchement.
	//
	//  ⚠️ CE QUI MANQUAIT : UNE AMORCE. `DessinerComposants` dessine l'etat
	//     vide et son menu APRES la zone qui remplit. Le contenu depasse donc
	//     la vue d'entree, `maxY` naît positif, le defilement devient possible
	//     -- et c'est LUI qui ouvre la boucle que (d2) ne pouvait pas ouvrir.
	//
	//  Le critere : `maxY` doit rester EGAL A LA SUITE (`apres` + espacement),
	//  et non grandir a chaque cran.
	{
		NkGuiContext ctx;
		ctx.viewW = 400;
		ctx.viewH = 400;
		if (g_fontOk)
			ctx.font = &g_font;
		const float32 kSuite = 40.f;
		tournerAvecSuite(ctx, true, 0.f, 0.f, kSuite);
		tournerAvecSuite(ctx, true, 0.f, 0.f, kSuite);
		const float32 m0 = borne(ctx, "panneau");
		float32 pire = m0;
		for (uint32 i = 0; i < 15u; ++i) {
			tournerAvecSuite(ctx, true, 0.f, -3.f, kSuite);
			const float32 m = borne(ctx, "panneau");
			if (m > pire)
				pire = m;
		}
		const float32 m1 = borne(ctx, "panneau");
		const float32 s1 = position(ctx, "panneau");
		printf("        [remplit + suite de %.0f] maxY depart = %.1f, apres 15 crans = %.1f "
			   "(pire %.1f) ; defilement = %.1f\n",
			   (double)kSuite, (double)m0, (double)m1, (double)pire, (double)s1);
		Check(m0 > 1.f, "(d4) la suite deborde bien : il y a de quoi defiler au depart");
		// ⚠️ LA BORNE NE DOIT PAS BOUGER. Si elle suit le defilement, chaque
		//    cran en autorise un de plus : c'est le « scroll qui depasse tout
		//    le contenu » que Rodolf decrit.
		const float32 derive = (m1 > m0 ? m1 - m0 : 0.f);
		printf("        derive de la borne : %.1f px\n", (double)derive);
		Check(derive <= 1.f,
			  "(d4) LA BORNE NE DERIVE PAS -- le fond ne recule pas quand on descend");
		Check(s1 <= m1 + 1.f, "(d4) et le defilement reste dans sa borne");
	}
}

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
