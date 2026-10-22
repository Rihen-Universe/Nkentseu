// =============================================================================
// NKMath/NkLibm.h
// Les fonctions mathématiques de base, écrites pour Nkentseu, sans <math.h>.
//
// Rodolf (22/10/2026) : « penses-tu que tu pourras aussi faire de même pour la
// bibliothèque mathématique, avec sin, cos, etc. ? » (après NkSortie, qui
// remplace fputs et printf).
//
// Trois familles, trois façons de faire aussi bien que la libm :
//  1. EXACTES, par manipulation des bits ou par une instruction du processeur :
//     Fabs, Copysign, Trunc, Floor, Ceil, Round, Sqrt. Résultat identique à la
//     libm, au bit près.
//  2. sin, cos, tan : réduction de l'angle modulo pi/2, puis un polynôme sur
//     [-pi/4, pi/4]. Les noyaux et la réduction moyenne suivent fdlibm (Sun,
//     1993) par musl ; leur avis de licence est recopié dans NkLibm.cpp. Pour
//     les très grands angles (au-delà de 2^20 * pi/2), réduction EXACTE de
//     Payne et Hanek, écrite ici avec des entiers de 128 bits, sur 1 472 bits de
//     2/pi CALCULÉS en entiers exacts (formule de Machin), pas recopiés.
//  3. exp et log : réduction par ln 2, puis l'approximation de fdlibm.
//
// Les versions float32 passent par le calcul float64 : précision au moins égale
// à celle de sinf, expf... de la libm.
//
// Le banc contre la libm (écart en ULP, valeurs spéciales, temps) :
// NKMath/bancs/banc_libm.cpp.
//
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// DATE : 2026-10-22
// LICENCE : Proprietary - All Rights Reserved (see LICENSE) ; les parties issues
//           de fdlibm gardent leur avis (voir NkLibm.cpp)
// =============================================================================
#pragma once
#ifndef NK_MATH_NKLIBM_H_INCLUDED
#define NK_MATH_NKLIBM_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKMath/NkMathApi.h"

namespace nkentseu {
	namespace math {
		namespace libm {

			// ── 1. Exactes, EN LIGNE ici : comme les fonctions intégrées du
			// compilateur (fabs, sqrt), elles ne coûtent pas d'appel. ───────
			namespace detail {
				union Mot {
						float64 f;
						uint64 u;
				};
				inline uint64 Bits(float64 x) noexcept {
					Mot m;
					m.f = x;
					return m.u;
				}
				inline float64 DeBits(uint64 u) noexcept {
					Mot m;
					m.u = u;
					return m.f;
				}
			} // namespace detail

			inline float64 Fabs(float64 x) noexcept {
				return detail::DeBits(detail::Bits(x) & 0x7FFFFFFFFFFFFFFFull);
			}

			inline float64 Copysign(float64 x, float64 signe) noexcept {
				return detail::DeBits((detail::Bits(x) & 0x7FFFFFFFFFFFFFFFull) |
									  (detail::Bits(signe) & 0x8000000000000000ull));
			}

			inline float64 Trunc(float64 x) noexcept {
				// |x| < 2^52 : la conversion vers un entier de 64 bits tronque, en une
				// instruction (cvttsd2si), et l'aller-retour est exact. Au-delà, x est
				// déjà entier (ou infini, ou NaN). Copysign garde le zéro négatif.
				if (Fabs(x) < 4503599627370496.0)
					return Copysign((float64)(int64)x, x);
				return x;
			}

			// Floor, Ceil, Round SANS BRANCHEMENT : la comparaison devient 0 ou 1
			// (mesuré le 22/10 : avec un « ? : », Ceil et Round coûtaient trois fois
			// la libm sur des valeurs au hasard, le processeur se trompant de branche).
			inline float64 Floor(float64 x) noexcept {
				const float64 t = Trunc(x);
				return t - (float64)(t > x); // seul un négatif non entier descend ; -0 reste -0
			}

			inline float64 Ceil(float64 x) noexcept {
				const float64 t = Trunc(x);
				return Copysign(t + (float64)(t < x), x); // Copysign : ceil(-0,5) = -0
			}

			inline float64 Round(float64 x) noexcept { // la moitié s'éloigne de zéro, comme round()
				const float64 t = Trunc(x);
				// x - t est exact (t est x sans ses bits de fraction) : pas le piège
				// de floor(x + 0,5), faux pour 0,49999999999999994.
				return t + Copysign((float64)(Fabs(x - t) >= 0.5), x);
			}

			NKENTSEU_MATH_API float64 Sqrt(float64 x) noexcept;
			NKENTSEU_MATH_API float32 Sqrt(float32 x) noexcept;

			// ── 2. Trigonométrie (radians) ─────────────────────────────────
			NKENTSEU_MATH_API float64 Sin(float64 x) noexcept;
			NKENTSEU_MATH_API float64 Cos(float64 x) noexcept;
			NKENTSEU_MATH_API float64 Tan(float64 x) noexcept;
			NKENTSEU_MATH_API float32 Sin(float32 x) noexcept;
			NKENTSEU_MATH_API float32 Cos(float32 x) noexcept;
			NKENTSEU_MATH_API float32 Tan(float32 x) noexcept;

			// ── 3. Exponentielle et logarithme ─────────────────────────────
			NKENTSEU_MATH_API float64 Exp(float64 x) noexcept;
			NKENTSEU_MATH_API float64 Log(float64 x) noexcept;
			NKENTSEU_MATH_API float32 Exp(float32 x) noexcept;
			NKENTSEU_MATH_API float32 Log(float32 x) noexcept;

			/// x * 2^n, sans perte tant que le résultat est représentable.
			NKENTSEU_MATH_API float64 Scalbn(float64 x, int32 n) noexcept;

		} // namespace libm
	} // namespace math
} // namespace nkentseu

#endif // NK_MATH_NKLIBM_H_INCLUDED
