/**
 * @File    NkUnicode.cpp
 * @Brief   Tables Unicode et algorithme bidi.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 */

#include "pch.h"
#include "NKFont/NkUnicode.h"
#include <cstring>

namespace nkentseu {

// ─────────────────────────────────────────────────────────────────────────────
//  Tables compactes
//  Couverture : Latin (U+0000-U+024F), Arabe (U+0600-U+06FF),
//               Hébreu (U+0590-U+05FF), CJK BMP basique
// ─────────────────────────────────────────────────────────────────────────────

// Catégories page 0 (U+0000-U+00FF) — valeur = NkUniCategory cast en uint8
const uint8 NkUnicode::kCategoryPage0[256] = {
    // U+0000-U+001F : Cc (control)
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
    // U+0020-U+007E : Zs, Po, Nd, Lu, Ll, etc.
    20,16,16,16,15,16,16,16,16,16,16,15,16,16,16,16, // SP ! " # $ % & ' ( ) * + , - . /
     7, 7, 7, 7, 7, 7, 7, 7, 7, 7,16,16,15,15,15,16, // 0-9 : ; < = > ?
    16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // @ A-O
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,13,16,14,15,11, // P-Z [ \ ] ^ _
    16, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // ` a-o
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,13,16,14,16,15, // p-z { | } ~ DEL
    // U+0080-U+00FF : Latin-1 Supplement
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
    20,16,15,15,15,15,16,15,16,15, 0,16,15,15,16, 0,
    15,15, 7, 7,16, 1,16,15,16, 7, 1,16, 7, 7, 7,16,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0,15, 0, 0, 0, 0, 0, 0, 0, 0,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1,15, 1, 1, 1, 1, 1, 1, 1, 1,
};

// Bidi types page 0
const uint8 NkUnicode::kBidiPage0[256] = {
    // U+0000-U+001F
     9, 9, 9, 9, 9, 9, 9, 9, 9,11,10,11,10,10, 9, 9,
     9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,10,10,10,11,
    // U+0020-U+007F
    12,13,13,13,15,15,13,13,13,13,13, 5,13, 5,13,13,
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3,13,13,13,13,13,13,
    13, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,13,13,13,13,13,
    13, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,13,13,13,13, 9,
    // U+0080-U+00BF
     9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
     9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    12, 3, 4, 4, 4, 4, 4, 4, 4, 4, 0, 3,13,13,13,13,
     5, 5, 3, 3,13, 1,13,13,13, 3, 0, 3, 3, 3, 3,13,
    // U+00C0-U+00FF
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0,13, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0,13, 0, 0, 0, 0, 0, 0, 0, 0,
};

// ─────────────────────────────────────────────────────────────────────────────
//  Lookup avec fallback pour les blocs hors page 0
// ─────────────────────────────────────────────────────────────────────────────

uint8 NkUnicode::LookupCategory(uint32 cp) noexcept {
    if (cp < 256) return kCategoryPage0[cp];
    // Latin Extended (U+0100-U+024F) : majoritairement Lu/Ll
    if (cp >= 0x0100 && cp <= 0x024F) return (cp & 1) ? 1 : 0; // Ll ou Lu
    // Combining marks (U+0300-U+036F)
    if (cp >= 0x0300 && cp <= 0x036F) return 5; // Mn
    // Hebrew (U+0590-U+05FF)
    if (cp >= 0x0590 && cp <= 0x05FF) {
        if (cp >= 0x0591 && cp <= 0x05C7) return 5; // Mn
        if (cp >= 0x05D0 && cp <= 0x05EA) return 4; // Lo
        return 16; // Po
    }
    // Arabic (U+0600-U+06FF)
    if (cp >= 0x0600 && cp <= 0x06FF) {
        if (cp >= 0x0621 && cp <= 0x063A) return 4; // Lo
        if (cp >= 0x0641 && cp <= 0x064A) return 4;
        if (cp >= 0x0660 && cp <= 0x0669) return 7; // Nd
        if (cp >= 0x064B && cp <= 0x065F) return 5; // Mn
        return 16;
    }
    // CJK (U+4E00-U+9FFF)
    if (cp >= 0x4E00 && cp <= 0x9FFF) return 4; // Lo
    // Hangul syllables
    if (cp >= 0xAC00 && cp <= 0xD7A3) return 4; // Lo
    // Surrogates
    if (cp >= 0xD800 && cp <= 0xDFFF) return 22; // Cs
    // PUA
    if (cp >= 0xE000 && cp <= 0xF8FF) return 23; // Co
    // Default : Lo
    if (cp <= 0x10FFFF) return 4;
    return 24; // Cn
}

uint8 NkUnicode::LookupBidi(uint32 cp) noexcept {
    if (cp < 256) return kBidiPage0[cp];
    if (cp >= 0x0300 && cp <= 0x036F) return static_cast<uint8>(NkBidiType::NSM);
    if (cp >= 0x0590 && cp <= 0x05FF) return static_cast<uint8>(NkBidiType::R);
    if (cp >= 0x0600 && cp <= 0x06FF) {
        if (cp >= 0x0660 && cp <= 0x0669) return static_cast<uint8>(NkBidiType::AN);
        if (cp >= 0x064B && cp <= 0x065F) return static_cast<uint8>(NkBidiType::NSM);
        return static_cast<uint8>(NkBidiType::AL);
    }
    if (cp >= 0x200F) return static_cast<uint8>(NkBidiType::R);
    if (cp >= 0x4E00 && cp <= 0x9FFF) return static_cast<uint8>(NkBidiType::L);
    if (cp >= 0xAC00 && cp <= 0xD7A3) return static_cast<uint8>(NkBidiType::L);
    return static_cast<uint8>(NkBidiType::L);
}

// ─────────────────────────────────────────────────────────────────────────────
//  API publique
// ─────────────────────────────────────────────────────────────────────────────

NkUniCategory NkUnicode::GetCategory(uint32 cp) noexcept {
    return static_cast<NkUniCategory>(LookupCategory(cp));
}
NkBidiType NkUnicode::GetBidiType(uint32 cp) noexcept {
    return static_cast<NkBidiType>(LookupBidi(cp));
}
bool NkUnicode::IsLetter    (uint32 cp) noexcept { auto c=GetCategory(cp); return c<=NkUniCategory::Lo; }
bool NkUnicode::IsDigit     (uint32 cp) noexcept { return GetCategory(cp)==NkUniCategory::Nd; }
bool NkUnicode::IsWhitespace(uint32 cp) noexcept { return cp==0x20||cp==0x09||cp==0x0A||cp==0x0D||GetCategory(cp)==NkUniCategory::Zs; }
bool NkUnicode::IsCombining (uint32 cp) noexcept { auto c=GetCategory(cp); return c>=NkUniCategory::Mn&&c<=NkUniCategory::Me; }
bool NkUnicode::IsRTL       (uint32 cp) noexcept { auto b=GetBidiType(cp); return b==NkBidiType::R||b==NkBidiType::AL; }
bool NkUnicode::IsArabic    (uint32 cp) noexcept { return cp>=0x0600&&cp<=0x06FF; }
bool NkUnicode::IsHebrew    (uint32 cp) noexcept { return cp>=0x0590&&cp<=0x05FF; }

bool NkUnicode::IsMirrored(uint32 cp) noexcept {
    return cp=='('||cp==')'||cp=='['||cp==']'||cp=='{'||cp=='}'
        || cp==0x2039||cp==0x203A||cp==0x2018||cp==0x2019;
}
uint32 NkUnicode::GetMirror(uint32 cp) noexcept {
    switch (cp) {
        case '(':  return ')'; case ')':  return '(';
        case '[':  return ']'; case ']':  return '[';
        case '{':  return '}'; case '}':  return '{';
        case 0x2039: return 0x203A; case 0x203A: return 0x2039;
        default: return cp;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  UTF-8
// ─────────────────────────────────────────────────────────────────────────────

uint32 NkUnicode::DecodeOneUTF8(const uint8* src, usize rem, uint32& advance) noexcept {
    if (rem == 0) { advance = 1; return 0xFFFD; }
    const uint8 b0 = src[0];
    if (b0 < 0x80) { advance = 1; return b0; }
    if (b0 < 0xC0) { advance = 1; return 0xFFFD; } // continuation orpheline
    if (b0 < 0xE0) {
        if (rem < 2 || (src[1] & 0xC0) != 0x80) { advance = 1; return 0xFFFD; }
        advance = 2;
        return ((b0 & 0x1F) << 6) | (src[1] & 0x3F);
    }
    if (b0 < 0xF0) {
        if (rem < 3 || (src[1]&0xC0)!=0x80 || (src[2]&0xC0)!=0x80) { advance=1; return 0xFFFD; }
        advance = 3;
        return ((b0&0x0F)<<12)|((src[1]&0x3F)<<6)|(src[2]&0x3F);
    }
    if (b0 < 0xF8) {
        if (rem < 4 || (src[1]&0xC0)!=0x80 || (src[2]&0xC0)!=0x80 || (src[3]&0xC0)!=0x80) { advance=1; return 0xFFFD; }
        advance = 4;
        return ((b0&0x07)<<18)|((src[1]&0x3F)<<12)|((src[2]&0x3F)<<6)|(src[3]&0x3F);
    }
    advance = 1; return 0xFFFD;
}

bool NkUnicode::UTF8Decode(
    const uint8* utf8, usize byteLen,
    NkMemArena& arena, uint32*& outCps, uint32& outLen) noexcept
{
    // Passe 1 : compte les codepoints
    uint32 count = 0;
    for (usize i = 0; i < byteLen; ) {
        uint32 adv = 1;
        DecodeOneUTF8(utf8 + i, byteLen - i, adv);
        i += adv; ++count;
    }
    outLen = count;
    outCps = arena.Alloc<uint32>(count + 1);
    if (!outCps) return false;
    // Passe 2 : décode
    uint32 idx = 0;
    for (usize i = 0; i < byteLen && idx < count; ) {
        uint32 adv = 1;
        outCps[idx++] = DecodeOneUTF8(utf8 + i, byteLen - i, adv);
        i += adv;
    }
    outCps[count] = 0;
    return true;
}

usize NkUnicode::UTF8Encode(
    const uint32* cps, uint32 count, uint8* dst, usize dstSize) noexcept
{
    usize pos = 0;
    for (uint32 i = 0; i < count; ++i) {
        const uint32 cp = cps[i];
        if (cp < 0x80) {
            if (pos + 1 > dstSize) break;
            dst[pos++] = static_cast<uint8>(cp);
        } else if (cp < 0x800) {
            if (pos + 2 > dstSize) break;
            dst[pos++] = static_cast<uint8>(0xC0 | (cp >> 6));
            dst[pos++] = static_cast<uint8>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            if (pos + 3 > dstSize) break;
            dst[pos++] = static_cast<uint8>(0xE0 | (cp >> 12));
            dst[pos++] = static_cast<uint8>(0x80 | ((cp >> 6) & 0x3F));
            dst[pos++] = static_cast<uint8>(0x80 | (cp & 0x3F));
        } else {
            if (pos + 4 > dstSize) break;
            dst[pos++] = static_cast<uint8>(0xF0 | (cp >> 18));
            dst[pos++] = static_cast<uint8>(0x80 | ((cp >> 12) & 0x3F));
            dst[pos++] = static_cast<uint8>(0x80 | ((cp >> 6) & 0x3F));
            dst[pos++] = static_cast<uint8>(0x80 | (cp & 0x3F));
        }
    }
    return pos;
}

bool NkUnicode::UTF16Decode(
    const uint16* utf16, usize wordLen,
    NkMemArena& arena, uint32*& outCps, uint32& outLen) noexcept
{
    uint32 count = 0;
    for (usize i = 0; i < wordLen; ) {
        const uint16 w = utf16[i++];
        if (w >= 0xD800 && w <= 0xDBFF && i < wordLen) ++i; // surrogate pair
        ++count;
    }
    outLen = count;
    outCps = arena.Alloc<uint32>(count + 1);
    if (!outCps) return false;
    uint32 idx = 0;
    for (usize i = 0; i < wordLen && idx < count; ) {
        const uint16 w = utf16[i++];
        if (w >= 0xD800 && w <= 0xDBFF && i < wordLen) {
            const uint16 lo = utf16[i++];
            outCps[idx++] = 0x10000u + ((static_cast<uint32>(w - 0xD800)) << 10) + (lo - 0xDC00);
        } else {
            outCps[idx++] = w;
        }
    }
    outCps[count] = 0;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Bidi (UAX#9 simplifié — P1, X1-X8, W1-W7, N0-N2, I1-I2, L1-L2)
// ─────────────────────────────────────────────────────────────────────────────

int32 NkUnicode::DetectBaseLevel(const uint32* cps, uint32 count) noexcept {
    for (uint32 i = 0; i < count; ++i) {
        const NkBidiType t = GetBidiType(cps[i]);
        if (t == NkBidiType::L)  return 0;
        if (t == NkBidiType::R || t == NkBidiType::AL) return 1;
    }
    return 0; // défaut LTR
}

void NkUnicode::ResolveWeakTypes(
    NkBidiType* types, const uint8* levels, uint32 count) noexcept
{
    (void)levels;
    // W1 : NSM hérite du type précédent
    NkBidiType prevStrong = NkBidiType::L;
    for (uint32 i = 0; i < count; ++i) {
        if (types[i] == NkBidiType::NSM) types[i] = prevStrong;
        if (types[i] == NkBidiType::L || types[i] == NkBidiType::R ||
            types[i] == NkBidiType::AL) prevStrong = types[i];
    }
    // W2 : EN après AL → AN
    NkBidiType lastStrong = NkBidiType::L;
    for (uint32 i = 0; i < count; ++i) {
        if (types[i] == NkBidiType::AL) lastStrong = NkBidiType::AL;
        else if (types[i] == NkBidiType::L || types[i] == NkBidiType::R) lastStrong = types[i];
        else if (types[i] == NkBidiType::EN && lastStrong == NkBidiType::AL)
            types[i] = NkBidiType::AN;
    }
    // W3 : AL → R
    for (uint32 i = 0; i < count; ++i)
        if (types[i] == NkBidiType::AL) types[i] = NkBidiType::R;
    // W4 : ES entre EN → EN ; CS entre EN → EN ; CS entre AN → AN
    for (uint32 i = 1; i + 1 < count; ++i) {
        if (types[i] == NkBidiType::ES &&
            types[i-1] == NkBidiType::EN && types[i+1] == NkBidiType::EN)
            types[i] = NkBidiType::EN;
        else if (types[i] == NkBidiType::CS) {
            if (types[i-1] == NkBidiType::EN && types[i+1] == NkBidiType::EN) types[i] = NkBidiType::EN;
            else if (types[i-1] == NkBidiType::AN && types[i+1] == NkBidiType::AN) types[i] = NkBidiType::AN;
        }
    }
    // W5 : ET adjacent à EN → EN
    for (uint32 i = 0; i < count; ++i) {
        if (types[i] == NkBidiType::ET) {
            bool adj = false;
            if (i > 0 && types[i-1] == NkBidiType::EN) adj = true;
            if (i+1 < count && types[i+1] == NkBidiType::EN) adj = true;
            if (adj) types[i] = NkBidiType::EN;
        }
    }
    // W6 : ES, ET, CS restants → ON
    for (uint32 i = 0; i < count; ++i)
        if (types[i]==NkBidiType::ES||types[i]==NkBidiType::ET||types[i]==NkBidiType::CS)
            types[i] = NkBidiType::ON;
    // W7 : EN après L fort → L
    lastStrong = NkBidiType::L;
    for (uint32 i = 0; i < count; ++i) {
        if (types[i] == NkBidiType::L || types[i] == NkBidiType::R) lastStrong = types[i];
        else if (types[i] == NkBidiType::EN && lastStrong == NkBidiType::L) types[i] = NkBidiType::L;
    }
}

void NkUnicode::ResolveNeutralTypes(
    NkBidiType* types, const uint8* levels, uint32 count, int32 paraLevel) noexcept
{
    (void)levels;
    // N1/N2 : neutres (ON, WS, B, S) entre deux forts de même type → même type
    //          sinon → type du niveau de paragraphe
    for (uint32 i = 0; i < count; ) {
        if (types[i] != NkBidiType::ON && types[i] != NkBidiType::WS &&
            types[i] != NkBidiType::B  && types[i] != NkBidiType::S) {
            ++i; continue;
        }
        const uint32 start = i;
        while (i < count && (types[i]==NkBidiType::ON||types[i]==NkBidiType::WS||
                              types[i]==NkBidiType::B ||types[i]==NkBidiType::S)) ++i;
        NkBidiType before = (start > 0) ? types[start-1] : static_cast<NkBidiType>(paraLevel==1?1:0);
        NkBidiType after  = (i < count) ? types[i]       : static_cast<NkBidiType>(paraLevel==1?1:0);
        const NkBidiType fill = (before == after) ? before
                              : static_cast<NkBidiType>(paraLevel==1 ? 1 : 0);
        for (uint32 j = start; j < i; ++j) types[j] = fill;
    }
}

void NkUnicode::ResolveImplicitLevels(
    uint8* levels, const NkBidiType* types, uint32 count) noexcept
{
    for (uint32 i = 0; i < count; ++i) {
        const uint8 lvl = levels[i];
        if (types[i] == NkBidiType::R && (lvl & 1) == 0) levels[i]++;  // I1
        if (types[i] == NkBidiType::L && (lvl & 1) == 1) levels[i]++;  // I2
        if (types[i] == NkBidiType::AN && (lvl & 1) == 0) levels[i] += 2;
        if (types[i] == NkBidiType::EN && (lvl & 1) == 0) levels[i] += 2;
    }
}

bool NkUnicode::BidiResolve(
    const uint32* codepoints, uint32 count,
    NkMemArena& arena, uint8*& outLevels, int32 baseLevel) noexcept
{
    outLevels = arena.Alloc<uint8>(count);
    NkBidiType* types = arena.Alloc<NkBidiType>(count);
    if (!outLevels || !types) return false;

    if (baseLevel < 0) baseLevel = DetectBaseLevel(codepoints, count);
    baseLevel &= 1;

    for (uint32 i = 0; i < count; ++i) {
        types[i]     = GetBidiType(codepoints[i]);
        outLevels[i] = static_cast<uint8>(baseLevel);
    }
    ResolveWeakTypes(types, outLevels, count);
    ResolveNeutralTypes(types, outLevels, count, baseLevel);
    ResolveImplicitLevels(outLevels, types, count);
    return true;
}

bool NkUnicode::BidiReorder(
    const uint8* levels, uint32 count,
    NkMemArena& arena, uint32*& outOrder) noexcept
{
    outOrder = arena.Alloc<uint32>(count);
    if (!outOrder) return false;
    for (uint32 i = 0; i < count; ++i) outOrder[i] = i;

    // Algorithme L2 : pour chaque niveau de max à 1, inverse les runs
    uint8 maxLevel = 0;
    for (uint32 i = 0; i < count; ++i) if (levels[i] > maxLevel) maxLevel = levels[i];

    for (uint8 level = maxLevel; level >= 1; --level) {
        for (uint32 i = 0; i < count; ) {
            if (levels[i] >= level) {
                uint32 j = i;
                while (j < count && levels[j] >= level) ++j;
                // Inverse outOrder[i..j-1]
                uint32 lo = i, hi = j - 1;
                while (lo < hi) {
                    uint32 tmp = outOrder[lo]; outOrder[lo] = outOrder[hi]; outOrder[hi] = tmp;
                    ++lo; --hi;
                }
                i = j;
            } else { ++i; }
        }
    }
    return true;
}

} // namespace nkentseu
