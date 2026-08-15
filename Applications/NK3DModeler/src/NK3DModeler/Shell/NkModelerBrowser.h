#pragma once
// -----------------------------------------------------------------------------
// @File    NkModelerBrowser.h
// @Brief   NAVIGATEUR DE PROJET (panneau du bas) : arborescence des dossiers,
//          vignettes des assets, recherche, classement et glisser-deposer.
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


		// â”€â”€ NAVIGATEUR DE PROJET (bas) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// `sortCombo` porte le deroulant de CLASSEMENT. Il est fourni par la boucle
		// principale, comme pour les autres combos : un popup se peint APRES tout le
		// reste, il ne peut donc pas se declarer ici.
		inline void PaintBrowser(NkModelerPainter &p, const NkRect &r, NkModelerState &st,
								 NkHitRegistry &hit, NkWidgetState &ws, const nkgui::NkGuiInput &in,
								 nkgui::NkGuiContext *guiCtx = nullptr,
								 NkComboPending *sortCombo = nullptr) {
			const float32 treeScroll = st.scrollTree;
			const float32 assetScroll = st.scrollAssets;
			p.Fill(r, NkRole::PanelBg);
			p.HLine(r.x, r.y, r.w);

			const float32 topH = 28.f;
			const float32 ih = p.IconSize();
			p.Fill({r.x, r.y, r.w, topH}, NkRole::PanelHeader);
			p.TextV(r.x + kPad, r.y, topH, "Navigateur de projet");
			float32 x = r.x + kPad + p.TextW("Navigateur de projet") + 10.f;
			{
				// Meme croix que les panneaux lateraux, meme effet : elle referme, et la
				// poignee du bas ramene le navigateur.
				const NkRect cr{x - 3.f, r.y + 4.f, 20.f, topH - 8.f};
				const bool over = hit.Add("brw.close", cr);
				if (over)
					p.Fill(cr, NkRole::AccentUi, 2.f);
				// FLECHE de repli vers le bas, pas une croix (meme regle que les
				// panneaux lateraux — Rihen, 11 aout).
				p.IconV(x, r.y, topH, NkIcon::ChevronDown,
						over ? NkRole::TextOnAccent : NkRole::Text, 11.f);
				if (hit.Clicked("brw.close"))
					st.showBrowser = false;
			}
			x += 22.f;
			p.VLine(x, r.y + 6.f, topH - 12.f);
			x += 10.f;
			struct B {
					NkIcon ic;
					const char *label;
			};
			// UN COMBO « Creer » (le meme sous-menu que le clic droit) remplace
			// la rangee de boutons, plus « Importer » (Rihen).
			{
				const float32 bw = 18.f + p.TextW("Creer") + 24.f;
				const NkRect br{x - 4.f, r.y + 3.f, bw, topH - 6.f};
				HoverFill(p, br, hit.Add("brw.creer", br), 2.f);
				p.IconV(x, r.y, topH, NkIcon::Add, NkRole::Text, 13.f);
				p.TextV(x + 18.f, r.y, topH, "Creer");
				p.IconV(x + 18.f + p.TextW("Creer") + 6.f, r.y, topH, NkIcon::ChevronDown,
						NkRole::TextMuted, 11.f);
				if (hit.Clicked("brw.creer")) {
					st.browMenuIdx = -4; // liste Creer SEULE (pas d'Importer)
					st.browMenuCreat = true;
					st.browMenuX = br.x;
					st.browMenuY = br.y + br.h + 2.f;
				}
				x += bw + 8.f;
				const float32 bw2 = 18.f + p.TextW("Importer") + 10.f;
				const NkRect br2{x - 4.f, r.y + 3.f, bw2, topH - 6.f};
				HoverFill(p, br2, hit.Add("brw.imp", br2), 2.f);
				p.IconV(x, r.y, topH, NkIcon::Import, NkRole::Text, 13.f);
				p.TextV(x + 18.f, r.y, topH, "Importer");
				x += bw2 + 8.f;
			}
			p.VLine(x, r.y + 6.f, topH - 12.f);
			x += 10.f;
			// HISTORIQUE : chaque changement de dossier s'enregistre (sauf via
			// les fleches elles-memes).
			if (st.browHistLen == 0) {
				st.browHist[0] = -1;
				st.browHistLen = 1;
				st.browHistPos = 0;
			}
			if (st.browserFolder != st.browPrevFolder) {
				if (!st.browHistNav && st.browHistPos < 63) {
					st.browHistLen = st.browHistPos + 1;
					st.browHist[st.browHistLen++] = st.browserFolder;
					st.browHistPos = st.browHistLen - 1;
				}
				st.browHistNav = false;
				st.browPrevFolder = st.browserFolder;
			}
			const bool canB = st.browHistPos > 0;
			const bool canF = st.browHistPos + 1 < st.browHistLen;
			hit.Add("brw.back", {x - 4.f, r.y + 3.f, 22.f, topH - 6.f});
			p.IconV(x, r.y, topH, NkIcon::ArrowLeft,
					canB ? NkRole::Text : NkRole::TextMuted, 13.f);
			if (canB && hit.Clicked("brw.back")) {
				st.browHistPos--;
				st.browHistNav = true;
				int32 tg9 = st.browHist[st.browHistPos];
				if (tg9 >= 0 && st.browserKind[tg9] == 255)
					tg9 = -1; // dossier disparu entre-temps
				st.browserFolder = tg9;
			}
			hit.Add("brw.fwd", {x + 18.f, r.y + 3.f, 22.f, topH - 6.f});
			p.IconV(x + 22.f, r.y, topH, NkIcon::ArrowRight,
					canF ? NkRole::Text : NkRole::TextMuted, 13.f);
			if (canF && hit.Clicked("brw.fwd")) {
				st.browHistPos++;
				st.browHistNav = true;
				int32 tg9 = st.browHist[st.browHistPos];
				if (tg9 >= 0 && st.browserKind[tg9] == 255)
					tg9 = -1; // dossier disparu entre-temps
				st.browserFolder = tg9;
			}
			// FIL D'ARIANE dynamique et cliquable (chemin REEL du dossier).
			{
				int32 chain[16];
				int32 nCh = 0;
				for (int32 c9 = st.browserFolder; c9 >= 0 && nCh < 16;
					 c9 = st.browserParent[c9])
					chain[nCh++] = c9;
				float32 bx = x + 50.f;
				hit.Add("brw.crumb.root",
						{bx, r.y + 3.f, p.TextW("Contenu") + 6.f, topH - 6.f});
				p.TextV(bx, r.y, topH, "Contenu",
						st.browserFolder < 0 ? NkRole::Text : NkRole::TextMuted);
				if (hit.Clicked("brw.crumb.root"))
					st.browserFolder = -1;
				bx += p.TextW("Contenu") + 8.f;
				for (int32 c9 = nCh - 1; c9 >= 0; --c9) {
					p.TextV(bx, r.y, topH, ">", NkRole::TextMuted);
					bx += p.TextW(">") + 6.f;
					const int32 fi = chain[c9];
					char ck[24];
					snprintf(ck, sizeof(ck), "brw.crumb.%d", fi);
					const float32 wN = p.TextW(st.browserNames[fi]);
					hit.Add(ck, {bx, r.y + 3.f, wN + 6.f, topH - 6.f});
					p.TextV(bx, r.y, topH, st.browserNames[fi],
							fi == st.browserFolder ? NkRole::Text : NkRole::TextMuted);
					if (hit.Clicked(ck))
						st.browserFolder = fi;
					bx += wN + 10.f;
				}
			}
			p.HLine(r.x, r.y + topH - 1.f, r.w);

			// Arbre de dossiers : LES DOSSIERS CREES PAR L'UTILISATEUR, plus une
			// racine fixe. Ils structurent le PROJET et non le disque : ils seront
			// enregistres DANS le fichier de projet.
			st.browserRect = r; // routage des raccourcis (voir PaintSceneMenus)
			// Un menu ou une carte de depot OUVERT bloque tout le navigateur en
			// dessous : les clics ne TRAVERSENT plus (constate par Rihen).
			// ... y compris un menu ouvert AILLEURS (hierarchie, Ajouter) qui
			// deborde sur le navigateur : lui aussi est peint apres ce panneau.
			const bool uiModal = (st.browMenuIdx != -1) || (st.browAskIdx >= 0) ||
								 st.UiBlocks(hit.Mouse().x, hit.Mouse().y);
			const float32 treeW = r.w * 0.18f;
			const float32 ty = r.y + topH;
			const float32 th = r.h - topH;
			p.Clip({r.x, ty, r.w, th});
			p.Fill({r.x, ty, treeW, th}, NkRole::WindowBg);
			hit.Add("brow.tree", {r.x, ty, treeW, th});
			if (hit.RightClicked("brow.tree")) {
				st.browMenuIdx = -2; // fond : Nouveau dossier / Coller
				st.browMenuX = hit.Mouse().x;
				st.browMenuY = hit.Mouse().y;
			}
			p.VLine(r.x + treeW, ty, th);
			// Le FOND de la grille est une zone : cliquer dans le vide
			// DESELECTIONNE (les cartes, declarees apres, gardent leurs clics).
			hit.Add("brow.grid", {r.x + treeW, ty, r.w - treeW, th});
			if (!uiModal && hit.Clicked("brow.grid"))
				st.selectedAsset = -1;
			if (hit.RightClicked("brow.grid")) {
				st.browMenuIdx = -2;
				st.browMenuX = hit.Mouse().x;
				st.browMenuY = hit.Mouse().y;
			}
			int32 dropTo = -999; // -1 racine, i dossier, -100 fond de grille
			int32 chevB2 = -1;   // dossier dont la FLECHE vient d'etre cliquee
			const bool freshB = hit.MouseDown() && !st.browMouseWasDown && !uiModal;
			const nkgui::NkVec2 bm = hit.Mouse();
			int32 folderCount = 0;
			{
				float32 dy = ty + S(4.f) - treeScroll;
				char fkey[40];
				// Racine Â« Contenu Â».
				{
					const NkRect rowR{r.x, dy, treeW, kRowH};
					const bool over = hit.Add("brow.root", rowR);
					const bool on = (st.browserFolder < 0);
					if (on)
						p.Fill(rowR, NkRole::AccentUi);
					else
						HoverFill(p, rowR, over, 0.f);
					p.IconV(r.x + S(6.f), dy, kRowH, NkIcon::Drawer,
							on ? NkRole::TextOnAccent : NkRole::Text, 13.f);
					p.TextV(r.x + S(24.f), dy, kRowH, "Contenu",
							on ? NkRole::TextOnAccent : NkRole::Text);
					if (!uiModal && hit.Clicked("brow.root"))
						st.browserFolder = -1;
					if (st.browDragging && NkHitRegistry::Contains(rowR, bm)) {
						dropTo = -1; // vers la RACINE
						p.Fill({rowR.x, rowR.y + rowR.h - S(2.f), rowR.w, S(2.f)},
							   NkRole::AccentUi);
					}
					dy += kRowH;
				}
				// ARBRE RECURSIF : chaque sous-dossier s'indente sous son parent,
				// comme la hierarchie (regle de Rihen, facon Unreal).
				int32 tstk[64];
				int32 tdep[64];
				int32 tsp = 0;
				for (int32 i = st.browserCount - 1; i >= 0; --i)
					if (st.browserKind[i] == 1 &&
						(st.browserParent[i] < 0 ||
						 st.browserKind[st.browserParent[i]] != 1)) {
						tstk[tsp] = i;
						tdep[tsp] = 0;
						++tsp;
					}
				while (tsp > 0) {
					--tsp;
					const int32 i = tstk[tsp];
					const int32 dep = tdep[tsp];
					++folderCount;
					const float32 ind = (float32)dep * S(14.f);
					const bool on = (st.browserFolder == i);
					const NkRect rowR{r.x, dy, treeW, kRowH};
					snprintf(fkey, sizeof(fkey), "brow.dir.%d", i);
					const bool over = hit.Add(fkey, rowR);
					if (on)
						p.Fill(rowR, NkRole::AccentUi);
					else
						HoverFill(p, rowR, over, 0.f);
					// CHEVRON si le dossier a des SOUS-DOSSIERS ; icone COLOREE si
					// plein, eteinte si vide (regles de Rihen).
					bool hasSub = false, hasAny = false;
					for (int32 c7 = 0; c7 < st.browserCount; ++c7)
						if (st.browserParent[c7] == i && st.browserKind[c7] != 255) {
							hasAny = true;
							if (st.browserKind[c7] == 1)
								hasSub = true;
						}
					const bool foldB = ((st.browFold[i >> 5] >> (i & 31)) & 1u) != 0u;
					if (hasSub) {
						snprintf(fkey, sizeof(fkey), "brow.chev.%d", i);
						// PLIAGE : la FLECHE seule, en geometrie brute (voir la
						// hierarchie -- meme raison).
						const NkRect chevB{r.x + ind - S(4.f), dy, S(24.f), kRowH};
						p.IconV(r.x + S(2.f) + ind, dy, kRowH,
								foldB ? NkIcon::ChevronRight : NkIcon::ChevronDown,
								on ? NkRole::TextOnAccent : NkRole::TextMuted, 11.f);
						if (in.mouseClicked[0] && !uiModal &&
							NkHitRegistry::Contains(chevB, bm)) {
							st.browFold[i >> 5] ^= (1u << (i & 31));
							chevB2 = i; // ce clic n'entre pas dans le dossier
						}
					}
					p.IconV(r.x + S(18.f) + ind, dy, kRowH,
							on ? NkIcon::FolderOpen : NkIcon::Folder,
							hasAny ? NkRole::TypeFolder : NkRole::TextMuted, 13.f);
					snprintf(fkey, sizeof(fkey), "brow.dirname.%d", i);
					EditableText(p, hit, ws, in, fkey,
								 {r.x + S(36.f) + ind, dy, treeW - S(42.f) - ind, kRowH},
								 st.browserNames[i],
								 on ? NkRole::TextOnAccent : NkRole::Text, st.browserNames[i], 32u);
					snprintf(fkey, sizeof(fkey), "brow.dir.%d", i);
					if (!uiModal && hit.Clicked(fkey) && chevB2 != i)
						st.browserFolder = i;
					if (in.mouseClicked[1] && NkHitRegistry::Contains(rowR, bm)) {
						st.browMenuIdx = i;
						st.browMenuX = bm.x;
						st.browMenuY = bm.y;
					}
					// GLISSER-DEPOSER : armement sur la ligne, cible au survol.
					if (freshB && NkHitRegistry::Contains(rowR, bm)) {
						st.browDragIdx = i;
						st.browDragX = bm.x;
						st.browDragY = bm.y;
						st.browDragging = false;
						st.browDragFromTree = true;
					}
					if (st.browDragging && st.browDragIdx != i &&
						NkHitRegistry::Contains(rowR, bm)) {
						dropTo = i;
						p.Fill({rowR.x, rowR.y + rowR.h - S(2.f), rowR.w, S(2.f)},
							   NkRole::AccentUi);
					}
					dy += kRowH;
					if (!foldB)
					for (int32 c6 = st.browserCount - 1; c6 >= 0; --c6)
						if (st.browserKind[c6] == 1 && st.browserParent[c6] == i && tsp < 62) {
							tstk[tsp] = c6;
							tdep[tsp] = dep + 1;
							++tsp;
						}
				}
				if (folderCount == 0)
					p.TextV(r.x + S(8.f), dy, kRowH, "Aucun dossier", NkRole::TextMuted);
			}

			// â”€â”€ CARTES : le contenu du dossier courant â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
			// Plus AUCUNE donnee simulee : chaque carte est un materiau ou une
			// texture cree par l'utilisateur, rattache au dossier ouvert. Le style
			// (ombre, damier, bande de type, pied a deux lignes) est celui valide
			// avec Rihen sur les cartes precedentes.
			const float32 ax = r.x + treeW + S(14.f);
			const float32 tw = 96.f;
			const float32 pvH = 96.f;
			const float32 barH2 = 3.f;
			const float32 footH = 34.f;
			const float32 cardH = pvH + barH2 + footH;
			float32 tx = ax;
			float32 tyy = ty + S(38.f) - assetScroll; // sous le bandeau de recherche
			int32 shown = 0;
			char akey[40];
			const float32 wrapW = r.x + r.w - S(16.f);
			// L'ORDRE D'AFFICHAGE VIENT DU CLASSEMENT, plus de l'ordre de creation
			// (Rihen : « on doit avoir un vrai systeme pour classer et organiser un
			// espace comme l'explorateur de fichiers »). Le filtrage et la recherche
			// vivent avec lui, dans NkBrowVisible : deux endroits qui decident ce qui
			// est visible finiraient par ne plus etre d'accord.
			int32 vis[NkModelerState::kMaxBrowser];
			const int32 visN = NkBrowVisible(st, vis, NkModelerState::kMaxBrowser);
			for (int32 vi = 0; vi < visN; ++vi) {
				const int32 i = vis[vi];
				++shown;
				if (tx + tw > wrapW) { // retour a la ligne
					tx = ax;
					tyy += cardH + S(14.f);
				}
				const uint8 kind = st.browserKind[i];
				// COULEUR ET NOM viennent du point de passage unique (NkModelerUI.h) :
				// la pastille de filtre et le liseret d'onglet lisent la MEME table.
				const NkColor role = NkAssetColor(p, kind, st.browserSub[i]);
				const char *kindName = NkAssetKindName(kind, st.browserSub[i]);

				// Ombre portee legere, comme Unreal.
				p.Fill({tx + 2.f, tyy + 3.f, tw, cardH}, NkColor{0, 0, 0, 90}, 3.f);
				snprintf(akey, sizeof(akey), "brow.card.%d", i);
				const bool selCard = (st.selectedAsset == i);
				hit.Add(akey, {tx, tyy, tw, cardH});
				if (selCard)
					p.Fill({tx - 2.f, tyy - 2.f, tw + 4.f, cardH + 4.f}, NkRole::AccentUi, 3.f);

				// Damier de fond : il dit Â« ce fond est vide Â».
				const float32 c = 8.f;
				p.Fill({tx, tyy, tw, pvH}, NkRole::InputBg);
				for (int32 gx = 0; gx * c < tw; ++gx)
					for (int32 gy = 0; gy * c < pvH; ++gy)
						if (((gx + gy) & 1) == 0) {
							const float32 cw = ((float32)(gx + 1) * c > tw) ? tw - (float32)gx * c : c;
							const float32 ch =
								((float32)(gy + 1) * c > pvH) ? pvH - (float32)gy * c : c;
							p.Fill({tx + (float32)gx * c, tyy + (float32)gy * c, cw, ch},
								   NkRole::WindowBg);
						}
				const float32 cx = tx + tw * 0.5f, cy = tyy + pvH * 0.5f;
				// ── MINIATURES : POINT D'ACCROCHE UNIQUE (Rihen, 8 aout 2026) ──
				// TOUS les fichiers -- procedural ou non -- montreront bientot une
				// MINIATURE plutot qu'un glyphe : soit une vignette, soit le
				// RESULTAT de l'asset.
				//
				// REGLE POSEE PAR RIHEN, a ne pas retrouver a la dure plus tard :
				// la miniature vient de LA VUE, c'est-a-dire de ce que la vue 3D
				// regarde -- PAS d'un noeud camera de la scene. Une camera est un
				// objet que l'utilisateur place pour un rendu ; la vignette d'un
				// asset doit montrer l'asset tel qu'on le voit en le travaillant,
				// et un projet sans camera doit avoir ses vignettes quand meme.
				//
				// Tout le dessin ci-dessous est le REPLI : il n'a lieu que tant
				// qu'aucune miniature n'existe pour cette carte. C'est ici, et
				// nulle part ailleurs, qu'elle viendra s'inserer -- un second
				// endroit qui dessinerait un apercu divergerait au premier
				// changement de cadrage.
				if (kind == 1) {
					// DOSSIER : chemise avec rabat ; PLEINE ou VIDE selon son
					// contenu (previsualisation par contenu, regle de Rihen).
					bool fullF = false;
					for (int32 j3 = 0; j3 < st.browserCount; ++j3)
						if (st.browserKind[j3] != 255 && st.browserParent[j3] == i) {
							fullF = true;
							break;
						}
					p.Fill({cx - 22.f, cy - 18.f, 18.f, 7.f}, role); // rabat
					if (fullF) {
						// des feuilles depassent : le dossier est habite
						p.Fill({cx - 14.f, cy - 14.f, 30.f, 4.f}, NkRole::Text);
						p.Fill({cx - 17.f, cy - 9.f, 36.f, 4.f}, NkRole::TextMuted);
						p.Fill({cx - 22.f, cy - 11.f, 44.f, 28.f}, role);
					} else {
						// vide : le corps n'est qu'un contour
						p.Fill({cx - 22.f, cy - 11.f, 44.f, 28.f}, NkRole::PanelHeader);
						p.OutlineSharp({cx - 22.f, cy - 11.f, 44.f, 28.f}, role);
					}
				} else if (kind == 0) {
					// PROCEDURAL : DEUX NOEUDS RELIES, pas un cube (Rihen). Un
					// fichier procedural ne montre pas un objet, il montre la
					// RECETTE qui le fabrique -- l'ancien cube le faisait passer
					// pour un Model, c'est-a-dire pour son resultat.
					// Les broches sont dessinees : c'est ce qui distingue un
					// graphe de deux rectangles quelconques a cette taille.
					const float32 nw = 20.f, nh = 13.f;
					const float32 ax0 = cx - 23.f, ay0 = cy - 16.f;
					const float32 bx0 = cx + 3.f, by0 = cy + 3.f;
					p.Fill({ax0, ay0, nw, nh}, NkRole::PanelHeader);
					p.Fill({ax0, ay0, nw, 4.f}, role); // bandeau d'en-tete
					p.OutlineSharp({ax0, ay0, nw, nh}, role);
					p.Fill({bx0, by0, nw, nh}, NkRole::PanelHeader);
					p.Fill({bx0, by0, nw, 4.f}, role);
					p.OutlineSharp({bx0, by0, nw, nh}, role);
					// Le FIL : sortie droite du premier -> entree gauche du second.
					const float32 ox = ax0 + nw, oy = ay0 + nh - 4.f;
					const float32 ix = bx0, iy = by0 + 4.f;
					p.Disc(ox, oy, 3.f, role);
					p.Disc(ix, iy, 3.f, role);
					p.Line(ox, oy, ox + 6.f, oy, role);
					p.Line(ox + 6.f, oy, ix - 6.f, iy, role);
					p.Line(ix - 6.f, iy, ix, iy, role);
				} else if (kind == 2) {
					// MATERIAU : la BOULE D'APERCU REELLE (ids 4400+, rendue par
					// l'hote et televersee quand elle perime) — la carte doit
					// refleter le materiau (Rihen, 11 aout). Repli : l'ancien
					// disque si l'emplacement est invalide.
					const int32 mTh = st.browserMat[i] - 1;
					if (mTh >= 0 && mTh < 64) {
						const float32 side2 = (tw < pvH ? tw : pvH) - S(8.f);
						p.Image(4400u + (uint32)mTh,
								{cx - side2 * 0.5f, tyy + (pvH - side2) * 0.5f, side2,
								 side2});
					} else {
						p.Disc(cx, cy, 22.f, role);
						p.Disc(cx - 8.f, cy - 8.f, 5.f, NkRole::Text);
					}
				} else if (kind == 5) {
					// SCENE : la MINIATURE REELLE si elle existe (elle vient de
					// LA VUE, cf. la regle ci-dessus) ; sinon le globe raye --
					// un monde a ouvrir.
					const int32 dTh = st.browserDoc[i] - 1;
					if (dTh >= 0 && dTh < NkModelerState::kMaxDocs &&
						st.docThumb[dTh] == 1 && st.docThumbW[dTh] > 0 &&
						st.docThumbH[dTh] > 0) {
						// AJUSTEE SANS DEFORMER, centree sur le damier : la vue
						// est large, la carte presque carree — deformer rendrait
						// toutes les vignettes menteuses.
						const float32 iw = (float32)st.docThumbW[dTh];
						const float32 ih = (float32)st.docThumbH[dTh];
						const float32 sc =
							((tw / iw) < (pvH / ih)) ? (tw / iw) : (pvH / ih);
						const float32 dw = iw * sc, dh = ih * sc;
						p.Image(4500u + (uint32)dTh, {tx + (tw - dw) * 0.5f,
													  tyy + (pvH - dh) * 0.5f, dw, dh});
					} else {
						p.Disc(cx, cy, 22.f, role);
						p.Fill({cx - 22.f, cy - 2.f, 44.f, 4.f}, NkRole::PanelHeader);
						p.Fill({cx - 2.f, cy - 22.f, 4.f, 44.f}, NkRole::PanelHeader);
					}
				} else if (kind == 6) {
					// MESH reutilisable : cube plein -- une piece prete a cloner.
					p.Fill({cx - 16.f, cy - 14.f, 32.f, 28.f}, role);
					p.Line(cx - 16.f, cy - 14.f, cx - 8.f, cy - 22.f, NkRole::Text);
					p.Line(cx - 8.f, cy - 22.f, cx + 24.f, cy - 22.f, NkRole::Text);
					p.Line(cx + 16.f, cy - 14.f, cx + 24.f, cy - 22.f, NkRole::Text);
					p.Line(cx + 24.f, cy - 22.f, cx + 24.f, cy + 6.f, NkRole::Text);
					p.Line(cx + 16.f, cy + 14.f, cx + 24.f, cy + 6.f, NkRole::Text);
				} else if (kind == 4) {
					// DATASET IA : un document ligne (paires d'entrainement JSONL).
					p.Fill({cx - 16.f, cy - 20.f, 32.f, 40.f}, NkRole::PanelHeader);
					p.OutlineSharp({cx - 16.f, cy - 20.f, 32.f, 40.f}, role);
					for (int32 ln = 0; ln < 4; ++ln)
						p.Fill({cx - 11.f, cy - 13.f + (float32)ln * 8.f,
								(ln == 3) ? 12.f : 22.f, 3.f},
							   role);
				} else {
					const float32 q = 6.f, half = q * 3.f;
					const NkColor damier = p.C(NkRole::PanelHeader);
					for (int32 gx = 0; gx < 6; ++gx)
						for (int32 gy = 0; gy < 6; ++gy)
							p.Fill({cx - half + (float32)gx * q, cy - half + (float32)gy * q, q, q},
								   ((gx + gy) & 1) ? role : damier);
				}

				// Bande de type, puis nom et type DANS la carte, tronques.
				p.Fill({tx, tyy + pvH, tw, barH2}, role);
				p.Fill({tx, tyy + pvH + barH2, tw, footH}, NkRole::PanelHeader);
				{
					const float32 fy = tyy + pvH + barH2;
					const float32 lh = p.LineH();
					const float32 pad = 6.f;
					float32 fyy = fy + (footH - (lh * 2.f + 2.f)) * 0.5f;
					snprintf(akey, sizeof(akey), "brow.cardname.%d", i);
					// Le nom est CLIPPE a la carte : il debordait sur la carte
					// voisine des qu'il etait long (constate par Rihen).
					p.Clip({tx + pad, fyy, tw - pad * 2.f, lh + 2.f});
					if (EditableText(p, hit, ws, in, akey, {tx + pad, fyy, tw - pad * 2.f, lh + 2.f},
									 st.browserNames[i], NkRole::Text, st.browserNames[i], 32u)) {
						// RENOMMER LA CARTE D'UNE SCENE RENOMME SON DOCUMENT (Rihen :
						// « renommer dans le navigateur ne l'a pas fait dans
						// l'onglet »). Uniquement AU MOMENT de la validation : une
						// copie a chaque frame ecraserait, elle, le renommage fait
						// depuis l'onglet -- les deux sens doivent coexister. Le nom
						// va au DOCUMENT, donc l'onglet qui le montre suit tout seul.
						if (st.browserKind[i] == 5) {
							const int32 d9 = st.browserDoc[i] - 1;
							if (d9 >= 0 && d9 < NkModelerState::kMaxDocs && st.docUsed[d9])
								NkWidgetState::Copy(st.docName[d9], st.browserNames[i], 31u);
						}
					}
					p.Unclip();
					fyy += lh + 2.f;
					p.TextClipped(tx + pad, fyy, tw - pad * 2.f, kindName, NkRole::TextMuted);
				}
				snprintf(akey, sizeof(akey), "brow.card.%d", i);
				if (!uiModal && kind == 1 && hit.DoubleClicked(akey))
					st.browserFolder = i; // double-clic : ENTRER dans le dossier
				if (!uiModal && kind != 1 && hit.DoubleClicked(akey)) {
					// OUVRIR l'asset : une SCENE s'ajoute a la barre d'onglets
					// avec son contenu ; les autres natures ouvrent leur EDITEUR
					// specialise dans un onglet dedie (Rihen).
					const uint8 tk9 = (uint8)(kind == 5 ? 0 : 1 + kind);
					int32 tb = -1;
					// UNE SCENE se retrouve par son DOCUMENT : c'est lui qui porte le
					// contenu, et c'est ce lien qui manquait -- le double-clic
					// fabriquait un document neuf, donc une scene VIDE a la place de
					// celle qu'on croyait rouvrir.
					const int32 dCard = (kind == 5) ? (st.browserDoc[i] - 1) : -1;
					for (int32 t9 = 0; t9 < st.sceneCount; ++t9) {
						if (kind == 5) {
							if (st.sceneTabKind[t9] == 0 && st.TabDoc(t9) == dCard && dCard >= 0)
								tb = t9;
						} else if (st.sceneTabAsset[t9] == i + 1 && st.sceneTabKind[t9] == tk9)
							tb = t9; // deja ouvert : on l'ACTIVE simplement
					}
					if (tb < 0 && st.sceneCount < 8) {
						int32 d9 = dCard;
						if (kind == 5) {
							// Carte de scene sans document (fichier d'une version
							// anterieure) : on lui en donne un, vide, plutot que de
							// refuser -- mais VIDE, et il le restera : rien n'a ete
							// enregistre pour elle.
							if (d9 < 0 || d9 >= NkModelerState::kMaxDocs || !st.docUsed[d9]) {
								d9 = st.DocAlloc();
								if (d9 >= 0) {
									NkWidgetState::Copy(st.docName[d9], st.browserNames[i], 31u);
									st.docScene[d9] = (uint8)st.sceneIdNext++;
									st.docBlank[d9] = true;
									st.docCard[d9] = i + 1;
									st.browserDoc[i] = d9 + 1;
								}
							}
						} else {
							// EDITEUR d'asset : sa maquette est TRANSITOIRE, elle
							// meurt avec la vue. L'asset, lui, est la carte.
							d9 = st.DocAlloc();
							if (d9 >= 0) {
								st.docTransient[d9] = true;
								NkWidgetState::Copy(st.docName[d9], st.browserNames[i], 31u);
								st.docScene[d9] = (uint8)st.sceneIdNext++;
								st.docBlank[d9] = true;
							}
						}
						// Table des documents pleine : on n'ouvre RIEN plutot qu'un
						// onglet qui ne montrerait aucune scene.
						if (d9 >= 0) {
							tb = st.sceneCount++;
							st.sceneTabAsset[tb] = i + 1;
							st.sceneTabKind[tb] = tk9;
							st.sceneTabDoc[tb] = d9;
						}
					}
					if (tb >= 0)
						NkActivateTab(st, tb);
				}
				if (in.mouseClicked[1] &&
					NkHitRegistry::Contains({tx, tyy, tw, cardH}, bm)) {
					st.browMenuIdx = i;
					st.browMenuX = hit.Mouse().x;
					st.browMenuY = hit.Mouse().y;
				}
				{
					// GLISSER-DEPOSER : la carte s'arme au premier appui ; une
					// carte-DOSSIER est une cible de depot.
					const NkRect cardR{tx, tyy, tw, cardH};
					if (freshB && NkHitRegistry::Contains(cardR, bm)) {
						st.browDragIdx = i;
						st.browDragX = bm.x;
						st.browDragY = bm.y;
						st.browDragging = false;
						st.browDragFromTree = false;
					}
					if (st.browDragging && st.browDragIdx != i && kind == 1 &&
						NkHitRegistry::Contains(cardR, bm)) {
						dropTo = i;
						p.OutlineSharp(cardR, NkRole::AccentUi);
					}
				}
				if (!uiModal && hit.Clicked(akey))
					st.selectedAsset = i;
				tx += tw + S(12.f);
			}
			if (shown == 0) {
				// Dire POURQUOI c'est vide : un filtre actif n'est pas un dossier
				// vide, et laisser croire l'inverse ferait chercher un bug.
				const bool filtering = st.searchBrowser[0] || st.browFilter != 0u;
				p.TextV(ax, ty + S(40.f), kRowH,
						filtering ? "Aucun element ne correspond a la recherche"
								  : "Vide -- creez un dossier, un materiau ou une texture",
						NkRole::TextMuted);
			}

			// ── RECHERCHE + PASTILLES DE TYPE, sur son bandeau (maquette) ───────
			// Peinte APRES les cartes, donc PAR-DESSUS : au defilement, elles
			// remontaient sur le bandeau et passaient devant (constate par
			// Rihen). Un bandeau fixe doit couvrir ce qui defile, et il gagne du
			// meme coup les clics sur la bande qu'il occupe.
			{
				const float32 fy = ty + S(6.f);
				const float32 fh = S(22.f);
				p.Fill({r.x + treeW + 1.f, ty, r.w - treeW - 1.f, S(34.f)}, NkRole::PanelHeader);
				p.HLine(r.x + treeW + 1.f, ty + S(34.f), r.w - treeW - 1.f);
				// Le champ de la hierarchie : filtrage a la frappe, invite gardee
				// HORS du buffer, croix d'effacement.
				PaintSearch(p, {ax - S(6.f), 0.f, S(192.f), 0.f}, fy - S(4.f), hit, ws, in,
							"brow.search", st.searchBrowser);
				float32 px = ax + S(196.f);
				// Le nom et la couleur viennent du point de passage unique : une
				// pastille qui aurait garde sa propre table finirait par annoncer
				// une couleur que les cartes n'emploient plus.
				static const uint8 kChipKind[5] = {6, 0, 2, 3, 5};
				char ck[32];
				for (int32 ci = 0; ci < 5; ++ci) {
					const uint8 ckd = kChipKind[ci];
					// Le filtre « procedural » couvre les QUATRE graphes : sa
					// pastille porte donc le nom de la famille, pas d'un sous-type.
					const char *cname = (ckd == 0) ? "Procedural" : NkAssetKindName(ckd, 0);
					const NkColor ccol = NkAssetColor(p, ckd, 0);
					const float32 cw = p.TextW(cname) + S(28.f);
					if (px + cw > r.x + r.w - S(10.f))
						break;
					const NkRect cr{px, fy, cw, fh};
					snprintf(ck, sizeof(ck), "brow.chip.%d", ci);
					const bool ov = hit.Add(ck, cr);
					const bool on = (st.browFilter & (1u << ckd)) != 0;
					p.Fill(cr, on ? NkRole::InputBg : NkRole::PanelBg, 11.f);
					if (on)
						p.OutlineSharp(cr, ccol);
					else
						p.OutlineSharp(cr, ov ? NkRole::AccentUi : NkRole::Border);
					// La PUCE porte la couleur de la famille : c'est elle qui les
					// distingue d'un coup d'oeil, comme sur la maquette.
					p.Fill({cr.x + S(9.f), cr.y + fh * 0.5f - S(3.f), S(6.f), S(6.f)}, ccol,
						   3.f);
					p.TextV(cr.x + S(20.f), cr.y, fh, cname,
							on ? NkRole::Text : NkRole::TextMuted);
					if (hit.Clicked(ck))
						st.browFilter ^= (1u << ckd);
					px += cw + S(6.f);
				}
				// ── CLASSEMENT, cale a DROITE ───────────────────────────────
				// Meme place que dans un explorateur : le tri est un reglage de la
				// vue, pas un filtre -- le coller aux pastilles les ferait passer
				// pour une pastille de plus.
				{
					static const char *const kSortN[3] = {"Nom", "Type", "Date"};
					const char *cur = kSortN[(st.browSort >= 0 && st.browSort < 3)
												 ? st.browSort
												 : 0];
					const float32 aw = fh; // bouton de sens, carre
					const float32 sw = p.TextW("Type") + S(34.f);
					const float32 sx = r.x + r.w - S(10.f) - aw - S(4.f) - sw;
					if (sx > px + S(8.f) && sortCombo) {
						p.TextV(sx - p.TextW("Trier") - S(8.f), fy, fh, "Trier",
								NkRole::TextMuted);
						Combo(p, hit, ws, "brow.sort", {sx, fy, sw, fh}, kSortN, nullptr, 3,
							  st.browSort, *sortCombo);
						const NkRect ar2{sx + sw + S(4.f), fy, aw, fh};
						const bool ov2 = hit.Add("brow.sortdir", ar2);
						p.Outline(ar2, NkRole::Border, ov2 ? NkRole::PanelBg : NkRole::InputBg,
								  4.f);
						// Le CHEVRON dit le sens : vers le bas = croissant, comme la
						// fleche d'un en-tete de colonne d'explorateur.
						p.IconV(ar2.x, ar2.y, ar2.h,
								st.browSortDesc ? NkIcon::ChevronUp : NkIcon::ChevronDown,
								NkRole::Text, 11.f);
						if (hit.Clicked("brow.sortdir"))
							st.browSortDesc = !st.browSortDesc;
					}
					(void)cur;
				}
			}

			p.Unclip();
			const NkRect treeArea{r.x, ty, treeW, th};
			// La zone des cartes va JUSQU'AU BORD DROIT du panneau : elle
			// s'arretait 6 px avant, et sa barre paraissait flotter (Rihen).
			const NkRect assetArea{ax - S(10.f), ty + S(34.f), r.x + r.w - (ax - S(10.f)),
								   th - S(34.f)};
			hit.Add("brow.tree", treeArea);
			hit.Wheel("brow.tree", st.scrollTree, 5.f * kRowH + S(8.f), treeArea.h);
			hit.Add("brow.assets", assetArea);
			// La hauteur de contenu etait figee a 125 px, valeur de l'ancienne carte.
			// Les cartes font maintenant 133 px : le defilement s'arretait avant le bas
			// et la derniere rangee restait inaccessible.
			const float32 assetContentH = (tyy + cardH + S(14.f)) - ty + assetScroll;
			hit.Wheel("brow.assets", st.scrollAssets, assetContentH, assetArea.h);
			// LES DEUX COTES DU NAVIGATEUR portent la meme barre que les
			// proprietes (Rihen). La grille commence sous le bandeau de
			// recherche : sa gouttiere aussi, sinon la barre le recouvrirait.
			NkPaintVScroll(p, guiCtx, treeArea, 5.f * kRowH + S(8.f), st.scrollTree, 0x42524F57u);
			NkPaintVScroll(p, guiCtx, assetArea, assetContentH, st.scrollAssets, 0x42415353u);
			// LACHER du glisser-deposer (4 sens, garde anti-cycle) + fantome.
			if (st.browDragIdx >= 0) {
				if (hit.MouseDown()) {
					if (!st.browDragging) {
						const float32 dxb = bm.x - st.browDragX, dyb = bm.y - st.browDragY;
						if (dxb * dxb + dyb * dyb > 36.f)
							st.browDragging = true;
					}
					if (st.browDragging)
						p.TextV(bm.x + S(14.f), bm.y - kRowH * 0.5f, kRowH,
								st.browserNames[st.browDragIdx], NkRole::Text);
				} else {
					if (st.browDragging) {
						// ── LACHER SUR LA VUE 3D : ON FIGE UN JETON, ON NE FAIT RIEN ──
						// Avant, seul un MODEL etait traite, et il atterrissait a
						// l'origine parce que personne ne savait ou le curseur
						// pointait dans la scene -- l'hote n'exposait aucun pick.
						// Desormais chaque nature est traitee, et AUCUNE ne reste
						// muette : un refus silencieux est indistinguable d'un
						// glisser-deposer casse.
						//
						// Rien ne s'applique ICI : la reponse du pick n'existe qu'a
						// la frame suivante. On fige donc TOUT ce dont le geste aura
						// besoin, et main.cpp applique quand la reponse arrive.
						if (!NkHitRegistry::Contains(st.browserRect, bm) &&
							NkHitRegistry::Contains(st.viewRect, bm)) {
							st.dropIdx = st.browDragIdx;
							st.dropKind = st.browserKind[st.browDragIdx];
							st.dropSrcNode = st.browserSrcNode[st.browDragIdx];
							st.dropMat = st.browserMat[st.browDragIdx];
							snprintf(st.dropName, sizeof(st.dropName), "%s",
									 st.browserNames[st.browDragIdx]);
							st.dropMenuTarget = -1; // un jeton neuf n'herite d'aucun menu
							// COORDONNEES FENETRE, telles quelles : l'hote soustrait
							// SON origine de vue. Passer `bm - st.viewRect` ferait de
							// `viewRect` une seconde source pour la meme origine.
							demo::Demo3DHostPickRequest(bm.x, bm.y);
							st.browDragIdx = -1;
						}
						if (st.browDragIdx >= 0 && dropTo == -999 &&
							NkHitRegistry::Contains({r.x + treeW, ty, r.w - treeW, th}, bm))
							dropTo = -100; // fond de grille = dossier COURANT
						if (dropTo != -999) {
							const int32 dest = (dropTo == -100) ? st.browserFolder : dropTo;
							bool ok5 = (dest != st.browDragIdx);
							for (int32 c5 = dest; c5 >= 0 && ok5; c5 = st.browserParent[c5])
								if (c5 == st.browDragIdx)
									ok5 = false;
							if (ok5) {
								const bool destTree =
									NkHitRegistry::Contains({r.x, ty, treeW, th}, bm);
								if (st.browDragFromTree != destTree) {
									// TRAVERSEE gauche <-> droite, dans les DEUX sens :
									// GAUCHE -> DROITE : la carte Copier/Deplacer decide
									// (Rihen) -- cliquer dans le vide annulera.
									st.browAskIdx = st.browDragIdx;
									st.browAskDest = dest;
									st.browAskX = bm.x;
									st.browAskY = bm.y;
								} else {
									NkBrowRequestTransfer(st, st.browDragIdx, dest, false,
														  bm.x, bm.y);
								}
							}
						}
					}
					st.browDragIdx = -1;
				}
			}
			if (!hit.MouseDown() && st.browDragIdx < 0)
				st.browDragging = false;
			st.browMouseWasDown = hit.MouseDown();

			char cnt[32];
			snprintf(cnt, sizeof(cnt), "%d element(s)", shown);
			const float32 ew = p.TextW(cnt);
			p.TextV(r.x + r.w - ew - kPad, r.y + r.h - kRowH, kRowH, cnt, NkRole::TextMuted);
			(void)ih;
		}

		// â”€â”€ CONTENU DES MENUS â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// Une TABLE plutot que du code : ajouter une entree ne demande pas de
		// toucher au rendu, et la meme table servira la palette de recherche et le
		// menu contextuel -- une liste ecrite deux fois finit par diverger.
		//
		// `command` est la CLE STABLE de NkShortcutTable quand l'entree en a une :
		// c'est elle qui permet d'afficher le raccourci sans le recopier. Vide pour
		// les entrees qui n'en ont pas encore.
		struct NkMenuItem {
				const char *label;   ///< nullptr = separateur
				const char *command; ///< cle NkShortcutTable, ou ""
				bool submenu;		 ///< ouvre un sous-menu
		};
		struct NkMenuDef {
				const char *title;
				const NkMenuItem *items;
				int32 count;
		};

		inline const NkMenuDef *NkMenus(int32 &outCount) {
			static const NkMenuItem kFile[] = {
				{"Nouveau", "", false},
				{"Ouvrir...", "", false},
				{"Ouvrir recent", "", true},
				{nullptr, "", false},
				// Le RACCOURCI est lu dans la table, jamais recopie ici : rebinder
				// une touche met l'affichage a jour tout seul.
				{"Enregistrer", "app.enregistrer", false},
				{"Enregistrer tout", "app.enregistrer_tout", false},
				{"Enregistrer sous...", "", false},
				{nullptr, "", false},
				{"Importer", "", true},
				{"Exporter", "", true},
				{nullptr, "", false},
				{"Quitter", "", false},
			};
			static const NkMenuItem kEdit[] = {
				{"Annuler", "app.annuler", false}, {"Refaire", "app.refaire", false},
				{nullptr, "", false},			   {"Dupliquer", "objet.dupliquer", false},
				{"Supprimer", "objet.supprimer", false}, {nullptr, "", false},
				{"Preferences...", "", false},
			};
			static const NkMenuItem kWindow[] = {
				{"Hierarchie", "", false},		{"Proprietes", "", false}, {"Details", "", false},
				{"Navigateur de projet", "", false}, {nullptr, "", false},
				{"Panneau d'outils", "app.panneau_outils", false}, {nullptr, "", false},
				{"Plein ecran", "", false},
			};
			static const NkMenuItem kTools[] = {
				{"Rechercher une commande", "app.palette", false}, {nullptr, "", false},
				{"Retopologier", "", false},   {"Decimer...", "", false},
				{nullptr, "", false},		   {"Extensions", "", true},
			};
			static const NkMenuItem kSelect[] = {
				{"Tout selectionner", "", false}, {"Tout deselectionner", "", false},
				{"Inverser", "", false},		  {nullptr, "", false},
				{"Par type", "", true},
			};
			static const NkMenuItem kObject[] = {
				{"Deplacer", "objet.deplacer", false}, {"Tourner", "objet.tourner", false},
				{"Redimensionner", "objet.echelle", false}, {nullptr, "", false},
				{"Ajouter un modificateur", "", true}, {"Appliquer tout", "", false},
			};
			static const NkMenuItem kHelp[] = {
				{"Documentation", "", false}, {"Raccourcis clavier", "", false},
				{nullptr, "", false},		  {"A propos", "", false},
			};
			// LE NOMBRE D'ENTREES SE DEDUIT DE LA TABLE, il ne se recopie pas.
			// Il etait ecrit a la main : ajouter « Enregistrer tout » a kFile sans
			// toucher le 11 a fait DISPARAITRE « Quitter » (constate par Rihen).
			// C'est la quatrieme fois que ce depot paie une table recopiee ailleurs
			// (les combos de la sortie, kVidExt fige a 3, kHdrNames reste a six) --
			// ici la duplication disparait pour de bon.
#define NK_MENU(nom, tab) {nom, tab, (int32)(sizeof(tab) / sizeof((tab)[0]))}
			static const NkMenuDef kMenus[] = {
				NK_MENU("Fichier", kFile),	   NK_MENU("Edition", kEdit),
				NK_MENU("Fenetre", kWindow),   NK_MENU("Outils", kTools),
				NK_MENU("Selection", kSelect), NK_MENU("Objet", kObject),
				NK_MENU("Aide", kHelp),
			};
#undef NK_MENU
			outCount = (int32)(sizeof(kMenus) / sizeof(kMenus[0]));
			return kMenus;
		}

	} // namespace nk3d
} // namespace nkentseu
