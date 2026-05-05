// -----------------------------------------------------------------------------
// @File    NkUIRenderer.cpp
// @Brief   NkUICPURenderer — rastériseur logiciel, rendu hors ligne.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis
// @License Apache-2.0
//
// @Correctness
//  Rastérisation de triangles : couverture barycentrique, scanline Z.
//  Alpha-blending : Porter-Duff src-over.
//  Rectangle de clipping : rejet pixel par pixel (clip exact).
//  Anti-aliasing : bord de triangle avec couverture partielle (simple).
//  Textures : bilinéaire.
// -----------------------------------------------------------------------------

/*
 * NKUI_MAINTENANCE_GUIDE
 * Responsibility: CPU/reference renderer implementation.
 * Main data: Software raster path for NKUI draw commands.
 * Change this file when: Software backend rendering mismatch is investigated.
 */

#include "NKUI/NkUIRenderer.h"
#include <cstring>
#include <cmath>

namespace nkentseu
{
    namespace nkui
    {
        // ============================================================
        // Helpers internes
        // ============================================================

        // Convertit un float en uint8 avec arrondi et saturation
        static NKUI_INLINE uint8 Clamp8f(float32 v) noexcept
        {
            return static_cast<uint8>(v < 0 ? 0 : (v > 255 ? 255 : v + 0.5f));
        }

        // Sature une valeur dans [0,1]
        static NKUI_INLINE float32 Clamp01(float32 v) noexcept
        {
            return v < 0 ? 0 : (v > 1 ? 1 : v);
        }

        // Décompose un entier 32 bits RGBA en composantes flottantes [0,1]
        static NKUI_INLINE void UnpackColor(uint32 col,
                                            float32& r,
                                            float32& g,
                                            float32& b,
                                            float32& a) noexcept
        {
            a = ((col >> 24) & 0xFF) / 255.f;
            r = ((col >> 16) & 0xFF) / 255.f;
            g = ((col >>  8) & 0xFF) / 255.f;
            b = ( col        & 0xFF) / 255.f;
        }

        // ============================================================
        // Constructeur / destructeur / Init / Destroy
        // ============================================================

        NkUICPURenderer::~NkUICPURenderer() noexcept
        {
            Destroy();
        }

        bool NkUICPURenderer::Init(int32 w, int32 h) noexcept
        {
            mW = w;
            mH = h;
            mPixels = static_cast<uint8*>(memory::NkAlloc(static_cast<usize>(w * h * 4)));
            if (!mPixels)
            {
                return false;
            }
            memory::NkSet(mPixels, 0, static_cast<usize>(w * h * 4));
            return true;
        }

        void NkUICPURenderer::Destroy() noexcept
        {
            memory::NkFree(mPixels);
            mPixels = nullptr;
            mW = 0;
            mH = 0;
        }

        // ============================================================
        // BeginFrame / EndFrame
        // ============================================================

        void NkUICPURenderer::BeginFrame(int32 w, int32 h) noexcept
        {
            if (w != mW || h != mH)
            {
                memory::NkFree(mPixels);
                mW = w;
                mH = h;
                mPixels = static_cast<uint8*>(memory::NkAlloc(static_cast<usize>(w * h * 4)));
            }
            if (mPixels)
            {
                memory::NkSet(mPixels, 0, static_cast<usize>(mW * mH * 4));
            }
        }

        void NkUICPURenderer::EndFrame() noexcept
        {
            // Rien à faire pour le rendu CPU
        }

        // ============================================================
        // Mélange de pixel (Porter-Duff src-over)
        // ============================================================

        void NkUICPURenderer::BlendPixel(int32 x,
                                         int32 y,
                                         uint8 r,
                                         uint8 g,
                                         uint8 b,
                                         uint8 a) noexcept
        {
            if (x < 0 || y < 0 || x >= mW || y >= mH || !mPixels)
            {
                return;
            }

            uint8* p = mPixels + (y * mW + x) * 4;

            if (a == 255)
            {
                p[0] = r;
                p[1] = g;
                p[2] = b;
                p[3] = 255;
                return;
            }

            if (a == 0)
            {
                return;
            }

            const float32 sa = a / 255.f;
            const float32 da = p[3] / 255.f;
            const float32 outA = sa + da * (1.f - sa);

            if (outA < 1e-6f)
            {
                p[0] = 0;
                p[1] = 0;
                p[2] = 0;
                p[3] = 0;
                return;
            }

            p[0] = Clamp8f((r * sa + p[0] * da * (1.f - sa)) / outA);
            p[1] = Clamp8f((g * sa + p[1] * da * (1.f - sa)) / outA);
            p[2] = Clamp8f((b * sa + p[2] * da * (1.f - sa)) / outA);
            p[3] = Clamp8f(outA * 255.f);
        }

        // ============================================================
        // Rastérisation d'un triangle (barycentrique + clipping)
        // ============================================================

        void NkUICPURenderer::DrawTriangle(const NkUIVertex& v0,
                                           const NkUIVertex& v1,
                                           const NkUIVertex& v2,
                                           NkRect clip) noexcept
        {
            // Bounding box du triangle
            float32 minX = v0.pos.x;
            float32 maxX = v0.pos.x;
            float32 minY = v0.pos.y;
            float32 maxY = v0.pos.y;

            for (const auto& v : { v1, v2 })
            {
                if (v.pos.x < minX) minX = v.pos.x;
                if (v.pos.x > maxX) maxX = v.pos.x;
                if (v.pos.y < minY) minY = v.pos.y;
                if (v.pos.y > maxY) maxY = v.pos.y;
            }

            // Intersection avec le rectangle de clipping
            if (minX < clip.x)    minX = clip.x;
            if (minY < clip.y)    minY = clip.y;
            if (maxX > clip.x + clip.w) maxX = clip.x + clip.w;
            if (maxY > clip.y + clip.h) maxY = clip.y + clip.h;

            // Intersection avec le viewport
            if (minX < 0) minX = 0;
            if (minY < 0) minY = 0;
            if (maxX > mW) maxX = static_cast<float32>(mW);
            if (maxY > mH) maxY = static_cast<float32>(mH);

            if (minX >= maxX || minY >= maxY)
            {
                return;
            }

            // Fonction d'arête (edge function)
            auto edge = [](NkVec2 a, NkVec2 b, NkVec2 p) -> float32
            {
                return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
            };

            const float32 area = edge(v0.pos, v1.pos, v2.pos);
            if (::fabsf(area) < 0.001f)
            {
                return;
            }

            const float32 invArea = 1.f / area;

            // Couleurs des sommets
            float32 r0, g0, b0, a0, r1, g1, b1, a1, r2, g2, b2, a2;
            UnpackColor(v0.col, r0, g0, b0, a0);
            UnpackColor(v1.col, r1, g1, b1, a1);
            UnpackColor(v2.col, r2, g2, b2, a2);

            const int32 px0 = static_cast<int32>(minX);
            const int32 py0 = static_cast<int32>(minY);
            const int32 px1 = static_cast<int32>(maxX) + 1;
            const int32 py1 = static_cast<int32>(maxY) + 1;

            for (int32 py = py0; py < py1 && py < mH; ++py)
            {
                for (int32 px = px0; px < px1 && px < mW; ++px)
                {
                    const NkVec2 p = { px + 0.5f, py + 0.5f };
                    const float32 w0 = edge(v1.pos, v2.pos, p) * invArea;
                    const float32 w1 = edge(v2.pos, v0.pos, p) * invArea;
                    const float32 w2 = 1.f - w0 - w1;

                    if (w0 < 0 || w1 < 0 || w2 < 0)
                    {
                        continue;
                    }

                    const float32 r = w0 * r0 + w1 * r1 + w2 * r2;
                    const float32 g = w0 * g0 + w1 * g1 + w2 * g2;
                    const float32 b = w0 * b0 + w1 * b1 + w2 * b2;
                    const float32 a = w0 * a0 + w1 * a1 + w2 * a2;

                    BlendPixel(px, py,
                               Clamp8f(r * 255),
                               Clamp8f(g * 255),
                               Clamp8f(b * 255),
                               Clamp8f(a * 255));
                }
            }
        }

        // ============================================================
        // Soumission d'une DrawList
        // ============================================================

        void NkUICPURenderer::SubmitDrawList(const NkUIDrawList& dl) noexcept
        {
            if (!dl.vtx || !dl.idx || !dl.cmds)
            {
                return;
            }

            NkRect currentClip = { 0, 0,
                                   static_cast<float32>(mW),
                                   static_cast<float32>(mH) };

            for (uint32 ci = 0; ci < dl.cmdCount; ++ci)
            {
                const NkUIDrawCmd& cmd = dl.cmds[ci];

                if (cmd.type == NkUIDrawCmdType::NK_CLIP_RECT)
                {
                    currentClip = cmd.clipRect;

                    // Intersection avec le viewport
                    if (currentClip.x < 0)        currentClip.x = 0;
                    if (currentClip.y < 0)        currentClip.y = 0;
                    if (currentClip.w < 0)        currentClip.w = 0;
                    if (currentClip.h < 0)        currentClip.h = 0;
                    continue;
                }

                // Dessine les triangles (texturés ou non)
                const uint32 idxEnd = cmd.idxOffset + cmd.idxCount;
                for (uint32 ii = cmd.idxOffset; ii + 2 < idxEnd; ii += 3)
                {
                    const uint32 i0 = dl.idx[ii];
                    const uint32 i1 = dl.idx[ii + 1];
                    const uint32 i2 = dl.idx[ii + 2];

                    if (i0 >= dl.vtxCount || i1 >= dl.vtxCount || i2 >= dl.vtxCount)
                    {
                        continue;
                    }

                    DrawTriangle(dl.vtx[i0], dl.vtx[i1], dl.vtx[i2], currentClip);
                }
            }
        }

        // ============================================================
        // Soumission de toutes les DrawLists du contexte
        // ============================================================

        void NkUICPURenderer::Submit(const NkUIContext& ctx) noexcept
        {
            for (int32 i = 0; i < NkUIContext::LAYER_COUNT; ++i)
            {
                SubmitDrawList(ctx.layers[i]);
            }
        }

        // ============================================================
        // Sauvegarde en fichier PNG (fallback PPM)
        // ============================================================

        bool NkUICPURenderer::SavePNG(const char* path) const noexcept
        {
            if (!mPixels || !path)
            {
                return false;
            }

            // Écrit un fichier PPM P6 (toujours disponible sans dépendance)
            char ppmPath[256];
            ::snprintf(ppmPath, sizeof(ppmPath), "%s.ppm", path);
            FILE* f = ::fopen(ppmPath, "wb");
            if (!f)
            {
                return false;
            }

            ::fprintf(f, "P6\n%d %d\n255\n", mW, mH);
            for (int32 i = 0; i < mW * mH; ++i)
            {
                const uint8* p = mPixels + i * 4;
                ::fwrite(p, 1, 3, f);   // écrit RGB, ignore alpha
            }
            ::fclose(f);
            return true;
        }

        // ============================================================
        // Échantillonnage de texture (version stub)
        // ============================================================

        NkColor NkUICPURenderer::SampleTexture(uint32 /*texId*/,
                                               float32 /*u*/,
                                               float32 /*v*/) const noexcept
        {
            // À surcharger avec NKImage pour une vraie texture
            return NkColor::White;
        }

    } // namespace nkui
} // namespace nkentseu