// =============================================================================
// NKMedia/Video/NkBitWriter.h
// -----------------------------------------------------------------------------
// Écrivain de bits MSB-first (gros-boutiste au bit près) — requis par les codecs
// vidéo type MPEG (flux de VLC/Huffman). Accumule dans un tampon octet croissant.
// Zero-STL, nkentseu::media.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace media {

		struct NkBitWriter {
			public:
				// Écrit les `n` bits de poids faible de `value` (0..32), MSB en premier.
				void PutBits(uint32 value, int32 n) {
					for (int32 i = n - 1; i >= 0; --i) {
						mAcc = (uint8)((mAcc << 1) | ((value >> i) & 1u));
						if (++mNbits == 8) {
							mBytes.PushBack(mAcc);
							mAcc = 0;
							mNbits = 0;
						}
					}
				}

				// Aligne sur l'octet en insérant des `0` (utilisé avant les start codes).
				void AlignByteZero() {
					while (mNbits != 0)
						PutBits(0, 1);
				}

				// Écrit un start code 32 bits (00 00 01 xx) — DOIT être aligné octet.
				void PutStartCode(uint8 code) {
					AlignByteZero();
					mBytes.PushBack(0x00);
					mBytes.PushBack(0x00);
					mBytes.PushBack(0x01);
					mBytes.PushBack(code);
				}

				const uint8 *Data() const {
					return mBytes.Data();
				}
				usize Size() const {
					return (usize)mBytes.Size();
				}
				int32 BitPos() const {
					return mNbits;
				}
				void Clear() {
					mBytes.Clear();
					mAcc = 0;
					mNbits = 0;
				}

			private:
				NkVector<uint8> mBytes;
				uint8 mAcc = 0;
				int32 mNbits = 0;
		};

	} // namespace media
} // namespace nkentseu
