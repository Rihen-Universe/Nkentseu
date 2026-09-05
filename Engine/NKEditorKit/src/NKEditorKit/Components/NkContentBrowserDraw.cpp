// -----------------------------------------------------------------------------
// @File    NkContentBrowserDraw.cpp
// @Brief   Le dessin du navigateur de contenu — LE MIXTE Unreal + Aetherion
//          (Rodolf, 2026-08-30), et la preuve que la declaration est LUE.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
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
// =============================================================================
//  CE QUE LE MIXTE PORTE (directive de Rodolf, 2026-08-30)
// =============================================================================
//  D'Aetherion (la structure) : fil d'Ariane cliquable + recherche · puces de
//  filtre par nature, colorees, combinables · arbre de dossiers repliable ·
//  cartes avec badge de type colore · compteur + « Tout selectionner » +
//  « Trier par : Nom » · curseur de taille + bascule grille/liste · barre
//  d'etat basse · bouton Importer en accent.
//  D'Unreal (les comportements) : Creer / Importer / Tout enregistrer en tete ·
//  le type sous le nom · selection multiple avec compteur en barre d'etat ·
//  Favoris et Recents = des RACINES de l'arbre de dossiers (de la donnee, pas
//  du code).
//
//  ⚠️ LA COLONNE DE DOSSIERS EST `tree_view`, PAS UNE COPIE. La declaration le
//     promettait (`folder_tree` porte `component = "tree_view"`) ; le dessin le
//     tient : `NkDrawTreeView` sur `m.folders`. Une quatrieme copie d'arbre
//     aurait ete exactement ce que la porte « la couche du dessous d'abord »
//     interdit.
//
// =============================================================================
//  CE QUE CE FICHIER NE FAIT PAS — nomme, pour que personne ne le cherche
// =============================================================================
//  - **Il ne charge aucune vignette, et il n'en AFFICHE pas encore.** C'est le
//    MORCEAU 2 du chantier : il exige une methode d'image sur
//    `NkComponentPaint` (additive, avec defaut), pas encore tranchee a l'ecran.
//    `thumbnail` reste une poignee opaque ; a zero, on peint l'icone de nature.
//  - **Il n'implemente pas la variante `Columns`.** Declaree, rendue comme
//    `DenseList` en attendant les colonnes triables — dit ici plutot que
//    laisse a decouvrir.
//  - **La saisie CLAVIER de la recherche transite par l'hote.** Le composant
//    pose `searchFocused` au clic et AFFICHE `filter` ; l'hote, qui a le
//    clavier, y ecrit. Meme contournement assume que `renameBuf` du tree_view :
//    l'entree clavier manque a `NkComponentInput`, et c'est deja au canal.
//  - **La variante `minimal` est l'ANCIEN rendu**, garde tel quel : en-tete,
//    bande « Creer », fil d'Ariane, colonne de dossiers simple, grille nue.
//    « Plusieurs composants, l'application choisit » (Rodolf).
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

			/// Les puces de filtre : aucune enfoncee = tout passe ; sinon OU entre
			/// les natures enfoncees. Les DOSSIERS passent toujours — les puces
			/// filtrent des fichiers (regle du navigateur historique, qui n'a pas
			/// de puce « dossier »).
			bool PassesKinds(const NkContentBrowserModel &m, const NkAssetEntry &e) {
				if (e.isFolder)
					return true;
				bool anyActive = false;
				for (uint32 k = 0; k < (uint32)m.kinds.Size(); ++k) {
					if (!m.kinds[k].active)
						continue;
					anyActive = true;
					if (m.kinds[k].role == e.kindRole)
						return true;
				}
				return !anyActive;
			}

			/// Le libelle d'une entree, jamais nul — un `Text(nullptr)` traverserait
			/// tout le peintre pour n'echouer qu'au rasteriseur, loin de sa cause.
			const char *Label(const NkAssetEntry &e) {
				const char *n = e.name.Data();
				return n ? n : "";
			}

			/// Comparaison de tri : dossiers d'abord, puis nom (ASCII, sans casse).
			/// Rend vrai si `a` passe AVANT `b` en ordre croissant.
			bool SortBefore(const NkAssetEntry &a, const NkAssetEntry &b) {
				if (a.isFolder != b.isFolder)
					return a.isFolder;
				const char *pa = Label(a), *pb = Label(b);
				while (*pa && *pb) {
					char ca = *pa, cb = *pb;
					if (ca >= 'A' && ca <= 'Z')
						ca = (char)(ca - 'A' + 'a');
					if (cb >= 'A' && cb <= 'Z')
						cb = (char)(cb - 'A' + 'a');
					if (ca != cb)
						return ca < cb;
					++pa;
					++pb;
				}
				return *pb != 0; // prefixe commun : le plus court d'abord
			}

			/// Un entier en texte, sans <cstdio> (zero-STL). Ecrit a `out + at`,
			/// rend la nouvelle fin. `cap` compte le zero terminal.
			uint32 PutUInt(char *out, uint32 cap, uint32 at, uint32 v) {
				char tmp[12];
				uint32 n = 0;
				do {
					tmp[n++] = (char)('0' + (v % 10u));
					v /= 10u;
				} while (v && n < sizeof(tmp));
				while (n && at + 1 < cap)
					out[at++] = tmp[--n];
				out[at] = '\0';
				return at;
			}
			uint32 PutStr(char *out, uint32 cap, uint32 at, const char *s) {
				for (; s && *s && at + 1 < cap; ++s)
					out[at++] = *s;
				out[at] = '\0';
				return at;
			}

			/// Un bouton de texte : fond, contour, libelle centre. Rend vrai au
			/// clic. AUCUNE decision de rendu ici — les roles viennent de l'appelant.
			bool TextButton(NkComponentPaint &p, const NkComponentInput &in, const NkPaintRect &r,
							const char *label, uint16 bg, uint16 txt, uint16 outline,
							float32 rounding) {
				p.Fill(r, bg, rounding);
				if (outline)
					p.OutlineSharp(r, outline);
				p.Text(r, label, txt, NkTextAlign::Center);
				return in.mousePressed && r.Contains(in.mouseX, in.mouseY);
			}

			/// L'instance qui regle le tree_view EMBARQUE : pas de bande de titre,
			/// pas de recherche, pas de pied (le navigateur a les siens), pas de
			/// colonnes oeil/cadenas (des dossiers), double-clic = activer (un
			/// arbre de DOSSIERS ouvre, il ne renomme pas — la meme mesure que
			/// NKCode/NkExplorer). Construite UNE fois : c'est un reglage du
			/// composant, pas un etat.
			const NkComponentInstance &EmbeddedTreeValues() {
				static NkComponentInstance inst(NkTreeViewDecl());
				static bool init = false;
				if (!init) {
					inst.SetParam("show_header", 0.f);
					inst.SetParam("show_search", 0.f);
					inst.SetParam("show_footer", 0.f);
					inst.SetParam("show_visibility", 0.f);
					inst.SetParam("show_lock", 0.f);
					inst.SetParam("activate_on_double_click", 1.f);
					init = true;
				}
				return inst;
			}

			// ── LA VIGNETTE EST PEINTE (2026-09-05) ────────────────────────
			// ⚠️ MESURE AVANT CORRECTION : `NkAssetEntry::thumbnail` etait declaree,
			//    documentee (« identifiant OPAQUE »), portee par le modele... et lue
			//    NULLE PART. Le dessin ne peignait l'icone que si elle valait ZERO :
			//    une entree AVEC vignette ne montrait donc RIEN -- ni image, ni icone.
			//    C'est la neuvieme fois de ce chantier qu'un parametre declare n'est
			//    pas honore, et le seul remede qui tienne est de le mesurer.
			// Le contrat de `ImagePolygone` est respecte : un peintre qui ne sait pas
			// texturer rend FAUX, et l'appelant retombe sur l'icone -- rien n'est simule.
			bool DrawThumb(NkComponentPaint &p, const NkPaintRect &r, nk_uint64 handle) {
				if (handle == 0 || r.w <= 0.f || r.h <= 0.f)
					return false;
				const float32 xy[8] = {r.x, r.y, r.x + r.w, r.y, r.x + r.w, r.y + r.h, r.x, r.y + r.h};
				const float32 uv[8] = {0.f, 0.f, 1.f, 0.f, 1.f, 1.f, 0.f, 1.f};
				return p.ImagePolygone(xy, uv, 4, (uint32)handle, 100.f);
			}

			/// Le pont des evenements de l'arbre embarque vers ceux du navigateur :
			/// une selection de dossier EST une navigation.
			struct TreeBridge {
					NkContentBrowserResult *res = nullptr;
					const NkContentBrowserHooks *hooks = nullptr;
			};
			void TreeOnSelect(void *user, int32 index, const char *id) {
				TreeBridge *b = (TreeBridge *)user;
				b->res->navigated = true;
				if (b->hooks->onNavigate)
					b->hooks->onNavigate(b->hooks->user, id ? id : "");
				(void)index;
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
			auto M = [&](const char *k) {
				return NkBrowserMetric(s, k) * in.surfaceScale;
			};
			auto P = [&](const char *k) {
				return NkBrowserParam(s, k);
			};

			// La variante effective, puis le GESTE DE L'UTILISATEUR par-dessus : la
			// bascule grille/liste (m.viewMode) prime sur le style, exactement
			// comme m.thumbSize prime sur thumb_size — sans cet ordre, le bouton
			// de la barre de filtres serait ecrase a chaque image.
			NkBrowserVariant variant = NkBrowserEffectiveVariant(s);
			const bool minimal = (variant == NkBrowserVariant::Minimal);
			if (!minimal && m.viewMode == 0)
				variant = NkBrowserVariant::Grid;
			else if (!minimal && m.viewMode == 1)
				variant = NkBrowserVariant::DenseList;

			const bool showTree = P("show_tree") > 0.5f && !m.treeCollapsed;
			const bool showFooter = P("show_footer") > 0.5f;
			const bool showFilters = !minimal && P("show_filters") > 0.5f;
			const bool showStatus = !minimal && P("show_status") > 0.5f;
			// ② (05/09) La bande de tete et les trois boutons d'action se taisent sur
			//    demande : un selecteur de fichiers reutilise ce dessin comme volet
			//    droit, et n'y « cree » ni n'y « importe » rien. Defaut = 1 partout.
			const bool showHeader = P("show_header") > 0.5f;
			const bool showActions = P("show_actions") > 0.5f;
			const float32 thumb = (m.thumbSize > 0.f ? m.thumbSize : P("thumb_size")) * in.surfaceScale;

			const float32 pad = M("card_pad");
			const float32 rowH = M("row_h");

			p.PushClip(rect);
			p.Fill(rect, s.panelBg);

			// ── BANDE D'ONGLETS ─────────────────────────────────────────────────
			const float32 headerH = showHeader ? M("header_h") : 0.f;
			NkPaintRect header{rect.x, rect.y, rect.w, headerH};
			if (showHeader) {
				p.Fill(header, s.headerBg);
				// ⚠️ `header.w - 2 * card_pad`, PAS `header.w` — le texte est pose a
				//    `header.x + card_pad` ; la largeur pleine lui ferait calculer son
				//    point de troncature sur 2 pads qu'il n'a pas (mesure 34b).
				p.Text({header.x + pad, header.y, header.w - 2.f * pad, header.h},
					   m.headerTitle.Empty() ? "Contenu" : m.headerTitle.Data(), s.text);
				p.HLine(rect.x, rect.y + headerH, rect.w, s.border);
			}

			// ── BARRE D'OUTILS ──────────────────────────────────────────────────
			// Mixte : Creer / Importer / Tout enregistrer + fil d'Ariane + boite de
			// recherche. Minimal : l'ancienne bande (« Creer » texte inerte).
			const float32 toolbarH = M("toolbar_h");
			NkPaintRect toolbar{rect.x, rect.y + headerH, rect.w, toolbarH};
			p.Fill(toolbar, s.headerBg);
			float32 contentTop = toolbar.y + toolbarH;
			NkPaintRect searchBox{0.f, 0.f, 0.f, 0.f};
			if (minimal) {
				p.Text({toolbar.x + pad, toolbar.y, toolbar.w * 0.5f, toolbar.h}, "Créer", s.text);
			} else {
				const float32 btnH = rowH;
				const float32 btnY = toolbar.y + (toolbar.h - btnH) * 0.5f;
				float32 bx = toolbar.x + pad;
				// Creer — le « Add » d'Unreal, au nom de l'historique.
				if (showActions) {
					const float32 bw = p.TextWidth("Créer") + 2.f * pad;
					NkPaintRect r{bx, btnY, bw, btnH};
					if (TextButton(p, in, r, "Créer", s.cardBg, s.text, s.border, 0.f) &&
						hooks.onCreate)
						hooks.onCreate(hooks.user);
					bx += bw + pad * 0.5f;
				}
				// Importer — en ACCENT, comme le bouton d'Aetherion.
				if (showActions) {
					const float32 bw = p.TextWidth("Importer") + 2.f * pad;
					NkPaintRect r{bx, btnY, bw, btnH};
					if (TextButton(p, in, r, "Importer", s.activeMark, s.badgeText, 0, 0.f) &&
						hooks.onImport)
						hooks.onImport(hooks.user);
					bx += bw + pad * 0.5f;
				}
				// Tout enregistrer — le « Save All » d'Unreal.
				if (showActions) {
					const float32 bw = p.TextWidth("Tout enregistrer") + 2.f * pad;
					NkPaintRect r{bx, btnY, bw, btnH};
					if (TextButton(p, in, r, "Tout enregistrer", s.cardBg, s.text, s.border, 0.f) &&
						hooks.onSaveAll)
						hooks.onSaveAll(hooks.user);
					bx += bw + pad;
				}
				// La boite de recherche, a droite.
				const float32 searchW = M("search_w");
				searchBox = {toolbar.x + toolbar.w - pad - searchW, btnY, searchW, btnH};
				p.Fill(searchBox, s.cardBg, pad * 0.25f);
				p.OutlineSharp(searchBox, m.searchFocused ? s.activeMark : s.border);
				const bool hasFilter = m.filter[0] != '\0';
				p.Text({searchBox.x + pad * 0.5f, searchBox.y, searchBox.w - pad, searchBox.h},
					   hasFilter ? m.filter : "Rechercher...", hasFilter ? s.text : s.textMuted);
				// Le focus se POSE au clic dans la boite et se REPREND au clic
				// ailleurs — l'hote, qui a le clavier, ecrit dans `filter` tant
				// qu'il est pose (contournement nomme en tete de fichier).
				if (in.mousePressed)
					m.searchFocused = searchBox.Contains(in.mouseX, in.mouseY);

				// LE FIL D'ARIANE, entre les boutons et la recherche, cliquable —
				// clippe a sa zone : un chemin profond ne doit pas traverser la
				// boite de recherche.
				NkPaintRect crumbs{bx, toolbar.y, searchBox.x - pad - bx, toolbar.h};
				if (crumbs.w > 0.f) {
					p.PushClip(crumbs);
					float32 cx = crumbs.x;
					for (uint32 i = 0; i < (uint32)m.breadcrumb.Size(); ++i) {
						const char *t = m.breadcrumb[i].Data();
						if (!t)
							continue;
						const float32 tw = p.TextWidth(t);
						NkPaintRect cell{cx, crumbs.y, tw, crumbs.h};
						p.Text(cell, t, i + 1 == (uint32)m.breadcrumb.Size() ? s.text : s.textMuted);
						if (in.mousePressed && cell.Contains(in.mouseX, in.mouseY)) {
							res.navigated = true;
							// ⚠️ L'INDEX, pas seulement le libelle : deux dossiers du meme
							//    nom dans un chemin (`src/nkgui/src`) rendaient la charge
							//    AMBIGUE -- un selecteur de fichiers ne peut pas deviner
							//    lequel. Le libelle reste, pour ne rien casser.
							res.navigatedCrumb = (int32)i;
							if (hooks.onNavigate)
								hooks.onNavigate(hooks.user, t);
						}
						cx += tw + pad * 0.5f;
						if (i + 1 < (uint32)m.breadcrumb.Size()) {
							p.Text({cx, crumbs.y, p.TextWidth(">"), crumbs.h}, ">", s.textMuted);
							cx += p.TextWidth(">") + pad * 0.5f;
						}
					}
					p.PopClip();
				}
			}
			p.HLine(rect.x, contentTop, rect.w, s.border);

			// ── FIL D'ARIANE (variante minimale seulement — le mixte l'a mis dans
			//    la barre d'outils) ────────────────────────────────────────────────
			if (minimal) {
				const float32 crumbH = rowH;
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

			// ── LA LISTE VISIBLE : filtres, puces, tri — calculee AVANT les
			//    rangees, parce que le compteur en a besoin ────────────────────────
			NkVector<int32> vis;
			for (uint32 i = 0; i < (uint32)m.entries.Size(); ++i) {
				const NkAssetEntry &e = m.entries[i];
				if (!PassesFilter(e, m.filter))
					continue;
				if (!minimal && !PassesKinds(m, e))
					continue;
				if (hooks.acceptEntry && !hooks.acceptEntry(hooks.user, e))
					continue;
				vis.PushBack((int32)i);
			}
			if (!minimal) {
				// Tri par insertion — stable, sans allocation, et les listes sont
				// de l'ordre du millier. Dossiers d'abord, puis nom.
				for (uint32 i = 1; i < (uint32)vis.Size(); ++i) {
					const int32 v = vis[i];
					uint32 j = i;
					while (j > 0) {
						const bool before = SortBefore(m.entries[(uint32)v],
													   m.entries[(uint32)vis[j - 1]]);
						if (m.sortAsc ? !before : before)
							break;
						vis[j] = vis[j - 1];
						--j;
					}
					vis[j] = v;
				}
			}

			// ── RANGEE DES PUCES DE FILTRE + curseur + bascule ──────────────────
			if (showFilters) {
				const float32 filterH = M("filter_h");
				NkPaintRect fr{rect.x, contentTop, rect.w, filterH};
				const float32 chipH = rowH;
				const float32 chipY = fr.y + (fr.h - chipH) * 0.5f;
				float32 fx = fr.x + pad;
				p.Text({fx, fr.y, p.TextWidth("Filtres :"), fr.h}, "Filtres :", s.textMuted);
				fx += p.TextWidth("Filtres :") + pad;
				for (uint32 k = 0; k < (uint32)m.kinds.Size(); ++k) {
					NkBrowserKind &kind = m.kinds[k];
					const char *lbl = kind.label.Data() ? kind.label.Data() : "";
					// puce : pastille de la couleur de la nature + libelle ;
					// enfoncee = contour de sa couleur et texte plein.
					const float32 dotW = chipH * 0.33f;
					const float32 chipW = dotW + p.TextWidth(lbl) + 2.f * pad;
					NkPaintRect chip{fx, chipY, chipW, chipH};
					p.Fill(chip, s.chipBg, pad * 0.25f);
					if (kind.active)
						p.OutlineSharp(chip, kind.role);
					p.Fill({chip.x + pad * 0.5f, chipY + (chipH - dotW) * 0.5f, dotW, dotW},
						   kind.role, dotW * 0.5f);
					p.Text({chip.x + pad * 0.5f + dotW, chip.y, chipW - dotW - pad, chip.h}, lbl,
						   kind.active ? s.text : s.textMuted);
					if (in.mousePressed && chip.Contains(in.mouseX, in.mouseY))
						kind.active = !kind.active;
					fx += chipW + pad * 0.5f;
				}

				// A droite : curseur de taille de vignettes, puis bascule
				// grille/liste (deux boutons a glyphe dessine — l'atlas d'icones
				// est une dependance nommee de NKGui, on ne l'attend pas ici).
				const float32 togW = chipH;
				float32 rx = fr.x + fr.w - pad - togW;
				{
					// bouton LISTE : trois lignes horizontales.
					NkPaintRect r{rx, chipY, togW, chipH};
					const bool on = (variant == NkBrowserVariant::DenseList);
					p.Fill(r, s.chipBg, pad * 0.25f);
					if (on)
						p.OutlineSharp(r, s.activeMark);
					for (uint32 l = 0; l < 3; ++l)
						p.Fill({r.x + r.w * 0.25f, r.y + r.h * (0.3f + 0.2f * (float32)l),
								r.w * 0.5f, M("stroke_w")},
							   on ? s.text : s.textMuted);
					if (in.mousePressed && r.Contains(in.mouseX, in.mouseY))
						m.viewMode = 1;
					rx -= togW + pad * 0.25f;
				}
				{
					// bouton GRILLE : quatre carres.
					NkPaintRect r{rx, chipY, togW, chipH};
					const bool on = (variant == NkBrowserVariant::Grid);
					p.Fill(r, s.chipBg, pad * 0.25f);
					if (on)
						p.OutlineSharp(r, s.activeMark);
					const float32 q = r.w * 0.2f;
					for (uint32 gy = 0; gy < 2; ++gy)
						for (uint32 gx = 0; gx < 2; ++gx)
							p.Fill({r.x + r.w * 0.25f + (float32)gx * (q + r.w * 0.1f),
									r.y + r.h * 0.25f + (float32)gy * (q + r.h * 0.1f), q, q},
								   on ? s.text : s.textMuted);
					if (in.mousePressed && r.Contains(in.mouseX, in.mouseY))
						m.viewMode = 0;
					rx -= M("slider_w") + pad;
				}
				{
					// LE CURSEUR : la piste, la poignee, et la valeur qui va au
					// MODELE (m.thumbSize — le geste de l'utilisateur, qui prime).
					// Les bornes viennent de la DECLARATION de `thumb_size`.
					const NkParamDecl *tp = NkContentBrowserDecl().FindParam("thumb_size");
					const float32 lo = tp ? tp->minVal : 0.f;
					const float32 hi = tp ? tp->maxVal : 1.f;
					NkPaintRect track{rx, chipY, M("slider_w"), chipH};
					const float32 lineY = track.y + (track.h - M("stroke_w")) * 0.5f;
					p.Fill({track.x, lineY, track.w, M("stroke_w")}, s.border);
					const float32 cur = (m.thumbSize > 0.f ? m.thumbSize : P("thumb_size"));
					float32 t = (hi > lo) ? (cur - lo) / (hi - lo) : 0.f;
					if (t < 0.f)
						t = 0.f;
					if (t > 1.f)
						t = 1.f;
					const float32 hw = pad * 0.75f;
					p.Fill({track.x + t * (track.w - hw), track.y + track.h * 0.2f, hw,
							track.h * 0.6f},
						   s.activeMark, hw * 0.25f);
					if (in.mouseDown && track.Contains(in.mouseX, in.mouseY)) {
						float32 nt = (in.mouseX - track.x) / (track.w > 0.f ? track.w : 1.f);
						if (nt < 0.f)
							nt = 0.f;
						if (nt > 1.f)
							nt = 1.f;
						m.thumbSize = lo + nt * (hi - lo);
					}
				}
				contentTop += filterH;
				p.HLine(rect.x, contentTop, rect.w, s.border);
			}

			// ── RANGEE D'INFORMATION : repli de l'arbre, compteur, Tout
			//    selectionner, Trier par ──────────────────────────────────────────
			if (!minimal) {
				const float32 infoH = M("info_h");
				NkPaintRect ir{rect.x, contentTop, rect.w, infoH};
				float32 ix = ir.x + pad;
				{
					// Le repli de la colonne de dossiers (Aetherion : repliable).
					const char *lbl = m.treeCollapsed ? ">" : "<";
					NkPaintRect r{ix, ir.y + (ir.h - rowH) * 0.5f, rowH, rowH};
					if (r.h > ir.h) {
						r.y = ir.y;
						r.h = ir.h;
					}
					if (TextButton(p, in, r, lbl, s.chipBg, s.textMuted, s.border, 0.f))
						m.treeCollapsed = !m.treeCollapsed;
					ix += r.w + pad;
				}
				// « N elements · M selectionne(s) » — compte sur la liste VISIBLE,
				// comme Aetherion (« 18 elements · 1 selectionne »).
				char cnt[96];
				uint32 at = PutUInt(cnt, sizeof(cnt), 0, (uint32)vis.Size());
				at = PutStr(cnt, sizeof(cnt), at, " élément(s)");
				if ((uint32)m.chosen.Size() > 0) {
					at = PutStr(cnt, sizeof(cnt), at, " · ");
					at = PutUInt(cnt, sizeof(cnt), at, (uint32)m.chosen.Size());
					at = PutStr(cnt, sizeof(cnt), at, " sélectionné(s)");
				}
				p.Text({ix, ir.y, ir.w * 0.5f, ir.h}, cnt, s.textMuted);

				// A droite : « Trier par : Nom » puis « Tout selectionner ».
				const char *sortLbl = m.sortAsc ? "Trier par : Nom (a-z)" : "Trier par : Nom (z-a)";
				const float32 sortW = p.TextWidth(sortLbl) + 2.f * pad;
				NkPaintRect sortBtn{ir.x + ir.w - pad - sortW, ir.y, sortW, ir.h};
				p.Text(sortBtn, sortLbl, s.textMuted, NkTextAlign::Center);
				if (in.mousePressed && sortBtn.Contains(in.mouseX, in.mouseY))
					m.sortAsc = !m.sortAsc;
				const char *selLbl = "Tout sélectionner";
				const float32 selW = p.TextWidth(selLbl) + 2.f * pad;
				NkPaintRect selBtn{sortBtn.x - pad - selW, ir.y, selW, ir.h};
				p.Text(selBtn, selLbl, s.textMuted, NkTextAlign::Center);
				if (in.mousePressed && selBtn.Contains(in.mouseX, in.mouseY)) {
					m.chosen.Clear();
					for (uint32 i = 0; i < (uint32)vis.Size(); ++i)
						m.chosen.PushBack(vis[i]);
					res.selectionChanged = true;
				}
				contentTop += infoH;
				p.HLine(rect.x, contentTop, rect.w, s.border);
			}

			// ── LE CORPS : colonne de dossiers + vue d'assets ───────────────────
			const float32 statusH = showStatus ? M("status_h") : 0.f;
			const float32 bodyBottom = rect.y + rect.h - statusH;
			float32 gridX = rect.x;
			float32 gridW = rect.w;
			if (showTree) {
				// `tree_width` est une FRACTION, pas une longueur : pas d'echelle.
				const float32 treeW = rect.w * P("tree_width");
				NkPaintRect tree{rect.x, contentTop, treeW, bodyBottom - contentTop};
				if (minimal) {
					// L'ANCIENNE colonne : le fil de dossiers du modele, tel quel.
					p.Fill(tree, s.headerBg);
					float32 ty = tree.y;
					for (uint32 i = 0; i < (uint32)m.breadcrumb.Size(); ++i) {
						const char *t = m.breadcrumb[i].Data();
						if (!t)
							continue;
						p.Icon({tree.x + pad, ty, rowH, rowH}, 1, s.folderTint);
						p.Text({tree.x + pad + rowH, ty, tree.w - pad - rowH, rowH}, t, s.text);
						ty += rowH;
					}
				} else {
					// LE VRAI ARBRE : le composant `tree_view` du kit, sur le
					// modele de dossiers de l'application. Une selection de
					// dossier EST une navigation — le pont traduit.
					TreeBridge bridge;
					bridge.res = &res;
					bridge.hooks = &hooks;
					NkTreeViewStyle ts;
					ts.panelBg = s.headerBg;
					ts.headerBg = s.headerBg;
					ts.border = s.border;
					ts.text = s.text;
					ts.textMuted = s.textMuted;
					ts.rowHover = s.chipBg;
					ts.activeMark = s.activeMark;
					ts.activeText = s.badgeText;
					ts.chosenMark = s.chosenMark;
					ts.guide = s.border;
					ts.dropMark = s.activeMark;
					ts.iconTint = s.folderTint;
					ts.dimTint = s.textMuted;
					ts.icons = s.treeIcons;
					ts.values = &EmbeddedTreeValues();
					NkTreeViewHooks th;
					th.user = &bridge;
					th.onSelect = &TreeOnSelect;
					NkDrawTreeView(p, in, tree, m.folders, ts, th);
				}
				p.VLine(tree.x + tree.w, tree.y, tree.h, s.border);
				gridX = tree.x + tree.w + M("stroke_w");
				gridW = rect.w - treeW - M("stroke_w");
			}

			NkPaintRect area{gridX, contentTop, gridW, bodyBottom - contentTop};
			p.PushClip(area);

			// ── LES ENTREES ─────────────────────────────────────────────────────
			const float32 gap = M("card_gap");
			const float32 footerH = showFooter ? M("footer_h") : 0.f;
			const float32 stroke = M("stroke_w");

			const bool asGrid = minimal || (variant == NkBrowserVariant::Grid);
			// ⚠️ EN BANDE COURTE, LA VIGNETTE CEDE, LE PIED RESTE (mesure du 30/08,
			//    premier consommateur externe) : le pied porte le nom et le type —
			//    c'est LUI l'information, la vignette n'est que l'illustration.
			//    Plancher 16 px : en dessous une vignette ne montre plus rien.
			float32 thumbFit = thumb;
			if (asGrid && showFooter) {
				const float32 place = area.h - footerH - gap;
				if (thumbFit > place)
					thumbFit = place > 16.f ? place : 16.f;
			}
			const float32 cellW = asGrid ? (thumbFit + gap) : area.w;
			const float32 cellH = asGrid ? (thumbFit + footerH + gap) : rowH;
			int32 perRow = asGrid ? (int32)(area.w / (cellW > 0.f ? cellW : 1.f)) : 1;
			if (perRow < 1)
				perRow = 1;

			int32 visible = 0;
			int32 hitIndex = -1;
			for (uint32 vi = 0; vi < (uint32)vis.Size(); ++vi) {
				const int32 idx = vis[vi];
				const NkAssetEntry &e = m.entries[(uint32)idx];

				const int32 col = asGrid ? (visible % perRow) : 0;
				const int32 row = asGrid ? (visible / perRow) : visible;
				NkPaintRect cell{area.x + (float32)col * cellW, area.y + (float32)row * cellH - m.scroll,
								 asGrid ? thumbFit : area.w, asGrid ? (thumbFit + footerH) : rowH};
				++visible;

				// Hors champ : on saute le DESSIN, pas le comptage. Compter apres
				// aurait rendu le defilement dependant de ce qui est visible.
				if (cell.y + cell.h < area.y || cell.y > area.y + area.h)
					continue;

				const bool isActive = (m.active == idx);
				const bool isChosen = m.IsChosen(idx);

				if (asGrid) {
					p.Fill(cell, s.cardBg, pad * 0.5f);
					const float32 thumbZoneH = cell.h - footerH;
					const NkPaintRect zoneVign{cell.x + pad, cell.y + pad, cell.w - pad * 2.f,
											   thumbZoneH - pad * 2.f};
					if (!DrawThumb(p, zoneVign, e.thumbnail))
						p.Icon(zoneVign, e.isFolder ? 1 : 2, e.isFolder ? s.folderTint : e.kindRole);
					// LE BADGE DE TYPE (Aetherion) : la couleur de la nature en
					// fond, pose au bas de la zone de vignette. Pas en minimal, et
					// pas sur un dossier — le dossier EST sa couleur.
					if (!minimal && !e.isFolder && e.kindLabel && e.kindLabel[0]) {
						const float32 bh = M("badge_h");
						const float32 bw = p.TextWidth(e.kindLabel) + pad;
						NkPaintRect badge{cell.x + cell.w - bw - pad * 0.5f,
										  cell.y + thumbZoneH - bh - pad * 0.5f, bw, bh};
						if (badge.x >= cell.x && badge.y >= cell.y) {
							p.Fill(badge, e.kindRole, bh * 0.2f);
							p.Text(badge, e.kindLabel, s.badgeText, NkTextAlign::Center);
						}
					}
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
					const NkPaintRect zoneVign{cell.x + pad, cell.y, rowH, rowH};
					if (!DrawThumb(p, zoneVign, e.thumbnail))
						p.Icon(zoneVign, e.isFolder ? 1 : 2, e.isFolder ? s.folderTint : e.kindRole);
					p.Text({cell.x + pad + rowH, cell.y, cell.w * 0.6f, cell.h}, Label(e), s.text);
					p.Text({cell.x + cell.w * 0.6f, cell.y, cell.w * 0.4f, cell.h},
						   e.kindLabel ? e.kindLabel : "", s.textMuted);
					// Les colonnes greffees par l'application. En variante `Grid`
					// elles n'ont pas de place : c'est dit dans la declaration du
					// point de greffe, pas laisse a deviner.
					if (hooks.extraColumnText && hooks.extraColumnCount > 0) {
						const float32 colW = cell.w * 0.4f / (float32)hooks.extraColumnCount;
						for (int32 c = 0; c < hooks.extraColumnCount; ++c) {
							const char *t = hooks.extraColumnText(hooks.user, idx, c);
							if (t)
								p.Text({cell.x + cell.w * 0.6f + (float32)c * colW, cell.y, colW,
										cell.h},
									   t, s.textMuted);
						}
					}
				}

				// ⚠️ LES DEUX ETATS DE SELECTION, DISTINCTS. L'encodage (aplat contre
				//    contour) est l'ecart n.3, un arbitrage qui appartient a Rodolf.
				if (isChosen)
					p.OutlineSharp({cell.x - stroke, cell.y - stroke, cell.w + stroke * 2.f,
									cell.h + stroke * 2.f},
								   s.chosenMark);
				if (isActive)
					p.Outline(cell, s.activeMark, s.cardBg, pad * 0.5f);

				if (hooks.cardOverlay)
					hooks.cardOverlay(hooks.user, p, idx, cell.x, cell.y, cell.w, cell.h);

				if (cell.Contains(in.mouseX, in.mouseY))
					hitIndex = idx;
			}

			// ── LES EVENEMENTS PARTENT D'ICI, ET DE NULLE PART AILLEURS ─────────
			// Un seul endroit, apres la boucle : sinon un double-clic sur une carte
			// qui en recouvre une autre partirait deux fois. Le composant SIGNALE,
			// il n'agit pas.
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
				// l'annonce.
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

			// ── LA BARRE D'ETAT (apres la grille : sa zone est disjointe, et le
			//    compteur de selection doit refleter CE tour-ci, « Tout
			//    selectionner » compris) ──────────────────────────────────────────
			if (showStatus) {
				NkPaintRect sb{rect.x, bodyBottom, rect.w, statusH};
				p.Fill(sb, s.statusBg);
				p.HLine(rect.x, sb.y, rect.w, s.border);
				if (m.active >= 0 && m.active < (int32)m.entries.Size()) {
					const NkAssetEntry &e = m.entries[(uint32)m.active];
					const char *name = Label(e);
					float32 sx = sb.x + pad;
					const float32 nameW = p.TextWidth(name);
					p.Text({sx, sb.y, nameW, sb.h}, name, s.text);
					sx += nameW;
					char rest[192];
					uint32 at = 0;
					if (e.kindLabel && e.kindLabel[0]) {
						at = PutStr(rest, sizeof(rest), at, "  |  ");
						at = PutStr(rest, sizeof(rest), at, e.kindLabel);
					}
					if (hooks.statusText) {
						const char *extra = hooks.statusText(hooks.user, m.active);
						if (extra && extra[0]) {
							at = PutStr(rest, sizeof(rest), at, " · ");
							at = PutStr(rest, sizeof(rest), at, extra);
						}
					}
					if (at > 0)
						p.Text({sx, sb.y, sb.w * 0.6f - (sx - sb.x), sb.h}, rest, s.textMuted);
				} else {
					p.Text({sb.x + pad, sb.y, sb.w * 0.5f, sb.h}, "Aucune sélection",
						   s.textMuted);
				}
				const char *right = m.statusRight.Data();
				if (right && right[0])
					p.Text({sb.x + sb.w * 0.6f, sb.y, sb.w * 0.4f - pad, sb.h}, right,
						   s.textMuted, NkTextAlign::Right);
			}

			p.PopClip(); // rect
			return res;
		}

	} // namespace editorkit
} // namespace nkentseu
