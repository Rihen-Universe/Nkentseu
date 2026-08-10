//
// NkPdfFont.cpp — voir NkPdfFont.h.
//
#include "NKMedia/Pdf/NkPdfFont.h"

namespace nkentseu {
	namespace media {
		namespace pdf {

			// ============================================================
			// Chargement
			// ============================================================

			void NkPdfFont::Unload() {
				if (mHasFace) {
					nkfont::NkFreeFontFace(&mFace);
					mHasFace = false;
				}
				mProgram.Clear();
				mWidths.Clear();
				mCidCodes.Clear();
				mCidWidths.Clear();
				mUniCodes.Clear();
				mUniText.Clear();
			}

			bool NkPdfFont::Load(const NkPdfDoc &doc, const NkPdfVal &fontDict) {
				Unload();
				if (!fontDict.IsDictLike())
					return false;

				{
					const NkPdfVal bf = doc.DictGet(fontDict, "BaseFont");
					int32 n = 0;
					const char *s = doc.Text(bf, &n);
					for (int32 i = 0; i < n; ++i)
						mBaseFont += s[i];
				}

				// La table /ToUnicode vit sur le dictionnaire de TETE, meme pour un
				// Type0 dont le programme de police est porte par le descendant.
				ParseToUnicode(doc, fontDict);

				const NkPdfVal subtype = doc.DictGet(fontDict, "Subtype");
				NkPdfVal descFont = fontDict; // pour un Type0, le vrai porteur est le descendant

				if (doc.NameIs(subtype, "Type0")) {
					mTwoByte = true;
					// /Encoding Identity-H (ou -V) : le code du flux EST le CID. Les
					// CMap nommees autres qu'Identity ne sont pas gerees — elles sont
					// rares et demandent d'embarquer les tables d'Adobe.
					const NkPdfVal enc = doc.DictGet(fontDict, "Encoding");
					mIdentityCid = doc.NameIs(enc, "Identity-H") || doc.NameIs(enc, "Identity-V");
					const NkPdfVal df = doc.DictGet(fontDict, "DescendantFonts");
					if (df.kind == NK_PDF_ARRAY && df.b > 0)
						descFont = doc.ArrayAt(df, 0);
				}

				// ── Largeurs ──
				if (mTwoByte) {
					mDefaultWidth = doc.Num(doc.DictGet(descFont, "DW"), 1000.0);
					// /W : suite de « code [w1 w2 ...] » ou « premier dernier w ».
					const NkPdfVal W = doc.DictGet(descFont, "W");
					if (W.kind == NK_PDF_ARRAY) {
						int32 i = 0;
						while (i < W.b) {
							const NkPdfVal a = doc.ArrayAt(W, i);
							if (!a.IsNum())
								break;
							const NkPdfVal b = doc.ArrayAt(W, i + 1);
							if (b.kind == NK_PDF_ARRAY) { // code [w...]
								const uint32 base = static_cast<uint32>(a.num);
								for (int32 k = 0; k < b.b; ++k) {
									mCidCodes.PushBack(base + static_cast<uint32>(k));
									mCidWidths.PushBack(doc.Num(doc.ArrayAt(b, k), mDefaultWidth));
								}
								i += 2;
							} else if (b.IsNum()) { // premier dernier largeur
								const NkPdfVal c = doc.ArrayAt(W, i + 2);
								const uint32 first = static_cast<uint32>(a.num);
								const uint32 last = static_cast<uint32>(b.num);
								const double w = doc.Num(c, mDefaultWidth);
								// Borne : un intervalle aberrant ferait exploser la memoire.
								const uint32 lim = (last > first + 65535u) ? first + 65535u : last;
								for (uint32 cc = first; cc <= lim; ++cc) {
									mCidCodes.PushBack(cc);
									mCidWidths.PushBack(w);
								}
								i += 3;
							} else
								break;
						}
					}
				} else {
					mFirstChar = static_cast<int32>(doc.Num(doc.DictGet(fontDict, "FirstChar"), 0));
					const NkPdfVal w = doc.DictGet(fontDict, "Widths");
					if (w.kind == NK_PDF_ARRAY)
						for (int32 i = 0; i < w.b; ++i)
							mWidths.PushBack(doc.Num(doc.ArrayAt(w, i), 0.0));
					const NkPdfVal fd = doc.DictGet(fontDict, "FontDescriptor");
					mDefaultWidth = doc.Num(doc.DictGet(fd, "MissingWidth"), 500.0);
				}

				// ── Programme de police ──
				const NkPdfVal fd = doc.DictGet(descFont, "FontDescriptor");
				NkPdfVal prog = doc.DictGet(fd, "FontFile2"); // TrueType
				if (prog.kind != NK_PDF_STREAM)
					prog = doc.DictGet(fd, "FontFile3"); // CFF / OpenType
				// /FontFile (Type 1 brut) volontairement ignore : 2,1 % du corpus, et
				// il faudrait un interpreteur de charstrings Type 1 complet.
				//
				// ⚠️ NE PAS ECHOUER ICI. Une police dont on ne sait pas DESSINER les
				// glyphes reste parfaitement utilisable pour LIRE : /ToUnicode, les
				// largeurs et l'encodage sont deja lus plus haut, et ce sont eux — et
				// non les contours — qui donnent le texte. Echouer jetait tout, et le
				// texte avec.
				//
				// Ce que ca coutait, mesure sur un cours produit par LaTeX : 64 pages,
				// 2 157 102 octets de contenu decode, 147 590 operations executees,
				// 10 104 ordres de texte... et ZERO caractere extrait, parce que les
				// polices Type 1 faisaient echouer leur chargement. Le rendu, lui, ne
				// change pas : `AppendGlyph` se garde deja sur `mHasFace`, donc rien
				// n'est dessine de ce qui ne peut pas l'etre.
				if (prog.kind != NK_PDF_STREAM || !doc.DecodeStream(prog, mProgram) || mProgram.Empty() ||
					!nkfont::NkInitFontFace(&mFace, mProgram.Data(), mProgram.Size(), 0)) {
					mHasFace = false;
					mUnitsPerEmInv = 1.0 / 1000.0; // convention PDF pour les largeurs
					return true;				   // lisible, mais non dessinable
				}
				mHasFace = true;

				// Echelle du programme : nkfont::NkScaleForEmToPixels(1) donne le facteur
				// « unites de police -> em », ce qu'il nous faut pour normaliser.
				const nkft_float32 s = nkfont::NkScaleForEmToPixels(&mFace, 1.0f);
				mUnitsPerEmInv = (s > 0.f) ? static_cast<double>(s) : (1.0 / 1000.0);
				return true;
			}

			// ============================================================
			// Codes, largeurs, glyphes
			// ============================================================

			int32 NkPdfFont::NextCode(const uint8 *s, int32 len, int32 i, uint32 *codeOut) const {
				if (i >= len) {
					if (codeOut)
						*codeOut = 0;
					return 0;
				}
				if (mTwoByte) {
					// Gros-boutiste, et un octet isole en fin de chaine est tolere
					// plutot que de perdre le dernier caractere.
					const uint32 hi = s[i];
					const uint32 lo = (i + 1 < len) ? s[i + 1] : 0u;
					if (codeOut)
						*codeOut = (hi << 8) | lo;
					return (i + 1 < len) ? 2 : 1;
				}
				if (codeOut)
					*codeOut = s[i];
				return 1;
			}

			double NkPdfFont::Advance(uint32 code) const {
				if (mTwoByte) {
					for (usize i = 0; i < mCidCodes.Size(); ++i)
						if (mCidCodes[i] == code)
							return mCidWidths[i];
					return mDefaultWidth;
				}
				const int32 idx = static_cast<int32>(code) - mFirstChar;
				if (idx >= 0 && static_cast<usize>(idx) < mWidths.Size()) {
					const double w = mWidths[static_cast<usize>(idx)];
					if (w > 0.0)
						return w;
					// Une largeur nulle declaree est legitime (glyphe sans avance) :
					// on la respecte plutot que de la remplacer par le defaut.
					return w;
				}
				return mDefaultWidth;
			}

			NkGlyphId NkPdfFont::GlyphOf(uint32 code) const {
				if (!mHasFace)
					return 0;
				if (mTwoByte) {
					// Identity : le CID est directement l'identifiant de glyphe.
					// (Un /CIDToGIDMap en flux n'est pas gere ; il est rare avec des
					// polices embarquees, qui sont deja indexees par CID.)
					if (mIdentityCid)
						return static_cast<NkGlyphId>(code);
					return static_cast<NkGlyphId>(code);
				}
				// Police simple : on tente la table de caracteres avec le code tel
				// quel. Si elle ne donne rien, on prend le code COMME identifiant de
				// glyphe — c'est le comportement des polices a encodage sur mesure,
				// tres frequentes dans les PDF generes.
				const NkGlyphId g = nkfont::NkFindGlyphIndex(&mFace, static_cast<NkFontCodepoint>(code));
				if (g != 0)
					return g;
				return static_cast<NkGlyphId>(code);
			}

			bool NkPdfFont::AppendGlyph(uint32 code, double tx, double ty, double scale, double shearX,
										double vScale, NkPdfPath &out) const {
				if (!mHasFace)
					return false;
				const NkGlyphId g = GlyphOf(code);
				nkfont::NkFontVertexBuffer buf;
				if (!nkfont::NkGetGlyphShape(&mFace, g, &buf) || buf.count == 0)
					return false;

				// Unites de police -> em -> taille demandee. L'axe Y du PDF monte,
				// celui de l'image descend : l'appelant fournit `vScale` negatif pour
				// exprimer ce retournement, ce qui evite de le cabler ici.
				const double k = mUnitsPerEmInv * scale;
				auto X = [&](double px, double py) { return tx + (px * k) + (py * k * shearX); };
				auto Y = [&](double px, double py) {
					(void)px;
					return ty + (py * k * vScale);
				};

				double cx = 0.0, cy = 0.0; // point courant, en unites de police
				for (nkft_uint32 i = 0; i < buf.count; ++i) {
					const nkfont::NkFontVertex &v = buf.verts[i];
					const double vx = static_cast<double>(v.x), vy = static_cast<double>(v.y);
					switch (v.type) {
						case nkfont::NK_FONT_VERTEX_MOVE:
							out.MoveTo(X(vx, vy), Y(vx, vy));
							break;
						case nkfont::NK_FONT_VERTEX_LINE:
							out.LineTo(X(vx, vy), Y(vx, vy));
							break;
						case nkfont::NK_FONT_VERTEX_CURVE: { // quadratique (TrueType)
							const double qx = static_cast<double>(v.cx), qy = static_cast<double>(v.cy);
							out.QuadTo(X(qx, qy), Y(qx, qy), X(vx, vy), Y(vx, vy));
							break;
						}
						case nkfont::NK_FONT_VERTEX_CUBIC: { // cubique (CFF)
							const double a1x = static_cast<double>(v.cx), a1y = static_cast<double>(v.cy);
							const double a2x = static_cast<double>(v.cx1), a2y = static_cast<double>(v.cy1);
							out.CurveTo(X(a1x, a1y), Y(a1x, a1y), X(a2x, a2y), Y(a2x, a2y), X(vx, vy),
										Y(vx, vy));
							break;
						}
						default: break;
					}
					cx = vx;
					cy = vy;
				}
				(void)cx;
				(void)cy;
				out.Close();
				return true;
			}

			int32 NkPdfFont::DiagGlyphsWithShape(int32 n) const {
				if (!mHasFace)
					return -1; // pas de face du tout : distinct de « face sans contour »
				int32 ok = 0;
				for (int32 g = 0; g < n; ++g) {
					nkfont::NkFontVertexBuffer buf;
					if (nkfont::NkGetGlyphShape(&mFace, static_cast<NkGlyphId>(g), &buf) && buf.count > 0)
						++ok;
				}
				return ok;
			}


			// ============================================================
			// /ToUnicode
			// ============================================================
			//
			// La table est un petit programme PostScript. On n'en interprete que les
			// deux constructions qui portent l'information :
			//   <src> <dst>            entre beginbfchar et endbfchar
			//   <lo> <hi> <dst>        entre beginbfrange et endbfrange (dst incremente)
			//   <lo> <hi> [<d1> <d2>]  meme chose, avec un tableau de destinations
			// Les destinations sont en UTF-16BE ; on convertit en UTF-8.

			// UTF-16BE (octets bruts) -> UTF-8. Gere les paires de substitution : sans
			// elles, les ideogrammes et emoji ressortiraient casses.
			static void Utf16BeToUtf8(const NkVector<uint8> &b, NkString &out) {
				for (usize i = 0; i + 1 < b.Size(); i += 2) {
					uint32 u = (static_cast<uint32>(b[i]) << 8) | b[i + 1];
					if (u >= 0xD800u && u <= 0xDBFFu && i + 3 < b.Size()) {
						const uint32 lo = (static_cast<uint32>(b[i + 2]) << 8) | b[i + 3];
						if (lo >= 0xDC00u && lo <= 0xDFFFu) {
							u = 0x10000u + ((u - 0xD800u) << 10) + (lo - 0xDC00u);
							i += 2;
						}
					}
					if (u < 0x80u) {
						out += static_cast<char>(u);
					} else if (u < 0x800u) {
						out += static_cast<char>(0xC0u | (u >> 6));
						out += static_cast<char>(0x80u | (u & 0x3Fu));
					} else if (u < 0x10000u) {
						out += static_cast<char>(0xE0u | (u >> 12));
						out += static_cast<char>(0x80u | ((u >> 6) & 0x3Fu));
						out += static_cast<char>(0x80u | (u & 0x3Fu));
					} else {
						out += static_cast<char>(0xF0u | (u >> 18));
						out += static_cast<char>(0x80u | ((u >> 12) & 0x3Fu));
						out += static_cast<char>(0x80u | ((u >> 6) & 0x3Fu));
						out += static_cast<char>(0x80u | (u & 0x3Fu));
					}
				}
			}

			void NkPdfFont::ParseToUnicode(const NkPdfDoc &doc, const NkPdfVal &fontDict) {
				const NkPdfVal tu = doc.DictGet(fontDict, "ToUnicode");
				if (tu.kind != NK_PDF_STREAM)
					return;
				NkVector<uint8> d;
				if (!doc.DecodeStream(tu, d) || d.Empty())
					return;

				const uint8 *p = d.Data();
				const usize n = d.Size();

				auto keyword = [&](usize i, const char *kw) {
					usize k = 0;
					for (; kw[k]; ++k)
						if (i + k >= n || p[i + k] != static_cast<uint8>(kw[k]))
							return false;
					return true;
				};
				auto readHex = [&](usize &i, NkVector<uint8> &out) -> bool {
					while (i < n && p[i] != 0x3C && p[i] != 0x3E && p[i] != 0x5B && p[i] != 0x5D)
						++i;
					if (i >= n || p[i] != 0x3C)
						return false;
					++i;
					out.Clear();
					int32 hi = -1;
					for (; i < n && p[i] != 0x3E; ++i) {
						int32 h = -1;
						const uint8 c = p[i];
						if (c >= 0x30 && c <= 0x39)
							h = c - 0x30;
						else if (c >= 0x61 && c <= 0x66)
							h = c - 0x61 + 10;
						else if (c >= 0x41 && c <= 0x46)
							h = c - 0x41 + 10;
						else
							continue;
						if (hi < 0)
							hi = h;
						else {
							out.PushBack(static_cast<uint8>((hi << 4) | h));
							hi = -1;
						}
					}
					if (hi >= 0)
						out.PushBack(static_cast<uint8>(hi << 4));
					if (i < n)
						++i;
					return true;
				};
				auto toCode = [](const NkVector<uint8> &b) {
					uint32 v = 0;
					for (usize i = 0; i < b.Size() && i < 4; ++i)
						v = (v << 8) | b[i];
					return v;
				};

				for (usize i = 0; i < n;) {
					if (keyword(i, "beginbfchar")) {
						i += 11;
						NkVector<uint8> src, dst;
						while (i < n && !keyword(i, "endbfchar")) {
							if (!readHex(i, src))
								break;
							if (!readHex(i, dst))
								break;
							NkString txt;
							Utf16BeToUtf8(dst, txt);
							if (!txt.Empty()) {
								mUniCodes.PushBack(toCode(src));
								mUniText.PushBack(txt);
							}
						}
						continue;
					}
					if (keyword(i, "beginbfrange")) {
						i += 12;
						NkVector<uint8> lo, hi2, dst;
						while (i < n && !keyword(i, "endbfrange")) {
							if (!readHex(i, lo))
								break;
							if (!readHex(i, hi2))
								break;
							usize j = i;
							while (j < n && (p[j] == 32 || p[j] == 13 || p[j] == 10 || p[j] == 9))
								++j;
							const uint32 c0 = toCode(lo), c1 = toCode(hi2);
							// Borne : un intervalle aberrant ferait exploser la memoire.
							const uint32 lim = (c1 > c0 + 65535u) ? c0 + 65535u : c1;
							if (j < n && p[j] == 0x5B) { // tableau de destinations
								i = j + 1;
								for (uint32 cc = c0; cc <= lim && i < n; ++cc) {
									usize k = i;
									while (k < n && (p[k] == 32 || p[k] == 13 || p[k] == 10))
										++k;
									if (k < n && p[k] == 0x5D) {
										i = k + 1;
										break;
									}
									if (!readHex(i, dst))
										break;
									NkString txt;
									Utf16BeToUtf8(dst, txt);
									if (!txt.Empty()) {
										mUniCodes.PushBack(cc);
										mUniText.PushBack(txt);
									}
								}
								continue;
							}
							if (!readHex(i, dst))
								break;
							// Destination INCREMENTALE : le dernier caractere avance avec
							// le code source.
							for (uint32 cc = c0; cc <= lim; ++cc) {
								NkVector<uint8> cur = dst;
								if (cur.Size() >= 2) {
									uint32 tail = (static_cast<uint32>(cur[cur.Size() - 2]) << 8) |
												  cur[cur.Size() - 1];
									tail += (cc - c0);
									cur[cur.Size() - 2] = static_cast<uint8>((tail >> 8) & 0xFFu);
									cur[cur.Size() - 1] = static_cast<uint8>(tail & 0xFFu);
								} else if (cur.Size() == 1) {
									cur[0] = static_cast<uint8>((cur[0] + (cc - c0)) & 0xFFu);
								}
								NkString txt;
								Utf16BeToUtf8(cur, txt);
								if (!txt.Empty()) {
									mUniCodes.PushBack(cc);
									mUniText.PushBack(txt);
								}
							}
						}
						continue;
					}
					++i;
				}
			}

			NkString NkPdfFont::ToUnicode(uint32 code) const {
				for (usize i = 0; i < mUniCodes.Size(); ++i)
					if (mUniCodes[i] == code)
						return mUniText[i];
				return NkString();
			}

			// ============================================================
			// Cache
			// ============================================================

			void NkPdfFontCache::Clear() {
				for (usize i = 0; i < mSlots.Size(); ++i)
					delete mSlots[i].font;
				mSlots.Clear();
			}

			NkPdfFont *NkPdfFontCache::Get(const NkPdfDoc &doc, const NkPdfVal &resources, const char *name,
										   int32 nameLen) {
				if (!name || nameLen <= 0)
					return nullptr;
				NkString key;
				for (int32 i = 0; i < nameLen; ++i)
					key += name[i];
				for (usize i = 0; i < mSlots.Size(); ++i)
					if (mSlots[i].name == key)
						return mSlots[i].font; // peut etre nullptr : echec deja constate

				const NkPdfVal fonts = doc.DictGet(resources, "Font");
				const NkPdfVal fd = doc.DictGet(fonts, key.CStr());

				Slot s;
				s.name = key;
				s.font = nullptr;
				if (fd.IsDictLike()) {
					NkPdfFont *f = new NkPdfFont();
					if (f->Load(doc, fd))
						s.font = f;
					else
						delete f; // police sans programme exploitable : on memorise
								  // l'echec pour ne pas le retenter a chaque glyphe
				}
				mSlots.PushBack(s);
				return s.font;
			}

		} // namespace pdf
	} // namespace media
} // namespace nkentseu
