#pragma once
// =============================================================================
// NkTexture.h
// Système de textures du NKRenderer.
// Gère : Texture2D, TextureCube, Texture3D, RenderTarget, TextureAtlas
//
// Architecture :
//   NkTextureManager (singleton par device) → gère le cache
//   NkTexture2D / NkTextureCube / NkTexture3D → wrappers de haut niveau
//   NkRenderTarget → surface de rendu offscreen (G-Buffer, shadow maps, etc.)
// =============================================================================
#include "NkRenderTypes.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKImage/Core/NkImage.h"

namespace nkentseu {
    namespace renderer {

        // =============================================================================
        // Paramètres de création d'une texture
        // =============================================================================
        struct NkTextureParams {
            // Format & dimensions
            NkGPUFormat     format      = NkGPUFormat::NK_RGBA8_SRGB;
            uint32          width       = 1;
            uint32          height      = 1;
            uint32          depth       = 1;      // Texture3D uniquement
            uint32          mipLevels   = 0;      // 0 = auto-générer toute la chaîne
            uint32          arrayLayers = 1;
            NkSampleCount   samples     = NkSampleCount::NK_S1;

            // Filtrage
            NkFilter        magFilter   = NkFilter::NK_LINEAR;
            NkFilter        minFilter   = NkFilter::NK_LINEAR;
            NkMipFilter     mipFilter   = NkMipFilter::NK_LINEAR;
            float           anisotropy  = 16.f;
            NkAddressMode   wrapU       = NkAddressMode::NK_REPEAT;
            NkAddressMode   wrapV       = NkAddressMode::NK_REPEAT;
            NkAddressMode   wrapW       = NkAddressMode::NK_REPEAT;
            NkBorderColor   borderColor = NkBorderColor::NK_TRANSPARENT_BLACK;

            // Flags
            bool            srgb           = true;
            bool            generateMips   = true;
            bool            isHDR          = false;
            bool            isShadowMap    = false;
            bool            isRenderTarget = false;
            bool            isStorageImage = false;
            const char*     debugName      = nullptr;

            // Presets
            static NkTextureParams Albedo() {
                NkTextureParams p;
                p.format = NkGPUFormat::NK_RGBA8_SRGB;
                p.srgb = true;
                p.generateMips = true;
                p.anisotropy = 16.f;
                return p;
            }
            static NkTextureParams Normal() {
                NkTextureParams p;
                p.format = NkGPUFormat::NK_RGBA8_UNORM;
                p.srgb = false;
                p.generateMips = true;
                p.anisotropy = 16.f;
                return p;
            }
            static NkTextureParams HDR() {
                NkTextureParams p;
                p.format = NkGPUFormat::NK_RGBA16_FLOAT;
                p.srgb = false;
                p.isHDR = true;
                p.generateMips = true;
                p.anisotropy = 16.f;
                return p;
            }
            static NkTextureParams Linear() {
                NkTextureParams p;
                p.format = NkGPUFormat::NK_RGBA8_UNORM;
                p.srgb = false;
                p.generateMips = true;
                return p;
            }
            static NkTextureParams UISprite() {
                NkTextureParams p;
                p.format = NkGPUFormat::NK_RGBA8_SRGB;
                p.srgb = true;
                p.generateMips = false;
                p.wrapU = NkAddressMode::NK_CLAMP_TO_EDGE;
                p.wrapV = NkAddressMode::NK_CLAMP_TO_EDGE;
                p.anisotropy = 1.f;
                return p;
            }
            static NkTextureParams ShadowDepth(uint32 sz = 2048) {
                NkTextureParams p;
                p.format = NkGPUFormat::NK_D32_FLOAT;
                p.width = sz; p.height = sz;
                p.srgb = false;
                p.generateMips = false;
                p.isShadowMap = true;
                p.wrapU = NkAddressMode::NK_CLAMP_TO_EDGE;
                p.wrapV = NkAddressMode::NK_CLAMP_TO_EDGE;
                return p;
            }
        };

        // =============================================================================
        // NkTexture2D
        // =============================================================================
        class NkTexture2D {
            public:
                NkTexture2D() = default;
                ~NkTexture2D() { Destroy(); }

                // Non-copiable
                NkTexture2D(const NkTexture2D&) = delete;
                NkTexture2D& operator=(const NkTexture2D&) = delete;

                // Déplaçable
                NkTexture2D(NkTexture2D&& o) noexcept;
                NkTexture2D& operator=(NkTexture2D&& o) noexcept;

                // ── Création ──────────────────────────────────────────────────────────────
                // Depuis un fichier image (PNG, JPG, HDR, TGA, BMP, QOI, etc.)
                bool LoadFromFile(NkIDevice* device, const char* path,
                                const NkTextureParams& params = NkTextureParams::Albedo());

                // Depuis des pixels en mémoire
                bool LoadFromMemory(NkIDevice* device,
                                    const void* pixels, uint32 width, uint32 height,
                                    NkGPUFormat format,
                                    const NkTextureParams& params = NkTextureParams::Albedo());

                // Depuis un NkImage déjà chargé
                bool LoadFromImage(NkIDevice* device, const NkImage& image,
                                const NkTextureParams& params = NkTextureParams::Albedo());

                // Texture procédurale (couleur unie)
                bool CreateSolid(NkIDevice* device, const NkColor4f& color, uint32 size = 4);

                // Texture procédurale checkerboard (debug)
                bool CreateCheckerboard(NkIDevice* device, const NkColor4f& a, const NkColor4f& b,
                                        uint32 size = 64, uint32 tileSize = 8);

                // Texture de 1 pixel blanc (placeholder)
                bool CreateWhite(NkIDevice* device);
                bool CreateBlack(NkIDevice* device);
                bool CreateNormal(NkIDevice* device); // (0.5, 0.5, 1.0) = normal haut

                void Destroy();

                // ── Accesseurs ────────────────────────────────────────────────────────────
                bool              IsValid()    const { return mTexHandle.IsValid(); }
                NkTextureHandle   GetHandle()  const { return mTexHandle; }
                NkSamplerHandle   GetSampler() const { return mSamplerHandle; }
                uint32            GetWidth()   const { return mWidth; }
                uint32            GetHeight()  const { return mHeight; }
                uint32            GetMipLevels() const { return mMipLevels; }
                NkGPUFormat       GetFormat()  const { return mFormat; }
                const NkString&   GetPath()    const { return mPath; }
                NkTexture2DID     GetID()      const { return mID; }
                bool              IsSRGB()     const { return mSRGB; }
                float             GetAspect()  const { return mHeight > 0 ? (float)mWidth/(float)mHeight : 1.f; }

            private:
                NkIDevice*      mDevice       = nullptr;
                NkTextureHandle mTexHandle;
                NkSamplerHandle mSamplerHandle;
                uint32          mWidth        = 0;
                uint32          mHeight       = 0;
                uint32          mMipLevels    = 1;
                NkGPUFormat     mFormat       = NkGPUFormat::NK_RGBA8_SRGB;
                bool            mSRGB         = true;
                NkString        mPath;
                NkTexture2DID   mID;

                static uint64   sIDCounter;

                bool FinalizeSampler(const NkTextureParams& params);
        };

        // =============================================================================
        // NkTextureCube (IBL, skybox, reflection probes)
        // =============================================================================
        class NkTextureCube {
            public:
                NkTextureCube() = default;
                ~NkTextureCube() { Destroy(); }
                NkTextureCube(const NkTextureCube&) = delete;
                NkTextureCube& operator=(const NkTextureCube&) = delete;

                // Charger depuis 6 fichiers (ordre: +X,-X,+Y,-Y,+Z,-Z)
                bool LoadFromFiles(NkIDevice* device, const char* faces[6]);

                // Charger depuis une image HDRI équirectangulaire (conversion CPU→Cubemap)
                bool LoadFromEquirectangular(NkIDevice* device, const char* hdriPath,
                                            uint32 faceSize = 1024);

                // Charger depuis un .hdr / .exr en latlong
                bool LoadFromLatlongFile(NkIDevice* device, const char* path,
                                        uint32 faceSize = 1024);

                void Destroy();

                bool              IsValid()   const { return mTexHandle.IsValid(); }
                NkTextureHandle   GetHandle() const { return mTexHandle; }
                NkSamplerHandle   GetSampler()const { return mSamplerHandle; }
                uint32            GetSize()   const { return mFaceSize; }
                NkTextureCubeID   GetID()     const { return mID; }

            private:
                NkIDevice*      mDevice  = nullptr;
                NkTextureHandle mTexHandle;
                NkSamplerHandle mSamplerHandle;
                uint32          mFaceSize = 0;
                NkTextureCubeID mID;
                static uint64   sIDCounter;
        };

        // =============================================================================
        // NkTexture3D (volumes, LUTs 3D de couleur, voxels, etc.)
        // =============================================================================
        class NkTexture3D {
            public:
                NkTexture3D() = default;
                ~NkTexture3D() { Destroy(); }
                NkTexture3D(const NkTexture3D&) = delete;
                NkTexture3D& operator=(const NkTexture3D&) = delete;

                bool Create(NkIDevice* device, uint32 w, uint32 h, uint32 d,
                            NkGPUFormat format = NkGPUFormat::NK_RGBA16_FLOAT,
                            const void* data = nullptr);

                // LUT 3D pour color grading (16³ ou 32³ en .cube)
                bool LoadColorLUT(NkIDevice* device, const char* cubePath);

                void Destroy();

                bool              IsValid()   const { return mTexHandle.IsValid(); }
                NkTextureHandle   GetHandle() const { return mTexHandle; }
                NkSamplerHandle   GetSampler()const { return mSamplerHandle; }
                NkTexture3DID     GetID()     const { return mID; }

            private:
                NkIDevice*      mDevice  = nullptr;
                NkTextureHandle mTexHandle;
                NkSamplerHandle mSamplerHandle;
                NkTexture3DID   mID;
                static uint64   sIDCounter;
        };

        // =============================================================================
        // NkRenderTarget
        // Surface de rendu offscreen multi-attachments.
        // Utilisé pour : G-Buffer, Shadow maps, Post-processing, Bloom, etc.
        // =============================================================================
        class NkRenderTarget {
            public:
                NkRenderTarget() = default;
                ~NkRenderTarget() { Destroy(); }
                NkRenderTarget(const NkRenderTarget&) = delete;
                NkRenderTarget& operator=(const NkRenderTarget&) = delete;

                bool Create(NkIDevice* device, const NkRenderTargetDesc& desc);

                // Recréer sur resize
                bool Resize(uint32 newWidth, uint32 newHeight);

                void Destroy();

                // ── Accesseurs ────────────────────────────────────────────────────────────
                bool                 IsValid()             const { return mIsValid; }
                uint32               GetWidth()            const { return mWidth; }
                uint32               GetHeight()           const { return mHeight; }
                NkTextureHandle      GetColorTexture(uint32 idx=0) const;
                NkTextureHandle      GetDepthTexture()     const { return mDepthTex; }
                NkSamplerHandle      GetColorSampler(uint32 idx=0) const;
                NkSamplerHandle      GetDepthSampler()     const { return mDepthSampler; }
                NkRenderPassHandle   GetRenderPass()       const { return mRenderPass; }
                NkFramebufferHandle  GetFramebuffer()      const { return mFramebuffer; }
                NkRenderTargetID     GetID()               const { return mID; }

                // Attacher plusieurs couleurs (G-Buffer : albedo, normal, ORM, emissive)
                NkRenderTarget& AddColorAttachment(NkGPUFormat fmt = NkGPUFormat::NK_RGBA16_FLOAT);
                NkRenderTarget& SetDepthAttachment(NkGPUFormat fmt = NkGPUFormat::NK_D32_FLOAT);
                bool            Finalize(NkIDevice* device, uint32 w, uint32 h, uint32 samples=1);

            private:
                struct ColorAttachment {
                    NkTextureHandle tex;
                    NkSamplerHandle sampler;
                    NkGPUFormat     format = NkGPUFormat::NK_RGBA16_FLOAT;
                };

                NkIDevice*            mDevice      = nullptr;
                NkVector<ColorAttachment> mColorAttachments;
                NkVector<NkGPUFormat>     mColorFormats;
                NkTextureHandle       mDepthTex;
                NkSamplerHandle       mDepthSampler;
                NkGPUFormat           mDepthFormat = NkGPUFormat::NK_D32_FLOAT;
                NkRenderPassHandle    mRenderPass;
                NkFramebufferHandle   mFramebuffer;
                uint32                mWidth       = 0;
                uint32                mHeight      = 0;
                uint32                mSamples     = 1;
                bool                  mIsValid     = false;
                NkRenderTargetID      mID;
                static uint64         sIDCounter;
        };

        // =============================================================================
        // NkTextureAtlas
        // Plusieurs sprites/glyphes dans une seule texture.
        // Utilisé par : NkSpriteRenderer, NkFontRenderer
        // =============================================================================
        struct NkAtlasRegion {
            NkVec2f uvMin  = {0,0};
            NkVec2f uvMax  = {1,1};
            uint32  pixX   = 0;
            uint32  pixY   = 0;
            uint32  pixW   = 0;
            uint32  pixH   = 0;
            NkString name;
        };

        class NkTextureAtlas {
            public:
                NkTextureAtlas() = default;
                ~NkTextureAtlas() = default;

                // Créer un atlas depuis un dossier de sprites (packing automatique)
                bool BuildFromDirectory(NkIDevice* device, const char* dir,
                                        uint32 maxSize = 4096);

                // Ajouter manuellement une région (texture sheet pré-découpée)
                void AddRegion(const NkString& name, uint32 x, uint32 y, uint32 w, uint32 h);
                void SetTexture(NkTexture2D* tex) { mTexture = tex; }

                const NkAtlasRegion* FindRegion(const NkString& name) const;
                NkTexture2D*         GetTexture() const { return mTexture; }
                uint32               RegionCount() const { return (uint32)mRegions.Size(); }

            private:
                NkTexture2D*               mTexture = nullptr;
                NkVector<NkAtlasRegion>    mRegions;
                NkUnorderedMap<NkString, uint32> mIndexByName;
        };

        // =============================================================================
        // NkTextureManager
        // Cache centralisé — évite les doublons, partage les handles entre matériaux.
        // =============================================================================
        class NkTextureManager {
            public:
                static NkTextureManager& Get();

                void Init(NkIDevice* device);
                void Shutdown();

                // Charger / récupérer du cache
                NkTexture2D*  LoadTexture2D(const char* path,
                                            const NkTextureParams& params = NkTextureParams::Albedo());
                NkTextureCube* LoadTextureCube(const char* hdriPath, uint32 faceSize = 1024);

                // Textures procédurales / built-in
                NkTexture2D*  GetWhite();
                NkTexture2D*  GetBlack();
                NkTexture2D*  GetFlatNormal();
                NkTexture2D*  GetCheckerboard();

                // Libérer une texture (si plus aucun matériau ne la référence)
                void          Release(NkTexture2D* tex);
                void          Release(NkTextureCube* tex);

                // Libérer les textures inutilisées (garbage collect)
                void          CollectGarbage();

                NkIDevice*    GetDevice() const { return mDevice; }

            private:
                NkTextureManager() = default;

                struct Tex2DEntry {
                    NkTexture2D* tex    = nullptr;
                    uint32       refCount = 0;
                    NkString     path;
                };
                struct TexCubeEntry {
                    NkTextureCube* tex  = nullptr;
                    uint32         refCount = 0;
                    NkString       path;
                };

                NkIDevice*  mDevice = nullptr;
                NkUnorderedMap<NkString, Tex2DEntry>   mTex2DCache;
                NkUnorderedMap<NkString, TexCubeEntry> mTexCubeCache;

                NkTexture2D* mWhite       = nullptr;
                NkTexture2D* mBlack       = nullptr;
                NkTexture2D* mFlatNormal  = nullptr;
                NkTexture2D* mChecker     = nullptr;

                void CreateBuiltins();
        };

        // =============================================================================
        // Implémentation inline des méthodes simples
        // =============================================================================

        inline NkTexture2D::NkTexture2D(NkTexture2D&& o) noexcept
            : mDevice(o.mDevice), mTexHandle(o.mTexHandle), mSamplerHandle(o.mSamplerHandle)
            , mWidth(o.mWidth), mHeight(o.mHeight), mMipLevels(o.mMipLevels)
            , mFormat(o.mFormat), mSRGB(o.mSRGB), mPath(traits::NkMove(o.mPath)), mID(o.mID) {
            o.mDevice = nullptr;
            o.mTexHandle = {};
            o.mSamplerHandle = {};
        }

        inline NkTexture2D& NkTexture2D::operator=(NkTexture2D&& o) noexcept {
            if (this != &o) {
                Destroy();
                mDevice = o.mDevice; mTexHandle = o.mTexHandle; mSamplerHandle = o.mSamplerHandle;
                mWidth = o.mWidth; mHeight = o.mHeight; mMipLevels = o.mMipLevels;
                mFormat = o.mFormat; mSRGB = o.mSRGB; mPath = traits::NkMove(o.mPath); mID = o.mID;
                o.mDevice = nullptr; o.mTexHandle = {}; o.mSamplerHandle = {};
            }
            return *this;
        }

        inline void NkTexture2D::Destroy() {
            if (!mDevice) return;
            if (mSamplerHandle.IsValid()) mDevice->DestroySampler(mSamplerHandle);
            if (mTexHandle.IsValid())     mDevice->DestroyTexture(mTexHandle);
            mTexHandle = {}; mSamplerHandle = {};
            mDevice = nullptr;
        }

        inline void NkTextureCube::Destroy() {
            if (!mDevice) return;
            if (mSamplerHandle.IsValid()) mDevice->DestroySampler(mSamplerHandle);
            if (mTexHandle.IsValid())     mDevice->DestroyTexture(mTexHandle);
            mTexHandle = {}; mSamplerHandle = {};
            mDevice = nullptr;
        }

        inline void NkTexture3D::Destroy() {
            if (!mDevice) return;
            if (mSamplerHandle.IsValid()) mDevice->DestroySampler(mSamplerHandle);
            if (mTexHandle.IsValid())     mDevice->DestroyTexture(mTexHandle);
            mTexHandle = {}; mSamplerHandle = {};
            mDevice = nullptr;
        }

    } // namespace renderer
} // namespace nkentseu
