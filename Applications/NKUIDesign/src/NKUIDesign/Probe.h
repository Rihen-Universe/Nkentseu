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

		rep.Append("\n--- le bloc .nkgui produit, tel quel ---\n");
		rep.Append(ctrl);

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
