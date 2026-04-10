// -----------------------------------------------------------------------------
// FICHIER: NKFont/Core/NkFontDetect.cpp
// DESCRIPTION: Implémentation de la détection automatique du type de fonte.
// -----------------------------------------------------------------------------

// ============================================================
// INCLUDES
// ============================================================

#include "NkFontDetect.h"
#include "NKFont/NkFont.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  define nkft_fopen(p, m) fopen(p, m)
#else
#  define nkft_fopen(p, m) fopen(p, m)
#endif

namespace nkentseu {

    // ============================================================
    // Codepoints de test représentatifs
    // ============================================================

    const NkFontCodepoint NkFontDetector::sDefaultTestCodepoints[] = {
        'A', 'B', 'C', 'E', 'G', 'H', 'O', 'Q', 'S',  // majuscules avec courbes
        'a', 'b', 'c', 'd', 'e', 'g', 'o', 'p', 'q',  // minuscules avec courbes
        '0', '3', '5', '6', '8', '9',                 // chiffres
        '!', '?', '@',                                // ponctuation
    };

    const nkft_int32 NkFontDetector::sDefaultTestCount =
        (nkft_int32)(sizeof(sDefaultTestCodepoints) / sizeof(sDefaultTestCodepoints[0]));

    // ============================================================
    // GlyphHasCurves — analyse les vertices d'un glyphe
    // ============================================================

    bool NkFontDetector::GlyphHasCurves(const nkfont::NkFontFaceInfo* info,
                                        NkGlyphId glyph) {
        nkfont::NkFontVertexBuffer vbuf;
        if (!nkfont::NkGetGlyphShape(info, glyph, &vbuf)) return false;

        for (nkft_uint32 i = 0; i < vbuf.count; ++i) {
            const auto& v = vbuf.verts[i];
            if (v.type == nkfont::NK_FONT_VERTEX_CURVE ||
                v.type == nkfont::NK_FONT_VERTEX_CUBIC) {
                return true;
            }
        }
        return false;
    }

    // ============================================================
    // DetectNativeSize — taille native d'une police bitmap
    // ============================================================

    nkft_float32 NkFontDetector::DetectNativeSize(const nkfont::NkFontFaceInfo* info) {
        // Pour une police bitmap, unitsPerEm est souvent une puissance de 2 basse
        // ou la taille pixel est directement lisible via os2.sTypoAscender
        nkft_int32 asc, desc, lg;
        nkfont::NkGetFontVMetrics(info, &asc, &desc, &lg);
        nkft_int32 fullH = asc - desc;
        if (fullH <= 0) return 0.f;

        // Si unitsPerEm est très petit, la fonte est bitmap
        // et la taille naturelle = unitsPerEm pixels
        nkft_int32 upm = info->unitsPerEm;
        if (upm > 0 && upm <= 256) {
            // Arrondit à la taille pixel la plus proche
            // ex: ProggyClean a upm=512, fullH=512 → taille native 13px empiriquement
            // On calcule scale = 1.0 (1 font_unit = 1 pixel)
            nkft_float32 nativePx = (nkft_float32)upm; // 1:1 mapping
            // Common bitmap sizes: 8, 9, 10, 11, 12, 13, 14, 16, 18, 20
            static const nkft_float32 snapSizes[] = {8, 9, 10, 11, 12, 13, 14, 16, 18, 20, 24, 0};
            nkft_float32 scale = nkfont::NkScaleForPixelHeight(info, nativePx);
            nkft_float32 computedPx = (nkft_float32)fullH * scale;

            for (int i = 0; snapSizes[i] > 0; ++i) {
                if (fabsf(computedPx - snapSizes[i]) < 1.5f) {
                    return snapSizes[i];
                }
            }
        }
        return 0.f;
    }

    // ============================================================
    // Analyze
    // ============================================================

    NkFontProfile NkFontDetector::Analyze(const nkfont::NkFontFaceInfo* info,
                                          const NkFontCodepoint* testCps,
                                          nkft_int32 testCount) {
        NkFontProfile profile;
        if (!info) return profile;

        profile.isCFF = info->isCFF;

        // ── Détection CFF ─────────────────────────────────────────────────────
        if (info->isCFF) {
            profile.kind               = NkFontKind::VectorCFF;
            profile.oversampleH        = 2;
            profile.oversampleV        = 1;
            profile.useNearestFilter   = false;
            profile.pixelSnapH         = false;
            return profile;
        }

        // ── Détection table de bitmaps embarqués ─────────────────────────────
        // (EBDT/EBLC ou CBDT/CBLC — tables de glyphes bitmap dans les TTF)
        // Note: on ne les lit pas, mais leur présence indique une police mixte.
        // Pour simplifier on se base sur l'analyse des contours.

        // ── Analyse des contours ──────────────────────────────────────────────
        const NkFontCodepoint* cps    = testCps ? testCps : sDefaultTestCodepoints;
        nkft_int32              count = testCps ? testCount : sDefaultTestCount;

        nkft_int32 tested    = 0;
        nkft_int32 withCurve = 0;

        for (nkft_int32 i = 0; i < count; ++i) {
            NkGlyphId gid = nkfont::NkFindGlyphIndex(info, cps[i]);
            if (gid == 0) continue;
            ++tested;
            if (GlyphHasCurves(info, gid)) ++withCurve;
        }

        profile.sampleGlyphCount = tested;
        profile.curveGlyphCount  = withCurve;
        profile.curveRatio = (tested > 0) ? (nkft_float32)withCurve / (nkft_float32)tested : 0.f;

        // ── Classification ────────────────────────────────────────────────────
        // Un ratio de courbes < 10% → police bitmap (lignes droites uniquement)
        const bool isBitmap = (profile.curveRatio < 0.10f);

        if (isBitmap) {
            profile.kind                 = NkFontKind::Bitmap;
            profile.oversampleH          = 1;  // oversample inutile sur bitmap
            profile.oversampleV          = 1;
            profile.pixelSnapH           = true;
            profile.useNearestFilter     = true;
            profile.nativeSizePx         = DetectNativeSize(info);
            profile.recommendedMinSizePx = profile.nativeSizePx > 0.f ? profile.nativeSizePx : 8.f;
            profile.recommendedMaxSizePx = profile.nativeSizePx * 4.f; // multiples entiers
        } else {
            profile.kind                 = NkFontKind::Vector;
            profile.oversampleH          = 2;  // bon compromis qualité/atlas size
            profile.oversampleV          = 1;
            profile.pixelSnapH           = false;
            profile.useNearestFilter     = false;
            profile.nativeSizePx         = 0.f; // pas de taille native pour vectoriel
            profile.recommendedMinSizePx = 8.f;
            profile.recommendedMaxSizePx = 144.f;
        }

        return profile;
    }

    // ============================================================
    // AnalyzeBuffer / AnalyzeFile
    // ============================================================

    NkFontProfile NkFontDetector::AnalyzeBuffer(const nkft_uint8* data,
                                                nkft_size size,
                                                nkft_int32 faceIndex) {
        nkfont::NkFontFaceInfo info;
        if (!nkfont::NkInitFontFace(&info, data, size, faceIndex)) {
            return NkFontProfile{};
        }
        return Analyze(&info);
    }

    NkFontProfile NkFontDetector::AnalyzeFile(const char* path,
                                              nkft_int32 faceIndex) {
        FILE* f = nkft_fopen(path, "rb");
        if (!f) return NkFontProfile{};

        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (sz <= 0) {
            fclose(f);
            return NkFontProfile{};
        }

        nkft_uint8* data = (nkft_uint8*)malloc((nkft_size)sz);
        if (!data) {
            fclose(f);
            return NkFontProfile{};
        }
        fread(data, 1, (nkft_size)sz, f);
        fclose(f);

        NkFontProfile result = AnalyzeBuffer(data, (nkft_size)sz, faceIndex);
        free(data);
        return result;
    }

    // ============================================================
    // ApplyOptimalConfig
    // ============================================================

    void NkFontDetector::ApplyOptimalConfig(const NkFontProfile& profile,
                                            nkft_float32 sizePx,
                                            NkFontConfig* outConfig) {
        if (!outConfig) return;

        outConfig->oversampleH = profile.oversampleH;
        outConfig->oversampleV = profile.oversampleV;
        outConfig->pixelSnapH  = profile.pixelSnapH;
        outConfig->sizePixels  = SnapSize(profile, sizePx);
    }

    // ============================================================
    // RecommendsNearestFilter
    // ============================================================

    bool NkFontDetector::RecommendsNearestFilter(const NkFontProfile& profile) {
        return profile.useNearestFilter;
    }

    // ============================================================
    // SnapSize
    // ============================================================

    nkft_float32 NkFontDetector::SnapSize(const NkFontProfile& profile,
                                          nkft_float32 sizePx) {
        if (profile.kind != NkFontKind::Bitmap || profile.nativeSizePx <= 0.f) {
            return sizePx; // vectoriel : taille libre
        }

        // Pour les bitmaps, snap au multiple entier le plus proche de la taille native
        nkft_float32 native   = profile.nativeSizePx;
        nkft_float32 ratio    = sizePx / native;
        nkft_float32 snapped  = floorf(ratio + 0.5f); // arrondit au multiple entier
        if (snapped < 1.f) snapped = 1.f;
        return snapped * native;
    }

} // namespace nkentseu

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// ============================================================