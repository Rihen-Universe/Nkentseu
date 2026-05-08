// =============================================================================
// NkTextureLibrary.cpp  — NKRenderer v5.0
// =============================================================================
#include "NkTextureLibrary.h"
#include "NkResources.h"
#include "NKImage/NKImage.h"
#include <cmath>
#include <cstring>

namespace nkentseu {
    namespace renderer {

        NkTextureLibrary::~NkTextureLibrary() { Shutdown(); }

        // =====================================================================
        // Lifecycle
        // =====================================================================
        NkRResult NkTextureLibrary::Init(NkIDevice* device, NkResources* resources) {
            if (!device) {
                NkRSetLastError(NkRResult::NK_ERR_INVALID_DEVICE,
                                "NkTextureLibrary::Init device==nullptr");
                return NkRResult::NK_ERR_INVALID_DEVICE;
            }
            mDevice    = device;
            mResources = resources;

            // Defaults : si NkResources est dispo, on enveloppe ses textures et samplers ;
            // sinon on cree des fallback locaux.
            NkSamplerHandle defaultSampler{};
            if (mResources && mResources->IsReady()) {
                defaultSampler = mResources->GetSamplerLinearRepeat();
                // ownsSampler=false : les samplers appartiennent a NkResources
                // ownsTexture=false : les textures default appartiennent a NkResources
                //                    (sinon double-free au shutdown).
                mWhite  = WrapRHI(mResources->GetWhiteTex(),   defaultSampler, 1, 1, 1, "White1x1",  false, false);
                mBlack  = WrapRHI(mResources->GetBlackTex(),   defaultSampler, 1, 1, 1, "Black1x1",  false, false);
                mNormal = WrapRHI(mResources->GetNormalTex(),  defaultSampler, 1, 1, 1, "Normal1x1", false, false);
                mError  = WrapRHI(mResources->GetMagentaTex(), defaultSampler, 1, 1, 1, "Error1x1",  false, false);
            } else {
                // Fallback : creer des 1x1 en interne
                NkSamplerDesc sd = NkSamplerDesc::Linear();
                sd.addressU = sd.addressV = sd.addressW = NkAddressMode::NK_REPEAT;
                NkSamplerHandle s = mDevice->CreateSampler(sd);
                if (!s.IsValid()) {
                    NkRSetLastError(NkRResult::NK_ERR_OUT_OF_MEMORY,
                                    "NkTextureLibrary fallback sampler failed");
                    return NkRResult::NK_ERR_OUT_OF_MEMORY;
                }

                auto Make1x1 = [&](uint8 r, uint8 g, uint8 b, uint8 a, const char* name) {
                    uint8 px[4] = {r,g,b,a};
                    NkTextureDesc d = NkTextureDesc::Tex2D(1,1, NkGPUFormat::NK_RGBA8_UNORM, 1);
                    d.bindFlags = NkBindFlags::NK_SHADER_RESOURCE;
                    d.usage = NkResourceUsage::NK_DEFAULT;
                    d.initialData = px;
                    d.debugName = name;
                    NkTextureHandle rhi = mDevice->CreateTexture(d);
                    return WrapRHI(rhi, s, 1, 1, 1, name, false);
                };
                mWhite  = Make1x1(255,255,255,255, "White1x1");
                mBlack  = Make1x1(  0,  0,  0,255, "Black1x1");
                mNormal = Make1x1(128,128,255,255, "Normal1x1");
                mError  = Make1x1(255,  0,255,255, "Error1x1");

                // Le sampler appartient a la lib (a detruire au shutdown)
                // -> on le memorise dans une entry orpheline. Plus simple :
                //    chaque entry a son propre sampler (ownsSampler flag).
                // Pour cette branche fallback, on owne le sampler via les 4 entries.
                // NB: Release detruira le sampler 4 fois — race possible.
                // Solution : marquer ownsSampler=true sur la premiere seulement.
                if (auto* eW = mTextures.Find(mWhite.id))  eW->ownsSampler = true;
            }
            return NkRResult::NK_OK;
        }

        void NkTextureLibrary::Shutdown() { ReleaseAll(); }

        void NkTextureLibrary::SetCustomLoader(NkImageLoaderFn load,
                                                NkImageFreeFn free, void* user) {
            mCustomLoad = load;
            mCustomFree = free;
            mCustomUser = user;
        }

        // =====================================================================
        // Helpers internes
        // =====================================================================
        NkTexHandle NkTextureLibrary::AllocHandle() { return {mNextId++}; }

        NkTexHandle NkTextureLibrary::WrapRHI(NkTextureHandle rhi, NkSamplerHandle sampler,
                                              uint32 w, uint32 h, uint32 mips,
                                              const NkString& dbgName,
                                              bool ownsSampler, bool ownsTexture) {
            if (!rhi.IsValid()) return NkTexHandle::Null();
            NkTexHandle out = AllocHandle();
            TexEntry e;
            e.rhi         = rhi;
            e.sampler     = sampler;
            e.path        = dbgName;
            e.width       = w;
            e.height      = h;
            e.mipLevels   = mips;
            e.refCount    = 1;
            e.ownsTexture = ownsTexture;
            e.ownsSampler = ownsSampler;
            e.bytes       = EstimateBytes(w, h, mips, 4);
            mTotalBytes  += e.bytes;
            mTextures.Insert(out.id, e);
            return out;
        }

        uint64 NkTextureLibrary::EstimateBytes(uint32 w, uint32 h, uint32 mips,
                                                uint32 bpp, uint32 layers) {
            uint64 total = 0;
            uint32 cw = w, ch = h;
            for (uint32 m = 0; m < mips; m++) {
                total += (uint64)cw * ch * bpp;
                if (cw > 1) cw >>= 1;
                if (ch > 1) ch >>= 1;
            }
            return total * layers;
        }

        NkSamplerHandle NkTextureLibrary::PickSampler(const NkLoadOptions& opts) const {
            if (!mResources || !mResources->IsReady()) return NkSamplerHandle{};
            if (opts.useAnisotropic && !opts.useClampEdge) return mResources->GetSamplerAnisotropic16();
            if (opts.useClampEdge)                         return mResources->GetSamplerLinearClamp();
            return mResources->GetSamplerLinearRepeat();
        }

        // =====================================================================
        // NKImage backend
        // =====================================================================
        bool NkTextureLibrary::LoadWithNKImage(const NkString& path, NkImageData& out) {
            if (mCustomLoad) {
                return mCustomLoad(path.CStr(), &out, mCustomUser);
            }
            // Force 4 channels (RGBA) — NkImage gere la conversion en interne
            NkImage* img = NkImage::Load(path.CStr(), 4);
            if (!img) return false;

            out.width    = (uint32)img->Width();
            out.height   = (uint32)img->Height();
            out.channels = 4;
            out.isHDR    = img->IsHDR();

            if (out.isHDR) {
                // NK_RGBA128F : 4 floats par pixel
                uint64 sz = (uint64)out.width * out.height * 4;
                out.hdrPixels = new float32[sz];
                memcpy(out.hdrPixels, img->Pixels(), sz * sizeof(float32));
            } else {
                uint64 sz = (uint64)out.width * out.height * 4;
                out.pixels = new uint8[sz];
                memcpy(out.pixels, img->Pixels(), sz);
            }
            img->Free();
            return true;
        }

        void NkTextureLibrary::FreeImageData(NkImageData& d) {
            if (mCustomFree) {
                mCustomFree(&d, mCustomUser);
            } else {
                if (d.pixels)    { delete[] d.pixels;    d.pixels=nullptr; }
                if (d.hdrPixels) { delete[] d.hdrPixels; d.hdrPixels=nullptr; }
            }
        }

        // =====================================================================
        // Upload helpers
        // =====================================================================
        NkTexHandle NkTextureLibrary::UploadColorTexture(const uint8* rgba,
                                                         uint32 w, uint32 h,
                                                         bool srgb, bool genMips,
                                                         NkSamplerHandle sampler,
                                                         const char* debugName) {
            if (!rgba || w == 0 || h == 0) return NkTexHandle::Null();

            // Calcul du nombre de mips
            uint32 mips = 1;
            if (genMips) {
                uint32 cw = w, ch = h;
                while (cw > 1 || ch > 1) {
                    if (cw > 1) cw >>= 1;
                    if (ch > 1) ch >>= 1;
                    mips++;
                }
            }

            NkTextureDesc d;
            d.type        = NkTextureType::NK_TEX2D;
            d.format      = srgb ? NkGPUFormat::NK_RGBA8_SRGB : NkGPUFormat::NK_RGBA8_UNORM;
            d.width       = w;
            d.height      = h;
            d.depth       = 1;
            d.arrayLayers = 1;
            d.mipLevels   = mips;
            d.bindFlags   = NkBindFlags::NK_SHADER_RESOURCE;
            d.usage       = NkResourceUsage::NK_DEFAULT;
            d.initialData = rgba;
            d.rowPitch    = w * 4;
            d.debugName   = debugName;

            NkTextureHandle rhi = mDevice->CreateTexture(d);
            if (!rhi.IsValid()) return NkTexHandle::Null();

            // Genere les mips secondaires (mip 0 deja fourni)
            if (mips > 1) mDevice->GenerateMipmaps(rhi, NkFilter::NK_LINEAR);

            return WrapRHI(rhi, sampler, w, h, mips, debugName, false);
        }

        NkTexHandle NkTextureLibrary::UploadHDRTexture(const float32* rgbaF,
                                                       uint32 w, uint32 h,
                                                       NkSamplerHandle sampler,
                                                       const char* debugName) {
            if (!rgbaF || w == 0 || h == 0) return NkTexHandle::Null();

            NkTextureDesc d;
            d.type        = NkTextureType::NK_TEX2D;
            d.format      = NkGPUFormat::NK_RGBA32_FLOAT;
            d.width       = w;
            d.height      = h;
            d.depth       = 1;
            d.arrayLayers = 1;
            d.mipLevels   = 1;
            d.bindFlags   = NkBindFlags::NK_SHADER_RESOURCE;
            d.usage       = NkResourceUsage::NK_DEFAULT;
            d.initialData = rgbaF;
            d.rowPitch    = w * 16;       // 4 floats * 4 bytes
            d.debugName   = debugName;

            NkTextureHandle rhi = mDevice->CreateTexture(d);
            if (!rhi.IsValid()) return NkTexHandle::Null();

            return WrapRHI(rhi, sampler, w, h, 1, debugName, false);
        }

        NkTexHandle NkTextureLibrary::UploadCubemap(const uint8* faces[6],
                                                     uint32 w, uint32 h,
                                                     bool srgb,
                                                     NkSamplerHandle sampler,
                                                     const char* debugName) {
            for (int i = 0; i < 6; i++) if (!faces[i]) return NkTexHandle::Null();
            if (w != h) return NkTexHandle::Null();    // cubemap doit etre carre

            NkTextureDesc d;
            d.type        = NkTextureType::NK_CUBE;
            d.format      = srgb ? NkGPUFormat::NK_RGBA8_SRGB : NkGPUFormat::NK_RGBA8_UNORM;
            d.width       = w;
            d.height      = h;
            d.depth       = 1;
            d.arrayLayers = 6;
            d.mipLevels   = 1;
            d.bindFlags   = NkBindFlags::NK_SHADER_RESOURCE;
            d.usage       = NkResourceUsage::NK_DEFAULT;
            d.initialData = nullptr;        // upload face-par-face apres
            d.debugName   = debugName;

            NkTextureHandle rhi = mDevice->CreateTexture(d);
            if (!rhi.IsValid()) return NkTexHandle::Null();

            // Upload des 6 faces
            for (uint32 f = 0; f < 6; f++) {
                mDevice->WriteTextureRegion(rhi, faces[f],
                                            0, 0, 0,
                                            w, h, 1,
                                            0, f,                  // mipLevel=0, layer=f
                                            w * 4);
            }

            NkTexHandle out = WrapRHI(rhi, sampler, w, h, 1, debugName, false);
            if (auto* e = mTextures.Find(out.id)) {
                e->isCube = true;
                e->bytes  = EstimateBytes(w, h, 1, 4, 6);
            }
            return out;
        }

        // =====================================================================
        // Public Load API
        // =====================================================================
        NkTexHandle NkTextureLibrary::Load(const NkString& path, const NkLoadOptions& opts) {
            // Cache hit
            auto* cached = mPathCache.Find(path);
            if (cached) {
                if (auto* e = mTextures.Find(cached->id)) {
                    e->refCount++;
                    return *cached;
                }
            }

            NkImageData img{};
            if (!LoadWithNKImage(path, img)) {
                NkRSetLastError(NkRResult::NK_ERR_IO, "NkTextureLibrary::Load file not found");
                return mError;
            }

            NkSamplerHandle samp = PickSampler(opts);
            const char* dbg = opts.debugName ? opts.debugName : path.CStr();

            NkTexHandle out;
            if (img.isHDR) {
                out = UploadHDRTexture(img.hdrPixels, img.width, img.height, samp, dbg);
            } else {
                out = UploadColorTexture(img.pixels, img.width, img.height,
                                          opts.srgb, opts.genMipmaps, samp, dbg);
            }
            FreeImageData(img);

            if (out.IsValid()) {
                mPathCache.Insert(path, out);
                if (auto* e = mTextures.Find(out.id)) e->path = path;
            }
            return out;
        }

        NkTexHandle NkTextureLibrary::LoadHDR(const NkString& path, const NkLoadOptions& opts) {
            // HDR : par defaut srgb=false (deja en lineaire), genMips=opt
            NkLoadOptions o = opts;
            o.srgb = false;
            return Load(path, o);
        }

        NkTexHandle NkTextureLibrary::LoadCubemap(const NkString paths[6], const NkLoadOptions& opts) {
            // Charge les 6 faces
            NkImageData faces[6]{};
            uint32 W = 0, H = 0;
            for (int i = 0; i < 6; i++) {
                if (!LoadWithNKImage(paths[i], faces[i])) {
                    for (int j = 0; j < i; j++) FreeImageData(faces[j]);
                    NkRSetLastError(NkRResult::NK_ERR_IO,
                                    "NkTextureLibrary::LoadCubemap face missing");
                    return mError;
                }
                if (i == 0) { W = faces[i].width; H = faces[i].height; }
                else if (faces[i].width != W || faces[i].height != H) {
                    // Toutes les faces doivent etre de meme taille
                    for (int j = 0; j <= i; j++) FreeImageData(faces[j]);
                    NkRSetLastError(NkRResult::NK_ERR_BAD_FORMAT,
                                    "NkTextureLibrary::LoadCubemap faces de taille differente");
                    return mError;
                }
            }

            const uint8* facesPtr[6] = {
                faces[0].pixels, faces[1].pixels, faces[2].pixels,
                faces[3].pixels, faces[4].pixels, faces[5].pixels
            };
            NkSamplerHandle samp = mResources && mResources->IsReady()
                                 ? mResources->GetSamplerCubemap()
                                 : PickSampler(opts);
            const char* dbg = opts.debugName ? opts.debugName : "Cubemap";
            NkTexHandle out = UploadCubemap(facesPtr, W, H, opts.srgb, samp, dbg);

            for (int i = 0; i < 6; i++) FreeImageData(faces[i]);
            return out;
        }

        // =====================================================================
        // Manuel
        // =====================================================================
        NkTexHandle NkTextureLibrary::Create(const NkTextureCreateDesc& desc) {
            NkTextureDesc d;
            d.type        = desc.isCubemap ? NkTextureType::NK_CUBE : NkTextureType::NK_TEX2D;
            d.format      = desc.format;
            d.width       = desc.width;
            d.height      = desc.height;
            d.depth       = desc.depth;
            d.arrayLayers = desc.isCubemap ? 6 : 1;
            d.mipLevels   = desc.mipLevels > 0 ? desc.mipLevels : 1;
            d.bindFlags   = NkBindFlags::NK_SHADER_RESOURCE;
            d.usage       = NkResourceUsage::NK_DEFAULT;
            d.initialData = desc.pixels;
            d.rowPitch    = desc.width * 4;
            d.debugName   = desc.debugName;

            NkTextureHandle rhi = mDevice->CreateTexture(d);
            if (!rhi.IsValid()) return NkTexHandle::Null();

            if (desc.genMips && d.mipLevels > 1) mDevice->GenerateMipmaps(rhi, NkFilter::NK_LINEAR);

            NkSamplerHandle samp = mResources && mResources->IsReady()
                                 ? mResources->GetSamplerLinearRepeat()
                                 : NkSamplerHandle{};
            return WrapRHI(rhi, samp, desc.width, desc.height, d.mipLevels,
                          desc.debugName ? desc.debugName : "Manual", false);
        }

        NkTexHandle NkTextureLibrary::CreateRenderTarget(uint32 w, uint32 h, NkGPUFormat format,
                                                         bool depth, bool readable,
                                                         const NkString& name) {
            NkTextureDesc d;
            d.type        = NkTextureType::NK_TEX2D;
            d.format      = format;
            d.width       = w;
            d.height      = h;
            d.depth       = 1;
            d.arrayLayers = 1;
            d.mipLevels   = 1;
            if (depth) {
                d.bindFlags = NkBindFlags::NK_DEPTH_STENCIL;
                if (readable) d.bindFlags = d.bindFlags | NkBindFlags::NK_SHADER_RESOURCE;
            } else {
                d.bindFlags = NkBindFlags::NK_RENDER_TARGET;
                if (readable) d.bindFlags = d.bindFlags | NkBindFlags::NK_SHADER_RESOURCE;
            }
            d.usage     = NkResourceUsage::NK_DEFAULT;
            d.debugName = name.CStr();

            NkTextureHandle rhi = mDevice->CreateTexture(d);
            if (!rhi.IsValid()) return NkTexHandle::Null();

            NkSamplerHandle samp = mResources && mResources->IsReady()
                                 ? mResources->GetSamplerLinearClamp()
                                 : NkSamplerHandle{};
            NkTexHandle out = WrapRHI(rhi, samp, w, h, 1, name, false);
            if (auto* e = mTextures.Find(out.id)) e->isRT = true;
            return out;
        }

        // =====================================================================
        // Update / Release
        // =====================================================================
        bool NkTextureLibrary::Update(NkTexHandle h, const void* data, uint32 rowPitch,
                                       uint32 mipLevel, uint32 layer) {
            auto* e = mTextures.Find(h.id);
            if (!e) return false;
            if (mipLevel == 0 && layer == 0) {
                return mDevice->WriteTexture(e->rhi, data, rowPitch);
            }
            return mDevice->WriteTextureRegion(e->rhi, data,
                                                0, 0, 0,
                                                e->width >> mipLevel,
                                                e->height >> mipLevel, 1,
                                                mipLevel, layer, rowPitch);
        }

        void NkTextureLibrary::Release(NkTexHandle& h) {
            auto* e = mTextures.Find(h.id);
            if (!e) return;
            if (--e->refCount == 0) {
                if (e->ownsTexture && e->rhi.IsValid())     mDevice->DestroyTexture(e->rhi);
                if (e->ownsSampler && e->sampler.IsValid()) mDevice->DestroySampler(e->sampler);
                if (!e->path.Empty())                       mPathCache.Erase(e->path);
                mTotalBytes -= e->bytes;
                mTextures.Erase(h.id);
            }
            h = NkTexHandle::Null();
        }

        void NkTextureLibrary::ReleaseAll() {
            for (auto& [id, e] : mTextures) {
                if (e.ownsTexture && e.rhi.IsValid())     mDevice->DestroyTexture(e.rhi);
                if (e.ownsSampler && e.sampler.IsValid()) mDevice->DestroySampler(e.sampler);
            }
            mTextures.Clear();
            mPathCache.Clear();
            mTotalBytes = 0;
        }

        // =====================================================================
        // Acces RHI
        // =====================================================================
        NkTextureHandle NkTextureLibrary::GetRHIHandle(NkTexHandle h) const {
            auto* e = mTextures.Find(h.id);
            return e ? e->rhi : NkTextureHandle{};
        }
        NkSamplerHandle NkTextureLibrary::GetRHISampler(NkTexHandle h) const {
            auto* e = mTextures.Find(h.id);
            return e ? e->sampler : NkSamplerHandle{};
        }
        bool NkTextureLibrary::HasMipmaps(NkTexHandle h) const {
            auto* e = mTextures.Find(h.id);
            return e ? (e->mipLevels > 1) : false;
        }

        // =====================================================================
        // BRDF LUT (Cook-Torrance split-sum, calcul Hammersley + GGX)
        // Cache : calcule au premier appel.
        // =====================================================================
        NkTexHandle NkTextureLibrary::GetBRDFLUT() {
            if (mBRDFLUT.IsValid()) return mBRDFLUT;
            mBRDFLUT = CreateBRDFLUT();
            return mBRDFLUT;
        }

        NkTexHandle NkTextureLibrary::CreateBRDFLUT() {
            const uint32 SZ = 256;
            NkVector<uint16> lut;
            lut.Resize(SZ * SZ * 2);

            for (uint32 y = 0; y < SZ; y++) {
                float32 NdotV = (y + 0.5f) / SZ;
                for (uint32 x = 0; x < SZ; x++) {
                    float32 rough = (x + 0.5f) / SZ;
                    float32 A = 0.f, B = 0.f;
                    const uint32 SAMPLES = 1024;
                    for (uint32 i = 0; i < SAMPLES; i++) {
                        // Hammersley sequence (Van der Corput radical inverse base 2)
                        uint32 bits = i;
                        bits = (bits << 16u) | (bits >> 16u);
                        bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
                        bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
                        bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
                        bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
                        float32 xi  = (float32)bits * 2.3283064365386963e-10f;
                        float32 phi = 2.f * 3.14159265f * (float32)i / (float32)SAMPLES;
                        // GGX importance sample
                        float32 a    = rough * rough;
                        float32 cosT = sqrtf((1.f - xi) / (1.f + (a*a - 1.f) * xi));
                        float32 sinT = sqrtf(1.f - cosT * cosT);
                        float32 Hx   = sinT * cosf(phi);
                        float32 Hz   = cosT;
                        float32 VdH  = NdotV * Hz + sqrtf(1.f - NdotV * NdotV) * Hx;
                        float32 NdH  = Hz;
                        float32 NdL  = 2.f * VdH * Hz - NdotV;
                        if (NdL > 0.f) {
                            float32 k    = rough * rough / 2.f;
                            float32 G    = (NdotV / (NdotV*(1.f-k)+k))
                                         * (NdL   / (NdL  *(1.f-k)+k));
                            float32 GVis = G * VdH / (NdH * NdotV + 1e-7f);
                            float32 Fc   = powf(1.f - VdH, 5.f);
                            A += (1.f - Fc) * GVis;
                            B += Fc * GVis;
                        }
                    }
                    A /= SAMPLES; B /= SAMPLES;
                    lut[(y*SZ + x)*2 + 0] = (uint16)(A * 65535.f);
                    lut[(y*SZ + x)*2 + 1] = (uint16)(B * 65535.f);
                }
            }

            NkTextureDesc d;
            d.type        = NkTextureType::NK_TEX2D;
            d.format      = NkGPUFormat::NK_RG16_FLOAT;
            d.width       = SZ;
            d.height      = SZ;
            d.depth       = 1;
            d.arrayLayers = 1;
            d.mipLevels   = 1;
            d.bindFlags   = NkBindFlags::NK_SHADER_RESOURCE;
            d.usage       = NkResourceUsage::NK_DEFAULT;
            d.initialData = lut.Data();
            d.rowPitch    = SZ * 4;       // 2x uint16 par texel = 4 bytes
            d.debugName   = "BRDFLUT";

            NkTextureHandle rhi = mDevice->CreateTexture(d);
            if (!rhi.IsValid()) return NkTexHandle::Null();

            NkSamplerHandle samp = mResources && mResources->IsReady()
                                 ? mResources->GetSamplerLinearClamp()
                                 : NkSamplerHandle{};
            return WrapRHI(rhi, samp, SZ, SZ, 1, "BRDFLUT", false);
        }

    } // namespace renderer
} // namespace nkentseu
