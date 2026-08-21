#pragma once
// -----------------------------------------------------------------------------
// @File    Layout.h
// @Brief   La position d'un DOCUMENT, calculee — avec la semantique du kit.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  ⚠️ CE FICHIER N'INVENTE AUCUNE SEMANTIQUE, ET C'EST SA SEULE REGLE
// =============================================================================
//  Ce que « extensible », « a poids », « fraction », « min », « max » et
//  « aligne » VEULENT DIRE est fixe une fois pour toutes dans
//  `NKEditorKit/Components/NkLayoutSolve.h`, chez le gardien de la forme. Ce
//  fichier appelle ses fonctions (`solvedetail::Axis`, `Clamp`, `IsWeighted`,
//  `WeightOf`, `Aligned`) — il ne les recopie pas.
//
//  La raison est ecrite dans le fichier du kit et vaut d'etre repetee : si
//  chaque consommateur interpretait ces mots lui-meme, quatre agents ecriraient
//  quatre semantiques, et « la position est un resultat » deviendrait faux —
//  elle serait quatre resultats.
//
//  ALORS POURQUOI UN SECOND PARCOURS ? Parce que les deux echelles n'ont pas la
//  meme FORME DE DONNEES, et seulement pour ca :
//
//    | | `NkSolveLayout` (kit) | `NkComputeLayout` (ici) |
//    |---|---|---|
//    | l'arbre | une TABLE compilee, parent avant enfant | un arbre MUTABLE, remanie a la souris |
//    | les liens | par NOM (`parent = "toolbar"`) | par INDEX (les noms changent en cours d'edition) |
//    | les nombres | les metriques du composant | les metriques du DOCUMENT |
//    | le parcours | une boucle, l'ordre de la table suffit | recursif : l'ordre n'est pas garanti |
//
//  Autrement dit : **meme semantique, deux structures de donnees.** Le jour ou
//  un document se fige en composant de bibliotheque, il devient une table et
//  c'est le solveur du kit qui le sert — sans qu'un seul mot change de sens.
//
// =============================================================================
//  UNE DIVERGENCE TROUVEE, ET C'EST LE KIT QUI A TRANCHE
// =============================================================================
//  Ce solveur repartissait le reste EN BOUCLE (un enfant ramene par sa borne
//  rend sa part aux autres) ; celui du kit le faisait en UNE passe, et laissait
//  un creux inexplique dans le parent. **Deux solveurs qui divergent, c'est deux
//  semantiques pour une meme declaration** — exactement ce que cette forme
//  existe pour empecher.
//
//  Le gardien a adopte la boucle le jour meme, et `NkLayoutSolve.h` fait foi.
//  ⚠️ CE FICHIER MIROITE MAINTENANT SA SEMANTIQUE FINALE, POINT PAR POINT :
//    - repartition iterative, gel par `solvedetail::AtBound`, 8 passes au plus ;
//    - `mainAlign` n'est applique QUE s'il ne reste rien a poids (sinon les
//      poids ont deja tout pris, par definition) ;
//    - en grille, la hauteur de rangee est le MAX de ses enfants, et une hauteur
//      a poids retombe sur une cellule carree.
//
//  ⚠️ SI VOUS MODIFIEZ UNE DE CES TROIS REGLES ICI SANS LA MODIFIER LA-BAS, vous
//     reintroduisez exactement le defaut qui vient d'etre repare — et cette fois
//     personne ne le verra, parce que les deux fichiers ne seront plus ecrits en
//     meme temps.
//
// =============================================================================
//  LA SOURIS N'ECRIT JAMAIS UNE POSITION
// =============================================================================
//  `NkResizeByDrag` traduit un deplacement de souris en PROPRIETE DECLAREE :
//  une taille pour un `Fixed`, un POIDS pour un `Expand`/`Weight`. C'est la
//  difference entre un outil de design et un constructeur d'interfaces, et elle
//  tient dans cette fonction.
//
// OU AJOUTER LA PROCHAINE CHOSE :
//   un agencement de plus -> d'abord dans `NkComponentLayout.h` (chez le
//   gardien), puis un `case` dans `PlaceChildren` ici. Jamais l'inverse : un
//   agencement que le kit ignore serait un agencement que les composants ne
//   savent pas rendre.
// -----------------------------------------------------------------------------

#include "NKEditorKit/Components/NkComponentPaint.h"
#include "NKEditorKit/Components/NkLayoutSolve.h"

#include "Document.h"

namespace nkuidesign {

	using nkentseu::editorkit::NkPaintRect;

	// ── LE RESULTAT ─────────────────────────────────────────────────────────
	// Un rectangle par noeud, indexe comme `NkUIDocument::nodes`.
	//
	// ⚠️ RIEN DE CE TYPE NE SE SERIALISE. C'est la contrepartie de la regle de
	//    `Document.h` : si un jour quelqu'un ecrit un `NkLayoutResult` dans un
	//    fichier, le document redevient un jeu de coordonnees avec une couche de
	//    proprietes par-dessus pour faire joli.
	//
	// ⚠️ `valid` n'est pas de la coquetterie : un noeud non place n'a PAS de
	//    rectangle, et rendre `{0,0,0,0}` le ferait passer pour un noeud pose a
	//    l'origine avec une taille nulle — indistinguable a l'oeil, different
	//    pour un test de survol.
	struct NkLayoutResult {
			NkVector<NkPaintRect> rects;
			NkVector<uint8> valid;

			bool Has(int32 i) const {
				return i >= 0 && (uint32)i < (uint32)valid.Size() && valid[(uint32)i] != 0;
			}
			const NkPaintRect &At(int32 i) const {
				return rects[(uint32)i];
			}
			uint32 ValidCount() const {
				uint32 n = 0;
				for (uint32 i = 0; i < (uint32)valid.Size(); ++i)
					if (valid[i])
						++n;
				return n;
			}
	};

	namespace layoutdetail {

		using namespace nkentseu::editorkit;

		inline void PlaceChildren(const NkUIDocument &doc, const NkMetricSource &m, int32 node,
								  NkLayoutResult &out);

		inline void PlaceSubtree(const NkUIDocument &doc, const NkMetricSource &m, int32 node,
								 const NkPaintRect &r, NkLayoutResult &out) {
			out.rects[(uint32)node] = r;
			out.valid[(uint32)node] = 1;
			PlaceChildren(doc, m, node, out);
		}

		inline void PlaceChildren(const NkUIDocument &doc, const NkMetricSource &m, int32 node,
								  NkLayoutResult &out) {
			const NkUINode &p = doc.nodes[(uint32)node];
			const NkVector<int32> &kids = p.children;
			const uint32 n = (uint32)kids.Size();
			if (n == 0 || p.layout.kind == NkLayoutKind::None)
				return;

			// Gouttiere et marge par leur NOM : c'est la regle du kit (« un
			// espacement est du style »), et c'est ce qui permet de changer
			// l'aeration de tout un document en une valeur.
			const float32 pad = m.Get(p.padName.Data(), 0.f);
			const float32 gap = m.Get(p.spacingName.Data(), 0.f);
			const NkPaintRect &r = out.rects[(uint32)node];
			NkPaintRect inner;
			inner.x = r.x + pad;
			inner.y = r.y + pad;
			inner.w = r.w - 2.f * pad;
			inner.h = r.h - 2.f * pad;
			if (inner.w < 0.f)
				inner.w = 0.f;
			if (inner.h < 0.f)
				inner.h = 0.f;

			// ── ANCRAGE ─────────────────────────────────────────────────────
			// Chaque enfant est pose independamment contre les bords qu'il declare.
			// Deux bords opposes = il s'etire entre eux ; un seul = il s'y colle a
			// sa taille ; aucun = il est centre. Meme regle que le solveur du kit.
			if (p.layout.kind == NkLayoutKind::Anchor) {
				for (uint32 i = 0; i < n; ++i) {
					const NkUINode &c = doc.nodes[(uint32)kids[i]];
					const uint8 e = c.anchorEdges;
					const bool l = (e & nkanchor::Left) != 0, ri = (e & nkanchor::Right) != 0;
					const bool t = (e & nkanchor::Top) != 0, b = (e & nkanchor::Bottom) != 0;
					float32 w = (l && ri) ? solvedetail::Clamp(inner.w, c.width)
										  : solvedetail::Axis(c.width, m, inner.w, c.width.minVal);
					float32 h = (t && b) ? solvedetail::Clamp(inner.h, c.height)
										 : solvedetail::Axis(c.height, m, inner.h, c.height.minVal);
					NkPaintRect cr;
					cr.w = w;
					cr.h = h;
					cr.x = (!l && ri) ? inner.x + inner.w - w
									  : (l ? inner.x : inner.x + (inner.w - w) * 0.5f);
					cr.y = (!t && b) ? inner.y + inner.h - h
									 : (t ? inner.y : inner.y + (inner.h - h) * 0.5f);
					PlaceSubtree(doc, m, kids[i], cr, out);
				}
				return;
			}

			// ── GRILLE ──────────────────────────────────────────────────────
			// Miroir exact du kit : colonnes imposees (ou auto-remplissage a partir
			// d'une largeur de cellule nommee), largeur de cellule uniforme, et
			// hauteur de RANGEE egale au plus grand de ses enfants. Une hauteur a
			// poids retombe sur une cellule carree — c'est ce que fait deja la
			// grille de cartes du navigateur de contenu, ca ne s'invente pas ici.
			if (p.layout.kind == NkLayoutKind::Grid) {
				uint32 cols = p.layout.gridColumns;
				if (cols == 0) {
					const float32 cell = m.Get(p.layout.gridCellMetric, 0.f);
					cols = (cell > 0.f) ? (uint32)((inner.w + gap) / (cell + gap)) : 1u;
					if (cols == 0)
						cols = 1u;
				}
				float32 cw = (inner.w - gap * (float32)(cols - 1)) / (float32)cols;
				if (cw < 0.f)
					cw = 0.f;
				float32 rowY = inner.y, rowH = 0.f;
				for (uint32 i = 0; i < n; ++i) {
					const NkUINode &c = doc.nodes[(uint32)kids[i]];
					const uint32 col = i % cols;
					if (col == 0 && i > 0) {
						rowY += rowH + gap;
						rowH = 0.f;
					}
					float32 h = solvedetail::Axis(c.height, m, inner.h, c.height.minVal);
					if (solvedetail::IsWeighted(c.height) || h <= 0.f)
						h = cw;
					if (h > rowH)
						rowH = h;
					NkPaintRect cr;
					cr.x = inner.x + (cw + gap) * (float32)col;
					cr.y = rowY;
					cr.w = cw;
					cr.h = h;
					PlaceSubtree(doc, m, kids[i], cr, out);
				}
				return;
			}

			// ── LIGNE ET COLONNE ────────────────────────────────────────────
			// Le meme code, un axe echange. Les ecrire deux fois aurait garanti
			// qu'une correction n'en repare qu'une.
			const bool horiz = (p.layout.kind == NkLayoutKind::Row);
			const float32 mainAvail = (horiz ? inner.w : inner.h) - gap * (float32)(n - 1);
			const float32 crossAvail = horiz ? inner.h : inner.w;

			NkVector<float32> main;
			main.Resize(n, 0.f);

			// ── A. ce qui ne se partage pas ─────────────────────────────────
			float32 fixedSum = 0.f;
			bool anyWeighted = false;
			for (uint32 i = 0; i < n; ++i) {
				const NkUINode &c = doc.nodes[(uint32)kids[i]];
				const NkSizeDecl &sz = horiz ? c.width : c.height;
				if (solvedetail::IsWeighted(sz)) {
					anyWeighted = true;
				} else {
					main[i] = solvedetail::Axis(sz, m, mainAvail, sz.minVal);
					fixedSum += main[i];
				}
			}

			// ── B. le partage, EN BOUCLE ────────────────────────────────────
			// Un enfant qui a bute sur une borne est FIGE : sa part sort du partage
			// et ce qu'il n'a pas pris revient aux autres. Sans cette boucle, un
			// creux inexplique apparait dans le parent. Le gel se DEDUIT de la
			// valeur posee (`AtBound`) plutot que de se stocker — meme choix que le
			// kit, pour que les deux se relisent l'un l'autre.
			if (anyWeighted) {
				const uint16 kMaxPasses = 8;
				for (uint16 pass = 0; pass < kMaxPasses; ++pass) {
					float32 frozenSum = 0.f, freeWeight = 0.f;
					for (uint32 i = 0; i < n; ++i) {
						const NkUINode &c = doc.nodes[(uint32)kids[i]];
						const NkSizeDecl &sz = horiz ? c.width : c.height;
						if (!solvedetail::IsWeighted(sz))
							continue;
						if (pass > 0 && solvedetail::AtBound(main[i], sz))
							frozenSum += main[i];
						else
							freeWeight += solvedetail::WeightOf(sz);
					}
					if (freeWeight <= 0.f)
						break;
					float32 rem = mainAvail - fixedSum - frozenSum;
					if (rem < 0.f)
						rem = 0.f;
					bool changed = false;
					for (uint32 i = 0; i < n; ++i) {
						const NkUINode &c = doc.nodes[(uint32)kids[i]];
						const NkSizeDecl &sz = horiz ? c.width : c.height;
						if (!solvedetail::IsWeighted(sz))
							continue;
						if (pass > 0 && solvedetail::AtBound(main[i], sz))
							continue;
						const float32 v =
							solvedetail::Clamp(rem * solvedetail::WeightOf(sz) / freeWeight, sz);
						if (v != main[i])
							changed = true;
						main[i] = v;
					}
					if (!changed)
						break;
				}
			}

			// ── C. les positions ────────────────────────────────────────────
			// L'alignement principal n'a de sens que s'il RESTE de la place, donc
			// seulement quand rien n'est a poids — sinon les poids l'ont deja toute
			// prise, par definition.
			float32 cursor = horiz ? inner.x : inner.y;
			if (!anyWeighted)
				cursor = solvedetail::Aligned(cursor, horiz ? inner.w : inner.h,
											  fixedSum + gap * (float32)(n - 1), p.layout.mainAlign);

			for (uint32 i = 0; i < n; ++i) {
				const NkUINode &c = doc.nodes[(uint32)kids[i]];
				const NkSizeDecl &szCross = horiz ? c.height : c.width;
				float32 cross;
				if (p.layout.crossAlign == NkAlign::Stretch || solvedetail::IsWeighted(szCross))
					cross = solvedetail::Clamp(crossAvail, szCross);
				else
					cross = solvedetail::Axis(szCross, m, crossAvail, szCross.minVal);
				const float32 crossPos =
					solvedetail::Aligned(horiz ? inner.y : inner.x, crossAvail, cross, p.layout.crossAlign);

				NkPaintRect cr;
				if (horiz) {
					cr.x = cursor;
					cr.y = crossPos;
					cr.w = main[i];
					cr.h = cross;
				} else {
					cr.x = crossPos;
					cr.y = cursor;
					cr.w = cross;
					cr.h = main[i];
				}
				cursor += main[i] + gap;
				PlaceSubtree(doc, m, kids[i], cr, out);
			}
		}

	} // namespace layoutdetail

	// ── LE POINT D'ENTREE ───────────────────────────────────────────────────
	// `surface` est le rectangle offert au document. Le meme document dans deux
	// surfaces differentes produit deux resultats differents — c'est la
	// definition meme de « la position est un resultat », et c'est ce que la
	// sonde mesure.
	inline void NkComputeLayout(const NkUIDocument &doc, const NkPaintRect &surface,
								NkLayoutResult &out) {
		const uint32 n = (uint32)doc.nodes.Size();
		out.rects.Clear();
		out.valid.Clear();
		out.rects.Resize(n, NkPaintRect());
		out.valid.Resize(n, (uint8)0);
		if (n == 0)
			return;
		layoutdetail::PlaceSubtree(doc, doc.MetricSource(), 0, surface, out);
	}

	// ═══════════════════════════════════════════════════════════════════════════
	//  LA SOURIS, TRADUITE EN PROPRIETES
	// ═══════════════════════════════════════════════════════════════════════════

	/// Le noeud le plus PROFOND dont le rectangle contient le point. C'est la
	/// « pose a la souris » : on ne retient pas ou l'utilisateur a lache, on
	/// retient DANS QUI il a lache. Rend -1 si le point tombe hors de la racine.
	inline int32 NkPickNode(const NkUIDocument &doc, const NkLayoutResult &lay, float32 x, float32 y) {
		int32 best = -1, bestDepth = -1;
		for (uint32 i = 0; i < (uint32)doc.nodes.Size(); ++i) {
			if (!lay.Has((int32)i) || !lay.At((int32)i).Contains(x, y))
				continue;
			int32 depth = 0;
			for (int32 c = doc.nodes[i].parent; c >= 0; c = doc.nodes[(uint32)c].parent)
				++depth;
			if (depth > bestDepth) {
				bestDepth = depth;
				best = (int32)i;
			}
		}
		return best;
	}

	/// TIRER UN BORD. `deltaPx` est ce que la souris a parcouru ; ce qui est
	/// ECRIT est une propriete de taille.
	///
	/// ⚠️ LA CONVERSION EST LE COEUR DE LA REGLE DE RODOLF :
	///      - un enfant `Fixed` recoit une nouvelle TAILLE — un nombre de pixels,
	///        pas une position ;
	///      - un enfant `Expand` / `Weight` recoit un nouveau POIDS, deduit du
	///        rapport entre la taille qu'il occupe et celle qu'on veut. Il bascule
	///        en `Weight` (un poids de 1,37 n'est plus « extensible »), et le
	///        document reste responsive : agrandi de 20 % dans une fenetre, il
	///        vaudra 20 % de plus dans toutes les autres ;
	///      - un `Fraction` recoit une nouvelle FRACTION, par le meme rapport.
	///
	/// ⚠️ ET LA CONDITION D'EXISTENCE, qui n'est pas une precaution de style : la
	///    conversion divise par la taille courante. Sans le refus quand elle est
	///    nulle (noeud jamais place, ou place a zero), on ecrirait un poids infini
	///    ou nul dans le document — une donnee fausse produite par un geste qui
	///    n'avait aucun sens. On refuse, et le retour le dit.
	inline bool NkResizeByDrag(NkUIDocument &doc, const NkLayoutResult &lay, int32 node, bool horizontal,
							   float32 deltaPx) {
		using namespace nkentseu::editorkit;
		if (!doc.IsValidIndex(node) || !lay.Has(node))
			return false;
		const NkPaintRect &r = lay.At(node);
		const float32 current = horizontal ? r.w : r.h;
		if (current <= 0.001f)
			return false; // rien a mesurer : on n'invente pas un rapport
		const float32 wanted = current + deltaPx;
		if (wanted < 0.f)
			return false;

		NkSizeDecl &s = horizontal ? doc.nodes[(uint32)node].width : doc.nodes[(uint32)node].height;
		const float32 ratio = wanted / current;
		switch (s.mode) {
			case NkSizeMode::Fixed:
				s.value = solvedetail::Clamp(wanted, s);
				break;
			case NkSizeMode::Fraction:
				s.value = s.value > 0.f ? s.value * ratio : ratio;
				break;
			case NkSizeMode::Content:
				// Un `Content` tire cesse d'etre un `Content` : l'utilisateur vient
				// de dire « pas cette taille-la ». On le fige a ce qu'il voulait,
				// plutot que d'ignorer le geste sans rien dire.
				s.mode = NkSizeMode::Fixed;
				s.value = solvedetail::Clamp(wanted, s);
				break;
			case NkSizeMode::Weight:
			case NkSizeMode::Expand: {
				const float32 w0 = solvedetail::WeightOf(s);
				float32 nw = w0 * ratio;
				if (nw < 0.0001f)
					nw = 0.0001f;
				s.mode = NkSizeMode::Weight;
				s.value = nw;
				break;
			}
			case NkSizeMode::Count:
				return false;
		}
		doc.MarkHumanEdit(node);
		return true;
	}

	/// Passer un noeud en taille FIXE en partant de ce qu'il mesure a l'ecran —
	/// « fige-le tel qu'il est ». Meme condition d'existence : sans rectangle
	/// valide, il n'y a pas de « tel qu'il est ».
	inline bool NkFreezeSize(NkUIDocument &doc, const NkLayoutResult &lay, int32 node, bool horizontal) {
		using namespace nkentseu::editorkit;
		if (!doc.IsValidIndex(node) || !lay.Has(node))
			return false;
		const float32 current = horizontal ? lay.At(node).w : lay.At(node).h;
		if (current <= 0.f)
			return false;
		NkSizeDecl &s = horizontal ? doc.nodes[(uint32)node].width : doc.nodes[(uint32)node].height;
		s.mode = NkSizeMode::Fixed;
		s.value = solvedetail::Clamp(current, s);
		doc.MarkHumanEdit(node);
		return true;
	}

} // namespace nkuidesign
