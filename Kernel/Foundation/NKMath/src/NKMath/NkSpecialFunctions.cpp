// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NKMath/NkSpecialFunctions.cpp (voir NkSpecialFunctions.h)
//
// (19/10/2026) Trois regimes, tous en double precision :
//   |x| < 0,5     : la serie entiere de erf (une douzaine de termes) ;
//   0,5 <= x < 9  : le developpement de Taylor autour du point de grille le plus proche (pas de 1/32) :
//                   erfc(a + h) = erfc(a) + 2/√π e^(-a²) Σ_{n>=1} (-1)^n H_{n-1}(a) h^n / n!, ou H sont les
//                   polynomes d'Hermite (H0 = 1, H1 = 2a, H_{n+1} = 2a H_n - 2n H_{n-1}) ; |h| <= 1/64,
//                   14 termes suffisent jusqu'a 9 (le reste est sous 1e-18 en relatif). La table (erfc(a) et
//                   e^(-a²) aux points de grille) est calculee une fois, au premier appel : la serie pour a < 1,
//                   la fraction continue au-dela (exactes, mais lentes : c'est pourquoi elles ne servent qu'a la
//                   table) ;
//   x >= 9        : la fraction continue (Lentz), qui y converge en une dizaine d'etages.
// Avant la table (premiere version du 19/10) : 478 ns par appel contre 57 pour la bibliotheque standard.
// =============================================================================

#include "pch.h"

#include "NKMath/NkSpecialFunctions.h"
#include "NKMath/NkFunctions.h"

namespace nkentseu {
	namespace math {

		namespace {
			constexpr float64 kDeuxSurRacinePi = 1.1283791670955126; // 2/√π
			constexpr float64 kRacinePi = 1.7724538509055160;		  // √π
			constexpr int32 kParUnite = 32;							  // le pas de la grille : 1/32
			constexpr int32 kPremier = 16;							  // 0,5
			constexpr int32 kDernier = 288;							  // 9
			constexpr int32 kTermes = 14;

			float64 Abs(float64 v) noexcept {
				return v < 0.0 ? -v : v;
			}

			/// erf(z) par la serie entiere (0 <= z < 2), arret a 1e-17 pres en relatif.
			float64 ErfSerie(float64 z) noexcept {
				if (z == 0.0) {
					return 0.0;
				}
				float64 somme = 0.0;
				float64 p = z; // (-1)^n z^(2n+1) / n!
				const float64 z2 = z * z;
				for (int32 n = 0; n < 200; ++n) {
					const float64 t = p / (float64)(2 * n + 1);
					somme += t;
					if (Abs(t) < 1e-17 * Abs(somme)) {
						break;
					}
					p *= -z2 / (float64)(n + 1);
				}
				return kDeuxSurRacinePi * somme;
			}

			/// erfc(z) par la fraction continue (Lentz), pour z >= 1 (lente pres de 1 : la table seulement).
			float64 ErfcFraction(float64 z, int32 etages) noexcept {
				const float64 petit = 1e-300;
				float64 f = z;
				float64 c = f;
				float64 d = 0.0;
				for (int32 k = 1; k < etages; ++k) {
					const float64 a = 0.5 * (float64)k;
					d = z + a * d;
					d = Abs(d) < petit ? petit : d;
					c = z + a / c;
					c = Abs(c) < petit ? petit : c;
					d = 1.0 / d;
					const float64 delta = c * d;
					f *= delta;
					if (Abs(delta - 1.0) < 4e-16) { // a deux ulp de 1 : la fraction ne bouge plus
						break;
					}
				}
				return NkExp(-z * z) / (kRacinePi * f);
			}

			struct Table {
					float64 erfc[kDernier + 1];
					float64 gauss[kDernier + 1]; ///< e^(-a²)
					Table() noexcept {
						for (int32 k = 0; k <= kDernier; ++k) {
							const float64 a = (float64)k / (float64)kParUnite;
							erfc[k] = k < kPremier ? 0.0 : (a < 1.0 ? 1.0 - ErfSerie(a) : ErfcFraction(a, 100000));
							gauss[k] = NkExp(-a * a);
						}
					}
			};

			const Table &LaTable() noexcept {
				static const Table t;
				return t;
			}

			/// erfc(x) pour 0,5 <= x < 9 - 1/64, par Taylor autour du point de grille.
			float64 ErfcGrille(float64 x) noexcept {
				const Table &t = LaTable();
				const int32 k = (int32)(x * (float64)kParUnite + 0.5);
				const float64 a = (float64)k / (float64)kParUnite;
				const float64 h = x - a;
				const float64 deuxA = 2.0 * a;
				float64 hPrec = 0.0; // H_{n-2}
				float64 hCour = 1.0; // H_{n-1}
				float64 puissance = 1.0; // h^n / n!
				float64 somme = 0.0;
				float64 signe = -1.0;
				for (int32 n = 1; n <= kTermes; ++n) {
					puissance *= h / (float64)n;
					somme += signe * hCour * puissance;
					// H_n = 2a H_{n-1} - 2 (n-1) H_{n-2}
					const float64 hSuiv = deuxA * hCour - 2.0 * (float64)(n - 1) * hPrec;
					hPrec = hCour;
					hCour = hSuiv;
					signe = -signe;
				}
				return t.erfc[k] + kDeuxSurRacinePi * t.gauss[k] * somme;
			}
		} // namespace

		float64 NkErfc(float64 x) noexcept {
			if (x != x) {
				return x; // NaN
			}
			if (x < 0.0) {
				return 2.0 - NkErfc(-x);
			}
			if (x < 0.5) {
				return 1.0 - ErfSerie(x);
			}
			if (x < 9.0 - 1.0 / 64.0) {
				return ErfcGrille(x);
			}
			if (x > 27.3) {
				return 0.0; // sous le plus petit double
			}
			return ErfcFraction(x, 500);
		}

		float64 NkErf(float64 x) noexcept {
			if (x != x) {
				return x;
			}
			if (x < 0.0) {
				return -NkErf(-x);
			}
			return x < 0.5 ? ErfSerie(x) : 1.0 - NkErfc(x);
		}

		float32 NkErf(float32 x) noexcept {
			return (float32)NkErf((float64)x);
		}

		float32 NkErfc(float32 x) noexcept {
			return (float32)NkErfc((float64)x);
		}

	} // namespace math
} // namespace nkentseu
