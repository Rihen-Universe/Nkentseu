#pragma once
// -----------------------------------------------------------------------------
// @File    NkModelerHierarchy.h
// @Brief   HIERARCHIE de la scene (panneau de gauche) et MENUS DE SCENE peints
//          par-dessus tout : raccourcis globaux, menu contextuel des lignes de
//          la hierarchie ET du clic droit dans la vue 3D, dialogue de
//          confirmation de suppression.
//
//          Extrait de NkModelerScreens.h pendant la refonte d'interface --
//          « subdiviser les gros fichiers » (Rihen, 13 aout 2026).
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "NK3DModeler/Shell/NkModelerUI.h"
#include "NK3DModeler/Shell/NkModelerInput.h"
#include "NK3DModeler/Shell/NkModelerWidgets.h"
#include "NK3DModeler/Shell/NkModelerTables.h"
#include "NK3DModeler/Shell/NkModelerCommon.h"
#include "NK3DModeler/Viewport/NkDemo3DHost.h"
#include "NKEditorKit/NkShortcutTable.h"

namespace nkentseu {
	namespace nk3d {

		// ── MENUS ET RACCOURCIS DE SCENE (peints PAR-DESSUS tout) ───────────
		// Raccourcis globaux (X/Suppr, Maj+D, Ctrl+C/V), menu contextuel --
		// lignes de la hierarchie ET clic droit dans la vue 3D (Rihen) --,
		// dialogue de confirmation de suppression. Appele en DERNIER par main.
		inline void PaintSceneMenus(NkModelerPainter &p, const NkRect &area, const NkRect &view,
									NkModelerState &st, NkHitRegistry &hit, NkWidgetState &ws,
									const nkgui::NkGuiInput &in) {
			const int32 kNumObj2 = demo::Demo3DHostObjectCount();
			const int32 kFirstLight = kNumObj2;
			const int32 selLight = demo::Demo3DHostSelectedLight();
			const int32 activeObj = demo::Demo3DHostActiveObject();
			char key[40];
			// Operations du NAVIGATEUR, partagees par le menu et les raccourcis.
			auto BrCopyRec = [&](int32 src, int32 par) { NkBrowCopyRecU(st, src, par); };
			auto BrDelRec = [&](int32 root2) { NkBrowDelRec(st, root2); };
			auto BrPaste = [&](int32 dest5) {
				if (st.browClip < 0 || st.browserKind[st.browClip] == 255)
					return;
				if (st.browClipCut)
					for (int32 c4 = dest5; c4 >= 0; c4 = st.browserParent[c4])
						if (c4 == st.browClip)
							return; // pas dans sa propre descendance
				NkBrowRequestTransfer(st, st.browClip, dest5, !st.browClipCut,
									  in.mousePos.x, in.mousePos.y);
				if (st.browClipCut && st.browConfSrc < 0)
					st.browClip = -1; // deplace sans conflit ; sinon le dialogue
			};
			// CLIC DROIT DANS LA VUE 3D : le menu du noeud ACTIF — et, SANS noeud
			// actif, le menu AJOUTER au pointeur (Rihen, 10 aout : le vide de la
			// scene doit proposer copier/coller/dupliquer/supprimer/ajouter —
			// les quatre premiers vivent dans le menu du noeud, l'ajout ici).
			// L'objet nait au curseur 3D, comme depuis la barre.
			if (in.mouseClicked[1] && st.hierMenuNode < 0 &&
				NkHitRegistry::Contains(view, in.mousePos)) {
				const int32 actV = st.activeEmpty >= 0
									   ? st.activeEmpty
									   : (selLight >= 0 ? kFirstLight + selLight : activeObj);
				if (actV >= 0) {
					st.hierMenuNode = actV;
					st.hierMenuX = in.mousePos.x;
					st.hierMenuY = in.mousePos.y;
				} else {
					st.voidMenuOpen = 1;
					st.voidMenuX = in.mousePos.x;
					st.voidMenuY = in.mousePos.y;
				}
			}
			// SUPPRIMER (X ou Suppr : le sous-arbre part avec, regle de Rihen),
			// DUPLIQUER (Maj+D), COPIER / COLLER (Ctrl+C / Ctrl+V). Valables
			// aussi la souris sur la vue 3D -- jamais pendant une saisie.
			if (!ws.editing) {
				bool delK = in.keyInit[(int32)nkgui::NkGuiKey::Delete] ||
							(in.keyInit[(int32)nkgui::NkGuiKey::X] && !in.ctrlDown &&
							 !hit.MouseDown());
				// UN SEUL POINT DE PASSAGE pour Maj+D : la voie POLLEE de l'hote
				// (sk & 1, ci-dessous). L'evenement clavier declenchait EN PLUS,
				// une frame avant le bit polle -- deux duplications par pression
				// (constate par Rihen ; visible depuis que le callback clavier
				// renseigne Shift pour le presse-papiers).
				bool dupK = false;
				for (int32 ci = 0; ci < in.charCount; ++ci) {
					const uint32 cp = in.chars[ci];
					// X pendant un glissement = verrou d'axe du gizmo, pas une
					// suppression : la souris doit etre relachee.
					if ((cp == 'x' || cp == 'X') && !in.ctrlDown && !hit.MouseDown())
						delK = true;
					// (PLUS de repli caractere pour Maj+D : depuis que les
					// modificateurs sont renseignes par le callback clavier, le
					// caractere « D » arrivait UNE FRAME apres l'evenement et
					// dupliquait une seconde fois -- constate par Rihen.)
				}
				// Les raccourcis POLLES par l'hote (seule voie fiable pour les
				// lettres, constatee avec Rihen) s'ajoutent aux evenements.
				const int32 sk = demo::Demo3DHostTakeShortcuts();
				delK = delK || (sk & 8) != 0;
				dupK = dupK || (sk & 1) != 0;
				// AU-DESSUS DU NAVIGATEUR, les raccourcis agissent sur LUI.
				// UN SEUL POINT DE PASSAGE pour Ctrl+C / Ctrl+V aussi : la voie
				// POLLEE (sk & 2 / sk & 4), comme Maj+D ci-dessus. L'evenement
				// (wantPaste, keyInit) declenchait EN PLUS, une frame avant le
				// bit polle -- DEUX collages par pression, depuis la vue comme
				// depuis la hierarchie (constate par Rihen). Copier deux fois
				// etait invisible (meme presse-papiers) ; coller deux fois
				// creait deux noeuds.
				if (NkHitRegistry::Contains(st.browserRect, in.mousePos)) {
					if (delK && st.selectedAsset >= 0)
						BrDelRec(st.selectedAsset);
					else if (dupK && st.selectedAsset >= 0)
						BrCopyRec(st.selectedAsset, st.browserParent[st.selectedAsset]);
					if ((sk & 2) != 0 && st.selectedAsset >= 0) {
						st.browClip = st.selectedAsset;
						st.browClipCut = false;
					}
					if ((sk & 64) != 0 && st.selectedAsset >= 0) {
						st.browClip = st.selectedAsset;
						st.browClipCut = true;
					}
					if ((sk & 4) != 0)
						BrPaste(st.browserFolder);
				} else {
				const int32 actN = st.activeEmpty >= 0
									   ? st.activeEmpty
									   : (selLight >= 0 ? kFirstLight + selLight : activeObj);
				if (delK && !st.delAskOpen) {
					// CONFIRMATION D'ABORD (Rihen) : on memorise les cibles, le
					// dialogue tranche -- y compris le sort des enfants.
					st.delNodeCount = 0;
					for (int32 n2 = 0; n2 < kNumObj2; ++n2)
						if (demo::Demo3DHostObjectSelected(n2) && st.delNodeCount < 64)
							st.delNodes[st.delNodeCount++] = n2;
					if (selLight >= 0 && st.delNodeCount < 64)
						st.delNodes[st.delNodeCount++] = kFirstLight + selLight;
					if (st.activeEmpty >= 0 && st.delNodeCount < 64)
						st.delNodes[st.delNodeCount++] = st.activeEmpty;
					if (st.delNodeCount > 0) {
						st.delHasKids = false;
						for (int32 di = 0; di < st.delNodeCount; ++di)
							if (NkHierHasLiveKids(st.delNodes[di]))
								st.delHasKids = true;
						st.delAskOpen = true;
					}
				} else if (dupK && actN >= 0) {
					const int32 nn = demo::Demo3DHostDuplicateNode(actN);
					if (nn >= 0) {
						NkHierNameNewNode(st, actN, nn); // « Sol.001 » (Rihen)
						demo::Demo3DHostSelectEmptyNode(nn);
					}
				}
				// Meme point de passage unique que ci-dessus : la voie POLLEE
				// seule, sinon evenement + bit pollent DEUX collages.
				if ((sk & 2) != 0 && actN >= 0) {
					demo::Demo3DHostCopyNode(actN);
					NkHierNodeName(st, actN, st.clipName, sizeof(st.clipName));
				}
				if ((sk & 4) != 0) {
					const int32 nn = demo::Demo3DHostPasteNode();
					if (nn >= 0) {
						NkHierComposeName(st, st.clipName, nn);
						demo::Demo3DHostSelectEmptyNode(nn);
					}
				}
				// Ctrl+P / Maj+P polles : parenter la selection a l'actif,
				// ou tout deparenter -- meme regle que la hierarchie.
				if (sk & (16 | 32)) {
					for (int32 n2 = 0; n2 < 90; ++n2) {
						const bool selN = n2 < kNumObj2
											  ? demo::Demo3DHostObjectSelected(n2)
											  : (selLight == n2 - kFirstLight);
						if (!selN || n2 == actN)
							continue;
						demo::Demo3DHostSetNodeParent(n2,
													  (sk & 16) && actN >= 0 ? actN : -1);
					}
					if ((sk & 32) && actN >= 0 && actN < 90)
						demo::Demo3DHostSetNodeParent(actN, -1);
				}
				}
			}
			// MENU CONTEXTUEL : Dupliquer / Copier / Coller / Supprimer, avec
			// ou sans les enfants (les deux variantes demandees par Rihen).
			if (st.hierMenuNode >= 0) {
				const int32 tnM = st.hierMenuNode;
				const int32 ukM = demo::Demo3DHostUserKind(tnM);
				// Isoler : uniquement ce qui se MODELISE (ni lumiere ni empty).
				const bool isoOk = tnM < 86 || (ukM >= 1 && ukM <= 3) || ukM >= 6;
				const bool promOk = demo::Demo3DHostNodeParent(tnM) >= 0;
				const char *hmIt[12];
				int32 hmAct[12];
				int32 nH = 0;
				hmIt[nH] = "Ajouter un enfant...";
				hmAct[nH++] = 5;
				if (isoOk) {
					hmIt[nH] = "Isoler (editer comme Model)";
					hmAct[nH++] = 6;
				}
				if (promOk) {
					hmIt[nH] = "Promouvoir en parent";
					hmAct[nH++] = 7;
				}
				hmIt[nH] = "Copier proprietes";
				hmAct[nH++] = 8;
				if (st.propClipNode > 0 && st.propClipNode != tnM + 1) {
					hmIt[nH] = "Coller proprietes";
					hmAct[nH++] = 9;
				}
				hmIt[nH] = "Dupliquer  (Maj+D)";
				hmAct[nH++] = 0;
				// LES DEUX VARIANTES (Rihen, 10 aout) : dupliquer/coller en
				// FRERE (comportement historique) ou en ENFANT du noeud clique.
				hmIt[nH] = "Dupliquer comme enfant";
				hmAct[nH++] = 11;
				hmIt[nH] = "Copier  (Ctrl+C)";
				hmAct[nH++] = 1;
				hmIt[nH] = "Coller  (Ctrl+V)";
				hmAct[nH++] = 2;
				hmIt[nH] = "Coller comme enfant";
				hmAct[nH++] = 10;
				hmIt[nH] = "Supprimer...  (X)";
				hmAct[nH++] = 3;
				// LARGEUR = l'entree la plus longue ; vers le HAUT si le bas
				// manquerait (Rihen : tout doit toujours se voir).
				float32 wH = 0.f;
				for (int32 mi = 0; mi < nH; ++mi)
					if (p.TextW(hmIt[mi]) > wH)
						wH = p.TextW(hmIt[mi]);
				NkRect mr{st.hierMenuX, st.hierMenuY, wH + S(28.f),
						  kRowH * (float32)nH};
				if (mr.y + mr.h > area.y + area.h)
					mr.y = st.hierMenuY - mr.h; // vers le HAUT
				if (mr.y < area.y)
					mr.y = area.y;
				st.UiBlockAdd(mr); // les panneaux du dessous ne repondent plus
				p.Outline(mr, NkRole::Border, NkRole::PanelHeader, 3.f);
				int32 mact = -1;
				for (int32 mi = 0; mi < nH; ++mi) {
					const NkRect it{mr.x, mr.y + (float32)mi * kRowH, mr.w, kRowH};
					snprintf(key, sizeof(key), "hier.menu.%d", mi);
					HoverFill(p, it, hit.Add(key, it), 0.f);
					p.TextV(it.x + S(10.f), it.y, kRowH, hmIt[mi]);
					if (hit.Clicked(key))
						mact = hmAct[mi];
				}
				if (mact >= 0) {
					const int32 tn = st.hierMenuNode;
					if (mact == 0) {
						const int32 nn = demo::Demo3DHostDuplicateNode(tn);
						if (nn >= 0) {
							NkHierNameNewNode(st, tn, nn);
							demo::Demo3DHostSelectEmptyNode(nn);
						}
					} else if (mact == 1) {
						demo::Demo3DHostCopyNode(tn);
						NkHierNodeName(st, tn, st.clipName, sizeof(st.clipName));
					} else if (mact == 2) {
						const int32 nn = demo::Demo3DHostPasteNode();
						if (nn >= 0) {
							NkHierComposeName(st, st.clipName, nn);
							demo::Demo3DHostSelectEmptyNode(nn);
						}
					} else if (mact == 3) {
						// CONFIRMATION d'abord -- le dialogue tranche pour les
						// enfants (Rihen).
						st.delNodes[0] = tn;
						st.delNodeCount = 1;
						st.delHasKids = NkHierHasLiveKids(tn);
						st.delAskOpen = true;
					} else if (mact == 5) {
						// AJOUTER UN ENFANT : tout le menu Ajouter ; l'objet
						// clique est le PARENT du nouveau (Rihen).
						// Dans un Model, l'enfant nait FRERE sous la racine.
						st.addParentNode = NkParentTargetAllowed(st, tn);
						st.addAnchor = {st.hierMenuX, st.hierMenuY - S(26.f), 0.f,
										0.f};
						if (!ws.ComboOpen("tb.addmenu"))
							ws.ToggleCombo("tb.addmenu");
					} else if (mact == 6) {
						// ISOLER : un onglet Model edite CE noeud, seul, sans
						// etre gene par le reste de la scene (Rihen).
						const int32 dH = st.TabDoc(st.activeTab);
						if (st.sceneCount < 8 && dH >= 0) {
							// L'ISOLATION est un document TRANSITOIRE : il n'existe
							// que le temps de la vue, et le noeud rentre chez lui
							// quand elle se ferme. Il n'a donc ni carte ni ligne
							// dans le fichier -- ce serait une scene fantome.
							const int32 d7 = st.DocAlloc();
							if (d7 >= 0) {
								st.docTransient[d7] = true;
								NkHierNodeName(st, tn, st.docName[d7], 32);
								st.docIsoNode[d7] = tn + 1;
								st.docIsoHome[d7] = st.docScene[dH];
								st.docScene[d7] = (uint8)st.sceneIdNext++;
								st.docBlank[d7] = true;
								const int32 tb7 = st.sceneCount++;
								st.sceneTabKind[tb7] = 7;
								st.sceneTabAsset[tb7] = 0;
								st.sceneTabDoc[tb7] = d7;
								NkActivateTab(st, tb7);
							}
						}
					} else if (mact == 7) {
						// PROMOUVOIR : il prend la place de son parent ; le
						// parent devient son fils, ses freres ses enfants.
						const int32 pp7 = demo::Demo3DHostNodeParent(tn);
						if (pp7 >= 0) {
							const int32 gp7 = demo::Demo3DHostNodeParent(pp7);
							const int32 nc7 = demo::Demo3DHostNodeCount();
							for (int32 c7 = 0; c7 < nc7; ++c7)
								if (c7 != tn && !NkHierNodeSkip(c7) &&
									demo::Demo3DHostNodeParent(c7) == pp7)
									demo::Demo3DHostSetNodeParent(c7, tn);
							demo::Demo3DHostSetNodeParent(tn, gp7);
							demo::Demo3DHostSetNodeParent(pp7, tn);
						}
					} else if (mact == 8) {
						st.propClipNode = tn + 1; // source des proprietes
					} else if (mact == 9) {
						// COLLER LES PROPRIETES : matiere, dimensions, lumiere --
						// JAMAIS la transform (position/rotation restent siennes).
						const int32 s9 = st.propClipNode - 1;
						float32 t9[3], mt9, rg9;
						if (demo::Demo3DHostMeshMaterial(s9, t9, &mt9, &rg9)) {
							demo::Demo3DHostSetMeshTint(tn, t9);
							demo::Demo3DHostSetMeshMetalRough(tn, mt9, rg9);
						}
						float32 bs9[3];
						demo::Demo3DHostNodeBaseSize(s9, bs9);
						demo::Demo3DHostSetNodeBaseSize(tn, bs9);
						const bool sL9 = demo::Demo3DHostUserKind(s9) == 5 ||
										 (s9 >= 86 && s9 < 90);
						const bool dL9 = demo::Demo3DHostUserKind(tn) == 5 ||
										 (tn >= 86 && tn < 90);
						if (sL9 && dL9) {
							float32 rgE, inE, outE, aw9, ah9;
							bool sh9;
							int32 ty9;
							if (demo::Demo3DHostLightEx(s9, &rgE, &inE, &outE, &aw9,
														&ah9, &sh9, &ty9))
								demo::Demo3DHostSetLightEx(tn, rgE, inE, outE, aw9,
														   ah9, sh9);
							float32 c9[3], it9;
							if (demo::Demo3DHostUserLightParams(s9, c9, &it9))
								demo::Demo3DHostSetUserLightParams(tn, c9, it9);
						}
					} else if (mact == 10) {
						// COLLER COMME ENFANT : le colle nait sous le noeud clique
						// (dans un Model, la cible autorisee est la racine).
						const int32 nn = demo::Demo3DHostPasteNode();
						if (nn >= 0) {
							NkHierComposeName(st, st.clipName, nn);
							const int32 tg = NkParentTargetAllowed(st, tn);
							if (tg >= 0 && tg != nn)
								demo::Demo3DHostSetNodeParent(nn, tg);
							demo::Demo3DHostSelectEmptyNode(nn);
							NkMarkDirty(st);
						}
					} else if (mact == 11) {
						// DUPLIQUER COMME ENFANT : la copie devient fils de
						// l'original au lieu de naitre a cote.
						const int32 nn = demo::Demo3DHostDuplicateNode(tn);
						if (nn >= 0) {
							NkHierNameNewNode(st, tn, nn);
							const int32 tg = NkParentTargetAllowed(st, tn);
							if (tg >= 0 && tg != nn)
								demo::Demo3DHostSetNodeParent(nn, tg);
							demo::Demo3DHostSelectEmptyNode(nn);
							NkMarkDirty(st);
						}
					}
					st.hierMenuNode = -1;
				} else if (hit.AnyClick() && !NkHitRegistry::Contains(mr, hit.Mouse())) {
					st.hierMenuNode = -1;
				}
			}
			// ── MENU DU VIDE (Rihen, 10 aout) : Ajouter / Copier / Coller /
			// Dupliquer / Supprimer — et d'autres viendront. Ouvert par clic
			// droit hors de tout noeud, dans la vue 3D comme dans la
			// hierarchie. Les actions a cible visent le noeud ACTIF s'il en
			// reste un ; sans cible elles s'affichent en sourdine, cliquables
			// pour rien n'est pas un etat (regle du depot).
			if (st.voidMenuOpen) {
				const int32 actV2 = st.activeEmpty >= 0
										? st.activeEmpty
										: (selLight >= 0 ? kFirstLight + selLight : activeObj);
				const bool canPaste2 = st.clipName[0] != 0;
				const char *vmIt[8];
				int32 vmAct[8];
				bool vmOn[8];
				int32 nV = 0;
				// « Ajouter » est un SOUS-MENU (question de Rihen, 10 aout — oui,
				// c'est mieux) : le survol ouvre la cascade categories -> types
				// (PaintAddObjectMenu), accrochee au bord droit de la ligne,
				// comme « Creer > » du navigateur. Le clic n'a rien a faire.
				vmIt[nV] = "Ajouter              >";
				vmAct[nV] = 0;
				vmOn[nV++] = true;
				vmIt[nV] = "Copier  (Ctrl+C)";
				vmAct[nV] = 1;
				vmOn[nV++] = actV2 >= 0;
				vmIt[nV] = "Coller  (Ctrl+V)";
				vmAct[nV] = 2;
				vmOn[nV++] = canPaste2;
				vmIt[nV] = "Dupliquer  (Maj+D)";
				vmAct[nV] = 3;
				vmOn[nV++] = actV2 >= 0;
				vmIt[nV] = "Supprimer...  (X)";
				vmAct[nV] = 4;
				vmOn[nV++] = actV2 >= 0;
				float32 wV = 0.f;
				for (int32 mi = 0; mi < nV; ++mi)
					if (p.TextW(vmIt[mi]) > wV)
						wV = p.TextW(vmIt[mi]);
				NkRect mrV{st.voidMenuX, st.voidMenuY, wV + S(28.f), kRowH * (float32)nV};
				if (mrV.y + mrV.h > area.y + area.h)
					mrV.y = st.voidMenuY - mrV.h;
				if (mrV.y < area.y)
					mrV.y = area.y;
				st.UiBlockAdd(mrV);
				p.Outline(mrV, NkRole::Border, NkRole::PanelHeader, 3.f);
				int32 vact = -1;
				for (int32 mi = 0; mi < nV; ++mi) {
					const NkRect it{mrV.x, mrV.y + (float32)mi * kRowH, mrV.w, kRowH};
					snprintf(key, sizeof(key), "void.menu.%d", mi);
					const bool overV = vmOn[mi] && hit.Add(key, it);
					if (vmOn[mi])
						HoverFill(p, it, overV, 0.f);
					p.TextV(it.x + S(10.f), it.y, kRowH, vmIt[mi],
							vmOn[mi] ? NkRole::Text : NkRole::TextMuted);
					// SOUS-MENU AJOUTER : il SUIT le survol — il s'ouvre sur sa
					// ligne, se ferme des qu'une autre entree est survolee.
					if (overV && vmAct[mi] == 0 && !ws.ComboOpen("tb.addmenu")) {
						st.addParentNode = -1;
						// La cascade se place a (a.x, a.y + a.h + 2) : h=0 et
						// y = ligne - 2 la posent exactement au niveau de la ligne.
						st.addAnchor = {mrV.x + mrV.w - S(2.f), it.y - 2.f, 0.f, 0.f};
						ws.ToggleCombo("tb.addmenu");
					} else if (overV && vmAct[mi] != 0 && ws.ComboOpen("tb.addmenu")) {
						ws.CloseCombo();
					}
					if (vmOn[mi] && hit.Clicked(key))
						vact = vmAct[mi];
				}
				if (vact >= 0 && vact != 0) {
					if (vact == 1) {
						demo::Demo3DHostCopyNode(actV2);
						NkHierNodeName(st, actV2, st.clipName, sizeof(st.clipName));
					} else if (vact == 2) {
						const int32 nn = demo::Demo3DHostPasteNode();
						if (nn >= 0) {
							NkHierComposeName(st, st.clipName, nn);
							demo::Demo3DHostSelectEmptyNode(nn);
							NkMarkDirty(st);
						}
					} else if (vact == 3) {
						const int32 nn = demo::Demo3DHostDuplicateNode(actV2);
						if (nn >= 0) {
							NkHierNameNewNode(st, actV2, nn);
							demo::Demo3DHostSelectEmptyNode(nn);
							NkMarkDirty(st);
						}
					} else if (vact == 4) {
						st.delNodes[0] = actV2;
						st.delNodeCount = 1;
						st.delHasKids = NkHierHasLiveKids(actV2);
						st.delAskOpen = true;
					}
					st.voidMenuOpen = 0;
				} else if (hit.AnyClick() && hit.IsHovered("addm.sub")) {
					// Une CREATION dans la cascade Ajouter ferme tout le menu.
					st.voidMenuOpen = 0;
				} else if (hit.AnyClick() && !NkHitRegistry::Contains(mrV, hit.Mouse()) &&
						   !hit.IsHovered("addm.panel") && !hit.IsHovered("addm.sub")) {
					// Clic ailleurs : fermer — sauf dans la cascade Ajouter, qui
					// fait partie du menu.
					st.voidMenuOpen = 0;
				}
			}
			// DIALOGUE DE CONFIRMATION : aucune suppression directe (Rihen).
			// Si un parent est vise, l'utilisateur choisit le sort des enfants.
			if (st.delAskOpen) {
				hit.Add("hier.delveil", area); // voile : la liste ne repond plus
				const float32 dw = area.w - S(24.f) < S(250.f) ? area.w - S(24.f) : S(250.f);
				const float32 dh = kRowH * (st.delHasKids ? 5.f : 3.f) + S(8.f);
				const NkRect dr{area.x + (area.w - dw) * 0.5f, area.y + area.h * 0.32f, dw, dh};
				p.Outline(dr, NkRole::AccentUi, NkRole::PanelHeader, 4.f);
				char title[96];
				if (st.delNodeCount == 1) {
					char nm[48];
					NkHierNodeName(st, st.delNodes[0], nm, sizeof(nm));
					snprintf(title, sizeof(title), "Supprimer \"%s\" ?", nm);
				} else {
					snprintf(title, sizeof(title), "Supprimer %d elements ?",
							 st.delNodeCount);
				}
				p.TextV(dr.x + S(10.f), dr.y + S(4.f), kRowH, title);
				float32 dy = dr.y + S(4.f) + kRowH;
				if (st.delHasKids) {
					p.TextV(dr.x + S(10.f), dy, kRowH, "Des enfants en dependent :",
							NkRole::TextMuted);
					dy += kRowH;
				}
				int32 choice = -1; // 0 avec enfants, 1 garder, 2 annuler
				{
					const NkRect b0{dr.x + S(8.f), dy + S(2.f), dr.w - S(16.f), kRowH - S(4.f)};
					hit.Add("hier.del.ok", b0);
					p.Fill(b0, NkRole::AccentUi, 3.f);
					p.TextV(b0.x + S(8.f), dy, kRowH,
							st.delHasKids ? "Supprimer avec les enfants" : "Supprimer",
							NkRole::TextOnAccent);
					if (hit.Clicked("hier.del.ok"))
						choice = 0;
					dy += kRowH;
				}
				if (st.delHasKids) {
					const NkRect b1{dr.x + S(8.f), dy + S(2.f), dr.w - S(16.f), kRowH - S(4.f)};
					hit.Add("hier.del.keep", b1);
					p.Outline(b1, NkRole::Border, NkRole::InputBg, 3.f);
					p.TextV(b1.x + S(8.f), dy, kRowH, "Garder les enfants");
					if (hit.Clicked("hier.del.keep"))
						choice = 1;
					dy += kRowH;
				}
				{
					const NkRect b2{dr.x + S(8.f), dy + S(2.f), dr.w - S(16.f), kRowH - S(4.f)};
					hit.Add("hier.del.cancel", b2);
					p.Outline(b2, NkRole::Border, NkRole::InputBg, 3.f);
					p.TextV(b2.x + S(8.f), dy, kRowH, "Annuler  (Echap)");
					if (hit.Clicked("hier.del.cancel") ||
						in.keyInit[(int32)nkgui::NkGuiKey::Escape])
						choice = 2;
				}
				if (choice == 0 || choice == 1) {
					for (int32 di = 0; di < st.delNodeCount; ++di)
						demo::Demo3DHostDeleteNode(st.delNodes[di], choice == 0);
					demo::Demo3DHostDeselectAll();
				}
				if (choice >= 0) {
					st.delAskOpen = false;
					st.delNodeCount = 0;
				}
			}
			// ── MENU CONTEXTUEL DU NAVIGATEUR : PARTOUT (arbre, grille, carte
			// ou vide), et son contenu s'adapte au presse-papiers (Rihen).
			if (st.browMenuIdx != -1) {
				const bool onCard = st.browMenuIdx >= 0;
				// -4 : combo Creer de la barre -> UNIQUEMENT la liste de creation
				// (le clic droit garde Importer + sous-menu Creer).
				const bool creatOnly = (st.browMenuIdx == -4);
				const bool canPaste =
					st.browClip >= 0 && st.browserKind[st.browClip] != 255;
				const char *bmIt[12];
				int32 bmAct[12];
				int32 nIt = 0;
				if (!creatOnly) {
					bmIt[nIt] = "Creer                >";
					bmAct[nIt++] = 100; // ouvre le SOUS-MENU au survol
					bmIt[nIt] = "Importer...";
					bmAct[nIt++] = 20;
				}
				if (onCard) {
					bmIt[nIt] = "Couper";
					bmAct[nIt++] = 0;
					bmIt[nIt] = "Copier";
					bmAct[nIt++] = 1;
				}
				if (canPaste && !creatOnly) {
					bmIt[nIt] = "Coller";
					bmAct[nIt++] = 2;
				}
				if (onCard) {
					bmIt[nIt] = "Dupliquer";
					bmAct[nIt++] = 3;
					bmIt[nIt] = "Supprimer";
					bmAct[nIt++] = 4;
				}
				// LARGEUR = l'entree la plus longue ; s'ouvre vers le HAUT si le
				// bas manquerait : tout doit toujours se voir (Rihen).
				float32 wM2 = 0.f;
				for (int32 mi = 0; mi < nIt; ++mi)
					if (p.TextW(bmIt[mi]) > wM2)
						wM2 = p.TextW(bmIt[mi]);
				NkRect mr2{st.browMenuX, st.browMenuY, creatOnly ? 0.f : wM2 + S(28.f),
						   kRowH * (float32)nIt};
				if (mr2.y + mr2.h > area.y + area.h)
					mr2.y = st.browMenuY - mr2.h; // vers le HAUT
				if (mr2.y < area.y)
					mr2.y = area.y;
				st.UiBlockAdd(mr2);
				if (!creatOnly)
					p.Outline(mr2, NkRole::Border, NkRole::PanelHeader, 3.f);
				int32 act2 = -1;
				float32 creatY = mr2.y;
				for (int32 mi = 0; mi < nIt; ++mi) {
					const NkRect it{mr2.x, mr2.y + (float32)mi * kRowH, mr2.w, kRowH};
					snprintf(key, sizeof(key), "brw.menu.%d", mi);
					const bool overIt = hit.Add(key, it);
					HoverFill(p, it, overIt, 0.f);
					p.TextV(it.x + S(10.f), it.y, kRowH, bmIt[mi]);
					// le survol OUVRE le sous-menu Creer, un autre item le ferme
					if (overIt)
						st.browMenuCreat = (bmAct[mi] == 100);
					if (bmAct[mi] == 100)
						creatY = it.y;
					if (hit.Clicked(key) && bmAct[mi] != 100)
						act2 = bmAct[mi];
				}
				NkRect sub2{0.f, 0.f, 0.f, 0.f};
				NkRect gr3{0.f, 0.f, 0.f, 0.f};
				if (st.browMenuCreat) {
					// SOUS-MENU Creer : tout ce qui peut naitre ici (Rihen), dont
					// la SCENE et le MESH reutilisable.
					// Le GRAPHE remplace le blueprint : c'est un editeur nodal, et
					// il en existe plusieurs natures (Rihen).
					static const char *const kCr[7] = {"Dossier", "Scene", "Model",
													   "Materiau", "Texture",
													   "Graphe             >",
													   "Dataset"};
					float32 wS2 = 0.f;
					for (int32 mi = 0; mi < 7; ++mi)
						if (p.TextW(kCr[mi]) > wS2)
							wS2 = p.TextW(kCr[mi]);
					sub2 = {mr2.x + mr2.w + 2.f, creatY, wS2 + S(28.f), kRowH * 7.f};
					if (sub2.y + sub2.h > area.y + area.h)
						sub2.y = area.y + area.h - sub2.h;
					if (sub2.x + sub2.w > area.x + area.w)
						sub2.x = mr2.x - sub2.w - 2.f;
					st.UiBlockAdd(sub2);
					p.Outline(sub2, NkRole::Border, NkRole::PanelHeader, 3.f);
					float32 grY = sub2.y;
					for (int32 mi = 0; mi < 7; ++mi) {
						const NkRect it{sub2.x, sub2.y + (float32)mi * kRowH, sub2.w, kRowH};
						snprintf(key, sizeof(key), "brw.sub.%d", mi);
						const bool ovG = hit.Add(key, it);
						HoverFill(p, it, ovG, 0.f);
						p.TextV(it.x + S(10.f), it.y, kRowH, kCr[mi]);
						if (mi == 5) {
							grY = it.y;
							if (ovG)
								st.browMenuGraph = true;
						} else if (ovG) {
							st.browMenuGraph = false;
						}
						if (hit.Clicked(key) && mi != 5)
							act2 = 10 + mi;
					}
					if (st.browMenuGraph) {
						// SOUS-MENU GRAPHE : les natures d'editeur nodal (Rihen).
						static const char *const kGr[4] = {
							"Modelisation procedurale", "Texturing procedural",
							"Materiau", "Motion"};
						float32 wG = 0.f;
						for (int32 gi = 0; gi < 4; ++gi)
							if (p.TextW(kGr[gi]) > wG)
								wG = p.TextW(kGr[gi]);
						gr3 = {sub2.x + sub2.w + 2.f, grY, wG + S(28.f), kRowH * 4.f};
						if (gr3.y + gr3.h > area.y + area.h)
							gr3.y = area.y + area.h - gr3.h;
						if (gr3.x + gr3.w > area.x + area.w)
							gr3.x = sub2.x - gr3.w - 2.f;
						st.UiBlockAdd(gr3);
						p.Outline(gr3, NkRole::Border, NkRole::PanelHeader, 3.f);
						for (int32 gi = 0; gi < 4; ++gi) {
							const NkRect it{gr3.x, gr3.y + (float32)gi * kRowH, gr3.w,
											kRowH};
							snprintf(key, sizeof(key), "brw.gr.%d", gi);
							HoverFill(p, it, hit.Add(key, it), 0.f);
							p.TextV(it.x + S(10.f), it.y, kRowH, kGr[gi]);
							if (hit.Clicked(key))
								act2 = 30 + gi;
						}
					}
				}
				if (act2 >= 0) {
					// TOUTE action de ce menu touche l'arbre (creation, copie,
					// deplacement, suppression) : le marquer ICI, en amont, evite
					// d'avoir a y penser branche par branche -- et c'est justement
					// une branche oubliee qui ferait quitter sans rien demander.
					NkMarkTreeDirty(st);
					const int32 tgt = st.browMenuIdx;
					// le dossier VISE (carte-dossier cliquee) sinon le courant
					const int32 destF =
						(onCard && st.browserKind[tgt] == 1) ? tgt : st.browserFolder;
					if (act2 >= 30 && act2 <= 33 &&
						st.browserCount < NkModelerState::kMaxBrowser) {
						// GRAPHE : un asset nodal, avec sa NATURE en sous-type.
						static const char *const kGrN[4] = {"Graphe_Modelisation",
															"Graphe_Texturing",
															"Graphe_Materiau",
															"Graphe_Motion"};
						const int32 kg = st.browserCount++;
						st.browserKind[kg] = 0;
						st.browserSub[kg] = (uint8)(act2 - 30);
						st.browserParent[kg] = destF;
						NkBrowUniqueName(st, 0, destF, kGrN[act2 - 30],
										 st.browserNames[kg], 32);
					} else if (act2 >= 10 && act2 <= 16 &&
							   st.browserCount < NkModelerState::kMaxBrowser) {
						// dossier, scene, mesh, materiau, texture, blueprint, dataset
						static const uint8 kNewK[7] = {1, 5, 6, 2, 3, 0, 4};
						static const char *const kNewN[7] = {"Dossier", "Scene", "Model",
															 "Materiau", "Texture", "BP",
															 "Dataset"};
						const int32 k5 = st.browserCount++;
						const uint8 nk5 = kNewK[act2 - 10];
						st.browserKind[k5] = nk5;
						st.browserParent[k5] = destF;
						st.browserMat[k5] = 0;
						st.browserDoc[k5] = 0;
						st.browserSrcNode[k5] = 0;
						st.browserFile[k5][0] = 0;
						NkBrowUniqueName(st, nk5, destF, kNewN[act2 - 10],
										 st.browserNames[k5], 32);
						// « TOUT CE QUI EST FICHIER EST UN ASSET REEL » (Rihen).
						// Une carte creee ici recoit SA MATIERE tout de suite : sans
						// cela, « + Materiau » ne posait qu'un nom, et les materiaux
						// du projet formaient un monde separe des cartes.
						if (nk5 == 2) {
							const int32 sl = demo::Demo3DHostProjMatCreate();
							if (sl >= 0) {
								st.browserMat[k5] = sl + 1;
								demo::Demo3DHostProjMatSetName(sl, st.browserNames[k5]);
							}
						} else if (nk5 == 5) {
							// Une SCENE creee ici est un vrai document, sinon son
							// double-clic fabriquerait une scene vide sans lien.
							const int32 dN = st.DocAlloc();
							if (dN >= 0) {
								NkWidgetState::Copy(st.docName[dN], st.browserNames[k5], 31u);
								st.docScene[dN] = (uint8)st.sceneIdNext++;
								st.docBlank[dN] = true;
								st.docCard[dN] = k5 + 1;
								st.browserDoc[k5] = dN + 1;
							}
						}
					} else if (act2 == 0) {
						st.browClip = tgt;
						st.browClipCut = true;
					} else if (act2 == 1) {
						st.browClip = tgt;
						st.browClipCut = false;
					} else if (act2 == 2) {
						// dans le dossier CLIQUE, pas la racine (Rihen)
						BrPaste(destF);
					} else if (act2 == 3) {
						BrCopyRec(tgt, st.browserParent[tgt]);
					} else if (act2 == 4) {
						BrDelRec(tgt);
					}
					st.browMenuIdx = -1;
					st.browMenuCreat = false; // sinon le prochain menu l'ouvrirait
					st.browMenuGraph = false;
				} else if (hit.AnyClick() && !NkHitRegistry::Contains(mr2, hit.Mouse()) &&
						   !(st.browMenuCreat && NkHitRegistry::Contains(sub2, hit.Mouse())) &&
						   !(st.browMenuGraph && NkHitRegistry::Contains(gr3, hit.Mouse())) &&
						   !hit.IsHovered("brw.creer")) { // pas le clic d'OUVERTURE
					st.browMenuIdx = -1;
					st.browMenuCreat = false;
				}
			}
			// ── CARTE DU LACHER D'UN MODEL SUR UN OBJET (specif. de Rodolf) ──
			// « Deposer un model sur un model l'ajoute comme enfant de ce
			// dernier OU comme element independant, selon un choix valide
			// depuis un menu qui va apparaitre. »
			//
			// TROIS issues, et la troisieme n'est pas un defaut : un menu ferme
			// sans choix est un geste ANNULE, pas « enfant par defaut ». Le
			// jeton se detruit alors sans rien faire.
			//
			// ⚠️ TOUT CE QUE CE MENU UTILISE EST DEJA FIGE dans le jeton -- le
			// modele source, le noeud cible, la position. Entre le lacher et le
			// clic ici il s'ecoule du TEMPS UTILISATEUR : la selection du
			// navigateur peut avoir change, la camera bouge, la cible etre
			// supprimee. Rien n'est relu.
			if (st.dropIdx >= 0 && st.dropMenuTarget >= 0) {
				static const char *const kDrop[3] = {"Ajouter comme enfant",
													 "Ajouter comme element independant",
													 "Annuler"};
				NkRect dr3{st.dropMenuX, st.dropMenuY, S(230.f), kRowH * 3.f};
				if (dr3.y + dr3.h > area.y + area.h)
					dr3.y = area.y + area.h - dr3.h;
				if (dr3.x + dr3.w > area.x + area.w)
					dr3.x = area.x + area.w - dr3.w;
				// ETANCHEITE : ce menu est peint SUR LA VUE 3D, qui lit l'input
				// DIRECTEMENT sans passer par le registre de zones. Sans ce
				// blocage, cliquer « Ajouter comme enfant » selectionnerait AUSSI
				// l'objet situe derriere le menu -- le clic traverserait. C'est
				// exactement le defaut que `UiBlockAdd` a ete ecrit pour clore
				// (badges et listes posees sur la vue).
				st.UiBlockAdd(dr3);
				p.Outline(dr3, NkRole::AccentUi, NkRole::PanelHeader, 3.f);
				int32 dchoix = -1;
				for (int32 mi = 0; mi < 3; ++mi) {
					const NkRect it{dr3.x, dr3.y + (float32)mi * kRowH, dr3.w, kRowH};
					snprintf(key, sizeof(key), "drop.ask.%d", mi);
					HoverFill(p, it, hit.Add(key, it), 0.f);
					p.TextV(it.x + S(10.f), it.y, kRowH, kDrop[mi]);
					if (hit.Clicked(key))
						dchoix = mi;
				}
				if (dchoix == 0 || dchoix == 1) {
					// LA CIBLE PEUT AVOIR DISPARU pendant que le menu etait
					// ouvert -- ce n'est pas theorique avec un menu qui attend un
					// clic. On le CONSTATE et on le DIT, plutot que de parenter a
					// un noeud supprime ou de retomber en silence sur « racine ».
					const bool cibleVivante =
						!demo::Demo3DHostNodeDeleted(st.dropMenuTarget);
					if (dchoix == 0 && !cibleVivante) {
						snprintf(st.hierNote, sizeof(st.hierNote),
								 "L'objet vise a disparu : « %s » n'a pas ete ajoute",
								 st.dropName);
					} else {
						const int32 nn = NkDropSpawnModel(st);
						if (nn >= 0) {
							const float32 rot[3] = {0.f, 0.f, 0.f};
							const float32 scl[3] = {1.f, 1.f, 1.f};
							demo::Demo3DHostSetEmptyTransform(nn, st.dropWorld, rot, scl);
							if (dchoix == 0)
								(void)demo::Demo3DHostSetNodeParent(nn, st.dropMenuTarget);
							demo::Demo3DHostSelectEmptyNode(nn);
						}
					}
				}
				// LE JETON SE CONSOMME UNE FOIS, quel que soit le choix -- y
				// compris « Annuler » et le clic dans le vide. Un jeton qui
				// survivrait a sa validite serait lu par le lacher suivant, et
				// sa reponse perimee aurait l'air d'un resultat.
				if (dchoix >= 0 ||
					(hit.AnyClick() && !NkHitRegistry::Contains(dr3, hit.Mouse()))) {
					st.dropIdx = -1;
					st.dropMenuTarget = -1;
					st.dropKind = 255;
					st.dropSrcNode = 0;
					st.dropMat = 0;
				}
			}
			// CARTE du depot GAUCHE -> DROITE : Copier / Deplacer / Annuler ;
			// cliquer dans le vide annule aussi (Rihen).
			if (st.browAskIdx >= 0) {
				static const char *const kAsk[3] = {"Deplacer ici", "Copier ici",
													"Annuler"};
				NkRect ar3{st.browAskX, st.browAskY, S(160.f), kRowH * 3.f};
				if (ar3.y + ar3.h > area.y + area.h)
					ar3.y = area.y + area.h - ar3.h;
				p.Outline(ar3, NkRole::AccentUi, NkRole::PanelHeader, 3.f);
				int32 ask2 = -1;
				for (int32 mi = 0; mi < 3; ++mi) {
					const NkRect it{ar3.x, ar3.y + (float32)mi * kRowH, ar3.w, kRowH};
					snprintf(key, sizeof(key), "brw.ask.%d", mi);
					HoverFill(p, it, hit.Add(key, it), 0.f);
					p.TextV(it.x + S(10.f), it.y, kRowH, kAsk[mi]);
					if (hit.Clicked(key))
						ask2 = mi;
				}
				if (ask2 == 0) {
					NkBrowRequestTransfer(st, st.browAskIdx, st.browAskDest, false,
										  in.mousePos.x, in.mousePos.y);
				} else if (ask2 == 1) {
					NkBrowRequestTransfer(st, st.browAskIdx, st.browAskDest, true,
										  in.mousePos.x, in.mousePos.y);
				}
				if (ask2 >= 0 ||
					(hit.AnyClick() && !NkHitRegistry::Contains(ar3, hit.Mouse())))
					st.browAskIdx = -1; // le vide ANNULE
			}
			// CONFLIT D'HOMONYME (facon Windows) : Renommer / Remplacer /
			// Arreter -- remplacer deux DOSSIERS homonymes les FUSIONNE
			// recursivement (regle de Rihen).
			if (st.browConfSrc >= 0) {
				NkRect cr3{st.browConfX, st.browConfY, S(220.f), kRowH * 4.f + S(6.f)};
				if (cr3.y + cr3.h > area.y + area.h)
					cr3.y = area.y + area.h - cr3.h;
				if (cr3.x + cr3.w > area.x + area.w)
					cr3.x = area.x + area.w - cr3.w;
				p.Outline(cr3, NkRole::AccentUi, NkRole::PanelHeader, 3.f);
				char t7[64];
				snprintf(t7, sizeof(t7), "\"%s\" existe deja ici",
						 st.browserNames[st.browConfSrc]);
				p.TextV(cr3.x + S(8.f), cr3.y + S(3.f), kRowH, t7);
				static const char *const kCf[3] = {"Renommer", "Remplacer", "Arreter"};
				int32 cAct = -1;
				for (int32 mi = 0; mi < 3; ++mi) {
					const NkRect it{cr3.x, cr3.y + S(3.f) + kRowH * (float32)(mi + 1),
									cr3.w, kRowH};
					snprintf(key, sizeof(key), "brw.conf.%d", mi);
					HoverFill(p, it, hit.Add(key, it), 0.f);
					p.TextV(it.x + S(10.f), it.y, kRowH, kCf[mi]);
					if (hit.Clicked(key))
						cAct = mi;
				}
				if (cAct >= 0) {
					const int32 cs = st.browConfSrc;
					const int32 cd = st.browConfDest;
					if (cAct == 0) {
						// RENOMMER : suffixe unique, puis transfert.
						if (st.browConfCopy) {
							NkBrowCopyRecU(st, cs, cd); // les noms y sont uniques
						} else {
							char nn7[32];
							NkBrowUniqueName(st, st.browserKind[cs], cd,
											st.browserNames[cs], nn7, 32);
							snprintf(st.browserNames[cs], 32, "%s", nn7);
							st.browserParent[cs] = cd;
						}
					} else if (cAct == 1) {
						if (st.browserKind[cs] == 1) {
							// dossier : FUSION (les fichiers homonymes rejoignent
							// la file et repassent ici un par un)
							if (st.browConfCopy)
								NkBrowCopyReplace(st, cs, cd);
							else
								NkBrowMoveReplace(st, cs, cd);
						} else {
							NkBrowReplaceOne(st, cs, cd, st.browConfCopy);
						}
					}
					if (cAct != 2 && !st.browConfCopy && st.browClip == cs)
						st.browClip = -1; // le couper est consomme
					st.browConfSrc = -1;
					// la FILE continue : le prochain conflit reprend le dialogue
					if (st.browConfQN > 0) {
						st.browConfQN--;
						st.browConfSrc = st.browConfQ[st.browConfQN][0];
						st.browConfDest = st.browConfQ[st.browConfQN][1];
						st.browConfCopy =
							((st.browConfQCopy >> st.browConfQN) & 1u) != 0u;
					}
				} else if (hit.AnyClick() && !NkHitRegistry::Contains(cr3, hit.Mouse())) {
					st.browConfSrc = -1; // le vide ARRETE
				}
			}
		}
		inline void PaintHierarchy(NkModelerPainter &p, const NkRect &r, NkModelerState &st,
								   NkHitRegistry &hit, NkWidgetState &ws, const nkgui::NkGuiInput &in,
								   nkgui::NkGuiContext *guiCtx = nullptr) {
			p.Fill(r, NkRole::PanelBg);
			p.VLine(r.x + r.w - 1.f, r.y, r.h);
			float32 y = PaintPanelTab(p, r, "Hierarchie", &hit, &st.showLeft,
									  "hier.close", NkIcon::ChevronLeft);
			// Les editeurs SANS design defini (materiau, texture, blueprint,
			// dataset) n'ont PAS de hierarchie : seuls Scene et Model ont
			// l'interface complete (Rihen).
			{
				const uint8 tkH = st.sceneTabKind[st.activeTab];
				if (tkH != 0 && tkH != 7) {
					p.TextV(r.x + S(12.f), y + S(6.f), kRowH,
							"Indisponible pour cet editeur", NkRole::TextMuted);
					return;
				}
			}
			y = PaintSearch(p, r, y, hit, ws, in, "hier.search", st.searchHier);

			// TROIS COLONNES D'ETAT depuis que le RENDU a la sienne (Rihen) :
			// oeil (visible dans la vue), camera (present dans l'image
			// produite), cadenas (selectionnable). Elles sont distinctes : on
			// travaille souvent avec un repere qui n'a rien a faire dans le
			// rendu final.
			const float32 colEye = r.x + r.w - S(70.f);
			const float32 colCam = r.x + r.w - S(48.f);
			const float32 colLock = r.x + r.w - S(26.f);
			const float32 colType = r.x + r.w - S(144.f);

			// L'EN-TETE annonce TOUTES les colonnes : nom, type, oeil, camera,
			// cadenas -- pour que l'utilisateur sache exactement ce que c'est.
			p.Fill({r.x, y, r.w, kRowH}, NkRole::WindowBg);
			p.TextV(r.x + S(34.f), y, kRowH, "Nom");
			p.TextV(colType, y, kRowH, "Type", NkRole::TextMuted);
			p.IconV(colEye, y, kRowH, NkIcon::Eye, NkRole::TextMuted, 12.f);
			p.IconV(colCam, y, kRowH, NkIcon::Camera, NkRole::TextMuted, 12.f);
			p.IconV(colLock, y, kRowH, NkIcon::Lock, NkRole::TextMuted, 12.f);
			p.HLine(r.x, y + kRowH - 1.f, r.w);
			y += kRowH;

			const float32 listTop = y;
			const float32 listH = r.y + r.h - kRowH - listTop;
			const NkRect listR{r.x, listTop, r.w, listH};
			// LA GOUTTIERE EST RESERVEE AVANT DE PEINDRE : le contenu s'arrete
			// avant elle. Sans cela, les bandeaux de selection couraient jusqu'au
			// bord et passaient SOUS la barre, qui semblait alors decollee et
			// traversee par le dessin (Rihen).
			const NkRect listInner{r.x, listTop, r.w - editorkit::NkScrollbarWidth(), listH};
			hit.Add("hier.list", listR);
			p.Clip(listInner);
			hit.PushClip(listInner); // les lignes defilees hors de vue ne cliquent pas

			char key[40];
			float32 yy = y - st.scrollHier;
			int32 visibleCount = 0;

			// Racine : LA SCENE, renommable -- et elle seule. Dans un editeur de
			// MODEL il n'y a pas de ligne de document : le model EST la racine,
			// et l'afficher en plus donnait deux lignes « Model » de meme nom
			// (constate par Rihen sur sa capture). Une scene, elle, n'est pas un
			// noeud : sa ligne est donc necessaire.
			const int32 dAct = st.TabDoc(st.activeTab);
			if (st.sceneTabKind[st.activeTab] != 7 && dAct >= 0) {
				const NkRect rowR{r.x, yy, r.w, kRowH};
				hit.Add("hier.scene", rowR);
				p.IconV(r.x + S(6.f), yy, kRowH, NkIcon::Globe, NkRole::Text, 13.f);
				if (EditableText(p, hit, ws, in, "hier.scene.name",
								 {r.x + S(24.f), yy, colType - r.x - S(30.f), kRowH},
								 st.docName[dAct], NkRole::Text, st.docName[dAct], 32u)) {
					// Troisieme voie de renommage (avec l'onglet et la carte) : elle
					// doit propager comme les deux autres, sinon le navigateur garde
					// l'ancien nom.
					const int32 e8 = st.docCard[dAct] - 1;
					if (e8 >= 0 && e8 < st.browserCount && st.browserKind[e8] == 5)
						NkWidgetState::Copy(st.browserNames[e8], st.docName[dAct], 31u);
				}
				p.TextV(colType, yy, kRowH, "Scene", NkRole::TextMuted);
				yy += kRowH;
				++visibleCount;
			}

			// ── ARBRE REEL DE PARENTE ───────────────────────────────────────
			// L'arbre suit la TABLE DE PARENTE de l'hote : les anciens groupes
			// sont devenus de vrais EMPTIES (noeuds 90..95) et TOUT noeud peut
			// etre parent ou enfant, quelle que soit sa nature (regle de Rihen).
			// Le CHEVRON plie/deplie -- et RIEN d'autre : le clic de ligne ne
			// fait que selectionner, et un parent se selectionne SEUL (sa
			// transformation emporte ses enfants, pas sa selection). Glisser une
			// ligne sur une autre PARENTE ; vers le vide de la liste, DEPARENTE.
			const int32 kNumObj2 = demo::Demo3DHostObjectCount();
			const int32 kFirstLight = kNumObj2;
			const int32 kFirstEmpty2 = 90;
			const int32 kNNodes = demo::Demo3DHostNodeCount();
			const int32 activeObj = demo::Demo3DHostActiveObject();
			const int32 selLight = demo::Demo3DHostSelectedLight();
			const bool searching = (st.searchHier[0] != 0);
			// Un objet ou une lumiere redevenus actifs (clic vue ou hierarchie)
			// reprennent la main sur l'empty actif.
			// L'EMPTY ACTIF vit dans l'HOTE (gizmo des empties) : une seule
			// source de verite, la vue et la hierarchie restent d'accord.
			st.activeEmpty = demo::Demo3DHostSelectedEmptyNode();
			if (!hit.MouseDown() && st.hierDragNode < 0)
				st.hierDragging = false; // le relachement est digere, une frame apres
			// SOUS UN MENU, ce panneau ne repond plus : les menus sont peints
			// APRES lui, donc son clic etait deja parti (voir UiBlocks).
			const bool uiBlk = st.UiBlocks(hit.Mouse().x, hit.Mouse().y);
			int32 aliveCount = 0, selCount = 0;
			for (int32 n2 = 0; n2 < kFirstEmpty2; ++n2) {
				if (NkHierNodeSkip(n2))
					continue;
				++aliveCount;
				if (n2 < kNumObj2 ? demo::Demo3DHostObjectSelected(n2)
								  : (selLight == n2 - kFirstLight))
					++selCount;
			}
			char nameBuf[48];
			const bool freshPress = hit.MouseDown() && !st.hierMouseWasDown;
			int32 dropHover = -1;
			// ── MAJ+CLIC = PLAGE (Rihen, 10 aout) : tout ce qui s'affiche entre
			// l'ANCRE (dernier clic sans Maj) et la ligne cliquee. La plage ne
			// s'applique qu'APRES le parcours : l'ordre affiche n'est complet
			// qu'a la fin de la boucle.
			int32 visOrder[256];
			uint8 visIsEmpty[256];
			int32 visLight[256];
			int32 visCount = 0;
			int32 rangeTarget = -1;
			// Pile explicite (racines : les empties d'abord -- les familles --
			// puis tout noeud sans parent) ; en RECHERCHE, liste plate.
			int32 stack[200];
			int32 sdepth[200];
			int32 sp = 0;
			if (searching) {
				for (int32 n2 = kNNodes - 1; n2 >= 0; --n2) {
					if (NkHierNodeSkip(n2))
						continue;
					stack[sp] = n2;
					sdepth[sp] = 0;
					++sp;
				}
			} else {
				int32 roots[200];
				int32 nRoots = 0;
				// EST RACINE : sans parent, OU dont le parent n'est pas listable
				// ici (supprime, ou parti dans un autre document par isolation).
				// Sans cette seconde regle l'orphelin DISPARAISSAIT de l'arbre --
				// impossible a selectionner (constate par Rihen).
				auto isRoot2 = [](int32 n3) {
					const int32 pa = demo::Demo3DHostNodeParent(n3);
					return pa < 0 || NkHierNodeSkip(pa);
				};
				for (int32 n2 = kFirstEmpty2; n2 < kNNodes; ++n2)
					if (!NkHierNodeSkip(n2) && isRoot2(n2))
						roots[nRoots++] = n2;
				for (int32 n2 = 0; n2 < kFirstEmpty2; ++n2)
					if (!NkHierNodeSkip(n2) && isRoot2(n2))
						roots[nRoots++] = n2;
				for (int32 i2 = nRoots - 1; i2 >= 0; --i2) {
					stack[sp] = roots[i2];
					sdepth[sp] = 0;
					++sp;
				}
			}
			while (sp > 0) {
				--sp;
				const int32 node = stack[sp];
				const int32 depth = sdepth[sp];
				const bool isLight = node >= kFirstLight && node < kFirstEmpty2;
				const bool isEmpty = node >= kFirstEmpty2;
				const int32 li = isLight ? node - kFirstLight : -1;
				NkHierNodeName(st, node, nameBuf, sizeof(nameBuf));
				// Enfants REELLEMENT listes : un chevron qui ne deplie rien de
				// visible donne l'impression que le pliage est casse (Rihen).
				const bool hasKids = NkHierHasLiveKids(node);
				// Borne EXPLICITE : le tableau couvre 160 noeuds (5 x 32). Un index
				// hors limites ecrirait dans l'etat voisin (bug deja paye).
				const int32 foldW = (node >> 5) < 5 ? (node >> 5) : 4;
				const bool folded = ((st.hierFold[foldW] >> (node & 31)) & 1u) != 0u;
				bool chevHit = false; // clic tombe sur la fleche : pas de selection
				const bool sel = isEmpty
									 ? (demo::Demo3DHostEmptyNodeSelected(node) ||
										st.activeEmpty == node)
								 : isLight ? (selLight == li)
										   : demo::Demo3DHostObjectSelected(node);
				const bool show = !searching || NkNameMatches(nameBuf, st.searchHier);
				if (show) {
					++visibleCount;
					// Ordre AFFICHE, pour la plage Maj+clic (hors clip de
					// defilement : c'est l'ordre qui compte, pas la visibilite).
					if (visCount < 256) {
						visOrder[visCount] = node;
						visIsEmpty[visCount] = isEmpty ? 1 : 0;
						visLight[visCount] = li;
						++visCount;
					}
					const NkRect rowR{r.x, yy, r.w, kRowH};
					if (yy >= listTop - kRowH && yy < listTop + listH) {
						snprintf(key, sizeof(key), "hier.row.%d", node);
						const bool over = hit.Add(key, rowR);
						if (sel)
							p.Fill(rowR, NkRole::AccentUi);
						else
							HoverFill(p, rowR, over, 0.f);
						const NkRole fg = sel ? NkRole::TextOnAccent : NkRole::Text;
						const NkRole dim = sel ? NkRole::TextOnAccent : NkRole::TextMuted;
						const float32 ind = searching ? S(6.f) : S(4.f) + (float32)depth * S(14.f);
						// CHEVRON : la SEULE commande de pliage -- le clic de ligne
						// pliait aussi, trop sensible et genant pour renommer (Rihen).
						if (hasKids && !searching) {
							// zone LARGE : le pliage doit etre aise (Rihen)
							// PLIAGE : la FLECHE seule, teste en GEOMETRIE BRUTE --
							// une zone nommee depend de l'ordre de declaration et du
							// registre ; le pliage, lui, doit toujours repondre.
							const NkRect chevR{r.x + ind - S(4.f), yy, S(24.f), kRowH};
							p.IconV(r.x + ind + S(2.f), yy, kRowH,
									folded ? NkIcon::ChevronRight : NkIcon::ChevronDown, fg, 11.f);
							if (in.mouseClicked[0] && !uiBlk && !st.hierDragging &&
								!ws.dragging &&
								NkHitRegistry::Contains(chevR, hit.Mouse())) {
								st.hierFold[foldW] ^= (1u << (node & 31));
								chevHit = true; // ce clic ne selectionne pas
							}
						}
						const float32 tx = r.x + ind + S(18.f);
						// Un OBJET UTILISATEUR de nature maillage garde l'icone maillage.
						const int32 ukind = node >= 96 ? demo::Demo3DHostUserKind(node) : 0;
						const bool isUserMesh = ukind >= 1 && ukind <= 3;
						// L'ICONE VIENT DE LA NATURE DU NOEUD, decidee en un seul
						// endroit (NkNodeIcon) : model, maillage, lumiere, camera,
						// empty... La hierarchie et le panneau de proprietes montrent
						// ainsi toujours le meme dessin (Rihen).
						p.IconV(tx, yy, kRowH, NkNodeIcon(node), fg, 13.f);
						p.Clip({rowR.x, yy, colType - rowR.x - S(8.f), kRowH});
						snprintf(key, sizeof(key), "hier.name.%d", node);
						EditableText(p, hit, ws, in, key,
									 {tx + S(18.f), yy, colType - tx - S(26.f), kRowH}, nameBuf, fg,
									 st.customNames[node], 24u);
						p.Unclip();
						// le TYPE affiche precise la nature de la lumiere (Rihen)
						static const char *const kLTt[4] = {"Soleil", "Point light",
															"Spot", "Area"};
						const char *tyTxt = isEmpty
												? NkUserKindLabel(node >= 96 ? ukind : 4)
												: (isLight ? "Lumiere" : "Maillage");
						if (isLight)
							tyTxt = kLTt[demo::Demo3DHostLightType(li) & 3];
						else if (isEmpty && ukind == 5)
							tyTxt = kLTt[demo::Demo3DHostUserSub(node) & 3];
						// Une CAMERA s'annonce « Camera » : c'est techniquement un
						// empty de sous-type 10, mais l'utilisateur voit une camera
						// dans sa scene, pas un empty (Rihen).
						else if (isEmpty && ukind == 4 &&
								 demo::Demo3DHostUserSub(node) == 10)
							tyTxt = "Camera";
						// MODEL et MESH sont deux natures DISTINCTES : le conteneur
						// s'annonce Model, sa matiere reste des maillages (Rihen).
						else if (demo::Demo3DHostNodeIsModel(node))
							tyTxt = "Model";
						else if (demo::Demo3DHostNodeIsMesh(node))
							tyTxt = "Mesh";
						// Dans un MODEL, lumieres/cameras/empties ne font PAS
						// partie du model : aides purement cosmetiques (Rihen).
						if (st.sceneTabKind[st.activeTab] == 7 &&
							(isLight || (isEmpty && (ukind == 4 || ukind == 5))))
							tyTxt = "Cosmetique";
						p.TextV(colType, yy, kRowH, tyTxt, dim);
						if (!isEmpty && !isLight && sel && node == activeObj)
							p.Fill({colType - S(12.f), yy + kRowH * 0.5f - S(2.f), S(4.f), S(4.f)}, fg);
						// L'OEIL, pour TOUS : cacher un parent cache son sous-arbre
						// (etat propre des enfants conserve, restaure au retour).
						{
							const bool hidden = isLight ? demo::Demo3DHostLightHidden(li)
														: demo::Demo3DHostObjectHidden(node);
							snprintf(key, sizeof(key), "hier.eye.%d", node);
							const NkRect eyeR{colEye - S(3.f), yy, S(20.f), kRowH};
							HoverFill(p, eyeR, hit.Add(key, eyeR) && !sel, 2.f);
							p.IconV(colEye, yy, kRowH, hidden ? NkIcon::EyeClosed : NkIcon::Eye,
									hidden ? dim : fg, 12.f);
							if (!uiBlk && hit.Clicked(key)) {
								if (isLight)
									demo::Demo3DHostSetLightHidden(li, !hidden);
								else
									demo::Demo3DHostSetObjectHidden(node, !hidden);
							}
						}
						// LA CAMERA : present dans l'IMAGE PRODUITE. Distinct de
						// l'oeil -- un repere, une lumiere temoin ou un guide
						// restent visibles pour travailler tout en n'ayant rien a
						// faire dans le rendu final (Rihen). Comme le cadenas,
						// l'icone montre l'etat EFFECTIF : une exclusion heritee
						// d'un parent se lit en teinte attenuee, sinon elle
						// paraitrait inexplicable sur l'enfant.
						{
							const bool nrOwn = demo::Demo3DHostNodeNoRender(node);
							const bool nrEff = demo::Demo3DHostNodeNoRenderEff(node);
							snprintf(key, sizeof(key), "hier.cam.%d", node);
							const NkRect camR{colCam - S(3.f), yy, S(20.f), kRowH};
							HoverFill(p, camR, hit.Add(key, camR) && !sel, 2.f);
							p.IconV(colCam, yy, kRowH,
									nrEff ? NkIcon::CameraOff : NkIcon::Camera,
									nrOwn ? fg : (nrEff ? NkRole::AccentUi : dim), 12.f);
							if (!uiBlk && hit.Clicked(key))
								demo::Demo3DHostSetNodeNoRender(node, !nrOwn);
						}
						// LE CADENAS, pour TOUS : verrouille = INselectionnable, et
						// cadenasser un parent verrouille son sous-arbre (chaque
						// enfant garde son propre drapeau).
						//
						// L'ICONE MONTRE L'ETAT EFFECTIF, pas le drapeau propre : un
						// enfant dont le parent est cadenasse refuse la selection, et
						// afficher son cadenas OUVERT rendait ce refus incomprehensible
						// (Rihen : « je ne peux selectionner ni le parent ni l'enfant »).
						// Le cadenas HERITE se dessine en teinte attenuee : on voit
						// qu'il vient d'un ancetre et qu'il ne s'ouvre pas ici.
						bool lok = false;
						bool lokEff = false;
						{
							lok = demo::Demo3DHostObjectLocked(node);
							lokEff = demo::Demo3DHostObjectLockedEff(node);
							snprintf(key, sizeof(key), "hier.lock.%d", node);
							const NkRect lockR{colLock - S(3.f), yy, S(20.f), kRowH};
							HoverFill(p, lockR, hit.Add(key, lockR) && !sel, 2.f);
							p.IconV(colLock, yy, kRowH,
									lokEff ? NkIcon::Lock : NkIcon::Unlock,
									lok ? fg : (lokEff ? NkRole::AccentUi : dim), 12.f);
							if (!uiBlk && hit.Clicked(key)) {
								// Le clic n'agit que sur SON drapeau : un verrou herite
								// se libere sur l'ancetre qui le porte.
								if (lokEff && !lok)
									NkHierLockedName(st, node, st.hierNote, sizeof(st.hierNote));
								else
									demo::Demo3DHostSetObjectLocked(node, !lok);
							}
						}
						// SELECTION : la ligne ou le nom. Un parent se selectionne
						// SEUL, un cadenasse JAMAIS ; Maj/Ctrl+clic = multi.
						bool wantSel = false;
						if (!uiBlk && !st.hierDragging && !st.delAskOpen && !chevHit) {
							snprintf(key, sizeof(key), "hier.row.%d", node);
							wantSel = hit.Clicked(key);
							snprintf(key, sizeof(key), "hier.name.%d", node);
							wantSel = wantSel || hit.Clicked(key);
						}
						if (wantSel && lokEff) {
							// REFUS EXPLIQUE : sans message, un clic sans effet passe
							// pour une panne. On nomme le verrou -- le sien ou celui
							// de l'ancetre qui le lui impose (Rihen).
							NkHierLockedName(st, node, st.hierNote, sizeof(st.hierNote));
						}
						if (wantSel) {
							// MAJ = PLAGE depuis l'ancre (appliquee apres le
							// parcours) ; CTRL = bascule un a un ; clic nu =
							// selection seule ET pose l'ancre.
							if (hit.ShiftDown() && st.hierAnchor >= 0 &&
								st.hierAnchor != node) {
								rangeTarget = node;
							} else if (isEmpty && !lokEff) {
								if (hit.CtrlDown()) {
									demo::Demo3DHostToggleEmptyNode(node); // multi successif
								} else {
									demo::Demo3DHostDeselectAll();
									demo::Demo3DHostSelectEmptyNode(node);
								}
								st.activeEmpty = node;
								st.hierAnchor = node;
							} else if (isLight) {
								if (!lokEff) {
									demo::Demo3DHostSelectLight(li);
									st.hierAnchor = node;
								}
								st.activeEmpty = -1;
							} else if (!lokEff) {
								demo::Demo3DHostSelectObject(node, hit.CtrlDown());
								st.activeEmpty = -1;
								st.hierAnchor = node;
							}
						}
						// GLISSER-DEPOSER : armement au premier appui sur la ligne ;
						// la cible est la ligne survolee au lacher.
						const nkgui::NkVec2 hm = hit.Mouse();
						if (freshPress && !uiBlk && NkHitRegistry::Contains(rowR, hm) &&
							hm.x < r.x + r.w - S(14.f)) {
							st.hierDragNode = node;
							st.hierDragX = hm.x;
							st.hierDragY = hm.y;
							st.hierDragging = false;
						}
						if (st.hierDragging && st.hierDragNode != node &&
							NkHitRegistry::Contains(rowR, hm)) {
							dropHover = node;
							p.Fill({rowR.x, rowR.y + rowR.h - S(2.f), rowR.w, S(2.f)},
								   NkRole::AccentUi);
						}
						// MENU CONTEXTUEL au clic droit : TOUTE la largeur de la
						// ligne -- meme au-dessus du nom, de l'oeil, du cadenas
						// ou du chevron (les zones fines volaient le clic).
						if (in.mouseClicked[1] && !uiBlk &&
							NkHitRegistry::Contains(rowR, hm)) {
							st.hierMenuNode = node;
							st.hierMenuX = hm.x;
							st.hierMenuY = hm.y;
						}
					}
					yy += kRowH;
				}
				if (hasKids && !searching && !folded) {
					for (int32 c2 = kNNodes - 1; c2 >= 0; --c2)
						if (!NkHierNodeSkip(c2) && demo::Demo3DHostNodeParent(c2) == node &&
							sp < 196) {
							stack[sp] = c2;
							sdepth[sp] = depth + 1;
							++sp;
						}
				}
			}
			// Lacher du glisser-deposer + fantome sous le curseur.
			if (st.hierDragNode >= 0) {
				const nkgui::NkVec2 dm = hit.Mouse();
				if (hit.MouseDown()) {
					if (!st.hierDragging) {
						const float32 ddx = dm.x - st.hierDragX, ddy = dm.y - st.hierDragY;
						if (ddx * ddx + ddy * ddy > 36.f)
							st.hierDragging = true;
					}
					if (st.hierDragging) {
						NkHierNodeName(st, st.hierDragNode, nameBuf, sizeof(nameBuf));
						p.TextV(dm.x + S(14.f), dm.y - kRowH * 0.5f, kRowH, nameBuf, NkRole::Text);
					}
				} else {
					if (st.hierDragging) {
						if (NkHitRegistry::Contains(st.browserRect, dm)) {
							// DEPOSER dans le NAVIGATEUR : l'objet devient un asset
							// MESH reutilisable, souvenir de sa source (Rihen).
							if (st.browserCount < NkModelerState::kMaxBrowser) {
								const int32 k6 = st.browserCount++;
								st.browserKind[k6] = 6;
								st.browserParent[k6] = st.browserFolder;
								// ARCHIVE hote : l'asset survit a la suppression
								// de l'original dans la scene (retour de Rihen).
								const int32 arc6 =
									demo::Demo3DHostArchiveNode(st.hierDragNode);
								st.browserSrcNode[k6] =
									(arc6 >= 0 ? arc6 : st.hierDragNode) + 1;
								char bnm[32];
								NkHierNodeName(st, st.hierDragNode, bnm, sizeof(bnm));
								NkBrowUniqueName(st, 6, st.browserFolder, bnm,
												 st.browserNames[k6], 32);
							}
						} else if (dropHover >= 0 && dropHover != st.hierDragNode) {
							// Mesh -> Mesh refuse : dans un Model, la seule cible
							// est la racine (les maillages sont FRERES).
							const int32 tg = NkParentTargetAllowed(st, dropHover);
							if (tg >= 0 && tg != st.hierDragNode)
								demo::Demo3DHostSetNodeParent(st.hierDragNode, tg);
						}
						else if (dropHover < 0 && NkHitRegistry::Contains(listR, dm))
							demo::Demo3DHostSetNodeParent(st.hierDragNode, -1);
					}
					// hierDragging reste vrai jusqu'a la frame suivante : le clic
					// de relachement ne doit ni selectionner ni deselectionner.
					st.hierDragNode = -1;
				}
			}
			// ── APPLICATION DE LA PLAGE MAJ+CLIC, l'ordre affiche etant complet.
			// Additive (facon Blender) : elle ETEND la selection sans rien
			// deselectionner. Les lumieres sont sautees : leur selection est
			// UNIQUE (selLight) — chaque ligne volerait l'emplacement a la
			// precedente. Les verrouilles aussi, comme au clic.
			if (rangeTarget >= 0) {
				int32 ia = -1, ib = -1;
				for (int32 v = 0; v < visCount; ++v) {
					if (visOrder[v] == st.hierAnchor)
						ia = v;
					if (visOrder[v] == rangeTarget)
						ib = v;
				}
				if (ia >= 0 && ib >= 0) {
					if (ia > ib) {
						const int32 t = ia;
						ia = ib;
						ib = t;
					}
					for (int32 v = ia; v <= ib; ++v) {
						const int32 n4 = visOrder[v];
						if (demo::Demo3DHostObjectLockedEff(n4))
							continue;
						if (visIsEmpty[v]) {
							if (!demo::Demo3DHostEmptyNodeSelected(n4))
								demo::Demo3DHostToggleEmptyNode(n4);
						} else if (visLight[v] < 0) {
							demo::Demo3DHostSelectObject(n4, true); // additif
						}
					}
					// L'ACTIF suit la ligne cliquee quand c'est un noeud
					// utilisateur — le panneau montre ce qu'on vient de viser.
					if (rangeTarget >= kFirstEmpty2)
						st.activeEmpty = rangeTarget;
				}
			}
			st.hierMouseWasDown = hit.MouseDown();
			// CTRL+P PARENTE la selection a l'ACTIF ; MAJ+P DEPARENTE -- le
			// pendant clavier du glisser-deposer. Jamais pendant une saisie.
			if (!ws.editing) {
				bool wantP = false, wantU = false;
				for (int32 ci = 0; ci < in.charCount; ++ci) {
					const uint32 cp = in.chars[ci];
					if (cp == 'p' || cp == 'P' || cp == 16u) {
						if (in.ctrlDown)
							wantP = true;
						else if (in.shiftDown)
							wantU = true;
					}
				}
				const int32 act = st.activeEmpty >= 0
									  ? st.activeEmpty
									  : (selLight >= 0 ? kFirstLight + selLight : activeObj);
				if (wantP && act < 0)
					wantP = false;
				if (wantP || wantU) {
					for (int32 n2 = 0; n2 < kFirstEmpty2; ++n2) {
						const bool selN = n2 < kNumObj2
											  ? demo::Demo3DHostObjectSelected(n2)
											  : (selLight == n2 - kFirstLight);
						if (!selN || n2 == act)
							continue;
						demo::Demo3DHostSetNodeParent(n2, wantP ? act : -1);
					}
					if (wantU && act >= 0 && act < kFirstEmpty2)
						demo::Demo3DHostSetNodeParent(act, -1);
				}
			}
			hit.PopClip();
			p.Unclip();

			// UN CLIC DANS LE VIDE DESELECTIONNE.
			if (hit.RightClicked("hier.list") && !uiBlk && st.hierMenuNode < 0) {
				// CLIC DROIT DANS LE VIDE : le MENU DU VIDE (Ajouter / Copier /
				// Coller / Dupliquer / Supprimer — Rihen, 10 aout), le meme que
				// celui de la vue 3D. Il est peint par la vue, par-dessus tout.
				st.voidMenuOpen = 1;
				st.voidMenuX = hit.Mouse().x;
				st.voidMenuY = hit.Mouse().y;
			}
			if (hit.Clicked("hier.list") && !st.hierDragging && st.hierMenuNode < 0) {
				demo::Demo3DHostDeselectAll();
				st.activeEmpty = -1;
			}

			// Molette par CONTENANCE : les lignes recouvrent la liste, le survol
			// exact la rendait morte (constate). La barre est COLLEE au bord
			// droit ; seule sa zone de saisie s'arrete avant le splitter.
			hit.WheelIn(listR, st.scrollHier, (float32)visibleCount * kRowH, listH);
			// LA MEME BARRE QUE LES PROPRIETES (Rihen) : celle de NKEditorKit.
			NkPaintVScroll(p, guiCtx, listR, (float32)visibleCount * kRowH, st.scrollHier,
						   0x48494552u);

			const float32 fy = r.y + r.h - kRowH;
			p.Fill({r.x, fy, r.w, kRowH}, NkRole::WindowBg);
			p.HLine(r.x, fy, r.w);
			char foot[72];
			snprintf(foot, sizeof(foot), "%d objet(s), %d selectionne(s)", aliveCount, selCount);
			// Un MESSAGE remplace le decompte quand une action vient d'etre
			// refusee : sans lui, un clic sans effet passe pour une panne (Rihen a
			// perdu une seance sur un objet verrouille par megarde).
			if (st.hierNote[0]) {
				p.Clip({r.x, fy, r.w - S(6.f), kRowH});
				p.TextV(r.x + kPad, fy, kRowH, st.hierNote, NkRole::AccentUi);
				p.Unclip();
				// Il s'efface au clic suivant AILLEURS que sur une ligne refusee.
				if (hit.Clicked("hier.list"))
					st.hierNote[0] = 0;
			} else {
				p.TextV(r.x + kPad, fy, kRowH, foot, NkRole::TextMuted);
			}
		}

		// â”€â”€ GIZMO DE NAVIGATION, FACON BLENDER â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// Six boules reliees au centre, une par DEMI-AXE. Les positives sont PLEINES
		// et portent leur lettre ; les negatives sont CREUSES et muettes.
		//
		// C'est cette dissymetrie qui fait tout le travail : elle dit d'un coup d'oeil
		// de quel cote on regarde. Un simple trepied a trois branches, comme celui que
		// j'avais dessine, ne distingue pas +X de -X -- on ne sait donc jamais si la
		// scene est vue de face ou de dos.
		//
		// Les boules du FOND sont dessinees en PREMIER : sans cet ordre, un demi-axe
		// qui s'eloigne passerait par-dessus celui qui s'approche, et la profondeur

	} // namespace nk3d
} // namespace nkentseu
