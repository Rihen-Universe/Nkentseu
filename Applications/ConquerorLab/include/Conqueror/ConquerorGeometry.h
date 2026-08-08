#pragma once
// =============================================================================
// ConquerorGeometry.h — geometrie de grille, LOGIQUE PURE, header-only.
//
// Inclus par les modules de regles ET par l'atelier. C'est DELIBEREMENT le meme
// code des deux cotes : une distance calculee differemment ici et la casserait
// le determinisme (REGLES §17.4).
//
// CE FICHIER NE DEPEND QUE DE ConquerorRulesABI.h.
// Aucune dependance a NKMath : tout y est inline et entier, donc un module
// compile par l'atelier n'a AUCUN symbole a lier. La projection ecran
// (cos/sin/arrondi) vit ailleurs — c'est de la presentation, pas de la regle :
// voir ConquerorLab/NkcBoardRender.h.
//
// Conventions :
//   - Hexagone : coordonnees AXIALES (q, r). Cube implicite x=q, z=r, y=-x-z.
//   - Carre    : (q, r) interpretes comme (x, y).
// =============================================================================

#include "Conqueror/ConquerorRulesABI.h"

namespace nkentseu {
	namespace conqueror {

		/// Nombre de voisins d'une topologie.
		inline int32 NeighborCount(NkcTopology t) noexcept {
			switch (t) {
				case NkcTopology::HexPointy:
				case NkcTopology::HexFlat: return 6;
				case NkcTopology::Square4: return 4;
				case NkcTopology::Square8: return 8;
			}
			return 0;
		}

		/// i-eme voisin de `c`. L'ORDRE EST NORMATIF : il fixe l'ordre de
		/// generation des coups, donc la reproductibilite. Ne jamais le changer.
		inline NkcCoord Neighbor(NkcTopology t, NkcCoord c, int32 i) noexcept {
			static const int8 kHex[6][2] = {{+1, 0}, {+1, -1}, {0, -1},
											{-1, 0}, {-1, +1}, {0, +1}};
			static const int8 kSq4[4][2] = {{+1, 0}, {0, -1}, {-1, 0}, {0, +1}};
			static const int8 kSq8[8][2] = {{+1, 0}, {+1, -1}, {0, -1}, {-1, -1},
											{-1, 0}, {-1, +1}, {0, +1}, {+1, +1}};
			const int8 *d = nullptr;
			switch (t) {
				case NkcTopology::HexPointy:
				case NkcTopology::HexFlat:
					if (i < 0 || i >= 6) return c;
					d = kHex[i];
					break;
				case NkcTopology::Square4:
					if (i < 0 || i >= 4) return c;
					d = kSq4[i];
					break;
				case NkcTopology::Square8:
					if (i < 0 || i >= 8) return c;
					d = kSq8[i];
					break;
				default: return c;
			}
			NkcCoord out;
			out.q = static_cast<int16>(c.q + d[0]);
			out.r = static_cast<int16>(c.r + d[1]);
			return out;
		}

		inline bool CoordEqual(NkcCoord a, NkcCoord b) noexcept {
			return a.q == b.q && a.r == b.r;
		}

		/// Distance sur le graphe de voisinage. Entiere, donc exacte partout.
		inline int32 Distance(NkcTopology t, NkcCoord a, NkcCoord b) noexcept {
			const int32 dq = static_cast<int32>(a.q) - static_cast<int32>(b.q);
			const int32 dr = static_cast<int32>(a.r) - static_cast<int32>(b.r);
			const int32 aq = dq < 0 ? -dq : dq;
			const int32 ar = dr < 0 ? -dr : dr;
			switch (t) {
				case NkcTopology::HexPointy:
				case NkcTopology::HexFlat: {
					const int32 s  = dq + dr;
					const int32 as = s < 0 ? -s : s;
					return (aq + ar + as) / 2;			 // distance hexagonale
				}
				case NkcTopology::Square4: return aq + ar;			  // Manhattan
				case NkcTopology::Square8: return aq > ar ? aq : ar;	  // Tchebychev
			}
			return 0;
		}

	} // namespace conqueror
} // namespace nkentseu
