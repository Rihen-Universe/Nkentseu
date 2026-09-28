#pragma once
// -----------------------------------------------------------------------------
// @File    CasOngletsVerticaux.h
// @Brief   (b19) Les onglets savent-ils se mettre en COLONNE, et le défaut
//          reste-t-il la ligne ?
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  CE QUE ÇA MESURE
// =============================================================================
//  Rodolf, 28/09 : « on a déjà des onglets horizontaux, on doit aussi avoir des
//  onglets verticaux ».
//
//  🔴 ET LE CRITÈRE EST GÉOMÉTRIQUE, PAS UN COMPTEUR. « La barre s'est montée »
//     serait vert dans les deux orientations : ce qui distingue une colonne
//     d'une ligne, ce sont les RECTANGLES. On lit donc le relevé.
//
//   (v1) NÉGATIF D'ABORD : sans la clé, les onglets sont sur UNE LIGNE — même
//        `y`, `x` croissant. Un document écrit avant aujourd'hui ne bouge pas.
//   (v2) avec `orientation = "verticale"` : même `x`, `y` croissant.
//   (v3) en colonne, tous les onglets ont la MÊME LARGEUR — c'est ce qui fait
//        une colonne droite. Des largeurs qui suivent le libellé donneraient un
//        escalier, et c'est ce qu'on obtiendrait en oubliant le `-1.f`.
//   (v4) le COMPORTEMENT ne change pas : les deux orientations montent le même
//        nombre d'onglets. Une colonne qui en perdrait un passerait (v2) et (v3).
// -----------------------------------------------------------------------------

static void CasOngletsVerticaux() {
	// =====================================================================
	printf("\n-- (b19) les onglets en COLONNE, et le defaut reste la LIGNE\n");
	// =====================================================================

	// ⚠️ LES DEUX DOCUMENTS NE DIFFÈRENT QUE PAR LA CLÉ. Des libellés ou un
	//    conteneur différents auraient mêlé deux causes au même écart.
	static const char kHoriz[] = "nkgui 0.4\n"
								 "widgets {\n"
								 "  TabBar \"onglets\" { tabs = [\"Un\", \"Deuxieme\", \"Trois\"] }\n"
								 "}\n";
	static const char kVert[] =
		"nkgui 0.4\n"
		"widgets {\n"
		"  TabBar \"onglets\" { tabs = [\"Un\", \"Deuxieme\", \"Trois\"], orientation = \"verticale\" }\n"
		"}\n";

	struct Releve {
			uint32 n = 0u;
			NkRect r[8];
	};
	auto lire = [](Scene &s) {
		Releve v;
		int32 n = 0;
		const NkGuiNote *notes = NkGuiIntrospectNotes(s.ctx, n);
		for (int32 i = 0; i < n && v.n < 8u; ++i)
			if (notes[i].nature == NkGuiNature::Onglet)
				v.r[v.n++] = notes[i].rect;
		return v;
	};

	Releve h, v;
	{
		Scene s;
		Check(s.Charger(kHoriz, (uint32)(sizeof(kHoriz) - 1u), 400, 300),
			  "(v1) le document SANS la cle se charge");
		NkGuiIntrospectActiver(s.ctx, true);
		s.Image();
		h = lire(s);
		s.exe.Debrancher(s.ctx);
	}
	{
		Scene s;
		Check(s.Charger(kVert, (uint32)(sizeof(kVert) - 1u), 400, 300),
			  "(v2) le document AVEC `orientation = verticale` se charge");
		NkGuiIntrospectActiver(s.ctx, true);
		s.Image();
		v = lire(s);
		s.exe.Debrancher(s.ctx);
	}

	printf("        horizontal : %u onglet(s)", h.n);
	for (uint32 i = 0; i < h.n; ++i)
		printf("  (%.0f,%.0f %.0fx%.0f)", (double)h.r[i].x, (double)h.r[i].y, (double)h.r[i].w,
			   (double)h.r[i].h);
	printf("\n        vertical   : %u onglet(s)", v.n);
	for (uint32 i = 0; i < v.n; ++i)
		printf("  (%.0f,%.0f %.0fx%.0f)", (double)v.r[i].x, (double)v.r[i].y, (double)v.r[i].w,
			   (double)v.r[i].h);
	printf("\n");

	// ── (v1) LE NEGATIF : une LIGNE ────────────────────────────────────
	CheckEqU(h.n, 3u, "(v1) trois onglets sans la cle");
	if (h.n >= 2u) {
		Check(h.r[1].x > h.r[0].x + 1.f && h.r[1].y <= h.r[0].y + 0.5f,
			  "(v1) SANS LA CLE ILS SONT SUR UNE LIGNE — x croit, y ne bouge pas");
	}

	// ── (v2) LA COLONNE ────────────────────────────────────────────────
	CheckEqU(v.n, 3u, "(v4) trois onglets AVEC la cle — la colonne n'en perd aucun");
	if (v.n >= 2u) {
		Check(v.r[1].y > v.r[0].y + 1.f && v.r[1].x <= v.r[0].x + 0.5f,
			  "(v2) AVEC LA CLE ILS SONT EN COLONNE — y croit, x ne bouge pas");
	}

	// ── (v3) LA COLONNE EST DROITE ─────────────────────────────────────
	if (v.n >= 3u) {
		const float32 d01 = v.r[1].w - v.r[0].w, d12 = v.r[2].w - v.r[1].w;
		const float32 a01 = d01 < 0.f ? -d01 : d01, a12 = d12 < 0.f ? -d12 : d12;
		Check(a01 <= 0.5f && a12 <= 0.5f,
			  "(v3) EN COLONNE TOUS ONT LA MEME LARGEUR — sinon c'est un escalier");
	}
	// Et la contre-epreuve : en LIGNE, les largeurs DIFFERENT (elles suivent le
	// libelle). Sans ce critere, une version qui mettrait tout le monde a la
	// meme largeur dans les deux sens passerait (v3) sans rien prouver.
	if (h.n >= 2u) {
		const float32 d = h.r[1].w - h.r[0].w;
		Check((d < -0.5f || d > 0.5f),
			  "(v3) EN LIGNE elles different — la largeur suit le libelle");
	}
}

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
