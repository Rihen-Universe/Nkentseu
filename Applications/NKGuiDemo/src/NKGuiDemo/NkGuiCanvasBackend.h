#pragma once
// =============================================================================
// NkGuiCanvasBackend.h (démo) — rend un NkGuiDrawList via NKCanvas (NkIRenderer2D).
// Backend LOCAL à la démo (le cœur NKGui reste render-agnostique). Gère l'atlas
// de police (gray8 -> RGBA blanc+alpha) pour les commandes texturées. Modelé sur
// NkUICanvasBackend. Sera industrialisé en Phase 7 (Integrations).
// =============================================================================
#include "NKCanvas/Renderer/Core/NkIRenderer2D.h"
#include "NKCanvas/Renderer/Core/NkRenderer2DTypes.h"   // NkVertex2D
#include "NKCanvas/Renderer/Resources/NkTexture.h"      // NkTexture / NkTextureFilter
#include "NKContainers/Sequential/NkVector.h"
#include "NKMemory/NKMemory.h"                           // NkGetDefaultAllocator
#include "NKMath/NkRectangle.h"                          // NkRect2i
#include "NKGui/NKGui.h"

namespace nkgdemo {

    class NkGuiCanvasBackend {
    public:
        ~NkGuiCanvasBackend() {
            auto& alloc = nkentseu::memory::NkGetDefaultAllocator();
            if (mFontTex) { alloc.Delete(mFontTex); mFontTex = nullptr; }
            for (nkentseu::uint32 i = 0; i < mImages.Size(); ++i)
                if (mImages[i].tex) alloc.Delete(mImages[i].tex);
        }

        bool Init(nkentseu::renderer::NkIRenderer2D* renderer) {
            mRenderer = renderer;
            return renderer != nullptr;
        }

        // Upload (ou ré-upload) d'un atlas alpha8 sous l'id `texId` : étend gray ->
        // RGBA (blanc + alpha = couverture du glyphe).
        bool UploadFontGray8(nkentseu::uint32 texId, const nkentseu::uint8* gray,
                             nkentseu::int32 w, nkentseu::int32 h) {
            using namespace nkentseu;
            if (!mRenderer || !gray || w <= 0 || h <= 0 || texId == 0u) return false;

            const usize n = static_cast<usize>(w) * static_cast<usize>(h);
            mExpand.Resize(n * 4u);
            uint8* d = mExpand.Data();
            for (usize i = 0; i < n; ++i) {
                d[i * 4u + 0u] = 255u;
                d[i * 4u + 1u] = 255u;
                d[i * 4u + 2u] = 255u;
                d[i * 4u + 3u] = gray[i];
            }

            // (Re)créer la texture si elle n'existe pas OU si la taille change (rechargement
            // de police à une autre taille = DPI). Sinon Update() déborderait l'ancienne taille.
            if (!mFontTex || mFontTexW != w || mFontTexH != h) {
                auto& alloc = memory::NkGetDefaultAllocator();
                if (mFontTex) { alloc.Delete(mFontTex); mFontTex = nullptr; }
                mFontTex = alloc.New<renderer::NkTexture>();
                if (!mFontTex) return false;
                if (!mFontTex->Create(*mRenderer, static_cast<uint32>(w), static_cast<uint32>(h))) {
                    alloc.Delete(mFontTex); mFontTex = nullptr; return false;
                }
                mFontTex->SetFilter(renderer::NkTextureFilter::NK_LINEAR);
                mFontTexW = w; mFontTexH = h;
            }
            mFontTexId = texId;
            return mFontTex->Update(mExpand.Data(), static_cast<uint32>(w), static_cast<uint32>(h), 0, 0);
        }

        // Upload (ou ré-upload) d'une VRAIE image RGBA (4 octets/pixel, tight) sous
        // `texId` → texture résolue par Submit pour les commandes Image().
        bool UploadImageRGBA(nkentseu::uint32 texId, const nkentseu::uint8* rgba,
                             nkentseu::int32 w, nkentseu::int32 h) {
            using namespace nkentseu;
            if (!mRenderer || !rgba || w <= 0 || h <= 0 || texId == 0u) return false;
            renderer::NkTexture* tex = nullptr;
            for (uint32 i = 0; i < mImages.Size(); ++i) if (mImages[i].id == texId) { tex = mImages[i].tex; break; }
            if (!tex) {
                auto& alloc = memory::NkGetDefaultAllocator();
                tex = alloc.New<renderer::NkTexture>();
                if (!tex) return false;
                if (!tex->Create(*mRenderer, static_cast<uint32>(w), static_cast<uint32>(h))) {
                    alloc.Delete(tex); return false;
                }
                tex->SetFilter(renderer::NkTextureFilter::NK_LINEAR);
                mImages.PushBack(ImgTex{ texId, tex });
            }
            return tex->Update(rgba, static_cast<uint32>(w), static_cast<uint32>(h), 0, 0);
        }

        void Submit(const nkentseu::nkgui::NkGuiDrawList& dl,
                    nkentseu::uint32 fbW, nkentseu::uint32 fbH) {
            using namespace nkentseu;
            if (!mRenderer || dl.vtx.Size() == 0 || dl.idx.Size() == 0) return;

            mScratch.Resize(dl.vtx.Size());
            for (uint32 i = 0; i < dl.vtx.Size(); ++i) {
                const nkgui::NkGuiVertex& s = dl.vtx[i];
                renderer::NkVertex2D& d = mScratch[i];
                d.x = s.pos.x; d.y = s.pos.y;
                d.u = s.uv.x;  d.v = s.uv.y;
                d.r = static_cast<uint8>( s.col        & 0xFFu);
                d.g = static_cast<uint8>((s.col >>  8) & 0xFFu);
                d.b = static_cast<uint8>((s.col >> 16) & 0xFFu);
                d.a = static_cast<uint8>((s.col >> 24) & 0xFFu);
            }

            for (uint32 ci = 0; ci < dl.cmds.Size(); ++ci) {
                const nkgui::NkGuiDrawCmd& dc = dl.cmds[ci];
                if (dc.idxCount == 0u) continue;

                const bool hasClip = (dc.clipRect.w < 1.0e8f && dc.clipRect.h < 1.0e8f);
                if (hasClip) {
                    float32 x0 = dc.clipRect.x < 0.f ? 0.f : dc.clipRect.x;
                    float32 y0 = dc.clipRect.y < 0.f ? 0.f : dc.clipRect.y;
                    float32 x1 = dc.clipRect.x + dc.clipRect.w;
                    float32 y1 = dc.clipRect.y + dc.clipRect.h;
                    if (x1 > static_cast<float32>(fbW)) x1 = static_cast<float32>(fbW);
                    if (y1 > static_cast<float32>(fbH)) y1 = static_cast<float32>(fbH);
                    if (x1 <= x0 || y1 <= y0) continue;
                    mRenderer->SetClip(math::NkRect2i{ static_cast<int32>(x0), static_cast<int32>(y0),
                                                       static_cast<int32>(x1 - x0), static_cast<int32>(y1 - y0) });
                }

                renderer::NkTexture* tex = nullptr;
                if (dc.type == nkgui::NkGuiDrawCmdType::TexturedTriangles) {
                    if (dc.texId == mFontTexId) tex = mFontTex;
                    else for (uint32 ti = 0; ti < mImages.Size(); ++ti)
                        if (mImages[ti].id == dc.texId) { tex = mImages[ti].tex; break; }
                }

                mRenderer->DrawVertices(mScratch.Data(), static_cast<uint32>(dl.vtx.Size()),
                                        dl.idx.Data() + dc.idxOffset, dc.idxCount, tex);

                if (hasClip) mRenderer->PopClip();
            }
        }

    private:
        struct ImgTex { nkentseu::uint32 id; nkentseu::renderer::NkTexture* tex; };
        nkentseu::renderer::NkIRenderer2D*                 mRenderer  = nullptr;
        nkentseu::renderer::NkTexture*                     mFontTex   = nullptr;
        nkentseu::uint32                                   mFontTexId = 0u;
        nkentseu::int32                                    mFontTexW  = 0;
        nkentseu::int32                                    mFontTexH  = 0;
        nkentseu::NkVector<ImgTex>                         mImages;
        nkentseu::NkVector<nkentseu::renderer::NkVertex2D> mScratch;
        nkentseu::NkVector<nkentseu::uint8>                mExpand;
    };

} // namespace nkgdemo
