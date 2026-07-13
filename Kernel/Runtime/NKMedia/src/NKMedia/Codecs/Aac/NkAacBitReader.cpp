// =============================================================================
// NKMedia/Codecs/Aac/NkAacBitReader.cpp — self-test du lecteur de bits MSB-first.
// =============================================================================
#include "NKMedia/Codecs/Aac/NkAacBitReader.h"
#include "NKMedia/Video/NkBitWriter.h"

namespace nkentseu {
	namespace media {

		bool NkAacBitReader::SelfTest() {
			// 1) Round-trip : écrit une suite de champs de largeurs variées, relit à l'identique.
			NkBitWriter w;
			const struct {
					uint32 v;
					int32 n;
			} seq[] = {{1, 1}, {0, 1}, {5, 3}, {0xA, 4}, {0x2AB, 12}, {0xFFFF, 16},
					   {0x1234567u, 25}, {0x7Fu, 7}, {0u, 5}, {0xDEADBEEFu, 32}};
			const int32 count = (int32)(sizeof(seq) / sizeof(seq[0]));
			for (int32 i = 0; i < count; ++i)
				w.PutBits(seq[i].v, seq[i].n);
			w.AlignByteZero();

			NkAacBitReader r(w.Data(), w.Size());
			for (int32 i = 0; i < count; ++i) {
				const uint32 mask = (seq[i].n >= 32) ? 0xFFFFFFFFu : ((1u << seq[i].n) - 1u);
				if (r.ReadBits(seq[i].n) != (seq[i].v & mask))
					return false;
			}

			// 2) PeekBits ne doit pas avancer.
			{
				const uint8 bytes[2] = {0xB5, 0x3C}; // 1011'0101 0011'1100
				NkAacBitReader p(bytes, 2);
				if (p.PeekBits(4) != 0xB)
					return false;
				if (p.Tell() != 0)
					return false;
				if (p.ReadBits(4) != 0xB)
					return false;
				if (p.ReadBits(4) != 0x5)
					return false;
				if (p.PeekBits(8) != 0x3C)
					return false;
				if (p.ReadBits(8) != 0x3C)
					return false;
			}

			// 3) ByteAlign + SkipBits + BitsLeft.
			{
				const uint8 bytes[3] = {0xFF, 0x00, 0xAA};
				NkAacBitReader a(bytes, 3);
				a.ReadBits(3);		 // pos=3
				a.ByteAlign();		 // pos=8
				if (a.Tell() != 8)
					return false;
				if (a.ReadBits(8) != 0x00)
					return false; // 2e octet
				if (a.BitsLeft() != 8)
					return false;
				a.SkipBits(4);
				if (a.ReadBits(4) != 0xA)
					return false; // bas de 0xAA
			}

			// 4) Overrun : lecture au-delà de la fin renvoie 0 et marque Overrun.
			{
				const uint8 b = 0x80;
				NkAacBitReader o(&b, 1);
				if (o.ReadBits(1) != 1)
					return false;
				o.SkipBits(20); // dépasse
				if (o.ReadBits(4) != 0)
					return false;
				if (!o.Overrun())
					return false;
			}
			return true;
		}

	} // namespace media
} // namespace nkentseu
