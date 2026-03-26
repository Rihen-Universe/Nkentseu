// =============================================================================
// NkTexture.cpp — Implémentation du wrapper de texture GPU.
// =============================================================================
#include "pch/pch.h"
#include "NKRenderer/Core/NkTexture.h"

namespace nkentseu {

    // ── Helpers internes ────────────────────────────────────────────────────────

    static NkGPUFormat ImageFormatToGPU(NkImagePixelFormat fmt, bool srgb = true) {
        switch (fmt) {
            case NkImagePixelFormat::NK_GRAY8:    return NkGPUFormat::NK_R8_UNORM;
            case NkImagePixelFormat::NK_GRAY_A16: return NkGPUFormat::NK_RG8_UNORM;
            case NkImagePixelFormat::NK_RGB24:    // padded to RGBA au moment upload
            case NkImagePixelFormat::NK_RGBA32:   return srgb ? NkGPUFormat::NK_RGBA8_SRGB
                                                              : NkGPUFormat::NK_RGBA8_UNORM;
            case NkImagePixelFormat::NK_RGBA128F: return NkGPUFormat::NK_RGBA32_FLOAT;
            case NkImagePixelFormat::NK_RGB96F:   return NkGPUFormat::NK_RGB32_FLOAT;
            default:                              return NkGPUFormat::NK_RGBA8_SRGB;
        }
    }

    // ── Create ──────────────────────────────────────────────────────────────────

    bool NkTexture::Create(NkIDevice* device, const NkTextureCreateDesc& desc) noexcept {
        if (!device) return false;
        Destroy();
        mDevice = device;
        mName   = desc.name;
        mWidth  = desc.width;
        mHeight = desc.height;
        mFormat = desc.format;

        NkTextureDesc td;
        td.type       = NkTextureType::NK_TEX2D;
        td.format     = desc.format;
        td.width      = desc.width;
        td.height     = desc.height;
        td.mipLevels  = desc.mipLevels == 0 ? 1 : desc.mipLevels;
        td.samples    = desc.samples;
        td.debugName  = desc.name;

        if (desc.isRenderTarget) {
            td.bindFlags = NkBindFlags::NK_RENDER_TARGET | NkBindFlags::NK_SHADER_RESOURCE;
        } else if (desc.isDepthBuffer) {
            td.bindFlags = NkBindFlags::NK_DEPTH_STENCIL | NkBindFlags::NK_SHADER_RESOURCE;
        } else {
            td.bindFlags = NkBindFlags::NK_SHADER_RESOURCE;
        }
        mMipLevels = td.mipLevels;
        mTexture   = device->CreateTexture(td);
        if (!mTexture.IsValid()) return false;

        mSampler = device->CreateSampler(desc.sampler);
        return mSampler.IsValid();
    }

    // ── LoadFromImage ───────────────────────────────────────────────────────────

    bool NkTexture::LoadFromImage(NkIDevice* device, const NkImage* image,
                                   const NkSamplerDesc& samplerDesc,
                                   const char* name, bool generateMips) noexcept {
        if (!device || !image || !image->IsValid()) return false;
        Destroy();
        mDevice = device;
        mName   = name;
        mWidth  = (uint32)image->Width();
        mHeight = (uint32)image->Height();
        mFormat = ImageFormatToGPU(image->Format());

        // RGB24 → RGBA32 (les GPU ne supportent généralement pas RGB24 natif)
        NkImage* converted = nullptr;
        const NkImage* src = image;
        if (image->Format() == NkImagePixelFormat::NK_RGB24) {
            converted = image->Convert(NkImagePixelFormat::NK_RGBA32);
            src = converted ? converted : image;
            mFormat = NkGPUFormat::NK_RGBA8_SRGB;
        }

        uint32 mips = generateMips ? 0u : 1u; // 0 = calcul auto
        NkTextureDesc td = NkTextureDesc::Tex2D(mWidth, mHeight, mFormat, mips);
        td.initialData  = src->Pixels();
        td.rowPitch     = (uint32)src->Stride();
        td.debugName    = mName;
        mMipLevels      = (mips == 0) ? 1u : mips; // sera mis à jour après génération

        mTexture = device->CreateTexture(td);
        if (converted) converted->Free();

        if (!mTexture.IsValid()) return false;

        if (generateMips && mips == 0) {
            device->GenerateMipmaps(mTexture, NkFilter::NK_LINEAR);
        }

        mSampler = device->CreateSampler(samplerDesc);
        return mTexture.IsValid() && mSampler.IsValid();
    }

    // ── LoadFromFile ────────────────────────────────────────────────────────────

    bool NkTexture::LoadFromFile(NkIDevice* device, const char* path,
                                  const NkSamplerDesc& samplerDesc,
                                  bool generateMips) noexcept {
        if (!device || !path) return false;
        NkImage* img = NkImage::Load(path, 4); // force RGBA
        if (!img) return false;
        bool ok = LoadFromImage(device, img, samplerDesc, path, generateMips);
        img->Free();
        return ok;
    }

    // ── CreateSolid ─────────────────────────────────────────────────────────────

    NkTextureRef NkTexture::CreateSolid(NkIDevice* device, math::NkColor color,
                                        const char* name) noexcept {
        if (!device) return {};
        uint8 pixels[4] = {color.r, color.g, color.b, color.a};
        NkImage* img = NkImage::Wrap(pixels, 1, 1, NkImagePixelFormat::NK_RGBA32, 4);
        if (!img) return {};

        auto tex = memory::NkMakeShared<NkTexture>();
        if (!tex || !tex->LoadFromImage(device, img, NkSamplerDesc::Nearest(), name, false)) {
            img->Free();
            return {};
        }
        img->Free();
        return tex;
    }

    // ── Destroy ─────────────────────────────────────────────────────────────────

    void NkTexture::Destroy() noexcept {
        if (!mDevice) return;
        if (mSampler.IsValid()) { mDevice->DestroySampler(mSampler); }
        if (mTexture.IsValid()) { mDevice->DestroyTexture(mTexture); }
        mDevice = nullptr;
    }

    // ── Bind ────────────────────────────────────────────────────────────────────

    void NkTexture::Bind(NkIDevice* device, NkDescSetHandle set, uint32 binding) const noexcept {
        if (!device || !mTexture.IsValid() || !mSampler.IsValid()) return;
        device->BindTextureSampler(set, binding, mTexture, mSampler);
    }

} // namespace nkentseu
