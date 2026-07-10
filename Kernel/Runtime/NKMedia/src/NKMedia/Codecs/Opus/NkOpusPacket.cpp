// =============================================================================
// NKMedia/Codecs/Opus/NkOpusPacket.cpp — parsing de paquet Opus (RFC 6716 §3).
// =============================================================================
#include "NKMedia/Codecs/Opus/NkOpusPacket.h"

namespace nkentseu {
	namespace media {

		namespace {

			// Table de configuration (RFC 6716 §3.1) : config 0..31 → mode/bande/durée.
			void ConfigToParams(int32 c, NkOpusMode &mode, NkOpusBandwidth &bw, float32 &ms) {
				if (c < 12) { // SILK-only : NB/MB/WB × {10,20,40,60} ms
					mode = NkOpusMode::NK_SILK_ONLY;
					bw = (c < 4) ? NkOpusBandwidth::NK_NB : (c < 8) ? NkOpusBandwidth::NK_MB : NkOpusBandwidth::NK_WB;
					const float32 t[4] = {10.f, 20.f, 40.f, 60.f};
					ms = t[c & 3];
				} else if (c < 16) { // Hybrid : SWB/FB × {10,20} ms
					mode = NkOpusMode::NK_HYBRID;
					bw = (c < 14) ? NkOpusBandwidth::NK_SWB : NkOpusBandwidth::NK_FB;
					ms = (c & 1) ? 20.f : 10.f;
				} else { // CELT-only : NB/WB/SWB/FB × {2.5,5,10,20} ms
					mode = NkOpusMode::NK_CELT_ONLY;
					const NkOpusBandwidth b[4] = {NkOpusBandwidth::NK_NB, NkOpusBandwidth::NK_WB,
												  NkOpusBandwidth::NK_SWB, NkOpusBandwidth::NK_FB};
					bw = b[(c - 16) >> 2];
					const float32 t[4] = {2.5f, 5.f, 10.f, 20.f};
					ms = t[c & 3];
				}
			}

			// Longueur de trame encodée (RFC 6716 §3.2.1) : 1 ou 2 octets. Renvoie #octets consommés (0=err).
			int32 ReadFrameLength(const uint8 *p, usize avail, int32 &length) {
				if (avail < 1)
					return 0;
				const uint8 b0 = p[0];
				if (b0 < 252) {
					length = (int32)b0;
					return 1;
				}
				if (avail < 2)
					return 0;
				length = (int32)b0 + (int32)p[1] * 4; // b0∈[252,255], max 1275
				return 2;
			}

		} // namespace

		bool NkOpusPacket::Parse(const uint8 *data, usize len, NkOpusPacketInfo &out) {
			if (data == nullptr || len < 1)
				return false;
			const uint8 toc = data[0];
			out.config = toc >> 3;
			out.stereo = ((toc >> 2) & 1) != 0;
			const int32 code = toc & 3;
			ConfigToParams(out.config, out.mode, out.bandwidth, out.frameSizeMs);
			out.frameCount = 0;

			usize pos = 1; // après le TOC

			if (code == 0) {
				// 1 trame = tout le reste.
				out.frameCount = 1;
				out.frames[0].offset = pos;
				out.frames[0].size = len - pos;
				return true;
			}
			if (code == 1) {
				// 2 trames CBR, tailles égales.
				const usize rem = len - pos;
				if ((rem & 1) != 0)
					return false; // doit être pair
				const usize half = rem / 2;
				out.frameCount = 2;
				out.frames[0].offset = pos;
				out.frames[0].size = half;
				out.frames[1].offset = pos + half;
				out.frames[1].size = half;
				return true;
			}
			if (code == 2) {
				// 2 trames VBR : longueur de la 1re, la 2e = reste.
				int32 n1 = 0;
				const int32 used = ReadFrameLength(data + pos, len - pos, n1);
				if (used == 0)
					return false;
				pos += (usize)used;
				if (pos + (usize)n1 > len)
					return false;
				out.frameCount = 2;
				out.frames[0].offset = pos;
				out.frames[0].size = (usize)n1;
				out.frames[1].offset = pos + (usize)n1;
				out.frames[1].size = len - (pos + (usize)n1);
				return true;
			}
			// code == 3 : trames multiples.
			if (pos >= len)
				return false;
			const uint8 fcb = data[pos++]; // frame count byte
			const bool vbr = (fcb & 0x80) != 0;
			const bool pad = (fcb & 0x40) != 0;
			const int32 M = fcb & 0x3F;
			if (M < 1 || M > 48)
				return false;

			// Padding (RFC 6716 §3.2.5) : une suite d'octets ; 255 → +254 et continue, sinon +valeur.
			usize padBytes = 0;
			if (pad) {
				while (pos < len) {
					const uint8 pb = data[pos++];
					if (pb == 255) {
						padBytes += 254;
					} else {
						padBytes += pb;
						break;
					}
				}
			}

			out.frameCount = M;
			if (vbr) {
				// M-1 longueurs, dernière trame = reste.
				usize lenAcc = 0;
				for (int32 i = 0; i < M - 1; ++i) {
					int32 ni = 0;
					const int32 used = ReadFrameLength(data + pos, len - pos, ni);
					if (used == 0)
						return false;
					pos += (usize)used;
					out.frames[i].size = (usize)ni;
					lenAcc += (usize)ni;
				}
				// place les offsets après avoir lu toutes les longueurs.
				if (pos + lenAcc + padBytes > len)
					return false;
				usize off = pos;
				for (int32 i = 0; i < M - 1; ++i) {
					out.frames[i].offset = off;
					off += out.frames[i].size;
				}
				out.frames[M - 1].offset = off;
				const usize tail = len - padBytes;
				if (tail < off)
					return false;
				out.frames[M - 1].size = tail - off;
			} else {
				// CBR : (reste - padding) / M, tailles égales.
				if (len < pos + padBytes)
					return false;
				const usize rem = (len - padBytes) - pos;
				if (M == 0 || (rem % (usize)M) != 0)
					return false;
				const usize each = rem / (usize)M;
				usize off = pos;
				for (int32 i = 0; i < M; ++i) {
					out.frames[i].offset = off;
					out.frames[i].size = each;
					off += each;
				}
			}
			return true;
		}

		bool NkOpusPacket::SelfTest() {
			bool ok = true;

			// Table de config : quelques points clés.
			{
				NkOpusMode m;
				NkOpusBandwidth b;
				float32 ms;
				ConfigToParams(0, m, b, ms);
				if (m != NkOpusMode::NK_SILK_ONLY || b != NkOpusBandwidth::NK_NB || ms != 10.f)
					ok = false;
				ConfigToParams(15, m, b, ms);
				if (m != NkOpusMode::NK_HYBRID || b != NkOpusBandwidth::NK_FB || ms != 20.f)
					ok = false;
				ConfigToParams(31, m, b, ms);
				if (m != NkOpusMode::NK_CELT_ONLY || b != NkOpusBandwidth::NK_FB || ms != 20.f)
					ok = false;
			}

			// Code 0 : TOC + 10 octets → 1 trame de 10.
			{
				uint8 pkt[11];
				pkt[0] = (16 << 3) | 0; // config 16 (CELT FB 2.5ms), code 0
				for (int i = 1; i < 11; ++i)
					pkt[i] = (uint8)i;
				NkOpusPacketInfo info;
				if (!NkOpusPacket::Parse(pkt, 11, info) || info.frameCount != 1 || info.frames[0].size != 10)
					ok = false;
			}
			// Code 1 : 2 trames égales.
			{
				uint8 pkt[9];
				pkt[0] = (16 << 3) | 1; // code 1
				for (int i = 1; i < 9; ++i)
					pkt[i] = (uint8)i;
				NkOpusPacketInfo info;
				if (!NkOpusPacket::Parse(pkt, 9, info) || info.frameCount != 2 || info.frames[0].size != 4 ||
					info.frames[1].size != 4)
					ok = false;
			}
			// Code 2 : 1re trame longueur 3, reste 4.
			{
				uint8 pkt[9];
				pkt[0] = (16 << 3) | 2; // code 2
				pkt[1] = 3;				// longueur 1re trame
				for (int i = 2; i < 9; ++i)
					pkt[i] = (uint8)i;
				NkOpusPacketInfo info;
				if (!NkOpusPacket::Parse(pkt, 9, info) || info.frameCount != 2 || info.frames[0].size != 3 ||
					info.frames[1].size != 4)
					ok = false;
			}
			// Code 3 CBR : 3 trames de 2 octets.
			{
				uint8 pkt[8];
				pkt[0] = (16 << 3) | 3; // code 3
				pkt[1] = 3;				// fcb : VBR=0, pad=0, M=3
				for (int i = 2; i < 8; ++i)
					pkt[i] = (uint8)i;
				NkOpusPacketInfo info;
				if (!NkOpusPacket::Parse(pkt, 8, info) || info.frameCount != 3 || info.frames[0].size != 2 ||
					info.frames[2].size != 2)
					ok = false;
			}

			return ok;
		}

	} // namespace media
} // namespace nkentseu
