// =============================================================================
// NKMedia/Codecs/Opus/Silk/NkSilkMath.h
// -----------------------------------------------------------------------------
// Décodeur Opus/SILK (RFC 6716 §4.2) — primitives à virgule fixe (port fidèle de
// libopus silk/SigProc_FIX.h / macros.h). Réutilisées par toutes les briques SILK
// (gains, NLSF/LPC, LTP, excitation, synthèse). Sémantique EXACTE (bit-exact) :
// décalages arithmétiques, multiplications 16/32 bits, SMULWB via intermédiaire
// 64 bits. Zero-STL, nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace media {

		// Toutes les opérations reproduisent libopus (fixed-point). Regroupées en
		// méthodes statiques pour ne pas polluer le namespace (évite les collisions
		// avec les macros CELT/H.264).
		struct NkSilkMath {
				static constexpr int32 kInt32Max = 0x7FFFFFFF;

				static inline int32 LSHIFT(int32 a, int32 s) {
					return (int32)((uint32)a << s);
				}
				static inline int32 RSHIFT(int32 a, int32 s) {
					return a >> s;
				}
				static inline int32 ADD_RSHIFT32(int32 a, int32 b, int32 s) {
					return a + (b >> s);
				}
				static inline int32 MUL(int32 a, int32 b) {
					return a * b;
				}
				// silk_SMULWB : a (32b) × 16 bits bas de b, résultat >>16.
				static inline int32 SMULWB(int32 a, int32 b) {
					return (int32)(((int64)a * (int16)b) >> 16);
				}
				static inline int32 SMLAWB(int32 a, int32 b, int32 c) {
					return a + SMULWB(b, c);
				}
				// silk_SMULBB : 16 bits × 16 bits → 32.
				static inline int32 SMULBB(int32 a, int32 b) {
					return (int32)((int16)a) * (int32)((int16)b);
				}
				static inline int32 MLA(int32 a, int32 b, int32 c) {
					return a + b * c;
				}
				static inline int32 maxInt(int32 a, int32 b) {
					return a > b ? a : b;
				}
				static inline int32 minInt(int32 a, int32 b) {
					return a < b ? a : b;
				}
				static inline int32 min32(int32 a, int32 b) {
					return a < b ? a : b;
				}
				// silk_LIMIT_int(a, lo, hi) — cas lo <= hi (clamp).
				static inline int32 LIMIT_int(int32 a, int32 lo, int32 hi) {
					return maxInt(minInt(a, hi), lo);
				}

				// ── Primitives supplémentaires (NLSF/LPC) — port fidèle SigProc_FIX.h ──
				static constexpr int32 kInt32Min = (int32)(-2147483647 - 1);
				static constexpr int32 kInt16Max = 32767;
				static constexpr int32 kInt16Min = -32768;

				// SILK_FIX_CONST(C, Q) = round(C * 2^Q). Constante à virgule fixe.
				static constexpr int32 FIX_CONST(double c, int32 q) {
					return (int32)(c * (double)((int64)1 << q) + 0.5);
				}

				static inline int32 ADD32(int32 a, int32 b) {
					return a + b;
				}
				static inline int32 SUB32(int32 a, int32 b) {
					return a - b;
				}
				static inline int32 ADD16(int32 a, int32 b) {
					return a + b;
				}
				static inline int32 SUB16(int32 a, int32 b) {
					return a - b;
				}
				static inline int32 DIV32_16(int32 a, int32 b) {
					return (int32)(a / b);
				}
				static inline int32 DIV32(int32 a, int32 b) {
					return (int32)(a / b);
				}
				static inline int32 ADD_LSHIFT32(int32 a, int32 b, int32 s) {
					return a + LSHIFT(b, s);
				}
				static inline int64 RSHIFT64(int64 a, int32 s) {
					return a >> s;
				}
				static inline int64 SMULL(int32 a, int32 b) {
					return (int64)a * (int64)b;
				}
				static inline int32 SMMUL(int32 a, int32 b) {
					return (int32)(SMULL(a, b) >> 32);
				}
				static inline int32 SMULWW(int32 a, int32 b) {
					return (int32)(((int64)a * b) >> 16);
				}
				static inline int32 SMLAWW(int32 a, int32 b, int32 c) {
					return (int32)(a + (((int64)b * c) >> 16));
				}
				static inline int32 abs32(int32 a) {
					return a > 0 ? a : -a;
				}
				static inline int32 SAT16(int32 a) {
					return a > kInt16Max ? kInt16Max : (a < kInt16Min ? kInt16Min : a);
				}
				// silk_ADD_SAT16 : addition saturée dans le domaine int16.
				static inline int32 ADD_SAT16(int32 a, int32 b) {
					return SAT16(a + b);
				}
				// silk_SUB_SAT32 : soustraction saturée dans le domaine int32.
				static inline int32 SUB_SAT32(int32 a, int32 b) {
					int64 r = (int64)a - (int64)b;
					if (r > kInt32Max)
						r = kInt32Max;
					else if (r < kInt32Min)
						r = kInt32Min;
					return (int32)r;
				}
				// silk_LIMIT(a, l1, l2) général (l1 et l2 dans n'importe quel ordre).
				static inline int32 LIMIT(int32 a, int32 l1, int32 l2) {
					return l1 > l2 ? (a > l1 ? l1 : (a < l2 ? l2 : a)) : (a > l2 ? l2 : (a < l1 ? l1 : a));
				}
				// Décalage gauche saturé (borne l'entrée pour éviter l'overflow après <<).
				static inline int32 LSHIFT_SAT32(int32 a, int32 s) {
					return LSHIFT(LIMIT(a, RSHIFT(kInt32Min, s), RSHIFT(kInt32Max, s)), s);
				}
				// silk_RSHIFT_ROUND (arrondi au plus proche lors d'un décalage droit).
				static inline int32 RSHIFT_ROUND(int32 a, int32 s) {
					return s == 1 ? (a >> 1) + (a & 1) : ((a >> (s - 1)) + 1) >> 1;
				}
				static inline int64 RSHIFT_ROUND64(int64 a, int32 s) {
					return s == 1 ? (a >> 1) + (a & 1) : ((a >> (s - 1)) + 1) >> 1;
				}
				// MUL32_FRAC_Q(a, b, Q) = arrondi( a*b / 2^Q ) via produit 64 bits.
				static inline int32 MUL32_FRAC_Q(int32 a, int32 b, int32 Q) {
					return (int32)RSHIFT_ROUND64(SMULL(a, b), Q);
				}
				// silk_CLZ32 : nombre de bits de tête à zéro (0 → 32).
				static inline int32 CLZ32(int32 in32) {
					return in32 ? (int32)__builtin_clz((uint32)in32) : 32;
				}

				// ── Arithmétique à débordement autorisé (synthèse : RAND, filtres) ──
				static inline int32 ADD32_ovflw(int32 a, int32 b) {
					return (int32)((uint32)a + (uint32)b);
				}
				static inline int32 SUB32_ovflw(int32 a, int32 b) {
					return (int32)((uint32)a - (uint32)b);
				}
				static inline int32 MLA_ovflw(int32 a, int32 b, int32 c) {
					return ADD32_ovflw(a, (int32)((uint32)b * (uint32)c));
				}
				static inline int32 LSHIFT_ovflw(int32 a, int32 s) {
					return (int32)((uint32)a << s);
				}
				static inline int32 SMLABB_ovflw(int32 a, int32 b, int32 c) {
					return ADD32_ovflw(a, SMULBB(b, c));
				}
				// silk_ADD_SAT32 : addition saturée dans le domaine int32.
				static inline int32 ADD_SAT32(int32 a, int32 b) {
					int64 r = (int64)a + (int64)b;
					if (r > kInt32Max)
						r = kInt32Max;
					else if (r < kInt32Min)
						r = kInt32Min;
					return (int32)r;
				}
				// silk_RAND : LCG (RAND_INCREMENT + seed*RAND_MULTIPLIER, débordement ok).
				static inline int32 RAND(int32 seed) {
					return MLA_ovflw(907633515, seed, 196314165);
				}
				// silk_DIV32_varQ : approximation de (a32<<Qres)/b32.
				static int32 DIV32_varQ(int32 a32, int32 b32, int32 Qres);
				// silk_INVERSE32_varQ : approximation de (1<<Qres)/b32.
				static int32 INVERSE32_varQ(int32 b32, int32 Qres);

				// silk_log2lin : approximation de 2^() (inverse de silk_lin2log). Q7 → linéaire.
				static int32 log2lin(int32 inLog_Q7);
		};

	} // namespace media
} // namespace nkentseu
