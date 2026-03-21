/**
 * @File    NkBDFParser.cpp
 * @Brief   Implémentation des parsers BDF et PCF.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 */

#include "pch.h"
#include "NKFont/NkBDFParser.h"
#include <cstring>
#include <cctype>

namespace nkentseu {

// ─────────────────────────────────────────────────────────────────────────────
//  NkBDFFont
// ─────────────────────────────────────────────────────────────────────────────

const NkBDFGlyph* NkBDFFont::FindGlyph(uint32 encoding) const noexcept {
    for (uint32 i = 0; i < numGlyphs; ++i)
        if (glyphs[i].encoding == encoding) return &glyphs[i];
    return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
//  NkBDFParser helpers
// ─────────────────────────────────────────────────────────────────────────────

bool NkBDFParser::ParseLine(NkStreamReader& s, char* buf, usize bufSize) noexcept {
    usize i = 0;
    while (!s.IsEOF() && i < bufSize - 1) {
        const uint8 c = s.ReadU8();
        if (c == '\n') break;
        if (c != '\r') buf[i++] = static_cast<char>(c);
    }
    buf[i] = '\0';
    return i > 0 || !s.IsEOF();
}

bool NkBDFParser::ParseInt(const char* str, int32& out) noexcept {
    while (*str == ' ') ++str;
    if (!*str) return false;
    bool neg = false;
    if (*str == '-') { neg = true; ++str; }
    out = 0;
    while (*str >= '0' && *str <= '9') { out = out * 10 + (*str - '0'); ++str; }
    if (neg) out = -out;
    return true;
}

void NkBDFParser::HexRowToBitmap(const char* hex, NkBDFGlyph& glyph, int32 row) noexcept {
    if (!glyph.bitmap.IsValid()) return;
    uint8* rowPtr = glyph.bitmap.RowPtr(row);
    const int32 w = glyph.bbxWidth;
    int32 col = 0;
    for (int32 i = 0; hex[i] && col < w; i += 2) {
        uint8 byte = 0;
        for (int j = 0; j < 2; ++j) {
            const char c = hex[i + j];
            uint8 nibble = 0;
            if      (c >= '0' && c <= '9') nibble = static_cast<uint8>(c - '0');
            else if (c >= 'A' && c <= 'F') nibble = static_cast<uint8>(c - 'A' + 10);
            else if (c >= 'a' && c <= 'f') nibble = static_cast<uint8>(c - 'a' + 10);
            byte = static_cast<uint8>((byte << 4) | nibble);
        }
        for (int bit = 7; bit >= 0 && col < w; --bit, ++col)
            rowPtr[col] = (byte >> bit) & 1 ? 255 : 0;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  NkBDFParser::Parse
// ─────────────────────────────────────────────────────────────────────────────

bool NkBDFParser::Parse(
    const uint8* data, usize size,
    NkMemArena& arena, NkBDFFont& out) noexcept
{
    ::memset(&out, 0, sizeof(out));
    if (!data || size < 4) return false;

    NkStreamReader s(data, size);
    char line[512];

    // Passe 1 : compte les glyphes et lit les métriques globales
    uint32 numGlyphs = 0;
    while (ParseLine(s, line, sizeof(line))) {
        if (::strncmp(line, "CHARS ", 6) == 0) {
            ParseInt(line + 6, reinterpret_cast<int32&>(numGlyphs));
            break;
        }
        if (::strncmp(line, "FONT_ASCENT ", 12) == 0) ParseInt(line + 12, out.metrics.fontAscent);
        if (::strncmp(line, "FONT_DESCENT ", 13) == 0) ParseInt(line + 13, out.metrics.fontDescent);
        if (::strncmp(line, "DEFAULT_CHAR ", 13) == 0) ParseInt(line + 13, reinterpret_cast<int32&>(out.metrics.defaultChar));
        if (::strncmp(line, "SIZE ", 5) == 0) {
            ParseInt(line + 5, out.metrics.pointSize);
            // "SIZE <pt> <rx> <ry>"
            const char* p = line + 5;
            while (*p && *p != ' ') ++p;
            if (*p) { ++p; ParseInt(p, out.metrics.resolutionX); }
            while (*p && *p != ' ') ++p;
            if (*p) { ++p; ParseInt(p, out.metrics.resolutionY); }
        }
    }
    if (numGlyphs == 0) return false;

    out.glyphs    = arena.Alloc<NkBDFGlyph>(numGlyphs);
    out.numGlyphs = numGlyphs;
    if (!out.glyphs) return false;

    // Passe 2 : parse chaque glyphe
    s.Seek(0);
    uint32 gi = 0;
    NkBDFGlyph* cur = nullptr;
    int32 bitmapRow = 0;

    while (ParseLine(s, line, sizeof(line)) && gi < numGlyphs) {
        if (::strncmp(line, "STARTCHAR", 9) == 0) {
            cur = &out.glyphs[gi];
            ::memset(cur, 0, sizeof(NkBDFGlyph));
            bitmapRow = 0;
        } else if (cur && ::strncmp(line, "ENCODING ", 9) == 0) {
            ParseInt(line + 9, reinterpret_cast<int32&>(cur->encoding));
        } else if (cur && ::strncmp(line, "DWIDTH ", 7) == 0) {
            ParseInt(line + 7, cur->dwidthX);
        } else if (cur && ::strncmp(line, "BBX ", 4) == 0) {
            const char* p = line + 4;
            ParseInt(p, cur->bbxWidth);
            while (*p && *p != ' ') ++p; if (*p) ++p;
            ParseInt(p, cur->bbxHeight);
            while (*p && *p != ' ') ++p; if (*p) ++p;
            ParseInt(p, cur->bbxOffX);
            while (*p && *p != ' ') ++p; if (*p) ++p;
            ParseInt(p, cur->bbxOffY);
        } else if (cur && ::strcmp(line, "BITMAP") == 0) {
            cur->bitmap = NkBitmap::Alloc(arena, cur->bbxWidth, cur->bbxHeight);
        } else if (cur && cur->bitmap.IsValid() && ::strcmp(line, "ENDCHAR") == 0) {
            ++gi;
            cur = nullptr;
        } else if (cur && cur->bitmap.IsValid() && bitmapRow < cur->bbxHeight) {
            HexRowToBitmap(line, *cur, bitmapRow++);
        }
    }

    out.isValid = (gi > 0);
    out.numGlyphs = gi;
    return out.isValid;
}

// ─────────────────────────────────────────────────────────────────────────────
//  NkPCFParser::Parse
// ─────────────────────────────────────────────────────────────────────────────

bool NkPCFParser::Parse(
    const uint8* data, usize size,
    NkMemArena& arena, NkBDFFont& out) noexcept
{
    ::memset(&out, 0, sizeof(out));
    if (!data || size < 8) return false;

    NkStreamReader s(data, size);

    // Magic LE
    const uint32 magic = s.ReadU32LE();
    if (magic != PCF_MAGIC) return false;

    const uint32 tableCount = s.ReadU32LE();
    if (tableCount == 0 || tableCount > 32) return false;

    TableEntry* tables = arena.Alloc<TableEntry>(tableCount);
    if (!tables) return false;

    for (uint32 i = 0; i < tableCount; ++i) {
        tables[i].type   = s.ReadU32LE();
        tables[i].format = s.ReadU32LE();
        tables[i].size   = s.ReadU32LE();
        tables[i].offset = s.ReadU32LE();
    }
    if (s.HasError()) return false;

    uint32 numGlyphs = 0;

    // Lit d'abord les métriques pour connaître numGlyphs
    for (uint32 i = 0; i < tableCount; ++i) {
        if (tables[i].type == PCF_METRICS) {
            NkStreamReader ts(data + tables[i].offset, tables[i].size);
            const uint32 fmt  = ts.ReadU32LE();
            const bool bigEndian = (fmt & PCF_BYTE_MASK) != 0;
            numGlyphs = bigEndian ? ts.ReadU32() : ts.ReadU32LE();
            break;
        }
    }
    if (numGlyphs == 0) return false;

    out.glyphs    = arena.Alloc<NkBDFGlyph>(numGlyphs);
    out.numGlyphs = numGlyphs;
    if (!out.glyphs) return false;

    // Parse chaque table dans l'ordre
    for (uint32 i = 0; i < tableCount; ++i) {
        if (tables[i].offset + tables[i].size > size) continue;
        NkStreamReader ts(data + tables[i].offset, tables[i].size);
        const uint32 fmt = ts.ReadU32LE();

        switch (tables[i].type) {
            case PCF_METRICS:
                ParseMetricsTable(ts, fmt, out, arena);
                break;
            case PCF_BITMAPS:
                ParseBitmapsTable(ts, fmt, out, arena);
                break;
            case PCF_BDF_ENCODINGS:
                ParseEncodingTable(ts, fmt, out);
                break;
            case PCF_BDF_ACCELERATORS:
                ParseAccelTable(ts, fmt, out);
                break;
            default:
                break;
        }
    }

    out.isValid = true;
    return true;
}

bool NkPCFParser::ParseMetricsTable(
    NkStreamReader& s, uint32 format,
    NkBDFFont& out, NkMemArena& arena) noexcept
{
    const bool be = (format & PCF_BYTE_MASK) != 0;
    const uint32 count = be ? s.ReadU32() : s.ReadU32LE();
    if (count != out.numGlyphs) return false;

    for (uint32 i = 0; i < count; ++i) {
        NkBDFGlyph& g = out.glyphs[i];
        // Format compressé (5 octets par métrique)
        const uint8 lsb   = s.ReadU8();
        const uint8 rsb   = s.ReadU8();
        const uint8 w     = s.ReadU8();
        const uint8 asc   = s.ReadU8();
        const uint8 desc  = s.ReadU8();

        g.bbxOffX  = static_cast<int32>(lsb) - 0x80;
        g.bbxOffY  = -(static_cast<int32>(desc) - 0x80);
        g.bbxWidth = static_cast<int32>(rsb) - 0x80
                   - (static_cast<int32>(lsb) - 0x80);
        g.bbxHeight= (static_cast<int32>(asc) - 0x80)
                   + (static_cast<int32>(desc) - 0x80);
        g.dwidthX  = static_cast<int32>(w) - 0x80;
        (void)arena;
    }
    return !s.HasError();
}

bool NkPCFParser::ParseBitmapsTable(
    NkStreamReader& s, uint32 format,
    NkBDFFont& out, NkMemArena& arena) noexcept
{
    const bool be      = (format & PCF_BYTE_MASK) != 0;
    const bool msbBit  = (format & PCF_BIT_MASK) != 0;
    const uint32 scanUnit = 1u << (format & PCF_SCAN_UNIT_MASK); // 1,2,4

    const uint32 count = be ? s.ReadU32() : s.ReadU32LE();
    if (count != out.numGlyphs) return false;

    uint32* offsets = arena.Alloc<uint32>(count);
    if (!offsets) return false;
    for (uint32 i = 0; i < count; ++i)
        offsets[i] = be ? s.ReadU32() : s.ReadU32LE();

    uint32 bitmapSizes[4];
    for (int i = 0; i < 4; ++i)
        bitmapSizes[i] = be ? s.ReadU32() : s.ReadU32LE();

    const uint32 dataSize = bitmapSizes[format & 3];
    const uint8* bitmapData = s.CurrentPtr();
    (void)dataSize;

    for (uint32 i = 0; i < count; ++i) {
        NkBDFGlyph& g = out.glyphs[i];
        if (g.bbxWidth <= 0 || g.bbxHeight <= 0) continue;
        g.bitmap = NkBitmap::Alloc(arena, g.bbxWidth, g.bbxHeight);
        if (!g.bitmap.IsValid()) continue;

        const uint8* src = bitmapData + offsets[i];
        const uint32 rowBytes = ((g.bbxWidth + 7) / 8 + scanUnit - 1) / scanUnit * scanUnit;

        for (int32 row = 0; row < g.bbxHeight; ++row) {
            uint8* dst = g.bitmap.RowPtr(row);
            for (int32 col = 0; col < g.bbxWidth; ++col) {
                const uint32 bitIdx = msbBit
                    ? (static_cast<uint32>(col))
                    : (static_cast<uint32>(col ^ 7));
                const uint8 byte = src[row * rowBytes + bitIdx / 8];
                const uint8 bit  = msbBit
                    ? ((byte >> (7 - (bitIdx % 8))) & 1)
                    : ((byte >> (    bitIdx % 8 )) & 1);
                dst[col] = bit ? 255 : 0;
            }
        }
    }
    return true;
}

bool NkPCFParser::ParseEncodingTable(
    NkStreamReader& s, uint32 format,
    NkBDFFont& out) noexcept
{
    const bool be = (format & PCF_BYTE_MASK) != 0;
    const int16 minByte2    = be ? s.ReadI16() : s.ReadI16LE();
    const int16 maxByte2    = be ? s.ReadI16() : s.ReadI16LE();
    const int16 minByte1    = be ? s.ReadI16() : s.ReadI16LE();
    const int16 maxByte1    = be ? s.ReadI16() : s.ReadI16LE();
    const int16 defaultChar = be ? s.ReadI16() : s.ReadI16LE();
    out.metrics.defaultChar = static_cast<uint32>(defaultChar);

    const int32 numEncodings = (maxByte2 - minByte2 + 1) * (maxByte1 - minByte1 + 1);
    (void)minByte1; (void)maxByte1;

    for (int32 i = 0; i < numEncodings && i < static_cast<int32>(out.numGlyphs); ++i) {
        const uint16 glyphIdx = be ? s.ReadU16() : s.ReadU16LE();
        if (glyphIdx != 0xFFFF && glyphIdx < out.numGlyphs) {
            const uint32 code = static_cast<uint32>(minByte2) + static_cast<uint32>(i);
            out.glyphs[glyphIdx].encoding = code;
        }
    }
    return !s.HasError();
}

bool NkPCFParser::ParseAccelTable(
    NkStreamReader& s, uint32 format,
    NkBDFFont& out) noexcept
{
    const bool be = (format & PCF_BYTE_MASK) != 0;
    s.Skip(8); // flags
    const int32 fontAscent  = be ? s.ReadI32() : s.ReadI32LE();
    const int32 fontDescent = be ? s.ReadI32() : s.ReadI32LE();
    out.metrics.fontAscent  = fontAscent;
    out.metrics.fontDescent = fontDescent;
    return !s.HasError();
}

} // namespace nkentseu
