//
// NkPdfType1.cpp — voir NkPdfType1.h.
//
#include "NKMedia/Pdf/NkPdfType1.h"
#include <cstdlib>

namespace nkentseu {
	namespace media {
		namespace pdf {

			namespace {

				/// Contre-epreuve du banc (NK_PDF_MUTATION=<nom>) ; sans la variable, toujours faux.
				bool T1MutationProg(const char *nom) {
					const char *v = std::getenv("NK_PDF_MUTATION");
					if (!v || !nom)
						return false;
					int32 i = 0;
					while (v[i] && nom[i] && v[i] == nom[i])
						++i;
					return v[i] == 0 && nom[i] == 0;
				}

				inline bool T1Blanc(uint8 c) {
					return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == 0;
				}
				inline bool T1Delim(uint8 c) {
					return c == '/' || c == '[' || c == ']' || c == '{' || c == '}' || c == '(' || c == ')' || c == '<' || c == '>';
				}
				inline bool T1Hex(uint8 c) {
					return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
				}
				inline int32 T1HexVal(uint8 c) {
					return c <= '9' ? c - '0' : (c >= 'a' ? c - 'a' + 10 : c - 'A' + 10);
				}

				/// Le jeton suivant a partir de `i` (blancs et commentaires sautes). Rend la
				/// position du jeton et sa longueur ; un nom garde sa barre (« /Subrs »).
				bool T1Jeton(const uint8 *p, usize n, usize &i, usize &debut, usize &lon) {
					while (i < n) {
						if (T1Blanc(p[i]))
							++i;
						else if (p[i] == '%') {
							while (i < n && p[i] != '\n' && p[i] != '\r')
								++i;
						} else
							break;
					}
					if (i >= n)
						return false;
					debut = i;
					if (p[i] == '/') {
						++i;
						while (i < n && !T1Blanc(p[i]) && !T1Delim(p[i]))
							++i;
					} else if (T1Delim(p[i]))
						++i;
					else
						while (i < n && !T1Blanc(p[i]) && !T1Delim(p[i]))
							++i;
					lon = i - debut;
					return true;
				}
				inline bool T1Egal(const uint8 *p, usize debut, usize lon, const char *s) {
					usize k = 0;
					for (; s[k]; ++k)
						if (k >= lon || p[debut + k] != (uint8)s[k])
							return false;
					return k == lon;
				}
				/// Un nombre PostScript (entier ou reel) ; `ok` faux si ce n'en est pas un.
				double T1Nombre(const uint8 *p, usize debut, usize lon, bool &ok) {
					ok = false;
					usize i = debut, fin = debut + lon;
					bool neg = false;
					if (i < fin && (p[i] == '-' || p[i] == '+')) {
						neg = p[i] == '-';
						++i;
					}
					double v = 0.0, f = 0.1;
					bool chiffre = false, point = false;
					for (; i < fin; ++i) {
						const uint8 c = p[i];
						if (c >= '0' && c <= '9') {
							if (point) {
								v += (c - '0') * f;
								f *= 0.1;
							} else
								v = v * 10.0 + (c - '0');
							chiffre = true;
						} else if (c == '.' && !point)
							point = true;
						else if ((c == 'e' || c == 'E') && chiffre) {
							++i;
							bool en = false;
							if (i < fin && (p[i] == '-' || p[i] == '+')) {
								en = p[i] == '-';
								++i;
							}
							int32 e = 0;
							for (; i < fin && p[i] >= '0' && p[i] <= '9'; ++i)
								e = e * 10 + (p[i] - '0');
							double m = 1.0;
							for (int32 k = 0; k < e && k < 300; ++k)
								m *= 10.0;
							v = en ? v / m : v * m;
							break;
						} else
							return 0.0;
					}
					ok = chiffre;
					return neg ? -v : v;
				}
				/// Cherche le motif `s` (un nom ou un mot) dans [de, n) ; rend sa position ou n.
				usize T1Chercher(const uint8 *p, usize n, usize de, const char *s) {
					usize l = 0;
					while (s[l])
						++l;
					for (usize i = de; i + l <= n; ++i) {
						usize k = 0;
						while (k < l && p[i + k] == (uint8)s[k])
							++k;
						if (k == l && (i + l == n || T1Blanc(p[i + l]) || T1Delim(p[i + l])))
							return i;
					}
					return n;
				}

				/// Dechiffrement Type 1 (eexec : r = 55665 ; charstrings : r = 4330).
				void T1Dechiffrer(const uint8 *in, usize n, uint16 r, NkVector<uint8> &out, usize sauter) {
					out.Clear();
					if (n > sauter)
						out.Reserve(n - sauter);
					for (usize i = 0; i < n; ++i) {
						const uint8 c = in[i];
						const uint8 clair = (uint8)(c ^ (r >> 8));
						r = (uint16)((c + r) * 52845u + 22719u);
						if (i >= sauter)
							out.PushBack(clair);
					}
				}

				// StandardEncoding (ISO 32000-1, annexe D) : code -> nom de glyphe.
				struct T1Std {
						uint8 code;
						const char *nom;
				};
				static const T1Std kStandard[] = {
					{32, "space"}, {33, "exclam"}, {34, "quotedbl"}, {35, "numbersign"}, {36, "dollar"}, {37, "percent"}, {38, "ampersand"},
					{39, "quoteright"}, {40, "parenleft"}, {41, "parenright"}, {42, "asterisk"}, {43, "plus"}, {44, "comma"}, {45, "hyphen"},
					{46, "period"}, {47, "slash"}, {48, "zero"}, {49, "one"}, {50, "two"}, {51, "three"}, {52, "four"}, {53, "five"},
					{54, "six"}, {55, "seven"}, {56, "eight"}, {57, "nine"}, {58, "colon"}, {59, "semicolon"}, {60, "less"}, {61, "equal"},
					{62, "greater"}, {63, "question"}, {64, "at"}, {65, "A"}, {66, "B"}, {67, "C"}, {68, "D"}, {69, "E"}, {70, "F"}, {71, "G"},
					{72, "H"}, {73, "I"}, {74, "J"}, {75, "K"}, {76, "L"}, {77, "M"}, {78, "N"}, {79, "O"}, {80, "P"}, {81, "Q"}, {82, "R"},
					{83, "S"}, {84, "T"}, {85, "U"}, {86, "V"}, {87, "W"}, {88, "X"}, {89, "Y"}, {90, "Z"}, {91, "bracketleft"},
					{92, "backslash"}, {93, "bracketright"}, {94, "asciicircum"}, {95, "underscore"}, {96, "quoteleft"}, {97, "a"}, {98, "b"},
					{99, "c"}, {100, "d"}, {101, "e"}, {102, "f"}, {103, "g"}, {104, "h"}, {105, "i"}, {106, "j"}, {107, "k"}, {108, "l"},
					{109, "m"}, {110, "n"}, {111, "o"}, {112, "p"}, {113, "q"}, {114, "r"}, {115, "s"}, {116, "t"}, {117, "u"}, {118, "v"},
					{119, "w"}, {120, "x"}, {121, "y"}, {122, "z"}, {123, "braceleft"}, {124, "bar"}, {125, "braceright"}, {126, "asciitilde"},
					{161, "exclamdown"}, {162, "cent"}, {163, "sterling"}, {164, "fraction"}, {165, "yen"}, {166, "florin"}, {167, "section"},
					{168, "currency"}, {169, "quotesingle"}, {170, "quotedblleft"}, {171, "guillemotleft"}, {172, "guilsinglleft"},
					{173, "guilsinglright"}, {174, "fi"}, {175, "fl"}, {177, "endash"}, {178, "dagger"}, {179, "daggerdbl"},
					{180, "periodcentered"}, {182, "paragraph"}, {183, "bullet"}, {184, "quotesinglbase"}, {185, "quotedblbase"},
					{186, "quotedblright"}, {187, "guillemotright"}, {188, "ellipsis"}, {189, "perthousand"}, {191, "questiondown"},
					{193, "grave"}, {194, "acute"}, {195, "circumflex"}, {196, "tilde"}, {197, "macron"}, {198, "breve"}, {199, "dotaccent"},
					{200, "dieresis"}, {202, "ring"}, {203, "cedilla"}, {205, "hungarumlaut"}, {206, "ogonek"}, {207, "caron"},
					{208, "emdash"}, {225, "AE"}, {227, "ordfeminine"}, {232, "Lslash"}, {233, "Oslash"}, {234, "OE"},
					{235, "ordmasculine"}, {241, "ae"}, {245, "dotlessi"}, {248, "lslash"}, {249, "oslash"}, {250, "oe"},
					{251, "germandbls"},
				};

				/// L'etat de l'interpreteur de charstrings.
				struct T1Etat {
						double pile[32];
						int32 sp = 0;
						double ps[32];
						int32 psp = 0;
						double x = 0.0, y = 0.0;
						double dx = 0.0, dy = 0.0; // decalage (seac : l'accent)
						int32 segments = 0;
						bool flex = false;
						double fx[8], fy[8];
						int32 nf = 0;
						bool fin = false;
						double largeur = 0.0;
						bool seac = false;
						double asb = 0.0, adx = 0.0, ady = 0.0;
						int32 bchar = 0, achar = 0;
						NkVector<NkPdfT1Op> *ops = nullptr;

						void Pousser(double v) {
							if (sp < 32)
								pile[sp++] = v;
						}
						double Tirer() {
							return sp > 0 ? pile[--sp] : 0.0;
						}
						void Fermer() {
							if (segments > 0) {
								NkPdfT1Op o;
								o.type = 3;
								ops->PushBack(o);
							}
							segments = 0;
						}
						void Deplacer(double nx, double ny) {
							x = nx;
							y = ny;
							if (flex) { // le flex : on retient le point, rien n'est trace
								if (nf < 8) {
									fx[nf] = x;
									fy[nf] = y;
									++nf;
								}
								return;
							}
							Fermer();
							NkPdfT1Op o;
							o.type = 0;
							o.x2 = x + dx;
							o.y2 = y + dy;
							ops->PushBack(o);
						}
						void Ligne(double nx, double ny) {
							x = nx;
							y = ny;
							NkPdfT1Op o;
							o.type = 1;
							o.x2 = x + dx;
							o.y2 = y + dy;
							ops->PushBack(o);
							++segments;
						}
						void Courbe(double ax, double ay, double bx, double by, double cx, double cy) {
							NkPdfT1Op o;
							o.type = 2;
							o.x0 = ax + dx;
							o.y0 = ay + dy;
							o.x1 = bx + dx;
							o.y1 = by + dy;
							o.x2 = cx + dx;
							o.y2 = cy + dy;
							x = cx;
							y = cy;
							ops->PushBack(o);
							++segments;
						}
				};

			} // namespace

			const char *NkPdfType1::StandardName(uint32 code) {
				for (const T1Std &e : kStandard)
					if (e.code == code)
						return e.nom;
				return nullptr;
			}

			bool NkPdfType1::Load(const uint8 *p, usize n, usize longueur1) {
				mValide = false;
				mPrive.Clear();
				mGlyphes.Clear();
				mSubrDebut.Clear();
				mSubrLongueur.Clear();
				mEncodageStandard = false;
				for (int32 i = 0; i < 256; ++i)
					mEncodage[i].Clear();
				if (!p || n < 16)
					return false;
				// ── 1. la partie en CLAIR ──
				const usize eexec = T1Chercher(p, n, 0, "eexec");
				if (eexec >= n)
					return false;
				{ // /FontMatrix [a b c d e f]
					const usize fm = T1Chercher(p, eexec, 0, "/FontMatrix");
					if (fm < eexec) {
						usize i = fm + 11, d = 0, l = 0;
						double v[6];
						int32 k = 0;
						while (k < 6 && T1Jeton(p, eexec, i, d, l)) {
							bool ok = false;
							const double x = T1Nombre(p, d, l, ok);
							if (ok)
								v[k++] = x;
							else if (!(l == 1 && (p[d] == '[' || p[d] == '{')))
								break;
						}
						if (k == 6 && v[0] != 0.0)
							for (int32 j = 0; j < 6; ++j)
								mMatrice[j] = v[j];
					}
				}
				{ // /Encoding StandardEncoding, ou « dup N /nom put »
					const usize en = T1Chercher(p, eexec, 0, "/Encoding");
					if (en < eexec) {
						usize i = en + 9, d = 0, l = 0;
						if (T1Jeton(p, eexec, i, d, l) && T1Egal(p, d, l, "StandardEncoding"))
							mEncodageStandard = true;
						else {
							i = en + 9;
							while (T1Jeton(p, eexec, i, d, l)) {
								if (T1Egal(p, d, l, "readonly") || T1Egal(p, d, l, "def"))
									break;
								if (!T1Egal(p, d, l, "dup"))
									continue;
								usize d2 = 0, l2 = 0, d3 = 0, l3 = 0, d4 = 0, l4 = 0;
								const usize sauve = i;
								if (!T1Jeton(p, eexec, i, d2, l2) || !T1Jeton(p, eexec, i, d3, l3) || !T1Jeton(p, eexec, i, d4, l4)) {
									i = sauve;
									continue;
								}
								bool ok = false;
								const double c = T1Nombre(p, d2, l2, ok);
								if (ok && c >= 0.0 && c < 256.0 && l3 > 1 && p[d3] == '/' && T1Egal(p, d4, l4, "put"))
									mEncodage[(int32)c] = NkString((const char *)p + d3 + 1, (NkString::SizeType)(l3 - 1));
								else
									i = sauve;
							}
						}
					}
				}
				// ── 2. la partie chiffree (eexec), binaire ou hexadecimale ──
				usize debut = eexec + 5;
				while (debut < n && (p[debut] == ' ' || p[debut] == '\t' || p[debut] == '\r' || p[debut] == '\n'))
					++debut;
				if (longueur1 > eexec && longueur1 < n)
					debut = longueur1; // /Length1 fait foi quand il est coherent
				if (debut + 4 > n)
					return false;
				NkVector<uint8> chiffre;
				const uint16 cle = T1MutationProg("type1-eexec-faux") ? 55666u : 55665u; // contre-epreuve : la cle faussee
				const bool hexa = T1Hex(p[debut]) && T1Hex(p[debut + 1]) && T1Hex(p[debut + 2]) && T1Hex(p[debut + 3]);
				if (hexa) {
					int32 moitie = -1;
					for (usize i = debut; i < n; ++i) {
						if (T1Blanc(p[i]))
							continue;
						if (!T1Hex(p[i]))
							break;
						if (moitie < 0)
							moitie = T1HexVal(p[i]);
						else {
							chiffre.PushBack((uint8)((moitie << 4) | T1HexVal(p[i])));
							moitie = -1;
						}
					}
					T1Dechiffrer(chiffre.Data(), chiffre.Size(), cle, mPrive, 4);
				} else
					T1Dechiffrer(p + debut, n - debut, cle, mPrive, 4);
				const uint8 *q = mPrive.Data();
				const usize m = mPrive.Size();
				if (m < 16)
					return false;
				// ── 3. le dictionnaire prive ──
				const usize posSubrs = T1Chercher(q, m, 0, "/Subrs");
				const usize posCs = T1Chercher(q, m, 0, "/CharStrings");
				{
					// cherche AVANT les Subrs (rien de binaire encore) ; « absent » rend la borne
					const usize borne = posSubrs < posCs ? posSubrs : posCs;
					const usize li = T1Chercher(q, borne, 0, "/lenIV");
					if (li < borne) {
						usize i = li + 6, d = 0, l = 0;
						bool ok = false;
						if (T1Jeton(q, m, i, d, l)) {
							const double v = T1Nombre(q, d, l, ok);
							if (ok)
								mLenIV = (int32)v;
						}
					}
				}
				if (posSubrs < m) {
					usize i = posSubrs + 6, d = 0, l = 0;
					bool ok = false;
					if (T1Jeton(q, m, i, d, l)) {
						const double nb = T1Nombre(q, d, l, ok);
						const int32 total = ok && nb > 0.0 && nb < 65536.0 ? (int32)nb : 0;
						mSubrDebut.Resize((usize)total);
						mSubrLongueur.Resize((usize)total);
						for (int32 k = 0; k < total; ++k)
							mSubrDebut[(usize)k] = mSubrLongueur[(usize)k] = 0;
						int32 lus = 0;
						while (lus < total && T1Jeton(q, m, i, d, l)) {
							if (!T1Egal(q, d, l, "dup")) {
								if (T1Egal(q, d, l, "/CharStrings") || T1Egal(q, d, l, "end"))
									break;
								continue;
							}
							usize d2 = 0, l2 = 0, d3 = 0, l3 = 0, d4 = 0, l4 = 0;
							if (!T1Jeton(q, m, i, d2, l2) || !T1Jeton(q, m, i, d3, l3) || !T1Jeton(q, m, i, d4, l4))
								break;
							bool o1 = false, o2 = false;
							const double idx = T1Nombre(q, d2, l2, o1), lon = T1Nombre(q, d3, l3, o2);
							if (!o1 || !o2 || lon < 0.0)
								continue;
							const usize b = i + 1; // RD (ou -|), puis UN espace, puis les octets
							if (b + (usize)lon > m)
								break;
							if (idx >= 0.0 && (int32)idx < total) {
								mSubrDebut[(usize)idx] = b;
								mSubrLongueur[(usize)idx] = (usize)lon;
							}
							i = b + (usize)lon;
							++lus;
						}
					}
				}
				if (posCs >= m)
					return false;
				{
					usize i = posCs + 12, d = 0, l = 0;
					bool ok = false;
					if (!T1Jeton(q, m, i, d, l))
						return false;
					const double nb = T1Nombre(q, d, l, ok);
					const int32 total = ok && nb > 0.0 ? (int32)nb : 1 << 30;
					while ((int32)mGlyphes.Size() < total && T1Jeton(q, m, i, d, l)) {
						if (T1Egal(q, d, l, "end"))
							break;
						if (l < 2 || q[d] != '/')
							continue; // « dict », « dup », « begin », « ND », « |- »...
						usize d2 = 0, l2 = 0, d3 = 0, l3 = 0;
						if (!T1Jeton(q, m, i, d2, l2) || !T1Jeton(q, m, i, d3, l3))
							break;
						bool o = false;
						const double lon = T1Nombre(q, d2, l2, o);
						if (!o || lon < 0.0)
							continue;
						const usize b = i + 1;
						if (b + (usize)lon > m)
							break;
						Glyphe g;
						g.nom = NkString((const char *)q + d + 1, (NkString::SizeType)(l - 1));
						g.debut = b;
						g.longueur = (usize)lon;
						mGlyphes.PushBack(g);
						i = b + (usize)lon;
					}
				}
				mValide = !mGlyphes.Empty();
				return mValide;
			}

			const char *NkPdfType1::BuiltinName(uint32 code) const {
				if (code >= 256u)
					return nullptr;
				if (!mEncodage[code].Empty())
					return mEncodage[code].CStr();
				if (mEncodageStandard)
					return StandardName(code);
				return nullptr;
			}

			int32 NkPdfType1::GlyphIndex(const char *nom, int32 len) const {
				if (!nom || len <= 0)
					return -1;
				for (usize i = 0; i < mGlyphes.Size(); ++i) {
					const NkString &g = mGlyphes[i].nom;
					if ((int32)g.Length() != len)
						continue;
					int32 k = 0;
					while (k < len && g.CStr()[k] == nom[k])
						++k;
					if (k == len)
						return (int32)i;
				}
				return -1;
			}

			namespace {
				/// Execute un charstring (deja dechiffre). `prof` : profondeur des callsubr.
				bool T1Executer(const uint8 *cs, usize n, T1Etat &E, const NkVector<uint8> &prive, const NkVector<usize> &subrDebut,
								const NkVector<usize> &subrLon, int32 lenIV, int32 prof) {
					if (prof > 12)
						return false;
					usize i = 0;
					while (i < n && !E.fin) {
						const uint8 v = cs[i++];
						if (v >= 32) { // un nombre
							if (v <= 246)
								E.Pousser((double)((int32)v - 139));
							else if (v <= 250) {
								if (i >= n)
									return false;
								E.Pousser((double)(((int32)v - 247) * 256 + (int32)cs[i++] + 108));
							} else if (v <= 254) {
								if (i >= n)
									return false;
								E.Pousser((double)(-((int32)v - 251) * 256 - (int32)cs[i++] - 108));
							} else {
								if (i + 4 > n)
									return false;
								const int32 w = (int32)(((uint32)cs[i] << 24) | ((uint32)cs[i + 1] << 16) | ((uint32)cs[i + 2] << 8) | (uint32)cs[i + 3]);
								i += 4;
								E.Pousser((double)w);
							}
							continue;
						}
						switch (v) {
							case 1:	 // hstem
							case 3:	 // vstem
								E.sp = 0;
								break;
							case 4: { // vmoveto
								const double dy = E.Tirer();
								E.sp = 0;
								E.Deplacer(E.x, E.y + dy);
								break;
							}
							case 5: { // rlineto
								const double dy = E.Tirer(), dx = E.Tirer();
								E.sp = 0;
								E.Ligne(E.x + dx, E.y + dy);
								break;
							}
							case 6: { // hlineto
								const double dx = E.Tirer();
								E.sp = 0;
								E.Ligne(E.x + dx, E.y);
								break;
							}
							case 7: { // vlineto
								const double dy = E.Tirer();
								E.sp = 0;
								E.Ligne(E.x, E.y + dy);
								break;
							}
							case 8: { // rrcurveto
								if (E.sp < 6)
									return false;
								const double *a = &E.pile[E.sp - 6];
								const double x1 = E.x + a[0], y1 = E.y + a[1], x2 = x1 + a[2], y2 = y1 + a[3], x3 = x2 + a[4], y3 = y2 + a[5];
								E.sp = 0;
								E.Courbe(x1, y1, x2, y2, x3, y3);
								break;
							}
							case 9: // closepath
								E.sp = 0;
								E.Fermer();
								break;
							case 10: { // callsubr
								const int32 k = (int32)E.Tirer();
								if (k < 0 || (usize)k >= subrDebut.Size() || subrLon[(usize)k] == 0)
									return false;
								NkVector<uint8> s;
								if (lenIV >= 0)
									T1Dechiffrer(prive.Data() + subrDebut[(usize)k], subrLon[(usize)k], 4330u, s, (usize)lenIV);
								else
									for (usize j = 0; j < subrLon[(usize)k]; ++j)
										s.PushBack(prive[subrDebut[(usize)k] + j]);
								if (!T1Executer(s.Data(), s.Size(), E, prive, subrDebut, subrLon, lenIV, prof + 1))
									return false;
								break;
							}
							case 11: // return
								return true;
							case 13: { // hsbw sbx wx
								const double wx = E.Tirer(), sbx = E.Tirer();
								E.sp = 0;
								E.largeur = wx;
								E.x = sbx;
								E.y = 0.0;
								break;
							}
							case 14: // endchar
								E.Fermer();
								E.fin = true;
								return true;
							case 21: { // rmoveto
								const double dy = E.Tirer(), dx = E.Tirer();
								E.sp = 0;
								E.Deplacer(E.x + dx, E.y + dy);
								break;
							}
							case 22: { // hmoveto
								const double dx = E.Tirer();
								E.sp = 0;
								E.Deplacer(E.x + dx, E.y);
								break;
							}
							case 30: { // vhcurveto dy1 dx2 dy2 dx3
								if (E.sp < 4)
									return false;
								const double *a = &E.pile[E.sp - 4];
								const double x1 = E.x, y1 = E.y + a[0], x2 = x1 + a[1], y2 = y1 + a[2], x3 = x2 + a[3], y3 = y2;
								E.sp = 0;
								E.Courbe(x1, y1, x2, y2, x3, y3);
								break;
							}
							case 31: { // hvcurveto dx1 dx2 dy2 dy3
								if (E.sp < 4)
									return false;
								const double *a = &E.pile[E.sp - 4];
								const double x1 = E.x + a[0], y1 = E.y, x2 = x1 + a[1], y2 = y1 + a[2], x3 = x2, y3 = y2 + a[3];
								E.sp = 0;
								E.Courbe(x1, y1, x2, y2, x3, y3);
								break;
							}
							case 12: { // escape
								if (i >= n)
									return false;
								const uint8 w = cs[i++];
								switch (w) {
									case 0:	 // dotsection
									case 1:	 // vstem3
									case 2:	 // hstem3
										E.sp = 0;
										break;
									case 6: { // seac asb adx ady bchar achar
										if (E.sp < 5)
											return false;
										const double *a = &E.pile[E.sp - 5];
										E.asb = a[0];
										E.adx = a[1];
										E.ady = a[2];
										E.bchar = (int32)a[3];
										E.achar = (int32)a[4];
										E.seac = true;
										E.sp = 0;
										E.fin = true;
										return true;
									}
									case 7: { // sbw sbx sby wx wy
										if (E.sp < 4)
											return false;
										const double *a = &E.pile[E.sp - 4];
										E.x = a[0];
										E.y = a[1];
										E.largeur = a[2];
										E.sp = 0;
										break;
									}
									case 12: { // div
										const double b = E.Tirer(), a = E.Tirer();
										E.Pousser(b != 0.0 ? a / b : 0.0);
										break;
									}
									case 16: { // callothersubr
										const int32 autre = (int32)E.Tirer();
										int32 nArgs = (int32)E.Tirer();
										if (nArgs < 0 || nArgs > E.sp)
											nArgs = E.sp;
										double args[16];
										const int32 na = nArgs < 16 ? nArgs : 16;
										for (int32 k = 0; k < na; ++k)
											args[k] = E.pile[E.sp - nArgs + k];
										E.sp -= nArgs;
										if (autre == 1) { // le debut d'un flex
											E.flex = true;
											E.nf = 0;
										} else if (autre == 2) { // un point du flex : retenu par rmoveto
										} else if (autre == 0 && E.flex) { // la fin du flex : deux courbes
											E.flex = false;
											// 7 points retenus : [0] la reference (ignoree), [1..3] et [4..6]
											// les deux courbes ; la premiere part du point courant d'AVANT le flex
											if (E.nf >= 7) {
												E.Courbe(E.fx[1], E.fy[1], E.fx[2], E.fy[2], E.fx[3], E.fy[3]);
												E.Courbe(E.fx[4], E.fy[4], E.fx[5], E.fy[5], E.fx[6], E.fy[6]);
											}
											if (na >= 3 && E.psp + 2 <= 32) { // pop pop setcurrentpoint : x puis y
												E.ps[E.psp++] = args[2];
												E.ps[E.psp++] = args[1];
											}
										} else if (autre == 3) { // remplacement d'indices : le subr 3
											if (E.psp < 32)
												E.ps[E.psp++] = 3.0;
										} else { // inconnu : ses arguments reviennent par pop, dans l'ordre
											for (int32 k = na - 1; k >= 0; --k)
												if (E.psp < 32)
													E.ps[E.psp++] = args[k];
										}
										break;
									}
									case 17: // pop
										E.Pousser(E.psp > 0 ? E.ps[--E.psp] : 0.0);
										break;
									case 33: { // setcurrentpoint x y
										const double y = E.Tirer(), x = E.Tirer();
										E.sp = 0;
										E.x = x;
										E.y = y;
										break;
									}
									default:
										E.sp = 0;
										break;
								}
								break;
							}
							default:
								E.sp = 0;
								break;
						}
					}
					return true;
				}
			} // namespace

			bool NkPdfType1::Executer(int32 index, NkVector<NkPdfT1Op> &ops, double dx, double dy, double *largeur, int32 profondeurSeac) const {
				if (!mValide || index < 0 || (usize)index >= mGlyphes.Size() || profondeurSeac > 2)
					return false;
				const Glyphe &g = mGlyphes[(usize)index];
				NkVector<uint8> cs;
				if (mLenIV >= 0)
					T1Dechiffrer(mPrive.Data() + g.debut, g.longueur, 4330u, cs, (usize)mLenIV);
				else
					for (usize j = 0; j < g.longueur; ++j)
						cs.PushBack(mPrive[g.debut + j]);
				T1Etat E;
				E.ops = &ops;
				E.dx = dx;
				E.dy = dy;
				const bool ok = T1Executer(cs.Data(), cs.Size(), E, mPrive, mSubrDebut, mSubrLongueur, mLenIV, 0);
				if (largeur)
					*largeur = E.largeur;
				if (!ok)
					return false;
				if (E.seac && !T1MutationProg("type1-seac-ignore")) { // un caractere ACCENTUE : la base, puis l'accent decale de (adx - asb, ady)
					const char *nb = StandardName((uint32)E.bchar), *na = StandardName((uint32)E.achar);
					int32 lb = 0, la = 0;
					while (nb && nb[lb])
						++lb;
					while (na && na[la])
						++la;
					const int32 ib = GlyphIndex(nb, lb), ia = GlyphIndex(na, la);
					if (ib >= 0)
						(void)Executer(ib, ops, dx, dy, nullptr, profondeurSeac + 1);
					if (ia >= 0)
						(void)Executer(ia, ops, dx + E.adx - E.asb, dy + E.ady, nullptr, profondeurSeac + 1);
				}
				return true;
			}

			bool NkPdfType1::Outline(int32 index, NkVector<NkPdfT1Op> &ops, double *largeur) const {
				return Executer(index, ops, 0.0, 0.0, largeur, 0);
			}

		} // namespace pdf
	} // namespace media
} // namespace nkentseu
