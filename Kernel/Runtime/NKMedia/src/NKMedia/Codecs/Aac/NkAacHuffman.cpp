// =============================================================================
// NKMedia/Codecs/Aac/NkAacHuffman.cpp — décodage spectral + scalefactor (ISO 14496-3).
// =============================================================================
#include "NKMedia/Codecs/Aac/NkAacHuffman.h"
#include "NKMedia/Video/NkBitWriter.h"

namespace nkentseu {
	namespace media {

		namespace {
			// Codebooks NON signés (les valeurs décodées sont des magnitudes → bit de signe
			// après le codeword pour chaque valeur non nulle). Table 4.6.2 du standard.
			bool IsUnsignedCb(int32 cb) {
				return cb == 3 || cb == 4 || cb == 7 || cb == 8 || cb == 9 || cb == 10 || cb == 11;
			}

			// Recherche du codeword : lit bit à bit MSB-first jusqu'à correspondance (len, cw).
			// Les codes sont préfixe-libres → la 1re correspondance est LA solution.
			const NkAacHcbEntry *FindCode(const NkAacHcbBook &bk, NkAacBitReader &br) {
				uint32 code = 0;
				for (int32 len = 1; len <= 20; ++len) {
					code = (code << 1) | br.ReadBit();
					for (int32 i = 0; i < bk.count; ++i)
						if ((int32)bk.entries[i].len == len && (uint32)bk.entries[i].cw == code)
							return &bk.entries[i];
				}
				return nullptr;
			}

			// Échappement du codebook 11 : si la magnitude vaut 16, lit N bits '1' (jusqu'au
			// premier '0'), puis N+4 bits → valeur = 2^(N+4) + mot. Signe préservé.
			int32 ApplyEscape(NkAacBitReader &br, int32 x) {
				if (x != 16 && x != -16)
					return x;
				const bool neg = (x < 0);
				int32 i = 4;
				for (; i < 16; ++i)
					if (br.ReadBit() == 0)
						break;
				if (i >= 16)
					return x; // séquence invalide → garde la valeur
				const int32 off = (int32)br.ReadBits(i);
				const int32 j = off | (1 << i);
				return neg ? -j : j;
			}
		} // namespace

		void NkAacHuffman::DecodeSpectral(NkAacBitReader &br, int32 cb, int32 *out) {
			if (cb < 1 || cb > 11) {
				out[0] = out[1] = out[2] = out[3] = 0;
				return;
			}
			const NkAacHcbBook &bk = kAacHcbBooks[cb];
			const NkAacHcbEntry *e = FindCode(bk, br);
			const int32 dim = bk.dim;
			for (int32 i = 0; i < dim; ++i)
				out[i] = e ? (int32)e->v[i] : 0;

			if (IsUnsignedCb(cb))
				for (int32 i = 0; i < dim; ++i)
					if (out[i] != 0 && br.ReadBit())
						out[i] = -out[i];

			if (cb == 11)
				for (int32 i = 0; i < dim; ++i)
					out[i] = ApplyEscape(br, out[i]);
		}

		int32 NkAacHuffman::DecodeScaleFactor(NkAacBitReader &br) {
			const NkAacHcbEntry *e = FindCode(kAacHcbBooks[0], br);
			return e ? (int32)e->v[0] : 60;
		}

		bool NkAacHuffman::SelfTest() {
			// Round-trip sur TOUTES les entrées de chaque codebook : écrit le codeword (+ bits
			// de signe / échappement selon le cas), décode, vérifie l'égalité des valeurs.
			for (int32 cb = 1; cb <= 11; ++cb) {
				const NkAacHcbBook &bk = kAacHcbBooks[cb];
				const bool uns = IsUnsignedCb(cb);
				for (int32 i = 0; i < bk.count; ++i) {
					const NkAacHcbEntry &e = bk.entries[i];
					// Ordre du flux (comme le décodeur) : codeword, PUIS tous les bits de signe,
					// PUIS tous les échappements. Signe positif (0) écrit → magnitudes conservées.
					int32 expect[4] = {0, 0, 0, 0};
					NkBitWriter w;
					w.PutBits(e.cw, e.len);
					if (uns)
						for (int32 d = 0; d < bk.dim; ++d)
							if ((int32)e.v[d] != 0)
								w.PutBits(0, 1);
					for (int32 d = 0; d < bk.dim; ++d) {
						int32 mag = (int32)e.v[d];
						if (cb == 11 && mag == 16) {
							// échappement : N=0 (un seul '0'), puis 4 bits = 0100 → 16 + 4 = 20.
							w.PutBits(0, 1);
							w.PutBits(4, 4);
							mag = 20;
						}
						expect[d] = mag;
					}
					w.AlignByteZero();

					NkAacBitReader br(w.Data(), w.Size());
					int32 out[4] = {0, 0, 0, 0};
					NkAacHuffman::DecodeSpectral(br, cb, out);
					for (int32 d = 0; d < bk.dim; ++d)
						if (out[d] != expect[d])
							return false;
				}
			}

			// Scalefactor : round-trip de toutes les entrées.
			{
				const NkAacHcbBook &bk = kAacHcbBooks[0];
				for (int32 i = 0; i < bk.count; ++i) {
					const NkAacHcbEntry &e = bk.entries[i];
					NkBitWriter w;
					w.PutBits(e.cw, e.len);
					w.AlignByteZero();
					NkAacBitReader br(w.Data(), w.Size());
					if (NkAacHuffman::DecodeScaleFactor(br) != (int32)e.v[0])
						return false;
				}
			}
			return true;
		}

	} // namespace media
} // namespace nkentseu
