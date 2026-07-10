// =============================================================================
// NKMedia/Codecs/Video/H264/NkH264BitWriter.h
// -----------------------------------------------------------------------------
// Écrivain de bitstream H.264 (ISO/IEC 14496-10) — MSB-first + codes Exp-Golomb
// (ue/se, §9.1) + émission de NAL units en Annex-B (start code 00 00 00 01) avec
// prévention d'émulation (insertion de 0x03 après 00 00 0x). Base de l'encodeur
// H.264 from-scratch (sans ffmpeg). Zero-STL, nkentseu::media.
//
// Algorithmes réécrits à la sauce Nkentseu (réf lue : minih264 lieff, spec H.264).
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace media {

		// Écrit les bits d'un RBSP (Raw Byte Sequence Payload), puis encapsule en NAL.
		struct NkH264BitWriter {
			public:
				void Clear() {
					mRbsp.Clear();
					mAcc = 0;
					mNbits = 0;
				}

				// n bits (0..32) de `value`, MSB en premier.
				void PutBits(uint32 value, int32 n) {
					for (int32 i = n - 1; i >= 0; --i) {
						mAcc = (uint8)((mAcc << 1) | ((value >> i) & 1u));
						if (++mNbits == 8) {
							mRbsp.PushBack(mAcc);
							mAcc = 0;
							mNbits = 0;
						}
					}
				}

				void PutFlag(int32 b) {
					PutBits(b ? 1u : 0u, 1);
				}

				// ue(v) — Exp-Golomb non signé (§9.1) : size = ilog2(val+1)+1 ; émet 2*size-1 bits de val+1.
				void Ue(uint32 val) {
					uint32 v = val + 1;
					int32 size = 0;
					uint32 t = v;
					while (t) {
						++size;
						t >>= 1;
					}
					PutBits(v, 2 * size - 1);
				}

				// se(v) — Exp-Golomb signé : 0→0, 1→1, -1→2, 2→3, -2→4…
				void Se(int32 val) {
					uint32 u = (val <= 0) ? (uint32)(-2 * val) : (uint32)(2 * val - 1);
					Ue(u);
				}

				// rbsp_trailing_bits : bit '1' puis remplissage '0' jusqu'à l'octet.
				void TrailingBits() {
					PutBits(1, 1);
					while (mNbits != 0)
						PutBits(0, 1);
				}

				// Termine le RBSP (aligne) et l'ENCAPSULE en NAL Annex-B dans `out` :
				// start code 00 00 00 01 + (nal_ref_idc<<5 | nal_unit_type) + RBSP avec anti-émulation.
				void EmitNal(NkVector<uint8> &out, int32 nalRefIdc, int32 nalUnitType) {
					// Aligne (le RBSP doit déjà finir sur TrailingBits pour les slices/SPS/PPS).
					if (mNbits != 0) {
						mRbsp.PushBack((uint8)(mAcc << (8 - mNbits)));
						mAcc = 0;
						mNbits = 0;
					}
					out.PushBack(0x00);
					out.PushBack(0x00);
					out.PushBack(0x00);
					out.PushBack(0x01);
					out.PushBack((uint8)(((nalRefIdc & 3) << 5) | (nalUnitType & 0x1F)));
					// Anti-émulation : insérer 0x03 après deux 0x00 consécutifs suivis de <=0x03.
					int32 zeros = 0;
					for (uint64 i = 0; i < mRbsp.Size(); ++i) {
						const uint8 b = mRbsp[i];
						if (zeros >= 2 && b <= 0x03) {
							out.PushBack(0x03);
							zeros = 0;
						}
						out.PushBack(b);
						zeros = (b == 0x00) ? (zeros + 1) : 0;
					}
					Clear();
				}

			private:
				NkVector<uint8> mRbsp;
				uint8 mAcc = 0;
				int32 mNbits = 0;
		};

	} // namespace media
} // namespace nkentseu
