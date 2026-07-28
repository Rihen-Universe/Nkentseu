// =============================================================================
// NKMedia/Codecs/Video/AV1/NkAv1Symbol.h
// -----------------------------------------------------------------------------
// Décodeur ARITHMÉTIQUE MULTI-SYMBOLE d'AV1 (§8.2 de la spec), + lecteur de bits
// pour les en-têtes (§4, §8.1). ÉCRIT DEPUIS LA SPEC — aucun code tiers.
//
// Le cœur entropique d'AV1 : contrairement au bool coder binaire de VP8/VP9, AV1
// décode des symboles à N valeurs via une CDF (fonction de répartition cumulée sur
// 15 bits), avec ADAPTATION de la CDF après chaque symbole (§8.3.2). Deux registres :
//   SymbolRange  (rng) : plage courante, dans [2^14, 2^15].
//   SymbolValue  (dif) : différence courante, stockée INVERSÉE (complément).
// Constantes normatives : EC_PROB_SHIFT = 6, EC_MIN_PROB = 4.
//
// Une CDF de N symboles est un tableau de N+1 entiers : cdf[0..N-2] = valeurs
// cumulées (cdf[i] = P(symbole <= i) * 2^15), cdf[N-1] vaut 2^15 (implicite dans le
// calcul), et cdf[N] = compteur d'adaptation (nb de fois adaptée, plafonné). On
// stocke donc N+1 uint16 ; les fonctions reçoivent nsymbs = N.
//
// AUTEUR : Rihen — from-scratch depuis "AV1 Bitstream & Decoding Process Spec".
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace media {

		// FloorLog2(x) pour x >= 1 : indice du bit de poids fort (§4.7).
		static inline int32 NkAv1FloorLog2(uint32 x) {
			int32 s = 0;
			while (x != 0) {
				x >>= 1;
				++s;
			}
			return s - 1;
		}

		// =====================================================================
		// Lecteur de bits big-endian pour les en-têtes (descripteurs f(n), uvlc,
		// le(n), leb128, su(n), ns(n) — §4, §8.1). MSB d'abord.
		// =====================================================================
		struct NkAv1BitReader {
				const uint8 *data = nullptr;
				usize size = 0;
				usize bitPos = 0;
				bool error = false;

				NkAv1BitReader() = default;
				NkAv1BitReader(const uint8 *d, usize s) : data(d), size(s) {}

				// f(n) : n bits non signés, MSB d'abord.
				uint32 F(int32 n) {
					uint32 v = 0;
					for (int32 i = 0; i < n; ++i) {
						const usize byteIdx = bitPos >> 3;
						if (byteIdx >= size) {
							error = true;
							return v << (n - i); // padding logique
						}
						const int32 b = (data[byteIdx] >> (7 - (bitPos & 7))) & 1;
						++bitPos;
						v = (v << 1) | (uint32)b;
					}
					return v;
				}
				int32 Bit() { return (int32)F(1); }

				// su(n) : entier signé — magnitude sur n bits puis complément (§4.10.6).
				int32 Su(int32 n) {
					int32 value = (int32)F(n);
					const int32 signMask = 1 << (n - 1);
					if (value & signMask)
						value = value - 2 * signMask;
					return value;
				}

				// ns(n) : entier non signé à taille non uniforme (§4.10.7).
				uint32 Ns(uint32 n) {
					const int32 w = NkAv1FloorLog2(n) + 1;
					const uint32 m = (1u << w) - n;
					const uint32 v = F(w - 1);
					if (v < m)
						return v;
					const uint32 extraBit = F(1);
					return (v << 1) - m + extraBit;
				}

				// uvlc() : code universel à longueur variable (§4.10.3).
				uint32 Uvlc() {
					int32 leadingZeros = 0;
					while (true) {
						const int32 done = Bit();
						if (done)
							break;
						++leadingZeros;
						if (leadingZeros >= 32 || error)
							break;
					}
					if (leadingZeros >= 32)
						return 0xFFFFFFFFu;
					const uint32 value = F(leadingZeros);
					return value + (1u << leadingZeros) - 1;
				}

				// le(n) : n octets little-endian, à un bord d'octet (§4.10.4).
				uint32 Le(int32 nBytes) {
					uint32 t = 0;
					for (int32 i = 0; i < nBytes; ++i) {
						const uint32 byte = F(8);
						t += byte << (i * 8);
					}
					return t;
				}

				// Alignement sur l'octet supérieur (byte_alignment, §5.3.5).
				void ByteAlign() {
					while ((bitPos & 7) != 0)
						F(1);
				}

				usize BytePos() const { return (bitPos + 7) >> 3; }
				bool ByteAligned() const { return (bitPos & 7) == 0; }
		};

		// LEB128 (§4.10.5) : renvoie la valeur, avance *bytesRead. Max 8 octets.
		static inline uint64 NkAv1Leb128(const uint8 *data, usize size, usize *bytesRead) {
			uint64 value = 0;
			usize i = 0;
			for (; i < 8; ++i) {
				if (i >= size) {
					if (bytesRead)
						*bytesRead = i;
					return value; // tronqué
				}
				const uint8 b = data[i];
				value |= (uint64)(b & 0x7F) << (i * 7);
				if (!(b & 0x80)) {
					++i;
					break;
				}
			}
			if (bytesRead)
				*bytesRead = i;
			return value;
		}

		// =====================================================================
		// Décodeur de symboles arithmétique (§8.2). init_symbol / decode_symbol /
		// decode_symbol non adaptatif / read_bool équiprobable / renorm.
		// =====================================================================
		struct NkAv1SymbolDecoder {
				static constexpr int32 kEcProbShift = 6;
				static constexpr int32 kEcMinProb = 4;

				const uint8 *data = nullptr;
				usize size = 0;      // taille de la donnée de tile (en octets)
				usize bitPos = 0;    // position de lecture en BITS (pour f(n))
				uint32 symbolValue = 0;
				uint32 symbolRange = 0;
				int32 symbolMaxBits = 0;
				bool disableCdfUpdate = false;
				bool error = false;

				// f(n) : lit n bits MSB-first depuis la donnée de tile.
				uint32 ReadBits(int32 n) {
					uint32 v = 0;
					for (int32 i = 0; i < n; ++i) {
						const usize byteIdx = bitPos >> 3;
						int32 b = 0;
						if (byteIdx < size)
							b = (data[byteIdx] >> (7 - (bitPos & 7))) & 1;
						else
							error = true;
						++bitPos;
						v = (v << 1) | (uint32)b;
					}
					return v;
				}

				// init_symbol( sz ) — §8.2.2.
				void Init(const uint8 *d, usize sz, bool disableCdf) {
					data = d;
					size = sz;
					bitPos = 0;
					disableCdfUpdate = disableCdf;
					error = false;
					const int32 numBits = (int32)((sz * 8u < 15u) ? (sz * 8u) : 15u);
					const uint32 buf = ReadBits(numBits);
					const uint32 paddedBuf = buf << (15 - numBits);
					symbolValue = ((1u << 15) - 1) ^ paddedBuf;
					symbolRange = 1u << 15;
					symbolMaxBits = (int32)(8u * (uint32)sz) - 15;
				}

				// Renormalisation (§8.2.6).
				void Renorm() {
					const int32 bits = 15 - NkAv1FloorLog2(symbolRange);
					symbolRange = symbolRange << bits;
					int32 numBits = bits;
					const int32 maxb = symbolMaxBits > 0 ? symbolMaxBits : 0;
					if (numBits > maxb)
						numBits = maxb;
					const uint32 newData = ReadBits(numBits);
					const uint32 paddedData = newData << (bits - numBits);
					symbolValue = paddedData ^ (((symbolValue + 1) << bits) - 1);
					symbolMaxBits -= bits;
				}

				// decode_symbol( cdf, nsymbs ) — §8.2.4. cdf a nsymbs+1 entrées (dernière =
				// compteur d'adaptation). Renvoie le symbole [0, nsymbs-1] et adapte la CDF.
				int32 DecodeSymbol(uint16 *cdf, int32 nsymbs) {
					const int32 N = nsymbs;
					uint32 cur = symbolRange;
					uint32 prev;
					int32 symbol = -1;
					do {
						++symbol;
						prev = cur;
						const uint32 f = (1u << 15) - (uint32)cdf[symbol];
						cur = ((symbolRange >> 8) * (f >> kEcProbShift)) >> (7 - kEcProbShift);
						cur += (uint32)kEcMinProb * (uint32)(N - 1 - symbol);
					} while (symbolValue < cur);
					symbolRange = prev - cur;
					symbolValue = symbolValue - cur;
					Renorm();
					if (!disableCdfUpdate)
						UpdateCdf(cdf, symbol, nsymbs);
					return symbol;
				}

				// decode_symbol non adaptatif (CDF constante — §8.2.4 sans update).
				int32 DecodeSymbolNoAdapt(const uint16 *cdf, int32 nsymbs) {
					const int32 N = nsymbs;
					uint32 cur = symbolRange;
					uint32 prev;
					int32 symbol = -1;
					do {
						++symbol;
						prev = cur;
						const uint32 f = (1u << 15) - (uint32)cdf[symbol];
						cur = ((symbolRange >> 8) * (f >> kEcProbShift)) >> (7 - kEcProbShift);
						cur += (uint32)kEcMinProb * (uint32)(N - 1 - symbol);
					} while (symbolValue < cur);
					symbolRange = prev - cur;
					symbolValue = symbolValue - cur;
					Renorm();
					return symbol;
				}

				// read_bit équiprobable : lit 1 bit via le décodeur de symboles avec une CDF
				// uniforme NON adaptative {1<<14, 1<<15} (read_literal, §8.2 / §8.3). On passe
				// par EXACTEMENT le même chemin arithmétique que DecodeSymbol (mêmes formules,
				// EC_MIN_PROB inclus) pour garantir la cohérence interne — le bit renvoyé est
				// l'indice de symbole (0 = valeur haute, 1 = valeur basse), MSB d'abord.
				int32 ReadBit() {
					static const uint16 kUniformCdf[3] = {1u << 14, 1u << 15, 0};
					return DecodeSymbolNoAdapt(kUniformCdf, 2);
				}

				// L(n) : lit n bits équiprobables, MSB d'abord (read_literal, §8.3).
				uint32 ReadLiteral(int32 n) {
					uint32 x = 0;
					for (int32 i = 0; i < n; ++i)
						x = (x << 1) | (uint32)ReadBit();
					return x;
				}

				// NS(n) équiprobable dans le domaine symbole (decode_unsigned_subexp helpers
				// utilisent ReadLiteral — voir NkAv1Decoder.cpp).

				// Adaptation de CDF (§8.3.2 "Symbol decoding process" — update_cdf).
				// BUG CORRIGE (audit 2026-07-28) : la version précédente inversait le sens
				// de l'adaptation (if (i<symbol) incrémentait au lieu de décrémenter, et
				// vice versa). Transcrit ICI mot-à-mot depuis la spec :
				//   rate = 3 + (cdf[N]>15) + (cdf[N]>31) + Min(FloorLog2(N),2)
				//   tmp = 0
				//   for i in 0..N-2:
				//     tmp = (i==symbol) ? (1<<15) : tmp   // tmp bascule à 32768 dès i>=symbol
				//     if (tmp < cdf[i]) cdf[i] -= (cdf[i]-tmp) >> rate
				//     else               cdf[i] += (tmp-cdf[i]) >> rate
				//   cdf[N] += (cdf[N] < 32)
				// Net : cdf[i] DECROIT vers 0 pour i<symbol, CROIT vers 32768 pour i>=symbol.
				static void UpdateCdf(uint16 *cdf, int32 symbol, int32 nsymbs) {
					const int32 N = nsymbs;
					const uint32 count = cdf[N]; // compteur d'adaptation
					int32 rate = 3 + (count > 15 ? 1 : 0) + (count > 31 ? 1 : 0);
					const int32 fl = NkAv1FloorLog2((uint32)N);
					rate += (fl < 2 ? fl : 2);
					int32 tmp = 0;
					for (int32 i = 0; i < N - 1; ++i) {
						if (i == symbol)
							tmp = 1 << 15;
						const int32 ci = (int32)cdf[i];
						if (tmp < ci)
							cdf[i] = (uint16)(ci - ((ci - tmp) >> rate));
						else
							cdf[i] = (uint16)(ci + ((tmp - ci) >> rate));
					}
					if (count < 32)
						cdf[N] = (uint16)(count + 1);
				}

				// exit_symbol : rien à faire ici (pas de vérif de padding stricte).
				bool Overran() const { return error; }
		};

	} // namespace media
} // namespace nkentseu
