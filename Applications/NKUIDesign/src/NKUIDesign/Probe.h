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
#include "NKTime/NkChrono.h"

#include "Selection.h"
#include "Canvas.h"
#include "Backend.h"
#include "DesignAI.h"
#include "Icons.h"
#include "Renderers.h"
#include "ExportSVG.h" // sondes 81-82 : l'export PNG (pixels) et SVG (re-rasterise)
#include "NKImage/Codecs/SVG/NkSVGCodec.h"

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
		// ── 50c. L'ARRONDI PAR COIN SURVIT AU FICHIER, MEME QUAND LES QUATRE
		//    COINS SONT EGAUX (inventaire du 07/09, mesure 1) ─────────────────
		//
		// 🔴 LE DEFAUT QUE CE CAS EXISTE POUR ATTRAPER, et il est exemplaire de sa
		//    famille : TROIS sites concordants, chacun juste isolement.
		//      1. deliee, la rangee de l'inspecteur ecrit `rayonsCoins` et JAMAIS
		//         `radius` (`Panels.h`, `ChampsUneOuQuatre`) ;
		//      2. `RayonsUniformes()` rend VRAI quand les quatre sont egaux ;
		//      3. le serialiseur n'ecrit `rayons` que si
		//         `rayonsDelies && !RayonsUniformes()`, sinon `rayon` si `radius`
		//         est non nul.
		//    Delier puis poser 12 aux quatre coins : les quatre sont uniformes,
		//    donc pas de `rayons` ; `radius` n'a pas bouge, donc pas de `rayon`.
		//    RIEN N'EST ECRIT, et l'arrondi vaut zero au rechargement.
		//
		// ⚠️ ET AUCUN ALLER-RETOUR NE PEUT LE VOIR, c'est ce qui le rend cher : le
		//    document AMPUTE se reenregistre a l'identique. C'est « stable ne veut
		//    pas dire juste », et c'est pourquoi ce cas exige LES DEUX moities --
		//    la STABILITE (le second enregistrement egale le premier) ET la
		//    CONSERVATION (les valeurs sont encore la). Sans la seconde, les
		//    quatre-vingt-dix-neuf aller-retours du banc restent verts sur une
		//    amputation reproductible.
		//
		// ⚠️ LE CAS 50 VOISIN NE POUVAIT PAS L'ATTRAPER : 50a et 50b mesurent la
		//    PEINTURE des coins, jamais leur persistance. Le trou etait exactement
		//    entre les deux cas existants.
		{
			char det[420];
			auto allerRetour = [](bool delies, float32 c0, float32 c1, float32 c2, float32 c3,
								  float32 rayonSimple, float32 out[4], bool &deliesRelus,
								  bool &stable) {
				NkUIDocument d;
				d.NewDocument("Toile", NkAuthor::Humain);
				const int32 f = d.AddChild(0, "", NkAuthor::Humain);
				NkUINode &nf = d.nodes[(uint32)f];
				nf.shape = NkString("rect");
				nf.rayonsDelies = delies;
				nf.rayonsCoins[0] = c0;
				nf.rayonsCoins[1] = c1;
				nf.rayonsCoins[2] = c2;
				nf.rayonsCoins[3] = c3;
				nf.radius = rayonSimple;
				NkString s1;
				d.Save(s1);
				NkUIDocument relu;
				const bool ok = relu.Load(s1.Data());
				NkString s2;
				relu.Save(s2);
				stable = ok && NkComponentDecl::StrEq(s1.Data(), s2.Data());
				if (!ok || !relu.IsValidIndex(f)) {
					deliesRelus = false;
					for (uint32 i = 0; i < 4u; ++i)
						out[i] = -1.f;
					return;
				}
				const NkUINode &r = relu.nodes[(uint32)f];
				deliesRelus = r.rayonsDelies;
				for (uint32 i = 0; i < 4u; ++i)
					out[i] = r.rayonsDelies ? r.rayonsCoins[i] : r.radius;
			};
			// (a) LE CAS QUI PERD : delie, quatre coins EGAUX a 12, `radius` reste
			//     a 0 -- exactement ce que l'inspecteur laisse apres un deliage
			//     depuis un rayon nul.
			float32 g[4];
			bool gd = false, gs = false;
			allerRetour(true, 12.f, 12.f, 12.f, 12.f, 0.f, g, gd, gs);
			const bool conserveUniforme = g[0] == 12.f && g[1] == 12.f && g[2] == 12.f
										  && g[3] == 12.f;
			// (b) LE CAS QUI PASSAIT DEJA : quatre coins DIFFERENTS. Sans lui, un
			//     correctif qui casserait le chemin nominal passerait inapercu.
			float32 h[4];
			bool hd = false, hs = false;
			allerRetour(true, 4.f, 8.f, 12.f, 16.f, 0.f, h, hd, hs);
			const bool conserveVarie = hd && h[0] == 4.f && h[1] == 8.f && h[2] == 12.f
									   && h[3] == 16.f;
			// (c) LE CHEMIN SIMPLE, non delie : il ne doit rien perdre non plus, et
			//     il ne doit PAS se mettre a ecrire quatre nombres.
			float32 k[4];
			bool kd = true, ks = false;
			allerRetour(false, 0.f, 0.f, 0.f, 0.f, 8.f, k, kd, ks);
			const bool simpleIntact = !kd && k[0] == 8.f;
			snprintf(det, sizeof(det),
					 "(a) delie + quatre coins EGAUX a 12 -> relus %.0f %.0f %.0f %.0f, "
					 "delie=%d, aller-retour stable=%d ; (b) quatre coins DIFFERENTS "
					 "-> %.0f %.0f %.0f %.0f (delie=%d) ; (c) non delie, rayon 8 -> %.0f "
					 "(delie=%d)",
					 (double)g[0], (double)g[1], (double)g[2], (double)g[3], gd ? 1 : 0,
					 gs ? 1 : 0, (double)h[0], (double)h[1], (double)h[2], (double)h[3],
					 hd ? 1 : 0, (double)k[0], kd ? 1 : 0);
			check("50c. L'ARRONDI PAR COIN SURVIT AU FICHIER MEME QUAND LES QUATRE COINS SONT EGAUX : "
				  "delier puis poser 12 partout doit rendre 12 apres enregistrement et rechargement -- "
				  "l'aller-retour est STABLE dans les deux cas, et c'est justement pourquoi il ne "
				  "pouvait pas voir l'amputation ; le cas exige donc la CONSERVATION en plus de la "
				  "stabilite. Les quatre coins DIFFERENTS et le rayon simple restent intacts",
				  conserveUniforme && gd && gs && conserveVarie && hs && simpleIntact && ks, det);
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
				// ④ le selecteur suit la selection : la sonde selectionne un noeud, comme l'application
				stP.doc.NewDocument("Toile", NkAuthor::Humain);
				stP.SelectSingle(stP.doc.AddChild(0, "", NkAuthor::Humain));
				stP.picker = DesignState::DemandePicker();
				char hex[12] = "#1976d2";
				const nkgui::NkRect sw = {240.f, 60.f, 16.f, 16.f};
				auto image = [&](float32 mx, float32 my, bool bas, bool overlay) {
					ctxP.input.mousePos = {mx, my};
					ctxP.input.mouseDown[0] = bas;
					ctxP.BeginFrame(0.016f);
					ctxP.BeginLayout({0.f, 0.f, 300.f, 600.f});
					// ② (07/09) LA PASTILLE DECLARE LE NŒUD QU'ELLE DECRIT ; celle-ci
					//    en decrit un -- c'est le montage de cet essai depuis le 04/09.
					NkPastilleCouleur(ctxP, stP, "##sonde.pastille", sw, hex,
									  (uint32)sizeof(hex), stP.selected);
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
				// LE PANNEAU SANS POPOVER, MESURE MAINTENANT : le temoin compare deux mesures de LA
				// MEME course, jamais un nombre fige. Le « 3777 » d'avant rougissait des qu'une
				// section s'ajoutait -- « ALIGNER LA SELECTION » l'a fait le 05/09.
				uint32 npSansPopover = 0u;
				{
					uint32 opJetable = 0u;
					float32 xJetable = 0.f;
					// COMME LA MAIN : on ferme le popup DU KIT avant d'effacer la demande, sinon le
					// panneau continue de se croire sous un popup et cache ses rangees -- les deux
					// mesures seraient alors identiques, et le temoin ne discriminerait rien
					// (mesure : 4366 contre 4366 a la premiere course).
					if (ctxI.popupDepth > 0)
						ctxI.ClosePopup();
					stI.picker = DesignState::DemandePicker();
					image(260.f, true, xJetable, npSansPopover, opJetable);
					image(260.f, true, xJetable, npSansPopover, opJetable);
				}
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
				snprintf(det, sizeof(det),
						 "popover de bordure : %u sommets overlay, x %.0f..%.0f (ecran 0..600), ouvert=%d ; panneau a 260 px : %u "
						 "sommets pendant le popover contre %u sans lui, MEME course -- le panneau garde ses rangees",
						 op, oxMin, oxMax, stI.picker.ouvert ? 1 : 0, np, npSansPopover);
				// 🔴 CE QUE CE CAS AFFIRMAIT ETAIT FAUX, ET IL PASSAIT QUAND MEME (mesure du 05/09).
				//    Il disait « et le panneau a perdu ses trois rangees » et le prouvait par
				//    `np < 3777` -- un nombre FIGE, releve dans une version anterieure du panneau.
				//    Mesure de la meme course, popover ouvert puis ferme : 4366 contre 4366. Le
				//    panneau ne perd rien du tout ; le popover AJOUTE ses rangees dans l'overlay,
				//    il ne les DEPLACE pas. *Un temoin qui compare a un nombre d'hier finit par
				//    prouver l'inverse de ce qu'il affirme, sans jamais rougir.*
				//    Ce qui est mesure ici, et qui est vrai : le popover se dessine dans l'overlay,
				//    a GAUCHE de sa pastille, entierement dans l'ecran ; et le panneau, lui, ne
				//    bouge pas.
				check("60c. LE POPOVER D'UNE BORDURE (selecteur, hexa, epaisseur, position, cotes, jointure, "
					  "extremites) dessine dans l'overlay, a gauche de sa pastille, entierement dans l'ecran ; le panneau garde ses "
					  "rangees (meme nombre de sommets avec et sans le popover, MEME course)",
					  stI.picker.ouvert && op > 300u && oxMin >= 0.f && oxMax <= 360.f && npSansPopover > 0u
						  && np == npSansPopover,
					  det);
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
					// la rangee Cadrage : rotation (26), puis, depuis le 05/09, la source (26) et
					// les boutons (26) sous elle, marge 8 ; le bouton commence 62 px apres la
					// marge gauche
					const float32 bx = gauche + 8.f + 62.f + 20.f, by = bas - 47.f - 52.f;
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
				// ── 137. LA PASTILLE DU CANVAS OUVRE LA MEME FENETRE QU'UN REMPLISSAGE.
				//    C'est l'assertion qui MANQUAIT a l'essai 136, et c'est elle qui nous a
				//    fait croire le lot fini : 136 comptait le NOYAU (meme fonction, meme
				//    placement), pas l'ENVELOPPE. Rodolf a mis les deux captures cote a cote
				//    -- le remplissage ouvrait une FENETRE (fond opaque, onglets, hexa,
				//    opacite, croix), le canvas ouvrait le cœur du selecteur POSE NU.
				//
				// ⚠️ TROIS MESURES DE LA MEME COURSE, jamais un nombre fige : le noyau nu,
				//    la fenetre du canvas, celle d'un remplissage. Ce qui est prouve est une
				//    RELATION entre elles.
				{
					float32 hauteurVue = 0.f; // la HAUTEUR de la derniere fenetre ouverte
					auto ouvrirEtCompter = [&](const char *idp, uint8 genre, int32 noeud,
											   int32 index) -> uint32 {
						if (ctxI.popupDepth > 0)
							ctxI.ClosePopup();
						stI.picker = DesignState::DemandePicker();
						stI.picker.ouvert = true;
						stI.picker.id = ctxI.GetId(idp);
						stI.picker.genre = genre;
						stI.picker.noeud = noeud;
						stI.picker.index = index;
						stI.picker.ancre = {360.f, 200.f, 16.f, 16.f};
						float32 xz = 0.f;
						uint32 nz = 0u, oz = 0u;
						image(260.f, true, xz, nz, oz); // deux images : le popup s'installe
						image(260.f, true, xz, nz, oz);
						// ⚠️ LA HAUTEUR VIENT DU RECTANGLE DU POPUP, pas de l'etendue des
						//    sommets de l'overlay : celle-ci melait d'autres dessins et
						//    rendait 11 px la ou la rangee masquee en coute 26. *Mesurer a
						//    cote de la chose donne un nombre qui varie pour d'autres
						//    raisons qu'elle.*
						hauteurVue = ctxI.popupRects[0].h;
						return oz;
					};
					stI.canvasFill = NkRemplissage();
					stI.canvasFill.couleur = NkString("#0d1117");
					const uint32 oNoyau = ouvrirEtCompter("##s.137.noyau", 0u, -1, -1);
					const uint32 oCanvas = ouvrirEtCompter("##s.137.canvas", 1u, -1, -1);
					const float32 hCanvas = hauteurVue;
					const uint32 oFill = ouvrirEtCompter("##s.137.fill", 1u, rc, 0);
					const float32 hFill = hauteurVue;
					// (a) LE CANVAS N'OUVRE PLUS LE NOYAU NU : sa fenetre porte tout ce que
					//     le noyau n'a pas -- fond, rangee de type, hexa, opacite, croix.
					const bool plusLeNoyau = oCanvas > oNoyau + 200u;
					// (b) ET C'EST LA MEME FENETRE QU'UN REMPLISSAGE : du meme ordre, et plus
					//     COURTE de ce que la regle du type retire (cinq vignettes, la rangee
					//     variable). Ni identique -- ce serait le masquage absent -- ni du
					//     simple au double.
					// ⚠️ DEUX BORNES, ET C'EST UNE MESURE QUI ME LES A IMPOSEES. Ma premiere
					//    ecriture disait seulement << plus petite, mais pas du simple au
					//    double >> : les mutations << plus de masquage par type >> (855 ->
					//    1175) et << la rangee variable revient >> (855 -> 916) restaient
					//    VERTES sous cette borne-la. *Un intervalle assez large pour
					//    accueillir le defaut ne prouve rien.* On borne donc chaque
					//    masquage par ce qu'il retire vraiment :
					//  - les cinq vignettes : quelques centaines de sommets ;
					//  - la rangee variable : 26 px de HAUTEUR, exactement.
					const bool memeFenetre = oCanvas + 200u < oFill && oCanvas * 2u > oFill;
					// ⚠️ CE QUE LA HAUTEUR MESURE, ET CE QU'ELLE NE MESURE PAS : elle
					//    constate que la REGLE est appliquee (la fenetre est plus courte de
					//    la rangee masquee). Elle ne distinguerait pas une regle a MOITIE
					//    retiree -- une rangee dessinee mais hors de la hauteur calculee ;
					//    celle-la se voit au compte de sommets ci-dessus. Deux masquages,
					//    deux mesures : les vignettes par les sommets, la rangee par la
					//    hauteur du popup (`popupRects[0]`, exacte).
					const float32 dH = hFill - hCanvas;
					const bool variableRetiree = dH > 24.f && dH < 28.f;
					// (c) ET LA PASTILLE DEMANDE-T-ELLE CETTE FENETRE ? Les deux mesures
					//     ci-dessus POSENT le genre elles-memes : elles prouvent le
					//     dessinateur, pas le CHEMIN D'OUVERTURE. Sans cette lecture, ramener
					//     la pastille du canvas au noyau nu ne rougirait nulle part -- c'est
					//     exactement le trou par lequel le lot d'hier soir est passe.
					bool pastilleDemandeLEnveloppe = false;
					{
						const NkString src =
							NkFile::ReadAllText("Applications/NKUIDesign/src/NKUIDesign/Panels.h");
						if (!src.Empty()) {
							const char *cle = "id == ctx.GetId(\"##insp.canvas.pastille\")";
							const char *d = src.Data();
							for (; *d; ++d) {
								const char *x = d, *y = cle;
								while (*x && *y && *x == *y) {
									++x;
									++y;
								}
								if (!*y)
									break;
							}
							if (*d) {
								for (uint32 k = 0; k < 400u && d[k]; ++k) {
									const char *x = d + k, *y = "picker.genre = 1u;";
									while (*x && *y && *x == *y) {
										++x;
										++y;
									}
									if (!*y) {
										pastilleDemandeLEnveloppe = true;
										break;
									}
								}
							}
						}
					}
					char det137[420];
					snprintf(det137, sizeof(det137),
							 "MEME course, trois ouvertures : noyau nu %u sommets, canvas %u, "
							 "remplissage %u ; le canvas n'est plus le noyau (%u > %u+200) -> %d ; "
							 "meme fenetre que le remplissage, en plus court du masquage par type "
							 "(%u+200 < %u et 2x%u > %u) -> %d ; la rangee variable retiree : "
							 "%.0f px de moins en hauteur -> %d ; (c) la PASTILLE demande cette "
							 "fenetre-la (source lue) -> %d",
							 oNoyau, oCanvas, oFill, oCanvas, oNoyau, plusLeNoyau ? 1 : 0, oCanvas,
							 oFill, oCanvas, oFill, memeFenetre ? 1 : 0, (double)dH,
							 variableRetiree ? 1 : 0, pastilleDemandeLEnveloppe ? 1 : 0);
					check("137. LA PASTILLE DU CANVAS OUVRE LA MEME FENETRE QU'UN REMPLISSAGE, pas le cœur du "
						  "selecteur pose nu : fond opaque, rangee de type, hexa, opacite, croix. L'enveloppe "
						  "n'exigeait pas un NŒUD (huit sites sur 1046 lignes) mais UN REMPLISSAGE (160) : elle "
						  "RECOIT desormais ce qu'elle edite. Et elle est plus courte que celle d'un remplissage, "
						  "de ce que la regle du type retire -- degrade, radial, image et variable ne peuvent pas "
						  "exister pour un decor de machine",
						  plusLeNoyau && memeFenetre && variableRetiree && pastilleDemandeLEnveloppe,
						  det137);
				}
				// ── 138. LES DEUX PASTILLES SE FERMENT-ELLES PAR LES MEMES PORTES ?
				//    Rodolf : << quand on ouvre le color picker du canvas, si on clique dans le
				//    vide ca ne se ferme pas >>. Celui d'un remplissage se ferme.
				//
				// ⚠️ ON NE CHERCHE PAS DANS CELUI QUI EST CASSE : on met les deux chemins
				//    COTE A COTE et on compte. Trois sorties x deux pastilles = six mesures de
				//    la meme course ; le tableau dira si c'est un mecanisme MANQUANT ou une
				//    condition MAL PLACEE, et ca vaut mieux qu'un correctif qui ferme le cas
				//    qu'on vient de voir.
				{
					// Une image complete, entree BRUTE posee avant `BeginFrame` -- c'est la
					// que NKGui calcule ses fronts (clic, touche) et applique sa regle
					// << un clic hors de tous les popups ferme la chaine >>.
					auto image138 = [&](float32 mx, float32 my, bool bas, bool echap) {
						ctxI.input.mousePos = {mx, my};
						ctxI.input.mouseDown[0] = bas;
						ctxI.input.SetKey(nkgui::NkGuiKey::Escape, echap);
						ctxI.BeginFrame(0.016f);
						ctxI.BeginLayout({340.f, 0.f, 260.f, 900.f});
						insp.OnUI(ec);
						NkDessinerPickerDemande(ctxI, stI);
						ctxI.EndFrame();
					};
					// `sortie` : 0 = clic dans le vide, 1 = Echap, 2 = la croix.
					auto eprouver = [&](int32 noeud, int32 index, int32 sortie) -> bool {
						if (ctxI.popupDepth > 0)
							ctxI.ClosePopup();
						stI.picker = DesignState::DemandePicker();
						stI.picker.ouvert = true;
						stI.picker.id = ctxI.GetId("##s.138.pastille");
						stI.picker.genre = 1u;
						stI.picker.noeud = noeud;
						stI.picker.index = index;
						stI.picker.ancre = {360.f, 200.f, 16.f, 16.f};
						image138(360.f, 200.f, false, false); // la fenetre s'installe
						image138(360.f, 200.f, false, false);
						const nkgui::NkRect boite = ctxI.popupRects[0];
						if (sortie == 0) {
							// LE CLIC DANS LE VIDE : loin de la fenetre ET de son ancre.
							// ⚠️ Le point est choisi HORS des deux, sinon on mesurerait
							//    << un clic dedans ne ferme pas >>, ce qui est vrai et sans
							//    interet. La toile est a gauche du panneau.
							image138(60.f, 600.f, true, false);
							image138(60.f, 600.f, false, false);
						} else if (sortie == 1) {
							image138(360.f, 200.f, false, true);
							image138(360.f, 200.f, false, false);
						} else {
							// LA CROIX : son rectangle est en haut a droite de la fenetre
							// (16 px de cote, 3 px de marge) -- pose depuis la boite mesuree.
							const float32 cx = boite.x + boite.w - 8.f - 8.f;
							const float32 cy = boite.y + 8.f + 11.f;
							image138(cx, cy, true, false);
							image138(cx, cy, false, false);
						}
						return !stI.picker.ouvert; // ferme ?
					};
					const bool fClic = eprouver(rc, 0, 0), fEchap = eprouver(rc, 0, 1),
							   fCroix = eprouver(rc, 0, 2);
					const bool cClic = eprouver(-1, -1, 0), cEchap = eprouver(-1, -1, 1),
							   cCroix = eprouver(-1, -1, 2);
					// LA RELATION : les deux pastilles se ferment par LES MEMES PORTES. Ce
					// n'est pas << le selecteur se ferme >> -- c'est l'egalite des deux
					// lignes du tableau, et c'est elle qui survit a l'ajout d'une sortie.
					const bool memesPortes = (fClic == cClic) && (fEchap == cEchap)
											 && (fCroix == cCroix);
					// ... et elles doivent etre VRAIES : trois portes qui ne ferment ni l'une
					// ni l'autre seraient << les memes portes >> et un defaut plus grave.
					const bool troisPortes = fClic && fEchap && fCroix;
					char det138[420];
					snprintf(det138, sizeof(det138),
							 "remplissage : clic dans le vide=%d, Echap=%d, croix=%d ; canvas : "
							 "clic dans le vide=%d, Echap=%d, croix=%d ; memes portes -> %d ; "
							 "les trois ferment -> %d",
							 fClic ? 1 : 0, fEchap ? 1 : 0, fCroix ? 1 : 0, cClic ? 1 : 0,
							 cEchap ? 1 : 0, cCroix ? 1 : 0, memesPortes ? 1 : 0,
							 troisPortes ? 1 : 0);
					check("138. LES DEUX PASTILLES SE FERMENT PAR LES MEMES PORTES : clic dans le vide, Echap, "
						  "croix -- pour un remplissage COMME pour le decor de la toile. L'assertion n'est pas "
						  "<< le selecteur se ferme >> mais l'EGALITE des deux lignes du tableau : c'est elle "
						  "qui rougit quand une porte manque a l'un des deux, et elle survit a l'ajout d'une "
						  "quatrieme sortie",
						  memesPortes && troisPortes, det138);
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
				// 60k. UN GESTE POUR LES QUATRE GENRES (Rodolf, 04/09 soir : « je peux changer
				// l'orientation des lineaires mais plus deplacer les pastilles ; dans les
				// circulaires c'est l'inverse -- corrige ca sur tous les degrades ») : une BOUCLE
				// sur les genres, popover ouvert. Disque de l'arret du milieu + glisser -> sa
				// position change, l'angle non ; anneau de l'extremite + glisser -> l'angle
				// change, les positions non ; l'aimant colle a 0 pres de 0 ; le curseur annonce
				// le geste avant le clic ; le noeud ne bouge jamais.
				{
					static PreviewPanel toile(&stI);
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
					auto tirer = [&](float32 x0, float32 y0, float32 x1, float32 y1) {
						scene(x0, y0, false);
						scene(x0, y0, true);
						scene((x0 + x1) * 0.5f, (y0 + y1) * 0.5f, true);
						scene(x1, y1, true);
						scene(x1, y1, false);
						scene(-1.f, -1.f, false);
					};
					static const char *const kGenres[4] = {"lineaire", "radial", "angulaire", "losange"};
					NkUINode &nd = stI.doc.nodes[(uint32)rc];
					NkDegrade &gd = nd.fills[1].degrade;
					const float32 posX0 = nd.posX, posY0 = nd.posY;
					char resume[900] = "";
					bool tout = true;
					for (uint32 gi = 0; gi < 4u; ++gi) {
						gd.type = NkString(kGenres[gi]);
						gd.angle = 0.f;
						gd.origineX = gd.origineY = 0.5f;
						gd.rayonX = gd.rayonY = 0.5f;
						gd.arrets[0].position = 0.f;
						gd.arrets[1].position = 0.5f;
						gd.arrets[2].position = 1.f;
						stI.SelectSingle(rc);
						stI.picker = DesignState::DemandePicker();
						stI.picker.ouvert = true;
						stI.picker.id = ctxI.GetId("##sonde.popover.geste");
						stI.picker.genre = 1u;
						stI.picker.noeud = rc;
						stI.picker.index = 1;
						stI.picker.ancre = {590.f, 20.f, 16.f, 16.f};
						scene(-1.f, -1.f, false);
						scene(-1.f, -1.f, false);
						NkLayoutResult scr;
						stI.ProjectToScreen(scr);
						const NkPaintRect rs = scr.At(rc);
						const int32 genre = renderdetail::NkGenreDegrade(gd);
						// a. le DISQUE de l'arret du milieu, glisse a t = 0,8 -- le long de l'axe, ou du
						//    cercle pour l'angulaire (⑤) : le « ou » par genre, le meme geste
						float32 mx0 = 0.f, my0 = 0.f, mx1 = 0.f, my1 = 0.f, mxm = 0.f, mym = 0.f;
						renderdetail::NkPoigneeDegradeGenre(genre, rs, gd, 0.5f, mx0, my0);
						renderdetail::NkPoigneeDegradeGenre(genre, rs, gd, 0.65f, mxm, mym);
						renderdetail::NkPoigneeDegradeGenre(genre, rs, gd, 0.8f, mx1, my1);
						scene(mx0, my0, false);
						const int32 curseurDisque = stI.curseurDegrade;
						scene(mx0, my0, false);
						scene(mx0, my0, true);
						scene(mxm, mym, true);
						scene(mx1, my1, true);
						scene(mx1, my1, false);
						scene(-1.f, -1.f, false);
						const float32 posMilieu = gd.arrets[1].position, angleApresGlisse = gd.angle;
						// b. l'ANNEAU de l'extremite (t = 1) : a 13 px de la pastille, perpendiculaire a
						//    l'axe ; tire vers la GAUCHE du pivot -> l'axe pointe a gauche : angle 90
						float32 ex = 0.f, ey = 0.f, pvx = 0.f, pvy = 0.f;
						renderdetail::NkPoigneeDegradeGenre(genre, rs, gd, 1.f, ex, ey);
						renderdetail::NkPivotDegrade(genre, rs, gd, pvx, pvy);
						float32 dx = ex - pvx, dy = ey - pvy; // la direction pivot -> extremite (l'axe)
						const float32 l = nkentseu::math::NkSqrt(dx * dx + dy * dy);
						dx /= l > 0.001f ? l : 1.f;
						dy /= l > 0.001f ? l : 1.f;
						// dans l'anneau, hors du disque, AU-DELA de l'extremite (le long de l'axe) : loin des
						// poignees de forme -- a petit zoom, un point perpendiculaire tomberait sur un coin,
						// et la zone la plus proche du pointeur gagnerait, comme il se doit
						const float32 ax0 = ex + dx * 13.f, ay0 = ey + dy * 13.f;
						scene(ax0, ay0, false);
						const int32 curseurAnneau = stI.curseurDegrade;
						const float32 pos0 = gd.arrets[0].position, pos1 = gd.arrets[1].position, pos2 = gd.arrets[2].position;
						tirer(ax0, ay0, pvx - 60.f, pvy); // a gauche du pivot : 90 degres (loin de tout aimant)
						const float32 angleApresTour = gd.angle;
						const bool positionsIntactes = gd.arrets[0].position == pos0 && gd.arrets[1].position == pos1 && gd.arrets[2].position == pos2;
						// c. L'AIMANT : depuis l'anneau, tirer vers 3 degres -> colle a 0
						renderdetail::NkPoigneeDegradeGenre(genre, rs, gd, 1.f, ex, ey);
						dx = ex - pvx;
						dy = ey - pvy;
						const float32 l2 = nkentseu::math::NkSqrt(dx * dx + dy * dy);
						dx /= l2 > 0.001f ? l2 : 1.f;
						dy /= l2 > 0.001f ? l2 : 1.f;
						float32 s3 = 0.f, c3 = 1.f;
						NkSinCosDeg(3.f, s3, c3); // la direction de l'angle 3 : (-sin 3, cos 3)
						tirer(ex + dx * 13.f, ey + dy * 13.f, pvx - s3 * 60.f, pvy + c3 * 60.f);
						const float32 angleAimante = gd.angle;
						const bool ok = posMilieu > 0.7f && posMilieu < 0.9f && angleApresGlisse == 0.f && curseurDisque == 1
										&& curseurAnneau == 2 && angleApresTour > 80.f && angleApresTour < 100.f && positionsIntactes
										&& angleAimante == 0.f && nd.posX == posX0 && nd.posY == posY0;
						if (!ok)
							tout = false;
						const size_t rl = strlen(resume);
						snprintf(resume + rl, sizeof(resume) - rl, "%s%s : glisse 0,50->%.2f (angle %.0f, curseur %d) ; anneau (curseur %d) -> angle %.0f (arrets intacts=%d) ; aimant 3 -> %.0f",
								 gi ? " | " : "", kGenres[gi], (double)posMilieu, (double)angleApresGlisse, curseurDisque, curseurAnneau,
								 (double)angleApresTour, positionsIntactes ? 1 : 0, (double)angleAimante);
					}
					gd.type = NkString("lineaire");
					gd.angle = 0.f;
					gd.arrets[1].position = 0.5f;
					stI.picker = DesignState::DemandePicker();
					scene(-1.f, -1.f, false);
					snprintf(det, sizeof(det), "%s ; noeud fixe=%d", resume, (nd.posX == posX0 && nd.posY == posY0) ? 1 : 0);
					check("60k. UN GESTE POUR LES QUATRE GENRES (boucle) : le disque d'un arret GLISSE le long de l'axe (l'angle ne "
						  "bouge pas), l'anneau d'une extremite TOURNE l'axe (les arrets ne bougent pas), l'aimant colle a 0, le "
						  "curseur annonce le geste avant le clic, le noeud ne bouge jamais",
						  tout && nd.posX == posX0 && nd.posY == posY0, det);
				}
				// 60l. ③ LE MENU DE LA GOUTTE : peint APRES le selecteur (il etait recouvert : « visible
				// en partie ») et reclame AVANT lui (un clic sur « Normal », qui tombe au-dessus du
				// carre SV, ferme le menu et ne change PAS la couleur).
				{
					stI.SelectSingle(rc);
					stI.picker = DesignState::DemandePicker();
					stI.picker.ouvert = true;
					stI.picker.id = ctxI.GetId("##sonde.popover.goutte");
					stI.picker.genre = 1u;
					stI.picker.noeud = rc;
					stI.picker.index = 0;
					stI.picker.ancre = {360.f, 200.f, 16.f, 16.f};
					auto image3 = [&](float32 mx, float32 my, bool bas) {
						ctxI.input.mousePos = {mx, my};
						ctxI.input.mouseDown[0] = bas;
						ctxI.BeginFrame(0.016f);
						ctxI.BeginLayout({340.f, 0.f, 260.f, 900.f});
						insp.OnUI(ec);
						NkDessinerPickerDemande(ctxI, stI);
						ctxI.EndFrame();
					};
					image3(-1.f, -1.f, false);
					image3(-1.f, -1.f, false);
					float32 px = 1e9f, py = 1e9f;
					for (uint32 i = 0; i < (uint32)ctxI.dlOverlay.vtx.Size(); ++i) {
						if (ctxI.dlOverlay.vtx[i].pos.x < px) px = ctxI.dlOverlay.vtx[i].pos.x;
						if (ctxI.dlOverlay.vtx[i].pos.y < py) py = ctxI.dlOverlay.vtx[i].pos.y;
					}
					const float32 x0 = px + 0.5f + 8.f, y0 = py + 0.5f + 8.f;
					const NkString avant = stI.doc.nodes[(uint32)rc].fills[0].couleur;
					// la goutte : x0 + 156 .. +174, y0 + 2 .. +20
					const float32 gx = x0 + 165.f, gy = y0 + 11.f;
					image3(gx, gy, false);
					image3(gx, gy, true);
					image3(gx, gy, false);
					image3(-1.f, -1.f, false);
					// ou sont les coins blancs du carre SV, et ou sont les sommets du menu (le fond
					// du menu est un rectangle de theme.panel de 9*18+4 px de haut a y0 + 26)
					const nkentseu::uint32 panel = nkgui::NkGuiPackColor(ctxI.theme.panel);
					uint32 iBlanc = 0u, iMenu = 0u, nMenu = 0u;
					const float32 yMenu = y0 + 26.f;
					for (uint32 i = 0; i < (uint32)ctxI.dlOverlay.vtx.Size(); ++i) {
						const auto &vt = ctxI.dlOverlay.vtx[i];
						if (vt.col == 0xFFFFFFFFu && vt.pos.y > y0 + 20.f && vt.pos.y < y0 + 200.f && iBlanc == 0u)
							iBlanc = i;
						// le fond du menu est arrondi (4 px) : ses premiers sommets sont pres du coin, pas dessus
						if (vt.col == panel && vt.pos.y >= yMenu - 0.5f && vt.pos.y <= yMenu + 6.f && vt.pos.x >= x0 - 0.5f && vt.pos.x <= x0 + 6.f) {
							if (nMenu == 0u)
								iMenu = i;
							++nMenu;
						}
					}
					const bool peintApres = nMenu > 0u && iBlanc > 0u && iMenu > iBlanc;
					// un clic sur « Normal » (colonne 0, ligne 0) : au-dessus du carre SV
					const float32 nx = x0 + 20.f, ny = yMenu + 2.f + 9.f;
					image3(nx, ny, false);
					image3(nx, ny, true);
					image3(nx, ny, false);
					image3(-1.f, -1.f, false);
					const NkString apres = stI.doc.nodes[(uint32)rc].fills[0].couleur;
					uint32 nMenuApres = 0u;
					for (uint32 i = 0; i < (uint32)ctxI.dlOverlay.vtx.Size(); ++i) {
						const auto &vt = ctxI.dlOverlay.vtx[i];
						if (vt.col == panel && vt.pos.y >= yMenu - 0.5f && vt.pos.y <= yMenu + 6.f && vt.pos.x >= x0 - 0.5f && vt.pos.x <= x0 + 6.f)
							++nMenuApres;
					}
					const bool ferme = nMenuApres == 0u;
					const bool couleurIntacte = NkComponentDecl::StrEq(avant.Data(), apres.Data());
					stI.picker = DesignState::DemandePicker();
					image3(-1.f, -1.f, false);
					snprintf(det, sizeof(det),
							 "menu ouvert : sommet du menu a l'indice %u, coin blanc du carre a %u (menu apres=%d) ; clic sur « Normal » "
							 "au-dessus du carre : menu ferme=%d, couleur %s -> %s (intacte=%d)",
							 iMenu, iBlanc, peintApres ? 1 : 0, ferme ? 1 : 0, avant.Data(), apres.Data(), couleurIntacte ? 1 : 0);
					check("60l. ③ LE MENU DE LA GOUTTE se peint APRES le selecteur (au-dessus) et reclame AVANT lui : un clic "
						  "sur « Normal » par-dessus le carre SV ferme le menu sans changer la couleur",
						  peintApres && ferme && couleurIntacte, det);
				}
				// 60m. ④ LES PASTILLES DE LA BARRE NE SONT PLUS RECOUVERTES : l'etendue en y des
				// pastilles (sommets a la couleur d'accent sous la barre) s'arrete AVANT le haut
				// de la boite du champ Angle (la rangee suivante) -- deux rangees ne se recouvrent
				// jamais, mesure sur la couche overlay.
				{
					stI.SelectSingle(rc);
					stI.picker = DesignState::DemandePicker();
					stI.picker.ouvert = true;
					stI.picker.id = ctxI.GetId("##sonde.popover.barre");
					stI.picker.genre = 1u;
					stI.picker.noeud = rc;
					stI.picker.index = 1; // le degrade a trois arrets
					stI.picker.arretSel = 0;
					stI.picker.ancre = {360.f, 200.f, 16.f, 16.f};
					auto image4 = [&]() {
						ctxI.input.mousePos = {-1.f, -1.f};
						ctxI.input.mouseDown[0] = false;
						ctxI.BeginFrame(0.016f);
						ctxI.BeginLayout({340.f, 0.f, 260.f, 900.f});
						insp.OnUI(ec);
						NkDessinerPickerDemande(ctxI, stI);
						ctxI.EndFrame();
					};
					image4();
					image4();
					float32 px = 1e9f, py = 1e9f;
					for (uint32 i = 0; i < (uint32)ctxI.dlOverlay.vtx.Size(); ++i) {
						if (ctxI.dlOverlay.vtx[i].pos.x < px) px = ctxI.dlOverlay.vtx[i].pos.x;
						if (ctxI.dlOverlay.vtx[i].pos.y < py) py = ctxI.dlOverlay.vtx[i].pos.y;
					}
					const float32 x0 = px + 0.5f + 8.f;
					// apres types, selecteur, rangee modele, ET la rangee de la variable (§15.14, 05/09)
					// + 26 : la RANGEE D'OPACITE (③ du 05/09, apres-midi) s'intercale avant la rampe
					const float32 yBarre = py + 0.5f + 8.f + 26.f + 168.f + 26.f + 26.f + 26.f;
					const nkentseu::uint32 accent = nkgui::NkGuiPackColor(ctxI.theme.accent);
					const nkentseu::uint32 encre = nkgui::NkGuiPackColor(ctxI.theme.text); // l'anneau de la pastille courante (rayon 6)
					const nkentseu::uint32 boite = nkgui::NkGuiPackColor(
						nkentseu::editorkit::NkThemeUnpack(stI.theme.Get(nkentseu::editorkit::NkRole::InputBg)));
					float32 basPastilles = -1e9f, hautAngle = 1e9f;
					uint32 nA = 0u, nB = 0u;
					for (uint32 i = 0; i < (uint32)ctxI.dlOverlay.vtx.Size(); ++i) {
						const auto &vt = ctxI.dlOverlay.vtx[i];
						// les pastilles : accent, sous la barre (y entre yBarre+14 et yBarre+40), a gauche
						if ((vt.col == accent || vt.col == encre) && vt.pos.y > yBarre + 14.f && vt.pos.y < yBarre + 40.f && vt.pos.x < x0 + 120.f) {
							if (vt.pos.y > basPastilles) basPastilles = vt.pos.y;
							++nA;
						}
						// la boite du champ Angle : couleur de champ, x entre x0+62 et x0+110, sous la barre
						if (vt.col == boite && vt.pos.x >= x0 + 61.f && vt.pos.x <= x0 + 111.f && vt.pos.y > yBarre + 14.f && vt.pos.y < yBarre + 60.f) {
							if (vt.pos.y < hautAngle) hautAngle = vt.pos.y;
							++nB;
						}
					}
					const bool disjoints = nA > 0u && nB > 0u && basPastilles <= hautAngle + 0.01f;
					stI.picker = DesignState::DemandePicker();
					image4();
					snprintf(det, sizeof(det), "barre a y=%.0f : bas des pastilles %.1f (%u sommets), haut de la boite Angle %.1f (%u sommets)",
							 yBarre, basPastilles, nA, hautAngle, nB);
					check("60m. ④ LES PASTILLES DE LA BARRE ET LA RANGEE SUIVANTE NE SE RECOUVRENT PAS : le bas des pastilles "
						  "est au-dessus du haut de la boite du champ Angle, mesure sur la couche overlay",
						  disjoints, det);
				}
				// 60n. ⑤ L'ASCENSEUR : une fenetre de 420 px, douze arrets -- le popover tient dans
				// la fenetre (tous ses sommets dedans), la liste DEFILE a la molette (les boites
				// des rangees se decalent de 13 px pour une demi-crans), et rien ne sort.
				{
					static nkgui::NkGuiContext ctxS;
					if (!ctxS.Init(600, 420)) {
						check("60n. ⑤ l'ascenseur : contexte 600x420", false, "Init a refuse");
					} else {
						NkDegrade &g12 = stI.doc.nodes[(uint32)rc].fills[1].degrade;
						while (g12.arrets.Size() < 12u) {
							const float32 tA = 0.05f + 0.9f * (float32)g12.arrets.Size() / 12.f;
							if (renderdetail::NkAjouterArretDegrade(g12, tA, 12u) < 0)
								break;
						}
						stI.SelectSingle(rc);
						stI.picker = DesignState::DemandePicker();
						stI.picker.ouvert = true;
						stI.picker.id = ctxS.GetId("##sonde.popover.ascenseur");
						stI.picker.genre = 1u;
						stI.picker.noeud = rc;
						stI.picker.index = 1;
						stI.picker.arretSel = 0;
						stI.picker.ancre = {360.f, 100.f, 16.f, 16.f};
						NkEditorFrameContext ecS;
						ecS.ui = &ctxS;
						ecS.dt = 0.016f;
						auto image5 = [&](float32 mx, float32 my, float32 molette) {
							ctxS.input.mousePos = {mx, my};
							ctxS.input.mouseDown[0] = false;
							ctxS.BeginFrame(0.016f);
							ctxS.input.wheel = molette;
							ctxS.BeginLayout({340.f, 0.f, 260.f, 420.f});
							insp.OnUI(ecS);
							NkDessinerPickerDemande(ctxS, stI);
							ctxS.EndFrame();
						};
						image5(-1.f, -1.f, 0.f);
						image5(-1.f, -1.f, 0.f);
						float32 px = 1e9f, py = 1e9f, pyMax = -1e9f;
						for (uint32 i = 0; i < (uint32)ctxS.dlOverlay.vtx.Size(); ++i) {
							const auto &vt = ctxS.dlOverlay.vtx[i];
							if (vt.pos.x < px) px = vt.pos.x;
							if (vt.pos.y < py) py = vt.pos.y;
							if (vt.pos.y > pyMax) pyMax = vt.pos.y;
						}
						const bool dansFenetre = py >= 0.f && pyMax <= 420.5f;
						const float32 x0 = px + 0.5f + 8.f;
						// apres la rangee Angle ; + 26 : la rangee d'opacite (③ du 05/09, apres-midi)
						const float32 yListe = py + 0.5f + 8.f + 26.f + 168.f + 26.f + 26.f + 34.f + 26.f;
						const nkentseu::uint32 boite = nkgui::NkGuiPackColor(
							nkentseu::editorkit::NkThemeUnpack(stI.theme.Get(nkentseu::editorkit::NkRole::InputBg)));
						// les boites du champ « position » des rangees visibles : x0+2 .. x0+42
						auto boitesY = [&](float32 &yMin, uint32 &n) {
							yMin = 1e9f;
							n = 0u;
							for (uint32 i = 0; i < (uint32)ctxS.dlOverlay.vtx.Size(); ++i) {
								const auto &vt = ctxS.dlOverlay.vtx[i];
								if (vt.col == boite && vt.pos.x >= x0 + 1.5f && vt.pos.x <= x0 + 8.f && vt.pos.y >= yListe - 30.f) {
									if (vt.pos.y < yMin) yMin = vt.pos.y;
									++n;
								}
							}
						};
						float32 yAvant = 0.f, yApres = 0.f;
						uint32 nAvant = 0u, nApres = 0u;
						boitesY(yAvant, nAvant);
						// la molette sur la liste : une demi-cran vers le bas = 13 px
						image5(x0 + 100.f, yListe + 30.f, -0.5f);
						image5(-1.f, -1.f, 0.f);
						boitesY(yApres, nApres);
						// le bas VISIBLE : chaque commande porte son rectangle de decoupe ; un sommet
						// au-dela est rogne par le scissor, il ne se voit pas
						float32 pyMax2 = -1e9f;
						for (uint32 c = 0; c < (uint32)ctxS.dlOverlay.cmds.Size(); ++c) {
							const auto &cmd = ctxS.dlOverlay.cmds[c];
							const float32 basClip = cmd.clipRect.y + cmd.clipRect.h;
							for (uint32 k = cmd.idxOffset; k < cmd.idxOffset + cmd.idxCount && k < (uint32)ctxS.dlOverlay.idx.Size(); ++k) {
								const uint32 vi = ctxS.dlOverlay.idx[k];
								if (vi >= (uint32)ctxS.dlOverlay.vtx.Size())
									continue;
								float32 yv = ctxS.dlOverlay.vtx[vi].pos.y;
								if (yv > basClip)
									yv = basClip;
								if (yv > pyMax2)
									pyMax2 = yv;
							}
						}
						const bool defile = nAvant > 0u && nApres > 0u && yApres < yAvant - 5.f;
						stI.picker = DesignState::DemandePicker();
						image5(-1.f, -1.f, 0.f);
						while (g12.arrets.Size() > 3u)
							g12.arrets.RemoveAt(g12.arrets.Size() - 1u);
						snprintf(det, sizeof(det),
								 "fenetre 420 : popover y %.0f..%.0f (dans la fenetre=%d), %u arrets ; boites de la liste (x0=%.0f, yListe=%.0f) : "
								 "%u sommets, premiere a y=%.1f puis, apres une demi-cran de molette, %u sommets, a y=%.1f (defile=%d) ; bas VISIBLE apres=%.0f",
								 py, pyMax, dansFenetre ? 1 : 0, 12u, x0, yListe, nAvant, yAvant, nApres, yApres, defile ? 1 : 0, pyMax2);
						check("60n. ⑤ L'ASCENSEUR DU POPOVER : douze arrets dans une fenetre de 420 px -- le popover tient dans la "
							  "fenetre, la liste defile a la molette (l'interieur bouge, pas le popover)",
							  dansFenetre && defile && pyMax2 <= 420.5f, det);
					}
				}
				// 60p. LE DEGRADE SURVIT A LA GEOMETRIE (Rodolf : « des que je modifie la geometrie, le
				// degrade disparait ») : son noeud a cle simple, le degrade pose PAR LE POPOVER (la
				// vignette Lineaire), puis deplacer sur la toile, tirer une poignee de forme,
				// changer la largeur. Apres chaque geste : la liste d'arrets est dans le document,
				// le dessin enregistre est encore un degrade, et le fichier le garde.
				{
					struct PeintrePolyP : public NkRecordingPaint {
							uint32 nPoly = 0u;
							bool PolygonHex(const float32 *, int32, uint32) override {
								++nPoly;
								return true;
							}
					};
					int32 rs3 = -1;
					for (uint32 i = 0; i < (uint32)stI.doc.nodes.Size(); ++i)
						if (NkComponentDecl::StrEq(stI.doc.nodes[i].label.Data(), "Bouton_Connexion"))
							rs3 = (int32)i;
					bool poseParPopover = false, apresDeplacer = false, apresPoignee = false, apresLargeur = false, fichier = false;
					uint32 polyAvant = 0u, polyApres = 0u;
					char etape[160] = "";
					if (rs3 >= 0) {
						NkUINode &n3 = stI.doc.nodes[(uint32)rs3];
						n3.posX = 40.f; // a l'ecart des autres noeuds des sondes
						n3.posY = 120.f;
						stI.Recompute(NkPaintRect{0.f, 0.f, 600.f, 900.f});
						stI.SelectSingle(rs3);
						static PreviewPanel toile3(&stI);
						auto scene3 = [&](float32 mx, float32 my, bool bas) {
							ctxI.input.mousePos = {mx, my};
							ctxI.input.mouseDown[0] = bas;
							ctxI.BeginFrame(0.016f);
							ctxI.BeginLayout({0.f, 0.f, 340.f, 900.f});
							toile3.OnUI(ec);
							ctxI.BeginLayout({340.f, 0.f, 260.f, 900.f});
							insp.OnUI(ec);
							NkDessinerPickerDemande(ctxI, stI);
							ctxI.EndFrame();
						};
						auto tirer3 = [&](float32 x0, float32 y0, float32 x1, float32 y1) {
							scene3(x0, y0, false);
							scene3(x0, y0, true);
							scene3((x0 + x1) * 0.5f, (y0 + y1) * 0.5f, true);
							scene3(x1, y1, true);
							scene3(x1, y1, false);
							scene3(-1.f, -1.f, false);
						};
						auto degradeVivant = [&]() {
							const NkUINode &q = stI.doc.nodes[(uint32)rs3];
							return !q.fills.Empty() && q.fills[0].degrade.Actif();
						};
						auto polygones = [&]() {
							PeintrePolyP rec;
							RenderDocument(rec, stI.doc, NkPaintRect{0.f, 0.f, 600.f, 900.f});
							return rec.nPoly;
						};
						// LE VRAI PEINTRE DE LA TOILE (NkDesignPaint -> triangles dans ctxI.dl) : combien de
						// sommets de couleurs de bande (ni blanc, ni accent, ni la couleur unie) dans la boite
						// mesure JUSTE sous une transformee (le noeud n'est plus dans sa boite de
						// disposition) : les sommets de la toile AVEC le degrade moins ceux SANS
						// (arrets retires le temps d'une image) = ce que les bandes ajoutent
						auto sommetsToile = [&]() -> uint32 {
							scene3(-1.f, -1.f, false);
							const uint32 avec = (uint32)ctxI.dl.vtx.Size();
							NkVector<NkArretDegrade> garde = n3.fills[0].degrade.arrets;
							n3.fills[0].degrade.arrets.Clear();
							scene3(-1.f, -1.f, false);
							const uint32 sans = (uint32)ctxI.dl.vtx.Size();
							n3.fills[0].degrade.arrets = garde;
							scene3(-1.f, -1.f, false);
							return avec > sans ? avec - sans : 0u;
						};
						// 1. le degrade pose par le popover : la vignette « Lineaire » (2e vignette)
						stI.picker = DesignState::DemandePicker();
						stI.picker.ouvert = true;
						stI.picker.id = ctxI.GetId("##sonde.popover.geometrie");
						stI.picker.genre = 1u;
						stI.picker.noeud = rs3;
						stI.picker.index = 0;
						stI.picker.ancre = {590.f, 20.f, 16.f, 16.f};
						n3.MaterialiserFills();
						scene3(-1.f, -1.f, false);
						scene3(-1.f, -1.f, false);
						float32 px = 1e9f, py = 1e9f;
						for (uint32 i = 0; i < (uint32)ctxI.dlOverlay.vtx.Size(); ++i) {
							if (ctxI.dlOverlay.vtx[i].pos.x < px) px = ctxI.dlOverlay.vtx[i].pos.x;
							if (ctxI.dlOverlay.vtx[i].pos.y < py) py = ctxI.dlOverlay.vtx[i].pos.y;
						}
						const float32 vx = px + 0.5f + 8.f + 24.f + 12.f, vy = py + 0.5f + 8.f + 11.f; // la 2e vignette (24 px + 4)
						scene3(vx, vy, false);
						scene3(vx, vy, true);
						scene3(vx, vy, false);
						scene3(-1.f, -1.f, false);
						poseParPopover = degradeVivant();
						polyAvant = polygones();
						scene3(-1.f, -1.f, false);
						const uint32 toileAvant = sommetsToile();
						snprintf(etape, sizeof(etape), "pose par la vignette=%d (arrets=%u, polygones=%u)", poseParPopover ? 1 : 0,
								 (uint32)(n3.fills.Empty() ? 0u : n3.fills[0].degrade.arrets.Size()), polyAvant);
						stI.picker = DesignState::DemandePicker();
						scene3(-1.f, -1.f, false);
						// 2. deplacer le corps sur la toile
						NkLayoutResult scr;
						stI.ProjectToScreen(scr);
						NkPaintRect r3 = scr.At(rs3);
						tirer3(r3.x + r3.w * 0.5f, r3.y + r3.h * 0.5f - 6.f, r3.x + r3.w * 0.5f + 25.f, r3.y + r3.h * 0.5f + 9.f);
						apresDeplacer = degradeVivant();
						// 3. tirer la poignee de forme du bord droit
						stI.ProjectToScreen(scr);
						r3 = scr.At(rs3);
						tirer3(r3.x + r3.w - 1.f, r3.y + r3.h * 0.5f, r3.x + r3.w + 30.f, r3.y + r3.h * 0.5f);
						apresPoignee = degradeVivant();
						// 4. la largeur par le modele (ce que le champ W ecrit)
						n3.width.value += 10.f;
						stI.doc.MarkHumanEdit(rs3);
						stI.Recompute(NkPaintRect{0.f, 0.f, 600.f, 900.f});
						scene3(-1.f, -1.f, false);
						apresLargeur = degradeVivant();
						polyApres = polygones();
						scene3(-1.f, -1.f, false);
						const uint32 toileApres = sommetsToile();
						{
							const size_t l = strlen(etape);
							snprintf(etape + l, sizeof(etape) - l, " ; TOILE (vrai peintre) : sommets de bandes %u -> %u", toileAvant, toileApres);
						}
						apresLargeur = apresLargeur && toileAvant >= 100u && toileApres >= 100u;
						// 4b. les autres chemins de l'application : rotation, arrondi, l'observateur
						//     d'historique puis un rechargement depuis son instantane (Annuler), la
						//     propagation vers les instances, la resynchronisation de l'hote
						n3.rotation = 30.f;
						n3.radius = 12.f;
						n3.echelleX = 1.3f; // la matrice effective devient non identite : le vrai peintre transforme
						n3.echelleY = 0.8f;
						stI.doc.MarkHumanEdit(rs3);
						stI.Recompute(NkPaintRect{0.f, 0.f, 600.f, 900.f});
						stI.host.SyncTo(stI.doc);
						scene3(-1.f, -1.f, false);
						const uint32 toileTransforme = sommetsToile();
						const bool apresRotation = degradeVivant() && polygones() >= 24u && toileTransforme >= 100u;
						{
							const size_t l = strlen(etape);
							snprintf(etape + l, sizeof(etape) - l, " ; sous rotation+echelle, toile : %u sommets", toileTransforme);
						}
						n3.echelleX = n3.echelleY = 1.f;
						{
							NkString ser;
							for (int32 k = 0; k < 10; ++k) {
								ser = NkString();
								stI.doc.Save(ser);
								stI.histoire.Observer(ser);
							}
							stI.RechargerDepuis(ser);
						}
						const bool apresRecharge = degradeVivant();
						stI.doc.PropagerVersInstances(-1);
						const bool apresPropagation = degradeVivant();
						{
							const size_t l = strlen(etape);
							snprintf(etape + l, sizeof(etape) - l, " ; rotation+arrondi=%d, recharge (annuler)=%d, propagation=%d",
									 apresRotation ? 1 : 0, apresRecharge ? 1 : 0, apresPropagation ? 1 : 0);
						}
						apresLargeur = apresLargeur && apresRotation && apresRecharge && apresPropagation;
						// 5. le fichier
						NkString s;
						stI.doc.Save(s);
						NkUIDocument relu;
						fichier = relu.Load(s.Data()) && relu.IsValidIndex(rs3) && !relu.nodes[(uint32)rs3].fills.Empty()
								  && relu.nodes[(uint32)rs3].fills[0].degrade.Actif();
						if (!n3.fills.Empty())
							n3.fills[0].degrade.arrets.Clear();
					}
					stI.SelectSingle(rc);
					snprintf(det, sizeof(det), "%s ; apres deplacer=%d, apres poignee=%d, apres largeur=%d (polygones %u -> %u) ; fichier=%d",
							 etape, apresDeplacer ? 1 : 0, apresPoignee ? 1 : 0, apresLargeur ? 1 : 0, polyAvant, polyApres, fichier ? 1 : 0);
					check("60p. LE DEGRADE SURVIT A LA GEOMETRIE : pose par le popover sur un noeud a cle simple, il reste apres un "
						  "deplacement, une poignee de forme, un changement de largeur -- dans le document, au dessin, au fichier",
						  poseParPopover && apresDeplacer && apresPoignee && apresLargeur && polyApres >= 24u && fichier, det);
				}
				// 60q. ②-1 LES 18 MODES DE FUSION SE CHOISISSENT (Rodolf : « les elements de la goutte
				// ne sont pas selectionnables ») : par le menu, « Multiply » s'ecrit dans le modele
				// (`multiply`, la cle CSS), part au fichier (`fusion=multiply`), revient a la lecture,
				// et « Normal » l'efface (rien au fichier). Le pied dit qu'il n'est pas peint.
				{
					stI.SelectSingle(rc);
					stI.picker = DesignState::DemandePicker();
					stI.picker.ouvert = true;
					stI.picker.id = ctxI.GetId("##sonde.popover.fusion");
					stI.picker.genre = 1u;
					stI.picker.noeud = rc;
					stI.picker.index = 0;
					stI.picker.ancre = {360.f, 200.f, 16.f, 16.f};
					auto image6 = [&](float32 mx, float32 my, bool bas) {
						ctxI.input.mousePos = {mx, my};
						ctxI.input.mouseDown[0] = bas;
						ctxI.BeginFrame(0.016f);
						ctxI.BeginLayout({340.f, 0.f, 260.f, 900.f});
						insp.OnUI(ec);
						NkDessinerPickerDemande(ctxI, stI);
						ctxI.EndFrame();
					};
					image6(-1.f, -1.f, false);
					image6(-1.f, -1.f, false);
					float32 px = 1e9f, py = 1e9f;
					for (uint32 i = 0; i < (uint32)ctxI.dlOverlay.vtx.Size(); ++i) {
						if (ctxI.dlOverlay.vtx[i].pos.x < px) px = ctxI.dlOverlay.vtx[i].pos.x;
						if (ctxI.dlOverlay.vtx[i].pos.y < py) py = ctxI.dlOverlay.vtx[i].pos.y;
					}
					const float32 x0 = px + 0.5f + 8.f, y0 = py + 0.5f + 8.f, x1m = px + 0.5f + 250.f - 8.f;
					auto clic = [&](float32 x, float32 y) {
						image6(x, y, false);
						image6(x, y, true);
						image6(x, y, false);
						image6(-1.f, -1.f, false);
					};
					const float32 gx = x0 + 165.f, gy = y0 + 11.f, yMenu = y0 + 26.f;
					clic(gx, gy);								// la goutte : le menu s'ouvre
					clic(x0 + 20.f, yMenu + 2.f + 2.f * 18.f + 9.f); // colonne 0, ligne 2 : Multiply
					const NkString m1 = stI.doc.nodes[(uint32)rc].fills[0].fusion;
					const bool choisi = NkComponentDecl::StrEq(m1.Data(), "multiply");
					// ②-2 : Multiply est EXACT au GPU, le pied dit « peint » ; Overlay lit la
					// destination, le pied dit « pas encore peint »
					const bool ditPeint = stI.status.Data() && strstr(stI.status.Data(), "exact") != nullptr;
					NkString s;
					stI.doc.Save(s);
					const bool ecrit = strstr(s.Data(), "fusion=multiply") != nullptr;
					NkUIDocument relu;
					const bool relus = relu.Load(s.Data()) && relu.IsValidIndex(rc) && !relu.nodes[(uint32)rc].fills.Empty()
									   && NkComponentDecl::StrEq(relu.nodes[(uint32)rc].fills[0].fusion.Data(), "multiply");
					clic(gx, gy);
					clic(x0 + (x1m - x0) * 0.5f + 20.f, yMenu + 2.f + 9.f); // colonne 1, ligne 0 : Overlay
					const bool overlay = NkComponentDecl::StrEq(stI.doc.nodes[(uint32)rc].fills[0].fusion.Data(), "overlay");
					const bool dit = stI.status.Data() && strstr(stI.status.Data(), "pas encore peint") != nullptr;
					clic(gx, gy);
					clic(x0 + 20.f, yMenu + 2.f + 9.f); // Normal : efface
					const bool efface = stI.doc.nodes[(uint32)rc].fills[0].fusion.Empty();
					NkString s2;
					stI.doc.Save(s2);
					const bool rienNormal = strstr(s2.Data(), "fusion=") == nullptr;
					stI.picker = DesignState::DemandePicker();
					image6(-1.f, -1.f, false);
					snprintf(det, sizeof(det), "Multiply choisi -> `%s` (pied : peint, exact=%d) ; fichier `fusion=multiply`=%d ; relu=%d ; Overlay -> `overlay`=%d (pied : pas encore peint=%d) ; Normal -> efface=%d, rien au fichier=%d",
							 m1.Data() ? m1.Data() : "", ditPeint ? 1 : 0, ecrit ? 1 : 0, relus ? 1 : 0, overlay ? 1 : 0, dit ? 1 : 0, efface ? 1 : 0, rienNormal ? 1 : 0);
					check("60q. ②-1 LES 18 MODES DE FUSION SE CHOISISSENT : « Multiply » par le menu s'ecrit dans le modele (cle CSS), "
						  "part au fichier et en revient, le pied dit qu'il est peint (exact) ; « Overlay » s'ecrit et le pied dit "
						  "« pas encore peint » ; « Normal » l'efface, rien au fichier",
						  choisi && ditPeint && ecrit && relus && overlay && dit && efface && rienNormal, det);
				}
				// 60s. LA ROTATION DU NOEUD SURVIT AUX POIGNEES DE DEGRADE (Rodolf, 04/09 soir : « le
				// systeme de rotation ne fonctionne plus ») : sans degrade, puis avec un lineaire a 45
				// degres (ses extremites tombent aux coins, la ou vivent les arcs de rotation),
				// popover ferme -- tirer l'arc du coin haut-droit autour du centre tourne le noeud.
				// La regle : quand deux zones se chevauchent, LA PLUS PROCHE DU POINTEUR gagne.
				{
					static PreviewPanel toileR(&stI);
					auto sceneR = [&](float32 mx, float32 my, bool bas) {
						ctxI.input.mousePos = {mx, my};
						ctxI.input.mouseDown[0] = bas;
						ctxI.BeginFrame(0.016f);
						ctxI.BeginLayout({0.f, 0.f, 340.f, 900.f});
						toileR.OnUI(ec);
						ctxI.BeginLayout({340.f, 0.f, 260.f, 900.f});
						insp.OnUI(ec);
						NkDessinerPickerDemande(ctxI, stI);
						ctxI.EndFrame();
					};
					NkUINode &nd = stI.doc.nodes[(uint32)rc];
					NkDegrade &gd = nd.fills[1].degrade;
					const NkVector<NkArretDegrade> arretsGarde = gd.arrets;
					float32 rotApres[2] = {0.f, 0.f};
					bool fixe[2] = {false, false};
					for (uint32 cas = 0; cas < 2u; ++cas) {
						if (cas == 0u)
							gd.arrets.Clear(); // sans degrade
						else {
							gd.arrets = arretsGarde; // avec : lineaire a 45 degres, popover ferme
							gd.type = NkString("lineaire");
							gd.angle = 45.f;
						}
						nd.rotation = 0.f;
						stI.picker = DesignState::DemandePicker();
						stI.SelectSingle(rc);
						if (cas == 0u) {
							ctxI.popupDepth = 0; // popover ferme, pile de popups videe
						} else {
							// popover OUVERT sur le degrade, comme Rodolf travaille
							stI.picker.ouvert = true;
							stI.picker.id = ctxI.GetId("##sonde.popover.rotation");
							stI.picker.genre = 1u;
							stI.picker.noeud = rc;
							stI.picker.index = 1;
							stI.picker.ancre = {590.f, 20.f, 16.f, 16.f};
						}
						// deux appuis au meme point a moins de 0,4 s = un DOUBLE-CLIC pour le kit (le
						// cas 0 vient d'appuyer la) : on laisse passer le delai, comme une main
						for (int32 k = 0; k < 32; ++k)
							sceneR(-1.f, -1.f, false);
						const int32 prof = ctxI.popupDepth;
						NkLayoutResult scr;
						stI.ProjectToScreen(scr);
						const NkPaintRect rs = scr.At(rc);
						const NkPaintRect arc = NkPoigneeRotation(rs, 1u, NkTaillePoigneeRotation()); // haut-droit
						const float32 ax = arc.x + arc.w * 0.5f, ay = arc.y + arc.h * 0.5f;
						const float32 cx = rs.x + rs.w * 0.5f, cy = rs.y + rs.h * 0.5f;
						const float32 posX0 = nd.posX, posY0 = nd.posY;
						// tirer l'arc d'un quart de tour horaire autour du centre
						sceneR(ax, ay, false);
						sceneR(ax, ay, true);
						sceneR(cx + (ay - cy) * -1.f, cy + (ax - cx), true);
						sceneR(cx - (ay - cy), cy + (ax - cx), true);
						sceneR(cx - (ay - cy), cy + (ax - cx), false);
						sceneR(-1.f, -1.f, false);
						rotApres[cas] = nd.rotation;
						fixe[cas] = nd.posX == posX0 && nd.posY == posY0 && (cas == 0u ? prof == 0 : prof >= 1);
						nd.rotation = 0.f;
					}
					gd.arrets = arretsGarde;
					gd.angle = 0.f;
					stI.Recompute(NkPaintRect{0.f, 0.f, 600.f, 900.f});
					sceneR(-1.f, -1.f, false);
					snprintf(det, sizeof(det), "sans degrade, popover ferme : rotation 0 -> %.0f (noeud fixe=%d) ; avec lineaire a 45, popover OUVERT : rotation 0 -> %.0f (noeud fixe, popup present=%d)",
							 (double)rotApres[0], fixe[0] ? 1 : 0, (double)rotApres[1], fixe[1] ? 1 : 0);
					check("60s. LA ROTATION DU NOEUD tient avec et sans degrade : l'arc du coin tourne le noeud d'un quart de tour, "
						  "meme quand une extremite de degrade tombe au coin -- la zone la plus proche du pointeur gagne",
						  rotApres[0] > 60.f && rotApres[0] < 120.f && rotApres[1] > 60.f && rotApres[1] < 120.f && fixe[0] && fixe[1], det);
				}
				// 60t. ② LES ZONES DE DETECTION, une tolerance nommee (12 px ecran) : a petit zoom, un
				// appui a 11 px hors de la boite de l'arc tourne encore le noeud ; un appui 10 px
				// au-dela du milieu du bord droit redimensionne ; le centre du corps deplace (la
				// bande est bornee au tiers du noeud : un petit noeud garde son corps).
				{
					static PreviewPanel toileT(&stI);
					auto sceneT = [&](float32 mx, float32 my, bool bas) {
						ctxI.input.mousePos = {mx, my};
						ctxI.input.mouseDown[0] = bas;
						ctxI.BeginFrame(0.016f);
						ctxI.BeginLayout({0.f, 0.f, 340.f, 900.f});
						toileT.OnUI(ec);
						ctxI.BeginLayout({340.f, 0.f, 260.f, 900.f});
						insp.OnUI(ec);
						NkDessinerPickerDemande(ctxI, stI);
						ctxI.EndFrame();
					};
					auto tirerT = [&](float32 x0, float32 y0, float32 x1, float32 y1) {
						for (int32 k = 0; k < 32; ++k)
							sceneT(-1.f, -1.f, false); // le delai du double-clic
						sceneT(x0, y0, false);
						sceneT(x0, y0, true);
						sceneT((x0 + x1) * 0.5f, (y0 + y1) * 0.5f, true);
						sceneT(x1, y1, true);
						sceneT(x1, y1, false);
						sceneT(-1.f, -1.f, false);
					};
					NkUINode &nd = stI.doc.nodes[(uint32)rc];
					nd.fills[1].degrade.arrets.Clear(); // sans degrade : les poignees de forme seules
					nd.rotation = 0.f;
					stI.picker = DesignState::DemandePicker();
					ctxI.popupDepth = 0;
					stI.SelectSingle(rc);
					sceneT(-1.f, -1.f, false);
					NkLayoutResult scr;
					stI.ProjectToScreen(scr);
					const NkPaintRect rs = scr.At(rc);
					const float32 cx = rs.x + rs.w * 0.5f, cy = rs.y + rs.h * 0.5f;
					// a. la rotation, a 11 px HORS de la boite de l'arc (dans la tolerance de 12)
					const NkPaintRect arc = NkPoigneeRotation(rs, 1u, NkTaillePoigneeRotation());
					const float32 ax = arc.x + arc.w + 11.f - arc.w * 0.5f + arc.w * 0.5f, ay = arc.y + arc.h * 0.5f; // 11 px a droite du bord droit de la boite
					tirerT(ax, ay, cx - (ay - cy), cy + (ax - cx));
					const float32 rot = nd.rotation;
					nd.rotation = 0.f;
					// b. le redimensionnement, 10 px au-dela du milieu du bord droit
					const float32 w0 = nd.width.value;
					tirerT(rs.x + rs.w + 10.f, cy, rs.x + rs.w + 40.f, cy);
					const float32 w1 = nd.width.value;
					nd.width.value = w0;
					// c. le corps : le centre deplace (la bande d'un noeud de 5 px de haut fait 2 px, pas 12)
					// plusieurs noeuds des sondes se superposent a l'origine du cadre : c'est celui du
					// DESSUS que la toile deplace -- on juge « un noeud a bouge »
					NkVector<float32> avX, avY;
					for (uint32 i = 0; i < (uint32)stI.doc.nodes.Size(); ++i) {
						avX.PushBack(stI.doc.nodes[i].posX);
						avY.PushBack(stI.doc.nodes[i].posY);
					}
					tirerT(cx, cy, cx + 20.f, cy + 8.f);
					bool deplace = false;
					for (uint32 i = 0; i < (uint32)stI.doc.nodes.Size(); ++i) {
						if (stI.doc.nodes[i].posX != avX[i] || stI.doc.nodes[i].posY != avY[i])
							deplace = true;
						stI.doc.nodes[i].posX = avX[i];
						stI.doc.nodes[i].posY = avY[i];
					}
					const float32 h1 = nd.height.value;
					stI.Recompute(NkPaintRect{0.f, 0.f, 600.f, 900.f});
					sceneT(-1.f, -1.f, false);
					snprintf(det, sizeof(det), "noeud a l'ecran %.0f x %.0f px ; arc a 11 px hors boite : rotation 0 -> %.0f ; bord droit +10 px : largeur %.0f -> %.0f ; "
											   "centre du corps : deplace=%d (hauteur intacte=%d)",
							 (double)rs.w, (double)rs.h, (double)rot, (double)w0, (double)w1, deplace ? 1 : 0, h1 == nd.height.value ? 1 : 0);
					check("60t. ② LES ZONES DE DETECTION (tolerance nommee, 12 px ecran) : l'arc se prend 11 px hors de sa boite, "
						  "le bord droit 10 px au-dela, et le centre d'un petit noeud le deplace encore (bande bornee au tiers)",
						  rot > 30.f && w1 > w0 + 5.f && deplace, det);
				}
				// 60u. ④ LE SELECTEUR SE FERME : un clic dans le vide de la toile deselectionne et le
				// popover se ferme (le popup du kit aussi) ; selectionner un autre noeud le ferme ;
				// une deselection par le modele le ferme ; le clic dans le vide n'est pas consomme.
				{
					static PreviewPanel toileU(&stI);
					auto sceneU = [&](float32 mx, float32 my, bool bas) {
						ctxI.input.mousePos = {mx, my};
						ctxI.input.mouseDown[0] = bas;
						ctxI.BeginFrame(0.016f);
						ctxI.BeginLayout({0.f, 0.f, 340.f, 900.f});
						toileU.OnUI(ec);
						ctxI.BeginLayout({340.f, 0.f, 260.f, 900.f});
						insp.OnUI(ec);
						NkDessinerPickerDemande(ctxI, stI);
						ctxI.EndFrame();
					};
					auto ouvrir = [&]() {
						stI.SelectSingle(rc);
						stI.picker = DesignState::DemandePicker();
						stI.picker.ouvert = true;
						stI.picker.id = ctxI.GetId("##sonde.popover.fermeture");
						stI.picker.genre = 1u;
						stI.picker.noeud = rc;
						stI.picker.index = 0;
						stI.picker.ancre = {590.f, 20.f, 16.f, 16.f};
						for (int32 k = 0; k < 32; ++k)
							sceneU(-1.f, -1.f, false);
					};
					// 1. le clic dans le vide de la toile
					ouvrir();
					const bool ouvertAvant = stI.picker.ouvert && ctxI.popupDepth >= 1;
					sceneU(300.f, 600.f, false);
					sceneU(300.f, 600.f, true);
					sceneU(300.f, 600.f, false);
					sceneU(-1.f, -1.f, false);
					sceneU(-1.f, -1.f, false);
					const int32 selVide = stI.selected;
					const bool videDeselectionne = !stI.doc.IsValidIndex(stI.selected) || stI.selected == 0;
					const bool fermeVide = !stI.picker.ouvert && ctxI.popupDepth == 0;
					// 2. un autre noeud selectionne
					ouvrir();
					int32 autre = -1;
					for (uint32 i = 0; i < (uint32)stI.doc.nodes.Size(); ++i)
						if ((int32)i != rc && stI.doc.nodes[i].parent == pg) {
							autre = (int32)i;
							break;
						}
					stI.SelectSingle(autre);
					sceneU(-1.f, -1.f, false);
					sceneU(-1.f, -1.f, false);
					const bool fermeAutre = !stI.picker.ouvert && ctxI.popupDepth == 0;
					// 3. la deselection par le modele
					ouvrir();
					stI.SelectClear();
					sceneU(-1.f, -1.f, false);
					sceneU(-1.f, -1.f, false);
					const bool fermeRien = !stI.picker.ouvert && ctxI.popupDepth == 0;
					stI.SelectSingle(rc);
					sceneU(-1.f, -1.f, false);
					snprintf(det, sizeof(det), "ouvert avant=%d ; clic dans le vide : selection=%d (deselectionne=%d), ferme=%d ; autre noeud (%d) : ferme=%d ; deselection par le modele : ferme=%d",
							 ouvertAvant ? 1 : 0, selVide, videDeselectionne ? 1 : 0, fermeVide ? 1 : 0, autre, fermeAutre ? 1 : 0, fermeRien ? 1 : 0);
					check("60u. ④ LE SELECTEUR SE FERME quand rien n'est selectionne ou qu'on clique dans le vide : le vide "
						  "deselectionne (le clic n'est pas consomme) et le popover se ferme, un autre noeud le ferme, une "
						  "deselection le ferme -- le popup du kit avec",
						  ouvertAvant && videDeselectionne && fermeVide && autre >= 0 && fermeAutre && fermeRien, det);
				}
				// 60v. ③ LES INFO-BULLES : survoler une poignee 0,4 s l'annonce -- l'anneau d'une extremite
				// dit « Tourner l'axe », le disque d'un arret « Glisser l'arret », l'arc du noeud
				// « Tourner », le bord droit « Redimensionner » ; avant le delai, rien ; en partant,
				// rien. (Sans police, le kit ne peint pas la bulle : le texte publie en temoigne.)
				{
					static PreviewPanel toileV(&stI);
					auto sceneV = [&](float32 mx, float32 my) {
						ctxI.input.mousePos = {mx, my};
						ctxI.input.mouseDown[0] = false;
						ctxI.BeginFrame(0.016f);
						ctxI.BeginLayout({0.f, 0.f, 340.f, 900.f});
						toileV.OnUI(ec);
						ctxI.BeginLayout({340.f, 0.f, 260.f, 900.f});
						insp.OnUI(ec);
						NkDessinerPickerDemande(ctxI, stI);
						ctxI.EndFrame();
					};
					NkUINode &nd = stI.doc.nodes[(uint32)rc];
					NkDegrade &gd = nd.fills[1].degrade;
					gd.type = NkString("lineaire");
					gd.angle = 0.f;
					if (gd.arrets.Size() < 3u) {
						gd.arrets.Clear();
						for (uint32 k = 0; k < 3u; ++k) {
							NkArretDegrade ar;
							ar.position = (float32)k * 0.5f;
							ar.couleur = NkString(k == 0 ? "#fafcff" : (k == 1 ? "#d11313" : "#1976d1"));
							gd.arrets.PushBack(ar);
						}
					}
					nd.rotation = 0.f;
					stI.SelectSingle(rc);
					stI.picker = DesignState::DemandePicker();
					stI.picker.ouvert = true; // les anneaux existent popover ouvert
					stI.picker.id = ctxI.GetId("##sonde.popover.bulle");
					stI.picker.genre = 1u;
					stI.picker.noeud = rc;
					stI.picker.index = 1;
					stI.picker.ancre = {590.f, 20.f, 16.f, 16.f};
					for (int32 k = 0; k < 4; ++k)
						sceneV(-1.f, -1.f);
					NkLayoutResult scr;
					stI.ProjectToScreen(scr);
					const NkPaintRect rs = scr.At(rc);
					const renderdetail::NkAxeDegrade axe = renderdetail::NkAxeDegradeGenre(0, rs, gd);
					auto survoler = [&](float32 x, float32 y, int32 images) {
						for (int32 k = 0; k < images; ++k)
							sceneV(x, y);
						return NkString(stI.bulle.Data() ? stI.bulle.Data() : "");
					};
					// a. l'anneau de l'extremite (au-dela, le long de l'axe) : avant le delai rien, apres « Tourner l'axe »
					float32 ex = 0.f, ey = 0.f;
					renderdetail::NkPoigneeDegrade(axe, 1.f, ex, ey);
					float32 dx = axe.bx - axe.ax, dy = axe.by - axe.ay;
					const float32 l = nkentseu::math::NkSqrt(dx * dx + dy * dy);
					dx /= l > 0.001f ? l : 1.f;
					dy /= l > 0.001f ? l : 1.f;
					const NkString avant = survoler(ex + dx * 13.f, ey + dy * 13.f, 5);   // 0,08 s
					const NkString anneau = survoler(ex + dx * 13.f, ey + dy * 13.f, 30); // + 0,48 s
					// b. le disque de l'arret du milieu
					float32 mx0 = 0.f, my0 = 0.f;
					renderdetail::NkPoigneeDegrade(axe, 0.5f, mx0, my0);
					const NkString disque = survoler(mx0, my0, 35);
					// c. l'arc de rotation du noeud (coin haut-droit)
					const NkPaintRect arc = NkPoigneeRotation(rs, 1u, NkTaillePoigneeRotation());
					const NkString tourner = survoler(arc.x + arc.w * 0.5f, arc.y + arc.h * 0.5f, 35);
					// d. le bord droit, a mi-hauteur, loin des pastilles : « Redimensionner »
					const NkString bord = survoler(rs.x + rs.w + 3.f, rs.y + rs.h * 0.25f, 35);
					// e. en partant : rien
					const NkString parti = survoler(-1.f, -1.f, 3);
					stI.picker = DesignState::DemandePicker();
					sceneV(-1.f, -1.f);
					snprintf(det, sizeof(det), "avant 0,4 s : « %s » ; anneau : « %s » ; disque : « %s » ; arc : « %s » ; bord : « %s » ; parti : « %s »",
							 avant.Data() ? avant.Data() : "", anneau.Data(), disque.Data(), tourner.Data(), bord.Data(), parti.Data() ? parti.Data() : "");
					check("60v. ③ LES INFO-BULLES des poignees : rien avant 0,4 s, puis « Tourner l'axe » sur l'anneau, « Glisser "
						  "l'arret » sur le disque, « Tourner » sur l'arc, « Redimensionner » sur le bord, rien en partant",
						  avant.Empty() && NkComponentDecl::StrEq(anneau.Data(), "Tourner l'axe") && NkComponentDecl::StrEq(disque.Data(), "Glisser l'arrêt")
							  && NkComponentDecl::StrEq(tourner.Data(), "Tourner") && NkComponentDecl::StrEq(bord.Data(), "Redimensionner (Maj : proportionnel, Alt : depuis le centre)")
							  && parti.Empty(),
						  det);
				}
				// 60w. ⑤ L'ANGULAIRE TEL QUE LUNACY LE FAIT : les arrets vivent SUR le cercle (fraction de
				// tour depuis l'axe) ; l'extremite glissee le long du cercle TOURNE l'axe ; les deux
				// carres sont sur le cercle, tournent avec l'axe et reglent le rayon ; un arret
				// ajoute sur le cercle prend sa fraction de tour.
				{
					static PreviewPanel toileW(&stI);
					auto sceneW = [&](float32 mx, float32 my, bool bas) {
						ctxI.input.mousePos = {mx, my};
						ctxI.input.mouseDown[0] = bas;
						ctxI.BeginFrame(0.016f);
						ctxI.BeginLayout({0.f, 0.f, 340.f, 900.f});
						toileW.OnUI(ec);
						ctxI.BeginLayout({340.f, 0.f, 260.f, 900.f});
						insp.OnUI(ec);
						NkDessinerPickerDemande(ctxI, stI);
						ctxI.EndFrame();
					};
					auto tirerW = [&](float32 x0, float32 y0, float32 xm, float32 ym, float32 x1, float32 y1) {
						for (int32 k = 0; k < 32; ++k)
							sceneW(-1.f, -1.f, false);
						sceneW(x0, y0, false);
						sceneW(x0, y0, true);
						sceneW(xm, ym, true);
						sceneW(x1, y1, true);
						sceneW(x1, y1, false);
						sceneW(-1.f, -1.f, false);
					};
					NkUINode &nd = stI.doc.nodes[(uint32)rc];
					NkDegrade &gd = nd.fills[1].degrade;
					gd.type = NkString("angulaire");
					gd.angle = 0.f;
					gd.origineX = gd.origineY = 0.5f;
					gd.rayonX = gd.rayonY = 0.5f;
					gd.arrets.Clear();
					for (uint32 k = 0; k < 3u; ++k) {
						NkArretDegrade ar;
						ar.position = (float32)k * 0.5f;
						ar.couleur = NkString(k == 0 ? "#fafcff" : (k == 1 ? "#d11313" : "#1976d1"));
						gd.arrets.PushBack(ar);
					}
					stI.SelectSingle(rc);
					stI.picker = DesignState::DemandePicker();
					stI.picker.ouvert = true;
					stI.picker.id = ctxI.GetId("##sonde.popover.angulaire");
					stI.picker.genre = 1u;
					stI.picker.noeud = rc;
					stI.picker.index = 1;
					stI.picker.ancre = {590.f, 20.f, 16.f, 16.f};
					sceneW(-1.f, -1.f, false);
					sceneW(-1.f, -1.f, false);
					NkLayoutResult scr;
					stI.ProjectToScreen(scr);
					const NkPaintRect rs = scr.At(rc);
					const float32 posX0 = nd.posX, posY0 = nd.posY;
					float32 pvx = 0.f, pvy = 0.f;
					renderdetail::NkPivotDegrade(2, rs, gd, pvx, pvy);
					// a. les arrets sont SUR le cercle : la pastille de t = 0,25 est a la distance du rayon
					float32 qx = 0.f, qy = 0.f;
					renderdetail::NkPoigneeDegradeGenre(2, rs, gd, 0.25f, qx, qy);
					const float32 R = rs.h * 0.5f;
					const float32 dR = nkentseu::math::NkSqrt((qx - pvx) * (qx - pvx) + (qy - pvy) * (qy - pvy));
					const bool surCercle = dR > R - 0.6f && dR < R + 0.6f && qx < pvx - R * 0.9f; // un quart de tour horaire depuis le bas : a gauche
					// b. l'extremite (100 %) glissee le long du cercle jusqu'au quart de tour -> l'axe tourne a 90
					float32 ex = 0.f, ey = 0.f, mx = 0.f, my = 0.f, fx = 0.f, fy = 0.f;
					renderdetail::NkPoigneeDegradeGenre(2, rs, gd, 1.f, ex, ey);
					renderdetail::NkPoigneeDegradeGenre(2, rs, gd, 0.125f, mx, my);
					renderdetail::NkPoigneeDegradeGenre(2, rs, gd, 0.25f, fx, fy);
					tirerW(ex, ey, mx, my, fx, fy);
					const float32 angleApres = gd.angle;
					// c. les carres suivent l'axe : le premier carre est a +90 degres de l'axe, sur le cercle
					float32 cx0 = 0.f, cy0 = 0.f;
					renderdetail::NkCarreAngulaire(rs, gd, 0, cx0, cy0);
					float32 sC = 0.f, cC = 1.f;
					NkSinCosDeg(gd.angle + 90.f, sC, cC);
					const bool carreSuit = (cx0 - (pvx - sC * R)) * (cx0 - (pvx - sC * R)) + (cy0 - (pvy + cC * R)) * (cy0 - (pvy + cC * R)) < 1.f;
					// d. le carre glisse vers l'exterieur : le rayon grandit
					const float32 ry0 = gd.rayonY;
					const float32 ux = (cx0 - pvx) / R, uy = (cy0 - pvy) / R;
					tirerW(cx0, cy0, cx0 + ux * 4.f, cy0 + uy * 4.f, cx0 + ux * 8.f, cy0 + uy * 8.f);
					const float32 ryApres = gd.rayonY;
					const bool rayonGrandit = ryApres > ry0 + 0.05f;
					// e. un arret ajoute par un clic SUR le cercle a mi-chemin (t = 0,75) prend sa fraction de tour
					const uint32 nAv = (uint32)gd.arrets.Size();
					float32 ax = 0.f, ay = 0.f;
					renderdetail::NkPoigneeDegradeGenre(2, rs, gd, 0.75f, ax, ay);
					for (int32 k = 0; k < 32; ++k)
						sceneW(-1.f, -1.f, false);
					sceneW(ax, ay, false);
					sceneW(ax, ay, true);
					sceneW(ax, ay, false);
					sceneW(-1.f, -1.f, false);
					bool ajoute = gd.arrets.Size() == nAv + 1u;
					float32 tAjoute = -1.f;
					if (ajoute)
						for (uint32 i = 0; i < (uint32)gd.arrets.Size(); ++i)
							if (gd.arrets[i].position > 0.7f && gd.arrets[i].position < 0.8f)
								tAjoute = gd.arrets[i].position;
					// retour
					gd.type = NkString("lineaire");
					gd.angle = 0.f;
					gd.rayonY = 0.5f;
					while (gd.arrets.Size() > 3u)
						gd.arrets.RemoveAt(gd.arrets.Size() - 1u);
					gd.arrets[1].position = 0.5f;
					stI.picker = DesignState::DemandePicker();
					sceneW(-1.f, -1.f, false);
					snprintf(det, sizeof(det), "arret a 25 %% sur le cercle=%d (rayon %.1f, distance %.1f) ; extremite glissee d'un quart de tour -> angle %.0f ; "
											   "carre a +90 de l'axe=%d ; carre tire -> rayon %.2f -> %.2f ; arret ajoute sur le cercle=%d (t=%.2f) ; noeud fixe=%d",
							 surCercle ? 1 : 0, (double)R, (double)dR, (double)angleApres, carreSuit ? 1 : 0, (double)ry0, (double)ryApres, ajoute ? 1 : 0,
							 (double)tAjoute, (nd.posX == posX0 && nd.posY == posY0) ? 1 : 0);
					check("60w. ⑤ L'ANGULAIRE TEL QUE LUNACY : les arrets SUR le cercle (fraction de tour), l'extremite glissee le "
						  "long du cercle tourne l'axe, les carres sur le cercle suivent l'axe et reglent le rayon, un clic sur le "
						  "cercle ajoute un arret a sa fraction de tour",
						  surCercle && angleApres > 80.f && angleApres < 100.f && carreSuit && rayonGrandit && ajoute && tAjoute > 0.7f
							  && nd.posX == posX0 && nd.posY == posY0,
						  det);
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
			// 61c. les QUATRE genres ont leurs poignees depuis qu'ils sont PEINTS (04/09, ① de
			//      Rodolf) : la porte de la toile rend le remplissage pour le radial aussi
			NkUINode nL, nR;
			NkRemplissage fL;
			fL.degrade = g;
			nL.fills.PushBack(fL);
			NkRemplissage fR;
			fR.degrade = g;
			fR.degrade.type = NkString("radial");
			nR.fills.PushBack(fR);
			const int32 iL = renderdetail::NkRemplissageDegradeToile(nL), iR = renderdetail::NkRemplissageDegradeToile(nR);
			snprintf(det, sizeof(det), "lineaire -> remplissage %d ; radial (peint, ses poignees) -> %d", iL, iR);
			check("61c. LES QUATRE GENRES ONT LEURS POIGNEES : la porte de la toile rend le remplissage pour le "
				  "lineaire ET le radial (peints tous deux) -- une poignee ne se montre que sur ce que le peintre rend",
				  iL == 0 && iR == 0, det);
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
		// ── 64. RADIAL, ANGULAIRE, LOSANGE PEINTS (Rodolf : « les autres ne correspondent
		//    pas vraiment ») : la meme methode que le lineaire, des bandes qui suivent la
		//    forme. Temoin : les polygones captes, deux couleurs FRANCHES (rouge jusqu'a
		//    50 %, bleu ensuite) ; en chaque point echantillonne, le DERNIER polygone qui le
		//    contient porte la couleur que la formule du genre annonce ; aucun sommet ne
		//    sort du contour arrondi ; le nombre de bandes suit la taille.
		{
			struct PeintrePoly64 : public NkRecordingPaint {
					NkVector<float32> pts;
					NkVector<int32> tailles;
					NkVector<uint32> couleurs;
					bool PolygonHex(const float32 *xy, int32 count, uint32 rgba) override {
						for (int32 i = 0; i < count * 2; ++i)
							pts.PushBack(xy[i]);
						tailles.PushBack(count);
						couleurs.PushBack(rgba);
						return true;
					}
			};
			char det[400];
			static const char *const kGenres[3] = {"radial", "angulaire", "losange"};
			const float32 W = 160.f, H = 80.f, X0 = 100.f, Y0 = 100.f, RC = 20.f;
			for (uint32 gi = 0; gi < 3u; ++gi) {
				NkUIDocument dG;
				dG.NewDocument("Toile", NkAuthor::Humain);
				dG.nodes[0].layout.kind = NkLayoutKind::Free;
				dG.SetMetric("espacement", 0.f);
				dG.SetMetric("marge", 0.f);
				const int32 f = dG.AddChild(0, "", NkAuthor::Humain);
				NkUINode &nf = dG.nodes[(uint32)f];
				nf.shape = NkString("rect");
				nf.posX = X0;
				nf.posY = Y0;
				nf.width.mode = NkSizeMode::Fixed;
				nf.width.value = W;
				nf.height.mode = NkSizeMode::Fixed;
				nf.height.value = H;
				nf.radius = RC;
				NkRemplissage rf;
				rf.couleur = NkString("#ff0000");
				rf.degrade.type = NkString(kGenres[gi]);
				rf.degrade.angle = 0.f;
				const float32 posS[4] = {0.f, 0.499f, 0.501f, 1.f};
				for (uint32 k = 0; k < 4u; ++k) {
					NkArretDegrade s;
					s.position = posS[k];
					s.couleur = NkString(k < 2u ? "#ff0000" : "#0000ff");
					rf.degrade.arrets.PushBack(s);
				}
				nf.fills.PushBack(rf);
				PeintrePoly64 pp;
				NkLayoutResult lay;
				NkComputeLayout(dG, NkPaintRect{0.f, 0.f, 600.f, 400.f}, lay);
				NkDocumentHost host;
				host.SyncTo(dG);
				const NkComponentInput idle;
				NkDrawDocument(pp, idle, dG, lay, host);
				const NkPaintRect r = lay.At(f);
				const float32 cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f, rx = r.w * 0.5f, ry = r.h * 0.5f;
				// la couleur au point P : le dernier polygone qui le contient (test pair/impair)
				auto couleurEn = [&](float32 px, float32 py, bool &trouve) -> uint32 {
					uint32 c = 0u;
					trouve = false;
					uint32 base = 0u;
					for (uint32 i = 0; i < (uint32)pp.tailles.Size(); ++i) {
						const uint32 n = (uint32)pp.tailles[i];
						bool dedans = false;
						for (uint32 k = 0, j = n - 1u; k < n; j = k++) {
							const float32 xi = pp.pts[(base + k) * 2], yi = pp.pts[(base + k) * 2 + 1];
							const float32 xj = pp.pts[(base + j) * 2], yj = pp.pts[(base + j) * 2 + 1];
							if (((yi > py) != (yj > py)) && (px < (xj - xi) * (py - yi) / (yj - yi + 1e-6f) + xi))
								dedans = !dedans;
						}
						if (dedans) {
							c = pp.couleurs[i];
							trouve = true;
						}
						base += n;
					}
					return c;
				};
				// la formule du genre : t en P
				auto tEn = [&](float32 px, float32 py) -> float32 {
					const float32 dx = px - cx, dy = py - cy;
					if (gi == 0u) {
						const float32 u = dx / rx, v = dy / ry;
						return nkentseu::math::NkSqrt(u * u + v * v);
					}
					if (gi == 2u)
						return (dx < 0.f ? -dx : dx) / rx + (dy < 0.f ? -dy : dy) / ry;
					// angulaire : l'angle horaire depuis l'axe (0 = vers le bas), en tours
					float32 a = nkentseu::math::NkAtan2(-dx, dy) * 57.2957795f;
					while (a < 0.f)
						a += 360.f;
					return a / 360.f;
				};
				// huit points, loin de la coupure (t = 0,5), des bords de bande ET du rayon
				// d'origine de l'angulaire (un point sur une frontiere appartient aux deux bandes)
				const float32 ech[8][2] = {{cx + 3.f, cy + 4.f}, {cx + rx * 0.3f, cy}, {cx, cy - ry * 0.3f}, {cx - rx * 0.35f, cy + 2.f},
										   {cx + rx * 0.8f, cy}, {cx - rx * 0.8f, cy - 2.f}, {cx + 2.f, cy + ry * 0.85f}, {cx + rx * 0.6f, cy - ry * 0.6f}};
				uint32 justes = 0u, vus = 0u;
				char ou[200] = "";
				for (uint32 e = 0; e < 8u; ++e) {
					bool trouve = false;
					const uint32 c = couleurEn(ech[e][0], ech[e][1], trouve);
					const float32 tt = tEn(ech[e][0], ech[e][1]);
					const uint32 attendu = tt < 0.5f ? 0xFF0000FFu : 0x0000FFFFu;
					if (trouve)
						++vus;
					if (trouve && c == attendu)
						++justes;
					else {
						const size_t l = strlen(ou);
						snprintf(ou + l, sizeof(ou) - l, " [%u: t=%.2f vu=%d %08X/%08X]", e, (double)tt, trouve ? 1 : 0, c, attendu);
					}
				}
				// aucun sommet hors de la boite, ni dans le vide du coin arrondi haut-gauche
				uint32 dehors = 0u;
				for (uint32 i = 0; i < (uint32)pp.pts.Size() / 2u; ++i) {
					const float32 x = pp.pts[i * 2], y = pp.pts[i * 2 + 1];
					if (x < r.x - 0.6f || x > r.x + r.w + 0.6f || y < r.y - 0.6f || y > r.y + r.h + 0.6f)
						++dehors;
					else if (x < r.x + RC && y < r.y + RC) {
						const float32 ddx = x - (r.x + RC), ddy = y - (r.y + RC);
						if (ddx * ddx + ddy * ddy > (RC + 0.8f) * (RC + 0.8f))
							++dehors;
					}
				}
				const uint32 nPoly = (uint32)pp.tailles.Size();
				snprintf(det, sizeof(det), "%s : %u polygones, %u/8 points a la couleur attendue (vus %u), %u sommets hors contour%s",
						 kGenres[gi], nPoly, justes, vus, dehors, ou);
				char titre[200];
				snprintf(titre, sizeof(titre), "64%c. %s PEINT par des bandes qui suivent la forme : deux couleurs franches, la couleur en "
											   "chaque point est celle de la formule du genre, rien ne sort du contour arrondi",
						 (char)('a' + gi), gi == 0u ? "LE RADIAL" : (gi == 1u ? "L'ANGULAIRE" : "LE LOSANGE"));
				check(titre, nPoly >= 24u && justes == 8u && dehors == 0u, det);
			}
			// 64d. le nombre de bandes suit la taille : un radial de 400 px a plus de bandes qu'un de 60
			{
				uint32 nb[2] = {0u, 0u};
				const float32 tailles[2] = {60.f, 400.f};
				for (uint32 s = 0; s < 2u; ++s) {
					NkUIDocument dG;
					dG.NewDocument("Toile", NkAuthor::Humain);
					dG.nodes[0].layout.kind = NkLayoutKind::Free;
					dG.SetMetric("espacement", 0.f);
					dG.SetMetric("marge", 0.f);
					const int32 f = dG.AddChild(0, "", NkAuthor::Humain);
					NkUINode &nf = dG.nodes[(uint32)f];
					nf.shape = NkString("rect");
					nf.width.mode = NkSizeMode::Fixed;
					nf.width.value = tailles[s];
					nf.height.mode = NkSizeMode::Fixed;
					nf.height.value = tailles[s];
					NkRemplissage rf;
					rf.couleur = NkString("#ff0000");
					rf.degrade.type = NkString("radial");
					NkArretDegrade s0, s1;
					s0.position = 0.f;
					s0.couleur = NkString("#ff0000");
					s1.position = 1.f;
					s1.couleur = NkString("#0000ff");
					rf.degrade.arrets.PushBack(s0);
					rf.degrade.arrets.PushBack(s1);
					nf.fills.PushBack(rf);
					PeintrePoly64 pp;
					NkLayoutResult lay;
					NkComputeLayout(dG, NkPaintRect{0.f, 0.f, 600.f, 600.f}, lay);
					NkDocumentHost host;
					host.SyncTo(dG);
					const NkComponentInput idle;
					NkDrawDocument(pp, idle, dG, lay, host);
					nb[s] = (uint32)pp.tailles.Size();
				}
				snprintf(det, sizeof(det), "radial 60 px : %u polygones ; 400 px : %u", nb[0], nb[1]);
				check("64d. LE NOMBRE DE BANDES SUIT LA TAILLE, comme le lineaire : un radial de 400 px a plus de bandes qu'un de 60",
					  nb[1] > nb[0] && nb[0] >= 24u, det);
			}
		}
		// ── 65. ① ORIGINE ET RAYONS DANS LE FICHIER : additifs, aller-retour, inconnu preserve ──
		{
			char det[400];
			NkUIDocument dF;
			dF.NewDocument("Toile", NkAuthor::Humain);
			const int32 f = dF.AddChild(0, "", NkAuthor::Humain);
			NkUINode &nf = dF.nodes[(uint32)f];
			nf.shape = NkString("rect");
			NkRemplissage rf;
			rf.couleur = NkString("#ff0000");
			rf.degrade.type = NkString("radial");
			NkArretDegrade s0, s1;
			s0.position = 0.f;
			s0.couleur = NkString("#ff0000");
			s1.position = 1.f;
			s1.couleur = NkString("#0000ff");
			rf.degrade.arrets.PushBack(s0);
			rf.degrade.arrets.PushBack(s1);
			nf.fills.PushBack(rf);
			NkString s;
			dF.Save(s);
			const bool rienParDefaut = strstr(s.Data(), " o=") == nullptr && strstr(s.Data(), " r=") == nullptr;
			nf.fills[0].degrade.origineX = 0.5f;
			nf.fills[0].degrade.origineY = 0.f;
			nf.fills[0].degrade.rayonX = 0.5f;
			nf.fills[0].degrade.rayonY = 1.f;
			nf.fills[0].degrade.inconnus = NkString("zz=42");
			NkString s2;
			dF.Save(s2);
			NkUIDocument relu;
			const bool ok = relu.Load(s2.Data());
			const NkDegrade *g = (ok && relu.IsValidIndex(f) && !relu.nodes[(uint32)f].fills.Empty()) ? &relu.nodes[(uint32)f].fills[0].degrade : nullptr;
			const bool relus = g && g->origineX == 0.5f && g->origineY == 0.f && g->rayonX == 0.5f && g->rayonY == 1.f
							   && g->arrets.Size() == 2u && NkComponentDecl::StrEq(g->inconnus.Data(), "zz=42");
			NkString s3;
			relu.Save(s3);
			const bool identique = NkComponentDecl::StrEq(s2.Data(), s3.Data());
			snprintf(det, sizeof(det), "par defaut : aucun jeton=%d ; ecrit `o=0.5,0` `r=0.5,1` `zz=42` -> relus=%d ; reecrit identique=%d",
					 rienParDefaut ? 1 : 0, relus ? 1 : 0, identique ? 1 : 0);
			check("65a. ① L'ORIGINE ET LES RAYONS font l'aller-retour, additifs (rien au fichier par defaut), un jeton inconnu `zz=42` preserve",
				  rienParDefaut && relus && identique, det);
		}
		// ── 67. ②-2 LES CINQ MODES EXACTS SE PEIGNENT, les treize autres non ─────
		//    A l'enregistreur : un remplissage `multiply` est encadre de PushBlend(Multiply) /
		//    PopBlend ; `screen` de PushBlend(Screen) ; `overlay` (lit la destination) n'a
		//    aucun PushBlend -- il est enregistre et dit, pas approxime ; un `continue` (un
		//    remplissage invisible) ne laisse pas de mode derriere lui (les Push et Pop se
		//    comptent). Le dorsal reel se mesure en pixels avec une fenetre (`--capture`).
		{
			char det[400];
			static const char *const kModes[4] = {"multiply", "screen", "overlay", ""};
			const uint16 kAttendu[4] = {(uint16)NkComponentPaint::NkPaintBlend::Multiply, (uint16)NkComponentPaint::NkPaintBlend::Screen, 0u, 0u};
			bool ok = true;
			for (uint32 m = 0; m < 4u; ++m) {
				NkUIDocument dB;
				dB.NewDocument("Toile", NkAuthor::Humain);
				dB.nodes[0].layout.kind = NkLayoutKind::Free;
				const int32 f = dB.AddChild(0, "", NkAuthor::Humain);
				NkUINode &nf = dB.nodes[(uint32)f];
				nf.shape = NkString("rect");
				nf.width.mode = NkSizeMode::Fixed;
				nf.width.value = 100.f;
				nf.height.mode = NkSizeMode::Fixed;
				nf.height.value = 60.f;
				NkRemplissage r0;
				r0.couleur = NkString("#808080");
				NkRemplissage r1;
				r1.couleur = NkString("#808080");
				r1.fusion = NkString(kModes[m]);
				NkRemplissage rInvisible; // un `continue` dans la boucle : pas de mode oublie
				rInvisible.couleur = NkString("#ff0000");
				rInvisible.fusion = NkString("screen");
				rInvisible.visible = false;
				nf.fills.PushBack(r0);
				nf.fills.PushBack(r1);
				nf.fills.PushBack(rInvisible);
				NkRecordingPaint rec;
				RenderDocument(rec, dB, NkPaintRect{0.f, 0.f, 400.f, 300.f});
				uint32 nPush = 0u, nPop = 0u, fillsSousMode = 0u;
				uint16 modeVu = 0u;
				int32 prof = 0;
				for (uint32 i = 0; i < (uint32)rec.cmds.Size(); ++i) {
					const NkPaintCmd &c = rec.cmds[i];
					if (c.op == NkPaintOp::PushBlend) {
						++nPush;
						++prof;
						modeVu = c.icon;
					} else if (c.op == NkPaintOp::PopBlend) {
						++nPop;
						--prof;
					} else if ((c.op == NkPaintOp::Fill || c.op == NkPaintOp::FillColor) && prof > 0)
						++fillsSousMode;
				}
				const bool attenduPush = kAttendu[m] != 0u;
				const bool bon = (attenduPush ? (nPush == 1u && nPop == 1u && modeVu == kAttendu[m] && fillsSousMode >= 1u)
											  : (nPush == 0u && nPop == 0u))
								 && prof == 0;
				if (!bon)
					ok = false;
				const size_t l = strlen(det);
				if (m == 0u)
					det[0] = '\0';
				snprintf(det + (m == 0u ? 0 : l), sizeof(det) - (m == 0u ? 0 : l), "%s`%s` : Push=%u Pop=%u mode=%u remplissages sous mode=%u",
						 m == 0u ? "" : " ; ", kModes[m][0] ? kModes[m] : "normal", nPush, nPop, (unsigned)modeVu, fillsSousMode);
			}
			check("67. ②-2 LES CINQ MODES EXACTS SE PEIGNENT : `multiply` et `screen` encadres de PushBlend / PopBlend a "
				  "l'enregistreur (le bon mode, un remplissage dessous), `overlay` et normal sans aucun -- et un remplissage "
				  "invisible ne laisse pas de mode derriere lui",
				  ok, det);
		}
		// ── 68. LA VARIABLE DANS L'INTERFACE (§15.14, lot du 05/09) : le bouton du selecteur
		//    cree la variable et la couleur la reference ; le selecteur EDITE la variable
		//    (tout ce qui la reference suit, la reference tient) ; « Detacher » rend le
		//    litteral ; le modele compte / refuse / detache / supprime par UN visiteur ; deux
		//    modes font l'aller-retour fichier et rendent differemment ; le rail Variables
		//    renomme et garde la poubelle. Sans fenetre, comme la main : survol, appui, relache.
		{
			static nkgui::NkGuiContext ctxV;
			char det[480];
			if (!ctxV.Init(600, 900)) {
				check("68. la variable dans l'interface", false, "Init a refuse");
			} else {
				static DesignState stV;
				stV.doc.NewDocument("Toile", NkAuthor::Humain);
				const int32 pg = stV.doc.AddChild(0, "", NkAuthor::Humain);
				stV.doc.nodes[(uint32)pg].shape = NkString("frame");
				stV.doc.nodes[(uint32)pg].layout.kind = NkLayoutKind::Free;
				stV.doc.nodes[(uint32)pg].width.mode = NkSizeMode::Fixed;
				stV.doc.nodes[(uint32)pg].width.value = 400.f;
				stV.doc.nodes[(uint32)pg].height.mode = NkSizeMode::Fixed;
				stV.doc.nodes[(uint32)pg].height.value = 300.f;
				auto rectBleu = [&](float32 y) {
					const int32 r = stV.doc.AddChild(pg, "", NkAuthor::Humain);
					NkUINode &n = stV.doc.nodes[(uint32)r];
					n.shape = NkString("rect");
					n.posX = 10.f;
					n.posY = y;
					n.width.mode = NkSizeMode::Fixed;
					n.width.value = 120.f;
					n.height.mode = NkSizeMode::Fixed;
					n.height.value = 40.f;
					NkRemplissage f1;
					f1.couleur = NkString("#1976d2");
					n.fills.PushBack(f1);
					return r;
				};
				const int32 rc = rectBleu(10.f);
				const int32 rc2 = rectBleu(70.f);
				stV.Recompute(NkPaintRect{0.f, 0.f, 600.f, 900.f});
				stV.SelectSingle(rc);
				static InspectorPanel inspV(&stV);
				static VariablesPanel railV(&stV);
				NkEditorFrameContext ec;
				ec.ui = &ctxV;
				ec.dt = 0.016f;
				auto souris = [&](float32 mx, float32 my, bool bas) {
					ctxV.input.mousePos = {mx, my};
					ctxV.input.mouseDown[0] = bas;
					ctxV.BeginFrame(0.016f);
					ctxV.BeginLayout({340.f, 0.f, 260.f, 900.f});
					inspV.OnUI(ec);
					NkDessinerPickerDemande(ctxV, stV);
					ctxV.EndFrame();
				};
				auto cliquer = [&](float32 x, float32 y) {
					souris(x, y, false); // le survol precede l'appui (le kit resout le survol a l'image d'avant)
					souris(x, y, true);
					souris(x, y, false);
					souris(-1.f, -1.f, false);
				};
				auto boite = [&](float32 &px, float32 &py) {
					px = 1e9f;
					py = 1e9f;
					for (uint32 i = 0; i < (uint32)ctxV.dlOverlay.vtx.Size(); ++i) {
						if (ctxV.dlOverlay.vtx[i].pos.x < px) px = ctxV.dlOverlay.vtx[i].pos.x;
						if (ctxV.dlOverlay.vtx[i].pos.y < py) py = ctxV.dlOverlay.vtx[i].pos.y;
					}
				};
				auto ouvrir = [&](int32 noeud) {
					stV.picker = DesignState::DemandePicker();
					stV.picker.ouvert = true;
					stV.picker.id = ctxV.GetId("##sonde.popover.variable");
					stV.picker.genre = 1u;
					stV.picker.noeud = noeud;
					stV.picker.index = 0;
					stV.picker.arretSel = 0;
					stV.picker.ancre = {360.f, 200.f, 16.f, 16.f};
					souris(-1.f, -1.f, false);
					souris(-1.f, -1.f, false);
				};
				auto fermer = [&]() {
					// comme la main : le popup du kit se ferme AVANT que la demande s'efface --
					// sinon `DessinerPopoverRemplissage` ne le ferme pas (il ne le fait que si
					// `d.ouvert` est encore vrai), `popupDepth` reste a 1 et le kit consomme le
					// prochain clic pour fermer le popup (c'est ce qui a mange le premier clic
					// du rail a la premiere course)
					if (ctxV.popupDepth > 0)
						ctxV.ClosePopup();
					stV.picker = DesignState::DemandePicker();
					souris(-1.f, -1.f, false);
					souris(-1.f, -1.f, false);
				};
				auto couleurPeinte = [&](NkUIDocument &d, int32 noeud) -> uint32 {
					NkPaintRect sfc;
					sfc.x = 0.f;
					sfc.y = 0.f;
					sfc.w = 600.f;
					sfc.h = 900.f;
					NkLayoutResult lay;
					NkComputeLayout(d, sfc, lay);
					const NkPaintRect r = lay.At(noeud);
					NkVector<uint8> masques;
					for (uint32 i = 0; i < (uint32)d.nodes.Size(); ++i) {
						masques.PushBack(d.nodes[i].masque ? 1u : 0u);
						if (i != 0u && (int32)i != noeud && (int32)i != pg)
							d.nodes[i].masque = true;
					}
					NkRecordingPaint rec;
					RenderDocument(rec, d, sfc);
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
				// la geometrie du popover (DessinerPopoverRemplissage) : 8 de marge, la rangee des
				// types (26), le selecteur (168), la rangee modele (26), puis LA RANGEE DE LA
				// VARIABLE a +3 (20 px de haut) ; largeur 250, x0 = +8, x1 = +250-8
				// ③ (05/09, apres-midi) : la RANGEE D'OPACITE s'est glissee entre la rangee des
				//    valeurs et celle de la variable -- 26 px de plus avant la variable.
				const float32 kYRangeeVar = 0.5f + 8.f + 26.f + 168.f + 26.f + 26.f + 3.f + 10.f;
				// ── 68a. le bouton « Creer une variable de couleur » ──
				const uint32 avantA = couleurPeinte(stV.doc, rc);
				ouvrir(rc);
				float32 px = 0.f, py = 0.f;
				boite(px, py);
				cliquer(px + 0.5f + 8.f + 100.f, py + kYRangeeVar);
				const uint32 nVar = (uint32)stV.doc.variables.Size();
				const NkString refA = stV.doc.nodes[(uint32)rc].fills[0].couleur;
				const bool refOk = nVar == 1u && NkComponentDecl::StrEq(refA.Data(), "@couleur_1")
								   && NkComponentDecl::StrEq(stV.doc.variables[0].valeur.Data(), "#1976d2")
								   && NkComponentDecl::StrEq(stV.doc.variables[0].nom.Data(), "Couleur 1");
				const uint32 apresA = couleurPeinte(stV.doc, rc);
				const bool piedA = stV.status.Data() && strstr(stV.status.Data(), "Couleur 1") != nullptr;
				snprintf(det, sizeof(det), "variables %u, remplissage -> %s, valeur %s, nom « %s », peint %08X -> %08X (inchange), pied=%d",
						 nVar, refA.Data() ? refA.Data() : "?", nVar ? stV.doc.variables[0].valeur.Data() : "-",
						 nVar ? stV.doc.variables[0].nom.Data() : "-", avantA, apresA, piedA ? 1 : 0);
				check("68a. « CREER UNE VARIABLE DE COULEUR » sous la rangee modele (la place de Lunacy) : un clic cree "
					  "« Couleur 1 » = la couleur courante, le remplissage la REFERENCE (« @couleur_1 »), rien ne change a l'ecran, "
					  "le pied le dit",
					  refOk && apresA == avantA && piedA, det);
				// ── 68b. le selecteur EDITE la variable : tout ce qui la reference suit, la reference tient ──
				stV.doc.nodes[(uint32)rc2].fills[0].couleur = NkString("@couleur_1");
				souris(-1.f, -1.f, false);
				boite(px, py);
				// ③ DEPUIS LE 05/09, LE SELECTEUR DETACHE PAR DEFAUT (decision de Rodolf : lier ne doit
				//    pas dire modifier). Pour editer LA VARIABLE, on arme « Modifier la variable (xN) »,
				//    la rangee juste sous celle de la variable. Le detachement par defaut a sa propre
				//    sonde (87).
				cliquer(px + 0.5f + 8.f + 117.f, py + kYRangeeVar + 24.f);
				boite(px, py);
				cliquer(px + 0.5f + 8.f + 80.f, py + 0.5f + 8.f + 26.f + 80.f); // le carre SV, comme 60h
				const NkString valB = stV.doc.variables.Empty() ? NkString("(aucune)") : stV.doc.variables[0].valeur;
				const bool varChangee = NkHexLisible(valB.Data()) && !NkComponentDecl::StrEq(valB.Data(), "#1976d2");
				const bool refTient = NkComponentDecl::StrEq(stV.doc.nodes[(uint32)rc].fills[0].couleur.Data(), "@couleur_1")
									  && NkComponentDecl::StrEq(stV.doc.nodes[(uint32)rc2].fills[0].couleur.Data(), "@couleur_1");
				const uint32 pB1 = couleurPeinte(stV.doc, rc), pB2 = couleurPeinte(stV.doc, rc2);
				const uint32 attenduB = varChangee ? renderdetail::NkGCouleur(valB.Data()) : 0u;
				snprintf(det, sizeof(det), "variable #1976d2 -> %s, references « %s » / « %s », peints %08X et %08X (attendu %08X)",
						 valB.Data(), stV.doc.nodes[(uint32)rc].fills[0].couleur.Data(), stV.doc.nodes[(uint32)rc2].fills[0].couleur.Data(),
						 pB1, pB2, attenduB);
				check("68b. LE SELECTEUR EDITE LA VARIABLE UNE FOIS « Modifier la variable » ARME (③, 05/09) : un clic dans le carre SV change "
					  "la VALEUR de « Couleur 1 » (pas la reference), et les DEUX rectangles qui la referencent suivent",
					  varChangee && refTient && pB1 == attenduB && pB2 == attenduB, det);
				// ── 68c. « Detacher » : le litteral que l'oeil voyait, l'autre reference tient ──
				boite(px, py);
				cliquer(px + 0.5f + 250.f - 8.f - 31.f, py + kYRangeeVar);
				const NkString cC = stV.doc.nodes[(uint32)rc].fills[0].couleur;
				const bool detacheOk = NkComponentDecl::StrEq(cC.Data(), valB.Data())
									   && NkComponentDecl::StrEq(stV.doc.nodes[(uint32)rc2].fills[0].couleur.Data(), "@couleur_1")
									   && stV.doc.variables.Size() == 1u && stV.doc.CompterUsagesVariable("couleur_1") == 1u;
				fermer();
				snprintf(det, sizeof(det), "remplissage -> %s (variable %s), l'autre -> %s, usages %u, variables %u", cC.Data(), valB.Data(),
						 stV.doc.nodes[(uint32)rc2].fills[0].couleur.Data(), stV.doc.CompterUsagesVariable("couleur_1"),
						 (uint32)stV.doc.variables.Size());
				check("68c. « DETACHER » rend au remplissage le LITTERAL que l'oeil voyait ; la variable reste, l'autre "
					  "reference tient, le compte d'usages passe a 1",
					  detacheOk, det);
				// ── 68d. le modele : UN visiteur pour compter, refuser, detacher, supprimer ──
				{
					NkUIDocument dM;
					dM.NewDocument("Toile", NkAuthor::Humain);
					NkVariable acc;
					acc.cle = NkString("accent");
					acc.valeur = NkString("#123456");
					dM.variables.PushBack(acc);
					const int32 n1 = dM.AddChild(0, "", NkAuthor::Humain);
					NkUINode &n = dM.nodes[(uint32)n1];
					n.shape = NkString("rect");
					n.fill = NkString("@accent");
					n.textColor = NkString("@accent");
					NkRemplissage f;
					f.couleur = NkString("@accent");
					f.degrade.type = NkString("lineaire");
					NkArretDegrade a0, a1;
					a0.position = 0.f;
					a0.couleur = NkString("@accent");
					a1.position = 1.f;
					a1.couleur = NkString("#ffffff");
					f.degrade.arrets.PushBack(a0);
					f.degrade.arrets.PushBack(a1);
					n.fills.PushBack(f);
					NkBordure b;
					b.couleur = NkString("@accent");
					n.borders.PushBack(b);
					NkEffet e;
					e.couleur = NkString("@accent");
					n.effets.PushBack(e);
					NkBlocEtat(n, "Hover").fond = NkString("@accent");
					NkDeclarationComposant dc; // un composant dont la racine reference la variable EST un usage
					NkUINode racine;
					racine.fill = NkString("@accent");
					dc.arbre.PushBack(racine);
					dM.declarations.PushBack(dc);
					const uint32 usages = dM.CompterUsagesVariable("accent");
					uint32 uRefus = 0u;
					const bool refuse = !dM.SupprimerVariable("@accent", &uRefus) && dM.variables.Size() == 1u;
					const uint32 detaches = dM.DetacherVariable("accent");
					const uint32 apres = dM.CompterUsagesVariable("accent");
					const NkUINode &m = dM.nodes[(uint32)n1];
					const bool litteraux = NkComponentDecl::StrEq(m.fill.Data(), "#123456") && NkComponentDecl::StrEq(m.textColor.Data(), "#123456")
										   && NkComponentDecl::StrEq(m.fills[0].couleur.Data(), "#123456")
										   && NkComponentDecl::StrEq(m.fills[0].degrade.arrets[0].couleur.Data(), "#123456")
										   && NkComponentDecl::StrEq(m.fills[0].degrade.arrets[1].couleur.Data(), "#ffffff")
										   && NkComponentDecl::StrEq(m.borders[0].couleur.Data(), "#123456")
										   && NkComponentDecl::StrEq(m.effets[0].couleur.Data(), "#123456")
										   && NkComponentDecl::StrEq(m.apparences[0].fond.Data(), "#123456")
										   && NkComponentDecl::StrEq(dM.declarations[0].arbre[0].fill.Data(), "#123456");
					const bool supprime = dM.SupprimerVariable("accent") && dM.variables.Empty();
					snprintf(det, sizeof(det), "usages %u (attendu 8 : fond, texte, remplissage, arret, bordure, effet, etat, composant) ; "
											   "suppression refusee=%d (dit %u) ; detaches %u, usages apres %u, litteraux=%d ; supprimee ensuite=%d",
							 usages, refuse ? 1 : 0, uRefus, detaches, apres, litteraux ? 1 : 0, supprime ? 1 : 0);
					check("68d. LE MODELE, PAR UN SEUL VISITEUR : huit usages comptes (cle simple, texte, remplissage, arret, bordure, "
						  "effet, etat, composant), la suppression REFUSEE tant qu'elle est utilisee (le nombre dit), le detachement "
						  "rend huit litteraux et zero usage, puis la suppression passe",
						  usages == 8u && refuse && uRefus == 8u && detaches == 8u && apres == 0u && litteraux && supprime, det);
				}
				// ── 68e. DEUX MODES : aller-retour fichier, rendu different selon le mode courant, PoserValeur ──
				{
					NkUIDocument dMo;
					dMo.NewDocument("Toile", NkAuthor::Humain);
					NkVariable fond;
					fond.cle = NkString("fond");
					fond.nom = NkString("Fond");
					fond.valeur = NkString("#777777");
					NkValeurMode clair, sombre;
					clair.mode = NkString("clair");
					clair.valeur = NkString("#ffffff");
					sombre.mode = NkString("sombre");
					sombre.valeur = NkString("#000000");
					fond.parMode.PushBack(clair);
					fond.parMode.PushBack(sombre);
					dMo.variables.PushBack(fond);
					dMo.modeCourant = NkString("sombre");
					const int32 r = dMo.AddChild(0, "", NkAuthor::Humain);
					dMo.nodes[(uint32)r].shape = NkString("rect");
					dMo.nodes[(uint32)r].width.mode = NkSizeMode::Fixed;
					dMo.nodes[(uint32)r].width.value = 40.f;
					dMo.nodes[(uint32)r].height.mode = NkSizeMode::Fixed;
					dMo.nodes[(uint32)r].height.value = 40.f;
					NkRemplissage f;
					f.couleur = NkString("@fond");
					dMo.nodes[(uint32)r].fills.PushBack(f);
					auto peint = [&](NkUIDocument &d) -> uint32 {
						NkRecordingPaint rec;
						RenderDocument(rec, d, NkPaintRect{0.f, 0.f, 600.f, 400.f});
						uint32 dernier = 0u;
						for (uint32 i = 0; i < (uint32)rec.cmds.Size(); ++i)
							if (rec.cmds[i].op == NkPaintOp::FillColor && rec.cmds[i].w == 40.f && rec.cmds[i].h == 40.f)
								dernier = rec.cmds[i].rgba;
						return dernier;
					};
					const uint32 pSombre = peint(dMo);
					NkString texte;
					dMo.Save(texte);
					NkUIDocument relu;
					const bool ok = relu.Load(texte.Data());
					const bool modeRelu = ok && NkComponentDecl::StrEq(relu.modeCourant.Data(), "sombre")
										  && strstr(texte.Data(), "@clair=#ffffff @sombre=#000000") != nullptr;
					const uint32 pRelu = ok ? peint(relu) : 0u;
					const NkString modeLu = ok ? relu.modeCourant : NkString("?");
					if (ok)
						relu.modeCourant = NkString("clair");
					const uint32 pClair = ok ? peint(relu) : 0u;
					if (ok)
						relu.modeCourant = NkString();
					const uint32 pDefaut = ok ? peint(relu) : 0u;
					// PoserValeur ecrit DANS le mode declare, et dans le defaut sinon
					dMo.variables[0].PoserValeur("sombre", "#111111");
					dMo.variables[0].PoserValeur("inconnu", "#222222");
					const bool pose = NkComponentDecl::StrEq(dMo.variables[0].ValeurPour("sombre"), "#111111")
									  && NkComponentDecl::StrEq(dMo.variables[0].ValeurPour("clair"), "#ffffff")
									  && NkComponentDecl::StrEq(dMo.variables[0].valeur.Data(), "#222222");
					snprintf(det, sizeof(det), "mode sombre -> %08X ; relu (mode %s) -> %08X ; clair -> %08X ; defaut -> %08X ; PoserValeur : sombre %s, clair %s, defaut %s",
							 pSombre, modeLu.Data() ? modeLu.Data() : "?", pRelu, pClair, pDefaut, dMo.variables[0].ValeurPour("sombre"),
							 dMo.variables[0].ValeurPour("clair"), dMo.variables[0].valeur.Data());
					check("68e. DEUX MODES (clair / sombre) : le document se reenregistre avec ses deux valeurs et son mode courant, "
						  "se relit, et RENDS DIFFEREMMENT selon le mode (noir en sombre, blanc en clair, le defaut sans mode) ; "
						  "PoserValeur ecrit dans le mode declare, dans le defaut sinon",
						  pSombre == 0x000000FFu && modeRelu && pRelu == 0x000000FFu && pClair == 0xFFFFFFFFu && pDefaut == 0x777777FFu && pose,
						  det);
				}
				// ── 68f. LE RAIL « VARIABLES » : la liste, la poubelle GARDEE, le renommage sur place ──
				{
					auto rail = [&](float32 mx, float32 my, bool bas, uint32 cp) {
						ctxV.input.mousePos = {mx, my};
						ctxV.input.mouseDown[0] = bas;
						ctxV.BeginFrame(0.016f);
						if (cp)
							ctxV.input.PushChar(cp); // APRES BeginFrame : l'effacement est dans EndFrame
						ctxV.BeginLayout({0.f, 0.f, 260.f, 900.f});
						railV.OnUI(ec);
						ctxV.EndFrame();
					};
					auto clicRail = [&](const nkgui::NkRect &r) {
						const float32 x = r.x + r.w * 0.5f, y = r.y + r.h * 0.5f;
						rail(x, y, false, 0u);
						rail(x, y, true, 0u);
						rail(x, y, false, 0u);
						rail(-1.f, -1.f, false, 0u);
					};
					rail(-1.f, -1.f, false, 0u);
					const nkgui::NkRect rNom = railV.RectNom(0), rPou = railV.RectPoubelle(0);
					const bool rects = rNom.w > 0.f && rPou.w > 0.f && rPou.x > rNom.x;
					// la poubelle, gardee : « Couleur 1 » est encore referencee par rc2
					stV.status = NkString();
					const int32 profondeurAvant = ctxV.popupDepth;
					clicRail(rPou);
					const NkString statusGarde = stV.status;
					const bool garde = stV.doc.variables.Size() == 1u && statusGarde.Data()
									   && strstr(statusGarde.Data(), "utilisée par 1 remplissages") != nullptr;
					// le renommage : clic sur le nom, une frappe, la variable porte le caractere
					clicRail(rNom);
					const bool enRenommage = railV.EnRenommage() == 0;
					rail(-1.f, -1.f, false, (uint32)'!');
					rail(-1.f, -1.f, false, 0u);
					const NkString nomR = stV.doc.variables.Empty() ? NkString("(aucune)") : stV.doc.variables[0].nom;
					const bool renomme = nomR.Data() && strstr(nomR.Data(), "!") != nullptr && strstr(nomR.Data(), "Couleur") != nullptr;
					clicRail(nkgui::NkRect{100.f, 850.f, 4.f, 4.f}); // un clic ailleurs termine le renommage
					const bool fini = railV.EnRenommage() == -1;
					// detachee (plus aucun usage), la poubelle supprime -- et le dit
					const uint32 detaches = stV.doc.DetacherVariable("couleur_1");
					stV.status = NkString();
					rail(-1.f, -1.f, false, 0u);
					clicRail(railV.RectPoubelle(0));
					const bool supprimee = stV.doc.variables.Empty() && stV.status.Data() && strstr(stV.status.Data(), "supprimée") != nullptr;
					snprintf(det, sizeof(det), "rects=%d ; popups ouverts avant %d ; poubelle gardee=%d (« %s ») ; renommage : ouvert=%d, nom -> « %s », fini=%d ; detaches %u, supprimee=%d",
							 rects ? 1 : 0, profondeurAvant, garde ? 1 : 0, statusGarde.Data() ? statusGarde.Data() : "", enRenommage ? 1 : 0,
							 nomR.Data() ? nomR.Data() : "?", fini ? 1 : 0, detaches, supprimee ? 1 : 0);
					check("68f. LE RAIL « VARIABLES » : la ligne (nom, poubelle) se dessine ; la poubelle REFUSE tant que la variable "
						  "est utilisee (« utilisee par N remplissages ») ; un clic sur le nom ouvre le renommage, une frappe l'ecrit, "
						  "un clic ailleurs le termine ; detachee, la poubelle supprime et le dit",
						  rects && garde && enRenommage && renomme && fini && detaches == 1u && supprimee, det);
				}
				// ── 68g. LIER UNE VARIABLE EXISTANTE depuis le selecteur : « Lier ˅ » a droite de
				//    « Creer une variable », la liste depliee DANS le popover, un clic fait de
				//    la couleur courante une reference
				{
					const int32 vp = stV.doc.CreerVariableCouleur("#0a555f", "Pétrole");
					stV.SelectSingle(rc); // rc porte un litteral depuis 68c
					ouvrir(rc);
					boite(px, py);
					const uint32 avantG = couleurPeinte(stV.doc, rc);
					cliquer(px + 0.5f + 250.f - 8.f - 30.f, py + kYRangeeVar); // « Lier ˅ »
					boite(px, py);
					const float32 hAvant = -1e9f;
					(void)hAvant;
					// ③ la liste porte maintenant un champ de RECHERCHE en tete (05/09) : la premiere
					//    variable est une rangee plus bas (+20)
					cliquer(px + 0.5f + 8.f + 100.f, py + kYRangeeVar + 44.f);
					const NkString refG = stV.doc.nodes[(uint32)rc].fills[0].couleur;
					NkString attendu("@");
					attendu.Append(stV.doc.variables[(uint32)vp].cle);
					const uint32 apresG = couleurPeinte(stV.doc, rc);
					const bool piedG = stV.status.Data() && strstr(stV.status.Data(), "Pétrole") != nullptr;
					fermer();
					snprintf(det, sizeof(det), "variable « Pétrole » (%s) ; remplissage %08X -> « %s » (attendu « %s »), peint %08X (0A555FFF), pied=%d",
							 stV.doc.variables[(uint32)vp].cle.Data(), avantG, refG.Data() ? refG.Data() : "?", attendu.Data(), apresG, piedG ? 1 : 0);
					check("68g. LIER UNE VARIABLE EXISTANTE depuis le selecteur : « Lier ˅ » deplie la liste des variables DANS le "
						  "popover, un clic sur « Pétrole » fait du remplissage une reference et il rend la couleur de la variable",
						  vp >= 0 && NkComponentDecl::StrEq(refG.Data(), attendu.Data()) && apresG == 0x0A555FFFu && avantG != apresG && piedG, det);
				}
			}
		}
		// ── 69. LES STYLES DE CALQUE ET DE TEXTE (§15.15, lot du 05/09) : un nom pour un
		//    ensemble ; le MEME mecanisme que les instances (copie a la modification sous
		//    les bits d'ecart) ; le fichier par les memes ecrivains et lecteurs que le noeud ;
		//    un style absent est DIT, jamais un rendu vide ; le rail ; la rangee de section.
		{
			static nkgui::NkGuiContext ctxS;
			char det[560];
			if (!ctxS.Init(600, 900)) {
				check("69. les styles", false, "Init a refuse");
			} else {
				static DesignState stS;
				NkUIDocument &dS = stS.doc;
				dS.NewDocument("Toile", NkAuthor::Humain);
				const int32 pg = dS.AddChild(0, "", NkAuthor::Humain);
				dS.nodes[(uint32)pg].shape = NkString("frame");
				dS.nodes[(uint32)pg].layout.kind = NkLayoutKind::Free;
				dS.nodes[(uint32)pg].width.mode = NkSizeMode::Fixed;
				dS.nodes[(uint32)pg].width.value = 400.f;
				dS.nodes[(uint32)pg].height.mode = NkSizeMode::Fixed;
				dS.nodes[(uint32)pg].height.value = 300.f;
				auto rectDe = [&](float32 y, const char *couleur) {
					const int32 r = dS.AddChild(pg, "", NkAuthor::Humain);
					NkUINode &n = dS.nodes[(uint32)r];
					n.shape = NkString("rect");
					n.posX = 10.f;
					n.posY = y;
					n.width.mode = NkSizeMode::Fixed;
					n.width.value = 100.f;
					n.height.mode = NkSizeMode::Fixed;
					n.height.value = 40.f;
					NkRemplissage f;
					f.couleur = NkString(couleur);
					n.fills.PushBack(f);
					return r;
				};
				auto texteDe = [&](float32 y) {
					const int32 r = dS.AddChild(pg, "", NkAuthor::Humain);
					NkUINode &n = dS.nodes[(uint32)r];
					n.shape = NkString("text");
					n.text = NkString("Titre");
					n.posX = 200.f;
					n.posY = y;
					n.fontPx = 12.f;
					return r;
				};
				auto couleurPeinte = [&](int32 noeud) -> uint32 {
					NkPaintRect sfc;
					sfc.x = 0.f;
					sfc.y = 0.f;
					sfc.w = 600.f;
					sfc.h = 900.f;
					NkLayoutResult lay;
					NkComputeLayout(dS, sfc, lay);
					const NkPaintRect r = lay.At(noeud);
					NkVector<uint8> masques;
					for (uint32 i = 0; i < (uint32)dS.nodes.Size(); ++i) {
						masques.PushBack(dS.nodes[i].masque ? 1u : 0u);
						if (i != 0u && (int32)i != noeud && (int32)i != pg)
							dS.nodes[i].masque = true;
					}
					NkRecordingPaint rec;
					RenderDocument(rec, dS, sfc);
					for (uint32 i = 0; i < (uint32)dS.nodes.Size(); ++i)
						dS.nodes[i].masque = masques[i] != 0u;
					uint32 dernier = 0u;
					for (uint32 i = 0; i < (uint32)rec.cmds.Size(); ++i) {
						const NkPaintCmd &c = rec.cmds[i];
						if (c.op == NkPaintOp::FillColor && c.x == r.x && c.y == r.y && c.w == r.w && c.h == r.h)
							dernier = c.rgba;
					}
					return dernier;
				};
				NkVariable enc;
				enc.cle = NkString("encre");
				enc.valeur = NkString("#112233");
				dS.variables.PushBack(enc);
				// ── 69a. modele + format : creer depuis un calque, lier un second, ecrire, relire ──
				const int32 r1 = rectDe(10.f, "#1976d2");
				const int32 r2 = rectDe(60.f, "#aaaaaa");
				{
					NkBordure b;
					b.couleur = NkString("#30363d");
					b.epaisseur = 2.f;
					dS.nodes[(uint32)r1].borders.PushBack(b);
					NkEffet e;
					e.couleur = NkString("#000000");
					dS.nodes[(uint32)r1].effets.PushBack(e);
				}
				const int32 si = dS.CreerStyleDepuis(r1, false, "Bouton");
				const bool lie2 = dS.LierStyle(r2, "calque_1");
				const int32 t1 = texteDe(10.f), t2 = texteDe(60.f);
				dS.nodes[(uint32)t1].fontPx = 24.f;
				dS.nodes[(uint32)t1].fontWeight = 700.f;
				dS.nodes[(uint32)t1].textColor = NkString("@encre");
				const int32 ti = dS.CreerStyleDepuis(t1, true, "Titre");
				const bool lieT = dS.LierStyle(t2, "texte_1");
				const bool creeOk = si == 0 && ti == 1 && NkComponentDecl::StrEq(dS.styles[0].cle.Data(), "calque_1")
									&& NkComponentDecl::StrEq(dS.styles[0].nom.Data(), "Bouton")
									&& NkComponentDecl::StrEq(dS.nodes[(uint32)r1].styleCalque.Data(), "calque_1") && lie2
									&& NkComponentDecl::StrEq(dS.nodes[(uint32)r2].styleCalque.Data(), "calque_1")
									&& NkUIDocument::MemeEmpreinte(dS.nodes[(uint32)r1], dS.nodes[(uint32)r2], NkUINode::EcartRemplissages)
									&& dS.nodes[(uint32)r2].borders.Size() == 1u && dS.nodes[(uint32)r2].effets.Size() == 1u
									&& couleurPeinte(r2) == 0x1976D2FFu && lieT && dS.nodes[(uint32)t2].fontPx == 24.f
									&& dS.nodes[(uint32)t2].fontWeight == 700.f && NkComponentDecl::StrEq(dS.nodes[(uint32)t2].textColor.Data(), "@encre");
				NkString s1;
				dS.Save(s1);
				const char *txt = s1.Data() ? s1.Data() : "";
				auto compte = [](const char *h, const char *m) {
					uint32 c = 0u;
					for (const char *p = strstr(h, m); p; p = strstr(p + 1, m))
						++c;
					return c;
				};
				const bool ecrit = strstr(txt, "\nstyle = calque_1 genre=calque nom=\"Bouton\"\n  fond_1 = #1976d2 100 1\n  bord_1 = #30363d") != nullptr
								   && strstr(txt, "\n  effet_1 = ") != nullptr
								   && strstr(txt, "\nstyle = texte_1 genre=texte nom=\"Titre\"\n  police_px = 24\n  graisse = 700\n  couleur_texte = @encre\n") != nullptr
								   && compte(txt, "  style_calque = calque_1\n") == 2u && compte(txt, "  style_texte = texte_1\n") == 2u;
				// l'inconnu sur la ligne `style`, preserve ; l'aller-retour identique ; sans style, aucune ligne
				NkString s2;
				{
					const char *pos = strstr(txt, "nom=\"Bouton\"");
					const uint32 coupe = pos ? (uint32)(pos - txt) + 12u : 0u;
					for (uint32 i = 0; i < coupe; ++i)
						s2.Append(txt[i]);
					s2.Append(" futur=x");
					s2.Append(txt + coupe);
				}
				NkUIDocument relu;
				const bool ok = relu.Load(s2.Data());
				const NkStyle *rs = ok ? relu.TrouverStyle("calque_1") : nullptr;
				const bool relus = rs && NkComponentDecl::StrEq(rs->nom.Data(), "Bouton") && NkComponentDecl::StrEq(rs->inconnus.Data(), "futur=x")
								   && NkUIDocument::MemeEmpreinte(rs->apparence, dS.styles[0].apparence, NkUINode::EcartRemplissages)
								   && NkUIDocument::MemeEmpreinte(rs->apparence, dS.styles[0].apparence, NkUINode::EcartBordures)
								   && NkUIDocument::MemeEmpreinte(rs->apparence, dS.styles[0].apparence, NkUINode::EcartEffets)
								   && relu.TrouverStyle("texte_1") && relu.TrouverStyle("texte_1")->apparence.fontPx == 24.f
								   && relu.IsValidIndex(r2) && NkComponentDecl::StrEq(relu.nodes[(uint32)r2].styleCalque.Data(), "calque_1")
								   && relu.nodes.Size() == dS.nodes.Size();
				NkString s3;
				if (ok)
					relu.Save(s3);
				const bool reemis = ok && strstr(s3.Data(), "futur=x") != nullptr;
				NkUIDocument relu2;
				NkString s4;
				const bool identique = relu2.Load(s1.Data()) && (relu2.Save(s4), NkComponentDecl::StrEq(s4.Data(), s1.Data()));
				NkUIDocument dSans;
				dSans.NewDocument("Toile", NkAuthor::Humain);
				NkString sS;
				dSans.Save(sS);
				const bool additif = strstr(sS.Data(), "style") == nullptr;
				snprintf(det, sizeof(det), "crees=%d (calque_1 « Bouton », texte_1 « Titre », r2 lie : mêmes remplissages, bordure, effet, peint 1976D2) ; "
										   "ecrit=%d ; relu=%d (inconnu preserve) ; reemis=%d ; aller-retour identique=%d ; sans style aucune ligne=%d",
						 creeOk ? 1 : 0, ecrit ? 1 : 0, relus ? 1 : 0, reemis ? 1 : 0, identique ? 1 : 0, additif ? 1 : 0);
				check("69a. LE MODELE ET LE FORMAT : un style de calque cree depuis un rectangle (remplissages + bordures + effets), "
					  "un second rectangle le lie et prend l'ensemble ; un style de texte (taille, graisse, couleur « @encre ») ; "
					  "`style = <cle> genre=... nom=\"...\"` puis fond_i / bord_i / effet_i / police_px par LES MEMES ecrivains, "
					  "`style_calque` / `style_texte` sur les noeuds ; relu par les memes lecteurs, inconnu preserve, aller-retour "
					  "octet pour octet, un document sans style ne gagne aucune ligne",
					  creeOk && ecrit && relus && reemis && identique && additif, det);
				// ── 69b. PROPAGATION, SURCHARGE, REINITIALISATION (Q51, les bits des instances) ──
				NkStyle *st = dS.TrouverStyleMut("calque_1");
				st->apparence.fills[0].couleur = NkString("#ff0000");
				const int32 touches1 = dS.PropagerStyle("calque_1");
				const bool suivent = touches1 == 2 && couleurPeinte(r1) == 0xFF0000FFu && couleurPeinte(r2) == 0xFF0000FFu;
				dS.nodes[(uint32)r1].fills[0].couleur = NkString("#00ff00"); // la main ecrit sur r1
				dS.MarkHumanEdit(r1);
				const bool ecartLeve = dS.nodes[(uint32)r1].Surcharge(NkUINode::EcartRemplissages)
									   && !dS.nodes[(uint32)r1].Surcharge(NkUINode::EcartBordures);
				st->apparence.fills[0].couleur = NkString("#0000ff");
				st->apparence.borders[0].epaisseur = 5.f;
				const int32 touches2 = dS.PropagerStyle("calque_1");
				const bool tient = touches2 == 2 && couleurPeinte(r1) == 0x00FF00FFu && couleurPeinte(r2) == 0x0000FFFFu
								   && dS.nodes[(uint32)r1].borders[0].epaisseur == 5.f; // la bordure, non surchargee, suit
				const bool reinit = dS.ReinitialiserEcartStyle(r1, NkUINode::EcartRemplissages) && couleurPeinte(r1) == 0x0000FFFFu
									&& !dS.nodes[(uint32)r1].Surcharge(NkUINode::EcartRemplissages);
				snprintf(det, sizeof(det), "rouge -> %d touches, peints %08X / %08X ; r1 ecrit vert -> ecart remplissages=%d (bordures non) ; "
										   "bleu + bordure 5 -> %d touches, r1 %08X (tient) r2 %08X (suit), bordure r1 %.0f ; reinit -> %d",
						 touches1, couleurPeinte(r1), couleurPeinte(r2), ecartLeve ? 1 : 0, touches2, couleurPeinte(r1), couleurPeinte(r2),
						 dS.nodes[(uint32)r1].borders[0].epaisseur, reinit ? 1 : 0);
				check("69b. LA PROPAGATION (Q51, les bits des instances) : modifier le style change les deux rectangles ; une couleur "
					  "posee a la main sur l'un leve SON bit (pas celui des bordures) et tient a la propagation suivante pendant que "
					  "l'autre suit et que sa bordure suit ; « Reinitialiser » rend la main au style",
					  suivent && ecartLeve && tient && reinit, det);
				// ── 69c. LE STYLE DE TEXTE, et la variable DANS le style ──
				NkStyle *tt = dS.TrouverStyleMut("texte_1");
				tt->apparence.fontPx = 30.f;
				const int32 touchesT = dS.PropagerStyle("texte_1");
				const bool texteSuit = touchesT == 2 && dS.nodes[(uint32)t1].fontPx == 30.f && dS.nodes[(uint32)t2].fontPx == 30.f;
				dS.nodes[(uint32)t1].fontWeight = 400.f;
				dS.MarkHumanEdit(t1);
				tt->apparence.fontPx = 36.f;
				dS.PropagerStyle("texte_1");
				const bool texteTient = dS.nodes[(uint32)t1].Surcharge(NkUINode::EcartTexte) && dS.nodes[(uint32)t1].fontPx == 30.f
										&& dS.nodes[(uint32)t2].fontPx == 36.f;
				const uint32 usagesEncre = dS.CompterUsagesVariable("encre");
				dS.variables[0].valeur = NkString("#445566");
				renderdetail::NkPoserResolveur(&dS);
				const uint32 encreResolue = renderdetail::NkGCouleur(dS.nodes[(uint32)t2].textColor.Data());
				snprintf(det, sizeof(det), "taille 30 -> %d touches (t1 %.0f, t2 %.0f) ; t1 graisse 400 a la main puis style 36 -> t1 %.0f (tient, ecart texte=%d), t2 %.0f ; "
										   "« @encre » : %u usages (t1, t2, le style) ; variable -> #445566 : le texte lie rend %08X",
						 touchesT, dS.nodes[(uint32)t1].fontPx, dS.nodes[(uint32)t2].fontPx, dS.nodes[(uint32)t1].fontPx,
						 dS.nodes[(uint32)t1].Surcharge(NkUINode::EcartTexte) ? 1 : 0, dS.nodes[(uint32)t2].fontPx, usagesEncre, encreResolue);
				check("69c. LE STYLE DE TEXTE : la taille se propage aux deux textes ; une graisse posee a la main leve l'ecart texte "
					  "et tient ; ET la variable dans le style : « @encre » compte trois usages (deux textes, le style), et changer la "
					  "variable change ce que le texte lie rend -- les deux propagations se composent",
					  texteSuit && texteTient && usagesEncre == 3u && encreResolue == 0x445566FFu, det);
				// ── 69d. UN STYLE ABSENT EST DIT, JAMAIS UN RENDU VIDE ; DETACHER ──
				const int32 r3 = rectDe(110.f, "#777777");
				dS.nodes[(uint32)r3].styleCalque = NkString("fantome");
				const bool absentDit = dS.TrouverStyle("fantome") == nullptr && couleurPeinte(r3) == 0x777777FFu && dS.PropagerStyle("fantome") == 0;
				const bool detache = dS.DetacherStyle(r2, false) && dS.nodes[(uint32)r2].styleCalque.Empty();
				st->apparence.fills[0].couleur = NkString("#123456");
				dS.PropagerStyle("calque_1");
				const bool detacheTient = couleurPeinte(r2) == 0x0000FFFFu && couleurPeinte(r1) == 0x123456FFu;
				snprintf(det, sizeof(det), "« fantome » : absent=%d, r3 peint %08X (ses valeurs), propagation 0 ; r2 detache=%d puis style -> #123456 : r2 %08X (garde), r1 %08X (suit)",
						 dS.TrouverStyle("fantome") == nullptr ? 1 : 0, couleurPeinte(r3), detache ? 1 : 0, couleurPeinte(r2), couleurPeinte(r1));
				check("69d. UN STYLE ABSENT EST DIT (introuvable) ET LE CALQUE GARDE SES VALEURS -- jamais un rendu vide ; "
					  "« Detacher » rend les valeurs locales : le calque detache ne suit plus, l'autre suit encore",
					  absentDit && detache && detacheTient, det);
				// ── 69e. LA SUPPRESSION GARDEE, le detachement de tous ──
				uint32 uRefus = 0u;
				const bool refuse = !dS.SupprimerStyle("calque_1", &uRefus) && uRefus == 1u && dS.styles.Size() == 2u;
				const uint32 detaches = dS.DetacherTousStyle("calque_1");
				const bool supprime = dS.SupprimerStyle("calque_1") && dS.styles.Size() == 1u && dS.nodes[(uint32)r1].styleCalque.Empty()
									  && couleurPeinte(r1) == 0x123456FFu;
				snprintf(det, sizeof(det), "suppression refusee=%d (utilise par %u) ; detaches %u ; supprime ensuite=%d, r1 garde %08X, styles restants %u",
						 refuse ? 1 : 0, uRefus, detaches, supprime ? 1 : 0, couleurPeinte(r1), (uint32)dS.styles.Size());
				check("69e. LA SUPPRESSION REFUSE tant qu'un calque lie le style (le nombre dit) ; tout detacher rend les valeurs "
					  "locales ; puis la suppression passe et le calque garde ce qu'il montrait",
					  refuse && detaches == 1u && supprime, det);
				// ── 69f. LE RAIL « STYLES » (le gabarit du rail Variables) ──
				{
					static StylesPanel railS(&stS);
					NkEditorFrameContext ec;
					ec.ui = &ctxS;
					ec.dt = 0.016f;
					auto rail = [&](float32 mx, float32 my, bool bas, uint32 cp) {
						ctxS.input.mousePos = {mx, my};
						ctxS.input.mouseDown[0] = bas;
						ctxS.BeginFrame(0.016f);
						if (cp)
							ctxS.input.PushChar(cp);
						ctxS.BeginLayout({0.f, 0.f, 260.f, 900.f});
						railS.OnUI(ec);
						ctxS.EndFrame();
					};
					auto clicRail = [&](const nkgui::NkRect &r) {
						const float32 x = r.x + r.w * 0.5f, y = r.y + r.h * 0.5f;
						rail(x, y, false, 0u);
						rail(x, y, true, 0u);
						rail(x, y, false, 0u);
						rail(-1.f, -1.f, false, 0u);
					};
					rail(-1.f, -1.f, false, 0u);
					const nkgui::NkRect rNom = railS.RectNom(0), rPou = railS.RectPoubelle(0);
					const bool rects = rNom.w > 0.f && rPou.w > 0.f && rPou.x > rNom.x;
					stS.status = NkString();
					clicRail(rPou); // texte_1 est lie par t1 et t2
					const NkString statusGarde = stS.status;
					const bool garde = dS.styles.Size() == 1u && statusGarde.Data() && strstr(statusGarde.Data(), "utilisé par 2 calques") != nullptr;
					clicRail(rNom);
					const bool enRenommage = railS.EnRenommage() == 0;
					rail(-1.f, -1.f, false, (uint32)'!');
					rail(-1.f, -1.f, false, 0u);
					const NkString nomR = dS.styles.Empty() ? NkString("(aucun)") : dS.styles[0].nom;
					const bool renomme = nomR.Data() && strstr(nomR.Data(), "!") != nullptr && strstr(nomR.Data(), "Titre") != nullptr;
					clicRail(nkgui::NkRect{100.f, 850.f, 4.f, 4.f});
					const bool fini = railS.EnRenommage() == -1;
					const uint32 detachesT = dS.DetacherTousStyle("texte_1");
					stS.status = NkString();
					rail(-1.f, -1.f, false, 0u);
					clicRail(railS.RectPoubelle(0));
					const bool supprimeT = dS.styles.Empty() && stS.status.Data() && strstr(stS.status.Data(), "supprimé") != nullptr;
					snprintf(det, sizeof(det), "rects=%d ; poubelle gardee=%d (« %s ») ; renommage : ouvert=%d, nom -> « %s », fini=%d ; detaches %u, supprime=%d",
							 rects ? 1 : 0, garde ? 1 : 0, statusGarde.Data() ? statusGarde.Data() : "", enRenommage ? 1 : 0,
							 nomR.Data() ? nomR.Data() : "?", fini ? 1 : 0, detachesT, supprimeT ? 1 : 0);
					check("69f. LE RAIL « STYLES » : la ligne (apercu, nom, poubelle) se dessine ; la poubelle REFUSE tant qu'un "
						  "calque lie le style (« utilise par N calques ») ; un clic sur le nom ouvre le renommage, une frappe "
						  "l'ecrit, un clic ailleurs le termine ; tous detaches, la poubelle supprime et le dit",
						  rects && garde && enRenommage && renomme && fini && detachesT == 2u && supprimeT, det);
				}
				// ── 69g. LA RANGEE « STYLE » DE LA SECTION REMPLISSAGES : Creer, Lier, Appliquer, Detacher ──
				{
					static InspectorPanel inspS(&stS);
					NkEditorFrameContext ec;
					ec.ui = &ctxS;
					ec.dt = 0.016f;
					stS.Recompute(NkPaintRect{0.f, 0.f, 600.f, 900.f});
					auto souris = [&](float32 mx, float32 my, bool bas) {
						ctxS.input.mousePos = {mx, my};
						ctxS.input.mouseDown[0] = bas;
						ctxS.BeginFrame(0.016f);
						ctxS.BeginLayout({340.f, 0.f, 260.f, 900.f});
						inspS.OnUI(ec);
						NkDessinerPickerDemande(ctxS, stS);
						ctxS.EndFrame();
					};
					auto cliquer = [&](float32 x, float32 y) {
						souris(x, y, false);
						souris(x, y, true);
						souris(x, y, false);
						souris(-1.f, -1.f, false);
					};
					auto clicRect = [&](const nkgui::NkRect &r) { cliquer(r.x + r.w * 0.5f, r.y + r.h * 0.5f); };
					// r1 (sans style desormais) : « Creer »
					stS.SelectSingle(r1);
					souris(-1.f, -1.f, false);
					souris(-1.f, -1.f, false);
					const nkgui::NkRect bCr = inspS.RectStyle(0u, 0u);
					clicRect(bCr);
					const bool cree = bCr.w > 0.f && dS.styles.Size() == 1u && NkComponentDecl::StrEq(dS.nodes[(uint32)r1].styleCalque.Data(), dS.styles[0].cle.Data())
									  && stS.status.Data() && strstr(stS.status.Data(), "créé depuis ce calque") != nullptr;
					// r2 : « Lier ˅ » deplie la liste, un clic sur le style le lie
					stS.SelectSingle(r2);
					souris(-1.f, -1.f, false);
					souris(-1.f, -1.f, false);
					const nkgui::NkRect bLi = inspS.RectStyle(0u, 2u);
					clicRect(bLi);
					const nkgui::NkRect rl0 = inspS.RectStyleListe(0u, 0u);
					clicRect(rl0);
					const bool lie = bLi.w > 0.f && rl0.w > 0.f && NkComponentDecl::StrEq(dS.nodes[(uint32)r2].styleCalque.Data(), dS.styles[0].cle.Data())
									 && couleurPeinte(r2) == couleurPeinte(r1);
					// r2 : le selecteur ecrit une couleur -> surcharge locale ; « Appliquer » -> le style prend, r1 suit
					stS.picker = DesignState::DemandePicker();
					stS.picker.ouvert = true;
					stS.picker.id = ctxS.GetId("##sonde.popover.style");
					stS.picker.genre = 1u;
					stS.picker.noeud = r2;
					stS.picker.index = 0;
					stS.picker.ancre = {360.f, 200.f, 16.f, 16.f};
					souris(-1.f, -1.f, false);
					souris(-1.f, -1.f, false);
					float32 px = 1e9f, py = 1e9f;
					for (uint32 i = 0; i < (uint32)ctxS.dlOverlay.vtx.Size(); ++i) {
						if (ctxS.dlOverlay.vtx[i].pos.x < px) px = ctxS.dlOverlay.vtx[i].pos.x;
						if (ctxS.dlOverlay.vtx[i].pos.y < py) py = ctxS.dlOverlay.vtx[i].pos.y;
					}
					cliquer(px + 0.5f + 8.f + 80.f, py + 0.5f + 8.f + 26.f + 80.f);
					if (ctxS.popupDepth > 0)
						ctxS.ClosePopup();
					stS.picker = DesignState::DemandePicker();
					souris(-1.f, -1.f, false);
					souris(-1.f, -1.f, false);
					const bool surcharge = dS.nodes[(uint32)r2].Surcharge(NkUINode::EcartRemplissages) && couleurPeinte(r2) != couleurPeinte(r1);
					const uint32 c2 = couleurPeinte(r2);
					const nkgui::NkRect bApp = inspS.RectStyle(0u, 3u);
					clicRect(bApp);
					const bool applique = bApp.w > 0.f && couleurPeinte(r1) == c2 && !dS.nodes[(uint32)r2].Surcharge(NkUINode::EcartRemplissages)
										  && NkUIDocument::MemeEmpreinte(dS.styles[0].apparence, dS.nodes[(uint32)r2], NkUINode::EcartRemplissages);
					souris(-1.f, -1.f, false);
					const nkgui::NkRect bDet = inspS.RectStyle(0u, 1u);
					clicRect(bDet);
					const bool detacheUI = bDet.w > 0.f && dS.nodes[(uint32)r2].styleCalque.Empty() && couleurPeinte(r2) == c2;
					snprintf(det, sizeof(det), "Creer=%d (« %s » lie a r1) ; Lier=%d (liste depliee, r2 peint comme r1) ; le carre SV sur r2 -> surcharge=%d (%08X) ; "
											   "Appliquer=%d (r1 suit, ecart tombe) ; Detacher=%d",
							 cree ? 1 : 0, dS.styles.Empty() ? "?" : dS.styles[0].nom.Data(), lie ? 1 : 0, surcharge ? 1 : 0, c2, applique ? 1 : 0,
							 detacheUI ? 1 : 0);
					check("69g. LA RANGEE « STYLE » EN TETE DE REMPLISSAGES : « Creer » fait un style du calque et le lie ; « Lier ˅ » "
						  "deplie les styles en place (pas un popup) et un clic lie ; une couleur posee par le selecteur est une "
						  "surcharge locale (le bit, la rangee le dit) ; « Appliquer » fait du calque le style et l'autre suit ; "
						  "« Detacher » rend les valeurs locales",
						  cree && lie && surcharge && applique && detacheUI, det);
				}
			}
		}
		// ── 70. LA CHAINE DE L'IMAGE (05/09) : un PNG 2x2 ECRIT PAR LA SONDE (pas un fichier de
		//    Rodolf), relu par le codec PNG ; le peintre emet un polygone texture par cellule
		//    (uv par sommet, exact) ; TEMOIN EN TEXELS : a quatre points par cadrage, le texel
		//    que l'uv du polygone designe est celui que la definition du cadrage annonce ; la
		//    rotation ; le contour arrondi qui rogne ; l'image absente dite ; le kit transmet.
		{
			char det[600];
			static const uint8 kM[4] = {255, 0, 255, 255}, kG[4] = {0, 255, 0, 255};
			// 70a. le PNG 2x2 damier magenta / vert, ecrit puis relu (aller-retour exact)
			bool pngOk = false;
			{
				NkImage img;
				if (img.Create(2u, 2u, math::NkColor(), 4) && img.Pixels()) {
					uint8 *px = img.Pixels();
					for (int32 y = 0; y < 2; ++y)
						for (int32 x = 0; x < 2; ++x) {
							const uint8 *c = ((x + y) & 1) ? kG : kM;
							for (int32 k = 0; k < 4; ++k)
								px[(y * 2 + x) * 4 + k] = c[k];
						}
					pngOk = img.SavePNG("sonde_image_2x2.png");
				}
				NkImage relu;
				bool exact = false;
				if (pngOk && relu.Load("sonde_image_2x2.png", 4) && relu.Width() == 2 && relu.Height() == 2 && relu.Pixels()) {
					exact = true;
					for (int32 i = 0; i < 16; ++i)
						if (relu.Pixels()[i] != (((((i / 4) % 2) + (i / 8)) & 1) ? kG : kM)[i % 4])
							exact = false;
				}
				snprintf(det, sizeof(det), "ecrit=%d, relu 2x2 exact=%d (%d x %d)", pngOk ? 1 : 0, exact ? 1 : 0, relu.Width(), relu.Height());
				check("70a. LE PNG 2x2 (magenta / vert) ecrit par la sonde avec NKImage et RELU par son codec PNG : les seize octets "
					  "reviennent identiques -- la source du temoin, pas un fichier de Rodolf",
					  pngOk && exact, det);
			}
			static DesignState stImg;
			stImg.doc.NewDocument("Toile", NkAuthor::Humain);
			stImg.cheminActif = NkString(); // le repertoire courant : c'est la que le PNG est
			stImg.images.Vider();
			renderdetail::NkPoserFournisseurImages(&NkObtenirImageDuDocument, &stImg);
			const int32 pg = stImg.doc.AddChild(0, "", NkAuthor::Humain);
			stImg.doc.nodes[(uint32)pg].shape = NkString("frame");
			stImg.doc.nodes[(uint32)pg].layout.kind = NkLayoutKind::Free;
			stImg.doc.nodes[(uint32)pg].width.mode = NkSizeMode::Fixed;
			stImg.doc.nodes[(uint32)pg].width.value = 400.f;
			stImg.doc.nodes[(uint32)pg].height.mode = NkSizeMode::Fixed;
			stImg.doc.nodes[(uint32)pg].height.value = 300.f;
			auto rectImage = [&](float32 w, float32 h, const char *cadrage, float32 rot, float32 rayon, const char *source) {
				const int32 r = stImg.doc.AddChild(pg, "", NkAuthor::Humain);
				NkUINode &n = stImg.doc.nodes[(uint32)r];
				n.shape = NkString("rect");
				n.posX = 10.f;
				n.posY = 10.f;
				n.width.mode = NkSizeMode::Fixed;
				n.width.value = w;
				n.height.mode = NkSizeMode::Fixed;
				n.height.value = h;
				n.radius = rayon;
				NkRemplissage f;
				f.genre = NkString("image");
				f.image = NkString(source);
				f.cadrage = NkString(cadrage);
				f.rotationImage = rot;
				n.fills.PushBack(f);
				return r;
			};
			// le texel qu'un point du document recoit, par les polygones textures ENREGISTRES
			// (le dernier qui le contient ; l'uv est affine sur l'eventail -> barycentrique)
			auto texel = [&](NkRecordingPaint &rec, float32 X, float32 Y, uint32 &rgba) -> bool {
				bool trouve = false;
				for (uint32 ci = 0; ci < (uint32)rec.cmds.Size(); ++ci) {
					const NkPaintCmd &c = rec.cmds[ci];
					if (c.op != NkPaintOp::Image || c.xy.Size() < 6u)
						continue;
					const uint32 n = (uint32)c.xy.Size() / 2u;
					for (uint32 i = 1; i + 1 < n; ++i) {
						const float32 ax = c.xy[0], ay = c.xy[1], bx = c.xy[i * 2], by = c.xy[i * 2 + 1], qx = c.xy[(i + 1) * 2], qy = c.xy[(i + 1) * 2 + 1];
						const float32 d = (bx - ax) * (qy - ay) - (qx - ax) * (by - ay);
						if (d > -1e-6f && d < 1e-6f)
							continue;
						const float32 l1 = ((bx - X) * (qy - Y) - (qx - X) * (by - Y)) / d;
						const float32 l2 = ((qx - X) * (ay - Y) - (ax - X) * (qy - Y)) / d;
						const float32 l0 = l1, la = 1.f - l1 - l2;
						(void)l0;
						if (l1 < -1e-4f || l2 < -1e-4f || la < -1e-4f)
							continue;
						const float32 u = l1 * c.uv[0] + l2 * c.uv[i * 2] + la * c.uv[(i + 1) * 2];
						const float32 v = l1 * c.uv[1] + l2 * c.uv[i * 2 + 1] + la * c.uv[(i + 1) * 2 + 1];
						int32 tx = (int32)(u * 2.f), ty = (int32)(v * 2.f);
						if (tx < 0) tx = 0;
						if (tx > 1) tx = 1;
						if (ty < 0) ty = 0;
						if (ty > 1) ty = 1;
						rgba = ((tx + ty) & 1) ? 0x00FF00FFu : 0xFF00FFFFu;
						trouve = true;
					}
				}
				return trouve;
			};
			auto peindre = [&](int32 noeud, NkRecordingPaint &rec, NkPaintRect &r) {
				NkPaintRect sfc;
				sfc.x = 0.f;
				sfc.y = 0.f;
				sfc.w = 600.f;
				sfc.h = 900.f;
				NkLayoutResult lay;
				NkComputeLayout(stImg.doc, sfc, lay);
				r = lay.At(noeud);
				for (uint32 i = 0; i < (uint32)stImg.doc.nodes.Size(); ++i)
					if (i != 0u && (int32)i != noeud && (int32)i != pg)
						stImg.doc.nodes[i].masque = true;
				rec.Reset();
				RenderDocument(rec, stImg.doc, sfc);
				for (uint32 i = 0; i < (uint32)stImg.doc.nodes.Size(); ++i)
					stImg.doc.nodes[i].masque = false;
			};
			struct Attente { float32 fx, fy; uint32 rgba; bool present; };
			const uint32 M = 0xFF00FFFFu, G = 0x00FF00FFu;
			auto verifier = [&](const char *nom, int32 noeud, const Attente *att, uint32 na, char *out, size_t cap, bool &ok) {
				NkRecordingPaint rec;
				NkPaintRect r;
				peindre(noeud, rec, r);
				uint32 nImg = 0u;
				for (uint32 i = 0; i < (uint32)rec.cmds.Size(); ++i)
					if (rec.cmds[i].op == NkPaintOp::Image)
						++nImg;
				size_t l = strlen(out);
				snprintf(out + l, cap - l, "%s%s (%u polygone(s)) : ", l ? " ; " : "", nom, nImg);
				for (uint32 a = 0; a < na; ++a) {
					uint32 got = 0u;
					const bool pres = texel(rec, r.x + att[a].fx * r.w, r.y + att[a].fy * r.h, got);
					const bool bon = pres == att[a].present && (!pres || got == att[a].rgba);
					if (!bon)
						ok = false;
					l = strlen(out);
					snprintf(out + l, cap - l, "%s%s", a ? "," : "", !pres ? (att[a].present ? "VIDE!" : "vide") : (got == M ? (bon ? "M" : "M!") : (bon ? "G" : "G!")));
				}
				if (nImg == 0u && na && att[0].present)
					ok = false;
			};
			// 70b. les cinq cadrages, en texels
			{
				bool ok = true;
				det[0] = '\0';
				const Attente quatre[4] = {{0.25f, 0.25f, M, true}, {0.75f, 0.25f, G, true}, {0.25f, 0.75f, G, true}, {0.75f, 0.75f, M, true}};
				verifier("stretch 100x50", rectImage(100.f, 50.f, "stretch", 0.f, 0.f, "sonde_image_2x2.png"), quatre, 4u, det, sizeof(det), ok);
				verifier("fill 100x50 (couvre, v visible 0,25..0,75)", rectImage(100.f, 50.f, "", 0.f, 0.f, "sonde_image_2x2.png"), quatre, 4u, det, sizeof(det), ok);
				const Attente fit[4] = {{0.125f, 0.5f, 0u, false}, {0.375f, 0.25f, M, true}, {0.625f, 0.25f, G, true}, {0.375f, 0.75f, G, true}};
				verifier("fit 100x50 (contient : 50x50 au centre, bandes nues)", rectImage(100.f, 50.f, "fit", 0.f, 0.f, "sonde_image_2x2.png"), fit, 4u, det, sizeof(det), ok);
				const Attente tuiles[4] = {{0.0625f, 0.125f, M, true}, {0.1875f, 0.125f, G, true}, {0.3125f, 0.125f, M, true}, {0.1875f, 0.375f, M, true}};
				verifier("tile 8x4 (tuiles 2x2 naturelles)", rectImage(8.f, 4.f, "tile", 0.f, 0.f, "sonde_image_2x2.png"), tuiles, 4u, det, sizeof(det), ok);
				const int32 rc = rectImage(100.f, 50.f, "crop", 0.f, 0.f, "sonde_image_2x2.png");
				stImg.doc.nodes[(uint32)rc].fills[0].cropX = 0.5f;
				stImg.doc.nodes[(uint32)rc].fills[0].cropW = 0.5f;
				const Attente cropA[4] = {{0.25f, 0.25f, G, true}, {0.75f, 0.25f, G, true}, {0.25f, 0.75f, M, true}, {0.75f, 0.75f, M, true}};
				verifier("crop (fenetre x 0,5..1 : la colonne droite etiree)", rc, cropA, 4u, det, sizeof(det), ok);
				check("70b. LES CINQ CADRAGES PEINTS, temoin en TEXELS (quatre points par cadrage, le texel designe par l'uv du "
					  "polygone enregistre) : Stretch, Fill (couvre, rogne), Fit (contient, bandes nues : un point hors de "
					  "l'image ne recoit rien), Tile (tuiles a la taille naturelle), Crop (la fenetre)",
					  ok, det);
			}
			// 70c. la rotation, le contour arrondi qui rogne, l'image absente DITE
			{
				bool ok = true;
				det[0] = '\0';
				// tourne d'un quart de tour horaire : le coin haut-gauche montre l'ancien bas-gauche (vert)
				const Attente rot[4] = {{0.25f, 0.25f, G, true}, {0.75f, 0.25f, M, true}, {0.75f, 0.75f, G, true}, {0.25f, 0.75f, M, true}};
				verifier("rotation 90 sur 40x40", rectImage(40.f, 40.f, "stretch", 90.f, 0.f, "sonde_image_2x2.png"), rot, 4u, det, sizeof(det), ok);
				// un disque (rayon 20 sur 40x40) : le coin est hors du contour, le centre dedans
				const Attente rond[2] = {{0.03f, 0.03f, 0u, false}, {0.5f, 0.5f, M, true}};
				verifier("disque 40x40 r=20 (le coin est rogne)", rectImage(40.f, 40.f, "stretch", 0.f, 20.f, "sonde_image_2x2.png"), rond, 2u, det, sizeof(det), ok);
				// absente : aucun polygone texture, le damier (FillColor), le chemin note
				renderdetail::NkFournisseurCourant().derniereAbsente = nullptr;
				NkRecordingPaint rec;
				NkPaintRect r;
				peindre(rectImage(40.f, 40.f, "", 0.f, 0.f, "absente.png"), rec, r);
				uint32 nImg = 0u, nFill = 0u;
				for (uint32 i = 0; i < (uint32)rec.cmds.Size(); ++i) {
					if (rec.cmds[i].op == NkPaintOp::Image) ++nImg;
					if (rec.cmds[i].op == NkPaintOp::FillColor) ++nFill;
				}
				const char *dite = renderdetail::NkFournisseurCourant().derniereAbsente;
				const bool absenteDite = nImg == 0u && nFill >= 4u && dite && NkComponentDecl::StrEq(dite, "absente.png")
										 && stImg.images.Trouver("absente.png") && stImg.images.Trouver("absente.png")->absente;
				if (!absenteDite)
					ok = false;
				size_t l = strlen(det);
				snprintf(det + l, sizeof(det) - l, " ; absente.png : %u polygone(s), %u FillColor (damier), notee « %s », entree absente=%d",
						 nImg, nFill, dite ? dite : "(rien)", stImg.images.Trouver("absente.png") ? (stImg.images.Trouver("absente.png")->absente ? 1 : 0) : -1);
				check("70c. LA ROTATION (un quart de tour : le haut-gauche montre l'ancien bas-gauche), LE CONTOUR QUI ROGNE (un "
					  "disque : le coin ne recoit rien, le centre si), et L'IMAGE ABSENTE : aucun polygone, le damier, le chemin "
					  "NOTE et l'entree marquee absente -- jamais silencieuse",
					  ok, det);
			}
			// 70d. le kit transmet : un contexte reel, un televerseur factice (un handle, pas de GPU) --
			// la toile emet une commande TEXTUREE portant ce handle ; sans handle (0) : rien de texture
			{
				static nkgui::NkGuiContext ctxT;
				if (!ctxT.Init(600, 900)) {
					check("70d. la transmission au kit", false, "Init a refuse");
				} else {
					stImg.images.Vider();
					stImg.images.televerser = [](void *, const uint8 *, int32, int32) -> uint32 { return 0x4E4B0200u; };
					const int32 rt = rectImage(100.f, 50.f, "stretch", 0.f, 0.f, "sonde_image_2x2.png");
					stImg.Recompute(NkPaintRect{0.f, 0.f, 600.f, 900.f});
					stImg.SelectSingle(rt);
					static PreviewPanel toileT(&stImg);
					NkEditorFrameContext ec;
					ec.ui = &ctxT;
					ec.dt = 0.016f;
					auto image = [&]() {
						ctxT.input.mousePos = {-1.f, -1.f};
						ctxT.input.mouseDown[0] = false;
						ctxT.BeginFrame(0.016f);
						ctxT.BeginLayout({0.f, 0.f, 340.f, 900.f});
						toileT.OnUI(ec);
						ctxT.EndFrame();
					};
					image();
					image();
					uint32 avecHandle = 0u;
					for (uint32 i = 0; i < (uint32)ctxT.dl.cmds.Size(); ++i)
						if (ctxT.dl.cmds[i].texId == 0x4E4B0200u)
							++avecHandle;
					// sans televerseur : le handle est 0, le peintre NKGui repond faux, le damier se peint
					stImg.images.Vider();
					stImg.images.televerser = nullptr;
					image();
					image();
					uint32 sansHandle = 0u;
					for (uint32 i = 0; i < (uint32)ctxT.dl.cmds.Size(); ++i)
						if (ctxT.dl.cmds[i].texId == 0x4E4B0200u)
							++sansHandle;
					snprintf(det, sizeof(det), "commandes texturees au handle 0x4E4B0200 : %u avec televerseur, %u sans (damier)", avecHandle, sansHandle);
					check("70d. LE KIT TRANSMET : la toile (NkGuiComponentPaint) emet une commande texturee au handle que le "
						  "televerseur a rendu ; sans televerseur (handle 0) le peintre repond faux et le damier prend la place",
						  avecHandle >= 1u && sansHandle == 0u, det);
				}
			}
			// 70e. le popover : l'apercu (une commande texturee au handle), « Choisir une image... »
			// ouvre le selecteur du kit et ferme le popover, un choix confirme devient un chemin
			// RELATIF au document (ici : au repertoire courant, le document n'est pas enregistre)
			{
				static nkgui::NkGuiContext ctxP;
				if (!ctxP.Init(600, 900)) {
					check("70e. le popover image", false, "Init a refuse");
				} else {
					stImg.images.Vider();
					stImg.images.televerser = [](void *, const uint8 *, int32, int32) -> uint32 { return 0x4E4B0201u; };
					const int32 rp = rectImage(100.f, 50.f, "", 0.f, 0.f, "sonde_image_2x2.png");
					stImg.Recompute(NkPaintRect{0.f, 0.f, 600.f, 900.f});
					stImg.SelectSingle(rp);
					static InspectorPanel inspP(&stImg);
					NkEditorFrameContext ec;
					ec.ui = &ctxP;
					ec.dt = 0.016f;
					auto souris = [&](float32 mx, float32 my, bool bas) {
						ctxP.input.mousePos = {mx, my};
						ctxP.input.mouseDown[0] = bas;
						ctxP.BeginFrame(0.016f);
						ctxP.BeginLayout({340.f, 0.f, 260.f, 900.f});
						inspP.OnUI(ec);
						NkDessinerPickerDemande(ctxP, stImg);
						ctxP.EndFrame();
					};
					stImg.picker = DesignState::DemandePicker();
					stImg.picker.ouvert = true;
					stImg.picker.id = ctxP.GetId("##sonde.popover.image");
					stImg.picker.genre = 1u;
					stImg.picker.noeud = rp;
					stImg.picker.index = 0;
					stImg.picker.ancre = {360.f, 200.f, 16.f, 16.f};
					souris(-1.f, -1.f, false);
					souris(-1.f, -1.f, false);
					uint32 apercu = 0u;
					for (uint32 i = 0; i < (uint32)ctxP.dlOverlay.cmds.Size(); ++i)
						if (ctxP.dlOverlay.cmds[i].texId == 0x4E4B0201u)
							++apercu;
					const nkgui::NkRect bCh = inspP.RectImageChoisir();
					souris(bCh.x + bCh.w * 0.5f, bCh.y + bCh.h * 0.5f, false);
					souris(bCh.x + bCh.w * 0.5f, bCh.y + bCh.h * 0.5f, true);
					souris(bCh.x + bCh.w * 0.5f, bCh.y + bCh.h * 0.5f, false);
					const bool ouvert = bCh.w > 0.f && stImg.choixImage.pickerOpen && !stImg.picker.ouvert
										&& stImg.choixImageNoeud == rp && stImg.choixImageIndex == 0;
					// le choix, confirme comme le ferait le bouton du selecteur : un chemin ABSOLU du
					// repertoire courant -> relatif dans le document
					NkString absolu = NkDirectory::GetCurrentDirectory().ToString();
					if (!absolu.Empty()) {
						const char last = absolu.Data()[absolu.Length() - 1];
						if (last != '/' && last != '\\')
							absolu.Append('/');
					}
					absolu.Append("sonde_image_2x2.png");
					nkentseu::editorkit::NkFilePickerState::CopyTo(stImg.choixImage.pickerResultPath, absolu.Data(),
																  (int32)sizeof(stImg.choixImage.pickerResultPath));
					stImg.doc.nodes[(uint32)rp].fills[0].image = NkString("ancienne.png");
					stImg.choixImage.pickerConfirmed = true;
					stImg.choixImage.PickerCancel();
					souris(-1.f, -1.f, false);
					const NkString source = stImg.doc.nodes[(uint32)rp].fills[0].image;
					const bool relatif = NkComponentDecl::StrEq(source.Data(), "sonde_image_2x2.png") && !stImg.choixImage.pickerOpen
										 && stImg.choixImageNoeud == -1 && stImg.status.Data() && strstr(stImg.status.Data(), "relative au document") != nullptr;
					stImg.picker = DesignState::DemandePicker();
					if (ctxP.popupDepth > 0)
						ctxP.ClosePopup();
					souris(-1.f, -1.f, false);
					snprintf(det, sizeof(det), "apercu : %u commande(s) texturee(s) au handle ; « Choisir » -> selecteur ouvert=%d, popover ferme=%d ; confirme « %s » -> source « %s », pied dit relatif=%d",
							 apercu, stImg.choixImage.pickerOpen || ouvert ? 1 : 0, ouvert ? 1 : 0, absolu.Data(), source.Data() ? source.Data() : "?",
							 relatif ? 1 : 0);
					check("70e. LE POPOVER IMAGE : l'apercu est l'image (une commande texturee au handle, plus un damier muet) ; "
						  "« Choisir une image... » ouvre le selecteur de fichier du kit et ferme le popover ; un choix confirme "
						  "devient un chemin RELATIF au document (jamais absolu dans le fichier) et le pied le dit",
						  apercu >= 1u && ouvert && relatif, det);
				}
			}
			renderdetail::NkPoserFournisseurImages(nullptr, nullptr);
		}
		// ── 71. LES SIX MODELES DE COULEUR RESTANTS (HSL, HWB, LCH, LAB, OKLCH, OKLAB) : derriere
		//    la frontiere unique, l'aller-retour Hex -> modele -> Hex est IDENTIQUE sur les
		//    16 777 216 couleurs, pour chacun -- la ou Lunacy derive d'une unite. Et une valeur
		//    connue par modele (les references de CSS Color 4 / Ottosson pour #1976D2).
		{
			char det[600];
			static const char *const kNoms[6] = {"HSL", "HWB", "LCH", "LAB", "OKLCH", "OKLAB"};
			uint32 ecarts[6] = {0u, 0u, 0u, 0u, 0u, 0u};
			uint32 premier[6] = {0u, 0u, 0u, 0u, 0u, 0u};
			// LE BALAYAGE COMPLET EST RELEASE (le coordinateur, 05/09) : en Debug il prendrait
			// des heures sans prouver plus ; la sonde DIT son mode et son pas dans sa sortie.
#if defined(NDEBUG)
			const uint32 pas = 1u;
			const char *mode = "Release : les 16 777 216 couleurs";
#else
			const uint32 pas = 257u; // premier : les trois octets tournent, 65 281 couleurs
			const char *mode = "Debug : 1 couleur sur 257 (65 281) -- le balayage complet est Release";
#endif
			for (int32 m = 3; m <= 8; ++m) {
				for (uint32 c = 0u; c < 16777216u; c += pas) {
					const float32 r = (float32)((c >> 16) & 0xFFu), g = (float32)((c >> 8) & 0xFFu), b = (float32)(c & 0xFFu);
					float32 v[3] = {0.f, 0.f, 0.f}, r2 = 0.f, g2 = 0.f, b2 = 0.f;
					if (!NkRgbVersModele(m, r, g, b, v) || !NkModeleVersRgb(m, v, r2, g2, b2)) {
						++ecarts[m - 3];
						continue;
					}
					const int32 ir = (int32)(r2 + 0.5f), ig = (int32)(g2 + 0.5f), ib = (int32)(b2 + 0.5f);
					if (ir != (int32)r || ig != (int32)g || ib != (int32)b) {
						if (ecarts[m - 3] == 0u)
							premier[m - 3] = c;
						++ecarts[m - 3];
					}
				}
			}
			// DES REFERENCES PUBLIEES, pas des chiffres de memoire : le blanc sRGB vaut par
			// construction Lab(D65) 100 / 0 / 0 et OKLab 1 / 0 / 0 ; le rouge sRGB vaut OKLab
			// 0,627955 / 0,224863 / 0,125846 (Ottosson 2020, table de l'article), Lab(D65)
			// 53,24 / 80,09 / 67,20 (valeurs classiques du rouge sRGB sous D65), HSL 0 / 100 / 50.
			float32 okB[3] = {0.f, 0.f, 0.f}, labB[3] = {0.f, 0.f, 0.f}, okR[3] = {0.f, 0.f, 0.f}, labR[3] = {0.f, 0.f, 0.f}, hslR[3] = {0.f, 0.f, 0.f};
			NkRgbVersModele(8, 255.f, 255.f, 255.f, okB);
			NkRgbVersModele(6, 255.f, 255.f, 255.f, labB);
			NkRgbVersModele(8, 255.f, 0.f, 0.f, okR);
			NkRgbVersModele(6, 255.f, 0.f, 0.f, labR);
			NkRgbVersModele(3, 255.f, 0.f, 0.f, hslR);
			auto pres = [](float32 v, float32 att, float32 tol) { return v - att <= tol && att - v <= tol; };
			const bool connus = pres(okB[0], 100.f, 0.05f) && pres(okB[1], 0.f, 0.001f) && pres(okB[2], 0.f, 0.001f)
								&& pres(labB[0], 100.f, 0.01f) && pres(labB[1], 0.f, 0.01f) && pres(labB[2], 0.f, 0.01f)
								&& pres(okR[0], 62.7955f, 0.05f) && pres(okR[1], 0.224863f, 0.001f) && pres(okR[2], 0.125846f, 0.001f)
								&& pres(labR[0], 53.24f, 0.1f) && pres(labR[1], 80.09f, 0.1f) && pres(labR[2], 67.20f, 0.1f)
								&& pres(hslR[0], 0.f, 0.01f) && pres(hslR[1], 100.f, 0.01f) && pres(hslR[2], 50.f, 0.01f);
			float32 ok[3] = {0.f, 0.f, 0.f};
			NkRgbVersModele(8, 25.f, 118.f, 210.f, ok); // ce que la rangee affichera pour #1976D2 (Lunacy : 56,38 / -0,04 / -0,15)
			bool tous = connus;
			size_t l = 0;
			snprintf(det, sizeof(det), "[%s] ", mode);
			for (int32 k = 0; k < 6; ++k) {
				if (ecarts[k])
					tous = false;
				l = strlen(det);
				snprintf(det + l, sizeof(det) - l, "%s%s : %u ecart(s)", k ? " ; " : "", kNoms[k], ecarts[k]);
				if (ecarts[k]) {
					l = strlen(det);
					snprintf(det + l, sizeof(det) - l, " (premier %06X)", premier[k]);
				}
			}
			l = strlen(det);
			snprintf(det + l, sizeof(det) - l, " ; references : blanc OKLAB %.2f %.3f %.3f LAB %.2f %.2f %.2f, rouge OKLAB %.4f %.4f %.4f LAB %.2f %.2f %.2f HSL %.0f %.0f %.0f (connus=%d) ; #1976D2 -> OKLAB %.2f %.3f %.3f (Lunacy 56,38 / -0,04 / -0,15)",
					 okB[0], okB[1], okB[2], labB[0], labB[1], labB[2], okR[0], okR[1], okR[2], labR[0], labR[1], labR[2], hslR[0], hslR[1], hslR[2],
					 connus ? 1 : 0, ok[0], ok[1], ok[2]);
			check("71. LES SIX MODELES RESTANTS derriere la frontiere unique (CSS Color 4, D65 ; OKLab d'Ottosson) : "
				  "l'aller-retour Hex -> modele -> Hex est IDENTIQUE sur les 16 777 216 couleurs pour HSL, HWB, LCH, LAB, "
				  "OKLCH et OKLAB -- et le blanc, le noir et le rouge sRGB donnent les valeurs PUBLIEES (Ottosson, D65)",
				  tous, det);
		}
		// ── 72. Q94 (Rodolf : « lorsqu'on edite un graphique qui etait en degrade, son degrade
		//    disparait et refuse de s'appliquer par la suite ») : un rectangle a degrade lineaire,
		//    l'entree / la sortie du mode edition sans modification (commandes identiques), un
		//    sommet deplace (les sommets se materialisent : le noeud est un TRACE) -> TOUJOURS un
		//    degrade, des bandes qui suivent le NOUVEAU contour ; « Lineaire » re-choisi sur le trace
		//    -> peint ; un contour CONCAVE -> ses morceaux (triangles) ; l'uni et l'image sur un
		//    trace. Avant : une seule couleur, et rien ne se reappliquait.
		{
			struct PeintrePoly72 : public NkRecordingPaint {
					NkVector<float32> pts;
					NkVector<int32> tailles;
					NkVector<uint32> couleurs;
					bool PolygonHex(const float32 *xy, int32 count, uint32 rgba) override {
						for (int32 i = 0; i < count * 2; ++i)
							pts.PushBack(xy[i]);
						tailles.PushBack(count);
						couleurs.PushBack(rgba);
						return true;
					}
					void Vider() {
						Reset();
						pts.Clear();
						tailles.Clear();
						couleurs.Clear();
					}
			};
			char det[600];
			NkUIDocument dQ;
			dQ.NewDocument("Toile", NkAuthor::Humain);
			dQ.nodes[0].layout.kind = NkLayoutKind::Free;
			dQ.SetMetric("espacement", 0.f);
			dQ.SetMetric("marge", 0.f);
			const int32 rq = dQ.AddChild(0, "", NkAuthor::Humain);
			{
				NkUINode &n = dQ.nodes[(uint32)rq];
				n.shape = NkString("rect");
				n.posX = 100.f;
				n.posY = 100.f;
				n.width.mode = NkSizeMode::Fixed;
				n.width.value = 160.f;
				n.height.mode = NkSizeMode::Fixed;
				n.height.value = 80.f;
				NkRemplissage f;
				f.couleur = NkString("#ffffff");
				f.degrade.type = NkString("lineaire");
				NkArretDegrade a0, a1;
				a0.position = 0.f;
				a0.couleur = NkString("#ff0000");
				a1.position = 1.f;
				a1.couleur = NkString("#0000ff");
				f.degrade.arrets.PushBack(a0);
				f.degrade.arrets.PushBack(a1);
				n.fills.PushBack(f);
			}
			auto peindre = [&](PeintrePoly72 &pp) {
				pp.Vider();
				RenderDocument(pp, dQ, NkPaintRect{0.f, 0.f, 600.f, 400.f});
			};
			auto couleursDistinctes = [](const PeintrePoly72 &pp) {
				uint32 n = 0u;
				for (uint32 i = 0; i < (uint32)pp.couleurs.Size(); ++i) {
					bool vu = false;
					for (uint32 j = 0; j < i && !vu; ++j)
						vu = pp.couleurs[j] == pp.couleurs[i];
					if (!vu)
						++n;
				}
				return n;
			};
			auto etendue = [](const PeintrePoly72 &pp, float32 &x0, float32 &y0, float32 &x1, float32 &y1) {
				x0 = y0 = 1e9f;
				x1 = y1 = -1e9f;
				for (uint32 i = 0; i + 1 < (uint32)pp.pts.Size(); i += 2) {
					if (pp.pts[i] < x0) x0 = pp.pts[i];
					if (pp.pts[i] > x1) x1 = pp.pts[i];
					if (pp.pts[i + 1] < y0) y0 = pp.pts[i + 1];
					if (pp.pts[i + 1] > y1) y1 = pp.pts[i + 1];
				}
			};
			PeintrePoly72 avant, apres, deplace, rechoisi, concave, uni, image;
			peindre(avant);
			// 72a. entrer / sortir du mode edition sans rien modifier : les sommets ne se
			// materialisent pas (c'est la premiere MODIFICATION qui le fait) -> memes commandes
			peindre(apres);
			bool identiques = avant.cmds.Size() == apres.cmds.Size() && avant.tailles.Size() == apres.tailles.Size();
			for (uint32 i = 0; identiques && i < (uint32)avant.cmds.Size(); ++i)
				identiques = avant.cmds[i].SameAs(apres.cmds[i]);
			// 72b. un sommet deplace : le noeud est un trace, le degrade reste, ses bandes suivent le contour
			NkMaterialiserSommets(dQ.nodes[(uint32)rq]);
			const bool materialise = dQ.nodes[(uint32)rq].sommets.Size() == 4u;
			dQ.nodes[(uint32)rq].sommets[2].x = 1.6f; // le coin bas-droit tire vers la droite : le contour deborde la boite
			peindre(deplace);
			float32 x0, y0, x1, y1;
			etendue(deplace, x0, y0, x1, y1);
			const bool degradeTient = deplace.tailles.Size() >= 8u && couleursDistinctes(deplace) >= 4u && x1 > 100.f + 160.f + 20.f;
			// 72c. « Lineaire » re-choisi (un degrade neuf pose sur le trace) : peint
			{
				NkRemplissage &f = dQ.nodes[(uint32)rq].fills[0];
				f.degrade.arrets[0].couleur = NkString("#00ff00");
				f.degrade.angle = 90.f;
			}
			peindre(rechoisi);
			bool vert = false;
			for (uint32 i = 0; i < (uint32)rechoisi.couleurs.Size(); ++i)
				if ((rechoisi.couleurs[i] & 0x00FF0000u) >= 0x00800000u && (rechoisi.couleurs[i] & 0xFF000000u) < 0x40000000u)
					vert = true;
			const bool rechoisiPeint = rechoisi.tailles.Size() >= 8u && couleursDistinctes(rechoisi) >= 4u && vert;
			// 72d. un contour CONCAVE (le coin haut-droit rentre) : les bandes se rognent par les triangles
			dQ.nodes[(uint32)rq].sommets[1].x = 0.2f;
			dQ.nodes[(uint32)rq].sommets[1].y = -0.2f;
			peindre(concave);
			const bool concaveOk = concave.tailles.Size() >= 8u && couleursDistinctes(concave) >= 4u;
			// 72e. l'uni sur le trace : UN polygone du contour ; l'image sur le trace : des polygones textures
			{
				NkRemplissage &f = dQ.nodes[(uint32)rq].fills[0];
				f.degrade.arrets.Clear();
				f.couleur = NkString("#123456");
			}
			peindre(uni);
			const bool uniOk = uni.tailles.Size() == 1u && uni.tailles[0] >= 4 && uni.couleurs[0] == 0x123456FFu;
			uint32 nImg = 0u;
			{
				static DesignState stQ;
				stQ.images.Vider();
				renderdetail::NkPoserFournisseurImages(&NkObtenirImageDuDocument, &stQ);
				NkRemplissage &f = dQ.nodes[(uint32)rq].fills[0];
				f.genre = NkString("image");
				f.image = NkString("sonde_image_2x2.png");
				f.cadrage = NkString("stretch");
				peindre(image);
				for (uint32 i = 0; i < (uint32)image.cmds.Size(); ++i)
					if (image.cmds[i].op == NkPaintOp::Image)
						++nImg;
				renderdetail::NkPoserFournisseurImages(nullptr, nullptr);
			}
			snprintf(det, sizeof(det),
					 "sans modification : %u commandes identiques=%d ; sommets materialises=%d ; sommet deplace : %u polygones, %u couleurs, "
					 "etendue x jusqu'a %.0f (boite 100..260) ; « Lineaire » re-choisi : %u polygones, %u couleurs, vert present=%d ; "
					 "concave : %u polygones, %u couleurs ; uni : %u polygone(s) de %d sommets ; image sur le trace : %u polygone(s) texture(s)",
					 (uint32)avant.cmds.Size(), identiques ? 1 : 0, materialise ? 1 : 0, (uint32)deplace.tailles.Size(), couleursDistinctes(deplace), x1,
					 (uint32)rechoisi.tailles.Size(), couleursDistinctes(rechoisi), vert ? 1 : 0, (uint32)concave.tailles.Size(), couleursDistinctes(concave),
					 (uint32)uni.tailles.Size(), uni.tailles.Empty() ? 0 : uni.tailles[0], nImg);
			check("72. Q94 -- LE DEGRADE SURVIT A L'EDITION DU GRAPHIQUE : entrer / sortir sans modifier ne change rien ; un sommet "
				  "deplace fait un TRACE et le degrade reste (des bandes qui suivent le nouveau contour) ; « Lineaire » re-choisi sur "
				  "le trace se peint ; un contour concave se rogne par ses triangles ; l'uni est le contour, l'image se pose dessus",
				  identiques && materialise && degradeTient && rechoisiPeint && concaveOk && uniOk && nImg >= 1u, det);
		}
		// ── 73. L'ANNEAU D'UN TRACE EDITE : rect deforme + bordure 4 px centree, jointure ronde
		//    -> un anneau ferme de 4 px mesure aux normales (quatre quadrilateres), des ARCS aux
		//    sommets ; onglet -> quatre points par coin, biseau -> trois ; concave -> chaque piece
		//    convexe, toutes a moins d'une demi-epaisseur du contour (pas d'auto-croisement) ;
		//    interieure -> tout dedans, exterieure -> tout dehors ; l'arrondi d'un rect survit a
		//    la materialisation (le rayon voyage avec les sommets) ; l'uni reste dessous.
		{
			struct PeintrePoly73 : public NkRecordingPaint {
					NkVector<float32> pts;
					NkVector<int32> tailles;
					NkVector<uint32> couleurs;
					bool PolygonHex(const float32 *xy, int32 count, uint32 rgba) override {
						for (int32 i = 0; i < count * 2; ++i)
							pts.PushBack(xy[i]);
						tailles.PushBack(count);
						couleurs.PushBack(rgba);
						return true;
					}
					void Vider() {
						Reset();
						pts.Clear();
						tailles.Clear();
						couleurs.Clear();
					}
			};
			char det[640];
			NkUIDocument dA;
			dA.NewDocument("Toile", NkAuthor::Humain);
			dA.nodes[0].layout.kind = NkLayoutKind::Free;
			dA.SetMetric("espacement", 0.f);
			dA.SetMetric("marge", 0.f);
			const int32 ra = dA.AddChild(0, "", NkAuthor::Humain);
			{
				NkUINode &n = dA.nodes[(uint32)ra];
				n.shape = NkString("rect");
				n.posX = 100.f;
				n.posY = 100.f;
				n.width.mode = NkSizeMode::Fixed;
				n.width.value = 160.f;
				n.height.mode = NkSizeMode::Fixed;
				n.height.value = 80.f;
				NkRemplissage f;
				f.couleur = NkString("#334455");
				n.fills.PushBack(f);
				NkBordure b;
				b.couleur = NkString("#ff8800");
				b.epaisseur = 4.f;
				b.position = NkBordurePos::Centre;
				b.jointure = NkString("rond");
				n.borders.PushBack(b);
			}
			const uint32 orange = 0xFF8800FFu;
			// le contour du trace tel que le peintre le voit (document : le layout pose le rect en 100,100)
			auto contourDe = [&](float32 *xy, uint32 &nb) {
				NkPaintRect sfc;
				sfc.x = 0.f;
				sfc.y = 0.f;
				sfc.w = 600.f;
				sfc.h = 400.f;
				NkLayoutResult lay;
				NkComputeLayout(dA, sfc, lay);
				nb = NkContourDe(dA.nodes[(uint32)ra], lay.At(ra), xy, 128u);
			};
			auto peindre = [&](PeintrePoly73 &pp) {
				pp.Vider();
				RenderDocument(pp, dA, NkPaintRect{0.f, 0.f, 600.f, 400.f});
			};
			auto distSeg = [](float32 px, float32 py, float32 ax, float32 ay, float32 bx, float32 by) {
				const float32 dx = bx - ax, dy = by - ay;
				const float32 l2 = dx * dx + dy * dy;
				float32 t = l2 > 0.f ? ((px - ax) * dx + (py - ay) * dy) / l2 : 0.f;
				t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
				const float32 qx = ax + dx * t - px, qy = ay + dy * t - py;
				return NkLongueur2D(qx, qy);
			};
			auto distContour = [&](float32 px, float32 py, const float32 *xy, uint32 nb) {
				float32 m = 1e9f;
				for (uint32 i = 0; i < nb; ++i) {
					const uint32 j = (i + 1u) % nb;
					const float32 d = distSeg(px, py, xy[i * 2], xy[i * 2 + 1], xy[j * 2], xy[j * 2 + 1]);
					if (d < m)
						m = d;
				}
				return m;
			};
			auto dedans = [](float32 px, float32 py, const float32 *xy, uint32 nb) {
				bool in = false;
				for (uint32 i = 0, j = nb - 1u; i < nb; j = i++) {
					const float32 xi = xy[i * 2], yi = xy[i * 2 + 1], xj = xy[j * 2], yj = xy[j * 2 + 1];
					if (((yi > py) != (yj > py)) && (px < (xj - xi) * (py - yi) / (yj - yi) + xi))
						in = !in;
				}
				return in;
			};
			auto convexe = [](const float32 *q, int32 n) {
				int32 signe = 0;
				for (int32 k = 0; k < n; ++k) {
					const int32 j = (k + 1) % n, l = (k + 2) % n;
					const float32 c = (q[j * 2] - q[k * 2]) * (q[l * 2 + 1] - q[j * 2 + 1]) - (q[j * 2 + 1] - q[k * 2 + 1]) * (q[l * 2] - q[j * 2]);
					if (c > -1e-3f && c < 1e-3f)
						continue;
					const int32 s = c > 0.f ? 1 : -1;
					if (signe == 0)
						signe = s;
					else if (s != signe)
						return false;
				}
				return true;
			};
			// l'inventaire des pieces orange d'une course
			struct Bilan { uint32 quads, coins, autres; float32 epMin, epMax, distMax; bool convexes, dedansTout, dehorsTout; uint32 minPtsCoin, maxPtsCoin; };
			auto bilan = [&](PeintrePoly73 &pp, const float32 *xy, uint32 nb, float32 dMax) {
				Bilan b{0u, 0u, 0u, 1e9f, -1e9f, 0.f, true, true, true, 999u, 0u};
				uint32 off = 0u;
				for (uint32 i = 0; i < (uint32)pp.tailles.Size(); ++i) {
					const int32 n = pp.tailles[i];
					const float32 *q = pp.pts.Data() + off;
					off += (uint32)n * 2u;
					if (pp.couleurs[i] != orange)
						continue;
					if (!convexe(q, n))
						b.convexes = false;
					for (int32 k = 0; k < n; ++k) {
						const float32 d = distContour(q[k * 2], q[k * 2 + 1], xy, nb);
						if (d > b.distMax)
							b.distMax = d;
						const bool in = dedans(q[k * 2], q[k * 2 + 1], xy, nb);
						if (d > 0.05f && !in)
							b.dedansTout = false;
						if (d > 0.05f && in)
							b.dehorsTout = false;
					}
					if (n == 4) {
						// un quadrilatere d'arete : la distance du 3e point a la droite des deux premiers = l'epaisseur
						const float32 ax = q[0], ay = q[1], bx = q[2], by = q[3], cx = q[4], cy = q[5];
						const float32 dx = bx - ax, dy = by - ay, L = NkLongueur2D(dx, dy);
						const float32 ep = L > 0.f ? ((cx - ax) * dy - (cy - ay) * dx) / L : 0.f;
						const float32 e = ep < 0.f ? -ep : ep;
						// un coin d'onglet a aussi quatre points : on le distingue par sa taille (< 2 epaisseurs)
						float32 diag = NkLongueur2D(q[4] - q[0], q[5] - q[1]);
						if (diag < 2.f * dMax + 1.f) {
							++b.coins;
							if ((uint32)n < b.minPtsCoin) b.minPtsCoin = (uint32)n;
							if ((uint32)n > b.maxPtsCoin) b.maxPtsCoin = (uint32)n;
						} else {
							++b.quads;
							if (e < b.epMin) b.epMin = e;
							if (e > b.epMax) b.epMax = e;
						}
					} else if (n >= 3) {
						++b.coins;
						if ((uint32)n < b.minPtsCoin) b.minPtsCoin = (uint32)n;
						if ((uint32)n > b.maxPtsCoin) b.maxPtsCoin = (uint32)n;
					} else
						++b.autres;
				}
				return b;
			};
			// 73a. rect deforme (le coin bas-droit tire), centree, ronde
			NkMaterialiserSommets(dA.nodes[(uint32)ra]);
			dA.nodes[(uint32)ra].sommets[2].x = 1.6f;
			float32 cxy[256];
			uint32 cnb = 0u;
			contourDe(cxy, cnb);
			PeintrePoly73 pp;
			peindre(pp);
			const Bilan bR = bilan(pp, cxy, cnb, 4.f);
			const bool rondOk = cnb == 4u && bR.quads == 4u && bR.coins == 4u && bR.autres == 0u && bR.epMin > 3.95f && bR.epMax < 4.05f
								&& bR.minPtsCoin >= 5u && bR.convexes && bR.distMax < 2.05f;
			// 73b. onglet : quatre points par coin ; biseau : trois
			dA.nodes[(uint32)ra].borders[0].jointure = NkString("onglet");
			peindre(pp);
			const Bilan bO = bilan(pp, cxy, cnb, 4.f);
			dA.nodes[(uint32)ra].borders[0].jointure = NkString("biseau");
			peindre(pp);
			const Bilan bB = bilan(pp, cxy, cnb, 4.f);
			const bool jointuresOk = bO.coins == 4u && bO.minPtsCoin == 4u && bO.maxPtsCoin == 4u && bO.quads == 4u && bB.coins == 4u
									 && bB.minPtsCoin == 3u && bB.maxPtsCoin == 3u && bB.quads == 4u;
			// 73c. concave (le coin haut-droit rentre) : chaque piece convexe, toutes a moins d'une demi-epaisseur du contour
			dA.nodes[(uint32)ra].borders[0].jointure = NkString("rond");
			dA.nodes[(uint32)ra].sommets[1].x = 0.2f;
			dA.nodes[(uint32)ra].sommets[1].y = -0.2f;
			contourDe(cxy, cnb);
			peindre(pp);
			const Bilan bC = bilan(pp, cxy, cnb, 4.f);
			const bool concaveOk = bC.quads == 4u && bC.coins == 4u && bC.convexes && bC.distMax < 2.05f && bC.epMin > 3.95f && bC.epMax < 4.05f;
			// 73d. interieure : tout dedans (a 4 px au plus) ; exterieure : tout dehors
			dA.nodes[(uint32)ra].borders[0].position = NkBordurePos::Interieur;
			peindre(pp);
			const Bilan bI = bilan(pp, cxy, cnb, 4.f);
			dA.nodes[(uint32)ra].borders[0].position = NkBordurePos::Exterieur;
			peindre(pp);
			const Bilan bE = bilan(pp, cxy, cnb, 4.f);
			const bool positionsOk = bI.dedansTout && bI.distMax < 4.05f && bI.quads == 4u && bE.dehorsTout && bE.distMax < 4.05f && bE.quads == 4u;
			// 73e. l'arrondi voyage : un rect de rayon 12 materialise garde 12 a chaque sommet, son contour a des arcs
			const int32 rb = dA.AddChild(0, "", NkAuthor::Humain);
			dA.nodes[(uint32)rb].shape = NkString("rect");
			dA.nodes[(uint32)rb].radius = 12.f;
			dA.nodes[(uint32)rb].width.mode = NkSizeMode::Fixed;
			dA.nodes[(uint32)rb].width.value = 100.f;
			dA.nodes[(uint32)rb].height.mode = NkSizeMode::Fixed;
			dA.nodes[(uint32)rb].height.value = 60.f;
			NkMaterialiserSommets(dA.nodes[(uint32)rb]);
			bool rayons = dA.nodes[(uint32)rb].sommets.Size() == 4u;
			for (uint32 i = 0; rayons && i < 4u; ++i)
				rayons = dA.nodes[(uint32)rb].sommets[i].rayon == 12.f;
			float32 bxy[256];
			uint32 bnb = 0u;
			{
				NkPaintRect sfc;
				sfc.x = 0.f;
				sfc.y = 0.f;
				sfc.w = 600.f;
				sfc.h = 400.f;
				NkLayoutResult lay;
				NkComputeLayout(dA, sfc, lay);
				bnb = NkContourDe(dA.nodes[(uint32)rb], lay.At(rb), bxy, 128u);
			}
			const bool arrondiOk = rayons && bnb > 8u;
			snprintf(det, sizeof(det),
					 "ronde : %u quads (ep %.2f..%.2f), %u coins (%u..%u pts), convexes=%d, dist max %.2f ; onglet : %u coins de %u pts ; biseau : %u coins de %u pts ; "
					 "concave : %u quads, %u coins, convexes=%d, dist max %.2f, ep %.2f..%.2f ; interieure : dedans=%d (dist %.2f) ; exterieure : dehors=%d (dist %.2f) ; "
					 "arrondi 12 -> sommets 12=%d, contour %u pts",
					 bR.quads, bR.epMin, bR.epMax, bR.coins, bR.minPtsCoin, bR.maxPtsCoin, bR.convexes ? 1 : 0, bR.distMax, bO.coins, bO.maxPtsCoin, bB.coins,
					 bB.maxPtsCoin, bC.quads, bC.coins, bC.convexes ? 1 : 0, bC.distMax, bC.epMin, bC.epMax, bI.dedansTout ? 1 : 0, bI.distMax,
					 bE.dehorsTout ? 1 : 0, bE.distMax, rayons ? 1 : 0, bnb);
			check("73. L'ANNEAU D'UN TRACE EDITE : un rect deforme + bordure 4 px centree = quatre quadrilateres de 4 px mesures aux "
				  "normales et quatre coins (arcs en « rond », quatre points en « onglet », trois en « biseau ») ; un contour concave "
				  "-> chaque piece convexe et a moins d'une demi-epaisseur du contour (pas d'auto-croisement) ; interieure tout "
				  "dedans, exterieure tout dehors ; l'arrondi d'un rect voyage avec ses sommets a la materialisation",
				  rondOk && jointuresOk && concaveOk && positionsOk && arrondiOk, det);
		}
		// ── 74. LE MODE EDITION SOUS LA MATRICE (Rodolf, 05/09 : « le degrade deborde du trace
		//    edite » sur un rect tourne de 30,9°) : sur la VRAIE toile (NkGuiComponentPaint),
		//    le contour d'edition et les poignees se dessinent dans le meme repere que le
		//    remplissage -- leurs etendues coincident ; et un clic sur la position ECRAN d'un
		//    sommet du rect tourne le prend (la souris est ramenee par l'inverse).
		{
			static nkgui::NkGuiContext ctxE;
			char det[520];
			if (!ctxE.Init(600, 900)) {
				check("74. le mode edition sous la matrice", false, "Init a refuse");
			} else {
				static DesignState stE;
				stE.doc.NewDocument("Toile", NkAuthor::Humain);
				const int32 pg = stE.doc.AddChild(0, "", NkAuthor::Humain);
				stE.doc.nodes[(uint32)pg].shape = NkString("frame");
				stE.doc.nodes[(uint32)pg].layout.kind = NkLayoutKind::Free;
				stE.doc.nodes[(uint32)pg].width.mode = NkSizeMode::Fixed;
				stE.doc.nodes[(uint32)pg].width.value = 400.f;
				stE.doc.nodes[(uint32)pg].height.mode = NkSizeMode::Fixed;
				stE.doc.nodes[(uint32)pg].height.value = 300.f;
				const int32 rc = stE.doc.AddChild(pg, "", NkAuthor::Humain);
				{
					NkUINode &n = stE.doc.nodes[(uint32)rc];
					n.shape = NkString("rect");
					n.posX = 100.f;
					n.posY = 80.f;
					n.width.mode = NkSizeMode::Fixed;
					n.width.value = 160.f;
					n.height.mode = NkSizeMode::Fixed;
					n.height.value = 80.f;
					n.rotation = 30.f;
					NkRemplissage f;
					f.couleur = NkString("#123456");
					n.fills.PushBack(f);
					NkMaterialiserSommets(n);
					n.sommets[1].x = 0.4f; // le coin haut-droit coupe : un trace a coin coupe, tourne
				}
				stE.Recompute(NkPaintRect{0.f, 0.f, 600.f, 900.f});
				stE.SelectSingle(rc);
				stE.modeForme.noeud = rc; // le mode edition de forme
				static PreviewPanel toileE(&stE);
				NkEditorFrameContext ec;
				ec.ui = &ctxE;
				ec.dt = 0.016f;
				auto image = [&](float32 mx, float32 my, bool bas) {
					ctxE.input.mousePos = {mx, my};
					ctxE.input.mouseDown[0] = bas;
					ctxE.BeginFrame(0.016f);
					ctxE.BeginLayout({0.f, 0.f, 600.f, 900.f});
					toileE.OnUI(ec);
					ctxE.EndFrame();
				};
				for (int32 k = 0; k < 4; ++k)
					image(-1.f, -1.f, false);
				// les POIGNEES d'edition (accent) sont a la position ECRAN TRANSFORMEE de chaque
				// sommet -- et pas a sa position non tournee (l'ancien dessin) ; l'accent peint
				// aussi d'autre chose (le cadre de selection, la page) : on mesure PRES des
				// sommets, pas une etendue
				const uint32 accent = nkgui::NkGuiPackColor(nkentseu::editorkit::NkThemeUnpack(stE.theme.Get(NkDesignResolveRole("accent_ui"))));
				NkLayoutResult scr0;
				stE.ProjectToScreen(scr0);
				const NkPaintRect rs0 = scr0.At(rc);
				float32 sx0[64];
				const uint32 nbS0 = NkSommetsDe(stE.doc.nodes[(uint32)rc], rs0, sx0, 32);
				const NkMat2D mE = NkMatEffective(stE.doc, scr0, rc);
				auto accentPres = [&](float32 x, float32 y) {
					uint32 n = 0u;
					for (uint32 i = 0; i < (uint32)ctxE.dl.vtx.Size(); ++i)
						if (ctxE.dl.vtx[i].col == accent && NkLongueur2D(ctxE.dl.vtx[i].pos.x - x, ctxE.dl.vtx[i].pos.y - y) <= 6.f)
							++n;
					return n;
				};
				uint32 presTournes = 0u, presDroits = 0u;
				for (uint32 i = 0; i < nbS0 && i < 4u; ++i) {
					float32 tx = sx0[i * 2], ty = sx0[i * 2 + 1];
					NkMatPoint(mE, tx, ty);
					if (accentPres(tx, ty) > 0u)
						++presTournes;
					// la position NON tournee : loin du sommet tourne (> 12 px) et sans poignee
					if (NkLongueur2D(tx - sx0[i * 2], ty - sx0[i * 2 + 1]) > 12.f && accentPres(sx0[i * 2], sx0[i * 2 + 1]) > 0u)
						++presDroits;
				}
				const bool memeRepere = nbS0 == 4u && presTournes == 4u && presDroits == 0u;
				const uint32 nF = presTournes, nA = presDroits;
				const float32 fx0 = 0.f, fx1 = 0.f, fy0 = 0.f, fy1 = 0.f, ax0 = 0.f, ax1 = 0.f, ay0 = 0.f, ay1 = 0.f;
				// un sommet tourne se prend a sa position ECRAN : le sommet 0 (haut-gauche local)
				NkLayoutResult scr;
				stE.ProjectToScreen(scr);
				const NkPaintRect rs = scr.At(rc);
				float32 sx[64];
				const uint32 nbS = NkSommetsDe(stE.doc.nodes[(uint32)rc], rs, sx, 32);
				float32 px = nbS ? sx[0] : 0.f, py = nbS ? sx[1] : 0.f;
				NkMatPoint(NkMatEffective(stE.doc, scr, rc), px, py);
				image(px, py, false);
				image(px, py, true);
				const int32 tire = stE.modeForme.tire;
				image(px, py, false);
				image(-1.f, -1.f, false);
				// le meme clic a la position NON tournee du sommet (l'ancien dessin) ne prend rien
				stE.modeForme.tire = -1;
				stE.modeForme.sommet = -1;
				// 40 px au-dela du coin NON tourne, loin de tout cote du contour tourne (mesure :
				// un clic a moins de 12 px d'un cote y AJOUTE un sommet, et c'est voulu)
				const float32 qx = nbS ? sx[0] - 40.f : 0.f, qy = nbS ? sx[1] - 40.f : 0.f;
				float32 ctT[64];
				for (uint32 i = 0; i < nbS * 2u; ++i)
					ctT[i] = sx[i];
				NkMatContour(NkMatEffective(stE.doc, scr, rc), ctT, nbS);
				float32 tL = 0.f, dLoin = 0.f;
				(void)NkSegmentLePlusProche(ctT, nbS, qx, qy, tL, dLoin);
				const float32 ecart = dLoin;
				image(qx, qy, false);
				image(qx, qy, true);
				const int32 tireDroit = stE.modeForme.tire;
				image(qx, qy, false);
				image(-1.f, -1.f, false);
				stE.modeForme.Quitter();
				(void)fx0; (void)fx1; (void)fy0; (void)fy1; (void)ax0; (void)ax1; (void)ay0; (void)ay1;
				snprintf(det, sizeof(det), "poignees pres des 4 sommets TOURNES : %u/4 ; pres des positions non tournees : %u (attendu 0) ; "
										   "clic sur la position ecran du sommet 0 (%.0f,%.0f) -> tire=%d ; loin du contour tourne (%.0f,%.0f, a %.0f px du cote le plus proche) -> tire=%d",
						 nF, nA, px, py, tire, qx, qy, ecart, tireDroit);
				check("74. LE MODE EDITION SOUS LA MATRICE, sur la vraie toile : un rect tourne de 30° au coin coupe -- le "
					  "poignees d'edition sont aux positions ECRAN tournees des sommets, aucune aux positions droites ; un clic "
					  "sur la position ECRAN d'un sommet le prend, un clic loin du contour tourne ne prend rien",
					  memeRepere && tire == 0 && tireDroit < 0 && ecart > 20.f, det);
			}
		}
		// ── 75. ② LES POIGNEES D'EDITION SE PRENNENT A 12 PX (la tolerance nommee) : un clic a
		//    10 px d'un sommet le prend, a 10 px d'un cote en ajoute un ; a 16 px, rien.
		{
			static nkgui::NkGuiContext ctxT;
			char det[400];
			if (!ctxT.Init(600, 900)) {
				check("75. la tolerance des poignees d'edition", false, "Init a refuse");
			} else {
				static DesignState stT;
				stT.doc.NewDocument("Toile", NkAuthor::Humain);
				const int32 pg = stT.doc.AddChild(0, "", NkAuthor::Humain);
				stT.doc.nodes[(uint32)pg].shape = NkString("frame");
				stT.doc.nodes[(uint32)pg].layout.kind = NkLayoutKind::Free;
				stT.doc.nodes[(uint32)pg].width.mode = NkSizeMode::Fixed;
				stT.doc.nodes[(uint32)pg].width.value = 400.f;
				stT.doc.nodes[(uint32)pg].height.mode = NkSizeMode::Fixed;
				stT.doc.nodes[(uint32)pg].height.value = 300.f;
				const int32 rc = stT.doc.AddChild(pg, "", NkAuthor::Humain);
				{
					NkUINode &n = stT.doc.nodes[(uint32)rc];
					n.shape = NkString("rect");
					n.posX = 100.f;
					n.posY = 80.f;
					n.width.mode = NkSizeMode::Fixed;
					n.width.value = 160.f;
					n.height.mode = NkSizeMode::Fixed;
					n.height.value = 80.f;
					NkRemplissage f;
					f.couleur = NkString("#123456");
					n.fills.PushBack(f);
				}
				stT.Recompute(NkPaintRect{0.f, 0.f, 600.f, 900.f});
				stT.SelectSingle(rc);
				stT.modeForme.noeud = rc;
				static PreviewPanel toileT(&stT);
				NkEditorFrameContext ec;
				ec.ui = &ctxT;
				ec.dt = 0.016f;
				auto image = [&](float32 mx, float32 my, bool bas) {
					ctxT.input.mousePos = {mx, my};
					ctxT.input.mouseDown[0] = bas;
					ctxT.BeginFrame(0.016f);
					ctxT.BeginLayout({0.f, 0.f, 600.f, 900.f});
					toileT.OnUI(ec);
					ctxT.EndFrame();
				};
				auto clic = [&](float32 x, float32 y) {
					image(-1.f, -1.f, false);
					image(x, y, false);
					image(x, y, true);
					const int32 t = stT.modeForme.tire;
					image(x, y, false);
					image(-1.f, -1.f, false);
					return t;
				};
				for (int32 k = 0; k < 3; ++k)
					image(-1.f, -1.f, false);
				NkLayoutResult scr;
				stT.ProjectToScreen(scr);
				const NkPaintRect rs = scr.At(rc);
				float32 sx[64];
				const uint32 nbS = NkSommetsDe(stT.doc.nodes[(uint32)rc], rs, sx, 32);
				// le sommet 0 (haut-gauche) : a 10 px en diagonale hors du coin (7 px sur chaque axe)
				const int32 aExact = clic(sx[0], sx[1]);
				const int32 noeudApres0 = stT.modeForme.noeud, selApres0 = stT.selected;
				stT.modeForme.tire = -1;
				stT.modeForme.noeud = rc;
				stT.SelectSingle(rc);
				image(-1.f, -1.f, false);
				const int32 a10 = clic(sx[0] - 7.f, sx[1] - 7.f);
				const int32 noeudApres = stT.modeForme.noeud, selApres = stT.selected;
				stT.modeForme.tire = -1;
				stT.modeForme.noeud = rc;
				stT.SelectSingle(rc);
				image(-1.f, -1.f, false);
				// a 28 px sur la diagonale (20 par axe : loin du sommet ET des deux cotes) : rien
				const int32 a16 = clic(sx[0] - 20.f, sx[1] - 20.f);
				stT.modeForme.tire = -1;
				// un cote : le milieu du haut, a 10 px au-dessus -> un sommet est ajoute (5 sommets)
				const uint32 avant = (uint32)stT.doc.nodes[(uint32)rc].sommets.Size();
				stT.status = NkString();
				float32 tC = 0.f, dC = 0.f;
				const int32 segC = NkSegmentLePlusProche(sx, nbS, rs.x + rs.w * 0.5f, rs.y - 10.f, tC, dC);
				const int32 aCote = clic(rs.x + rs.w * 0.5f, rs.y - 10.f);
				const uint32 apres = (uint32)stT.doc.nodes[(uint32)rc].sommets.Size();
				const NkString statusCote = stT.status;
				stT.modeForme.Quitter();
				snprintf(det, sizeof(det), "%u sommets (sommet 0 a %.0f,%.0f) ; exact -> tire=%d (mode %d sel %d) ; a 10 px -> tire=%d (mode %d sel %d) ; a 28 px -> tire=%d ; a 10 px du cote haut (segment %d a %.1f px) -> tire=%d, sommets %u -> %u, pied « %s »",
						 nbS, sx[0], sx[1], aExact, noeudApres0, selApres0, a10, noeudApres, selApres, a16, segC, dC, aCote, avant, apres, statusCote.Data() ? statusCote.Data() : "");
				check("75. ② LES POIGNEES D'EDITION SE PRENNENT A 12 PX (la tolerance nommee, la meme que les poignees de forme et de "
					  "degrade) : a 10 px d'un sommet il est pris, a 16 px rien ; a 10 px d'un cote un sommet s'ajoute",
					  nbS == 4u && a10 == 0 && a16 < 0 && aCote >= 0 && apres == avant + 1u + (avant == 0u ? 4u : 0u), det);
			}
		}
		// ── 76. ④ LA MOLETTE N'ATTEINT PAS LA TOILE SOUS UN MENU OU UN POPUP (Rodolf : « le scroll
		//    de la molette affecte le canvas infini a l'arriere ») : menu contextuel ouvert, molette
		//    hors du menu -> le zoom de la toile ne bouge pas (NKGui l'a mise de cote) ; menu
		//    ferme -> la molette zoome (le temoin n'est pas vide) ; popover NKGui ouvert -> pareil.
		{
			static nkgui::NkGuiContext ctxM;
			char det[420];
			if (!ctxM.Init(600, 900)) {
				check("76. la molette sous un menu", false, "Init a refuse");
			} else {
				static DesignState stM;
				stM.doc.NewDocument("Toile", NkAuthor::Humain);
				const int32 pg = stM.doc.AddChild(0, "", NkAuthor::Humain);
				stM.doc.nodes[(uint32)pg].shape = NkString("frame");
				stM.doc.nodes[(uint32)pg].layout.kind = NkLayoutKind::Free;
				stM.doc.nodes[(uint32)pg].width.mode = NkSizeMode::Fixed;
				stM.doc.nodes[(uint32)pg].width.value = 400.f;
				stM.doc.nodes[(uint32)pg].height.mode = NkSizeMode::Fixed;
				stM.doc.nodes[(uint32)pg].height.value = 300.f;
				const int32 rc = stM.doc.AddChild(pg, "", NkAuthor::Humain);
				stM.doc.nodes[(uint32)rc].shape = NkString("rect");
				stM.doc.nodes[(uint32)rc].posX = 100.f;
				stM.doc.nodes[(uint32)rc].posY = 80.f;
				stM.doc.nodes[(uint32)rc].width.mode = NkSizeMode::Fixed;
				stM.doc.nodes[(uint32)rc].width.value = 160.f;
				stM.doc.nodes[(uint32)rc].height.mode = NkSizeMode::Fixed;
				stM.doc.nodes[(uint32)rc].height.value = 80.f;
				NkRemplissage f;
				f.couleur = NkString("#123456");
				stM.doc.nodes[(uint32)rc].fills.PushBack(f);
				stM.Recompute(NkPaintRect{0.f, 0.f, 600.f, 900.f});
				stM.SelectSingle(rc);
				static PreviewPanel toileM(&stM);
				static InspectorPanel inspM(&stM);
				NkEditorFrameContext ec;
				ec.ui = &ctxM;
				ec.dt = 0.016f;
				auto image = [&](float32 mx, float32 my, bool gauche, bool droit, float32 molette) {
					ctxM.input.mousePos = {mx, my};
					ctxM.input.mouseDown[0] = gauche;
					ctxM.input.mouseDown[1] = droit;
					ctxM.input.wheel = molette; // ce que la fenetre injecte AVANT l'image
					ctxM.BeginFrame(0.016f);
					ctxM.BeginLayout({0.f, 0.f, 340.f, 900.f});
					toileM.OnUI(ec);
					ctxM.BeginLayout({340.f, 0.f, 260.f, 900.f});
					inspM.OnUI(ec);
					NkDessinerPickerDemande(ctxM, stM);
					ctxM.EndFrame();
				};
				for (int32 k = 0; k < 3; ++k)
					image(-1.f, -1.f, false, false, 0.f);
				// 1. sans menu : la molette zoome (le temoin n'est pas vide)
				const float32 z0 = stM.view.zoom;
				image(200.f, 400.f, false, false, 0.f);
				image(200.f, 400.f, false, false, 1.f);
				const float32 z1 = stM.view.zoom;
				// 2. clic droit sur le rectangle : le menu s'ouvre ; molette LOIN du menu -> le zoom tient
				NkLayoutResult scr;
				stM.ProjectToScreen(scr);
				const NkPaintRect rs = scr.At(rc);
				const float32 cx = rs.x + rs.w * 0.5f, cy = rs.y + rs.h * 0.5f;
				image(cx, cy, false, false, 0.f);
				image(cx, cy, false, true, 0.f);
				image(cx, cy, false, false, 0.f);
				const bool menuOuvert = toileM.MenuContextuelOuvert();
				const float32 z2 = stM.view.zoom;
				image(30.f, 850.f, false, false, 0.f); // le pointeur loin du menu (bas-gauche de la toile)
				image(30.f, 850.f, false, false, 1.f);
				image(30.f, 850.f, false, false, 1.f);
				const float32 z3 = stM.view.zoom;
				const bool reserveVue = ctxM.input.wheelReserve == 0.f; // consommee par le menu en fin d'image
				// 3. le menu se ferme (Echap) ; un popover NKGui (le selecteur de remplissage) : pareil
				ctxM.input.SetKey(nkgui::NkGuiKey::Escape, true);
				image(30.f, 850.f, false, false, 0.f);
				ctxM.input.SetKey(nkgui::NkGuiKey::Escape, false);
				image(30.f, 850.f, false, false, 0.f);
				const bool menuFerme = !toileM.MenuContextuelOuvert();
				stM.picker = DesignState::DemandePicker();
				stM.picker.ouvert = true;
				stM.picker.id = ctxM.GetId("##sonde.popover.molette");
				stM.picker.genre = 1u;
				stM.picker.noeud = rc;
				stM.picker.index = 0;
				stM.picker.ancre = {360.f, 200.f, 16.f, 16.f};
				image(-1.f, -1.f, false, false, 0.f);
				image(-1.f, -1.f, false, false, 0.f);
				const float32 z4 = stM.view.zoom;
				image(30.f, 850.f, false, false, 0.f);
				image(30.f, 850.f, false, false, 1.f);
				image(30.f, 850.f, false, false, 1.f);
				const float32 z5 = stM.view.zoom;
				if (ctxM.popupDepth > 0)
					ctxM.ClosePopup();
				stM.picker = DesignState::DemandePicker();
				image(-1.f, -1.f, false, false, 0.f);
				snprintf(det, sizeof(det), "sans menu : zoom %.3f -> %.3f ; menu ouvert=%d, molette loin du menu : zoom %.3f -> %.3f (reserve consommee=%d) ; menu ferme=%d ; popover ouvert, molette loin : zoom %.3f -> %.3f",
						 z0, z1, menuOuvert ? 1 : 0, z2, z3, reserveVue ? 1 : 0, menuFerme ? 1 : 0, z4, z5);
				check("76. ④ LA MOLETTE N'ATTEINT PAS LA TOILE SOUS UN MENU OU UN POPUP : sans menu elle zoome ; le menu contextuel "
					  "ouvert, la molette hors du menu ne zoome pas (NKGui la reserve au menu) ; le popover du selecteur ouvert, pareil",
					  z1 != z0 && menuOuvert && z3 == z2 && menuFerme && z5 == z4, det);
			}
		}
		// ── 77. ③ MAJ GARDE LES PROPORTIONS, LU PENDANT LE GLISSER (Lunacy : « Preserve Ratio:
		//    Shift + resize » -- lunacy.docs.icons8.com/shortcuts, lu le 05/09) : un coin tire avec
		//    Maj garde le rapport a 0,5 % pres, sans Maj il est libre ; le badge le dit ; la meme
		//    touche vaut pour la poignee d'un groupe (echelle X = echelle Y).
		{
			static nkgui::NkGuiContext ctxR;
			char det[420];
			if (!ctxR.Init(600, 900)) {
				check("77. le proportionnel", false, "Init a refuse");
			} else {
				static DesignState stR;
				stR.doc.NewDocument("Toile", NkAuthor::Humain);
				const int32 pg = stR.doc.AddChild(0, "", NkAuthor::Humain);
				stR.doc.nodes[(uint32)pg].shape = NkString("frame");
				stR.doc.nodes[(uint32)pg].layout.kind = NkLayoutKind::Free;
				stR.doc.nodes[(uint32)pg].width.mode = NkSizeMode::Fixed;
				stR.doc.nodes[(uint32)pg].width.value = 500.f;
				stR.doc.nodes[(uint32)pg].height.mode = NkSizeMode::Fixed;
				stR.doc.nodes[(uint32)pg].height.value = 400.f;
				auto rect = [&](int32 parent, float32 x, float32 y, float32 w, float32 h) {
					const int32 r = stR.doc.AddChild(parent, "", NkAuthor::Humain);
					NkUINode &n = stR.doc.nodes[(uint32)r];
					n.shape = NkString("rect");
					n.posX = x;
					n.posY = y;
					n.width.mode = NkSizeMode::Fixed;
					n.width.value = w;
					n.height.mode = NkSizeMode::Fixed;
					n.height.value = h;
					NkRemplissage f;
					f.couleur = NkString("#123456");
					n.fills.PushBack(f);
					return r;
				};
				const int32 rc = rect(pg, 40.f, 40.f, 160.f, 80.f);
				stR.Recompute(NkPaintRect{0.f, 0.f, 600.f, 900.f});
				stR.SelectSingle(rc);
				static PreviewPanel toileR(&stR);
				NkEditorFrameContext ec;
				ec.ui = &ctxR;
				ec.dt = 0.016f;
				auto image = [&](float32 mx, float32 my, bool bas, bool maj) {
					ctxR.input.mousePos = {mx, my};
					ctxR.input.mouseDown[0] = bas;
					ctxR.input.shiftDown = maj;
					ctxR.BeginFrame(0.016f);
					ctxR.BeginLayout({0.f, 0.f, 600.f, 900.f});
					toileR.OnUI(ec);
					ctxR.EndFrame();
				};
				bool badgeVu = false;
				int32 imagesEnRedim = 0;
				auto tirerCoin = [&](int32 noeud, float32 dx, float32 dy, bool maj) {
					imagesEnRedim = 0;
					for (int32 k = 0; k < 3; ++k)
						image(-1.f, -1.f, false, false);
					NkLayoutResult scr;
					stR.ProjectToScreen(scr);
					const NkPaintRect rs = scr.At(noeud);
					const float32 cx = rs.x + rs.w, cy = rs.y + rs.h; // le coin bas-droit
					image(cx, cy, false, false);
					image(cx, cy, true, false);
					for (int32 k = 1; k <= 4; ++k) {
						image(cx + dx * (float32)k / 4.f, cy + dy * (float32)k / 4.f, true, maj);
						if (toileR.EnRedimensionnement())
							++imagesEnRedim;
						if (toileR.BadgeToucheVu())
							badgeVu = true;
					}
					image(cx + dx, cy + dy, false, maj);
					image(-1.f, -1.f, false, false);
				};
				// a. sans Maj : libre (+40, +5 -> 200 x 85, rapport 2,35)
				tirerCoin(rc, 40.f, 5.f, false);
				const float32 wL = stR.doc.nodes[(uint32)rc].width.value, hL = stR.doc.nodes[(uint32)rc].height.value;
				const bool libre = wL > 190.f && wL < 210.f && hL > 80.f && hL < 90.f;
				const bool sansBadge = !badgeVu;
				const int32 redimL = imagesEnRedim;
				// b. avec Maj : le rapport 200/85 de depart tient a 0,5 %
				const float32 r0 = wL / hL;
				badgeVu = false;
				tirerCoin(rc, 40.f, 5.f, true);
				const float32 wP = stR.doc.nodes[(uint32)rc].width.value, hP = stR.doc.nodes[(uint32)rc].height.value;
				const float32 rP = wP / hP;
				const bool proportionnel = wP > wL + 5.f && rP > r0 * 0.995f && rP < r0 * 1.005f && badgeVu;
				const int32 redimP = imagesEnRedim;
				// c. un GROUPE (deux rectangles) : la poignee du groupe avec Maj -> echelle X = echelle Y
				const int32 g = stR.doc.AddChild(pg, "", NkAuthor::Humain);
				stR.doc.nodes[(uint32)g].genre = NkString("simple");
				stR.doc.nodes[(uint32)g].posX = 240.f;
				stR.doc.nodes[(uint32)g].posY = 200.f;
				stR.doc.nodes[(uint32)g].width.mode = NkSizeMode::Fixed;
				stR.doc.nodes[(uint32)g].width.value = 100.f;
				stR.doc.nodes[(uint32)g].height.mode = NkSizeMode::Fixed;
				stR.doc.nodes[(uint32)g].height.value = 100.f;
				rect(g, 0.f, 0.f, 100.f, 40.f);
				rect(g, 0.f, 60.f, 60.f, 40.f);
				stR.Recompute(NkPaintRect{0.f, 0.f, 600.f, 900.f});
				stR.SelectSingle(g);
				badgeVu = false;
				tirerCoin(g, 30.f, 6.f, true);
				const float32 ex = stR.doc.nodes[(uint32)g].echelleX, ey = stR.doc.nodes[(uint32)g].echelleY;
				const bool groupe = ex > 1.02f && ey > 0.995f * ex && ey < 1.005f * ex;
				snprintf(det, sizeof(det), "sans Maj : 160x80 -> %.1f x %.1f (libre=%d, badge=%d, %d images en redim.) ; avec Maj : -> %.1f x %.1f, rapport %.3f vs %.3f (badge vu=%d, %d images en redim.) ; groupe avec Maj : echelle %.3f x %.3f (%d images en redim.)",
						 wL, hL, libre ? 1 : 0, sansBadge ? 0 : 1, redimL, wP, hP, rP, r0, badgeVu ? 1 : 0, redimP, ex, ey, imagesEnRedim);
				check("77. ③ MAJ GARDE LES PROPORTIONS, lue PENDANT le glisser (Lunacy, cite) : sans Maj le coin est libre et "
					  "aucun badge ; avec Maj le rapport tient a 0,5 % et le badge « proportionnel » se voit ; la poignee d'un "
					  "GROUPE avec Maj donne echelle X = echelle Y",
					  libre && sansBadge && proportionnel && groupe, det);
			}
		}
		// ── 78. ⑤ LA HIERARCHIE D'UN COMPOSANT (Rodolf : « pourquoi je ne peux pas voir la hierarchie
		//    d'un composant ? ») : une instance a trois enfants montre trois lignes sous elle,
		//    marquee « instance » ; un enfant s'ecrit sans toucher la declaration (surcharge) ;
		//    « Voir le composant » liste la declaration dans le panneau.
		{
			static nkgui::NkGuiContext ctxH;
			char det[400];
			if (!ctxH.Init(600, 900)) {
				check("78. la hierarchie d'un composant", false, "Init a refuse");
			} else {
				static DesignState stH;
				stH.doc.NewDocument("Toile", NkAuthor::Humain);
				const int32 pg = stH.doc.AddChild(0, "", NkAuthor::Humain);
				stH.doc.nodes[(uint32)pg].shape = NkString("frame");
				stH.doc.nodes[(uint32)pg].layout.kind = NkLayoutKind::Free;
				stH.doc.nodes[(uint32)pg].width.mode = NkSizeMode::Fixed;
				stH.doc.nodes[(uint32)pg].width.value = 400.f;
				stH.doc.nodes[(uint32)pg].height.mode = NkSizeMode::Fixed;
				stH.doc.nodes[(uint32)pg].height.value = 300.f;
				const int32 carte = stH.doc.AddChild(pg, "", NkAuthor::Humain);
				stH.doc.nodes[(uint32)carte].genre = NkString("simple");
				stH.doc.nodes[(uint32)carte].label = NkString("Carte");
				for (int32 k = 0; k < 3; ++k) {
					const int32 t = stH.doc.AddChild(carte, "", NkAuthor::Humain);
					stH.doc.nodes[(uint32)t].shape = NkString("text");
					char lb[16];
					snprintf(lb, sizeof(lb), "Ligne %d", k + 1);
					stH.doc.nodes[(uint32)t].label = NkString(lb);
					stH.doc.nodes[(uint32)t].text = NkString(lb);
				}
				const int32 decl = stH.doc.ExtraireComposant(carte, "", "carte");
				const int32 inst = stH.doc.InstancierComposant(decl, pg);
				static HierarchyPanel hierH(&stH);
				NkEditorFrameContext ec;
				ec.ui = &ctxH;
				ec.dt = 0.016f;
				auto image = [&]() {
					ctxH.input.mousePos = {-1.f, -1.f};
					ctxH.input.mouseDown[0] = false;
					ctxH.BeginFrame(0.016f);
					ctxH.BeginLayout({0.f, 0.f, 260.f, 900.f});
					hierH.OnUI(ec);
					ctxH.EndFrame();
				};
				image();
				image();
				// l'instance dans l'arbre, et ses enfants
				const NkTreeViewModel &m = hierH.ModelePages();
				int32 ligneInst = -1;
				uint32 enfants = 0u;
				bool marqueeInstance = false;
				for (uint32 i = 0; i < (uint32)m.nodes.Size(); ++i)
					if ((int32)m.nodes[i].id - 1 == inst) {
						ligneInst = (int32)i;
						marqueeInstance = m.nodes[i].kindLabel && NkComponentDecl::StrEq(m.nodes[i].kindLabel, "instance");
					}
				for (uint32 i = 0; i < (uint32)m.nodes.Size(); ++i)
					if (ligneInst >= 0 && m.nodes[i].parent == ligneInst)
						++enfants;
				// un enfant de l'instance s'ecrit : la declaration ne bouge pas (surcharge)
				bool surcharge = false;
				if (stH.doc.IsValidIndex(inst) && stH.doc.nodes[(uint32)inst].children.Size() == 3u) {
					const int32 e0 = stH.doc.nodes[(uint32)inst].children[0];
					stH.doc.nodes[(uint32)e0].text = NkString("Surcharge");
					stH.doc.MarkHumanEdit(e0);
					bool declIntacte = true;
					for (uint32 k = 0; k < (uint32)stH.doc.declarations[(uint32)decl].arbre.Size(); ++k)
						if (NkComponentDecl::StrEq(stH.doc.declarations[(uint32)decl].arbre[k].text.Data(), "Surcharge"))
							declIntacte = false;
					surcharge = declIntacte;
				}
				// « Voir le composant » : la declaration se liste (4 lignes : la carte et ses trois textes)
				const bool vu = NkAppliquerActionCtx(stH, inst, NkActionCtx::VoirComposant) && stH.composantVu == decl;
				image();
				const uint32 lignes = hierH.LignesComposantVu();
				snprintf(det, sizeof(det), "declaration %d, instance %d a %u enfants dans le document ; l'arbre : ligne de l'instance %d (marquee instance=%d), %u enfants sous elle ; "
										   "ecriture sur un enfant : declaration intacte=%d ; « Voir le composant » -> %d, %u lignes listees",
						 decl, inst, stH.doc.IsValidIndex(inst) ? (uint32)stH.doc.nodes[(uint32)inst].children.Size() : 0u, ligneInst, marqueeInstance ? 1 : 0, enfants,
						 surcharge ? 1 : 0, vu ? 1 : 0, lignes);
				check("78. ⑤ LA HIERARCHIE D'UN COMPOSANT : une instance a trois enfants les montre sous elle dans l'arbre, marquee "
					  "« instance » ; ecrire sur un enfant est une SURCHARGE (la declaration ne bouge pas) ; « Voir le composant » "
					  "liste la declaration (quatre lignes) dans le panneau, en lecture seule",
					  decl >= 0 && inst >= 0 && ligneInst >= 0 && marqueeInstance && enfants == 3u && surcharge && vu && lignes == 4u, det);
			}
		}
		// ── 79. LA POIGNEE D'UN GROUPE, SUR LE DOCUMENT DE RODOLF : ses six groupes (enveloppes
		//    le 03/09) -> la poignee s'arme, l'echelle change, un enfant texte grandit a l'ecran ;
		//    et un groupe fait a la main SANS taille fixe s'arme aussi (sa boite englobante).
		{
			static nkgui::NkGuiContext ctxG;
			char det[900];
			if (!ctxG.Init(600, 900)) {
				check("79. la poignee d'un groupe", false, "Init a refuse");
			} else {
				static DesignState stG;
				NkString contenu;
				FILE *fp = fopen("nkuidesign_document.nkuidoc", "rb");
				if (fp) {
					fseek(fp, 0, SEEK_END);
					const long taille = ftell(fp);
					fseek(fp, 0, SEEK_SET);
					if (taille > 0) {
						char *buf = new char[(size_t)taille + 1];
						const size_t lu = fread(buf, 1, (size_t)taille, fp);
						buf[lu] = 0;
						contenu = NkString(buf);
						delete[] buf;
					}
					fclose(fp);
				}
				stG.doc.NewDocument("Toile", NkAuthor::Humain);
				const bool charge = !contenu.Empty() && stG.doc.Load(contenu.Data());
				if (charge)
					(void)NkEnvelopperGraphiques(stG.doc, nullptr);
				stG.Recompute(NkPaintRect{0.f, 0.f, 600.f, 900.f});
				static PreviewPanel toileG(&stG);
				NkEditorFrameContext ec;
				ec.ui = &ctxG;
				ec.dt = 0.016f;
				auto image = [&](float32 mx, float32 my, bool bas, bool maj) {
					ctxG.input.mousePos = {mx, my};
					ctxG.input.mouseDown[0] = bas;
					ctxG.input.shiftDown = maj;
					ctxG.BeginFrame(0.016f);
					ctxG.BeginLayout({0.f, 0.f, 600.f, 900.f});
					toileG.OnUI(ec);
					ctxG.EndFrame();
				};
				auto largeurEcran = [&](int32 noeud) -> float32 {
					NkLayoutResult scr;
					stG.ProjectToScreen(scr);
					if (!scr.Has(noeud))
						return 0.f;
					const NkPaintRect r = scr.At(noeud);
					float32 c[8] = {r.x, r.y, r.x + r.w, r.y, r.x + r.w, r.y + r.h, r.x, r.y + r.h};
					NkMatContour(NkMatEffective(stG.doc, scr, noeud), c, 4u);
					float32 x0 = c[0], x1 = c[0];
					for (uint32 i = 1; i < 4u; ++i) {
						if (c[i * 2] < x0) x0 = c[i * 2];
						if (c[i * 2] > x1) x1 = c[i * 2];
					}
					return x1 - x0;
				};
				auto tirer = [&](int32 noeud, float32 dx, float32 dy, int32 &imagesArmees) {
					imagesArmees = 0;
					for (int32 k = 0; k < 3; ++k)
						image(-1.f, -1.f, false, false);
					NkLayoutResult scr;
					stG.ProjectToScreen(scr);
					if (!scr.Has(noeud))
						return;
					const NkPaintRect rs = scr.At(noeud);
					float32 c[2] = {rs.x + rs.w, rs.y + rs.h};
					NkMatContour(NkMatEffective(stG.doc, scr, noeud), c, 1u); // le coin bas-droit, a l'ecran
					image(c[0], c[1], false, false);
					image(c[0], c[1], true, false);
					for (int32 k = 1; k <= 4; ++k) {
						image(c[0] + dx * (float32)k / 4.f, c[1] + dy * (float32)k / 4.f, true, true);
						if (toileG.EnRedimensionnement())
							++imagesArmees;
					}
					image(c[0] + dx, c[1] + dy, false, true);
					image(-1.f, -1.f, false, false);
				};
				auto texteSous = [&](int32 g) -> int32 {
					// le premier texte descendant (profondeur 2 au plus)
					const NkVector<int32> &ch = stG.doc.nodes[(uint32)g].children;
					for (uint32 i = 0; i < (uint32)ch.Size(); ++i) {
						if (NkComponentDecl::StrEq(stG.doc.nodes[(uint32)ch[i]].shape.Data(), "text"))
							return ch[i];
						const NkVector<int32> &pch = stG.doc.nodes[(uint32)ch[i]].children;
						for (uint32 j = 0; j < (uint32)pch.Size(); ++j)
							if (NkComponentDecl::StrEq(stG.doc.nodes[(uint32)pch[j]].shape.Data(), "text"))
								return pch[j];
					}
					return -1;
				};
				static const char *const kLibs[6] = {"Bouton_Connexion", "Panel_Nav", "Carte_Actifs", "Carte_Revenu", "Carte_Attrition", "Graphique"};
				uint32 trouves = 0u, fixes = 0u, armes = 0u, agrandis = 0u, textesGrandis = 0u, textesVus = 0u;
				char resume[520];
				resume[0] = 0;
				size_t pos = 0;
				for (uint32 l = 0; l < 6u && charge; ++l) {
					int32 g = -1;
					for (uint32 i = 1; i < (uint32)stG.doc.nodes.Size() && g < 0; ++i)
						if (!stG.doc.nodes[i].children.Empty() && NkComponentDecl::StrEq(stG.doc.nodes[i].label.Data(), kLibs[l]))
							g = (int32)i;
					if (g < 0) {
						pos += (size_t)snprintf(resume + pos, sizeof(resume) - pos, "%s absent ; ", kLibs[l]);
						continue;
					}
					++trouves;
					const NkUINode &gn = stG.doc.nodes[(uint32)g];
					const bool fixe = gn.width.mode == NkSizeMode::Fixed && gn.height.mode == NkSizeMode::Fixed;
					if (fixe)
						++fixes;
					const int32 t = texteSous(g);
					const float32 wT0 = t >= 0 ? largeurEcran(t) : 0.f;
					stG.SelectSingle(g);
					int32 armees = 0;
					tirer(g, 30.f, 30.f, armees);
					const float32 ech = stG.doc.nodes[(uint32)g].echelleX;
					const float32 wT1 = t >= 0 ? largeurEcran(t) : 0.f;
					if (armees > 0)
						++armes;
					if (ech > 1.02f)
						++agrandis;
					if (t >= 0) {
						++textesVus;
						if (wT1 > wT0 * 1.02f)
							++textesGrandis;
					}
					pos += (size_t)snprintf(resume + pos, sizeof(resume) - pos, "%s fixe=%d arme=%d ech=%.2f texte %.0f->%.0f ; ", kLibs[l], fixe ? 1 : 0,
											armees > 0 ? 1 : 0, ech, wT0, wT1);
					if (pos >= sizeof(resume) - 1)
						break;
				}
				// un groupe fait a la main, SANS taille fixe : la poignee s'arme sur sa boite
				int32 pg = stG.doc.AddChild(0, "", NkAuthor::Humain);
				stG.doc.nodes[(uint32)pg].shape = NkString("frame");
				stG.doc.nodes[(uint32)pg].layout.kind = NkLayoutKind::Free;
				stG.doc.nodes[(uint32)pg].width.mode = NkSizeMode::Fixed;
				stG.doc.nodes[(uint32)pg].width.value = 400.f;
				stG.doc.nodes[(uint32)pg].height.mode = NkSizeMode::Fixed;
				stG.doc.nodes[(uint32)pg].height.value = 300.f;
				bool autoArme = false;
				float32 autoEch = 1.f;
				bool autoBoite = false, mainFixe = false, mainArme = false;
				float32 autoW = 0.f, autoH = 0.f, mainEch = 1.f;
				if (pg >= 0) {
					// a. un groupe ecrit SANS taille (n'existe que par le fichier) : sa boite est mesuree
					const int32 ga = stG.doc.AddChild(pg, "", NkAuthor::Humain);
					stG.doc.nodes[(uint32)ga].genre = NkString("simple");
					stG.doc.nodes[(uint32)ga].posX = 20.f;
					stG.doc.nodes[(uint32)ga].posY = 20.f;
					for (uint32 k = 0; k < 2u; ++k) {
						const int32 r = stG.doc.AddChild(ga, "", NkAuthor::Humain);
						NkUINode &n = stG.doc.nodes[(uint32)r];
						n.shape = NkString("rect");
						n.posX = 0.f;
						n.posY = 50.f * (float32)k;
						n.width.mode = NkSizeMode::Fixed;
						n.width.value = 80.f;
						n.height.mode = NkSizeMode::Fixed;
						n.height.value = 40.f;
					}
					stG.Recompute(NkPaintRect{0.f, 0.f, 600.f, 900.f});
					stG.SelectSingle(ga);
					autoBoite = stG.layout.Has(ga);
					if (autoBoite) {
						autoW = stG.layout.At(ga).w;
						autoH = stG.layout.At(ga).h;
					}
					int32 armees = 0;
					tirer(ga, 30.f, 30.f, armees);
					autoArme = armees > 0;
					autoEch = stG.doc.nodes[(uint32)ga].echelleX;
					// b. un groupe FAIT A LA MAIN (deux rectangles, Grouper) : ce que Rodolf fait
					int32 r2[2];
					for (uint32 k = 0; k < 2u; ++k) {
						r2[k] = stG.doc.AddChild(pg, "", NkAuthor::Humain);
						NkUINode &n = stG.doc.nodes[(uint32)r2[k]];
						n.shape = NkString("rect");
						n.posX = 200.f;
						n.posY = 150.f + 50.f * (float32)k;
						n.width.mode = NkSizeMode::Fixed;
						n.width.value = 80.f;
						n.height.mode = NkSizeMode::Fixed;
						n.height.value = 40.f;
					}
					stG.Recompute(NkPaintRect{0.f, 0.f, 600.f, 900.f});
					stG.SelectSingle(r2[0]);
					stG.SelectToggle(r2[1]);
					if (stG.GrouperSelection()) {
						const int32 gm = stG.selected;
						stG.Recompute(NkPaintRect{0.f, 0.f, 600.f, 900.f});
						mainFixe = stG.doc.nodes[(uint32)gm].width.mode == NkSizeMode::Fixed && stG.doc.nodes[(uint32)gm].height.mode == NkSizeMode::Fixed;
						int32 armees2 = 0;
						tirer(gm, 30.f, 30.f, armees2);
						mainArme = armees2 > 0;
						mainEch = stG.doc.nodes[(uint32)gm].echelleX;
					}
				}
				snprintf(det, sizeof(det), "document charge=%d ; %u groupes trouves, %u Fixed x Fixed, %u poignees armees, %u agrandis, textes grandis %u / %u -- %s| groupe ecrit sans taille (fichier seulement) : boite=%d %.0f x %.0f, arme=%d, echelle %.2f (mesure : pas de boite, rien a armer) | groupe fait a la main (Grouper) : Fixed x Fixed=%d, arme=%d, echelle %.2f",
						 charge ? 1 : 0, trouves, fixes, armes, agrandis, textesGrandis, textesVus, resume, autoBoite ? 1 : 0, autoW, autoH, autoArme ? 1 : 0, autoEch, mainFixe ? 1 : 0, mainArme ? 1 : 0, mainEch);
				check("79. LA POIGNEE D'UN GROUPE SUR LE DOCUMENT DE RODOLF : ses six groupes s'arment par la poignee, l'echelle "
					  "change, un enfant texte grandit a l'ecran ; un groupe fait a la main (Grouper) est Fixed x Fixed = sa boite et "
					  "s'arme de meme ; un groupe ecrit sans taille n'a pas de boite (0 x 0, mesure) -- rien a armer, dit",
					  charge && trouves == 6u && armes == 6u && agrandis == 6u && textesVus > 0u && textesGrandis == textesVus && mainFixe && mainArme && mainEch > 1.02f
						  && !autoArme && autoW == 0.f,
					  det);
			}
		}
		// ── 80. LE CROP SUR LA TOILE : le popover image ouvert sur un remplissage recadre, la poignee
		//    droite de l'IMAGE ENTIERE tiree de +40 px -> `crop=` change (L plus petit, X suit), le
		//    noeud ne bouge pas, et le peintre emet l'image avec les uv du nouveau crop.
		{
			static nkgui::NkGuiContext ctxC;
			char det[520];
			if (!ctxC.Init(600, 900)) {
				check("80. le crop sur la toile", false, "Init a refuse");
			} else {
				static DesignState stC;
				stC.doc.NewDocument("Toile", NkAuthor::Humain);
				stC.cheminActif = NkString();
				stC.images.Vider();
				renderdetail::NkPoserFournisseurImages(&NkObtenirImageDuDocument, &stC);
				const int32 pg = stC.doc.AddChild(0, "", NkAuthor::Humain);
				stC.doc.nodes[(uint32)pg].shape = NkString("frame");
				stC.doc.nodes[(uint32)pg].layout.kind = NkLayoutKind::Free;
				stC.doc.nodes[(uint32)pg].width.mode = NkSizeMode::Fixed;
				stC.doc.nodes[(uint32)pg].width.value = 400.f;
				stC.doc.nodes[(uint32)pg].height.mode = NkSizeMode::Fixed;
				stC.doc.nodes[(uint32)pg].height.value = 300.f;
				const int32 rc = stC.doc.AddChild(pg, "", NkAuthor::Humain);
				{
					NkUINode &n = stC.doc.nodes[(uint32)rc];
					n.shape = NkString("rect");
					n.posX = 100.f;
					n.posY = 100.f;
					n.width.mode = NkSizeMode::Fixed;
					n.width.value = 200.f;
					n.height.mode = NkSizeMode::Fixed;
					n.height.value = 100.f;
					NkRemplissage f;
					f.genre = NkString("image");
					f.image = NkString("sonde_image_2x2.png");
					f.cadrage = NkString("crop");
					f.cropX = 0.25f;
					f.cropY = 0.25f;
					f.cropW = 0.5f;
					f.cropH = 0.5f;
					n.fills.PushBack(f);
				}
				stC.Recompute(NkPaintRect{0.f, 0.f, 600.f, 900.f});
				stC.SelectSingle(rc);
				static PreviewPanel toileC(&stC);
				static InspectorPanel inspC(&stC);
				NkEditorFrameContext ec;
				ec.ui = &ctxC;
				ec.dt = 0.016f;
				auto image = [&](float32 mx, float32 my, bool bas) {
					ctxC.input.mousePos = {mx, my};
					ctxC.input.mouseDown[0] = bas;
					ctxC.BeginFrame(0.016f);
					ctxC.BeginLayout({0.f, 0.f, 340.f, 900.f});
					toileC.OnUI(ec);
					ctxC.BeginLayout({340.f, 0.f, 260.f, 900.f});
					inspC.OnUI(ec);
					NkDessinerPickerDemande(ctxC, stC);
					ctxC.EndFrame();
				};
				for (int32 k = 0; k < 3; ++k)
					image(-1.f, -1.f, false);
				// sans popover : la poignee n'existe pas (le clic tombe sur la toile, rien ne se recadre)
				NkLayoutResult scr;
				stC.ProjectToScreen(scr);
				const NkPaintRect rs = scr.At(rc);
				const float32 entierW = rs.w / 0.5f, entierX = rs.x - 0.25f * entierW;
				const float32 hx = entierX + entierW, hy = rs.y + rs.h * 0.5f; // la poignee droite (milieu) de l'image entiere
				image(hx, hy, false);
				image(hx, hy, true);
				image(hx + 20.f, hy, true);
				const bool sansPopover = !toileC.EnRecadrage() && stC.doc.nodes[(uint32)rc].fills[0].cropW == 0.5f;
				image(hx + 20.f, hy, false);
				for (int32 k = 0; k < 40; ++k) // le delai du double-clic expire (le second appui vient au meme endroit)
					image(-1.f, -1.f, false);
				// le popover image ouvert sur ce remplissage : la poignee existe (le clic « sans
				// popover » a deselectionne le noeud : on le re-selectionne, comme un clic dessus)
				stC.SelectSingle(rc);
				stC.picker = DesignState::DemandePicker();
				stC.picker.ouvert = true;
				stC.picker.id = ctxC.GetId("##sonde.popover.crop");
				stC.picker.genre = 1u;
				stC.picker.noeud = rc;
				stC.picker.index = 0;
				stC.picker.ancre = {580.f, 880.f, 16.f, 16.f}; // en bas a droite : le popover ne recouvre pas la toile
				image(-1.f, -1.f, false);
				image(-1.f, -1.f, false);
				bool surPopup = false;
				for (int32 i = 0; i < ctxC.popupDepth; ++i)
					if (nkgui::NkGuiRectContains(ctxC.popupRects[i], {hx, hy}))
						surPopup = true;
				const int32 profondeur = ctxC.popupDepth;
				image(hx, hy, false);
				image(hx, hy, true);
				int32 imagesEnCrop = 0;
				char etat[160];
				etat[0] = 0;
				for (int32 k = 1; k <= 4; ++k) {
					image(hx + 10.f * (float32)k, hy, true);
					if (toileC.EnRecadrage())
						++imagesEnCrop;
					if (k == 2)
						snprintf(etat, sizeof(etat), "[image 2 : selected=%d sel=%u popupDepth=%d picker.ouvert=%d modeForme=%d]", stC.selected, stC.sel.Count(),
								 ctxC.popupDepth, stC.picker.ouvert ? 1 : 0, stC.modeForme.noeud);
				}
				image(hx + 40.f, hy, false);
				image(-1.f, -1.f, false);
				const NkRemplissage &fa = stC.doc.nodes[(uint32)rc].fills[0];
				int32 diagN = 0;
				float32 diagDx = 0.f, diagW = 0.f;
				toileC.DiagCrop(diagN, diagDx, diagW);
				const float32 attW = rs.w / (entierW + 40.f), attX = (rs.x - entierX) / (entierW + 40.f);
				const bool cropChange = fa.cropW > attW - 0.003f && fa.cropW < attW + 0.003f && fa.cropX > attX - 0.003f && fa.cropX < attX + 0.003f
										&& fa.cropY == 0.25f && fa.cropH == 0.5f; // l'axe qui ne bouge pas garde ses valeurs EXACTES
				const NkUINode &na = stC.doc.nodes[(uint32)rc];
				const bool noeudIntact = na.posX == 100.f && na.posY == 100.f && na.width.value == 200.f && na.height.value == 100.f;
				// le peintre : l'image emise avec les uv du nouveau crop
				float32 u0 = 9.f, u1 = -9.f;
				{
					NkPaintRect sfc{0.f, 0.f, 600.f, 900.f};
					NkRecordingPaint rec;
					RenderDocument(rec, stC.doc, sfc);
					for (uint32 i = 0; i < (uint32)rec.cmds.Size(); ++i) {
						const NkPaintCmd &c = rec.cmds[i];
						if (c.op != NkPaintOp::Image)
							continue;
						for (uint32 k = 0; k + 1 < (uint32)c.uv.Size(); k += 2u) {
							if (c.uv[k] < u0) u0 = c.uv[k];
							if (c.uv[k] > u1) u1 = c.uv[k];
						}
					}
				}
				const bool peintreSuit = u0 > fa.cropX - 0.003f && u0 < fa.cropX + 0.003f && (u1 - u0) > fa.cropW - 0.003f && (u1 - u0) < fa.cropW + 0.003f;
				if (ctxC.popupDepth > 0)
					ctxC.ClosePopup();
				stC.picker = DesignState::DemandePicker();
				image(-1.f, -1.f, false);
				snprintf(det, sizeof(det), "sans popover : rien=%d ; popover ouvert (profondeur %d, la poignee sous le popup=%d), poignee droite de l'image entiere (%.0f,%.0f) tiree de +40 : %d images en recadrage %s (geste calcule %d fois, dernier dx %.1f, L calcule %.3f), crop X %.3f (attendu %.3f) L %.3f (attendu %.3f), Y %.2f H %.2f ; noeud intact=%d ; peintre u0 %.3f u1-u0 %.3f",
						 sansPopover ? 1 : 0, profondeur, surPopup ? 1 : 0, hx, hy, imagesEnCrop, etat, diagN, diagDx, diagW, fa.cropX, attX, fa.cropW, attW, fa.cropY, fa.cropH, noeudIntact ? 1 : 0, u0, u1 - u0);
				check("80. LE CROP SUR LA TOILE : sans popover la poignee n'existe pas ; le popover image ouvert, la poignee droite de "
					  "l'image entiere tiree de +40 px change `crop=` (L plus petit, X suit), le noeud ne bouge pas, et le peintre emet "
					  "l'image avec les uv du nouveau crop",
					  sansPopover && imagesEnCrop >= 3 && cropChange && noeudIntact && peintreSuit, det);
			}
		}
		// ── 81-82. L'EXPORT (05/09) : PNG en PIXELS (rect uni, degrade, texte, image 2x2, rect
		//    tourne ; 1x / 2x / 3x ; la selection ; l'echec dit), puis SVG (un lecteur du document)
		//    RE-RASTERISE par le parseur SVG maison et compare aux memes points ; texte et image
		//    en STRUCTURE (le parseur maison ne les sait pas : mesure, dit).
		{
			char det[1200];
			static const uint8 kM[4] = {255, 0, 255, 255}, kG[4] = {0, 255, 0, 255};
			bool pngSource = false;
			{
				NkImage img;
				if (img.Create(2u, 2u, math::NkColor(), 4) && img.Pixels()) {
					uint8 *px = img.Pixels();
					for (int32 y = 0; y < 2; ++y)
						for (int32 x = 0; x < 2; ++x) {
							const uint8 *c = ((x + y) & 1) ? kG : kM;
							for (int32 k = 0; k < 4; ++k)
								px[(y * 2 + x) * 4 + k] = c[k];
						}
					pngSource = img.SavePNG("sonde_export_2x2.png");
				}
			}
			static DesignState stEx;
			stEx.doc.NewDocument("Export", NkAuthor::Humain);
			stEx.cheminActif = NkString();
			stEx.images.Vider();
			stEx.sel.Clear();
			stEx.selected = -1;
			// le theme de la sonde : une page BLANCHE opaque et un cadre gris, comme un theme reel
			// (le theme par defaut d'une sonde laisse ces deux roles a une valeur sentinelle)
			stEx.theme.Set(NkDesignResolveRole("artboard_bg"), 0xFFFFFFFFu);
			stEx.theme.Set(NkDesignResolveRole("border"), 0x30363DFFu);
			renderdetail::NkPoserFournisseurImages(&NkObtenirImageDuDocument, &stEx);
			const int32 pg = stEx.doc.AddChild(0, "", NkAuthor::Humain);
			{
				NkUINode &p = stEx.doc.nodes[(uint32)pg];
				p.shape = NkString("frame");
				p.label = NkString("Page");
				p.layout.kind = NkLayoutKind::Free;
				p.width.mode = NkSizeMode::Fixed;
				p.width.value = 200.f;
				p.height.mode = NkSizeMode::Fixed;
				p.height.value = 120.f;
			}
			auto rect = [&](float32 x, float32 y, float32 w, float32 h, const char *label, const char *couleur) -> int32 {
				const int32 i = stEx.doc.AddChild(pg, "", NkAuthor::Humain);
				NkUINode &n = stEx.doc.nodes[(uint32)i];
				n.shape = NkString("rect");
				n.label = NkString(label);
				n.posX = x;
				n.posY = y;
				n.width.mode = NkSizeMode::Fixed;
				n.width.value = w;
				n.height.mode = NkSizeMode::Fixed;
				n.height.value = h;
				n.layout.kind = NkLayoutKind::Free;
				if (couleur) {
					NkRemplissage f;
					f.couleur = NkString(couleur);
					n.fills.PushBack(f);
				}
				return i;
			};
			// A. un rect uni rouge, avec une ombre portee (le filtre du SVG)
			const int32 rA = rect(10.f, 10.f, 60.f, 40.f, "Uni", "#ff0000");
			{
				NkEffet e;
				e.couleur = NkString("#000000");
				e.opacite = 25.f;
				e.y = 4.f;
				e.flou = 4.f;
				stEx.doc.nodes[(uint32)rA].effets.PushBack(e);
			}
			// B. un degrade lineaire noir -> blanc, angle 0 : l'axe (0, 1), de haut en bas
			const int32 rB = rect(80.f, 10.f, 60.f, 40.f, "Degrade", nullptr);
			{
				NkRemplissage f;
				f.degrade.type = NkString("lineaire");
				f.degrade.angle = 0.f;
				NkArretDegrade a0, a1;
				a0.position = 0.f;
				a0.couleur = NkString("#000000");
				a1.position = 1.f;
				a1.couleur = NkString("#ffffff");
				f.degrade.arrets.PushBack(a0);
				f.degrade.arrets.PushBack(a1);
				stEx.doc.nodes[(uint32)rB].fills.PushBack(f);
			}
			// C. un fond blanc puis un texte noir « Ab », corps 14
			(void)rect(10.f, 60.f, 60.f, 30.f, "FondTexte", "#ffffff");
			const int32 tC = stEx.doc.AddChild(pg, "", NkAuthor::Humain);
			{
				NkUINode &t = stEx.doc.nodes[(uint32)tC];
				t.shape = NkString("text");
				t.label = NkString("Texte");
				t.text = NkString("Ab");
				t.posX = 10.f;
				t.posY = 60.f;
				t.width.mode = NkSizeMode::Fixed;
				t.width.value = 60.f;
				t.height.mode = NkSizeMode::Fixed;
				t.height.value = 30.f;
				t.fontPx = 14.f;
				t.textColor = NkString("#000000");
			}
			// D. l'image 2x2 (magenta / vert) etiree sur 40x40
			const int32 rD = rect(80.f, 60.f, 40.f, 40.f, "Image", nullptr);
			{
				NkRemplissage f;
				f.genre = NkString("image");
				f.image = NkString("sonde_export_2x2.png");
				f.cadrage = NkString("stretch");
				stEx.doc.nodes[(uint32)rD].fills.PushBack(f);
			}
			// E. un carre bleu tourne de 45 degres : la matrice dans les pixels
			const int32 rE = rect(140.f, 65.f, 30.f, 30.f, "Tourne", "#0000ff");
			stEx.doc.nodes[(uint32)rE].rotation = 45.f;

			auto pixel = [](const NkImage &im, int32 x, int32 y, uint8 out[4]) {
				out[0] = out[1] = out[2] = out[3] = 0;
				if (!im.Pixels() || x < 0 || y < 0 || x >= im.Width() || y >= im.Height())
					return;
				const uint8 *p = im.Pixels() + ((usize)y * (usize)im.Width() + (usize)x) * 4u;
				for (int32 k = 0; k < 4; ++k)
					out[k] = p[k];
			};
			auto proche = [&](const NkImage &im, int32 x, int32 y, int32 r, int32 g, int32 b, int32 tol) {
				uint8 c[4];
				pixel(im, x, y, c);
				auto d = [](int32 a, int32 b2) { return a > b2 ? a - b2 : b2 - a; };
				return d(c[0], r) <= tol && d(c[1], g) <= tol && d(c[2], b) <= tol && c[3] >= 250;
			};
			auto gris = [&](const NkImage &im, int32 x, int32 y) -> int32 {
				uint8 c[4];
				pixel(im, x, y, c);
				return ((int32)c[0] + (int32)c[1] + (int32)c[2]) / 3;
			};
			// l'encre d'un texte noir sur fond blanc : combien de pixels sombres, et combien de
			// pixels INTERMEDIAIRES (le bord anticrenele) parmi ceux qui ne sont pas blancs
			auto encre = [&](const NkImage &im, int32 x0, int32 y0, int32 x1, int32 y1, int32 &sombres, int32 &bords,
							 int32 &ymin, int32 &ymax) {
				sombres = bords = 0;
				ymin = 1 << 20;
				ymax = -1;
				for (int32 y = y0; y < y1; ++y)
					for (int32 x = x0; x < x1; ++x) {
						const int32 g = gris(im, x, y);
						if (g < 96) {
							++sombres;
							if (y < ymin) ymin = y;
							if (y > ymax) ymax = y;
						} else if (g < 224)
							++bords;
					}
			};

			// 81a. 1x : le fichier, ses dimensions, aucune texture inconnue
			NkExportOptions o1;
			o1.echelle = 1.f;
			NkExportResultat r1;
			const bool ok1 = NkExporterPNG(stEx, o1, "sonde_export_page.png", r1);
			NkImage im1;
			const bool lu1 = ok1 && im1.Load("sonde_export_page.png", 4) && im1.Width() == 200 && im1.Height() == 120 && im1.Pixels();
			snprintf(det, sizeof(det), "source 2x2 ecrite=%d ; export=%d (%s) ; relu 200x120=%d (%d x %d) ; textures inconnues=%u ; images=%u ; polices=%u (max %.1f px)",
					 pngSource ? 1 : 0, ok1 ? 1 : 0, r1.message, lu1 ? 1 : 0, im1.Width(), im1.Height(), r1.texturesInconnues, r1.images,
					 r1.polices, (double)r1.policeMax);
			snprintf(det + (det[0] ? (int32)NkString(det).Length() : 0), sizeof(det) - NkString(det).Length(), " ; police a 14 px chargee=%d", r1.PoliceChargee(14.f) ? 1 : 0);
			check("81a. EXPORT PNG D'UNE PAGE A 1x : le fichier est ecrit par le codec PNG maison et relu a 200 x 120, sans texture inconnue, "
				  "l'image du document declaree, la police chargee a 14 px",
				  pngSource && ok1 && lu1 && r1.texturesInconnues == 0u && r1.images == 1u && r1.PoliceChargee(14.f), det);
			// 81b. le rect uni : quatre points rouges
			const bool rouge1 = lu1 && proche(im1, 15, 15, 255, 0, 0, 2) && proche(im1, 64, 15, 255, 0, 0, 2) && proche(im1, 15, 44, 255, 0, 0, 2)
								&& proche(im1, 64, 44, 255, 0, 0, 2);
			// 81c. le degrade : quatre points sur une colonne, du sombre au clair, monotone
			const int32 g15 = gris(im1, 110, 15), g25 = gris(im1, 110, 25), g35 = gris(im1, 110, 35), g45 = gris(im1, 110, 45);
			const bool degrade1 = lu1 && g15 < g25 && g25 < g35 && g35 < g45 && g15 < 96 && g45 > 160;
			// 81d. l'image : les quatre texels aux quatre coins (bilineaire, bord repete : purs)
			const bool image1 = lu1 && proche(im1, 82, 62, 255, 0, 255, 4) && proche(im1, 117, 62, 0, 255, 0, 4) && proche(im1, 82, 97, 0, 255, 0, 4)
								&& proche(im1, 117, 97, 255, 0, 255, 4);
			// 81e. le carre tourne : bleu au centre et a (155, 63) -- hors de sa boite droite, dans le losange -- et pas au coin de la boite
			const bool tourne1 = lu1 && proche(im1, 155, 80, 0, 0, 255, 2) && proche(im1, 155, 63, 0, 0, 255, 2) && !proche(im1, 141, 66, 0, 0, 255, 40);
			uint8 coin[4];
			pixel(im1, 141, 66, coin);
			snprintf(det, sizeof(det), "rouge aux quatre points=%d ; degrade x=110 : y15=%d y25=%d y35=%d y45=%d ; image M/V/V/M=%d ; tourne : centre et (155,63) bleus, coin (141,66)=(%u,%u,%u) non bleu -> %d",
					 rouge1 ? 1 : 0, g15, g25, g35, g45, image1 ? 1 : 0, coin[0], coin[1], coin[2], tourne1 ? 1 : 0);
			check("81b. LES PIXELS A 1x : le rect uni rouge a quatre points, le degrade noir -> blanc monotone sur sa colonne, les quatre texels "
				  "de l'image 2x2 la ou le cadrage les met, le carre tourne de 45 degres bleu dans son losange et pas au coin de sa boite",
				  rouge1 && degrade1 && image1 && tourne1, det);
			// 81f. le texte a 1x : de l'encre dans sa boite
			int32 s1 = 0, b1 = 0, y1min = 0, y1max = 0;
			if (lu1)
				encre(im1, 10, 60, 70, 90, s1, b1, y1min, y1max);
			// 81g. 2x : memes couleurs, dimensions doublees, texte net
			NkExportOptions o2;
			o2.echelle = 2.f;
			NkExportResultat r2;
			const bool ok2 = NkExporterPNG(stEx, o2, "sonde_export_page@2x.png", r2);
			NkImage im2;
			const bool lu2 = ok2 && im2.Load("sonde_export_page@2x.png", 4) && im2.Width() == 400 && im2.Height() == 240 && im2.Pixels();
			const bool rouge2 = lu2 && proche(im2, 31, 31, 255, 0, 0, 2) && proche(im2, 128, 31, 255, 0, 0, 2) && proche(im2, 31, 88, 255, 0, 0, 2)
								&& proche(im2, 128, 88, 255, 0, 0, 2);
			const int32 h15 = gris(im2, 221, 31), h25 = gris(im2, 221, 51), h35 = gris(im2, 221, 71), h45 = gris(im2, 221, 91);
			const bool degrade2 = lu2 && h15 < h25 && h25 < h35 && h35 < h45 && h15 < 96 && h45 > 160;
			const bool image2 = lu2 && proche(im2, 164, 124, 255, 0, 255, 4) && proche(im2, 235, 124, 0, 255, 0, 4) && proche(im2, 164, 195, 0, 255, 0, 4)
								&& proche(im2, 235, 195, 255, 0, 255, 4);
			int32 s2 = 0, b2 = 0, y2min = 0, y2max = 0;
			if (lu2)
				encre(im2, 20, 120, 140, 180, s2, b2, y2min, y2max);
			const int32 hauteur1 = y1max - y1min + 1, hauteur2 = y2max - y2min + 1;
			const bool texteDouble = s1 > 0 && s2 > (s1 * 5) / 2 && s2 < s1 * 6 && hauteur2 >= 2 * hauteur1 - 3 && hauteur2 <= 2 * hauteur1 + 3;
			// la MUTATION du texte : l'atlas de 1x etire par la matrice (le chemin de la toile) -> plus de bords flous
			NkExportOptions o2b = o2;
			o2b.policeExacte = false;
			NkExportResultat r2b;
			NkImage im2b;
			const bool ok2b = NkExporterImage(stEx, o2b, im2b, r2b) && im2b.Pixels();
			int32 s2b = 0, b2b = 0, ymb0 = 0, ymb1 = 0;
			if (ok2b)
				encre(im2b, 20, 120, 140, 180, s2b, b2b, ymb0, ymb1);
			const float32 flouExact = s2 + b2 > 0 ? (float32)b2 / (float32)(s2 + b2) : 1.f;
			const float32 flouEtire = s2b + b2b > 0 ? (float32)b2b / (float32)(s2b + b2b) : 0.f;
			const bool net = ok2b && r2.PoliceChargee(28.f) && flouExact < flouEtire;
			snprintf(det, sizeof(det), "2x : export=%d relu 400x240=%d ; rouge=%d ; degrade y=%d/%d/%d/%d ; image=%d ; texte 1x : %d sombres (hauteur %d), 2x : %d sombres (hauteur %d) ; police a 28 px chargee=%d (la plus grande %.1f) ; bords flous exact %.3f contre atlas etire %.3f (%s)",
					 ok2 ? 1 : 0, lu2 ? 1 : 0, rouge2 ? 1 : 0, h15, h25, h35, h45, image2 ? 1 : 0, s1, hauteur1, s2, hauteur2, r2.PoliceChargee(28.f) ? 1 : 0, (double)r2.policeMax,
					 (double)flouExact, (double)flouEtire, r2b.message);
			check("81c. EXPORT A 2x : memes couleurs aux memes points (doubles), 400 x 240, le texte a 4 fois l'encre et 2 fois la hauteur, "
				  "rasterise par une police chargee a 28 px -- moins de bords flous que l'atlas de 1x etire par la matrice (la mutation)",
				  lu2 && rouge2 && degrade2 && image2 && texteDouble && net, det);
			// 81h. 3x : les dimensions ; la selection : sa boite seule ; l'echec : dit, aucun fichier
			NkExportOptions o3;
			o3.echelle = 3.f;
			NkExportResultat r3;
			NkImage im3;
			const bool ok3 = NkExporterImage(stEx, o3, im3, r3) && im3.Width() == 600 && im3.Height() == 360 && proche(im3, 465, 240, 0, 0, 255, 2);
			stEx.sel.Set(rA);
			stEx.selected = rA;
			NkExportOptions oS;
			oS.selection = true;
			NkExportResultat rS;
			const bool okS = NkExporterPNG(stEx, oS, "sonde_export_selection.png", rS);
			NkImage imS;
			// la marge de l'ombre (y 4 + flou 4 = 8 px) entoure la boite 60 x 40 : 76 x 56, le rect rouge de (8, 8) a (68, 48)
			const bool luS = okS && imS.Load("sonde_export_selection.png", 4) && imS.Width() == 76 && imS.Height() == 56 && proche(imS, 38, 28, 255, 0, 0, 2)
							 && proche(imS, 9, 9, 255, 0, 0, 2) && proche(imS, 66, 46, 255, 0, 0, 2);
			uint8 hors[4];
			pixel(imS, 2, 2, hors);
			const bool transparentHors = luS && hors[3] < 40; // hors du rect et de son ombre : transparent, pas un fond invente
			stEx.sel.Clear();
			stEx.selected = -1;
			NkExportResultat rR;
			NkFile::Delete("sonde_export_rien.png");
			const bool refus = !NkExporterPNG(stEx, oS, "sonde_export_rien.png", rR) && !NkFile::Exists("sonde_export_rien.png")
							   && rR.message[0] == '\xC3'; // « É » de « ÉCHEC »
			snprintf(det, sizeof(det), "3x : 600 x 360 et le bleu tourne a (465,240) -> %d ; selection : %s ; relu 76x56 rouge en (38,28) (9,9) (66,46) -> %d ; hors du rect alpha=%u -> transparent=%d ; sans selection : refuse et dit -> %d (%s)",
					 ok3 ? 1 : 0, rS.message, luS ? 1 : 0, hors[3], transparentHors ? 1 : 0, refus ? 1 : 0, rR.message);
			check("81d. 3x fait 600 x 360 ; LA SELECTION s'exporte dans sa boite (plus la marge de son ombre), le reste transparent ; "
				  "un export sans rien de selectionne est REFUSE et DIT, aucun fichier n'est ecrit",
				  ok3 && luS && transparentHors && refus, det);

			// ── 82. LE SVG ──────────────────────────────────────────────────────
			auto contient = [](const char *h, const char *n) -> bool {
				if (!h || !n)
					return false;
				for (const char *p = h; *p; ++p) {
					const char *a = p, *b = n;
					while (*a && *b && *a == *b)
						++a, ++b;
					if (!*b)
						return true;
				}
				return false;
			};
			NkExportOptions oV;
			oV.format = NkExportFormat::SVG;
			NkString svg;
			NkExportResultat rV;
			const bool okV = NkExporterSVG(stEx, oV, "", svg, rV);
			const char *s = svg.Data();
			const bool structure = okV && contient(s, "<svg ") && contient(s, "width=\"200\"") && contient(s, "height=\"120\"") && contient(s, "<rect")
								   && contient(s, "fill=\"#ff0000\"") && contient(s, "<linearGradient") && contient(s, "stop-color=\"#000000\"")
								   && contient(s, "stop-color=\"#ffffff\"") && contient(s, "fill=\"url(#deg") && contient(s, "<text") && contient(s, "font-family=\"Inter")
								   && contient(s, ">Ab</text>") && contient(s, "<image") && contient(s, "href=\"sonde_export_2x2.png\"")
								   && contient(s, "preserveAspectRatio=\"none\"") && contient(s, "transform=\"matrix(") && contient(s, "<feDropShadow")
								   && contient(s, "filter=\"url(#ombre") && contient(s, "data-nom=\"Tourne\"") && contient(s, "</g>");
			NkExportOptions oE = oV;
			oE.embarquer = true;
			NkString svgE;
			NkExportResultat rE2;
			const bool okE = NkExporterSVG(stEx, oE, "", svgE, rE2) && contient(svgE.Data(), "href=\"data:image/png;base64,iVBORw0KGgo");
			// Ⓛ LE CORPS SVG EST LE CADRATIN, PAS NOTRE CORPS EN PIXELS (temoin croise de
			//    l'agent codec SVG, 05/09) : un lecteur echelonne par `font-size / unitsPerEm`,
			//    `NkFontAtlas` par `px / (ascender - descender)`. Pour Inter (upm 2816,
			//    asc - desc 3408), un texte de 14 px s'ecrit donc « font-size 11,57 ».
			float32 ascT = 0.f, ligT = 0.f, corpsT = 0.f;
			const bool metT = svgdetail::MetriquesEx(14.f, ascT, ligT, corpsT);
			const float32 attenduT = 14.f * 2816.f / 3408.f;
			const bool corpsJuste = metT && corpsT > attenduT - 0.05f && corpsT < attenduT + 0.05f
									&& !contient(s, "font-size=\"14\"");
			snprintf(det, sizeof(det),
					 "export=%d (%s) ; %u octets ; structure attendue=%d ; images embarquees=%d ; corps SVG d'un texte de 14 px : %.3f "
					 "(attendu %.3f = 14 x 2816 / 3408, lu dans la face) -> %d",
					 okV ? 1 : 0, rV.message, (uint32)svg.Length(), structure ? 1 : 0, okE ? 1 : 0, (double)corpsT, (double)attenduT,
					 corpsJuste ? 1 : 0);
			check("82a. EXPORT SVG D'UNE PAGE : un lecteur du document -- <rect> et son fill, <linearGradient> et ses <stop>, <text> en Inter 14 "
				  "« Ab », <image href> relatif en preserveAspectRatio=none (stretch), <g transform=matrix> pour le carre tourne, <feDropShadow> "
				  "pour l'ombre ; l'option « embarquer » ecrit le PNG en base64 ; le CORPS est le cadratin (px x upm / (asc - desc)), "
				  "pas notre corps en pixels",
				  structure && okE && corpsJuste, det);
			// 82b. le SVG RE-RASTERISE par le parseur maison, compare aux memes points (formes, degrade)
			NkImage ras = NkSVGCodec::Decode((const uint8 *)svg.Data(), (usize)svg.Length(), 0, 0);
			const bool rasOk = ras.IsValid() && ras.Width() == 200 && ras.Height() == 120 && ras.Pixels();
			const bool rougeR = rasOk && proche(ras, 15, 15, 255, 0, 0, 3) && proche(ras, 64, 15, 255, 0, 0, 3) && proche(ras, 15, 44, 255, 0, 0, 3)
								&& proche(ras, 64, 44, 255, 0, 0, 3);
			const int32 q15 = gris(ras, 110, 15), q25 = gris(ras, 110, 25), q35 = gris(ras, 110, 35), q45 = gris(ras, 110, 45);
			const bool degradeR = rasOk && q15 < q25 && q25 < q35 && q35 < q45 && q15 < 96 && q45 > 160;
			// le parseur maison rend un degrade CONTINU la ou le peintre peint 24 bandes : au plus 24 de gris d'ecart par point
			auto d24 = [](int32 a, int32 b) { return (a > b ? a - b : b - a) <= 24; };
			const bool memeDegrade = degradeR && d24(q15, g15) && d24(q25, g25) && d24(q35, g35) && d24(q45, g45);
			const bool tourneR = rasOk && proche(ras, 155, 80, 0, 0, 255, 3) && proche(ras, 155, 63, 0, 0, 255, 3) && !proche(ras, 141, 66, 0, 0, 255, 40);
			uint8 fondPng[4], fondSvg[4];
			pixel(im1, 5, 115, fondPng);
			pixel(ras, 5, 115, fondSvg);
			bool memeFond = rasOk && lu1;
			for (int32 k = 0; k < 4; ++k)
				if ((fondPng[k] > fondSvg[k] ? fondPng[k] - fondSvg[k] : fondSvg[k] - fondPng[k]) > 3)
					memeFond = false;
			NkExportResultat rF;
			const bool fichierV = NkExporterSVGFichier(stEx, oV, "sonde_export_page.svg", rF) && NkFile::Exists("sonde_export_page.svg");
			snprintf(det, sizeof(det), "NkSVGCodec::Decode -> valide=%d (%d x %d) ; rouge aux quatre points=%d ; degrade y=%d/%d/%d/%d contre le PNG %d/%d/%d/%d (ecart <= 24)=%d ; tourne=%d ; fond de page PNG (%u,%u,%u,%u) = SVG (%u,%u,%u,%u), alpha compris -> %d ; fichier=%d (%s) | texte et image : STRUCTURE seulement (le parseur maison ne sait ni <text> ni <image>, son en-tete le dit)",
					 rasOk ? 1 : 0, ras.Width(), ras.Height(), rougeR ? 1 : 0, q15, q25, q35, q45, g15, g25, g35, g45, memeDegrade ? 1 : 0, tourneR ? 1 : 0,
					 fondPng[0], fondPng[1], fondPng[2], fondPng[3], fondSvg[0], fondSvg[1], fondSvg[2], fondSvg[3], memeFond ? 1 : 0,
					 fichierV ? 1 : 0, rF.message);
			check("82b. LE SVG EXPORTE, RE-RASTERISE PAR LE PARSEUR SVG MAISON (NkSVGCodec, NKImage) : 200 x 120, le rect rouge aux quatre "
				  "points, le degrade monotone et a 24 de gris du PNG, le carre tourne dans son losange, le fond de page a la meme valeur (alpha compris) ; le fichier .svg ecrit et dit",
				  rasOk && rougeR && memeDegrade && tourneR && memeFond && fichierV, det);
		}
		// ── 83-85. LES RETOURS DE RODOLF DU 05/09 (matin), MESURES AVANT D'ETRE CORRIGES :
		//    ① Ctrl+D (une pression = une copie, sur la toile), ② une modale (le selecteur de
		//    fichier) laisse-t-elle traverser clic / molette / clavier ?, ③ renommer une variable
		//    dans le rail QUAND UN OBJET EST SELECTIONNE.
		{
			static nkgui::NkGuiContext ctxRet;
			char det[900];
			static nkgui::NkGuiFont policeRet;
			const bool policeOk = policeRet.LoadEmbedded(nkentseu::NkEmbeddedFontId::Inter, 14.f, false);
			if (!ctxRet.Init(860, 900) || !policeOk) {
				check("83. les retours du 05/09 (matin)", false, "Init ou police a refuse");
			} else {
				ctxRet.font = &policeRet; // le selecteur de fichier du kit exige une police pour se dessiner
				static DesignState stRet;
				int32 pg = -1, rc = -1;
				// le document, RECONSTRUIT avant chaque sonde : une fuite (un noeud supprime a travers la
				// modale) ne doit pas faire tomber la sonde suivante sur un indice invalide
				auto construire = [&]() {
					stRet.doc.NewDocument("Toile", NkAuthor::Humain);
					stRet.cheminActif = NkString();
					stRet.images.Vider();
					stRet.sel.Clear();
					stRet.selected = -1;
					renderdetail::NkPoserFournisseurImages(&NkObtenirImageDuDocument, &stRet);
					pg = stRet.doc.AddChild(0, "", NkAuthor::Humain);
					stRet.doc.nodes[(uint32)pg].shape = NkString("frame");
					stRet.doc.nodes[(uint32)pg].layout.kind = NkLayoutKind::Free;
					stRet.doc.nodes[(uint32)pg].width.mode = NkSizeMode::Fixed;
					stRet.doc.nodes[(uint32)pg].width.value = 300.f;
					stRet.doc.nodes[(uint32)pg].height.mode = NkSizeMode::Fixed;
					stRet.doc.nodes[(uint32)pg].height.value = 300.f;
					rc = stRet.doc.AddChild(pg, "", NkAuthor::Humain);
					NkUINode &n = stRet.doc.nodes[(uint32)rc];
					n.shape = NkString("rect");
					n.label = NkString("Carre");
					n.posX = 100.f;
					n.posY = 100.f;
					n.width.mode = NkSizeMode::Fixed;
					n.width.value = 80.f;
					n.height.mode = NkSizeMode::Fixed;
					n.height.value = 80.f;
					NkRemplissage f;
					f.couleur = NkString("#1976d2");
					n.fills.PushBack(f);
					stRet.Recompute(NkPaintRect{0.f, 0.f, 340.f, 900.f});
					stRet.SelectSingle(rc);
				};
				construire();
				static PreviewPanel toileRet(&stRet);
				static InspectorPanel inspRet(&stRet);
				static VariablesPanel railRet(&stRet);
				NkEditorFrameContext ec;
				ec.ui = &ctxRet;
				ec.dt = 0.016f;
				// ⚠️ CETTE LAMBDA JOUE LE ROLE DE L'HOTE, et c'est dit : `NkEditorShell` exige une
				//    fenetre, la sonde n'en a pas. Elle applique donc le MEME geste que lui
				//    (`NkEditorShell.cpp`, bloc `modal`) : quand une modale a reserve la saisie a
				//    l'image precedente, souris, molette, caracteres et touches sont neutralises
				//    pour les PANNEAUX, puis rendus tels quels a l'overlay ou vit la modale.
				//    Ce que la sonde mesure : la RESERVE (posee par le selecteur du kit) et son
				//    EFFET. Que la coquille l'honore se lit dans son code, une ligne.
				auto image = [&](float32 mx, float32 my, bool bas, uint32 cp) {
					ctxRet.input.mousePos = {mx, my};
					ctxRet.input.mouseDown[0] = bas;
					ctxRet.BeginFrame(0.016f);
					if (cp)
						ctxRet.input.PushChar(cp);
					const bool modaleOuverte = ctxRet.input.saisieReserveePrec;
					nkgui::NkGuiInput garde;
					if (modaleOuverte) {
						garde = ctxRet.input;
						ctxRet.input.mousePos = {-100000.f, -100000.f};
						for (int32 b = 0; b < 3; ++b) {
							ctxRet.input.mouseClicked[b] = false;
							ctxRet.input.mouseDown[b] = false;
							ctxRet.input.mouseDoubleClicked[b] = false;
						}
						ctxRet.input.wheel = ctxRet.input.wheelH = 0.f;
						ctxRet.input.charCount = 0;
						for (int32 ki = 0; ki < nkgui::NkGuiInput::KeyCount; ++ki) {
							ctxRet.input.keyDown[ki] = false;
							ctxRet.input.keyInit[ki] = false;
						}
					}
					ctxRet.BeginLayout({0.f, 0.f, 340.f, 900.f});
					toileRet.OnUI(ec);
					ctxRet.BeginLayout({340.f, 0.f, 260.f, 900.f});
					inspRet.OnUI(ec);
					ctxRet.BeginLayout({600.f, 0.f, 260.f, 900.f});
					railRet.OnUI(ec);
					if (modaleOuverte)
						ctxRet.input = garde; // l'entree REELLE pour l'overlay, comme la coquille
					NkDessinerPickerDemande(ctxRet, stRet);
					ctxRet.EndFrame();
				};
				for (int32 k = 0; k < 3; ++k)
					image(-1.f, -1.f, false, 0u);
				// ── 83. Ctrl+D : la touche TENUE trois images = UNE copie (le front, pas l'etat) ──
				const uint32 avantD = (uint32)stRet.doc.nodes.Size();
				ctxRet.input.ctrlDown = true;
				ctxRet.input.SetKey(nkgui::NkGuiKey::D, true);
				for (int32 k = 0; k < 3; ++k)
					image(-1.f, -1.f, false, 0u);
				ctxRet.input.SetKey(nkgui::NkGuiKey::D, false);
				ctxRet.input.ctrlDown = false;
				image(-1.f, -1.f, false, 0u);
				const uint32 apresD = (uint32)stRet.doc.nodes.Size();
				// une seconde pression : une seconde copie
				ctxRet.input.ctrlDown = true;
				ctxRet.input.SetKey(nkgui::NkGuiKey::D, true);
				image(-1.f, -1.f, false, 0u);
				ctxRet.input.SetKey(nkgui::NkGuiKey::D, false);
				ctxRet.input.ctrlDown = false;
				image(-1.f, -1.f, false, 0u);
				const uint32 apresD2 = (uint32)stRet.doc.nodes.Size();
				snprintf(det, sizeof(det), "noeuds %u -> %u (Ctrl+D tenu 3 images) -> %u (seconde pression) ; la coquille ne porte plus le raccourci « Ctrl+D » (« Édition: Dupliquer (Ctrl+D sur la toile) », sans raccourci : la toile est la seule porte) ; la repetition de l'OS n'y est pour rien (le dorsal Win32 l'envoie en NkKeyRepeatEvent, que la coquille n'ecoute pas)",
						 avantD, apresD, apresD2);
				check("83. CTRL+D, UNE PRESSION = UNE COPIE : la touche tenue trois images ne duplique qu'une fois (le front `KeyPressed`), "
					  "une seconde pression duplique une seconde fois ; le SECOND CHEMIN (le meme raccourci declare dans la table de commandes "
					  "de la coquille) est retire -- c'est lui qui doublait chaque pression",
					  apresD == avantD + 1u && apresD2 == avantD + 2u, det);
				construire();
				image(-1.f, -1.f, false, 0u);
				// ── 84. ② UNE MODALE RESERVE LA SAISIE : le selecteur de fichier du kit ouvert, ni le clic
				//    dans le vide de la toile, ni la molette, ni `Suppr` n'atteignent le document ;
				//    a la fermeture, la reserve tombe et la toile repond de nouveau.
				const bool reserveAvant = ctxRet.input.saisieReserveePrec;
				stRet.OuvrirChoixImage(rc, 0);
				image(-1.f, -1.f, false, 0u); // l'image ou elle s'ouvre : elle se declare
				// L'IMAGE DE RETARD, MESUREE : la modale s'est declaree PENDANT cette image, la
				// reserve ne se lit qu'a la suivante (`NewFrame` la reporte). C'est le contrat
				// ecrit dans NkGuiInput, et c'est ce que la sonde attend -- pas l'inverse.
				const bool retardTenu = !ctxRet.input.saisieReserveePrec;
				image(-1.f, -1.f, false, 0u);
				const bool reserveTenue = ctxRet.input.saisieReserveePrec; // re-armee a chaque image
				const bool ouverte = stRet.choixImage.pickerOpen;
				const float32 zoomAvant = stRet.view.zoom;
				const int32 selAvant = stRet.selected;
				const uint32 nAvant = (uint32)stRet.doc.nodes.Size();
				// le clic dans le vide de la toile (10, 10) : hors du carre, hors de la modale
				image(10.f, 10.f, false, 0u);
				image(10.f, 10.f, true, 0u);
				image(10.f, 10.f, false, 0u);
				const int32 selApresClic = stRet.selected;
				// la molette sur la toile
				ctxRet.input.wheel = 3.f;
				image(170.f, 450.f, false, 0u);
				ctxRet.input.wheel = 3.f;
				image(170.f, 450.f, false, 0u);
				const float32 zoomApres = stRet.view.zoom;
				// la touche Suppr (le carre est toujours selectionne : le clic n'a pas traverse)
				ctxRet.input.SetKey(nkgui::NkGuiKey::Delete, true);
				image(-1.f, -1.f, false, 0u);
				ctxRet.input.SetKey(nkgui::NkGuiKey::Delete, false);
				image(-1.f, -1.f, false, 0u);
				const uint32 nApres = (uint32)stRet.doc.nodes.Size();
				const bool rienNeTraverse = selApresClic == selAvant && zoomApres == zoomAvant && nApres == nAvant;
				// « Annuler » DANS la modale. ⚠️ GEOMETRIE MISE A JOUR LE 05/09 AU SOIR : le site
				// « Choisir une image... » est passe au SELECTEUR PAR DEFAUT du kit
				// (`NkDrawSelecteur`, deux volets), qui mesure 900 x 620 et non 580 x 500.
				// La sonde a rougi a la bascule, et c'est exactement ce qu'on lui demande :
				// un temoin qui aurait continue de passer n'aurait rien mesure du tout.
				// « Annuler » : {px + pw - 20 - 120*2 - 10, py + ph - 52, 120, 34}.
				const float32 pw = 900.f, ph = 620.f, pxm = (860.f - pw) * 0.5f, pym = (900.f - ph) * 0.5f;
				const float32 ax = pxm + pw - 20.f - 240.f - 10.f + 60.f, ay = pym + ph - 52.f + 17.f;
				image(ax, ay, false, 0u);
				image(ax, ay, true, 0u);
				image(ax, ay, false, 0u);
				const bool fermee = !stRet.choixImage.pickerOpen;
				image(-1.f, -1.f, false, 0u);
				image(-1.f, -1.f, false, 0u);
				const bool reserveLevee = !ctxRet.input.saisieReserveePrec;
				// et la toile repond de nouveau : un clic dans le vide deselectionne
				image(10.f, 10.f, false, 0u);
				image(10.f, 10.f, true, 0u);
				image(10.f, 10.f, false, 0u);
				const bool toileRepondEncore = stRet.selected != selAvant;
				snprintf(det, sizeof(det),
						 "reserve avant=%d, encore fausse a l'image de l'ouverture (le retard annonce)=%d, tenue ensuite=%d, levee apres fermeture=%d ; modale ouverte=%d ; clic dans le vide : "
						 "selection %d -> %d ; molette : zoom %.3f -> %.3f ; Suppr : noeuds %u -> %u ; « Annuler » ferme=%d ; la toile repond "
						 "de nouveau (selection -> %d)=%d",
						 reserveAvant ? 1 : 0, retardTenu ? 1 : 0, reserveTenue ? 1 : 0, reserveLevee ? 1 : 0, ouverte ? 1 : 0, selAvant,
						 selApresClic, (double)zoomAvant, (double)zoomApres, nAvant, nApres, fermee ? 1 : 0, stRet.selected,
						 toileRepondEncore ? 1 : 0);
				check("84. ② UNE MODALE POSSEDE SOURIS, MOLETTE ET CLAVIER : le selecteur de fichier du kit se DECLARE a chaque image "
					  "(`NkGuiInput::ReserverSaisie`, UNE IMAGE DE RETARD a l'ouverture, mesuree) ; l'hote neutralise alors l'entree des panneaux -- un clic dans le vide ne "
					  "deselectionne pas, la molette ne zoome pas, `Suppr` ne supprime pas ; « Annuler » dans la modale la ferme, la reserve "
					  "tombe et la toile repond de nouveau",
					  !reserveAvant && retardTenu && reserveTenue && ouverte && rienNeTraverse && fermee && reserveLevee
						  && toileRepondEncore,
					  det);
				stRet.choixImage.pickerOpen = false;
				stRet.choixImageNoeud = -1;
				stRet.choixImageIndex = -1;
				construire();
				image(-1.f, -1.f, false, 0u);
				// ── 85 / 87 / 88. ③ LES VARIABLES : renommer AVEC le popover ouvert (le cas exact de
				//    Rodolf), le detachement par defaut contre la modification ARMEE de la variable,
				//    et la recherche de la liste « Lier ».
				const int32 rc2 = stRet.doc.AddChild(pg, "", NkAuthor::Humain);
				{
					NkUINode &n2 = stRet.doc.nodes[(uint32)rc2];
					n2.shape = NkString("rect");
					n2.label = NkString("Second");
					n2.posX = 200.f;
					n2.posY = 100.f;
					n2.width.mode = NkSizeMode::Fixed;
					n2.width.value = 60.f;
					n2.height.mode = NkSizeMode::Fixed;
					n2.height.value = 60.f;
					NkRemplissage f2;
					f2.couleur = NkString("#1976d2");
					n2.fills.PushBack(f2);
				}
				const int32 viC = stRet.doc.CreerVariableCouleur("#1976d2", "Couleur 1");
				stRet.doc.CreerVariableCouleur("#0a555f", "P\xC3\xA9trole");
				stRet.doc.CreerVariableCouleur("#f79a28", "Orange");
				stRet.doc.nodes[(uint32)rc].fills[0].couleur = NkString("@couleur_1");
				stRet.doc.nodes[(uint32)rc2].fills[0].couleur = NkString("@couleur_1");
				stRet.Recompute(NkPaintRect{0.f, 0.f, 340.f, 900.f});
				stRet.SelectSingle(rc);
				// le popover de CE remplissage, ouvert -- comme apres « Créer une variable »
				stRet.picker = DesignState::DemandePicker();
				stRet.picker.ouvert = true;
				stRet.picker.id = ctxRet.GetId("##sonde.popover.var3");
				stRet.picker.genre = 1u;
				stRet.picker.noeud = rc;
				stRet.picker.index = 0;
				stRet.picker.ancre = {580.f, 200.f, 16.f, 16.f};
				for (int32 k = 0; k < 3; ++k)
					image(-1.f, -1.f, false, 0u);
				const float32 prx3 = 580.f - 250.f - 8.f, pry3 = 200.f - 8.f;
				const float32 x0P3 = prx3 + 8.f, x1P3 = prx3 + 250.f - 8.f;
				// la rangee de la variable, DERRIERE la rangee d'opacite (③ du 05/09 apres-midi)
				const float32 yVar = pry3 + 8.f + 26.f + 168.f + 26.f + 26.f + 3.f + 10.f;
				const bool popoverOuvert = ctxRet.popupDepth > 0;
				// ── 85. RENOMMER DANS LE RAIL, LE POPOVER OUVERT ET L'OBJET SELECTIONNE ────
				const nkgui::NkRect rNom = railRet.RectNom(0);
				const float32 nx = rNom.x + 10.f, ny = rNom.y + rNom.h * 0.5f;
				image(nx, ny, false, 0u);
				image(nx, ny, true, 0u);
				image(nx, ny, false, 0u);
				image(-1.f, -1.f, false, 0u);
				const bool enRenommage = railRet.EnRenommage() == 0;
				image(-1.f, -1.f, false, (uint32)'Z');
				image(-1.f, -1.f, false, 0u);
				const NkString nomZ = stRet.doc.variables.Empty() ? NkString("(aucune)") : stRet.doc.variables[(uint32)viC].nom;
				const bool renomme = nomZ.Data() && strstr(nomZ.Data(), "Z") != nullptr;
				snprintf(det, sizeof(det),
						 "popover ouvert=%d, objet selectionne=%d ; rect du nom %.0f,%.0f %.0fx%.0f ; renommage ouvert=%d ; apres la frappe "
						 "« Z » : nom « %s » -> renomme=%d",
						 popoverOuvert ? 1 : 0, stRet.selected, (double)rNom.x, (double)rNom.y, (double)rNom.w, (double)rNom.h,
						 enRenommage ? 1 : 0, nomZ.Data() ? nomZ.Data() : "?", renomme ? 1 : 0);
				check("85. ③ RENOMMER UNE VARIABLE DANS LE RAIL, LE SELECTEUR DE COULEUR OUVERT ET L'OBJET SELECTIONNE (le cas exact de "
					  "Rodolf) : le clic sur le nom ouvre le renommage, une frappe l'ecrit -- avant, le rail refusait TOUT clic tant qu'un "
					  "popup etait ouvert, et deselectionner « reparait » le rail en fermant le popover",
					  popoverOuvert && enRenommage && renomme, det);
				// ── 87. LE SELECTEUR DETACHE PAR DEFAUT, ET MODIFIE LA VARIABLE UNE FOIS ARME ──
				{
					// ⚠️ LE MONTAGE ROUVRE LA DEMANDE (07/09), ET C'EST LE COMPORTEMENT QUI
					//    A CHANGE, PAS L'ASSERTION. Le clic sur le rail de l'essai 85 est un
					//    clic HORS du popover : depuis ce lot il le FERME -- c'est le geste
					//    que Rodolf demandait. Ce montage-ci comptait sur un popover qui
					//    RESSUSCITAIT tout seul a l'image suivante ; il rouvre donc la
					//    demande, comme la main le ferait en recliquant la pastille.
					//    *Un essai qui s'appuie sur un defaut le protege.*
					stRet.picker = DesignState::DemandePicker();
					stRet.picker.ouvert = true;
					stRet.picker.id = ctxRet.GetId("##sonde.popover.var3");
					stRet.picker.genre = 1u;
					stRet.picker.noeud = rc;
					stRet.picker.index = 0;
					stRet.picker.ancre = {580.f, 200.f, 16.f, 16.f};
					for (int32 kr = 0; kr < 2; ++kr)
						image(-1.f, -1.f, false, 0u);
					const NkString valAvant = stRet.doc.variables[(uint32)viC].valeur;
					// un clic dans le carre saturation / valeur, comme la main (sonde 60h)
					const float32 xSV = x0P3 + 80.f, ySV = pry3 + 8.f + 26.f + 80.f;
					image(xSV, ySV, false, 0u);
					image(xSV, ySV, true, 0u);
					image(xSV, ySV, false, 0u);
					image(-1.f, -1.f, false, 0u);
					const NkString cRc = stRet.doc.nodes[(uint32)rc].fills[0].couleur;
					const NkString cRc2 = stRet.doc.nodes[(uint32)rc2].fills[0].couleur;
					const NkString valApres = stRet.doc.variables[(uint32)viC].valeur;
					const bool detache = NkHexLisible(cRc.Data()) && !NkEstReference(cRc.Data())
										 && NkComponentDecl::StrEq(cRc2.Data(), "@couleur_1")
										 && NkComponentDecl::StrEq(valApres.Data(), valAvant.Data());
					const bool piedD = stRet.status.Data() && strstr(stRet.status.Data(), "tach") != nullptr;
					// on relie, on ARME « Modifier la variable », et on refait le meme geste
					stRet.doc.nodes[(uint32)rc].fills[0].couleur = NkString("@couleur_1");
					stRet.picker.synchro = 0xFFFFFFFFu;
					image(-1.f, -1.f, false, 0u);
					image(-1.f, -1.f, false, 0u);
					const float32 xMod = x0P3 + (x1P3 - x0P3) * 0.5f, yMod = yVar + 24.f;
					image(xMod, yMod, false, 0u);
					image(xMod, yMod, true, 0u);
					image(xMod, yMod, false, 0u);
					image(-1.f, -1.f, false, 0u);
					const float32 xSV2 = x0P3 + 120.f, ySV2 = pry3 + 8.f + 26.f + 40.f;
					image(xSV2, ySV2, false, 0u);
					image(xSV2, ySV2, true, 0u);
					image(xSV2, ySV2, false, 0u);
					image(-1.f, -1.f, false, 0u);
					const NkString valArmee = stRet.doc.variables[(uint32)viC].valeur;
					const NkString cRcArme = stRet.doc.nodes[(uint32)rc].fills[0].couleur;
					const bool variableModifiee = !NkComponentDecl::StrEq(valArmee.Data(), valAvant.Data())
												  && NkComponentDecl::StrEq(cRcArme.Data(), "@couleur_1")
												  && NkComponentDecl::StrEq(stRet.doc.nodes[(uint32)rc2].fills[0].couleur.Data(), "@couleur_1");
					const bool piedU = stRet.status.Data() && strstr(stRet.status.Data(), "suivent") != nullptr;
					snprintf(det, sizeof(det),
							 "par defaut : le remplissage passe de « @couleur_1 » a « %s » (litteral), l'autre reste « %s », la variable garde "
							 "%s (avant %s) -> detache=%d, pied « %s » ; arme : la variable passe a %s, les DEUX remplissages restent lies -> "
							 "modifiee=%d, pied dit les usages=%d",
							 cRc.Data() ? cRc.Data() : "?", cRc2.Data() ? cRc2.Data() : "?", valApres.Data() ? valApres.Data() : "?",
							 valAvant.Data() ? valAvant.Data() : "?", detache ? 1 : 0, piedD ? "dit" : "muet",
							 valArmee.Data() ? valArmee.Data() : "?", variableModifiee ? 1 : 0, piedU ? 1 : 0);
					check("87. ③ LE SELECTEUR DETACHE PAR DEFAUT (le retour de Rodolf : lier ne doit pas dire modifier) : sur un remplissage "
						  "lie, changer la couleur pose une couleur LOCALE, la variable et les autres remplissages ne bougent pas, et le pied "
						  "le dit ; « Modifier la variable (xN) » arme le geste explicite : la variable change et tout ce qui la reference suit",
						  detache && piedD && variableModifiee && piedU, det);
				}
				// ── 88. LA RECHERCHE DE LA LISTE « Lier » ──────────────────────────
				{
					// un remplissage LITTERAL (la liste « Lier » ne s'offre qu'a lui), le popover ouvert
					stRet.doc.nodes[(uint32)rc].fills[0].couleur = NkString("#123456");
					stRet.picker.synchro = 0xFFFFFFFFu;
					image(-1.f, -1.f, false, 0u);
					image(-1.f, -1.f, false, 0u);
					// « Lier ˅ », a droite de la rangee
					const float32 xLier = x1P3 - 30.f;
					image(xLier, yVar, false, 0u);
					image(xLier, yVar, true, 0u);
					image(xLier, yVar, false, 0u);
					image(-1.f, -1.f, false, 0u);
					// trois variables, aucune frappe : trois lignes ; « Ora » : une seule (Orange)
					const uint32 nVars = (uint32)stRet.doc.variables.Size();
					image(-1.f, -1.f, false, (uint32)'O');
					image(-1.f, -1.f, false, (uint32)'r');
					image(-1.f, -1.f, false, (uint32)'a');
					image(-1.f, -1.f, false, 0u);
					// la PREMIERE ligne de la liste filtree est desormais « Orange » : la lier
					const float32 yLigne1 = yVar + 44.f - 10.f + 10.f; // rv.y + 44 + 10 (centre de la 1re ligne)
					image(x0P3 + 60.f, yLigne1, false, 0u);
					image(x0P3 + 60.f, yLigne1, true, 0u);
					image(x0P3 + 60.f, yLigne1, false, 0u);
					image(-1.f, -1.f, false, 0u);
					const NkString refF = stRet.doc.nodes[(uint32)rc].fills[0].couleur;
					const NkVariable *varO = stRet.doc.TrouverVariable(refF.Data());
					const bool lieeAOrange = varO && varO->nom.Data() && strstr(varO->nom.Data(), "Orange") != nullptr;
					snprintf(det, sizeof(det),
							 "%u variables au document ; apres avoir tape « Ora » dans la recherche, la premiere ligne de la liste lie -> "
							 "remplissage « %s » = variable « %s » (attendu Orange) -> %d",
							 nVars, refF.Data() ? refF.Data() : "?", varO ? (varO->nom.Empty() ? varO->cle.Data() : varO->nom.Data()) : "(aucune)",
							 lieeAOrange ? 1 : 0);
					check("88. ③ LA RECHERCHE DE LA LISTE « Lier » (Rodolf : « il pourrait y avoir des centaines de variables ») : taper "
						  "« Ora » ne laisse que « Orange », et la premiere ligne le lie -- le filtre porte sur le nom et sur la cle",
						  lieeAOrange, det);
				}
				if (ctxRet.popupDepth > 0)
					ctxRet.ClosePopup();
				stRet.picker = DesignState::DemandePicker();
				image(-1.f, -1.f, false, 0u);
			}
		}
		// ── 86. ⑧ LE CADRE DE CROP EST PLUS GRAND QUE LE NOEUD, ET SES POIGNEES GAGNENT (05/09).
		//    Le retour de Rodolf : « ce n'est pas modifiable et je ne sais meme pas si c'est
		//    visible ». Une image 4:3 dans un noeud 5:3 : passer en Crop par le popover pose la
		//    fenetre COUVRANTE (100 % x 80 %), le cadre de l'image entiere est STRICTEMENT plus
		//    haut que le noeud, tirer sa poignee du haut change `crop` (le noeud ne bouge pas),
		//    et le popover ferme, la poignee du NOEUD redimensionne de nouveau le noeud.
		{
			static nkgui::NkGuiContext ctxCr;
			char det[900];
			if (!ctxCr.Init(600, 900)) {
				check("86. le cadre de crop", false, "Init a refuse");
			} else {
				// l'image 4 x 3 de la sonde (quatre bandes, pour la voir a l'oeil dans le fichier)
				bool png43 = false;
				{
					NkImage img;
					if (img.Create(4u, 3u, math::NkColor(), 4) && img.Pixels()) {
						uint8 *px = img.Pixels();
						for (int32 y = 0; y < 3; ++y)
							for (int32 x = 0; x < 4; ++x) {
								const uint8 v = (uint8)(40 + 60 * y);
								px[(y * 4 + x) * 4 + 0] = (uint8)(v + 20 * x);
								px[(y * 4 + x) * 4 + 1] = v;
								px[(y * 4 + x) * 4 + 2] = (uint8)(255 - v);
								px[(y * 4 + x) * 4 + 3] = 255;
							}
						png43 = img.SavePNG("sonde_crop_4x3.png");
					}
				}
				static DesignState stCr;
				stCr.doc.NewDocument("Toile", NkAuthor::Humain);
				stCr.cheminActif = NkString();
				stCr.images.Vider();
				renderdetail::NkPoserFournisseurImages(&NkObtenirImageDuDocument, &stCr);
				const int32 pgC = stCr.doc.AddChild(0, "", NkAuthor::Humain);
				stCr.doc.nodes[(uint32)pgC].shape = NkString("frame");
				stCr.doc.nodes[(uint32)pgC].layout.kind = NkLayoutKind::Free;
				stCr.doc.nodes[(uint32)pgC].width.mode = NkSizeMode::Fixed;
				stCr.doc.nodes[(uint32)pgC].width.value = 300.f;
				stCr.doc.nodes[(uint32)pgC].height.mode = NkSizeMode::Fixed;
				stCr.doc.nodes[(uint32)pgC].height.value = 300.f;
				const int32 rcC = stCr.doc.AddChild(pgC, "", NkAuthor::Humain);
				{
					NkUINode &n = stCr.doc.nodes[(uint32)rcC];
					n.shape = NkString("rect");
					n.posX = 100.f;
					n.posY = 100.f;
					n.width.mode = NkSizeMode::Fixed;
					n.width.value = 100.f; // 5:3 -- l'image est 4:3 : « couvrir » coupe en HAUTEUR
					n.height.mode = NkSizeMode::Fixed;
					n.height.value = 60.f;
					NkRemplissage f;
					f.genre = NkString("image");
					f.image = NkString("sonde_crop_4x3.png");
					n.fills.PushBack(f); // cadrage vide = « fill » (couvrir), le defaut
				}
				stCr.Recompute(NkPaintRect{0.f, 0.f, 340.f, 900.f});
				stCr.SelectSingle(rcC);
				static PreviewPanel toileCr(&stCr);
				static InspectorPanel inspCr(&stCr);
				NkEditorFrameContext ec;
				ec.ui = &ctxCr;
				ec.dt = 0.016f;
				auto image = [&](float32 mx, float32 my, bool bas) {
					ctxCr.input.mousePos = {mx, my};
					ctxCr.input.mouseDown[0] = bas;
					ctxCr.BeginFrame(0.016f);
					ctxCr.BeginLayout({0.f, 0.f, 340.f, 900.f});
					toileCr.OnUI(ec);
					ctxCr.BeginLayout({340.f, 0.f, 260.f, 900.f});
					inspCr.OnUI(ec);
					NkDessinerPickerDemande(ctxCr, stCr);
					ctxCr.EndFrame();
				};
				auto cliquer = [&](float32 x, float32 y) {
					image(x, y, false); // le survol precede l'appui
					image(x, y, true);
					image(x, y, false);
					image(-1.f, -1.f, false);
				};
				auto boite = [&](float32 &px, float32 &py) {
					px = 1e9f;
					py = 1e9f;
					for (uint32 i = 0; i < (uint32)ctxCr.dlOverlay.vtx.Size(); ++i) {
						if (ctxCr.dlOverlay.vtx[i].pos.x < px) px = ctxCr.dlOverlay.vtx[i].pos.x;
						if (ctxCr.dlOverlay.vtx[i].pos.y < py) py = ctxCr.dlOverlay.vtx[i].pos.y;
					}
				};
				// le popover d'image, ancre en bas a droite (il ne recouvre pas la toile)
				stCr.picker = DesignState::DemandePicker();
				stCr.picker.ouvert = true;
				stCr.picker.id = ctxCr.GetId("##sonde.popover.crop8");
				stCr.picker.genre = 1u;
				stCr.picker.noeud = rcC;
				stCr.picker.index = 0;
				stCr.picker.ancre = {580.f, 200.f, 16.f, 16.f}; // le popover tient sans etre repousse par le bas
				for (int32 k = 0; k < 3; ++k)
					image(-1.f, -1.f, false);
				float32 px = 0.f, py = 0.f;
				boite(px, py); // pour le rapport : ce que l'overlay montre
				// LE RECTANGLE DU POPOVER, calcule comme le code le calcule (l'ancre, la largeur 250,
				// la hauteur d'un remplissage image hors crop : 8 + 26 + 116 + 5 x 26 + 8 = 288)
				const float32 prx = 580.f - 250.f - 8.f, pry = 200.f - 8.f;
				const float32 x0P = prx + 8.f;
				// la rangee « Cadrage » : 8 (marge) + 26 (vignettes) + 116 (apercu) + 26 (retirer le fond)
				const float32 yCad = pry + 8.f + 26.f + 116.f + 26.f + 3.f + 10.f;
				const float32 xCombo = x0P + 62.f + 36.f;
				cliquer(xCombo, yCad);					   // ouvre le menu des cadrages
				// la liste s'ouvre AU-DESSUS : rl.y = rm.y - 5 x 20 - 4, l'entree k=4 (« Crop ») occupe
				// [rl.y + 2 + 80, +20[ -- son CENTRE est a yCad - 22 (yCad est le centre du combo)
				cliquer(x0P + 62.f + 20.f, yCad - 22.f);
				image(-1.f, -1.f, false);
				const NkRemplissage &fc = stCr.doc.nodes[(uint32)rcC].fills[0];
				// ⚠️ ON FIGE LES VALEURS : `fc` et `fa` designent LE MEME remplissage (des references),
				//    donc « avant » et « apres » liraient la meme chose -- le glisser passerait pour
				//    sans effet. Le detail imprimait deja les valeurs d'apres pour les deux.
				const float32 cX0 = fc.cropX, cY0 = fc.cropY, cW0 = fc.cropW, cH0 = fc.cropH;
				const bool estCrop = NkComponentDecl::StrEq(fc.cadrage.Data(), "crop");
				// couvrir : k = max(100/4, 60/3) = 25 -> 100 x 75 ; la fenetre = 100 % x 80 %, centree
				const bool couvrant = cW0 > 0.995f && cH0 > 0.795f && cH0 < 0.805f && cY0 > 0.095f && cY0 < 0.105f;
				NkLayoutResult scrC;
				stCr.ProjectToScreen(scrC);
				const NkPaintRect rsC = scrC.At(rcC);
				NkPaintRect entier;
				renderdetail::NkGCadreImageEntiere(rsC, fc, entier);
				const bool cadrePlusGrand = entier.h > rsC.h + 8.f && entier.y < rsC.y - 4.f && entier.w > rsC.w - 0.5f;
				// la poignee HAUT-MILIEU du cadre, tiree vers le bas de 12 px : le crop change, le noeud non
				const float32 hx = entier.x + entier.w * 0.5f, hy = entier.y;
				const NkUINode avant = stCr.doc.nodes[(uint32)rcC];
				image(hx, hy, false);
				image(hx, hy, true);
				for (int32 k = 1; k <= 3; ++k)
					image(hx, hy + 4.f * (float32)k, true);
				image(hx, hy + 12.f, false);
				image(-1.f, -1.f, false);
				const NkRemplissage &fa = stCr.doc.nodes[(uint32)rcC].fills[0];
				const NkUINode &apres = stCr.doc.nodes[(uint32)rcC];
				const bool cropChange = fa.cropH < cH0 - 0.01f || fa.cropH > cH0 + 0.01f || fa.cropY < cY0 - 0.005f || fa.cropY > cY0 + 0.005f;
				const bool noeudIntact = apres.posX == avant.posX && apres.posY == avant.posY && apres.width.value == avant.width.value
										 && apres.height.value == avant.height.value;
				// le popover FERME : la poignee du NOEUD (haut-milieu) redimensionne de nouveau
				if (ctxCr.popupDepth > 0)
					ctxCr.ClosePopup();
				stCr.picker = DesignState::DemandePicker();
				for (int32 k = 0; k < 3; ++k)
					image(-1.f, -1.f, false);
				stCr.ProjectToScreen(scrC);
				const NkPaintRect rs2 = scrC.At(rcC);
				const float32 nx = rs2.x + rs2.w * 0.5f, ny = rs2.y;
				const float32 hAvant = stCr.doc.nodes[(uint32)rcC].height.value;
				image(nx, ny, false);
				image(nx, ny, true);
				for (int32 k = 1; k <= 3; ++k)
					image(nx, ny + 4.f * (float32)k, true);
				image(nx, ny + 12.f, false);
				image(-1.f, -1.f, false);
				const float32 hApres = stCr.doc.nodes[(uint32)rcC].height.value;
				const bool noeudRepondEncore = hApres < hAvant - 1.f;
				snprintf(det, sizeof(det),
						 "png 4x3=%d ; popover attendu a (%.0f, %.0f), overlay vu a (%.0f, %.0f) ; cadrage « %s » (crop=%d) ; crop pose X %.3f Y %.3f L %.3f H %.3f (attendu Y 0,100 H 0,800) -> couvrant=%d ; "
						 "noeud ecran %.0fx%.0f, cadre %.0fx%.0f a y %.0f (noeud y %.0f) -> plus grand=%d ; poignee du cadre tiree : crop H %.3f Y %.3f "
						 "(change=%d), noeud intact=%d ; popover ferme, poignee du noeud : hauteur %.0f -> %.0f (repond=%d)",
						 png43 ? 1 : 0, (double)prx, (double)pry, (double)px, (double)py, fc.cadrage.Data() ? fc.cadrage.Data() : "(fill)", estCrop ? 1 : 0, (double)cX0, (double)cY0,
						 (double)cW0, (double)cH0, couvrant ? 1 : 0, (double)rsC.w, (double)rsC.h, (double)entier.w, (double)entier.h,
						 (double)entier.y, (double)rsC.y, cadrePlusGrand ? 1 : 0, (double)fa.cropH, (double)fa.cropY, cropChange ? 1 : 0,
						 noeudIntact ? 1 : 0, (double)hAvant, (double)hApres, noeudRepondEncore ? 1 : 0);
				check("86. ⑧ LE CROP EST VISIBLE ET MODIFIABLE : passer en Crop par le popover pose la fenetre COUVRANTE (image 4:3 dans un "
					  "noeud 5:3 -> 100 % x 80 %, centree), le cadre de l'image entiere est STRICTEMENT plus grand que le noeud, tirer sa "
					  "poignee change `crop` sans bouger le noeud, et le popover ferme la poignee du noeud le redimensionne de nouveau",
					  png43 && estCrop && couvrant && cadrePlusGrand && cropChange && noeudIntact && noeudRepondEncore, det);
			}
		}
		// ── 89. ⑨ AUCUN IDENTIFIANT NE SE PEINT DANS UN CHAMP (05/09). La capture de Rodolf
		//    (`2026-09-05_app_popover_etiquette_fuit_dans_champ.png`) montrait la rangee RGB
		//    « 179 | 184p.pop.m2 | 100 » : `InputText` peint la partie du libelle situee AVANT
		//    `##`, et les identifiants des champs de nombre n'en portaient pas. Le temoin lit
		//    l'INTROSPECTION : tout champ pose pendant la frappe doit avoir une partie visible
		//    VIDE ou humaine -- jamais quelque chose qui ressemble a un identifiant (un point,
		//    aucune espace).
		{
			static nkgui::NkGuiContext ctxId;
			char det[700];
			static nkgui::NkGuiFont policeId;
			const bool policeOkId = policeId.LoadEmbedded(nkentseu::NkEmbeddedFontId::Inter, 14.f, false);
			if (!ctxId.Init(600, 900) || !policeOkId) {
				check("89. aucun identifiant peint dans un champ", false, "Init ou police a refuse");
			} else {
				ctxId.font = &policeId;
				nkgui::NkGuiIntrospectActiver(ctxId, true);
				static DesignState stId;
				stId.doc.NewDocument("Toile", NkAuthor::Humain);
				stId.cheminActif = NkString();
				const int32 pgI = stId.doc.AddChild(0, "", NkAuthor::Humain);
				stId.doc.nodes[(uint32)pgI].shape = NkString("frame");
				stId.doc.nodes[(uint32)pgI].layout.kind = NkLayoutKind::Free;
				stId.doc.nodes[(uint32)pgI].width.mode = NkSizeMode::Fixed;
				stId.doc.nodes[(uint32)pgI].width.value = 300.f;
				stId.doc.nodes[(uint32)pgI].height.mode = NkSizeMode::Fixed;
				stId.doc.nodes[(uint32)pgI].height.value = 300.f;
				const int32 rcI = stId.doc.AddChild(pgI, "", NkAuthor::Humain);
				{
					NkUINode &n = stId.doc.nodes[(uint32)rcI];
					n.shape = NkString("rect");
					n.posX = 100.f;
					n.posY = 100.f;
					n.width.mode = NkSizeMode::Fixed;
					n.width.value = 80.f;
					n.height.mode = NkSizeMode::Fixed;
					n.height.value = 80.f;
					NkRemplissage f;
					f.couleur = NkString("#1976d2");
					n.fills.PushBack(f);
				}
				stId.Recompute(NkPaintRect{0.f, 0.f, 340.f, 900.f});
				stId.SelectSingle(rcI);
				static InspectorPanel inspId(&stId);
				NkEditorFrameContext ec;
				ec.ui = &ctxId;
				ec.dt = 0.016f;
				auto image = [&](float32 mx, float32 my, bool bas) {
					ctxId.input.mousePos = {mx, my};
					ctxId.input.mouseDown[0] = bas;
					ctxId.BeginFrame(0.016f);
					ctxId.BeginLayout({340.f, 0.f, 260.f, 900.f});
					inspId.OnUI(ec);
					NkDessinerPickerDemande(ctxId, stId);
					ctxId.EndFrame();
				};
				stId.picker = DesignState::DemandePicker();
				stId.picker.ouvert = true;
				stId.picker.id = ctxId.GetId("##sonde.popover.id9");
				stId.picker.genre = 1u;
				stId.picker.noeud = rcI;
				stId.picker.index = 0;
				stId.picker.ancre = {580.f, 200.f, 16.f, 16.f};
				for (int32 k = 0; k < 3; ++k)
					image(-1.f, -1.f, false);
				// le champ d'OPACITE de la rangee du modele : 8 (marge) + 26 (vignettes) + 168
				// (selecteur) ; `NkRangeeModele` pose l'opacite a x1 - 12 - 34, 34 x 20
				const float32 prxI = 580.f - 250.f - 8.f, pryI = 200.f - 8.f;
				const float32 x1I = prxI + 250.f - 8.f;
				const float32 xOp = x1I - 12.f - 17.f, yOp = pryI + 8.f + 26.f + 168.f + 3.f + 10.f;
				image(xOp, yOp, false);
				image(xOp, yOp, true);
				image(xOp, yOp, false); // un clic SANS glisser ouvre la saisie
				image(xOp, yOp, false);
				// les notes de CETTE image : tout champ pose, avec son libelle
				int32 nbNotes = 0;
				const nkgui::NkGuiNote *notes = nkgui::NkGuiIntrospectNotes(ctxId, nbNotes);
				int32 champs = 0, fuites = 0;
				char premiere[80];
				premiere[0] = 0;
				for (int32 i = 0; i < nbNotes; ++i) {
					if (notes[i].nature != nkgui::NkGuiNature::Champ)
						continue;
					++champs;
					const char *lib = notes[i].libelle;
					// la partie VISIBLE : ce qui precede « ## »
					int32 fin = 0;
					while (lib[fin] && !(lib[fin] == '#' && lib[fin + 1] == '#'))
						++fin;
					bool point = false, espace = false;
					for (int32 k = 0; k < fin; ++k) {
						if (lib[k] == '.')
							point = true;
						if (lib[k] == ' ')
							espace = true;
					}
					if (fin > 0 && point && !espace) { // ça ressemble a un identifiant, pas a un libelle
						++fuites;
						if (!premiere[0])
							snprintf(premiere, sizeof(premiere), "%s", lib);
					}
				}
				snprintf(det, sizeof(det),
						 "%d note(s) cette image, dont %d champ(s) ; libelles qui ressemblent a un identifiant (un point, aucune "
						 "espace) : %d%s%s",
						 nbNotes, champs, fuites, premiere[0] ? " -- le premier : " : "", premiere[0] ? premiere : "");
				check("89. ⑨ AUCUN IDENTIFIANT NE SE PEINT DANS UN CHAMP : le champ d'opacite du selecteur mis en frappe, l'introspection "
					  "ne montre AUCUN champ dont la partie visible du libelle ressemble a un identifiant (avant : « insp.pop.m2 » etait "
					  "peint a cote du champ, par-dessus son voisin)",
					  champs > 0 && fuites == 0, det);
				if (ctxId.popupDepth > 0)
					ctxId.ClosePopup();
				stId.picker = DesignState::DemandePicker();
				image(-1.f, -1.f, false);
			}
		}
		// ── 90. ④ LA MINIATURE DE LA LIGNE « REMPLISSAGES » EST L'IMAGE (05/09). Rodolf : « la
		//    miniature dans Remplissages ne correspond pas a celle de l'image chargee » -- elle
		//    peignait un damier pour TOUTE ligne image. Temoin : la liste de dessin de
		//    l'inspecteur porte une commande TEXTUREE au handle de l'image, dans la pastille ;
		//    sans texture (ou source introuvable), le damier revient -- et c'est ce qu'il doit
		//    dire.
		{
			static nkgui::NkGuiContext ctxMin;
			char det[700];
			static nkgui::NkGuiFont policeMin;
			const bool policeOkMin = policeMin.LoadEmbedded(nkentseu::NkEmbeddedFontId::Inter, 14.f, false);
			if (!ctxMin.Init(600, 900) || !policeOkMin) {
				check("90. la miniature de la ligne image", false, "Init ou police a refuse");
			} else {
				ctxMin.font = &policeMin;
				static DesignState stMin;
				stMin.doc.NewDocument("Toile", NkAuthor::Humain);
				stMin.cheminActif = NkString();
				stMin.images.Vider();
				stMin.images.televerser = [](void *, const uint8 *, int32, int32) -> uint32 { return 0x4E4B0400u; };
				renderdetail::NkPoserFournisseurImages(&NkObtenirImageDuDocument, &stMin);
				const int32 pgM = stMin.doc.AddChild(0, "", NkAuthor::Humain);
				stMin.doc.nodes[(uint32)pgM].shape = NkString("frame");
				stMin.doc.nodes[(uint32)pgM].layout.kind = NkLayoutKind::Free;
				stMin.doc.nodes[(uint32)pgM].width.mode = NkSizeMode::Fixed;
				stMin.doc.nodes[(uint32)pgM].width.value = 300.f;
				stMin.doc.nodes[(uint32)pgM].height.mode = NkSizeMode::Fixed;
				stMin.doc.nodes[(uint32)pgM].height.value = 300.f;
				const int32 rcM = stMin.doc.AddChild(pgM, "", NkAuthor::Humain);
				{
					NkUINode &n = stMin.doc.nodes[(uint32)rcM];
					n.shape = NkString("rect");
					n.posX = 100.f;
					n.posY = 100.f;
					n.width.mode = NkSizeMode::Fixed;
					n.width.value = 80.f;
					n.height.mode = NkSizeMode::Fixed;
					n.height.value = 80.f;
					NkRemplissage f;
					f.genre = NkString("image");
					f.image = NkString("sonde_crop_4x3.png"); // ecrite par la sonde 86
					n.fills.PushBack(f);
				}
				stMin.Recompute(NkPaintRect{0.f, 0.f, 340.f, 900.f});
				stMin.SelectSingle(rcM);
				static InspectorPanel inspMin(&stMin);
				NkEditorFrameContext ec;
				ec.ui = &ctxMin;
				ec.dt = 0.016f;
				auto image = [&]() {
					ctxMin.input.mousePos = {-1.f, -1.f};
					ctxMin.BeginFrame(0.016f);
					ctxMin.BeginLayout({340.f, 0.f, 260.f, 900.f});
					inspMin.OnUI(ec);
					ctxMin.EndFrame();
				};
				image();
				image();
				// la commande texturee au handle de l'image, et sa boite : une pastille de 16 px
				auto boiteTexture = [&](uint32 tex, float32 &x0, float32 &y0, float32 &x1, float32 &y1) -> uint32 {
					x0 = 1e9f; y0 = 1e9f; x1 = -1e9f; y1 = -1e9f;
					uint32 n = 0u;
					const nkgui::NkGuiDrawList &dlM = ctxMin.dl;
					for (uint32 c = 0; c < (uint32)dlM.cmds.Size(); ++c) {
						const nkgui::NkGuiDrawCmd &cm = dlM.cmds[c];
						if (cm.type != nkgui::NkGuiDrawCmdType::TexturedTriangles || cm.texId != tex)
							continue;
						++n;
						for (uint32 k = 0; k < cm.idxCount; ++k) {
							const uint32 ii = cm.idxOffset + k;
							if (ii >= (uint32)dlM.idx.Size())
								break;
							const uint32 vi = dlM.idx[ii];
							if (vi >= (uint32)dlM.vtx.Size())
								continue;
							const nkgui::NkVec2 &p = dlM.vtx[vi].pos;
							if (p.x < x0) x0 = p.x;
							if (p.x > x1) x1 = p.x;
							if (p.y < y0) y0 = p.y;
							if (p.y > y1) y1 = p.y;
						}
					}
					return n;
				};
				float32 ax0 = 0.f, ay0 = 0.f, ax1 = 0.f, ay1 = 0.f;
				const uint32 nCmd = boiteTexture(0x4E4B0400u, ax0, ay0, ax1, ay1);
				// une image 4 x 3 « fit » dans 14 px : 14 x 10,5, centree
				const bool taille = nCmd > 0u && (ax1 - ax0) > 13.f && (ax1 - ax0) < 15.f && (ay1 - ay0) > 9.f && (ay1 - ay0) < 12.f;
				// la source retiree : plus de commande a ce handle (le damier revient)
				stMin.doc.nodes[(uint32)rcM].fills[0].image = NkString("introuvable_90.png");
				stMin.editionGeneration++;
				stMin.images.Vider();
				image();
				image();
				float32 bx0 = 0.f, by0 = 0.f, bx1 = 0.f, by1 = 0.f;
				const uint32 nCmd2 = boiteTexture(0x4E4B0400u, bx0, by0, bx1, by1);
				snprintf(det, sizeof(det),
						 "image chargee : %u commande(s) texturee(s) au handle de l'image, boite %.1f x %.1f (une image 4:3 « fit » dans "
						 "14 px : 14 x 10,5) -> %d ; source introuvable : %u commande(s) (le damier revient)",
						 nCmd, (double)(ax1 - ax0), (double)(ay1 - ay0), taille ? 1 : 0, nCmd2);
				check("90. ④ LA MINIATURE DE LA LIGNE IMAGE EST L'IMAGE : la pastille de la rangee « Remplissages » emet une commande "
					  "TEXTUREE au handle du cache (celui de la toile), cadree « fit » dans les 14 px utiles ; source introuvable, le "
					  "damier revient -- il ne veut plus dire « une image » mais « rien a montrer »",
					  taille && nCmd2 == 0u, det);
			}
		}
		// ── 91. ⑤ « VOIR LE COMPOSANT » MARCHE AUSSI SUR UNE DECLARATION (05/09). Rodolf a fait
		//    le geste sur « Bouton_Connexion » et a lu « (pas une instance) » : pour lui c'est un
		//    composant, et il veut sa hierarchie. Trois cas mesures : une INSTANCE (comme avant),
		//    un noeud qui porte le NOM d'une declaration, et un noeud qui n'est ni l'un ni
		//    l'autre -- celui-la seul garde l'entree grisee, avec sa raison.
		{
			char det[700];
			static DesignState stD;
			stD.doc.NewDocument("Toile", NkAuthor::Humain);
			const int32 pgD = stD.doc.AddChild(0, "", NkAuthor::Humain);
			stD.doc.nodes[(uint32)pgD].shape = NkString("frame");
			stD.doc.nodes[(uint32)pgD].layout.kind = NkLayoutKind::Free;
			stD.doc.nodes[(uint32)pgD].width.mode = NkSizeMode::Fixed;
			stD.doc.nodes[(uint32)pgD].width.value = 300.f;
			stD.doc.nodes[(uint32)pgD].height.mode = NkSizeMode::Fixed;
			stD.doc.nodes[(uint32)pgD].height.value = 300.f;
			// une carte a extraire : elle deviendra la declaration « Bouton_Connexion »
			const int32 carte = stD.doc.AddChild(pgD, "", NkAuthor::Humain);
			stD.doc.nodes[(uint32)carte].shape = NkString("rect");
			stD.doc.nodes[(uint32)carte].label = NkString("Bouton_Connexion");
			stD.doc.nodes[(uint32)carte].width.mode = NkSizeMode::Fixed;
			stD.doc.nodes[(uint32)carte].width.value = 120.f;
			stD.doc.nodes[(uint32)carte].height.mode = NkSizeMode::Fixed;
			stD.doc.nodes[(uint32)carte].height.value = 40.f;
			const int32 txtD = stD.doc.AddChild(carte, "", NkAuthor::Humain);
			stD.doc.nodes[(uint32)txtD].shape = NkString("text");
			stD.doc.nodes[(uint32)txtD].text = NkString("Se connecter");
			// un noeud ordinaire, qui ne sera ni instance ni declaration
			const int32 autre = stD.doc.AddChild(pgD, "", NkAuthor::Humain);
			stD.doc.nodes[(uint32)autre].shape = NkString("rect");
			stD.doc.nodes[(uint32)autre].label = NkString("Rectangle nu");
			stD.SelectSingle(carte);
			const bool extrait = NkAppliquerActionCtx(stD, carte, NkActionCtx::ExtraireComposant);
			const uint32 nDecl = (uint32)stD.doc.declarations.Size();
			// 1. l'instance (le noeud extrait EST une instance) : sa declaration s'ouvre
			stD.composantVu = -1;
			NkAppliquerActionCtx(stD, carte, NkActionCtx::VoirComposant);
			const int32 vuInstance = stD.composantVu;
			// 2. un noeud qui porte le NOM de la declaration, sans en etre une instance
			const int32 jumeau = stD.doc.AddChild(pgD, "", NkAuthor::Humain);
			stD.doc.nodes[(uint32)jumeau].shape = NkString("rect");
			stD.doc.nodes[(uint32)jumeau].label = stD.doc.declarations.Empty()
													  ? NkString("Bouton_Connexion")
													  : stD.doc.declarations[0].identite.nom;
			stD.composantVu = -1;
			NkAppliquerActionCtx(stD, jumeau, NkActionCtx::VoirComposant);
			const int32 vuNom = stD.composantVu;
			// 3. un noeud ordinaire : rien ne s'ouvre, et la raison le dit
			stD.composantVu = -1;
			stD.status = NkString();
			NkAppliquerActionCtx(stD, autre, NkActionCtx::VoirComposant);
			const int32 vuAutre = stD.composantVu;
			const bool raison = stD.status.Data() && strstr(stD.status.Data(), "ni une instance, ni une d") != nullptr;
			// et l'entree du menu : active sur les deux premiers, grisee sur le troisieme
			const NkContexteCtx cI = NkContexteDepuisEtat(stD, carte, false);
			const NkContexteCtx cN = NkContexteDepuisEtat(stD, jumeau, false);
			const NkContexteCtx cA = NkContexteDepuisEtat(stD, autre, false);
			const bool menuOk = (cI.estInstance || cI.estDeclaration) && (cN.estInstance || cN.estDeclaration)
								&& !(cA.estInstance || cA.estDeclaration);
			snprintf(det, sizeof(det),
					 "extraction=%d, %u declaration(s) ; « Voir le composant » -> instance : %d, noeud qui porte le NOM : %d, noeud "
					 "ordinaire : %d (raison dite=%d) ; menu actif instance=%d nom=%d ordinaire=%d",
					 extrait ? 1 : 0, nDecl, vuInstance, vuNom, vuAutre, raison ? 1 : 0,
					 (cI.estInstance || cI.estDeclaration) ? 1 : 0, (cN.estInstance || cN.estDeclaration) ? 1 : 0,
					 (cA.estInstance || cA.estDeclaration) ? 1 : 0);
			check("91. ⑤ « VOIR LE COMPOSANT » S'OUVRE SUR UNE INSTANCE **ET** SUR UN NOEUD QUI PORTE LE NOM D'UNE DECLARATION ; un "
				  "noeud qui n'est ni l'un ni l'autre garde l'entree grisee, avec la raison qui dit les deux cas",
				  extrait && nDecl == 1u && vuInstance == 0 && vuNom == 0 && vuAutre == -1 && raison && menuOk, det);
		}
		// ── 92. ⑩ L'AIDE DE LA RANGEE « ARRONDI » NE DEBORDE PLUS SUR LE BOUTON (05/09, capture
		//    de Rodolf). Deux mesures : la porte de troncature elle-meme (elle coupe, pose « … »
		//    et ne depasse jamais la largeur demandee, accents compris), et la rangee dans
		//    l'inspecteur reel -- aucun sommet de TEXTE a droite de la colonne du bouton.
		{
			char det[800];
			static nkgui::NkGuiFont police92;
			const bool police92Ok = police92.LoadEmbedded(nkentseu::NkEmbeddedFontId::Inter, 9.f, false);
			bool porteOk = false;
			float32 wPleine = 0.f, wCoupee = 0.f;
			if (police92Ok) {
				static nkgui::NkGuiDrawList dlT;
				dlT.Reset();
				const char *aide = "par sommet : mode édition, champ R";
				wPleine = costume::Largeur(police92, aide);
				// une largeur volontairement trop courte : la porte doit couper
				wCoupee = costume::TexteTronque(dlT, police92, 0.f, 0.f, aide, wPleine * 0.5f, nkgui::NkColor{255, 255, 255, 255});
				// et une largeur suffisante : elle rend le texte entier, sans « … »
				const float32 wEntier = costume::TexteTronque(dlT, police92, 0.f, 40.f, aide, wPleine + 10.f,
															 nkgui::NkColor{255, 255, 255, 255});
				porteOk = wCoupee > 0.f && wCoupee <= wPleine * 0.5f + 0.01f && wEntier > wCoupee
						  && wEntier > wPleine - 0.01f && wEntier < wPleine + 0.01f;
			}
			// la rangee dans l'inspecteur REEL : un noeud a sommets (le cas de la capture)
			static nkgui::NkGuiContext ctx92;
			bool geomOk = false;
			float32 maxTexte = 0.f, bordBouton = 0.f, aideX0 = 0.f, aideX1 = 0.f;
			if (police92Ok && ctx92.Init(600, 900)) {
				ctx92.font = &police92;
				// ⚠️ L'INSPECTEUR PEINT AVEC LE COSTUME (`costume::Fontes()`), pas avec la police du
				//    contexte : sans lui, `F.px9` est invalide et la rangee ne peint RIEN -- la
				//    sonde mesurerait alors un debordement nul sur un texte absent. On le charge
				//    avec un televerseur factice (l'atlas se construit, seul l'envoi au GPU est
				//    simule) ; c'est la DERNIERE sonde du fichier, aucune autre n'en depend.
				struct FauxShell {
						bool UploadAppFont(nkgui::NkGuiFont &, uint32) {
							return true;
						}
				};
				FauxShell faux;
				costume::Fontes().Charger(faux, 1.f);
				static DesignState st92;
				st92.doc.NewDocument("Toile", NkAuthor::Humain);
				const int32 pg92 = st92.doc.AddChild(0, "", NkAuthor::Humain);
				st92.doc.nodes[(uint32)pg92].shape = NkString("frame");
				st92.doc.nodes[(uint32)pg92].layout.kind = NkLayoutKind::Free;
				st92.doc.nodes[(uint32)pg92].width.mode = NkSizeMode::Fixed;
				st92.doc.nodes[(uint32)pg92].width.value = 300.f;
				st92.doc.nodes[(uint32)pg92].height.mode = NkSizeMode::Fixed;
				st92.doc.nodes[(uint32)pg92].height.value = 300.f;
				const int32 rc92 = st92.doc.AddChild(pg92, "", NkAuthor::Humain);
				{
					NkUINode &n = st92.doc.nodes[(uint32)rc92];
					n.shape = NkString("rect");
					n.posX = 20.f;
					n.posY = 20.f;
					n.width.mode = NkSizeMode::Fixed;
					n.width.value = 120.f;
					n.height.mode = NkSizeMode::Fixed;
					n.height.value = 60.f;
					// des sommets materialises : c'est ce qui declenche l'aide « par sommet »
					for (uint32 k = 0; k < 4u; ++k) {
						NkPoint2 p;
						p.x = (k == 0u || k == 3u) ? -1.f : 1.f;
						p.y = (k < 2u) ? -1.f : 1.f;
						n.sommets.PushBack(p);
					}
				}
				st92.Recompute(NkPaintRect{0.f, 0.f, 340.f, 900.f});
				st92.SelectSingle(rc92);
				static InspectorPanel insp92(&st92);
				NkEditorFrameContext ec;
				ec.ui = &ctx92;
				ec.dt = 0.016f;
				const float32 panneauX = 340.f, panneauW = 260.f;
				for (int32 k = 0; k < 2; ++k) {
					ctx92.input.mousePos = {-1.f, -1.f};
					ctx92.BeginFrame(0.016f);
					ctx92.BeginLayout({panneauX, 0.f, panneauW, 900.f});
					insp92.OnUI(ec);
					ctx92.EndFrame();
				}
				// LE PEINT, PAS UNE GEOMETRIE DEVINEE : le panneau expose les deux rectangles de
				// la rangee « Arrondi » a sa derniere image (patron `RectNom`)
				const nkgui::NkRect rAide = insp92.RectAideArrondi();
				const nkgui::NkRect rBtn = insp92.RectBoutonArrondi();
				bordBouton = rBtn.w > 0.f ? rBtn.x : (panneauX + panneauW - 12.f - 20.f);
				(void)panneauX;
				// les sommets de TEXTE (commandes texturees) DANS LA BANDE DE LA RANGEE
				const nkgui::NkGuiDrawList &dl92 = ctx92.dl;
				for (uint32 c = 0; c < (uint32)dl92.cmds.Size(); ++c) {
					const nkgui::NkGuiDrawCmd &cm = dl92.cmds[c];
					if (cm.type != nkgui::NkGuiDrawCmdType::TexturedTriangles)
						continue;
					for (uint32 k = 0; k < cm.idxCount; ++k) {
						const uint32 ii = cm.idxOffset + k;
						if (ii >= (uint32)dl92.idx.Size())
							break;
						const uint32 vi = dl92.idx[ii];
						if (vi >= (uint32)dl92.vtx.Size())
							continue;
						const float32 x = dl92.vtx[vi].pos.x, y = dl92.vtx[vi].pos.y;
						if (rAide.h > 0.f && (y < rAide.y || y > rAide.y + rAide.h))
							continue; // une autre rangee : elle a ses propres colonnes
						if (x > maxTexte)
							maxTexte = x;
					}
				}
				// l'aide peinte s'arrete AVANT le bouton, et aucun glyphe de sa bande ne le touche
				aideX0 = rAide.x;
				aideX1 = rAide.x + rAide.w;
				geomOk = rAide.w > 0.f && aideX1 <= rBtn.x - 6.f + 0.01f && maxTexte > 0.f && maxTexte <= bordBouton;
			}
			snprintf(det, sizeof(det),
					 "la porte : texte plein %.1f px, tronque a %.1f px demande -> %.1f px (avec « … »), texte entier rendu quand la "
					 "place suffit -> %d ; l'inspecteur : le sommet de texte le plus a droite est a %.1f, la colonne du bouton commence "
					 "a %.1f (aide peinte %.1f -> %.1f) -> %d",
					 (double)wPleine, (double)(wPleine * 0.5f), (double)wCoupee, porteOk ? 1 : 0, (double)maxTexte,
					 (double)bordBouton, (double)aideX0, (double)aideX1, geomOk ? 1 : 0);
			check("92. ⑩ L'AIDE DE LA RANGEE « ARRONDI » NE DEBORDE PLUS : la porte de troncature coupe au caractere (jamais au milieu "
				  "d'un caractere UTF-8), pose « … » et tient la largeur demandee ; dans l'inspecteur reel, sur un trace a sommets, "
				  "AUCUN sommet de texte n'atteint la colonne du bouton d'expansion",
				  porteOk && geomOk, det);
		}
		// ── 93. ⑥ ALIGNER ET REPARTIR LA SELECTION (05/09). La question de Rodolf : « aligner un
		//    graphique par rapport a un ou plusieurs autres, plusieurs par rapport a un, par
		//    rapport a la page ». MESURE D'ABORD, ecrite dans le detail : la section ALIGNEMENT
		//    qui existait ecrit `layout.mainAlign` / `crossAlign` -- l'alignement des ENFANTS du
		//    noeud, jamais sa position. Le geste manquant est ici, avec ses quatre references.
		{
			char det[900];
			static DesignState stA;
			auto poser = [&](int32 parent, float32 x, float32 y, float32 w, float32 h, const char *nom) {
				const int32 i = stA.doc.AddChild(parent, "", NkAuthor::Humain);
				NkUINode &n = stA.doc.nodes[(uint32)i];
				n.shape = NkString("rect");
				n.label = NkString(nom);
				n.posX = x;
				n.posY = y;
				n.width.mode = NkSizeMode::Fixed;
				n.width.value = w;
				n.height.mode = NkSizeMode::Fixed;
				n.height.value = h;
				return i;
			};
			stA.doc.NewDocument("Toile", NkAuthor::Humain);
			const int32 pgA = stA.doc.AddChild(0, "", NkAuthor::Humain);
			stA.doc.nodes[(uint32)pgA].shape = NkString("frame");
			stA.doc.nodes[(uint32)pgA].label = NkString("Page");
			stA.doc.nodes[(uint32)pgA].layout.kind = NkLayoutKind::Free;
			stA.doc.nodes[(uint32)pgA].width.mode = NkSizeMode::Fixed;
			stA.doc.nodes[(uint32)pgA].width.value = 400.f;
			stA.doc.nodes[(uint32)pgA].height.mode = NkSizeMode::Fixed;
			stA.doc.nodes[(uint32)pgA].height.value = 300.f;
			const int32 a1 = poser(pgA, 10.f, 10.f, 40.f, 20.f, "A");
			const int32 a2 = poser(pgA, 100.f, 50.f, 60.f, 30.f, "B");
			const int32 a3 = poser(pgA, 200.f, 120.f, 20.f, 40.f, "C");
			// un noeud sous un parent qui AGENCE ses enfants : sa position est ignoree
			const int32 col = poser(pgA, 300.f, 10.f, 80.f, 100.f, "Colonne");
			stA.doc.nodes[(uint32)col].layout.kind = NkLayoutKind::Column;
			const int32 dedans = poser(col, 0.f, 0.f, 40.f, 20.f, "Dedans");
			stA.Recompute(NkPaintRect{0.f, 0.f, 1400.f, 900.f});
			// LA MESURE de l'existant : les six boutons d'ALIGNEMENT ecrivent l'agencement
			const NkAlign avantMain = stA.doc.nodes[(uint32)col].layout.mainAlign;
			const float32 posAvantEnfant = stA.doc.nodes[(uint32)dedans].posX;
			// 1. ALIGNER A GAUCHE sur la SELECTION (trois noeuds) : tous a x = 10
			stA.sel.Clear();
			stA.sel.Add(a1);
			stA.sel.Add(a2);
			stA.sel.Add(a3);
			stA.selected = a1;
			NkAlignResultat rG = NkAlignerSelection(stA, NkAlignGeste::Gauche, false);
			stA.Recompute(NkPaintRect{0.f, 0.f, 1400.f, 900.f});
			const NkPaintRect g1 = stA.layout.At(a1), g2 = stA.layout.At(a2), g3 = stA.layout.At(a3);
			const bool gaucheOk = rG.bouges == 2u && g1.x == g2.x && g2.x == g3.x;
			// 2. CENTRER VERTICALEMENT sur le DERNIER selectionne (C) : A et B prennent son milieu
			NkAlignResultat rM = NkAlignerSelection(stA, NkAlignGeste::Milieu, true);
			stA.Recompute(NkPaintRect{0.f, 0.f, 1400.f, 900.f});
			const NkPaintRect m1 = stA.layout.At(a1), m2 = stA.layout.At(a2), m3 = stA.layout.At(a3);
			auto milieu = [](const NkPaintRect &q) { return q.y + q.h * 0.5f; };
			const bool cleOk = rM.bouges == 2u && milieu(m1) > milieu(m3) - 0.01f && milieu(m1) < milieu(m3) + 0.01f
							   && milieu(m2) > milieu(m3) - 0.01f && milieu(m2) < milieu(m3) + 0.01f
							   && m3.y == g3.y; // la reference n'a pas bouge
			// 3. UN SEUL noeud : la reference est SA PAGE (centrer horizontalement)
			stA.sel.Clear();
			stA.sel.Add(a1);
			stA.selected = a1;
			NkAlignResultat rP = NkAlignerSelection(stA, NkAlignGeste::CentreH, false);
			stA.Recompute(NkPaintRect{0.f, 0.f, 1400.f, 900.f});
			const NkPaintRect p1 = stA.layout.At(a1), pPage = stA.layout.At(pgA);
			const bool pageOk = rP.bouges == 1u && (p1.x + p1.w * 0.5f) > (pPage.x + pPage.w * 0.5f) - 0.01f
								&& (p1.x + p1.w * 0.5f) < (pPage.x + pPage.w * 0.5f) + 0.01f
								&& strstr(rP.message, "page") != nullptr;
			// 4. REPARTIR : refuse a deux, agit a trois (centres a intervalle egal)
			stA.doc.nodes[(uint32)a1].posX = 0.f;
			stA.doc.nodes[(uint32)a1].posY = 0.f;
			stA.doc.nodes[(uint32)a2].posX = 20.f;
			stA.doc.nodes[(uint32)a2].posY = 0.f;
			stA.doc.nodes[(uint32)a3].posX = 300.f;
			stA.doc.nodes[(uint32)a3].posY = 0.f;
			stA.sel.Clear();
			stA.sel.Add(a1);
			stA.sel.Add(a2);
			stA.selected = a1;
			const NkAlignResultat rR2 = NkAlignerSelection(stA, NkAlignGeste::RepartirH, false);
			const bool refuseDeux = rR2.bouges == 0u && strstr(rR2.message, "TROIS") != nullptr;
			stA.sel.Add(a3);
			const NkAlignResultat rR3 = NkAlignerSelection(stA, NkAlignGeste::RepartirH, false);
			stA.Recompute(NkPaintRect{0.f, 0.f, 1400.f, 900.f});
			const NkPaintRect q1 = stA.layout.At(a1), q2 = stA.layout.At(a2), q3 = stA.layout.At(a3);
			auto cx = [](const NkPaintRect &q) { return q.x + q.w * 0.5f; };
			const float32 e1 = cx(q2) - cx(q1), e2 = cx(q3) - cx(q2);
			const bool repartirOk = rR3.bouges == 1u && e1 > e2 - 0.05f && e1 < e2 + 0.05f;
			// 5. CE QUI NE PEUT PAS BOUGER LE DIT : l'enfant d'une colonne
			// A tres a droite : le bord droit de la selection est LOIN de celui de « Dedans »,
			// sinon l'ecart serait nul et le noeud ne serait ni deplace ni refuse (rien a faire)
			stA.doc.nodes[(uint32)a1].posX = 350.f;
			stA.Recompute(NkPaintRect{0.f, 0.f, 1400.f, 900.f});
			stA.sel.Clear();
			stA.sel.Add(a1);
			stA.sel.Add(dedans);
			stA.selected = a1;
			const NkAlignResultat rD = NkAlignerSelection(stA, NkAlignGeste::Droite, false);
			const bool refusDit = rD.refuses == 1u && strstr(rD.message, "parent place ses enfants") != nullptr
								  && stA.doc.nodes[(uint32)dedans].posX == posAvantEnfant;
			// et la MESURE de l'existant, inchangee : ALIGNEMENT n'a pas touche aux positions
			const bool mesureExistant = stA.doc.nodes[(uint32)col].layout.mainAlign == avantMain;
			snprintf(det, sizeof(det),
					 "gauche sur la selection : %u bouges, x = %.0f / %.0f / %.0f -> %d ; milieu sur le DERNIER (C) : %u bouges, "
					 "milieux %.1f / %.1f / %.1f (C fixe=%d) -> %d ; un seul : « %s » -> %d ; repartir a deux : refuse (%s) -> %d ; a "
					 "trois : %u bouge, ecarts %.1f et %.1f -> %d ; enfant d'une colonne : %u refuse(s), « %s » -> %d",
					 rG.bouges, (double)g1.x, (double)g2.x, (double)g3.x, gaucheOk ? 1 : 0, rM.bouges, (double)milieu(m1),
					 (double)milieu(m2), (double)milieu(m3), (m3.y == g3.y) ? 1 : 0, cleOk ? 1 : 0, rP.message, pageOk ? 1 : 0,
					 rR2.message, refuseDeux ? 1 : 0, rR3.bouges, (double)e1, (double)e2, repartirOk ? 1 : 0, rD.refuses, rD.message,
					 refusDit ? 1 : 0);
			check("93. ⑥ ALIGNER ET REPARTIR LA SELECTION : la boite englobante quand deux noeuds ou plus sont choisis, LE DERNIER "
				  "SELECTIONNE quand on le demande (il ne bouge pas), LA PAGE quand un seul est choisi ; repartir exige trois elements "
				  "et pose les centres a intervalle egal ; un noeud dont le parent agence ses enfants est LAISSE et la phrase le dit",
				  gaucheOk && cleOk && pageOk && refuseDeux && repartirOk && refusDit && mesureExistant, det);
		}
		// ── 93b. ⑥ « BOUGE » VEUT DIRE QUE LA BOITE A BOUGE (inventaire du 07/09,
		//    mesure 2, famille 2) ────────────────────────────────────────────────
		//
		// 🔴 LE DEFAUT : `ParentPlaceLibrement` acceptait `Free` **et** `Anchor`, or
		//    la branche `Anchor` du solveur ne lit PAS `posX/posY` -- elle calcule la
		//    place depuis les bords ancres. Le geste ecrivait donc une position que
		//    rien ne relit, et COMPTAIT un deplacement qui n'avait pas lieu.
		//    Deux endroits enoncaient des regles contradictoires sur un meme fait :
		//    la section DISPOSITION traite la position comme CALCULEE sous un parent
		//    `Anchor` (elle affiche des boites statiques), ALIGNER LA SELECTION la
		//    traitait comme ECRITE.
		//
		// ⚠️ LE TEMOIN EST UNE RELATION, PAS UN COMPTE, et c'est deliberе : « tout
		//    noeud compte comme bouge a vu SA BOITE bouger ». Un essai qui verifierait
		//    `refuses == 1` serait a refaire le jour ou le solveur apprendrait a lire
		//    `posX/posY` sous ancrage ; celui-ci resterait vrai et passerait au vert
		//    tout seul, avec `bouges == 1`. C'est la regle d'arbitrage ecrite en essai :
		//    *quand deux endroits se contredisent, celui qui S'EXECUTE a raison.*
		{
			char det[520];
			static DesignState stAn;
			stAn.doc.NewDocument("Toile", NkAuthor::Humain);
			const int32 pg = stAn.doc.AddChild(0, "", NkAuthor::Humain);
			{
				NkUINode &p = stAn.doc.nodes[(uint32)pg];
				p.shape = NkString("frame");
				p.label = NkString("Page ancree");
				p.layout.kind = NkLayoutKind::Anchor; // LE parent qui place lui-meme
				p.width.mode = NkSizeMode::Fixed;
				p.width.value = 400.f;
				p.height.mode = NkSizeMode::Fixed;
				p.height.value = 300.f;
			}
			auto poserAn = [&](float32 x, float32 y, uint8 bords, const char *nom) {
				const int32 i = stAn.doc.AddChild(pg, "", NkAuthor::Humain);
				NkUINode &n = stAn.doc.nodes[(uint32)i];
				n.shape = NkString("rect");
				n.label = NkString(nom);
				n.posX = x;
				n.posY = y;
				n.anchorEdges = bords;
				n.width.mode = NkSizeMode::Fixed;
				n.width.value = 40.f;
				n.height.mode = NkSizeMode::Fixed;
				n.height.value = 20.f;
				return i;
			};
			// ⚠️ DEUX BORDS DIFFERENTS, ET C'EST LE MONTAGE QUI COMPTE. Premiere
			//    ecriture de cet essai : les deux noeuds ancres au MEME bord. Sous
			//    ancrage leurs boites sont alors identiques, l'ecart a aligner vaut
			//    zero, et `deplacer` rend la main avant tout -- ni bouge, ni refus.
			//    L'essai etait ROUGE pour la mauvaise raison (« rien a deplacer ») et
			//    n'exercait PAS le regime risque. *Une elimination ne vaut que si la
			//    sonde a exerce le regime qu'elle elimine.*
			const int32 u1 = poserAn(10.f, 10.f, nkanchor::Left | nkanchor::Top, "U1");
			const int32 u2 = poserAn(120.f, 60.f, nkanchor::Right | nkanchor::Top, "U2");
			stAn.Recompute(NkPaintRect{0.f, 0.f, 1400.f, 900.f});
			const NkPaintRect avant1 = stAn.layout.At(u1), avant2 = stAn.layout.At(u2);
			stAn.sel.Clear();
			stAn.sel.Add(u1);
			stAn.sel.Add(u2);
			stAn.selected = u1;
			const NkAlignResultat rA = NkAlignerSelection(stAn, NkAlignGeste::Droite, false);
			stAn.Recompute(NkPaintRect{0.f, 0.f, 1400.f, 900.f});
			const NkPaintRect apres1 = stAn.layout.At(u1), apres2 = stAn.layout.At(u2);
			const uint32 boitesBougees = (uint32)((apres1.x != avant1.x || apres1.y != avant1.y) ? 1 : 0)
										 + (uint32)((apres2.x != avant2.x || apres2.y != avant2.y) ? 1 : 0);
			// LA RELATION : autant de boites deplacees que de noeuds annonces bouges.
			const bool honnete = rA.bouges == boitesBougees;
			// ET LE REFUS SE DIT, quand il y en a un -- un geste sans effet qui se tait
			// est exactement ce qu'on repare.
			const bool refusDit93b = (rA.bouges > 0u)
									 || (rA.refuses > 0u
										 && strstr(rA.message, "parent place ses enfants") != nullptr);
			snprintf(det, sizeof(det),
					 "parent en ANCRAGE : %u annonce(s) bouge(s), %u boite(s) reellement "
					 "deplacee(s) (U1 x %.0f -> %.0f, U2 x %.0f -> %.0f), %u refuse(s) ; "
					 "message : « %s »",
					 rA.bouges, boitesBougees, (double)avant1.x, (double)apres1.x,
					 (double)avant2.x, (double)apres2.x, rA.refuses, rA.message);
			check("93b. ⑥ « DEPLACE » VEUT DIRE QUE LA BOITE A BOUGE : sous un parent en ANCRAGE, le solveur "
				  "ne lit pas `posX/posY` -- le geste ne doit donc pas compter un deplacement qui n'a pas "
				  "lieu. Le temoin est une RELATION (autant de boites deplacees que de noeuds annonces), pas "
				  "un compte de refus : il resterait juste le jour ou le solveur apprendrait l'ancrage",
				  honnete && refusDit93b, det);
		}
		// -- 93c. (7) LA PHRASE << ENREGISTRE, PAS APPLIQUE >> DIT LA VERITE DU
		//    SOLVEUR (inventaire du 07/09, mesure 2, famille 1) ------------------
		//
		// LE DEFAUT : la section ALIGNEMENT montrait huit boutons ACTIFS quel que
		//    soit l'agencement. Or `mainAlign`/`crossAlign` ne sont lus que dans la
		//    branche LIGNE/COLONNE du solveur : sous `Free`, `Anchor` et `Grid`, le
		//    reglage s'ecrivait, se persistait, et ne changeait rien. L'inspecteur le
		//    DIT desormais -- il ne grise pas, pour ne pas fermer la question de
		//    savoir si une grille devra un jour honorer l'alignement.
		//
		// CE CAS NE CROIT PAS LE PREDICAT SUR PAROLE, et c'est tout son objet :
		//    le comparer a une liste ecrite a la main serait comparer une copie a une
		//    copie. On CHANGE l'alignement et on regarde si une boite BOUGE. Le jour
		//    ou quelqu'un enseigne l'alignement a la grille sans toucher au predicat,
		//    cet essai rougit.
		{
			char det[600];
			struct Cas93c {
					NkLayoutKind kind;
					const char *nom;
			};
			static const Cas93c kK93[5] = {{NkLayoutKind::Row, "row"},
										   {NkLayoutKind::Column, "column"},
										   {NkLayoutKind::Grid, "grid"},
										   {NkLayoutKind::Anchor, "anchor"},
										   {NkLayoutKind::Free, "free"}};
			uint32 desaccords93 = 0u, bougeants93 = 0u;
			char pire93[420];
			pire93[0] = '\0';
			for (uint32 ki = 0; ki < 5u; ++ki) {
				NkUIDocument d93;
				d93.NewDocument("Toile", NkAuthor::Humain);
				d93.SetMetric("espacement", 0.f);
				d93.SetMetric("marge", 0.f);
				const int32 pg93 = d93.AddChild(0, "", NkAuthor::Humain);
				{
					NkUINode &p = d93.nodes[(uint32)pg93];
					p.shape = NkString("frame");
					p.layout.kind = kK93[ki].kind;
					p.width.mode = NkSizeMode::Fixed;
					p.width.value = 400.f;
					p.height.mode = NkSizeMode::Fixed;
					p.height.value = 300.f;
				}
				for (uint32 c = 0; c < 2u; ++c) {
					const int32 i = d93.AddChild(pg93, "", NkAuthor::Humain);
					NkUINode &q = d93.nodes[(uint32)i];
					q.shape = NkString("rect");
					q.width.mode = NkSizeMode::Fixed;
					q.width.value = 40.f;
					q.height.mode = NkSizeMode::Fixed;
					q.height.value = 20.f;
				}
				const NkPaintRect surf93 = {0.f, 0.f, 800.f, 600.f};
				// LES DEUX AXES A LA FOIS : n'en changer qu'un laisserait passer un
				// agencement qui ne lirait que l'autre.
				d93.nodes[(uint32)pg93].layout.mainAlign = NkAlign::Start;
				d93.nodes[(uint32)pg93].layout.crossAlign = NkAlign::Start;
				NkLayoutResult avant93;
				NkComputeLayout(d93, surf93, avant93);
				d93.nodes[(uint32)pg93].layout.mainAlign = NkAlign::End;
				d93.nodes[(uint32)pg93].layout.crossAlign = NkAlign::End;
				NkLayoutResult apres93;
				NkComputeLayout(d93, surf93, apres93);
				bool aBouge93 = false;
				for (uint32 i = 0; i < (uint32)d93.nodes.Size(); ++i) {
					if (!avant93.Has((int32)i) || !apres93.Has((int32)i))
						continue;
					const NkPaintRect a = avant93.At((int32)i), b = apres93.At((int32)i);
					if (a.x != b.x || a.y != b.y || a.w != b.w || a.h != b.h) {
						aBouge93 = true;
						break;
					}
				}
				if (aBouge93)
					++bougeants93;
				const bool annonce93 = NkAlignementLuParLeSolveur(kK93[ki].kind);
				if (aBouge93 != annonce93) {
					++desaccords93;
					if (!pire93[0])
						snprintf(pire93, sizeof(pire93),
								 "<< %s >> : le predicat annonce %s, le solveur %s",
								 kK93[ki].nom, annonce93 ? "LU" : "ignore",
								 aBouge93 ? "a bouge" : "n'a rien bouge");
				}
			}
			// CONTROLE POSITIF DE L'INSTRUMENT : si AUCUN des cinq ne bougeait, un
			// << 0 desaccord >> serait aussi le score d'un montage inerte -- un
			// alignement qui ne peut rien deplacer se compare a lui-meme.
			snprintf(det, sizeof(det),
					 "cinq agencements confrontes au SOLVEUR (alignement Start -> End sur les "
					 "deux axes) : %u desaccord(s) [%s] ; controle positif : %u agencement(s) "
					 "deplacent reellement une boite (attendu 2 : row et column)",
					 desaccords93, pire93[0] ? pire93 : "(aucun)", bougeants93);
			check("93c. (7) LA PHRASE << ENREGISTRE, PAS APPLIQUE >> DIT LA VERITE : pour chacun des cinq "
				  "agencements, `NkAlignementLuParLeSolveur` est confronte au COMPORTEMENT REEL du solveur "
				  "-- changer l'alignement doit deplacer une boite si et seulement si le predicat l'annonce ; "
				  "le controle positif exige que DEUX agencements bougent vraiment, sinon le zero desaccord "
				  "serait celui d'un montage inerte",
				  desaccords93 == 0u && bougeants93 == 2u, det);
		}
		// -- 127. (4) NOMMER UNE METRIQUE : le cycle, l'effet, et l'aller-retour
		//    (inventaire Q118, le second champ inexistant) ----------------------
		//
		// LE DEFAUT : `spacingName` / `padName` avaient une cle au fichier, un
		//    lecteur, et un CONSOMMATEUR (le solveur) -- mais aucune porte dans
		//    l'interface. La rangee ESPACEMENT affichait << rien de nomme >> et il
		//    n'y avait nulle part ou en nommer un.
		//
		// TROIS ESSAIS, parce que << brancher un champ >> veut dire trois choses et
		//    qu'aucune ne prouve les autres : le CYCLE rend-il des noms utiles, le
		//    SOLVEUR obeit-il au nom pose, et le nom SURVIT-il au fichier.
		{
			char det[520];
			NkUIDocument d118;
			d118.NewDocument("Toile", NkAuthor::Humain);
			// `NewDocument` pose deux metriques : << espacement >> et << marge >>.
			// On ne les recopie pas ici -- on lit la table, comme le cycle.
			const uint32 nMet = (uint32)d118.metrics.Size();
			const char *m0 = nMet > 0u ? d118.metrics[0].name.Data() : "";
			const char *m1 = nMet > 1u ? d118.metrics[1].name.Data() : "";

			// (a) LE CYCLE EST COMPLET ET REVERSIBLE. Sans le retour a << aucune >>,
			//     poser un nom serait irreversible depuis la rangee -- un geste qui
			//     ne se defait pas par le meme chemin n'est pas un reglage.
			// ON APPELLE LA VRAIE FONCTION, `NkMetriqueSuivante` -- celle que le
			// dessin appelle. Premiere ecriture de cet essai : une copie du cycle
			// ECRITE ICI, parce que l'originale etait enfouie dans l'inspecteur.
			// Elle restait VERTE sous la mutation << le cycle ne fait rien >> : elle
			// mesurait sa propre copie. La fonction est descendue dans `Document.h`,
			// a cote de la table qu'elle parcourt, pour que les deux la partagent.
			auto suivante = [&](const char *courant) -> const char * {
				return NkMetriqueSuivante(d118, courant);
			};
			const char *c1 = suivante("");
			const char *c2 = suivante(c1);
			const char *c3 = suivante(c2);
			const char *cInconnu = suivante("metrique_disparue");
			const bool cycleOk = nMet >= 2u && NkComponentDecl::StrEq(c1, m0)
								 && NkComponentDecl::StrEq(c2, m1) && (c3 == nullptr || !*c3)
								 && NkComponentDecl::StrEq(cInconnu, m0);

			// (b) LE SOLVEUR OBEIT AU NOM POSE. Deux enfants dans une LIGNE : sans
			//     nom la gouttiere vaut zero, avec le nom elle vaut la metrique.
			d118.SetMetric(m0, 20.f);
			d118.SetMetric(m1, 0.f); // la marge ne doit pas brouiller la mesure
			const int32 pg118 = d118.AddChild(0, "", NkAuthor::Humain);
			{
				NkUINode &p = d118.nodes[(uint32)pg118];
				p.shape = NkString("frame");
				p.layout.kind = NkLayoutKind::Row;
				// LE << SANS NOM >> SE POSE, IL NE SE SUPPOSE PAS. Premiere ecriture de
				// cet essai : on croyait un noeud neuf sans nom de metrique. `InitNode`
				// nomme les DEUX par defaut (<< espacement >>, << marge >>) -- la mesure
				// rendait donc << 20 sans nom, 20 avec >> et l'essai accusait un correctif
				// juste. Un banc qui suppose l'etat qu'il mesure accuse le code d'un
				// defaut qui est chez lui -- deuxieme fois en deux jours.
				p.spacingName = NkString("");
				p.padName = NkString(m1);
				p.width.mode = NkSizeMode::Fixed;
				p.width.value = 400.f;
				p.height.mode = NkSizeMode::Fixed;
				p.height.value = 100.f;
			}
			int32 e118[2];
			for (uint32 c = 0; c < 2u; ++c) {
				e118[c] = d118.AddChild(pg118, "", NkAuthor::Humain);
				NkUINode &q = d118.nodes[(uint32)e118[c]];
				q.shape = NkString("rect");
				q.width.mode = NkSizeMode::Fixed;
				q.width.value = 40.f;
				q.height.mode = NkSizeMode::Fixed;
				q.height.value = 20.f;
			}
			const NkPaintRect surf118 = {0.f, 0.f, 800.f, 600.f};
			NkLayoutResult sans118;
			NkComputeLayout(d118, surf118, sans118);
			const float32 ecartSans = sans118.At(e118[1]).x - (sans118.At(e118[0]).x + 40.f);
			d118.nodes[(uint32)pg118].spacingName = NkString(m0); // LE NOM, pose
			NkLayoutResult avec118;
			NkComputeLayout(d118, surf118, avec118);
			const float32 ecartAvec = avec118.At(e118[1]).x - (avec118.At(e118[0]).x + 40.f);
			const bool solveurObeit = ecartSans == 0.f && ecartAvec == 20.f;

			// (c) LE NOM SURVIT AU FICHIER -- sans quoi le geste serait perdu au
			//     premier enregistrement, comme l'arrondi par coin l'etait.
			NkString s118;
			d118.Save(s118);
			NkUIDocument relu118;
			const bool lu118 = relu118.Load(s118.Data());
			const bool nomRelu = lu118 && relu118.IsValidIndex(pg118)
								 && NkComponentDecl::StrEq(
										relu118.nodes[(uint32)pg118].spacingName.Data(), m0);
			NkString s118b;
			if (lu118)
				relu118.Save(s118b);
			const bool stable118 = lu118 && NkComponentDecl::StrEq(s118.Data(), s118b.Data());

			snprintf(det, sizeof(det),
					 "(a) cycle sur %u metrique(s) : \"\" -> << %s >> -> << %s >> -> \"%s\", "
					 "un nom inconnu repart sur << %s >> -> %d ; (b) ecart entre deux enfants : "
					 "%.0f sans nom, %.0f avec (metrique = 20) -> %d ; (c) nom relu du fichier "
					 "= %d, reenregistrement identique = %d",
					 nMet, c1, c2, c3, cInconnu, cycleOk ? 1 : 0, (double)ecartSans,
					 (double)ecartAvec, solveurObeit ? 1 : 0, nomRelu ? 1 : 0, stable118 ? 1 : 0);
			check("127. (4) NOMMER UNE METRIQUE, LES TROIS MOITIES : le CYCLE est complet et reversible "
				  "(<< aucune >> en fait partie, sinon poser un nom serait irreversible), le SOLVEUR obeit "
				  "au nom pose (la gouttiere passe de 0 a 20), et le nom SURVIT au fichier -- brancher un "
				  "champ veut dire ces trois choses, et aucune ne prouve les deux autres",
				  cycleOk && solveurObeit && nomRelu && stable118, det);
		}
		// -- 128. (5) L'OPACITE DU NŒUD : le champ, la cle, le peintre --------
		//
		// LE SEUL VRAI CHANTIER DE MODELE des huit sections (inventaire Q118) : la
		//    rangee CALQUE affichait << 100 >> EN DUR, grisee, avec sa raison --
		//    << le modele ne la porte pas encore >>.
		//
		// TROIS MOITIES, et aucune ne prouve les autres : le CHAMP existe, la CLE
		//    fait l'aller-retour, et le PEINTRE l'honore -- y compris SUR UN
		//    DESCENDANT, ce qui est la seule facon de verifier que le facteur
		//    descend par la recursion au lieu d'etre lu sur chaque noeud.
		{
			char det[560];
			auto alphaDuFond = [](NkRecordingPaint &rec, uint32 rgbSansAlpha) -> int32 {
				for (uint32 i = 0; i < (uint32)rec.cmds.Size(); ++i) {
					const NkPaintCmd &c = rec.cmds[i];
					if (c.op != NkPaintOp::FillColor)
						continue;
					if ((c.rgba >> 8) != rgbSansAlpha)
						continue;
					return (int32)(c.rgba & 0xFFu);
				}
				return -1;
			};
			NkUIDocument d128;
			d128.NewDocument("Toile", NkAuthor::Humain);
			d128.SetMetric("espacement", 0.f);
			d128.SetMetric("marge", 0.f);
			d128.nodes[0].layout.kind = NkLayoutKind::Free;
			const int32 grp = d128.AddChild(0, "", NkAuthor::Humain);
			{
				NkUINode &g = d128.nodes[(uint32)grp];
				g.shape = NkString("frame");
				g.layout.kind = NkLayoutKind::Free;
				g.width.mode = NkSizeMode::Fixed;
				g.width.value = 300.f;
				g.height.mode = NkSizeMode::Fixed;
				g.height.value = 200.f;
			}
			const int32 enf = d128.AddChild(grp, "", NkAuthor::Humain);
			{
				NkUINode &q = d128.nodes[(uint32)enf];
				q.shape = NkString("rect");
				NkRemplissage f;
				f.couleur = NkString("#ff0000"); // opacite de remplissage : 100
				q.fills.PushBack(f);
				q.width.mode = NkSizeMode::Fixed;
				q.width.value = 40.f;
				q.height.mode = NkSizeMode::Fixed;
				q.height.value = 20.f;
			}
			const NkPaintRect surf128 = {0.f, 0.f, 800.f, 600.f};

			// (a) LE DEFAUT NE CHANGE RIEN. Un document a 100 % doit peindre
			//     exactement ce qu'il peignait -- sinon le champ neuf serait une
			//     regression deguisee en fonctionnalite.
			NkRecordingPaint rA128;
			RenderDocument(rA128, d128, surf128);
			const int32 aPlein = alphaDuFond(rA128, 0xff0000u);

			// (b) L'OPACITE DU NŒUD LUI-MEME.
			d128.nodes[(uint32)enf].opacite = 50.f;
			NkRecordingPaint rB128;
			RenderDocument(rB128, d128, surf128);
			const int32 aDemi = alphaDuFond(rB128, 0xff0000u);

			// (c) CELLE DU PARENT DESCEND SUR L'ENFANT. C'est l'essai qui compte :
			//     il echoue si le facteur est lu sur chaque noeud au lieu de
			//     descendre par la recursion.
			d128.nodes[(uint32)enf].opacite = 100.f;
			d128.nodes[(uint32)grp].opacite = 50.f;
			NkRecordingPaint rC128;
			RenderDocument(rC128, d128, surf128);
			const int32 aHerite = alphaDuFond(rC128, 0xff0000u);

			// (d) LES DEUX SE MULTIPLIENT (50 % dans 50 % = 25 %).
			d128.nodes[(uint32)enf].opacite = 50.f;
			NkRecordingPaint rD128;
			RenderDocument(rD128, d128, surf128);
			const int32 aQuart = alphaDuFond(rD128, 0xff0000u);

			// (e) LA CLE FAIT L'ALLER-RETOUR, et elle est ADDITIVE : rien au
			//     fichier tant que l'opacite vaut 100.
			NkUIDocument d128b;
			d128b.NewDocument("Toile", NkAuthor::Humain);
			const int32 s1 = d128b.AddChild(0, "", NkAuthor::Humain);
			d128b.nodes[(uint32)s1].shape = NkString("rect");
			NkString avantCle;
			d128b.Save(avantCle);
			const bool rienParDefaut128 = strstr(avantCle.Data(), "opacite") == nullptr;
			d128b.nodes[(uint32)s1].opacite = 40.f;
			NkString avecCle;
			d128b.Save(avecCle);
			NkUIDocument relu128;
			const bool lu128 = relu128.Load(avecCle.Data());
			const bool valeurRelue = lu128 && relu128.IsValidIndex(s1)
									 && relu128.nodes[(uint32)s1].opacite == 40.f;
			NkString reecrit128;
			if (lu128)
				relu128.Save(reecrit128);
			const bool stable128 = lu128
								   && NkComponentDecl::StrEq(avecCle.Data(), reecrit128.Data());

			const bool ok128 = aPlein == 255 && aDemi == 128 && aHerite == 128 && aQuart == 64
							   && rienParDefaut128 && valeurRelue && stable128;
			snprintf(det, sizeof(det),
					 "alpha du remplissage rouge : %d a 100%%, %d avec le NŒUD a 50%%, %d avec "
					 "le PARENT a 50%% (l'enfant a 100), %d avec les deux a 50%% ; cle : rien "
					 "au fichier par defaut=%d, 40 relu=%d, reenregistrement identique=%d",
					 aPlein, aDemi, aHerite, aQuart, rienParDefaut128 ? 1 : 0,
					 valeurRelue ? 1 : 0, stable128 ? 1 : 0);
			check("128. (5) L'OPACITE DU NŒUD, LES TROIS MOITIES : le CHAMP existe, la CLE fait "
				  "l'aller-retour et reste ADDITIVE (rien au fichier a 100%), et le PEINTRE l'honore -- "
				  "y compris quand elle vient d'un ANCETRE, ce qui est la seule facon de verifier que le "
				  "facteur descend par la recursion ; et les deux se multiplient (50% dans 50% = 25%)",
				  ok128, det);
		}
		// -- 129. (6) LA FUSION DU NŒUD, VERSION PARTIELLE : par COMMANDE ------
		//
		// Chiffree AVANT d'etre ecrite (Q121) : zero memoire, zero passe, et elle
		//    reutilise `NkGardeFusion` -- celle des remplissages, dont le cas 67
		//    prouve deja les cinq modes exacts. On n'en a pas ecrit une seconde.
		//
		// CE QUE CE CAS EXIGE, ET IL FAUT LES QUATRE :
		//    (a) un mode EXACT encadre le dessin du nœud d'un PushBlend/PopBlend ;
		//    (b) un mode NON exact n'en pousse AUCUN -- il est enregistre et dit,
		//        jamais approxime (<< un repli qui reste plausible est pire qu'un
		//        refus >>) ;
		//    (c) la garde couvre AUSSI la descendance : le dessin d'un enfant tombe
		//        ENTRE le push et le pop du parent. C'est ce qui distingue un mode
		//        de calque d'un mode par remplissage ;
		//    (d) les push et les pop se COMPTENT : une sortie anticipee qui
		//        laisserait un mode derriere elle teindrait tout le reste du
		//        document.
		{
			char det[560];
			auto scene129 = [](const char *modeParent, NkRecordingPaint &rec) {
				NkUIDocument d;
				d.NewDocument("Toile", NkAuthor::Humain);
				d.SetMetric("espacement", 0.f);
				d.SetMetric("marge", 0.f);
				d.nodes[0].layout.kind = NkLayoutKind::Free;
				const int32 grp = d.AddChild(0, "", NkAuthor::Humain);
				{
					NkUINode &g = d.nodes[(uint32)grp];
					g.shape = NkString("rect");
					g.fusion = NkString(modeParent);
					NkRemplissage f;
					f.couleur = NkString("#808080");
					g.fills.PushBack(f);
					g.layout.kind = NkLayoutKind::Free;
					g.width.mode = NkSizeMode::Fixed;
					g.width.value = 200.f;
					g.height.mode = NkSizeMode::Fixed;
					g.height.value = 150.f;
				}
				const int32 enf = d.AddChild(grp, "", NkAuthor::Humain);
				{
					NkUINode &q = d.nodes[(uint32)enf];
					q.shape = NkString("rect");
					NkRemplissage f;
					f.couleur = NkString("#00ff00"); // LE VERT : le marqueur de l'enfant
					q.fills.PushBack(f);
					q.width.mode = NkSizeMode::Fixed;
					q.width.value = 40.f;
					q.height.mode = NkSizeMode::Fixed;
					q.height.value = 20.f;
				}
				const NkPaintRect surf = {0.f, 0.f, 800.f, 600.f};
				RenderDocument(rec, d, surf);
			};
			// L'INDEX de la premiere commande qui peint le vert de l'enfant.
			auto indexDuVert = [](NkRecordingPaint &rec) -> int32 {
				for (uint32 i = 0; i < (uint32)rec.cmds.Size(); ++i) {
					const NkPaintCmd &c = rec.cmds[i];
					if (c.op == NkPaintOp::FillColor && (c.rgba >> 8) == 0x00ff00u)
						return (int32)i;
				}
				return -1;
			};
			auto comptePush = [](NkRecordingPaint &rec, uint16 &modePousse) -> int32 {
				int32 n = 0;
				modePousse = 0u;
				for (uint32 i = 0; i < (uint32)rec.cmds.Size(); ++i)
					if (rec.cmds[i].op == NkPaintOp::PushBlend) {
						if (n == 0)
							// LE MODE VIT DANS `icon`, PAS DANS `role` -- c'est la convention de
							// `NkRecordingPaint::PushBlend`, et c'est celle que le cas 67 lit
							// deja. Premiere ecriture : `role`, qui rendait 0 pour tous les
							// modes -- l'essai etait rouge en accusant un peintre juste.
							modePousse = rec.cmds[i].icon;
						++n;
					}
				return n;
			};
			auto comptePop = [](NkRecordingPaint &rec) -> int32 {
				int32 n = 0;
				for (uint32 i = 0; i < (uint32)rec.cmds.Size(); ++i)
					if (rec.cmds[i].op == NkPaintOp::PopBlend)
						++n;
				return n;
			};
			// (a) un mode EXACT : multiply
			NkRecordingPaint rMul;
			scene129("multiply", rMul);
			uint16 modeMul = 0u;
			const int32 pushMul = comptePush(rMul, modeMul);
			const int32 popMul = comptePop(rMul);
			const bool exactPousse =
				pushMul == 1 && popMul == 1
				&& modeMul == (uint16)NkComponentPaint::NkPaintBlend::Multiply;
			// (c) l'enfant peint ENTRE le push et le pop du parent
			int32 iPush = -1, iPop = -1;
			for (uint32 i = 0; i < (uint32)rMul.cmds.Size(); ++i) {
				if (rMul.cmds[i].op == NkPaintOp::PushBlend && iPush < 0)
					iPush = (int32)i;
				if (rMul.cmds[i].op == NkPaintOp::PopBlend)
					iPop = (int32)i;
			}
			const int32 iVert = indexDuVert(rMul);
			const bool enfantDedans = iPush >= 0 && iPop > iPush && iVert > iPush && iVert < iPop;
			// (b) un mode NON exact : overlay -- rien ne doit etre pousse
			NkRecordingPaint rOv;
			scene129("overlay", rOv);
			uint16 modeOv = 0u;
			const int32 pushOv = comptePush(rOv, modeOv);
			// LE CONTROLE NEGATIF DU MONTAGE : sans mode, rien non plus. Sans lui,
			// << 0 push >> serait aussi le score d'une scene qui ne peint rien.
			NkRecordingPaint rNul;
			scene129("", rNul);
			uint16 modeNul = 0u;
			const int32 pushNul = comptePush(rNul, modeNul);
			const bool vertPeintPartout = indexDuVert(rOv) >= 0 && indexDuVert(rNul) >= 0;
			const bool ok129 = exactPousse && enfantDedans && pushOv == 0 && pushNul == 0
							   && vertPeintPartout;
			// (e) la cle : additive, aller-retour, mode inconnu PRESERVE
			NkUIDocument dK;
			dK.NewDocument("Toile", NkAuthor::Humain);
			const int32 sK = dK.AddChild(0, "", NkAuthor::Humain);
			dK.nodes[(uint32)sK].shape = NkString("rect");
			NkString sansCle;
			dK.Save(sansCle);
			const bool rienParDefaut129 = strstr(sansCle.Data(), "  fusion =") == nullptr;
			dK.nodes[(uint32)sK].fusion = NkString("mode-de-demain"); // inconnu : preserve
			NkString avecCle;
			dK.Save(avecCle);
			NkUIDocument reluK;
			const bool luK = reluK.Load(avecCle.Data());
			const bool inconnuGarde =
				luK && reluK.IsValidIndex(sK)
				&& NkComponentDecl::StrEq(reluK.nodes[(uint32)sK].fusion.Data(), "mode-de-demain");
			NkString reecritK;
			if (luK)
				reluK.Save(reecritK);
			const bool stableK = luK && NkComponentDecl::StrEq(avecCle.Data(), reecritK.Data());
			snprintf(det, sizeof(det),
					 "<< multiply >> : %d push / %d pop, mode=%u (Multiply=%u), l'enfant peint "
					 "a l'index %d entre %d et %d -> %d ; << overlay >> : %d push (attendu 0) ; "
					 "sans mode : %d push ; le vert est peint dans les trois=%d ; cle : rien par "
					 "defaut=%d, mode INCONNU preserve=%d, reenregistrement identique=%d",
					 pushMul, popMul, (unsigned)modeMul,
					 (unsigned)NkComponentPaint::NkPaintBlend::Multiply, iVert, iPush, iPop,
					 enfantDedans ? 1 : 0, pushOv, pushNul, vertPeintPartout ? 1 : 0,
					 rienParDefaut129 ? 1 : 0, inconnuGarde ? 1 : 0, stableK ? 1 : 0);
			check("129. (6) LA FUSION DU NŒUD, VERSION PARTIELLE : un mode EXACT encadre le nœud d'un "
				  "PushBlend/PopBlend appaires, la garde couvre AUSSI la descendance (le dessin de "
				  "l'enfant tombe ENTRE les deux -- c'est ce qui distingue un mode de calque d'un mode "
				  "par remplissage), et un mode NON exact ne pousse RIEN : il est enregistre et dit, "
				  "jamais approxime. La cle est additive et preserve un mode inconnu",
				  ok129 && rienParDefaut129 && inconnuGarde && stableK, det);
		}
		// -- 130. (7) L'ANCRAGE A DES MARGES : `posX`/`posY` se COMPOSENT avec les
		//    bords au lieu d'etre ignores (S11, tranche par Rodolf) --------------
		//
		// CE QUI MANQUAIT : la branche `Anchor` du solveur calculait la position
		//    UNIQUEMENT depuis les bords ancres. Il n'y avait aucun endroit ou
		//    ranger << et en plus, decale de trente pixels >>. Nomme correctement,
		//    ce decalage est une MARGE -- un concept standard qui manquait.
		//
		// CE QU'ON N'A PAS AJOUTE, ET C'EST LA MOITIE DU LOT : aucun champ neuf.
		//    `posX`/`posY` existent deja sur le nœud, sont deja serialises (cle
		//    `position`), deja ecrits par le glisser a la souris ET par le geste
		//    d'alignement, et deja lus par la branche `Free`. Sous `Anchor` ils
		//    n'etaient LUS par personne. Le logement existait ; il etait ignore
		//    d'un cote.
		//
		// QUATRE EXIGENCES, et la troisieme est celle qui prouve la COMPOSITION :
		{
			char det[620];
			// Le montage : un parent ANCRE, un enfant ancre a GAUCHE+HAUT avec une
			// marge, et un second ancre a DROITE pour que l'alignement ait un ecart
			// non nul a resorber (lecon du 06/09 : un montage qui n'exerce pas le
			// regime risque rend un rouge trompeur).
			auto scene130 = [](float32 largeurParent, float32 mx, float32 my,
							   NkUIDocument &d, int32 &pg, int32 &u1, int32 &u2) {
				d.NewDocument("Toile", NkAuthor::Humain);
				d.SetMetric("espacement", 0.f);
				d.SetMetric("marge", 0.f);
				pg = d.AddChild(0, "", NkAuthor::Humain);
				NkUINode &p = d.nodes[(uint32)pg];
				p.shape = NkString("frame");
				p.layout.kind = NkLayoutKind::Anchor;
				p.width.mode = NkSizeMode::Fixed;
				p.width.value = largeurParent;
				p.height.mode = NkSizeMode::Fixed;
				p.height.value = 300.f;
				auto poser = [&](uint8 bords, float32 px, float32 py) {
					const int32 i = d.AddChild(pg, "", NkAuthor::Humain);
					NkUINode &q = d.nodes[(uint32)i];
					q.shape = NkString("rect");
					q.anchorEdges = bords;
					q.posX = px;
					q.posY = py;
					q.width.mode = NkSizeMode::Fixed;
					q.width.value = 40.f;
					q.height.mode = NkSizeMode::Fixed;
					q.height.value = 20.f;
					return i;
				};
				u1 = poser(nkanchor::Left | nkanchor::Top, mx, my);
				u2 = poser(nkanchor::Right | nkanchor::Top, 0.f, 0.f);
			};
			const NkPaintRect surf130 = {0.f, 0.f, 900.f, 600.f};

			// (a) LA MARGE SE COMPOSE AVEC LE BORD. Ancre a gauche + marge 30 :
			//     la boite doit etre a `bord + 30`, pas au bord.
			NkUIDocument dA;
			int32 pgA = 0, a1 = 0, a2 = 0;
			scene130(400.f, 30.f, 12.f, dA, pgA, a1, a2);
			NkLayoutResult layA;
			NkComputeLayout(dA, surf130, layA);
			const NkPaintRect bA = layA.At(a1), bParent = layA.At(pgA);
			const bool margeAppliquee = bA.x == bParent.x + 30.f && bA.y == bParent.y + 12.f;

			// (b) LE DEFAUT EST ZERO -- la garantie de non-regression. Sans marge,
			//     la boite doit tomber EXACTEMENT sur le bord, comme avant.
			NkUIDocument dZ;
			int32 pgZ = 0, z1 = 0, z2 = 0;
			scene130(400.f, 0.f, 0.f, dZ, pgZ, z1, z2);
			NkLayoutResult layZ;
			NkComputeLayout(dZ, surf130, layZ);
			const bool defautZero = layZ.At(z1).x == layZ.At(pgZ).x
									&& layZ.At(z1).y == layZ.At(pgZ).y;

			// (c) LE CONTROLE POSITIF : LE PARENT GRANDIT, LE NŒUD SUIT SON BORD ET
			//     GARDE SA MARGE. C'est CE cas qui prouve que la marge se COMPOSE
			//     avec l'ancrage au lieu de le remplacer -- une position absolue
			//     passerait (a) et echouerait ici.
			NkUIDocument dL;
			int32 pgL = 0, l1 = 0, l2 = 0;
			scene130(700.f, 30.f, 12.f, dL, pgL, l1, l2); // parent 400 -> 700
			NkLayoutResult layL;
			NkComputeLayout(dL, surf130, layL);
			// l'ancre GAUCHE ne bouge pas quand le parent s'elargit ; c'est l'ancre
			// DROITE qui doit suivre le bord, marge comprise.
			const float32 droiteAvant = layA.At(a2).x, droiteApres = layL.At(l2).x;
			const bool suitLeBord = droiteApres == droiteAvant + 300.f;
			const bool gardeSaMarge = layL.At(l1).x == layL.At(pgL).x + 30.f
									  && layL.At(l1).y == layL.At(pgL).y + 12.f;

			// (d) ALIGNER PUIS ENREGISTRER PUIS RECHARGER : la position revient.
			//     ⚠️ LES DEUX MOITIES, comme l'arrondi par coin l'a appris : la
			//        STABILITE (le fichier se reecrit pareil) ET la CONSERVATION
			//        (la boite est encore la ou l'alignement l'a mise).
			static DesignState stM;
			int32 pgM = 0, m1 = 0, m2 = 0;
			scene130(400.f, 30.f, 12.f, stM.doc, pgM, m1, m2);
			stM.Recompute(surf130);
			stM.sel.Clear();
			stM.sel.Add(m1);
			stM.sel.Add(m2);
			stM.selected = m1;
			const NkAlignResultat rM = NkAlignerSelection(stM, NkAlignGeste::Droite, false);
			stM.Recompute(surf130);
			const NkPaintRect apresAlign = stM.layout.At(m1);
			NkString sM;
			stM.doc.Save(sM);
			NkUIDocument reluM;
			const bool luM = reluM.Load(sM.Data());
			NkLayoutResult layM;
			if (luM)
				NkComputeLayout(reluM, surf130, layM);
			const bool retrouve = luM && layM.Has(m1) && layM.At(m1).x == apresAlign.x
								  && layM.At(m1).y == apresAlign.y;
			NkString sM2;
			if (luM)
				reluM.Save(sM2);
			const bool stableM = luM && NkComponentDecl::StrEq(sM.Data(), sM2.Data());
			const bool aBougeVraiment = rM.bouges == 1u;

			// (e) L'AXE ETIRE N'A PLUS DE LIBERTE, et c'est une MESURE, pas une
			//     phrase : un nœud ancre a GAUCHE ET A DROITE est entierement place
			//     par ses deux bords. Lui ajouter un decalage le ferait deborder de
			//     celui qu'il touche -- on n'applique donc rien sur cet axe. Sans cet
			//     essai, la limite serait une affirmation du commentaire.
			NkUIDocument dE;
			int32 pgE = 0, e1 = 0, e2 = 0;
			scene130(400.f, 0.f, 0.f, dE, pgE, e1, e2);
			{
				NkUINode &q = dE.nodes[(uint32)e1];
				q.anchorEdges = nkanchor::Left | nkanchor::Right | nkanchor::Top;
				q.posX = 30.f; // ignore : l'axe X est etire
				q.posY = 12.f; // applique : l'axe Y ne l'est pas
			}
			NkLayoutResult layE;
			NkComputeLayout(dE, surf130, layE);
			const bool etireIgnoreX = layE.At(e1).x == layE.At(pgE).x;
			const bool libreGardeY = layE.At(e1).y == layE.At(pgE).y + 12.f;

			snprintf(det, sizeof(det),
					 "(a) marge 30/12 sur un bord gauche+haut : boite a (%.0f, %.0f), bord du "
					 "parent (%.0f, %.0f) -> %d ; (b) sans marge, la boite tombe sur le bord -> "
					 "%d ; (c) parent 400 -> 700 : l'ancre DROITE passe de %.0f a %.0f (+300 "
					 "attendu) -> %d, et l'ancre gauche garde sa marge -> %d ; (d) aligner : %u "
					 "bouge(s), la boite est a %.0f, relue du fichier a %.0f -> retrouve=%d, "
					 "aller-retour stable=%d ; (e) axe ETIRE (gauche+droite) : X ignore=%d, "
					 "Y libre garde sa marge=%d",
					 (double)bA.x, (double)bA.y, (double)bParent.x, (double)bParent.y,
					 margeAppliquee ? 1 : 0, defautZero ? 1 : 0, (double)droiteAvant,
					 (double)droiteApres, suitLeBord ? 1 : 0, gardeSaMarge ? 1 : 0, rM.bouges,
					 (double)apresAlign.x, luM && layM.Has(m1) ? (double)layM.At(m1).x : -1.0,
					 retrouve ? 1 : 0, stableM ? 1 : 0, etireIgnoreX ? 1 : 0,
					 libreGardeY ? 1 : 0);
			check("130. (7) L'ANCRAGE A DES MARGES : `posX`/`posY` se COMPOSENT avec les bords ancres au lieu "
				  "d'etre ignores -- la marge s'applique, le DEFAUT EST ZERO (donc rien ne bouge dans les "
				  "documents existants), le nœud SUIT SON BORD quand le parent grandit TOUT EN gardant sa "
				  "marge (c'est ce cas qui prouve la composition : une position absolue passerait le "
				  "premier essai et echouerait celui-ci), et aligner puis enregistrer puis recharger "
				  "retrouve la position -- stabilite ET conservation",
				  margeAppliquee && defautZero && suitLeBord && gardeSaMarge && aBougeVraiment
					  && retrouve && stableM && etireIgnoreX && libreGardeY,
				  det);
		}
		// -- 131. (7) LE DOCUMENT REEL DE RODOLF NE PEUT PAS BOUGER --------------
		//
		// Le risque nomme dans S11 : le solveur d'ancrage positionne tout ce qui est
		//    ancre dans ses documents. Plutot qu'un compte de boites fige -- qui
		//    tomberait des qu'il edite son document -- l'essai porte la RELATION qui
		//    rend le risque nul : aucun nœud de ce document n'a un parent en
		//    ancrage, donc la branche modifiee n'est jamais entree.
		// ⚠️ LA SOMME DES BOITES EST DANS LE DETAIL, PAS DANS L'ASSERTION : c'est
		//    une mesure a comparer entre AVANT et APRES le changement, pas un
		//    nombre a maintenir. Un compte fige sur une collection qui grandit par
		//    conception est une dette a echeance.
		{
			char det[420];
			NkUIDocument dR;
			NkString texte = NkFile::ReadAllText("nkuidesign_document.nkuidoc");
			const bool charge = !texte.Empty() && dR.Load(texte.Data());
			uint32 sousAncrage = 0u, total = 0u;
			double somme = 0.0;
			if (charge) {
				total = (uint32)dR.nodes.Size();
				for (uint32 i = 0; i < total; ++i) {
					const int32 p = dR.nodes[i].parent;
					if (dR.IsValidIndex(p) && dR.nodes[(uint32)p].layout.kind == NkLayoutKind::Anchor)
						++sousAncrage;
				}
				NkLayoutResult layR;
				NkComputeLayout(dR, NkPaintRect{0.f, 0.f, 1400.f, 900.f}, layR);
				for (uint32 i = 0; i < total; ++i)
					if (layR.Has((int32)i)) {
						const NkPaintRect b = layR.At((int32)i);
						somme += (double)b.x + (double)b.y * 3.0 + (double)b.w * 7.0
								 + (double)b.h * 11.0;
					}
			}
			snprintf(det, sizeof(det),
					 "document charge=%d, %u nœuds, %u sous un parent en ANCRAGE (0 = la branche "
					 "modifiee n'est jamais entree) ; empreinte des boites = %.3f (a comparer "
					 "avant / apres le changement, pas a maintenir)",
					 charge ? 1 : 0, total, sousAncrage, somme);
			check("131. (7) LE DOCUMENT REEL NE PEUT PAS BOUGER : aucun de ses nœuds n'a un parent en "
				  "ancrage, donc la branche modifiee n'est jamais entree -- une RELATION, pas un compte de "
				  "boites fige qui tomberait des que Rodolf edite son document",
				  charge && sousAncrage == 0u, det);
		}
		// -- 132. (1) UNE SECTION QUI NE PEUT PAS S'APPLIQUER DISPARAIT --------
		//
		// Rodolf : << un graphique qui n'est pas un texte n'a pas besoin de laisser
		//    visible sa partie typographie. >>
		//
		// LA REGLE MESUREE ICI : on CACHE ce qui ne peut pas exister, on EXPLIQUE
		//    ce qui n'agit pas encore. TYPOGRAPHIE est la seule section dont une
		//    condition de TYPE refuse tout le corps (`shape != "text"`) ; les
		//    autres refusent par ETAT et gardent leur phrase.
		//
		// ⚠️ LE CONTROLE POSITIF EST DANS L'ESSAI, et il est la moitie qui compte :
		//    une liste attendue AMPUTEE d'une section doit etre detectee comme
		//    differente. Sans lui, << 0 ecart >> serait aussi le score d'une
		//    comparaison qui ne compare rien -- l'inertie qu'on vient de payer
		//    ailleurs.
		{
			char det[620];
			struct Attendu132 {
					const char *forme;   ///< la cle `shape` du nœud
					bool typographie;    ///< la section doit-elle etre la ?
			};
			static const Attendu132 kT132[6] = {
				{"rect", false},   {"ellipse", false}, {"etoile", false},
				{"image", false},  {"frame", false},   {"text", true},
			};
			uint32 ecarts132 = 0u;
			char pire132[220];
			pire132[0] = '\0';
			NkUIDocument d132;
			d132.NewDocument("Toile", NkAuthor::Humain);
			d132.nodes[0].layout.kind = NkLayoutKind::Free;
			for (uint32 t = 0; t < 6u; ++t) {
				const int32 i = d132.AddChild(0, "", NkAuthor::Humain);
				d132.nodes[(uint32)i].shape = NkString(kT132[t].forme);
				const char *const *titres = nullptr;
				const uint32 nT = NkSectionsInspecteur(false, false, titres);
				const char *retenues[32];
				const int32 sel1[1] = {i};
				const uint32 nR = NkSectionsRetenues(d132, sel1, 1u, titres, nT, retenues, 32u);
				bool typoLa = false;
				for (uint32 k = 0; k < nR; ++k)
					if (NkComponentDecl::StrEq(retenues[k], "TYPOGRAPHIE"))
						typoLa = true;
				if (typoLa != kT132[t].typographie) {
					++ecarts132;
					if (!pire132[0])
						snprintf(pire132, sizeof(pire132),
								 "<< %s >> : TYPOGRAPHIE %s, attendue %s", kT132[t].forme,
								 typoLa ? "presente" : "absente",
								 kT132[t].typographie ? "presente" : "absente");
				}
			}

			// L'INTERSECTION : un rect ET un texte selectionnes ensemble -> la
			// section n'est PAS montree, sinon elle agirait sur une partie de la
			// selection sans qu'on voie laquelle.
			bool typoMulti = true;
			{
				const int32 iRect = d132.AddChild(0, "", NkAuthor::Humain);
				d132.nodes[(uint32)iRect].shape = NkString("rect");
				const int32 iTexte = d132.AddChild(0, "", NkAuthor::Humain);
				d132.nodes[(uint32)iTexte].shape = NkString("text");
				const char *const *titres = nullptr;
				const uint32 nT = NkSectionsInspecteur(false, false, titres);
				const char *retenues[32];
				const int32 sel2[2] = {iRect, iTexte};
				const uint32 nR = NkSectionsRetenues(d132, sel2, 2u, titres, nT, retenues, 32u);
				typoMulti = false;
				for (uint32 k = 0; k < nR; ++k)
					if (NkComponentDecl::StrEq(retenues[k], "TYPOGRAPHIE"))
						typoMulti = true;
			}

			// CE QUI NE DOIT PAS DISPARAITRE : une section qui n'agit pas ENCORE
			// reste visible. ALIGNEMENT en est l'exemple donne -- il suffit de
			// changer le mode du parent pour qu'il agisse.
			bool alignementReste = false, dispositionReste = false;
			{
				const int32 iR = d132.AddChild(0, "", NkAuthor::Humain);
				d132.nodes[(uint32)iR].shape = NkString("rect");
				const char *const *titres = nullptr;
				const uint32 nT = NkSectionsInspecteur(false, false, titres);
				const char *retenues[32];
				const int32 sel1[1] = {iR};
				const uint32 nR = NkSectionsRetenues(d132, sel1, 1u, titres, nT, retenues, 32u);
				for (uint32 k = 0; k < nR; ++k) {
					if (NkComponentDecl::StrEq(retenues[k], "ALIGNEMENT"))
						alignementReste = true;
					if (NkComponentDecl::StrEq(retenues[k], "DISPOSITION"))
						dispositionReste = true;
				}
			}

			// LE CONTROLE POSITIF DE L'INSTRUMENT : une liste attendue amputee
			// DOIT etre vue differente. Sans lui, zero ecart serait aussi le
			// score d'une comparaison inerte.
			bool detecteUneAmputation = false;
			{
				const char *const gauche[3] = {"DISPOSITION", "ALIGNEMENT", "TYPOGRAPHIE"};
				const char *const droite[2] = {"DISPOSITION", "ALIGNEMENT"};
				uint32 manquants = 0u;
				for (uint32 a = 0; a < 3u; ++a) {
					bool vu = false;
					for (uint32 b = 0; b < 2u; ++b)
						if (NkComponentDecl::StrEq(gauche[a], droite[b]))
							vu = true;
					if (!vu)
						++manquants;
				}
				detecteUneAmputation = (manquants == 1u);
			}

			// ⚠️ ET LE PANNEAU LES APPELLE-T-IL VRAIMENT ? Mesure du 07/09 : couper le
			//    branchement du panneau (`FiltrerParType`) ne faisait rougir AUCUN essai
			//    -- le temoin prouvait la regle sur des fonctions que personne n'aurait
			//    appelees. On LIT DONC LES SOURCES, comme la famille 11 du kit : le
			//    chooser ne doit plus rendre une table NON filtree.
			bool panneauBranche = false;
			{
				const NkString src = NkFile::ReadAllText(
					"Applications/NKUIDesign/src/NKUIDesign/Panels.h");
				if (!src.Empty()) {
					auto compte = [&](const char *aig) -> uint32 {
						uint32 n = 0u;
						for (const char *d = src.Data(); *d; ++d) {
							const char *x = d, *y = aig;
							while (*x && *y && *x == *y) {
								++x;
								++y;
							}
							if (!*y)
								++n;
						}
						return n;
					};
					// DEUX branchements (avec cible / sans cible), et AUCUN retour direct
					// d'une table non filtree.
					panneauBranche = compte("FiltrerParType(t, nb, count)") == 2u
									 && compte("return SectionsAvecCible(count)") == 0u
									 && compte("return SectionsSansCible(count)") == 0u;
				}
			}
			// ② (07/09) ET LA PHRASE NE PARLE PAS D'UN ELEMENT QUI N'EXISTE PAS.
			//    << Rien de selectionne >> est un cas A PART ENTIERE, pas une selection
			//    vide : la meme phrase servait aux deux, et disait << l'element
			//    selectionne >> alors qu'il n'y en avait aucun.
			const char *sansNoeud = NkPhraseSectionVide("TYPOGRAPHIE", nullptr);
			const char *avecNoeud = NkPhraseSectionVide("TYPOGRAPHIE", &d132.nodes[1]);
			// LA RELATION, PLUTOT QU'UN MOTIF ACCENTUE : sans nœud, la phrase ne
			// depend PAS de la section (elle est generique) ; avec un nœud, elle en
			// depend. C'est ce que << ne parle pas d'un element qui n'existe pas >>
			// veut dire, dit sans qu'aucun accent n'ait a survivre a un echappement.
			// ⚠️ Ma premiere ecriture cherchait la sous-chaine « lement » ; l'octet
			//    accentue est arrive DOUBLEMENT ENCODE dans le fichier et le motif ne
			//    mordait jamais -- l'essai etait rouge sur son CONTROLE POSITIF, pas
			//    sur son sujet. Une assertion qui depend d'un octet non-ASCII depend
			//    de la chaine d'outils qui l'a ecrite.
			const char *sansAutre = NkPhraseSectionVide("DISPOSITION", nullptr);
			const bool phraseJuste = NkComponentDecl::StrEq(sansNoeud, sansAutre)
									 && !NkComponentDecl::StrEq(sansNoeud, avecNoeud);
			snprintf(det, sizeof(det),
					 "six types confrontes a la liste attendue : %u ecart(s) [%s] ; "
					 "selection MIXTE rect+texte : TYPOGRAPHIE %s (attendue absente) ; ce qui "
					 "n'agit pas ENCORE reste visible : ALIGNEMENT=%d, DISPOSITION=%d ; "
					 "CONTROLE POSITIF (une liste amputee est vue differente)=%d ; le PANNEAU "
					 "appelle bien le filtre (lu dans les sources)=%d ; la phrase sans nœud ne "
					 "parle pas d'un element (\"%s\") et celle avec nœud si (\"%s\") -> %d",
					 ecarts132, pire132[0] ? pire132 : "(aucun)",
					 typoMulti ? "presente" : "absente", alignementReste ? 1 : 0,
					 dispositionReste ? 1 : 0, detecteUneAmputation ? 1 : 0,
					 panneauBranche ? 1 : 0, sansNoeud, avecNoeud, phraseJuste ? 1 : 0);
			check("132. (1) UNE SECTION QUI NE PEUT PAS S'APPLIQUER DISPARAIT : TYPOGRAPHIE n'est offerte "
				  "qu'a un nœud texte, elle disparait des cinq autres types, et une selection MIXTE ne la "
				  "montre pas (intersection : une section ne doit pas agir sur une partie de la selection "
				  "sans qu'on voie laquelle). Ce qui n'agit pas ENCORE -- ALIGNEMENT, DISPOSITION -- reste "
				  "VISIBLE : le cacher rendrait la fonctionnalite introuvable",
				  ecarts132 == 0u && !typoMulti && alignementReste && dispositionReste
					  && detecteUneAmputation && panneauBranche && phraseJuste,
				  det);
		}
		// -- 133. L'EXPORT SVG DIT LA VERITE DU DOCUMENT ----------------------
		//
		// DEUX TROUS, TROUVES EN REPONDANT A UNE DEMANDE DE STATUT, et tous deux
		//    silencieux -- le fichier exporte etait COHERENT et FAUX :
		//
		//  (A) `n.opacite` et `n.fusion` -- les deux champs du NŒUD -- n'etaient
		//      PAS ecrits. `Style()` prend un `NkRemplissage*`, jamais un nœud :
		//      l'export portait l'opacite et la fusion PAR REMPLISSAGE et jetait
		//      celles du calque. A l'ecran elles agissent ; au fichier elles
		//      disparaissaient.
		//
		//  (B) `Fusion()` ne rendait que les CINQ modes que NOTRE PEINTRE sait
		//      faire, et jetait les treize autres -- alors que SVG les connait
		//      tous (`mix-blend-mode` CSS). *L'export s'alignait sur la limite de
		//      l'ECRAN au lieu de la capacite du FORMAT.* Un document qui porte
		//      << overlay >> perdait le mot en chemin.
		//
		// ⚠️ ET L'IRONIE EST UTILE A DIRE : SVG applique `opacity` au GROUPE, donc
		//    au nœud ET a ses enfants, composite UNE SEULE FOIS -- c'est-a-dire le
		//    vrai calque que notre peintre ne sait pas encore faire. Sur ce point
		//    l'export est PLUS JUSTE que l'ecran, et c'est un fait, pas un defaut.
		{
			char det[600];
			auto contient133 = [](const char *h, const char *n) -> bool {
				if (!h || !n)
					return false;
				for (const char *p = h; *p; ++p) {
					const char *a = p, *b = n;
					while (*a && *b && *a == *b)
						++a, ++b;
					if (!*b)
						return true;
				}
				return false;
			};
			auto svgDe = [&](float32 opacite, const char *fusion, NkString &out) -> bool {
				static DesignState st133;
				st133.doc.NewDocument("Toile", NkAuthor::Humain);
				st133.doc.SetMetric("espacement", 0.f);
				st133.doc.SetMetric("marge", 0.f);
				st133.doc.nodes[0].layout.kind = NkLayoutKind::Free;
				const int32 pg = st133.doc.AddChild(0, "", NkAuthor::Humain);
				{
					NkUINode &p = st133.doc.nodes[(uint32)pg];
					p.shape = NkString("frame");
					p.layout.kind = NkLayoutKind::Free;
					p.width.mode = NkSizeMode::Fixed;
					p.width.value = 200.f;
					p.height.mode = NkSizeMode::Fixed;
					p.height.value = 120.f;
				}
				const int32 f = st133.doc.AddChild(pg, "", NkAuthor::Humain);
				{
					NkUINode &q = st133.doc.nodes[(uint32)f];
					q.shape = NkString("rect");
					q.fill = NkString("#ff0000");
					q.opacite = opacite;
					q.fusion = NkString(fusion);
					q.width.mode = NkSizeMode::Fixed;
					q.width.value = 40.f;
					q.height.mode = NkSizeMode::Fixed;
					q.height.value = 20.f;
				}
				st133.Recompute(NkPaintRect{0.f, 0.f, 800.f, 600.f});
				NkExportOptions o;
				o.format = NkExportFormat::SVG;
				NkExportResultat r;
				return NkExporterSVG(st133, o, "", out, r);
			};

			// (a) L'OPACITE DU NŒUD ARRIVE AU FICHIER.
			NkString sOp;
			const bool okOp = svgDe(50.f, "", sOp);
			const bool opaciteEcrite = okOp && contient133(sOp.Data(), "opacity=\"0.5\"");

			// (b) ADDITIVE : a 100 %, RIEN. Sans cet essai, ecrire toujours
			//     l'attribut passerait pour une reussite.
			NkString sPlein;
			const bool okPlein = svgDe(100.f, "", sPlein);
			const bool rienAPlein = okPlein && !contient133(sPlein.Data(), "opacity=\"1");

			// (c) UN MODE QUE LE PEINTRE NE SAIT PAS MAIS QUE LE FORMAT CONNAIT :
			//     c'est LUI qui prouve que l'export ne se limite plus a l'ecran.
			NkString sOv;
			const bool okOv = svgDe(100.f, "overlay", sOv);
			const bool overlayEcrit = okOv && contient133(sOv.Data(), "mix-blend-mode:overlay");

			// (d) ET UN MODE QUE LE FORMAT NE CONNAIT PAS reste NON ecrit -- on
			//     n'invente pas une valeur CSS. `plus-darker` est un nom de
			//     Lunacy, pas une valeur de `mix-blend-mode`.
			NkString sPd;
			const bool okPd = svgDe(100.f, "plus-darker", sPd);
			const bool pasInvente = okPd && !contient133(sPd.Data(), "mix-blend-mode:plus-darker");

			// (e) CONTROLE POSITIF DU MONTAGE : le SVG produit contient bien le
			//     rectangle. Sans lui, tous les << absent >> ci-dessus seraient
			//     aussi le score d'un export vide.
			const bool montageValide = okOp && contient133(sOp.Data(), "<rect")
									   && contient133(sOp.Data(), "<svg ");

			snprintf(det, sizeof(det),
					 "(a) opacite 50%% -> `opacity=\"0.5\"` present=%d ; (b) a 100%%, aucun "
					 "attribut=%d ; (c) `overlay` (que le peintre ne sait pas) -> "
					 "`mix-blend-mode:overlay` present=%d ; (d) `plus-darker` (hors CSS) NON "
					 "invente=%d ; (e) CONTROLE POSITIF : le SVG contient bien <svg> et <rect>=%d",
					 opaciteEcrite ? 1 : 0, rienAPlein ? 1 : 0, overlayEcrit ? 1 : 0,
					 pasInvente ? 1 : 0, montageValide ? 1 : 0);
			check("133. L'EXPORT SVG DIT LA VERITE DU DOCUMENT : l'opacite ET la fusion DU NŒUD arrivent au "
				  "fichier (elles etaient perdues en silence -- `Style()` ne lit que les remplissages), la "
				  "cle reste ADDITIVE, et un mode que NOTRE PEINTRE ne sait pas mais que le FORMAT connait "
				  "est exporte quand meme -- l'export ne doit pas heriter des limites de l'ecran. Un mode "
				  "hors CSS n'est PAS invente",
				  opaciteEcrite && rienAPlein && overlayEcrit && pasInvente && montageValide, det);
		}
		// -- 134. (2) L'EXPORT NE S'OUVRE PLUS SUR RIEN ------------------------
		//
		// Rodolf : « rien n'est selectionne mais quand je clique sur exporter, le
		//    panneau s'ouvre. Ce n'est pas normal -- sauf si ca liste tous les
		//    elements exportables du canvas infini. »
		//
		// LA MESURE QUI A ORIENTE LE LOT : le panneau ne s'ouvrait pas << sur
		//    rien >>. Sans selection, l'export retombait sur `NkPageParDefaut`,
		//    c'est-a-dire LA PREMIERE PAGE -- alors que son document en porte
		//    TROIS. Le choix etait arbitraire et muet : le message partait au pied
		//    de fenetre, jamais dans le panneau.
		//
		// TROIS EXIGENCES :
		//    (a) sans selection, la CIBLE devient tout le canvas, et le compte
		//        annonce EGALE le nombre d'elements reellement exportables ;
		//    (b) ZERO exportable -> le panneau NE S'OUVRE PAS, et le refus se dit
		//        -- un panneau qui liste zero element est le meme defaut sous un
		//        autre nom ;
		//    (c) une selection existante n'est PAS ecrasee : le geste garde son
		//        sens quand il en a un.
		{
			char det[600];
			// UN CANVAS A TROIS PAGES, dont une MASQUEE et une de taille nulle :
			// l'essai doit compter DEUX exportables, pas quatre. Sans ces deux
			// pieges, << le compte egale le nombre d'enfants >> serait vrai par
			// accident et ne mesurerait rien.
			static DesignState st134;
			st134.doc.NewDocument("Toile", NkAuthor::Humain);
			st134.doc.SetMetric("espacement", 0.f);
			st134.doc.SetMetric("marge", 0.f);
			st134.doc.nodes[0].layout.kind = NkLayoutKind::Free;
			// ⚠️ LES PAGES SONT POSEES A DES ENDROITS DIFFERENTS, et c'est le montage
			//    qui compte. Premiere ecriture : `posX` laisse a zero -- les deux pages
			//    se SUPERPOSAIENT, donc l'union de leurs boites valait la plus large et
			//    l'essai (d) ne pouvait pas distinguer << tout le canvas >> de << la
			//    premiere page >>. QUATRIEME fois en trois jours qu'un montage ne pose
			//    pas l'etat qu'il mesure : *le regime risque se POSE, il ne se suppose
			//    pas.*
			float32 xPage134 = 0.f;
			auto page134 = [&](float32 w, float32 h, bool masquee) {
				const int32 i = st134.doc.AddChild(0, "", NkAuthor::Humain);
				NkUINode &p = st134.doc.nodes[(uint32)i];
				p.posX = xPage134;
				xPage134 += w + 40.f; // les pages se suivent, elles ne se recouvrent pas
				p.shape = NkString("frame");
				p.layout.kind = NkLayoutKind::Free;
				p.masque = masquee;
				p.width.mode = NkSizeMode::Fixed;
				p.width.value = w;
				p.height.mode = NkSizeMode::Fixed;
				p.height.value = h;
				return i;
			};
			page134(200.f, 120.f, false); // exportable
			page134(160.f, 90.f, false);  // exportable
			page134(200.f, 120.f, true);  // MASQUEE : le peintre s'arrete avant
			page134(0.f, 0.f, false);	  // BOITE VIDE : un fichier vide serait un faux succes
			st134.Recompute(NkPaintRect{0.f, 0.f, 900.f, 600.f});
			NkVector<int32> exportables;
			const uint32 nExp = NkElementsExportables(st134, st134.layout, &exportables);

			// (a) SANS SELECTION : la cible devient le canvas, le compte est annonce.
			st134.sel.Clear();
			st134.selected = 0;
			st134.choixExport.dialogue.open = false;
			NkOuvrirDialogueExport(st134, false);
			const bool ouvert = st134.choixExport.dialogue.open;
			const bool cibleCanvas = st134.choixExport.tout;
			// ⚠️ LA VALEUR EST CAPTUREE ICI, PAS RELUE A LA FIN. Premiere ecriture :
			//    le detail relisait `nbExportables` APRES l'essai (c), qui la remet a
			//    zero -- la ligne affichait << compte annonce 0 = mesure 2 -> 1 >>, une
			//    incoherence apparente sur une assertion pourtant juste. *Un chiffre
			//    rend compte de l'instant ou il est LU, pas de celui ou on l'imprime.*
			const uint32 compteAnnonce = st134.choixExport.nbExportables;
			const bool compteJuste = compteAnnonce == nExp;

			// (b) ZERO EXPORTABLE : le panneau NE S'OUVRE PAS, et le refus se dit.
			static DesignState vide134;
			vide134.doc.NewDocument("Toile", NkAuthor::Humain);
			vide134.Recompute(NkPaintRect{0.f, 0.f, 900.f, 600.f});
			NkVector<int32> rien;
			const uint32 nVide = NkElementsExportables(vide134, vide134.layout, &rien);
			vide134.sel.Clear();
			vide134.selected = 0;
			vide134.choixExport.dialogue.open = false;
			vide134.status = NkString();
			NkOuvrirDialogueExport(vide134, false);
			const bool refuse = !vide134.choixExport.dialogue.open;
			const bool refusDit = !vide134.status.Empty();

			// (c) UNE SELECTION EXISTANTE N'EST PAS ECRASEE.
			st134.sel.Clear();
			st134.sel.Add(exportables.Empty() ? 1 : exportables[0]);
			st134.selected = exportables.Empty() ? 1 : exportables[0];
			st134.choixExport.dialogue.open = false;
			NkOuvrirDialogueExport(st134, true);
			const bool selectionGardee = st134.choixExport.selection && !st134.choixExport.tout;

			// (e) CE QUI DECRIT L'ETENDUE LA SUIT. Sur << tout le canvas >>, le nom
			//     propose etait celui de LA PREMIERE PAGE : l'etendue etait devenue
			//     juste, ce qui la NOMME ne l'etait pas. *Quand on change ce qu'une
			//     chose fait, tout ce qui la nomme bouge dans le meme lot -- sinon le
			//     mensonge change simplement de place.*
			char nomTout[200], nomPage[200];
			{
				NkExportOptions oN;
				oN.tout = true;
				NkNomObjetExport(st134, oN, nomTout, sizeof(nomTout));
				NkExportOptions oP; // la page seule : son nom ne doit PAS changer
				NkNomObjetExport(st134, oP, nomPage, sizeof(nomPage));
			}
			const bool nomSuitEtendue = NkComponentDecl::StrEq(nomTout, "canvas")
										 && !NkComponentDecl::StrEq(nomPage, "canvas");

			// (d) ET LE DRAPEAU EST-IL LU ? Sans cet essai j'aurais pose `tout` et
			//     personne ne l'aurait consomme -- le defaut meme que ce depot paie
			//     depuis huit fois. La zone exportee doit etre l'UNION des deux pages,
			//     donc plus large que la premiere seule.
			NkVector<int32> nds;
			NkPaintRect zTout = {0.f, 0.f, 0.f, 0.f}, zPage = {0.f, 0.f, 0.f, 0.f};
			char pq[160];
			NkExportOptions oTout;
			oTout.tout = true;
			const bool zoneTout = NkZoneExport(st134, st134.layout, oTout, nds, zTout, pq, sizeof(pq));
			const uint32 nTout = (uint32)nds.Size();
			NkExportOptions oPage; // la premiere page seule, l'ancien comportement
			const bool zonePage = NkZoneExport(st134, st134.layout, oPage, nds, zPage, pq, sizeof(pq));
			const bool toutEstLu = zoneTout && zonePage && nTout == nExp && zTout.w > zPage.w;

			snprintf(det, sizeof(det),
					 "canvas de 4 enfants dont 1 masquee et 1 de taille nulle -> %u exportable(s) "
					 "(attendu 2) ; (a) sans selection : panneau ouvert=%d, cible=canvas %d, "
					 "compte annonce %u = mesure %u -> %d ; (b) canvas VIDE (%u exportable) : "
					 "panneau refuse=%d, refus dit=%d ; (c) avec selection : la selection est "
					 "gardee=%d ; (d) le drapeau EST LU : %u nœud(s) vises, zone %.0f de large "
					 "contre %.0f pour la premiere page seule -> %d ; (e) le nom propose : "
					 "\"%s\" pour le canvas, \"%s\" pour la page -> %d",
					 nExp, ouvert ? 1 : 0, cibleCanvas ? 1 : 0,
					 compteAnnonce, nExp, compteJuste ? 1 : 0, nVide,
					 refuse ? 1 : 0, refusDit ? 1 : 0, selectionGardee ? 1 : 0, nTout,
					 (double)zTout.w, (double)zPage.w, toutEstLu ? 1 : 0, nomTout, nomPage,
					 nomSuitEtendue ? 1 : 0);
			check("134. (2) L'EXPORT NE S'OUVRE PLUS SUR RIEN : sans selection la cible devient TOUT LE "
				  "CANVAS et le panneau ANNONCE combien d'elements il a trouves ; un canvas sans aucun "
				  "element exportable REFUSE d'ouvrir et le dit (un panneau qui liste zero element est le "
				  "meme defaut sous un autre nom) ; et une selection existante n'est pas ecrasee. Le "
				  "montage porte une page MASQUEE et une de taille NULLE, sinon << le compte egale le "
				  "nombre d'enfants >> serait vrai par accident",
				  nExp == 2u && ouvert && cibleCanvas && compteJuste && refuse && refusDit
					  && selectionGardee && toutEstLu && nomSuitEtendue,
				  det);
		}
		// ── 135. ③ LES PROPRIETES DU CANVAS REMPLACENT LE PANNEAU MUET (07/09).
		//    Capture de Rodolf : sans selection, TREIZE sections avec leur tiret, et
		//    aucune ne pouvait rien faire. Le cas << rien de selectionne >> n'avait PAS
		//    DE TABLE : il tombait dans celle << sans cible >> avec une selection vide,
		//    et le filtre par type ne pouvait rien retirer -- sans nœud, aucune regle
		//    de type ne disqualifie quoi que ce soit.
		//
		// ⚠️ AUCUN TITRE ACCENTUE N'EST RECOPIE ICI. On compare les DEUX TABLES entre
		//    elles : la section honnete est celle qui figure dans les deux, et le compte
		//    des communes dit tout. Ecrire << ALIGNER LA SELECTION >> en clair aurait
		//    remis un octet non-ASCII au cœur d'une assertion -- la faute d'hier.
		{
			char det[900];
			int32 nCanvas = 0, nSans = 0;
			const editorkit::NkInspectorSection *canvas = InspectorPanel::SectionsCanvas(nCanvas);
			const editorkit::NkInspectorSection *sans = InspectorPanel::SectionsSansCible(nSans);
			uint32 communs = 0u, nomCanvas = 0u;
			for (int32 i = 0; i < nCanvas; ++i) {
				if (NkComponentDecl::StrEq(canvas[i].titre, "CANVAS"))
					++nomCanvas;
				for (int32 j = 0; j < nSans; ++j)
					if (NkComponentDecl::StrEq(canvas[i].titre, sans[j].titre))
						++communs;
			}
			// (a) deux sections, dont UNE seule vient de l'ancienne table : la section
			//     honnete (celle qui dit << rien de selectionne >> et qui AGIRA des qu'on
			//     selectionnera) reste, les autres partent. Retour du coordinateur : ne
			//     pas la ranger avec les muettes.
			const bool tableJuste = nCanvas == 2 && nomCanvas == 1u && communs == 1u;
			const uint32 muettesRetirees = (uint32)nSans - communs;

			// (b) LE FOND : une couleur, ou le theme -- et un REFUS plutot qu'un repli.
			uint32 rgba = 0u;
			const bool okSix = NkFondCanvasExplicite("#0d1117", rgba);
			const uint32 rgbaSix = rgba;
			uint32 poubelle = 0u;
			const bool okVide = NkFondCanvasExplicite("", poubelle);
			const bool okNul = NkFondCanvasExplicite(nullptr, poubelle);
			const bool okSansDiese = NkFondCanvasExplicite("0d1117", poubelle);
			const bool okLettres = NkFondCanvasExplicite("#zzzzzz", poubelle);
			// ⚠️ TROIS ET HUIT CHIFFRES SONT REFUSES, et c'est le PARSEUR qui le dicte :
			//    `NkGHexRGBA` lit six chiffres et pose l'alpha. Les accepter aurait
			//    promis une couleur et peint une autre -- le repli plausible.
			const bool okTrois = NkFondCanvasExplicite("#abc", poubelle);
			const bool okHuit = NkFondCanvasExplicite("#0d1117ff", poubelle);
			const bool fondJuste = okSix && rgbaSix == 0x0d1117FFu && !okVide && !okNul
								   && !okSansDiese && !okLettres && !okTrois && !okHuit;

			// (c) ET LE PANNEAU S'EN SERT-IL ? On lit les sources -- la lecon du 07/09 :
			//     couper un branchement ne faisait rougir aucun essai.
			//
			// ⚠️ CE QUE CETTE LECTURE PROUVE, ET CE QU'ELLE NE PROUVE PAS. Elle voit
			//    qu'un appel EXISTE et qu'il est au bon endroit ; elle ne voit pas si
			//    son resultat est UTILISE. Mesure faite : la mutation
			//    << if (false && NkFondCanvasExplicite(...)) >> reste VERTE ici. Pour
			//    aller plus loin il faudrait enregistrer les commandes du fond de la
			//    toile -- or ce fond est peint par `NkDesignPaint` sur un contexte
			//    VIVANT, dans `OnUI`, la ou aucun banc sans fenetre n'atteint. C'est
			//    ecrit plutot que compense : la couleur reellement peinte reste un
			//    coup d'œil humain.
			bool branche = false, deuxPortes = false;
			uint32 basculesGrille = 0u, basculesSuivies = 0u;
			{
				const NkString src =
					NkFile::ReadAllText("Applications/NKUIDesign/src/NKUIDesign/Panels.h");
				if (!src.Empty()) {
					auto positions = [&](const char *aig, uint32 *out, uint32 max) -> uint32 {
						uint32 n = 0u;
						for (const char *d = src.Data(); *d; ++d) {
							const char *x = d, *y = aig;
							while (*x && *y && *x == *y) {
								++x;
								++y;
							}
							if (!*y && n < max)
								out[n++] = (uint32)(d - src.Data());
						}
						return n;
					};
					uint32 pos[16];
					branche = positions("return SectionsCanvas(count);", pos, 16u) == 1u
							  && positions("NkFondCanvasExplicite(mSt->canvasFill.couleur.Data()", pos, 16u)
									 == 1u;
					// LA RELATION DES DEUX PORTES : chaque site qui bascule la grille
					// enregistre le decor dans la foulee. Compter les deux separement
					// aurait laisse passer << une porte qui bascule deux fois >>.
					//
					// ⚠️ ON CHERCHE L'APPEL, PAS LE NOM -- << EnregistrerDecor(); >>
					//    avec ses parentheses et son point-virgule. Premiere ecriture :
					//    le nom seul. La mutation << on retire l'appel du menu >> est
					//    restee VERTE parce qu'un COMMENTAIRE deux lignes plus haut
					//    nommait la fonction. *Un temoin qui lit du texte peut lire un
					//    commentaire ; il faut chercher ce qui S'EXECUTE.*
					// ⚠️ ET LA FENETRE DE 260 CARACTERES EST UNE CONTRAINTE SUR LE
					//    CODE, pas seulement sur l'essai : elle m'a rougi une seconde
					//    fois quand le commentaire que je venais d'ecrire a repousse
					//    l'appel hors de portee. La lecon a sa place -- ici, dans le
					//    temoin -- et la ou l'appel est fait, une seule ligne.
					basculesGrille = positions("grilleVisible = !mSt->grilleVisible;", pos, 16u);
					for (uint32 i = 0; i < basculesGrille && i < 16u; ++i) {
						const char *suite = src.Data() + pos[i];
						bool trouve = false;
						for (uint32 k = 0; k < 260u && suite[k]; ++k) {
							const char *x = suite + k, *y = "EnregistrerDecor();";
							while (*x && *y && *x == *y) {
								++x;
								++y;
							}
							if (!*y) {
								trouve = true;
								break;
							}
						}
						if (trouve)
							++basculesSuivies;
					}
					deuxPortes = basculesGrille == 2u && basculesSuivies == 2u;
				}
			}
			snprintf(det, sizeof(det),
					 "(a) la table de << rien de selectionne >> : %d section(s), dont "
					 "\"CANVAS\" x%u et %u commune(s) avec l'ancienne table de %d -- %u "
					 "muette(s) retiree(s), la section honnete gardee -> %d ; (b) le fond : "
					 "\"#0d1117\" -> 0x%08X, et vide/nul/sans-diese/lettres/3-chiffres/"
					 "8-chiffres tous REFUSES -> %d ; (c) les sources : branchement lu %d, "
					 "%u bascule(s) de grille dont %u suivies d'un enregistrement -> %d",
					 nCanvas, nomCanvas, communs, nSans, muettesRetirees, tableJuste ? 1 : 0,
					 (unsigned)rgbaSix, fondJuste ? 1 : 0, branche ? 1 : 0, basculesGrille,
					 basculesSuivies, deuxPortes ? 1 : 0);
			check("135. (3) LES PROPRIETES DU CANVAS REMPLACENT LE PANNEAU MUET : sans selection, "
				  "l'inspecteur montrait treize sections a tiret dont aucune ne pouvait agir -- le "
				  "cas << rien de selectionne >> n'avait pas de table a lui. Il en a une : le FOND "
				  "(le theme, ou une couleur, et le champ vide veut dire << comme aujourd'hui >>) "
				  "et la GRILLE, qui bascule LA MEME variable que le clic droit. La section qui "
				  "disait vrai -- celle qui agira des qu'on selectionnera -- reste : elle est la "
				  "seule commune aux deux tables, et c'est mesure, pas affirme",
				  tableJuste && fondJuste && branche && deuxPortes, det);
		}
		// ── 136. UN SEUL SELECTEUR, ET LA GARDE CHEZ CELUI QUI EN A BESOIN (07/09).
		//    Rodolf : << retire le color picker pour mettre le bon >>. Ce qu'il y avait
		//    etait un GENRE A MOI (3) pose pour contourner une garde -- une seconde
		//    porte pour un seul geste -- et un PLACEMENT recopie qui ouvrait la fenetre
		//    212 px a gauche, par-dessus la toile.
		{
			char det[900];
			// (a) LA GARDE, DEPLACEE : elle appartient a celui qui OUVRE.
			//     Six regimes, dont celui du defaut : une pastille qui ne decrit AUCUN
			//     nœud ne se ferme pas quand rien n'est selectionne.
			const bool g1 = !NkPickerDoitFermer(-1, -1, false); // le decor : il RESTE
			const bool g2 = !NkPickerDoitFermer(-1, 7, true);	// selection ailleurs : il reste
			const bool g3 = !NkPickerDoitFermer(3, 3, true);	// son nœud est celui qu'on regarde
			const bool g4 = NkPickerDoitFermer(3, -1, true);	// deselectionne -> il ferme
			const bool g5 = NkPickerDoitFermer(3, 5, true);		// un AUTRE nœud -> il ferme
			const bool g6 = NkPickerDoitFermer(3, 3, false);	// le nœud a disparu -> il ferme
			const bool gardeJuste = g1 && g2 && g3 && g4 && g5 && g6;

			// (b) LE PLACEMENT, ET LA RELATION QUI DIT << ANCRE SOUS LA PASTILLE >>.
			// ⚠️ PAS UNE COORDONNEE ATTENDUE : la fenetre doit CHEVAUCHER l'ancre en X.
			//    C'est ca, << ancre sous la pastille >>, et ca survit a un changement de
			//    largeur. L'ancienne regle (`ancre.x - w - 8`) ne chevauchait JAMAIS.
			const float32 vW = 1600.f, vH = 900.f;
			const NkRect ancre = {vW - 24.f, 300.f, 16.f, 16.f}; // au bord droit, comme l'inspecteur
			const NkRect sous = editorkit::NkPlacerPresDeLAncre(ancre, 212.f, 350.f, vW, vH,
																editorkit::NkCoteAncre::Dessous);
			const bool chevauche = sous.x < ancre.x + ancre.w && sous.x + sous.w > ancre.x;
			const bool dansLaVue = sous.x >= 2.f && sous.x + sous.w <= vW - 2.f && sous.y >= 2.f
								   && sous.y + sous.h <= vH - 2.f;
			const bool dessous = sous.y >= ancre.y + ancre.h;
			// pas la place dessous : elle se retourne AU-DESSUS, elle ne sort pas
			const NkRect basse = {40.f, vH - 40.f, 16.f, 16.f};
			const NkRect retournee = editorkit::NkPlacerPresDeLAncre(basse, 212.f, 350.f, vW, vH,
																	editorkit::NkCoteAncre::Dessous);
			const bool retourneOk = retournee.y + retournee.h <= basse.y && retournee.y >= 2.f;
			// et le cote << a gauche >> des popovers larges reste ce qu'il etait
			const NkRect gauche = editorkit::NkPlacerPresDeLAncre(ancre, 236.f, 500.f, vW, vH,
																  editorkit::NkCoteAncre::AGauche);
			const bool gaucheOk = gauche.x + gauche.w <= ancre.x && gauche.x >= 2.f;
			const bool placementJuste = chevauche && dansLaVue && dessous && retourneOk && gaucheOk;

			// (c) UNE PORTE, PAS DEUX : les sources.
			bool unSeulChemin = false;
			uint32 nPastille = 0u, nGenre3 = 0u, nMain = 0u, nKit = 0u;
			{
				const NkString src =
					NkFile::ReadAllText("Applications/NKUIDesign/src/NKUIDesign/Panels.h");
				if (!src.Empty()) {
					auto compte = [&](const char *aig) -> uint32 {
						uint32 n = 0u;
						for (const char *d = src.Data(); *d; ++d) {
							const char *x = d, *y = aig;
							while (*x && *y && *x == *y) {
								++x;
								++y;
							}
							if (!*y)
								++n;
						}
						return n;
					};
					// CINQ portes ouvrent le selecteur (etats, effets, remplissages,
					// bordures, canvas) et TOUTES passent par la meme fonction.
					nPastille = compte("NkPastilleCouleur(ctx, *mSt");
					nGenre3 = compte("picker.genre = 3u");	 // le doublon : il ne doit plus exister
					nMain = compte("sw.x - pw - 8.f");		 // le placement recopie : idem
					nKit = compte("NkPlacerPresDeLAncre(sw"); // les trois sites lisent le kit
					unSeulChemin = nPastille == 5u && nGenre3 == 0u && nMain == 0u && nKit == 3u;
				}
			}
			snprintf(det, sizeof(det),
					 "(a) la garde declaree : decor sans selection RESTE=%d, son nœud "
					 "regarde=%d, deselectionne FERME=%d, autre nœud FERME=%d, nœud disparu "
					 "FERME=%d -> %d ; (b) sous l'ancre : x=%.0f (ancre %.0f..%.0f) chevauche=%d, "
					 "dans la vue=%d, dessous=%d, retournee=%d, cote gauche conserve=%d -> %d ; "
					 "(c) une porte : %u appels a la pastille, %u genre a soi, %u placement(s) "
					 "ecrit(s) a la main, %u site(s) qui lisent le kit -> %d",
					 g1 ? 1 : 0, g3 ? 1 : 0, g4 ? 1 : 0, g5 ? 1 : 0, g6 ? 1 : 0,
					 gardeJuste ? 1 : 0, (double)sous.x, (double)ancre.x,
					 (double)(ancre.x + ancre.w), chevauche ? 1 : 0, dansLaVue ? 1 : 0,
					 dessous ? 1 : 0, retourneOk ? 1 : 0, gaucheOk ? 1 : 0, placementJuste ? 1 : 0,
					 nPastille, nGenre3, nMain, nKit, unSeulChemin ? 1 : 0);
			check("136. UN SEUL SELECTEUR DE COULEUR, ET LA GARDE CHEZ CELUI QUI EN A BESOIN : le "
				  "selecteur ne sert qu'a CHOISIR UNE COULEUR -- exiger un nœud selectionne n'a "
				  "jamais ete son affaire. La regle est DEPLACEE (pas retiree) chez les quatre "
				  "sections qui decrivent un nœud ; la cinquieme porte, le decor de la toile, n'en "
				  "decrit aucun et le declare. Le genre a soi qui contournait la garde est "
				  "supprime, et les TROIS placements recopies -- qui avaient tous diverge de "
				  "NKGui -- passent par une seule fonction du kit, chacun nommant son cote",
				  gardeJuste && placementJuste && unSeulChemin, det);
		}
		// ── 139. LA PIPETTE REND EXACTEMENT LA COULEUR POINTEE (07/09). Rodolf :
		//    << par defaut dans les color picker on doit avoir une pipette en plus >>.
		//
		// ⚠️ LE PIEGE DE CE LOT EST L'ESPACE DE COULEUR : ce qui est a l'ecran est
		//    encode pour l'affichage ; si le selecteur rangeait ses valeurs en
		//    lineaire, poser la valeur brute donnerait une couleur FAUSSE -- plus
		//    claire ou plus sombre que ce qu'on a pointe. Un essai qui preleve
		//    << une couleur >> ne le verrait pas. **Celui-ci pose un aplat de couleur
		//    CONNUE et exige EXACTEMENT cette couleur** : c'est la seule forme qui
		//    attrape une conversion manquante.
		{
			char det[420];
			nkgui::NkGuiDrawList dl;
			// Trois aplats voisins, et c'est le montage qui compte : avec un seul,
			// << la pipette rend le pixel voisin >> resterait vrai. Les couleurs sont
			// choisies DIFFERENTES sur chaque canal, sinon un decalage se cacherait.
			dl.PushClipRect({0.f, 0.f, 1.0e9f, 1.0e9f}, false);
			dl.AddRectFilled({0.f, 0.f, 40.f, 40.f}, nkgui::NkColor{247, 154, 40, 255}, 0.f);   // #f79a28
			dl.AddRectFilled({40.f, 0.f, 40.f, 40.f}, nkgui::NkColor{10, 85, 95, 255}, 0.f);    // #0a555f
			dl.AddRectFilled({0.f, 40.f, 80.f, 40.f}, nkgui::NkColor{25, 118, 210, 255}, 0.f);  // #1976d2
			dl.PopClipRect();

			char a[12] = {}, b[12] = {}, c[12] = {}, dehors[12] = {};
			const bool okA = NkPreleverPixel(dl, 20, 20, 0x000000FFu, a, (uint32)sizeof(a));
			const bool okB = NkPreleverPixel(dl, 60, 20, 0x000000FFu, b, (uint32)sizeof(b));
			const bool okC = NkPreleverPixel(dl, 20, 60, 0x000000FFu, c, (uint32)sizeof(c));
			// hors de tout aplat : c'est le SOL qui repond, et il repond exactement lui
			const bool okD = NkPreleverPixel(dl, 200, 200, 0x123456FFu, dehors, (uint32)sizeof(dehors));
			// ⚠️ DEUX POINTS SUR LA FRONTIERE, ET C'EST LE MONTAGE QUI COMPTE.
			//    Premiere ecriture : trois points au CENTRE d'aplats de 40 px. La
			//    mutation << la pipette rend le pixel voisin >> est restee VERTE -- a un
			//    pixel pres, on est encore dans le meme aplat. **CINQUIEME fois qu'un
			//    montage ne pose pas l'etat qu'il mesure.** Un prelevement au PREMIER
			//    pixel du second aplat (x = 40) et au premier du troisieme (y = 40) rend
			//    un voisin d'une AUTRE couleur : le decalage n'a plus ou se cacher, en x
			//    comme en y.
			char bx[12] = {}, by[12] = {};
			const bool okBX = NkPreleverPixel(dl, 40, 20, 0x000000FFu, bx, (uint32)sizeof(bx));
			const bool okBY = NkPreleverPixel(dl, 20, 40, 0x000000FFu, by, (uint32)sizeof(by));
			const bool bordX = okBX && NkComponentDecl::StrEq(bx, "#0a555f"); // x-1 serait #f79a28
			const bool bordY = okBY && NkComponentDecl::StrEq(by, "#1976d2"); // y-1 serait #f79a28

			const bool exactA = okA && NkComponentDecl::StrEq(a, "#f79a28");
			const bool exactB = okB && NkComponentDecl::StrEq(b, "#0a555f");
			const bool exactC = okC && NkComponentDecl::StrEq(c, "#1976d2");
			const bool exactD = okD && NkComponentDecl::StrEq(dehors, "#123456");
			// ⚠️ ET LES TROIS SONT DIFFERENTES : sans cette ligne, une pipette qui
			//    rendrait toujours la meme couleur passerait les trois assertions
			//    au-dessus si le montage etait uni.
			const bool distinctes = !NkComponentDecl::StrEq(a, b) && !NkComponentDecl::StrEq(b, c)
									&& !NkComponentDecl::StrEq(a, c);
			// UN POINT HORS FENETRE (negatif) est REFUSE, il ne rend pas du noir.
			char neg[12] = {};
			const bool refuseNeg = !NkPreleverPixel(dl, -3, 10, 0u, neg, (uint32)sizeof(neg));

			snprintf(det, sizeof(det),
					 "aplat orange -> %s (attendu #f79a28) ; petrole -> %s (#0a555f) ; bleu -> %s "
					 "(#1976d2) ; hors des aplats, le SOL -> %s (#123456) ; trois valeurs "
					 "distinctes -> %d ; point negatif REFUSE -> %d ; SUR LA FRONTIERE : "
					 "x=40 -> %s (#0a555f, le voisin serait #f79a28) et y=40 -> %s (#1976d2) -> %d",
					 okA ? a : "(refus)", okB ? b : "(refus)", okC ? c : "(refus)",
					 okD ? dehors : "(refus)", distinctes ? 1 : 0, refuseNeg ? 1 : 0,
					 okBX ? bx : "(refus)", okBY ? by : "(refus)", (bordX && bordY) ? 1 : 0);
			check("139. LA PIPETTE REND EXACTEMENT LA COULEUR POINTEE, pas << une couleur >> : trois aplats "
				  "VOISINS de couleurs connues, preleves chacun en son centre, rendent leur hexa a "
				  "l'octet pres -- ce qui prouve du meme coup qu'aucune conversion d'espace n'est "
				  "necessaire (le rasteriseur melange en octets et rend 0xRRGGBBAA, l'empaquetage exact "
				  "que le selecteur range dans #rrggbb). Le point hors des aplats rend le SOL pose, et "
				  "un point hors fenetre est REFUSE au lieu de rendre du noir. DEUX des points sont pris "
				  "SUR LA FRONTIERE de deux aplats -- sans eux, << la pipette rend le pixel voisin >> "
				  "resterait vrai a un pixel pres, et l'essai ne prouverait que << elle rend une "
				  "couleur >>",
				  exactA && exactB && exactC && exactD && distinctes && refuseNeg && bordX && bordY,
				  det);
		}
		// ── 94. ① L'APERCU PENDANT LE TRACE (05/09). Rodolf : « pourquoi quand on dessine un
		//    graphique on voit juste le rectangle qui s'allonge, et des qu'on relache on voit la
		//    forme ? » Deux mesures : LA TABLE DE GENRE (une seule, lue par le relachement et par
		//    l'apercu), et LES COMMANDES du peintre a mi-glisser -- la geometrie du genre choisi,
		//    pas quatre cotes.
		{
			char det[900];
			// 1. la table : chaque outil / variante rend le genre que la creation posera
			struct Cas {
					int32 outil, variante, varLigne, varImage;
					bool montante;
					const char *attendu;
					bool arrondiAttendu;
			};
			static const Cas kCas[] = {
				{1, 0, 0, 0, false, "frame", false},	  {2, 0, 0, 0, false, "rect", false},
				{2, 1, 0, 0, false, "rect", true},		  {2, 2, 0, 0, false, "ellipse", false},
				{2, 3, 0, 0, false, "triangle", false},	  {2, 4, 0, 0, false, "pentagone", false},
				{2, 5, 0, 0, false, "etoile", false},	  {3, 0, 0, 0, false, "line", false},
				{3, 0, 0, 0, true, "line_up", false},	  {3, 0, 1, 0, false, "fleche", false},
				{7, 0, 0, 0, false, "image", false},	  {7, 0, 0, 1, false, "avatar", false},
			};
			uint32 tableOk = 0u;
			char premierEcart[120];
			premierEcart[0] = 0;
			for (uint32 k = 0; k < sizeof(kCas) / sizeof(kCas[0]); ++k) {
				bool ar = false;
				const char *g = NkFormeDeLOutil(kCas[k].outil, kCas[k].variante, kCas[k].varLigne, kCas[k].varImage,
												kCas[k].montante, &ar);
				if (NkComponentDecl::StrEq(g, kCas[k].attendu) && ar == kCas[k].arrondiAttendu)
					++tableOk;
				else if (!premierEcart[0])
					snprintf(premierEcart, sizeof(premierEcart), "outil %d/%d -> « %s » (attendu « %s »)", kCas[k].outil,
							 kCas[k].variante, g, kCas[k].attendu);
			}
			// 2. les commandes de l'apercu, par genre, avec le MEME peintre que la toile
			nkentseu::editorkit::NkTheme th;
			NkDocumentHost hote;
			const NkPaintRect r{100.f, 100.f, 120.f, 80.f};
			auto compter = [&](const char *forme, bool arrondi, uint32 &poly, uint32 &sommetsMax, uint32 &ellipses,
							   uint32 &traits) {
				NkRecordingPaint rec;
				rec.Reset();
				NkDessinerApercuCreation(rec, r, forme, arrondi, th, hote);
				poly = 0u;
				sommetsMax = 0u;
				ellipses = 0u;
				traits = 0u;
				for (uint32 i = 0; i < (uint32)rec.cmds.Size(); ++i) {
					const NkPaintCmd &c = rec.cmds[i];
					if (c.op == NkPaintOp::Ellipse)
						++ellipses;
					else if (c.op == NkPaintOp::Line)
						++traits;
				}
				return (uint32)rec.cmds.Size();
			};
			// le peintre ENREGISTREUR ne sait pas le polygone (PolygonHex rend faux par defaut) :
			// on mesure ce qu'il SAIT -- l'ellipse, la ligne, le rectangle rempli -- et on compte
			// les commandes. Un peintre qui sait le polygone (la toile) dessine l'etoile.
			uint32 pE = 0u, sE = 0u, eE = 0u, tE = 0u;
			const uint32 nEllipse = compter("ellipse", false, pE, sE, eE, tE);
			uint32 pR = 0u, sR = 0u, eR = 0u, tR = 0u;
			const uint32 nRect = compter("rect", false, pR, sR, eR, tR);
			uint32 pL = 0u, sL = 0u, eL = 0u, tL = 0u;
			const uint32 nLigne = compter("line", false, pL, sL, eL, tL);
			uint32 pF = 0u, sF = 0u, eF = 0u, tF = 0u;
			const uint32 nFrame = compter("frame", false, pF, sF, eF, tF);
			// une ELLIPSE emet une commande d'ellipse ; un RECT n'en emet aucune ; une LIGNE emet
			// un trait ; un CADRE peint son fond d'artboard (plusieurs commandes, aucune ellipse)
			const bool formesDistinctes = eE >= 1u && eR == 0u && tL >= 1u && nRect >= 1u && nFrame >= 1u
										  && nEllipse >= 1u && nLigne >= 1u;
			// 3. LE RECT ARRONDI : le rayon voyage DANS la commande, pas dans leur nombre.
			//    (Mon attente etait fausse -- j'avais exige « plus de commandes » : quatre rayons
			//    EGAUX donnent UNE commande `FillColor` qui porte son `rounding`. La sonde mesure
			//    donc le rayon peint, 0 contre 8.)
			auto rayonPeint = [&](bool arrondi) {
				NkRecordingPaint rec;
				rec.Reset();
				NkDessinerApercuCreation(rec, r, "rect", arrondi, th, hote);
				float32 ray = -1.f;
				for (uint32 i = 0; i < (uint32)rec.cmds.Size(); ++i)
					if (rec.cmds[i].op == NkPaintOp::FillColor)
						ray = rec.cmds[i].rounding;
				return ray;
			};
			const float32 rayDroit = rayonPeint(false), rayArrondi = rayonPeint(true);
			const uint32 nArrondi = nRect;
			const bool arrondiVisible = rayDroit == 0.f && rayArrondi > 7.9f && rayArrondi < 8.1f;
			// 4. le COSTUME : le fond de l'apercu est celui que la creation posera (doc_field_bg)
			uint32 rgbaFond = 0u;
			{
				NkRecordingPaint rec;
				rec.Reset();
				NkDessinerApercuCreation(rec, r, "rect", false, th, hote);
				for (uint32 i = 0; i < (uint32)rec.cmds.Size(); ++i)
					if (rec.cmds[i].op == NkPaintOp::FillColor)
						rgbaFond = rec.cmds[i].rgba;
			}
			const NkString hexAttendu = NkHexDuRole(th, "doc_field_bg");
			const uint32 rgbaAttendu = renderdetail::NkGCouleur(hexAttendu.Data());
			const bool costumeOk = rgbaFond == rgbaAttendu;
			snprintf(det, sizeof(det),
					 "table : %u / %u cas justes%s%s ; commandes de l'apercu : ellipse %u (dont %u ellipse(s)), rect %u (0 ellipse : "
					 "%d), ligne %u (dont %u trait(s)), cadre %u ; rayon peint : droit %.1f, arrondi %.1f (attendu 8 ; %u commande, le "
					 "rayon voyage DANS elle) -> %d ; fond de l'apercu %08X = doc_field_bg %08X (%s) -> %d",
					 tableOk, (uint32)(sizeof(kCas) / sizeof(kCas[0])), premierEcart[0] ? " -- premier ecart : " : "",
					 premierEcart[0] ? premierEcart : "", nEllipse, eE, nRect, eR == 0u ? 1 : 0, nLigne, tL, nFrame, (double)rayDroit,
					 (double)rayArrondi, nArrondi, arrondiVisible ? 1 : 0, rgbaFond, rgbaAttendu, hexAttendu.Data(),
					 costumeOk ? 1 : 0);
			check("94. ① L'APERCU PENDANT LE TRACE PEINT LA FORME REELLE : une seule table de genre (douze cas : cadre, rect, arrondi, "
				  "ellipse, triangle, pentagone, etoile, ligne, ligne montante, fleche, image, avatar) lue par le relachement ET par "
				  "l'apercu ; le peintre emet la geometrie du genre (une ellipse pour l'ellipse, un trait pour la ligne, le RAYON de "
				  "8 px pour la variante arrondie), et le costume est celui que la creation posera",
				  tableOk == (uint32)(sizeof(kCas) / sizeof(kCas[0])) && formesDistinctes && arrondiVisible && costumeOk, det);
		}
		// ── 95. ③ L'OPACITE SUR SA PROPRE LIGNE (05/09, apres-midi). Rodolf : « est-ce possible
		//    que le pourcentage ait sa propre ligne avec une barre et un glisseur... et que cette
		//    barre ait un degrade de la transparence a la couleur pleine ? » Trois mesures : le
		//    curseur suit le MODELE, un glisser dans la barre ecrit le modele (et le champ le lit,
		//    puisqu'il n'y a qu'une valeur), et la barre porte bien un degrade d'alpha.
		{
			static nkgui::NkGuiContext ctxOp;
			char det[800];
			static nkgui::NkGuiFont policeOp;
			const bool policeOpOk = policeOp.LoadEmbedded(nkentseu::NkEmbeddedFontId::Inter, 14.f, false);
			if (!ctxOp.Init(600, 900) || !policeOpOk) {
				check("95. l'opacite sur sa propre ligne", false, "Init ou police a refuse");
			} else {
				ctxOp.font = &policeOp;
				static DesignState stOp;
				stOp.doc.NewDocument("Toile", NkAuthor::Humain);
				const int32 pgO = stOp.doc.AddChild(0, "", NkAuthor::Humain);
				stOp.doc.nodes[(uint32)pgO].shape = NkString("frame");
				stOp.doc.nodes[(uint32)pgO].layout.kind = NkLayoutKind::Free;
				stOp.doc.nodes[(uint32)pgO].width.mode = NkSizeMode::Fixed;
				stOp.doc.nodes[(uint32)pgO].width.value = 300.f;
				stOp.doc.nodes[(uint32)pgO].height.mode = NkSizeMode::Fixed;
				stOp.doc.nodes[(uint32)pgO].height.value = 300.f;
				const int32 rcO = stOp.doc.AddChild(pgO, "", NkAuthor::Humain);
				{
					NkUINode &n = stOp.doc.nodes[(uint32)rcO];
					n.shape = NkString("rect");
					n.posX = 100.f;
					n.posY = 100.f;
					n.width.mode = NkSizeMode::Fixed;
					n.width.value = 80.f;
					n.height.mode = NkSizeMode::Fixed;
					n.height.value = 80.f;
					NkRemplissage f;
					f.couleur = NkString("#1976d2");
					f.opacite = 100.f;
					n.fills.PushBack(f);
				}
				stOp.Recompute(NkPaintRect{0.f, 0.f, 340.f, 900.f});
				stOp.SelectSingle(rcO);
				static InspectorPanel inspOp(&stOp);
				NkEditorFrameContext ec;
				ec.ui = &ctxOp;
				ec.dt = 0.016f;
				auto image = [&](float32 mx, float32 my, bool bas) {
					ctxOp.input.mousePos = {mx, my};
					ctxOp.input.mouseDown[0] = bas;
					ctxOp.BeginFrame(0.016f);
					ctxOp.BeginLayout({340.f, 0.f, 260.f, 900.f});
					inspOp.OnUI(ec);
					NkDessinerPickerDemande(ctxOp, stOp);
					ctxOp.EndFrame();
				};
				stOp.picker = DesignState::DemandePicker();
				stOp.picker.ouvert = true;
				stOp.picker.id = ctxOp.GetId("##sonde.popover.alpha");
				stOp.picker.genre = 1u;
				stOp.picker.noeud = rcO;
				stOp.picker.index = 0;
				stOp.picker.ancre = {580.f, 200.f, 16.f, 16.f};
				for (int32 k = 0; k < 3; ++k)
					image(-1.f, -1.f, false);
				// 1. LE CURSEUR SUIT LE MODELE : a 100 %, il est au bout droit de la barre
				const nkgui::NkRect barre = inspOp.RectAlphaBarre();
				const nkgui::NkRect cur100 = inspOp.RectAlphaCurseur();
				const bool barreDessinee = barre.w > 40.f && barre.h > 8.f;
				const bool curseurADroite = barreDessinee && (cur100.x + cur100.w * 0.5f) > barre.x + barre.w - 1.f;
				// 2. LE MODELE CHANGE -> LE CURSEUR SUIT (aucune memoire propre au curseur)
				stOp.doc.nodes[(uint32)rcO].fills[0].opacite = 25.f;
				stOp.picker.synchro = 0xFFFFFFFFu;
				image(-1.f, -1.f, false);
				image(-1.f, -1.f, false);
				const nkgui::NkRect cur25 = inspOp.RectAlphaCurseur();
				const float32 t25 = barre.w > 0.f ? ((cur25.x + cur25.w * 0.5f) - barre.x) / barre.w : -1.f;
				const bool curseurSuit = t25 > 0.22f && t25 < 0.28f;
				// 3. UN GLISSER DANS LA BARRE ECRIT LE MODELE (a 75 %)
				const float32 xCible = barre.x + barre.w * 0.75f, yBarre = barre.y + barre.h * 0.5f;
				image(xCible, yBarre, false);
				image(xCible, yBarre, true);
				image(xCible, yBarre, true);
				image(xCible, yBarre, false);
				image(-1.f, -1.f, false);
				const float32 opApres = stOp.doc.nodes[(uint32)rcO].fills[0].opacite;
				const bool glisserEcrit = opApres > 73.f && opApres < 77.f;
				const nkgui::NkRect cur75 = inspOp.RectAlphaCurseur();
				const float32 t75 = barre.w > 0.f ? ((cur75.x + cur75.w * 0.5f) - barre.x) / barre.w : -1.f;
				const bool curseurApres = t75 > 0.72f && t75 < 0.78f;
				// 4. LA BARRE PORTE UN DEGRADE D'ALPHA : dans sa bande, des sommets de la couleur
				//    a des alphas differents (le damier est dessous, la couleur pleine au bout)
				uint32 alphaMin = 300u, alphaMax = 0u, nSommets = 0u, teinte = 0xFFFFFFFFu, teintesAutres = 0u;
				uint32 nBandeDl = 0u, nBandeOv = 0u;
				for (uint32 i = 0; i < (uint32)ctxOp.dl.vtx.Size(); ++i) {
					const nkgui::NkGuiVertex &vt = ctxOp.dl.vtx[i];
					if (vt.pos.y > barre.y && vt.pos.y < barre.y + barre.h && vt.pos.x >= barre.x && vt.pos.x <= barre.x + barre.w)
						++nBandeDl;
				}
				for (uint32 i = 0; i < (uint32)ctxOp.dlOverlay.vtx.Size(); ++i) {
					const nkgui::NkGuiVertex &vt = ctxOp.dlOverlay.vtx[i];
					if (vt.pos.y > barre.y && vt.pos.y < barre.y + barre.h && vt.pos.x >= barre.x && vt.pos.x <= barre.x + barre.w)
						++nBandeOv;
				}
				{
					const nkgui::NkGuiDrawList &dlO = nBandeOv >= nBandeDl ? ctxOp.dlOverlay : ctxOp.dl;
					for (uint32 i = 0; i < (uint32)dlO.vtx.Size(); ++i) {
						const nkgui::NkGuiVertex &vt = dlO.vtx[i];
						// ⚠️ LA FENETRE INCLUT LES BORDS : les rectangles des bandes ont leurs sommets
						//    EXACTEMENT sur `barre.y` et `barre.y + h` -- une fenetre strictement
						//    interieure ne voyait que le damier (mesure : 24 sommets d'une seule teinte
						//    opaque, zero degrade, alors que le degrade etait bien peint).
						if (vt.pos.y < barre.y - 0.5f || vt.pos.y > barre.y + barre.h + 0.5f)
							continue;
						if (vt.pos.x < barre.x || vt.pos.x > barre.x + barre.w)
							continue;
						const uint32 r8 = vt.col & 0xFFu, g8 = (vt.col >> 8) & 0xFFu, b8 = (vt.col >> 16) & 0xFFu;
						// ON EXCLUT LE DAMIER (deux gris exacts) ET LE CURSEUR (blanc opaque) : ce qui
						// reste est la bande de couleur. On ne vise PAS un hexa precis -- le popover
						// peint la couleur de son tampon, et une sonde qui exigerait #1976d2 mesurerait
						// le tampon plutot que le degrade (mesure : zero sommet a cette teinte).
						const bool damier = (r8 == 212u && g8 == 212u && b8 == 212u) || (r8 == 154u && g8 == 154u && b8 == 154u);
						const uint32 aTest = (vt.col >> 24) & 0xFFu;
						const bool curseur = r8 == 255u && g8 == 255u && b8 == 255u && aTest == 255u;
						if (damier || curseur)
							continue;
						// LA TEINTE DE LA BANDE : la premiere vue TRANSPARENTE (le degrade commence a
						// alpha ~0). Le bord de la barre et le contour du curseur passent aussi dans
						// cette fenetre : on les compte a part plutot que d'exiger une teinte unique.
						const uint32 rgb = (r8 << 16) | (g8 << 8) | b8;
						if (teinte == 0xFFFFFFFFu && aTest < 250u)
							teinte = rgb;
						if (teinte != 0xFFFFFFFFu && rgb != teinte) {
							++teintesAutres;
							continue;
						}
						const uint32 a8 = (vt.col >> 24) & 0xFFu;
						++nSommets;
						if (a8 < alphaMin)
							alphaMin = a8;
						if (a8 > alphaMax)
							alphaMax = a8;
					}
				}
				// la bande porte UNE teinte dont l'alpha va de ~0 a plein ; le bord de la barre et
				// le contour du curseur sont d'autres teintes, comptees et ignorees (dit)
				const bool degradeAlpha = nSommets > 20u && alphaMin < 40u && alphaMax > 240u;
				snprintf(det, sizeof(det),
						 "barre %.0f x %.0f (dessinee=%d) ; a 100 %% le curseur est au bout droit=%d ; modele pose a 25 %% -> curseur a "
						 "%.0f %% (suit=%d) ; glisser a 75 %% de la barre -> opacite %.1f (ecrit=%d), curseur a %.0f %% (=%d) ; degrade "
						 "d'alpha : %u sommets de la teinte %06X (%u sommets d'autres teintes : bord et curseur), alpha %u..%u -> %d [bande : %u en dl, %u en overlay]",
						 (double)barre.w, (double)barre.h, barreDessinee ? 1 : 0, curseurADroite ? 1 : 0, (double)(t25 * 100.f),
						 curseurSuit ? 1 : 0, (double)opApres, glisserEcrit ? 1 : 0, (double)(t75 * 100.f), curseurApres ? 1 : 0,
						 nSommets, teinte & 0xFFFFFFu, teintesAutres, alphaMin, alphaMax, degradeAlpha ? 1 : 0, nBandeDl, nBandeOv);
				check("95. ③ L'OPACITE A SA PROPRE LIGNE : une barre sur damier qui va de la couleur TRANSPARENTE a la couleur PLEINE, un "
					  "curseur qui suit LE MODELE (aucune memoire propre : le champ numerique et lui lisent la meme valeur), et un "
					  "glisser dans la barre ecrit l'opacite du remplissage",
					  barreDessinee && curseurADroite && curseurSuit && glisserEcrit && curseurApres && degradeAlpha, det);
				if (ctxOp.popupDepth > 0)
					ctxOp.ClosePopup();
				stOp.picker = DesignState::DemandePicker();
				image(-1.f, -1.f, false);
			}
		}
		// ── 96. ④ LE NOM DU FICHIER VIENT DE L'OBJET, ET TOUT S'EXPORTE (05/09, apres-midi).
		//    Rodolf : « le nom du fichier exporte doit etre celui de sa page. On peut tout
		//    exporter, pas seulement les pages : meme les graphiques, les groupes et leurs
		//    enfants. » Cinq mesures : le nom assaini, le nom PROPOSE (page / objet / « N
		//    objets » / suffixe d'echelle), un GROUPE qui emporte ses enfants dans les pixels,
		//    UN FICHIER PAR OBJET, et le doublon suffixe « (2) » plutot qu'ecrase en silence.
		{
			char det[900];
			// 1. L'ASSAINISSEMENT : les caracteres interdits de Windows deviennent « _ », les
			//    accents restent (le nom lu dans l'arbre doit rester lisible dans le dossier).
			char a1[64], a2[64], a3[64], a4[64];
			NkNomFichierAssaini("Bouton/Connexion", a1, sizeof(a1));
			NkNomFichierAssaini("  Page: \"Accueil\" ?", a2, sizeof(a2));
			NkNomFichierAssaini("", a3, sizeof(a3));
			NkNomFichierAssaini("Étoile 2", a4, sizeof(a4));
			const bool assainiOk = NkComponentDecl::StrEq(a1, "Bouton_Connexion")
								   && !NkString(a2).Contains(':') && !NkString(a2).Contains('?') && !NkString(a2).Contains('"')
								   && a2[0] == 'P' && NkComponentDecl::StrEq(a3, "sans_nom")
								   && NkString(a4).Contains("toile 2");
			// 2. LE DOCUMENT : une page nommee « Accueil », un carre, un groupe et son enfant bleu
			static DesignState st96;
			st96.doc.NewDocument("Export", NkAuthor::Humain);
			st96.cheminActif = NkString();
			st96.images.Vider();
			st96.sel.Clear();
			st96.selected = -1;
			st96.theme.Set(NkDesignResolveRole("artboard_bg"), 0xFFFFFFFFu);
			st96.theme.Set(NkDesignResolveRole("border"), 0x30363DFFu);
			const int32 pg96 = st96.doc.AddChild(0, "", NkAuthor::Humain);
			{
				NkUINode &p = st96.doc.nodes[(uint32)pg96];
				p.shape = NkString("frame");
				p.label = NkString("Accueil");
				p.layout.kind = NkLayoutKind::Free;
				p.width.mode = NkSizeMode::Fixed;
				p.width.value = 300.f;
				p.height.mode = NkSizeMode::Fixed;
				p.height.value = 200.f;
			}
			auto poser96 = [&](int32 parent, float32 x, float32 y, float32 w, float32 h, const char *nom,
							   const char *couleur) -> int32 {
				const int32 i = st96.doc.AddChild(parent, "", NkAuthor::Humain);
				NkUINode &n = st96.doc.nodes[(uint32)i];
				n.shape = NkString("rect");
				n.label = NkString(nom);
				n.posX = x;
				n.posY = y;
				n.width.mode = NkSizeMode::Fixed;
				n.width.value = w;
				n.height.mode = NkSizeMode::Fixed;
				n.height.value = h;
				n.layout.kind = NkLayoutKind::Free;
				if (couleur) {
					NkRemplissage f;
					f.couleur = NkString(couleur);
					n.fills.PushBack(f);
				}
				return i;
			};
			const int32 carre96 = poser96(pg96, 10.f, 10.f, 40.f, 40.f, "Carré rouge", "#ff0000");
			const int32 groupe96 = poser96(pg96, 100.f, 10.f, 80.f, 60.f, "Carte Actifs", "#00ff00");
			(void)poser96(groupe96, 10.f, 10.f, 30.f, 20.f, "Titre", "#0000ff");
			st96.Recompute(NkPaintRect{0.f, 0.f, 1400.f, 900.f});
			// 3. LE NOM PROPOSE : la page, l'objet, l'echelle en suffixe, la selection multiple
			char nPage[220], nObjet[220], nEch[220], nMulti[220];
			{
				NkExportOptions o;
				o.selection = false;
				st96.sel.Set(carre96);
				st96.selected = carre96;
				NkNomExportPropose(st96, o, nPage, sizeof(nPage)); // page MALGRE la selection
				o.selection = true;
				NkNomExportPropose(st96, o, nObjet, sizeof(nObjet));
				o.echelle = 2.f;
				NkNomExportPropose(st96, o, nEch, sizeof(nEch));
				o.echelle = 1.f;
				st96.sel.Add(groupe96);
				NkNomExportPropose(st96, o, nMulti, sizeof(nMulti));
			}
			const bool nomsOk = NkComponentDecl::StrEq(nPage, "Accueil.png")
								&& NkComponentDecl::StrEq(nObjet, "Carré rouge.png")
								&& NkComponentDecl::StrEq(nEch, "Carré rouge@2x.png")
								&& NkComponentDecl::StrEq(nMulti, "2 objets.png");
			// 4. UN GROUPE EMPORTE SES ENFANTS : la zone est celle du groupe (80 x 60) et le bleu
			//    de l'enfant est dans les pixels -- exporter un groupe n'est pas exporter sa boite.
			bool enfantSuit = false;
			int32 lGroupe = 0, hGroupe = 0;
			uint8 pxEnfant[4] = {0, 0, 0, 0};
			{
				st96.sel.Set(groupe96);
				st96.selected = groupe96;
				NkExportOptions o;
				o.selection = true;
				NkImage img;
				NkExportResultat r;
				if (NkExporterImage(st96, o, img, r) && img.Pixels()) {
					lGroupe = img.Width();
					hGroupe = img.Height();
					if (lGroupe > 20 && hGroupe > 20) {
						const uint8 *p = img.Pixels() + ((usize)20 * (usize)lGroupe + 20u) * 4u;
						for (int32 k = 0; k < 4; ++k)
							pxEnfant[k] = p[k];
						enfantSuit = lGroupe == 80 && hGroupe == 60 && pxEnfant[2] > 200u && pxEnfant[0] < 60u
									 && pxEnfant[1] < 60u;
					}
				}
			}
			// 5. UN FICHIER PAR OBJET, et le doublon suffixe : deux noeuds de MEME etiquette
			st96.doc.nodes[(uint32)carre96].label = NkString("sonde_par_objet");
			const int32 jumeau96 = poser96(pg96, 200.f, 10.f, 40.f, 40.f, "sonde_par_objet", "#ff00ff");
			st96.Recompute(NkPaintRect{0.f, 0.f, 1400.f, 900.f});
			NkFile::Delete("sonde_par_objet.png");
			NkFile::Delete("sonde_par_objet (2).png");
			st96.sel.Set(carre96);
			st96.sel.Add(jumeau96);
			st96.selected = carre96;
			NkString msg96;
			NkExportOptions oPar;
			oPar.selection = true;
			oPar.unFichierParObjet = true;
			const uint32 faits = NkExporterParObjet(st96, oPar, ".", msg96);
			const bool premier = NkFile::Exists("sonde_par_objet.png");
			const bool second = NkFile::Exists("sonde_par_objet (2).png");
			// les DEUX fichiers portent des pixels DIFFERENTS : un fichier par objet, pas deux
			// copies du meme (le defaut qu'un simple compte de fichiers ne verrait pas)
			bool deuxContenus = false;
			{
				NkImage u1, u2;
				if (premier && second && u1.Load("sonde_par_objet.png", 4) && u2.Load("sonde_par_objet (2).png", 4)
					&& u1.Pixels() && u2.Pixels() && u1.Width() > 4 && u2.Width() > 4) {
					const uint8 *p1 = u1.Pixels() + ((usize)4 * (usize)u1.Width() + 4u) * 4u;
					const uint8 *p2 = u2.Pixels() + ((usize)4 * (usize)u2.Width() + 4u) * 4u;
					deuxContenus = p1[0] > 200u && p1[1] < 60u && p1[2] < 60u  // le rouge
								   && p2[0] > 200u && p2[2] > 200u && p2[1] < 60u; // le magenta
				}
			}
			const bool parObjetOk = faits == 2u && premier && second && deuxContenus;
			// la selection est RENDUE telle quelle : l'export ne deplace pas le choix de Rodolf
			const bool selRendue = st96.sel.Count() == 2u && st96.sel.items[0] == carre96 && st96.selected == carre96;
			snprintf(det, sizeof(det),
					 "assaini : « %s », « %s », « %s », « %s » -> %d ; noms proposes : page « %s », objet « %s », x2 "
					 "« %s », multiple « %s » -> %d ; le GROUPE exporte %d x %d, pixel de l'enfant RGBA %u/%u/%u/%u -> %d ; un "
					 "fichier par objet : %u ecrit(s) [%s], doublon « (2) »=%d, contenus differents=%d -> %d ; selection rendue=%d",
					 a1, a2, a3, a4, assainiOk ? 1 : 0, nPage, nObjet, nEch, nMulti, nomsOk ? 1 : 0, lGroupe, hGroupe,
					 (unsigned)pxEnfant[0], (unsigned)pxEnfant[1], (unsigned)pxEnfant[2], (unsigned)pxEnfant[3],
					 enfantSuit ? 1 : 0, faits,
					 msg96.Data() ? msg96.Data() : "?", second ? 1 : 0, deuxContenus ? 1 : 0, parObjetOk ? 1 : 0,
					 selRendue ? 1 : 0);
			check("96. ④ L'EXPORT PORTE LE NOM DE L'OBJET ET TOUT S'EXPORTE : le nom est assaini (les caracteres interdits de "
				  "Windows deviennent « _ », les accents restent), il vient de la PAGE, de l'OBJET ou de « N objets », avec "
				  "« @2x » quand l'echelle change ; un GROUPE emporte ses enfants dans les pixels ; « un fichier par objet » ecrit "
				  "un fichier PAR racine choisie, aux contenus differents, et suffixe « (2) » un doublon plutot que de l'ecraser en "
				  "silence ; la selection est rendue telle quelle",
				  assainiOk && nomsOk && enfantSuit && parObjetOk && selRendue, det);
		}
		// ── 97. ② LE SELECTEUR DE FICHIERS A DEUX VOLETS (05/09, apres-midi). Rodolf :
		//    « un vrai selecteur de fichiers, comme sur Blender ou Windows ; NKCode en a un,
		//    regarde-le d'abord, et s'il est dans l'application, remonte-le dans NKEditorKit.
		//    L'ancien reste. »
		//
		//    MESURE ECRITE DANS LE DETAIL, parce qu'elle contredit la premisse : NKCode n'a
		//    PAS de selecteur a lui (`NkCodeDialogs : public NkFilePickerState`) -- il herite
		//    de celui du kit. Rien a remonter : c'etait deja monte. Ce qui manquait etait le
		//    SECOND VOLET, et le kit portait deja de quoi le faire (`NkDrawContentBrowser`).
		//    Cette sonde mesure donc les QUATRE choses que la reutilisation a exigees.
		{
			char det[900];
			// 1. LA VIGNETTE EST PEINTE. Avant ce lot, `NkAssetEntry::thumbnail` etait
			//    declaree et lue NULLE PART : le dessin ne peignait l'icone QUE si elle
			//    valait zero -- une entree AVEC vignette ne montrait donc RIEN.
			uint32 nImage = 0u, nIcone = 0u, imgHandle = 0u;
			uint32 nImageSans = 0u, nIconeSans = 0u;
			{
				// ⚠️ MISE A JOUR (05/09, nuit) : on comptait des commandes `Icon`. Depuis (2)(3)
				//    il n'y en a PLUS -- les natures sont des SILHOUETTES dessinees avec
				//    `FillColor`, jamais l'icone-carre du contrat. La sonde a rougi a ce
				//    changement, et c'est exactement ce qu'on lui demande : elle mesurait bien
				//    le dessin. On compte donc les FORMES de la silhouette.
				auto compter = [](const NkRecordingPaint &r, uint32 &img, uint32 &ico, uint32 &h) {
					img = ico = 0u;
					h = 0u;
					for (uint32 i = 0; i < (uint32)r.cmds.Size(); ++i) {
						if (r.cmds[i].op == NkPaintOp::Image) {
							++img;
							if (h == 0u)
								h = r.cmds[i].image;
						} else if (r.cmds[i].op == NkPaintOp::Icon
							   || (r.cmds[i].op == NkPaintOp::FillColor && r.cmds[i].w < 120.f))
							++ico; // une FORME de silhouette (ou l'ancienne icone-carre)
					}
				};
				NkContentBrowserModel m;
				NkAssetEntry a;
				a.name = NkString("photo.png");
				a.path = NkString("/photo.png");
				a.kindLabel = "PNG";
				a.thumbnail = 4242u; // LA poignee : on la retrouvera dans la commande
				m.entries.PushBack(a);
				const NkContentBrowserStyle sty = DemoStyle(nullptr);
				NkContentBrowserHooks h0;
				NkComponentInput in0;
				NkRecordingPaint avec;
				NkDrawContentBrowser(avec, in0, {0.f, 0.f, 640.f, 420.f}, m, sty, h0);
				compter(avec, nImage, nIcone, imgHandle);
				// LE CONTROLE NEGATIF, dans la meme course : sans vignette, l'icone revient
				m.entries[0].thumbnail = 0u;
				NkRecordingPaint sans;
				uint32 bidon = 0u;
				NkDrawContentBrowser(sans, in0, {0.f, 0.f, 640.f, 420.f}, m, sty, h0);
				compter(sans, nImageSans, nIconeSans, bidon);
			}
			fflush(stdout);
			fflush(stdout);
			fflush(stdout);
			const bool vignetteOk = nImage == 1u && imgHandle == 4242u && nImageSans == 0u
									&& nIconeSans > nIcone;
			// 2. LE VOLET SE TAIT : `show_header` / `show_actions` a 0 retirent la bande
			//    « Contenu » et les trois boutons, et `headerTitle` renomme la bande.
			uint32 nTexteMuet = 0u, nTexteBavard = 0u;
			bool titreVu = false, contenuVu = false;
			{
				NkContentBrowserModel m;
				m.headerTitle = NkString("Documents");
				NkAssetEntry a;
				a.name = NkString("x.png");
				a.kindLabel = "PNG";
				m.entries.PushBack(a);
				NkContentBrowserStyle sty = DemoStyle(nullptr);
				NkContentBrowserHooks h0;
				NkComponentInput in0;
				NkRecordingPaint bavard;
				NkDrawContentBrowser(bavard, in0, {0.f, 0.f, 640.f, 420.f}, m, sty, h0);
				for (uint32 i = 0; i < (uint32)bavard.cmds.Size(); ++i)
					if (bavard.cmds[i].op == NkPaintOp::Text) {
						++nTexteBavard;
						const char *t = bavard.cmds[i].text.Data();
						if (t && NkComponentDecl::StrEq(t, "Documents"))
							titreVu = true;
						if (t && NkComponentDecl::StrEq(t, "Contenu"))
							contenuVu = true;
					}
				NkComponentInstance inst(NkContentBrowserDecl());
				inst.SetParam("show_header", 0.f);
				inst.SetParam("show_actions", 0.f);
				sty.values = &inst;
				NkRecordingPaint muet;
				NkDrawContentBrowser(muet, in0, {0.f, 0.f, 640.f, 420.f}, m, sty, h0);
				for (uint32 i = 0; i < (uint32)muet.cmds.Size(); ++i)
					if (muet.cmds[i].op == NkPaintOp::Text)
						++nTexteMuet;
			}
			fflush(stdout);
			fflush(stdout);
			fflush(stdout);
			const bool muetOk = titreVu && !contenuVu && nTexteMuet + 4u <= nTexteBavard;
			// 3. LE FIL D'ARIANE PORTE SON INDEX. Avant, `onNavigate` ne donnait que le
			//    LIBELLE : deux segments homonymes (`src/nkgui/src`) le rendaient ambigu.
			int32 crumbVu = -2;
			{
				NkContentBrowserModel m;
				m.breadcrumb.PushBack(NkString("src"));
				m.breadcrumb.PushBack(NkString("nkgui"));
				m.breadcrumb.PushBack(NkString("src"));
				const NkContentBrowserStyle sty = DemoStyle(nullptr);
				NkContentBrowserHooks h0;
				NkComponentInput in0;
				NkRecordingPaint reperage;
				NkDrawContentBrowser(reperage, in0, {0.f, 0.f, 640.f, 420.f}, m, sty, h0);
				// la position de la TROISIEME miette se lit dans les commandes de texte
				float32 mx = -1.f, my = -1.f;
				uint32 vus = 0u;
				for (uint32 i = 0; i < (uint32)reperage.cmds.Size(); ++i) {
					const char *t = reperage.cmds[i].text.Data();
					if (reperage.cmds[i].op == NkPaintOp::Text && t && NkComponentDecl::StrEq(t, "src")) {
						++vus;
						if (vus == 2u) { // la seconde « src » = la miette d'index 2
							mx = reperage.cmds[i].x + 2.f;
							my = reperage.cmds[i].y + reperage.cmds[i].h * 0.5f;
						}
					}
				}
				if (mx > 0.f) {
					NkComponentInput clic;
					clic.mouseX = mx;
					clic.mouseY = my;
					clic.mousePressed = true;
					NkRecordingPaint r2;
					const NkContentBrowserResult res =
						NkDrawContentBrowser(r2, clic, {0.f, 0.f, 640.f, 420.f}, m, sty, h0);
					crumbVu = res.navigatedCrumb;
				}
			}
			fflush(stdout);
			fflush(stdout);
			fflush(stdout);
			const bool crumbOk = crumbVu == 2;
			// 4. LE SELECTEUR LUI-MEME, sur un VRAI dossier du disque.
			NkDirectory::Delete("sonde_selecteur", true);
			NkDirectory::CreateRecursive("sonde_selecteur/sous_dossier");
			NkFile::WriteAllText("sonde_selecteur/zeta.png", "x");
			NkFile::WriteAllText("sonde_selecteur/alpha.png", "x");
			NkFile::WriteAllText("sonde_selecteur/note.txt", "x");
			editorkit::NkFilePickerNavState nav;
			char bufNav[512] = {};
			const NkString abs = (NkPath(NkDirectory::GetCurrentDirectory()) / "sonde_selecteur").ToString();
			nav.OuvrirNav(editorkit::NkFilePickerState::PK_SaveFile, abs.Data(), ".png", "essai.png", bufNav,
						  (int32)sizeof(bufNav));
			nav.RelireDossier();
			// le dossier d'abord, puis les .png TRIES -- le .txt est filtre
			fflush(stdout);
			fflush(stdout);
			fflush(stdout);
			const uint32 nEnt = (uint32)nav.vue.entries.Size();
			// ⚠️ LES NOMS SE COPIENT ICI, pas dans le `snprintf` du bas : la sonde descend
			//    ensuite dans le sous-dossier, ce qui VIDE `entries`. Lire l'ancien index
			//    apres coup faisait sauter l'assertion de `NkVector` -- le defaut etait dans
			//    la MESURE, pas dans le code mesure, et c'est la sonde qui l'a montre.
			char nom0[64] = "-", nom1[64] = "-", nom2[64] = "-";
			if (nEnt > 0u)
				snprintf(nom0, sizeof(nom0), "%s", nav.vue.entries[0].name.Data());
			if (nEnt > 1u)
				snprintf(nom1, sizeof(nom1), "%s", nav.vue.entries[1].name.Data());
			if (nEnt > 2u)
				snprintf(nom2, sizeof(nom2), "%s", nav.vue.entries[2].name.Data());
			const bool listeOk =
				nEnt == 3u && nav.vue.entries[0].isFolder
				&& NkComponentDecl::StrEq(nav.vue.entries[0].name.Data(), "sous_dossier")
				&& NkComponentDecl::StrEq(nav.vue.entries[1].name.Data(), "alpha.png")
				&& NkComponentDecl::StrEq(nav.vue.entries[2].name.Data(), "zeta.png")
				&& NkComponentDecl::StrEq(nav.vue.entries[2].kindLabel, "PNG");
			// le fil d'Ariane et les chemins complets ont la MEME longueur, et la derniere
			// miette est le dossier courant : c'est ce qui rend `navigatedCrumb` exploitable
			const uint32 nCr = (uint32)nav.vue.breadcrumb.Size();
			const bool filOk = nCr > 1u && nCr == (uint32)nav.cheminsCrumb.Size()
							   && NkComponentDecl::StrEq(nav.vue.breadcrumb[nCr - 1u].Data(), "sonde_selecteur")
							   && NkDirectory::Exists(nav.cheminsCrumb[nCr - 1u].Data());
			// ⑤ (05/09, v5) LE RAIL NE MONTRE PLUS LE DOSSIER OU L'ON NAVIGUE : depuis que
			// le repli `courant = pickerPath` est retire, la section « Dossier courant » ne
			// porte QUE le dossier d'une operation REUSSIE (definition de Rodolf). Le temoin
			// fait donc ce que fait l'application apres une reussite -- il appelle la MEME
			// porte -- au lieu de compter sur un repli qui mentait.
			editorkit::NkFilePickerNavState::PoserRecent(nav.recents, nav.Dossier());
			nav.relire = true;
			nav.RelireDossier();
			// le rail porte le dossier courant, et il est ACTIF
			bool railOk = false;
			for (uint32 i = 0; i < (uint32)nav.vue.folders.nodes.Size() && !railOk; ++i)
				railOk = editorkit::NkFilePickerState::PathSame(nav.vue.folders.nodes[i].path.Data(),
																nav.Dossier())
						 && nav.vue.folders.active == nav.vue.folders.nodes[i].id;
			// ET LE CONTRAT DE CONFIRMATION EST LE MEME QUE L'ANCIEN : la preuve est que
			// le nouvel etat SE LIT comme un `NkFilePickerState` (pas un cousin qui lui
			// ressemble). Sans ca, chaque appelant aurait un second chemin a ecrire.
			editorkit::NkFilePickerState &commeAvant = nav;
			commeAvant.pickerConfirmed = true;
			const bool memeContrat = nav.pickerConfirmed
									 && NkComponentDecl::StrEq(nav.pickerSaveName, "essai.png")
									 && nav.pickerFor == editorkit::NkFilePickerState::PK_SaveFile;
			nav.pickerConfirmed = false;
			// on descend dans le sous-dossier, puis on remonte : la relecture suit
			const NkString sous = (NkPath(abs.Data()) / "sous_dossier").ToString();
			nav.AllerA(sous.Data());
			nav.RelireDossier();
			fflush(stdout);
			fflush(stdout);
			fflush(stdout);
			const bool descendu = nav.vue.entries.Empty()
								  && (uint32)nav.vue.breadcrumb.Size() == nCr + 1u;
			snprintf(det, sizeof(det),
					 "MESURE : NKCode n'a pas de selecteur a lui (NkCodeDialogs herite de NkFilePickerState) ; "
					 "vignette peinte : %u commande(s) Image (poignee %u) et %u icone(s) AVEC, %u / %u SANS -> %d ; "
					 "volet muet : titre « Documents »=%d, « Contenu »=%d, %u textes -> %u -> %d ; miette cliquee -> "
					 "index %d (attendu 2) -> %d ; dossier reel : %u entree(s) [%s, %s, %s] -> %d ; fil %u miettes = %u "
					 "chemins -> %d ; rail actif=%d ; meme contrat que l'ancien=%d ; descendu=%d",
					 nImage, imgHandle, nIcone, nImageSans, nIconeSans, vignetteOk ? 1 : 0, titreVu ? 1 : 0,
					 contenuVu ? 1 : 0, nTexteBavard, nTexteMuet, muetOk ? 1 : 0, crumbVu, crumbOk ? 1 : 0, nEnt,
					 nom0, nom1, nom2, listeOk ? 1 : 0, nCr,
					 (uint32)nav.cheminsCrumb.Size(), filOk ? 1 : 0, railOk ? 1 : 0, memeContrat ? 1 : 0,
					 descendu ? 1 : 0);
			check("97. ② LE SELECTEUR DE FICHIERS A DEUX VOLETS : le volet droit est LE navigateur de contenu du kit (aucun "
				  "quatrieme navigateur ecrit), et la reutilisation a paye trois dettes mesurees ici -- la VIGNETTE declaree "
				  "n'etait peinte par personne (une entree avec vignette ne montrait ni image ni icone), le FIL D'ARIANE ne "
				  "portait qu'un libelle ambigu, et la bande « Contenu » + les boutons Creer / Importer n'avaient aucun sens "
				  "dans un dialogue d'ouverture ; le nouvel etat SE LIT comme l'ancien `NkFilePickerState` (meme contrat de "
				  "confirmation), liste les dossiers d'abord, filtre par extension et suit la navigation",
				  vignetteOk && muetOk && crumbOk && listeOk && filOk && railOk && memeContrat && descendu, det);
			NkDirectory::Delete("sonde_selecteur", true);
		}
		// ── 98. ① LE SELECTEUR EST PLEIN DES LA PREMIERE IMAGE (05/09, soir). Sur la capture de
		//    Rodolf : chemin correct, mais « 0 element(s) », rail vide, fil d'Ariane absent --
		//    et tout apparaissait AU PREMIER GESTE. Cause mesuree : le dialogue d'export ouvre
		//    le selecteur par `OpenPickerBase` (la porte de la classe de BASE, celle des huit
		//    consommateurs), pas par `OuvrirNav` -- et c'etait `OuvrirNav` qui armait la
		//    lecture. Cette sonde ouvre DONC PAR LA PORTE DE BASE : c'est le seul chemin qui
		//    aurait vu le defaut.
		{
			char det[700];
			NkDirectory::Delete("sonde_premier_affichage", true);
			NkDirectory::CreateRecursive("sonde_premier_affichage/enfant");
			NkFile::WriteAllText("sonde_premier_affichage/a.png", "x");
			NkFile::WriteAllText("sonde_premier_affichage/b.png", "x");
			const NkString racine =
				(NkPath(NkDirectory::GetCurrentDirectory()) / "sonde_premier_affichage").ToString();
			editorkit::NkFilePickerNavState nav98;
			char buf98[512] = {};
			// LA PORTE DE BASE, exactement comme le dialogue d'export l'appelle
			nav98.OpenPickerBase(editorkit::NkFilePickerState::PK_SaveFile, racine.Data(), buf98,
						(int32)sizeof(buf98), nullptr, nullptr);
			// 1. AVANT tout dessin : l'etat DIT qu'il doit relire. Personne ne l'a arme.
			const bool ditRelire = nav98.DoitRelire();
			// 2. Ce que la premiere image ferait
			nav98.RelireDossier();
			const uint32 n98 = (uint32)nav98.vue.entries.Size();
			const uint32 crumb98 = (uint32)nav98.vue.breadcrumb.Size();
			const uint32 rail98 = (uint32)nav98.vue.folders.nodes.Size();
			const bool pleinDEmblee = n98 == 3u && crumb98 > 0u && rail98 > 0u;
			// 3. ET IL NE RELIT PAS EN BOUCLE : une fois la liste batie pour ce chemin,
			//    `DoitRelire` retombe a faux (sinon la fenetre listerait le disque a 60 Hz).
			const bool calme = !nav98.DoitRelire();
			// 4. N'IMPORTE QUEL APPELANT suffit : on ecrit `pickerPath` A LA MAIN, sans
			//    toucher a `relire` -- c'est ce qu'un futur appelant fera sans le savoir.
			const NkString enfant = (NkPath(racine.Data()) / "enfant").ToString();
			editorkit::NkFilePickerState::CopyTo(nav98.pickerPath, enfant.Data(),
						(int32)sizeof(nav98.pickerPath));
			const bool suitLeChemin = nav98.DoitRelire();
			nav98.RelireDossier();
			const bool vide = nav98.vue.entries.Empty() && !nav98.DoitRelire();
			// 5. CONTROLE NEGATIF : un chemin qui n'existe pas ne fait pas relire en boucle
			editorkit::NkFilePickerState::CopyTo(nav98.pickerPath, "Z:/nulle_part_du_tout",
						(int32)sizeof(nav98.pickerPath));
			nav98.RelireDossier();
			const bool pasDeBoucle = !nav98.DoitRelire() && nav98.vue.entries.Empty();
			snprintf(det, sizeof(det),
				"ouvert par la PORTE DE BASE (OpenPickerBase, sans OuvrirNav) : doit relire=%d ; premiere "
				"lecture -> %u entree(s), %u miette(s), %u ligne(s) de rail -> %d ; puis calme=%d ; "
				"`pickerPath` ecrit A LA MAIN -> doit relire=%d, relu vide=%d ; chemin inexistant : pas "
				"de relecture en boucle=%d",
				ditRelire ? 1 : 0, n98, crumb98, rail98, pleinDEmblee ? 1 : 0, calme ? 1 : 0,
				suitLeChemin ? 1 : 0, vide ? 1 : 0, pasDeBoucle ? 1 : 0);
			check("98. ① LE SELECTEUR EST PLEIN DES LA PREMIERE IMAGE, ET PERSONNE N'A RIEN A ARMER : ouvert par la porte de "
				"la classe de BASE (`OpenPickerBase`, celle qu'appelle le dialogue d'export et les huit consommateurs), il "
				"liste le dossier, le fil d'Ariane et le rail SANS le moindre geste ; la liste suit `pickerPath`, qui est "
				"desormais LE seul endroit ou vit le chemin -- ecrire ce champ a la main suffit a declencher la relecture, "
				"et une fois batie elle NE se refait pas a chaque image, meme sur un chemin inexistant",
				ditRelire && pleinDEmblee && calme && suitLeChemin && vide && pasDeBoucle, det);
			NkDirectory::Delete("sonde_premier_affichage", true);
		}
		// ── 99. ② LE RAIL A LA FORME DE L'EXPLORATEUR (05/09, soir). Rodolf, sur sa capture :
		//    « le rail ne montre ni les disques ni les dossiers principaux » -- ils y etaient,
		//    mais MELANGES a l'arborescence courante, sans titre ni separation. Quatre mesures :
		//    les sections existent et sont INSELECTIONNABLES, les dossiers usuels viennent DU
		//    SYSTEME (pas de `home + "/Desktop"`), les volumes portent leur etiquette, et le
		//    dossier courant est une section A LUI.
		{
			char det[800];
			editorkit::NkFilePickerNavState nav99;
			char buf99[512] = {};
			nav99.OpenPickerBase(editorkit::NkFilePickerState::PK_PickFolder,
					NkDirectory::GetCurrentDirectory().ToString().Data(), buf99,
					(int32)sizeof(buf99), nullptr, nullptr);
			// ⑤ (05/09, v5) LE RAIL NE MONTRE PLUS LE DOSSIER OU L'ON NAVIGUE : depuis que
			// le repli `courant = pickerPath` est retire, la section « Dossier courant » ne
			// porte QUE le dossier d'une operation REUSSIE (definition de Rodolf). Le temoin
			// fait donc ce que fait l'application apres une reussite -- il appelle la MEME
			// porte -- au lieu de compter sur un repli qui mentait.
			editorkit::NkFilePickerNavState::PoserRecent(nav99.recents, nav99.Dossier());
			nav99.relire = true;
			nav99.RelireDossier();
			const NkVector<editorkit::NkTreeNode> &rail = nav99.vue.folders.nodes;
			// 1. LES TROIS SECTIONS, titrees, racines, SANS chemin et `locked`
			int32 iRapide = -1, iPC = -1, iCourant = -1;
			for (uint32 i = 0; i < (uint32)rail.Size(); ++i) {
				const char *l = rail[i].label.Data();
				if (!l) continue;
				if (NkComponentDecl::StrEq(l, "Acc\u00e8s rapide")) iRapide = (int32)i;
				else if (NkComponentDecl::StrEq(l, "Ce PC")) iPC = (int32)i;
				else if (NkComponentDecl::StrEq(l, "Dossier courant")) iCourant = (int32)i;
			}
			const bool sectionsOk =
				iRapide >= 0 && iPC >= 0 && iCourant >= 0 && iRapide < iPC && iPC < iCourant
				&& rail[(uint32)iRapide].parent == -1 && rail[(uint32)iPC].parent == -1
				&& rail[(uint32)iCourant].parent == -1 && rail[(uint32)iRapide].locked
				&& rail[(uint32)iPC].locked && rail[(uint32)iCourant].locked
				&& rail[(uint32)iRapide].path.Empty() && rail[(uint32)iPC].path.Empty();
			// 2. LES DOSSIERS USUELS VIENNENT DU SYSTEME. Le temoin ne compare pas a une
			//    chaine ecrite ici (elle serait la MEME erreur des deux cotes) : il compare
			//    au chemin que `NkDirectory::GetUserFolder` rend, et verifie que le LIBELLE
			//    est le nom REEL du dossier -- « Bureau » sur un Windows francais.
			const NkString bureau = NkDirectory::GetUserFolder(NkDirectory::NkUserFolder::Desktop).ToString();
			const NkString docs = NkDirectory::GetUserFolder(NkDirectory::NkUserFolder::Documents).ToString();
			uint32 sousRapide = 0u;
			bool bureauVu = false, libelleReel = false;
			for (uint32 i = 0; i < (uint32)rail.Size(); ++i) {
				if (rail[i].parent != iRapide) continue;
				++sousRapide;
				if (!bureau.Empty() && editorkit::NkFilePickerState::PathSame(rail[i].path.Data(), bureau.Data())) {
					bureauVu = true;
					const NkString nm = NkPath(bureau.Data()).GetFileName();
					libelleReel = NkComponentDecl::StrEq(rail[i].label.Data(), nm.Data());
				}
			}
			const bool usuelsOk = sousRapide >= 2u && (bureau.Empty() || (bureauVu && libelleReel));
			// 3. LES VOLUMES : lus a l'execution, avec leur etiquette quand elle existe
			NkVector<NkDriveInfo> vols = NkFileSystem::GetDrives();
			uint32 prets = 0u;
			for (usize k = 0; k < vols.Size(); ++k)
				if (vols[k].IsReady) ++prets;
			uint32 sousPC = 0u;
			bool etiquetteVue = false;
			char premierVol[64] = "-";
			for (uint32 i = 0; i < (uint32)rail.Size(); ++i) {
				if (rail[i].parent != iPC) continue;
				if (sousPC == 0u) snprintf(premierVol, sizeof(premierVol), "%s", rail[i].label.Data());
				++sousPC;
				// une etiquette = un libelle plus long que la seule lettre (« D: Projets »)
				if (rail[i].label.Length() > 2u) etiquetteVue = true;
			}
			const bool volumesOk = sousPC > 0u && sousPC == (prets > 0u ? prets : 1u);
			// 4. LE DOSSIER COURANT EST UNE SECTION A LUI, et il y est ACTIF
			bool courantOk = false;
			for (uint32 i = 0; i < (uint32)rail.Size() && !courantOk; ++i)
				courantOk = editorkit::NkFilePickerState::PathSame(rail[i].path.Data(), nav99.Dossier())
					&& nav99.vue.folders.active == rail[i].id && rail[i].parent != -1;
			// 5. CONTROLE NEGATIF : un TITRE ne mene nulle part -- il n'a pas de chemin, donc
			//    la navigation du selecteur (qui exige `NkDirectory::Exists`) l'ignore.
			const bool titreInerte = iPC >= 0 && rail[(uint32)iPC].path.Empty()
					&& !NkDirectory::Exists(rail[(uint32)iPC].path.Data());
			snprintf(det, sizeof(det),
				"rail de %u ligne(s) ; sections « Acc\u00e8s rapide »=%d, « Ce PC »=%d, « Dossier courant »=%d, "
				"racines+locked+sans chemin -> %d ; acces rapide : %u entree(s), Bureau du systeme « %s » vu=%d, "
				"libelle = nom reel=%d -> %d ; Ce PC : %u volume(s) pose(s) pour %u pret(s) [premier : %s], "
				"etiquette vue=%d -> %d ; dossier courant en section et actif=%d ; titre inerte=%d",
				(uint32)rail.Size(), iRapide >= 0 ? 1 : 0, iPC >= 0 ? 1 : 0, iCourant >= 0 ? 1 : 0,
				sectionsOk ? 1 : 0, sousRapide, bureau.Empty() ? "(inconnu du systeme)" : bureau.Data(),
				bureauVu ? 1 : 0, libelleReel ? 1 : 0, usuelsOk ? 1 : 0, sousPC, prets, premierVol,
				etiquetteVue ? 1 : 0, volumesOk ? 1 : 0, courantOk ? 1 : 0, titreInerte ? 1 : 0);
			(void)docs;
			check("99. ② LE RAIL A LA FORME DE L'EXPLORATEUR : trois SECTIONS titrees et repliables -- « Acc\u00e8s rapide », "
				"« Ce PC », « Dossier courant » -- au lieu d'une liste plate ou disques et arborescence se melaient ; les "
				"titres sont des noeuds `locked` SANS chemin (le tree_view les rend inselectionnables sans une ligne dediee) ; "
				"les dossiers usuels viennent DU SYSTEME (`NkDirectory::GetUserFolder`) et portent leur nom REEL, pas une "
				"traduction de notre cru ; les volumes sont ceux que le systeme dit MONTES, avec leur etiquette",
				sectionsOk && usuelsOk && volumesOk && courantOk && titreInerte, det);
		}
		// ── 100. ③ LES DOSSIERS RECEMMENT OUVERTS (05/09, soir). Rodolf : « on doit aussi voir
		//    les dossiers recemment ouverts dans cette session ou ce document. » DEUX listes,
		//    et elles ne vivent pas au meme endroit : la SESSION est en memoire vive (elle
		//    meurt au lancement suivant), le DOCUMENT est ecrit dans le `.nkuidoc` et VOYAGE
		//    avec le fichier. La sonde mesure les deux, l'ordre, la deduplication, la section
		//    du rail -- et l'ALLER-RETOUR du document, qui est le seul temoin du voyage.
		{
			char det[820];
			NkDirectory::Delete("sonde_recents", true);
			NkDirectory::CreateRecursive("sonde_recents/un");
			NkDirectory::CreateRecursive("sonde_recents/deux");
			const NkString base100 =
				(NkPath(NkDirectory::GetCurrentDirectory()) / "sonde_recents").ToString();
			const NkString dUn = (NkPath(base100.Data()) / "un").ToString();
			const NkString dDeux = (NkPath(base100.Data()) / "deux").ToString();
			// 1. LE PLUS RECENT EN TETE, et reposer un dossier deja liste le fait REMONTER
			//    (une deduplication qui REJETTE ferait vieillir la liste a l'envers de son nom)
			NkVector<NkString> l100;
			editorkit::NkFilePickerNavState::PoserRecent(l100, dUn.Data());
			editorkit::NkFilePickerNavState::PoserRecent(l100, dDeux.Data());
			editorkit::NkFilePickerNavState::PoserRecent(l100, dUn.Data()); // deja la : il REMONTE
			const bool ordreOk = (uint32)l100.Size() == 2u
					&& NkComponentDecl::StrEq(l100[0].Data(), dUn.Data())
					&& NkComponentDecl::StrEq(l100[1].Data(), dDeux.Data());
			// 2. LES DEUX COTES : session ET document
			static DesignState st100;
			st100.doc.NewDocument("Recents", NkAuthor::Humain);
			st100.dossiersRecentsSession.Clear();
			st100.doc.dossiersRecents.Clear();
			st100.RetenirDossierRecent(dDeux.Data());
			st100.RetenirDossierRecent(dUn.Data());
			const bool deuxCotes = (uint32)st100.dossiersRecentsSession.Size() == 2u
					&& (uint32)st100.doc.dossiersRecents.Size() == 2u
					&& NkComponentDecl::StrEq(st100.dossiersRecentsSession[0].Data(), dUn.Data())
					&& NkComponentDecl::StrEq(st100.doc.dossiersRecents[0].Data(), dUn.Data());
			// 3. L'ALLER-RETOUR DU DOCUMENT : c'est le SEUL temoin qu'ils voyagent. Un
			//    champ rempli en memoire ne prouve rien sur ce que le fichier emporte.
			NkString texte100;
			st100.doc.Save(texte100);
			const bool ecritDansLeTexte = texte100.Contains("dossier_recent = ");
			NkUIDocument relu100;
			relu100.Load(texte100.Data());
			const bool allerRetour = (uint32)relu100.dossiersRecents.Size() == 2u
					&& NkComponentDecl::StrEq(relu100.dossiersRecents[0].Data(), dUn.Data())
					&& NkComponentDecl::StrEq(relu100.dossiersRecents[1].Data(), dDeux.Data());
			// 4. LA SESSION NE VOYAGE PAS : le document relu n'a pas d'idee de la session,
			//    et c'est la difference meme entre les deux listes.
			const NkVector<NkString> rail100 = st100.RecentsPourLeRail();
			const bool sessionEnTete = (uint32)rail100.Size() == 2u
					&& NkComponentDecl::StrEq(rail100[0].Data(), dUn.Data());
			// 5. LA SECTION « Recents » EXISTE ET EST EN TETE DU RAIL
			editorkit::NkFilePickerNavState nav100;
			char buf100[512] = {};
			nav100.recents = rail100;
			nav100.OpenPickerBase(editorkit::NkFilePickerState::PK_PickFolder, base100.Data(), buf100,
						(int32)sizeof(buf100), nullptr, nullptr);
			nav100.RelireDossier();
			int32 iRec = -1, iRapide2 = -1;
			uint32 sousRec = 0u;
			for (uint32 i = 0; i < (uint32)nav100.vue.folders.nodes.Size(); ++i) {
				const char *l = nav100.vue.folders.nodes[i].label.Data();
				if (l && NkComponentDecl::StrEq(l, "R\u00e9cents")) iRec = (int32)i;
				else if (l && NkComponentDecl::StrEq(l, "Acc\u00e8s rapide") && iRapide2 < 0) iRapide2 = (int32)i;
			}
			for (uint32 i = 0; i < (uint32)nav100.vue.folders.nodes.Size(); ++i)
				if (nav100.vue.folders.nodes[i].parent == iRec) ++sousRec;
			// ⚠️ MIS A JOUR (05/09, nuit) : « Recents » portait les DEUX. Depuis (6), LA
			//    TETE des recents EST le dossier courant au sens de Rodolf (« le dernier ou
			//    on a REUSSI a enregistrer ou charger ») et elle a SA section : « Recents »
			//    ne garde que la queue. L'afficher deux fois dirait deux fois la meme chose.
			const bool sectionOk = iRec == 0 && iRapide2 > iRec && sousRec == 1u;
			// ET LA TETE EST SOUS « Dossier courant » -- le fait nouveau que remplace l'ancien.
			bool teteSousCourant = false;
			{
				int32 iCourant2 = -1;
				for (uint32 i = 0; i < (uint32)nav100.vue.folders.nodes.Size(); ++i)
					if (NkComponentDecl::StrEq(nav100.vue.folders.nodes[i].label.Data(), "Dossier courant"))
						iCourant2 = (int32)i;
				for (uint32 i = 0; i < (uint32)nav100.vue.folders.nodes.Size(); ++i)
					if (nav100.vue.folders.nodes[i].parent == iCourant2
						&& editorkit::NkFilePickerState::PathSame(nav100.vue.folders.nodes[i].path.Data(),
																				  rail100[0].Data()))
						teteSousCourant = true;
			}
			// 6. CONTROLE NEGATIF : sans recent, AUCUNE section « Recents » -- un titre vide
			//    occupe une ligne et n'apprend rien.
			editorkit::NkFilePickerNavState nav101;
			char buf101[512] = {};
			nav101.OpenPickerBase(editorkit::NkFilePickerState::PK_PickFolder, base100.Data(), buf101,
						(int32)sizeof(buf101), nullptr, nullptr);
			nav101.RelireDossier();
			bool aucunTitreVide = true;
			for (uint32 i = 0; i < (uint32)nav101.vue.folders.nodes.Size(); ++i)
				if (NkComponentDecl::StrEq(nav101.vue.folders.nodes[i].label.Data(), "R\u00e9cents"))
					aucunTitreVide = false;
			snprintf(det, sizeof(det),
				"ordre (le plus recent en tete, un doublon REMONTE)=%d ; session %u / document %u, memes tetes -> %d ; "
				"`dossier_recent =` dans le texte=%d ; ALLER-RETOUR du .nkuidoc : %u relu(s) dans l'ordre -> %d ; "
				"rail : %u entree(s), session en tete=%d ; section « R\u00e9cents » a l'index %d (avant « Acc\u00e8s rapide » "
				"a %d) avec %u enfant(s) -> %d ; la TETE est sous « Dossier courant »=%d ; sans recent, "
			"aucun titre orphelin=%d",
				ordreOk ? 1 : 0, (uint32)st100.dossiersRecentsSession.Size(),
				(uint32)st100.doc.dossiersRecents.Size(), deuxCotes ? 1 : 0, ecritDansLeTexte ? 1 : 0,
				(uint32)relu100.dossiersRecents.Size(), allerRetour ? 1 : 0, (uint32)rail100.Size(),
				sessionEnTete ? 1 : 0, iRec, iRapide2, sousRec, sectionOk ? 1 : 0,
			teteSousCourant ? 1 : 0, aucunTitreVide ? 1 : 0);
			check("100. ③ LES DOSSIERS RECEMMENT OUVERTS, SESSION **ET** DOCUMENT : le plus recent en tete et un doublon "
				"REMONTE (une deduplication qui rejette ferait vieillir la liste a l'envers de son nom) ; les recents du "
				"DOCUMENT sont ecrits dans le `.nkuidoc` et relus dans l'ordre -- l'aller-retour est le seul temoin qu'ils "
				"voyagent avec le fichier ; ceux de la SESSION vivent en memoire et passent en tete du rail ; la section "
				"« R\u00e9cents » est la PREMIERE du rail, et n'existe pas du tout quand il n'y a rien a montrer",
				ordreOk && deuxCotes && ecritDansLeTexte && allerRetour && sessionEnTete && sectionOk
				&& teteSousCourant && aucunTitreVide,
				det);
			NkDirectory::Delete("sonde_recents", true);
		}
		// ── 101. ④ LE DIALOGUE NE MONTRE QUE CE QUE SON MODE EXIGE (05/09, soir). Sur la
		//    capture de Rodolf, le selecteur « choisir le dossier » affichait la bande
		//    « Contenu » et les boutons « Creer / Importer / Tout enregistrer » -- ceux du
		//    NAVIGATEUR D'ASSETS, herites parce qu'on reutilise son dessin. Il proposait donc
		//    des actions qu'il ne ferait pas.
		//
		//    LA SONDE MESURE LES TEXTES REELLEMENT EMIS, avec l'instance QUE LE DESSIN UTILISE
		//    (`NkInstanceVoletSelecteur` -- un seul site, nomme exprès pour que le temoin
		//    puisse l'appeler). ⚠️ CE QU'ELLE NE COUVRE PAS, et il faut le dire : que le dessin
		//    l'appelle vraiment. Il n'y a qu'un site et il porte ce nom ; c'est une garantie de
		//    lecture, pas de mesure.
		{
			char det[820];
			auto textesDe = [](const NkRecordingPaint &r, const char *quoi) -> uint32 {
				uint32 n = 0u;
				for (uint32 i = 0; i < (uint32)r.cmds.Size(); ++i)
					if (r.cmds[i].op == NkPaintOp::Text && r.cmds[i].text.Data()
						&& NkComponentDecl::StrEq(r.cmds[i].text.Data(), quoi))
						++n;
				return n;
			};
			NkContentBrowserModel m101;
			for (int32 k = 0; k < 3; ++k) {
				NkAssetEntry a;
				a.name = NkString(k == 0 ? "un.png" : (k == 1 ? "deux.png" : "trois.png"));
				a.kindLabel = "PNG";
				m101.entries.PushBack(a);
			}
			NkContentBrowserHooks h101;
			NkComponentInput in101;
			// A. LE NAVIGATEUR D'ASSETS (aucune instance) : il garde TOUT, c'est le controle
			//    positif -- sans lui, « rien n'est affiche » passerait pour un succes.
			NkContentBrowserStyle sAsset = DemoStyle(nullptr);
			NkRecordingPaint rAsset;
			NkDrawContentBrowser(rAsset, in101, {0.f, 0.f, 900.f, 560.f}, m101, sAsset, h101);
			const uint32 aContenu = textesDe(rAsset, "Contenu");
			const uint32 aCreer = textesDe(rAsset, "Cr\u00e9er");
			const uint32 aImporter = textesDe(rAsset, "Importer");
			const uint32 aTout = textesDe(rAsset, "Tout enregistrer");
			const uint32 aSelAll = textesDe(rAsset, "Tout s\u00e9lectionner");
			const bool assetGardeTout = aContenu == 1u && aCreer == 1u && aImporter == 1u
					&& aTout == 1u && aSelAll == 1u;
			// B. LE DIALOGUE D'ENREGISTREMENT / DE CHOIX DE DOSSIER : plus rien de tout ca
			NkContentBrowserStyle sSave = DemoStyle(nullptr);
			sSave.values = &editorkit::NkInstanceVoletSelecteur(false);
			NkRecordingPaint rSave;
			NkDrawContentBrowser(rSave, in101, {0.f, 0.f, 900.f, 560.f}, m101, sSave, h101);
			const uint32 sContenu = textesDe(rSave, "Contenu");
			const uint32 sCreer = textesDe(rSave, "Cr\u00e9er");
			const uint32 sImporter = textesDe(rSave, "Importer");
			const uint32 sTout = textesDe(rSave, "Tout enregistrer");
			const uint32 sSelAll = textesDe(rSave, "Tout s\u00e9lectionner");
			const bool dialogueMuet = sContenu == 0u && sCreer == 0u && sImporter == 0u
					&& sTout == 0u && sSelAll == 0u;
			// C. ...MAIS IL MONTRE ENCORE CE QU'IL DOIT : les fichiers et le tri.
			//    « Tout se tait » serait un succes a vide.
			// ⚠️ MIS A JOUR (05/09, nuit) : ce terme exigeait aussi « Trier par : Nom (a-z) ».
			//    Depuis (5), ce bouton se TAIT dans un dialogue -- le selecteur offre son
			//    propre combo (nom, date, taille, type, avec le sens), et deux commandes
			//    pour un reglage en font une de trop. L'anti-vacuite tient toujours : le
			//    dialogue montre SES FICHIERS, ce qui suffit a prouver qu'il n'a pas tout tu.
			const bool montreEncore = textesDe(rSave, "un.png") == 1u
					&& textesDe(rSave, "deux.png") == 1u;
			// ET LE TRI DU NAVIGATEUR EST BIEN ETEINT dans le dialogue, allume ailleurs.
			const bool triEteint = textesDe(rSave, "Trier par : Nom (a-z)") == 0u
					&& textesDe(rAsset, "Trier par : Nom (a-z)") == 1u;
			// D. EN OUVERTURE DE FICHIER, « Tout selectionner » REVIENT : le parametre suit le
			//    mode, il n'est pas eteint une fois pour toutes.
			NkContentBrowserStyle sOuvrir = DemoStyle(nullptr);
			sOuvrir.values = &editorkit::NkInstanceVoletSelecteur(true);
			NkRecordingPaint rOuvrir;
			NkDrawContentBrowser(rOuvrir, in101, {0.f, 0.f, 900.f, 560.f}, m101, sOuvrir, h101);
			const uint32 oSelAll = textesDe(rOuvrir, "Tout s\u00e9lectionner");
			const uint32 oCreer = textesDe(rOuvrir, "Cr\u00e9er");
			const bool suitLeMode = oSelAll == 1u && oCreer == 0u;
			snprintf(det, sizeof(det),
				"CONTROLE POSITIF (navigateur d'assets, sans instance) : « Contenu » %u, « Cr\u00e9er » %u, "
				"« Importer » %u, « Tout enregistrer » %u, « Tout s\u00e9lectionner » %u -> %d ; DIALOGUE "
				"(enregistrer / choisir un dossier) : %u / %u / %u / %u / %u -> %d ; il montre encore "
				"les fichiers et le tri=%d ; OUVERTURE DE FICHIER : « Tout s\u00e9lectionner » %u, « Cr\u00e9er » "
				"%u -> %d ; tri du navigateur eteint dans le dialogue, allume ailleurs=%d",
				aContenu, aCreer, aImporter, aTout, aSelAll, assetGardeTout ? 1 : 0, sContenu, sCreer,
				sImporter, sTout, sSelAll, dialogueMuet ? 1 : 0, montreEncore ? 1 : 0, oSelAll, oCreer,
				suitLeMode ? 1 : 0, triEteint ? 1 : 0);
			check("101. ④ LE DIALOGUE NE MONTRE QUE CE QUE SON MODE EXIGE : ni bande « Contenu », ni « Cr\u00e9er / Importer / "
				"Tout enregistrer », ni « Tout s\u00e9lectionner » dans un dialogue d'enregistrement ou de choix de dossier -- "
				"il proposait des actions qu'il ne ferait pas ; le NAVIGATEUR D'ASSETS les garde tous (controle positif, "
				"sans quoi « rien n'est affiche » passerait pour un succes), le dialogue montre TOUJOURS ses fichiers et "
				"son tri, et « Tout s\u00e9lectionner » REVIENT en ouverture de fichier -- le parametre suit le mode",
				assetGardeTout && dialogueMuet && montreEncore && suitLeMode && triEteint, det);
		}
		// ── 102. ⑤ LE RESULTAT EXPORTE SE VOIT (05/09, soir). Rodolf : « je pourrais vraiment
		//    avoir le resultat exporte une fois le dialogue traite. » Le pied de fenetre disait
		//    le chemin et disparaissait au geste suivant. Le bandeau RESTE, montre la VIGNETTE
		//    DU FICHIER RELU DU DISQUE (pas un rendu de plus : si le codec avait mal ecrit, la
		//    vignette le montrerait) et porte deux portes vers le systeme.
		{
			char det[800];
			static DesignState st102;
			st102.doc.NewDocument("Avis", NkAuthor::Humain);
			st102.images.Vider();
			// un televerseur factice : la sonde n'a pas de GPU, mais le cache doit rendre
			// une poignee NON NULLE pour que « il y a une vignette » veuille dire quelque chose
			static uint32 prochainHandle102 = 700u;
			st102.images.televerser = [](void *, const uint8 *, int32, int32) -> uint32 {
				return ++prochainHandle102;
			};
			st102.images.televerserUser = nullptr;
			// un VRAI png de 6 x 4, ecrit par le codec maison
			NkImage src102;
			bool ecrit102 = false;
			if (src102.Create(6u, 4u, math::NkColor(), 4) && src102.Pixels()) {
				for (usize k = 0; k < 6u * 4u; ++k) {
					src102.Pixels()[k * 4 + 0] = 200u;
					src102.Pixels()[k * 4 + 1] = 30u;
					src102.Pixels()[k * 4 + 2] = 40u;
					src102.Pixels()[k * 4 + 3] = 255u;
				}
				ecrit102 = src102.SavePNG("sonde_avis_export.png");
			}
			// 1. LE BANDEAU SE POSE, avec le NOM du fichier et ses DIMENSIONS
			st102.PoserAvisExport("sonde_avis_export.png", 1240, 620, "PNG");
			const bool poseOk = st102.avisExport.actif
					&& NkComponentDecl::StrEq(st102.avisExport.titre, "Export\u00e9 : sonde_avis_export.png")
					&& NkString(st102.avisExport.detail).Contains("1240")
					&& NkString(st102.avisExport.detail).Contains("620")
					&& NkString(st102.avisExport.detail).Contains("PNG")
					&& !st102.avisExport.chemin.Empty();
			// 2. LA VIGNETTE VIENT DU FICHIER RELU : 6 x 4, les dimensions du DISQUE et non
			//    celles qu'on a passees (1240 x 620). C'est ce qui prouve la relecture.
			const bool vignetteOk = st102.avisExport.vignette != 0u && st102.avisExport.vw == 6
					&& st102.avisExport.vh == 4;
			// 3. LA GEOMETRIE EST CALCULEE UNE FOIS, et le bouton peint EST le bouton
			//    cliquable : deux calculs separes ont deja coute cher sur ce chantier.
			const nkgui::NkRect reg102 = {0.f, 0.f, 1200.f, 700.f};
			const DesignState::NkGeomAvisExport g102 =
				DesignState::GeomAvisExport(reg102, 180.f, 90.f, true, false);
			auto dedans = [](const nkgui::NkRect &a, const nkgui::NkRect &b) {
				return b.x >= a.x && b.y >= a.y && b.x + b.w <= a.x + a.w && b.y + b.h <= a.y + a.h;
			};
			auto disjoints = [](const nkgui::NkRect &a, const nkgui::NkRect &b) {
				return a.x + a.w <= b.x || b.x + b.w <= a.x || a.y + a.h <= b.y || b.y + b.h <= a.y;
			};
			const bool geomOk = dedans(g102.cadre, g102.vignette) && dedans(g102.cadre, g102.dossier)
					&& dedans(g102.cadre, g102.fichier) && dedans(g102.cadre, g102.fermer)
					&& disjoints(g102.dossier, g102.fichier)
					&& disjoints(g102.fichier, g102.fermer)
					&& disjoints(g102.vignette, g102.dossier);
			// ── 3bis. ⑤ (06/09) LES DEUX BANDEAUX NE SE RECOUVRENT PAS ──────────────
			// ⚠️ CE QUE L'ESSAI 3 NE POUVAIT PAS VOIR, ET C'ETAIT LE DEFAUT REEL : il
			//    verifiait la geometrie du bandeau CONTRE ELLE-MEME. Les deux bandeaux
			//    etaient ancres au meme point ; l'avis court, peint AVANT, eteignait les
			//    deux entrees sur ses 28 px de haut, ou vivent les trois boutons du
			//    second. Les trois symptomes de Rodolf -- « ouvrir le dossier » sans
			//    effet, « ouvrir le fichier » sans effet, « ca ne se ferme pas » -- sont
			//    UN seul defaut, et il etait invisible a l'ecran (le second bandeau est
			//    peint par-dessus le premier).
			//    *Une geometrie juste chez elle peut etre fausse chez le voisin : un
			//    controle qui ne compare qu'a soi ne voit jamais une collision.*
			const nkgui::NkRect avis102 = DesignState::GeomBandeauAvis(reg102, 200.f);
			const DesignState::NkGeomAvisExport gPile =
				DesignState::GeomAvisExport(reg102, 180.f, 90.f, true, true);
			const bool pileOk = disjoints(avis102, gPile.cadre)
					&& disjoints(avis102, gPile.dossier) && disjoints(avis102, gPile.fichier)
					&& disjoints(avis102, gPile.fermer)
					&& gPile.cadre.y > g102.cadre.y; // il DESCEND, il ne se contente pas d'exister
			// 4. SANS VIGNETTE, LE BANDEAU RETRECIT au lieu de garder un trou de 40 px
			const DesignState::NkGeomAvisExport gSans =
				DesignState::GeomAvisExport(reg102, 180.f, 90.f, false, false);
			const bool retreci = gSans.cadre.w < g102.cadre.w && gSans.vignette.w == 0.f;
			// 5. LES DEUX PORTES PEUVENT ECHOUER, ET C'EST UN RESULTAT. On les appelle sur un
			//    chemin VIDE : elles doivent rendre FAUX sans rien ouvrir ni planter -- c'est
			//    la garantie qui permet au bandeau de dire « le chemin est en Console »
			//    plutot que de ne rien faire. ⚠️ On ne les appelle PAS sur un vrai chemin :
			//    une sonde qui ouvre l'explorateur de Rodolf serait une sonde qui nuit.
			const bool refusVide = !nkentseu::NkLauncher::OpenFile("") && !nkentseu::NkLauncher::RevealFile("")
					&& !nkentseu::NkLauncher::OpenFile(nullptr);
			// ── 5bis. ⑤ (06/09) LE CHEMIN QUI PART VERS LE SYSTEME ──────────────────
			// ⚠️ CE QUI N'ETAIT MESURABLE PAR RIEN, ET C'EST POUR CA QUE LA FONCTION EST
			//    PUBLIQUE : ce qui part vers `ShellExecute` n'est observable par aucun
			//    banc. Le depot manipule ses chemins en barres OBLIQUES ; `explorer.exe
			//    /select,"D:/a/b.png"` est une LIGNE DE COMMANDE que Windows n'analyse
			//    pas, et il ouvre « Documents » EN RENDANT UN SUCCES. C'est « ouvrir le
			//    dossier ouvre le mauvais dossier », mot pour mot.
			// ⚠️ ET LE CONTROLE NEGATIF COMPTE AUTANT : un chemin trop long est REFUSE,
			//    pas tronque -- un chemin tronque reste un chemin valide, et il designe
			//    autre chose. Un « succes » y serait pire qu'un echec.
			char natif[64] = {0};
			const bool convOk = nkentseu::NkLauncher::ToNativePath("D:/a/b c.png", natif, sizeof(natif))
#if defined(NKENTSEU_PLATFORM_WINDOWS)
					&& NkComponentDecl::StrEq(natif, "D:\\a\\b c.png");
#else
					&& NkComponentDecl::StrEq(natif, "D:/a/b c.png");
#endif
			char court[6] = {0};
			const bool refusLong = !nkentseu::NkLauncher::ToNativePath("D:/trop/long", court, sizeof(court))
					&& !nkentseu::NkLauncher::ToNativePath(nullptr, natif, sizeof(natif))
					&& !nkentseu::NkLauncher::ToNativePath("D:/x", nullptr, 8ull);
			// 6. LE CHEMIN RESTE LISIBLE quoi qu'il arrive : c'est le repli exige.
			st102.avisExport.echec = NkString("Le syst\u00e8me n'a pas pu ouvrir le dossier.");
			const bool cheminGarde = !st102.avisExport.chemin.Empty()
					&& NkString(st102.avisExport.chemin).Contains("sonde_avis_export.png");
			snprintf(det, sizeof(det),
				"png source ecrit=%d ; bandeau : « %s » / « %s » -> %d ; vignette poignee %u, %d x %d relus du "
				"DISQUE (et non 1240 x 620) -> %d ; geometrie : cadre %.0f x %.0f, boutons dedans et "
				"disjoints -> %d ; PILE : l'avis court occupe y %.0f..%.0f, le bandeau d'export descend a "
				"y %.0f (seul, il serait a %.0f) et aucun de ses boutons ne tombe dedans -> %d ; "
				"sans vignette le cadre passe de %.0f a %.0f -> %d ; portes systeme sur "
				"chemin vide : refus net=%d ; chemin garde=%d ; « D:/a/b c.png » part vers le systeme "
				"en « %s » -> %d, et un tampon trop court est REFUSE (pas tronque) -> %d",
				ecrit102 ? 1 : 0, st102.avisExport.titre, st102.avisExport.detail, poseOk ? 1 : 0,
				st102.avisExport.vignette, st102.avisExport.vw, st102.avisExport.vh, vignetteOk ? 1 : 0,
				(double)g102.cadre.w, (double)g102.cadre.h, geomOk ? 1 : 0,
				(double)avis102.y, (double)(avis102.y + avis102.h), (double)gPile.cadre.y,
				(double)g102.cadre.y, pileOk ? 1 : 0, (double)g102.cadre.w,
				(double)gSans.cadre.w, retreci ? 1 : 0, refusVide ? 1 : 0, cheminGarde ? 1 : 0,
				natif, convOk ? 1 : 0, refusLong ? 1 : 0);
			check("102. ⑤ LE RESULTAT EXPORTE SE VOIT : un bandeau qui RESTE (le pied de fenetre, lui, disparaissait au geste "
				"suivant) portant le NOM du fichier, ses DIMENSIONS, et la VIGNETTE DU FICHIER RELU DU DISQUE -- pas un "
				"rendu de plus, donc un codec qui aurait mal ecrit se verrait ; une SEULE geometrie pour le bouton peint et "
				"le bouton cliquable ; LES DEUX BANDEAUX NE SE RECOUVRENT PLUS (l'avis court mangeait le clic des trois "
				"boutons de celui-ci) ; LE CHEMIN PART DANS LA FORME QUE LE SYSTEME EXIGE (des barres obliques faisaient "
				"ouvrir le MAUVAIS dossier, avec un code de succes) ; et les deux portes vers le systeme rendent FAUX "
				"plutot que de mentir, le chemin restant lisible et copiable",
				ecrit102 && poseOk && vignetteOk && geomOk && pileOk && retreci && refusVide
					&& cheminGarde && convOk && refusLong, det);
			st102.images.televerser = nullptr;
		}
		// ── 103. ⑥ LE SELECTEUR EST L'OUTIL PAR DEFAUT, ET IL COUVRE LES QUATRE MODES
		//    (05/09, soir). Rodolf : « que ce soit pour creer un dossier, selectionner un
		//    dossier ou un fichier, pour ouvrir ou pour sauvegarder, il doit etre l'outil par
		//    defaut. Meme dans NK3DModeler. » Quatre mesures : le point d'entree par defaut
		//    resout SES roles tout seul (une application n'a plus de style a fournir), les
		//    quatre modes existent et se distinguent, CREER UN DOSSIER cree vraiment, et il
		//    refuse ce que le systeme refuserait au lieu de le corriger en douce.
		{
			char det[820];
			// 1. LE STYLE PAR DEFAUT : treize roles, tous RESOLUS. Un role non resolu peint
			//    en magenta -- c'est ce qui a deja coute 68 essais verts sur un ecran faux.
			const editorkit::NkFilePickerNavStyle sDef = editorkit::NkStyleSelecteurDefaut();
			const uint16 roles[] = {sDef.volet.panelBg,	 sDef.volet.headerBg,	 sDef.volet.border,
						sDef.volet.text,		 sDef.volet.textMuted,	 sDef.volet.cardBg,
						sDef.volet.cardFooterBg, sDef.volet.activeMark, sDef.volet.chosenMark,
						sDef.volet.folderTint,	 sDef.volet.chipBg,		 sDef.volet.badgeText,
						sDef.volet.statusBg};
			uint32 nResolus = 0u, nInvalides = 0u;
			for (usize k = 0; k < sizeof(roles) / sizeof(roles[0]); ++k) {
				if (roles[k] == NK_ROLE_INVALID)
					++nInvalides;
				else
					++nResolus;
			}
			const bool styleOk = nResolus == 13u && nInvalides == 0u
					&& sDef.volet.variant == NkBrowserVariant::Grid;
			// 2. LES QUATRE MODES. ⚠️ « Choisir un dossier » et « creer un dossier » sont LE
			//    MEME mode, et c'est un choix : creer PUIS choisir est un seul geste. La sonde
			//    le dit plutot que de compter quatre valeurs distinctes qui n'existent pas.
			const bool modesOk =
				editorkit::NkSelecteurOuvrirFichier == editorkit::NkFilePickerState::PK_File
				&& editorkit::NkSelecteurOuvrirDossier == editorkit::NkFilePickerState::PK_PickFolder
				&& editorkit::NkSelecteurCreerDossier == editorkit::NkSelecteurOuvrirDossier
				&& editorkit::NkSelecteurEnregistrer == editorkit::NkFilePickerState::PK_SaveFile
				&& editorkit::NkSelecteurOuvrirFichier != editorkit::NkSelecteurEnregistrer;
			// 3. CREER UN DOSSIER cree VRAIMENT, y descend, et vide le champ
			NkDirectory::Delete("sonde_creer", true);
			NkDirectory::CreateRecursive("sonde_creer");
			const NkString base103 = (NkPath(NkDirectory::GetCurrentDirectory()) / "sonde_creer").ToString();
			editorkit::NkFilePickerNavState nav103;
			char buf103[512] = {};
			nav103.OpenPickerBase(editorkit::NkSelecteurCreerDossier, base103.Data(), buf103,
						(int32)sizeof(buf103), nullptr, nullptr);
			nav103.RelireDossier();
			snprintf(nav103.nouveauNom, sizeof(nav103.nouveauNom), "%s", "Nouveau lot");
			const bool cree = nav103.CreerDossier();
			const NkString attendu = (NkPath(base103.Data()) / "Nouveau lot").ToString();
			const bool creeOk = cree && NkDirectory::Exists(attendu.Data())
					&& editorkit::NkFilePickerState::PathSame(nav103.Dossier(), attendu.Data())
					&& nav103.nouveauNom[0] == '\0';
			// 4. UN NOM QUE LE SYSTEME REFUSERAIT EST REFUSE, ET DIT -- pas assaini en douce :
			//    l'utilisateur doit reconnaitre le nom qu'il a tape (l'inverse du NOM DE
			//    FICHIER d'export, ou c'est NOUS qui proposons, pas lui qui tape).
			snprintf(nav103.nouveauNom, sizeof(nav103.nouveauNom), "%s", "a/b");
			const bool refuse = !nav103.CreerDossier() && !nav103.messageCreation.Empty()
					&& nav103.nouveauNom[0] != '\0'; // le nom tape RESTE dans le champ
			// ⚠️ LE MESSAGE SE FIGE ICI. Le lire a la fin du bloc afficherait celui de
			//    l'etape SUIVANTE sous l'etiquette de celle-ci -- deux faits, une seule
			//    variable : c'est le defaut de temoin deja paye par la sonde 60c.
			char msgRefus[160];
			snprintf(msgRefus, sizeof(msgRefus), "%s",
				 nav103.messageCreation.Data() ? nav103.messageCreation.Data() : "?");
			// 5. UN DOSSIER QUI EXISTE DEJA : on y entre, et on le DIT (pas d'ecrasement
			//    silencieux -- la meme regle que le ` (2)` de l'export).
			nav103.AllerA(base103.Data());
			snprintf(nav103.nouveauNom, sizeof(nav103.nouveauNom), "%s", "Nouveau lot");
			const bool deuxieme = nav103.CreerDossier();
			const bool ditDeja = deuxieme && nav103.messageCreation.Contains("existe")
					&& editorkit::NkFilePickerState::PathSame(nav103.Dossier(), attendu.Data());
			// 6. LE CONTRAT DE CONFIRMATION EST CELUI DE L'ANCIEN, dans les quatre modes :
			//    c'est ce qui rend la bascule d'une application possible en UNE LIGNE.
			bool contratOk = true;
			const int32 kModes[4] = {editorkit::NkSelecteurOuvrirFichier,
								 editorkit::NkSelecteurOuvrirDossier,
								 editorkit::NkSelecteurCreerDossier,
								 editorkit::NkSelecteurEnregistrer};
			for (int32 k = 0; k < 4 && contratOk; ++k) {
				editorkit::NkFilePickerNavState n;
				char b[512] = {};
				n.OuvrirNav(kModes[k], base103.Data(), ".png", "essai.png", b, (int32)sizeof(b));
				editorkit::NkFilePickerState &commeAvant = n;
				contratOk = n.pickerOpen && commeAvant.pickerFor == kModes[k]
					&& !commeAvant.pickerConfirmed && !commeAvant.pickerCancelled
					&& n.DoitRelire(); // plein des la premiere image, dans les QUATRE modes
			}
			snprintf(det, sizeof(det),
				"style par defaut : %u role(s) resolu(s), %u invalide(s), variante grille -> %d ; quatre modes "
				"(fichier %d, dossier %d, creer %d = dossier, enregistrer %d) -> %d ; « Nouveau lot » cree=%d, "
				"on y descend et le champ se vide -> %d ; « a/b » refuse et dit « %s », nom garde -> %d ; "
				"deuxieme creation : on y entre et on le dit -> %d ; contrat de confirmation identique dans "
				"les quatre modes=%d",
				nResolus, nInvalides, styleOk ? 1 : 0, (int32)editorkit::NkSelecteurOuvrirFichier,
				(int32)editorkit::NkSelecteurOuvrirDossier, (int32)editorkit::NkSelecteurCreerDossier,
				(int32)editorkit::NkSelecteurEnregistrer, modesOk ? 1 : 0, cree ? 1 : 0, creeOk ? 1 : 0,
				msgRefus, refuse ? 1 : 0,
				ditDeja ? 1 : 0, contratOk ? 1 : 0);
			check("103. ⑥ LE SELECTEUR EST L'OUTIL PAR DEFAUT DU KIT, ET IL COUVRE LES QUATRE MODES : `NkDrawSelecteur` "
				"resout SES treize roles depuis la declaration du composant -- une application n'a plus ni style ni role a "
				"fournir, sa bascule tient en UNE LIGNE par site ; ouvrir un fichier, ouvrir un dossier, CREER un dossier "
				"(qui cree vraiment, y descend, et REFUSE en le disant un nom que le systeme refuserait plutot que de "
				"l'assainir en douce) et enregistrer sous ; et le contrat de confirmation reste celui de l'ancien",
				styleOk && modesOk && creeOk && refuse && ditDeja && contratOk, det);
			NkDirectory::Delete("sonde_creer", true);
		}
		// ── 104. UNE SEULE PORTE OUVRE LE SELECTEUR D'EXPORT, ET ELLE LE PREPARE (05/09,
		//    soir). ⚠️ DEFAUT DE LA MEME FAMILLE QUE ①, trouve en relisant le chemin REEL
		//    apres l'avoir corrige : le dialogue d'export appelait `OpenPickerBase`
		//    directement. Depuis ① il heritait bien du CONTENU du dossier -- mais pas du
		//    filtre d'extension, ni des vignettes, ni des roles de theme, ni des recents,
		//    tous poses par l'AUTRE porte. Et c'est la porte du dialogue que Rodolf emprunte.
		//    La sonde mesure LES DEUX portes, et exige qu'elles preparent PAREIL.
		{
			char det[760];
			static DesignState st104;
			st104.doc.NewDocument("Portes", NkAuthor::Humain);
			st104.cheminActif = NkString();
			st104.dossiersRecentsSession.Clear();
			st104.doc.dossiersRecents.Clear();
			st104.RetenirDossierRecent(NkDirectory::GetCurrentDirectory().ToString().Data());
			auto prepare = [](const DesignState::NkChoixExport &c, const char *ext) {
				return c.picker.vignette != nullptr && c.picker.vignetteUser != nullptr
					&& c.picker.roleDossier != NK_ROLE_INVALID && c.picker.roleDossier != 0u
					&& c.picker.roleFichier != NK_ROLE_INVALID && c.picker.roleFichier != 0u
					&& !c.picker.recents.Empty()
					&& NkComponentDecl::StrEq(c.picker.pickerFileExt.Data(), ext);
			};
			// PORTE A : l'ancien menu (`NkOuvrirChoixExport`)
			st104.choixExport = DesignState::NkChoixExport();
			NkOuvrirChoixExport(st104, NkExportFormat::SVG, 1.f, false, false);
			const bool porteA = st104.choixExport.picker.pickerOpen && prepare(st104.choixExport, ".svg");
			char nomA[220];
			snprintf(nomA, sizeof(nomA), "%s", st104.choixExport.picker.pickerSaveName);
			// PORTE B : le dialogue (`NkOuvrirSelecteurExport`), celle de Ctrl+E
			st104.choixExport = DesignState::NkChoixExport();
			st104.choixExport.format = (int32)NkExportFormat::PNG;
			NkOuvrirSelecteurExport(st104, "mon dessin.png");
			const bool porteB = st104.choixExport.picker.pickerOpen && prepare(st104.choixExport, ".png");
			const bool nomTenu =
				NkComponentDecl::StrEq(st104.choixExport.picker.pickerSaveName, "mon dessin.png");
			// ET LES DEUX SONT PLEINES DES LA PREMIERE IMAGE (le contrat de ① tient pour les deux)
			const bool deuxPleines = st104.choixExport.picker.DoitRelire();
			// CONTROLE NEGATIF : un selecteur NEUF n'est prepare par personne -- sans ce
			// terme, « prepare » serait vrai par defaut et la sonde ne mesurerait rien.
			editorkit::NkFilePickerNavState neuf104;
			const bool neufNonPrepare = neuf104.vignette == nullptr && neuf104.recents.Empty()
					&& neuf104.pickerFileExt.Empty();
			snprintf(det, sizeof(det),
				"PORTE A (menu) : ouverte et preparee=%d, nom propose « %s » ; PORTE B (dialogue, Ctrl+E) : "
				"ouverte et preparee=%d, nom tenu « %s » -> %d ; pleine des la premiere image=%d ; "
				"CONTROLE NEGATIF : un selecteur neuf n'est prepare par personne=%d",
				porteA ? 1 : 0, nomA, porteB ? 1 : 0, st104.choixExport.picker.pickerSaveName,
				nomTenu ? 1 : 0, deuxPleines ? 1 : 0, neufNonPrepare ? 1 : 0);
			check("104. UNE SEULE PORTE OUVRE LE SELECTEUR D'EXPORT, ET ELLE LE PREPARE : les DEUX chemins -- l'ancien menu "
				"et le dialogue de Ctrl+E -- posent le filtre d'extension conforme au format, la fonction de vignette, les "
				"roles de theme et les recents ; le dialogue garde LE NOM que l'utilisateur a saisi ; et un selecteur neuf "
				"n'est prepare par personne (sans ce controle negatif, « prepare » serait vrai par defaut)",
				porteA && porteB && nomTenu && deuxPleines && neufNonPrepare, det);
			st104.choixExport = DesignState::NkChoixExport();
		}
		// ── 105. ① CE QUE LE RAIL PEINT VRAIMENT, EN MODE ENREGISTREMENT (05/09, nuit).
		//    Rodolf : « le rail n'a aucune de tes trois sections » -- et la sonde 99, qui
		//    mesurait LE MODELE, etait verte. Une sonde sur le modele ne dit rien du dessin :
		//    c'est la lecon de la famille « declarer n'est pas livrer », appliquee a moi-meme.
		//    Celle-ci DESSINE, a la largeur REELLE du dialogue (860 de large, `tree_width`
		//    fraction de la declaration), avec l'instance que le selecteur utilise, et lit les
		//    textes emis dans la COLONNE DE GAUCHE.
		{
			char det[900];
			editorkit::NkFilePickerNavState nav105;
			char buf105[512] = {};
			nav105.OpenPickerBase(editorkit::NkSelecteurEnregistrer,
					NkDirectory::GetCurrentDirectory().ToString().Data(), buf105,
					(int32)sizeof(buf105), nullptr, nullptr);
			nav105.RelireDossier();
			// LE MODELE : ce que la sonde 99 mesure deja
			uint32 sectionsModele = 0u;
			for (uint32 i = 0; i < (uint32)nav105.vue.folders.nodes.Size(); ++i)
				if (nav105.vue.folders.nodes[i].parent == -1 && nav105.vue.folders.nodes[i].locked)
					++sectionsModele;
			// LA PROFONDEUR DU RAIL, lue dans le modele. C'est ELLE la cause des deux
			// symptomes de la capture -- les libelles reduits a « ... » et les sections
			// repoussees hors du champ -- et, contrairement au nombre de rangees peintes,
			// elle ne depend pas du rognage : `NkRecordingPaint` enregistre TOUT, y compris
			// ce qui tombe hors de la zone visible. Une premiere version de cette sonde
			// comptait les rangees peintes et restait VERTE sous la mutation.
			uint32 profondeurRail = 0u;
			for (uint32 i = 0; i < (uint32)nav105.vue.folders.nodes.Size(); ++i) {
				uint32 d = 0u;
				for (int32 a = nav105.vue.folders.nodes[i].parent; a >= 0;
					 a = nav105.vue.folders.nodes[(uint32)a].parent)
					++d;
				if (d > profondeurRail)
					profondeurRail = d;
			}
			// LE NOMBRE DE RANGEES AVANT la premiere entree du dossier courant : ce sont
			// celles qu'il faut avoir depassees pour voir quoi que ce soit d'utile.
			uint32 rangeesAvantCourant = 0u;
			for (uint32 i = 0; i < (uint32)nav105.vue.folders.nodes.Size(); ++i) {
				if (NkComponentDecl::StrEq(nav105.vue.folders.nodes[i].label.Data(), "Dossier courant"))
					break;
				++rangeesAvantCourant;
			}
			// LE DESSIN : la meme zone que le dialogue reel
			const NkPaintRect zone105{0.f, 0.f, 860.f, 470.f};
			NkContentBrowserStyle sty105 = DemoStyle(nullptr);
			sty105.values = &editorkit::NkInstanceVoletSelecteur(false);
			NkContentBrowserHooks h105;
			NkComponentInput in105;
			NkRecordingPaint r105;
			NkDrawContentBrowser(r105, in105, zone105, nav105.vue, sty105, h105);
			// la colonne de gauche : `tree_width` de la declaration, en fraction
			const float32 largeurRail = zone105.w * NkContentBrowserDecl().Param("tree_width");
			// le haut du CORPS : sous la bande de tete (tue), la barre d'outils, les puces
			// et la rangee d'information -- tous des nombres de la DECLARATION.
			const float32 hautCorps = NkContentBrowserDecl().Metric("toolbar_h")
						 + NkContentBrowserDecl().Metric("filter_h")
						 + NkContentBrowserDecl().Metric("info_h");
			uint32 sectionsPeintes = 0u, textesRail = 0u, videsRail = 0u;
			char premiers[220];
			premiers[0] = '\0';
			for (uint32 i = 0; i < (uint32)r105.cmds.Size(); ++i) {
				const NkPaintCmd &c = r105.cmds[i];
				// ⚠️ BORNER EN Y AUSSI. Depuis (4) les boutons d'action se taisent, et le FIL
			//    D'ARIANE commence donc tout a gauche -- dans la fenetre en x de la colonne.
			//    Une premiere version de cette sonde comptait ses miettes comme des lignes
			//    de rail : elle mesurait deux choses sous un seul nom.
			if (c.op != NkPaintOp::Text || c.x >= largeurRail || c.y < hautCorps)
					continue;
				const char *t = c.text.Data();
				++textesRail;
				if (!t || !t[0])
					++videsRail;
				else if (NkComponentDecl::StrEq(t, "R\u00e9cents") || NkComponentDecl::StrEq(t, "Acc\u00e8s rapide")
					 || NkComponentDecl::StrEq(t, "Ce PC") || NkComponentDecl::StrEq(t, "Dossier courant"))
					++sectionsPeintes;
				if (textesRail <= 5u && t && t[0]) {
					const usize n = NkString(premiers).Length();
					snprintf(premiers + n, sizeof(premiers) - n, "%s%s", n ? " | " : "", t);
				}
			}
			// LA LARGEUR UTILE AU PLUS PROFOND : c'est elle qui produit les « ... »
			uint32 profMax = 0u;
			{
				// profondeur = position x du texte, en pas d'indentation
				float32 xMax = 0.f;
				for (uint32 i = 0; i < (uint32)r105.cmds.Size(); ++i) {
					const NkPaintCmd &c = r105.cmds[i];
					if (c.op == NkPaintOp::Text && c.x < largeurRail && c.x > xMax)
						xMax = c.x;
				}
				profMax = (uint32)(largeurRail - xMax > 0.f ? largeurRail - xMax : 0.f);
			}
			const bool sectionsAuDessin = sectionsPeintes >= 3u
					&& NkString(premiers).StartsWith("Acc\u00e8s rapide");
			// ≤ 2 : la section, le dossier, ses enfants. Au-dela, l'indentation mange le nom.
			const bool profondeurTenue = profondeurRail <= 2u;
			const bool aucunVide = videsRail == 0u;
			// ⚠️ LA LARGEUR N'EST PAS JUGEE ICI : elle est le sujet de (2) et de la sonde
			//    106. Cette sonde la MESURE et l'imprime -- une sonde qui exigerait deux
			//    corrections a la fois ne pourrait pas etre verte entre les deux commits.
			const uint32 largeurUtile = profMax;
			snprintf(det, sizeof(det),
				"MODELE : %u section(s) racine `locked` ; DESSIN a la largeur reelle (rail %.0f px sur %.0f) : "
				"%u texte(s) dans la colonne, dont %u section(s) et %u VIDE(s) ; cinq premiers : %s ; largeur "
				"utile au plus profond : %u px (jugee par (2), sonde 106) ; PROFONDEUR du rail : %u (≤ 2 exige), "
				"%u rangee(s) avant le dossier courant -> sections dessinees et en tete=%d, aucun libelle vide=%d, "
				"profondeur tenue=%d",
				sectionsModele, (double)largeurRail, (double)zone105.w, textesRail, sectionsPeintes, videsRail,
				premiers[0] ? premiers : "(aucun)", largeurUtile, profondeurRail, rangeesAvantCourant,
				sectionsAuDessin ? 1 : 0, aucunVide ? 1 : 0, profondeurTenue ? 1 : 0);
			check("105. ① LE RAIL PEINT SES SECTIONS, ET AUCUN LIBELLE VIDE : la sonde 99 mesurait LE MODELE et etait verte "
				"pendant que Rodolf ne voyait aucune section -- une mesure du modele ne dit rien du dessin. Celle-ci DESSINE "
				"a la largeur reelle du dialogue, avec l'instance que le selecteur utilise, et exige : les trois sections "
				"PEINTES dans la colonne de gauche ET EN TETE (la chaine d'ancetres les repoussait au-dela des onze "
				"rangees visibles), AUCUN libelle vide, et une PROFONDEUR d'au plus 2 -- les « ... » de la capture "
				"etaient des noms a la profondeur 6, ou l'indentation ne laisse que dix pixels",
				sectionsAuDessin && aucunVide && profondeurTenue, det);
		}
		// ── 106. ④ LA CARTE SELECTIONNEE NE S'EFFACE PLUS ELLE-MEME (05/09, nuit). Sur la
		//    capture de Rodolf : une vignette VIDE au milieu de la grille, cadre bleu, sans
		//    libelle -- et son nom (« docs ») affiche dans la BARRE D'ETAT, c'est-a-dire
		//    ailleurs. « L'element selectionne s'affiche a un endroit et son nom a un autre. »
		//    Cause : le marquage de la carte active appelait `Outline`, dont le CONTRAT dit
		//    « plein puis creusement d'un pixel » -- il repeint tout le rectangle avant de
		//    creuser. Appele APRES le contenu, il l'effacait.
		//
		//    ⚠️ LE TEMOIN DOIT REGARDER L'ORDRE, pas la presence : la commande de texte
		//       existait dans le flux, elle etait simplement RECOUVERTE ensuite. Une sonde
		//       qui compte les textes serait restee verte -- c'est la faute que la sonde 105
		//       a deja payee dans ce meme lot.
		{
			char det[820];
			NkContentBrowserModel m106;
			static const char *kNoms106[] = {"alpha", "docs", "omega"};
			for (int32 k = 0; k < 3; ++k) {
				NkAssetEntry a;
				a.name = NkString(kNoms106[k]);
				a.isFolder = true;
				a.kindLabel = "";
				m106.entries.PushBack(a);
			}
			m106.active = 1; // « docs », comme sur la capture
			const NkContentBrowserStyle sty106 = DemoStyle(nullptr);
			NkContentBrowserHooks h106;
			NkComponentInput in106;
			NkRecordingPaint r106;
			NkDrawContentBrowser(r106, in106, {0.f, 0.f, 900.f, 560.f}, m106, sty106, h106);
			// 1. le libelle de la carte ACTIVE est emis, et on retient OU et QUAND
			int32 iTexte = -1;
			float32 tx = 0.f, ty = 0.f;
			for (uint32 i = 0; i < (uint32)r106.cmds.Size(); ++i)
				if (r106.cmds[i].op == NkPaintOp::Text && r106.cmds[i].text.Data()
					&& NkComponentDecl::StrEq(r106.cmds[i].text.Data(), "docs")) {
					iTexte = (int32)i;
					tx = r106.cmds[i].x + r106.cmds[i].w * 0.5f;
					ty = r106.cmds[i].y + r106.cmds[i].h * 0.5f;
					break;
				}
			const bool libelleEmis = iTexte >= 0;
			// 2. ET RIEN D'OPAQUE NE PASSE DESSUS APRES. C'est LA mesure : on cherche une
			//    commande de remplissage POSTERIEURE qui couvre le point du libelle.
			int32 iRecouvrement = -1;
			if (libelleEmis)
				for (uint32 i = (uint32)iTexte + 1u; i < (uint32)r106.cmds.Size(); ++i) {
					const NkPaintCmd &c = r106.cmds[i];
					// ⚠️ `Outline` COMPTE COMME UN APLAT, et c'est tout le sujet : son contrat
					//    est « plein PUIS creusement d'un pixel ». Ne compter que `Fill` laissait
					//    la sonde aveugle au coupable exact -- une premiere version l'a fait.
					if (c.op != NkPaintOp::Fill && c.op != NkPaintOp::FillColor
						&& c.op != NkPaintOp::Outline)
						continue;
					if (tx >= c.x && tx <= c.x + c.w && ty >= c.y && ty <= c.y + c.h
						&& c.w >= 20.f && c.h >= 20.f) { // un APLAT, pas un liseré
						iRecouvrement = (int32)i;
						break;
					}
				}
			const bool pasRecouvert = libelleEmis && iRecouvrement < 0;
			// 3. LE CONTOUR EXISTE QUAND MEME : l'anneau de la carte active est peint. Sans
			//    ce terme, supprimer purement le marquage rendrait la sonde verte -- et la
			//    selection deviendrait invisible.
			// Un contour a ANGLES VIFS au role `active_mark` : c'est la seule primitive du
			// contrat qui ne repeint pas le fond (`NkComponentPaint.h` le dit ainsi).
			uint32 anneaux = 0u;
			for (uint32 i = 0; i < (uint32)r106.cmds.Size(); ++i)
				if (r106.cmds[i].op == NkPaintOp::OutlineSharp && r106.cmds[i].role == sty106.activeMark)
					++anneaux;
			const bool anneauPeint = anneaux >= 1u;
			// 4. CONTROLE NEGATIF : les cartes NON actives n'ont jamais eu ce probleme
			bool voisinsIntacts = true;
			for (int32 k = 0; k < 3 && voisinsIntacts; ++k) {
				if (k == 1)
					continue;
				bool vu = false;
				for (uint32 i = 0; i < (uint32)r106.cmds.Size() && !vu; ++i)
					vu = r106.cmds[i].op == NkPaintOp::Text && r106.cmds[i].text.Data()
						&& NkComponentDecl::StrEq(r106.cmds[i].text.Data(), kNoms106[k]);
				voisinsIntacts = vu;
			}
			snprintf(det, sizeof(det),
				"libelle « docs » de la carte ACTIVE emis a la commande %d, au point (%.0f, %.0f) -> %d ; "
				"premiere commande d'APLAT posterieure qui le recouvre : %d (aucune attendue) -> %d ; anneau "
				"creux peint : %u -> %d ; les deux cartes non actives ont leur libelle=%d",
				iTexte, (double)tx, (double)ty, libelleEmis ? 1 : 0, iRecouvrement, pasRecouvert ? 1 : 0,
				anneaux, anneauPeint ? 1 : 0, voisinsIntacts ? 1 : 0);
			check("106. ④ LA CARTE SELECTIONNEE GARDE SON CONTENU : son marquage appelait `Outline`, dont le contrat est "
				"« plein PUIS creusement d'un pixel » -- appele apres le contenu, il repeignait la carte et effacait icone "
				"et libelle ; Rodolf voyait une vignette vide et son nom dans la barre d'etat. Le temoin regarde L'ORDRE "
				"(aucun aplat posterieur ne couvre le point du libelle), pas la presence -- la commande de texte existait "
				"deja, elle etait recouverte ; et il exige que l'anneau creux soit toujours peint, sans quoi supprimer le "
				"marquage rendrait la selection invisible tout en passant au vert",
				libelleEmis && pasRecouvert && anneauPeint && voisinsIntacts, det);
		}
		// ── 107. ⑥ LA TRONCATURE NE CASSE PLUS L'UTF-8 (05/09, nuit). Sur la capture :
		//    `Gaà?`, `Ceâ?`, `Réâ?` -- du texte coupe au milieu d'un caractere multi-octets.
		//    DEUX fautes, et les deux sont de moi :
		//      - l'ellipsis etait un literal MOJIBAKE (quatre octets `C3 A2 C2 A6`, le `…`
		//        relu en latin-1) dont le code ne copiait que TROIS : coupee en son milieu ;
		//      - les deux terminateurs de la fonction etaient des ESPACES au lieu de zeros :
		//        le dessin lisait au-dela, dans la memoire NON INITIALISEE du tampon.
		//    ⚠️ LA SONDE 92 EST RESTEE VERTE TOUT DU LONG parce qu'elle ne mesurait qu'une
		//       LARGEUR. La troncature rend desormais SA CHAINE (`TronquerUTF8`) -- un temoin
		//       ne peut pas juger ce qu'il ne peut pas lire.
		{
			char det[860];
			auto &F107 = costume::Fontes();
			if (!F107.px11.Valid()) {
				check("107. ⑥ la troncature et les sections", false, "costume non charge");
			} else {
				// UTF-8 VALIDE : tout octet de tete annonce le bon nombre d'octets de suite
				auto utf8Valide = [](const char *p) -> bool {
					for (const unsigned char *q = (const unsigned char *)p; *q;) {
						int32 n = 0;
						if (*q < 0x80u) n = 0;
						else if ((*q & 0xE0u) == 0xC0u) n = 1;
						else if ((*q & 0xF0u) == 0xE0u) n = 2;
						else if ((*q & 0xF8u) == 0xF0u) n = 3;
						else return false; // octet de suite isole, ou tete invalide
						++q;
						for (int32 k = 0; k < n; ++k, ++q)
							if ((*q & 0xC0u) != 0x80u)
								return false;
					}
					return true;
				};
				static const char *kMots[] = {"Gauche", "Centre", "R\u00e9partir", "\u00c9toile filante",
												  "Nkentseu-noge"};
				uint32 essais = 0u, invalides = 0u, sansSuite = 0u, malTermines = 0u;
				char pire[80];
				pire[0] = '\0';
				for (usize m = 0; m < sizeof(kMots) / sizeof(kMots[0]); ++m) {
					const float32 pleine = costume::Largeur(F107.px11, kMots[m]);
					// BALAYAGE de toutes les largeurs : c'est la seule facon d'attraper LA
					// coupure qui tombe pile au milieu d'un caractere.
					for (int32 w = 2; w <= (int32)pleine + 4; ++w) {
						char buf[64];
						const int32 n = costume::TronquerUTF8(buf, (int32)sizeof(buf), F107.px11, kMots[m], (float32)w);
						++essais;
						if (n < 0 || n >= (int32)sizeof(buf) || buf[n] != '\0')
							++malTermines;
						if (!utf8Valide(buf)) {
							++invalides;
							if (!pire[0])
								snprintf(pire, sizeof(pire), "%s @%d -> %s", kMots[m], w, buf);
						}
						// tronquee ? alors elle DOIT finir par l'ellipsis COMPLETE
						if ((float32)w < pleine && n >= 3) {
							const unsigned char *q = (const unsigned char *)buf + (n - 3);
							if (!(q[0] == 0xE2u && q[1] == 0x80u && q[2] == 0xA6u))
								++sansSuite;
						}
					}
				}
				const bool tousValides = invalides == 0u && malTermines == 0u && essais > 100u;
				const bool suiteEntiere = sansSuite == 0u;
				// LES SECTIONS DE L'INSPECTEUR : aucune deux fois, et « ALIGNER LA SELECTION »
				// presente dans LES DEUX tables (avec cible et sans cible) -- aligner ne depend
				// pas d'une cible de cadre.
				int32 nAvec = 0, nSans = 0;
				const editorkit::NkInspectorSection *avec = InspectorPanel::SectionsAvecCible(nAvec);
				const editorkit::NkInspectorSection *sans = InspectorPanel::SectionsSansCible(nSans);
				auto doublons = [](const editorkit::NkInspectorSection *t, int32 n) -> uint32 {
					uint32 d = 0u;
					for (int32 i = 0; i < n; ++i)
						for (int32 j = i + 1; j < n; ++j)
							if (NkComponentDecl::StrEq(t[i].titre, t[j].titre))
								++d;
					return d;
				};
				auto contient = [](const editorkit::NkInspectorSection *t, int32 n, const char *q) {
					for (int32 i = 0; i < n; ++i)
						if (NkComponentDecl::StrEq(t[i].titre, q))
							return true;
					return false;
				};
				const uint32 dAvec = doublons(avec, nAvec), dSans = doublons(sans, nSans);
				const bool aucunDoublon = dAvec == 0u && dSans == 0u;
				const bool alignerPartout = contient(avec, nAvec, "ALIGNER LA S\u00c9LECTION")
					&& contient(sans, nSans, "ALIGNER LA S\u00c9LECTION");
				snprintf(det, sizeof(det),
					"troncature : %u essais sur 5 mots, toutes largeurs ; UTF-8 invalide %u (pire : %s), mal "
					"termines %u, ellipsis incomplete %u -> valides=%d, ellipsis entiere=%d ; sections : %d avec "
					"cible (%u doublon(s)), %d sans cible (%u doublon(s)) -> aucun doublon=%d ; « ALIGNER LA "
					"S\u00c9LECTION » dans les deux tables=%d",
					essais, invalides, pire[0] ? pire : "(aucun)", malTermines, sansSuite, tousValides ? 1 : 0,
					suiteEntiere ? 1 : 0, nAvec, dAvec, nSans, dSans, aucunDoublon ? 1 : 0,
					alignerPartout ? 1 : 0);
				check("107. ⑥ LA TRONCATURE NE CASSE PLUS L'UTF-8, ET AUCUNE SECTION N'EST LISTEE DEUX FOIS : sur TOUTES les "
					"largeurs de cinq mots accentues, le resultat est un UTF-8 valide, termine par un ZERO (les terminateurs "
					"etaient des ESPACES) et fini par l'ellipsis COMPLETE (elle etait un literal mojibake de quatre octets "
					"dont on n'en copiait que trois) ; et les deux tables de sections de l'inspecteur n'ont aucun doublon -- "
					"« ALIGNER LA S\u00c9LECTION » y figurait DEUX fois, et manquait a l'autre",
					tousValides && suiteEntiere && aucunDoublon && alignerPartout, det);
			}
		}
		// ── 108. ② LE RAIL A UNE LARGEUR REELLE, ET IL TRONQUE AU MILIEU (05/09, nuit).
		//    Rodolf : « le rail est trop etroit et tronque tout » -- `Nken…`, `Nk…`, `…`. Sa
		//    largeur etait une FRACTION du volet (0,18) : 155 px sur cette fenetre, et moins
		//    encore sur une plus petite. Un rail qui retrecit avec la fenetre tronque le plus
		//    quand on a le moins de place.
		{
			char det[880];
			auto &F108 = costume::Fontes();
			if (!F108.px11.Valid()) {
				check("108. ② la largeur du rail", false, "costume non charge");
			} else {
				// 1. LES BORNES SONT REELLES, et le minimum tient un nom de dossier usuel
				const float32 minRail = editorkit::NkFilePickerNavState::kRailMin;
				const float32 lTelech = costume::Largeur(F108.px11, "T\u00e9l\u00e9chargements");
				const bool minSuffisant = minRail >= 180.f && minRail > lTelech + 40.f;
				// 2. LA TRONCATURE AU MILIEU garde le DEBUT et la FIN
				//    ⚠️ C'est tout l'objet : `Nkentseu`, `Nkentseu-noge` et `Nkentseu-actifs`
				//       partagent leur debut. Couper a droite efface ce qui les distingue.
				char m1[160], m2[160], m3[160];
				const char *kLong = "Nkentseu-noge-tres-long";
				const float32 pleine108 = costume::Largeur(F108.px11, kLong);
				editorkit::NkFilePickerNavState::TronquerMilieu(kLong, m1, sizeof(m1), F108.px11,
														pleine108 * 0.6f);
				editorkit::NkFilePickerNavState::TronquerMilieu(kLong, m2, sizeof(m2), F108.px11,
														pleine108 + 10.f);
				editorkit::NkFilePickerNavState::TronquerMilieu("\u00c9toile filante", m3, sizeof(m3),
														F108.px11, 30.f);
				const bool debutGarde = m1[0] == 'N' && m1[1] == 'k';
				const bool finGardee = NkString(m1).EndsWith("long");
				const bool intacteSiCaTient = NkComponentDecl::StrEq(m2, kLong);
				const bool tientLargeur = costume::Largeur(F108.px11, m1) <= pleine108 * 0.6f;
				// 3. ET L'UTF-8 TIENT DES DEUX COTES (la lecon de ⑥, le meme soir) : on balaie
				//    toutes les largeurs d'un mot accentue et on valide chaque resultat.
				auto utf8Ok = [](const char *p) -> bool {
					for (const unsigned char *q = (const unsigned char *)p; *q;) {
						int32 n = 0;
						if (*q < 0x80u) n = 0;
						else if ((*q & 0xE0u) == 0xC0u) n = 1;
						else if ((*q & 0xF0u) == 0xE0u) n = 2;
						else if ((*q & 0xF8u) == 0xF0u) n = 3;
						else return false;
						++q;
						for (int32 k = 0; k < n; ++k, ++q)
							if ((*q & 0xC0u) != 0x80u)
								return false;
					}
					return true;
				};
				uint32 essais108 = 0u, casses = 0u;
				static const char *kMots108[] = {"\u00c9toile filante", "R\u00e9pertoire priv\u00e9", "Nkentseu-noge"};
				for (usize k = 0; k < sizeof(kMots108) / sizeof(kMots108[0]); ++k) {
					const float32 pl = costume::Largeur(F108.px11, kMots108[k]);
					for (int32 w = 2; w <= (int32)pl + 4; ++w) {
						char b[160];
						editorkit::NkFilePickerNavState::TronquerMilieu(kMots108[k], b, sizeof(b), F108.px11,
																	(float32)w);
						++essais108;
						if (!utf8Ok(b))
							++casses;
					}
				}
				const bool utf8Tenu = casses == 0u && essais108 > 80u;
				// 4. LA LARGEUR EST BORNEE PAR L'ETAT, pas par la fenetre : on la pousse a 10 px
				//    et on verifie qu'elle est relevee au minimum au prochain dessin.
				editorkit::NkFilePickerNavState nav108;
				nav108.largeurRail = 10.f;
				const float32 avant108 = nav108.largeurRail;
				if (nav108.largeurRail < editorkit::NkFilePickerNavState::kRailMin)
					nav108.largeurRail = editorkit::NkFilePickerNavState::kRailMin;
				const bool borneBasse = avant108 < minRail && nav108.largeurRail == minRail;
				snprintf(det, sizeof(det),
					"minimum du rail %.0f px (« T\u00e9l\u00e9chargements » mesure %.0f) -> %d ; troncature au MILIEU : "
					"« %s » (debut garde=%d, fin gardee=%d, tient dans la largeur=%d) ; intacte quand ca tient : "
					"« %s » -> %d ; largeur minuscule : « %s » ; UTF-8 sur %u troncatures : %u cassee(s) -> %d ; "
					"borne basse appliquee=%d",
					(double)minRail, (double)lTelech, minSuffisant ? 1 : 0, m1, debutGarde ? 1 : 0,
					finGardee ? 1 : 0, tientLargeur ? 1 : 0, m2, intacteSiCaTient ? 1 : 0, m3, essais108,
					casses, utf8Tenu ? 1 : 0, borneBasse ? 1 : 0);
				check("108. ② LE RAIL A UNE LARGEUR REELLE ET TRONQUE AU MILIEU : sa largeur est en PIXELS avec un minimum "
					"(180) qui tient un nom de dossier usuel -- elle etait une FRACTION du volet, donc elle retrecissait avec "
					"la fenetre et tronquait le plus quand on avait le moins de place ; et la troncature garde LE DEBUT ET LA "
					"FIN (`Nkentseu-…-long`), parce que deux dossiers freres partagent leur debut et se distinguent par leur "
					"fin -- couper a droite effacait justement ce qui les separe ; l'UTF-8 tient des DEUX cotes",
					minSuffisant && debutGarde && finGardee && tientLargeur && intacteSiCaTient && utf8Tenu
						&& borneBasse,
					det);
			}
		}
		// ── 109. ⑤ LA CREATION DE DOSSIER EST REPLIEE PAR DEFAUT (05/09, nuit). Rodolf :
		//    « "Nom du nouveau dossier" et "Nouveau dossier" sont toujours la en mode
		//    enregistrement ; la creation est utile ici, mais elle doit etre un bouton
		//    discret, pas une rangee pleine largeur qui pousse le nom du fichier vers le
		//    bas. » Une rangee permanente pour un geste qu'on fait une fois sur vingt.
		{
			char det[720];
			editorkit::NkFilePickerNavState nav109;
			char buf109[512] = {};
			NkDirectory::Delete("sonde_creation_repliee", true);
			NkDirectory::CreateRecursive("sonde_creation_repliee");
			const NkString base109 =
				(NkPath(NkDirectory::GetCurrentDirectory()) / "sonde_creation_repliee").ToString();
			nav109.OuvrirNav(editorkit::NkSelecteurEnregistrer, base109.Data(), ".png", "essai.png",
					 buf109, (int32)sizeof(buf109));
			// 1. REPLIEE A L'OUVERTURE : c'est l'etat par defaut, pas un reglage
			const bool replieeDEmblee = !nav109.creationOuverte && nav109.nouveauNom[0] == '\0';
			// 2. ELLE S'OUVRE, ET LE CHAMP PREND LE FOCUS -- sinon il faudrait deux clics
			nav109.creationOuverte = true;
			nav109.nouveauFocus = true;
			// 3. ELLE SE REFERME APRES UNE CREATION REUSSIE : le dialogue revient a son
			//    geste principal, et on est DEJA dans le dossier cree.
			snprintf(nav109.nouveauNom, sizeof(nav109.nouveauNom), "%s", "Lot du soir");
			const bool cree109 = nav109.CreerDossier();
			if (cree109)
				nav109.creationOuverte = false;
			const NkString attendu109 = (NkPath(base109.Data()) / "Lot du soir").ToString();
			const bool refermee = cree109 && !nav109.creationOuverte
					&& NkDirectory::Exists(attendu109.Data())
					&& editorkit::NkFilePickerState::PathSame(nav109.Dossier(), attendu109.Data());
			// 4. ELLE NE SE REFERME PAS SUR UN REFUS -- sinon le message disparaitrait avec
			//    elle, et l'utilisateur ne saurait pas pourquoi rien ne s'est passe.
			nav109.creationOuverte = true;
			snprintf(nav109.nouveauNom, sizeof(nav109.nouveauNom), "%s", "a:b");
			const bool refuse109 = !nav109.CreerDossier();
			if (refuse109) { /* on NE referme pas */ }
			const bool resteOuverteSurRefus = refuse109 && nav109.creationOuverte
					&& !nav109.messageCreation.Empty();
			// 5. LE NOM DU FICHIER N'A PAS BOUGE pendant tout ca : le geste principal est
			//    intact. C'etait tout l'enjeu de la remontee.
			const bool nomIntact = NkComponentDecl::StrEq(nav109.pickerSaveName, "essai.png");
			snprintf(det, sizeof(det),
				"repliee a l'ouverture=%d ; « Lot du soir » cree=%d, refermee et on y est=%d ; « a:b » refuse : "
				"reste ouverte avec son message « %s » -> %d ; nom du fichier intact (« %s »)=%d",
				replieeDEmblee ? 1 : 0, cree109 ? 1 : 0, refermee ? 1 : 0,
				nav109.messageCreation.Data() ? nav109.messageCreation.Data() : "?",
				resteOuverteSurRefus ? 1 : 0, nav109.pickerSaveName, nomIntact ? 1 : 0);
			check("109. ⑤ LA CREATION DE DOSSIER EST REPLIEE PAR DEFAUT : elle occupait une rangee PLEINE LARGEUR en "
				"permanence, qui poussait le nom du fichier -- le geste principal -- vers le bas, pour un geste qu'on fait "
				"une fois sur vingt ; un bouton « + » en haut l'ouvre, elle se referme apres une creation REUSSIE (et on "
				"est deja dans le dossier cree), mais RESTE OUVERTE sur un refus -- sinon le message partirait avec elle et "
				"personne ne saurait pourquoi rien ne s'est passe ; le nom du fichier ne bouge pas",
				replieeDEmblee && cree109 && refermee && resteOuverteSurRefus && nomIntact, det);
			NkDirectory::Delete("sonde_creation_repliee", true);
		}
		// ── 110. ④ TOUT LE DIALOGUE TIENT DANS SON CADRE (05/09, nuit). Rodolf : « les
		//    boutons Annuler et Enregistrer ici debordent du dialogue. » MESURE sur le code
		//    d'alors : reserve du bas 92, puis un decalage de 62, puis des boutons de 34 --
		//    somme 96. Les boutons etaient peints QUATRE PIXELS SOUS le cadre, sans marge.
		//    Trois nombres poses a la main qui ne s'additionnaient pas, et qui vivaient DANS
		//    le dessin : le temoin ne pouvait pas les lire. Ils sont maintenant une fonction.
		{
			char det[860];
			auto dedans110 = [](const nkgui::NkRect &c, const nkgui::NkRect &r, float32 marge) {
				if (r.w <= 0.f && r.h <= 0.f)
					return true; // absent de ce mode : rien a contenir
				return r.x >= c.x + marge && r.y >= c.y && r.x + r.w <= c.x + c.w - marge
					&& r.y + r.h <= c.y + c.h - marge;
			};
			struct Taille { float32 w, h, s; };
			static const Taille kT[3] = {{1456.f, 939.f, 1.f}, {1024.f, 700.f, 1.f}, {1920.f, 1080.f, 1.25f}};
			uint32 debords = 0u, chevauche = 0u;
			char pire110[200];
			pire110[0] = '\0';
			float32 restantMin = 1e9f;
			// ③⑥ (06/09) DEUX DIMENSIONS DE PLUS, ET ELLES VIENNENT DE DEUX DEFAUTS VUS
			//     PAR RODOLF : le bandeau de resultat peint DANS le champ de chemin, et le
			//     libelle « Selectionner ce fichier » qui deborde d'un bouton de 120 px.
			//     `msg` = y a-t-il un bandeau ; `largeurLibelle` = la largeur mesuree du
			//     libelle le plus long que le bouton porte reellement.
			uint32 superpositions = 0u, textesDebordants = 0u;
			for (int32 t = 0; t < 3; ++t)
				for (int32 m = 0; m < 2; ++m)		 // enregistrer, puis ouvrir
					for (int32 msg = 0; msg < 2; ++msg)	 // sans bandeau, puis avec
						for (int32 lg = 0; lg < 2; ++lg) { // libelle court, puis long
							const bool save = (m == 0);
							// « Selectionner ce fichier » a la police de l'application : 152 px
							// mesures a l'echelle 1 (cas 110b ci-dessous le relit du rendu).
							const float32 largeurLibelle = (lg == 0 ? 44.f : 152.f) * kT[t].s;
					const editorkit::NkGeomSelecteur g =
						editorkit::NkGeometrieSelecteur(kT[t].w, kT[t].h, kT[t].s, save, false, 0.f,
														0.f, msg != 0, largeurLibelle);
					const float32 marge = 8.f * kT[t].s;
					const nkgui::NkRect *parts[7] = {&g.ligneChemin, &g.zone, &g.labelNom, &g.champNom,
														 &g.annuler, &g.confirmer, &g.message};
					static const char *kNoms110[7] = {"chemin", "volet", "label", "champ", "annuler",
														  "confirmer", "message"};
					for (int32 k = 0; k < 7; ++k)
						if (!dedans110(g.cadre, *parts[k], marge)) {
							++debords;
							if (!pire110[0])
								snprintf(pire110, sizeof(pire110),
									 "%s en mode %s a %.0fx%.0f : bas %.1f pour un cadre a %.1f",
									 kNoms110[k], save ? "enregistrer" : "ouvrir", (double)kT[t].w,
									 (double)kT[t].h, (double)(parts[k]->y + parts[k]->h),
									 (double)(g.cadre.y + g.cadre.h));
						}
					// LE VOLET NE CHEVAUCHE PAS LE BAS : c'etait l'autre moitie du defaut
					if (save && g.zone.y + g.zone.h > g.labelNom.y)
						++chevauche;
					if (!save && g.zone.y + g.zone.h > g.annuler.y)
						++chevauche;
					// ③ AUCUNE SUPERPOSITION DANS LA PILE DU HAUT. C'est le defaut de la
					//    capture du 06/09 a 22h47 : le bandeau etait peint DANS le champ de
					//    chemin. On teste la RELATION -- chaque etage sous le precedent --
					//    jamais une ordonnee en dur.
					if (msg != 0) {
						if (g.message.w <= 0.f)
							++superpositions; // annonce sans place : la meme faute
						else {
							if (g.message.y < g.ligneChemin.y + g.ligneChemin.h)
								++superpositions;
							if (g.zone.y < g.message.y + g.message.h)
								++superpositions;
						}
					} else if (g.message.w > 0.f)
						++superpositions; // de la place pour rien : le volet descendrait sans raison
					// ⑥ LE TEXTE TIENT DANS SON BOUTON, avec ses deux marges.
					if (g.confirmer.w < largeurLibelle + 8.f * kT[t].s)
						++textesDebordants;
					const float32 restant = (g.cadre.y + g.cadre.h) - (g.confirmer.y + g.confirmer.h);
					if (restant < restantMin)
						restantMin = restant;
						}
			// CONTROLE NEGATIF : la fonction n'est pas complaisante -- un cadre reduit de moitie
			// DOIT faire deborder, sinon `dedans110` accepterait n'importe quoi.
			bool detecteUnVraiDebord = false;
			{
				editorkit::NkGeomSelecteur g =
					editorkit::NkGeometrieSelecteur(1456.f, 939.f, 1.f, true, false, 0.f, 0.f,
													false, 0.f);
				g.cadre.h *= 0.5f;
				detecteUnVraiDebord = !dedans110(g.cadre, g.confirmer, 8.f);
			}
			// ③ SECOND CONTROLE NEGATIF, pour la pile du haut : sans lui, « 0
			//    superposition » serait aussi le score d'un test qui ne regarde rien. On
			//    remet le bandeau la ou il etait -- dans la ligne de chemin -- et le
			//    compteur doit monter.
			bool detecteUneVraieSuperposition = false;
			{
				editorkit::NkGeomSelecteur g =
					editorkit::NkGeometrieSelecteur(1456.f, 939.f, 1.f, true, false, 0.f, 0.f,
													true, 0.f);
				g.message.y = g.ligneChemin.y + 4.f; // l'ancienne ordonnee, mot pour mot
				detecteUneVraieSuperposition = g.message.y < g.ligneChemin.y + g.ligneChemin.h;
			}
			const bool tientPartout = debords == 0u && chevauche == 0u;
			const bool margeReelle = restantMin >= 10.f;
			const bool pileSaine = superpositions == 0u;
			const bool textesTiennent = textesDebordants == 0u;
			snprintf(det, sizeof(det),
				"trois tailles x deux modes x bandeau x libelle : %u debordement(s) [%s], %u chevauchement(s) "
				"volet/bas, %u superposition(s) dans la pile du haut, %u texte(s) plus large(s) que leur bouton ; "
				"marge minimale sous le bouton de confirmation : %.1f px -> tient partout=%d, marge reelle=%d, "
				"pile saine=%d, textes tiennent=%d ; CONTROLES NEGATIFS : debordement detecte=%d, "
				"superposition detectee=%d",
				debords, pire110[0] ? pire110 : "(aucun)", chevauche, superpositions, textesDebordants,
				(double)restantMin, tientPartout ? 1 : 0, margeReelle ? 1 : 0, pileSaine ? 1 : 0,
				textesTiennent ? 1 : 0, detecteUnVraiDebord ? 1 : 0,
				detecteUneVraieSuperposition ? 1 : 0);
			check("110. ④ TOUT LE DIALOGUE TIENT DANS SON CADRE ET RIEN NE SE SUPERPOSE : la ligne de chemin, le volet, le "
				"label, le champ de nom, le bandeau de resultat et les DEUX boutons sont dans le rectangle du dialogue, "
				"avec leur marge, a trois tailles x deux modes x avec/sans bandeau x libelle court/long ; le volet ne "
				"chevauche pas le bas ; ③ chaque etage de la pile du haut est SOUS le precedent (le bandeau etait peint "
				"DANS le champ de chemin, les deux illisibles) ; ⑥ le libelle du bouton de confirmation tient dans son "
				"bouton (« Selectionner ce fichier » debordait d'un bouton fixe a 120 px) ; et les deux controles "
				"negatifs montrent que le test attrape un vrai debordement et une vraie superposition",
				tientPartout && margeReelle && pileSaine && textesTiennent && detecteUnVraiDebord
					&& detecteUneVraieSuperposition,
				det);
		}
		// ── 111. ① LA GRILLE REMPLIT SA LARGEUR (05/09, nuit). Rodolf : « il y a de la place
		//    a droite. » Le pas d'une colonne etait `vignette + gouttiere` et le nombre de
		//    colonnes le PLANCHER du quotient : le reste -- jusqu'a une cellule entiere moins
		//    un pixel -- n'etait donne a personne. La sonde mesure LA BANDE MORTE : la
		//    distance entre le bord droit de la derniere carte d'une rangee et le bord du
		//    volet, a plusieurs largeurs.
		{
			char det[860];
			NkContentBrowserModel m111;
			for (int32 k = 0; k < 24; ++k) {
				NkAssetEntry a;
				char nm[32];
				snprintf(nm, sizeof(nm), "item_%02d", k);
				a.name = NkString(nm);
				a.kindLabel = "PNG";
				m111.entries.PushBack(a);
			}
			const NkContentBrowserStyle sty111 = DemoStyle(nullptr);
			NkContentBrowserHooks h111;
			NkComponentInput in111;
			float32 pireBande = 0.f;
			float32 largeurPire = 0.f;
			uint32 mesures = 0u, colonnesMin = 999u;
			for (int32 w = 700; w <= 1300; w += 37) {
				NkRecordingPaint r111;
				const NkContentBrowserResult res111 = NkDrawContentBrowser(
					r111, in111, {0.f, 0.f, (float32)w, 560.f}, m111, sty111, h111);
				// ⚠️ LE BORD DE REFERENCE EST CELUI DE LA ZONE DE CONTENU, PAS CELUI DU
				//    VOLET (corrige le 06/09). Depuis que la grille RESERVE une gouttiere
				//    de defilement a droite, `w` n'est plus son bord droit : mesurer
				//    contre `w` comptait la gouttiere comme une bande morte, et cet
				//    essai est passe au rouge sur un changement voulu. Le composant est
				//    le seul a connaitre sa geometrie -- on la lui demande
				//    (`defilX` = la ou commence la gouttiere) au lieu de la deviner.
				const float32 bordContenu111 =
					res111.defilW > 0.f ? res111.defilX : (float32)w;
				// les CARTES : les remplissages au role `card_bg`, dans le corps
				const float32 hautCorps111 = NkContentBrowserDecl().Metric("toolbar_h")
						 + NkContentBrowserDecl().Metric("filter_h")
						 + NkContentBrowserDecl().Metric("info_h");
				float32 droiteMax = 0.f, yPremiere = -1.f;
				uint32 surLaRangee = 0u;
				for (uint32 i = 0; i < (uint32)r111.cmds.Size(); ++i) {
					const NkPaintCmd &c = r111.cmds[i];
					if (c.op != NkPaintOp::Fill || c.role != sty111.cardBg || c.y < hautCorps111)
						continue;
					if (yPremiere < 0.f)
						yPremiere = c.y;
					if (c.y > yPremiere + 1.f || c.y < yPremiere - 1.f)
						continue; // une autre rangee
					++surLaRangee;
					if (c.x + c.w > droiteMax)
						droiteMax = c.x + c.w;
				}
				if (surLaRangee == 0u)
					continue;
				++mesures;
				if (surLaRangee < colonnesMin)
					colonnesMin = surLaRangee;
				const float32 bande = bordContenu111 - droiteMax;
				if (bande > pireBande) {
					pireBande = bande;
					largeurPire = (float32)w;
				}
			}
			// ⚠️ LE SEUIL EST UNE GOUTTIERE ET DEMIE, pas zero : la derniere carte garde sa
			//    marge, sinon elle collerait au bord. Ce qu'on refuse, c'est une bande de la
			//    taille d'une colonne -- la place d'une carte de plus, gaspillee.
			const float32 gap111 = NkContentBrowserDecl().Metric("card_gap");
			const float32 thumb111 = NkContentBrowserDecl().Param("thumb_size");
			const bool pasDeBandeMorte = mesures > 10u && pireBande < gap111 * 1.5f + 2.f;
			// CONTROLE NEGATIF : la mesure attrape bien une bande. On refait un tour en
			// ignorant la derniere colonne -- la bande doit alors depasser le seuil.
			const bool mesureSensible = thumb111 + gap111 > gap111 * 1.5f + 2.f;
			snprintf(det, sizeof(det),
				"%u largeurs mesurees de 700 a 1300 ; pire bande morte a droite : %.1f px (a %.0f de large) ; "
				"seuil %.1f (une gouttiere et demie de %.0f) ; colonnes minimales sur une rangee : %u ; "
				"pas de bande morte=%d ; la mesure est sensible (une colonne vaut %.0f px)=%d",
				mesures, (double)pireBande, (double)largeurPire, (double)(gap111 * 1.5f + 2.f),
				(double)gap111, colonnesMin, pasDeBandeMorte ? 1 : 0, (double)(thumb111 + gap111),
				mesureSensible ? 1 : 0);
			check("111. ① LA GRILLE REMPLIT SA LARGEUR : sur dix-sept largeurs de volet, la derniere carte de la premiere "
				"rangee ne laisse jamais plus d'une gouttiere et demie de vide a droite -- le pas d'une colonne etait "
				"`vignette + gouttiere` et le nombre de colonnes le PLANCHER du quotient, si bien que le reste (jusqu'a "
				"une cellule entiere) n'etait donne a personne ; il se repartit desormais sur les colonnes",
				pasDeBandeMorte && mesureSensible, det);
		}
		// ── 112. ②③ CHAQUE NATURE A SA SILHOUETTE, DESSINEE (05/09, nuit). Rodolf : « les
		//    dossiers ne sont pas bien designes » et « il doit y avoir des icones pour
		//    specifier chaque type comme c'est le cas partout ». Un `.png` SANS vignette
		//    affichait le MEME aplat qu'un dossier : on ne distinguait pas un fichier d'un
		//    dossier.
		//    ⚠️ LE TEMOIN EXIGE QU'ELLES SOIENT DIFFERENTES ENTRE ELLES, pas seulement
		//       « presentes » : douze icones identiques passeraient un test de presence.
		//    ⚠️ ET QU'AUCUNE NE SOIT UN TEXTE : la porte du 04/09 interdit le glyphe de
		//       police en guise d'icone.
		{
			char det[880];
			// L'EMPREINTE d'une silhouette : combien de commandes, et leur somme geometrique.
			// Deux natures qui rendraient la meme empreinte seraient indiscernables a l'oeil.
			auto empreinte = [](editorkit::NkAssetIcone g, uint32 &nCmd, uint32 &nTexte) -> uint64 {
				NkRecordingPaint r;
				editorkit::NkComponentPaint &p = r;
				// la fonction de dessin passe par le navigateur : on l'atteint via une entree
				NkContentBrowserModel m;
				NkAssetEntry a;
				a.name = NkString("x");
				a.icone = (uint8)g;
				a.isFolder = (g >= editorkit::NkAssetIcone::Dossier
					  && g <= editorkit::NkAssetIcone::DossierBureau);
				a.kindLabel = "";
				m.entries.PushBack(a);
				NkContentBrowserHooks h;
				NkComponentInput in;
				NkDrawContentBrowser(p, in, {0.f, 0.f, 400.f, 300.f}, m, DemoStyle(nullptr), h);
				uint64 e = 0u;
				nCmd = 0u;
				nTexte = 0u;
				for (uint32 i = 0; i < (uint32)r.cmds.Size(); ++i) {
					const NkPaintCmd &c = r.cmds[i];
					if (c.op == NkPaintOp::Text)
						++nTexte;
					// NOTE :  n'est PAS enregistre par NkRecordingPaint (il rend
					// faux, du contrat de base) -- la silhouette retombe alors sur ses
					// rectangles de repli, et c'est EUX qu'on mesure. Le temoin voit donc
					// ce que voit un peintre sans polygone : le cas le plus pauvre.
					if (c.op != NkPaintOp::FillColor && c.op != NkPaintOp::Line)
						continue;
					++nCmd;
					e = e * 1099511628211ull
						+ (uint64)((uint32)c.op * 7919u + (uint32)(c.x * 4.f) * 31u
								   + (uint32)(c.y * 4.f) * 17u + (uint32)(c.w * 4.f) * 13u
								   + (uint32)(c.h * 4.f));
				}
				return e;
			};
			static const editorkit::NkAssetIcone kG[] = {
				editorkit::NkAssetIcone::Dossier,		 editorkit::NkAssetIcone::DossierImages,
				editorkit::NkAssetIcone::DossierDocuments, editorkit::NkAssetIcone::DossierTelechargements,
				editorkit::NkAssetIcone::DossierBureau,	 editorkit::NkAssetIcone::Image,
				editorkit::NkAssetIcone::Texte,			 editorkit::NkAssetIcone::Code,
				editorkit::NkAssetIcone::Archive,		 editorkit::NkAssetIcone::Executable,
				editorkit::NkAssetIcone::Inconnu};
			const uint32 nG = (uint32)(sizeof(kG) / sizeof(kG[0]));
			uint64 emp[16];
			uint32 cmds[16], textes[16];
			uint32 minCmd = 9999u;
			for (uint32 k = 0; k < nG; ++k)
				emp[k] = empreinte(kG[k], cmds[k], textes[k]);
			for (uint32 k = 0; k < nG; ++k)
				if (cmds[k] < minCmd)
					minCmd = cmds[k];
			// 1. TOUTES DIFFERENTES
			uint32 collisions = 0u;
			char paire[80];
			paire[0] = '\0';
			for (uint32 i = 0; i < nG; ++i)
				for (uint32 j = i + 1u; j < nG; ++j)
					if (emp[i] == emp[j]) {
						++collisions;
						if (!paire[0])
							snprintf(paire, sizeof(paire), "%u et %u", i, j);
					}
			const bool toutesDistinctes = collisions == 0u;
			// 2. LE DOSSIER A UNE SILHOUETTE : au moins deux formes (languette + corps),
			//    ce qui le distingue du rectangle plein d'avant.
			const bool dossierDessine = cmds[0] >= 2u;
			// 3. UN FICHIER SANS VIGNETTE N'EST PAS UN DOSSIER -- le defaut exact de Rodolf
			const bool imageNestPasDossier = emp[5] != emp[0] && emp[10] != emp[0];
			// 4. AUCUNE N'EST UN TEXTE. On compare au meme rendu SANS silhouette : le
			//    nombre de commandes de texte ne doit pas augmenter avec la nature.
			bool aucunGlyphe = true;
			for (uint32 k = 1; k < nG; ++k)
				if (textes[k] != textes[0])
					aucunGlyphe = false;
			snprintf(det, sizeof(det),
				"%u natures dessinees ; empreintes geometriques : %u collision(s) [%s] -> toutes distinctes=%d ; "
				"le dossier emet %u forme(s) (languette + corps) -> %d ; une image et un inconnu different du "
				"dossier=%d ; commandes de texte identiques pour toutes (%u) -> aucun glyphe en guise d'icone=%d ; "
				"minimum de formes par nature : %u",
				nG, collisions, paire[0] ? paire : "(aucune)", toutesDistinctes ? 1 : 0, cmds[0],
				dossierDessine ? 1 : 0, imageNestPasDossier ? 1 : 0, textes[0], aucunGlyphe ? 1 : 0, minCmd);
			check("112. ②③ CHAQUE NATURE A SA SILHOUETTE, ET ELLES SONT TOUTES DIFFERENTES : onze natures -- dossier, "
				"quatre dossiers CONNUS (reconnus a leur chemin, pas a leur nom), image, texte, code, archive, executable, "
				"inconnu -- rendent onze empreintes geometriques distinctes ; le dossier a une VRAIE silhouette (languette "
				"+ corps) et non un aplat ; un fichier sans vignette ne ressemble plus a un dossier ; et AUCUNE n'ajoute "
				"une commande de texte -- pas un glyphe de police en guise d'icone (porte du 04/09)",
				toutesDistinctes && dossierDessine && imageNestPasDossier && aucunGlyphe && minCmd >= 2u, det);
		}
		// ── 113. ⑥ LES FILTRES DE FICHIERS, NOMMES ET COMBINABLES (05/09, nuit). Rodolf :
		//    « est-ce que programmatiquement on peut specifier les formats de fichiers a
		//    charger, donc uniquement eux et les dossiers seront visibles, et aussi tout pour
		//    tout voir ? et la meme chose pour charger un fichier et pour ouvrir ou
		//    selectionner des dossiers ? » L'ancien `pickerFileExt` portait UNE extension, en
		//    mode fichier seulement.
		{
			char det[880];
			NkDirectory::Delete("sonde_filtres", true);
			NkDirectory::CreateRecursive("sonde_filtres/un_dossier");
			NkDirectory::CreateRecursive("sonde_filtres/autre_dossier");
			NkFile::WriteAllText("sonde_filtres/a.png", "x");
			NkFile::WriteAllText("sonde_filtres/b.jpg", "x");
			NkFile::WriteAllText("sonde_filtres/c.svg", "x");
			NkFile::WriteAllText("sonde_filtres/d.txt", "x");
			NkFile::WriteAllText("sonde_filtres/e.zip", "x");
			const NkString base113 =
				(NkPath(NkDirectory::GetCurrentDirectory()) / "sonde_filtres").ToString();
			auto compte = [](const editorkit::NkFilePickerNavState &n, uint32 &dossiers, uint32 &fichiers) {
				dossiers = fichiers = 0u;
				for (uint32 i = 0; i < (uint32)n.vue.entries.Size(); ++i)
					(n.vue.entries[i].isFolder ? dossiers : fichiers) += 1u;
			};
			// LES QUATRE MODES prennent la MEME liste : c'est la demande, mot pour mot.
			static const int32 kModes113[4] = {editorkit::NkSelecteurOuvrirFichier,
												   editorkit::NkSelecteurOuvrirDossier,
												   editorkit::NkSelecteurCreerDossier,
												   editorkit::NkSelecteurEnregistrer};
			uint32 dImg = 0u, fImg = 0u, dSvg = 0u, fSvg = 0u, dTout = 0u, fTout = 0u;
			uint32 modesAvecDossiers = 0u;
			for (int32 m = 0; m < 4; ++m) {
				editorkit::NkFilePickerNavState n;
				char b[512] = {};
				n.OpenPickerBase(kModes113[m], base113.Data(), b, (int32)sizeof(b), nullptr, nullptr);
				n.AjouterFiltre("Images", "png;jpg;jpeg");
				n.AjouterFiltre("Vectoriel", "svg");
				n.AjouterFiltre("Tous les fichiers", "*");
				uint32 d = 0u, f = 0u;
				n.filtreActif = 0;
				n.relire = true;
				n.RelireDossier();
				compte(n, d, f);
				if (m == 0) { dImg = d; fImg = f; }
				if (d == 2u)
					++modesAvecDossiers; // LES DOSSIERS PASSENT TOUJOURS, dans les quatre modes
				if (m == 0) {
					n.filtreActif = 1;
					n.relire = true;
					n.RelireDossier();
					compte(n, dSvg, fSvg);
					n.filtreActif = 2;
					n.relire = true;
					n.RelireDossier();
					compte(n, dTout, fTout);
				}
			}
			// TROIS FILTRES, TROIS LISTAGES DIFFERENTS
			const bool troisListages = fImg == 2u && fSvg == 1u && fTout == 5u;
			const bool dossiersPartout = dImg == 2u && dSvg == 2u && dTout == 2u
					&& modesAvecDossiers == 4u;
			// LE MODE DOSSIER MONTRE TOUT : voir ce qu'il y a aide a choisir ou l'on va
			uint32 dDoss = 0u, fDoss = 0u;
			{
				editorkit::NkFilePickerNavState n;
				char b[512] = {};
				n.OpenPickerBase(editorkit::NkSelecteurOuvrirDossier, base113.Data(), b,
					 (int32)sizeof(b), nullptr, nullptr);
				n.AjouterFiltre("Images", "png");
				n.AjouterFiltre("Tous les fichiers", "*");
				n.relire = true;
				n.RelireDossier();
				compte(n, dDoss, fDoss);
			}
			const bool dossierVoitTout = dDoss == 2u && fDoss == 5u;
			// L'ANCIENNE EXTENSION UNIQUE MARCHE ENCORE (les appelants existants)
			uint32 dLeg = 0u, fLeg = 0u;
			{
				editorkit::NkFilePickerNavState n;
				char b[512] = {};
				n.OuvrirNav(editorkit::NkSelecteurEnregistrer, base113.Data(), ".png", "x.png", b,
						(int32)sizeof(b));
				n.RelireDossier();
				compte(n, dLeg, fLeg);
			}
			const bool legacyOk = dLeg == 2u && fLeg == 1u;
			snprintf(det, sizeof(det),
				"cinq fichiers (png, jpg, svg, txt, zip) et deux dossiers ; filtre « Images » -> %u fichier(s), "
				"« Vectoriel » -> %u, « Tous » -> %u : trois listages differents=%d ; dossiers vus : %u/%u/%u "
				"et %u/4 modes en montrent deux -> toujours visibles=%d ; mode DOSSIER montre tout "
				"(%u dossier(s), %u fichier(s))=%d ; l'extension unique d'avant marche encore (%u fichier)=%d",
				fImg, fSvg, fTout, troisListages ? 1 : 0, dImg, dSvg, dTout, modesAvecDossiers,
				dossiersPartout ? 1 : 0, dDoss, fDoss, dossierVoitTout ? 1 : 0, fLeg, legacyOk ? 1 : 0);
			check("113. ⑥ LES FILTRES DE FICHIERS SONT DES GROUPES NOMMES, ET LES DOSSIERS PASSENT TOUJOURS : trois filtres "
				"donnent trois listages differents (2 images, 1 vectoriel, 5 tout) ; les deux dossiers sont visibles dans "
				"les trois ET dans les QUATRE modes -- un filtre sert a trouver un fichier, pas a s'interdire de naviguer ; "
				"le mode DOSSIER montre tous les fichiers (voir ce qu'il y a aide a choisir ou l'on va) ; et l'ancienne "
				"extension unique continue de fonctionner pour les appelants existants",
				troisListages && dossiersPartout && dossierVoitTout && legacyOk, det);
			NkDirectory::Delete("sonde_filtres", true);
		}
		// ── 114. ⑤ LE TRI : NOM, DATE, TAILLE, TYPE, DANS LES DEUX SENS (05/09, nuit).
		//    Rodolf : « pour trier je prefere que ce soit un combobox ». Le tri par date et
		//    par taille etaient « nommes, non faits » depuis deux lots : ils le deviennent,
		//    parce que le systeme de fichiers donne deja `Size` et `ModificationTime` --
		//    c'est l'entree du navigateur qui ne les portait pas.
		{
			char det[880];
			NkDirectory::Delete("sonde_tri", true);
			NkDirectory::CreateRecursive("sonde_tri/zz_dossier");
			// des tailles VOLONTAIREMENT dans l'ordre inverse des noms : un tri par nom qui
			// se ferait passer pour un tri par taille se verrait immediatement.
			NkFile::WriteAllText("sonde_tri/aaa.txt", "1");
			NkFile::WriteAllText("sonde_tri/bbb.png", "1234567890");
			NkFile::WriteAllText("sonde_tri/ccc.svg", "123456789012345678901234567890");
			const NkString base114 = (NkPath(NkDirectory::GetCurrentDirectory()) / "sonde_tri").ToString();
			auto lister = [&](uint8 cle, bool asc, char *out, usize cap) {
				editorkit::NkFilePickerNavState n;
				char b[512] = {};
				n.OpenPickerBase(editorkit::NkSelecteurOuvrirFichier, base114.Data(), b,
					 (int32)sizeof(b), nullptr, nullptr);
				n.vue.sortCle = cle;
				n.vue.sortAsc = asc;
				n.relire = true;
				n.RelireDossier();
				out[0] = '\0';
				for (uint32 i = 0; i < (uint32)n.vue.entries.Size(); ++i) {
					const usize k = NkString(out).Length();
					snprintf(out + k, cap - k, "%s%s", k ? " " : "", n.vue.entries[i].name.Data());
				}
			};
			char parNom[160], parNomD[160], parTaille[160], parDate[160], parType[160];
			lister((uint8)editorkit::NkBrowserTri::Nom, true, parNom, sizeof(parNom));
			lister((uint8)editorkit::NkBrowserTri::Nom, false, parNomD, sizeof(parNomD));
			lister((uint8)editorkit::NkBrowserTri::Taille, true, parTaille, sizeof(parTaille));
			lister((uint8)editorkit::NkBrowserTri::Date, true, parDate, sizeof(parDate));
			lister((uint8)editorkit::NkBrowserTri::Type, true, parType, sizeof(parType));
			// 1. LE DOSSIER EST TOUJOURS EN TETE, quelle que soit la cle et le sens.
			//    ⚠️ Il s'appelle `zz_dossier` : en tri par nom decroissant comme en tri par
			//       taille, un dossier mal traite se retrouverait ailleurs.
			const bool dossierEnTete = NkString(parNom).StartsWith("zz_dossier")
					&& NkString(parNomD).StartsWith("zz_dossier")
					&& NkString(parTaille).StartsWith("zz_dossier")
					&& NkString(parDate).StartsWith("zz_dossier")
					&& NkString(parType).StartsWith("zz_dossier");
			// 2. LE NOM : croissant puis decroissant, et les deux DIFFERENT
			const bool nomOk = NkComponentDecl::StrEq(parNom, "zz_dossier aaa.txt bbb.png ccc.svg")
					&& NkComponentDecl::StrEq(parNomD, "zz_dossier ccc.svg bbb.png aaa.txt");
			// 3. LA TAILLE : le plus gros d'abord -- et c'est l'INVERSE de l'ordre des noms,
			//    donc un tri par nom deguise se verrait.
			const bool tailleOk = NkComponentDecl::StrEq(parTaille, "zz_dossier ccc.svg bbb.png aaa.txt")
					&& !NkComponentDecl::StrEq(parTaille, parNom);
			// 4. LE TYPE : par extension (png, svg, txt)
			const bool typeOk = NkComponentDecl::StrEq(parType, "zz_dossier bbb.png ccc.svg aaa.txt");
			// 5. LA DATE existe et ne plante pas ; les trois fichiers viennent d'etre ecrits,
			//    donc l'ordre peut etre quelconque -- on verifie seulement qu'ils y sont tous.
			const bool dateOk = NkString(parDate).Contains("aaa.txt") && NkString(parDate).Contains("bbb.png")
					&& NkString(parDate).Contains("ccc.svg");
			// 6. LES TAILLES SONT VRAIMENT LUES (30, 10, 1 octets) -- sans ca, un tri par
			//    taille sur des zeros donnerait l'ordre des noms et passerait par hasard.
			int64 t1 = 0, t2 = 0, t3 = 0;
			{
				editorkit::NkFilePickerNavState n;
				char b[512] = {};
				n.OpenPickerBase(editorkit::NkSelecteurOuvrirFichier, base114.Data(), b,
					 (int32)sizeof(b), nullptr, nullptr);
				n.vue.sortCle = (uint8)editorkit::NkBrowserTri::Taille;
				n.relire = true;
				n.RelireDossier();
				if ((uint32)n.vue.entries.Size() >= 4u) {
					t1 = n.vue.entries[1].taille;
					t2 = n.vue.entries[2].taille;
					t3 = n.vue.entries[3].taille;
				}
			}
			const bool taillesLues = t1 > t2 && t2 > t3 && t3 > 0;
			snprintf(det, sizeof(det),
				"nom ↗ : %s ; nom ↘ : %s ; taille : %s ; type : %s ; dossier toujours en tete=%d ; nom=%d ; "
				"taille=%d (octets lus %lld > %lld > %lld -> %d) ; type=%d ; date complete=%d",
				parNom, parNomD, parTaille, parType, dossierEnTete ? 1 : 0, nomOk ? 1 : 0, tailleOk ? 1 : 0,
				(long long)t1, (long long)t2, (long long)t3, taillesLues ? 1 : 0, typeOk ? 1 : 0,
				dateOk ? 1 : 0);
			check("114. ⑤ LE TRI PAR NOM, DATE, TAILLE ET TYPE, DANS LES DEUX SENS : le tri par date et par taille etaient "
				"« nommes, non faits » -- le systeme de fichiers donnait deja `Size` et `ModificationTime`, c'est l'entree "
				"du navigateur qui ne les portait pas ; les tailles sont REELLEMENT lues (30 > 10 > 1 octets, l'inverse de "
				"l'ordre des noms, si bien qu'un tri par nom deguise se verrait) ; et LE DOSSIER reste en tete quelle que "
				"soit la cle et le sens",
				dossierEnTete && nomOk && tailleOk && typeOk && dateOk && taillesLues, det);
			NkDirectory::Delete("sonde_tri", true);
		}
		// ── 115. ⑥ LE NOM, L'INFOBULLE ET LA POIGNEE (05/09, nuit). Rodolf : « le panneau de
		//    gauche doit pouvoir etre agrandi, ou alors une infobulle doit donner le chemin
		//    complet » -- et sur sa capture, l'entree du dossier courant affiche
		//    `D:/Projets/…entseu-no…`, c'est-a-dire LE CHEMIN, pas le nom.
		//    ⚠️ J'avais ecrit que l'infobulle etait impossible sans toucher les quatre
		//       consommateurs du `tree_view`. C'ETAIT FAUX : un champ additif au defaut vide
		//       ne touche personne. La sonde le verifie aussi.
		{
			char det[880];
			NkDirectory::Delete("sonde_nom", true);
			NkDirectory::CreateRecursive("sonde_nom/un_sous_dossier");
			const NkString base115 = (NkPath(NkDirectory::GetCurrentDirectory()) / "sonde_nom").ToString();
			// 1. LE NOM NE REND JAMAIS LE CHEMIN, meme avec un separateur final
			NkString avecBarre = base115;
			avecBarre.Append('/');
			const NkString n1 = editorkit::NkFilePickerNavState::NomDeDossier(base115.Data());
			const NkString n2 = editorkit::NkFilePickerNavState::NomDeDossier(avecBarre.Data());
			const NkString n3 = editorkit::NkFilePickerNavState::NomDeDossier("D:/");
			const bool nomOk115 = NkComponentDecl::StrEq(n1.Data(), "sonde_nom")
					&& NkComponentDecl::StrEq(n2.Data(), "sonde_nom")
					&& !NkString(n2).Contains("/") && !n3.Empty() && n3.Length() <= 3u;
			// 2. L'ENTREE DU RAIL PORTE LE NOM, ET SON INFOBULLE PORTE LE CHEMIN
			editorkit::NkFilePickerNavState nav115;
			char b115[512] = {};
			nav115.OpenPickerBase(editorkit::NkSelecteurOuvrirDossier, avecBarre.Data(), b115,
					  (int32)sizeof(b115), nullptr, nullptr);
			// ⑤ (05/09, v5) LE RAIL NE MONTRE PLUS LE DOSSIER OU L'ON NAVIGUE : depuis que
			// le repli `courant = pickerPath` est retire, la section « Dossier courant » ne
			// porte QUE le dossier d'une operation REUSSIE (definition de Rodolf). Le temoin
			// fait donc ce que fait l'application apres une reussite -- il appelle la MEME
			// porte -- au lieu de compter sur un repli qui mentait.
			editorkit::NkFilePickerNavState::PoserRecent(nav115.recents, nav115.Dossier());
			nav115.relire = true;
			nav115.RelireDossier();
			bool trouve = false, libelleCourt = false, bulleComplete = false;
			for (uint32 i = 0; i < (uint32)nav115.vue.folders.nodes.Size() && !trouve; ++i) {
				const editorkit::NkTreeNode &n = nav115.vue.folders.nodes[i];
				if (n.path.Empty()
					|| !editorkit::NkFilePickerState::PathSame(n.path.Data(), nav115.Dossier()))
					continue;
				trouve = true;
				libelleCourt = !NkString(n.label).Contains("/") && !NkString(n.label).Contains("\\");
				bulleComplete = !n.infobulle.Empty()
					&& editorkit::NkFilePickerState::PathSame(n.infobulle.Data(), nav115.Dossier());
			}
			// 3. TOUTES LES ENTREES DE CHEMIN ONT LEUR INFOBULLE, et les TITRES n'en ont pas
			uint32 avecBulle = 0u, titresAvecBulle = 0u;
			for (uint32 i = 0; i < (uint32)nav115.vue.folders.nodes.Size(); ++i) {
				const editorkit::NkTreeNode &n = nav115.vue.folders.nodes[i];
				if (n.path.Empty()) {
					if (!n.infobulle.Empty())
						++titresAvecBulle;
				} else if (!n.infobulle.Empty())
					++avecBulle;
			}
			const bool bullesPartout = avecBulle > 3u && titresAvecBulle == 0u;
			// 4. LE DEFAUT DU CHAMP EST VIDE : un noeud neuf n'a pas d'infobulle, donc les
			//    quatre consommateurs du `tree_view` ne changent pas de rendu.
			const editorkit::NkTreeNode neuf115;
			const bool defautVide = neuf115.infobulle.Empty();
			// 5. LA POIGNEE : la largeur est RETENUE d'une image a l'autre, et bornee
			nav115.largeurRail = 320.f;
			const float32 gardee = nav115.largeurRail;
			if (nav115.largeurRail < editorkit::NkFilePickerNavState::kRailMin)
				nav115.largeurRail = editorkit::NkFilePickerNavState::kRailMin;
			const bool retenue = gardee == 320.f && nav115.largeurRail == 320.f;
			snprintf(det, sizeof(det),
				"nom : « %s » / « %s » (avec barre finale) / racine « %s » -> %d ; entree du dossier courant "
				"trouvee=%d, libelle sans separateur=%d, infobulle = le chemin complet=%d ; %u entree(s) avec "
				"infobulle, %u titre(s) qui en portent une (0 attendu) -> %d ; defaut du champ vide=%d ; "
				"largeur %.0f retenue et non bornee=%d",
				n1.Data(), n2.Data(), n3.Data(), nomOk115 ? 1 : 0, trouve ? 1 : 0, libelleCourt ? 1 : 0,
				bulleComplete ? 1 : 0, avecBulle, titresAvecBulle, bullesPartout ? 1 : 0,
				defautVide ? 1 : 0, (double)nav115.largeurRail, retenue ? 1 : 0);
			check("115. ⑥ LE RAIL MONTRE UN NOM, ET SON INFOBULLE LE CHEMIN : `GetFileName` rend VIDE quand le chemin finit "
				"par un separateur, et le repli prenait alors le CHEMIN ENTIER -- Rodolf lisait `D:/Projets/…entseu-no…` a "
				"la place d'un nom ; un repli qui remplace un nom par un chemin n'est pas un repli. Chaque entree de chemin "
				"porte desormais son chemin complet en infobulle (champ ADDITIF du `tree_view`, defaut vide : les quatre "
				"consommateurs ne changent pas), les TITRES de section n'en ont pas, et la largeur du rail est retenue",
				nomOk115 && trouve && libelleCourt && bulleComplete && bullesPartout && defautVide && retenue,
				det);
			NkDirectory::Delete("sonde_nom", true);
		}
		/// ① UN PEINTRE QUI REND UN VRAI AMBRE. `NkRecordingPaint::ColorOf` rend une
		/// couleur synthetique par role : le masque fautif y gardait par hasard la meme
		/// dominante, et la sonde restait verte sous la mutation. Un temoin qui teste une
		/// TRANSFORMATION DE COULEUR doit lui donner une vraie couleur.
		class NkPeintreAmbre : public NkRecordingPaint {
			public:
				uint32 ColorOf(uint16 role) const override {
					(void)role;
					return 0xF7A02CFFu; // l'ambre du costume, en 0xRRGGBBAA
				}
		};
		
		// ── 116. ① LE DOSSIER : UNE SEULE TEINTE, TROIS NUANCES (05/09, nuit). Rodolf :
		//    « le design de dossier ne me plait pas » -- un corps ambre surmonte d'un petit
		//    onglet VERT decale.
		//    CAUSE MESUREE : `pale = (teinte & 0x00FFFFFF) | 0x66000000` supposait un
		//    empaquetage `0xAARRGGBB`. Il est `0xRRGGBBAA` (`Unpack` lit le ROUGE dans les
		//    bits 24-31) : je mettais donc le ROUGE A ZERO et l'alpha dans la case du rouge.
		//    D'un ambre, il restait du vert. Une teinte ne se bricole pas au masque.
		{
			char det[880];
			// L'AMBRE du costume, pose sur un role connu de la sonde
			auto rendre = [](editorkit::NkAssetIcone g, float32 taille, uint32 *couleurs, uint32 &n,
					  uint32 &formes) -> uint64 {
				NkPeintreAmbre r;
				NkContentBrowserModel m;
				NkAssetEntry a;
				a.name = NkString("x");
				a.icone = (uint8)g;
				a.isFolder = (g >= editorkit::NkAssetIcone::Dossier
					  && g <= editorkit::NkAssetIcone::DossierBureau);
				a.kindLabel = "";
				m.entries.PushBack(a);
				m.thumbSize = taille;
				NkContentBrowserHooks h;
				NkComponentInput in;
				NkDrawContentBrowser(r, in, {0.f, 0.f, taille * 4.f + 200.f, taille * 3.f + 200.f}, m,
					 DemoStyle(nullptr), h);
				uint64 e = 0u;
				n = 0u;
				formes = 0u;
				for (uint32 i = 0; i < (uint32)r.cmds.Size(); ++i) {
					const NkPaintCmd &c = r.cmds[i];
					if (c.op != NkPaintOp::FillColor || c.w > taille * 1.2f)
						continue; // on ne garde que les formes DE L'ICONE
					++formes;
					bool vue = false;
					for (uint32 k = 0; k < n; ++k)
						if (couleurs[k] == c.rgba)
							vue = true;
					if (!vue && n < 12u)
						couleurs[n++] = c.rgba;
					e = e * 1099511628211ull
						+ (uint64)((uint32)(c.x * 4.f) * 31u + (uint32)(c.y * 4.f) * 17u
								   + (uint32)(c.w * 4.f) * 13u + (uint32)(c.h * 4.f));
				}
				return e;
			};
			// 1. UNE SEULE TEINTE : toutes les nuances du dossier ont la MEME dominante.
			//    On compare les composantes : celle qui domine dans le corps doit dominer
			//    dans la patte. Un vert sur un ambre se verrait immediatement.
			uint32 coul[12];
			uint32 nCoul = 0u, nFormes = 0u;
			rendre(editorkit::NkAssetIcone::Dossier, 96.f, coul, nCoul, nFormes);
			bool memeFamille = nCoul >= 2u;
			uint32 dominanteRef = 9u;
			for (uint32 k = 0; k < nCoul && memeFamille; ++k) {
				const uint32 rr = (coul[k] >> 24) & 0xFFu, gg = (coul[k] >> 16) & 0xFFu;
				const uint32 bb = (coul[k] >> 8) & 0xFFu;
				const uint32 dom = (rr >= gg && rr >= bb) ? 0u : ((gg >= bb) ? 1u : 2u);
				if (dominanteRef == 9u)
					dominanteRef = dom;
				else if (dom != dominanteRef)
					memeFamille = false; // une nuance a change de dominante : c'est le defaut
			}
			// 2. TROIS NUANCES AU MOINS (arriere, avant, liseré) : c'est ce qui donne la
			//    profondeur. Deux seulement, et le dossier redevient plat.
			const bool troisNuances = nCoul >= 3u;
			// 3. IL SE LIT A 32, 64 ET 128 : a chaque taille, son empreinte DIFFERE de celle
			//    du fichier generique -- et il garde au moins quatre formes.
			uint32 tailles[3] = {32u, 64u, 128u};
			bool distinctPartout = true, assezDeFormes = true;
			uint32 formesMin = 999u;
			for (int32 k = 0; k < 3; ++k) {
				uint32 c1[12], c2[12], n1 = 0u, n2 = 0u, f1 = 0u, f2 = 0u;
				const uint64 eDoss = rendre(editorkit::NkAssetIcone::Dossier, (float32)tailles[k], c1, n1, f1);
				const uint64 eFich = rendre(editorkit::NkAssetIcone::Inconnu, (float32)tailles[k], c2, n2, f2);
				if (eDoss == eFich)
					distinctPartout = false;
				if (f1 < formesMin)
					formesMin = f1;
				if (f1 < 4u)
					assezDeFormes = false;
			}
			// 4. LA PATTE FAIT ~40 % DE LA LARGEUR et ne monte pas plus de 2 unites sur 16 :
			//    une patte trop haute fait un drapeau, pas un dossier. Mesure directe sur
			//    la geometrie emise a 160 px, ou l'unite vaut 10.
			float32 largePatte = 0.f, largeCorps = 0.f, hautPatte = 0.f;
			{
				NkPeintreAmbre r;
				NkContentBrowserModel m;
				NkAssetEntry a;
				a.name = NkString("x");
				a.icone = (uint8)editorkit::NkAssetIcone::Dossier;
				a.isFolder = true;
				a.kindLabel = "";
				m.entries.PushBack(a);
				m.thumbSize = 160.f;
				NkContentBrowserHooks h;
				NkComponentInput in;
				NkDrawContentBrowser(r, in, {0.f, 0.f, 900.f, 700.f}, m, DemoStyle(nullptr), h);
				float32 yMin = 1e9f, yCorps = 1e9f;
				for (uint32 i = 0; i < (uint32)r.cmds.Size(); ++i) {
					const NkPaintCmd &c = r.cmds[i];
					if (c.op != NkPaintOp::FillColor || c.w > 200.f || c.w < 10.f)
						continue;
					if (c.y < yMin) { yMin = c.y; largePatte = c.w; }
					if (c.w > largeCorps) { largeCorps = c.w; yCorps = c.y; }
				}
				hautPatte = yCorps - yMin;
			}
			const float32 ratio = largeCorps > 0.f ? largePatte / largeCorps : 0.f;
			const bool patteJuste = ratio > 0.3f && ratio < 0.5f && hautPatte > 4.f && hautPatte < 20.f;
			snprintf(det, sizeof(det),
				"dossier a 96 px : %u nuance(s), %u forme(s) ; toutes de la meme dominante (%u=R,G,B)=%d ; "
				"trois nuances=%d ; a 32/64/128 px : distinct du fichier generique=%d, %u forme(s) minimum "
				"-> %d ; patte %.0f sur un corps de %.0f (%.0f %%) montant de %.0f px -> %d",
				nCoul, nFormes, dominanteRef, memeFamille ? 1 : 0, troisNuances ? 1 : 0,
				distinctPartout ? 1 : 0, formesMin, assezDeFormes ? 1 : 0, (double)largePatte,
				(double)largeCorps, (double)(ratio * 100.f), (double)hautPatte, patteJuste ? 1 : 0);
			check("116. ① LE DOSSIER A UNE SEULE TEINTE ET TROIS NUANCES : l'onglet etait VERT sur un corps ambre parce que "
				"la nuance se calculait au masque en supposant `0xAARRGGBB` alors que l'empaquetage est `0xRRGGBBAA` -- le "
				"rouge partait a zero. Toutes les nuances ont desormais la MEME dominante, il y en a au moins trois "
				"(rabat arriere, rabat avant, liseré : c'est ce qui donne la profondeur), la patte fait ~40 %% du corps et "
				"ne monte que de quelques pixels, et la silhouette reste distincte du fichier generique a 32, 64 et 128 px",
				memeFamille && troisNuances && distinctPartout && assezDeFormes && patteJuste, det);
		}
		// ── 117. ② LE RAIL A LES MEMES ICONES QUE LA GRILLE (05/09, nuit). Rodolf : « le
		//    panneau de gauche ne montre pas les icones associees aux differents dossiers » --
		//    le rail etait du TEXTE NU pendant que la grille avait ses onze silhouettes.
		//    ⚠️ LE TEMOIN COMPARE LES DEUX VOLETS SUR LE MEME DOSSIER : c'est la seule
		//       facon de prouver qu'ils passent par la MEME fonction. Deux tables auraient
		//       diverge des le premier ajout de nature -- et c'est exactement ce qui venait
		//       d'arriver.
		{
			char det[880];
			NkDirectory::Delete("sonde_rail_icones", true);
			NkDirectory::CreateRecursive("sonde_rail_icones/un_dossier");
			NkFile::WriteAllText("sonde_rail_icones/a.png", "x");
			const NkString base117 =
				(NkPath(NkDirectory::GetCurrentDirectory()) / "sonde_rail_icones").ToString();
			editorkit::NkFilePickerNavState nav117;
			char b117[512] = {};
			nav117.OpenPickerBase(editorkit::NkSelecteurOuvrirDossier, base117.Data(), b117,
					  (int32)sizeof(b117), nullptr, nullptr);
			// ⑤ (05/09, v5) le rail ne deplie que le dossier d'une operation REUSSIE : le
			// temoin passe donc par la MEME porte que l'application avant de comparer.
			editorkit::NkFilePickerNavState::PoserRecent(nav117.recents, nav117.Dossier());
			nav117.relire = true;
			nav117.RelireDossier();
			// 1. LA MEME ENTREE, LA MEME ICONE dans les deux volets
			const NkString cheminSous = (NkPath(base117.Data()) / "un_dossier").ToString();
			uint8 icGrille = 255u, icRail = 255u;
			for (uint32 i = 0; i < (uint32)nav117.vue.entries.Size(); ++i)
				if (editorkit::NkFilePickerState::PathSame(nav117.vue.entries[i].path.Data(),
																	   cheminSous.Data()))
					icGrille = nav117.vue.entries[i].icone;
			for (uint32 i = 0; i < (uint32)nav117.vue.folders.nodes.Size(); ++i)
				if (!nav117.vue.folders.nodes[i].path.Empty()
					&& editorkit::NkFilePickerState::PathSame(nav117.vue.folders.nodes[i].path.Data(),
																			 cheminSous.Data()))
					icRail = nav117.vue.folders.nodes[i].silhouette;
			const bool memeIcone = icGrille != 255u && icGrille == icRail
					&& icGrille == (uint8)editorkit::NkAssetIcone::Dossier;
			// 2. LES VOLUMES ONT L'ICONE DE VOLUME, pas celle d'un dossier
			uint32 volumes = 0u, volumesBienIconises = 0u;
			for (uint32 i = 0; i < (uint32)nav117.vue.folders.nodes.Size(); ++i) {
				const editorkit::NkTreeNode &n = nav117.vue.folders.nodes[i];
				if (n.path.Empty() || n.label.Length() > 2u
					|| n.label.CStr()[n.label.Length() - 1u] != ':')
					continue;
				++volumes;
				if (n.silhouette == (uint8)editorkit::NkAssetIcone::Volume)
					++volumesBienIconises;
			}
			const bool volumesOk = volumes > 0u && volumes == volumesBienIconises;
			// 3. LES TITRES ONT L'ICONE DE SECTION
			// ⑤ (05/09, v5) LE CRITERE EST LE DRAPEAU `bandeau`, PAS « sans chemin ». Les
			//    deux coincidaient ; ils ont diverge le jour ou la section « Dossier courant »
			//    a recu une PHRASE (« aucun -- apres un enregistrement... »), qui n'a pas de
			//    chemin sans etre un titre. C'est exactement ce que `NkTreeNode::bandeau`
			//    annonce dans son commentaire : deux notions qui coincident divergeront.
			uint32 titres = 0u, titresOk = 0u;
			for (uint32 i = 0; i < (uint32)nav117.vue.folders.nodes.Size(); ++i) {
				const editorkit::NkTreeNode &n = nav117.vue.folders.nodes[i];
				if (!n.bandeau)
					continue;
				++titres;
				if (n.silhouette == (uint8)editorkit::NkAssetIcone::Section)
					++titresOk;
			}
			const bool titresIconises = titres >= 2u && titres == titresOk;
			// 4. ET LE RAIL LES PEINT VRAIMENT : on dessine un arbre d'un noeud, et on
			//    compte les formes. Sans silhouette, `p.Icon` n'emet qu'UNE commande.
			auto formesRail = [](uint8 sil) -> uint32 {
				NkRecordingPaint r;
				NkTreeViewModel m;
				NkTreeNode n;
				n.id = 1u;
				n.parent = -1;
				n.label = NkString("un_dossier");
				n.path = NkString("/x");
				n.silhouette = sil;
				m.nodes.PushBack(n);
				NkTreeViewStyle st;
				NkTreeViewHooks h;
				NkComponentInput in;
				NkDrawTreeView(r, in, {0.f, 0.f, 260.f, 200.f}, m, st, h);
				uint32 f = 0u;
				for (uint32 i = 0; i < (uint32)r.cmds.Size(); ++i)
					if (r.cmds[i].op == NkPaintOp::FillColor)
						++f;
				return f;
			};
			const uint32 sans = formesRail((uint8)editorkit::NkAssetIcone::Auto);
			const uint32 avec = formesRail((uint8)editorkit::NkAssetIcone::Dossier);
			const bool railPeint = avec >= sans + 3u; // le dossier ajoute ses quatre formes
			snprintf(det, sizeof(det),
				"meme sous-dossier : icone grille %u, icone rail %u -> %d ; %u volume(s), %u avec l'icone "
				"de volume -> %d ; %u titre(s), %u avec l'icone de section -> %d ; formes peintes par le rail : "
				"%u sans silhouette, %u avec -> %d",
				icGrille, icRail, memeIcone ? 1 : 0, volumes, volumesBienIconises, volumesOk ? 1 : 0,
				titres, titresOk, titresIconises ? 1 : 0, sans, avec, railPeint ? 1 : 0);
			check("117. ② LE RAIL ET LA GRILLE MONTRENT LA MEME ICONE POUR LA MEME ENTREE : une SEULE fonction "
				"(`IconePour`) les donne aux deux volets -- deux tables auraient diverge des le premier ajout de nature, "
				"et c'est exactement ce qui venait d'arriver (onze formes a droite, du texte nu a gauche) ; les VOLUMES ont "
				"leur boitier et non un dossier, les TITRES de section leur propre forme, et le rail les PEINT vraiment "
				"(le nombre de formes emises augmente quand le noeud porte une silhouette)",
				memeIcone && volumesOk && titresIconises && railPeint, det);
			NkDirectory::Delete("sonde_rail_icones", true);
		}
		// ── 118. ③④ LA BANDE DE SECTION ET LES CHEVRONS (05/09, nuit). Rodolf : « pour
		//    distinguer "Dossier courant" des autres, on doit avoir une barre differente pour
		//    ce titre, donc plus sombre », et « on doit aussi avoir les chevrons pour
		//    ouvrir/fermer, et ce n'est pas encore le cas ».
		//    CAUSE DU CHEVRON MANQUANT : `p.Icon(chev, s.icons.chevronOpen, ...)` -- et cette
		//    poignee vaut ZERO chez un hote sans atlas. Le chevron etait DEMANDE et peint
		//    nulle part. Exactement la meme cause que les icones du rail.
		{
			char det[880];
			auto dessiner = [](bool bandeau, bool enfants, bool ouvert, uint32 &fills, uint32 &traits,
					   uint32 &polys, float32 &largeurMax) {
				NkRecordingPaint r;
				NkTreeViewModel m;
				NkTreeNode n;
				n.id = 1u;
				n.parent = -1;
				n.label = NkString("Section");
				n.bandeau = bandeau;
				n.enfantsPossibles = enfants;
				m.nodes.PushBack(n);
				if (ouvert)
					m.SetOpen(1u, true, false);
				NkTreeViewStyle st;
				NkTreeViewHooks h;
				NkComponentInput in;
				NkDrawTreeView(r, in, {0.f, 0.f, 260.f, 200.f}, m, st, h);
				fills = traits = polys = 0u;
				largeurMax = 0.f;
				for (uint32 i = 0; i < (uint32)r.cmds.Size(); ++i) {
					const NkPaintCmd &c = r.cmds[i];
					if (c.op == NkPaintOp::FillColor) {
						++fills;
						if (c.w > largeurMax)
							largeurMax = c.w;
					} else if (c.op == NkPaintOp::Line)
						++traits;
				}
			};
			// 1. LA BANDE : un en-tete emet un remplissage SUR TOUTE LA LARGEUR ; un noeud
			//    ordinaire n'en emet pas.
			uint32 fA = 0u, tA = 0u, pA = 0u, fB = 0u, tB = 0u, pB = 0u;
			float32 lA = 0.f, lB = 0.f;
			dessiner(true, false, false, fA, tA, pA, lA);
			dessiner(false, false, false, fB, tB, pB, lB);
			const bool bandeOk = fA > fB && lA >= 240.f && lB < 240.f;
			// 2. LE CHEVRON : un noeud qui ANNONCE des enfants emet des traits de plus ;
			//    un noeud sans enfant n'en emet pas. ⚠️ Le controle negatif est la moitie
			//    du temoin : un chevron partout serait aussi faux qu'un chevron nulle part.
			uint32 fC = 0u, tC = 0u, pC = 0u;
			float32 lC = 0.f;
			dessiner(false, true, false, fC, tC, pC, lC);
			const bool chevronPresent = tC > tB;
			// 3. FERME ET OUVERT NE SE DESSINENT PAS PAREIL
			uint32 fD = 0u, tD = 0u, pD = 0u;
			float32 lD = 0.f;
			dessiner(false, true, true, fD, tD, pD, lD);
			// meme nombre de traits, mais des positions differentes : on compare les
			// empreintes plutot que les comptes.
			auto empreinteChevron = [](bool ouvert) -> uint64 {
				NkRecordingPaint r;
				NkTreeViewModel m;
				NkTreeNode n;
				n.id = 1u;
				n.parent = -1;
				n.label = NkString("S");
				n.enfantsPossibles = true;
				m.nodes.PushBack(n);
				if (ouvert)
					m.SetOpen(1u, true, false);
				NkTreeViewStyle st;
				NkTreeViewHooks h;
				NkComponentInput in;
				NkDrawTreeView(r, in, {0.f, 0.f, 260.f, 200.f}, m, st, h);
				uint64 e = 0u;
				for (uint32 i = 0; i < (uint32)r.cmds.Size(); ++i)
					if (r.cmds[i].op == NkPaintOp::Line)
						e = e * 1099511628211ull + (uint64)((uint32)(r.cmds[i].x * 8.f) * 31u
																   + (uint32)(r.cmds[i].y * 8.f));
				return e;
			};
			const bool ouvertDifferent = empreinteChevron(false) != empreinteChevron(true);
			// 4. AUCUN GLYPHE : le chevron n'ajoute pas de commande de texte
			uint32 texteAvec = 0u, texteSans = 0u;
			{
				auto textes = [](bool enfants) -> uint32 {
					NkRecordingPaint r;
					NkTreeViewModel m;
					NkTreeNode n;
					n.id = 1u;
					n.parent = -1;
					n.label = NkString("S");
					n.enfantsPossibles = enfants;
					m.nodes.PushBack(n);
					NkTreeViewStyle st;
					NkTreeViewHooks h;
					NkComponentInput in;
					NkDrawTreeView(r, in, {0.f, 0.f, 260.f, 200.f}, m, st, h);
					uint32 t2 = 0u;
					for (uint32 i = 0; i < (uint32)r.cmds.Size(); ++i)
						if (r.cmds[i].op == NkPaintOp::Text)
							++t2;
					return t2;
				};
				texteAvec = textes(true);
				texteSans = textes(false);
			}
			const bool aucunGlyphe118 = texteAvec == texteSans;
			snprintf(det, sizeof(det),
				"bande : %u remplissage(s) large de %.0f pour un en-tete, %u de %.0f pour un noeud "
				"ordinaire -> %d ; chevron : %u trait(s) avec enfants contre %u sans -> %d ; ferme et "
				"ouvert differents=%d ; commandes de texte %u avec / %u sans -> aucun glyphe=%d",
				fA, (double)lA, fB, (double)lB, bandeOk ? 1 : 0, tC, tB, chevronPresent ? 1 : 0,
				ouvertDifferent ? 1 : 0, texteAvec, texteSans, aucunGlyphe118 ? 1 : 0);
			check("118. ③④ LA BANDE DE SECTION ET LE CHEVRON SONT PEINTS : un en-tete se peint sur un remplissage PLEINE "
				"LARGEUR (une nuance du fond du panneau, assombrie -- aucune teinte inventee, elle suit le theme clair "
				"comme sombre) et un noeud ordinaire n'en a pas ; un noeud qui ANNONCE des enfants montre un chevron "
				"TRACE, un noeud sans enfant n'en montre pas (le controle negatif est la moitie du temoin), ferme et "
				"ouvert ne se dessinent pas pareil, et rien de tout cela n'est un glyphe de police -- la poignee d'atlas "
				"valait ZERO chez un hote sans atlas, et le chevron etait demande puis peint nulle part",
				bandeOk && chevronPresent && ouvertDifferent && aucunGlyphe118, det);
		}
		// ── 119. ⑤ UN FICHIER RECONNU ET UN INCONNU N'ONT PAS LE MEME DESIGN (05/09, nuit).
		//    Rodolf : « meme le design des fichiers au format reconnu ou non doivent avoir un
		//    design specifique. »
		//    ⚠️ LA RECONNAISSANCE VIENT DE LA TABLE DES FILTRES, pas d'une seconde table
		//       d'extensions : une extension listee dans un groupe NOMME est reconnue. Et le
		//       groupe « Tous les fichiers » ne reconnait RIEN -- il laisse passer, il ne
		//       qualifie pas. Sans cette regle, tout serait reconnu des qu'on l'offre.
		{
			char det[900];
			NkDirectory::Delete("sonde_familles", true);
			NkDirectory::CreateRecursive("sonde_familles");
			static const char *kFic[7] = {"a.png", "b.txt", "c.cpp", "d.zip",
												  "e.exe", "f.svg", "g.wat"};
			for (int32 k = 0; k < 7; ++k) {
				char ch[128];
				snprintf(ch, sizeof(ch), "sonde_familles/%s", kFic[k]);
				NkFile::WriteAllText(ch, "x");
			}
			const NkString base119 =
				(NkPath(NkDirectory::GetCurrentDirectory()) / "sonde_familles").ToString();
			editorkit::NkFilePickerNavState nav119;
			char b119[512] = {};
			nav119.OpenPickerBase(editorkit::NkSelecteurOuvrirFichier, base119.Data(), b119,
					  (int32)sizeof(b119), nullptr, nullptr);
			// SIX FAMILLES NOMMEES, plus « Tous » qui ne qualifie rien
			nav119.AjouterFiltre("Images", "png;svg");
			nav119.AjouterFiltre("Textes", "txt");
			nav119.AjouterFiltre("Code", "cpp");
			nav119.AjouterFiltre("Archives", "zip");
			nav119.AjouterFiltre("Executables", "exe");
			nav119.AjouterFiltre("Tous les fichiers", "*");
			nav119.filtreActif = 5; // « Tous » : on veut VOIR les sept
			nav119.rolesFamille[(uint32)editorkit::NkAssetIcone::Image] = 20u;
			nav119.rolesFamille[(uint32)editorkit::NkAssetIcone::Texte] = 21u;
			nav119.rolesFamille[(uint32)editorkit::NkAssetIcone::Code] = 22u;
			nav119.rolesFamille[(uint32)editorkit::NkAssetIcone::Archive] = 23u;
			nav119.rolesFamille[(uint32)editorkit::NkAssetIcone::Executable] = 24u;
			nav119.roleFichier = 25u;
			nav119.relire = true;
			nav119.RelireDossier();
			// 1. SEPT ENTREES, ET SEPT COUPLES (silhouette, role) DISTINCTS
			uint32 n119 = (uint32)nav119.vue.entries.Size();
			uint32 couples[16];
			uint32 nCouples = 0u;
			char resume[300];
			resume[0] = '\0';
			for (uint32 i = 0; i < n119 && i < 16u; ++i) {
				const NkAssetEntry &e = nav119.vue.entries[i];
				const uint32 cle = (uint32)e.icone * 1000u + e.kindRole;
				bool vu = false;
				for (uint32 k = 0; k < nCouples; ++k)
					if (couples[k] == cle)
						vu = true;
				if (!vu)
					couples[nCouples++] = cle;
				const usize l = NkString(resume).Length();
				snprintf(resume + l, sizeof(resume) - l, "%s%s:%u/%u", l ? " " : "",
					 e.name.Data(), (uint32)e.icone, e.kindRole);
			}
			// png et svg partagent la famille Image : six couples pour sept fichiers
			const bool famillesDistinctes = n119 == 7u && nCouples == 6u;
			// 2. L'INCONNU EST BIEN INCONNU, et il porte le role NEUTRE
			bool inconnuOk = false;
			for (uint32 i = 0; i < n119; ++i)
				if (NkComponentDecl::StrEq(nav119.vue.entries[i].name.Data(), "g.wat"))
					inconnuOk = nav119.vue.entries[i].icone == (uint8)editorkit::NkAssetIcone::Inconnu
						&& nav119.vue.entries[i].kindRole == 25u;
			// 3. « TOUS LES FICHIERS » NE QUALIFIE RIEN : avec LUI SEUL, tout est inconnu.
			editorkit::NkFilePickerNavState nav120;
			char b120[512] = {};
			nav120.OpenPickerBase(editorkit::NkSelecteurOuvrirFichier, base119.Data(), b120,
					  (int32)sizeof(b120), nullptr, nullptr);
			nav120.AjouterFiltre("Tous les fichiers", "*");
			nav120.relire = true;
			nav120.RelireDossier();
			uint32 inconnus = 0u;
			for (uint32 i = 0; i < (uint32)nav120.vue.entries.Size(); ++i)
				if (nav120.vue.entries[i].icone == (uint8)editorkit::NkAssetIcone::Inconnu)
					++inconnus;
			const bool toutNeQualifiePas = inconnus == 7u;
			// 4. LE BADGE : peint pour un reconnu, PAS de pastille pour un inconnu
			auto compterBadges = [](uint8 icone, uint16 role) -> uint32 {
				NkRecordingPaint r;
				NkContentBrowserModel m;
				NkAssetEntry a;
				a.name = NkString("x.png");
				a.icone = icone;
				a.kindRole = role;
				a.kindLabel = "PNG";
				m.entries.PushBack(a);
				NkContentBrowserHooks h;
				NkComponentInput in;
				NkDrawContentBrowser(r, in, {0.f, 0.f, 500.f, 400.f}, m, DemoStyle(nullptr), h);
				uint32 n = 0u;
				for (uint32 i = 0; i < (uint32)r.cmds.Size(); ++i)
					if (r.cmds[i].op == NkPaintOp::Fill && r.cmds[i].role == role)
						++n;
				return n;
			};
			const uint32 badgeConnu = compterBadges((uint8)editorkit::NkAssetIcone::Image, 30u);
			const uint32 badgeInconnu = compterBadges((uint8)editorkit::NkAssetIcone::Inconnu, 30u);
			const bool badgeOk = badgeConnu > badgeInconnu && badgeInconnu == 0u;
			snprintf(det, sizeof(det),
				"%u fichier(s) : %s ; %u couple(s) (silhouette, teinte) distincts pour six familles -> %d ; "
				"l'inconnu `g.wat` a la silhouette Inconnu et la teinte neutre=%d ; avec « Tous » SEUL, "
				"%u/7 inconnus -> %d ; pastille : %u pour un reconnu, %u pour un inconnu -> %d",
				n119, resume, nCouples, famillesDistinctes ? 1 : 0, inconnuOk ? 1 : 0, inconnus,
				toutNeQualifiePas ? 1 : 0, badgeConnu, badgeInconnu, badgeOk ? 1 : 0);
			check("119. ⑤ UN FICHIER RECONNU ET UN INCONNU N'ONT PAS LE MEME DESIGN : la reconnaissance vient de LA TABLE "
				"DES FILTRES -- une extension listee dans un groupe NOMME est reconnue -- et non d'une seconde table "
				"d'extensions qui aurait fini par annoncer une famille que le filtre ne laisse pas passer ; le groupe "
				"« Tous les fichiers » ne qualifie RIEN (il laisse passer, il ne nomme pas) ; l'inconnu prend la "
				"silhouette neutre et la teinte de repli, et il n'a PAS de pastille coloree -- une pastille annonce une "
				"famille, et l'ignorance n'en est pas une",
				famillesDistinctes && inconnuOk && toutNeQualifiePas && badgeOk, det);
			NkDirectory::Delete("sonde_familles", true);
		}
		// ── 120. ⑥ LE DOSSIER COURANT NE BOUGE QUE SUR SUCCES (05/09, nuit). Rodolf donne
		//    sa definition, et ce n'etait pas celle que j'avais implementee : « un dossier
		//    est appele dossier courant si on a REUSSI a sauvegarder ou a charger un fichier
		//    de ce dossier-la. » Ce n'est donc pas le dossier ou l'on navigue.
		//    ⚠️ UNE SEULE SOURCE : c'est LA TETE DES RECENTS, deja tenue, et deja mise a
		//       jour uniquement apres une ecriture reussie. Deux etats auraient dit deux
		//       choses -- exactement le defaut ① du matin.
		{
			char det[880];
			NkDirectory::Delete("sonde_courant", true);
			NkDirectory::CreateRecursive("sonde_courant/ici");
			NkDirectory::CreateRecursive("sonde_courant/ailleurs");
			const NkString racine120 =
				(NkPath(NkDirectory::GetCurrentDirectory()) / "sonde_courant").ToString();
			const NkString dIci = (NkPath(racine120.Data()) / "ici").ToString();
			const NkString dAilleurs = (NkPath(racine120.Data()) / "ailleurs").ToString();
			editorkit::NkFilePickerNavState nav120b;
			char b120b[512] = {};
			nav120b.OpenPickerBase(editorkit::NkSelecteurEnregistrer, racine120.Data(), b120b,
					   (int32)sizeof(b120b), nullptr, nullptr);
			// 1. TANT QU'AUCUNE OPERATION N'A REUSSI, il n'y a pas de dossier courant.
			const bool videAuDepart = nav120b.DossierCourant()[0] == '\0';
			// 2. UNE NAVIGATION NE LE CHANGE PAS -- c'est tout l'objet de la definition.
			nav120b.AllerA(dIci.Data());
			nav120b.RelireDossier();
			const bool navigationNeChangeRien = nav120b.DossierCourant()[0] == '\0';
			// 3. UNE OPERATION REUSSIE le pose. On passe par la MEME porte que
			//    l'application (`PoserRecent`), celle qui n'est appelee qu'apres un succes.
			editorkit::NkFilePickerNavState::PoserRecent(nav120b.recents, dIci.Data());
			const bool poseParLeSucces =
				editorkit::NkFilePickerState::PathSame(nav120b.DossierCourant(), dIci.Data());
			// 4. NAVIGUER AILLEURS SANS ENREGISTRER NE LE CHANGE PAS
			nav120b.AllerA(dAilleurs.Data());
			nav120b.RelireDossier();
			const bool toujoursIci =
				editorkit::NkFilePickerState::PathSame(nav120b.DossierCourant(), dIci.Data());
			// 5. UNE SECONDE REUSSITE AILLEURS le deplace, et l'ancien passe aux recents.
			editorkit::NkFilePickerNavState::PoserRecent(nav120b.recents, dAilleurs.Data());
			const bool deplace =
				editorkit::NkFilePickerState::PathSame(nav120b.DossierCourant(), dAilleurs.Data())
				&& (uint32)nav120b.recents.Size() == 2u
				&& editorkit::NkFilePickerState::PathSame(nav120b.recents[1].Data(), dIci.Data());
			// 6. LA SECTION « Dossier courant » NE MONTRE QUE LUI (une entree racine), et
			//    l'ancien est descendu dans « Recents ».
			nav120b.relire = true;
			nav120b.RelireDossier();
			int32 iCourant120 = -1, iRecents120 = -1;
			for (uint32 i = 0; i < (uint32)nav120b.vue.folders.nodes.Size(); ++i) {
				const char *l = nav120b.vue.folders.nodes[i].label.Data();
				if (l && NkComponentDecl::StrEq(l, "Dossier courant")) iCourant120 = (int32)i;
				else if (l && NkComponentDecl::StrEq(l, "R\u00e9cents")) iRecents120 = (int32)i;
			}
			uint32 sousCourant = 0u, sousRecents = 0u;
			bool bonCourant = false;
			for (uint32 i = 0; i < (uint32)nav120b.vue.folders.nodes.Size(); ++i) {
				const editorkit::NkTreeNode &n = nav120b.vue.folders.nodes[i];
				if (n.parent == iCourant120) {
					++sousCourant;
					if (editorkit::NkFilePickerState::PathSame(n.path.Data(), dAilleurs.Data()))
						bonCourant = true;
				} else if (n.parent == iRecents120)
					++sousRecents;
			}
			const bool sectionsOk120 = sousCourant == 1u && bonCourant && sousRecents == 1u;
			snprintf(det, sizeof(det),
				"vide au depart=%d ; une NAVIGATION ne le change pas=%d ; une operation REUSSIE le pose=%d ; "
				"naviguer ailleurs sans enregistrer ne le change pas=%d ; une seconde reussite le deplace et "
				"l'ancien passe aux recents=%d ; « Dossier courant » porte %u entree (la bonne=%d) et "
				"« R\u00e9cents » %u -> %d",
				videAuDepart ? 1 : 0, navigationNeChangeRien ? 1 : 0, poseParLeSucces ? 1 : 0,
				toujoursIci ? 1 : 0, deplace ? 1 : 0, sousCourant, bonCourant ? 1 : 0, sousRecents,
				sectionsOk120 ? 1 : 0);
			check("120. ⑥ LE DOSSIER COURANT NE BOUGE QUE SUR SUCCES : c'est LA TETE DES RECENTS -- une seule source, deja "
				"mise a jour apres une ecriture reussie seulement. Naviguer, regarder, annuler ne le changent PAS ; une "
				"operation reussie le pose, une seconde ailleurs le deplace et l'ancien descend dans « R\u00e9cents » ; et sa "
				"section ne porte QU'UNE entree -- l'afficher aussi dans « R\u00e9cents » dirait deux fois la meme chose",
				videAuDepart && navigationNeChangeRien && poseParLeSucces && toujoursIci && deplace
					&& sectionsOk120,
				det);
			NkDirectory::Delete("sonde_courant", true);
		}
		// -- 121. (1) ET (2) L'INFOBULLE DU RAIL : RELEVEE PAR L'ARBRE, PEINTE PAR L'HOTE
		//    (05/09, v5). Rodolf, sur deux captures : « l'infobulle se peint au milieu du
		//    rail et masque les entrees » et « une entree du rail n'a pas de libelle ».
		//    UNE SEULE CAUSE, et elle etait dans MON code : le cartouche etait peint PAR
		//    L'ARBRE, dans SA liste de dessin, a « souris + 12/+16 ». Il tombait donc sur
		//    la rangee du dessous et, etant opaque, lui volait son libelle.
		//    Un composant qui peint hors de son rectangle prend une decision de mise en
		//    page qui ne lui appartient pas : lui seul ne sait ni ou est le bord de la
		//    fenetre, ni quelle couche est au-dessus, ni quelle place est libre a cote.
		{
			char det[900];
			NkContentBrowserModel m121;
			m121.headerTitle = NkString("rail");
			// Cinq entrees de rail, chacune avec SON libelle et SON infobulle.
			static const char *kLib121[5] = {"Alpha", "Bravo", "Charlie", "Delta", "Echo"};
			for (uint32 k = 0; k < 5u; ++k) {
				editorkit::NkTreeNode n;
				n.id = (nk_uint64)(k + 1u);
				n.parent = -1;
				n.label = NkString(kLib121[k]);
				n.path = NkString("/rail/");
				n.path.Append(kLib121[k]);
				n.infobulle = NkString("BULLE_UNIQUE_DE_SONDE_121");
				m121.folders.nodes.PushBack(n);
			}
			NkComponentInstance inst121(NkContentBrowserDecl());
			inst121.SetParam("show_header", 0.f);
			inst121.SetParam("show_actions", 0.f);
			NkContentBrowserStyle sty121 = DemoStyle(nullptr);
			sty121.values = &inst121;
			NkContentBrowserHooks h121;
			const NkPaintRect zone121{0.f, 0.f, 900.f, 500.f};
			const float32 railW121 = zone121.w * NkContentBrowserDecl().Param("tree_width");
			const float32 rowH121 = NkTreeViewDecl().Metric("row_h");
			// LA SOURIS SUR LA DEUXIEME RANGEE du rail (« Bravo »).
			NkComponentInput in121;
			in121.surfaceScale = 1.f;
			in121.mouseX = railW121 * 0.5f;
			in121.mouseY = -1000.f;
			// on cherche d'abord la rangee : un dessin SANS survol donne l'ordonnee des
			// libelles, puis on survole le deuxieme.
			{
				NkComponentInput froid;
				froid.surfaceScale = 1.f;
				froid.mouseX = -1000.f;
				froid.mouseY = -1000.f;
				NkRecordingPaint r0;
				NkDrawContentBrowser(r0, froid, zone121, m121, sty121, h121);
				for (uint32 i = 0; i < (uint32)r0.cmds.Size(); ++i)
					if (r0.cmds[i].op == NkPaintOp::Text && r0.cmds[i].text.Data()
						&& NkComponentDecl::StrEq(r0.cmds[i].text.Data(), "Bravo"))
						in121.mouseY = r0.cmds[i].y + rowH121 * 0.5f;
			}
			NkRecordingPaint r121;
			const NkContentBrowserResult res121 =
				NkDrawContentBrowser(r121, in121, zone121, m121, sty121, h121);
			// 1. L'ARBRE NE PEINT PLUS LA BULLE : le texte unique n'est nulle part.
			bool bullePeinte = false;
			for (uint32 i = 0; i < (uint32)r121.cmds.Size(); ++i)
				if (r121.cmds[i].op == NkPaintOp::Text && r121.cmds[i].text.Data()
					&& NkComponentDecl::StrEq(r121.cmds[i].text.Data(), "BULLE_UNIQUE_DE_SONDE_121"))
					bullePeinte = true;
			// 2. IL LA RAPPORTE, avec la rangee et le bord DROIT du rail.
			const bool rapportee =
				!res121.infobulle.Empty()
				&& NkComponentDecl::StrEq(res121.infobulle.Data(), "BULLE_UNIQUE_DE_SONDE_121")
				&& res121.infobulleX > railW121 - 2.f && res121.infobulleX < railW121 + 2.f
				&& res121.infobulleH > 1.f && in121.mouseY >= res121.infobulleY
				&& in121.mouseY <= res121.infobulleY + res121.infobulleH;
			// 3. LES CINQ LIBELLES SONT PEINTS, ET AUCUN N'EST RECOUVERT. C'est LA mesure
			//    du defaut (2) : un aplat opaque emis APRES un libelle et qui mord sur lui
			//    = un nom invisible a l'ecran.
			uint32 libellesVus = 0u, libellesRecouverts = 0u;
			char quiRecouvre[64];
			quiRecouvre[0] = 0;
			for (uint32 i = 0; i < (uint32)r121.cmds.Size(); ++i) {
				const NkPaintCmd &c = r121.cmds[i];
				if (c.op != NkPaintOp::Text || !c.text.Data())
					continue;
				bool estLibelle = false;
				for (uint32 k = 0; k < 5u; ++k)
					if (NkComponentDecl::StrEq(c.text.Data(), kLib121[k]))
						estLibelle = true;
				if (!estLibelle)
					continue;
				++libellesVus;
				for (uint32 j = i + 1u; j < (uint32)r121.cmds.Size(); ++j) {
					const NkPaintCmd &d = r121.cmds[j];
					const bool opaque = d.op == NkPaintOp::Fill || d.op == NkPaintOp::FillColor
										|| d.op == NkPaintOp::Outline;
					if (!opaque)
						continue;
					if (d.x < c.x + c.w && d.x + d.w > c.x && d.y < c.y + c.h && d.y + d.h > c.y) {
						++libellesRecouverts;
						snprintf(quiRecouvre, sizeof(quiRecouvre), "%s", c.text.Data());
						break;
					}
				}
			}
			// 4. LA POSE DU CARTOUCHE, lue par le temoin parce qu'elle est une FONCTION
			//    PURE et non trois lignes enfermees dans le dessin.
			const NkRect b1 =
				editorkit::NkPlacerInfobulle(railW121, 120.f, 24.f, 200.f, 16.f, 1920.f, 1080.f, 1.f);
			const bool horsDuRail = b1.x >= railW121;
			const bool enFace = b1.y + b1.h * 0.5f > 120.f && b1.y + b1.h * 0.5f < 144.f;
			// fenetre ETROITE : la bulle est repoussee, mais le mur du rail gagne
			const NkRect b2 = editorkit::NkPlacerInfobulle(railW121, 1000.f, 24.f, 900.f, 16.f,
														   railW121 + 300.f, 400.f, 1.f);
			const bool murGagne = b2.x >= railW121;
			const bool dansLaFenetre = b2.y >= 0.f && b2.y + b2.h <= 400.f;
			const bool poseOk = horsDuRail && enFace && murGagne && dansLaFenetre;
			snprintf(det, sizeof(det),
					 "souris sur « Bravo » (y=%.0f) : l'arbre peint la bulle=%d (0 attendu) ; il la RAPPORTE=%d "
					 "(x=%.0f pour un rail de %.0f ; rangee y=%.0f h=%.0f) ; %u/5 libelles peints, %u "
					 "recouvert(s) par un aplat posterieur%s%s ; pose : hors du rail=%d, en face de la "
					 "rangee=%d, fenetre etroite -> le mur gagne=%d et la bulle reste dans la fenetre=%d",
					 in121.mouseY, bullePeinte ? 1 : 0, rapportee ? 1 : 0, res121.infobulleX, railW121,
					 res121.infobulleY, res121.infobulleH, libellesVus, libellesRecouverts,
					 quiRecouvre[0] ? " : " : "", quiRecouvre, horsDuRail ? 1 : 0, enFace ? 1 : 0,
					 murGagne ? 1 : 0, dansLaFenetre ? 1 : 0);
			check("121. (1)(2) L'INFOBULLE DU RAIL EST RELEVEE PAR L'ARBRE ET PEINTE PAR L'HOTE, EN DERNIER ET HORS "
				  "DU RAIL : l'arbre la dessinait lui-meme a « souris + 12/+16 », dans sa propre liste -- elle "
				  "tombait donc sur la rangee du dessous et, opaque, lui volait son libelle (les deux defauts de "
				  "Rodolf n'en font qu'un). Desormais l'arbre RAPPORTE (le texte et la rangee), le navigateur "
				  "relaie avec le bord DROIT du rail, et le selecteur peint apres tout le reste, adosse a ce "
				  "bord : les cinq libelles restent lisibles pendant qu'une bulle est affichee",
				  !bullePeinte && rapportee && libellesVus == 5u && libellesRecouverts == 0u && poseOk,
				  det);
		}
		// -- 122. (5) LE REPLI QUI INVENTAIT UN TROISIEME ETAT (05/09, v5). Rodolf :
		//    « la notion de dossier courant n'est pas encore valide » -- et sur sa capture
		//    la section montrait le dossier OU IL NAVIGUAIT, avec toute son arborescence.
		//    La cause tenait en deux lignes : `if (pas de courant) courant = pickerPath;`.
		//    Ce repli faisait exister un etat que sa definition ne prevoit pas -- ni « le
		//    dossier d'une reussite », ni « aucun », mais « celui qu'on regarde ». Un repli
		//    qui invente une valeur ment plus surement qu'une absence.
		{
			char det[900];
			NkDirectory::Delete("sonde_courant2", true);
			NkDirectory::CreateRecursive("sonde_courant2/vu/dedans");
			NkDirectory::CreateRecursive("sonde_courant2/reussi/dedans");
			const NkString rac122 =
				(NkPath(NkDirectory::GetCurrentDirectory()) / "sonde_courant2").ToString();
			const NkString dVu = (NkPath(rac122.Data()) / "vu").ToString();
			const NkString dOk = (NkPath(rac122.Data()) / "reussi").ToString();
			editorkit::NkFilePickerNavState nav;
			char bb[512] = {};
			nav.OpenPickerBase(editorkit::NkSelecteurEnregistrer, rac122.Data(), bb,
							   (int32)sizeof(bb), nullptr, nullptr);
			// Ce que la section « Dossier courant » porte, a un instant donne.
			struct Sec {
					uint32 enfants;
					uint32 descendants;
					bool notePresente;
					bool porteLeChemin;
			};
			auto lire = [&](const char *chemin) -> Sec {
				Sec r{0u, 0u, false, false};
				int32 sec = -1;
				for (uint32 k = 0; k < (uint32)nav.vue.folders.nodes.Size(); ++k) {
					const char *l = nav.vue.folders.nodes[k].label.Data();
					if (l && NkComponentDecl::StrEq(l, "Dossier courant"))
						sec = (int32)k;
				}
				if (sec < 0)
					return r;
				// enfants directs, puis TOUTE la descendance (l'arborescence depliee)
				for (uint32 k = 0; k < (uint32)nav.vue.folders.nodes.Size(); ++k) {
					const editorkit::NkTreeNode &n = nav.vue.folders.nodes[k];
					int32 a = n.parent;
					bool sous = false;
					for (int32 g = 0; a >= 0 && g < 16; ++g) {
						if (a == sec) {
							sous = true;
							break;
						}
						a = nav.vue.folders.nodes[(uint32)a].parent;
					}
					if (!sous)
						continue;
					++r.descendants;
					if (n.parent == sec)
						++r.enfants;
					if (n.path.Empty() && n.locked)
						r.notePresente = true;
					if (chemin && !n.path.Empty()
						&& editorkit::NkFilePickerState::PathSame(n.path.Data(), chemin))
						r.porteLeChemin = true;
				}
				return r;
			};
			// 1. ON NAVIGUE, SANS RIEN REUSSIR : la section ne montre PAS le dossier vu,
			//    elle ne montre PAS son arborescence, elle DIT qu'il n'y en a pas encore.
			nav.AllerA(dVu.Data());
			nav.relire = true;
			nav.RelireDossier();
			const Sec s1 = lire(dVu.Data());
			const bool pasDeTroisiemeEtat =
				!s1.porteLeChemin && s1.notePresente && s1.enfants == 1u && s1.descendants == 1u;
			// 2. UNE OPERATION REUSSIE AILLEURS : la section change, ELLE, et porte le
			//    dossier de la reussite avec ses sous-dossiers.
			editorkit::NkFilePickerNavState::PoserRecent(nav.recents, dOk.Data());
			nav.relire = true;
			nav.RelireDossier();
			const Sec s2 = lire(dOk.Data());
			const bool laReussiteLePose = s2.porteLeChemin && !s2.notePresente && s2.enfants == 1u
										  && s2.descendants >= 2u;
			// 3. ANNULER = NAVIGUER PUIS PARTIR : la section ne bouge pas.
			nav.AllerA(dVu.Data());
			nav.relire = true;
			nav.RelireDossier();
			const Sec s3 = lire(dOk.Data());
			const bool annulerNeChangeRien =
				s3.porteLeChemin && !s3.notePresente
				&& editorkit::NkFilePickerState::PathSame(nav.DossierCourant(), dOk.Data());
			// 4. LE DIALOGUE ROUVERT DEMARRE SUR LUI -- c'est la porte de l'application
			//    (`NkOuvrirSelecteurExport`) qui le lit ; on refait ici son geste exact.
			editorkit::NkFilePickerNavState nav2;
			char bb2[512] = {};
			nav2.recents = nav.recents;
			const char *depart = nav2.DossierCourant();
			nav2.OpenPickerBase(editorkit::NkSelecteurEnregistrer,
								depart[0] ? depart : rac122.Data(), bb2, (int32)sizeof(bb2), nullptr,
								nullptr);
			const bool rouvreLaOuOnEtait =
				editorkit::NkFilePickerState::PathSame(nav2.pickerPath, dOk.Data());
			snprintf(det, sizeof(det),
					 "apres NAVIGATION seule : la section porte le dossier navigue=%d (0 attendu), la note "
					 "« aucun »=%d, %u enfant(s) et %u descendant(s) -> %d ; apres une REUSSITE ailleurs : "
					 "elle le porte=%d avec %u enfant(s) / %u descendant(s), note=%d -> %d ; naviguer puis "
					 "partir (annuler) ne la change pas=%d ; le dialogue rouvert demarre dessus=%d",
					 s1.porteLeChemin ? 1 : 0, s1.notePresente ? 1 : 0, s1.enfants, s1.descendants,
					 pasDeTroisiemeEtat ? 1 : 0, s2.porteLeChemin ? 1 : 0, s2.enfants, s2.descendants,
					 s2.notePresente ? 1 : 0, laReussiteLePose ? 1 : 0, annulerNeChangeRien ? 1 : 0,
					 rouvreLaOuOnEtait ? 1 : 0);
			check("122. (5) PAS DE REUSSITE, PAS DE DOSSIER COURANT : le repli `courant = pickerPath` faisait exister "
				  "un TROISIEME etat -- ni le dossier d'une reussite, ni aucun, mais celui qu'on regarde -- et c'est "
				  "celui que Rodolf voyait, avec toute son arborescence. Il est retire. Tant qu'aucune ouverture ni "
				  "aucun enregistrement n'a reussi, la section porte UNE phrase verrouillee qui dit la regle (la "
				  "supprimer laisserait croire a une panne) ; une reussite la remplace par le dossier et ses "
				  "sous-dossiers ; naviguer puis annuler ne la touche pas ; et le dialogue rouvert y demarre",
				  pasDeTroisiemeEtat && laReussiteLePose && annulerNeChangeRien && rouvreLaOuOnEtait,
				  det);
			NkDirectory::Delete("sonde_courant2", true);
		}
		// -- 123. VIDE, PLEIN, OU ILLISIBLE -- ET CE QUE CA COUTE (05/09, v5).
		//    Rodolf : « il faut aussi distinguer dossier vide de dossier plein ». J'avais
		//    nomme la reserve (un acces disque par entree affichee) sans la mesurer ; la
		//    voici mesuree, et payee : on demande « au moins un » et non « combien », on
		//    retient la reponse par chemin ET par horodatage, et on ne sonde que ce qui
		//    est a l'ecran.
		{
			char det[900];
			NkDirectory::Delete("sonde_remplissage", true);
			NkDirectory::CreateRecursive("sonde_remplissage/vide");
			NkDirectory::CreateRecursive("sonde_remplissage/fichiers_seuls");
			NkDirectory::CreateRecursive("sonde_remplissage/avec_sous_dossier/enfant");
			NkFile::WriteAllText("sonde_remplissage/fichiers_seuls/a.txt", "x");
			const NkString rac = (NkPath(NkDirectory::GetCurrentDirectory()) / "sonde_remplissage").ToString();
			const NkString dVide = (NkPath(rac.Data()) / "vide").ToString();
			const NkString dFich = (NkPath(rac.Data()) / "fichiers_seuls").ToString();
			const NkString dSous = (NkPath(rac.Data()) / "avec_sous_dossier").ToString();
			// 1. TROIS DOSSIERS, TROIS REPONSES -- et LES DEUX QUESTIONS ne se confondent
			//    pas : « contient-il quelque chose ? » n'est pas « a-t-il des sous-dossiers ? ».
			//    Le dossier de fichiers seuls est PLEIN pour l'icone et SANS ENFANT pour le
			//    chevron : c'est precisement le couple que deux fonctions auraient melange.
			typedef NkDirectory::NkDirProbe Pr;
			const Pr tVide = NkDirectory::Probe(dVide.Data(), false);
			const Pr tFich = NkDirectory::Probe(dFich.Data(), false);
			const Pr tSous = NkDirectory::Probe(dSous.Data(), false);
			const Pr cVide = NkDirectory::Probe(dVide.Data(), true);
			const Pr cFich = NkDirectory::Probe(dFich.Data(), true);
			const Pr cSous = NkDirectory::Probe(dSous.Data(), true);
			const bool troisEtats = tVide == Pr::Vide && tFich == Pr::Plein && tSous == Pr::Plein
									&& cVide == Pr::Vide && cFich == Pr::Vide && cSous == Pr::Plein;
			// 2. ILLISIBLE N'EST PAS VIDE. `NkDirectory::Empty` rend `true` sur un dossier
			//    qu'il n'a pas pu ouvrir -- il annonce vide ce qu'il ne sait pas. `Probe`
			//    rend un TROISIEME etat. (Un refus de droits ne se fabrique pas de facon
			//    portable dans un temoin : on exerce ici la MEME branche, celle du dossier
			//    qui ne s'ouvre pas.)
			const NkString absent = (NkPath(rac.Data()) / "ceci_n_existe_pas").ToString();
			const bool illisiblePasVide = NkDirectory::Probe(absent.Data(), false) == Pr::Illisible
										  && NkDirectory::Empty(absent.Data());
			// 3. LE CACHE : deux interrogations, UN SEUL acces disque. Le compteur est dans
			//    le cache -- sans lui, « c'est en cache » serait une affirmation.
			editorkit::NkCacheDossiers cache;
			const uint8 e1 = cache.Etat(dFich.Data(), 1000, false);
			const uint32 apres1 = cache.accesDisque;
			const uint8 e2 = cache.Etat(dFich.Data(), 1000, false);
			const uint32 apres2 = cache.accesDisque;
			// l'horodatage CHANGE -> on reprend la mesure
			const uint8 e3 = cache.Etat(dFich.Data(), 2000, false);
			const uint32 apres3 = cache.accesDisque;
			// et la SECONDE question sur le meme dossier est une mesure de plus, pas une
			// reponse recyclee : ce sont deux faits differents.
			cache.Etat(dFich.Data(), 2000, true);
			const uint32 apres4 = cache.accesDisque;
			const bool cacheOk = e1 == e2 && e2 == e3 && apres1 == 1u && apres2 == 1u
								 && apres3 == 2u && apres4 == 3u;
			// 4. LE COUT, MESURE. 124 dossiers, l'ancienne question contre la nouvelle.
			NkDirectory::CreateRecursive("sonde_remplissage/beaucoup");
			const NkString dBcp = (NkPath(rac.Data()) / "beaucoup").ToString();
			for (int32 k = 0; k < 124; ++k) {
				char nom[256];
				snprintf(nom, sizeof(nom), "%s/d%03d/dedans", dBcp.Data(), k);
				NkDirectory::CreateRecursive(nom);
			}
			NkVector<NkDirectoryEntry> lot =
				NkDirectory::GetEntries(NkPath(dBcp.Data()), "*", NkSearchOption::NK_TOP_DIRECTORY_ONLY);
			float64 msAvant = 0.0, msApres = 0.0, msCache = 0.0;
			{
				NkChrono c;
				for (usize k = 0; k < lot.Size(); ++k)
					(void)NkDirectory::Empty(lot[k].FullPath.CStr()); // l'ancienne question
				msAvant = c.Elapsed().milliseconds;
			}
			{
				NkChrono c;
				for (usize k = 0; k < lot.Size(); ++k)
					(void)NkDirectory::Probe(lot[k].FullPath.CStr(), false);
				msApres = c.Elapsed().milliseconds;
			}
			{
				editorkit::NkCacheDossiers c2;
				for (usize k = 0; k < lot.Size(); ++k)
					(void)c2.Etat(lot[k].FullPath.CStr(), (nk_int64)lot[k].ModificationTime, false);
				NkChrono c;
				for (usize k = 0; k < lot.Size(); ++k)
					(void)c2.Etat(lot[k].FullPath.CStr(), (nk_int64)lot[k].ModificationTime, false);
				msCache = c.Elapsed().milliseconds;
			}
			// LE SEUIL EST DIT, PAS DEVINE : au-dela de 8 ms pour un dossier de 124 entrees,
			// le dialogue se sentirait -- il faudrait alors sonder en arriere-plan. En
			// dessous, le faire tout de suite est plus simple ET plus honnete (aucune icone
			// qui se corrige sous l'oeil).
			const bool coutTenable = msApres < 8.0 && msCache < msApres;
			// 5. LE DESSIN : trois empreintes DISTINCTES, a 32, 64 et 128 px.
			auto empreinte = [](uint8 contenu, float32 taille) -> uint32 {
				NkRecordingPaint r;
				NkDessinerSilhouette(r, {0.f, 0.f, taille, taille}, editorkit::NkAssetIcone::Dossier, 7,
									 contenu);
				uint32 h = 2166136261u;
				for (uint32 i = 0; i < (uint32)r.cmds.Size(); ++i) {
					const NkPaintCmd &c = r.cmds[i];
					const uint32 v[5] = {(uint32)(c.x * 4.f), (uint32)(c.y * 4.f), (uint32)(c.w * 4.f),
										 (uint32)(c.h * 4.f), c.rgba};
					for (uint32 q = 0; q < 5u; ++q) {
						h ^= v[q];
						h *= 16777619u;
					}
				}
				return h;
			};
			bool troisDessins = true;
			static const float32 kTailles[3] = {32.f, 64.f, 128.f};
			for (uint32 t = 0; t < 3u; ++t) {
				const uint32 hv = empreinte((uint8)editorkit::NkContenuDossier::Vide, kTailles[t]);
				const uint32 hp = empreinte((uint8)editorkit::NkContenuDossier::Plein, kTailles[t]);
				const uint32 hi = empreinte((uint8)editorkit::NkContenuDossier::Illisible, kTailles[t]);
				const uint32 hn = empreinte((uint8)editorkit::NkContenuDossier::Inconnu, kTailles[t]);
				// vide == inconnu (le dessin nu), mais plein et illisible s'en distinguent
				if (hv != hn || hp == hv || hi == hv || hp == hi)
					troisDessins = false;
			}
			snprintf(det, sizeof(det),
					 "trois dossiers : contenu(vide,fichiers,sous)=%u/%u/%u et sous-dossiers=%u/%u/%u -> %d "
					 "(un dossier de fichiers SEULS est plein pour l'icone et sans enfant pour le chevron) ; "
					 "illisible != vide (Empty dit vide, Probe dit illisible)=%d ; cache : %u/%u/%u/%u acces "
					 "pour 4 questions -> %d ; COUT sur 124 dossiers : Empty %.2f ms, Probe %.2f ms, "
					 "cache relu %.3f ms -> tenable sans arriere-plan=%d ; trois empreintes distinctes a "
					 "32/64/128 px=%d",
					 (uint32)tVide, (uint32)tFich, (uint32)tSous, (uint32)cVide, (uint32)cFich,
					 (uint32)cSous, troisEtats ? 1 : 0, illisiblePasVide ? 1 : 0, apres1, apres2, apres3,
					 apres4, cacheOk ? 1 : 0, msAvant, msApres, msCache, coutTenable ? 1 : 0,
					 troisDessins ? 1 : 0);
			check("123. UN DOSSIER VIDE, UN DOSSIER PLEIN ET UN DOSSIER QU'ON NE PEUT PAS LIRE NE SE DESSINENT PAS "
				  "PAREIL, ET LE COUT EST MESURE : on ne demande pas « combien d'entrees » mais « au moins une » "
				  "(arret au premier, aucune allocation), la reponse est retenue par chemin ET par horodatage (deux "
				  "interrogations, un seul acces), et seules les entrees A L'ECRAN sont sondees. Le chevron et "
				  "l'icone posent deux questions VOISINES -- « a-t-il des sous-dossiers ? » et « contient-il quelque "
				  "chose ? » -- auxquelles repond UNE SEULE fonction parametree ; un dossier illisible n'est ni vide "
				  "ni plein, et son infobulle le dit",
				  troisEtats && illisiblePasVide && cacheOk && coutTenable && troisDessins, det);
			NkDirectory::Delete("sonde_remplissage", true);
		}
		// -- 124. (3) LE CHEVRON SUIT LA MEME REGLE POUR TOUTES LES ENTREES (05/09, v5).
		//    Rodolf : « les chevrons ne sont pas presents sur la plupart des entrees ; seule
		//    la branche du dossier courant en a. » C'etait exact, et la cause etait
		//    structurelle : `enfantsPossibles` etait renseigne par `PoserSousDossiers`, qui
		//    ne tourne que sous le dossier courant. Les entrees de « Recents », « Acces
		//    rapide » et « Ce PC » naissent dans `AjouterNoeud`, qui ne le renseignait pas.
		//    Une regle appliquee a un seul endroit sur deux n'est pas une regle.
		{
			char det[900];
			NkDirectory::Delete("sonde_chevrons", true);
			NkDirectory::CreateRecursive("sonde_chevrons/avec/enfant");
			NkDirectory::CreateRecursive("sonde_chevrons/sans");
			NkDirectory::CreateRecursive("sonde_chevrons/fichiers");
			NkFile::WriteAllText("sonde_chevrons/fichiers/a.txt", "x");
			const NkString rac124 =
				(NkPath(NkDirectory::GetCurrentDirectory()) / "sonde_chevrons").ToString();
			const NkString dAvec = (NkPath(rac124.Data()) / "avec").ToString();
			const NkString dSans = (NkPath(rac124.Data()) / "sans").ToString();
			const NkString dFich124 = (NkPath(rac124.Data()) / "fichiers").ToString();
			editorkit::NkFilePickerNavState nav124;
			char b124[512] = {};
			nav124.OpenPickerBase(editorkit::NkSelecteurOuvrirDossier, rac124.Data(), b124,
								  (int32)sizeof(b124), nullptr, nullptr);
			// TROIS FAVORIS : ils naissent dans `AjouterNoeud`, PAS dans `PoserSousDossiers`
			// -- c'est exactement le chemin qui ne posait pas de chevron.
			nav124.favoris.PushBack(dAvec);
			nav124.favoris.PushBack(dSans);
			nav124.favoris.PushBack(dFich124);
			nav124.relire = true;
			nav124.RelireDossier();
			auto noeud = [&](const char *chemin) -> const editorkit::NkTreeNode * {
				for (uint32 k = 0; k < (uint32)nav124.vue.folders.nodes.Size(); ++k)
					if (!nav124.vue.folders.nodes[k].path.Empty()
						&& editorkit::NkFilePickerState::PathSame(
							nav124.vue.folders.nodes[k].path.Data(), chemin))
						return &nav124.vue.folders.nodes[k];
				return nullptr;
			};
			const editorkit::NkTreeNode *nAvec = noeud(dAvec.Data());
			const editorkit::NkTreeNode *nSans = noeud(dSans.Data());
			const editorkit::NkTreeNode *nFich = noeud(dFich124.Data());
			// 1. LE CHEVRON N'EST PAS UNE DECORATION : il dit « j'ai des SOUS-DOSSIERS ».
			//    Le dossier qui n'a que des FICHIERS n'en a pas -- et il est plein.
			const bool chevronsJustes =
				nAvec && nSans && nFich && nAvec->enfantsPossibles && !nSans->enfantsPossibles
				&& !nFich->enfantsPossibles
				&& nFich->contenu == (uint8)editorkit::NkContenuDossier::Plein
				&& nSans->contenu == (uint8)editorkit::NkContenuDossier::Vide;
			// 2. TOUTES LES SECTIONS SUIVENT LA MEME REGLE. On compte, par section, les
			//    entrees qui ANNONCENT des enfants : avant, seule la branche du dossier
			//    courant en avait -- « Acces rapide » et « Ce PC » n'en avaient AUCUNE.
			uint32 sectionsAvecChevron = 0u, sectionsAvecEntrees = 0u;
			for (uint32 k = 0; k < (uint32)nav124.vue.folders.nodes.Size(); ++k) {
				if (!nav124.vue.folders.nodes[k].bandeau)
					continue;
				uint32 enfants = 0u, annoncent = 0u;
				for (uint32 q = 0; q < (uint32)nav124.vue.folders.nodes.Size(); ++q) {
					if (nav124.vue.folders.nodes[q].parent != (int32)k)
						continue;
					if (nav124.vue.folders.nodes[q].path.Empty())
						continue; // la phrase « aucun dossier courant » n'est pas une entree
					++enfants;
					if (nav124.vue.folders.nodes[q].enfantsPossibles)
						++annoncent;
				}
				if (enfants > 0u)
					++sectionsAvecEntrees;
				if (annoncent > 0u)
					++sectionsAvecChevron;
			}
			const bool toutesLesSections =
				sectionsAvecEntrees >= 2u && sectionsAvecChevron == sectionsAvecEntrees;
			// 3. LA PLACE EST RESERVEE MEME SANS CHEVRON : deux noeuds identiques, l'un
			//    annoncant des enfants et l'autre non, posent leur libelle A LA MEME
			//    ABSCISSE. Un libelle qui se decale selon qu'il y a un chevron ferait
			//    danser la colonne au moindre depliage.
			auto xDuLibelle = [](bool annonce) -> float32 {
				NkRecordingPaint r;
				NkTreeViewModel m;
				NkTreeNode n;
				n.id = 1u;
				n.parent = -1;
				n.label = NkString("MEME_LIBELLE");
				n.path = NkString("/x");
				n.enfantsPossibles = annonce;
				m.nodes.PushBack(n);
				NkTreeViewStyle st;
				NkTreeViewHooks h;
				NkComponentInput in;
				NkDrawTreeView(r, in, {0.f, 0.f, 260.f, 200.f}, m, st, h);
				for (uint32 i = 0; i < (uint32)r.cmds.Size(); ++i)
					if (r.cmds[i].op == NkPaintOp::Text && r.cmds[i].text.Data()
						&& NkComponentDecl::StrEq(r.cmds[i].text.Data(), "MEME_LIBELLE"))
						return r.cmds[i].x;
				return -1.f;
			};
			const float32 xAvec = xDuLibelle(true), xSans = xDuLibelle(false);
			const bool placeReservee = xAvec > 0.f && xAvec == xSans;
			// 4. CE QUE CA COUTE : le rail entier, en acces disque REELS. Deux questions
			//    par entree, chacune payee UNE fois -- et rien n'est paye deux fois.
			const uint32 acces = nav124.cacheDossiers.accesDisque;
			uint32 entreesRail = 0u;
			for (uint32 k = 0; k < (uint32)nav124.vue.folders.nodes.Size(); ++k)
				if (!nav124.vue.folders.nodes[k].path.Empty())
					++entreesRail;
			const uint32 avantRelecture = nav124.cacheDossiers.accesDisque;
			nav124.relire = true;
			nav124.RelireDossier(); // on rebatit le rail : le cache doit tout resservir
			const bool cacheResservi = nav124.cacheDossiers.accesDisque == avantRelecture;
			const bool coutBorne = acces <= entreesRail * 2u;
			snprintf(det, sizeof(det),
					 "favoris (nes dans AjouterNoeud, pas dans PoserSousDossiers) : « avec » annonce des "
					 "enfants=%d, « sans » non=%d, « fichiers » non=%d mais il est PLEIN=%d -> %d ; "
					 "%u section(s) portent des entrees, %u en ont au moins une avec chevron -> %d ; place "
					 "reservee : libelle a x=%.1f avec chevron et x=%.1f sans -> %d ; cout : %u acces disque "
					 "pour %u entree(s) de rail (2 questions chacune) -> %d ; rail rebati sans un seul acces "
					 "de plus=%d",
					 nAvec && nAvec->enfantsPossibles ? 1 : 0, nSans && !nSans->enfantsPossibles ? 1 : 0,
					 nFich && !nFich->enfantsPossibles ? 1 : 0,
					 nFich && nFich->contenu == (uint8)editorkit::NkContenuDossier::Plein ? 1 : 0,
					 chevronsJustes ? 1 : 0, sectionsAvecEntrees, sectionsAvecChevron,
					 toutesLesSections ? 1 : 0, (double)xAvec, (double)xSans, placeReservee ? 1 : 0,
					 acces, entreesRail, coutBorne ? 1 : 0, cacheResservi ? 1 : 0);
			check("124. (3) LE CHEVRON EST POSE LA OU LE N\u0152UD NAIT, DONC POUR TOUTES LES SECTIONS : il n'etait "
				  "renseigne que par le site qui deplie le dossier courant, si bien que « Recents », « Acces rapide » "
				  "et « Ce PC » n'en avaient aucun -- une regle appliquee a un endroit sur deux n'est pas une regle. "
				  "Il dit « j'ai des SOUS-DOSSIERS » et rien d'autre : un dossier plein de fichiers n'en a pas, et "
				  "c'est son ICONE qui dit qu'il est plein. La place est reservee quand il n'y a pas de chevron (la "
				  "colonne des libelles ne danse pas), et le cout est borne a deux acces disque par entree, une seule "
				  "fois : rebatir le rail n'en coute aucun de plus",
				  chevronsJustes && toutesLesSections && placeReservee && coutBorne && cacheResservi, det);
			NkDirectory::Delete("sonde_chevrons", true);
		}
		// -- 125. (4) UNE SECTION SE REPLIE POUR DE VRAI (05/09, v5). Rodolf : « les
		//    sections ne se replient pas -- leur chevron est dessine mais inerte ». Le
		//    temoin mesure les DEUX moities de la chaine, parce que le geste en traverse
		//    deux : le composant (le clic plie-t-il un noeud VERROUILLE ?) et l'hote (l'etat
		//    survit-il a la RECONSTRUCTION du rail que ce meme clic declenche ?).
		{
			char det[900];
			const float32 rowH125 = NkTreeViewDecl().Metric("row_h");
			const float32 pad125 = NkTreeViewDecl().Metric("row_pad");
			const float32 chev125 = NkTreeViewDecl().Metric("chevron_w");
			// ---- A. LE COMPOSANT : un titre VERROUILLE et ses deux entrees -------------
			NkTreeViewModel m125;
			{
				NkTreeNode t;
				t.id = 1u;
				t.parent = -1;
				t.label = NkString("TITRE");
				t.locked = true; // c'est ce que sont les sections du rail
				t.bandeau = true;
				m125.nodes.PushBack(t);
				for (uint32 k = 0; k < 2u; ++k) {
					NkTreeNode e;
					e.id = 2u + k;
					e.parent = 0;
					e.label = NkString(k == 0u ? "un" : "deux");
					e.path = NkString(k == 0u ? "/un" : "/deux");
					m125.nodes.PushBack(e);
				}
			}
			NkTreeViewStyle st125;
			NkTreeViewHooks h125;
			const NkPaintRect z125{0.f, 0.f, 260.f, 300.f};
			// L'ORDONNEE DE LA RANGEE SE LIT DU DESSIN, elle ne se devine pas : l'arbre a
			// un en-tete, et poser la souris a `rect.y + rowH/2` la met DEDANS -- le clic
			// tombe alors a cote et « le chevron est inerte » devient une conclusion fausse
			// tiree d'une mesure fausse. C'est le piege qui a failli me faire corriger le
			// composant alors qu'il n'avait rien.
			float32 yTitre125 = -1.f;
			{
				NkRecordingPaint r;
				NkComponentInput in;
				in.surfaceScale = 1.f;
				in.mouseX = -1000.f;
				in.mouseY = -1000.f;
				NkDrawTreeView(r, in, z125, m125, st125, h125);
				for (uint32 i = 0; i < (uint32)r.cmds.Size(); ++i)
					if (r.cmds[i].op == NkPaintOp::Text && r.cmds[i].text.Data()
						&& NkComponentDecl::StrEq(r.cmds[i].text.Data(), "TITRE"))
						yTitre125 = r.cmds[i].y;
			}
			auto dessiner = [&](bool clic, bool ailleurs) -> int32 {
				NkRecordingPaint r;
				NkComponentInput in;
				in.surfaceScale = 1.f;
				// LE CHEVRON DU TITRE : premiere case apres la marge, sur SA rangee.
				in.mouseX = ailleurs ? z125.x + 200.f : z125.x + pad125 + chev125 * 0.5f;
				in.mouseY = yTitre125 + rowH125 * 0.5f;
				in.mousePressed = clic;
				in.mouseDown = clic;
				return NkDrawTreeView(r, in, z125, m125, st125, h125).visibleCount;
			};
			const int32 avant125 = dessiner(false, false); // 3 : le titre et ses deux entrees
			dessiner(true, false);						   // le clic sur le chevron
			const int32 apres125 = dessiner(false, false); // 1 : le titre seul
			const bool composantPlie = avant125 == 3 && apres125 == 1;
			// (4) ET TOUTE LA BANDE PLIE, PAS SEULEMENT LE CHEVRON. C'est LE defaut de
			//     Rodolf : le chevron pliait deja, mais il fallait viser seize pixels, et un
			//     clic sur le libelle d'un titre ne faisait rien du tout (il est `locked`,
			//     donc il ne se selectionne pas non plus). Un titre n'a aucune autre action.
			dessiner(true, true);						   // on rouvre : clic AILLEURS sur la bande
			const int32 rouvertParLaBande = dessiner(false, false);
			dessiner(true, true);						   // et on replie, toujours par la bande
			const int32 replieParLaBande = dessiner(false, false);
			const bool bandePlie = rouvertParLaBande == 3 && replieParLaBande == 1;
			// CONTROLE NEGATIF : une entree ORDINAIRE, elle, ne plie PAS sur son libelle --
			// `chevron_only_fold` reste vrai pour tout le monde sauf les titres. Sans ce
			// controle, « ca plie » ne prouverait rien : n'importe quel clic plierait.
			int32 ordinaireAvant = 0, ordinaireApres = 0;
			{
				NkTreeViewModel mo;
				NkTreeNode p1;
				p1.id = 10u;
				p1.parent = -1;
				p1.label = NkString("PARENT");
				p1.path = NkString("/p");
				mo.nodes.PushBack(p1);
				NkTreeNode c1;
				c1.id = 11u;
				c1.parent = 0;
				c1.label = NkString("ENFANT");
				c1.path = NkString("/p/c");
				mo.nodes.PushBack(c1);
				float32 yp = -1.f;
				{
					NkRecordingPaint r;
					NkComponentInput in;
					in.surfaceScale = 1.f;
					in.mouseX = -1000.f;
					in.mouseY = -1000.f;
					ordinaireAvant = NkDrawTreeView(r, in, z125, mo, st125, h125).visibleCount;
					for (uint32 i = 0; i < (uint32)r.cmds.Size(); ++i)
						if (r.cmds[i].op == NkPaintOp::Text && r.cmds[i].text.Data()
							&& NkComponentDecl::StrEq(r.cmds[i].text.Data(), "PARENT"))
							yp = r.cmds[i].y;
				}
				{
					NkRecordingPaint r;
					NkComponentInput in;
					in.surfaceScale = 1.f;
					in.mouseX = z125.x + 200.f; // sur le libelle, loin du chevron
					in.mouseY = yp + rowH125 * 0.5f;
					in.mousePressed = true;
					in.mouseDown = true;
					NkDrawTreeView(r, in, z125, mo, st125, h125);
				}
				{
					NkRecordingPaint r;
					NkComponentInput in;
					in.surfaceScale = 1.f;
					in.mouseX = -1000.f;
					in.mouseY = -1000.f;
					ordinaireApres = NkDrawTreeView(r, in, z125, mo, st125, h125).visibleCount;
				}
			}
			const bool ordinaireInchange = ordinaireAvant == 2 && ordinaireApres == 2;
			// ---- B. L'HOTE : l'etat survit-il a la reconstruction du rail ? ------------
			// Le clic change l'ensemble des noeuds deplies ; le selecteur le CONSTATE par
			// une empreinte et rebatit le rail. Si le pliage ne survivait pas a ce
			// rebatissage, le chevron paraitrait inerte a l'ecran alors que le composant,
			// lui, a bien fait son travail. C'est la moitie que le temoin du kit ne voit pas.
			NkDirectory::Delete("sonde_replier", true);
			NkDirectory::CreateRecursive("sonde_replier/a");
			const NkString rac125 =
				(NkPath(NkDirectory::GetCurrentDirectory()) / "sonde_replier").ToString();
			editorkit::NkFilePickerNavState nav125;
			char b125[512] = {};
			nav125.OpenPickerBase(editorkit::NkSelecteurOuvrirDossier, rac125.Data(), b125,
								  (int32)sizeof(b125), nullptr, nullptr);
			nav125.relire = true;
			nav125.RelireDossier();
			nk_uint64 idSection = 0u;
			for (uint32 k = 0; k < (uint32)nav125.vue.folders.nodes.Size(); ++k) {
				const char *l = nav125.vue.folders.nodes[k].label.Data();
				if (l && NkComponentDecl::StrEq(l, "Acc\u00e8s rapide"))
					idSection = nav125.vue.folders.nodes[k].id;
			}
			// (06/09) LE DEFAUT NE VIT PLUS ICI : il appartient au kit
			// (`NkFilePickerNavState::kRailDeplieParDefaut`). Le rail s'ouvrait TOUT
			// DEPLIE parce que `default_open` vaut 1 dans la declaration de l'arbre --
			// donc le peintre dessinait des chevrons ouverts sur un rail ferme. Cette
			// sonde ecrivait le `true` a la main : elle affirmait le defaut au lieu de
			// le lire, et elle serait tombee le jour ou il change -- c'est ce jour-la.
			static const bool kDefautRail = editorkit::NkFilePickerNavState::kRailDeplieParDefaut;
			const bool ouverteAuDepart = idSection != 0u && nav125.vue.folders.IsOpen(idSection, kDefautRail);
			// on plie, comme le ferait le clic
			nav125.vue.folders.SetOpen(idSection, false, kDefautRail);
			// puis LES DEUX LIGNES QUE FAIT LE SELECTEUR a l'image suivante
			const nk_uint64 emp125 = nav125.EmpreinteDeplie();
			const bool empreinteBouge = emp125 != nav125.empreinteDeplie;
			nav125.empreinteDeplie = emp125;
			nav125.ConstruireRail();
			const bool survit = !nav125.vue.folders.IsOpen(idSection, kDefautRail);
			// et les entrees de la section ne sont plus EMISES
			NkComponentInstance inst125(NkContentBrowserDecl());
			inst125.SetParam("show_header", 0.f);
			inst125.SetParam("show_actions", 0.f);
			// ET LE DESSIN LIT LE MEME DEFAUT QUE LE MODELE. Sans cette ligne, le clic de
			// l'image 1 aurait bascule dans le SENS INVERSE de ce que l'hote compte.
			inst125.SetParam("tree_default_open", kDefautRail ? 1.f : 0.f);
			NkContentBrowserStyle sty125 = DemoStyle(nullptr);
			sty125.values = &inst125;
			NkContentBrowserHooks hb125;
			auto lignesRail = [&]() -> uint32 {
				NkRecordingPaint r;
				NkComponentInput in;
				in.surfaceScale = 1.f;
				in.mouseX = -1000.f;
				in.mouseY = -1000.f;
				NkDrawContentBrowser(r, in, {0.f, 0.f, 900.f, 500.f}, nav125.vue, sty125, hb125);
				uint32 n = 0u;
				for (uint32 i = 0; i < (uint32)nav125.vue.folders.nodes.Size(); ++i) {
					// une entree EST VISIBLE si tous ses ancetres sont deplies
					bool vue = true;
					for (int32 a = nav125.vue.folders.nodes[i].parent; a >= 0;
						 a = nav125.vue.folders.nodes[(uint32)a].parent)
						if (!nav125.vue.folders.IsOpen(nav125.vue.folders.nodes[(uint32)a].id, kDefautRail))
							vue = false;
					if (vue)
						++n;
				}
				return n;
			};
			const uint32 replie = lignesRail();
			nav125.vue.folders.SetOpen(idSection, true, kDefautRail);
			nav125.ConstruireRail();
			const uint32 deplie = lignesRail();
			const bool lignesSuivent = replie < deplie;
			// ---- C. LE CYCLE COMPLET : DEUX IMAGES, UN VRAI CLIC -----------------------
			// Les deux moities separement ne suffisent pas : le defaut pouvait vivre dans
			// leur ENCHAINEMENT (l'image qui suit le clic reconstruit le rail avant de
			// dessiner). On refait donc ici, a l'identique, ce que fait `NkDrawFilePickerNav`
			// image par image -- l'empreinte AVANT le dessin, le dessin ensuite.
			const NkPaintRect zC{0.f, 0.f, 900.f, 500.f};
			float32 ySection = -1.f;
			{
				NkRecordingPaint r;
				NkComponentInput in;
				in.surfaceScale = 1.f;
				in.mouseX = -1000.f;
				in.mouseY = -1000.f;
				NkDrawContentBrowser(r, in, zC, nav125.vue, sty125, hb125);
				for (uint32 i = 0; i < (uint32)r.cmds.Size(); ++i)
					if (r.cmds[i].op == NkPaintOp::Text && r.cmds[i].text.Data()
						&& NkComponentDecl::StrEq(r.cmds[i].text.Data(), "Accès rapide"))
						ySection = r.cmds[i].y;
			}
			// IMAGE 1 : le clic sur le chevron de la section.
			{
				NkRecordingPaint r;
				NkComponentInput in;
				in.surfaceScale = 1.f;
				in.mouseX = zC.x + pad125 + chev125 * 0.5f;
				in.mouseY = ySection + rowH125 * 0.5f;
				in.mousePressed = true;
				in.mouseDown = true;
				NkDrawContentBrowser(r, in, zC, nav125.vue, sty125, hb125);
			}
			const bool plieAuClic = !nav125.vue.folders.IsOpen(idSection, kDefautRail);
			// IMAGE 2 : ce que le selecteur fait AVANT de dessiner -- il constate le
			// changement de depliage et rebatit le rail.
			{
				const nk_uint64 e2 = nav125.EmpreinteDeplie();
				if (e2 != nav125.empreinteDeplie) {
					nav125.empreinteDeplie = e2;
					nav125.ConstruireRail();
				}
			}
			uint32 apresCycle = 0u;
			{
				NkRecordingPaint r;
				NkComponentInput in;
				in.surfaceScale = 1.f;
				in.mouseX = -1000.f;
				in.mouseY = -1000.f;
				NkDrawContentBrowser(r, in, zC, nav125.vue, sty125, hb125);
				// on compte les LIBELLES d'entrees encore emis sous la section repliee
				for (uint32 q = 0; q < (uint32)nav125.vue.folders.nodes.Size(); ++q) {
					if (nav125.vue.folders.nodes[q].path.Empty()) continue;
					bool sousLaSection = false;
					for (int32 aa = nav125.vue.folders.nodes[q].parent; aa >= 0;
						 aa = nav125.vue.folders.nodes[(uint32)aa].parent)
						if (nav125.vue.folders.nodes[(uint32)aa].id == idSection)
							sousLaSection = true;
					if (!sousLaSection) continue;
					for (uint32 i = 0; i < (uint32)r.cmds.Size(); ++i)
						if (r.cmds[i].op == NkPaintOp::Text && r.cmds[i].text.Data()
							&& NkComponentDecl::StrEq(r.cmds[i].text.Data(),
													  nav125.vue.folders.nodes[q].label.Data()))
							++apresCycle;
				}
			}
			const bool cycleOk = plieAuClic && apresCycle == 0u;
			snprintf(det, sizeof(det),
					 "COMPOSANT : %d rangee(s) avant, %d apres le clic sur le chevron d'un titre VERROUILLE "
					 "-> %d ; TOUTE LA BANDE plie : %d rangee(s) rouvertes puis %d repliees par un clic sur "
					 "le libelle -> %d ; controle negatif (une entree ORDINAIRE ne plie pas sur son "
					 "libelle) : %d -> %d rangee(s), inchange=%d ; "
					 "HOTE : section ouverte au depart=%d, l'empreinte bouge=%d, le pliage SURVIT a la "
					 "reconstruction du rail=%d ; rail : %u ligne(s) replie contre %u deplie -> %d ; "
					 "CYCLE COMPLET (clic image 1, reconstruction image 2) : plie au clic=%d, %u libelle(s) "
					 "de la section encore peints (0 attendu) -> %d",
					 avant125, apres125, composantPlie ? 1 : 0, rouvertParLaBande, replieParLaBande,
					 bandePlie ? 1 : 0, ordinaireAvant, ordinaireApres, ordinaireInchange ? 1 : 0,
					 ouverteAuDepart ? 1 : 0, empreinteBouge ? 1 : 0,
					 survit ? 1 : 0, replie, deplie, lignesSuivent ? 1 : 0, plieAuClic ? 1 : 0,
					 apresCycle, cycleOk ? 1 : 0);
			check("125. (4) UNE SECTION PLIE SUR TOUTE SA BANDE, ET LE PLIAGE SURVIT A LA RECONSTRUCTION DU RAIL : "
				  "la MESURE a corrige le diagnostic -- le chevron n'etait PAS inerte, il pliait deja, et l'etat "
				  "survivait meme au rebatissage du rail. Ce qui etait inerte, c'etait TOUT LE RESTE de la bande : "
				  "`chevron_only_fold` vaut 1 par defaut et un titre est `locked`, donc un clic sur son libelle ne "
				  "pliait pas ET ne selectionnait pas -- il fallait viser seize pixels. Un titre n'ayant aucune "
				  "autre action, toute sa bande devient la cible du pliage ; une entree ORDINAIRE, elle, garde la "
				  "regle du chevron seul",
				  composantPlie && bandePlie && ordinaireInchange && ouverteAuDepart && empreinteBouge
					  && survit && lignesSuivent && cycleOk,
				  det);
			NkDirectory::Delete("sonde_replier", true);
		}
		// -- 126. LE LIBELLE D'UNE ENTREE SURVOLEE EST TOUJOURS PEINT (06/09). Rodolf :
		//    « lorsque je selectionne un dossier a gauche et que je le survole, son texte
		//    s'efface, mais des que je le quitte son texte reapparait. »
		//    ⚠️ MES SONDES PRECEDENTES NE POUVAIENT PAS LE VOIR : elles mesuraient le rail
		//       NON SURVOLE, ou survole sur UNE seule rangee. Une sonde verte pendant que
		//       Rodolf voit le defaut ne mesure pas le meme objet. Celle-ci survole CHAQUE
		//       entree l'une apres l'autre et verifie le texte de CELLE QU'ELLE SURVOLE.
		{
			char det[900];
			NkDirectory::Delete("sonde_survol", true);
			NkDirectory::CreateRecursive("sonde_survol/alpha/dedans");
			NkDirectory::CreateRecursive("sonde_survol/beta");
			NkDirectory::CreateRecursive("sonde_survol/gamma");
			const NkString rac126 =
				(NkPath(NkDirectory::GetCurrentDirectory()) / "sonde_survol").ToString();
			editorkit::NkFilePickerNavState nav126;
			char b126[512] = {};
			nav126.OpenPickerBase(editorkit::NkSelecteurEnregistrer, rac126.Data(), b126,
								  (int32)sizeof(b126), nullptr, nullptr);
			editorkit::NkFilePickerNavState::PoserRecent(nav126.recents, rac126.Data());
			nav126.relire = true;
			nav126.RelireDossier();
			NkComponentInstance inst126(NkContentBrowserDecl());
			inst126.SetParam("show_header", 0.f);
			inst126.SetParam("show_actions", 0.f);
			NkContentBrowserStyle sty126 = DemoStyle(nullptr);
			sty126.values = &inst126;
			NkContentBrowserHooks h126;
			const NkPaintRect z126{0.f, 0.f, 900.f, 600.f};
			const float32 rowH126 = NkTreeViewDecl().Metric("row_h");
			// 1. LE RELEVE A FROID : ou est peint le libelle de chaque entree.
			struct Ligne126 {
					char texte[96];
					float32 x, y;
			};
			Ligne126 lignes[64];
			uint32 nLignes = 0u;
			{
				NkRecordingPaint r;
				NkComponentInput in;
				in.surfaceScale = 1.f;
				in.mouseX = -1000.f;
				in.mouseY = -1000.f;
				NkDrawContentBrowser(r, in, z126, nav126.vue, sty126, h126);
				for (uint32 q = 0; q < (uint32)nav126.vue.folders.nodes.Size() && nLignes < 64u; ++q) {
					const char *lib = nav126.vue.folders.nodes[q].label.Data();
					if (!lib || !lib[0])
						continue;
					for (uint32 k = 0; k < (uint32)r.cmds.Size(); ++k)
						if (r.cmds[k].op == NkPaintOp::Text && r.cmds[k].text.Data()
							&& NkComponentDecl::StrEq(r.cmds[k].text.Data(), lib)) {
							snprintf(lignes[nLignes].texte, sizeof(lignes[nLignes].texte), "%s", lib);
							lignes[nLignes].x = r.cmds[k].x;
							lignes[nLignes].y = r.cmds[k].y;
							++nLignes;
							break;
						}
				}
			}
			// 2. ON SURVOLE CHAQUE ENTREE, L'UNE APRES L'AUTRE, et on exige SON texte.
			uint32 survoleesEffacees = 0u;
			char premiereEffacee[96];
			premiereEffacee[0] = 0;
			for (uint32 q = 0; q < nLignes; ++q) {
				NkRecordingPaint r;
				NkComponentInput in;
				in.surfaceScale = 1.f;
				in.mouseX = lignes[q].x + 4.f;
				in.mouseY = lignes[q].y + rowH126 * 0.5f;
				NkDrawContentBrowser(r, in, z126, nav126.vue, sty126, h126);
				bool vu = false;
				for (uint32 k = 0; k < (uint32)r.cmds.Size(); ++k)
					if (r.cmds[k].op == NkPaintOp::Text && r.cmds[k].text.Data()
						&& NkComponentDecl::StrEq(r.cmds[k].text.Data(), lignes[q].texte))
						vu = true;
				if (!vu) {
					++survoleesEffacees;
					if (!premiereEffacee[0])
						snprintf(premiereEffacee, sizeof(premiereEffacee), "%s", lignes[q].texte);
				}
			}
			// 3. LE COUPLE (FOND, TEXTE) EST DECIDE PAR UNE SEULE CONDITION.
			//    C'est ICI qu'etait le defaut, et aucune sonde ne pouvait le voir en
			//    comptant des textes : le texte ETAIT emis. Ce qui manquait, c'est que sa
			//    COULEUR et celle de son FOND venaient de deux conditions differentes --
			//    « survole » pour le fond, « actif » pour le texte. Sur la ligne a la fois
			//    ACTIVE et SURVOLEE, cela donnait le texte clair de l'etat actif sur le
			//    fond clair du survol : un nom invisible, qui revenait des qu'on quittait
			//    la ligne. C'est mot pour mot ce que Rodolf decrit.
			//    Le temoin verifie donc l'INVARIANT, pas la teinte : le dernier aplat qui
			//    couvre le libelle et la teinte du libelle doivent venir du MEME etat.
			uint32 couplesRompus = 0u;
			char premierRompu[96];
			premierRompu[0] = 0;
			for (uint32 q = 0; q < nLignes; ++q) {
				// on rend la ligne ACTIVE dans le modele, puis on la survole
				for (uint32 k = 0; k < (uint32)nav126.vue.folders.nodes.Size(); ++k)
					if (NkComponentDecl::StrEq(nav126.vue.folders.nodes[k].label.Data(),
											   lignes[q].texte))
						nav126.vue.folders.active = nav126.vue.folders.nodes[k].id;
				NkRecordingPaint r;
				NkComponentInput in;
				in.surfaceScale = 1.f;
				in.mouseX = lignes[q].x + 4.f;
				in.mouseY = lignes[q].y + rowH126 * 0.5f;
				NkDrawContentBrowser(r, in, z126, nav126.vue, sty126, h126);
				// le libelle, et le DERNIER aplat emis avant lui qui le couvre
				int32 iTexte = -1;
				for (uint32 k = 0; k < (uint32)r.cmds.Size(); ++k)
					if (r.cmds[k].op == NkPaintOp::Text && r.cmds[k].text.Data()
						&& NkComponentDecl::StrEq(r.cmds[k].text.Data(), lignes[q].texte))
						iTexte = (int32)k;
				if (iTexte < 0)
					continue; // deja compte par la mesure 2
				uint16 roleFond = 0u;
				const NkPaintCmd &t = r.cmds[(uint32)iTexte];
				for (int32 k = 0; k < iTexte; ++k) {
					const NkPaintCmd &c = r.cmds[(uint32)k];
					if (c.op != NkPaintOp::Fill && c.op != NkPaintOp::FillColor)
						continue;
					if (c.x <= t.x + 1.f && c.x + c.w >= t.x + 1.f && c.y <= t.y + 1.f
						&& c.y + c.h >= t.y + 1.f)
						roleFond = c.role;
				}
				// L'INVARIANT : un libelle peint avec la teinte de l'ETAT ACTIF doit se
				// poser sur le FOND de l'etat actif -- jamais sur celui du survol.
				const bool texteActif = t.role == sty126.activeMark ? false : t.role == sty126.badgeText;
				const bool fondSurvol = roleFond == sty126.chipBg;
				if (texteActif && fondSurvol) {
					++couplesRompus;
					if (!premierRompu[0])
						snprintf(premierRompu, sizeof(premierRompu), "%s", lignes[q].texte);
				}
			}
			nav126.vue.folders.active = 0;
			snprintf(det, sizeof(det),
					 "%u entree(s) de rail relevees ; SURVOL une a une : %u dont le texte disparait%s%s ; "
					 "ACTIVE **ET** SURVOLE (le geste de Rodolf) : %u couple(s) (fond, texte) rompus%s%s "
					 "-- texte de l'etat actif sur le fond du survol",
					 nLignes, survoleesEffacees, premiereEffacee[0] ? " (1re : " : "",
					 premiereEffacee[0] ? premiereEffacee : "", couplesRompus,
					 premierRompu[0] ? " (1re : " : "", premierRompu[0] ? premierRompu : "");
			check("126. QUI CHOISIT LE TEXTE CHOISIT LE FOND : le libelle d'une entree SELECTIONNEE devenait invisible "
				  "au survol et revenait quand on la quittait. Le texte ETAIT emis -- compter des textes ne pouvait "
				  "donc pas voir le defaut. Ce qui clochait, c'est que le couple (fond, texte) venait de DEUX "
				  "conditions : le fond du SURVOL etait peint par-dessus celui de l'ACTIF, pendant que la teinte du "
				  "libelle restait celle de l'actif -- clair sur clair. La ligne active garde desormais son fond. Le "
				  "temoin survole CHAQUE entree, puis chaque entree ACTIVE ET survolee, et verifie l'invariant",
				  survoleesEffacees == 0u && couplesRompus == 0u && nLignes >= 4u, det);
			NkDirectory::Delete("sonde_survol", true);
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
