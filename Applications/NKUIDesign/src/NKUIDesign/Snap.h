#pragma once
// -----------------------------------------------------------------------------
// @File    Snap.h
// @Brief   L'AIMANTATION DE LA TOILE (Lunacy) : bords, centres, page, et les
//          ESPACEMENTS EGAUX — plus les guides a dessiner.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  ⚠️ C'EST UN MECANISME, PAS UN DESSIN — ET LA SEPARATION EST LA RAISON
//     D'ETRE DE CE FICHIER
// =============================================================================
//  `NkCalculerSnap` ne connait ni la souris, ni l'ecran, ni le zoom : il prend
//  un rectangle en espace DOCUMENT, une disposition, une tolerance en unites
//  DOCUMENT, et rend un decalage plus les guides a peindre. Il n'appelle aucune
//  primitive de dessin.
//
//  Pourquoi cette insistance : ecrit dans `PreviewPanel::OnUI` — le seul
//  endroit ou le drag existe — le calcul aurait ete DANS un endroit qu'aucun
//  banc ne peut atteindre. Ce depot a deja paye cette facture (Q41 : « le zoom
//  et le deplacement vivent dans OnUI, un endroit qu AUCUN banc ne peut
//  atteindre : ils sont donc restes trois etapes sans preuve »). Ici, la
//  recette exerce le VRAI calcul, celui que la souris appelle.
//
//  ⚠️ LA TOLERANCE EST EN UNITES DOCUMENT, ET L'APPELANT DIVISE PAR LE ZOOM.
//     C'est la meme faute que le depot a corrigee sur le redimensionnement :
//     un seuil de 6 px ECRAN vaut 6 unites au zoom 1 et 1,5 unite au zoom 4.
//     Une tolerance non divisee ferait un aimant qui colle de plus en plus fort
//     a mesure qu'on zoome — c'est-a-dire l'inverse de ce qu'on veut, puisqu'on
//     zoome PRECISEMENT pour placer finement.
//
// =============================================================================
//  CE QU'IL AIMANTE, ET DANS QUEL ORDRE (contrat Lunacy)
// =============================================================================
//  Par axe, on collecte les VALEURS CANDIDATES visibles :
//    - de chaque VOISIN (meme parent, ni soi-meme ni un de ses descendants) :
//      son bord bas, son centre, son bord haut  (3 par axe) ;
//    - du PARENT (la page) : ses deux bords et son centre (3 par axe).
//  Et on compare a trois SONDES du rectangle deplace : son bord bas, son
//  centre, son bord haut. Le meilleur ecart de chaque axe gagne s'il tient dans
//  la tolerance. Les deux axes sont INDEPENDANTS — un bord peut coller en X
//  sans rien coller en Y, et c'est le comportement de Lunacy.
//
//  ⚠️ LE CENTRE CONTRE LE CENTRE PASSE AVANT LE BORD CONTRE LE BORD A EGALITE.
//     Sans regle de departage, deux candidats a la meme distance donnaient un
//     resultat qui dependait de l'ORDRE DES NOEUDS dans le document — donc
//     d'une renumerotation. Un aimant qui change d'avis quand on supprime un
//     noeud ailleurs est pire qu'un aimant qui se trompe : il n'est pas
//     reproductible, et il ne se mesure pas.
//
//  L'ESPACEMENT EGAL (« smart distribute ») est un TROISIEME candidat par axe :
//  si un voisin est avant et un autre apres le rectangle deplace sur le meme
//  axe, la position qui rend les deux ecarts EGAUX est proposee. C'est ce qui
//  fait dire a Lunacy « ces trois-la sont regulierement espaces ».
// -----------------------------------------------------------------------------
#include "Layout.h"

namespace nkuidesign {

	using nkentseu::float32;
	using nkentseu::int32;
	using nkentseu::uint32;

	/// UN GUIDE A PEINDRE, en espace DOCUMENT (l'appelant le projette a l'ecran
	/// avec la MEME vue que le reste — jamais une conversion de son cru).
	/// `coord` : x pour un guide VERTICAL, y pour un guide HORIZONTAL.
	/// `de`/`a` : l'etendue sur l'autre axe — de quoi couvrir a la fois le
	/// rectangle deplace et le voisin qui l'aimante, ce qui fait qu'on VOIT
	/// contre quoi on s'aligne au lieu de deviner.
	struct NkSnapGuide {
			bool actif = false;
			float32 coord = 0.f;
			float32 de = 0.f, a = 0.f;
			int32 voisin = -1;	///< le noeud qui aimante, -1 = la page (le parent)
			float32 ecart = 0.f; ///< la distance mesuree (badge) quand elle a un sens
			bool badge = false;	///< vrai pour un espacement egal : la distance se DIT
	};

	/// LE RESULTAT : un decalage a AJOUTER au rectangle, et les guides.
	/// dx/dy valent 0 quand rien n'aimante — jamais une valeur « presque nulle »
	/// qu'un appelant prendrait pour un snap.
	struct NkSnapResultat {
			float32 dx = 0.f, dy = 0.f;
			NkSnapGuide guideV; ///< ligne VERTICALE (alignement en X)
			NkSnapGuide guideH; ///< ligne HORIZONTALE (alignement en Y)
			bool Aimante() const { return guideV.actif || guideH.actif; }
	};

	namespace snapdetail {

		/// Un candidat d'axe : la valeur a atteindre, la sonde du rectangle
		/// deplace qui doit l'atteindre, et de quoi peindre le guide.
		struct Cand {
				float32 valeur = 0.f; ///< la coordonnee visee (espace document)
				float32 sonde = 0.f;  ///< la coordonnee du rect deplace qui vise
				int32 voisin = -1;
				int32 rang = 2; ///< 0 = centre/centre (prioritaire), 1 = page, 2 = bord
				bool badge = false;
				float32 ecart = 0.f;
		};

		/// `a` descend-il de `b` ? Un descendant du noeud deplace BOUGE AVEC LUI :
		/// l'aimanter reviendrait a s'aimanter sur soi-meme, ce qui colle
		/// toujours et n'aligne rien.
		inline bool Descend(const NkUIDocument &doc, int32 a, int32 b) {
			if (!doc.IsValidIndex(a) || !doc.IsValidIndex(b))
				return false;
			for (int32 p = doc.nodes[(uint32)a].parent; p >= 0; p = doc.nodes[(uint32)p].parent)
				if (p == b)
					return true;
			return false;
		}

		/// Le meilleur candidat d'un axe : plus petit ecart, puis plus petit
		/// rang. ⚠️ LE DEPARTAGE PAR RANG N'EST PAS COSMETIQUE — sans lui, deux
		/// candidats a egalite se departageaient par l'ordre des noeuds.
		inline bool Meilleur(const Cand *c, int32 n, float32 tol, Cand &out, float32 &delta) {
			bool trouve = false;
			float32 meilleurEcart = 0.f;
			for (int32 i = 0; i < n; ++i) {
				const float32 d = c[i].valeur - c[i].sonde;
				const float32 ad = d < 0.f ? -d : d;
				if (ad > tol)
					continue;
				if (!trouve || ad < meilleurEcart - 0.0001f
					|| (ad < meilleurEcart + 0.0001f && c[i].rang < out.rang)) {
					trouve = true;
					meilleurEcart = ad;
					out = c[i];
					delta = d;
				}
			}
			return trouve;
		}

	} // namespace snapdetail

	/// L'AIMANTATION DE `rect` (espace DOCUMENT, deja deplace par la souris)
	/// contre ses voisins et sa page. `noeud` sert a s'exclure soi-meme et ses
	/// descendants ; `tolDoc` est la tolerance EN UNITES DOCUMENT (l'appelant a
	/// divise ses pixels ecran par le zoom).
	inline NkSnapResultat NkCalculerSnap(const NkUIDocument &doc, const NkLayoutResult &lay,
										 int32 noeud, const NkPaintRect &rect, float32 tolDoc) {
		NkSnapResultat r;
		if (tolDoc <= 0.f || !doc.IsValidIndex(noeud))
			return r;
		const int32 parent = doc.nodes[(uint32)noeud].parent;
		if (!doc.IsValidIndex(parent) || !lay.Has(parent))
			return r;

		enum { kMaxCand = 256 };
		snapdetail::Cand cx[kMaxCand], cy[kMaxCand];
		int32 nx = 0, ny = 0;
		const float32 sondeX[3] = {rect.x, rect.x + rect.w * 0.5f, rect.x + rect.w};
		const float32 sondeY[3] = {rect.y, rect.y + rect.h * 0.5f, rect.y + rect.h};

		// ── LA PAGE (le parent) : ses bords et son centre ────────────────────
		{
			const NkPaintRect p = lay.At(parent);
			const float32 vx[3] = {p.x, p.x + p.w * 0.5f, p.x + p.w};
			const float32 vy[3] = {p.y, p.y + p.h * 0.5f, p.y + p.h};
			for (int32 i = 0; i < 3 && nx < kMaxCand; ++i)
				for (int32 j = 0; j < 3 && nx < kMaxCand; ++j) {
					snapdetail::Cand &c = cx[nx++];
					c.valeur = vx[i];
					c.sonde = sondeX[j];
					c.voisin = -1;
					c.rang = (i == 1 && j == 1) ? 0 : 1;
				}
			for (int32 i = 0; i < 3 && ny < kMaxCand; ++i)
				for (int32 j = 0; j < 3 && ny < kMaxCand; ++j) {
					snapdetail::Cand &c = cy[ny++];
					c.valeur = vy[i];
					c.sonde = sondeY[j];
					c.voisin = -1;
					c.rang = (i == 1 && j == 1) ? 0 : 1;
				}
		}
		// ── LES VOISINS : memes parent, ni soi ni un descendant ──────────────
		const nkentseu::NkVector<int32> &fratrie = doc.nodes[(uint32)parent].children;
		for (uint32 k = 0; k < (uint32)fratrie.Size(); ++k) {
			const int32 v = fratrie[k];
			if (v == noeud || !lay.Has(v) || snapdetail::Descend(doc, v, noeud))
				continue;
			const NkPaintRect q = lay.At(v);
			const float32 vx[3] = {q.x, q.x + q.w * 0.5f, q.x + q.w};
			const float32 vy[3] = {q.y, q.y + q.h * 0.5f, q.y + q.h};
			for (int32 i = 0; i < 3 && nx < kMaxCand; ++i)
				for (int32 j = 0; j < 3 && nx < kMaxCand; ++j) {
					snapdetail::Cand &c = cx[nx++];
					c.valeur = vx[i];
					c.sonde = sondeX[j];
					c.voisin = v;
					c.rang = (i == 1 && j == 1) ? 0 : 2;
				}
			for (int32 i = 0; i < 3 && ny < kMaxCand; ++i)
				for (int32 j = 0; j < 3 && ny < kMaxCand; ++j) {
					snapdetail::Cand &c = cy[ny++];
					c.valeur = vy[i];
					c.sonde = sondeY[j];
					c.voisin = v;
					c.rang = (i == 1 && j == 1) ? 0 : 2;
				}
		}
		// ── L'ESPACEMENT EGAL : un voisin AVANT, un voisin APRES ─────────────
		// La position qui rend les deux ecarts identiques. C'est le seul
		// candidat qui merite un BADGE : il annonce une DISTANCE, la ou les
		// autres n'annoncent qu'un alignement.
		for (uint32 ka = 0; ka < (uint32)fratrie.Size(); ++ka) {
			const int32 va = fratrie[ka];
			if (va == noeud || !lay.Has(va) || snapdetail::Descend(doc, va, noeud))
				continue;
			for (uint32 kb = 0; kb < (uint32)fratrie.Size(); ++kb) {
				const int32 vb = fratrie[kb];
				if (vb == noeud || vb == va || !lay.Has(vb) || snapdetail::Descend(doc, vb, noeud))
					continue;
				const NkPaintRect A = lay.At(va), B = lay.At(vb);
				// X : A finit avant, B commence apres -> l'ecart egal
				if (A.x + A.w <= B.x && nx < kMaxCand) {
					const float32 libre = B.x - (A.x + A.w) - rect.w;
					if (libre > 0.f) {
						snapdetail::Cand &c = cx[nx++];
						c.valeur = A.x + A.w + libre * 0.5f;
						c.sonde = rect.x;
						c.voisin = va;
						c.rang = 1;
						c.badge = true;
						c.ecart = libre * 0.5f;
					}
				}
				if (A.y + A.h <= B.y && ny < kMaxCand) {
					const float32 libre = B.y - (A.y + A.h) - rect.h;
					if (libre > 0.f) {
						snapdetail::Cand &c = cy[ny++];
						c.valeur = A.y + A.h + libre * 0.5f;
						c.sonde = rect.y;
						c.voisin = va;
						c.rang = 1;
						c.badge = true;
						c.ecart = libre * 0.5f;
					}
				}
			}
		}

		snapdetail::Cand gagnantX, gagnantY;
		float32 dx = 0.f, dy = 0.f;
		if (snapdetail::Meilleur(cx, nx, tolDoc, gagnantX, dx)) {
			r.dx = dx;
			r.guideV.actif = true;
			r.guideV.coord = gagnantX.valeur;
			r.guideV.voisin = gagnantX.voisin;
			r.guideV.badge = gagnantX.badge;
			r.guideV.ecart = gagnantX.ecart;
			// L'etendue VERTICALE du guide : de quoi couvrir le rect deplace ET
			// son voisin — c'est ce qui montre CONTRE QUOI on s'aligne.
			const NkPaintRect ref =
				lay.Has(gagnantX.voisin) ? lay.At(gagnantX.voisin) : lay.At(parent);
			const float32 y0 = (rect.y < ref.y) ? rect.y : ref.y;
			const float32 y1 = ((rect.y + rect.h) > (ref.y + ref.h)) ? (rect.y + rect.h)
																	 : (ref.y + ref.h);
			r.guideV.de = y0;
			r.guideV.a = y1;
		}
		if (snapdetail::Meilleur(cy, ny, tolDoc, gagnantY, dy)) {
			r.dy = dy;
			r.guideH.actif = true;
			r.guideH.coord = gagnantY.valeur;
			r.guideH.voisin = gagnantY.voisin;
			r.guideH.badge = gagnantY.badge;
			r.guideH.ecart = gagnantY.ecart;
			const NkPaintRect ref =
				lay.Has(gagnantY.voisin) ? lay.At(gagnantY.voisin) : lay.At(parent);
			const float32 x0 = (rect.x < ref.x) ? rect.x : ref.x;
			const float32 x1 = ((rect.x + rect.w) > (ref.x + ref.w)) ? (rect.x + rect.w)
																	 : (ref.x + ref.w);
			r.guideH.de = x0;
			r.guideH.a = x1;
		}
		return r;
	}

} // namespace nkuidesign
