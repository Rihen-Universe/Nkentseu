/**
 * @File    NkCFFParser.cpp
 * @Brief   Implémentation du parser CFF / interpréteur Type 2.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 */

#include "pch.h"
#include "NKFont/NkCFFParser.h"
#include <cstring>
#include <cmath>   // sqrtf — seule utilisation autorisée pour l'opérateur sqrt Type2

namespace nkentseu {

// ─────────────────────────────────────────────────────────────────────────────
//  Utilitaires
// ─────────────────────────────────────────────────────────────────────────────

uint32 NkCFFParser::CalcSubrBias(uint32 count) noexcept {
    if (count <  1240) return 107;
    if (count < 33900) return 1131;
    return 32768;
}

// Lit un entier varié depuis un DICT CFF (format DICT encoding)
static bool ReadDictInt(NkStreamReader& s, int32& val) noexcept {
    if (!s.HasBytes(1)) return false;
    const uint8 b = s.ReadU8();
    if      (b >= 32  && b <= 246)  { val = static_cast<int32>(b) - 139; return true; }
    else if (b >= 247 && b <= 250)  { val = (static_cast<int32>(b)-247)*256 + s.ReadU8() + 108; return true; }
    else if (b >= 251 && b <= 254)  { val = -(static_cast<int32>(b)-251)*256 - s.ReadU8() - 108; return true; }
    else if (b == 28) { val = s.ReadI16(); return true; }
    else if (b == 29) { val = s.ReadI32(); return true; }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Parse — point d'entrée
// ─────────────────────────────────────────────────────────────────────────────

bool NkCFFParser::Parse(
    const uint8* data, usize size,
    NkMemArena& arena, NkCFFFont& out) noexcept
{
    ::memset(&out, 0, sizeof(out));
    if (!data || size < 4) return false;

    out.rawData = data;
    out.rawSize = size;

    NkStreamReader s(data, size);

    // ── Header ────────────────────────────────────────────────────────────────
    uint32 hdrSize = 0; uint8 offSize = 0;
    if (!ParseHeader(s, hdrSize, offSize)) return false;
    s.Seek(hdrSize);

    // ── Name INDEX (on skip — une seule police dans la table CFF d'un OTF) ───
    {
        NkCFFIndexEntry* nameEntries = nullptr; uint32 nameCount = 0;
        if (!ParseINDEX(s, nameEntries, nameCount, arena)) return false;
        if (nameCount == 0) return false;
        // Skip name data
        if (nameCount > 0 && nameEntries)
            s.Seek(nameEntries[nameCount-1].offset + nameEntries[nameCount-1].length);
    }

    // ── Top DICT INDEX ────────────────────────────────────────────────────────
    {
        NkCFFIndexEntry* tdEntries = nullptr; uint32 tdCount = 0;
        if (!ParseINDEX(s, tdEntries, tdCount, arena)) return false;
        if (tdCount == 0 || !tdEntries) return false;
        const uint32 tdOff = tdEntries[0].offset;
        const uint32 tdLen = tdEntries[0].length;
        if (tdOff + tdLen > size) return false;
        if (!ParseTopDict(data + tdOff, tdLen, out.topDict)) return false;
    }

    // ── String INDEX (skip — on n'a pas besoin des noms de glyphes) ──────────
    {
        NkCFFIndexEntry* strEntries = nullptr; uint32 strCount = 0;
        ParseINDEX(s, strEntries, strCount, arena);
    }

    // ── Global Subr INDEX ─────────────────────────────────────────────────────
    {
        const usize gsubrPos = s.Tell();
        NkCFFIndexEntry* gEntries = nullptr; uint32 gCount = 0;
        ParseINDEX(s, gEntries, gCount, arena);
        out.globalSubrs     = data + gsubrPos;
        out.globalSubrsSize = static_cast<uint32>(s.Tell() - gsubrPos);
        out.globalSubrBias  = CalcSubrBias(gCount);
        (void)gEntries;
    }

    // ── CharStrings INDEX ─────────────────────────────────────────────────────
    if (out.topDict.charStringsOffset <= 0) return false;
    {
        NkStreamReader cs(data + out.topDict.charStringsOffset,
                          size  - out.topDict.charStringsOffset);
        if (!ParseINDEX(cs, out.charStrings, out.numGlyphs, arena)) return false;
        // Corrige les offsets pour être absolus dans rawData
        const uint32 base = static_cast<uint32>(out.topDict.charStringsOffset);
        for (uint32 i = 0; i < out.numGlyphs; ++i)
            out.charStrings[i].offset += base;
    }

    // ── Private DICT + Local Subrs ────────────────────────────────────────────
    if (out.topDict.privateOffset > 0 && out.topDict.privateLength > 0) {
        const uint32 pOff = static_cast<uint32>(out.topDict.privateOffset);
        const uint32 pLen = static_cast<uint32>(out.topDict.privateLength);
        if (pOff + pLen <= size)
            ParsePrivateDict(data + pOff, pLen, out.topDict);

        if (out.topDict.subrsOffset > 0) {
            const uint32 lsOff = pOff + static_cast<uint32>(out.topDict.subrsOffset);
            out.localSubrs     = data + lsOff;
            out.localSubrsSize = (lsOff < size) ? static_cast<uint32>(size - lsOff) : 0;
            // Compte les subrs pour calculer le biais
            if (out.localSubrsSize >= 2) {
                NkStreamReader ls(out.localSubrs, out.localSubrsSize);
                const uint16 lCount = ls.ReadU16();
                out.localSubrBias = CalcSubrBias(lCount);
            }
        }
    }

    out.isValid = true;
    return true;
}

bool NkCFFParser::ParseHeader(NkStreamReader& s, uint32& hdrSize, uint8& offSize) noexcept {
    s.ReadU8(); // major
    s.ReadU8(); // minor
    hdrSize = s.ReadU8();
    offSize = s.ReadU8();
    return !s.HasError();
}

bool NkCFFParser::ParseINDEX(
    NkStreamReader& s,
    NkCFFIndexEntry*& entries, uint32& count,
    NkMemArena& arena) noexcept
{
    count   = 0;
    entries = nullptr;
    const uint16 rawCount = s.ReadU16();
    if (s.HasError()) return false;
    if (rawCount == 0) return true; // INDEX vide valide

    count = rawCount;
    const uint8 offSize = s.ReadU8();
    if (offSize < 1 || offSize > 4) return false;

    entries = arena.Alloc<NkCFFIndexEntry>(count);
    if (!entries) return false;

    // Lit count+1 offsets
    uint32* offsets = arena.Alloc<uint32>(count + 1);
    if (!offsets) return false;
    for (uint32 i = 0; i <= count; ++i) {
        uint32 off = 0;
        for (uint8 b = 0; b < offSize; ++b)
            off = (off << 8) | s.ReadU8();
        offsets[i] = off;
    }

    const usize dataStart = s.Tell(); // début des données INDEX
    for (uint32 i = 0; i < count; ++i) {
        entries[i].offset = static_cast<uint32>(dataStart) + offsets[i] - 1;
        entries[i].length = offsets[i+1] - offsets[i];
    }
    // Skip les données
    if (count > 0)
        s.Seek(dataStart + offsets[count] - 1);

    return !s.HasError();
}

bool NkCFFParser::ParseTopDict(
    const uint8* data, uint32 size, NkCFFTopDict& out) noexcept
{
    ::memset(&out, 0, sizeof(out));
    out.charStringsOffset = -1;
    NkStreamReader s(data, size);

    float stack[48]; int32 stackTop = 0;

    while (!s.IsEOF() && !s.HasError()) {
        const uint8 b = s.ReadU8();

        if (b == 12) { // Escape
            const uint8 b2 = s.ReadU8();
            switch (b2) {
                case 30: out.isCIDFont = true; break;
                default: stackTop = 0; break;
            }
            stackTop = 0;
            continue;
        }
        // Opérateurs simples
        if (b <= 21) {
            switch (b) {
                case 17: // CharStrings
                    if (stackTop >= 1) out.charStringsOffset = static_cast<int32>(stack[stackTop-1]);
                    break;
                case 18: // Private
                    if (stackTop >= 2) {
                        out.privateLength = static_cast<int32>(stack[stackTop-2]);
                        out.privateOffset = static_cast<int32>(stack[stackTop-1]);
                    }
                    break;
                case 15: // charset
                    if (stackTop >= 1) out.charsetOffset = static_cast<int32>(stack[stackTop-1]);
                    break;
                case 16: // Encoding
                    if (stackTop >= 1) out.encodingOffset = static_cast<int32>(stack[stackTop-1]);
                    break;
            }
            stackTop = 0;
            continue;
        }
        // Nombres
        if (b == 30) { // Real number (BCD) — converti en float approx
            float real = 0.f; bool neg = false; bool frac = false;
            float fracDiv = 1.f;
            while (!s.IsEOF()) {
                const uint8 nb = s.ReadU8();
                auto processNibble = [&](uint8 n) {
                    if      (n < 10)  { if (frac) { fracDiv *= 10.f; real += n / fracDiv; } else real = real*10.f + n; }
                    else if (n == 0xa) { frac = true; }
                    else if (n == 0xb) { /* E+ */ }
                    else if (n == 0xc) { /* E- */ }
                    else if (n == 0xe) { neg = true; }
                };
                processNibble((nb >> 4) & 0xF);
                if ((nb & 0xF) == 0xF) break;
                processNibble(nb & 0xF);
            }
            if (stackTop < 48) stack[stackTop++] = neg ? -real : real;
        } else {
            int32 ival = 0;
            s.Seek(s.Tell() - 1);
            if (!ReadDictInt(s, ival)) { stackTop = 0; continue; }
            if (stackTop < 48) stack[stackTop++] = static_cast<float>(ival);
        }
    }
    return out.charStringsOffset > 0;
}

bool NkCFFParser::ParsePrivateDict(
    const uint8* data, uint32 size, NkCFFTopDict& out) noexcept
{
    NkStreamReader s(data, size);
    float stack[48]; int32 stackTop = 0;

    while (!s.IsEOF() && !s.HasError()) {
        const uint8 b = s.ReadU8();
        if (b == 12) {
            s.ReadU8(); // escape operand — skip
            stackTop = 0; continue;
        }
        if (b <= 21) {
            switch (b) {
                case 20: // defaultWidthX
                    if (stackTop >= 1) out.defaultWidthX = static_cast<int32>(stack[stackTop-1]);
                    break;
                case 21: // nominalWidthX
                    if (stackTop >= 1) out.nominalWidthX = static_cast<int32>(stack[stackTop-1]);
                    break;
                case 19: // Subrs
                    if (stackTop >= 1) out.subrsOffset = static_cast<int32>(stack[stackTop-1]);
                    break;
            }
            stackTop = 0; continue;
        }
        int32 ival = 0;
        s.Seek(s.Tell() - 1);
        if (!ReadDictInt(s, ival)) { stackTop = 0; continue; }
        if (stackTop < 48) stack[stackTop++] = static_cast<float>(ival);
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  ParseGlyph
// ─────────────────────────────────────────────────────────────────────────────

bool NkCFFParser::ParseGlyph(
    const NkCFFFont& font, uint32 glyphId,
    NkMemArena& arena, NkCFFGlyph& out) noexcept
{
    ::memset(&out, 0, sizeof(out));
    if (!font.isValid || glyphId >= font.numGlyphs) return false;

    const NkCFFIndexEntry& entry = font.charStrings[glyphId];
    if (entry.offset + entry.length > font.rawSize) return false;

    const uint32 maxPts = 4096;
    const uint32 maxCnt = 256;

    T2State state;
    ::memset(&state, 0, sizeof(state));
    state.defaultWidth = static_cast<float>(font.topDict.defaultWidthX);
    state.nominalWidth = static_cast<float>(font.topDict.nominalWidthX);
    state.advanceWidth = state.defaultWidth;
    state.points        = arena.Alloc<NkCFFPoint>(maxPts);
    state.contourStarts = arena.Alloc<uint32>(maxCnt);
    state.maxPoints     = maxPts;
    state.maxContours   = maxCnt;
    state.arena         = &arena;

    if (!state.points || !state.contourStarts) return false;

    const uint8* cs    = font.rawData + entry.offset;
    const uint32 csLen = entry.length;
    if (!InterpretCharString(cs, csLen, font, state, 0)) return false;
    T2_ClosePath(state);

    // Construit les contours
    out.advanceWidth = state.advanceWidth;
    out.numContours  = state.numContours;
    out.contours     = arena.Alloc<NkCFFContour>(state.numContours);
    if (!out.contours) return false;

    for (uint32 c = 0; c < state.numContours; ++c) {
        const uint32 start = state.contourStarts[c];
        const uint32 end   = (c + 1 < state.numContours)
                           ? state.contourStarts[c + 1]
                           : state.numPoints;
        out.contours[c].points    = state.points + start;
        out.contours[c].numPoints = end - start;
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Interpréteur Type 2
// ─────────────────────────────────────────────────────────────────────────────

void NkCFFParser::T2_PushPoint(T2State& s, float x, float y, bool onCurve) noexcept {
    if (s.numPoints >= s.maxPoints) return;
    s.points[s.numPoints++] = {x, y, onCurve};
}

void NkCFFParser::T2_MoveTo(T2State& s, float dx, float dy) noexcept {
    T2_ClosePath(s);
    s.x += dx; s.y += dy;
    if (s.numContours < s.maxContours)
        s.contourStarts[s.numContours++] = s.numPoints;
    T2_PushPoint(s, s.x, s.y, true);
    s.inPath = true;
}

void NkCFFParser::T2_LineTo(T2State& s, float dx, float dy) noexcept {
    s.x += dx; s.y += dy;
    T2_PushPoint(s, s.x, s.y, true);
}

void NkCFFParser::T2_CurveTo(T2State& s,
    float dx1, float dy1, float dx2, float dy2, float dx3, float dy3) noexcept
{
    T2_PushPoint(s, s.x + dx1,             s.y + dy1,             false);
    T2_PushPoint(s, s.x + dx1 + dx2,       s.y + dy1 + dy2,       false);
    s.x += dx1 + dx2 + dx3;
    s.y += dy1 + dy2 + dy3;
    T2_PushPoint(s, s.x, s.y, true);
}

void NkCFFParser::T2_ClosePath(T2State& s) noexcept {
    if (!s.inPath) return;
    // Le point de fermeture = premier point du contour courant
    // (déjà présent — le rasterizer ferme implicitement)
    s.inPath = false;
}

float NkCFFParser::T2_ReadNumber(NkStreamReader& sr, uint8 b0) noexcept {
    if      (b0 >= 32  && b0 <= 246)  return static_cast<float>(b0) - 139.f;
    else if (b0 >= 247 && b0 <= 250)  return static_cast<float>((b0-247)*256 + sr.ReadU8() + 108);
    else if (b0 >= 251 && b0 <= 254)  return static_cast<float>(-(b0-251)*256 - sr.ReadU8() - 108);
    else if (b0 == 28) return static_cast<float>(sr.ReadI16());
    else if (b0 == 29) return static_cast<float>(sr.ReadI32());
    else if (b0 == 255) {
        // Fixed 16.16
        const int32 raw = sr.ReadI32();
        return static_cast<float>(raw) / 65536.f;
    }
    return 0.f;
}

bool NkCFFParser::T2_CallSubr(
    const NkCFFFont& font, bool isGlobal, int32 idx,
    T2State& state, int32 depth) noexcept
{
    if (depth > 10) return false;

    const uint8* subrData = isGlobal ? font.globalSubrs : font.localSubrs;
    const uint32 subrSize = isGlobal ? font.globalSubrsSize : font.localSubrsSize;
    const uint32 bias     = isGlobal ? font.globalSubrBias : font.localSubrBias;
    if (!subrData || subrSize < 2) return false;

    NkStreamReader idx_s(subrData, subrSize);
    const uint16 count = idx_s.ReadU16();
    if (count == 0) return false;
    const uint32 biasedIdx = static_cast<uint32>(idx + static_cast<int32>(bias));
    if (biasedIdx >= count) return false;

    const uint8 offSize = idx_s.ReadU8();
    if (offSize < 1 || offSize > 4) return false;

    // Lit les offsets pour biasedIdx et biasedIdx+1
    uint32 offA = 0, offB = 0;
    for (uint32 i = 0; i <= biasedIdx + 1; ++i) {
        uint32 off = 0;
        for (uint8 b = 0; b < offSize; ++b) off = (off << 8) | idx_s.ReadU8();
        if (i == biasedIdx)     offA = off;
        if (i == biasedIdx + 1) offB = off;
    }
    const usize dataBase = idx_s.Tell();
    const uint32 csOff   = static_cast<uint32>(dataBase) + offA - 1;
    const uint32 csLen   = offB - offA;
    if (csOff + csLen > subrSize) return false;

    return InterpretCharString(subrData + csOff, csLen, font, state, depth + 1);
}

bool NkCFFParser::InterpretCharString(
    const uint8* cs, uint32 csLen,
    const NkCFFFont& font, T2State& state, int32 depth) noexcept
{
    NkStreamReader s(cs, csLen);

    while (!s.IsEOF() && !s.HasError()) {
        const uint8 b = s.ReadU8();

        // Nombres
        if ((b >= 32 && b <= 254) || b == 28 || b == 255 || b == 29) {
            const float v = T2_ReadNumber(s, b);
            if (state.stackTop < 48) state.stack[state.stackTop++] = v;
            continue;
        }

        float* stk = state.stack;
        int32& top = state.stackTop;

        // Lecture de la largeur implicite (premier arg de moveto si top est impair pour hstem etc.)
        auto ReadWidth = [&]() {
            if (!state.widthRead) {
                state.widthRead = true;
                // La largeur est présente si le nombre d'arguments est impair
                // pour les opérateurs qui normalement reçoivent un nombre pair
                state.advanceWidth = state.nominalWidth + stk[0];
            }
        };

        switch (b) {
            // ── Fin ─────────────────────────────────────────────────────────
            case 14: // endchar
                T2_ClosePath(state);
                return true;

            // ── Mouvements ───────────────────────────────────────────────────
            case 21: // rmoveto
                if (top > 2) ReadWidth();
                T2_MoveTo(state, stk[top-2], stk[top-1]);
                top = 0; break;
            case 22: // hmoveto
                if (top > 1) ReadWidth();
                T2_MoveTo(state, stk[top-1], 0.f);
                top = 0; break;
            case 4:  // vmoveto
                if (top > 1) ReadWidth();
                T2_MoveTo(state, 0.f, stk[top-1]);
                top = 0; break;

            // ── Lignes ──────────────────────────────────────────────────────
            case 5:  // rlineto
                for (int32 i = 0; i + 1 < top; i += 2)
                    T2_LineTo(state, stk[i], stk[i+1]);
                top = 0; break;
            case 6:  // hlineto
                for (int32 i = 0; i < top; ++i)
                    T2_LineTo(state, (i%2==0) ? stk[i] : 0.f, (i%2==1) ? stk[i] : 0.f);
                top = 0; break;
            case 7:  // vlineto
                for (int32 i = 0; i < top; ++i)
                    T2_LineTo(state, (i%2==1) ? stk[i] : 0.f, (i%2==0) ? stk[i] : 0.f);
                top = 0; break;

            // ── Courbes ──────────────────────────────────────────────────────
            case 8:  // rrcurveto
                for (int32 i = 0; i + 5 < top; i += 6)
                    T2_CurveTo(state, stk[i],stk[i+1],stk[i+2],stk[i+3],stk[i+4],stk[i+5]);
                top = 0; break;
            case 27: // hhcurveto
            {
                int32 i = 0;
                if (top % 4 == 1) { T2_CurveTo(state, stk[i+1], stk[0], stk[i+2], stk[i+3], stk[i+4], 0.f); i += 5; }
                for (; i + 3 < top; i += 4)
                    T2_CurveTo(state, stk[i], 0.f, stk[i+1], stk[i+2], stk[i+3], 0.f);
                top = 0; break;
            }
            case 31: // hvcurveto
            {
                int32 i = 0;
                while (i + 3 < top) {
                    if ((i/4) % 2 == 0) {
                        float dy3 = (top - i == 5) ? stk[i+4] : 0.f;
                        T2_CurveTo(state, stk[i],0.f,stk[i+1],stk[i+2],0.f,stk[i+3]+dy3);
                        i += (top - i == 5) ? 5 : 4;
                    } else {
                        float dx3 = (top - i == 5) ? stk[i+4] : 0.f;
                        T2_CurveTo(state, 0.f,stk[i],stk[i+1],stk[i+2],stk[i+3]+dx3,0.f);
                        i += (top - i == 5) ? 5 : 4;
                    }
                }
                top = 0; break;
            }
            case 30: // vhcurveto
            {
                int32 i = 0;
                while (i + 3 < top) {
                    if ((i/4) % 2 == 0) {
                        float dx3 = (top - i == 5) ? stk[i+4] : 0.f;
                        T2_CurveTo(state, 0.f,stk[i],stk[i+1],stk[i+2],stk[i+3]+dx3,0.f);
                        i += (top - i == 5) ? 5 : 4;
                    } else {
                        float dy3 = (top - i == 5) ? stk[i+4] : 0.f;
                        T2_CurveTo(state, stk[i],0.f,stk[i+1],stk[i+2],0.f,stk[i+3]+dy3);
                        i += (top - i == 5) ? 5 : 4;
                    }
                }
                top = 0; break;
            }
            case 26: // vvcurveto
            {
                int32 i = 0;
                if (top % 4 == 1) { T2_CurveTo(state, stk[0], stk[i+1], stk[i+2], stk[i+3], 0.f, stk[i+4]); i += 5; }
                for (; i + 3 < top; i += 4)
                    T2_CurveTo(state, 0.f, stk[i], stk[i+1], stk[i+2], 0.f, stk[i+3]);
                top = 0; break;
            }
            case 24: // rcurveline
                for (int32 i = 0; i + 5 < top - 2; i += 6)
                    T2_CurveTo(state, stk[i],stk[i+1],stk[i+2],stk[i+3],stk[i+4],stk[i+5]);
                T2_LineTo(state, stk[top-2], stk[top-1]);
                top = 0; break;
            case 25: // rlinecurve
                for (int32 i = 0; i < top - 6; i += 2)
                    T2_LineTo(state, stk[i], stk[i+1]);
                T2_CurveTo(state, stk[top-6],stk[top-5],stk[top-4],stk[top-3],stk[top-2],stk[top-1]);
                top = 0; break;

            // ── Hints (ignorés — pas de VM hinting dans ce module) ───────────
            case 1:  // hstem
            case 3:  // vstem
            case 18: // hstemhm
            case 23: // vstemhm
                state.numHints += top / 2;
                top = 0; break;
            case 19: // hintmask
            case 20: { // cntrmask
                state.numHints += top / 2;
                const int32 maskBytes = (state.numHints + 7) / 8;
                s.Skip(static_cast<usize>(maskBytes));
                top = 0; break;
            }

            // ── Appels subr ──────────────────────────────────────────────────
            case 10: { // callsubr
                if (top < 1) return false;
                const int32 idx = static_cast<int32>(stk[--top]);
                if (!T2_CallSubr(font, false, idx, state, depth)) return false;
                break;
            }
            case 29: { // callgsubr
                if (top < 1) return false;
                const int32 idx = static_cast<int32>(stk[--top]);
                if (!T2_CallSubr(font, true, idx, state, depth)) return false;
                break;
            }
            case 11: // return
                return true;

            // ── Escape (opérateur 2 octets) ───────────────────────────────────
            case 12: {
                const uint8 b2 = s.ReadU8();
                switch (b2) {
                    case 34: // hflex
                        if (top >= 7) {
                            T2_CurveTo(state, stk[0],0,stk[1],stk[2],stk[3],0);
                            T2_CurveTo(state, stk[4],0,stk[5],stk[6],stk[7-top%4],-state.y);
                        }
                        top = 0; break;
                    case 35: // flex
                        if (top >= 13) {
                            T2_CurveTo(state,stk[0],stk[1],stk[2],stk[3],stk[4],stk[5]);
                            T2_CurveTo(state,stk[6],stk[7],stk[8],stk[9],stk[10],stk[11]);
                        }
                        top = 0; break;
                    case 36: // hflex1
                        if (top >= 9) {
                            T2_CurveTo(state,stk[0],stk[1],stk[2],stk[3],stk[4],0);
                            T2_CurveTo(state,stk[5],0,stk[6],stk[7],stk[8],-stk[1]-stk[3]-stk[7]);
                        }
                        top = 0; break;
                    case 37: // flex1
                        if (top >= 11) {
                            T2_CurveTo(state,stk[0],stk[1],stk[2],stk[3],stk[4],stk[5]);
                            T2_CurveTo(state,stk[6],stk[7],stk[8],stk[9],stk[10],stk[11 < top ? 11 : 10]);
                        }
                        top = 0; break;
                    // Opérateurs arithmétiques (utilisés dans certains subrs)
                    case 3:  if (top>=2) stk[top-2] = (stk[top-2]!=0&&stk[top-1]!=0)?1.f:0.f; --top; break; // and
                    case 4:  if (top>=2) stk[top-2] = (stk[top-2]!=0||stk[top-1]!=0)?1.f:0.f; --top; break; // or
                    case 5:  if (top>=1) stk[top-1] = (stk[top-1]==0.f)?1.f:0.f; break; // not
                    case 9:  if (top>=1) stk[top-1] = stk[top-1]<0?-stk[top-1]:stk[top-1]; break; // abs
                    case 10: if (top>=2) { stk[top-2] += stk[top-1]; --top; } break; // add
                    case 11: if (top>=2) { stk[top-2] -= stk[top-1]; --top; } break; // sub
                    case 12: if (top>=2) { if(stk[top-1]!=0) stk[top-2]/=stk[top-1]; --top; } break; // div
                    case 14: if (top>=1) stk[top-1] = -stk[top-1]; break; // neg
                    case 15: if (top>=2) { stk[top-2]=(stk[top-2]==stk[top-1])?1.f:0.f; --top; } break; // eq
                    case 18: if (top>=4) { // ifelse
                        if (stk[top-2] > stk[top-1]) stk[top-4] = stk[top-3];
                        top -= 3;
                    } break;
                    case 20: if (top>=2) { stk[top-2]*=stk[top-1]; --top; } break; // mul
                    case 23: if (top>=1) { stk[top-1]=sqrtf(stk[top-1]>0?stk[top-1]:0); } break; // sqrt
                    default: top = 0; break;
                }
                break;
            }
            default: top = 0; break;
        }
    }
    return !s.HasError();
}

} // namespace nkentseu
