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
#include "NKFileSystem/NkFile.h"

#include "Backend.h"
#include "DesignAI.h"
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

	/// Resolution de role pour la sonde : deterministe, sans theme, sans fichier.
	/// Deux noms differents peuvent collisionner — sans importance ici, ou l'on
	/// compare toujours deux rendus du MEME document ; ce serait a revoir le jour
	/// ou un essai voudrait attraper une erreur de role.
	inline uint16 ProbeResolveRole(const char *name) {
		uint32 h = 2166136261u;
		for (const char *p = name; p && *p; ++p)
			h = (h ^ (uint32)(uint8)*p) * 16777619u;
		return (uint16)(1u + (h % 250u));
	}

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
		NkDocumentHost host;
		host.resolve = &ProbeResolveRole;
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
		const NkGfxChoice cVk = NkGfxResolve(nullptr, kVulkanArgs, 2);
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
		const NkGfxChoice cEnvSeul = NkGfxResolve("vulkan", nullptr, 0);
		const char *kDx12[] = {"--gfx=dx12"};
		const NkGfxChoice cEnvEtLigne = NkGfxResolve("vulkan", kDx12, 1);
		const NkGfxChoice cRien = NkGfxResolve(nullptr, nullptr, 0);
		const NkGfxChoice cVide = NkGfxResolve("", nullptr, 0);
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

		// -- 32. LA RESOLUTION DES ROLES DE THEME ---------------------------
		// ⚠️ CETTE FAMILLE NAIT DU PREMIER TEMOIN VISUEL (18/08). L'application
		//    a ouvert sa fenetre et le navigateur de contenu s'est peint en
		//    MAGENTA FRANC. Ce n'est pas un accident d'affichage : c'est le repli
		//    delibere de `NkTheme::Get` pour un identifiant de role hors table --
		//    « ca doit sauter aux yeux, pas se fondre en noir ». Il a fait son
		//    travail.
		//
		//    LA CAUSE, lue dans le code et non devinee : les noms canoniques de
		//    `themedetail::RoleNames()` sont en snake_case (« panel_bg »,
		//    « panel_header », « input_bg »), `NkResolveRole` compare OCTET POUR
		//    OCTET, et les roles par defaut declares sont en PascalCase
		//    (« PanelBg », « PanelHeader », « InputBg »). Aucun ne tombe juste,
		//    donc NK_ROLE_INVALID (0xFFFF), donc magenta.
		//
		//    ⚠️ ET VOICI POURQUOI 21/21 PUIS 68/68 N'ONT RIEN VU : la sonde
		//    resout les roles avec `ProbeResolveRole`, qui HACHE n'importe quel
		//    nom vers 1..250. Il ne rend JAMAIS NK_ROLE_INVALID. Un resolveur qui
		//    dit oui a tout ne peut pas voir un nom faux -- c'est la meme famille
		//    de defaut que « ca repond toujours ». La reponse n'est pas de changer
		//    le resolveur de la sonde (il doit rester injectif pour que les essais
		//    de dessin comparent des couleurs distinctes), c'est d'appeler ICI le
		//    resolveur REEL, celui que l'application utilise.
		{
			// 32a. LE RESOLVEUR REEL SAIT DIRE NON -- a etablir AVANT de s'en
			//      servir pour juger quoi que ce soit. Un resolveur permissif
			//      rendrait 32b et l'audit muets.
			const uint16 connu = NkResolveRole("panel_bg");
			const uint16 inconnu = NkResolveRole("role_qui_n_existe_pas_du_tout");
			check("32a. le resolveur REEL accepte un nom canonique ET REFUSE un nom faux",
				  connu != NK_ROLE_INVALID && inconnu == NK_ROLE_INVALID,
				  connu != NK_ROLE_INVALID ? "panel_bg resolu, nom faux rejete"
										   : "PANEL_BG LUI-MEME NE RESOUT PAS");

			// 32b. LE RESOLVEUR DE LA SONDE EST PERMISSIF, ET ON L'ECRIT.
			//      Ce n'est pas un reproche : c'est la raison pour laquelle les
			//      essais de dessin ne prouvent RIEN sur la validite des noms.
			check("32b. et le resolveur de la SONDE, lui, dit oui a tout (donc ne juge pas les noms)",
				  ProbeResolveRole("role_qui_n_existe_pas_du_tout") != NK_ROLE_INVALID &&
					  ProbeResolveRole("panel_bg") != ProbeResolveRole("border"),
				  "permissif ET injectif : bon pour comparer des dessins, aveugle aux noms");

			// 32c. MES PROPRES NOMS DE ROLES RESOLVENT. C'est la part qui
			//      m'appartient, et elle doit etre verte.
			const char *kMiens[] = {"panel_bg", "border", "text", "text_muted"};
			bool miensOk = true;
			for (uint32 m = 0; m < 4; ++m)
				if (NkResolveRole(kMiens[m]) == NK_ROLE_INVALID)
					miensOk = false;
			check("32c. les 4 roles employes par `Renderers.h` resolvent tous", miensOk,
				  "panel_bg, border, text, text_muted");

			// 32d. L'AUDIT DU COMPOSANT DE REFERENCE -- DIAGNOSTIC, PAS VERDICT.
			//      `NkContentBrowserModel.h` n'est pas mon fichier : je COMPTE et
			//      je NOMME, je ne corrige pas. L'assertion porte sur ce qui est
			//      a moi -- que l'audit ait REELLEMENT regarde quelque chose --
			//      et le detail part au canal.
			const NkComponentDecl &d = NkContentBrowserDecl();
			uint16 vus = 0, casses = 0;
			NkString liste;
			for (uint16 t = 0; t < d.tokenCount; ++t) {
				++vus;
				const char *role = d.tokens[t].defaultRole;
				if (NkResolveRole(role) == NK_ROLE_INVALID) {
					++casses;
					if (!liste.Empty())
						liste.Append(", ");
					liste.Append(d.tokens[t].name);
					liste.Append("->");
					liste.Append(role);
				}
			}
			snprintf(buf, sizeof(buf), "%u jetons examines, %u role(s) NON RESOLU(S)", (uint32)vus,
					 (uint32)casses);
			check("32d. l'audit a REELLEMENT parcouru les jetons declares (sinon 0 casse ne dit rien)",
				  vus > 0, buf);

			rep.Append("\n--- audit des roles declares du navigateur de contenu ---\n");
			rep.Append(buf);
			rep.Append("\n");
			if (casses > 0) {
				rep.Append("  ⚠️ NON RESOLUS (fichier d'un autre agent, porte au canal) : ");
				rep.Append(liste);
				rep.Append("\n  cause : roles declares en PascalCase, table canonique en snake_case.\n");
			}
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
