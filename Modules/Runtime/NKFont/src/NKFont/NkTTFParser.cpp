/**
 * @File    NkTTFParser.cpp
 * @Brief   Implémentation du parser TTF/OTF avec support complet des tables.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * Ce fichier implémente le parsing des polices TrueType et OpenType,
 * incluant les tables essentielles pour le rendu de texte.
 */

#include "pch.h"
#include "NKFont/NkTTFParser.h"
#include <cstring>

namespace nkentseu {

// ============================================================================
//  NkTTFFont — méthodes inline
// ============================================================================

uint16 NkTTFFont::GetGlyphId(uint32 codepoint) const noexcept {
    // Recherche dichotomique dans les plages cmap
    uint32 lo = 0, hi = numCmapRanges;
    while (lo < hi) {
        const uint32 mid = (lo + hi) >> 1;
        const NkTTFCmapRange& r = cmapRanges[mid];
        if (codepoint < r.startCode) {
            hi = mid;
        } else if (codepoint > r.endCode) {
            lo = mid + 1;
        } else {
            if (r.isDelta) {
                return static_cast<uint16>((codepoint + static_cast<uint32>(r.idDelta)) & 0xFFFF);
            } else {
                return static_cast<uint16>(r.glyphId + (codepoint - r.startCode));
            }
        }
    }
    return 0;
}

NkTTFHMetric NkTTFFont::GetHMetric(uint16 glyphId) const noexcept {
    if (!hmetrics || numHMetrics == 0) {
        NkTTFHMetric empty = {0, 0};
        return empty;
    }
    if (glyphId < numHMetrics) {
        return hmetrics[glyphId];
    }
    // Dernière entrée répétée pour les glyphes sans largeur individuelle
    return hmetrics[numHMetrics - 1];
}

uint32 NkTTFFont::GetGlyfOffset(uint16 glyphId) const noexcept {
    if (!locaOffsets || glyphId >= maxp.numGlyphs) {
        return 0xFFFFFFFFu;
    }
    return locaOffsets[glyphId];
}

// ============================================================================
//  Utilitaires
// ============================================================================

uint32 NkTTFParser::FindTable(const NkTTFTableRecord* records, uint16 numTables, uint32 tag) noexcept {
    for (uint16 i = 0; i < numTables; ++i) {
        if (records[i].tag == tag) {
            return records[i].offset;
        }
    }
    return 0;
}

uint16 NkTTFParser::BinarySearchKern(const NkTTFKernPair* pairs, uint32 count, uint16 left, uint16 right) noexcept {
    const uint32 key = (static_cast<uint32>(left) << 16) | right;
    uint32 lo = 0, hi = count;
    while (lo < hi) {
        const uint32 mid = (lo + hi) >> 1;
        const uint32 cur = (static_cast<uint32>(pairs[mid].left) << 16) | pairs[mid].right;
        if (key < cur) {
            hi = mid;
        } else if (key > cur) {
            lo = mid + 1;
        } else {
            return static_cast<uint16>(mid);
        }
    }
    return 0xFFFF;
}

void NkTTFParser::ConvertUTF16ToUTF8(const uint16* src, usize srcLen, char* dst, usize dstSize) noexcept {
    if (!src || !dst || dstSize == 0) return;
    
    usize idx = 0;
    for (usize i = 0; i < srcLen && idx < dstSize - 1; ++i) {
        uint32 cp = src[i];
        
        // Surrogate pairs (UTF-16)
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < srcLen) {
            uint32 low = src[i + 1];
            if (low >= 0xDC00 && low <= 0xDFFF) {
                cp = ((cp - 0xD800) << 10) + (low - 0xDC00) + 0x10000;
                ++i;
            }
        }
        
        // Encoder en UTF-8
        if (cp < 0x80) {
            dst[idx++] = static_cast<char>(cp);
        } else if (cp < 0x800) {
            if (idx + 1 >= dstSize - 1) break;
            dst[idx++] = static_cast<char>(0xC0 | ((cp >> 6) & 0x1F));
            dst[idx++] = static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            if (idx + 2 >= dstSize - 1) break;
            dst[idx++] = static_cast<char>(0xE0 | ((cp >> 12) & 0x0F));
            dst[idx++] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            dst[idx++] = static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x110000) {
            if (idx + 3 >= dstSize - 1) break;
            dst[idx++] = static_cast<char>(0xF0 | ((cp >> 18) & 0x07));
            dst[idx++] = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            dst[idx++] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            dst[idx++] = static_cast<char>(0x80 | (cp & 0x3F));
        }
    }
    dst[idx] = 0;
}

// ============================================================================
//  Parse — point d'entrée principal
// ============================================================================

bool NkTTFParser::Parse(
    const uint8* data, usize size,
    NkMemArena& arena, NkTTFFont& out) noexcept
{
    // Initialisation
    ::memset(&out, 0, sizeof(out));
    if (!data || size < 12) return false;

    NkStreamReader s(data, size);
    out.rawData = data;
    out.rawSize = size;

    // Vérification de la signature
    const uint32 sfVersion = s.ReadU32();
    if (sfVersion != kTTF_SFVERSION_TRUE  &&
        sfVersion != kTTF_SFVERSION_OTF   &&
        sfVersion != kTTF_SFVERSION_TRUE2 &&
        sfVersion != kTTF_SFVERSION_TYPE1) {
        return false;
    }

    out.hasCFF = (sfVersion == kTTF_SFVERSION_OTF);

    // Parse le table directory
    uint16 numTables = 0;
    NkTTFTableRecord* tableRecords = nullptr;
    if (!ParseTableDirectory(s, out, arena, numTables, tableRecords)) {
        return false;
    }

    const uint8* rawData = out.rawData;
    const usize rawSize = out.rawSize;

    // Macro pour les fonctions SANS arena
    #define PARSE_TABLE_NO_ARENA(tag, parserFunc) \
        do { \
            uint32 off = FindTable(tableRecords, numTables, Tag::tag); \
            if (off != 0) { \
                NkStreamReader ts(rawData + off, (off < rawSize) ? (rawSize - off) : 0); \
                if (!parserFunc(ts, out)) return false; \
            } \
        } while(0)

    // Macro pour les fonctions AVEC arena
    #define PARSE_TABLE_WITH_ARENA(tag, parserFunc) \
        do { \
            uint32 off = FindTable(tableRecords, numTables, Tag::tag); \
            if (off != 0) { \
                NkStreamReader ts(rawData + off, (off < rawSize) ? (rawSize - off) : 0); \
                if (!parserFunc(ts, out, arena)) return false; \
            } \
        } while(0)

    // Tables obligatoires (sans arena)
    PARSE_TABLE_NO_ARENA(head, ParseHead);
    PARSE_TABLE_NO_ARENA(hhea, ParseHhea);
    PARSE_TABLE_NO_ARENA(maxp, ParseMaxp);
    
    // Tables obligatoires avec arena
    PARSE_TABLE_WITH_ARENA(hmtx, ParseHmtx);
    PARSE_TABLE_WITH_ARENA(loca, ParseLoca);
    PARSE_TABLE_WITH_ARENA(cmap, ParseCmap);
    
    // Table name avec arena
    PARSE_TABLE_WITH_ARENA(name, ParseName);

    // Tables optionnelles sans arena
    PARSE_TABLE_NO_ARENA(OS_2, ParseOS2);
    PARSE_TABLE_NO_ARENA(post, ParsePost);
    
    // Tables optionnelles avec arena
    PARSE_TABLE_WITH_ARENA(kern, ParseKern);
    
    // Tables optionnelles sans arena
    PARSE_TABLE_NO_ARENA(cvt,  ParseCvt);
    PARSE_TABLE_NO_ARENA(fpgm, ParseFpgm);
    PARSE_TABLE_NO_ARENA(prep, ParsePrep);

    #undef PARSE_TABLE_NO_ARENA
    #undef PARSE_TABLE_WITH_ARENA

    // Récupération des offsets des tables spéciales
    for (uint16 i = 0; i < numTables; ++i) {
        if (tableRecords[i].tag == Tag::glyf) {
            out.glyfOffset = tableRecords[i].offset;
            out.glyfLength = tableRecords[i].length;
        }
        if (tableRecords[i].tag == Tag::GPOS) out.hasGPOS = true;
        if (tableRecords[i].tag == Tag::GSUB) out.hasGSUB = true;
    }

    out.isValid = true;
    return true;
}

// ============================================================================
//  ParseTableDirectory
// ============================================================================

bool NkTTFParser::ParseTableDirectory(
    NkStreamReader& s, NkTTFFont& out, NkMemArena& arena,
    uint16& numTables, NkTTFTableRecord*& records) noexcept
{
    const uint16 numTablesVal = s.ReadU16();
    numTables = numTablesVal;
    
    s.Skip(6); // searchRange, entrySelector, rangeShift

    if (numTables == 0 || numTables > 64) return false;

    // Alloue et lit les records
    records = arena.Alloc<NkTTFTableRecord>(numTables);
    if (!records) return false;

    for (uint16 i = 0; i < numTables; ++i) {
        records[i].tag      = s.ReadTag();
        records[i].checksum = s.ReadU32();
        records[i].offset   = s.ReadU32();
        records[i].length   = s.ReadU32();
        
        // Validation basique
        if (records[i].offset >= out.rawSize || records[i].length > out.rawSize - records[i].offset) {
            return false;
        }
    }
    
    return !s.HasError();
}

// ============================================================================
//  Tables individuelles
// ============================================================================

bool NkTTFParser::ParseHead(NkStreamReader& s, NkTTFFont& out) noexcept {
    out.head.majorVersion      = s.ReadU16();
    out.head.minorVersion      = s.ReadU16();
    out.head.fontRevision      = s.ReadFixed();
    s.ReadU32(); // checkSumAdjustment
    const uint32 magic = s.ReadU32();
    if (magic != 0x5F0F3CF5u) return false; // magic number obligatoire
    s.ReadU16(); // flags
    out.head.unitsPerEm        = s.ReadU16();
    if (out.head.unitsPerEm < 16 || out.head.unitsPerEm > 16384) return false;
    s.Skip(16); // created + modified (LONGDATETIME x2)
    out.head.xMin              = s.ReadFWord();
    out.head.yMin              = s.ReadFWord();
    out.head.xMax              = s.ReadFWord();
    out.head.yMax              = s.ReadFWord();
    out.head.macStyle          = s.ReadU16();
    out.head.lowestRecPPEM     = s.ReadU16();
    out.head.fontDirectionHint = s.ReadI16();
    out.head.indexToLocFormat  = s.ReadI16();
    out.head.glyphDataFormat   = s.ReadI16();
    return !s.HasError();
}

bool NkTTFParser::ParseHhea(NkStreamReader& s, NkTTFFont& out) noexcept {
    s.Skip(4); // version
    out.hhea.ascender           = s.ReadFWord();
    out.hhea.descender          = s.ReadFWord();
    out.hhea.lineGap            = s.ReadFWord();
    out.hhea.advanceWidthMax    = s.ReadUFWord();
    out.hhea.minLeftSideBearing = s.ReadFWord();
    out.hhea.minRightSideBearing= s.ReadFWord();
    out.hhea.xMaxExtent         = s.ReadFWord();
    out.hhea.caretSlopeRise     = s.ReadI16();
    out.hhea.caretSlopeRun      = s.ReadI16();
    out.hhea.caretOffset        = s.ReadI16();
    s.Skip(8); // reserved
    out.hhea.metricDataFormat   = s.ReadI16();
    out.hhea.numberOfHMetrics   = s.ReadU16();
    return !s.HasError();
}

bool NkTTFParser::ParseMaxp(NkStreamReader& s, NkTTFFont& out) noexcept {
    const uint32 version = s.ReadU32();
    out.maxp.numGlyphs = s.ReadU16();
    if (out.maxp.numGlyphs == 0) return false;
    
    if (version == 0x00010000u) {
        out.maxp.maxPoints            = s.ReadU16();
        out.maxp.maxContours          = s.ReadU16();
        out.maxp.maxCompositePoints   = s.ReadU16();
        out.maxp.maxCompositeContours = s.ReadU16();
        out.maxp.maxZones             = s.ReadU16();
        out.maxp.maxTwilightPoints    = s.ReadU16();
        out.maxp.maxStorage           = s.ReadU16();
        out.maxp.maxFunctionDefs      = s.ReadU16();
        out.maxp.maxInstructionDefs   = s.ReadU16();
        out.maxp.maxStackElements     = s.ReadU16();
        out.maxp.maxSizeOfInstructions= s.ReadU16();
        out.maxp.maxComponentElements = s.ReadU16();
        out.maxp.maxComponentDepth    = s.ReadU16();
    }
    return !s.HasError();
}

bool NkTTFParser::ParseHmtx(NkStreamReader& s, NkTTFFont& out, NkMemArena& arena) noexcept {
    const uint16 nHM     = out.hhea.numberOfHMetrics;
    const uint16 nGlyphs = out.maxp.numGlyphs;
    if (nHM == 0 || nHM > nGlyphs) return false;

    out.hmetrics = arena.Alloc<NkTTFHMetric>(nGlyphs);
    if (!out.hmetrics) return false;
    out.numHMetrics = nHM;

    for (uint16 i = 0; i < nHM; ++i) {
        out.hmetrics[i].advanceWidth = s.ReadU16();
        out.hmetrics[i].lsb          = s.ReadI16();
    }
    
    // Glyphes sans entrée individuelle héritent du dernier advanceWidth
    const uint16 lastAW = out.hmetrics[nHM - 1].advanceWidth;
    for (uint16 i = nHM; i < nGlyphs; ++i) {
        out.hmetrics[i].advanceWidth = lastAW;
        out.hmetrics[i].lsb          = s.ReadI16();
    }
    return !s.HasError();
}

bool NkTTFParser::ParseLoca(NkStreamReader& s, NkTTFFont& out, NkMemArena& arena) noexcept {
    const uint16 n = out.maxp.numGlyphs;
    out.locaOffsets = arena.Alloc<uint32>(n + 1);
    if (!out.locaOffsets) return false;

    if (out.head.indexToLocFormat == 0) {
        // Format court : offsets / 2
        for (uint16 i = 0; i <= n; ++i) {
            out.locaOffsets[i] = static_cast<uint32>(s.ReadU16()) << 1;
        }
    } else {
        // Format long : offsets directs
        for (uint16 i = 0; i <= n; ++i) {
            out.locaOffsets[i] = s.ReadU32();
        }
    }
    return !s.HasError();
}

bool NkTTFParser::ParseCmap(NkStreamReader& s, NkTTFFont& out, NkMemArena& arena) noexcept {
    const size_t cmapStart = s.Tell() - 4; // Revenir au début de la table cmap
    s.ReadU16(); // version
    const uint16 numSubtables = s.ReadU16();

    // Cherche la meilleure sous-table
    uint32 bestOffset = 0;
    uint16 bestFormat = 0;
    uint16 bestPriority = 0xFFFF;

    for (uint16 i = 0; i < numSubtables; ++i) {
        const uint16 platformId = s.ReadU16();
        const uint16 encodingId = s.ReadU16();
        const uint32 offset     = s.ReadU32();

        const bool isUnicodeFull = (platformId == 3 && encodingId == 10)
                                 || (platformId == 0 && encodingId == 4);
        const bool isUnicodeBMP  = (platformId == 3 && encodingId ==  1)
                                 || (platformId == 0 && encodingId == 3);

        uint16 priority = 0xFFFF;
        if (isUnicodeFull) priority = 0;
        else if (isUnicodeBMP) priority = 1;
        
        if (priority < bestPriority) {
            bestOffset = offset;
            bestFormat = (priority == 0) ? 12 : 4;
            bestPriority = priority;
        }
    }

    if (bestOffset == 0) return false;

    // Créer un sous-stream à partir des données brutes
    const uint8* tableData = out.rawData + bestOffset;
    NkStreamReader sub(tableData, out.rawSize - bestOffset);
    const uint16 fmt = sub.ReadU16();

    if (fmt == 4)  return ParseCmapFormat4(sub, out, arena);
    if (fmt == 12) return ParseCmapFormat12(sub, out, arena);
    return false;
}

bool NkTTFParser::ParseCmapFormat4(NkStreamReader& s, NkTTFFont& out, NkMemArena& arena) noexcept {
    s.ReadU16(); // length
    s.ReadU16(); // language
    const uint16 segCount = s.ReadU16() >> 1;
    s.Skip(6); // searchRange, entrySelector, rangeShift

    if (segCount == 0 || segCount > 8192) return false;

    // Lit les 4 tableaux du format 4
    uint16* endCodes    = arena.Alloc<uint16>(segCount);
    uint16* startCodes  = arena.Alloc<uint16>(segCount);
    int16*  idDeltas    = arena.Alloc<int16> (segCount);
    uint16* idRangeOffs = arena.Alloc<uint16>(segCount);
    if (!endCodes || !startCodes || !idDeltas || !idRangeOffs) return false;

    for (uint16 i = 0; i < segCount; ++i) endCodes[i]    = s.ReadU16();
    s.ReadU16(); // reservedPad
    for (uint16 i = 0; i < segCount; ++i) startCodes[i]  = s.ReadU16();
    for (uint16 i = 0; i < segCount; ++i) idDeltas[i]    = s.ReadI16();
    for (uint16 i = 0; i < segCount; ++i) idRangeOffs[i] = s.ReadU16();
    if (s.HasError()) return false;

    // Compte les plages valides (exclut le sentinel 0xFFFF)
    uint32 numRanges = 0;
    for (uint16 i = 0; i < segCount; ++i) {
        if (startCodes[i] != 0xFFFF) numRanges++;
    }

    out.cmapRanges = arena.Alloc<NkTTFCmapRange>(numRanges);
    if (!out.cmapRanges) return false;
    out.numCmapRanges = numRanges;

    uint32 ri = 0;
    for (uint16 i = 0; i < segCount && ri < numRanges; ++i) {
        if (startCodes[i] == 0xFFFF) continue;
        out.cmapRanges[ri].startCode = startCodes[i];
        out.cmapRanges[ri].endCode   = endCodes[i];
        out.cmapRanges[ri].idDelta   = idDeltas[i];
        out.cmapRanges[ri].isDelta   = (idRangeOffs[i] == 0);
        out.cmapRanges[ri].glyphId   = 0;
        ++ri;
    }
    return true;
}

bool NkTTFParser::ParseCmapFormat12(NkStreamReader& s, NkTTFFont& out, NkMemArena& arena) noexcept {
    s.ReadU16(); // reserved
    s.ReadU32(); // length
    s.ReadU32(); // language
    const uint32 numGroups = s.ReadU32();
    if (numGroups == 0 || numGroups > 65536) return false;

    out.cmapRanges = arena.Alloc<NkTTFCmapRange>(numGroups);
    if (!out.cmapRanges) return false;
    out.numCmapRanges = numGroups;

    for (uint32 i = 0; i < numGroups; ++i) {
        out.cmapRanges[i].startCode = s.ReadU32();
        out.cmapRanges[i].endCode   = s.ReadU32();
        out.cmapRanges[i].glyphId   = s.ReadU32();
        out.cmapRanges[i].isDelta   = false;
        out.cmapRanges[i].idDelta   = 0;
    }
    return !s.HasError();
}

bool NkTTFParser::ParseKern(NkStreamReader& s, NkTTFFont& out, NkMemArena& arena) noexcept {
    s.ReadU16(); // version
    const uint16 nTables = s.ReadU16();

    for (uint16 t = 0; t < nTables; ++t) {
        s.ReadU16(); // version
        const uint16 length  = s.ReadU16();
        const uint16 coverage= s.ReadU16();
        const uint16 format  = (coverage >> 8) & 0xFF;
        const bool   isHoriz = !(coverage & 0x01); // bit 0 = vertical

        if (format != 0 || !isHoriz) {
            s.Skip(length - 6);
            continue;
        }

        const uint16 nPairs = s.ReadU16();
        s.Skip(6); // searchRange, entrySelector, rangeShift

        out.kernPairs = arena.Alloc<NkTTFKernPair>(nPairs);
        if (!out.kernPairs) return false;
        out.numKernPairs = nPairs;

        for (uint16 i = 0; i < nPairs; ++i) {
            out.kernPairs[i].left  = s.ReadU16();
            out.kernPairs[i].right = s.ReadU16();
            out.kernPairs[i].value = s.ReadI16();
        }
        break; // On prend la première table horizontale format 0
    }
    return !s.HasError();
}

bool NkTTFParser::ParseOS2(NkStreamReader& s, NkTTFFont& out) noexcept {
    s.ReadU16(); // version
    out.os2.xAvgCharWidth  = s.ReadI16();
    out.os2.usWeightClass  = s.ReadU16();
    out.os2.usWidthClass   = s.ReadU16();
    out.os2.fsType         = s.ReadU16();
    s.Skip(10 * 2); // sTypoAscender etc. — version 0
    s.Skip(4 + 4 + 4 + 4); // ulUnicodeRange1..4
    s.Skip(4);              // achVendID
    s.ReadU16();            // fsSelection
    s.ReadU16();            // usFirstCharIndex
    s.ReadU16();            // usLastCharIndex
    out.os2.sTypoAscender  = s.ReadI16();
    out.os2.sTypoDescender = s.ReadI16();
    out.os2.sTypoLineGap   = s.ReadI16();
    out.os2.usWinAscent    = s.ReadU16();
    out.os2.usWinDescent   = s.ReadU16();
    
    // Version >= 2
    if (!s.HasError() && s.Remaining() >= 10) {
        s.Skip(8); // ulCodePageRange
        out.os2.sxHeight   = s.ReadI16();
        out.os2.sCapHeight = s.ReadI16();
        out.os2.usDefaultChar = s.ReadU16();
        out.os2.usBreakChar   = s.ReadU16();
    }
    return !s.HasError();
}

bool NkTTFParser::ParsePost(NkStreamReader& s, NkTTFFont& out) noexcept {
    s.ReadU32(); // version
    out.post.italicAngle          = s.ReadFixed();
    out.post.underlinePosition    = s.ReadFWord();
    out.post.underlineThickness   = s.ReadFWord();
    out.post.isFixedPitch         = s.ReadU32();
    out.post.minMemType42         = s.ReadU32();
    out.post.maxMemType42         = s.ReadU32();
    return !s.HasError();
}

bool NkTTFParser::ParseCvt(NkStreamReader& s, NkTTFFont& out) noexcept {
    // cvt est exposé via pointeur direct
    out.cvtData = s.CurrentPtr();
    out.cvtLength = static_cast<uint32>(s.Remaining());
    return true;
}

bool NkTTFParser::ParseFpgm(NkStreamReader& s, NkTTFFont& out) noexcept {
    out.fpgmData = s.CurrentPtr();
    out.fpgmLength = static_cast<uint32>(s.Remaining());
    return true;
}

bool NkTTFParser::ParsePrep(NkStreamReader& s, NkTTFFont& out) noexcept {
    out.prepData = s.CurrentPtr();
    out.prepLength = static_cast<uint32>(s.Remaining());
    return true;
}

bool NkTTFParser::ParseName(NkStreamReader& s, NkTTFFont& out, NkMemArena& arena) noexcept {
    uint16 format = s.ReadU16();
    
    if (format != 0 && format != 1) return true; // Format non supporté, ignorer
    
    uint16 numRecords = s.ReadU16();
    uint16 storageOffset = s.ReadU16();
    
    // Allouer les enregistrements
    out.name.records = arena.Alloc<NkTTFNameRecord>(numRecords);
    if (!out.name.records) return false;
    out.name.numRecords = numRecords;
    
    // Lire tous les enregistrements
    for (uint16 i = 0; i < numRecords; ++i) {
        auto& rec = out.name.records[i];
        rec.platformId = s.ReadU16();
        rec.encodingId = s.ReadU16();
        rec.languageId = s.ReadU16();
        rec.nameId     = s.ReadU16();
        rec.length     = s.ReadU16();
        rec.offset     = s.ReadU16();
        rec.string = nullptr;
    }
    
    // Stocker les données des chaînes
    out.name.stringData = out.rawData + storageOffset;
    
    // Initialiser les chaînes vides
    out.name.familyName[0] = 0;
    out.name.styleName[0] = 0;
    out.name.fullName[0] = 0;
    out.name.copyright[0] = 0;
    
    // Parcourir pour extraire les chaînes intéressantes
    for (uint16 i = 0; i < numRecords; ++i) {
        const auto& rec = out.name.records[i];
        
        // Platform 3 = Windows, encoding 1 = Unicode BMP, language 0x0409 = English US
        if (rec.platformId == 3 && rec.encodingId == 1 && rec.languageId == 0x0409) {
            const uint8* strData = out.name.stringData + rec.offset;
            usize strLen = rec.length / 2; // UTF-16
            
            if (rec.nameId == 1 && strLen < 64) { // Family name
                ConvertUTF16ToUTF8(reinterpret_cast<const uint16*>(strData), strLen, 
                                   out.name.familyName, sizeof(out.name.familyName));
            } 
            else if (rec.nameId == 2 && strLen < 32) { // Style name
                ConvertUTF16ToUTF8(reinterpret_cast<const uint16*>(strData), strLen,
                                   out.name.styleName, sizeof(out.name.styleName));
            }
            else if (rec.nameId == 4 && strLen < 128) { // Full name
                ConvertUTF16ToUTF8(reinterpret_cast<const uint16*>(strData), strLen,
                                   out.name.fullName, sizeof(out.name.fullName));
            }
            else if (rec.nameId == 0 && strLen < 256) { // Copyright
                ConvertUTF16ToUTF8(reinterpret_cast<const uint16*>(strData), strLen,
                                   out.name.copyright, sizeof(out.name.copyright));
            }
        }
    }
    
    return true;
}

// ============================================================================
//  ParseGlyph
// ============================================================================

bool NkTTFParser::ParseGlyph(
    const NkTTFFont& font, uint16 glyphId,
    NkMemArena& arena, NkTTFGlyphData& out) noexcept
{
    ::memset(&out, 0, sizeof(out));
    if (!font.isValid || glyphId >= font.maxp.numGlyphs) return false;

    const uint32 glyfOff = font.GetGlyfOffset(glyphId);
    const uint32 nextOff = font.locaOffsets[glyphId + 1];
    if (glyfOff == nextOff) {
        out.numContours = 0;
        return true; // glyphe vide
    }

    if (glyfOff + 10 > font.rawSize) return false;

    NkStreamReader s(font.rawData + font.glyfOffset + glyfOff,
                     font.rawSize - font.glyfOffset - glyfOff);

    out.numContours = s.ReadI16();
    const int16 xMin = s.ReadFWord();
    const int16 yMin = s.ReadFWord();
    const int16 xMax = s.ReadFWord();
    const int16 yMax = s.ReadFWord();

    if (out.numContours > 0) {
        out.simple.xMin = xMin;
        out.simple.yMin = yMin;
        out.simple.xMax = xMax;
        out.simple.yMax = yMax;
        return ParseSimpleGlyph(s, out.numContours, out.simple, arena);
    } else if (out.numContours < 0) {
        out.composite.xMin = xMin;
        out.composite.yMin = yMin;
        out.composite.xMax = xMax;
        out.composite.yMax = yMax;
        return ParseCompositeGlyph(s, out.composite, arena);
    }
    return true;
}

bool NkTTFParser::ParseSimpleGlyph(
    NkStreamReader& s, int16 numContours,
    NkTTFSimpleGlyph& out, NkMemArena& arena) noexcept
{
    if (numContours <= 0 || numContours > kTTF_MAX_CONTOURS) return false;

    uint16* endPtsOfContours = arena.Alloc<uint16>(numContours);
    if (!endPtsOfContours) return false;
    for (int16 i = 0; i < numContours; ++i) {
        endPtsOfContours[i] = s.ReadU16();
    }

    const uint16 numPoints = endPtsOfContours[numContours - 1] + 1;
    if (numPoints > kTTF_MAX_POINTS) return false;

    out.instructionLength = s.ReadU16();
    out.instructions = nullptr;
    if (out.instructionLength > 0) {
        out.instructions = arena.Alloc<uint8>(out.instructionLength);
        if (!out.instructions) return false;
        s.ReadBytes(out.instructions, out.instructionLength);
    }

    // Alloue les contours et points
    out.contours = arena.Alloc<NkTTFContour>(numContours);
    NkTTFPoint* pts = arena.Alloc<NkTTFPoint>(numPoints);
    if (!out.contours || !pts) return false;
    out.numContours = numContours;

    // Lecture des flags avec compression RLE (repeat)
    uint8* flags = arena.Alloc<uint8>(numPoints);
    if (!flags) return false;
    for (uint16 i = 0; i < numPoints; ) {
        const uint8 flag = s.ReadU8();
        flags[i++] = flag;
        if (flag & kPointFlag_Repeat) {
            uint8 count = s.ReadU8();
            while (count-- && i < numPoints) flags[i++] = flag;
        }
    }

    // Lecture des coordonnées X (delta-encodées)
    int16 curX = 0;
    for (uint16 i = 0; i < numPoints; ++i) {
        const uint8 f = flags[i];
        if (f & kPointFlag_XShortVec) {
            const int16 dx = s.ReadU8();
            curX += (f & kPointFlag_XSameOrPos) ? dx : -dx;
        } else if (!(f & kPointFlag_XSameOrPos)) {
            curX += s.ReadI16();
        }
        pts[i].x = curX;
    }

    // Lecture des coordonnées Y (delta-encodées)
    int16 curY = 0;
    for (uint16 i = 0; i < numPoints; ++i) {
        const uint8 f = flags[i];
        if (f & kPointFlag_YShortVec) {
            const int16 dy = s.ReadU8();
            curY += (f & kPointFlag_YSameOrPos) ? dy : -dy;
        } else if (!(f & kPointFlag_YSameOrPos)) {
            curY += s.ReadI16();
        }
        pts[i].y     = curY;
        pts[i].flags = flags[i];
    }

    // Découpe les points en contours
    uint16 ptStart = 0;
    for (int16 c = 0; c < numContours; ++c) {
        out.contours[c].points    = pts + ptStart;
        out.contours[c].numPoints = endPtsOfContours[c] - ptStart + 1;
        ptStart = endPtsOfContours[c] + 1;
    }

    return !s.HasError();
}

bool NkTTFParser::ParseCompositeGlyph(
    NkStreamReader& s, NkTTFCompositeGlyph& out, NkMemArena& arena) noexcept
{
    NkTTFComponent tmp[kTTF_MAX_COMPONENTS];
    uint16 count = 0;

    uint16 flags = kCompFlag_MoreComponents;
    while ((flags & kCompFlag_MoreComponents) && count < kTTF_MAX_COMPONENTS) {
        flags = s.ReadU16();
        NkTTFComponent& comp = tmp[count++];
        comp.glyphIndex    = s.ReadU16();
        comp.hasTransform  = false;
        comp.useMyMetrics  = (flags & kCompFlag_UseMyMetrics) != 0;
        comp.transform     = NkMatrix2x2::Identity();

        // Arguments : translation ou indice de point
        if (flags & kCompFlag_ArgsAreWords) {
            comp.dx = s.ReadI16();
            comp.dy = s.ReadI16();
        } else {
            comp.dx = static_cast<int8>(s.ReadU8());
            comp.dy = static_cast<int8>(s.ReadU8());
        }
        
        // Transformation
        if (flags & kCompFlag_WeHaveScale) {
            const int16 sc = s.ReadF2Dot14();
            comp.transform.xx = F16Dot16::FromRaw(static_cast<int32>(sc) << 2);
            comp.transform.yy = comp.transform.xx;
            comp.hasTransform = true;
        } else if (flags & kCompFlag_WeHaveXYScale) {
            const int16 sx = s.ReadF2Dot14();
            const int16 sy = s.ReadF2Dot14();
            comp.transform.xx = F16Dot16::FromRaw(static_cast<int32>(sx) << 2);
            comp.transform.yy = F16Dot16::FromRaw(static_cast<int32>(sy) << 2);
            comp.hasTransform = true;
        } else if (flags & kCompFlag_WeHave2x2) {
            const int16 a = s.ReadF2Dot14();
            const int16 b = s.ReadF2Dot14();
            const int16 c = s.ReadF2Dot14();
            const int16 d = s.ReadF2Dot14();
            comp.transform.xx = F16Dot16::FromRaw(static_cast<int32>(a) << 2);
            comp.transform.xy = F16Dot16::FromRaw(static_cast<int32>(b) << 2);
            comp.transform.yx = F16Dot16::FromRaw(static_cast<int32>(c) << 2);
            comp.transform.yy = F16Dot16::FromRaw(static_cast<int32>(d) << 2);
            comp.hasTransform = true;
        }
        comp.flags = flags;
    }

    // Instructions optionnelles
    out.instructions = nullptr;
    out.instructionLength = 0;
    if (flags & kCompFlag_WeHaveInstr) {
        out.instructionLength = s.ReadU16();
        if (out.instructionLength > 0) {
            out.instructions = arena.Alloc<uint8>(out.instructionLength);
            if (out.instructions) {
                s.ReadBytes(out.instructions, out.instructionLength);
            }
        }
    }

    out.numComponents = count;
    out.components = arena.AllocCopy<NkTTFComponent>(tmp, count);
    return !s.HasError() && out.components != nullptr;
}

// ============================================================================
//  DecomposeGlyph
// ============================================================================

bool NkTTFParser::DecomposeGlyph(
    const NkTTFFont& font, uint16 glyphId, uint16 ppem,
    NkMemArena& arena,
    NkPoint26_6*& outPoints, uint8*& outTags,
    uint16*& outContourEnds, uint16& outNumPoints, uint16& outNumContours) noexcept
{
    const uint16 maxPts = kTTF_MAX_POINTS * 4;
    const uint16 maxCnt = kTTF_MAX_CONTOURS * 4;

    outPoints      = arena.Alloc<NkPoint26_6>(maxPts);
    outTags        = arena.Alloc<uint8>      (maxPts);
    outContourEnds = arena.Alloc<uint16>     (maxCnt);
    outNumPoints   = 0;
    outNumContours = 0;

    if (!outPoints || !outTags || !outContourEnds) return false;

    return DecomposeGlyphRec(font, glyphId,
        NkMatrix2x2::Identity(), 0, 0, ppem, arena,
        outPoints, outTags, outContourEnds,
        outNumPoints, outNumContours,
        maxPts, maxCnt, 0);
}

bool NkTTFParser::DecomposeGlyphRec(
    const NkTTFFont& font, uint16 glyphId,
    const NkMatrix2x2& xform, int32 dx, int32 dy,
    uint16 ppem, NkMemArena& arena,
    NkPoint26_6* points, uint8* tags, uint16* contourEnds,
    uint16& pointCount, uint16& contourCount,
    uint16 maxPoints, uint16 maxContours, int32 depth) noexcept
{
    if (depth > 8) return false; // protection anti-boucle infinie

    NkTTFGlyphData glyph;
    if (!ParseGlyph(font, glyphId, arena, glyph)) return false;
    if (glyph.numContours == 0) return true; // glyphe vide (espace)

    // Facteur de conversion : design units → F26Dot6
    // scale = ppem * 64 / unitsPerEm
    const int32 scale = (static_cast<int32>(ppem) * 64) / font.head.unitsPerEm;

    if (glyph.numContours > 0) {
        const NkTTFSimpleGlyph& sg = glyph.simple;
        for (int16 c = 0; c < sg.numContours; ++c) {
            if (contourCount >= maxContours) return false;
            const NkTTFContour& cont = sg.contours[c];
            for (uint16 p = 0; p < cont.numPoints; ++p) {
                if (pointCount >= maxPoints) return false;
                F26Dot6 px = F26Dot6::FromRaw((cont.points[p].x * scale));
                F26Dot6 py = F26Dot6::FromRaw((cont.points[p].y * scale));
                // Applique transformation + translation
                if (!xform.IsIdentity()) xform.Transform(px, py);
                px.raw += dx * scale;
                py.raw += dy * scale;
                points[pointCount] = {px, py};
                tags[pointCount]   = cont.points[p].flags;
                ++pointCount;
            }
            contourEnds[contourCount++] = pointCount - 1;
        }
    } else {
        // Composite : récursion sur chaque composant
        const NkTTFCompositeGlyph& cg = glyph.composite;
        for (uint16 i = 0; i < cg.numComponents; ++i) {
            const NkTTFComponent& comp = cg.components[i];
            NkMatrix2x2 combined = comp.hasTransform ? xform * comp.transform : xform;
            if (!DecomposeGlyphRec(
                    font, comp.glyphIndex, combined,
                    dx + comp.dx, dy + comp.dy, ppem, arena,
                    points, tags, contourEnds,
                    pointCount, contourCount,
                    maxPoints, maxContours, depth + 1)) {
                return false;
            }
        }
    }
    return true;
}

} // namespace nkentseu