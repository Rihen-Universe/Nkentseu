// =============================================================================
// NkTerminalEmulateur.cpp — la machine a etats VT de NkTerm. Voir l'en-tete
// pour ce qui a change en descendant de NKCode, et pour ce qui manque encore.
// =============================================================================
#include "NKEditorKit/Terminal/NkTerminalEmulateur.h"

#include <cstdio>

namespace nkentseu {
	namespace editorkit {

		namespace {
			int32 Borne(int32 v, int32 lo, int32 hi) {
				return v < lo ? lo : (v > hi ? hi : v);
			}

			/// Le jeu DEC « dessin de lignes » (ESC ( 0) : les lettres de `j` a `x`
			/// deviennent des coins et des traits. 0 = pas de correspondance.
			uint32 DessinDeLigne(uint32 c) {
				switch (c) {
					case 'j': return 0x2518; // ┘
					case 'k': return 0x2510; // ┐
					case 'l': return 0x250C; // ┌
					case 'm': return 0x2514; // └
					case 'n': return 0x253C; // ┼
					case 'q': return 0x2500; // ─
					case 't': return 0x251C; // ├
					case 'u': return 0x2524; // ┤
					case 'v': return 0x2534; // ┴
					case 'w': return 0x252C; // ┬
					case 'x': return 0x2502; // │
					case 'a': return 0x2592; // ▒
					case '`': return 0x25C6; // ◆
					case '~': return 0x00B7; // ·
					default: return 0;
				}
			}

			int32 EncoderU8(uint32 cp, char *dst) {
				if (cp < 0x80) {
					dst[0] = static_cast<char>(cp);
					return 1;
				}
				if (cp < 0x800) {
					dst[0] = static_cast<char>(0xC0 | (cp >> 6));
					dst[1] = static_cast<char>(0x80 | (cp & 0x3F));
					return 2;
				}
				if (cp < 0x10000) {
					dst[0] = static_cast<char>(0xE0 | (cp >> 12));
					dst[1] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
					dst[2] = static_cast<char>(0x80 | (cp & 0x3F));
					return 3;
				}
				dst[0] = static_cast<char>(0xF0 | (cp >> 18));
				dst[1] = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
				dst[2] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
				dst[3] = static_cast<char>(0x80 | (cp & 0x3F));
				return 4;
			}

			/// « file://hote/C:/a%20b » -> « C:/a b » ; « file:///home/x » -> « /home/x ».
			NkString CheminDeFichierUri(const char *u) {
				const char *p = u;
				if (p[0] == 'f' && p[1] == 'i' && p[2] == 'l' && p[3] == 'e' && p[4] == ':' && p[5] == '/' && p[6] == '/') {
					p += 7;
					while (*p && *p != '/')
						++p; // l'hote
				}
				// « /C:/... » : la barre de tete ne fait pas partie d'un chemin Windows.
				if (p[0] == '/' && p[1] && p[2] == ':')
					++p;
				NkString out;
				for (; *p; ++p) {
					if (*p == '%' && p[1] && p[2]) {
						auto hex = [](char h) -> int32 {
							return (h >= '0' && h <= '9')	? h - '0'
								   : (h >= 'a' && h <= 'f') ? h - 'a' + 10
								   : (h >= 'A' && h <= 'F') ? h - 'A' + 10
															: -1;
						};
						const int32 a = hex(p[1]), b = hex(p[2]);
						if (a >= 0 && b >= 0) {
							out += static_cast<char>(a * 16 + b);
							p += 2;
							continue;
						}
					}
					out += *p;
				}
				return out;
			}
		} // namespace

		NkTerm::NkTerm() {
			Resize(80, 24);
		}

		void NkTerm::BlankLine(Line &ln) const {
			ln.Clear();
			NkTermCell c;
			c.bg = mBg; // effacer peint avec le fond COURANT (comportement xterm)
			for (int16 i = 0; i < mCols; ++i)
				ln.PushBack(c);
		}

		void NkTerm::EnsureWidth(Line &ln) const {
			while (static_cast<int16>(ln.Size()) < mCols)
				ln.PushBack(NkTermCell{});
		}

		NkTermCell NkTerm::Blank() const {
			NkTermCell c;
			c.bg = mBg;
			return c;
		}

		void NkTerm::Resize(int16 cols, int16 rows) {
			if (cols < 1)
				cols = 1;
			if (rows < 1)
				rows = 1;
			if (cols == mCols && rows == mRows && mScreen.Size() == static_cast<usize>(rows))
				return;
			// Ecran qui RETRECIT sous le curseur : les lignes du haut partent dans
			// l'historique, l'invite reste visible.
			int32 decale = 0;
			if (!mAlt && mScreen.Size() > 0 && mCy >= rows) {
				decale = mCy - rows + 1;
				for (int32 r = 0; r < decale && r < static_cast<int32>(mScreen.Size()); ++r)
					PousserHistorique(mScreen[static_cast<usize>(r)]);
			}
			NkVector<Line> ancien = static_cast<NkVector<Line> &&>(mScreen);
			mScreen.Clear();
			mCols = cols;
			mRows = rows;
			for (int16 r = 0; r < mRows; ++r) {
				mScreen.PushBack(Line());
				const usize src = static_cast<usize>(r + decale);
				if (src < ancien.Size()) {
					Line &ln = mScreen[mScreen.Size() - 1];
					ln = static_cast<Line &&>(ancien[src]);
					while (static_cast<int16>(ln.Size()) > mCols)
						ln.PopBack();
					EnsureWidth(ln);
				} else {
					BlankLine(mScreen[mScreen.Size() - 1]);
				}
			}
			mCy = static_cast<int16>(mCy - decale);
			mTop = 0;
			mBot = mRows - 1;
			if (mCx > mCols)
				mCx = mCols;
			if (mCy >= mRows)
				mCy = mRows - 1;
			if (mCy < 0)
				mCy = 0;
			++mVersion;
		}

		const NkTerm::Line &NkTerm::LineAt(usize i) const {
			if (i < mNbScroll)
				return mScroll[(mTeteScroll + i) % mScroll.Size()];
			return mScreen[i - mNbScroll];
		}

		NkString NkTerm::TexteLigne(usize i) const {
			NkString out;
			if (i >= TotalLines())
				return out;
			const Line &ln = LineAt(i);
			usize fin = ln.Size();
			while (fin > 0 && (ln[fin - 1].cp == 0x20 || ln[fin - 1].cp == 0))
				--fin;
			for (usize c = 0; c < fin; ++c) {
				char u8[5];
				const int32 n = EncoderU8(ln[c].cp ? ln[c].cp : 0x20, u8);
				for (int32 k = 0; k < n; ++k)
					out += u8[k];
			}
			return out;
		}

		void NkTerm::Clear() {
			mScroll.Clear();
			mTeteScroll = mNbScroll = 0;
			mFg = mBg = kNkTermDefaut;
			mAttr = 0;
			for (usize r = 0; r < mScreen.Size(); ++r)
				BlankLine(mScreen[r]);
			mCx = mCy = 0;
			mTop = 0;
			mBot = mRows - 1;
			mState = Ground;
			mAlt = mAppCursor = mBracketedPaste = mDessinLignes = false;
			mAutoWrap = mCursorVis = mCurseurClignote = true;
			mCurseurForme = NkTermCurseur::Bloc;
			++mVersion;
		}

		void NkTerm::PrendreReponses(NkVector<char> &out) {
			for (usize i = 0; i < mReponses.Size(); ++i)
				out.PushBack(mReponses[i]);
			mReponses.Clear();
		}

		void NkTerm::Repondre(const char *s) {
			for (; *s; ++s)
				mReponses.PushBack(*s);
		}

		// ── Defilement ────────────────────────────────────────────────────────
		void NkTerm::PousserHistorique(Line &ln) {
			if (mScroll.Size() < kHistorique) {
				mScroll.PushBack(static_cast<Line &&>(ln));
				++mNbScroll;
			} else {
				// Anneau plein : on ECRASE la plus ancienne, sans rien deplacer.
				mScroll[mTeteScroll] = static_cast<Line &&>(ln);
				mTeteScroll = (mTeteScroll + 1) % mScroll.Size();
			}
		}

		void NkTerm::ScrollUp() {
			if (mTop == 0 && !mAlt)
				PousserHistorique(mScreen[0]);
			for (int16 r = mTop; r < mBot; ++r)
				mScreen[r] = static_cast<Line &&>(mScreen[r + 1]);
			BlankLine(mScreen[mBot]);
		}

		void NkTerm::ScrollDown() {
			for (int16 r = mBot; r > mTop; --r)
				mScreen[r] = static_cast<Line &&>(mScreen[r - 1]);
			BlankLine(mScreen[mTop]);
		}

		void NkTerm::LineFeed() {
			if (mCy == mBot)
				ScrollUp();
			else if (mCy < mRows - 1)
				++mCy;
		}

		void NkTerm::PutGlyph(uint32 cp) {
			if (mDessinLignes && cp < 0x80) {
				const uint32 d = DessinDeLigne(cp);
				if (d)
					cp = d;
			}
			// Retour a la ligne DIFFERE (comme xterm) : le curseur reste APRES la
			// derniere colonne, et c'est le glyphe SUIVANT qui passe a la ligne.
			if (mCx >= mCols) {
				if (mAutoWrap) {
					mCx = 0;
					LineFeed();
				} else {
					mCx = mCols - 1;
				}
			}
			EnsureWidth(mScreen[mCy]);
			NkTermCell &cell = mScreen[mCy][mCx];
			cell.cp = cp;
			cell.fg = mFg;
			cell.bg = mBg;
			cell.attr = mAttr;
			mDernier = cp;
			++mCx;
		}

		// ── Octet a octet ─────────────────────────────────────────────────────
		void NkTerm::Feed(const char *data, usize len) {
			if (len == 0)
				return;
			for (usize i = 0; i < len; ++i)
				Byte(static_cast<unsigned char>(data[i]));
			++mVersion;
		}

		void NkTerm::Byte(unsigned char b) {
			switch (mState) {
				case Ground:
					Ground_(b);
					break;
				case Esc:
					Esc_(b);
					break;
				case Csi:
					Csi_(b);
					break;
				case Osc:
					Osc_(b);
					break;
				case OscEsc:
					if (b == '\\') {
						FinOsc();
						mState = Ground;
					} else {
						mState = Osc;
					}
					break;
				case Dcs: // consomme jusqu'au ST (ESC \) ou BEL
					if (b == 0x1b)
						mState = DcsEsc;
					else if (b == 0x07)
						mState = Ground;
					break;
				case DcsEsc:
					mState = (b == '\\') ? Ground : Dcs;
					break;
				case EscCharset:
					// Le jeu designe pour G0 : '0' = dessin de lignes, le reste = ASCII.
					mDessinLignes = (b == '0');
					mState = Ground;
					break;
				case EscArg:
					mState = Ground;
					break;
			}
		}

		void NkTerm::Ground_(unsigned char b) {
			if (mU8need) {
				if ((b & 0xC0) == 0x80) {
					mU8 = (mU8 << 6) | (b & 0x3F);
					if (--mU8need == 0)
						PutGlyph(mU8);
					return;
				}
				mU8need = 0; // sequence cassee : on repart sur cet octet
			}
			switch (b) {
				case 0x1b:
					mState = Esc;
					return;
				case '\r':
					mCx = 0;
					return;
				case '\n':
				case 0x0b:
				case 0x0c:
					LineFeed();
					return;
				case '\b':
					if (mCx >= mCols)
						mCx = mCols - 1;
					if (mCx > 0)
						--mCx;
					return;
				case '\t': {
					const int16 n = static_cast<int16>((mCx / 8 + 1) * 8);
					mCx = n >= mCols ? mCols - 1 : n;
					return;
				}
				case 0x0e: // SO / SI : bascule de jeu, sans objet ici
				case 0x0f:
				case 0x07: // BEL
					return;
				default:
					break;
			}
			if (b < 0x20 || b == 0x7f)
				return;
			if (b < 0x80) {
				PutGlyph(b);
				return;
			}
			if ((b & 0xE0) == 0xC0) {
				mU8 = b & 0x1F;
				mU8need = 1;
			} else if ((b & 0xF0) == 0xE0) {
				mU8 = b & 0x0F;
				mU8need = 2;
			} else if ((b & 0xF8) == 0xF0) {
				mU8 = b & 0x07;
				mU8need = 3;
			} else {
				PutGlyph(0xFFFD);
			}
		}

		void NkTerm::Sauver() {
			mSaveCx = mCx;
			mSaveCy = mCy;
			mSaveFg = mFg;
			mSaveBg = mBg;
			mSaveAttr = mAttr;
		}

		void NkTerm::Restaurer() {
			mCx = mSaveCx;
			mCy = mSaveCy;
			mFg = mSaveFg;
			mBg = mSaveBg;
			mAttr = mSaveAttr;
			if (mCy >= mRows)
				mCy = mRows - 1;
		}

		void NkTerm::Esc_(unsigned char b) {
			mState = Ground;
			switch (b) {
				case '[':
					mCsiLen = 0;
					mState = Csi;
					return;
				case ']':
					mOscLen = 0;
					mState = Osc;
					return;
				case 'P': // DCS : consomme
					mState = Dcs;
					return;
				case 'M': // RI : remonte, defile vers le bas en haut de region
					if (mCy == mTop)
						ScrollDown();
					else if (mCy > 0)
						--mCy;
					return;
				case 'D': // IND
					LineFeed();
					return;
				case 'E': // NEL
					mCx = 0;
					LineFeed();
					return;
				case '7':
					Sauver();
					return;
				case '8':
					Restaurer();
					return;
				case 'c': { // RIS : remise a zero complete
					const int16 c = mCols, r = mRows;
					Clear();
					Resize(c, r);
					return;
				}
				case '(':
					mState = EscCharset;
					return;
				case ')':
				case '*':
				case '+':
					mState = EscArg; // jeux G1..G3 : un octet a consommer, sans effet
					return;
				default:
					return; // '=' '>' (pave numerique) et le reste : sans effet
			}
		}

		void NkTerm::Csi_(unsigned char b) {
			if (b >= 0x40 && b <= 0x7E) { // octet final
				mCsi[mCsiLen < static_cast<int32>(sizeof(mCsi)) ? mCsiLen : static_cast<int32>(sizeof(mCsi)) - 1] = 0;
				Dispatch(static_cast<char>(b));
				if (mState == Csi)
					mState = Ground;
				return;
			}
			if (b == 0x1b) { // sequence avortee
				mState = Esc;
				return;
			}
			if (mCsiLen < static_cast<int32>(sizeof(mCsi)) - 1)
				mCsi[mCsiLen++] = static_cast<char>(b);
		}

		void NkTerm::Osc_(unsigned char b) {
			if (b == 0x07) {
				FinOsc();
				mState = Ground;
				return;
			}
			if (b == 0x1b) {
				mState = OscEsc;
				return;
			}
			if (mOscLen < static_cast<int32>(sizeof(mOsc)) - 1)
				mOsc[mOscLen++] = static_cast<char>(b);
		}

		void NkTerm::FinOsc() {
			mOsc[mOscLen] = 0;
			const char *s = mOsc;
			int32 code = 0;
			while (*s >= '0' && *s <= '9')
				code = code * 10 + (*s++ - '0');
			if (*s == ';')
				++s;
			if (code == 0 || code == 2) {
				mTitre = NkString(s);
			} else if (code == 7) {
				mDossier = CheminDeFichierUri(s);
			} else if (code == 9 && s[0] == '9' && s[1] == ';') {
				// Windows Terminal : « 9;9;"C:\chemin" » (guillemets facultatifs).
				const char *p = s + 2;
				NkString d;
				for (; *p; ++p)
					if (*p != '"')
						d += *p;
				mDossier = d;
			} else if (code == 633 && s[0] == 'P' && s[1] == ';') {
				// VS Code : « 633;P;Cwd=/chemin ».
				const char *p = s + 2;
				if (p[0] == 'C' && p[1] == 'w' && p[2] == 'd' && p[3] == '=')
					mDossier = NkString(p + 4);
			}
			mOscLen = 0;
		}

		int32 NkTerm::Params(int32 *out, int32 maxN, char *prefixe, char *intermediaire) {
			*prefixe = 0;
			*intermediaire = 0;
			int32 n = 0, cur = 0;
			bool any = false;
			const char *p = mCsi;
			mCsi[mCsiLen] = 0;
			if (*p == '?' || *p == '>' || *p == '=' || *p == '<')
				*prefixe = *p++;
			for (; *p; ++p) {
				if (*p >= '0' && *p <= '9') {
					cur = cur * 10 + (*p - '0');
					any = true;
				} else if (*p == ';' || *p == ':') {
					// « : » est le separateur des sous-parametres (38:2::r:g:b) ; on
					// l'aplatit comme « ; », ce que font la plupart des emulateurs.
					if (n < maxN)
						out[n++] = any ? cur : -1;
					cur = 0;
					any = false;
				} else if (*p >= 0x20 && *p <= 0x2F) {
					*intermediaire = *p; // « SP q », « ! p »...
				}
			}
			if (n < maxN && (any || n > 0 || mCsiLen > 0))
				out[n++] = any ? cur : -1; // -1 = absent
			return n;
		}

		void NkTerm::Dispatch(char fin) {
			int32 p[32];
			char pre = 0, inter = 0;
			const int32 n = Params(p, 32, &pre, &inter);
			auto P = [&](int32 i, int32 def) { return (i < n && p[i] > 0) ? p[i] : def; };
			const int32 derniereCol = mCols - 1, derniereLigne = mRows - 1;
			if (inter == ' ' && fin == 'q') { // DECSCUSR : forme du curseur
				const int32 f = P(0, 1);
				mCurseurClignote = (f == 0 || f == 1 || f == 3 || f == 5);
				mCurseurForme = (f <= 2) ? NkTermCurseur::Bloc : (f <= 4 ? NkTermCurseur::Souligne : NkTermCurseur::Barre);
				return;
			}
			if (inter == '!' && fin == 'p') { // DECSTR : remise a zero douce
				mFg = mBg = kNkTermDefaut;
				mAttr = 0;
				mAppCursor = mBracketedPaste = false;
				mCursorVis = mAutoWrap = true;
				mTop = 0;
				mBot = derniereLigne;
				return;
			}
			// Un DEPLACEMENT annule le « retour a la ligne differe » ; une couleur
			// (SGR) ou un effacement ne le doivent PAS : un prompt colore qui
			// atteint la derniere colonne passerait sinon a la ligne trop tard.
			const char *kDeplacements = "HfABCDEFG`adeIZr";
			for (const char *k = kDeplacements; *k; ++k)
				if (*k == fin && mCx >= mCols)
					mCx = static_cast<int16>(derniereCol);
			switch (fin) {
				case 'H':
				case 'f':
					mCy = static_cast<int16>(Borne(P(0, 1) - 1, 0, derniereLigne));
					mCx = static_cast<int16>(Borne(P(1, 1) - 1, 0, derniereCol));
					break;
				case 'A':
					mCy = static_cast<int16>(Borne(mCy - P(0, 1), mCy >= mTop ? mTop : 0, derniereLigne));
					break;
				case 'B':
					mCy = static_cast<int16>(Borne(mCy + P(0, 1), 0, mCy <= mBot ? mBot : derniereLigne));
					break;
				case 'C':
				case 'a':
					mCx = static_cast<int16>(Borne(mCx + P(0, 1), 0, derniereCol));
					break;
				case 'D':
					mCx = static_cast<int16>(Borne(mCx - P(0, 1), 0, derniereCol));
					break;
				case 'E':
					mCx = 0;
					mCy = static_cast<int16>(Borne(mCy + P(0, 1), 0, derniereLigne));
					break;
				case 'F':
					mCx = 0;
					mCy = static_cast<int16>(Borne(mCy - P(0, 1), 0, derniereLigne));
					break;
				case 'G':
				case '`':
					mCx = static_cast<int16>(Borne(P(0, 1) - 1, 0, derniereCol));
					break;
				case 'd':
					mCy = static_cast<int16>(Borne(P(0, 1) - 1, 0, derniereLigne));
					break;
				case 'e':
					mCy = static_cast<int16>(Borne(mCy + P(0, 1), 0, derniereLigne));
					break;
				case 'I': // CHT : tabulations vers l'avant
					for (int32 k = 0; k < P(0, 1); ++k)
						mCx = static_cast<int16>(Borne((mCx / 8 + 1) * 8, 0, derniereCol));
					break;
				case 'Z': // CBT : tabulations vers l'arriere
					for (int32 k = 0; k < P(0, 1); ++k)
						mCx = static_cast<int16>(Borne(((mCx - 1) / 8) * 8, 0, derniereCol));
					break;
				case 'J':
					EraseDisplay(n > 0 && p[0] > 0 ? p[0] : 0);
					break;
				case 'K':
					EraseLine(n > 0 && p[0] > 0 ? p[0] : 0);
					break;
				case 'm':
					if (pre == 0)
						Sgr(p, n);
					break;
				case 'X': {
					EnsureWidth(mScreen[mCy]);
					const int32 x0 = mCx >= mCols ? derniereCol : mCx;
					for (int32 c = x0; c < x0 + P(0, 1) && c < mCols; ++c)
						mScreen[mCy][c] = Blank();
					break;
				}
				case 'P': {
					EnsureWidth(mScreen[mCy]);
					Line &ln = mScreen[mCy];
					const int32 x0 = mCx >= mCols ? derniereCol : mCx;
					const int32 k = Borne(P(0, 1), 0, mCols - x0);
					for (int32 c = x0; c < mCols - k; ++c)
						ln[c] = ln[c + k];
					for (int32 c = mCols - k; c < mCols; ++c)
						ln[c] = Blank();
					break;
				}
				case '@': {
					EnsureWidth(mScreen[mCy]);
					Line &ln = mScreen[mCy];
					const int32 x0 = mCx >= mCols ? derniereCol : mCx;
					const int32 k = Borne(P(0, 1), 0, mCols - x0);
					for (int32 c = mCols - 1; c >= x0 + k; --c)
						ln[c] = ln[c - k];
					for (int32 c = x0; c < x0 + k; ++c)
						ln[c] = Blank();
					break;
				}
				case 'L':
					if (mCy >= mTop && mCy <= mBot)
						for (int32 k = 0; k < P(0, 1); ++k) {
							for (int16 r = mBot; r > mCy; --r)
								mScreen[r] = static_cast<Line &&>(mScreen[r - 1]);
							BlankLine(mScreen[mCy]);
						}
					break;
				case 'M':
					if (mCy >= mTop && mCy <= mBot)
						for (int32 k = 0; k < P(0, 1); ++k) {
							for (int16 r = mCy; r < mBot; ++r)
								mScreen[r] = static_cast<Line &&>(mScreen[r + 1]);
							BlankLine(mScreen[mBot]);
						}
					break;
				case 'S':
					for (int32 k = 0; k < P(0, 1); ++k)
						ScrollUp();
					break;
				case 'T':
					if (pre == 0)
						for (int32 k = 0; k < P(0, 1); ++k)
							ScrollDown();
					break;
				case 'b': // REP : repete le dernier glyphe
					for (int32 k = 0; k < P(0, 1) && k < 4096; ++k)
						PutGlyph(mDernier);
					break;
				case 'c': // DA : « qui es-tu ? » -> VT102 avec options
					if (pre == 0)
						Repondre("\x1b[?1;2c");
					else if (pre == '>')
						Repondre("\x1b[>0;10;1c");
					break;
				case 'n': // DSR
					if (P(0, 0) == 5) {
						Repondre("\x1b[0n");
					} else if (P(0, 0) == 6) {
						char r[32];
						std::snprintf(r, sizeof(r), "\x1b[%d;%dR", static_cast<int>(mCy + 1),
									  static_cast<int>(CursorCol() + 1));
						Repondre(r);
					}
					break;
				case 'r':
					if (pre == 0) {
						mTop = static_cast<int16>(Borne(P(0, 1) - 1, 0, derniereLigne));
						mBot = static_cast<int16>(Borne(P(1, mRows) - 1, 0, derniereLigne));
						if (mBot <= mTop) {
							mTop = 0;
							mBot = static_cast<int16>(derniereLigne);
						}
						mCx = 0;
						mCy = 0;
					}
					break;
				case 's':
					if (pre == 0)
						Sauver();
					break;
				case 'u':
					if (pre == 0)
						Restaurer();
					break;
				case 'h':
				case 'l':
					Mode(pre, p, n, fin == 'h');
					break;
				default:
					break; // t (fenetre), g (tabulations) : sans objet
			}
		}

		void NkTerm::Mode(char prefixe, const int32 *p, int32 n, bool set) {
			if (prefixe != '?')
				return; // IRM (4), LNM (20) : non pris en charge
			for (int32 i = 0; i < n; ++i) {
				switch (p[i]) {
					case 1:
						mAppCursor = set;
						break;
					case 7:
						mAutoWrap = set;
						break;
					case 12:
						mCurseurClignote = set;
						break;
					case 25:
						mCursorVis = set;
						break;
					case 2004:
						mBracketedPaste = set;
						break;
					case 47:
					case 1047:
					case 1049:
						if (set && !mAlt) {
							if (p[i] == 1049)
								Sauver();
							mAltSave = mScreen;
							mAltCx = mCx;
							mAltCy = mCy;
							mAlt = true;
							for (usize r = 0; r < mScreen.Size(); ++r)
								BlankLine(mScreen[r]);
							mCx = mCy = 0;
						} else if (!set && mAlt) {
							mScreen = static_cast<NkVector<Line> &&>(mAltSave);
							mAltSave.Clear();
							// L'ecran principal a pu etre sauve a une autre taille.
							while (static_cast<int16>(mScreen.Size()) < mRows) {
								mScreen.PushBack(Line());
								BlankLine(mScreen[mScreen.Size() - 1]);
							}
							while (static_cast<int16>(mScreen.Size()) > mRows)
								mScreen.PopBack();
							for (usize r = 0; r < mScreen.Size(); ++r) {
								while (static_cast<int16>(mScreen[r].Size()) > mCols)
									mScreen[r].PopBack();
								EnsureWidth(mScreen[r]);
							}
							mCx = mAltCx;
							mCy = mAltCy;
							mAlt = false;
							if (p[i] == 1049)
								Restaurer();
							if (mCy >= mRows)
								mCy = mRows - 1;
						}
						break;
					default:
						break; // souris (1000..1006), focus (1004)... : sans objet
				}
			}
		}

		void NkTerm::EraseDisplay(int32 mode) {
			if (mode == 2 || mode == 3) {
				for (int16 r = 0; r < mRows; ++r)
					BlankLine(mScreen[r]);
				if (mode == 3) {
					mScroll.Clear();
					mTeteScroll = mNbScroll = 0;
				}
				return;
			}
			const int32 x = mCx >= mCols ? mCols - 1 : mCx;
			if (mode == 0) {
				EnsureWidth(mScreen[mCy]);
				for (int32 c = x; c < mCols; ++c)
					mScreen[mCy][c] = Blank();
				for (int16 r = static_cast<int16>(mCy + 1); r < mRows; ++r)
					BlankLine(mScreen[r]);
			} else if (mode == 1) {
				for (int16 r = 0; r < mCy; ++r)
					BlankLine(mScreen[r]);
				EnsureWidth(mScreen[mCy]);
				for (int32 c = 0; c <= x && c < mCols; ++c)
					mScreen[mCy][c] = Blank();
			}
		}

		void NkTerm::EraseLine(int32 mode) {
			EnsureWidth(mScreen[mCy]);
			const int32 x = mCx >= mCols ? mCols - 1 : mCx;
			int32 c0 = x, c1 = mCols - 1;
			if (mode == 1) {
				c0 = 0;
				c1 = x;
			} else if (mode == 2) {
				c0 = 0;
			}
			for (int32 c = c0; c <= c1 && c < mCols; ++c)
				mScreen[mCy][c] = Blank();
		}

		void NkTerm::Sgr(const int32 *p, int32 n) {
			if (n == 0 || (n == 1 && p[0] <= 0)) {
				mFg = mBg = kNkTermDefaut;
				mAttr = 0;
				return;
			}
			for (int32 i = 0; i < n; ++i) {
				const int32 c = p[i] < 0 ? 0 : p[i];
				if (c == 0) {
					mFg = mBg = kNkTermDefaut;
					mAttr = 0;
				} else if (c == 1) {
					mAttr |= NK_TERM_GRAS;
				} else if (c == 2) {
					mAttr |= NK_TERM_PALE;
				} else if (c == 3) {
					mAttr |= NK_TERM_ITALIQUE;
				} else if (c == 4) {
					mAttr |= NK_TERM_SOULIGNE;
				} else if (c == 7) {
					mAttr |= NK_TERM_INVERSE;
				} else if (c == 9) {
					mAttr |= NK_TERM_BARRE;
				} else if (c == 21 || c == 22) {
					mAttr &= static_cast<uint8>(~(NK_TERM_GRAS | NK_TERM_PALE));
				} else if (c == 23) {
					mAttr &= static_cast<uint8>(~NK_TERM_ITALIQUE);
				} else if (c == 24) {
					mAttr &= static_cast<uint8>(~NK_TERM_SOULIGNE);
				} else if (c == 27) {
					mAttr &= static_cast<uint8>(~NK_TERM_INVERSE);
				} else if (c == 29) {
					mAttr &= static_cast<uint8>(~NK_TERM_BARRE);
				} else if (c >= 30 && c <= 37) {
					mFg = NkTermIndice(static_cast<uint32>(c - 30));
				} else if (c >= 90 && c <= 97) {
					mFg = NkTermIndice(static_cast<uint32>(c - 90 + 8));
				} else if (c >= 40 && c <= 47) {
					mBg = NkTermIndice(static_cast<uint32>(c - 40));
				} else if (c >= 100 && c <= 107) {
					mBg = NkTermIndice(static_cast<uint32>(c - 100 + 8));
				} else if (c == 39) {
					mFg = kNkTermDefaut;
				} else if (c == 49) {
					mBg = kNkTermDefaut;
				} else if (c == 38 || c == 48) {
					uint32 col = kNkTermDefaut;
					bool ok = false;
					if (i + 2 < n && p[i + 1] == 5) {
						col = NkTermIndice(static_cast<uint32>(p[i + 2] < 0 ? 0 : p[i + 2]));
						i += 2;
						ok = true;
					} else if (i + 1 < n && p[i + 1] == 2) {
						// « 38;2;r;g;b » ou, depuis « 38:2::r:g:b », un champ
						// d'espace colorimetrique vide (-1) avant r.
						int32 j = i + 2;
						if (j + 3 < n && p[j] < 0)
							++j;
						if (j + 2 < n) {
							auto v = [&](int32 k) { return static_cast<uint32>(p[k] < 0 ? 0 : p[k]); };
							col = NkTermRgb(v(j), v(j + 1), v(j + 2));
							i = j + 2;
							ok = true;
						}
					}
					if (ok) {
						if (c == 38)
							mFg = col;
						else
							mBg = col;
					}
				}
			}
		}

	} // namespace editorkit
} // namespace nkentseu
