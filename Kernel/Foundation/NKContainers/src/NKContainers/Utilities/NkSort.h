#pragma once
// =============================================================================
// NkSort.h — Trier une plage, sans STL.
//
// POURQUOI CE FICHIER EXISTE (2026-08-17) :
//   `NkVector` ne sait pas trier — vérifié, aucune méthode `Sort` dans
//   `NkVector.h`. Conséquence : qui a besoin d'ordonner une séquence appelle
//   `std::sort`, y compris **à l'intérieur de NKContainers lui-même**
//   (`NkBasicString.h`, `NkSpan.h`) et dans les modules qui le consomment.
//   Un moteur qui refuse la STL ne peut pas laisser son conteneur principal
//   sans tri : le manque se paie en dépendances dispersées, une par site.
//
// CE QU'IL FAIT : tri en place, sans allocation, comparateur paramétrable.
//   - **quicksort** avec pivot médian-de-trois — le pivot naïf (premier
//     élément) dégénère en O(n²) sur une séquence DÉJÀ TRIÉE, qui est
//     précisément le cas fréquent quand on trie des chemins de fichiers ou des
//     identifiants ;
//   - bascule en **tri par insertion** sous 16 éléments : plus rapide à cette
//     taille, et c'est là que la récursion coûterait plus que le tri ;
//   - **récursion bornée** sur la plus petite moitié, itération sur l'autre :
//     la profondeur de pile reste en O(log n) même dans le pire découpage.
//
// CE QU'IL NE FAIT PAS : il n'est **pas stable**. Deux éléments équivalents
// peuvent changer d'ordre relatif. C'est écrit ici parce qu'un tri instable
// employé là où l'on attendait la stabilité produit un défaut qui ne se voit
// qu'avec des doublons — donc tard, et rarement.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#ifndef __NKENTSEU_NKSORT_H__
#define __NKENTSEU_NKSORT_H__

#include "NKCore/NkTypes.h"
#include "NKCore/NkTraits.h"

namespace nkentseu {

	namespace detail {

		template <typename T> inline void NkSwapValeurs(T &a, T &b) noexcept {
			T tmp = traits::NkMove(a);
			a = traits::NkMove(b);
			b = traits::NkMove(tmp);
		}

		/// Tri par insertion — le meilleur choix en dessous de quelques dizaines
		/// d'éléments, et la brique sur laquelle le quicksort se replie.
		template <typename T, typename Compare>
		inline void NkTriInsertion(T *debut, T *fin, Compare avant) {
			for (T *i = debut + 1; i < fin; ++i) {
				T valeur = traits::NkMove(*i);
				T *j = i;
				while (j > debut && avant(valeur, *(j - 1))) {
					*j = traits::NkMove(*(j - 1));
					--j;
				}
				*j = traits::NkMove(valeur);
			}
		}

		/// Médiane de trois, placée en position `debut` : protège du pire cas
		/// sur une séquence déjà ordonnée.
		template <typename T, typename Compare>
		inline T &NkPivotMedian(T *debut, T *fin, Compare avant) {
			T *milieu = debut + (fin - debut) / 2;
			T *dernier = fin - 1;
			if (avant(*milieu, *debut))
				NkSwapValeurs(*milieu, *debut);
			if (avant(*dernier, *debut))
				NkSwapValeurs(*dernier, *debut);
			if (avant(*dernier, *milieu))
				NkSwapValeurs(*dernier, *milieu);
			NkSwapValeurs(*debut, *milieu);
			return *debut;
		}

	} // namespace detail

	/// Trie [debut, fin) en place selon `avant(a, b)` — vrai si `a` précède `b`.
	template <typename T, typename Compare> inline void NkSort(T *debut, T *fin, Compare avant) {
		constexpr isize kSeuilInsertion = 16;

		while (fin - debut > kSeuilInsertion) {
			T &pivot = detail::NkPivotMedian(debut, fin, avant);

			T *i = debut;
			T *j = fin;
			for (;;) {
				do {
					++i;
				} while (i < fin && avant(*i, pivot));
				do {
					--j;
				} while (avant(pivot, *j));
				if (i >= j)
					break;
				detail::NkSwapValeurs(*i, *j);
			}
			detail::NkSwapValeurs(*debut, *j);

			// Récursion sur la PLUS PETITE moitié, itération sur l'autre : la
			// pile reste en O(log n) même si les découpages sont déséquilibrés.
			if (j - debut < fin - (j + 1)) {
				NkSort(debut, j, avant);
				debut = j + 1;
			} else {
				NkSort(j + 1, fin, avant);
				fin = j;
			}
		}

		if (fin - debut > 1)
			detail::NkTriInsertion(debut, fin, avant);
	}

	/// Surcharge par défaut : ordre croissant via `operator<`.
	template <typename T> inline void NkSort(T *debut, T *fin) {
		NkSort(debut, fin, [](const T &a, const T &b) { return a < b; });
	}

} // namespace nkentseu

#endif // __NKENTSEU_NKSORT_H__
