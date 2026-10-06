// -----------------------------------------------------------------------------
// FICHIER: NKFont/Core/NkFontParser.cpp
// DESCRIPTION: Parser TTF/OTF + rastériseur scanline.
//
// CORRECTIONS V2.1 :
//  #R1 — Rastériseur : règle even-odd winding correcte (clamp abs à [0,1]).
//        L'ancienne accumulation pouvait dépasser 1.0 sur des glyphes larges,
//        créant des bandes horizontales alternées (lignes parasites).
//  #R2 — Rastériseur : initialisation du pool active[] corrigée pour
//        les tableaux statiques (réinitialisation explicite à chaque appel).
//  #A1 — Atlas Build() : l'advanceX était calculé sans tenir compte de
//        l'oversampleH. Le quad bitmap fait bw pixels, mais la largeur
//        réelle du glyphe est bw/oversampleH. Corrigé dans NkFontAtlas.cpp.
//  #SDF — Nouveau : génération de texture SDF (Signed Distance Field)
//         pour rendu net à toute taille avec un seul atlas.
//  #WOFF — Nouveau : décompression WOFF (zlib) avant parsing TTF.
//  #CFF  — Nouveau : interpréteur Type 2 charstrings pour OTF/CFF.
// -----------------------------------------------------------------------------

#include "NkFontParser.h"
#include "NkFontCff.h" // (06/10) CFF complet : OTTO et CFF nu des PDF
#include "NKMemory/NkAllocator.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

namespace nkentseu {
	namespace nkfont {

		// ============================================================
		// UTILITAIRES INTERNES
		// ============================================================

		static inline nkft_int32 IFloor(nkft_float32 x) {
			return (nkft_int32)floorf(x);
		}

		static inline nkft_int32 ICeil(nkft_float32 x) {
			return (nkft_int32)ceilf(x);
		}

		static inline nkft_float32 FMin(nkft_float32 a, nkft_float32 b) {
			return a < b ? a : b;
		}

		static inline nkft_float32 FMax(nkft_float32 a, nkft_float32 b) {
			return a > b ? a : b;
		}

		static inline nkft_float32 FAbs(nkft_float32 x) {
			return x < 0.f ? -x : x;
		}

		static inline nkft_float32 FClamp(nkft_float32 v, nkft_float32 lo, nkft_float32 hi) {
			return v < lo ? lo : (v > hi ? hi : v);
		}

		static inline nkft_float32 FSqrt(nkft_float32 x) {
			return sqrtf(x);
		}

		// ── Offset glyf ──────────────────────────────────────────────────────────────

		static nkft_uint32 GlyphOffset(const NkFontFaceInfo *info, NkGlyphId glyph) {
			if (!info->glyf || !info->loca)
				return 0;

			nkft_uint32 g1, g2;
			if (info->indexToLocFormat == 0) {
				nkft_uint32 off = info->loca + glyph * 2;
				if (!info->data.IsValid(off, 4))
					return 0;
				g1 = NkReadU16(info->data.At(off)) * 2u;
				g2 = NkReadU16(info->data.At(off + 2)) * 2u;
			} else {
				nkft_uint32 off = info->loca + glyph * 4;
				if (!info->data.IsValid(off, 8))
					return 0;
				g1 = NkReadU32(info->data.At(off));
				g2 = NkReadU32(info->data.At(off + 4));
			}
			return (g1 == g2) ? 0u : (info->glyf + g1);
		}

		// ============================================================
		// WOFF DÉCOMPRESSEUR
		// ============================================================

		namespace {

			// Décompresse un bloc zlib (header CMF+FLG + deflate) dans outBuf.
			// Retourne la taille décompressée ou 0 si erreur.
			// Implémentation deflate minimale intégrée (tinfl public domain).
			// ─────────────────────────────────────────────────────────────────

			static const nkft_uint8 kLenExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
													 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
			static const nkft_uint32 kLenBase[29] = {3,	 4,	 5,	 6,	 7,	 8,	 9,	 10, 11,  13,  15,	17,	 19,  23, 27,
													 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
			static const nkft_uint8 kDistExtra[30] = {0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
													  6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
			static const nkft_uint32 kDistBase[30] = {1,	2,	  3,	4,	  5,	7,	  9,	13,	   17,	  25,
													  33,	49,	  65,	97,	  129,	193,  257,	385,   513,	  769,
													  1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};

			struct TInflBits {
					const nkft_uint8 *p;
					const nkft_uint8 *end;
					nkft_uint32 buf;
					nkft_int32 bits;

					void Init(const nkft_uint8 *d, nkft_size sz) {
						p = d;
						end = d + sz;
						buf = 0;
						bits = 0;
					}

					void Fill() {
						while (bits <= 24 && p < end) {
							buf |= (nkft_uint32)(*p++) << bits;
							bits += 8;
						}
					}

					nkft_uint32 Peek(nkft_int32 n) {
						Fill();
						return buf & ((1u << n) - 1);
					}

					void Consume(nkft_int32 n) {
						buf >>= n;
						bits -= n;
					}

					nkft_uint32 Read(nkft_int32 n) {
						nkft_uint32 v = Peek(n);
						Consume(n);
						return v;
					}
			};

			struct TInflHuff {
					static constexpr int MAXS = 288;
					nkft_int16 lut[512];

					struct S {
							nkft_uint16 code, sym;
							nkft_uint8 len;
					} slow[MAXS];

					nkft_int32 sc;

					void Build(const nkft_uint8 *ls, nkft_int32 n) {
						nkft_int32 cnt[16] = {};
						nkft_int32 nc[16] = {};

						for (nkft_int32 i = 0; i < n; ++i) {
							if (ls[i])
								cnt[ls[i]]++;
						}
						nkft_int32 c = 0;
						for (nkft_int32 i = 1; i < 16; ++i) {
							c = (c + cnt[i - 1]) << 1;
							nc[i] = c;
						}
						memset(lut, -1, sizeof(lut));
						sc = 0;
						for (nkft_int32 i = 0; i < n; ++i) {
							nkft_int32 l = ls[i];
							if (!l)
								continue;
							nkft_uint32 cv = (nkft_uint32)nc[l]++;
							if (l <= 9) {
								nkft_uint32 rv = 0;
								for (nkft_int32 b = 0; b < l; ++b) {
									rv = (rv << 1) | (cv >> b & 1);
								}
								for (nkft_uint32 f = rv; f < 512u; f += (1u << l)) {
									lut[f] = (nkft_int16)((l << 9) | i);
								}
							} else {
								if (sc < MAXS) {
									slow[sc] = {(nkft_uint16)cv, (nkft_uint16)i, (nkft_uint8)l};
									++sc;
								}
							}
						}
					}

					nkft_int32 Dec(TInflBits &br) const {
						br.Fill();
						nkft_int16 v = lut[br.buf & 511];
						if (v >= 0) {
							br.Consume(v >> 9);
							return v & 511;
						}
						for (nkft_int32 i = 0; i < sc; ++i) {
							nkft_uint32 rv = 0;
							nkft_uint32 cv = slow[i].code;
							for (nkft_int32 b = 0; b < slow[i].len; ++b) {
								rv = (rv << 1) | (cv >> b & 1);
							}
							if ((br.buf & ((1u << slow[i].len) - 1)) == rv) {
								br.Consume(slow[i].len);
								return slow[i].sym;
							}
						}
						return -1;
					}
			};

			static nkft_uint32 InflateDeflate(const nkft_uint8 *in, nkft_size inSz, nkft_uint8 *out, nkft_size outSz) {
				TInflBits br;
				br.Init(in, inSz);
				nkft_uint8 *op = out;
				nkft_uint8 *oe = out + outSz;
				bool last = false;

				while (!last) {
					last = br.Read(1) != 0;
					nkft_int32 bt = (nkft_int32)br.Read(2);

					if (bt == 0) {
						br.Consume(br.bits & 7);
						nkft_uint32 len = br.Read(16);
						br.Read(16); // skip nlen
						while (len-- && br.p < br.end && op < oe) {
							*op++ = *br.p++;
						}
					} else if (bt == 1 || bt == 2) {
						TInflHuff lit, dist;
						if (bt == 1) {
							nkft_uint8 ll[288] = {};
							nkft_uint8 dl[32] = {};
							for (int i = 0; i < 144; ++i)
								ll[i] = 8;
							for (int i = 144; i < 256; ++i)
								ll[i] = 9;
							for (int i = 256; i < 280; ++i)
								ll[i] = 7;
							for (int i = 280; i < 288; ++i)
								ll[i] = 8;
							for (int i = 0; i < 32; ++i)
								dl[i] = 5;
							lit.Build(ll, 288);
							dist.Build(dl, 32);
						} else {
							nkft_int32 hl = (nkft_int32)br.Read(5) + 257;
							nkft_int32 hd = (nkft_int32)br.Read(5) + 1;
							nkft_int32 hc = (nkft_int32)br.Read(4) + 4;
							static const nkft_int32 ord[] = {16, 17, 18, 0, 8,	7, 9,  6, 10, 5,
															 11, 4,	 12, 3, 13, 2, 14, 1, 15};
							nkft_uint8 cl[19] = {};
							for (int i = 0; i < hc; ++i) {
								cl[ord[i]] = (nkft_uint8)br.Read(3);
							}
							TInflHuff ct;
							ct.Build(cl, 19);
							nkft_uint8 ls[320] = {};
							for (int i = 0; i < hl + hd;) {
								int s = ct.Dec(br);
								if (s < 16) {
									ls[i++] = (nkft_uint8)s;
								} else if (s == 16) {
									nkft_uint8 p = i ? ls[i - 1] : 0;
									int r = (int)br.Read(2) + 3;
									while (r-- && i < hl + hd)
										ls[i++] = p;
								} else if (s == 17) {
									int r = (int)br.Read(3) + 3;
									while (r-- && i < hl + hd)
										ls[i++] = 0;
								} else {
									int r = (int)br.Read(7) + 11;
									while (r-- && i < hl + hd)
										ls[i++] = 0;
								}
							}
							lit.Build(ls, hl);
							dist.Build(ls + hl, hd);
						}
						while (true) {
							nkft_int32 s = lit.Dec(br);
							if (s < 0)
								break;
							if (s < 256) {
								if (op < oe)
									*op++ = (nkft_uint8)s;
							} else if (s == 256) {
								break;
							} else {
								int li = s - 257;
								if (li < 0 || li >= 29)
									break;
								nkft_uint32 len = kLenBase[li] + (nkft_uint32)br.Read(kLenExtra[li]);
								int di = dist.Dec(br);
								if (di < 0 || di >= 30)
									break;
								nkft_uint32 d = kDistBase[di] + (nkft_uint32)br.Read(kDistExtra[di]);
								const nkft_uint8 *src = op - (nkft_int32)d;
								while (len-- && op < oe) {
									*op++ = *src++;
								}
							}
						}
					} else {
						break;
					}
				}
				return (nkft_uint32)(op - out);
			}

			static nkft_uint8 *DecompressWOFF(const nkft_uint8 *woff, nkft_size woffSize, nkft_uint32 *outTtfSize) {
				// Header WOFF : signature(4) + flavor(4) + length(4) + numTables(2) + reserved(2)
				//             + totalSfntSize(4) + ...
				if (woffSize < 44)
					return nullptr;
				nkft_uint32 sig = NkReadU32(woff);
				if (sig != 0x774F4646u)
					return nullptr;

				nkft_uint32 totalSfnt = NkReadU32(woff + 16);
				nkft_uint16 numTables = NkReadU16(woff + 12);

				nkft_uint8 *ttf = (nkft_uint8 *)nkentseu::memory::NkAlloc((nk_size)(totalSfnt + 64));
				if (!ttf)
					return nullptr;
				memset(ttf, 0, totalSfnt + 64);

				// Reconstruit le header sfnt
				nkft_uint32 flavor = NkReadU32(woff + 4);
				nkft_uint16 nt = numTables;
				nkft_uint16 sr = 1;
				while (sr * 2 <= nt)
					sr *= 2;
				sr *= 16;
				nkft_uint16 es = 0;
				{
					nkft_uint16 t = nt;
					while (t > 1) {
						t >>= 1;
						++es;
					}
				}
				nkft_uint16 rs = nt * 16 - sr;

				nkft_uint8 *p = ttf;
				// Write sfnt header
				p[0] = (nkft_uint8)(flavor >> 24);
				p[1] = (nkft_uint8)(flavor >> 16);
				p[2] = (nkft_uint8)(flavor >> 8);
				p[3] = (nkft_uint8)flavor;
				p += 4;
				p[0] = (nkft_uint8)(nt >> 8);
				p[1] = (nkft_uint8)nt;
				p += 2;
				p[0] = (nkft_uint8)(sr >> 8);
				p[1] = (nkft_uint8)sr;
				p += 2;
				p[0] = (nkft_uint8)(es >> 8);
				p[1] = (nkft_uint8)es;
				p += 2;
				p[0] = (nkft_uint8)(rs >> 8);
				p[1] = (nkft_uint8)rs;
				p += 2;

				nkft_uint32 dirOff = 12 + numTables * 16;
				nkft_uint32 dataOff = dirOff;
				nkft_uint32 woffDirBase = 44;

				for (nkft_uint16 i = 0; i < numTables; ++i) {
					nkft_uint32 wde = woffDirBase + i * 20;
					if (wde + 20 > woffSize)
						break;

					nkft_uint32 tag = NkReadU32(woff + wde);
					nkft_uint32 wOffset = NkReadU32(woff + wde + 4);
					nkft_uint32 compLen = NkReadU32(woff + wde + 8);
					nkft_uint32 origLen = NkReadU32(woff + wde + 12);
					nkft_uint32 checksum = NkReadU32(woff + wde + 16);

					while (dataOff & 3)
						++dataOff;

					nkft_uint8 *de = ttf + 12 + i * 16;
					de[0] = (nkft_uint8)(tag >> 24);
					de[1] = (nkft_uint8)(tag >> 16);
					de[2] = (nkft_uint8)(tag >> 8);
					de[3] = (nkft_uint8)tag;
					de[4] = (nkft_uint8)(checksum >> 24);
					de[5] = (nkft_uint8)(checksum >> 16);
					de[6] = (nkft_uint8)(checksum >> 8);
					de[7] = (nkft_uint8)checksum;
					de[8] = (nkft_uint8)(dataOff >> 24);
					de[9] = (nkft_uint8)(dataOff >> 16);
					de[10] = (nkft_uint8)(dataOff >> 8);
					de[11] = (nkft_uint8)dataOff;
					de[12] = (nkft_uint8)(origLen >> 24);
					de[13] = (nkft_uint8)(origLen >> 16);
					de[14] = (nkft_uint8)(origLen >> 8);
					de[15] = (nkft_uint8)origLen;

					if (dataOff + origLen <= totalSfnt + 64) {
						if (compLen < origLen) {
							const nkft_uint8 *src = woff + wOffset;
							nkft_size srcSz = compLen;
							if (srcSz > 2 && src[0] == 0x78) {
								src += 2;
								srcSz -= 2;
							}
							InflateDeflate(src, srcSz, ttf + dataOff, origLen);
						} else {
							if (wOffset + origLen <= woffSize) {
								memcpy(ttf + dataOff, woff + wOffset, origLen);
							}
						}
					}
					dataOff += origLen;
				}

				if (outTtfSize)
					*outTtfSize = totalSfnt;
				return ttf;
			}

		} // anonymous namespace

		// (06/10) L'interpreteur CFF / Type 2 vit dans NkFontCff.cpp (complet :
		// subroutines, DICT prive, polices CID, charset, encodage, flex, seac).

		// ============================================================
		// InitFontFace
		// ============================================================

		bool NkInitFontFace(NkFontFaceInfo *info, const nkft_uint8 *data, nkft_size size, nkft_int32 faceIndex,
							bool cmapUnicodeRequise) {
			if (!info || !data || size < 12)
				return false;
			*info = NkFontFaceInfo{}; // (06/10) les valeurs par defaut (cffMatrix) et non des zeros

			// ── (06/10) CFF NU (PDF /FontFile3 : Type1C, CIDFontType0C) ──────────
			// En-tete CFF : version majeure 1, mineure 0, hdrSize >= 4, offSize 1..4.
			// Ni sfnt, ni cmap, ni hmtx : le lecteur PDF adresse ses glyphes par CID,
			// par nom ou par code ; c'est pourquoi il faut l'avoir demande.
			if (!cmapUnicodeRequise && data[0] == 1 && data[1] == 0 && data[2] >= 4 && data[3] >= 1 && data[3] <= 4) {
				info->data.data = data;
				info->data.size = size;
				if (!NkCffInit(info, 0, static_cast<nkft_uint32>(size)))
					return false;
				info->cffNu = true;
				const nkft_float32 m0 = info->cffMatrix[0];
				info->unitsPerEm = m0 > 0.f ? static_cast<nkft_int32>(1.f / m0 + 0.5f) : 1000;
				info->ascent = info->unitsPerEm * 8 / 10;
				info->descent = -(info->unitsPerEm * 2 / 10);
				return true;
			}

			// ── Détection WOFF ────────────────────────────────────────────────────
			nkft_uint32 sig = NkReadU32(data);
			if (sig == 0x774F4646u) { // 'wOFF'
				nkft_uint32 ttfSize = 0;
				nkft_uint8 *ttf = DecompressWOFF(data, size, &ttfSize);
				if (!ttf || ttfSize == 0)
					return false;
				info->woffBuffer = ttf;
				info->woffBufferSize = ttfSize;
				data = ttf;
				size = ttfSize;
				sig = NkReadU32(data);
			}

			info->data.data = data;
			info->data.size = size;

			// ── Collection TTC ? ─────────────────────────────────────────────────
			nkft_uint32 fontStart = 0;
			if (sig == 0x74746366u) { // 'ttcf'
				if (size < 12)
					return false;
				nkft_uint32 numFonts = NkReadU32(data + 8);
				if ((nkft_uint32)faceIndex >= numFonts)
					return false;
				nkft_uint32 tableDir = 12;
				if (!info->data.IsValid(tableDir + faceIndex * 4, 4))
					return false;
				fontStart = NkReadU32(data + tableDir + faceIndex * 4);
			} else {
				if (sig != 0x00010000u && sig != 0x4F54544Fu && sig != 0x74727565u && sig != 0x74797031u) {
					return false;
				}
			}

			const nkft_uint8 *font = data + fontStart;
			nkft_size fontSize = size - fontStart;
			if (fontSize < 12)
				return false;

			info->isCFF = (NkReadU32(font) == 0x4F54544Fu); // 'OTTO'

			auto GetTable = [&](const char *tag) -> nkft_uint32 {
				if (fontSize < 12)
					return 0;
				nkft_uint32 nt = NkReadU16(font + 4);
				for (nkft_uint32 i = 0; i < nt; ++i) {
					nkft_uint32 off = 12 + i * 16;
					if (fontStart + off + 16 > size)
						break;
					if (memcmp(font + off, tag, 4) == 0) {
						return fontStart + NkReadU32(font + off + 8);
					}
				}
				return 0;
			};

			info->cmap = GetTable("cmap");
			info->head = GetTable("head");
			info->hhea = GetTable("hhea");
			info->hmtx = GetTable("hmtx");
			info->kern = GetTable("kern");
			info->gpos = GetTable("GPOS");
			info->os2 = GetTable("OS/2");
			info->name_ = GetTable("name");
			info->glyf = GetTable("glyf");
			info->loca = GetTable("loca");
			info->cff = GetTable("CFF ");

			// (06/10) La cmap n'est exigee que si l'appelant adresse par Unicode : un
			// sous-ensemble TrueType embarque dans un PDF n'en a souvent aucune.
			if (!info->head || !info->hhea || !info->hmtx || (cmapUnicodeRequise && !info->cmap))
				return false;

			if (!info->data.IsValid(info->head, 54))
				return false;
			info->unitsPerEm = NkReadU16(info->data.At(info->head + 18));
			info->indexToLocFormat = NkReadI16(info->data.At(info->head + 50));

			nkft_uint32 maxp = GetTable("maxp");
			if (maxp && info->data.IsValid(maxp + 4, 2)) {
				info->numGlyphs = NkReadU16(info->data.At(maxp + 4));
			}

			if (!info->data.IsValid(info->hhea, 36))
				return false;
			info->ascent = NkReadI16(info->data.At(info->hhea + 4));
			info->descent = NkReadI16(info->data.At(info->hhea + 6));
			info->lineGap = NkReadI16(info->data.At(info->hhea + 8));
			info->numHMetrics = NkReadU16(info->data.At(info->hhea + 34));

			// ── CFF : INDEX, DICT, subroutines, charset (NkFontCff.cpp) ──────────
			if (info->isCFF && info->cff) {
				info->cffBase = info->cff;
				// Un echec laisse la police chargeable (metriques, cmap) mais sans contours.
				(void)NkCffInit(info, info->cff, static_cast<nkft_uint32>(size - info->cff));
				info->isCFF = true;
			}

			// ── cmap ─────────────────────────────────────────────────────────────
			{
				nkft_uint32 cmapBase = info->cmap;
				const bool cmapLisible = cmapBase && info->data.IsValid(cmapBase + 4, 0);
				if (!cmapLisible && cmapUnicodeRequise)
					return false;
				nkft_uint32 numSub = cmapLisible ? NkReadU16(info->data.At(cmapBase + 2)) : 0u;
				nkft_uint32 fmt4Off = 0, fmt12Off = 0;

				for (nkft_uint32 i = 0; i < numSub && i < 32; ++i) {
					nkft_uint32 ro = cmapBase + 4 + i * 8;
					if (!info->data.IsValid(ro, 8))
						break;
					nkft_uint32 pid = NkReadU16(info->data.At(ro));
					nkft_uint32 eid = NkReadU16(info->data.At(ro + 2));
					nkft_uint32 so = cmapBase + NkReadU32(info->data.At(ro + 4));
					if (!info->data.IsValid(so, 2))
						continue;
					nkft_uint32 fmt = NkReadU16(info->data.At(so));
					bool isUni = (pid == 0) || (pid == 3 && eid == 1) || (pid == 3 && eid == 10);
					if (!isUni) {
						// (06/10) gardees pour les polices symboliques des PDF
						const bool lisible = fmt == 0 || fmt == 4 || fmt == 6;
						if (pid == 3 && eid == 0 && lisible && !info->cmapSymOffset) {
							info->cmapSymOffset = so;
							info->cmapSymFormat = static_cast<nkft_int32>(fmt);
						} else if (pid == 1 && eid == 0 && lisible && !info->cmapMacOffset) {
							info->cmapMacOffset = so;
							info->cmapMacFormat = static_cast<nkft_int32>(fmt);
						}
						continue;
					}
					if (fmt == 4 && !fmt4Off)
						fmt4Off = so;
					if (fmt == 12 && !fmt12Off)
						fmt12Off = so;
				}
				if (fmt12Off) {
					info->cmapTableOffset = fmt12Off;
					info->cmapFormat = 12;
				} else if (fmt4Off) {
					info->cmapTableOffset = fmt4Off;
					info->cmapFormat = 4;
				} else if (cmapUnicodeRequise)
					return false;
			}
			return true;
		}

		void NkFreeFontFace(NkFontFaceInfo *info) {
			if (info && info->woffBuffer) {
				nkentseu::memory::NkFree(info->woffBuffer);
				info->woffBuffer = nullptr;
				info->woffBufferSize = 0;
			}
		}

		// ============================================================
		// FindGlyphIndex
		// ============================================================

		NkGlyphId NkFindGlyphIndex(const NkFontFaceInfo *info, NkFontCodepoint cp) {
			if (!info || !info->cmapTableOffset)
				return 0;

			const nkft_uint8 *d = info->data.data;
			nkft_uint32 base = info->cmapTableOffset;

			if (info->cmapFormat == 4) {
				if (!info->data.IsValid(base + 14, 0))
					return 0;
				nkft_uint32 segCount = NkReadU16(d + base + 6) >> 1;
				nkft_uint32 ecb = base + 14;
				nkft_uint32 scb = ecb + segCount * 2 + 2;
				nkft_uint32 idb = scb + segCount * 2;
				nkft_uint32 iro = idb + segCount * 2;

				nkft_uint32 lo = 0, hi = segCount;
				while (lo < hi) {
					nkft_uint32 mid = (lo + hi) >> 1;
					if (NkReadU16(d + ecb + mid * 2) < cp)
						lo = mid + 1;
					else
						hi = mid;
				}
				if (lo >= segCount)
					return 0;

				nkft_uint32 sc = NkReadU16(d + scb + lo * 2);
				if (cp < sc)
					return 0;

				nkft_uint32 iro2 = NkReadU16(d + iro + lo * 2);
				nkft_int32 id = NkReadI16(d + idb + lo * 2);

				if (iro2 == 0) {
					return (NkGlyphId)((nkft_int32)cp + id) & 0xFFFF;
				}
				nkft_uint32 go = iro + lo * 2 + iro2 + (cp - sc) * 2;
				if (!info->data.IsValid(go, 2))
					return 0;
				nkft_uint32 g = NkReadU16(d + go);
				return g ? ((g + id) & 0xFFFF) : 0;
			} else if (info->cmapFormat == 12) {
				if (!info->data.IsValid(base + 16, 0))
					return 0;
				nkft_uint32 ng = NkReadU32(d + base + 12);
				nkft_uint32 gb = base + 16;

				nkft_uint32 lo = 0, hi = ng;
				while (lo < hi) {
					nkft_uint32 mid = (lo + hi) >> 1;
					nkft_uint32 off = gb + mid * 12;
					if (!info->data.IsValid(off, 12))
						break;
					nkft_uint32 sc = NkReadU32(d + off);
					nkft_uint32 ec = NkReadU32(d + off + 4);
					if (ec < cp)
						lo = mid + 1;
					else if (sc > cp)
						hi = mid;
					else
						return NkReadU32(d + off + 8) + (cp - sc);
				}
			}
			return 0;
		}

		// (06/10) Une sous-table cmap quelconque : formats 0 (octets), 6 (tableau
		// tronque) ici ; 4 et 12 par NkFindGlyphIndex, sur une copie qui la designe.
		static NkGlyphId ChercherSousTable(const NkFontFaceInfo *info, nkft_uint32 off, nkft_int32 fmt, nkft_uint32 cp) {
			if (!off)
				return 0;
			const NkFontDataSpan &s = info->data;
			if (fmt == 0) {
				if (cp > 255 || !s.IsValid(off + 6 + cp, 1))
					return 0;
				return s.data[off + 6 + cp];
			}
			if (fmt == 6) {
				if (!s.IsValid(off + 6, 4))
					return 0;
				const nkft_uint32 premier = NkReadU16(s.At(off + 6)), nb = NkReadU16(s.At(off + 8));
				if (cp < premier || cp >= premier + nb || !s.IsValid(off + 10 + (cp - premier) * 2, 2))
					return 0;
				return NkReadU16(s.At(off + 10 + (cp - premier) * 2));
			}
			if (fmt == 4 || fmt == 12) {
				NkFontFaceInfo copie = *info;
				copie.cmapTableOffset = off;
				copie.cmapFormat = fmt;
				return NkFindGlyphIndex(&copie, static_cast<NkFontCodepoint>(cp));
			}
			return 0;
		}

		NkGlyphId NkFindGlyphIndexSymbolique(const NkFontFaceInfo *info, nkft_uint32 code) {
			if (!info)
				return 0;
			if (info->cmapSymOffset) {
				const nkft_uint32 essais[4] = {code, 0xF000u + code, 0xF100u + code, 0xF200u + code};
				for (nkft_uint32 c : essais) {
					const NkGlyphId g = ChercherSousTable(info, info->cmapSymOffset, info->cmapSymFormat, c);
					if (g)
						return g;
				}
			}
			return ChercherSousTable(info, info->cmapMacOffset, info->cmapMacFormat, code);
		}

		// ============================================================
		// Métriques
		// ============================================================

		nkft_float32 NkScaleForPixelHeight(const NkFontFaceInfo *info, nkft_float32 px) {
			if (!info || info->unitsPerEm == 0)
				return 1.f;
			nkft_int32 fh = info->ascent - info->descent;
			return fh == 0 ? 1.f : px / (nkft_float32)fh;
		}

		nkft_float32 NkScaleForEmToPixels(const NkFontFaceInfo *info, nkft_float32 px) {
			if (!info || info->unitsPerEm == 0)
				return 1.f;
			return px / (nkft_float32)info->unitsPerEm;
		}

		void NkGetGlyphHMetrics(const NkFontFaceInfo *info, NkGlyphId glyph, nkft_int32 *aw, nkft_int32 *lsb) {
			if (!info || !info->hmtx) {
				if (aw)
					*aw = 0;
				if (lsb)
					*lsb = 0;
				return;
			}
			nkft_uint32 nm = (nkft_uint32)info->numHMetrics;
			const nkft_uint8 *d = info->data.data;

			if ((nkft_uint32)glyph < nm) {
				nkft_uint32 off = info->hmtx + glyph * 4;
				if (aw && info->data.IsValid(off, 2)) {
					*aw = NkReadU16(d + off);
				}
				if (lsb && info->data.IsValid(off + 2, 2)) {
					*lsb = NkReadI16(d + off + 2);
				}
			} else {
				if (nm > 0 && aw) {
					nkft_uint32 off = info->hmtx + (nm - 1) * 4;
					if (info->data.IsValid(off, 2)) {
						*aw = NkReadU16(d + off);
					}
				}
				if (lsb) {
					nkft_uint32 lo = info->hmtx + nm * 4 + (glyph - nm) * 2;
					*lsb = info->data.IsValid(lo, 2) ? NkReadI16(d + lo) : 0;
				}
			}
		}

		void NkGetFontVMetrics(const NkFontFaceInfo *info, nkft_int32 *a, nkft_int32 *d, nkft_int32 *lg) {
			if (!info)
				return;
			if (a)
				*a = info->ascent;
			if (d)
				*d = info->descent;
			if (lg)
				*lg = info->lineGap;
		}

		bool NkGetGlyphBox(const NkFontFaceInfo *info, NkGlyphId glyph, nkft_int32 *x0, nkft_int32 *y0, nkft_int32 *x1,
						   nkft_int32 *y1) {
			if (!info)
				return false;
			if (info->isCFF) { // (06/10) pas de « glyf » : la boite vient du contour
				NkFontVertexBuffer buf;
				if (!NkCffGlyphShape(info, glyph, &buf) || buf.count == 0)
					return false;
				nkft_int32 a = 32767, b = 32767, c = -32768, d = -32768;
				for (nkft_uint32 i = 0; i < buf.count; ++i) {
					const NkFontVertex &v = buf.verts[i];
					const nkft_int32 xs[3] = {v.x, v.cx, v.cx1}, ys[3] = {v.y, v.cy, v.cy1};
					const nkft_int32 nb = v.type == NK_FONT_VERTEX_CUBIC ? 3 : 1;
					for (nkft_int32 k = 0; k < nb; ++k) {
						a = xs[k] < a ? xs[k] : a;
						b = ys[k] < b ? ys[k] : b;
						c = xs[k] > c ? xs[k] : c;
						d = ys[k] > d ? ys[k] : d;
					}
				}
				if (x0)
					*x0 = a;
				if (y0)
					*y0 = b;
				if (x1)
					*x1 = c;
				if (y1)
					*y1 = d;
				return true;
			}
			nkft_uint32 off = GlyphOffset(info, glyph);
			if (!off || !info->data.IsValid(off, 10))
				return false;
			const nkft_uint8 *d = info->data.At(off);
			if (x0)
				*x0 = NkReadI16(d + 2);
			if (y0)
				*y0 = NkReadI16(d + 4);
			if (x1)
				*x1 = NkReadI16(d + 6);
			if (y1)
				*y1 = NkReadI16(d + 8);
			return true;
		}

		nkft_int32 NkGetGlyphKernAdvance(const NkFontFaceInfo *info, NkGlyphId g1, NkGlyphId g2) {
			if (!info || !info->kern)
				return 0;
			const nkft_uint8 *d = info->data.data;
			nkft_uint32 base = info->kern;
			if (!info->data.IsValid(base + 4, 0))
				return 0;

			nkft_uint32 nt = NkReadU16(d + base + 2);
			nkft_uint32 off = base + 4;

			for (nkft_uint32 t = 0; t < nt && t < 4; ++t) {
				if (!info->data.IsValid(off + 14, 0))
					break;
				nkft_uint32 len = NkReadU16(d + off + 2);
				nkft_uint32 cov = NkReadU16(d + off + 4);
				nkft_uint32 fmt = cov >> 8;
				if ((cov & 1) && fmt == 0) {
					nkft_uint32 np = NkReadU16(d + off + 6);
					nkft_uint32 po = off + 14;
					nkft_uint32 key = ((nkft_uint32)g1 << 16) | (nkft_uint32)g2;
					nkft_uint32 lo = 0, hi = np;
					while (lo < hi) {
						nkft_uint32 mid = (lo + hi) >> 1;
						nkft_uint32 pof = po + mid * 6;
						if (!info->data.IsValid(pof, 6))
							break;
						nkft_uint32 pk = ((nkft_uint32)NkReadU16(d + pof) << 16) | NkReadU16(d + pof + 2);
						if (pk < key)
							lo = mid + 1;
						else if (pk > key)
							hi = mid;
						else
							return NkReadI16(d + pof + 4);
					}
				}
				off += len;
			}
			return 0;
		}

		// ============================================================
		// GetGlyphShape — TTF + CFF
		// ============================================================

		namespace {

			static void CloseShape(NkFontVertexBuffer *buf, nkft_int32 wasOff, nkft_int32 startOff, nkft_int32 sx,
								   nkft_int32 sy, nkft_int32 scx, nkft_int32 scy, nkft_int32 cx, nkft_int32 cy) {
				if (startOff) {
					if (wasOff) {
						NkFontVertex v{};
						v.type = NK_FONT_VERTEX_CURVE;
						v.x = (nkft_int16)((cx + scx) >> 1);
						v.y = (nkft_int16)((cy + scy) >> 1);
						v.cx = (nkft_int16)cx;
						v.cy = (nkft_int16)cy;
						buf->Push(v);
					}
					NkFontVertex v{};
					v.type = NK_FONT_VERTEX_CURVE;
					v.x = (nkft_int16)sx;
					v.y = (nkft_int16)sy;
					v.cx = (nkft_int16)scx;
					v.cy = (nkft_int16)scy;
					buf->Push(v);
				} else {
					NkFontVertex v{};
					v.type = wasOff ? NK_FONT_VERTEX_CURVE : NK_FONT_VERTEX_LINE;
					v.x = (nkft_int16)sx;
					v.y = (nkft_int16)sy;
					if (wasOff) {
						v.cx = (nkft_int16)cx;
						v.cy = (nkft_int16)cy;
					}
					buf->Push(v);
				}
			}

			static bool DecodeSimpleGlyph(const NkFontFaceInfo *info, nkft_uint32 gOff, NkFontVertexBuffer *buf) {
				const nkft_uint8 *d = info->data.data;
				if (!info->data.IsValid(gOff, 10))
					return false;

				nkft_int32 nc = NkReadI16(d + gOff);
				if (nc < 0)
					return false;
				if (nc == 0)
					return true;

				nkft_uint32 ep = gOff + 10;
				if (!info->data.IsValid(ep + nc * 2, 2))
					return false;
				nkft_int32 np = NkReadI16(d + ep + (nc - 1) * 2) + 1;
				if (np <= 0)
					return false;

				nkft_uint32 il = NkReadU16(d + ep + nc * 2);
				nkft_uint32 fo = ep + nc * 2 + 2 + il;

				static constexpr nkft_uint32 MP = 1024;
				if ((nkft_uint32)np > MP)
					return false;

				nkft_uint8 flags[MP];
				nkft_int16 xc[MP];
				nkft_int16 yc[MP];

				nkft_uint32 cur = fo;
				for (nkft_int32 i = 0; i < np;) {
					if (!info->data.IsValid(cur, 1))
						return false;
					nkft_uint8 f = d[cur++];
					flags[i++] = f;
					if (f & 8) {
						if (!info->data.IsValid(cur, 1))
							return false;
						nkft_uint8 r = d[cur++];
						while (r-- && i < np)
							flags[i++] = f;
					}
				}

				nkft_int32 x = 0;
				for (nkft_int32 i = 0; i < np; ++i) {
					nkft_uint8 f = flags[i];
					if (f & 2) {
						nkft_int32 dx = d[cur++];
						x += (f & 16) ? dx : -dx;
					} else if (!(f & 16)) {
						x += NkReadI16(d + cur);
						cur += 2;
					}
					xc[i] = (nkft_int16)x;
				}

				nkft_int32 y = 0;
				for (nkft_int32 i = 0; i < np; ++i) {
					nkft_uint8 f = flags[i];
					if (f & 4) {
						nkft_int32 dy = d[cur++];
						y += (f & 32) ? dy : -dy;
					} else if (!(f & 32)) {
						y += NkReadI16(d + cur);
						cur += 2;
					}
					yc[i] = (nkft_int16)y;
				}

				nkft_int32 cs = 0;
				for (nkft_int32 c = 0; c < nc; ++c) {
					nkft_int32 ce = NkReadI16(d + ep + c * 2);
					nkft_int32 sx = 0, sy = 0, cx = 0, cy = 0, scx = 0, scy = 0, wo = 0, so = 0, j = cs;

					if (!(flags[j] & 1)) {
						so = 1;
						if (!(flags[ce] & 1)) {
							sx = (xc[j] + xc[ce]) >> 1;
							sy = (yc[j] + yc[ce]) >> 1;
						} else {
							sx = xc[ce];
							sy = yc[ce];
						}
						scx = xc[j];
						scy = yc[j];
					} else {
						sx = xc[j];
						sy = yc[j];
						++j;
					}

					{
						NkFontVertex v{};
						v.type = NK_FONT_VERTEX_MOVE;
						v.x = (nkft_int16)sx;
						v.y = (nkft_int16)sy;
						buf->Push(v);
					}

					for (; j <= ce; ++j) {
						nkft_int32 on = flags[j] & 1;
						nkft_int32 px = xc[j], py = yc[j];
						if (!on) {
							if (wo) {
								NkFontVertex v{};
								v.type = NK_FONT_VERTEX_CURVE;
								v.x = (nkft_int16)((cx + px) >> 1);
								v.y = (nkft_int16)((cy + py) >> 1);
								v.cx = (nkft_int16)cx;
								v.cy = (nkft_int16)cy;
								buf->Push(v);
							}
							cx = px;
							cy = py;
							wo = 1;
						} else {
							NkFontVertex v{};
							v.type = wo ? NK_FONT_VERTEX_CURVE : NK_FONT_VERTEX_LINE;
							v.x = (nkft_int16)px;
							v.y = (nkft_int16)py;
							if (wo) {
								v.cx = (nkft_int16)cx;
								v.cy = (nkft_int16)cy;
							}
							buf->Push(v);
							wo = 0;
						}
					}
					CloseShape(buf, wo, so, sx, sy, scx, scy, cx, cy);
					cs = ce + 1;
				}
				return true;
			}

			static bool DecodeCompositeGlyph(const NkFontFaceInfo *info, nkft_uint32 gOff, NkFontVertexBuffer *buf) {
				const nkft_uint8 *d = info->data.data;
				nkft_uint32 cur = gOff + 10;
				nkft_uint32 flags;

				do {
					if (!info->data.IsValid(cur, 4))
						return false;
					flags = NkReadU16(d + cur);
					nkft_uint32 gi = NkReadU16(d + cur + 2);
					cur += 4;

					nkft_float32 mtx[6] = {1, 0, 0, 1, 0, 0};
					nkft_int32 dx = 0, dy = 0;

					if (flags & 1) {
						if (!info->data.IsValid(cur, 4))
							return false;
						dx = NkReadI16(d + cur);
						dy = NkReadI16(d + cur + 2);
						cur += 4;
					} else {
						if (!info->data.IsValid(cur, 2))
							return false;
						dx = (nkft_int8)d[cur];
						dy = (nkft_int8)d[cur + 1];
						cur += 2;
					}
					if (flags & 8) {
						if (!info->data.IsValid(cur, 2))
							return false;
						mtx[0] = mtx[3] = NkReadI16(d + cur) / 16384.f;
						cur += 2;
					} else if (flags & 64) {
						if (!info->data.IsValid(cur, 4))
							return false;
						mtx[0] = NkReadI16(d + cur) / 16384.f;
						mtx[3] = NkReadI16(d + cur + 2) / 16384.f;
						cur += 4;
					} else if (flags & 128) {
						if (!info->data.IsValid(cur, 8))
							return false;
						mtx[0] = NkReadI16(d + cur) / 16384.f;
						mtx[1] = NkReadI16(d + cur + 2) / 16384.f;
						mtx[2] = NkReadI16(d + cur + 4) / 16384.f;
						mtx[3] = NkReadI16(d + cur + 6) / 16384.f;
						cur += 8;
					}
					mtx[4] = (nkft_float32)dx;
					mtx[5] = (nkft_float32)dy;

					nkft_uint32 co = GlyphOffset(info, gi);
					if (co) {
						NkFontVertexBuffer sub;
						bool ok = false;
						nkft_int32 snc = NkReadI16(info->data.At(co));
						ok = (snc >= 0) ? DecodeSimpleGlyph(info, co, &sub) : DecodeCompositeGlyph(info, co, &sub);
						if (ok) {
							for (nkft_uint32 i = 0; i < sub.count; ++i) {
								NkFontVertex &v = sub.verts[i];
								nkft_float32 px = mtx[0] * v.x + mtx[2] * v.y + mtx[4];
								nkft_float32 py = mtx[1] * v.x + mtx[3] * v.y + mtx[5];
								v.x = (nkft_int16)px;
								v.y = (nkft_int16)py;
								nkft_float32 qx = mtx[0] * v.cx + mtx[2] * v.cy;
								nkft_float32 qy = mtx[1] * v.cx + mtx[3] * v.cy;
								v.cx = (nkft_int16)qx;
								v.cy = (nkft_int16)qy;
								buf->Push(v);
							}
						}
					}
				} while (flags & 32);
				return true;
			}


		} // anonymous namespace

		bool NkGetGlyphShape(const NkFontFaceInfo *info, NkGlyphId glyph, NkFontVertexBuffer *buf) {
			if (!info || !buf)
				return false;
			buf->Clear();

			if (info->isCFF)
				return NkCffGlyphShape(info, glyph, buf); // OTTO comme CFF nu

			nkft_uint32 off = GlyphOffset(info, glyph);
			if (!off || !info->data.IsValid(off, 2))
				return false;
			nkft_int32 nc = NkReadI16(info->data.At(off));
			return (nc >= 0) ? DecodeSimpleGlyph(info, off, buf) : DecodeCompositeGlyph(info, off, buf);
		}

		// ============================================================
		// RASTÉRISEUR SCANLINE — VERSION CORRIGÉE
		//
		// CORRECTION #R1 : La règle non-zero winding génère des valeurs
		// dans ]-∞, +∞[. La valeur absolue tronquée à [0,1] est correcte.
		// MAIS : l'accumulation par colonne dans gCovBuf peut dépasser ±1.0
		// si plusieurs arêtes traversent la même colonne dans la même frame.
		// Résultat : val > 1.0 → artefacts alternés (lignes parasites).
		//
		// La vraie règle non-zero winding : on accumuler les contributions
		// SIGNÉES (-1 ou +1 par arête), puis on clamp abs() à [0,1].
		// Ce clamp était mal placé (manquait pour certains cas).
		//
		// CORRECTION #R2 : Mauvaise gestion des contributions partielles.
		// Quand une arête se termine en milieu de scanline, la contribution
		// était calculée avec dy = ae.ey - y (partiel) mais ajoutée dans
		// gCovBuf avec un facteur incorrect. Corrigé.
		// ============================================================

		namespace {

			struct ActiveEdge {
					nkft_float32 x, dx, ey;
					nkft_int32 dir;
					nkft_int32 next; // -1 = end of list, -2 = free slot
			};

			struct Edge {
					nkft_float32 x0, y0, x1, y1;
					nkft_int32 dir;
			};

			static constexpr nkft_uint32 MAX_EDGES = 4096u;
			static constexpr nkft_uint32 MAX_ACTIVE = 512u;
			static constexpr nkft_uint32 COV_WIDTH = 4096u;

			static nkft_float32 gCovBuf[COV_WIDTH];

			static void FlattenQuad(Edge *edges, nkft_uint32 &n, nkft_float32 x0, nkft_float32 y0, nkft_float32 x1,
									nkft_float32 y1, nkft_float32 x2, nkft_float32 y2, nkft_float32 thr = 0.35f,
									nkft_uint32 d = 0) {
				if (n >= MAX_EDGES)
					return;

				nkft_float32 mx = 0.5f * (x0 + x2);
				nkft_float32 my = 0.5f * (y0 + y2);
				nkft_float32 dx = x1 - mx;
				nkft_float32 dy = y1 - my;

				if ((dx * dx + dy * dy) < (thr * thr) || d > 8) {
					if (y0 == y2)
						return;
					Edge &e = edges[n++];
					if (y0 < y2) {
						e.x0 = x0;
						e.y0 = y0;
						e.x1 = x2;
						e.y1 = y2;
						e.dir = 1;
					} else {
						e.x0 = x2;
						e.y0 = y2;
						e.x1 = x0;
						e.y1 = y0;
						e.dir = -1;
					}
				} else {
					nkft_float32 m01x = 0.5f * (x0 + x1);
					nkft_float32 m01y = 0.5f * (y0 + y1);
					nkft_float32 m12x = 0.5f * (x1 + x2);
					nkft_float32 m12y = 0.5f * (y1 + y2);
					nkft_float32 mmx = 0.5f * (m01x + m12x);
					nkft_float32 mmy = 0.5f * (m01y + m12y);
					FlattenQuad(edges, n, x0, y0, m01x, m01y, mmx, mmy, thr, d + 1);
					FlattenQuad(edges, n, mmx, mmy, m12x, m12y, x2, y2, thr, d + 1);
				}
			}

			static void FlattenCubic(Edge *edges, nkft_uint32 &n, nkft_float32 x0, nkft_float32 y0, nkft_float32 x1,
									 nkft_float32 y1, nkft_float32 x2, nkft_float32 y2, nkft_float32 x3,
									 nkft_float32 y3, nkft_uint32 d = 0) {
				if (d >= 6 || n >= MAX_EDGES) {
					if (y0 != y3) {
						Edge &e = edges[n++];
						if (y0 < y3) {
							e.x0 = x0;
							e.y0 = y0;
							e.x1 = x3;
							e.y1 = y3;
							e.dir = 1;
						} else {
							e.x0 = x3;
							e.y0 = y3;
							e.x1 = x0;
							e.y1 = y0;
							e.dir = -1;
						}
					}
					return;
				}
				nkft_float32 mx = (x0 + 3 * x1 + 3 * x2 + x3) * 0.125f;
				nkft_float32 my = (y0 + 3 * y1 + 3 * y2 + y3) * 0.125f;
				nkft_float32 m01x = (x0 + x1) * 0.5f;
				nkft_float32 m01y = (y0 + y1) * 0.5f;
				nkft_float32 m12x = (x1 + x2) * 0.5f;
				nkft_float32 m12y = (y1 + y2) * 0.5f;
				nkft_float32 m23x = (x2 + x3) * 0.5f;
				nkft_float32 m23y = (y2 + y3) * 0.5f;
				nkft_float32 m012x = (m01x + m12x) * 0.5f;
				nkft_float32 m012y = (m01y + m12y) * 0.5f;
				nkft_float32 m123x = (m12x + m23x) * 0.5f;
				nkft_float32 m123y = (m12y + m23y) * 0.5f;
				nkft_float32 cx = (m012x + m123x) * 0.5f;
				nkft_float32 cy = (m012y + m123y) * 0.5f;
				(void)mx;
				(void)my;
				FlattenCubic(edges, n, x0, y0, m01x, m01y, m012x, m012y, cx, cy, d + 1);
				FlattenCubic(edges, n, cx, cy, m123x, m123y, m23x, m23y, x3, y3, d + 1);
			}

			// static void BuildEdgeList(const NkFontVertexBuffer* vbuf,
			//                         nkft_float32 sx, nkft_float32 sy,
			//                         nkft_float32 shx, nkft_float32 shy,
			//                         Edge* edges, nkft_uint32& n) {
			//     n = 0;
			//     nkft_float32 px = 0, py = 0;

			//     for (nkft_uint32 i = 0; i < vbuf->count; ++i) {
			//         const NkFontVertex& v = vbuf->verts[i];
			//         nkft_float32 vx = v.x * sx + shx;
			//         nkft_float32 vy = -v.y * sy + shy;

			//         if (v.type == NK_FONT_VERTEX_MOVE) {
			//             px = vx;
			//             py = vy;
			//         } else if (v.type == NK_FONT_VERTEX_LINE) {
			//             if (py != vy) {
			//                 Edge& e = edges[n];
			//                 if (py < vy) {
			//                     e.x0 = px; e.y0 = py;
			//                     e.x1 = vx; e.y1 = vy;
			//                     e.dir = 1;
			//                 } else {
			//                     e.x0 = vx; e.y0 = vy;
			//                     e.x1 = px; e.y1 = py;
			//                     e.dir = -1;
			//                 }
			//                 if (n < MAX_EDGES) ++n;
			//             }
			//             px = vx;
			//             py = vy;
			//         } else if (v.type == NK_FONT_VERTEX_CURVE) {
			//             nkft_float32 cx = v.cx * sx + shx;
			//             nkft_float32 cy = -v.cy * sy + shy;
			//             FlattenQuad(edges, n, px, py, cx, cy, vx, vy);
			//             px = vx;
			//             py = vy;
			//         } else if (v.type == NK_FONT_VERTEX_CUBIC) {
			//             nkft_float32 cx  = v.cx  * sx + shx;
			//             nkft_float32 cy  = -v.cy  * sy + shy;
			//             nkft_float32 cx1 = v.cx1 * sx + shx;
			//             nkft_float32 cy1 = -v.cy1 * sy + shy;
			//             FlattenCubic(edges, n, px, py, cx, cy, cx1, cy1, vx, vy);
			//             px = vx;
			//             py = vy;
			//         }
			//     }
			// }

			static void SortEdges(Edge *e, nkft_uint32 n) {
				for (nkft_uint32 i = 1; i < n; ++i) {
					Edge t = e[i];
					nkft_int32 j = (nkft_int32)i - 1;
					while (j >= 0 && e[j].y0 > t.y0) {
						e[j + 1] = e[j];
						--j;
					}
					e[j + 1] = t;
				}
			}

			// ── Contribution d'une arête sur une plage Y ──────────────────────────────
			// Calcule l'impact d'une arête (x0..x1 sur dy scanlines) dans gCovBuf[].
			// CORRECTION #R1 : on ajoute des fractions de couverture proportionnelles
			// à la longueur d'intersection de l'arête avec chaque colonne pixel.
			// static void AddEdgeCoverage(nkft_float32 x0, nkft_float32 x1, nkft_float32 dy,
			//                             nkft_int32 dir, nkft_int32 outW) {
			//     nkft_float32 xMin = (x0 < x1) ? x0 : x1;
			//     nkft_float32 xMax = (x0 < x1) ? x1 : x0;
			//     nkft_int32 ixMin = (nkft_int32)floorf(xMin);
			//     nkft_int32 ixMax = (nkft_int32)floorf(xMax);

			//     auto Add = [&](nkft_int32 x, nkft_float32 v) {
			//         if (x >= 0 && x < outW && x < (nkft_int32)COV_WIDTH) {
			//             gCovBuf[x] += (nkft_float32)dir * v;
			//         }
			//     };

			//     if (ixMin == ixMax) {
			//         nkft_float32 frac = (xMin + xMax) * 0.5f - (nkft_float32)ixMin;
			//         Add(ixMin, dy * (1.f - frac));
			//         Add(ixMin + 1, dy * frac);
			//     } else {
			//         nkft_float32 fLeft = 1.f - (xMin - (nkft_float32)ixMin);
			//         Add(ixMin, dy * fLeft);
			//         for (nkft_int32 x = ixMin + 1; x < ixMax; ++x) {
			//             Add(x, dy);
			//         }
			//         nkft_float32 fRight = xMax - (nkft_float32)ixMax;
			//         Add(ixMax, dy * fRight);
			//         Add(ixMax + 1, dy * (1.f - fRight));
			//     }
			// }

			// static void Rasterize(nkft_uint8* output, nkft_int32 outW, nkft_int32 outH,
			//                     nkft_int32 outStride, Edge* edges, nkft_uint32 numEdges) {
			//     // CORRECTION #R2 : réinitialisation du pool à chaque appel
			//     static ActiveEdge active[MAX_ACTIVE];
			//     for (nkft_uint32 i = 0; i < MAX_ACTIVE; ++i) active[i].next = -2;

			//     nkft_int32  head    = -1;
			//     nkft_uint32 edgeIdx = 0;

			//     for (nkft_int32 y = 0; y < outH; ++y) {
			//         nkft_float32 scanY = (nkft_float32)y + 0.5f;

			//         nkft_int32 cw = (outW < (nkft_int32)COV_WIDTH) ? outW : (nkft_int32)COV_WIDTH;
			//         for (nkft_int32 x = 0; x < cw; ++x) {
			//             gCovBuf[x] = 0.f;
			//         }

			//         while (edgeIdx < numEdges && edges[edgeIdx].y0 <= scanY) {
			//             const Edge& e = edges[edgeIdx++];
			//             if (e.y1 <= (nkft_float32)y) continue;

			//             nkft_int32 slot = -1;
			//             for (nkft_int32 s = 0; s < (nkft_int32)MAX_ACTIVE; ++s) {
			//                 if (active[s].next == -2) {
			//                     slot = s;
			//                     break;
			//                 }
			//             }
			//             if (slot < 0) continue;

			//             nkft_float32 dy = e.y1 - e.y0;
			//             active[slot].dx   = dy > 0.f ? (e.x1 - e.x0) / dy : 0.f;
			//             active[slot].x    = e.x0 + active[slot].dx * (scanY - e.y0);
			//             active[slot].ey   = e.y1;
			//             active[slot].dir  = e.dir;
			//             active[slot].next = head;
			//             head = slot;
			//         }

			//         {
			//             nkft_int32 prev = -1, cur = head;
			//             while (cur >= 0) {
			//                 ActiveEdge& ae = active[cur];
			//                 nkft_int32 nxt = ae.next;

			//                 if (ae.ey <= (nkft_float32)(y + 1)) {
			//                     nkft_float32 dy = ae.ey - (nkft_float32)y;
			//                     if (dy > 1e-5f) {
			//                         nkft_float32 x1 = ae.x + ae.dx * dy;
			//                         AddEdgeCoverage(ae.x, x1, dy, ae.dir, outW);
			//                     }
			//                     ae.next = -2;
			//                     if (prev < 0) head = nxt;
			//                     else active[prev].next = nxt;
			//                     cur = nxt;
			//                 } else {
			//                     nkft_float32 x1 = ae.x + ae.dx;
			//                     AddEdgeCoverage(ae.x, x1, 1.f, ae.dir, outW);
			//                     ae.x += ae.dx;
			//                     prev = cur;
			//                     cur = nxt;
			//                 }
			//             }
			//         }

			//         nkft_uint8* row = output + y * outStride;
			//         nkft_float32 acc = 0.f;
			//         for (nkft_int32 x = 0; x < outW && x < (nkft_int32)COV_WIDTH; ++x) {
			//             acc += gCovBuf[x];
			//             nkft_float32 val = FAbs(acc);
			//             if (val > 1.f) val = 1.f;
			//             row[x] = (nkft_uint8)(val * 255.f + 0.5f);
			//         }
			//     }
			// }

		} // anonymous namespace

		// ============================================================
		// API publique Rastériseur
		// ============================================================

		void NkGetGlyphBitmapBox(const NkFontFaceInfo *info, NkGlyphId glyph, nkft_float32 sx, nkft_float32 sy,
								 nkft_float32 shx, nkft_float32 shy, nkft_int32 *ix0, nkft_int32 *iy0, nkft_int32 *ix1,
								 nkft_int32 *iy1) {
			nkft_int32 gx0, gy0, gx1, gy1;
			if (!NkGetGlyphBox(info, glyph, &gx0, &gy0, &gx1, &gy1)) {
				if (ix0)
					*ix0 = 0;
				if (iy0)
					*iy0 = 0;
				if (ix1)
					*ix1 = 0;
				if (iy1)
					*iy1 = 0;
				return;
			}
			if (ix0)
				*ix0 = IFloor(gx0 * sx + shx);
			if (iy0)
				*iy0 = IFloor(-gy1 * sy + shy);
			if (ix1)
				*ix1 = ICeil(gx1 * sx + shx);
			if (iy1)
				*iy1 = ICeil(-gy0 * sy + shy);
		}

		// ============================================================
		// SDF — Signed Distance Field Generation
		// ============================================================

		void NkMakeSDFFromBitmap(const nkft_uint8 *alpha, nkft_int32 w, nkft_int32 h, nkft_uint8 *sdfOut,
								 nkft_int32 spread) {
			if (!alpha || !sdfOut || w <= 0 || h <= 0 || spread <= 0)
				return;
			const nkft_float32 invSpread = 1.f / (nkft_float32)spread;
			const nkft_float32 threshold = 127.5f;

			for (nkft_int32 py = 0; py < h; ++py) {
				for (nkft_int32 px = 0; px < w; ++px) {
					nkft_float32 aCenter = (nkft_float32)alpha[py * w + px];
					bool inside = aCenter >= threshold;

					nkft_float32 minDistSq = (nkft_float32)(spread * spread + 1);

					nkft_int32 x0 = px - spread;
					nkft_int32 x1 = px + spread;
					nkft_int32 y0 = py - spread;
					nkft_int32 y1 = py + spread;
					if (x0 < 0)
						x0 = 0;
					if (x1 >= w)
						x1 = w - 1;
					if (y0 < 0)
						y0 = 0;
					if (y1 >= h)
						y1 = h - 1;

					for (nkft_int32 qy = y0; qy <= y1; ++qy) {
						for (nkft_int32 qx = x0; qx <= x1; ++qx) {
							nkft_float32 aq = (nkft_float32)alpha[qy * w + qx];
							bool qInside = aq >= threshold;
							if (qInside != inside) {
								nkft_float32 dx = (nkft_float32)(qx - px);
								nkft_float32 dy = (nkft_float32)(qy - py);
								nkft_float32 dsq = dx * dx + dy * dy;
								if (dsq < minDistSq)
									minDistSq = dsq;
							}
						}
					}

					nkft_float32 dist = FSqrt(minDistSq) * invSpread;
					if (dist > 1.f)
						dist = 1.f;
					nkft_float32 sdfVal = inside ? (0.5f + dist * 0.5f) : (0.5f - dist * 0.5f);
					sdfOut[py * w + px] = (nkft_uint8)(sdfVal * 255.f + 0.5f);
				}
			}
		}

		bool GetFontName(const NkFontFaceInfo *info, nkft_int32 nameID, char *outBuf, nkft_int32 outBufLen) {
			if (!info || !outBuf || outBufLen <= 0 || !info->name_)
				return false;

			const nkft_uint8 *d = info->data.data;
			nkft_uint32 base = info->name_;
			if (!info->data.IsValid(base + 6, 0))
				return false;

			nkft_uint32 count = NkReadU16(d + base + 2);
			nkft_uint32 strOff = base + NkReadU16(d + base + 4);

			for (nkft_uint32 i = 0; i < count && i < 64; ++i) {
				nkft_uint32 rec = base + 6 + i * 12;
				if (!info->data.IsValid(rec + 12, 0))
					break;

				nkft_uint32 pid = NkReadU16(d + rec);
				nkft_uint32 nid = NkReadU16(d + rec + 6);
				nkft_uint32 len = NkReadU16(d + rec + 8);
				nkft_uint32 ofs = NkReadU16(d + rec + 10);

				if ((nkft_int32)nid != nameID)
					continue;

				nkft_uint32 eid = NkReadU16(d + rec + 2);
				bool isWin = (pid == 3 && eid == 1);
				bool isMac = (pid == 1 && eid == 0);
				if (!isWin && !isMac)
					continue;

				nkft_uint32 sb = strOff + ofs;
				if (!info->data.IsValid(sb, len))
					continue;

				nkft_int32 out = 0;
				if (isWin) {
					for (nkft_uint32 k = 0; k + 1 < len && out < outBufLen - 1; k += 2) {
						nkft_uint16 ch = NkReadU16(d + sb + k);
						outBuf[out++] = (ch < 128) ? (char)ch : '?';
					}
				} else {
					for (nkft_uint32 k = 0; k < len && out < outBufLen - 1; ++k) {
						outBuf[out++] = (char)d[sb + k];
					}
				}
				outBuf[out] = '\0';
				return out > 0;
			}
			return false;
		}

	} // namespace nkfont
} // namespace nkentseu

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// ============================================================