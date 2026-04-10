// =============================================================================
// NkFont.cpp — FreeType 2 integration
// Link with: -lfreetype (CMake: target_link_libraries(... Freetype::Freetype))
// =============================================================================
#include "NkFont.h"
#include "NKContext/Renderer/Core/NkIRenderer2D.h"
#include "NKLogger/NkLog.h"

#include <cstdio>
#include <cstring>

// FreeType headers
// #include <ft2build.h>
// #include FT_FREETYPE_H
// #include FT_GLYPH_H
// #include FT_OUTLINE_H
// #include FT_BITMAP_H
// #include FT_STROKER_H

#define NK_FONT_LOG(...) logger.Infof("[NkFont] " __VA_ARGS__)
#define NK_FONT_ERR(...) logger.Errorf("[NkFont] " __VA_ARGS__)
#define NK_FT_CHECK(err, msg) do { if ((err) != 0) { NK_FONT_ERR(msg " (FT error %d)", (int)(err)); return false; } } while(0)

namespace nkentseu {
    namespace renderer {

        // =============================================================================
        NkFont::NkFont(NkFont&& other) noexcept
            : mFTLibrary(other.mFTLibrary), mFTFace(other.mFTFace)
            , mFontData(static_cast<NkVector<uint8>&&>(other.mFontData))
            , mPages(static_cast<NkVector<AtlasPage>&&>(other.mPages))
            , mRenderer(other.mRenderer)
        {
            other.mFTLibrary = nullptr;
            other.mFTFace    = nullptr;
            other.mRenderer  = nullptr;
        }

        NkFont& NkFont::operator=(NkFont&& other) noexcept {
            if (this != &other) {
                Destroy();
                mFTLibrary = other.mFTLibrary;
                mFTFace    = other.mFTFace;
                mFontData  = static_cast<NkVector<uint8>&&>(other.mFontData);
                mPages     = static_cast<NkVector<AtlasPage>&&>(other.mPages);
                mRenderer  = other.mRenderer;
                other.mFTLibrary = nullptr;
                other.mFTFace    = nullptr;
                other.mRenderer  = nullptr;
            }
            return *this;
        }

        // =============================================================================
        void NkFont::Destroy() {
            // if (mFTFace) {
            //     FT_Done_Face(static_cast<FT_Face>(mFTFace));
            //     mFTFace = nullptr;
            // }
            // if (mFTLibrary) {
            //     FT_Done_FreeType(static_cast<FT_Library>(mFTLibrary));
            //     mFTLibrary = nullptr;
            // }
            // mPages.Clear();
        }

        // =============================================================================
        bool NkFont::LoadFromFile(NkIRenderer2D& renderer, const char* path) {
            if (!path || !*path) return false;

            // Load file to memory
            FILE* f = fopen(path, "rb");
            if (!f) { NK_FONT_ERR("Cannot open '%s'", path); return false; }
            fseek(f, 0, SEEK_END);
            const long sz = ftell(f);
            rewind(f);
            if (sz <= 0) { fclose(f); NK_FONT_ERR("Empty file '%s'", path); return false; }
            mFontData.Resize((NkVector<uint8>::SizeType)sz);
            fread(mFontData.Data(), 1, (size_t)sz, f);
            fclose(f);

            return LoadFromMemory(renderer, mFontData.Data(), (usize)sz);
        }

        // =============================================================================
        bool NkFont::LoadFromMemory(NkIRenderer2D& renderer, const void* data, usize sizeBytes) {
            if (!data || sizeBytes == 0) return false;
            mRenderer = &renderer;

            // Initialize FreeType
            // FT_Library ftLib = nullptr;
            // FT_Error   err   = FT_Init_FreeType(&ftLib);
            // NK_FT_CHECK(err, "FT_Init_FreeType");
            // mFTLibrary = ftLib;

            // // Load font face from memory
            // FT_Face ftFace = nullptr;
            // err = FT_New_Memory_Face(ftLib,
            //     static_cast<const FT_Byte*>(data),
            //     (FT_Long)sizeBytes, 0, &ftFace);
            // NK_FT_CHECK(err, "FT_New_Memory_Face");
            // mFTFace = ftFace;

            // // Select Unicode charmap
            // err = FT_Select_Charmap(ftFace, FT_ENCODING_UNICODE);
            // if (err != 0) {
            //     NK_FONT_LOG("No Unicode charmap, trying first available");
            // }

            // NK_FONT_LOG("Loaded '%s %s'",
            //     ftFace->family_name  ? ftFace->family_name  : "?",
            //     ftFace->style_name   ? ftFace->style_name   : "?");
            return true;
        }

        // =============================================================================
        const NkGlyph* NkFont::AtlasPage::Find(const GlyphKey& key) const {
            for (const auto& cg : cache) {
                if (cg.key == key) return &cg.glyph;
            }
            return nullptr;
        }

        // =============================================================================
        NkFont::AtlasPage& NkFont::GetOrCreatePage(NkIRenderer2D& renderer, uint32 charSize) const {
            for (auto& page : mPages) {
                if (page.characterSize == charSize) return page;
            }
            AtlasPage page;
            page.characterSize = charSize;
            page.atlasW  = 512;
            page.atlasH  = 512;
            page.cpuPixels.Resize((NkVector<uint8>::SizeType)(page.atlasW * page.atlasH * 4u), 0u);
            // White background with zero alpha
            page.texture.Create(renderer, page.atlasW, page.atlasH, NkColor2D::Transparent());
            mPages.PushBack(static_cast<AtlasPage&&>(page));
            return mPages.Back();
        }

        // =============================================================================
        NkGlyph NkFont::LoadGlyph(AtlasPage& page, uint32 codepoint,
                                    uint32 charSize, bool bold) const {
            NkGlyph g;
            if (!mFTFace) return g;

            // FT_Face face = static_cast<FT_Face>(mFTFace);
            // FT_Error err = FT_Set_Pixel_Sizes(face, 0, charSize);
            // if (err != 0) return g;

            // const FT_UInt glyphIndex = FT_Get_Char_Index(face, (FT_ULong)codepoint);

            // FT_Int32 loadFlags = FT_LOAD_TARGET_NORMAL | FT_LOAD_FORCE_AUTOHINT;
            // err = FT_Load_Glyph(face, glyphIndex, loadFlags);
            // if (err != 0) return g;

            // if (bold) {
            //     // FT_GlyphSlot_Embolden(face->glyph);
            // }

            // err = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
            // if (err != 0) return g;

            // const FT_GlyphSlot slot   = face->glyph;
            // const FT_Bitmap&   bitmap = slot->bitmap;

            // const uint32 bmpW = (uint32)bitmap.width;
            // const uint32 bmpH = (uint32)bitmap.rows;
            // const uint32 pad  = 2;

            // // Row packing — advance to next row if needed
            // if (page.packX + bmpW + pad >= page.atlasW) {
            //     page.packX = pad;
            //     page.packY += page.rowHeight + pad;
            //     page.rowHeight = 0;
            // }
            // if (page.packY + bmpH + pad >= page.atlasH) {
            //     // Atlas full — in production you'd create a new page; log a warning here
            //     NK_FONT_ERR("Glyph atlas full for size %u", charSize);
            //     return g;
            // }

            // // Copy bitmap into CPU atlas
            // for (uint32 row = 0; row < bmpH; ++row) {
            //     const uint8* src = bitmap.buffer + row * (uint32)bitmap.pitch;
            //     uint8*       dst = page.cpuPixels.Data() +
            //                     ((usize)(page.packY + row) * page.atlasW + page.packX) * 4u;
            //     for (uint32 col = 0; col < bmpW; ++col) {
            //         dst[col*4+0] = 255;
            //         dst[col*4+1] = 255;
            //         dst[col*4+2] = 255;
            //         dst[col*4+3] = src[col];
            //     }
            // }

            // // Upload the touched region
            // page.texture.Update(page.cpuPixels.Data() + (page.packY * page.atlasW) * 4u,
            //                     page.atlasW, bmpH, 0, page.packY);

            // g.textureRect = { (int32)page.packX, (int32)page.packY, (int32)bmpW, (int32)bmpH };
            // g.bounds      = {
            //     (float32)slot->bitmap_left,
            //     -(float32)slot->bitmap_top,
            //     (float32)bmpW,
            //     (float32)bmpH
            // };
            // g.advance = (float32)(slot->advance.x >> 6);
            // g.valid   = true;

            // if (bmpH > page.rowHeight) page.rowHeight = bmpH;
            // page.packX += bmpW + pad;

            return g;
        }

        // =============================================================================
        const NkGlyph& NkFont::GetGlyph(uint32 codepoint, uint32 characterSize, bool bold) const {
            static NkGlyph sEmpty{};
            if (!mFTFace || !mRenderer) return sEmpty;

            AtlasPage& page = GetOrCreatePage(*mRenderer, characterSize);
            GlyphKey   key  = { codepoint, characterSize, bold };

            const NkGlyph* cached = page.Find(key);
            if (cached) return *cached;

            NkGlyph g = LoadGlyph(page, codepoint, characterSize, bold);
            AtlasPage::CachedGlyph cg;
            cg.key   = key;
            cg.glyph = g;
            page.cache.PushBack(cg);
            return page.cache.Back().glyph;
        }

        // =============================================================================
        float32 NkFont::GetKerning(uint32 first, uint32 second, uint32 characterSize) const {
            if (!mFTFace || first == 0 || second == 0) return 0.f;
            // FT_Face face = static_cast<FT_Face>(mFTFace);
            // if (!FT_HAS_KERNING(face)) return 0.f;
            // FT_Set_Pixel_Sizes(face, 0, characterSize);
            // FT_UInt gi1 = FT_Get_Char_Index(face, (FT_ULong)first);
            // FT_UInt gi2 = FT_Get_Char_Index(face, (FT_ULong)second);
            // FT_Vector kerning{};
            // FT_Get_Kerning(face, gi1, gi2, FT_KERNING_DEFAULT, &kerning);
            // return (float32)(kerning.x >> 6);
            return 0.f;
        }

        // =============================================================================
        float32 NkFont::GetLineHeight(uint32 characterSize) const {
            // if (!mFTFace) return (float32)characterSize;
            // FT_Face face = static_cast<FT_Face>(mFTFace);
            // FT_Set_Pixel_Sizes(face, 0, characterSize);
            // return (float32)(face->size->metrics.height >> 6);
            return 0.f;
        }

        // =============================================================================
        const NkTexture* NkFont::GetAtlasTexture(uint32 characterSize) const {
            for (const auto& page : mPages) {
                if (page.characterSize == characterSize) return &page.texture;
            }
            return nullptr;
        }

    } // namespace renderer
} // namespace nkentseu