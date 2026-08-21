#pragma once
// -----------------------------------------------------------------------------
// @File    NkTreeViewProbe.h
// @Brief   LE BANC de l'arbre, sans fenetre, sans GPU et SANS CIBLE DE BUILD.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  COMMENT ON LE LANCE — deux commandes, depuis la racine du depot
// =============================================================================
//  ⚠️ Ce banc ne demande AUCUNE cible Jenga et n'est lie par aucune. C'est
//     volontaire : la sonde du navigateur de contenu vit dans l'application
//     `NKUIDesign`, donc la lancer suppose que l'application construit. Celle-ci
//     se compile a la main, en une commande, meme si tout le reste est casse.
//
//  1. LE BANC DE NEUTRALITE — « le modele compile-t-il sans NKGui ? » (C3) :
//
//     g++ -std=c++17 -Wall -fsyntax-only \
//       -I Engine/NKEditorKit/src \
//       -I Kernel/Foundation/NKCore/src -I Kernel/Foundation/NKContainers/src \
//       -I Kernel/Foundation/NKMemory/src -I Kernel/Foundation/NKMath/src \
//       -I Kernel/Foundation/NKPlatform/src \
//       -x c++ Engine/NKEditorKit/src/NKEditorKit/Components/NkTreeViewModel.h
//
//     ⚠️ SON TEMOIN NEGATIF EST OBLIGATOIRE, et il n'est pas optionnel a lire :
//        la MEME commande sur `Engine/NKEditorKit/src/NKEditorKit/NkEditorInspector.h`
//        — un en-tete du MEME kit qui, lui, depend de NKGui — doit ECHOUER.
//        Sans lui, un `0` ne veut rien dire : il vaudrait aussi pour un fichier
//        vide. C'est la correction que la sonde du navigateur a deja payee (son
//        premier temoin echouait parce qu'un chemin manquait, pas parce que le
//        modele refusait quoi que ce soit).
//
//  2. LE BANC DE COMPORTEMENT — celui de ce fichier. IL SE LIE, et avec
//     `clang++`, PAS `g++` : le depot est un depot **clang-mingw**, c'est
//     `config/toolchain.jenga` qui le dit (`nk-windows-clang-mingw`). Deux
//     conclusions trop rapides ont ete payees ici, autant les ecrire :
//
//     ⚠️ (a) CHAQUE MODULE A SON PROPRE `pch/`, et ses sources incluent
//            `pch.h`. Sans `-I Kernel/Foundation/<Module>/pch`, 16 sources du
//            noyau sur 25 echouent sur `'pch.h' file not found` — ce qui n'a
//            RIEN a voir avec le composant qu'on essaie de lier.
//     ⚠️ (b) `ld` RAPPORTE tres bien les symboles manquants sous clang++. Le
//            mutisme observe auparavant venait de la chaine g++, pas du lieur.
//            Ne pas rediagnostiquer : lire ce qu'il imprime.
//
//     CF="-std=c++17 --target=x86_64-w64-windows-gnu \
//         -DWINVER=0x0601 -D_WIN32_WINNT=0x0601 -include new \
//         -I Engine/NKEditorKit/src \
//         -I Kernel/Foundation/{NKCore,NKContainers,NKMemory,NKMath,NKPlatform}/src \
//         -I Kernel/Foundation/{NKCore,NKContainers,NKMemory,NKMath,NKPlatform}/pch"
//
//     # un pilote de trois lignes, hors depot, qui allume juste la garde :
//     #   #define NKTREEVIEW_PROBE_MAIN
//     #   #include "NKEditorKit/Components/NkTreeViewProbe.h"
//     clang++ $CF -c probe_main.cpp -o probe_main.o
//     clang++ $CF -c Engine/NKEditorKit/src/NKEditorKit/Components/NkTreeViewDraw.cpp -o draw.o
//     clang++ $CF -c Engine/NKEditorKit/src/NKEditorKit/Components/NkComponentRegistry.cpp -o reg.o
//     # + NKContainers/src/**/String/**.cpp (Encoding/ compris), NKMemory/*.cpp, NKCore/**.cpp
//     clang++ --target=x86_64-w64-windows-gnu *.o -o nktree_probe && ./nktree_probe
//
//     `-include new` est OBLIGATOIRE : les sources du noyau supposent le PCH
//     qui l'apporte. MSVC n'est PAS une voie : il reclame `/Zc:__cplusplus` et
//     `/Zc:preprocessor`, et bute quand meme dans `NKPlatform`.
//
// =============================================================================
//  CE QU'IL PROUVE — et il faut le lire avec sa portee
// =============================================================================
//  Deux choses, et rien de plus :
//   (a) *la declaration modifiee change les commandes de dessin produites* —
//       donc la chaine declaration -> fichier -> dessin est reelle ;
//   (b) *les comportements d'arbre que les trois copies existantes ont chacune
//       a moitie — recursion, plage Maj+clic, garde anti-cycle, renommage —
//       partent bien du composant partage.*
//
//  ⚠️ CE QU'IL NE PROUVE PAS, DIFFERE ET NOMME :
//     - la conformite aux planches (`AetherionWorldOutlinerDetailsLight.png` et
//       les deux captures du 18/08) : ca se voit a l'ecran, pas ici ;
//     - tout ce qui depend d'une largeur de texte REELLE — ellipse, curseur de
//       saisie, position du chemin en liste plate. Les metriques du peintre
//       enregistreur sont fictives et le disent (`NkRecordingPaint.h`) ;
//     - qu'un geste soit agreable : un banc pose des positions, il ne juge pas.
//     **Aucun temoin visuel n'a ete pris, et aucun n'est revendique.**
//
//  LES TROIS CONTROLES QUI RENDENT LES AUTRES LISIBLES, sans lesquels un banc
//  qui ne sait dire que « oui » ne mesure rien :
//   1. TEMOIN DE BRUIT (essai 1) — la meme mesure repetee sans rien changer. Le
//      peintre enregistreur n'a ni horloge ni aleatoire : le plancher attendu
//      est EXACTEMENT zero, et on le VERIFIE au lieu de le supposer.
//   2. CONTROLE POSITIF (essai 2) — un ecrasement connu DOIT changer le dessin.
//   3. CONTROLE NEGATIF (essais 8, 12, 20b, 21b, 25b) — ce qui ne doit RIEN
//      changer ne change rien, et la commande sait pourtant trouver.
//
//  ⚠️ REGIMES COUVERTS, ecrits AVEC le resultat (face n.7 de la grille du
//     corpus) : un seul jeu de donnees (7 noeuds, 3 racines, profondeur 2),
//     panneau 320x600, echelle 1.0 sauf a l'essai 16, les DEUX variantes,
//     filtre vide sauf a l'essai 19.
//     **NON couverts** : arbre vide, filtre actif en variante plate, profondeur
//     > 2, plus de noeuds que de lignes visibles (le defilement n'est exerce
//     qu'a vide), panneau plus etroit que ses colonnes, un `acceptNode` qui
//     refuse, plusieurs arbres dans la meme image.
// -----------------------------------------------------------------------------

#include "NKEditorKit/Components/NkComponentCheck.h"
#include "NKEditorKit/Components/NkComponentInstance.h"
#include "NKEditorKit/Components/NkRecordingPaint.h"
#include "NKEditorKit/Components/NkTreeViewModel.h"

#include <cstdio>

namespace nkentseu {
	namespace editorkit {
		namespace treeprobe {

			// ── LE COMPTEUR D'EVENEMENTS ────────────────────────────────────────
			// Le second bout de la verification. La declaration dit que le composant
			// emet six evenements ; rien dans le compilateur ne verifie qu'il les
			// emet. On branche donc les crochets et on compte les departs — c'est le
			// meme angle mort que « le point de verite d'un encodage est son
			// consommateur ».
			struct Events {
					int32 selects = 0, activates = 0, menus = 0, expands = 0;
					int32 renames = 0, drops = 0, flags = 0;
					int32 lastIndex = -1, lastMenuIndex = -2;
					bool lastOpen = false;
					uint8 lastDropPos = 255;
					bool anyEmptyPayload = false;
					char lastOld[64] = {};
					char lastNew[64] = {};

					static void Copy(char *d, uint32 cap, const char *s) {
						uint32 i = 0;
						if (s)
							for (; s[i] && i + 1 < cap; ++i)
								d[i] = s[i];
						d[i] = '\0';
					}
					/// Un `id` vide est LEGITIME quand `index` vaut -1 (le fond du
					/// panneau) : on ne compte une charge vide que sur une vraie ligne.
					static void Note(Events *e, int32 index, const char *id) {
						e->lastIndex = index;
						if (index >= 0 && (!id || !*id))
							e->anyEmptyPayload = true;
					}
					static void OnSelect(void *u, int32 index, const char *id) {
						Events *e = (Events *)u;
						++e->selects;
						Note(e, index, id);
					}
					static void OnActivate(void *u, int32 index, const char *id) {
						Events *e = (Events *)u;
						++e->activates;
						Note(e, index, id);
					}
					static void OnMenu(void *u, int32 index, float32, float32) {
						Events *e = (Events *)u;
						++e->menus;
						e->lastMenuIndex = index;
					}
					static void OnExpand(void *u, int32 index, const char *id, bool open) {
						Events *e = (Events *)u;
						++e->expands;
						e->lastOpen = open;
						Note(e, index, id);
					}
					static void OnRename(void *u, int32 index, const char *id, const char *o,
										 const char *n2) {
						Events *e = (Events *)u;
						++e->renames;
						Copy(e->lastOld, sizeof(e->lastOld), o);
						Copy(e->lastNew, sizeof(e->lastNew), n2);
						Note(e, index, id);
					}
					static void OnDrop(void *u, const char *, const char *, uint8 pos, const char *) {
						Events *e = (Events *)u;
						++e->drops;
						e->lastDropPos = pos;
					}
					static void OnFlag(void *u, int32 index, const char *id, uint8, bool) {
						Events *e = (Events *)u;
						++e->flags;
						Note(e, index, id);
					}
			};

			inline NkTreeViewHooks MakeHooks(Events *e) {
				NkTreeViewHooks h;
				h.user = e;
				h.onSelect = &Events::OnSelect;
				h.onActivate = &Events::OnActivate;
				h.onContextMenu = &Events::OnMenu;
				h.onExpand = &Events::OnExpand;
				h.onRename = &Events::OnRename;
				h.onDrop = &Events::OnDrop;
				h.onToggleFlag = &Events::OnFlag;
				return h;
			}

			// ── LE JEU DE DONNEES ───────────────────────────────────────────────
			// C'est l'arbre de la planche `AetherionWorldOutlinerDetailsLight.png`,
			// aux memes libelles : 7 noeuds, 3 racines, profondeur 2. Reprendre la
			// planche plutot qu'inventer un arbre evite un banc qui valide un cas que
			// personne ne rencontrera.
			inline void FillDemo(NkTreeViewModel &m) {
				struct Row {
						const char *label;
						int32 parent;
						bool hidden;
				};
				static const Row kRows[] = {
					{"Environnement", -1, false},		   // 0
					{"Sol", 0, false},					   // 1
					{"Eclairage", 0, false},			   // 2
					{"Lumiere directionnelle", 2, false},  // 3
					{"Lumiere ponctuelle", 2, true},	   // 4
					{"PersonnagePrincipal", -1, false},	   // 5
					{"Camera_Cinematique", -1, true},	   // 6
				};
				m.nodes.Clear();
				for (uint32 i = 0; i < sizeof(kRows) / sizeof(kRows[0]); ++i) {
					NkTreeNode n;
					// ⚠️ L'IDENTITE NE VAUT JAMAIS 0 : `0` signifie « aucun noeud ».
					//    Un jeu d'essai qui commencerait a 0 rendrait le premier noeud
					//    indistinguable de l'absence de noeud, et l'essai passerait
					//    pour une mauvaise raison.
					n.id = (nk_uint64)(i + 1) * 1000ull + 7ull;
					n.parent = kRows[i].parent;
					n.label = NkString(kRows[i].label);
					// Un chemin NON VIDE, et c'est volontaire : c'est la charge que les
					// evenements portent vers un blueprint. Un banc qui le laisserait
					// vide validerait une charge inutilisable sans s'en apercevoir.
					n.path = NkString("/monde/");
					n.path.Append(kRows[i].label);
					n.icon = (uint16)(20 + i);
					n.kindRole = (uint16)(11 + (i % 4));
					n.kindLabel = "Acteur";
					n.hidden = kRows[i].hidden;
					m.nodes.PushBack(n);
				}
			}

			inline NkTreeViewStyle DemoStyle(const NkComponentInstance *inst) {
				NkTreeViewStyle s;
				// Roles arbitraires mais DISTINCTS : le peintre enregistreur rend une
				// couleur injective par role, donc une erreur de role se verrait dans
				// le flux. Deux roles egaux la rendraient invisible.
				s.panelBg = 1;
				s.headerBg = 2;
				s.border = 3;
				s.text = 4;
				s.textMuted = 5;
				s.rowHover = 6;
				s.activeMark = 7;
				s.activeText = 8;
				s.chosenMark = 9;
				s.guide = 10;
				s.dropMark = 11;
				s.iconTint = 12;
				s.dimTint = 13;
				s.icons.chevronClosed = 101;
				s.icons.chevronOpen = 102;
				s.icons.eyeOpen = 103;
				s.icons.eyeClosed = 104;
				s.icons.lockOpen = 105;
				s.icons.lockClosed = 106;
				s.values = inst;
				return s;
			}

			inline const NkPaintRect kPanel = {0.f, 0.f, 320.f, 600.f};

			/// Une passe de dessin sur un modele DONNE — le modele persiste entre les
			/// passes, parce qu'un arbre a de la memoire (pliage, ancre, source de
			/// glisser) et qu'un banc qui le recreerait a chaque image ne pourrait
			/// tester aucun geste en deux temps.
			inline NkTreeViewResult RenderInto(NkRecordingPaint &rec, NkTreeViewModel &m,
											   const NkComponentInstance *inst,
											   const NkComponentInput &in, NkTreeViewHooks *hooks) {
				rec.Reset();
				const NkTreeViewStyle s = DemoStyle(inst);
				NkTreeViewHooks none;
				return NkDrawTreeView(rec, in, kPanel, m, s, hooks ? *hooks : none);
			}

			/// Passe sur un modele NEUF — pour les essais qui comparent deux dessins.
			inline void RenderFresh(NkRecordingPaint &rec, const NkComponentInstance *inst,
									const NkComponentInput &in) {
				NkTreeViewModel m;
				FillDemo(m);
				m.active = m.nodes[0].id;
				m.chosen.PushBack(m.nodes[0].id);
				RenderInto(rec, m, inst, in, nullptr);
			}

			// ── OUTILS DE LECTURE DU FLUX ───────────────────────────────────────
			/// Abscisse du texte d'un libelle donne. C'est ce qui rend l'essai
			/// d'indentation GEOMETRIQUE et non declaratif : on ne demande pas au
			/// composant s'il indente, on mesure ou il a ecrit.
			inline float32 TextXOf(const NkRecordingPaint &r, const char *label) {
				for (uint32 i = 0; i < (uint32)r.cmds.Size(); ++i) {
					if (r.cmds[i].op != NkPaintOp::Text)
						continue;
					const char *t = r.cmds[i].text.Data();
					if (!t)
						continue;
					const char *a = t, *b = label;
					for (; *a && *b; ++a, ++b)
						if (*a != *b)
							break;
					if (*a == '\0' && *b == '\0')
						return r.cmds[i].x;
				}
				return -1.f;
			}
			inline bool HasText(const NkRecordingPaint &r, const char *label) {
				return TextXOf(r, label) >= 0.f;
			}

			/// Centre vertical de la ligne de rang `ordinal`, calcule DEPUIS la
			/// declaration. ⚠️ Ne jamais l'ecrire en dur : la sonde du navigateur de
			/// contenu a paye exactement ca — un point de clic ecrit a la main
			/// tombait hors de la zone visee, et l'un des essais PASSAIT quand meme,
			/// a vide.
			inline float32 RowCenterY(const NkComponentDecl &d, int32 ordinal) {
				const float32 top = d.Metric("header_h") + d.Metric("search_h");
				return top + ((float32)ordinal + 0.5f) * d.Metric("row_h");
			}
			/// Abscisse tombant sur le CHEVRON d'un noeud de profondeur `depth`.
			inline float32 ChevronX(const NkComponentDecl &d, int32 depth) {
				return d.Metric("row_pad") + (float32)depth * d.Metric("indent_step") +
					   d.Metric("chevron_w") * 0.5f;
			}
			/// Abscisse tombant sur le LIBELLE d'un noeud de profondeur `depth`.
			/// (colonne oeil affichee, cadenas masque — les defauts declares)
			inline float32 LabelX(const NkComponentDecl &d, int32 depth) {
				return d.Metric("row_pad") + (float32)depth * d.Metric("indent_step") +
					   d.Metric("chevron_w") + d.Metric("icon_w") * 2.f + d.Metric("row_pad");
			}

			/// Comparaison locale : ce banc ne tire aucune dependance de chaine.
			inline bool StrEqLocal(const char *a, const char *b) {
				return NkComponentDecl::StrEq(a, b);
			}

			// ── LA SONDE ────────────────────────────────────────────────────────
			inline int Run() {
				const NkComponentDecl &decl = NkTreeViewDecl();
				int pass = 0, total = 0;
				char buf[256];

				auto check = [&](const char *label, bool ok, const char *detail) {
					++total;
					if (ok)
						++pass;
					fputs(ok ? "  [ok]    " : "  [ECHEC] ", stdout);
					fputs(label, stdout);
					if (detail && *detail) {
						fputs("  -- ", stdout);
						fputs(detail, stdout);
					}
					fputc('\n', stdout);
				};

				fputs("=== NkTreeView --probe : la forme tient-elle sur un SECOND composant ? ===\n",
					  stdout);
				fputs("Seance SANS GPU : aucune fenetre, aucun temoin visuel pris ni revendique.\n",
					  stdout);
				fputs("Regime : 7 noeuds (l'arbre de la planche du 18/08), 3 racines, profondeur 2,\n"
					  "panneau 320x600, echelle 1.0 (sauf essai 16), les DEUX variantes.\n"
					  "Hors regime : arbre vide, filtre en variante plate, profondeur > 2,\n"
					  "defilement charge, panneau etroit, acceptNode qui refuse.\n\n",
					  stdout);

				const NkComponentInput idle;

				// ── 1. TEMOIN DE BRUIT ──────────────────────────────────────────
				NkRecordingPaint a1, a2;
				RenderFresh(a1, nullptr, idle);
				RenderFresh(a2, nullptr, idle);
				snprintf(buf, sizeof(buf), "%u commandes, %u differences", (uint32)a1.cmds.Size(),
						 a1.DiffCount(a2));
				check("1.  TEMOIN DE BRUIT : deux passes identiques -> 0 difference",
					  a1.DiffCount(a2) == 0, buf);
				check("1b. la passe produit REELLEMENT quelque chose (sinon 0=0 ne prouve rien)",
					  a1.cmds.Size() > 30 && HasText(a1, "Environnement"), buf);

				// ── 2. CONTROLE POSITIF ─────────────────────────────────────────
				NkComponentInstance mi(decl);
				mi.SetMetric("row_h", 40.f);
				NkRecordingPaint b;
				RenderFresh(b, &mi, idle);
				snprintf(buf, sizeof(buf), "row_h 24 -> 40 : %u commandes differentes",
						 a1.DiffCount(b));
				check("2.  CONTROLE POSITIF : une METRIQUE ecrasee change le dessin",
					  a1.DiffCount(b) > 0, buf);

				// ── 3. UN PARAMETRE ─────────────────────────────────────────────
				NkComponentInstance pi(decl);
				pi.SetParam("show_lock", 1.f);
				NkRecordingPaint c;
				RenderFresh(c, &pi, idle);
				snprintf(buf, sizeof(buf), "show_lock 0 -> 1 : %u differences", a1.DiffCount(c));
				check("3.  un PARAMETRE ecrase change le dessin", a1.DiffCount(c) > 0, buf);

				// ── 4. UNE VARIANTE ─────────────────────────────────────────────
				NkComponentInstance vi(decl);
				vi.SetVariantByName("flat_list");
				NkRecordingPaint d2;
				RenderFresh(d2, &vi, idle);
				snprintf(buf, sizeof(buf), "tree -> flat_list : %u differences", a1.DiffCount(d2));
				check("4.  une VARIANTE change la mise en page (un modele, N rendus)",
					  a1.DiffCount(d2) > 0, buf);

				// ── 5. INSTANCE VIERGE ──────────────────────────────────────────
				NkComponentInstance pristine(decl);
				NkRecordingPaint e;
				RenderFresh(e, &pristine, idle);
				snprintf(buf, sizeof(buf), "%u differences (attendu : 0)", a1.DiffCount(e));
				check("5.  une instance VIERGE se comporte comme la declaration",
					  a1.DiffCount(e) == 0, buf);

				// ── 6. ALLER-RETOUR PAR LE FICHIER ──────────────────────────────
				NkString text;
				mi.Save(text);
				NkComponentInstance reloaded(decl);
				uint32 unknown = 0, applied = 0;
				const bool loaded = reloaded.Load(text.Data(), &unknown, &applied);
				NkRecordingPaint f;
				RenderFresh(f, &reloaded, idle);
				snprintf(buf, sizeof(buf), "entete=%d applique=%u inconnu=%u, %u diff. avec l'ecrit",
						 loaded ? 1 : 0, applied, unknown, b.DiffCount(f));
				check("6.  ALLER-RETOUR FICHIER : ecrit -> texte -> relu -> MEME dessin",
					  loaded && unknown == 0 && applied > 0 && b.DiffCount(f) == 0, buf);
				snprintf(buf, sizeof(buf), "%u differences (doit rester > 0)", a1.DiffCount(f));
				check("6b. et le dessin relu differe TOUJOURS de la reference", a1.DiffCount(f) > 0,
					  buf);

				// ── 7. UN JETON REAFFECTE ───────────────────────────────────────
				NkComponentInstance ti(decl);
				ti.SetTokenRole("active_mark", "PanelHeader");
				check("7.  un JETON se reaffecte a un autre role, et l'instance le retient",
					  ti.IsTokenOverridden("active_mark"), ti.TokenRole("active_mark"));

				// ── 8. CONTROLE NEGATIF ─────────────────────────────────────────
				NkComponentInstance bogus(decl);
				bogus.SetMetric("cle_qui_nexiste_pas", 999.f);
				NkRecordingPaint g;
				RenderFresh(g, &bogus, idle);
				snprintf(buf, sizeof(buf), "%u ecrasements retenus, %u differences (attendu : 0 et 0)",
						 bogus.OverrideCount(), a1.DiffCount(g));
				check("8.  CONTROLE NEGATIF : une cle inconnue de la declaration ne change RIEN",
					  bogus.OverrideCount() == 0 && a1.DiffCount(g) == 0, buf);
				NkComponentInstance perime(decl);
				uint32 u2 = 0, a2c = 0;
				perime.Load("nkuicomp 1\nmetrique disparue = 3\nmetrique row_h = 30\n", &u2, &a2c);
				snprintf(buf, sizeof(buf), "inconnu=%u applique=%u", u2, a2c);
				check("8b. un fichier a moitie perime se charge, et COMPTE l'inconnu",
					  u2 == 1 && a2c == 1, buf);

				// ── 9. LES BORNES VIENNENT DE LA DECLARATION ────────────────────
				NkComponentInstance clamp(decl);
				clamp.SetMetric("row_h", -12.f);
				snprintf(buf, sizeof(buf), "-12 ramene a %.1f", clamp.Metric("row_h"));
				check("9.  une longueur negative est refusee par l'INSTANCE, pas par l'appelant",
					  clamp.Metric("row_h") >= 0.f, buf);

				// ── 10. RECURSION : REPLIER CACHE LA DESCENDANCE ────────────────
				// L'essai le plus specifique a ce composant. On replie « Eclairage »
				// (2 enfants) et on compte les lignes emises : 7 -> 5, exactement.
				{
					NkTreeViewModel m;
					FillDemo(m);
					NkRecordingPaint r0;
					const NkTreeViewResult before = RenderInto(r0, m, nullptr, idle, nullptr);
					m.SetOpen(m.nodes[2].id, false, true);
					NkRecordingPaint r1;
					const NkTreeViewResult after = RenderInto(r1, m, nullptr, idle, nullptr);
					snprintf(buf, sizeof(buf), "%d lignes -> %d apres pliage de « Eclairage »",
							 before.visibleCount, after.visibleCount);
					check("10. RECURSION : replier un noeud retire EXACTEMENT sa descendance",
						  before.visibleCount == 7 && after.visibleCount == 5, buf);
					check("10b. et ses deux enfants ont vraiment disparu du flux (pas seulement du "
						  "compteur)",
						  HasText(r0, "Lumiere ponctuelle") && !HasText(r1, "Lumiere ponctuelle"), "");
				}

				// ── 11. PROFONDEUR : L'INDENTATION EST GEOMETRIQUE ──────────────
				// On ne demande pas au composant s'il indente : on mesure OU il a
				// ecrit. L'ecart entre un enfant et son parent doit valoir exactement
				// une metrique `indent_step`, et rien d'autre.
				{
					const float32 xRoot = TextXOf(a1, "Environnement");
					const float32 xKid = TextXOf(a1, "Sol");
					const float32 xGrand = TextXOf(a1, "Lumiere directionnelle");
					const float32 step = decl.Metric("indent_step");
					const float32 d1 = xKid - xRoot, d3 = xGrand - xRoot;
					snprintf(buf, sizeof(buf), "racine=%.1f enfant=%.1f petit-enfant=%.1f, pas=%.1f",
							 xRoot, xKid, xGrand, step);
					check("11. la PROFONDEUR se lit dans la geometrie : 1 cran, puis 2",
						  xRoot >= 0.f && d1 > step - 0.01f && d1 < step + 0.01f &&
							  d3 > step * 2.f - 0.01f && d3 < step * 2.f + 0.01f,
						  buf);
				}

				// ── 12. LA VARIANTE PLATE IGNORE LE PLIAGE, SANS Y TOUCHER ──────
				// C'est la condition C2 rendue mesurable : la variante change ce qui
				// s'affiche, et ne touche RIEN dans le modele.
				{
					NkTreeViewModel m;
					FillDemo(m);
					m.SetOpen(m.nodes[2].id, false, true);
					m.active = m.nodes[5].id;
					m.chosen.PushBack(m.nodes[5].id);
					const uint32 toggledBefore = (uint32)m.toggled.Size();
					NkComponentInstance flat(decl);
					flat.SetVariantByName("flat_list");
					NkRecordingPaint rf;
					const NkTreeViewResult rv = RenderInto(rf, m, &flat, idle, nullptr);
					snprintf(buf, sizeof(buf), "%d lignes en plat (7 attendu), toggled %u -> %u",
							 rv.visibleCount, toggledBefore, (uint32)m.toggled.Size());
					check("12. C2 : la variante plate montre tout SANS modifier l'etat de pliage",
						  rv.visibleCount == 7 && m.toggled.Size() == toggledBefore &&
							  m.active == m.nodes[5].id && m.chosen.Size() == 1,
						  buf);
				}

				// ── 13. onSelect PART, AVEC SA CHARGE ───────────────────────────
				{
					NkTreeViewModel m;
					FillDemo(m);
					Events ev;
					NkTreeViewHooks h = MakeHooks(&ev);
					NkComponentInput click;
					click.mouseX = LabelX(decl, 0);
					click.mouseY = RowCenterY(decl, 5); // « PersonnagePrincipal »
					click.mousePressed = true;
					NkRecordingPaint r;
					RenderInto(r, m, nullptr, click, &h);
					snprintf(buf, sizeof(buf), "onSelect=%d index=%d", ev.selects, ev.lastIndex);
					check("13. onSelect part au clic, avec l'index de la ligne visee",
						  ev.selects == 1 && ev.lastIndex == 5, buf);
					// ⚠️ `ev.selects > 0` FAIT PARTIE DE LA CONDITION : sans lui,
					//    l'essai passerait quand RIEN ne part — un succes a vide, la
					//    face n.2 de la grille du corpus, deja payee par la sonde du
					//    navigateur.
					check("13b. la CHARGE `id` n'est pas vide (un blueprint doit s'en servir)",
						  ev.selects > 0 && !ev.anyEmptyPayload,
						  ev.selects > 0 ? "" : "aucun evenement : vacuite");
					check("13c. hasDefaultAction=true est HONORE : le modele a deja bouge",
						  m.active == m.nodes[5].id && m.chosen.Size() == 1, "");
				}

				// ── 14. LE CHEVRON PLIE, ET NE SELECTIONNE PAS ──────────────────
				// Lecon de NK3DModeler, gardee en parametre (`chevron_only_fold`) :
				// « le clic de ligne pliait aussi, trop sensible et genant pour
				// renommer » (Rihen).
				{
					NkTreeViewModel m;
					FillDemo(m);
					Events ev;
					NkTreeViewHooks h = MakeHooks(&ev);
					NkComponentInput click;
					click.mouseX = ChevronX(decl, 0);
					click.mouseY = RowCenterY(decl, 0); // « Environnement », qui a des enfants
					click.mousePressed = true;
					NkRecordingPaint r;
					RenderInto(r, m, nullptr, click, &h);
					snprintf(buf, sizeof(buf), "onExpand=%d (open=%d) onSelect=%d", ev.expands,
							 ev.lastOpen ? 1 : 0, ev.selects);
					check("14. le CHEVRON plie, et le clic qui plie NE selectionne PAS",
						  ev.expands == 1 && !ev.lastOpen && ev.selects == 0, buf);
					check("14b. et le pliage a bien pris dans le modele", !m.IsOpen(m.nodes[0].id, true),
						  "");
				}

				// ── 15. CTRL AJOUTE, PUIS RETIRE ────────────────────────────────
				{
					NkTreeViewModel m;
					FillDemo(m);
					Events ev;
					NkTreeViewHooks h = MakeHooks(&ev);
					NkComponentInput click;
					click.mouseX = LabelX(decl, 0);
					click.mousePressed = true;
					NkRecordingPaint r;
					click.mouseY = RowCenterY(decl, 0);
					RenderInto(r, m, nullptr, click, &h);
					click.ctrl = true;
					click.mouseY = RowCenterY(decl, 5);
					RenderInto(r, m, nullptr, click, &h);
					const uint32 afterAdd = (uint32)m.chosen.Size();
					RenderInto(r, m, nullptr, click, &h); // meme Ctrl+clic : bascule inverse
					snprintf(buf, sizeof(buf), "1 -> %u -> %u", afterAdd, (uint32)m.chosen.Size());
					check("15. SELECTION MULTIPLE : Ctrl+clic ajoute, puis le meme retire",
						  afterAdd == 2 && m.chosen.Size() == 1, buf);
				}

				// ── 16. MAJ+CLIC SELECTIONNE LA PLAGE AFFICHEE ──────────────────
				// Venue de NK3DModeler, absente des deux copies Nogee. La plage suit
				// l'ordre AFFICHE : c'est pourquoi le parcours n'est ecrit qu'une
				// fois dans le dessin.
				{
					NkTreeViewModel m;
					FillDemo(m);
					Events ev;
					NkTreeViewHooks h = MakeHooks(&ev);
					NkComponentInput click;
					click.mouseX = LabelX(decl, 0);
					click.mousePressed = true;
					NkRecordingPaint r;
					click.mouseY = RowCenterY(decl, 0); // ancre sur « Environnement »
					RenderInto(r, m, nullptr, click, &h);
					click.shift = true;
					// ⚠️ L'ABSCISSE SUIT LA PROFONDEUR DE LA LIGNE VISEE, et ce n'est
					//    pas un detail : « Lumiere directionnelle » est a PROFONDEUR 2,
					//    donc toute sa ligne est decalee de deux crans. Viser le
					//    `LabelX` de la profondeur 0 tombait sur sa colonne OEIL, et le
					//    composant refusait la selection — a raison, un clic sur l'oeil
					//    bascule la visibilite. Le premier ecrit de cet essai accusait
					//    donc le composant d'un defaut qui etait celui du banc.
					click.mouseX = LabelX(decl, 2);
					click.mouseY = RowCenterY(decl, 3); // jusqu'a « Lumiere directionnelle »
					RenderInto(r, m, nullptr, click, &h);
					snprintf(buf, sizeof(buf), "%u selectionnes (4 attendus : rangs 0 a 3)",
							 (uint32)m.chosen.Size());
					check("16. SELECTION DE PLAGE : Maj+clic prend les rangs affiches, bornes comprises",
						  m.chosen.Size() == 4, buf);
				}

				// ── 17. LE CYCLE EST REFUSE — ET LE RESTE EST ACCEPTE ───────────
				// Garde generique, gratuite parce que le composant a la chaine de
				// parents sous les yeux. Nogee a du l'ecrire a la main (`SetParent`
				// n'en a aucune) ; NK3DModeler ne l'a pas.
				{
					NkTreeViewModel m;
					FillDemo(m);
					Events ev;
					NkTreeViewHooks h = MakeHooks(&ev);
					NkComponentInput press;
					press.mouseX = LabelX(decl, 0);
					press.mouseY = RowCenterY(decl, 0); // saisir « Environnement »
					press.mousePressed = true;
					NkRecordingPaint r;
					RenderInto(r, m, nullptr, press, &h);

					NkComponentInput drop;
					drop.mouseX = LabelX(decl, 2);
					drop.mouseY = RowCenterY(decl, 3); // lacher sur son PETIT-ENFANT
					drop.dragType = "node";
					drop.dragReleased = true;
					const NkTreeViewResult bad = RenderInto(r, m, nullptr, drop, &h);
					snprintf(buf, sizeof(buf), "refuse=%d, onDrop=%d (0 attendu)",
							 bad.dropRefusedCycle ? 1 : 0, ev.drops);
					check("17. GARDE ANTI-CYCLE : lacher un noeud dans sa propre descendance est REFUSE",
						  bad.dropRefusedCycle && ev.drops == 0, buf);

					// CONTRE-EPREUVE, et elle est indispensable : sans elle, un
					// composant qui refuserait TOUT passerait l'essai ci-dessus.
					NkTreeViewModel m2;
					FillDemo(m2);
					Events ev2;
					NkTreeViewHooks h2 = MakeHooks(&ev2);
					NkComponentInput press2;
					press2.mouseX = LabelX(decl, 0);
					press2.mouseY = RowCenterY(decl, 5); // saisir « PersonnagePrincipal »
					press2.mousePressed = true;
					RenderInto(r, m2, nullptr, press2, &h2);
					const NkTreeViewResult good = RenderInto(r, m2, nullptr, drop, &h2);
					snprintf(buf, sizeof(buf), "accepte=%d onDrop=%d position=%u",
							 good.dropAccepted ? 1 : 0, ev2.drops, (uint32)ev2.lastDropPos);
					check("17b. CONTRE-EPREUVE : un depot hors de la descendance est ACCEPTE",
						  good.dropAccepted && ev2.drops == 1 &&
							  ev2.lastDropPos == (uint8)NkTreeDropPos::Into,
						  buf);
				}

				// ── 18. LES TROIS POSITIONS DE DEPOT ────────────────────────────
				// Aucune des trois copies ne sait reordonner. Le tiers haut d'une
				// ligne doit donner `before`, pas `into`.
				{
					NkTreeViewModel m;
					FillDemo(m);
					Events ev;
					NkTreeViewHooks h = MakeHooks(&ev);
					NkComponentInput press;
					press.mouseX = LabelX(decl, 0);
					press.mouseY = RowCenterY(decl, 5);
					press.mousePressed = true;
					NkRecordingPaint r;
					RenderInto(r, m, nullptr, press, &h);
					NkComponentInput drop;
					drop.mouseX = LabelX(decl, 0);
					// Tiers HAUT de la ligne 0 : y = haut + 10 % de la hauteur de ligne.
					drop.mouseY = decl.Metric("header_h") + decl.Metric("search_h") +
								  decl.Metric("row_h") * 0.1f;
					drop.dragType = "node";
					drop.dragReleased = true;
					RenderInto(r, m, nullptr, drop, &h);
					snprintf(buf, sizeof(buf), "position=%u (%u attendu = before)",
							 (uint32)ev.lastDropPos, (uint32)NkTreeDropPos::Before);
					check("18. le TIERS HAUT d'une ligne donne `before` (reordonner), pas `into`",
						  ev.drops == 1 && ev.lastDropPos == (uint8)NkTreeDropPos::Before, buf);
				}

				// ── 19. RENOMMAGE EN PLACE ──────────────────────────────────────
				{
					NkTreeViewModel m;
					FillDemo(m);
					Events ev;
					NkTreeViewHooks h = MakeHooks(&ev);
					NkComponentInput click;
					click.mouseX = LabelX(decl, 0);
					click.mouseY = RowCenterY(decl, 5);
					click.mousePressed = true;
					NkRecordingPaint r;
					RenderInto(r, m, nullptr, click, &h); // selectionne
					NkComponentInput dbl = click;
					dbl.mousePressed = false;
					dbl.doubleClick = true;
					RenderInto(r, m, nullptr, dbl, &h); // double-clic -> saisie armee
					const bool started = (m.renaming == m.nodes[5].id);
					// L'HOTE ecrit dans le tampon (le composant n'a pas le clavier) et
					// leve le drapeau de validation.
					Events::Copy(m.renameBuf, (uint32)sizeof(m.renameBuf), "Heros");
					m.renameCommit = true;
					RenderInto(r, m, nullptr, idle, &h);
					snprintf(buf, sizeof(buf), "debut=%d onRename=%d « %s » -> « %s »", started ? 1 : 0,
							 ev.renames, ev.lastOld, ev.lastNew);
					check("19. RENOMMAGE : le double-clic arme la saisie, la validation de l'hote emet",
						  started && ev.renames == 1, buf);
					check("19b. hasDefaultAction=false est HONORE : le composant n'a PAS renomme le "
						  "noeud",
						  StrEqLocal(m.nodes[5].label.Data(), "PersonnagePrincipal"), "");

					// CONTRE-EPREUVE : valider sans avoir change le texte n'emet rien.
					// Sans elle, un composant qui emettrait a chaque image passerait.
					RenderInto(r, m, nullptr, dbl, &h);
					Events::Copy(m.renameBuf, (uint32)sizeof(m.renameBuf), "PersonnagePrincipal");
					m.renameCommit = true;
					const int32 before = ev.renames;
					RenderInto(r, m, nullptr, idle, &h);
					snprintf(buf, sizeof(buf), "onRename %d -> %d (inchange attendu)", before,
							 ev.renames);
					check("19c. CONTRE-EPREUVE : valider un nom IDENTIQUE n'emet rien",
						  ev.renames == before, buf);
				}

				// ── 20. CONTROLE NEGATIF DES EVENEMENTS ─────────────────────────
				{
					NkTreeViewModel m;
					FillDemo(m);
					Events ev;
					NkTreeViewHooks h = MakeHooks(&ev);
					NkRecordingPaint r;
					RenderInto(r, m, nullptr, idle, &h);
					snprintf(buf, sizeof(buf),
							 "select=%d activate=%d menu=%d expand=%d rename=%d drop=%d flag=%d",
							 ev.selects, ev.activates, ev.menus, ev.expands, ev.renames, ev.drops,
							 ev.flags);
					check("20. CONTROLE NEGATIF : sans entree, AUCUN des sept evenements ne part",
						  ev.selects == 0 && ev.activates == 0 && ev.menus == 0 && ev.expands == 0 &&
							  ev.renames == 0 && ev.drops == 0 && ev.flags == 0,
						  buf);
				}

				// ── 21. DECOUPE EQUILIBREE ──────────────────────────────────────
				snprintf(buf, sizeof(buf), "profondeur max %u", a1.MaxClipDepth());
				check("21. la pile de decoupe est equilibree (PushClip == PopClip)", a1.ClipBalanced(),
					  buf);

				// ── 22. LA PRECONDITION D'ORDRE SE VERIFIE, ET DISCRIMINE ───────
				{
					NkTreeViewModel ok;
					FillDemo(ok);
					// Le meme arbre en ordre LARGEUR : « le parent d'abord » est
					// respecte, l'ordre prefixe ne l'est pas. Un verificateur qui ne
					// dirait que « oui » ne servirait a rien — celui-ci doit refuser.
					NkTreeViewModel bfs;
					FillDemo(bfs);
					NkTreeNode tmp = bfs.nodes[1];
					bfs.nodes[1] = bfs.nodes[2]; // « Eclairage » avant « Sol »
					bfs.nodes[2] = tmp;
					bfs.nodes[3].parent = 1; // ses enfants suivent son nouvel indice
					bfs.nodes[4].parent = 1;
					bfs.nodes[2].parent = 0;
					// « Sol » (indice 2) a pour parent 0, alors que la branche
					// courante est [0,1,...] : ce n'est plus un ordre prefixe.
					snprintf(buf, sizeof(buf), "prefixe=%d, largeur=%d (1 puis 0 attendus)",
							 ok.IsWellFormed() ? 1 : 0, bfs.IsWellFormed() ? 1 : 0);
					check("22. la precondition d'ordre se VERIFIE, et le verificateur discrimine",
						  ok.IsWellFormed() && !bfs.IsWellFormed(), buf);
				}

				// ── 23. LA CONVERGENCE `.nkgui` EST PRODUITE, PAS AFFIRMEE ──────
				char ctrl[3072];
				const uint32 nWritten = NkWriteControllerBlock(decl, ctrl, sizeof(ctrl));
				snprintf(buf, sizeof(buf), "%u evenements declares, bloc de %u octets", decl.eventCount,
						 nWritten);
				check("23. le bloc `controller` de la spec .nkgui v0.2 s'emet depuis la declaration",
					  nWritten > 0 && decl.eventCount == 7, buf);
				{
					bool typesOk = true;
					for (uint16 i = 0; i < decl.eventCount; ++i)
						for (uint8 k = 0; k < decl.events[i].argCount; ++k)
							if (decl.events[i].args[k].kind == NkArgKind::Void ||
								decl.events[i].args[k].kind >= NkArgKind::Count)
								typesOk = false;
					check("23b. toutes les charges ont un type de la spec (aucun `Void` en argument)",
						  typesOk, "");
				}

				// ── 24. LES LIBELLES D'ENUM SUIVENT L'ENUMERATION C++ ───────────
				// ⚠️ RIEN DANS LE COMPILATEUR NE LE VERIFIE : la charge `position`
				//    est un rang, et la declaration en donne les noms dans un tableau
				//    separe. C'est exactement « le point de verite d'un encodage est
				//    son consommateur » — et le consommateur d'un blueprint sera ce
				//    tableau. Un decalage d'un rang ferait lire `after` la ou le C++
				//    dit `into`, sans une seule erreur de compilation.
				{
					const NkEventDecl *ed = decl.FindEvent("onDrop");
					const NkArgDecl *pos = nullptr;
					if (ed)
						for (uint8 k = 0; k < ed->argCount; ++k)
							if (NkComponentDecl::StrEq(ed->args[k].name, "position"))
								pos = &ed->args[k];
					const bool ok =
						pos && pos->enumCount == (uint8)NkTreeDropPos::Count &&
						NkComponentDecl::StrEq(pos->enumNames[(uint8)NkTreeDropPos::Before], "before") &&
						NkComponentDecl::StrEq(pos->enumNames[(uint8)NkTreeDropPos::Into], "into") &&
						NkComponentDecl::StrEq(pos->enumNames[(uint8)NkTreeDropPos::After], "after");
					snprintf(buf, sizeof(buf), "%u libelles declares pour %u valeurs C++",
							 pos ? (uint32)pos->enumCount : 0u, (uint32)NkTreeDropPos::Count);
					check("24. les LIBELLES d'enum de la charge suivent l'enumeration C++", ok, buf);
				}

				// ── 25. LE REGISTRE ENUMERE ─────────────────────────────────────
				NkComponentRegistry::Register(decl);
				NkComponentRegistry::Register(decl); // idempotence
				snprintf(buf, sizeof(buf), "%u composant(s) enregistre(s)",
						 NkComponentRegistry::Count());
				check("25. le registre enumere, et l'enregistrement est idempotent",
					  NkComponentRegistry::Count() == 1 &&
						  NkComponentRegistry::Find("tree_view") == &decl,
					  buf);

				// ── 26. L'ECHELLE VIENT DE LA SURFACE ───────────────────────────
				// ⚠️ SEQUENTIEL, DONC IL NE DISCRIMINE PAS UNE GLOBALE : on pose 1,
				//    puis 2. Le seul temoin qui trancherait est SIMULTANE — deux
				//    fenetres a DPI differents au meme instant. Il exige un GPU : il
				//    est DIFFERE, et rien ici ne le remplace.
				{
					NkComponentInput hidpi;
					hidpi.surfaceScale = 2.f;
					NkRecordingPaint s2;
					RenderFresh(s2, nullptr, hidpi);
					snprintf(buf, sizeof(buf), "echelle 1.0 -> 2.0 : %u differences",
							 a1.DiffCount(s2));
					check("26. l'echelle de SURFACE traverse jusqu'aux metriques (temoin simultane "
						  "DIFFERE)",
						  a1.DiffCount(s2) > 0, buf);
				}

				// ── 27. LE VERIFICATEUR PARTAGE DE LA FORME ────────────────────
				// `NkCheckComponent` est arrive dans la forme PENDANT l'ecriture de ce
				// composant, avec un catalogue de roles. Le passer n'est pas une
				// formalite : il exige que les quatre evenements du role `tree` soient
				// declares AVEC LA MEME FORME DE CHARGE, sans quoi un blueprint branche
				// dessus recevrait autre chose que ce qu'il attend.
				{
					NkFormIssue issues[16];
					const NkCheckReport rp = NkCheckComponent(decl, issues, 16);
					snprintf(buf, sizeof(buf), "%u erreur(s), %u note(s)", rp.errors, rp.notes);
					check("27. la declaration passe `NkCheckComponent` SANS ERREUR", rp.errors == 0,
						  buf);
					for (uint16 i = 0; i < rp.written; ++i) {
						fputs(issues[i].level == NkIssueLevel::Error ? "         (erreur) "
																	: "         (note)   ",
							  stdout);
						fputs(issues[i].code, stdout);
						fputs(" : ", stdout);
						fputs(issues[i].subject, stdout);
						fputc('\n', stdout);
					}
					// CONTRE-EPREUVE : le verificateur sait-il seulement rougir ? Un role
					// hors catalogue doit produire une erreur. Sans ce controle, un
					// « 0 erreur » ne se distinguerait pas d'un verificateur muet.
					NkComponentDecl fake = decl;
					fake.role = "arbre_qui_nexiste_pas";
					const NkCheckReport bad = NkCheckComponent(fake);
					snprintf(buf, sizeof(buf), "%u erreur(s) sur un role hors catalogue", bad.errors);
					check("27b. CONTRE-EPREUVE : le verificateur SAIT rougir", bad.errors > 0, buf);
				}

				// ── 28. LE DOUBLE-CLIC : DEUX USAGES, UN REGLAGE ──────────────
				// Le role `tree` exige `onActivate`. Aucun arbre de SCENE n'active ;
				// l'arbre de FICHIERS de NKCode, si. Declarer l'evenement sans jamais
				// l'emettre aurait ete ma propre condition d'echec C5.
				{
					NkComponentInstance act(decl);
					act.SetParam("activate_on_double_click", 1.f);
					NkTreeViewModel m;
					FillDemo(m);
					Events ev;
					NkTreeViewHooks h = MakeHooks(&ev);
					NkComponentInput dbl;
					dbl.mouseX = LabelX(decl, 0);
					dbl.mouseY = RowCenterY(decl, 5);
					dbl.doubleClick = true;
					NkRecordingPaint r;
					RenderInto(r, m, &act, dbl, &h);
					snprintf(buf, sizeof(buf), "onActivate=%d, renommage arme=%d (attendu : 1 et 0)",
							 ev.activates, m.renaming != 0 ? 1 : 0);
					check("28. `activate_on_double_click` HONORE : le double-clic active au lieu de "
						  "renommer",
						  ev.activates == 1 && m.renaming == 0, buf);
				}

				// ── 29. LE MENU DU FOND PORTE index = -1 ───────────────────
				// La convention du role, verifiee aux deux bouts : sur une ligne, puis
				// sur le vide. Sans le second, on ne saurait pas si -1 arrive jamais.
				{
					NkTreeViewModel m;
					FillDemo(m);
					Events ev;
					NkTreeViewHooks h = MakeHooks(&ev);
					NkComponentInput rc;
					rc.mouseX = LabelX(decl, 0);
					rc.mouseY = RowCenterY(decl, 2);
					rc.rightPressed = true;
					NkRecordingPaint r;
					RenderInto(r, m, nullptr, rc, &h);
					const int32 onRow = ev.lastMenuIndex;
					// ⚠️ SOUS LES LIGNES, MAIS DANS LE PANNEAU. Le rang 40 tombait a
					//    ~1000 px dans une surface haute de 600 : la souris sortait de
					//    la zone, `inArea` devenait faux, et AUCUN des deux cas ne
					//    s'executait — ni la ligne, ni le vide. L'essai imputait au
					//    composant un menu du fond manquant qu'il emet parfaitement.
					//    Le rang 10 est vide (l'arbre en a 7) et reste dans la surface.
					rc.mouseY = RowCenterY(decl, 10); // sous la derniere ligne, dans la zone
					RenderInto(r, m, nullptr, rc, &h);
					snprintf(buf, sizeof(buf), "sur ligne=%d, sur le fond=%d (2 puis -1 attendus)",
							 onRow, ev.lastMenuIndex);
					check("29. onContextMenu : l'index de la ligne, et -1 sur le fond",
						  ev.menus == 2 && onRow == 2 && ev.lastMenuIndex == -1, buf);
				}

				fputs("\n--- le bloc .nkgui produit, tel quel ---\n", stdout);
				fputs(ctrl, stdout);
				fputs("\n--- le fichier d'ecarts produit, tel quel ---\n", stdout);
				fputs(text.Data() ? text.Data() : "", stdout);

				fprintf(stdout, "\n=== RESULTAT : %d / %d ===\n", pass, total);
				fflush(stdout);
				return (pass == total) ? 0 : 1;
			}

		} // namespace treeprobe
	} // namespace editorkit
} // namespace nkentseu

#ifdef NKTREEVIEW_PROBE_MAIN
// Le pilote du banc. Sous garde de macro pour que ce fichier reste un EN-TETE :
// Jenga compile `src/**.cpp`, jamais les `.h` — ce banc ne peut donc pas se
// glisser par accident dans la bibliotheque.
int main() {
	return nkentseu::editorkit::treeprobe::Run();
}
#endif
