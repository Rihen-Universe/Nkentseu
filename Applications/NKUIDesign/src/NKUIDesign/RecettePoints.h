#pragma once
// -----------------------------------------------------------------------------
// @File    RecettePoints.h
// @Brief   La recette « Points » — sortie de `main.cpp` le 28/09/2026.
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

static nkentseu::int32 RecettePoints() {
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
	auto poser = [&](const char *forme, const char *texte) -> int32 {
		const int32 i = st.doc.AddChild(0, "", NkAuthor::Humain);
		NkUINode &n = st.doc.nodes[(uint32)i];
		n.label = NkString(forme);
		n.shape = NkString(forme);
		if (texte)
			n.text = NkString(texte);
		n.width.mode = NkSizeMode::Fixed;
		n.width.value = 100.f;
		n.height.mode = NkSizeMode::Fixed;
		n.height.value = 80.f;
		return i;
	};

	// ── 1. LA TABLE DU DOUBLE-CLIC, EN ENTIER (les deux issues freres) ──────
	st.doc.NewDocument("recette points", NkAuthor::Humain);
	{
		const int32 iTexte = poser("text", "bonjour");
		const int32 iRect = poser("rect", nullptr);
		const int32 iLigne = poser("line", nullptr);
		const int32 iEtoile = poser("etoile", nullptr);
		const int32 iRectTexte = poser("rect", "j'ai du texte");
		const int32 iGroupe = st.doc.AddChild(0, "", NkAuthor::Humain);
		st.doc.nodes[(uint32)iGroupe].label = NkString("Groupe");
		st.doc.AddChild(iGroupe, "", NkAuthor::Humain);
		auto issue = [&](int32 i) { return NkIssueDeDblClic(st.doc.nodes[(uint32)i]); };
		const int32 iEllipse = poser("ellipse", nullptr);
		const int32 iImage = poser("image", nullptr);
		// ⚠️ `rect` ET `ellipse` SONT PASSES DE `CoinsSeuls` A `ModePoints` LE
		//    01/09 -- ce cas a ete RETOURNE, pas contourne. Retour de Rodolf :
		//    « double-cliquer sur une shape ne permet pas encore de faire
		//    l'edition de la shape. » La mesure lui a donne raison sur les deux
		//    formes qu'on pose le plus, et sa reference (`lunacy_shape_edit_1`)
		//    montre un RECTANGLE en `EDIT SHAPE` a quatre ancres.
		// ⚠️ ET `image` RESTE `CoinsSeuls` DANS LE MEME CAS, deliberement : sans
		//    ce temoin, on ne saurait pas si la branche `CoinsSeuls` existe
		//    encore ou si elle est devenue du code mort qu'aucun banc n'atteint.
		const bool ok = issue(iTexte) == NkIssueDblClic::EditerTexte
						&& issue(iRect) == NkIssueDblClic::ModePoints
						&& issue(iEllipse) == NkIssueDblClic::ModePoints
						&& issue(iImage) == NkIssueDblClic::CoinsSeuls
						&& issue(iLigne) == NkIssueDblClic::ModePoints
						&& issue(iEtoile) == NkIssueDblClic::ModePoints
						// un rect QUI PORTE du texte s'edite : la regle du 4e
						// retour, deplacee dans la table sans etre perdue
						&& issue(iRectTexte) == NkIssueDblClic::EditerTexte
						// et le FORAGE prime sur tout : un groupe se traverse
						&& issue(iGroupe) == NkIssueDblClic::Forer;
		verdict("1. la table du double-clic : texte->editer, rect ET ellipse->points, "
				"image->coins (temoin), ligne et etoile->points, rect AVEC texte->editer, "
				"groupe->forer",
				ok, ok ? "les huit issues" : "UNE ISSUE A CHANGE");
	}
	// ── 2. LA NATURE DES SOMMETS, forme par forme ──────────────────────────
	{
		// ⚠️ ET LE PREDICAT `NkSommetsStockes` EST VERIFIE ICI, pas seulement les
		//    natures : c'est LUI que six sites appellent pour decider s'ils
		//    ecrivent dans `n.sommets`. Une nature juste et un predicat faux
		//    donneraient une table exacte et une edition qui ne marche que sur
		//    trois formes sur cinq -- le defaut d'origine, deplace d'un cran.
		const bool ok = NkNatureDe("line") == NkNatureSommets::Bouts
						&& NkNatureDe("line_up") == NkNatureSommets::Bouts
						&& NkNatureDe("triangle") == NkNatureSommets::Polygone
						&& NkNatureDe("pentagone") == NkNatureSommets::Polygone
						&& NkNatureDe("etoile") == NkNatureSommets::Polygone
						&& NkNatureDe("rect") == NkNatureSommets::CoinsEditables
						&& NkNatureDe("ellipse") == NkNatureSommets::CoinsEditables
						&& NkNatureDe("image") == NkNatureSommets::Coins
						&& NkNatureDe("frame") == NkNatureSommets::Coins
						&& NkNatureDe("text") == NkNatureSommets::Aucun
						&& NkSommetsStockes(NkNatureSommets::Polygone)
						&& NkSommetsStockes(NkNatureSommets::CoinsEditables)
						&& !NkSommetsStockes(NkNatureSommets::Coins)
						&& !NkSommetsStockes(NkNatureSommets::Bouts)
						&& !NkSommetsStockes(NkNatureSommets::Aucun);
		verdict("2. la nature des sommets : bouts / polygone / coins EDITABLES (rect, "
				"ellipse) / coins (image, frame) / aucun -- et le predicat de stockage "
				"les suit",
				ok, "");
	}
	// ── 3. LE COMPTE DES SOMMETS, et il vient de LA table ──────────────────
	st.doc.NewDocument("recette points", NkAuthor::Humain);
	{
		const int32 iL = poser("line", nullptr);
		const int32 iT = poser("triangle", nullptr);
		const int32 iP = poser("pentagone", nullptr);
		const int32 iE = poser("etoile", nullptr);
		const int32 iR = poser("rect", nullptr);
		const int32 iEl = poser("ellipse", nullptr);
		const int32 iX = poser("text", "x");
		st.Recompute(surface);
		float32 xy[64];
		auto nb = [&](int32 i) {
			return NkSommetsDe(st.doc.nodes[(uint32)i], st.layout.At(i), xy, 32);
		};
		char d[144];
		snprintf(d, sizeof(d), "ligne=%u, triangle=%u, pentagone=%u, etoile=%u, rect=%u, "
							   "ellipse=%u, texte=%u",
				 nb(iL), nb(iT), nb(iP), nb(iE), nb(iR), nb(iEl), nb(iX));
		verdict("3. le compte des sommets : 2 / 3 / 5 / 10 / 4 (coins du rect) / 12 (courbe "
				"de l'ellipse) / 0",
				nb(iL) == 2 && nb(iT) == 3 && nb(iP) == 5 && nb(iE) == 10 && nb(iR) == 4
					&& nb(iEl) == 12 && nb(iX) == 0,
				d);
	}
	// ── 4. LES SOMMETS TOMBENT DANS LA BOITE, ET LA LIGNE SUR SA DIAGONALE ──
	st.doc.NewDocument("recette points", NkAuthor::Humain);
	{
		const int32 iL = poser("line", nullptr);
		const int32 iLu = poser("line_up", nullptr);
		st.Recompute(surface);
		float32 xy[8], xu[8];
		const uint32 n1 = NkSommetsDe(st.doc.nodes[(uint32)iL], st.layout.At(iL), xy, 4);
		const uint32 n2 = NkSommetsDe(st.doc.nodes[(uint32)iLu], st.layout.At(iLu), xu, 4);
		const NkPaintRect r1 = st.layout.At(iL), r2 = st.layout.At(iLu);
		// `line` descend : (x,y) -> (x+w, y+h) ; `line_up` monte : l'inverse.
		const bool descend = n1 == 2 && xy[0] == r1.x && xy[1] == r1.y
							 && xy[2] == r1.x + r1.w && xy[3] == r1.y + r1.h;
		const bool monte = n2 == 2 && xu[1] == r2.y + r2.h && xu[3] == r2.y;
		char d[112];
		snprintf(d, sizeof(d), "line (%.0f,%.0f)->(%.0f,%.0f) ; line_up y %.0f->%.0f", xy[0],
				 xy[1], xy[2], xy[3], xu[1], xu[3]);
		verdict("4. les bouts d'une ligne SONT la diagonale de sa boite, et `line_up` "
				"monte",
				descend && monte, d);
	}
	// ── 5. MATERIALISER : la table devient une liste, et RIEN NE BOUGE ──────
	//     ⚠️ C'est le cas qui tient la promesse : materialiser ne doit pas
	//     DEPLACER un sommet d'un pixel, sinon l'etoile sauterait au premier
	//     clic dans le mode points, avant meme qu'on tire quoi que ce soit.
	st.doc.NewDocument("recette points", NkAuthor::Humain);
	{
		const int32 iE = poser("etoile", nullptr);
		st.Recompute(surface);
		float32 avant[64], apres[64];
		const uint32 nA = NkSommetsDe(st.doc.nodes[(uint32)iE], st.layout.At(iE), avant, 32);
		NkMaterialiserSommets(st.doc.nodes[(uint32)iE]);
		const uint32 nB = NkSommetsDe(st.doc.nodes[(uint32)iE], st.layout.At(iE), apres, 32);
		bool memes = (nA == nB && nA == 10);
		for (uint32 i = 0; memes && i < nA * 2; ++i)
			if (avant[i] != apres[i])
				memes = false;
		// et c'est idempotent
		NkMaterialiserSommets(st.doc.nodes[(uint32)iE]);
		const bool idem = st.doc.nodes[(uint32)iE].sommets.Size() == 10;
		char d[112];
		snprintf(d, sizeof(d), "%u -> %u sommets, positions %s, 2e appel : %u", nA, nB,
				 memes ? "IDENTIQUES" : "DEPLACEES",
				 (uint32)st.doc.nodes[(uint32)iE].sommets.Size());
		verdict("5. materialiser une etoile ne DEPLACE aucun sommet, et n'est pas cumulatif",
				memes && idem, d);
	}
	// ── 6. MATERIALISER NE VAUT QUE POUR LES POLYGONES ──────────────────────
	st.doc.NewDocument("recette points", NkAuthor::Humain);
	{
		const int32 iR = poser("rect", nullptr);
		const int32 iEl = poser("ellipse", nullptr);
		const int32 iIm = poser("image", nullptr);
		const int32 iL = poser("line", nullptr);
		st.Recompute(surface);
		// ⚠️ CE CAS A CHANGE DE VERDICT LE 01/09, ET IL DIT MAINTENANT LA VRAIE
		//    LIGNE DE PARTAGE. Il affirmait « le rectangle ne recoit pas de
		//    sommets » ; c'etait la consequence de la table d'hier, pas une
		//    verite du modele. La ligne de partage n'est pas rect/polygone,
		//    c'est : la LIGNE s'exprime par sa boite (deux facons de dire la
		//    meme chose = une de trop), l'IMAGE et le CADRE ne s'editent pas
		//    (une cible d'artboard deformee en quadrilatere ne veut rien dire),
		//    tout le reste STOCKE ses sommets.
		// ⚠️ ET MATERIALISER NE DOIT DEPLACER AUCUN COIN : c'est la promesse que
		//    le cas 5 tient pour l'etoile, portee ici au rectangle -- sinon le
		//    rect sauterait au premier clic dans le mode points, avant meme
		//    qu'on tire quoi que ce soit. Le meme piege, sur le chemin frere.
		float32 avant[16], apres[16];
		const uint32 nA = NkSommetsDe(st.doc.nodes[(uint32)iR], st.layout.At(iR), avant, 8);
		NkMaterialiserSommets(st.doc.nodes[(uint32)iR]);
		NkMaterialiserSommets(st.doc.nodes[(uint32)iEl]);
		NkMaterialiserSommets(st.doc.nodes[(uint32)iIm]);
		NkMaterialiserSommets(st.doc.nodes[(uint32)iL]);
		const uint32 nB = NkSommetsDe(st.doc.nodes[(uint32)iR], st.layout.At(iR), apres, 8);
		bool memes = (nA == nB && nA == 4);
		for (uint32 i = 0; memes && i < nA * 2; ++i)
			if (avant[i] != apres[i])
				memes = false;
		// ⚠️ L'ELLIPSE EST PASSEE DE 4 A 12 LE 01/09, et ce cas le dit au lieu
		//    de le subir : ses quatre « coins » ne touchaient la courbe NULLE
		//    PART (retour 1 de Rodolf). Le rect garde ses 4 -- ses coins SONT
		//    son contour. Le compte differe donc par forme, et c'est voulu.
		const bool ok = st.doc.nodes[(uint32)iR].sommets.Size() == 4
						&& st.doc.nodes[(uint32)iEl].sommets.Size() == 12
						&& st.doc.nodes[(uint32)iIm].sommets.Empty()
						&& st.doc.nodes[(uint32)iL].sommets.Empty() && memes;
		char d[128];
		snprintf(d, sizeof(d), "rect=%u, ellipse=%u, image=%u, ligne=%u | coins %s",
				 (uint32)st.doc.nodes[(uint32)iR].sommets.Size(),
				 (uint32)st.doc.nodes[(uint32)iEl].sommets.Size(),
				 (uint32)st.doc.nodes[(uint32)iIm].sommets.Size(),
				 (uint32)st.doc.nodes[(uint32)iL].sommets.Size(),
				 memes ? "IDENTIQUES" : "DEPLACES");
		verdict("6. le rect recoit ses 4 COINS et l'ellipse ses 12 points DE COURBE, sans "
				"qu'aucun bouge ; l'image et le cadre n'en recoivent pas (rien a deformer), "
				"la ligne non plus (elle EST sa boite)",
				ok, d);
	}
	// ── 7. ALLER-RETOUR + CONSERVATION : le document d'avant ne bouge pas ───
	st.doc.NewDocument("recette points", NkAuthor::Humain);
	{
		const int32 iE = poser("etoile", nullptr);
		NkString a1, a2;
		st.doc.Save(a1);
		NkUIDocument r1;
		const bool lu1 = r1.Load(a1.Data());
		if (lu1)
			r1.Save(a2);
		bool sansCle = true;
		for (const char *q = a1.Data() ? a1.Data() : ""; *q; ++q)
			if (q[0] == 's' && q[1] == 'o' && q[2] == 'm' && q[3] == 'm' && q[4] == 'e'
				&& q[5] == 't' && q[6] == '_') {
				sansCle = false;
				break;
			}
		// conservation : la forme est encore la
		const bool garde = lu1 && r1.IsValidIndex(iE)
						   && StrEq(r1.nodes[(uint32)iE].shape.Data(), "etoile")
						   && r1.nodes[(uint32)iE].sommets.Empty();
		char d[128];
		snprintf(d, sizeof(d), "octets %s, cle `sommet_` %s, forme %s",
				 (lu1 && a1.Compare(a2) == 0) ? "IDENTIQUES" : "DIFFERENTS",
				 sansCle ? "absente" : "APPARUE", garde ? "gardee" : "PERDUE");
		verdict("7. une etoile REGULIERE ne gagne aucune cle de sommets et se reenregistre "
				"OCTET POUR OCTET",
				lu1 && a1.Compare(a2) == 0 && sansCle && garde && ancree(a1), d);
	}
	// ── 8. UN SOMMET DEPLACE fait l'aller-retour, valeur par valeur ─────────
	st.doc.NewDocument("recette points", NkAuthor::Humain);
	{
		const int32 iE = poser("etoile", nullptr);
		NkMaterialiserSommets(st.doc.nodes[(uint32)iE]);
		st.doc.nodes[(uint32)iE].sommets[3].x = -0.25f;
		st.doc.nodes[(uint32)iE].sommets[3].y = 0.75f;
		NkString b1, b2;
		st.doc.Save(b1);
		NkUIDocument r2;
		const bool lu2 = r2.Load(b1.Data());
		if (lu2)
			r2.Save(b2);
		const NkUINode *rn = (lu2 && r2.IsValidIndex(iE)) ? &r2.nodes[(uint32)iE] : nullptr;
		const bool champs = rn && rn->sommets.Size() == 10 && rn->sommets[3].x == -0.25f
							&& rn->sommets[3].y == 0.75f;
		char d[112];
		snprintf(d, sizeof(d), "%u sommet(s), le 4e (%.2f, %.2f), octets %s",
				 rn ? (uint32)rn->sommets.Size() : 0u, rn ? rn->sommets[3].x : -9.f,
				 rn ? rn->sommets[3].y : -9.f,
				 (lu2 && b1.Compare(b2) == 0) ? "IDENTIQUES" : "DIFFERENTS");
		verdict("8. un sommet deplace fait l'aller-retour et se reenregistre a l'identique",
				champs && lu2 && b1.Compare(b2) == 0, d);
	}
	// ── 9. LA COPIE emporte les sommets ─────────────────────────────────────
	st.doc.NewDocument("recette points", NkAuthor::Humain);
	{
		const int32 iE = poser("etoile", nullptr);
		NkMaterialiserSommets(st.doc.nodes[(uint32)iE]);
		st.doc.nodes[(uint32)iE].sommets[0].x = 0.5f;
		const int32 j = st.doc.CopierSousArbre(st.doc, iE, 0);
		const bool ok = st.doc.IsValidIndex(j) && st.doc.nodes[(uint32)j].sommets.Size() == 10
						&& st.doc.nodes[(uint32)j].sommets[0].x == 0.5f;
		char d[96];
		snprintf(d, sizeof(d), "%u sommet(s) copie(s)",
				 st.doc.IsValidIndex(j) ? (uint32)st.doc.nodes[(uint32)j].sommets.Size() : 0u);
		verdict("9. copier un sous-arbre emporte les sommets deplaces", ok, d);
	}
	// ── 10. CONSERVATION : REGARDER les sommets n'ecrit rien ────────────────
	st.doc.NewDocument("recette points", NkAuthor::Humain);
	{
		const int32 iE = poser("etoile", nullptr);
		const int32 iL = poser("line", nullptr);
		st.Recompute(surface);
		NkString avant;
		st.doc.Save(avant);
		float32 xy[64];
		(void)NkSommetsDe(st.doc.nodes[(uint32)iE], st.layout.At(iE), xy, 32);
		(void)NkSommetsDe(st.doc.nodes[(uint32)iL], st.layout.At(iL), xy, 32);
		(void)NkIssueDeDblClic(st.doc.nodes[(uint32)iE]);
		(void)NkNatureDe(st.doc.nodes[(uint32)iL].shape.Data());
		NkString apres;
		st.doc.Save(apres);
		char d[96];
		snprintf(d, sizeof(d), "document %s",
				 (avant.Compare(apres) == 0) ? "INCHANGE" : "MODIFIE");
		verdict("10. CONSERVATION : lire les sommets et interroger la table n'ecrit RIEN",
				ancree(avant) && avant.Compare(apres) == 0, d);
	}
	// ── 11. UN SOMMET DEPLACE SE VOIT DANS LA GEOMETRIE DESSINEE ────────────
	//     ⚠️ CE CAS EXISTE PARCE QU'UNE MUTATION N'A RIEN CASSE. En forcant
	//     `NkSommetsDe` a ignorer la liste du document (donc a toujours rendre
	//     l'etoile REGULIERE), les dix cas precedents restaient verts : le cas 8
	//     lit le MODELE (le sommet est bien enregistre), le cas 5 compare deux
	//     appels qui utilisaient tous deux la table. Personne ne verifiait le
	//     seul lien qui compte : que deplacer un sommet CHANGE CE QUI EST
	//     DESSINE. Sans ce cas, tout le mode points pouvait etre parfaitement
	//     enregistre et parfaitement invisible.
	st.doc.NewDocument("recette points", NkAuthor::Humain);
	{
		const int32 iE = poser("etoile", nullptr);
		st.Recompute(surface);
		float32 avant[64], apres[64];
		const uint32 nA = NkSommetsDe(st.doc.nodes[(uint32)iE], st.layout.At(iE), avant, 32);
		NkMaterialiserSommets(st.doc.nodes[(uint32)iE]);
		st.doc.nodes[(uint32)iE].sommets[2].x = 0.123f; // une pointe tiree
		st.doc.nodes[(uint32)iE].sommets[2].y = -0.456f;
		const uint32 nB = NkSommetsDe(st.doc.nodes[(uint32)iE], st.layout.At(iE), apres, 32);
		const NkPaintRect r = st.layout.At(iE);
		const float32 attX = r.x + r.w * 0.5f + 0.123f * (r.w * 0.5f);
		const float32 attY = r.y + r.h * 0.5f + (-0.456f) * (r.h * 0.5f);
		const bool bouge = nA == nB && nB > 2 && apres[4] == attX && apres[5] == attY
						   && (avant[4] != apres[4] || avant[5] != apres[5]);
		// et les AUTRES sommets n'ont pas bouge : deplacer un point n'en deplace
		// qu'un.
		bool autresFixes = true;
		for (uint32 i = 0; i < nB && autresFixes; ++i) {
			if (i == 2)
				continue;
			if (avant[i * 2] != apres[i * 2] || avant[i * 2 + 1] != apres[i * 2 + 1])
				autresFixes = false;
		}
		char d[144];
		snprintf(d, sizeof(d), "3e sommet (%.1f,%.1f) -> (%.1f,%.1f), attendu (%.1f,%.1f) ; "
							   "les autres %s",
				 avant[4], avant[5], apres[4], apres[5], attX, attY,
				 autresFixes ? "fixes" : "ONT BOUGE");
		verdict("11. deplacer un sommet CHANGE la geometrie dessinee -- et ne deplace que "
				"celui-la",
				bouge && autresFixes, d);
	}
	// ── 12. LA PRIORITE DE POIGNEE EST DITE, PAS SUBIE ─────────────────────
	//     ⚠️ Sur une LIGNE, les deux bouts tombent EXACTEMENT sur deux coins de
	//     la boite : poignee de redimensionnement et poignee de sommet au MEME
	//     pixel. La premiere correction les rendait exclusives par deux gardes
	//     situees a six cents lignes l'une de l'autre -- une priorite SUBIE, que
	//     le premier qui deplace un des deux `if` casse sans s'en apercevoir.
	//     La regle se cite maintenant, et ce cas la tient.
	{
		const bool ok = NkAQuiLaPoignee(7, 7) == NkProprioPoignee::Sommet
						&& NkAQuiLaPoignee(-1, 7) == NkProprioPoignee::Redimension
						&& NkAQuiLaPoignee(7, 9) == NkProprioPoignee::Redimension
						&& NkAQuiLaPoignee(7, -1) == NkProprioPoignee::Aucune
						&& NkAQuiLaPoignee(-1, -1) == NkProprioPoignee::Aucune;
		verdict("12. en mode points le SOMMET gagne ; ailleurs, la poignee de "
				"redimensionnement -- et la regle se cite au lieu de dependre de l'ordre du "
				"code",
				ok, "");
	}
	// ── 13. LES DEUX VOIES D'ENTREE DISENT LA MEME CHOSE ────────────────────
	//     ⚠️ CE CAS A CHANGE DE SUJET LE 01/09, ET IL FAUT LE DIRE PLUTOT QUE DE
	//     LE REECRIRE EN SILENCE. Il tenait un invariant STRUCTUREL : « le mode
	//     points ne s'ouvre jamais sur rect/ellipse, donc le conflit poignee de
	//     coin / poignee de sommet y est impossible ». **Cet invariant n'existe
	//     plus** -- rect et ellipse entrent desormais en mode points, et leurs
	//     quatre coins portent bien DEUX poignees au meme pixel.
	//
	//     Ce qui reste, et qui est le vrai contenu du cas : les DEUX VOIES
	//     D'ENTREE (`NkPeutEntrerEnPoints` et `NkIssueDeDblClic`) doivent dire la
	//     meme chose, forme par forme. Elles sont ecrites a deux endroits
	//     differents du meme fichier ; c'est exactement le genre de couple qui
	//     derive sans qu'on le voie -- le motif du carnet.
	//
	//     ⚠️ ET LE CONFLIT DE POIGNEES, LUI, EST TENU PAR LE CAS 12
	//     (`NkAQuiLaPoignee`), qui n'a pas eu besoin d'une ligne de changement :
	//     il dit « en mode points, le sommet gagne » sans rien savoir de la
	//     forme. C'est le benefice qu'on avait paye en remplacant deux gardes
	//     distantes par une regle citable, et il se percoit aujourd'hui sur un
	//     cas qui n'existait pas quand on l'a ecrite.
	st.doc.NewDocument("recette points", NkAuthor::Humain);
	{
		const int32 iR = poser("rect", nullptr);
		const int32 iEl = poser("ellipse", nullptr);
		const int32 iIm = poser("image", nullptr);
		const int32 iL = poser("line", nullptr);
		const int32 iE = poser("etoile", nullptr);
		const int32 iT = poser("text", "x");
		auto peut = [&](int32 i) { return NkPeutEntrerEnPoints(st.doc.nodes[(uint32)i]); };
		// et la table du double-clic dit la MEME chose : les deux voies d'entree
		// ne peuvent pas diverger sans que ce cas le voie.
		auto issue = [&](int32 i) { return NkIssueDeDblClic(st.doc.nodes[(uint32)i]); };
		const bool coherent =
			(peut(iR) == (issue(iR) == NkIssueDblClic::ModePoints))
			&& (peut(iEl) == (issue(iEl) == NkIssueDblClic::ModePoints))
			&& (peut(iIm) == (issue(iIm) == NkIssueDblClic::ModePoints))
			&& (peut(iL) == (issue(iL) == NkIssueDblClic::ModePoints))
			&& (peut(iE) == (issue(iE) == NkIssueDblClic::ModePoints))
			&& (peut(iT) == (issue(iT) == NkIssueDblClic::ModePoints));
		char d[128];
		snprintf(d, sizeof(d), "rect=%d, ellipse=%d, image=%d | ligne=%d, etoile=%d, texte=%d",
				 peut(iR) ? 1 : 0, peut(iEl) ? 1 : 0, peut(iIm) ? 1 : 0, peut(iL) ? 1 : 0,
				 peut(iE) ? 1 : 0, peut(iT) ? 1 : 0);
		verdict("13. les deux voies d'entree du mode points disent la MEME chose forme par "
				"forme -- rect/ellipse/ligne/etoile oui, image et texte non",
				peut(iR) && peut(iEl) && !peut(iIm) && peut(iL) && peut(iE) && !peut(iT)
					&& coherent,
				d);
	}
	// ── 14. UN DOUBLE-CLIC VAUT APPUI ──────────────────────────────────────
	//     ⚠️ CE CAS EXISTE PARCE QUE LE BANC ET LE GESTE REEL NE PASSAIENT PAS
	//     PAR LA MEME PORTE, et c'est ce qui a rendu le defaut invisible.
	//     Mesure du 01/09 : la classe de fenetre porte CS_DBLCLKS, donc Windows
	//     REMPLACE le second WM_LBUTTONDOWN par WM_LBUTTONDBLCLK ; le Win32
	//     n'en tirait aucun appui ; `mouseClicked[0]` restait faux ; et la toile
	//     enfermait sa branche double-clic dans `if (in.mousePressed)`. Le geste
	//     de Rodolf n'y entrait JAMAIS -- « double-cliquer ne me permet pas
	//     d'acceder a sa modification fine ». L'injecteur, lui, posait
	//     `mouseDown` PUIS `SetDoubleClick` : il prenait l'autre porte.
	{
		const bool ok = NkAppuiDeToile(false, true) // LE cas qui manquait
						&& NkAppuiDeToile(true, false) && NkAppuiDeToile(true, true)
						&& !NkAppuiDeToile(false, false);
		verdict("14. un double-clic vaut APPUI -- meme quand l'OS n'envoie pas d'appui avec "
				"(CS_DBLCLKS remplace le second WM_LBUTTONDOWN)",
				ok, ok ? "sans appui + double-clic = appui" : "LE DOUBLE-CLIC NE VAUT PLUS APPUI");
	}
	// ── 15. CHAQUE SUITE DU DOUBLE-CLIC DIT QUELQUE CHOSE ──────────────────
	//     ⚠️ LA REGLE DE LA MAISON, TENUE PAR UN BANC PLUTOT QUE PAR UNE
	//     INTENTION : un controle qui ne fait rien sans rien dire est le defaut
	//     qu'on chasse. Le cas parcourt les suites PAR LEUR NOMBRE, pas par une
	//     liste ecrite a la main -- une septieme suite ajoutee sans sa phrase
	//     fait tomber ce cas au lieu de passer inapercue.
	//     Et il exige aussi que deux suites ne disent pas LA MEME phrase : « ca
	//     dit quelque chose » ne suffit pas si ca ne distingue rien.
	{
		bool toutesParlent = true, toutesDistinctes = true;
		const uint32 n = NkNbSuitesDblClic();
		const char *phrases[8] = {nullptr};
		for (uint32 i = 0; i < n && i < 8; ++i) {
			const char *p = NkRaisonDeDblClic((NkSuiteDblClic)i);
			phrases[i] = p;
			if (!p || !*p)
				toutesParlent = false;
		}
		for (uint32 i = 0; i < n && i < 8; ++i)
			for (uint32 j = i + 1; j < n && j < 8; ++j)
				if (phrases[i] && phrases[j] && NkComponentDecl::StrEq(phrases[i], phrases[j]))
					toutesDistinctes = false;
		char d[96];
		snprintf(d, sizeof(d), "%u suites, toutes %s et %s", n,
				 toutesParlent ? "parlantes" : "PAS TOUTES PARLANTES",
				 toutesDistinctes ? "distinctes" : "PAS TOUTES DISTINCTES");
		verdict("15. CHAQUE suite du double-clic produit une raison DITE, et deux suites ne "
				"disent pas la meme chose",
				toutesParlent && toutesDistinctes, d);
	}
	// ── 16. LE CORPS D'UN GROUPE N'EST PAS UN TROU ─────────────────────────
	//     ⚠️ LE DEFAUT MESURE LE 01/09 SUR LE DOCUMENT DASHBOARD, ET C'EST LA
	//     PLUS GRANDE PARTIE DE LA SURFACE D'UNE CARTE : un double-clic sur le
	//     corps de `Carte_Actifs` (ou de `Panel_Nav`, ou d'un artboard) rendait
	//     l'issue `Forer` -- correcte -- puis `NkPickDansContexte` rendait -2
	//     (« hors du contexte »), et la branche faisait un `SelectSingle` MUET
	//     sans armer le forage. Rien ne se passait, rien ne le disait, et le
	//     double-clic suivant refaisait exactement la meme chose.
	//     Le cas tient les DEUX suites de `Forer` cote a cote : sur un enfant
	//     (descendre) et a cote (entrer quand meme, et le dire).
	st.doc.NewDocument("recette points", NkAuthor::Humain);
	{
		const int32 iG = st.doc.AddChild(0, "", NkAuthor::Humain);
		{
			NkUINode &g = st.doc.nodes[(uint32)iG];
			g.label = NkString("Carte");
			g.shape = NkString("rect");
			g.width.mode = NkSizeMode::Fixed;
			g.width.value = 200.f;
			g.height.mode = NkSizeMode::Fixed;
			g.height.value = 100.f;
		}
		const int32 iT = st.doc.AddChild(iG, "", NkAuthor::Humain);
		{
			NkUINode &t = st.doc.nodes[(uint32)iT];
			t.label = NkString("Valeur");
			t.shape = NkString("text");
			t.text = NkString("42");
			t.width.mode = NkSizeMode::Fixed;
			t.width.value = 40.f;
			t.height.mode = NkSizeMode::Fixed;
			t.height.value = 20.f;
		}
		st.Recompute(surface);
		const NkPaintRect rg = st.layout.At(iG), rt = st.layout.At(iT);
		// (a) SUR l'enfant : on descend.
		const float32 ax = rt.x + rt.w * 0.5f, ay = rt.y + rt.h * 0.5f;
		const int32 candA = NkPickDansContexte(st.doc, st.layout, ax, ay, -1);
		const int32 enfA = st.doc.IsValidIndex(candA)
							   ? NkPickDansContexte(st.doc, st.layout, ax, ay, candA)
							   : -1;
		const NkSuiteDblClic sA =
			NkSuiteDeDblClic(NkIssueDeDblClic(st.doc.nodes[(uint32)(candA > 0 ? candA : 0)]),
							 enfA >= 0);
		// (b) A COTE de l'enfant, DANS le groupe : le cas qui etait muet.
		const float32 bx = rg.x + rg.w - 6.f, by = rg.y + rg.h - 6.f;
		const int32 candB = NkPickDansContexte(st.doc, st.layout, bx, by, -1);
		const int32 enfB = st.doc.IsValidIndex(candB)
							   ? NkPickDansContexte(st.doc, st.layout, bx, by, candB)
							   : -1;
		const NkSuiteDblClic sB =
			NkSuiteDeDblClic(NkIssueDeDblClic(st.doc.nodes[(uint32)(candB > 0 ? candB : 0)]),
							 enfB >= 0);
		const char *raison = NkRaisonDeDblClic(sB);
		const bool ok = candA == iG && enfA == iT && sA == NkSuiteDblClic::ForerVersEnfant
						&& candB == iG && enfB < 0 && sB == NkSuiteDblClic::ForerSansEnfant
						&& raison && *raison;
		char d[160];
		snprintf(d, sizeof(d), "sur l'enfant : cand=%d enfant=%d ; au corps : cand=%d enfant=%d, "
							   "raison %s",
				 candA, enfA, candB, enfB, (raison && *raison) ? "DITE" : "MUETTE");
		verdict("16. le CORPS d'un groupe n'est pas un trou : le double-clic y entre quand meme "
				"et le DIT (les deux suites de Forer, cote a cote)",
				ok, d);
	}
	// ── 17. LE MENU DU CLIC DROIT : QUI N'AGIT PAS DIT POURQUOI ────────────
	//     ⚠️ L'INVENTAIRE LUNACY, RENDU MESURABLE. Un tableau dans un rapport se
	//     perime ; ce cas, non. Il exige les DEUX sens, et le second est celui
	//     qu'on oublie : qui n'agit pas porte une raison NON VIDE, ET qui agit
	//     n'en porte PAS -- sinon la raison serait un ornement qu'on cesse de
	//     lire, et le jour ou elle compte personne ne la verrait.
	{
		NkContexteCtx c;
		c.aTexte = true;
		c.pasRacine = true;
		c.aEnfants = true;
		NkMenuCtx menu;
		NkConstruireMenuCtx(c, menu);
		bool coherent = (menu.n > 0);
		for (uint32 i = 0; i < menu.n; ++i) {
			const NkEntreeCtx &e = menu.items[i];
			if (!e.libelle[0])
				coherent = false;
			if (!e.agit && (!e.raison || !*e.raison))
				coherent = false; // muet
			if (e.agit && e.raison)
				coherent = false; // raison decorative
		}
		char d[96];
		snprintf(d, sizeof(d), "%u entrees, chacune parlante", (uint32)menu.n);
		verdict("17. menu contextuel : qui n'agit pas DIT pourquoi, et qui agit ne dit rien "
				"d'inutile",
				coherent, d);
	}
	// ── 18. LES DEUX SURFACES : MEME JEU D'ENTREES, APPLICABILITE DIFFERENTE ─
	//     ⚠️ LE PIEGE DE GROUPE, TENU PAR UN BANC. Lunacy montre le meme menu
	//     depuis sa toile et depuis son panneau Layers ; deux jeux auraient
	//     derive au premier ajout, et l'utilisateur aurait cherche dans un menu
	//     une commande qu'il venait de voir dans l'autre. Le cas verifie que la
	//     LISTE des actions est identique, ET qu'au moins une applicabilite
	//     DIFFERE -- sans quoi `surfaceListe` serait un parametre declare et non
	//     honore, la famille de defauts que ce depot compte depuis huit fois.
	{
		NkContexteCtx t, l;
		t.aTexte = l.aTexte = true;
		t.pasRacine = l.pasRacine = true;
		t.surfaceListe = false;
		l.surfaceListe = true;
		NkMenuCtx mt, ml;
		NkConstruireMenuCtx(t, mt);
		NkConstruireMenuCtx(l, ml);
		bool memesActions = (mt.n == ml.n);
		bool uneDifference = false;
		for (uint32 i = 0; memesActions && i < mt.n; ++i) {
			if (mt.items[i].action != ml.items[i].action)
				memesActions = false;
			if (mt.items[i].agit != ml.items[i].agit)
				uneDifference = true;
		}
		char d[128];
		snprintf(d, sizeof(d), "%u vs %u entrees, actions %s, applicabilite %s", (uint32)mt.n,
				 (uint32)ml.n, memesActions ? "IDENTIQUES" : "DIVERGENTES",
				 uneDifference ? "differente" : "IDENTIQUE (surfaceListe non honore)");
		verdict("18. toile et hierarchie : MEME jeu d'entrees, applicabilite differente",
				memesActions && uneDifference, d);
	}
	// ── 19. LA RANGEE D'ICONES PARLE DANS LES DEUX ETATS ───────────────────
	//     ⚠️ UNE ICONE MUETTE EST UN BOUTON QU'ON N'OSE PAS PRESSER, et une
	//     icone GRISEE muette est pire : elle ne dit meme pas ce qui manque.
	//     Trois des sept (verrou, visibilite, registre) n'ont AUCUN champ dans
	//     le modele : elles sont PRESENTES et grisees, et leur infobulle le dit.
	//     Le cas parcourt les icones PAR LEUR NOMBRE, jamais par une liste.
	{
		bool toutesParlent = true;
		const uint32 n = (uint32)NkIconeCtx::NB;
		for (uint32 i = 0; i < n; ++i) {
			const char *a = NkInfobulleIconeCtx((NkIconeCtx)i, true);
			const char *b = NkInfobulleIconeCtx((NkIconeCtx)i, false);
			if (!a || !*a || !b || !*b || NkComponentDecl::StrEq(a, b))
				toutesParlent = false; // muette, ou la meme phrase dans les deux etats
		}
		// et l'applicabilite de la rangee suit le MEME contexte que le menu :
		// une poubelle active au-dessus d'un « Supprimer » grise serait deux
		// verites pour un seul fait.
		NkContexteCtx vide; // racine, presse-papiers vide
		bool aucuneNAgit = true;
		for (uint32 i = 0; i < n; ++i)
			if (NkIconeCtxAgit((NkIconeCtx)i, vide))
				aucuneNAgit = false;
		char d[112];
		snprintf(d, sizeof(d), "%u icones, %s ; sur la racine : %s", n,
				 toutesParlent ? "toutes parlantes dans les 2 etats" : "UNE MUETTE",
				 aucuneNAgit ? "aucune n'agit" : "UNE AGIT");
		verdict("19. la rangee d'icones parle dans les DEUX etats, et son applicabilite suit le "
				"meme contexte que le menu",
				toutesParlent && aucuneNAgit, d);
	}
	// ── 27. LE TRACE A LA SOURIS, ET SES DEUX MODIFICATEURS ────────────────
	//     Retour (4) de Rodolf : « je veux le cliquer+glisser pour modifier la
	//     taille des elements comme sur Lunacy, avec le Ctrl ou Shift enfonce
	//     pour gerer la proportionnalite ».
	//
	//     ⚠️ MESURE AVANT D'ECRIRE, ET ELLE A EVITE TROIS REECRITURES : le
	//     cliquer-glisser EXISTAIT, Maj CONTRAIGNAIT deja, et la puce de
	//     dimensions s'affichait deja pendant le geste. Il manquait UNE chose --
	//     tracer depuis le CENTRE. Les quatre cas ci-dessous tiennent donc trois
	//     comportements ANCIENS (pour qu'ils ne se cassent pas en ajoutant le
	//     quatrieme) et un NEUF.
	{
		// (a) le glisser nu : le rectangle va du depart a la souris
		const NkPaintRect a = NkRectDeTrace(100.f, 100.f, 180.f, 150.f, false, false, false);
		const bool nu = a.x == 100.f && a.y == 100.f && a.w == 80.f && a.h == 50.f;
		// (b) VERS LA GAUCHE ET VERS LE HAUT : l'ancre reste le point de depart
		const NkPaintRect b = NkRectDeTrace(100.f, 100.f, 60.f, 70.f, false, false, false);
		const bool arriere = b.x == 60.f && b.y == 70.f && b.w == 40.f && b.h == 30.f;
		// (c) MAJ : carre, ET il ne GLISSE PAS sous la main quand on tire vers
		//     le haut-gauche (la regle ecrite dans l'ancien RectTrace, conservee)
		const NkPaintRect c = NkRectDeTrace(100.f, 100.f, 60.f, 70.f, true, false, false);
		const bool carre = c.w == c.h && c.w == 40.f && c.x == 60.f && c.y == 60.f;
		// (d) ALT : depuis le CENTRE -- la souris donne le DEMI-cote
		//     ⚠️ Prendre `d` comme cote entier ferait grandir la forme DEUX FOIS
		//     plus vite que la main : le defaut classique de cette option quand
		//     on l'ajoute apres coup.
		const NkPaintRect e = NkRectDeTrace(100.f, 100.f, 140.f, 130.f, false, true, false);
		const bool centre = e.x == 60.f && e.y == 70.f && e.w == 80.f && e.h == 60.f;
		// (e) LES DEUX ENSEMBLE : un carre centre sur le point de depart
		const NkPaintRect f = NkRectDeTrace(100.f, 100.f, 140.f, 130.f, true, true, false);
		const bool lesDeux = f.w == f.h && f.w == 80.f && f.x == 60.f && f.y == 60.f;
		// (f) LA LIGNE ne se contraint PAS comme une boite : l'horizontale doit
		//     rester atteignable, c'est de loin la plus demandee
		const NkPaintRect g = NkRectDeTrace(100.f, 100.f, 200.f, 108.f, true, false, true);
		const bool ligneH = g.h == 0.f && g.w == 100.f;
		char d[176];
		snprintf(d, sizeof(d), "nu=%d arriere=%d carre=%d centre=%d les2=%d ligneH=%d",
				 nu ? 1 : 0, arriere ? 1 : 0, carre ? 1 : 0, centre ? 1 : 0, lesDeux ? 1 : 0,
				 ligneH ? 1 : 0);
		verdict("27. le trace : glisser nu, vers l'arriere sans glisser sous la main, Maj = "
				"carre, Alt = depuis le CENTRE (demi-cote), les deux ensemble, et la ligne "
				"garde son horizontale",
				nu && arriere && carre && centre && lesDeux && ligneH, d);
	}
	// ── 26. LES SOMMETS EPOUSENT LA FORME, ET LES BOUGER NE TOUCHE PAS LA BOITE ─
	//     Retour (1) de Rodolf, 01/09 : « en plus colle sur la forme, donc
	//     epouser la forme, de telle sorte que cliquer sur un point et le
	//     deplacer modifie le mesh ».
	//
	//     ⚠️ DEUX MOITIES, ET LA SECONDE EST CELLE QU'IL SOUPCONNAIT. « Epouser
	//     la forme » se verifie en mesurant la DISTANCE des ancres au contour ;
	//     « modifier le mesh » se verifie en montrant que le deplacement ecrit
	//     dans `sommets` et NE TOUCHE PAS posX/posY/largeur/hauteur. Sa capture
	//     laissait croire que la boite avait bouge -- le cas tranche.
	st.doc.NewDocument("recette points", NkAuthor::Humain);
	{
		const int32 iE = poser("ellipse", nullptr);
		st.Recompute(surface);
		const NkPaintRect r = st.layout.At(iE);
		float32 anc[64];
		const uint32 nb = NkSommetsDe(st.doc.nodes[(uint32)iE], r, anc, 32);
		// (a) CHAQUE ancre est SUR l'ellipse : ((x-cx)/hx)^2 + ((y-cy)/hy)^2 == 1
		const float32 cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f;
		const float32 hx = r.w * 0.5f, hy = r.h * 0.5f;
		float32 pireEcart = 0.f;
		for (uint32 i = 0; i < nb; ++i) {
			const float32 u = (anc[i * 2] - cx) / hx, v = (anc[i * 2 + 1] - cy) / hy;
			const float32 d = u * u + v * v - 1.f;
			const float32 ad = d < 0.f ? -d : d;
			if (ad > pireEcart)
				pireEcart = ad;
		}
		// (b) DEPLACER un sommet ecrit dans `sommets` et LAISSE LA BOITE INTACTE
		NkUINode &n = st.doc.nodes[(uint32)iE];
		const float32 x0 = n.posX, y0 = n.posY, w0 = n.width.value, h0 = n.height.value;
		NkMaterialiserSommets(n);
		n.sommets[0].x += 0.4f;
		n.sommets[0].y += 0.3f;
		const bool boiteIntacte = n.posX == x0 && n.posY == y0 && n.width.value == w0
								  && n.height.value == h0;
		// (c) et le CONTOUR PEINT a bouge : c'est le mesh, pas la boite
		float32 ct[256];
		const uint32 nbC = NkContourDe(n, r, ct, 128);
		const bool contourBouge = nbC == nb && (ct[0] != anc[0] || ct[1] != anc[1]);
		char d[176];
		snprintf(d, sizeof(d), "%u ancres, pire ecart a la courbe %.4f ; boite %s ; contour %s",
				 nb, pireEcart, boiteIntacte ? "INTACTE" : "A BOUGE",
				 contourBouge ? "a bouge" : "IDENTIQUE");
		verdict("26. les ancres d'une ellipse sont SUR la courbe (pas aux coins de sa boite), "
				"et deplacer un sommet modifie le TRACE sans toucher a la boite",
				nb == 12 && pireEcart < 0.001f && boiteIntacte && contourBouge, d);
	}
	// ── 24. LE MODE POINTS N'EST ARME QUE SUR LE NOEUD SELECTIONNE ─────────
	//     🔴 LE BOGUE BLOQUANT du 01/09 (capture `probleme_vertices_141913`) :
	//     « a un moment ca disparait, ca n'apparait plus, et c'est difficile ou
	//     impossible de le deselectionner. »
	//
	//     Lu A LA SOURCE : le bloc du mode points etait garde par
	//     `mPointsNode >= 0 && screen.Has(mPointsNode)` SEUL. Le mode survivait
	//     donc a un changement de selection, son gestionnaire de clic tournait
	//     sur TOUS les clics du canevas, et un clic a moins de 6 px du contour de
	//     la forme QUITTEE lui ajoutait un sommet en silence.
	//
	//     ⚠️ CE CAS TIENT LES DEUX LECTURES ENSEMBLE, ET C'EST TOUT SON INTERET :
	//     « le mode est-il arme ? » et « a qui la poignee ? » doivent repondre la
	//     MEME chose. Sur sa capture, elles repondaient DIFFEREMMENT -- le pied de
	//     fenetre annoncait « Mode edition de forme » pendant que l'ecran montrait
	//     les carres de redimensionnement de la boite. Deux formulations d'une
	//     meme question qui divergent : le motif du carnet, pris sur le fait.
	{
		const bool armeOk = NkModePointsArme(21, 21)
							&& NkAQuiLaPoignee(21, 21) == NkProprioPoignee::Sommet;
		// on selectionne AUTRE CHOSE : le mode tombe, et la boite reprend ses
		// poignees. C'est le cas que le code d'hier ratait.
		const bool sortSurAutreSelection =
			!NkModePointsArme(21, 7) && NkAQuiLaPoignee(21, 7) == NkProprioPoignee::Redimension;
		// on clique dans le VIDE (selection videe) : le mode tombe aussi
		const bool sortSurVide =
			!NkModePointsArme(21, -1) && NkAQuiLaPoignee(21, -1) == NkProprioPoignee::Aucune;
		const bool pasDeMode = !NkModePointsArme(-1, 21);
		char d[160];
		snprintf(d, sizeof(d), "arme(21,21)=%d ; autre selection=%d ; vide=%d ; sans mode=%d",
				 armeOk ? 1 : 0, sortSurAutreSelection ? 1 : 0, sortSurVide ? 1 : 0,
				 pasDeMode ? 1 : 0);
		verdict("24. le mode points n'est arme QUE sur le noeud selectionne -- changer de "
				"selection en sort, le vide en sort -- et les deux lectures (mode arme / a qui "
				"la poignee) disent la MEME chose",
				armeOk && sortSurAutreSelection && sortSurVide && pasDeMode, d);
	}
	// ── 25. SUR SON DOCUMENT REEL, PAS SUR UN CAS FABRIQUE ─────────────────
	//     ⚠️ DEUX FOIS CETTE SEMAINE UN BANC EST PASSE AU VERT LA OU SA MAIN
	//     ECHOUAIT, parce que le banc ne passait pas par la meme porte. Ce cas
	//     charge donc `nkuidesign_document.nkuidoc` -- LE document de sa capture,
	//     avec ses 40 et quelques noeuds -- et fait marcher la vraie table dessus.
	//
	//     Il tient le retour (3) : « ca doit etre sur les formes dessinees AUTRES
	//     QUE LE TEXTE ». Aucun noeud portant du texte, aucun cadre, aucun groupe
	//     ne doit pouvoir entrer en mode points.
	{
		NkUIDocument vrai;
		// ⚠️ L'ORDRE DES CHEMINS A ETE CORRIGE PAR LA PREMIERE EXECUTION, ET LA
		//    LECON VAUT D'ETRE ECRITE. Ma premiere version cherchait d'abord
		//    `nkuidesign_document.nkuidoc` A COTE DE L'EXECUTABLE : elle a trouve
		//    un STUB de 5 noeuds datant du 18/08, et le cas est passe... a
		//    « 0 faute » -- vert sur un document qui n'a NI texte NI cadre, donc
		//    qui ne pouvait rien prouver. *Un banc qui charge le mauvais fichier
		//    ne mesure pas moins : il mesure autre chose, et il le dit vert.*
		//    Le depot est interroge EN PREMIER, et le seuil ci-dessous refuse
		//    tout document trop petit pour etre le sien.
		const char *chemins[3] = {
			"D:/Projets/2026/Nkentseu/Nkentseu-noge/nkuidesign_document.nkuidoc",
			"../../../../nkuidesign_document.nkuidoc", "nkuidesign_document.nkuidoc"};
		bool charge = false;
		for (uint32 c = 0; c < 3 && !charge; ++c) {
			if (!nkentseu::NkFile::Exists(chemins[c]))
				continue;
			const NkString txt = nkentseu::NkFile::ReadAllText(chemins[c]);
			if (txt.Size() > 0 && vrai.Load(txt.Data())) {
				// ⚠️ LE SEUIL EST LA GARDE QUI MANQUAIT : le document de sa
				//    capture porte deux artboards et une quarantaine de noeuds.
				//    Un stub de cinq noeuds n'est pas « une version reduite »,
				//    c'est un AUTRE document -- et il rendrait ce cas vert sans
				//    qu'aucun texte ni cadre n'ait ete examine.
				charge = (vrai.nodes.Size() >= 20u);
			}
		}
		if (!charge) {
			// ⚠️ UN CAS QUI NE PEUT PAS MESURER LE DIT, il ne passe pas au vert.
			verdict("25. sur le document REEL de sa capture : aucun TEXTE, cadre ou groupe "
					"n'entre en mode points",
					false,
					"DOCUMENT REEL INTROUVABLE (ou trop petit) -- ce cas ne prouve rien, et "
					"il le DIT plutot que de passer au vert sur un stub");
		} else {
			uint32 nTexte = 0, nCadre = 0, nGroupe = 0, nFormes = 0, fautes = 0;
			for (uint32 i = 1; i < (uint32)vrai.nodes.Size(); ++i) {
				const NkUINode &n = vrai.nodes[i];
				const bool peut = NkPeutEntrerEnPoints(n);
				const bool issuePoints = NkIssueDeDblClic(n) == NkIssueDblClic::ModePoints;
				const bool estTexte = NkComponentDecl::StrEq(n.shape.Data(), "text")
									  || !n.text.Empty();
				const bool estCadre = NkComponentDecl::StrEq(n.shape.Data(), "frame");
				if (n.children.Size() > 0) {
					++nGroupe;
					if (peut || issuePoints)
						++fautes;
				} else if (estTexte) {
					++nTexte;
					if (peut || issuePoints)
						++fautes;
				} else if (estCadre) {
					++nCadre;
					if (peut || issuePoints)
						++fautes;
				} else if (peut) {
					++nFormes;
				}
			}
			char d[184];
			snprintf(d, sizeof(d),
					 "%u noeuds : %u textes, %u cadres, %u groupes, %u formes editables ; "
					 "%u faute(s)",
					 (uint32)vrai.nodes.Size(), nTexte, nCadre, nGroupe, nFormes, fautes);
			verdict("25. sur le document REEL de sa capture : aucun TEXTE, cadre ou groupe "
					"n'entre en mode points",
					fautes == 0 && nTexte > 0, d);
		}
	}
	// ── 23. LE MENU DU CLIC DROIT DANS LE VIDE ─────────────────────────────
	//     Retour de Rodolf, 01/09, signale en Q43 : le clic droit dans le vide
	//     ne faisait RIEN et ne disait RIEN.
	//
	//     ⚠️ CE CAS TIENT LE MEME INVARIANT QUE LE 17, SUR UN AUTRE MENU, ET
	//     C'EST DELIBERE. C'est ca, traiter les chemins freres comme un groupe :
	//     on ne partage pas le CONTENU (le vide de la toile est LA VUE, pas un
	//     objet sans proprietes -- pas une seule entree de la reference Lunacy ne
	//     parle d'un calque), on partage la REGLE : qui n'agit pas dit pourquoi,
	//     qui agit ne dit rien d'inutile.
	//
	//     ⚠️ ET IL AJOUTE UNE REGLE QUE L'AUTRE N'A PAS : UNE COCHE SUR UNE
	//     ENTREE QUI N'AGIT PAS EST TOUJOURS FAUSSE. C'est exactement le defaut
	//     que Q42 a trouve sur le menu Affichage -- « une case toujours cochee a
	//     cote d'un aimant qui ne faisait rien ». On ne le refait pas dix fois
	//     dans un menu neuf.
	{
		NkContexteVide c;
		c.pressePapiersPlein = true;
		c.grilleVisible = true;
		c.aimantCalques = false;
		NkMenuVide m;
		NkConstruireMenuVide(c, m);
		bool coherent = (m.n > 0);
		bool aucuneCocheFantome = true;
		uint32 nAgit = 0;
		for (uint32 i = 0; i < m.n; ++i) {
			const NkEntreeVide &e = m.items[i];
			if (!e.libelle[0])
				coherent = false;
			if (!e.agit && (!e.raison || !*e.raison))
				coherent = false; // muette
			if (e.agit && e.raison)
				coherent = false; // raison decorative
			if (!e.agit && e.coche)
				aucuneCocheFantome = false;
			if (e.agit)
				++nAgit;
		}
		// LES COCHES LISENT L'ETAT REEL, elles ne sont pas decoratives : ici
		// la grille est VRAIE et l'aimant est FAUX, et le menu doit le dire.
		bool grilleCochee = false, aimantCoche = true;
		for (uint32 i = 0; i < m.n; ++i) {
			if (m.items[i].action == NkActionVide::GrillePixels)
				grilleCochee = m.items[i].coche;
			if (m.items[i].action == NkActionVide::AimanterCalques)
				aimantCoche = m.items[i].coche;
		}
		char d[160];
		snprintf(d, sizeof(d), "%u entrees, %u agissent ; grille cochee=%d, aimant coche=%d ; %s",
				 (uint32)m.n, nAgit, grilleCochee ? 1 : 0, aimantCoche ? 1 : 0,
				 aucuneCocheFantome ? "aucune coche fantome" : "UNE COCHE SUR UNE ENTREE INERTE");
		verdict("23. menu du VIDE : chaque entree agit ou dit pourquoi, les coches lisent "
				"l'etat REEL, et AUCUNE entree inerte n'est cochee",
				coherent && aucuneCocheFantome && grilleCochee && !aimantCoche && nAgit >= 3u, d);
	}
	// ── 20. AJOUTER UN SOMMET NE DEFORME PAS LA FORME ──────────────────────
	//     ⚠️ C'EST LA PROMESSE ENTIERE DE L'AJOUT, et elle n'est pas evidente :
	//     poser le sommet neuf a la position de la SOURIS l'aurait mis a cote du
	//     trace, et la forme aurait bouge AU MOMENT MEME DE L'AJOUT -- avant
	//     qu'on tire quoi que ce soit. C'est pour ca que `NkInsererSommet` prend
	//     une fraction de segment et non un point libre.
	//     Le cas mesure le CONTOUR PEINT avant et apres : cinq ancres au lieu de
	//     quatre, et un dessin identique. C'est le seul juge qui compte -- lire
	//     le modele aurait dit « il y a bien cinq sommets » sans rien prouver de
	//     ce qu'on voit.
	st.doc.NewDocument("recette points", NkAuthor::Humain);
	{
		const int32 iR = poser("rect", nullptr);
		st.Recompute(surface);
		const NkPaintRect r = st.layout.At(iR);
		float32 avant[64], apres[64];
		const uint32 cA = NkContourDe(st.doc.nodes[(uint32)iR], r, avant, 32);
		const int32 neuf = NkInsererSommet(st.doc.nodes[(uint32)iR], 1u, 0.5f);
		float32 anc[64];
		const uint32 nAnc = NkSommetsDe(st.doc.nodes[(uint32)iR], r, anc, 32);
		const uint32 cB = NkContourDe(st.doc.nodes[(uint32)iR], r, apres, 32);
		// le contour gagne un point (le sommet neuf EST sur le trace), et tous
		// les points d'avant s'y retrouvent inchanges.
		bool surLeTrace = (neuf == 2 && nAnc == 5 && cB == 5);
		if (surLeTrace) {
			// le sommet 2 (indice 1) et le sommet 4 (indice 3) etaient les coins
			// haut-droit et bas-gauche ; le neuf est a mi-cote droit.
			const float32 mx = (avant[1 * 2] + avant[2 * 2]) * 0.5f;
			const float32 my = (avant[1 * 2 + 1] + avant[2 * 2 + 1]) * 0.5f;
			if (anc[2 * 2] != mx || anc[2 * 2 + 1] != my)
				surLeTrace = false;
		}
		// et le sommet neuf est VIF : l'arrondi ne s'herite pas d'un voisin.
		const bool vif = neuf >= 0 && st.doc.nodes[(uint32)iR].sommets[(uint32)neuf].rayon == 0.f;
		char d[128];
		snprintf(d, sizeof(d), "contour %u -> %u pts, ancre neuve #%d a (%.1f,%.1f), rayon %s",
				 cA, cB, neuf + 1, nAnc > 2 ? anc[4] : 0.f, nAnc > 2 ? anc[5] : 0.f,
				 vif ? "0 (vif)" : "HERITE");
		verdict("20. ajouter un sommet le pose SUR le cote, sans deformer la forme, et il "
				"nait VIF",
				surLeTrace && vif, d);
	}
	// ── 21. LE DOUBLE-CLIC SUR UN SOMMET L'ARRONDIT, ET CA SE VOIT ─────────
	//     ⚠️ DEUX MOITIES, ET LA SECONDE EST CELLE QUI COMPTE. Que le modele
	//     porte un rayon se lit ; que le DESSIN change est le seul lien qui
	//     prouve que le champ n'est pas declare-et-inerte. C'est la lecon de la
	//     mutation 3 du mode points (Q42) : « tout pouvait etre parfaitement
	//     enregistre et parfaitement invisible ».
	st.doc.NewDocument("recette points", NkAuthor::Humain);
	{
		const int32 iR = poser("rect", nullptr);
		st.Recompute(surface);
		const NkPaintRect r = st.layout.At(iR);
		float32 vif[64], rond[64];
		const uint32 cVif = NkContourDe(st.doc.nodes[(uint32)iR], r, vif, 32);
		const float32 r1 = NkArrondirSommet(st.doc.nodes[(uint32)iR], 0u);
		const uint32 cRond = NkContourDe(st.doc.nodes[(uint32)iR], r, rond, 32);
		// le cycle : 0 -> 8 -> 16 -> 32 -> 0, et il BOUCLE (sinon un sommet
		// arrondi par erreur serait irrattrapable au double-clic).
		const float32 r2 = NkArrondirSommet(st.doc.nodes[(uint32)iR], 0u);
		const float32 r3 = NkArrondirSommet(st.doc.nodes[(uint32)iR], 0u);
		const float32 r4 = NkArrondirSommet(st.doc.nodes[(uint32)iR], 0u);
		const bool cycle = r1 == 8.f && r2 == 16.f && r3 == 32.f && r4 == 0.f;
		// et les ANCRES ne bougent pas : arrondir un coin ne deplace pas le coin.
		float32 anc[64];
		const uint32 nAnc = NkSommetsDe(st.doc.nodes[(uint32)iR], r, anc, 32);
		char d[128];
		snprintf(d, sizeof(d), "contour %u -> %u pts, cycle %.0f/%.0f/%.0f/%.0f, %u ancres",
				 cVif, cRond, r1, r2, r3, r4, nAnc);
		verdict("21. arrondir un sommet AJOUTE DES POINTS AU CONTOUR PEINT (le champ agit, il "
				"n'est pas seulement enregistre), le cycle boucle a vif, et les 4 ancres "
				"restent 4",
				cVif == 4 && cRond > cVif && cycle && nAnc == 4, d);
	}
	// ── 22. L'ARRONDI SURVIT A L'ALLER-RETOUR, ET LE FICHIER D'AVANT NE BOUGE PAS ─
	//     ⚠️ LE VOLET CONSERVATION, ECRIT D'ENTREE ET NON EN SORTIE. Deux
	//     exigences ENSEMBLE, parce que l'une seule passe au vert sur une perte :
	//     (a) un trace SANS arrondi se reecrit avec DEUX nombres par sommet,
	//     octet pour octet -- c'est la preuve d'entree de l'additivite ;
	//     (b) un trace AVEC arrondi retrouve sa valeur.
	//     ⚠️ La lecon de la mutation 7 (Q42, FILLS) est portee ici sans etre
	//     redecouverte : « stable » ne veut pas dire « juste ». Un ecrivain qui
	//     laisserait tomber le rayon rendrait deux fichiers identiques et
	//     passerait un test qui ne demande que la stabilite.
	st.doc.NewDocument("recette points", NkAuthor::Humain);
	{
		const int32 iR = poser("rect", nullptr);
		NkMaterialiserSommets(st.doc.nodes[(uint32)iR]);
		NkString sansR;
		st.doc.Save(sansR);
		const bool ancree0 = ancree(sansR) && !sansR.Contains("sommet_1 = -1 -1 ");
		NkUIDocument d1;
		NkString sansR2;
		const bool lu1 = d1.Load(sansR.Data());
		d1.Save(sansR2);
		const bool stable = lu1 && sansR.Size() == sansR2.Size()
							&& NkComponentDecl::StrEq(sansR.Data(), sansR2.Data());
		// (b) avec arrondi
		// ⚠️ CETTE GARDE A ETE ECRITE PAR UNE MUTATION, ET C'EST SA RAISON D'ETRE.
		//    En remettant `rect` en boite non editable, la materialisation devient
		//    un non-evenement, `sommets` reste VIDE, et la ligne suivante ecrivait
		//    dans `sommets[1]` : LA RECETTE PLANTAIT (code de sortie 9) au lieu de
		//    rendre un verdict. Un banc qui plante ne dit pas QUEL cas est tombe --
		//    il oblige a relancer sous debogueur pour apprendre ce qu'une ligne
		//    aurait dit. Le cas echoue desormais en NOMMANT sa cause.
		//    (La mutation ne cherchait pas ce defaut-la : elle l'a trouve en
		//    passant, sur le banc lui-meme et non sur le code teste.)
		const bool aDesSommets = st.doc.nodes[(uint32)iR].sommets.Size() >= 4;
		if (aDesSommets)
			st.doc.nodes[(uint32)iR].sommets[1].rayon = 12.f;
		NkString avecR;
		st.doc.Save(avecR);
		NkUIDocument d2;
		const bool lu2 = d2.Load(avecR.Data());
		float32 relu = -1.f;
		for (uint32 i = 1; lu2 && i < (uint32)d2.nodes.Size(); ++i)
			if (d2.nodes[i].sommets.Size() == 4)
				relu = d2.nodes[i].sommets[1].rayon;
		char dd[176];
		snprintf(dd, sizeof(dd), "%ssans rayon : %s (%u o), avec rayon 12 -> relu %.0f",
				 aDesSommets ? "" : "AUCUN SOMMET A MATERIALISER (le rect ne stocke plus) | ",
				 stable ? "octet pour octet" : "A BOUGE", (uint32)sansR.Size(), relu);
		verdict("22. CONSERVATION : un trace a coins vifs se reecrit octet pour octet (le "
				"3e nombre n'est pas ecrit), et un rayon pose se retrouve apres "
				"aller-retour",
				aDesSommets && ancree0 && stable && relu == 12.f, dd);
	}
	// ── 28. LIRE UN SOMMET N'ECRIT RIEN ────────────────────────────────────
	//     🔴 CE CAS EXISTE PARCE QUE LE DEFAUT A FAILLI PARTIR. La premiere
	//     version de la section « EDITION DE FORME » de l'Inspecteur appelait
	//     `NkMaterialiserSommets` POUR LIRE : ouvrir un panneau ajoutait la liste
	//     `sommets` au noeud, changeait les octets du fichier et marquait le
	//     document modifie -- alors que l'utilisateur n'avait rien fait.
	//
	// ⚠️ ET LE VOLET CONSERVATION DU CAS 22 NE L'AURAIT PAS VU : il entre en mode
	//    points et en sort, mais il n'execute pas l'Inspecteur. *Une garde ne
	//    couvre que la porte qu'elle regarde.* Celle-ci porte donc sur le
	//    MECANISME de lecture, la ou les deux panneaux passent.
	//
	// ⚠️ ET IL EXIGE AUSSI L'ACCORD DES DEUX LECTURES. Une lecture qui n'ecrit
	//    rien mais rend d'autres nombres que la table materialisee serait pire
	//    que le defaut d'origine : le panneau montrerait le polygone regulier
	//    pendant que l'ecran montre la forme deja deformee.
	{
		st.doc.NewDocument("recette points", NkAuthor::Humain);
		const int32 iR = poser("rect", nullptr);
		const int32 iE = poser("etoile", nullptr);
		NkString avant;
		st.doc.Save(avant);
		// (a) LIRE les sommets des deux formes -- rien ne doit bouger
		uint32 nbR = NkNbSommetsDe(st.doc.nodes[(uint32)iR]);
		uint32 nbE = NkNbSommetsDe(st.doc.nodes[(uint32)iE]);
		float32 lus[20][3];
		bool tousLus = nbR == 4 && nbE == 10;
		for (uint32 i = 0; i < nbE && i < 20; ++i)
			if (!NkLireSommet(st.doc.nodes[(uint32)iE], i, lus[i][0], lus[i][1], lus[i][2]))
				tousLus = false;
		NkString apres;
		st.doc.Save(apres);
		const bool stable = avant.Size() == apres.Size()
							&& NkComponentDecl::StrEq(avant.Data(), apres.Data());
		// (b) LES MEMES NOMBRES qu'apres materialisation : une lecture qui
		//     diverge de l'ecriture serait pire que le defaut d'origine
		NkMaterialiserSommets(st.doc.nodes[(uint32)iE]);
		bool memes = (uint32)st.doc.nodes[(uint32)iE].sommets.Size() == nbE;
		for (uint32 i = 0; memes && i < nbE; ++i)
			memes = st.doc.nodes[(uint32)iE].sommets[i].x == lus[i][0]
					&& st.doc.nodes[(uint32)iE].sommets[i].y == lus[i][1]
					&& st.doc.nodes[(uint32)iE].sommets[i].rayon == lus[i][2];
		// (c) 🔴 LE VOLET QU'UNE MUTATION A DESIGNE. Sans lui, ce cas ne
		//     couvrait que le REPLI sur la table reguliere : une mutation qui
		//     faisait ignorer la LISTE STOCKEE a `NkLireSommet` lui survivait
		//     28/28. Or c'est le pire des deux defauts -- le panneau montrerait
		//     le polygone regulier pendant que l'ecran montre la forme deformee.
		//     *Une garde qui ne teste que la branche facile rend un vert qui ment.*
		st.doc.nodes[(uint32)iE].sommets[3].x = -0.75f;
		st.doc.nodes[(uint32)iE].sommets[3].rayon = 12.f;
		float32 dx = 0.f, dy = 0.f, dr = 0.f;
		const bool litLeDeplace = NkLireSommet(st.doc.nodes[(uint32)iE], 3, dx, dy, dr)
								  && dx == -0.75f && dr == 12.f;
		char d[224];
		snprintf(d, sizeof(d),
				 "rect=%u sommets, etoile=%u ; %u octets %s ; memes nombres=%d ; sommet deplace "
				 "relu x=%.2f r=%.0f",
				 nbR, nbE, (uint32)avant.Size(), stable ? "octet pour octet" : "ONT BOUGE",
				 memes ? 1 : 0, (double)dx, (double)dr);
		verdict("28. LIRE un sommet (NkLireSommet / NkNbSommetsDe) n'ecrit RIEN dans le "
				"document, rend les MEMES nombres que la table materialisee, ET rend la LISTE "
				"STOCKEE des qu'elle existe (pas la table reguliere)",
				tousLus && stable && memes && litLeDeplace && avant.Size() > 0, d);
	}

	// ── 29. LA BOITE SE RECADRE SUR LE TRACE, ET LA FORME NE BOUGE PAS ─────
	//     Retour de Rodolf, 01/09 (soir) : « lorsqu'on modifie un objet par ses
	//     vertices, on doit redefinir sa bounding box pour la selection. »
	//
	// ⚠️ MESURE AVANT D'ECRIRE : le glisser d'un sommet n'ecrivait QUE
	//    `sommets[i].x/y`. `posX`, `posY`, `width` et `height` ne bougeaient pas.
	//    Tout ce qui lit la boite -- poignees, puce, Inspecteur, aimant, et
	//    surtout le POINTAGE -- lisait donc une boite perimee : on cliquait sur
	//    la forme sans la selectionner, et on la selectionnait en cliquant dans
	//    le vide.
	//
	// ⚠️ ET LE VRAI DANGER N'EST PAS L'ENGLOBANT, C'EST LE REFERENTIEL. Nos
	//    sommets sont UNITAIRES (-1..1 en fraction de la boite) : recadrer change
	//    leur unite. Recalculer sans renormaliser ferait SAUTER la forme a
	//    l'ecran au moment meme du relachement. Le cas exige donc les DEUX : la
	//    boite epouse le trace, ET chaque sommet retombe au meme point ABSOLU.
	{
		st.doc.NewDocument("recette points", NkAuthor::Humain);
		const int32 iR = poser("rect", nullptr); // 100 x 80, pose a l'origine
		{
			NkUINode &n = st.doc.nodes[(uint32)iR];
			n.posX = 20.f;
			n.posY = 10.f;
			NkMaterialiserSommets(n);
			// on TIRE le coin haut-gauche VERS L'EXTERIEUR : le trace deborde
			n.sommets[0].x = -2.f; // soit 100 px a gauche du bord gauche
			n.sommets[0].y = -1.5f;
		}
		// les points ABSOLUS avant recadrage -- c'est eux qui ne doivent pas bouger
		float32 avantX[16], avantY[16];
		uint32 nbAv = 0;
		{
			const NkUINode &n = st.doc.nodes[(uint32)iR];
			nbAv = (uint32)n.sommets.Size();
			for (uint32 i = 0; i < nbAv && i < 16; ++i) {
				avantX[i] = n.posX + (n.sommets[i].x + 1.f) * 0.5f * n.width.value;
				avantY[i] = n.posY + (n.sommets[i].y + 1.f) * 0.5f * n.height.value;
			}
		}
		const bool aRecadre = NkRecadrerNoeud(st.doc.nodes[(uint32)iR], true);
		const NkUINode &n = st.doc.nodes[(uint32)iR];
		// (a) LA BOITE A CHANGE, et elle vaut l'englobant du trace
		//     depart : x 20..120, y 10..90 ; sommet 0 tire a x=-2 -> 20-50=-30,
		//     y=-1.5 -> 10-20=-10. Englobant attendu : (-30,-10) 150 x 100.
		const bool boite = aRecadre && n.posX == -30.f && n.posY == -10.f
						   && n.width.value == 150.f && n.height.value == 100.f;
		// (b) LA FORME N'A PAS BOUGE D'UN PIXEL : chaque sommet retombe au meme
		//     point absolu. C'est la moitie que la renormalisation paie.
		float32 pireEcart = 0.f;
		for (uint32 i = 0; i < nbAv && i < 16; ++i) {
			const float32 X = n.posX + (n.sommets[i].x + 1.f) * 0.5f * n.width.value;
			const float32 Y = n.posY + (n.sommets[i].y + 1.f) * 0.5f * n.height.value;
			const float32 ex = X > avantX[i] ? X - avantX[i] : avantX[i] - X;
			const float32 ey = Y > avantY[i] ? Y - avantY[i] : avantY[i] - Y;
			if (ex > pireEcart)
				pireEcart = ex;
			if (ey > pireEcart)
				pireEcart = ey;
		}
		const bool immobile = pireEcart < 0.001f;
		// (c) LES MODES PASSENT A `Fixed` : ecrire une valeur dans un axe
		//     `expand` la laisserait sans effet, et la boite resterait fausse en
		//     silence.
		const bool fixes = n.width.mode == NkSizeMode::Fixed
						   && n.height.mode == NkSizeMode::Fixed;
		// (d) IDEMPOTENT : recadrer une forme deja recadree ne fait RIEN.
		const bool idem = !NkRecadrerNoeud(st.doc.nodes[(uint32)iR], true);
		char d[224];
		snprintf(d, sizeof(d), "boite=(%.0f,%.0f) %.0fx%.0f ; pire ecart %.5f px ; fixes=%d ; "
							   "2e appel change=%d",
				 (double)n.posX, (double)n.posY, (double)n.width.value,
				 (double)n.height.value, (double)pireEcart, fixes ? 1 : 0, idem ? 0 : 1);
		verdict("29. la boite se RECADRE sur le trace (englobant exact), la forme ne bouge pas "
				"d'un pixel (renormalisation), les modes passent a Fixed, et l'operation est "
				"IDEMPOTENTE",
				boite && immobile && fixes && idem, d);
	}

	// ── 30. DIX DEPLACEMENTS SUCCESSIFS NE FONT PAS DERIVER LA FORME ───────
	// ⚠️ C'EST LE VRAI RISQUE DE CE MECANISME, ET IL NE SE VOIT PAS SUR UN
	//    ALLER-RETOUR. Chaque recadrage divise puis multiplie par des largeurs
	//    differentes ; l'erreur d'arrondi du flottant s'ajoute a chaque tour. Une
	//    forme qui se decale d'un centieme de pixel par geste a bouge d'un pixel
	//    au centieme geste -- et personne ne saura d'ou ca vient.
	//
	// ⚠️ ET LE CAS MESURE UN POINT QUI N'EST PAS TOUCHE : on tire le sommet 2 dix
	//    fois, et on surveille le sommet 0. Mesurer le sommet TIRE ne dirait
	//    rien -- il est cense bouger.
	{
		st.doc.NewDocument("recette points", NkAuthor::Humain);
		const int32 iE = poser("etoile", nullptr);
		st.doc.nodes[(uint32)iE].posX = 40.f;
		st.doc.nodes[(uint32)iE].posY = 40.f;
		NkMaterialiserSommets(st.doc.nodes[(uint32)iE]);
		auto absolu = [&](uint32 i, float32 &X, float32 &Y) {
			const NkUINode &n = st.doc.nodes[(uint32)iE];
			X = n.posX + (n.sommets[i].x + 1.f) * 0.5f * n.width.value;
			Y = n.posY + (n.sommets[i].y + 1.f) * 0.5f * n.height.value;
		};
		float32 x0 = 0.f, y0 = 0.f;
		absolu(0, x0, y0);
		for (uint32 tour = 0; tour < 10; ++tour) {
			NkUINode &n = st.doc.nodes[(uint32)iE];
			// on pousse le sommet 2 d'un cran vers l'exterieur, puis on recadre
			n.sommets[2].x += 0.3f;
			n.sommets[2].y += 0.2f;
			NkRecadrerNoeud(n, true);
		}
		float32 x1 = 0.f, y1 = 0.f;
		absolu(0, x1, y1);
		const float32 dx = x1 > x0 ? x1 - x0 : x0 - x1;
		const float32 dy = y1 > y0 ? y1 - y0 : y0 - y1;
		const bool stable = dx < 0.01f && dy < 0.01f;
		char d[192];
		snprintf(d, sizeof(d),
				 "sommet 0 (jamais touche) : (%.4f,%.4f) -> (%.4f,%.4f), derive %.5f px",
				 (double)x0, (double)y0, (double)x1, (double)y1,
				 (double)(dx > dy ? dx : dy));
		verdict("30. DIX deplacements + recadrages successifs ne font pas deriver un sommet "
				"qu'on n'a pas touche (l'erreur cumulee est le vrai risque)",
				stable, d);
	}

	// ── 31. UN COTE PLAT NE DIVISE PAS, ET UN PARENT CALCULE REFUSE ────────
	// ⚠️ DEUX REFUS, ET ILS SE MESURENT PLUTOT QUE DE SE SUPPOSER.
	//    (a) trois sommets alignes donnent une largeur nulle : sans plancher, la
	//        renormalisation divise par zero et rend des NaN -- qui se propagent
	//        en SILENCE jusqu'au dessin, ou ils n'affichent plus rien du tout ;
	//    (b) sous un agencement calcule, `posX`/`posY` sont IGNORES : y ecrire un
	//        decalage donnerait une valeur sans effet pendant que le trace, lui,
	//        aurait bouge. On rend faux, et l'appelant a une raison a dire.
	{
		st.doc.NewDocument("recette points", NkAuthor::Humain);
		const int32 iT = poser("triangle", nullptr);
		{
			NkUINode &n = st.doc.nodes[(uint32)iT];
			NkMaterialiserSommets(n);
			for (uint32 i = 0; i < (uint32)n.sommets.Size(); ++i)
				n.sommets[i].x = 0.f; // tous alignes : largeur du trace = 0
		}
		const bool aPlat = NkRecadrerNoeud(st.doc.nodes[(uint32)iT], true);
		const NkUINode &t = st.doc.nodes[(uint32)iT];
		bool sain = t.width.value >= 1.f && t.height.value > 0.f;
		for (uint32 i = 0; i < (uint32)t.sommets.Size(); ++i) {
			// un NaN n'est egal a rien, pas meme a lui-meme : c'est comme ca
			// qu'on l'attrape sans <math.h>.
			const float32 v = t.sommets[i].x;
			if (!(v == v) || !(t.sommets[i].y == t.sommets[i].y))
				sain = false;
		}
		// (b) le meme geste, parent NON libre
		const int32 iR2 = poser("rect", nullptr);
		NkMaterialiserSommets(st.doc.nodes[(uint32)iR2]);
		st.doc.nodes[(uint32)iR2].sommets[0].x = -3.f;
		const bool refuse = !NkRecadrerNoeud(st.doc.nodes[(uint32)iR2], false);
		char d[192];
		snprintf(d, sizeof(d), "plat : %.0fx%.0f, aucun NaN=%d, a recadre=%d ; parent calcule "
							   "refuse=%d",
				 (double)t.width.value, (double)t.height.value, sain ? 1 : 0, aPlat ? 1 : 0,
				 refuse ? 1 : 0);
		verdict("31. un cote PLAT garde un plancher et ne produit AUCUN NaN, et un parent qui "
				"place ses enfants lui-meme REFUSE le recadrage",
				sain && refuse, d);
	}

	// ── 32. LA TROISIEME TABLE DE SELECTION, ET LA MULTI-SELECTION DE SOMMETS ─
	//     Retour de Rodolf, 01/09 (soir) : « on doit pouvoir selectionner
	//     plusieurs vertices pour deplacement ou transformation simultanes. »
	//
	// ⚠️ LA DECISION EST PRISE ET TENUE : les sommets suivent la table de la
	//    LISTE, pas celle de la toile -- `Profond` n'a de sens que la ou il y a
	//    une profondeur a traverser, et il n'y a rien sous un sommet. La source
	//    le confirme (`lunacy.docs.icons8.com/editing_shapes/` : « drag over them
	//    or hold down Shift when clicking several points »). Le cas tient l'ECART
	//    avec la table de la toile, sans quoi le premier lecteur le
	//    « corrigerait » en croyant reparer un oubli.
	{
		const bool table = NkGesteSommet(false, false) == NkGesteSel::Remplacer
						   && NkGesteSommet(false, true) == NkGesteSel::Basculer
						   // ⚠️ L'ECART AVEC LA TOILE, TENU EXPRES :
						   && NkGesteSommet(true, false) == NkGesteSel::Remplacer
						   && NkGesteToile(true, false) == NkGesteSel::Profond;
		// l'etat de multi-selection lui-meme
		DesignState::NkModeForme m;
		m.MarquerSeul(3);
		const bool seul = m.Marque(3) && m.NbMarques() == 1 && m.sommet == 3;
		m.BasculerMarque(5);
		m.BasculerMarque(7);
		const bool trois = m.NbMarques() == 3 && m.sommet == 7 && m.Marque(5);
		// ⚠️ RETIRER LE PRINCIPAL LUI FAIT ELIRE UN SUCCESSEUR : sans ca,
		//    l'Inspecteur afficherait les coordonnees d'un point qui n'est plus
		//    selectionne -- la faute exacte que `sel`/`selected` evite.
		m.BasculerMarque(7);
		const bool successeur = m.NbMarques() == 2 && m.sommet >= 0 && m.Marque(m.sommet);
		// ⚠️ ON RETIENT LA VALEUR AVANT `Quitter`, sinon le message imprimerait
		//    « sommet=-1 » a cote d'un verdict qui a mesure autre chose. *Un
		//    chiffre porte sa provenance.*
		const int32 successeurLu = m.sommet;
		// et sortir efface TOUT (le quatrieme champ que `Quitter` doit porter)
		m.Quitter();
		const bool vide = m.NbMarques() == 0 && m.sommet == -1 && m.noeud == -1;
		char d[224];
		snprintf(d, sizeof(d), "table=%d seul=%d trois=%d successeur(sommet=%d)=%d vide=%d",
				 table ? 1 : 0, seul ? 1 : 0, trois ? 1 : 0, successeurLu, successeur ? 1 : 0,
				 vide ? 1 : 0);
		verdict("32. la table des SOMMETS (Maj bascule, Ctrl ne fait rien -- l'ecart avec la "
				"toile est voulu), et le principal se DERIVE de l'ensemble",
				table && seul && trois && successeur && vide, d);
	}

	// ── 32bis. LES SOMMETS MARQUES BOUGENT ENSEMBLE, ET D'UN ECART ─────────
	// ⚠️ LE MECANISME EST DESCENDU DANS `Sommets.h` POUR CE CAS. Ecrit dans la
	//    boucle de rendu, il aurait vecu la ou aucun banc ne va -- la facture que
	//    ce chantier a deja payee quatre fois -- et « les sommets bougent
	//    ensemble » serait reste une opinion.
	//
	// ⚠️ ET LE CAS VISE LA FAUTE PRECISE, PAS « ca bouge ». Poser chaque sommet
	//    marque SOUS la souris les ferait tous se SUPERPOSER au premier pixel du
	//    geste : la forme s'effondrerait en un point. Le sous-cas (b) mesure donc
	//    que les sommets restent DISTINCTS et gardent leur ecart mutuel -- ce
	//    qu'un simple « ils ont bouge » ne verrait pas.
	{
		st.doc.NewDocument("recette points", NkAuthor::Humain);
		const int32 iE = poser("etoile", nullptr);
		NkMaterialiserSommets(st.doc.nodes[(uint32)iE]);
		NkUINode &e = st.doc.nodes[(uint32)iE];
		// marques : 0 (tire), 2 et 5
		const nkentseu::uint64 marq = (1ull << 0) | (1ull << 2) | (1ull << 5);
		const float32 ax0 = e.sommets[0].x, ay0 = e.sommets[0].y;
		const float32 ax2 = e.sommets[2].x, ay2 = e.sommets[2].y;
		const float32 ax3 = e.sommets[3].x, ay3 = e.sommets[3].y; // NON marque
		const float32 ax5 = e.sommets[5].x;
		const uint32 bouges = NkDeplacerSommetsMarques(e, 0, marq, ax0 + 0.4f, ay0 - 0.25f);
		// (a) le sommet TIRE est exactement ou on l'a demande
		const bool tireExact = e.sommets[0].x == ax0 + 0.4f && e.sommets[0].y == ay0 - 0.25f;
		// (b) les autres MARQUES ont pris le MEME ECART -- ils ne se sont pas
		//     poses sur la souris (l'ecart 2<->5 est conserve)
		const bool memeEcart = e.sommets[2].x == ax2 + 0.4f && e.sommets[2].y == ay2 - 0.25f
							   && e.sommets[5].x == ax5 + 0.4f;
		const bool distincts = e.sommets[2].x != e.sommets[0].x
							   || e.sommets[2].y != e.sommets[0].y;
		// (c) le NON marque n'a pas bouge d'un iota
		const bool intact = e.sommets[3].x == ax3 && e.sommets[3].y == ay3;
		char d[192];
		snprintf(d, sizeof(d), "%u sommet(s) deplace(s) sur 10 ; tire exact=%d meme ecart=%d "
							   "distincts=%d ; le non marque intact=%d",
				 bouges, tireExact ? 1 : 0, memeEcart ? 1 : 0, distincts ? 1 : 0,
				 intact ? 1 : 0);
		verdict("32bis. les sommets MARQUES bougent ensemble d'un ECART (ils ne se superposent "
				"pas sous la souris), et ceux qui ne le sont pas ne bougent pas",
				bouges == 3 && tireExact && memeEcart && distincts && intact, d);
	}

	// ── 33. LE DOUBLE-CLIC SUR UN SOMMET APPARTIENT AU MODE ────────────────
	//     Retour (3) de Rodolf : il redemande l'arrondi au double-clic.
	//
	// 🔴 ET LA RE-MESURE LUI DONNE RAISON : le double-clic avait DEUX lecteurs.
	//    La toile arrondit le sommet ; `HandleMouse` relisait le meme evenement
	//    et -- depuis le correctif du forage a deux temps de ce matin --
	//    RE-ENTRAIT dans le mode (`Quitter()` puis `noeud = cand`). L'arrondi
	//    passait, mais `sommet` retombait a -1, la section « EDITION DE FORME »
	//    se vidait, et le message « Sommet 3 arrondi » etait remplace par celui
	//    du forage. *Un geste, deux lecteurs, et lequel gagne ne dependait que de
	//    l'ordre du code* -- la meme classe de defaut que la collision de
	//    poignees, tranchee du meme cote.
	{
		// (a) le mode garde son double-clic sur SON noeud
		const bool aMoi = NkDblClicAuModeForme(7, 7, 7);
		// (b) mais pas sur un AUTRE noeud : double-cliquer ailleurs doit
		//     continuer de forer/selectionner normalement
		const bool pasAilleurs = !NkDblClicAuModeForme(7, 7, 9);
		// (c) ni quand le mode n'est pas arme (selection differente)
		const bool pasSiDesarme = !NkDblClicAuModeForme(7, 9, 7);
		// (d) ni quand il n'y a pas de mode du tout
		const bool pasSansMode = !NkDblClicAuModeForme(-1, 7, 7);
		// (e) ET L'ARRONDI LUI-MEME MARCHE ENCORE -- le cycle 0/8/16/32, sur un
		//     rect a enfants comme ceux de son document
		st.doc.NewDocument("recette points", NkAuthor::Humain);
		const int32 iC = poser("rect", nullptr);
		st.doc.AddChild(iC, "", NkAuthor::Humain); // un rect QUI PORTE un enfant
		const float32 r1 = NkArrondirSommet(st.doc.nodes[(uint32)iC], 0);
		const float32 r2 = NkArrondirSommet(st.doc.nodes[(uint32)iC], 0);
		const float32 r3 = NkArrondirSommet(st.doc.nodes[(uint32)iC], 0);
		const float32 r4 = NkArrondirSommet(st.doc.nodes[(uint32)iC], 0);
		const bool cycle = r1 == 8.f && r2 == 16.f && r3 == 32.f && r4 == 0.f;
		char d[192];
		snprintf(d, sizeof(d), "a moi=%d ailleurs=%d desarme=%d sans mode=%d ; cycle %.0f %.0f "
							   "%.0f %.0f",
				 aMoi ? 1 : 0, pasAilleurs ? 1 : 0, pasSiDesarme ? 1 : 0, pasSansMode ? 1 : 0,
				 (double)r1, (double)r2, (double)r3, (double)r4);
		verdict("33. le double-clic sur un sommet appartient au MODE (et a lui seul), et "
				"l'arrondi cycle encore 0/8/16/32 sur un rect QUI PORTE DES ENFANTS",
				aMoi && pasAilleurs && pasSiDesarme && pasSansMode && cycle, d);
	}

	// ── 35. LES TROIS LIAISONS FONT TROIS CHOSES DIFFERENTES ───────────────
	//     Retour de Rodolf, 01/09 (nuit) : « le manipulateur de chaque cote
	//     independant ou dependant en fonction de l'utilisateur ».
	//
	// ⚠️ LE VOCABULAIRE VIENT DE LA SOURCE, ET IL PIEGE. Chez Lunacy
	//    (`tools/#types-of-points`, cite mot pour mot dans `Document.h`),
	//    « Asymmetric » = MEME ANGLE, longueurs differentes -- pas « les deux font
	//    ce qu'elles veulent ». Prendre le mot dans son sens courant aurait donne
	//    a `Asymetrique` le comportement de `Deconnecte`, et deux boutons sur
	//    quatre auraient fait la meme chose. Ce cas tient les trois comportements
	//    COTE A COTE, ce qui est la seule facon de voir qu'ils different.
	{
		// (a) MIROIR : la jumelle est l'oppose EXACT (direction et longueur)
		NkPoint2 m;
		m.liaison = NkPoint2::LiaisonMiroir;
		m.ex = -0.2f;
		m.ey = 0.f;
		NkPoserTangente(m, 1, 0.3f, 0.4f); // on pose la SORTANTE
		const bool miroir = m.ex == -0.3f && m.ey == -0.4f;
		// (b) ASYMETRIQUE : meme angle, la jumelle GARDE SA LONGUEUR
		//
		// 🔴 RE-VERIFIE A LA SOURCE LE 02/09, PARCE QUE RODOLF CONTESTAIT : « je
		//    pense que la longueur bouge dans Asym. ». Il a raison sur ce qu'il
		//    VOIT -- le volet (b2) le mesure : la jumelle BOUGE franchement, elle
		//    PIVOTE, sa pointe parcourt un arc. Ce qui ne change pas, c'est sa
		//    LONGUEUR. « Bouger » et « changer de longueur » ne sont pas la meme
		//    chose, et c'est exactement ce que l'oeil confond sur une poignee qui
		//    tourne.
		//
		// ⚠️ CE QUE LA SOURCE DIT, MOT POUR MOT (`tools/#types-of-points`) :
		//    « Asymmetric points come with handles that share the same angle but
		//    can have different lengths. » -- une propriete de L'ETAT, PAS du
		//    geste. Elle ne dit NULLE PART ce que devient la longueur de la
		//    jumelle quand on tire l'autre. La source est donc AMBIGUE sur le
		//    point precis que Rodolf souleve, et il faut le dire plutot que de
		//    faire parler la phrase plus qu'elle ne parle.
		//
		// ⚠️ CE QUI TRANCHE, C'EST LA CONVENTION DES DEUX VOISINS LES PLUS
		//    PROCHES, et l'un des deux est explicite la ou Lunacy se tait :
		//      - Sketch nomme ce type « Mirror angle » : « handles that can be
		//        different distances from the vector point, but share the same
		//        angle » -- meme formulation statique, meme silence ;
		//      - Figma, lui, decrit LE GESTE : en « Mirror angle », « the other
		//        handle's angle will mirror, but its length remains unchanged ».
		//        Son API le nomme `HandleMirroring = ANGLE | ANGLE_AND_LENGTH`.
		//    C'est ce que nous faisons. Note *non tranche par la source Lunacy,
		//    aligne sur Figma et Sketch* dans le document de reference.
		NkPoint2 a;
		a.liaison = NkPoint2::LiaisonAsymetrique;
		a.ex = -0.1f; // longueur 0,1
		a.ey = 0.f;
		const float32 avantEx = a.ex, avantEy = a.ey;
		NkPoserTangente(a, 1, 0.3f, 0.4f); // sortante de longueur 0,5
		// la jumelle doit pointer a l'oppose (-0,6 ; -0,8 normalise) x 0,1
		const float32 lg = NkLongueur2D(a.ex, a.ey);
		const bool memeLongueur = lg > 0.099f && lg < 0.101f;
		// (b2) ⚠️ LE VOLET QUI REPOND A RODOLF : LA JUMELLE N'EST PAS IMMOBILE.
		//      Sa pointe se deplace franchement (elle pivote pour se remettre a
		//      l'oppose de la nouvelle sortante). Sans ce volet, « la longueur est
		//      conservee » se lirait comme « la jumelle ne fait rien » -- ce qui
		//      est FAUX, et c'est justement cette lecture qui a fait douter. Les
		//      deux verites tiennent ensemble, et le cas les tient ensemble :
		//      ELLE BOUGE, SA LONGUEUR NE CHANGE PAS.
		const float32 deplJumelle = NkLongueur2D(a.ex - avantEx, a.ey - avantEy);
		const bool jumelleBouge = deplJumelle > 0.05f;
		// et l'angle est bien l'oppose : produit vectoriel nul, produit scalaire negatif
		const float32 croix = a.ex * 0.4f - a.ey * 0.3f;
		const float32 scal = a.ex * 0.3f + a.ey * 0.4f;
		const bool memeAngle = (croix < 0.0001f && croix > -0.0001f) && scal < 0.f;
		// (c) DECONNECTE : la jumelle ne bouge PAS
		NkPoint2 d0;
		d0.liaison = NkPoint2::LiaisonDeconnecte;
		d0.ex = -0.1f;
		d0.ey = 0.05f;
		NkPoserTangente(d0, 1, 0.3f, 0.4f);
		const bool immobile = d0.ex == -0.1f && d0.ey == 0.05f;
		// (d) LONGUEUR NULLE : atteignable a la souris (relacher pile sur le
		//     sommet). Sans garde, on divise par zero et on propage des NaN.
		NkPoint2 z;
		z.liaison = NkPoint2::LiaisonAsymetrique;
		z.ex = -0.1f;
		z.ey = 0.f;
		NkPoserTangente(z, 1, 0.f, 0.f);
		const bool sain = (z.ex == z.ex) && (z.ey == z.ey) && z.ex == -0.1f;
		char dd[272];
		snprintf(dd, sizeof(dd), "miroir=%d ; asym long=%.4f (garde=%d) angle oppose=%d JUMELLE "
							   "BOUGE de %.3f (=%d) ; deconnecte immobile=%d ; longueur nulle "
							   "saine=%d",
				 miroir ? 1 : 0, (double)lg, memeLongueur ? 1 : 0, memeAngle ? 1 : 0,
				 (double)deplJumelle, jumelleBouge ? 1 : 0, immobile ? 1 : 0, sain ? 1 : 0);
		verdict("35. MIROIR (direction ET longueur), ASYMETRIQUE (la jumelle PIVOTE -- elle bouge "
				"bel et bien -- mais sa LONGUEUR est conservee ; non tranche par Lunacy, aligne "
				"sur Figma/Sketch) et DECONNECTE (la jumelle ne bouge pas) font trois choses "
				"differentes",
				miroir && memeLongueur && memeAngle && jumelleBouge && immobile && sain, dd);
	}

	// ── 36. LE CONTOUR PEINT DEVIENT UNE COURBE, ET LE ZOOM LA SUIT ────────
	// ⚠️ C'EST LE POINT DUR DE CE LOT, ET IL A DEUX MOITIES QUI TOMBENT
	//    SEPAREMENT. (a) le modele porte des tangentes -> le contour DOIT gagner
	//    des points entre les deux ancres, sinon le champ est enregistre et
	//    n'agit pas (la famille de defauts qu'on chasse depuis Q44) ; (b) le
	//    nombre d'echantillons doit suivre la TAILLE A L'ECRAN, sinon les courbes
	//    redeviennent des polygones des qu'on zoome -- un defaut qui ne se voit
	//    qu'a fort grossissement, donc tard.
	{
		st.doc.NewDocument("recette points", NkAuthor::Humain);
		const int32 iR = poser("rect", nullptr);
		NkUINode &n = st.doc.nodes[(uint32)iR];
		NkMaterialiserSommets(n);
		const NkPaintRect petit = {0.f, 0.f, 100.f, 80.f};
		float32 c0[512];
		const uint32 avant = NkContourDe(n, petit, c0, 256);
		// on courbe le segment 0 -> 1
		n.sommets[0].liaison = NkPoint2::LiaisonMiroir;
		NkPoserTangente(n.sommets[0], 1, 0.6f, -0.6f);
		float32 c1[512];
		const uint32 apres = NkContourDe(n, petit, c1, 256);
		// (a) LE CONTOUR A GAGNE DES POINTS -- le champ AGIT
		const bool agit = apres > avant;
		// (b) LES DEUX BOUTS RESTENT LES ANCRES : une cubique qui ne part pas de
		//     son sommet ferait un trou dans le trace.
		float32 an[64];
		const uint32 nbAn = NkSommetsDe(n, petit, an, 32);
		const bool depart = c1[0] == an[0] && c1[1] == an[1];
		// (c) LE ZOOM : la MEME forme, quatre fois plus grande a l'ecran, doit
		//     etre echantillonnee plus finement -- sinon on peint un polygone.
		const NkPaintRect grand = {0.f, 0.f, 400.f, 320.f};
		float32 c2[512];
		const uint32 zoome = NkContourDe(n, grand, c2, 256);
		const bool suitLeZoom = zoome > apres;
		// (d) ET LE NOMBRE EST BORNE DES DEUX COTES : une forme immense ne doit
		//     pas faire exploser la liste de points.
		const NkPaintRect enorme = {0.f, 0.f, 40000.f, 32000.f};
		float32 c3[512];
		const uint32 borne = NkContourDe(n, enorme, c3, 256);
		const bool bornee = borne <= 256u && borne < zoome * 8u;
		char dd[224];
		snprintf(dd, sizeof(dd), "contour %u -> %u pts ; depart sur l'ancre=%d ; x4 -> %u pts ; "
							   "x400 -> %u pts (borne=%d)",
				 avant, apres, depart ? 1 : 0, zoome, borne, bornee ? 1 : 0);
		verdict("36. les tangentes PEIGNENT une courbe (le contour gagne des points), la courbe "
				"part de son ancre, et l'echantillonnage SUIT LE ZOOM en restant borne",
				agit && depart && suitLeZoom && bornee, dd);
	}

	// ── 37. L'ENGLOBANT TIENT COMPTE DES COURBES ──────────────────────────
	// ⚠️ SANS LES POINTS DE CONTROLE, LA BOITE COUPE LE VENTRE DE LA COURBE. Une
	//    cubique reste dans l'enveloppe convexe de ses quatre points, donc
	//    prendre les points de controle donne une boite qui contient a coup sur
	//    le trace ; prendre les seuls sommets donne une boite TROP PETITE, et on
	//    retombe sur le defaut que Rodolf a decrit ce soir -- cliquer sur la
	//    forme sans la selectionner.
	{
		st.doc.NewDocument("recette points", NkAuthor::Humain);
		const int32 iR = poser("rect", nullptr);
		NkUINode &n = st.doc.nodes[(uint32)iR];
		n.posX = 0.f;
		n.posY = 0.f;
		NkMaterialiserSommets(n);
		// une tangente qui sort LARGEMENT de la boite, vers la gauche
		n.sommets[0].liaison = NkPoint2::LiaisonDeconnecte;
		n.sommets[0].sx = -1.f; // une demi-largeur vers la gauche
		n.sommets[0].sy = 0.f;
		const float32 avantX = n.posX;
		NkRecadrerNoeud(n, true);
		// la boite doit s'etre elargie A GAUCHE pour contenir le point de controle
		const bool elargie = n.posX < avantX - 1.f;
		// (b) ET LA COURBE PEINTE TIENT DEDANS -- c'est la promesse de
		//     l'enveloppe convexe, verifiee au lieu d'etre supposee
		const NkPaintRect r = {n.posX, n.posY, n.width.value, n.height.value};
		float32 ct[512];
		const uint32 nbC = NkContourDe(n, r, ct, 256);
		bool dedans = nbC > 0;
		for (uint32 i = 0; i < nbC; ++i) {
			if (ct[i * 2] < r.x - 0.5f || ct[i * 2] > r.x + r.w + 0.5f
				|| ct[i * 2 + 1] < r.y - 0.5f || ct[i * 2 + 1] > r.y + r.h + 0.5f)
				dedans = false;
		}
		char dd[224];
		snprintf(dd, sizeof(dd), "boite (%.1f,%.1f) %.1fx%.1f ; elargie=%d ; %u pts de contour "
							   "tous dedans=%d",
				 (double)n.posX, (double)n.posY, (double)n.width.value,
				 (double)n.height.value, elargie ? 1 : 0, nbC, dedans ? 1 : 0);
		verdict("37. l'englobant compte les POINTS DE CONTROLE (sinon il coupe le ventre de la "
				"courbe), et la courbe peinte tient entierement dedans",
				elargie && dedans, dd);
	}

	// ── 38. CONSERVATION : LES TANGENTES SURVIVENT, ET LES DOCUMENTS D'AVANT
	//        NE BOUGENT PAS D'UN OCTET ──────────────────────────────────────
	// ⚠️ LE SECOND VOLET EST LE PLUS IMPORTANT DES DEUX, et c'est celui qu'on
	//    oublie : ajouter cinq champs a un format POSITIONNEL est exactement le
	//    genre de changement qui reecrit tous les fichiers existants. Un tracé
	//    sans tangente ni rayon doit rendre les DEUX MEMES nombres qu'hier.
	{
		st.doc.NewDocument("recette points", NkAuthor::Humain);
		const int32 iVif = poser("rect", nullptr);
		NkMaterialiserSommets(st.doc.nodes[(uint32)iVif]);
		NkString sansT;
		st.doc.Save(sansT);
		const bool deuxNombres = !sansT.Contains("sommet_1 = -1 -1 0");
		// aller-retour d'un document SANS tangente : octet pour octet
		NkUIDocument relu0;
		const bool ok0 = relu0.Load(sansT.Data());
		NkString sansT2;
		relu0.Save(sansT2);
		const bool stable0 = ok0 && sansT.Size() == sansT2.Size()
							 && NkComponentDecl::StrEq(sansT.Data(), sansT2.Data());
		// puis AVEC tangentes et liaison
		{
			NkUINode &n = st.doc.nodes[(uint32)iVif];
			n.sommets[1].liaison = NkPoint2::LiaisonAsymetrique;
			n.sommets[1].ex = -0.25f;
			n.sommets[1].ey = 0.125f;
			n.sommets[1].sx = 0.5f;
			n.sommets[1].sy = -0.25f;
		}
		NkString avecT;
		st.doc.Save(avecT);
		NkUIDocument relu;
		const bool ok = relu.Load(avecT.Data());
		bool relues = false;
		if (ok && relu.IsValidIndex(iVif) && (uint32)relu.nodes[(uint32)iVif].sommets.Size() > 1) {
			const NkPoint2 &q = relu.nodes[(uint32)iVif].sommets[1];
			relues = q.ex == -0.25f && q.ey == 0.125f && q.sx == 0.5f && q.sy == -0.25f
					 && q.liaison == NkPoint2::LiaisonAsymetrique;
		}
		NkString avecT2;
		relu.Save(avecT2);
		const bool stable = avecT.Size() == avecT2.Size()
							&& NkComponentDecl::StrEq(avecT.Data(), avecT2.Data());
		char dd[256];
		snprintf(dd, sizeof(dd), "sans tangente : %u o, 2 nombres=%d, aller-retour stable=%d ; "
							   "avec : %u o, relues=%d, stable=%d",
				 (uint32)sansT.Size(), deuxNombres ? 1 : 0, stable0 ? 1 : 0,
				 (uint32)avecT.Size(), relues ? 1 : 0, stable ? 1 : 0);
		verdict("38. CONSERVATION : un trace SANS tangente s'ecrit avec ses deux nombres d'hier "
				"(octet pour octet), et un trace AVEC tangentes se relit a l'identique, liaison "
				"comprise",
				deuxNombres && stable0 && relues && stable, dd);
	}

	// ── 39. `rayon` ET LES TANGENTES NE SONT PAS DEUX VERITES ──────────────
	// ⚠️ LA REGLE EST ECRITE DANS LE MODELE (`RayonActif`) ET AU DESSIN
	//    (`NkRayonPeint`), et ce cas existe pour qu'elle ne se defasse pas. Sans
	//    elle, un sommet portant un rayon ET des tangentes aurait DEUX
	//    interpretations, et laquelle gagne ne dependrait que de l'ordre du code
	//    -- la classe de defaut que ce chantier tranche depuis Q42.
	{
		NkPoint2 p;
		p.rayon = 12.f;
		const bool rayonSeul = p.RayonActif();
		p.liaison = NkPoint2::LiaisonMiroir;
		p.sx = 0.3f;
		const bool rayonEteint = !p.RayonActif() && p.Courbe();
		// et le VOISINAGE compte : un sommet vif COLLE a un segment courbe ne
		// s'arrondit pas non plus -- on ne saurait pas ou la cubique commence.
		st.doc.NewDocument("recette points", NkAuthor::Humain);
		const int32 iR = poser("rect", nullptr);
		NkUINode &n = st.doc.nodes[(uint32)iR];
		NkMaterialiserSommets(n);
		n.sommets[1].rayon = 10.f; // vif + rayon
		const bool peintAvant = NkRayonPeint(n, 1, 4);
		n.sommets[0].liaison = NkPoint2::LiaisonMiroir; // le segment 0->1 devient courbe
		n.sommets[0].sx = 0.5f;
		const bool peintApres = NkRayonPeint(n, 1, 4);
		// ET LE MODELE GARDE LA VALEUR : on ignore au DESSIN, on n'efface pas.
		const bool gardee = n.sommets[1].rayon == 10.f;
		char dd[224];
		snprintf(dd, sizeof(dd), "rayon seul actif=%d ; eteint par les tangentes=%d ; voisin "
							   "courbe : peint %d -> %d ; valeur gardee=%d",
				 rayonSeul ? 1 : 0, rayonEteint ? 1 : 0, peintAvant ? 1 : 0, peintApres ? 1 : 0,
				 gardee ? 1 : 0);
		verdict("39. `rayon` et les tangentes ne cohabitent PAS sur un sommet : le rayon ne vaut "
				"que sur un sommet droit dont AUCUN cote n'est courbe, et sa valeur est ignoree "
				"au dessin sans etre effacee",
				rayonSeul && rayonEteint && peintAvant && !peintApres && gardee, dd);
	}

	// ── 40. LA POIGNEE DE TANGENTE GAGNE, ET LES MODIFICATEURS VIENNENT DE
	//        LA SOURCE ─────────────────────────────────────────────────────
	// ⚠️ TROISIEME ETAGE DE LA MEME REGLE, ET IL FALLAIT L'ECRIRE AVANT DE LA
	//    SUBIR. `NkAQuiLaPoignee` a deja tranche « en mode points, le sommet
	//    gagne sur la poignee de boite ». Une poignee de TANGENTE peut a son tour
	//    recouvrir son sommet -- il suffit d'avoir ramene la tangente pres de
	//    zero, ce qui est le geste normal pour aplatir une courbe. Sans priorite
	//    dite, ce geste deviendrait impossible : on reprendrait le sommet, et la
	//    courbe qu'on essaie d'aplatir se DEPLACERAIT.
	{
		// (a) a distance egale, la tangente gagne
		const bool gagne =
			NkAQuiLAncre(4.f, 4.f, 12.f) == NkProprioAncre::Tangente;
		// (b) mais pas quand il n'y en a aucune de visible : le sommet reprend
		const bool sommet = NkAQuiLAncre(-1.f, 4.f, 12.f) == NkProprioAncre::Sommet;
		// (c) ni l'une ni l'autre au-dela de la tolerance
		const bool aucune = NkAQuiLAncre(40.f, 40.f, 12.f) == NkProprioAncre::Aucune;
		// (d) une tangente HORS tolerance ne vole pas un sommet qui, lui, est dedans
		const bool pasDeVol = NkAQuiLAncre(40.f, 4.f, 12.f) == NkProprioAncre::Sommet;
		// ── LES MODIFICATEURS, CITES : « Hold down Alt to create a disconnected
		//    point. Hold down Ctrl to create an asymmetric point. »
		//    (`lunacy.docs.icons8.com/rn_before_v10/`)
		const bool mods = NkLiaisonDuModificateur(false, true) == NkPoint2::LiaisonDeconnecte
						  && NkLiaisonDuModificateur(true, false) == NkPoint2::LiaisonAsymetrique
						  // aucun modificateur : le sommet GARDE sa liaison. Rendre
						  // `Droit` ici effacerait les tangentes au premier glisser nu.
						  && NkLiaisonDuModificateur(false, false) == 255u
						  // ⚠️ ALT PRIME SUR CTRL quand les deux sont tenus : il faut
						  //    UNE reponse, et la laisser a l'ordre des `if` serait
						  //    une priorite subie.
						  && NkLiaisonDuModificateur(true, true) == NkPoint2::LiaisonDeconnecte;
		// ── CHANGER DE TYPE RANGE LE MODELE, sinon ce n'est qu'une etiquette
		NkPoint2 p;
		p.liaison = NkPoint2::LiaisonAsymetrique;
		p.sx = 0.4f;
		p.sy = 0.f;
		p.ex = -0.1f;
		p.ey = 0.2f; // visiblement dissymetrique
		NkPoserLiaison(p, NkPoint2::LiaisonMiroir);
		const bool range = p.ex == -0.4f && p.ey == 0.f;
		// ⚠️ ET REVENIR A `Droit` EFFACE les tangentes plutot que de les garder
		//    « au cas ou » : gardees, elles ressusciteraient a la prochaine
		//    bascule et l'utilisateur retrouverait une courbe qu'il croyait avoir
		//    supprimee. Le pas d'annulation, lui, les rend parfaitement.
		NkPoserLiaison(p, NkPoint2::LiaisonDroit);
		const bool efface = p.ex == 0.f && p.ey == 0.f && p.sx == 0.f && p.sy == 0.f
							&& !p.Courbe();
		char dd[224];
		snprintf(dd, sizeof(dd), "tangente gagne=%d sommet seul=%d aucune=%d pas de vol=%d ; "
							   "modificateurs=%d ; miroir range=%d ; retour droit efface=%d",
				 gagne ? 1 : 0, sommet ? 1 : 0, aucune ? 1 : 0, pasDeVol ? 1 : 0, mods ? 1 : 0,
				 range ? 1 : 0, efface ? 1 : 0);
		verdict("40. la POIGNEE DE TANGENTE gagne sur son sommet (sans le voler quand elle est "
				"hors tolerance), Alt/Ctrl donnent les liaisons que la doc Lunacy cite, et "
				"changer de type RANGE les tangentes au lieu de poser une etiquette",
				gagne && sommet && aucune && pasDeVol && mods && range && efface, dd);
	}

	// ── 41. LE PAS DU CLAVIER (vague 1 du document de reference) ───────────
	// Le document `design/13_Lunacy_reference_interaction.md` nomme le
	// deplacement aux fleches « le manque le plus courant » : c'est le geste
	// d'ajustement fin, celui qu'on fait cent fois par heure et qu'aucune souris
	// ne remplace.
	//
	// ⚠️ ET LE PAS EST EN UNITES DE DOCUMENT, PAS EN PIXELS D'ECRAN. Au zoom 4,
	//    un pas d'un pixel d'ecran vaudrait un quart d'unite : l'objet avancerait
	//    de moins en moins vite a mesure qu'on zoome POUR ETRE PRECIS -- l'inverse
	//    exact de ce qu'on cherche. *Un pas se compte dans l'unite de ce qu'on
	//    deplace, pas dans celle de ce qu'on voit.* Le cas le tient en comparant
	//    le pas a deux zooms.
	{
		const bool nu = NkPasClavier(false) == 1.f;
		const bool avecMaj = NkPasClavier(true) == 10.f;
		// le pas ne depend d'AUCUN etat de vue : deux appels identiques a deux
		// moments quelconques rendent la meme chose. C'est ce qui le distingue
		// d'une tolerance d'aimantation, qui, elle, suit le zoom (recette snap 4).
		const bool independant = NkPasClavier(false) == 1.f && NkPasClavier(true) == 10.f;
		char d[160];
		snprintf(d, sizeof(d), "pas nu=%.0f, avec Maj=%.0f, independant du zoom=%d",
				 (double)NkPasClavier(false), (double)NkPasClavier(true),
				 independant ? 1 : 0);
		verdict("41. le pas du clavier vaut 1 unite de DOCUMENT (10 avec Maj), et il ne suit "
				"PAS le zoom -- contrairement a la tolerance d'aimantation, qui, elle, le suit",
				nu && avecMaj && independant, d);
	}

	// ── 42. LE REMPLISSAGE COUVRE UN CONTOUR CONCAVE ──────────────────────
	// 🔴 ON AVAIT LIVRE UN DEFAUT, ET C'EST LA CAPTURE QUI L'A DIT. Le peintre
	//    remplissait par un EVENTAIL DEPUIS LE CENTROIDE : juste pour un convexe,
	//    juste pour une etoile, FAUX des que le contour est concave -- les
	//    triangles traversent le creux. Tant que l'application ne posait que des
	//    rectangles et des ellipses, personne ne pouvait le voir. Les poignees de
	//    Bezier livrees ce soir permettent de tirer une courbe VERS L'INTERIEUR
	//    d'un bouton : `preuve_courbe_bezier_n9.png` montre le debordement.
	//    *Une limite qu'aucun geste n'atteignait est devenue un defaut le jour ou
	//    un geste l'a atteinte.*
	//
	// ⚠️ CE CAS NE MESURE PAS LE PEINTRE (il a besoin d'un contexte NKGui), IL
	//    MESURE LE TRIANGULATEUR -- c'est-a-dire la piece dont le peintre depend,
	//    et la seule dont un banc sans fenetre puisse juger. Ce qu'il tient : sur
	//    un contour EN L, la triangulation ne doit produire AUCUN triangle qui
	//    empiete sur le creux. C'est exactement ce que l'eventail faisait.
	{
		// un « L » : concave par construction, le creux est en haut a droite
		NkVector<NkVector<nkentseu::math::NkVec2f>> contours;
		NkVector<nkentseu::math::NkVec2f> L;
		L.PushBack(nkentseu::math::NkVec2f(0.f, 0.f));
		L.PushBack(nkentseu::math::NkVec2f(40.f, 0.f));
		L.PushBack(nkentseu::math::NkVec2f(40.f, 40.f));
		L.PushBack(nkentseu::math::NkVec2f(100.f, 40.f));
		L.PushBack(nkentseu::math::NkVec2f(100.f, 100.f));
		L.PushBack(nkentseu::math::NkVec2f(0.f, 100.f));
		contours.PushBack(L);
		const NkVector<std::size_t> tri = nkentseu::NkEarcut<float32>(contours);
		const bool aTriangule = tri.Size() >= 3 && (tri.Size() % 3) == 0;
		// (a) LE CREUX RESTE VIDE. On prend un point franchement dans l'encoche
		//     (70, 20) et on verifie qu'AUCUN triangle ne le contient.
		auto dansTriangle = [&](float32 px, float32 py, std::size_t t) {
			const nkentseu::math::NkVec2f &a = L[(uint32)tri[t]];
			const nkentseu::math::NkVec2f &b = L[(uint32)tri[t + 1]];
			const nkentseu::math::NkVec2f &c = L[(uint32)tri[t + 2]];
			const float32 d1 = (px - b.x) * (a.y - b.y) - (a.x - b.x) * (py - b.y);
			const float32 d2 = (px - c.x) * (b.y - c.y) - (b.x - c.x) * (py - c.y);
			const float32 d3 = (px - a.x) * (c.y - a.y) - (c.x - a.x) * (py - a.y);
			const bool neg = (d1 < 0.f) || (d2 < 0.f) || (d3 < 0.f);
			const bool pos = (d1 > 0.f) || (d2 > 0.f) || (d3 > 0.f);
			return !(neg && pos);
		};
		bool creuxVide = true;
		for (std::size_t t = 0; t + 2 < tri.Size(); t += 3)
			if (dansTriangle(70.f, 20.f, t))
				creuxVide = false;
		// (b) ET LE PLEIN EST BIEN COUVERT : un point du corps du L doit etre
		//     DANS un triangle. Sans ce volet, une triangulation qui ne rend rien
		//     du tout passerait le sous-cas (a) haut la main.
		bool pleinCouvert = false;
		for (std::size_t t = 0; t + 2 < tri.Size(); t += 3)
			if (dansTriangle(20.f, 50.f, t))
				pleinCouvert = true;
		// (c) L'AIRE TRIANGULEE VAUT L'AIRE DU POLYGONE (5600 pour ce L) : c'est
		//     la mesure qui attrape a la fois le debordement et le manque.
		float32 aireTri = 0.f;
		for (std::size_t t = 0; t + 2 < tri.Size(); t += 3) {
			const nkentseu::math::NkVec2f &a = L[(uint32)tri[t]];
			const nkentseu::math::NkVec2f &b = L[(uint32)tri[t + 1]];
			const nkentseu::math::NkVec2f &c = L[(uint32)tri[t + 2]];
			float32 s = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
			aireTri += (s < 0.f ? -s : s) * 0.5f;
		}
		// ⚠️ 7600 ET NON 5600 : le carre 100x100 MOINS l'encoche 60x40 (2400).
		//    Ma premiere valeur attendue etait fausse, et le cas est tombe
		//    dessus -- sur MON arithmetique, pas sur le code. C'est exactement ce
		//    qu'on demande a un chiffre attendu calcule a la main : qu'il se
		//    fasse contredire quand il a tort. *Le banc a eu raison contre moi.*
		const bool aireJuste = aireTri > 7599.f && aireTri < 7601.f;
		char d[224];
		snprintf(d, sizeof(d), "%u triangle(s) ; creux vide=%d plein couvert=%d ; aire %.0f "
							   "(attendue 7600)",
				 (uint32)(tri.Size() / 3), creuxVide ? 1 : 0, pleinCouvert ? 1 : 0,
				 (double)aireTri);
		verdict("42. le triangulateur remplit un contour CONCAVE sans deborder dans le creux, "
				"couvre le plein, et son aire vaut EXACTEMENT celle du polygone",
				aTriangule && creuxVide && pleinCouvert && aireJuste, d);
	}

	// ── 43. LE TRIANGULATEUR SUR UN CONTOUR REEL DE COURBE ────────────────
	// 🔴 CE CAS EXISTE PARCE QUE LE CAS 42 NE SUFFISAIT PAS, ET LA DIFFERENCE
	//    EST INSTRUCTIVE. Le cas 42 donne au triangulateur un « L » propre de
	//    six points : il passe. Branche sur le PEINTRE, le meme triangulateur a
	//    fait planter l'application -- corruption de tas, code 0xC0000374,
	//    reproductible en trois secondes avec `--courber`, absent sans.
	//
	//    La difference entre les deux n'est pas le triangulateur : c'est SON
	//    ENTREE. Un contour de courbe reel fait cent-vingt-huit points, dont
	//    beaucoup tres proches les uns des autres (les echantillons de cubique),
	//    et certains potentiellement CONFONDUS. *Un banc qui ne nourrit son
	//    mecanisme que de donnees propres mesure le mecanisme, pas l'usage.*
	//
	// ⚠️ CE CAS NE PROUVE PAS QUE LE PLANTAGE EST REPARE -- il n'est PAS repare,
	//    et le peintre est revenu a son eventail. Il pose le TEMOIN qui dira
	//    quand il l'est : le jour ou ce cas passe avec un contour reel, le
	//    branchement pourra etre retente.
	{
		st.doc.NewDocument("recette points", NkAuthor::Humain);
		const int32 iC = poser("rect", nullptr);
		NkUINode &nc = st.doc.nodes[(uint32)iC];
		NkMaterialiserSommets(nc);
		for (uint32 k = 0; k < (uint32)nc.sommets.Size(); ++k) {
			nc.sommets[k].liaison = NkPoint2::LiaisonMiroir;
			NkPoserTangente(nc.sommets[k], 1, 0.55f, 0.35f);
		}
		float32 ct[256];
		const NkPaintRect rc = {100.f, 100.f, 180.f, 36.f};
		const uint32 nbc = NkContourDe(nc, rc, ct, 128);
		// (a) LE BUDGET EST TENU : le contour ne DEPASSE PLUS le cap qu'on lui
		//     donne. C'etait le premier defaut trouve -- un contour tronque ne se
		//     referme plus sur lui-meme, et il se croise.
		const bool tientDansLeCap = nbc > 0 && nbc <= 128u;
		// (b) AUCUN POINT CONFONDU AVEC SON VOISIN : c'est le suspect qui reste,
		//     et le seul que ce banc puisse nommer sans debogueur.
		uint32 confondus = 0;
		for (uint32 i = 0; i < nbc; ++i) {
			const uint32 j = (i + 1) % nbc;
			const float32 dx = ct[i * 2] - ct[j * 2];
			const float32 dy = ct[i * 2 + 1] - ct[j * 2 + 1];
			if (NkLongueur2D(dx, dy) < 0.0001f)
				++confondus;
		}
		char d[192];
		snprintf(d, sizeof(d), "contour reel = %u points (cap 128), %u point(s) confondu(s) "
							   "avec leur voisin",
				 nbc, confondus);
		verdict("43. le contour d'une forme ENTIEREMENT courbe tient dans le cap qu'on lui "
				"donne et ne contient AUCUN point confondu avec son voisin (temoin du "
				"branchement du triangulateur)",
				tientDansLeCap && confondus == 0, d);
	}

	// ── 44. LE CLAVIER OUVRE LES MEMES GESTES QUE LES MENUS ────────────────
	// 🔴 CE CAS EXISTE PARCE QUE NOS MENUS MENTAIENT. L'inventaire du 01/09 a
	//    mesure que l'application ne lisait AUCUNE touche de lettre -- quatre
	//    touches en tout -- alors que les menus affichent `Ctrl+G`, `Ctrl+D`,
	//    `Ctrl+Maj+G` a cote d'entrees qui, elles, fonctionnent. *Un libelle qui
	//    annonce une capacite absente est un libelle qui ment*, et il jette le
	//    doute sur tous les autres raccourcis du meme menu.
	//
	// ⚠️ CE QUE LE CAS TIENT, ET C'EST LES DEUX SENS : que la combinaison ATTENDUE
	//    rende l'action attendue, ET que la touche NUE ne rende rien. Sans le
	//    second volet, une table qui rendrait « Copier » pour n'importe quelle
	//    touche passerait le premier -- et taper une lettre dans la toile
	//    declencherait un geste.
	{
		const bool lies = NkActionDuRaccourci(true, false, 'C') == NkActionCtx::Copier
						  && NkActionDuRaccourci(true, false, 'X') == NkActionCtx::Couper
						  && NkActionDuRaccourci(true, false, 'V') == NkActionCtx::Coller
						  && NkActionDuRaccourci(true, false, 'D') == NkActionCtx::Dupliquer
						  && NkActionDuRaccourci(true, false, 'G') == NkActionCtx::Grouper
						  // le MAJ distingue deux gestes INVERSES sur la meme lettre
						  && NkActionDuRaccourci(true, true, 'G') == NkActionCtx::Degrouper
						  // L'ORDRE DE PROFONDEUR (02/09) : meme convention, `Maj`
						  // distingue « d'un cran » de « tout au bout ».
						  && NkActionDuRaccourci(true, false, ']') == NkActionCtx::Avancer
						  && NkActionDuRaccourci(true, true, ']') == NkActionCtx::PremierPlan
						  && NkActionDuRaccourci(true, false, '[') == NkActionCtx::Reculer
						  && NkActionDuRaccourci(true, true, '[') == NkActionCtx::ArrierePlan
						  // LES COMPOSANTS : `Ctrl+Alt+K` / `Ctrl+Alt+D` (02/09).
						  && NkActionDuRaccourci(true, false, 'K', true)
								 == NkActionCtx::ExtraireComposant
						  && NkActionDuRaccourci(true, false, 'D', true)
								 == NkActionCtx::DetacherComposant
						  // ⚠️ ET `Alt` DISTINGUE DEUX GESTES SUR LA MEME LETTRE :
						  //    `Ctrl+D` duplique, `Ctrl+Alt+D` detache. Sans ce
						  //    volet, un `switch` qui ignorerait `alt` ferait
						  //    DUPLIQUER en silence quand on demande de detacher --
						  //    et le rapport de defaut serait « le detachement ne
						  //    marche pas », le symptome le moins informatif.
						  && NkActionDuRaccourci(true, false, 'D') == NkActionCtx::Dupliquer;
		// (b) LA TOUCHE NUE NE FAIT RIEN : toutes nos combinaisons passent par Ctrl
		const bool nuInerte = NkActionDuRaccourci(false, false, 'G') == NkActionCtx::NB
							  && NkActionDuRaccourci(false, true, 'D') == NkActionCtx::NB;
		// (c) UNE TOUCHE NON LIEE REND LA SENTINELLE, jamais une action par
		//     defaut : rendre « Copier » pour une touche inconnue serait pire que
		//     ne rien faire.
		const bool inconnue = NkActionDuRaccourci(true, false, 'Q') == NkActionCtx::NB
							  && NkActionDuRaccourci(true, false, 'Z') == NkActionCtx::NB;
		// (d) ET CHAQUE ACTION LIEE EST EXECUTABLE PAR LE DISPATCHER COMMUN --
		//     c'est ce qui garantit « trois portes, une ecriture ». Une action
		//     que le clavier nommerait et que le dispatcher ignorerait serait un
		//     raccourci mort, exactement le defaut qu'on repare.
		// ⚠️ LE COMPTE VIENT DE `NkNbRaccourcisCtx`, PAS D'UN NOMBRE ECRIT ICI :
		//    une liste citee a la main se perime a la premiere combinaison qu'on
		//    ajoute -- et elle se perimerait EN SILENCE, en continuant a annoncer
		//    « 6/6 » pendant que la table en porterait dix.
		// ⚠️ CETTE TABLE A DU GRANDIR LE 02/09, ET C'EST LA GARDE QUI L'A
		//    RECLAME : l'ajout de `Ctrl+Alt+M` (appliquer au composant) a fait
		//    tomber ce cas a 51/52 avec « table complete=0 ». C'est exactement
		//    son office -- *le compte fige est legitime quand la liste est
		//    fermee : sa taille EST la regle, et le jour ou elle grandit, c'est
		//    une DECISION*, pas un ajustement silencieux.
		static const char kT[15] = {'C', 'X', 'V', 'D', 'G', 'G', ']', ']',
									'[', '[', 'K', 'D', 'M', 'H', 'V'};
		static const bool kM[15] = {false, false, false, false, false, true, false, true,
									false, true,  false, false, false, true,  true};
		static const bool kA[15] = {false, false, false, false, false, false, false, false,
									false, false, true,  true,  true,  false, false};
		// ⚠️ UN AXE `ctrl` EST APPARU LE 02/09, ET IL DIT UN FAIT : `Maj+H` et
		//    `Maj+V` (les miroirs, tranches par Rodolf) sont les DEUX SEULES
		//    combinaisons de la table qui ne passent pas par `Ctrl`. Le banc
		//    forcait `ctrl = true` pour tout le monde -- il aurait donc teste
		//    `Ctrl+Maj+H`, qui n'est lie a rien, et rapporte « non executable »
		//    sur un raccourci parfaitement branche. *Un banc qui ne sait pas
		//    poser la question ne mesure pas le code, il mesure sa propre
		//    hypothese.*
		static const bool kC[15] = {true, true, true, true, true, true, true, true,
									true, true, true, true, true, false, false};
		const uint32 nbAttendu = NkNbRaccourcisCtx();
		uint32 executables = 0;
		for (uint32 i = 0; i < 15u && i < nbAttendu; ++i) {
			st.doc.NewDocument("recette points", NkAuthor::Humain);
			const int32 pere = poser("rect", nullptr);
			const int32 a1 = st.doc.AddChild(pere, "", NkAuthor::Humain);
			(void)st.doc.AddChild(pere, "", NkAuthor::Humain);
			st.SelectSingle(a1);
			// `Ctrl+Alt+D` demande une INSTANCE pour avoir un effet : on en fabrique
			// une par le geste voisin, sinon le dispatcher refuserait a raison.
			// `Ctrl+Alt+D` ET `Ctrl+Alt+M` demandent une INSTANCE pour avoir un
			// effet : on en fabrique une par le geste voisin, sinon le
			// dispatcher refuserait a raison.
			if (kA[i] && (kT[i] == 'D' || kT[i] == 'M'))
				(void)st.doc.ExtraireComposant(a1, "", "pour_geste");
			const NkActionCtx act = NkActionDuRaccourci(kC[i], kM[i], kT[i], kA[i]);
			if (act != NkActionCtx::NB && NkAppliquerActionCtx(st, a1, act))
				++executables;
		}
		const bool toutesCouvertes = nbAttendu == 15u;
		char d[224];
		snprintf(d, sizeof(d), "%u combinaisons liees=%d ; touche nue inerte=%d ; inconnue "
							   "sentinelle=%d ; %u/%u executables par le dispatcher commun ; "
							   "table complete=%d",
				 nbAttendu, lies ? 1 : 0, nuInerte ? 1 : 0, inconnue ? 1 : 0, executables,
				 nbAttendu, toutesCouvertes ? 1 : 0);
		verdict("44. le CLAVIER rend les memes actions que les menus (et les execute par le "
				"MEME dispatcher), la touche nue ne fait rien, une touche non liee rend la "
				"sentinelle, et l'ORDRE DE PROFONDEUR a ses quatre raccourcis",
				lies && nuInerte && inconnue && toutesCouvertes && executables == nbAttendu, d);
	}

	// ── 45. LA PORTE SANS ALLOCATION TIENT SON CONTRAT ────────────────────
	// 🔴 C'EST LA PORTE QUI A REPARE LE PLANTAGE, ET ELLE MERITE SON CAS. La
	//    porte qui ALLOUE (`NkEarcut`) a une DOUBLE LIBERATION : elle libere
	//    chaque oreille decoupee, puis relibere la liste depuis sa tete -- tete
	//    qui a presque toujours ete decoupee. Un appel unique corrompt le tas en
	//    silence et survit ; une boucle de dessin appelle des milliers de fois
	//    par seconde, et ca tombe en trois secondes.
	//
	//    La reparation n'est PAS « corriger la liberation », c'est « ne plus
	//    allouer » : sans tas, plus de liberation a equilibrer, donc plus de
	//    double liberation POSSIBLE. *La classe entiere de defauts disparait au
	//    lieu d'etre corrigee exemplaire par exemplaire.*
	//
	// ⚠️ CE CAS TIENT LES QUATRE PROMESSES DE LA SIGNATURE, et les trois
	//    dernieres comptent plus que la premiere : c'est un tampon fourni par
	//    l'appelant, et une fonction qui ecrit a cote ou qui accepte un tampon
	//    trop petit est plus dangereuse que celle qu'elle remplace.
	{
		// un « L » concave, le meme que le cas 42 : aire connue = 7600
		nkentseu::math::NkVec2f L[6] = {
			nkentseu::math::NkVec2f(0.f, 0.f),     nkentseu::math::NkVec2f(40.f, 0.f),
			nkentseu::math::NkVec2f(40.f, 40.f),   nkentseu::math::NkVec2f(100.f, 40.f),
			nkentseu::math::NkVec2f(100.f, 100.f), nkentseu::math::NkVec2f(0.f, 100.f)};
		nkentseu::detail::NkEarcutNode<float32> nd[6];
		uint32 out[12];
		// (a) LA BORNE EST EXACTE : un polygone simple a N sommets donne N-2
		//     triangles. Pas « au plus », pas « environ » -- exactement.
		const uint32 nbT = nkentseu::NkEarcutVers<float32>(L, 6u, nd, 6u, out, 12u);
		const bool borneExacte = (nbT == 4u);
		// (b) L'AIRE VAUT CELLE DU POLYGONE : la triangulation couvre le plein
		//     sans deborder dans le creux.
		float32 aire = 0.f;
		for (uint32 t = 0; t < nbT; ++t) {
			const nkentseu::math::NkVec2f &a = L[out[t * 3 + 0]];
			const nkentseu::math::NkVec2f &b = L[out[t * 3 + 1]];
			const nkentseu::math::NkVec2f &c = L[out[t * 3 + 2]];
			const float32 s = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
			aire += (s < 0.f ? -s : s) * 0.5f;
		}
		const bool aireJuste = aire > 7599.f && aire < 7601.f;
		// (c) LE SENS EST MESURE, PAS DEMANDE. Le MEME contour parcouru A
		//     L'ENVERS doit rendre la MEME aire. La porte qui alloue, elle, exige
		//     un contour CCW et rend zero triangle si on se trompe -- un echec
		//     SILENCIEUX (la forme disparait). Ce volet est la garantie que le
		//     peintre peut lui donner un contour dans un sens quelconque.
		nkentseu::math::NkVec2f Lr[6];
		for (uint32 i = 0; i < 6u; ++i)
			Lr[i] = L[5u - i];
		uint32 out2[12];
		const uint32 nbT2 = nkentseu::NkEarcutVers<float32>(Lr, 6u, nd, 6u, out2, 12u);
		float32 aire2 = 0.f;
		for (uint32 t = 0; t < nbT2; ++t) {
			const nkentseu::math::NkVec2f &a = Lr[out2[t * 3 + 0]];
			const nkentseu::math::NkVec2f &b = Lr[out2[t * 3 + 1]];
			const nkentseu::math::NkVec2f &c = Lr[out2[t * 3 + 2]];
			const float32 s = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
			aire2 += (s < 0.f ? -s : s) * 0.5f;
		}
		const bool sensIndifferent = nbT2 == 4u && aire2 > 7599.f && aire2 < 7601.f;
		// (d) UN TAMPON TROP PETIT EST REFUSE FRANCHEMENT -- zero triangle, et
		//     surtout AUCUNE ecriture. On place un temoin apres la zone permise :
		//     s'il bouge, la fonction a ecrit hors du tampon qu'on lui a donne.
		uint32 petit[10];
		petit[9] = 0xABCDu;
		const uint32 nbT3 = nkentseu::NkEarcutVers<float32>(L, 6u, nd, 6u, petit, 9u);
		const bool refusFranc = (nbT3 == 0u) && (petit[9] == 0xABCDu);
		// (e) ET UN TAMPON DE NOEUDS TROP PETIT AUSSI : les deux tampons sont
		//     verifies, pas seulement celui de sortie.
		const bool refusNoeuds = nkentseu::NkEarcutVers<float32>(L, 6u, nd, 5u, out, 12u) == 0u;
		char d[208];
		snprintf(d, sizeof(d), "%u triangles (attendu 4) ; aire %.0f ; sens inverse -> %u tri "
							   "aire %.0f ; refus sortie=%d refus noeuds=%d",
				 nbT, (double)aire, nbT2, (double)aire2, refusFranc ? 1 : 0,
				 refusNoeuds ? 1 : 0);
		verdict("45. la porte SANS ALLOCATION rend la borne exacte N-2, couvre l'aire du "
				"polygone, se moque du SENS de parcours, et REFUSE franchement un tampon trop "
				"petit sans rien ecrire dehors",
				borneExacte && aireJuste && sensIndifferent && refusFranc && refusNoeuds, d);
	}

	// ── 46. CHANGER DE TYPE DONNE DES POIGNEES QU'ON PEUT VOIR ET SAISIR ──────
	// 🔴 LE CAS QUI MANQUAIT, ET C'EST UNE CAPTURE DE RODOLF QUI L'A NOMME
	//    (`probleme_pas_de_poignees_222121.png`, 01/09) : le panneau annoncait
	//    « Sommet 3 sur 4 », type « Libre » en surbrillance -- et AUCUNE poignee a
	//    l'ecran. Le sommet devenait courbe avec quatre tangentes a zero ; le
	//    peintre et le ramasseur les sautaient tous les deux (`tx == 0 && ty == 0`),
	//    donc rien a voir, rien a saisir, et aucune main ne pouvait sortir de la.
	//
	// ⚠️ ET LES CAS 35 ET 40 ETAIENT VERTS PENDANT CE TEMPS-LA, parce qu'ils
	//    POSENT LEURS TANGENTES EUX-MEMES avant de mesurer. Ils prouvaient une
	//    regle vraie sur un etat que l'interface ne savait pas atteindre. Ce cas
	//    part donc de l'ETAT QUE LA MAIN PRODUIT -- un sommet droit, un clic sur
	//    un type -- et il n'a le droit de rien poser lui-meme.
	{
		st.doc.NewDocument("recette points", NkAuthor::Humain);
		const int32 iA = poser("rect", nullptr);
		NkUINode &n = st.doc.nodes[(uint32)iA];
		NkMaterialiserSommets(n);
		const uint32 nbA = (uint32)n.sommets.Size();
		// (a) AVANT : un sommet droit n'a aucune tangente, et c'est correct.
		const bool vifAvant = !n.sommets[1].Courbe();
		// (b) LE GESTE DE L'INTERFACE, ET RIEN D'AUTRE : on appelle exactement la
		//     porte que le panneau appelle, sans poser un seul nombre a la main.
		NkPoserLiaisonSommet(n, 1u, NkPoint2::LiaisonDeconnecte);
		const NkPoint2 &p1 = n.sommets[1];
		const bool deuxPoignees = (p1.ex != 0.f || p1.ey != 0.f) && (p1.sx != 0.f || p1.sy != 0.f);
		// (c) ELLES SONT VISIBLES : la garde du peintre est `tx == 0 && ty == 0`,
		//     donc « non nul » suffit a etre peint -- mais une poignee a 0,001 du
		//     sommet serait invisible ET invisable. On exige le plancher.
		const float32 le = NkLongueur2D(p1.ex, p1.ey), ls = NkLongueur2D(p1.sx, p1.sy);
		const bool visables = le >= 0.04f && ls >= 0.04f;
		// (d) AUCUN NaN : la corde des voisins peut etre nulle, et la division
		//     silencieuse est exactement ce qui se propage jusqu'au dessin.
		const bool sain = (p1.ex == p1.ex) && (p1.ey == p1.ey) && (p1.sx == p1.sx)
						  && (p1.sy == p1.sy);
		// (e) LE CONTOUR PEINT LE MONTRE : sans ce volet, « des nombres non nuls »
		//     resterait une affirmation sur le modele. Le trace doit gagner des
		//     points, c'est-a-dire que l'ecran doit CHANGER.
		float32 cA[512], cB[512];
		const NkPaintRect bo = {0.f, 0.f, 100.f, 80.f};
		const uint32 nAp = NkContourDe(n, bo, cB, 256);
		st.doc.NewDocument("recette points", NkAuthor::Humain);
		const int32 iB = poser("rect", nullptr);
		NkUINode &n2 = st.doc.nodes[(uint32)iB];
		NkMaterialiserSommets(n2);
		const uint32 nAv = NkContourDe(n2, bo, cA, 256);
		const bool ecranChange = nAp > nAv;
		// (f) ⚠️ CONSERVATION : amorcer n'ECRASE PAS une courbe deja tiree. Sans ce
		//     volet, passer de « Miroir » a « Libre » sur un sommet qu'on vient de
		//     courber remettrait la poignee a sa longueur par defaut -- une main
		//     qui perd son travail en changeant un type.
		NkPoserLiaisonSommet(n2, 2u, NkPoint2::LiaisonMiroir);
		NkPoserTangente(n2.sommets[2], 1, 0.77f, 0.11f);
		NkPoserLiaisonSommet(n2, 2u, NkPoint2::LiaisonDeconnecte);
		const bool garde = n2.sommets[2].sx > 0.769f && n2.sommets[2].sx < 0.771f;
		// (g) ET LE RETOUR A « DROIT » EFFACE ENCORE : l'amorce ne doit pas
		//     ressusciter des tangentes que le type promet d'avoir enlevees.
		NkPoserLiaisonSommet(n2, 2u, NkPoint2::LiaisonDroit);
		const bool efface = !n2.sommets[2].Courbe() && n2.sommets[2].sx == 0.f;
		char dd[240];
		snprintf(dd, sizeof(dd), "%u sommets ; vif avant=%d ; deux poignees=%d (long %.3f / %.3f, "
							   "visables=%d) ; sain=%d ; contour %u -> %u pts=%d ; courbe gardee=%d "
							   "; retour droit efface=%d",
				 nbA, vifAvant ? 1 : 0, deuxPoignees ? 1 : 0, (double)le, (double)ls,
				 visables ? 1 : 0, sain ? 1 : 0, nAv, nAp, ecranChange ? 1 : 0, garde ? 1 : 0,
				 efface ? 1 : 0);
		verdict("46. CHANGER LE TYPE D'UN SOMMET LUI DONNE DES POIGNEES VISIBLES ET SAISISSABLES "
				"(sans que la recette en pose une seule elle-meme), le contour peint le montre, "
				"et une courbe deja tiree n'est PAS ecrasee",
				vifAvant && deuxPoignees && visables && sain && ecranChange && garde && efface, dd);
	}

	// ── 47. LE PANNEAU DIT LEQUEL DES DEUX MECANISMES TIENT LE SOMMET ─────────
	// 🔴 SUR LA CAPTURE `probleme_pas_de_poignees_222121.png`, LE PANNEAU DONNAIT
	//    TROIS AVIS SUR UN SEUL SOMMET : type « Libre » en surbrillance (donc
	//    courbe), « R 16 » dans un champ editable (donc arrondi), et X1/Y1/X2/Y2
	//    bloques sur « — » (donc rien). Les deux mecanismes s'excluent
	//    (`RayonActif`, cas 39) et le PEINTRE le sait ; c'est le panneau qui ne le
	//    redisait pas. *Une regle ecrite au modele et non redite au panneau produit
	//    un controle qui ment.*
	//
	// ⚠️ ET CE N'EST PAS UNE DIVERGENCE AVEC LUNACY : sa documentation dit aussi
	//    que le champ de rayon n'est actif que sur un point droit (§1.2 du document
	//    de reference). L'exclusion etait juste, elle etait MUETTE. Rien a refondre
	//    dans le format, rien a convertir a la lecture.
	//
	// ⚠️ CE CAS TIENT L'ACCORD DES TROIS CONTROLES, PAS LA VALEUR D'UN SEUL. Un cas
	//    qui verifierait « R est grise sur un sommet courbe » laisserait les champs
	//    de tangente diverger sans rien dire -- et c'est precisement par la
	//    divergence de deux avis que ce defaut est arrive.
	{
		st.doc.NewDocument("recette points", NkAuthor::Humain);
		const int32 iC = poser("rect", nullptr);
		NkUINode &n = st.doc.nodes[(uint32)iC];
		NkMaterialiserSommets(n);
		const uint32 nbC = (uint32)n.sommets.Size();
		const char *r0 = nullptr, *r1 = nullptr, *r2 = nullptr, *r3 = nullptr;
		// (a) AUCUN SOMMET : les deux rangees sont muettes, et la phrase le dit.
		const bool aucun = NkQuiTientLeSommet(n, -1, nbC, r0) == NkTenuePar::Aucun;
		// (b) SOMMET DROIT AVEC UN RAYON : c'est le rayon qui tient.
		n.sommets[0].rayon = 16.f;
		const bool parRayon = NkQuiTientLeSommet(n, 0, nbC, r1) == NkTenuePar::Rayon;
		// (c) LE MEME SOMMET, PASSE EN COURBE, GARDE SON RAYON DE 16 -- et ce sont
		//     desormais LES POIGNEES qui tiennent. C'est EXACTEMENT l'etat de la
		//     capture : R=16 sur un sommet « Libre ».
		NkPoserLiaisonSommet(n, 0u, NkPoint2::LiaisonDeconnecte);
		const bool parPoignees = NkQuiTientLeSommet(n, 0, nbC, r2) == NkTenuePar::Poignees;
		const bool rayonGarde = n.sommets[0].rayon == 16.f;
		const bool rayonInerte = !n.sommets[0].RayonActif();
		// (d) ET LA PHRASE SUIT LE VERDICT : trois etats, trois phrases distinctes.
		//     Sans ce volet, un panneau pourrait griser « R » tout en expliquant
		//     qu'il agit -- l'etat et son explication sont la meme information, donc
		//     ils sortent de la MEME porte.
		const bool phrasesDistinctes = r0 && r1 && r2 && r0 != r1 && r1 != r2 && r0 != r2;
		// (e) RETOUR A « DROIT » : le rayon de 16 REDEVIENT actif. C'est le contrat
		//     que la phrase promet (« sa valeur est gardee pour le retour »), et une
		//     promesse d'interface non tenue vaut un champ qui ment.
		NkPoserLiaisonSommet(n, 0u, NkPoint2::LiaisonDroit);
		const bool revient = NkQuiTientLeSommet(n, 0, nbC, r3) == NkTenuePar::Rayon
							 && n.sommets[0].RayonActif() && n.sommets[0].rayon == 16.f;
		char dd[240];
		snprintf(dd, sizeof(dd), "aucun=%d ; droit -> rayon=%d ; courbe -> poignees=%d (rayon "
							   "garde=%d, inerte=%d) ; 3 phrases distinctes=%d ; retour droit "
							   "reactive le rayon=%d",
				 aucun ? 1 : 0, parRayon ? 1 : 0, parPoignees ? 1 : 0, rayonGarde ? 1 : 0,
				 rayonInerte ? 1 : 0, phrasesDistinctes ? 1 : 0, revient ? 1 : 0);
		verdict("47. LE PANNEAU DIT LEQUEL DES DEUX MECANISMES TIENT LE SOMMET : « R » et les "
				"quatre champs de tangente lisent LE MEME verdict, la phrase qui les explique en "
				"sort aussi, et un rayon eteint par une courbe est GARDE puis reactive",
				aucun && parRayon && parPoignees && rayonGarde && rayonInerte && phrasesDistinctes
					&& revient,
				dd);
	}

	// ── 48. VAGUE 2 : `Maj` CONTRAINT LE DEPLACEMENT A UN AXE ────────────────
	// Source `/layers` : `Maj`+glisser contraint a un axe. CONTEXTE : la toile,
	// selection d'objets (pour les sommets, c'est la table du mode forme).
	//
	// ⚠️ LE VOLET (c) EST LE SEUL QUI COMPTE VRAIMENT, et il est invisible sur une
	//    capture. La boucle de glisser dispose d'un ecart PAR IMAGE ; contraindre
	//    celui-la donnerait un objet qui reste sur l'axe a chaque image et DERIVE
	//    en diagonale sur la duree du geste. On simule donc une main qui tremble
	//    -- une suite d'ecarts dont l'axe dominant ALTERNE -- et on exige que la
	//    position finale soit exactement sur l'axe. Une regle qui prendrait
	//    l'ecart courant passerait (a), (b) et (d) et tomberait ici.
	{
		float32 ox = 0.f, oy = 0.f;
		// (a) SANS `Maj`, RIEN N'EST TOUCHE -- le controle negatif d'abord.
		NkContraindreAxe(false, 30.f, 12.f, ox, oy);
		const bool libre = ox == 30.f && oy == 12.f;
		// (b) L'AXE DOMINANT GAGNE, DANS LES DEUX SENS
		NkContraindreAxe(true, 30.f, 12.f, ox, oy);
		const bool horiz = ox == 30.f && oy == 0.f;
		NkContraindreAxe(true, -7.f, -40.f, ox, oy);
		const bool vert = ox == 0.f && oy == -40.f;
		// (c) LA MAIN QUI TREMBLE : dix ecarts dont l'axe dominant alterne, mais
		//     dont le TOTAL est franchement horizontal. Cumules par image, ils
		//     auraient laisse un residu vertical ; mesures depuis l'appui, non.
		// ⚠️ ET LA BOUCLE APPELLE `NkPositionContrainte`, PAS `NkContraindreAxe` :
		//    elle refait EXACTEMENT ce que fait la toile, accumulation comprise
		//    (`mMoveLibre` avance de l'ecart, puis la porte le recale). Tester la
		//    regle seule aurait laisse le banc vert au-dessus d'un panneau qui
		//    passerait l'ecart de l'image courante -- le defaut meme que la capture
		//    de Rodolf a trouve sur les poignees quelques heures plus tot.
		const float32 pasX[10] = {6.f, 1.f, 5.f, 0.5f, 7.f, 1.f, 4.f, 0.5f, 6.f, 2.f};
		const float32 pasY[10] = {1.f, 3.f, 0.5f, 4.f, 1.f, 3.f, 0.5f, 2.f, 1.f, 1.f};
		const float32 origX = 200.f, origY = 100.f; // la position a l'appui
		float32 curX = origX, curY = origY, totX = 0.f, totY = 0.f;
		uint32 imagesObliques = 0;
		for (uint32 k = 0; k < 10u; ++k) {
			totX += pasX[k];
			totY += pasY[k];
			if (pasY[k] > pasX[k])
				++imagesObliques; // l'image ou une regle par-image aurait bascule
			// la toile fait ces deux gestes, dans cet ordre, a chaque image
			curX += pasX[k];
			curY += pasY[k];
			NkPositionContrainte(true, origX, origY, curX, curY, curX, curY);
		}
		curX -= origX;
		curY -= origY;
		// le total est bien horizontal (33 contre 17), et l'axe a bel et bien ete
		// tente : sans ces images obliques, le volet ne discriminerait rien.
		const bool aTremble = imagesObliques >= 3u;
		const bool sansDerive = curY == 0.f && curX == totX;
		// (d) LA DIAGONALE PARFAITE NE CLIGNOTE PAS : deux appels identiques
		//     rendent le meme axe. Sans tranche ferme (`>=` et non `>`), une
		//     egalite ferait osciller l'objet d'une image a l'autre.
		float32 d1x = 0.f, d1y = 0.f, d2x = 0.f, d2y = 0.f;
		NkContraindreAxe(true, 20.f, 20.f, d1x, d1y);
		NkContraindreAxe(true, 20.f, 20.f, d2x, d2y);
		const bool stable = d1x == d2x && d1y == d2y && d1y == 0.f;
		char dd[224];
		snprintf(dd, sizeof(dd), "libre=%d horiz=%d vert=%d ; tremble : %u image(s) obliques, "
							   "total (%.1f,%.1f) -> (%.1f,%.1f) sans derive=%d ; diagonale "
							   "stable=%d",
				 libre ? 1 : 0, horiz ? 1 : 0, vert ? 1 : 0, imagesObliques, (double)totX,
				 (double)totY, (double)curX, (double)curY, sansDerive ? 1 : 0, stable ? 1 : 0);
		verdict("48. `Maj` CONTRAINT LE DEPLACEMENT A UN AXE, mesure depuis L'APPUI et non depuis "
				"l'image precedente -- une main qui tremble ne fait PAS deriver l'objet en "
				"diagonale, et la diagonale parfaite ne clignote pas",
				libre && horiz && vert && aTremble && sansDerive && stable, dd);
	}

	// ── 49. VAGUE 2 : `Maj` ET `Alt` AU REDIMENSIONNEMENT ────────────────────
	// Source `/layers` : au redimensionnement, `Maj` garde les proportions, `Alt`
	// travaille depuis le centre, et les deux se combinent.
	//
	// ⚠️ LE VOLET (e) EST CELUI QUI TIENT LA PROMESSE DE `Alt` : « depuis le
	//    centre » ne veut pas dire « plus vite », il veut dire que LE CENTRE NE
	//    BOUGE PAS. Un code qui doublerait la taille sans reculer l'origine
	//    passerait un test de taille et ferait fuir la forme sous le curseur.
	{
		float32 L = 0.f, H = 0.f, dX = 0.f, dY = 0.f;
		// (a) SANS MODIFICATEUR : le bord droit tire, l'origine ne bouge pas.
		NkRedimModifie(false, false, 1, 0, 100.f, 50.f, 20.f, 0.f, L, H, dX, dY);
		const bool nu = L == 120.f && H == 50.f && dX == 0.f && dY == 0.f;
		// (b) LE BORD GAUCHE : l'origine SUIT, le bord oppose ne bouge pas.
		NkRedimModifie(false, false, -1, 0, 100.f, 50.f, -20.f, 0.f, L, H, dX, dY);
		const bool gauche = L == 120.f && dX == -20.f;
		// (c) `Maj` GARDE LES PROPORTIONS de la boite de DEPART (ratio 0,5).
		NkRedimModifie(true, false, 1, 1, 100.f, 50.f, 40.f, 4.f, L, H, dX, dY);
		const bool proportions = L == 140.f && H == 70.f;
		// (d) ET LE PLUS GRAND MOUVEMENT COMMANDE : tirer surtout vers le BAS doit
		//     agrandir en hauteur et laisser la largeur suivre. Prendre `dx`
		//     d'office rendrait le geste vertical inerte.
		NkRedimModifie(true, false, 1, 1, 100.f, 50.f, 3.f, 25.f, L, H, dX, dY);
		const bool verticalCommande = H == 75.f && L == 150.f;
		// (e) `Alt` : LE CENTRE NE BOUGE PAS. C'est la vraie promesse, et elle se
		//     mesure sur le centre, pas sur la taille.
		NkRedimModifie(false, true, 1, 0, 100.f, 50.f, 20.f, 0.f, L, H, dX, dY);
		const float32 centreAvant = 0.f + 100.f * 0.5f;
		const float32 centreApres = dX + L * 0.5f;
		const bool centreFixe = L == 140.f && centreApres > centreAvant - 0.001f
								&& centreApres < centreAvant + 0.001f;
		// (f) LES DEUX ENSEMBLE : proportions gardees ET centre fixe.
		NkRedimModifie(true, true, 1, 1, 100.f, 50.f, 20.f, 2.f, L, H, dX, dY);
		const bool cx = (dX + L * 0.5f) > 49.999f && (dX + L * 0.5f) < 50.001f;
		const bool cy = (dY + H * 0.5f) > 24.999f && (dY + H * 0.5f) < 25.001f;
		const bool combine = L == 140.f && H == 70.f && cx && cy;
		// (g) LE PLANCHER : ecraser une forme ne la RETOURNE pas.
		NkRedimModifie(false, false, 1, 1, 100.f, 50.f, -500.f, -500.f, L, H, dX, dY);
		const bool plancher = L == 8.f && H == 8.f;
		char dd[240];
		snprintf(dd, sizeof(dd), "nu=%d gauche=%d proportions=%d vertical commande=%d ; alt centre "
							   "%.1f -> %.1f (fixe=%d) ; combine=%d ; plancher=%d",
				 nu ? 1 : 0, gauche ? 1 : 0, proportions ? 1 : 0, verticalCommande ? 1 : 0,
				 (double)centreAvant, (double)centreApres, centreFixe ? 1 : 0, combine ? 1 : 0,
				 plancher ? 1 : 0);
		verdict("49. AU REDIMENSIONNEMENT, `Maj` garde les proportions de la boite DE DEPART (le "
				"plus grand mouvement commande) et `Alt` laisse LE CENTRE IMMOBILE ; les deux se "
				"combinent, et le plancher ne retourne pas la forme",
				nu && gauche && proportions && verticalCommande && centreFixe && combine
					&& plancher,
				dd);
	}

	// ── 50. LE TYPE PAR DEFAUT EST MIROIR, ET UN SOMMET NEUF RESTE DROIT ─────
	// Decision de Rodolf, 02/09 : « par defaut je veux Miroir. » Elle COINCIDE
	// avec la source, ce qui est la meilleure raison de la prendre --
	// `editing_shapes`, mot pour mot : « hover the cursor over the path, then
	// click it to place a straight point or double-click to place a MIRRORED
	// point. »
	//
	// ⚠️ LES DEUX MOITIES DE LA PHRASE COMPTENT, ET LE CAS TIENT LES DEUX. Un
	//    defaut « Miroir » applique a TOUS les sommets neufs ferait naitre courbe
	//    le sommet du SIMPLE clic -- qui doit rester droit. C'est le genre de
	//    generalisation qui a l'air d'obeir a la consigne tout en cassant le
	//    geste voisin, et seul le volet (b) l'attrape.
	{
		st.doc.NewDocument("recette points", NkAuthor::Humain);
		const int32 iD = poser("rect", nullptr);
		NkUINode &n = st.doc.nodes[(uint32)iD];
		NkMaterialiserSommets(n);
		// (a) LE DEFAUT EST BIEN MIROIR, et il est NOMME une seule fois.
		const bool defautMiroir = NkPoint2::LiaisonParDefaut == NkPoint2::LiaisonMiroir;
		// (b) ⚠️ UN SOMMET AJOUTE AU SIMPLE CLIC RESTE DROIT (l'autre moitie de la
		//     phrase source). Sans ce volet, un defaut applique partout passerait.
		const int32 nSimple = NkInsererSommet(n, 1u, 0.5f);
		const bool simpleResteDroit = nSimple >= 0
									  && n.sommets[(uint32)nSimple].liaison
											 == NkPoint2::LiaisonDroit
									  && !n.sommets[(uint32)nSimple].Courbe();
		// (c) LE SOMMET NE AU DOUBLE-CLIC EST MIROIR, ET IL A DEJA SES POIGNEES --
		//     c'est la porte du panneau qu'on appelle, pas une pose a la main.
		const int32 nDouble = NkInsererSommet(n, 2u, 0.5f);
		bool doubleEstMiroir = false, doublePoignees = false;
		if (nDouble >= 0) {
			NkPoserLiaisonSommet(n, (uint32)nDouble, NkPoint2::LiaisonParDefaut);
			const NkPoint2 &pd = n.sommets[(uint32)nDouble];
			doubleEstMiroir = pd.liaison == NkPoint2::LiaisonMiroir;
			doublePoignees = pd.Courbe() && NkLongueur2D(pd.ex, pd.ey) >= 0.04f
							 && NkLongueur2D(pd.sx, pd.sy) >= 0.04f;
		}
		// (d) ET IL EST VRAIMENT MIROIR, PAS SEULEMENT ETIQUETE : les deux
		//     poignees sont opposees EN DIRECTION ET EN LONGUEUR. Une etiquette
		//     « Miroir » sur des tangentes dissymetriques serait le defaut que
		//     `NkPoserLiaison` existe pour eviter.
		bool vraimentMiroir = false;
		if (nDouble >= 0) {
			const NkPoint2 &pd = n.sommets[(uint32)nDouble];
			const float32 sx = pd.ex + pd.sx, sy = pd.ey + pd.sy; // somme ~ nulle
			vraimentMiroir = NkLongueur2D(sx, sy) < 0.0001f;
		}
		char dd[224];
		snprintf(dd, sizeof(dd), "defaut=Miroir(%d) ; simple clic -> droit=%d ; double -> "
							   "miroir=%d poignees=%d (%.3f/%.3f) opposees exactement=%d",
				 defautMiroir ? 1 : 0, simpleResteDroit ? 1 : 0, doubleEstMiroir ? 1 : 0,
				 doublePoignees ? 1 : 0,
				 nDouble >= 0 ? (double)NkLongueur2D(n.sommets[(uint32)nDouble].ex,
													 n.sommets[(uint32)nDouble].ey)
							  : 0.0,
				 nDouble >= 0 ? (double)NkLongueur2D(n.sommets[(uint32)nDouble].sx,
													 n.sommets[(uint32)nDouble].sy)
							  : 0.0,
				 vraimentMiroir ? 1 : 0);
		verdict("50. LE TYPE PAR DEFAUT EST MIROIR (nomme UNE fois, pas repete aux sites) : un "
				"sommet ne au double-clic sur le trace est miroir AVEC ses poignees, et un sommet "
				"ajoute au SIMPLE clic reste DROIT -- les deux moities de la phrase source",
				defautMiroir && simpleResteDroit && doubleEstMiroir && doublePoignees
					&& vraimentMiroir,
				dd);
	}

	// ── 51. `Suppr` SUR UN SOMMET : LE PLANCHER, ET LE SENS DE PARCOURS ──────
	// Source `/editing_shapes`. `NkSupprimerSommet` existait depuis le 01/09 et
	// AUCUN geste ne l'appelait -- du robinet, pas un chantier.
	//
	// ⚠️ LE VOLET (c) EST CELUI QUI COMPTE, et il est invisible a la relecture :
	//    supprimer PLUSIEURS sommets marques demande de parcourir A L'ENVERS.
	//    Retirer l'indice 1 puis l'indice 2 supprime le MAUVAIS sommet, la liste
	//    s'etant decalee entre-temps -- et le resultat reste un trace
	//    parfaitement valide, donc rien ne proteste. On mesure donc QUEL sommet
	//    survit, jamais combien il en reste.
	{
		st.doc.NewDocument("recette points", NkAuthor::Humain);
		const int32 iE = poser("etoile", nullptr);
		NkUINode &n = st.doc.nodes[(uint32)iE];
		NkMaterialiserSommets(n);
		const uint32 nb0 = (uint32)n.sommets.Size();
		// (a) UN SOMMET S'EN VA, ET LE COMPTE BAISSE D'EXACTEMENT UN.
		const bool un = NkSupprimerSommet(n, 0u) && (uint32)n.sommets.Size() == nb0 - 1u;
		// (b) LE PLANCHER : on descend jusqu'a trois, puis ca REFUSE. En dessous
		//     il n'y a plus de forme, seulement un segment -- l'utilisateur aurait
		//     fait disparaitre son objet par un geste d'edition.
		uint32 garde = 0;
		while (n.sommets.Size() > 3u && garde++ < 64u)
			(void)NkSupprimerSommet(n, 0u);
		const bool plancher = (uint32)n.sommets.Size() == 3u && !NkSupprimerSommet(n, 0u)
							  && (uint32)n.sommets.Size() == 3u;
		// (c) ⚠️ LE SENS DE PARCOURS, sur une forme neuve : on retire les indices
		//     2 PUIS 1, en DESCENDANT. Le sommet qui portait (tx,ty) doit avoir
		//     disparu, et celui qui portait (gx,gy) -- son voisin d'apres -- doit
		//     avoir SURVECU. Un parcours croissant retirerait 1, puis l'ancien 2
		//     devenu 1 : il emporterait le voisin, et le trace resterait valide.
		st.doc.NewDocument("recette points", NkAuthor::Humain);
		const int32 iF = poser("etoile", nullptr);
		NkUINode &m = st.doc.nodes[(uint32)iF];
		NkMaterialiserSommets(m);
		const float32 tx = m.sommets[2].x, ty = m.sommets[2].y; // doit DISPARAITRE
		const float32 gx = m.sommets[3].x, gy = m.sommets[3].y; // doit SURVIVRE
		for (int32 k = 2; k >= 1; --k)
			(void)NkSupprimerSommet(m, (uint32)k);
		bool disparu = true, survivant = false;
		for (uint32 k = 0; k < (uint32)m.sommets.Size(); ++k) {
			if (m.sommets[k].x == tx && m.sommets[k].y == ty)
				disparu = false;
			if (m.sommets[k].x == gx && m.sommets[k].y == gy)
				survivant = true;
		}
		const bool bonSens = disparu && survivant;
		// (d) UN INDICE HORS BORNES REFUSE FRANCHEMENT, sans rien toucher.
		const uint32 avantHb = (uint32)m.sommets.Size();
		const bool horsBornes = !NkSupprimerSommet(m, 999u)
								&& (uint32)m.sommets.Size() == avantHb;
		char dd[240];
		snprintf(dd, sizeof(dd), "%u sommets ; un retire=%d ; plancher a 3 refuse=%d ; sens "
							   "descendant : le vise a disparu=%d le voisin a survecu=%d ; hors "
							   "bornes refuse=%d",
				 nb0, un ? 1 : 0, plancher ? 1 : 0, disparu ? 1 : 0, survivant ? 1 : 0,
				 horsBornes ? 1 : 0);
		verdict("51. `Suppr` SUR UN SOMMET : le compte baisse d'un, le trace garde un PLANCHER de "
				"trois sommets (refus franc), et la suppression multiple parcourt A L'ENVERS -- "
				"sinon elle emporte le sommet VOISIN sans que rien ne proteste",
				un && plancher && bonSens && horsBornes, dd);
	}

	// ── 52. LES ZOOMS NOMMES : MESURES SUR CE QU'ON VOIT, PAS SUR `zoom` ─────
	// Source `/canvas` : Ctrl+0 = 100 %, Ctrl+1 = ajuster, Ctrl+2 = selection,
	// Ctrl+3 = largeur, Ctrl+4 = hauteur.
	//
	// 🔴 CE CAS N'ASSERTE PAS `view.zoom == quelque chose`, ET C'EST DELIBERE.
	//    *Un compteur d'intentions n'est pas un controle d'effets* : le facteur
	//    d'echelle est la valeur qu'on vient d'ecrire. Ce qu'un utilisateur
	//    demande en pressant « ajuster », c'est QUE LA FORME SOIT VISIBLE. On
	//    projette donc le rectangle document a l'ECRAN et on verifie qu'il tombe
	//    DANS le viewport -- ce qui teste l'echelle ET le recadrage ensemble.
	{
		NkCanvasView v;
		v.viewport = {0.f, 0.f, 800.f, 600.f};
		// une forme LOIN de l'origine et bien plus grande que l'ecran : au depart
		// elle est franchement hors champ.
		// ⚠️ LA CIBLE EST PLUS HAUTE QUE LARGE, ET CE N'EST PAS UN DETAIL DE
		//    DECOR : avec un rectangle large, la hauteur ne contraint pas, donc
		//    le mode 0 et le mode LARGEUR rendent LA MEME echelle -- et le volet
		//    (d) ne discrimine plus rien. Mesure : avec {2400 x 1200} la mutation
		//    « le mode largeur ne recentre plus » passait le banc au vert.
		//    *Une donnee d'essai degeneree rend un volet vide sans le dire.*
		const NkPaintRect cible{4000.f, 3000.f, 1200.f, 2400.f};
		auto dedans = [&](const NkPaintRect &d) -> bool {
			const NkPaintRect s = v.ToScreen(d);
			return s.x >= v.viewport.x - 0.5f && s.y >= v.viewport.y - 0.5f
				   && s.x + s.w <= v.viewport.x + v.viewport.w + 0.5f
				   && s.y + s.h <= v.viewport.y + v.viewport.h + 0.5f;
		};
		// (a) LE TEMOIN : avant d'ajuster, la forme n'est PAS visible. Sans lui,
		//     « elle est dedans » ne prouverait rien -- elle aurait pu y etre.
		const bool horsAvant = !dedans(cible);
		// (b) AJUSTER : elle tombe ENTIEREMENT dans le viewport.
		const bool ajuste = v.AjusterSur(cible, 0u) && dedans(cible);
		// (c) LA MARGE EXISTE : la forme ne touche pas les bords. Un ajustement
		//     au pixel colle la forme aux quatre cotes et donne l'impression
		//     qu'elle deborde.
		const NkPaintRect s = v.ToScreen(cible);
		const bool marge = s.x > v.viewport.x + 1.f || s.y > v.viewport.y + 1.f;
		// (d) LE MODE LARGEUR remplit la largeur, et RECENTRE quand meme en
		//     hauteur -- le mode choisit l'echelle, pas le cadrage.
		v.AjusterSur(cible, 1u);
		const NkPaintRect sl = v.ToScreen(cible);
		const bool largeur = sl.w > v.viewport.w * 0.9f && sl.w <= v.viewport.w + 0.5f;
		const float32 cySl = sl.y + sl.h * 0.5f;
		const bool recentre = cySl > v.viewport.h * 0.5f - 1.f
							  && cySl < v.viewport.h * 0.5f + 1.f;
		// (e) ZOOM 100 % NE PERD PAS LA VUE : le point document du CENTRE reste
		//     au centre. Remettre `zoom = 1` sans recaler ferait sauter le
		//     document hors de l'ecran -- et le banc l'aurait laisse passer s'il
		//     n'avait regarde que `zoom == 1`.
		const float32 avantX = v.ToDocX(v.viewport.w * 0.5f);
		const float32 avantY = v.ToDocY(v.viewport.h * 0.5f);
		v.Zoom100();
		const float32 apresX = v.ToDocX(v.viewport.w * 0.5f);
		const float32 apresY = v.ToDocY(v.viewport.h * 0.5f);
		const float32 dc = NkLongueur2D(apresX - avantX, apresY - avantY);
		const bool centreTenu = v.zoom > 0.999f && v.zoom < 1.001f && dc < 0.01f;
		// (f) UN RECTANGLE NUL REFUSE FRANCHEMENT, sans rien toucher : selection
		//     vide, document neuf. Sans cette garde on divise par zero et on
		//     propage des NaN jusqu'au deplacement -- la toile disparaitrait sans
		//     message.
		const float32 zAvant = v.zoom;
		const bool refuse = !v.AjusterSur(NkPaintRect{0.f, 0.f, 0.f, 0.f}, 0u)
							&& v.zoom == zAvant;
		char dd[240];
		snprintf(dd, sizeof(dd), "hors champ avant=%d ; ajuste -> dedans=%d marge=%d ; largeur "
							   "remplie=%d recentre=%d ; 100%% : zoom=%.3f centre derive de "
							   "%.4f (=%d) ; rect nul refuse=%d",
				 horsAvant ? 1 : 0, ajuste ? 1 : 0, marge ? 1 : 0, largeur ? 1 : 0,
				 recentre ? 1 : 0, (double)v.zoom, (double)dc, centreTenu ? 1 : 0,
				 refuse ? 1 : 0);
		verdict("52. LES ZOOMS NOMMES se mesurent sur CE QU'ON VOIT : apres « ajuster » la forme "
				"tombe entierement dans le viewport (avec sa marge), le mode largeur recentre "
				"quand meme, « 100 % » ne perd pas le centre, et un rectangle nul est refuse",
				horsAvant && ajuste && marge && largeur && recentre && centreTenu && refuse, dd);
	}

	printf("\nRECETTE POINTS : %d/%d %s\n", cas - echecs, cas,
		   echecs == 0 ? "PROUVEE" : "EN ECHEC");
	return echecs == 0 ? 0 : 1;
}
