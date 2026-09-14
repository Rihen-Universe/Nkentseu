// -----------------------------------------------------------------------------
// @File    NkTabStripDraw.cpp
// @Brief   Le dessin de la bande d'onglets — LE code, celui que le modeleur et
//          la coquille appellent tous les deux.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  LA REGLE QUI GOUVERNE CE FICHIER, ET ELLE EST MECANIQUE
// =============================================================================
//  ⚠️ **PAS UN SEUL NOMBRE DE PIXELS N'EST ECRIT ICI.** Toute longueur passe par
//     `M("...")`. Toute couleur passe par un role du style. Les seules
//     constantes tolerees sont des FRACTIONS sans dimension (`0.5f` pour un
//     centrage). C'est la meme regle que `NkTreeViewDraw.cpp`, et c'est elle qui
//     rend la MUTATION possible : changer `tab_pad_x` dans `NkTabStripModel.h`
//     doit changer la bande des DEUX hotes. Si un seul bouge, le partage est une
//     fiction — et c'est exactement le temoin que le canal exige.
//
//  ETAT DE L'ART QUE CECI CORRIGE : `PaintTabsI` (NK3DModeler) ecrit `S(10.f)`,
//  `S(44.f)`, `S(24.f)`, `S(20.f)`, `S(7.f)`, `3.f`, `2.f`, `S(5.f)`, `S(6.f)`,
//  `S(8.f)`, `S(4.f)`, `S(32.f)`, `10.f`, `12.f` — quatorze litteraux disperses
//  dans 130 lignes. Ici il y en a vingt-et-un, tous dans `kMetrics`.
//
// =============================================================================
//  L'ORGANISATION
// =============================================================================
//   1. `Mesurer` — LA GEOMETRIE, ecrite UNE FOIS. Le dessin et le test de clic
//      la lisent tous les deux, donc ils ne peuvent pas diverger. C'est la
//      reponse au defaut du modeleur ou « la zone du nom recouvrait celle de
//      l'onglet et lui VOLAIT le survol » (Rihen) : deux calculs, deux verites.
//   2. `NkDrawTabStrip` — la peinture, puis LES EVENEMENTS, apres la boucle.
//
//  ⚠️ POURQUOI LES EVENEMENTS PARTENT APRES LA BOUCLE : le modeleur fait
//     `NkCloseSceneTab(st, i); break;` AU MILIEU de son parcours, avec le
//     commentaire « la liste a change ». Il a raison de sortir — mais il ne
//     peint alors PAS les onglets suivants, qui clignotent une image sur deux
//     quand on ferme. Ici le composant ne ferme rien : il RAPPORTE, l'hote
//     agit apres. Le clignotement devient impossible au lieu d'etre evite.
// -----------------------------------------------------------------------------

#include "NKEditorKit/Components/NkTabStripModel.h"

namespace nkentseu {
	namespace editorkit {

		namespace {

			/// Un libelle jamais nul — un `Text(nullptr)` traverserait tout le
			/// peintre pour n'echouer qu'au rasteriseur, loin de sa cause.
			const char *Label(const NkTabItem &t) {
				const char *s = t.label.Data();
				return s ? s : "";
			}

			/// La CROIX, TRACEE. ⚠️ Le kit n'a aucun atlas : « tout ce qui doit se
			/// voir se TRACE ». Un `Icon()` ici ne peindrait qu'un carre chez
			/// l'hote qui n'a pas la poignee — defaut deja mesure dans ce depot.
			/// Rend faux si le peintre ne sait pas tracer de segment : l'appelant
			/// peint alors un repli VISIBLE plutot qu'un vide silencieux.
			bool Croix(NkComponentPaint &p, const NkPaintRect &zone, float32 taille, uint16 role,
					   float32 epaisseur) {
				const float32 cx = zone.x + zone.w * 0.5f;
				const float32 cy = zone.y + zone.h * 0.5f;
				const float32 d = taille * 0.5f;
				const bool a = p.Line(cx - d, cy - d, cx + d, cy + d, role, epaisseur);
				const bool b = p.Line(cx - d, cy + d, cx + d, cy - d, role, epaisseur);
				return a && b;
			}

			/// Le `+`, TRACE. Meme raison que la croix.
			bool Plus(NkComponentPaint &p, const NkPaintRect &zone, float32 taille, uint16 role,
					  float32 epaisseur) {
				const float32 cx = zone.x + zone.w * 0.5f;
				const float32 cy = zone.y + zone.h * 0.5f;
				const float32 d = taille * 0.5f;
				const bool a = p.Line(cx - d, cy, cx + d, cy, role, epaisseur);
				const bool b = p.Line(cx, cy - d, cx, cy + d, role, epaisseur);
				return a && b;
			}

			/// Un repli VISIBLE quand le peintre ne sait pas tracer : un petit
			/// rectangle plein. ⚠️ JAMAIS un repli muet — la regle du depot est
			/// « texture de repli contre refus nomme, jamais un repli muet ».
			void Repli(NkComponentPaint &p, const NkPaintRect &zone, float32 taille, uint16 role) {
				const float32 d = taille * 0.5f;
				p.Fill({zone.x + (zone.w - d) * 0.5f, zone.y + (zone.h - d) * 0.5f, d, d}, role);
			}

		} // namespace

		NkTabStripResult NkDrawTabStrip(NkComponentPaint &p, const NkComponentInput &in,
										const NkPaintRect &rect, NkTabStripModel &m,
										const NkTabStripStyle &s, const NkTabStripHooks &hooks) {
			NkTabStripResult res;
			if (rect.w <= 0.f || rect.h <= 0.f)
				return res;

			// ── LES NOMBRES, TOUS, VIENNENT D'ICI ───────────────────────────────
			// L'ECHELLE VIENT DE L'ENTREE, PAS DU PEINTRE (arbitrage du 18/08) :
			// elle appartient a la SURFACE, que la disposition lit au meme endroit
			// que le dessin.
			auto M = [&](const char *k) {
				return NkTabMetric(s, k) * in.surfaceScale;
			};
			auto P = [&](const char *k) {
				return NkTabParam(s, k);
			};

			const NkTabVariant variant = NkTabEffectiveVariant(s);
			const bool segmente = (variant == NkTabVariant::Segmente);

			// ⚠️ LA VARIANTE SEGMENTEE N'A NI CROIX NI `+`, ET CE N'EST PAS UN
			//    REGLAGE QU'ELLE ECRASE : ce sont des onglets de VUE (NkAiPanel),
			//    on ne « ferme » pas une vue et on n'en « ajoute » pas une. Les
			//    parametres restent vrais pour la variante Documents.
			const bool showAdd = !segmente && P("show_add_button") > 0.5f;
			const bool showClose = !segmente && P("show_close") > 0.5f;
			const bool showDot = !segmente && P("show_modified_dot") > 0.5f;
			const bool showSep = !segmente && P("show_separators") > 0.5f;
			const bool accentBar = P("active_accent_bar") > 0.5f;
			const bool accentBottom = P("accent_bar_bottom") > 0.5f;
			const bool kindTop = P("kind_bar_top") > 0.5f;

			const float32 padX = M("band_pad_x");
			const float32 insetY = M("tab_inset_y");
			const float32 tabPad = M("tab_pad_x");
			const float32 labPad = M("label_pad_x");
			const float32 labRes = M("label_reserve");
			const float32 gap = M("tab_gap");
			const float32 sepGap = M("sep_gap");
			const float32 sepInset = M("sep_inset_y");
			const float32 round = M("tab_rounding");
			const float32 kindH = M("kind_bar_h");
			const float32 accH = M("accent_bar_h");
			const float32 closeW = M("close_w");
			const float32 closeR = M("close_right");
			const float32 closeMark = M("close_mark");
			const float32 dotD = M("dot_d");
			const float32 addW = M("add_w");
			const float32 addPad = M("add_pad");
			const float32 addMark = M("add_mark");
			const float32 stroke = M("stroke_w");
			const float32 segMin = M("seg_min_w");

			const int32 n = (int32)m.tabs.Size();

			p.PushClip(rect);
			p.Fill(rect, s.bandBg);
			// Le trait BAS de la bande : il la separe de ce qui suit. Chez le
			// modeleur c'est `p.HLine(r.x, r.y + r.h - 1.f, r.w)`.
			p.HLine(rect.x, rect.y + rect.h - stroke, rect.w, s.border);

			const float32 tabH = rect.h - insetY * 2.f;
			const float32 tabY = rect.y + insetY;

			// ── 1. LA GEOMETRIE, CALCULEE UNE SEULE FOIS ────────────────────────
			// Elle sert au dessin ET au test de clic. Deux calculs separes, c'est
			// le defaut « la zone du nom volait le survol de l'onglet » du modeleur.
			float32 x = rect.x + padX;
			// La variante segmentee partage la largeur en parts egales, bornee.
			float32 segW = 0.f;
			if (segmente && n > 0) {
				segW = (rect.w - padX * 2.f - gap * (float32)(n - 1)) / (float32)n;
				if (segW < segMin)
					segW = segMin;
			}
			for (int32 i = 0; i < n; ++i) {
				const NkTabItem &t = m.tabs[(usize)i];
				NkTabRect g;
				g.id = t.id;
				g.w = segmente ? segW : (p.TextWidth(Label(t)) + tabPad);
				g.x = x;
				g.y = tabY;
				g.h = tabH;
				g.labelX = x + labPad;
				// Le libelle CEDE devant la reserve de droite, jamais l'inverse :
				// c'est lui qui s'ellipse (obligation du peintre).
				g.labelW = g.w - labRes;
				if (g.labelW < 0.f)
					g.labelW = 0.f;
				if (segmente) {
					g.labelX = x;
					g.labelW = g.w;
				}
				res.tabs.PushBack(g);
				x += g.w + gap;
				if (showSep && i + 1 < n)
					x += sepGap;
			}
			const float32 xApresOnglets = x;
			NkPaintRect addR{xApresOnglets + addPad, tabY, addW, tabH};
			res.usedW = (showAdd ? (addR.x + addR.w) : xApresOnglets) - rect.x;
			res.overflow = res.usedW > rect.w;

			// ── 2. LA PEINTURE ──────────────────────────────────────────────────
			// Les gestes sont RELEVES pendant la boucle et EMIS apres : agir
			// pendant, c'est modifier la liste qu'on parcourt.
			nk_uint64 vouluActif = 0;
			nk_uint64 vouluFerme = 0;
			nk_uint64 vouluContexte = 0;
			bool contexteSurOnglet = false;
			bool vouluAjout = false;

			for (int32 i = 0; i < n; ++i) {
				const NkTabItem &t = m.tabs[(usize)i];
				const NkTabRect &g = res.tabs[(usize)i];
				const NkPaintRect tr{g.x, g.y, g.w, g.h};

				const bool actif = (t.id != 0 && t.id == m.active);
				const bool survol = tr.Contains(in.mouseX, in.mouseY);
				if (survol)
					res.hoveredId = t.id;

				p.Fill(tr, actif ? s.tabActiveBg : (survol ? s.tabHoverBg : s.tabBg), round);

				// ── LE LISERET DE NATURE ────────────────────────────────────────
				// Distinguer d'un oeil un onglet d'EDITEUR d'une scene. La couleur
				// vient de l'HOTE (`kindRole`) : le kit ne connait pas les natures
				// d'asset d'une application, et c'est ce qui le rend partageable.
				if (t.kindRole != 0)
					p.Fill({tr.x, kindTop ? tr.y : (tr.y + tr.h - kindH), tr.w, kindH}, t.kindRole);

				// ── LE SECOND SIGNAL DE L'ACTIF (§6) ────────────────────────────
				if (actif && accentBar)
					p.Fill({tr.x, accentBottom ? (tr.y + tr.h - accH) : tr.y, tr.w, accH}, s.accent);

				// ── LE LIBELLE ──────────────────────────────────────────────────
				// ⚠️ IL EST PEINT MEME QUAND L'HOTE RENOMME. Le composant n'a pas
				//    le clavier ; c'est l'hote qui, s'il ouvre un editeur sur
				//    `labelX/labelW`, le peint PAR-DESSUS. Sauter le libelle ici
				//    demanderait au composant de savoir qu'une saisie est en cours —
				//    un etat qu'il ne possede pas et qu'il ne doit pas deviner.
				p.Text({g.labelX, tr.y, g.labelW, tr.h}, Label(t), actif ? s.text : s.textMuted,
					   segmente ? NkTextAlign::Center : NkTextAlign::Left);

				// ── PASTILLE ET CROIX : MEME EMPLACEMENT, CONDITIONS DISTINCTES ──
				// ⚠️ C'est la correction d'un defaut MESURE chez le modeleur, et il
				//    vaut d'etre redit : la pastille « non enregistre » vaut pour
				//    TOUT onglet, la croix seulement pour un onglet fermable. Les
				//    imbriquer — la pastille SOUS la condition de la croix — faisait
				//    qu'avec un seul document, le cas le plus courant, aucune
				//    modification n'etait jamais signalee (constate par Rihen).
				//    Ici les deux `if` sont freres, jamais l'un dans l'autre.
				if (!segmente) {
					const NkPaintRect cr{tr.x + tr.w - closeR, tr.y, closeW, tr.h};
					const bool surCroix = cr.Contains(in.mouseX, in.mouseY);
					const bool croixVisible = showClose && t.closable && surCroix;
					if (showDot && t.modified && !croixVisible) {
						// MEME emplacement que la croix : la largeur de l'onglet ne
						// bouge donc pas quand un document devient modifie.
						const NkPaintRect dr{cr.x + (cr.w - dotD) * 0.5f, cr.y + (cr.h - dotD) * 0.5f,
											 dotD, dotD};
						if (!p.Ellipse(dr, actif ? s.text : s.textMuted))
							p.Fill(dr, actif ? s.text : s.textMuted, dotD * 0.5f);
					} else if (showClose && t.closable) {
						if (surCroix)
							p.Fill(cr, s.tabHoverBg, round);
						if (!Croix(p, cr, closeMark, s.textMuted, stroke))
							Repli(p, cr, closeMark, s.textMuted);
					}
					if (surCroix && showClose && t.closable && in.mousePressed)
						vouluFerme = t.id;
					else if (survol && in.mousePressed)
						vouluActif = t.id;
				} else if (survol && in.mousePressed) {
					vouluActif = t.id;
				}

				if (survol && in.rightPressed) {
					vouluContexte = t.id;
					contexteSurOnglet = true;
				}

				if (hooks.tabOverlay)
					hooks.tabOverlay(hooks.user, p, i, tr.x, tr.y, tr.w, tr.h);

				// ── LE FILET DE SEPARATION ──────────────────────────────────────
				// Regle de Rihen : sans lui, deux onglets cote a cote se confondent.
				if (showSep && i + 1 < n)
					p.VLine(tr.x + tr.w + gap, tr.y + sepInset, tr.h - sepInset * 2.f, s.border);
			}

			// ── LE BOUTON + ─────────────────────────────────────────────────────
			if (showAdd) {
				const bool surAdd = addR.Contains(in.mouseX, in.mouseY);
				if (surAdd)
					p.Fill(addR, s.tabHoverBg, round);
				if (!Plus(p, addR, addMark, s.text, stroke))
					Repli(p, addR, addMark, s.text);
				if (surAdd && in.mousePressed)
					vouluAjout = true;
			}

			// ── 3. LES EVENEMENTS, TOUS ICI, APRES LA BOUCLE ────────────────────
			if (vouluFerme != 0) {
				res.closeRequested = true;
				res.closeId = vouluFerme;
			} else if (vouluActif != 0 && vouluActif != m.active) {
				m.active = vouluActif; // `hasDefaultAction = true` : deja a jour
				res.selectionChanged = true;
			}
			if (vouluAjout)
				res.addRequested = true;
			if (in.rightPressed && rect.Contains(in.mouseX, in.mouseY)) {
				res.contextMenu = true;
				res.contextId = contexteSurOnglet ? vouluContexte : 0;
			}
			// L'infobulle est RELEVEE, pas peinte : seul l'hote sait ou est le bord
			// de la fenetre et quelle couche est au-dessus (meme regle que l'arbre).
			if (res.hoveredId != 0) {
				for (int32 i = 0; i < n; ++i)
					if (m.tabs[(usize)i].id == res.hoveredId) {
						res.infobulle = m.tabs[(usize)i].infobulle;
						break;
					}
			}

			p.PopClip();
			return res;
		}

	} // namespace editorkit
} // namespace nkentseu
