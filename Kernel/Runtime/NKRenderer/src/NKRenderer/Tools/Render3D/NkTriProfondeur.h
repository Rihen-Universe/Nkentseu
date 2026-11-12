// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// -----------------------------------------------------------------------------
// NKRenderer/Tools/Render3D/NkTriProfondeur.h : le tri des dessins par
// profondeur, STABLE et en temps lineaire, sans la bibliotheque standard.
//
// POURQUOI (sim-cabine, 12/11/2026). NkRender3D::SortDrawCalls triait mOpaque
// et mTransparent par INSERTION, en recopiant des SortedDC entiers (un
// NkDrawCall3D d'environ 400 octets, avec un NkVector) a chaque decalage :
// O(n2) recopies lourdes. Mesure dans le demonstrateur CAMRAIL : 29 pieces de
// cabine (les plus proches) soumises APRES ~430 pieces de decor remontaient
// chacune toute la liste, ~2,5 ms par image.
//
// CE QUI EST TRIE : des CLES de 32 bits (la profondeur rendue triable en
// entier) et des INDICES, jamais les dessins. Tri par base 256 (4 passes,
// LSD) : chaque passe est stable, l'ordre d'arrivee departage donc les
// profondeurs egales EXACTEMENT comme le faisait l'insertion (comparaison
// stricte). Les dessins sont ensuite permutes une seule fois (cycles).
//
// Les NaN : l'insertion les traitait a sa facon (une comparaison fausse les
// laisse en place) ; NkTriStableProfondeurs refuse une liste qui en contient
// et l'appelant garde alors l'ancien tri (meme resultat qu'avant, au bit pres).
// -----------------------------------------------------------------------------
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace renderer {

		/// La cle d'une profondeur : l'ordre des entiers non signes est celui des
		/// flottants (finis ou infinis) ; -0 et +0 donnent la meme cle (egaux pour <).
		inline uint32 NkCleProfondeur(float32 f) {
			union {
					float32 f;
					uint32 u;
			} v;
			v.f = f;
			if ((v.u & 0x7FFFFFFFu) == 0u) {
				return 0x80000000u; // -0 comme +0
			}
			return (v.u & 0x80000000u) ? ~v.u : (v.u | 0x80000000u);
		}

		inline bool NkEstNaN(float32 f) {
			union {
					float32 f;
					uint32 u;
			} v;
			v.f = f;
			return (v.u & 0x7F800000u) == 0x7F800000u && (v.u & 0x007FFFFFu) != 0u;
		}

		/// Tri STABLE de n profondeurs : rend dans `ordre` les indices d'origine,
		/// du premier dessine au dernier (croissant ; decroissant pour les
		/// transparents). Quatre tampons de n entiers sont fournis par l'appelant.
		/// Rend faux (et ne trie rien) si une profondeur est NaN.
		/// `instable` (bancs seulement) : contre-epreuve, les egalites sont
		/// departagees a l'envers ; le banc d'empreinte DOIT alors rougir.
		inline bool NkTriStableProfondeurs(const float32 *prof, uint32 n, bool decroissant, uint32 *ordre, uint32 *cles,
										   uint32 *tmpOrdre, uint32 *tmpCles, bool instable = false) {
			for (uint32 i = 0; i < n; ++i) {
				if (NkEstNaN(prof[i])) {
					return false;
				}
			}
			for (uint32 i = 0; i < n; ++i) {
				const uint32 src = instable ? n - 1u - i : i;
				const uint32 c = NkCleProfondeur(prof[src]);
				cles[i] = decroissant ? ~c : c;
				ordre[i] = src;
			}
			uint32 *ca = cles, *oa = ordre, *cb = tmpCles, *ob = tmpOrdre;
			for (uint32 passe = 0; passe < 4; ++passe) {
				const uint32 decal = passe * 8u;
				uint32 compte[257];
				for (uint32 k = 0; k < 257; ++k) {
					compte[k] = 0u;
				}
				for (uint32 i = 0; i < n; ++i) {
					++compte[((ca[i] >> decal) & 0xFFu) + 1u];
				}
				// Un octet partout le meme : la passe ne changerait rien.
				bool inutile = false;
				for (uint32 k = 1; k <= 256; ++k) {
					if (compte[k] == n) {
						inutile = true;
						break;
					}
				}
				if (inutile) {
					continue;
				}
				for (uint32 k = 1; k <= 256; ++k) {
					compte[k] += compte[k - 1];
				}
				for (uint32 i = 0; i < n; ++i) {
					const uint32 b = (ca[i] >> decal) & 0xFFu;
					const uint32 j = compte[b]++;
					cb[j] = ca[i];
					ob[j] = oa[i];
				}
				uint32 *t = ca;
				ca = cb;
				cb = t;
				t = oa;
				oa = ob;
				ob = t;
			}
			if (oa != ordre) {
				for (uint32 i = 0; i < n; ++i) {
					ordre[i] = oa[i];
					cles[i] = ca[i];
				}
			}
			return true;
		}

		/// L'ANCIEN tri (insertion, comparaison stricte), sur les seules profondeurs :
		/// la reference des bancs, et le repli quand une profondeur est NaN.
		inline void NkTriInsertionReference(const float32 *prof, uint32 n, bool decroissant, uint32 *ordre) {
			for (uint32 i = 0; i < n; ++i) {
				ordre[i] = i;
			}
			for (uint32 i = 1; i < n; ++i) {
				const uint32 cle = ordre[i];
				int32 j = (int32)i - 1;
				while (j >= 0 && (decroissant ? prof[ordre[j]] < prof[cle] : prof[ordre[j]] > prof[cle])) {
					ordre[j + 1] = ordre[j];
					j--;
				}
				ordre[j + 1] = cle;
			}
		}

		/// Vrai si l'insertion ne deplacerait rien : aucune paire VOISINE inversee
		/// (l'insertion ne deplace un element que si son voisin de gauche le depasse).
		inline bool NkDejaTrie(const float32 *prof, uint32 n, bool decroissant) {
			for (uint32 i = 1; i < n; ++i) {
				if (decroissant ? prof[i - 1] < prof[i] : prof[i - 1] > prof[i]) {
					return false;
				}
			}
			return true;
		}


		/// Applique un ordre : la position j recoit l'element ordre[j]. Par cycles,
		/// chaque element est DEPLACE une fois, jamais recopie. `ordre` est consomme
		/// (rendu a l'identite).
		template <typename T> inline void NkPermuterParCycles(T *elems, uint32 *ordre, uint32 n) {
			for (uint32 i = 0; i < n; ++i) {
				if (ordre[i] == i) {
					continue;
				}
				T tmp = static_cast<T &&>(elems[i]);
				uint32 j = i;
				for (;;) {
					const uint32 k = ordre[j];
					ordre[j] = j;
					if (k == i) {
						elems[j] = static_cast<T &&>(tmp);
						break;
					}
					elems[j] = static_cast<T &&>(elems[k]);
					j = k;
				}
			}
		}

		/// Le tri d'une file de dessins (elements a champ `depth`), tel que
		/// NkRender3D::SortDrawCalls l'appelle, et tel que les bancs le mesurent :
		///  - deja dans l'ordre (second appel d'une image) : rien ne bouge ;
		///  - sinon cles stables (repli : l'ancien tri si NaN), puis permutation.
		/// `prof` et `tampon` sont des tampons de travail (redimensionnes ici).
		/// `rappel` (facultatif) voit les profondeurs et l'ordre avant la permutation.
		/// Rend vrai si la file a ete permutee.
		template <typename T>
		inline bool NkTrierParProfondeur(T *elems, uint32 n, bool decroissant, NkVector<float32> &prof,
										 NkVector<uint32> &tampon,
										 void (*rappel)(const float32 *, const uint32 *, uint32, void *) = nullptr,
										 void *donnee = nullptr, bool instable = false) {
			if (n < 2u) {
				return false;
			}
			prof.Resize(n);
			for (uint32 i = 0; i < n; ++i) {
				prof[i] = elems[i].depth;
			}
			if (NkDejaTrie(prof.Data(), n, decroissant)) {
				return false;
			}
			tampon.Resize((usize)n * 4u);
			uint32 *ordre = tampon.Data(), *cles = ordre + n, *tmpOrdre = cles + n, *tmpCles = tmpOrdre + n;
			if (!NkTriStableProfondeurs(prof.Data(), n, decroissant, ordre, cles, tmpOrdre, tmpCles, instable)) {
				NkTriInsertionReference(prof.Data(), n, decroissant, ordre); // une profondeur NaN
			}
			if (rappel != nullptr) {
				rappel(prof.Data(), ordre, n, donnee);
			}
			NkPermuterParCycles(elems, ordre, n);
			return true;
		}

	} // namespace renderer
} // namespace nkentseu
