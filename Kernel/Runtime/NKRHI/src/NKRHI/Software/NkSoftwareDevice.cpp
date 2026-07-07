// =============================================================================
// NkRHI_Device_SW.cpp — Software Rasterizer
// =============================================================================
#include "NkSoftwareDevice.h"
#include "NkSoftwareCommandBuffer.h"
#include "NKRHI/Core/NkGpuPolicy.h"
#include "NKLogger/NkLog.h"
#include "NKCore/NkTraits.h"
#include "NKMemory/NkAllocator.h"
#include "NkSWFastPath.h"
#include "Trace/NkSWRayTrace.h"   // v4 Phase 4 : ray-tracing CPU (BPR, fondation)
// v5 Phase A : VM bytecode NkSL (exécute les vrais shaders sur software)
#include "NKSL/NKSL.h"
#include "NKRHI/SL/NkSLIntegration.h"
#include "NKSL/VM/NkSLVM.h"
#include "NKSL/VM/NkSLByteCodeIO.h"
#include <cstdlib>   // getenv

// #include "NKRHI/Core/NkSkSL.h"

#include <cstring>
#include <cmath>
#include <algorithm>
#include <cassert>

#define NK_SW_LOG(...) logger_src.Infof("[NkRHI_SW] " __VA_ARGS__)

namespace nkentseu {

    // =============================================================================
    // NkSWTexture helpers
    // =============================================================================
    NkSWColor NkSWTexture::Read(uint32 x, uint32 y, uint32 mip) const {
        if (mips.Empty() || mip >= mips.Size()) return {};
        uint32 w = Width(mip);
        uint32 h = Height(mip);
        uint32 bpp = Bpp();
        x = math::NkMin(x, w - 1);
        y = math::NkMin(y, h - 1);
        const uint8* p = mips[mip].Data() + (y * w + x) * bpp;
        NkSWColor c;
        if (desc.format == NkGPUFormat::NK_D32_FLOAT) {
            float z = 1.0f;
            memcpy(&z, p, sizeof(float));
            c.r = z;
            c.g = z;
            c.b = z;
            c.a = 1.0f;
            return c;
        }
        // RT couleur HDR RGBA32F (float linéaire, ordre RGBA écrit par OutputPixelF) : lecture
        // directe, valeurs NON clampées (le tonemap fait l'ACES ensuite sur le vrai HDR).
        if (desc.format == NkGPUFormat::NK_RGBA32_FLOAT) {
            const float* fp = (const float*)p;
            c.r = fp[0]; c.g = fp[1]; c.b = fp[2]; c.a = fp[3];
            return c;
        }
        // Les render targets sont écrites par le rasterizer via sw_detail::StorePixel, qui
        // respecte NK_SW_PIXEL_BGRA (ordre [B][G][R][A] sur Windows). Les textures chargées
        // (PNG) restent en RGBA. On lit donc les RT dans l'ordre natif pour éviter un swap R↔B
        // au sampling (post-process) qui se propageait jusqu'à l'écran.
        const bool bgra = isRenderTarget && (NK_SW_PIXEL_BGRA != 0);
        const uint32 ir = bgra ? 2u : 0u, ib = bgra ? 0u : 2u;
        if (bpp >= 4) { c.r = p[ir]/255.f; c.g = p[1]/255.f; c.b = p[ib]/255.f; c.a = p[3]/255.f; }
        else if (bpp == 3) { c.r = p[ir]/255.f; c.g = p[1]/255.f; c.b = p[ib]/255.f; c.a = 1.f; }
        else if (bpp == 2) { c.r = p[0]/255.f; c.g = p[1]/255.f; c.a = 1.f; }
        else               { c.r = c.g = c.b = p[0]/255.f; c.a = 1.f; }
        return c;
    }

    void NkSWTexture::Write(uint32 x, uint32 y, const NkSWColor& c, uint32 mip) {
        if (mips.Empty() || mip >= mips.Size()) return;
        uint32 w = Width(mip);
        uint32 h = Height(mip);
        uint32 bpp = Bpp();
        if (x >= w || y >= h) return;
        uint8* p = mips[mip].Data() + (y * w + x) * bpp;
        // Clamp
        float32 r = math::NkMax(0.f,math::NkMin(1.f,c.r));
        float32 g = math::NkMax(0.f,math::NkMin(1.f,c.g));
        float32 b = math::NkMax(0.f,math::NkMin(1.f,c.b));
        float32 a = math::NkMax(0.f,math::NkMin(1.f,c.a));
        // Cas spécial float32 pour depth
        if (desc.format == NkGPUFormat::NK_D32_FLOAT) {
            memcpy(p, &c.r, 4); return;
        }
        // RT couleur HDR RGBA32F : écriture directe NON clampée (ordre RGBA).
        if (desc.format == NkGPUFormat::NK_RGBA32_FLOAT) {
            float* fp = (float*)p; fp[0]=c.r; fp[1]=c.g; fp[2]=c.b; fp[3]=c.a; return;
        }
        const bool bgra = isRenderTarget && (NK_SW_PIXEL_BGRA != 0);
        const uint32 ir = bgra ? 2u : 0u, ib = bgra ? 0u : 2u;
        if (bpp >= 4) { p[ir]=(uint8)(r*255); p[1]=(uint8)(g*255); p[ib]=(uint8)(b*255); p[3]=(uint8)(a*255); }
        else if (bpp == 3) { p[ir]=(uint8)(r*255); p[1]=(uint8)(g*255); p[ib]=(uint8)(b*255); }
        else if (bpp == 2) { p[0]=(uint8)(r*255); p[1]=(uint8)(g*255); }
        else               { p[0]=(uint8)(r*255); }
    }

    NkSWColor NkSWTexture::Sample(float u, float32 v, uint32 mip) const {
        if (mips.Empty()) return { 0,0,0,1 };
        mip = math::NkMin(mip, (uint32)mips.Size()-1);
        uint32 w = Width(mip);
        uint32 h = Height(mip);
        // Wrap
        u = u - math::NkFloor(u);
        v = v - math::NkFloor(v);
        float32 px = u * w - 0.5f;
        float32 py = v * h - 0.5f;
        // Bilinéaire
        int x0 = (int)math::NkFloor(px), y0 = (int)math::NkFloor(py);
        int x1 = x0 + 1, y1 = y0 + 1;
        float32 fx = px - x0, fy = py - y0;
        auto wrap = [](int v, int sz) { return ((v % sz) + sz) % sz; };
        NkSWColor c00 = Read((uint32)wrap(x0,w), (uint32)wrap(y0,h), mip);
        NkSWColor c10 = Read((uint32)wrap(x1,w), (uint32)wrap(y0,h), mip);
        NkSWColor c01 = Read((uint32)wrap(x0,w), (uint32)wrap(y1,h), mip);
        NkSWColor c11 = Read((uint32)wrap(x1,w), (uint32)wrap(y1,h), mip);
        auto lerp4 = [&](NkSWColor a, NkSWColor b, float32 t) -> NkSWColor {
            return { a.r+(b.r-a.r)*t, a.g+(b.g-a.g)*t, a.b+(b.b-a.b)*t, a.a+(b.a-a.a)*t };
        };
        return lerp4(lerp4(c00,c10,fx), lerp4(c01,c11,fx), fy);
    }

    // =============================================================================
    // NkSWRasterizer
    // =============================================================================
    NkVertexSoftware NkSWRasterizer::ClipToNDC(const NkVertexSoftware& v) const {
        NkVertexSoftware r = v;
        r.clipZ = v.position.z;
        r.clipW = v.position.w;
        float32 invW = v.position.w != 0.f ? 1.f / v.position.w : 0.f;
        r.position.x *= invW;
        r.position.y *= invW;
        r.position.z *= invW;
        r.position.w  = invW; // stocker 1/w pour interpolation perspective
        return r;
    }

    NkVertexSoftware NkSWRasterizer::NDCToScreen(const NkVertexSoftware& v, float32 w, float32 h) const {
        NkVertexSoftware r = v;
        r.position.x = (v.position.x + 1.f) * 0.5f * w;
        r.position.y = (1.f - v.position.y) * 0.5f * h; // Y-flip
        r.position.z = v.position.z * 0.5f + 0.5f;       // [-1,1] → [0,1]
        return r;
    }

    NkVertexSoftware NkSWRasterizer::Interpolate(const NkVertexSoftware& a, const NkVertexSoftware& b, float32 t) const {
        NkVertexSoftware r;
        r.position.x = a.position.x + (b.position.x - a.position.x) * t;
        r.position.y = a.position.y + (b.position.y - a.position.y) * t;
        r.position.z = a.position.z + (b.position.z - a.position.z) * t;
        r.position.w = a.position.w + (b.position.w - a.position.w) * t;
        r.clipZ = a.clipZ + (b.clipZ - a.clipZ) * t;
        r.clipW = a.clipW + (b.clipW - a.clipW) * t;
        r.uv.x = a.uv.x + (b.uv.x - a.uv.x) * t;
        r.uv.y = a.uv.y + (b.uv.y - a.uv.y) * t;
        r.color.r = a.color.r + (b.color.r - a.color.r) * t;
        r.color.g = a.color.g + (b.color.g - a.color.g) * t;
        r.color.b = a.color.b + (b.color.b - a.color.b) * t;
        r.color.a = a.color.a + (b.color.a - a.color.a) * t;
        for (int i=0; i<16; i++) r.attrs[i] = a.attrs[i]+(b.attrs[i]-a.attrs[i])*t;
        return r;
    }

    NkVertexSoftware NkSWRasterizer::BaryInterp(const NkVertexSoftware& v0, const NkVertexSoftware& v1,
                                        const NkVertexSoftware& v2,
                                        float32 l0, float32 l1, float32 l2) const {
        // Interpolation perspective-correcte avec 1/w stocké dans position.w
        float32 w = l0 * v0.position.w + l1 * v1.position.w + l2 * v2.position.w;
        float32 invW = w != 0.f ? 1.f / w : 0.f;
        NkVertexSoftware r;
        auto lerp3 = [&](float a, float32 b, float32 c) {
            return (l0*a*v0.position.w + l1*b*v1.position.w + l2*c*v2.position.w) * invW;
        };
        r.uv.x    = lerp3(v0.uv.x, v1.uv.x, v2.uv.x);
        r.uv.y    = lerp3(v0.uv.y, v1.uv.y, v2.uv.y);
        r.normal.x= lerp3(v0.normal.x, v1.normal.x, v2.normal.x);
        r.normal.y= lerp3(v0.normal.y, v1.normal.y, v2.normal.y);
        r.normal.z= lerp3(v0.normal.z, v1.normal.z, v2.normal.z);
        r.color.r = lerp3(v0.color.r, v1.color.r, v2.color.r);
        r.color.g = lerp3(v0.color.g, v1.color.g, v2.color.g);
        r.color.b = lerp3(v0.color.b, v1.color.b, v2.color.b);
        r.color.a = lerp3(v0.color.a, v1.color.a, v2.color.a);
        for (int i=0; i<16; i++) r.attrs[i] = lerp3(v0.attrs[i], v1.attrs[i], v2.attrs[i]);
        return r;
    }

    bool NkSWRasterizer::DepthTest(uint32 x, uint32 y, float32 z) {
        if (!mState.depthTarget || !mState.pipeline) return true;
        if (!mState.pipeline->depthTest) return true;

        NkSWColor d = mState.depthTarget->Read(x, y);
        float32 dz = d.r; // depth stocke dans le canal rouge
        constexpr float32 kDepthEpsilon = 1e-5f;
        bool pass = false;
        switch (mState.pipeline->depthOp) {
            case NkCompareOp::NK_LESS:          pass = z <  (dz + kDepthEpsilon); break;
            case NkCompareOp::NK_LESS_EQUAL:    pass = z <= (dz + kDepthEpsilon); break;
            case NkCompareOp::NK_GREATER:       pass = z >  (dz - kDepthEpsilon); break;
            case NkCompareOp::NK_GREATER_EQUAL: pass = z >= (dz - kDepthEpsilon); break;
            case NkCompareOp::NK_EQUAL:         pass = z == dz; break;
            case NkCompareOp::NK_NOT_EQUAL:     pass = z != dz; break;
            case NkCompareOp::NK_ALWAYS:        pass = true; break;
            case NkCompareOp::NK_NEVER:         pass = false; break;
            default:                            pass = z <  (dz + kDepthEpsilon); break;
        }
        if (pass && mState.pipeline->depthWrite) {
            NkSWColor dc{ z, z, z, 1.f };
            mState.depthTarget->Write(x, y, dc);
        }
        return pass;
    }

    float32 NkSWRasterizer::ApplyBlendFactor(NkBlendFactor f, float32 src, float32 dst, float32 srcA, float32 dstA) const {
        switch (f) {
            case NkBlendFactor::NK_ZERO:             return 0.f;
            case NkBlendFactor::NK_ONE:              return 1.f;
            case NkBlendFactor::NK_SRC_COLOR:         return src;
            case NkBlendFactor::NK_ONE_MINUS_SRC_COLOR: return 1.f - src;
            case NkBlendFactor::NK_SRC_ALPHA:         return srcA;
            case NkBlendFactor::NK_ONE_MINUS_SRC_ALPHA: return 1.f - srcA;
            case NkBlendFactor::NK_DST_ALPHA:         return dstA;
            case NkBlendFactor::NK_ONE_MINUS_DST_ALPHA: return 1.f - dstA;
            default:                              return 1.f;
        }
    }

    NkSWColor NkSWRasterizer::BlendColor(const NkSWColor& src, const NkSWColor& dst) const {
        if (!mState.pipeline || !mState.pipeline->blendEnable) return src;
        NkSWColor r;
        r.r = src.r * ApplyBlendFactor(mState.pipeline->srcColor, src.r, dst.r, src.a, dst.a)
            + dst.r * ApplyBlendFactor(mState.pipeline->dstColor, src.r, dst.r, src.a, dst.a);
        r.g = src.g * ApplyBlendFactor(mState.pipeline->srcColor, src.g, dst.g, src.a, dst.a)
            + dst.g * ApplyBlendFactor(mState.pipeline->dstColor, src.g, dst.g, src.a, dst.a);
        r.b = src.b * ApplyBlendFactor(mState.pipeline->srcColor, src.b, dst.b, src.a, dst.a)
            + dst.b * ApplyBlendFactor(mState.pipeline->dstColor, src.b, dst.b, src.a, dst.a);
        r.a = src.a; // simplification alpha
        return r;
    }

    void NkSWRasterizer::DrawPoint(const NkVertexSoftware& v0) {
        if (!mState.colorTarget && !mState.depthTarget) return;
        NkSWTexture* dimTarget = mState.colorTarget ? mState.colorTarget : mState.depthTarget;
        auto ndc  = ClipToNDC(v0);
        auto scr  = NDCToScreen(ndc, (float)dimTarget->Width(), (float)dimTarget->Height());
        uint32 x = (uint32)scr.position.x;
        uint32 y = (uint32)scr.position.y;
        if (x >= dimTarget->Width() || y >= dimTarget->Height()) return;
        const uint32 clipMinXVal = (this->clipMaxX > this->clipMinX) ? this->clipMinX : 0u;
        const uint32 clipMinYVal = (this->clipMaxY > this->clipMinY) ? this->clipMinY : 0u;
        const uint32 clipMaxXSafe = (this->clipMaxX > this->clipMinX) ? this->clipMaxX : dimTarget->Width();
        const uint32 clipMaxYSafe = (this->clipMaxY > this->clipMinY) ? this->clipMaxY : dimTarget->Height();
        if (x < clipMinXVal || y < clipMinYVal || x >= clipMaxXSafe || y >= clipMaxYSafe) return;
        if (!DepthTest(x, y, scr.position.z)) return;
        if (mState.colorTarget) {
            NkSWColor c = scr.color;
            if (mState.shader && mState.shader->fragFn) {
                c = mState.shader->fragFn(scr, mState.uniformData, mState.texSampler);
            } else if (mState.texSampler && scr.uv.x >= 0.f && scr.uv.y >= 0.f) {
                const NkSWTexture* tex = static_cast<const NkSWTexture*>(mState.texSampler);
                const float32 su = math::NkClamp(scr.uv.x, 0.f, 1.f);
                const float32 sv = math::NkClamp(scr.uv.y, 0.f, 1.f);
                const NkSWColor tc = tex->Sample(su, sv);
                c = {c.r * tc.r, c.g * tc.g, c.b * tc.b, c.a * tc.a};
            }
            mState.colorTarget->Write(x, y, c);
        }
    }

    void NkSWRasterizer::DrawLine(const NkVertexSoftware& v0, const NkVertexSoftware& v1) {
        if (!mState.colorTarget && !mState.depthTarget) return;
        NkSWTexture* dimTarget = mState.colorTarget ? mState.colorTarget : mState.depthTarget;
        float32 W = (float)dimTarget->Width();
        float32 H = (float)dimTarget->Height();
        auto ndc0 = NDCToScreen(ClipToNDC(v0), W, H);
        auto ndc1 = NDCToScreen(ClipToNDC(v1), W, H);

        float32 dx = ndc1.position.x - ndc0.position.x;
        float32 dy = ndc1.position.y - ndc0.position.y;
        float32 steps = math::NkMax(math::NkFabs(dx), math::NkFabs(dy));
        if (steps < 1.f) { DrawPoint(v0); return; }
        float32 inv = 1.f / steps;
        for (float s = 0; s <= steps; s++) {
            float32 t = s * inv;
            NkVertexSoftware p = Interpolate(ndc0, ndc1, t);
            uint32 x = (uint32)p.position.x, y = (uint32)p.position.y;
            if (x >= (uint32)W || y >= (uint32)H) continue;
            const uint32 clipMinXVal = (this->clipMaxX > this->clipMinX) ? this->clipMinX : 0u;
            const uint32 clipMinYVal = (this->clipMaxY > this->clipMinY) ? this->clipMinY : 0u;
            const uint32 clipMaxXSafe = (this->clipMaxX > this->clipMinX) ? this->clipMaxX : static_cast<uint32>(W);
            const uint32 clipMaxYSafe = (this->clipMaxY > this->clipMinY) ? this->clipMaxY : static_cast<uint32>(H);
            if (x < clipMinXVal || y < clipMinYVal || x >= clipMaxXSafe || y >= clipMaxYSafe) continue;
            if (!DepthTest(x, y, p.position.z)) continue;
            if (mState.colorTarget) {
                NkSWColor c = p.color;
                if (mState.shader && mState.shader->fragFn) {
                    c = mState.shader->fragFn(p, mState.uniformData, mState.texSampler);
                } else if (mState.texSampler && p.uv.x >= 0.f && p.uv.y >= 0.f) {
                    const NkSWTexture* tex = static_cast<const NkSWTexture*>(mState.texSampler);
                    const float32 su = math::NkClamp(p.uv.x, 0.f, 1.f);
                    const float32 sv = math::NkClamp(p.uv.y, 0.f, 1.f);
                    const NkSWColor tc = tex->Sample(su, sv);
                    c = {c.r * tc.r, c.g * tc.g, c.b * tc.b, c.a * tc.a};
                }
                mState.colorTarget->Write(x, y, c);
            }
        }
    }

    void NkSWRasterizer::DrawTriangle(const NkVertexSoftware& v0, const NkVertexSoftware& v1, const NkVertexSoftware& v2) {
        if (!mState.colorTarget && !mState.depthTarget) return;
        NkSWTexture* target = mState.colorTarget ? mState.colorTarget : mState.depthTarget;

        const float32 W = (float32)target->Width();
        const float32 H = (float32)target->Height();

        // Transformer vers l'espace écran
        auto s0 = NDCToScreen(ClipToNDC(v0), W, H);
        auto s1 = NDCToScreen(ClipToNDC(v1), W, H);
        auto s2 = NDCToScreen(ClipToNDC(v2), W, H);

        if (mState.wireframe) { DrawLine(s0,s1); DrawLine(s1,s2); DrawLine(s2,s0); return; }

        // Aire signée (cross product 2D)
        float32 area2 =
            (s1.position.x - s0.position.x) *
            (s2.position.y - s0.position.y) -
            (s2.position.x - s0.position.x) *
            (s1.position.y - s0.position.y);

        if (fabsf(area2) < 0.1f) return;  // triangle dégénéré

        // Culling
        if (mState.pipeline && mState.pipeline->cullMode != NkCullMode::NK_NONE) {
            bool ccw = mState.pipeline->frontFace == NkFrontFace::NK_CCW;
            bool isFront = ccw ? (area2 > 0) : (area2 < 0);
            if ((mState.pipeline->cullMode == NkCullMode::NK_BACK  && !isFront) ||
                (mState.pipeline->cullMode == NkCullMode::NK_FRONT &&  isFront))
                return;
        }

        // ── Trier par Y croissant ─────────────────────────────────────────────────
        // On a besoin de 3 pointeurs pour le tri stable sans copie
        const NkVertexSoftware* pa = &s0;
        const NkVertexSoftware* pb = &s1;
        const NkVertexSoftware* pc = &s2;
        if (pa->position.y > pb->position.y) std::swap(pa, pb);
        if (pb->position.y > pc->position.y) std::swap(pb, pc);
        if (pa->position.y > pb->position.y) std::swap(pa, pb);

        // Bornes viewport
        const int32 clipX0 = (clipMaxX > clipMinX) ? (int32)clipMinX : 0;
        const int32 clipY0 = (clipMaxY > clipMinY) ? (int32)clipMinY : 0;
        const int32 clipX1 = (clipMaxX > clipMinX) ? (int32)clipMaxX : (int32)W;
        const int32 clipY1 = (clipMaxY > clipMinY) ? (int32)clipMaxY : (int32)H;

        const int32 yMin = math::NkClamp((int32)ceilf(pa->position.y),  clipY0, clipY1 - 1);
        const int32 yMax = math::NkClamp((int32)floorf(pc->position.y), clipY0, clipY1 - 1);
        if (yMin > yMax) return;

        // Helper : interpoler attributs d'une arête à hauteur targetY
        // Retourne x + couleur + uv interpolés
        struct EdgePoint {
            float32 x;
            float32 r, g, b, a;
            float32 u, v_coord;
            float32 z;
        };

        auto EdgeStep = [](const NkVertexSoftware& va, const NkVertexSoftware& vb, float32 ty, EdgePoint& out) {
            const float32 dy = vb.position.y - va.position.y;
            if (fabsf(dy) < 1e-6f) {
                out.x = va.position.x;
                out.r = va.color.r; out.g = va.color.g;
                out.b = va.color.b; out.a = va.color.a;
                out.u = va.uv.x;   out.v_coord = va.uv.y;
                out.z = va.position.z;
                return;
            }
            const float32 t = (ty - va.position.y) / dy;
            out.x = va.position.x + (vb.position.x - va.position.x) * t;
            out.r = va.color.r + (vb.color.r - va.color.r) * t;
            out.g = va.color.g + (vb.color.g - va.color.g) * t;
            out.b = va.color.b + (vb.color.b - va.color.b) * t;
            out.a = va.color.a + (vb.color.a - va.color.a) * t;
            out.u = va.uv.x    + (vb.uv.x    - va.uv.x)    * t;
            out.v_coord = va.uv.y + (vb.uv.y - va.uv.y)    * t;
            out.z = va.position.z + (vb.position.z - va.position.z) * t;
        };

        // Pixel/fragment shader
        const bool hasShader  = mState.shader && mState.shader->fragFn;
        const bool hasTex     = !hasShader && mState.texSampler;
        const NkSWTexture* tex = hasTex ? static_cast<const NkSWTexture*>(mState.texSampler) : nullptr;
        const int32 texW = tex ? (int32)tex->Width()  : 0;
        const int32 texH = tex ? (int32)tex->Height() : 0;

        const bool useBlend   = mState.pipeline && mState.pipeline->blendEnable;
        const bool hasDepth   = mState.depthTarget && mState.pipeline &&
                                (mState.pipeline->depthTest || mState.pipeline->depthWrite);

        // ── Scan-line loop ────────────────────────────────────────────────────────
        for (int32 y = yMin; y <= yMax; ++y) {
            const float32 fy = (float32)y + 0.5f;

            // Côté long pa→pc
            EdgePoint eL, eR;
            EdgeStep(*pa, *pc, fy, eL);

            // Côté court : pa→pb ou pb→pc
            if (fy <= pb->position.y)
                EdgeStep(*pa, *pb, fy, eR);
            else
                EdgeStep(*pb, *pc, fy, eR);

            // Assurer L ≤ R
            if (eL.x > eR.x) { EdgePoint tmp = eL; eL = eR; eR = tmp; }

            const int32 xStart = math::NkClamp((int32)ceilf(eL.x - 0.5f), clipX0, clipX1 - 1);
            const int32 xEnd   = math::NkClamp((int32)floorf(eR.x + 0.5f), clipX0, clipX1 - 1);
            if (xStart > xEnd) continue;

            // Deltas par pixel
            const float32 spanW = eR.x - eL.x;
            const float32 inv   = (spanW > 1e-6f) ? 1.f / spanW : 0.f;
            const float32 off   = (float32)xStart - eL.x + 0.5f;

            const float32 drDx = (eR.r - eL.r) * inv;
            const float32 dgDx = (eR.g - eL.g) * inv;
            const float32 dbDx = (eR.b - eL.b) * inv;
            const float32 daDx = (eR.a - eL.a) * inv;
            const float32 duDx = (eR.u - eL.u) * inv;
            const float32 dvDx = (eR.v_coord - eL.v_coord) * inv;
            const float32 dzDx = (eR.z - eL.z) * inv;

            float32 cr = eL.r + drDx * off;
            float32 cg = eL.g + dgDx * off;
            float32 cb = eL.b + dbDx * off;
            float32 ca = eL.a + daDx * off;
            float32 cu = eL.u + duDx * off;
            float32 cv = eL.v_coord + dvDx * off;
            float32 cz = eL.z + dzDx * off;

            const int32 count = xEnd - xStart + 1;

            // Couleur uniforme + opaque sur le span → SIMD fill
            const bool uniformColor = (fabsf(drDx) < 1e-4f && fabsf(dgDx) < 1e-4f &&
                                    fabsf(dbDx) < 1e-4f && fabsf(daDx) < 1e-4f);
            const uint8 ca8 = (uint8)math::NkClamp((int32)(ca * 255.f + 0.5f), 0, 255);

            uint8* colorRow = mState.colorTarget
                ? mState.colorTarget->mips[0].Data() + y * (int32)W * 4
                : nullptr;

            if (colorRow && !hasShader && !hasTex && uniformColor && !hasDepth) {
                const uint8 sr = (uint8)math::NkClamp((int32)(cr * 255.f + 0.5f), 0, 255);
                const uint8 sg = (uint8)math::NkClamp((int32)(cg * 255.f + 0.5f), 0, 255);
                const uint8 sb = (uint8)math::NkClamp((int32)(cb * 255.f + 0.5f), 0, 255);

                if (ca8 == 255u) {
                    nkentseu::sw_detail::FillSpanOpaque(colorRow, xStart, xEnd + 1, sr, sg, sb);
                } else if (ca8 > 0u && useBlend) {
                    nkentseu::sw_detail::BlendSpanAlpha(colorRow, xStart, xEnd + 1, sr, sg, sb, ca8);
                }
                continue;
            }

            // ── Chemin général pixel par pixel ────────────────────────────────────
            for (int32 x = xStart; x <= xEnd; ++x) {
                // Depth test
                if (hasDepth) {
                    if (!DepthTest((uint32)x, (uint32)y, cz)) {
                        cr += drDx; cg += dgDx; cb += dbDx; ca += daDx;
                        cu += duDx; cv += dvDx; cz += dzDx;
                        continue;
                    }
                }

                // Couleur du fragment
                uint8 sr, sg, sb, sa;

                if (hasShader) {
                    // Appeler le pixel shader
                    NkVertexSoftware frag;
                    frag.position = {(float32)x + 0.5f, (float32)y + 0.5f, cz, 1.f};
                    frag.color    = {cr, cg, cb, ca};
                    frag.uv       = {cu, cv};
                    auto c = mState.shader->fragFn(frag, mState.uniformData, mState.texSampler);
                    sr = (uint8)math::NkClamp((int32)(c.r * 255.f + 0.5f), 0, 255);
                    sg = (uint8)math::NkClamp((int32)(c.g * 255.f + 0.5f), 0, 255);
                    sb = (uint8)math::NkClamp((int32)(c.b * 255.f + 0.5f), 0, 255);
                    sa = (uint8)math::NkClamp((int32)(c.a * 255.f + 0.5f), 0, 255);
                } else if (hasTex && tex) {
                    // Sample texture (nearest-neighbor)
                    const int32 tx = math::NkClamp((int32)(cu * texW), 0, texW - 1);
                    const int32 ty = math::NkClamp((int32)(cv * texH), 0, texH - 1);
                    NkSWColor tc = tex->Read((uint32)tx, (uint32)ty, 0);
                    sr = (uint8)math::NkClamp((int32)((tc.r * cr) * 255.f + 0.5f), 0, 255);
                    sg = (uint8)math::NkClamp((int32)((tc.g * cg) * 255.f + 0.5f), 0, 255);
                    sb = (uint8)math::NkClamp((int32)((tc.b * cb) * 255.f + 0.5f), 0, 255);
                    sa = (uint8)math::NkClamp((int32)((tc.a * ca) * 255.f + 0.5f), 0, 255);
                } else {
                    sr = (uint8)math::NkClamp((int32)(cr * 255.f + 0.5f), 0, 255);
                    sg = (uint8)math::NkClamp((int32)(cg * 255.f + 0.5f), 0, 255);
                    sb = (uint8)math::NkClamp((int32)(cb * 255.f + 0.5f), 0, 255);
                    sa = (uint8)math::NkClamp((int32)(ca * 255.f + 0.5f), 0, 255);
                }

                // Écrire dans la texture couleur
                if (colorRow && sa > 0u) {
                    uint8* p = colorRow + x * 4;
                    if (!useBlend || sa == 255u) {
                        nkentseu::sw_detail::StorePixel(p, sr, sg, sb, sa);
                    } else {
                        nkentseu::sw_detail::BlendPixel(p, sr, sg, sb, sa);
                    }
                }

                cr += drDx; cg += dgDx; cb += dbDx; ca += daDx;
                cu += duDx; cv += dvDx; cz += dzDx;
            }
        }
    }

    void NkSWRasterizer::DrawTriangles(const NkVertexSoftware* verts, uint32 count) {
        if (!verts || count == 0) return;
        NkPrimitiveTopology topology = NkPrimitiveTopology::NK_TRIANGLE_LIST;
        if (mState.pipeline) topology = mState.pipeline->topology;

        switch (topology) {
            case NkPrimitiveTopology::NK_POINT_LIST:
                for (uint32 i = 0; i < count; ++i) DrawPoint(verts[i]);
                break;
            case NkPrimitiveTopology::NK_LINE_LIST:
                for (uint32 i = 0; i + 1 < count; i += 2) DrawLine(verts[i], verts[i + 1]);
                break;
            case NkPrimitiveTopology::NK_LINE_STRIP:
                for (uint32 i = 1; i < count; ++i) DrawLine(verts[i - 1], verts[i]);
                break;
            case NkPrimitiveTopology::NK_TRIANGLE_STRIP:
                for (uint32 i = 2; i < count; ++i) DrawTriangle(verts[i - 2], verts[i - 1], verts[i]);
                break;
            case NkPrimitiveTopology::NK_TRIANGLE_FAN:
                for (uint32 i = 2; i < count; ++i) DrawTriangle(verts[0], verts[i - 1], verts[i]);
                break;
            case NkPrimitiveTopology::NK_PATCH_LIST:
                // Software backend does not evaluate patches yet.
                break;
            case NkPrimitiveTopology::NK_TRIANGLE_LIST:
            default:
                for (uint32 i = 0; i + 2 < count; i += 3) DrawTriangle(verts[i], verts[i + 1], verts[i + 2]);
                break;
        }
    }

    // =============================================================================
    // NkSoftwareDevice
    // =============================================================================
    NkSoftwareDevice::~NkSoftwareDevice() { if (mIsValid) Shutdown(); }

    bool NkSoftwareDevice::Initialize(const NkDeviceInitInfo& init) {
        InitNativePresenter(init.surface);

        mInit   = init;
        NkGpuPolicy::ApplyPreContext(mInit.context);

        const NkSoftwareDesc& swCfg = mInit.context.software;
        mWidth  = NkDeviceInitWidth(init);
        mHeight = NkDeviceInitHeight(init);
        if (mWidth == 0)  mData.width = mWidth  = 512;
        if (mHeight == 0) mData.height = mHeight = 512;
        mUseSse = swCfg.useSSE;
        mThreadCount = swCfg.threadCount > 0
            ? swCfg.threadCount
            : math::NkMax(1u, (uint32)std::thread::hardware_concurrency());
        if (!swCfg.threading) {
            mThreadCount = 1;
        }

        // v4 Phase 2 : SSAA opt-in (NK_SW_SSAA=1..4, défaut 1 = off)
        { const char* e = std::getenv("NK_SW_SSAA"); uint32 s = e ? (uint32)std::atoi(e) : 1u;
          mSSAA = s < 1u ? 1u : (s > 4u ? 4u : s); }

        // v4 Phase 4 : mode BPR ray-tracing live (NK_SW_RT=1) — chaque Present ray-trace.
        { const char* e = std::getenv("NK_SW_RT"); mRtMode = (e && e[0] == '1'); }
        // Resolution scale du RT : rendu à 1/scale puis upscale (défaut 4 ; plus grand = plus fluide).
        { const char* e = std::getenv("NK_SW_RT_SCALE"); uint32 s = e ? (uint32)std::atoi(e) : 4u;
          mRtScale = s < 1u ? 1u : (s > 16u ? 16u : s); }

        // Garde-fou : signaler un toggle debug qui plombe la perf (piège NK_SW_NOMT).
        if (const char* e = std::getenv("NK_SW_NOMT")) if (e[0] == '1')
            NK_SW_LOG("ATTENTION: NK_SW_NOMT=1 actif -> rendu MONO-THREAD force (debug). Retire la variable pour la perf.\n");

        CreateSwapchainObjects();

        mCaps.computeShaders     = NkDeviceInitComputeEnabledForApi(mInit, NkGraphicsApi::NK_GFX_API_SOFTWARE);
        mCaps.drawIndirect       = false;
        mCaps.multiViewport      = false;
        mCaps.independentBlend   = true;
        mCaps.maxTextureDim2D    = 4096;
        mCaps.maxColorAttachments= 1;
        mCaps.maxVertexAttributes= 16;
        mCaps.maxPushConstantBytes=256;

        mIsValid = true;
        // swfast::GetThreadPool().Init(mThreadCount);
        NK_SW_LOG("Initialisé (%u×%u, %u threads, SSE=%s)\n", mWidth, mHeight, mThreadCount, mUseSse ? "on" : "off");

        // v4 Phase 4 : self-test ray-tracing (NK_SW_RT_TEST=1) → Build/Captures/rt_selftest.ppm
        if (const char* e = std::getenv("NK_SW_RT_TEST")) {
            if (e[0] == '1') {
                swtrace::SelfTest("Build/Captures/rt_selftest.ppm");
                NK_SW_LOG("Ray-trace self-test ecrit: Build/Captures/rt_selftest.ppm\n");
            }
        }
        return true;
    }

    void NkSoftwareDevice::CreateSwapchainObjects() {
        // v4 Phase 2 : le swapchain est rendu à la résolution SSAA (hiW×hiH),
        // puis résolu (downsample) vers mResolveBuf (taille fenêtre) au Present.
        const uint32 hiW = mWidth  * mSSAA;
        const uint32 hiH = mHeight * mSSAA;

        // Color backbuffer
        NkTextureDesc cd;
        cd.format     = NkGPUFormat::NK_RGBA8_UNORM;
        cd.width      = hiW;
        cd.height     = hiH;
        cd.bindFlags  = NkBindFlags::NK_RENDER_TARGET | NkBindFlags::NK_SHADER_RESOURCE;
        cd.debugName  = "SW_Backbuffer";
        auto colorH = CreateTexture(cd);

        // Depth buffer
        NkTextureDesc dd;
        dd.format     = NkGPUFormat::NK_D32_FLOAT;
        dd.width      = hiW;
        dd.height     = hiH;
        dd.bindFlags  = NkBindFlags::NK_DEPTH_STENCIL;
        dd.debugName  = "SW_Depthbuffer";
        auto depthH = CreateTexture(dd);

        NkRenderPassDesc rpd;
        rpd.AddColor(NkAttachmentDesc::Color(NkGPUFormat::NK_RGBA8_UNORM))
        .SetDepth(NkAttachmentDesc::Depth());
        mSwapchainRP = CreateRenderPass(rpd);

        NkFramebufferDesc fbd;
        fbd.renderPass = mSwapchainRP;
        fbd.colorAttachments.PushBack(colorH);
        fbd.depthAttachment = depthH;
        fbd.width = hiW;
        fbd.height = hiH;
        mSwapchainFB = CreateFramebuffer(fbd);

        // Buffer de résolution taille-fenêtre (uniquement si SSAA actif)
        if (mSSAA > 1)
            mResolveBuf.Assign((uint8)0, (NkVector<uint8>::SizeType)(mWidth * mHeight * 4u));
    }

    void NkSoftwareDevice::Shutdown() {
        // swfast::GetThreadPool().Shutdown();
        mBuffers.Clear(); 
        mTextures.Clear(); 
        mSamplers.Clear();
        mShaders.Clear(); 
        mPipelines.Clear(); 
        mRenderPasses.Clear();
        mFramebuffers.Clear(); 
        mDescLayouts.Clear(); 
        mDescSets.Clear();
        ShutdownNativePresenter();
        mIsValid = false;
        NK_SW_LOG("Shutdown\n");
    }

    // =============================================================================
    // Buffers
    // =============================================================================
    NkBufferHandle NkSoftwareDevice::CreateBuffer(const NkBufferDesc& desc) {
        threading::NkScopedLockMutex lock(mMutex);
        NkSWBuffer b;
        b.desc = desc;
        b.data.Resize((size_t)desc.sizeBytes, 0);
        if (desc.initialData)
            memcpy(b.data.Data(), desc.initialData, (size_t)desc.sizeBytes);
        uint64 hid = NextId(); mBuffers[hid] = traits::NkMove(b);
        NkBufferHandle h; h.id = hid; return h;
    }
    void NkSoftwareDevice::DestroyBuffer(NkBufferHandle& h) {
        threading::NkScopedLockMutex lock(mMutex); mBuffers.Erase(h.id); h.id = 0;
    }
    bool NkSoftwareDevice::WriteBuffer(NkBufferHandle buf, const void* data, uint64 sz, uint64 off) {
        auto* it = mBuffers.Find(buf.id); if (!it) return false;
        if (off + sz > it->data.Size()) return false;
        memcpy(it->data.Data() + off, data, (size_t)sz);
        return true;
    }
    bool NkSoftwareDevice::WriteBufferAsync(NkBufferHandle buf, const void* data, uint64 sz, uint64 off) {
        return WriteBuffer(buf, data, sz, off);
    }
    bool NkSoftwareDevice::ReadBuffer(NkBufferHandle buf, void* out, uint64 sz, uint64 off) {
        auto* it = mBuffers.Find(buf.id); if (!it) return false;
        memcpy(out, it->data.Data() + off, (size_t)sz);
        return true;
    }
    NkMappedMemory NkSoftwareDevice::MapBuffer(NkBufferHandle buf, uint64 off, uint64 sz) {
        auto* it = mBuffers.Find(buf.id); if (!it) return {};
        uint64 mapSz = sz > 0 ? sz : it->desc.sizeBytes - off;
        it->mapped = true;
        return { it->data.Data() + off, mapSz };
    }
    void NkSoftwareDevice::UnmapBuffer(NkBufferHandle buf) {
        auto* it = mBuffers.Find(buf.id); if (it) it->mapped = false;
    }

    // =============================================================================
    // Textures
    // =============================================================================
    NkTextureHandle NkSoftwareDevice::CreateTexture(const NkTextureDesc& desc) {
        threading::NkScopedLockMutex lock(mMutex);
        NkSWTexture t;
        t.desc = desc;
        t.isRenderTarget = NkHasFlag(desc.bindFlags, NkBindFlags::NK_RENDER_TARGET) ||
                        NkHasFlag(desc.bindFlags, NkBindFlags::NK_DEPTH_STENCIL);

        // Render target couleur HDR (RGBA16F=8 o/px) : on la stocke en **RGBA32F** (16 o/px, float
        // linéaire, ordre RGBA). Le rasterizer écrit alors ses valeurs SANS clamp [0,1] (chemin
        // `colorFloat`) → le tonemap lit le VRAI HDR (rolloff des hautes lumières, traînées des
        // point lights), au lieu de l'ancien remap RGBA8 qui écrasait tout à [0,1]. Le stride de
        // ligne est désormais W*colorBpp (plus de W*4 hardcodé). Les RT ≤4 o/px restent RGBA8/BGRA.
        const bool isColorRT = NkHasFlag(desc.bindFlags, NkBindFlags::NK_RENDER_TARGET) &&
                              !NkHasFlag(desc.bindFlags, NkBindFlags::NK_DEPTH_STENCIL);
        if (isColorRT && NkFormatBytesPerPixel(desc.format) > 4u)
            t.desc.format = NkGPUFormat::NK_RGBA32_FLOAT;

        uint32 bpp = NkFormatBytesPerPixel(t.desc.format);
        // FIX heap-overflow (NKRenderer sur software) : le rasterizer et les clears écrivent
        // toujours 4 octets/pixel. Un format inconnu (NkFormatBytesPerPixel → 0) donnait une
        // allocation de 0 octet, et une render target < 4 o/pixel (R8/RG8) était sous-allouée →
        // écriture au-delà → corruption du heap (nœuds de map) → crash aléatoire dans Find.
        uint32 allocBpp = (bpp == 0u) ? 4u : bpp;
        if (t.isRenderTarget && allocBpp < 4u) allocBpp = 4u;
        // Cubemaps (6 faces), arrays (cascades CSM), 3D : allouer toutes les couches.
        const uint32 layers = math::NkMax(1u, desc.arrayLayers) * math::NkMax(1u, desc.depth);

        uint32 mipCount = desc.mipLevels == 0
            ? (uint32)(math::NkFloor(math::NkLog2(static_cast<float32>(math::NkMax(desc.width, desc.height)))) + 1.0f)
            : desc.mipLevels;

        uint32 w = desc.width, hgt = desc.height;
        for (uint32 m = 0; m < mipCount; m++) {
            const uint32 texels = math::NkMax(1u, w) * math::NkMax(1u, hgt) * layers;
            uint32 sz = texels * allocBpp;
            uint8 value = (uint8)(desc.format == NkGPUFormat::NK_D32_FLOAT ? 0x3F : 0);
            t.mips.EmplaceBack(sz, value);
            // Init depth à 1.0 (toutes les couches)
            if (desc.format == NkGPUFormat::NK_D32_FLOAT) {
                float32 one = 1.0f;
                for (uint32 i = 0; i < texels; i++)
                    memcpy(t.mips[m].Data() + i*4, &one, 4);
            }
            w >>= 1;
            hgt >>= 1;
        }

        if (desc.initialData) {
            uint32 rp = desc.rowPitch > 0 ? desc.rowPitch : desc.width * bpp;
            uint32 dataSz = math::NkMin((uint32)t.mips[0].Size(), rp * desc.height);
            memcpy(t.mips[0].Data(), desc.initialData, dataSz);
        }

        uint64 hid = NextId(); mTextures[hid] = traits::NkMove(t);
        NkTextureHandle textureHandle;
        textureHandle.id = hid;
        return textureHandle;
    }

    void NkSoftwareDevice::DestroyTexture(NkTextureHandle& h) {
        threading::NkScopedLockMutex lock(mMutex); mTextures.Erase(h.id); h.id = 0;
    }

    bool NkSoftwareDevice::WriteTexture(NkTextureHandle t, const void* p, uint32 rp) {
        auto* it = mTextures.Find(t.id); if (!it) return false;
        auto& tex = *it;
        uint32 bpp = tex.Bpp();
        uint32 stride = rp > 0 ? rp : tex.Width() * bpp;
        for (uint32 y = 0; y < tex.Height(); y++)
            memcpy(tex.mips[0].Data() + y * tex.Width() * bpp,
                (const uint8*)p + y * stride,
                tex.Width() * bpp);
        return true;
    }

    bool NkSoftwareDevice::WriteTextureRegion(NkTextureHandle t, const void* pixels,
        uint32 x, uint32 y, uint32 /*z*/, uint32 w, uint32 h, uint32 /*d2*/,
        uint32 mip, uint32 /*layer*/, uint32 rowPitch) {
        auto* it = mTextures.Find(t.id); if (!it) return false;
        auto& tex = *it;
        if (mip >= tex.mips.Size()) return false;
        uint32 bpp = tex.Bpp();
        uint32 stride = rowPitch > 0 ? rowPitch : w * bpp;
        for (uint32 row = 0; row < h; row++) {
            uint32 dstY = y + row;
            if (dstY >= tex.Height(mip)) break;
            memcpy(tex.mips[mip].Data() + (dstY * tex.Width(mip) + x) * bpp,
                (const uint8*)pixels + row * stride,
                math::NkMin(w * bpp, (tex.Width(mip) - x) * bpp));
        }
        return true;
    }

    bool NkSoftwareDevice::GenerateMipmaps(NkTextureHandle t, NkFilter f) {
        auto* it = mTextures.Find(t.id); if (!it) return false;
        auto& tex = *it;
        for (uint32 m = 1; m < (uint32)tex.mips.Size(); m++) {
            uint32 sw = tex.Width(m-1), sh = tex.Height(m-1);
            uint32 dw = tex.Width(m),   dh = tex.Height(m);
            for (uint32 y = 0; y < dh; y++)
                for (uint32 x = 0; x < dw; x++) {
                    NkSWColor c = tex.Sample((x+.5f)/dw, (y+.5f)/dh, m-1);
                    tex.Write(x, y, c, m);
                }
        }
        return true;
    }

    // =============================================================================
    // Samplers
    // =============================================================================
    NkSamplerHandle NkSoftwareDevice::CreateSampler(const NkSamplerDesc& d) {
        threading::NkScopedLockMutex lock(mMutex);
        uint64 hid = NextId(); mSamplers[hid] = { d };
        NkSamplerHandle h; h.id = hid; return h;
    }
    void NkSoftwareDevice::DestroySampler(NkSamplerHandle& h) {
        threading::NkScopedLockMutex lock(mMutex); mSamplers.Erase(h.id); h.id = 0;
    }

    // =============================================================================
    // Shaders
    // =============================================================================
    NkShaderHandle NkSoftwareDevice::CreateShader(const NkShaderDesc& desc) {
        threading::NkScopedLockMutex lock(mMutex);
        NkSWShader sh;

        for (uint32 i = 0; i < (uint32)desc.stages.Size(); i++) {
            const auto& s = desc.stages[i];

            // ── Cas SkSL (fonctions déjà compilées, stockées par valeur) ──────────
            // if (s.hasSwFn) {
            //     if (s.stage == NkShaderStage::NK_VERTEX   && s.swVertFn) sh.vertFn = s.swVertFn;
            //     if (s.stage == NkShaderStage::NK_FRAGMENT && s.swFragFn) sh.fragFn = s.swFragFn;
            //     continue;
            // }

            // ── Cas AddSWFn (ancien API via pointeur heap) ────────────────────────
            if (s.cpuFn == nullptr) continue;
            
            if (s.stage == NkShaderStage::NK_VERTEX) {
                sh.vertFn = *static_cast<const NkVertexShaderSoftware*>(s.cpuFn);
                nkentseu::memory::NkGetDefaultAllocator().Delete(
                    const_cast<NkVertexShaderSoftware*>(static_cast<const NkVertexShaderSoftware*>(s.cpuFn)));
            } else if (s.stage == NkShaderStage::NK_FRAGMENT) {
                sh.fragFn = *static_cast<const NkPixelShaderSoftware*>(s.cpuFn);
                nkentseu::memory::NkGetDefaultAllocator().Delete(
                    const_cast<NkPixelShaderSoftware*>(static_cast<const NkPixelShaderSoftware*>(s.cpuFn)));
            } else if (s.stage == NkShaderStage::NK_COMPUTE) {
                sh.isCompute = true;
                sh.computeFn = *static_cast<const NkComputeShaderSoftware*>(s.cpuFn);
                nkentseu::memory::NkGetDefaultAllocator().Delete(
                    const_cast<NkComputeShaderSoftware*>(static_cast<const NkComputeShaderSoftware*>(s.cpuFn)));
            }
        }

        // ── v5 Phase A : VM bytecode NkSL (opt-in NK_SW_VM) — exécute le VRAI shader ──
        // Compile swSource → NK_BYTECODE → NkSLVM. STOPGAP mono-UBO (env.uniforms = set0,b0) ;
        // le multi-UBO (Camera/Object/Lights) = Phase B. Si la VM installe vertFn/fragFn, le
        // fixed-function ci-dessous est court-circuité (garde !sh.vertFn).
        {
            const char* vSw = nullptr; const char* fSw = nullptr; bool hasCpu = false;
            for (uint32 i = 0; i < (uint32)desc.stages.Size(); ++i) {
                const auto& s = desc.stages[i];
                if (s.cpuFn) hasCpu = true;
                if (s.stage == NkShaderStage::NK_VERTEX   && s.swSource) vSw = s.swSource;
                if (s.stage == NkShaderStage::NK_FRAGMENT && s.swSource) fSw = s.swSource;
            }
            const char* vmEnv = std::getenv("NK_SW_VM");
            if (vmEnv && vmEnv[0] == '1' && !hasCpu && vSw && fSw) {
                NkSLCompiler& comp = nksl::GetCompiler();
                NkSLCompileOptions o; o.optimize = false; o.debugInfo = false;
                auto vr = comp.CompileWithReflection(NkString(vSw), NkSLStage::NK_VERTEX,   NkSLTarget::NK_BYTECODE, o, "sw.vert");
                auto fr = comp.CompileWithReflection(NkString(fSw), NkSLStage::NK_FRAGMENT, NkSLTarget::NK_BYTECODE, o, "sw.frag");
                if (!vr.result.success) for (auto& e : vr.result.errors) NK_SW_LOG("VM vert L%u: %s\n", e.line, e.message.CStr());
                if (!fr.result.success) for (auto& e : fr.result.errors) NK_SW_LOG("VM frag L%u: %s\n", e.line, e.message.CStr());
                if (vr.result.success && fr.result.success) {
                    auto& alloc = nkentseu::memory::NkGetDefaultAllocator();
                    NkSLByteProgram* vp = alloc.New<NkSLByteProgram>();
                    NkSLByteProgram* fp = alloc.New<NkSLByteProgram>();
                    bool dv = NkSLByteCodeDeserialize(vr.result.bytecode.Data(), vr.result.bytecode.Size(), *vp);
                    bool df = NkSLByteCodeDeserialize(fr.result.bytecode.Data(), fr.result.bytecode.Size(), *fp);
                    if (dv && df && vp->IsValid() && fp->IsValid()) {
                        sh.vmVert = vp; sh.vmFrag = fp;
                        NkSoftwareDevice* dev = this;
                        sh.vertFn = [dev, vp](const void* vdata, uint32 idx, const void*) -> NkVertexSoftware {
                            NkVertexSoftware out{};
                            float outBuf[64] = {0};
                            uint32 stride = dev->SwCurStride(); if (stride == 0) stride = 68u;
                            NkSLVMEnv env;
                            env.inputs   = (const float*)((const uint8*)vdata + (usize)idx * stride);
                            env.outputs  = outBuf;
                            env.vertexID = (int)idx;   // gl_VertexID (instanceID = 0 pour l'instant)
                            for (uint32 ub = 0; ub < (uint32)vp->uboBlocks.Size() && ub < 8; ++ub)   // multi-UBO + push
                                env.uboBlobs[ub] = (vp->uboBlocks[ub].set == 0xFFFFu)
                                                 ? dev->SwPushData()
                                                 : dev->SwGetUBOBytes(vp->uboBlocks[ub].set, vp->uboBlocks[ub].binding);
                            env.uniforms = env.uboBlobs[0];   // fallback bloc 0
                            NkSLVM::Execute(*vp, env);
                            out.position = { outBuf[0], outBuf[1], outBuf[2], outBuf[3] };
                            const uint32 nv = vp->outputFloats > 4 ? vp->outputFloats - 4 : 0;
                            for (uint32 k = 0; k < nv && k < 16; ++k) out.attrs[k] = outBuf[4 + k];
                            out.attrCount = nv;
                            return out;
                        };
                        sh.fragFn = [dev, fp](const NkVertexSoftware& frag, const void*, const void*) -> math::NkVec4 {
                            float outBuf[8] = {0};
                            NkSLVMEnv env;
                            env.inputs   = frag.attrs;
                            env.outputs  = outBuf;
                            for (uint32 ub = 0; ub < (uint32)fp->uboBlocks.Size() && ub < 8; ++ub)   // multi-UBO + push
                                env.uboBlobs[ub] = (fp->uboBlocks[ub].set == 0xFFFFu)
                                                 ? dev->SwPushData()
                                                 : dev->SwGetUBOBytes(fp->uboBlocks[ub].set, fp->uboBlocks[ub].binding);
                            env.uniforms = env.uboBlobs[0];   // fallback bloc 0
                            env.ctx      = dev;
                            env.sampleTex = [](void* c, int s, float u, float v, float, float o[4]) {
                                auto* d = (NkSoftwareDevice*)c;
                                const NkSWTexture* t = d->SwGetTexAt(2, (uint32)s);   // set material d'abord
                                if (!t) t = d->SwGetTexAt(0, (uint32)s);              // puis frame
                                if (t) { auto col = t->Sample(u, v, 0); o[0]=col.x; o[1]=col.y; o[2]=col.z; o[3]=col.w; }
                                else  { o[0]=o[1]=o[2]=o[3]=1.f; }
                            };
                            env.texSize = [](void*, int, float o[2]) { o[0]=o[1]=1.f; };
                            NkSLVM::Execute(*fp, env);
                            return { outBuf[0], outBuf[1], outBuf[2], outBuf[3] };
                        };
                        NK_SW_LOG("VM NkSL bytecode installe (vert outF=%u frag inF=%u samplers=%u).\n",
                                  vp->outputFloats, fp->inputFloats, (uint32)fp->samplers.Size());
                    } else {
                        alloc.Delete(vp); alloc.Delete(fp);
                        NK_SW_LOG("VM NkSL : deserialize KO -> fallback fixed-function.\n");
                    }
                }
            }
        }

        // ── v4 Phase 5 (A) : shaders NkSL de NKRenderer (source, sans cpuFn) ──────────
        // On installe un shader fixed-function TAILLÉ sur les conventions NKRenderer :
        //   vertex  : viewProj(set0,b0 @128) × model(set1,b1 @0) × aPos ; format 6 attributs.
        //   fragment: Lambert (lumière directionnelle) × albedo(set2,b1) × tint(set1,b1 @128) × couleur.
        // Le shader interroge le device par (set,binding) au shade-time (multi-descriptor-set).
        {
            bool hasSource = false, hasCpuFn = false;
            for (uint32 i = 0; i < (uint32)desc.stages.Size(); ++i) {
                const auto& s = desc.stages[i];
                if (s.cpuFn) hasCpuFn = true;
                if (s.glslSource || s.swSource || !s.spirvBinary.Empty()) hasSource = true;
            }

            // ── v4 Phase 5 (A) : passes PLEIN-ÉCRAN (post-process) ────────────────────────
            // Tonemap/Bloom/FXAA/SSAO dessinent un triangle plein-écran (NkVertex3D, aPos@0
            // déjà en NDC, aUV@36) et échantillonnent leur RT d'entrée au binding 0. Elles ne
            // doivent PAS subir viewProj×model (sinon triangle déformé -> écran noir). On leur
            // installe un fixed-function PASS-THROUGH : position brute + sample input, + tonemap
            // ACES basique pour la passe Tonemap. Détecté via debugName ('PP_*').
            bool isFullscreen = false, isTonemap = false, isSSAO = false, isSkippablePP = false;
            if (desc.debugName && desc.debugName[0]) {
                const char* dn = desc.debugName;
                if (std::strncmp(dn, "PP_", 3) == 0) isFullscreen = true;
                if (std::strstr(dn, "Tonemap")) isTonemap = true;
                if (std::strstr(dn, "SSAO"))    isSSAO    = true;   // SSAO brut (pas SSAOBlur -> copie)
                if (std::strstr(dn, "SSAOBlur")) isSSAO   = false;
                // PERF : SSAO/SSAOBlur/Bloom sont INUTILISÉS par notre tonemap (qui n'échantillonne
                // que le HDR au binding 0). Ces ~14 passes plein-écran coûtent cher pour rien en
                // software → on les SKIP (rasterisation coupée). On garde Tonemap + FXAA (image finale).
                if (std::strstr(dn, "SSAO") || std::strstr(dn, "Bloom")) isSkippablePP = true;
            }
            if (isSkippablePP && hasSource && !hasCpuFn && !sh.vertFn && !sh.fragFn) {
                sh.genVertsFromID = true;
                sh.vertFn = [](const void*, uint32, const void*) -> NkVertexSoftware {
                    NkVertexSoftware out{}; out.position = { 0.f, 0.f, 0.f, -1.f };   // clippé -> pas de rasterisation
                    return out;
                };
                sh.fragFn = [](const NkVertexSoftware&, const void*, const void*) -> math::NkVec4 { return {0,0,0,0}; };
                NK_SW_LOG("Shader NkSL plein-ecran '%s' -> SKIP (inutilise en software, gain perf).\n",
                          desc.debugName ? desc.debugName : "?");
            }
            if (isFullscreen && !isSkippablePP && hasSource && !hasCpuFn && !sh.vertFn && !sh.fragFn) {
                sh.genVertsFromID = true;   // triangle plein-écran synthétisé (tolère vdata==nullptr)
                NkSoftwareDevice* dev = this;
                auto rf = [](const uint8* p, uint32 o) { float f; memcpy(&f, p + o, 4); return f; };
                sh.vertFn = [dev](const void* vdata, uint32 idx, const void*) -> NkVertexSoftware {
                    float px, py, uu, vv;
                    if (!vdata) {
                        // Vertexless (gl_VertexID) : grand triangle couvrant l'écran.
                        // idx 0->(uv 0,0) 1->(2,0) 2->(0,2) ; pos = uv*2-1 -> (-1,-1),(3,-1),(-1,3)
                        uu = (idx == 1u) ? 2.f : 0.f;
                        vv = (idx == 2u) ? 2.f : 0.f;
                        px = uu * 2.f - 1.f;
                        py = vv * 2.f - 1.f;
                    } else {
                        uint32 stride = dev->SwCurStride(); if (stride == 0) stride = 56u;  // NkVertex3D
                        const uint8* v = (const uint8*)vdata + (uint64)idx * stride;
                        auto g = [&](uint32 o, float def) -> float {
                            if (o + 4u > stride) return def; float f; memcpy(&f, v + o, 4); return f; };
                        px=g(0,0.f); py=g(4,0.f);    // aPos.xy déjà en NDC
                        uu=g(36,0.f); vv=g(40,0.f);  // aUV (location 3, offset 36)
                    }
                    NkVertexSoftware out{};
                    out.position = { px, py, 0.f, 1.f };    // pass-through (pas de transform)
                    out.normal   = { 0.f, 0.f, 1.f };
                    out.uv       = { uu, vv };
                    out.color    = { 1.f, 1.f, 1.f, 1.f };
                    return out;
                };
                sh.fragFn = [dev, rf, isTonemap, isSSAO](const NkVertexSoftware& f, const void*, const void*) -> math::NkVec4 {
                    if (isSSAO) return { 1.f, 1.f, 1.f, 1.f };   // facteur AO neutre (pas d'occlusion)
                    const NkSWTexture* src = dev->SwGetTexAt(0, 0);  // RT d'entrée au binding 0 (modèle aplati)
                    if (!src) return { 0.f, 0.f, 0.f, 1.f };
                    // PERF : passe plein-écran 1:1 -> NEAREST (1 lecture) au lieu de Sample bilinéaire
                    // (4 lectures + lerp). Tonemap/FXAA copient la RT à l'identique -> nearest exact.
                    const uint32 sw=src->Width(0), shh=src->Height(0);
                    int tx=(int)(f.uv.x*sw), ty=(int)(f.uv.y*shh);
                    if(tx<0)tx=0; if(tx>=(int)sw)tx=(int)sw-1; if(ty<0)ty=0; if(ty>=(int)shh)ty=(int)shh-1;
                    NkSWColor c = src->Read((uint32)tx,(uint32)ty,0);
                    if (!isTonemap) return { c.r, c.g, c.b, c.a };  // FXAA/Bloom : copie directe
                    // ── BLOOM (approximation 1-passe, auto-contenu). La chaîne Dual-Kawase (12 passes)
                    // est skippée en software ; à la place on échantillonne le HDR autour du pixel sur
                    // 2 anneaux (16 taps nearest), on extrait la partie brillante (luma > seuil) et on
                    // l'ajoute au HDR AVANT l'ACES → halo autour des point lights / hautes lumières.
                    // N'a d'effet que grâce au RT float HDR (valeurs > 1). Toggle NK_SW_NOBLOOM=1.
                    static const bool s_noBloom = [](){ const char* e=std::getenv("NK_SW_NOBLOOM"); return e && e[0]=='1'; }();
                    if (!s_noBloom) {
                        auto readUV = [&](float u, float v)->NkSWColor {
                            int x=(int)(u*sw), y=(int)(v*shh);
                            if(x<0)x=0; if(x>=(int)sw)x=(int)sw-1; if(y<0)y=0; if(y>=(int)shh)y=(int)shh-1;
                            return src->Read((uint32)x,(uint32)y,0); };
                        const float TH = 2.2f;                  // seuil bright-pass (ne blooomer que les vraies hautes lumières)
                        const float radii[2] = { 0.006f, 0.014f };
                        const float rw[2]    = { 1.0f,   0.5f };
                        float br=0.f,bg=0.f,bb=0.f, wsum=0.f;
                        for (int ri=0; ri<2; ++ri) {
                            for (int i=0;i<8;++i) {
                                const float ang = ((float)i + (ri?0.5f:0.f)) * 0.7853981634f;
                                NkSWColor bc = readUV(f.uv.x + std::cos(ang)*radii[ri],
                                                      f.uv.y + std::sin(ang)*radii[ri]);
                                const float lum = bc.r*0.299f + bc.g*0.587f + bc.b*0.114f;
                                if (lum > TH) { const float k = (lum - TH) * rw[ri];
                                    br += bc.r*k; bg += bc.g*k; bb += bc.b*k; wsum += rw[ri]; }
                            }
                        }
                        if (wsum > 0.f) { const float inv = 1.f/wsum, strength = 0.45f;
                            c.r += br*inv*strength; c.g += bg*inv*strength; c.b += bb*inv*strength; }
                    }
                    // Tonemap : ACES + exposure/gamma depuis les push constants PC[0]=(exposure,gamma,..).
                    // Bornés à des plages saines : des push constants absentes/garbage sur-exposaient
                    // le fond (clear sombre {0.05} -> lavande).
                    float exposure = 1.f, gamma = 2.2f;
                    if (const uint8* pc = dev->SwPushData()) { exposure = rf(pc, 0); gamma = rf(pc, 4); }
                    if (!(exposure > 0.05f && exposure < 8.f))  exposure = 1.f;
                    if (!(gamma    > 1.0f  && gamma    < 3.0f)) gamma    = 2.2f;
                    auto aces = [](float x){ const float a=2.51f,b=0.03f,c2=2.43f,d=0.59f,e=0.14f;
                        float y=(x*(a*x+b))/(x*(c2*x+d)+e); return y<0.f?0.f:(y>1.f?1.f:y); };
                    float r=aces(c.r*exposure), g=aces(c.g*exposure), b=aces(c.b*exposure);
                    if (gamma > 0.001f) { const float ig=1.f/gamma; r=std::pow(r,ig); g=std::pow(g,ig); b=std::pow(b,ig); }
                    return { r, g, b, 1.f };
                };
                NK_SW_LOG("Shader NkSL plein-ecran '%s' -> fixed-function pass-through (sample input @b0%s).\n",
                          desc.debugName ? desc.debugName : "?", isTonemap ? " + tonemap ACES" : "");
            }

            // ── v4 Phase 5 (B) : SKYBOX ───────────────────────────────────────────────────
            // La skybox est un triangle plein-écran (gl_VertexID) rendu au FAR plane avec depth
            // LEQUAL + depthWrite=false : elle ne remplit QUE le fond (là où depth==1). Le
            // fixed-function 3D lui donnait une profondeur proche -> elle occultait la scène
            // (grand plan sombre). On force donc depth=1 (position z=w) + un ciel dégradé neutre.
            const bool isSkybox = (desc.debugName && std::strstr(desc.debugName, "Skybox"));
            if (isSkybox && hasSource && !hasCpuFn && !sh.vertFn && !sh.fragFn) {
                sh.genVertsFromID = true;
                NkSoftwareDevice* dev = this;
                sh.vertFn = [dev](const void* vdata, uint32 idx, const void*) -> NkVertexSoftware {
                    float px, py, uu, vv;
                    if (!vdata) { uu=(idx==1u)?2.f:0.f; vv=(idx==2u)?2.f:0.f; px=uu*2.f-1.f; py=vv*2.f-1.f; }
                    else { uint32 st=dev->SwCurStride(); if(st==0)st=56u; const uint8* v=(const uint8*)vdata+(uint64)idx*st;
                           auto g=[&](uint32 o,float d){ if(o+4u>st)return d; float f; memcpy(&f,v+o,4); return f; };
                           px=g(0,0.f); py=g(4,0.f); uu=g(36,0.f); vv=g(40,0.f); }
                    NkVertexSoftware out{};
                    out.position = { px, py, 1.f, 1.f };   // z=w -> depth=1 (far plane) : n'occulte rien
                    out.normal = {0.f,0.f,1.f}; out.uv = { uu, vv }; out.color = {1.f,1.f,1.f,1.f};
                    return out;
                };
                sh.fragFn = [](const NkVertexSoftware& f, const void*, const void*) -> math::NkVec4 {
                    // Ciel dégradé neutre (gris-bleu clair). uv.y : 0=bas/horizon, 1=haut/zénith.
                    // Volontairement quasi-neutre (peu sensible au swap R/B en attendant son fix).
                    float t = f.uv.y; if (t<0.f)t=0.f; if (t>1.f)t=1.f;
                    float top=0.62f, hor=0.86f; float g = hor + (top-hor)*t;
                    return { g*0.92f, g*0.96f, g, 1.f };
                };
                NK_SW_LOG("Shader NkSL 'Skybox' -> fixed-function ciel far-plane (non-occultant).\n");
            }

            // ── InfiniteGrid : grille de sol infinie. Triangle plein-écran + reconstruction de
            // rayon par pixel : on intersecte le rayon caméra avec le plan y=planeY et on dessine
            // cellules + lignes + axes. Rendu au far plane (depth=1) -> correctement occulté par
            // les objets opaques (depthTest LEQUAL). Push constants = GridPC (6 vec4).
            const bool isGrid = (desc.debugName && std::strstr(desc.debugName, "InfiniteGrid"));
            if (isGrid && hasSource && !hasCpuFn && !sh.vertFn && !sh.fragFn) {
                sh.genVertsFromID = true;
                sh.fragDepth      = true;   // depth = point d'intersection sol (coplanaire au sol)
                NkSoftwareDevice* dev = this;
                sh.vertFn = [dev](const void* vdata, uint32 idx, const void*) -> NkVertexSoftware {
                    float px, py, uu, vv;
                    if (!vdata) { uu=(idx==1u)?2.f:0.f; vv=(idx==2u)?2.f:0.f; px=uu*2.f-1.f; py=vv*2.f-1.f; }
                    else { uint32 st=dev->SwCurStride(); if(st==0)st=56u; const uint8* v=(const uint8*)vdata+(uint64)idx*st;
                           auto g=[&](uint32 o,float d){ if(o+4u>st)return d; float f; memcpy(&f,v+o,4); return f; };
                           px=g(0,0.f); py=g(4,0.f); uu=g(36,0.f); vv=g(40,0.f); }
                    NkVertexSoftware out{};
                    out.position = { px, py, 1.f, 1.f };   // far plane (depth=1) -> occulté par les objets
                    out.normal = {0.f,0.f,1.f}; out.uv = { uu, vv }; out.color = {1.f,1.f,1.f,1.f};
                    return out;
                };
                sh.fragFn = [dev](const NkVertexSoftware& f, const void*, const void*) -> math::NkVec4 {
                    static const bool s_nogrid = [](){ const char* e=std::getenv("NK_SW_NOGRID"); return e&&e[0]=='1'; }();
                    if (s_nogrid) return { 0.f,0.f,0.f,0.f };   // A/B perf : coût de la grille
                    const uint8* cam = dev->SwGetUBOBytes(0,0);   // CameraUBO
                    const uint8* pc  = dev->SwPushData();          // GridPC
                    if (!cam) return { 0.f,0.f,0.f,0.f };
                    auto rf = [](const uint8* p, uint32 o){ float v; memcpy(&v,p+o,4); return v; };
                    // NDC du pixel (uv 0..1 -> NDC -1..1 ; uv.y=0 en haut).
                    // NDC du pixel. Le rasterizer software mappe NDC y=+1 -> HAUT de l'écran, et
                    // pour ce triangle plein-écran uv.y=1 est en haut -> ndcy = uv.y*2-1 (et NON
                    // 1-uv.y*2, qui inversait le rayon en Y => grille vue « d'en dessous »).
                    float ndcx = f.uv.x*2.f - 1.f, ndcy = f.uv.y*2.f - 1.f;
                    // Rayon monde : origine = camPos (@256), direction via le point far reconstruit
                    // (invViewProj @192, ndc z=+1). Robuste vis-à-vis de la convention NDC z.
                    const float* ivp  = (const float*)(cam + 192);
                    const float* cpos = (const float*)(cam + 256);
                    float pf[4];
                    for (int i=0;i<4;++i) pf[i]=ivp[0*4+i]*ndcx+ivp[1*4+i]*ndcy+ivp[2*4+i]*1.f+ivp[3*4+i]*1.f;
                    if (fabsf(pf[3])<1e-9f) return {0,0,0,0};
                    float ox=cpos[0], oy=cpos[1], oz=cpos[2];
                    float fx=pf[0]/pf[3], fy=pf[1]/pf[3], fz=pf[2]/pf[3];
                    float dx=fx-ox, dy=fy-oy, dz=fz-oz;
                    const float planeY = pc ? rf(pc, 80) : 0.f;    // extra.x
                    if (fabsf(dy) < 1e-6f) return {0,0,0,0};
                    float t = (planeY - oy) / dy;
                    if (t <= 0.f) return {0,0,0,0};                // plan derrière / au-dessus -> ciel
                    float wx = ox + dx*t, wz = oz + dz*t;          // point d'intersection sol
                    // Profondeur du point d'intersection (viewProj @128) -> depth [0,1] pour le
                    // depth-test : la grille devient coplanaire au sol et est occultée par les objets.
                    { const float* vp = (const float*)(cam + 128);
                      float cz = vp[0*4+2]*wx + vp[1*4+2]*planeY + vp[2*4+2]*wz + vp[3*4+2];
                      float cw = vp[0*4+3]*wx + vp[1*4+3]*planeY + vp[2*4+3]*wz + vp[3*4+3];
                      if (fabsf(cw) > 1e-9f) {
                          float d = (cz/cw)*0.5f + 0.5f - 0.0015f;   // biais vers la caméra (anti z-fight sol)
                          swraster::tl_fragDepth = d < 0.f ? 0.f : (d > 1.f ? 1.f : d);
                          swraster::tl_fragDepthSet = true;
                      } }
                    // Paramètres grille (défauts raisonnables si pas de push const).
                    float cellSize  = pc ? rf(pc,64) : 1.f;
                    float majorEvery= pc ? rf(pc,68) : 10.f;
                    float fadeEnd   = pc ? rf(pc,76) : 60.f;
                    if (cellSize   < 1e-3f) cellSize = 1.f;
                    if (majorEvery < 1.f)   majorEvery = 10.f;
                    math::NkVec4 lineCol = pc ? math::NkVec4{rf(pc,0),rf(pc,4),rf(pc,8),rf(pc,12)} : math::NkVec4{0.8f,0.8f,0.8f,1.f};
                    math::NkVec4 cellCol = pc ? math::NkVec4{rf(pc,16),rf(pc,20),rf(pc,24),rf(pc,28)} : math::NkVec4{0.55f,0.55f,0.58f,0.6f};
                    math::NkVec4 axXCol  = pc ? math::NkVec4{rf(pc,32),rf(pc,36),rf(pc,40),rf(pc,44)} : math::NkVec4{0.85f,0.25f,0.25f,1.f};
                    math::NkVec4 axZCol  = pc ? math::NkVec4{rf(pc,48),rf(pc,52),rf(pc,56),rf(pc,60)} : math::NkVec4{0.25f,0.4f,0.85f,1.f};
                    // Distance à la ligne la plus proche (largeur world proportionnelle à la distance
                    // pour garder ~une largeur écran constante ; pas de dérivées en software).
                    float dist = t; if (dist<0.f) dist=0.f;
                    float lw = 0.02f * dist * cellSize / 20.f; if (lw < 0.008f*cellSize) lw = 0.008f*cellSize;
                    auto lineFactor = [&](float cs)->float{
                        float ax = fabsf(wx/cs - math::NkFloor(wx/cs + 0.5f)) * cs;
                        float az = fabsf(wz/cs - math::NkFloor(wz/cs + 0.5f)) * cs;
                        float d = ax < az ? ax : az;
                        return 1.f - math::NkClamp(d / lw, 0.f, 1.f);
                    };
                    float minorL = lineFactor(cellSize);
                    float majorL = lineFactor(cellSize * majorEvery);
                    // Fade distance.
                    float fade = 1.f - math::NkClamp(dist / fadeEnd, 0.f, 1.f);
                    if (fade <= 0.f) return {0,0,0,0};
                    // Composition : cellule (fond) -> lignes mineures -> majeures -> axes.
                    math::NkVec4 col = cellCol;
                    if (minorL > 0.01f) col = { lineCol.x, lineCol.y, lineCol.z, math::NkMax(col.w, lineCol.w*minorL) };
                    if (majorL > 0.01f) col = { lineCol.x, lineCol.y, lineCol.z, math::NkMax(col.w, lineCol.w*majorL) };
                    float axw = lw * 1.5f;
                    if (fabsf(wx) < axw) col = { axZCol.x, axZCol.y, axZCol.z, math::NkMax(col.w, axZCol.w) }; // axe Z (x=0)
                    if (fabsf(wz) < axw) col = { axXCol.x, axXCol.y, axXCol.z, math::NkMax(col.w, axXCol.w) }; // axe X (z=0)
                    col.w *= fade;
                    return col;
                };
                NK_SW_LOG("Shader NkSL 'InfiniteGrid' -> fixed-function grille sol (ray-plane).\n");
            }

            // ── DebugLine / DebugLineOverlay : géométrie de debug (axes, gizmo) au format LIGNE
            // (pos vec3 @0 + couleur vec4 @12, stride 28). Le rasterizer software ne gère pas la
            // topologie LINE : il remplissait les « lignes » en gros triangles pleins (les axes
            // X rouge / Z bleu devenaient d'énormes bandes occultant la scène — d'abord magenta
            // via le fixed-function 3D). En attendant une vraie rasterisation de lignes, on NE
            // dessine PAS le debug en software (sommet clippé). La scène reste dégagée.
            // ── Passe SHADOW (rendu depth des casters dans le shadow atlas VSM). Maintenant que le
            // rasterizer software gère le VIEWPORT (chaque slot rasterise dans son tileRect au lieu
            // de tout l'atlas 4096²) ET que le fragment géométrie échantillonne l'atlas (ombres
            // portées), on RÉTABLIT ce rendu. Le FBO est depth-only (colorBuf=null) : le rasterizer
            // écrit seulement la depth, sans appeler le fragFn (chemin `depthOnly`). Sommet :
            // clip = lightVP(push const @0) · model(ObjUBO set1,b1 @0) · localPos. depthRemap=1 en
            // software (renderMatrix en [-1,1]) → Project mappe *0.5+0.5, cohérent avec le sampling.
            //
            // Le shader INSTANCIÉ ("ShadowInstanced") reste SKIP : l'instancing n'est pas géré en
            // software (le vertFn ne reçoit pas gl_InstanceID), donc les N cubes se rendraient tous à
            // l'origine (model=identité, offset par instance ignoré) → un blob d'ombre parasite. On
            // le clippe. Les casters NON-instanciés (sphères, colonnes, cube central) passent par
            // "Shadow_DepthOnly" (RenderShadowPass itère mShadowCasters avec un ObjUBO par caster).
            const bool isShadowInst = (desc.debugName && std::strstr(desc.debugName, "ShadowInstanced"));
            const bool isShadowPass = (desc.debugName && std::strstr(desc.debugName, "Shadow"));
            if (isShadowPass && hasSource && !hasCpuFn && !sh.vertFn && !sh.fragFn) {
                if (isShadowInst) {
                    // Ombres des casters INSTANCIÉS (cubes) : le pass shadow instancié rend n instances
                    // en 1 draw. model par instance = InstanceUBO(set1,b4) @ instIdx*64 (mat4 col-major) ;
                    // l'ObjectUBO(1,1) est l'identité (le VS GPU fait uObj.model·inst[id]·pos). On indexe
                    // via SwCurInstance() ; ExecuteDraw*Fast re-génère les sommets par instance grâce à
                    // usesInstancing=true. clip = lightVP(pushConst@0) · instModel · localPos.
                    NkSoftwareDevice* dev = this;
                    sh.usesInstancing = true;
                    sh.vertFn = [dev](const void* vdata, uint32 idx, const void*) -> NkVertexSoftware {
                        uint32 stride = dev->SwCurStride(); if (stride == 0) stride = 12u;
                        const uint8* v = (const uint8*)vdata + (uint64)idx * stride;
                        float px=0.f,py=0.f,pz=0.f;
                        if (stride >= 12u) { memcpy(&px,v,4); memcpy(&py,v+4,4); memcpy(&pz,v+8,4); }
                        float wx=px, wy=py, wz=pz, ww=1.f;
                        if (const uint8* inst = dev->SwGetUBOBytes(1,4)) {          // InstanceUBO
                            const float* m = (const float*)(inst + (uint64)dev->SwCurInstance()*64u);
                            wx=m[0]*px+m[4]*py+m[ 8]*pz+m[12];
                            wy=m[1]*px+m[5]*py+m[ 9]*pz+m[13];
                            wz=m[2]*px+m[6]*py+m[10]*pz+m[14];
                            ww=m[3]*px+m[7]*py+m[11]*pz+m[15];
                        }
                        float cx=wx, cy=wy, cz=wz, cw=ww;
                        if (const uint8* pc = dev->SwPushData()) {                  // lightVP
                            const float* L = (const float*)pc;
                            cx=L[0]*wx+L[4]*wy+L[ 8]*wz+L[12]*ww;
                            cy=L[1]*wx+L[5]*wy+L[ 9]*wz+L[13]*ww;
                            cz=L[2]*wx+L[6]*wy+L[10]*wz+L[14]*ww;
                            cw=L[3]*wx+L[7]*wy+L[11]*wz+L[15]*ww;
                        }
                        NkVertexSoftware out{}; out.position = { cx, cy, cz, cw };
                        return out;
                    };
                    sh.fragFn = [](const NkVertexSoftware&, const void*, const void*) -> math::NkVec4 { return {0,0,0,1}; };
                    NK_SW_LOG("Shader NkSL '%s' -> fixed-function depth-only INSTANCIE (ombres cubes).\n",
                              desc.debugName ? desc.debugName : "ShadowInstanced");
                } else {
                    NkSoftwareDevice* dev = this;
                    sh.vertFn = [dev](const void* vdata, uint32 idx, const void*) -> NkVertexSoftware {
                        uint32 stride = dev->SwCurStride(); if (stride == 0) stride = 12u;
                        const uint8* v = (const uint8*)vdata + (uint64)idx * stride;
                        float px=0.f,py=0.f,pz=0.f;
                        if (stride >= 12u) { memcpy(&px,v,4); memcpy(&py,v+4,4); memcpy(&pz,v+8,4); }
                        // model (ObjUBO set1,b1 @0) : localPos -> worldPos
                        float wx=px, wy=py, wz=pz, ww=1.f;
                        if (const uint8* obj = dev->SwGetUBOBytes(1,1)) {
                            const float* m = (const float*)obj;
                            wx=m[0]*px+m[4]*py+m[ 8]*pz+m[12];
                            wy=m[1]*px+m[5]*py+m[ 9]*pz+m[13];
                            wz=m[2]*px+m[6]*py+m[10]*pz+m[14];
                            ww=m[3]*px+m[7]*py+m[11]*pz+m[15];
                        }
                        // lightVP (push const @0) : worldPos -> clip lumière
                        float cx=wx, cy=wy, cz=wz, cw=ww;
                        if (const uint8* pc = dev->SwPushData()) {
                            const float* L = (const float*)pc;
                            cx=L[0]*wx+L[4]*wy+L[ 8]*wz+L[12]*ww;
                            cy=L[1]*wx+L[5]*wy+L[ 9]*wz+L[13]*ww;
                            cz=L[2]*wx+L[6]*wy+L[10]*wz+L[14]*ww;
                            cw=L[3]*wx+L[7]*wy+L[11]*wz+L[15]*ww;
                        }
                        NkVertexSoftware out{}; out.position = { cx, cy, cz, cw };
                        return out;
                    };
                    sh.fragFn = [](const NkVertexSoftware&, const void*, const void*) -> math::NkVec4 { return {0,0,0,1}; };
                    NK_SW_LOG("Shader NkSL '%s' -> fixed-function depth-only (rendu atlas d'ombre VSM).\n",
                              desc.debugName ? desc.debugName : "Shadow");
                }
            }

            const bool isDebugLine = (desc.debugName && std::strstr(desc.debugName, "DebugLine"));
            if (isDebugLine && hasSource && !hasCpuFn && !sh.vertFn && !sh.fragFn) {
                sh.vertFn = [](const void*, uint32, const void*) -> NkVertexSoftware {
                    NkVertexSoftware out{}; out.position = { 0.f, 0.f, 0.f, -1.f };   // w<0 -> clippé
                    return out;
                };
                sh.fragFn = [](const NkVertexSoftware&, const void*, const void*) -> math::NkVec4 {
                    return { 0.f, 0.f, 0.f, 0.f };
                };
                NK_SW_LOG("Shader NkSL '%s' -> non dessine en software (lignes debug, topologie LINE non geree).\n",
                          desc.debugName ? desc.debugName : "DebugLine");
            }

            // ── Render2D / overlay / texte : quads 2D. Format aPos(vec2@0) aUV(vec2@8)
            // aColor(uint@16, RGBA packé) aFlags(uint@20), stride 24. gl_Position = ortho(push
            // const mat4 @0) × pos. FS : flag2=texturé (atlas @binding0), flag1=texte (couverture
            // = atlas.a × couleur), sinon quad plein = couleur. Sans ce handler, l'overlay/HUD
            // était rendu par le fixed-function 3D (format incompatible) -> cassé.
            const bool isR2D = (desc.debugName && (std::strstr(desc.debugName,"Render2D") ||
                                                    std::strstr(desc.debugName,"Overlay")));
            if (isR2D && hasSource && !hasCpuFn && !sh.vertFn && !sh.fragFn) {
                NkSoftwareDevice* dev = this;
                sh.vertFn = [dev](const void* vdata, uint32 idx, const void*) -> NkVertexSoftware {
                    uint32 stride = dev->SwCurStride(); if (stride == 0) stride = 24u;
                    const uint8* v = (const uint8*)vdata + (uint64)idx * stride;
                    auto gf = [&](uint32 o, float d){ if(o+4u>stride)return d; float f; memcpy(&f,v+o,4); return f; };
                    float px=gf(0,0.f), py=gf(4,0.f), uu=gf(8,0.f), vv=gf(12,0.f);
                    uint32 col=0, flg=0;
                    if (16u+4u<=stride) memcpy(&col, v+16, 4);
                    if (20u+4u<=stride) memcpy(&flg, v+20, 4);
                    float cr=(col&0xFFu)/255.f, cg=((col>>8)&0xFFu)/255.f, cb=((col>>16)&0xFFu)/255.f, ca=((col>>24)&0xFFu)/255.f;
                    // ortho (mat4 column-major) dans les push constants @0.
                    float clip[4]={px,py,0.f,1.f};
                    if (const uint8* pc=dev->SwPushData()) { const float* m=(const float*)pc;
                        clip[0]=m[0]*px+m[4]*py+m[12]; clip[1]=m[1]*px+m[5]*py+m[13];
                        clip[2]=m[2]*px+m[6]*py+m[14]; clip[3]=m[3]*px+m[7]*py+m[15]; }
                    NkVertexSoftware out{};
                    out.position={clip[0],clip[1],clip[2],clip[3]};
                    out.uv={uu,vv}; out.color={cr,cg,cb,ca};
                    out.attrs[0]=(float)flg; out.attrCount=1;   // flags (flat)
                    return out;
                };
                sh.fragFn = [dev](const NkVertexSoftware& f, const void*, const void*) -> math::NkVec4 {
                    const uint32 flg=(uint32)(f.attrs[0]+0.5f);
                    float r=f.color.r, g=f.color.g, b=f.color.b, a=f.color.a;
                    if (flg & 2u) {   // texturé (atlas @binding0)
                        const NkSWTexture* at = dev->SwGetTexAt(0,0);
                        if (at && !at->mips.Empty()) {
                            NkSWColor t = at->Sample(f.uv.x, f.uv.y);
                            if (flg & 1u) { a *= t.a; }               // texte : couverture = alpha atlas
                            else { r*=t.r; g*=t.g; b*=t.b; a*=t.a; }  // quad texturé
                        }
                    }
                    if (a < 0.01f) return { 0.f,0.f,0.f,0.f };        // discard (alpha 0 -> blend no-op)
                    return { r,g,b,a };
                };
                NK_SW_LOG("Shader NkSL '%s' -> fixed-function 2D (overlay/texte, ortho+atlas).\n",
                          desc.debugName ? desc.debugName : "Render2D");
            }

            if (hasSource && !hasCpuFn && !sh.vertFn && !sh.fragFn) {
                NkSoftwareDevice* dev = this;
                auto rf = [](const uint8* p, uint32 o) { float f; memcpy(&f, p + o, 4); return f; };
                auto mulCM = [](const float* m, float x, float y, float z, float w, float* o) {
                    o[0]=m[0]*x+m[4]*y+m[ 8]*z+m[12]*w; o[1]=m[1]*x+m[5]*y+m[ 9]*z+m[13]*w;
                    o[2]=m[2]*x+m[6]*y+m[10]*z+m[14]*w; o[3]=m[3]*x+m[7]*y+m[11]*z+m[15]*w;
                };
                sh.vertFn = [dev, rf, mulCM](const void* vdata, uint32 idx, const void*) -> NkVertexSoftware {
                    uint32 stride = dev->SwCurStride(); if (stride == 0) stride = 12u;
                    const uint8* v = (const uint8*)vdata + (uint64)idx * stride;
                    // Lecture BORNÉE par le stride réel (évite tout OOB). Layout NKRenderer :
                    // aPos@0 aNormal@12 aTangent@24 aUV@36 aUV2@44 aColor@52.
                    auto g = [&](uint32 o, float def) -> float { return (o + 4u <= stride) ? rf(v, o) : def; };
                    const float px=g(0,0.f), py=g(4,0.f), pz=g(8,0.f);
                    const float nx=g(12,0.f), ny=g(16,0.f), nz=g(20,1.f);
                    const float uu=g(36,0.f), vv=g(40,0.f);
                    // Couleur : NkVertex3D (stride 56) la stocke PACKÉE en uint RGBA @52 (4 octets),
                    // PAS en vec4 float. La lire comme float donnait un bitpattern d'uint interprété
                    // en float = NaN (ex. blanc 0xFFFFFFFF) -> albédo NaN -> canal rouge tué (sol/objets
                    // cyan). On la DÉPAQUÈTE. (Le format 68 o à vec4 float n'est pas utilisé ici.)
                    float cr=1.f,cg=1.f,cb=1.f,ca=1.f;
                    if (52u+4u <= stride) { uint32 col; memcpy(&col, v+52, 4);
                        cr=(col&0xFFu)/255.f; cg=((col>>8)&0xFFu)/255.f; cb=((col>>16)&0xFFu)/255.f; ca=((col>>24)&0xFFu)/255.f; }

                    const uint8* cam = dev->SwGetUBOBytes(0,0);   // CameraUBO (set0,binding0)
                    const uint8* obj = dev->SwGetUBOBytes(1,1);   // ObjectUBO (set1,binding1 : NKRenderer BindUniformBuffer(os,1,...))
                    float world[4]={px,py,pz,1.f};
                    float wnx=nx,wny=ny,wnz=nz;
                    if (obj) {
                        const float* model=(const float*)(obj+0);
                        mulCM(model, px,py,pz,1.f, world);
                        wnx=model[0]*nx+model[4]*ny+model[8]*nz;
                        wny=model[1]*nx+model[5]*ny+model[9]*nz;
                        wnz=model[2]*nx+model[6]*ny+model[10]*nz;
                    }
                    float clip[4]={world[0],world[1],world[2],1.f};
                    if (cam) { const float* vp=(const float*)(cam+128); mulCM(vp, world[0],world[1],world[2],world[3], clip); }

                    NkVertexSoftware out{};
                    out.position = { clip[0],clip[1],clip[2],clip[3] };
                    out.normal   = { wnx,wny,wnz };
                    out.uv       = { uu,vv };
                    out.color    = { cr,cg,cb,ca };
                    out.attrs[0]=world[0]; out.attrs[1]=world[1]; out.attrs[2]=world[2]; out.attrCount=3;  // pos monde -> vue (spéculaire)
                    return out;
                };
                sh.fragFn = [dev, rf](const NkVertexSoftware& f, const void*, const void*) -> math::NkVec4 {
                    // PERF : résout les bindings UNE FOIS PAR DRAW (pas par pixel). Les pointeurs UBO/texture
                    // sont constants pendant la rasterisation d'un draw ; SwBindGen() n'est incrémenté qu'au
                    // changement de descriptor set (donc ~1×/objet). Sur le grand sol (~900k px) on passait
                    // ~6 lookups (map/scan) PAR PIXEL → désormais 1 comparaison de génération par pixel.
                    struct FragBind { uint64 gen; const uint8 *obj,*cam,*lu,*su; const NkSWTexture *alb,*atlas; };
                    static thread_local FragBind fb{ ~0ull, nullptr,nullptr,nullptr,nullptr, nullptr,nullptr };
                    const uint64 bgen = dev->SwBindGen();
                    if (fb.gen != bgen) {
                        fb.gen = bgen;
                        fb.obj   = dev->SwGetUBOBytes(1,1);   // ObjectUBO (model/tint/metal/rough)
                        fb.cam   = dev->SwGetUBOBytes(0,0);   // CameraUBO (camPos @256)
                        fb.lu    = dev->SwGetUBOBytes(0,2);   // LightsUBO
                        fb.su    = dev->SwGetUBOBytes(0,3);   // ShadowSlots UBO
                        fb.alb   = dev->SwFindTexInSet(2,1);  // albedo
                        fb.atlas = dev->SwFindTexInSet(0,11); if (!fb.atlas) fb.atlas = dev->SwFindTexInSet(0,12);
                    }
                    float nx=f.normal.x, ny=f.normal.y, nz=f.normal.z;
                    const float len=std::sqrt(nx*nx+ny*ny+nz*nz); if (len>1e-6f){nx/=len;ny/=len;nz/=len;}
                    // ── Albédo = vertex color × tint (+ texture) ; matériau metallic/roughness ──
                    float cr=f.color.r, cg=f.color.g, cb=f.color.b, ca=f.color.a;
                    float metal=0.f, rough=0.5f;
                    if (const uint8* obj=fb.obj) {   // tint @128, metallic @144, roughness @148
                        cr*=rf(obj,128); cg*=rf(obj,132); cb*=rf(obj,136); ca*=rf(obj,140);
                        metal=rf(obj,144); rough=rf(obj,148);
                        if (!(metal>=0.f&&metal<=1.f)) metal=0.f; if (!(rough>0.f&&rough<=1.f)) rough=0.5f;
                    }
                    if (const NkSWTexture* alb=fb.alb) {   // albedo (match exact set2,b1)
                        if (!alb->mips.Empty()) {
                            const uint32 w=alb->Width(0),h=alb->Height(0),bpp=alb->Bpp();
                            float su=f.uv.x-std::floor(f.uv.x), sv=f.uv.y-std::floor(f.uv.y);
                            int tx=(int)(su*w), ty=(int)(sv*h);
                            if(tx<0)tx=0; if(tx>=(int)w)tx=(int)w-1; if(ty<0)ty=0; if(ty>=(int)h)ty=(int)h-1;
                            const uint8* tp=alb->mips[0].Data()+((uint32)ty*w+(uint32)tx)*bpp;
                            if (bpp>=3){ cr*=tp[0]/255.f; cg*=tp[1]/255.f; cb*=tp[2]/255.f; }
                        }
                    }
                    // Direction de vue (depuis camPos @256).
                    float vx=0.f,vy=0.f,vz=1.f;
                    if (const uint8* cam=fb.cam) {
                        const float* cp=(const float*)(cam+256);
                        vx=cp[0]-f.attrs[0]; vy=cp[1]-f.attrs[1]; vz=cp[2]-f.attrs[2];
                        float vl=std::sqrt(vx*vx+vy*vy+vz*vz); if(vl>1e-6f){vx/=vl;vy/=vl;vz/=vl;}
                    }
                    const float kd = 1.f - 0.7f*metal;
                    const float f0 = 0.04f;
                    const float shin = 4.f + 180.f*(1.f-rough)*(1.f-rough);
                    // ── Éclairage : VRAIES lumières du LightsUBO (set0,binding2). Layout :
                    //    pos[32]@0 {xyz,type}, color[32]@512 {rgb,intensity}, dir[32]@1024 {xyz,range},
                    //    angles[32]@1536 {cosInner,cosOuter,shadow,cookie}, count@2048.
                    //    directionnelle(type0) L=-dir ; point(1)/spot(2) L=pos-worldPos + atténuation.
                    // Ambiant HÉMISPHÉRIQUE (approx IBL) : ciel (normale vers le haut) légèrement
                    // bleuté, sol (vers le bas) sombre et chaud — plus naturel que le plat 0.20.
                    const float up = ny*0.5f + 0.5f;                       // world normal.y -> [0,1]
                    const float ambR = 0.10f + (0.30f-0.10f)*up;
                    const float ambG = 0.10f + (0.34f-0.10f)*up;
                    const float ambB = 0.09f + (0.42f-0.09f)*up;
                    float litR=cr*ambR, litG=cg*ambG, litB=cb*ambB;
                    // ── Ombres portées VSM : slots UBO @(0,3), atlas depth D32 @(0,11). Le fragment
                    // se projette dans le(s) slot(s) de la lumière via shadowMatrix (==renderMatrix),
                    // échantillonne l'atlas et compare la profondeur (PCF 3×3). depthRemap=1 en
                    // software → fragD = ndcz*0.5+0.5 (idem Project côté rasterizer). Retourne un
                    // facteur [0,1] (1=éclairé, 0=ombre). Voir NkVirtualShadowMaps (layout std140).
                    const uint8* su = fb.su;
                    const NkSWTexture* atlas = fb.atlas;
                    static const bool s_noShadow = [](){ const char* e=std::getenv("NK_SW_NOSHADOW"); return e && e[0]=='1'; }();  // diag perf
                    const bool hasShadows = !s_noShadow && su && atlas && !atlas->mips.Empty();
                    // ndl = N·L de la lumière (pour le slope bias). Parité GPU : biais normal (offset
                    // le long de N, côté récepteur) + biais de profondeur slope-scaled (÷ N·L).
                    auto sampleShadow = [&](int lightIdx, float ndl)->float {
                        if (!hasShadows || lightIdx<0 || lightIdx>=32) return 1.f;
                        const float* gcfg   = (const float*)(su + 28928u);  // .x=numSlots .z=depthRemap
                        const float* biasP  = (const float*)(su + 28944u);  // .x=shadowBias .y=normalBias
                        const float* firstA = (const float*)(su + 28672u);  // firstSlotPerLight (packé vec4)
                        const float* countA = (const float*)(su + 28800u);  // slotCountPerLight
                        const int numSlots  = (int)gcfg[0];
                        if (numSlots<=0) return 1.f;
                        const float depthRemap = gcfg[2];
                        const int first = (int)firstA[lightIdx];
                        const int count = (int)countA[lightIdx];
                        if (first<0 || count<=0) return 1.f;
                        // Biais de profondeur SLOPE-SCALED : plus la surface est rasante (N·L faible),
                        // plus l'écart de profondeur par texel est grand → biais ∝ 1/(N·L), borné.
                        const float shBias0 = (biasP[0]>0.f) ? biasP[0] : 0.0005f;
                        const float invNdl  = 1.f / (ndl > 0.15f ? ndl : 0.15f);   // borné (anti sur-biais rasant)
                        const float sbias   = shBias0 * invNdl;
                        // Biais NORMAL (world units) : pousse le point échantillonné le long de la normale
                        // avant projection → anti peter-panning (décollement de l'ombre au pied du caster).
                        const float nBias = (biasP[1]>0.f) ? biasP[1] : 0.f;
                        const uint32 aw = atlas->Width(0), ah = atlas->Height(0);
                        const float* ad = (const float*)atlas->mips[0].Data();
                        for (int s=0;s<count;++s) {
                            const int slot = first + s;
                            if (slot<0 || slot>=numSlots) continue;
                            const float* sm  = (const float*)(su + (uint32)slot*112u);        // shadowMatrix @0
                            const float* tuv = (const float*)(su + (uint32)slot*112u + 64u);  // tileUV @64 (minU,minV,maxU,maxV)
                            const float wpx=f.attrs[0]+nx*nBias, wpy=f.attrs[1]+ny*nBias, wpz=f.attrs[2]+nz*nBias;
                            float qx=sm[0]*wpx+sm[4]*wpy+sm[ 8]*wpz+sm[12];
                            float qy=sm[1]*wpx+sm[5]*wpy+sm[ 9]*wpz+sm[13];
                            float qz=sm[2]*wpx+sm[6]*wpy+sm[10]*wpz+sm[14];
                            float qw=sm[3]*wpx+sm[7]*wpy+sm[11]*wpz+sm[15];
                            if (qw<=1e-6f) continue;
                            const float ndcx=qx/qw, ndcy=qy/qw, ndcz=qz/qw;
                            if (ndcx<-1.f||ndcx>1.f||ndcy<-1.f||ndcy>1.f) continue;   // hors de ce slot -> cascade suivante
                            float fragD = (depthRemap>0.5f) ? (ndcz*0.5f+0.5f) : ndcz;
                            if (fragD<0.f||fragD>1.f) continue;
                            const float u = tuv[0] + (ndcx*0.5f+0.5f)*(tuv[2]-tuv[0]);
                            const float vv= tuv[1] + (ndcy*0.5f+0.5f)*(tuv[3]-tuv[1]);   // software : pas de Y-flip
                            // PCF. Défaut = 3×3 (9 taps, pénombre douce). Mesuré : le nombre de taps
                            // n'est PAS le goulot (3×3 ~283ms vs 2×2 ~309ms = bruit de mesure ±40ms ;
                            // le coût par pixel est dominé par le fragment 3D d'éclairage, pas par le
                            // sampling d'ombre). On garde donc le 3×3 (meilleure qualité, perf neutre).
                            // Toggle : NK_SW_PCF=2 sélectionne un 2×2 bilinéaire (4 taps, plus dur).
                            auto tap = [&](int ax,int ay)->float {
                                if (ax<0||ay<0||ax>=(int)aw||ay>=(int)ah) return 1.f;   // hors tuile => éclairé
                                return (fragD - sbias > ad[(uint32)ay*aw+(uint32)ax]) ? 0.f : 1.f;
                            };
                            static const bool s_pcf2 = [](){ const char* e=std::getenv("NK_SW_PCF"); return e && e[0]=='2'; }();
                            if (s_pcf2) {   // 2×2 bilinéaire (expérimental, bords plus durs)
                                const float fx = u*(float)aw - 0.5f, fy = vv*(float)ah - 0.5f;
                                const int x0 = (int)std::floor(fx), y0 = (int)std::floor(fy);
                                const float wx = fx - (float)x0, wy = fy - (float)y0;
                                const float s00=tap(x0,y0),   s10=tap(x0+1,y0);
                                const float s01=tap(x0,y0+1), s11=tap(x0+1,y0+1);
                                return (s00*(1.f-wx)+s10*wx)*(1.f-wy) + (s01*(1.f-wx)+s11*wx)*wy;
                            }
                            const int cx=(int)(u*(float)aw), cy=(int)(vv*(float)ah);   // 3×3 (défaut)
                            float acc=0.f;
                            for (int dy=-1;dy<=1;++dy) for (int dx=-1;dx<=1;++dx) acc += tap(cx+dx,cy+dy);
                            return acc*(1.f/9.f);
                        }
                        return 1.f;   // aucun slot ne couvre ce fragment -> éclairé
                    };
                    const uint8* lu = fb.lu;
                    int lcount = lu ? *(const int*)(lu+2048) : 0;
                    if (lcount<0) lcount=0; if (lcount>8) lcount=8;
                    for (int i=0;i<lcount;++i) {
                        const float* P=(const float*)(lu+(uint32)i*16u);
                        const float* C=(const float*)(lu+512u+(uint32)i*16u);
                        const float* D=(const float*)(lu+1024u+(uint32)i*16u);
                        const int type=(int)P[3];
                        float Lx,Ly,Lz, att=1.f;
                        if (type==0) { Lx=-D[0]; Ly=-D[1]; Lz=-D[2]; }
                        else {
                            Lx=P[0]-f.attrs[0]; Ly=P[1]-f.attrs[1]; Lz=P[2]-f.attrs[2];
                            float dist=std::sqrt(Lx*Lx+Ly*Ly+Lz*Lz); if(dist>1e-4f){Lx/=dist;Ly/=dist;Lz/=dist;}
                            float range=D[3]; if(range<1e-3f)range=25.f;
                            att=1.f-dist/range; if(att<0.f)att=0.f; att*=att;
                            if (type==2) { const float* A=(const float*)(lu+1536u+(uint32)i*16u);
                                float th=Lx*(-D[0])+Ly*(-D[1])+Lz*(-D[2]);
                                float k=(th-A[1])/(A[0]-A[1]+1e-4f); if(k<0)k=0; if(k>1)k=1; att*=k; }
                        }
                        { float ll=std::sqrt(Lx*Lx+Ly*Ly+Lz*Lz); if(ll>1e-6f){Lx/=ll;Ly/=ll;Lz/=ll;} }
                        float ndl=nx*Lx+ny*Ly+nz*Lz; if(ndl<0.f)ndl=0.f;
                        float inten=C[3]; if(!(inten>0.f))inten=1.f;
                        // Ombre portée : atténue la contribution DIRECTE (diffus+spéc), pas l'ambiant.
                        // PERF : n'échantillonne l'atlas (PCF) que pour les fragments FACE à la lumière
                        // (ndl>0) — sinon la contribution directe est nulle et l'ombre n'a aucun effet.
                        // Diag : NK_SW_SHADOW_NODIR=1 coupe l'ombre de la directionnelle (isole point/spot).
                        static const bool s_noDirSh = [](){ const char* e=std::getenv("NK_SW_SHADOW_NODIR"); return e && e[0]=='1'; }();
                        if (ndl>0.f && !(s_noDirSh && type==0)) inten *= sampleShadow(i, ndl);
                        float lr=C[0]*inten*att, lg=C[1]*inten*att, lb=C[2]*inten*att;
                        // diffus
                        litR += cr*ndl*lr*kd; litG += cg*ndl*lg*kd; litB += cb*ndl*lb*kd;
                        // spéculaire Blinn-Phong (teinté albédo pour les métaux)
                        float hx=Lx+vx, hy=Ly+vy, hz=Lz+vz;
                        float hl=std::sqrt(hx*hx+hy*hy+hz*hz); if(hl>1e-6f){hx/=hl;hy/=hl;hz/=hl;}
                        float ndh=nx*hx+ny*hy+nz*hz; if(ndh<0.f)ndh=0.f;
                        float s=std::pow(ndh,shin)*ndl;
                        litR += s*((1.f-metal)*f0+metal*cr)*lr;
                        litG += s*((1.f-metal)*f0+metal*cg)*lg;
                        litB += s*((1.f-metal)*f0+metal*cb)*lb;
                    }
                    if (lcount==0) {   // fallback : pas de LightsUBO -> lumière fixe approx
                        const float lx=0.4f,ly=0.82f,lz=0.41f;
                        float ndl=nx*lx+ny*ly+nz*lz; if(ndl<0.f)ndl=0.f;
                        float d=0.28f+0.72f*ndl; litR=cr*d; litG=cg*d; litB=cb*d;
                    }
                    return { litR, litG, litB, ca };
                };
                NK_SW_LOG("Shader NkSL sans cpuFn -> fixed-function NKRenderer installe (viewProj x model + vraies lumieres + spec).\n");
            }
        }

        uint64 hid = NextId();
        mShaders[hid] = traits::NkMove(sh);
        NkShaderHandle h; h.id = hid;
        return h;
    }

    void NkSoftwareDevice::DestroyShader(NkShaderHandle& h) {
        threading::NkScopedLockMutex lock(mMutex);
        if (auto* s = mShaders.Find(h.id)) {   // v5 Phase A : libérer les programmes VM heap
            auto& alloc = nkentseu::memory::NkGetDefaultAllocator();
            if (s->vmVert)    alloc.Delete(s->vmVert);
            if (s->vmFrag)    alloc.Delete(s->vmFrag);
            if (s->vmCompute) alloc.Delete(s->vmCompute);
        }
        mShaders.Erase(h.id); h.id = 0;
    }

    // =============================================================================
    // Pipelines
    // =============================================================================
    NkPipelineHandle NkSoftwareDevice::CreateGraphicsPipeline(const NkGraphicsPipelineDesc& d) {
        threading::NkScopedLockMutex lock(mMutex);
        NkSWPipeline p;
        p.shaderId     = d.shader.id;
        p.isCompute    = false;
        p.depthTest    = d.depthStencil.depthTestEnable;
        p.depthWrite   = d.depthStencil.depthWriteEnable;
        p.depthOp      = d.depthStencil.depthCompareOp;
        // v4 : on respecte le cullMode demandé (le cœur swraster gère le winding, y-down).
        p.cullMode     = d.rasterizer.cullMode;
        p.frontFace    = d.rasterizer.frontFace;
        p.topology     = d.topology;
        p.vertexStride = d.vertexLayout.bindings.Size() > 0 ? d.vertexLayout.bindings[0].stride : 0;
        if (d.blend.attachments.Size() > 0) {
            p.blendEnable = d.blend.attachments[0].blendEnable;
            p.srcColor    = d.blend.attachments[0].srcColor;
            p.dstColor    = d.blend.attachments[0].dstColor;
        }
        uint64 hid = NextId(); mPipelines[hid] = p;
        NkPipelineHandle h; h.id = hid; return h;
    }

    NkPipelineHandle NkSoftwareDevice::CreateComputePipeline(const NkComputePipelineDesc& d) {
        threading::NkScopedLockMutex lock(mMutex);
        NkSWPipeline p;
        p.shaderId  = d.shader.id;
        p.isCompute = true;
        uint64 hid = NextId(); mPipelines[hid] = p;
        NkPipelineHandle h; h.id = hid; return h;
    }

    void NkSoftwareDevice::DestroyPipeline(NkPipelineHandle& h) {
        threading::NkScopedLockMutex lock(mMutex); mPipelines.Erase(h.id); h.id = 0;
    }

    // =============================================================================
    // Render Passes & Framebuffers
    // =============================================================================
    NkRenderPassHandle NkSoftwareDevice::CreateRenderPass(const NkRenderPassDesc& d) {
        threading::NkScopedLockMutex lock(mMutex);
        uint64 hid = NextId(); mRenderPasses[hid] = { d };
        NkRenderPassHandle h; h.id = hid; return h;
    }

    void NkSoftwareDevice::DestroyRenderPass(NkRenderPassHandle& h) {
        threading::NkScopedLockMutex lock(mMutex); mRenderPasses.Erase(h.id); h.id = 0;
    }

    NkFramebufferHandle NkSoftwareDevice::CreateFramebuffer(const NkFramebufferDesc& d) {
        threading::NkScopedLockMutex lock(mMutex);

        NkSWFramebuffer fb;
        fb.colorId = d.colorAttachments.Size() > 0 ? d.colorAttachments[0].id : 0;
        fb.depthId = d.depthAttachment.id;
        fb.w = d.width; fb.h = d.height;

        uint64 hid = NextId(); 
        mFramebuffers[hid] = fb;

        NkFramebufferHandle h; 
        h.id = hid; 
        return h;
    }

    void NkSoftwareDevice::DestroyFramebuffer(NkFramebufferHandle& h) {
        threading::NkScopedLockMutex lock(mMutex); mFramebuffers.Erase(h.id); h.id = 0;
    }

    // =============================================================================
    // Descriptor Sets
    // =============================================================================
    NkDescSetHandle NkSoftwareDevice::CreateDescriptorSetLayout(const NkDescriptorSetLayoutDesc& d) {
        threading::NkScopedLockMutex lock(mMutex);
        uint64 hid = NextId(); mDescLayouts[hid] = { d };
        NkDescSetHandle h; h.id = hid; return h;
    }
    void NkSoftwareDevice::DestroyDescriptorSetLayout(NkDescSetHandle& h) {
        threading::NkScopedLockMutex lock(mMutex); mDescLayouts.Erase(h.id); h.id = 0;
    }
    NkDescSetHandle NkSoftwareDevice::AllocateDescriptorSet(NkDescSetHandle layout) {
        threading::NkScopedLockMutex lock(mMutex);
        NkSWDescSet ds;
        uint64 hid = NextId(); mDescSets[hid] = ds;
        NkDescSetHandle h; h.id = hid; return h;
    }
    void NkSoftwareDevice::FreeDescriptorSet(NkDescSetHandle& h) {
        threading::NkScopedLockMutex lock(mMutex); mDescSets.Erase(h.id); h.id = 0;
    }
    void NkSoftwareDevice::UpdateDescriptorSets(const NkDescriptorWrite* writes, uint32 n) {
        threading::NkScopedLockMutex lock(mMutex);
        for (uint32 i = 0; i < n; i++) {
            auto& w = writes[i];
            auto* sit = mDescSets.Find(w.set.id); if (!sit) continue;
            NkSWDescSet::Binding b{ w.binding, w.type, w.buffer.id, w.texture.id, w.sampler.id };
            bool found = false;
            for (auto& e : sit->bindings) if (e.slot == w.binding) { e = b; found = true; break; }
            if (!found) sit->bindings.PushBack(b);
        }
    }

    // =============================================================================
    // Command Buffers
    // =============================================================================
    NkICommandBuffer* NkSoftwareDevice::CreateCommandBuffer(NkCommandBufferType t) {
        return nkentseu::memory::NkGetDefaultAllocator().New<NkSoftwareCommandBuffer>(this, t);
    }

    void NkSoftwareDevice::DestroyCommandBuffer(NkICommandBuffer*& cb) { nkentseu::memory::NkGetDefaultAllocator().Delete(cb); cb = nullptr; }

    // =============================================================================
    // Submit
    // =============================================================================
    void NkSoftwareDevice::Submit(NkICommandBuffer* const* cbs, uint32 n, NkFenceHandle fence) {
        for (uint32 i = 0; i < n; i++) {
            auto* sw = dynamic_cast<NkSoftwareCommandBuffer*>(cbs[i]);
            if (sw) sw->Execute(this);
        }
        
        if (fence.IsValid()) {
            auto* it = mFences.Find(fence.id); if (it) it->signaled = true;
        }
    }

    void NkSoftwareDevice::SubmitAndPresent(NkICommandBuffer* cb) {
        NkICommandBuffer* cbs[] = { cb };
        Submit(cbs, 1, {});
        Present();
    }

    // v4 Phase 4 — BPR : l'app fournit sa caméra (view/proj). On en déduit l'inverse view-proj
    // pour reconstruire la géométrie monde à partir du clip-space des draws.
    void NkSoftwareDevice::SetRtCamera(const math::NkMat4f& view, const math::NkMat4f& proj) {
        mRtView = view; mRtProj = proj;
        mRtInvVP = (proj * view).Inverse();
        mRtHasCamera = true;
    }

    // Collecte un triangle (sommets en clip-space) → monde, poussé dans la scène BPR.
    void NkSoftwareDevice::RtAddTriangle(const NkVertexSoftware& a, const NkVertexSoftware& b, const NkVertexSoftware& c) {
        auto toWorld = [&](const NkVertexSoftware& v) -> swtrace::V3 {
            const math::NkVec4 wh = mRtInvVP * v.position;           // clip → monde (homogène)
            const float iw = (std::fabs(wh.w) > 1e-8f) ? 1.f / wh.w : 0.f;
            return swtrace::V3(wh.x * iw, wh.y * iw, wh.z * iw);
        };
        swtrace::Tri t;
        t.v0 = toWorld(a); t.v1 = toWorld(b); t.v2 = toWorld(c);
        t.color = swtrace::V3(a.color.x, a.color.y, a.color.z);      // albedo = couleur du sommet
        t.reflect = 0.f;
        mRtScene.PushBack(t);
    }

    // Rend la scène BPR dans buf : géométrie de l'app si collectée, sinon scène built-in animée.
    void NkSoftwareDevice::RtRenderInto(uint8* buf, int w, int h, bool bgra) {
        if (mRtHasCamera && !mRtScene.Empty()) {
            const swtrace::V3 ld = swtrace::Normalize(swtrace::V3(-0.6f, -1.f, -0.4f));
            swtrace::RenderScene(buf, w, h, mRtView, mRtProj, mRtScene.Data(), (int)mRtScene.Size(),
                                 ld, swtrace::V3(0.55f, 0.75f, 0.95f), swtrace::Quality::Live(), bgra);
        } else {
            swtrace::RenderLive(buf, w, h, (uint32)mFrameNumber, bgra);
        }
    }

    // Récupère les octets d'un UBO lié à (set, binding) pour le shader taillé NKRenderer.
    // Recherche d'un UBO dans un seul set slot (helper interne).
    const uint8* NkSoftwareDevice::SwFindUBOInSet(uint32 set, uint32 binding) {
        if (set >= kMaxCurSets || mCurDescSets[set] == 0) return nullptr;
        auto* ds = GetDescSet(mCurDescSets[set]);
        if (!ds) return nullptr;
        for (uint32 i = 0; i < (uint32)ds->bindings.Size(); ++i) {
            const auto& b = ds->bindings[i];
            if (b.slot == binding && b.bufId &&
                (b.type == NkDescriptorType::NK_UNIFORM_BUFFER ||
                 b.type == NkDescriptorType::NK_UNIFORM_BUFFER_DYNAMIC)) {
                auto* buf = GetBuf(b.bufId);
                return (buf && !buf->data.Empty()) ? buf->data.Data() : nullptr;
            }
        }
        return nullptr;
    }

    const uint8* NkSoftwareDevice::SwGetUBOBytes(uint32 set, uint32 binding) {
        // Cache thread_local (4 entrées) : (gen,set,binding) -> ptr. La résolution suivante itère
        // les bindings + balaie les sets ; l'appeler PAR PIXEL coûtait cher. Clé = SwBindGen()
        // (constante pendant la rasterisation MT d'un draw), donc sûr sans verrou.
        struct UEnt { uint64 gen; uint32 sb; const uint8* ptr; };
        static thread_local UEnt uc[4] = {};
        const uint64 gen = mBindGen; const uint32 sb = (set<<16)|binding;
        for (int i=0;i<4;++i) if (uc[i].gen==gen && uc[i].sb==sb) return uc[i].ptr;
        const uint8* res = nullptr;
        if (!(res = SwFindUBOInSet(set, binding))) {
            for (uint32 s = 0; s < kMaxCurSets && !res; ++s) if (s!=set) res = SwFindUBOInSet(s, binding);
        }
        static thread_local uint32 ur=0; uc[ur&3] = { gen, sb, res }; ++ur;
        return res;
    }

    const NkSWTexture* NkSoftwareDevice::SwFindTexInSet(uint32 set, uint32 binding) {
        if (set >= kMaxCurSets || mCurDescSets[set] == 0) return nullptr;
        auto* ds = GetDescSet(mCurDescSets[set]);
        if (!ds) return nullptr;
        for (uint32 i = 0; i < (uint32)ds->bindings.Size(); ++i) {
            const auto& b = ds->bindings[i];
            if (b.slot == binding && b.texId &&
                (b.type == NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER ||
                 b.type == NkDescriptorType::NK_SAMPLED_TEXTURE)) {
                return GetTex(b.texId);
            }
        }
        return nullptr;
    }

    const NkSWTexture* NkSoftwareDevice::SwGetTexAt(uint32 set, uint32 binding) {
        struct TEnt { uint64 gen; uint32 sb; const NkSWTexture* ptr; };
        static thread_local TEnt tc[4] = {};
        const uint64 gen = mBindGen; const uint32 sb = (set<<16)|binding;
        for (int i=0;i<4;++i) if (tc[i].gen==gen && tc[i].sb==sb) return tc[i].ptr;
        const NkSWTexture* res = SwFindTexInSet(set, binding);
        for (uint32 s = 0; s < kMaxCurSets && !res; ++s) if (s!=set) res = SwFindTexInSet(s, binding);   // modèle aplati
        static thread_local uint32 tr=0; tc[tr&3] = { gen, sb, res }; ++tr;
        return res;
    }

    void NkSoftwareDevice::Present() {
        if (mInit.presentCallback) {
            mInit.presentCallback();
            return;
        }
        
        // v4 Phase 4 : mode BPR — ray-trace la scène dans le backbuffer (ordre natif) avant présentation.
        if (mRtMode) {
            auto* fbit = mFramebuffers.Find(mSwapchainFB.id);
            auto* tit  = fbit ? mTextures.Find(fbit->colorId) : nullptr;
            if (tit && !tit->mips.Empty()) {
                const int W = (int)tit->Width(0), H = (int)tit->Height(0);
                uint8* dst = tit->mips[0].Data();
                const bool bgra = (NK_SW_PIXEL_BGRA != 0);
                const int s = (int)(mRtScale < 1u ? 1u : mRtScale);
                if (s <= 1 || W < s || H < s) {
                    RtRenderInto(dst, W, H, bgra);
                } else {
                    // Rendu RT basse résolution (1/s) puis upscale nearest → gros gain de fluidité.
                    const int lowW = W / s, lowH = H / s;
                    if (mRtLowBuf.Size() < (NkVector<uint8>::SizeType)((usize)lowW * lowH * 4u))
                        mRtLowBuf.Assign((uint8)0, (NkVector<uint8>::SizeType)((usize)lowW * lowH * 4u));
                    uint8* low = mRtLowBuf.Data();
                    RtRenderInto(low, lowW, lowH, bgra);
                    for (int y = 0; y < H; ++y) {
                        const uint8* srow = low + (usize)(y * lowH / H) * lowW * 4u;
                        uint8* drow = dst + (usize)y * W * 4u;
                        for (int x = 0; x < W; ++x) {
                            const uint8* sp = srow + (usize)(x * lowW / W) * 4u;
                            uint8* dp = drow + (usize)x * 4u;
                            dp[0]=sp[0]; dp[1]=sp[1]; dp[2]=sp[2]; dp[3]=sp[3];
                        }
                    }
                }
            }
        }

        if (mSSAA > 1) ResolveFramebuffer();
        const uint8* pixels = BackbufferPixels();
        if (!pixels || mWidth == 0 || mHeight == 0) return;

        usize pixelSize = mWidth * mHeight * 4u;

    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        // Copier pixels vers DIBSection puis BitBlt vers l'ecran
        if (mData.dibBits && mData.dibDC && mData.hdc) {
            // DIRECT memcpy : le framebuffer est déjà en BGRA (ordre GDI)
            // grâce à NK_SW_PIXEL_BGRA dans NkSWPixel.h
            memcpy(mData.dibBits, pixels, pixelSize);
            BitBlt(static_cast<HDC>(mData.hdc), 0, 0, (int)mWidth, (int)mHeight, static_cast<HDC>(mData.dibDC), 0, 0, SRCCOPY);
        }

    #elif defined(NKENTSEU_WINDOWING_XLIB)
        Display* disp = static_cast<Display*>(mData.display);
        XImage*  img  = static_cast<XImage*>(mData.ximage);
        GC       gc   = static_cast<GC>(mData.gc);
        if (!img) return;

        // Copier pixels (RGBA â†’ BGRA si nÃ©cessaire selon le visual)
        if (img->byte_order == LSBFirst) {
            // Convertir RGBA8 â†’ BGRA8 pour X11
            uint32 count = mWidth * mHeight;
            uint32* src  = (uint32*)pixels;
            uint32* dst  = (uint32*)img->data;
            for (uint32 i = 0; i < count; ++i) {
                uint32 p = src[i];
                dst[i] = ((p & 0x000000FF) << 16) |  // Râ†’B
                        ( p & 0x0000FF00)         |  // G
                        ((p & 0x00FF0000) >> 16)   |  // Bâ†’R
                        ( p & 0xFF000000);             // A
            }
        } else {
            memcpy(img->data, pixels, pixelSize);
        }

        if (mData.useSHM) {
            XShmSegmentInfo shm;
            shm.shmid   = mData.shmid;
            shm.shmaddr = img->data;
            XShmPutImage(disp, (::Window)mData.window, gc, img,
                        0, 0, 0, 0, mWidth, mHeight, False);
        } else {
            XPutImage(disp, (::Window)mData.window, gc, img,
                    0, 0, 0, 0, mWidth, mHeight);
        }
        XFlush(disp);

    #elif defined(NKENTSEU_WINDOWING_XCB)
        xcb_connection_t* conn = static_cast<xcb_connection_t*>(mData.connection);
        if (!conn) return;
        xcb_put_image(conn, XCB_IMAGE_FORMAT_Z_PIXMAP,
                    (xcb_window_t)mData.window,
                    (xcb_gcontext_t)mData.gc,
                    (uint16_t)mWidth, (uint16_t)mHeight,
                    0, 0, 0, 24,
                    (uint32_t)pixelSize,
                    (const uint8_t*)pixels);
        xcb_flush(conn);

    #elif defined(NKENTSEU_WINDOWING_WAYLAND)
        auto* wlDisplay = static_cast<wl_display*>(mData.wlDisplay);
        auto* wlSurface = static_cast<wl_surface*>(mData.wlSurface);
        auto* wlBuffer  = static_cast<wl_buffer*>(mData.wlBuffer);
        auto* shmPixels = static_cast<uint8*>(mData.shmPixels);
        if (!mData.waylandConfigured || !wlSurface || !wlBuffer || !shmPixels) return;

        const uint32 stride = mData.shmStride ? mData.shmStride : (mWidth * 4u);
        if (stride < 4u) return;

        const uint32 maxRows = static_cast<uint32>(mData.shmSize / stride);
        const uint32 rows = math::NkMin(mHeight, maxRows);
        const uint32 cols = math::NkMin(mWidth, stride / 4u);

        // Wayland SHM ARGB8888 on little-endian is stored as BGRA bytes.
        for (uint32 y = 0; y < rows; ++y) {
            const uint8* src = pixels + y * (4u * mWidth);
            uint8* dst = shmPixels + static_cast<uint64>(y) * static_cast<uint64>(stride);
            for (uint32 x = 0; x < cols; ++x) {
                dst[x * 4u + 0u] = src[x * 4u + 2u]; // B <- R
                dst[x * 4u + 1u] = src[x * 4u + 1u]; // G
                dst[x * 4u + 2u] = src[x * 4u + 0u]; // R <- B
                dst[x * 4u + 3u] = 255u;             // A
            }
        }

        wl_surface_attach(wlSurface, wlBuffer, 0, 0);
        wl_surface_damage(wlSurface, 0, 0, (int32_t)mWidth, (int32_t)mHeight);
        wl_surface_commit(wlSurface);
        if (wlDisplay) {
            wl_display_flush(wlDisplay);
        }

    #elif defined(NKENTSEU_PLATFORM_ANDROID)
        ANativeWindow* win = static_cast<ANativeWindow*>(mData.nativeWindow);

        if (!win) return;

        ANativeWindow_Buffer buf;
        ARect bounds = {0, 0, (int32_t)mWidth, (int32_t)mHeight};

        if (ANativeWindow_lock(win, &buf, &bounds) == 0) {
            uint32 copyW = math::NkMin((uint32)buf.stride, mWidth);

            for (uint32 y = 0; y < mHeight && y < (uint32)buf.height; ++y) {
                memcpy((uint8_t*)buf.bits + y*buf.stride*4, pixels + y * (4u * mWidth), copyW*4);
            }

            ANativeWindow_unlockAndPost(win);
        }

    #elif defined(NKENTSEU_PLATFORM_HARMONYOS)
        // Le framebuffer est en RGBA8888 (NK_SW_PIXEL_BGRA = 0 sur HarmonyOS).
        // OHNativeWindow attend du RGBA_8888 — pas de swap de canaux.
        //
        // Méthode 1 : OHNativeWindow direct (NDK API 9+)
        if (mData.ohNativeWindow) {
            OHNativeWindow* win = static_cast<OHNativeWindow*>(mData.ohNativeWindow);
    
            // Configurer le format RGBA_8888 (à faire une fois lors de l'init)
            // OH_NativeWindow_NativeWindowHandleOpt(win, SET_FORMAT, PIXEL_FMT_RGBA_8888);
            // OH_NativeWindow_NativeWindowHandleOpt(win, SET_BUFFER_GEOMETRY, (int)mWidth, (int)mHeight);
    
            OHNativeWindowBuffer* buffer = nullptr;
            int fenceFd = -1;
    
            int32_t ret = OH_NativeWindow_NativeWindowRequestBuffer(win, &buffer, &fenceFd);
            if (ret == 0 && buffer) {
                void* addr = nullptr;
                OH_NativeWindow_GetBufferHandleFromNative(buffer, &addr);
                // addr pointe vers les pixels de la surface (RGBA_8888)
                if (addr) {
                    // Direct memcpy — pas de swap de canaux (RGBA = RGBA)
                    memcpy(addr, pixels, (usize)mWidth * mHeight * 4u);
                }
                // Soumettre le buffer
                OH_NativeWindow_NativeWindowFlushBuffer(win, buffer, fenceFd, {});
            }
            return;
        }
    
        // Méthode 2 : callback bridge ArkTS (presentCallback fourni par NkHarmonyBridge)
        // Le callback reçoit les pixels RGBA et les présente via PixelMap + ImageBitmap.
        if (mData.presentCallback) {
            using PresentFn = void(*)(const uint8* pixels, uint32 w, uint32 h);
            auto fn = reinterpret_cast<PresentFn>(mData.presentCallback);
            fn(pixels, mWidth, mHeight);
            return;
        }
    
        // Pas de handle — frame silencieusement ignorée
        // (normal pendant la phase d'initialisation avant OnSurfaceCreated)

    #elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
        EM_ASM({
            var id = UTF8ToString($0);
            var canvas = null;
            if (id && id.length > 0) {
                canvas = document.getElementById(id.charAt(0) === '#' ? id.substring(1) : id);
            }
            if (!canvas && Module['canvas']) {
                canvas = Module['canvas'];
            }
            if (!canvas) return;

            var ctx = canvas.getContext('2d');
            if (!ctx) return;

            var imgData = ctx.createImageData($1, $2);
            var src = new Uint8Array(Module.HEAPU8.buffer, $3, $1 * $2 * 4);
            imgData.data.set(src);
            ctx.putImageData(imgData, 0, 0);
        },
        mData.canvasId,
        (int)mWidth, (int)mHeight,
        (int)(uintptr_t)pixels);
    #endif

    // #if defined(NKENTSEU_PLATFORM_WINDOWS)
    //     HWND hwnd = mInit.surface.hwnd;
    //     const uint8* pixels = BackbufferPixels();
    //     if (!hwnd || !pixels || mWidth == 0 || mHeight == 0) return;
    
    //     HDC hdc = GetDC(hwnd);
    //     if (!hdc) return;
    
    //     BITMAPINFO bmi{};
    //     bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    //     bmi.bmiHeader.biWidth       = static_cast<LONG>(mWidth);
    //     bmi.bmiHeader.biHeight      = -static_cast<LONG>(mHeight);  // top-down
    //     bmi.bmiHeader.biPlanes      = 1;
    //     bmi.bmiHeader.biBitCount    = 32;
    //     bmi.bmiHeader.biCompression = BI_RGB
    
    //     // BI_RGB avec 32 bpp : GDI interprète les pixels comme BGRX
    //     // Le framebuffer est maintenant en BGRA → compatible direct !
    //     StretchDIBits(hdc,
    //         0, 0, static_cast<int>(mWidth), static_cast<int>(mHeight),
    //         0, 0, static_cast<int>(mWidth), static_cast<int>(mHeight),
    //         pixels, &bmi, DIB_RGB_COLORS, SRCCOPY);
    //     ReleaseDC(hwnd, hdc);
    // #endif
    }

    const uint8* NkSoftwareDevice::BackbufferPixels() const {
        // v4 Phase 2 : si SSAA actif, on présente le buffer résolu (taille fenêtre)
        if (mSSAA > 1)
            return mResolveBuf.Empty() ? nullptr : mResolveBuf.Data();

        const auto* fbit = mFramebuffers.Find(mSwapchainFB.id);

        if (!fbit) return nullptr;

        const auto* tit = mTextures.Find(fbit->colorId);

        if (!tit || tit->mips.Empty()) return nullptr;

        return tit->mips[0].Data();
    }

    // =============================================================================
    // v4 Phase 2 : résolution SSAA — box downsample hi-res → mResolveBuf (fenêtre)
    // Moyenne mSSAA×mSSAA sous-échantillons par pixel de sortie. Ordre d'octets natif
    // préservé (moyenne par position d'octet, indépendante de BGRA/RGBA).
    // =============================================================================
    void NkSoftwareDevice::ResolveFramebuffer() {
        if (mSSAA <= 1) return;
        const auto* fbit = mFramebuffers.Find(mSwapchainFB.id);
        if (!fbit) return;
        const auto* tit = mTextures.Find(fbit->colorId);
        if (!tit || tit->mips.Empty()) return;

        const uint8* src = tit->mips[0].Data();
        const uint32 ss  = mSSAA;
        const uint32 hiW = mWidth * ss;
        const uint32 inv = ss * ss;

        if (mResolveBuf.Size() < (NkVector<uint8>::SizeType)((usize)mWidth * mHeight * 4u))
            mResolveBuf.Assign((uint8)0, (NkVector<uint8>::SizeType)((usize)mWidth * mHeight * 4u));
        uint8* dst = mResolveBuf.Data();

        for (uint32 y = 0; y < mHeight; ++y) {
            for (uint32 x = 0; x < mWidth; ++x) {
                uint32 a0 = 0, a1 = 0, a2 = 0, a3 = 0;
                for (uint32 sy = 0; sy < ss; ++sy) {
                    const uint8* row = src + ((usize)(y * ss + sy) * hiW + (usize)x * ss) * 4u;
                    for (uint32 sx = 0; sx < ss; ++sx) {
                        a0 += row[sx*4+0]; a1 += row[sx*4+1]; a2 += row[sx*4+2]; a3 += row[sx*4+3];
                    }
                }
                uint8* o = dst + ((usize)y * mWidth + x) * 4u;
                o[0] = (uint8)(a0 / inv); o[1] = (uint8)(a1 / inv);
                o[2] = (uint8)(a2 / inv); o[3] = (uint8)(a3 / inv);
            }
        }
    }

    // =============================================================================
    // Fence
    // =============================================================================
    NkFenceHandle NkSoftwareDevice::CreateFence(bool signaled) {
        uint64 hid = NextId(); mFences[hid] = { signaled };
        NkFenceHandle h; h.id = hid; return h;
    }
    void NkSoftwareDevice::DestroyFence(NkFenceHandle& h) { mFences.Erase(h.id); h.id = 0; }
    bool NkSoftwareDevice::WaitFence(NkFenceHandle f, uint64) {
        auto* it = mFences.Find(f.id); return it && it->signaled;
    }
    bool NkSoftwareDevice::IsFenceSignaled(NkFenceHandle f) {
        auto* it = mFences.Find(f.id); return it && it->signaled;
    }
    void NkSoftwareDevice::ResetFence(NkFenceHandle f) {
        auto* it = mFences.Find(f.id); if (it) it->signaled = false;
    }

    // =============================================================================
    // Frame
    // =============================================================================
    bool NkSoftwareDevice::BeginFrame(NkFrameContext& frame) {
        if (mRtMode) mRtScene.Clear();   // v4 Phase 4 : réinitialiser la géométrie BPR collectée
        // Clear depth
        auto* fbit = mFramebuffers.Find(mSwapchainFB.id);
        if (fbit) {
            auto* dit = mTextures.Find(fbit->depthId);
            if (dit && !dit->mips.Empty()) {
                float32 one = 1.0f;
                uint32 count = dit->Width() * dit->Height();
                for (uint32 i = 0; i < count; i++)
                    memcpy(dit->mips[0].Data() + i*4, &one, 4);
            }
        }
        frame.frameIndex  = mFrameIndex;
        frame.frameNumber = mFrameNumber;
        return true;
    }

    void NkSoftwareDevice::EndFrame(NkFrameContext&) { ++mFrameNumber; }

    void NkSoftwareDevice::OnResize(uint32 w, uint32 h) {
        if (w == 0 || h == 0) return;
        ShutdownNativePresenter();

        mInit.surface.height = h;
        mInit.surface.width = w;

        if (!InitNativePresenter(mInit.surface)) {
            NK_SW_LOG("OnResize: InitNativePresenter failed\n");
            mIsValid = false;
        }

        mData.width = w;
        mData.height = h;
        mWidth = w; 
        mHeight = h;


        DestroyFramebuffer(mSwapchainFB);
        DestroyRenderPass(mSwapchainRP);

        CreateSwapchainObjects();

        if (mInit.resizeCallback) {
            mInit.resizeCallback(w, h);
        }
    }

    // =============================================================================
    // Accesseurs
    // =============================================================================
    NkSWBuffer*      NkSoftwareDevice::GetBuf  (uint64 id) { return mBuffers.Find(id); }
    NkSWTexture*     NkSoftwareDevice::GetTex  (uint64 id) { return mTextures.Find(id); }
    NkSWSampler*     NkSoftwareDevice::GetSamp (uint64 id) { return mSamplers.Find(id); }
    NkSWShader*      NkSoftwareDevice::GetShader(uint64 id){ return mShaders.Find(id); }
    NkSWPipeline*    NkSoftwareDevice::GetPipe (uint64 id) { return mPipelines.Find(id); }
    NkSWDescSet*     NkSoftwareDevice::GetDescSet(uint64 id){ return mDescSets.Find(id); }
    NkSWFramebuffer* NkSoftwareDevice::GetFBO  (uint64 id) { return mFramebuffers.Find(id); }
    NkSWRenderPass*  NkSoftwareDevice::GetRP   (uint64 id) { return mRenderPasses.Find(id); }

        // =============================================================================
    //  InitNativePresenter â€” par plateforme
    // =============================================================================
    bool NkSoftwareDevice::InitNativePresenter(const NkSurfaceDesc& surf) {
        uint32 w = surf.width, h = surf.height;

    // â”€â”€ Windows â€” GDI DIBSection â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        mData.hwnd = surf.hwnd;
        HWND hwnd  = static_cast<HWND>(surf.hwnd);
        mData.hdc  = GetDC(hwnd);

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = (LONG)w;
        bmi.bmiHeader.biHeight      = -(LONG)h; // top-down
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void* bits = nullptr;
        mData.dibBitmap = CreateDIBSection(
            static_cast<HDC>(mData.hdc), &bmi,
            DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!mData.dibBitmap) { NK_SW_LOG("CreateDIBSection failed\n"); return false; }

        mData.dibBits = bits;
        mData.dibDC   = CreateCompatibleDC(static_cast<HDC>(mData.hdc));
        SelectObject(static_cast<HDC>(mData.dibDC), static_cast<HBITMAP>(mData.dibBitmap));
        return true;

    // â”€â”€ Linux XLib â€” XShm (shared memory) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    #elif defined(NKENTSEU_WINDOWING_XLIB)
        Display*      display = static_cast<Display*>(surf.display);
        ::Window      xwin    = (::Window)surf.window;
        mData.display = display;
        mData.window  = xwin;

        // CrÃ©er un GC
        GC gc = XCreateGC(display, xwin, 0, nullptr);
        mData.gc = (void*)gc;
        mData.shmInfo = nullptr;

        // Essayer XShm (shared memory â€” plus rapide)
        int shmMajor, shmMinor; Bool pixmaps;
        mData.useSHM = (XShmQueryVersion(display, &shmMajor, &shmMinor, &pixmaps) == True);
        if (mData.useSHM) {
            const char* wslInterop = std::getenv("WSL_INTEROP");
            const char* wslDistro  = std::getenv("WSL_DISTRO_NAME");
            if ((wslInterop && *wslInterop) || (wslDistro && *wslDistro) ||
                (std::getenv("NK_X11_DISABLE_SHM") != nullptr)) {
                mData.useSHM = false;
            }
        }

        if (mData.useSHM) {
            XShmSegmentInfo* shm = nkentseu::memory::NkGetDefaultAllocator().New<XShmSegmentInfo>();
            Visual* vis  = DefaultVisual(display, DefaultScreen(display));
            int     depth= DefaultDepth(display, DefaultScreen(display));
            XImage* img  = XShmCreateImage(display, vis, depth, ZPixmap,
                                            nullptr, shm, w, h);
            if (!img) { mData.useSHM = false; nkentseu::memory::NkGetDefaultAllocator().Delete(shm); goto fallback_xlib; }
            shm->shmid = shmget(IPC_PRIVATE, img->bytes_per_line * img->height,
                                IPC_CREAT | 0777);
            if (shm->shmid < 0) { XDestroyImage(img); mData.useSHM=false; nkentseu::memory::NkGetDefaultAllocator().Delete(shm); goto fallback_xlib; }
            shm->shmaddr = img->data = (char*)shmat(shm->shmid, nullptr, 0);
            if (shm->shmaddr == (char*)-1) {
                img->data = nullptr;
                XDestroyImage(img);
                shmctl(shm->shmid, IPC_RMID, nullptr);
                mData.useSHM = false;
                nkentseu::memory::NkGetDefaultAllocator().Delete(shm);
                goto fallback_xlib;
            }
            shm->readOnly = False;
            if (!XShmAttach(display, shm)) {
                shmdt(shm->shmaddr);
                shmctl(shm->shmid, IPC_RMID, nullptr);
                img->data = nullptr;
                XDestroyImage(img);
                mData.useSHM = false;
                nkentseu::memory::NkGetDefaultAllocator().Delete(shm);
                goto fallback_xlib;
            }
            mData.ximage = img;
            mData.shmInfo = shm;
            mData.shmid  = shm->shmid;
            NK_SW_LOG("XShm presenter OK (%ux%u)\n", w, h);
            return true;
        }

        fallback_xlib: {
            // Fallback : XImage classique
            Visual* vis   = DefaultVisual(display, DefaultScreen(display));
            int     depth = DefaultDepth(display, DefaultScreen(display));
            char*   data  = static_cast<char*>(nkentseu::memory::NkAlloc((nk_size)w * (nk_size)h * 4u));
            XImage* img   = XCreateImage(display, vis, depth, ZPixmap, 0,
                                        data, w, h, 32, 0);
            mData.ximage = img;
            NK_SW_LOG("XImage (no SHM) presenter OK (%ux%u)\n", w, h);
            return img != nullptr;
        }

    // â”€â”€ Linux XCB â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    #elif defined(NKENTSEU_WINDOWING_XCB)
        xcb_connection_t* conn = static_cast<xcb_connection_t*>(surf.connection);
        mData.connection = conn;
        mData.window     = surf.window;

        xcb_screen_t* screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
        uint32 gcMask   = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND;
        uint32 gcValues[2] = { screen->black_pixel, screen->white_pixel };
        xcb_gcontext_t gc = xcb_generate_id(conn);
        xcb_create_gc(conn, gc, (xcb_window_t)surf.window, gcMask, gcValues);
        mData.gc = gc;
        xcb_flush(conn);
        return true;

    // â”€â”€ Wayland â€” wl_shm â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    #elif defined(NKENTSEU_WINDOWING_WAYLAND)
        mData.wlDisplay = surf.display;
        mData.wlSurface = surf.surface;
        mData.wlBuffer  = surf.shmBuffer;
        mData.shmPixels = surf.shmPixels;
        mData.waylandConfigured = surf.waylandConfigured;
        mData.shmStride = surf.shmStride ? surf.shmStride : (w * 4u);
        mData.shmSize   = (uint64)mData.shmStride * (uint64)h;
        return (surf.display != nullptr &&
                surf.surface != nullptr &&
                surf.shmBuffer != nullptr &&
                surf.shmPixels != nullptr);

    // â”€â”€ Android â€” ANativeWindow â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    #elif defined(NKENTSEU_PLATFORM_ANDROID)
        mData.nativeWindow = surf.nativeWindow;
        ANativeWindow_setBuffersGeometry(
            static_cast<ANativeWindow*>(surf.nativeWindow),
            (int32_t)w, (int32_t)h,
            WINDOW_FORMAT_RGBA_8888);
        return surf.nativeWindow != nullptr;

    #elif defined(NKENTSEU_PLATFORM_HARMONYOS)
        // HarmonyOS Software Backend — Stratégie de présentation
        //
        // HarmonyOS tourne sur ARM64 (Kirin) → NEON disponible, OpenGL ES 3.2+,
        // Vulkan 1.0+. Le backend Software est donc un fallback de dernier recours.
        //
        // Méthode 1 (recommandée) : OHNativeWindow direct
        //   OH_NativeWindow_NativeWindowRequestBuffer() → lock pixels → memcpy → post
        //   Disponible si surf.ohNativeWindow est fourni par NkHarmonyOnSurfaceCreated.
        //
        // Méthode 2 (fallback) : callback bridge ArkTS
        //   NkHarmonyBridge.presentFrame(pixelData) → canvas.putImageData
        //   Utilisé si OHNativeWindow n'est pas accessible directement.
    
        mData.ohNativeWindow = surf.ohNativeWindow;  // fourni par NkHarmonyOnSurfaceCreated
        mData.presentCallback = surf.presentCallback;
    
        if (!mData.ohNativeWindow && !mData.presentCallback) {
            NK_SW_LOG("[HarmonyOS] Aucun handle natif fourni. "
                    "Appeler SetHarmonyNativeWindow() après OnSurfaceCreated.\n");
            // Pas une erreur fatale — le handle peut arriver plus tard
            return true;
        }
    
        NK_SW_LOG("[HarmonyOS] Software presenter OK (%ux%u)\n", w, h);
        return true;
    // â”€â”€ macOS â€” CGContext â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    #elif defined(NKENTSEU_PLATFORM_MACOS)
        mData.nsView = surf.view;
        // CGContext est recrÃ©Ã© Ã  chaque Present depuis drawRect: â€” pas de state ici
        return surf.view != nullptr;

    // â”€â”€ WebAssembly â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    #elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
        mData.canvasId = surf.canvasId;
        return surf.canvasId != nullptr;

    #else
        return false;
    #endif
    }

    // =============================================================================
    //  ShutdownNativePresenter
    // =============================================================================
    void NkSoftwareDevice::ShutdownNativePresenter() {
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        if (mData.dibDC)     { DeleteDC(static_cast<HDC>(mData.dibDC));         mData.dibDC = nullptr; }
        if (mData.dibBitmap) { DeleteObject(static_cast<HBITMAP>(mData.dibBitmap)); mData.dibBitmap = nullptr; }
        if (mData.hdc && mData.hwnd) {
            ReleaseDC(static_cast<HWND>(mData.hwnd), static_cast<HDC>(mData.hdc));
            mData.hdc = nullptr;
        }

    #elif defined(NKENTSEU_PLATFORM_HARMONYOS)
        // Libérer EGL si utilisé (méthode 3 — rare pour le backend software)
        if (mData.eglSurface || mData.eglContext) {
            // eglMakeCurrent(EGL_NO_DISPLAY, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            // eglDestroySurface(mData.eglDisplay, mData.eglSurface);
            // eglDestroyContext(mData.eglDisplay, mData.eglContext);
            mData.eglSurface = nullptr;
            mData.eglContext = nullptr;
            mData.eglDisplay = nullptr;
        }
        mData.ohNativeWindow  = nullptr;
        mData.presentCallback = nullptr;

    #elif defined(NKENTSEU_WINDOWING_XLIB)
        if (mData.ximage) {
            Display* disp = static_cast<Display*>(mData.display);
            XImage* img   = static_cast<XImage*>(mData.ximage);
            auto* shm = static_cast<XShmSegmentInfo*>(mData.shmInfo);
            if (mData.useSHM && shm) {
                XShmDetach(disp, shm);
                if (shm->shmaddr && shm->shmaddr != (char*)-1) {
                    shmdt(shm->shmaddr);
                }
                if (shm->shmid >= 0) {
                    shmctl(shm->shmid, IPC_RMID, nullptr);
                }
                nkentseu::memory::NkGetDefaultAllocator().Delete(shm);
                mData.shmInfo = nullptr;
                mData.shmid = -1;
                img->data = nullptr;
            } else {
                nkentseu::memory::NkFree(img->data);
                img->data = nullptr;
            }
            XDestroyImage(img);
            mData.ximage = nullptr;
        }
        if (mData.shmInfo) {
            nkentseu::memory::NkGetDefaultAllocator().Delete(static_cast<XShmSegmentInfo*>(mData.shmInfo));
            mData.shmInfo = nullptr;
        }
        mData.useSHM = false;
        if (mData.gc && mData.display) {
            XFreeGC(static_cast<Display*>(mData.display),
                    static_cast<GC>(mData.gc));
            mData.gc = nullptr;
        }

    #elif defined(NKENTSEU_WINDOWING_XCB)
        if (mData.gc && mData.connection) {
            xcb_free_gc(static_cast<xcb_connection_t*>(mData.connection),
                        (xcb_gcontext_t)mData.gc);
            mData.gc = 0;
        }

    #elif defined(NKENTSEU_WINDOWING_WAYLAND)
        // Wayland : mÃ©moire gÃ©rÃ©e par wl_shm â€” pas de libÃ©ration ici
        mData.wlDisplay = nullptr;
        mData.shmPixels = nullptr;
        mData.wlSurface = nullptr;
        mData.wlBuffer = nullptr;
        mData.waylandConfigured = false;
        mData.shmStride = 0;
        mData.shmSize = 0;
    #endif
    }
 
    void NkSoftwareDevice::SetHarmonyNativeWindow(void* ohNativeWindow) {
#if defined(NKENTSEU_PLATFORM_HARMONYOS)
        // Appelé depuis NkHarmonyOnSurfaceCreated() côté C++ :
        //
        //   void NkHarmonyOnSurfaceCreated(OH_NativeXComponent* component,
        //                                  OHNativeWindow* window) {
        //       g_device->SetHarmonyNativeWindow(window);
        //       // Émettre NkWindowSurfaceCreatedEvent vers NkWESystem
        //   }
        //
        // Le handle peut arriver APRÈS Initialize() car la surface XComponent
        // est créée de façon asynchrone par ArkTS.
        mData.ohNativeWindow = ohNativeWindow;
 
        if (ohNativeWindow) {
            // Configurer le format et la taille du buffer natif
            // OHNativeWindow* win = static_cast<OHNativeWindow*>(ohNativeWindow);
            // OH_NativeWindow_NativeWindowHandleOpt(win, SET_FORMAT, PIXEL_FMT_RGBA_8888);
            // OH_NativeWindow_NativeWindowHandleOpt(win, SET_BUFFER_GEOMETRY,
            //                                       (int)mWidth, (int)mHeight);
            NK_SW_LOG("[HarmonyOS] OHNativeWindow set: %p\n", ohNativeWindow);
        }
#else
        (void)ohNativeWindow;
#endif
    }
} // namespace nkentseu
