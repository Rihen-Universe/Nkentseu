// =============================================================================
// NkTexture.cpp
// =============================================================================
#include "NkTexture.h"

#include "NKLogger/NkLog.h"
#include <cstring>

namespace nkentseu {
    namespace renderer {

        uint64 NkTexture2D::sIDCounter = 1;
        uint64 NkTextureCube::sIDCounter = 1;
        uint64 NkTexture3D::sIDCounter = 1;
        uint64 NkRenderTarget::sIDCounter = 1;

        namespace {
            static NkGPUFormat ImageFormatToGPU(NkImagePixelFormat fmt, bool srgb = true) {
                switch (fmt) {
                    case NkImagePixelFormat::NK_GRAY8:    return NkGPUFormat::NK_R8_UNORM;
                    case NkImagePixelFormat::NK_GRAY_A16: return NkGPUFormat::NK_RG8_UNORM;
                    case NkImagePixelFormat::NK_RGB24:
                    case NkImagePixelFormat::NK_RGBA32:   return srgb ? NkGPUFormat::NK_RGBA8_SRGB
                                                                      : NkGPUFormat::NK_RGBA8_UNORM;
                    case NkImagePixelFormat::NK_RGBA128F: return NkGPUFormat::NK_RGBA32_FLOAT;
                    case NkImagePixelFormat::NK_RGB96F:   return NkGPUFormat::NK_RGB32_FLOAT;
                    default:                              return NkGPUFormat::NK_RGBA8_SRGB;
                }
            }

            static uint32 GpuBytesPerPixel(NkGPUFormat f) {
                switch (f) {
                    case NkGPUFormat::NK_R8_UNORM:        return 1;
                    case NkGPUFormat::NK_RG8_UNORM:       return 2;
                    case NkGPUFormat::NK_RGBA8_UNORM:
                    case NkGPUFormat::NK_RGBA8_SRGB:      return 4;
                    case NkGPUFormat::NK_RGBA16_FLOAT:    return 8;
                    case NkGPUFormat::NK_RGBA32_FLOAT:    return 16;
                    case NkGPUFormat::NK_RGB32_FLOAT:     return 12;
                    case NkGPUFormat::NK_D32_FLOAT:       return 4;
                    default:                               return 4;
                }
            }

            static NkSamplerDesc MakeSamplerFromParams(const NkTextureParams& p) {
                NkSamplerDesc s{};
                s.magFilter = p.magFilter;
                s.minFilter = p.minFilter;
                s.mipFilter = p.mipFilter;
                s.addressU = p.wrapU;
                s.addressV = p.wrapV;
                s.addressW = p.wrapW;
                s.maxAnisotropy = p.anisotropy;
                s.borderColor = p.borderColor;
                if (p.isShadowMap) {
                    s.compareEnable = true;
                    s.compareOp = NkCompareOp::NK_LESS_EQUAL;
                    s.addressU = NkAddressMode::NK_CLAMP_TO_EDGE;
                    s.addressV = NkAddressMode::NK_CLAMP_TO_EDGE;
                    s.addressW = NkAddressMode::NK_CLAMP_TO_EDGE;
                }
                return s;
            }
        }

        bool NkTexture2D::FinalizeSampler(const NkTextureParams& params) {
            if (!mDevice) return false;
            if (mSamplerHandle.IsValid()) {
                mDevice->DestroySampler(mSamplerHandle);
            }
            NkSamplerDesc s = MakeSamplerFromParams(params);
            mSamplerHandle = mDevice->CreateSampler(s);
            return mSamplerHandle.IsValid();
        }

        bool NkTexture2D::LoadFromFile(NkIDevice* device, const char* path,
                                       const NkTextureParams& params) {
            if (!device || !path) return false;
            NkImage* img = NkImage::Load(path, 0);
            if (!img || !img->IsValid()) {
                if (img) img->Free();
                return false;
            }
            bool ok = LoadFromImage(device, *img, params);
            if (ok) {
                mPath = path;
            }
            img->Free();
            return ok;
        }

        bool NkTexture2D::LoadFromMemory(NkIDevice* device,
                                         const void* pixels, uint32 width, uint32 height,
                                         NkGPUFormat format,
                                         const NkTextureParams& params) {
            if (!device || width == 0 || height == 0) return false;

            Destroy();
            mDevice = device;
            mWidth = width;
            mHeight = height;
            mFormat = format;
            mSRGB = params.srgb;
            mMipLevels = params.mipLevels > 0 ? params.mipLevels : (params.generateMips ? 0u : 1u);
            mID = { sIDCounter++ };

            NkTextureDesc td{};
            td.type = NkTextureType::NK_TEX2D;
            td.format = format;
            td.width = width;
            td.height = height;
            td.depth = 1;
            td.arrayLayers = 1;
            td.mipLevels = mMipLevels;
            td.samples = params.samples;
            td.bindFlags = NkBindFlags::NK_SHADER_RESOURCE;
            if (params.isRenderTarget) td.bindFlags = td.bindFlags | NkBindFlags::NK_RENDER_TARGET;
            if (params.isStorageImage) td.bindFlags = td.bindFlags | NkBindFlags::NK_UNORDERED_ACCESS;
            if (params.isShadowMap || format == NkGPUFormat::NK_D32_FLOAT) {
                td.bindFlags = NkBindFlags::NK_DEPTH_STENCIL | NkBindFlags::NK_SHADER_RESOURCE;
            }
            td.initialData = pixels;
            td.rowPitch = width * GpuBytesPerPixel(format);
            td.debugName = params.debugName;

            mTexHandle = device->CreateTexture(td);
            if (!mTexHandle.IsValid()) {
                Destroy();
                return false;
            }

            if (pixels && !td.initialData) {
                device->WriteTexture(mTexHandle, pixels, td.rowPitch);
            }

            if (!FinalizeSampler(params)) {
                Destroy();
                return false;
            }

            if (params.generateMips && mMipLevels != 1) {
                device->GenerateMipmaps(mTexHandle, params.minFilter);
            }
            return true;
        }

        bool NkTexture2D::LoadFromImage(NkIDevice* device, const NkImage& image,
                                        const NkTextureParams& params) {
            if (!image.IsValid()) return false;
            NkGPUFormat fmt = params.format;
            if (fmt == NkGPUFormat::NK_RGBA8_SRGB || fmt == NkGPUFormat::NK_RGBA8_UNORM) {
                fmt = ImageFormatToGPU(image.Format(), params.srgb);
            }
            return LoadFromMemory(device,
                                  image.Pixels(),
                                  (uint32)image.Width(),
                                  (uint32)image.Height(),
                                  fmt,
                                  params);
        }

        bool NkTexture2D::CreateSolid(NkIDevice* device, const NkColor4f& color, uint32 size) {
            if (size == 0) size = 1;
            NkVector<uint8> pixels;
            pixels.Resize((usize)size * (usize)size * 4);
            const uint8 r = (uint8)NkClamp(color.r * 255.f, 0.f, 255.f);
            const uint8 g = (uint8)NkClamp(color.g * 255.f, 0.f, 255.f);
            const uint8 b = (uint8)NkClamp(color.b * 255.f, 0.f, 255.f);
            const uint8 a = (uint8)NkClamp(color.a * 255.f, 0.f, 255.f);
            for (usize i = 0; i < pixels.Size(); i += 4) {
                pixels[i + 0] = r;
                pixels[i + 1] = g;
                pixels[i + 2] = b;
                pixels[i + 3] = a;
            }
            NkTextureParams p = NkTextureParams::Linear();
            p.generateMips = false;
            p.debugName = "SolidTexture";
            return LoadFromMemory(device, pixels.Data(), size, size, NkGPUFormat::NK_RGBA8_UNORM, p);
        }

        bool NkTexture2D::CreateCheckerboard(NkIDevice* device, const NkColor4f& a, const NkColor4f& b,
                                             uint32 size, uint32 tileSize) {
            if (size == 0) size = 2;
            if (tileSize == 0) tileSize = 1;
            NkVector<uint8> pixels;
            pixels.Resize((usize)size * (usize)size * 4);

            const uint8 ar = (uint8)NkClamp(a.r * 255.f, 0.f, 255.f);
            const uint8 ag = (uint8)NkClamp(a.g * 255.f, 0.f, 255.f);
            const uint8 ab = (uint8)NkClamp(a.b * 255.f, 0.f, 255.f);
            const uint8 aa = (uint8)NkClamp(a.a * 255.f, 0.f, 255.f);
            const uint8 br = (uint8)NkClamp(b.r * 255.f, 0.f, 255.f);
            const uint8 bg = (uint8)NkClamp(b.g * 255.f, 0.f, 255.f);
            const uint8 bb = (uint8)NkClamp(b.b * 255.f, 0.f, 255.f);
            const uint8 ba = (uint8)NkClamp(b.a * 255.f, 0.f, 255.f);

            for (uint32 y = 0; y < size; ++y) {
                for (uint32 x = 0; x < size; ++x) {
                    const bool sel = (((x / tileSize) + (y / tileSize)) & 1u) == 0u;
                    const usize idx = ((usize)y * (usize)size + (usize)x) * 4;
                    pixels[idx + 0] = sel ? ar : br;
                    pixels[idx + 1] = sel ? ag : bg;
                    pixels[idx + 2] = sel ? ab : bb;
                    pixels[idx + 3] = sel ? aa : ba;
                }
            }

            NkTextureParams p = NkTextureParams::UISprite();
            p.generateMips = false;
            p.debugName = "CheckerTexture";
            return LoadFromMemory(device, pixels.Data(), size, size, NkGPUFormat::NK_RGBA8_UNORM, p);
        }

        bool NkTexture2D::CreateWhite(NkIDevice* device) {
            return CreateSolid(device, NkColor4f::White(), 1);
        }

        bool NkTexture2D::CreateBlack(NkIDevice* device) {
            return CreateSolid(device, NkColor4f::Black(), 1);
        }

        bool NkTexture2D::CreateNormal(NkIDevice* device) {
            return CreateSolid(device, NkColor4f(0.5f, 0.5f, 1.0f, 1.0f), 1);
        }

        bool NkTextureCube::LoadFromFiles(NkIDevice* device, const char* faces[6]) {
            if (!device || !faces) return false;

            Destroy();
            mDevice = device;
            mID = { sIDCounter++ };

            NkImage* imgs[6] = {};
            for (uint32 i = 0; i < 6; ++i) {
                if (!faces[i]) return false;
                imgs[i] = NkImage::Load(faces[i], 4);
                if (!imgs[i] || !imgs[i]->IsValid()) {
                    for (uint32 j = 0; j < 6; ++j) if (imgs[j]) imgs[j]->Free();
                    return false;
                }
            }

            mFaceSize = (uint32)imgs[0]->Width();
            NkTextureDesc td = NkTextureDesc::Cubemap(mFaceSize, NkGPUFormat::NK_RGBA8_SRGB, 1);
            td.initialData = nullptr;
            td.debugName = "TextureCube";
            mTexHandle = device->CreateTexture(td);
            if (!mTexHandle.IsValid()) {
                for (uint32 j = 0; j < 6; ++j) imgs[j]->Free();
                Destroy();
                return false;
            }

            for (uint32 i = 0; i < 6; ++i) {
                device->WriteTextureRegion(
                    mTexHandle,
                    imgs[i]->Pixels(),
                    0, 0, 0,
                    (uint32)imgs[i]->Width(), (uint32)imgs[i]->Height(), 1,
                    0, i,
                    (uint32)imgs[i]->Stride());
                imgs[i]->Free();
            }

            NkSamplerDesc s = NkSamplerDesc::Clamp();
            s.magFilter = NkFilter::NK_LINEAR;
            s.minFilter = NkFilter::NK_LINEAR;
            s.mipFilter = NkMipFilter::NK_LINEAR;
            mSamplerHandle = device->CreateSampler(s);
            if (!mSamplerHandle.IsValid()) {
                Destroy();
                return false;
            }
            return true;
        }

        bool NkTextureCube::LoadFromEquirectangular(NkIDevice* device, const char* hdriPath,
                                                    uint32 faceSize) {
            if (!device || !hdriPath) return false;
            NkImage* img = NkImage::Load(hdriPath, 4);
            if (!img || !img->IsValid()) {
                if (img) img->Free();
                return false;
            }

            const uint8* px = img->Pixels();
            NkColor4f avg(1, 1, 1, 1);
            if (px && img->Width() > 0 && img->Height() > 0) {
                const int32 cx = img->Width() / 2;
                const int32 cy = img->Height() / 2;
                const uint8* p = img->RowPtr(cy) + cx * 4;
                avg = NkColor4f(p[0] / 255.f, p[1] / 255.f, p[2] / 255.f, p[3] / 255.f);
            }
            img->Free();

            Destroy();
            mDevice = device;
            mFaceSize = faceSize > 0 ? faceSize : 1;
            mID = { sIDCounter++ };

            NkVector<uint8> face;
            face.Resize((usize)mFaceSize * (usize)mFaceSize * 4);
            const uint8 r = (uint8)NkClamp(avg.r * 255.f, 0.f, 255.f);
            const uint8 g = (uint8)NkClamp(avg.g * 255.f, 0.f, 255.f);
            const uint8 b = (uint8)NkClamp(avg.b * 255.f, 0.f, 255.f);
            const uint8 a = (uint8)NkClamp(avg.a * 255.f, 0.f, 255.f);
            for (usize i = 0; i < face.Size(); i += 4) {
                face[i + 0] = r;
                face[i + 1] = g;
                face[i + 2] = b;
                face[i + 3] = a;
            }

            NkTextureDesc td = NkTextureDesc::Cubemap(mFaceSize, NkGPUFormat::NK_RGBA8_UNORM, 1);
            td.debugName = "EquirectCube";
            mTexHandle = device->CreateTexture(td);
            if (!mTexHandle.IsValid()) {
                Destroy();
                return false;
            }
            const uint32 rowPitch = mFaceSize * 4;
            for (uint32 i = 0; i < 6; ++i) {
                device->WriteTextureRegion(mTexHandle, face.Data(), 0, 0, 0,
                                           mFaceSize, mFaceSize, 1,
                                           0, i, rowPitch);
            }
            mSamplerHandle = device->CreateSampler(NkSamplerDesc::Clamp());
            if (!mSamplerHandle.IsValid()) {
                Destroy();
                return false;
            }
            return true;
        }

        bool NkTextureCube::LoadFromLatlongFile(NkIDevice* device, const char* path,
                                                uint32 faceSize) {
            return LoadFromEquirectangular(device, path, faceSize);
        }

        bool NkTexture3D::Create(NkIDevice* device, uint32 w, uint32 h, uint32 d,
                                 NkGPUFormat format, const void* data) {
            if (!device || w == 0 || h == 0 || d == 0) return false;
            Destroy();
            mDevice = device;
            mID = { sIDCounter++ };

            NkTextureDesc td = NkTextureDesc::Tex3D(w, h, d, format);
            td.initialData = data;
            td.rowPitch = w * GpuBytesPerPixel(format);
            td.debugName = "Texture3D";
            mTexHandle = device->CreateTexture(td);
            if (!mTexHandle.IsValid()) {
                Destroy();
                return false;
            }
            if (data) {
                device->WriteTexture(mTexHandle, data, td.rowPitch);
            }
            mSamplerHandle = device->CreateSampler(NkSamplerDesc::Linear());
            if (!mSamplerHandle.IsValid()) {
                Destroy();
                return false;
            }
            return true;
        }

        bool NkTexture3D::LoadColorLUT(NkIDevice* device, const char* cubePath) {
            (void)cubePath;
            // Fallback identity-ish LUT 16^3
            const uint32 size = 16;
            NkVector<float> voxels;
            voxels.Resize((usize)size * (usize)size * (usize)size * 4);
            usize idx = 0;
            for (uint32 z = 0; z < size; ++z) {
                for (uint32 y = 0; y < size; ++y) {
                    for (uint32 x = 0; x < size; ++x) {
                        voxels[idx++] = (float)x / (float)(size - 1);
                        voxels[idx++] = (float)y / (float)(size - 1);
                        voxels[idx++] = (float)z / (float)(size - 1);
                        voxels[idx++] = 1.f;
                    }
                }
            }
            return Create(device, size, size, size, NkGPUFormat::NK_RGBA32_FLOAT, voxels.Data());
        }

        bool NkRenderTarget::Create(NkIDevice* device, const NkRenderTargetDesc& desc) {
            mColorFormats.Clear();
            mColorFormats.PushBack(desc.colorFormat);
            mDepthFormat = desc.depthFormat;
            return Finalize(device, desc.width, desc.height, desc.samples);
        }

        bool NkRenderTarget::Resize(uint32 newWidth, uint32 newHeight) {
            if (!mDevice || newWidth == 0 || newHeight == 0) return false;
            return Finalize(mDevice, newWidth, newHeight, mSamples);
        }

        NkTextureHandle NkRenderTarget::GetColorTexture(uint32 idx) const {
            if (idx >= mColorAttachments.Size()) return {};
            return mColorAttachments[idx].tex;
        }

        NkSamplerHandle NkRenderTarget::GetColorSampler(uint32 idx) const {
            if (idx >= mColorAttachments.Size()) return {};
            return mColorAttachments[idx].sampler;
        }

        NkRenderTarget& NkRenderTarget::AddColorAttachment(NkGPUFormat fmt) {
            mColorFormats.PushBack(fmt);
            return *this;
        }

        NkRenderTarget& NkRenderTarget::SetDepthAttachment(NkGPUFormat fmt) {
            mDepthFormat = fmt;
            return *this;
        }

        bool NkRenderTarget::Finalize(NkIDevice* device, uint32 w, uint32 h, uint32 samples) {
            if (!device || w == 0 || h == 0) return false;

            Destroy();
            mDevice = device;
            mWidth = w;
            mHeight = h;
            mSamples = samples;
            mID = { sIDCounter++ };

            if (mColorFormats.Empty()) {
                mColorFormats.PushBack(NkGPUFormat::NK_RGBA16_FLOAT);
            }

            for (const auto& fmt : mColorFormats) {
                NkTextureDesc td = NkTextureDesc::RenderTarget(w, h, fmt, (NkSampleCount)samples);
                td.debugName = "RTColor";
                ColorAttachment ca{};
                ca.format = fmt;
                ca.tex = device->CreateTexture(td);
                if (!ca.tex.IsValid()) {
                    Destroy();
                    return false;
                }
                ca.sampler = device->CreateSampler(NkSamplerDesc::Linear());
                if (!ca.sampler.IsValid()) {
                    Destroy();
                    return false;
                }
                mColorAttachments.PushBack(ca);
            }

            if (mDepthFormat != NkGPUFormat::NK_UNDEFINED) {
                NkTextureDesc dd = NkTextureDesc::DepthStencil(w, h, mDepthFormat, (NkSampleCount)samples);
                dd.debugName = "RTDepth";
                mDepthTex = device->CreateTexture(dd);
                if (!mDepthTex.IsValid()) {
                    Destroy();
                    return false;
                }
                mDepthSampler = device->CreateSampler(NkSamplerDesc::Shadow());
            }

            NkRenderPassDesc rp{};
            for (const auto& fmt : mColorFormats) {
                rp.AddColor(NkAttachmentDesc::Color(fmt));
            }
            if (mDepthTex.IsValid()) {
                rp.SetDepth(NkAttachmentDesc::Depth(mDepthFormat));
            }
            rp.debugName = "RenderTargetRP";
            mRenderPass = device->CreateRenderPass(rp);
            if (!mRenderPass.IsValid()) {
                Destroy();
                return false;
            }

            NkFramebufferDesc fb{};
            fb.renderPass = mRenderPass;
            fb.width = w;
            fb.height = h;
            for (const auto& ca : mColorAttachments) {
                fb.colorAttachments.PushBack(ca.tex);
            }
            fb.depthAttachment = mDepthTex;
            fb.debugName = "RenderTargetFB";
            mFramebuffer = device->CreateFramebuffer(fb);
            if (!mFramebuffer.IsValid()) {
                Destroy();
                return false;
            }

            mIsValid = true;
            return true;
        }

        void NkRenderTarget::Destroy() {
            if (!mDevice) {
                mColorAttachments.Clear();
                mIsValid = false;
                return;
            }

            if (mFramebuffer.IsValid()) {
                mDevice->DestroyFramebuffer(mFramebuffer);
            }
            if (mRenderPass.IsValid()) {
                mDevice->DestroyRenderPass(mRenderPass);
            }

            for (auto& ca : mColorAttachments) {
                if (ca.sampler.IsValid()) mDevice->DestroySampler(ca.sampler);
                if (ca.tex.IsValid()) mDevice->DestroyTexture(ca.tex);
            }
            mColorAttachments.Clear();

            if (mDepthSampler.IsValid()) mDevice->DestroySampler(mDepthSampler);
            if (mDepthTex.IsValid()) mDevice->DestroyTexture(mDepthTex);

            mDepthSampler = {};
            mDepthTex = {};
            mRenderPass = {};
            mFramebuffer = {};
            mDevice = nullptr;
            mWidth = mHeight = 0;
            mIsValid = false;
            mID = {};
        }

        bool NkTextureAtlas::BuildFromDirectory(NkIDevice* device, const char* dir,
                                                uint32 maxSize) {
            (void)device;
            (void)dir;
            (void)maxSize;
            // Deterministic fallback: atlas build from directory is not available in this module.
            return false;
        }

        void NkTextureAtlas::AddRegion(const NkString& name, uint32 x, uint32 y, uint32 w, uint32 h) {
            NkAtlasRegion r{};
            r.name = name;
            r.pixX = x;
            r.pixY = y;
            r.pixW = w;
            r.pixH = h;

            const float tw = (mTexture && mTexture->GetWidth() > 0) ? (float)mTexture->GetWidth() : 1.f;
            const float th = (mTexture && mTexture->GetHeight() > 0) ? (float)mTexture->GetHeight() : 1.f;
            r.uvMin = { x / tw, y / th };
            r.uvMax = { (x + w) / tw, (y + h) / th };

            mIndexByName[name] = (uint32)mRegions.Size();
            mRegions.PushBack(r);
        }

        const NkAtlasRegion* NkTextureAtlas::FindRegion(const NkString& name) const {
            const uint32* idx = mIndexByName.Find(name);
            if (!idx) return nullptr;
            if (*idx >= mRegions.Size()) return nullptr;
            return &mRegions[*idx];
        }

        NkTextureManager& NkTextureManager::Get() {
            static NkTextureManager sInst;
            return sInst;
        }

        void NkTextureManager::Init(NkIDevice* device) {
            if (mDevice == device && mDevice) return;
            Shutdown();
            mDevice = device;
            if (mDevice) {
                CreateBuiltins();
            }
        }

        void NkTextureManager::Shutdown() {
            for (auto it = mTex2DCache.Begin(); it != mTex2DCache.End(); ++it) {
                delete it->Second.tex;
            }
            for (auto it = mTexCubeCache.Begin(); it != mTexCubeCache.End(); ++it) {
                delete it->Second.tex;
            }
            mTex2DCache.Clear();
            mTexCubeCache.Clear();

            delete mWhite; mWhite = nullptr;
            delete mBlack; mBlack = nullptr;
            delete mFlatNormal; mFlatNormal = nullptr;
            delete mChecker; mChecker = nullptr;

            mDevice = nullptr;
        }

        NkTexture2D* NkTextureManager::LoadTexture2D(const char* path,
                                                     const NkTextureParams& params) {
            if (!mDevice || !path) return nullptr;
            NkString key(path);
            Tex2DEntry* found = mTex2DCache.Find(key);
            if (found && found->tex) {
                found->refCount++;
                return found->tex;
            }

            NkTexture2D* tex = new NkTexture2D();
            if (!tex->LoadFromFile(mDevice, path, params)) {
                delete tex;
                return nullptr;
            }

            Tex2DEntry e{};
            e.tex = tex;
            e.refCount = 1;
            e.path = key;
            mTex2DCache.Insert(key, e);
            return tex;
        }

        NkTextureCube* NkTextureManager::LoadTextureCube(const char* hdriPath, uint32 faceSize) {
            if (!mDevice || !hdriPath) return nullptr;
            NkString key(hdriPath);
            TexCubeEntry* found = mTexCubeCache.Find(key);
            if (found && found->tex) {
                found->refCount++;
                return found->tex;
            }

            NkTextureCube* tex = new NkTextureCube();
            if (!tex->LoadFromEquirectangular(mDevice, hdriPath, faceSize)) {
                delete tex;
                return nullptr;
            }

            TexCubeEntry e{};
            e.tex = tex;
            e.refCount = 1;
            e.path = key;
            mTexCubeCache.Insert(key, e);
            return tex;
        }

        NkTexture2D* NkTextureManager::GetWhite() { return mWhite; }
        NkTexture2D* NkTextureManager::GetBlack() { return mBlack; }
        NkTexture2D* NkTextureManager::GetFlatNormal() { return mFlatNormal; }
        NkTexture2D* NkTextureManager::GetCheckerboard() { return mChecker; }

        void NkTextureManager::Release(NkTexture2D* tex) {
            if (!tex) return;
            NkString toErase;
            bool found = false;
            for (auto it = mTex2DCache.Begin(); it != mTex2DCache.End(); ++it) {
                if (it->Second.tex == tex) {
                    if (it->Second.refCount > 0) it->Second.refCount--;
                    if (it->Second.refCount == 0) {
                        delete it->Second.tex;
                        toErase = it->First;
                        found = true;
                    }
                    break;
                }
            }
            if (found) mTex2DCache.Erase(toErase);
        }

        void NkTextureManager::Release(NkTextureCube* tex) {
            if (!tex) return;
            NkString toErase;
            bool found = false;
            for (auto it = mTexCubeCache.Begin(); it != mTexCubeCache.End(); ++it) {
                if (it->Second.tex == tex) {
                    if (it->Second.refCount > 0) it->Second.refCount--;
                    if (it->Second.refCount == 0) {
                        delete it->Second.tex;
                        toErase = it->First;
                        found = true;
                    }
                    break;
                }
            }
            if (found) mTexCubeCache.Erase(toErase);
        }

        void NkTextureManager::CollectGarbage() {
            NkVector<NkString> dead2D;
            for (auto it = mTex2DCache.Begin(); it != mTex2DCache.End(); ++it) {
                if (it->Second.refCount == 0) {
                    delete it->Second.tex;
                    dead2D.PushBack(it->First);
                }
            }
            for (const auto& k : dead2D) mTex2DCache.Erase(k);

            NkVector<NkString> deadCube;
            for (auto it = mTexCubeCache.Begin(); it != mTexCubeCache.End(); ++it) {
                if (it->Second.refCount == 0) {
                    delete it->Second.tex;
                    deadCube.PushBack(it->First);
                }
            }
            for (const auto& k : deadCube) mTexCubeCache.Erase(k);
        }

        void NkTextureManager::CreateBuiltins() {
            delete mWhite;      mWhite = nullptr;
            delete mBlack;      mBlack = nullptr;
            delete mFlatNormal; mFlatNormal = nullptr;
            delete mChecker;    mChecker = nullptr;

            mWhite = new NkTexture2D();
            mBlack = new NkTexture2D();
            mFlatNormal = new NkTexture2D();
            mChecker = new NkTexture2D();

            if (mWhite)      mWhite->CreateWhite(mDevice);
            if (mBlack)      mBlack->CreateBlack(mDevice);
            if (mFlatNormal) mFlatNormal->CreateNormal(mDevice);
            if (mChecker)    mChecker->CreateCheckerboard(mDevice,
                                                          NkColor4f(0.85f, 0.85f, 0.85f, 1.f),
                                                          NkColor4f(0.25f, 0.25f, 0.25f, 1.f),
                                                          64, 8);
        }

    } // namespace renderer
} // namespace nkentseu
