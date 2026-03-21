// =============================================================================
// NkRHI_Device_SW.cpp — Software Rasterizer
// =============================================================================
#include "NkRHI_Device_SW.h"
#include "NkCommandBuffer_SW.h"
#include <cstring>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <cassert>

#define NK_SW_LOG(...) printf("[NkRHI_SW] " __VA_ARGS__)

namespace nkentseu {

// =============================================================================
// NkSWTexture helpers
// =============================================================================
NkSWColor NkSWTexture::Read(uint32 x, uint32 y, uint32 mip) const {
    if (mips.empty() || mip >= mips.size()) return {};
    uint32 w = Width(mip);
    uint32 h = Height(mip);
    uint32 bpp = Bpp();
    x = std::min(x, w - 1);
    y = std::min(y, h - 1);
    const uint8_t* p = mips[mip].data() + (y * w + x) * bpp;
    NkSWColor c;
    if (bpp >= 4) { c.r = p[0]/255.f; c.g = p[1]/255.f; c.b = p[2]/255.f; c.a = p[3]/255.f; }
    else if (bpp == 3) { c.r = p[0]/255.f; c.g = p[1]/255.f; c.b = p[2]/255.f; c.a = 1.f; }
    else if (bpp == 2) { c.r = p[0]/255.f; c.g = p[1]/255.f; c.a = 1.f; }
    else               { c.r = c.g = c.b = p[0]/255.f; c.a = 1.f; }
    return c;
}

void NkSWTexture::Write(uint32 x, uint32 y, const NkSWColor& c, uint32 mip) {
    if (mips.empty() || mip >= mips.size()) return;
    uint32 w = Width(mip);
    uint32 h = Height(mip);
    uint32 bpp = Bpp();
    if (x >= w || y >= h) return;
    uint8_t* p = mips[mip].data() + (y * w + x) * bpp;
    // Clamp
    float r = std::max(0.f,std::min(1.f,c.r));
    float g = std::max(0.f,std::min(1.f,c.g));
    float b = std::max(0.f,std::min(1.f,c.b));
    float a = std::max(0.f,std::min(1.f,c.a));
    // Cas spécial float pour depth
    if (desc.format == NkFormat::D32_Float) {
        memcpy(p, &c.r, 4); return;
    }
    if (bpp >= 4) { p[0]=(uint8_t)(r*255); p[1]=(uint8_t)(g*255); p[2]=(uint8_t)(b*255); p[3]=(uint8_t)(a*255); }
    else if (bpp == 3) { p[0]=(uint8_t)(r*255); p[1]=(uint8_t)(g*255); p[2]=(uint8_t)(b*255); }
    else if (bpp == 2) { p[0]=(uint8_t)(r*255); p[1]=(uint8_t)(g*255); }
    else               { p[0]=(uint8_t)(r*255); }
}

NkSWColor NkSWTexture::Sample(float u, float v, uint32 mip) const {
    if (mips.empty()) return { 0,0,0,1 };
    mip = std::min(mip, (uint32)mips.size()-1);
    uint32 w = Width(mip);
    uint32 h = Height(mip);
    // Wrap
    u = u - std::floor(u);
    v = v - std::floor(v);
    float px = u * w - 0.5f;
    float py = v * h - 0.5f;
    // Bilinéaire
    int x0 = (int)std::floor(px), y0 = (int)std::floor(py);
    int x1 = x0 + 1, y1 = y0 + 1;
    float fx = px - x0, fy = py - y0;
    auto wrap = [](int v, int sz) { return ((v % sz) + sz) % sz; };
    NkSWColor c00 = Read((uint32)wrap(x0,w), (uint32)wrap(y0,h), mip);
    NkSWColor c10 = Read((uint32)wrap(x1,w), (uint32)wrap(y0,h), mip);
    NkSWColor c01 = Read((uint32)wrap(x0,w), (uint32)wrap(y1,h), mip);
    NkSWColor c11 = Read((uint32)wrap(x1,w), (uint32)wrap(y1,h), mip);
    auto lerp4 = [&](NkSWColor a, NkSWColor b, float t) -> NkSWColor {
        return { a.r+(b.r-a.r)*t, a.g+(b.g-a.g)*t, a.b+(b.b-a.b)*t, a.a+(b.a-a.a)*t };
    };
    return lerp4(lerp4(c00,c10,fx), lerp4(c01,c11,fx), fy);
}

// =============================================================================
// NkSWRasterizer
// =============================================================================
NkSWVertex NkSWRasterizer::ClipToNDC(const NkSWVertex& v) const {
    NkSWVertex r = v;
    float invW = v.position.w != 0.f ? 1.f / v.position.w : 0.f;
    r.position.x *= invW;
    r.position.y *= invW;
    r.position.z *= invW;
    r.position.w  = invW; // stocker 1/w pour interpolation perspective
    return r;
}

NkSWVertex NkSWRasterizer::NDCToScreen(const NkSWVertex& v, float w, float h) const {
    NkSWVertex r = v;
    r.position.x = (v.position.x + 1.f) * 0.5f * w;
    r.position.y = (1.f - v.position.y) * 0.5f * h; // Y-flip
    r.position.z = v.position.z * 0.5f + 0.5f;       // [-1,1] → [0,1]
    return r;
}

NkSWVertex NkSWRasterizer::Interpolate(const NkSWVertex& a, const NkSWVertex& b, float t) const {
    NkSWVertex r;
    r.position.x = a.position.x + (b.position.x - a.position.x) * t;
    r.position.y = a.position.y + (b.position.y - a.position.y) * t;
    r.position.z = a.position.z + (b.position.z - a.position.z) * t;
    r.position.w = a.position.w + (b.position.w - a.position.w) * t;
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
                                       float l0, float l1, float l2) const {
    // Interpolation perspective-correcte avec 1/w stocké dans position.w
    float w = l0 * v0.position.w + l1 * v1.position.w + l2 * v2.position.w;
    float invW = w != 0.f ? 1.f / w : 0.f;
    NkSWVertex r;
    auto lerp3 = [&](float a, float b, float c) {
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

bool NkSWRasterizer::DepthTest(uint32 x, uint32 y, float z) {
    if (!mState.depthTarget || !mState.pipeline) return true;
    if (!mState.pipeline->depthTest) return true;

    NkSWColor d = mState.depthTarget->Read(x, y);
    float dz = d.r; // depth stocké dans le canal rouge
    bool pass = false;
    switch (mState.pipeline->depthOp) {
        case NkCompareOp::Less:         pass = z < dz;  break;
        case NkCompareOp::LessEqual:    pass = z <= dz; break;
        case NkCompareOp::Greater:      pass = z > dz;  break;
        case NkCompareOp::GreaterEqual: pass = z >= dz; break;
        case NkCompareOp::Equal:        pass = z == dz; break;
        case NkCompareOp::NotEqual:     pass = z != dz; break;
        case NkCompareOp::Always:       pass = true;    break;
        case NkCompareOp::Never:        pass = false;   break;
        default:                        pass = z < dz;  break;
    }
    if (pass && mState.pipeline->depthWrite) {
        NkSWColor dc{ z, z, z, 1.f };
        mState.depthTarget->Write(x, y, dc);
    }
    return pass;
}

float NkSWRasterizer::ApplyBlendFactor(NkBlendFactor f, float src, float dst, float srcA, float dstA) const {
    switch (f) {
        case NkBlendFactor::Zero:             return 0.f;
        case NkBlendFactor::One:              return 1.f;
        case NkBlendFactor::SrcColor:         return src;
        case NkBlendFactor::OneMinusSrcColor: return 1.f - src;
        case NkBlendFactor::SrcAlpha:         return srcA;
        case NkBlendFactor::OneMinusSrcAlpha: return 1.f - srcA;
        case NkBlendFactor::DstAlpha:         return dstA;
        case NkBlendFactor::OneMinusDstAlpha: return 1.f - dstA;
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
    if (!mState.colorTarget) return;
    auto ndc  = ClipToNDC(v0);
    auto scr  = NDCToScreen(ndc, (float)mState.colorTarget->Width(),
                                  (float)mState.colorTarget->Height());
    uint32 x = (uint32)scr.position.x;
    uint32 y = (uint32)scr.position.y;
    if (x >= mState.colorTarget->Width() || y >= mState.colorTarget->Height()) return;
    if (!DepthTest(x, y, scr.position.z)) return;
    NkSWColor c = mState.shader && mState.shader->fragFn
        ? mState.shader->fragFn(scr, mState.uniformData, nullptr)
        : scr.color;
    if (mState.colorTarget) mState.colorTarget->Write(x, y, c);
}

void NkSWRasterizer::DrawLine(const NkSWVertex& v0, const NkSWVertex& v1) {
    if (!mState.colorTarget) return;
    float W = (float)mState.colorTarget->Width();
    float H = (float)mState.colorTarget->Height();
    auto ndc0 = NDCToScreen(ClipToNDC(v0), W, H);
    auto ndc1 = NDCToScreen(ClipToNDC(v1), W, H);

    float dx = ndc1.position.x - ndc0.position.x;
    float dy = ndc1.position.y - ndc0.position.y;
    float steps = std::max(std::abs(dx), std::abs(dy));
    if (steps < 1.f) { DrawPoint(v0); return; }
    float inv = 1.f / steps;
    for (float s = 0; s <= steps; s++) {
        float t = s * inv;
        NkSWVertex p = Interpolate(ndc0, ndc1, t);
        uint32 x = (uint32)p.position.x, y = (uint32)p.position.y;
        if (x >= (uint32)W || y >= (uint32)H) continue;
        if (!DepthTest(x, y, p.position.z)) continue;
        NkSWColor c = mState.shader && mState.shader->fragFn
            ? mState.shader->fragFn(p, mState.uniformData, nullptr) : p.color;
        mState.colorTarget->Write(x, y, c);
    }
}

void NkSWRasterizer::DrawTriangle(const NkSWVertex& v0,
                                   const NkSWVertex& v1,
                                   const NkSWVertex& v2) {
    if (!mState.colorTarget) return;
    float W = (float)mState.colorTarget->Width();
    float H = (float)mState.colorTarget->Height();

    auto s0 = NDCToScreen(ClipToNDC(v0), W, H);
    auto s1 = NDCToScreen(ClipToNDC(v1), W, H);
    auto s2 = NDCToScreen(ClipToNDC(v2), W, H);

    // Wireframe
    if (mState.wireframe) { DrawLine(s0,s1); DrawLine(s1,s2); DrawLine(s2,s0); return; }

    // Culling — calcul de l'aire signée (cross product 2D)
    float area2 = (s1.position.x-s0.position.x)*(s2.position.y-s0.position.y)
                - (s2.position.x-s0.position.x)*(s1.position.y-s0.position.y);

    if (mState.pipeline) {
        NkCullMode cm = mState.pipeline->cullMode;
        bool ccw      = mState.pipeline->frontFace == NkFrontFace::CCW;
        if (cm != NkCullMode::None) {
            bool isFront = ccw ? (area2 > 0) : (area2 < 0);
            if ((cm == NkCullMode::Back  &&  isFront) ||
                (cm == NkCullMode::Front && !isFront)) return;
        }
    }
    if (std::abs(area2) < 0.001f) return;
    float invArea = 1.f / area2;

    // Bounding box
    int minX = (int)std::floor(std::min({s0.position.x, s1.position.x, s2.position.x}));
    int maxX = (int)std::ceil (std::max({s0.position.x, s1.position.x, s2.position.x}));
    int minY = (int)std::floor(std::min({s0.position.y, s1.position.y, s2.position.y}));
    int maxY = (int)std::ceil (std::max({s0.position.y, s1.position.y, s2.position.y}));
    minX = std::max(minX, 0); maxX = std::min(maxX, (int)W-1);
    minY = std::max(minY, 0); maxY = std::min(maxY, (int)H-1);

    // Rasterisation par scanline
    for (int y = minY; y <= maxY; y++) {
        for (int x = minX; x <= maxX; x++) {
            float px = x + 0.5f, py = y + 0.5f;
            // Coordonnées barycentriques
            float l0 = ((s1.position.x - s2.position.x)*(py - s2.position.y)
                      - (s1.position.y - s2.position.y)*(px - s2.position.x)) * invArea;
            float l1 = ((s2.position.x - s0.position.x)*(py - s0.position.y)
                      - (s2.position.y - s0.position.y)*(px - s0.position.x)) * invArea;
            float l2 = 1.f - l0 - l1;
            if (l0 < 0 || l1 < 0 || l2 < 0) continue;

            // Interpoler
            float z = l0*s0.position.z + l1*s1.position.z + l2*s2.position.z;
            if (!DepthTest((uint32)x, (uint32)y, z)) continue;

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

void NkSWRasterizer::DrawTriangles(const NkSWVertex* verts, uint32 count) {
    for (uint32 i = 0; i + 2 < count; i += 3)
        DrawTriangle(verts[i], verts[i+1], verts[i+2]);
}

// =============================================================================
// NkDevice_SW
// =============================================================================
NkDevice_SW::~NkDevice_SW() { if (mIsValid) Shutdown(); }

bool NkDevice_SW::Initialize(NkIGraphicsContext* ctx) {
    mCtx    = ctx;
    mWidth  = ctx ? ctx->GetInfo().windowWidth  : 512;
    mHeight = ctx ? ctx->GetInfo().windowHeight : 512;
    if (mWidth == 0)  mWidth  = 512;
    if (mHeight == 0) mHeight = 512;

    CreateSwapchainObjects();

    mCaps.computeShaders     = true; // émulé CPU
    mCaps.drawIndirect       = false;
    mCaps.multiViewport      = false;
    mCaps.independentBlend   = true;
    mCaps.maxTextureDim2D    = 4096;
    mCaps.maxColorAttachments= 1;
    mCaps.maxVertexAttributes= 16;
    mCaps.maxPushConstantBytes=256;

    mIsValid = true;
    NK_SW_LOG("Initialisé (%u×%u, %u threads)\n", mWidth, mHeight,
              std::thread::hardware_concurrency());
    return true;
}

void NkDevice_SW::CreateSwapchainObjects() {
    // Color backbuffer
    NkTextureDesc cd;
    cd.format     = NkFormat::RGBA8_Unorm;
    cd.width      = mWidth;
    cd.height     = mHeight;
    cd.bindFlags  = NkBindFlags::RenderTarget | NkBindFlags::ShaderResource;
    cd.debugName  = "SW_Backbuffer";
    auto colorH = CreateTexture(cd);

    // Depth buffer
    NkTextureDesc dd;
    dd.format     = NkFormat::D32_Float;
    dd.width      = mWidth;
    dd.height     = mHeight;
    dd.bindFlags  = NkBindFlags::DepthStencil;
    dd.debugName  = "SW_Depthbuffer";
    auto depthH = CreateTexture(dd);

    NkRenderPassDesc rpd;
    rpd.AddColor(NkAttachmentDesc::Color(NkFormat::RGBA8_Unorm))
       .SetDepth(NkAttachmentDesc::Depth());
    mSwapchainRP = CreateRenderPass(rpd);

    NkFramebufferDesc fbd;
    fbd.renderPass = mSwapchainRP;
    fbd.colorAttachments[0] = colorH; fbd.colorCount = 1;
    fbd.depthAttachment = depthH;
    fbd.width = mWidth; fbd.height = mHeight;
    mSwapchainFB = CreateFramebuffer(fbd);
}

void NkDevice_SW::Shutdown() {
    mBuffers.clear(); mTextures.clear(); mSamplers.clear();
    mShaders.clear(); mPipelines.clear(); mRenderPasses.clear();
    mFramebuffers.clear(); mDescLayouts.clear(); mDescSets.clear();
    mIsValid = false;
    NK_SW_LOG("Shutdown\n");
}

// =============================================================================
// Buffers
// =============================================================================
NkBufferHandle NkDevice_SW::CreateBuffer(const NkBufferDesc& desc) {
    std::lock_guard<std::mutex> lock(mMutex);
    NkSWBuffer b;
    b.desc = desc;
    b.data.resize((size_t)desc.sizeBytes, 0);
    if (desc.initialData)
        memcpy(b.data.data(), desc.initialData, (size_t)desc.sizeBytes);
    uint64 hid = NextId(); mBuffers[hid] = std::move(b);
    NkBufferHandle h; h.id = hid; return h;
}
void NkDevice_SW::DestroyBuffer(NkBufferHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex); mBuffers.erase(h.id); h.id = 0;
}
bool NkDevice_SW::WriteBuffer(NkBufferHandle buf, const void* data, uint64 sz, uint64 off) {
    auto it = mBuffers.find(buf.id); if (it == mBuffers.end()) return false;
    if (off + sz > it->second.data.size()) return false;
    memcpy(it->second.data.data() + off, data, (size_t)sz);
    return true;
}
bool NkDevice_SW::WriteBufferAsync(NkBufferHandle buf, const void* data, uint64 sz, uint64 off) {
    return WriteBuffer(buf, data, sz, off);
}
bool NkDevice_SW::ReadBuffer(NkBufferHandle buf, void* out, uint64 sz, uint64 off) {
    auto it = mBuffers.find(buf.id); if (it == mBuffers.end()) return false;
    memcpy(out, it->second.data.data() + off, (size_t)sz);
    return true;
}
NkMappedMemory NkDevice_SW::MapBuffer(NkBufferHandle buf, uint64 off, uint64 sz) {
    auto it = mBuffers.find(buf.id); if (it == mBuffers.end()) return {};
    uint64 mapSz = sz > 0 ? sz : it->second.desc.sizeBytes - off;
    it->second.mapped = true;
    return { it->second.data.data() + off, mapSz };
}
void NkDevice_SW::UnmapBuffer(NkBufferHandle buf) {
    auto it = mBuffers.find(buf.id); if (it != mBuffers.end()) it->second.mapped = false;
}

// =============================================================================
// Textures
// =============================================================================
NkTextureHandle NkDevice_SW::CreateTexture(const NkTextureDesc& desc) {
    std::lock_guard<std::mutex> lock(mMutex);
    NkSWTexture t;
    t.desc = desc;
    t.isRenderTarget = NkHasFlag(desc.bindFlags, NkBindFlags::RenderTarget) ||
                       NkHasFlag(desc.bindFlags, NkBindFlags::DepthStencil);

    uint32 bpp = NkFormatBytesPerPixel(desc.format);
    uint32 mipCount = desc.mipLevels == 0
        ? (uint32)(std::floor(std::log2(std::max(desc.width, desc.height))) + 1)
        : desc.mipLevels;

    uint32 w = desc.width, h = desc.height;
    for (uint32 m = 0; m < mipCount; m++) {
        uint32 sz = std::max(1u,w) * std::max(1u,h) * bpp;
        t.mips.emplace_back(sz, (uint8_t)(desc.format==NkFormat::D32_Float ? 0x3F : 0));
        // Init depth à 1.0
        if (desc.format == NkFormat::D32_Float) {
            float one = 1.0f;
            for (uint32 i = 0; i < std::max(1u,w)*std::max(1u,h); i++)
                memcpy(t.mips[m].data() + i*4, &one, 4);
        }
        w >>= 1; h >>= 1;
    }

    if (desc.initialData) {
        uint32 rp = desc.rowPitch > 0 ? desc.rowPitch : desc.width * bpp;
        uint32 dataSz = std::min((uint32)t.mips[0].size(), rp * desc.height);
        memcpy(t.mips[0].data(), desc.initialData, dataSz);
    }

    uint64 hid = NextId(); mTextures[hid] = std::move(t);
    NkTextureHandle h; h.id = hid; return h;
}

void NkDevice_SW::DestroyTexture(NkTextureHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex); mTextures.erase(h.id); h.id = 0;
}

bool NkDevice_SW::WriteTexture(NkTextureHandle t, const void* p, uint32 rp) {
    auto it = mTextures.find(t.id); if (it == mTextures.end()) return false;
    auto& tex = it->second;
    uint32 bpp = tex.Bpp();
    uint32 stride = rp > 0 ? rp : tex.Width() * bpp;
    for (uint32 y = 0; y < tex.Height(); y++)
        memcpy(tex.mips[0].data() + y * tex.Width() * bpp,
               (const uint8*)p + y * stride,
               tex.Width() * bpp);
    return true;
}

bool NkDevice_SW::WriteTextureRegion(NkTextureHandle t, const void* pixels,
    uint32 x, uint32 y, uint32 /*z*/, uint32 w, uint32 h, uint32 /*d2*/,
    uint32 mip, uint32 /*layer*/, uint32 rowPitch) {
    auto it = mTextures.find(t.id); if (it == mTextures.end()) return false;
    auto& tex = it->second;
    if (mip >= tex.mips.size()) return false;
    uint32 bpp = tex.Bpp();
    uint32 stride = rowPitch > 0 ? rowPitch : w * bpp;
    for (uint32 row = 0; row < h; row++) {
        uint32 dstY = y + row;
        if (dstY >= tex.Height(mip)) break;
        memcpy(tex.mips[mip].data() + (dstY * tex.Width(mip) + x) * bpp,
               (const uint8*)pixels + row * stride,
               std::min(w * bpp, (tex.Width(mip) - x) * bpp));
    }
    return true;
}

bool NkDevice_SW::GenerateMipmaps(NkTextureHandle t, NkFilter f) {
    auto it = mTextures.find(t.id); if (it == mTextures.end()) return false;
    auto& tex = it->second;
    for (uint32 m = 1; m < (uint32)tex.mips.size(); m++) {
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
NkSamplerHandle NkDevice_SW::CreateSampler(const NkSamplerDesc& d) {
    std::lock_guard<std::mutex> lock(mMutex);
    uint64 hid = NextId(); mSamplers[hid] = { d };
    NkSamplerHandle h; h.id = hid; return h;
}
void NkDevice_SW::DestroySampler(NkSamplerHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex); mSamplers.erase(h.id); h.id = 0;
}

// =============================================================================
// Shaders
// =============================================================================
NkShaderHandle NkDevice_SW::CreateShader(const NkShaderDesc& desc) {
    std::lock_guard<std::mutex> lock(mMutex);
    NkSWShader sh;
    // Shaders CPU passés via ptrData
    for (uint32 i = 0; i < desc.stageCount; i++) {
        auto& s = desc.stages[i];
        if (s.stage == NkShaderStage::Vertex   && s.cpuVertFn) sh.vertFn = s.cpuVertFn;
        if (s.stage == NkShaderStage::Fragment && s.cpuFragFn) sh.fragFn = s.cpuFragFn;
        if (s.stage == NkShaderStage::Compute  && s.cpuCompFn) {
            sh.isCompute = true;
            sh.computeFn = s.cpuCompFn;
        }
    }
    // Shader par défaut si non fourni : couleur de vertex passthrough
    if (!sh.fragFn && !sh.isCompute)
        sh.fragFn = [](const NkSWVertex& v, const void*, const void*) { return v.color; };

    uint64 hid = NextId(); mShaders[hid] = std::move(sh);
    NkShaderHandle h; h.id = hid; return h;
}
void NkDevice_SW::DestroyShader(NkShaderHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex); mShaders.erase(h.id); h.id = 0;
}

// =============================================================================
// Pipelines
// =============================================================================
NkPipelineHandle NkDevice_SW::CreateGraphicsPipeline(const NkGraphicsPipelineDesc& d) {
    std::lock_guard<std::mutex> lock(mMutex);
    NkSWPipeline p;
    p.shaderId     = d.shader.id;
    p.isCompute    = false;
    p.depthTest    = d.depthStencil.depthTestEnable;
    p.depthWrite   = d.depthStencil.depthWriteEnable;
    p.depthOp      = d.depthStencil.depthCompareOp;
    p.cullMode     = d.rasterizer.cullMode;
    p.frontFace    = d.rasterizer.frontFace;
    p.topology     = d.topology;
    p.vertexStride = d.vertexLayout.bindingCount > 0 ? d.vertexLayout.bindings[0].stride : 0;
    if (d.blend.attachmentCount > 0) {
        p.blendEnable = d.blend.attachments[0].blendEnable;
        p.srcColor    = d.blend.attachments[0].srcColor;
        p.dstColor    = d.blend.attachments[0].dstColor;
    }
    uint64 hid = NextId(); mPipelines[hid] = p;
    NkPipelineHandle h; h.id = hid; return h;
}

NkPipelineHandle NkDevice_SW::CreateComputePipeline(const NkComputePipelineDesc& d) {
    std::lock_guard<std::mutex> lock(mMutex);
    NkSWPipeline p;
    p.shaderId  = d.shader.id;
    p.isCompute = true;
    uint64 hid = NextId(); mPipelines[hid] = p;
    NkPipelineHandle h; h.id = hid; return h;
}

void NkDevice_SW::DestroyPipeline(NkPipelineHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex); mPipelines.erase(h.id); h.id = 0;
}

// =============================================================================
// Render Passes & Framebuffers
// =============================================================================
NkRenderPassHandle NkDevice_SW::CreateRenderPass(const NkRenderPassDesc& d) {
    std::lock_guard<std::mutex> lock(mMutex);
    uint64 hid = NextId(); mRenderPasses[hid] = { d };
    NkRenderPassHandle h; h.id = hid; return h;
}
void NkDevice_SW::DestroyRenderPass(NkRenderPassHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex); mRenderPasses.erase(h.id); h.id = 0;
}
NkFramebufferHandle NkDevice_SW::CreateFramebuffer(const NkFramebufferDesc& d) {
    std::lock_guard<std::mutex> lock(mMutex);
    NkSWFramebuffer fb;
    fb.colorId = d.colorCount > 0 ? d.colorAttachments[0].id : 0;
    fb.depthId = d.depthAttachment.id;
    fb.w = d.width; fb.h = d.height;
    uint64 hid = NextId(); mFramebuffers[hid] = fb;
    NkFramebufferHandle h; h.id = hid; return h;
}
void NkDevice_SW::DestroyFramebuffer(NkFramebufferHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex); mFramebuffers.erase(h.id); h.id = 0;
}

// =============================================================================
// Descriptor Sets
// =============================================================================
NkDescSetHandle NkDevice_SW::CreateDescriptorSetLayout(const NkDescriptorSetLayoutDesc& d) {
    std::lock_guard<std::mutex> lock(mMutex);
    uint64 hid = NextId(); mDescLayouts[hid] = { d };
    NkDescSetHandle h; h.id = hid; return h;
}
void NkDevice_SW::DestroyDescriptorSetLayout(NkDescSetHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex); mDescLayouts.erase(h.id); h.id = 0;
}
NkDescSetHandle NkDevice_SW::AllocateDescriptorSet(NkDescSetHandle layout) {
    std::lock_guard<std::mutex> lock(mMutex);
    NkSWDescSet ds;
    uint64 hid = NextId(); mDescSets[hid] = ds;
    NkDescSetHandle h; h.id = hid; return h;
}
void NkDevice_SW::FreeDescriptorSet(NkDescSetHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex); mDescSets.erase(h.id); h.id = 0;
}
void NkDevice_SW::UpdateDescriptorSets(const NkDescriptorWrite* writes, uint32 n) {
    std::lock_guard<std::mutex> lock(mMutex);
    for (uint32 i = 0; i < n; i++) {
        auto& w = writes[i];
        auto sit = mDescSets.find(w.set.id); if (sit == mDescSets.end()) continue;
        NkSWDescSet::Binding b{ w.binding, w.type, w.buffer.id, w.texture.id, w.sampler.id };
        bool found = false;
        for (auto& e : sit->second.bindings) if (e.slot == w.binding) { e = b; found = true; break; }
        if (!found) sit->second.bindings.push_back(b);
    }
}

// =============================================================================
// Command Buffers
// =============================================================================
NkICommandBuffer* NkDevice_SW::CreateCommandBuffer(NkCommandBufferType t) {
    return new NkCommandBuffer_SW(this, t);
}
void NkDevice_SW::DestroyCommandBuffer(NkICommandBuffer*& cb) { delete cb; cb = nullptr; }

// =============================================================================
// Submit
// =============================================================================
void NkDevice_SW::Submit(NkICommandBuffer* const* cbs, uint32 n, NkFenceHandle fence) {
    for (uint32 i = 0; i < n; i++) {
        auto* sw = dynamic_cast<NkCommandBuffer_SW*>(cbs[i]);
        if (sw) sw->Execute(this);
    }
    if (fence.IsValid()) {
        auto it = mFences.find(fence.id); if (it != mFences.end()) it->second.signaled = true;
    }
}

void NkDevice_SW::SubmitAndPresent(NkICommandBuffer* cb) {
    NkICommandBuffer* cbs[] = { cb };
    Submit(cbs, 1, {});
    Present();
}

void NkDevice_SW::Present() {
    // Hook : copier le backbuffer vers la surface native si disponible
    // Le contexte SW peut utiliser BackbufferPixels() pour afficher via SDL/SFML etc.
}

const uint8_t* NkDevice_SW::BackbufferPixels() const {
    auto fbit = mFramebuffers.find(mSwapchainFB.id);
    if (fbit == mFramebuffers.end()) return nullptr;
    auto tit = mTextures.find(fbit->second.colorId);
    if (tit == mTextures.end() || tit->second.mips.empty()) return nullptr;
    return tit->second.mips[0].data();
}

// =============================================================================
// Fence
// =============================================================================
NkFenceHandle NkDevice_SW::CreateFence(bool signaled) {
    uint64 hid = NextId(); mFences[hid] = { signaled };
    NkFenceHandle h; h.id = hid; return h;
}
void NkDevice_SW::DestroyFence(NkFenceHandle& h) { mFences.erase(h.id); h.id = 0; }
bool NkDevice_SW::WaitFence(NkFenceHandle f, uint64) {
    auto it = mFences.find(f.id); return it != mFences.end() && it->second.signaled;
}
bool NkDevice_SW::IsFenceSignaled(NkFenceHandle f) {
    auto it = mFences.find(f.id); return it != mFences.end() && it->second.signaled;
}
void NkDevice_SW::ResetFence(NkFenceHandle f) {
    auto it = mFences.find(f.id); if (it != mFences.end()) it->second.signaled = false;
}

// =============================================================================
// Frame
// =============================================================================
void NkDevice_SW::BeginFrame(NkFrameContext& frame) {
    // Clear depth
    auto fbit = mFramebuffers.find(mSwapchainFB.id);
    if (fbit != mFramebuffers.end()) {
        auto dit = mTextures.find(fbit->second.depthId);
        if (dit != mTextures.end() && !dit->second.mips.empty()) {
            float one = 1.0f;
            uint32 count = dit->second.Width() * dit->second.Height();
            for (uint32 i = 0; i < count; i++)
                memcpy(dit->second.mips[0].data() + i*4, &one, 4);
        }
    }
    frame.frameIndex  = mFrameIndex;
    frame.frameNumber = mFrameNumber;
}
void NkDevice_SW::EndFrame(NkFrameContext&) { ++mFrameNumber; }

void NkDevice_SW::OnResize(uint32 w, uint32 h) {
    if (w == 0 || h == 0) return;
    mWidth = w; mHeight = h;
    DestroyFramebuffer(mSwapchainFB);
    DestroyRenderPass(mSwapchainRP);
    CreateSwapchainObjects();
}

// =============================================================================
// Accesseurs
// =============================================================================
NkSWBuffer*      NkDevice_SW::GetBuf  (uint64 id) { auto it=mBuffers.find(id);    return it!=mBuffers.end()    ?&it->second:nullptr; }
NkSWTexture*     NkDevice_SW::GetTex  (uint64 id) { auto it=mTextures.find(id);   return it!=mTextures.end()   ?&it->second:nullptr; }
NkSWSampler*     NkDevice_SW::GetSamp (uint64 id) { auto it=mSamplers.find(id);   return it!=mSamplers.end()   ?&it->second:nullptr; }
NkSWShader*      NkDevice_SW::GetShader(uint64 id){ auto it=mShaders.find(id);    return it!=mShaders.end()    ?&it->second:nullptr; }
NkSWPipeline*    NkDevice_SW::GetPipe (uint64 id) { auto it=mPipelines.find(id);  return it!=mPipelines.end()  ?&it->second:nullptr; }
NkSWDescSet*     NkDevice_SW::GetDescSet(uint64 id){auto it=mDescSets.find(id);   return it!=mDescSets.end()   ?&it->second:nullptr; }
NkSWFramebuffer* NkDevice_SW::GetFBO  (uint64 id) { auto it=mFramebuffers.find(id);return it!=mFramebuffers.end()?&it->second:nullptr;}

} // namespace nkentseu
