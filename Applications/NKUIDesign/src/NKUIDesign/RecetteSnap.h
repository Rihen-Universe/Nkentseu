#pragma once
// -----------------------------------------------------------------------------
// @File    RecetteSnap.h
// @Brief   La recette « Snap » — sortie de `main.cpp` le 28/09/2026.
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

static nkentseu::int32 RecetteSnap() {
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
	static DesignState st;
	const NkPaintRect surface = {0.f, 0.f, 1200.f, 800.f};
	int32 cadre = -1;
	auto poser = [&](const char *nom, float32 x, float32 y, float32 w, float32 h) -> int32 {
		const int32 i = st.doc.AddChild(cadre, "", NkAuthor::Humain);
		NkUINode &n = st.doc.nodes[(uint32)i];
		n.label = NkString(nom);
		n.shape = NkString("rect");
		n.posX = x;
		n.posY = y;
		n.width.mode = NkSizeMode::Fixed;
		n.width.value = w;
		n.height.mode = NkSizeMode::Fixed;
		n.height.value = h;
		return i;
	};
	// La scene : une page 0..600 x 0..400, un voisin fixe, un mobile.
	auto scene = [&](int32 &voisin, int32 &mobile) {
		st.doc.NewDocument("recette snap", NkAuthor::Humain);
		cadre = st.doc.AddChild(0, "", NkAuthor::Humain);
		{
			NkUINode &c = st.doc.nodes[(uint32)cadre];
			c.label = NkString("Page");
			c.shape = NkString("frame");
			c.layout.kind = NkLayoutKind::Free;
			c.posX = 0.f;
			c.posY = 0.f;
			c.width.mode = NkSizeMode::Fixed;
			c.width.value = 600.f;
			c.height.mode = NkSizeMode::Fixed;
			c.height.value = 400.f;
		}
		voisin = poser("Voisin", 100.f, 100.f, 80.f, 60.f);
		mobile = poser("Mobile", 300.f, 250.f, 40.f, 30.f);
		st.Recompute(surface);
	};
	// Le rectangle du mobile, DEPLACE de (dx,dy) : exactement ce que le geste
	// donne au calcul.
	auto rectDe = [&](int32 n, float32 dx, float32 dy) {
		NkPaintRect r = st.layout.At(n);
		r.x += dx;
		r.y += dy;
		return r;
	};
	const float32 tol6 = 6.f; // 6 px ecran au zoom 1

	int32 voisin = -1, mobile = -1;
	// ── 1. BORD GAUCHE contre BORD GAUCHE, a 3 px : ca colle EXACTEMENT
	scene(voisin, mobile);
	{
		const NkPaintRect rv = st.layout.At(voisin);
		const NkPaintRect rm = st.layout.At(mobile);
		// on amene le mobile a 3 px a droite du bord gauche du voisin
		const float32 dx = (rv.x + 3.f) - rm.x;
		const NkSnapResultat s = NkCalculerSnap(st.doc, st.layout, mobile, rectDe(mobile, dx, 0.f), tol6);
		const bool ok = s.guideV.actif && s.dx == -3.f && s.guideV.coord == rv.x
						&& s.guideV.voisin == voisin;
		char d[128];
		nkentseu::NkSnprintf(d, sizeof(d), "dx=%.1f (attendu -3), guide x=%.1f (bord voisin %.1f), voisin=%d",
				 s.dx, s.guideV.coord, rv.x, s.guideV.voisin);
		verdict("bord a 3 px du bord voisin : snap EXACT, guide sur le bord", ok, d);
	}
	// ── 2. HORS TOLERANCE : rien ne bouge, et AUCUN guide (le controle negatif
	//    sans lequel le cas 1 ne prouve rien : un aimant qui colle toujours
	//    passerait le cas 1 les yeux fermes)
	// ⚠️ 33 px, ET LE CHIFFRE EST CHOISI. La premiere version disait 20 px et
	//    ECHOUAIT -- non par un defaut du calcul, mais parce qu a 20 px de son
	//    bord gauche le mobile avait son CENTRE exactement sur le centre du
	//    voisin : un vrai alignement, correctement detecte. La lecon vaut d etre
	//    ecrite : dans un banc d aimantation, une scene mal choisie fabrique des
	//    alignements par accident, et on accuse le code. A 33 px, le candidat le
	//    plus proche tombe a 7 px -- JUSTE au-dela des 6 de tolerance, ce qui
	//    fait de ce cas une borne et plus seulement un contre-exemple.
	{
		const NkPaintRect rv = st.layout.At(voisin);
		const NkPaintRect rm = st.layout.At(mobile);
		const float32 dx = (rv.x + 33.f) - rm.x;
		const NkSnapResultat s = NkCalculerSnap(st.doc, st.layout, mobile, rectDe(mobile, dx, 0.f), tol6);
		char d[96];
		nkentseu::NkSnprintf(d, sizeof(d), "dx=%.1f, guideV=%s", s.dx, s.guideV.actif ? "oui" : "non");
		verdict("a 33 px (candidat le plus proche a 7) : RIEN ne colle, aucun guide",
				s.dx == 0.f && !s.guideV.actif, d);
	}
	// ── 3. AIMANT ETEINT : la tolerance tombe a zero, plus rien ne colle
	{
		const NkPaintRect rv = st.layout.At(voisin);
		const NkPaintRect rm = st.layout.At(mobile);
		const float32 dx = (rv.x + 3.f) - rm.x;
		const NkSnapResultat s = NkCalculerSnap(st.doc, st.layout, mobile, rectDe(mobile, dx, 0.f), 0.f);
		char d[96];
		nkentseu::NkSnprintf(d, sizeof(d), "dx=%.1f, aimante=%s", s.dx, s.Aimante() ? "oui" : "non");
		verdict("aimant eteint (tolerance nulle) : le meme geste ne colle plus",
				s.dx == 0.f && !s.Aimante(), d);
	}
	// ── 4. LA TOLERANCE SE DIVISE PAR LE ZOOM. 6 px ecran au zoom 4 = 1,5 unite
	//    document : un ecart de 3 unites, qui collait au zoom 1, ne colle plus.
	{
		const NkPaintRect rv = st.layout.At(voisin);
		const NkPaintRect rm = st.layout.At(mobile);
		const float32 dx = (rv.x + 3.f) - rm.x;
		st.view.zoom = 4.f;
		const float32 tolZoom4 = st.view.ToDocLength(DesignState::kSnapTolEcran);
		const NkSnapResultat s =
			NkCalculerSnap(st.doc, st.layout, mobile, rectDe(mobile, dx, 0.f), tolZoom4);
		st.view.zoom = 1.f;
		char d[112];
		nkentseu::NkSnprintf(d, sizeof(d), "tolerance zoom 4 = %.2f unites, dx=%.1f", tolZoom4, s.dx);
		verdict("la tolerance suit le ZOOM : 3 unites ne collent plus au zoom 4",
				tolZoom4 == 1.5f && s.dx == 0.f, d);
	}
	// ── 5. CENTRE contre CENTRE
	scene(voisin, mobile);
	{
		const NkPaintRect rv = st.layout.At(voisin);
		const NkPaintRect rm = st.layout.At(mobile);
		const float32 cv = rv.y + rv.h * 0.5f;
		const float32 cm = rm.y + rm.h * 0.5f;
		const float32 dy = (cv + 2.f) - cm; // centre du mobile a 2 px sous celui du voisin
		const NkSnapResultat s = NkCalculerSnap(st.doc, st.layout, mobile, rectDe(mobile, 0.f, dy), tol6);
		char d[112];
		nkentseu::NkSnprintf(d, sizeof(d), "dy=%.1f (attendu -2), guide y=%.1f (centre voisin %.1f)", s.dy,
				 s.guideH.coord, cv);
		verdict("centre contre centre : le guide se pose sur le CENTRE du voisin",
				s.guideH.actif && s.dy == -2.f && s.guideH.coord == cv, d);
	}
	// ── 6. LA PAGE aimante aussi (bord gauche du cadre)
	scene(voisin, mobile);
	{
		const NkPaintRect rp = st.layout.At(cadre);
		const NkPaintRect rm = st.layout.At(mobile);
		const float32 dx = (rp.x + 4.f) - rm.x;
		const NkSnapResultat s = NkCalculerSnap(st.doc, st.layout, mobile, rectDe(mobile, dx, 0.f), tol6);
		char d[112];
		nkentseu::NkSnprintf(d, sizeof(d), "dx=%.1f (attendu -4), voisin=%d (-1 = la page)", s.dx,
				 s.guideV.voisin);
		verdict("la PAGE aimante comme un voisin, et le guide le dit (voisin = -1)",
				s.guideV.actif && s.dx == -4.f && s.guideV.voisin == -1, d);
	}
	// ── 7. L'ESPACEMENT EGAL, avec son BADGE de distance
	{
		st.doc.NewDocument("recette snap", NkAuthor::Humain);
		cadre = st.doc.AddChild(0, "", NkAuthor::Humain);
		{
			NkUINode &c = st.doc.nodes[(uint32)cadre];
			c.label = NkString("Page");
			c.shape = NkString("frame");
			c.layout.kind = NkLayoutKind::Free;
			c.width.mode = NkSizeMode::Fixed;
			c.width.value = 600.f;
			c.height.mode = NkSizeMode::Fixed;
			c.height.value = 400.f;
		}
		// A finit a 100, B commence a 300 : 200 px libres, le mobile fait 40 ->
		// 160 de libre, donc 80 de chaque cote. La position egale est x = 180.
		poser("A", 40.f, 50.f, 60.f, 30.f);
		poser("B", 300.f, 50.f, 60.f, 30.f);
		const int32 m = poser("Mobile", 176.f, 50.f, 40.f, 30.f); // a 4 px de la place egale
		st.Recompute(surface);
		const NkSnapResultat s = NkCalculerSnap(st.doc, st.layout, m, st.layout.At(m), tol6);
		const NkPaintRect rp = st.layout.At(cadre);
		char d[144];
		nkentseu::NkSnprintf(d, sizeof(d), "dx=%.1f (attendu 4), badge=%s, ecart=%.1f (attendu 80)", s.dx,
				 s.guideV.badge ? "oui" : "non", s.guideV.ecart);
		verdict("espacement EGAL : la place du milieu aimante, et l'ecart se DIT",
				s.guideV.actif && s.dx == 4.f && s.guideV.badge && s.guideV.ecart == 80.f
					&& s.guideV.coord == rp.x + 180.f,
				d);
	}
	// ── 8. ON NE S'AIMANTE PAS SUR SOI-MEME NI SUR SON DESCENDANT
	//    (un descendant BOUGE AVEC le noeud : il collerait toujours, et
	//    n'alignerait rien — le snap qui ne sert a rien mais qui bloque tout)
	{
		st.doc.NewDocument("recette snap", NkAuthor::Humain);
		cadre = st.doc.AddChild(0, "", NkAuthor::Humain);
		{
			NkUINode &c = st.doc.nodes[(uint32)cadre];
			c.label = NkString("Page");
			c.shape = NkString("frame");
			c.layout.kind = NkLayoutKind::Free;
			c.width.mode = NkSizeMode::Fixed;
			c.width.value = 600.f;
			c.height.mode = NkSizeMode::Fixed;
			c.height.value = 400.f;
		}
		// ⚠️ (233, 155) N EST PAS UN CHIFFRE AU HASARD. A (200, 200) ce cas
		//    ECHOUAIT : le bord haut du mobile tombait pile sur le CENTRE
		//    vertical de la page (400 / 2). Le calcul avait raison, la scene
		//    avait tort. Ici le mobile est loin des trois reperes de chaque axe
		//    de la page -- donc le SEUL candidat a portee est son propre enfant,
		//    a 2 px. S il collait, ce cas le dirait.
		const int32 m = poser("Mobile", 233.f, 155.f, 40.f, 30.f);
		st.doc.nodes[(uint32)m].layout.kind = NkLayoutKind::Free;
		const int32 enf = st.doc.AddChild(m, "", NkAuthor::Humain);
		{
			NkUINode &e = st.doc.nodes[(uint32)enf];
			e.label = NkString("Enfant");
			e.shape = NkString("rect");
			e.posX = 2.f;
			e.posY = 2.f;
			e.width.mode = NkSizeMode::Fixed;
			e.width.value = 10.f;
			e.height.mode = NkSizeMode::Fixed;
			e.height.value = 10.f;
		}
		st.Recompute(surface);
		// seul le mobile et son enfant existent : aucun voisin, et la page est
		// loin (bords a 0 et 600). Rien ne doit coller.
		const NkSnapResultat s = NkCalculerSnap(st.doc, st.layout, m, st.layout.At(m), tol6);
		char d[96];
		nkentseu::NkSnprintf(d, sizeof(d), "dx=%.1f dy=%.1f, aimante=%s", s.dx, s.dy,
				 s.Aimante() ? "oui" : "non");
		verdict("ni soi-meme ni son enfant : au milieu de la page, rien ne colle",
				!s.Aimante(), d);
	}
	// ── 9. L'ESPACEMENT REPETE : deux blocs a N, le troisieme s'aimante a N ──
	//     Retour de Rodolf, 01/09 : « pas de snap proportionnel, par exemple
	//     snapper par rapport a l'espace entre deux blocs deja poses ».
	//
	//     ⚠️ CE CAS EST CONSTRUIT POUR QU'AUCUN AUTRE CANDIDAT NE PUISSE LE
	//     REUSSIR A SA PLACE, et c'est la lecon des scenes de snap de Q42
	//     appliquee d'entree. La position visee (300) n'est ni un bord, ni un
	//     centre, ni le milieu entre A et B, ni un repere de la page : si
	//     l'espacement repete n'existait pas, RIEN ne collerait. Sans cette
	//     precaution, le cas serait passe au vert sur un alignement de bord
	//     fabrique par accident, et n'aurait rien prouve du tout.
	{
		st.doc.NewDocument("recette snap", NkAuthor::Humain);
		cadre = st.doc.AddChild(0, "", NkAuthor::Humain);
		{
			NkUINode &c = st.doc.nodes[(uint32)cadre];
			c.label = NkString("Page");
			c.shape = NkString("frame");
			c.layout.kind = NkLayoutKind::Free;
			c.width.mode = NkSizeMode::Fixed;
			c.width.value = 900.f;
			c.height.mode = NkSizeMode::Fixed;
			c.height.value = 400.f;
		}
		// A : 60..120 ; B : 180..240 -> l'ecart MODELE vaut 60.
		// Le troisieme doit donc se poser a 240 + 60 = 300.
		poser("A", 60.f, 47.f, 60.f, 30.f);
		poser("B", 180.f, 47.f, 60.f, 30.f);
		const int32 m = poser("Mobile", 304.f, 47.f, 60.f, 30.f); // a 4 px de la place
		st.doc.nodes[(uint32)m].posY = 143.f; // hors de la bande de A et B : aucun
											  // alignement vertical ne peut aider
		st.Recompute(surface);
		const NkPaintRect rp = st.layout.At(cadre);
		const NkSnapResultat s = NkCalculerSnap(st.doc, st.layout, m, st.layout.At(m), tol6);
		const bool colle = s.guideV.actif && s.dx == -4.f && s.guideV.coord == rp.x + 300.f;
		const bool dit = s.guideV.badge && s.guideV.ecart == 60.f;
		// LES DEUX DISTANCES SONT ECRITES : celle du modele ET celle de la copie.
		bool deuxMesures = s.nbMesures >= 2u;
		if (deuxMesures)
			for (uint32 i = 0; i < 2u; ++i)
				if (s.mesures[i].valeur != 60.f)
					deuxMesures = false;
		char d[176];
		nkentseu::NkSnprintf(d, sizeof(d), "dx=%.1f (attendu -4), ecart=%.0f (attendu 60), %u mesure(s) "
							   "ecrite(s) [%.0f, %.0f]",
				 s.dx, s.guideV.ecart, s.nbMesures, s.nbMesures > 0 ? s.mesures[0].valeur : -1.f,
				 s.nbMesures > 1 ? s.mesures[1].valeur : -1.f);
		verdict("9. espacement REPETE : deux blocs espaces de 60, le troisieme s'aimante a 60 "
				"-- et les DEUX distances sont ecrites (le modele ET la copie)",
				colle && dit && deuxMesures, d);
	}
	// ── 10. L'ESPACEMENT REPETE MARCHE DANS LES DEUX SENS ────────────────────
	//     ⚠️ LE CHEMIN FRERE, TENU PAR UN BANC. N'ecrire que « apres B » aurait
	//     donne un aimant qui marche quand on construit vers la droite et pas
	//     vers la gauche -- une asymetrie que personne ne devine et que tout le
	//     monde prend pour une panne. Le cas pose le mobile AVANT A.
	{
		st.doc.NewDocument("recette snap", NkAuthor::Humain);
		cadre = st.doc.AddChild(0, "", NkAuthor::Humain);
		{
			NkUINode &c = st.doc.nodes[(uint32)cadre];
			c.label = NkString("Page");
			c.shape = NkString("frame");
			c.layout.kind = NkLayoutKind::Free;
			c.width.mode = NkSizeMode::Fixed;
			c.width.value = 900.f;
			c.height.mode = NkSizeMode::Fixed;
			c.height.value = 400.f;
		}
		// A : 400..460 ; B : 520..580 -> ecart 60. AVANT A : le bord DROIT du
		// mobile doit se poser a 400 - 60 = 340, donc son bord gauche a 280.
		poser("A", 400.f, 47.f, 60.f, 30.f);
		poser("B", 520.f, 47.f, 60.f, 30.f);
		const int32 m = poser("Mobile", 277.f, 143.f, 60.f, 30.f); // a 3 px
		st.Recompute(surface);
		const NkPaintRect rp = st.layout.At(cadre);
		const NkSnapResultat s = NkCalculerSnap(st.doc, st.layout, m, st.layout.At(m), tol6);
		char d[144];
		nkentseu::NkSnprintf(d, sizeof(d), "dx=%.1f (attendu 3), guide x=%.0f (attendu %.0f), ecart=%.0f",
				 s.dx, s.guideV.coord, rp.x + 340.f, s.guideV.ecart);
		verdict("10. l'espacement repete marche AUSSI vers la gauche : on construit une serie "
				"dans les deux sens",
				s.guideV.actif && s.dx == 3.f && s.guideV.coord == rp.x + 340.f
					&& s.guideV.ecart == 60.f,
				d);
	}
	// ── 11. DEUX BLOCS COLLES NE SONT PAS UN MOTIF ───────────────────────────
	//     ⚠️ UN ECART NUL N'EST PAS UNE REGULARITE A REPRODUIRE. En faire une
	//     collerait le troisieme bloc aux deux autres a la moindre approche, et
	//     l'aimant deviendrait de la glu -- le genre de comportement pour lequel
	//     on finit par eteindre l'aimantation, donc par perdre tout le reste.
	{
		st.doc.NewDocument("recette snap", NkAuthor::Humain);
		cadre = st.doc.AddChild(0, "", NkAuthor::Humain);
		{
			NkUINode &c = st.doc.nodes[(uint32)cadre];
			c.label = NkString("Page");
			c.shape = NkString("frame");
			c.layout.kind = NkLayoutKind::Free;
			c.width.mode = NkSizeMode::Fixed;
			c.width.value = 900.f;
			c.height.mode = NkSizeMode::Fixed;
			c.height.value = 400.f;
		}
		// A et B se TOUCHENT (60..120 et 120..180) : l'ecart modele est 0.
		poser("A", 60.f, 47.f, 60.f, 30.f);
		poser("B", 120.f, 47.f, 60.f, 30.f);
		// le mobile est a 3 px du bord droit de B : si l'ecart 0 etait un motif,
		// il collerait a 180 en se disant « repete ».
		const int32 m = poser("Mobile", 183.f, 143.f, 60.f, 30.f);
		st.Recompute(surface);
		const NkSnapResultat s = NkCalculerSnap(st.doc, st.layout, m, st.layout.At(m), tol6);
		// il PEUT coller (bord contre bord, c'est legitime) mais JAMAIS avec un
		// badge d'espacement repete a 0 : c'est ca qu'on interdit.
		const bool pasDeMotifNul = !(s.guideV.badge && s.guideV.ecart == 0.f);
		bool aucuneMesureNulle = true;
		for (uint32 i = 0; i < s.nbMesures; ++i)
			if (s.mesures[i].valeur == 0.f)
				aucuneMesureNulle = false;
		char d[144];
		nkentseu::NkSnprintf(d, sizeof(d), "dx=%.1f, badge=%s, ecart=%.1f, %u mesure(s)", s.dx,
				 s.guideV.badge ? "oui" : "non", s.guideV.ecart, s.nbMesures);
		verdict("11. deux blocs COLLES ne fabriquent pas un motif d'espacement 0 (sinon "
				"l'aimant devient de la glu)",
				pasDeMotifNul && aucuneMesureNulle, d);
	}
	printf("\nRECETTE SNAP : %d/%d %s\n", cas - echecs, cas,
		   echecs == 0 ? "PROUVEE" : "EN ECHEC");
	return echecs == 0 ? 0 : 1;
}
