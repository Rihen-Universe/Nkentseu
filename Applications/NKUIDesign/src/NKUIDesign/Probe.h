#pragma once
// -----------------------------------------------------------------------------
// @File    Probe.h
// @Brief   LE TEMOIN DE LA TRANCHE, sans fenetre et sans GPU.
// @Author  Rihen
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

		rep.Append("=== NKUIDesign --probe : la declaration est-elle LUE ? ===\n");
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
		const float32 top0 = dcl.Metric("header_h") + dcl.Metric("toolbar_h") + dcl.Metric("row_h");
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
		check("14. le bloc `controller` de la spec .nkgui v0.2 s'emet depuis la declaration",
			  n > 0 && decl.eventCount == 5, buf);

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
				if (!NkCanonicalRoleName(kCas[i].ecrit, got, sizeof(got)) ||
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
			const bool nomme = NkRoleAudit::FaultCount() == 1 &&
							   SameText(NkRoleAudit::Faults()[0].name.Data(),
										"RoleQuiNExistePasDuTout");
			NkRoleAudit::Reset();
			NkDesignResolveRole("panel_bg");
			const bool muetQuandTouVaBien = NkRoleAudit::FaultCount() == 0;
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
			//    La regle juste est STRUCTURELLE, pas devinatoire : dans un flux
			//    correctement imbrique, le dernier `PushClip` avant le premier
			//    `PopClip` est le clip LE PLUS INTERNE -- ici, la grille.
			float32 gridX = -1.f;
			uint32 iClip = 0, nbClips = 0;
			NkString clips;
			for (uint32 i = 0; i < (uint32)g.cmds.Size(); ++i) {
				if (g.cmds[i].op == NkPaintOp::PopClip)
					break;
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
				for (uint32 i = fin + 1; i < (uint32)flux.cmds.Size(); ++i) {
					const NkPaintCmd &c = flux.cmds[i];
					if (c.op == NkPaintOp::PushClip || c.op == NkPaintOp::PopClip ||
						bandePleineLargeur(c))
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
			NkRecordingPaint faute;
			for (uint32 i = 0; i < (uint32)g.cmds.Size(); ++i)
				faute.cmds.PushBack(g.cmds[i]);
			NkPaintCmd intrus;
			intrus.op = NkPaintOp::Fill;
			intrus.x = gridX - 40.f; // commence DANS la colonne
			intrus.y = bandeTop + 10.f;
			intrus.w = 90.f;		 // et finit 50 px DANS la grille
			intrus.h = 20.f;
			faute.cmds.PushBack(intrus);
			uint32 fCol = 0, fGrille = 0;
			float32 fPire = 0.f;
			// L'intrus est pose AVANT la frontiere de parcours : on etend `fin`
			// jusqu'a lui pour qu'il soit juge du cote « colonne ».
			compter(faute, (uint32)faute.cmds.Size(), fCol, fGrille, fPire, nullptr);
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
			NkDocumentHost::FillDemoTree(t);
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
			NkDocumentHost::FillDemoTree(t2);
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

		char tail[128];
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
