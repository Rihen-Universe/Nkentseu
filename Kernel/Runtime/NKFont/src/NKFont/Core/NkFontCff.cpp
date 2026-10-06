// -----------------------------------------------------------------------------
// @File    Kernel/Runtime/NKFont/src/NKFont/Core/NkFontCff.cpp
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @Brief   CFF complet (Adobe TN #5176 et #5177). Voir NkFontCff.h.
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#include "NkFontCff.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace nkentseu {
	namespace nkfont {

		namespace {

			// ── Chaines standard (TN 5176, annexe A) : SID 0..390 ────────────────
			const char *const kStd[] = {
				".notdef", "space", "exclam", "quotedbl", "numbersign", "dollar", "percent", "ampersand",
				"quoteright", "parenleft", "parenright", "asterisk", "plus", "comma", "hyphen", "period", "slash",
				"zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "colon", "semicolon",
				"less", "equal", "greater", "question", "at", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K",
				"L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "bracketleft", "backslash",
				"bracketright", "asciicircum", "underscore", "quoteleft", "a", "b", "c", "d", "e", "f", "g", "h", "i",
				"j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z", "braceleft",
				"bar", "braceright", "asciitilde", "exclamdown", "cent", "sterling", "fraction", "yen", "florin",
				"section", "currency", "quotesingle", "quotedblleft", "guillemotleft", "guilsinglleft",
				"guilsinglright", "fi", "fl", "endash", "dagger", "daggerdbl", "periodcentered", "paragraph",
				"bullet", "quotesinglbase", "quotedblbase", "quotedblright", "guillemotright", "ellipsis",
				"perthousand", "questiondown", "grave", "acute", "circumflex", "tilde", "macron", "breve",
				"dotaccent", "dieresis", "ring", "cedilla", "hungarumlaut", "ogonek", "caron", "emdash", "AE",
				"ordfeminine", "Lslash", "Oslash", "OE", "ordmasculine", "ae", "dotlessi", "lslash", "oslash", "oe",
				"germandbls", "onesuperior", "logicalnot", "mu", "trademark", "Eth", "onehalf", "plusminus", "Thorn",
				"onequarter", "divide", "brokenbar", "degree", "thorn", "threequarters", "twosuperior", "registered",
				"minus", "eth", "multiply", "threesuperior", "copyright", "Aacute", "Acircumflex", "Adieresis",
				"Agrave", "Aring", "Atilde", "Ccedilla", "Eacute", "Ecircumflex", "Edieresis", "Egrave", "Iacute",
				"Icircumflex", "Idieresis", "Igrave", "Ntilde", "Oacute", "Ocircumflex", "Odieresis", "Ograve",
				"Otilde", "Scaron", "Uacute", "Ucircumflex", "Udieresis", "Ugrave", "Yacute", "Ydieresis", "Zcaron",
				"aacute", "acircumflex", "adieresis", "agrave", "aring", "atilde", "ccedilla", "eacute",
				"ecircumflex", "edieresis", "egrave", "iacute", "icircumflex", "idieresis", "igrave", "ntilde",
				"oacute", "ocircumflex", "odieresis", "ograve", "otilde", "scaron", "uacute", "ucircumflex",
				"udieresis", "ugrave", "yacute", "ydieresis", "zcaron", "exclamsmall", "Hungarumlautsmall",
				"dollaroldstyle", "dollarsuperior", "ampersandsmall", "Acutesmall", "parenleftsuperior",
				"parenrightsuperior", "twodotenleader", "onedotenleader", "zerooldstyle", "oneoldstyle",
				"twooldstyle", "threeoldstyle", "fouroldstyle", "fiveoldstyle", "sixoldstyle", "sevenoldstyle",
				"eightoldstyle", "nineoldstyle", "commasuperior", "threequartersemdash", "periodsuperior",
				"questionsmall", "asuperior", "bsuperior", "centsuperior", "dsuperior", "esuperior", "isuperior",
				"lsuperior", "msuperior", "nsuperior", "osuperior", "rsuperior", "ssuperior", "tsuperior", "ff",
				"ffi", "ffl", "parenleftinferior", "parenrightinferior", "Circumflexsmall", "hyphensuperior",
				"Gravesmall", "Asmall", "Bsmall", "Csmall", "Dsmall", "Esmall", "Fsmall", "Gsmall", "Hsmall",
				"Ismall", "Jsmall", "Ksmall", "Lsmall", "Msmall", "Nsmall", "Osmall", "Psmall", "Qsmall", "Rsmall",
				"Ssmall", "Tsmall", "Usmall", "Vsmall", "Wsmall", "Xsmall", "Ysmall", "Zsmall", "colonmonetary",
				"onefitted", "rupiah", "Tildesmall", "exclamdownsmall", "centoldstyle", "Lslashsmall",
				"Scaronsmall", "Zcaronsmall", "Dieresissmall", "Brevesmall", "Caronsmall", "Dotaccentsmall",
				"Macronsmall", "figuredash", "hypheninferior", "Ogoneksmall", "Ringsmall", "Cedillasmall",
				"questiondownsmall", "oneeighth", "threeeighths", "fiveeighths", "seveneighths", "onethird",
				"twothirds", "zerosuperior", "foursuperior", "fivesuperior", "sixsuperior", "sevensuperior",
				"eightsuperior", "ninesuperior", "zeroinferior", "oneinferior", "twoinferior", "threeinferior",
				"fourinferior", "fiveinferior", "sixinferior", "seveninferior", "eightinferior", "nineinferior",
				"centinferior", "dollarinferior", "periodinferior", "commainferior", "Agravesmall", "Aacutesmall",
				"Acircumflexsmall", "Atildesmall", "Adieresissmall", "Aringsmall", "AEsmall", "Ccedillasmall",
				"Egravesmall", "Eacutesmall", "Ecircumflexsmall", "Edieresissmall", "Igravesmall", "Iacutesmall",
				"Icircumflexsmall", "Idieresissmall", "Ethsmall", "Ntildesmall", "Ogravesmall", "Oacutesmall",
				"Ocircumflexsmall", "Otildesmall", "Odieresissmall", "OEsmall", "Oslashsmall", "Ugravesmall",
				"Uacutesmall", "Ucircumflexsmall", "Udieresissmall", "Yacutesmall", "Thornsmall", "Ydieresissmall",
				"001.000", "001.001", "001.002", "001.003", "Black", "Bold", "Book", "Light", "Medium", "Regular",
				"Roman", "Semibold"};
			constexpr nkft_uint32 kNbStd = static_cast<nkft_uint32>(sizeof(kStd) / sizeof(kStd[0]));
			static_assert(sizeof(kStd) / sizeof(kStd[0]) == 391, "391 chaines standard (TN 5176, annexe A)");

			// ── StandardEncoding : code -> SID (TN 5176, annexe B) ──────────────
			nkft_uint32 StdEncSid(nkft_uint32 code) {
				if (code >= 32 && code <= 126)
					return code - 31;
				static const nkft_uint16 haut[][2] = {
					{161, 96},	{162, 97},	{163, 98},	{164, 99},	{165, 100}, {166, 101}, {167, 102}, {168, 103},
					{169, 104}, {170, 105}, {171, 106}, {172, 107}, {173, 108}, {174, 109}, {175, 110}, {177, 111},
					{178, 112}, {179, 113}, {180, 114}, {182, 115}, {183, 116}, {184, 117}, {185, 118}, {186, 119},
					{187, 120}, {188, 121}, {189, 122}, {191, 123}, {193, 124}, {194, 125}, {195, 126}, {196, 127},
					{197, 128}, {198, 129}, {199, 130}, {200, 131}, {202, 132}, {203, 133}, {205, 134}, {206, 135},
					{207, 136}, {208, 137}, {225, 138}, {227, 139}, {232, 140}, {233, 141}, {234, 142}, {235, 143},
					{241, 144}, {245, 145}, {248, 146}, {249, 147}, {250, 148}, {251, 149}};
				for (const auto &e : haut)
					if (e[0] == code)
						return e[1];
				return 0;
			}

			// ── INDEX (TN 5176, §5) : offsets a base 1 ──────────────────────────
			struct Index {
					const nkft_uint8 *d = nullptr;
					nkft_size taille = 0;
					nkft_uint32 count = 0, offSize = 0, offArr = 0, base = 0, fin = 0;

					bool Lire(const NkFontDataSpan &s, nkft_uint32 at) {
						d = s.data;
						taille = s.size;
						count = 0;
						if (!at || !s.IsValid(at, 2))
							return false;
						count = NkReadU16(s.At(at));
						if (count == 0) {
							fin = at + 2;
							return true;
						}
						if (!s.IsValid(at + 2, 1))
							return false;
						offSize = s.data[at + 2];
						if (offSize < 1 || offSize > 4)
							return false;
						offArr = at + 3;
						const nkft_size nOff = static_cast<nkft_size>(count + 1) * offSize;
						if (!s.IsValid(offArr, nOff))
							return false;
						base = static_cast<nkft_uint32>(offArr + nOff - 1);
						const nkft_uint32 dernier = Off(count);
						fin = base + dernier;
						return dernier >= 1 && fin <= taille;
					}

					nkft_uint32 Off(nkft_uint32 i) const {
						const nkft_uint8 *p = d + offArr + static_cast<nkft_size>(i) * offSize;
						nkft_uint32 v = 0;
						for (nkft_uint32 k = 0; k < offSize; ++k)
							v = (v << 8) | p[k];
						return v;
					}

					bool Item(nkft_uint32 i, nkft_uint32 &debut, nkft_uint32 &len) const {
						if (i >= count)
							return false;
						const nkft_uint32 a = Off(i), b = Off(i + 1);
						if (a < 1 || b < a)
							return false;
						debut = base + a;
						len = b - a;
						return static_cast<nkft_size>(debut) + len <= taille;
					}
			};

			nkft_int32 Biais(nkft_uint32 n) {
				return n < 1240 ? 107 : (n < 33900 ? 1131 : 32768);
			}

			// ── DICT (TN 5176, §4) : operandes puis operateur ; 12 x -> 1200 + x ──
			template <typename F> void ParcourirDict(const nkft_uint8 *p, const nkft_uint8 *e, F &&f) {
				double pile[48];
				nkft_int32 n = 0;
				auto pousser = [&](double v) {
					if (n < 48)
						pile[n++] = v;
				};
				while (p < e) {
					const nkft_uint8 b = *p;
					if (b == 28) {
						if (e - p < 3)
							return;
						pousser(static_cast<nkft_int16>((p[1] << 8) | p[2]));
						p += 3;
					} else if (b == 29) {
						if (e - p < 5)
							return;
						const nkft_uint32 v = (static_cast<nkft_uint32>(p[1]) << 24) |
											  (static_cast<nkft_uint32>(p[2]) << 16) |
											  (static_cast<nkft_uint32>(p[3]) << 8) | p[4];
						pousser(static_cast<nkft_int32>(v));
						p += 5;
					} else if (b == 30) { // reel : quartets, 0xF termine
						char tampon[48];
						nkft_int32 k = 0;
						++p;
						bool finReel = false;
						while (p < e && !finReel) {
							const nkft_uint8 octet = *p++;
							for (nkft_int32 moitie = 0; moitie < 2 && !finReel; ++moitie) {
								const nkft_uint8 q = moitie == 0 ? (octet >> 4) : (octet & 0xF);
								if (k > 40)
									continue;
								if (q <= 9)
									tampon[k++] = static_cast<char>('0' + q);
								else if (q == 0xA)
									tampon[k++] = '.';
								else if (q == 0xB)
									tampon[k++] = 'E';
								else if (q == 0xC) {
									tampon[k++] = 'E';
									tampon[k++] = '-';
								} else if (q == 0xE)
									tampon[k++] = '-';
								else if (q == 0xF)
									finReel = true;
							}
						}
						tampon[k] = 0;
						pousser(atof(tampon));
					} else if (b >= 32 && b <= 246) {
						pousser(static_cast<double>(b) - 139.0);
						++p;
					} else if (b >= 247 && b <= 250) {
						if (e - p < 2)
							return;
						pousser((b - 247) * 256.0 + p[1] + 108.0);
						p += 2;
					} else if (b >= 251 && b <= 254) {
						if (e - p < 2)
							return;
						pousser(-(b - 251) * 256.0 - p[1] - 108.0);
						p += 2;
					} else if (b <= 21) {
						nkft_int32 op = b;
						++p;
						if (b == 12) {
							if (p >= e)
								return;
							op = 1200 + *p++;
						}
						f(op, pile, n);
						n = 0;
					} else
						++p; // reserve
				}
			}

			// ── DICT prive : subroutines locales et largeurs ────────────────────
			struct Prive {
					nkft_uint32 lsubrs = 0;
					double defW = 0.0, nomW = 0.0;
			};

			void LirePrive(const NkFontFaceInfo *info, nkft_uint32 off, nkft_uint32 len, Prive &pr) {
				if (!off || !len || !info->data.IsValid(off, len))
					return;
				nkft_uint32 subrs = 0;
				ParcourirDict(info->data.At(off), info->data.At(off) + len, [&](nkft_int32 op, const double *v, nkft_int32 n) {
					if (n < 1)
						return;
					if (op == 19)
						subrs = static_cast<nkft_uint32>(v[n - 1]);
					else if (op == 20)
						pr.defW = v[n - 1];
					else if (op == 21)
						pr.nomW = v[n - 1];
				});
				pr.lsubrs = subrs ? off + subrs : 0;
			}

			// FDSelect (formats 0 et 3) : le Font DICT d'un glyphe d'une police CID.
			nkft_uint32 FdDuGlyphe(const NkFontFaceInfo *info, NkGlyphId g) {
				const NkFontDataSpan &s = info->data;
				const nkft_uint32 at = info->cffFDSelect;
				if (!at || !s.IsValid(at, 1))
					return 0;
				const nkft_uint8 fmt = s.data[at];
				if (fmt == 0)
					return s.IsValid(at + 1 + g, 1) ? s.data[at + 1 + g] : 0;
				if (fmt == 3 && s.IsValid(at + 1, 2)) {
					const nkft_uint32 nR = NkReadU16(s.At(at + 1));
					nkft_uint32 p = at + 3;
					for (nkft_uint32 i = 0; i < nR; ++i, p += 3) {
						if (!s.IsValid(p, 5))
							return 0;
						const nkft_uint32 premier = NkReadU16(s.At(p)), suivant = NkReadU16(s.At(p + 3));
						if (g >= premier && g < suivant)
							return s.data[p + 2];
					}
				}
				return 0;
			}

			void PriveDuGlyphe(const NkFontFaceInfo *info, NkGlyphId g, Prive &pr) {
				if (!info->cffCid) {
					LirePrive(info, info->cffPrivate, info->cffPrivateLen, pr);
					return;
				}
				Index fdA;
				nkft_uint32 d = 0, l = 0;
				if (!fdA.Lire(info->data, info->cffFDArray) || !fdA.Item(FdDuGlyphe(info, g), d, l))
					return;
				nkft_uint32 pl = 0, po = 0;
				ParcourirDict(info->data.At(d), info->data.At(d) + l, [&](nkft_int32 op, const double *v, nkft_int32 n) {
					if (op == 18 && n >= 2) {
						pl = static_cast<nkft_uint32>(v[0]);
						po = static_cast<nkft_uint32>(v[1]);
					}
				});
				if (pl)
					LirePrive(info, info->cffBase + po, pl, pr);
			}

			// Charset : f(gid, sid_ou_cid) pour chaque glyphe ; arret si f rend vrai.
			template <typename F> void ParcourirCharset(const NkFontFaceInfo *info, F &&f) {
				const nkft_int32 ng = info->cffNumGlyphs;
				if (ng <= 0 || f(0u, 0u))
					return;
				const nkft_uint32 cs = info->cffCharset;
				if (cs == 0) { // ISOAdobe : gid i -> SID i
					for (nkft_int32 g = 1; g < ng && g <= 228; ++g)
						if (f(static_cast<nkft_uint32>(g), static_cast<nkft_uint32>(g)))
							return;
					return;
				}
				if (cs == 1 || cs == 2)
					return; // Expert / ExpertSubset : polices de titrage, absentes des documents
				const NkFontDataSpan &s = info->data;
				nkft_uint32 at = info->cffBase + cs;
				if (!s.IsValid(at, 1))
					return;
				const nkft_uint8 fmt = s.data[at++];
				nkft_uint32 g = 1;
				if (fmt == 0) {
					for (; g < static_cast<nkft_uint32>(ng); ++g, at += 2) {
						if (!s.IsValid(at, 2) || f(g, NkReadU16(s.At(at))))
							return;
					}
				} else if (fmt == 1 || fmt == 2) {
					const nkft_uint32 tl = fmt == 1 ? 3u : 4u;
					while (g < static_cast<nkft_uint32>(ng)) {
						if (!s.IsValid(at, tl))
							return;
						const nkft_uint32 premier = NkReadU16(s.At(at));
						const nkft_uint32 reste = fmt == 1 ? s.data[at + 2] : NkReadU16(s.At(at + 2));
						at += tl;
						for (nkft_uint32 k = 0; k <= reste && g < static_cast<nkft_uint32>(ng); ++k, ++g)
							if (f(g, premier + k))
								return;
					}
				}
			}

			NkGlyphId GlyphFromSid(const NkFontFaceInfo *info, nkft_uint32 sid) {
				NkGlyphId r = 0;
				if (!sid)
					return 0;
				ParcourirCharset(info, [&](nkft_uint32 g, nkft_uint32 v) {
					if (v != sid)
						return false;
					r = static_cast<NkGlyphId>(g);
					return true;
				});
				return r;
			}

			// Nom d'un SID : chaine standard, sinon INDEX des chaines.
			bool NomDuSid(const NkFontFaceInfo *info, nkft_uint32 sid, const char *&p, nkft_int32 &len) {
				if (sid < kNbStd) {
					p = kStd[sid];
					len = static_cast<nkft_int32>(strlen(p));
					return true;
				}
				Index st;
				nkft_uint32 d = 0, l = 0;
				if (!st.Lire(info->data, info->cffStrings) || !st.Item(sid - kNbStd, d, l))
					return false;
				p = reinterpret_cast<const char *>(info->data.At(d));
				len = static_cast<nkft_int32>(l);
				return true;
			}

			// ── Interpreteur Type 2 (TN 5177) ───────────────────────────────────
			struct Interp {
					const NkFontFaceInfo *info = nullptr;
					NkFontVertexBuffer *buf = nullptr;
					Index gsubrs, lsubrs;
					nkft_int32 gbias = 0, lbias = 0;
					double pile[48] = {};
					nkft_int32 n = 0;
					double x = 0.0, y = 0.0;   // point courant (unites de police)
					double ox = 0.0, oy = 0.0; // decalage (accent d'un seac)
					double sx = 0.0, sy = 0.0; // debut du contour ouvert
					bool ouvert = false;
					nkft_int32 nStems = 0;
					bool largeurVue = false;
					bool fini = false;
					double transitoire[32] = {};
					bool seac = false;
					double seacAdx = 0.0, seacAdy = 0.0;
					nkft_uint32 seacBase = 0, seacAccent = 0;

					static nkft_int16 Q(double v) {
						const double r = floor(v + 0.5);
						return static_cast<nkft_int16>(r < -32768.0 ? -32768.0 : (r > 32767.0 ? 32767.0 : r));
					}

					void Pousser(double v) {
						if (n < 48)
							pile[n++] = v;
					}

					void Emettre(nkft_uint8 type, double px, double py, double c1x = 0.0, double c1y = 0.0,
								 double c2x = 0.0, double c2y = 0.0) {
						NkFontVertex v{};
						v.type = type;
						v.x = Q(px + ox);
						v.y = Q(py + oy);
						v.cx = Q(c1x + ox);
						v.cy = Q(c1y + oy);
						v.cx1 = Q(c2x + ox);
						v.cy1 = Q(c2y + oy);
						buf->Push(v);
					}

					// Le contour se ferme ; le point courant, lui, reste (rmoveto est relatif a lui).
					void Fermer() {
						if (ouvert && (x != sx || y != sy))
							Emettre(NK_FONT_VERTEX_LINE, sx, sy);
						ouvert = false;
					}

					void MoveTo(double dx, double dy) {
						Fermer();
						x += dx;
						y += dy;
						Emettre(NK_FONT_VERTEX_MOVE, x, y);
						sx = x;
						sy = y;
						ouvert = true;
					}

					void Ouvrir() {
						if (!ouvert)
							MoveTo(0.0, 0.0);
					}

					void LineTo(double dx, double dy) {
						Ouvrir();
						x += dx;
						y += dy;
						Emettre(NK_FONT_VERTEX_LINE, x, y);
					}

					void CurveTo(double dx1, double dy1, double dx2, double dy2, double dx3, double dy3) {
						Ouvrir();
						const double c1x = x + dx1, c1y = y + dy1;
						const double c2x = c1x + dx2, c2y = c1y + dy2;
						x = c2x + dx3;
						y = c2y + dy3;
						Emettre(NK_FONT_VERTEX_CUBIC, x, y, c1x, c1y, c2x, c2y);
					}

					// La largeur est l'operande EN TROP du premier operateur qui vide la pile.
					void Largeur(bool enTrop) {
						if (largeurVue)
							return;
						largeurVue = true;
						if (enTrop && n > 0) {
							for (nkft_int32 i = 1; i < n; ++i)
								pile[i - 1] = pile[i];
							--n;
						}
					}

					bool Executer(const nkft_uint8 *p, nkft_uint32 len, nkft_int32 profondeur) {
						const nkft_uint8 *e = p + len;
						while (p < e && !fini) {
							const nkft_uint8 b = *p++;
							if (b == 28) {
								if (e - p < 2)
									return false;
								Pousser(static_cast<nkft_int16>((p[0] << 8) | p[1]));
								p += 2;
								continue;
							}
							if (b >= 32 && b <= 246) {
								Pousser(static_cast<double>(b) - 139.0);
								continue;
							}
							if (b >= 247 && b <= 250) {
								if (p >= e)
									return false;
								Pousser((b - 247) * 256.0 + *p++ + 108.0);
								continue;
							}
							if (b >= 251 && b <= 254) {
								if (p >= e)
									return false;
								Pousser(-(b - 251) * 256.0 - *p++ - 108.0);
								continue;
							}
							if (b == 255) { // 16.16 fixe
								if (e - p < 4)
									return false;
								const nkft_uint32 v = (static_cast<nkft_uint32>(p[0]) << 24) |
													  (static_cast<nkft_uint32>(p[1]) << 16) |
													  (static_cast<nkft_uint32>(p[2]) << 8) | p[3];
								Pousser(static_cast<nkft_int32>(v) / 65536.0);
								p += 4;
								continue;
							}
							switch (b) {
								case 1:	 // hstem
								case 3:	 // vstem
								case 18: // hstemhm
								case 23: // vstemhm
									Largeur((n & 1) != 0);
									nStems += n / 2;
									n = 0;
									break;
								case 19: // hintmask
								case 20: // cntrmask
								{
									Largeur((n & 1) != 0);
									nStems += n / 2; // vstem implicite
									n = 0;
									const nkft_int32 octets = (nStems + 7) / 8;
									if (e - p < octets)
										return false;
									p += octets;
									break;
								}
								case 21: // rmoveto
									Largeur(n > 2);
									if (n >= 2)
										MoveTo(pile[n - 2], pile[n - 1]);
									n = 0;
									break;
								case 22: // hmoveto
									Largeur(n > 1);
									if (n >= 1)
										MoveTo(pile[n - 1], 0.0);
									n = 0;
									break;
								case 4: // vmoveto
									Largeur(n > 1);
									if (n >= 1)
										MoveTo(0.0, pile[n - 1]);
									n = 0;
									break;
								case 5: // rlineto
									for (nkft_int32 i = 0; i + 1 < n; i += 2)
										LineTo(pile[i], pile[i + 1]);
									n = 0;
									break;
								case 6: // hlineto
								case 7: // vlineto
								{
									bool h = b == 6;
									for (nkft_int32 i = 0; i < n; ++i, h = !h) {
										if (h)
											LineTo(pile[i], 0.0);
										else
											LineTo(0.0, pile[i]);
									}
									n = 0;
									break;
								}
								case 8: // rrcurveto
									for (nkft_int32 i = 0; i + 5 < n; i += 6)
										CurveTo(pile[i], pile[i + 1], pile[i + 2], pile[i + 3], pile[i + 4], pile[i + 5]);
									n = 0;
									break;
								case 24: // rcurveline
								{
									nkft_int32 i = 0;
									for (; i + 5 < n - 2; i += 6)
										CurveTo(pile[i], pile[i + 1], pile[i + 2], pile[i + 3], pile[i + 4], pile[i + 5]);
									if (i + 1 < n)
										LineTo(pile[i], pile[i + 1]);
									n = 0;
									break;
								}
								case 25: // rlinecurve
								{
									nkft_int32 i = 0;
									for (; i + 1 < n - 6; i += 2)
										LineTo(pile[i], pile[i + 1]);
									if (i + 5 < n)
										CurveTo(pile[i], pile[i + 1], pile[i + 2], pile[i + 3], pile[i + 4], pile[i + 5]);
									n = 0;
									break;
								}
								case 26: // vvcurveto : dx1? {dya dxb dyb dyc}+
								{
									nkft_int32 i = 0;
									double dx1 = 0.0;
									if (n & 1)
										dx1 = pile[i++];
									for (; i + 3 < n; i += 4, dx1 = 0.0)
										CurveTo(dx1, pile[i], pile[i + 1], pile[i + 2], 0.0, pile[i + 3]);
									n = 0;
									break;
								}
								case 27: // hhcurveto : dy1? {dxa dxb dyb dxc}+
								{
									nkft_int32 i = 0;
									double dy1 = 0.0;
									if (n & 1)
										dy1 = pile[i++];
									for (; i + 3 < n; i += 4, dy1 = 0.0)
										CurveTo(pile[i], dy1, pile[i + 1], pile[i + 2], pile[i + 3], 0.0);
									n = 0;
									break;
								}
								case 30: // vhcurveto
								case 31: // hvcurveto
								{
									bool h = b == 31;
									nkft_int32 i = 0;
									while (i + 3 < n) {
										const bool dernier = (n - i) == 5;
										const double f = dernier ? pile[i + 4] : 0.0;
										if (h)
											CurveTo(pile[i], 0.0, pile[i + 1], pile[i + 2], f, pile[i + 3]);
										else
											CurveTo(0.0, pile[i], pile[i + 1], pile[i + 2], pile[i + 3], f);
										i += dernier ? 5 : 4;
										h = !h;
									}
									n = 0;
									break;
								}
								case 10: // callsubr
								case 29: // callgsubr
								{
									if (n < 1 || profondeur >= 10)
										return false;
									const nkft_int32 idx = static_cast<nkft_int32>(pile[--n]) + (b == 10 ? lbias : gbias);
									const Index &ix = b == 10 ? lsubrs : gsubrs;
									nkft_uint32 d = 0, l = 0;
									if (idx < 0 || !ix.Item(static_cast<nkft_uint32>(idx), d, l))
										return false;
									if (!Executer(info->data.At(d), l, profondeur + 1))
										return false;
									break;
								}
								case 11: // return
									return true;
								case 14: // endchar (et « seac » a 4 operandes)
									Largeur(n == 1 || n == 5);
									if (n >= 4) {
										seac = true;
										seacAdx = pile[0];
										seacAdy = pile[1];
										seacBase = static_cast<nkft_uint32>(pile[2]);
										seacAccent = static_cast<nkft_uint32>(pile[3]);
									}
									Fermer();
									fini = true;
									n = 0;
									return true;
								case 12: {
									if (p >= e)
										return false;
									const nkft_uint8 c = *p++;
									Escape(c);
									break;
								}
								default:
									n = 0; // reserve
									break;
							}
						}
						return true;
					}

					void Escape(nkft_uint8 c) {
						auto deux = [&](double &a, double &bb) -> bool {
							if (n < 2)
								return false;
							a = pile[n - 2];
							bb = pile[n - 1];
							n -= 2;
							return true;
						};
						double a = 0.0, bb = 0.0;
						switch (c) {
							case 35: // flex
								if (n >= 13) {
									CurveTo(pile[0], pile[1], pile[2], pile[3], pile[4], pile[5]);
									CurveTo(pile[6], pile[7], pile[8], pile[9], pile[10], pile[11]);
								}
								n = 0;
								break;
							case 34: // hflex
								if (n >= 7) {
									CurveTo(pile[0], 0.0, pile[1], pile[2], pile[3], 0.0);
									CurveTo(pile[4], 0.0, pile[5], -pile[2], pile[6], 0.0);
								}
								n = 0;
								break;
							case 36: // hflex1
								if (n >= 9) {
									CurveTo(pile[0], pile[1], pile[2], pile[3], pile[4], 0.0);
									CurveTo(pile[5], 0.0, pile[6], pile[7], pile[8], -(pile[1] + pile[3] + pile[7]));
								}
								n = 0;
								break;
							case 37: // flex1
								if (n >= 11) {
									double dx = 0.0, dy = 0.0;
									for (nkft_int32 k = 0; k < 10; k += 2) {
										dx += pile[k];
										dy += pile[k + 1];
									}
									const bool horiz = fabs(dx) > fabs(dy);
									CurveTo(pile[0], pile[1], pile[2], pile[3], pile[4], pile[5]);
									CurveTo(pile[6], pile[7], pile[8], pile[9], horiz ? pile[10] : -dx, horiz ? -dy : pile[10]);
								}
								n = 0;
								break;
							case 3: // and
								if (deux(a, bb))
									Pousser((a != 0.0 && bb != 0.0) ? 1.0 : 0.0);
								break;
							case 4: // or
								if (deux(a, bb))
									Pousser((a != 0.0 || bb != 0.0) ? 1.0 : 0.0);
								break;
							case 5: // not
								if (n >= 1)
									pile[n - 1] = pile[n - 1] == 0.0 ? 1.0 : 0.0;
								break;
							case 9: // abs
								if (n >= 1)
									pile[n - 1] = fabs(pile[n - 1]);
								break;
							case 10: // add
								if (deux(a, bb))
									Pousser(a + bb);
								break;
							case 11: // sub
								if (deux(a, bb))
									Pousser(a - bb);
								break;
							case 12: // div
								if (deux(a, bb))
									Pousser(bb != 0.0 ? a / bb : 0.0);
								break;
							case 14: // neg
								if (n >= 1)
									pile[n - 1] = -pile[n - 1];
								break;
							case 15: // eq
								if (deux(a, bb))
									Pousser(a == bb ? 1.0 : 0.0);
								break;
							case 18: // drop
								if (n >= 1)
									--n;
								break;
							case 20: // put : val i
								if (deux(a, bb) && bb >= 0.0 && bb < 32.0)
									transitoire[static_cast<nkft_int32>(bb)] = a;
								break;
							case 21: // get : i
								if (n >= 1) {
									const nkft_int32 i = static_cast<nkft_int32>(pile[n - 1]);
									pile[n - 1] = (i >= 0 && i < 32) ? transitoire[i] : 0.0;
								}
								break;
							case 22: // ifelse : s1 s2 v1 v2
								if (n >= 4) {
									const double r = pile[n - 2] <= pile[n - 1] ? pile[n - 4] : pile[n - 3];
									n -= 4;
									Pousser(r);
								}
								break;
							case 23: // random : deterministe (un rendu doit etre reproductible)
								Pousser(0.5);
								break;
							case 24: // mul
								if (deux(a, bb))
									Pousser(a * bb);
								break;
							case 26: // sqrt
								if (n >= 1)
									pile[n - 1] = pile[n - 1] > 0.0 ? sqrt(pile[n - 1]) : 0.0;
								break;
							case 27: // dup
								if (n >= 1)
									Pousser(pile[n - 1]);
								break;
							case 28: // exch
								if (n >= 2) {
									const double t = pile[n - 1];
									pile[n - 1] = pile[n - 2];
									pile[n - 2] = t;
								}
								break;
							case 29: // index
								if (n >= 1) {
									nkft_int32 i = static_cast<nkft_int32>(pile[n - 1]);
									if (i < 0)
										i = 0;
									pile[n - 1] = (i < n - 1) ? pile[n - 2 - i] : 0.0;
								}
								break;
							case 30: // roll : N J
								if (deux(a, bb)) {
									const nkft_int32 cnt = static_cast<nkft_int32>(a);
									if (cnt > 0 && cnt <= n) {
										nkft_int32 j = static_cast<nkft_int32>(bb) % cnt;
										if (j < 0)
											j += cnt;
										double tmp[48];
										const nkft_int32 debut = n - cnt;
										for (nkft_int32 k = 0; k < cnt; ++k)
											tmp[(k + j) % cnt] = pile[debut + k];
										for (nkft_int32 k = 0; k < cnt; ++k)
											pile[debut + k] = tmp[k];
									}
								}
								break;
							default: // dotsection et reserves
								n = 0;
								break;
						}
					}
			};

			bool Dessiner(const NkFontFaceInfo *info, NkGlyphId g, NkFontVertexBuffer *buf, double ox, double oy,
						  bool seacPermis) {
				Index cs;
				nkft_uint32 d = 0, l = 0;
				if (!cs.Lire(info->data, info->cffCharStrings) || !cs.Item(g, d, l))
					return false;
				Prive pr;
				PriveDuGlyphe(info, g, pr);
				Interp it;
				it.info = info;
				it.buf = buf;
				it.ox = ox;
				it.oy = oy;
				if (info->cffGSubrs && it.gsubrs.Lire(info->data, info->cffGSubrs))
					it.gbias = Biais(it.gsubrs.count);
				if (pr.lsubrs && it.lsubrs.Lire(info->data, pr.lsubrs))
					it.lbias = Biais(it.lsubrs.count);
				it.Executer(info->data.At(d), l, 0); // un charstring tronque garde ce qu'il a trace
				it.Fermer();
				// « seac » : glyphe de base a l'origine, accent decale de (adx, ady) ; les
				// deux designes par leur code dans StandardEncoding (TN 5177, annexe C).
				if (it.seac && seacPermis && !info->cffCid) {
					const NkGlyphId gb = GlyphFromSid(info, StdEncSid(it.seacBase));
					const NkGlyphId ga = GlyphFromSid(info, StdEncSid(it.seacAccent));
					if (gb)
						Dessiner(info, gb, buf, ox, oy, false);
					if (ga)
						Dessiner(info, ga, buf, ox + it.seacAdx, oy + it.seacAdy, false);
				}
				return true;
			}

		} // namespace

		bool NkCffInit(NkFontFaceInfo *info, nkft_uint32 cffOffset, nkft_uint32 cffLen) {
			if (!info)
				return false;
			const NkFontDataSpan &s = info->data;
			const nkft_uint32 b = cffOffset;
			if (!s.IsValid(b, 4) || s.data[b] != 1) // CFF version 1 (CFF2 non gere)
				return false;
			const nkft_uint32 hdrSize = s.data[b + 2];
			Index noms, top, chaines, gsubrs;
			if (!noms.Lire(s, b + hdrSize) || !top.Lire(s, noms.fin) || top.count < 1 || !chaines.Lire(s, top.fin) ||
				!gsubrs.Lire(s, chaines.fin))
				return false;
			nkft_uint32 td = 0, tl = 0;
			if (!top.Item(0, td, tl))
				return false;
			nkft_uint32 cs = 0, privLen = 0, privOff = 0, charset = 0, enc = 0, fdArr = 0, fdSel = 0;
			nkft_int32 csType = 2;
			bool cid = false;
			double mat[6] = {0.001, 0.0, 0.0, 0.001, 0.0, 0.0};
			ParcourirDict(s.At(td), s.At(td) + tl, [&](nkft_int32 op, const double *v, nkft_int32 n) {
				switch (op) {
					case 17:
						if (n >= 1)
							cs = static_cast<nkft_uint32>(v[0]);
						break;
					case 18:
						if (n >= 2) {
							privLen = static_cast<nkft_uint32>(v[0]);
							privOff = static_cast<nkft_uint32>(v[1]);
						}
						break;
					case 15:
						if (n >= 1)
							charset = static_cast<nkft_uint32>(v[0]);
						break;
					case 16:
						if (n >= 1)
							enc = static_cast<nkft_uint32>(v[0]);
						break;
					case 1206:
						if (n >= 1)
							csType = static_cast<nkft_int32>(v[0]);
						break;
					case 1207:
						if (n >= 6)
							for (nkft_int32 i = 0; i < 6; ++i)
								mat[i] = v[i];
						break;
					case 1230:
						cid = true;
						break;
					case 1236:
						if (n >= 1)
							fdArr = static_cast<nkft_uint32>(v[0]);
						break;
					case 1237:
						if (n >= 1)
							fdSel = static_cast<nkft_uint32>(v[0]);
						break;
					default: break;
				}
			});
			if (!cs || csType != 2)
				return false;
			Index charStrings;
			if (!charStrings.Lire(s, b + cs) || charStrings.count == 0)
				return false;
			info->isCFF = true;
			info->cffBase = b;
			info->cffLen = cffLen;
			info->cffStrings = top.fin;
			info->cffGSubrs = chaines.fin;
			info->cffCharStrings = b + cs;
			info->cffNumGlyphs = static_cast<nkft_int32>(charStrings.count);
			info->cffPrivate = privLen ? b + privOff : 0;
			info->cffPrivateLen = privLen;
			info->cffCharset = charset;
			info->cffEncoding = enc;
			info->cffCid = cid;
			info->cffFDArray = (cid && fdArr) ? b + fdArr : 0;
			info->cffFDSelect = (cid && fdSel) ? b + fdSel : 0;
			for (nkft_int32 i = 0; i < 6; ++i)
				info->cffMatrix[i] = static_cast<nkft_float32>(mat[i]);
			if (info->numGlyphs <= 0)
				info->numGlyphs = info->cffNumGlyphs;
			return true;
		}

		bool NkCffGlyphShape(const NkFontFaceInfo *info, NkGlyphId glyph, NkFontVertexBuffer *buf) {
			if (!info || !buf || !info->isCFF || !info->cffCharStrings)
				return false;
			buf->Clear();
			Dessiner(info, glyph, buf, 0.0, 0.0, true);
			return buf->count > 0;
		}

		nkft_int32 NkCffGlyphCount(const NkFontFaceInfo *info) {
			return (info && info->isCFF) ? info->cffNumGlyphs : 0;
		}

		bool NkCffIsCID(const NkFontFaceInfo *info) {
			return info && info->isCFF && info->cffCid;
		}

		NkGlyphId NkCffGlyphFromCID(const NkFontFaceInfo *info, nkft_uint32 cid) {
			if (!info || !info->isCFF)
				return 0;
			if (!info->cffCid) // police nommee dans une CIDFont : le CID est l'indice
				return cid < static_cast<nkft_uint32>(info->cffNumGlyphs) ? static_cast<NkGlyphId>(cid) : 0;
			if (cid == 0)
				return 0;
			NkGlyphId r = 0;
			ParcourirCharset(info, [&](nkft_uint32 g, nkft_uint32 v) {
				if (v != cid)
					return false;
				r = static_cast<NkGlyphId>(g);
				return true;
			});
			return r;
		}

		NkGlyphId NkCffGlyphFromName(const NkFontFaceInfo *info, const char *name, nkft_int32 len) {
			if (!info || !info->isCFF || info->cffCid || !name || len <= 0)
				return 0;
			NkGlyphId r = 0;
			ParcourirCharset(info, [&](nkft_uint32 g, nkft_uint32 sid) {
				const char *p = nullptr;
				nkft_int32 l = 0;
				if (g == 0 || !NomDuSid(info, sid, p, l) || l != len || memcmp(p, name, static_cast<size_t>(len)) != 0)
					return false;
				r = static_cast<NkGlyphId>(g);
				return true;
			});
			return r;
		}

		nkft_int32 NkCffGlyphName(const NkFontFaceInfo *info, NkGlyphId glyph, char *out, nkft_int32 cap) {
			if (!info || !info->isCFF || info->cffCid || !out || cap <= 1)
				return 0;
			nkft_int32 longueur = 0;
			ParcourirCharset(info, [&](nkft_uint32 g, nkft_uint32 sid) {
				if (g != glyph)
					return false;
				const char *p = nullptr;
				nkft_int32 l = 0;
				if (NomDuSid(info, sid, p, l)) {
					longueur = l < cap - 1 ? l : cap - 1;
					memcpy(out, p, static_cast<size_t>(longueur));
				}
				return true;
			});
			out[longueur] = 0;
			return longueur;
		}

		NkGlyphId NkCffGlyphFromCode(const NkFontFaceInfo *info, nkft_uint32 code) {
			if (!info || !info->isCFF || info->cffCid || code > 255)
				return 0;
			const nkft_uint32 enc = info->cffEncoding;
			if (enc == 0) // StandardEncoding
				return GlyphFromSid(info, StdEncSid(code));
			if (enc == 1) // ExpertEncoding : polices de titrage
				return 0;
			const NkFontDataSpan &s = info->data;
			nkft_uint32 at = info->cffBase + enc;
			if (!s.IsValid(at, 2))
				return 0;
			const nkft_uint8 fmt = s.data[at] & 0x7F;
			const bool supplements = (s.data[at] & 0x80) != 0;
			++at;
			if (fmt == 0) {
				const nkft_uint32 nCodes = s.data[at++];
				if (!s.IsValid(at, nCodes))
					return 0;
				for (nkft_uint32 i = 0; i < nCodes; ++i)
					if (s.data[at + i] == code)
						return static_cast<NkGlyphId>(i + 1);
				at += nCodes;
			} else if (fmt == 1) {
				const nkft_uint32 nRanges = s.data[at++];
				if (!s.IsValid(at, nRanges * 2))
					return 0;
				nkft_uint32 g = 1;
				for (nkft_uint32 r = 0; r < nRanges; ++r, at += 2) {
					const nkft_uint32 premier = s.data[at], reste = s.data[at + 1];
					if (code >= premier && code <= premier + reste)
						return static_cast<NkGlyphId>(g + (code - premier));
					g += reste + 1;
				}
			} else
				return 0;
			if (supplements && s.IsValid(at, 1)) {
				const nkft_uint32 nSups = s.data[at++];
				for (nkft_uint32 i = 0; i < nSups && s.IsValid(at, 3); ++i, at += 3)
					if (s.data[at] == code)
						return GlyphFromSid(info, NkReadU16(s.At(at + 1)));
			}
			return 0;
		}

	} // namespace nkfont
} // namespace nkentseu
