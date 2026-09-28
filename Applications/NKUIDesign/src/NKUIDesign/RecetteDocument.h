#pragma once
// -----------------------------------------------------------------------------
// @File    RecetteDocument.h
// @Brief   La recette « Document » — sortie de `main.cpp` le 28/09/2026.
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

static nkentseu::int32 RecetteDocument() {
	using namespace nkuidesign;
	using nkentseu::float32;
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

	// ── LE DOCUMENT DE RODOLF D'ABORD, SON JUMEAU VERSIONNE ENSUITE ────────
	// Lance depuis la racine de l'arbre, le premier chemin EST le fichier que
	// son application ouvre. Lance ailleurs, le second garde la recette
	// reproductible -- c'est le meme document a quelques valeurs pres (il en
	// derive).
	static const char *const kCandidats[2] = {
		"nkuidesign_document.nkuidoc",
		"Applications/NKUIDesign/design/mises_en_scene/demo_ecran_01.nkuidoc"};
	const char *chemin = nullptr;
	for (uint32 i = 0; i < 2 && !chemin; ++i)
		if (nkentseu::NkFile::Exists(kCandidats[i]))
			chemin = kCandidats[i];
	if (!chemin) {
		printf("ECHEC  0. le document a mesurer est INTROUVABLE  -- essaye : \"%s\" puis "
			   "\"%s\" (lancer depuis la racine de l'arbre)\n",
			   kCandidats[0], kCandidats[1]);
		printf("\nRECETTE DOCUMENT : 0/1 EN ECHEC\n");
		return 1;
	}
	NkUIDocument doc;
	{
		const NkString texte = nkentseu::NkFile::ReadAllText(chemin);
		const bool lu = !texte.Empty() && doc.Load(texte.Data());
		char d[320];
		nkentseu::NkSnprintf(d, sizeof(d), "fichier=%s  titre=%s  noeuds=%u", chemin, doc.title.Data(),
				 (uint32)doc.nodes.Size());
		verdict("1. LE DOCUMENT REEL SE LIT (et la recette DIT lequel elle a lu)",
				lu && doc.nodes.Size() > 1, d);
		if (!lu) {
			printf("\nRECETTE DOCUMENT : %d/%d EN ECHEC\n", cas - echecs, cas);
			return 1;
		}
	}
	NkLayoutResult lay;
	NkComputeLayout(doc, NkPaintRect{0.f, 0.f, 1600.f, 1000.f}, lay);

	// LE DOUBLE-CLIC, TEL QUE LA TOILE L'EXECUTE — mForage compris.
	// ⚠️ LA BOUCLE EST LA MOITIE DE LA MESURE. Un double-clic sur une forme
	//    posee DANS un artboard ne l'atteint pas du premier coup : il FORE.
	//    Compter les coups est donc la seule facon de repondre a « double-
	//    cliquer sur un rectangle ne permet pas d'avoir ca » autrement que par
	//    oui/non -- la reponse peut etre « si, mais au troisieme coup ».
	// LE POINT OU L'ON PEUT VRAIMENT VISER CE NOEUD -- son CORPS LIBRE.
	// ⚠️ MESURER AU CENTRE ETAIT UNE ERREUR, ET ELLE A RENDU UN CHIFFRE FAUX.
	//    Le centre d'un bouton est occupe par son libelle, le centre d'une carte
	//    par sa valeur, le centre d'un champ par son texte d'aide : viser le
	//    centre, c'est viser l'ENFANT ou le FRERE du dessus, jamais le
	//    rectangle. Et le pointage a raison de rendre celui du dessus -- c'est le
	//    contrat de Lunacy, et c'est la regle qu'on a POSEE au 4e retour de
	//    Rodolf. *La question n'est donc pas « le centre ouvre-t-il le mode »
	//    mais « existe-t-il un endroit ou l'utilisateur peut atteindre cette
	//    forme », et c'est un balayage, pas un point.*
	// Rend faux si AUCUN point du rectangle ne designe ce noeud (entierement
	// recouvert) -- un cas qui doit se DIRE, pas se confondre avec un refus.
	auto corpsLibre = [&](int32 noeud, float32 &ox, float32 &oy) -> bool {
		if (!lay.Has(noeud))
			return false;
		const NkPaintRect r = lay.At(noeud);
		for (uint32 gy = 0; gy < 7; ++gy)
			for (uint32 gx = 0; gx < 7; ++gx) {
				const float32 px = r.x + r.w * ((float32)gx + 0.5f) / 7.f;
				const float32 py = r.y + r.h * ((float32)gy + 0.5f) / 7.f;
				if (NkPickSelectable(doc, lay, px, py) == noeud) {
					ox = px;
					oy = py;
					return true;
				}
			}
		return false;
	};

	// Rend le nombre de double-clics jusqu'a la suite `cible`, ou -1.
	auto coupsJusquA = [&](int32 noeud, NkSuiteDblClic cible, int32 &atteint) -> int32 {
		atteint = -1;
		float32 cx = 0.f, cy = 0.f;
		if (!corpsLibre(noeud, cx, cy))
			return -2; // entierement recouvert : ce n'est pas un refus du mecanisme
		int32 forage = -1;
		for (int32 coup = 1; coup <= 6; ++coup) {
			// ⚠️ LE MEME ORDRE QUE LA TOILE, `forageAvant` COMPRIS. C'est tout
			//    l'objet de cette recette : emprunter la porte du geste, pas une
			//    porte voisine qui lui ressemble.
			const int32 forageAvant = forage;
			int32 cand = NkPickDansContexte(doc, lay, cx, cy, forage);
			if (cand == -2) {
				forage = -1;
				cand = NkPickTopLevel(doc, lay, cx, cy);
			}
			if (cand < 0)
				return -1;
			const NkUINode &cn = doc.nodes[(uint32)cand];
			const NkIssueDblClic issue = NkIssueDeDblClic(cn);
			const int32 enfant = (issue == NkIssueDblClic::Forer)
									 ? NkPickDansContexte(doc, lay, cx, cy, cand)
									 : -1;
			const NkSuiteDblClic suite = NkSuiteDeDblClic(issue, enfant >= 0,
														  NkFormeEditable(cn),
														  forageAvant == cand);
			atteint = (suite == NkSuiteDblClic::ForerVersEnfant) ? enfant : cand;
			if (suite == cible)
				return coup;
			if (suite == NkSuiteDblClic::ForerVersEnfant
				|| suite == NkSuiteDblClic::ForerSansEnfant)
				forage = cand;
			else
				return -1; // une suite terminale qui n'est pas la cible : ca n'ira pas plus loin
		}
		return -1;
	};

	// ── 2. LES RECTANGLES DU DOCUMENT OUVRENT-ILS LEURS SOMMETS ? ─────────
	//     Retour (1) de Rodolf, 01/09 : « les rectangles presents, quand je
	//     double-clique dessus, ne font pas apparaitre ces elements de
	//     modification de vertices ; mais quand j'en cree un nouveau ca
	//     apparait. » L'ecart est ICI, et nulle part dans une forme fabriquee.
	//
	// ⚠️ LE CAS PORTE SUR **TOUS** LES RECTANGLES, PAS SUR LES « SIMPLES ». La
	//    premiere version de ce cas ecartait les rects a enfants (« un rect a
	//    enfants FORE, c'est un autre cas ») : elle rendait 12/14 et **passait a
	//    cote du defaut**, parce que les rectangles que Rodolf VOIT sont
	//    justement ceux qui portent des enfants -- le bouton « Se connecter »,
	//    `Panel_Nav`, les cartes. Les 12 qui ouvraient etaient les BARRES DU
	//    GRAPHIQUE, celles qu'on ne double-clique jamais. *Un banc qui ecarte le
	//    cas difficile mesure la partie facile et rend un vert qui ment.*
	{
		uint32 nbRect = 0, ouvrent = 0, recouverts = 0;
		char premierRate[256];
		char premierRecouvert[224];
		premierRate[0] = 0;
		premierRecouvert[0] = 0;
		for (uint32 i = 1; i < (uint32)doc.nodes.Size(); ++i) {
			const NkUINode &n = doc.nodes[i];
			if (!NkComponentDecl::StrEq(n.shape.Data(), "rect") || !n.text.Empty())
				continue; // un rect QUI PORTE du texte s'edite : c'est le cas 3
			++nbRect;
			int32 atteint = -1;
			const int32 coups = coupsJusquA((int32)i, NkSuiteDblClic::ModePoints, atteint);
			if (coups > 0)
				++ouvrent;
			else if (coups == -2) {
				// ENTIEREMENT RECOUVERT : aucun point de ce rectangle ne le
				// designe. Ce n'est pas un refus du mecanisme, c'est une
				// consequence du contrat « le plus haut gagne » -- et ca se DIT.
				++recouverts;
				if (!premierRecouvert[0])
					nkentseu::NkSnprintf(premierRecouvert, sizeof(premierRecouvert),
							 " ; recouvert : \"%s\" (n%u) n'a aucun pixel a lui", n.label.Data(),
							 i);
			} else if (!premierRate[0])
				nkentseu::NkSnprintf(premierRate, sizeof(premierRate),
						 " ; 1er RATE = \"%s\" (n%u) -> on reste sur \"%s\" (n%d)",
						 n.label.Data(), i,
						 (atteint >= 0) ? doc.nodes[(uint32)atteint].label.Data() : "rien",
						 atteint);
		}
		char d[512];
		nkentseu::NkSnprintf(d, sizeof(d), "%u rect(s), %u ouvrent le mode, %u entierement recouvert(s)%s%s",
				 nbRect, ouvrent, recouverts, premierRecouvert, premierRate);
		verdict("2. CHAQUE RECTANGLE DU DOCUMENT REEL ouvre le mode edition de forme depuis son "
				"CORPS LIBRE (retour 1 de Rodolf)",
				nbRect > 0 && ouvrent + recouverts == nbRect, d);
	}

	// ── 5. LE RECTANGLE QUI PORTE DES ENFANTS -- L'IMPASSE, NOMMEE ────────
	//     C'est le cas que la mesure a trouve, et il merite son propre verdict :
	//     un rect a enfants rendait `Forer` a chaque coup, le forage etait arme,
	//     le double-clic suivant ressortait au premier niveau (le contexte rend
	//     -2) et refaisait exactement la meme chose. Le geste tournait en rond,
	//     indefiniment, sur le rectangle le plus visible de son ecran.
	// ⚠️ ET LE CAS EXIGE **DEUX** CHOSES, PAS UNE : que le mode s'ouvre, et qu'il
	//    ne s'ouvre PAS du premier coup. Le premier double-clic doit rester
	//    « j'entre dans le groupe » -- c'est le geste de Lunacy et il sert (une
	//    fois dedans, les clics simples designent le contenu). Un cas qui
	//    n'exigerait que « ca finit par s'ouvrir » laisserait passer la version
	//    qui ouvre du premier coup et supprime le forage.
	{
		uint32 nbConteneurs = 0, ouvrent = 0, auPremierCoup = 0;
		char premier[224];
		premier[0] = 0;
		for (uint32 i = 1; i < (uint32)doc.nodes.Size(); ++i) {
			const NkUINode &n = doc.nodes[i];
			if (!NkComponentDecl::StrEq(n.shape.Data(), "rect") || n.children.Size() == 0)
				continue;
			++nbConteneurs;
			int32 atteint = -1;
			const int32 coups = coupsJusquA((int32)i, NkSuiteDblClic::ModePoints, atteint);
			if (coups == 1)
				++auPremierCoup;
			if (coups > 0) {
				++ouvrent;
				if (!premier[0])
					nkentseu::NkSnprintf(premier, sizeof(premier), " ; ex. \"%s\" (n%u) en %d coup(s)",
							 n.label.Data(), i, coups);
			}
		}
		char d[320];
		nkentseu::NkSnprintf(d, sizeof(d),
				 "%u rect(s) a enfants, %u ouvrent le mode, %u des le 1er coup (doit rester 0)%s",
				 nbConteneurs, ouvrent, auPremierCoup, premier);
		verdict("5. UN RECTANGLE QUI PORTE DES ENFANTS finit par ouvrir SA forme -- et jamais "
				"au premier coup (le 1er double-clic ENTRE, comme dans Lunacy)",
				nbConteneurs > 0 && ouvrent == nbConteneurs && auPremierCoup == 0, d);
	}

	// ── 3. UN TEXTE N'A PAS DE SOMMETS, ET IL N'EN A NULLE PART ───────────
	//     Retour (2) de Rodolf, 01/09 : « ca doit etre sur les formes dessinees
	//     autres que le texte ». Un texte s'edite par sa saisie, jamais par son
	//     contour.
	// ⚠️ DEUX CHOSES SE MESURENT ICI, PAS UNE : que le double-clic ouvre bien la
	//    SAISIE, et que la table des sommets refuse le texte a la source
	//    (`NkNatureDe` -> `Aucun`, `NkSommetsDe` -> 0). La premiere sans la
	//    seconde laisserait un chemin lateral (le raccourci, un futur bouton)
	//    poser des poignees sur un mot.
	{
		uint32 nbTexte = 0, editent = 0, sansSommets = 0;
		for (uint32 i = 1; i < (uint32)doc.nodes.Size(); ++i) {
			const NkUINode &n = doc.nodes[i];
			if (!NkComponentDecl::StrEq(n.shape.Data(), "text"))
				continue;
			++nbTexte;
			int32 atteint = -1;
			if (coupsJusquA((int32)i, NkSuiteDblClic::EditerTexte, atteint) > 0)
				++editent;
			float32 xy[64];
			const NkPaintRect r = lay.Has((int32)i) ? lay.At((int32)i) : NkPaintRect{};
			if (NkNatureDe(n.shape.Data()) == NkNatureSommets::Aucun
				&& NkSommetsDe(n, r, xy, 32) == 0)
				++sansSommets;
		}
		char d[192];
		nkentseu::NkSnprintf(d, sizeof(d), "%u texte(s) : %u ouvrent la saisie, %u sans aucun sommet",
				 nbTexte, editent, sansSommets);
		verdict("3. UN TEXTE OUVRE SA SAISIE ET N'A AUCUN SOMMET, dans le document reel "
				"(retour 2 de Rodolf)",
				nbTexte > 0 && editent == nbTexte && sansSommets == nbTexte, d);
	}

	// ── 4. CONSERVATION : MESURER N'ECRIT RIEN ───────────────────────────
	//     Volet obligatoire de toute garde de ce chantier : lire un document,
	//     le disposer et simuler des double-clics dessus ne doit pas modifier
	//     un octet. Sans ce volet, la recette pourrait « reussir » en
	//     materialisant les sommets de chaque rect au passage.
	{
		NkString avant, apres;
		doc.Save(avant);
		NkLayoutResult l2;
		NkComputeLayout(doc, NkPaintRect{0.f, 0.f, 900.f, 700.f}, l2);
		for (uint32 i = 1; i < (uint32)doc.nodes.Size(); ++i)
			(void)NkIssueDeDblClic(doc.nodes[i]);
		doc.Save(apres);
		const bool stable = avant.Size() == apres.Size()
							&& NkComponentDecl::StrEq(avant.Data(), apres.Data());
		char d[128];
		nkentseu::NkSnprintf(d, sizeof(d), "%u octets %s", (uint32)avant.Size(),
				 stable ? "octet pour octet" : "ONT BOUGE");
		verdict("4. CONSERVATION : lire, disposer et interroger le document n'ecrit RIEN",
				stable && avant.Size() > 0, d);
	}

	// ── 6. LE PANNEAU DROIT CHANGE AVEC LA TOILE ─────────────────────────
	//     Comparaison en deux temps de Rodolf, 01/09 : temps 1
	//     (`lunacy_2temps_selection_181741.png`) le panneau habituel ; temps 2
	//     (`lunacy_2temps_edition_181745.png`) une section `EDIT SHAPE` REMPLACE
	//     la geometrie, et LAYER / FILLS / BORDERS / EFFECTS / PROTOTYPING
	//     restent en dessous.
	//
	// ⚠️ CE CAS EXISTE PARCE QUE CE TROISIEME CHANGEMENT AVAIT ETE RATE, et il
	//    n'a ete vu que quand Rodolf a envoye la PAIRE d'images. La consigne
	//    ecrite disait « les poignees de boite disparaissent » et ne parlait pas
	//    du panneau. *Une question sur du visuel se pose avec une capture* -- et
	//    ce qui a ete rate une fois se tient desormais au banc, pas a l'oeil.
	//
	// ⚠️ ET IL EXIGE LES DEUX SENS. « ÉDITION DE FORME est la » ne prouve rien si
	//    DISPOSITION est restee a cote : ce serait un AJOUT, et l'ecran aurait
	//    deux X et deux Y sans rien qui dise lequel parle du sommet. Le cas
	//    verifie donc aussi ce qui doit AVOIR DISPARU, et ce qui doit RESTER.
	{
		const char *const *forme = nullptr;
		const uint32 nf = NkSectionsInspecteur(true, false, forme);
		const char *const *normal = nullptr;
		const uint32 nn = NkSectionsInspecteur(false, false, normal);
		auto contient = [](const char *const *l, uint32 n, const char *quoi) {
			for (uint32 i = 0; i < n; ++i)
				if (NkComponentDecl::StrEq(l[i], quoi))
					return true;
			return false;
		};
		// (a) la section neuve est PREMIERE -- une section d'edition qu'il
		//     faudrait aller chercher au bas du panneau n'est pas trouvee
		const bool premiere = nf > 0 && NkComponentDecl::StrEq(forme[0], "ÉDITION DE FORME");
		// (b) la GEOMETRIE a disparu (remplacement, pas ajout)
		const bool remplace = !contient(forme, nf, "DISPOSITION")
							  && !contient(forme, nf, "ALIGNEMENT")
							  && !contient(forme, nf, "ESPACEMENT")
							  && !contient(forme, nf, "ANCRAGE");
		// (c) ce qui decrit l'OBJET reste -- chez Lunacy comme chez nous
		const bool restent = contient(forme, nf, "REMPLISSAGES")
							 && contient(forme, nf, "BORDURES")
							 && contient(forme, nf, "APPARENCE")
							 && contient(forme, nf, "EFFETS");
		// (d) TEMOIN : hors du mode, la geometrie est bien la et la section
		//     d'edition ABSENTE. Sans ce temoin, une fonction qui rendrait
		//     toujours la liste du mode forme passerait les trois premiers.
		const bool temoin = contient(normal, nn, "DISPOSITION")
							&& !contient(normal, nn, "ÉDITION DE FORME");
		char d[192];
		nkentseu::NkSnprintf(d, sizeof(d), "mode forme : %u sections (1re = %s) ; normal : %u sections",
				 nf, nf > 0 ? forme[0] : "?", nn);
		verdict("6. LE PANNEAU DROIT CHANGE AVEC LA TOILE : « ÉDITION DE FORME » PREMIERE, la "
				"geometrie REMPLACEE (pas doublee), l'apparence conservee, et le temoin hors "
				"mode",
				premiere && remplace && restent && temoin, d);
	}

	// ── 7. SUR SON DOCUMENT : LA BOITE SUIT LE TRACE, ET LE POINTAGE SUIT ──
	//     Retour de Rodolf, 01/09 (soir) : « on doit redefinir sa bounding box
	//     pour la selection ». Le mot qui compte est « pour la selection » : ce
	//     n'est pas le lisere qui le gene, c'est que le clic rate.
	//
	// ⚠️ CE CAS MESURE LE POINTAGE, PAS LA BOITE. Verifier que `width` a la bonne
	//    valeur serait verifier que j'ai su ecrire une soustraction. Ce qu'il
	//    faut prouver, c'est qu'apres avoir tire un sommet HORS de la boite
	//    d'origine, un clic la ou la forme se VOIT maintenant l'attrape -- et
	//    qu'un clic la ou elle n'est PLUS ne l'attrape plus. Les deux sens, sur
	//    son document, avec le vrai `NkComputeLayout` et le vrai `NkPickNode`.
	{
		uint32 nbEssais = 0, pointageOk = 0;
		char premierRate[224];
		premierRate[0] = 0;
		for (uint32 i = 1; i < (uint32)doc.nodes.Size() && nbEssais < 6; ++i) {
			NkUINode &n = doc.nodes[i];
			if (!NkComponentDecl::StrEq(n.shape.Data(), "rect") || n.children.Size() > 0
				|| !n.text.Empty())
				continue;
			const int32 pa = n.parent;
			if (!doc.IsValidIndex(pa) || doc.nodes[(uint32)pa].layout.kind != NkLayoutKind::Free)
				continue;
			++nbEssais;
			// on TIRE le coin haut-gauche loin en dehors, comme la main le ferait
			NkMaterialiserSommets(n);
			n.sommets[0].x = -3.f;
			n.sommets[0].y = -3.f;
			NkRecadrerNoeud(n, true);
			NkLayoutResult l3;
			NkComputeLayout(doc, NkPaintRect{0.f, 0.f, 1600.f, 1000.f}, l3);
			if (!l3.Has((int32)i))
				continue;
			const NkPaintRect r = l3.At((int32)i);
			// 🔴 LE POINT D'ESSAI VIENT DU TRACE, PAS DE LA BOITE, ET LA MUTATION
			//    A DU ME LE DIRE. Ma premiere version visait `r.x + 3` : un point
			//    a trois pixels du bord de la boite est DANS la boite par
			//    construction, quelle qu'elle soit. Le cas mesurait donc la boite
			//    contre elle-meme -- et sous la mutation « le recadrage ne fait
			//    rien », il restait VERT. *Un banc qui prend ses deux mesures du
			//    meme cote ne mesure rien.*
			//    On vise donc le SOMMET TIRE, la ou la forme SE VOIT maintenant :
			//    sans recadrage il tombe loin hors de la boite perimee, et le
			//    pointage le rate -- ce que Rodolf decrit.
			float32 anc[64];
			const uint32 nbA = NkSommetsDe(n, r, anc, 32);
			if (nbA == 0)
				continue;
			// le sommet 0 est celui qu'on a tire ; on vise un peu EN DEDANS de
			// lui, vers le centre, pour ne pas jouer sur le pixel du bord.
			const float32 cxT = r.x + r.w * 0.5f, cyT = r.y + r.h * 0.5f;
			const float32 vx = anc[0] + (cxT - anc[0]) * 0.06f;
			const float32 vy = anc[1] + (cyT - anc[1]) * 0.06f;
			const bool dedans = NkPickSelectable(doc, l3, vx, vy) == (int32)i;
			// (b) ET LA BOITE N'EST PAS DEVENUE LA TOILE ENTIERE : un clic bien
			//     au-dela du trace ne l'attrape pas (sinon « recadrer » pourrait
			//     se contenter d'agrandir sans borne, et le cas (a) passerait
			//     pour une mauvaise raison).
			const bool dehors =
				NkPickSelectable(doc, l3, anc[0] - 40.f, anc[1] - 40.f) != (int32)i;
			if (dedans && dehors)
				++pointageOk;
			else if (!premierRate[0])
				nkentseu::NkSnprintf(premierRate, sizeof(premierRate),
						 " ; 1er rate = \"%s\" (n%u) dedans=%d dehors=%d", n.label.Data(), i,
						 dedans ? 1 : 0, dehors ? 1 : 0);
		}
		char d[320];
		nkentseu::NkSnprintf(d, sizeof(d), "%u forme(s) deformee(s), %u attrapables la ou elles se voient%s",
				 nbEssais, pointageOk, premierRate);
		verdict("7. SUR SON DOCUMENT : apres avoir tire un sommet HORS de la boite, le POINTAGE "
				"suit la forme (on l'attrape ou elle est, pas ou elle etait)",
				nbEssais > 0 && pointageOk == nbEssais, d);
	}

	// ── 8. LES SIX FORMES QUI REFUSAIENT HIER : L'ARRONDI MARCHE ENCORE ────
	//     Retour (3) de Rodolf : il redemande le double-clic qui arrondit. Il
	//     avait ete livre AVANT le correctif du blocage et celui du forage a deux
	//     temps -- donc avant deux changements qui touchent le meme geste.
	//
	// ⚠️ ON RE-MESURE SUR LES FORMES QUI REFUSAIENT, PAS SUR UNE FORME NEUVE.
	//    C'est la lecon de ce matin : les six rectangles a enfants (« Se
	//    connecter », `Panel_Nav`, les quatre cartes) sont exactement ceux qu'un
	//    banc a noeuds fabriques n'exerce jamais. Si le double-clic sur sommet
	//    devait casser quelque part, c'est la.
	{
		uint32 nbC = 0, arrondissent = 0, contourSuit = 0;
		char premierRate[224];
		premierRate[0] = 0;
		for (uint32 i = 1; i < (uint32)doc.nodes.Size(); ++i) {
			NkUINode &n = doc.nodes[i];
			if (!NkComponentDecl::StrEq(n.shape.Data(), "rect") || n.children.Size() == 0)
				continue;
			++nbC;
			// le mode est arme sur CE noeud : le double-clic lui appartient
			const bool aMoi = NkDblClicAuModeForme((int32)i, (int32)i, (int32)i);
			const float32 r1 = NkArrondirSommet(n, 0);
			const float32 r2 = NkArrondirSommet(n, 0);
			const float32 r3 = NkArrondirSommet(n, 0);
			const float32 r4 = NkArrondirSommet(n, 0);
			const bool cycle = r1 == 8.f && r2 == 16.f && r3 == 32.f && r4 == 0.f;
			if (aMoi && cycle)
				++arrondissent;
			else if (!premierRate[0])
				nkentseu::NkSnprintf(premierRate, sizeof(premierRate),
						 " ; 1er rate = \"%s\" (n%u) au mode=%d cycle %.0f/%.0f/%.0f/%.0f",
						 n.label.Data(), i, aMoi ? 1 : 0, (double)r1, (double)r2, (double)r3,
						 (double)r4);
			// ⚠️ ET LE CONTOUR PEINT DOIT SUIVRE, pas seulement le modele : un
			//    rayon enregistre que le dessin n'honore pas est « un champ qui
			//    n'agit pas », la famille de defauts qu'on chasse. C'est la
			//    mutation qui avait mordu en Q44.
			(void)NkArrondirSommet(n, 0); // -> 8 px
			const NkPaintRect rr = lay.Has((int32)i) ? lay.At((int32)i) : NkPaintRect{};
			float32 ct[256], an[64];
			const uint32 nbAn = NkSommetsDe(n, rr, an, 32);
			const uint32 nbCt = NkContourDe(n, rr, ct, 128);
			if (nbCt > nbAn)
				++contourSuit;
			(void)NkArrondirSommet(n, 0); // on repart du cycle, sans laisser de trace
			(void)NkArrondirSommet(n, 0);
			(void)NkArrondirSommet(n, 0);
		}
		char d[320];
		nkentseu::NkSnprintf(d, sizeof(d), "%u rect(s) a enfants : %u arrondissent (cycle 0/8/16/32), %u "
							   "dont le CONTOUR PEINT suit%s",
				 nbC, arrondissent, contourSuit, premierRate);
		verdict("8. LES FORMES QUI REFUSAIENT HIER : le double-clic appartient au mode, "
				"l'arrondi cycle encore, et le contour PEINT le montre (retour 3 de Rodolf)",
				nbC > 0 && arrondissent == nbC && contourSuit == nbC, d);
	}

	printf("\nRECETTE DOCUMENT : %d/%d %s\n", cas - echecs, cas,
		   echecs == 0 ? "PROUVEE" : "EN ECHEC");
	return echecs == 0 ? 0 : 1;
}
