// =============================================================================
// NKMedia/Codecs/Opus/NkOpusRange.cpp — range coder Opus (RFC 6716 §4.1).
// Port fidèle de libopus (entdec.c / entenc.c). Aller-retour prouvé en self-test.
// =============================================================================
#include "NKMedia/Codecs/Opus/NkOpusRange.h"
#include "NKMemory/NKMemory.h"

namespace nkentseu {
	namespace media {

		namespace {
			constexpr int32 EC_SYM_BITS = 8;
			constexpr int32 EC_CODE_BITS = 32;
			constexpr uint32 EC_SYM_MAX = 0xFFu;
			constexpr uint32 EC_CODE_TOP = 0x80000000u;			   // 1<<31
			constexpr uint32 EC_CODE_BOT = EC_CODE_TOP >> EC_SYM_BITS; // 1<<23
			constexpr int32 EC_CODE_SHIFT = EC_CODE_BITS - EC_SYM_BITS - 1; // 23
			constexpr int32 EC_CODE_EXTRA = (EC_CODE_BITS - 2) % EC_SYM_BITS + 1; // 7
			constexpr int32 EC_WINDOW_SIZE = 32;
			constexpr int32 EC_UINT_BITS = 8;

			uint32 Mini(uint32 a, uint32 b) {
				return a < b ? a : b;
			}
			// EC_ILOG : nombre de bits significatifs (position du MSB + 1). ILOG(0)=0.
			int32 Ilog(uint32 v) {
				int32 r = 0;
				while (v) {
					++r;
					v >>= 1;
				}
				return r;
			}
		} // namespace

		// ======================================================================
		//  DÉCODEUR
		// ======================================================================
		static uint8 ReadByte(NkOpusRangeDecoder *d) {
			return d->offs < d->storage ? d->buf[d->offs++] : 0;
		}
		static uint8 ReadByteFromEnd(NkOpusRangeDecoder *d) {
			return d->end_offs < d->storage ? d->buf[d->storage - ++d->end_offs] : 0;
		}
		static void DecNormalize(NkOpusRangeDecoder *d) {
			while (d->rng <= EC_CODE_BOT) {
				int32 sym;
				d->nbits_total += EC_SYM_BITS;
				d->rng <<= EC_SYM_BITS;
				sym = d->rem;
				d->rem = ReadByte(d);
				sym = (sym << EC_SYM_BITS | d->rem) >> (EC_SYM_BITS - EC_CODE_EXTRA);
				d->val = ((d->val << EC_SYM_BITS) + (EC_SYM_MAX & ~(uint32)sym)) & (EC_CODE_TOP - 1);
			}
		}

		void NkOpusRangeDecoder::Init(const uint8 *buffer, uint32 storageBytes) {
			buf = buffer;
			storage = storageBytes;
			end_offs = 0;
			end_window = 0;
			nend_bits = 0;
			nbits_total = EC_CODE_BITS + 1 - (EC_CODE_BITS - EC_CODE_EXTRA) / EC_SYM_BITS * EC_SYM_BITS;
			offs = 0;
			rng = 1u << EC_CODE_EXTRA;
			rem = ReadByte(this);
			val = rng - 1 - ((uint32)rem >> (EC_SYM_BITS - EC_CODE_EXTRA));
			error = 0;
			DecNormalize(this);
		}

		uint32 NkOpusRangeDecoder::Decode(uint32 ft) {
			ext = rng / ft;
			const uint32 s = val / ext;
			return ft - Mini(s + 1, ft);
		}
		uint32 NkOpusRangeDecoder::DecodeBin(uint32 bits) {
			ext = rng >> bits;
			const uint32 s = val / ext;
			return (1u << bits) - Mini(s + 1u, 1u << bits);
		}
		void NkOpusRangeDecoder::Update(uint32 fl, uint32 fh, uint32 ft) {
			const uint32 s = ext * (ft - fh);
			val -= s;
			rng = fl > 0 ? ext * (fh - fl) : rng - s;
			DecNormalize(this);
		}
		int32 NkOpusRangeDecoder::DecodeBitLogp(uint32 logp) {
			const uint32 r = rng;
			const uint32 d = val;
			const uint32 s = r >> logp;
			const int32 ret = d < s ? 1 : 0;
			if (!ret)
				val = d - s;
			rng = ret ? s : r - s;
			DecNormalize(this);
			return ret;
		}
		int32 NkOpusRangeDecoder::DecodeIcdf(const uint8 *icdf, uint32 ftb) {
			uint32 s = rng;
			const uint32 d = val;
			const uint32 r = s >> ftb;
			int32 ret = -1;
			uint32 t;
			do {
				t = s;
				s = r * icdf[++ret];
			} while (d < s);
			val = d - s;
			rng = t - s;
			DecNormalize(this);
			return ret;
		}
		uint32 NkOpusRangeDecoder::DecodeBits(uint32 bits) {
			uint32 window = end_window;
			int32 available = nend_bits;
			if ((uint32)available < bits) {
				do {
					window |= (uint32)ReadByteFromEnd(this) << available;
					available += EC_SYM_BITS;
				} while (available <= EC_WINDOW_SIZE - EC_SYM_BITS);
			}
			const uint32 ret = window & ((1u << bits) - 1u);
			window >>= bits;
			available -= (int32)bits;
			end_window = window;
			nend_bits = available;
			nbits_total += (int32)bits;
			return ret;
		}
		uint32 NkOpusRangeDecoder::DecodeUint(uint32 ft) {
			ft--;
			int32 ftb = Ilog(ft);
			if (ftb > EC_UINT_BITS) {
				ftb -= EC_UINT_BITS;
				const uint32 f = (ft >> ftb) + 1;
				const uint32 s = Decode(f);
				Update(s, s + 1, f);
				const uint32 t = (s << ftb) | DecodeBits((uint32)ftb);
				if (t <= ft)
					return t;
				error = 1;
				return ft;
			}
			const uint32 f2 = ft + 1;
			const uint32 s = Decode(f2);
			Update(s, s + 1, f2);
			return s;
		}

		// ======================================================================
		//  ENCODEUR
		// ======================================================================
		static void WriteByte(NkOpusRangeEncoder *e, uint32 value) {
			if (e->offs + e->end_offs >= e->storage) {
				e->error = -1;
				return;
			}
			e->buf[e->offs++] = (uint8)value;
		}
		static void WriteByteAtEnd(NkOpusRangeEncoder *e, uint32 value) {
			if (e->offs + e->end_offs >= e->storage) {
				e->error = -1;
				return;
			}
			e->buf[e->storage - ++e->end_offs] = (uint8)value;
		}
		static void EncCarryOut(NkOpusRangeEncoder *e, int32 c) {
			if ((uint32)c != EC_SYM_MAX) {
				const int32 carry = c >> EC_SYM_BITS;
				if (e->rem >= 0)
					WriteByte(e, (uint32)(e->rem + carry));
				if (e->ext > 0) {
					const uint32 sym = (EC_SYM_MAX + (uint32)carry) & EC_SYM_MAX;
					do {
						WriteByte(e, sym);
					} while (--e->ext > 0);
				}
				e->rem = c & (int32)EC_SYM_MAX;
			} else {
				e->ext++;
			}
		}
		static void EncNormalize(NkOpusRangeEncoder *e) {
			while (e->rng <= EC_CODE_BOT) {
				EncCarryOut(e, (int32)(e->val >> EC_CODE_SHIFT));
				e->val = (e->val << EC_SYM_BITS) & (EC_CODE_TOP - 1);
				e->rng <<= EC_SYM_BITS;
				e->nbits_total += EC_SYM_BITS;
			}
		}

		void NkOpusRangeEncoder::Init(uint8 *buffer, uint32 sizeBytes) {
			buf = buffer;
			end_offs = 0;
			end_window = 0;
			nend_bits = 0;
			nbits_total = EC_CODE_BITS + 1;
			offs = 0;
			rng = EC_CODE_TOP;
			rem = -1;
			val = 0;
			ext = 0;
			storage = sizeBytes;
			error = 0;
		}
		void NkOpusRangeEncoder::Encode(uint32 fl, uint32 fh, uint32 ft) {
			const uint32 r = rng / ft;
			if (fl > 0) {
				val += rng - r * (ft - fl);
				rng = r * (fh - fl);
			} else {
				rng -= r * (ft - fh);
			}
			EncNormalize(this);
		}
		void NkOpusRangeEncoder::EncodeBin(uint32 fl, uint32 fh, uint32 bits) {
			const uint32 r = rng >> bits;
			if (fl > 0) {
				val += rng - r * ((1u << bits) - fl);
				rng = r * (fh - fl);
			} else {
				rng -= r * ((1u << bits) - fh);
			}
			EncNormalize(this);
		}
		void NkOpusRangeEncoder::EncodeBitLogp(int32 v, uint32 logp) {
			uint32 r = rng;
			const uint32 l = val;
			const uint32 s = r >> logp;
			r -= s;
			if (v)
				val = l + r;
			rng = v ? s : r;
			EncNormalize(this);
		}
		void NkOpusRangeEncoder::EncodeIcdf(int32 s, const uint8 *icdf, uint32 ftb) {
			const uint32 r = rng >> ftb;
			if (s > 0) {
				val += rng - r * icdf[s - 1];
				rng = r * (uint32)(icdf[s - 1] - icdf[s]);
			} else {
				rng -= r * icdf[s];
			}
			EncNormalize(this);
		}
		void NkOpusRangeEncoder::EncodeBits(uint32 fl, uint32 bits) {
			uint32 window = end_window;
			int32 used = nend_bits;
			if ((uint32)(used + (int32)bits) > (uint32)EC_WINDOW_SIZE) {
				do {
					WriteByteAtEnd(this, window & EC_SYM_MAX);
					window >>= EC_SYM_BITS;
					used -= EC_SYM_BITS;
				} while (used >= EC_SYM_BITS);
			}
			window |= fl << used;
			used += (int32)bits;
			end_window = window;
			nend_bits = used;
			nbits_total += (int32)bits;
		}
		void NkOpusRangeEncoder::EncodeUint(uint32 fl, uint32 ft) {
			ft--;
			int32 ftb = Ilog(ft);
			if (ftb > EC_UINT_BITS) {
				ftb -= EC_UINT_BITS;
				const uint32 f = (ft >> ftb) + 1;
				Encode(fl >> ftb, (fl >> ftb) + 1, f);
				EncodeBits(fl & (((uint32)1 << ftb) - 1u), (uint32)ftb);
			} else {
				Encode(fl, fl + 1, ft + 1);
			}
		}
		void NkOpusRangeEncoder::Done() {
			int32 l = EC_CODE_BITS - Ilog(rng);
			uint32 msk = (EC_CODE_TOP - 1) >> l;
			uint32 end = (val + msk) & ~msk;
			if ((end | msk) >= val + rng) {
				l++;
				msk >>= 1;
				end = (val + msk) & ~msk;
			}
			while (l > 0) {
				EncCarryOut(this, (int32)(end >> EC_CODE_SHIFT));
				end = (end << EC_SYM_BITS) & (EC_CODE_TOP - 1);
				l -= EC_SYM_BITS;
			}
			if (rem >= 0 || ext > 0)
				EncCarryOut(this, 0);
			// Vide les bits bruts bufferisés vers la fin du buffer.
			uint32 window = end_window;
			int32 used = nend_bits;
			while (used >= EC_SYM_BITS) {
				WriteByteAtEnd(this, window & EC_SYM_MAX);
				window >>= EC_SYM_BITS;
				used -= EC_SYM_BITS;
			}
			if (!error) {
				// Zéro dans l'espace libre entre les deux flux.
				for (uint32 i = offs; i < storage - end_offs; ++i)
					buf[i] = 0;
				if (used > 0) {
					if (end_offs >= storage) {
						error = -1;
					} else {
						l = -l;
						if (offs + end_offs >= storage && l < used) {
							window &= ((uint32)1 << l) - 1u;
							error = -1;
						}
						buf[storage - end_offs - 1] |= (uint8)window;
					}
				}
			}
		}

		// ======================================================================
		//  SELF-TEST : aller-retour encode -> decode.
		// ======================================================================
		bool NkOpusRange::SelfTest() {
			bool ok = true;
			const uint32 CAP = 4096;
			uint8 *buffer = (uint8 *)memory::NkAlloc(CAP);
			if (!buffer)
				return false;

			// icdf de test (3 symboles), ftb=4 → total 16. icdf décroissant, dernier = 0.
			// P(0)=8/16, P(1)=5/16, P(2)=3/16. icdf[k] = 16 - somme cumulée.
			const uint8 icdf[3] = {8, 3, 0};

			// Séquence à coder : symboles icdf + bits bruts + uint.
			const int32 syms[10] = {0, 2, 1, 1, 0, 2, 0, 1, 2, 0};
			const uint32 rawbits[5] = {5, 0, 15, 8, 3};	 // 4 bits chacun
			const uint32 uints[4] = {0, 100, 999, 42};	 // uint sur ft=1000

			NkOpusRangeEncoder enc;
			enc.Init(buffer, CAP);
			for (int32 i = 0; i < 10; ++i)
				enc.EncodeIcdf(syms[i], icdf, 4);
			for (int32 i = 0; i < 5; ++i)
				enc.EncodeBits(rawbits[i], 4);
			for (int32 i = 0; i < 4; ++i)
				enc.EncodeUint(uints[i], 1000);
			enc.Done();
			if (enc.error != 0)
				ok = false;

			NkOpusRangeDecoder dec;
			dec.Init(buffer, CAP);
			for (int32 i = 0; i < 10; ++i) {
				const int32 s = dec.DecodeIcdf(icdf, 4);
				if (s != syms[i])
					ok = false;
			}
			for (int32 i = 0; i < 5; ++i) {
				const uint32 b = dec.DecodeBits(4);
				if (b != rawbits[i])
					ok = false;
			}
			for (int32 i = 0; i < 4; ++i) {
				const uint32 u = dec.DecodeUint(1000);
				if (u != uints[i])
					ok = false;
			}

			// 2e test : cdf via Encode/Decode/Update (proba uniforme ft=64).
			{
				NkOpusRangeEncoder e2;
				e2.Init(buffer, CAP);
				const uint32 vals[6] = {0, 63, 31, 10, 50, 7};
				for (int32 i = 0; i < 6; ++i)
					e2.Encode(vals[i], vals[i] + 1, 64);
				e2.Done();
				NkOpusRangeDecoder d2;
				d2.Init(buffer, CAP);
				for (int32 i = 0; i < 6; ++i) {
					const uint32 fs = d2.Decode(64);
					d2.Update(fs, fs + 1, 64);
					if (fs != vals[i])
						ok = false;
				}
			}

			memory::NkFree(buffer);
			return ok;
		}

	} // namespace media
} // namespace nkentseu
