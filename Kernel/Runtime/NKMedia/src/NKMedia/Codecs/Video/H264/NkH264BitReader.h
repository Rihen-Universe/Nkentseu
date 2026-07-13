// =============================================================================
// NKMedia/Codecs/Video/H264/NkH264BitReader.h
// -----------------------------------------------------------------------------
// Lecteur de bits MSB-first pour le DÉCODEUR H.264 (le pendant de NkH264BitWriter).
// Opère sur un RBSP déjà dé-émulé (00 00 03 retirés). Fournit : lecture de n bits,
// Exp-Golomb ue(v)/se(v), et PEEK (lecture sans consommer) indispensable au décodage
// des VLC CAVLC (on lit les bits suivants puis on cherche l'entrée de table qui matche).
// Header-only, zero-STL, namespace nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace media {

		struct NkH264BitReader {
				const uint8 *data = nullptr;
				usize sizeBits = 0;
				usize pos = 0; // position courante en bits

				NkH264BitReader() = default;
				NkH264BitReader(const uint8 *d, usize nbytes) : data(d), sizeBits(nbytes * 8), pos(0) {
				}

				bool Eof() const {
					return pos >= sizeBits;
				}
				usize BitsLeft() const {
					return (pos < sizeBits) ? (sizeBits - pos) : 0;
				}

				uint32 U1() {
					if (pos >= sizeBits)
						return 0;
					const uint32 b = (data[pos >> 3] >> (7 - (pos & 7))) & 1u;
					++pos;
					return b;
				}
				uint32 U(int32 n) {
					uint32 v = 0;
					for (int32 i = 0; i < n; ++i)
						v = (v << 1) | U1();
					return v;
				}
				// Lit sans consommer (jusqu'à 24 bits) — pour matcher les VLC.
				uint32 Peek(int32 n) const {
					uint32 v = 0;
					usize p = pos;
					for (int32 i = 0; i < n; ++i) {
						const uint32 b = (p < sizeBits) ? ((data[p >> 3] >> (7 - (p & 7))) & 1u) : 0u;
						v = (v << 1) | b;
						++p;
					}
					return v;
				}
				void Skip(int32 n) {
					pos += (usize)n;
				}

				// Exp-Golomb non signé ue(v).
				uint32 UE() {
					int32 z = 0;
					while (pos < sizeBits && U1() == 0)
						++z;
					uint32 v = 0;
					for (int32 i = 0; i < z; ++i)
						v = (v << 1) | U1();
					return ((1u << z) - 1u) + v;
				}
				// Exp-Golomb signé se(v).
				int32 SE() {
					const uint32 k = UE();
					return (k & 1) ? (int32)((k + 1) / 2) : -(int32)(k / 2);
				}
		};

	} // namespace media
} // namespace nkentseu
