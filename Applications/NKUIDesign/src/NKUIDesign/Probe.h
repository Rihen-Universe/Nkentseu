#pragma once
// -----------------------------------------------------------------------------
// @File    Probe.h
// @Brief   LE TEMOIN DE LA TRANCHE, sans fenetre et sans GPU.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  CE QU'IL PROUVE — une seule chose, et il faut la lire avec sa portee
// =============================================================================
//  « Changer un parametre dans NkUIDesign change le rendu SANS RECOMPILER. »
//
//  Sous sa forme mesurable en seance sans GPU : *la declaration modifiee produit
//  des commandes de dessin differentes*, et *un fichier texte suffit a la
//  modifier*. Le patron est celui de la sonde de glisser-deposer du 17/08
//  (11/11) : entrees posees a la main, sortie lue, deux sens.
//
//  ⚠️ CE QU'IL NE PROUVE PAS, et qui est DIFFERE ET NOMME :
//     - que le rendu est CONFORME AUX PLANCHES — ca se voit a l'ecran ;
//     - que le texte s'ellipse correctement — les metriques de texte du peintre
//       enregistreur sont fictives et le disent (`NkRecordingPaint.h`) ;
//     - que l'interaction a la souris est agreable — un banc pose des positions,
//       il ne juge pas un geste.
//     Aucun temoin visuel n'a ete pris et aucun n'est revendique.
//
// =============================================================================
//  LES TROIS CONTROLES QUI RENDENT LES AUTRES LISIBLES
// =============================================================================
//  Sans eux, un banc qui ne sait dire que « oui » ne mesure rien.
//
//  1. **LE TEMOIN DE BRUIT** (essai 1) : la meme mesure repetee SANS RIEN
//     CHANGER. Le peintre enregistreur est entierement deterministe — aucune
//     horloge, aucun aleatoire —, donc le plancher attendu est EXACTEMENT zero.
//     On le VERIFIE au lieu de le supposer : sans ce chiffre, aucun ecart plus
//     bas ne voudrait dire quoi que ce soit.
//  2. **LE CONTROLE POSITIF** (essai 2) : un ecrasement connu DOIT produire une
//     difference. Il prouve que la sonde sait voir un changement — sans lui, un
//     « aucune difference » ne se distinguerait pas d'une sonde aveugle.
//  3. **LE CONTROLE NEGATIF** (essai 8) : un ecrasement sur une cle que la
//     declaration ne connait pas NE DOIT RIEN changer, et doit se COMPTER comme
//     inconnue. Il prouve que la difference vue en 2 vient bien de la
//     declaration, et pas du simple fait d'avoir touche a l'instance.
//
//  REGIME COUVERT, ecrit avec le resultat : un seul jeu de donnees (12 entrees,
//  fil d'Ariane a 3 niveaux, filtre vide), une seule taille de panneau, echelle
//  1.0, variantes `grid` et `dense_list`. **Non couverts** : liste vide, filtre
//  actif, echelle != 1, panneau plus etroit qu'une carte, variante `columns`
//  (declaree, rendue comme `dense_list` — cf. `NkContentBrowserDraw.cpp`).
// -----------------------------------------------------------------------------

#include "NKEditorKit/Components/NkComponentInstance.h"
#include "NKEditorKit/Components/NkContentBrowserModel.h"
#include "NKEditorKit/Components/NkRecordingPaint.h"
#include "NKEditorKit/Components/NkTreeViewModel.h"
#include "NKFileSystem/NkFile.h"

#include "Selection.h"
#include "Canvas.h"
#include "Backend.h"
#include "DesignAI.h"
#include "Icons.h"
#include "Renderers.h"

#include <cstdio>

namespace nkuidesign {

	using namespace nkentseu;
	using namespace nkentseu::editorkit;

	// ── Le compteur d'evenements : le second bout de la verification ─────────
	// La declaration dit qu'un composant emet `onDoubleClick`. Rien dans le
	// compilateur ne verifie qu'il l'emet reellement — c'est le meme angle mort
	// que « le point de verite d'un encodage est son consommateur ». On branche
	// donc les crochets et on compte les departs.
	struct ProbeEvents {
			int32 selects = 0, doubleClicks = 0, contextMenus = 0, drops = 0, navigates = 0;
			int32 lastIndex = -1;
			bool pathWasEmpty = false;

			static void OnSelect(void *u, int32 i, const char *p) {
				auto *e = (ProbeEvents *)u;
				++e->selects;
				e->lastIndex = i;
				if (!p || !*p)
					e->pathWasEmpty = true;
			}
			static void OnDouble(void *u, int32 i, const char *p) {
				auto *e = (ProbeEvents *)u;
				++e->doubleClicks;
				e->lastIndex = i;
				if (!p || !*p)
					e->pathWasEmpty = true;
			}
			static void OnMenu(void *u, int32 i, float32, float32) {
				auto *e = (ProbeEvents *)u;
				++e->contextMenus;
				e->lastIndex = i;
			}
			static void OnDrop(void *u, int32, const char *) {
				((ProbeEvents *)u)->drops++;
			}
			static void OnNav(void *u, const char *) {
				((ProbeEvents *)u)->navigates++;
			}
	};

	/// L'ARBRE de demonstration DE LA SONDE — sa propre copie, comme
	/// `FillDemoModel` : l'application charge desormais le sien depuis une
	/// RESSOURCE (cadrage 31/08), et un cote de la mesure doit venir
	/// d'ailleurs que du code teste.
	inline void ProbeFillDemoTree(nkentseu::editorkit::NkTreeViewModel &t) {
		struct Row {
				nkentseu::int32 parent;
				const char *label;
				const char *kind;
		};
		static const Row kRows[] = {
			{-1, "Scene", "racine"},	  {0, "Environnement", "groupe"},
			{1, "Soleil", "lumiere"},	  {1, "Ciel", "lumiere"},
			{0, "Decor", "groupe"},		  {4, "Sol", "maillage"},
			{4, "Rocher", "maillage"},	  {4, "Caisse", "maillage"},
			{0, "Personnages", "groupe"}, {8, "Heros", "maillage"},
			{9, "Squelette", "os"},		  {8, "Garde", "maillage"},
		};
		t.nodes.Clear();
		for (nkentseu::uint32 i = 0; i < sizeof(kRows) / sizeof(kRows[0]); ++i) {
			nkentseu::editorkit::NkTreeNode n;
			n.id = (nkentseu::nk_uint64)(i + 1);
			n.parent = kRows[i].parent;
			n.label = NkString(kRows[i].label);
			n.path = NkString("/scene/");
			n.path.Append(kRows[i].label);
			n.kindLabel = kRows[i].kind;
			n.kindRole = NkDocumentHost::RoleOfKind(kRows[i].kind);
			t.nodes.PushBack(n);
		}
		t.active = 6;
		t.chosen.PushBack(6);
	}

	inline void FillDemoModel(NkContentBrowserModel &m) {
		struct Row {
				const char *name;
				const char *kind;
				bool folder;
		};
		static const Row kRows[] = {
			{"Materiaux", "dossier", true},	  {"Maillages", "dossier", true},
			{"Textures", "dossier", true},	  {"caisse.nkmesh", "maillage", false},
			{"sol.nkmat", "materiau", false}, {"bois_albedo.nktex", "texture", false},
			{"metal.nkmat", "materiau", false}, {"perso.nkmesh", "maillage", false},
			{"ciel.nktex", "texture", false}, {"herbe.nkmat", "materiau", false},
			{"rocher.nkmesh", "maillage", false}, {"eau.nkmat", "materiau", false},
		};
		m.entries.Clear();
		for (uint32 i = 0; i < sizeof(kRows) / sizeof(kRows[0]); ++i) {
			NkAssetEntry e;
			e.name = NkString(kRows[i].name);
			// ⚠️ UN CHEMIN NON VIDE, ET C'EST VOLONTAIRE : `path` est la charge que
			//    les evenements portent vers un blueprint. Un banc qui le laisserait
			//    vide validerait une charge inutilisable sans s'en apercevoir.
			e.path = NkString("/projet/");
			e.path.Append(kRows[i].name);
			e.isFolder = kRows[i].folder;
			e.kindLabel = kRows[i].kind;
			e.kindRole = (uint16)(4 + (i % 5));
			m.entries.PushBack(e);
		}
		m.breadcrumb.Clear();
		m.breadcrumb.PushBack(NkString("projet"));
		m.breadcrumb.PushBack(NkString("assets"));
		m.breadcrumb.PushBack(NkString("niveau1"));

		// Les NATURES du mixte (puces de filtre) — roles arbitraires mais
		// DISTINCTS, meme regle que DemoStyle : le peintre enregistreur est
		// injectif par role, une erreur de role se verrait.
		m.kinds.Clear();
		static const char *kKinds[] = {"Maillage", "Materiau", "Texture"};
		for (uint32 k = 0; k < sizeof(kKinds) / sizeof(kKinds[0]); ++k) {
			nkentseu::editorkit::NkBrowserKind kind;
			kind.label = NkString(kKinds[k]);
			kind.role = (uint16)(4 + k);
			m.kinds.PushBack(kind);
		}

		// L'arbre de dossiers embarque (tree_view) — trois noeuds en ordre
		// prefixe, assez pour que la colonne emette des lignes.
		m.folders.nodes.Clear();
		static const struct {
				int32 parent;
				const char *label;
		} kFolders[] = {{-1, "projet"}, {0, "assets"}, {1, "niveau1"}};
		for (uint32 k = 0; k < sizeof(kFolders) / sizeof(kFolders[0]); ++k) {
			nkentseu::editorkit::NkTreeNode n;
			n.id = (nkentseu::nk_uint64)(k + 1);
			n.parent = kFolders[k].parent;
			n.label = NkString(kFolders[k].label);
			n.path = NkString("/");
			n.path.Append(kFolders[k].label);
			m.folders.nodes.PushBack(n);
		}

		m.statusRight = NkString("Sauvegarde");
	}

	inline NkContentBrowserStyle DemoStyle(const NkComponentInstance *inst) {
		NkContentBrowserStyle s;
		// Roles arbitraires mais DISTINCTS : le peintre enregistreur rend une
		// couleur injective par role, donc une erreur de role se verrait dans le
		// flux. Deux roles egaux la rendraient invisible.
		s.panelBg = 1;
		s.headerBg = 2;
		s.border = 3;
		s.text = 4;
		s.textMuted = 5;
		s.cardBg = 6;
		s.cardFooterBg = 7;
		s.activeMark = 8;
		s.chosenMark = 9;
		s.folderTint = 10;
		s.chipBg = 11;
		s.badgeText = 12;
		s.statusBg = 13;
		s.variant = NkBrowserVariant::Grid;
		s.values = inst;
		return s;
	}

	/// Une passe de dessin complete, deterministe.
	inline void Render(NkRecordingPaint &rec, const NkComponentInstance *inst,
					   const NkComponentInput &in, NkContentBrowserHooks *hooks = nullptr) {
		rec.Reset();
		NkContentBrowserModel m;
		FillDemoModel(m);
		m.active = 3;
		m.chosen.PushBack(3);
		const NkContentBrowserStyle s = DemoStyle(inst);
		NkContentBrowserHooks h;
		NkDrawContentBrowser(rec, in, {0.f, 0.f, 900.f, 600.f}, m, s, hooks ? *hooks : h);
	}

	// ═══════════════════════════════════════════════════════════════════════════
	//  OUTILLAGE DES ESSAIS D'APPLICATION (17 et suivants)
	// ═══════════════════════════════════════════════════════════════════════════

	/// Deux textes sont-ils IDENTIQUES, octet pour octet ? Sert aux cas negatifs
	/// de l'IA : « le document n'a pas bouge » verifie par le nombre de noeuds
	/// passerait a cote d'une provenance salie ou d'un reglage ecrase.
	inline bool SameText(const char *a, const char *b) {
		if (!a || !b)
			return a == b;
		for (; *a && *b; ++a, ++b)
			if (*a != *b)
				return false;
		return *a == *b;
	}

	/// Recherche de sous-chaine, sans `<cstring>` (zero-STL).
	inline bool Contains(const char *hay, const char *needle) {
		if (!hay || !needle || !*needle)
			return false;
		for (; *hay; ++hay) {
			const char *a = hay, *b = needle;
			while (*a && *b && *a == *b) {
				++a;
				++b;
			}
			if (!*b)
				return true;
		}
		return false;
	}

	// ── LE RESOLVEUR DE LA SONDE A ETE SUPPRIME, ET C'EST LE CORRECTIF ─────
	// Il vivait ici :
	//
	//     inline uint16 ProbeResolveRole(const char *name) {
	//         uint32 h = 2166136261u;                    // FNV-1a
	//         for (const char *p = name; p && *p; ++p)
	//             h = (h ^ (uint32)(uint8)*p) * 16777619u;
	//         return (uint16)(1u + (h % 250u));          // JAMAIS NK_ROLE_INVALID
	//     }
	//
	// ⚠️ UN RESOLVEUR QUI DIT OUI A TOUT NE PEUT PAS VOIR UN NOM FAUX. C'est le
	//    defaut le plus cher du 18/08, et il est d'une autre nature que ceux que
	//    la sonde attrape d'habitude : ce n'est pas un essai qui manquait, c'est
	//    l'INSTRUMENT qui rendait tous les essais aveugles a une classe entiere
	//    d'erreurs. 21/21 puis 68/68 puis 72/72, tous verts, pendant que
	//    l'application s'ouvrait en MAGENTA PLEIN ECRAN.
	//
	// Sa justification etait l'injectivite -- « les essais de dessin doivent
	// comparer des couleurs distinctes ». Elle ne tenait pas : le resolveur REEL
	// est injectif sur les 30 roles du coeur, et la ou deux jetons partagent un
	// role (`card_bg` et `card_footer_bg` heritent tous deux d'`input_bg`), les
	// peindre de la meme couleur est la VERITE de l'ecran, pas un defaut de
	// l'instrument. Un banc qui exige des couleurs distinctes la ou l'ecran n'en
	// a pas mesure une reconstruction, pas la chose.
	//
	// La sonde passe donc par `NkDesignResolveRole` -- exactement la fonction de
	// l'editeur fenetre, et par defaut de `NkDocumentHost`. Il n'y a plus qu'une
	// resolution dans le programme, et c'est la seule facon qu'un essai headless
	// dise quelque chose sur ce que l'ecran montrera.

	// ── UNE SECONDE DECLARATION, DEFINIE ICI ────────────────────────────────
	// Q61 §8.3 : *« une forme validee sur un seul cas n'est pas validee »*. Le
	// second composant reel (l'arbre) arrive en parallele ; en attendant, cette
	// declaration d'essai repond a une question PLUS ETROITE mais reelle :
	//
	//   ⚠️ CE QU'ELLE PROUVE : que l'APPLICATION (palette, arbre, agencement,
	//      sauvegarde, catalogue d'IA) ne connait pas `content_browser` — poser un
	//      composant qu'elle n'a jamais vu marche sans qu'une ligne la nomme.
	//   ⚠️ CE QU'ELLE NE PROUVE PAS : que la FORME de declaration convient a un
	//      second composant reel. Elle est ecrite par celui qui teste, donc elle
	//      rentre dans la forme par construction. Seul l'arbre, ecrit par
	//      quelqu'un d'autre, repondra a ca.
	inline const NkComponentDecl &ProbeSecondDecl() {
		static const NkParamDecl kParams[] = {
			{"titre_h", "Hauteur du titre", NkParamKind::Float, 24.f, 8.f, 64.f, nullptr, 0},
		};
		static const NkMetricDecl kMetrics[] = {
			{"marge", 6.f, "marge interne"},
		};
		static const NkVariantDecl kVariants[] = {
			{"simple", "Simple", "une seule colonne"},
		};
		static const NkComponentDecl d = [] {
			NkComponentDecl x;
			x.name = "essai_panneau";
			x.title = "Panneau d'essai";
			x.summary = "declaration definie dans la sonde : elle n'existe que pour verifier que "
						"l'application ne connait aucun nom de composant";
			x.params = kParams;
			x.paramCount = 1;
			x.metrics = kMetrics;
			x.metricCount = 1;
			x.variants = kVariants;
			x.variantCount = 1;
			return x;
		}();
		return d;
	}

	// ── LE DETECTEUR DE COORDONNEE ──────────────────────────────────────────
	// Il lit un document enregistre et cherche une cle de POSITION. C'est le
	// temoin mecanique de la regle de Rodolf — « l'outil n'enregistre jamais une
	// coordonnee » — et il vaut mieux qu'une relecture humaine, parce qu'il tourne
	// a chaque passage.
	//
	// ⚠️ IL SE COMPARE PAR SON PREMIER JETON DE LIGNE, pas par sous-chaine : un
	//    `strstr("x")` attraperait `max`, `ecart`, et le mot « extensible ». Un
	//    detecteur qui crie sur tout ne se lit plus au bout de deux jours.
	inline bool ProbeFindsCoordinate(const char *text, char *outKey, uint32 keyCap) {
		static const char *kForbidden[] = {"x", "y", "position", "pos", "left", "top", "rect", "abs"};
		if (outKey && keyCap)
			outKey[0] = 0;
		const char *p = text;
		while (p && *p) {
			while (*p == ' ' || *p == '\t')
				++p;
			char tok[32];
			uint32 n = 0;
			while (*p && *p != ' ' && *p != '\t' && *p != '=' && *p != '\n' && *p != '\r' && n < 31)
				tok[n++] = *p++;
			tok[n] = 0;
			for (uint32 f = 0; f < sizeof(kForbidden) / sizeof(kForbidden[0]); ++f)
				if (NkComponentDecl::StrEq(tok, kForbidden[f])) {
					if (outKey && keyCap) {
						uint32 c = 0;
						for (; tok[c] && c + 1 < keyCap; ++c)
							outKey[c] = tok[c];
						outKey[c] = 0;
					}
					return true;
				}
			while (*p && *p != '\n')
				++p;
			if (*p)
				++p;
		}
		return false;
	}

	/// Le dessin complet d'un document, enregistre. C'est ce qui porte le coeur du
	/// temoin a l'echelle du document : *ecrit -> texte -> relu -> MEME dessin*.
	inline void RenderDocument(NkRecordingPaint &rec, const NkUIDocument &doc,
							   const NkPaintRect &surface) {
		rec.Reset();
		NkLayoutResult lay;
		NkComputeLayout(doc, surface, lay);
		NkDocumentHost host; // sa resolution PAR DEFAUT est celle de l'application
		host.SyncTo(doc);
		const NkComponentInput idle;
		NkDrawDocument(rec, idle, doc, lay, host);
	}

	/// Un document d'essai : une racine en colonne, un entete FIXE, un corps
	/// EXTENSIBLE qui contient un composant. Il porte les deux modes de taille et
	/// une imbrication a deux niveaux — le minimum pour que les essais
	/// d'agencement discriminent quelque chose.
	inline void BuildProbeDocument(NkUIDocument &doc) {
		doc.NewDocument("Essai", NkAuthor::Humain);
		doc.nodes[0].layout.kind = NkLayoutKind::Column;
		// Metriques du document a zero : un essai d'agencement doit mesurer la
		// repartition, pas les gouttieres. Elles ont leur propre essai.
		doc.SetMetric("espacement", 0.f);
		doc.SetMetric("marge", 0.f);

		const int32 header = doc.AddChild(0, "", NkAuthor::Humain);
		doc.nodes[(uint32)header].label = NkString("Entete");
		doc.nodes[(uint32)header].height.mode = NkSizeMode::Fixed;
		doc.nodes[(uint32)header].height.value = 60.f;

		const int32 body = doc.AddChild(0, "", NkAuthor::Humain);
		doc.nodes[(uint32)body].label = NkString("Corps");
		doc.nodes[(uint32)body].layout.kind = NkLayoutKind::Row;
		doc.AddChild(body, "content_browser", NkAuthor::Humain);
	}

	/// La reponse d'IA de reference — un document VALIDE. Elle n'est pas « ce
	/// qu'un modele produirait » : c'est ce que l'outil doit savoir accepter.
	inline const char *ProbeValidReply() {
		return "Voici la proposition demandee :\n"
			   "nkuidoc 1\n"
			   "titre = Propose par la machine\n"
			   "noeud 0\n"
			   "  libelle = Bloc\n"
			   "  composant = \n"
			   "  enfants = 1\n"
			   "  largeur = extensible 0 0 0\n"
			   "  hauteur = fixe 220 0 0\n"
			   "  agencement = ligne\n"
			   "  ecart = 4\n"
			   "  marge = 4\n"
			   "noeud 1\n"
			   "  libelle = Navigateur\n"
			   "  composant = content_browser\n"
			   "  enfants =\n"
			   "  largeur = extensible 0 0 0\n"
			   "  hauteur = extensible 0 0 0\n"
			   "  reglage param thumb_size = 120\n";
	}

	// ── LA SONDE ────────────────────────────────────────────────────────────
	inline int RunProbe() {
		NkString rep;
		int pass = 0, total = 0;
		auto check = [&](const char *label, bool ok, const char *detail) {
			++total;
			if (ok)
				++pass;
			rep.Append(ok ? "  [ok]   " : "  [ECHEC] ");
			rep.Append(label);
			if (detail && *detail) {
				rep.Append("  -- ");
				rep.Append(detail);
			}
			rep.Append('\n');
		};
		char buf[256];

		// ⚠️ LE RAPPORT PORTE L'IDENTITE DU BINAIRE QUI L'A ECRIT, et ce n'est
		//    pas de la decoration : le 19/08 au matin, un rapport annoncant
		//    « 78/78 » a fait croire que le binaire livre etait perime. Il ne
		//    l'etait pas -- le binaire rendait 103/103. **C'etait le FICHIER de
		//    rapport qui datait de la veille** : la sonde ecrit dans le repertoire
		//    COURANT, et toutes les courses suivantes avaient ete lancees depuis la
		//    racine du depot, laissant intacte la copie posee a cote de
		//    l'executable.
		//
		//    Un rapport sans date ressemble EXACTEMENT a un rapport frais. Avec
		//    cette ligne, un fichier perime se denonce tout seul. C'est la lecon
		//    « une mesure prise dans un etat qu'on n'a pas verifie » appliquee a
		//    l'etat le plus banal qui soit : la fraicheur du fichier qu'on lit.
		rep.Append("=== NKUIDesign --probe : la declaration est-elle LUE ? ===\n");
		rep.Append("binaire compile le " __DATE__ " a " __TIME__ "\n");
		rep.Append("Seance SANS GPU : aucune fenetre ouverte, aucun temoin visuel.\n");
		rep.Append("Regime couvert : 12 entrees, panneau 900x600, echelle 1.0,\n");
		rep.Append("variantes grid + dense_list. Hors regime : liste vide, filtre\n");
		rep.Append("actif, echelle != 1, variante columns.\n\n");

		const NkComponentInput idle;

		// ── 1. TEMOIN DE BRUIT ──────────────────────────────────────────────
		NkRecordingPaint a1, a2;
		Render(a1, nullptr, idle);
		Render(a2, nullptr, idle);
		snprintf(buf, sizeof(buf), "%u commandes, %u differences", (uint32)a1.cmds.Size(),
				 a1.DiffCount(a2));
		check("1. TEMOIN DE BRUIT : deux passes identiques -> 0 difference", a1.DiffCount(a2) == 0,
			  buf);
		check("1b. la passe produit REELLEMENT quelque chose (sinon 0=0 ne prouve rien)",
			  a1.cmds.Size() > 20, buf);

		// ── 2. CONTROLE POSITIF : une metrique ecrasee ──────────────────────
		NkComponentInstance inst(NkContentBrowserDecl());
		inst.SetMetric("card_gap", 40.f);
		NkRecordingPaint b;
		Render(b, &inst, idle);
		snprintf(buf, sizeof(buf), "card_gap 12 -> 40 : %u commandes differentes de la reference",
				 a1.DiffCount(b));
		check("2. CONTROLE POSITIF : card_gap ecrase -> le dessin change", a1.DiffCount(b) > 0, buf);

		// ── 3. UN PARAMETRE ─────────────────────────────────────────────────
		NkComponentInstance p(NkContentBrowserDecl());
		p.SetParam("thumb_size", 160.f);
		NkRecordingPaint c;
		Render(c, &p, idle);
		snprintf(buf, sizeof(buf), "thumb_size 96 -> 160 : %u differences", a1.DiffCount(c));
		check("3. un PARAMETRE ecrase change le dessin", a1.DiffCount(c) > 0, buf);

		// ── 4. UNE VARIANTE ─────────────────────────────────────────────────
		NkComponentInstance v(NkContentBrowserDecl());
		v.SetVariantByName("dense_list");
		NkRecordingPaint d;
		Render(d, &v, idle);
		snprintf(buf, sizeof(buf), "grid -> dense_list : %u differences", a1.DiffCount(d));
		check("4. une VARIANTE change la mise en page (un modele, N rendus)", a1.DiffCount(d) > 0,
			  buf);

		// ── 4b. LA VARIANTE `minimal` EST L'ANCIEN RENDU, ET ELLE DESSINE ───
		// Ajoutee avec le mixte du 30/08 : `grid` est devenu le mixte, l'ancien
		// dessin survit sous `minimal`. Un chemin declare que rien n'exerce est
		// un chemin qui casse en silence — condition d'existence d'abord (des
		// commandes sortent), puis discrimination (il differe du mixte).
		NkComponentInstance vmin(NkContentBrowserDecl());
		vmin.SetVariantByName("minimal");
		NkRecordingPaint dmin;
		Render(dmin, &vmin, idle);
		snprintf(buf, sizeof(buf), "%u commandes, %u differences avec le mixte",
				 (uint32)dmin.cmds.Size(), a1.DiffCount(dmin));
		check("4b. la variante `minimal` (l'ancien rendu) dessine, et differe du mixte",
			  dmin.cmds.Size() > 20 && a1.DiffCount(dmin) > 0, buf);

		// ── 5. UNE INSTANCE VIERGE N'IMPOSE RIEN ────────────────────────────
		// Le defaut evite : `Variant()` rendait 0 quand rien n'etait pose, ce qui
		// forcait `grid` a toute application branchant une instance. Un defaut
		// muet — la vue s'affichait, simplement pas la bonne.
		NkComponentInstance pristine(NkContentBrowserDecl());
		NkRecordingPaint e;
		Render(e, &pristine, idle);
		snprintf(buf, sizeof(buf), "%u differences (attendu : 0)", a1.DiffCount(e));
		check("5. une instance VIERGE se comporte comme la declaration", a1.DiffCount(e) == 0, buf);

		// ── 6. L'ALLER-RETOUR PAR LE FICHIER — le coeur du temoin ───────────
		// C'est CE controle qui dit « sans recompiler » : le dessin suit un
		// fichier TEXTE, pas un litteral C++.
		NkString text;
		inst.Save(text);
		NkComponentInstance reloaded(NkContentBrowserDecl());
		uint32 unknown = 0, applied = 0;
		const bool loaded = reloaded.Load(text.Data(), &unknown, &applied);
		NkRecordingPaint f;
		Render(f, &reloaded, idle);
		snprintf(buf, sizeof(buf), "entete=%d applique=%u inconnu=%u, %u differences avec l'ecrit",
				 loaded ? 1 : 0, applied, unknown, b.DiffCount(f));
		check("6. ALLER-RETOUR FICHIER : ecrit -> texte -> relu -> MEME dessin",
			  loaded && unknown == 0 && applied > 0 && b.DiffCount(f) == 0, buf);
		snprintf(buf, sizeof(buf), "%u differences avec la reference (doit rester > 0)",
				 a1.DiffCount(f));
		check("6b. et le dessin relu differe TOUJOURS de la reference", a1.DiffCount(f) > 0, buf);

		// ── 7. UN JETON REAFFECTE ───────────────────────────────────────────
		NkComponentInstance t(NkContentBrowserDecl());
		t.SetTokenRole("card_bg", "PanelHeader");
		check("7. un JETON se reaffecte a un autre role, et l'instance le retient",
			  t.IsTokenOverridden("card_bg"), t.TokenRole("card_bg"));

		// ── 8. CONTROLE NEGATIF ─────────────────────────────────────────────
		NkComponentInstance bogus(NkContentBrowserDecl());
		bogus.SetMetric("cle_qui_nexiste_pas", 999.f);
		NkRecordingPaint g;
		Render(g, &bogus, idle);
		snprintf(buf, sizeof(buf), "%u ecrasements retenus, %u differences (attendu : 0 et 0)",
				 bogus.OverrideCount(), a1.DiffCount(g));
		check("8. CONTROLE NEGATIF : une cle inconnue de la declaration ne change RIEN",
			  bogus.OverrideCount() == 0 && a1.DiffCount(g) == 0, buf);

		NkComponentInstance perime(NkContentBrowserDecl());
		uint32 u2 = 0, a2c = 0;
		perime.Load("nkuicomp 1\nmetrique disparue = 3\nparam thumb_size = 120\n", &u2, &a2c);
		snprintf(buf, sizeof(buf), "inconnu=%u applique=%u", u2, a2c);
		check("8b. un fichier a moitie perime se charge quand meme, et COMPTE l'inconnu",
			  u2 == 1 && a2c == 1, buf);

		// ── 9. LES BORNES VIENNENT DE LA DECLARATION ────────────────────────
		NkComponentInstance clamp(NkContentBrowserDecl());
		clamp.SetParam("thumb_size", 9999.f);
		snprintf(buf, sizeof(buf), "9999 borne a %.1f (max declare : 256)", clamp.Param("thumb_size"));
		check("9. une valeur hors bornes est ramenee par la DECLARATION, pas par l'editeur",
			  clamp.Param("thumb_size") <= 256.f, buf);

		// ── 10. LES EVENEMENTS PARTENT VRAIMENT ─────────────────────────────
		ProbeEvents ev;
		NkContentBrowserHooks h;
		h.user = &ev;
		h.onSelect = &ProbeEvents::OnSelect;
		h.onDoubleClick = &ProbeEvents::OnDouble;
		h.onContextMenu = &ProbeEvents::OnMenu;
		h.onDrop = &ProbeEvents::OnDrop;
		h.onNavigate = &ProbeEvents::OnNav;

		// ⚠️ LE POINT DE CLIC SE CALCULE DEPUIS LA DECLARATION, il ne s'ecrit pas.
		//    Premiere version : (60, 300) en dur. Elle tombait DANS LA COLONNE
		//    D'ARBRE (largeur = 900 x `tree_width` 0.18 = 162 px), donc hors de la
		//    zone des cartes : aucun evenement ne partait, et les essais 10 et 11
		//    echouaient sur une sonde mal visee, pas sur un defaut du composant.
		//
		//    Le pire n'etait pas l'echec — c'etait l'essai 10b, qui PASSAIT :
		//    « la charge `path` n'est pas vide » etait vrai parce qu'aucune charge
		//    n'avait ete produite. **Un succes a vide**, la face n.2 de la grille
		//    du corpus (reussir pour la mauvaise raison). Il porte desormais sa
		//    propre condition d'existence.
		const NkComponentDecl &dcl = NkContentBrowserDecl();
		const float32 treeW = 900.f * dcl.Param("tree_width");
		const float32 thumb0 = dcl.Param("thumb_size");
		// ⚠️ MIS A JOUR AVEC LE MIXTE (2026-08-30) : le fil d'Ariane a rejoint la
		//    barre d'outils, et deux rangees se sont inserees au-dessus de la
		//    grille — les puces de filtre (`filter_h`) et la rangee d'information
		//    (`info_h`). Le point de clic se calcule toujours DEPUIS LA
		//    DECLARATION, jamais en dur — la lecon d'origine de cette famille.
		const float32 top0 = dcl.Metric("header_h") + dcl.Metric("toolbar_h") +
							 dcl.Metric("filter_h") + dcl.Metric("info_h");
		NkComponentInput click;
		click.mouseX = treeW + 1.f + thumb0 * 0.5f;
		click.mouseY = top0 + thumb0 * 0.5f;
		click.mousePressed = true;
		NkRecordingPaint r10;
		Render(r10, nullptr, click, &h);
		snprintf(buf, sizeof(buf), "onSelect=%d index=%d", ev.selects, ev.lastIndex);
		check("10. onSelect part au clic, avec un index valide", ev.selects == 1 && ev.lastIndex >= 0,
			  buf);
		// ⚠️ `ev.selects > 0` FAIT PARTIE DE LA CONDITION, et c'est tout l'objet de
		//    la correction ci-dessus : sans ce terme, l'essai passait quand rien
		//    ne partait. Une assertion sur une charge doit d'abord exiger qu'une
		//    charge existe.
		check("10b. la CHARGE `path` n'est pas vide (un blueprint doit pouvoir s'en servir)",
			  ev.selects > 0 && !ev.pathWasEmpty, ev.selects > 0 ? "" : "aucun evenement : vacuite");

		ProbeEvents ev2;
		h.user = &ev2;
		NkComponentInput dbl = click;
		dbl.mousePressed = false;
		dbl.doubleClick = true;
		NkRecordingPaint r11;
		Render(r11, nullptr, dbl, &h);
		snprintf(buf, sizeof(buf), "onDoubleClick=%d onSelect=%d", ev2.doubleClicks, ev2.selects);
		check("11. onDoubleClick part au double-clic, et onSelect NE part PAS",
			  ev2.doubleClicks == 1 && ev2.selects == 0, buf);

		// CONTROLE NEGATIF DES EVENEMENTS : sans entree, rien ne part. Sans lui,
		// des crochets appeles a chaque image passeraient pour un succes.
		ProbeEvents ev3;
		h.user = &ev3;
		NkRecordingPaint r12;
		Render(r12, nullptr, idle, &h);
		snprintf(buf, sizeof(buf), "select=%d double=%d menu=%d nav=%d", ev3.selects,
				 ev3.doubleClicks, ev3.contextMenus, ev3.navigates);
		check("12. CONTROLE NEGATIF : sans entree, AUCUN evenement ne part",
			  ev3.selects == 0 && ev3.doubleClicks == 0 && ev3.contextMenus == 0 &&
				  ev3.navigates == 0,
			  buf);

		// ── 13. DECOUPE EQUILIBREE ──────────────────────────────────────────
		snprintf(buf, sizeof(buf), "profondeur max %u", a1.MaxClipDepth());
		check("13. la pile de decoupe est equilibree (PushClip == PopClip)", a1.ClipBalanced(), buf);

		// ── 14. LA CONVERGENCE `.nkgui` EST PRODUITE, PAS AFFIRMEE ──────────
		const NkComponentDecl &decl = NkContentBrowserDecl();
		char ctrl[2048];
		const uint32 n = NkWriteControllerBlock(decl, ctrl, sizeof(ctrl));
		snprintf(buf, sizeof(buf), "%u evenements declares, bloc de %u octets", decl.eventCount, n);
		// 8 depuis le mixte du 30/08 : les 5 d'origine + onCreate / onImport /
		// onSaveAll (les boutons de tete, a charge vide — le fait suffit).
		check("14. le bloc `controller` de la spec .nkgui v0.2 s'emet depuis la declaration",
			  n > 0 && decl.eventCount == 8, buf);

		// Toutes les charges sont-elles dans le vocabulaire de la spec ?
		bool typesOk = true;
		for (uint16 i = 0; i < decl.eventCount; ++i)
			for (uint8 k = 0; k < decl.events[i].argCount; ++k)
				if (decl.events[i].args[k].kind == NkArgKind::Void ||
					decl.events[i].args[k].kind >= NkArgKind::Count)
					typesOk = false;
		check("14b. toutes les charges declarees ont un type de la spec (aucun `Void` en argument)",
			  typesOk, "");

		// ── 15. LE REGISTRE ENUMERE ─────────────────────────────────────────
		NkComponentRegistry::Register(decl);
		NkComponentRegistry::Register(decl); // idempotence
		snprintf(buf, sizeof(buf), "%u composant(s) enregistre(s)", NkComponentRegistry::Count());
		check("15. le registre enumere, et l'enregistrement est idempotent",
			  NkComponentRegistry::Count() == 1 &&
				  NkComponentRegistry::Find("content_browser") == &decl,
			  buf);

		// ── 16. L'ECHELLE VIENT DE LA SURFACE, PAS DU PEINTRE ───────────────
		// Arbitrage du 18/08. Ce qu'on peut mesurer ici : la valeur portee par
		// l'entree traverse bien jusqu'aux metriques.
		//
		// ⚠️ CE QUE CE CONTROLE NE PROUVE PAS, et il faut le dire avec lui : il
		//    est SEQUENTIEL. Une variable globale de processus le passerait
		//    exactement pareil — on la poserait a 1, puis a 2. **Le seul temoin
		//    qui discrimine est simultane** : deux fenetres a DPI differents
		//    ouvertes EN MEME TEMPS, produisant des metriques differentes au meme
		//    instant. Il exige un GPU et deux fenetres : il est DIFFERE, et rien
		//    ici ne le remplace.
		NkComponentInput hidpi;
		hidpi.surfaceScale = 2.f;
		NkRecordingPaint s2r;
		Render(s2r, nullptr, hidpi);
		snprintf(buf, sizeof(buf), "echelle 1.0 -> 2.0 : %u differences", a1.DiffCount(s2r));
		check("16. l'echelle de SURFACE traverse jusqu'aux metriques (temoin simultane DIFFERE)",
			  a1.DiffCount(s2r) > 0, buf);

		// ══════════════════════════════════════════════════════════════════
		//  L'APPLICATION — palette, composition, agencement, document, IA
		// ══════════════════════════════════════════════════════════════════
		rep.Append("\n--- l'application : palette, composition, agencement, document, IA ---\n");

		// ── 17. LA PALETTE BOUCLE SUR LE REGISTRE ───────────────────────────
		// La question exacte : peut-on poser CHAQUE composant declare, sans que
		// l'application en nomme un seul ? Condition d'existence d'abord — un
		// registre vide ferait passer la boucle sans rien poser.
		//
		// ⚠️ L'ARBRE EST INSCRIT ICI, ET PAS SEULEMENT POUR GROSSIR LE COMPTE :
		//    c'est le SECOND composant reel, ecrit par quelqu'un d'autre. La
		//    declaration d'essai (`ProbeSecondDecl`) ne prouvait que l'ignorance
		//    des noms par l'application -- elle est ecrite par celui qui teste,
		//    donc elle rentre dans la forme par construction. L'arbre, lui,
		//    repond a la question posee en Q61 §8.3 : la forme tient-elle sur un
		//    composant qu'on n'a pas ecrit ? Et il apporte 13 jetons de plus a la
		//    famille 33, qui etaient tous en PascalCase.
		NkComponentRegistry::Register(nkentseu::editorkit::NkTreeViewDecl());
		NkComponentRegistry::Register(ProbeSecondDecl());
		const uint16 regCount = NkComponentRegistry::Count();
		check("17. le registre contient au moins DEUX declarations (sinon 18 ne prouve rien)",
			  regCount >= 2, "");

		NkUIDocument pal;
		pal.NewDocument("Palette", NkAuthor::Humain);
		uint32 posed = 0;
		for (uint16 i = 0; i < regCount; ++i) {
			const NkComponentDecl *d = NkComponentRegistry::At(i);
			if (d && pal.AddChild(0, d->name, NkAuthor::Humain) >= 0)
				++posed;
		}
		snprintf(buf, sizeof(buf), "%u declare(s), %u pose(s)", regCount, posed);
		check("17b. chaque composant DECLARE se pose, sans qu'un seul nom soit ecrit dans la palette",
			  regCount > 0 && posed == regCount, buf);

		const int32 bogusNode = pal.AddChild(0, "composant_qui_nexiste_pas", NkAuthor::Humain);
		snprintf(buf, sizeof(buf), "retour %d, %u noeud(s) au lieu de %u", bogusNode, pal.NodeCount(),
				 posed + 1);
		check("17c. CONTROLE NEGATIF : un nom absent du registre est REFUSE, et rien n'est ajoute",
			  bogusNode < 0 && pal.NodeCount() == posed + 1, buf);

		// ── 18. LE SECOND COMPOSANT ─────────────────────────────────────────
		check("18. la seconde declaration (definie dans la sonde) se pose comme la premiere",
			  NkComponentRegistry::Find("essai_panneau") != nullptr &&
				  pal.AddChild(0, "essai_panneau", NkAuthor::Humain) >= 0,
			  "prouve que l'APPLICATION ne connait aucun nom ; ne prouve rien sur la FORME");

		// ── 19. LA COMPOSITION ──────────────────────────────────────────────
		NkUIDocument doc;
		BuildProbeDocument(doc);
		const NkPaintRect surfA = {0.f, 0.f, 900.f, 600.f};
		NkLayoutResult layA;
		NkComputeLayout(doc, surfA, layA);

		const int32 corps = 2, dansCorps = 3;
		const bool nested = doc.IsValidIndex(dansCorps) && doc.nodes[(uint32)dansCorps].parent == corps;
		const bool bothPlaced = layA.Has(corps) && layA.Has(dansCorps);
		bool contained = false;
		if (bothPlaced) {
			const NkPaintRect &pr = layA.At(corps);
			const NkPaintRect &cr = layA.At(dansCorps);
			contained = cr.x >= pr.x - 0.01f && cr.y >= pr.y - 0.01f &&
						cr.x + cr.w <= pr.x + pr.w + 0.01f && cr.y + cr.h <= pr.y + pr.h + 0.01f;
		}
		snprintf(buf, sizeof(buf), "%u noeud(s), %u place(s)", doc.NodeCount(), layA.ValidCount());
		check("19. IMBRICATION : un composant pose DANS un autre, et son rectangle est CONTENU "
			  "dans celui du parent",
			  nested && bothPlaced && contained, buf);

		check("19b. CONTROLE NEGATIF : reparenter un noeud dans son propre enfant est refuse (cycle)",
			  !doc.Reparent(corps, dansCorps) && doc.nodes[(uint32)dansCorps].parent == corps, "");
		check("19c. CONTROLE NEGATIF : la racine ne se deplace pas et ne se supprime pas",
			  !doc.Reparent(0, corps) && !doc.RemoveSubtree(0), "");

		// ── 20. LA POSITION EST UN RESULTAT ─────────────────────────────────
		// ⚠️ L'ESSAI QUI PORTE TOUTE LA REGLE DE RODOLF. Le meme document, deux
		//    surfaces : si la position etait enregistree, elle ne bougerait pas.
		const NkPaintRect surfB = {0.f, 0.f, 1400.f, 600.f};
		NkLayoutResult layB;
		NkComputeLayout(doc, surfB, layB);
		const bool haveBoth = layA.Has(dansCorps) && layB.Has(dansCorps);
		const float32 wA = haveBoth ? layA.At(dansCorps).w : 0.f;
		const float32 wB = haveBoth ? layB.At(dansCorps).w : 0.f;
		snprintf(buf, sizeof(buf), "largeur %0.1f -> %0.1f pour une surface 900 -> 1400", wA, wB);
		check("20. LA POSITION EST UN RESULTAT : le meme document dans deux surfaces donne deux "
			  "mises en page",
			  haveBoth && wA > 0.f && wB > wA, buf);

		// Le controle qui DISCRIMINE : un enfant fixe ne doit PAS bouger quand
		// l'extensible bouge. Sans lui, « tout change » passerait pour une preuve
		// alors que ce serait le symptome d'un solveur qui ignore les modes.
		const int32 entete = 1;
		const float32 hA = layA.Has(entete) ? layA.At(entete).h : -1.f;
		const float32 hB = layB.Has(entete) ? layB.At(entete).h : -2.f;
		snprintf(buf, sizeof(buf), "entete FIXE : %0.1f puis %0.1f (declare 60)", hA, hB);
		check("20b. …et un enfant FIXE garde sa taille pendant que l'extensible change", hA == 60.f && hB == 60.f,
			  buf);

		// ── 21. LES BORNES MORDENT ──────────────────────────────────────────
		NkUIDocument bounded;
		bounded.NewDocument("Bornes", NkAuthor::Humain);
		bounded.SetMetric("espacement", 0.f);
		bounded.SetMetric("marge", 0.f);
		bounded.nodes[0].layout.kind = NkLayoutKind::Row;
		const int32 capped = bounded.AddChild(0, "", NkAuthor::Humain);
		const int32 free1 = bounded.AddChild(0, "", NkAuthor::Humain);
		bounded.nodes[(uint32)capped].width.maxVal = 200.f;
		NkLayoutResult layC;
		NkComputeLayout(bounded, surfA, layC);
		const float32 cw = layC.Has(capped) ? layC.At(capped).w : -1.f;
		const float32 fw = layC.Has(free1) ? layC.At(free1).w : -1.f;
		snprintf(buf, sizeof(buf), "borne %0.1f (max 200) + libre %0.1f = %0.1f (surface 900)", cw, fw,
				 cw + fw);
		check("21. un MAX mord, et ce qu'il rend va aux AUTRES (le parent reste rempli)",
			  layC.Has(capped) && layC.Has(free1) && cw <= 200.01f && cw > 0.f &&
				  (cw + fw) > 899.f && (cw + fw) < 901.f,
			  buf);

		// ── 22. L'AGENCEMENT DU PARENT CHANGE LE RESULTAT ───────────────────
		NkUIDocument arranged = doc;
		arranged.nodes[0].layout.kind = NkLayoutKind::Row;
		NkLayoutResult layD;
		NkComputeLayout(arranged, surfA, layD);
		const bool arrangedDiffers = layD.Has(entete) && layA.Has(entete) &&
									 !NkDesignAI::SameRect(layD.At(entete), layA.At(entete));
		check("22. changer l'AGENCEMENT du parent (colonne -> ligne) change la mise en page",
			  arrangedDiffers, "");

		NkUIDocument grid = doc;
		grid.nodes[0].layout.kind = NkLayoutKind::Grid;
		grid.nodes[0].layout.gridColumns = 2;
		NkLayoutResult layE;
		NkComputeLayout(grid, surfA, layE);
		const bool gridDiffers = layE.Has(entete) && !NkDesignAI::SameRect(layE.At(entete), layA.At(entete)) &&
								 !NkDesignAI::SameRect(layE.At(entete), layD.At(entete));
		check("22b. et la GRILLE donne un troisieme resultat, distinct des deux autres", gridDiffers, "");

		// ── 23. LA SOURIS ECRIT UNE PROPRIETE, JAMAIS UNE COORDONNEE ────────
		NkUIDocument dragged = doc;
		NkLayoutResult layF;
		NkComputeLayout(dragged, surfA, layF);
		const NkSizeMode modeBefore = dragged.nodes[(uint32)entete].height.mode;
		const float32 valBefore = dragged.nodes[(uint32)entete].height.value;
		const bool dragOk = NkResizeByDrag(dragged, layF, entete, false, 40.f);
		const float32 valAfter = dragged.nodes[(uint32)entete].height.value;
		snprintf(buf, sizeof(buf), "mode %s, valeur %0.1f -> %0.1f", NkSizeModeName(modeBefore), valBefore,
				 valAfter);
		check("23. TIRER UN BORD ecrit la TAILLE DECLAREE (ici un FIXE : 60 -> 100)",
			  dragOk && valAfter > valBefore + 39.f && valAfter < valBefore + 41.f, buf);

		// Le meme geste sur un EXTENSIBLE doit ecrire un POIDS, pas une taille.
		NkUIDocument dragged2 = doc;
		NkLayoutResult layG;
		NkComputeLayout(dragged2, surfA, layG);
		const bool dragOk2 = NkResizeByDrag(dragged2, layG, corps, false, -100.f);
		snprintf(buf, sizeof(buf), "mode devenu %s, poids %0.3f",
				 NkSizeModeName(dragged2.nodes[(uint32)corps].height.mode),
				 dragged2.nodes[(uint32)corps].height.value);
		check("23b. …et sur un EXTENSIBLE il ecrit un POIDS (le document reste responsive)",
			  dragOk2 && dragged2.nodes[(uint32)corps].height.mode == NkSizeMode::Weight &&
				  dragged2.nodes[(uint32)corps].height.value > 0.f,
			  buf);

		check("23c. CONDITION D'EXISTENCE : sans rectangle calcule, le glisser est REFUSE "
			  "(pas de poids invente)",
			  !NkResizeByDrag(dragged2, NkLayoutResult(), corps, false, 10.f), "");

		// ⚠️ LE TEMOIN CENTRAL DE LA REGLE. Et son CONTROLE POSITIF juste apres :
		//    un detecteur qui ne trouve jamais rien ne prouve rien.
		NkString dragText;
		dragged.Save(dragText);
		char foundKey[32];
		const bool hasCoord = ProbeFindsCoordinate(dragText.Data(), foundKey, sizeof(foundKey));
		snprintf(buf, sizeof(buf), "%u octets relus%s%s", (uint32)dragText.Length(),
				 hasCoord ? ", trouve : " : ", aucune cle de position", hasCoord ? foundKey : "");
		check("24. LE DOCUMENT ENREGISTRE APRES UN GLISSER NE CONTIENT AUCUNE COORDONNEE",
			  dragText.Length() > 0 && !hasCoord, buf);
		check("24b. CONTROLE POSITIF DU DETECTEUR : sur un texte qui EN contient une, il la trouve",
			  ProbeFindsCoordinate("nkuidoc 1\nnoeud 0\n  x = 12\n", foundKey, sizeof(foundKey)),
			  foundKey);

		// ── 25. LE DOCUMENT : ECRIT -> TEXTE -> RELU -> MEME DESSIN ─────────
		NkString docText;
		doc.Save(docText);
		NkUIDocument reread;
		uint32 docUnknown = 0;
		const bool reloadOk = reread.Load(docText.Data(), &docUnknown);
		snprintf(buf, sizeof(buf), "%u noeud(s) ecrits, %u relus, %u inconnu(s)", doc.NodeCount(),
				 reread.NodeCount(), docUnknown);
		check("25. ALLER-RETOUR DOCUMENT : il se relit, avec le meme nombre de noeuds",
			  reloadOk && docUnknown == 0 && reread.NodeCount() == doc.NodeCount() &&
				  reread.NodeCount() > 1,
			  buf);

		NkRecordingPaint dr1, dr2;
		RenderDocument(dr1, doc, surfA);
		RenderDocument(dr2, reread, surfA);
		snprintf(buf, sizeof(buf), "%u commandes, %u differences", (uint32)dr1.cmds.Size(),
				 dr1.DiffCount(dr2));
		check("25b. LE COEUR DU TEMOIN, A L'ECHELLE DU DOCUMENT : le relu donne le MEME DESSIN",
			  dr1.cmds.Size() > 20 && dr1.DiffCount(dr2) == 0, buf);

		// CONTROLE POSITIF du comparateur de documents : une modification DOIT se
		// voir. Sans lui, « 0 difference » ne se distinguerait pas d'un
		// comparateur aveugle.
		NkUIDocument changed = reread;
		changed.nodes[(uint32)entete].height.value = 140.f;
		NkRecordingPaint dr3;
		RenderDocument(dr3, changed, surfA);
		snprintf(buf, sizeof(buf), "%u differences apres avoir change une hauteur declaree",
				 dr1.DiffCount(dr3));
		check("25c. CONTROLE POSITIF : changer une taille declaree CHANGE le dessin",
			  dr1.DiffCount(dr3) > 0, buf);

		check("25d. le rejeu integre rend 0 divergence sur un document sain",
			  NkDesignAI::ReplayDiffs(doc, surfA) == 0, "");

		// ── 26. UN DOCUMENT INCOHERENT EST REFUSE, PAS RAFISTOLE ────────────
		NkUIDocument broken;
		check("26. un document dont un enfant n'existe pas est REFUSE (pas d'arbre a moitie "
			  "reconstruit)",
			  !broken.Load("nkuidoc 1\nnoeud 0\n  enfants = 7\n"), "");
		check("26b. un texte sans en-tete est refuse", !broken.Load("noeud 0\n  enfants =\n"), "");

		// ── 27. LA PROVENANCE ───────────────────────────────────────────────
		NkUIDocument prov;
		prov.NewDocument("Provenance", NkAuthor::Humain);
		const int32 byHand = prov.AddChild(0, "content_browser", NkAuthor::Humain);
		const int32 byMachine = prov.AddChild(0, "content_browser", NkAuthor::IA, "conserve");
		const bool posedAtCreation = prov.IsValidIndex(byHand) && prov.IsValidIndex(byMachine) &&
									 prov.nodes[(uint32)byHand].prov.author == NkAuthor::Humain &&
									 prov.nodes[(uint32)byMachine].prov.author == NkAuthor::IA;
		snprintf(buf, sizeof(buf), "%u humain(s), %u ia", prov.CountByAuthor(NkAuthor::Humain),
				 prov.CountByAuthor(NkAuthor::IA));
		check("27. LA PROVENANCE EST POSEE A LA CREATION, pas ajoutee ensuite", posedAtCreation, buf);

		prov.MarkVerified(byMachine);
		const bool wasVerified = prov.nodes[(uint32)byMachine].prov.verified;
		prov.MarkHumanEdit(byMachine);
		prov.MarkHumanEdit(byHand);
		snprintf(buf, sizeof(buf), "ia : corrigee=%d verifiee=%d | humain : corrigee=%d",
				 prov.nodes[(uint32)byMachine].prov.corrected ? 1 : 0,
				 prov.nodes[(uint32)byMachine].prov.verified ? 1 : 0,
				 prov.nodes[(uint32)byHand].prov.corrected ? 1 : 0);
		check("27b. une main qui passe apres l'IA marque CORRIGEE — et pas quand elle passe apres "
			  "elle-meme",
			  wasVerified && prov.nodes[(uint32)byMachine].prov.corrected &&
				  !prov.nodes[(uint32)byHand].prov.corrected,
			  buf);
		check("27c. et TOUTE edition retire le tampon « rejouee » (une verification porte sur ce "
			  "qui a ete verifie)",
			  !prov.nodes[(uint32)byMachine].prov.verified, "");

		NkString provText;
		prov.Save(provText);
		NkUIDocument provBack;
		provBack.Load(provText.Data());
		check("27d. la provenance SURVIT a l'aller-retour fichier (sinon le corpus la perd)",
			  provBack.NodeCount() == prov.NodeCount() && provBack.CountByAuthor(NkAuthor::IA) == 1 &&
				  provBack.CountCorrected() == 1,
			  "");

		// ── 28. L'IA : LE CATALOGUE VIENT DU REGISTRE ───────────────────────
		NkString catalog;
		NkDesignAI::BuildCatalog(catalog);
		uint32 named = 0;
		for (uint16 i = 0; i < NkComponentRegistry::Count(); ++i) {
			const NkComponentDecl *d = NkComponentRegistry::At(i);
			if (!d)
				continue;
			// Recherche naive de sous-chaine : suffisante ici, le catalogue est court.
			const char *s = catalog.Data();
			for (; s && *s; ++s) {
				const char *a = s, *b = d->name;
				while (*a && *b && *a == *b) {
					++a;
					++b;
				}
				if (!*b) {
					++named;
					break;
				}
			}
		}
		snprintf(buf, sizeof(buf), "%u composant(s) nomme(s) sur %u declare(s)", named,
				 NkComponentRegistry::Count());
		check("28. LE CATALOGUE DONNE A L'IA EST ENGENDRE DEPUIS LE REGISTRE (aucune liste ecrite)",
			  NkComponentRegistry::Count() > 0 && named == NkComponentRegistry::Count(), buf);

		NkString prompt;
		NkDesignAI::BuildPrompt("un panneau avec un navigateur", prompt);
		check("28b. le prompt engendre son VOCABULAIRE depuis les memes fonctions que l'ecrivain",
			  Contains(prompt.Data(), NkSizeModeName(NkSizeMode::Expand)) &&
				  Contains(prompt.Data(), NkLayoutKindName(NkLayoutKind::Grid)) &&
				  Contains(prompt.Data(), NkAlignName(NkAlign::Stretch)),
			  "");

		// ── 29. L'IA PASSE PAR LA MEME PORTE QUE LA MAIN ────────────────────
		NkUIDocument aiDoc;
		BuildProbeDocument(aiDoc);
		NkString beforeText;
		aiDoc.Save(beforeText);
		const uint32 nodesBefore = aiDoc.NodeCount();

		NkCannedBackend canned;
		canned.canned = NkString(ProbeValidReply());
		NkDesignAI ai;
		ai.SetBackend(&canned);
		const NkAIResult ok = ai.Ask("un bloc avec un navigateur", aiDoc, 0);
		snprintf(buf, sizeof(buf), "verdict=%s, +%u noeud(s), %u divergence(s) au rejeu",
				 NkAIVerdictName(ok.verdict), ok.nodesAdded, ok.replayDiffs);
		check("29. UNE REPONSE VALIDE ATTERRIT DANS LE DOCUMENT, par la meme fonction que la main",
			  ok.Accepted() && ok.nodesAdded == 2 && aiDoc.NodeCount() == nodesBefore + 2, buf);

		const bool stamped = ok.Accepted() && aiDoc.IsValidIndex(ok.graftedRoot) &&
							 aiDoc.nodes[(uint32)ok.graftedRoot].prov.author == NkAuthor::IA &&
							 aiDoc.nodes[(uint32)ok.graftedRoot].prov.verified;
		snprintf(buf, sizeof(buf), "%u noeud(s) d'origine IA", aiDoc.CountByAuthor(NkAuthor::IA));
		check("29b. la PROVENANCE se remplit toute seule : auteur = ia, et rejouee parce qu'elle "
			  "L'A ETE",
			  stamped && aiDoc.CountByAuthor(NkAuthor::IA) == 2, buf);

		// La suite immediate, et c'est elle qui fabrique le corpus : Rodolf
		// modifie ce que la machine a pose.
		if (ok.Accepted())
			aiDoc.MarkHumanEdit(ok.graftedRoot);
		check("29c. et la correction humaine qui suit devient le SIGNAL le plus precieux du corpus",
			  ok.Accepted() && aiDoc.nodes[(uint32)ok.graftedRoot].prov.corrected &&
				  !aiDoc.nodes[(uint32)ok.graftedRoot].prov.verified,
			  "");

		// ── 30. LES CAS NEGATIFS : LE DOCUMENT RESTE INTACT ─────────────────
		// ⚠️ COMPARAISON OCTET POUR OCTET AVANT/APRES. Un « rien n'a change »
		//    verifie par le nombre de noeuds passerait a cote d'une provenance
		//    salie ou d'un reglage ecrase.
		NkUIDocument guard;
		BuildProbeDocument(guard);
		NkString guardBefore;
		guard.Save(guardBefore);

		struct BadCase {
				const char *label;
				const char *reply;
				NkAIVerdict expected;
		};
		const BadCase kBad[] = {
			{"30. texte sans document", "Bien sur ! Voici une belle interface.\n",
			 NkAIVerdict::TexteNonConforme},
			{"30b. composant inconnu du registre",
			 "nkuidoc 1\nnoeud 0\n  composant = widget_magique\n  enfants =\n",
			 NkAIVerdict::ComposantInconnu},
			{"30c. structure incoherente (enfant inexistant)",
			 "nkuidoc 1\nnoeud 0\n  enfants = 9\n", NkAIVerdict::TexteNonConforme},
		};
		for (uint32 i = 0; i < sizeof(kBad) / sizeof(kBad[0]); ++i) {
			NkDesignAI bad;
			NkCannedBackend badBackend;
			badBackend.canned = NkString(kBad[i].reply);
			bad.SetBackend(&badBackend);
			const NkAIResult r = bad.Ask("peu importe", guard, 0);
			NkString after;
			guard.Save(after);
			const bool intact = SameText(guardBefore.Data(), after.Data());
			snprintf(buf, sizeof(buf), "verdict=%s, document %s", NkAIVerdictName(r.verdict),
					 intact ? "INTACT (octet pour octet)" : "MODIFIE");
			check(kBad[i].label, !r.Accepted() && r.verdict == kBad[i].expected && intact, buf);
		}

		NkDesignAI mute;
		NkCannedBackend silent;
		silent.canned = NkString("");
		mute.SetBackend(&silent);
		const NkAIResult muteRes = mute.Ask("rien", guard, 0);
		NkString afterMute;
		guard.Save(afterMute);
		check("30d. un backend muet est un REFUS, pas un document vide",
			  muteRes.verdict == NkAIVerdict::BackendMuet && SameText(guardBefore.Data(), afterMute.Data()),
			  "");

		// ⚠️ SANS CE CONTROLE, LES QUATRE PRECEDENTS NE VALENT RIEN : un
		//    comparateur qui rendrait toujours « identique » les ferait tous
		//    passer, y compris si le document avait ete saccage.
		check("30e. CONTROLE POSITIF DU COMPARATEUR : identique = vrai, different = faux",
			  SameText(guardBefore.Data(), guardBefore.Data()) &&
				  !SameText("nkuidoc 1\ntitre = A\n", "nkuidoc 1\ntitre = B\n") && !SameText("a", "ab"),
			  "");

		// -- 31. LE CHOIX DU BACKEND GRAPHIQUE ------------------------------
		// ⚠️ CETTE FAMILLE EXISTE PARCE QUE SON ABSENCE A COUTE UN ESSAI GPU.
		//    Le choix de backend vivait dans `main`, donc hors de portee d'une
		//    sonde headless. Il a tourne une fois, sur GPU, et son journal a
		//    imprime « backend graphique demande : {} (source : {}) » : la trace
		//    existait et ne disait RIEN (mauvaise famille de formatage, plus des
		//    accolades nues -- cf. l'en-tete de `Backend.h`). Le seul code que le
		//    GPU touchait etait le seul code SANS TEMOIN. La resolution est
		//    maintenant une fonction pure, et c'est EXACTEMENT celle que `main`
		//    appelle : la sonde ne verifie pas une copie de la regle, elle
		//    verifie la regle.

		// Deux detecteurs, et chacun est controle AVANT de servir a quoi que ce
		// soit : un detecteur toujours-vrai ferait passer toute la famille a vide.
		auto hasBrace = [](const char *t) {
			for (; t && *t; ++t)
				if (*t == '{' || *t == '}')
					return true;
			return false;
		};
		auto contains = [](const char *hay, const char *needle) {
			if (!hay || !needle || !*needle)
				return false;
			for (; *hay; ++hay) {
				const char *h = hay;
				const char *n = needle;
				while (*h && *n && *h == *n) {
					++h;
					++n;
				}
				if (!*n)
					return true;
			}
			return false;
		};
		check("31. CONTROLE DES DEUX DETECTEURS (sans lui, 31a-31i passeraient a vide)",
			  hasBrace("a{0}b") && hasBrace("}") && !hasBrace("aucune accolade ici") &&
				  contains("demande : vulkan |", "vulkan") && contains("abc", "abc") &&
				  !contains("demande : opengl", "vulkan") && !contains("ab", "abc"),
			  "voit une accolade, voit un sous-texte, et REFUSE dans les deux sens");

		// 31a. LA LIGNE EXISTE -- a verifier AVANT de chercher ce qu'elle contient.
		const char *kVulkanArgs[] = {"--small", "--gfx=vulkan"};
		const NkGfxChoice cVk = NkGfxResolve(nullptr, nullptr, kVulkanArgs, 2);
		const NkString lineVk = NkGfxJournalLine(cVk);
		snprintf(buf, sizeof(buf), "%u caracteres", (uint32)lineVk.Length());
		check("31a. la ligne de journal EXISTE et n'est pas un moignon", lineVk.Length() > 60, buf);

		// 31b. ET ELLE A REELLEMENT SUBSTITUE -- le defaut du 18/08, en une ligne.
		check("31b. AUCUNE accolade non substituee ne subsiste", !hasBrace(lineVk.Data()),
			  lineVk.Data());

		// 31c. DEMANDE **ET** RETENU, tous deux nommes (regle 2 de Rodolf).
		check("31c. la ligne nomme le backend DEMANDE, le RETENU, et le POURQUOI",
			  contains(lineVk.Data(), "vulkan") && contains(lineVk.Data(), "demande") &&
				  contains(lineVk.Data(), "retenu") && contains(lineVk.Data(), "pourquoi"),
			  "");

		// 31d. LA TABLE DE RESOLUTION, avec son controle positif : six noms doivent
		//      donner six valeurs DISTINCTES. Un correspondant constant rendrait
		//      « toutes supportees » vrai sans rien resoudre du tout.
		const char *kNames[] = {"auto", "opengl", "vulkan", "dx11", "dx12", "software"};
		NkEditorGfxApi got[6];
		bool allParsed = true, allSupported = true;
		for (uint32 i = 0; i < 6; ++i) {
			NkGfxChoice c;
			allParsed = allParsed && NkGfxParse(kNames[i], c);
			allSupported = allSupported && c.supported;
			got[i] = c.api;
		}
		bool allDistinct = true;
		for (uint32 i = 0; i < 6; ++i)
			for (uint32 j = i + 1; j < 6; ++j)
				if (got[i] == got[j])
					allDistinct = false;
		check("31d. les 6 noms sont acceptes ET donnent 6 API DISTINCTES",
			  allParsed && allSupported && allDistinct,
			  allDistinct ? "aucune collision" : "DEUX NOMS TOMBENT SUR LA MEME API");

		// 31e. `metal` : accepte a l'ANALYSE, refuse a la RESOLUTION, AVEC raison.
		//      Un refus sans raison est un repli muet deguise en refus.
		NkGfxChoice cMetal;
		const bool metalParsed = NkGfxParse("metal", cMetal);
		const NkString lineMetal = NkGfxJournalLine(cMetal);
		check("31e. `metal` est REFUSE, avec une raison non vide, et la ligne le dit",
			  metalParsed && !cMetal.supported && cMetal.reason && *cMetal.reason &&
				  contains(lineMetal.Data(), "refuse") && !hasBrace(lineMetal.Data()),
			  cMetal.reason);

		NkGfxChoice cJunk;
		check("31f. une valeur inconnue est REFUSEE, pas rabattue sur un defaut",
			  NkGfxParse("directx", cJunk) && !cJunk.supported && cJunk.reason && *cJunk.reason,
			  cJunk.reason);

		// 31g. L'ORDRE DE PRIORITE, VERIFIE DANS LES DEUX SENS. « La ligne de
		//      commande gagne » passerait AUSSI si la variable etait ignoree tout
		//      court : il faut donc prouver D'ABORD que la variable est lue.
		const NkGfxChoice cEnvSeul = NkGfxResolve(nullptr, "vulkan", nullptr, 0);
		const char *kDx12[] = {"--gfx=dx12"};
		const NkGfxChoice cEnvEtLigne = NkGfxResolve(nullptr, "vulkan", kDx12, 1);
		const NkGfxChoice cRien = NkGfxResolve(nullptr, nullptr, nullptr, 0);
		const NkGfxChoice cVide = NkGfxResolve(nullptr, "", nullptr, 0);
		check("31g. priorite : la variable est LUE, puis la ligne de commande la BAT",
			  cEnvSeul.api == NkEditorGfxApi::Vulkan &&
				  cEnvSeul.source == NkGfxSource::Environnement &&
				  cEnvEtLigne.api == NkEditorGfxApi::DX12 &&
				  cEnvEtLigne.source == NkGfxSource::LigneDeCommande &&
				  cRien.source == NkGfxSource::DetectionAuto &&
				  cVide.source == NkGfxSource::DetectionAuto,
			  "et une variable VIDE vaut « absente », pas « erreur »");

		// 31h. `auto` NE SE JOURNALISE JAMAIS COMME RETENU. C'est l'autre moitie
		//      de la regle 2 : « auto » dit QUI DECIDE, jamais CE QUI A ETE PRIS.
		check("31h. `auto` retient une API CONCRETE, jamais le mot « auto »",
			  cRien.effective && *cRien.effective && !SameText(cRien.effective, "auto"),
			  cRien.effective);

		// 31i. L'OPTION EST RECONNUE SANS ETRE GOURMANDE : `--gfx=`, et rien d'autre.
		check("31i. `--gfx=` est reconnue, les autres options restent tranquilles",
			  NkGfxArgValue("--gfx=vulkan") && SameText(NkGfxArgValue("--gfx=vulkan"), "vulkan") &&
				  !NkGfxArgValue("--small") && !NkGfxArgValue("--probe") &&
				  !NkGfxArgValue("--gfy=vulkan") && !NkGfxArgValue(nullptr),
			  "");

		// -- 32. UNE SEULE RESOLUTION DANS LE PROGRAMME --------------------
		// ⚠️ CETTE FAMILLE NAIT DU PREMIER TEMOIN VISUEL (18/08). L'application
		//    a ouvert sa fenetre et le navigateur de contenu s'est peint en
		//    MAGENTA FRANC. Ce n'est pas un accident d'affichage : c'est le repli
		//    delibere de `NkTheme::Get` pour un identifiant de role hors table --
		//    « ca doit sauter aux yeux, pas se fondre en noir ». Il a fait son
		//    travail.
		//
		//    LA CAUSE, lue dans le code et non devinee : les noms canoniques de
		//    `themedetail::RoleNames()` sont en snake_case (« panel_bg »),
		//    `NkResolveRole` compare OCTET POUR OCTET, et les roles par defaut
		//    declares sont en PascalCase (« PanelBg »). Aucun ne tombe juste,
		//    donc NK_ROLE_INVALID (0xFFFF), donc magenta.
		//
		//    ⚠️ ET VOICI POURQUOI 21/21, PUIS 68/68, PUIS 72/72 N'ONT RIEN VU :
		//    la sonde resolvait les roles avec SA PROPRE fonction, un hachage
		//    vers 1..250 qui ne rendait JAMAIS NK_ROLE_INVALID. Un resolveur qui
		//    dit oui a tout ne peut pas voir un nom faux. Ce n'etait pas un essai
		//    manquant : c'etait l'INSTRUMENT, aveugle a une classe entiere
		//    d'erreurs, et aucun essai supplementaire ne l'aurait rattrape.
		//
		//    LA CORRECTION EST STRUCTURELLE, PAS ADDITIVE : le resolveur de la
		//    sonde a ete SUPPRIME (voir le bloc en haut de ce fichier). Il n'y a
		//    plus qu'une resolution -- `NkDesignResolveRole` -- et elle est la
		//    valeur PAR DEFAUT de `NkDocumentHost`, donc on ne peut plus en poser
		//    une autre par distraction.
		{
			// 32a. LE RESOLVEUR DU THEME SAIT DIRE NON -- a etablir AVANT de s'en
			//      servir pour juger quoi que ce soit. Un resolveur permissif
			//      rendrait tout ce qui suit muet.
			const uint16 connu = NkResolveRole("panel_bg");
			const uint16 inconnu = NkResolveRole("role_qui_n_existe_pas_du_tout");
			check("32a. le resolveur du THEME accepte un nom canonique ET REFUSE un nom faux",
				  connu != NK_ROLE_INVALID && inconnu == NK_ROLE_INVALID,
				  connu != NK_ROLE_INVALID ? "panel_bg resolu, nom faux rejete"
										   : "PANEL_BG LUI-MEME NE RESOUT PAS");

			// 32b. LA SONDE ET L'APPLICATION PARTAGENT LA MEME RESOLUTION.
			//      C'est l'assertion qui remplace l'ancien « le resolveur de la
			//      sonde dit oui a tout, donc ne juge pas les noms » -- une phrase
			//      qui decrivait le defaut au lieu de l'interdire. Ici, si
			//      quelqu'un repose un resolveur maison sur l'hote, cette ligne
			//      rougit.
			const NkDocumentHost temoinHote;
			check("32b. l'hote de document resout PAR DEFAUT avec la fonction de l'application",
				  temoinHote.resolve == &NkDesignResolveRole,
				  "une seule resolution dans le programme, sonde comprise");

			// 32c. ET CETTE RESOLUTION-LA SAIT DIRE NON AUSSI. Sans cette ligne,
			//      32b garantirait seulement que les deux cotes sont d'accord --
			//      y compris d'accord pour tout accepter.
			check("32c. la resolution de l'APPLICATION refuse un nom qui n'existe sous aucune forme",
				  NkDesignResolveRole("role_qui_n_existe_pas_du_tout") == NK_ROLE_INVALID &&
					  NkDesignResolveRole("panel_bg") != NkDesignResolveRole("border"),
				  "elle refuse l'inconnu, et reste injective sur deux roles distincts");

			// 32d. MES PROPRES NOMS DE ROLES RESOLVENT. C'est la part qui
			//      m'appartient, et elle doit etre verte.
			const char *kMiens[] = {"panel_bg", "border", "text", "text_muted", "accent_ui"};
			bool miensOk = true;
			for (uint32 m = 0; m < 5; ++m)
				if (NkDesignResolveRole(kMiens[m]) == NK_ROLE_INVALID)
					miensOk = false;
			check("32d. les 5 roles ecrits en clair dans `Renderers.h` et `Panels.h` resolvent tous",
				  miensOk, "panel_bg, border, text, text_muted, accent_ui");
		}

		// -- 33. LA CANONISATION, ET LE REPLI FRANC -------------------------
		// ⚠️ CE QUI A ETE REPARE ICI EST LA CAUSE, PAS LES VINGT-TROIS NOMS.
		//    Renommer les `defaultRole` de `NkContentBrowserModel.h` (10) et de
		//    `NkTreeViewModel.h` (13) aurait rendu l'ecran juste ce soir et
		//    laisse le vingt-quatrieme jeton refaire la meme erreur -- une
		//    convention que l'auteur doit CONNAITRE pour l'appliquer sera
		//    enfreinte par le prochain auteur. Deux jetons sur deux fichiers
		//    ecrits par deux personnes, 23 sur 23 : ce n'est pas une inattention,
		//    c'est une classe de defaut.
		//
		//    (a) la resolution CANONISE -> la classe de defaut disparait ;
		//    (b) le repli est FRANC -> ce que (a) ne couvre pas se DIT.
		{
			NkRoleAudit::Reset();
			nkentseu::editorkit::NkRoleAudit::Reset();

			// 33a. LA CANONISATION EST JUSTE SUR DES CAS ECRITS D'AVANCE. La
			//      table est posee AVANT de mesurer quoi que ce soit : une
			//      fonction jugee sur ses propres sorties ne serait jamais fausse.
			struct Cas {
					const char *ecrit;
					const char *attendu;
			};
			static const Cas kCas[] = {
				{"PanelBg", "panel_bg"},		   {"PanelHeader", "panel_header"},
				{"TextOnAccent", "text_on_accent"}, {"AccentUi", "accent_ui"},
				{"TypeFolder", "type_folder"},	   {"InputBg", "input_bg"},
				// ⚠️ LES DEUX SUIVANTS SONT LE CONTROLE NEGATIF DE LA FONCTION :
				//    un nom deja canonique ne doit pas bouger d'un octet, et un
				//    role d'EXTENSION d'application non plus. Sans eux, une
				//    fonction qui abimerait tout sauf le PascalCase passerait.
				{"panel_bg", "panel_bg"},		   {"nk3d.anneau_brosse", "nk3d.anneau_brosse"},
			};
			bool canonOk = true;
			uint32 casVus = 0;
			NkString canonEcarts;
			for (uint32 i = 0; i < sizeof(kCas) / sizeof(kCas[0]); ++i) {
				char got[96];
				++casVus;
				// ⚠️ REPOINTEE, PAS SUPPRIMEE. Cette famille a ete ecrite pour
				//    mordre, et elle mord encore -- sur l'exemplaire du kit, qui est
				//    desormais le seul. Supprimer l'essai avec le doublon aurait
				//    retire la garde en meme temps que la redondance.
				if (!nkentseu::editorkit::NkCanonicalRoleName(kCas[i].ecrit, got, sizeof(got)) ||
					!SameText(got, kCas[i].attendu)) {
					canonOk = false;
					if (!canonEcarts.Empty())
						canonEcarts.Append(", ");
					canonEcarts.Append(kCas[i].ecrit);
					canonEcarts.Append("->");
					canonEcarts.Append(got);
				}
			}
			snprintf(buf, sizeof(buf), "%u cas ecrits d'avance%s%s", casVus,
					 canonOk ? ", tous conformes" : ", ECARTS : ",
					 canonOk ? "" : canonEcarts.Data());
			check("33a. la canonisation rend EXACTEMENT la forme attendue (et laisse le snake_case "
				  "intact)",
				  canonOk && casVus == 8, buf);

			// 33b. ELLE NE FABRIQUE PAS UN NOM VALIDE A PARTIR DE RIEN. Une
			//      canonisation qui rattraperait tout serait le meme defaut que le
			//      hachage qu'elle remplace, deplace d'un cran.
			check("33b. un nom qui n'existe sous AUCUNE ecriture reste non resolu",
				  NkDesignResolveRole("RoleQuiNExistePasDuTout") == NK_ROLE_INVALID,
				  "la canonisation rattrape une GRAPHIE, elle n'invente pas un role");

			// 33c. LE REPLI EST FRANC, VERIFIE DANS LES DEUX SENS. Le magenta
			//      disait « il y a un probleme » ; il ne disait ni lequel, ni
			//      combien, ni ou. L'audit doit NOMMER le role fautif -- et ne
			//      rien nommer quand tout va bien.
			// ⚠️ REPOINTE SUR L'AUDIT DU KIT, PAS SUPPRIME. Depuis le
			//    2026-08-29, `NkDesignResolveRole` DELEGUE a
			//    `NkRoleRegistry::Find` : c'est donc le kit qui note la faute, et
			//    le registre local ne recoit plus rien. Cet essai a MORDU sur ce
			//    changement -- il est passe de 131/131 a 130/131 en disant
			//    « L'AUDIT N'A PAS NOMME LE FAUTIF », ce qui etait exact. On le
			//    fait donc regarder au bon endroit ; le supprimer aurait retire la
			//    garde en meme temps que le doublon qu'elle venait de detecter.
			const bool nomme = nkentseu::editorkit::NkRoleAudit::FaultCount() == 1 &&
							   SameText(nkentseu::editorkit::NkRoleAudit::Faults()[0].name.Data(),
										"RoleQuiNExistePasDuTout");
			NkRoleAudit::Reset();
			nkentseu::editorkit::NkRoleAudit::Reset();
			NkDesignResolveRole("panel_bg");
			const bool muetQuandTouVaBien = nkentseu::editorkit::NkRoleAudit::FaultCount() == 0;
			check("33c. le repli est FRANC : il NOMME le role fautif, et se tait quand il n'y en a pas",
				  nomme && muetQuandTouVaBien,
				  nomme ? "role fautif nomme, silence sinon" : "L'AUDIT N'A PAS NOMME LE FAUTIF");

			// 33d. TOUS LES JETONS DE TOUS LES COMPOSANTS ENREGISTRES RESOLVENT.
			//      ⚠️ C'EST L'ESSAI QUI DEVAIT MORDRE SUR L'ETAT DU 18/08, et il
			//      mord : sans la canonisation, il rend 23 roles non resolus (10
			//      pour le navigateur, 13 pour l'arbre) et la sonde sort en 1.
			//      Mesure faite en desactivant la canonisation, pas supposee.
			//
			//      Il boucle sur le REGISTRE et ne nomme aucun composant : le jour
			//      ou un troisieme s'inscrit, il est couvert sans qu'une ligne
			//      bouge ici. C'est la difference entre un essai et un audit.
			NkRoleAudit::Reset();
			nkentseu::editorkit::NkRoleAudit::Reset();
			uint32 jetonsVus = 0, jetonsCasses = 0;
			NkString casses;
			const uint16 nbComp = NkComponentRegistry::Count();
			for (uint16 c = 0; c < nbComp; ++c) {
				const NkComponentDecl *d = NkComponentRegistry::At(c);
				if (!d)
					continue;
				for (uint16 t = 0; t < d->tokenCount; ++t) {
					++jetonsVus;
					if (NkDesignResolveRole(d->tokens[t].defaultRole) == NK_ROLE_INVALID) {
						++jetonsCasses;
						if (!casses.Empty())
							casses.Append(", ");
						casses.Append(d->name);
						casses.Append('.');
						casses.Append(d->tokens[t].name);
						casses.Append("->");
						casses.Append(d->tokens[t].defaultRole);
					}
				}
			}
			snprintf(buf, sizeof(buf), "%u composant(s), %u jeton(s) examine(s), %u non resolu(s)",
					 (uint32)nbComp, jetonsVus, jetonsCasses);
			check("33d. l'audit a REELLEMENT parcouru des jetons (sinon 0 casse ne dit rien)",
				  jetonsVus >= 20 && nbComp >= 2, buf);
			check("33e. AUCUN jeton declare ne tombe dans le repli magenta",
				  jetonsCasses == 0, jetonsCasses == 0 ? buf : casses.Data());

			// 33f. LE RATTRAPAGE EST TRACE, ET C'EST LA MOITIE QUI MANQUERAIT.
			//      Sans cette liste, (a) rendrait les declarations fausses
			//      INVISIBLES : l'ecran serait juste, personne ne corrigerait
			//      jamais la source, et le jour ou la canonisation bougerait, 23
			//      jetons casseraient d'un coup. C'est « une protection qui
			//      empeche d'aller verifier » -- ici elle est mesuree et publiee.
			//
			//      ⚠️ CE QU'ELLE N'ASSERTE PAS, ET LA PREMIERE ECRITURE LE FAISAIT :
			//         « RescuedCount() > 0 ». Cet essai serait devenu ROUGE le jour
			//         ou quelqu'un corrigerait les declarations a la source --
			//         c'est-a-dire qu'il aurait puni le correctif qu'il reclame. Un
			//         essai qui echoue quand le probleme est resolu n'est pas un
			//         essai, c'est un cliquet. L'assertion porte donc sur la FORME
			//         de la trace, jamais sur le nombre, et le nombre est publie a
			//         cote comme diagnostic.
			bool traceOk = true;
			for (uint32 i = 0; i < NkRoleAudit::RescuedCount(); ++i)
				if (NkRoleAudit::Rescued()[i].name.Empty() ||
					NkRoleAudit::Rescued()[i].canon.Empty())
					traceOk = false;
			snprintf(buf, sizeof(buf),
					 "%u graphie(s) PascalCase distincte(s), sur %u jeton(s) declare(s)",
					 NkRoleAudit::RescuedCount(), jetonsVus);
			check("33f. tout rattrapage porte le nom DECLARE et la forme qui a resolu (liste de "
				  "travail de la correction a la source)",
				  traceOk, buf);

			NkString resume;
			NkRoleAudit::Summary(resume, 32);
			rep.Append("\n--- audit des roles declares, TOUS composants du registre ---\n  ");
			rep.Append(resume);
			rep.Append("\n  cause : roles declares en PascalCase, table canonique en snake_case.\n"
					   "  correctif ICI : la resolution canonise (Roles.h). Correctif A LA SOURCE :\n"
					   "  a `NkRoleRegistry::Find` -- porte au canal, hors perimetre de cet agent.\n");
		}


		// -- 34. LA GEOMETRIE : CE QUI DEBORDE, ET DE COMBIEN ----------------
		// ⚠️ NEE D'UNE LECTURE SUR IMAGE, ET C'EST EXACTEMENT POURQUOI ELLE
		//    EXISTE. Le 18/08 au soir, en regardant la capture, on a rapporte
		//    « la colonne d'arborescence se superpose a la premiere rangee de
		//    cartes » -- plus grave que mon ecart n. 11, qui disait « rognees au
		//    bord gauche ». Deux lectures differentes de la MEME image, et aucune
		//    des deux n'est un instrument.
		//
		//    Ce banc tranche avec des nombres. Il ne compare pas des pixels : il
		//    lit les RECTANGLES que le composant a emis.
		//
		// ⚠️ ET IL NE CONNAIT PAS LA GEOMETRIE DU COMPOSANT. La frontiere entre
		//    la colonne et la grille n'est PAS recalculee ici a partir de
		//    `tree_width` -- ce serait reconstruire le calcul que l'on veut
		//    verifier, et il serait juste par construction. Elle est lue dans le
		//    `PushClip` que le composant emet lui-meme pour sa grille : c'est SA
		//    declaration de « voici ma zone », et c'est contre elle qu'on juge.
		{
			const NkPaintRect zone = {0.f, 0.f, 900.f, 600.f}; // ce que `Render` donne
			NkRecordingPaint g;
			Render(g, nullptr, idle);

			// 34a. CONDITION D'EXISTENCE, d'abord : sans commandes, « rien ne
			//      deborde » serait vrai et ne vaudrait rien.
			check("34a. le composant a REELLEMENT emis des commandes (sinon 0 debordement ne dit rien)",
				  g.cmds.Size() > 20, "");

			// 34b. RIEN NE SORT DU RECTANGLE DONNE AU COMPOSANT.
			//      C'est la question « rognees au bord gauche » : un dessin pose
			//      a gauche de `zone.x` serait coupe par la fenetre.
			uint32 dehors = 0;
			float32 pireGauche = 0.f, pireDroite = 0.f;
			NkString horsZone;
			for (uint32 i = 0; i < (uint32)g.cmds.Size(); ++i) {
				const NkPaintCmd &c = g.cmds[i];
				if (c.op == NkPaintOp::PushClip || c.op == NkPaintOp::PopClip)
					continue;
				const float32 gauche = zone.x - c.x;
				const float32 droite = (c.x + c.w) - (zone.x + zone.w);
				if (gauche > 0.01f || droite > 0.01f) {
					++dehors;
					if (gauche > pireGauche)
						pireGauche = gauche;
					if (droite > pireDroite)
						pireDroite = droite;
					if (dehors <= 3) {
						if (!horsZone.Empty())
							horsZone.Append(", ");
						horsZone.Append(NkPaintOpName(c.op));
						char b[64];
						snprintf(b, sizeof(b), "@x=%.1f w=%.1f", c.x, c.w);
						horsZone.Append(b);
					}
				}
			}
			// ⚠️ UN DEBORDEMENT CONNU, NOMME, ET QUI N'EST PAS MON FICHIER :
			//    `NkContentBrowserDraw.cpp:129` passe `header.w` a un texte pose a
			//    `header.x + card_pad` -- donc **8 px de plus que le panneau**. Le
			//    clip du panneau le masque a l'ecran, et c'est bien la le probleme :
			//    **le texte croit disposer de 8 px qu'il n'a pas**, donc son point
			//    de troncature est calcule sur une largeur fausse. C'est la meme
			//    famille que « les libelles tronques ».
			//
			//    L'assertion est **monotone** et c'est delibere : `<= 1`, pas `== 1`.
			//    Ecrite `== 1`, elle deviendrait ROUGE le jour ou l'autre agent
			//    corrige -- elle punirait le correctif qu'elle reclame, exactement
			//    l'erreur que j'ai deja faite et retiree en 33f. Elle mord si un
			//    debordement NOUVEAU apparait, jamais si celui-ci disparait.
			// ⚠️ CETTE LIGNE A ETE UNE QUARANTAINE, ET ELLE N'EN EST PLUS UNE.
			//    Elle a d'abord tolere `<= 1` : un debordement connu de 8 px vivait
			//    dans `NkContentBrowserDraw.cpp:129` (`header.w` passe a un texte
			//    pose a `header.x + card_pad`). La tolerance etait **monotone** —
			//    `<= 1`, jamais `== 1` — pour ne pas rougir le jour ou quelqu'un
			//    corrigerait ; c'est ce qui a permis de resserrer a `== 0` sans
			//    rien casser le jour ou la correction est arrivee.
			//    *Une quarantaine ecrite `==` aurait puni son propre correctif.*
			snprintf(buf, sizeof(buf),
					 "%u commande(s) hors zone ; debord max gauche %.1f px, droite %.1f px", dehors,
					 pireGauche, pireDroite);
			check("34b. AUCUNE commande ne sort du rectangle donne au composant", dehors == 0,
				  dehors == 0 ? buf : horsZone.Data());

			// 34c. LA COLONNE ET LA GRILLE NE SE CHEVAUCHENT PAS.
			//      La frontiere est celle que le composant a DECLAREE lui-meme
			//      (son premier `PushClip`), pas une que je recalcule.
			//
			// ⚠️ CORRECTION DE L'INSTRUMENT, ET ELLE VAUT D'ETRE ECRITE : ma
			//    premiere version prenait le PREMIER `PushClip`. Elle est passee
			//    VERTE avec « frontiere x=0.0 » -- c'est-a-dire qu'elle mesurait
			//    le clip du PANNEAU ENTIER, pas celui de la grille. Un vert pour
			//    la mauvaise raison, exactement la 2e face de la grille, et je ne
			//    l'ai vu que parce que le chiffre publie a cote (`x=0.0`) etait
			//    absurde. **Publier la valeur intermediaire a sauve le banc.**
			//
			//    La regle juste est STRUCTURELLE, pas devinatoire — et elle a du
			//    CHANGER avec le mixte du 30/08, en le disant : la version
			//    precedente prenait « le dernier `PushClip` avant le premier
			//    `PopClip` ». Depuis le mixte, deux decoupes s'ouvrent ET se
			//    ferment AVANT la grille (le fil d'Ariane clippe a sa zone de
			//    barre d'outils, puis les decoupes internes du tree_view embarque)
			//    -- l'ancien repere aurait retenu le clip du fil d'Ariane, un vert
			//    pour la mauvaise raison. Le repere qui tient : **la grille est la
			//    DERNIERE decoupe ouverte du flux** -- tout ce qui vient apres elle
			//    est son contenu, puis les barres basses pleine largeur.
			float32 gridX = -1.f;
			uint32 iClip = 0, nbClips = 0;
			NkString clips;
			for (uint32 i = 0; i < (uint32)g.cmds.Size(); ++i) {
				if (g.cmds[i].op == NkPaintOp::PushClip) {
					++nbClips;
					gridX = g.cmds[i].x;
					iClip = i;
					char b[64];
					snprintf(b, sizeof(b), "%s(x=%.1f w=%.1f)", nbClips > 1 ? " > " : "",
							 g.cmds[i].x, g.cmds[i].w);
					clips.Append(b);
				}
			}
			snprintf(buf, sizeof(buf), "%u clip(s) imbrique(s) : %s -- frontiere retenue x=%.1f",
					 nbClips, clips.Data(), gridX);
			check("34c. le composant DECLARE la zone de sa grille, et elle n'est PAS le panneau entier",
				  gridX > 0.01f && nbClips >= 2, buf);

			// 34d. DANS LES DEUX SENS -- c'est ce qui separe « rogne » de
			//      « pose par-dessus ». Un dessin de la colonne qui franchit la
			//      frontiere se pose SUR la grille ; un dessin de la grille qui
			//      la franchit en arriere se pose SUR la colonne.
			//
			// ⚠️ SECONDE CORRECTION DE L'INSTRUMENT, ET ELLE A RENVERSE LE
			//    VERDICT. Sans borne verticale, le banc accusait deux textes qui
			//    ne sont PAS dans la colonne : « Creer » (barre d'outils, y=28) et
			//    « niveau1 » (fil d'Ariane, y=64). Ces bandes traversent
			//    legitimement tout le panneau. Les compter, c'etait fabriquer un
			//    debordement qui n'existe pas -- et **confirmer la lecture faite
			//    sur l'image que ce banc devait justement departager**.
			//    La borne vient du clip declare par le composant : la colonne
			//    commence a la MEME hauteur que la grille.
			const float32 bandeTop = (iClip < (uint32)g.cmds.Size()) ? g.cmds[iClip].y : 0.f;

			// ⚠️ LE COMPTAGE EST UNE FONCTION, APPELEE DEUX FOIS : une fois sur le
			//    flux reel, une fois sur un flux volontairement fautif (34e). Ecrire
			//    deux boucles jumelles aurait mesure une RECONSTRUCTION -- la 4e
			//    face de la grille -- et le controle positif n'aurait rien prouve
			//    du detecteur reellement employe.
			// ⚠️ TROISIEME BORNE, ET LA DERNIERE : une commande qui couvre TOUTE la
			//    largeur du panneau est une BANDE (separateur, fond de barre), pas
			//    un element de colonne. Le critere est EXACT -- « commence au bord
			//    gauche et finit au bord droit » -- et non un seuil du genre
			//    « plus de 90 % ». Un seuil arbitraire aurait laisse passer une
			//    bande a 89 % et rejete un vrai debordement a 91 %.
			auto bandePleineLargeur = [&](const NkPaintCmd &c) {
				return c.x <= zone.x + 0.01f && (c.x + c.w) >= zone.x + zone.w - 0.01f;
			};
			auto compter = [&](const NkRecordingPaint &flux, uint32 fin, uint32 &colGrille,
							   uint32 &grilleCol, float32 &pire, NkString *coupables) {
				colGrille = 0;
				grilleCol = 0;
				pire = 0.f;
				for (uint32 i = 0; i < fin && i < (uint32)flux.cmds.Size(); ++i) {
					const NkPaintCmd &c = flux.cmds[i];
					if (c.op == NkPaintOp::PushClip || c.op == NkPaintOp::PopClip ||
						bandePleineLargeur(c))
						continue;
					if (c.x < gridX - 0.01f && c.y >= bandeTop - 0.01f && (c.x + c.w) > gridX + 0.01f) {
						++colGrille;
						const float32 de = (c.x + c.w) - gridX;
						if (de > pire)
							pire = de;
						if (coupables && colGrille <= 3) {
							if (!coupables->Empty())
								coupables->Append(", ");
							coupables->Append(NkPaintOpName(c.op));
							char b[96];
							snprintf(b, sizeof(b), "@x=%.1f y=%.1f w=%.1f deborde de %.1f px [%s]", c.x,
									 c.y, c.w, de, c.text.Data() ? c.text.Data() : "");
							coupables->Append(b);
						}
					}
				}
				// ⚠️ LE COTE « GRILLE » S'ARRETE A LA FERMETURE DE SA DECOUPE.
				//    Depuis le mixte, la BARRE D'ETAT s'emet APRES le PopClip de la
				//    grille (son compteur doit refleter le tour courant) : son
				//    texte de gauche commence a x=pad, DANS l'ancienne zone jugee
				//    « colonne ». Sans cette borne, l'instrument accuserait la
				//    barre d'etat d'un chevauchement qui n'existe pas — la meme
				//    erreur que les deux bandes de la seconde correction.
				for (uint32 i = fin + 1; i < (uint32)flux.cmds.Size(); ++i) {
					const NkPaintCmd &c = flux.cmds[i];
					if (c.op == NkPaintOp::PopClip)
						break;
					if (c.op == NkPaintOp::PushClip || bandePleineLargeur(c))
						continue;
					if (c.x < gridX - 0.01f && c.y >= bandeTop - 0.01f)
						++grilleCol;
				}
			};

			uint32 colonneSurGrille = 0, grilleSurColonne = 0;
			float32 pireColonne = 0.f;
			NkString coupables;
			if (gridX >= 0.f)
				compter(g, iClip, colonneSurGrille, grilleSurColonne, pireColonne, &coupables);

			snprintf(buf, sizeof(buf),
					 "bande jugee : x<%.1f et y>=%.1f ; colonne->grille : %u (debord max %.1f px) ; "
					 "grille->colonne : %u",
					 gridX, bandeTop, colonneSurGrille, pireColonne, grilleSurColonne);
			check("34d. la colonne d'arborescence et la grille ne se chevauchent NI dans un sens NI dans l'autre",
				  colonneSurGrille == 0 && grilleSurColonne == 0,
				  (colonneSurGrille == 0 && grilleSurColonne == 0) ? buf : coupables.Data());

			// 34e. CONTROLE POSITIF -- SANS LUI, 34d NE VAUT RIEN.
			//      Un banc borne deux fois de suite finit par ne plus rien voir :
			//      j'ai elargi la borne pour supprimer deux faux positifs, il faut
			//      donc prouver qu'il reste capable de mordre. On injecte UNE
			//      commande volontairement fautive -- posee dans la colonne, a la
			//      bonne hauteur, traversant la frontiere -- et le MEME detecteur
			//      doit la compter, avec le bon debordement.
			// ⚠️ L'INTRUS S'INSERE A LA FRONTIERE, IL NE S'AJOUTE PLUS A LA FIN —
			//    et la premiere version de cette correction a ete PRISE PAR SON
			//    PROPRE CONTROLE : etendre `fin` a la taille du flux (l'ancienne
			//    methode) faisait entrer la BARRE D'ETAT du mixte dans le cote
			//    « colonne » — 2 detections, debord 377 px, mesure du 30/08. Le
			//    detecteur reel juge [0, iClip) ; l'intrus doit donc etre juge
			//    DANS cette fenetre, pas dans une fenetre elargie qui n'existe
			//    nulle part ailleurs.
			NkRecordingPaint faute;
			for (uint32 i = 0; i < (uint32)g.cmds.Size(); ++i) {
				if (i == iClip) {
					NkPaintCmd intrus;
					intrus.op = NkPaintOp::Fill;
					intrus.x = gridX - 40.f; // commence DANS la colonne
					intrus.y = bandeTop + 10.f;
					intrus.w = 90.f;		 // et finit 50 px DANS la grille
					intrus.h = 20.f;
					faute.cmds.PushBack(intrus);
				}
				faute.cmds.PushBack(g.cmds[i]);
			}
			uint32 fCol = 0, fGrille = 0;
			float32 fPire = 0.f;
			// La fenetre jugee est la MEME que pour le flux reel, decalee du seul
			// intrus insere : c'est la condition pour que 34e prouve le detecteur
			// employe, pas un detecteur elargi pour l'occasion.
			compter(faute, iClip + 1, fCol, fGrille, fPire, nullptr);
			snprintf(buf, sizeof(buf), "intrus injecte -> %u detecte(s), debord mesure %.1f px (attendu 50.0)",
					 fCol, fPire);
			check("34e. CONTROLE POSITIF : le MEME detecteur voit un chevauchement injecte", fCol == 1 && fPire > 49.9f && fPire < 50.1f, buf);

			rep.Append("\n--- geometrie du navigateur de contenu, mesuree ---\n  ");
			rep.Append(buf);
			rep.Append("\n  ");
			snprintf(buf, sizeof(buf), "%u commande(s) au total, %u hors du rectangle donne",
					 (uint32)g.cmds.Size(), dehors);
			rep.Append(buf);
			rep.Append("\n");
		}

		// -- 35. LA CONFIGURATION : LUE PAR DEFAUT, ET PAR LA MEME FONCTION ---
		// Directive de Rodolf (18/08) : le reglage se change depuis l'interface,
		// et **la config est lue par defaut au demarrage**. Quatre sources, du plus
		// local au plus durable : `--gfx` > `NK_GFX_API` > **fichier** > detection.
		//
		// ⚠️ CETTE FAMILLE EXISTE POUR UNE RAISON PRECISE, ET C'EST LA MIENNE :
		//    « la sonde lit la meme configuration que l'application ». Si `--probe`
		//    gardait sa propre resolution pendant que `main` lit un fichier, on
		//    aurait deux sources de verite pour une meme chose — le defaut exact
		//    qui a laisse vivre le magenta pendant 72 essais verts. Ici, `main` et
		//    la sonde appellent **la meme `NkGfxResolve`**, et le contenu du
		//    fichier lui est **passe** : la sonde exerce donc la resolution reelle
		//    sur des contenus qu'aucun fichier du disque ne contiendrait tous.
		{
			// 35a. L'ANALYSE D'UN FICHIER, sur les cas tordus ecrits d'avance.
			//      ⚠️ « cle absente » et « cle sans valeur » doivent rendre la MEME
			//      chose : une cle posee et vide ne doit pas ecraser la detection
			//      par une chaine vide. C'est la meme regle que pour une variable
			//      d'environnement vide, deja acquise en 31.
			static const char *kCfg = "# le backend graphique de NkUIDesign\n"
									  "gfx = opengl   \n"
									  "vide =\n"
									  "# gfx = vulkan   <- commente, ne doit PAS gagner\n"
									  "autre = 3\n";
			char v[32];
			const bool lu = NkGfxConfigValue(kCfg, "gfx", v, sizeof(v));
			char vVide[32], vAbsente[32];
			const bool luVide = NkGfxConfigValue(kCfg, "vide", vVide, sizeof(vVide));
			const bool luAbsente = NkGfxConfigValue(kCfg, "pas_la", vAbsente, sizeof(vAbsente));
			snprintf(buf, sizeof(buf), "gfx='%s' (%d), vide=%d, absente=%d", lu ? v : "", (int)lu,
					 (int)luVide, (int)luAbsente);
			check("35a. la cle se lit, les blancs de fin tombent, un commentaire ne gagne pas, "
				  "vide == absente",
				  lu && SameText(v, "opengl") && !luVide && !luAbsente, buf);

			// 35b. LA PRIORITE, DANS LES DEUX SENS. Verifier seulement que
			//      l'environnement gagne sur le fichier passerait aussi si le
			//      fichier etait purement ignore — c'est la lecon de 31g, appliquee
			//      a la source neuve.
			static const char *kArgDx12[] = {"--gfx=dx12"};
			const NkGfxChoice cCfgSeul = NkGfxResolve("vulkan", nullptr, nullptr, 0);
			const NkGfxChoice cCfgEtEnv = NkGfxResolve("vulkan", "opengl", nullptr, 0);
			const NkGfxChoice cTout = NkGfxResolve("vulkan", "opengl", kArgDx12, 1);
			snprintf(buf, sizeof(buf), "fichier seul -> %s (%s) ; +env -> %s ; +ligne -> %s",
					 cCfgSeul.effective, NkGfxSourceName(cCfgSeul.source), cCfgEtEnv.effective,
					 cTout.effective);
			check("35b. le fichier bat la detection, l'environnement bat le fichier, la ligne bat tout",
				  SameText(cCfgSeul.effective, "vulkan") &&
					  cCfgSeul.source == NkGfxSource::FichierDeConfig &&
					  SameText(cCfgEtEnv.effective, "opengl") && SameText(cTout.effective, "dx12"),
				  buf);

			// 35c. UN REPLI QUI NE VERROUILLE PAS DEHORS. Une config qui nomme un
			//      backend indisponible doit **demarrer quand meme** : sinon
			//      l'utilisateur ne peut plus atteindre les Preferences pour
			//      corriger le reglage qui l'empeche de demarrer.
			const NkGfxChoice cCfgFaux = NkGfxResolve("metal", nullptr, nullptr, 0);
			snprintf(buf, sizeof(buf), "config 'metal' -> supported=%d, repli=%d, refuse='%s', retenu='%s'",
					 (int)cCfgFaux.supported, (int)cCfgFaux.fellBack, cCfgFaux.refused,
					 cCfgFaux.effective);
			check("35c. une config indisponible DEMARRE sur le repli, en nommant ce qu'elle refusait",
				  cCfgFaux.supported && cCfgFaux.fellBack && SameText(cCfgFaux.refused, "metal") &&
					  *cCfgFaux.effective && !SameText(cCfgFaux.effective, "auto"),
				  buf);

			// 35d. ET LA LIGNE DE COMMANDE, ELLE, REFUSE TOUJOURS. C'est l'autre
			//      moitie : appliquer le repli a `--gfx` ferait mesurer sur autre
			//      chose que ce qui a ete demande — ce que la regle 3 interdit.
			//      Sans cette ligne, 35c pourrait passer avec un repli applique
			//      partout, et personne ne verrait la difference.
			static const char *kArgMetal[] = {"--gfx=metal"};
			const NkGfxChoice cLigneFausse = NkGfxResolve(nullptr, nullptr, kArgMetal, 1);
			snprintf(buf, sizeof(buf), "--gfx=metal -> supported=%d, repli=%d",
					 (int)cLigneFausse.supported, (int)cLigneFausse.fellBack);
			check("35d. `--gfx` indisponible REFUSE le lancement (deux sources, deux conduites)",
				  !cLigneFausse.supported && !cLigneFausse.fellBack, buf);

			// 35e. LE REPLI SE CRIE, ET EN TETE DE LIGNE. Une annonce posee apres
			//      quatre champs se lit apres coup, quand on cherche deja pourquoi
			//      ca ne marche pas.
			const NkString ligneRepli = NkGfxJournalLine(cCfgFaux);
			const bool enTete = Contains(ligneRepli.Data(), "!! LA CONFIGURATION DEMANDE 'metal'");
			const bool ditQuOnNeReecritPas = Contains(ligneRepli.Data(), "n'a PAS ete reecrite");
			check("35e. le journal CRIE le repli, en tete, et dit que la config n'a pas ete reecrite",
				  enTete && ditQuOnNeReecritPas, ligneRepli.Data());

			// 35f. LA LIGNE NOMME LAQUELLE DES QUATRE SOURCES A DECIDE.
			//      Sans ca, un utilisateur qui regle une valeur et en voit une
			//      autre se lancer n'a aucun moyen de comprendre.
			const NkString ligneCfg = NkGfxJournalLine(cCfgSeul);
			check("35f. le journal nomme la SOURCE qui a decide (les quatre sont distinguables)",
				  Contains(ligneCfg.Data(), "fichier de configuration") &&
					  !Contains(ligneCfg.Data(), "{"),
				  ligneCfg.Data());

			rep.Append("\n--- la ligne de journal d'un repli de configuration ---\n");
			rep.Append(ligneRepli);
			rep.Append('\n');
		}


		// -- 36. LE CHEVRON : EST-CE QU'ON A LE PROBLEME ? -------------------
		// ⚠️ CETTE FAMILLE COMMENCE PAR LA QUESTION QU'ON POSE AVANT « comment
		//    fait-on ca ». On m'a transmis, et j'ai relaye moi-meme : *« pas de
		//    chevron = un arbre qui ne se plie pas »*. C'est une deduction faite
		//    en regardant une capture — la troisieme de la journee, apres deux qui
		//    se sont revelees fausses. Avant de construire un systeme d'icones,
		//    on mesure si le geste marche.
		//
		//    LA LECTURE DU CODE DIT DEJA NON : dans `NkTreeViewDraw.cpp`,
		//    `hitChevron` est **geometrique** (`chev.Contains(mouseX, mouseY)`),
		//    il ne depend d'aucune poignee d'icone. Le dessin de l'icone et la
		//    zone cliquable sont deux choses separees. Mais une lecture n'est pas
		//    une mesure : on exerce le clic.
		{
			NkTreeViewModel t;
			ProbeFillDemoTree(t);
			NkTreeViewStyle st;
			st.values = nullptr; // les defauts declares suffisent

			// 36a. CONDITION D'EXISTENCE : il faut un noeud QUI A des enfants, et
			//      qui soit ouvert au depart. Sans ca, « l'etat a change » ne
			//      voudrait rien dire.
			const nkentseu::nk_uint64 racine = t.nodes[0].id; // « Scene »
			const bool aDesEnfants = t.nodes.Size() > 1 && t.nodes[1].parent == 0;
			const bool ouvertAuDepart = t.IsOpen(racine, true);
			snprintf(buf, sizeof(buf), "%u noeuds, racine a des enfants=%d, ouverte=%d",
					 (uint32)t.nodes.Size(), (int)aDesEnfants, (int)ouvertAuDepart);
			check("36a. l'arbre d'essai a un noeud pliable, ouvert au depart", aDesEnfants && ouvertAuDepart,
				  buf);

			// 36b. LE CLIC DANS LA ZONE DU CHEVRON PLIE -- SANS AUCUNE ICONE.
			//      Les poignees de `st.icons` valent toutes 0 : rien n'est peint.
			//      Si l'etat bascule quand meme, le mecanisme est INTACT et le
			//      defaut est purement visuel.
			//      La position est calculee depuis les METRIQUES DECLAREES, pas
			//      ecrite en dur : `row_pad` puis la moitie de `chevron_w`.
			const NkComponentDecl &dTree = nkentseu::editorkit::NkTreeViewDecl();
			const float32 rowPad = dTree.Metric("row_pad");
			const float32 chevW = dTree.Metric("chevron_w");
			const float32 headerH = dTree.Metric("header_h") + dTree.Metric("search_h");
			const float32 rowH = dTree.Metric("row_h");
			NkComponentInput clic;
			clic.mouseX = rowPad + chevW * 0.5f;   // au milieu du chevron
			clic.mouseY = headerH + rowH * 0.5f;   // au milieu de la 1re ligne
			clic.mouseDown = true;
			clic.mousePressed = true;
			NkRecordingPaint rec;
			nkentseu::editorkit::NkTreeViewHooks h;
			const NkTreeViewResult res =
				nkentseu::editorkit::NkDrawTreeView(rec, clic, {0.f, 0.f, 320.f, 400.f}, t, st, h);
			const bool plieMaintenant = !t.IsOpen(racine, true);
			snprintf(buf, sizeof(buf),
					 "clic a (%.1f, %.1f) ; openChanged=%d ; racine ouverte apres = %d",
					 clic.mouseX, clic.mouseY, (int)res.openChanged, (int)t.IsOpen(racine, true));
			check("36b. le clic sur la zone du chevron PLIE, alors qu'AUCUNE icone n'est dessinee",
				  res.openChanged && plieMaintenant, buf);

			// 36c. ET IL DEPLIE AU CLIC SUIVANT. Un mecanisme qui ne ferait que
			//      plier passerait 36b et serait quand meme casse.
			NkRecordingPaint rec2;
			const NkTreeViewResult res2 =
				nkentseu::editorkit::NkDrawTreeView(rec2, clic, {0.f, 0.f, 320.f, 400.f}, t, st, h);
			check("36c. et il DEPLIE au clic suivant (sinon le pliage serait a sens unique)",
				  res2.openChanged && t.IsOpen(racine, true), "");

			// 36d. LE DEFAUT EST BIEN VISUEL : le composant DEMANDE une icone
			//      (il emet la commande) et la poignee vaut 0, donc rien n'est
			//      peint. C'est ce qui separe « le composant ne dessine pas » de
			//      « l'hote ne lui donne rien ».
			uint32 iconesDemandees = 0, iconesVides = 0;
			for (uint32 i = 0; i < (uint32)rec.cmds.Size(); ++i)
				if (rec.cmds[i].op == NkPaintOp::Icon) {
					++iconesDemandees;
					if (rec.cmds[i].icon == 0)
						++iconesVides;
				}
			snprintf(buf, sizeof(buf), "%u commande(s) Icon emise(s), dont %u a poignee NULLE",
					 iconesDemandees, iconesVides);
			check("36d. le composant DEMANDE ses icones ; c'est l'HOTE qui n'en fournit aucune",
				  iconesDemandees > 0 && iconesVides == iconesDemandees, buf);

			// 36e. L'HOTE FOURNIT DESORMAIS SES POIGNEES -- non nulles ET
			//      DISTINCTES. Deux poignees egales peindraient le meme signe pour
			//      « ouvert » et « ferme » : le chevron existerait et ne dirait
			//      rien, ce qui est le defaut d'origine sous une autre forme.
			const NkTreeViewIcons mien = NkDesignTreeIcons();
			const uint16 six[6] = {mien.chevronClosed, mien.chevronOpen, mien.eyeOpen,
								   mien.eyeClosed,	   mien.lockOpen,	 mien.lockClosed};
			bool toutesPosees = true, toutesDistinctes = true;
			for (uint32 a = 0; a < 6; ++a) {
				if (six[a] == 0)
					toutesPosees = false;
				for (uint32 b2 = a + 1; b2 < 6; ++b2)
					if (six[a] == six[b2])
						toutesDistinctes = false;
			}
			check("36e. l'hote pose ses SIX poignees, toutes non nulles et toutes distinctes",
				  toutesPosees && toutesDistinctes, "");

			// 36f. ⚠️ LE SIGNE CHANGE AVEC L'ETAT -- c'est la seule chose qui
			//      manquait vraiment. Un chevron qui ne changerait pas au pliage
			//      serait un decor, pas un indicateur : il passerait 36e et
			//      laisserait l'utilisateur exactement aussi aveugle.
			//      Mesure SANS GPU : on lit la poignee emise, jamais des pixels.
			NkTreeViewStyle avecIcones = st;
			avecIcones.icons = mien;
			NkTreeViewModel t2;
			ProbeFillDemoTree(t2);
			const NkComponentInput repos;
			NkRecordingPaint ouvert;
			nkentseu::editorkit::NkDrawTreeView(ouvert, repos, {0.f, 0.f, 320.f, 400.f}, t2,
												avecIcones, h);
			t2.SetOpen(t2.nodes[0].id, false, true); // on replie la racine
			NkRecordingPaint ferme;
			nkentseu::editorkit::NkDrawTreeView(ferme, repos, {0.f, 0.f, 320.f, 400.f}, t2,
												avecIcones, h);
			uint16 poigneeOuvert = 0, poigneeFerme = 0;
			for (uint32 i = 0; i < (uint32)ouvert.cmds.Size(); ++i)
				if (ouvert.cmds[i].op == NkPaintOp::Icon && ouvert.cmds[i].icon == mien.chevronOpen) {
					poigneeOuvert = ouvert.cmds[i].icon;
					break;
				}
			for (uint32 i = 0; i < (uint32)ferme.cmds.Size(); ++i)
				if (ferme.cmds[i].op == NkPaintOp::Icon && ferme.cmds[i].icon == mien.chevronClosed) {
					poigneeFerme = ferme.cmds[i].icon;
					break;
				}
			snprintf(buf, sizeof(buf), "deplie -> poignee %u ; replie -> poignee %u (attendu %u / %u)",
					 (uint32)poigneeOuvert, (uint32)poigneeFerme, (uint32)mien.chevronOpen,
					 (uint32)mien.chevronClosed);
			check("36f. le chevron CHANGE de signe entre deplie et replie",
				  poigneeOuvert == mien.chevronOpen && poigneeFerme == mien.chevronClosed &&
					  poigneeOuvert != poigneeFerme,
				  buf);

			rep.Append("\n--- le chevron : mecanisme ou dessin ? ---\n  ");
			rep.Append(buf);
			rep.Append("\n  VERDICT : le pliage FONCTIONNE sans icone. Le manque est le SIGNE,\n"
					   "  pas le geste -- on ne voit pas ou cliquer, mais cliquer marche.\n");
		}


		// -- 37. ECRIRE LA CONFIGURATION SANS DETRUIRE CE QU'ON N'A PAS ECRIT --
		// La chaine se referme : **Preferences -> fichier -> demarrage**. Cette
		// famille couvre le maillon neuf, et surtout ce qu'il ne doit JAMAIS
		// faire. La production du texte est PURE (`NkGfxConfigSet` prend l'ancien
		// texte, rend le nouveau) : la sonde exerce donc des contenus qu'aucun
		// fichier reel ne contiendrait tous, sans ecrire un octet.
		{
			// 37a. LE REMPLACEMENT PRESERVE TOUT LE RESTE. C'est l'exigence n. 1 :
			//      le fichier a pu etre edite a la main. Un `Enregistrer` qui
			//      REGENERE le fichier effacerait le travail de quelqu'un sans
			//      qu'un seul message le signale.
			static const char *kAvant = "# mon fichier a moi\n"
										"gfx = opengl\n"
										"autre = 42\n"
										"# une note en bas\n";
			NkString apres;
			NkGfxConfigSet(kAvant, "gfx", "vulkan", apres);
			const bool garde = Contains(apres.Data(), "# mon fichier a moi") &&
							   Contains(apres.Data(), "autre = 42") &&
							   Contains(apres.Data(), "# une note en bas");
			const bool remplace = Contains(apres.Data(), "gfx = vulkan") &&
								  !Contains(apres.Data(), "gfx = opengl");
			check("37a. la cle est remplacee, et TOUT le reste survit (commentaires, autres cles)",
				  garde && remplace, apres.Data());

			// 37b. CLE ABSENTE -> AJOUTEE, sans rien perdre. Sans ce cas, le
			//      premier enregistrement sur un fichier neuf ne ferait rien, et
			//      le bouton « Enregistrer » mentirait en silence.
			NkString ajout;
			NkGfxConfigSet("# rien que des notes\nautre = 1\n", "gfx", "dx12", ajout);
			check("37b. une cle absente est AJOUTEE, le reste est conserve",
				  Contains(ajout.Data(), "gfx = dx12") && Contains(ajout.Data(), "autre = 1") &&
					  Contains(ajout.Data(), "# rien que des notes"),
				  ajout.Data());

			// 37c. UNE CLE COMMENTEE N'EST PAS LA CLE. `# gfx = vulkan` doit
			//      rester un commentaire : le remplacer reviendrait a decommenter
			//      une ligne que quelqu'un avait volontairement desactivee.
			NkString comm;
			NkGfxConfigSet("# gfx = vulkan\n", "gfx", "opengl", comm);
			check("37c. une cle COMMENTEE reste commentee, et la vraie cle est ajoutee a cote",
				  Contains(comm.Data(), "# gfx = vulkan") && Contains(comm.Data(), "gfx = opengl"),
				  comm.Data());

			// 37d. ⚠️ L'ALLER-RETOUR PASSE PAR LA MEME LECTURE QUE L'APPLICATION.
			//      C'est la condition qui empeche de recreer deux verites en
			//      ajoutant l'ecriture : ce qu'on ECRIT doit etre exactement ce que
			//      `NkGfxConfigValue` RELIT, et ce que `NkGfxResolve` en fait.
			char relu[32];
			const bool lu = NkGfxConfigValue(apres.Data(), "gfx", relu, sizeof(relu));
			const NkGfxChoice apresEcriture = NkGfxResolve(lu ? relu : nullptr, nullptr, nullptr, 0);
			snprintf(buf, sizeof(buf), "ecrit 'vulkan' -> relu '%s' -> resolu '%s' (source : %s)",
					 lu ? relu : "", apresEcriture.effective, NkGfxSourceName(apresEcriture.source));
			check("37d. ALLER-RETOUR : ce qui est ecrit est relu par la MEME lecture, et resolu pareil",
				  lu && SameText(relu, "vulkan") && SameText(apresEcriture.effective, "vulkan") &&
					  apresEcriture.source == NkGfxSource::FichierDeConfig,
				  buf);

			// 37e. LES TROIS ETATS D'UN FICHIER SONT DISTINGUES. « absent » et
			//      « present mais inexploitable » se comportaient pareil — un repli
			//      MUET. Ils doivent se separer, sinon l'utilisateur qui a ecrit
			//      quelque chose de faux ne l'apprend jamais.
			char v[32];
			const NkGfxConfigState stAbsent = NkGfxConfigClassify(false, "", v, sizeof(v));
			const NkGfxConfigState stLu = NkGfxConfigClassify(true, "gfx = dx11\n", v, sizeof(v));
			const NkGfxConfigState stCasse =
				NkGfxConfigClassify(true, "\x01\x02 ceci n'est pas une config\n", v, sizeof(v));
			check("37e. absent / lu / present-mais-illisible sont TROIS etats distincts",
				  stAbsent == NkGfxConfigState::Absent && stLu == NkGfxConfigState::CleLue &&
					  stCasse == NkGfxConfigState::PresentSansCle,
				  "");

			// 37f. ET L'ETAT « ILLISIBLE » PORTE UN MESSAGE, les autres non.
			//      Un etat qu'on distingue sans le dire ne sert a rien.
			const char *mCasse = NkGfxConfigStateMessage(NkGfxConfigState::PresentSansCle);
			const char *mLu = NkGfxConfigStateMessage(NkGfxConfigState::CleLue);
			check("37f. l'etat illisible se DIT (et promet de ne rien ecraser), les autres se taisent",
				  mCasse && *mCasse && Contains(mCasse, "n'a PAS ete modifie") && mLu && !*mLu, "");

			// 37g. ⚠️ L'ECRITURE EST ATOMIQUE, ET C'EST LE SEUL ESSAI QUI TOUCHE LE
			//      DISQUE. Deux choses a prouver : le contenu arrive, et **aucun
			//      temporaire ne survit**. Un `.tmp` oublie a cote du fichier est
			//      le signe d'un renommage qui n'a pas eu lieu — donc d'une
			//      ecriture qui a pu laisser le fichier a moitie ecrit.
			static const char *kEssai = "nkuidesign_essai.cfg";
			NkString tmpPath(kEssai);
			tmpPath.Append(".tmp");
			const bool ecrit = NkGfxConfigSetKey(kEssai, "gfx", "software");
			const NkString relire =
				nkentseu::NkFile::Exists(kEssai) ? nkentseu::NkFile::ReadAllText(kEssai) : NkString("");
			const bool tmpParti = !nkentseu::NkFile::Exists(tmpPath.Data());
			snprintf(buf, sizeof(buf), "ecrit=%d, contenu='%s', temporaire restant=%d", (int)ecrit,
					 relire.Data(), (int)!tmpParti);
			check("37g. l'ecriture atterrit sur le disque ET ne laisse aucun temporaire derriere",
				  ecrit && Contains(relire.Data(), "gfx = software") && tmpParti, buf);

			// 37h. UN SECOND ENREGISTREMENT NE DUPLIQUE PAS LA CLE. Sans ce
			//      controle, chaque `Enregistrer` ajouterait une ligne, et le
			//      fichier finirait par contenir dix `gfx` dont seule la premiere
			//      compterait -- un defaut qui ne se voit qu'apres coup.
			NkGfxConfigSetKey(kEssai, "gfx", "opengl");
			const NkString deux =
				nkentseu::NkFile::Exists(kEssai) ? nkentseu::NkFile::ReadAllText(kEssai) : NkString("");
			uint32 occurrences = 0;
			for (const char *c = deux.Data(); c && *c; ++c)
				if (c[0] == 'g' && c[1] == 'f' && c[2] == 'x')
					++occurrences;
			snprintf(buf, sizeof(buf), "%u occurrence(s) de 'gfx' apres deux enregistrements",
					 occurrences);
			check("37h. deux enregistrements successifs laissent UNE seule cle", occurrences == 1, buf);
			nkentseu::NkFile::Delete(kEssai);
		}

		rep.Append("\n--- les lignes de journal du backend, telles quelles ---\n");
		rep.Append(lineVk);
		rep.Append('\n');
		rep.Append(lineMetal);
		rep.Append('\n');

		rep.Append("\n--- le bloc .nkgui produit, tel quel ---\n");
		rep.Append(ctrl);

		rep.Append("\n--- le document d'essai produit, tel quel ---\n");
		rep.Append(dragText);

		rep.Append("\n--- le fichier d'ecarts produit, tel quel ---\n");
		rep.Append(text);

		// ════════════════════════════════════════════════════════════════
		//  38. LA TOILE -- un noeud POSE se dessine ou on l'a pose
		// ════════════════════════════════════════════════════════════════
		//
		//  Etape 1 du chemin vers la specification (document 3). Le modele
		//  porte desormais les DEUX natures :
		//    - sous un parent `Column`/`Row`/`Grid`/`Anchor`, la position se
		//      CALCULE -- c'est le modele declaratif du 18 aout, intact ;
		//    - sous un parent `Free`, la position se LIT.
		//
		//  ⚠️ CE N'EST PAS « AJOUTER X/Y A TOUT ». Le champ ne vaut que sous
		//     un parent `Free`, exactement comme `anchorEdges` ne vaut que
		//     sous un parent `Anchor`. Donner des coordonnees a tous les
		//     noeuds aurait debloque la toile en une heure et detruit ce que
		//     l'apercu sait rendre depuis dix jours.
		{
			rep.Append("\n-- 38. LA TOILE : un noeud pose se dessine ou on l'a pose --\n");
			// ⚠️ `NewDocument` D ABORD : `AddChild(-1, ...)` rend -1 -- il EXIGE un
			//    parent valide, il ne cree pas de racine. Mon premier essai indexait
			//    `nodes[(uint32)-1]` et le banc est mort sur une assertion de
			//    `NkVector`. Le garde a fait exactement son travail.
			NkUIDocument d;
			d.NewDocument("Toile", NkAuthor::Humain);
			const int32 page = 0;
			d.nodes[(uint32)page].label = NkString("Page");
			d.nodes[(uint32)page].layout.kind = NkLayoutKind::Free;
			d.nodes[(uint32)page].width.mode = NkSizeMode::Fixed;
			d.nodes[(uint32)page].width.value = 1280.f;
			d.nodes[(uint32)page].height.mode = NkSizeMode::Fixed;
			d.nodes[(uint32)page].height.value = 720.f;

			const int32 forme = d.AddChild(page, "", NkAuthor::Humain);
			d.nodes[(uint32)forme].label = NkString("Carte");
			d.nodes[(uint32)forme].posX = 95.f;
			d.nodes[(uint32)forme].posY = 228.f;
			d.nodes[(uint32)forme].width.mode = NkSizeMode::Fixed;
			d.nodes[(uint32)forme].width.value = 320.f;
			d.nodes[(uint32)forme].height.mode = NkSizeMode::Fixed;
			d.nodes[(uint32)forme].height.value = 180.f;

			NkPaintRect surface;
			surface.x = 0.f;
			surface.y = 0.f;
			surface.w = 1280.f;
			surface.h = 720.f;
			NkLayoutResult lay;
			NkComputeLayout(d, surface, lay);
			const NkPaintRect &rp = lay.rects[(uint32)page];
			const NkPaintRect &rf = lay.rects[(uint32)forme];

			snprintf(buf, sizeof(buf), "page (%.1f,%.1f) forme (%.1f,%.1f %.0fx%.0f)",
					 rp.x, rp.y, rf.x, rf.y, rf.w, rf.h);
			check("38. une forme posee a (95,228) se dessine a (95,228)",
				  rf.x == rp.x + 95.f && rf.y == rp.y + 228.f, buf);
			check("38b. et elle garde la taille qu'elle declare (320x180) -- la toile "
				  "ne fabrique pas un second systeme de tailles",
				  rf.w == 320.f && rf.h == 180.f, buf);

			// ⚠️ 38c. LE CONTROLE NEGATIF, REPRIS DE `NKGuiDrawTest` : sans lui,
			//     un solveur qui poserait TOUT a (95,228) -- ou qui ignorerait la
			//     position et laisserait le premier enfant en haut a gauche --
			//     passerait 38. On change UNE coordonnee et le rectangle doit
			//     bouger d'autant, exactement.
			d.nodes[(uint32)forme].posX = 495.f;
			NkLayoutResult lay2;
			NkComputeLayout(d, surface, lay2);
			const NkPaintRect &rf2 = lay2.rects[(uint32)forme];
			snprintf(buf, sizeof(buf), "x passe de %.1f a %.1f (attendu +400)", rf.x, rf2.x);
			check("38c. CONTROLE NEGATIF : deplacer la forme de +400 en X deplace son "
				  "rectangle de +400 -- le solveur LIT la position, il ne la devine pas",
				  rf2.x == rf.x + 400.f && rf2.y == rf.y, buf);
			d.nodes[(uint32)forme].posX = 95.f;

			// ⚠️ 38d. LA NON-REGRESSION QUI COMPTE : le meme document, son parent
			//     remis en `Column`, doit IGNORER la position et replacer la forme
			//     par le calcul. C'est ce qui prouve que les deux natures
			//     cohabitent au lieu de se remplacer -- et que le modele
			//     declaratif du 18 aout est intact.
			d.nodes[(uint32)page].layout.kind = NkLayoutKind::Column;
			NkLayoutResult lay3;
			NkComputeLayout(d, surface, lay3);
			const NkPaintRect &rf3 = lay3.rects[(uint32)forme];
			snprintf(buf, sizeof(buf), "en Column : (%.1f,%.1f) -- la position posee est ignoree",
					 rf3.x, rf3.y);
			check("38d. NON-REGRESSION : sous un parent `Column`, la position posee est "
				  "IGNOREE et le calcul reprend la main (les deux natures cohabitent)",
				  rf3.x != rp.x + 95.f || rf3.y != rp.y + 228.f, buf);

			// 38e. Aller-retour du champ : ecrit seulement s'il existe, relu juste.
			d.nodes[(uint32)page].layout.kind = NkLayoutKind::Free;
			NkString texte;
			d.Save(texte);
			NkUIDocument relu;
			const bool ok = relu.Load(texte.Data(), nullptr);
			check("38e. la position fait l'aller-retour par le fichier",
				  ok && relu.NodeCount() == d.NodeCount()
					  && relu.nodes[(uint32)forme].posX == 95.f
					  && relu.nodes[(uint32)forme].posY == 228.f,
				  "");

			// ⚠️ 38f. UN DOCUMENT DECLARATIF N'ECRIT AUCUNE LIGNE `position`.
			//     C'est ce qui garantit que le document du 18 aout se
			//     reenregistre a l'identique : le champ neuf ne s'invite pas
			//     dans les fichiers qui ne s'en servent pas.
			NkUIDocument decl;
			decl.NewDocument("Declaratif", NkAuthor::Humain);
			const int32 rac = 0;
			decl.nodes[(uint32)rac].layout.kind = NkLayoutKind::Column;
			decl.AddChild(rac, "", NkAuthor::Humain);
			NkString td;
			decl.Save(td);
			check("38f. un document DECLARATIF n'ecrit aucune ligne `position`",
				  td.Find("  position = ") == NkString::npos, "");
			// ⚠️ ON CHERCHE LA CLE, PAS LE MOT. Premier essai : `Find("position")`
			//    -- rouge, parce que l en-tete du fichier CONTIENT le mot dans sa
			//    prose (« La position se CALCULE »). Un controle qui cherche un mot
			//    dans un fichier qui parle de lui-meme trouve toujours quelque chose.
		}

		// ════════════════════════════════════════════════════════════════
		//  39. LA VUE DE TOILE -- deux espaces, une seule traduction
		// ════════════════════════════════════════════════════════════════
		//
		//  ⚠️ LE PIEGE DE CETTE FAMILLE, ET IL EST INVISIBLE : un aller-retour
		//     `ToDoc(ToScreen(p)) == p` est **satisfait par l'IDENTITE**. Une vue
		//     qui ignorerait completement le zoom et le deplacement le passerait
		//     sans broncher -- meme forme que le temoin de bruit qui comparait
		//     deux sorties de la meme fonction, et que les deux `== npos` qui
		//     rendaient T5 vert sur un ecrivain mort.
		//
		//     D'ou : les attendus sont **calcules a la main**, et 39c verifie
		//     explicitement que la vue N'EST PAS l'identite.
		{
			rep.Append("\n-- 39. LA VUE DE TOILE : document <-> ecran --\n");
			NkCanvasView v;
			v.viewport.x = 300.f;
			v.viewport.y = 50.f;
			v.viewport.w = 800.f;
			v.viewport.h = 600.f;
			v.zoom = 2.5f;
			v.panX = 137.f;
			v.panY = -42.f;

			// A LA MAIN : 300 + 137 + 95*2.5 = 674.5 ; 50 - 42 + 228*2.5 = 578.0
			const float32 sx = v.ToScreenX(95.f);
			const float32 sy = v.ToScreenY(228.f);
			snprintf(buf, sizeof(buf), "(95,228) -> (%.2f,%.2f) attendu (674.50,578.00)", sx, sy);
			check("39. DOCUMENT -> ECRAN, sur des nombres ecrits a la main "
				  "(viewport 300/50, zoom 2.5, pan 137/-42)",
				  sx == 674.5f && sy == 578.f, buf);

			// La TAILLE subit l'echelle et PAS le deplacement : c'est une
			// longueur, pas une position. 320*2.5 = 800 ; 180*2.5 = 450.
			NkPaintRect d;
			d.x = 95.f;
			d.y = 228.f;
			d.w = 320.f;
			d.h = 180.f;
			const NkPaintRect s = v.ToScreen(d);
			snprintf(buf, sizeof(buf), "taille %.1fx%.1f attendu 800.0x450.0", s.w, s.h);
			check("39b. la TAILLE subit l'echelle et PAS le deplacement -- une "
				  "longueur n'est pas une position",
				  s.w == 800.f && s.h == 450.f, buf);

			// ⚠️ 39c. LE CONTROLE NEGATIF : sans lui, l'identite passe 39d.
			//     Deux points distants de 100 en document doivent etre distants
			//     de 250 a l'ecran au zoom 2.5. Une vue identite les laisserait
			//     a 100.
			const float32 ecart = v.ToScreenX(200.f) - v.ToScreenX(100.f);
			snprintf(buf, sizeof(buf), "ecart ecran %.1f pour 100 en document (attendu 250)", ecart);
			check("39c. CONTROLE NEGATIF : la vue n'est PAS l'identite -- 100 en "
				  "document font 250 a l'ecran au zoom 2.5",
				  ecart == 250.f && v.ToScreenX(0.f) != 0.f, buf);

			// 39d. L'aller-retour. ⚠️ Il est le PLUS FAIBLE de la famille et il
			//      ne vaut que colle a 39 et 39c : seul, l'identite le passe.
			const float32 back = v.ToDocX(v.ToScreenX(95.f));
			const float32 backY = v.ToDocY(v.ToScreenY(228.f));
			check("39d. l'aller-retour ecran -> document rend le point de depart "
				  "(faible seul : ancre par 39 et 39c)",
				  back == 95.f && backY == 228.f, "");

			// 39e. Une LONGUEUR ecran rendue en longueur document : l'echelle
			//      seule. 20 px d'ecran font 8 unites de document au zoom 2.5.
			snprintf(buf, sizeof(buf), "20 px ecran -> %.2f document (attendu 8.00)",
					 v.ToDocLength(20.f));
			check("39e. une LONGUEUR ecran se rend en document par l'echelle seule",
				  v.ToDocLength(20.f) == 8.f, buf);
		}

		// ── LE ZOOM AUTOUR DU CURSEUR ────────────────────────────────────
		//  Le defaut le plus courant de toute toile : le contenu « fuit » sous
		//  la souris parce qu'on a change l'echelle sans recaler le deplacement.
		//  Il ne se voit pas sur une capture fixe.
		{
			NkCanvasView v;
			v.viewport.x = 300.f;
			v.viewport.y = 50.f;
			v.zoom = 1.f;
			v.panX = 0.f;
			v.panY = 0.f;

			const float32 curseurX = 500.f, curseurY = 400.f;
			const float32 docAvantX = v.ToDocX(curseurX);
			const float32 docAvantY = v.ToDocY(curseurY);
			v.ZoomAt(2.f, curseurX, curseurY);
			const float32 apresX = v.ToScreenX(docAvantX);
			const float32 apresY = v.ToScreenY(docAvantY);

			snprintf(buf, sizeof(buf), "le point sous le curseur : (%.2f,%.2f) -> (%.2f,%.2f)",
					 curseurX, curseurY, apresX, apresY);
			check("39f. ZOOM AU CURSEUR : le point du document sous le curseur ne "
				  "bouge PAS d'un pixel",
				  apresX == curseurX && apresY == curseurY, buf);

			// ⚠️ 39g. LE CONTROLE NEGATIF DE 39f, ET IL EST INDISPENSABLE : une
			//     vue qui n'aurait PAS zoome laisserait AUSSI le point immobile.
			//     Il faut donc qu'un AUTRE point, lui, ait bien bouge -- et que
			//     l'echelle ait change.
			const float32 autre = v.ToScreenX(docAvantX + 100.f);
			snprintf(buf, sizeof(buf), "zoom %.2f ; un point a +100 est a %.1f (curseur %.1f)",
					 v.zoom, autre, curseurX);
			check("39g. CONTROLE NEGATIF : l'echelle a VRAIMENT change -- un point "
				  "voisin s'est ecarte de 200 px, pas de 100",
				  v.zoom == 2.f && (autre - curseurX) == 200.f, buf);

			// 39h. Les bornes : un zoom ne descend pas a zero (ToDoc deviendrait
			//      infini) et ne monte pas sans fin.
			NkCanvasView b;
			for (uint32 i = 0; i < 40; ++i)
				b.ZoomAt(0.5f, 100.f, 100.f);
			const float32 bas = b.zoom;
			for (uint32 i = 0; i < 60; ++i)
				b.ZoomAt(2.f, 100.f, 100.f);
			snprintf(buf, sizeof(buf), "plancher %.4f, plafond %.2f", bas, b.zoom);
			check("39h. le zoom est BORNE des deux cotes -- un zoom nul rendrait "
				  "`ToDoc` infini",
				  bas == NkCanvasView::MinZoom() && b.zoom == NkCanvasView::MaxZoom(), buf);

			// 39i. Le deplacement : N pixels ecran deplacent la vue de N pixels
			//      ecran, quel que soit le zoom. La main suit le curseur.
			NkCanvasView p;
			p.zoom = 4.f;
			const float32 avant = p.ToScreenX(10.f);
			p.PanBy(33.f, 0.f);
			snprintf(buf, sizeof(buf), "au zoom 4, un glissement de 33 px deplace de %.1f px",
					 p.ToScreenX(10.f) - avant);
			check("39i. le DEPLACEMENT est en pixels ecran, quel que soit le zoom "
				  "-- la main suit le curseur, pas le document",
				  (p.ToScreenX(10.f) - avant) == 33.f, buf);
		}

		// ════════════════════════════════════════════════════════════════
		//  40. LA SELECTION -- un etat, donc TROIS captures
		// ════════════════════════════════════════════════════════════════
		//
		//  ⚠️ UN ETAT NE SE MESURE PAS EN DEUX POINTS. Selectionner puis
		//     verifier qu'on a bien selectionne ne dit rien du retour : une
		//     structure qui ne se vide JAMAIS passe tous les controles d'ajout.
		//     D'ou trois captures a chaque fois -- avant, pendant, **et le
		//     retour au bit pres**.
		{
			rep.Append("\n-- 40. LA SELECTION : un etat, trois captures --\n");
			NkUIDocument d;
			d.NewDocument("Selection", NkAuthor::Humain);
			d.nodes[0].layout.kind = NkLayoutKind::Free;
			d.SetMetric("espacement", 0.f);
			d.SetMetric("marge", 0.f);
			const int32 a = d.AddChild(0, "", NkAuthor::Humain);
			const int32 b = d.AddChild(0, "", NkAuthor::Humain);
			const int32 c = d.AddChild(0, "", NkAuthor::Humain);
			const float32 xs[3] = {10.f, 200.f, 400.f};
			const int32 ids[3] = {a, b, c};
			for (uint32 i = 0; i < 3; ++i) {
				NkUINode &n = d.nodes[(uint32)ids[i]];
				n.posX = xs[i];
				n.posY = 50.f;
				n.width.mode = NkSizeMode::Fixed;
				n.width.value = 100.f;
				n.height.mode = NkSizeMode::Fixed;
				n.height.value = 80.f;
			}
			NkPaintRect surface;
			surface.x = 0.f;
			surface.y = 0.f;
			surface.w = 1000.f;
			surface.h = 600.f;
			NkLayoutResult lay;
			NkComputeLayout(d, surface, lay);

			// ── LES TROIS CAPTURES DU CLIC SIMPLE ────────────────────────
			NkSelection sel;
			const uint32 avant = sel.Count();
			sel.Set(a);
			const uint32 pendant = sel.Count();
			sel.Clear();
			const uint32 apres = sel.Count();
			snprintf(buf, sizeof(buf), "avant %u, pendant %u, apres %u", avant, pendant, apres);
			check("40. TROIS CAPTURES : vide -> un noeud -> vide. Le RETOUR est la "
				  "troisieme, et c'est celle qu'on oublie",
				  avant == 0 && pendant == 1 && apres == 0 && sel.Primary() == -1, buf);

			// ── CTRL+CLIC : AJOUTE, PUIS RETIRE (doc 3 §11.5) ────────────
			sel.Clear();
			sel.Toggle(a);
			sel.Toggle(b);
			const uint32 deux = sel.Count();
			const int32 principal = sel.Primary();
			sel.Toggle(a);
			snprintf(buf, sizeof(buf), "apres deux bascules : %u, principal %d ; retire a : %u",
					 deux, principal, sel.Count());
			check("40b. `Ctrl`+clic AJOUTE puis RETIRE -- et le retrait rend "
				  "exactement l'etat d'avant (1 element, et c'est `b`)",
				  deux == 2 && principal == a && sel.Count() == 1 && sel.Contains(b)
					  && !sel.Contains(a),
				  buf);

			// ⚠️ 40c. LE CONTROLE NEGATIF QU'ON OUBLIE : cliquer dans le VIDE
			//     deselectionne tout. Sans lui, une selection qui ne se vide
			//     jamais passe 40, 40b, 40d et 40e sans broncher.
			//
			//     ⚠️ IL A ECHOUE DES SA PREMIERE EXECUTION, POUR LA BONNE RAISON :
			//     `NkPickNode` rendait la RACINE, qui couvre toute la surface et
			//     repond donc a tous les clics. « Cliquer dans le vide » ne
			//     pouvait pas arriver. D ou `NkPickSelectable`.
			sel.Clear();
			sel.Set(a);
			sel.Add(b);
			const int32 rien = NkPickSelectable(d, lay, 900.f, 550.f);
			if (rien < 0)
				sel.Set(rien);
			snprintf(buf, sizeof(buf), "pointage dans le vide -> %d ; selection %u", rien,
					 sel.Count());
			check("40c. CONTROLE NEGATIF : cliquer dans le VIDE ne designe aucun "
				  "noeud ET vide la selection",
				  rien < 0 && sel.Count() == 0 && sel.Primary() == -1, buf);

			// ── LE RECTANGLE, EN ESPACE DOCUMENT ─────────────────────────
			// Il couvre les deux premieres formes (x 10..110 et 200..300) et pas
			// la troisieme (400..500).
			sel.Clear();
			NkPickInRect(d, lay, NkRectFromPoints(5.f, 40.f, 320.f, 140.f), sel);
			snprintf(buf, sizeof(buf), "%u prises (attendu 2 : a et b, pas c)", sel.Count());
			check("40d. LE RECTANGLE prend ce qu'il chevauche -- deux formes sur "
				  "trois, en espace DOCUMENT",
				  sel.Count() == 2 && sel.Contains(a) && sel.Contains(b) && !sel.Contains(c),
				  buf);

			// ⚠️ 40e. LE CONTROLE NEGATIF DU RECTANGLE : un rectangle pose la ou
			//     il n'y a rien ne prend RIEN. Sans lui, un `NkPickInRect` qui
			//     prendrait tout passerait 40d (qui ne compte que 2 sur 3... non :
			//     il le verrait). Mais un `NkPickInRect` qui prendrait tout ce qui
			//     est A GAUCHE du bord droit le passerait aussi. Celui-ci ferme la
			//     porte.
			sel.Clear();
			NkPickInRect(d, lay, NkRectFromPoints(600.f, 300.f, 900.f, 500.f), sel);
			check("40e. CONTROLE NEGATIF : un rectangle pose dans le vide ne prend "
				  "RIEN -- et la RACINE n'est jamais prise, sinon tout le serait",
				  sel.Count() == 0, "");

			// 40f. Tracer a l'envers (de la droite vers la gauche) prend autant.
			NkSelection s1, s2;
			NkPickInRect(d, lay, NkRectFromPoints(5.f, 40.f, 320.f, 140.f), s1);
			NkPickInRect(d, lay, NkRectFromPoints(320.f, 140.f, 5.f, 40.f), s2);
			snprintf(buf, sizeof(buf), "endroit %u, envers %u", s1.Count(), s2.Count());
			check("40f. tracer le rectangle A L'ENVERS prend exactement autant -- "
				  "un rectangle se normalise avant de servir",
				  s1.Count() == s2.Count() && s2.Contains(a) && s2.Contains(b), buf);

			// 40g. Un index perime ne survit pas. `RemoveSubtree` renumerote.
			NkSelection per;
			per.Set(a);
			per.Add(b);
			per.Add(999);
			const uint32 avecFaux = per.Count();
			per.DropInvalid(d);
			snprintf(buf, sizeof(buf), "%u -> %u apres nettoyage", avecFaux, per.Count());
			check("40g. un index qui n'existe plus est RETIRE -- une selection "
				  "perimee designerait un autre noeud apres renumerotation",
				  avecFaux == 3 && per.Count() == 2, buf);

			// ⚠️ 40h. LA SELECTION N'EST PAS DANS LE DOCUMENT, ET C'EST MESURE
			//     MECANIQUEMENT : on enregistre, on selectionne, on reenregistre.
			//     Les deux textes doivent etre IDENTIQUES. Si la selection etait
			//     serialisee, un simple clic salirait le fichier -- et deux
			//     personnes ouvrant le meme document heriteraient de la selection
			//     de l'autre.
			NkString t1, t2;
			d.Save(t1);
			NkSelection ailleurs;
			ailleurs.Set(a);
			ailleurs.Add(b);
			ailleurs.Toggle(c);
			d.Save(t2);
			check("40h. SELECTIONNER NE TOUCHE PAS LE DOCUMENT : le texte enregistre "
				  "est identique avant et apres (elle vit dans la VUE)",
				  t1.Compare(t2) == 0 && t1.Size() > 0, "");
		}

		// ════════════════════════════════════════════════════════════════
		//  41. UNE FORME POSEE SE DESSINE -- le controle qui manquait
		// ════════════════════════════════════════════════════════════════
		//
		//  ⚠️ C EST LE CONTROLE QUI AURAIT DU EXISTER AUX ETAPES 1, 2 ET 3.
		//     Elles ont mesure la POSITION (le solveur), la PROJECTION (la vue) et
		//     la SELECTION -- et aucune n a mesure que quelque chose ETAIT DESSINE.
		//     Resultat : trois etapes vertes a 126 controles, et un ecran noir.
		//
		//     Le defaut etait ecrit depuis le debut : `DrawFrame` est `{}`, trois
		//     parametres sans nom, et `Document.h` dit « Vide = un CADRE : le noeud
		//     n affiche rien ». Les formes de la toile etaient des cadres.
		//
		//  ⚠️ ET ON NE MESURE PAS DES PIXELS : on compte les COMMANDES DE DESSIN
		//     emises. C est ce que `NkRecordingPaint` sait faire sans GPU. « Le
		//     dump n est pas l ecran » reste vrai -- mais une fonction vide
		//     n emet rien, et ca, ca se compte.
		{
			rep.Append("\n-- 41. UNE FORME POSEE SE DESSINE --\n");
			NkUIDocument d;
			d.NewDocument("Toile", NkAuthor::Humain);
			d.nodes[0].layout.kind = NkLayoutKind::Free;
			d.SetMetric("espacement", 0.f);
			d.SetMetric("marge", 0.f);
			const int32 forme = d.AddChild(0, "", NkAuthor::Humain);
			d.nodes[(uint32)forme].label = NkString("Forme");
			d.nodes[(uint32)forme].posX = 95.f;
			d.nodes[(uint32)forme].posY = 30.f;
			d.nodes[(uint32)forme].width.mode = NkSizeMode::Fixed;
			d.nodes[(uint32)forme].width.value = 220.f;
			d.nodes[(uint32)forme].height.mode = NkSizeMode::Fixed;
			d.nodes[(uint32)forme].height.value = 90.f;

			NkPaintRect surface;
			surface.x = 0.f;
			surface.y = 0.f;
			surface.w = 800.f;
			surface.h = 600.f;
			NkRecordingPaint rec;
			RenderDocument(rec, d, surface);
			const uint32 posee = (uint32)rec.cmds.Size();

			snprintf(buf, sizeof(buf), "%u commande(s) de dessin", posee);
			check("41. une forme POSEE emet des commandes de dessin -- une fonction "
				  "vide n emet rien, et c est ce qui se comptait",
				  posee > 0, buf);

			// La geometrie emise doit tomber SUR la forme, pas ailleurs. Sans ca,
			// « ca dessine » pourrait vouloir dire « ca dessine n importe ou ».
			bool surLaForme = false;
			for (uint32 i = 0; i < posee; ++i) {
				const NkPaintCmd &c = rec.cmds[i];
				if (c.x >= 94.f && c.x <= 96.f && c.y >= 29.f && c.y <= 31.f && c.w >= 219.f
					&& c.w <= 221.f)
					surLaForme = true;
			}
			check("41b. et la geometrie emise tombe SUR la forme -- (95,30) 220x90, "
				  "pas ailleurs",
				  surLaForme, "");

			// ⚠️ 41c. LE CONTROLE NEGATIF, ET IL PROTEGE L EXISTANT : le MEME
			//     document, son parent remis en `Column`, ne doit RIEN emettre
			//     pour ce noeud. Un cadre d agencement reste invisible -- sinon le
			//     document du 18 aout se mettrait a montrer des boites partout.
			d.nodes[0].layout.kind = NkLayoutKind::Column;
			NkRecordingPaint rec2;
			RenderDocument(rec2, d, surface);
			snprintf(buf, sizeof(buf), "pose %u commande(s), en Column %u", posee,
					 (uint32)rec2.cmds.Size());
			check("41c. CONTROLE NEGATIF : le MEME noeud sous un parent `Column` "
				  "n emet RIEN -- un cadre d agencement reste invisible",
				  rec2.cmds.Empty(), buf);

			// 41d. Le document de DEMONSTRATION complet dessine ses formes ET
			//      garde ses composants. C est la forme que Rodolf voit.
			NkUIDocument demo;
			NkBuildDemoDocument(demo);
			NkRecordingPaint rec3;
			RenderDocument(rec3, demo, surface);
			uint32 formes = 0;
			for (uint32 i = 0; i < (uint32)rec3.cmds.Size(); ++i)
				if (rec3.cmds[i].op == NkPaintOp::Text
					&& (rec3.cmds[i].text.Find("Forme A") != NkString::npos
						|| rec3.cmds[i].text.Find("Forme B") != NkString::npos))
					++formes;
			snprintf(buf, sizeof(buf), "%u commande(s), dont %u libelle(s) de forme",
					 (uint32)rec3.cmds.Size(), formes);
			check("41d. le document de DEMONSTRATION dessine ses DEUX formes posees "
				  "(leurs libelles sortent dans le flux)",
				  formes == 2, buf);

			// ⚠️ 41e. L APPLICATION SAIT-ELLE RELIRE CE QU ELLE ENREGISTRE ?
			//     Ce controle manquait, et son absence a coute une heure ce matin :
			//     le document de demonstration s affichait la ou le fichier
			//     enregistre aurait du. Save -> Load -> Save : les deux textes
			//     doivent etre identiques. Un format qui ne se relit pas perd le
			//     travail SANS RIEN DIRE -- c est la pire des pertes.
			NkString e1, e2;
			demo.Save(e1);
			NkUIDocument relu;
			uint32 inconnus = 0;
			const bool lu = relu.Load(e1.Data(), &inconnus);
			if (lu)
				relu.Save(e2);
			snprintf(buf, sizeof(buf), "relu=%d, %u noeud(s), %u inconnu(s), textes %s",
					 lu ? 1 : 0, lu ? relu.NodeCount() : 0u, inconnus,
					 (lu && e1.Compare(e2) == 0) ? "IDENTIQUES" : "DIFFERENTS");
			check("41e. le document de demonstration se RELIT et se reenregistre a "
				  "l identique -- une page `free` et deux positions comprises",
				  lu && e1.Compare(e2) == 0 && relu.NodeCount() == demo.NodeCount(), buf);

			// ⚠️ 41f. LA CHAINE DU DESIGNER (2026-08-30) : ce que les outils posent
			//     — un artboard (`forme = frame`), un rectangle, un texte avec son
			//     contenu — fait l'aller-retour par le fichier. C'est l'etape 5 du
			//     mandat : « sauvegarder, rouvrir, retrouver EXACTEMENT ». Le
			//     document de depart est CELUI de Ctrl+N (NkBuildBlankDocument),
			//     pas une copie ecrite ici.
			NkUIDocument dessin;
			NkBuildBlankDocument(dessin);
			const int32 pageIdx = dessin.nodes[0].children.Empty() ? -1 : dessin.nodes[0].children[0];
			int32 rectIdx = -1, texteIdx = -1;
			if (dessin.IsValidIndex(pageIdx)) {
				rectIdx = dessin.AddChild(pageIdx, "", NkAuthor::Humain);
				if (dessin.IsValidIndex(rectIdx)) {
					NkUINode &nr = dessin.nodes[(uint32)rectIdx];
					nr.label = NkString("Rectangle 1");
					nr.shape = NkString("rect");
					nr.posX = 24.f;
					nr.posY = 300.f;
					nr.width.mode = NkSizeMode::Fixed;
					nr.width.value = 342.f;
					nr.height.mode = NkSizeMode::Fixed;
					nr.height.value = 44.f;
				}
				texteIdx = dessin.AddChild(pageIdx, "", NkAuthor::Humain);
				if (dessin.IsValidIndex(texteIdx)) {
					NkUINode &nt = dessin.nodes[(uint32)texteIdx];
					nt.label = NkString("Texte 1");
					nt.shape = NkString("text");
					nt.text = NkString("Connexion");
					nt.posX = 24.f;
					nt.posY = 200.f;
					nt.width.mode = NkSizeMode::Fixed;
					nt.width.value = 200.f;
					nt.height.mode = NkSizeMode::Fixed;
					nt.height.value = 28.f;
				}
			}
			NkString f1, f2;
			dessin.Save(f1);
			NkUIDocument redessine;
			uint32 inconnus2 = 0;
			const bool lu2 = redessine.Load(f1.Data(), &inconnus2);
			if (lu2)
				redessine.Save(f2);
			const bool formesRetrouvees =
				lu2 && redessine.IsValidIndex(rectIdx) && redessine.IsValidIndex(texteIdx)
				&& StrEq(redessine.nodes[(uint32)rectIdx].shape.Data(), "rect")
				&& StrEq(redessine.nodes[(uint32)texteIdx].shape.Data(), "text")
				&& StrEq(redessine.nodes[(uint32)texteIdx].text.Data(), "Connexion")
				&& dessin.IsValidIndex(pageIdx)
				&& StrEq(redessine.nodes[(uint32)pageIdx].shape.Data(), "frame");
			snprintf(buf, sizeof(buf), "relu=%d, formes+texte %s, textes %s", lu2 ? 1 : 0,
					 formesRetrouvees ? "RETROUVES" : "PERDUS",
					 (lu2 && f1.Compare(f2) == 0) ? "IDENTIQUES" : "DIFFERENTS");
			check("41f. un dessin (artboard + rectangle + texte) se sauve, se relit et se "
				  "reenregistre a l identique — la chaine du designer, etape 5",
				  formesRetrouvees && f1.Compare(f2) == 0, buf);
		}

		// ═══════════════════════════════════════════════════════════════════════
		//  42. LA LISTE DE REMPLISSAGES (Lunacy « FILLS », 01/09)
		// ═══════════════════════════════════════════════════════════════════════
		// ⚠️ LE PREMIER CAS EST LA PREUVE D'ENTREE, PAS UNE VERIFICATION DE
		//    SORTIE. Le mandat impose l'additivite : « les documents d'avant se
		//    reenregistrent OCTET POUR OCTET ». Si 42a tombe, le reste de la
		//    famille ne vaut rien -- on aurait ajoute une capacite en changeant
		//    les fichiers de tout le monde.
		{
			// 42a. UN DOCUMENT D'AVANT (cle simple `fond`) ne gagne aucune cle.
			{
				NkUIDocument d;
				d.NewDocument("avant", NkAuthor::Humain);
				const int32 i = d.AddChild(0, "", NkAuthor::Humain);
				d.nodes[(uint32)i].label = NkString("Boite");
				d.nodes[(uint32)i].shape = NkString("rect");
				d.nodes[(uint32)i].fill = NkString("#12ab34");
				NkString a1, a2;
				d.Save(a1);
				NkUIDocument r;
				const bool lu = r.Load(a1.Data());
				if (lu)
					r.Save(a2);
				bool sansListe = true, avecSimple = false;
				for (const char *q = a1.Data() ? a1.Data() : ""; *q; ++q)
					if (q[0] == 'f' && q[1] == 'o' && q[2] == 'n' && q[3] == 'd') {
						if (q[4] == '_')
							sansListe = false;
						else if (q[4] == ' ')
							avecSimple = true;
					}
				// ⚠️ « OCTET POUR OCTET » NE SUFFIT PAS, ET UNE MUTATION L'A
				//    PROUVE. En forcant l'ecriture de la liste meme vide, la cle
				//    `fond` DISPARAISSAIT du fichier : le document perdait sa
				//    couleur, et se reenregistrait ensuite parfaitement identique
				//    a sa version amputee. Ce cas passait au vert sur une PERTE
				//    DE DONNEE. On exige donc les trois choses a la fois : la cle
				//    simple PRESENTE, la cle de liste ABSENTE, et la valeur
				//    RETROUVEE apres relecture -- la stabilite ne prouve rien
				//    toute seule, elle prouve seulement qu'on est stable.
				const bool valeurGardee =
					lu && r.IsValidIndex(i) && StrEq(r.nodes[(uint32)i].fill.Data(), "#12ab34");
				snprintf(buf, sizeof(buf), "relu=%d, octets %s, `fond` %s, `fond_` %s, valeur %s",
						 lu ? 1 : 0, (lu && a1.Compare(a2) == 0) ? "IDENTIQUES" : "DIFFERENTS",
						 avecSimple ? "presente" : "DISPARUE", sansListe ? "absente" : "APPARUE",
						 valeurGardee ? "gardee" : "PERDUE");
				check("42a. un document a cle simple `fond` se reenregistre OCTET POUR "
					  "OCTET, garde sa cle et sa valeur, et ne gagne aucune cle de liste "
					  "— la preuve d entree",
					  lu && a1.Compare(a2) == 0 && sansListe && avecSimple && valeurGardee, buf);
			}
			// 42b. LA LISTE fait l'aller-retour : couleurs, opacites, yeux.
			{
				NkUIDocument d;
				d.NewDocument("liste", NkAuthor::Humain);
				const int32 i = d.AddChild(0, "", NkAuthor::Humain);
				{
					NkUINode &n = d.nodes[(uint32)i];
					n.label = NkString("Boite");
					n.shape = NkString("rect");
					NkRemplissage fa;
					fa.couleur = NkString("#ff0000");
					NkRemplissage fb;
					fb.couleur = NkString("#00ff00");
					fb.opacite = 50.f;
					NkRemplissage fc;
					fc.couleur = NkString("#0000ff");
					fc.visible = false;
					n.fills.PushBack(fa);
					n.fills.PushBack(fb);
					n.fills.PushBack(fc);
				}
				NkString b1, b2;
				d.Save(b1);
				NkUIDocument r;
				const bool lu = r.Load(b1.Data());
				if (lu)
					r.Save(b2);
				const NkUINode *rn = (lu && r.IsValidIndex(i)) ? &r.nodes[(uint32)i] : nullptr;
				const bool champs = rn && rn->fills.Size() == 3
									&& StrEq(rn->fills[0].couleur.Data(), "#ff0000")
									&& rn->fills[1].opacite == 50.f
									&& rn->fills[2].visible == false
									&& StrEq(rn->fills[2].couleur.Data(), "#0000ff");
				snprintf(buf, sizeof(buf), "relu=%d, %u remplissage(s), champs %s, octets %s",
						 lu ? 1 : 0, rn ? (uint32)rn->fills.Size() : 0u,
						 champs ? "RETROUVES" : "PERDUS",
						 (lu && b1.Compare(b2) == 0) ? "IDENTIQUES" : "DIFFERENTS");
				check("42b. trois remplissages (couleur, opacite, oeil) font l aller-retour "
					  "et se reenregistrent a l identique",
					  champs && lu && b1.Compare(b2) == 0, buf);
			}
			// 42c. LE FOND EFFECTIF : le DERNIER visible gagne (ordre Lunacy), et
			//      une liste entierement masquee ne peint RIEN.
			{
				NkUINode n;
				n.fill = NkString("#111111");
				const char *avant = n.FondEffectif();
				n.MaterialiserFills();
				NkRemplissage f2c;
				f2c.couleur = NkString("#222222");
				n.fills.PushBack(f2c);
				const char *dessus = n.FondEffectif();
				n.fills[1].visible = false;
				const char *sous = n.FondEffectif();
				n.fills[0].visible = false;
				const char *rien = n.FondEffectif();
				snprintf(buf, sizeof(buf), "simple=%s, dessus=%s, sous=%s, tout masque=%s",
						 avant ? avant : "(rien)", dessus ? dessus : "(rien)",
						 sous ? sous : "(rien)", rien ? rien : "(rien)");
				check("42c. le fond effectif : le DERNIER visible gagne, et une liste "
					  "entierement masquee ne peint RIEN (elle ne retombe pas sur `fond`)",
					  avant && StrEq(avant, "#111111") && dessus && StrEq(dessus, "#222222")
						  && sous && StrEq(sous, "#111111") && rien == nullptr,
					  buf);
			}
			// 42d. MATERIALISER part de la cle simple et n'est pas cumulatif.
			{
				NkUINode n;
				n.fill = NkString("#abcdef");
				n.MaterialiserFills();
				const uint32 un = (uint32)n.fills.Size();
				n.MaterialiserFills();
				const uint32 deux = (uint32)n.fills.Size();
				const bool ok = un == 1 && deux == 1
								&& StrEq(n.fills[0].couleur.Data(), "#abcdef")
								&& n.fills[0].opacite == 100.f && n.fills[0].visible;
				snprintf(buf, sizeof(buf), "1er appel=%u, 2e appel=%u, couleur=%s", un, deux,
						 n.fills.Empty() ? "(vide)" : n.fills[0].couleur.Data());
				check("42d. materialiser part de la cle simple, opaque et visible, et le "
					  "second appel ne duplique rien",
					  ok, buf);
			}
			// 42e. LA COPIE emporte la liste — meme classe de piege que le champ
			//      oublie qu'a trouve la recette gestes.
			{
				NkUIDocument d;
				d.NewDocument("copie", NkAuthor::Humain);
				const int32 i = d.AddChild(0, "", NkAuthor::Humain);
				d.nodes[(uint32)i].label = NkString("Source");
				d.nodes[(uint32)i].shape = NkString("rect");
				d.nodes[(uint32)i].MaterialiserFills();
				d.nodes[(uint32)i].fills[0].couleur = NkString("#c0ffee");
				d.nodes[(uint32)i].fills[0].opacite = 42.f;
				const int32 j = d.CopierSousArbre(d, i, 0);
				const bool ok = d.IsValidIndex(j) && d.nodes[(uint32)j].fills.Size() == 1
								&& StrEq(d.nodes[(uint32)j].fills[0].couleur.Data(), "#c0ffee")
								&& d.nodes[(uint32)j].fills[0].opacite == 42.f;
				snprintf(buf, sizeof(buf), "copie=%d, %u remplissage(s), opacite=%.0f", j,
						 d.IsValidIndex(j) ? (uint32)d.nodes[(uint32)j].fills.Size() : 0u,
						 (d.IsValidIndex(j) && !d.nodes[(uint32)j].fills.Empty())
							 ? d.nodes[(uint32)j].fills[0].opacite
							 : -1.f);
				check("42e. copier un sous-arbre emporte la LISTE de remplissages, opacite "
					  "comprise",
					  ok, buf);
			}
			// 42f. UN NOEUD MATERIALISE N'ECRIT PAS DEUX VERITES (le pendant de
			//      43f). ⚠️ AJOUTE PAR SYMETRIE, PAS PAR UN ECHEC : la mutation
			//      « ecrire la cle simple MEME quand la liste existe » ne cassait
			//      rien cote bordures tant qu'aucun cas n'avait les DEUX. La
			//      lecon T5 du depot dit qu'on l'applique au banc VOISIN, pas
			//      seulement la ou on l'a trouvee — donc ici aussi, pour `fond`.
			{
				NkUIDocument d;
				d.NewDocument("deux verites", NkAuthor::Humain);
				const int32 i = d.AddChild(0, "", NkAuthor::Humain);
				d.nodes[(uint32)i].label = NkString("Boite");
				d.nodes[(uint32)i].shape = NkString("rect");
				d.nodes[(uint32)i].fill = NkString("#111111"); // la cle simple RESTE posee
				d.nodes[(uint32)i].MaterialiserFills();
				d.nodes[(uint32)i].fills[0].couleur = NkString("#999999"); // la liste diverge
				NkString f1, f2;
				d.Save(f1);
				bool aListe = false, aSimple = false;
				for (const char *q = f1.Data() ? f1.Data() : ""; *q; ++q)
					if (q[0] == 'f' && q[1] == 'o' && q[2] == 'n' && q[3] == 'd') {
						if (q[4] == '_')
							aListe = true;
						else if (q[4] == ' ')
							aSimple = true;
					}
				NkUIDocument r;
				const bool lu = r.Load(f1.Data());
				if (lu)
					r.Save(f2);
				const bool bonneAutorite =
					lu && r.IsValidIndex(i) && r.nodes[(uint32)i].fills.Size() == 1
					&& StrEq(r.nodes[(uint32)i].fills[0].couleur.Data(), "#999999");
				snprintf(buf, sizeof(buf), "`fond_` %s, `fond` %s, couleur relue %s",
						 aListe ? "presente" : "ABSENTE",
						 aSimple ? "PRESENTE (deux verites)" : "absente",
						 bonneAutorite ? r.nodes[(uint32)i].fills[0].couleur.Data() : "(perdue)");
				check("42f. un noeud MATERIALISE n ecrit QUE la liste de remplissages : la cle "
					  "simple ne subsiste pas a cote, et la liste fait autorite",
					  aListe && !aSimple && bonneAutorite && lu && f1.Compare(f2) == 0, buf);
			}
		}

		// ═══════════════════════════════════════════════════════════════════════
		//  43. LA LISTE DE BORDURES (Lunacy « BORDERS », 01/09)
		// ═══════════════════════════════════════════════════════════════════════
		// ⚠️ LES CAS PORTENT LE VOLET *CONSERVATION* DES LEUR ECRITURE, pas apres
		//    coup : la porte du depot est tombee ce matin sur la famille 42, ou
		//    « octet pour octet » passait au vert sur une cle DISPARUE. Chaque
		//    aller-retour exige donc les deux -- la stabilite ET le contenu
		//    attendu encore la, champ par champ.
		{
			// 43a. Un document a cles simples (`couleur_bord` + `bordure`) ne
			//      gagne aucune cle de liste, garde ses cles ET ses valeurs.
			{
				NkUIDocument d;
				d.NewDocument("avant", NkAuthor::Humain);
				const int32 i = d.AddChild(0, "", NkAuthor::Humain);
				d.nodes[(uint32)i].label = NkString("Boite");
				d.nodes[(uint32)i].shape = NkString("rect");
				d.nodes[(uint32)i].borderColor = NkString("#334455");
				d.nodes[(uint32)i].borderW = 3.f;
				NkString a1, a2;
				d.Save(a1);
				NkUIDocument r;
				const bool lu = r.Load(a1.Data());
				if (lu)
					r.Save(a2);
				bool sansListe = true, avecSimple = false, avecLargeur = false;
				for (const char *q = a1.Data() ? a1.Data() : ""; *q; ++q) {
					if (q[0] == 'b' && q[1] == 'o' && q[2] == 'r' && q[3] == 'd' && q[4] == '_')
						sansListe = false;
					if (q[0] == 'c' && q[1] == 'o' && q[2] == 'u' && q[3] == 'l' && q[4] == 'e'
						&& q[5] == 'u' && q[6] == 'r' && q[7] == '_' && q[8] == 'b')
						avecSimple = true;
					if (q[0] == 'b' && q[1] == 'o' && q[2] == 'r' && q[3] == 'd' && q[4] == 'u'
						&& q[5] == 'r' && q[6] == 'e')
						avecLargeur = true;
				}
				const bool garde = lu && r.IsValidIndex(i)
								   && StrEq(r.nodes[(uint32)i].borderColor.Data(), "#334455")
								   && r.nodes[(uint32)i].borderW == 3.f;
				snprintf(buf, sizeof(buf),
						 "octets %s, `couleur_bord` %s, `bordure` %s, `bord_` %s, valeurs %s",
						 (lu && a1.Compare(a2) == 0) ? "IDENTIQUES" : "DIFFERENTS",
						 avecSimple ? "presente" : "DISPARUE",
						 avecLargeur ? "presente" : "DISPARUE",
						 sansListe ? "absente" : "APPARUE", garde ? "gardees" : "PERDUES");
				check("43a. un document a cles simples de bordure se reenregistre OCTET POUR "
					  "OCTET, garde ses deux cles et leurs valeurs, sans cle de liste",
					  lu && a1.Compare(a2) == 0 && sansListe && avecSimple && avecLargeur
						  && garde,
					  buf);
			}
			// 43b. La LISTE fait l'aller-retour : couleur, opacite, oeil,
			//      epaisseur ET POSITION -- les cinq champs, un par un.
			{
				NkUIDocument d;
				d.NewDocument("liste", NkAuthor::Humain);
				const int32 i = d.AddChild(0, "", NkAuthor::Humain);
				{
					NkUINode &n = d.nodes[(uint32)i];
					n.label = NkString("Boite");
					n.shape = NkString("rect");
					NkBordure ba;
					ba.couleur = NkString("#ff0000");
					ba.epaisseur = 2.f;
					ba.position = NkBordurePos::Interieur;
					NkBordure bb;
					bb.couleur = NkString("#00ff00");
					bb.opacite = 40.f;
					bb.visible = false;
					bb.epaisseur = 5.5f;
					bb.position = NkBordurePos::Exterieur;
					n.borders.PushBack(ba);
					n.borders.PushBack(bb);
				}
				NkString b1, b2;
				d.Save(b1);
				NkUIDocument r;
				const bool lu = r.Load(b1.Data());
				if (lu)
					r.Save(b2);
				const NkUINode *rn = (lu && r.IsValidIndex(i)) ? &r.nodes[(uint32)i] : nullptr;
				const bool champs = rn && rn->borders.Size() == 2
									&& StrEq(rn->borders[0].couleur.Data(), "#ff0000")
									&& rn->borders[0].epaisseur == 2.f
									&& rn->borders[0].position == NkBordurePos::Interieur
									&& rn->borders[1].opacite == 40.f
									&& rn->borders[1].visible == false
									&& rn->borders[1].epaisseur == 5.5f
									&& rn->borders[1].position == NkBordurePos::Exterieur;
				snprintf(buf, sizeof(buf), "%u bordure(s), champs %s, octets %s",
						 rn ? (uint32)rn->borders.Size() : 0u, champs ? "RETROUVES" : "PERDUS",
						 (lu && b1.Compare(b2) == 0) ? "IDENTIQUES" : "DIFFERENTS");
				check("43b. deux bordures (couleur, opacite, oeil, epaisseur, POSITION) font "
					  "l aller-retour et se reenregistrent a l identique",
					  champs && lu && b1.Compare(b2) == 0, buf);
			}
			// 43c. LA POSITION CHANGE LE DESSIN, et c'est mesure sur la GEOMETRIE,
			//      pas sur la valeur du champ. Trois positions, trois cadres
			//      distincts -- sinon le menu a trois entrees en aurait deux qui
			//      mentent.
			{
				NkRecordingPaint rec;
				auto premierRect = [&](NkBordurePos pos, float32 &x, float32 &w) {
					rec.cmds.Clear();
					NkBordure b;
					b.couleur = NkString("#ffffff");
					b.epaisseur = 4.f;
					b.position = pos;
					// Rayons NULS et interieur transparent : cette sonde mesure le
					// DECALAGE de position, pas l'arrondi -- lui donner un rayon
					// changerait ce qu'elle observe.
					const float32 R0[4] = {0.f, 0.f, 0.f, 0.f};
					renderdetail::NkGCadre(rec, {100.f, 100.f, 50.f, 50.f}, b, R0, 0u);
					x = rec.cmds.Empty() ? -1.f : rec.cmds[0].x;
					w = rec.cmds.Empty() ? -1.f : rec.cmds[0].w;
				};
				float32 xi = 0.f, wi = 0.f, xc = 0.f, wc = 0.f, xe = 0.f, we = 0.f;
				premierRect(NkBordurePos::Interieur, xi, wi);
				premierRect(NkBordurePos::Centre, xc, wc);
				premierRect(NkBordurePos::Exterieur, xe, we);
				snprintf(buf, sizeof(buf),
						 "interieur x=%.0f l=%.0f, centre x=%.0f l=%.0f, exterieur x=%.0f "
						 "l=%.0f",
						 xi, wi, xc, wc, xe, we);
				check("43c. la POSITION de bordure change la GEOMETRIE peinte : trois "
					  "positions, trois cadres distincts (0 / e-demi / e)",
					  xi == 100.f && xc == 98.f && xe == 96.f && wi == 50.f && wc == 54.f
						  && we == 58.f,
					  buf);
			}
			// 43d. La copie emporte la liste de bordures (le collage qui perd).
			{
				NkUIDocument d;
				d.NewDocument("copie", NkAuthor::Humain);
				const int32 i = d.AddChild(0, "", NkAuthor::Humain);
				d.nodes[(uint32)i].label = NkString("Source");
				d.nodes[(uint32)i].borderColor = NkString("#abcdef");
				d.nodes[(uint32)i].borderW = 2.f;
				d.nodes[(uint32)i].MaterialiserBorders();
				d.nodes[(uint32)i].borders[0].position = NkBordurePos::Exterieur;
				const int32 j = d.CopierSousArbre(d, i, 0);
				const bool ok = d.IsValidIndex(j) && d.nodes[(uint32)j].borders.Size() == 1
								&& StrEq(d.nodes[(uint32)j].borders[0].couleur.Data(), "#abcdef")
								&& d.nodes[(uint32)j].borders[0].epaisseur == 2.f
								&& d.nodes[(uint32)j].borders[0].position
									   == NkBordurePos::Exterieur;
				snprintf(buf, sizeof(buf), "%u bordure(s) copiee(s)",
						 d.IsValidIndex(j) ? (uint32)d.nodes[(uint32)j].borders.Size() : 0u);
				check("43d. copier un sous-arbre emporte la LISTE de bordures, position "
					  "comprise",
					  ok, buf);
			}
			// 43e. MATERIALISER part des DEUX cles simples (couleur ET epaisseur).
			{
				NkUINode n;
				n.borderColor = NkString("#123456");
				n.borderW = 7.f;
				n.MaterialiserBorders();
				const uint32 un = (uint32)n.borders.Size();
				n.MaterialiserBorders();
				const bool ok = un == 1 && n.borders.Size() == 1
								&& StrEq(n.borders[0].couleur.Data(), "#123456")
								&& n.borders[0].epaisseur == 7.f && n.borders[0].visible;
				snprintf(buf, sizeof(buf), "1er=%u, 2e=%u, couleur=%s, epaisseur=%.0f", un,
						 (uint32)n.borders.Size(),
						 n.borders.Empty() ? "(vide)" : n.borders[0].couleur.Data(),
						 n.borders.Empty() ? -1.f : n.borders[0].epaisseur);
				check("43e. materialiser une bordure part des DEUX cles simples (couleur ET "
					  "epaisseur) et ne duplique pas",
					  ok, buf);
			}
			// 43f. UN NOEUD MATERIALISE N'ECRIT PAS DEUX VERITES.
			//      ⚠️ CE CAS EXISTE PARCE QU'UNE MUTATION N'A RIEN CASSE. En
			//      forcant l'ecriture de `bordure` MEME quand la liste existe,
			//      aucun des cinq cas precedents ne bougeait : 43a n'a pas de
			//      liste, 43b a une liste mais un `borderW` nul, et les autres ne
			//      passent pas par le fichier. Le seul document qui expose le
			//      defaut est celui qui a les DEUX -- une liste ET des cles
			//      simples non nulles -- c'est-a-dire tout noeud MATERIALISE
			//      depuis l'inspecteur, donc le cas le plus courant en usage
			//      reel. Un fichier a deux verites ferait choisir le lecteur,
			//      c'est-a-dire deviner.
			{
				NkUIDocument d;
				d.NewDocument("deux verites", NkAuthor::Humain);
				const int32 i = d.AddChild(0, "", NkAuthor::Humain);
				d.nodes[(uint32)i].label = NkString("Boite");
				d.nodes[(uint32)i].shape = NkString("rect");
				d.nodes[(uint32)i].borderColor = NkString("#334455");
				d.nodes[(uint32)i].borderW = 3.f; // les cles simples RESTENT posees
				d.nodes[(uint32)i].MaterialiserBorders();
				d.nodes[(uint32)i].borders[0].epaisseur = 9.f; // la liste diverge
				NkString f1, f2;
				d.Save(f1);
				bool aListe = false, aSimpleCouleur = false, aSimpleLargeur = false;
				for (const char *q = f1.Data() ? f1.Data() : ""; *q; ++q) {
					if (q[0] == 'b' && q[1] == 'o' && q[2] == 'r' && q[3] == 'd' && q[4] == '_')
						aListe = true;
					if (q[0] == 'c' && q[1] == 'o' && q[2] == 'u' && q[3] == 'l' && q[4] == 'e'
						&& q[5] == 'u' && q[6] == 'r' && q[7] == '_' && q[8] == 'b')
						aSimpleCouleur = true;
					if (q[0] == 'b' && q[1] == 'o' && q[2] == 'r' && q[3] == 'd' && q[4] == 'u'
						&& q[5] == 'r' && q[6] == 'e' && q[7] == ' ')
						aSimpleLargeur = true;
				}
				NkUIDocument r;
				const bool lu = r.Load(f1.Data());
				if (lu)
					r.Save(f2);
				// conservation : c'est bien la valeur de LA LISTE qui survit (9),
				// pas celle de la cle simple (3) -- l'autorite est nommee.
				const bool bonneAutorite = lu && r.IsValidIndex(i)
										   && r.nodes[(uint32)i].borders.Size() == 1
										   && r.nodes[(uint32)i].borders[0].epaisseur == 9.f;
				snprintf(buf, sizeof(buf),
						 "`bord_` %s, `couleur_bord` %s, `bordure` %s, epaisseur relue %.0f",
						 aListe ? "presente" : "ABSENTE",
						 aSimpleCouleur ? "PRESENTE (deux verites)" : "absente",
						 aSimpleLargeur ? "PRESENTE (deux verites)" : "absente",
						 bonneAutorite ? r.nodes[(uint32)i].borders[0].epaisseur : -1.f);
				check("43f. un noeud MATERIALISE n ecrit QUE la liste : aucune cle simple ne "
					  "subsiste a cote, et c est la valeur de la liste qui fait autorite",
					  aListe && !aSimpleCouleur && !aSimpleLargeur && bonneAutorite
						  && lu && f1.Compare(f2) == 0,
					  buf);
			}
		}

		// ═══════════════════════════════════════════════════════════════════════
		//  44. LA LISTE D'EFFETS (Lunacy « EFFECTS », 01/09)
		// ═══════════════════════════════════════════════════════════════════════
		// ⚠️ LE VOLET CONSERVATION EST ECRIT D'ENTREE, pas ajoute apres coup, et
		//    le cas « deux verites » est la des le depart : ce sont les deux
		//    defauts que les familles 42 et 43 ont payes en apprenant. La lecon
		//    d'un banc ne couvre pas le banc voisin toute seule -- on la porte.
		{
			// 44a. Un document SANS effet ne gagne aucune cle. C'est la preuve
			//      d'entree, et elle est plus facile ici qu'ailleurs : les effets
			//      n'ont AUCUNE cle simple a preserver, le modele n'a jamais porte
			//      d'ombre. La regle tient quand meme, et il faut le VERIFIER --
			//      « plus facile » n'est pas « acquis ».
			{
				NkUIDocument d;
				d.NewDocument("sans effet", NkAuthor::Humain);
				const int32 i = d.AddChild(0, "", NkAuthor::Humain);
				d.nodes[(uint32)i].label = NkString("Boite");
				d.nodes[(uint32)i].shape = NkString("rect");
				d.nodes[(uint32)i].fill = NkString("#101010");
				NkString a1, a2;
				d.Save(a1);
				NkUIDocument r;
				const bool lu = r.Load(a1.Data());
				if (lu)
					r.Save(a2);
				bool sansCle = true;
				for (const char *q = a1.Data() ? a1.Data() : ""; *q; ++q)
					if (q[0] == 'e' && q[1] == 'f' && q[2] == 'f' && q[3] == 'e' && q[4] == 't'
						&& q[5] == '_') {
						sansCle = false;
						break;
					}
				// conservation : le reste du noeud est encore la
				const bool garde = lu && r.IsValidIndex(i)
								   && StrEq(r.nodes[(uint32)i].fill.Data(), "#101010")
								   && r.nodes[(uint32)i].effets.Empty();
				snprintf(buf, sizeof(buf), "octets %s, cle `effet_` %s, le reste %s",
						 (lu && a1.Compare(a2) == 0) ? "IDENTIQUES" : "DIFFERENTS",
						 sansCle ? "absente" : "APPARUE", garde ? "garde" : "PERDU");
				check("44a. un document sans effet ne gagne aucune cle, se reenregistre OCTET "
					  "POUR OCTET et garde le reste de son apparence",
					  lu && a1.Compare(a2) == 0 && sansCle && garde, buf);
			}
			// 44b. LES HUIT CHAMPS font l'aller-retour, un par un. ⚠️ QUATRE
			//      NOMBRES, PAS DEUX : X, Y, flou ET etendue. Un banc qui n'en
			//      verifie que deux laisse passer la moitie du gabarit.
			{
				NkUIDocument d;
				d.NewDocument("effets", NkAuthor::Humain);
				const int32 i = d.AddChild(0, "", NkAuthor::Humain);
				{
					NkUINode &n = d.nodes[(uint32)i];
					n.label = NkString("Boite");
					n.shape = NkString("rect");
					NkEffet ea; // le gabarit EXACT de lunacy_props_11
					ea.type = NkEffetType::OmbrePortee;
					ea.x = 0.f;
					ea.y = 4.f;
					ea.flou = 4.f;
					ea.etendue = 0.f;
					ea.couleur = NkString("#000000");
					ea.opacite = 25.f;
					NkEffet eb;
					eb.type = NkEffetType::OmbreInterne;
					eb.x = -3.5f;
					eb.y = 7.f;
					eb.flou = 12.f;
					eb.etendue = 2.5f;
					eb.couleur = NkString("#ff00ff");
					eb.opacite = 60.f;
					eb.visible = false;
					n.effets.PushBack(ea);
					n.effets.PushBack(eb);
				}
				NkString b1, b2;
				d.Save(b1);
				NkUIDocument r;
				const bool lu = r.Load(b1.Data());
				if (lu)
					r.Save(b2);
				const NkUINode *rn = (lu && r.IsValidIndex(i)) ? &r.nodes[(uint32)i] : nullptr;
				const bool champs = rn && rn->effets.Size() == 2
									&& rn->effets[0].type == NkEffetType::OmbrePortee
									&& rn->effets[0].y == 4.f && rn->effets[0].flou == 4.f
									&& rn->effets[0].opacite == 25.f
									&& StrEq(rn->effets[0].couleur.Data(), "#000000")
									&& rn->effets[1].type == NkEffetType::OmbreInterne
									&& rn->effets[1].x == -3.5f && rn->effets[1].flou == 12.f
									&& rn->effets[1].etendue == 2.5f
									&& rn->effets[1].visible == false;
				snprintf(buf, sizeof(buf), "%u effet(s), champs %s, octets %s",
						 rn ? (uint32)rn->effets.Size() : 0u, champs ? "RETROUVES" : "PERDUS",
						 (lu && b1.Compare(b2) == 0) ? "IDENTIQUES" : "DIFFERENTS");
				check("44b. deux effets (type, X, Y, flou, etendue, couleur, opacite, oeil) "
					  "font l aller-retour et se reenregistrent a l identique",
					  champs && lu && b1.Compare(b2) == 0, buf);
			}
			// 44c. LES DEUX TYPES NE SE CONFONDENT PAS. ⚠️ « ombre_portee » et
			//      « ombre_interne » partagent leurs SIX premiers caracteres : un
			//      lecteur qui discrimine sur le premier rendrait toujours le
			//      meme type, et 44b ne le verrait pas si les deux effets avaient
			//      le meme. Ce cas existe pour ce piege precis.
			{
				const bool ok = NkParseEffetType("ombre_portee") == NkEffetType::OmbrePortee
								&& NkParseEffetType("ombre_interne") == NkEffetType::OmbreInterne
								&& StrEq(NkEffetTypeNom(NkEffetType::OmbrePortee), "ombre_portee")
								&& StrEq(NkEffetTypeNom(NkEffetType::OmbreInterne),
										 "ombre_interne");
				snprintf(buf, sizeof(buf), "portee->%s, interne->%s",
						 NkEffetTypeNom(NkParseEffetType("ombre_portee")),
						 NkEffetTypeNom(NkParseEffetType("ombre_interne")));
				check("44c. les deux types d effet se relisent distinctement (ils partagent "
					  "six caracteres sur douze)",
					  ok, buf);
			}
			// 44d. L OMBRE EST PEINTE, ET AVANT LA FORME. Mesure sur la
			//      GEOMETRIE : une ombre visible ajoute des commandes, une ombre
			//      a l oeil ferme n en ajoute AUCUNE.
			{
				NkRecordingPaint rec;
				NkUINode n;
				n.shape = NkString("rect");
				NkEffet e;
				e.couleur = NkString("#000000");
				e.y = 4.f;
				e.flou = 4.f;
				e.opacite = 50.f;
				n.effets.PushBack(e);
				renderdetail::NkGOmbres(rec, {100.f, 100.f, 50.f, 50.f}, n);
				const uint32 avecOmbre = (uint32)rec.cmds.Size();
				rec.cmds.Clear();
				n.effets[0].visible = false;
				renderdetail::NkGOmbres(rec, {100.f, 100.f, 50.f, 50.f}, n);
				const uint32 oeilFerme = (uint32)rec.cmds.Size();
				rec.cmds.Clear();
				n.effets[0].visible = true;
				n.effets[0].type = NkEffetType::OmbreInterne;
				renderdetail::NkGOmbres(rec, {100.f, 100.f, 50.f, 50.f}, n);
				const uint32 interne = (uint32)rec.cmds.Size();
				snprintf(buf, sizeof(buf),
						 "portee=%u commande(s), oeil ferme=%u, interne=%u (non peinte, dit "
						 "au code)",
						 avecOmbre, oeilFerme, interne);
				check("44d. l ombre portee PEINT (plusieurs anneaux pour le flou), l oeil "
					  "ferme ne peint RIEN, et l ombre interne non plus (son peintre manque)",
					  avecOmbre > 1u && oeilFerme == 0u && interne == 0u, buf);
			}
			// 44e. La copie emporte la liste d effets.
			{
				NkUIDocument d;
				d.NewDocument("copie", NkAuthor::Humain);
				const int32 i = d.AddChild(0, "", NkAuthor::Humain);
				d.nodes[(uint32)i].label = NkString("Source");
				NkEffet e;
				e.couleur = NkString("#123456");
				e.etendue = 6.f;
				d.nodes[(uint32)i].effets.PushBack(e);
				const int32 j = d.CopierSousArbre(d, i, 0);
				const bool ok = d.IsValidIndex(j) && d.nodes[(uint32)j].effets.Size() == 1
								&& StrEq(d.nodes[(uint32)j].effets[0].couleur.Data(), "#123456")
								&& d.nodes[(uint32)j].effets[0].etendue == 6.f;
				snprintf(buf, sizeof(buf), "%u effet(s) copie(s)",
						 d.IsValidIndex(j) ? (uint32)d.nodes[(uint32)j].effets.Size() : 0u);
				check("44e. copier un sous-arbre emporte la LISTE d effets, etendue comprise",
					  ok, buf);
			}
		}

		char tail[128];
		// ── 45. LA TRANSFORMEE ENTIERE, VUE PAR LE PEINTRE ENREGISTREUR ─────
		// Le document empile sa matrice effective (rotation, miroirs, ECHELLE,
		// ancetres compris) dans le peintre ; tout ce qu'un noeud dessine --
		// forme, composant, TEXTE -- passe dessous. C'est ici que le texte tourne
		// se prouve sans fenetre : un `Text` emis entre un Push non identite et
		// son Pop.
		{
			NkUIDocument dT;
			dT.NewDocument("Toile", NkAuthor::Humain);
			dT.nodes[0].layout.kind = NkLayoutKind::Free;
			dT.SetMetric("espacement", 0.f);
			dT.SetMetric("marge", 0.f);
			const int32 grp = dT.AddChild(0, "", NkAuthor::Humain);
			dT.nodes[(uint32)grp].shape = NkString("rect");
			dT.nodes[(uint32)grp].layout.kind = NkLayoutKind::Free;
			dT.nodes[(uint32)grp].posX = 100.f;
			dT.nodes[(uint32)grp].posY = 100.f;
			dT.nodes[(uint32)grp].width.mode = NkSizeMode::Fixed;
			dT.nodes[(uint32)grp].width.value = 200.f;
			dT.nodes[(uint32)grp].height.mode = NkSizeMode::Fixed;
			dT.nodes[(uint32)grp].height.value = 150.f;
			// comme chez Rodolf : `Bouton_Connexion` (rect) porte « Texte du bouton »
			// (forme `text`, « Se connecter »)
			const int32 bouton = dT.AddChild(grp, "", NkAuthor::Humain);
			dT.nodes[(uint32)bouton].shape = NkString("text");
			dT.nodes[(uint32)bouton].text = NkString("Se connecter");
			dT.nodes[(uint32)bouton].posX = 10.f;
			dT.nodes[(uint32)bouton].posY = 10.f;
			dT.nodes[(uint32)bouton].width.mode = NkSizeMode::Fixed;
			dT.nodes[(uint32)bouton].width.value = 120.f;
			dT.nodes[(uint32)bouton].height.mode = NkSizeMode::Fixed;
			dT.nodes[(uint32)bouton].height.value = 40.f;
			NkPaintRect surfT;
			surfT.x = 0.f;
			surfT.y = 0.f;
			surfT.w = 800.f;
			surfT.h = 600.f;
			char det[320];
			// 45a. Droit : AUCUNE matrice empilee -- le dessin d'avant, a l'octet.
			NkRecordingPaint droit;
			RenderDocument(droit, dT, surfT);
			uint32 pushDroit = 0u, texteDroit = 0u;
			for (uint32 i = 0; i < (uint32)droit.cmds.Size(); ++i) {
				if (droit.cmds[i].op == NkPaintOp::PushTransform)
					++pushDroit;
				if (droit.cmds[i].op == NkPaintOp::Text)
					++texteDroit;
			}
			snprintf(det, sizeof(det), "%u Push, %u Text sur %u commande(s)", pushDroit, texteDroit,
					 (uint32)droit.cmds.Size());
			check("45a. un document DROIT n empile AUCUNE matrice : il emet ce qu il emettait "
				  "(et son texte est bien la)",
				  pushDroit == 0u && texteDroit > 0u, det);
			// 45b. Tourne de 30 degres ET double en largeur : la matrice empilee
			// est celle de NkMatDe -- a = cos*sx, b = sin*sx, c = -sin, d = cos.
			dT.nodes[(uint32)grp].rotation = 30.f;
			dT.nodes[(uint32)grp].echelleX = 2.f;
			NkRecordingPaint rec;
			RenderDocument(rec, dT, surfT);
			uint32 nPush = 0u, nPop = 0u, texteSous = 0u, profondeur = 0u, pushMax = 0u;
			bool matriceJuste = false, enfantHerite = false;
			float32 a0 = 0.f, b0 = 0.f, c0 = 0.f, d0 = 0.f, e0 = 0.f, f0 = 0.f;
			for (uint32 i = 0; i < (uint32)rec.cmds.Size(); ++i) {
				const NkPaintCmd &c = rec.cmds[i];
				if (c.op == NkPaintOp::PushTransform) {
					++nPush;
					++profondeur;
					if (profondeur > pushMax)
						pushMax = profondeur;
					const float32 co = 0.8660254f, si = 0.5f;
					const bool juste = c.x > 2.f * co - 0.01f && c.x < 2.f * co + 0.01f
									   && c.y > 2.f * si - 0.01f && c.y < 2.f * si + 0.01f
									   && c.w > -si - 0.01f && c.w < -si + 0.01f
									   && c.h > co - 0.01f && c.h < co + 0.01f;
					if (nPush == 1u) {
						matriceJuste = juste;
						a0 = c.x; b0 = c.y; c0 = c.w; d0 = c.h; e0 = c.rounding; f0 = c.tf;
					} else if (nPush == 2u) {
						// l'enfant droit sous un parent tourne : SA matrice effective est
						// celle du parent, translation comprise (composee, pas recopiee)
						enfantHerite = juste && c.rounding > e0 - 0.01f && c.rounding < e0 + 0.01f
									   && c.tf > f0 - 0.01f && c.tf < f0 + 0.01f;
					}
				} else if (c.op == NkPaintOp::PopTransform) {
					++nPop;
					if (profondeur > 0u)
						--profondeur;
				} else if (c.op == NkPaintOp::Text && profondeur > 0u)
					++texteSous;
			}
			(void)a0; (void)b0; (void)c0; (void)d0;
			snprintf(det, sizeof(det),
					 "%u Push / %u Pop, profondeur max %u, 1re matrice juste=%d (a=%.3f b=%.3f "
					 "c=%.3f d=%.3f), enfant herite=%d, Text SOUS matrice=%u",
					 nPush, nPop, pushMax, matriceJuste ? 1 : 0, a0, b0, c0, d0, enfantHerite ? 1 : 0,
					 texteSous);
			check("45b. tourne de 30 degres et double en largeur, le groupe empile LA matrice "
				  "de NkMatDe (rotation x echelle), equilibree Push/Pop, jamais emboitee : chaque "
				  "noeud empile SA matrice effective avant ses enfants",
				  nPush >= 2u && nPush == nPop && pushMax == 1u && matriceJuste, det);
			check("45c. l enfant DROIT d un parent tourne herite la matrice du parent, "
				  "translation comprise -- composee par NkMatEffective, pas recopiee",
				  enfantHerite, det);
			check("45d. LE TEXTE TOURNE : le `Text` du bouton est emis SOUS une matrice non "
				  "identite -- c est le peintre enregistreur qui le prouve, sans fenetre",
				  texteSous > 0u, det);
			// 45e. Le format : additif, aller-retour, et une echelle nulle est bornee.
			NkString avec;
			dT.Save(avec);
			NkUIDocument relu;
			const bool chargee = relu.Load(avec.Data());
			const bool relue = chargee && relu.IsValidIndex(grp)
							   && relu.nodes[(uint32)grp].echelleX > 1.99f
							   && relu.nodes[(uint32)grp].echelleX < 2.01f
							   && relu.nodes[(uint32)grp].echelleY == 1.f;
			dT.nodes[(uint32)grp].echelleX = 1.f;
			NkString sans;
			dT.Save(sans);
			const bool additif = strstr(avec.Data(), "echelle_x = 2") != nullptr
								 && strstr(avec.Data(), "echelle_y") == nullptr
								 && strstr(sans.Data(), "echelle_") == nullptr;
			const bool bornee = NkEchelleSaine(0.f) == 0.001f && NkEchelleSaine(-0.f) == 0.001f
								&& NkEchelleSaine(-3.f) == -3.f && NkEchelleSaine(1e9f) == 1000.f;
			snprintf(det, sizeof(det), "relue=%d additif=%d bornee=%d", relue ? 1 : 0,
					 additif ? 1 : 0, bornee ? 1 : 0);
			check("45e. `echelle_x` / `echelle_y` : additives (absentes a 1), relues a l "
				  "identique, et une echelle nulle est bornee loin de zero (la matrice reste "
				  "inversible : le pointage vit)",
				  relue && additif && bornee, det);
		}
		// ── 46. L'ECHELLE PAR NATURE : « le redimensionnement descend dans l'arbre,
		//     et une feuille est la ou l'arbre s'arrete » (Rodolf). Le banc du
		//     coordinateur : Graphique et ses 11 barres.
		{
			NkUIDocument dG;
			dG.NewDocument("Toile", NkAuthor::Humain);
			dG.nodes[0].layout.kind = NkLayoutKind::Free;
			dG.SetMetric("espacement", 0.f);
			dG.SetMetric("marge", 0.f);
			const int32 graphique = dG.AddChild(0, "", NkAuthor::Humain);
			dG.nodes[(uint32)graphique].shape = NkString("rect");
			dG.nodes[(uint32)graphique].label = NkString("Graphique");
			dG.nodes[(uint32)graphique].layout.kind = NkLayoutKind::Free;
			dG.nodes[(uint32)graphique].posX = 100.f;
			dG.nodes[(uint32)graphique].posY = 100.f;
			dG.nodes[(uint32)graphique].width.mode = NkSizeMode::Fixed;
			dG.nodes[(uint32)graphique].width.value = 220.f;
			dG.nodes[(uint32)graphique].height.mode = NkSizeMode::Fixed;
			dG.nodes[(uint32)graphique].height.value = 100.f;
			int32 barres[11];
			for (int32 b = 0; b < 11; ++b) {
				barres[b] = dG.AddChild(graphique, "", NkAuthor::Humain);
				NkUINode &nb = dG.nodes[(uint32)barres[b]];
				nb.shape = NkString("rect");
				nb.posX = 10.f + 20.f * (float32)b;
				nb.posY = 20.f;
				nb.width.mode = NkSizeMode::Fixed;
				nb.width.value = 12.f;
				nb.height.mode = NkSizeMode::Fixed;
				nb.height.value = 60.f;
			}
			NkPaintRect surfG;
			surfG.x = 0.f;
			surfG.y = 0.f;
			surfG.w = 800.f;
			surfG.h = 600.f;
			NkLayoutResult layG;
			NkComputeLayout(dG, surfG, layG);
			// la boite VISIBLE d'une barre : son rectangle de mise en page passe par
			// la matrice effective (celle que le peintre et le pointage lisent)
			auto visible = [&](int32 i, float32 &cx, float32 &largeur) {
				const NkPaintRect r = layG.At(i);
				float32 xy[8] = {r.x, r.y, r.x + r.w, r.y, r.x + r.w, r.y + r.h, r.x, r.y + r.h};
				NkMatContour(NkMatEffective(dG, layG, i), xy, 4);
				cx = (xy[0] + xy[2]) * 0.5f;
				largeur = xy[2] - xy[0];
			};
			float32 c0[11], l0[11];
			for (int32 b = 0; b < 11; ++b)
				visible(barres[b], c0[b], l0[b]);
			// 46a. le GROUPE double en largeur (ce que la poignee ecrit sur un groupe)
			dG.nodes[(uint32)graphique].echelleX = 2.f;
			uint32 taillesChangees = 0u, ecartsChanges = 0u;
			float32 c1[11], l1[11];
			for (int32 b = 0; b < 11; ++b) {
				visible(barres[b], c1[b], l1[b]);
				if (l1[b] > l0[b] * 1.99f && l1[b] < l0[b] * 2.01f)
					++taillesChangees;
				if (b > 0) {
					const float32 e0 = c0[b] - c0[b - 1], e1 = c1[b] - c1[b - 1];
					if (e1 > e0 * 1.99f && e1 < e0 * 2.01f)
						++ecartsChanges;
				}
			}
			char det[256];
			snprintf(det, sizeof(det), "%u/11 barres deux fois plus larges, %u/10 ecarts doubles "
										"(barre 0 : %.0f -> %.0f de large, centre %.0f -> %.0f)",
					 taillesChangees, ecartsChanges, l0[0], l1[0], c0[0], c1[0]);
			check("46a. redimensionner GRAPHIQUE (son echelle) change la TAILLE et la POSITION "
				  "relative de ses 11 barres, par la matrice -- rien n'est recopie sur elles",
				  taillesChangees == 11u && ecartsChanges == 10u, det);
			// 46b. une FEUILLE se redimensionne seule : la barre 3 double, les autres ne bougent pas
			dG.nodes[(uint32)graphique].echelleX = 1.f;
			dG.nodes[(uint32)barres[3]].width.value = 24.f;
			NkComputeLayout(dG, surfG, layG);
			uint32 immobiles = 0u;
			float32 c3 = 0.f, l3 = 0.f;
			for (int32 b = 0; b < 11; ++b) {
				float32 cx = 0.f, lg = 0.f;
				visible(barres[b], cx, lg);
				if (b == 3) {
					c3 = cx;
					l3 = lg;
				} else if (cx == c0[b] && lg == l0[b])
					++immobiles;
			}
			snprintf(det, sizeof(det), "barre 3 : %.0f de large (centre %.0f) ; %u/10 autres immobiles", l3, c3,
					 immobiles);
			check("46b. redimensionner UNE barre (feuille) ne bouge aucune autre : l'arbre s'arrete a la feuille",
				  l3 == 24.f && immobiles == 10u, det);
			// 46c. le fichier : `echelle_x = 2` sur le groupe seulement, rien sur les barres
			dG.nodes[(uint32)graphique].echelleX = 2.f;
			NkString sG;
			dG.Save(sG);
			uint32 nEch = 0u;
			for (const char *q = sG.Data(); (q = strstr(q, "echelle_x")) != nullptr; ++q)
				++nEch;
			snprintf(det, sizeof(det), "%u cle(s) echelle_x dans le fichier", nEch);
			check("46c. le facteur est PORTE PAR LE GROUPE : une seule cle dans le fichier, les barres n'en ont pas",
				  nEch == 1u, det);
		}
		// ── 47. LE REFUS PAR AXE, dans LA matrice : dessin et pointage lisent la meme
		{
			NkUIDocument dR;
			dR.NewDocument("Toile", NkAuthor::Humain);
			dR.nodes[0].layout.kind = NkLayoutKind::Free;
			dR.SetMetric("espacement", 0.f);
			dR.SetMetric("marge", 0.f);
			const int32 grp = dR.AddChild(0, "", NkAuthor::Humain);
			dR.nodes[(uint32)grp].shape = NkString("rect");
			dR.nodes[(uint32)grp].layout.kind = NkLayoutKind::Free;
			dR.nodes[(uint32)grp].posX = 100.f;
			dR.nodes[(uint32)grp].posY = 100.f;
			dR.nodes[(uint32)grp].width.mode = NkSizeMode::Fixed;
			dR.nodes[(uint32)grp].width.value = 200.f;
			dR.nodes[(uint32)grp].height.mode = NkSizeMode::Fixed;
			dR.nodes[(uint32)grp].height.value = 100.f;
			dR.nodes[(uint32)grp].rotation = 30.f;
			dR.nodes[(uint32)grp].echelleX = 2.f;
			int32 enf[4];
			for (int32 e = 0; e < 4; ++e) {
				enf[e] = dR.AddChild(grp, "", NkAuthor::Humain);
				NkUINode &ne = dR.nodes[(uint32)enf[e]];
				ne.shape = NkString("text");
				ne.text = NkString("Etiquette");
				ne.posX = 10.f + 40.f * (float32)e;
				ne.posY = 10.f;
				ne.width.mode = NkSizeMode::Fixed;
				ne.width.value = 30.f;
				ne.height.mode = NkSizeMode::Fixed;
				ne.height.value = 16.f;
			}
			dR.nodes[(uint32)enf[1]].refusRotation = true;
			dR.nodes[(uint32)enf[2]].refusEchelle = true;
			dR.nodes[(uint32)enf[3]].refusPosition = true;
			NkPaintRect surfR;
			surfR.x = 0.f;
			surfR.y = 0.f;
			surfR.w = 800.f;
			surfR.h = 600.f;
			NkLayoutResult layR;
			NkComputeLayout(dR, surfR, layR);
			const NkMat2D m0 = NkMatEffective(dR, layR, enf[0]);
			const NkMat2D m1 = NkMatEffective(dR, layR, enf[1]);
			const NkMat2D m2 = NkMatEffective(dR, layR, enf[2]);
			const NkMat2D m3 = NkMatEffective(dR, layR, enf[3]);
			const float32 co = 0.8660254f, si = 0.5f;
			// enfant 0 : subit tout (a = 2cos, b = 2sin, c = -sin, d = cos)
			const bool toutSubi = m0.a > 2.f * co - 0.01f && m0.a < 2.f * co + 0.01f && m0.b > 2.f * si - 0.01f
								  && m0.b < 2.f * si + 0.01f;
			// enfant 1 : refuse la rotation -> garde l'echelle du parent, sans rotation (a = 2, b = 0, d = 1)
			const bool sansRotation = m1.a > 1.99f && m1.a < 2.01f && m1.b > -0.001f && m1.b < 0.001f
									  && m1.d > 0.999f && m1.d < 1.001f;
			// enfant 2 : refuse l'echelle -> tourne, mais a = cos (pas 2cos)
			const bool sansEchelle = m2.a > co - 0.01f && m2.a < co + 0.01f && m2.b > si - 0.01f && m2.b < si + 0.01f;
			// enfant 3 : refuse la position -> son centre reste celui de la mise en page
			const NkPaintRect r3 = layR.At(enf[3]);
			float32 x3 = r3.x + r3.w * 0.5f, y3 = r3.y + r3.h * 0.5f;
			const float32 cx3 = x3, cy3 = y3;
			NkMatPoint(m3, x3, y3);
			const bool centreTenu = x3 > cx3 - 0.01f && x3 < cx3 + 0.01f && y3 > cy3 - 0.01f && y3 < cy3 + 0.01f;
			// et l'enfant 0, lui, a bien bouge avec le parent
			const NkPaintRect r0 = layR.At(enf[0]);
			float32 x0 = r0.x + r0.w * 0.5f, y0 = r0.y + r0.h * 0.5f;
			const float32 cx0 = x0;
			NkMatPoint(m0, x0, y0);
			const bool bouge0 = x0 < cx0 - 1.f || x0 > cx0 + 1.f;
			char det[300];
			snprintf(det, sizeof(det),
					 "enfant 0 subit tout=%d (a=%.2f b=%.2f) ; refus R : a=%.2f b=%.2f d=%.2f (=%d) ; refus E : "
					 "a=%.2f b=%.2f (=%d) ; refus P : centre (%.0f,%.0f)->(%.0f,%.0f) tenu=%d, enfant 0 bouge=%d",
					 toutSubi ? 1 : 0, m0.a, m0.b, m1.a, m1.b, m1.d, sansRotation ? 1 : 0, m2.a, m2.b,
					 sansEchelle ? 1 : 0, cx3, cy3, x3, y3, centreTenu ? 1 : 0, bouge0 ? 1 : 0);
			check("47a. LE REFUS PAR AXE est dans la matrice effective : R garde l'echelle sans la rotation, "
				  "E tourne sans l'echelle, P tient son centre -- et le frere qui ne refuse rien subit tout",
				  toutSubi && sansRotation && sansEchelle && centreTenu && bouge0, det);
			// 47b. le POINTAGE lit la meme matrice : le centre visible de l'enfant 0 (deplace) est
			// dedans ; son centre de mise en page (d'ou il est parti) n'y est plus
			const bool dedansVisible = NkPointDansNoeud(dR, layR, enf[0], x0, y0);
			const bool partiDeLa = !NkPointDansNoeud(dR, layR, enf[0], cx0, r0.y + r0.h * 0.5f);
			// et l'enfant 3 (refus P) s'attrape la ou il est reste
			const bool attrapeSurPlace = NkPointDansNoeud(dR, layR, enf[3], cx3, cy3);
			snprintf(det, sizeof(det), "enfant 0 : dedans la ou il se voit=%d, plus la d'ou il est parti=%d ; "
										"enfant 3 (refus P) attrape sur place=%d",
					 dedansVisible ? 1 : 0, partiDeLa ? 1 : 0, attrapeSurPlace ? 1 : 0);
			check("47b. le POINTAGE honore les refus sans second code : il lit la matrice du dessin",
				  dedansVisible && partiDeLa && attrapeSurPlace, det);
			// 47c. le peintre enregistreur : l'enfant qui refuse la rotation est emis SOUS une
			// matrice sans rotation (b = 0), le frere sous une matrice tournee
			NkRecordingPaint recR;
			RenderDocument(recR, dR, surfR);
			uint32 pushTournes = 0u, pushDroits = 0u;
			for (uint32 i = 0; i < (uint32)recR.cmds.Size(); ++i) {
				const NkPaintCmd &c = recR.cmds[i];
				if (c.op != NkPaintOp::PushTransform)
					continue;
				if (c.y > -0.001f && c.y < 0.001f)
					++pushDroits;
				else
					++pushTournes;
			}
			snprintf(det, sizeof(det), "%u matrice(s) tournee(s), %u droite(s) (le refus R, echelle 2 sans rotation)",
					 pushTournes, pushDroits);
			check("47c. a l'ecran (peintre enregistreur) : le refus R dessine droit sous un parent tourne, "
				  "ses freres tournent",
				  pushTournes >= 3u && pushDroits == 1u, det);
			// 47d. le format : trois cles additives, relues
			NkString sR;
			dR.Save(sR);
			NkUIDocument reluR;
			const bool chargeeR = reluR.Load(sR.Data());
			const bool relusR = chargeeR && reluR.IsValidIndex(enf[3]) && reluR.nodes[(uint32)enf[1]].refusRotation
								&& reluR.nodes[(uint32)enf[2]].refusEchelle && reluR.nodes[(uint32)enf[3]].refusPosition
								&& !reluR.nodes[(uint32)enf[0]].refusRotation;
			const bool additifsR = strstr(sR.Data(), "refus_rotation = 1") != nullptr
								   && strstr(sR.Data(), "refus_echelle = 1") != nullptr
								   && strstr(sR.Data(), "refus_position = 1") != nullptr;
			snprintf(det, sizeof(det), "relus=%d additifs=%d", relusR ? 1 : 0, additifsR ? 1 : 0);
			check("47d. `refus_position` / `refus_rotation` / `refus_echelle` : additives, relues a l'identique",
				  relusR && additifsR, det);
		}
		// La GEOMETRIE PEINTE d'un enregistrement : chaque commande (hors Push/Pop)
		// avec ses quatre coins passes par la matrice en vigueur. Deux documents qui
		// donnent la meme liste peignent les memes pixels -- la comptabilite Push/Pop
		// n'en fait pas partie.
		struct GeoPeinte {
				static void Extraire(const NkRecordingPaint &r, NkVector<float32> &out) {
					float32 pile[16][6];
					int32 sp = 0;
					for (uint32 i = 0; i < (uint32)r.cmds.Size(); ++i) {
						const NkPaintCmd &c = r.cmds[i];
						if (c.op == NkPaintOp::PushTransform) {
							if (sp < 16) {
								pile[sp][0] = c.x;
								pile[sp][1] = c.y;
								pile[sp][2] = c.w;
								pile[sp][3] = c.h;
								pile[sp][4] = c.rounding;
								pile[sp][5] = c.tf;
							}
							++sp;
							continue;
						}
						if (c.op == NkPaintOp::PopTransform) {
							if (sp > 0)
								--sp;
							continue;
						}
						float32 m[6] = {1.f, 0.f, 0.f, 1.f, 0.f, 0.f};
						if (sp > 0 && sp <= 16)
							for (int32 k = 0; k < 6; ++k)
								m[k] = pile[sp - 1][k];
						const float32 xs[4] = {c.x, c.x + c.w, c.x + c.w, c.x};
						const float32 ys[4] = {c.y, c.y, c.y + c.h, c.y + c.h};
						out.PushBack((float32)(uint8)c.op);
						out.PushBack((float32)c.role);
						out.PushBack((float32)(c.rgba >> 8));
						for (int32 k = 0; k < 4; ++k) {
							out.PushBack(m[0] * xs[k] + m[2] * ys[k] + m[4]);
							out.PushBack(m[1] * xs[k] + m[3] * ys[k] + m[5]);
						}
					}
				}
				static bool Memes(const NkRecordingPaint &a, const NkRecordingPaint &b, uint32 &premiere) {
					NkVector<float32> ga, gb;
					Extraire(a, ga);
					Extraire(b, gb);
					premiere = 0u;
					if (ga.Size() != gb.Size())
						return false;
					for (uint32 i = 0; i < (uint32)ga.Size(); ++i) {
						const float32 d = ga[i] - gb[i];
						if (d > 0.05f || d < -0.05f) {
							premiere = i / 11u;
							return false;
						}
					}
					return true;
				}
		};
		// ── 48. FEUILLES ET GROUPES : la cle, la garde, l'ENVELOPPEMENT pixel pour pixel
		{
			NkUIDocument dF;
			dF.NewDocument("Toile", NkAuthor::Humain);
			dF.nodes[0].layout.kind = NkLayoutKind::Free;
			dF.SetMetric("espacement", 0.f);
			dF.SetMetric("marge", 0.f);
			const int32 bouton = dF.AddChild(0, "", NkAuthor::Humain);
			dF.nodes[(uint32)bouton].shape = NkString("rect");
			dF.nodes[(uint32)bouton].label = NkString("Bouton_Connexion");
			dF.nodes[(uint32)bouton].role = NkString("Button");
			dF.nodes[(uint32)bouton].layout.kind = NkLayoutKind::Free;
			dF.nodes[(uint32)bouton].posX = 30.f;
			dF.nodes[(uint32)bouton].posY = 316.f;
			dF.nodes[(uint32)bouton].width.mode = NkSizeMode::Fixed;
			dF.nodes[(uint32)bouton].width.value = 180.f;
			dF.nodes[(uint32)bouton].height.mode = NkSizeMode::Fixed;
			dF.nodes[(uint32)bouton].height.value = 36.f;
			dF.nodes[(uint32)bouton].rotation = 12.f;
			const int32 texte = dF.AddChild(bouton, "", NkAuthor::Humain);
			dF.nodes[(uint32)texte].shape = NkString("text");
			dF.nodes[(uint32)texte].label = NkString("Texte du bouton");
			dF.nodes[(uint32)texte].text = NkString("Se connecter");
			dF.nodes[(uint32)texte].posX = 0.f;
			dF.nodes[(uint32)texte].posY = 10.f;
			dF.nodes[(uint32)texte].width.mode = NkSizeMode::Fixed;
			dF.nodes[(uint32)texte].width.value = 180.f;
			dF.nodes[(uint32)texte].height.mode = NkSizeMode::Fixed;
			dF.nodes[(uint32)texte].height.value = 16.f;
			const int32 apres = dF.AddChild(0, "", NkAuthor::Humain); // un frere APRES, pour le rang
			dF.nodes[(uint32)apres].shape = NkString("rect");
			dF.nodes[(uint32)apres].posX = 300.f;
			dF.nodes[(uint32)apres].posY = 10.f;
			dF.nodes[(uint32)apres].width.mode = NkSizeMode::Fixed;
			dF.nodes[(uint32)apres].width.value = 20.f;
			dF.nodes[(uint32)apres].height.mode = NkSizeMode::Fixed;
			dF.nodes[(uint32)apres].height.value = 20.f;
			NkPaintRect surfF;
			surfF.x = 0.f;
			surfF.y = 0.f;
			surfF.w = 800.f;
			surfF.h = 600.f;
			char det[300];
			// 48a. la cle et l'inference
			NkUINode vide;
			vide.shape = NkString("rect");
			NkUINode groupeVide;
			groupeVide.genre = NkString("simple");
			NkUINode planche;
			planche.shape = NkString("frame");
			NkUINode inconnu;
			inconnu.genre = NkString("animation");
			NkString sI;
			dF.nodes[(uint32)apres].genre = NkString("animation");
			dF.Save(sI);
			NkUIDocument reluI;
			const bool inconnuGarde = reluI.Load(sI.Data()) && reluI.IsValidIndex(apres)
									  && NkComponentDecl::StrEq(reluI.nodes[(uint32)apres].genre.Data(), "animation")
									  && strstr(sI.Data(), "groupe = animation") != nullptr;
			dF.nodes[(uint32)apres].genre = NkString();
			snprintf(det, sizeof(det), "rect nu=feuille:%d, groupe vide declare=groupe:%d, planche=groupe:%d, "
										"genre inconnu=groupe:%d et preserve au fichier:%d, rect a enfants=groupe (infere):%d",
					 !NkEstGroupe(vide) ? 1 : 0, NkEstGroupe(groupeVide) ? 1 : 0, NkEstGroupe(planche) ? 1 : 0,
					 NkEstGroupe(inconnu) ? 1 : 0, inconnuGarde ? 1 : 0, NkEstGroupe(dF.nodes[(uint32)bouton]) ? 1 : 0);
			check("48a. `groupe = <genre>` : additive, absente = inferee, un groupe VIDE reste un groupe, "
				  "une planche est un groupe, un genre inconnu se relit et se reemet intact",
				  !NkEstGroupe(vide) && NkEstGroupe(groupeVide) && NkEstGroupe(planche) && NkEstGroupe(inconnu)
					  && inconnuGarde && NkEstGroupe(dF.nodes[(uint32)bouton]),
				  det);
			// 48b. l'enveloppement : pixel pour pixel (peintre enregistreur), rang et comptes
			NkRecordingPaint avant;
			RenderDocument(avant, dF, surfF);
			const uint32 nAvant = (uint32)dF.nodes.Size();
			NkVector<int32> env;
			const uint32 nEnv = NkEnvelopperGraphiques(dF, &env);
			NkRecordingPaint apresR;
			RenderDocument(apresR, dF, surfF);
			uint32 premiereDiff = 0u;
			const bool memesCommandes = GeoPeinte::Memes(avant, apresR, premiereDiff);
			const int32 gi = env.Empty() ? -1 : env[0];
			const bool structure = nEnv == 1u && dF.IsValidIndex(gi) && (uint32)dF.nodes.Size() == nAvant + 1u
								   && dF.nodes[(uint32)gi].children.Size() == 2u
								   && dF.nodes[(uint32)gi].children[0] == bouton
								   && dF.nodes[(uint32)gi].children[1] == texte
								   && dF.nodes[0].children.Size() == 2u && dF.nodes[0].children[0] == gi
								   && dF.nodes[0].children[1] == apres
								   && NkComponentDecl::StrEq(dF.nodes[(uint32)gi].genre.Data(), "simple")
								   && NkComponentDecl::StrEq(dF.nodes[(uint32)gi].label.Data(), "Bouton_Connexion")
								   && NkComponentDecl::StrEq(dF.nodes[(uint32)gi].role.Data(), "Button")
								   && dF.nodes[(uint32)gi].rotation == 12.f && dF.nodes[(uint32)bouton].rotation == 0.f
								   && dF.nodes[(uint32)bouton].children.Empty();
			snprintf(det, sizeof(det), "%u enveloppement(s), %u -> %u noeuds, %u commandes avant / %u apres, "
										"geometrie peinte identique=%d (1re difference : commande %u), structure=%d",
					 nEnv, nAvant, (uint32)dF.nodes.Size(), (uint32)avant.cmds.Size(), (uint32)apresR.cmds.Size(),
					 memesCommandes ? 1 : 0, premiereDiff, structure ? 1 : 0);
			check("48b. ENVELOPPER un graphique a enfants : un groupe prend sa place, sa boite, son etiquette, "
				  "son role et sa rotation ; il devient sa premiere feuille ; l'enfant suit -- et le peintre "
				  "emet EXACTEMENT les memes commandes (rien n'a bouge a l'ecran)",
				  memesCommandes && structure, det);
			// 48c. la migration ne se refait pas : un second passage n'enveloppe rien
			const uint32 nEnv2 = NkEnvelopperGraphiques(dF, nullptr);
			snprintf(det, sizeof(det), "second passage : %u", nEnv2);
			check("48c. un document deja migre ne bouge plus (second passage : 0)", nEnv2 == 0u, det);
			// 48d. la garde : sous le point, la feuille `apres` -- le conteneur rendu est la page, pas elle
			NkLayoutResult layF;
			NkComputeLayout(dF, surfF, layF);
			const NkPaintRect ra = layF.At(apres);
			const int32 conteneur = NkPickFreeContainer(dF, layF, ra.x + ra.w * 0.5f, ra.y + ra.h * 0.5f);
			const NkPaintRect rg = layF.At(gi);
			const int32 conteneurG = NkPickFreeContainer(dF, layF, rg.x + 2.f, rg.y + 2.f);
			snprintf(det, sizeof(det), "sur la feuille : conteneur=%d (page=0) ; sur le groupe : conteneur=%d (groupe=%d)",
					 conteneur, conteneurG, gi);
			check("48d. LA GARDE : la pose, le depot et la creation ne rendent jamais une feuille comme conteneur "
				  "-- le groupe, lui, en est un",
				  conteneur == 0 && conteneurG == gi, det);
			// 48e. le document de Rodolf, en LECTURE SEULE : six graphiques a enfants, et rien ne bouge
			{
				FILE *fp = fopen("nkuidesign_document.nkuidoc", "rb");
				if (fp) {
					fseek(fp, 0, SEEK_END);
					const long taille = ftell(fp);
					fseek(fp, 0, SEEK_SET);
					NkString contenu;
					if (taille > 0) {
						char *buf = new char[(size_t)taille + 1];
						const size_t lu = fread(buf, 1, (size_t)taille, fp);
						buf[lu] = 0;
						contenu = NkString(buf);
						delete[] buf;
					}
					fclose(fp);
					NkUIDocument dRod;
					if (dRod.Load(contenu.Data())) {
						NkPaintRect surfRod;
						surfRod.x = 0.f;
						surfRod.y = 0.f;
						surfRod.w = 1600.f;
						surfRod.h = 1000.f;
						NkRecordingPaint rAvant;
						RenderDocument(rAvant, dRod, surfRod);
						NkVector<int32> envR;
						const uint32 nR = NkEnvelopperGraphiques(dRod, &envR);
						NkRecordingPaint rApres;
						RenderDocument(rApres, dRod, surfRod);
						uint32 diff = 0u;
						const bool memes = GeoPeinte::Memes(rAvant, rApres, diff);
						NkString noms;
						for (uint32 k = 0; k < (uint32)envR.Size(); ++k) {
							if (k)
								noms.Append(", ");
							noms.Append(dRod.nodes[(uint32)envR[k]].label);
						}
						snprintf(det, sizeof(det), "%u enveloppe(s) : %s ; %u commandes, geometrie peinte identique=%d (1re diff : commande %u)",
								 nR, noms.Data(), (uint32)rAvant.cmds.Size(), memes ? 1 : 0, diff);
						check("48e. LE DOCUMENT DE RODOLF (lecture seule) : ses six graphiques a enfants "
							  "s'enveloppent, et le peintre emet les memes commandes avant et apres",
							  nR == 6u && memes, det);
					}
				}
			}
		}
		// ── 49. LE DEGRADE SUIT LA FORME et honore son angle ─────────────────
		{
			// un enregistreur qui CAPTE les polygones (le kit n'en enregistre pas)
			struct PeintrePoly : public NkRecordingPaint {
					NkVector<float32> pts;	 ///< x,y a la suite
					NkVector<int32> tailles; ///< points par polygone
					NkVector<uint32> couleurs;
					bool PolygonHex(const float32 *xy, int32 count, uint32 rgba) override {
						for (int32 i = 0; i < count * 2; ++i)
							pts.PushBack(xy[i]);
						tailles.PushBack(count);
						couleurs.PushBack(rgba);
						return true;
					}
			};
			NkUIDocument dD;
			dD.NewDocument("Toile", NkAuthor::Humain);
			dD.nodes[0].layout.kind = NkLayoutKind::Free;
			dD.SetMetric("espacement", 0.f);
			dD.SetMetric("marge", 0.f);
			const int32 f = dD.AddChild(0, "", NkAuthor::Humain);
			NkUINode &nf = dD.nodes[(uint32)f];
			nf.shape = NkString("rect");
			nf.posX = 100.f;
			nf.posY = 100.f;
			nf.width.mode = NkSizeMode::Fixed;
			nf.width.value = 120.f;
			nf.height.mode = NkSizeMode::Fixed;
			nf.height.value = 60.f;
			nf.radius = 20.f;
			NkRemplissage rf;
			rf.couleur = NkString("#ff0000");
			NkArretDegrade s0, s1;
			s0.position = 0.f;
			s0.couleur = NkString("#ff0000");
			s1.position = 1.f;
			s1.couleur = NkString("#0000ff");
			rf.degrade.type = NkString("lineaire");
			rf.degrade.arrets.PushBack(s0);
			rf.degrade.arrets.PushBack(s1);
			nf.fills.PushBack(rf);
			NkPaintRect surfD;
			surfD.x = 0.f;
			surfD.y = 0.f;
			surfD.w = 800.f;
			surfD.h = 600.f;
			const float32 X0 = 100.f, Y0 = 100.f, W = 120.f, H = 60.f, R = 20.f;
			// dedans le contour arrondi, a 0.6 px pres
			auto dedans = [&](float32 x, float32 y) -> bool {
				const float32 tol = 0.6f;
				if (x < X0 - tol || x > X0 + W + tol || y < Y0 - tol || y > Y0 + H + tol)
					return false;
				const float32 qx = x < X0 + R ? X0 + R : (x > X0 + W - R ? X0 + W - R : x);
				const float32 qy = y < Y0 + R ? Y0 + R : (y > Y0 + H - R ? Y0 + H - R : y);
				const float32 ddx = x - qx, ddy = y - qy;
				return ddx * ddx + ddy * ddy <= (R + tol) * (R + tol);
			};
			auto mesurer = [&](PeintrePoly &pp, uint32 &nPoly, uint32 &horsContour, float32 &yMin,
							   float32 &yMax, float32 &xMin, float32 &xMax, float32 &xMaxPremier,
							   float32 &yMaxPremier) {
				nPoly = (uint32)pp.tailles.Size();
				horsContour = 0u;
				yMin = 1e9f; yMax = -1e9f; xMin = 1e9f; xMax = -1e9f; xMaxPremier = -1e9f; yMaxPremier = -1e9f;
				uint32 k = 0;
				for (uint32 i = 0; i < nPoly; ++i) {
					for (int32 j = 0; j < pp.tailles[i]; ++j, ++k) {
						const float32 x = pp.pts[k * 2], y = pp.pts[k * 2 + 1];
						if (!dedans(x, y))
							++horsContour;
						if (y < yMin) yMin = y;
						if (y > yMax) yMax = y;
						if (x < xMin) xMin = x;
						if (x > xMax) xMax = x;
						if (i == 0u) {
							if (x > xMaxPremier) xMaxPremier = x;
							if (y > yMaxPremier) yMaxPremier = y;
						}
					}
				}
			};
			char det[300];
			// 49a. angle 0 (haut -> bas) : 24 bandes, toutes DANS le contour arrondi, qui couvrent la forme
			PeintrePoly p0;
			RenderDocument(p0, dD, surfD);
			uint32 nP = 0u, hors = 0u;
			float32 yMin, yMax, xMin, xMax, xMaxP, yMaxP;
			mesurer(p0, nP, hors, yMin, yMax, xMin, xMax, xMaxP, yMaxP);
			// la couleur est prise au MILIEU de la bande : la premiere est rouge DOMINANT,
			// pas rouge pur -- et la derniere atteint vraiment le dernier arret
			const bool premiereRouge = !p0.couleurs.Empty() && ((p0.couleurs[0] >> 24) & 0xFFu) > 200u
									   && ((p0.couleurs[0] >> 8) & 0xFFu) < 60u;
			const bool derniereBleue = !p0.couleurs.Empty() && ((p0.couleurs[(uint32)p0.couleurs.Size() - 1u] >> 8) & 0xFFu) > 0xF0u;
			snprintf(det, sizeof(det), "%u polygone(s), %u point(s) hors contour, y %.1f..%.1f, x %.1f..%.1f, 1re bande : y max %.1f (rouge=%d), derniere bleue=%d",
					 nP, hors, yMin, yMax, xMin, xMax, yMaxP, premiereRouge ? 1 : 0, derniereBleue ? 1 : 0);
			check("49a. LE DEGRADE SUIT L'ARRONDI : une bande par 2 px, en polygones, aucun point hors du contour arrondi, "
				  "la forme couverte de haut en bas, du rouge au bleu",
				  nP == (uint32)renderdetail::NkBandesDegrade(H) && hors == 0u && yMin < Y0 + 0.6f
					  && yMax > Y0 + H - 0.6f && xMin < X0 + 0.6f && xMax > X0 + W - 0.6f
					  && yMaxP < Y0 + H / (float32)renderdetail::NkBandesDegrade(H) + 0.6f && premiereRouge
					  && derniereBleue,
				  det);
			// 49b. la premiere bande (dans l'arc) est PLUS ETROITE que la forme : elle ne sort pas des coins
			float32 xMinP = 1e9f;
			for (int32 j = 0; j < p0.tailles[0]; ++j)
				if (p0.pts[j * 2] < xMinP)
					xMinP = p0.pts[j * 2];
			snprintf(det, sizeof(det), "1re bande : x %.1f..%.1f (forme %.0f..%.0f, rayon %.0f)", xMinP, xMaxP, X0, X0 + W, R);
			check("49b. la bande du haut vit ENTRE les deux arcs : plus etroite que la forme des deux cotes",
				  xMinP > X0 + 5.f && xMaxP < X0 + W - 5.f, det);
			// 49c. angle 270 (gauche -> droite) : l'axe tourne, les bandes deviennent verticales
			dD.nodes[(uint32)f].fills[0].degrade.angle = 270.f;
			PeintrePoly p270;
			RenderDocument(p270, dD, surfD);
			mesurer(p270, nP, hors, yMin, yMax, xMin, xMax, xMaxP, yMaxP);
			snprintf(det, sizeof(det), "%u polygone(s), %u hors contour, 1re bande : x max %.1f (largeur de bande %.1f), y max %.1f",
					 nP, hors, xMaxP, W / 24.f, yMaxP);
			check("49c. l'ANGLE est honore : a 270 (gauche -> droite) la premiere bande est une tranche VERTICALE a gauche, "
				  "toujours dans le contour",
				  nP == (uint32)renderdetail::NkBandesDegrade(W) && hors == 0u
					  && xMaxP < X0 + W / (float32)renderdetail::NkBandesDegrade(W) + 0.6f && yMaxP > Y0 + H - 12.f,
				  det);
			// 49d. sans arrondi : les bandes sont des rectangles pleine largeur (rien n'a change pour un rect droit)
			dD.nodes[(uint32)f].fills[0].degrade.angle = 0.f;
			dD.nodes[(uint32)f].radius = 0.f;
			PeintrePoly pDroit;
			RenderDocument(pDroit, dD, surfD);
			bool quatrePoints = pDroit.tailles.Size() == (uint32)renderdetail::NkBandesDegrade(H);
			for (uint32 i = 0; quatrePoints && i < (uint32)pDroit.tailles.Size(); ++i)
				if (pDroit.tailles[i] != 4)
					quatrePoints = false;
			mesurer(pDroit, nP, hors, yMin, yMax, xMin, xMax, xMaxP, yMaxP);
			snprintf(det, sizeof(det), "%u polygone(s) a 4 points=%d, x %.1f..%.1f", nP, quatrePoints ? 1 : 0, xMin, xMax);
			check("49d. sans arrondi, des rectangles pleine largeur (un par 2 px) : un rect droit garde son degrade",
				  quatrePoints && xMin < X0 + 0.6f && xMax > X0 + W - 0.6f, det);
			// 49e. un peintre SANS polygone (l'enregistreur du kit) retombe sur les bandes d'avant : rien ne casse
			dD.nodes[(uint32)f].radius = 20.f;
			NkRecordingPaint sansPoly;
			RenderDocument(sansPoly, dD, surfD);
			uint32 bandes = 0u;
			for (uint32 i = 0; i < (uint32)sansPoly.cmds.Size(); ++i)
				if (sansPoly.cmds[i].op == NkPaintOp::FillColor && sansPoly.cmds[i].w > W - 0.6f && sansPoly.cmds[i].h < H / 24.f + 1.f)
					++bandes;
			snprintf(det, sizeof(det), "%u bande(s) rectangulaires chez un peintre sans polygone", bandes);
			check("49e. un peintre sans polygone retombe sur des bandes rectangulaires, MEME calcul de couleur (le repli est nomme)",
				  bandes == (uint32)renderdetail::NkBandesDegrade(H), det);
		}
		// ── 50. LES OMBRES PAR COIN ──────────────────────────────────────────
		{
			NkUIDocument dO;
			dO.NewDocument("Toile", NkAuthor::Humain);
			dO.nodes[0].layout.kind = NkLayoutKind::Free;
			dO.SetMetric("espacement", 0.f);
			dO.SetMetric("marge", 0.f);
			const int32 f = dO.AddChild(0, "", NkAuthor::Humain);
			NkUINode &nf = dO.nodes[(uint32)f];
			nf.shape = NkString("rect");
			nf.posX = 100.f;
			nf.posY = 100.f;
			nf.width.mode = NkSizeMode::Fixed;
			nf.width.value = 120.f;
			nf.height.mode = NkSizeMode::Fixed;
			nf.height.value = 60.f;
			nf.rayonsDelies = true;
			nf.rayonsCoins[0] = 20.f; // haut-gauche arrondi, les trois autres droits
			NkEffet ombre;
			ombre.type = NkEffetType::OmbrePortee;
			ombre.x = 0.f;
			ombre.y = 4.f;
			ombre.flou = 0.f;
			ombre.etendue = 0.f;
			ombre.couleur = NkString("#102030");
			ombre.opacite = 50.f;
			nf.effets.PushBack(ombre);
			NkPaintRect surfO;
			surfO.x = 0.f;
			surfO.y = 0.f;
			surfO.w = 800.f;
			surfO.h = 600.f;
			NkRecordingPaint rec;
			RenderDocument(rec, dO, surfO);
			uint32 nOmbre = 0u, arrondis20 = 0u, droits = 0u, autres = 0u;
			for (uint32 i = 0; i < (uint32)rec.cmds.Size(); ++i) {
				const NkPaintCmd &c = rec.cmds[i];
				if (c.op != NkPaintOp::FillColor || (c.rgba >> 8) != 0x102030u)
					continue;
				++nOmbre;
				if (c.rounding > 19.5f && c.rounding < 20.5f)
					++arrondis20;
				else if (c.rounding == 0.f)
					++droits;
				else
					++autres;
			}
			char det[220];
			snprintf(det, sizeof(det), "%u commande(s) d'ombre : %u arrondie(s) a 20, %u droite(s), %u autre(s)",
					 nOmbre, arrondis20, droits, autres);
			check("50a. L'OMBRE SUIT LES COINS : un seul coin arrondi (20) donne une piece d'ombre arrondie a 20 "
				  "et des pieces droites -- plus de rayon uniforme invente (4)",
				  nOmbre > 1u && arrondis20 == 1u && droits >= 1u && autres == 0u, det);
			// 50b. quatre coins egaux : une seule piece, au rayon du noeud (rien n'a change pour l'uniforme)
			nf.rayonsDelies = false;
			nf.radius = 8.f;
			NkRecordingPaint rec2;
			RenderDocument(rec2, dO, surfO);
			uint32 n2 = 0u, r8 = 0u;
			for (uint32 i = 0; i < (uint32)rec2.cmds.Size(); ++i) {
				const NkPaintCmd &c = rec2.cmds[i];
				if (c.op != NkPaintOp::FillColor || (c.rgba >> 8) != 0x102030u)
					continue;
				++n2;
				if (c.rounding > 7.5f && c.rounding < 8.5f)
					++r8;
			}
			snprintf(det, sizeof(det), "%u commande(s), %u au rayon 8", n2, r8);
			check("50b. quatre coins egaux : une seule piece d'ombre, au rayon du noeud", n2 == 1u && r8 == 1u, det);
		}
		// ── 51. BORDURES PAR COTE, JOINTURES, EXTREMITES ─────────────────────
		{
			struct PeintrePoly51 : public NkRecordingPaint {
					NkVector<int32> tailles;
					NkVector<float32> pts;
					bool PolygonHex(const float32 *xy, int32 count, uint32 rgba) override {
						(void)rgba;
						for (int32 i = 0; i < count * 2; ++i)
							pts.PushBack(xy[i]);
						tailles.PushBack(count);
						return true;
					}
			};
			char det[300];
			NkUIDocument dB;
			dB.NewDocument("Toile", NkAuthor::Humain);
			dB.nodes[0].layout.kind = NkLayoutKind::Free;
			dB.SetMetric("espacement", 0.f);
			dB.SetMetric("marge", 0.f);
			const int32 f = dB.AddChild(0, "", NkAuthor::Humain);
			{
				NkUINode &nf = dB.nodes[(uint32)f];
				nf.shape = NkString("rect");
				nf.posX = 100.f;
				nf.posY = 100.f;
				nf.width.mode = NkSizeMode::Fixed;
				nf.width.value = 100.f;
				nf.height.mode = NkSizeMode::Fixed;
				nf.height.value = 50.f;
				NkRemplissage rf;
				rf.couleur = NkString("#ffffff");
				nf.fills.PushBack(rf);
				NkBordure b;
				b.couleur = NkString("#123456");
				b.epaisseur = 1.f;
				b.position = NkBordurePos::Interieur;
				b.cotes[0] = 4.f; // le haut plus epais
				nf.borders.PushBack(b);
			}
			NkPaintRect surfB;
			surfB.x = 0.f;
			surfB.y = 0.f;
			surfB.w = 800.f;
			surfB.h = 600.f;
			// 51a. le format : cotes / jointure / extremite additifs, et un jeton inconnu preserve
			{
				NkString s1;
				dB.Save(s1);
				const bool cle = strstr(s1.Data(), "cotes=4,1,1,1") != nullptr && strstr(s1.Data(), "jointure=") == nullptr;
				// on glisse un jeton inconnu au bout de la ligne bord_1
				NkString s2;
				const char *pos = strstr(s1.Data(), "cotes=4,1,1,1");
				const uint32 coupe = (uint32)(pos - s1.Data()) + 13u;
				for (uint32 i = 0; i < coupe; ++i)
					s2.Append(s1.Data()[i]);
				s2.Append(" jointure=rond extremite=carree futur=x");
				s2.Append(s1.Data() + coupe);
				NkUIDocument relu;
				const bool ok = relu.Load(s2.Data());
				NkString s3;
				if (ok)
					relu.Save(s3);
				const NkBordure *rb = (ok && relu.IsValidIndex(f) && !relu.nodes[(uint32)f].borders.Empty())
										  ? &relu.nodes[(uint32)f].borders[0] : nullptr;
				const bool relus = rb && rb->Cote(0) == 4.f && rb->Cote(1) == 1.f
								   && NkComponentDecl::StrEq(rb->jointure.Data(), "rond")
								   && NkComponentDecl::StrEq(rb->extremite.Data(), "carree")
								   && NkComponentDecl::StrEq(rb->inconnus.Data(), "futur=x");
				const bool reemis = ok && strstr(s3.Data(), "jointure=rond") != nullptr
									&& strstr(s3.Data(), "extremite=carree") != nullptr && strstr(s3.Data(), "futur=x") != nullptr;
				snprintf(det, sizeof(det), "cle=%d relus=%d reemis=%d", cle ? 1 : 0, relus ? 1 : 0, reemis ? 1 : 0);
				check("51a. `bord_` gagne cotes= / jointure= / extremite=, additifs (rien au defaut), et un jeton "
					  "inconnu (`futur=x`) se relit et se reemet intact",
					  cle && relus && reemis, det);
			}
			// 51b. le peintre : le haut a 4, les autres a 1 -- l'interieur creuse est decale de 4 en haut, de 1 ailleurs
			{
				NkRecordingPaint rec;
				RenderDocument(rec, dB, surfB);
				bool creuxJuste = false;
				uint32 nBlancs = 0u;
				for (uint32 i = 0; i < (uint32)rec.cmds.Size(); ++i) {
					const NkPaintCmd &c = rec.cmds[i];
					if (c.op != NkPaintOp::FillColor || (c.rgba >> 8) != 0xFFFFFFu)
						continue;
					++nBlancs;
					if (c.x == 101.f && c.y == 104.f && c.w == 98.f && c.h == 45.f)
						creuxJuste = true;
				}
				snprintf(det, sizeof(det), "%u remplissage(s) blanc(s), creux (101,104) 98x45 trouve=%d", nBlancs, creuxJuste ? 1 : 0);
				check("51b. QUATRE EPAISSEURS : le creux interieur est decale de 4 en haut et de 1 sur les trois autres cotes",
					  creuxJuste, det);
			}
			// 51c. jointure ronde aux coins droits : l'exterieur s'arrondit de l'epaisseur (ici 4, le max des cotes)
			{
				dB.nodes[(uint32)f].borders[0].jointure = NkString("rond");
				NkRecordingPaint rec;
				RenderDocument(rec, dB, surfB);
				uint32 arrondis = 0u;
				for (uint32 i = 0; i < (uint32)rec.cmds.Size(); ++i) {
					const NkPaintCmd &c = rec.cmds[i];
					if (c.op == NkPaintOp::FillColor && (c.rgba >> 8) == 0x123456u && c.rounding > 0.f)
						++arrondis;
				}
				snprintf(det, sizeof(det), "%u piece(s) de cadre arrondie(s)", arrondis);
				check("51c. JOINTURE RONDE : sur un rect droit, le cadre exterieur s'arrondit de l'epaisseur du coin", arrondis >= 1u, det);
			}
			// 51d. biseau : un polygone a 8 points (coins coupes) chez un peintre a polygone ; l'onglet sinon
			{
				dB.nodes[(uint32)f].borders[0].jointure = NkString("biseau");
				PeintrePoly51 pp;
				RenderDocument(pp, dB, surfB);
				bool octogone = false;
				for (uint32 i = 0; i < (uint32)pp.tailles.Size(); ++i)
					if (pp.tailles[i] == 8)
						octogone = true;
				NkRecordingPaint sans;
				RenderDocument(sans, dB, surfB);
				uint32 cadreDroit = 0u;
				for (uint32 i = 0; i < (uint32)sans.cmds.Size(); ++i)
					if (sans.cmds[i].op == NkPaintOp::FillColor && (sans.cmds[i].rgba >> 8) == 0x123456u && sans.cmds[i].rounding == 0.f)
						++cadreDroit;
				snprintf(det, sizeof(det), "octogone=%d ; sans polygone : %u piece(s) droite(s) (onglet)", octogone ? 1 : 0, cadreDroit);
				check("51d. BISEAU : l'exterieur est un octogone (coins coupes) ; sans polygone, repli sur l'onglet, dit au code",
					  octogone && cadreDroit >= 1u, det);
				dB.nodes[(uint32)f].borders[0].jointure = NkString();
			}
			// 51e. extremites d'une LIGNE : ronde = deux disques ; carree = prolongee d'une demi-epaisseur ; plate = rien
			{
				const int32 li = dB.AddChild(0, "", NkAuthor::Humain);
				NkUINode &nl = dB.nodes[(uint32)li];
				nl.shape = NkString("line");
				nl.posX = 300.f;
				nl.posY = 100.f;
				nl.width.mode = NkSizeMode::Fixed;
				nl.width.value = 100.f;
				nl.height.mode = NkSizeMode::Fixed;
				nl.height.value = 20.f; // une diagonale : une hauteur nulle est rejetee avant la branche
				NkBordure bl;
				bl.couleur = NkString("#123456");
				bl.epaisseur = 4.f;
				nl.borders.PushBack(bl);
				auto etendueLigne = [&](const char *ext, uint32 &disques, float32 &xMin, float32 &xMax) {
					dB.nodes[(uint32)li].borders[0].extremite = NkString(ext);
					PeintrePoly51 pp;
					RenderDocument(pp, dB, surfB);
					disques = 0u;
					xMin = 1e9f;
					xMax = -1e9f;
					uint32 k = 0;
					for (uint32 i = 0; i < (uint32)pp.tailles.Size(); ++i) {
						if (pp.tailles[i] == 16)
							++disques;
						for (int32 j2 = 0; j2 < pp.tailles[i]; ++j2, ++k) {
							const float32 x = pp.pts[k * 2], y = pp.pts[k * 2 + 1];
							if (y > 90.f && y < 130.f && x > 250.f) { // la ligne (100..120 en y), pas le rect
								if (x < xMin) xMin = x;
								if (x > xMax) xMax = x;
							}
						}
					}
				};
				uint32 dP = 0u, dR = 0u, dC = 0u;
				float32 aP, bP, aR, bR, aC, bC;
				etendueLigne("", dP, aP, bP);
				etendueLigne("ronde", dR, aR, bR);
				etendueLigne("carree", dC, aC, bC);
				snprintf(det, sizeof(det), "plate : %u disque(s), x %.0f..%.0f ; ronde : %u disque(s), x %.0f..%.0f ; carree : %u, x %.0f..%.0f",
						 dP, aP, bP, dR, aR, bR, dC, aC, bC);
				check("51e. EXTREMITES d'une ligne (diagonale 100 x 20, trait de 4) : plate = rien, ronde = deux disques "
					  "qui depassent de 2, carree = prolongee d'une demi-epaisseur de chaque cote",
					  dP == 0u && aP > 299.3f && aP < 300.7f && bP > 399.3f && bP < 400.7f && dR == 2u && aR < 298.5f
						  && bR > 401.5f && dC == 0u && aC < 298.5f && aC > 297.f && bC > 401.5f && bC < 403.f,
					  det);
				const bool ouverte = NkFormeOuverte(dB.nodes[(uint32)li]) && !NkFormeOuverte(dB.nodes[(uint32)f]);
				check("51f. une ligne est une forme OUVERTE, un rect non : c'est ce qui grise les extremites dans l'inspecteur",
					  ouverte, "");
			}
		}
		// ── 52. UNE LIGNE HORIZONTALE SE VOIT (hauteur nulle) ──────────────────
		{
			NkUIDocument dL;
			dL.NewDocument("Toile", NkAuthor::Humain);
			dL.nodes[0].layout.kind = NkLayoutKind::Free;
			dL.SetMetric("espacement", 0.f);
			dL.SetMetric("marge", 0.f);
			const int32 li = dL.AddChild(0, "", NkAuthor::Humain);
			NkUINode &nl = dL.nodes[(uint32)li];
			nl.shape = NkString("line");
			nl.posX = 100.f;
			nl.posY = 100.f;
			nl.width.mode = NkSizeMode::Fixed;
			nl.width.value = 100.f;
			nl.height.mode = NkSizeMode::Fixed;
			nl.height.value = 0.f; // un glisser parfaitement horizontal
			NkPaintRect surfL;
			surfL.x = 0.f;
			surfL.y = 0.f;
			surfL.w = 800.f;
			surfL.h = 600.f;
			NkRecordingPaint rec;
			RenderDocument(rec, dL, surfL);
			uint32 lignes = 0u;
			for (uint32 i = 0; i < (uint32)rec.cmds.Size(); ++i)
				if (rec.cmds[i].op == NkPaintOp::Line && rec.cmds[i].w > 99.f)
					++lignes;
			char det[120];
			snprintf(det, sizeof(det), "%u trait(s) de 100 px sur %u commande(s)", lignes, (uint32)rec.cmds.Size());
			check("52. une ligne HORIZONTALE (hauteur nulle) se voit : une forme ouverte n'a besoin que d'une dimension",
				  lignes == 1u, det);
		}
		// ── 53. LE DEGRADE, SOLIDE ET ROBUSTE : les cinq pieges ──────────────
		{
			using renderdetail::NkCouleurDegradeEn;
			using renderdetail::NkBandesDegrade;
			auto arret = [](float32 pos, const char *coul, float32 op) {
				NkArretDegrade a;
				a.position = pos;
				a.couleur = NkString(coul);
				a.opacite = op;
				return a;
			};
			char det[300];
			// 53a. L'ARRET TRANSPARENT : rouge opaque -> rouge transparent. Le milieu
			// doit rester ROUGE (interpolation premultipliee) ; une interpolation
			// naive donnerait du gris sale (r qui tombe vers 0 avec l'alpha).
			{
				NkDegrade g;
				g.arrets.PushBack(arret(0.f, "#ff0000", 100.f));
				g.arrets.PushBack(arret(1.f, "#ff0000", 0.f));
				const uint32 c = NkCouleurDegradeEn(g, 0.5f);
				const uint32 R = (c >> 24) & 0xFFu, G = (c >> 16) & 0xFFu, B = (c >> 8) & 0xFFu, A = c & 0xFFu;
				snprintf(det, sizeof(det), "milieu = R%u V%u B%u A%u (attendu R255 V0 B0 A~128)", R, G, B, A);
				check("53a. ARRET TRANSPARENT : « rouge opaque -> transparent » reste ROUGE en s'effacant "
					  "(interpolation premultipliee) -- une interpolation naive passerait par du gris sale",
					  R > 250u && G < 5u && B < 5u && A > 120u && A < 136u, det);
			}
			// 53b. deux couleurs opaques : le milieu est bien la moyenne (sRGB direct, l'espace NOMME)
			{
				NkDegrade g;
				g.arrets.PushBack(arret(0.f, "#000000", 100.f));
				g.arrets.PushBack(arret(1.f, "#ffffff", 100.f));
				const uint32 c = NkCouleurDegradeEn(g, 0.5f);
				const uint32 R = (c >> 24) & 0xFFu;
				snprintf(det, sizeof(det), "milieu = %u (sRGB direct : 128 ; un melange lineaire donnerait ~188)", R);
				check("53b. L'ESPACE EST NOMME -- sRGB direct, celui de Lunacy : noir -> blanc donne 128 au milieu",
					  R > 125u && R < 131u, det);
			}
			// 53c. arrets NON TRIES et hors bornes : l'ordre du fichier n'a aucune autorite
			{
				NkDegrade g;
				g.arrets.PushBack(arret(1.f, "#0000ff", 100.f)); // le dernier, ecrit en premier
				g.arrets.PushBack(arret(0.f, "#ff0000", 100.f));
				const uint32 avant = NkCouleurDegradeEn(g, -0.5f);
				const uint32 apres = NkCouleurDegradeEn(g, 1.5f);
				const uint32 mid = NkCouleurDegradeEn(g, 0.5f);
				snprintf(det, sizeof(det), "avant=%08X (rouge attendu), apres=%08X (bleu attendu), milieu=%08X", avant, apres, mid);
				check("53c. arrets NON TRIES : tries a la lecture ; avant le premier et apres le dernier, "
					  "la couleur du BORD, jamais du noir",
					  avant == 0xFF0000FFu && apres == 0x0000FFFFu && ((mid >> 24) & 0xFFu) > 120u
						  && ((mid >> 8) & 0xFFu) > 120u,
					  det);
			}
			// 53d. DOUBLON de position = coupure franche (une fonctionnalite, pas un bug)
			{
				NkDegrade g;
				g.arrets.PushBack(arret(0.f, "#ff0000", 100.f));
				g.arrets.PushBack(arret(0.5f, "#ff0000", 100.f));
				g.arrets.PushBack(arret(0.5f, "#0000ff", 100.f));
				g.arrets.PushBack(arret(1.f, "#0000ff", 100.f));
				const uint32 juste = NkCouleurDegradeEn(g, 0.49f);
				const uint32 apres = NkCouleurDegradeEn(g, 0.51f);
				snprintf(det, sizeof(det), "0.49 -> %08X (rouge), 0.51 -> %08X (bleu)", juste, apres);
				check("53d. DEUX ARRETS A LA MEME POSITION = coupure franche : rouge d'un cote, bleu de l'autre, "
					  "aucun melange -- c'est ce qui fait les bandes nettes",
					  juste == 0xFF0000FFu && apres == 0x0000FFFFu, det);
			}
			// 53e. UN SEUL arret = uni de cette couleur ; zero = rien (pas de division par zero)
			{
				NkDegrade g;
				g.arrets.PushBack(arret(0.3f, "#00ff00", 50.f));
				const uint32 a0 = NkCouleurDegradeEn(g, 0.f), a1 = NkCouleurDegradeEn(g, 1.f);
				NkDegrade vide;
				const uint32 z = NkCouleurDegradeEn(vide, 0.5f);
				snprintf(det, sizeof(det), "un arret : t=0 -> %08X, t=1 -> %08X (attendu 00FF0080) ; zero arret -> %08X", a0, a1, z);
				check("53e. UN SEUL arret = un uni de cette couleur (opacite comprise), partout ; zero arret ne calcule rien",
					  a0 == a1 && ((a0 >> 16) & 0xFFu) == 0xFFu && (a0 & 0xFFu) > 120u && (a0 & 0xFFu) < 136u && z == 0u, det);
			}
			// 53f. LES BANDES SUIVENT LA TAILLE : 24 pour une pastille, 192 au plafond
			{
				const int32 b16 = NkBandesDegrade(16.f), b200 = NkBandesDegrade(200.f), b2000 = NkBandesDegrade(2000.f);
				snprintf(det, sizeof(det), "16 px -> %d bandes, 200 px -> %d, 2000 px -> %d", b16, b200, b2000);
				check("53f. le nombre de bandes SUIT la taille dessinee (une par 2 px, borne 24..192) : 24 bandes "
					  "suffisent pour une pastille, pas pour un fond de 800 px",
					  b16 == 24 && b200 == 100 && b2000 == 192, det);
			}
			// 53g. l'opacite par arret : additive au fichier, relue a l'identique
			{
				NkUIDocument dOp;
				dOp.NewDocument("Toile", NkAuthor::Humain);
				const int32 f = dOp.AddChild(0, "", NkAuthor::Humain);
				NkRemplissage rf;
				rf.couleur = NkString("#ff0000");
				rf.degrade.type = NkString("lineaire");
				rf.degrade.arrets.PushBack(arret(0.f, "#ff0000", 100.f));
				rf.degrade.arrets.PushBack(arret(1.f, "#ff0000", 0.f));
				dOp.nodes[(uint32)f].fills.PushBack(rf);
				NkString s;
				dOp.Save(s);
				NkUIDocument relu;
				const bool ok = relu.Load(s.Data());
				const NkDegrade *g = (ok && relu.IsValidIndex(f) && !relu.nodes[(uint32)f].fills.Empty())
										? &relu.nodes[(uint32)f].fills[0].degrade : nullptr;
				const bool relus = g && g->arrets.Size() == 2u && g->arrets[0].opacite == 100.f
								   && g->arrets[1].opacite == 0.f;
				const bool additif = strstr(s.Data(), "1:#ff0000:0") != nullptr && strstr(s.Data(), "0:#ff0000 ") != nullptr;
				snprintf(det, sizeof(det), "relus=%d additif=%d", relus ? 1 : 0, additif ? 1 : 0);
				check("53g. l'OPACITE PAR ARRET fait l'aller-retour, additive : rien n'est ecrit tant qu'elle vaut 100",
					  relus && additif, det);
			}
		}
		// ── 54. LE SELECTEUR : LA PASTILLE DEMANDE, L'OVERLAY DESSINE ────────
		// Le defaut trouve le 04/09 : un popup dessine DEPUIS un panneau vit avec
		// une souris a moins cent mille -- le shell masque l'entree du corps des
		// qu'un popup est survole et ne la restaure qu'apres les panneaux. La
		// sonde porte la souris elle-meme, sans fenetre.
		{
			static nkgui::NkGuiContext ctxP;
			char det[320];
			if (!ctxP.Init(400, 600)) {
				check("54. le selecteur de couleur : contexte sans fenetre", false, "Init a refuse");
			} else {
				static DesignState stP;
				stP.picker = DesignState::DemandePicker();
				char hex[12] = "#1976d2";
				const nkgui::NkRect sw = {240.f, 60.f, 16.f, 16.f};
				auto image = [&](float32 mx, float32 my, bool bas, bool overlay) {
					ctxP.input.mousePos = {mx, my};
					ctxP.input.mouseDown[0] = bas;
					ctxP.BeginFrame(0.016f);
					ctxP.BeginLayout({0.f, 0.f, 300.f, 600.f});
					NkPastilleCouleur(ctxP, stP, "##sonde.pastille", sw, hex, (uint32)sizeof(hex));
					if (overlay)
						NkDessinerPickerDemande(ctxP, stP);
					const uint32 n = (uint32)ctxP.dlOverlay.vtx.Size();
					ctxP.EndFrame();
					return n;
				};
				// 1. survol : aucune demande
				image(248.f, 68.f, false, true);
				const bool riendemande = !stP.picker.ouvert;
				// 2. appui puis relachement sur la pastille : LA DEMANDE est posee
				image(248.f, 68.f, true, true);
				const uint32 apresRelache = image(248.f, 68.f, false, true);
				const bool demande = stP.picker.ouvert && stP.picker.id != 0u
									 && NkComponentDecl::StrEq(stP.picker.hex, "#1976d2");
				// 3. l'image suivante : le selecteur DESSINE dans la couche overlay
				const uint32 dessine = image(248.f, 68.f, false, true);
				const bool tient = stP.picker.ouvert && ctxP.IsPopupOpen(stP.picker.id);
				// 4. CONTROLE NEGATIF : sans le crochet d'overlay, rien n'est peint --
				//    c'est exactement ce que voyait Rodolf avant le remede.
				const uint32 sansOverlay = image(248.f, 68.f, false, false);
				snprintf(det, sizeof(det),
						 "survol : aucune demande=%d ; apres relachement : demande=%d (overlay %u) ; image "
						 "suivante : dessine %u sommets, popup ouvert=%d ; SANS le crochet : %u sommets",
						 riendemande ? 1 : 0, demande ? 1 : 0, apresRelache, dessine, tient ? 1 : 0, sansOverlay);
				check("54. LE SELECTEUR : la pastille DEMANDE (dans le panneau), le crochet d'OVERLAY dessine "
					  "(la ou l'entree est reelle) -- et sans ce crochet, rien n'est peint",
					  riendemande && demande && tient && dessine > 200u && sansOverlay == 0u, det);
			}
		}
		// ── 55. DESSINER HORS D'UNE PAGE (et la symetrie avec le reparentage) ─
		{
			NkUIDocument dH;
			dH.NewDocument("Toile", NkAuthor::Humain);
			// une page de 400x300 posee a (50,50) dans une surface plus grande
			const int32 pg = dH.AddChild(0, "", NkAuthor::Humain);
			dH.nodes[(uint32)pg].shape = NkString("frame");
			dH.nodes[(uint32)pg].layout.kind = NkLayoutKind::Free;
			dH.nodes[(uint32)pg].posX = 50.f;
			dH.nodes[(uint32)pg].posY = 50.f;
			dH.nodes[(uint32)pg].width.mode = NkSizeMode::Fixed;
			dH.nodes[(uint32)pg].width.value = 400.f;
			dH.nodes[(uint32)pg].height.mode = NkSizeMode::Fixed;
			dH.nodes[(uint32)pg].height.value = 300.f;
			NkPaintRect surfH;
			surfH.x = 0.f;
			surfH.y = 0.f;
			surfH.w = 900.f;
			surfH.h = 700.f;
			NkLayoutResult layH;
			NkComputeLayout(dH, surfH, layH);
			char det[280];
			// 55a. la racine d'un document NEUF est libre : une toile, pas une colonne
			const bool racineLibre = dH.nodes[0].layout.kind == NkLayoutKind::Free;
			// dans la page -> la page ; hors de la page -> LA RACINE (plus de refus)
			const int32 dedans = NkConteneurPourCreation(dH, layH, 200.f, 150.f);
			const int32 dehors = NkConteneurPourCreation(dH, layH, 700.f, 600.f);
			const int32 avant = NkPickFreeContainer(dH, layH, 700.f, 600.f);
			snprintf(det, sizeof(det), "racine libre=%d ; dans la page -> %d (page=%d) ; hors page -> %d "
									   "(racine=0) ; l'ancien predicat rendait %d",
					 racineLibre ? 1 : 0, dedans, pg, dehors, avant);
			check("55a. DESSINER HORS D'UNE PAGE : le conteneur de creation rend la page quand on est "
				  "dedans, et LA RACINE quand on est dehors -- l'ancien predicat refusait (-1)",
				  racineLibre && dedans == pg && dehors == 0 && avant == 0, det);
			// 55b. LA SYMETRIE, qui etait la contradiction : ce que le reparentage
			// autorise, la creation doit l'autoriser. On sort un noeud de la page,
			// puis on verifie qu'un geste de creation aurait pu le poser la.
			const int32 forme = dH.AddChild(pg, "", NkAuthor::Humain);
			dH.nodes[(uint32)forme].shape = NkString("rect");
			dH.nodes[(uint32)forme].width.mode = NkSizeMode::Fixed;
			dH.nodes[(uint32)forme].width.value = 40.f;
			dH.nodes[(uint32)forme].height.mode = NkSizeMode::Fixed;
			dH.nodes[(uint32)forme].height.value = 40.f;
			const bool sorti = dH.Reparent(forme, 0);
			const bool aLaRacine = sorti && dH.nodes[(uint32)forme].parent == 0;
			snprintf(det, sizeof(det), "retire de la page=%d, parent=%d ; creation hors page -> %d",
					 sorti ? 1 : 0, dH.nodes[(uint32)forme].parent, dehors);
			check("55b. LA SYMETRIE EST RETABLIE : le reparentage sort un objet de la page, et la creation "
				  "sait desormais l'y mettre -- les deux chemins repondent la MEME chose",
				  aLaRacine && dehors == 0, det);
		}
		// ── 56. UNE FORME NAIT AVEC SON APPARENCE ────────────────────────────
		// Rodolf : « quand on cree un graphique qui a un remplissage, ca doit
		// activer son remplissage dans les proprietes ». Le modele doit porter ce
		// que le peintre aurait pris -- sinon l'inspecteur (qui lit le modele)
		// reste vide devant une forme visiblement pleine.
		{
			nkentseu::editorkit::NkTheme th;
			const NkString fond = NkHexDuRole(th, "doc_field_bg");
			const NkString trait = NkHexDuRole(th, "doc_text");
			char det[240];
			const bool formeHex = NkHexLisible(fond.Data()) && NkHexLisible(trait.Data());
			snprintf(det, sizeof(det), "doc_field_bg -> %s ; doc_text -> %s", fond.Data(), trait.Data());
			check("56a. un role de theme se lit en « #rrggbb » : la creation peut POSER ce que le peintre "
				  "aurait pris (une seule source pour ce qu'on voit et ce qu'on montre)",
				  formeHex, det);
			// 56b. la regle de nature : une ligne recoit un TRAIT, une forme fermee un FOND
			NkUINode ligne, rect, texte, cadre;
			ligne.shape = NkString("line");
			rect.shape = NkString("rect");
			texte.shape = NkString("text");
			cadre.shape = NkString("frame");
			snprintf(det, sizeof(det), "ligne ouverte=%d, rect ouvert=%d", NkFormeOuverte(ligne) ? 1 : 0,
					 NkFormeOuverte(rect) ? 1 : 0);
			check("56b. la nature decide : une forme OUVERTE (ligne) recoit une bordure, une forme fermee "
				  "un remplissage -- texte et cadre, ni l'un ni l'autre",
				  NkFormeOuverte(ligne) && !NkFormeOuverte(rect) && !NkFormeOuverte(texte)
					  && !NkFormeOuverte(cadre),
				  det);
		}
		// ── 57. N ARRETS, POUR TOUS LES TYPES ────────────────────────────────
		// Rodolf : « on peut ajouter des pastilles sur les degrades si on veut,
		// donc le lineaire peut avoir plus de 2 ». Le calcul ne regarde JAMAIS le
		// type pour compter les arrets -- cette sonde le prouve plutot que de le
		// promettre, et elle protege la propriete contre une future optimisation
		// qui traiterait le lineaire « a deux bouts ».
		{
			auto arret = [](float32 pos, const char *coul) {
				NkArretDegrade a;
				a.position = pos;
				a.couleur = NkString(coul);
				return a;
			};
			static const char *const kTypes[5] = {"lineaire", "radial", "angulaire", "losange", "conique_inconnu"};
			bool tousPareils = true;
			uint32 milieuxVus = 0u;
			for (uint32 k = 0; k < 5u; ++k) {
				NkDegrade g;
				g.type = NkString(kTypes[k]);
				g.arrets.PushBack(arret(0.f, "#ff0000"));
				g.arrets.PushBack(arret(0.5f, "#00ff00")); // l'arret DU MILIEU
				g.arrets.PushBack(arret(1.f, "#0000ff"));
				const uint32 c = renderdetail::NkCouleurDegradeEn(g, 0.5f);
				if (((c >> 16) & 0xFFu) > 0xF0u && ((c >> 24) & 0xFFu) < 0x10u)
					++milieuxVus; // vert pur au milieu : l'arret intermediaire compte
				else
					tousPareils = false;
			}
			// et douze arrets se lisent aussi bien que trois
			NkDegrade douze;
			douze.type = NkString("lineaire");
			for (uint32 i = 0; i < 12u; ++i) {
				char h[12];
				snprintf(h, sizeof(h), "#%02x0000", (uint32)(i * 20u));
				douze.arrets.PushBack(arret((float32)i / 11.f, h));
			}
			const uint32 c11 = renderdetail::NkCouleurDegradeEn(douze, 1.f);
			const bool douzeOk = ((c11 >> 24) & 0xFFu) == 220u;
			char det[220];
			snprintf(det, sizeof(det), "%u/5 types honorent l'arret du milieu ; douze arrets : dernier = %02X (attendu DC)",
					 milieuxVus, (uint32)((c11 >> 24) & 0xFFu));
			check("57. N ARRETS POUR TOUS LES TYPES : lineaire, radial, angulaire, losange et un type INCONNU "
				  "honorent tous l'arret intermediaire -- le calcul ne regarde jamais le type pour compter",
				  tousPareils && milieuxVus == 5u && douzeOk, det);
		}
		// ── 58. DEUX BOUTONS, DEUX COULEURS, UN SEUL COMPOSANT ───────────────
		{
			NkUIDocument dI;
			dI.NewDocument("Toile", NkAuthor::Humain);
			const int32 pg = dI.AddChild(0, "", NkAuthor::Humain);
			dI.nodes[(uint32)pg].shape = NkString("frame");
			dI.nodes[(uint32)pg].layout.kind = NkLayoutKind::Free;
			dI.nodes[(uint32)pg].width.mode = NkSizeMode::Fixed;
			dI.nodes[(uint32)pg].width.value = 400.f;
			dI.nodes[(uint32)pg].height.mode = NkSizeMode::Fixed;
			dI.nodes[(uint32)pg].height.value = 300.f;
			// un bouton, bleu, qu'on ERIGE en composant (jamais implicitement)
			const int32 bt = dI.AddChild(pg, "", NkAuthor::Humain);
			dI.nodes[(uint32)bt].shape = NkString("rect");
			dI.nodes[(uint32)bt].label = NkString("Bouton");
			dI.nodes[(uint32)bt].width.mode = NkSizeMode::Fixed;
			dI.nodes[(uint32)bt].width.value = 120.f;
			dI.nodes[(uint32)bt].height.mode = NkSizeMode::Fixed;
			dI.nodes[(uint32)bt].height.value = 40.f;
			{
				NkRemplissage f;
				f.couleur = NkString("#1976d2");
				dI.nodes[(uint32)bt].fills.PushBack(f);
			}
			const int32 decl = dI.ExtraireComposant(bt, "", "bouton");
			const uint32 nDeclApres = (uint32)dI.declarations.Size();
			const int32 a1 = dI.InstancierComposant(decl, pg);
			const int32 a2 = dI.InstancierComposant(decl, pg);
			char det[300];
			if (decl < 0 || !dI.IsValidIndex(a1) || !dI.IsValidIndex(a2)) {
				check("58. deux instances d'un composant", false, "l'extraction ou l'instanciation a refuse");
			} else {
				// LE GESTE, exactement comme l'inspecteur le fait : on ecrit la
				// couleur DANS L'INSTANCE et on marque la surcharge.
				dI.nodes[(uint32)a1].MaterialiserFills();
				if (!dI.nodes[(uint32)a1].fills.Empty())
					dI.nodes[(uint32)a1].fills[0].couleur = NkString("#d21976");
				dI.nodes[(uint32)a1].ecarts |= NkUINode::EcartRemplissages;
				dI.MarkHumanEdit(a1);
				const char *c1 = dI.nodes[(uint32)a1].fills.Empty() ? "" : dI.nodes[(uint32)a1].fills[0].couleur.Data();
				const char *c2 = dI.nodes[(uint32)a2].fills.Empty() ? "" : dI.nodes[(uint32)a2].fills[0].couleur.Data();
				const bool separees = NkComponentDecl::StrEq(c1, "#d21976") && NkComponentDecl::StrEq(c2, "#1976d2");
				const bool memeComposant = NkComponentDecl::StrEq(dI.nodes[(uint32)a1].instanceDe.Data(),
																   dI.nodes[(uint32)a2].instanceDe.Data());
				const bool aucunNouveau = (uint32)dI.declarations.Size() == nDeclApres;
				snprintf(det, sizeof(det), "instance 1 = %s, instance 2 = %s ; meme composant=%d ; %u declaration(s) "
										   "avant et apres",
						 c1, c2, memeComposant ? 1 : 0, (uint32)dI.declarations.Size());
				check("58a. changer la couleur d'UNE instance ne touche pas l'autre, et NE CREE AUCUN composant : "
					  "la couleur n'est qu'une propriete, et l'ecart vit sur l'instance",
					  separees && memeComposant && aucunNouveau, det);
				// 58b. la DECLARATION change : l'instance sans surcharge suit, celle qui
				// a surcharge TIENT (c'est la moitie de Q51 qu'un banc peut prouver)
				if (!dI.declarations[(uint32)decl].arbre.Empty()) {
					NkUINode &ref = dI.declarations[(uint32)decl].arbre[0];
					ref.MaterialiserFills();
					if (!ref.fills.Empty())
						ref.fills[0].couleur = NkString("#00aa00");
					else {
						NkRemplissage f;
						f.couleur = NkString("#00aa00");
						ref.fills.PushBack(f);
					}
				}
				const int32 touchees = dI.PropagerVersInstances(decl);
				const char *d1 = dI.nodes[(uint32)a1].fills.Empty() ? "" : dI.nodes[(uint32)a1].fills[0].couleur.Data();
				const char *d2 = dI.nodes[(uint32)a2].fills.Empty() ? "" : dI.nodes[(uint32)a2].fills[0].couleur.Data();
				const bool surchargeTient = NkComponentDecl::StrEq(d1, "#d21976");
				const bool libreSuit = NkComponentDecl::StrEq(d2, "#00aa00");
				snprintf(det, sizeof(det), "%d instance(s) touchee(s) ; surchargee = %s (tient), libre = %s "
										   "(suit la declaration #00aa00)",
						 touchees, d1, d2);
				check("58b. la DECLARATION change : l'instance qui a surcharge sa couleur la GARDE, et celle "
					  "qui n'a rien surcharge SUIT -- la propagation compare le CONTENU, pas le nombre",
					  surchargeTient && libreSuit && touchees >= 1, det);
				// 58c. l'aller-retour : la surcharge est un fait du document, pas de la session
				NkString s;
				dI.Save(s);
				NkUIDocument relu;
				const bool chargee = relu.Load(s.Data());
				const bool ecartRelu = chargee && relu.IsValidIndex(a1)
									   && relu.nodes[(uint32)a1].Surcharge(NkUINode::EcartRemplissages)
									   && !relu.nodes[(uint32)a2].Surcharge(NkUINode::EcartRemplissages);
				snprintf(det, sizeof(det), "aller-retour : ecart relu sur l'instance 1=%d, absent sur la 2=%d",
						 (chargee && relu.IsValidIndex(a1) && relu.nodes[(uint32)a1].Surcharge(NkUINode::EcartRemplissages)) ? 1 : 0,
						 (chargee && relu.IsValidIndex(a2) && !relu.nodes[(uint32)a2].Surcharge(NkUINode::EcartRemplissages)) ? 1 : 0);
				check("58c. la SURCHARGE survit a l'aller-retour (`ecarts` au fichier) : c'est un fait du "
					  "document, pas de la session",
					  ecartRelu, det);
			}
		}
		// ── 59. AJOUTER UN ARRET : UNE FONCTION, ET RIEN NE SAUTE ────────────
		{
			NkDegrade g;
			NkArretDegrade a0, a1;
			a0.position = 0.f;
			a0.couleur = NkString("#000000");
			a1.position = 1.f;
			a1.couleur = NkString("#ffffff");
			g.arrets.PushBack(a0);
			g.arrets.PushBack(a1);
			const uint32 avant = renderdetail::NkCouleurDegradeEn(g, 0.25f);
			const int32 idx = renderdetail::NkAjouterArretDegrade(g, 0.25f, 12u);
			const uint32 apres = renderdetail::NkCouleurDegradeEn(g, 0.25f);
			// et ailleurs non plus, rien ne bouge
			const uint32 ailleurs = renderdetail::NkCouleurDegradeEn(g, 0.75f);
			// le plafond est respecte
			for (uint32 k = 0; k < 20u; ++k)
				renderdetail::NkAjouterArretDegrade(g, 0.5f, 12u);
			const uint32 nb = (uint32)g.arrets.Size();
			char det[240];
			snprintf(det, sizeof(det), "arret ajoute en %d ; couleur en 0.25 : %08X -> %08X ; en 0.75 : %08X ; "
									   "plafond 12 -> %u arrets",
					 idx, avant, apres, ailleurs, nb);
			check("59. AJOUTER UN ARRET (la fonction que la barre ET, demain, le segment de la toile "
				  "appellent) : il nait a la couleur qu'avait le degrade ICI -- l'image ne saute pas -- et "
				  "le plafond tient",
				  idx == 2 && avant == apres && ailleurs == 0xBFBFBFFFu && nb == 12u, det);
		}
		// ── 60. L'INSPECTEUR A DEUX LARGEURS : RIEN NE SORT DU CADRE ─────────
		// « Une largeur reduite est un cas d'epreuve, pas un accident. » Le
		// panneau se dessine sans fenetre (contexte NKGui sans fenetre), a 260
		// puis a 170 px, et on mesure l'etendue en x de TOUS les sommets peints.
		{
			static nkgui::NkGuiContext ctxI;
			char det[320];
			if (!ctxI.Init(600, 900)) {
				check("60. l'inspecteur sans fenetre", false, "Init a refuse");
			} else {
				static DesignState stI;
				stI.doc.NewDocument("Toile", NkAuthor::Humain);
				const int32 pg = stI.doc.AddChild(0, "", NkAuthor::Humain);
				stI.doc.nodes[(uint32)pg].shape = NkString("frame");
				stI.doc.nodes[(uint32)pg].layout.kind = NkLayoutKind::Free;
				stI.doc.nodes[(uint32)pg].width.mode = NkSizeMode::Fixed;
				stI.doc.nodes[(uint32)pg].width.value = 400.f;
				stI.doc.nodes[(uint32)pg].height.mode = NkSizeMode::Fixed;
				stI.doc.nodes[(uint32)pg].height.value = 300.f;
				const int32 rc = stI.doc.AddChild(pg, "", NkAuthor::Humain);
				{
					NkUINode &n = stI.doc.nodes[(uint32)rc];
					n.shape = NkString("rect");
					n.width.mode = NkSizeMode::Fixed;
					n.width.value = 120.f;
					n.height.mode = NkSizeMode::Fixed;
					n.height.value = 40.f;
					NkRemplissage f1;
					f1.couleur = NkString("#1976d2");
					NkRemplissage f2; // un degrade a trois arrets
					f2.couleur = NkString("#ffffff");
					f2.degrade.type = NkString("lineaire");
					for (uint32 k = 0; k < 3u; ++k) {
						NkArretDegrade ar;
						ar.position = (float32)k * 0.5f;
						ar.couleur = NkString(k == 0 ? "#fafcff" : (k == 1 ? "#d11313" : "#1976d1"));
						f2.degrade.arrets.PushBack(ar);
					}
					n.fills.PushBack(f1);
					n.fills.PushBack(f2);
					NkBordure b;
					b.couleur = NkString("#30363d");
					b.epaisseur = 2.f;
					n.borders.PushBack(b);
				}
				stI.Recompute(NkPaintRect{0.f, 0.f, 600.f, 900.f});
				stI.SelectSingle(rc);
				static InspectorPanel insp(&stI);
				NkEditorFrameContext ec;
				ec.ui = &ctxI;
				ec.dt = 0.016f;
				float32 yDebMin = 1e9f, yDebMax = -1e9f;
				auto image = [&](float32 largeur, bool popover, float32 &xMax, uint32 &nSommets, uint32 &nOverlay) {
					const nkgui::NkRect region = {600.f - largeur, 0.f, largeur, 900.f};
					ctxI.input.mousePos = {-1.f, -1.f};
					ctxI.input.mouseDown[0] = false;
					ctxI.BeginFrame(0.016f);
					ctxI.BeginLayout(region);
					insp.OnUI(ec);
					if (popover)
						NkDessinerPickerDemande(ctxI, stI);
					xMax = -1e9f;
					yDebMin = 1e9f;
					yDebMax = -1e9f;
					for (uint32 i = 0; i < (uint32)ctxI.dl.vtx.Size(); ++i) {
						if (ctxI.dl.vtx[i].pos.x > xMax)
							xMax = ctxI.dl.vtx[i].pos.x;
						if (ctxI.dl.vtx[i].pos.x > 600.5f) { // ou ca deborde
							if (ctxI.dl.vtx[i].pos.y < yDebMin) yDebMin = ctxI.dl.vtx[i].pos.y;
							if (ctxI.dl.vtx[i].pos.y > yDebMax) yDebMax = ctxI.dl.vtx[i].pos.y;
						}
					}
					nSommets = (uint32)ctxI.dl.vtx.Size();
					nOverlay = (uint32)ctxI.dlOverlay.vtx.Size();
					ctxI.EndFrame();
				};
				float32 x260 = 0.f, x170 = 0.f;
				uint32 n260 = 0u, n170 = 0u, o = 0u;
				image(260.f, false, x260, n260, o);
				image(170.f, false, x170, n170, o);
				snprintf(det, sizeof(det), "260 px : %u sommets, x max %.1f (bord 600) ; 170 px : %u sommets, x max %.1f "
										   "(bord 600) ; ce qui deborde a 170 px est entre y=%.0f et y=%.0f",
						 n260, x260, n170, x170, yDebMin, yDebMax);
				check("60a. L'INSPECTEUR A 260 PUIS A 170 PX : aucun sommet peint ne depasse le bord droit du "
					  "panneau -- rien ne sort du cadre, mesure au temoin",
					  n260 > 0u && n170 > 0u && x260 <= 600.5f && x170 <= 600.5f, det);
				// 60b. le POPOVER du remplissage (degrade) dessine, dans la couche overlay, dans l'ecran
				stI.picker = DesignState::DemandePicker();
				stI.picker.ouvert = true;
				stI.picker.id = ctxI.GetId("##sonde.popover");
				stI.picker.genre = 1u;
				stI.picker.noeud = rc;
				stI.picker.index = 1;
				stI.picker.ancre = {360.f, 200.f, 16.f, 16.f};
				float32 xp = 0.f;
				uint32 np = 0u, op = 0u;
				image(260.f, true, xp, np, op);
				image(260.f, true, xp, np, op);
				float32 oxMin = 1e9f, oxMax = -1e9f;
				for (uint32 i = 0; i < (uint32)ctxI.dlOverlay.vtx.Size(); ++i) {
					if (ctxI.dlOverlay.vtx[i].pos.x < oxMin) oxMin = ctxI.dlOverlay.vtx[i].pos.x;
					if (ctxI.dlOverlay.vtx[i].pos.x > oxMax) oxMax = ctxI.dlOverlay.vtx[i].pos.x;
				}
				snprintf(det, sizeof(det), "popover : %u sommets overlay, x %.0f..%.0f (ecran 0..600), ouvert=%d, arret courant=%d",
						 op, oxMin, oxMax, stI.picker.ouvert ? 1 : 0, stI.picker.arretSel);
				check("60b. LE POPOVER D'UN DEGRADE (types, selecteur, hexa, barre, liste de trois arrets) dessine "
					  "dans l'overlay, a GAUCHE de sa pastille, et reste dans l'ecran",
					  stI.picker.ouvert && op > 300u && oxMin >= 0.f && oxMax <= 600.5f && oxMax <= 360.f, det);
				// 60d. LE POPOVER TIENT DANS LA FENETRE : la pastille sur la DERNIERE ligne
				// visible (Rodolf l'a vu coupe par le bas) -- il est remonte, et tous ses
				// sommets restent dans la fenetre (600 x 900)
				stI.picker = DesignState::DemandePicker();
				stI.picker.ouvert = true;
				stI.picker.id = ctxI.GetId("##sonde.popover.bas");
				stI.picker.genre = 1u;
				stI.picker.noeud = rc;
				stI.picker.index = 1;
				stI.picker.ancre = {360.f, 870.f, 16.f, 16.f};
				image(260.f, true, xp, np, op);
				image(260.f, true, xp, np, op);
				float32 oyMin = 1e9f, oyMax = -1e9f;
				for (uint32 i = 0; i < (uint32)ctxI.dlOverlay.vtx.Size(); ++i) {
					if (ctxI.dlOverlay.vtx[i].pos.y < oyMin) oyMin = ctxI.dlOverlay.vtx[i].pos.y;
					if (ctxI.dlOverlay.vtx[i].pos.y > oyMax) oyMax = ctxI.dlOverlay.vtx[i].pos.y;
				}
				snprintf(det, sizeof(det), "pastille a y=870 (fenetre 900) : popover y %.0f..%.0f, %u sommets", oyMin, oyMax, op);
				check("60d. LE POPOVER TIENT DANS LA FENETRE : pastille sur la derniere ligne visible, le popover est "
					  "REMONTE et tous ses sommets restent dans la fenetre -- la liste d'arrets et le + sont atteignables",
					  op > 300u && oyMin >= 0.f && oyMax <= 900.5f, det);
				// 60c. le popover de BORDURE (genre 2) : hexa, epaisseur, position, cotes,
				// jointure, extremites -- dans l'overlay, a gauche, dans l'ecran
				stI.picker = DesignState::DemandePicker();
				stI.picker.ouvert = true;
				stI.picker.id = ctxI.GetId("##sonde.popover.bord");
				stI.picker.genre = 2u;
				stI.picker.noeud = rc;
				stI.picker.index = 0;
				stI.picker.ancre = {360.f, 300.f, 16.f, 16.f};
				image(260.f, true, xp, np, op);
				image(260.f, true, xp, np, op);
				oxMin = 1e9f;
				oxMax = -1e9f;
				for (uint32 i = 0; i < (uint32)ctxI.dlOverlay.vtx.Size(); ++i) {
					if (ctxI.dlOverlay.vtx[i].pos.x < oxMin) oxMin = ctxI.dlOverlay.vtx[i].pos.x;
					if (ctxI.dlOverlay.vtx[i].pos.x > oxMax) oxMax = ctxI.dlOverlay.vtx[i].pos.x;
				}
				snprintf(det, sizeof(det), "popover de bordure : %u sommets overlay, x %.0f..%.0f (ecran 0..600), ouvert=%d ; "
										   "panneau a 260 px : %u sommets (etait 3777 avec les trois rangees)",
						 op, oxMin, oxMax, stI.picker.ouvert ? 1 : 0, np);
				check("60c. LE POPOVER D'UNE BORDURE (selecteur, hexa, epaisseur, position, cotes, jointure, "
					  "extremites) dessine dans l'overlay, a gauche de sa pastille, dans l'ecran -- et le panneau "
					  "a perdu ses trois rangees",
					  stI.picker.ouvert && op > 300u && oxMin >= 0.f && oxMax <= 360.f && np < 3777u, det);
				// 60e. LE POPOVER IMAGE : le cadrage est un MENU `Fill ˅` (sa capture ⑤), pas cinq
				// puces qui se repliaient sur deux lignes. La sonde porte la souris : un clic sur le
				// bouton ouvre le menu DANS la boite, un clic sur « Tile » le pose au modele.
				{
					NkRemplissage fi;
					fi.genre = NkString("image");
					stI.doc.nodes[(uint32)rc].fills.PushBack(fi);
					const int32 iImg = (int32)stI.doc.nodes[(uint32)rc].fills.Size() - 1;
					stI.picker = DesignState::DemandePicker();
					stI.picker.ouvert = true;
					stI.picker.id = ctxI.GetId("##sonde.popover.image");
					stI.picker.genre = 1u;
					stI.picker.noeud = rc;
					stI.picker.index = iImg;
					stI.picker.ancre = {360.f, 300.f, 16.f, 16.f};
					auto souris = [&](float32 mx, float32 my, bool bas) {
						ctxI.input.mousePos = {mx, my};
						ctxI.input.mouseDown[0] = bas;
						ctxI.BeginFrame(0.016f);
						ctxI.BeginLayout({340.f, 0.f, 260.f, 900.f});
						insp.OnUI(ec);
						NkDessinerPickerDemande(ctxI, stI);
						const uint32 n = (uint32)ctxI.dlOverlay.vtx.Size();
						oxMin = 1e9f; oxMax = -1e9f; oyMin = 1e9f; oyMax = -1e9f;
						for (uint32 i = 0; i < n; ++i) {
							const nkgui::NkVec2 q = ctxI.dlOverlay.vtx[i].pos;
							if (q.x < oxMin) oxMin = q.x;
							if (q.x > oxMax) oxMax = q.x;
							if (q.y < oyMin) oyMin = q.y;
							if (q.y > oyMax) oyMax = q.y;
						}
						ctxI.EndFrame();
						return n;
					};
					souris(-1.f, -1.f, false);
					const uint32 oFerme = souris(-1.f, -1.f, false);
					const float32 bas = oyMax, gauche = oxMin;
					// la rangee Cadrage est l'avant-derniere (rotation 26, marge 8) ; le bouton
					// commence 62 px apres la marge gauche
					const float32 bx = gauche + 8.f + 62.f + 20.f, by = bas - 47.f;
					souris(bx, by, true);
					souris(bx, by, false);
					const uint32 oOuvert = souris(bx, by, false);
					const bool dansFenetre = oxMin >= 0.f && oyMin >= 0.f && oxMax <= 600.5f && oyMax <= 900.5f;
					// le menu s'ouvre vers le haut : « Tile » est la 4e rangee (k=3)
					const float32 ty = by - 3.f - 104.f + 2.f + 3.f * 20.f + 10.f - 10.f;
					souris(bx, ty, true);
					souris(bx, ty, false);
					const uint32 oPose = souris(-1.f, -1.f, false);
					const NkRemplissage &fr = stI.doc.nodes[(uint32)rc].fills[(uint32)iImg];
					snprintf(det, sizeof(det),
							 "popover image : %u sommets ferme, %u menu ouvert (clic a %.0f,%.0f), %u apres « Tile » ; "
							 "cadrage=`%s` ; dans la fenetre=%d",
							 oFerme, oOuvert, bx, by, oPose, fr.cadrage.Data(), dansFenetre ? 1 : 0);
					check("60e. LE POPOVER IMAGE : le cadrage est un MENU `Fill ˅` (Lunacy), pas cinq puces -- le "
						  "bouton l'ouvre dans la boite, « Tile » le pose au modele et le referme, tout reste dans la fenetre",
						  oFerme > 100u && oOuvert > oFerme + 20u && oPose < oOuvert && fr.EstImage()
							  && NkComponentDecl::StrEq(fr.cadrage.Data(), "tile") && dansFenetre,
						  det);
				}
				// 60f. LA RANGEE MODELE (ses neuf captures) : `[Modele ˅] v1 v2 v3 [op %]` sur UNE
				// ligne ; chaque champ tient `-54,00` (six caracteres, LAB) sans troncature --
				// mesure a la police du panneau ; rien ne se chevauche, tout tient entre x0 et x1.
				{
					stI.picker = DesignState::DemandePicker();
					stI.picker.ouvert = true;
					stI.picker.id = ctxI.GetId("##sonde.popover.modele");
					stI.picker.genre = 1u;
					stI.picker.noeud = rc;
					stI.picker.index = 0;
					stI.picker.ancre = {360.f, 200.f, 16.f, 16.f};
					float32 xq = 0.f;
					uint32 nq = 0u, oq = 0u;
					image(260.f, true, xq, nq, oq);
					image(260.f, true, xq, nq, oq);
					float32 pxMin = 1e9f, pxMax = -1e9f, pyMin = 1e9f;
					for (uint32 i = 0; i < (uint32)ctxI.dlOverlay.vtx.Size(); ++i) {
						const nkgui::NkVec2 q = ctxI.dlOverlay.vtx[i].pos;
						if (q.x < pxMin) pxMin = q.x;
						if (q.x > pxMax) pxMax = q.x;
						if (q.y < pyMin) pyMin = q.y;
					}
					const float32 x0 = pxMin + 0.5f + 8.f, x1 = pxMax - 0.5f - 8.f;
					nkgui::NkRect menu, ch[3], op;
					NkRangeeModele(x0, x1, 0.f, menu, ch, op);
					auto &F = costume::Fontes();
					// sans fenetre la police n'est pas chargee et `Largeur` rend 0 : la sonde ne
					// se laisse pas passer a vide -- budget de 6 px par caractere a px10, dit
					float32 lTexte = costume::Largeur(F.px10, "-54,00");
					float32 lHex = costume::Largeur(F.px10, "#1976d2");
					const bool policeAbsente = lTexte <= 0.f || lHex <= 0.f;
					if (policeAbsente) {
						lTexte = 6.f * 6.f;
						lHex = 7.f * 6.f;
					}
					bool tient = ch[0].w >= lTexte + 6.f && ch[1].w >= lTexte + 6.f && ch[2].w >= lTexte + 6.f
								 && (ch[2].x + ch[2].w - ch[0].x) >= lHex + 6.f;
					bool ordre = menu.x >= x0 && menu.x + menu.w + 4.f <= ch[0].x && ch[0].x + ch[0].w <= ch[1].x
								 && ch[1].x + ch[1].w <= ch[2].x && ch[2].x + ch[2].w + 4.f <= op.x
								 && op.x + op.w + 12.f <= x1 + 0.5f;
					snprintf(det, sizeof(det),
							 "popover %.0f px : menu %.0f, trois champs de %.1f px (« -54,00 » = %.1f px%s), opacite %.0f ; "
							 "hexa sur %.0f px (« #1976d2 » = %.1f px)",
							 pxMax - pxMin, menu.w, ch[0].w, lTexte, policeAbsente ? " -- police absente sans fenetre, budget 6 px/car." : "",
							 op.w, ch[2].x + ch[2].w - ch[0].x, lHex);
					check("60f. LA RANGEE MODELE SUR UNE LIGNE (ses neuf captures) : menu, trois champs qui tiennent "
						  "« -54,00 » sans troncature, opacite -- mesure a la police, rien ne se chevauche",
						  tient && ordre && oq > 300u, det);
					// 60g. L'ALLER-RETOUR IDENTIQUE, la ou Lunacy derive (1976D2 -> 1A76D1) : sur
					// les 16 777 216 couleurs, Hex -> RGB -> Hex et Hex -> HSB -> Hex rendent le
					// meme hexa. La derive des ENTIERS AFFICHES est comptee (c'est pourquoi
					// l'affichage n'ecrit jamais) ; puis, au popover, HSB affiche trois images de
					// suite sans toucher l'hexa ; enfin la lecture francaise (virgule, signe).
					uint32 ratesRgb = 0u, ratesHsb = 0u, deriveEntiers = 0u;
					for (uint32 c = 0u; c < 0x1000000u; ++c) {
						const float32 r = (float32)((c >> 16) & 255u), g = (float32)((c >> 8) & 255u), b = (float32)(c & 255u);
						char h0[12], h1[12];
						NkRgbVersHex(r, g, b, h0);
						float32 v[3], r2, g2, b2;
						NkRgbVersModele(1, r, g, b, v);
						NkModeleVersRgb(1, v, r2, g2, b2);
						NkRgbVersHex(r2, g2, b2, h1);
						if (!NkComponentDecl::StrEq(h0, h1)) ++ratesRgb;
						NkRgbVersModele(2, r, g, b, v);
						NkModeleVersRgb(2, v, r2, g2, b2);
						NkRgbVersHex(r2, g2, b2, h1);
						if (!NkComponentDecl::StrEq(h0, h1)) ++ratesHsb;
						const float32 ve[3] = {(float32)(int32)(v[0] + 0.5f), (float32)(int32)(v[1] + 0.5f), (float32)(int32)(v[2] + 0.5f)};
						NkModeleVersRgb(2, ve, r2, g2, b2);
						NkRgbVersHex(r2, g2, b2, h1);
						if (!NkComponentDecl::StrEq(h0, h1)) ++deriveEntiers;
					}
					// au popover : choisir HSB par le menu, trois images, l'hexa du modele n'a pas bouge
					auto souris = [&](float32 mx, float32 my, bool bas) {
						ctxI.input.mousePos = {mx, my};
						ctxI.input.mouseDown[0] = bas;
						ctxI.BeginFrame(0.016f);
						ctxI.BeginLayout({340.f, 0.f, 260.f, 900.f});
						insp.OnUI(ec);
						NkDessinerPickerDemande(ctxI, stI);
						ctxI.EndFrame();
					};
					const float32 yRangee = pyMin + 0.5f + 8.f + 26.f + 168.f + 3.f;
					const float32 mx = x0 + 25.f, my = yRangee + 10.f;
					souris(mx, my, true);
					souris(mx, my, false);
					const float32 hy = yRangee - 9.f * 20.f - 4.f + 2.f + 2.f * 20.f + 10.f; // la rangee HSB (k=2)
					souris(x0 + 20.f, hy, true);
					souris(x0 + 20.f, hy, false);
					souris(-1.f, -1.f, false);
					souris(-1.f, -1.f, false);
					souris(-1.f, -1.f, false);
					const bool hexaIntact = NkComponentDecl::StrEq(stI.doc.nodes[(uint32)rc].fills[0].couleur.Data(), "#1976d2")
											&& NkComponentDecl::StrEq(stI.picker.hex, "#1976d2");
					float32 l1 = 0.f, l2 = 0.f, l3 = 0.f, l4 = 1.f;
					const bool lecture = NkLireNombreFr("49,21", l1) && NkLireNombreFr("-54,0", l2) && NkLireNombreFr("0.564", l3)
										 && !NkLireNombreFr("abc", l4)
										 && l1 > 49.2f && l1 < 49.22f && l2 == -54.f && l3 > 0.563f && l3 < 0.565f;
					char fr[16];
					NkEcrireNombreFr(fr, (uint32)sizeof(fr), -54.0f, 2);
					const bool ecriture = NkComponentDecl::StrEq(fr, "-54,00");
					snprintf(det, sizeof(det),
							 "16 777 216 couleurs : rates RGB=%u, rates HSB=%u ; derive des ENTIERS affiches=%u (la raison "
							 "de ne pas ecrire) ; hexa intact apres HSB affiche=%d ; lecture fr=%d, ecriture « %s »",
							 ratesRgb, ratesHsb, deriveEntiers, hexaIntact ? 1 : 0, lecture ? 1 : 0, fr);
					check("60g. L'ALLER-RETOUR Hex -> modele -> Hex est IDENTIQUE sur les 16,7 M de couleurs (RGB et "
						  "HSB), l'affichage n'ecrit jamais l'hexa (la derive de Lunacy), la lecture accepte la "
						  "virgule, le point et le signe",
						  ratesRgb == 0u && ratesHsb == 0u && hexaIntact && lecture && ecriture, det);
				}
				// 60h. LE SELECTEUR S'APPLIQUE A L'OBJET (Rodolf, 04/09 : « quand je modifie le
				// color picker ca ne se reflete pas sur l'objet, uni ou degrade »). Sans fenetre :
				// un clic dans le carre SV du popover ; puis le MODELE a change, et la commande
				// de remplissage ENREGISTREE du noeud a change de couleur -- noeud libre, degrade,
				// instance (la surcharge doit gagner, et l'autre instance ne pas bouger).
				{
					auto souris = [&](float32 mx, float32 my, bool bas) {
						ctxI.input.mousePos = {mx, my};
						ctxI.input.mouseDown[0] = bas;
						ctxI.BeginFrame(0.016f);
						ctxI.BeginLayout({340.f, 0.f, 260.f, 900.f});
						insp.OnUI(ec);
						NkDessinerPickerDemande(ctxI, stI);
						ctxI.EndFrame();
					};
					auto boite = [&](float32 &px, float32 &py) {
						px = 1e9f;
						py = 1e9f;
						for (uint32 i = 0; i < (uint32)ctxI.dlOverlay.vtx.Size(); ++i) {
							if (ctxI.dlOverlay.vtx[i].pos.x < px) px = ctxI.dlOverlay.vtx[i].pos.x;
							if (ctxI.dlOverlay.vtx[i].pos.y < py) py = ctxI.dlOverlay.vtx[i].pos.y;
						}
					};
					auto compter = [&](nkentseu::uint32 rgba) {
						NkRecordingPaint rec;
						RenderDocument(rec, stI.doc, NkPaintRect{0.f, 0.f, 600.f, 900.f});
						uint32 n = 0u;
						for (uint32 i = 0; i < (uint32)rec.cmds.Size(); ++i)
							if ((rec.cmds[i].op == NkPaintOp::Fill || rec.cmds[i].op == NkPaintOp::FillColor) && rec.cmds[i].rgba == rgba)
								++n;
						return n;
					};
					auto empreinte = [&]() {
						NkRecordingPaint rec;
						RenderDocument(rec, stI.doc, NkPaintRect{0.f, 0.f, 600.f, 900.f});
						nkentseu::uint32 h = 2166136261u;
						for (uint32 i = 0; i < (uint32)rec.cmds.Size(); ++i)
							h = (h ^ rec.cmds[i].rgba) * 16777619u;
						return h;
					};
					// un clic au centre du carre SV du popover ouvert sur (noeud, index, arret)
					auto cliquerSV = [&](int32 noeud, int32 index, int32 arret) {
						stI.picker = DesignState::DemandePicker();
						stI.picker.ouvert = true;
						stI.picker.id = ctxI.GetId("##sonde.popover.applique");
						stI.picker.genre = 1u;
						stI.picker.noeud = noeud;
						stI.picker.index = index;
						stI.picker.arretSel = arret;
						stI.picker.ancre = {360.f, 200.f, 16.f, 16.f};
						souris(-1.f, -1.f, false);
						souris(-1.f, -1.f, false);
						float32 px, py;
						boite(px, py);
						const float32 cx = px + 0.5f + 8.f + 80.f, cy = py + 0.5f + 8.f + 26.f + 80.f;
						souris(cx, cy, false); // le survol precede l'appui (le kit resout le survol a l'image d'avant)
						souris(cx, cy, true);
						souris(cx, cy, false);
						souris(-1.f, -1.f, false);
						stI.picker = DesignState::DemandePicker();
						souris(-1.f, -1.f, false);
					};
					// 1. le noeud libre, uni
					const nkentseu::uint32 bleu = renderdetail::NkGCouleur("#1976d2");
					const uint32 avantLibre = compter(bleu);
					stI.SelectSingle(rc);
					cliquerSV(rc, 0, 0);
					const NkString cLibre = stI.doc.nodes[(uint32)rc].fills[0].couleur;
					const bool modeleLibre = NkHexLisible(cLibre.Data()) && !NkComponentDecl::StrEq(cLibre.Data(), "#1976d2");
					const uint32 apresLibreAncien = compter(bleu);
					const uint32 apresLibreNouveau = compter(renderdetail::NkGCouleur(cLibre.Data()));
					// 2. le degrade (fills[1], arret 0 = #fafcff)
					const nkentseu::uint32 avantDeg = empreinte();
					cliquerSV(rc, 1, 0);
					const NkString cArret = stI.doc.nodes[(uint32)rc].fills[1].degrade.arrets[0].couleur;
					const bool modeleDeg = NkHexLisible(cArret.Data()) && !NkComponentDecl::StrEq(cArret.Data(), "#fafcff");
					const bool dessinDeg = empreinte() != avantDeg;
					// 3. l'instance : un composant extrait d'un bouton bleu, deux instances ; on
					//    edite la premiere -- elle change, la seconde non
					const int32 bt = stI.doc.AddChild(pg, "", NkAuthor::Humain);
					stI.doc.nodes[(uint32)bt].shape = NkString("rect");
					stI.doc.nodes[(uint32)bt].label = NkString("Bouton");
					stI.doc.nodes[(uint32)bt].width.mode = NkSizeMode::Fixed;
					stI.doc.nodes[(uint32)bt].width.value = 100.f;
					stI.doc.nodes[(uint32)bt].height.mode = NkSizeMode::Fixed;
					stI.doc.nodes[(uint32)bt].height.value = 30.f;
					{
						NkRemplissage fb;
						fb.couleur = NkString("#2e7d32");
						stI.doc.nodes[(uint32)bt].fills.PushBack(fb);
					}
					const int32 decl = stI.doc.ExtraireComposant(bt, "", "bouton");
					const int32 a1 = stI.doc.InstancierComposant(decl, pg);
					const int32 a2 = stI.doc.InstancierComposant(decl, pg);
					bool instanceOk = false;
					uint32 avantInst = 0u, apresInstAncien = 0u, apresInstNouveau = 0u;
					NkString cInst, cAutre;
					if (decl >= 0 && stI.doc.IsValidIndex(a1) && stI.doc.IsValidIndex(a2)) {
						stI.Recompute(NkPaintRect{0.f, 0.f, 600.f, 900.f});
						const nkentseu::uint32 vert = renderdetail::NkGCouleur("#2e7d32");
						avantInst = compter(vert);
						stI.SelectSingle(a1);
						souris(-1.f, -1.f, false);
						cliquerSV(a1, 0, 0);
						cInst = stI.doc.nodes[(uint32)a1].fills.Empty() ? NkString("(vide)") : stI.doc.nodes[(uint32)a1].fills[0].couleur;
						cAutre = stI.doc.nodes[(uint32)a2].fills.Empty() ? NkString("(vide)") : stI.doc.nodes[(uint32)a2].fills[0].couleur;
						apresInstAncien = compter(vert);
						apresInstNouveau = NkHexLisible(cInst.Data()) ? compter(renderdetail::NkGCouleur(cInst.Data())) : 0u;
						instanceOk = NkHexLisible(cInst.Data()) && !NkComponentDecl::StrEq(cInst.Data(), "#2e7d32")
									 && NkComponentDecl::StrEq(cAutre.Data(), "#2e7d32") && apresInstAncien < avantInst
									 && apresInstNouveau >= 1u;
					}
					stI.SelectSingle(rc);
					snprintf(det, sizeof(det),
							 "libre : #1976d2 -> %s, commandes bleues %u -> %u, nouvelles %u ; degrade : arret0 #fafcff -> %s, dessin change=%d ; "
							 "instance : #2e7d32 -> %s (l'autre : %s), vertes %u -> %u, nouvelles %u",
							 cLibre.Data(), avantLibre, apresLibreAncien, apresLibreNouveau, cArret.Data(), dessinDeg ? 1 : 0,
							 cInst.Data() ? cInst.Data() : "?", cAutre.Data() ? cAutre.Data() : "?", avantInst, apresInstAncien, apresInstNouveau);
					check("60h. LE SELECTEUR S'APPLIQUE A L'OBJET : un clic dans le carre SV change le modele ET la commande "
						  "de remplissage enregistree -- noeud libre, arret de degrade, instance (l'autre instance ne bouge pas)",
						  modeleLibre && apresLibreAncien < avantLibre && apresLibreNouveau >= 1u && modeleDeg && dessinDeg && instanceOk,
						  det);
				}
				// 60i. LE CAS DE RODOLF : son Bouton_Connexion porte la CLE SIMPLE `fond = #0969da`
				// (pas de liste). Le geste complet : clic sur la pastille de la LIGNE, survol
				// puis clic dans le carre SV -- et la commande de remplissage ENREGISTREE du
				// noeud doit avoir change de couleur. C'est ce qu'il voit ne pas se produire.
				{
					const int32 rs = stI.doc.AddChild(pg, "", NkAuthor::Humain);
					stI.doc.nodes[(uint32)rs].shape = NkString("rect");
					stI.doc.nodes[(uint32)rs].label = NkString("Bouton_Connexion");
					stI.doc.nodes[(uint32)rs].width.mode = NkSizeMode::Fixed;
					stI.doc.nodes[(uint32)rs].width.value = 180.f;
					stI.doc.nodes[(uint32)rs].height.mode = NkSizeMode::Fixed;
					stI.doc.nodes[(uint32)rs].height.value = 36.f;
					stI.doc.nodes[(uint32)rs].fill = NkString("#0969da"); // la cle simple, comme son fichier
					stI.Recompute(NkPaintRect{0.f, 0.f, 600.f, 900.f});
					stI.SelectSingle(rs);
					stI.picker = DesignState::DemandePicker();
					// L'ORDRE DE LA COQUILLE : le corps (les panneaux) voit une souris MASQUEE
					// des que le pointeur est sur un popup ; l'entree reelle est restauree
					// avant le crochet d'overlay. La sonde reproduit ce masquage.
					auto image2 = [&](float32 mx, float32 my, bool bas) {
						ctxI.input.mousePos = {mx, my};
						ctxI.input.mouseDown[0] = bas;
						ctxI.BeginFrame(0.016f);
						ctxI.BeginLayout({340.f, 0.f, 260.f, 900.f});
						bool surPopup = false;
						for (int32 i = 0; i < ctxI.popupDepth; ++i)
							if (nkgui::NkGuiRectContains(ctxI.popupRects[i], ctxI.input.mousePos))
								surPopup = true;
						nkgui::NkGuiInput sauve = ctxI.input;
						if (surPopup) {
							ctxI.input.mousePos = {-100000.f, -100000.f};
							for (int32 i = 0; i < 3; ++i) {
								ctxI.input.mouseClicked[i] = false;
								ctxI.input.mouseDown[i] = false;
								ctxI.input.mouseDoubleClicked[i] = false;
							}
						}
						insp.OnUI(ec);
						if (surPopup)
							ctxI.input = sauve;
						NkDessinerPickerDemande(ctxI, stI);
						ctxI.EndFrame();
					};
					auto compterBleu = [&](nkentseu::uint32 rgba) {
						NkRecordingPaint rec;
						RenderDocument(rec, stI.doc, NkPaintRect{0.f, 0.f, 600.f, 900.f});
						uint32 n = 0u;
						for (uint32 i = 0; i < (uint32)rec.cmds.Size(); ++i)
							if ((rec.cmds[i].op == NkPaintOp::Fill || rec.cmds[i].op == NkPaintOp::FillColor) && rec.cmds[i].rgba == rgba)
								++n;
						return n;
					};
					const nkentseu::uint32 bleuR = renderdetail::NkGCouleur("#0969da");
					const uint32 avant = compterBleu(bleuR);
					image2(-1.f, -1.f, false);
					image2(-1.f, -1.f, false);
					// la pastille de la ligne : le rectangle plein de sa couleur dans la couche du panneau
					float32 sx0 = 1e9f, sy0 = 1e9f, sx1 = -1e9f, sy1 = -1e9f;
					for (uint32 i = 0; i < (uint32)ctxI.dl.vtx.Size(); ++i) {
						const auto &vt = ctxI.dl.vtx[i];
						if (vt.col == 0xFFDA6909u || vt.col == 0x0969DAFFu) {
							if (vt.pos.x < sx0) sx0 = vt.pos.x;
							if (vt.pos.y < sy0) sy0 = vt.pos.y;
							if (vt.pos.x > sx1) sx1 = vt.pos.x;
							if (vt.pos.y > sy1) sy1 = vt.pos.y;
						}
					}
					const bool pastilleVue = sx1 > sx0 && sy1 > sy0 && sx1 - sx0 < 40.f;
					const float32 pcx = (sx0 + sx1) * 0.5f, pcy = (sy0 + sy1) * 0.5f;
					image2(pcx, pcy, false);
					image2(pcx, pcy, true);
					image2(pcx, pcy, false);
					image2(pcx, pcy, false);
					const bool demande = stI.picker.ouvert && stI.picker.genre == 1u && stI.picker.noeud == rs;
					// le popover : sa boite, puis le carre SV
					float32 px = 1e9f, py = 1e9f;
					for (uint32 i = 0; i < (uint32)ctxI.dlOverlay.vtx.Size(); ++i) {
						if (ctxI.dlOverlay.vtx[i].pos.x < px) px = ctxI.dlOverlay.vtx[i].pos.x;
						if (ctxI.dlOverlay.vtx[i].pos.y < py) py = ctxI.dlOverlay.vtx[i].pos.y;
					}
					const float32 cx = px + 0.5f + 8.f + 80.f, cy = py + 0.5f + 8.f + 26.f + 80.f;
					image2(cx, cy, false);
					image2(cx, cy, true);
					image2(cx, cy, false);
					image2(-1.f, -1.f, false);
					const NkUINode &ns = stI.doc.nodes[(uint32)rs];
					const NkString cListe = ns.fills.Empty() ? NkString("(aucune)") : ns.fills[0].couleur;
					const uint32 apresAncien = compterBleu(bleuR);
					const uint32 apresNouveau = NkHexLisible(cListe.Data()) ? compterBleu(renderdetail::NkGCouleur(cListe.Data())) : 0u;
					stI.picker = DesignState::DemandePicker();
					image2(-1.f, -1.f, false);
					snprintf(det, sizeof(det),
							 "pastille vue=%d a (%.0f,%.0f) ; demande=%d ; cle simple `%s`, liste `%s` ; commandes #0969da %u -> %u, "
							 "nouvelles %u",
							 pastilleVue ? 1 : 0, pcx, pcy, demande ? 1 : 0, ns.fill.Data() ? ns.fill.Data() : "", cListe.Data(),
							 avant, apresAncien, apresNouveau);
					check("60i. LE CAS DE RODOLF (cle simple `fond = #0969da`, par la pastille de la LIGNE) : le clic "
						  "dans le carre SV change la couleur PEINTE du noeud",
						  pastilleVue && demande && avant >= 1u && apresAncien < avant && apresNouveau >= 1u, det);
				}
				// 60k. ⑥ LES POIGNEES SUR LA TOILE, popover de remplissage OUVERT (c'est ainsi qu'on
				// regle un degrade) : glisser l'EXTREMITE oriente le degrade (angle) et ne deplace
				// pas le noeud ; un arret intermediaire glisse ; hors poignee, la toile deplace
				// bien le noeud (controle). Rodolf : « ca deplace plutot la geometrie ».
				{
					static PreviewPanel toile(&stI);
					stI.SelectSingle(rc);
					stI.picker = DesignState::DemandePicker();
					stI.picker.ouvert = true;
					stI.picker.id = ctxI.GetId("##sonde.popover.toile");
					stI.picker.genre = 1u;
					stI.picker.noeud = rc;
					stI.picker.index = 1;
					stI.picker.ancre = {590.f, 20.f, 16.f, 16.f}; // le popover se pose a droite, loin de la toile
					auto scene = [&](float32 mx, float32 my, bool bas) {
						ctxI.input.mousePos = {mx, my};
						ctxI.input.mouseDown[0] = bas;
						ctxI.BeginFrame(0.016f);
						ctxI.BeginLayout({0.f, 0.f, 340.f, 900.f});
						toile.OnUI(ec);
						ctxI.BeginLayout({340.f, 0.f, 260.f, 900.f});
						insp.OnUI(ec);
						NkDessinerPickerDemande(ctxI, stI);
						ctxI.EndFrame();
					};
					scene(-1.f, -1.f, false);
					scene(-1.f, -1.f, false);
					NkLayoutResult scr;
					stI.ProjectToScreen(scr);
					NkUINode &nd = stI.doc.nodes[(uint32)rc];
					NkDegrade &gd = nd.fills[1].degrade;
					gd.angle = 0.f;
					const NkPaintRect rs = scr.At(rc);
					const renderdetail::NkAxeDegrade axe = renderdetail::NkAxeDegradeDe(rs, gd);
					const float32 posX0 = nd.posX, posY0 = nd.posY;
					const int32 prof = ctxI.popupDepth;
					// 1. l'extremite (t = 1) : on la tire a GAUCHE du centre -> angle 90
					float32 hx = 0.f, hy = 0.f;
					renderdetail::NkPoigneeDegrade(axe, 1.f, hx, hy);
					const float32 cx = rs.x + rs.w * 0.5f, cy = rs.y + rs.h * 0.5f;
					scene(hx, hy, false);
					scene(hx, hy, true);
					scene(hx - 10.f, hy - 10.f, true);
					scene(cx - 60.f, cy, true);
					scene(cx - 60.f, cy, false);
					scene(-1.f, -1.f, false);
					const float32 angle1 = gd.angle;
					const bool noeudFixe1 = nd.posX == posX0 && nd.posY == posY0;
					// 2. l'arret du milieu (t = 0.5) glisse vers 0.75 le long du nouvel axe
					const renderdetail::NkAxeDegrade axe2 = renderdetail::NkAxeDegradeDe(rs, gd);
					float32 mx0 = 0.f, my0 = 0.f, mx1 = 0.f, my1 = 0.f;
					renderdetail::NkPoigneeDegrade(axe2, 0.5f, mx0, my0);
					renderdetail::NkPoigneeDegrade(axe2, 0.75f, mx1, my1);
					scene(mx0, my0, false);
					scene(mx0, my0, true);
					scene((mx0 + mx1) * 0.5f, (my0 + my1) * 0.5f, true);
					scene(mx1, my1, true);
					scene(mx1, my1, false);
					scene(-1.f, -1.f, false);
					const float32 pos1 = gd.arrets[1].position;
					const bool noeudFixe2 = nd.posX == posX0 && nd.posY == posY0;
					// 3. CONTROLE : hors des poignees et du segment, la toile deplace le noeud
					// un point du CORPS : a plus de 8 px des bords (sinon c'est une poignee de
					// taille) et a plus de 8 px de l'axe horizontal (angle 90) et de ses arrets
					float32 ox = cx - 30.f, oy = cy - 7.f;
					// plusieurs noeuds des sondes precedentes se superposent a l'origine du cadre :
					// c'est celui du DESSUS que la toile deplace -- on juge « un noeud a bouge »
					NkVector<float32> avantX, avantY;
					for (uint32 i = 0; i < (uint32)stI.doc.nodes.Size(); ++i) {
						avantX.PushBack(stI.doc.nodes[i].posX);
						avantY.PushBack(stI.doc.nodes[i].posY);
					}
					scene(ox, oy, false);
					scene(ox, oy, true);
					scene(ox + 15.f, oy + 9.f, true);
					scene(ox + 30.f, oy + 18.f, true);
					scene(ox + 30.f, oy + 18.f, false);
					scene(-1.f, -1.f, false);
					bool noeudBouge = false;
					for (uint32 i = 0; i < (uint32)stI.doc.nodes.Size(); ++i) {
						if (stI.doc.nodes[i].posX != avantX[i] || stI.doc.nodes[i].posY != avantY[i])
							noeudBouge = true;
						stI.doc.nodes[i].posX = avantX[i];
						stI.doc.nodes[i].posY = avantY[i];
					}
					stI.Recompute(NkPaintRect{0.f, 0.f, 600.f, 900.f});
					stI.picker = DesignState::DemandePicker();
					scene(-1.f, -1.f, false);
					snprintf(det, sizeof(det),
							 "popover ouvert (profondeur %d) ; extremite tiree a gauche du centre : angle 0 -> %.1f, noeud fixe=%d ; "
							 "arret du milieu : 0.50 -> %.2f, noeud fixe=%d ; hors poignee : le noeud bouge=%d",
							 prof, (double)angle1, noeudFixe1 ? 1 : 0, (double)pos1, noeudFixe2 ? 1 : 0, noeudBouge ? 1 : 0);
					if (!noeudBouge) {
						const size_t l = strlen(det);
						snprintf(det + l, sizeof(det) - l, " ; statut : %s", stI.status.Data() ? stI.status.Data() : "");
					}
					check("60k. ⑥ SUR LA TOILE, POPOVER OUVERT : glisser l'extremite ORIENTE le degrade (angle) sans "
						  "deplacer le noeud, un arret intermediaire glisse le long de l'axe, et hors poignee la toile "
						  "deplace bien le noeud",
						  prof >= 1 && angle1 > 80.f && angle1 < 100.f && noeudFixe1 && pos1 > 0.65f && pos1 < 0.85f && noeudFixe2 && noeudBouge,
						  det);
				}
				// 60j. ② LA SAISIE DIRECTE DU CODE COULEUR, par UNE porte (NkPorteHex / NkPorteNombre) :
				// l'hexa du popover (`ff0000` sans #), un champ de valeur (clic net -> frappe, `0`
				// dans R), l'hexa sur la LIGNE (`00ff00`), et `zz` refuse sans rien perdre.
				{
					// ⚠️ SANS POLICE (pas de fenetre), le champ du kit ne « consomme » pas le clic
					//    (il lui faut une face pour poser le caret) et EndFrame defocalise. La sonde
					//    le dit et consomme elle-meme le clic d'appui sur un champ texte.
					auto frappe = [&](float32 mx, float32 my, bool bas, const char *texte, bool entree, bool toutSel, bool appuiChamp = false) {
						ctxI.input.mousePos = {mx, my};
						ctxI.input.mouseDown[0] = bas;
						if (entree)
							ctxI.input.SetKey(nkgui::NkGuiKey::Enter, true);
						ctxI.BeginFrame(0.016f);
						if (toutSel)
							ctxI.input.wantSelectAll = true;
						for (const char *q = texte; q && *q; ++q)
							ctxI.input.PushChar((uint32)(unsigned char)*q);
						ctxI.BeginLayout({340.f, 0.f, 260.f, 900.f});
						insp.OnUI(ec);
						NkDessinerPickerDemande(ctxI, stI);
						if (appuiChamp && ctxI.inputId != 0u)
							ctxI.inputClickConsumed = true;
						ctxI.EndFrame();
						if (entree)
							ctxI.input.SetKey(nkgui::NkGuiKey::Enter, false);
					};
					// vider le champ comme a la main : Fin, puis neuf Retour arriere (le
					// caret d'un clic sans police est au debut ; « tout selectionner » est
					// une commande de la coquille, pas une touche)
					auto touche = [&](nkgui::NkGuiKey k, float32 mx, float32 my) {
						ctxI.input.SetKey(k, true);
						frappe(mx, my, false, "", false, false);
						ctxI.input.SetKey(k, false);
						frappe(mx, my, false, "", false, false);
					};
					auto vider = [&](float32 mx, float32 my) {
						touche(nkgui::NkGuiKey::End, mx, my);
						for (int32 k = 0; k < 9; ++k)
							touche(nkgui::NkGuiKey::Backspace, mx, my);
					};
					auto peint = [&](const char *hex) {
						NkRecordingPaint rec;
						RenderDocument(rec, stI.doc, NkPaintRect{0.f, 0.f, 600.f, 900.f});
						const nkentseu::uint32 rgba = renderdetail::NkGCouleur(hex);
						uint32 n = 0u;
						for (uint32 i = 0; i < (uint32)rec.cmds.Size(); ++i)
							if ((rec.cmds[i].op == NkPaintOp::Fill || rec.cmds[i].op == NkPaintOp::FillColor) && rec.cmds[i].rgba == rgba)
								++n;
						return n;
					};
					// 1. l'hexa du popover, sur rc fills[0]
					stI.SelectSingle(rc);
					stI.picker = DesignState::DemandePicker();
					stI.picker.ouvert = true;
					stI.picker.id = ctxI.GetId("##sonde.popover.saisie");
					stI.picker.genre = 1u;
					stI.picker.noeud = rc;
					stI.picker.index = 0;
					stI.picker.ancre = {360.f, 200.f, 16.f, 16.f};
					frappe(-1.f, -1.f, false, "", false, false);
					frappe(-1.f, -1.f, false, "", false, false);
					float32 px = 1e9f, py = 1e9f, pxM = -1e9f;
					for (uint32 i = 0; i < (uint32)ctxI.dlOverlay.vtx.Size(); ++i) {
						if (ctxI.dlOverlay.vtx[i].pos.x < px) px = ctxI.dlOverlay.vtx[i].pos.x;
						if (ctxI.dlOverlay.vtx[i].pos.y < py) py = ctxI.dlOverlay.vtx[i].pos.y;
						if (ctxI.dlOverlay.vtx[i].pos.x > pxM) pxM = ctxI.dlOverlay.vtx[i].pos.x;
					}
					const float32 x0 = px + 0.5f + 8.f, x1 = pxM - 0.5f - 8.f;
					const float32 yR = py + 0.5f + 8.f + 26.f + 168.f + 3.f;
					nkgui::NkRect rmM, chM[3], opM;
					NkRangeeModele(x0, x1, yR, rmM, chM, opM);
					const float32 hx = chM[0].x + 20.f, hy = yR + 10.f;
					// le modele est peut-etre reste sur HSB (sonde 60g) : on remet Hex par le menu
					frappe(rmM.x + 20.f, hy, false, "", false, false);
					frappe(rmM.x + 20.f, hy, true, "", false, false);
					frappe(rmM.x + 20.f, hy, false, "", false, false);
					const float32 hexY = rmM.y - 9.f * 20.f - 4.f + 2.f + 10.f; // la rangee Hex (k=0)
					frappe(rmM.x + 20.f, hexY, false, "", false, false);
					frappe(rmM.x + 20.f, hexY, true, "", false, false);
					frappe(rmM.x + 20.f, hexY, false, "", false, false);
					frappe(-1.f, -1.f, false, "", false, false);
					frappe(hx, hy, false, "", false, false);
					frappe(hx, hy, true, "", false, false, true);
					frappe(hx, hy, false, "", false, false);
					const bool focus1 = ctxI.inputId != 0u && ctxI.popupDepth == 1;
					vider(hx, hy);
					frappe(hx, hy, false, "ff0000", false, false);
					frappe(hx, hy, false, "", true, false);
					frappe(-1.f, -1.f, false, "", false, false);
					const NkString c1 = stI.doc.nodes[(uint32)rc].fills[0].couleur;
					const bool hexaPopover = NkComponentDecl::StrEq(c1.Data(), "#ff0000") && peint("#ff0000") >= 1u;
					// 2. le refus : `zz` ne change rien et ne perd rien
					frappe(hx, hy, false, "", false, false);
					frappe(hx, hy, true, "", false, false, true);
					frappe(hx, hy, false, "", false, false);
					vider(hx, hy);
					frappe(hx, hy, false, "zz", false, false);
					frappe(hx, hy, false, "", true, false);
					frappe(-1.f, -1.f, false, "", false, false);
					const bool refus = NkComponentDecl::StrEq(stI.doc.nodes[(uint32)rc].fills[0].couleur.Data(), "#ff0000");
					// 3. le champ R : menu du modele -> RGB, clic net sur le champ 0, `0`, Entree
					frappe(rmM.x + 20.f, hy, false, "", false, false);
					frappe(rmM.x + 20.f, hy, true, "", false, false);
					frappe(rmM.x + 20.f, hy, false, "", false, false);
					const float32 ry = rmM.y - 9.f * 20.f - 4.f + 2.f + 1.f * 20.f + 10.f; // la rangee RGB (k=1)
					frappe(rmM.x + 20.f, ry, false, "", false, false);
					frappe(rmM.x + 20.f, ry, true, "", false, false);
					frappe(rmM.x + 20.f, ry, false, "", false, false);
					frappe(-1.f, -1.f, false, "", false, false);
					frappe(hx, hy, false, "", false, false);
					frappe(hx, hy, true, "", false, false, true);
					frappe(hx, hy, false, "", false, false); // le clic net : la saisie s'ouvre
					frappe(hx, hy, false, "0", false, true);
					frappe(hx, hy, false, "", true, false);
					frappe(-1.f, -1.f, false, "", false, false);
					const NkString c3 = stI.doc.nodes[(uint32)rc].fills[0].couleur;
					const bool champR = NkComponentDecl::StrEq(c3.Data(), "#000000");
					stI.picker = DesignState::DemandePicker();
					frappe(-1.f, -1.f, false, "", false, false);
					// 4. l'hexa sur la LIGNE du noeud a cle simple (rs) : `00ff00`
					int32 rs2 = -1;
					for (uint32 i = 0; i < (uint32)stI.doc.nodes.Size(); ++i)
						if (NkComponentDecl::StrEq(stI.doc.nodes[i].label.Data(), "Bouton_Connexion"))
							rs2 = (int32)i;
					bool ligne = false;
					NkString c4;
					if (rs2 >= 0) {
						stI.SelectSingle(rs2);
						frappe(-1.f, -1.f, false, "", false, false);
						frappe(-1.f, -1.f, false, "", false, false);
						const NkString cAv = stI.doc.nodes[(uint32)rs2].fills.Empty() ? stI.doc.nodes[(uint32)rs2].fill
																				  : stI.doc.nodes[(uint32)rs2].fills[0].couleur;
						const nkentseu::uint32 rgbaAv = renderdetail::NkGCouleur(cAv.Data());
						float32 sx0 = 1e9f, sy0 = 1e9f, sx1 = -1e9f, sy1 = -1e9f;
						for (uint32 i = 0; i < (uint32)ctxI.dl.vtx.Size(); ++i) {
							const auto &vt = ctxI.dl.vtx[i];
							if (vt.col == rgbaAv || vt.col == ((rgbaAv >> 24) | ((rgbaAv >> 8) & 0xFF00u) | ((rgbaAv << 8) & 0xFF0000u) | (rgbaAv << 24))) {
								if (vt.pos.x < sx0) sx0 = vt.pos.x;
								if (vt.pos.y < sy0) sy0 = vt.pos.y;
								if (vt.pos.x > sx1) sx1 = vt.pos.x;
								if (vt.pos.y > sy1) sy1 = vt.pos.y;
							}
						}
						const float32 lx = sx1 + 30.f, ly = (sy0 + sy1) * 0.5f;
						frappe(lx, ly, false, "", false, false);
						frappe(lx, ly, true, "", false, false, true);
						frappe(lx, ly, false, "", false, false);
						vider(lx, ly);
						frappe(lx, ly, false, "00ff00", false, false);
						frappe(lx, ly, false, "", true, false);
						frappe(-1.f, -1.f, false, "", false, false);
						c4 = stI.doc.nodes[(uint32)rs2].fills.Empty() ? NkString("(aucune)") : stI.doc.nodes[(uint32)rs2].fills[0].couleur;
						ligne = NkComponentDecl::StrEq(c4.Data(), "#00ff00") && peint("#00ff00") >= 1u;
					}
					stI.SelectSingle(rc);
					snprintf(det, sizeof(det),
							 "champ focalise dans le popover=%d ; popover : `ff0000` -> %s ; `zz` refuse=%d ; champ R `0` -> %s ; ligne : `00ff00` -> %s",
							 focus1 ? 1 : 0, c1.Data(), refus ? 1 : 0, c3.Data(), c4.Data() ? c4.Data() : "?");
					check("60j. ② LA SAISIE DIRECTE DU CODE COULEUR par UNE porte : l'hexa du popover (sans #), un champ "
						  "de valeur au clic net, l'hexa sur la LIGNE -- la couleur peinte suit, `zz` est refuse sans rien perdre",
						  focus1 && hexaPopover && refus && champR && ligne, det);
				}
				stI.picker = DesignState::DemandePicker();
			}
		}
		// ── 61. LES POIGNEES DE DEGRADE : une geometrie, deux lecteurs ───────
		{
			using renderdetail::NkAxeDegrade;
			using renderdetail::NkAxeDegradeDe;
			using renderdetail::NkPoigneeDegrade;
			using renderdetail::NkQuiPrendLeClicDegrade;
			NkDegrade g;
			for (uint32 k = 0; k < 3u; ++k) {
				NkArretDegrade ar;
				ar.position = (float32)k * 0.5f;
				ar.couleur = NkString("#ff0000");
				g.arrets.PushBack(ar);
			}
			const NkPaintRect r{100.f, 100.f, 120.f, 60.f};
			char det[300];
			// 61a. angle 0 (haut -> bas) : trois poignees sur la verticale du centre
			NkAxeDegrade a0 = NkAxeDegradeDe(r, g);
			float32 x0, y0, x1, y1, x2, y2;
			NkPoigneeDegrade(a0, 0.f, x0, y0);
			NkPoigneeDegrade(a0, 0.5f, x1, y1);
			NkPoigneeDegrade(a0, 1.f, x2, y2);
			const bool vertical = x0 == 160.f && y0 == 100.f && x1 == 160.f && y1 == 130.f && x2 == 160.f && y2 == 160.f;
			// angle 270 (gauche -> droite) : sur l'horizontale du centre
			g.angle = 270.f;
			NkAxeDegrade a270 = NkAxeDegradeDe(r, g);
			float32 hx0, hy0, hx2, hy2;
			NkPoigneeDegrade(a270, 0.f, hx0, hy0);
			NkPoigneeDegrade(a270, 1.f, hx2, hy2);
			const bool horizontal = hx0 > 99.9f && hx0 < 100.1f && hy0 > 129.9f && hy0 < 130.1f && hx2 > 219.9f
									&& hx2 < 220.1f;
			g.angle = 0.f;
			snprintf(det, sizeof(det), "angle 0 : (%.0f,%.0f) (%.0f,%.0f) (%.0f,%.0f) ; angle 270 : (%.0f,%.0f) -> (%.0f,%.0f)",
					 x0, y0, x1, y1, x2, y2, hx0, hy0, hx2, hy2);
			check("61a. UNE POIGNEE PAR ARRET, a sa position sur l'axe : trois arrets = trois poignees (0, 50, 100 %), "
				  "et l'axe tourne avec l'angle -- la meme convention que le peintre",
				  vertical && horizontal, det);
			// 61b. UNE SEULE DECISION, ORDONNEE : a 6.5 px de la poignee du milieu, c'est la POIGNEE (pas un arret
			// parasite) ; sur le segment loin des poignees, c'est le SEGMENT (-2) ; ailleurs, la toile (-1)
			const int32 surPoignee = NkQuiPrendLeClicDegrade(a0, g, 160.f, 136.5f, 8.f, 5.f);
			const int32 surSegment = NkQuiPrendLeClicDegrade(a0, g, 161.f, 145.f, 8.f, 5.f);
			const int32 surRien = NkQuiPrendLeClicDegrade(a0, g, 190.f, 145.f, 8.f, 5.f);
			const int32 auBord = NkQuiPrendLeClicDegrade(a0, g, 167.5f, 130.f, 8.f, 5.f); // au bord de la pastille
			snprintf(det, sizeof(det), "a 6.5 px sous la poignee du milieu -> %d (poignee 1) ; sur le segment -> %d (-2) ; "
									   "loin -> %d (-1) ; au bord de la pastille -> %d (poignee 1)",
					 surPoignee, surSegment, surRien, auBord);
			check("61b. QUI PREND LE CLIC : la poignee AVANT le segment, le segment AVANT la toile -- un clic au "
				  "bord d'une pastille ne cree jamais un arret parasite",
				  surPoignee == 1 && surSegment == -2 && surRien == -1 && auBord == 1, det);
			// 61c. seul le LINEAIRE a ses poignees : un type nomme, pas peint, n'en montre pas
			NkUINode nL, nR;
			NkRemplissage fL;
			fL.degrade = g;
			nL.fills.PushBack(fL);
			NkRemplissage fR;
			fR.degrade = g;
			fR.degrade.type = NkString("radial");
			nR.fills.PushBack(fR);
			const int32 iL = renderdetail::NkRemplissageDegradeToile(nL), iR = renderdetail::NkRemplissageDegradeToile(nR);
			snprintf(det, sizeof(det), "lineaire -> remplissage %d ; radial (nomme, pas peint) -> %d", iL, iR);
			check("61c. seules les poignees du LINEAIRE se montrent : une poignee sur un degrade que le peintre ne rend "
				  "pas serait un dessin faux",
				  iL == 0 && iR == -1, det);
		}
		// ── 62. LE REMPLISSAGE IMAGE : un type, cinq cadrages, un damier dit ───
		{
			NkUIDocument dIm;
			dIm.NewDocument("Toile", NkAuthor::Humain);
			const int32 f = dIm.AddChild(0, "", NkAuthor::Humain);
			{
				NkUINode &n = dIm.nodes[(uint32)f];
				n.shape = NkString("rect");
				n.posX = 100.f;
				n.posY = 100.f;
				n.width.mode = NkSizeMode::Fixed;
				n.width.value = 64.f;
				n.height.mode = NkSizeMode::Fixed;
				n.height.value = 32.f;
				NkRemplissage r;
				r.genre = NkString("image");
				r.cadrage = NkString("tile");
				r.image = NkString("photos/stand.png");
				r.rotationImage = 90.f;
				n.fills.PushBack(r);
			}
			char det[300];
			// 62a. le format : jetons additifs, aller-retour, inconnu preserve, Fill = rien
			NkString s1;
			dIm.Save(s1);
			const bool cles = strstr(s1.Data(), "genre=image") && strstr(s1.Data(), "cadrage=tile")
							  && strstr(s1.Data(), "image=photos/stand.png") && strstr(s1.Data(), "rotation_image=90");
			NkString s2;
			const char *pos = strstr(s1.Data(), "rotation_image=90");
			const uint32 coupe = (uint32)(pos - s1.Data()) + 17u;
			for (uint32 i = 0; i < coupe; ++i)
				s2.Append(s1.Data()[i]);
			s2.Append(" futur=oui");
			s2.Append(s1.Data() + coupe);
			NkUIDocument relu;
			const bool ok = relu.Load(s2.Data());
			const NkRemplissage *rf = (ok && relu.IsValidIndex(f) && !relu.nodes[(uint32)f].fills.Empty())
										  ? &relu.nodes[(uint32)f].fills[0] : nullptr;
			const bool relus = rf && rf->EstImage() && NkComponentDecl::StrEq(rf->cadrage.Data(), "tile")
							   && NkComponentDecl::StrEq(rf->image.Data(), "photos/stand.png") && rf->rotationImage == 90.f
							   && NkComponentDecl::StrEq(rf->inconnus.Data(), "futur=oui");
			NkString s3;
			if (ok)
				relu.Save(s3);
			const bool reemis = ok && strstr(s3.Data(), "futur=oui") != nullptr;
			// un remplissage uni d'avant ne gagne aucun jeton
			NkUIDocument dUni;
			dUni.NewDocument("Toile", NkAuthor::Humain);
			const int32 u = dUni.AddChild(0, "", NkAuthor::Humain);
			NkRemplissage ru;
			ru.couleur = NkString("#123456");
			dUni.nodes[(uint32)u].fills.PushBack(ru);
			NkString sU;
			dUni.Save(sU);
			const bool additif = strstr(sU.Data(), "genre=") == nullptr && strstr(sU.Data(), "cadrage=") == nullptr;
			snprintf(det, sizeof(det), "cles=%d relus=%d reemis=%d ; un uni d'avant sans jeton=%d", cles ? 1 : 0,
					 relus ? 1 : 0, reemis ? 1 : 0, additif ? 1 : 0);
			check("62a. L'IMAGE EST UN TYPE DE REMPLISSAGE (tranche par sa capture) : `genre=image cadrage=... "
				  "image=... rotation_image=...`, additifs, relus, l'inconnu preserve, Fill = rien au fichier",
				  cles && relus && reemis && additif, det);
			// 62b. le peintre : le DAMIER, dans la boite, dit -- pas une image inventee
			NkPaintRect surfIm;
			surfIm.x = 0.f;
			surfIm.y = 0.f;
			surfIm.w = 800.f;
			surfIm.h = 600.f;
			NkRecordingPaint rec;
			RenderDocument(rec, dIm, surfIm);
			uint32 clairs = 0u, hors = 0u;
			bool fond = false;
			for (uint32 i = 0; i < (uint32)rec.cmds.Size(); ++i) {
				const NkPaintCmd &c = rec.cmds[i];
				if (c.op != NkPaintOp::FillColor)
					continue;
				if ((c.rgba >> 8) == 0xD4D4D4u) {
					++clairs;
					if (c.x < 99.9f || c.y < 99.9f || c.x + c.w > 164.1f || c.y + c.h > 132.1f)
						++hors;
				} else if ((c.rgba >> 8) == 0x9A9A9Au)
					fond = true;
			}
			snprintf(det, sizeof(det), "fond gris=%d, %u carreaux clairs (64x32 / 8 px : 16 attendus), %u hors de la boite", fond ? 1 : 0,
					 clairs, hors);
			check("62b. le peintre montre le DAMIER d'une image absente (fond gris + carreaux clairs de 8 px, tous "
				  "dans la boite) -- la source n'est pas chargee, et c'est dit, pas simule",
				  fond && clairs == 16u && hors == 0u, det);
		}
		// ── 63. LA VARIABLE DE COULEUR : reference, litteral, absente, modes, Q51 ─
		{
			auto rectAvecFond = [](NkUIDocument &d, int32 parent, const char *couleur) {
				const int32 f = d.AddChild(parent, "", NkAuthor::Humain);
				NkUINode &n = d.nodes[(uint32)f];
				n.shape = NkString("rect");
				n.posX = 10.f;
				n.posY = 10.f;
				n.width.mode = NkSizeMode::Fixed;
				n.width.value = 40.f;
				n.height.mode = NkSizeMode::Fixed;
				n.height.value = 40.f;
				NkRemplissage r;
				r.couleur = NkString(couleur);
				n.fills.PushBack(r);
				return f;
			};
			auto couleurPeinte = [](NkUIDocument &d, int32 noeud) -> uint32 {
				NkPaintRect s;
				s.x = 0.f;
				s.y = 0.f;
				s.w = 600.f;
				s.h = 400.f;
				NkLayoutResult lay;
				NkComputeLayout(d, s, lay);
				const NkPaintRect r = lay.At(noeud);
				// on ne peint QUE ce noeud : tout le reste est masque le temps de la mesure
				NkVector<uint8> masques;
				for (uint32 i = 0; i < (uint32)d.nodes.Size(); ++i) {
					masques.PushBack(d.nodes[i].masque ? 1u : 0u);
					if (i != 0u && (int32)i != noeud)
						d.nodes[i].masque = true;
				}
				NkRecordingPaint rec;
				RenderDocument(rec, d, s);
				for (uint32 i = 0; i < (uint32)d.nodes.Size(); ++i)
					d.nodes[i].masque = masques[i] != 0u;
				uint32 dernier = 0u;
				for (uint32 i = 0; i < (uint32)rec.cmds.Size(); ++i) {
					const NkPaintCmd &c = rec.cmds[i];
					if (c.op == NkPaintOp::FillColor && c.x == r.x && c.y == r.y && c.w == r.w && c.h == r.h)
						dernier = c.rgba;
				}
				return dernier;
			};
			char det[320];
			NkUIDocument dV;
			dV.NewDocument("Toile", NkAuthor::Humain);
			NkVariable prim;
			prim.cle = NkString("primaire");
			prim.nom = NkString("Dark Primary");
			prim.valeur = NkString("#1976d2");
			NkValeurMode sombre;
			sombre.mode = NkString("sombre");
			sombre.valeur = NkString("#0d47a1");
			prim.parMode.PushBack(sombre);
			dV.variables.PushBack(prim);
			const int32 ref = rectAvecFond(dV, 0, "@primaire");
			const int32 lit = rectAvecFond(dV, 0, "#ff0000");
			const int32 abs = rectAvecFond(dV, 0, "@inconnue");
			// 63a. reference, litteral, absente
			const uint32 cRef = couleurPeinte(dV, ref), cLit = couleurPeinte(dV, lit), cAbs = couleurPeinte(dV, abs);
			const bool absenteDite = renderdetail::NkResolveurCourant().derniereAbsente != nullptr;
			snprintf(det, sizeof(det), "reference -> %08X (1976D2FF), litteral -> %08X (FF0000FF), absente -> %08X (magenta), dite=%d",
					 cRef, cLit, cAbs, absenteDite ? 1 : 0);
			check("63a. UNE REFERENCE (« @primaire ») se peint a la valeur de la variable, un litteral reste lui-meme, "
				  "et une variable ABSENTE se voit (magenta) et se DIT -- jamais silencieusement noire",
				  cRef == 0x1976D2FFu && cLit == 0xFF0000FFu && cAbs == 0xFF00FFFFu && absenteDite, det);
			// 63b. les MODES : le meme document rend differemment selon son mode courant
			dV.modeCourant = NkString("sombre");
			const uint32 cSombre = couleurPeinte(dV, ref);
			dV.modeCourant = NkString("clair"); // un mode que la variable ne declare pas : la valeur par defaut
			const uint32 cClair = couleurPeinte(dV, ref);
			dV.modeCourant = NkString();
			snprintf(det, sizeof(det), "mode sombre -> %08X (0D47A1FF) ; mode clair (non declare) -> %08X (defaut 1976D2FF)", cSombre, cClair);
			check("63b. LES MODES, place reservee : la meme reference rend la valeur du mode courant, et un mode "
				  "que la variable ne declare pas retombe sur sa valeur par defaut -- Dark Pro / Light Pro sans recopier",
				  cSombre == 0x0D47A1FFu && cClair == 0x1976D2FFu, det);
			// 63c. le format : la ligne `variable`, le nom entre guillemets, le mode, l'inconnu preserve, additif
			dV.modeCourant = NkString("sombre");
			NkString s1;
			dV.Save(s1);
			const bool cles = strstr(s1.Data(), "variable = primaire #1976d2 nom=\"Dark Primary\" @sombre=#0d47a1") != nullptr
							  && strstr(s1.Data(), "mode = sombre") != nullptr && strstr(s1.Data(), "fond_1 = @primaire ") != nullptr;
			NkString s2;
			const char *pos = strstr(s1.Data(), "@sombre=#0d47a1");
			const uint32 coupe = (uint32)(pos - s1.Data()) + 15u;
			for (uint32 i = 0; i < coupe; ++i)
				s2.Append(s1.Data()[i]);
			s2.Append(" futur=x");
			s2.Append(s1.Data() + coupe);
			NkUIDocument relu;
			const bool ok = relu.Load(s2.Data());
			const NkVariable *rv = ok ? relu.TrouverVariable("primaire") : nullptr;
			const bool relus = rv && NkComponentDecl::StrEq(rv->nom.Data(), "Dark Primary")
							   && NkComponentDecl::StrEq(rv->valeur.Data(), "#1976d2") && rv->parMode.Size() == 1u
							   && NkComponentDecl::StrEq(rv->parMode[0].valeur.Data(), "#0d47a1")
							   && NkComponentDecl::StrEq(rv->inconnus.Data(), "futur=x")
							   && NkComponentDecl::StrEq(relu.modeCourant.Data(), "sombre")
							   && relu.IsValidIndex(ref) && NkComponentDecl::StrEq(relu.nodes[(uint32)ref].fills[0].couleur.Data(), "@primaire");
			NkString s3;
			if (ok)
				relu.Save(s3);
			const bool reemis = ok && strstr(s3.Data(), "futur=x") != nullptr;
			NkUIDocument dSans;
			dSans.NewDocument("Toile", NkAuthor::Humain);
			NkString sS;
			dSans.Save(sS);
			const bool additif = strstr(sS.Data(), "variable = ") == nullptr && strstr(sS.Data(), "mode = ") == nullptr;
			dV.modeCourant = NkString();
			snprintf(det, sizeof(det), "cles=%d relus=%d reemis=%d ; document sans variable : aucune ligne=%d", cles ? 1 : 0,
					 relus ? 1 : 0, reemis ? 1 : 0, additif ? 1 : 0);
			check("63c. LE FORMAT : `variable = <cle> <valeur> nom=\"...\" @mode=valeur`, `mode = ...`, la reference "
				  "« @primaire » dans `fond_`, l'inconnu preserve, et un document sans variable ne gagne aucune ligne",
				  cles && relus && reemis && additif, det);
			// 63d. Q51 par la reference : un composant dont le fond est « @primaire », deux instances ;
			// l'une pose un litteral (surcharge locale) ; on change la variable -> l'autre suit, elle tient
			const int32 bt = rectAvecFond(dV, 0, "@primaire");
			dV.nodes[(uint32)bt].label = NkString("Bouton");
			const int32 decl = dV.ExtraireComposant(bt, "", "bouton");
			const int32 a1 = dV.InstancierComposant(decl, 0);
			const int32 a2 = dV.InstancierComposant(decl, 0);
			bool q51 = false;
			if (decl >= 0 && dV.IsValidIndex(a1) && dV.IsValidIndex(a2)) {
				dV.nodes[(uint32)a1].MaterialiserFills();
				if (!dV.nodes[(uint32)a1].fills.Empty())
					dV.nodes[(uint32)a1].fills[0].couleur = NkString("#ff0000");
				dV.nodes[(uint32)a1].ecarts |= NkUINode::EcartRemplissages;
				dV.variables[0].valeur = NkString("#00aa00"); // la variable change
				const uint32 c1 = couleurPeinte(dV, a1), c2 = couleurPeinte(dV, a2);
				q51 = c1 == 0xFF0000FFu && c2 == 0x00AA00FFu;
				snprintf(det, sizeof(det), "variable -> #00aa00 : instance surchargee (litteral) -> %08X (tient), instance libre (@primaire) -> %08X (suit)", c1, c2);
			} else
				snprintf(det, sizeof(det), "extraction ou instanciation refusee");
			check("63d. LA PROPAGATION Q51, par la reference : changer la variable propage a tout ce qui la reference ; "
				  "une surcharge locale (un litteral pose sur une instance) TIENT -- meme mecanisme que les composants",
				  q51, det);
		}
		snprintf(tail, sizeof(tail), "\n=== RESULTAT : %d / %d ===\n", pass, total);
		rep.Append(tail);

		fputs(rep.Data(), stdout);
		fflush(stdout);
		// ⚠️ ET DANS UN FICHIER : une application `windowedapp` sur Windows n'a pas
		//    toujours de console attachee. Une sortie qu'on ne peut pas relire ne
		//    prouve rien — et « ca a tourne » n'est pas « ca tient ».
		NkFile::WriteAllText("nkuidesign_probe.txt", rep.Data());
		return (pass == total) ? 0 : 1;
	}

} // namespace nkuidesign
