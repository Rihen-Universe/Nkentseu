#pragma once
// -----------------------------------------------------------------------------
// @File    RecetteSelection.h
// @Brief   La recette « Selection » — sortie de `main.cpp` le 28/09/2026.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  POURQUOI CE FICHIER EXISTE
// =============================================================================
//  Rodolf, 28/09 : « tout fichier de plus de 1k ligne reste trop volumineux [...]
//  on peut encore subdiviser dans des fonctions et des fichiers spécifiques ».
//
//  `main.cpp` faisait 10 765 lignes, dont **6 497 de recettes** -- des bancs de
//  mesure qui ne tournent pas quand l'application s'ouvre. Ce n'etait donc pas
//  l'application qui etait grosse : c'etait un fichier qui hebergeait huit bancs.
//
//  Apres le decoupage : `main.cpp` tombe a 4 268 lignes.
//
// =============================================================================
//  ⚠️ CE FICHIER S'INCLUT DEPUIS `main.cpp`, ET SEULEMENT DE LA
// =============================================================================
//  Une recette lit l'etat global de l'application (`gDesign`, les tables
//  d'actions, les crochets). Elle n'est pas une bibliotheque : c'est un banc qui
//  vit dans l'unite de traduction du programme.
//
//  Le decoupage est un RANGEMENT, pas une modularisation : rien n'a ete renomme,
//  aucune signature n'a bouge, et le code est identique a l'octet pres. C'est ce
//  qui permet de le verifier par la construction et par le verdict des recettes
//  elles-memes -- si l'une d'elles changeait de resultat, le deplacement aurait
//  ete faux.
// -----------------------------------------------------------------------------

static nkentseu::int32 RecetteSelection() {
	using namespace nkuidesign;
	using nkentseu::int32;
	using nkentseu::uint32;
	int32 cas = 0, echecs = 0;
	auto verdict = [&](const char *nom, bool ok, const char *detail) {
		++cas;
		printf("%s  %s%s%s\n", ok ? "OK   " : "ECHEC", nom, (detail && *detail) ? "  -- " : "",
			   (detail && *detail) ? detail : "");
		if (!ok)
			++echecs;
	};
	auto ancree = [](const NkString &s) -> bool {
		return s.Size() > 0 && s.Data() && s.Contains("nkuidoc") && s.Contains("noeud");
	};
	static DesignState st;
	const NkPaintRect surface = {0.f, 0.f, 1200.f, 800.f};
	int32 cadre = -1, a = -1, b = -1, c = -1, profond = -1;
	auto scene = [&]() {
		st.doc.NewDocument("recette selection", NkAuthor::Humain);
		cadre = st.doc.AddChild(0, "", NkAuthor::Humain);
		{
			NkUINode &f = st.doc.nodes[(uint32)cadre];
			f.label = NkString("Page");
			f.shape = NkString("frame");
			f.layout.kind = NkLayoutKind::Free;
			f.width.mode = NkSizeMode::Fixed;
			f.width.value = 600.f;
			f.height.mode = NkSizeMode::Fixed;
			f.height.value = 400.f;
		}
		auto poser = [&](int32 parent, const char *nom, float32 x, float32 y, float32 w,
						 float32 h) {
			const int32 i = st.doc.AddChild(parent, "", NkAuthor::Humain);
			NkUINode &n = st.doc.nodes[(uint32)i];
			n.label = NkString(nom);
			n.shape = NkString("rect");
			n.layout.kind = NkLayoutKind::Free;
			n.posX = x;
			n.posY = y;
			n.width.mode = NkSizeMode::Fixed;
			n.width.value = w;
			n.height.mode = NkSizeMode::Fixed;
			n.height.value = h;
			return i;
		};
		a = poser(cadre, "A", 20.f, 20.f, 100.f, 80.f);
		b = poser(cadre, "B", 200.f, 150.f, 60.f, 40.f);
		c = poser(cadre, "C", 400.f, 300.f, 50.f, 50.f);
		profond = poser(a, "Profond", 10.f, 10.f, 30.f, 20.f);
		st.Recompute(surface);
		st.SelectClear();
	};

	// ── 1. LES DEUX TABLES, ET ELLES SONT DIFFERENTES A DESSEIN ─────────────
	{
		const bool okToile = NkGesteToile(false, false) == NkGesteSel::Remplacer
							 && NkGesteToile(true, false) == NkGesteSel::Profond
							 && NkGesteToile(false, true) == NkGesteSel::Basculer;
		const bool okListe = NkGesteListe(false, false) == NkGesteSel::Remplacer
							 && NkGesteListe(true, false) == NkGesteSel::Basculer
							 && NkGesteListe(false, true) == NkGesteSel::Basculer;
		// LE PIEGE 2, TENU EXPLICITEMENT : les deux tables ne doivent PAS etre
		// identiques -- si un jour quelqu'un « harmonise », ce cas le dit.
		const bool differentes = NkGesteToile(true, false) != NkGesteListe(true, false);
		verdict("1. les deux tables : toile (Ctrl=profond, Maj=basculer) et liste "
				"(Ctrl ou Maj=basculer), et elles DIFFERENT a dessein",
				okToile && okListe && differentes,
				differentes ? "Ctrl : toile=profond, liste=basculer" : "TABLES CONFONDUES");
	}
	// ── 2. LA RACINE NE S'INVITE JAMAIS (piege 1) ───────────────────────────
	scene();
	{
		st.sel.Set(0); // l'etat de demarrage : la racine est « selectionnee »
		st.selected = 0;
		NkAppliquerGeste(st.doc, st.sel, st.selected, a, NkGesteSel::Basculer);
		const bool ok = st.sel.Count() == 1 && st.sel.Primary() == a && !st.sel.Contains(0);
		char d[96];
		snprintf(d, sizeof(d), "compte=%u, principal=%d (racine %s)", st.sel.Count(),
				 st.sel.Primary(), st.sel.Contains(0) ? "PRESENTE" : "absente");
		verdict("2. Maj+clic depuis l'etat de demarrage : la RACINE ne rentre pas dans la "
				"selection",
				ok, d);
		// et on ne peut pas l'y forcer
		NkAppliquerGeste(st.doc, st.sel, st.selected, 0, NkGesteSel::Basculer);
		verdict("2b. la racine refuse meme un geste qui la vise explicitement",
				!st.sel.Contains(0) && st.sel.Count() == 1, "");
	}
	// ── 3. BASCULER AJOUTE PUIS RETIRE ──────────────────────────────────────
	scene();
	{
		NkAppliquerGeste(st.doc, st.sel, st.selected, a, NkGesteSel::Remplacer);
		NkAppliquerGeste(st.doc, st.sel, st.selected, b, NkGesteSel::Basculer);
		const uint32 deux = st.sel.Count();
		NkAppliquerGeste(st.doc, st.sel, st.selected, b, NkGesteSel::Basculer);
		const uint32 un = st.sel.Count();
		NkAppliquerGeste(st.doc, st.sel, st.selected, c, NkGesteSel::Remplacer);
		const uint32 remplace = st.sel.Count();
		char d[96];
		snprintf(d, sizeof(d), "ajout=%u, retrait=%u, remplacement=%u", deux, un, remplace);
		verdict("3. Basculer ajoute puis RETIRE ; Remplacer ecrase (1, pas 3)",
				deux == 2 && un == 1 && remplace == 1 && st.sel.Primary() == c, d);
	}
	// ── 4. LE PRINCIPAL EST LE PREMIER DESIGNE (l'Inspecteur le nomme) ──────
	scene();
	{
		NkAppliquerGeste(st.doc, st.sel, st.selected, b, NkGesteSel::Remplacer);
		NkAppliquerGeste(st.doc, st.sel, st.selected, a, NkGesteSel::Basculer);
		NkAppliquerGeste(st.doc, st.sel, st.selected, c, NkGesteSel::Basculer);
		char d[96];
		snprintf(d, sizeof(d), "principal=%d (B=%d), compte=%u", st.selected, b,
				 st.sel.Count());
		verdict("4. le PRINCIPAL reste le premier designe, pas le dernier",
				st.selected == b && st.sel.Count() == 3, d);
	}
	// ── 5. L'ENGLOBANT (piege 3 : un seul calcul, trois consommateurs) ──────
	scene();
	{
		st.sel.Set(a);
		st.sel.Add(b);
		st.selected = st.sel.Primary();
		NkPaintRect eng = {0.f, 0.f, 0.f, 0.f};
		const bool ok = NkRectSelection(st.doc, st.layout, st.sel, eng);
		const NkPaintRect ra = st.layout.At(a), rb = st.layout.At(b);
		const float32 x0 = ra.x < rb.x ? ra.x : rb.x;
		const float32 y0 = ra.y < rb.y ? ra.y : rb.y;
		const float32 x1 = (ra.x + ra.w) > (rb.x + rb.w) ? (ra.x + ra.w) : (rb.x + rb.w);
		const float32 y1 = (ra.y + ra.h) > (rb.y + rb.h) ? (ra.y + ra.h) : (rb.y + rb.h);
		char d[128];
		snprintf(d, sizeof(d), "englobant (%.0f,%.0f %.0fx%.0f), attendu (%.0f,%.0f %.0fx%.0f)",
				 eng.x, eng.y, eng.w, eng.h, x0, y0, x1 - x0, y1 - y0);
		verdict("5. l'englobant de la multi-selection couvre EXACTEMENT les deux",
				ok && eng.x == x0 && eng.y == y0 && eng.w == x1 - x0 && eng.h == y1 - y0, d);
	}
	// ── 5b. L'ENGLOBANT IGNORE LA RACINE (elle couvre tout : elle noierait tout)
	scene();
	{
		st.sel.Set(0);
		st.sel.Add(b);
		NkPaintRect eng = {0.f, 0.f, 0.f, 0.f};
		const bool ok = NkRectSelection(st.doc, st.layout, st.sel, eng);
		const NkPaintRect rb = st.layout.At(b);
		char d[112];
		snprintf(d, sizeof(d), "englobant (%.0f,%.0f %.0fx%.0f), B (%.0f,%.0f %.0fx%.0f)",
				 eng.x, eng.y, eng.w, eng.h, rb.x, rb.y, rb.w, rb.h);
		verdict("5b. l'englobant IGNORE la racine : sinon elle couvrirait toute la page et "
				"la boite ne dirait plus rien",
				ok && eng.w == rb.w && eng.h == rb.h, d);
	}
	// ── 6. LES VALEURS MIXTES : commune quand elle l'est, « — » sinon ───────
	scene();
	{
		st.sel.Set(a);
		st.sel.Add(b);
		float32 v = -1.f;
		// largeurs differentes -> MIXTE
		const bool mixte = !NkValeurCommune(st.doc, st.sel,
											[](const NkUINode &n) { return n.width.value; }, v);
		// on les egalise -> COMMUNE
		st.doc.nodes[(uint32)b].width.value = st.doc.nodes[(uint32)a].width.value;
		float32 w = -1.f;
		const bool commune = NkValeurCommune(st.doc, st.sel,
											 [](const NkUINode &n) { return n.width.value; }, w);
		char d[112];
		snprintf(d, sizeof(d), "largeurs differentes -> %s ; egalisees -> %s (%.0f)",
				 mixte ? "MIXTE" : "commune", commune ? "commune" : "mixte", w);
		verdict("6. valeur commune / valeur MIXTE : l'Inspecteur saura quand ecrire un tiret",
				mixte && commune && w == 100.f, d);
	}
	// ── 7. LE RECTANGLE DE SELECTION PREND CE QU'IL TOUCHE (Lunacy) ─────────
	scene();
	{
		// une zone qui EFFLEURE A par son coin bas-droit, sans le contenir
		const NkPaintRect ra = st.layout.At(a);
		const NkPaintRect zone = {ra.x + ra.w - 5.f, ra.y + ra.h - 5.f, 60.f, 60.f};
		st.sel.Clear();
		NkPickInRect(st.doc, st.layout, zone, st.sel);
		const bool prisA = st.sel.Contains(a);
		// et une zone franchement a cote ne prend rien
		NkSelection loin;
		NkPickInRect(st.doc, st.layout, {5000.f, 5000.f, 10.f, 10.f}, loin);
		char d[112];
		snprintf(d, sizeof(d), "effleure A -> %s ; loin de tout -> %u element(s)",
				 prisA ? "PRIS" : "rate", loin.Count());
		verdict("7. le rectangle prend ce qu'il TOUCHE (pas ce qu'il contient), et rien "
				"quand il est loin",
				prisA && loin.Count() == 0, d);
	}
	// ── 8. VOLET CONSERVATION : selectionner n'ECRIT RIEN dans le document ──
	scene();
	{
		NkString avant;
		st.doc.Save(avant);
		st.sel.Set(a);
		st.sel.Add(b);
		st.sel.Toggle(c);
		st.sel.Toggle(c);
		NkAppliquerGeste(st.doc, st.sel, st.selected, profond, NkGesteSel::Profond);
		NkPaintRect eng;
		(void)NkRectSelection(st.doc, st.layout, st.sel, eng);
		float32 v = 0.f;
		(void)NkValeurCommune(st.doc, st.sel, [](const NkUINode &n) { return n.posX; }, v);
		NkString apres;
		st.doc.Save(apres);
		const bool dit = ancree(avant) && ancree(apres);
		char d[112];
		snprintf(d, sizeof(d), "document %s%s",
				 (avant.Compare(apres) == 0) ? "INCHANGE" : "MODIFIE",
				 dit ? "" : ", SERIALISATION MUETTE");
		verdict("8. CONSERVATION : selectionner, basculer, mesurer l'englobant et lire les "
				"valeurs n'ecrit RIEN dans le document",
				dit && avant.Compare(apres) == 0, d);
	}
	printf("\nRECETTE SELECTION : %d/%d %s\n", cas - echecs, cas,
		   echecs == 0 ? "PROUVEE" : "EN ECHEC");
	return echecs == 0 ? 0 : 1;
}
