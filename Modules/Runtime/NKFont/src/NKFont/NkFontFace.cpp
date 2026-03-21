/**
 * @File    NkFontFace.cpp
 * @Brief   Implémentation NkFontFace + NkFontLibrary.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 */
#include "pch.h"
#include "NKFont/NkFontFace.h"
#include "NKFont/NkWOFFParser.h"
#include <cstring>

namespace nkentseu {

// ─────────────────────────────────────────────────────────────────────────────
//  NkFontFace — métriques
// ─────────────────────────────────────────────────────────────────────────────

int32 NkFontFace::GetAscender() const noexcept {
    if (mFormat == NkFontFormat::TTF || mFormat == NkFontFormat::OTF_CFF
     || mFormat == NkFontFormat::WOFF) {
        const int32 scale = (static_cast<int32>(mPPEM) << F26Dot6::SHIFT)
                          / mTTF.head.unitsPerEm;
        // Préfère sTypoAscender si disponible
        const int16 asc = mTTF.os2.sTypoAscender != 0
                        ? mTTF.os2.sTypoAscender
                        : mTTF.hhea.ascender;
        return (static_cast<int32>(asc) * scale) >> F26Dot6::SHIFT;
    }
    if (mFormat == NkFontFormat::BDF || mFormat == NkFontFormat::PCF)
        return mBDF.metrics.fontAscent;
    return static_cast<int32>(mPPEM);
}

int32 NkFontFace::GetDescender() const noexcept {
    if (mFormat == NkFontFormat::TTF || mFormat == NkFontFormat::OTF_CFF
     || mFormat == NkFontFormat::WOFF) {
        const int32 scale = (static_cast<int32>(mPPEM) << F26Dot6::SHIFT)
                          / mTTF.head.unitsPerEm;
        const int16 desc = mTTF.os2.sTypoDescender != 0
                         ? mTTF.os2.sTypoDescender
                         : mTTF.hhea.descender;
        return (static_cast<int32>(desc) * scale) >> F26Dot6::SHIFT;
    }
    if (mFormat == NkFontFormat::BDF || mFormat == NkFontFormat::PCF)
        return -mBDF.metrics.fontDescent;
    return 0;
}

int32 NkFontFace::GetLineHeight() const noexcept {
    return GetAscender() - GetDescender() + GetAscender() / 8;
}

int32 NkFontFace::GetUnderlinePos() const noexcept {
    if (mFormat == NkFontFormat::TTF || mFormat == NkFontFormat::OTF_CFF) {
        const int32 scale = (static_cast<int32>(mPPEM) << F26Dot6::SHIFT)
                          / mTTF.head.unitsPerEm;
        return (static_cast<int32>(mTTF.post.underlinePosition) * scale) >> F26Dot6::SHIFT;
    }
    return -(mPPEM / 8);
}

F26Dot6 NkFontFace::GetKerning(uint16 left, uint16 right) const noexcept {
    if (mFormat == NkFontFormat::TTF || mFormat == NkFontFormat::OTF_CFF)
        return NkKerning::GetKerning(mTTF, left, right, mPPEM);
    return F26Dot6::Zero();
}

// ─────────────────────────────────────────────────────────────────────────────
//  NkFontFace — rasterisation
// ─────────────────────────────────────────────────────────────────────────────

bool NkFontFace::GetGlyph(uint32 codepoint, NkGlyph& out) noexcept {
    if (!mIsValid) return false;

    uint16 glyphId = 0;
    if (mFormat == NkFontFormat::TTF || mFormat == NkFontFormat::OTF_CFF
     || mFormat == NkFontFormat::WOFF)
        glyphId = mTTF.GetGlyphId(codepoint);
    else if (mFormat == NkFontFormat::BDF || mFormat == NkFontFormat::PCF) {
        const NkBDFGlyph* bg = mBDF.FindGlyph(codepoint);
        if (!bg) return false;
        glyphId = static_cast<uint16>(bg - mBDF.glyphs);
    }
    return GetGlyphById(glyphId, out);
}

bool NkFontFace::GetGlyphById(uint16 glyphId, NkGlyph& out) noexcept {
    ::memset(&out, 0, sizeof(out));
    out.glyphId = glyphId;

    // Cherche dans le cache/atlas
    if (mAtlas) {
        const NkCachedGlyph* cached = mAtlas->FindGlyph(glyphId, mPPEM);
        if (cached) {
            out.metrics = cached->metrics;
            out.bitmap  = cached->bitmap;
            out.uv      = cached->uv;
            out.inAtlas = cached->inAtlas;
            return true;
        }
    }
    // Rasterize à la demande
    return RasterizeGlyph(glyphId, out);
}

bool NkFontFace::RasterizeGlyph(uint16 glyphId, NkGlyph& out) noexcept {
    switch (mFormat) {
        case NkFontFormat::TTF:
        case NkFontFormat::WOFF:    return RasterizeTTF(glyphId, out);
        case NkFontFormat::OTF_CFF: return RasterizeCFF(glyphId, out);
        case NkFontFormat::BDF:
        case NkFontFormat::PCF:     return RasterizeBDF(glyphId, out);
        default: return false;
    }
}

bool NkFontFace::RasterizeTTF(uint16 glyphId, NkGlyph& out) noexcept {
    NkScratchArena scratch(*mArena);

    // Décompose le glyphe en contours F26Dot6
    NkPoint26_6* points = nullptr; uint8* tags = nullptr;
    uint16* contourEnds = nullptr; uint16 nPts = 0, nCnt = 0;

    if (!NkTTFParser::DecomposeGlyph(
            mTTF, glyphId, mPPEM, *mArena,
            points, tags, contourEnds, nPts, nCnt))
        return false;

    // Métriques
    const NkTTFHMetric hm = mTTF.GetHMetric(glyphId);
    const int32 scale = (static_cast<int32>(mPPEM) << F26Dot6::SHIFT)
                       / mTTF.head.unitsPerEm;
    out.metrics.xAdvance = F26Dot6::FromRaw(
        (static_cast<int32>(hm.advanceWidth) * scale) >> F26Dot6::SHIFT);
    out.metrics.bearingX = (static_cast<int32>(hm.lsb) * scale) >> F26Dot6::SHIFT;

    if (nPts == 0) {
        // Glyphe vide (espace)
        out.isEmpty = true;
        out.metrics.width = out.metrics.height = 0;
        if (mAtlas)
            mAtlas->AddGlyph(glyphId, mPPEM, out.metrics, NkBitmap{}, *mArena);
        return true;
    }

    // Hinting
    if (mHintingEnabled && mTTF.isValid) {
        // Récupère les instructions du glyphe
        NkTTFGlyphData glyphData;
        if (NkTTFParser::ParseGlyph(mTTF, glyphId, *mArena, glyphData)
            && glyphData.numContours > 0) {
            const uint8*  instrData = glyphData.simple.instructions;
            const uint16  instrLen  = glyphData.simple.instructionLength;
            if (instrData && instrLen > 0) {
                NkHinter::HintGlyph(
                    mHintVM, points, tags, contourEnds,
                    nPts, nCnt, instrData, instrLen, *mArena);
            }
        }
    }

    // Construit NkOutline
    NkOutline outline = NkOutline::FromDecomposed(
        points, tags, contourEnds, nPts, nCnt);

    // Flip Y : TrueType Y↑ → écran Y↓
    const int32 ascenderPx = GetAscender();
    outline.FlipY(F26Dot6::FromInt(ascenderPx));

    // Rasterize
    NkBitmap bmp;
    if (!NkScanline::RasterizeOutline(outline, bmp, *mArena)) return false;

    out.metrics.width   = bmp.width;
    out.metrics.height  = bmp.height;
    out.metrics.bearingY= ascenderPx;
    out.bitmap          = bmp;
    out.isEmpty         = false;

    // Stocke dans l'atlas
    if (mAtlas)
        mAtlas->AddGlyph(glyphId, mPPEM, out.metrics, bmp, *mArena);

    return true;
}

bool NkFontFace::RasterizeCFF(uint16 glyphId, NkGlyph& out) noexcept {
    NkScratchArena scratch(*mArena);
    NkCFFGlyph cffGlyph;
    if (!NkCFFParser::ParseGlyph(mCFF, glyphId, *mArena, cffGlyph)) return false;

    const int32 scale = (static_cast<int32>(mPPEM) << F26Dot6::SHIFT) / 1000;
    out.metrics.xAdvance = F26Dot6::FromRaw(
        static_cast<int32>(cffGlyph.advanceWidth * scale));

    if (cffGlyph.numContours == 0) { out.isEmpty = true; return true; }

    // Convertit les points CFF (float, design units) → F26Dot6 (pixels)
    uint32 totalPts = 0;
    for (uint32 c = 0; c < cffGlyph.numContours; ++c)
        totalPts += cffGlyph.contours[c].numPoints;

    NkPoint26_6* points  = mArena->Alloc<NkPoint26_6>(totalPts);
    uint8*       tags    = mArena->Alloc<uint8>       (totalPts);
    uint16*      ends    = mArena->Alloc<uint16>      (cffGlyph.numContours);
    if (!points || !tags || !ends) return false;

    uint32 pi = 0;
    for (uint32 c = 0; c < cffGlyph.numContours; ++c) {
        const NkCFFContour& cont = cffGlyph.contours[c];
        for (uint32 p = 0; p < cont.numPoints; ++p, ++pi) {
            points[pi].x = F26Dot6::FromRaw(static_cast<int32>(cont.points[p].x * scale));
            points[pi].y = F26Dot6::FromRaw(static_cast<int32>(cont.points[p].y * scale));
            tags[pi]     = cont.points[p].onCurve ? NK_TAG_ON_CURVE : NK_TAG_CUBIC;
        }
        ends[c] = static_cast<uint16>(pi - 1);
    }

    NkOutline outline = NkOutline::FromDecomposed(
        points, tags, ends,
        static_cast<uint16>(totalPts),
        static_cast<uint16>(cffGlyph.numContours));
    outline.FlipY(F26Dot6::FromInt(GetAscender()));

    NkBitmap bmp;
    if (!NkScanline::RasterizeOutline(outline, bmp, *mArena)) return false;
    out.metrics.width  = bmp.width;
    out.metrics.height = bmp.height;
    out.bitmap         = bmp;
    if (mAtlas) mAtlas->AddGlyph(glyphId, mPPEM, out.metrics, bmp, *mArena);
    return true;
}

bool NkFontFace::RasterizeBDF(uint16 glyphId, NkGlyph& out) noexcept {
    if (glyphId >= mBDF.numGlyphs) return false;
    const NkBDFGlyph& bg = mBDF.glyphs[glyphId];
    out.metrics.xAdvance = F26Dot6::FromInt(bg.dwidthX);
    out.metrics.bearingX = bg.bbxOffX;
    out.metrics.bearingY = bg.bbxOffY + bg.bbxHeight;
    out.metrics.width    = bg.bbxWidth;
    out.metrics.height   = bg.bbxHeight;
    out.bitmap           = bg.bitmap;
    out.isEmpty          = !bg.bitmap.IsValid();
    if (mAtlas) mAtlas->AddGlyph(glyphId, mPPEM, out.metrics, bg.bitmap, *mArena);
    return true;
}

bool NkFontFace::ShapeText(
    const char* text, NkGlyphRun& out,
    const NkShaper::Options& opts) noexcept
{
    if (!text || !mIsValid) return false;
    usize len = 0; while (text[len]) ++len;
    return ShapeText(reinterpret_cast<const uint8*>(text), len, out, opts);
}

bool NkFontFace::ShapeText(
    const uint8* utf8, usize byteLen,
    NkGlyphRun& out, const NkShaper::Options& opts) noexcept
{
    if (!mIsValid || !utf8) return false;
    NkShaper::Options o = opts;
    o.ppem = mPPEM;
    if (mFormat == NkFontFormat::TTF || mFormat == NkFontFormat::OTF_CFF)
        return NkShaper::Shape(mTTF, utf8, byteLen, *mArena, out, o);
    return false;
}

F26Dot6 NkFontFace::MeasureText(const char* text) noexcept {
    NkGlyphRun run;
    if (!ShapeText(text, run)) return F26Dot6::Zero();
    return run.totalAdvance;
}

void NkFontFace::Preload(const uint32* codepoints, uint32 count) noexcept {
    for (uint32 i = 0; i < count; ++i) {
        NkGlyph g; GetGlyph(codepoints[i], g);
    }
}

void NkFontFace::PreloadASCII() noexcept {
    for (uint32 cp = 0x20; cp <= 0x7E; ++cp) {
        NkGlyph g; GetGlyph(cp, g);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  NkFontLibrary
// ─────────────────────────────────────────────────────────────────────────────

NkFontFormat NkFontLibrary::DetectFormat(const uint8* data, usize size) noexcept {
    if (size < 4) return NkFontFormat::Unknown;
    const uint32 sig = (static_cast<uint32>(data[0]) << 24)
                     | (static_cast<uint32>(data[1]) << 16)
                     | (static_cast<uint32>(data[2]) <<  8)
                     |  static_cast<uint32>(data[3]);
    if (sig == kTTF_SFVERSION_TRUE || sig == kTTF_SFVERSION_TRUE2
     || sig == kTTF_SFVERSION_TYPE1) return NkFontFormat::TTF;
    if (sig == kTTF_SFVERSION_OTF)   return NkFontFormat::OTF_CFF;
    if (sig == kWOFF_SIGNATURE)      return NkFontFormat::WOFF;
    if (sig == kWOFF2_SIGNATURE)     return NkFontFormat::WOFF2;
    if (size > 1 && data[0] == 0x80 && data[1] == 0x01) return NkFontFormat::Type1;
    if (size > 10 && ::strncmp(reinterpret_cast<const char*>(data), "STARTFONT", 9) == 0)
        return NkFontFormat::BDF;
    if (sig == 0x70636601u) return NkFontFormat::PCF;
    // PFA : commence par "%!"
    if (size > 2 && data[0] == '%' && data[1] == '!') return NkFontFormat::Type1;
    return NkFontFormat::Unknown;
}

bool NkFontLibrary::Init(
    usize permanentBytes, usize scratchBytes,
    int32 atlasWidth, int32 atlasHeight,
    uint32 atlasCacheCapacity) noexcept
{
    if (!mPool.Init(permanentBytes, scratchBytes, scratchBytes / 2)) return false;
    if (!mAtlas.Init(mPermanent, atlasWidth, atlasHeight, atlasCacheCapacity))
        return false;
    mFaceCount = 0;
    mIsValid   = true;
    return true;
}

void NkFontLibrary::Destroy() noexcept {
    mPool.Destroy();
    mFaceCount = 0;
    mIsValid   = false;
}

NkFontFace* NkFontLibrary::AllocFace() noexcept {
    if (mFaceCount >= MAX_FACES) return nullptr;
    NkFontFace* face = mPermanent.Alloc<NkFontFace>();
    if (!face) return nullptr;
    new (face) NkFontFace(); // placement new pour initialiser
    face->mArena = &mPermanent;
    face->mAtlas = &mAtlas;
    mFaces[mFaceCount++] = face;
    return face;
}

NkFontFace* NkFontLibrary::LoadFont(
    const uint8* data, usize size, uint16 ppem) noexcept
{
    const NkFontFormat fmt = DetectFormat(data, size);
    switch (fmt) {
        case NkFontFormat::TTF:
        case NkFontFormat::OTF_CFF: return LoadTTF(data, size, ppem);
        case NkFontFormat::WOFF:
        case NkFontFormat::WOFF2:   return LoadWOFF(data, size, ppem); // NkWOFFParser gère les deux
        case NkFontFormat::BDF:
        case NkFontFormat::PCF:     return LoadBDF(data, size);
        default:                    return nullptr;
    }
}

NkFontFace* NkFontLibrary::LoadTTF(
    const uint8* data, usize size, uint16 ppem) noexcept
{
    NkFontFace* face = AllocFace();
    if (!face) return nullptr;

    if (!NkTTFParser::Parse(data, size, mPermanent, face->mTTF)) return nullptr;

    face->mFormat  = face->mTTF.hasCFF ? NkFontFormat::OTF_CFF : NkFontFormat::TTF;
    face->mPPEM    = ppem;
    face->mIsValid = true;

    // Init hinter
    if (face->mTTF.fpgmData || face->mTTF.cvtData) {
        NkHinter::Init(face->mTTF, ppem, mPermanent, face->mHintVM);
        NkHinter::RunPrep(face->mTTF, face->mHintVM, mScratch);
    } else {
        face->mHintingEnabled = false;
    }

    // Init CFF si OTF
    if (face->mFormat == NkFontFormat::OTF_CFF) {
        // Cherche la table CFF dans les données brutes
        // (déjà localisée par NkTTFParser — le glyfOffset pointe dessus pour OTF)
        // Pour simplifier : on re-parse depuis rawData
        if (face->mTTF.rawData && face->mTTF.glyfOffset > 0) {
            NkCFFParser::Parse(
                face->mTTF.rawData + face->mTTF.glyfOffset,
                face->mTTF.rawSize  - face->mTTF.glyfOffset,
                mPermanent, face->mCFF);
        }
    }
    return face;
}

NkFontFace* NkFontLibrary::LoadBDF(
    const uint8* data, usize size) noexcept
{
    const NkFontFormat fmt = DetectFormat(data, size);
    NkFontFace* face = AllocFace();
    if (!face) return nullptr;

    bool ok = false;
    if (fmt == NkFontFormat::BDF)
        ok = NkBDFParser::Parse(data, size, mPermanent, face->mBDF);
    else
        ok = NkPCFParser::Parse(data, size, mPermanent, face->mBDF);

    if (!ok) return nullptr;
    face->mFormat  = fmt;
    face->mPPEM    = static_cast<uint16>(face->mBDF.metrics.fontAscent
                   + face->mBDF.metrics.fontDescent);
    face->mHintingEnabled = false;
    face->mIsValid = true;
    return face;
}

NkFontFace* NkFontLibrary::LoadWOFF(
    const uint8* data, usize size, uint16 ppem) noexcept
{
    NkFontFace* face = AllocFace();
    if (!face) return nullptr;
    if (!NkWOFFParser::Parse(data, size, mPermanent, face->mTTF)) return nullptr;
    face->mFormat  = NkFontFormat::WOFF;
    face->mPPEM    = ppem;
    face->mIsValid = true;
    return face;
}

} // namespace nkentseu

// ─────────────────────────────────────────────────────────────────────────────
//  NkFontLibrary::LoadTTC — charge une face depuis un fichier TTC
// ─────────────────────────────────────────────────────────────────────────────

namespace nkentseu {

NkFontFace* NkFontLibrary::LoadTTC(
    const uint8* data, usize size,
    int32 faceIndex, uint16 ppem) noexcept
{
    if(!mIsValid) return nullptr;
    NkFontFace* face = AllocFace(); if(!face) return nullptr;
    if(!NkTTCParser::ParseFace(data, size, faceIndex, mPermanent, face->mTTF))
        return nullptr;
    face->mFormat  = NkFontFormat::TTF;
    face->mPPEM    = ppem;
    face->mIsValid = true;
    if(face->mTTF.fpgmData||face->mTTF.cvtData){
        NkHinter::Init(face->mTTF, ppem, mPermanent, face->mHintVM);
        NkHinter::RunPrep(face->mTTF, face->mHintVM, mScratch);
    } else face->mHintingEnabled=false;
    return face;
}

/// Retourne le nombre de faces dans un TTC
int32 NkFontLibrary::GetTTCFaceCount(const uint8* data, usize size) noexcept {
    int32 count=0;
    NkTTCParser::GetFaceCount(data,size,count);
    return count;
}

} // namespace nkentseu
