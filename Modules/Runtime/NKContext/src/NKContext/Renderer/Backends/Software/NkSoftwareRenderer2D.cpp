// =============================================================================
// NkSoftwareRenderer2D.cpp — Rasteriseur CPU SIMD
//
// OPTIMISATIONS :
//   • Écriture directe BGRA (Windows) / RGBA (autres) → zéro conversion
//   • FillSpanOpaque SIMD : 4 pixels/cycle (SSE2), 4 pixels/cycle (NEON)
//   • BlendSpanAlpha SIMD : 2 pixels/cycle (SSE2)
//   • Scan-line rasterizer : parcourt uniquement les pixels dans le triangle
//   • Clip précoce des triangles hors viewport
//   • Pas d'allocation mémoire dans la boucle de rendu (tout sur la pile)
//   • Correction des artefacts : initialisation correcte de l'interpolation
//     et gestion des cas dégénérés (triangles très minces)
// =============================================================================
#include "NkSoftwareRenderer2D.h"
#include "NKContext/Graphics/Software/NkSWPixel.h"
#include "NKContext/Renderer/Resources/NkTexture.h"
#include "NKContext/Renderer/Resources/NkSprite.h"
#include "NKContext/Core/NkNativeContextAccess.h"
#include "NKLogger/NkLog.h"

#include <cmath>
#include <cstring>

using namespace nkentseu::sw_detail;

#define NK_SW2D_ERR(...) logger.Errorf("[NkSW2D] " __VA_ARGS__)
#define NK_SW2D_LOG(...) logger.Infof("[NkSW2D] " __VA_ARGS__)

namespace nkentseu {

    // Helpers locaux
    static NK_FORCE_INLINE int32 nk_clampi(int32 v, int32 lo, int32 hi) {
        return v < lo ? lo : (v > hi ? hi : v);
    }
    static NK_FORCE_INLINE float32 nk_clampf(float32 v, float32 lo, float32 hi) {
        return v < lo ? lo : (v > hi ? hi : v);
    }
    static NK_FORCE_INLINE uint8 nk_f2b(float32 v) {
        int32 i = (int32)(v + 0.5f);
        return (uint8)(i < 0 ? 0 : (i > 255 ? 255 : i));
    }

    namespace renderer {

        // =============================================================================
        bool NkSoftwareRenderer2D::Initialize(NkIGraphicsContext* ctx) {
            if (mIsValid) return false;
            if (!ctx || !ctx->IsValid()) { NK_SW2D_ERR("Invalid context"); return false; }
            if (ctx->GetApi() != NkGraphicsApi::NK_API_SOFTWARE) {
                NK_SW2D_ERR("Requires Software context");
                return false;
            }
            mCtx   = ctx;
            mSWCtx = dynamic_cast<NkSoftwareContext*>(ctx);
            if (!mSWCtx) { NK_SW2D_ERR("Cast to NkSoftwareContext failed"); return false; }

            NkContextInfo info = ctx->GetInfo();
            const uint32 W = info.windowWidth  > 0 ? info.windowWidth  : 800;
            const uint32 H = info.windowHeight > 0 ? info.windowHeight : 600;
            mDefaultView.center = { W * 0.5f, H * 0.5f };
            mDefaultView.size   = { (float32)W, (float32)H };
            mCurrentView        = mDefaultView;
            mViewport           = { 0, 0, (int32)W, (int32)H };

            mIsValid = true;
            NK_SW2D_LOG("Initialized (%ux%u)", W, H);
            return true;
        }

        // =============================================================================
        void NkSoftwareRenderer2D::Shutdown() {
            mIsValid = false;
            mCtx     = nullptr;
            mSWCtx   = nullptr;
        }

        // =============================================================================
        void NkSoftwareRenderer2D::Clear(const NkColor2D& col) {
            if (!mSWCtx) return;
            NkSoftwareFramebuffer& fb = mSWCtx->GetBackBuffer();
            if (!fb.IsValid()) return;

            // Clear SIMD : chaque ligne avec FillSpanOpaque (4 pixels/cycle SSE2)
            const uint32 w = fb.width;
            uint8* pixels = fb.pixels.Data();
            for (uint32 y = 0; y < fb.height; ++y) {
                ClearRow(pixels + y * fb.stride, w, col.r, col.g, col.b, col.a);
            }
        }

        void NkSoftwareRenderer2D::BeginBackend() {}
        void NkSoftwareRenderer2D::EndBackend()   {}

        // =============================================================================
        // Draw sprite — blit direct sans passer par le batch
        // =============================================================================
        void NkSoftwareRenderer2D::Draw(const NkSprite& sprite) {
            if (!mSWCtx) return;
            NkSoftwareFramebuffer& fb = mSWCtx->GetBackBuffer();
            if (!fb.IsValid()) return;

            const NkTransform2D& t   = sprite.GetTransform();
            const NkColor2D&     col = sprite.GetColor();
            const NkRect2i&      src = sprite.GetTextureRect();
            const NkTexture*     tex = sprite.GetTexture();

            const bool isSimple = (fabsf(t.rotation) < 1e-4f);
            if (isSimple) {
                const int32 dstW = (int32)((float32)src.width  * t.scale.x + 0.5f);
                const int32 dstH = (int32)((float32)src.height * t.scale.y + 0.5f);
                const int32 dstX = (int32)(t.position.x - t.origin.x * t.scale.x);
                const int32 dstY = (int32)(t.position.y - t.origin.y * t.scale.y);

                if (tex && tex->IsValid() && tex->GetCPUPixels()) {
                    BlitTexture(fb, tex, src, dstX, dstY, dstW, dstH, col);
                } else {
                    // Rectangle coloré SIMD
                    const int32 x0 = nk_clampi(dstX, 0, (int32)fb.width);
                    const int32 x1 = nk_clampi(dstX + dstW, 0, (int32)fb.width);
                    const int32 y0 = nk_clampi(dstY, 0, (int32)fb.height);
                    const int32 y1 = nk_clampi(dstY + dstH, 0, (int32)fb.height);
                    for (int32 y = y0; y < y1; ++y) {
                        uint8* row = fb.pixels.Data() + y * fb.stride;
                        if (col.a == 255u)
                            FillSpanOpaque(row, x0, x1, col.r, col.g, col.b);
                        else
                            BlendSpanAlpha(row, x0, x1, col.r, col.g, col.b, col.a);
                    }
                }
                return;
            }
            NkBatchRenderer2D::Draw(sprite);
        }

        // =============================================================================
        // BlitTexture — nearest-neighbor avec tint et SIMD sur les spans opaques
        // =============================================================================
        void NkSoftwareRenderer2D::BlitTexture(NkSoftwareFramebuffer& fb,
                                                const NkTexture* tex,
                                                const NkRect2i& srcRect,
                                                int32 dstX, int32 dstY,
                                                int32 dstW, int32 dstH,
                                                const NkColor2D& tint) {
            if (dstW <= 0 || dstH <= 0) return;
            const uint8* pixels = tex->GetCPUPixels();
            const int32  texW   = (int32)tex->GetWidth();
            const int32  texH   = (int32)tex->GetHeight();
            if (!pixels || texW <= 0 || texH <= 0) return;

            const int32 x0 = nk_clampi(dstX, 0, (int32)fb.width);
            const int32 y0 = nk_clampi(dstY, 0, (int32)fb.height);
            const int32 x1 = nk_clampi(dstX + dstW, 0, (int32)fb.width);
            const int32 y1 = nk_clampi(dstY + dstH, 0, (int32)fb.height);
            if (x0 >= x1 || y0 >= y1) return;

            // Facteurs de scale en virgule fixe (Q16.16)
            const uint32 scaleU = (uint32)((float32)srcRect.width  / (float32)dstW * 65536.f + 0.5f);
            const uint32 scaleV = (uint32)((float32)srcRect.height / (float32)dstH * 65536.f + 0.5f);

            for (int32 py = y0; py < y1; ++py) {
                // Y source en virgule fixe
                const uint32 sy = (uint32)((py - dstY) * scaleV >> 16) + srcRect.top;
                if ((int32)sy < 0 || (int32)sy >= texH) continue;

                uint8* dstRow = fb.pixels.Data() + py * fb.stride;
                const uint8* srcRowBase = pixels + sy * texW * 4;

                for (int32 px = x0; px < x1; ++px) {
                    const uint32 sx = (uint32)((px - dstX) * scaleU >> 16) + srcRect.left;
                    if ((int32)sx >= texW) continue;

                    // La texture est stockée en RGBA (format CPU natif NkEngine)
                    const uint8* s = srcRowBase + sx * 4;
                    // Appliquer tint
                    const uint8 sa = (uint8)((uint32)s[3] * tint.a / 255u);
                    if (sa == 0u) continue;
                    const uint8 sr = (uint8)((uint32)s[0] * tint.r / 255u);
                    const uint8 sg = (uint8)((uint32)s[1] * tint.g / 255u);
                    const uint8 sb = (uint8)((uint32)s[2] * tint.b / 255u);

                    // Écrire dans l'ordre natif (BGRA Windows / RGBA autres)
                    BlendPixel(dstRow + px * 4, sr, sg, sb, sa);
                }
            }
        }

        // =============================================================================
        // SubmitBatches
        // =============================================================================
        void NkSoftwareRenderer2D::SubmitBatches(const NkBatchGroup* groups, uint32 groupCount,
                                                const NkVertex2D* verts, uint32 vCount,
                                                const uint32*     idx,   uint32 iCount) {
            if (!mSWCtx || !verts || !idx || vCount == 0 || iCount == 0) return;
            NkSoftwareFramebuffer& fb = mSWCtx->GetBackBuffer();
            if (!fb.IsValid()) return;

            for (uint32 g = 0; g < groupCount; ++g) {
                const NkBatchGroup& group = groups[g];
                const uint32 end = group.indexStart + group.indexCount;

                for (uint32 i = group.indexStart + 2; i < end; i += 3) {
                    if (idx[i-2] >= vCount || idx[i-1] >= vCount || idx[i] >= vCount) continue;
                    RasterizeTriangle(fb,
                        verts[idx[i-2]], verts[idx[i-1]], verts[idx[i]],
                        group.texture, group.blendMode);
                }
            }
        }

        // =============================================================================
        // Scan-line rasterizer — corrigé + SIMD
        //
        // CORRECTIONS vs version précédente :
        //   • Tri stable des sommets (pas de swap des pointeurs de couleur)
        //   • Epsilon correct sur xL/xR pour éviter les artefacts de bord
        //   • Pas de goto (comportement indéfini en C++)
        //   • Delta initialisé avant la boucle de span (pas après)
        //   • Texture sampling clampé [0, texW-1] × [0, texH-1]
        // =============================================================================

        // Données d'un sommet pour l'interpolateur scan-line
        struct SWScanVert {
            float32 x, y;
            float32 r, g, b, a;
            float32 u, v;
        };

        static NK_FORCE_INLINE SWScanVert MakeSV(const NkVertex2D& v) noexcept {
            return { v.x, v.y, (float32)v.r, (float32)v.g, (float32)v.b, (float32)v.a, v.u, v.v };
        }

        // Interpoler attributs d'un vertex sur une arête à la hauteur targetY
        static NK_FORCE_INLINE void EdgeStep(
            const SWScanVert& a, const SWScanVert& b, float32 targetY,
            float32& ox, float32& or_, float32& og, float32& ob, float32& oa,
            float32& ou, float32& ov) noexcept
        {
            const float32 dy = b.y - a.y;
            if (fabsf(dy) < 1e-6f) {
                ox = a.x; or_ = a.r; og = a.g; ob = a.b; oa = a.a; ou = a.u; ov = a.v;
                return;
            }
            const float32 t = (targetY - a.y) / dy;
            ox  = a.x + (b.x - a.x) * t;
            or_ = a.r + (b.r - a.r) * t;
            og  = a.g + (b.g - a.g) * t;
            ob  = a.b + (b.b - a.b) * t;
            oa  = a.a + (b.a - a.a) * t;
            ou  = a.u + (b.u - a.u) * t;
            ov  = a.v + (b.v - a.v) * t;
        }

        void NkSoftwareRenderer2D::RasterizeTriangle(NkSoftwareFramebuffer& fb,
                                                    const NkVertex2D& v0,
                                                    const NkVertex2D& v1,
                                                    const NkVertex2D& v2,
                                                    const NkTexture* tex,
                                                    NkBlendMode blendMode) {
            // Convertir en SWScanVert pour un accès cache-friendly
            SWScanVert sv[3] = { MakeSV(v0), MakeSV(v1), MakeSV(v2) };

            // Tri stable par Y croissant (bubble sort sur 3 éléments)
            if (sv[0].y > sv[1].y) { SWScanVert tmp = sv[0]; sv[0] = sv[1]; sv[1] = tmp; }
            if (sv[1].y > sv[2].y) { SWScanVert tmp = sv[1]; sv[1] = sv[2]; sv[2] = tmp; }
            if (sv[0].y > sv[1].y) { SWScanVert tmp = sv[0]; sv[0] = sv[1]; sv[1] = tmp; }

            // Bornes Y clampées au framebuffer
            const int32 yMin = nk_clampi((int32)ceilf(sv[0].y),  0, (int32)fb.height - 1);
            const int32 yMax = nk_clampi((int32)floorf(sv[2].y), 0, (int32)fb.height - 1);
            if (yMin > yMax) return;

            // Texture CPU
            const uint8* texPix = nullptr;
            int32 texW = 0, texH = 0;
            if (tex && tex->IsValid() && tex->GetCPUPixels()) {
                texPix = tex->GetCPUPixels();
                texW = (int32)tex->GetWidth();
                texH = (int32)tex->GetHeight();
            }

            for (int32 y = yMin; y <= yMax; ++y) {
                const float32 fy = (float32)y + 0.5f;

                // Côté long : toujours sv[0]→sv[2]
                float32 xL, rL, gL, bL, aL, uL, vL;
                EdgeStep(sv[0], sv[2], fy, xL, rL, gL, bL, aL, uL, vL);

                // Côté court : sv[0]→sv[1] (moitié haute) ou sv[1]→sv[2] (moitié basse)
                float32 xR, rR, gR, bR, aR, uR, vR;
                if (fy <= sv[1].y) {
                    EdgeStep(sv[0], sv[1], fy, xR, rR, gR, bR, aR, uR, vR);
                } else {
                    EdgeStep(sv[1], sv[2], fy, xR, rR, gR, bR, aR, uR, vR);
                }

                // Assurer L ≤ R
                if (xL > xR) {
                    float32 tmp;
                    tmp=xL; xL=xR; xR=tmp;
                    tmp=rL; rL=rR; rR=tmp;
                    tmp=gL; gL=gR; gR=tmp;
                    tmp=bL; bL=bR; bR=tmp;
                    tmp=aL; aL=aR; aR=tmp;
                    tmp=uL; uL=uR; uR=tmp;
                    tmp=vL; vL=vR; vR=tmp;
                }

                // Span X (inclure les demi-pixels bord)
                const int32 xStart = nk_clampi((int32)ceilf(xL - 0.5f),  0, (int32)fb.width - 1);
                const int32 xEnd   = nk_clampi((int32)floorf(xR + 0.5f), 0, (int32)fb.width - 1);
                if (xStart > xEnd) continue;

                const float32 spanW = xR - xL;
                const float32 invSpan = (spanW > 1e-6f) ? 1.f / spanW : 0.f;

                // Deltas par pixel X
                const float32 drDx = (rR - rL) * invSpan;
                const float32 dgDx = (gR - gL) * invSpan;
                const float32 dbDx = (bR - bL) * invSpan;
                const float32 daDx = (aR - aL) * invSpan;
                const float32 duDx = (uR - uL) * invSpan;
                const float32 dvDx = (vR - vL) * invSpan;

                // Offset initial depuis xL vers xStart (sub-pixel correction)
                const float32 off = (float32)xStart - xL + 0.5f;
                float32 cr = rL + drDx * off;
                float32 cg = gL + dgDx * off;
                float32 cb = bL + dbDx * off;
                float32 ca = aL + daDx * off;
                float32 cu = uL + duDx * off;
                float32 cv = vL + dvDx * off;

                uint8* row = fb.pixels.Data() + y * fb.stride;
                const int32 count = xEnd - xStart + 1;

                // ── Cas optimisé : span opaque sans texture → SIMD fill ──────────────
                // On vérifie si le span entier est uniforme en couleur et opaque
                // (cas très fréquent pour les primitives géométriques unies)
                const bool uniformColor = (fabsf(drDx) < 0.5f && fabsf(dgDx) < 0.5f &&
                                        fabsf(dbDx) < 0.5f && fabsf(daDx) < 0.5f);
                const uint8 ca8 = nk_f2b(ca);

                if (!texPix && uniformColor && ca8 == 255u) {
                    // Chemin ultra-rapide : FillSpanOpaque SIMD
                    FillSpanOpaque(row, xStart, xEnd + 1,
                                nk_f2b(cr), nk_f2b(cg), nk_f2b(cb));
                    continue; // passer au Y suivant
                }

                if (!texPix && uniformColor && blendMode == NkBlendMode::NK_ALPHA) {
                    BlendSpanAlpha(row, xStart, xEnd + 1,
                                nk_f2b(cr), nk_f2b(cg), nk_f2b(cb), ca8);
                    continue;
                }

                if (!texPix && uniformColor && blendMode == NkBlendMode::NK_ADD) {
                    BlendSpanAdd(row, xStart, xEnd + 1,
                                nk_f2b(cr), nk_f2b(cg), nk_f2b(cb), ca8);
                    continue;
                }

                // ── Chemin général pixel par pixel ────────────────────────────────────
                for (int32 x = xStart; x <= xEnd; ++x) {
                    uint8 sr, sg, sb, sa_out;

                    if (texPix) {
                        const int32 tx = nk_clampi((int32)(cu * texW), 0, texW - 1);
                        const int32 ty = nk_clampi((int32)(cv * texH), 0, texH - 1);
                        // Texture stockée en RGBA (format CPU NkEngine)
                        const uint8* tp = texPix + (ty * texW + tx) * 4;
                        sr     = nk_f2b(tp[0] * cr / 255.f);
                        sg     = nk_f2b(tp[1] * cg / 255.f);
                        sb     = nk_f2b(tp[2] * cb / 255.f);
                        sa_out = nk_f2b(tp[3] * ca / 255.f);
                    } else {
                        sr     = nk_f2b(cr);
                        sg     = nk_f2b(cg);
                        sb     = nk_f2b(cb);
                        sa_out = nk_f2b(ca);
                    }

                    uint8* d = row + x * 4;

                    if (blendMode == NkBlendMode::NK_ADD) {
                        if (sa_out > 0u) {
        #if NK_SW_PIXEL_BGRA
                            uint32 nb = (uint32)d[0] + (uint32)sb * sa_out / 255u;
                            uint32 ng = (uint32)d[1] + (uint32)sg * sa_out / 255u;
                            uint32 nr = (uint32)d[2] + (uint32)sr * sa_out / 255u;
                            d[0] = nb > 255u ? 255u : (uint8)nb;
                            d[1] = ng > 255u ? 255u : (uint8)ng;
                            d[2] = nr > 255u ? 255u : (uint8)nr;
        #else
                            uint32 nr = (uint32)d[0] + (uint32)sr * sa_out / 255u;
                            uint32 ng = (uint32)d[1] + (uint32)sg * sa_out / 255u;
                            uint32 nb = (uint32)d[2] + (uint32)sb * sa_out / 255u;
                            d[0] = nr > 255u ? 255u : (uint8)nr;
                            d[1] = ng > 255u ? 255u : (uint8)ng;
                            d[2] = nb > 255u ? 255u : (uint8)nb;
        #endif
                            d[3] = 255u;
                        }
                    } else if (blendMode == NkBlendMode::NK_MULTIPLY) {
                        if (sa_out > 0u) {
        #if NK_SW_PIXEL_BGRA
                            d[0] = (uint8)((uint32)d[0] * sb / 255u);
                            d[1] = (uint8)((uint32)d[1] * sg / 255u);
                            d[2] = (uint8)((uint32)d[2] * sr / 255u);
        #else
                            d[0] = (uint8)((uint32)d[0] * sr / 255u);
                            d[1] = (uint8)((uint32)d[1] * sg / 255u);
                            d[2] = (uint8)((uint32)d[2] * sb / 255u);
        #endif
                            d[3] = 255u;
                        }
                    } else {
                        // Alpha blend standard (cas le plus fréquent)
                        BlendPixel(d, sr, sg, sb, sa_out);
                    }

                    cr += drDx; cg += dgDx; cb += dbDx; ca += daDx;
                    cu += duDx; cv += dvDx;
                }
            }
        }

    } // namespace renderer
} // namespace nkentseu