// =============================================================================
// NkRHI_Device_SW.cpp — Software Rasterizer
// =============================================================================
#include "NkSoftwareDevice.h"
#include "NkSoftwareCommandBuffer.h"
#include "NKRHI/Core/NkGpuPolicy.h"
#include "NKLogger/NkLog.h"
#include "NKCore/NkTraits.h"
#include "NkSWFastPath.h"

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
        if (bpp >= 4) { c.r = p[0]/255.f; c.g = p[1]/255.f; c.b = p[2]/255.f; c.a = p[3]/255.f; }
        else if (bpp == 3) { c.r = p[0]/255.f; c.g = p[1]/255.f; c.b = p[2]/255.f; c.a = 1.f; }
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
        if (bpp >= 4) { p[0]=(uint8)(r*255); p[1]=(uint8)(g*255); p[2]=(uint8)(b*255); p[3]=(uint8)(a*255); }
        else if (bpp == 3) { p[0]=(uint8)(r*255); p[1]=(uint8)(g*255); p[2]=(uint8)(b*255); }
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
        return true;
    }

    void NkSoftwareDevice::CreateSwapchainObjects() {
        // Color backbuffer
        NkTextureDesc cd;
        cd.format     = NkGPUFormat::NK_RGBA8_UNORM;
        cd.width      = mWidth;
        cd.height     = mHeight;
        cd.bindFlags  = NkBindFlags::NK_RENDER_TARGET | NkBindFlags::NK_SHADER_RESOURCE;
        cd.debugName  = "SW_Backbuffer";
        auto colorH = CreateTexture(cd);

        // Depth buffer
        NkTextureDesc dd;
        dd.format     = NkGPUFormat::NK_D32_FLOAT;
        dd.width      = mWidth;
        dd.height     = mHeight;
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
        fbd.width = mWidth; 
        fbd.height = mHeight;
        mSwapchainFB = CreateFramebuffer(fbd);
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
        threading::NkScopedLock lock(mMutex);
        NkSWBuffer b;
        b.desc = desc;
        b.data.Resize((size_t)desc.sizeBytes, 0);
        if (desc.initialData)
            memcpy(b.data.Data(), desc.initialData, (size_t)desc.sizeBytes);
        uint64 hid = NextId(); mBuffers[hid] = traits::NkMove(b);
        NkBufferHandle h; h.id = hid; return h;
    }
    void NkSoftwareDevice::DestroyBuffer(NkBufferHandle& h) {
        threading::NkScopedLock lock(mMutex); mBuffers.Erase(h.id); h.id = 0;
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
        threading::NkScopedLock lock(mMutex);
        NkSWTexture t;
        t.desc = desc;
        t.isRenderTarget = NkHasFlag(desc.bindFlags, NkBindFlags::NK_RENDER_TARGET) ||
                        NkHasFlag(desc.bindFlags, NkBindFlags::NK_DEPTH_STENCIL);

        uint32 bpp = NkFormatBytesPerPixel(desc.format);
        uint32 mipCount = desc.mipLevels == 0
            ? (uint32)(math::NkFloor(math::NkLog2(static_cast<float32>(math::NkMax(desc.width, desc.height)))) + 1.0f)
            : desc.mipLevels;

        uint32 w = desc.width, hgt = desc.height;
        for (uint32 m = 0; m < mipCount; m++) {
            uint32 sz = math::NkMax(1u, w) * math::NkMax(1u, hgt) * bpp;
            uint8 value = (uint8)(desc.format == NkGPUFormat::NK_D32_FLOAT ? 0x3F : 0);
            t.mips.EmplaceBack(sz, value);
            // Init depth à 1.0
            if (desc.format == NkGPUFormat::NK_D32_FLOAT) {
                float32 one = 1.0f;
                for (uint32 i = 0; i < math::NkMax(1u,w) * math::NkMax(1u, hgt); i++)
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
        threading::NkScopedLock lock(mMutex); mTextures.Erase(h.id); h.id = 0;
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
        threading::NkScopedLock lock(mMutex);
        uint64 hid = NextId(); mSamplers[hid] = { d };
        NkSamplerHandle h; h.id = hid; return h;
    }
    void NkSoftwareDevice::DestroySampler(NkSamplerHandle& h) {
        threading::NkScopedLock lock(mMutex); mSamplers.Erase(h.id); h.id = 0;
    }

    // =============================================================================
    // Shaders
    // =============================================================================
    NkShaderHandle NkSoftwareDevice::CreateShader(const NkShaderDesc& desc) {
        threading::NkScopedLock lock(mMutex);
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
                delete static_cast<const NkVertexShaderSoftware*>(s.cpuFn);
            } else if (s.stage == NkShaderStage::NK_FRAGMENT) {
                sh.fragFn = *static_cast<const NkPixelShaderSoftware*>(s.cpuFn);
                delete static_cast<const NkPixelShaderSoftware*>(s.cpuFn);
            } else if (s.stage == NkShaderStage::NK_COMPUTE) {
                sh.isCompute = true;
                sh.computeFn = *static_cast<const NkComputeShaderSoftware*>(s.cpuFn);
                delete static_cast<const NkComputeShaderSoftware*>(s.cpuFn);
            }
        }

        uint64 hid = NextId();
        mShaders[hid] = traits::NkMove(sh);
        NkShaderHandle h; h.id = hid;
        return h;
    }

    void NkSoftwareDevice::DestroyShader(NkShaderHandle& h) {
        threading::NkScopedLock lock(mMutex); mShaders.Erase(h.id); h.id = 0;
    }

    // =============================================================================
    // Pipelines
    // =============================================================================
    NkPipelineHandle NkSoftwareDevice::CreateGraphicsPipeline(const NkGraphicsPipelineDesc& d) {
        threading::NkScopedLock lock(mMutex);
        NkSWPipeline p;
        p.shaderId     = d.shader.id;
        p.isCompute    = false;
        p.depthTest    = d.depthStencil.depthTestEnable;
        p.depthWrite   = d.depthStencil.depthWriteEnable;
        p.depthOp      = d.depthStencil.depthCompareOp;
        // Keep software raster stable across APIs while winding conventions are aligned.
        p.cullMode     = NkCullMode::NK_NONE;
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
        threading::NkScopedLock lock(mMutex);
        NkSWPipeline p;
        p.shaderId  = d.shader.id;
        p.isCompute = true;
        uint64 hid = NextId(); mPipelines[hid] = p;
        NkPipelineHandle h; h.id = hid; return h;
    }

    void NkSoftwareDevice::DestroyPipeline(NkPipelineHandle& h) {
        threading::NkScopedLock lock(mMutex); mPipelines.Erase(h.id); h.id = 0;
    }

    // =============================================================================
    // Render Passes & Framebuffers
    // =============================================================================
    NkRenderPassHandle NkSoftwareDevice::CreateRenderPass(const NkRenderPassDesc& d) {
        threading::NkScopedLock lock(mMutex);
        uint64 hid = NextId(); mRenderPasses[hid] = { d };
        NkRenderPassHandle h; h.id = hid; return h;
    }

    void NkSoftwareDevice::DestroyRenderPass(NkRenderPassHandle& h) {
        threading::NkScopedLock lock(mMutex); mRenderPasses.Erase(h.id); h.id = 0;
    }

    NkFramebufferHandle NkSoftwareDevice::CreateFramebuffer(const NkFramebufferDesc& d) {
        threading::NkScopedLock lock(mMutex);

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
        threading::NkScopedLock lock(mMutex); mFramebuffers.Erase(h.id); h.id = 0;
    }

    // =============================================================================
    // Descriptor Sets
    // =============================================================================
    NkDescSetHandle NkSoftwareDevice::CreateDescriptorSetLayout(const NkDescriptorSetLayoutDesc& d) {
        threading::NkScopedLock lock(mMutex);
        uint64 hid = NextId(); mDescLayouts[hid] = { d };
        NkDescSetHandle h; h.id = hid; return h;
    }
    void NkSoftwareDevice::DestroyDescriptorSetLayout(NkDescSetHandle& h) {
        threading::NkScopedLock lock(mMutex); mDescLayouts.Erase(h.id); h.id = 0;
    }
    NkDescSetHandle NkSoftwareDevice::AllocateDescriptorSet(NkDescSetHandle layout) {
        threading::NkScopedLock lock(mMutex);
        NkSWDescSet ds;
        uint64 hid = NextId(); mDescSets[hid] = ds;
        NkDescSetHandle h; h.id = hid; return h;
    }
    void NkSoftwareDevice::FreeDescriptorSet(NkDescSetHandle& h) {
        threading::NkScopedLock lock(mMutex); mDescSets.Erase(h.id); h.id = 0;
    }
    void NkSoftwareDevice::UpdateDescriptorSets(const NkDescriptorWrite* writes, uint32 n) {
        threading::NkScopedLock lock(mMutex);
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
        return new NkSoftwareCommandBuffer(this, t);
    }

    void NkSoftwareDevice::DestroyCommandBuffer(NkICommandBuffer*& cb) { delete cb; cb = nullptr; }

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

    void NkSoftwareDevice::Present() {
        if (mInit.presentCallback) {
            mInit.presentCallback();
            return;
        }
        
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
        const auto* fbit = mFramebuffers.Find(mSwapchainFB.id);

        if (!fbit) return nullptr;

        const auto* tit = mTextures.Find(fbit->colorId);

        if (!tit || tit->mips.Empty()) return nullptr;

        return tit->mips[0].Data();
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
            XShmSegmentInfo* shm = new XShmSegmentInfo();
            Visual* vis  = DefaultVisual(display, DefaultScreen(display));
            int     depth= DefaultDepth(display, DefaultScreen(display));
            XImage* img  = XShmCreateImage(display, vis, depth, ZPixmap,
                                            nullptr, shm, w, h);
            if (!img) { mData.useSHM = false; delete shm; goto fallback_xlib; }
            shm->shmid = shmget(IPC_PRIVATE, img->bytes_per_line * img->height,
                                IPC_CREAT | 0777);
            if (shm->shmid < 0) { XDestroyImage(img); mData.useSHM=false; delete shm; goto fallback_xlib; }
            shm->shmaddr = img->data = (char*)shmat(shm->shmid, nullptr, 0);
            if (shm->shmaddr == (char*)-1) {
                img->data = nullptr;
                XDestroyImage(img);
                shmctl(shm->shmid, IPC_RMID, nullptr);
                mData.useSHM = false;
                delete shm;
                goto fallback_xlib;
            }
            shm->readOnly = False;
            if (!XShmAttach(display, shm)) {
                shmdt(shm->shmaddr);
                shmctl(shm->shmid, IPC_RMID, nullptr);
                img->data = nullptr;
                XDestroyImage(img);
                mData.useSHM = false;
                delete shm;
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
            char*   data  = new char[w * h * 4];
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
                delete shm;
                mData.shmInfo = nullptr;
                mData.shmid = -1;
                img->data = nullptr;
            } else {
                delete[] img->data;
                img->data = nullptr;
            }
            XDestroyImage(img);
            mData.ximage = nullptr;
        }
        if (mData.shmInfo) {
            delete static_cast<XShmSegmentInfo*>(mData.shmInfo);
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
} // namespace nkentseu
