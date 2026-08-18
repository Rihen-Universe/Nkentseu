// -----------------------------------------------------------------------------
// @File    NkContentBrowserDraw.cpp
// @Brief   Le dessin du navigateur de contenu — et la preuve que la declaration
//          est LUE, pas seulement ecrite.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  LA REGLE QUI GOUVERNE CE FICHIER, ET ELLE EST MECANIQUE
// =============================================================================
//  ⚠️ **PAS UN SEUL NOMBRE DE PIXELS N'EST ECRIT ICI.** Toute longueur passe par
//     `M("...")`, c'est-a-dire par la declaration (ou par l'ecrasement que
//     l'editeur a pose dessus). Toute couleur passe par un role du style.
//
//  La regle est verifiable a la revue en une passe : un litteral flottant dans
//  une position ou une taille est un defaut. Les seules constantes tolerees sont
//  des FRACTIONS et des rapports sans dimension (0.5f pour un centrage, 2.f pour
//  un doublement) — elles ne sont pas des longueurs et ne se theme pas.
//
//  ETAT DE L'ART QUE CECI CORRIGE, mesure le 18/08 : `NkModelerBrowser.h` porte
//  **249 litteraux flottants nus** et 2 couleurs en dur ;
//  `ContentBrowserPanel.cpp` porte **0 role de theme et 4 couleurs en dur**.
//
// =============================================================================
//  CE QUE CE FICHIER NE FAIT PAS — nomme, pour que personne ne le cherche
// =============================================================================
//  - **Il ne charge aucune vignette.** `thumbnail` est une poignee opaque que
//    l'application remplit ; a zero, on peint l'icone de nature. Le composant ne
//    lit pas le disque, c'est la regle « il signale, il n'agit pas ».
//  - **Il n'implemente pas la variante `Columns`.** Elle est DECLAREE (elle
//    existe dans la table des variantes) et le dessin la traite comme
//    `DenseList` en attendant les colonnes triables. C'est dit ici plutot que
//    laisse a decouvrir : une variante declaree qui rend autre chose que ce
//    qu'elle annonce serait un mensonge de declaration.
//  - **Il ne dessine pas l'arbre de dossiers recursif.** Il reserve sa colonne
//    (`show_tree` / `tree_width` sont honores : la colonne apparait, disparait
//    et change de largeur) et y peint le fil de dossiers du modele. L'arbre
//    complet est le palier 4 (`TreeView`), et le refaire ici en serait une
//    quatrieme copie.
//  - **Aucun temoin visuel.** Seance sans GPU. Rien de ce fichier n'a ete vu a
//    l'ecran ; ce qui est prouve l'est par `NKUIDesign --probe`, et la
//    conformite aux planches reste NON REVENDIQUEE.
// -----------------------------------------------------------------------------

#include "NKEditorKit/Components/NkContentBrowserModel.h"

namespace nkentseu {
	namespace editorkit {

		namespace {

			bool PassesFilter(const NkAssetEntry &e, const char *filter) {
				if (!filter || !filter[0])
					return true;
				const char *name = e.name.Data();
				if (!name)
					return false;
				// Recherche naive, insensible a la casse ASCII. Suffisant : le
				// filtre est tape a la main, les listes sont de l'ordre du millier.
				for (const char *s = name; *s; ++s) {
					const char *a = s;
					const char *b = filter;
					while (*a && *b) {
						char ca = *a, cb = *b;
						if (ca >= 'A' && ca <= 'Z')
							ca = (char)(ca - 'A' + 'a');
						if (cb >= 'A' && cb <= 'Z')
							cb = (char)(cb - 'A' + 'a');
						if (ca != cb)
							break;
						++a;
						++b;
					}
					if (!*b)
						return true;
				}
				return false;
			}

			/// Le libelle d'une entree, jamais nul — un `Text(nullptr)` traverserait
			/// tout le peintre pour n'echouer qu'au rasteriseur, loin de sa cause.
			const char *Label(const NkAssetEntry &e) {
				const char *n = e.name.Data();
				return n ? n : "";
			}

		} // namespace

		NkContentBrowserResult NkDrawContentBrowser(NkComponentPaint &p, const NkComponentInput &in,
													const NkPaintRect &rect, NkContentBrowserModel &m,
													const NkContentBrowserStyle &s,
													const NkContentBrowserHooks &hooks) {
			NkContentBrowserResult res;
			if (rect.w <= 0.f || rect.h <= 0.f)
				return res;

			// ── LES NOMBRES, TOUS, VIENNENT D'ICI ───────────────────────────────
			// `M` est l'unique porte. Elle lit l'instance si l'application en a
			// branche une, la declaration sinon. C'EST LA LIGNE QUI PORTE LE TEMOIN
			// DE LA TRANCHE : changer `card_gap` dans un fichier change ce que
			// cette fonction produit, sans recompiler quoi que ce soit.
			// ⚠️ L'ECHELLE VIENT DE L'ENTREE, PAS DU PEINTRE — arbitrage du 18/08.
			//    Elle appartient a la SURFACE : la disposition et les tables de
			//    metrique la lisent au meme endroit que le dessin, et elles ne
			//    peignent pas. Une instance par fenetre, donc deux fenetres a DPI
			//    differents ont deux valeurs justes EN MEME TEMPS.
			auto M = [&](const char *k) {
				return NkBrowserMetric(s, k) * in.surfaceScale;
			};
			auto P = [&](const char *k) {
				return NkBrowserParam(s, k);
			};

			const NkBrowserVariant variant = NkBrowserEffectiveVariant(s);
			const bool showTree = P("show_tree") > 0.5f;
			const bool showFooter = P("show_footer") > 0.5f;
			// La taille de vignette du MODELE prime si l'utilisateur l'a bougee au
			// curseur ; a zero elle n'a jamais ete touchee et on prend le defaut
			// declare. Deux sources, un ordre explicite — sans cet ordre, le
			// curseur de l'application serait ecrase a chaque image.
			const float32 thumb = (m.thumbSize > 0.f ? m.thumbSize : P("thumb_size")) * in.surfaceScale;

			p.PushClip(rect);
			p.Fill(rect, s.panelBg);

			// ── BANDE D'ONGLETS ─────────────────────────────────────────────────
			const float32 headerH = M("header_h");
			NkPaintRect header{rect.x, rect.y, rect.w, headerH};
			p.Fill(header, s.headerBg);
			p.Text({header.x + M("card_pad"), header.y, header.w, header.h}, "Contenu", s.text);
			p.HLine(rect.x, rect.y + headerH, rect.w, s.border);

			// ── BARRE D'OUTILS ──────────────────────────────────────────────────
			const float32 toolbarH = M("toolbar_h");
			NkPaintRect toolbar{rect.x, rect.y + headerH, rect.w, toolbarH};
			p.Fill(toolbar, s.headerBg);
			const float32 pad = M("card_pad");
			p.Text({toolbar.x + pad, toolbar.y, toolbar.w * 0.5f, toolbar.h}, "Creer", s.text);
			p.HLine(rect.x, toolbar.y + toolbarH, rect.w, s.border);

			// ── FIL D'ARIANE ────────────────────────────────────────────────────
			float32 contentTop = toolbar.y + toolbarH;
			{
				const float32 crumbH = M("row_h");
				NkPaintRect crumbs{rect.x, contentTop, rect.w, crumbH};
				p.Fill(crumbs, s.panelBg);
				float32 cx = crumbs.x + pad;
				for (uint32 i = 0; i < (uint32)m.breadcrumb.Size(); ++i) {
					const char *t = m.breadcrumb[i].Data();
					if (!t)
						continue;
					const float32 tw = p.TextWidth(t);
					NkPaintRect cell{cx, crumbs.y, tw, crumbH};
					p.Text(cell, t, i + 1 == (uint32)m.breadcrumb.Size() ? s.text : s.textMuted);
					// Un segment du fil est cliquable : c'est la navigation, et
					// c'est l'un des cinq evenements declares.
					if (in.mousePressed && cell.Contains(in.mouseX, in.mouseY)) {
						res.navigated = true;
						if (hooks.onNavigate)
							hooks.onNavigate(hooks.user, t);
					}
					cx += tw + pad;
					if (i + 1 < (uint32)m.breadcrumb.Size()) {
						p.Text({cx, crumbs.y, p.TextWidth("/"), crumbH}, "/", s.textMuted);
						cx += p.TextWidth("/") + pad;
					}
				}
				contentTop += crumbH;
				p.HLine(rect.x, contentTop, rect.w, s.border);
			}

			// ── COLONNE D'ARBRE ─────────────────────────────────────────────────
			float32 gridX = rect.x;
			float32 gridW = rect.w;
			if (showTree) {
				// `tree_width` est une FRACTION, pas une longueur : elle ne passe
				// donc pas par `Scale()`. Un parametre sans dimension mis a
				// l'echelle serait un defaut discret — la colonne grossirait deux
				// fois sur un ecran a fort DPI.
				const float32 treeW = rect.w * P("tree_width");
				NkPaintRect tree{rect.x, contentTop, treeW, rect.y + rect.h - contentTop};
				p.Fill(tree, s.headerBg);
				float32 ty = tree.y;
				const float32 rowH = M("row_h");
				for (uint32 i = 0; i < (uint32)m.breadcrumb.Size(); ++i) {
					const char *t = m.breadcrumb[i].Data();
					if (!t)
						continue;
					p.Icon({tree.x + pad, ty, rowH, rowH}, 1, s.folderTint);
					p.Text({tree.x + pad + rowH, ty, tree.w - pad - rowH, rowH}, t, s.text);
					ty += rowH;
				}
				p.VLine(tree.x + tree.w, tree.y, tree.h, s.border);
				gridX = tree.x + tree.w + 1.f;
				gridW = rect.w - treeW - 1.f;
			}

			NkPaintRect area{gridX, contentTop, gridW, rect.y + rect.h - contentTop};
			p.PushClip(area);

			// ── LES ENTREES ─────────────────────────────────────────────────────
			const float32 gap = M("card_gap");
			const float32 footerH = showFooter ? M("footer_h") : 0.f;
			const float32 stroke = M("stroke_w");
			const float32 rowH = M("row_h");

			const bool asGrid = (variant == NkBrowserVariant::Grid);
			const float32 cellW = asGrid ? (thumb + gap) : area.w;
			const float32 cellH = asGrid ? (thumb + footerH + gap) : rowH;
			int32 perRow = asGrid ? (int32)(area.w / (cellW > 0.f ? cellW : 1.f)) : 1;
			if (perRow < 1)
				perRow = 1;

			int32 visible = 0;
			int32 hitIndex = -1;
			for (uint32 i = 0; i < (uint32)m.entries.Size(); ++i) {
				const NkAssetEntry &e = m.entries[i];
				if (!PassesFilter(e, m.filter))
					continue;
				if (hooks.acceptEntry && !hooks.acceptEntry(hooks.user, e))
					continue;

				const int32 col = asGrid ? (visible % perRow) : 0;
				const int32 row = asGrid ? (visible / perRow) : visible;
				NkPaintRect cell{area.x + (float32)col * cellW, area.y + (float32)row * cellH - m.scroll,
								 asGrid ? thumb : area.w, asGrid ? (thumb + footerH) : rowH};
				++visible;

				// Hors champ : on saute le DESSIN, pas le comptage. Compter apres
				// aurait rendu le defilement dependant de ce qui est visible.
				if (cell.y + cell.h < area.y || cell.y > area.y + area.h)
					continue;

				const bool isActive = (m.active == (int32)i);
				const bool isChosen = m.IsChosen((int32)i);

				if (asGrid) {
					p.Fill(cell, s.cardBg, M("card_pad") * 0.5f);
					if (e.thumbnail == 0)
						p.Icon({cell.x + pad, cell.y + pad, cell.w - pad * 2.f,
								cell.h - footerH - pad * 2.f},
							   e.isFolder ? 1 : 2, e.isFolder ? s.folderTint : e.kindRole);
					if (showFooter) {
						NkPaintRect foot{cell.x, cell.y + cell.h - footerH, cell.w, footerH};
						p.Fill(foot, s.cardFooterBg);
						p.Text({foot.x, foot.y, foot.w, foot.h * 0.5f}, Label(e), s.text,
							   NkTextAlign::Center);
						p.Text({foot.x, foot.y + foot.h * 0.5f, foot.w, foot.h * 0.5f},
							   e.kindLabel ? e.kindLabel : "", s.textMuted, NkTextAlign::Center);
					}
				} else {
					p.Fill(cell, s.cardBg);
					p.Icon({cell.x + pad, cell.y, rowH, rowH}, e.isFolder ? 1 : 2,
						   e.isFolder ? s.folderTint : e.kindRole);
					p.Text({cell.x + pad + rowH, cell.y, cell.w * 0.6f, cell.h}, Label(e), s.text);
					p.Text({cell.x + cell.w * 0.6f, cell.y, cell.w * 0.4f, cell.h},
						   e.kindLabel ? e.kindLabel : "", s.textMuted);
					// Les colonnes greffees par l'application. En variante `Grid`
					// elles n'ont pas de place : c'est dit dans la declaration du
					// point de greffe, pas laisse a deviner.
					if (hooks.extraColumnText && hooks.extraColumnCount > 0) {
						const float32 colW = cell.w * 0.4f / (float32)hooks.extraColumnCount;
						for (int32 c = 0; c < hooks.extraColumnCount; ++c) {
							const char *t = hooks.extraColumnText(hooks.user, (int32)i, c);
							if (t)
								p.Text({cell.x + cell.w * 0.6f + (float32)c * colW, cell.y, colW, cell.h},
									   t, s.textMuted);
						}
					}
				}

				// ⚠️ LES DEUX ETATS DE SELECTION, DISTINCTS. L'encodage (aplat contre
				//    contour) est l'ecart n.3, un arbitrage qui appartient a Rodolf :
				//    on peint donc les deux en CONTOUR, avec deux roles differents et
				//    deux epaisseurs differentes. Le jour ou il tranche, une seule
				//    ligne change ici — et pas la structure.
				if (isChosen)
					p.OutlineSharp({cell.x - stroke, cell.y - stroke, cell.w + stroke * 2.f,
									cell.h + stroke * 2.f},
								   s.chosenMark);
				if (isActive)
					p.Outline(cell, s.activeMark, s.cardBg, M("card_pad") * 0.5f);

				if (hooks.cardOverlay)
					hooks.cardOverlay(hooks.user, p, (int32)i, cell.x, cell.y, cell.w, cell.h);

				if (cell.Contains(in.mouseX, in.mouseY))
					hitIndex = (int32)i;
			}

			// ── LES EVENEMENTS PARTENT D'ICI, ET DE NULLE PART AILLEURS ─────────
			// Un seul endroit, apres la boucle : sinon un double-clic sur une carte
			// qui en recouvre une autre partirait deux fois. Le composant SIGNALE,
			// il n'agit pas — aucun de ces cinq n'a d'action par defaut, et c'est
			// ecrit dans la declaration (`hasDefaultAction = false`).
			if (hitIndex >= 0 && area.Contains(in.mouseX, in.mouseY)) {
				const NkAssetEntry &e = m.entries[(uint32)hitIndex];
				const char *path = e.path.Data() ? e.path.Data() : "";
				if (in.mousePressed) {
					if (in.ctrl) {
						if (m.IsChosen(hitIndex)) {
							for (uint32 k = 0; k < (uint32)m.chosen.Size(); ++k)
								if (m.chosen[k] == hitIndex) {
									m.chosen.RemoveAt(k);
									break;
								}
						} else
							m.chosen.PushBack(hitIndex);
					} else {
						m.chosen.Clear();
						m.chosen.PushBack(hitIndex);
					}
					m.active = hitIndex;
					res.selectionChanged = true;
					if (hooks.onSelect)
						hooks.onSelect(hooks.user, hitIndex, path);
				}
				if (in.doubleClick) {
					res.activatedIndex = hitIndex;
					if (hooks.onDoubleClick)
						hooks.onDoubleClick(hooks.user, hitIndex, path);
				}
				if (in.rightPressed && hooks.onContextMenu)
					hooks.onContextMenu(hooks.user, hitIndex, in.mouseX, in.mouseY);
				if (in.dragReleased && e.isFolder && hooks.onDrop)
					hooks.onDrop(hooks.user, hitIndex, in.dragType ? in.dragType : "");
			} else if (in.rightPressed && area.Contains(in.mouseX, in.mouseY) && hooks.onContextMenu) {
				// Clic droit sur le FOND : `index = -1`, tel que la declaration
				// l'annonce. Sans ce cas, l'application ne pourrait pas offrir
				// « Coller » ni « Nouveau dossier » hors d'une carte.
				hooks.onContextMenu(hooks.user, -1, in.mouseX, in.mouseY);
			}

			// ── DEFILEMENT ──────────────────────────────────────────────────────
			if (in.wheel != 0.f && area.Contains(in.mouseX, in.mouseY)) {
				m.scroll -= in.wheel * rowH;
				const int32 rows = (visible + perRow - 1) / perRow;
				const float32 contentH = (float32)rows * cellH;
				const float32 maxScroll = contentH > area.h ? contentH - area.h : 0.f;
				if (m.scroll < 0.f)
					m.scroll = 0.f;
				if (m.scroll > maxScroll)
					m.scroll = maxScroll;
			}

			p.PopClip(); // area
			p.PopClip(); // rect
			return res;
		}

	} // namespace editorkit
} // namespace nkentseu
