// =============================================================================
// NkUICanvasBackend.cpp — Backend NKUI -> NkRenderer2D (NKCanvas)
// =============================================================================
#include "NKCanvas/UI/NkUICanvasBackend.h"

#if defined(NK_CANVAS_WITH_NKUI) && NK_CANVAS_WITH_NKUI

#include "NKUI/NKUI.h"                               // NkUIContext / NkUIDrawList / NkUIVertex
#include "NKCanvas/Renderer/Core/NkIRenderer2D.h"    // NkIRenderer2D : DrawVertices/SetClip/PopClip
#include "NKMemory/NKMemory.h"                        // memory::NkGetDefaultAllocator

namespace nkentseu {
    namespace renderer {

        bool NkUICanvasBackend::Init(NkIRenderer2D* renderer) {
            if (!renderer) return false;
            mRenderer = renderer;
            return true;
        }

        void NkUICanvasBackend::Destroy() {
            auto& alloc = memory::NkGetDefaultAllocator();
            for (auto& it : mTextures) {
                if (it.Second) alloc.Delete(it.Second);
            }
            mTextures.Clear();
            mScratch.Clear();
            mExpand.Clear();
            mRenderer = nullptr;
        }

        bool NkUICanvasBackend::HasTexture(uint32 texId) const noexcept {
            if (texId == 0) return false;
            return mTextures.Find(texId) != nullptr;
        }

        NkTexture* NkUICanvasBackend::AcquireTexture(uint32 texId, int32 width, int32 height) {
            auto& alloc = memory::NkGetDefaultAllocator();
            NkTexture** found = mTextures.Find(texId);
            if (found && *found) {
                if ((*found)->GetWidth() == static_cast<uint32>(width) &&
                    (*found)->GetHeight() == static_cast<uint32>(height)) {
                    return *found;                       // reutilisable tel quel
                }
                alloc.Delete(*found);                    // taille differente -> recreer
                *found = nullptr;
            }
            NkTexture* tex = alloc.New<NkTexture>();
            if (!tex) return nullptr;
            if (!tex->Create(*mRenderer, static_cast<uint32>(width), static_cast<uint32>(height))) {
                alloc.Delete(tex);
                return nullptr;
            }
            tex->SetFilter(NkTextureFilter::NK_LINEAR);
            mTextures[texId] = tex;
            return tex;
        }

        bool NkUICanvasBackend::UploadTextureRGBA8(uint32 texId, const uint8* data, int32 width, int32 height) {
            if (!mRenderer || !data || width <= 0 || height <= 0 || texId == 0) return false;
            NkTexture* tex = AcquireTexture(texId, width, height);
            if (!tex) return false;
            return tex->Update(data, static_cast<uint32>(width), static_cast<uint32>(height), 0, 0);
        }

        bool NkUICanvasBackend::UploadTextureGray8(uint32 texId, const uint8* data, int32 width, int32 height) {
            if (!mRenderer || !data || width <= 0 || height <= 0 || texId == 0) return false;
            // Etend gray -> RGBA (blanc + alpha = gray) : convient aux atlas de police.
            const usize n = static_cast<usize>(width) * static_cast<usize>(height);
            mExpand.Resize(n * 4u);
            uint8* dst = mExpand.Data();
            for (usize i = 0; i < n; ++i) {
                dst[i * 4u + 0u] = 255u;
                dst[i * 4u + 1u] = 255u;
                dst[i * 4u + 2u] = 255u;
                dst[i * 4u + 3u] = data[i];
            }
            return UploadTextureRGBA8(texId, mExpand.Data(), width, height);
        }

        void NkUICanvasBackend::Submit(const nkui::NkUIContext& ctx, uint32 fbW, uint32 fbH) {
            if (!mRenderer) return;

            for (int32 layer = 0; layer < nkui::NkUIContext::LAYER_COUNT; ++layer) {
                const nkui::NkUIDrawList& dl = ctx.layers[layer];
                if (dl.cmdCount == 0u || dl.vtxCount == 0u || dl.idxCount == 0u) continue;

                // Conversion NkUIVertex -> NkVertex2D (une fois par couche).
                // col = NkColor::ToU32() = (a<<24)|(b<<16)|(g<<8)|r.
                mScratch.Resize(dl.vtxCount);
                for (uint32 i = 0; i < dl.vtxCount; ++i) {
                    const nkui::NkUIVertex& s = dl.vtx[i];
                    NkVertex2D& d = mScratch[i];
                    d.x = s.pos.x; d.y = s.pos.y;
                    d.u = s.uv.x;  d.v = s.uv.y;
                    d.r = static_cast<uint8>( s.col        & 0xFFu);
                    d.g = static_cast<uint8>((s.col >>  8) & 0xFFu);
                    d.b = static_cast<uint8>((s.col >> 16) & 0xFFu);
                    d.a = static_cast<uint8>((s.col >> 24) & 0xFFu);
                }

                for (uint32 ci = 0; ci < dl.cmdCount; ++ci) {
                    const nkui::NkUIDrawCmd& dc = dl.cmds[ci];
                    if (dc.idxCount == 0u) continue;
                    if (dc.type == nkui::NkUIDrawCmdType::NK_CLIP_RECT ||
                        dc.type == nkui::NkUIDrawCmdType::NK_SET_FONT) continue;   // non geometriques

                    // Clip (clamp au framebuffer). clipRect "infini" (1e9) -> pas de clip.
                    const bool hasClip = (dc.clipRect.w < 1.0e8f && dc.clipRect.h < 1.0e8f);
                    if (hasClip) {
                        float32 x0 = dc.clipRect.x < 0.f ? 0.f : dc.clipRect.x;
                        float32 y0 = dc.clipRect.y < 0.f ? 0.f : dc.clipRect.y;
                        float32 x1 = dc.clipRect.x + dc.clipRect.w;
                        float32 y1 = dc.clipRect.y + dc.clipRect.h;
                        if (x1 > static_cast<float32>(fbW)) x1 = static_cast<float32>(fbW);
                        if (y1 > static_cast<float32>(fbH)) y1 = static_cast<float32>(fbH);
                        if (x1 <= x0 || y1 <= y0) continue;     // clip vide -> rien a dessiner
                        mRenderer->SetClip(NkRect2i{ static_cast<int32>(x0), static_cast<int32>(y0),
                                                     static_cast<int32>(x1 - x0), static_cast<int32>(y1 - y0) });
                    }

                    NkTexture* tex = nullptr;
                    if (dc.type == nkui::NkUIDrawCmdType::NK_TEXTURED_TRIS && dc.texId != 0u) {
                        NkTexture** found = mTextures.Find(dc.texId);
                        if (found) tex = *found;
                    }

                    // Les indices de la cmd referencent le buffer vertex complet de la couche.
                    mRenderer->DrawVertices(mScratch.Data(), dl.vtxCount,
                                            dl.idx + dc.idxOffset, dc.idxCount, tex);

                    if (hasClip) mRenderer->PopClip();
                }
            }
        }

    } // namespace renderer
} // namespace nkentseu

#endif // NK_CANVAS_WITH_NKUI
