#pragma once
// -----------------------------------------------------------------------------
// @File    CasPastillesOnglets.h
// @Brief   (b22) Une barre d'onglets sait-elle être un rail de pastilles —
//          carrée, refermable, et sans tenir la sélection ?
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  CE QUE ÇA MESURE
// =============================================================================
//  Rodolf, 28/09 : « ce tiroir est aussi un ensemble de fenêtres, dont les
//  onglets verticaux ont été remplacés par les pastilles ».
//
//  Le rail de la coquille refaisait à la main la géométrie, le survol, la
//  sélection et l'infobulle d'une barre d'onglets — et n'émettait RIEN dans le
//  relevé. Pour qu'il puisse déléguer, la barre a dû apprendre trois choses.
//  Ce cas les éprouve une par une, et garde un négatif pour chacune.
//
//   (p1) PASTILLE : l'onglet est un CARRÉ de `cote`, et c'est l'appelant qui
//        peint son contenu. Le critère est géométrique (w == h == cote) ET
//        compte les appels du crochet : un carré sans crochet appelé serait
//        une pastille vide, et « la barre s'est montée » resterait vert.
//   (p2) REFERMABLE : cliquer l'onglet ACTIF rend −1.
//        NÉGATIF (p2b) : SANS `refermable`, le même clic garde l'onglet
//        sélectionné. Sans ce négatif, une version qui rendrait −1 à tous les
//        clics passerait (p2) les yeux fermés.
//   (p3) SÉLECTION IMPOSÉE : quand l'appelant la fournit, la barre ne mémorise
//        PAS la sienne. On impose 2, puis 0, et la barre doit suivre.
//        ⚠️ LE CRITÈRE EST « ELLE SUIT », PAS « ELLE REND CE QU'ON DONNE » :
//           une barre qui renverrait bêtement son entrée passerait aussi. On
//           vérifie donc dans le RELEVÉ quel onglet porte `SELECTION`.
//   (p4) LES PASTILLES ENTRENT DANS LE RELEVÉ. C'est le bénéfice direct, et
//        il se mesure : `NkGuiNature::Onglet` doit rendre `count` notes.
// -----------------------------------------------------------------------------

static void CasPastillesOnglets() {
	// =====================================================================
	printf("\n-- (b22) la barre d'onglets sait etre un rail de pastilles\n");
	// =====================================================================

	using namespace nkentseu;
	using namespace nkentseu::nkgui;

	static const char *kNoms[3] = {"Styles", "Variables", "Calques"};
	static const char *kBulles[3] = {"Les styles", "Les variables", "Les calques"};

	/// Ce que le crochet d'icone a vu : combien d'appels, et le dernier
	/// rectangle. Un compteur SEUL ne dirait pas si le carre est carre.
	struct Vu {
			uint32 appels = 0u;
			NkRect dernier{0.f, 0.f, 0.f, 0.f};
			float32 minCote = 1.0e9f, maxCote = -1.0e9f;
			int32 actifVu = -1;
	};

	auto crochet = [](NkGuiContext &, const NkRect &r, int32 index, bool actif, bool,
					  void *u) {
		auto *v = static_cast<Vu *>(u);
		++v->appels;
		v->dernier = r;
		const float32 c = r.w < r.h ? r.w : r.h;
		const float32 g = r.w > r.h ? r.w : r.h;
		if (c < v->minCote)
			v->minCote = c;
		if (g > v->maxCote)
			v->maxCote = g;
		if (actif)
			v->actifVu = index;
	};

	const NkRect kCol{0.f, 0.f, 28.f, 300.f};

	/// Une image complete sur une colonne de 28 px, comme le rail.
	auto image = [&](NkGuiContext &ctx, const NkGuiTabBarOptions &opt, NkVec2 souris,
					 bool bouton) -> int32 {
		ctx.input.mousePos = souris;
		ctx.input.mouseDown[0] = bouton;
		ctx.BeginFrame(1.f / 60.f);
		ctx.BeginLayout(kCol);
		ctx.DL().Reset();
		ctx.layout.itemSpacingY = 0.f;
		const int32 s = TabBarOpt(ctx, "rail.epreuve", kNoms, 3, nullptr, opt);
		ctx.EndFrame();
		return s;
	};

	// ── (p1) LA PASTILLE EST CARREE ET SON CROCHET EST APPELE ──────────
	{
		NkGuiContext ctx;
		ctx.viewW = 200;
		ctx.viewH = 400;
		if (g_fontOk)
			ctx.font = &g_font;
		NkGuiIntrospectActiver(ctx, true);
		Vu vu;
		NkGuiTabBarOptions opt;
		opt.vertical = true;
		opt.cote = 28.f;
		opt.refermable = true;
		opt.selectionImposee = -1;
		opt.infobulles = kBulles;
		opt.icone = crochet;
		opt.iconeUser = &vu;
		(void)image(ctx, opt, {-500.f, -500.f}, false);
		printf("        appels du crochet = %u ; cote min = %.1f, max = %.1f\n", vu.appels,
			   (double)vu.minCote, (double)vu.maxCote);
		Check(vu.appels == 3u, "(p1) le crochet est appele UNE fois par pastille");
		Check(vu.minCote > 27.f && vu.maxCote < 29.f,
			  "(p1) et le rectangle est CARRE, au cote demande");

		// ── (p4) LES PASTILLES SONT DANS LE RELEVE ────────────────────
		int32 nOnglets = 0;
		{
			int32 n = 0;
			const NkGuiNote *notes = NkGuiIntrospectNotes(ctx, n);
			for (int32 i = 0; i < n; ++i)
				if (notes[i].nature == NkGuiNature::Onglet)
					++nOnglets;
		}
		printf("        notes de nature `Onglet` = %d\n", nOnglets);
		Check(nOnglets == 3,
			  "(p4) LES PASTILLES ENTRENT DANS LE RELEVE -- elles n'y etaient pas");
	}

	// ── (p2) REFERMABLE : cliquer l'actif rend -1 ──────────────────────
	{
		NkGuiContext ctx;
		ctx.viewW = 200;
		ctx.viewH = 400;
		if (g_fontOk)
			ctx.font = &g_font;
		Vu vu;
		NkGuiTabBarOptions opt;
		opt.vertical = true;
		opt.cote = 28.f;
		opt.refermable = true;
		opt.infobulles = kBulles;
		opt.icone = crochet;
		opt.iconeUser = &vu;
		// La 2e pastille : centre a y = 28 + 14.
		const NkVec2 surDeux{14.f, 42.f};
		// ⚠️ TROIS IMAGES ET UN RELACHEMENT : `ButtonBehavior` valide au
		//    RELACHEMENT, et le survol se resout sur l'image precedente. Un
		//    banc d'une seule image conclurait « le clic ne marche pas ».
		opt.selectionImposee = -2; // la barre tient la sienne
		(void)image(ctx, opt, surDeux, false);
		(void)image(ctx, opt, surDeux, true);
		const int32 apresClic = image(ctx, opt, surDeux, false);
		printf("        apres un clic sur la 2e pastille : %d (attendu 1)\n", apresClic);
		Check(apresClic == 1, "(p2) le clic SELECTIONNE la pastille visee");
		// Deuxieme clic au MEME endroit : elle doit se refermer.
		(void)image(ctx, opt, surDeux, true);
		const int32 apresSecond = image(ctx, opt, surDeux, false);
		printf("        apres un second clic au meme endroit : %d (attendu -1)\n", apresSecond);
		Check(apresSecond == -1,
			  "(p2) REFERMABLE : recliquer la pastille ACTIVE la deselectionne");
	}

	// ── (p2b) LE NEGATIF : sans `refermable`, l'onglet RESTE ───────────
	{
		NkGuiContext ctx;
		ctx.viewW = 200;
		ctx.viewH = 400;
		if (g_fontOk)
			ctx.font = &g_font;
		NkGuiTabBarOptions opt;
		opt.vertical = true; // onglets ORDINAIRES : ni pastille, ni refermable
		const NkVec2 surDeux{14.f, 42.f};
		(void)image(ctx, opt, surDeux, false);
		(void)image(ctx, opt, surDeux, true);
		const int32 un = image(ctx, opt, surDeux, false);
		(void)image(ctx, opt, surDeux, true);
		const int32 deux = image(ctx, opt, surDeux, false);
		printf("        [sans refermable] premier clic : %d, second : %d (attendu le meme)\n", un,
			   deux);
		Check(deux >= 0, "(p2b) NEGATIF : sans `refermable`, un onglet ne se deselectionne PAS");
		Check(deux == un, "(p2b) et il reste le MEME onglet");
	}

	// ── (p3) SELECTION IMPOSEE : la barre suit, elle ne memorise pas ───
	{
		NkGuiContext ctx;
		ctx.viewW = 200;
		ctx.viewH = 400;
		if (g_fontOk)
			ctx.font = &g_font;
		NkGuiIntrospectActiver(ctx, true);
		Vu vu;
		NkGuiTabBarOptions opt;
		opt.vertical = true;
		opt.cote = 28.f;
		opt.refermable = true;
		opt.icone = crochet;
		opt.iconeUser = &vu;

		/// Quel onglet porte SELECTION dans le releve — la question ne se pose
		/// pas a la valeur RENDUE, qui pourrait n'etre qu'un echo de l'entree.
		auto selonReleve = [](NkGuiContext &c) -> int32 {
			int32 n = 0, k = 0;
			const NkGuiNote *notes = NkGuiIntrospectNotes(c, n);
			for (int32 i = 0; i < n; ++i)
				if (notes[i].nature == NkGuiNature::Onglet) {
					if (notes[i].etats & NK_GUI_ETAT_SELECTION)
						return k;
					++k;
				}
			return -1;
		};

		opt.selectionImposee = 2;
		(void)image(ctx, opt, {-500.f, -500.f}, false);
		const int32 vuDeux = selonReleve(ctx);
		vu.actifVu = -1;
		opt.selectionImposee = 0;
		(void)image(ctx, opt, {-500.f, -500.f}, false);
		const int32 vuZero = selonReleve(ctx);
		printf("        impose 2 -> le releve marque %d ; impose 0 -> %d\n", vuDeux, vuZero);
		Check(vuDeux == 2, "(p3) la barre PEINT l'onglet impose, pas le sien");
		Check(vuZero == 0, "(p3) et elle SUIT quand l'appelant change d'avis");
		Check(vu.actifVu == 0, "(p3) le crochet recoit `actif` sur le bon indice");
	}
}

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
