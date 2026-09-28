#pragma once
// -----------------------------------------------------------------------------
// @File    RecetteTransfo.h
// @Brief   La recette « Transfo » — sortie de `main.cpp` le 28/09/2026.
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

static nkentseu::int32 RecetteTransfo() {
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
	auto proche = [](float32 a, float32 b, float32 tol) {
		const float32 d = a - b;
		return (d < 0.f ? -d : d) <= tol;
	};
	static DesignState st;
	const NkPaintRect surface = {0.f, 0.f, 1200.f, 800.f};

	// ── 1. L'IDENTITE NE TOUCHE A RIEN ─────────────────────────────────────
	//     ⚠️ CE CAS N'EST PAS DECORATIF : sans le court-circuit `Identite()`,
	//     chaque point de chaque forme repasserait par un aller-retour en
	//     flottant qui le deplacerait d'un ulp, et un document NON TOURNE
	//     cesserait de se dessiner au pixel pres. Le defaut serait invisible a
	//     l'oeil et visible dans toutes les captures comparees.
	{
		NkTransfo t;
		float32 x = 137.5f, y = -42.25f;
		NkTransfoPoint(t, 10.f, 20.f, x, y);
		const bool ok = t.Identite() && x == 137.5f && y == -42.25f;
		char d[96];
		nkentseu::NkSnprintf(d, sizeof(d), "(137.5,-42.25) -> (%.4f,%.4f)", x, y);
		verdict("1. la transformation neutre ne deplace aucun point, AU BIT PRES", ok, d);
	}
	// ── 2. ALLER-RETOUR : l'inverse est vraiment l'inverse ─────────────────
	//     ⚠️ ET LE CAS QUI COMPTE EST « miroir ET rotation ENSEMBLE ». Un inverse
	//     qui garderait l'ordre direct marche parfaitement tant qu'il n'y a QUE
	//     l'un des deux -- il ne tombe que dans le cas mixte, le plus rare, donc
	//     le dernier trouve. C'est pour ca qu'il est teste en premier.
	{
		bool ok = true;
		float32 pireEcart = 0.f;
		const float32 angles[5] = {0.f, 30.f, 90.f, 187.5f, -45.f};
		for (uint32 a = 0; a < 5; ++a)
			for (uint32 m = 0; m < 4; ++m) {
				NkTransfo t;
				t.deg = angles[a];
				t.mh = (m & 1u) != 0u;
				t.mv = (m & 2u) != 0u;
				float32 x = 300.f, y = 120.f;
				NkTransfoPoint(t, 200.f, 150.f, x, y);
				NkTransfoPointInverse(t, 200.f, 150.f, x, y);
				const float32 e1 = x - 300.f, e2 = y - 120.f;
				const float32 e = (e1 < 0.f ? -e1 : e1) + (e2 < 0.f ? -e2 : e2);
				if (e > pireEcart)
					pireEcart = e;
				if (e > 0.01f)
					ok = false;
			}
		char d[96];
		nkentseu::NkSnprintf(d, sizeof(d), "20 combinaisons (5 angles x 4 miroirs), pire ecart %.5f px",
				 pireEcart);
		verdict("2. l'inverse est l'inverse, MIROIR ET ROTATION MELANGES", ok, d);
	}
	// ── 3. LE QUART DE TOUR, SUR DES NOMBRES ECRITS A LA MAIN ──────────────
	//     Un cas dont on connait la reponse sans calculer : le coin haut-gauche
	//     d'un carre tourne de 90 degres horaires part en haut-droite.
	{
		NkTransfo t;
		t.deg = 90.f;
		const NkPaintRect r = {100.f, 100.f, 200.f, 200.f}; // centre (200,200)
		float32 x = 100.f, y = 100.f;
		NkTransfoPoint(t, 200.f, 200.f, x, y);
		const bool ok = proche(x, 300.f, 0.01f) && proche(y, 100.f, 0.01f);
		char d[96];
		nkentseu::NkSnprintf(d, sizeof(d), "(100,100) -> (%.2f,%.2f), attendu (300,100)", x, y);
		verdict("3. 90 degres horaires : le coin haut-gauche passe en haut-droite", ok, d);
		(void)r;
	}
	// ── 4. LE MIROIR HORIZONTAL RETOURNE, ET SEULEMENT EN X ────────────────
	{
		NkTransfo t;
		t.mh = true;
		float32 x = 100.f, y = 130.f;
		NkTransfoPoint(t, 200.f, 200.f, x, y);
		const bool ok = proche(x, 300.f, 0.001f) && proche(y, 130.f, 0.001f);
		char d[96];
		nkentseu::NkSnprintf(d, sizeof(d), "(100,130) -> (%.2f,%.2f), attendu (300,130)", x, y);
		verdict("4. le miroir H retourne en X et LAISSE Y", ok, d);
	}
	// ── 5. L'ORDRE COMPTE, ET ON LE PROUVE ─────────────────────────────────
	//     ⚠️ CE CAS EXISTE POUR QUE PERSONNE NE « SIMPLIFIE » L'ORDRE UN JOUR.
	//     Miroir-puis-rotation et rotation-puis-miroir donnent des resultats
	//     DIFFERENTS (le miroir change le signe de l'angle). Si les deux donnaient
	//     la meme chose, l'ordre serait un detail -- ce cas montre qu'il n'en est
	//     pas un, donc que le commentaire qui le fixe est necessaire.
	{
		NkTransfo t;
		t.deg = 45.f;
		t.mh = true;
		float32 x1 = 300.f, y1 = 200.f;
		NkTransfoPoint(t, 200.f, 200.f, x1, y1); // miroir puis rotation (le notre)
		// l'ordre inverse, ecrit a la main pour la comparaison
		float32 dx = 300.f - 200.f, dy = 200.f - 200.f;
		float32 s = 0.f, c = 1.f;
		NkSinCosDeg(45.f, s, c);
		float32 rx = dx * c - dy * s, ry = dx * s + dy * c;
		rx = -rx; // miroir APRES
		const float32 x2 = 200.f + rx, y2 = 200.f + ry;
		const bool different = !proche(x1, x2, 1.f) || !proche(y1, y2, 1.f);
		char d[112];
		nkentseu::NkSnprintf(d, sizeof(d), "notre ordre (%.1f,%.1f) vs ordre inverse (%.1f,%.1f)", x1, y1, x2,
				 y2);
		verdict("5. miroir-puis-rotation N'EST PAS rotation-puis-miroir : l'ordre fixe dans le "
				"fichier est une decision, pas un detail",
				different, d);
	}
	// ── 6. LA PROPAGATION AUX ENFANTS, ET LES MIROIRS QUI S'ANNULENT ───────
	//     ⚠️ LE PIEGE EST DANS LA COMPOSITION DES MIROIRS. Un OU logique aurait
	//     rendu « retourne » un enfant retourne sous un parent retourne -- alors
	//     qu'il doit revenir a l'endroit. La moitie des cas faux, et INVISIBLE
	//     sur toute forme symetrique : on ne l'aurait vu que sur du texte.
	st.doc.NewDocument("recette transfo", NkAuthor::Humain);
	{
		const int32 g = st.doc.AddChild(0, "", NkAuthor::Humain);
		const int32 e = st.doc.AddChild(g, "", NkAuthor::Humain);
		const int32 pf = st.doc.AddChild(e, "", NkAuthor::Humain);
		st.doc.nodes[(uint32)g].rotation = 30.f;
		st.doc.nodes[(uint32)g].miroirH = true;
		st.doc.nodes[(uint32)e].rotation = 15.f;
		st.doc.nodes[(uint32)e].miroirH = true; // s'annule avec celui du parent
		st.doc.nodes[(uint32)pf].rotation = 5.f;
		st.doc.nodes[(uint32)pf].miroirV = true;
		const NkTransfo tg = NkTransfoEffective(st.doc, g);
		const NkTransfo te = NkTransfoEffective(st.doc, e);
		const NkTransfo tp = NkTransfoEffective(st.doc, pf);
		const bool ok = tg.deg == 30.f && tg.mh && !tg.mv && te.deg == 45.f && !te.mh && !te.mv
						&& tp.deg == 50.f && !tp.mh && tp.mv;
		char d[144];
		nkentseu::NkSnprintf(d, sizeof(d), "groupe %.0f/H=%d ; enfant %.0f/H=%d ; petit-fils %.0f/H=%d,V=%d",
				 tg.deg, tg.mh ? 1 : 0, te.deg, te.mh ? 1 : 0, tp.deg, tp.mh ? 1 : 0,
				 tp.mv ? 1 : 0);
		verdict("6. la rotation S'ACCUMULE le long des ancetres et les miroirs S'ANNULENT deux "
				"a deux",
				ok, d);
	}
	// ── 7. LE PICKING D'UN RECTANGLE TOURNE ────────────────────────────────
	//     ⚠️ « UN CLIC DANS UN RECTANGLE TOURNE N'EST PLUS UN TEST DE
	//     RECTANGLE » (arbitrage Q42, point 3). Le cas prend un point qui est
	//     DANS la boite droite et HORS de la forme tournee, et un autre qui fait
	//     l'inverse -- sans ces deux-la, un picking qui ignorerait la rotation
	//     passerait au vert.
	{
		NkTransfo t;
		t.deg = 45.f;
		const NkPaintRect r = {100.f, 180.f, 200.f, 40.f}; // centre (200,200)
		// le centre est dans les deux cas
		const bool centre = NkPointDansRectTransfo(t, r, 200.f, 200.f);
		// un point pres du bord GAUCHE de la boite droite : dans la boite,
		// HORS de la forme tournee de 45 degres
		const bool horsApres = !NkPointDansRectTransfo(t, r, 110.f, 200.f);
		// ⚠️ ET LE POINT INVERSE EST CALCULE, PAS DEVINE -- MA PREMIERE SCENE
		//    ETAIT FAUSSE, PAS LE CODE. J'avais pris (200,260), « en dessous de
		//    la boite » : il est aussi en dehors de la forme tournee, et le cas
		//    echouait en accusant le picking. La barre tournee de 45 degres
		//    s'etend le long de la DIAGONALE : un point a 60 px du centre dans
		//    cette direction vaut (200 + 60/racine(2), 200 + 60/racine(2)).
		//    C'est exactement la lecon des scenes de snap de Q42 -- « dans un
		//    banc de geometrie, une scene mal choisie fabrique le contre-exemple
		//    qu'on cherchait a eviter, et on accuse le calcul ».
		const float32 dd = 60.f * 0.70710678f; // 42,43 px sur chaque axe
		const bool dansApres = NkPointDansRectTransfo(t, r, 200.f + dd, 200.f + dd);
		char d[128];
		nkentseu::NkSnprintf(d, sizeof(d), "centre=%d, (110,200) exclu=%d, (%.0f,%.0f) inclus=%d",
				 centre ? 1 : 0, horsApres ? 1 : 0, 200.f + dd, 200.f + dd, dansApres ? 1 : 0);
		verdict("7. le picking suit la forme TOURNEE, pas sa boite : un point de la boite en "
				"sort, un point hors de la boite y entre",
				centre && horsApres && dansApres, d);
	}
	// ── 8. L'ENGLOBANT D'UN OBJET TOURNE EST PLUS GRAND QUE SA BOITE ───────
	//     Une carte de 200x40 tournee de 45 degres occupe un carre bien plus
	//     large. Un survol qui lirait la boite droite dessinerait un cadre qui
	//     COUPE l'objet.
	{
		NkTransfo t;
		t.deg = 45.f;
		const NkPaintRect r = {100.f, 180.f, 200.f, 40.f};
		const NkPaintRect e = NkEnglobantTransfo(t, r);
		const float32 attendu = (200.f + 40.f) * 0.70710678f; // (w+h)/racine(2)
		const bool ok = proche(e.w, attendu, 0.5f) && proche(e.h, attendu, 0.5f)
						&& proche(e.x + e.w * 0.5f, 200.f, 0.01f);
		char d[128];
		nkentseu::NkSnprintf(d, sizeof(d), "200x40 a 45 deg -> %.1fx%.1f (attendu %.1f), centre conserve",
				 e.w, e.h, attendu);
		verdict("8. l'englobant d'un objet tourne grandit, et son centre ne bouge pas", ok, d);
	}
	// ── 9. LES POIGNEES DE ROTATION SONT DEHORS ────────────────────────────
	//     ⚠️ « LE COIN REDIMENSIONNE, SON EXTERIEUR TOURNE. » Posees SUR les
	//     coins, elles auraient vole le geste de redimensionnement, cent fois
	//     plus frequent. Le cas verifie qu'aucune des quatre ne CHEVAUCHE la
	//     boite -- pas seulement qu'elles sont « a peu pres la ».
	{
		const NkPaintRect r = {100.f, 100.f, 200.f, 150.f};
		bool toutesDehors = true;
		for (uint32 k = 0; k < NkNbPoigneesRotation(); ++k) {
			const NkPaintRect p = NkPoigneeRotation(r, k, 12.f);
			const bool chevauche = p.x < r.x + r.w && p.x + p.w > r.x && p.y < r.y + r.h
								   && p.y + p.h > r.y;
			if (chevauche)
				toutesDehors = false;
		}
		const NkPaintRect p0 = NkPoigneeRotation(r, 0, 12.f);
		const NkPaintRect p2 = NkPoigneeRotation(r, 2, 12.f);
		const bool enDiagonale = p0.x < r.x && p0.y < r.y && p2.x > r.x + r.w
								 && p2.y > r.y + r.h;
		char d[128];
		nkentseu::NkSnprintf(d, sizeof(d), "%u poignees, coin 0 a (%.0f,%.0f), coin 2 a (%.0f,%.0f)",
				 NkNbPoigneesRotation(), p0.x, p0.y, p2.x, p2.y);
		verdict("9. les 4 poignees de rotation sont DEHORS et en diagonale : elles ne volent "
				"pas le geste de redimensionnement",
				toutesDehors && enDiagonale, d);
	}
	// ── 10. L'ANGLE MESURE ET SON AIMANT ───────────────────────────────────
	{
		const bool droite = proche(NkAngleDeg(0.f, 0.f, 100.f, 0.f), 0.f, 0.2f);
		const bool bas = proche(NkAngleDeg(0.f, 0.f, 0.f, 100.f), 90.f, 0.2f);
		const bool diag = proche(NkAngleDeg(0.f, 0.f, 100.f, 100.f), 45.f, 0.2f);
		const bool gauche = proche(NkAngleDeg(0.f, 0.f, -100.f, 0.f), 180.f, 0.2f);
		// l'aimant : 15 degres, et SEULEMENT quand Maj est tenue
		const bool aimant = NkAngleAimante(43.f, true) == 45.f && NkAngleAimante(43.f, false) == 43.f
							&& NkAngleAimante(-8.f, true) == -15.f;
		// et un angle est une DIRECTION, pas un compteur de tours
		const bool norm = NkAngleNormalise(370.f) == 10.f && NkAngleNormalise(-90.f) == 270.f
						  && NkAngleNormalise(0.f) == 0.f;
		char d[128];
		nkentseu::NkSnprintf(d, sizeof(d), "0/90/45/180 = %.1f/%.1f/%.1f/%.1f ; aimant %s ; normalise %s",
				 NkAngleDeg(0.f, 0.f, 100.f, 0.f), NkAngleDeg(0.f, 0.f, 0.f, 100.f),
				 NkAngleDeg(0.f, 0.f, 100.f, 100.f), NkAngleDeg(0.f, 0.f, -100.f, 0.f),
				 aimant ? "ok" : "FAUX", norm ? "ok" : "FAUX");
		verdict("10. l'angle sous la souris, son aimant a 15 degres (Maj SEULEMENT) et sa "
				"normalisation dans [0,360)",
				droite && bas && diag && gauche && aimant && norm, d);
	}
	// ── 11. CE QUE LE PEINTRE NE SAIT PAS FAIRE EST DIT, PAS CACHE ─────────
	//     ⚠️ MESURE A LA SOURCE : `Text()` prend un `NkPaintRect`, droit par
	//     construction -- il n'existe aucun moyen de peindre du texte tourne avec
	//     ce peintre. Le champ s'enregistre quand meme (sinon tourner un groupe
	//     qui contient un texte echouerait A MOITIE, sans qu'on sache pourquoi),
	//     et l'application LE DIT. Un cas le tient, pas une note.
	st.doc.NewDocument("recette transfo", NkAuthor::Humain);
	{
		const int32 iR = st.doc.AddChild(0, "", NkAuthor::Humain);
		st.doc.nodes[(uint32)iR].shape = NkString("rect");
		const int32 iT = st.doc.AddChild(0, "", NkAuthor::Humain);
		st.doc.nodes[(uint32)iT].shape = NkString("text");
		st.doc.nodes[(uint32)iT].text = NkString("bonjour");
		const int32 iRT = st.doc.AddChild(0, "", NkAuthor::Humain);
		st.doc.nodes[(uint32)iRT].shape = NkString("rect");
		st.doc.nodes[(uint32)iRT].text = NkString("j'ai du texte");
		const char *r = NkRaisonRotationTexte();
		const bool ok = NkPeintureSaitTourner(st.doc.nodes[(uint32)iR])
						&& !NkPeintureSaitTourner(st.doc.nodes[(uint32)iT])
						// ET un rect QUI PORTE du texte non plus : la meme regle que
						// dans la table du double-clic, prise au meme endroit
						&& !NkPeintureSaitTourner(st.doc.nodes[(uint32)iRT]) && r && *r;
		verdict("11. le peintre sait tourner une forme, PAS un texte (ni un rect qui en porte), "
				"et la raison est DITE",
				ok, ok ? "rect oui, texte non, rect-a-texte non, phrase non vide" : "UNE MUETTE");
	}
	// ── 12. CONSERVATION + ALLER-RETOUR ────────────────────────────────────
	//     ⚠️ LE VOLET CONSERVATION, ECRIT D'ENTREE. Deux exigences ENSEMBLE,
	//     parce que l'une seule passe au vert sur une perte : (a) un document
	//     SANS transformation se reecrit octet pour octet (les trois cles ne sont
	//     pas ecrites), (b) les trois valeurs se retrouvent apres aller-retour.
	//     C'est la lecon de la mutation 7 de Q42, portee ici au lieu d'y etre
	//     redecouverte.
	st.doc.NewDocument("recette transfo", NkAuthor::Humain);
	{
		const int32 iA = st.doc.AddChild(0, "", NkAuthor::Humain);
		st.doc.nodes[(uint32)iA].shape = NkString("rect");
		NkString sans, sans2;
		st.doc.Save(sans);
		NkUIDocument d0;
		const bool lu0 = d0.Load(sans.Data());
		d0.Save(sans2);
		const bool propre = !sans.Contains("rotation") && !sans.Contains("miroir_h")
							&& !sans.Contains("miroir_v");
		const bool stable = lu0 && sans.Size() == sans2.Size()
							&& NkComponentDecl::StrEq(sans.Data(), sans2.Data());
		// (b) avec les trois
		st.doc.nodes[(uint32)iA].rotation = 42.5f;
		st.doc.nodes[(uint32)iA].miroirH = true;
		st.doc.nodes[(uint32)iA].miroirV = true;
		NkString avec;
		st.doc.Save(avec);
		NkUIDocument d1;
		const bool lu1 = d1.Load(avec.Data());
		float32 rot = -1.f;
		bool mh = false, mv = false;
		for (uint32 i = 1; lu1 && i < (uint32)d1.nodes.Size(); ++i)
			if (d1.nodes[i].rotation != 0.f) {
				rot = d1.nodes[i].rotation;
				mh = d1.nodes[i].miroirH;
				mv = d1.nodes[i].miroirV;
			}
		char d[176];
		nkentseu::NkSnprintf(d, sizeof(d), "sans : %s%s (%u o) | avec : %.1f deg, H=%d, V=%d",
				 propre ? "aucune cle" : "CLE PARASITE",
				 stable ? ", octet pour octet" : ", A BOUGE", (uint32)sans.Size(), rot,
				 mh ? 1 : 0, mv ? 1 : 0);
		verdict("12. CONSERVATION : un document sans transformation n'ecrit aucune des trois "
				"cles et se reecrit octet pour octet ; les trois valeurs survivent a "
				"l'aller-retour",
				propre && stable && rot == 42.5f && mh && mv, d);
	}
	// ── 13. LA COPIE NE REDRESSE PAS L'OBJET ───────────────────────────────
	//     ⚠️ LE CHEMIN FRERE : `CopierSousArbre` et la copie de TRANSPOSITION
	//     recopient toutes deux un noeud, membre a membre, a deux endroits
	//     eloignes du meme fichier. Oublier les trois champs dans l'une aurait
	//     donne une copie de meme forme et d'orientation differente -- et c'est
	//     le genre de perte qu'on ne voit pas sur une forme symetrique.
	st.doc.NewDocument("recette transfo", NkAuthor::Humain);
	{
		const int32 iA = st.doc.AddChild(0, "", NkAuthor::Humain);
		st.doc.nodes[(uint32)iA].shape = NkString("triangle");
		st.doc.nodes[(uint32)iA].rotation = 33.f;
		st.doc.nodes[(uint32)iA].miroirV = true;
		NkString a1;
		st.doc.Save(a1);
		NkUIDocument src;
		src.Load(a1.Data());
		// la copie passe par le meme chemin que Ctrl+C / Ctrl+V
		const int32 copie = st.doc.CopierSousArbre(src, iA, 0);
		const bool ok = st.doc.IsValidIndex(copie) && st.doc.nodes[(uint32)copie].rotation == 33.f
						&& st.doc.nodes[(uint32)copie].miroirV
						&& !st.doc.nodes[(uint32)copie].miroirH;
		char d[112];
		nkentseu::NkSnprintf(d, sizeof(d), "copie : %.0f deg, H=%d, V=%d",
				 st.doc.IsValidIndex(copie) ? st.doc.nodes[(uint32)copie].rotation : -1.f,
				 (st.doc.IsValidIndex(copie) && st.doc.nodes[(uint32)copie].miroirH) ? 1 : 0,
				 (st.doc.IsValidIndex(copie) && st.doc.nodes[(uint32)copie].miroirV) ? 1 : 0);
		verdict("13. copier un objet tourne et retourne le garde tourne et retourne", ok, d);
	}
	// ── 14. UN GROUPE TOURNE D'UN BLOC : SES ENFANTS SE DEPLACENT ──────────
	//     ⚠️ C'EST LE CAS QUI M'A EMPECHE D'ECRIRE UNE FAUSSE PROPAGATION, ET IL
	//     FAUT LE DIRE. J'allais me servir de l'ANGLE CUMULE (`NkTransfoEffective`)
	//     pour peindre les enfants d'un groupe tourne. C'est faux : chaque ancetre
	//     tourne autour de SON PROPRE CENTRE, pas de celui de l'enfant. Un angle
	//     cumule fait pivoter chaque enfant SUR LUI-MEME -- le groupe se disloque
	//     au lieu de tourner d'un bloc, et chaque element reste obstinement a sa
	//     place.
	//
	//     Le cas prend un enfant DECENTRE dans son groupe : c'est le seul qui
	//     distingue les deux. Un enfant centre sur le centre du groupe donnerait
	//     LE MEME resultat avec la bonne et la mauvaise formule -- et le banc
	//     serait passe au vert sur la faute exacte qu'il doit attraper.
	st.doc.NewDocument("recette transfo", NkAuthor::Humain);
	{
		const int32 g = st.doc.AddChild(0, "", NkAuthor::Humain);
		NkUINode &gn = st.doc.nodes[(uint32)g];
		gn.shape = NkString("rect");
		gn.layout.kind = NkLayoutKind::Free;
		gn.width.mode = NkSizeMode::Fixed;
		gn.width.value = 200.f;
		gn.height.mode = NkSizeMode::Fixed;
		gn.height.value = 200.f;
		const int32 e = st.doc.AddChild(g, "", NkAuthor::Humain);
		NkUINode &en = st.doc.nodes[(uint32)e];
		en.shape = NkString("rect");
		en.posX = 150.f; // DECENTRE : c'est ce qui rend le cas discriminant
		en.posY = 80.f;
		en.width.mode = NkSizeMode::Fixed;
		en.width.value = 40.f;
		en.height.mode = NkSizeMode::Fixed;
		en.height.value = 40.f;
		st.Recompute(surface);
		const NkPaintRect rg = st.layout.At(g), re = st.layout.At(e);
		const float32 cgx = rg.x + rg.w * 0.5f, cgy = rg.y + rg.h * 0.5f;
		const float32 cex = re.x + re.w * 0.5f, cey = re.y + re.h * 0.5f;
		// sans rotation, la matrice effective est l'identite
		const bool neutre = NkMatEffective(st.doc, st.layout, e).Identite();
		// on tourne LE GROUPE d'un quart de tour
		st.doc.nodes[(uint32)g].rotation = 90.f;
		const NkMat2D me = NkMatEffective(st.doc, st.layout, e);
		float32 x = cex, y = cey;
		NkMatPoint(me, x, y);
		// le centre de l'enfant doit tourner de 90 degres AUTOUR DU CENTRE DU
		// GROUPE : (dx,dy) -> (-dy,dx)
		const float32 ax = cgx - (cey - cgy), ay = cgy + (cex - cgx);
		const bool bouge = proche(x, ax, 0.5f) && proche(y, ay, 0.5f);
		// et il A VRAIMENT BOUGE (sinon « proche » serait vrai par accident sur
		// un enfant centre) : c'est la garde qui rend le cas discriminant
		const bool aBouge = !proche(x, cex, 1.f) || !proche(y, cey, 1.f);
		char d[160];
		nkentseu::NkSnprintf(d, sizeof(d), "enfant (%.0f,%.0f) -> (%.0f,%.0f), attendu (%.0f,%.0f)%s", cex,
				 cey, x, y, ax, ay, aBouge ? "" : " [IL N'A PAS BOUGE]");
		verdict("14. tourner un GROUPE deplace ses enfants autour du centre DU GROUPE : il "
				"tourne d'un bloc, il ne se disloque pas",
				neutre && bouge && aBouge, d);
	}
	// ── 15. LA MATRICE ET SON INVERSE, ET LE PICKING QUI EN DECOULE ────────
	{
		st.doc.nodes[1].rotation = 37.f; // le groupe du cas precedent
		const NkMat2D m = NkMatEffective(st.doc, st.layout, 2);
		const NkMat2D inv = NkMatInverse(m);
		float32 x = 321.f, y = 654.f;
		NkMatPoint(m, x, y);
		NkMatPoint(inv, x, y);
		const bool rond = proche(x, 321.f, 0.01f) && proche(y, 654.f, 0.01f);
		// et le picking de l'enfant suit : son centre est toujours dedans, ou
		// qu'il soit parti
		const NkPaintRect re = st.layout.At(2);
		float32 cx = re.x + re.w * 0.5f, cy = re.y + re.h * 0.5f;
		NkMatPoint(m, cx, cy);
		const bool dedans = NkPointDansNoeud(st.doc, st.layout, 2, cx, cy);
		// un point a 200 px de la, lui, n'y est pas
		const bool dehors = !NkPointDansNoeud(st.doc, st.layout, 2, cx + 200.f, cy);
		char d[128];
		nkentseu::NkSnprintf(d, sizeof(d), "aller-retour (%.3f,%.3f), centre dedans=%d, +200px dehors=%d", x,
				 y, dedans ? 1 : 0, dehors ? 1 : 0);
		verdict("15. la matrice s'inverse, et le picking d'un enfant de groupe tourne suit son "
				"centre la ou il est parti",
				rond && dedans && dehors, d);
	}
	printf("\nRECETTE TRANSFO : %d/%d %s\n", cas - echecs, cas,
		   echecs == 0 ? "PROUVEE" : "EN ECHEC");
	return echecs == 0 ? 0 : 1;
}
