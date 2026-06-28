#pragma once
// =============================================================================
// NkGuiCanvasBackend.h — rend un nkgui::NkGuiDrawList via NKCanvas (NkIRenderer2D).
// Backend RÉUTILISABLE (lib) : le cœur NKGui reste render-agnostique ; ce pont
// traduit ses draw-lists en appels NkIRenderer2D. Gère l'atlas de police
// (gray8 -> RGBA blanc+alpha) et les images RGBA pour les commandes texturées.
//
// HEADER-ONLY : NKCanvas ne le compile pas (pas de .cpp) ; seuls les consommateurs
// (qui dépendent déjà de NKGui ET NKCanvas) l'incluent. Modelé sur NkUICanvasBackend.
// =============================================================================
#include "NKCanvas/Renderer/Core/NkIRenderer2D.h"
#include "NKCanvas/Renderer/Core/NkRenderer2DTypes.h"   // NkVertex2D
#include "NKCanvas/Renderer/Resources/NkTexture.h"      // NkTexture / NkTextureFilter
#include "NKContainers/Sequential/NkVector.h"
#include "NKMemory/NKMemory.h"                           // NkGetDefaultAllocator
#include "NKMath/NkRectangle.h"                          // NkRect2i
#include "NKGui/NKGui.h"

namespace nkentseu {
    namespace renderer {

        class NkGuiCanvasBackend {
        public:
            ~NkGuiCanvasBackend() {
                auto& alloc = nkentseu::memory::NkGetDefaultAllocator();
                for (nkentseu::uint32 i = 0; i < mFonts.Size(); ++i)
                    if (mFonts[i].tex) alloc.Delete(mFonts[i].tex);
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

                // Cherche l'atlas de police existant pour ce texId (UI, code, ...).
                FontTex* ft = nullptr;
                for (uint32 i = 0; i < mFonts.Size(); ++i) if (mFonts[i].id == texId) { ft = &mFonts[i]; break; }

                // (Re)créer la texture si elle n'existe pas OU si la taille change (rechargement
                // de police à une autre taille = DPI). Sinon Update() déborderait l'ancienne taille.
                auto& alloc = memory::NkGetDefaultAllocator();
                if (!ft) {
                    mFonts.PushBack(FontTex{ texId, nullptr, 0, 0 });
                    ft = &mFonts[mFonts.Size() - 1u];
                }
                if (!ft->tex || ft->w != w || ft->h != h) {
                    if (ft->tex) { alloc.Delete(ft->tex); ft->tex = nullptr; }
                    ft->tex = alloc.New<renderer::NkTexture>();
                    if (!ft->tex) return false;
                    if (!ft->tex->Create(*mRenderer, static_cast<uint32>(w), static_cast<uint32>(h))) {
                        alloc.Delete(ft->tex); ft->tex = nullptr; return false;
                    }
                    ft->tex->SetFilter(renderer::NkTextureFilter::NK_LINEAR);
                    ft->w = w; ft->h = h;
                }
                return ft->tex->Update(mExpand.Data(), static_cast<uint32>(w), static_cast<uint32>(h), 0, 0);
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
                        for (uint32 fi = 0; fi < mFonts.Size(); ++fi)
                            if (mFonts[fi].id == dc.texId) { tex = mFonts[fi].tex; break; }
                        if (!tex) for (uint32 ti = 0; ti < mImages.Size(); ++ti)
                            if (mImages[ti].id == dc.texId) { tex = mImages[ti].tex; break; }
                    }

                    // Ne soumet que le SOUS-ENSEMBLE de vertices reference par cette
                    // commande (indices rebases). Indispensable : passer tout le buffer
                    // depasse kMaxVertices (65536) des qu'un draw list est gros -> crash.
                    uint32 lo = 0xFFFFFFFFu, hi = 0u;
                    for (uint32 k = 0; k < dc.idxCount; ++k) {
                        const uint32 v = dl.idx[dc.idxOffset + k];
                        if (v < lo) lo = v;
                        if (v > hi) hi = v;
                    }
                    mIdxTmp.Resize(dc.idxCount);
                    for (uint32 k = 0; k < dc.idxCount; ++k) mIdxTmp[k] = dl.idx[dc.idxOffset + k] - lo;
                    mRenderer->DrawVertices(mScratch.Data() + lo, hi - lo + 1u,
                                            mIdxTmp.Data(), dc.idxCount, tex);

                    if (hasClip) mRenderer->PopClip();
                }
            }

        private:
            struct ImgTex  { nkentseu::uint32 id; nkentseu::renderer::NkTexture* tex; };
            // Un atlas de police par texId (interface, code, ...). Plusieurs polices
            // = plusieurs textures resolues par Submit selon le texId de la commande.
            struct FontTex { nkentseu::uint32 id; nkentseu::renderer::NkTexture* tex; nkentseu::int32 w; nkentseu::int32 h; };
            nkentseu::renderer::NkIRenderer2D*                 mRenderer  = nullptr;
            nkentseu::NkVector<FontTex>                        mFonts;
            nkentseu::NkVector<ImgTex>                         mImages;
            nkentseu::NkVector<nkentseu::renderer::NkVertex2D> mScratch;
            nkentseu::NkVector<nkentseu::uint32>               mIdxTmp;   // indices rebases par commande
            nkentseu::NkVector<nkentseu::uint8>                mExpand;
        };

    } // namespace renderer
} // namespace nkentseu
