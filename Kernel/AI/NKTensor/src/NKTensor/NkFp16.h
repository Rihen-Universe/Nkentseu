// =============================================================================
// NkFp16.h — demi-précision (IEEE 754 binary16) logicielle, CPU (NKAI mixed
// precision, brique 1/3).
//
// From-scratch : conversions f32<->f16 par manipulation de bits (aucune
// dépendance matérielle _Float16 / __fp16 / intrinsèques F16C — portable,
// pédagogique, correcte). Gère : arrondi au plus proche pair (round-to-nearest-
// even), dénormaux (des DEUX côtés : float dénormalisé -> demi-normal/zéro ET
// demi dénormalisé -> float normalisé), +-0, +-Inf, NaN (silencieuse, mantisse
// jamais nulle pour rester NaN).
//
// `NkFp16` est un type scalaire complet (arithmétique via aller-retour float)
// utilisable dans NK_DTYPE_DISPATCH (cf NkDType.h) exactement comme float/double :
// stockage 2 octets, calcul de RÉFÉRENCE en convertissant vers float32.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include <cstring> // memcpy (bit-cast portable, pas de type-punning UB)

namespace nkentseu {
	namespace ai {

		// -------------------------------------------------------------------------
		// Conversions bas niveau (bits <-> bits), utilisables indépendamment du
		// wrapper NkFp16 (ex: packing GPU packHalf2x16-like côté CPU).
		// -------------------------------------------------------------------------

		// float32 -> bits binary16 (round-to-nearest-even, gère Inf/NaN/dénormaux).
		inline uint16 NkF32ToF16Bits(float value) {
			uint32 x;
			std::memcpy(&x, &value, sizeof(x));

			const uint32 sign = (x >> 16) & 0x8000u;
			const uint32 exp32 = (x >> 23) & 0xFFu;
			const uint32 mant32 = x & 0x007FFFFFu;

			// Inf / NaN (exposant float plein).
			if (exp32 == 0xFFu) {
				if (mant32 == 0)
					return (uint16)(sign | 0x7C00u); // +-Inf
				uint32 m = mant32 >> 13;
				if (m == 0)
					m = 1; // ne jamais retomber sur Inf : NaN garde une mantisse non nulle
				return (uint16)(sign | 0x7C00u | m);
			}

			// Exposant rebiaisé pour l'espace demi-précision (biais 15 au lieu de 127).
			const int32 e = (int32)exp32 - 127 + 15;

			if (e <= 0) {
				// Sous le plus petit normal demi : zéro ou dénormal demi.
				if (e < -10)
					return (uint16)sign; // trop petit, même arrondi -> 0 signé

				const uint32 mfull = mant32 | 0x00800000u; // 24 bits (bit implicite restauré)
				const uint32 shift = (uint32)(14 - e);		// 14..24
				uint32 halfMantissa = mfull >> shift;
				const uint32 dropped = mfull & ((1u << shift) - 1u);
				const uint32 halfway = 1u << (shift - 1);
				// Round-to-nearest-even sur les bits jetés.
				if (dropped > halfway || (dropped == halfway && (halfMantissa & 1u)))
					++halfMantissa;
				if (halfMantissa == 0x0400u) // débordement -> devient le plus petit NORMAL demi
					return (uint16)(sign | 0x0400u);
				return (uint16)(sign | halfMantissa); // exposant champ = 0 (dénormal ou zéro)
			}

			if (e >= 31)
				return (uint16)(sign | 0x7C00u); // dépassement -> Inf

			// Cas normal : arrondir la mantisse 23 bits -> 10 bits (RNE).
			uint32 halfMantissa = mant32 >> 13;
			const uint32 dropped = mant32 & 0x1FFFu;
			const uint32 halfway = 0x1000u;
			uint32 expOut = (uint32)e;
			if (dropped > halfway || (dropped == halfway && (halfMantissa & 1u))) {
				++halfMantissa;
				if (halfMantissa == 0x0400u) { // la mantisse déborde -> incrémente l'exposant
					halfMantissa = 0;
					++expOut;
					if (expOut >= 31)
						return (uint16)(sign | 0x7C00u); // dépassement -> Inf
				}
			}
			return (uint16)(sign | (expOut << 10) | halfMantissa);
		}

		// bits binary16 -> float32 (exact : toute valeur binary16, y compris les
		// dénormaux, est représentable SANS perte en binary32).
		inline float NkF16BitsToF32(uint16 h) {
			const uint32 sign = (uint32)(h & 0x8000u) << 16;
			const uint32 exp = (h >> 10) & 0x1Fu;
			const uint32 mant = h & 0x03FFu;
			uint32 bits;

			if (exp == 0) {
				if (mant == 0) {
					bits = sign; // +-0
				} else {
					// Dénormal demi -> normaliser vers binary32 (décalage jusqu'au bit 10).
					uint32 m = mant;
					int32 shift = 0;
					while ((m & 0x0400u) == 0u) {
						m <<= 1;
						++shift;
					}
					m &= 0x03FFu;
					const int32 exp32 = 113 - shift; // biais : 127-15-shift+1
					bits = sign | ((uint32)exp32 << 23) | (m << 13);
				}
			} else if (exp == 0x1Fu) {
				bits = sign | 0x7F800000u | (mant << 13); // Inf (mant=0) ou NaN
			} else {
				const int32 exp32 = (int32)exp - 15 + 127;
				bits = sign | ((uint32)exp32 << 23) | (mant << 13);
			}

			float f;
			std::memcpy(&f, &bits, sizeof(f));
			return f;
		}

		// -------------------------------------------------------------------------
		// NkFp16 — type scalaire demi-précision (référence CPU : calcule toujours
		// en passant par float, puis rearrondit). Utilisable dans NK_DTYPE_DISPATCH
		// (NkDType.h) comme n'importe quel type flottant du tenseur.
		// -------------------------------------------------------------------------
		struct NkFp16 {
				uint16 bits = 0;

				NkFp16() = default;
				// Constructeur convertisseur UNIQUE (évite toute ambiguïté de surcharge
				// sur les casts fonctionnels `(T)v` utilisés par NK_DTYPE_DISPATCH).
				NkFp16(float f) : bits(NkF32ToF16Bits(f)) {
				}

				operator float() const {
					return NkF16BitsToF32(bits);
				}

				operator double() const {
					return (double)NkF16BitsToF32(bits);
				}

				static NkFp16 FromBits(uint16 b) {
					NkFp16 h;
					h.bits = b;
					return h;
				}
		};

		inline NkFp16 operator+(NkFp16 a, NkFp16 b) {
			return NkFp16((float)a + (float)b);
		}

		inline NkFp16 operator-(NkFp16 a, NkFp16 b) {
			return NkFp16((float)a - (float)b);
		}

		inline NkFp16 operator*(NkFp16 a, NkFp16 b) {
			return NkFp16((float)a * (float)b);
		}

		inline NkFp16 operator/(NkFp16 a, NkFp16 b) {
			return NkFp16((float)a / (float)b);
		}

		inline NkFp16 operator-(NkFp16 a) {
			return NkFp16(-(float)a);
		}

		inline bool operator==(NkFp16 a, NkFp16 b) {
			return (float)a == (float)b;
		}

		inline bool operator!=(NkFp16 a, NkFp16 b) {
			return (float)a != (float)b;
		}

		inline bool operator<(NkFp16 a, NkFp16 b) {
			return (float)a < (float)b;
		}

		inline bool operator>(NkFp16 a, NkFp16 b) {
			return (float)a > (float)b;
		}

		// Vrai si Inf ou NaN (test direct sur le champ exposant : évite un aller-retour float).
		inline bool NkFp16IsInfOrNan(NkFp16 h) {
			return ((h.bits >> 10) & 0x1Fu) == 0x1Fu;
		}

		inline bool NkFp16IsNan(NkFp16 h) {
			return ((h.bits >> 10) & 0x1Fu) == 0x1Fu && (h.bits & 0x03FFu) != 0u;
		}

	} // namespace ai
} // namespace nkentseu
