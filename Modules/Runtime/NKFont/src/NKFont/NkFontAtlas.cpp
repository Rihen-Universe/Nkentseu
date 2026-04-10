// -----------------------------------------------------------------------------
// FICHIER: NKFont/NkFontAtlas.cpp
// DESCRIPTION: NkFontAtlas + NkFont — Pipeline de construction de l'atlas.
//
// CORRECTIONS V2.1 :
//  #A1 CRITIQUE — advanceX et les UV étaient calculés sans corriger l'oversample.
//  #A2 — Les UV u0/u1 pointaient vers un glyphe 2× trop large dans l'atlas.
//  #SDF — Mode SDF optionnel.
//  #OUTLINE — Exposition des contours vectoriels via GetGlyphOutline.
// -----------------------------------------------------------------------------

#include "NKFont/NkFont.h"
#include "NKFont/Core/NkFontParser.h"
#include "NKFont/Core/NkUtils.h"
#include "NKLogger/NkLog.h"
#include "Core/NkUtils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fstream>

namespace nkentseu {

    using namespace math;

    // ============================================================
    // Shelf Rect Packer (inchangé)
    // ============================================================

    namespace {

        struct NkPackRect {
            nkft_int32  w = 0, h = 0, x = 0, y = 0;
            bool        packed = false;
            nkft_uint32 userIdx = 0;
        };

        struct NkShelfPacker {
            nkft_int32 atlasW = 0, atlasH = 0;
            nkft_int32 curX = 0, curY = 0;
            nkft_int32 shelfH = 0, padding = 1;

            void Init(nkft_int32 w, nkft_int32 h, nkft_int32 pad = 1) {
                atlasW = w;
                atlasH = h;
                curX = pad;
                curY = pad;
                shelfH = 0;
                padding = pad;
            }

            bool Pack(NkPackRect& r) {
                nkft_int32 rw = r.w + padding;
                nkft_int32 rh = r.h + padding;
                if (curX + rw > atlasW - padding) {
                    curX = padding;
                    curY += shelfH + padding;
                    shelfH = 0;
                }
                if (curY + rh > atlasH - padding) return false;
                r.x = curX;
                r.y = curY;
                r.packed = true;
                curX += rw;
                if (rh > shelfH) shelfH = rh;
                return true;
            }
        };

        static void CalcMultiplyLookup(nkft_uint8 out[256], nkft_float32 f) {
            for (nkft_int32 i = 0; i < 256; ++i) {
                nkft_int32 v = (nkft_int32)(i * f + 0.5f);
                out[i] = (nkft_uint8)((v > 255) ? 255 : (v < 0) ? 0 : v);
            }
        }

        static void ApplyMultiplyRect(const nkft_uint8 t[256],
                                      nkft_uint8* px,
                                      nkft_int32 x, nkft_int32 y,
                                      nkft_int32 w, nkft_int32 h,
                                      nkft_int32 stride) {
            nkft_uint8* base = px + y * stride + x;
            for (nkft_int32 r = 0; r < h; ++r) {
                nkft_uint8* p = base + r * stride;
                for (nkft_int32 c = 0; c < w; ++c, ++p) {
                    *p = t[*p];
                }
            }
        }

        // ── Fonctions d'aplatissement pour les courbes (utilisées par GetGlyphOutline) ──

        static void FlattenQuadratic(const NkVec2f& p0, const NkVec2f& p1, const NkVec2f& p2,
                                     float flatness_sq, int depth,
                                     NkVector<NkVec2f>& outPoints) {
            NkVec2f mx = { (p0.x + 2*p1.x + p2.x) / 4, (p0.y + 2*p1.y + p2.y) / 4 };
            NkVec2f mid = { (p0.x + p2.x) / 2, (p0.y + p2.y) / 2 };
            NkVec2f d = { mid.x - mx.x, mid.y - mx.y };
            if (depth > 16 || (d.x*d.x + d.y*d.y) <= flatness_sq) {
                outPoints.PushBack(p2);
                return;
            }
            NkVec2f m01 = { (p0.x + p1.x) / 2, (p0.y + p1.y) / 2 };
            NkVec2f m12 = { (p1.x + p2.x) / 2, (p1.y + p2.y) / 2 };
            NkVec2f mm  = { (m01.x + m12.x) / 2, (m01.y + m12.y) / 2 };
            FlattenQuadratic(p0, m01, mm, flatness_sq, depth+1, outPoints);
            FlattenQuadratic(mm, m12, p2, flatness_sq, depth+1, outPoints);
        }

        static void FlattenCubic(const NkVec2f& p0, const NkVec2f& p1,
                                 const NkVec2f& p2, const NkVec2f& p3,
                                 float flatness_sq, int depth,
                                 NkVector<NkVec2f>& outPoints) {
            NkVec2f d1 = { p1.x - p0.x, p1.y - p0.y };
            NkVec2f d2 = { p2.x - p1.x, p2.y - p1.y };
            NkVec2f d3 = { p3.x - p2.x, p3.y - p2.y };
            NkVec2f d  = { p3.x - p0.x, p3.y - p0.y };
            float ll = sqrtf(d1.x*d1.x+d1.y*d1.y) + sqrtf(d2.x*d2.x+d2.y*d2.y) + sqrtf(d3.x*d3.x+d3.y*d3.y);
            float sl = sqrtf(d.x*d.x+d.y*d.y);
            float flat = ll*ll - sl*sl;
            if (depth > 16 || flat <= flatness_sq) {
                outPoints.PushBack(p3);
                return;
            }
            NkVec2f m01 = { (p0.x + p1.x)/2, (p0.y + p1.y)/2 };
            NkVec2f m12 = { (p1.x + p2.x)/2, (p1.y + p2.y)/2 };
            NkVec2f m23 = { (p2.x + p3.x)/2, (p2.y + p3.y)/2 };
            NkVec2f m012 = { (m01.x + m12.x)/2, (m01.y + m12.y)/2 };
            NkVec2f m123 = { (m12.x + m23.x)/2, (m12.y + m23.y)/2 };
            NkVec2f m = { (m012.x + m123.x)/2, (m012.y + m123.y)/2 };
            FlattenCubic(p0, m01, m012, m, flatness_sq, depth+1, outPoints);
            FlattenCubic(m, m123, m23, p3, flatness_sq, depth+1, outPoints);
        }

    } // anonymous namespace

    // ============================================================
    // Plages Unicode (inchangées)
    // ============================================================

    static const nkft_uint32 sRangesDefault[]     = {0x0020, 0x00FF, 0};
    static const nkft_uint32 sRangesLatinExtA[]   = {0x0100, 0x024F, 0};
    static const nkft_uint32 sRangesCyrillic[]    = {0x0020, 0x00FF, 0x0400, 0x052F, 0x2DE0, 0x2DFF, 0xA640, 0xA69F, 0};
    static const nkft_uint32 sRangesGreek[]       = {0x0020, 0x00FF, 0x0370, 0x03FF, 0};
    static const nkft_uint32 sRangesChineseFull[] = {0x0020, 0x00FF, 0x2000, 0x206F, 0x3000, 0x30FF, 0x31F0, 0x31FF, 0xFF00, 0xFFEF, 0x4E00, 0x9FAF, 0};

    const nkft_uint32* NkFontAtlas::GetGlyphRangesDefault()      { return sRangesDefault; }
    const nkft_uint32* NkFontAtlas::GetGlyphRangesLatinExtA()    { return sRangesLatinExtA; }
    const nkft_uint32* NkFontAtlas::GetGlyphRangesCyrillic()     { return sRangesCyrillic; }
    const nkft_uint32* NkFontAtlas::GetGlyphRangesGreek()        { return sRangesGreek; }
    const nkft_uint32* NkFontAtlas::GetGlyphRangesChineseFull()  { return sRangesChineseFull; }

    // ============================================================
    // NkFontAtlas (constructeur, destructeur, Clear, etc. inchangés)
    // ============================================================

    NkFontAtlas::NkFontAtlas() {
        memset(fonts,        0, sizeof(fonts));
        memset(fontDataBufs, 0, sizeof(fontDataBufs));
        memset(customRects,  0, sizeof(customRects));
        memset(configs,      0, sizeof(configs));
    }

    NkFontAtlas::~NkFontAtlas() {
        Clear();
    }

    void NkFontAtlas::Clear() {
        ClearTexData();
        for (nkft_uint32 i = 0; i < fontCount; ++i) {
            if (fonts[i]) {
                delete fonts[i];
                fonts[i] = nullptr;
            }
        }
        fontCount = 0;
        for (nkft_uint32 i = 0; i < NK_FONT_ATLAS_MAX_FONTS; ++i) {
            if (fontDataBufs[i]) {
                free(fontDataBufs[i]);
                fontDataBufs[i] = nullptr;
            }
        }
        customRectCount = 0;
        texReady = false;
    }

    void NkFontAtlas::ClearTexData() {
        if (texPixels) {
            free(texPixels);
            texPixels = nullptr;
        }
        if (mTexPixelsRGBA) {
            free(mTexPixelsRGBA);
            mTexPixelsRGBA = nullptr;
        }
        texWidth = 0;
        texHeight = 0;
        texReady = false;
    }

    NkFont* NkFontAtlas::AddFontFromFile(const char* path,
                                         nkft_float32 sp,
                                         const NkFontConfig* cfg) {
        {
            std::ifstream f(path);
            if (!f.good()) {
                logger.Info("[NkFontAtlas] Fichier introuvable:{0}\n", path);
                return nullptr;
            }
        }
        FILE* f = fopen(path, "rb");
        if (!f) return nullptr;
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (sz <= 0) {
            fclose(f);
            return nullptr;
        }
        nkft_uint8* d = (nkft_uint8*)malloc((nkft_size)sz);
        if (!d) {
            fclose(f);
            return nullptr;
        }
        if ((long)fread(d, 1, (nkft_size)sz, f) != sz) {
            fclose(f);
            free(d);
            return nullptr;
        }
        fclose(f);
        logger.Info("[NkFontAtlas] TTF lu:{0}({1}o)\n", path, sz);
        return AddFontFromMemoryOwned(d, (nkft_size)sz, sp, cfg);
    }

    NkFont* NkFontAtlas::AddFontFromMemory(const nkft_uint8* d,
                                           nkft_size ds,
                                           nkft_float32 sp,
                                           const NkFontConfig* cfg) {
        nkft_uint8* c = (nkft_uint8*)malloc(ds);
        if (!c) return nullptr;
        memcpy(c, d, ds);
        return AddFontFromMemoryOwned(c, ds, sp, cfg);
    }

    NkFont* NkFontAtlas::AddFontFromMemoryOwned(nkft_uint8* d,
                                                nkft_size ds,
                                                nkft_float32 sp,
                                                const NkFontConfig* inCfg) {
        if (fontCount >= NK_FONT_ATLAS_MAX_FONTS) {
            logger.Info("[NkFontAtlas] Limite atteinte\n");
            return nullptr;
        }
        NkFontConfig& cfg = configs[fontCount];
        if (inCfg) cfg = *inCfg;
        cfg.fontData = d;
        cfg.fontDataSize = ds;
        cfg.fontDataOwned = true;
        cfg.sizePixels = sp;
        if (!cfg.glyphRanges) cfg.glyphRanges = GetGlyphRangesDefault();
        if (cfg.oversampleH < 1) cfg.oversampleH = 2;
        if (cfg.oversampleV < 1) cfg.oversampleV = 1;
        fontDataBufs[fontCount] = d;

        NkFont* font = nullptr;
        if (cfg.mergeMode && fontCount > 0) {
            for (nkft_int32 fi = (nkft_int32)fontCount - 1; fi >= 0; --fi) {
                if (!configs[fi].mergeMode && fonts[fi]) {
                    font = fonts[fi];
                    break;
                }
            }
            if (!font) font = fonts[fontCount - 1];
        } else {
            font = new NkFont();
            font->containerAtlas = this;
            font->config = cfg;
        }
        fonts[fontCount] = font;
        ++fontCount;
        texReady = false;
        logger.Info("[NkFontAtlas] AddFont#{0}:{1}px merge={2}\n",
                    fontCount - 1, sp, cfg.mergeMode ? 1 : 0);
        return font;
    }

    nkft_int32 NkFontAtlas::AddCustomRect(nkft_uint16 w, nkft_uint16 h) {
        if (customRectCount >= NK_FONT_ATLAS_MAX_CUSTOM_RECTS) return -1;
        auto& r = customRects[customRectCount];
        r.width = w;
        r.height = h;
        r.isPacked = false;
        texReady = false;
        return (nkft_int32)customRectCount++;
    }

    const NkFontAtlasCustomRect* NkFontAtlas::GetCustomRect(nkft_int32 idx) const {
        if (idx < 0 || (nkft_uint32)idx >= customRectCount) return nullptr;
        return &customRects[idx];
    }

    // ============================================================
    // Build() — avec stockage du NkFontFaceInfo dans NkFont
    // ============================================================

    bool NkFontAtlas::Build() {
        if (fontCount == 0) {
            logger.Info("[NkFontAtlas] Build():aucune fonte\n");
            return false;
        }
        ClearTexData();

        static nkfont::NkFontFaceInfo faceInfos[NK_FONT_ATLAS_MAX_FONTS];
        static nkft_float32 scales[NK_FONT_ATLAS_MAX_FONTS];

        // Libère les anciens buffers WOFF si nécessaire
        for (nkft_uint32 i = 0; i < NK_FONT_ATLAS_MAX_FONTS; ++i) {
            nkfont::NkFreeFontFace(&faceInfos[i]);
        }

        nkft_uint32 fontForConfig[NK_FONT_ATLAS_MAX_FONTS];
        {
            nkft_uint32 last = 0;
            for (nkft_uint32 i = 0; i < fontCount; ++i) {
                if (!configs[i].mergeMode) last = i;
                fontForConfig[i] = last;
            }
        }

        for (nkft_uint32 i = 0; i < fontCount; ++i) {
            const NkFontConfig& cfg = configs[i];
            if (!cfg.fontData || !cfg.fontDataSize) {
                logger.Info("[NkFontAtlas] Config{0} sans donnees\n", i);
                return false;
            }
            if (!nkfont::NkInitFontFace(&faceInfos[i], cfg.fontData, cfg.fontDataSize, cfg.fontIndex)) {
                logger.Info("[NkFontAtlas] InitFontFace echec config{0}\n", i);
                return false;
            }
            scales[i] = nkfont::NkScaleForPixelHeight(&faceInfos[i], cfg.sizePixels);
            if (!cfg.mergeMode) {
                NkFont* font = fonts[i];
                if (!font) continue;
                nkft_int32 asc, desc, lg;
                nkfont::NkGetFontVMetrics(&faceInfos[i], &asc, &desc, &lg);
                font->scale = scales[i];
                font->fontSize = cfg.sizePixels;
                font->ascent = (nkft_float32)asc * scales[i];
                font->descent = (nkft_float32)desc * scales[i];
                font->lineAdvance = (nkft_float32)(asc - desc + lg) * scales[i];
                // Stockage du faceInfo pour l'extraction des contours
                font->m_FaceInfo = &faceInfos[i];
                logger.Info("[NkFontAtlas] Font{0}:scale={1:.4f} asc={2:.1f} desc={3:.1f} la={4:.1f}\n",
                            i, font->scale, font->ascent, font->descent, font->lineAdvance);
            }
        }

        static NkPackRect      packRects   [NK_FONT_ATLAS_MAX_FONTS * 8192];
        static nkft_uint32     packCfgIdx  [NK_FONT_ATLAS_MAX_FONTS * 8192];
        static NkFontCodepoint packCp      [NK_FONT_ATLAS_MAX_FONTS * 8192];
        static NkGlyphId       packGlyphId [NK_FONT_ATLAS_MAX_FONTS * 8192];
        static bool            packIsSpace [NK_FONT_ATLAS_MAX_FONTS * 8192];
        nkft_uint32 totalRects = 0;
        static constexpr nkft_uint32 MAX_RECTS = NK_FONT_ATLAS_MAX_FONTS * 8192 - 1;

        for (nkft_uint32 ci = 0; ci < fontCount; ++ci) {
            const NkFontConfig& cfg = configs[ci];
            const nkfont::NkFontFaceInfo& fi = faceInfos[ci];
            nkft_float32 sc = scales[ci];
            const nkft_uint32* ranges = cfg.glyphRanges ? cfg.glyphRanges : GetGlyphRangesDefault();

            for (nkft_uint32 ri = 0; ranges[ri] && ranges[ri + 1]; ri += 2) {
                for (nkft_uint32 cp = ranges[ri]; cp <= ranges[ri + 1] && totalRects < MAX_RECTS; ++cp) {
                    NkGlyphId gid = nkfont::NkFindGlyphIndex(&fi, cp);
                    if (gid == 0 && cp != cfg.glyphFallback && cp != 0x0020u) continue;
                    nkft_int32 x0 = 0, y0 = 0, x1 = 0, y1 = 0;
                    nkfont::NkGetGlyphBitmapBox(&fi, gid,
                                              sc * cfg.oversampleH,
                                              sc * cfg.oversampleV,
                                              0, 0,
                                              &x0, &y0, &x1, &y1);
                    nkft_int32 gw = x1 - x0, gh = y1 - y0;
                    bool isSpace = (gw <= 0 || gh <= 0);
                    NkPackRect& r = packRects[totalRects];
                    r.w = isSpace ? 1 : (gw + 1);
                    r.h = isSpace ? 1 : (gh + 1);
                    packCfgIdx[totalRects] = ci;
                    packCp[totalRects] = cp;
                    packGlyphId[totalRects] = gid;
                    packIsSpace[totalRects] = isSpace;
                    ++totalRects;
                }
            }
            if (totalRects < MAX_RECTS) {
                NkGlyphId gid = nkfont::NkFindGlyphIndex(&fi, cfg.glyphFallback);
                if (gid) {
                    nkft_int32 x0 = 0, y0 = 0, x1 = 0, y1 = 0;
                    nkfont::NkGetGlyphBitmapBox(&fi, gid,
                                              sc * cfg.oversampleH,
                                              sc * cfg.oversampleV,
                                              0, 0,
                                              &x0, &y0, &x1, &y1);
                    nkft_int32 gw = x1 - x0, gh = y1 - y0;
                    bool isSpace = (gw <= 0 || gh <= 0);
                    NkPackRect& r = packRects[totalRects];
                    r.w = isSpace ? 1 : gw + 1;
                    r.h = isSpace ? 1 : gh + cfg.oversampleV;
                    packCfgIdx[totalRects] = ci;
                    packCp[totalRects] = cfg.glyphFallback;
                    packGlyphId[totalRects] = gid;
                    packIsSpace[totalRects] = isSpace;
                    ++totalRects;
                }
            }
        }

        nkft_uint32 crs = totalRects;
        for (nkft_uint32 i = 0; i < customRectCount && totalRects < MAX_RECTS; ++i) {
            NkPackRect& r = packRects[totalRects];
            r.w = customRects[i].width;
            r.h = customRects[i].height;
            packCfgIdx[totalRects] = 0;
            packCp[totalRects] = 0;
            packGlyphId[totalRects] = 0;
            packIsSpace[totalRects] = false;
            ++totalRects;
        }
        logger.Info("[NkFontAtlas] Build():{0} rects({1} custom)\n", totalRects, customRectCount);
        if (totalRects == 0) {
            logger.Info("[NkFontAtlas] Build():aucun glyphe\n");
            return false;
        }

        nkft_int32 atlasW = texDesiredWidth;
        if (atlasW == 0) {
            atlasW = 512;
            nkft_int32 needed = 0;
            for (nkft_uint32 i = 0; i < totalRects; ++i) {
                needed += (packRects[i].w + texGlyphPadding) * (packRects[i].h + texGlyphPadding);
            }
            while (atlasW * atlasW < needed * 2 && atlasW < 4096) {
                atlasW *= 2;
            }
        }
        nkft_int32 atlasH = atlasW;

        bool packOk = false;
        for (nkft_int32 at = 0; at < 8 && !packOk; ++at) {
            NkShelfPacker p;
            p.Init(atlasW, atlasH, texGlyphPadding);
            packOk = true;
            for (nkft_uint32 i = 0; i < totalRects; ++i) {
                packRects[i].packed = false;
                if (!p.Pack(packRects[i])) {
                    packOk = false;
                    break;
                }
            }
            if (!packOk) {
                if (atlasW == atlasH) atlasW *= 2;
                else atlasH = atlasW;
            }
        }
        if (!packOk) {
            logger.Info("[NkFontAtlas] Build():packing echec\n");
            return false;
        }
        logger.Info("[NkFontAtlas] Build():atlas{0}x{1}\n", atlasW, atlasH);

        texWidth = atlasW;
        texHeight = atlasH;
        texPixels = (nkft_uint8*)malloc((nkft_size)(atlasW * atlasH));
        if (!texPixels) return false;
        memset(texPixels, 0, (nkft_size)(atlasW * atlasH));

        nkft_uint8* sdfTemp = sdfMode
            ? (nkft_uint8*)malloc((nkft_size)(atlasW * atlasH))
            : nullptr;
        if (sdfMode && sdfTemp) memset(sdfTemp, 0, (nkft_size)(atlasW * atlasH));

        nkft_uint32 glyphsRast = 0;
        for (nkft_uint32 ri = 0; ri < crs; ++ri) {
            const NkPackRect& r = packRects[ri];
            if (!r.packed) continue;
            nkft_uint32 ci = packCfgIdx[ri];
            nkft_uint32 fi = fontForConfig[ci];
            const NkFontConfig& cfg = configs[ci];
            const nkfont::NkFontFaceInfo& face = faceInfos[ci];
            nkft_float32 sc = scales[ci];
            NkGlyphId gid = packGlyphId[ri];
            NkFontCodepoint cp = packCp[ri];
            bool isSpace = packIsSpace[ri];

            nkft_int32 bx0 = 0, by0 = 0, bx1 = 0, by1 = 0;
            nkfont::NkGetGlyphBitmapBox(&face, gid,
                                      sc * cfg.oversampleH,
                                      sc * cfg.oversampleV,
                                      0, 0,
                                      &bx0, &by0, &bx1, &by1);
            nkft_int32 bw = bx1 - bx0, bh = by1 - by0;

            if (!isSpace && bw > 0 && bh > 0) {
                nkft_uint8* dst = texPixels + r.y * atlasW + r.x;
                nkfont::NkMakeGlyphBitmap(&face, dst, bw, bh, atlasW,
                                        sc * cfg.oversampleH,
                                        sc * cfg.oversampleV,
                                        0, 0,
                                        gid);

                if (sdfMode && sdfTemp) {
                    nkft_uint8* sdfDst = sdfTemp + r.y * atlasW + r.x;
                    nkfont::NkMakeSDFFromBitmap(dst, bw, bh, sdfDst, sdfSpread);
                    for (nkft_int32 row = 0; row < bh; ++row) {
                        memcpy(dst + row * atlasW, sdfDst + row * atlasW, (nkft_size)bw);
                    }
                }

                if (cfg.rasterizerMultiply != 1.f) {
                    nkft_uint8 mt[256];
                    CalcMultiplyLookup(mt, cfg.rasterizerMultiply);
                    ApplyMultiplyRect(mt, texPixels, r.x, r.y, bw, bh, atlasW);
                }
                ++glyphsRast;
            }

            nkft_int32 advW = 0;
            nkfont::NkGetGlyphHMetrics(&face, gid, &advW, nullptr);
            nkft_float32 advX = (nkft_float32)advW * sc;
            if (advX < cfg.glyphMinAdvanceX) advX = cfg.glyphMinAdvanceX;
            if (advX > cfg.glyphMaxAdvanceX) advX = cfg.glyphMaxAdvanceX;
            if (cfg.pixelSnapH) advX = (nkft_float32)(nkft_int32)(advX + 0.5f);
            advX += cfg.glyphExtraSpacing;

            NkFont* font = fonts[fi];
            if (!font) continue;

            NkFontGlyph g;
            g.codepoint = cp;
            g.advanceX = advX;

            nkft_float32 osH = (nkft_float32)cfg.oversampleH;
            nkft_float32 osV = (nkft_float32)cfg.oversampleV;
            g.x0 = (nkft_float32)bx0 / osH + cfg.glyphOffset.x;
            g.y0 = (nkft_float32)by0 / osV + cfg.glyphOffset.y;
            g.x1 = (nkft_float32)bx1 / osH + cfg.glyphOffset.x;
            g.y1 = (nkft_float32)by1 / osV + cfg.glyphOffset.y;

            g.u0 = (nkft_float32)r.x / (nkft_float32)atlasW;
            g.v0 = (nkft_float32)r.y / (nkft_float32)atlasH;
            g.u1 = (nkft_float32)(r.x + (isSpace ? 0 : bw)) / (nkft_float32)atlasW;
            g.v1 = (nkft_float32)(r.y + (isSpace ? 0 : bh)) / (nkft_float32)atlasH;
            g.visible = (!isSpace && bw > 0 && bh > 0);

            if (!isSpace && cfg.oversampleH > 1) {
                nkft_float32 hpx = 0.5f / (nkft_float32)atlasW;
                g.u0 += hpx;
                g.u1 -= hpx;
            }
            if (!isSpace && cfg.oversampleV > 1) {
                nkft_float32 hpy = 0.5f / (nkft_float32)atlasH;
                g.v0 += hpy;
                g.v1 -= hpy;
            }

            font->AddGlyph(g);
        }
        logger.Info("[NkFontAtlas] Build():{0} glyphes rasterises\n", glyphsRast);

        if (sdfTemp) free(sdfTemp);

        for (nkft_uint32 i = 0; i < customRectCount; ++i) {
            nkft_uint32 ri = crs + i;
            if (ri >= totalRects || !packRects[ri].packed) continue;
            const NkPackRect& r = packRects[ri];
            customRects[i].x = (nkft_uint16)r.x;
            customRects[i].y = (nkft_uint16)r.y;
            customRects[i].u0 = (nkft_float32)r.x / (nkft_float32)atlasW;
            customRects[i].v0 = (nkft_float32)r.y / (nkft_float32)atlasH;
            customRects[i].u1 = (nkft_float32)(r.x + r.w) / (nkft_float32)atlasW;
            customRects[i].v1 = (nkft_float32)(r.y + r.h) / (nkft_float32)atlasH;
            customRects[i].isPacked = true;
        }

        for (nkft_uint32 fi = 0; fi < fontCount; ++fi) {
            NkFont* font = fonts[fi];
            if (!font) continue;
            if (font->glyphCount == 0) {
                logger.Info("[NkFontAtlas] Font{0}:0 glyphes\n", fi);
                continue;
            }
            NkFontCodepoint fb = configs[fi].glyphFallback;
            font->fallbackGlyph = font->FindGlyphNoFallback(fb);
            if (!font->fallbackGlyph && fb != 0x003Fu) {
                font->fallbackGlyph = font->FindGlyphNoFallback(0x003Fu);
            }
            if (font->fallbackGlyph) {
                font->fallbackAdvanceX = font->fallbackGlyph->advanceX;
            }
            font->BuildLookupTable();
            logger.Info("[NkFontAtlas] Font{0}:{1} glyphes fallback='{2}'\n",
                        fi, font->glyphCount, font->fallbackGlyph ? "ok" : "absent");
        }

        texReady = true;
        logger.Info("[NkFontAtlas] Build() termine.\n");
        return true;
    }

    // ============================================================
    // GetTexData (inchangé)
    // ============================================================

    void NkFontAtlas::GetTexDataAsAlpha8(nkft_uint8** op,
                                         nkft_int32* ow,
                                         nkft_int32* oh,
                                         nkft_int32* ob) {
        if (!texReady || !texPixels) {
            if (op) *op = nullptr;
            if (ow) *ow = 0;
            if (oh) *oh = 0;
            if (ob) *ob = 0;
            return;
        }
        if (op) *op = texPixels;
        if (ow) *ow = texWidth;
        if (oh) *oh = texHeight;
        if (ob) *ob = 1;
    }

    void NkFontAtlas::GetTexDataAsRGBA32(nkft_uint8** op,
                                         nkft_int32* ow,
                                         nkft_int32* oh,
                                         nkft_int32* ob) {
        if (!texPixels) {
            if (op) *op = nullptr;
            return;
        }
        if (!mTexPixelsRGBA) {
            mTexPixelsRGBA = (nkft_uint8*)malloc((nkft_size)(texWidth * texHeight * 4));
            if (!mTexPixelsRGBA) {
                if (op) *op = nullptr;
                return;
            }
            const nkft_uint8* src = texPixels;
            nkft_uint8* dst = mTexPixelsRGBA;
            for (nkft_int32 i = 0; i < texWidth * texHeight; ++i) {
                nkft_uint8 a = *src++;
                *dst++ = 255;
                *dst++ = 255;
                *dst++ = 255;
                *dst++ = a;
            }
        }
        if (op) *op = mTexPixelsRGBA;
        if (ow) *ow = texWidth;
        if (oh) *oh = texHeight;
        if (ob) *ob = 4;
    }

    // ============================================================
    // NkFont (constructeur, méthodes existantes inchangées)
    // ============================================================

    NkFont::NkFont() {
        glyphCount = 0;
        sortedCount = 0;
        fallbackGlyph = nullptr;
        fallbackAdvanceX = 0;
        memset(indexLookup, 0xFF, sizeof(indexLookup));
        memset(glyphSortedCodepoints, 0, sizeof(glyphSortedCodepoints));
        memset(glyphSortedIndices, 0, sizeof(glyphSortedIndices));
    }

    void NkFont::AddGlyph(const NkFontGlyph& g) {
        if (glyphCount >= NK_FONT_MAX_GLYPHS) return;
        glyphs[glyphCount] = g;
        AddGlyphSorted(g);
        ++glyphCount;
    }

    void NkFont::AddGlyphSorted(const NkFontGlyph& g) {
        if (sortedCount >= NK_FONT_MAX_GLYPHS) return;
        NkFontCodepoint cp = g.codepoint;
        nkft_uint16 idx = (nkft_uint16)glyphCount;
        nkft_uint32 pos = sortedCount;
        for (nkft_uint32 i = 0; i < sortedCount; ++i) {
            if (glyphSortedCodepoints[i] > cp) {
                pos = i;
                break;
            }
        }
        for (nkft_uint32 i = sortedCount; i > pos; --i) {
            glyphSortedCodepoints[i] = glyphSortedCodepoints[i - 1];
            glyphSortedIndices[i]    = glyphSortedIndices[i - 1];
        }
        glyphSortedCodepoints[pos] = cp;
        glyphSortedIndices[pos]    = idx;
        ++sortedCount;
    }

    void NkFont::BuildLookupTable() {
        memset(indexLookup, 0xFF, sizeof(indexLookup));
        for (nkft_uint32 i = 0; i < glyphCount; ++i) {
            NkFontCodepoint cp = glyphs[i].codepoint;
            if (cp < NK_FONT_INDEX_SIZE) {
                indexLookup[cp] = (nkft_uint16)i;
            }
        }
    }

    nkft_int32 NkFont::FindGlyphIndex(NkFontCodepoint c) const {
        if (c < NK_FONT_INDEX_SIZE) {
            nkft_uint16 idx = indexLookup[c];
            return (idx != 0xFFFF) ? (nkft_int32)idx : -1;
        }
        nkft_uint32 lo = 0, hi = sortedCount;
        while (lo < hi) {
            nkft_uint32 mid = (lo + hi) >> 1;
            if (glyphSortedCodepoints[mid] < c) {
                lo = mid + 1;
            } else if (glyphSortedCodepoints[mid] > c) {
                hi = mid;
            } else {
                return (nkft_int32)glyphSortedIndices[mid];
            }
        }
        return -1;
    }

    const NkFontGlyph* NkFont::FindGlyph(NkFontCodepoint c) const {
        nkft_int32 idx = FindGlyphIndex(c);
        if (idx >= 0) return &glyphs[idx];
        if (fallbackGlyph) return fallbackGlyph;
        idx = FindGlyphIndex(0x003F);
        return (idx >= 0) ? &glyphs[idx] : nullptr;
    }

    const NkFontGlyph* NkFont::FindGlyphNoFallback(NkFontCodepoint c) const {
        nkft_int32 i = FindGlyphIndex(c);
        return (i >= 0) ? &glyphs[i] : nullptr;
    }

    nkft_float32 NkFont::CalcTextSizeX(const char* text,
                                       const char* textEnd) const {
        nkft_float32 w = 0;
        const char* p = text;
        while (p < textEnd || (!textEnd && *p)) {
            NkFontCodepoint cp = DecodeUTF8(&p, textEnd);
            if (!cp) break;
            const NkFontGlyph* g = FindGlyph(cp);
            w += g ? g->advanceX : fallbackAdvanceX;
        }
        return w;
    }

    NkFontCodepoint NkFont::DecodeUTF8(const char** t, const char* e) {
        return NkFontDecodeUTF8(t, e);
    }

    NkFontCodepoint NkFontDecodeUTF8(const char** str, const char* end) {
        const nkft_uint8* s = (const nkft_uint8*)*str;
        const nkft_uint8* e = (const nkft_uint8*)end;
        if (!s || (e && s >= e) || !*s) return 0;

        NkFontCodepoint cp;
        nkft_uint8 b0 = *s;

        if (b0 < 0x80) {
            cp = b0;
            *str = (const char*)(s + 1);
            return cp;
        } else if ((b0 & 0xE0) == 0xC0) {
            if (e && s + 2 > e) {
                ++*str;
                return NKFONT_CODEPOINT_REPLACEMENT;
            }
            cp = ((nkft_uint32)(b0 & 0x1F) << 6) | (s[1] & 0x3F);
            *str = (const char*)(s + 2);
        } else if ((b0 & 0xF0) == 0xE0) {
            if (e && s + 3 > e) {
                ++*str;
                return NKFONT_CODEPOINT_REPLACEMENT;
            }
            cp = ((nkft_uint32)(b0 & 0x0F) << 12) |
                 ((nkft_uint32)(s[1] & 0x3F) << 6) |
                 (s[2] & 0x3F);
            *str = (const char*)(s + 3);
        } else if ((b0 & 0xF8) == 0xF0) {
            if (e && s + 4 > e) {
                ++*str;
                return NKFONT_CODEPOINT_REPLACEMENT;
            }
            cp = ((nkft_uint32)(b0 & 0x07) << 18) |
                 ((nkft_uint32)(s[1] & 0x3F) << 12) |
                 ((nkft_uint32)(s[2] & 0x3F) << 6) |
                 (s[3] & 0x3F);
            *str = (const char*)(s + 4);
        } else {
            ++*str;
            return NKFONT_CODEPOINT_REPLACEMENT;
        }
        return cp;
    }

    nkft_int32 NkFontEncodeUTF8(NkFontCodepoint c, char* out, nkft_int32 sz) {
        if (c < 0x80 && sz >= 1) {
            out[0] = (char)c;
            return 1;
        } else if (c < 0x800 && sz >= 2) {
            out[0] = (char)(0xC0 | (c >> 6));
            out[1] = (char)(0x80 | (c & 0x3F));
            return 2;
        } else if (c < 0x10000 && sz >= 3) {
            out[0] = (char)(0xE0 | (c >> 12));
            out[1] = (char)(0x80 | ((c >> 6) & 0x3F));
            out[2] = (char)(0x80 | (c & 0x3F));
            return 3;
        } else if (c < 0x110000 && sz >= 4) {
            out[0] = (char)(0xF0 | (c >> 18));
            out[1] = (char)(0x80 | ((c >> 12) & 0x3F));
            out[2] = (char)(0x80 | ((c >> 6) & 0x3F));
            out[3] = (char)(0x80 | (c & 0x3F));
            return 4;
        }
        return 0;
    }

    // ============================================================
    // GetGlyphOutline — IMPLÉMENTATION COMPLÈTE ET ROBUSTE
    // ============================================================
    bool NkFont::GetGlyphOutlinePoints(NkFontCodepoint cp, NkVector<NkFontOutlineVertex>& outVertices) const {
        outVertices.Clear();
        if (!containerAtlas || !m_FaceInfo) return false;

        NkGlyphId gid = nkfont::NkFindGlyphIndex(m_FaceInfo, cp);
        if (gid == 0) return false;

        // Récupération des courbes du glyphe
        nkfont::NkFontVertexBuffer vb;
        if (!nkfont::NkGetGlyphShape(m_FaceInfo, gid, &vb)) return false;

        // Appliquer l'échelle et le flip Y (car les contours sont en coordonnées TrueType Y-up)
        // On applique la même transformation que dans le rastériseur.
        const float sx = scale;
        const float sy = scale;
        const float ox = 0.0f; // offset X (on peut utiliser les métriques si besoin)
        const float oy = 0.0f; // offset Y

        // Reconstruction des contours à partir des vertices
        NkVector<NkVector<NkVec2f>> contours;
        NkVector<NkVec2f> currentContour;
        bool started = false;
        const float flatness_sq = 0.35f * 0.35f; // tolérance d'aplatissement

        for (nkft_uint32 i = 0; i < vb.count; ++i) {
            const nkfont::NkFontVertex& v = vb.verts[i];
            // Transformation : flip Y (car TrueType a Y vers le haut, l'écran a Y vers le bas)
            NkVec2f pt(v.x * sx + ox, -v.y * sy + oy);

            switch (v.type) {
                case nkfont::NK_FONT_VERTEX_MOVE:
                    if (started && currentContour.Size() > 0) {
                        contours.PushBack(currentContour);
                        currentContour.Clear();
                    }
                    started = true;
                    currentContour.PushBack(pt);
                    break;

                case nkfont::NK_FONT_VERTEX_LINE:
                    currentContour.PushBack(pt);
                    break;

                case nkfont::NK_FONT_VERTEX_CURVE: {
                    NkVec2f ctrl(v.cx * sx + ox, -v.cy * sy + oy);
                    NkVector<NkVec2f> tmp;
                    FlattenQuadratic(currentContour.Back(), ctrl, pt, flatness_sq, 0, tmp);
                    for (const auto& p : tmp) currentContour.PushBack(p);
                    break;
                }

                case nkfont::NK_FONT_VERTEX_CUBIC: {
                    NkVec2f c0(v.cx * sx + ox, -v.cy * sy + oy);
                    NkVec2f c1(v.cx1 * sx + ox, -v.cy1 * sy + oy);
                    NkVector<NkVec2f> tmp;
                    FlattenCubic(currentContour.Back(), c0, c1, pt, flatness_sq, 0, tmp);
                    for (const auto& p : tmp) currentContour.PushBack(p);
                    break;
                }
            }
        }
        if (started && currentContour.Size() > 0) {
            contours.PushBack(currentContour);
        }

        // Convertir les contours en sommets plats avec marqueur de fin de contour
        for (const auto& contour : contours) {
            for (size_t i = 0; i < contour.Size(); ++i) {
                NkFontOutlineVertex vtx;
                vtx.x = contour[i].x;
                vtx.y = contour[i].y;
                vtx.isEndOfContour = (i == contour.Size() - 1);
                outVertices.PushBack(vtx);
            }
        }

        return outVertices.Size() > 0;
    }

} // namespace nkentseu

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// ============================================================