#pragma once
// -----------------------------------------------------------------------------
// @File    RecetteAnnulation.h
// @Brief   La recette de l'ANNULATION — sortie de `main.cpp` le 28/09/2026.
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

static nkentseu::int32 RecetteAnnulation() {
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
	// ⚠️ L'ANCRE — LE VOLET *CONSERVATION*, SANS LEQUEL CETTE RECETTE ETAIT
	//    VERTE SUR UN ECRIVAIN MORT. Mesure du 01/09 : `NkUIDocument::Save`
	//    neutralisee (elle rend du vide) -> **RECETTE ANNULATION : 13/13
	//    PROUVEE**. Les treize cas ne comparaient que DEUX SORTIES DU MEME
	//    PRODUCTEUR : deux chaines vides sont egales, donc tout passait, et la
	//    recette declarait l'annulation prouvee sur un format qui n'ecrivait
	//    plus rien.
	//    C'est la porte du depot : « stable » ne veut pas dire « juste ». Un
	//    aller-retour porte DEUX exigences — la stabilite ET la conservation.
	//    Ici la conservation tient en une ligne : la serialisation doit DIRE
	//    quelque chose (son en-tete, et au moins un noeud).
	auto ancree = [](const NkString &s) -> bool {
		if (s.Size() == 0 || !s.Data())
			return false;
		return s.Contains("nkuidoc") && s.Contains("noeud");
	};
	// L'observateur pousse apres ~6 passages STABLES : on lui donne 10.
	auto stabiliser = [&](DesignState &s) {
		for (int32 i = 0; i < 10; ++i) {
			NkString o;
			s.doc.Save(o);
			s.histoire.Observer(o);
		}
	};
	static DesignState st; // static : l'etat est gros, pas sur la pile
	st.BuildStarterDocument();
	stabiliser(st);
	const NkString initial = ser(st);
	int32 echecs = 0, gestes = 0;
	NkString avant;
	auto avantGeste = [&] { avant = ser(st); };
	auto apresGeste = [&](const char *nom) {
		stabiliser(st);
		++gestes;
		const NkString apres = ser(st);
		// LA CONSERVATION D'ABORD : si les deux etats compares ne DISENT rien,
		// leur egalite ne prouve rien. On l'exige AVANT de la lire.
		const bool dit = ancree(avant) && ancree(apres);
		const uint32 nAvant = st.doc.NodeCount();
		st.Annuler();
		const bool okU = dit && identiques(ser(st), avant);
		st.Retablir();
		const bool okR = dit && identiques(ser(st), apres) && st.doc.NodeCount() == nAvant;
		printf("%s  %s (annuler %s avant, retablir %s apres%s)\n",
			   (okU && okR) ? "OK   " : "ECHEC", nom, okU ? "==" : "!=", okR ? "==" : "!=",
			   dit ? "" : ", SERIALISATION MUETTE");
		if (!okU || !okR)
			++echecs;
	};

	// 1. TRACE : poser une forme (le geste de l'outil rectangle).
	avantGeste();
	int32 forme = st.doc.AddChild(0, "", NkAuthor::Humain);
	{
		NkUINode &n = st.doc.nodes[(uint32)forme];
		n.label = NkString("Forme recette");
		n.shape = NkString("rect");
		n.posX = 10.f;
		n.posY = 12.f;
		n.width.mode = NkSizeMode::Fixed;
		n.width.value = 120.f;
		n.height.mode = NkSizeMode::Fixed;
		n.height.value = 60.f;
		st.doc.MarkHumanEdit(forme);
	}
	apresGeste("trace d'une forme");
	// 2. DEPLACEMENT (le drag entier = un pas : ici sa fin).
	avantGeste();
	st.doc.nodes[(uint32)forme].posX += 25.f;
	st.doc.MarkHumanEdit(forme);
	apresGeste("deplacement");
	// 3. REDIMENSION.
	avantGeste();
	st.doc.nodes[(uint32)forme].width.value += 40.f;
	st.doc.MarkHumanEdit(forme);
	apresGeste("redimension");
	// 4. RENOMMAGE (l'etiquette = la cle `label`).
	avantGeste();
	st.doc.nodes[(uint32)forme].label = NkString("Renommee");
	st.doc.MarkHumanEdit(forme);
	apresGeste("renommage");
	// 5. EDITION DE TEXTE (la cle `texte`).
	avantGeste();
	st.doc.nodes[(uint32)forme].shape = NkString("text");
	st.doc.nodes[(uint32)forme].text = NkString("Bonjour recette");
	st.doc.MarkHumanEdit(forme);
	apresGeste("edition de texte");
	// 6. PROPRIETE D'INSPECTEUR (apparence : fond hexa).
	avantGeste();
	st.doc.nodes[(uint32)forme].fill = NkString("#12ab34");
	st.doc.MarkHumanEdit(forme);
	apresGeste("propriete d'inspecteur (fond)");
	// 7. ROLE (le geste « promouvoir »).
	avantGeste();
	st.doc.nodes[(uint32)forme].role = NkString("bouton");
	st.doc.MarkHumanEdit(forme);
	apresGeste("role");
	// 8. METRIQUE D'ESPACEMENT (section ESPACEMENT de l'Inspecteur).
	avantGeste();
	st.doc.SetMetric("gouttiere_recette", 14.f);
	st.doc.MarkHumanEdit(0);
	apresGeste("metrique d'espacement");
	// 9. VERSION MOBILE (la transposition — un sous-arbre entier en un pas).
	avantGeste();
	{
		NkVector<NkString> constats;
		const int32 m = st.doc.TransposerVersMobile(forme, constats);
		if (m >= 0)
			st.doc.MarkHumanEdit(m);
	}
	apresGeste("version mobile (transposition)");
	// 10. SUPPRESSION d'un sous-arbre (renumerotation comprise — l'instantane
	//     restaure exactement, indices et tout).
	avantGeste();
	st.doc.RemoveSubtree(forme, nullptr);
	apresGeste("suppression d'un sous-arbre");

	// 11. LES ONGLETS (multi-documents, 01/09) : L'ANNULATION NE TRAVERSE PAS.
	//     Geste sur A, bascule vers B, Annuler -> B ne bouge pas ; retour vers
	//     A -> il est reste modifie, et SON annulation defait SON geste.
	{
		st.ouverts.Clear();
		{
			DesignState::NkDocOuvert s0;
			st.ouverts.PushBack(s0);
			st.ongletActif = 0;
		}
		NkUIDocument db;
		NkBuildBlankDocument(db);
		db.title = NkString("Recette B");
		const int32 iB = st.OuvrirOngletInactif(db, "");
		const NkString aAvant = ser(st);
		st.doc.SetMetric("onglet_recette", 3.f);
		st.doc.MarkHumanEdit(0);
		stabiliser(st);
		++gestes;
		const NkString aApres = ser(st);
		st.BasculerVers((uint32)iB);
		const NkString bAvant = ser(st);
		st.Annuler(); // il n'y a RIEN a annuler dans B — et surtout pas le geste de A
		const bool okB = identiques(ser(st), bAvant);
		st.BasculerVers(0);
		const bool okA1 = identiques(ser(st), aApres); // A est revenu MODIFIE
		st.Annuler();
		const bool okA2 = identiques(ser(st), aAvant); // et SON Ctrl+Z defait SON geste
		printf("%s  onglets : l'annulation ne traverse pas (B %s, A garde %s, A annule %s)\n",
			   (okB && okA1 && okA2) ? "OK   " : "ECHEC", okB ? "intact" : "TOUCHE",
			   okA1 ? "==" : "!=", okA2 ? "==" : "!=");
		if (!okB || !okA1 || !okA2)
			++echecs;
	}

	// 12. LE MULTILINGUE (01/09) : declarer une langue + poser une traduction
	//     est UN geste annulable, et l'aller-retour par la serialisation prouve
	//     que les cles additives (`langues`, `texte_<langue>`) SURVIVENT a
	//     Save -> Load -> Save octet pour octet (l'annulation restaure par Load).
	avantGeste();
	{
		st.doc.langues.PushBack(NkString("en"));
		const int32 tx = st.doc.AddChild(0, "", NkAuthor::Humain);
		if (st.doc.IsValidIndex(tx)) {
			NkUINode &n = st.doc.nodes[(uint32)tx];
			n.label = NkString("Texte multilingue");
			n.shape = NkString("text");
			n.text = NkString("Bonjour");
			n.PoserTexte("en", "Hello");
			st.doc.MarkHumanEdit(tx);
		}
	}
	apresGeste("multilingue (langue declaree + traduction)");

	// LA CONTRE-EPREUVE : N gestes, N Annuler -> l'etat initial EXACT.
	for (int32 i = 0; i < gestes; ++i)
		st.Annuler();
	const bool okInitial = ancree(initial) && identiques(ser(st), initial);
	printf("%s  %d gestes puis %d Annuler -> etat initial octet pour octet (et cet etat "
		   "DIT quelque chose)\n",
		   okInitial ? "OK   " : "ECHEC", gestes, gestes);
	if (!okInitial)
		++echecs;
	// Et le retour : N Retablir -> le dernier etat.
	printf("RECETTE ANNULATION : %d/%d %s\n", gestes + 1 - echecs, gestes + 1,
		   echecs == 0 ? "PROUVEE" : "EN ECHEC");
	return echecs == 0 ? 0 : 1;
}
