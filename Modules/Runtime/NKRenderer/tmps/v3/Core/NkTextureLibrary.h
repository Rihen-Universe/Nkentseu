#pragma once
// =============================================================================
// NkTextureLibrary.h
// Gestion haut-niveau des textures : chargement, cache, atlas, streaming.
//
// API simple :
//   auto tex = texLib->Load("Textures/wall_albedo.png");
//   auto cube = texLib->LoadCubemap({"px.hdr","nx.hdr",...});
//   auto rt   = texLib->CreateRenderTarget(1920, 1080, NkGPUFormat::NK_RGBA16_FLOAT);
// =============================================================================
#include "NKRHI/Core/NkTypes.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Associative/NkUnorderedMap.h"

namespace nkentseu {

    // =============================================================================
    // Identifiant de texture (haut niveau)
    // =============================================================================
    using NkTexId = uint64;
    static constexpr NkTexId NK_INVALID_TEX = 0;

    // =============================================================================
    // Descripteur de chargement
    // =============================================================================
    struct NkTextureLoadDesc {
        NkString    path;
        bool        srgb           = true;
        bool        generateMips   = true;
        bool        flipY          = false;
        uint32      maxMips        = 0;   // 0 = toute la chaîne
        NkFilter    filter         = NkFilter::NK_LINEAR;
        NkAddressMode addressMode  = NkAddressMode::NK_REPEAT;
        float32     anisotropy     = 16.f;
        const char* debugName      = nullptr;
    };

    // =============================================================================
    // Informations sur une texture chargée
    // =============================================================================
    struct NkTextureInfo {
        NkTexId         id         = NK_INVALID_TEX;
        NkTextureHandle handle;
        NkSamplerHandle sampler;
        uint32          width      = 0;
        uint32          height     = 0;
        uint32          depth      = 1;
        uint32          mipLevels  = 1;
        uint32          arrayLayers= 1;
        NkGPUFormat     format     = NkGPUFormat::NK_RGBA8_SRGB;
        NkTextureType   type       = NkTextureType::NK_TEX2D;
        bool            isValid()  const { return handle.IsValid(); }
    };

    // =============================================================================
    // NkTextureLibrary
    // =============================================================================
    class NkTextureLibrary {
        public:
            explicit NkTextureLibrary(NkIDevice* device);
            ~NkTextureLibrary();

            // ── Chargement depuis fichier ─────────────────────────────────────────────
            // Formats supportés : PNG, JPG, TGA, BMP, HDR (via stb_image)
            // KTX, DDS (formats GPU natifs compressés)
            NkTexId Load(const NkString& path, bool srgb = true, bool generateMips = true);
            NkTexId Load(const NkTextureLoadDesc& desc);

            // Cubemap : ordre [+X, -X, +Y, -Y, +Z, -Z]
            NkTexId LoadCubemap(const NkString paths[6], bool hdr = false);
            NkTexId LoadCubemapEquirect(const NkString& hdrPath); // HDRI équirectangulaire → cubemap

            // Texture 3D / volume
            NkTexId LoadVolume(const NkString& path);

            // ── Création programmatique ───────────────────────────────────────────────
            NkTexId CreateSolid(uint8 r, uint8 g, uint8 b, uint8 a = 255, const char* name = nullptr);
            NkTexId CreateSolidF(float32 r, float32 g, float32 b, float32 a = 1.f, const char* name = nullptr);
            NkTexId CreateCheckerboard(uint32 size, uint32 cellSize, NkClearColor c0, NkClearColor c1);
            NkTexId CreateGradient(uint32 width, uint32 height,
                                    NkClearColor from, NkClearColor to, bool vertical = true);
            NkTexId CreateNoise(uint32 width, uint32 height, uint32 seed = 0);

            // ── Render Targets ────────────────────────────────────────────────────────
            NkTexId CreateRenderTarget(uint32 w, uint32 h,
                                        NkGPUFormat format    = NkGPUFormat::NK_RGBA16_FLOAT,
                                        NkSampleCount msaa    = NkSampleCount::NK_S1,
                                        const char* debugName = nullptr);
            NkTexId CreateDepthTarget(uint32 w, uint32 h,
                                        NkGPUFormat format    = NkGPUFormat::NK_D32_FLOAT,
                                        NkSampleCount msaa    = NkSampleCount::NK_S1,
                                        const char* debugName = nullptr);

            // ── Upload de données brutes ──────────────────────────────────────────────
            NkTexId Upload(const void* pixels, uint32 w, uint32 h,
                            NkGPUFormat format = NkGPUFormat::NK_RGBA8_SRGB,
                            bool generateMips  = true,
                            const char* name   = nullptr);
            NkTexId UploadHDR(const float32* pixels, uint32 w, uint32 h,
                                const char* name = nullptr);

            // ── Accès ─────────────────────────────────────────────────────────────────
            const NkTextureInfo* Get(NkTexId id)         const;
            NkTextureHandle      GetHandle(NkTexId id)   const;
            NkSamplerHandle      GetSampler(NkTexId id)  const;
            bool                 IsValid(NkTexId id)     const;

            // Textures système pré-créées
            NkTexId GetWhite()       const { return mWhiteTexId; }
            NkTexId GetBlack()       const { return mBlackTexId; }
            NkTexId GetNormal()      const { return mDefaultNormalId; }  // (0.5, 0.5, 1.0)
            NkTexId GetCheckerboard()const { return mCheckerTexId; }

            // ── Cycle de vie ──────────────────────────────────────────────────────────
            void Release(NkTexId id);        // décrément ref count
            void ReleaseAll();
            void CollectGarbage();           // détruit les textures non référencées

            // ── Statistiques ──────────────────────────────────────────────────────────
            uint32 GetLoadedCount()  const;
            uint64 GetGPUMemoryBytes() const;

            bool Initialize();
            void Shutdown();

        private:
            NkIDevice*                              mDevice;
            NkUnorderedMap<NkTexId, NkTextureInfo>  mTextures;
            NkUnorderedMap<NkString, NkTexId>       mPathCache;   // path → id (évite les doublons)
            NkUnorderedMap<NkTexId, int32>          mRefCounts;
            uint64                                  mNextId = 1;
            NkTexId                                 mWhiteTexId     = NK_INVALID_TEX;
            NkTexId                                 mBlackTexId     = NK_INVALID_TEX;
            NkTexId                                 mDefaultNormalId= NK_INVALID_TEX;
            NkTexId mCheckerTexId   = NK_INVALID_TEX;

            NkTexId AllocId();
            NkSamplerHandle GetOrCreateSampler(NkFilter filter,
                                                NkAddressMode addr,
                                                float32 anisotropy);
            NkUnorderedMap<uint64, NkSamplerHandle> mSamplerCache;

            bool LoadSTB(const NkTextureLoadDesc& desc, NkTextureInfo& out);
            bool LoadKTX(const NkTextureLoadDesc& desc, NkTextureInfo& out);
            bool LoadDDS(const NkTextureLoadDesc& desc, NkTextureInfo& out);
            bool LoadHDR(const NkTextureLoadDesc& desc, NkTextureInfo& out);
            void GenerateBRDFLUT();
    };

} // namespace nkentseu

#pragma once
// =============================================================================
// NkTextureLibrary.h
// Gestion haut-niveau des textures : chargement, cache, render targets.
// =============================================================================
#include "NKRHI/Core/NkTypes.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Associative/NkUnorderedMap.h"

namespace nkentseu {

    using NkTexId = uint64;
    static constexpr NkTexId NK_INVALID_TEX = 0;

    struct NkTextureLoadDesc {
        NkString    path;
        bool        srgb          = true;
        bool        generateMips  = true;
        bool        flipY         = false;
        uint32      maxMips       = 0;
        NkFilter    filter        = NkFilter::NK_LINEAR;
        NkAddressMode addressMode = NkAddressMode::NK_REPEAT;
        float32     anisotropy    = 16.f;
        const char* debugName     = nullptr;
    };

    struct NkTextureInfo {
        NkTexId         id         = NK_INVALID_TEX;
        NkTextureHandle handle;
        NkSamplerHandle sampler;
        uint32          width      = 0;
        uint32          height     = 0;
        uint32          depth      = 1;
        uint32          mipLevels  = 1;
        uint32          arrayLayers= 1;
        NkGPUFormat     format     = NkGPUFormat::NK_RGBA8_SRGB;
        NkTextureType   type       = NkTextureType::NK_TEX2D;
        bool            isValid()  const { return handle.IsValid(); }
    };

    class NkTextureLibrary {
    public:
        explicit NkTextureLibrary(NkIDevice* device);
        ~NkTextureLibrary();

        bool Initialize();
        void Shutdown();

        // ── Chargement ────────────────────────────────────────────────────────
        NkTexId Load(const NkString& path, bool srgb=true, bool generateMips=true);
        NkTexId Load(const NkTextureLoadDesc& desc);
        NkTexId LoadCubemap(const NkString paths[6], bool hdr=false);
        NkTexId LoadCubemapEquirect(const NkString& hdrPath);
        NkTexId LoadVolume(const NkString& path);

        // ── Création programmatique ────────────────────────────────────────────
        NkTexId CreateSolid(uint8 r,uint8 g,uint8 b,uint8 a=255,const char* name=nullptr);
        NkTexId CreateSolidF(float32 r,float32 g,float32 b,float32 a=1.f,const char* name=nullptr);
        NkTexId CreateCheckerboard(uint32 size,uint32 cellSize,NkClearColor c0,NkClearColor c1);
        NkTexId CreateGradient(uint32 w,uint32 h,NkClearColor from,NkClearColor to,bool vert=true);
        NkTexId CreateNoise(uint32 w,uint32 h,uint32 seed=0);

        // ── Render Targets ────────────────────────────────────────────────────
        NkTexId CreateRenderTarget(uint32 w,uint32 h,
                                    NkGPUFormat fmt=NkGPUFormat::NK_RGBA16_FLOAT,
                                    NkSampleCount msaa=NkSampleCount::NK_S1,
                                    const char* dbg=nullptr);
        NkTexId CreateDepthTarget(uint32 w,uint32 h,
                                    NkGPUFormat fmt=NkGPUFormat::NK_D32_FLOAT,
                                    NkSampleCount msaa=NkSampleCount::NK_S1,
                                    const char* dbg=nullptr);

        // ── Upload brut ───────────────────────────────────────────────────────
        NkTexId Upload(const void* pixels,uint32 w,uint32 h,
                        NkGPUFormat fmt=NkGPUFormat::NK_RGBA8_SRGB,
                        bool generateMips=true,const char* name=nullptr);
        NkTexId UploadHDR(const float32* pixels,uint32 w,uint32 h,const char* name=nullptr);
        NkTexId UploadGray8(const uint8* pixels,uint32 w,uint32 h,const char* name=nullptr);

        // ── Accès ─────────────────────────────────────────────────────────────
        const NkTextureInfo* Get(NkTexId id)       const;
        NkTextureHandle      GetHandle(NkTexId id) const;
        NkSamplerHandle      GetSampler(NkTexId id)const;
        bool                 IsValid(NkTexId id)   const;

        // Textures système
        NkTexId GetWhite()        const { return mWhiteId; }
        NkTexId GetBlack()        const { return mBlackId; }
        NkTexId GetNormal()       const { return mNormalId; }
        NkTexId GetCheckerboard() const { return mCheckerId; }
        NkTexId GetBRDFLUT()      const { return mBRDFLUTId; }

        // ── Cycle de vie ──────────────────────────────────────────────────────
        void Release(NkTexId id);
        void ReleaseAll();
        void CollectGarbage();

        // ── Stats ─────────────────────────────────────────────────────────────
        uint32 GetLoadedCount()    const;
        uint64 GetGPUMemoryBytes() const;

    private:
        NkIDevice*    mDevice;
        NkTexId       mNextId = 1;
        NkTexId       mWhiteId=0, mBlackId=0, mNormalId=0, mCheckerId=0, mBRDFLUTId=0;
        NkUnorderedMap<NkTexId, NkTextureInfo> mTextures;
        NkUnorderedMap<NkString, NkTexId>      mPathCache;
        NkUnorderedMap<NkTexId, int32>         mRefCounts;
        NkUnorderedMap<uint64, NkSamplerHandle>mSamplerCache;

        NkTexId AllocId();
        NkSamplerHandle GetOrCreateSampler(NkFilter f, NkAddressMode a, float32 aniso);
        bool LoadSTB(const NkTextureLoadDesc& desc, NkTextureInfo& out);
        bool LoadHDRFile(const NkTextureLoadDesc& desc, NkTextureInfo& out);
        void GenerateBRDFLUT();
    };

} // namespace nkentseu
