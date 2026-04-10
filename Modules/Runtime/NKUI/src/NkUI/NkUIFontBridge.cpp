// -----------------------------------------------------------------------------
// FICHIER: NkUIFontBridge.cpp
// DESCRIPTION: Implémentation du pont NKUIFont ↔ NKFont.
//
// Ce fichier fait le lien entre :
//   - NkUIFontAtlas (système UI, simple atlas CPU/GPU Gray8)
//   - NkFontParser   (système NKFont, parser TTF/OTF + rastériseur)
//
// Il rastérise les glyphes demandés directement dans l'atlas NkUIFontAtlas
// et remplit le NkUIFont associé avec les bonnes métriques.
// -----------------------------------------------------------------------------

#include "NkUIFontBridge.h"
#include "NKFont/Core/NkFontParser.h"
#include "NKLogger/NkLog.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace nkentseu {
    namespace nkui {

        // ─────────────────────────────────────────────────────────────────────────────
        // Plages Unicode prédéfinies
        // ─────────────────────────────────────────────────────────────────────────────

        static const nkft_uint32 sRangesASCII[]    = { 0x0020, 0x007E, 0 };
        static const nkft_uint32 sRangesLatinExt[] = { 0x0020, 0x007E, 0x00A0, 0x00FF, 0 };

        const nkft_uint32* NkUIFontBridge::RangesASCII()        { return sRangesASCII;    }
        const nkft_uint32* NkUIFontBridge::RangesLatinExtended(){ return sRangesLatinExt; }
        const nkft_uint32* NkUIFontBridge::RangesDefault()      { return sRangesLatinExt; }

        // ─────────────────────────────────────────────────────────────────────────────
        // Destroy
        // ─────────────────────────────────────────────────────────────────────────────

        void NkUIFontBridge::Destroy() {
            if (useNKFont) {
                nkfont::NkFreeFontFace(&nkfontFace);
                if (nkfontData) { free(nkfontData); nkfontData = nullptr; }
                nkfontSize = 0;
            } else {
                if (customDesc.Destroy && fontHandle) {
                    customDesc.Destroy(fontHandle, customDesc.userData);
                }
                fontHandle = nullptr;
            }
            atlas  = nullptr;
            font   = nullptr;
        }

        // ─────────────────────────────────────────────────────────────────────────────
        // Init depuis fichier (backend NKFont)
        // ─────────────────────────────────────────────────────────────────────────────

        bool NkUIFontBridge::InitFromFile(NkUIFontAtlas* atlasIn, NkUIFont* fontOut,
                                        const char* path, nkft_float32 sizePx,
                                        const nkft_uint32* ranges)
        {
            if (!atlasIn || !fontOut || !path) {
                logger.Error("[NkUIFontBridge] InitFromFile: arguments invalides\n");
                return false;
            }

            // Lire le fichier
            FILE* f = fopen(path, "rb");
            if (!f) {
                logger.Error("[NkUIFontBridge] Fichier introuvable: %s\n", path);
                return false;
            }
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fseek(f, 0, SEEK_SET);
            if (sz <= 0) { fclose(f); return false; }

            nkft_uint8* data = (nkft_uint8*)malloc((nkft_size)sz);
            if (!data) { fclose(f); return false; }
            if ((long)fread(data, 1, (nkft_size)sz, f) != sz) {
                fclose(f); free(data); return false;
            }
            fclose(f);

            logger.Info("[NkUIFontBridge] Police lue: %s (%ld o)\n", path, sz);
            return InitFromMemory(atlasIn, fontOut, data, (nkft_size)sz, sizePx, ranges);
            // Note : InitFromMemory copie les données, donc on libère ici
            // Mais puisqu'on passe les données directement à InitFontFace qui garde
            // le pointeur, on stocke le pointeur dans nkfontData
        }

        // ─────────────────────────────────────────────────────────────────────────────
        // Init depuis mémoire (backend NKFont)
        // ─────────────────────────────────────────────────────────────────────────────

        bool NkUIFontBridge::InitFromMemory(NkUIFontAtlas* atlasIn, NkUIFont* fontOut,
                                            const nkft_uint8* data, nkft_size dataSize,
                                            nkft_float32 sizePx,
                                            const nkft_uint32* ranges)
        {
            if (!atlasIn || !fontOut || !data || dataSize == 0) return false;

            atlas     = atlasIn;
            font      = fontOut;
            useNKFont = true;

            // Copie les données (NkFontParser garde le pointeur)
            nkfontData = (nkft_uint8*)malloc(dataSize);
            if (!nkfontData) return false;
            memcpy(nkfontData, data, dataSize);
            nkfontSize = dataSize;

            // Initialise la face NKFont
            memset(&nkfontFace, 0, sizeof(nkfontFace));
            if (!nkfont::NkInitFontFace(&nkfontFace, nkfontData, nkfontSize, 0)) {
                logger.Error("[NkUIFontBridge] InitFontFace échoué\n");
                free(nkfontData); nkfontData = nullptr;
                return false;
            }

            logger.Info("[NkUIFontBridge] Face initialisée: CFF=%d, unitsPerEm=%d\n",
                    nkfontFace.isCFF, nkfontFace.unitsPerEm);

            return BuildAtlas(ranges ? ranges : RangesDefault(), sizePx);
        }

        // ─────────────────────────────────────────────────────────────────────────────
        // BuildAtlas — NKFont backend
        // ─────────────────────────────────────────────────────────────────────────────

        bool NkUIFontBridge::BuildAtlas(const nkft_uint32* ranges, nkft_float32 sizePx)
        {
            if (!atlas || !font || !ranges) return false;

            nkft_float32 scale = nkfont::NkScaleForPixelHeight(&nkfontFace, sizePx);

            // Métriques globales de la police
            {
                nkft_int32 asc, desc, lg;
                nkfont::NkGetFontVMetrics(&nkfontFace, &asc, &desc, &lg);
                font->metrics.ascender   = (nkft_float32)asc  * scale;
                font->metrics.descender  = (nkft_float32)(-desc) * scale; // positif pour descender
                font->metrics.lineHeight = (nkft_float32)(asc - desc + lg) * scale;
                font->metrics.lineGap    = (nkft_float32)lg   * scale;
                font->size               = sizePx;
                font->isBuiltin          = false;
                font->atlas              = atlas;

                // Largeur de l'espace
                NkGlyphId spaceGid = nkfont::NkFindGlyphIndex(&nkfontFace, 0x0020);
                if (spaceGid) {
                    nkft_int32 adv = 0;
                    nkfont::NkGetGlyphHMetrics(&nkfontFace, spaceGid, &adv, nullptr);
                    font->metrics.spaceWidth = (nkft_float32)adv * scale;
                } else {
                    font->metrics.spaceWidth = sizePx * 0.45f;
                }
                font->metrics.tabWidth = font->metrics.spaceWidth * 4.f;
            }

            nkft_uint32 numLoaded = 0, numSkipped = 0;

            // Parcours des plages Unicode
            for (nkft_uint32 ri = 0; ranges[ri] && ranges[ri+1]; ri += 2) {
                for (nkft_uint32 cp = ranges[ri]; cp <= ranges[ri+1]; ++cp) {

                    NkGlyphId gid = nkfont::NkFindGlyphIndex(&nkfontFace, cp);
                    if (gid == 0) { ++numSkipped; continue; }

                    // Bounding box du glyphe en pixels
                    nkft_int32 bx0, by0, bx1, by1;
                    nkfont::NkGetGlyphBitmapBox(&nkfontFace, gid,
                                            scale, scale, 0.f, 0.f,
                                            &bx0, &by0, &bx1, &by1);

                    nkft_int32 gw = bx1 - bx0;
                    nkft_int32 gh = by1 - by0;

                    // Métriques d'avance
                    nkft_int32 advW = 0, lsb = 0;
                    nkfont::NkGetGlyphHMetrics(&nkfontFace, gid, &advW, &lsb);
                    nkft_float32 advanceX  = (nkft_float32)advW * scale;
                    nkft_float32 bearingX  = (nkft_float32)lsb  * scale;
                    nkft_float32 bearingY  = font->metrics.ascender + (nkft_float32)by0;
                    // bearingY = distance du haut du glyphe à la baseline (positif vers le haut)
                    // Dans NkUIFont : top_of_glyph = baseline_y - bearingY
                    // stb convention : by0 est négatif pour les glyphes au-dessus de la baseline

                    if (gw > 0 && gh > 0) {
                        // Alloue dans l'atlas
                        nkft_int32 atlasX, atlasY;
                        if (!atlas->Alloc(gw, gh, atlasX, atlasY)) {
                            logger.Warn("[NkUIFontBridge] Atlas plein au glyphe U+%04X\n", cp);
                            break;
                        }

                        // Rastérise le glyphe directement dans l'atlas
                        nkft_uint8* dst = atlas->pixels + atlasY * NkUIFontAtlas::ATLAS_W + atlasX;
                        nkfont::NkMakeGlyphBitmap(&nkfontFace, dst, gw, gh,
                                                NkUIFontAtlas::ATLAS_W,
                                                scale, scale, 0.f, 0.f, gid);

                        // Enregistre le glyphe dans l'atlas
                        atlas->AddGlyph(cp, atlasX, atlasY, gw, gh,
                                        advanceX, bearingX,
                                        // bearingY en convention NkUIFont (distance baseline→top du glyph)
                                        -(nkft_float32)by0);
                        ++numLoaded;
                    } else {
                        // Glyphe invisible (espace, etc.) — ajouter quand même les métriques
                        if (atlas->numGlyphs < NkUIFontAtlas::MAX_GLYPHS) {
                            NkUIGlyph& g = atlas->glyphs[atlas->numGlyphs++];
                            g.codepoint = cp;
                            g.advanceX  = advanceX;
                            g.bearingX  = bearingX;
                            g.bearingY  = 0.f;
                            g.x0 = g.y0 = g.x1 = g.y1 = 0.f;
                            g.u0 = g.v0 = g.u1 = g.v1 = 0.f;
                        }
                        ++numLoaded;
                    }
                }
            }

            atlas->dirty = true;

            logger.Info("[NkUIFontBridge] Build: %u glyphes chargés, %u ignorés, asc=%.1f desc=%.1f\n",
                    numLoaded, numSkipped,
                    font->metrics.ascender, font->metrics.descender);

            return numLoaded > 0;
        }

        // ─────────────────────────────────────────────────────────────────────────────
        // Init avec backend custom
        // ─────────────────────────────────────────────────────────────────────────────

        bool NkUIFontBridge::InitCustom(NkUIFontAtlas* atlasIn, NkUIFont* fontOut,
                                        const char* path, nkft_float32 sizePx,
                                        const NkUIFontLoaderDesc& desc,
                                        const nkft_uint32* ranges)
        {
            if (!atlasIn || !fontOut || !desc.IsValid()) {
                logger.Error("[NkUIFontBridge] InitCustom: descripteur invalide\n");
                return false;
            }

            atlas      = atlasIn;
            font       = fontOut;
            useNKFont  = false;
            customDesc = desc;

            // Charge la police via le callback
            if (!desc.LoadFont(path, nullptr, 0, sizePx, desc.userData, &fontHandle)) {
                logger.Error("[NkUIFontBridge] LoadFont custom échoué: %s\n", path ? path : "null");
                return false;
            }
            if (!fontHandle) return false;

            // Récupère les métriques
            desc.GetMetrics(fontHandle, desc.userData, &fontOut->metrics);
            fontOut->size      = sizePx;
            fontOut->isBuiltin = false;
            fontOut->atlas     = atlasIn;

            return BuildAtlasCustom(ranges ? ranges : RangesDefault(), sizePx);
        }

        // ─────────────────────────────────────────────────────────────────────────────
        // BuildAtlas — backend custom
        // ─────────────────────────────────────────────────────────────────────────────

        bool NkUIFontBridge::BuildAtlasCustom(const nkft_uint32* ranges, nkft_float32 sizePx)
        {
            if (!atlas || !font || !ranges || !fontHandle) return false;

            nkft_uint32 numLoaded = 0;

            for (nkft_uint32 ri = 0; ranges[ri] && ranges[ri+1]; ri += 2) {
                for (nkft_uint32 cp = ranges[ri]; cp <= ranges[ri+1]; ++cp) {

                    nkft_int32 gw = 0, gh = 0;
                    if (!customDesc.GetBBox(fontHandle, cp, customDesc.userData, &gw, &gh))
                        continue;

                    nkft_int32 atlasX = 0, atlasY = 0;
                    NkUIGlyph  glyph{};
                    glyph.codepoint = cp;

                    if (gw > 0 && gh > 0) {
                        if (!atlas->Alloc(gw, gh, atlasX, atlasY)) {
                            logger.Warn("[NkUIFontBridge] Atlas plein (custom) U+%04X\n", cp);
                            break;
                        }

                        // Le callback rastérise directement dans l'atlas
                        if (!customDesc.GetGlyph(fontHandle, cp,
                                                atlas->pixels, NkUIFontAtlas::ATLAS_W,
                                                atlasX, atlasY, gw, gh,
                                                customDesc.userData, &glyph)) {
                            continue;
                        }

                        // Met à jour les UVs (le callback peut avoir rempli les UVs lui-même,
                        // ou on les calcule ici si le callback ne le fait pas)
                        if (glyph.u0 == 0.f && glyph.u1 == 0.f) {
                            glyph.x0 = (nkft_float32)atlasX;
                            glyph.y0 = (nkft_float32)atlasY;
                            glyph.x1 = (nkft_float32)(atlasX + gw);
                            glyph.y1 = (nkft_float32)(atlasY + gh);
                            glyph.u0 = (nkft_float32)atlasX  / NkUIFontAtlas::ATLAS_W;
                            glyph.v0 = (nkft_float32)atlasY  / NkUIFontAtlas::ATLAS_H;
                            glyph.u1 = (nkft_float32)(atlasX+gw) / NkUIFontAtlas::ATLAS_W;
                            glyph.v1 = (nkft_float32)(atlasY+gh) / NkUIFontAtlas::ATLAS_H;
                        }
                    }

                    if (atlas->numGlyphs < NkUIFontAtlas::MAX_GLYPHS)
                        atlas->glyphs[atlas->numGlyphs++] = glyph;
                    ++numLoaded;
                }
            }

            atlas->dirty = true;
            logger.Info("[NkUIFontBridge] Build custom: %u glyphes\n", numLoaded);
            return numLoaded > 0;
        }

    } // namespace nkui
} // namespace nkentseu