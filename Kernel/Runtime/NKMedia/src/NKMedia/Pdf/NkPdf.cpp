//
// NkPdf.cpp — analyseur de structure PDF (lecture seule).
//
// Voir NkPdf.h pour le perimetre, decide par mesure sur un corpus reel.
//
#include "NKMedia/Pdf/NkPdf.h"

#include "NKFileSystem/NkFile.h"
#include "NKImage/Core/NkImage.h" // NkDeflate::Decompress (FlateDecode)

namespace nkentseu {
	namespace media {
		namespace pdf {

			// ── Classes de caracteres PDF (§7.2) ──
			static inline bool IsWs(uint8 c) {
				return c == 0 || c == 9 || c == 10 || c == 12 || c == 13 || c == 32;
			}
			static inline bool IsDelim(uint8 c) {
				return c == '(' || c == ')' || c == '<' || c == '>' || c == '[' || c == ']' || c == '{' ||
					   c == '}' || c == '/' || c == '%';
			}
			static inline bool IsReg(uint8 c) { return !IsWs(c) && !IsDelim(c); }
			static inline int32 HexVal(uint8 c) {
				if (c >= '0' && c <= '9')
					return c - '0';
				if (c >= 'a' && c <= 'f')
					return c - 'a' + 10;
				if (c >= 'A' && c <= 'F')
					return c - 'A' + 10;
				return -1;
			}

			// Comparaison d'un mot-cle a la position p (sans consommer).
			static bool KeywordAt(const NkVector<uint8> &b, usize p, const char *kw) {
				usize i = 0;
				for (; kw[i]; ++i)
					if (p + i >= b.Size() || b[p + i] != static_cast<uint8>(kw[i]))
						return false;
				return true;
			}

			// kMaxDepth est declare dans NkPdf.h : les DEUX unites de compilation en
			// ont besoin (analyse ici, chainage /Parent et /Prev dans NkPdfLoad.cpp).

			// ============================================================
			// Pool de texte
			// ============================================================

			int32 NkPdfDoc::PoolPush(const char *s, int32 n) {
				const int32 off = static_cast<int32>(mPool.Size());
				for (int32 i = 0; i < n; ++i)
					mPool += s[i];
				return off;
			}

			// ============================================================
			// Analyse syntaxique
			// ============================================================

			void NkPdfDoc::SkipWs(usize &p) const {
				for (;;) {
					while (p < mBuf.Size() && IsWs(mBuf[p]))
						++p;
					// Commentaire « % » jusqu'a la fin de ligne.
					if (p < mBuf.Size() && mBuf[p] == '%') {
						while (p < mBuf.Size() && mBuf[p] != '\n' && mBuf[p] != '\r')
							++p;
						continue;
					}
					return;
				}
			}

			// Analyse la valeur a la position p et renvoie son index dans mVals.
			// Avance p au-dela de la valeur. Renvoie -1 si rien d'exploitable.
			int32 NkPdfDoc::ParseValueAt(usize &p, int32 depth) {
				if (depth > kMaxDepth)
					return -1;
				SkipWs(p);
				if (p >= mBuf.Size())
					return -1;

				const uint8 c = mBuf[p];

				// ── Dictionnaire « << ... >> » (et flux eventuel) ──
				if (c == '<' && p + 1 < mBuf.Size() && mBuf[p + 1] == '<') {
					p += 2;
					// Entrees collectees a part : mEnts doit rester contigu par
					// dictionnaire, or l'analyse des valeurs imbriquees y ajoute
					// elle-meme des entrees. On accumule donc localement puis on copie.
					NkVector<Ent> local;
					for (;;) {
						SkipWs(p);
						if (p + 1 < mBuf.Size() && mBuf[p] == '>' && mBuf[p + 1] == '>') {
							p += 2;
							break;
						}
						if (p >= mBuf.Size())
							break;
						if (mBuf[p] != '/') { // cle attendue : document malforme, on abandonne
							++p;
							continue;
						}
						const int32 keyIdx = ParseValueAt(p, depth + 1);
						if (keyIdx < 0)
							break;
						const int32 valIdx = ParseValueAt(p, depth + 1);
						if (valIdx < 0)
							break;
						Ent e;
						e.keyOff = mVals[static_cast<usize>(keyIdx)].a;
						e.keyLen = mVals[static_cast<usize>(keyIdx)].b;
						e.val = valIdx;
						local.PushBack(e);
					}
					NkPdfVal v;
					v.kind = NK_PDF_DICT;
					v.a = static_cast<int32>(mEnts.Size());
					v.b = static_cast<int32>(local.Size());
					for (usize i = 0; i < local.Size(); ++i)
						mEnts.PushBack(local[i]);

					// « stream » colle au dictionnaire => c'est un flux.
					usize q = p;
					SkipWs(q);
					if (KeywordAt(mBuf, q, "stream")) {
						q += 6;
						// EOL obligatoire apres « stream » : CRLF ou LF (jamais CR seul).
						if (q < mBuf.Size() && mBuf[q] == '\r')
							++q;
						if (q < mBuf.Size() && mBuf[q] == '\n')
							++q;
						v.kind = NK_PDF_STREAM;
						v.rawOff = q;
						v.rawLen = 0; // renseigne plus bas (/Length peut etre indirect)
						p = q;		  // la longueur reelle est resolue par l'appelant
					}
					const int32 idx = static_cast<int32>(mVals.Size());
					mVals.PushBack(v);

					if (v.kind == NK_PDF_STREAM) {
						// /Length peut etre une REFERENCE INDIRECTE : c'est le cas des
						// generateurs qui ecrivent le flux avant d'en connaitre la taille.
						// Il faut donc pouvoir resoudre AVANT d'avoir fini d'analyser.
						usize len = 0;
						bool haveLen = false;
						const NkPdfVal lenV = DictGet(mVals[static_cast<usize>(idx)], "Length");
						if (lenV.IsNum() && lenV.num >= 0) {
							len = static_cast<usize>(lenV.num);
							haveLen = true;
						}
						// Verification : le mot-cle « endstream » doit suivre. Sinon la
						// longueur annoncee est fausse (cas frequent) -> on la recalcule
						// en cherchant « endstream », plutot que de tronquer le flux.
						bool ok = false;
						if (haveLen && v.rawOff + len <= mBuf.Size()) {
							usize e = v.rawOff + len;
							SkipWs(e);
							ok = KeywordAt(mBuf, e, "endstream");
						}
						if (!ok) {
							usize e = v.rawOff;
							while (e + 9 <= mBuf.Size() && !KeywordAt(mBuf, e, "endstream"))
								++e;
							len = (e > v.rawOff) ? (e - v.rawOff) : 0;
							// Retire l'EOL qui precede « endstream » : il ne fait pas
							// partie des donnees.
							while (len > 0 && (mBuf[v.rawOff + len - 1] == '\n' || mBuf[v.rawOff + len - 1] == '\r'))
								--len;
						}
						mVals[static_cast<usize>(idx)].rawLen = len;
						usize e2 = v.rawOff + len;
						SkipWs(e2);
						if (KeywordAt(mBuf, e2, "endstream"))
							e2 += 9;
						p = e2;
					}
					return idx;
				}

				// ── Chaine hexadecimale « <ABCD> » ──
				if (c == '<') {
					++p;
					NkString s;
					int32 hi = -1;
					while (p < mBuf.Size() && mBuf[p] != '>') {
						const int32 h = HexVal(mBuf[p]);
						++p;
						if (h < 0)
							continue;
						if (hi < 0)
							hi = h;
						else {
							s += static_cast<char>((hi << 4) | h);
							hi = -1;
						}
					}
					if (hi >= 0) // nombre impair de chiffres : le dernier vaut « hi0 »
						s += static_cast<char>(hi << 4);
					if (p < mBuf.Size())
						++p; // '>'
					NkPdfVal v;
					v.kind = NK_PDF_STRING;
					v.a = PoolPush(s.CStr(), static_cast<int32>(s.Size()));
					v.b = static_cast<int32>(s.Size());
					const int32 idx = static_cast<int32>(mVals.Size());
					mVals.PushBack(v);
					return idx;
				}

				// ── Chaine litterale « (texte) » ──
				if (c == '(') {
					++p;
					NkString s;
					int32 nest = 1;
					while (p < mBuf.Size()) {
						const uint8 ch = mBuf[p++];
						if (ch == '\\') {
							if (p >= mBuf.Size())
								break;
							const uint8 e = mBuf[p++];
							switch (e) {
								case 'n': s += '\n'; break;
								case 'r': s += '\r'; break;
								case 't': s += '\t'; break;
								case 'b': s += '\b'; break;
								case 'f': s += '\f'; break;
								case '\r': // continuation de ligne : rien
									if (p < mBuf.Size() && mBuf[p] == '\n')
										++p;
									break;
								case '\n': break;
								default:
									if (e >= '0' && e <= '7') { // octal, 1 a 3 chiffres
										int32 o = e - '0';
										for (int32 k = 0; k < 2 && p < mBuf.Size() && mBuf[p] >= '0' && mBuf[p] <= '7';
											 ++k)
											o = o * 8 + (mBuf[p++] - '0');
										s += static_cast<char>(o & 0xFF);
									} else
										s += static_cast<char>(e);
									break;
							}
							continue;
						}
						if (ch == '(') {
							++nest;
							s += '(';
							continue;
						}
						if (ch == ')') {
							if (--nest == 0)
								break;
							s += ')';
							continue;
						}
						s += static_cast<char>(ch);
					}
					NkPdfVal v;
					v.kind = NK_PDF_STRING;
					v.a = PoolPush(s.CStr(), static_cast<int32>(s.Size()));
					v.b = static_cast<int32>(s.Size());
					const int32 idx = static_cast<int32>(mVals.Size());
					mVals.PushBack(v);
					return idx;
				}

				// ── Nom « /Nom » ──
				if (c == '/') {
					++p;
					NkString s;
					while (p < mBuf.Size() && IsReg(mBuf[p])) {
						uint8 ch = mBuf[p++];
						if (ch == '#' && p + 1 < mBuf.Size()) { // sequence #xx
							const int32 h1 = HexVal(mBuf[p]), h2 = HexVal(mBuf[p + 1]);
							if (h1 >= 0 && h2 >= 0) {
								ch = static_cast<uint8>((h1 << 4) | h2);
								p += 2;
							}
						}
						s += static_cast<char>(ch);
					}
					NkPdfVal v;
					v.kind = NK_PDF_NAME;
					v.a = PoolPush(s.CStr(), static_cast<int32>(s.Size()));
					v.b = static_cast<int32>(s.Size());
					const int32 idx = static_cast<int32>(mVals.Size());
					mVals.PushBack(v);
					return idx;
				}

				// ── Tableau « [ ... ] » ──
				if (c == '[') {
					++p;
					NkVector<int32> local; // meme raison que pour les dictionnaires
					for (;;) {
						SkipWs(p);
						if (p >= mBuf.Size())
							break;
						if (mBuf[p] == ']') {
							++p;
							break;
						}
						const int32 e = ParseValueAt(p, depth + 1);
						if (e < 0)
							break;
						local.PushBack(e);
					}
					NkPdfVal v;
					v.kind = NK_PDF_ARRAY;
					v.a = static_cast<int32>(mKids.Size());
					v.b = static_cast<int32>(local.Size());
					for (usize i = 0; i < local.Size(); ++i)
						mKids.PushBack(local[i]);
					const int32 idx = static_cast<int32>(mVals.Size());
					mVals.PushBack(v);
					return idx;
				}

				// ── Mots-cles ──
				if (KeywordAt(mBuf, p, "true") || KeywordAt(mBuf, p, "false")) {
					const bool val = mBuf[p] == 't';
					p += val ? 4u : 5u;
					NkPdfVal v;
					v.kind = NK_PDF_BOOL;
					v.num = val ? 1.0 : 0.0;
					const int32 idx = static_cast<int32>(mVals.Size());
					mVals.PushBack(v);
					return idx;
				}
				if (KeywordAt(mBuf, p, "null")) {
					p += 4;
					NkPdfVal v;
					const int32 idx = static_cast<int32>(mVals.Size());
					mVals.PushBack(v);
					return idx;
				}

				// ── Nombre, ou reference indirecte « N G R » ──
				if (c == '+' || c == '-' || c == '.' || (c >= '0' && c <= '9')) {
					const usize start = p;
					bool real = false;
					if (mBuf[p] == '+' || mBuf[p] == '-')
						++p;
					while (p < mBuf.Size() && ((mBuf[p] >= '0' && mBuf[p] <= '9') || mBuf[p] == '.')) {
						if (mBuf[p] == '.')
							real = true;
						++p;
					}
					// Conversion maison : pas de strtod (locale, et le buffer n'est pas
					// termine par un zero).
					double sign = 1.0, val = 0.0;
					usize q = start;
					if (mBuf[q] == '-') {
						sign = -1.0;
						++q;
					} else if (mBuf[q] == '+')
						++q;
					while (q < p && mBuf[q] >= '0' && mBuf[q] <= '9')
						val = val * 10.0 + (mBuf[q++] - '0');
					if (q < p && mBuf[q] == '.') {
						++q;
						double f = 0.1;
						while (q < p && mBuf[q] >= '0' && mBuf[q] <= '9') {
							val += (mBuf[q++] - '0') * f;
							f *= 0.1;
						}
					}
					val *= sign;

					// Reference indirecte : un ENTIER NON SIGNE suivi d'un entier et de
					// « R ». Il faut regarder devant SANS consommer, sinon un simple
					// nombre suivi d'un autre serait mange.
					if (!real && sign > 0) {
						usize look = p;
						SkipWs(look);
						const usize g0 = look;
						while (look < mBuf.Size() && mBuf[look] >= '0' && mBuf[look] <= '9')
							++look;
						if (look > g0) {
							int32 gen = 0;
							for (usize k = g0; k < look; ++k)
								gen = gen * 10 + (mBuf[k] - '0');
							usize look2 = look;
							SkipWs(look2);
							if (look2 < mBuf.Size() && mBuf[look2] == 'R' &&
								(look2 + 1 >= mBuf.Size() || !IsReg(mBuf[look2 + 1]))) {
								p = look2 + 1;
								NkPdfVal v;
								v.kind = NK_PDF_REF;
								v.a = static_cast<int32>(val);
								v.b = gen;
								const int32 idx = static_cast<int32>(mVals.Size());
								mVals.PushBack(v);
								return idx;
							}
						}
					}
					NkPdfVal v;
					v.kind = real ? NK_PDF_REAL : NK_PDF_INT;
					v.num = val;
					const int32 idx = static_cast<int32>(mVals.Size());
					mVals.PushBack(v);
					return idx;
				}

				// Jeton inconnu : on l'ignore en avancant d'un caractere regulier au
				// moins, pour ne jamais boucler indefiniment sur une entree malformee.
				if (IsReg(c)) {
					while (p < mBuf.Size() && IsReg(mBuf[p]))
						++p;
				} else
					++p;
				return -1;
			}

			// ============================================================
			// Acces aux objets
			// ============================================================

			NkPdfVal NkPdfDoc::Resolve(const NkPdfVal &v) const {
				NkPdfVal cur = v;
				for (int32 hop = 0; hop < 32; ++hop) { // borne : cycle de references possible
					if (cur.kind != NK_PDF_REF)
						return cur;
					const int32 idx = LoadObject(cur.a);
					if (idx < 0)
						return NkPdfVal();
					cur = mVals[static_cast<usize>(idx)];
				}
				return NkPdfVal();
			}

			NkPdfVal NkPdfDoc::DictGet(const NkPdfVal &dict, const char *key) const {
				if (!dict.IsDictLike())
					return NkPdfVal();
				int32 klen = 0;
				while (key[klen])
					++klen;
				for (int32 i = 0; i < dict.b; ++i) {
					const usize ei = static_cast<usize>(dict.a + i);
					if (ei >= mEnts.Size())
						break;
					const Ent &e = mEnts[ei];
					if (e.keyLen != klen)
						continue;
					bool same = true;
					for (int32 k = 0; k < klen && same; ++k)
						same = mPool.CStr()[e.keyOff + k] == key[k];
					if (same)
						return Resolve(mVals[static_cast<usize>(e.val)]);
				}
				return NkPdfVal();
			}

			NkPdfVal NkPdfDoc::DictValueAt(const NkPdfVal &dict, int32 i) const {
				if (!dict.IsDictLike() || i < 0 || i >= dict.b)
					return NkPdfVal();
				const usize ei = static_cast<usize>(dict.a + i);
				if (ei >= mEnts.Size())
					return NkPdfVal();
				return Resolve(mVals[static_cast<usize>(mEnts[ei].val)]);
			}

			NkPdfVal NkPdfDoc::ArrayAt(const NkPdfVal &arr, int32 i) const {
				if (arr.kind != NK_PDF_ARRAY || i < 0 || i >= arr.b)
					return NkPdfVal();
				const usize ki = static_cast<usize>(arr.a + i);
				if (ki >= mKids.Size())
					return NkPdfVal();
				return Resolve(mVals[static_cast<usize>(mKids[ki])]);
			}

			const char *NkPdfDoc::Text(const NkPdfVal &v, int32 *len) const {
				if (v.kind != NK_PDF_NAME && v.kind != NK_PDF_STRING) {
					if (len)
						*len = 0;
					return "";
				}
				if (len)
					*len = v.b;
				return mPool.CStr() + v.a;
			}

			bool NkPdfDoc::NameIs(const NkPdfVal &v, const char *name) const {
				if (v.kind != NK_PDF_NAME)
					return false;
				int32 n = 0;
				const char *s = Text(v, &n);
				for (int32 i = 0; i < n; ++i)
					if (!name[i] || name[i] != s[i])
						return false;
				return name[n] == 0;
			}

		} // namespace pdf
	} // namespace media
} // namespace nkentseu
