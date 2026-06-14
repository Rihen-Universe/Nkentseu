// =============================================================================
// NkFont.cpp — Wrapper GPU du module externe NKFont.
//
// Refactoring 2026-05-28 : NKCanvas ne réimplémente plus FreeType. Cette classe
// délègue la rasterisation au module NKFont (NkFontAtlas + NkFont), et gère côté
// GPU une "page" (atlas RGBA32 -> NkTexture) par taille de caractère demandée.
// =============================================================================
#include "NkFont.h"
#include "NKCanvas/Renderer/Core/NkIRenderer2D.h"

// Module externe NKFont (CPU) : rasterisation + atlas.
#include "NKFont/NkFont.h"

#include "NKMemory/NkAllocator.h"
#include "NKLogger/NkLog.h"
#include "NKFileSystem/NkFile.h"

#include <cstdio>
#include <cstring>
#include <utility>
#include <new>

#define NK_FONT_LOG(...) logger.Infof("[NkFont] " __VA_ARGS__)
#define NK_FONT_ERR(...) logger.Errorf("[NkFont] " __VA_ARGS__)

namespace nkentseu {
    namespace renderer {

        // =====================================================================
        // Move
        // =====================================================================
        NkFont::NkFont(NkFont&& o) noexcept
            : mFontData(std::move(o.mFontData))
            , mPages(std::move(o.mPages))
            , mRenderer(o.mRenderer)
            , mScratchGlyph(o.mScratchGlyph) {
            o.mRenderer = nullptr;
        }

        NkFont& NkFont::operator=(NkFont&& o) noexcept {
            if (this != &o) {
                Destroy();
                mFontData     = std::move(o.mFontData);
                mPages        = std::move(o.mPages);
                mRenderer     = o.mRenderer;
                mScratchGlyph = o.mScratchGlyph;
                o.mRenderer   = nullptr;
            }
            return *this;
        }

        // =====================================================================
        // Load
        // =====================================================================
        bool NkFont::LoadFromMemory(NkIRenderer2D& renderer,
                                    const void* data, usize sizeBytes) {
            if (!data || sizeBytes == 0) return false;
            Destroy();
            mRenderer = &renderer;
            mFontData.Resize((uint32)sizeBytes);
            ::memcpy(mFontData.Data(), data, sizeBytes);
            // Lazy : les pages (atlas + texture) sont créées à la demande dans
            // GetOrCreatePage, à la première taille de caractère utilisée.
            return true;
        }

        bool NkFont::LoadFromFile(NkIRenderer2D& renderer, const char* path) {
            if (!path) return false;
            // NKFileSystem (NkFile) au lieu de fopen/fread : coherent avec le moteur
            // + resolution unifiee des chemins/assets (Android).
            NkVector<nk_uint8> data = NkFile::ReadAllBytes(path);
            if (data.Empty()) { NK_FONT_ERR("LoadFromFile: ouverture/lecture impossible : %s", path); return false; }

            Destroy();
            mRenderer = &renderer;
            mFontData.Resize((uint32)data.Size());
            ::memcpy(mFontData.Data(), data.Data(), (size_t)data.Size());
            return true;
        }

        // =====================================================================
        // Pages (atlas module + texture GPU) — une par taille de caractère
        // =====================================================================
        NkFont::Page* NkFont::GetOrCreatePage(uint32 characterSize) const {
            for (usize i = 0; i < mPages.Size(); ++i)
                if (mPages[i] && mPages[i]->characterSize == characterSize)
                    return mPages[i];

            if (mFontData.Empty() || !mRenderer) return nullptr;

            // Alloue la Page (placement new — allocateur custom NKMemory).
            Page* page = static_cast<Page*>(memory::NkAlloc(sizeof(Page)));
            if (!page) return nullptr;
            new (page) Page();
            page->characterSize = characterSize;

            // Alloue l'atlas module (non-copyable -> heap + placement new).
            page->atlas = static_cast<::nkentseu::NkFontAtlas*>(
                memory::NkAlloc(sizeof(::nkentseu::NkFontAtlas)));
            if (!page->atlas) {
                page->~Page();
                memory::NkFree(page);
                return nullptr;
            }
            new (page->atlas) ::nkentseu::NkFontAtlas();

            // Rasterise la fonte à cette taille.
            page->moduleFont = page->atlas->AddFontFromMemory(
                mFontData.Data(),
                (nkft_size)mFontData.Size(),
                (nkft_float32)characterSize);
            page->atlas->Build();

            // Récupère l'atlas RGBA32 et l'uploade en texture GPU.
            nkft_uint8* pixels = nullptr;
            nkft_int32  w = 0, h = 0;
            page->atlas->GetTexDataAsRGBA32(&pixels, &w, &h);
            if (pixels && w > 0 && h > 0) {
                page->texture.Create(*mRenderer, (uint32)w, (uint32)h);
                page->texture.Update(pixels, (uint32)w, (uint32)h, 0, 0);
            } else {
                NK_FONT_ERR("GetOrCreatePage: atlas vide (taille %u)", characterSize);
            }

            mPages.PushBack(page);
            return page;
        }

        // =====================================================================
        // Glyph access
        // =====================================================================
        const NkGlyph& NkFont::GetGlyph(uint32 codepoint, uint32 characterSize,
                                        bool /*bold*/) const {
            mScratchGlyph = NkGlyph{};
            Page* page = GetOrCreatePage(characterSize);
            if (!page || !page->moduleFont || !page->atlas) return mScratchGlyph;

            const ::nkentseu::NkFontGlyph* g =
                page->moduleFont->FindGlyph((NkFontCodepoint)codepoint);
            if (!g) return mScratchGlyph;

            const int32 tw = (int32)page->atlas->texWidth;
            const int32 th = (int32)page->atlas->texHeight;

            // UV [0..1] -> rect pixels dans l'atlas.
            mScratchGlyph.textureRect.x      = (int32)(g->u0 * (float32)tw);
            mScratchGlyph.textureRect.y      = (int32)(g->v0 * (float32)th);
            mScratchGlyph.textureRect.width  = (int32)((g->u1 - g->u0) * (float32)tw);
            mScratchGlyph.textureRect.height = (int32)((g->v1 - g->v0) * (float32)th);

            // Quad local (bearing + taille), relatif au curseur.
            mScratchGlyph.bounds.x      = g->x0;
            mScratchGlyph.bounds.y      = g->y0;
            mScratchGlyph.bounds.width  = g->x1 - g->x0;
            mScratchGlyph.bounds.height = g->y1 - g->y0;

            mScratchGlyph.advance = g->advanceX;
            mScratchGlyph.valid   = true;
            return mScratchGlyph;
        }

        float32 NkFont::GetKerning(uint32 /*first*/, uint32 /*second*/,
                                   uint32 /*characterSize*/) const {
            // Le module NKFont n'expose pas le kerning par paire (intégré dans
            // l'advance des glyphes). Retour 0 : pas d'ajustement supplémentaire.
            return 0.f;
        }

        float32 NkFont::GetLineHeight(uint32 characterSize) const {
            Page* page = GetOrCreatePage(characterSize);
            if (page && page->moduleFont && page->moduleFont->lineAdvance > 0.f)
                return page->moduleFont->lineAdvance;
            return (float32)characterSize * 1.2f; // fallback raisonnable
        }

        const NkTexture* NkFont::GetAtlasTexture(uint32 characterSize) const {
            Page* page = GetOrCreatePage(characterSize);
            return page ? &page->texture : nullptr;
        }

        // =====================================================================
        // Destroy
        // =====================================================================
        void NkFont::Destroy() {
            for (usize i = 0; i < mPages.Size(); ++i) {
                Page* p = mPages[i];
                if (!p) continue;
                p->texture.Destroy();
                if (p->atlas) {
                    p->atlas->~NkFontAtlas();
                    memory::NkFree(p->atlas);
                    p->atlas = nullptr;
                }
                p->~Page();
                memory::NkFree(p);
            }
            mPages.Clear();
            mFontData.Clear();
            mRenderer = nullptr;
        }

    } // namespace renderer
} // namespace nkentseu
