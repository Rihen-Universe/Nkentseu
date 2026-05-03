// =============================================================================
// NkTextureLibrary.cpp  — NKRenderer v4.0
// =============================================================================
#include "NkTextureLibrary.h"
#include "NKImage/NKImage.h"        // Backend NKImage par défaut
#include <cmath>
#include <cstring>

namespace nkentseu {
namespace renderer {

    NkTextureLibrary::~NkTextureLibrary() { Shutdown(); }

    bool NkTextureLibrary::Init(NkIDevice* device) {
        mDevice = device;
        // Créer les textures built-in
        mWhite  = CreateBuiltin(255, 255, 255, 255, "White1x1");
        mBlack  = CreateBuiltin(  0,   0,   0, 255, "Black1x1");
        mNormal = CreateBuiltin(128, 128, 255, 255, "Normal1x1");
        mError  = CreateBuiltin(255,   0, 255, 255, "Error1x1");
        mBRDFLUT= CreateBRDFLUT();
        return mWhite.IsValid() && mBlack.IsValid() && mNormal.IsValid();
    }

    void NkTextureLibrary::Shutdown() {
        ReleaseAll();
    }

    void NkTextureLibrary::SetCustomLoader(NkImageLoaderFn load,
                                             NkImageFreeFn   free,
                                             void*           user) {
        mCustomLoad = load;
        mCustomFree = free;
        mCustomUser = user;
    }

    // ── Chargement ───────────────────────────────────────────────────────────
    NkTexHandle NkTextureLibrary::Load(const NkString& path, bool srgb, bool genMips) {
        // Cache
        auto* cached = mPathCache.Find(path);
        if (cached) {
            auto* e = mTextures.Find(cached->id);
            if (e) { e->refCount++; return *cached; }
        }

        NkImageData img;
        if (!LoadWithNKImage(path, img)) {
            // Retourner texture erreur si chargement échoue
            return mError;
        }

        NkTextureCreateDesc desc;
        desc.pixels    = img.pixels ? (const void*)img.pixels
                                    : (const void*)img.hdrPixels;
        desc.width     = img.width;
        desc.height    = img.height;
        desc.srgb      = srgb && !img.isHDR;
        desc.genMips   = genMips;
        desc.isHDR     = img.isHDR;
        desc.debugName = path;
        desc.format    = img.isHDR ? NkGPUFormat::NK_RGBA16F
                                   : (srgb ? NkGPUFormat::NK_RGBA8_SRGB
                                           : NkGPUFormat::NK_RGBA8_UNORM);

        NkTexHandle h = UploadToGPU(desc);

        // Libérer pixels CPU
        if (mCustomFree && mCustomLoad)
            mCustomFree(&img, mCustomUser);
        else if (img.pixels)   { delete[] img.pixels;    img.pixels=nullptr; }
        else if (img.hdrPixels){ delete[] img.hdrPixels; img.hdrPixels=nullptr; }

        if (h.IsValid()) {
            mPathCache.Insert(path, h);
            auto* e = mTextures.Find(h.id);
            if (e) e->path = path;
        }
        return h;
    }

    NkTexHandle NkTextureLibrary::LoadHDR(const NkString& path) {
        return Load(path, false, true);
    }

    NkTexHandle NkTextureLibrary::LoadCubemap(const NkString paths[6]) {
        // Charger les 6 faces et créer une cubemap
        NkTextureCreateDesc desc;
        desc.isCubemap = true;
        desc.debugName = "Cubemap";
        // TODO: implémenter via NKRHI CreateTextureCubemap
        return mBlack;
    }

    bool NkTextureLibrary::LoadWithNKImage(const NkString& path, NkImageData& out) {
        if (mCustomLoad) {
            return mCustomLoad(path.CStr(), &out, mCustomUser);
        }
        // Backend NKImage
        NkImage* img = NkImage::Load(path.CStr());
        if (!img) return false;
        out.width    = (uint32)img->width;
        out.height   = (uint32)img->height;
        out.channels = (uint32)img->channels;
        out.isHDR    = img->isHDR;
        if (img->isHDR) {
            uint32 sz = out.width * out.height * 4;
            out.hdrPixels = new float32[sz];
            memcpy(out.hdrPixels, img->pixelsF, sz * sizeof(float32));
        } else {
            uint32 sz = out.width * out.height * 4;
            out.pixels = new uint8[sz];
            // Convert to RGBA if needed
            if (out.channels == 3) {
                for (uint32 i = 0; i < out.width * out.height; i++) {
                    out.pixels[i*4+0] = img->pixels[i*3+0];
                    out.pixels[i*4+1] = img->pixels[i*3+1];
                    out.pixels[i*4+2] = img->pixels[i*3+2];
                    out.pixels[i*4+3] = 255;
                }
            } else {
                memcpy(out.pixels, img->pixels, sz);
            }
        }
        img->Free();
        return true;
    }

    // ── Création manuelle ────────────────────────────────────────────────────
    NkTexHandle NkTextureLibrary::Create(const NkTextureCreateDesc& desc) {
        return UploadToGPU(desc);
    }

    NkTexHandle NkTextureLibrary::CreateRenderTarget(uint32 w, uint32 h,
                                                       NkGPUFormat format,
                                                       bool depth, bool readable,
                                                       const NkString& name) {
        NkTextureDesc rhiDesc;
        rhiDesc.width  = w;
        rhiDesc.height = h;
        rhiDesc.depth  = 1;
        rhiDesc.format = format;
        rhiDesc.usage  = depth
            ? (NkTextureUsage::NK_DEPTH_STENCIL | (readable ? NkTextureUsage::NK_SHADER_RESOURCE : 0))
            : (NkTextureUsage::NK_RENDER_TARGET  | (readable ? NkTextureUsage::NK_SHADER_RESOURCE : 0));
        rhiDesc.name   = name;

        NkTextureHandle rhi = mDevice->CreateTexture(rhiDesc);
        if (!rhi.IsValid()) return NkTexHandle::Null();

        NkSamplerDesc sd;
        sd.filter    = NkSamplerFilter::NK_LINEAR;
        sd.addressU  = NkSamplerAddress::NK_CLAMP;
        sd.addressV  = NkSamplerAddress::NK_CLAMP;
        NkSamplerHandle samp = mDevice->CreateSampler(sd);

        NkTexHandle h = AllocHandle();
        TexEntry e;
        e.rhi      = rhi;
        e.sampler  = samp;
        e.refCount = 1;
        e.isRT     = true;
        e.path     = name;
        mTextures.Insert(h.id, e);
        return h;
    }

    NkTexHandle NkTextureLibrary::UploadToGPU(const NkTextureCreateDesc& desc) {
        if (!desc.pixels && !desc.isHDR) {
            // Juste créer la texture sans données initiales
        }

        NkTextureDesc rhiDesc;
        rhiDesc.width    = desc.width;
        rhiDesc.height   = desc.height;
        rhiDesc.depth    = desc.depth;
        rhiDesc.mipCount = desc.mipLevels;
        rhiDesc.format   = desc.format;
        rhiDesc.usage    = NkTextureUsage::NK_SHADER_RESOURCE;
        if (desc.genMips) rhiDesc.usage |= NkTextureUsage::NK_GENERATE_MIPS;
        rhiDesc.name     = desc.debugName;

        NkTextureHandle rhi = mDevice->CreateTextureWithData(rhiDesc, desc.pixels,
                                                               desc.width * 4);
        if (!rhi.IsValid()) return NkTexHandle::Null();

        if (desc.genMips) mDevice->GenerateMipmaps(rhi);

        NkSamplerDesc sd;
        sd.filter    = NkSamplerFilter::NK_LINEAR_MIPMAP;
        sd.addressU  = NkSamplerAddress::NK_REPEAT;
        sd.addressV  = NkSamplerAddress::NK_REPEAT;
        sd.maxAniso  = 16.f;
        NkSamplerHandle samp = mDevice->CreateSampler(sd);

        NkTexHandle h = AllocHandle();
        TexEntry e; e.rhi=rhi; e.sampler=samp; e.refCount=1;
        mTextures.Insert(h.id, e);
        return h;
    }

    // ── Built-ins ────────────────────────────────────────────────────────────
    NkTexHandle NkTextureLibrary::CreateBuiltin(uint8 r, uint8 g, uint8 b, uint8 a,
                                                  const NkString& name) {
        uint8 px[4] = {r,g,b,a};
        NkTextureCreateDesc d;
        d.pixels    = px;
        d.width     = 1; d.height = 1;
        d.srgb      = false; d.genMips = false;
        d.format    = NkGPUFormat::NK_RGBA8_UNORM;
        d.debugName = name;
        return UploadToGPU(d);
    }

    NkTexHandle NkTextureLibrary::CreateBRDFLUT() {
        // Pre-compute BRDF LUT 256x256 (Cook-Torrance split-sum)
        const uint32 SZ = 256;
        NkVector<uint16> lut(SZ * SZ * 2);
        for (uint32 y = 0; y < SZ; y++) {
            float32 NdotV = (y + 0.5f) / SZ;
            for (uint32 x = 0; x < SZ; x++) {
                float32 rough = (x + 0.5f) / SZ;
                // Hammersley importance sampling
                float32 A = 0.f, B = 0.f;
                const uint32 SAMPLES = 1024;
                for (uint32 i = 0; i < SAMPLES; i++) {
                    // Hammersley sequence
                    float32 phi = 2.f * 3.14159f * (float32)i / (float32)SAMPLES;
                    uint32 bits = i;
                    bits = (bits << 16u) | (bits >> 16u);
                    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
                    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
                    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
                    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
                    float32 xi = (float32)bits * 2.3283064365386963e-10f;
                    // GGX importance sample
                    float32 a  = rough * rough;
                    float32 cosT= sqrtf((1.f-xi)/(1.f+(a*a-1.f)*xi));
                    float32 sinT= sqrtf(1.f - cosT*cosT);
                    float32 Hx  = sinT*cosf(phi), Hy=sinT*sinf(phi), Hz=cosT;
                    float32 VdH = NdotV*Hz + sqrtf(1.f-NdotV*NdotV)*Hx;
                    float32 NdH = Hz, NdL = 2.f*VdH*Hz - NdotV;
                    if (NdL > 0.f) {
                        float32 k = rough * rough / 2.f;
                        float32 G = (NdotV/(NdotV*(1.f-k)+k)) * (NdL/(NdL*(1.f-k)+k));
                        float32 GVis = G*VdH/(NdH*NdotV+1e-7f);
                        float32 Fc = powf(1.f-VdH, 5.f);
                        A += (1.f-Fc)*GVis;
                        B += Fc*GVis;
                    }
                }
                A /= SAMPLES; B /= SAMPLES;
                lut[(y*SZ+x)*2+0] = (uint16)(A * 65535.f);
                lut[(y*SZ+x)*2+1] = (uint16)(B * 65535.f);
            }
        }

        NkTextureCreateDesc d;
        d.pixels    = lut.Data();
        d.width     = SZ; d.height = SZ;
        d.srgb      = false; d.genMips = false;
        d.format    = NkGPUFormat::NK_RG16F;
        d.debugName = "BRDFLUT";
        return UploadToGPU(d);
    }

    // ── Update ────────────────────────────────────────────────────────────────
    bool NkTextureLibrary::Update(NkTexHandle h, const void* data,
                                    uint32 rowPitch, uint32 mipLevel, uint32 layer) {
        auto* e = mTextures.Find(h.id);
        if (!e) return false;
        return mDevice->UpdateTexture(e->rhi, data, rowPitch, mipLevel, layer);
    }

    // ── Release ───────────────────────────────────────────────────────────────
    void NkTextureLibrary::Release(NkTexHandle& h) {
        auto* e = mTextures.Find(h.id);
        if (!e) return;
        if (--e->refCount == 0) {
            mDevice->DestroyTexture(e->rhi);
            if (e->sampler.IsValid()) mDevice->DestroySampler(e->sampler);
            if (!e->path.Empty()) mPathCache.Remove(e->path);
            mTextures.Remove(h.id);
        }
        h = NkTexHandle::Null();
    }

    void NkTextureLibrary::ReleaseAll() {
        for (auto& [id, e] : mTextures) {
            mDevice->DestroyTexture(e.rhi);
            if (e.sampler.IsValid()) mDevice->DestroySampler(e.sampler);
        }
        mTextures.Clear();
        mPathCache.Clear();
    }

    // ── Accès RHI ────────────────────────────────────────────────────────────
    NkTextureHandle NkTextureLibrary::GetRHIHandle(NkTexHandle h) const {
        auto* e = mTextures.Find(h.id);
        return e ? e->rhi : NkTextureHandle{};
    }
    NkSamplerHandle NkTextureLibrary::GetRHISampler(NkTexHandle h) const {
        auto* e = mTextures.Find(h.id);
        return e ? e->sampler : NkSamplerHandle{};
    }

    NkTexHandle NkTextureLibrary::AllocHandle() {
        return {mNextId++};
    }

} // namespace renderer
} // namespace nkentseu
