// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#pragma once
// =============================================================================
// NkConvexHull2D.h — enveloppe convexe d'un nuage de points 2D (en-tete seul).
//
// Principe (Andrew, 1979, variante de Graham ; « monotone chain »).
//   L'enveloppe convexe d'un ensemble fini de points est le plus petit polygone
//   convexe qui les contient tous : l'elastique tendu autour de clous.
//   1. On trie les points par x croissant, puis par y croissant (ordre lexicographique).
//   2. On construit l'enveloppe INFERIEURE de gauche a droite : on empile les
//      points, et tant que les trois derniers (A, B, C) ne font pas un virage
//      strictement a gauche, on depile B. Le test est le signe du produit
//      vectoriel
//          cross(A, B, C) = (B - A) x (C - A) = (Bx-Ax)(Cy-Ay) - (By-Ay)(Cx-Ax) :
//      > 0 virage a gauche (on garde B), <= 0 virage a droite ou alignement
//      (B est a l'interieur ou sur un cote : on le retire).
//   3. On fait de meme de droite a gauche pour l'enveloppe SUPERIEURE.
//   4. On concatene les deux chaines, sans repeter leurs extremites communes.
//   Complexite : O(n log n), dominee par le tri ; les deux balayages sont O(n)
//   car chaque point est empile et depile au plus une fois.
//   Resultat : sommets en ordre anti-horaire (repere x vers la droite, y vers le
//   haut), sans sommet colineaire. Cas limites : 0, 1 ou 2 points distincts
//   rendent ces points ; un nuage entierement aligne rend ses deux extremites.
//
// Usage dans FoodHunt : contour convexe du dessin d'un aliment (la forme de
// collision d'une piece) a partir des pixels opaques de sa vignette.
// Contre-epreuve du banc : remplacer `<= 0` par `< 0` laisse les points alignes
// dans l'enveloppe, le banc des cotes colineaires devient rouge.
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKMath/NkVec.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/Utilities/NkSort.h"

namespace nkentseu {

	namespace math {

	namespace detail {

		/// Produit vectoriel (B - A) x (C - A) : > 0 si A -> B -> C tourne a gauche.
		inline float32 NkHullCross(const NkVec2f &a, const NkVec2f &b, const NkVec2f &c) noexcept {
			return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
		}

	} // namespace detail

	/// Enveloppe convexe de `n` points ; `sortie` recoit les sommets en ordre anti-horaire.
	inline void NkConvexHull2D(const NkVec2f *points, usize n, NkVector<NkVec2f> &sortie) {
		sortie.Clear();
		if (n == 0 || points == nullptr) {
			return;
		}

		NkVector<NkVec2f> tri;
		tri.Reserve(n);
		for (usize i = 0; i < n; ++i) {
			tri.PushBack(points[i]);
		}
		::nkentseu::NkSort(tri.Data(), tri.Data() + tri.Size(), [](const NkVec2f &a, const NkVec2f &b) {
			return a.x < b.x || (a.x == b.x && a.y < b.y);
		});

		// Retire les doublons exacts (sinon cross = 0 les traiterait mal).
		usize m = 0;
		for (usize i = 0; i < tri.Size(); ++i) {
			if (m == 0 || tri[i].x != tri[m - 1].x || tri[i].y != tri[m - 1].y) {
				tri[m++] = tri[i];
			}
		}
		if (m <= 2) {
			for (usize i = 0; i < m; ++i) {
				sortie.PushBack(tri[i]);
			}
			return;
		}

		NkVector<NkVec2f> h;
		h.Resize(2 * m);
		usize k = 0;
		// Enveloppe inferieure (gauche -> droite)
		for (usize i = 0; i < m; ++i) {
			while (k >= 2 && detail::NkHullCross(h[k - 2], h[k - 1], tri[i]) <= 0.0f) {
				--k;
			}
			h[k++] = tri[i];
		}
		// Enveloppe superieure (droite -> gauche) ; `plancher` evite de depiler la chaine basse
		const usize plancher = k + 1;
		for (usize i = m - 1; i > 0; --i) {
			const NkVec2f &p = tri[i - 1];
			while (k >= plancher && detail::NkHullCross(h[k - 2], h[k - 1], p) <= 0.0f) {
				--k;
			}
			h[k++] = p;
		}
		// Le dernier point repete le premier : on ne le garde pas.
		for (usize i = 0; i + 1 < k; ++i) {
			sortie.PushBack(h[i]);
		}
	}

	/// Variante sur un NkVector.
	inline void NkConvexHull2D(const NkVector<NkVec2f> &points, NkVector<NkVec2f> &sortie) {
		NkConvexHull2D(points.Data(), points.Size(), sortie);
	}

	} // namespace math

} // namespace nkentseu
