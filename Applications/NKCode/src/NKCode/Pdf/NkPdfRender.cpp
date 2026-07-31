//
// NkPdfRender.cpp — interpretation du flux de contenu (voir NkPdfRender.h).
//
#include "NKCode/Pdf/NkPdfRender.h"

#include "NKImage/Core/NkImage.h"

namespace nkentseu {
	namespace nkcode {
		namespace pdf {

			static inline bool IsWsC(uint8 c) {
				return c == 0 || c == 9 || c == 10 || c == 12 || c == 13 || c == 32;
			}
			static inline bool IsDelimC(uint8 c) {
				return c == '(' || c == ')' || c == '<' || c == '>' || c == '[' || c == ']' || c == '{' ||
					   c == '}' || c == '/' || c == '%';
			}
			static inline double Abs2(double v) { return v < 0 ? -v : v; }
			static double Sqrt2(double v) {
				if (v <= 0.0)
					return 0.0;
				double x = v;
				for (int32 i = 0; i < 24; ++i)
					x = 0.5 * (x + v / x);
				return x;
			}
			static inline uint8 ToByte(double v) {
				if (v <= 0.0)
					return 0;
				if (v >= 1.0)
					return 255;
				return static_cast<uint8>(v * 255.0 + 0.5);
			}

			double NkPdfMat::Scale() const {
				// Moyenne geometrique des deux axes : une epaisseur de trait doit
				// suivre l'echelle meme quand la matrice est anisotrope.
				const double sx = Sqrt2(a * a + b * b);
				const double sy = Sqrt2(c * c + d * d);
				return Sqrt2(sx * sy);
			}

			void NkPdfRenderer::Note(const char *what) {
				// Une seule mention par fonctionnalite : un degrade repete 500 fois ne
				// doit pas produire 500 lignes.
				int32 n = 0;
				while (what[n])
					++n;
				const char *s = mUnsupported.CStr();
				const int32 tot = static_cast<int32>(mUnsupported.Size());
				for (int32 i = 0; i + n <= tot; ++i) {
					bool same = true;
					for (int32 k = 0; k < n && same; ++k)
						same = s[i + k] == what[k];
					if (same)
						return;
				}
				if (!mUnsupported.Empty())
					mUnsupported += ", ";
				mUnsupported += what;
			}

			// ============================================================
			// Analyse lexicale du flux de contenu
			// ============================================================
			//
			// Un flux de contenu est une suite « operandes puis operateur », en
			// notation postfixee. On accumule les operandes dans une pile bornee et on
			// agit sur l'operateur.

			namespace {
				struct Tok {
						enum Kind { Num, Str, Name, ArrBeg, ArrEnd, Op, End } kind = End;
						double num = 0.0;
						const uint8 *ptr = nullptr; // Str/Name/Op : debut
						int32 len = 0;
				};

				class Lex {
					public:
						Lex(const NkVector<uint8> &b) : mB(b) {}

						Tok Next() {
							Tok t;
							SkipWs();
							if (mP >= mB.Size()) {
								t.kind = Tok::End;
								return t;
							}
							const uint8 c = mB[mP];

							if (c == '[') {
								++mP;
								t.kind = Tok::ArrBeg;
								return t;
							}
							if (c == ']') {
								++mP;
								t.kind = Tok::ArrEnd;
								return t;
							}
							if (c == '/') {
								++mP;
								t.kind = Tok::Name;
								t.ptr = mB.Data() + mP;
								while (mP < mB.Size() && !IsWsC(mB[mP]) && !IsDelimC(mB[mP]))
									++mP;
								t.len = static_cast<int32>(mB.Data() + mP - t.ptr);
								return t;
							}
							if (c == '(') { // chaine litterale : on renvoie sa PLAGE brute
								++mP;
								t.kind = Tok::Str;
								t.ptr = mB.Data() + mP;
								int32 nest = 1;
								while (mP < mB.Size()) {
									const uint8 ch = mB[mP];
									if (ch == '\\') {
										mP += 2;
										continue;
									}
									if (ch == '(')
										++nest;
									else if (ch == ')' && --nest == 0)
										break;
									++mP;
								}
								t.len = static_cast<int32>(mB.Data() + mP - t.ptr);
								if (mP < mB.Size())
									++mP; // ')'
								return t;
							}
							if (c == '<' && mP + 1 < mB.Size() && mB[mP + 1] != '<') {
								++mP; // chaine hexadecimale
								t.kind = Tok::Str;
								t.ptr = mB.Data() + mP;
								while (mP < mB.Size() && mB[mP] != '>')
									++mP;
								t.len = static_cast<int32>(mB.Data() + mP - t.ptr);
								t.num = 1.0; // marqueur « hexadecimal »
								if (mP < mB.Size())
									++mP;
								return t;
							}
							if (c == '<' && mP + 1 < mB.Size() && mB[mP + 1] == '<') {
								// Dictionnaire en ligne (BDC, gs...) : on le SAUTE, seul
								// son operateur nous interesse a ce stade.
								int32 depth = 0;
								while (mP + 1 < mB.Size()) {
									if (mB[mP] == '<' && mB[mP + 1] == '<') {
										++depth;
										mP += 2;
										continue;
									}
									if (mB[mP] == '>' && mB[mP + 1] == '>') {
										--depth;
										mP += 2;
										if (depth <= 0)
											break;
										continue;
									}
									++mP;
								}
								return Next();
							}
							if (c == '+' || c == '-' || c == '.' || (c >= '0' && c <= '9')) {
								const usize s = mP;
								if (mB[mP] == '+' || mB[mP] == '-')
									++mP;
								while (mP < mB.Size() && ((mB[mP] >= '0' && mB[mP] <= '9') || mB[mP] == '.'))
									++mP;
								double sign = 1.0, val = 0.0;
								usize q = s;
								if (mB[q] == '-') {
									sign = -1.0;
									++q;
								} else if (mB[q] == '+')
									++q;
								while (q < mP && mB[q] >= '0' && mB[q] <= '9')
									val = val * 10.0 + (mB[q++] - '0');
								if (q < mP && mB[q] == '.') {
									++q;
									double f = 0.1;
									while (q < mP && mB[q] >= '0' && mB[q] <= '9') {
										val += (mB[q++] - '0') * f;
										f *= 0.1;
									}
								}
								t.kind = Tok::Num;
								t.num = val * sign;
								return t;
							}
							// Operateur : suite de caracteres reguliers.
							t.kind = Tok::Op;
							t.ptr = mB.Data() + mP;
							while (mP < mB.Size() && !IsWsC(mB[mP]) && !IsDelimC(mB[mP]))
								++mP;
							t.len = static_cast<int32>(mB.Data() + mP - t.ptr);
							if (t.len == 0) { // delimiteur isole : evite la boucle infinie
								++mP;
								return Next();
							}
							return t;
						}

						usize Pos() const { return mP; }
						void SetPos(usize p) { mP = p; }

					private:
						void SkipWs() {
							for (;;) {
								while (mP < mB.Size() && IsWsC(mB[mP]))
									++mP;
								if (mP < mB.Size() && mB[mP] == '%') { // commentaire
									while (mP < mB.Size() && mB[mP] != '\n' && mB[mP] != '\r')
										++mP;
									continue;
								}
								return;
							}
						}
						const NkVector<uint8> &mB;
						usize mP = 0;
				};

				// Decode une chaine litterale PDF (echappements) ou hexadecimale.
				static void DecodeStr(const uint8 *p, int32 len, bool hex, NkVector<uint8> &out) {
					out.Clear();
					if (hex) {
						int32 hi = -1;
						for (int32 i = 0; i < len; ++i) {
							int32 h = -1;
							const uint8 c = p[i];
							if (c >= '0' && c <= '9')
								h = c - '0';
							else if (c >= 'a' && c <= 'f')
								h = c - 'a' + 10;
							else if (c >= 'A' && c <= 'F')
								h = c - 'A' + 10;
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
						return;
					}
					for (int32 i = 0; i < len; ++i) {
						uint8 c = p[i];
						if (c != '\\') {
							out.PushBack(c);
							continue;
						}
						if (++i >= len)
							break;
						const uint8 e = p[i];
						switch (e) {
							case 'n': out.PushBack('\n'); break;
							case 'r': out.PushBack('\r'); break;
							case 't': out.PushBack('\t'); break;
							case 'b': out.PushBack('\b'); break;
							case 'f': out.PushBack('\f'); break;
							case '\r':
								if (i + 1 < len && p[i + 1] == '\n')
									++i;
								break;
							case '\n': break;
							default:
								if (e >= '0' && e <= '7') {
									int32 o = e - '0';
									for (int32 k = 0; k < 2 && i + 1 < len && p[i + 1] >= '0' && p[i + 1] <= '7';
										 ++k)
										o = o * 8 + (p[++i] - '0');
									out.PushBack(static_cast<uint8>(o & 0xFF));
								} else
									out.PushBack(e);
								break;
						}
					}
				}

				static bool OpIs(const Tok &t, const char *s) {
					int32 i = 0;
					for (; s[i]; ++i)
						if (i >= t.len || t.ptr[i] != static_cast<uint8>(s[i]))
							return false;
					return i == t.len;
				}
			} // namespace

			// ============================================================
			// Rendu d'une page
			// ============================================================

			bool NkPdfRenderer::PagePixelSize(NkPdfDoc &doc, int32 pageIdx, double dpi, int32 *w,
											  int32 *h) {
				double x0 = 0, y0 = 0, x1 = 612, y1 = 792;
				doc.PageMediaBox(pageIdx, &x0, &y0, &x1, &y1);
				const double k = dpi / 72.0;
				const int32 rot = doc.PageRotate(pageIdx);
				const bool swap = (rot == 90 || rot == 270);
				const double wPt = x1 - x0, hPt = y1 - y0;
				const int32 pw = static_cast<int32>((swap ? hPt : wPt) * k + 0.5);
				const int32 ph = static_cast<int32>((swap ? wPt : hPt) * k + 0.5);
				if (pw < 1 || ph < 1)
					return false;
				if (w)
					*w = pw;
				if (h)
					*h = ph;
				return true;
			}

			bool NkPdfRenderer::RenderPageWindow(NkPdfDoc &doc, int32 pageIdx, double dpi, double offX,
												 double offY, NkPdfCanvas &out) {
				if (!out.Valid())
					return false;
				// Meme chemin que RenderPage, mais SANS recreer le canevas et avec une
				// translation supplementaire : la fenetre visible se retrouve a
				// l'origine du canevas.
				mWindowMode = true;
				mWinOffX = offX;
				mWinOffY = offY;
				const bool ok = RenderPage(doc, pageIdx, dpi, out);
				mWindowMode = false;
				return ok;
			}

			bool NkPdfRenderer::RenderPage(NkPdfDoc &doc, int32 pageIdx, double dpi, NkPdfCanvas &out) {
				mDoc = &doc;
				mCv = &out;
				mFonts.Clear();
				mLastFont = nullptr;
				mUnsupported.Clear();
				mStats = Stats();
				mStack.Clear();
				mPath.Clear();
				mPendingClip = false;

				const NkPdfVal page = doc.Page(pageIdx);
				if (!page.IsDictLike())
					return false;

				double x0 = 0, y0 = 0, x1 = 612, y1 = 792; // Lettre par defaut
				if (!doc.PageMediaBox(pageIdx, &x0, &y0, &x1, &y1)) {
					// Une page sans MediaBox est malformee : plutot que d'echouer, on
					// prend le format par defaut de la specification et on le SIGNALE.
					Note("MediaBox absente (format par defaut applique)");
				}
				const double wPt = x1 - x0, hPt = y1 - y0;
				const double k = dpi / 72.0;
				const int32 rot = doc.PageRotate(pageIdx);
				const bool swap = (rot == 90 || rot == 270);

				int32 pw = static_cast<int32>((swap ? hPt : wPt) * k + 0.5);
				int32 ph = static_cast<int32>((swap ? wPt : hPt) * k + 0.5);
				if (pw < 1 || ph < 1)
					return false;
				// En mode FENETRE le canevas est fourni par l'appelant a la taille du
				// panneau : le recreer ruinerait tout l'interet (texture stable).
				if (!mWindowMode) {
					if (!out.Create(pw, ph))
						return false;
				} else {
					out.ClearClip();
					out.Clear(255, 255, 255, 255);
				}

				// Matrice de base : PDF a l'origine EN BAS a gauche et l'axe Y vers le
				// HAUT ; une image a l'origine en haut a gauche et l'axe Y vers le BAS.
				// D'ou le d negatif et la translation. La rotation de page s'y compose.
				NkPdfMat base;
				switch (rot) {
					case 90:
						base.a = 0; base.b = k; base.c = k; base.d = 0;
						base.e = -y0 * k; base.f = -x0 * k;
						break;
					case 180:
						base.a = -k; base.b = 0; base.c = 0; base.d = k;
						base.e = x1 * k; base.f = -y0 * k;
						break;
					case 270:
						base.a = 0; base.b = -k; base.c = -k; base.d = 0;
						base.e = y1 * k; base.f = x1 * k;
						break;
					default:
						base.a = k; base.b = 0; base.c = 0; base.d = -k;
						base.e = -x0 * k; base.f = y1 * k;
						break;
				}
				if (mWindowMode) { // decale la page pour amener la fenetre a l'origine
					base.e -= mWinOffX;
					base.f -= mWinOffY;
				}
				mGs = GState();
				mGs.ctm = base;

				// Le contenu peut etre UN flux ou un TABLEAU de flux, a concatener —
				// des generateurs coupent une page en plusieurs morceaux, et un
				// operateur peut etre a cheval sur deux d'entre eux.
				NkVector<uint8> content;
				const NkPdfVal cs = doc.DictGet(page, "Contents");
				if (cs.kind == NK_PDF_STREAM) {
					doc.DecodeStream(cs, content);
				} else if (cs.kind == NK_PDF_ARRAY) {
					for (int32 i = 0; i < cs.b; ++i) {
						NkVector<uint8> part;
						if (doc.DecodeStream(doc.ArrayAt(cs, i), part)) {
							for (usize j = 0; j < part.Size(); ++j)
								content.PushBack(part[j]);
							content.PushBack('\n'); // separateur : ne pas coller deux jetons
						}
					}
				}
				mStats.contentBytes = static_cast<int32>(content.Size());
				if (content.Empty())
					return true; // page vide : ce n'est pas une erreur

				NkPdfVal res = doc.DictGet(page, "Resources");
				if (!res.IsDictLike()) {
					// /Resources est HERITABLE : sans cette remontee, une page qui la
					// tient de son parent perdrait toutes ses polices et images.
					NkPdfVal node = page;
					for (int32 hop = 0; hop < 32 && node.IsDictLike(); ++hop) {
						node = doc.DictGet(node, "Parent");
						res = doc.DictGet(node, "Resources");
						if (res.IsDictLike())
							break;
					}
				}
				Run(content, res, 0);
				return true;
			}

			// ============================================================
			// Boucle d'interpretation
			// ============================================================

			void NkPdfRenderer::Run(const NkVector<uint8> &content, const NkPdfVal &resources, int32 depth) {
				if (depth > 12) { // formulaires imbriques : borne dure
					Note("imbrication de formulaires trop profonde");
					return;
				}
				NkPdfDoc &doc = *mDoc;
				Lex lex(content);

				// Pile d'operandes. Bornee : un flux malforme peut empiler sans fin.
				double num[32];
				int32 nNum = 0;
				Tok lastName;
				lastName.kind = Tok::End;
				NkVector<uint8> strBuf;

				// Tableau en cours (operateur TJ).
				bool inArr = false;
				NkVector<double> arrNums;
				NkVector<int32> arrIsStr; // 1 = element chaine, 0 = nombre
				NkVector<NkVector<uint8>> arrStrs;

				auto pushNum = [&](double v) {
					if (nNum < 32)
						num[nNum++] = v;
				};
				auto clear = [&]() {
					nNum = 0;
					lastName.kind = Tok::End;
				};

				for (;;) {
					Tok t = lex.Next();
					if (t.kind == Tok::End)
						break;

					if (t.kind == Tok::Op)
						++mStats.ops;

					if (t.kind == Tok::Num) {
						if (inArr) {
							arrNums.PushBack(t.num);
							arrIsStr.PushBack(0);
							arrStrs.PushBack(NkVector<uint8>());
						} else
							pushNum(t.num);
						continue;
					}
					if (t.kind == Tok::Name) {
						lastName = t;
						continue;
					}
					if (t.kind == Tok::ArrBeg) {
						inArr = true;
						arrNums.Clear();
						arrIsStr.Clear();
						arrStrs.Clear();
						continue;
					}
					if (t.kind == Tok::ArrEnd) {
						inArr = false;
						continue;
					}
					if (t.kind == Tok::Str) {
						NkVector<uint8> s;
						DecodeStr(t.ptr, t.len, t.num > 0.5, s);
						if (inArr) {
							arrNums.PushBack(0.0);
							arrIsStr.PushBack(1);
							arrStrs.PushBack(s);
						} else
							strBuf = s;
						continue;
					}

					// ── Operateurs ──

					// Etat graphique
					if (OpIs(t, "q")) {
						if (mStack.Size() < 64) {
							mGs.clip = mCv->TakeClip();
							mStack.PushBack(mGs);
						}
						clear();
						continue;
					}
					if (OpIs(t, "Q")) {
						if (!mStack.Empty()) {
							mGs = mStack[mStack.Size() - 1];
							mStack.Erase(mStack.Begin() + (mStack.Size() - 1));
							mCv->RestoreClip(mGs.clip);
						}
						clear();
						continue;
					}
					if (OpIs(t, "cm") && nNum >= 6) {
						NkPdfMat m;
						m.a = num[0]; m.b = num[1]; m.c = num[2];
						m.d = num[3]; m.e = num[4]; m.f = num[5];
						mGs.ctm = NkPdfMat::Mul(m, mGs.ctm);
						clear();
						continue;
					}
					if (OpIs(t, "w") && nNum >= 1) {
						mGs.lineWidth = num[0];
						clear();
						continue;
					}
					if (OpIs(t, "gs")) {
						// /ExtGState : on ne lit que l'opacite, la plus visible.
						if (lastName.kind == Tok::Name) {
							NkString nm;
							for (int32 i = 0; i < lastName.len; ++i)
								nm += static_cast<char>(lastName.ptr[i]);
							const NkPdfVal eg = doc.DictGet(doc.DictGet(resources, "ExtGState"), nm.CStr());
							const NkPdfVal ca = doc.DictGet(eg, "ca");
							if (ca.IsNum())
								mGs.fillAlpha = ca.num;
							const NkPdfVal CA = doc.DictGet(eg, "CA");
							if (CA.IsNum())
								mGs.strokeAlpha = CA.num;
							if (!doc.DictGet(eg, "SMask").IsNull())
								Note("masque de transparence");
						}
						clear();
						continue;
					}

					// Construction de trace : les points sont transformes ICI, une fois
					// pour toutes, en pixels — le rastériseur ignore tout des matrices.
					if (OpIs(t, "m") && nNum >= 2) {
						double X, Y;
						mGs.ctm.Apply(num[0], num[1], &X, &Y);
						mPath.MoveTo(X, Y);
						clear();
						continue;
					}
					if (OpIs(t, "l") && nNum >= 2) {
						double X, Y;
						mGs.ctm.Apply(num[0], num[1], &X, &Y);
						mPath.LineTo(X, Y);
						clear();
						continue;
					}
					if (OpIs(t, "c") && nNum >= 6) {
						double a1, b1, a2, b2, a3, b3;
						mGs.ctm.Apply(num[0], num[1], &a1, &b1);
						mGs.ctm.Apply(num[2], num[3], &a2, &b2);
						mGs.ctm.Apply(num[4], num[5], &a3, &b3);
						mPath.CurveTo(a1, b1, a2, b2, a3, b3);
						clear();
						continue;
					}
					if (OpIs(t, "v") && nNum >= 4) { // 1er controle = point courant
						double a2, b2, a3, b3;
						mGs.ctm.Apply(num[0], num[1], &a2, &b2);
						mGs.ctm.Apply(num[2], num[3], &a3, &b3);
						mPath.CurveTo(a2, b2, a2, b2, a3, b3);
						clear();
						continue;
					}
					if (OpIs(t, "y") && nNum >= 4) { // 2e controle = point final
						double a1, b1, a3, b3;
						mGs.ctm.Apply(num[0], num[1], &a1, &b1);
						mGs.ctm.Apply(num[2], num[3], &a3, &b3);
						mPath.CurveTo(a1, b1, a3, b3, a3, b3);
						clear();
						continue;
					}
					if (OpIs(t, "h")) {
						mPath.Close();
						clear();
						continue;
					}
					if (OpIs(t, "re") && nNum >= 4) {
						// Les 4 coins sont transformes individuellement : sous une
						// matrice qui tourne, un `re` n'est plus un rectangle aligne.
						double px[4], py[4];
						mGs.ctm.Apply(num[0], num[1], &px[0], &py[0]);
						mGs.ctm.Apply(num[0] + num[2], num[1], &px[1], &py[1]);
						mGs.ctm.Apply(num[0] + num[2], num[1] + num[3], &px[2], &py[2]);
						mGs.ctm.Apply(num[0], num[1] + num[3], &px[3], &py[3]);
						mPath.MoveTo(px[0], py[0]);
						mPath.LineTo(px[1], py[1]);
						mPath.LineTo(px[2], py[2]);
						mPath.LineTo(px[3], py[3]);
						mPath.Close();
						clear();
						continue;
					}

					// Decoupage : W/W* n'agit qu'a la PROCHAINE peinture (§8.5.4).
					if (OpIs(t, "W") || OpIs(t, "W*")) {
						mPendingClip = true;
						mPendingClipEO = OpIs(t, "W*");
						clear();
						continue;
					}

					// Peinture
					{
						const bool fillNz = OpIs(t, "f") || OpIs(t, "F");
						const bool fillEo = OpIs(t, "f*");
						const bool strokeOnly = OpIs(t, "S");
						const bool closeStroke = OpIs(t, "s");
						const bool fillStrokeNz = OpIs(t, "B") || OpIs(t, "b");
						const bool fillStrokeEo = OpIs(t, "B*") || OpIs(t, "b*");
						const bool noPaint = OpIs(t, "n");
						if (fillNz || fillEo || strokeOnly || closeStroke || fillStrokeNz || fillStrokeEo ||
							noPaint) {
							if (closeStroke || OpIs(t, "b") || OpIs(t, "b*"))
								mPath.Close();
							if ((fillNz || fillEo || fillStrokeNz || fillStrokeEo) && !mGs.fillIsPattern) {
								++mStats.fills;
								mCv->FillPath(mPath, fillEo || fillStrokeEo, ToByte(mGs.fill[0]),
											  ToByte(mGs.fill[1]), ToByte(mGs.fill[2]),
											  ToByte(mGs.fillAlpha));
							}
							if ((strokeOnly || closeStroke || fillStrokeNz || fillStrokeEo) &&
								!mGs.strokeIsPattern)
								mCv->StrokePath(mPath, mGs.lineWidth * mGs.ctm.Scale(),
												ToByte(mGs.stroke[0]), ToByte(mGs.stroke[1]),
												ToByte(mGs.stroke[2]), ToByte(mGs.strokeAlpha));
							if (mPendingClip) {
								mCv->SetClipFromPath(mPath, mPendingClipEO);
								mPendingClip = false;
							}
							mPath.Clear();
							clear();
							continue;
						}
					}

					// Couleurs
					if ((OpIs(t, "g") || OpIs(t, "G")) && nNum >= 1) {
						const bool isF = OpIs(t, "g");
						double *c = isF ? mGs.fill : mGs.stroke;
						(isF ? mGs.fillIsPattern : mGs.strokeIsPattern) = false;
						c[0] = c[1] = c[2] = num[0];
						clear();
						continue;
					}
					if ((OpIs(t, "rg") || OpIs(t, "RG")) && nNum >= 3) {
						const bool isF = OpIs(t, "rg");
						double *c = isF ? mGs.fill : mGs.stroke;
						(isF ? mGs.fillIsPattern : mGs.strokeIsPattern) = false;
						c[0] = num[0]; c[1] = num[1]; c[2] = num[2];
						clear();
						continue;
					}
					if ((OpIs(t, "k") || OpIs(t, "K")) && nNum >= 4) {
						// CMJN -> RVB, conversion simple : suffisante pour de la lecture,
						// une gestion colorimetrique complete serait hors sujet ici.
						const bool isF = OpIs(t, "k");
						double *c = isF ? mGs.fill : mGs.stroke;
						(isF ? mGs.fillIsPattern : mGs.strokeIsPattern) = false;
						c[0] = (1.0 - num[0]) * (1.0 - num[3]);
						c[1] = (1.0 - num[1]) * (1.0 - num[3]);
						c[2] = (1.0 - num[2]) * (1.0 - num[3]);
						clear();
						continue;
					}
					if (OpIs(t, "sc") || OpIs(t, "scn") || OpIs(t, "SC") || OpIs(t, "SCN")) {
						const bool isFill = (t.ptr[0] == 's');
						double *c = isFill ? mGs.fill : mGs.stroke;
						if (nNum >= 3) {
							c[0] = num[0]; c[1] = num[1]; c[2] = num[2];
						} else if (nNum == 1) {
							c[0] = c[1] = c[2] = num[0];
						} else if (nNum == 4) {
							c[0] = (1.0 - num[0]) * (1.0 - num[3]);
							c[1] = (1.0 - num[1]) * (1.0 - num[3]);
							c[2] = (1.0 - num[2]) * (1.0 - num[3]);
						} else if (lastName.kind == Tok::Name) {
							// Motif : on NE PEINT PAS. Voir GState::fillIsPattern.
							if (isFill)
								mGs.fillIsPattern = true;
							else
								mGs.strokeIsPattern = true;
							Note("motif (non peint)");
						}
						if (nNum >= 1) { // couleur explicite : ce n'est plus un motif
							if (isFill)
								mGs.fillIsPattern = false;
							else
								mGs.strokeIsPattern = false;
						}
						clear();
						continue;
					}
					if (OpIs(t, "cs") || OpIs(t, "CS")) {
						clear(); // l'espace est deduit du nombre de composantes
						continue;
					}
					if (OpIs(t, "sh")) {
						Note("degrade");
						clear();
						continue;
					}

					// Texte
					if (OpIs(t, "BT")) {
						mTm = NkPdfMat();
						mTlm = mTm;
						clear();
						continue;
					}
					if (OpIs(t, "ET")) {
						clear();
						continue;
					}
					if (OpIs(t, "Tf") && nNum >= 1) {
						mGs.fontSize = num[0];
						if (lastName.kind == Tok::Name)
							mGs.font = mFonts.Get(doc, resources, reinterpret_cast<const char *>(lastName.ptr),
												  lastName.len);
						if (mGs.font)
							mLastFont = mGs.font;
						else
							Note("police non embarquee (texte omis)");
						clear();
						continue;
					}
					if (OpIs(t, "Td") && nNum >= 2) {
						NkPdfMat m;
						m.e = num[0];
						m.f = num[1];
						mTlm = NkPdfMat::Mul(m, mTlm);
						mTm = mTlm;
						clear();
						continue;
					}
					if (OpIs(t, "TD") && nNum >= 2) {
						mGs.leading = -num[1];
						NkPdfMat m;
						m.e = num[0];
						m.f = num[1];
						mTlm = NkPdfMat::Mul(m, mTlm);
						mTm = mTlm;
						clear();
						continue;
					}
					if (OpIs(t, "Tm") && nNum >= 6) {
						mTlm.a = num[0]; mTlm.b = num[1]; mTlm.c = num[2];
						mTlm.d = num[3]; mTlm.e = num[4]; mTlm.f = num[5];
						mTm = mTlm;
						clear();
						continue;
					}
					if (OpIs(t, "T*")) {
						NkPdfMat m;
						m.f = -mGs.leading;
						mTlm = NkPdfMat::Mul(m, mTlm);
						mTm = mTlm;
						clear();
						continue;
					}
					if (OpIs(t, "TL") && nNum >= 1) { mGs.leading = num[0]; clear(); continue; }
					if (OpIs(t, "Tc") && nNum >= 1) { mGs.charSpace = num[0]; clear(); continue; }
					if (OpIs(t, "Tw") && nNum >= 1) { mGs.wordSpace = num[0]; clear(); continue; }
					if (OpIs(t, "Tz") && nNum >= 1) { mGs.hscale = num[0] / 100.0; clear(); continue; }
					if (OpIs(t, "Ts") && nNum >= 1) { mGs.rise = num[0]; clear(); continue; }
					if (OpIs(t, "Tr") && nNum >= 1) { mGs.render = static_cast<int32>(num[0]); clear(); continue; }

					// Ecriture effective
					{
						const bool tj = OpIs(t, "Tj");
						const bool bigTJ = OpIs(t, "TJ");
						const bool quote = OpIs(t, "'");
						const bool dquote = OpIs(t, "\"");
						if (tj || bigTJ || quote || dquote) {
							++mStats.textOps;
							if (quote || dquote) { // passage a la ligne implicite
								if (dquote && nNum >= 2) {
									mGs.wordSpace = num[0];
									mGs.charSpace = num[1];
								}
								NkPdfMat m;
								m.f = -mGs.leading;
								mTlm = NkPdfMat::Mul(m, mTlm);
								mTm = mTlm;
							}
							// Tr 3 = texte INVISIBLE : c'est le calque de reconnaissance
							// des documents scannes. Le peindre barbouillerait la page.
							const bool visible = (mGs.render != 3) && (mGs.render != 7);
							NkPdfPath glyphs;
							NkPdfFont *f = mGs.font;

							auto emit = [&](const NkVector<uint8> &s) {
								if (!f)
									return;
								mStats.strBytes += static_cast<int32>(s.Size());
								for (int32 i = 0; i < static_cast<int32>(s.Size());) {
									uint32 code = 0;
									const int32 used = f->NextCode(s.Data(), static_cast<int32>(s.Size()), i,
																   &code);
									if (used <= 0)
										break;
									i += used;
									const double w0 = f->Advance(code) / 1000.0;
									if (visible) {
										++mStats.glyphsAsked;
										if (mStats.nFirstCodes < 8)
											mStats.firstCodes[mStats.nFirstCodes++] = code;
										// Matrice de rendu du glyphe = Tm * CTM, avec la
										// taille et l'echelle horizontale.
										NkPdfMat trm = NkPdfMat::Mul(mTm, mGs.ctm);
										double ox, oy;
										trm.Apply(0.0, mGs.rise, &ox, &oy);
										// Echelle et cisaillement issus de la matrice
										// combinee : le texte suit les rotations.
										const double sx = mGs.fontSize * mGs.hscale;
										double ex, ey, uy;
										trm.Apply(sx, mGs.rise, &ex, &ey);
										trm.Apply(0.0, mGs.rise + mGs.fontSize, &uy, &oy);
										(void)uy;
										// Approximation assumee : on ne gere pas la
										// rotation arbitraire du texte, seulement
										// l'echelle et le retournement vertical.
										const double gscale = mGs.fontSize * Abs2(trm.a) * mGs.hscale;
										const double vs = (trm.d < 0) ? -1.0 : 1.0;
										double gx, gy;
										trm.Apply(0.0, mGs.rise, &gx, &gy);
										if (f->AppendGlyph(code, gx, gy, gscale, 0.0, vs, glyphs))
											++mStats.glyphsGot;
										(void)ex;
										(void)ey;
									}
									// Avance : largeur + inter-lettre (+ inter-mot pour
									// l'espace, code 32 des polices simples uniquement).
									double adv = w0 * mGs.fontSize + mGs.charSpace;
									if (code == 32 && !f->IsTwoByte())
										adv += mGs.wordSpace;
									NkPdfMat m;
									m.e = adv * mGs.hscale;
									mTm = NkPdfMat::Mul(m, mTm);
								}
							};

							if (bigTJ) {
								for (usize i = 0; i < arrIsStr.Size(); ++i) {
									if (arrIsStr[i])
										emit(arrStrs[i]);
									else {
										// Nombre : deplacement en 1/1000 d'em, SOUSTRAIT.
										NkPdfMat m;
										m.e = -arrNums[i] / 1000.0 * mGs.fontSize * mGs.hscale;
										mTm = NkPdfMat::Mul(m, mTm);
									}
								}
							} else
								emit(strBuf);

							if (visible && !glyphs.Empty())
								mCv->FillPath(glyphs, false, ToByte(mGs.fill[0]), ToByte(mGs.fill[1]),
											  ToByte(mGs.fill[2]), ToByte(mGs.fillAlpha));
							clear();
							continue;
						}
					}

					// Objets externes : images et formulaires
					if (OpIs(t, "Do")) {
						if (lastName.kind == Tok::Name)
							DoXObject(reinterpret_cast<const char *>(lastName.ptr), lastName.len, resources,
									  depth);
						clear();
						continue;
					}
					if (OpIs(t, "BI")) {
						// Image EN LIGNE : on saute jusqu'a « EI ». Les rendre demanderait
						// un second chemin de decodage pour un gain marginal.
						Note("image en ligne");
						usize p = lex.Pos();
						while (p + 1 < content.Size() &&
							   !(content[p] == 'E' && content[p + 1] == 'I' &&
								 (p + 2 >= content.Size() || IsWsC(content[p + 2]))))
							++p;
						lex.SetPos(p + 2 < content.Size() ? p + 2 : content.Size());
						clear();
						continue;
					}
					if (OpIs(t, "d0") || OpIs(t, "d1")) {
						Note("police de type 3");
						clear();
						continue;
					}

					clear(); // operateur non gere : on ignore ses operandes
				}
			}

			// ============================================================
			// XObjects
			// ============================================================

			void NkPdfRenderer::DoXObject(const char *name, int32 nameLen, const NkPdfVal &resources,
										  int32 depth) {
				NkPdfDoc &doc = *mDoc;
				NkString nm;
				for (int32 i = 0; i < nameLen; ++i)
					nm += name[i];
				const NkPdfVal xo = doc.DictGet(doc.DictGet(resources, "XObject"), nm.CStr());
				if (xo.kind != NK_PDF_STREAM)
					return;
				const NkPdfVal sub = doc.DictGet(xo, "Subtype");

				if (doc.NameIs(sub, "Image")) {
					++mStats.images;
					DrawImage(xo, depth);
					return;
				}
				if (doc.NameIs(sub, "Form")) {
					++mStats.forms;
					// Formulaire : contenu imbrique, avec sa propre matrice et ses
					// propres ressources. On sauvegarde l'etat comme le ferait un q/Q.
					NkVector<uint8> sc;
					if (!doc.DecodeStream(xo, sc))
						return;
					const GState saved = mGs;
					NkPdfPath savedPath = mPath;
					mPath.Clear();
					const NkPdfVal mtx = doc.DictGet(xo, "Matrix");
					if (mtx.kind == NK_PDF_ARRAY && mtx.b >= 6) {
						NkPdfMat m;
						m.a = doc.Num(doc.ArrayAt(mtx, 0), 1);
						m.b = doc.Num(doc.ArrayAt(mtx, 1), 0);
						m.c = doc.Num(doc.ArrayAt(mtx, 2), 0);
						m.d = doc.Num(doc.ArrayAt(mtx, 3), 1);
						m.e = doc.Num(doc.ArrayAt(mtx, 4), 0);
						m.f = doc.Num(doc.ArrayAt(mtx, 5), 0);
						mGs.ctm = NkPdfMat::Mul(m, mGs.ctm);
					}
					NkPdfVal r2 = doc.DictGet(xo, "Resources");
					if (!r2.IsDictLike())
						r2 = resources; // heritees du parent
					Run(sc, r2, depth + 1);
					mGs = saved;
					mPath = savedPath;
				}
			}

			// Dessine une image dans le carre unite transforme par la matrice courante.
			void NkPdfRenderer::DrawImage(const NkPdfVal &img, int32 depth) {
				(void)depth;
				NkPdfDoc &doc = *mDoc;
				const int32 iw = static_cast<int32>(doc.Num(doc.DictGet(img, "Width"), 0));
				const int32 ih = static_cast<int32>(doc.Num(doc.DictGet(img, "Height"), 0));
				if (iw <= 0 || ih <= 0)
					return;

				NkVector<uint8> data;
				if (!doc.DecodeStream(img, data) || data.Empty()) {
					Note("image illisible");
					return;
				}

				// Le filtre d'image (DCT/JPX/CCITT/JBIG2) n'est PAS decode par
				// DecodeStream : il rend les octets tels quels. On ne gere ici que le
				// cas brut ; le JPEG demanderait de brancher le codec NKImage, ce qui
				// suppose une image en memoire — signale plutot que rendu faux.
				const NkPdfVal filt = doc.DictGet(img, "Filter");
				bool raw = filt.IsNull();
				if (filt.kind == NK_PDF_NAME)
					raw = !(doc.NameIs(filt, "DCTDecode") || doc.NameIs(filt, "JPXDecode") ||
							doc.NameIs(filt, "CCITTFaxDecode") || doc.NameIs(filt, "JBIG2Decode"));
				else if (filt.kind == NK_PDF_ARRAY) {
					raw = true;
					for (int32 i = 0; i < filt.b; ++i) {
						const NkPdfVal f = doc.ArrayAt(filt, i);
						if (doc.NameIs(f, "DCTDecode") || doc.NameIs(f, "JPXDecode") ||
							doc.NameIs(f, "CCITTFaxDecode") || doc.NameIs(f, "JBIG2Decode"))
							raw = false;
					}
				}
				// JPEG : 24 % du corpus, et un document SCANNE est une page entiere
				// en JPEG — sans lui, ces pages sortent BLANCHES. On delegue au codec
				// de NKImage, qui sait decoder depuis la memoire. Le resultat est
				// normalise en RGBA (4 canaux) pour n'avoir qu'un seul chemin ensuite.
				NkImage decoded;
				bool viaCodec = false;
				if (!raw) {
					const bool isJpeg =
						(filt.kind == NK_PDF_NAME && doc.NameIs(filt, "DCTDecode")) ||
						(filt.kind == NK_PDF_ARRAY && [&] {
							for (int32 i = 0; i < filt.b; ++i)
								if (doc.NameIs(doc.ArrayAt(filt, i), "DCTDecode"))
									return true;
							return false;
						}());
					if (isJpeg && decoded.LoadFromMemory(data.Data(), data.Size(), 4) &&
						decoded.Width() > 0 && decoded.Height() > 0) {
						viaCodec = true;
					} else {
						Note(isJpeg ? "image JPEG illisible" : "image compressee (JPX/CCITT/JBIG2)");
						return;
					}
				}

				const int32 bpc = static_cast<int32>(doc.Num(doc.DictGet(img, "BitsPerComponent"), 8));
				const NkPdfVal csv = doc.DictGet(img, "ColorSpace");
				int32 comps = 1;
				if (doc.NameIs(csv, "DeviceRGB"))
					comps = 3;
				else if (doc.NameIs(csv, "DeviceCMYK"))
					comps = 4;
				else if (doc.NameIs(csv, "DeviceGray"))
					comps = 1;
				else if (csv.kind == NK_PDF_ARRAY)
					comps = 1; // espace indexe : traite en niveaux de gris, approximatif

				const bool mask = doc.Num(doc.DictGet(img, "ImageMask"), 0) != 0.0;
				if (mask)
					comps = 1;

				// Echantillonnage INVERSE : pour chaque pixel de destination on
				// remonte a la source. C'est ce qui gere correctement une image
				// tournee ou etiree, contrairement a un parcours de la source.
				NkPdfMat m = mGs.ctm;
				const double det = m.a * m.d - m.b * m.c;
				if (Abs2(det) < 1e-12)
					return;
				const double ia = m.d / det, ib = -m.b / det, ic = -m.c / det, id = m.a / det;
				const double ie = -(m.e * ia + m.f * ic), iff = -(m.e * ib + m.f * id);

				// Boite englobante du carre unite transforme.
				double bx0 = 1e30, by0 = 1e30, bx1 = -1e30, by1 = -1e30;
				const double cx[4] = {0, 1, 1, 0}, cy[4] = {0, 0, 1, 1};
				for (int32 i = 0; i < 4; ++i) {
					double X, Y;
					m.Apply(cx[i], cy[i], &X, &Y);
					if (X < bx0) bx0 = X;
					if (Y < by0) by0 = Y;
					if (X > bx1) bx1 = X;
					if (Y > by1) by1 = Y;
				}
				int32 px0 = static_cast<int32>(bx0), py0 = static_cast<int32>(by0);
				int32 px1 = static_cast<int32>(bx1) + 1, py1 = static_cast<int32>(by1) + 1;
				if (px0 < 0) px0 = 0;
				if (py0 < 0) py0 = 0;
				if (px1 > mCv->Width()) px1 = mCv->Width();
				if (py1 > mCv->Height()) py1 = mCv->Height();

				const usize rowBytes = (static_cast<usize>(iw) * static_cast<usize>(comps) *
										static_cast<usize>(bpc) + 7u) / 8u;
				uint8 *dst = mCv->Pixels();
				const uint8 fr = ToByte(mGs.fill[0]), fg = ToByte(mGs.fill[1]), fb = ToByte(mGs.fill[2]);
				const uint32 ga = static_cast<uint32>(ToByte(mGs.fillAlpha));

				for (int32 y = py0; y < py1; ++y) {
					for (int32 x = px0; x < px1; ++x) {
						const double dx = static_cast<double>(x) + 0.5;
						const double dy = static_cast<double>(y) + 0.5;
						const double u = dx * ia + dy * ic + ie;
						const double v = dx * ib + dy * id + iff;
						if (u < 0.0 || u >= 1.0 || v < 0.0 || v >= 1.0)
							continue;
						const int32 sx = static_cast<int32>(u * iw);
						// L'axe v du PDF monte, les lignes de l'image descendent.
						const int32 sy = static_cast<int32>((1.0 - v) * ih);
						if (sx < 0 || sx >= iw || sy < 0 || sy >= ih)
							continue;

						uint8 r = 0, g = 0, b = 0;
						uint32 a = ga;
						if (viaCodec) {
							// Le codec a normalise en RGBA : un seul cas a traiter, et
							// les dimensions viennent de LUI, pas du dictionnaire (les
							// deux peuvent differer sur un fichier abime).
							const int32 cw = decoded.Width(), ch = decoded.Height();
							const int32 cx2 = static_cast<int32>(u * cw);
							const int32 cy2 = static_cast<int32>((1.0 - v) * ch);
							if (cx2 < 0 || cx2 >= cw || cy2 < 0 || cy2 >= ch)
								continue;
							const uint8 *sp = decoded.Pixels() +
											  static_cast<usize>(cy2) * static_cast<usize>(decoded.Stride()) +
											  static_cast<usize>(cx2) * 4u;
							r = sp[0];
							g = sp[1];
							b = sp[2];
							if (sp[3] < 255)
								a = (a * sp[3]) / 255u;
						} else if (bpc == 8) {
							const usize off = static_cast<usize>(sy) * rowBytes +
											  static_cast<usize>(sx) * static_cast<usize>(comps);
							if (off + static_cast<usize>(comps) > data.Size())
								continue;
							if (comps == 1) {
								r = g = b = data[off];
							} else if (comps == 3) {
								r = data[off]; g = data[off + 1]; b = data[off + 2];
							} else {
								const double c0 = data[off] / 255.0, m0 = data[off + 1] / 255.0;
								const double y0 = data[off + 2] / 255.0, k0 = data[off + 3] / 255.0;
								r = ToByte((1 - c0) * (1 - k0));
								g = ToByte((1 - m0) * (1 - k0));
								b = ToByte((1 - y0) * (1 - k0));
							}
						} else if (bpc == 1) {
							const usize bit = static_cast<usize>(sx);
							const usize off = static_cast<usize>(sy) * rowBytes + bit / 8u;
							if (off >= data.Size())
								continue;
							const bool on = (data[off] >> (7u - (bit % 8u))) & 1u;
							if (mask) {
								// Un masque peint la COULEUR courante la ou le bit est 0.
								if (on)
									continue;
								r = fr; g = fg; b = fb;
							} else {
								r = g = b = on ? 255 : 0;
							}
						} else {
							continue; // 2/4/16 bits : rares, non geres
						}

						uint8 *p = dst + (static_cast<usize>(y) * static_cast<usize>(mCv->Width()) +
										  static_cast<usize>(x)) * 4u;
						p[0] = static_cast<uint8>((r * a + p[0] * (255u - a)) / 255u);
						p[1] = static_cast<uint8>((g * a + p[1] * (255u - a)) / 255u);
						p[2] = static_cast<uint8>((b * a + p[2] * (255u - a)) / 255u);
						p[3] = 255;
					}
				}
			}

		} // namespace pdf
	} // namespace nkcode
} // namespace nkentseu
