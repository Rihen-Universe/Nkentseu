#pragma once
// =============================================================================
// NkTextureLibrary.h  — NKRenderer v4.0  (Core/)
// Bibliothèque de textures : chargement (via NKImage), cache path, built-ins.
// L'utilisateur peut brancher son propre chargeur via NkImageLoaderFn.
// =============================================================================
#include "NkRendererTypes.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKContainers/Associative/NkHashMap.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
namespace renderer {

    // =========================================================================
    // Callback chargeur d'image custom (optionnel)
    // Permet de remplacer NKImage par son propre système
    // =========================================================================
    struct NkImageData {
        uint8*  pixels   = nullptr;
        uint32  width    = 0;
        uint32  height   = 0;
        uint32  channels = 4;    // 1=Gray, 3=RGB, 4=RGBA
        bool    isHDR    = false;
        float32*hdrPixels= nullptr;
    };
    using NkImageLoaderFn  = bool(*)(const char* path, NkImageData* out, void* user);
    using NkImageFreeFn    = void(*)(NkImageData* data, void* user);

    // =========================================================================
    // Descripteur de création manuelle
    // =========================================================================
    struct NkTextureCreateDesc {
        const void* pixels    = nullptr;
        uint32      width     = 0;
        uint32      height    = 0;
        uint32      depth     = 1;
        uint32      mipLevels = 0;      // 0 = auto
        bool        srgb      = true;
        bool        genMips   = true;
        bool        isHDR     = false;
        bool        isCubemap = false;
        NkGPUFormat format    = NkGPUFormat::NK_RGBA8_SRGB;
        NkString    debugName;
    };

    // =========================================================================
    // NkTextureLibrary
    // =========================================================================
    class NkTextureLibrary {
    public:
        NkTextureLibrary() = default;
        ~NkTextureLibrary();

        bool Init(NkIDevice* device);
        void Shutdown();

        // ── Chargeur custom (optionnel — remplace NKImage) ───────────────────
        void SetCustomLoader(NkImageLoaderFn load, NkImageFreeFn free, void* user);

        // ── Chargement fichier (NKImage par défaut) ──────────────────────────
        // Formats : PNG, JPEG, BMP, TGA, HDR, PPM, QOI, GIF, ICO, WebP, SVG
        NkTexHandle Load    (const NkString& path, bool srgb=true, bool genMips=true);
        NkTexHandle LoadHDR (const NkString& path);
        NkTexHandle LoadCubemap(const NkString paths[6]);

        // ── Création manuelle ─────────────────────────────────────────────────
        NkTexHandle Create(const NkTextureCreateDesc& desc);

        // ── Render target ────────────────────────────────────────────────────
        NkTexHandle CreateRenderTarget(uint32 w, uint32 h,
                                        NkGPUFormat format,
                                        bool depth   = false,
                                        bool readable= true,
                                        const NkString& name = "");

        // ── Mise à jour runtime ──────────────────────────────────────────────
        bool Update(NkTexHandle h, const void* data, uint32 rowPitch=0,
                     uint32 mipLevel=0, uint32 layer=0);

        // ── Destruction ───────────────────────────────────────────────────────
        void Release(NkTexHandle& h);
        void ReleaseAll();

        // ── Accès RHI ────────────────────────────────────────────────────────
        NkTextureHandle GetRHIHandle (NkTexHandle h) const;
        NkSamplerHandle GetRHISampler(NkTexHandle h) const;

        // ── Built-ins ────────────────────────────────────────────────────────
        NkTexHandle GetWhite1x1()   const { return mWhite; }
        NkTexHandle GetBlack1x1()   const { return mBlack; }
        NkTexHandle GetNormal1x1()  const { return mNormal; }
        NkTexHandle GetError()      const { return mError; }
        NkTexHandle GetBRDFLUT()    const { return mBRDFLUT; }

    private:
        struct TexEntry {
            NkTextureHandle rhi;
            NkSamplerHandle sampler;
            NkString        path;
            uint32          refCount = 0;
            bool            isRT     = false;
        };

        NkIDevice*                      mDevice     = nullptr;
        NkHashMap<uint64, TexEntry>     mTextures;
        NkHashMap<NkString, NkTexHandle>mPathCache;
        uint64                          mNextId     = 1;

        NkImageLoaderFn mCustomLoad = nullptr;
        NkImageFreeFn   mCustomFree = nullptr;
        void*           mCustomUser = nullptr;

        NkTexHandle mWhite, mBlack, mNormal, mError, mBRDFLUT;

        NkTexHandle AllocHandle();
        NkTexHandle CreateBuiltin(uint8 r, uint8 g, uint8 b, uint8 a,
                                   const NkString& name);
        NkTexHandle CreateBRDFLUT();
        bool        LoadWithNKImage(const NkString& path, NkImageData& out);
        NkTexHandle UploadToGPU(const NkTextureCreateDesc& desc);
    };

} // namespace renderer
} // namespace nkentseu
