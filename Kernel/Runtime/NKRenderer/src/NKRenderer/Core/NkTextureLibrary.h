#pragma once
// =============================================================================
// NkTextureLibrary.h  — NKRenderer v5.0  (Core/)
//
// Bibliotheque de textures du renderer :
//   - Chargement via NKImage (PNG/JPEG/BMP/TGA/HDR/QOI/GIF/WebP/SVG)
//   - Cache par chemin avec ref-count
//   - Generation de mipmaps via NkImage::Resize (LANCZOS3)
//   - Cubemap : 6 fichiers separes ou SVG/cross layout
//   - HDR : NK_RGB96F decode -> upload vers RGBA16_FLOAT GPU
//   - Render targets (color + depth)
//   - Default textures (delegues vers NkResources)
//   - Backend custom optionnel via NkImageLoaderFn
// =============================================================================
#include "NkRendererTypes.h"
#include "NkRendererResult.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKContainers/Associative/NkHashMap.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
    namespace renderer {

        class NkResources;   // forward — fournit les samplers par defaut

        // =====================================================================
        // Backend custom (optionnel — remplace NKImage)
        // =====================================================================
        struct NkImageData {
            uint8*   pixels    = nullptr;       // LDR uint8 RGBA
            uint32   width     = 0;
            uint32   height    = 0;
            uint32   channels  = 4;             // sortie attendue : 4 (RGBA)
            bool     isHDR     = false;
            float32* hdrPixels = nullptr;       // si isHDR : RGBA float32
        };
        using NkImageLoaderFn = bool(*)(const char* path, NkImageData* out, void* user);
        using NkImageFreeFn   = void(*)(NkImageData* data, void* user);

        // =====================================================================
        // Options de chargement
        // =====================================================================
        struct NkLoadOptions {
            bool srgb            = true;     // input file en sRGB ? (PBR : albedo=true, normal=false)
            bool genMipmaps      = true;     // generer chaine de mips via Lanczos3
            bool useAnisotropic  = true;     // utilise le sampler Aniso16 (sinon Linear)
            bool useClampEdge    = false;    // sinon REPEAT
            const char* debugName= nullptr;
        };

        // =====================================================================
        // Description de creation manuelle
        // =====================================================================
        struct NkTextureCreateDesc {
            const void* pixels    = nullptr;
            uint32      width     = 0;
            uint32      height    = 0;
            uint32      depth     = 1;
            uint32      mipLevels = 1;       // 1 = pas de mip
            bool        srgb      = false;
            bool        genMips   = false;
            bool        isHDR     = false;
            bool        isCubemap = false;
            NkGPUFormat format    = NkGPUFormat::NK_RGBA8_UNORM;
            const char* debugName = nullptr;
        };

        // =====================================================================
        // NkTextureLibrary
        // =====================================================================
        class NkTextureLibrary {
            public:
                NkTextureLibrary() = default;
                ~NkTextureLibrary();

                NkTextureLibrary(const NkTextureLibrary&) = delete;
                NkTextureLibrary& operator=(const NkTextureLibrary&) = delete;

                // Init prend NkResources en parametre pour samplers par defaut + textures fallback.
                // resources peut etre nullptr (mode degrade : la lib creera ses propres samplers).
                NkRResult Init(NkIDevice* device, NkResources* resources = nullptr);
                void      Shutdown();

                // ── Backend custom ────────────────────────────────────────────
                void SetCustomLoader(NkImageLoaderFn load, NkImageFreeFn free, void* user);

                // ── Chargement ────────────────────────────────────────────────
                NkTexHandle Load    (const NkString& path, const NkLoadOptions& opts = {});
                NkTexHandle LoadHDR (const NkString& path, const NkLoadOptions& opts = {});

                // 6 faces : ordre Vulkan/DX12/GL = +X, -X, +Y, -Y, +Z, -Z
                NkTexHandle LoadCubemap(const NkString paths[6], const NkLoadOptions& opts = {});

                // ── Creation manuelle ─────────────────────────────────────────
                NkTexHandle Create(const NkTextureCreateDesc& desc);

                // ── Render targets ────────────────────────────────────────────
                NkTexHandle CreateRenderTarget(uint32 w, uint32 h, NkGPUFormat format,
                                                bool depth = false, bool readable = true,
                                                const NkString& name = "");

                // ── Update runtime ────────────────────────────────────────────
                bool Update(NkTexHandle h, const void* data, uint32 rowPitch = 0,
                            uint32 mipLevel = 0, uint32 layer = 0);

                // ── Lifecycle ─────────────────────────────────────────────────
                void Release(NkTexHandle& h);
                void ReleaseAll();

                // ── Acces RHI ────────────────────────────────────────────────
                NkTextureHandle GetRHIHandle (NkTexHandle h) const;
                NkSamplerHandle GetRHISampler(NkTexHandle h) const;
                bool            HasMipmaps   (NkTexHandle h) const;

                // ── Built-ins (delegue NkResources si dispo, fallback sinon) ─
                NkTexHandle GetWhite1x1()  const { return mWhite; }
                NkTexHandle GetBlack1x1()  const { return mBlack; }
                NkTexHandle GetNormal1x1() const { return mNormal; }
                NkTexHandle GetError()     const { return mError; }
                NkTexHandle GetBRDFLUT();              // calcule a la demande, cache

                // ── Stats ────────────────────────────────────────────────────
                uint32 GetTextureCount() const { return (uint32)mTextures.Size(); }
                uint64 GetEstimatedVRAMBytes() const { return mTotalBytes; }

            private:
                struct TexEntry {
                    NkTextureHandle rhi;
                    NkSamplerHandle sampler;        // peut etre l'un des samplers de NkResources
                    NkString        path;
                    uint32          width    = 0;
                    uint32          height   = 0;
                    uint32          mipLevels= 1;
                    uint32          refCount = 0;
                    bool            isRT     = false;
                    bool            isCube   = false;
                    bool            ownsTexture = true;    // si false, la texture appartient a un autre layer
                                                            // (NkResources) — ne pas la detruire au release
                    bool            ownsSampler = false;   // si true, on detruit le sampler au release
                    uint64          bytes    = 0;
                };

                NkIDevice*                       mDevice    = nullptr;
                NkResources*                     mResources = nullptr;
                NkHashMap<uint64, TexEntry>      mTextures;
                NkHashMap<NkString, NkTexHandle> mPathCache;
                uint64                           mNextId    = 1;
                uint64                           mTotalBytes= 0;

                NkImageLoaderFn mCustomLoad = nullptr;
                NkImageFreeFn   mCustomFree = nullptr;
                void*           mCustomUser = nullptr;

                NkTexHandle mWhite, mBlack, mNormal, mError, mBRDFLUT;

                NkTexHandle AllocHandle();
                NkTexHandle WrapRHI(NkTextureHandle rhi, NkSamplerHandle sampler,
                                    uint32 w, uint32 h, uint32 mips,
                                    const NkString& dbgName,
                                    bool ownsSampler,
                                    bool ownsTexture = true);

                bool         LoadWithNKImage(const NkString& path, NkImageData& out);
                void         FreeImageData (NkImageData& data);
                NkSamplerHandle PickSampler(const NkLoadOptions& opts) const;

                NkTexHandle UploadColorTexture(const uint8* rgba, uint32 w, uint32 h,
                                                bool srgb, bool genMips,
                                                NkSamplerHandle sampler,
                                                const char* debugName);
                NkTexHandle UploadHDRTexture  (const float32* rgbaF, uint32 w, uint32 h,
                                                NkSamplerHandle sampler,
                                                const char* debugName);
                NkTexHandle UploadCubemap     (const uint8* faces[6], uint32 w, uint32 h,
                                                bool srgb,
                                                NkSamplerHandle sampler,
                                                const char* debugName);

                NkTexHandle CreateBRDFLUT();
                static uint64 EstimateBytes(uint32 w, uint32 h, uint32 mips,
                                            uint32 bytesPerPixel, uint32 layers = 1);
        };

    } // namespace renderer
} // namespace nkentseu
