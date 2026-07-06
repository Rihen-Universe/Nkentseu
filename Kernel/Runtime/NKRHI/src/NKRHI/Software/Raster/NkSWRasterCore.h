#pragma once
// =============================================================================
// NkSWRasterCore.h — Cœur de rasterisation logiciel (réécriture v4)
//
// Remplace le scanline affine de NkSWFastPath (swfast::DrawTriangleFast) par un
// rasterizer edge-function / barycentrique :
//   1. Clipping near-plane en clip-space (Sutherland–Hodgman) AVANT la division
//      perspective — corrige l'effondrement des sommets w<=0 à l'origine.
//   2. Interpolation PERSPECTIVE-CORRECTE des varyings (1/w) — corrige la
//      déformation des UV/couleurs en perspective.
//   3. Respect de l'état pipeline : depthOp configurable, blend factors, culling.
//
// Références Scratchapixel (archive locale D:\Scratchapixel\markdown) :
//   - rasterization-practical-implementation/rasterization-stage
//   - rasterization-practical-implementation/perspective-correct-interpolation-vertex-attributes
//   - rasterization-practical-implementation/visibility-problem-depth-buffer-depth-interpolation
//   - perspective-and-orthographic-projection-matrix/projection-matrix-GPU-rendering-pipeline-clipping
//
// Périmètre : uniquement src/NKRHI/Software/. N'inclut aucune interface partagée
// autre que celles déjà utilisées par le backend software.
// =============================================================================

#include "NKRHI/Software/NkSoftwareDevice.h"
#include "NKRHI/Software/NkSWPixel.h"
#include <cmath>
#include <cstring>

namespace nkentseu {
    namespace swraster {

        // =============================================================================
        // État de rendu extrait du pipeline (thread-safe, copié par valeur)
        // =============================================================================
        // Profondeur écrite par le fragment (shader->fragDepth). Le fragFn pose tl_fragDepth
        // (∈[0,1]) et tl_fragDepthSet=true ; le rasterizer l'utilise pour le depth-test/write.
        inline thread_local float tl_fragDepth    = 0.f;
        inline thread_local bool  tl_fragDepthSet = false;

        struct NkSWRenderState {
            bool          depthTest   = true;
            bool          depthWrite  = true;
            NkCompareOp   depthOp     = NkCompareOp::NK_LESS;
            NkCullMode    cullMode    = NkCullMode::NK_NONE;   // NK_NONE tant que le pipeline force NK_NONE
            NkFrontFace   frontFace   = NkFrontFace::NK_CCW;
            bool          blendEnable = false;
            NkBlendFactor srcColor    = NkBlendFactor::NK_SRC_ALPHA;
            NkBlendFactor dstColor    = NkBlendFactor::NK_ONE_MINUS_SRC_ALPHA;
        };

        // Batch de textures (aligné sur swfast::NkSWTextureBatch : tex[]+count)
        struct NkSWTexBatch {
            const NkSWTexture* tex[8] = {};
            uint32 count = 0;
        };

        // =============================================================================
        // Sommet en clip-space portant toutes les varyings (pour le clipping)
        // =============================================================================
        struct ClipVert {
            math::NkVec4 pos;      // clip-space (x,y,z,w) — AVANT division
            math::NkVec4 color;
            math::NkVec2 uv;
            math::NkVec3 normal;
            float32      attrs[16] = {};
            uint32       attrCount = 0;

            static ClipVert From(const NkVertexSoftware& v) {
                ClipVert c;
                c.pos = v.position; c.color = v.color; c.uv = v.uv; c.normal = v.normal;
                c.attrCount = v.attrCount < 16 ? v.attrCount : 16;
                for (uint32 i = 0; i < c.attrCount; ++i) c.attrs[i] = v.attrs[i];
                return c;
            }
            NkVertexSoftware ToVertex() const {
                NkVertexSoftware v{};
                v.position = pos; v.color = color; v.uv = uv; v.normal = normal;
                v.attrCount = attrCount;
                for (uint32 i = 0; i < attrCount; ++i) v.attrs[i] = attrs[i];
                return v;
            }
        };

        // Interpolation linéaire de deux sommets clip-space (utilisé par le clipping)
        static inline ClipVert LerpClip(const ClipVert& a, const ClipVert& b, float32 t) {
            ClipVert r;
            r.pos.x = a.pos.x + (b.pos.x - a.pos.x) * t;
            r.pos.y = a.pos.y + (b.pos.y - a.pos.y) * t;
            r.pos.z = a.pos.z + (b.pos.z - a.pos.z) * t;
            r.pos.w = a.pos.w + (b.pos.w - a.pos.w) * t;
            r.color.x = a.color.x + (b.color.x - a.color.x) * t;
            r.color.y = a.color.y + (b.color.y - a.color.y) * t;
            r.color.z = a.color.z + (b.color.z - a.color.z) * t;
            r.color.w = a.color.w + (b.color.w - a.color.w) * t;
            r.uv.x = a.uv.x + (b.uv.x - a.uv.x) * t;
            r.uv.y = a.uv.y + (b.uv.y - a.uv.y) * t;
            r.normal.x = a.normal.x + (b.normal.x - a.normal.x) * t;
            r.normal.y = a.normal.y + (b.normal.y - a.normal.y) * t;
            r.normal.z = a.normal.z + (b.normal.z - a.normal.z) * t;
            r.attrCount = a.attrCount > b.attrCount ? a.attrCount : b.attrCount;
            for (uint32 i = 0; i < r.attrCount; ++i) r.attrs[i] = a.attrs[i] + (b.attrs[i] - a.attrs[i]) * t;
            return r;
        }

        // =============================================================================
        // Clipping near-plane en clip-space : demi-espace intérieur  z + w >= 0
        // (near plane pour NDC z ∈ [-1,1], cohérent avec le mapping sz = z/w*0.5+0.5)
        // Sutherland–Hodgman : entrée = triangle (3), sortie = polygone convexe (≤4).
        // =============================================================================
        static inline float32 NearDist(const ClipVert& v) { return v.pos.z + v.pos.w; }

        static uint32 ClipNear(const ClipVert in[3], ClipVert out[4]) {
            const float32 kEps = 1e-6f;
            uint32 n = 0;
            for (uint32 i = 0; i < 3; ++i) {
                const ClipVert& cur = in[i];
                const ClipVert& nxt = in[(i + 1) % 3];
                const float32 dCur = NearDist(cur);
                const float32 dNxt = NearDist(nxt);
                const bool inCur = dCur >= 0.f;
                const bool inNxt = dNxt >= 0.f;
                if (inCur) { if (n < 4) out[n++] = cur; }
                if (inCur != inNxt) {
                    const float32 denom = dCur - dNxt;
                    const float32 t = (fabsf(denom) > kEps) ? dCur / denom : 0.f;
                    if (n < 4) out[n++] = LerpClip(cur, nxt, t);
                }
            }
            return n; // 0, 3 ou 4
        }

        // =============================================================================
        // Comparaison de profondeur selon depthOp
        // =============================================================================
        static inline bool DepthPass(NkCompareOp op, float32 zn, float32 zd) {
            switch (op) {
                case NkCompareOp::NK_NEVER:         return false;
                case NkCompareOp::NK_LESS:          return zn <  zd;
                case NkCompareOp::NK_EQUAL:         return zn == zd;
                case NkCompareOp::NK_LESS_EQUAL:    return zn <= zd;
                case NkCompareOp::NK_GREATER:       return zn >  zd;
                case NkCompareOp::NK_NOT_EQUAL:     return zn != zd;
                case NkCompareOp::NK_GREATER_EQUAL: return zn >= zd;
                case NkCompareOp::NK_ALWAYS:        return true;
                default:                            return zn < zd;
            }
        }

        // =============================================================================
        // Blend factors configurables → multiplicateur (r,g,b,a) pour src ou dst
        // s* = couleur source (0..1), d* = couleur destination (0..1)
        // =============================================================================
        static inline void BlendFactor(NkBlendFactor f,
                                       float32 sr, float32 sg, float32 sb, float32 sa,
                                       float32 dr, float32 dg, float32 db, float32 da,
                                       float32& fr, float32& fg, float32& fb, float32& fa) {
            switch (f) {
                case NkBlendFactor::NK_ZERO:                 fr=fg=fb=fa=0.f; break;
                case NkBlendFactor::NK_ONE:                  fr=fg=fb=fa=1.f; break;
                case NkBlendFactor::NK_SRC_COLOR:            fr=sr; fg=sg; fb=sb; fa=sa; break;
                case NkBlendFactor::NK_ONE_MINUS_SRC_COLOR:  fr=1-sr; fg=1-sg; fb=1-sb; fa=1-sa; break;
                case NkBlendFactor::NK_DST_COLOR:            fr=dr; fg=dg; fb=db; fa=da; break;
                case NkBlendFactor::NK_ONE_MINUS_DST_COLOR:  fr=1-dr; fg=1-dg; fb=1-db; fa=1-da; break;
                case NkBlendFactor::NK_SRC_ALPHA:            fr=fg=fb=fa=sa; break;
                case NkBlendFactor::NK_ONE_MINUS_SRC_ALPHA:  fr=fg=fb=fa=1-sa; break;
                case NkBlendFactor::NK_DST_ALPHA:            fr=fg=fb=fa=da; break;
                case NkBlendFactor::NK_ONE_MINUS_DST_ALPHA:  fr=fg=fb=fa=1-da; break;
                case NkBlendFactor::NK_SRC_ALPHA_SATURATE: { float32 s=sa<(1-da)?sa:(1-da); fr=fg=fb=s; fa=1.f; } break;
                default:                                     fr=fg=fb=fa=1.f; break;
            }
        }

        static inline float32 Clamp01(float32 v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }

        // Écrit un pixel (src en 0..1) selon l'état de blend
        static inline void OutputPixel(uint8* p, const NkSWRenderState& st,
                                       float32 sr, float32 sg, float32 sb, float32 sa) {
            if (st.blendEnable) {
                uint8 dr8, dg8, db8, da8;
                sw_detail::LoadPixel(p, dr8, dg8, db8, da8);
                const float32 dr = dr8 / 255.f, dg = dg8 / 255.f, db = db8 / 255.f, da = da8 / 255.f;
                float32 sfr, sfg, sfb, sfa, dfr, dfg, dfb, dfa;
                BlendFactor(st.srcColor, sr, sg, sb, sa, dr, dg, db, da, sfr, sfg, sfb, sfa);
                BlendFactor(st.dstColor, sr, sg, sb, sa, dr, dg, db, da, dfr, dfg, dfb, dfa);
                const float32 orr = Clamp01(sr * sfr + dr * dfr);
                const float32 og  = Clamp01(sg * sfg + dg * dfg);
                const float32 ob  = Clamp01(sb * sfb + db * dfb);
                const float32 oa  = Clamp01(sa * sfa + da * dfa);
                sw_detail::StorePixel(p, (uint8)(orr*255.f+.5f), (uint8)(og*255.f+.5f),
                                          (uint8)(ob*255.f+.5f), (uint8)(oa*255.f+.5f));
            } else {
                // Comportement historique préservé : opaque → store, sinon alpha-over
                const uint8 r8=(uint8)(Clamp01(sr)*255.f+.5f), g8=(uint8)(Clamp01(sg)*255.f+.5f);
                const uint8 b8=(uint8)(Clamp01(sb)*255.f+.5f), a8=(uint8)(Clamp01(sa)*255.f+.5f);
                if (a8 == 255u)     sw_detail::StorePixel(p, r8, g8, b8, a8);
                else if (a8 > 0u)   sw_detail::BlendPixel(p, r8, g8, b8, a8);
            }
        }

        // =============================================================================
        // Sommet projeté en screen-space (après division perspective)
        // =============================================================================
        // Auto-suffisant (varyings embarquées) → stockable dans un buffer de triangles
        // pré-projetés, sans pointeur vers un ClipVert transitoire (cf. binning tuilé).
        struct ScreenVert {
            float32 sx, sy, sz;   // screen x,y ; z ∈ [0,1] (depth buffer)
            float32 invW;         // 1/w (pour interpolation perspective-correcte)
            math::NkVec4 color;
            math::NkVec2 uv;
            math::NkVec3 normal;
            float32 attrs[16];
            uint32  attrCount;
        };

        // Viewport optionnel (w==0 => plein RT). Sous-rect = slots du shadow atlas.
        struct NkSWViewport { float32 x=0,y=0,w=0,h=0; bool flipY=true; };

        static inline ScreenVert Project(const ClipVert& v, float32 W, float32 H, const NkSWViewport* vp=nullptr) {
            ScreenVert s;
            // w≈0 (sommet quasi sur le plan near / à l'infini) : NE PAS mettre invW=0 (cela
            // projetterait au CENTRE de l'écran -> grands plans qui "rayonnent depuis le centre").
            // On utilise un invW de grande magnitude, signe préservé, pour l'envoyer hors-champ.
            const float32 aw = fabsf(v.pos.w);
            const float32 invW = (aw > 1e-6f) ? (1.f / v.pos.w)
                                              : ((v.pos.w >= 0.f) ? 1e6f : -1e6f);
            const float32 ndcx = v.pos.x * invW, ndcy = v.pos.y * invW;
            if (vp && vp->w > 0.f) {   // viewport sous-rect (NDC -> rect du viewport)
                s.sx = vp->x + (ndcx*0.5f + 0.5f) * vp->w;
                s.sy = vp->flipY ? (vp->y + (1.f - ndcy)*0.5f * vp->h)
                                 : (vp->y + (ndcy*0.5f + 0.5f) * vp->h);
            } else {                   // plein RT (comportement historique)
                s.sx = (ndcx + 1.f) * 0.5f * W;
                s.sy = (1.f - ndcy) * 0.5f * H;
            }
            s.sz = v.pos.z * invW * 0.5f + 0.5f;
            s.invW = invW;
            s.color = v.color; s.uv = v.uv; s.normal = v.normal;
            s.attrCount = v.attrCount < 16 ? v.attrCount : 16;
            for (uint32 i = 0; i < s.attrCount; ++i) s.attrs[i] = v.attrs[i];
            return s;
        }

        static inline float32 Orient2D(float32 ax, float32 ay, float32 bx, float32 by, float32 cx, float32 cy) {
            return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
        }

        // =============================================================================
        // Échantillonnage texture (Phase 2)
        //   Réf. Scratchapixel : introduction-to-texturing/introduction-to-texturing-texture-filtering
        //                        mathematics-physics-for-computer-graphics/interpolation/bilinear-filtering
        // =============================================================================
        static inline void ReadTexel(const uint8* d, int32 texW, int32 texH, uint32 bpp,
                                     int32 x, int32 y,
                                     float32& r, float32& g, float32& b, float32& a) {
            x = x < 0 ? 0 : (x >= texW ? texW - 1 : x);          // clamp-to-edge
            y = y < 0 ? 0 : (y >= texH ? texH - 1 : y);
            const uint8* tp = d + ((uint32)y * texW + (uint32)x) * bpp;
            if (bpp >= 4)      { r=tp[0]/255.f; g=tp[1]/255.f; b=tp[2]/255.f; a=tp[3]/255.f; }
            else if (bpp == 3) { r=tp[0]/255.f; g=tp[1]/255.f; b=tp[2]/255.f; a=1.f; }
            else               { r=g=b=tp[0]/255.f; a=1.f; }
        }

        static inline void SampleNearest(const uint8* d, int32 texW, int32 texH, uint32 bpp,
                                         float32 u, float32 v,
                                         float32& r, float32& g, float32& b, float32& a) {
            ReadTexel(d, texW, texH, bpp, (int32)(u * texW), (int32)(v * texH), r, g, b, a);
        }

        // Bilinéaire, convention centre-texel (u*W-0.5), clamp-to-edge
        static inline void SampleBilinear(const uint8* d, int32 texW, int32 texH, uint32 bpp,
                                          float32 u, float32 v,
                                          float32& r, float32& g, float32& b, float32& a) {
            const float32 fx = u * texW - 0.5f, fy = v * texH - 0.5f;
            const int32 x0 = (int32)floorf(fx), y0 = (int32)floorf(fy);
            const float32 dx = fx - (float32)x0, dy = fy - (float32)y0;
            float32 r00,g00,b00,a00, r10,g10,b10,a10, r01,g01,b01,a01, r11,g11,b11,a11;
            ReadTexel(d, texW, texH, bpp, x0,   y0,   r00,g00,b00,a00);
            ReadTexel(d, texW, texH, bpp, x0+1, y0,   r10,g10,b10,a10);
            ReadTexel(d, texW, texH, bpp, x0,   y0+1, r01,g01,b01,a01);
            ReadTexel(d, texW, texH, bpp, x0+1, y0+1, r11,g11,b11,a11);
            const float32 w00=(1-dx)*(1-dy), w10=dx*(1-dy), w01=(1-dx)*dy, w11=dx*dy;
            r = r00*w00 + r10*w10 + r01*w01 + r11*w11;
            g = g00*w00 + g10*w10 + g01*w01 + g11*w11;
            b = b00*w00 + b10*w10 + b01*w01 + b11*w11;
            a = a00*w00 + a10*w10 + a01*w01 + a11*w11;
        }

        // =============================================================================
        // Rasterisation d'UN triangle screen-space (barycentrique, perspective-correct)
        // =============================================================================
        static void RasterScreenTriangle(
            const ScreenVert& A, const ScreenVert& B, const ScreenVert& C,
            uint8* colorBuf, float32* depthBuf, uint32 W, uint32 H,
            int32 clipX0, int32 clipY0, int32 clipX1, int32 clipY1,
            const NkSWShader* shader, const void* uniformData, const void* texPtr,
            const NkSWTexBatch* texBatch, const NkSWRenderState& st)
        {
            float32 area = Orient2D(A.sx, A.sy, B.sx, B.sy, C.sx, C.sy);
            if (fabsf(area) < 1e-6f) return; // dégénéré

            // Culling par winding (area>0 : CCW en screen y-down)
            if (st.cullMode != NkCullMode::NK_NONE) {
                // frontFace CCW ⇒ face avant a NDC-CCW ⇒ screen (y-down) CW ⇒ area<0
                const bool frontIsNegArea = (st.frontFace == NkFrontFace::NK_CCW);
                const bool isFront = frontIsNegArea ? (area < 0.f) : (area > 0.f);
                if (st.cullMode == NkCullMode::NK_BACK  && !isFront) return;
                if (st.cullMode == NkCullMode::NK_FRONT &&  isFront) return;
            }

            // Normaliser pour area>0 afin d'unifier le test barycentrique
            const ScreenVert* v0 = &A; const ScreenVert* v1 = &B; const ScreenVert* v2 = &C;
            if (area < 0.f) { const ScreenVert* t = v1; v1 = v2; v2 = t; area = -area; }
            const float32 invArea = 1.f / area;

            // Bounding box clampée au clip rect
            float32 fMinX = v0->sx < v1->sx ? (v0->sx < v2->sx ? v0->sx : v2->sx) : (v1->sx < v2->sx ? v1->sx : v2->sx);
            float32 fMaxX = v0->sx > v1->sx ? (v0->sx > v2->sx ? v0->sx : v2->sx) : (v1->sx > v2->sx ? v1->sx : v2->sx);
            float32 fMinY = v0->sy < v1->sy ? (v0->sy < v2->sy ? v0->sy : v2->sy) : (v1->sy < v2->sy ? v1->sy : v2->sy);
            float32 fMaxY = v0->sy > v1->sy ? (v0->sy > v2->sy ? v0->sy : v2->sy) : (v1->sy > v2->sy ? v1->sy : v2->sy);

            int32 xMin = (int32)floorf(fMinX); int32 xMax = (int32)ceilf(fMaxX);
            int32 yMin = (int32)floorf(fMinY); int32 yMax = (int32)ceilf(fMaxY);
            if (xMin < clipX0) xMin = clipX0; if (xMax > clipX1 - 1) xMax = clipX1 - 1;
            if (yMin < clipY0) yMin = clipY0; if (yMax > clipY1 - 1) yMax = clipY1 - 1;
            if (xMin > xMax || yMin > yMax) return;

            const bool hasShader = shader && shader->fragFn;
            const bool depthOnly = (colorBuf == nullptr);
            const bool fragWritesDepth = shader && shader->fragDepth;   // depth calculée par le fragment
            const bool hasDepth  = depthBuf && (st.depthTest || st.depthWrite);

            // Texture (chemin non-shader) : nearest, mip 0 (bilinéaire = Phase 2)
            const NkSWTexture* tex = nullptr; int32 texW=0, texH=0; const uint8* texData=nullptr; uint32 texBpp=0;
            if (!hasShader && !depthOnly && texBatch && texBatch->count > 0 &&
                texBatch->tex[0] && !texBatch->tex[0]->mips.Empty()) {
                tex = texBatch->tex[0];
                texW = (int32)tex->Width(0); texH = (int32)tex->Height(0);
                texData = tex->mips[0].Data(); texBpp = tex->Bpp();
            }
            const bool texLinear = tex ? (tex->defaultSampler.desc.magFilter == NkFilter::NK_LINEAR) : false;

            // Varyings source (déjà multipliées par invW pour l'interpolation perspective)
            const ScreenVert& c0 = *v0; const ScreenVert& c1 = *v1; const ScreenVert& c2 = *v2;

            // Fonctions d'arête INCRÉMENTALES : Ei(x,y)=Ai*x+Bi*y+Ci, dEi/dx=Ai.
            // Setup par triangle → 3 additions/pixel au lieu de recalculer 3 Orient2D (~9 mults).
            // Après normalisation area>0, un pixel est intérieur ssi E0,E1,E2 >= 0.
            const float32 A0 = v1->sy - v2->sy, B0 = v2->sx - v1->sx, C0 = -(A0*v1->sx + B0*v1->sy);
            const float32 A1 = v2->sy - v0->sy, B1 = v0->sx - v2->sx, C1 = -(A1*v2->sx + B1*v2->sy);
            const float32 A2 = v0->sy - v1->sy, B2 = v1->sx - v0->sx, C2 = -(A2*v0->sx + B2*v0->sy);
            const float32 fxMin = (float32)xMin + 0.5f;

            for (int32 y = yMin; y <= yMax; ++y) {
                const float32 py = (float32)y + 0.5f;
                float32* depthRow = hasDepth ? (depthBuf + (uint32)y * W) : nullptr;
                uint8*   colorRow = depthOnly ? nullptr : (colorBuf + (uint32)y * W * 4u);

                float32 e0 = A0*fxMin + B0*py + C0;
                float32 e1 = A1*fxMin + B1*py + C1;
                float32 e2 = A2*fxMin + B2*py + C2;

                for (int32 x = xMin; x <= xMax; ++x) {
                    const float32 ce0 = e0, ce1 = e1, ce2 = e2;
                    e0 += A0; e1 += A1; e2 += A2;              // avance (sûr même sur 'continue')
                    if (ce0 < 0.f || ce1 < 0.f || ce2 < 0.f) continue;
                    const float32 px = (float32)x + 0.5f;
                    const float32 w0 = ce0 * invArea, w1 = ce1 * invArea, w2 = ce2 * invArea;

                    // Profondeur : interpolation linéaire en screen-space (z déjà /w)
                    float32 z = w0 * v0->sz + w1 * v1->sz + w2 * v2->sz;
                    // Shader qui calcule sa propre profondeur (grille) : on DIFFÈRE le depth-test
                    // après le fragFn (il faut d'abord connaître la depth du fragment).
                    if (depthRow && st.depthTest && !fragWritesDepth) {
                        if (!DepthPass(st.depthOp, z, depthRow[x])) continue;
                    }

                    if (depthOnly) {
                        if (depthRow && st.depthWrite) depthRow[x] = z;
                        continue;
                    }

                    // Interpolation PERSPECTIVE-CORRECTE : attr = Σ wi·(attri·invWi) / Σ wi·invWi
                    const float32 iw0 = w0 * v0->invW, iw1 = w1 * v1->invW, iw2 = w2 * v2->invW;
                    const float32 invSum = 1.f / (iw0 + iw1 + iw2);

                    uint8* p = colorRow + x * 4;
                    float32 or_, og, ob, oa;

                    if (hasShader) {
                        NkVertexSoftware frag{};
                        frag.position = { px, py, z, 1.f };
                        frag.color.x = (c0.color.x*iw0 + c1.color.x*iw1 + c2.color.x*iw2) * invSum;
                        frag.color.y = (c0.color.y*iw0 + c1.color.y*iw1 + c2.color.y*iw2) * invSum;
                        frag.color.z = (c0.color.z*iw0 + c1.color.z*iw1 + c2.color.z*iw2) * invSum;
                        frag.color.w = (c0.color.w*iw0 + c1.color.w*iw1 + c2.color.w*iw2) * invSum;
                        frag.uv.x = (c0.uv.x*iw0 + c1.uv.x*iw1 + c2.uv.x*iw2) * invSum;
                        frag.uv.y = (c0.uv.y*iw0 + c1.uv.y*iw1 + c2.uv.y*iw2) * invSum;
                        frag.normal.x = (c0.normal.x*iw0 + c1.normal.x*iw1 + c2.normal.x*iw2) * invSum;
                        frag.normal.y = (c0.normal.y*iw0 + c1.normal.y*iw1 + c2.normal.y*iw2) * invSum;
                        frag.normal.z = (c0.normal.z*iw0 + c1.normal.z*iw1 + c2.normal.z*iw2) * invSum;
                        const uint32 ac = c0.attrCount > c1.attrCount ? (c0.attrCount > c2.attrCount ? c0.attrCount : c2.attrCount)
                                                                       : (c1.attrCount > c2.attrCount ? c1.attrCount : c2.attrCount);
                        frag.attrCount = ac;
                        for (uint32 i = 0; i < ac; ++i)
                            frag.attrs[i] = (c0.attrs[i]*iw0 + c1.attrs[i]*iw1 + c2.attrs[i]*iw2) * invSum;

                        if (fragWritesDepth) tl_fragDepthSet = false;
                        auto c = shader->fragFn(frag, uniformData, texPtr);
                        or_ = c.r; og = c.g; ob = c.b; oa = c.a;
                        // Depth calculée par le fragment : test différé ICI (après le fragFn).
                        if (fragWritesDepth && tl_fragDepthSet) {
                            z = tl_fragDepth;
                            if (depthRow && st.depthTest && !DepthPass(st.depthOp, z, depthRow[x])) continue;
                        } else if (fragWritesDepth) {
                            continue;   // le fragment n'a pas produit de depth (pas de hit) -> rejeté
                        }
                    } else {
                        // Couleur perspective-correcte
                        or_ = (c0.color.x*iw0 + c1.color.x*iw1 + c2.color.x*iw2) * invSum;
                        og  = (c0.color.y*iw0 + c1.color.y*iw1 + c2.color.y*iw2) * invSum;
                        ob  = (c0.color.z*iw0 + c1.color.z*iw1 + c2.color.z*iw2) * invSum;
                        oa  = (c0.color.w*iw0 + c1.color.w*iw1 + c2.color.w*iw2) * invSum;
                        if (tex) {
                            float32 u = (c0.uv.x*iw0 + c1.uv.x*iw1 + c2.uv.x*iw2) * invSum;
                            float32 v = (c0.uv.y*iw0 + c1.uv.y*iw1 + c2.uv.y*iw2) * invSum;
                            float32 tr, tg, tb, ta;
                            if (texLinear) SampleBilinear(texData, texW, texH, texBpp, u, v, tr, tg, tb, ta);
                            else           SampleNearest (texData, texW, texH, texBpp, u, v, tr, tg, tb, ta);
                            or_*=tr; og*=tg; ob*=tb; oa*=ta;
                        }
                    }

                    if (depthRow && st.depthWrite) depthRow[x] = z;
                    OutputPixel(p, st, or_, og, ob, oa);
                }
            }
        }

        // =============================================================================
        // Point d'entrée : rasterise un triangle en clip-space (avec clipping near)
        // Signature volontairement compatible avec l'ancien DrawTriangleFast + état.
        // =============================================================================
        // Triangle prêt à rasteriser (déjà clippé + projeté). Auto-suffisant → stockable
        // dans un buffer pour le binning tuilé (setup fait une seule fois par frame).
        struct ReadyTri { ScreenVert s[3]; };

        // Clip near-plane + projection d'un triangle clip-space → 0..2 ReadyTri (fan).
        static inline uint32 ClipProject(
            const NkVertexSoftware& v0, const NkVertexSoftware& v1, const NkVertexSoftware& v2,
            float32 W, float32 H, ReadyTri out[2], const NkSWViewport* vp=nullptr)
        {
            ClipVert in[3] = { ClipVert::From(v0), ClipVert::From(v1), ClipVert::From(v2) };
            if (NearDist(in[0]) < 0.f && NearDist(in[1]) < 0.f && NearDist(in[2]) < 0.f) return 0;

            ClipVert poly[4];
            uint32 n = 3;
            const bool needClip = NearDist(in[0]) < 0.f || NearDist(in[1]) < 0.f || NearDist(in[2]) < 0.f;
            if (needClip) { n = ClipNear(in, poly); if (n < 3) return 0; }
            else { poly[0] = in[0]; poly[1] = in[1]; poly[2] = in[2]; }

            uint32 k = 0;
            for (uint32 i = 1; i + 1 < n && k < 2; ++i) {
                out[k].s[0] = Project(poly[0],   W, H, vp);
                out[k].s[1] = Project(poly[i],   W, H, vp);
                out[k].s[2] = Project(poly[i+1], W, H, vp);
                ++k;
            }
            return k;
        }

        static void DrawTriangle(
            const NkVertexSoftware& v0, const NkVertexSoftware& v1, const NkVertexSoftware& v2,
            uint8* colorBuf, float32* depthBuf, uint32 W, uint32 H,
            int32 clipX0, int32 clipY0, int32 clipX1, int32 clipY1,
            const NkSWShader* shader, const void* uniformData, const void* texPtr,
            const NkSWTexBatch* texBatch, const NkSWRenderState& st)
        {
            if (W == 0 || H == 0) return;
            ReadyTri rt[2];
            const uint32 k = ClipProject(v0, v1, v2, (float32)W, (float32)H, rt);
            for (uint32 i = 0; i < k; ++i)
                RasterScreenTriangle(rt[i].s[0], rt[i].s[1], rt[i].s[2],
                                     colorBuf, depthBuf, W, H, clipX0, clipY0, clipX1, clipY1,
                                     shader, uniformData, texPtr, texBatch, st);
        }

    } // namespace swraster
} // namespace nkentseu
