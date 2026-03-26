#pragma once
// =============================================================================
// NkTexture.h — Texture GPU haut niveau (wrappée autour du RHI).
// =============================================================================
#include "NKRenderer/Core/NkRenderTypes.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKImage/Core/NkImage.h"

namespace nkentseu {

    // =========================================================================
    // NkTextureDesc — description de création (haut niveau)
    // =========================================================================
    struct NKRENDERER_API NkTextureCreateDesc {
        const char*     name        = nullptr;
        NkGPUFormat     format      = NkGPUFormat::NK_RGBA8_SRGB;
        uint32          width       = 1;
        uint32          height      = 1;
        uint32          mipLevels   = 0;   // 0 = génère toute la mip chain
        NkSampleCount   samples     = NkSampleCount::NK_S1;
        bool            isRenderTarget = false;
        bool            isDepthBuffer  = false;
        // Sampler par défaut
        NkSamplerDesc   sampler     = NkSamplerDesc::Linear();
    };

    // =========================================================================
    // NkTexture — texture GPU avec sampler intégré
    // =========================================================================
    class NKRENDERER_API NkTexture {
        public:
            NkTexture()  = default;
            ~NkTexture() { Destroy(); }

            NkTexture(const NkTexture&)            = delete;
            NkTexture& operator=(const NkTexture&) = delete;

            // ── Création ──────────────────────────────────────────────────────────

            /// Crée une texture vide (render target, depth, etc.)
            bool Create(NkIDevice* device, const NkTextureCreateDesc& desc) noexcept;

            /// Charge depuis un NkImage déjà chargé.
            bool LoadFromImage(NkIDevice* device, const NkImage* image,
                            const NkSamplerDesc& sampler = NkSamplerDesc::Anisotropic(),
                            const char* name = nullptr,
                            bool generateMips = true) noexcept;

            /// Charge depuis un fichier (PNG, JPG, BMP, TGA, HDR…).
            bool LoadFromFile(NkIDevice* device, const char* path,
                            const NkSamplerDesc& sampler = NkSamplerDesc::Anisotropic(),
                            bool generateMips = true) noexcept;

            /// Crée une texture 1×1 de couleur unie (placeholder, white/black).
            static NkTextureRef CreateSolid(NkIDevice* device, math::NkColor color,
                                            const char* name = nullptr) noexcept;

            void Destroy() noexcept;

            // ── Accès ─────────────────────────────────────────────────────────────

            bool         IsValid()  const noexcept { return mTexture.IsValid(); }
            NkTextureHandle GetHandle() const noexcept { return mTexture; }
            NkSamplerHandle GetSampler()const noexcept { return mSampler; }
            uint32       GetWidth()  const noexcept { return mWidth; }
            uint32       GetHeight() const noexcept { return mHeight; }
            uint32       GetMipLevels() const noexcept { return mMipLevels; }
            NkGPUFormat  GetFormat() const noexcept { return mFormat; }
            const char*  GetName()   const noexcept { return mName; }

            // Bind helper : écriture dans un descriptor set
            void Bind(NkIDevice* device, NkDescSetHandle set, uint32 binding) const noexcept;

        private:
            NkIDevice*      mDevice    = nullptr;
            NkTextureHandle mTexture;
            NkSamplerHandle mSampler;
            uint32          mWidth     = 0;
            uint32          mHeight    = 0;
            uint32          mMipLevels = 1;
            NkGPUFormat     mFormat    = NkGPUFormat::NK_UNDEFINED;
            const char*     mName      = nullptr;
    };

    // ── Helpers de création rapide ─────────────────────────────────────────────

    inline NkTextureRef NkTextureLoad(NkIDevice* device, const char* path,
                                      bool generateMips = true) noexcept {
        auto tex = memory::NkMakeShared<NkTexture>();
        if (!tex || !tex->LoadFromFile(device, path, NkSamplerDesc::Anisotropic(), generateMips))
            return {};
        return tex;
    }

    inline NkTextureRef NkTextureFromImage(NkIDevice* device, const NkImage* img,
                                           bool generateMips = true) noexcept {
        auto tex = memory::NkMakeShared<NkTexture>();
        if (!tex || !tex->LoadFromImage(device, img, NkSamplerDesc::Anisotropic(), nullptr, generateMips))
            return {};
        return tex;
    }

} // namespace nkentseu
