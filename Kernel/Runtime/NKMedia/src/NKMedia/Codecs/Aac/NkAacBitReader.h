// =============================================================================
// NKMedia/Codecs/Aac/NkAacBitReader.h
// -----------------------------------------------------------------------------
// Lecteur de bits MSB-first (gros-boutiste au bit près) — fondation du décodeur
// AAC-LC (raw_data_block : flux de VLC/Huffman + champs de longueur fixe). Pointe
// dans un tampon octet en lecture seule. Renvoie 0 en fin de flux (pas de crash).
// Miroir en lecture de NkBitWriter. Zero-STL, nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace media {

		struct NkAacBitReader {
			public:
				NkAacBitReader() = default;
				NkAacBitReader(const uint8 *data, usize sizeBytes) {
					Init(data, sizeBytes);
				}

				void Init(const uint8 *data, usize sizeBytes) {
					mData = data;
					mSizeBits = (uint64)sizeBytes * 8u;
					mPos = 0;
				}

				// Lit `n` bits (0..32), MSB en premier. Au-delà de la fin → complète avec des 0.
				uint32 ReadBits(int32 n) {
					uint32 v = 0;
					for (int32 i = 0; i < n; ++i)
						v = (v << 1) | ReadBit();
					return v;
				}

				// Lit 1 bit. 0 si hors flux.
				uint32 ReadBit() {
					if (mPos >= mSizeBits) {
						++mPos; // continue à avancer pour que Tell() reflète la surconsommation
						return 0;
					}
					const uint64 byteIdx = mPos >> 3;
					const int32 bitIdx = 7 - (int32)(mPos & 7u); // MSB-first
					++mPos;
					return (uint32)((mData[byteIdx] >> bitIdx) & 1u);
				}

				// Consulte `n` bits sans avancer.
				uint32 PeekBits(int32 n) {
					const uint64 saved = mPos;
					const uint32 v = ReadBits(n);
					mPos = saved;
					return v;
				}

				// Avance de `n` bits (peut dépasser la fin).
				void SkipBits(int32 n) {
					mPos += (uint64)n;
				}

				// Aligne sur le prochain octet (jette les bits de bourrage).
				void ByteAlign() {
					if (mPos & 7u)
						mPos = (mPos + 7u) & ~(uint64)7u;
				}

				uint64 Tell() const {
					return mPos;
				}
				uint64 BitsLeft() const {
					return mPos < mSizeBits ? (mSizeBits - mPos) : 0;
				}
				bool Overrun() const {
					return mPos > mSizeBits;
				}

				static bool SelfTest();

			private:
				const uint8 *mData = nullptr;
				uint64 mSizeBits = 0;
				uint64 mPos = 0;
		};

	} // namespace media
} // namespace nkentseu
