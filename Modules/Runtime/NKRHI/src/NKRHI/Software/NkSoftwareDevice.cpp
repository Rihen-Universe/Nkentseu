// =============================================================================
// NkRHI_Device_SW.cpp — Software Rasterizer
// =============================================================================
#include "NkSoftwareDevice.h"
#include "NkSoftwareCommandBuffer.h"
#include "NKRHI/Core/NkGpuPolicy.h"
#include "NKLogger/NkLog.h"
#include "NKCore/NkTraits.h"
#include <cstring>
#include <cmath>
#include <algorithm>
#include <cassert>

#if defined(NKENTSEU_PLATFORM_WINDOWS)
#   ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
#   endif
#   include <windows.h>
#endif

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
    NkSWVertex NkSWRasterizer::ClipToNDC(const NkSWVertex& v) const {
        NkSWVertex r = v;
        r.clipZ = v.position.z;
        r.clipW = v.position.w;
        float32 invW = v.position.w != 0.f ? 1.f / v.position.w : 0.f;
        r.position.x *= invW;
        r.position.y *= invW;
        r.position.z *= invW;
        r.position.w  = invW; // stocker 1/w pour interpolation perspective
        return r;
    }

    NkSWVertex NkSWRasterizer::NDCToScreen(const NkSWVertex& v, float32 w, float32 h) const {
        NkSWVertex r = v;
        r.position.x = (v.position.x + 1.f) * 0.5f * w;
        r.position.y = (1.f - v.position.y) * 0.5f * h; // Y-flip
        r.position.z = v.position.z * 0.5f + 0.5f;       // [-1,1] → [0,1]
        return r;
    }

    NkSWVertex NkSWRasterizer::Interpolate(const NkSWVertex& a, const NkSWVertex& b, float32 t) const {
        NkSWVertex r;
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

    NkSWVertex NkSWRasterizer::BaryInterp(const NkSWVertex& v0, const NkSWVertex& v1,
                                        const NkSWVertex& v2,
                                        float32 l0, float32 l1, float32 l2) const {
        // Interpolation perspective-correcte avec 1/w stocké dans position.w
        float32 w = l0 * v0.position.w + l1 * v1.position.w + l2 * v2.position.w;
        float32 invW = w != 0.f ? 1.f / w : 0.f;
        NkSWVertex r;
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

    void NkSWRasterizer::DrawPoint(const NkSWVertex& v0) {
        if (!mState.colorTarget && !mState.depthTarget) return;
        NkSWTexture* dimTarget = mState.colorTarget ? mState.colorTarget : mState.depthTarget;
        auto ndc  = ClipToNDC(v0);
        auto scr  = NDCToScreen(ndc, (float)dimTarget->Width(), (float)dimTarget->Height());
        uint32 x = (uint32)scr.position.x;
        uint32 y = (uint32)scr.position.y;
        if (x >= dimTarget->Width() || y >= dimTarget->Height()) return;
        if (!DepthTest(x, y, scr.position.z)) return;
        if (mState.colorTarget) {
            NkSWColor c = mState.shader && mState.shader->fragFn
                ? mState.shader->fragFn(scr, mState.uniformData, mState.colorTarget)
                : scr.color;
            mState.colorTarget->Write(x, y, c);
        }
    }

    void NkSWRasterizer::DrawLine(const NkSWVertex& v0, const NkSWVertex& v1) {
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
            NkSWVertex p = Interpolate(ndc0, ndc1, t);
            uint32 x = (uint32)p.position.x, y = (uint32)p.position.y;
            if (x >= (uint32)W || y >= (uint32)H) continue;
            if (!DepthTest(x, y, p.position.z)) continue;
            if (mState.colorTarget) {
                NkSWColor c = mState.shader && mState.shader->fragFn
                    ? mState.shader->fragFn(p, mState.uniformData, mState.colorTarget) : p.color;
                mState.colorTarget->Write(x, y, c);
            }
        }
    }

    void NkSWRasterizer::DrawTriangle(const NkSWVertex& v0,
                                    const NkSWVertex& v1,
                                    const NkSWVertex& v2) {
        if (!mState.colorTarget && !mState.depthTarget) return;
        NkSWTexture* dimTarget = mState.colorTarget ? mState.colorTarget : mState.depthTarget;
        float32 W = (float)dimTarget->Width();
        float32 H = (float)dimTarget->Height();

        auto s0 = NDCToScreen(ClipToNDC(v0), W, H);
        auto s1 = NDCToScreen(ClipToNDC(v1), W, H);
        auto s2 = NDCToScreen(ClipToNDC(v2), W, H);

        // Wireframe
        if (mState.wireframe) { DrawLine(s0,s1); DrawLine(s1,s2); DrawLine(s2,s0); return; }

        // Culling — calcul de l'aire signée (cross product 2D)
        float32 area2 = (s1.position.x-s0.position.x)*(s2.position.y-s0.position.y)
                    - (s2.position.x-s0.position.x)*(s1.position.y-s0.position.y);

        if (mState.pipeline) {
            NkCullMode cm = mState.pipeline->cullMode;
            bool ccw      = mState.pipeline->frontFace == NkFrontFace::NK_CCW;
            if (cm != NkCullMode::NK_NONE) {
                bool isFront = ccw ? (area2 > 0) : (area2 < 0);
                if ((cm == NkCullMode::NK_BACK  && !isFront) ||
                    (cm == NkCullMode::NK_FRONT &&  isFront)) return;
            }
        }
        if (math::NkFabs(area2) < 0.001f) return;
        // Négatif car NDCToScreen flipe Y, ce qui inverse le signe du cross-product
        // (repère écran Y-bas vs repère math Y-haut). Sans ce signe, l0 et l1
        // sont négatifs pour les points intérieurs → tous les pixels rejetés.
        float32 invArea = -1.f / area2;

        // Bounding box
        
        int minX = (int)math::NkFloor(math::NkMin({s0.position.x, s1.position.x, s2.position.x}));
        int maxX = (int)math::NkCeil (math::NkMax({s0.position.x, s1.position.x, s2.position.x}));
        int minY = (int)math::NkFloor(math::NkMin({s0.position.y, s1.position.y, s2.position.y}));
        int maxY = (int)math::NkCeil (math::NkMax({s0.position.y, s1.position.y, s2.position.y}));
        minX = math::NkMax(minX, 0); maxX = math::NkMin(maxX, (int)W-1);
        minY = math::NkMax(minY, 0); maxY = math::NkMin(maxY, (int)H-1);

        // Rasterisation par scanline
        for (int y = minY; y <= maxY; y++) {
            for (int x = minX; x <= maxX; x++) {
                float32 px = x + 0.5f, py = y + 0.5f;
                // Coordonnées barycentriques
                float32 l0 = ((s1.position.x - s2.position.x)*(py - s2.position.y)
                        - (s1.position.y - s2.position.y)*(px - s2.position.x)) * invArea;
                float32 l1 = ((s2.position.x - s0.position.x)*(py - s0.position.y)
                        - (s2.position.y - s0.position.y)*(px - s0.position.x)) * invArea;
                float32 l2 = 1.f - l0 - l1;
                constexpr float32 kInsideEpsilon = 1e-6f;
                if (l0 < -kInsideEpsilon || l1 < -kInsideEpsilon || l2 < -kInsideEpsilon) continue;

                // Interpolation perspective-correcte de la profondeur.
                // position.z est déjà en [0,1] (NDCToScreen a fait z*0.5+0.5).
                // position.w = 1/clip_w (invW pour correction perspective).
                const float32 invWDenom = l0*s0.position.w + l1*s1.position.w + l2*s2.position.w;
                if (math::NkFabs(invWDenom) < 1e-8f) continue;
                float32 z = (l0*s0.position.z*s0.position.w +
                             l1*s1.position.z*s1.position.w +
                             l2*s2.position.z*s2.position.w) / invWDenom;
                z = math::NkClamp(z, 0.f, 1.f);
                if (!DepthTest((uint32)x, (uint32)y, z)) continue;

                if (mState.colorTarget) {
                    NkSWVertex frag = BaryInterp(s0, s1, s2, l0, l1, l2);
                    frag.position.x = px; frag.position.y = py; frag.position.z = z;

                    NkSWColor srcColor = (mState.shader && mState.shader->fragFn)
                        ? mState.shader->fragFn(frag, mState.uniformData, mState.colorTarget)
                        : frag.color;

                    if (mState.pipeline && mState.pipeline->blendEnable) {
                        NkSWColor dstColor = mState.colorTarget->Read((uint32)x, (uint32)y);
                        srcColor = BlendColor(srcColor, dstColor);
                    }

                    mState.colorTarget->Write((uint32)x, (uint32)y, srcColor);
                }
            }
        }
    }

    void NkSWRasterizer::DrawTriangles(const NkSWVertex* verts, uint32 count) {
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
        mInit   = init;
        NkGpuPolicy::ApplyPreContext(mInit.context);

        const NkSoftwareDesc& swCfg = mInit.context.software;
        mWidth  = NkDeviceInitWidth(init);
        mHeight = NkDeviceInitHeight(init);
        if (mWidth == 0)  mWidth  = 512;
        if (mHeight == 0) mHeight = 512;
        mUseSse = swCfg.useSSE;
        mThreadCount = swCfg.threadCount > 0
            ? swCfg.threadCount
            : math::NkMax(1u, (uint32)std::thread::hardware_concurrency());
        if (!swCfg.threading) {
            mThreadCount = 1;
        }

        CreateSwapchainObjects();

        mCaps.computeShaders     = NkDeviceInitComputeEnabledForApi(mInit, NkGraphicsApi::NK_API_SOFTWARE);
        mCaps.drawIndirect       = false;
        mCaps.multiViewport      = false;
        mCaps.independentBlend   = true;
        mCaps.maxTextureDim2D    = 4096;
        mCaps.maxColorAttachments= 1;
        mCaps.maxVertexAttributes= 16;
        mCaps.maxPushConstantBytes=256;

        mIsValid = true;
        NK_SW_LOG("Initialisé (%u×%u, %u threads, SSE=%s)\n", mWidth, mHeight,
                mThreadCount, mUseSse ? "on" : "off");
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
        fbd.width = mWidth; fbd.height = mHeight;
        mSwapchainFB = CreateFramebuffer(fbd);
    }

    void NkSoftwareDevice::Shutdown() {
        mBuffers.Clear(); mTextures.Clear(); mSamplers.Clear();
        mShaders.Clear(); mPipelines.Clear(); mRenderPasses.Clear();
        mFramebuffers.Clear(); mDescLayouts.Clear(); mDescSets.Clear();
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
            ? (uint32)(math::NkFloor(
                    math::NkLog2(static_cast<float32>(math::NkMax(desc.width, desc.height))))
                + 1.0f)
            : desc.mipLevels;

        uint32 w = desc.width, hgt = desc.height;
        for (uint32 m = 0; m < mipCount; m++) {
            uint32 sz = math::NkMax(1u,w) * math::NkMax(1u,hgt) * bpp;
            t.mips.EmplaceBack(sz, (uint8)(desc.format==NkGPUFormat::NK_D32_FLOAT ? 0x3F : 0));
            // Init depth à 1.0
            if (desc.format == NkGPUFormat::NK_D32_FLOAT) {
                float32 one = 1.0f;
                for (uint32 i = 0; i < math::NkMax(1u,w)*math::NkMax(1u,hgt); i++)
                    memcpy(t.mips[m].Data() + i*4, &one, 4);
            }
            w >>= 1; hgt >>= 1;
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
        // Shaders CPU passés via pointeurs de callbacks (stockés dans void*).
        for (uint32 i = 0; i < desc.stages.Size(); i++) {
            auto& s = desc.stages[i];
            if (s.stage == NkShaderStage::NK_VERTEX   && s.cpuVertFn)
                sh.vertFn = *static_cast<const NkSWVertexShader*>(s.cpuVertFn);
            if (s.stage == NkShaderStage::NK_FRAGMENT && s.cpuFragFn)
                sh.fragFn = *static_cast<const NkSWPixelShader*>(s.cpuFragFn);
            if (s.stage == NkShaderStage::NK_COMPUTE  && s.cpuCompFn) {
                sh.isCompute = true;
                sh.computeFn = *static_cast<const NkSWComputeShader*>(s.cpuCompFn);
            }
        }
        // Shader par défaut si non fourni : couleur de vertex passthrough
        if (!sh.fragFn && !sh.isCompute)
            sh.fragFn = [](const NkSWVertex& v, const void*, const void*) { return v.color; };

        uint64 hid = NextId(); mShaders[hid] = traits::NkMove(sh);
        NkShaderHandle h; h.id = hid; return h;
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
        uint64 hid = NextId(); mFramebuffers[hid] = fb;
        NkFramebufferHandle h; h.id = hid; return h;
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

#if defined(NKENTSEU_PLATFORM_WINDOWS)
    HWND hwnd = mInit.surface.hwnd;
    const uint8* pixels = BackbufferPixels();
    if (!hwnd || !pixels || mWidth == 0 || mHeight == 0) {
        return;
    }

    HDC hdc = GetDC(hwnd);
    if (!hdc) return;

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = static_cast<LONG>(mWidth);
    bmi.bmiHeader.biHeight = -static_cast<LONG>(mHeight);
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    StretchDIBits(
        hdc,
        0, 0, static_cast<int>(mWidth), static_cast<int>(mHeight),
        0, 0, static_cast<int>(mWidth), static_cast<int>(mHeight),
        pixels,
        &bmi,
        DIB_RGB_COLORS,
        SRCCOPY);
    ReleaseDC(hwnd, hdc);
#endif
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
    mWidth = w; mHeight = h;
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

} // namespace nkentseu
