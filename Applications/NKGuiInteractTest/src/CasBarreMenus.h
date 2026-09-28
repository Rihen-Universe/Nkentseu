#pragma once
// -----------------------------------------------------------------------------
// @File    CasBarreMenus.h
// @Brief   (b18) La barre de menus qui CONNAIT SA LIMITE, et le puits « … ».
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  CE QUE RODOLF A DEMANDE, ET CE QU'IL A DEMANDE ENSUITE
// =============================================================================
//  28/09, premiere demande : « [un bouton] qui permet de montrer les autres
//  menus et eviter un chevauchement avec le titre du document [...] ca ne doit
//  jamais se chevaucher [...] si le menu est touffu a partir d'un niveau ou
//  nombre de menus on devrait avoir un bouton "..." ».
//
//  28/09, seconde demande, et c'est elle qui a change la forme du correctif :
//  « la barre de menu qui connait sa limite c'est .nkgui ou en c++ ? si c'est en
//  c++ comment les utilisateurs peuvent reproduire ce comportement sur leurs
//  applications ? pense toujours aux utilisateurs autres que nous. »
//
//  D'ou les criteres (m3) et (m4) ci-dessous : ils ne mesurent PAS le C++, ils
//  mesurent ce qu'un DOCUMENT obtient en declarant deux cles. Si la regle
//  n'etait accessible qu'en C++, ces deux-la seraient rouges.
//
// =============================================================================
//  L'ORDRE DES CRITERES, ET POURQUOI LE NEGATIF EST LE PREMIER
// =============================================================================
//   (m1) LE NEGATIF : sans politique declaree, RIEN ne change. Un document ecrit
//        avant aujourd'hui doit monter a l'identique. Sans ce critere d'abord,
//        les cinq suivants pourraient etre verts sur une barre qui verse tout
//        dans le puits en permanence.
//   (m2) LA LARGEUR : aucun titre n'ecrit au-dela de la limite cedee. C'est le
//        chevauchement, et il se mesure en LISANT LES RECTANGLES du releve --
//        pas en comptant des pixels, qui ne diraient pas OU.
//   (m3) LA RESERVE DECLAREE PAR LE DOCUMENT (`reserveDroite`).
//   (m4) LE PLAFOND DE NOMBRE DECLARE PAR LE DOCUMENT (`maxMenus`), sur une
//        bande LARGE ou tout tiendrait. 🔴 C'est le critere qui empeche les deux
//        regles d'etre la meme : sur une bande etroite, un plafond de nombre
//        serait vert grace a la largeur, et on ne saurait rien.
//   (m5) RIEN NE DISPARAIT : les menus verses restent atteignables par le « … ».
//        Un correctif qui MASQUE ce qui deborde passerait (m2), (m3) et (m4).
//   (m6) LE PLANCHER : une reserve absurde ne rend pas les menus inatteignables.
//
// ⚠️ LE LIBELLE CHERCHE EST « … » EN UTF-8 (\xE2\x80\xA6), pas trois points. Le
//    releve porte ce que le widget a ECRIT ; chercher "..." rendrait nullptr sur
//    une barre parfaitement correcte.
// -----------------------------------------------------------------------------

// Dix menus, chacun avec une entree — assez pour deborder une bande etroite, et
// assez peu pour tenir dans une large. Le corps est le meme dans les six cas :
// ce qui change d'un cas a l'autre est la BANDE et les deux cles, jamais le
// contenu. Comparer deux documents differents n'aurait rien prouve.
#define NKCBM_DIX_MENUS                                                                            \
	"  Menu \"m1\" { label = \"Fichier\"\n    MenuItem \"i1\" { label = \"Nouveau\" }\n  }\n"       \
	"  Menu \"m2\" { label = \"Edition\"\n    MenuItem \"i2\" { label = \"Annuler\" }\n  }\n"       \
	"  Menu \"m3\" { label = \"Affichage\"\n    MenuItem \"i3\" { label = \"Zoom\" }\n  }\n"        \
	"  Menu \"m4\" { label = \"Objet\"\n    MenuItem \"i4\" { label = \"Grouper\" }\n  }\n"         \
	"  Menu \"m5\" { label = \"Cible\"\n    MenuItem \"i5\" { label = \"Classe\" }\n  }\n"          \
	"  Menu \"m6\" { label = \"Comportement\"\n    MenuItem \"i6\" { label = \"Simuler\" }\n  }\n"  \
	"  Menu \"m7\" { label = \"IA\"\n    MenuItem \"i7\" { label = \"Generer\" }\n  }\n"            \
	"  Menu \"m8\" { label = \"Fenetre\"\n    MenuItem \"i8\" { label = \"Disposition\" }\n  }\n"   \
	"  Menu \"m9\" { label = \"Aide\"\n    MenuItem \"i9\" { label = \"A propos\" }\n  }\n"         \
	"  Menu \"m10\" { label = \"Vues\"\n    MenuItem \"i10\" { label = \"Hierarchie\" }\n  }\n"

static const char *const kNkcbmPuits = "\xE2\x80\xA6"; // « … » tel que le widget l'ecrit

/// Le bord DROIT le plus a droite parmi les titres restes DANS la bande, et leur
/// nombre. « Dans la bande » = note de nature `Menu` au niveau -1 (la couche
/// principale) ; un menu verse dans le puits est note au niveau 0.
///
/// ⚠️ ON EXCLUT LE PUITS LUI-MEME DU BORD MESURE, et c'est motive : il a le droit
///    de se serrer contre la limite (c'est meme sa fonction). Le melanger aux
///    titres rendrait (m2) vert par construction.
static void NkcbmMesurerBande(const NkGuiContext &ctx, float32 &bordMax, uint32 &titres,
							  bool &puitsPresent, NkRect &rectPuits) {
	bordMax = 0.f;
	titres = 0u;
	puitsPresent = false;
	rectPuits = NkRect{0.f, 0.f, 0.f, 0.f};
	int32 n = 0;
	const NkGuiNote *notes = NkGuiIntrospectNotes(ctx, n);
	if (!notes)
		return;
	for (int32 i = 0; i < n; ++i) {
		if (notes[i].nature != NkGuiNature::Menu || notes[i].niveau != -1)
			continue;
		const bool estPuits = (notes[i].libelle[0] == '\xE2' && notes[i].libelle[1] == '\x80'
							   && notes[i].libelle[2] == '\xA6');
		if (estPuits) {
			puitsPresent = true;
			rectPuits = notes[i].rect;
			continue;
		}
		++titres;
		const float32 bord = notes[i].rect.x + notes[i].rect.w;
		if (bord > bordMax)
			bordMax = bord;
	}
}

/// Les menus verses DANS le puits : notes de nature `Menu` au niveau 0.
static uint32 NkcbmCompterDansLePuits(const NkGuiContext &ctx) {
	uint32 n0 = 0u;
	int32 n = 0;
	const NkGuiNote *notes = NkGuiIntrospectNotes(ctx, n);
	if (!notes)
		return 0u;
	for (int32 i = 0; i < n; ++i)
		if (notes[i].nature == NkGuiNature::Menu && notes[i].niveau == 0)
			++n0;
	return n0;
}

static void CasBarreMenus() {
	// =====================================================================
	printf("\n-- (b18) la barre de menus CONNAIT SA LIMITE, et ce qui deborde va sous « … »\n");
	// =====================================================================

	// ── (m1) LE NEGATIF : sans politique, rien ne bouge ─────────────────
	// 🔴 IL EST LE PREMIER PARCE QU'IL EST LE SEUL QUI PUISSE ACCUSER L'AJOUT.
	//    Les cinq autres prouvent que le puits s'ouvre ; celui-ci prouve qu'il
	//    ne s'ouvre pas quand personne ne l'a demande — c'est-a-dire que les
	//    documents deja ecrits montent comme avant.
	{
		static const char kDoc[] = "nkgui 0.4\n"
								   "widgets {\n"
								   "  MenuBar \"barre\" {\n" NKCBM_DIX_MENUS "  }\n"
								   "}\n";
		Scene s;
		Check(s.Charger(kDoc, (uint32)(sizeof(kDoc) - 1u), 1400, 120),
			  "(m1) le document a dix menus se charge");
		NkGuiIntrospectActiver(s.ctx, true);
		s.Image();
		float32 bord = 0.f;
		uint32 titres = 0u;
		bool puits = false;
		NkRect rp{};
		NkcbmMesurerBande(s.ctx, bord, titres, puits, rp);
		printf("        [large, sans politique] titres dans la bande = %u, bord droit = %.1f, "
			   "puits = %s, verses = %d\n",
			   titres, (double)bord, puits ? "OUI" : "non", s.ctx.menuBarDebordCompte);
		CheckEqU(titres, 10u, "(m1) LES DIX TITRES SONT DANS LA BANDE — rien n'a ete verse");
		Check(!puits, "(m1) ET AUCUN « … » N'EST APPARU — l'ajout est neutre par defaut");
		CheckEqU((uint32)s.ctx.menuBarDebordCompte, 0u, "(m1) le compteur de verses est a zero");
		s.exe.Debrancher(s.ctx);
	}

	// ── (m2) LA LARGEUR : aucun titre n'ecrit au-dela de la limite ──────
	{
		static const char kDoc[] = "nkgui 0.4\n"
								   "widgets {\n"
								   "  MenuBar \"barre\" {\n" NKCBM_DIX_MENUS "  }\n"
								   "}\n";
		Scene s;
		Check(s.Charger(kDoc, (uint32)(sizeof(kDoc) - 1u), 300, 120),
			  "(m2) le meme document, dans une bande ETROITE");
		NkGuiIntrospectActiver(s.ctx, true);
		s.Image();
		float32 bord = 0.f;
		uint32 titres = 0u;
		bool puits = false;
		NkRect rp{};
		NkcbmMesurerBande(s.ctx, bord, titres, puits, rp);
		const float32 limite = s.ctx.menuBarLimite;
		printf("        [etroit] titres = %u, bord droit = %.1f, limite = %.1f, puits en x = "
			   "%.1f (l = %.1f), verses = %d\n",
			   titres, (double)bord, (double)limite, (double)rp.x, (double)rp.w,
			   s.ctx.menuBarDebordCompte);
		Check(titres > 0u && titres < 10u,
			  "(m2) une PARTIE des titres tient — ni tout, ni rien");
		Check(puits, "(m2) ET LE « … » EST APPARU");
		// 🔴 LE CRITERE DE RODOLF, MOT POUR MOT : « ca ne doit jamais se
		//    chevaucher ». Il se lit sur les RECTANGLES du releve, parce qu'un
		//    compte de pixels dirait « il y a de l'encre » sans dire OU.
		Check(bord <= limite, "(m2) AUCUN TITRE N'ECRIT AU-DELA DE LA LIMITE CEDEE");
		Check(rp.x + rp.w <= s.ctx.menuBarRect.x + s.ctx.menuBarRect.w,
			  "(m2) et le « … » lui-meme reste DANS la bande");
		s.exe.Debrancher(s.ctx);
	}

	// ── (m3) LA RESERVE, DECLAREE PAR LE DOCUMENT ───────────────────────
	// 🔴 CE CRITERE REPOND A LA QUESTION DE RODOLF : pas une ligne de C++ n'est
	//    ecrite ici, et pourtant la bande se borne. Si la regle n'etait
	//    accessible qu'en C++, il serait rouge.
	{
		static const char kDoc[] = "nkgui 0.4\n"
								   "widgets {\n"
								   "  MenuBar \"barre\" {\n"
								   "    reserveDroite = 1100\n" NKCBM_DIX_MENUS "  }\n"
								   "}\n";
		Scene s;
		Check(s.Charger(kDoc, (uint32)(sizeof(kDoc) - 1u), 1400, 120),
			  "(m3) un document qui DECLARE `reserveDroite = 1100`");
		NkGuiIntrospectActiver(s.ctx, true);
		s.Image();
		float32 bord = 0.f;
		uint32 titres = 0u;
		bool puits = false;
		NkRect rp{};
		NkcbmMesurerBande(s.ctx, bord, titres, puits, rp);
		// La bande fait 1400 de large : 1100 reserves laissent 300, donc la
		// limite est a 300. L'attendu est calcule A LA MAIN, avant la mesure.
		//
		// 🔴 ET LA RESERVE A DU MONTER DE 700 A 1100, PARCE QUE MON PREMIER
		//    ATTENDU ETAIT FAUX : a 700, les dix titres TENAIENT encore (ils
		//    mesurent ~600 px avec cette police), donc aucun « … » n'avait de
		//    raison d'apparaitre -- le critere accusait un code correct. *Un
		//    attendu s'ecrit avec SA condition*, et la condition manquait ici :
		//    une reserve qui force VRAIMENT le depassement.
		const float32 attendu = 300.f;
		printf("        [reserve 1100 sur 1400] titres = %u, bord droit = %.1f, limite = %.1f\n",
			   titres, (double)bord, (double)s.ctx.menuBarLimite);
		CheckEqF(s.ctx.menuBarLimite, attendu, 0.5f,
				 "(m3) LA CLE DU DOCUMENT A ETE LUE — la limite est a 1400 - 1100");
		Check(bord <= attendu, "(m3) et aucun titre n'ecrit dans la reserve");
		Check(puits, "(m3) le « … » a pris le relais");
		s.exe.Debrancher(s.ctx);
	}

	// ── (m4) LE PLAFOND DE NOMBRE, SUR UNE BANDE OU TOUT TIENDRAIT ──────
	// 🔴 C'EST LE CRITERE QUI EMPECHE LES DEUX REGLES D'ETRE LA MEME. Rodolf a
	//    nomme DEUX declencheurs — « a partir d'un niveau OU nombre de menus ».
	//    Mesure sur une bande de 1400 : (m1) vient de prouver que les dix
	//    titres y tiennent. Si (m4) est vert ici, c'est le NOMBRE qui a
	//    tranche, et lui seul. Sur une bande etroite, ce critere aurait ete
	//    vert grace a la largeur et n'aurait rien dit.
	{
		static const char kDoc[] = "nkgui 0.4\n"
								   "widgets {\n"
								   "  MenuBar \"barre\" {\n"
								   "    maxMenus = 3\n" NKCBM_DIX_MENUS "  }\n"
								   "}\n";
		Scene s;
		Check(s.Charger(kDoc, (uint32)(sizeof(kDoc) - 1u), 1400, 120),
			  "(m4) un document qui DECLARE `maxMenus = 3`, dans une bande LARGE");
		NkGuiIntrospectActiver(s.ctx, true);
		s.Image();
		float32 bord = 0.f;
		uint32 titres = 0u;
		bool puits = false;
		NkRect rp{};
		NkcbmMesurerBande(s.ctx, bord, titres, puits, rp);
		printf("        [maxMenus 3, bande large] titres = %u, puits = %s, verses = %d\n", titres,
			   puits ? "OUI" : "non", s.ctx.menuBarDebordCompte);
		CheckEqU(titres, 3u, "(m4) EXACTEMENT TROIS TITRES — c'est le NOMBRE qui a tranche");
		Check(puits, "(m4) et le « … » porte les sept autres");
		CheckEqU((uint32)s.ctx.menuBarDebordCompte, 7u, "(m4) sept menus verses, aucun perdu");
		s.exe.Debrancher(s.ctx);
	}

	// ── (m5) RIEN NE DISPARAIT : le puits s'ouvre et les rend ───────────
	// 🔴 SANS CE CRITERE, UN CORRECTIF QUI MASQUE PASSERAIT TOUT LE RESTE.
	//    (m2), (m3) et (m4) sont tous verts sur une barre qui jetterait
	//    simplement les menus en trop. Ce que Rodolf a demande, c'est « montrer
	//    les AUTRES menus » — donc ils doivent etre atteignables.
	{
		static const char kDoc[] = "nkgui 0.4\n"
								   "widgets {\n"
								   "  MenuBar \"barre\" {\n"
								   "    maxMenus = 3\n" NKCBM_DIX_MENUS "  }\n"
								   "}\n";
		Scene s;
		Check(s.Charger(kDoc, (uint32)(sizeof(kDoc) - 1u), 1400, 120),
			  "(m5) le meme document a plafond trois");
		NkGuiIntrospectActiver(s.ctx, true);
		s.Image();
		float32 bord = 0.f;
		uint32 titres = 0u;
		bool puits = false;
		NkRect rp{};
		NkcbmMesurerBande(s.ctx, bord, titres, puits, rp);
		Check(puits, "(m5) le « … » est la, et on connait son rectangle");
		// Le clic se pose AU CENTRE du rectangle releve — pas a une coordonnee
		// devinee. Un banc qui viserait « a peu pres la » mesurerait sa propre
		// arithmetique.
		const float32 cx = rp.x + rp.w * 0.5f, cy = rp.y + rp.h * 0.5f;
		s.exe.PoserPointeur(s.ctx, cx, cy);
		s.ctx.input.mouseDown[0] = true;
		s.ctx.input.mouseClicked[0] = true;
		s.Image();
		s.ctx.input.mouseClicked[0] = false;
		s.ctx.input.mouseDown[0] = false;
		// ⚠️ DEUX IMAGES DE PLUS, ET C'EST LE CONTRAT DE NKGui, PAS UNE MARGE :
		//    un popup de menu se MESURE une image et s'applique a la suivante
		//    (`menuMeasure*` -> `MenuSizeSet`). Un banc a une seule image ne
		//    verrait jamais un menu deroule — l'en-tete de ce fichier le dit
		//    deja pour le survol, c'est le meme retard.
		s.Image();
		s.Image();
		const uint32 dansLePuits = NkcbmCompterDansLePuits(s.ctx);
		printf("        [puits ouvert] menus au niveau 0 = %u\n", dansLePuits);
		CheckEqU(dansLePuits, 7u,
				 "(m5) LES SEPT MENUS VERSES SONT ATTEIGNABLES — rien n'a ete masque");
		s.exe.Debrancher(s.ctx);
	}

	// ── (m6) LE PLANCHER : « serre » n'est pas « inutilisable » ─────────
	// Une reserve plus large que la bande elle-meme. Sans garde, la limite
	// passerait a gauche du debut de la bande et le « … » n'aurait plus ou se
	// poser : les dix menus deviendraient inatteignables — un defaut PIRE que
	// celui qu'on ferme.
	{
		static const char kDoc[] = "nkgui 0.4\n"
								   "widgets {\n"
								   "  MenuBar \"barre\" {\n"
								   "    reserveDroite = 5000\n" NKCBM_DIX_MENUS "  }\n"
								   "}\n";
		Scene s;
		Check(s.Charger(kDoc, (uint32)(sizeof(kDoc) - 1u), 600, 120),
			  "(m6) une reserve ABSURDE (5000 sur une bande de 600)");
		NkGuiIntrospectActiver(s.ctx, true);
		s.Image();
		float32 bord = 0.f;
		uint32 titres = 0u;
		bool puits = false;
		NkRect rp{};
		NkcbmMesurerBande(s.ctx, bord, titres, puits, rp);
		printf("        [reserve absurde] titres = %u, puits en x = %.1f (l = %.1f), bande x = "
			   "%.1f (l = %.1f)\n",
			   titres, (double)rp.x, (double)rp.w, (double)s.ctx.menuBarRect.x,
			   (double)s.ctx.menuBarRect.w);
		Check(puits, "(m6) LE « … » EXISTE QUAND MEME");
		// ⚠️ `puits &&` N'EST PAS UNE REDONDANCE : C'EST CE QUI REND LE CRITERE
		//    CAPABLE D'ECHOUER. Sans lui, un puits ABSENT laisse un rectangle nul
		//    {0,0,0,0} qui est trivialement « dans la bande » — le critere passait
		//    au vert exactement dans le cas ou il devait accuser, et c'est ce qui
		//    est arrive au premier passage de ce banc : le puits manquait, et
		//    seule la ligne d'au-dessus l'a dit.
		Check(puits && rp.w > 0.f && rp.x >= s.ctx.menuBarRect.x
				  && rp.x + rp.w <= s.ctx.menuBarRect.x + s.ctx.menuBarRect.w,
			  "(m6) et il est DANS la bande — les menus restent atteignables");
		s.exe.Debrancher(s.ctx);
	}
}

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
