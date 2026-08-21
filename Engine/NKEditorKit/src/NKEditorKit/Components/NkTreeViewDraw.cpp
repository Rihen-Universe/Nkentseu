// -----------------------------------------------------------------------------
// @File    NkTreeViewDraw.cpp
// @Brief   Le dessin de l'arbre — et la preuve que la forme tient sur un second
//          composant, d'une autre famille que celui pour lequel elle a ete ecrite.
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
//  Les seules constantes tolerees sont des FRACTIONS et des rapports SANS
//  DIMENSION — `0.5f` pour un centrage, `0.25f` / `0.75f` pour les tiers d'une
//  ligne. Elles ne sont pas des longueurs et ne se theme pas. Un litteral dans
//  une POSITION ou une TAILLE est un defaut, et la revue le voit en une passe.
//
//  ETAT DE L'ART QUE CECI CORRIGE : `NkModelerHierarchy.h` ecrit ses longueurs a
//  travers `S(...)` — un helper d'echelle, pas une source unique : les nombres y
//  sont malgre tout des litteraux disperses (`S(14.f)` pour l'indentation,
//  `S(24.f)` pour le chevron, `S(4.f)`, `S(18.f)`, `S(26.f)`...). Les changer
//  demande de les retrouver ; ici il y en a treize, tous dans `kMetrics`.
//
// =============================================================================
//  L'ORGANISATION, pour qu'on sache ou ajouter la prochaine chose
// =============================================================================
//   1. `ForEachVisibleNode` — LE PARCOURS, ecrit UNE FOIS. Profondeur,
//      pliage, filtre : tout est la. Le dessin et l'application d'une plage
//      Maj+clic l'utilisent tous les deux, donc ils ne peuvent pas diverger.
//   2. `NkDrawTreeView` — mise en page (bandeaux), puis emission des lignes,
//      puis LES EVENEMENTS, tous au meme endroit, apres la boucle.
//
//  ⚠️ POURQUOI LES EVENEMENTS PARTENT APRES LA BOUCLE, ET NULLE PART AILLEURS :
//     deux raisons, et la seconde a ete payee par Nogee. (a) une ligne qui en
//     recouvre une autre ferait partir l'evenement deux fois ; (b) agir pendant
//     le parcours modifie ce qu'on est en train de parcourir —
//     `WorldOutlinerPanel.cpp` a du differer son reparentage pour cette raison
//     exacte, avec dix lignes d'explication. Ici le parcours est un instantane,
//     donc le probleme ne peut pas se poser ; la regle reste, parce que la
//     raison (a) tient toujours.
//
// =============================================================================
//  CE QUE CE FICHIER NE FAIT PAS — nomme, pour que personne ne le cherche
// =============================================================================
//  - **Il ne renomme aucun noeud, ne deplace aucun noeud, ne cache rien.** Il
//    SIGNALE. Les deux seules choses qu'il ecrit sont a lui : l'etat de pliage
//    et la selection (declares `hasDefaultAction = true`).
//  - **Il ne lit pas le clavier** : `NkComponentInput` n'en porte pas. Le
//    renommage en place est donc a deux mains, cf. le bloc CONTOURNEMENT de
//    `NkTreeViewModel.h`.
//  - **Il ne dessine pas de barre de defilement.** La molette defile ; la barre
//    est un composant a part entiere, et la refaire ici en serait une copie de
//    plus (`NkEditorScrollbar.h` existe deja dans ce meme kit).
//  - **Aucun temoin visuel.** Seance sans GPU. Rien de ce fichier n'a ete vu a
//    l'ecran ; ce qui est prouve l'est par `NkTreeViewProbe.h`, et la conformite
//    aux planches reste NON REVENDIQUEE.
// -----------------------------------------------------------------------------

#include "NKEditorKit/Components/NkTreeViewModel.h"

namespace nkentseu {
	namespace editorkit {

		namespace {

			/// Recherche naive, insensible a la casse ASCII. Meme fonction et meme
			/// justification que chez le navigateur de contenu : le filtre est tape a
			/// la main, les listes sont de l'ordre du millier.
			bool Contains(const char *hay, const char *needle) {
				if (!needle || !needle[0])
					return true;
				if (!hay)
					return false;
				for (const char *s = hay; *s; ++s) {
					const char *a = s;
					const char *b = needle;
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

			/// Un libelle jamais nul — un `Text(nullptr)` traverserait tout le peintre
			/// pour n'echouer qu'au rasteriseur, loin de sa cause.
			const char *Label(const NkTreeNode &n) {
				const char *s = n.label.Data();
				return s ? s : "";
			}
			const char *Path(const NkTreeNode &n) {
				const char *s = n.path.Data();
				return s ? s : "";
			}

			/// Ecriture d'un entier sans `snprintf` : ce fichier ne tire aucune
			/// dependance de formatage (zero-STL, et pas de locale). Rend le nombre
			/// d'octets ecrits.
			uint32 AppendU32(char *out, uint32 cap, uint32 pos, uint32 v) {
				char tmp[12];
				int32 n = 0;
				if (v == 0)
					tmp[n++] = '0';
				while (v > 0 && n < 11) {
					tmp[n++] = (char)('0' + (v % 10u));
					v /= 10u;
				}
				for (int32 i = n - 1; i >= 0 && pos + 1 < cap; --i)
					out[pos++] = tmp[i];
				out[pos] = '\0';
				return pos;
			}
			uint32 AppendStr(char *out, uint32 cap, uint32 pos, const char *s) {
				for (; s && *s && pos + 1 < cap; ++s)
					out[pos++] = *s;
				out[pos] = '\0';
				return pos;
			}

			/// Copie bornee, terminee — le renommage recopie un libelle de longueur
			/// inconnue dans un tampon fixe.
			void CopyBounded(char *dst, uint32 cap, const char *src) {
				uint32 i = 0;
				if (src)
					for (; src[i] && i + 1 < cap; ++i)
						dst[i] = src[i];
				dst[i] = '\0';
			}
			bool StrSame(const char *a, const char *b) {
				if (!a || !b)
					return a == b;
				for (; *a && *b; ++a, ++b)
					if (*a != *b)
						return false;
				return *a == *b;
			}

			// ── LE PARCOURS, ECRIT UNE SEULE FOIS ───────────────────────────────
			// C'est LE point sensible de tout ce fichier, et c'est pour ca qu'il est
			// isole : la profondeur, le pliage et le filtre decident ensemble quelles
			// lignes existent. Deux copies de cette logique divergeraient — et la
			// plage Maj+clic selectionnerait alors des lignes que le dessin ne montre
			// pas, ce qui est exactement le genre de defaut qu'on n'attribue jamais a
			// sa cause.
			//
			// ⚠️ ET C'EST AUSSI CE QUI FAIT TENIR LA CONDITION C2 (« la variante est
			//    un parametre ») : la variante n'entre ici que par un booleen `flat`,
			//    qui ne change QUE deux choses — on ignore le pliage, et on
			//    n'indente plus par profondeur. Aucune des deux representations ne
			//    possede un champ de modele, et la selection est ailleurs.
			//
			// `fn(index, depth, hasChildren, isOpen)`.
			//
			// FILTRE : une branche reste visible si ELLE ou l'un de ses DESCENDANTS
			// correspond — lecon de `WorldOutlinerPanel`, sinon filtrer sur le seul
			// parent rend ses enfants inatteignables et la recherche devient un
			// masquage. En variante plate, le filtre s'applique noeud par noeud, ce
			// qui est le comportement attendu d'une liste.
			template <class Fn>
			void ForEachVisibleNode(const NkTreeViewModel &m, bool flat, bool defaultOpen,
									const NkTreeViewHooks &hooks, Fn fn) {
				const uint32 count = (uint32)m.nodes.Size();
				if (count == 0)
					return;

				// La branche courante : indices des ancetres du noeud en cours. En
				// ordre prefixe, elle se maintient en O(1) amorti.
				int32 chain[NkTreeViewModel::kMaxDepth];
				bool chainShows[NkTreeViewModel::kMaxDepth]; // ce niveau laisse-t-il voir ses enfants ?
				int32 sp = 0;

				for (uint32 i = 0; i < count; ++i) {
					const NkTreeNode &n = m.nodes[i];
					const int32 par = n.parent;

					while (sp > 0 && chain[sp - 1] != par)
						--sp;

					const int32 depth = sp;
					// Un ancetre replie cache toute sa descendance. `chainShows` porte
					// deja le ET de tous les niveaux au-dessus, donc un seul test.
					const bool ancestorsShow = flat || sp == 0 || chainShows[sp - 1];

					// EN ORDRE PREFIXE, LE PREMIER ENFANT EST LE NOEUD SUIVANT. C'est
					// la seule raison pour laquelle la precondition d'ordre est exigee
					// (cf. `NkTreeViewModel::IsWellFormed`), et elle rend cette
					// question O(1) au lieu d'un parcours par ligne.
					const bool hasKids = (i + 1u < count) && (m.nodes[i + 1].parent == (int32)i);
					const bool isOpen = flat ? true : m.IsOpen(n.id, defaultOpen);

					bool passes = Contains(Label(n), m.filter);
					if (passes && hooks.acceptNode)
						passes = hooks.acceptNode(hooks.user, n);
					if (!flat && !passes && m.filter[0]) {
						// Un descendant correspond-il ? Les descendants d'un noeud en
						// ordre prefixe sont contigus : ils s'arretent au premier noeud
						// dont le parent est hors de la branche.
						for (uint32 j = i + 1; j < count; ++j) {
							bool inSubtree = false;
							for (int32 a = m.nodes[j].parent; a >= 0; a = m.nodes[a].parent) {
								if (a == (int32)i) {
									inSubtree = true;
									break;
								}
								if (a >= (int32)j)
									break; // donnee incoherente : on n'insiste pas
							}
							if (!inSubtree)
								break;
							if (Contains(Label(m.nodes[j]), m.filter)) {
								passes = true;
								break;
							}
						}
					}

					if (ancestorsShow && passes)
						fn((int32)i, depth, hasKids, isOpen);

					if (sp < NkTreeViewModel::kMaxDepth) {
						chain[sp] = (int32)i;
						chainShows[sp] = ancestorsShow && isOpen;
						++sp;
					}
				}
			}

		} // namespace

		NkTreeViewResult NkDrawTreeView(NkComponentPaint &p, const NkComponentInput &in,
										const NkPaintRect &rect, NkTreeViewModel &m,
										const NkTreeViewStyle &s, const NkTreeViewHooks &hooks) {
			NkTreeViewResult res;
			if (rect.w <= 0.f || rect.h <= 0.f)
				return res;

			// ── LES NOMBRES, TOUS, VIENNENT D'ICI ───────────────────────────────
			// `M` est l'unique porte. Elle lit l'instance si l'application en a
			// branche une, la declaration sinon. C'est la ligne qui porte le temoin :
			// changer `row_h` dans un fichier change ce que cette fonction produit,
			// sans recompiler quoi que ce soit.
			// L'ECHELLE VIENT DE L'ENTREE, PAS DU PEINTRE (arbitrage du 18/08) : elle
			// appartient a la SURFACE, que la disposition lit au meme endroit que le
			// dessin.
			auto M = [&](const char *k) {
				return NkTreeMetric(s, k) * in.surfaceScale;
			};
			auto P = [&](const char *k) {
				return NkTreeParam(s, k);
			};

			const NkTreeVariant variant = NkTreeEffectiveVariant(s);
			const bool flat = (variant == NkTreeVariant::FlatList);
			const bool defaultOpen = P("default_open") > 0.5f;
			const bool showVis = P("show_visibility") > 0.5f;
			const bool showLock = P("show_lock") > 0.5f;
			const bool showType = P("show_type") > 0.5f;
			const bool guides = P("indent_guides") > 0.5f && !flat;
			const bool chevronOnly = P("chevron_only_fold") > 0.5f;
			const bool multiSel = P("multi_select") > 0.5f;
			const bool rangeSel = P("range_select") > 0.5f;
			// LE FAIT, PAS LE GESTE : le role `tree` reclame `onActivate`, mais les
			// trois copies d'arbre de SCENE mesurees RENOMMENT au double-clic au lieu
			// d'activer. Ce reglage porte les deux usages reels plutot que d'imposer
			// l'un des deux -- et il evite de declarer un evenement qui ne partirait
			// jamais, ce qui serait la condition d'echec C5 de ce composant.
			const bool activateOnDouble = P("activate_on_double_click") > 0.5f;

			const float32 rowH = M("row_h");
			const float32 pad = M("row_pad");
			const float32 chevW = M("chevron_w");
			const float32 iconW = M("icon_w");
			const float32 stroke = M("stroke_w");

			p.PushClip(rect);
			p.Fill(rect, s.panelBg);

			// ── BANDEAUX ────────────────────────────────────────────────────────
			float32 top = rect.y;
			if (P("show_header") > 0.5f) {
				const float32 h = M("header_h");
				NkPaintRect bar{rect.x, top, rect.w, h};
				p.Fill(bar, s.headerBg);
				p.Text({bar.x + pad, bar.y, bar.w - pad * 2.f, bar.h}, "Arbre", s.text);
				p.HLine(rect.x, top + h, rect.w, s.border);
				top += h;
			}
			if (P("show_search") > 0.5f) {
				const float32 h = M("search_h");
				NkPaintRect bar{rect.x, top, rect.w, h};
				p.Fill(bar, s.headerBg);
				// Le champ lui-meme est un composant a part (`NkEditorTextField.h`) :
				// on peint sa place et son contenu, on ne le reecrit pas ici.
				p.Text({bar.x + pad, bar.y, bar.w - pad * 2.f, bar.h},
					   m.filter[0] ? m.filter : "Rechercher...", m.filter[0] ? s.text : s.textMuted);
				p.HLine(rect.x, top + h, rect.w, s.border);
				top += h;
			}

			const bool showFooter = P("show_footer") > 0.5f;
			const float32 footerH = showFooter ? M("footer_h") : 0.f;
			NkPaintRect area{rect.x, top, rect.w, rect.y + rect.h - top - footerH};
			if (area.h < 0.f)
				area.h = 0.f;

			// ── LE RENOMMAGE : LA PART DE L'HOTE, TRAITEE AVANT LE DESSIN ───────
			// Traite ici et pas dans la boucle : un noeud en cours de renommage peut
			// avoir ete filtre, replie ou defile hors de vue entre deux images. Si la
			// validation ne survivait qu'a la condition d'etre visible, une saisie
			// validee au clavier serait silencieusement perdue.
			if (m.renaming != 0) {
				const int32 ri = m.IndexOf(m.renaming);
				if (ri < 0) {
					// Le noeud a disparu du modele : on abandonne, sans rien emettre.
					m.renaming = 0;
					m.renameCommit = false;
					m.renameCancel = false;
				} else if (m.renameCancel) {
					m.renaming = 0;
					m.renameCancel = false;
					m.renameCommit = false;
				} else if (m.renameCommit) {
					const NkTreeNode &n = m.nodes[(uint32)ri];
					// Un renommage qui ne change rien N'EST PAS un renommage : le
					// relayer ferait ecrire une commande d'annulation vide dans
					// l'historique de l'application, et l'utilisateur devrait annuler
					// deux fois pour voir un effet.
					if (!StrSame(Label(n), m.renameBuf)) {
						if (hooks.onRename)
							hooks.onRename(hooks.user, ri, Path(n), Label(n), m.renameBuf);
						res.renameCommitted = true;
					}
					m.renaming = 0;
					m.renameCommit = false;
				}
			}

			p.PushClip(area);

			// ── EMISSION DES LIGNES ─────────────────────────────────────────────
			const float32 indentStep = flat ? 0.f : M("indent_step");
			const float32 baseIndent = flat ? M("flat_indent") : 0.f;

			// Ce que la boucle RELEVE ; ce qu'elle DECIDE vient apres elle.
			int32 hitIndex = -1;	  ///< ligne sous la souris
			bool hitChevron = false;  ///< ... sur le chevron
			int32 hitFlag = -1;		  ///< 0 = oeil, 1 = cadenas
			bool hitLabel = false;	  ///< ... sur le libelle (declenche le renommage)
			int32 hitOrdinal = -1;	  ///< rang AFFICHE de la ligne touchee
			int32 anchorOrdinal = -1; ///< rang affiche de l'ancre de plage
			int32 ordinal = 0;
			NkTreeDropPos hitDropPos = NkTreeDropPos::Into;
			const bool dragging = (in.dragType != nullptr);

			ForEachVisibleNode(
				m, flat, defaultOpen, hooks,
				[&](int32 index, int32 depth, bool hasKids, bool isOpen) {
					const NkTreeNode &n = m.nodes[(uint32)index];
					const float32 y = area.y + (float32)ordinal * rowH - m.scroll;
					const int32 myOrdinal = ordinal;
					++ordinal;
					if (n.id == m.anchor)
						anchorOrdinal = myOrdinal;

					// Hors champ : on saute le DESSIN, pas le comptage — compter apres
					// rendrait le defilement dependant de ce qui est visible.
					if (y + rowH < area.y || y > area.y + area.h)
						return;

					const NkPaintRect row{area.x, y, area.w, rowH};
					const bool over = row.Contains(in.mouseX, in.mouseY);
					const bool isActive = (m.active == n.id);
					const bool isChosen = m.IsChosen(n.id);

					// ── FOND ────────────────────────────────────────────────────
					if (isActive)
						p.Fill(row, s.activeMark);
					else if (isChosen)
						p.OutlineSharp({row.x + stroke, row.y + stroke, row.w - stroke * 2.f,
										row.h - stroke * 2.f},
									   s.chosenMark);
					else if (over)
						p.Fill(row, s.rowHover);

					// LA BARRE D'ACCENT A GAUCHE : la planche du 18/08 la montre sur
					// la ligne selectionnee. Elle est peinte APRES le fond pour rester
					// visible quel que soit l'encodage retenu — l'arbitrage entre
					// « aplat pleine largeur » (capture sombre) et « fond teinte +
					// barre » (planche claire) appartient a Rodolf, et il ne change
					// qu'une ligne ici.
					if (isActive)
						p.Fill({row.x, row.y, M("accent_bar_w"), row.h}, s.chosenMark);

					const uint16 labelRole = isActive ? s.activeText : s.text;
					const uint16 mutedRole = isActive ? s.activeText : s.textMuted;

					float32 x = row.x + pad + baseIndent + (float32)depth * indentStep;

					// ── FILETS D'INDENTATION ────────────────────────────────────
					if (guides)
						for (int32 d = 0; d < depth; ++d)
							p.VLine(row.x + pad + (float32)d * indentStep + M("guide_x"), row.y, rowH,
									s.guide);

					// ── CHEVRON ─────────────────────────────────────────────────
					// Zone LARGE et cliquable meme sans enfant ? Non : sans enfant il
					// n'y a rien a plier, et une zone morte qui reagit au survol se
					// lit comme une panne. On reserve la place, on ne dessine rien.
					const NkPaintRect chev{x, row.y, chevW, rowH};
					if (hasKids && !flat) {
						p.Icon(chev, isOpen ? s.icons.chevronOpen : s.icons.chevronClosed, s.iconTint);
						if (over && chev.Contains(in.mouseX, in.mouseY))
							hitChevron = true;
					}
					x += chevW;

					// ── OEIL ET CADENAS ─────────────────────────────────────────
					// L'ICONE MONTRE L'ETAT EFFECTIF, pas le drapeau propre : lecon
					// payee par NK3DModeler (Rihen : « je ne peux selectionner ni le
					// parent ni l'enfant »). Un drapeau HERITE se peint attenue — on
					// voit qu'il vient d'un ancetre et qu'il ne s'ouvre pas ici.
					if (showVis) {
						const NkPaintRect r{x, row.y, iconW, rowH};
						p.Icon(r, n.hidden ? s.icons.eyeClosed : s.icons.eyeOpen,
							   n.flagsInherited ? s.dimTint : s.iconTint);
						if (over && r.Contains(in.mouseX, in.mouseY))
							hitFlag = 0;
						x += iconW;
					}
					if (showLock) {
						const NkPaintRect r{x, row.y, iconW, rowH};
						p.Icon(r, n.locked ? s.icons.lockClosed : s.icons.lockOpen,
							   n.flagsInherited ? s.dimTint : s.iconTint);
						if (over && r.Contains(in.mouseX, in.mouseY))
							hitFlag = 1;
						x += iconW;
					}

					// ── ICONE DE NATURE ─────────────────────────────────────────
					p.Icon({x, row.y, iconW, rowH}, n.icon, n.kindRole ? n.kindRole : s.iconTint);
					x += iconW;

					// ── COLONNES DE DROITE, RESERVEES AVANT LE LIBELLE ──────────
					// Le libelle prend ce qui reste : c'est lui qui doit s'ellipser,
					// jamais une colonne de largeur connue.
					float32 rightEdge = row.x + row.w - pad;
					const int32 extraCols = hooks.extraColumnText ? hooks.extraColumnCount : 0;
					if (extraCols > 0) {
						const float32 colW = iconW * 2.f;
						for (int32 c = extraCols - 1; c >= 0; --c) {
							rightEdge -= colW;
							const char *t = hooks.extraColumnText(hooks.user, index, c);
							if (t)
								p.Text({rightEdge, row.y, colW, rowH}, t, mutedRole);
						}
					}
					if (showType && n.kindLabel && n.kindLabel[0]) {
						const float32 tw = p.TextWidth(n.kindLabel);
						rightEdge -= tw + pad;
						p.Text({rightEdge, row.y, tw, rowH}, n.kindLabel, mutedRole);
					}

					// ── LIBELLE, OU BOITE DE SAISIE ─────────────────────────────
					const NkPaintRect labelRect{x, row.y, rightEdge - x, rowH};
					if (m.renaming == n.id) {
						// Saisie EN PLACE. Le tampon appartient au modele et l'hote y
						// ecrit — cf. le bloc CONTOURNEMENT de `NkTreeViewModel.h`.
						p.Fill(labelRect, s.rowHover);
						p.OutlineSharp(labelRect, s.activeMark);
						p.Text({labelRect.x + pad, labelRect.y, labelRect.w - pad * 2.f, labelRect.h},
							   m.renameBuf, s.text);
						// Curseur : un trait a la fin du texte saisi. Sa position
						// depend d'une largeur de texte, donc elle sera fausse sur le
						// peintre enregistreur (metriques fictives, assume) et juste a
						// l'ecran.
						p.VLine(labelRect.x + pad + p.TextWidth(m.renameBuf), labelRect.y, rowH,
								s.text);
					} else {
						p.Text(labelRect, Label(n), labelRole);
						if (over && labelRect.Contains(in.mouseX, in.mouseY))
							hitLabel = true;
						// EN LISTE PLATE, LE CHEMIN SUIT LE LIBELLE. Sans hierarchie
						// visible, deux noeuds de meme nom deviennent indistinguables
						// — c'est le defaut principal d'une liste plate, et il coute
						// une ligne a corriger.
						if (flat && Path(n)[0]) {
							const float32 lw = p.TextWidth(Label(n));
							p.Text({labelRect.x + lw + pad, labelRect.y, labelRect.w - lw - pad,
									labelRect.h},
								   Path(n), mutedRole);
						}
					}

					if (hooks.rowOverlay)
						hooks.rowOverlay(hooks.user, p, index, row.x, row.y, row.w, row.h);

					// ── CIBLE DE DEPOT : TROIS ZONES ────────────────────────────
					// Tiers haut / tiers bas = inserer avant / apres (reordonner),
					// milieu = devenir enfant (reparenter). Aucune des trois copies
					// existantes ne sait reordonner ; les fractions ci-dessous sont
					// sans dimension, donc elles ne sont pas des longueurs.
					if (over && dragging) {
						const float32 rel = (in.mouseY - row.y) / (rowH > 0.f ? rowH : 1.f);
						NkTreeDropPos pos = NkTreeDropPos::Into;
						if (rel < 0.25f)
							pos = NkTreeDropPos::Before;
						else if (rel > 0.75f)
							pos = NkTreeDropPos::After;
						hitDropPos = pos;
						const float32 lineH = M("drop_line_h");
						if (pos == NkTreeDropPos::Into)
							p.OutlineSharp(row, s.dropMark);
						else
							p.Fill({row.x, pos == NkTreeDropPos::Before ? row.y : row.y + rowH - lineH,
									row.w, lineH},
								   s.dropMark);
					}

					if (over) {
						hitIndex = index;
						hitOrdinal = myOrdinal;
					}
				});

			res.visibleCount = ordinal;

			// ── LES DECISIONS, TOUTES ICI ───────────────────────────────────────
			// Un seul bloc, apres le parcours. C'est aussi ce qui fait tenir la
			// condition C2 : la logique de selection existe A UN SEUL ENDROIT, et
			// aucune des deux variantes n'en a une a elle.
			const bool inArea = area.Contains(in.mouseX, in.mouseY);

			if (hitIndex >= 0 && inArea) {
				NkTreeNode &n = m.nodes[(uint32)hitIndex];
				const char *path = Path(n);

				// 1. LE CHEVRON — la seule commande de pliage quand
				//    `chevron_only_fold` est vrai. Lecon de NK3DModeler : le clic de
				//    ligne pliait aussi, « trop sensible et genant pour renommer »
				//    (Rihen). Le clic qui plie ne selectionne pas.
				const bool foldClick =
					in.mousePressed && (hitChevron || (!chevronOnly && !hitLabel && hitFlag < 0));
				if (foldClick) {
					const bool nowOpen = !m.IsOpen(n.id, defaultOpen);
					m.SetOpen(n.id, nowOpen, defaultOpen);
					res.openChanged = true;
					if (hooks.onExpand)
						hooks.onExpand(hooks.user, hitIndex, path, nowOpen);
				}

				// 2. LES DRAPEAUX — un drapeau HERITE ne se libere pas ici. Le refus
				//    est SIGNALE plutot que silencieux : un clic sans effet passe pour
				//    une panne, et ce depot a deja paye une seance sur ce motif.
				if (in.mousePressed && hitFlag >= 0) {
					if (!n.flagsInherited && hooks.onToggleFlag) {
						const bool value = (hitFlag == 0) ? !n.hidden : !n.locked;
						hooks.onToggleFlag(hooks.user, hitIndex, path,
										   (uint8)(hitFlag == 0 ? NkTreeFlag::Visible
																: NkTreeFlag::Locked),
										   value);
					}
				}

				// 3. LA SELECTION — un noeud VERROUILLE ne se selectionne jamais.
				if (in.mousePressed && !hitChevron && hitFlag < 0 && !n.locked) {
					if (rangeSel && in.shift && m.anchor != 0 && m.anchor != n.id &&
						anchorOrdinal >= 0 && hitOrdinal >= 0) {
						// PLAGE MAJ+CLIC, additive facon Blender : elle ETEND la
						// selection sans rien deselectionner. Venue de NK3DModeler,
						// absente des deux copies Nogee. On rejoue le MEME parcours
						// que le dessin — c'est pour ca qu'il est ecrit une seule
						// fois.
						const int32 lo = anchorOrdinal < hitOrdinal ? anchorOrdinal : hitOrdinal;
						const int32 hi = anchorOrdinal < hitOrdinal ? hitOrdinal : anchorOrdinal;
						int32 k = 0;
						ForEachVisibleNode(m, flat, defaultOpen, hooks,
										   [&](int32 idx, int32, bool, bool) {
											   const int32 o = k++;
											   if (o < lo || o > hi)
												   return;
											   const NkTreeNode &q = m.nodes[(uint32)idx];
											   if (q.locked || m.IsChosen(q.id))
												   return;
											   m.chosen.PushBack(q.id);
										   });
						m.active = n.id;
					} else if (multiSel && in.ctrl) {
						bool removed = false;
						for (uint32 c = 0; c < (uint32)m.chosen.Size(); ++c)
							if (m.chosen[c] == n.id) {
								m.chosen.RemoveAt(c);
								removed = true;
								break;
							}
						if (!removed)
							m.chosen.PushBack(n.id);
						m.active = removed ? 0 : n.id;
						m.anchor = n.id;
					} else {
												m.chosen.Clear();
						m.chosen.PushBack(n.id);
						m.active = n.id;
						m.anchor = n.id;
					}
					res.selectionChanged = true;
					m.dragSource = n.id; // source POSSIBLE d'un glisser qui commence
					// ⚠️ LA CHARGE EST CELLE DU ROLE `tree` : (index, id). Elle ne dit
					//    donc PAS ce qui est selectionne quand il y en a plusieurs -- le
					//    vocabulaire `.nkgui` n'a aucun type liste. Le compte reste
					//    disponible en C++ (`m.chosen.Size()`), jamais a un blueprint.
					if (hooks.onSelect)
						hooks.onSelect(hooks.user, hitIndex, path);
				}

				// 3bis. LE DOUBLE-CLIC -- deux usages reels, donc un reglage.
				// Arbre de SCENE (les trois copies mesurees) : il arme le RENOMMAGE.
				// Arbre de FICHIERS (`NKCode/Shell/NkExplorer.h`) : il ACTIVE.
				// Le composant ne renomme pas lui-meme : il ouvre la saisie, l'hote
				// ecrit dedans (il a le clavier), et `onRename` part au commit.
				if (in.doubleClick && hitLabel && !n.locked) {
					if (activateOnDouble) {
						if (hooks.onActivate)
							hooks.onActivate(hooks.user, hitIndex, path);
					} else if (m.renaming == 0) {
						m.renaming = n.id;
						CopyBounded(m.renameBuf, (uint32)sizeof(m.renameBuf), Label(n));
						res.renameStarted = true;
					}
				}

				// 4. LE MENU CONTEXTUEL — toute la largeur de la ligne, chevron et
				//    icones compris : les zones fines volaient le clic (NK3DModeler).
				if (in.rightPressed && hooks.onContextMenu)
					hooks.onContextMenu(hooks.user, hitIndex, in.mouseX, in.mouseY);

				// 5. LE DEPOT ────────────────────────────────────────────────────
				if (in.dragReleased) {
					const int32 srcIdx = m.IndexOf(m.dragSource);
					bool ok = true;
					// GARDE ANTI-CYCLE, GENERIQUE ET GRATUITE : le composant a la
					// chaine de parents sous les yeux. Nogee a du l'ecrire a la main
					// parce que `SetParent` n'a aucune garde interne ; NK3DModeler ne
					// l'a pas du tout. Ici elle est rendue une fois pour tous.
					if (srcIdx >= 0) {
						if (srcIdx == hitIndex)
							ok = false;
						int32 walk = (hitDropPos == NkTreeDropPos::Into) ? hitIndex
																		 : m.nodes[(uint32)hitIndex].parent;
						for (int32 d = 0; ok && walk >= 0 && d < NkTreeViewModel::kMaxDepth; ++d) {
							if (walk == srcIdx) {
								ok = false;
								break;
							}
							walk = m.nodes[(uint32)walk].parent;
						}
						if (!ok)
							res.dropRefusedCycle = true;
					}
					// LA REGLE PROPRE A L'APPLICATION, par-dessus la garde generique.
					if (ok && srcIdx >= 0 && hooks.acceptDrop &&
						!hooks.acceptDrop(hooks.user, m.nodes[(uint32)srcIdx], n, hitDropPos))
						ok = false;

					if (ok) {
						res.dropAccepted = true;
						res.dropSource = m.dragSource;
						res.dropTarget = n.id;
						res.dropPos = hitDropPos;
						if (hooks.onDrop)
							hooks.onDrop(hooks.user, srcIdx >= 0 ? Path(m.nodes[(uint32)srcIdx]) : "",
										 path, (uint8)hitDropPos, in.dragType ? in.dragType : "");
					}
					m.dragSource = 0;
				}
			} else if (inArea) {
				// ── LE VIDE DE LA LISTE ─────────────────────────────────────────
				// Clic droit : menu du vide, `node` vide, tel que la declaration
				// l'annonce. Sans ce cas, l'application ne pourrait offrir ni
				// « Ajouter » ni « Coller » hors d'une ligne.
				if (in.rightPressed && hooks.onContextMenu)
					hooks.onContextMenu(hooks.user, -1, in.mouseX, in.mouseY);
				// Clic nu : deselectionner.
				if (in.mousePressed && !in.ctrl && !in.shift) {
					m.ClearSelection();
					res.selectionChanged = true;
					if (hooks.onSelect)
						hooks.onSelect(hooks.user, -1, "");
				}
				// LACHER DANS LE VIDE = DEPARENTER (NK3DModeler). La cible est la
				// racine, donc un chemin vide et une position `Into`.
				if (in.dragReleased && m.dragSource != 0) {
					res.dropAccepted = true;
					res.dropSource = m.dragSource;
					res.dropTarget = 0;
					res.dropPos = NkTreeDropPos::Into;
					const int32 srcIdx = m.IndexOf(m.dragSource);
					if (hooks.onDrop)
						hooks.onDrop(hooks.user, srcIdx >= 0 ? Path(m.nodes[(uint32)srcIdx]) : "", "",
									 (uint8)NkTreeDropPos::Into, in.dragType ? in.dragType : "");
					m.dragSource = 0;
				}
			}

			// ── DEFILEMENT ──────────────────────────────────────────────────────
			if (in.wheel != 0.f && inArea) {
				m.scroll -= in.wheel * rowH;
				const float32 contentH = (float32)res.visibleCount * rowH;
				const float32 maxScroll = contentH > area.h ? contentH - area.h : 0.f;
				if (m.scroll < 0.f)
					m.scroll = 0.f;
				if (m.scroll > maxScroll)
					m.scroll = maxScroll;
			}

			p.PopClip(); // area

			// ── PIED ────────────────────────────────────────────────────────────
			// « 7 acteurs · 1 selectionne » sur la planche du 18/08. Le compte est
			// celui des lignes REELLEMENT emises, filtre compris : afficher le total
			// du modele pendant qu'une recherche masque la moitie de l'arbre ferait
			// douter du filtre, pas du compteur.
			if (showFooter) {
				NkPaintRect foot{rect.x, rect.y + rect.h - footerH, rect.w, footerH};
				p.Fill(foot, s.headerBg);
				p.HLine(foot.x, foot.y, foot.w, s.border);
				char line[96];
				uint32 n = AppendU32(line, (uint32)sizeof(line), 0, (uint32)res.visibleCount);
				n = AppendStr(line, (uint32)sizeof(line), n, " noeud(s), ");
				n = AppendU32(line, (uint32)sizeof(line), n, (uint32)m.chosen.Size());
				AppendStr(line, (uint32)sizeof(line), n, " selectionne(s)");
				p.Text({foot.x + pad, foot.y, foot.w - pad * 2.f, foot.h}, line, s.textMuted);
			}

			p.PopClip(); // rect
			return res;
		}

	} // namespace editorkit
} // namespace nkentseu
