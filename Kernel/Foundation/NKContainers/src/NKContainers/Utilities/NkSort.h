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
#include "NKMemory/NkAllocator.h"

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

	// -------------------------------------------------------------------------
	// TRI FUSION STABLE (04/11/2026)
	//
	// Principe (Knuth, TAOCP vol. 3, § 5.2.4). On coupe la plage en deux moitiés,
	// on trie chacune récursivement, puis on les FUSIONNE : on compare les têtes
	// des deux moitiés triées et on recopie la plus petite. Le coût vérifie
	// T(n) = 2 T(n/2) + O(n), soit O(n log n) comparaisons dans tous les cas, sans
	// pire cas quadratique (contrairement au quicksort de NkSort).
	//
	// Stabilité. Si deux éléments sont équivalents (ni a < b ni b < a), leur ordre
	// d'origine est conservé. À la fusion, l'élément de la moitié DROITE ne passe
	// devant celui de la moitié gauche que s'il lui est STRICTEMENT inférieur :
	// en cas d'égalité on prend la gauche, qui précédait déjà. C'est la propriété
	// dont le jeu a besoin pour l'ordre de dessin : deux objets à la même
	// profondeur ne doivent pas scintiller d'une image à l'autre.
	//
	// Mémoire. La fusion a besoin d'un tampon pour la moitié gauche (n/2 éléments
	// au plus). Il est pris UNE fois par NkGetDefaultAllocator, pas par appel
	// récursif. Les éléments sont déplacés (NkMove), jamais copiés : le tri
	// accepte donc les types qu'on ne peut que déplacer.
	// Sous 16 éléments on garde le tri par insertion, déjà stable (comparaison
	// stricte), comme NkSort.
	// -------------------------------------------------------------------------
	namespace detail {

		/// Fusionne [debut, milieu) et [milieu, fin), déjà triées, avec le tampon.
		template <typename T, typename Compare>
		inline void NkFusionStable(T *debut, T *milieu, T *fin, T *tampon, Compare avant) {
			const isize n = milieu - debut;
			for (isize k = 0; k < n; ++k) {
				new (tampon + k) T(traits::NkMove(debut[k]));
			}
			T *g = tampon;			// tête de la moitié gauche (dans le tampon)
			T *gFin = tampon + n;
			T *d = milieu;			// tête de la moitié droite (en place)
			T *sortie = debut;
			while (g < gFin && d < fin) {
				if (avant(*d, *g)) {	// droite STRICTEMENT avant gauche seulement
					*sortie++ = traits::NkMove(*d++);
				} else {
					*sortie++ = traits::NkMove(*g++);
				}
			}
			while (g < gFin) {
				*sortie++ = traits::NkMove(*g++);
			}
			for (isize k = 0; k < n; ++k) {
				tampon[k].~T();
			}
		}

		template <typename T, typename Compare>
		inline void NkTriFusionRec(T *debut, T *fin, T *tampon, Compare avant) {
			if (fin - debut <= 16) {
				if (fin - debut > 1) {
					NkTriInsertion(debut, fin, avant);
				}
				return;
			}
			T *milieu = debut + (fin - debut) / 2;
			NkTriFusionRec(debut, milieu, tampon, avant);
			NkTriFusionRec(milieu, fin, tampon, avant);
			// Déjà en ordre : rien à fusionner (cas fréquent sur données presque triées).
			if (!avant(*milieu, *(milieu - 1))) {
				return;
			}
			NkFusionStable(debut, milieu, fin, tampon, avant);
		}

	} // namespace detail

	/// Trie [debut, fin) en place, de façon STABLE, selon `avant(a, b)`.
	template <typename T, typename Compare> inline void NkStableSort(T *debut, T *fin, Compare avant) {
		const isize n = fin - debut;
		if (n < 2) {
			return;
		}
		if (n <= 16) {
			detail::NkTriInsertion(debut, fin, avant);
			return;
		}
		memory::NkAllocator &alloc = memory::NkGetDefaultAllocator();
		T *tampon = static_cast<T *>(alloc.Allocate(static_cast<memory::NkAllocator::SizeType>((n / 2 + 1) * static_cast<isize>(sizeof(T))),
													 static_cast<memory::NkAllocator::AlignType>(alignof(T))));
		detail::NkTriFusionRec(debut, fin, tampon, avant);
		alloc.Deallocate(tampon);
	}

	/// Surcharge par défaut : ordre croissant via `operator<`.
	template <typename T> inline void NkStableSort(T *debut, T *fin) {
		NkStableSort(debut, fin, [](const T &a, const T &b) { return a < b; });
	}

} // namespace nkentseu

#endif // __NKENTSEU_NKSORT_H__
