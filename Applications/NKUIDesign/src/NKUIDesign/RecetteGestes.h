#pragma once
// -----------------------------------------------------------------------------
// @File    RecetteGestes.h
// @Brief   La recette « Gestes » — sortie de `main.cpp` le 28/09/2026.
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

static nkentseu::int32 RecetteGestes() {
	using namespace nkuidesign;
	using nkentseu::int32;
	using nkentseu::uint32;
	auto ser = [](DesignState &s) {
		NkString o;
		s.doc.Save(o);
		return o;
	};
	auto identiques = [](const NkString &a, const NkString &b) -> bool {
		const char *x = a.Data() ? a.Data() : "";
		const char *y = b.Data() ? b.Data() : "";
		while (*x && *x == *y) {
			++x;
			++y;
		}
		return *x == *y;
	};
	auto stabiliser = [&](DesignState &s) {
		for (int32 i = 0; i < 10; ++i) {
			NkString o;
			s.doc.Save(o);
			s.histoire.Observer(o);
		}
	};
	int32 cas = 0, echecs = 0;
	auto verdict = [&](const char *nom, bool ok, const char *detail) {
		++cas;
		printf("%s  %s%s%s\n", ok ? "OK   " : "ECHEC", nom, (detail && *detail) ? "  -- " : "",
			   (detail && *detail) ? detail : "");
		if (!ok)
			++echecs;
	};
	static DesignState st; // static : l'etat est gros, pas sur la pile
	// La surface de disposition : la MEME que la toile appelle (espace
	// document). Grouper lit `layout` — sans elle il refuse, et il le dit.
	const NkPaintRect surface = {0.f, 0.f, 1200.f, 800.f};
	auto poser = [&](int32 parent, const char *nom, float32 x, float32 y, float32 w,
					 float32 h) -> int32 {
		const int32 i = st.doc.AddChild(parent, "", NkAuthor::Humain);
		NkUINode &n = st.doc.nodes[(uint32)i];
		n.label = NkString(nom);
		n.shape = NkString("rect");
		n.posX = x;
		n.posY = y;
		n.width.mode = NkSizeMode::Fixed;
		n.width.value = w;
		n.height.mode = NkSizeMode::Fixed;
		n.height.value = h;
		st.doc.MarkHumanEdit(i);
		return i;
	};
	auto compteLabel = [&](const char *nom) -> int32 {
		int32 n = 0;
		for (uint32 i = 0; i < (uint32)st.doc.nodes.Size(); ++i)
			if (st.doc.nodes[i].label.Data() && NkComponentDecl::StrEq(st.doc.nodes[i].label.Data(), nom))
				++n;
		return n;
	};

	// ── 1. COPIER + COLLER : le sous-arbre revient ENTIER, et une seule fois
	st.doc.NewDocument("recette gestes", NkAuthor::Humain);
	{
		const int32 pere = poser(0, "Pere", 10.f, 10.f, 200.f, 200.f);
		st.doc.nodes[(uint32)pere].layout.kind = NkLayoutKind::Free;
		poser(pere, "Fils", 5.f, 5.f, 40.f, 40.f);
		st.Recompute(surface);
		st.SelectSingle(pere);
		const uint32 nCopies = st.CopierSelection();
		st.CollerPressePapiers();
		st.Recompute(surface);
		char d[128];
		nkentseu::NkSnprintf(d, sizeof(d), "copies=%u, Pere x%d, Fils x%d", nCopies, compteLabel("Pere"),
				 compteLabel("Fils"));
		verdict("copier/coller : le sous-arbre revient ENTIER (pere + fils)",
				nCopies == 1 && compteLabel("Pere") == 2 && compteLabel("Fils") == 2, d);
	}
	// ── 2. LE PIEGE DU DOUBLON : pere ET fils selectionnes -> le fils ne se
	//    colle PAS deux fois (RacinesSelection). Sans ce filtre : Fils x4.
	st.doc.NewDocument("recette gestes", NkAuthor::Humain);
	{
		const int32 pere = poser(0, "Pere", 10.f, 10.f, 200.f, 200.f);
		st.doc.nodes[(uint32)pere].layout.kind = NkLayoutKind::Free;
		const int32 fils = poser(pere, "Fils", 5.f, 5.f, 40.f, 40.f);
		st.Recompute(surface);
		st.sel.Set(pere);
		st.sel.Add(fils); // la selection COUVRANTE, celle qui piege
		st.selected = st.sel.Primary();
		st.CopierSelection();
		st.CollerPressePapiers();
		char d[96];
		nkentseu::NkSnprintf(d, sizeof(d), "Pere x%d, Fils x%d (x4 = le doublon)", compteLabel("Pere"),
				 compteLabel("Fils"));
		verdict("copier pere+fils : le fils n'arrive PAS en double",
				compteLabel("Pere") == 2 && compteLabel("Fils") == 2, d);
	}
	// ── 2bis. LA COPIE NE PERD AUCUN CHAMP, et c'est mesure PAR LE FORMAT.
	//    ⚠️ C'EST LE DEFAUT QUE LES AUTRES CAS NE PEUVENT PAS VOIR. Compter des
	//    noeuds prouve qu'il y en a deux ; ca ne dit RIEN d'un `fontWeight` que
	//    `CopierSousArbre` aurait oublie de recopier — le collage aurait l'air
	//    juste et perdrait une propriete en silence, exactement le « collage qui
	//    perd » que l'avertissement de la methode annonce. On ecrit donc un
	//    noeud RICHE, on le colle, et on compare les deux SERIALISATIONS ligne a
	//    ligne : le juge vient du round-trip, pas du code teste (lecon T5).
	//    Ce cas echouera le jour ou un champ neuf de NkUINode oubliera la copie —
	//    c'est le rappel qu'on veut, et il est automatique.
	st.doc.NewDocument("recette gestes", NkAuthor::Humain);
	{
		const int32 a = poser(0, "Riche", 12.f, 34.f, 111.f, 22.f);
		{
			NkUINode &n = st.doc.nodes[(uint32)a];
			n.shape = NkString("text");
			n.text = NkString("Bonjour");
			n.fontPx = 19.f;
			n.fontWeight = 700;
			n.radius = 7.f;
			n.borderW = 3.f;
			n.role = NkString("titre");
			n.alignText = 2;
			st.doc.MarkHumanEdit(a);
		}
		st.Recompute(surface);
		st.SelectSingle(a);
		st.CopierSelection();
		st.CollerPressePapiers();
		const int32 b = st.selected;
		// ⚠️ LE JUGE N'EMPLOIE PAS `CopierSousArbre`, ET C'EST TOUTE LA QUESTION.
		//    Isoler les deux noeuds AVEC la copie aurait ete un controle de
		//    SYMETRIE : un champ oublie par la copie serait oublie des DEUX
		//    cotes, les deux serialisations resteraient identiques, et le cas
		//    passerait au vert en couvrant precisement le defaut qu'il cherche.
		//    (Le depot a deja paye cette lecon — Q14, « un controle de symetrie
		//    ne peut pas voir une erreur symetrique ».) On isole donc par
		//    Save/Load/RemoveSubtree : trois mecanismes prouves ailleurs, et
		//    aucun d'eux n'est le code teste.
		bool ok = st.doc.IsValidIndex(b) && b != a;
		NkString sa, sb;
		if (ok) {
			st.doc.nodes[(uint32)b].posX = st.doc.nodes[(uint32)a].posX;
			st.doc.nodes[(uint32)b].posY = st.doc.nodes[(uint32)a].posY;
			NkString tout;
			st.doc.Save(tout);
			NkUIDocument da, db;
			ok = da.Load(tout.Data()) && db.Load(tout.Data());
			if (ok) {
				// `a` et `b` sont les deux seuls enfants de la racine : on garde
				// le premier d'un cote, le second de l'autre.
				ok = da.nodes[0].children.Size() == 2 && db.nodes[0].children.Size() == 2;
				if (ok) {
					ok = da.RemoveSubtree(da.nodes[0].children[1])
						 && db.RemoveSubtree(db.nodes[0].children[0]);
				}
			}
			if (ok) {
				da.Save(sa);
				db.Save(sb);
				ok = identiques(sa, sb);
			}
		}
		char d[96];
		nkentseu::NkSnprintf(d, sizeof(d), "%u vs %u octets, %s", (unsigned)(sa.Data() ? strlen(sa.Data()) : 0),
				 (unsigned)(sb.Data() ? strlen(sb.Data()) : 0),
				 ok ? "serialisations IDENTIQUES" : "UN CHAMP MANQUE");
		verdict("la copie ne perd AUCUN champ (comparaison par le format)", ok, d);
	}
	// ── 3. COLLER DECALE de 10 px (sinon le collage disparait SOUS l'original)
	st.doc.NewDocument("recette gestes", NkAuthor::Humain);
	{
		const int32 a = poser(0, "Boite", 40.f, 60.f, 80.f, 30.f);
		st.Recompute(surface);
		st.SelectSingle(a);
		st.CopierSelection();
		st.CollerPressePapiers();
		const int32 b = st.selected;
		const bool ok = st.doc.IsValidIndex(b) && b != a
						&& st.doc.nodes[(uint32)b].posX == st.doc.nodes[(uint32)a].posX + 10.f
						&& st.doc.nodes[(uint32)b].posY == st.doc.nodes[(uint32)a].posY + 10.f;
		char d[96];
		nkentseu::NkSnprintf(d, sizeof(d), "original (%.0f,%.0f) -> colle (%.0f,%.0f)",
				 st.doc.nodes[(uint32)a].posX, st.doc.nodes[(uint32)a].posY,
				 st.doc.IsValidIndex(b) ? st.doc.nodes[(uint32)b].posX : -1.f,
				 st.doc.IsValidIndex(b) ? st.doc.nodes[(uint32)b].posY : -1.f);
		verdict("coller : decale de 10 px, et le colle devient la selection", ok, d);
	}
	// ── 4. COUPER : le presse-papiers est plein ET l'original est parti
	st.doc.NewDocument("recette gestes", NkAuthor::Humain);
	{
		poser(0, "Reste", 0.f, 0.f, 10.f, 10.f);
		const int32 a = poser(0, "Coupe", 40.f, 60.f, 80.f, 30.f);
		st.Recompute(surface);
		st.SelectSingle(a);
		st.CouperSelection();
		const bool parti = compteLabel("Coupe") == 0;
		st.CollerPressePapiers();
		char d[96];
		nkentseu::NkSnprintf(d, sizeof(d), "apres couper : x%d ; apres coller : x%d", parti ? 0 : 1,
				 compteLabel("Coupe"));
		verdict("couper puis coller : l'original part, le contenu revient",
				parti && compteLabel("Coupe") == 1, d);
	}
	// ── 5. DUPLIQUER SUR PLACE (Ctrl+D) : meme parent, decale
	st.doc.NewDocument("recette gestes", NkAuthor::Humain);
	{
		const int32 cadre = poser(0, "Cadre", 0.f, 0.f, 400.f, 400.f);
		st.doc.nodes[(uint32)cadre].layout.kind = NkLayoutKind::Free;
		const int32 a = poser(cadre, "Bouton", 20.f, 30.f, 100.f, 40.f);
		st.Recompute(surface);
		st.SelectSingle(a);
		st.DupliquerSelection();
		const int32 b = st.selected;
		const bool ok = st.doc.IsValidIndex(b) && b != a
						&& st.doc.nodes[(uint32)b].parent == cadre && compteLabel("Bouton") == 2;
		char d[96];
		nkentseu::NkSnprintf(d, sizeof(d), "Bouton x%d, meme parent=%s", compteLabel("Bouton"),
				 (st.doc.IsValidIndex(b) && st.doc.nodes[(uint32)b].parent == cadre) ? "oui" : "non");
		verdict("dupliquer : la copie nait dans le MEME parent", ok, d);
	}
	// ── 6. GROUPER PRESERVE LES POSITIONS A L'ECRAN, AU PIXEL
	//    (c'est LA propriete qui distingue un vrai Grouper d'un re-parentage)
	st.doc.NewDocument("recette gestes", NkAuthor::Humain);
	{
		const int32 cadre = poser(0, "Cadre", 0.f, 0.f, 400.f, 400.f);
		st.doc.nodes[(uint32)cadre].layout.kind = NkLayoutKind::Free;
		const int32 a = poser(cadre, "A", 20.f, 30.f, 60.f, 40.f);
		const int32 b = poser(cadre, "B", 150.f, 90.f, 60.f, 40.f);
		st.Recompute(surface);
		const NkPaintRect ra0 = st.layout.At(a), rb0 = st.layout.At(b);
		st.sel.Set(a);
		st.sel.Add(b);
		st.selected = st.sel.Primary();
		const bool fait = st.GrouperSelection();
		st.Recompute(surface);
		// apres groupement les index ne bougent pas (AddChild + Reparent), mais
		// on les RETROUVE par le libelle : ne jamais supposer une numerotation.
		int32 ia = -1, ib = -1, ig = -1;
		for (uint32 i = 0; i < (uint32)st.doc.nodes.Size(); ++i) {
			const char *l = st.doc.nodes[i].label.Data();
			if (!l)
				continue;
			if (NkComponentDecl::StrEq(l, "A"))
				ia = (int32)i;
			else if (NkComponentDecl::StrEq(l, "B"))
				ib = (int32)i;
			else if (NkComponentDecl::StrEq(l, "Groupe 1"))
				ig = (int32)i;
		}
		const bool dedans = ia >= 0 && ib >= 0 && ig >= 0 && st.doc.nodes[(uint32)ia].parent == ig
							&& st.doc.nodes[(uint32)ib].parent == ig;
		const NkPaintRect ra1 = (ia >= 0 && st.layout.Has(ia)) ? st.layout.At(ia) : NkPaintRect{-1.f, -1.f, 0.f, 0.f};
		const NkPaintRect rb1 = (ib >= 0 && st.layout.Has(ib)) ? st.layout.At(ib) : NkPaintRect{-1.f, -1.f, 0.f, 0.f};
		const bool memePlace = ra1.x == ra0.x && ra1.y == ra0.y && rb1.x == rb0.x && rb1.y == rb0.y;
		char d[160];
		nkentseu::NkSnprintf(d, sizeof(d), "A (%.0f,%.0f)->(%.0f,%.0f), B (%.0f,%.0f)->(%.0f,%.0f)", ra0.x,
				 ra0.y, ra1.x, ra1.y, rb0.x, rb0.y, rb1.x, rb1.y);
		verdict("grouper : les positions A L'ECRAN ne bougent pas d'un pixel",
				fait && dedans && memePlace, d);
		// ── 7. DEGROUPER remet tout en place, au pixel aussi
		st.SelectSingle(ig);
		const bool fait2 = st.DegrouperSelection();
		st.Recompute(surface);
		int32 ja = -1, jb = -1;
		bool groupeParti = true;
		for (uint32 i = 0; i < (uint32)st.doc.nodes.Size(); ++i) {
			const char *l = st.doc.nodes[i].label.Data();
			if (!l)
				continue;
			if (NkComponentDecl::StrEq(l, "A"))
				ja = (int32)i;
			else if (NkComponentDecl::StrEq(l, "B"))
				jb = (int32)i;
			else if (NkComponentDecl::StrEq(l, "Groupe 1"))
				groupeParti = false;
		}
		const NkPaintRect ra2 = (ja >= 0 && st.layout.Has(ja)) ? st.layout.At(ja) : NkPaintRect{-1.f, -1.f, 0.f, 0.f};
		const NkPaintRect rb2 = (jb >= 0 && st.layout.Has(jb)) ? st.layout.At(jb) : NkPaintRect{-1.f, -1.f, 0.f, 0.f};
		char d2[160];
		nkentseu::NkSnprintf(d2, sizeof(d2), "groupe parti=%s, A (%.0f,%.0f), B (%.0f,%.0f)",
				 groupeParti ? "oui" : "non", ra2.x, ra2.y, rb2.x, rb2.y);
		verdict("degrouper : le groupe part, les positions ecran tiennent",
				fait2 && groupeParti && ra2.x == ra0.x && ra2.y == ra0.y && rb2.x == rb0.x
					&& rb2.y == rb0.y,
				d2);
	}
	// ── 8. SUPPRIMER UNE MULTI-SELECTION : les DEUX partent (le piege est la
	//    renumerotation — supprimer le premier perime l'index du second)
	st.doc.NewDocument("recette gestes", NkAuthor::Humain);
	{
		const int32 a = poser(0, "A", 0.f, 0.f, 10.f, 10.f);
		poser(0, "Garde", 20.f, 0.f, 10.f, 10.f);
		const int32 c = poser(0, "C", 40.f, 0.f, 10.f, 10.f);
		st.Recompute(surface);
		st.sel.Set(a);
		st.sel.Add(c);
		st.selected = st.sel.Primary();
		st.SupprimerSelection();
		char d[96];
		nkentseu::NkSnprintf(d, sizeof(d), "A x%d, C x%d, Garde x%d", compteLabel("A"), compteLabel("C"),
				 compteLabel("Garde"));
		verdict("supprimer une multi-selection : les deux partent, le voisin reste",
				compteLabel("A") == 0 && compteLabel("C") == 0 && compteLabel("Garde") == 1, d);
	}
	// ── 9. TOUT SELECTIONNER au niveau du FORAGE (Lunacy), pas tout le document
	st.doc.NewDocument("recette gestes", NkAuthor::Humain);
	{
		const int32 cadre = poser(0, "Cadre", 0.f, 0.f, 400.f, 400.f);
		st.doc.nodes[(uint32)cadre].layout.kind = NkLayoutKind::Free;
		poser(cadre, "E1", 0.f, 0.f, 10.f, 10.f);
		poser(cadre, "E2", 20.f, 0.f, 10.f, 10.f);
		poser(cadre, "E3", 40.f, 0.f, 10.f, 10.f);
		st.Recompute(surface);
		st.forageToile = -1;
		const uint32 n0 = st.ToutSelectionner(); // premier niveau : le cadre seul
		st.forageToile = cadre;
		const uint32 n1 = st.ToutSelectionner(); // dans le cadre : les trois
		char d[96];
		nkentseu::NkSnprintf(d, sizeof(d), "premier niveau=%u, dans le cadre=%u", n0, n1);
		verdict("tout selectionner : au NIVEAU du forage, pas tout le document",
				n0 == 1 && n1 == 3, d);
	}
	// ── 10 a 13. UN GESTE = UN PAS D'ANNULATION (la mesure qui manquait)
	//    Un seul Annuler doit suffire a chaque geste : c'est ce que
	//    --recette-annulation ne pouvait pas dire.
	{
		struct Cas {
				const char *nom;
				int32 quoi; // 0 coller, 1 couper, 2 dupliquer, 3 grouper
		};
		const Cas cas4[4] = {{"coller", 0}, {"couper", 1}, {"dupliquer", 2}, {"grouper", 3}};
		for (int32 k = 0; k < 4; ++k) {
			st.doc.NewDocument("recette gestes", NkAuthor::Humain);
			const int32 cadre = poser(0, "Cadre", 0.f, 0.f, 400.f, 400.f);
			st.doc.nodes[(uint32)cadre].layout.kind = NkLayoutKind::Free;
			const int32 a = poser(cadre, "A", 20.f, 30.f, 60.f, 40.f);
			const int32 b = poser(cadre, "B", 150.f, 90.f, 60.f, 40.f);
			st.Recompute(surface);
			st.histoire = NkHistorique();
			stabiliser(st);
			const NkString avant = ser(st);
			switch (cas4[k].quoi) {
				case 0:
					st.SelectSingle(a);
					st.CopierSelection();
					st.CollerPressePapiers();
					break;
				case 1:
					st.SelectSingle(a);
					st.CouperSelection();
					break;
				case 2:
					st.SelectSingle(a);
					st.DupliquerSelection();
					break;
				default:
					st.sel.Set(a);
					st.sel.Add(b);
					st.selected = st.sel.Primary();
					st.GrouperSelection();
					break;
			}
			st.Recompute(surface);
			stabiliser(st);
			const bool aChange = !identiques(ser(st), avant);
			st.Annuler(); // UN SEUL
			const bool revenu = identiques(ser(st), avant);
			char d[96];
			nkentseu::NkSnprintf(d, sizeof(d), "le geste ecrit=%s, UN Annuler suffit=%s",
					 aChange ? "oui" : "non", revenu ? "oui" : "non");
			char nom[96];
			nkentseu::NkSnprintf(nom, sizeof(nom), "%s : UN SEUL pas d'annulation", cas4[k].nom);
			verdict(nom, aChange && revenu, d);
		}
	}
	// ── LE PLUS PROCHE ANCETRE COMMUN (groupement, 01/09) ──────────────────
	//     ⚠️ TROIS CAS DANS UN SEUL BANC, PARCE QU'ILS SE TIENNENT : le bon
	//     niveau, le refus du raccourci « tout a la racine », et la protection
	//     contre le cycle. Separes, le troisieme aurait ete « evident » et donc
	//     jamais ecrit -- or c'est celui qui fige l'application.
	{
		static NkUIDocument d;
		d.NewDocument("ancetre", NkAuthor::Humain);
		//        0
		//        +-- 1 page
		//            +-- 2 carte A      +-- 4 carte B
		//                +-- 3 texte        +-- 5 icone
		const int32 page = d.AddChild(0, "", NkAuthor::Humain);
		const int32 cA = d.AddChild(page, "", NkAuthor::Humain);
		const int32 tA = d.AddChild(cA, "", NkAuthor::Humain);
		const int32 cB = d.AddChild(page, "", NkAuthor::Humain);
		const int32 iB = d.AddChild(cB, "", NkAuthor::Humain);
		// (a) LE BON NIVEAU : deux petits-enfants de la page -> LA PAGE, pas la
		//     racine. C'est tout le retour : grouper deux elements d'une meme
		//     page ne doit PAS les sortir de cette page.
		const int32 deux[2] = {tA, iB};
		const int32 lca1 = NkAncetreCommun(d, deux, 2u);
		// (b) MEME PARENT : le calcul general doit redonner l'ancien resultat,
		//     sinon on aurait « generalise » en cassant le cas qui marchait.
		const int32 memeParent[2] = {cA, cB};
		const int32 lca2 = NkAncetreCommun(d, memeParent, 2u);
		// (c) UN SEUL element : son propre parent.
		const int32 seul[1] = {tA};
		const int32 lca3 = NkAncetreCommun(d, seul, 1u);
		// (d) LE CYCLE : un noeud avec son propre descendant -> on MONTE d'un
		//     cran, jamais « dans lui-meme ».
		const int32 cycle[2] = {cA, tA};
		const int32 lca4 = NkAncetreCommun(d, cycle, 2u);
		char det[176];
		nkentseu::NkSnprintf(det, sizeof(det),
				 "petits-enfants->%d (page=%d) ; meme parent->%d ; seul->%d ; noeud+descendant->%d "
				 "(pas %d)",
				 lca1, page, lca2, lca3, lca4, cA);
		verdict("le groupe nait au PLUS PROCHE ancetre commun (pas a la racine), le cas "
				"« meme parent » est preserve, et un noeud + son descendant remonte d'un cran "
				"au lieu de se mettre dans lui-meme",
				lca1 == page && lca2 == page && lca3 == cA && lca4 == page && lca4 != cA, det);
	}
	// ── VAGUE 2 : L'ORDRE DE PROFONDEUR, QUATRE GESTES ───────────────────────
	// Source `/layers` : `Ctrl+]`, `Ctrl+Maj+]`, `Ctrl+[`, `Ctrl+Maj+[`.
	// CONTEXTE : la toile et la Hierarchie (le meme geste des deux surfaces).
	//
	// ⚠️ LE SENS EST MESURE, PAS SUPPOSE. `NkDrawDocument` parcourt `children` en
	//    ordre CROISSANT : le dernier peint recouvre, donc LE RANG LE PLUS GRAND
	//    EST DEVANT. Ecrire les quatre commandes sur l'intuition inverse aurait
	//    donne quatre entrees qui font le contraire de leur nom -- sans qu'aucun
	//    code ne proteste, parce que le document resterait parfaitement valide.
	{
		NkUIDocument d;
		d.NewDocument("profondeur", NkAuthor::Humain);
		const int32 pg = d.AddChild(0, "", NkAuthor::Humain);
		const int32 a = d.AddChild(pg, "", NkAuthor::Humain);
		const int32 b = d.AddChild(pg, "", NkAuthor::Humain);
		const int32 c = d.AddChild(pg, "", NkAuthor::Humain);
		const int32 e = d.AddChild(pg, "", NkAuthor::Humain);
		auto rang = [&](int32 n) -> int32 {
			const NkVector<int32> &k = d.nodes[(uint32)pg].children;
			for (uint32 i = 0; i < (uint32)k.Size(); ++i)
				if (k[i] == n)
					return (int32)i;
			return -1;
		};
		const bool depart = rang(a) == 0 && rang(b) == 1 && rang(c) == 2 && rang(e) == 3;
		// (a) AVANCER D'UN RANG : un cran, pas quatre.
		const bool av = NkOrdreProfondeur(d, a, NkProfondeur::Avancer) && rang(a) == 1;
		// (b) RECULER D'UN RANG : le retour exact.
		const bool re = NkOrdreProfondeur(d, a, NkProfondeur::Reculer) && rang(a) == 0;
		// (c) LES DEUX EXTREMES
		const bool pp = NkOrdreProfondeur(d, a, NkProfondeur::PremierPlan) && rang(a) == 3;
		const bool ap = NkOrdreProfondeur(d, a, NkProfondeur::ArrierePlan) && rang(a) == 0;
		// (d) ⚠️ « D'UN CRAN » N'EST PAS « TOUT AU FOND », et c'est le volet qui
		//     distingue reellement les quatre commandes. Sur une pile de quatre,
		//     avancer `a` depuis le fond doit le mettre au rang 1 -- pas au rang 3.
		//     Sans ce volet, quatre commandes qui feraient toutes « aux extremes »
		//     passeraient (a), (b) et (c).
		const bool unCran = rang(a) == 0 && NkOrdreProfondeur(d, a, NkProfondeur::Avancer)
							&& rang(a) == 1 && rang(b) == 0;
		// (e) UNE COMMANDE QUI NE FAIT RIEN LE DIT. Deja tout devant et on avance :
		//     `false`. Un `true` complaisant ferait pousser un pas d'annulation
		//     vide et afficherait « modifie » sur un ecran identique.
		(void)NkOrdreProfondeur(d, a, NkProfondeur::PremierPlan);
		const bool franc = !NkOrdreProfondeur(d, a, NkProfondeur::Avancer)
						   && !NkOrdreProfondeur(d, a, NkProfondeur::PremierPlan);
		// (f) LA RACINE N'A PAS DE FRATRIE : refus franc, sans rien toucher.
		const bool racine = !NkOrdreProfondeur(d, 0, NkProfondeur::Avancer);
		// (g) CONSERVATION : aucun noeud n'a disparu ni change de parent -- on a
		//     reordonne, pas reparente. Quatre enfants au depart, quatre a la fin,
		//     tous encore sous la page.
		const NkVector<int32> &fin = d.nodes[(uint32)pg].children;
		bool memeFratrie = fin.Size() == 4u;
		for (uint32 i = 0; i < (uint32)fin.Size() && memeFratrie; ++i)
			memeFratrie = d.IsValidIndex(fin[i]) && d.nodes[(uint32)fin[i]].parent == pg;
		char det[224];
		nkentseu::NkSnprintf(det, sizeof(det), "depart=%d ; avancer=%d reculer=%d 1er plan=%d arriere=%d ; "
							   "un cran != tout au fond=%d ; refus franc=%d racine=%d ; "
							   "fratrie conservee=%d (%u enfants)",
				 depart ? 1 : 0, av ? 1 : 0, re ? 1 : 0, pp ? 1 : 0, ap ? 1 : 0, unCran ? 1 : 0,
				 franc ? 1 : 0, racine ? 1 : 0, memeFratrie ? 1 : 0, (uint32)fin.Size());
		verdict("L'ORDRE DE PROFONDEUR rend les QUATRE gestes de Lunacy (le rang le plus grand est "
				"DEVANT -- mesure sur l'ordre de peinture), « d'un cran » n'est pas « tout au "
				"fond », une commande sans effet le DIT, et rien n'est reparente",
				depart && av && re && pp && ap && unCran && franc && racine && memeFratrie, det);
	}

	// ── VAGUE 2 : VERROUILLER / MASQUER, MESURE PAR SES EFFETS ───────────────
	// Source `/layers`. CONTEXTE : la toile (ce qui se peint, ce qui s'attrape).
	//
	// 🔴 CE CAS N'ASSERTE AUCUN DRAPEAU, ET C'EST DELIBERE. *Un compteur
	//    d'intentions n'est pas un controle d'effets* : verifier
	//    « n.masque == true » ne prouverait rien -- ce serait relire la ligne
	//    qu'on vient d'ecrire. On mesure donc les DEUX effets reels :
	//      - le POINTAGE : un clic AU MEME POINT rend autre chose qu'avant ;
	//      - le DESSIN : le peintre n'emet plus les commandes de ce nœud.
	//
	// ⚠️ ET LE DESSIN SE MESURE PAR CE QUI EST EMIS, PAS PAR DES PIXELS :
	//    `NkRecordingPaint` -- le peintre enregistreur du kit, qui existait deja
	//    -- compte les commandes. Meme principe que `--dump-ui` (lire ce que
	//    l'interface a EMIS), et c'est la seule mesure de dessin qui tienne sans
	//    fenetre.
	{
		NkUIDocument d;
		d.NewDocument("verrou", NkAuthor::Humain);
		const int32 pg = d.AddChild(0, "", NkAuthor::Humain);
		d.nodes[(uint32)pg].shape = NkString("frame");
		d.nodes[(uint32)pg].layout.kind = NkLayoutKind::Free;
		d.nodes[(uint32)pg].width.mode = NkSizeMode::Fixed;
		d.nodes[(uint32)pg].width.value = 400.f;
		d.nodes[(uint32)pg].height.mode = NkSizeMode::Fixed;
		d.nodes[(uint32)pg].height.value = 300.f;
		// un groupe qui porte un enfant : c'est lui qui prouvera l'HERITAGE
		const int32 grp = d.AddChild(pg, "", NkAuthor::Humain);
		d.nodes[(uint32)grp].shape = NkString("rect");
		d.nodes[(uint32)grp].layout.kind = NkLayoutKind::Free;
		d.nodes[(uint32)grp].posX = 20.f;
		d.nodes[(uint32)grp].posY = 20.f;
		d.nodes[(uint32)grp].width.mode = NkSizeMode::Fixed;
		d.nodes[(uint32)grp].width.value = 200.f;
		d.nodes[(uint32)grp].height.mode = NkSizeMode::Fixed;
		d.nodes[(uint32)grp].height.value = 150.f;
		const int32 enf = d.AddChild(grp, "", NkAuthor::Humain);
		d.nodes[(uint32)enf].shape = NkString("rect");
		d.nodes[(uint32)enf].posX = 10.f;
		d.nodes[(uint32)enf].posY = 10.f;
		d.nodes[(uint32)enf].width.mode = NkSizeMode::Fixed;
		d.nodes[(uint32)enf].width.value = 80.f;
		d.nodes[(uint32)enf].height.mode = NkSizeMode::Fixed;
		d.nodes[(uint32)enf].height.value = 60.f;
		NkLayoutResult lv;
		NkComputeLayout(d, NkPaintRect{0.f, 0.f, 400.f, 300.f}, lv);
		// le point qui tombe SUR l'enfant (donc aussi sur le groupe, et sur la page)
		const float32 px = lv.At(enf).x + lv.At(enf).w * 0.5f;
		const float32 py = lv.At(enf).y + lv.At(enf).h * 0.5f;
		// (a) AVANT : le clic attrape bien l'enfant. Temoin sans lequel « le clic
		//     ne prend plus rien » pourrait vouloir dire « il ne prenait deja rien ».
		const bool avant = NkPickSelectable(d, lv, px, py) == enf;
		// (b) VERROUILLER L'ENFANT : le clic AU MEME POINT tombe sur son GROUPE.
		//     ⚠️ Volet qui distingue « inattrapable » de « surface inerte » : le
		//     clic TRAVERSE. Un `return -1` dans le pointage aurait rendu -1 ici
		//     et fait du verrou un TROU dans la toile.
		d.nodes[(uint32)enf].verrouille = true;
		const bool traverse = NkPickSelectable(d, lv, px, py) == grp;
		const bool verrouVisible = NkNoeudVisible(d, enf); // verrouiller ne masque pas
		d.nodes[(uint32)enf].verrouille = false;
		// (c) L'HERITAGE DU VERROU : verrouiller le GROUPE rend l'ENFANT
		//     inattrapable alors qu'aucun drapeau n'est pose sur lui.
		d.nodes[(uint32)grp].verrouille = true;
		const bool heriteVerrou = NkPickSelectable(d, lv, px, py) == pg;
		d.nodes[(uint32)grp].verrouille = false;
		// (d) MASQUER : le nœud sort du POINTAGE **et** du DESSIN.
		NkComponentInput inv;
		nkentseu::editorkit::NkRecordingPaint pv;
		NkDocumentHost hote;
		hote.SyncTo(d);
		pv.Reset();
		NkDrawDocument(pv, inv, d, lv, hote, 0);
		const uint32 cmdAvant = (uint32)pv.cmds.Size();
		d.nodes[(uint32)enf].masque = true;
		pv.Reset();
		NkDrawDocument(pv, inv, d, lv, hote, 0);
		const uint32 cmdApres = (uint32)pv.cmds.Size();
		const bool ecranChange = cmdApres < cmdAvant;
		const bool masqueInattrapable = NkPickSelectable(d, lv, px, py) == grp;
		d.nodes[(uint32)enf].masque = false;
		// (e) L'HERITAGE DU MASQUE : masquer le GROUPE retire AUSSI l'enfant du
		//     dessin -- le peintre ne descend pas dans un sous-arbre masque.
		d.nodes[(uint32)grp].masque = true;
		pv.Reset();
		NkDrawDocument(pv, inv, d, lv, hote, 0);
		const uint32 cmdGroupe = (uint32)pv.cmds.Size();
		const bool heriteMasque = cmdGroupe < cmdApres && !NkNoeudVisible(d, enf);
		d.nodes[(uint32)grp].masque = false;
		// (f) ⚠️ CONSERVATION : les deux cles sont ADDITIVES. Un document qui ne
		//     les porte pas ne les ecrit pas ; un document qui les porte les
		//     relit. Sans ce volet, on aurait pu ajouter deux cles au format et
		//     faire tomber le round-trip de tous les fichiers d'avant.
		NkString sansS;
		d.Save(sansS);
		const bool aucuneCle = strstr(sansS.Data(), "verrouille") == nullptr
								   && strstr(sansS.Data(), "masque") == nullptr;
		d.nodes[(uint32)enf].verrouille = true;
		d.nodes[(uint32)enf].masque = true;
		NkString avecS;
		d.Save(avecS);
		NkUIDocument relu;
		const bool chargee = relu.Load(avecS.Data());
		const bool relues = chargee && relu.IsValidIndex(enf)
							&& relu.nodes[(uint32)enf].verrouille
							&& relu.nodes[(uint32)enf].masque;
		NkString stable;
		if (chargee)
			relu.Save(stable);
		const bool allerRetour = chargee && stable.Size() == avecS.Size();
		char det[256];
		nkentseu::NkSnprintf(det, sizeof(det), "avant : clic->enfant=%d ; verrou : clic TRAVERSE vers le "
							   "groupe=%d (visible=%d) herite=%d ; masque : %u->%u commandes (=%d) "
							   "inattrapable=%d ; masque herite=%u cmd (=%d) ; sans cle=%d "
							   "relues=%d stable=%d",
				 avant ? 1 : 0, traverse ? 1 : 0, verrouVisible ? 1 : 0, heriteVerrou ? 1 : 0,
				 cmdAvant, cmdApres, ecranChange ? 1 : 0, masqueInattrapable ? 1 : 0, cmdGroupe,
				 heriteMasque ? 1 : 0, aucuneCle ? 1 : 0, relues ? 1 : 0, allerRetour ? 1 : 0);
		verdict("VERROUILLER / MASQUER se mesurent par leurs EFFETS : un verrouille n'est plus "
				"attrape (le clic TRAVERSE vers le dessous) mais reste peint, un masque sort du "
				"dessin ET du pointage, les deux s'HERITENT, et le format reste additif",
				avant && traverse && verrouVisible && heriteVerrou && ecranChange
					&& masqueInattrapable && heriteMasque && aucuneCle && relues && allerRetour,
				det);
	}

	// ── L'OEIL ET LE CADENAS DE LA HIERARCHIE : LES DEUX PIEGES ──────────────
	// Vague 2, la porte d'interface. *Un verrou que l'utilisateur ne peut ni voir
	// ni poser n'existe pas pour lui.*
	//
	// 📌 LE COMPOSANT D'ARBRE PORTAIT DEJA TOUT : `hidden`, `locked`,
	//    `flagsInherited`, les quatre icones et le rappel `onToggleFlag`.
	//    NKCraft avait paye la lecon a l'usage. Le travail n'etait pas de
	//    dessiner deux icones, c'etait de les BRANCHER -- d'ou ce cas, qui ne
	//    mesure QUE les deux endroits ou le branchement peut se tromper.
	{
		NkUIDocument d;
		d.NewDocument("icones", NkAuthor::Humain);
		const int32 pg = d.AddChild(0, "", NkAuthor::Humain);
		const int32 grp = d.AddChild(pg, "", NkAuthor::Humain);
		const int32 enf = d.AddChild(grp, "", NkAuthor::Humain);
		bool c1 = false, v1 = false, h1 = false;
		// (a) AU REPOS : rien n'est cache, rien n'est verrouille, rien n'est herite.
		NkDrapeauxArbre(d, enf, c1, v1, h1);
		const bool repos = !c1 && !v1 && !h1;
		// (b) LE NOEUD PORTE SON PROPRE DRAPEAU : effectif, mais PAS herite --
		//     c'est ici que l'icone doit rester CLIQUABLE.
		d.nodes[(uint32)enf].masque = true;
		NkDrapeauxArbre(d, enf, c1, v1, h1);
		const bool propre = c1 && v1 && !h1; // masque => inattrapable aussi
		d.nodes[(uint32)enf].masque = false;
		// (c) ⚠️ LE DRAPEAU VIENT DE L'ANCETRE : effectif ET herite. C'est ce
		//     second bit qui fait peindre l'icone ATTENUEE et refuser le clic ;
		//     sans lui le refus parait inexplicable, et c'est mot pour mot ce que
		//     NKCraft a paye (« je ne peux selectionner ni le parent ni
		//     l'enfant »).
		d.nodes[(uint32)grp].masque = true;
		NkDrapeauxArbre(d, enf, c1, v1, h1);
		const bool heriteEnfant = c1 && h1;
		// et le PORTEUR, lui, n'est pas herite : son icone reste cliquable.
		bool c2 = false, v2 = false, h2 = false;
		NkDrapeauxArbre(d, grp, c2, v2, h2);
		const bool porteurCliquable = c2 && !h2;
		d.nodes[(uint32)grp].masque = false;
		// (d) LE VERROU SEUL : inattrapable SANS etre cache. Confondre les deux
		//     donnerait un verrou invisible.
		d.nodes[(uint32)grp].verrouille = true;
		NkDrapeauxArbre(d, enf, c1, v1, h1);
		const bool verrouSeul = !c1 && v1 && h1;
		d.nodes[(uint32)grp].verrouille = false;
		// (e) 🔴 LA POLARITE -- LE PIEGE ENTIER DU BRANCHEMENT. L'oeil du
		//     composant dit « VISIBLE », notre champ dit « MASQUE » : la valeur
		//     s'INVERSE pour l'oeil et PAS pour le cadenas. Ecrit au site
		//     d'appel, ce detail passe la relecture puis fait le contraire a
		//     l'ecran -- et le defaut se lit « l'icone ne marche pas », jamais
		//     « le sens est inverse ».
		NkUINode &ne = d.nodes[(uint32)enf];
		NkPoserDrapeauArbre(ne, true, false); // oeil : « non visible »
		const bool oeilFerme = ne.masque;
		NkPoserDrapeauArbre(ne, true, true); // oeil : « visible »
		const bool oeilOuvert = !ne.masque;
		NkPoserDrapeauArbre(ne, false, true); // cadenas : « verrouille »
		const bool cadenasMis = ne.verrouille;
		NkPoserDrapeauArbre(ne, false, false); // cadenas : « libre »
		const bool cadenasOte = !ne.verrouille;
		const bool polarite = oeilFerme && oeilOuvert && cadenasMis && cadenasOte;
		char det[224];
		nkentseu::NkSnprintf(det, sizeof(det), "repos=%d ; drapeau propre : effectif sans herite=%d ; herite "
							   "de l'ancetre=%d (porteur cliquable=%d) ; verrou seul (non "
							   "cache)=%d ; polarite oeil INVERSEE + cadenas direct=%d",
				 repos ? 1 : 0, propre ? 1 : 0, heriteEnfant ? 1 : 0, porteurCliquable ? 1 : 0,
				 verrouSeul ? 1 : 0, polarite ? 1 : 0);
		verdict("L'OEIL ET LE CADENAS : les drapeaux arrivent EFFECTIFS a l'arbre, un drapeau "
				"venu d'un ANCETRE est marque herite (icone attenuee, clic refuse) alors que son "
				"porteur reste cliquable, et la POLARITE de l'oeil est inversee -- pas celle du "
				"cadenas",
				repos && propre && heriteEnfant && porteurCliquable && verrouSeul && polarite,
				det);
	}

	// ── COMPOSANTS DE DOCUMENT, ETAPE 1 : LE MODELE ET SON FORMAT ────────────
	// Modele `15_Modele_Composants.md`. C'est la LIGNE D'ARRIVEE du chantier :
	// « nkuidesign qui me permettra de designer nos premiers composants ».
	//
	// ⚠️ LE VOLET (a) EST LE PLUS IMPORTANT DE TOUT CE CAS, et c'est le moins
	//    spectaculaire : un document SANS composant doit se reenregistrer OCTET
	//    POUR OCTET. On vient d'ajouter trois cles au format ; sans ce volet, on
	//    aurait pu faire tomber l'aller-retour de TOUS les fichiers d'avant sans
	//    s'en apercevoir avant longtemps.
	{
		NkUIDocument d;
		d.NewDocument("composants", NkAuthor::Humain);
		const int32 pg = d.AddChild(0, "", NkAuthor::Humain);
		const int32 bt = d.AddChild(pg, "", NkAuthor::Humain);
		d.nodes[(uint32)bt].label = NkString("Bouton");
		// (a) CONSERVATION : aucune declaration, aucune instance -> aucune cle.
		NkString sans;
		d.Save(sans);
		// ⚠️ LE MARQUEUR DE BLOC SE CHERCHE EN DEBUT DE LIGNE, et cette
		//    precision n'est pas cosmetique : `composant` est DEJA une cle de
		//    nœud (le composant DE CODE, indente de deux espaces). Ma premiere
		//    version cherchait « composant » n'importe ou et declarait le
		//    document non conforme alors qu'il l'etait. *Les deux notions du
		//    §15.1 se sont marchees dessus des le premier banc* -- exactement
		//    ce que la separation des deux champs evite dans le modele.
		const bool aucuneCle = strstr(sans.Data(), "\ncomposant ") == nullptr
							   && strstr(sans.Data(), "instance = ") == nullptr
							   && strstr(sans.Data(), "\ndnoeud ") == nullptr;
		NkUIDocument r0;
		NkString sans2;
		const bool relu0 = r0.Load(sans.Data());
		if (relu0)
			r0.Save(sans2);
		const bool octetPourOctet = relu0 && sans2.Size() == sans.Size()
									&& strcmp(sans2.Data(), sans.Data()) == 0;
		// (b) UNE DECLARATION AVEC SON ARBRE, ET UNE INSTANCE QUI LA REFERENCE.
		NkDeclarationComposant dc;
		dc.identite.auteur = NkString("rodolf");
		dc.identite.nom = NkString("bouton_primaire");
		dc.identite.version = NkString("1");
		NkUINode racine;
		racine.label = NkString("Bouton primaire");
		racine.shape = NkString("rect");
		NkUINode libelle;
		libelle.label = NkString("Libelle");
		libelle.shape = NkString("text");
		dc.arbre.PushBack(racine);
		dc.arbre.PushBack(libelle);
		dc.arbre[0].children.PushBack(1);
		dc.arbre[1].parent = 0;
		d.declarations.PushBack(dc);
		const NkString cle = d.declarations[0].identite.Cle();
		const bool cleJuste = strcmp(cle.Data(), "rodolf/bouton_primaire@1") == 0;
		d.nodes[(uint32)bt].instanceDe = cle;
		d.nodes[(uint32)bt].ecarts = NkUINode::EcartTexte | NkUINode::EcartRemplissages;
		// (c) L'ALLER-RETOUR REND TOUT : identite, arbre de la declaration (sa
		//     STRUCTURE comprise), reference de l'instance et masque d'ecarts.
		NkString avec;
		d.Save(avec);
		NkUIDocument r1;
		const bool relu1 = r1.Load(avec.Data());
		const bool identite = relu1 && r1.declarations.Size() == 1
							  && strcmp(r1.declarations[0].identite.auteur.Data(), "rodolf") == 0
							  && strcmp(r1.declarations[0].identite.Cle().Data(),
										"rodolf/bouton_primaire@1")
										 == 0;
		// ⚠️ LA STRUCTURE DE L'ARBRE, PAS SEULEMENT SON COMPTE : deux nœuds lus
		//    dans le desordre feraient un compte juste et un arbre faux.
		const bool arbre = relu1 && r1.declarations[0].arbre.Size() == 2
						   && r1.declarations[0].arbre[1].parent == 0
						   && r1.declarations[0].arbre[0].children.Size() == 1;
		const bool instance = relu1 && r1.IsValidIndex(bt)
							  && strcmp(r1.nodes[(uint32)bt].instanceDe.Data(), cle.Data()) == 0
							  && r1.nodes[(uint32)bt].Surcharge(NkUINode::EcartTexte)
							  && r1.nodes[(uint32)bt].Surcharge(NkUINode::EcartRemplissages)
							  && !r1.nodes[(uint32)bt].Surcharge(NkUINode::EcartBordures);
		NkString avec2;
		if (relu1)
			r1.Save(avec2);
		const bool stable = relu1 && strcmp(avec2.Data(), avec.Data()) == 0;
		// (d) ⚠️ LA PORTE DE FORK. Une declaration d'un TIERS ne se modifie pas en
		//     place -- c'est de la structure, pas du comportement. Et l'auteur
		//     inconnu ne passe PAS : ne pas savoir qui on est n'autorise pas a
		//     toucher au bien d'autrui.
		NkIdentiteComposant mien;
		mien.auteur = NkString("rodolf");
		NkIdentiteComposant tiers;
		tiers.auteur = NkString("quelqu_un");
		NkIdentiteComposant local; // sans auteur : local au document
		const bool fork = NkPeutModifierDeclaration(mien, "rodolf")
						  && !NkPeutModifierDeclaration(tiers, "rodolf")
						  && !NkPeutModifierDeclaration(tiers, nullptr)
						  && NkPeutModifierDeclaration(local, "rodolf");
		char det[256];
		nkentseu::NkSnprintf(det, sizeof(det), "sans composant : aucune cle=%d octet pour octet=%d ; cle "
							   "« %s »=%d ; relu : identite=%d arbre (structure)=%d instance+"
							   "ecarts=%d stable=%d ; porte de fork=%d",
				 aucuneCle ? 1 : 0, octetPourOctet ? 1 : 0, cle.Data(), cleJuste ? 1 : 0,
				 identite ? 1 : 0, arbre ? 1 : 0, instance ? 1 : 0, stable ? 1 : 0, fork ? 1 : 0);
		verdict("COMPOSANTS (etape 1) : un document SANS composant se reenregistre OCTET POUR "
				"OCTET, une declaration voyage avec son identite ET la structure de son arbre, "
				"une instance garde sa reference et son masque d'ecarts, et la PORTE DE FORK "
				"refuse la declaration d'un tiers",
				aucuneCle && octetPourOctet && cleJuste && identite && arbre && instance && stable
					&& fork,
				det);
	}

	// ── LA CLE `unite` : RESERVEE, PAS EXPLOITEE (mandat VR/AR/XR, 02/09) ────
	// ROADMAP_PRODUITS.md §5 au parent : une CIBLE portera un jour une
	// projection en metres et en degres (« panneau a 2 m, 60° ») -- jamais la
	// page. Aujourd'hui seule la PLACE compte : la cle est additive, absente =
	// pixels, ni exploitee ni affichee. Son ABSENCE serait le seul irreversible.
	{
		NkUIDocument d;
		d.NewDocument("reservation", NkAuthor::Humain);
		const int32 pg = d.AddChild(0, "", NkAuthor::Humain);
		// (a) UN DOCUMENT QUI N'EN VEUT PAS NE LA GAGNE PAS : la cle n'apparait
		//     nulle part dans un fichier d'avant.
		NkString sans;
		d.Save(sans);
		const bool absente = strstr(sans.Data(), "unite") == nullptr;
		// (b) POSEE, ELLE VOYAGE -- a cote de `cible`, comme la roadmap le veut.
		d.nodes[(uint32)pg].target = NkString("Casque -- panneau 2 m");
		d.nodes[(uint32)pg].targetUnit = NkString("metres_degres");
		NkString avec;
		d.Save(avec);
		NkUIDocument r;
		const bool voyage = r.Load(avec.Data()) && r.IsValidIndex(pg)
							&& r.nodes[(uint32)pg].targetUnit.Data()
							&& strcmp(r.nodes[(uint32)pg].targetUnit.Data(), "metres_degres") == 0;
		// (c) ET LE REENREGISTREMENT EST STABLE.
		NkString avec2;
		if (voyage)
			r.Save(avec2);
		const bool stable = voyage && strcmp(avec2.Data(), avec.Data()) == 0;
		char det[160];
		nkentseu::NkSnprintf(det, sizeof(det), "absente d'un document d'avant=%d ; posee -> relue=%d ; "
							   "reenregistrement stable=%d",
				 absente ? 1 : 0, voyage ? 1 : 0, stable ? 1 : 0);
		verdict("LA CLE `unite` DES CIBLES EST RESERVEE (VR/AR/XR) : additive -- un document "
				"d'avant ne la gagne pas, posee elle voyage et se reenregistre stable",
				absente && voyage && stable, det);
	}

	// ── COMPOSANTS, ETAPE 2 : EXTRAIRE ET DETACHER, ENSEMBLE ─────────────────
	// Modele §15.4. *« Creer un composant » sans « detacher » enferme
	// l'utilisateur dans une decision qu'il ne peut pas defaire* -- donc les deux
	// arrivent dans le meme lot, ou aucun.
	//
	// ⚠️ LA GARDE CENTRALE EST L'ALLER-RETOUR **NEUTRE** : extraire puis detacher
	//    immediatement rend un sous-arbre EQUIVALENT a l'original. C'est elle qui
	//    prouve que l'extraction n'a rien perdu en chemin -- un champ oublie dans
	//    la copie se verrait ici, et nulle part ailleurs.
	//
	// ⚠️ « EQUIVALENT » SE COMPARE SUR UNE FORME CANONIQUE, PAS SUR LES INDICES :
	//    `RemoveSubtree` RENUMEROTE, donc les nœuds reviennent a d'autres places.
	//    Comparer le fichier entier ferait echouer un aller-retour pourtant
	//    parfait -- et on aurait « corrige » un defaut qui n'existe pas. On
	//    compare donc un parcours EN PROFONDEUR : libelle, forme, et nombre
	//    d'enfants a chaque etage.
	{
		struct Canon {
				static void Ecrire(const NkUIDocument &d, int32 i, NkString &out) {
					if (!d.IsValidIndex(i))
						return;
					const NkUINode &n = d.nodes[(uint32)i];
					out.Append("[");
					out.Append(n.label.Empty() ? "-" : n.label.Data());
					out.Append("|");
					out.Append(n.shape.Empty() ? "-" : n.shape.Data());
					out.Append("|");
					char b[16];
					nkentseu::NkSnprintf(b, sizeof(b), "%u", (uint32)n.children.Size());
					out.Append(b);
					for (uint32 c = 0; c < (uint32)n.children.Size(); ++c)
						Ecrire(d, n.children[c], out);
					out.Append("]");
				}
		};
		NkUIDocument d;
		d.NewDocument("extraction", NkAuthor::Humain);
		const int32 pg = d.AddChild(0, "", NkAuthor::Humain);
		const int32 bt = d.AddChild(pg, "", NkAuthor::Humain);
		d.nodes[(uint32)bt].label = NkString("Bouton");
		d.nodes[(uint32)bt].shape = NkString("rect");
		const int32 tx = d.AddChild(bt, "", NkAuthor::Humain);
		d.nodes[(uint32)tx].label = NkString("Libelle");
		d.nodes[(uint32)tx].shape = NkString("text");
		d.nodes[(uint32)tx].text = NkString("Valider");
		const int32 ic = d.AddChild(bt, "", NkAuthor::Humain);
		d.nodes[(uint32)ic].label = NkString("Icone");
		d.nodes[(uint32)ic].shape = NkString("rect");
		NkString avant;
		Canon::Ecrire(d, bt, avant);
		const uint32 nbAvant = (uint32)d.nodes.Size();
		// (a) EXTRAIRE : une declaration nait, le nœud devient une INSTANCE, et sa
		//     descendance quitte le document (elle vit dans la declaration).
		const int32 decl = d.ExtraireComposant(bt, "rodolf", "bouton_valider");
		const bool extrait = decl == 0 && d.declarations.Size() == 1
							 && d.declarations[0].arbre.Size() == 3;
		// on retrouve l'instance : c'est le seul nœud qui porte une reference.
		int32 inst = -1;
		for (uint32 k = 0; k < (uint32)d.nodes.Size(); ++k)
			if (!d.nodes[k].instanceDe.Empty())
				inst = (int32)k;
		// ⚠️ LA TAILLE EST CAPTUREE ICI, AU MOMENT OU L'ON JUGE. Lue plus bas
		//    pour la ligne de detail, elle vaudrait la taille APRES detachement
		//    -- le cas serait juste et sa ligne de detail mentirait, ce qui est
		//    pire qu'un cas faux : on lit le chiffre, pas l'assertion.
		const uint32 nbApresExtraction = (uint32)d.nodes.Size();
		// 🔴 CE VOLET A CHANGE DE SENS LE 02/09, ET C'EST UNE MESURE QUI L'A
		//    IMPOSE. Il exigeait l'inverse : « l'instance n'a plus d'enfants, le
		//    document en a DEUX de moins ». Cette conception-la faisait PERDRE SON
		//    LIBELLE a un bouton extrait (3 commandes de peintre avant, 2 apres --
		//    voir le cas « une instance peint le contenu de sa declaration »).
		//    L'instance GARDE desormais son sous-arbre, la declaration en detient
		//    la copie de reference. On exige donc le contraire : le document ne
		//    perd RIEN.
		// ⚠️ *Un cas qui se met a exiger l'inverse de la veille est un cas qu'il
		//    faut relire, pas ajuster jusqu'au vert.* Celui-ci a ete relu : c'est
		//    la conception qui etait fausse, et le volet qui la refletait.
		const bool estInstance = inst >= 0 && !d.nodes[(uint32)inst].children.Empty()
								 && nbApresExtraction == nbAvant;
		// (b) DETACHER : la descendance revient, la reference s'efface.
		const bool detache = inst >= 0 && d.DetacherInstance(inst)
							 && d.nodes[(uint32)inst].instanceDe.Empty()
							 && d.nodes[(uint32)inst].ecarts == 0u;
		NkString apres;
		if (inst >= 0)
			Canon::Ecrire(d, inst, apres);
		const bool neutre = strcmp(apres.Data(), avant.Data()) == 0;
		// (c) ⚠️ LES ECARTS SONT FUSIONNES, PAS JETES. Une instance dont le TEXTE a
		//     ete surcharge garde SON texte en se detachant. Sans ce volet, on
		//     pourrait « detacher » en rejouant betement la declaration -- et la
		//     main perdrait son travail EN SILENCE, la pire espece de perte.
		NkUIDocument e;
		e.NewDocument("ecarts", NkAuthor::Humain);
		const int32 p2 = e.AddChild(0, "", NkAuthor::Humain);
		const int32 b2 = e.AddChild(p2, "", NkAuthor::Humain);
		e.nodes[(uint32)b2].label = NkString("Bouton");
		e.nodes[(uint32)b2].text = NkString("Origine");
		(void)e.AddChild(b2, "", NkAuthor::Humain);
		const int32 d2 = e.ExtraireComposant(b2, "rodolf", "bt");
		int32 i2 = -1;
		for (uint32 k = 0; k < (uint32)e.nodes.Size(); ++k)
			if (!e.nodes[k].instanceDe.Empty())
				i2 = (int32)k;
		bool garde = false;
		if (d2 >= 0 && i2 >= 0) {
			e.nodes[(uint32)i2].text = NkString("Surcharge");
			e.nodes[(uint32)i2].ecarts = NkUINode::EcartTexte;
			e.DetacherInstance(i2);
			garde = strcmp(e.nodes[(uint32)i2].text.Data(), "Surcharge") == 0;
		}
		// (d) UN NŒUD ORDINAIRE NE SE DETACHE PAS : refus franc.
		const bool refus = !d.DetacherInstance(pg) && !d.DetacherInstance(0);
		char det[256];
		nkentseu::NkSnprintf(det, sizeof(det), "extrait=%d (declaration de %u nœuds) ; instance GARDE ses enfants "
							   "et document %u -> %u=%d ; detache=%d ; aller-retour NEUTRE=%d ; "
							   "ecart TEXTE conserve=%d ; refus franc=%d",
				 extrait ? 1 : 0,
				 d.declarations.Empty() ? 0u : (uint32)d.declarations[0].arbre.Size(), nbAvant,
				 nbApresExtraction, estInstance ? 1 : 0, detache ? 1 : 0, neutre ? 1 : 0,
				 garde ? 1 : 0, refus ? 1 : 0);
		verdict("COMPOSANTS (etape 2) : EXTRAIRE et DETACHER arrivent ensemble -- l'aller-retour "
				"est NEUTRE (le sous-arbre revient equivalent), un ecart de texte est FUSIONNE au "
				"detachement au lieu d'etre jete, et un nœud ordinaire refuse franchement",
				extrait && estInstance && detache && neutre && garde && refus, det);
	}

	// ── LE GESTE D'EXTRACTION, PAR LA MAIN ET NON PAR LA FONCTION ────────────
	// 🔴 C'EST LA LIGNE D'ARRIVEE DU CHANTIER : « nkuidesign qui me permettra de
	//    designer nos premiers composants ». Et « fini » ne veut PAS dire
	//    « `ExtraireComposant` est tenu par deux cas » -- ca, c'etait l'etape 1.
	//    Ca veut dire que RODOLF PEUT LE FAIRE A LA MAIN.
	//
	// ⚠️ CE CAS N'APPELLE DONC PAS `ExtraireComposant`. S'il le faisait, il
	//    rejouerait ce qui est deja prouve et n'apprendrait RIEN sur le geste.
	//    Il part d'une SELECTION reelle, demande a la table des raccourcis ce que
	//    `Ctrl+Alt+K` signifie, passe le resultat au DISPATCHER COMMUN -- celui
	//    du menu contextuel et du clavier -- et regarde le document.
	//    *Un geste qui n'existe pas ne devient pas vrai parce que la fonction
	//    dessous est eprouvee.*
	{
		static DesignState sg;
		sg.doc.NewDocument("geste", NkAuthor::Humain);
		const int32 pg = sg.doc.AddChild(0, "", NkAuthor::Humain);
		const int32 bt = sg.doc.AddChild(pg, "", NkAuthor::Humain);
		sg.doc.nodes[(uint32)bt].label = NkString("Bouton primaire");
		sg.doc.nodes[(uint32)bt].shape = NkString("rect");
		(void)sg.doc.AddChild(bt, "", NkAuthor::Humain);
		sg.SelectSingle(bt);
		const uint32 declAvant = (uint32)sg.doc.declarations.Size();
		// (a) LA MAIN : `Ctrl+Alt+K` sur la selection, par les DEUX portes que
		//     l'utilisateur emprunte -- la table des raccourcis, puis le
		//     dispatcher commun.
		const NkActionCtx a = NkActionDuRaccourci(true, false, 'K', true);
		const bool traite = a == NkActionCtx::ExtraireComposant
							&& NkAppliquerActionCtx(sg, sg.selected, a);
		// (b) LE DOCUMENT PORTE UN COMPOSANT, ET IL PORTE LE NOM QUE LA MAIN
		//     AVAIT DEJA DONNE. Un « composant_1 » aurait force Rodolf a renommer
		//     avant meme de voir ce qu'il vient de creer.
		const bool cree = (uint32)sg.doc.declarations.Size() == declAvant + 1u;
		const bool nomme = cree
						   && strcmp(sg.doc.declarations[declAvant].identite.nom.Data(),
									 "Bouton primaire")
								  == 0;
		// (c) ET LE NOEUD SELECTIONNE EST DEVENU UNE INSTANCE -- c'est ce que
		//     l'utilisateur voit : sa forme est toujours la, mais elle est
		//     desormais une instance de son composant.
		int32 inst = -1;
		for (uint32 k = 0; k < (uint32)sg.doc.nodes.Size(); ++k)
			if (!sg.doc.nodes[k].instanceDe.Empty())
				inst = (int32)k;
		const bool devenue = inst >= 0
							 && strcmp(sg.doc.nodes[(uint32)inst].instanceDe.Data(),
									   sg.doc.declarations[declAvant].identite.Cle().Data())
										== 0;
		// (d) ⚠️ ET LE COMPOSANT LOCAL NOUS APPARTIENT : auteur vide, donc la
		//     porte de fork autorise a le modifier. Lui inventer un nom d'auteur
		//     maintenant serait signer a la place de quelqu'un ; l'identite se
		//     pose le jour du PARTAGE.
		const bool aNous = cree
						   && NkPeutModifierDeclaration(sg.doc.declarations[declAvant].identite,
														"rodolf");
		// (e) LE GESTE INVERSE, PAR LA MEME PORTE : `Ctrl+Alt+D` detache.
		const NkActionCtx a2 = NkActionDuRaccourci(true, false, 'D', true);
		const bool detache = a2 == NkActionCtx::DetacherComposant && inst >= 0
							 && NkAppliquerActionCtx(sg, inst, a2)
							 && sg.doc.nodes[(uint32)inst].instanceDe.Empty();
		// (f) ⚠️ LE CONTROLE NEGATIF QUI COMPTE : la RACINE refuse, et elle le
		//     DIT. Sans lui, un dispatcher qui extrairait n'importe quoi passerait
		//     les cinq volets precedents.
		const uint32 avantRacine = (uint32)sg.doc.declarations.Size();
		const bool refuse = NkAppliquerActionCtx(sg, 0, a)
							&& (uint32)sg.doc.declarations.Size() == avantRacine;
		char det[256];
		nkentseu::NkSnprintf(det, sizeof(det), "Ctrl+Alt+K traite par le dispatcher=%d ; declaration creee=%d "
							   "nommee « %s »=%d ; le noeud est devenu une instance=%d ; local "
							   "donc modifiable=%d ; Ctrl+Alt+D detache=%d ; la racine refuse=%d",
				 traite ? 1 : 0, cree ? 1 : 0,
				 cree ? sg.doc.declarations[declAvant].identite.nom.Data() : "-", nomme ? 1 : 0,
				 devenue ? 1 : 0, aNous ? 1 : 0, detache ? 1 : 0, refuse ? 1 : 0);
		verdict("LE GESTE D'EXTRACTION EXISTE POUR LA MAIN : `Ctrl+Alt+K` sur une selection passe "
				"par la table des raccourcis PUIS par le dispatcher commun, et le document gagne "
				"un composant nomme comme le noeud ; `Ctrl+Alt+D` le detache ; la racine refuse",
				traite && cree && nomme && devenue && aNous && detache && refuse, det);
	}

	// ── REUTILISER : LA MOITIE QUI MANQUAIT AU CHANTIER ──────────────────────
	// Extraire sans pouvoir REPOSER, c'est une bibliotheque sans porte de
	// sortie. Ce cas exerce `InstancierComposant` : une deuxieme instance nait
	// depuis la declaration -- entiere, liee, et independante de la premiere.
	{
		NkUIDocument d;
		d.NewDocument("reutiliser", NkAuthor::Humain);
		const int32 pg = d.AddChild(0, "", NkAuthor::Humain);
		const int32 bt = d.AddChild(pg, "", NkAuthor::Humain);
		d.nodes[(uint32)bt].label = NkString("Bouton");
		const int32 tx = d.AddChild(bt, "", NkAuthor::Humain);
		d.nodes[(uint32)tx].label = NkString("Libelle");
		d.nodes[(uint32)tx].text = NkString("Valider");
		// ⚠️ ANTI-DONNEES-DEGENEREES : la declaration porte une DESCENDANCE
		//    (2 nœuds), sinon « materialisee » et « vide » se confondraient et
		//    les volets (a)/(b) passeraient a vide.
		const int32 decl = d.ExtraireComposant(bt, "", "bouton");
		const uint32 avant = (uint32)d.nodes.Size();
		// (a) POSER : une nouvelle instance nait sous la page, materialisee
		//     (le document gagne exactement les 2 nœuds de l'arbre).
		const int32 neuf = d.InstancierComposant(decl, pg);
		const bool posee = neuf > 0 && (uint32)d.nodes.Size() == avant + 2u
						   && !d.nodes[(uint32)neuf].instanceDe.Empty()
						   && d.nodes[(uint32)neuf].children.Size() == 1u;
		// (b) ENTIERE, PAR L'EFFET : le texte du libelle a voyage. Un lien pose
		//     sans descendance passerait (a) modifie... et tomberait ICI.
		bool entiere = false;
		if (posee) {
			const int32 enf = d.nodes[(uint32)neuf].children[0];
			const char *t = d.nodes[(uint32)enf].text.Data();
			entiere = t && strcmp(t, "Valider") == 0;
		}
		// (c) INDEPENDANTES : detacher la PREMIERE instance ne detache pas la
		//     seconde -- deux instances ne partagent pas leur lien.
		const bool independantes = posee && d.DetacherInstance(bt)
								   && !d.nodes[(uint32)neuf].instanceDe.Empty();
		// (d) REFUS FRANCS : declaration inconnue, parent invalide.
		const bool refus = d.InstancierComposant(99, pg) == -1
						   && d.InstancierComposant(decl, 9999) == -1;
		char det[224];
		nkentseu::NkSnprintf(det, sizeof(det),
				 "posee=%d (nœuds %u -> %u, enfants=%u) ; libelle voyage=%d ; "
				 "detacher l'une laisse l'autre liee=%d ; refus (decl 99, parent 9999)=%d",
				 posee ? 1 : 0, avant, (uint32)d.nodes.Size(),
				 neuf > 0 ? (uint32)d.nodes[(uint32)neuf].children.Size() : 0u, entiere ? 1 : 0,
				 independantes ? 1 : 0, refus ? 1 : 0);
		verdict("COMPOSANTS (reutiliser) : `InstancierComposant` pose une NOUVELLE instance "
				"entiere depuis la declaration, les instances sont independantes au detachement, "
				"et les entrees invalides refusent franchement",
				posee && entiere && independantes && refus, det);
	}

	// ── L'ARRONDI PAR COIN, ET LA BORDURE QUI L'EPOUSE ──────────────────────
	// 🔑 UN SEUL CAS POUR LES DEUX DEFAUTS, ET C'EST VOULU : *un contour carre
	//    sur une forme carree ne se remarque pas.* C'est en arrondissant que le
	//    defaut de la bordure apparait -- et l'arrondi par coin n'existant pas,
	//    personne n'avait pousse le cas. **Le manque du modele cachait le defaut
	//    du peintre.**
	{
		NkUIDocument d;
		d.NewDocument("coins", NkAuthor::Humain);
		const int32 pg = d.AddChild(0, "", NkAuthor::Humain);
		d.nodes[(uint32)pg].shape = NkString("frame");
		d.nodes[(uint32)pg].layout.kind = NkLayoutKind::Free;
		d.nodes[(uint32)pg].width.mode = NkSizeMode::Fixed;
		d.nodes[(uint32)pg].width.value = 300.f;
		d.nodes[(uint32)pg].height.mode = NkSizeMode::Fixed;
		d.nodes[(uint32)pg].height.value = 200.f;
		const int32 rc = d.AddChild(pg, "", NkAuthor::Humain);
		d.nodes[(uint32)rc].shape = NkString("rect");
		d.nodes[(uint32)rc].width.mode = NkSizeMode::Fixed;
		d.nodes[(uint32)rc].width.value = 120.f;
		d.nodes[(uint32)rc].height.mode = NkSizeMode::Fixed;
		d.nodes[(uint32)rc].height.value = 80.f;
		d.nodes[(uint32)rc].fill = NkString("#0969da");
		// (a) ADDITIVITE : un rayon UNIQUE garde la cle simple, octet pour octet.
		d.nodes[(uint32)rc].radius = 8.f;
		NkString simple;
		d.Save(simple);
		const bool cleSimple = strstr(simple.Data(), "rayons") == nullptr
							   && strstr(simple.Data(), "rayon = 8") != nullptr;
		NkUIDocument r0;
		NkString simple2;
		const bool octetPourOctet = r0.Load(simple.Data()) && (r0.Save(simple2), true)
									&& strcmp(simple2.Data(), simple.Data()) == 0;
		// (b) QUATRE RAYONS DIFFERENTS voyagent, dans l'ordre horaire.
		d.nodes[(uint32)rc].rayonsDelies = true;
		d.nodes[(uint32)rc].rayonsCoins[0] = 2.f;
		d.nodes[(uint32)rc].rayonsCoins[1] = 10.f;
		d.nodes[(uint32)rc].rayonsCoins[2] = 20.f;
		d.nodes[(uint32)rc].rayonsCoins[3] = 0.f;
		NkString quatre;
		d.Save(quatre);
		NkUIDocument r1;
		const bool relit = r1.Load(quatre.Data()) && r1.IsValidIndex(rc);
		const bool voyagent = relit && r1.nodes[(uint32)rc].rayonsDelies
							  && r1.nodes[(uint32)rc].RayonCoin(0) == 2.f
							  && r1.nodes[(uint32)rc].RayonCoin(1) == 10.f
							  && r1.nodes[(uint32)rc].RayonCoin(2) == 20.f
							  && r1.nodes[(uint32)rc].RayonCoin(3) == 0.f;
		// (c) LA PORTE : delies a false, les quatre valent le rayon simple.
		NkUINode nu;
		nu.radius = 6.f;
		const bool porte = nu.RayonCoin(0) == 6.f && nu.RayonCoin(3) == 6.f;
		// (d) 🔑 LE PEINTRE HONORE LES QUATRE, ET LA BORDURE LES SUIT. Quatre
		//     rayons distincts se composent en PLUS de commandes qu'un rayon
		//     uniforme ; la bordure arrondie en ajoute autant.
		auto compter = [&](NkUIDocument &doc) -> uint32 {
			NkLayoutResult lay;
			NkComputeLayout(doc, NkPaintRect{0.f, 0.f, 300.f, 200.f}, lay);
			NkComponentInput in;
			nkentseu::editorkit::NkRecordingPaint pv;
			NkDocumentHost hote;
			hote.SyncTo(doc);
			pv.Reset();
			NkDrawDocument(pv, in, doc, lay, hote, 0);
			return (uint32)pv.cmds.Size();
		};
		NkUIDocument u;
		u.Load(simple.Data());
		const uint32 cmdUniforme = compter(u);
		const uint32 cmdQuatre = compter(r1);
		const bool composeQuatre = cmdQuatre > cmdUniforme;
		// la bordure : elle DOIT ajouter des commandes qui suivent les coins
		NkBordure b;
		b.couleur = NkString("#ff0000");
		b.epaisseur = 3.f;
		b.position = NkBordurePos::Interieur;
		r1.nodes[(uint32)rc].borders.PushBack(b);
		const uint32 cmdBord = compter(r1);
		const bool bordureEpouse = cmdBord > cmdQuatre + 2u;
		char det[256];
		nkentseu::NkSnprintf(det, sizeof(det),
				 "cle simple gardee=%d, octet pour octet=%d ; quatre rayons voyagent=%d ; "
				 "porte (delies=false -> rayon simple)=%d ; peintre : %u cmd uniforme -> %u a "
				 "quatre coins=%d -> %u avec bordure=%d",
				 cleSimple ? 1 : 0, octetPourOctet ? 1 : 0, voyagent ? 1 : 0, porte ? 1 : 0,
				 cmdUniforme, cmdQuatre, composeQuatre ? 1 : 0, cmdBord,
				 bordureEpouse ? 1 : 0);
		verdict("ARRONDI PAR COIN + BORDURE QUI L'EPOUSE : la cle simple survit (octet pour "
				"octet), quatre rayons distincts voyagent dans l'ordre horaire, la porte "
				"`RayonCoin` rend le rayon simple quand rien n'est delie, et le PEINTRE compose "
				"les quatre coins ET l'anneau de bordure",
				cleSimple && octetPourOctet && voyagent && porte && composeQuatre
					&& bordureEpouse,
				det);
	}

	// ── LE CATALOGUE : CHAQUE ENTREE DU REGISTRE SE POSE ET SE DESSINE ──────
	// 🔑 LE SEUL CONTROLE QUI PASSE A L'ECHELLE. Eprouver trois echantillons
	//    dirait que trois marchent ; ce cas parcourt TOUT le registre et exige de
	//    chaque entree qu'elle (a) s'instancie sans refus et (b) **emette des
	//    commandes de peintre**. Le jour ou quelqu'un declare un composant sans
	//    le dessiner, c'est ici que ca tombe -- pas dans la palette de Rodolf.
	//
	// ⚠️ (b) EST LE VOLET QUI COMPTE. Une declaration seule passe (a) sans
	//    broncher : le registre ne sait pas dessiner. C'est le peintre qui
	//    distingue un composant d'une promesse -- et `DrawPlaceholder` emettant
	//    lui aussi des commandes, on exige EN PLUS que le nom ne soit pas rendu
	//    par le repli. *Un banc qui compte les commandes sans regarder QUI les
	//    emet validerait 106 rectangles gris.*
	{
		// LE BANC APPELLE LA MEME PORTE QUE LA FENETRE -- pas une copie.
		DesignState::PeuplerRegistre();
		const uint16 nReg = NkComponentRegistry::Count();
		uint32 poses = 0, dessines = 0, reserves = 0;
		char premierRate[64] = {0};
		for (uint16 ci = 0; ci < nReg; ++ci) {
			const NkComponentDecl *dcl = NkComponentRegistry::At(ci);
			if (!dcl || !dcl->name || !dcl->name[0])
				continue;
			NkUIDocument d;
			d.NewDocument("catalogue", NkAuthor::Humain);
			const int32 pg = d.AddChild(0, "", NkAuthor::Humain);
			d.nodes[(uint32)pg].shape = NkString("frame");
			d.nodes[(uint32)pg].layout.kind = NkLayoutKind::Free;
			d.nodes[(uint32)pg].width.mode = NkSizeMode::Fixed;
			d.nodes[(uint32)pg].width.value = 300.f;
			d.nodes[(uint32)pg].height.mode = NkSizeMode::Fixed;
			d.nodes[(uint32)pg].height.value = 200.f;
			const int32 cn = d.AddChild(pg, "", NkAuthor::Humain);
			if (cn < 0)
				continue;
			// POSER : c'est le geste de la palette, reduit a son effet.
			d.nodes[(uint32)cn].component = NkString(dcl->name);
			d.nodes[(uint32)cn].text = NkString("Essai");
			d.nodes[(uint32)cn].width.mode = NkSizeMode::Fixed;
			d.nodes[(uint32)cn].width.value = 160.f;
			d.nodes[(uint32)cn].height.mode = NkSizeMode::Fixed;
			d.nodes[(uint32)cn].height.value = 32.f;
			++poses;
			NkLayoutResult lay;
			NkComputeLayout(d, NkPaintRect{0.f, 0.f, 300.f, 200.f}, lay);
			NkComponentInput in;
			nkentseu::editorkit::NkRecordingPaint pv;
			NkDocumentHost hote;
			hote.SyncTo(d);
			pv.Reset();
			NkDrawDocument(pv, in, d, lay, hote, 0);
			// LE RESERVE SE RECONNAIT A SA PHRASE : `DrawPlaceholder` ecrit
			// « declare, dessin non branche ». On la cherche dans le flux.
			bool estReserve = false;
			uint32 cmds = 0;
			for (uint32 k = 0; k < (uint32)pv.cmds.Size(); ++k) {
				++cmds;
				const char *txt = pv.cmds[k].text.Data();
				if (txt && strstr(txt, "dessin non branche"))
					estReserve = true;
			}
			if (estReserve) {
				++reserves;
				if (!premierRate[0])
					nkentseu::NkSnprintf(premierRate, sizeof(premierRate), "%s", dcl->name);
			} else if (cmds > 0)
				++dessines;
			else if (!premierRate[0])
				nkentseu::NkSnprintf(premierRate, sizeof(premierRate), "%s (rien)", dcl->name);
		}
		char det[240];
		nkentseu::NkSnprintf(det, sizeof(det),
				 "%u entree(s) au registre ; %u posee(s) sans refus ; %u DESSINEE(S) ; "
				 "%u reserve(s)%s%s",
				 (uint32)nReg, poses, dessines, reserves, premierRate[0] ? " -- premier : " : "",
				 premierRate[0] ? premierRate : "");
		verdict("CATALOGUE : CHAQUE entree du registre se pose ET se dessine -- aucune n'est "
				"une promesse (un reserve « dessin non branche » fait tomber ce cas)",
				nReg > 0u && poses == (uint32)nReg && dessines == (uint32)nReg
					&& reserves == 0u,
				det);
	}

	// ── LE GLISSER-DEPOSER DEPUIS LA PALETTE ───────────────────────────────
	// 🔑 CE QUI EST EPROUVE ICI EST LA **DECISION**, pas la plomberie de souris.
	//    Le geste lui-meme (seuil, fantome, livraison) appartient a NKGui et est
	//    eprouve chez lui ; ce qui est A NOUS est : dans quel noeud du DOCUMENT
	//    tombe le composant, ou exactement apres aimantation, et ce qui se dit
	//    quand rien ne peut recevoir. La toile appelle EXACTEMENT ces deux
	//    fonctions -- elle n'en recopie aucune, sinon l'apercu et le depot
	//    pourraient diverger.
	//
	// ⚠️ AUCUNE COORDONNEE N'EST SUPPOSEE : les cibles se LISENT dans la
	//    disposition resolue. Un banc qui viserait « 150, 100 » parce que la
	//    page fait 300x200 casserait le jour ou la racine change d'agencement,
	//    et il casserait en accusant l'aimant.
	{
		NkUIDocument d;
		d.NewDocument("glisser", NkAuthor::Humain);
		const int32 pg = d.AddChild(0, "", NkAuthor::Humain);
		d.nodes[(uint32)pg].shape = NkString("frame");
		d.nodes[(uint32)pg].label = NkString("Page");
		d.nodes[(uint32)pg].layout.kind = NkLayoutKind::Free;
		d.nodes[(uint32)pg].width.mode = NkSizeMode::Fixed;
		d.nodes[(uint32)pg].width.value = 300.f;
		d.nodes[(uint32)pg].height.mode = NkSizeMode::Fixed;
		d.nodes[(uint32)pg].height.value = 200.f;
		// UN VOISIN, pour que l'aimant ait quelque chose a quoi s'aligner.
		const int32 vs = d.AddChild(pg, "", NkAuthor::Humain);
		d.nodes[(uint32)vs].shape = NkString("rect");
		d.nodes[(uint32)vs].posX = 100.f;
		d.nodes[(uint32)vs].posY = 50.f;
		d.nodes[(uint32)vs].width.mode = NkSizeMode::Fixed;
		d.nodes[(uint32)vs].width.value = 40.f;
		d.nodes[(uint32)vs].height.mode = NkSizeMode::Fixed;
		d.nodes[(uint32)vs].height.value = 20.f;
		NkLayoutResult lay;
		NkComputeLayout(d, NkPaintRect{0.f, 0.f, 800.f, 600.f}, lay);
		const bool disposee = lay.Has(pg) && lay.Has(vs);
		const NkPaintRect rPage = disposee ? lay.At(pg) : NkPaintRect{0.f, 0.f, 0.f, 0.f};
		const NkPaintRect rVois = disposee ? lay.At(vs) : NkPaintRect{0.f, 0.f, 0.f, 0.f};
		// Vue a l'identite : l'espace ECRAN et l'espace DOCUMENT coincident, donc
		// la MEME disposition sert au pointage et a l'aimantation.
		const float32 cx = rPage.x + rPage.w * 0.5f;
		const float32 cy = rPage.y + rPage.h * 0.5f;

		// 1. LE DEPOT POSE DANS LE CONTENEUR LIBRE SOUS LE CURSEUR.
		DesignState::PeuplerRegistre();
		glisser::NkVise v1 =
			glisser::NkViserDepot(d, lay, lay, cx, cy, cx, cy, false, 6.f);
		NkString dit1;
		const int32 n1 = glisser::NkDeposerVise(d, v1, "bouton", dit1);
		const bool pose = n1 >= 0 && d.IsValidIndex(n1)
						  && d.nodes[(uint32)n1].parent == pg
						  && d.nodes[(uint32)n1].component == NkString("bouton");
		char det1[224];
		nkentseu::NkSnprintf(det1, sizeof(det1), "vise=%d pose=%d parent=%d compo=%s dit=%s",
				 v1.parent, n1, n1 >= 0 ? d.nodes[(uint32)n1].parent : -1,
				 n1 >= 0 ? d.nodes[(uint32)n1].component.Data() : "-", dit1.Data());
		verdict("glisser : lache dans une page, le composant Y est pose (pas ailleurs)",
				disposee && pose && !dit1.Empty(), det1);

		// 2. HORS DE TOUT CONTENEUR LIBRE : RIEN N'EST CREE, ET LE REFUS PARLE.
		//    ⚠️ On compte les noeuds AVANT et APRES : « il a rendu -1 » n'est pas
		//       « il n'a rien ecrit ».
		//    ⚠️ LIMITE ASSUMEE DE CE CAS, ecrite pour qu'on ne la prenne pas
		//       pour une preuve : comparer `dit2` a `NkRefusHorsConteneur()`
		//       est TAUTOLOGIQUE -- les deux sortent de la meme fonction. Ce
		//       cas prouve que le refus PARLE et que rien n'est ecrit ; il ne
		//       prouve PAS que l'outil de trace dit la meme phrase. Cette
		//       propriete-la est STRUCTURELLE (un seul litteral dans les
		//       sources, les deux appelants passant par la fonction) et se
		//       verifie par un `grep`, pas d'ici. *Une comparaison qui ne peut
		//       pas echouer n'est pas un controle, c'est une decoration.*
		const uint32 avant = (uint32)d.nodes.Size();
		const float32 hx = rPage.x + rPage.w + 400.f;
		const float32 hy = rPage.y + rPage.h + 400.f;
		glisser::NkVise v2 =
			glisser::NkViserDepot(d, lay, lay, hx, hy, hx, hy, false, 6.f);
		NkString dit2;
		const int32 n2 = glisser::NkDeposerVise(d, v2, "bouton", dit2);
		const bool refus = !v2.possible && n2 < 0 && (uint32)d.nodes.Size() == avant
						   && dit2 == NkString(glisser::NkRefusHorsConteneur());
		char det2[224];
		nkentseu::NkSnprintf(det2, sizeof(det2), "possible=%d rendu=%d noeuds %u->%u dit=%s",
				 v2.possible ? 1 : 0, n2, avant, (uint32)d.nodes.Size(), dit2.Data());
		verdict("glisser : hors conteneur, RIEN n'est cree et le refus dit LA phrase "
				"des outils de trace",
				refus, det2);

		// 3. L'AIMANT DEPLACE LE POINT DE DEPOT -- et c'est bien LUI qui le fait.
		//    Deux visees a la MEME position : l'une aimantee, l'autre non. Sans la
		//    seconde, un point qui tombe juste prouverait l'aimant sans l'aimant.
		const float32 vx = rVois.x + 3.f; // dans la tolerance du bord gauche
		const float32 vy = rVois.y + 3.f;
		glisser::NkVise sans =
			glisser::NkViserDepot(d, lay, lay, vx, vy, vx, vy, false, 6.f);
		glisser::NkVise avec =
			glisser::NkViserDepot(d, lay, lay, vx, vy, vx, vy, true, 6.f);
		const bool brut = sans.possible && sans.docX == vx && sans.docY == vy;
		const bool colle = avec.possible && avec.docX == rVois.x && avec.docY == rVois.y;
		char det3[240];
		nkentseu::NkSnprintf(det3, sizeof(det3),
				 "voisin=(%.1f,%.1f) vise=(%.1f,%.1f) sans=(%.1f,%.1f) avec=(%.1f,%.1f)",
				 rVois.x, rVois.y, vx, vy, sans.docX, sans.docY, avec.docX, avec.docY);
		verdict("glisser : l'aimant colle le point au bord du voisin, et sans lui le "
				"point reste brut (c'est l'aimant qui bouge, pas autre chose)",
				brut && colle, det3);

		// 4. LE POINT AIMANTE EST CELUI QUI EST ECRIT. La visee ne sert a rien si
		//    le depot recalcule -- c'est le defaut « l'apercu montre A, le depot
		//    ecrit B », invisible en banc si l'on ne compare pas les deux.
		NkString dit4;
		const int32 n4 = glisser::NkDeposerVise(d, avec, "etiquette", dit4);
		const bool ecrit = n4 >= 0 && d.nodes[(uint32)n4].posX == avec.docX
						   && d.nodes[(uint32)n4].posY == avec.docY;
		char det4[224];
		nkentseu::NkSnprintf(det4, sizeof(det4), "vise=(%.2f,%.2f) ecrit=(%.2f,%.2f)", avec.docX,
				 avec.docY, n4 >= 0 ? d.nodes[(uint32)n4].posX : -1.f,
				 n4 >= 0 ? d.nodes[(uint32)n4].posY : -1.f);
		verdict("glisser : le noeud pose est ECRIT au point AIMANTE -- le depot ne "
				"redecide rien",
				ecrit, det4);

		// 5. UN COMPOSANT ABSENT DU REGISTRE EST REFUSE, ET LE REFUS LE NOMME.
		const uint32 avant5 = (uint32)d.nodes.Size();
		NkString dit5;
		const int32 n5 = glisser::NkDeposerVise(d, v1, "ce_composant_n_existe_pas", dit5);
		const bool nomme = n5 < 0 && (uint32)d.nodes.Size() == avant5
						   && dit5.Data() != nullptr
						   && strstr(dit5.Data(), "ce_composant_n_existe_pas") != nullptr;
		char det5[224];
		nkentseu::NkSnprintf(det5, sizeof(det5), "rendu=%d noeuds %u->%u dit=%s", n5, avant5,
				 (uint32)d.nodes.Size(), dit5.Data());
		verdict("glisser : un composant absent du registre est refuse, et le refus le "
				"NOMME (jamais « impossible » tout court)",
				nomme, det5);
	}

	// ── UN COMPOSANT SYSTEME SE POSE DEPUIS LA LISTE ───────────────────────
	// 🔴 IL SE FAISAIT RENVOYER AILLEURS. Le double-clic repondait « composant
	//    systeme : il se pose depuis la palette du rail, pas d'ici », et Rodolf
	//    a repondu « je ne comprends pas ». Cette phrase parlait de NOTRE
	//    architecture -- la separation §15.1 entre composants du kit et
	//    composants du document. Elle est vraie dans le code et n'a aucun sens
	//    sous la main : les deux natures paraissent dans LA MEME liste.
	//    *Un composant visible dans une liste doit pouvoir se poser ; sinon
	//    c'est la liste qui est mal faite, pas le geste.*
	//
	// ⚠️ CE CAS EXIGE UN EFFET, PAS L'ABSENCE D'UN MESSAGE. Verifier que le
	//    refus a disparu laisserait passer un geste devenu muet -- c'est-a-dire
	//    le remplacement d'un mauvais message par rien du tout.
	{
		DesignState::PeuplerRegistre();
		DesignState st;
		st.doc.NewDocument("systeme", NkAuthor::Humain);
		const int32 pg = st.doc.AddChild(0, "", NkAuthor::Humain);
		st.doc.nodes[(uint32)pg].label = NkString("Page");
		st.doc.nodes[(uint32)pg].layout.kind = NkLayoutKind::Free;
		// 🔴 UN NOEUD EST SELECTIONNE, ET C'EST TOUT L'OBJET DU CAS. Rodolf,
		//    03/09 : « je deplace Bouton_Connexion, ca deplace aussi le bouton
		//    que je viens d'ajouter, comme s'ils etaient lies », et « je
		//    n'arrive pas a le selectionner ». UN SEUL defaut derriere les
		//    deux : la pose prenait la SELECTION pour parent, donc le composant
		//    entrait DANS l'objet selectionne.
		//    *Poser a cote de ce qui est selectionne est ce qu'attend la main ;
		//    poser dedans est une decision d'arbre que personne n'a demandee.*
		// ⚠️ LE CAS D'AVANT SELECTIONNAIT RIEN (`SelectClear`), donc il ne
		//    pouvait PAS voir ce defaut : il mesurait le repli, jamais la
		//    regle. *Un cas qui n'exerce que le chemin vide valide le chemin
		//    vide.*
		const int32 cible = st.doc.AddChild(pg, "", NkAuthor::Humain);
		st.doc.nodes[(uint32)cible].label = NkString("Deja la");
		st.SelectSingle(cible);
		const uint32 avant = (uint32)st.doc.nodes.Size();
		// LE COMPOSANT VISE EST LU DU REGISTRE, jamais nomme en dur : un essai
		// qui ecrirait « bouton » cesserait de mesurer le jour ou la table
		// change, et il le ferait en silence.
		const NkComponentDecl *d0 = NkComponentRegistry::At(0);
		const bool pose = d0 && NkPoserComposantSysteme(st, d0->name);
		const uint32 apres = (uint32)st.doc.nodes.Size();
		const int32 neuf = (int32)apres - 1;
		// LE PARENT EST LA PAGE, ET SURTOUT PAS LA SELECTION.
		const bool bonParent = pose && st.doc.IsValidIndex(neuf)
							   && st.doc.nodes[(uint32)neuf].parent == pg
							   && st.doc.nodes[(uint32)neuf].parent != cible;
		const bool bonCompo = pose && d0
							  && st.doc.nodes[(uint32)neuf].component == NkString(d0->name);
		// ⚠️ ET LE MESSAGE NE DOIT PLUS RENVOYER AILLEURS : on cherche la phrase
		//    qui parlait d'architecture, pas une phrase precise -- exiger un
		//    libelle exact ferait tomber ce cas a la premiere reformulation.
		const bool sansRenvoi =
			st.status.Data() != nullptr && strstr(st.status.Data(), "palette du rail") == nullptr;
		char det[240];
		nkentseu::NkSnprintf(det, sizeof(det), "compo=%s pose=%d noeuds %u->%u parent=%d (page=%d, "
				 "selection=%d) dit=%s",
				 d0 && d0->name ? d0->name : "(aucun)", pose ? 1 : 0, avant, apres,
				 st.doc.IsValidIndex(neuf) ? st.doc.nodes[(uint32)neuf].parent : -1, pg,
				 cible, st.status.Data() ? st.status.Data() : "(muet)");
		verdict("composant systeme : le double-clic le POSE DANS LA PAGE, jamais dans "
				"l'objet selectionne (il ne renvoie plus vers « la palette du rail »)",
				pose && apres == avant + 1u && bonParent && bonCompo && sansRenvoi
					&& !st.status.Empty(),
				det);
	}

	// ── AUCUNE FORME DE NOEUD NE SE DESSINE SANS SON RAYON ─────────────────
	// 🔴 TROISIEME RETOUR DU MEME DEFAUT, ET LA CAUSE EST STRUCTURELLE. Rodolf
	//    a photographie un fond arrondi souligne d'un angle droit. Corrige une
	//    fois sur le chemin principal, il est revenu par les REPLIS -- puis je
	//    l'ai reproduit MOI-MEME, le lendemain, dans les huit composants de
	//    base : `Fill(r, role, rayon)` suivi de `OutlineSharp(r, bord)`.
	//
	//    La cause n'est pas l'etourderie : dans `NkComponentPaint`, `rounding` a
	//    une VALEUR PAR DEFAUT de `0.f` -- l'oublier compile en silence -- et
	//    `OutlineSharp` n'offre AUCUN parametre de rayon. *Une signature qu'on
	//    peut mal appeler sans erreur sera mal appelee, et le compte des fois ne
	//    mesure que le temps ecoule.*
	//
	// ⚠️ CE CAS NE COMPTE PAS LES ARRONDIS : il constate qu'AUCUNE commande
	//    `OutlineSharp` ne sort du peintre alors que tous les noeuds portent un
	//    rayon. C'est l'EFFET, et il est verifiable sans ecran. Un cas qui
	//    aurait compte les appels corriges serait reste vert le jour ou
	//    quelqu'un en ajoute un septieme.
	//
	// ⚠️ CE QU'IL NE COUVRE PAS, ECRIT POUR QU'ON NE LUI PRETE PAS PLUS.
	//    Tous les noeuds de ce document RECOIVENT UN FOND (explicite ou par
	//    role). Un contour carre y serait donc forcement pose PAR-DESSUS un
	//    fond arrondi -- le defaut photographie. Il reste un chemin ou
	//    `OutlineSharp` est LEGITIME : celui ou AUCUN fond n'est peint (tous
	//    les remplissages masques). La, il n'existe aucune couleur pour
	//    creuser l'interieur, et ce peintre n'a ni primitive d'anneau ni
	//    masque : `AddRect` non rempli est la seule forme creuse, et elle ne
	//    sait pas arrondir. **J'ai essaye l'anneau sur ce chemin : il peignait
	//    un bloc plein couleur bordure, et c'est le cas « un rectangle neuf
	//    n'a pas de bordure » qui l'a vu.** Manque porte au canal.
	{
		DesignState::PeuplerRegistre();
		NkUIDocument d;
		d.NewDocument("rayons", NkAuthor::Humain);
		const int32 pg = d.AddChild(0, "", NkAuthor::Humain);
		d.nodes[(uint32)pg].shape = NkString("frame");
		d.nodes[(uint32)pg].layout.kind = NkLayoutKind::Free;
		d.nodes[(uint32)pg].width.mode = NkSizeMode::Fixed;
		d.nodes[(uint32)pg].width.value = 400.f;
		d.nodes[(uint32)pg].height.mode = NkSizeMode::Fixed;
		d.nodes[(uint32)pg].height.value = 400.f;
		d.nodes[(uint32)pg].radius = 12.f; // l'artboard AUSSI porte un rayon
		// Un echantillon de chemins qui retombaient sur le contour carre : une
		// forme nue (repli du theme), un cadre d'image, et les composants de
		// base qui dessinent un fond ET un trait.
		const char *sujets[] = {"", "", "bouton", "champ_texte", "case_a_cocher",
								"barre_progression", "carte"};
		const char *formes[] = {"rect", "image", "", "", "", "", ""};
		const uint32 nSujets = (uint32)(sizeof(sujets) / sizeof(sujets[0]));
		for (uint32 i = 0; i < nSujets; ++i) {
			const int32 c = d.AddChild(pg, sujets[i], NkAuthor::Humain);
			if (c < 0)
				continue;
			NkUINode &nd = d.nodes[(uint32)c];
			if (formes[i][0])
				nd.shape = NkString(formes[i]);
			nd.radius = 12.f;
			nd.posX = 10.f;
			nd.posY = 10.f + (float32)i * 40.f;
			nd.width.mode = NkSizeMode::Fixed;
			nd.width.value = 200.f;
			nd.height.mode = NkSizeMode::Fixed;
			nd.height.value = 32.f;
		}
		NkLayoutResult lay;
		NkComputeLayout(d, NkPaintRect{0.f, 0.f, 500.f, 500.f}, lay);
		NkComponentInput in;
		nkentseu::editorkit::NkRecordingPaint pv;
		NkDocumentHost hote;
		hote.SyncTo(d);
		NkDrawDocument(pv, in, d, lay, hote, 0);
		uint32 carres = 0, arrondis = 0;
		for (uint32 k = 0; k < (uint32)pv.cmds.Size(); ++k) {
			if (pv.cmds[k].op == NkPaintOp::OutlineSharp)
				++carres;
			if (pv.cmds[k].rounding > 0.f)
				++arrondis;
		}
		char det[200];
		nkentseu::NkSnprintf(det, sizeof(det),
				 "%u noeud(s) a rayon 12 ; %u commande(s) ; %u contour(s) CARRE(S) ; "
				 "%u commande(s) arrondie(s)",
				 nSujets + 1u, (uint32)pv.cmds.Size(), carres, arrondis);
		// ⚠️ LES DEUX MOITIES COMPTENT. Sans `arrondis > 0`, un peintre qui ne
		//    dessinerait plus RIEN passerait ce cas : zero carre, zero tout.
		verdict("arrondis : avec des noeuds a rayon, le peintre de document n'emet "
				"AUCUN contour carre -- et il dessine bien des formes arrondies",
				carres == 0u && arrondis > 0u, det);
	}

	// ── LE BORNAGE DES RAYONS : LE CAS EXACT DE LA CAPTURE ──────────────────
	// 🔴 Rodolf, 03/09 : `Bouton_Connexion`, hauteur `fixed 36`, arrondi
	//    haut-droit **50,50** -- presque trois fois le maximum possible (18).
	//    Sa capture montre le bord droit en oblique et une ENCOCHE BLANCHE en bas
	//    a droite. Deux fautes empilees, et la seconde etait la mienne :
	//      1. aucun bornage du rayon contre la boite ;
	//      2. ma decomposition prenait le MAXIMUM des rayons adjacents pour ses
	//         bandes -- avec 18 d'un cote et 4 de l'autre, il restait un trou.
	//    ⚠️ *Une decomposition qui marche quand les quatre valeurs sont egales
	//       n'est pas verifiee : c'est le cas ou elle ne peut pas echouer.*
	{
		// (a) LA PORTE DE BORNAGE, seule -- elle se mesure sans peintre.
		const float32 R1[4] = {4.f, 50.5f, 4.f, 4.f};
		float32 B1[4];
		nkuidesign::renderdetail::NkGBornerRayons(200.f, 36.f, R1, B1);
		const bool borne = B1[1] == 18.f && B1[0] == 4.f && B1[3] == 4.f;
		// (b) LA REGLE CSS : deux coins d'un MEME BORD ne totalisent pas plus que
		//     ce bord. 18 + 18 sur un bord de 30 doit descendre a 15 + 15.
		const float32 R2[4] = {18.f, 18.f, 0.f, 0.f};
		float32 B2[4];
		nkuidesign::renderdetail::NkGBornerRayons(30.f, 40.f, R2, B2);
		const bool paire = (B2[0] + B2[1]) <= 30.001f && B2[0] > 14.f && B2[0] < 15.1f;
		// (c) 🔑 LA VALEUR VOULUE RESTE DANS LE DOCUMENT : borner est un geste du
		//     DESSIN, pas de la saisie. Si Rodolf agrandit le bouton, son 50,50
		//     revient tout seul -- on le prouve en re-bornant sur une boite plus
		//     grande sans avoir touche a la source.
		float32 B3[4];
		nkuidesign::renderdetail::NkGBornerRayons(200.f, 140.f, R1, B3);
		const bool revient = B3[1] == 50.5f;
		// (d) AUCUNE ENCOCHE : la surface peinte couvre bien la boite. On compte
		//     les commandes -- une decomposition qui laisse un trou en emet le
		//     meme nombre, donc on mesure la COUVERTURE par les rectangles rendus.
		NkUIDocument d;
		d.NewDocument("bornage", NkAuthor::Humain);
		const int32 pg = d.AddChild(0, "", NkAuthor::Humain);
		d.nodes[(uint32)pg].shape = NkString("frame");
		d.nodes[(uint32)pg].layout.kind = NkLayoutKind::Free;
		d.nodes[(uint32)pg].width.mode = NkSizeMode::Fixed;
		d.nodes[(uint32)pg].width.value = 300.f;
		d.nodes[(uint32)pg].height.mode = NkSizeMode::Fixed;
		d.nodes[(uint32)pg].height.value = 200.f;
		const int32 bt = d.AddChild(pg, "", NkAuthor::Humain);
		d.nodes[(uint32)bt].shape = NkString("rect");
		d.nodes[(uint32)bt].width.mode = NkSizeMode::Fixed;
		d.nodes[(uint32)bt].width.value = 200.f;
		d.nodes[(uint32)bt].height.mode = NkSizeMode::Fixed;
		d.nodes[(uint32)bt].height.value = 36.f;
		d.nodes[(uint32)bt].fill = NkString("#0969da");
		d.nodes[(uint32)bt].rayonsDelies = true;
		d.nodes[(uint32)bt].rayonsCoins[0] = 4.f;
		d.nodes[(uint32)bt].rayonsCoins[1] = 50.5f;
		d.nodes[(uint32)bt].rayonsCoins[2] = 4.f;
		d.nodes[(uint32)bt].rayonsCoins[3] = 4.f;
		NkLayoutResult lay;
		NkComputeLayout(d, NkPaintRect{0.f, 0.f, 300.f, 200.f}, lay);
		NkComponentInput in;
		nkentseu::editorkit::NkRecordingPaint pv;
		NkDocumentHost hote;
		hote.SyncTo(d);
		pv.Reset();
		NkDrawDocument(pv, in, d, lay, hote, 0);
		// ⚠️ ON TESTE L'ENCOCHE LA OU ELLE ETAIT : le coin BAS-DROIT, entre le
		//    carre d'angle (4 px) et la bande bornee par le voisin a 18. Un point
		//    a 10 px du bord droit et 2 px du bas doit etre COUVERT.
		const NkPaintRect rb = lay.Has(bt) ? lay.At(bt) : NkPaintRect{0.f, 0.f, 0.f, 0.f};
		const float32 px = rb.x + rb.w - 10.f, py = rb.y + rb.h - 2.f;
		bool couvert = false;
		for (uint32 k = 0; k < (uint32)pv.cmds.Size() && !couvert; ++k) {
			const auto &cm = pv.cmds[k];
			if (cm.op != nkentseu::editorkit::NkPaintOp::Fill
				&& cm.op != nkentseu::editorkit::NkPaintOp::FillColor)
				continue;
			if (px >= cm.x && px <= cm.x + cm.w && py >= cm.y && py <= cm.y + cm.h)
				couvert = true;
		}
		char det[240];
		nkentseu::NkSnprintf(det, sizeof(det),
				 "50.50 borne a %.1f sur une hauteur de 36=%d ; regle CSS (18+18 sur 30 -> "
				 "%.1f+%.1f)=%d ; la valeur REVIENT sur une boite plus grande=%d ; le coin "
				 "bas-droit est COUVERT (l'encoche)=%d",
				 (double)B1[1], borne ? 1 : 0, (double)B2[0], (double)B2[1], paire ? 1 : 0,
				 revient ? 1 : 0, couvert ? 1 : 0);
		verdict("BORNAGE DES RAYONS : un rayon ne depasse ni la moitie de la boite ni le bord "
				"qu'il partage, la valeur VOULUE reste dans le document (borne au DESSIN, pas a "
				"la saisie), et la decomposition ne laisse plus d'encoche",
				borne && paire && revient && couvert, det);
	}

	// ── LES DEGRADES : LE FORMAT D'ABORD ────────────────────────────────────
	// Les deux exigences posees avant d'ecrire une ligne d'interface :
	// additivite stricte, et relecture d'un type INCONNU sans perte.
	{
		NkUIDocument d;
		d.NewDocument("degrades", NkAuthor::Humain);
		const int32 pg = d.AddChild(0, "", NkAuthor::Humain);
		const int32 bt = d.AddChild(pg, "", NkAuthor::Humain);
		d.nodes[(uint32)bt].label = NkString("Bouton");
		// (a) ADDITIVITE : un remplissage UNI ne gagne aucune cle de degrade.
		{
			NkRemplissage f;
			f.couleur = NkString("#112233");
			d.nodes[(uint32)bt].fills.PushBack(f);
		}
		NkString uni;
		d.Save(uni);
		const bool aucuneCle = strstr(uni.Data(), "degrade_") == nullptr;
		NkUIDocument r0;
		NkString uni2;
		const bool octetPourOctet = r0.Load(uni.Data()) && (r0.Save(uni2), true)
									&& strcmp(uni2.Data(), uni.Data()) == 0;
		// (b) UN DEGRADE A DEUX ARRETS voyage : type, angle, positions, couleurs.
		{
			NkDegrade g;
			g.type = NkString("lineaire");
			g.angle = 90.f;
			NkArretDegrade a1;
			a1.position = 0.f;
			a1.couleur = NkString("#ff0000");
			NkArretDegrade a2;
			a2.position = 1.f;
			a2.couleur = NkString("#0000ff");
			g.arrets.PushBack(a1);
			g.arrets.PushBack(a2);
			d.nodes[(uint32)bt].fills[0].degrade = g;
		}
		NkString avec;
		d.Save(avec);
		NkUIDocument r1;
		const bool relit = r1.Load(avec.Data()) && r1.IsValidIndex(bt)
						   && !r1.nodes[(uint32)bt].fills.Empty();
		const NkDegrade *g1 = relit ? &r1.nodes[(uint32)bt].fills[0].degrade : nullptr;
		const bool voyage = g1 && g1->Actif() && g1->arrets.Size() == 2u
							&& strcmp(g1->type.Data(), "lineaire") == 0 && g1->angle == 90.f
							&& g1->arrets[0].position == 0.f
							&& strcmp(g1->arrets[0].couleur.Data(), "#ff0000") == 0
							&& g1->arrets[1].position == 1.f
							&& strcmp(g1->arrets[1].couleur.Data(), "#0000ff") == 0;
		// (c) LE REENREGISTREMENT EST STABLE.
		NkString avec2;
		if (relit)
			r1.Save(avec2);
		const bool stable = relit && strcmp(avec2.Data(), avec.Data()) == 0;
		// (d) 🔑 UN TYPE INCONNU REVIENT INTACT -- l'exigence qui distingue un
		//     format qui vieillit bien d'un format qui casse a la version
		//     suivante. On n'ecrit PAS « conique » dans le code : on le lit d'un
		//     texte, comme le ferait un fichier venu d'ailleurs.
		NkUIDocument r2;
		NkString source = avec;
		{
			// remplacer « lineaire » par un type que cette version ignore
			const char *pos = strstr(source.Data(), "lineaire");
			bool ok = pos != nullptr;
			if (ok) {
				NkString avant(source.Data());
				avant.Resize((uint32)(pos - source.Data()));
				NkString apres(pos + 8);
				NkString rec = avant;
				rec.Append("conique_futur");
				rec.Append(apres.Data());
				source = rec;
			}
		}
		const bool relitInconnu = r2.Load(source.Data()) && r2.IsValidIndex(bt)
								  && !r2.nodes[(uint32)bt].fills.Empty();
		const NkDegrade *g2 = relitInconnu ? &r2.nodes[(uint32)bt].fills[0].degrade : nullptr;
		NkString reemis;
		if (relitInconnu)
			r2.Save(reemis);
		const bool inconnuIntact = g2 && strcmp(g2->type.Data(), "conique_futur") == 0
								   && g2->arrets.Size() == 2u
								   && strstr(reemis.Data(), "conique_futur") != nullptr;
		// (e) 🔑 ET IL SE PEINT. Sans ce volet, les quatre precedents
		//     prouveraient un format que RIEN NE DESSINE -- exactement la
		//     « propriete neuve qui n'agit pas », pire que son absence.
		//     On compte les commandes du peintre ENREGISTREUR : un fond uni en
		//     pose UNE, un degrade en pose autant que de bandes.
		uint32 cmdUni = 0, cmdDeg = 0;
		{
			NkUIDocument e;
			e.NewDocument("peinture", NkAuthor::Humain);
			const int32 p2 = e.AddChild(0, "", NkAuthor::Humain);
			// ⚠️ LA PAGE DOIT ETRE UN FRAME DIMENSIONNE EN DISPOSITION LIBRE,
			//    sinon `NkComputeLayout` ne POSE rien et le peintre n'emet AUCUNE
			//    commande -- ma premiere version mesurait « 0 -> 0 » et aurait
			//    accuse le rendu des degrades alors que c'est le montage qui
			//    etait vide. **Donnees degenerees, cinquieme fois du chantier**,
			//    et la meme parade : donner au cas de quoi exprimer l'ecart.
			e.nodes[(uint32)p2].shape = NkString("frame");
			e.nodes[(uint32)p2].layout.kind = NkLayoutKind::Free;
			e.nodes[(uint32)p2].width.mode = NkSizeMode::Fixed;
			e.nodes[(uint32)p2].width.value = 300.f;
			e.nodes[(uint32)p2].height.mode = NkSizeMode::Fixed;
			e.nodes[(uint32)p2].height.value = 200.f;
			const int32 rc2 = e.AddChild(p2, "", NkAuthor::Humain);
			e.nodes[(uint32)rc2].shape = NkString("rect");
			e.nodes[(uint32)rc2].width.mode = NkSizeMode::Fixed;
			e.nodes[(uint32)rc2].width.value = 120.f;
			e.nodes[(uint32)rc2].height.mode = NkSizeMode::Fixed;
			e.nodes[(uint32)rc2].height.value = 60.f;
			NkRemplissage f2;
			f2.couleur = NkString("#112233");
			e.nodes[(uint32)rc2].fills.PushBack(f2);
			auto compter = [&]() -> uint32 {
				NkLayoutResult lay;
				NkComputeLayout(e, NkPaintRect{0.f, 0.f, 300.f, 200.f}, lay);
				NkComponentInput in;
				nkentseu::editorkit::NkRecordingPaint pv;
				NkDocumentHost hote;
				hote.SyncTo(e);
				pv.Reset();
				NkDrawDocument(pv, in, e, lay, hote, 0);
				return (uint32)pv.cmds.Size();
			};
			cmdUni = compter();
			NkDegrade g2;
			g2.type = NkString("lineaire");
			NkArretDegrade s1;
			s1.position = 0.f;
			s1.couleur = NkString("#ff0000");
			NkArretDegrade s2;
			s2.position = 1.f;
			s2.couleur = NkString("#0000ff");
			g2.arrets.PushBack(s1);
			g2.arrets.PushBack(s2);
			e.nodes[(uint32)rc2].fills[0].degrade = g2;
			cmdDeg = compter();
		}
		// ⚠️ ON EXIGE UN ECART FRANC, pas « plus grand » : une bande de plus
		//    pourrait venir d'ailleurs. Le degrade en pose 24, on demande au
		//    moins 10 de plus -- le seuil est BAS pour ne pas figer la finesse,
		//    et FRANC pour ne pas passer sur du bruit.
		const bool sePeint = cmdDeg >= cmdUni + 10u;

		char det[256];
		nkentseu::NkSnprintf(det, sizeof(det),
				 "uni : aucune cle degrade=%d, octet pour octet=%d ; deux arrets voyagent=%d ; "
				 "reenregistrement stable=%d ; TYPE INCONNU relu ET reemis intact=%d",
				 aucuneCle ? 1 : 0, octetPourOctet ? 1 : 0, voyage ? 1 : 0, stable ? 1 : 0,
				 inconnuIntact ? 1 : 0);
		{
			char q[96];
			nkentseu::NkSnprintf(q, sizeof(q), " ; PEINT : %u commande(s) uni -> %u avec degrade=%d",
					 cmdUni, cmdDeg, sePeint ? 1 : 0);
			const uint32 l = (uint32)strlen(det);
			if (l + strlen(q) + 1u < sizeof(det))
				nkentseu::NkSnprintf(det + l, sizeof(det) - l, "%s", q);
		}

		verdict("DEGRADES (format) : la cle est ADDITIVE (un remplissage uni ne la gagne pas), "
				"un degrade a deux arrets voyage avec son type, son angle et ses positions, et "
				"un TYPE INCONNU se relit ET se reemet SANS PERTE -- ET IL SE PEINT",
				aucuneCle && octetPourOctet && voyage && stable && inconnuIntact && sePeint,
				det);
	}

	// ── ② LA PROPAGATION, PAR LE GESTE : LES DEUX CAS JUMEAUX ───────────────
	// 🔑 C'EST LE GESTE QUE RODOLF DOIT POUVOIR JOUER A LA MAIN : deux
	//    instances, une surchargee en couleur, on applique la couleur de
	//    l'autre au composant -- les deux suivent, la surchargee garde la
	//    sienne. Le cas passe par `NkActionDuRaccourci` PUIS par le dispatcher,
	//    jamais par `AppliquerAuComposant` en direct : *un geste qui n'existe
	//    pas ne devient pas vrai parce que la fonction dessous est eprouvee.*
	{
		static DesignState sp;
		sp.doc.NewDocument("propagation", NkAuthor::Humain);
		sp.selected = -1;
		const int32 pg = sp.doc.AddChild(0, "", NkAuthor::Humain);
		const int32 bt = sp.doc.AddChild(pg, "", NkAuthor::Humain);
		sp.doc.nodes[(uint32)bt].label = NkString("Bouton");
		sp.doc.nodes[(uint32)bt].fill = NkString("#111111");
		(void)sp.doc.AddChild(bt, "", NkAuthor::Humain);
		const int32 decl = sp.doc.ExtraireComposant(bt, "", "bouton");
		// La SECONDE instance, par la porte de la palette.
		const int32 b2 = sp.doc.InstancierComposant(decl, pg);
		// ⚠️ ANTI-DONNEES-DEGENEREES : les deux instances partent de la MEME
		//    couleur. Si elles differaient deja, « les deux suivent » serait
		//    invérifiable -- on ne saurait pas si l'une a suivi ou si elle
		//    portait ce fond depuis le debut.
		const bool memeDepart = b2 > 0
								&& strcmp(sp.doc.nodes[(uint32)bt].fill.Data(), "#111111") == 0
								&& strcmp(sp.doc.nodes[(uint32)b2].fill.Data(), "#111111") == 0;
		// (a) LA SECONDE EST SURCHARGEE EN COULEUR -- ce que fait la main dans
		//     le champ hexa de l'inspecteur (qui pose le meme bit).
		sp.doc.nodes[(uint32)b2].fill = NkString("#ff0000");
		sp.doc.nodes[(uint32)b2].ecarts |= NkUINode::EcartRemplissages;
		// (b) ON CHANGE LE FOND DE LA PREMIERE, puis on l'applique AU COMPOSANT
		//     par le raccourci Ctrl+Alt+M -> table -> dispatcher.
		sp.doc.nodes[(uint32)bt].fill = NkString("#00ff00");
		sp.SelectSingle(bt);
		const NkActionCtx a = NkActionDuRaccourci(true, false, 'M', true);
		const bool traite = a == NkActionCtx::AppliquerAuComposant
							&& NkAppliquerActionCtx(sp, bt, a);
		// (c) LE JUMEAU 1 : LES DEUX SUIVENT... et la surchargee GARDE la sienne.
		const bool source = strcmp(sp.doc.nodes[(uint32)bt].fill.Data(), "#00ff00") == 0;
		const bool garde = strcmp(sp.doc.nodes[(uint32)b2].fill.Data(), "#ff0000") == 0;
		// (d) UNE TROISIEME instance, NEUVE et non surchargee, doit SUIVRE --
		//     sans elle, (c) passerait sur un code qui ne propage a personne.
		const int32 b3 = sp.doc.InstancierComposant(decl, pg);
		const bool suit = b3 > 0 && strcmp(sp.doc.nodes[(uint32)b3].fill.Data(), "#00ff00") == 0;
		// (e) LE JUMEAU 2 : MODIFIER UNE INSTANCE NE TOUCHE PAS L'AUTRE.
		sp.doc.nodes[(uint32)b3].fill = NkString("#0000ff");
		sp.doc.nodes[(uint32)b3].ecarts |= NkUINode::EcartRemplissages;
		const bool isole = strcmp(sp.doc.nodes[(uint32)bt].fill.Data(), "#00ff00") == 0
						   && strcmp(sp.doc.nodes[(uint32)b2].fill.Data(), "#ff0000") == 0;
		// (f) REFUS FRANC : un noeud ordinaire n'a rien a appliquer.
		const bool refuse = NkAppliquerActionCtx(sp, pg, a)
							&& sp.doc.AppliquerAuComposant(pg, "") == -1;
		char det[256];
		nkentseu::NkSnprintf(det, sizeof(det),
				 "meme depart=%d ; Ctrl+Alt+M traite par le dispatcher=%d ; la source porte "
				 "#00ff00=%d ; la SURCHARGEE garde #ff0000=%d ; une instance neuve SUIT=%d ; "
				 "modifier une instance n'en touche aucune autre=%d ; refus franc=%d",
				 memeDepart ? 1 : 0, traite ? 1 : 0, source ? 1 : 0, garde ? 1 : 0,
				 suit ? 1 : 0, isole ? 1 : 0, refuse ? 1 : 0);
		verdict("PROPAGATION (Q51 R1') PAR LE GESTE : `Ctrl+Alt+M` porte la couleur d'une "
				"instance au COMPOSANT, les instances non surchargees SUIVENT, celle qui a "
				"surcharge sa couleur la GARDE, et modifier une instance n'en touche aucune autre",
				memeDepart && traite && source && garde && suit && isole && refuse, det);
	}

	// ── POSER, PAR LE GESTE DE LA PALETTE ────────────────────────────────────
	// Le double-clic de la palette passe par `NkPoserComposantDocument` : c'est
	// ELLE qui choisit la cible et qui PARLE au pied. Meme exigence que le geste
	// d'extraction : la preuve par la main, pas par la fonction du modele.
	{
		static DesignState sp;
		sp.doc.NewDocument("poser", NkAuthor::Humain);
		sp.selected = -1;
		const int32 pg = sp.doc.AddChild(0, "", NkAuthor::Humain);
		sp.doc.nodes[(uint32)pg].label = NkString("Accueil");
		const int32 bt = sp.doc.AddChild(pg, "", NkAuthor::Humain);
		sp.doc.nodes[(uint32)bt].label = NkString("Bouton");
		(void)sp.doc.AddChild(bt, "", NkAuthor::Humain);
		const int32 decl = sp.doc.ExtraireComposant(bt, "", "bouton");
		// (a) AVEC selection : l'instance nait SOUS la selection, et devient la
		//     nouvelle selection (le pied nomme la cible).
		sp.SelectSingle(pg);
		const uint32 n0 = (uint32)sp.doc.nodes.Size();
		const bool sousSelection = NkPoserComposantDocument(sp, decl)
								   && (uint32)sp.doc.nodes.Size() == n0 + 2u
								   && sp.doc.IsValidIndex(sp.selected)
								   && sp.doc.nodes[(uint32)sp.selected].parent == pg
								   && !sp.doc.nodes[(uint32)sp.selected].instanceDe.Empty();
		// (b) SANS selection : repli sur la premiere page -- jamais la racine
		//     (poser « hors page » sans que la main l'ait demande).
		sp.selected = -1;
		const bool surPage = NkPoserComposantDocument(sp, decl)
							 && sp.doc.IsValidIndex(sp.selected)
							 && sp.doc.nodes[(uint32)sp.selected].parent == pg;
		// (c) SANS page : refus, ET IL SE DIT (jamais un geste sans effet muet).
		static DesignState vide;
		vide.doc.NewDocument("vide", NkAuthor::Humain);
		vide.selected = -1;
		vide.doc.declarations.PushBack(sp.doc.declarations[(uint32)decl]);
		vide.status = NkString("");
		const bool refusDit = !NkPoserComposantDocument(vide, 0) && !vide.status.Empty();
		char det[192];
		nkentseu::NkSnprintf(det, sizeof(det),
				 "sous la selection=%d ; sans selection -> premiere page=%d ; sans page : "
				 "refus qui parle=%d (« %s »)",
				 sousSelection ? 1 : 0, surPage ? 1 : 0, refusDit ? 1 : 0,
				 vide.status.Data() ? vide.status.Data() : "");
		verdict("POSER PAR LE GESTE : la palette pose sous la selection, se replie sur la "
				"premiere page sans selection, et sans page le refus SE DIT au pied",
				sousSelection && surPage && refusDit, det);
	}

	// ── LES SURCHARGES SE NOMMENT, DONC ELLES SE VOIENT ──────────────────────
	// *Une propriete surchargee doit se distinguer d'une propriete heritee, sinon
	// personne ne sait pourquoi une instance ne suit plus sa declaration.*
	//
	// 🔴 CE CAS GARDE LA TABLE DES NOMS ALIGNEE SUR L'ENUMERATION, et c'est son
	//    seul objet. Le jour ou quelqu'un ajoute un `EcartRotation` sans l'ajouter
	//    a `NkTousLesEcarts`, l'utilisateur aurait une propriete surchargee que
	//    l'interface ne sait pas NOMMER -- donc qu'il ne peut ni voir ni
	//    reinitialiser, et qui expliquerait sans raison visible pourquoi son
	//    instance ne suit plus sa declaration. Sans ce cas, la regle « ce que le
	//    modele porte, l'interface doit pouvoir le nommer » ne serait qu'un
	//    commentaire.
	{
		uint32 nbE = 0;
		const NkEcartNomme *t = NkTousLesEcarts(nbE);
		// (a) CHAQUE ENTREE PORTE UN NOM NON VIDE ET UN BIT UNIQUE. Deux entrees
		//     partageant un bit afficheraient deux lignes pour une seule
		//     surcharge, et « reinitialiser » l'une effacerait l'autre.
		bool nomsPleins = nbE > 0u, bitsUniques = true;
		uint32 union_ = 0u;
		for (uint32 i = 0; i < nbE; ++i) {
			if (!t[i].nom || !t[i].nom[0])
				nomsPleins = false;
			if ((union_ & t[i].bit) != 0u)
				bitsUniques = false;
			union_ |= t[i].bit;
		}
		// (b) ⚠️ LA TABLE COUVRE TOUTE L'ENUMERATION. On additionne les six bits
		//     connus : leur union doit valoir exactement celle de la table. Un bit
		//     ajoute a l'enum et oublie ici ferait tomber ce volet, et lui seul.
		const uint32 attendus = NkUINode::EcartRemplissages | NkUINode::EcartBordures
								| NkUINode::EcartEffets | NkUINode::EcartTexte
								| NkUINode::EcartApparence | NkUINode::EcartTaille;
		const bool couvre = union_ == attendus;
		// (c) LE MASQUE SE LIT PAR `Surcharge`, ET IL EST INDEPENDANT PAR BIT :
		//     poser « texte » ne pose pas « remplissages ».
		NkUINode n;
		n.ecarts = NkUINode::EcartTexte;
		const bool independant = n.Surcharge(NkUINode::EcartTexte)
								 && !n.Surcharge(NkUINode::EcartRemplissages) && n.ADesEcarts();
		// (d) REINITIALISER RETIRE LE BIT ET NE TOUCHE PAS AUX AUTRES -- ni a la
		//     VALEUR : la propriete redevient HERITEE, et c'est la declaration qui
		//     la fournira. Ecraser la valeur ici serait decider a la place de la
		//     propagation, dont Rodolf n'a pas encore tranche la regle.
		n.ecarts = NkUINode::EcartTexte | NkUINode::EcartBordures;
		n.text = NkString("Surcharge");
		n.ecarts &= ~NkUINode::EcartTexte;
		const bool reinit = !n.Surcharge(NkUINode::EcartTexte)
							&& n.Surcharge(NkUINode::EcartBordures)
							&& strcmp(n.text.Data(), "Surcharge") == 0;
		char det[224];
		nkentseu::NkSnprintf(det, sizeof(det), "%u ecarts nommes ; noms pleins=%d bits uniques=%d ; la table "
							   "couvre l'enumeration=%d (union %u contre %u) ; masque "
							   "independant=%d ; reinit retire le bit sans toucher la valeur=%d",
				 nbE, nomsPleins ? 1 : 0, bitsUniques ? 1 : 0, couvre ? 1 : 0, union_, attendus,
				 independant ? 1 : 0, reinit ? 1 : 0);
		verdict("LES SURCHARGES SE NOMMENT : chaque bit de l'enumeration a un nom dans la table "
				"(sinon il serait une surcharge INVISIBLE, ni voyable ni reinitialisable), les "
				"bits sont uniques et independants, et reinitialiser retire le BIT sans toucher "
				"a la VALEUR",
				nomsPleins && bitsUniques && couvre && independant && reinit, det);
	}

	// ── UNE INSTANCE PEINT LE CONTENU DE SA DECLARATION ──────────────────────
	// 🔴 CE CAS EST NE D'UNE QUESTION QUE LA PALETTE A POSEE : « poser une
	//    instance depuis la palette » n'a aucun sens si une instance NE DESSINE
	//    RIEN. Or a l'extraction, la descendance QUITTE le document (elle vit
	//    desormais dans la declaration) -- donc, sans resolution au peintre, un
	//    bouton extrait PERDRAIT SON LIBELLE a l'ecran.
	//
	// ⚠️ MESURE PAR LES COMMANDES EMISES, comme le masquage : ce qu'on veut
	//    savoir n'est pas « la declaration contient trois nœuds » (c'est deja
	//    tenu) mais « l'ECRAN montre encore la meme chose ». Le nombre de
	//    commandes du peintre avant l'extraction et apres doit etre IDENTIQUE :
	//    extraire est un geste de STRUCTURE, il ne doit rien changer a l'image.
	{
		NkUIDocument d;
		d.NewDocument("instance peinte", NkAuthor::Humain);
		const int32 pg = d.AddChild(0, "", NkAuthor::Humain);
		d.nodes[(uint32)pg].shape = NkString("frame");
		d.nodes[(uint32)pg].layout.kind = NkLayoutKind::Free;
		d.nodes[(uint32)pg].width.mode = NkSizeMode::Fixed;
		d.nodes[(uint32)pg].width.value = 400.f;
		d.nodes[(uint32)pg].height.mode = NkSizeMode::Fixed;
		d.nodes[(uint32)pg].height.value = 300.f;
		const int32 bt = d.AddChild(pg, "", NkAuthor::Humain);
		d.nodes[(uint32)bt].shape = NkString("rect");
		d.nodes[(uint32)bt].label = NkString("Bouton");
		d.nodes[(uint32)bt].posX = 20.f;
		d.nodes[(uint32)bt].posY = 20.f;
		d.nodes[(uint32)bt].width.mode = NkSizeMode::Fixed;
		d.nodes[(uint32)bt].width.value = 160.f;
		d.nodes[(uint32)bt].height.mode = NkSizeMode::Fixed;
		d.nodes[(uint32)bt].height.value = 48.f;
		// ⚠️ `Free` SUR LE BOUTON, ET CE N'EST PAS DU DECOR : un nœud ne se peint
		//    que s'il est POSE, c'est-a-dire si SON PARENT agence en libre ou en
		//    ancre. Sans cette ligne l'enfant texte n'emet AUCUNE commande, le
		//    compte vaut 2 avant comme apres, et le cas passe au vert sans rien
		//    discriminer. *Mesure du 02/09 : c'est exactement ce qui s'est
		//    produit au premier essai.* Meme famille que la cible trop large des
		//    zooms -- une donnee d'essai qui ne peut pas exprimer l'ecart rend un
		//    volet vide sans le dire.
		d.nodes[(uint32)bt].layout.kind = NkLayoutKind::Free;
		const int32 tx = d.AddChild(bt, "", NkAuthor::Humain);
		d.nodes[(uint32)tx].shape = NkString("text");
		d.nodes[(uint32)tx].text = NkString("Valider");
		d.nodes[(uint32)tx].width.mode = NkSizeMode::Fixed;
		d.nodes[(uint32)tx].width.value = 100.f;
		d.nodes[(uint32)tx].height.mode = NkSizeMode::Fixed;
		d.nodes[(uint32)tx].height.value = 20.f;
		NkLayoutResult lay;
		NkComputeLayout(d, NkPaintRect{0.f, 0.f, 400.f, 300.f}, lay);
		NkComponentInput in;
		nkentseu::editorkit::NkRecordingPaint pv;
		NkDocumentHost hote;
		hote.SyncTo(d);
		pv.Reset();
		NkDrawDocument(pv, in, d, lay, hote, 0);
		const uint32 avant = (uint32)pv.cmds.Size();
		// on extrait : la descendance quitte le document.
		const int32 decl = d.ExtraireComposant(bt, "", "bouton");
		int32 inst = -1;
		for (uint32 k = 0; k < (uint32)d.nodes.Size(); ++k)
			if (!d.nodes[k].instanceDe.Empty())
				inst = (int32)k;
		NkLayoutResult lay2;
		NkComputeLayout(d, NkPaintRect{0.f, 0.f, 400.f, 300.f}, lay2);
		hote.SyncTo(d);
		pv.Reset();
		NkDrawDocument(pv, in, d, lay2, hote, 0);
		const uint32 apres = (uint32)pv.cmds.Size();
		const bool memeImage = decl >= 0 && inst >= 0 && apres == avant;
		char det[224];
		nkentseu::NkSnprintf(det, sizeof(det), "declaration=%d instance=%d ; commandes emises %u avant -> %u "
							   "apres extraction (identiques=%d)",
				 decl, inst, avant, apres, memeImage ? 1 : 0);
		verdict("UNE INSTANCE PEINT LE CONTENU DE SA DECLARATION : extraire est un geste de "
				"STRUCTURE, donc l'ECRAN ne change pas -- le peintre emet exactement autant de "
				"commandes apres qu'avant",
				memeImage, det);
	}

	// ── UN RECTANGLE NEUF N'A PAS DE BORDURE ─────────────────────────────────
	// Rodolf : « quand je trace un rectangle il a une bordure que je n'ai pas
	// demandee ». Le nœud est PROPRE -- `CreerForme` n'ecrit ni bordure ni fond,
	// et le fichier ne porte aucune cle. Le contour venait du PEINTRE.
	//
	// 🔴 DEUX BRANCHES REPONDAIENT L'INVERSE A LA MEME QUESTION. Le repli de
	//    bordure se declenchait sur `!n.FondEffectif()` -- « le MODELE declare-t-il
	//    un fond ? » -- alors que la branche du dessus EN PEINT UN (le fond de
	//    role du theme). Le peintre peignait donc un fond, puis ajoutait un
	//    contour parce qu'il n'y en avait « pas ». `fondPeint` dit ce que le
	//    peintre A FAIT, pas ce que le modele DECLARE.
	//
	// ⚠️ ON MESURE L'OPERATION, PAS LE NOMBRE DE COMMANDES : un compte dirait
	//    « il y en a une de moins » sans dire LAQUELLE. On compte les
	//    `OutlineSharp` -- le contour lui-meme.
	{
		NkUIDocument d;
		d.NewDocument("bordure neuve", NkAuthor::Humain);
		const int32 pg = d.AddChild(0, "", NkAuthor::Humain);
		d.nodes[(uint32)pg].shape = NkString("frame");
		d.nodes[(uint32)pg].layout.kind = NkLayoutKind::Free;
		d.nodes[(uint32)pg].width.mode = NkSizeMode::Fixed;
		d.nodes[(uint32)pg].width.value = 400.f;
		d.nodes[(uint32)pg].height.mode = NkSizeMode::Fixed;
		d.nodes[(uint32)pg].height.value = 300.f;
		// un rectangle EXACTEMENT tel que le trace le pose : aucune cle de fond,
		// aucune cle de bordure.
		const int32 rc = d.AddChild(pg, "", NkAuthor::Humain);
		d.nodes[(uint32)rc].shape = NkString("rect");
		d.nodes[(uint32)rc].posX = 20.f;
		d.nodes[(uint32)rc].posY = 20.f;
		d.nodes[(uint32)rc].width.mode = NkSizeMode::Fixed;
		d.nodes[(uint32)rc].width.value = 120.f;
		d.nodes[(uint32)rc].height.mode = NkSizeMode::Fixed;
		d.nodes[(uint32)rc].height.value = 60.f;
		const bool noeudPropre = d.nodes[(uint32)rc].fill.Empty()
								 && d.nodes[(uint32)rc].fills.Empty()
								 && d.nodes[(uint32)rc].borderColor.Empty()
								 && d.nodes[(uint32)rc].borders.Empty()
								 && d.nodes[(uint32)rc].borderW == 0.f;
		NkLayoutResult lay;
		NkComputeLayout(d, NkPaintRect{0.f, 0.f, 400.f, 300.f}, lay);
		NkComponentInput in;
		nkentseu::editorkit::NkRecordingPaint pv;
		NkDocumentHost hote;
		hote.SyncTo(d);
		auto compter = [&](nkentseu::editorkit::NkPaintOp op) -> uint32 {
			uint32 k = 0;
			for (uint32 c = 0; c < (uint32)pv.cmds.Size(); ++c)
				if (pv.cmds[c].op == op)
					++k;
			return k;
		};
		pv.Reset();
		NkDrawDocument(pv, in, d, lay, hote, 0);
		const uint32 contours = compter(nkentseu::editorkit::NkPaintOp::OutlineSharp);
		const uint32 fonds = compter(nkentseu::editorkit::NkPaintOp::Fill)
							 + compter(nkentseu::editorkit::NkPaintOp::FillColor);
		// (a) AUCUN CONTOUR, et (b) UN FOND QUAND MEME. Le second volet compte
		//     autant que le premier : retirer le contour en rendant la forme
		//     INVISIBLE serait une regression, pas une correction.
		const bool sansContour = contours == 0u;
		const bool maisVisible = fonds >= 1u;
		// (c) ⚠️ LE REPLI RESTE POUR CE QUI N'A VRAIMENT RIEN. Une forme dont tous
		//     les remplissages sont MASQUES ne peint rien : le contour est alors la
		//     seule chose qui la rende reperable. Sans ce volet, on aurait pu
		//     SUPPRIMER le repli au lieu de corriger sa question -- et personne
		//     n'aurait vu la difference avant de masquer un remplissage.
		NkRemplissage f;
		f.couleur = NkString("#123456");
		f.visible = false; // masque : rien ne se peint
		d.nodes[(uint32)rc].fills.PushBack(f);
		pv.Reset();
		NkDrawDocument(pv, in, d, lay, hote, 0);
		const uint32 contoursMasque = compter(nkentseu::editorkit::NkPaintOp::OutlineSharp);
		const bool repliTenu = contoursMasque >= 1u;
		char det[240];
		nkentseu::NkSnprintf(det, sizeof(det), "noeud propre=%d ; contours=%u (attendu 0)=%d fonds=%u "
							   "(visible=%d) ; tous remplissages masques -> contours=%u (repli "
							   "tenu=%d)",
				 noeudPropre ? 1 : 0, contours, sansContour ? 1 : 0, fonds, maisVisible ? 1 : 0,
				 contoursMasque, repliTenu ? 1 : 0);
		verdict("UN RECTANGLE NEUF N'A PAS DE BORDURE : le nœud est propre, le peintre lui donne "
				"son fond de role SANS y ajouter de contour -- et le repli de contour reste pour "
				"une forme dont tous les remplissages sont masques",
				noeudPropre && sansContour && maisVisible && repliTenu, det);
	}

	printf("\nRECETTE GESTES : %d/%d %s\n", cas - echecs, cas,
		   echecs == 0 ? "PROUVEE" : "EN ECHEC");
	return echecs == 0 ? 0 : 1;
}
