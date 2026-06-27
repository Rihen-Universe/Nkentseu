#pragma once
// =============================================================================
// NkTerm.h — Emulateur de terminal VT (sous-ensemble xterm) : transforme le flux
//   d'octets d'un shell (via NkPty) en une GRILLE de cellules (caractere + couleur
//   avant/arriere) avec curseur, defilement et scrollback. Gere les sequences :
//     - texte UTF-8, \r \n \b \t, BEL
//     - CSI : CUP/CUU/CUD/CUF/CUB/CHA/VPA, ED (clear), EL, SGR (couleurs 16/256/RGB),
//             ECH/ICH/DCH/IL/DL, DECSTBM (region), DECSC/DECRC, modes DEC (?h/?l)
//     - ecran alternatif (?1049) pour vim/htop ; OSC (titre) consomme/ignore.
//   Rendu : l'UI lit TotalLines()/LineAt() et dessine chaque cellule (monospace).
// =============================================================================
#include "NKGui/NKGui.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
namespace nkcode {

    using namespace nkentseu;
    using namespace nkentseu::nkgui;

    struct NkTermCell {
        uint32  cp = 0x20;                 // codepoint Unicode (espace par defaut)
        NkColor fg = { 204, 204, 204, 255 };
        NkColor bg = { 0, 0, 0, 0 };       // a==0 => fond par defaut (transparent)
    };

    class NkTerm {
    public:
        using Line = NkVector<NkTermCell>;

        NkTerm() { Resize(80, 24); }

        int16 Cols() const { return mCols; }
        int16 Rows() const { return mRows; }

        // (Re)dimensionne la grille. Conserve le scrollback ; recadre l'ecran.
        void Resize(int16 cols, int16 rows) {
            if (cols < 1) cols = 1; if (rows < 1) rows = 1;
            if (cols == mCols && rows == mRows && mScreen.Size() == static_cast<usize>(rows)) return;
            NkVector<Line> oldScreen = mScreen;   // copie l'ancien ecran
            mCols = cols; mRows = rows;
            mScreen.Clear();
            for (int16 r = 0; r < mRows; ++r) {
                mScreen.PushBack(Line());
                BlankLine(mScreen[mScreen.Size() - 1]);
            }
            // Recopie le contenu existant (coin haut-gauche).
            for (usize r = 0; r < oldScreen.Size() && r < static_cast<usize>(mRows); ++r)
                for (usize c = 0; c < oldScreen[r].Size() && c < static_cast<usize>(mCols); ++c)
                    mScreen[r][c] = oldScreen[r][c];
            mTop = 0; mBot = mRows - 1;
            if (mCx >= mCols) mCx = mCols - 1; if (mCy >= mRows) mCy = mRows - 1;
        }

        // Total des lignes affichables (scrollback + ecran) et acces indexe.
        usize TotalLines() const { return mScroll.Size() + mScreen.Size(); }
        const Line& LineAt(usize i) const {
            return (i < mScroll.Size()) ? mScroll[i] : mScreen[i - mScroll.Size()];
        }
        // Position absolue du curseur (pour le dessiner).
        usize CursorLine() const { return mScroll.Size() + static_cast<usize>(mCy); }
        int16 CursorCol()  const { return mCx; }
        bool  CursorVisible() const { return mCursorVis; }

        // Injecte des octets bruts du shell.
        void Feed(const char* data, usize len) {
            for (usize i = 0; i < len; ++i) Byte(static_cast<unsigned char>(data[i]));
        }

        void Clear() {                       // reset complet (scrollback + ecran)
            mScroll.Clear();
            for (usize r = 0; r < mScreen.Size(); ++r) BlankLine(mScreen[r]);
            mCx = mCy = 0; mState = Ground;
        }

    private:
        enum State { Ground, Esc, Csi, Osc, OscEsc, EscArg };   // EscArg = consomme 1 octet

        void BlankLine(Line& ln) {
            ln.Clear();
            for (int16 c = 0; c < mCols; ++c) ln.PushBack(NkTermCell{});
        }
        void EnsureWidth(Line& ln) { while (static_cast<int16>(ln.Size()) < mCols) ln.PushBack(NkTermCell{}); }

        NkTermCell Blank() const { NkTermCell c; c.bg = mBg; return c; }   // espace avec fond courant

        // ── Defilement ────────────────────────────────────────────────────────
        void ScrollUp() {
            // Region [mTop, mBot] : la ligne du haut part au scrollback si region pleine.
            if (mTop == 0 && !mAlt) {
                mScroll.PushBack(mScreen[0]);
                while (mScroll.Size() > 5000) mScroll.Erase(mScroll.Begin());
            }
            for (int16 r = mTop; r < mBot; ++r) mScreen[r] = mScreen[r + 1];
            BlankLine(mScreen[mBot]);
        }
        void ScrollDown() {                  // RI / IL : descend la region, vide le haut
            for (int16 r = mBot; r > mTop; --r) mScreen[r] = mScreen[r - 1];
            BlankLine(mScreen[mTop]);
        }
        void LineFeed() { if (mCy >= mBot) ScrollUp(); else ++mCy; }

        void PutGlyph(uint32 cp) {
            if (mCx >= mCols) { mCx = 0; LineFeed(); }
            EnsureWidth(mScreen[mCy]);
            NkTermCell& cell = mScreen[mCy][mCx];
            cell.cp = cp; cell.fg = mFg; cell.bg = mBg;
            ++mCx;
        }

        // ── Octet a octet (avec decodage UTF-8 + machine a etats VT) ───────────
        void Byte(unsigned char b) {
            switch (mState) {
                case Ground:  Ground_(b); break;
                case Esc:     Esc_(b);    break;
                case Csi:     Csi_(b);    break;
                case Osc:     if (b == 0x07) mState = Ground; else if (b == 0x1b) mState = OscEsc; break;
                case OscEsc:  mState = (b == '\\') ? Ground : Osc; break;
                case EscArg:  mState = Ground; break;            // 1 octet apres ESC ( ) * +
            }
        }

        void Ground_(unsigned char b) {
            if (mU8need) {                                   // suite d'une sequence UTF-8
                if ((b & 0xC0) == 0x80) { mU8 = (mU8 << 6) | (b & 0x3F); if (--mU8need == 0) PutGlyph(mU8); return; }
                mU8need = 0;                                  // sequence cassee -> retombe
            }
            if (b == 0x1b) { mState = Esc; return; }
            if (b == '\r') { mCx = 0; return; }
            if (b == '\n' || b == 0x0b || b == 0x0c) { LineFeed(); return; }
            if (b == '\b') { if (mCx > 0) --mCx; return; }
            if (b == '\t') { int16 n = static_cast<int16>((mCx / 8 + 1) * 8); mCx = n >= mCols ? mCols - 1 : n; return; }
            if (b == 0x07) return;                            // BEL
            if (b < 0x20) return;                             // autres controles ignores
            if (b < 0x80) { PutGlyph(b); return; }
            // Debut d'une sequence UTF-8.
            if ((b & 0xE0) == 0xC0) { mU8 = b & 0x1F; mU8need = 1; }
            else if ((b & 0xF0) == 0xE0) { mU8 = b & 0x0F; mU8need = 2; }
            else if ((b & 0xF8) == 0xF0) { mU8 = b & 0x07; mU8need = 3; }
            else PutGlyph(0xFFFD);
        }

        void Esc_(unsigned char b) {
            switch (b) {
                case '[': mCsiLen = 0; mState = Csi; return;
                case ']': mState = Osc; return;               // OSC (titre...) : consomme
                case 'M': if (mCy <= mTop) ScrollDown(); else --mCy; mState = Ground; return;   // RI
                case '7': mSaveCx = mCx; mSaveCy = mCy; mState = Ground; return;                 // DECSC
                case '8': mCx = mSaveCx; mCy = mSaveCy; mState = Ground; return;                 // DECRC
                case '(': case ')': case '*': case '+': mState = EscArg; return;                 // jeu de car. : 1 octet
                default:  mState = Ground; return;
            }
        }

        void Csi_(unsigned char b) {
            if (b >= 0x40 && b <= 0x7E) {                     // octet final
                mCsi[mCsiLen < (int32)sizeof(mCsi) ? mCsiLen : (int32)sizeof(mCsi) - 1] = 0;
                Dispatch(static_cast<char>(b));
                mState = Ground;
                return;
            }
            if (mCsiLen < static_cast<int32>(sizeof(mCsi)) - 1) mCsi[mCsiLen++] = static_cast<char>(b);
        }

        // Parse les parametres numeriques de mCsi (hors prefixe '?'). N <= 16.
        int32 Params(int32* out, int32 maxN, bool* priv) {
            if (priv) *priv = false;
            int32 n = 0; int32 cur = 0; bool any = false;
            const char* p = mCsi; mCsi[mCsiLen] = 0;
            if (*p == '?') { if (priv) *priv = true; ++p; }
            for (; *p; ++p) {
                if (*p >= '0' && *p <= '9') { cur = cur * 10 + (*p - '0'); any = true; }
                else if (*p == ';') { if (n < maxN) out[n++] = any ? cur : 0; cur = 0; any = false; }
            }
            if (n < maxN) out[n++] = any ? cur : (n == 0 ? -1 : 0);   // -1 = "absent" pour 1er param
            return n;
        }

        void Dispatch(char fin) {
            int32 p[16]; bool priv = false;
            const int32 n = Params(p, 16, &priv);
            auto P = [&](int32 i, int32 def) { return (i < n && p[i] > 0) ? p[i] : def; };
            switch (fin) {
                case 'H': case 'f': { int32 r = P(0,1), c = P(1,1); mCy = Clamp(r-1,0,mRows-1); mCx = Clamp(c-1,0,mCols-1); } break;
                case 'A': mCy = Clamp(mCy - P(0,1), 0, mRows-1); break;
                case 'B': mCy = Clamp(mCy + P(0,1), 0, mRows-1); break;
                case 'C': mCx = Clamp(mCx + P(0,1), 0, mCols-1); break;
                case 'D': mCx = Clamp(mCx - P(0,1), 0, mCols-1); break;
                case 'E': mCx = 0; mCy = Clamp(mCy + P(0,1), 0, mRows-1); break;
                case 'F': mCx = 0; mCy = Clamp(mCy - P(0,1), 0, mRows-1); break;
                case 'G': mCx = Clamp(P(0,1)-1, 0, mCols-1); break;
                case 'd': mCy = Clamp(P(0,1)-1, 0, mRows-1); break;
                case 'J': EraseDisplay((n>0 && p[0]>0) ? p[0] : 0); break;
                case 'K': EraseLine((n>0 && p[0]>0) ? p[0] : 0); break;
                case 'm': Sgr(p, n); break;
                case 'X': EraseChars(P(0,1)); break;
                case 'P': DeleteChars(P(0,1)); break;
                case '@': InsertChars(P(0,1)); break;
                case 'L': InsertLines(P(0,1)); break;
                case 'M': DeleteLines(P(0,1)); break;
                case 'r': mTop = Clamp(P(0,1)-1,0,mRows-1); mBot = Clamp(P(1,mRows)-1,0,mRows-1); if (mBot < mTop) mBot = mTop; mCx = 0; mCy = mTop; break;
                case 's': mSaveCx = mCx; mSaveCy = mCy; break;
                case 'u': mCx = mSaveCx; mCy = mSaveCy; break;
                case 'h': case 'l': Mode(priv, p, n, fin == 'h'); break;
                default: break;
            }
        }

        void Mode(bool priv, int32* p, int32 n, bool set) {
            if (!priv) return;
            for (int32 i = 0; i < n; ++i) {
                if (p[i] == 25) mCursorVis = set;                       // ?25 curseur
                else if (p[i] == 1049 || p[i] == 47 || p[i] == 1047) {  // ecran alternatif
                    if (set && !mAlt) { mAltSave = mScreen; mAltCx = mCx; mAltCy = mCy; mAlt = true;
                                        for (usize r = 0; r < mScreen.Size(); ++r) BlankLine(mScreen[r]); mCx = mCy = 0; }
                    else if (!set && mAlt) { mScreen = mAltSave; mCx = mAltCx; mCy = mAltCy; mAlt = false; }
                }
            }
        }

        void EraseDisplay(int32 mode) {
            if (mode == 2 || mode == 3) {
                for (int16 r = 0; r < mRows; ++r) BlankLine(mScreen[r]);
                if (mode == 3) mScroll.Clear();
                return;
            }
            if (mode == 0) {                                  // du curseur a la fin
                EnsureWidth(mScreen[mCy]);
                for (int16 c = mCx; c < mCols; ++c) mScreen[mCy][c] = Blank();
                for (int16 r = mCy + 1; r < mRows; ++r) BlankLine(mScreen[r]);
            } else if (mode == 1) {                           // du debut au curseur
                for (int16 r = 0; r < mCy; ++r) BlankLine(mScreen[r]);
                EnsureWidth(mScreen[mCy]);
                for (int16 c = 0; c <= mCx && c < mCols; ++c) mScreen[mCy][c] = Blank();
            }
        }
        void EraseLine(int32 mode) {
            EnsureWidth(mScreen[mCy]);
            if (mode == 2) { for (int16 c = 0; c < mCols; ++c) mScreen[mCy][c] = Blank(); }
            else if (mode == 1) { for (int16 c = 0; c <= mCx && c < mCols; ++c) mScreen[mCy][c] = Blank(); }
            else { for (int16 c = mCx; c < mCols; ++c) mScreen[mCy][c] = Blank(); }
        }
        void EraseChars(int32 cnt) {
            EnsureWidth(mScreen[mCy]);
            for (int16 c = mCx; c < mCx + cnt && c < mCols; ++c) mScreen[mCy][c] = Blank();
        }
        void DeleteChars(int32 cnt) {
            EnsureWidth(mScreen[mCy]); Line& ln = mScreen[mCy];
            for (int32 k = 0; k < cnt; ++k) { for (int16 c = mCx; c < mCols - 1; ++c) ln[c] = ln[c+1]; ln[mCols-1] = Blank(); }
        }
        void InsertChars(int32 cnt) {
            EnsureWidth(mScreen[mCy]); Line& ln = mScreen[mCy];
            for (int32 k = 0; k < cnt; ++k) { for (int16 c = mCols - 1; c > mCx; --c) ln[c] = ln[c-1]; ln[mCx] = Blank(); }
        }
        void InsertLines(int32 cnt) {
            for (int32 k = 0; k < cnt; ++k) { for (int16 r = mBot; r > mCy; --r) mScreen[r] = mScreen[r-1]; BlankLine(mScreen[mCy]); }
        }
        void DeleteLines(int32 cnt) {
            for (int32 k = 0; k < cnt; ++k) { for (int16 r = mCy; r < mBot; ++r) mScreen[r] = mScreen[r+1]; BlankLine(mScreen[mBot]); }
        }

        // ── SGR (couleurs / attributs) ─────────────────────────────────────────
        void Sgr(int32* p, int32 n) {
            if (n == 0 || (n == 1 && p[0] <= 0)) { mFg = DefFg(); mBg = DefBg(); mBold = false; return; }
            for (int32 i = 0; i < n; ++i) {
                int32 c = p[i] < 0 ? 0 : p[i];
                if (c == 0) { mFg = DefFg(); mBg = DefBg(); mBold = false; }
                else if (c == 1) { mBold = true; }
                else if (c == 22) { mBold = false; }
                else if (c == 7) { NkColor t = mFg; mFg = (mBg.a ? mBg : DefBg2()); mBg = t; }   // reverse approx
                else if (c == 39) mFg = DefFg();
                else if (c == 49) mBg = DefBg();
                else if (c >= 30 && c <= 37) mFg = Ansi16(c - 30 + (mBold ? 8 : 0));
                else if (c >= 90 && c <= 97) mFg = Ansi16(c - 90 + 8);
                else if (c >= 40 && c <= 47) mBg = Ansi16(c - 40);
                else if (c >= 100 && c <= 107) mBg = Ansi16(c - 100 + 8);
                else if (c == 38 || c == 48) {                 // 256 / RGB
                    NkColor col = mFg;
                    if (i + 1 < n && p[i+1] == 5 && i + 2 < n) { col = Xterm256(p[i+2]); i += 2; }
                    else if (i + 1 < n && p[i+1] == 2 && i + 4 < n) { col = NkColor{ (uint8)p[i+2], (uint8)p[i+3], (uint8)p[i+4], 255 }; i += 4; }
                    if (c == 38) mFg = col; else mBg = col;
                }
            }
        }

        static NkColor DefFg()  { return { 204, 204, 204, 255 }; }
        static NkColor DefBg()  { return { 0, 0, 0, 0 }; }       // transparent (fond terminal)
        static NkColor DefBg2() { return { 13, 17, 23, 255 }; }  // fond opaque pour reverse
        static int32   Clamp(int32 v, int32 lo, int32 hi) { return v < lo ? lo : (v > hi ? hi : v); }

        static NkColor Ansi16(int32 i) {
            static const NkColor t[16] = {
                {  1,   4,   9, 255 }, { 248,  81,  73, 255 }, {  63, 185,  80, 255 }, { 210, 153,  34, 255 },
                { 88, 166, 255, 255 }, { 188, 140, 255, 255 }, {  57, 200, 214, 255 }, { 204, 204, 204, 255 },
                {110, 118, 129, 255 }, { 255, 123, 114, 255 }, {  86, 211, 100, 255 }, { 233, 196, 106, 255 },
                {121, 192, 255, 255 }, { 210, 168, 255, 255 }, {  86, 221, 232, 255 }, { 255, 255, 255, 255 },
            };
            return t[i < 0 ? 0 : (i > 15 ? 15 : i)];
        }
        static NkColor Xterm256(int32 idx) {
            if (idx < 16) return Ansi16(idx);
            if (idx >= 232) { uint8 v = static_cast<uint8>(8 + 10 * (idx - 232)); return { v, v, v, 255 }; }
            int32 i = idx - 16; int32 r = (i / 36) % 6, g = (i / 6) % 6, b = i % 6;
            auto ch = [](int32 v) -> uint8 { return static_cast<uint8>(v == 0 ? 0 : 55 + 40 * v); };
            return { ch(r), ch(g), ch(b), 255 };
        }

        // Etat.
        NkVector<Line> mScreen;             // lignes visibles (mRows)
        NkVector<Line> mScroll;             // scrollback
        NkVector<Line> mAltSave;            // ecran principal sauve (ecran alternatif)
        int16 mCols = 80, mRows = 24;
        int16 mCx = 0, mCy = 0;
        int16 mTop = 0, mBot = 23;          // region de defilement
        int16 mSaveCx = 0, mSaveCy = 0;
        int16 mAltCx = 0, mAltCy = 0;
        NkColor mFg = DefFg(), mBg = DefBg();
        bool  mBold = false;
        bool  mCursorVis = true;
        bool  mAlt = false;

        // Machine a etats + decodage UTF-8.
        State mState = Ground;
        char  mCsi[64] = {};
        int32 mCsiLen = 0;
        uint32 mU8 = 0; int32 mU8need = 0;
    };

} // namespace nkcode
} // namespace nkentseu
