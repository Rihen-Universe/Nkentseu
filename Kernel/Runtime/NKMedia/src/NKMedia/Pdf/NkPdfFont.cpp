//
// NkPdfFont.cpp — voir NkPdfFont.h.
//
#include "NKMedia/Pdf/NkPdfFont.h"
#include "NKMedia/Pdf/NkPdfGlyphList.h"
#include "NKMedia/Pdf/NkPdfType1.h"
#include "NKFont/Core/NkFontCff.h" // (06/10) CFF nu des PDF : CID, noms, encodage interne
#include "NKThreading/NkMutex.h"
#include <cstdlib>

namespace nkentseu {
	namespace media {
		namespace pdf {

			// ============================================================
			// (05/10) LES POLICES TYPE 1 (/FontFile) : DESSINEES
			// ============================================================
			// Leur programme est lu par NkPdfType1 (dechiffrement eexec, Subrs,
			// CharStrings, interpreteur de charstrings). Il est RATTACHE a la police par
			// cette table, et non par un membre : le chantier ne touche que NkPdfFont.cpp
			// et NkPdfType1.* (NkPdfFont.h, inclus par les applications, reste tel quel).
			// Le rendu des pages peut tourner sur plusieurs fils : la table est verrouillee.
			// Contre-epreuves (NK_PDF_MUTATION) : type1-eexec-faux, type1-seac-ignore
			// (NkPdfType1.cpp), type1-encodage-interne-seul, pdf-noms-tex-coupes (ici).
			namespace {
				struct T1Lien {
						const NkPdfFont *police = nullptr;
						NkPdfType1 *t1 = nullptr;
						int32 index[256];
				};
				threading::NkMutex &T1Verrou() {
					static threading::NkMutex m;
					return m;
				}
				NkVector<T1Lien> &T1Liens() {
					static NkVector<T1Lien> v;
					return v;
				}
				bool T1Mutation(const char *nom) {
					const char *v = std::getenv("NK_PDF_MUTATION");
					if (!v || !nom)
						return false;
					int32 i = 0;
					while (v[i] && nom[i] && v[i] == nom[i])
						++i;
					return v[i] == 0 && nom[i] == 0;
				}
				/// Le glyphe Type 1 du code `code` (le programme, et l'index du glyphe).
				const NkPdfType1 *T1Glyphe(const NkPdfFont *f, uint32 code, int32 &index) {
					index = -1;
					T1Verrou().Lock();
					const NkPdfType1 *r = nullptr;
					NkVector<T1Lien> &v = T1Liens();
					for (usize i = 0; i < v.Size(); ++i)
						if (v[i].police == f) {
							if (code < 256u)
								index = v[i].index[code];
							r = v[i].t1;
							break;
						}
					T1Verrou().Unlock();
					return r;
				}
				void T1Detacher(const NkPdfFont *f) {
					T1Verrou().Lock();
					NkVector<T1Lien> &v = T1Liens();
					for (usize i = 0; i < v.Size(); ++i)
						if (v[i].police == f) {
							delete v[i].t1;
							v[i] = v[v.Size() - 1];
							v.PopBack();
							break;
						}
					T1Verrou().Unlock();
				}
				/// code -> glyphe d'un programme Type 1 (defini apres les tables d'encodage).
				void T1Indexer(const NkPdfType1 *p, const NkVector<uint32> &codes, const NkVector<NkString> &noms, usize nDiff,
							   int32 baseEnc, int32 *index);
				/// (06/10) Le meme, pour un programme CFF (Type1C) lu par NKFont.
				void CffIndexer(const nkfont::NkFontFaceInfo &f, const NkVector<uint32> &codes,
								const NkVector<NkString> &noms, usize nDiff, int32 baseEnc, int32 *index);
				/// Le texte UTF-8 d'un point de code (BMP).
				NkString T1Utf8(uint32 u) {
					NkString t;
					if (u < 0x80u)
						t += static_cast<char>(u);
					else if (u < 0x800u) {
						t += static_cast<char>(0xC0u | (u >> 6));
						t += static_cast<char>(0x80u | (u & 0x3Fu));
					} else {
						t += static_cast<char>(0xE0u | (u >> 12));
						t += static_cast<char>(0x80u | ((u >> 6) & 0x3Fu));
						t += static_cast<char>(0x80u | (u & 0x3Fu));
					}
					return t;
				}
			} // namespace

			// ============================================================
			// Chargement
			// ============================================================

			void NkPdfFont::Unload() {
				T1Detacher(this);
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
				mNameCodes.Clear();
				mNames.Clear();
				mCidToGid.Clear();
				mCffIndexe = false;
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
				} else {
					// Police SIMPLE : /Encoding peut etre un nom direct ou un
					// dictionnaire portant /BaseEncoding. C'est la declaration qui
					// permettra de lire le texte d'une police sans /ToUnicode.
					const NkPdfVal enc = doc.DictGet(fontDict, "Encoding");
					NkPdfVal encName = enc;
					if (enc.IsDictLike())
						encName = doc.DictGet(enc, "BaseEncoding");
					if (doc.NameIs(encName, "WinAnsiEncoding"))
						mBaseEnc = NK_ENC_WINANSI;
					else if (doc.NameIs(encName, "MacRomanEncoding"))
						mBaseEnc = NK_ENC_MACROMAN;
					else if (doc.NameIs(encName, "StandardEncoding"))
						mBaseEnc = NK_ENC_STANDARD;

					// /Differences : surcharges par NOM de glyphe. Un nombre pose le
					// code courant, chaque nom suivant s'y assigne puis l'incremente.
					if (enc.IsDictLike()) {
						const NkPdfVal diff = doc.DictGet(enc, "Differences");
						if (diff.kind == NK_PDF_ARRAY) {
							uint32 cur = 0;
							for (int32 i = 0; i < diff.b; ++i) {
								const NkPdfVal v = doc.ArrayAt(diff, i);
								if (v.IsNum()) {
									cur = static_cast<uint32>(v.num);
								} else if (v.kind == NK_PDF_NAME) {
									int32 ln = 0;
									const char *nm = doc.Text(v, &ln);
									NkString s;
									for (int32 k = 0; k < ln; ++k)
										s += nm[k];
									mNameCodes.PushBack(cur++);
									mNames.PushBack(s);
								}
							}
						}
					}
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

				// Type 1 brut : ses charstrings ne sont toujours pas interpretes,
				// mais sa table d'encodage (code -> nom de glyphe) vit dans la
				// partie CLAIRE du programme (« dup N /nom put », avant eexec) — et
				// jointe a la liste de noms d'Adobe, elle suffit a LIRE le texte.
				// C'est le cas des PDF dvips/LaTeX sans /ToUnicode (arXiv, notes
				// Eberly), refuses en bloc avant ce repli. On ne le fait que si
				// rien d'autre ne donne deja le texte.
				const usize nDiff = mNameCodes.Size(); // (05/10) les noms venus de /Differences
				if (!mTwoByte && mUniCodes.Empty() && mNames.Empty()) {
					const NkPdfVal t1 = doc.DictGet(fd, "FontFile");
					if (t1.kind == NK_PDF_STREAM) {
						NkVector<uint8> clair;
						if (doc.DecodeStream(t1, clair) && !clair.Empty())
							ParseType1Encoding(clair);
					}
				}

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
				// (06/10) `cmapUnicodeRequise = false` : un PDF adresse ses glyphes par
				// CID, par nom ou par code. Avec `true` (le defaut, fait pour l'atlas
				// d'interface), NKFont refusait le CFF NU (/FontFile3) et tout TrueType
				// sans cmap Unicode -- c'est-a-dire toutes les polices des chapitres
				// XeLaTeX de Rodolf : pages sans un seul caractere.
				if (prog.kind != NK_PDF_STREAM || !doc.DecodeStream(prog, mProgram) || mProgram.Empty() ||
					!nkfont::NkInitFontFace(&mFace, mProgram.Data(), mProgram.Size(), 0, /*cmapUnicodeRequise=*/false)) {
					mHasFace = false;
					mUnitsPerEmInv = 1.0 / 1000.0; // convention PDF pour les largeurs
					// (05/10) Le Type 1 brut : son programme lu, ses glyphes dessines.
					const NkPdfVal t1 = doc.DictGet(fd, "FontFile");
					if (!mTwoByte && t1.kind == NK_PDF_STREAM) {
						NkVector<uint8> brut;
						if (doc.DecodeStream(t1, brut) && !brut.Empty()) {
							NkPdfType1 *p = new NkPdfType1();
							const double l1 = doc.Num(doc.DictGet(t1, "Length1"), 0.0);
							if (p->Load(brut.Data(), brut.Size(), l1 > 0.0 ? static_cast<usize>(l1) : 0u)) {
								T1Lien lien;
								lien.police = this;
								lien.t1 = p;
								const int32 be = mBaseEnc == NK_ENC_WINANSI ? 1 : mBaseEnc == NK_ENC_MACROMAN ? 2 : mBaseEnc == NK_ENC_STANDARD ? 3 : 0;
								T1Indexer(p, mNameCodes, mNames, nDiff, be, lien.index);
								T1Verrou().Lock();
								T1Liens().PushBack(lien);
								T1Verrou().Unlock();
							} else
								delete p;
						}
					}
					return true; // lisible ; dessinable si le Type 1 a ete lu
				}
				mHasFace = true;

				// (06/10) Adressage des glyphes selon le programme.
				if (mFace.isCFF && !mTwoByte) { // Type1C : par nom, comme le Type 1
					const int32 be = mBaseEnc == NK_ENC_WINANSI ? 1 : mBaseEnc == NK_ENC_MACROMAN ? 2 : mBaseEnc == NK_ENC_STANDARD ? 3 : 0;
					CffIndexer(mFace, mNameCodes, mNames, nDiff, be, mCffCodeGid);
					mCffIndexe = true;
				}
				if (!mFace.isCFF && mTwoByte) { // CIDFontType2 : /CIDToGIDMap en flux
					const NkPdfVal c2g = doc.DictGet(descFont, "CIDToGIDMap");
					NkVector<uint8> brut;
					if (c2g.kind == NK_PDF_STREAM && doc.DecodeStream(c2g, brut))
						for (usize i = 0; i + 1 < brut.Size(); i += 2)
							mCidToGid.PushBack(static_cast<uint16>((brut[i] << 8) | brut[i + 1]));
				}

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
					// (06/10) CFF : le charset d'une police CID associe glyphe et CID ;
					// une police nommee dans une CIDFont prend le CID comme indice.
					if (mFace.isCFF)
						return nkfont::NkCffGlyphFromCID(&mFace, code);
					// TrueType : /CIDToGIDMap en flux, sinon Identity (CID = glyphe).
					if (!mCidToGid.Empty())
						return code < mCidToGid.Size() ? static_cast<NkGlyphId>(mCidToGid[code]) : 0;
					return static_cast<NkGlyphId>(code);
				}
				if (mFace.isCFF) // (06/10) Type1C : la table indexee au chargement
					return (mCffIndexe && code < 256u) ? static_cast<NkGlyphId>(mCffCodeGid[code]) : 0;
				// Police simple : on tente la table de caracteres avec le code tel
				// quel, puis (06/10) les cmaps SYMBOLIQUES (3,0) et (1,0) d'un sous-
				// ensemble sans Unicode. Si rien ne repond, le code COMME identifiant
				// de glyphe — le comportement des polices a encodage sur mesure,
				// tres frequentes dans les PDF generes.
				NkGlyphId g = nkfont::NkFindGlyphIndex(&mFace, static_cast<NkFontCodepoint>(code));
				if (g == 0)
					g = nkfont::NkFindGlyphIndexSymbolique(&mFace, code);
				if (g != 0)
					return g;
				return static_cast<NkGlyphId>(code);
			}

			bool NkPdfFont::AppendGlyph(uint32 code, double tx, double ty, double scale, double shearX,
										double vScale, NkPdfPath &out) const {
				if (!mHasFace) {
					// (05/10) Type 1 : le contour de son interpreteur, mis a l'echelle par la
					// /FontMatrix (unites du programme -> em), puis comme les autres.
					int32 idx = -1;
					const NkPdfType1 *t1 = T1Glyphe(this, code, idx);
					if (!t1 || idx < 0)
						return false;
					NkVector<NkPdfT1Op> ops;
					if (!t1->Outline(idx, ops) || ops.Empty())
						return false;
					const double *M = t1->Matrix();
					auto EX = [&](double x, double y) { return M[0] * x + M[2] * y + M[4]; };
					auto EY = [&](double x, double y) { return M[1] * x + M[3] * y + M[5]; };
					auto X = [&](double x, double y) { return tx + EX(x, y) * scale + EY(x, y) * scale * shearX; };
					auto Y = [&](double x, double y) { return ty + EY(x, y) * scale * vScale; };
					for (usize i = 0; i < ops.Size(); ++i) {
						const NkPdfT1Op &o = ops[i];
						if (o.type == 0)
							out.MoveTo(X(o.x2, o.y2), Y(o.x2, o.y2));
						else if (o.type == 1)
							out.LineTo(X(o.x2, o.y2), Y(o.x2, o.y2));
						else if (o.type == 2)
							out.CurveTo(X(o.x0, o.y0), Y(o.x0, o.y0), X(o.x1, o.y1), Y(o.x1, o.y1), X(o.x2, o.y2), Y(o.x2, o.y2));
						else
							out.Close();
					}
					out.Close();
					return true;
				}
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
				// La table est DECLAREE. Ce qui suit peut encore echouer — et le
				// noter ici est ce qui permettra de distinguer « le document ne dit
				// rien » de « nous ne savons pas le lire ».
				mAvaitToUnicode = true;
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
				auto skipWs = [&](usize &i) {
					while (i < n && (p[i] == 32 || p[i] == 13 || p[i] == 10 || p[i] == 9))
						++i;
				};

				// ⚠️ Les boucles de section testent leur mot-cle de fin APRES avoir
				// saute les blancs, et refusent d'avancer si le prochain caractere
				// n'est pas « < ». Sans ces deux gardes, le test « endbfchar » ne
				// matchait jamais (i pointait sur l'espace davant) et readHex sautait
				// PAR-DESSUS la fin de section jusqu'au « < » suivant — devorant la
				// section bfrange d'apres comme des paires bfchar. Un CMap a section
				// unique passait ; un CMap multi-sections (bfchar + bfrange + bfchar,
				// courant chez les generateurs d'ebooks) etait mutile en silence.
				for (usize i = 0; i < n;) {
					if (keyword(i, "beginbfchar")) {
						i += 11;
						NkVector<uint8> src, dst;
						for (;;) {
							skipWs(i);
							if (i >= n || keyword(i, "endbfchar"))
								break;
							if (p[i] != 0x3C)
								break; // structure inattendue : rendre la main au
									   // balayage exterieur plutot que devorer la suite
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
						for (;;) {
							skipWs(i);
							if (i >= n || keyword(i, "endbfrange"))
								break;
							if (p[i] != 0x3C)
								break; // meme garde que bfchar : ne pas devorer la suite
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

			// ── Encodages de base de la spec PDF (ISO 32000-1, annexe D) ──
			// Tables code -> point Unicode (BMP), GENEREES par script depuis les
			// codecs cp1252 / mac_roman de Python — jamais retapees a la main
			// (une transcription manuelle a deja produit un decalage d'indexation
			// ailleurs dans ce depot). WinAnsiEncoding coincide avec Windows-1252 ;
			// les codes que la spec laisse indefinis valent 0 (aucun texte rendu).
			static const uint16 kWinAnsiToUni[256] = {
				0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
				0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
				0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F,
				0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F,
				0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, 0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F,
				0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059, 0x005A, 0x005B, 0x005C, 0x005D, 0x005E, 0x005F,
				0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006A, 0x006B, 0x006C, 0x006D, 0x006E, 0x006F,
				0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077, 0x0078, 0x0079, 0x007A, 0x007B, 0x007C, 0x007D, 0x007E, 0x007F,
				0x20AC, 0x0000, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x0000, 0x017D, 0x0000,
				0x0000, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x0000, 0x017E, 0x0178,
				0x00A0, 0x00A1, 0x00A2, 0x00A3, 0x00A4, 0x00A5, 0x00A6, 0x00A7, 0x00A8, 0x00A9, 0x00AA, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x00AF,
				0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x00B4, 0x00B5, 0x00B6, 0x00B7, 0x00B8, 0x00B9, 0x00BA, 0x00BB, 0x00BC, 0x00BD, 0x00BE, 0x00BF,
				0x00C0, 0x00C1, 0x00C2, 0x00C3, 0x00C4, 0x00C5, 0x00C6, 0x00C7, 0x00C8, 0x00C9, 0x00CA, 0x00CB, 0x00CC, 0x00CD, 0x00CE, 0x00CF,
				0x00D0, 0x00D1, 0x00D2, 0x00D3, 0x00D4, 0x00D5, 0x00D6, 0x00D7, 0x00D8, 0x00D9, 0x00DA, 0x00DB, 0x00DC, 0x00DD, 0x00DE, 0x00DF,
				0x00E0, 0x00E1, 0x00E2, 0x00E3, 0x00E4, 0x00E5, 0x00E6, 0x00E7, 0x00E8, 0x00E9, 0x00EA, 0x00EB, 0x00EC, 0x00ED, 0x00EE, 0x00EF,
				0x00F0, 0x00F1, 0x00F2, 0x00F3, 0x00F4, 0x00F5, 0x00F6, 0x00F7, 0x00F8, 0x00F9, 0x00FA, 0x00FB, 0x00FC, 0x00FD, 0x00FE, 0x00FF,
			};
			static const uint16 kMacRomanToUni[256] = {
				0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
				0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
				0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F,
				0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F,
				0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, 0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F,
				0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059, 0x005A, 0x005B, 0x005C, 0x005D, 0x005E, 0x005F,
				0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006A, 0x006B, 0x006C, 0x006D, 0x006E, 0x006F,
				0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077, 0x0078, 0x0079, 0x007A, 0x007B, 0x007C, 0x007D, 0x007E, 0x007F,
				0x00C4, 0x00C5, 0x00C7, 0x00C9, 0x00D1, 0x00D6, 0x00DC, 0x00E1, 0x00E0, 0x00E2, 0x00E4, 0x00E3, 0x00E5, 0x00E7, 0x00E9, 0x00E8,
				0x00EA, 0x00EB, 0x00ED, 0x00EC, 0x00EE, 0x00EF, 0x00F1, 0x00F3, 0x00F2, 0x00F4, 0x00F6, 0x00F5, 0x00FA, 0x00F9, 0x00FB, 0x00FC,
				0x2020, 0x00B0, 0x00A2, 0x00A3, 0x00A7, 0x2022, 0x00B6, 0x00DF, 0x00AE, 0x00A9, 0x2122, 0x00B4, 0x00A8, 0x2260, 0x00C6, 0x00D8,
				0x221E, 0x00B1, 0x2264, 0x2265, 0x00A5, 0x00B5, 0x2202, 0x2211, 0x220F, 0x03C0, 0x222B, 0x00AA, 0x00BA, 0x03A9, 0x00E6, 0x00F8,
				0x00BF, 0x00A1, 0x00AC, 0x221A, 0x0192, 0x2248, 0x2206, 0x00AB, 0x00BB, 0x2026, 0x00A0, 0x00C0, 0x00C3, 0x00D5, 0x0152, 0x0153,
				0x2013, 0x2014, 0x201C, 0x201D, 0x2018, 0x2019, 0x00F7, 0x25CA, 0x00FF, 0x0178, 0x2044, 0x20AC, 0x2039, 0x203A, 0xFB01, 0xFB02,
				0x2021, 0x00B7, 0x201A, 0x201E, 0x2030, 0x00C2, 0x00CA, 0x00C1, 0x00CB, 0x00C8, 0x00CD, 0x00CE, 0x00CF, 0x00CC, 0x00D3, 0x00D4,
				0xF8FF, 0x00D2, 0x00DA, 0x00DB, 0x00D9, 0x0131, 0x02C6, 0x02DC, 0x00AF, 0x02D8, 0x02D9, 0x02DA, 0x00B8, 0x02DD, 0x02DB, 0x02C7,
			};

			// (05/10) code -> glyphe d'un programme Type 1 : /Differences, puis l'encodage de
			// BASE declare (WinAnsi / MacRoman : par le TEXTE de chaque glyphe ; Standard :
			// par son nom), puis l'encodage interne du programme (ISO 32000-1, 9.6.6.2).
			namespace {
				void T1Indexer(const NkPdfType1 *p, const NkVector<uint32> &codes, const NkVector<NkString> &noms, usize nDiff,
							   int32 baseEnc, int32 *index) {
					const bool interneSeul = T1Mutation("type1-encodage-interne-seul");
					NkVector<NkString> texteGlyphe;
					if (!interneSeul && (baseEnc == 1 || baseEnc == 2))
						for (int32 g = 0; g < p->GlyphCount(); ++g)
							texteGlyphe.PushBack(NkPdfGlyphNameToText(p->GlyphName(g).CStr(), static_cast<int32>(p->GlyphName(g).Size())));
					for (uint32 code = 0; code < 256u; ++code) {
						int32 idx = -1;
						if (!interneSeul)
							for (usize i = 0; i < nDiff && i < codes.Size(); ++i)
								if (codes[i] == code)
									idx = p->GlyphIndex(noms[i].CStr(), static_cast<int32>(noms[i].Size()));
						if (idx < 0 && !interneSeul && baseEnc == 3) {
							const char *nm = NkPdfType1::StandardName(code);
							int32 l = 0;
							while (nm && nm[l])
								++l;
							idx = p->GlyphIndex(nm, l);
						}
						if (idx < 0 && !texteGlyphe.Empty()) {
							const uint32 u = baseEnc == 1 ? kWinAnsiToUni[code] : kMacRomanToUni[code];
							if (u) {
								const NkString t = T1Utf8(u);
								for (usize g = 0; g < texteGlyphe.Size() && idx < 0; ++g)
									if (texteGlyphe[g] == t)
										idx = static_cast<int32>(g);
							}
						}
						if (idx < 0) {
							const char *nm = p->BuiltinName(code);
							int32 l = 0;
							while (nm && nm[l])
								++l;
							idx = p->GlyphIndex(nm, l);
						}
						index[code] = idx;
					}
				}

				// (06/10) Le meme ordre que T1Indexer, pour un CFF : /Differences, encodage
				// de BASE (Standard par nom, WinAnsi/MacRoman par le texte du glyphe), puis
				// encodage INTERNE du programme. 0 = aucun glyphe.
				void CffIndexer(const nkfont::NkFontFaceInfo &f, const NkVector<uint32> &codes,
								const NkVector<NkString> &noms, usize nDiff, int32 baseEnc, int32 *index) {
					NkVector<NkString> texteGlyphe;
					if (baseEnc == 1 || baseEnc == 2) {
						char nom[128];
						const int32 ng = nkfont::NkCffGlyphCount(&f);
						for (int32 g = 0; g < ng; ++g) {
							const int32 l = nkfont::NkCffGlyphName(&f, static_cast<NkGlyphId>(g), nom, static_cast<int32>(sizeof(nom)));
							texteGlyphe.PushBack(l > 0 ? NkPdfGlyphNameToText(nom, l) : NkString());
						}
					}
					for (uint32 code = 0; code < 256u; ++code) {
						NkGlyphId g = 0;
						for (usize i = 0; i < nDiff && i < codes.Size() && !g; ++i)
							if (codes[i] == code)
								g = nkfont::NkCffGlyphFromName(&f, noms[i].CStr(), static_cast<int32>(noms[i].Size()));
						if (!g && baseEnc == 3) {
							const char *nm = NkPdfType1::StandardName(code);
							int32 l = 0;
							while (nm && nm[l])
								++l;
							g = nkfont::NkCffGlyphFromName(&f, nm, l);
						}
						if (!g && !texteGlyphe.Empty()) {
							const uint32 u = baseEnc == 1 ? kWinAnsiToUni[code] : kMacRomanToUni[code];
							if (u) {
								const NkString t = T1Utf8(u);
								for (usize k = 1; k < texteGlyphe.Size() && !g; ++k)
									if (texteGlyphe[k] == t)
										g = static_cast<NkGlyphId>(k);
							}
						}
						if (!g)
							g = nkfont::NkCffGlyphFromCode(&f, code);
						index[code] = static_cast<int32>(g);
					}
				}
			} // namespace

			// Table d'encodage en CLAIR d'un programme Type 1 : la partie avant
			// « eexec » est du PostScript lisible, et l'encodage s'y ecrit
			// « dup <code> /<nom> put », une ligne par caractere. On ne lit QUE
			// cela — aucun charstring, aucun dechiffrement.
			void NkPdfFont::ParseType1Encoding(const NkVector<uint8> &prog) {
				const uint8 *p = prog.Data();
				usize n = prog.Size();
				for (usize i = 0; i + 5 <= n; ++i)
					if (p[i] == 'e' && p[i + 1] == 'e' && p[i + 2] == 'x' && p[i + 3] == 'e' &&
						p[i + 4] == 'c') {
						n = i; // tout ce qui suit est chiffre : hors de portee, et inutile
						break;
					}
				auto estBlanc = [](uint8 c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
				for (usize i = 0; i + 4 <= n;) {
					if (p[i] == 'd' && p[i + 1] == 'u' && p[i + 2] == 'p' && estBlanc(p[i + 3])) {
						usize j = i + 4;
						while (j < n && estBlanc(p[j]))
							++j;
						uint32 code = 0;
						bool num = false;
						while (j < n && p[j] >= '0' && p[j] <= '9') {
							code = code * 10u + static_cast<uint32>(p[j] - '0');
							++j;
							num = true;
						}
						while (j < n && estBlanc(p[j]))
							++j;
						if (num && j < n && p[j] == '/') {
							++j;
							NkString nom;
							while (j < n && p[j] > ' ' && p[j] != '/' && p[j] != '(' && p[j] != ')' &&
								   p[j] != '[' && p[j] != ']' && p[j] != '{' && p[j] != '}') {
								nom += static_cast<char>(p[j]);
								++j;
							}
							usize k = j;
							while (k < n && estBlanc(p[k]))
								++k;
							// Seul le « put » scelle l'assignation ; sans lui, ce
							// « dup » appartenait a une autre construction.
							if (k + 3 <= n && p[k] == 'p' && p[k + 1] == 'u' && p[k + 2] == 't' &&
								nom.Size() > 0 && code < 256u && !(nom == ".notdef")) {
								mNameCodes.PushBack(code);
								mNames.PushBack(nom);
								i = k + 3;
								continue;
							}
						}
						i = j;
						continue;
					}
					++i;
				}
			}

			NkString NkPdfFont::ToUnicode(uint32 code) const {
				for (usize i = 0; i < mUniCodes.Size(); ++i)
					if (mUniCodes[i] == code)
						return mUniText[i];
				// Noms de glyphes (/Differences ou table Type 1 en clair) :
				// prioritaires sur l'encodage de base, qu'ils surchargent code par
				// code. Un nom opaque (« a35 ») rend vide -> l'encodage de base
				// reste tente ensuite.
				for (usize i = 0; i < mNameCodes.Size(); ++i)
					if (mNameCodes[i] == code) {
						NkString t = NkPdfGlyphNameToText(mNames[i].CStr(),
														  static_cast<int32>(mNames[i].Size()));
						if (t.Size() > 0)
							return t;
						// (05/10) Les noms de TeX hors de la liste d'Adobe : la police des grands
						// operateurs (cmex) nomme ses glyphes par leur TAILLE (« summationdisplay »,
						// « parenleftbig », « radicalBigg ») : le nom sans le suffixe de taille est,
						// lui, dans la liste (« summation » -> U+2211). Contre-epreuve :
						// NK_PDF_MUTATION=pdf-noms-tex-coupes.
						if (!T1Mutation("pdf-noms-tex-coupes")) {
							static const char *kSuffixes[] = {"display", "text", "Bigg", "bigg", "Big", "big", "bt", "tp", "bd"};
							const NkString &n = mNames[i];
							for (const char *suf : kSuffixes) {
								int32 ls = 0;
								while (suf[ls])
									++ls;
								const int32 ln = static_cast<int32>(n.Size());
								if (ln <= ls)
									continue;
								bool fin = true;
								for (int32 k = 0; k < ls && fin; ++k)
									fin = n.CStr()[ln - ls + k] == suf[k];
								if (!fin)
									continue;
								t = NkPdfGlyphNameToText(n.CStr(), ln - ls);
								if (t.Size() > 0)
									return t;
							}
						}
						break;
					}
				// Repli : police simple SANS entree /ToUnicode pour ce code, mais
				// dont le document declare l'encodage de base. La spec publie le
				// sens de chaque code — rien n'est devine. StandardEncoding n'est
				// rendu que sur sa zone ASCII, ou il coincide avec les autres ;
				// au-dela il diverge et il vaut mieux se taire que se tromper.
				if (!mTwoByte && code < 256u && mBaseEnc != NK_ENC_NONE) {
					uint32 u = 0;
					if (mBaseEnc == NK_ENC_WINANSI)
						u = kWinAnsiToUni[code];
					else if (mBaseEnc == NK_ENC_MACROMAN)
						u = kMacRomanToUni[code];
					else if (code >= 0x20u && code <= 0x7Eu)
						u = code;
					if (u) {
						NkString txt;
						if (u < 0x80u) {
							txt += static_cast<char>(u);
						} else if (u < 0x800u) {
							txt += static_cast<char>(0xC0u | (u >> 6));
							txt += static_cast<char>(0x80u | (u & 0x3Fu));
						} else {
							txt += static_cast<char>(0xE0u | (u >> 12));
							txt += static_cast<char>(0x80u | ((u >> 6) & 0x3Fu));
							txt += static_cast<char>(0x80u | (u & 0x3Fu));
						}
						return txt;
					}
				}
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

				// Resoudre le dictionnaire AVANT de consulter le cache : le nom seul
				// ne suffit pas. Dans une meme page, un formulaire XObject porte ses
				// propres ressources, et « /F4 » peut y designer une AUTRE police que
				// le /F4 du contenu principal. Un cache par nom rendait alors la
				// premiere : les codes du flux etaient cherches dans la table d'une
				// police etrangere, et tout ressortait vide. L'identite d'un objet
				// dictionnaire est (kind, a) — `a` indexe sa premiere entree dans
				// l'arene du document, unique par objet.
				const NkPdfVal fonts = doc.DictGet(resources, "Font");
				const NkPdfVal fd = doc.DictGet(fonts, key.CStr());

				for (usize i = 0; i < mSlots.Size(); ++i)
					if (mSlots[i].dictA == fd.a && mSlots[i].dictKind == fd.kind && mSlots[i].name == key)
						return mSlots[i].font; // peut etre nullptr : echec deja constate

				Slot s;
				s.name = key;
				s.dictA = fd.a;
				s.dictKind = fd.kind;
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
