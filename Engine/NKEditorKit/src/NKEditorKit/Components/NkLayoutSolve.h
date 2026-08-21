#pragma once
// -----------------------------------------------------------------------------
// @File    NkLayoutSolve.h
// @Brief   LA POSITION, CALCULEE : ce que la declaration de taille VEUT DIRE.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  CE FICHIER FAIT PARTIE DU CONTRAT, PAS DU DESSIN -- et c'est la seule chose
//  a comprendre avant d'y toucher
// =============================================================================
//  `NkComponentLayout.h` dit qu'un enfant peut etre « extensible » ou « a
//  poids ». Ce sont des MOTS tant que rien ne dit ce qu'ils produisent. Si
//  chaque composant les interpretait lui-meme, quatre agents ecriraient quatre
//  semantiques, et « la position est un resultat » serait faux : elle serait
//  quatre resultats.
//
//  D'ou la regle : **la forme appartient au gardien, donc sa semantique aussi.**
//  Un composant n'implemente pas la disposition -- il la LIT ici. Ce qui
//  appartient au dessin, c'est ce qu'on peint DANS les rectangles rendus.
//
// =============================================================================
//  CE QUE CE RESOLVEUR COUVRE, ET CE QU'IL NE COUVRE PAS
// =============================================================================
//  IL COUVRE la partie STATIQUE de l'arbre : les sous-elements declares, en
//  ligne, colonne, grille et ancrage, avec fige / contenu / fraction / poids /
//  extensible et leurs bornes.
//
//  ⚠️ IL NE COUVRE PAS la partie REPETEE PAR LES DONNEES -- les cartes d'un
//     navigateur, les lignes d'un arbre. Elles n'existent pas a la declaration :
//     leur NOMBRE vient du modele, et leur INDENTATION vient de la profondeur.
//     C'est le dessin qui les instancie, en lisant les METRIQUES declarees
//     (`card_gap`, `row_h`, `row_indent`). La position reste calculee, jamais
//     ecrite -- mais elle est calculee la-bas, avec la donnee sous la main.
//     **Cette frontiere est ecrite ici plutot que decouverte plus tard** : sans
//     elle, « la position est toujours un resultat de ce fichier » serait une
//     affirmation plus large que la mesure.
//
//  ⚠️ IL NE MESURE PAS LE CONTENU. `NkSizeMode::Content` a besoin de la largeur
//     d'un texte, donc d'une police, donc du peintre -- que ce fichier ne
//     connait pas, et ne doit pas connaitre. L'hote fournit donc les mesures
//     dans `NkContentMeasure` ; a defaut, `Content` vaut `minVal` (souvent 0).
//     **Un `Content` sans mesure fournie ne se plaint pas, il retrecit** : c'est
//     le seul endroit de ce fichier ou un manque est silencieux, et c'est pour
//     ca qu'il est ecrit en gras ici.
//
//  ⚠️ LA REDISTRIBUTION EST ITERATIVE, ET CE POINT A ETE CORRIGE LE JOUR MEME.
//     La premiere ecriture partageait le reste en UNE passe : un enfant a poids
//     ramene par son `minVal`/`maxVal` gardait sa part, sans la rendre aux
//     autres. Un creux apparaissait alors dans le parent, sans que rien
//     l'explique.
//
//     Ce n'est pas une revue qui l'a trouve : c'est le solveur ecrit **en
//     parallele** par l'agent de l'application NkUIDesign, qui le faisait, lui,
//     en boucle. **Deux solveurs qui divergent, c'est deux semantiques pour une
//     meme declaration** -- exactement ce que cette forme existe pour empecher.
//     Le meilleur des deux a ete adopte ici, et c'est ce fichier qui fait foi.
//
//     ⚠️ LA BOUCLE EST BORNEE A HUIT PASSES. Elle converge normalement en deux
//        ou trois (chaque passe fige au moins un enfant) ; la borne existe pour
//        qu'une declaration pathologique ne fasse pas tourner un editeur en
//        rond. Au-dela, le partage s'arrete la ou il en est -- et ne se plaint
//        pas, ce qui est le seul silence assume de ce fichier.
//
// EN-TETE PUR : `NkComponentDecl.h` et rien de plus. Aucun NKGui, aucun peintre,
//   aucune allocation -- l'appelant fournit le tableau de sortie.
// -----------------------------------------------------------------------------

#include "NKEditorKit/Components/NkComponentDecl.h"

namespace nkentseu {
	namespace editorkit {

		// ── D'OU VIENNENT LES NOMBRES ───────────────────────────────────────────
		// Une taille se NOMME (`"row_h"`), et ce nom se resout ou vivent les
		// valeurs. Deux sources existent et ce fichier ne doit connaitre ni l'une ni
		// l'autre : la DECLARATION (les defauts compiles) et l'INSTANCE (le fichier
		// d'ecarts, qui tire `NkVector`/`NkString` derriere elle). D'ou cette
		// indirection de quatre lignes : elle garde le resolveur compilable et
		// executable dans un banc nu, sans rien lier.
		struct NkMetricSource {
				const void *user = nullptr;
				float32 (*get)(const void *user, const char *name, float32 fallback) = nullptr;

				float32 Get(const char *name, float32 fallback = 0.f) const {
					return (get && name && *name) ? get(user, name, fallback) : fallback;
				}
		};

		/// Les defauts compiles d'un composant, vus comme source de metriques.
		/// L'adaptateur pour une `NkComponentInstance` vit dans son propre fichier :
		/// c'est elle qui depend de ce fichier, jamais l'inverse.
		inline NkMetricSource NkMetricsOf(const NkComponentDecl &d) {
			NkMetricSource s;
			s.user = &d;
			s.get = [](const void *u, const char *n, float32 f) -> float32 {
				return ((const NkComponentDecl *)u)->Number(n, f);
			};
			return s;
		}

		/// Mesures de contenu, une par sous-element, dans l'ordre de la table.
		/// `nullptr` = aucune mesure : `Content` retombe alors sur `minVal`.
		struct NkContentMeasure {
				const float32 *w = nullptr;
				const float32 *h = nullptr;
		};

		/// LE RESULTAT. C'est le seul endroit de toute la forme ou un `x` et un `y`
		/// apparaissent -- et ce sont des SORTIES. Si un `x` remonte un jour dans
		/// une declaration, c'est ce fichier qui a echoue a exprimer un besoin.
		struct NkSolvedRect {
				float32 x = 0.f, y = 0.f, w = 0.f, h = 0.f;
		};

		namespace solvedetail {
			inline float32 Clamp(float32 v, const NkSizeDecl &s) {
				if (v < s.minVal)
					v = s.minVal;
				if (s.maxVal > s.minVal && v > s.maxVal)
					v = s.maxVal;
				return v;
			}
			/// Longueur d'un axe pour tout ce qui n'est PAS a poids. Les modes a
			/// poids rendent 0 ici : ils sont servis par le partage du reste.
			inline float32 Axis(const NkSizeDecl &s, const NkMetricSource &m, float32 avail,
								float32 content) {
				float32 v = 0.f;
				switch (s.mode) {
					case NkSizeMode::Fixed:
						v = (s.valueMetric && *s.valueMetric) ? m.Get(s.valueMetric, s.value) : s.value;
						break;
					case NkSizeMode::Content:
						v = content;
						break;
					case NkSizeMode::Fraction: {
						const float32 f =
							(s.valueMetric && *s.valueMetric) ? m.Get(s.valueMetric, s.value) : s.value;
						v = avail * f;
						break;
					}
					default: // Weight, Expand : partages plus bas
						return 0.f;
				}
				return Clamp(v, s);
			}
			/// La valeur touche-t-elle une borne declaree ? C'est ainsi que le
			/// partage sait qu'un enfant est FIGE, sans avoir a le memoriser.
			/// La tolerance est serree : large, elle figerait des enfants qui ne
			/// butent pas, et le partage rendrait moins que la place disponible.
			inline bool AtBound(float32 v, const NkSizeDecl &s) {
				const float32 e = 0.001f;
				if (s.maxVal > s.minVal && v >= s.maxVal - e)
					return true;
				return s.minVal > 0.f && v <= s.minVal + e;
			}
			inline bool IsWeighted(const NkSizeDecl &s) {
				return s.mode == NkSizeMode::Weight || s.mode == NkSizeMode::Expand;
			}
			/// `Expand` EST `Weight(1)` -- un seul mecanisme, deux orthographes.
			inline float32 WeightOf(const NkSizeDecl &s) {
				if (s.mode == NkSizeMode::Expand)
					return 1.f;
				return s.value > 0.f ? s.value : 1.f;
			}
			inline float32 Aligned(float32 origin, float32 extent, float32 size, NkAlign a) {
				switch (a) {
					case NkAlign::Center:
						return origin + (extent - size) * 0.5f;
					case NkAlign::End:
						return origin + extent - size;
					default:
						return origin;
				}
			}
		} // namespace solvedetail

		// ── LE RESOLVEUR ────────────────────────────────────────────────────────
		// L'ORDRE DE LA TABLE FAIT LE TRAVAIL : un parent apparait toujours avant
		// ses enfants (contrainte d'ecriture de `NkComponentLayout.h`, verifiee par
		// `NkCheckComponent`). Donc une seule boucle de haut en bas suffit : quand on
		// arrive a l'element `i`, son rectangle est deja pose par son parent, et on
		// peut poser ceux de ses enfants. Aucune recursion, aucune pile, aucune
		// allocation -- c'est ce que la contrainte d'ecriture achete.
		//
		// Rend le nombre de rectangles ecrits, ou 0 si le tampon est trop petit ou
		// l'arbre vide. Jamais d'ecriture partielle silencieuse.
		inline uint16 NkSolveLayout(const NkComponentDecl &d, const NkMetricSource &m,
									const NkSolvedRect &rootRect, NkSolvedRect *out, uint16 cap,
									const NkContentMeasure *measure = nullptr) {
			if (!out || d.elementCount == 0 || cap < d.elementCount)
				return 0;
			const NkElementDecl *els = d.elements;
			const uint16 n = d.elementCount;

			for (uint16 i = 0; i < n; ++i)
				out[i] = NkSolvedRect{};

			// La racine recoit le rectangle que l'hote lui donne : le composant ne
			// decide pas de sa place, l'hote si. C'est la meme regle qu'a l'echelle
			// du dessin -- meme mecanisme aux deux echelles, jusqu'ici compris.
			const NkElementDecl *root = d.RootElement();
			if (!root)
				return 0;
			out[d.ElementIndex(root->name)] = rootRect;

			auto contentW = [&](uint16 idx) -> float32 {
				return (measure && measure->w) ? measure->w[idx] : els[idx].width.minVal;
			};
			auto contentH = [&](uint16 idx) -> float32 {
				return (measure && measure->h) ? measure->h[idx] : els[idx].height.minVal;
			};

			for (uint16 p = 0; p < n; ++p) {
				const NkLayoutDecl &L = els[p].layout;
				if (L.kind == NkLayoutKind::None)
					continue;

				const float32 pad = m.Get(L.padMetric, 0.f);
				const float32 gap = m.Get(L.spacingMetric, 0.f);
				const float32 bx = out[p].x + pad;
				const float32 by = out[p].y + pad;
				float32 bw = out[p].w - 2.f * pad;
				float32 bh = out[p].h - 2.f * pad;
				if (bw < 0.f)
					bw = 0.f;
				if (bh < 0.f)
					bh = 0.f;

				// Combien d'enfants, et ou ils sont. Deux balayages de la table
				// plutot qu'un tableau temporaire : pas d'allocation, et le second
				// balayage coute ce que coute une dizaine de comparaisons.
				uint16 childCount = 0;
				for (int32 c = d.NextChildOf(els[p].name, 0); c >= 0;
					 c = d.NextChildOf(els[p].name, (uint16)(c + 1)))
					++childCount;
				if (childCount == 0)
					continue;

				if (L.kind == NkLayoutKind::Anchor) {
					// ANCRAGE : chaque enfant est pose independamment, contre les
					// bords qu'il declare. Deux bords opposes = il s'etire entre eux ;
					// un seul = il s'y colle a sa taille ; aucun = il est centre.
					for (int32 c = d.NextChildOf(els[p].name, 0); c >= 0;
						 c = d.NextChildOf(els[p].name, (uint16)(c + 1))) {
						const uint16 k = (uint16)c;
						const uint8 e = els[k].anchorEdges;
						const bool l = (e & nkanchor::Left) != 0, r = (e & nkanchor::Right) != 0;
						const bool t = (e & nkanchor::Top) != 0, b = (e & nkanchor::Bottom) != 0;

						float32 w = (l && r) ? bw : solvedetail::Axis(els[k].width, m, bw, contentW(k));
						float32 h = (t && b) ? bh : solvedetail::Axis(els[k].height, m, bh, contentH(k));
						if (l && r)
							w = solvedetail::Clamp(w, els[k].width);
						if (t && b)
							h = solvedetail::Clamp(h, els[k].height);

						float32 x = bx, y = by;
						if (!l && r)
							x = bx + bw - w;
						else if (!l && !r)
							x = bx + (bw - w) * 0.5f;
						if (!t && b)
							y = by + bh - h;
						else if (!t && !b)
							y = by + (bh - h) * 0.5f;

						out[k] = NkSolvedRect{x, y, w, h};
					}
					continue;
				}

				if (L.kind == NkLayoutKind::Grid) {
					// GRILLE : soit un nombre de colonnes impose, soit un
					// auto-remplissage a partir d'une largeur de cellule NOMMEE.
					// L'auto-remplissage est ce que fait deja la grille de cartes du
					// navigateur de contenu -- il ne s'invente rien ici.
					uint16 cols = L.gridColumns;
					if (cols == 0) {
						const float32 cell = m.Get(L.gridCellMetric, 0.f);
						cols = (cell > 0.f) ? (uint16)((bw + gap) / (cell + gap)) : 1;
						if (cols == 0)
							cols = 1;
					}
					const float32 cw = (bw - gap * (float32)(cols - 1)) / (float32)cols;
					uint16 slot = 0;
					float32 rowY = by, rowH = 0.f;
					for (int32 c = d.NextChildOf(els[p].name, 0); c >= 0;
						 c = d.NextChildOf(els[p].name, (uint16)(c + 1))) {
						const uint16 k = (uint16)c;
						const uint16 col = (uint16)(slot % cols);
						if (col == 0 && slot > 0) {
							rowY += rowH + gap;
							rowH = 0.f;
						}
						float32 h = solvedetail::Axis(els[k].height, m, bh, contentH(k));
						if (solvedetail::IsWeighted(els[k].height) || h <= 0.f)
							h = cw; // cellule carree par defaut : une vignette l'est
						if (h > rowH)
							rowH = h;
						out[k] = NkSolvedRect{bx + (cw + gap) * (float32)col, rowY, cw, h};
						++slot;
					}
					continue;
				}

				// LIGNE ET COLONNE : le meme code, un axe echange. Les ecrire deux
				// fois aurait garanti qu'une correction n'en repare qu'une.
				//
				// TROIS TEMPS, et le deuxieme est le seul qui soit delicat :
				//   A. les tailles NON a poids se calculent et se posent ;
				//   B. le reste se partage entre les poids, EN BOUCLE tant qu'un
				//      partage bute sur une borne ;
				//   C. on parcourt les enfants dans l'ordre et on pose les positions.
				const bool horiz = (L.kind == NkLayoutKind::Row);
				const float32 mainAvail = (horiz ? bw : bh) - gap * (float32)(childCount - 1);
				const float32 crossAvail = horiz ? bh : bw;

				// La taille d'axe principal se range PROVISOIREMENT dans le
				// rectangle de sortie. C'est ce qui evite un tableau temporaire,
				// donc une allocation, donc une taille maximale d'arbre.
				auto mainOf = [&](uint16 k) -> float32 { return horiz ? out[k].w : out[k].h; };
				auto setMain = [&](uint16 k, float32 v) {
					if (horiz)
						out[k].w = v;
					else
						out[k].h = v;
				};

				// ── A. ce qui ne se partage pas ─────────────────────────────
				float32 fixedSum = 0.f;
				bool anyWeighted = false;
				for (int32 c = d.NextChildOf(els[p].name, 0); c >= 0;
					 c = d.NextChildOf(els[p].name, (uint16)(c + 1))) {
					const uint16 k = (uint16)c;
					const NkSizeDecl &sz = horiz ? els[k].width : els[k].height;
					if (solvedetail::IsWeighted(sz)) {
						anyWeighted = true;
						setMain(k, 0.f);
					} else {
						const float32 v =
							solvedetail::Axis(sz, m, mainAvail, horiz ? contentW(k) : contentH(k));
						fixedSum += v;
						setMain(k, v);
					}
				}

				// ── B. le partage, en boucle ────────────────────────────────
				// Un enfant qui a bute sur une borne est FIGE : sa part sort du
				// partage, et ce qu'il n'a pas pris revient aux autres. Sans cette
				// boucle, un creux inexplique apparait dans le parent.
				//
				// L'etat « fige » se DEDUIT de la valeur posee (elle touche une
				// borne) plutot que de se stocker : c'est ce qui garde le resolveur
				// sans memoire de travail. Le cas limite est connu et benin -- un
				// enfant dont la part juste vaut exactement sa borne est fige a
				// cette valeur, c'est-a-dire a la valeur qu'il aurait eue.
				if (anyWeighted) {
					const uint16 kMaxPasses = 8;
					for (uint16 pass = 0; pass < kMaxPasses; ++pass) {
						float32 frozenSum = 0.f, freeWeight = 0.f;
						for (int32 c = d.NextChildOf(els[p].name, 0); c >= 0;
							 c = d.NextChildOf(els[p].name, (uint16)(c + 1))) {
							const uint16 k = (uint16)c;
							const NkSizeDecl &sz = horiz ? els[k].width : els[k].height;
							if (!solvedetail::IsWeighted(sz))
								continue;
							if (pass > 0 && solvedetail::AtBound(mainOf(k), sz))
								frozenSum += mainOf(k);
							else
								freeWeight += solvedetail::WeightOf(sz);
						}
						if (freeWeight <= 0.f)
							break;
						float32 rem = mainAvail - fixedSum - frozenSum;
						if (rem < 0.f)
							rem = 0.f;

						bool changed = false;
						for (int32 c = d.NextChildOf(els[p].name, 0); c >= 0;
							 c = d.NextChildOf(els[p].name, (uint16)(c + 1))) {
							const uint16 k = (uint16)c;
							const NkSizeDecl &sz = horiz ? els[k].width : els[k].height;
							if (!solvedetail::IsWeighted(sz))
								continue;
							if (pass > 0 && solvedetail::AtBound(mainOf(k), sz))
								continue;
							const float32 v =
								solvedetail::Clamp(rem * solvedetail::WeightOf(sz) / freeWeight, sz);
							if (v != mainOf(k))
								changed = true;
							setMain(k, v);
						}
						if (!changed)
							break;
					}
				}

				// ── C. les positions ────────────────────────────────────────
				// Alignement sur l'axe principal : il n'a de sens que s'il RESTE de
				// la place, donc uniquement quand rien n'est a poids -- sinon les
				// poids l'ont deja toute prise, par definition.
				float32 cursor = horiz ? bx : by;
				if (!anyWeighted)
					cursor = solvedetail::Aligned(cursor, horiz ? bw : bh,
												  fixedSum + gap * (float32)(childCount - 1),
												  L.mainAlign);

				for (int32 c = d.NextChildOf(els[p].name, 0); c >= 0;
					 c = d.NextChildOf(els[p].name, (uint16)(c + 1))) {
					const uint16 k = (uint16)c;
					const NkSizeDecl &szCross = horiz ? els[k].height : els[k].width;
					const float32 main = mainOf(k);

					float32 cross = 0.f;
					if (L.crossAlign == NkAlign::Stretch || solvedetail::IsWeighted(szCross))
						cross = solvedetail::Clamp(crossAvail, szCross);
					else
						cross =
							solvedetail::Axis(szCross, m, crossAvail, horiz ? contentH(k) : contentW(k));
					const float32 crossPos = solvedetail::Aligned(horiz ? by : bx, crossAvail, cross,
																 L.crossAlign);

					out[k] = horiz ? NkSolvedRect{cursor, crossPos, main, cross}
								   : NkSolvedRect{crossPos, cursor, cross, main};
					cursor += main + gap;
				}
			}
			return n;
		}

	} // namespace editorkit
} // namespace nkentseu
