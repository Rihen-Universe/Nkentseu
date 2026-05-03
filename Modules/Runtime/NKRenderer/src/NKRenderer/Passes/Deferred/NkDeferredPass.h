#pragma once
// =============================================================================
// NkDeferredPass.h  — NKRenderer v4.0  (Passes/Deferred/)
//
// Passe de rendu différé (Deferred Shading).
// Pipeline : Geometry pass (G-buffer fill) → Lighting pass (accumulation HDR).
//
// G-buffer layout :
//   RT0 : Albedo.rgb  + Metallic.a         (RGBA8_UNORM)
//   RT1 : Normal.xyz  + Roughness.a        (RGBA16F)
//   RT2 : Emissive.rgb + AO.a              (RGBA16F)
//   RT3 : Velocity.rg                      (RG16F)   — TAA / motion blur
//   DS  : Depth 32F + Stencil 8            (D32_FLOAT_S8)
//   Out : LightAccum.rgba (HDR)            (RGBA16F)
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRenderer/Core/NkRendererConfig.h"
#include "NKRenderer/Core/NkRenderGraph.h"
#include "NKRenderer/Core/NkCamera.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Core/NkDescs.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
namespace renderer {

    class NkTextureLibrary;

    // =========================================================================
    // Configuration
    // =========================================================================
    struct NkDeferredConfig {
        uint32  maxLights           = 1024;
        uint32  tileSize            = 16;
        bool    tiledLighting       = true;
        bool    clusteredLighting   = false;
        uint32  clusterSlicesZ      = 24;
        bool    ibl                 = true;
        bool    ssao                = true;
        bool    ssr                 = false;
        bool    outputVelocity      = true;
        float32 emissiveScale       = 1.f;
    };

    // =========================================================================
    // G-buffer — handles renderer vers les render targets
    // =========================================================================
    struct NkGBuffer {
        NkTexHandle albedoMetallic;   // RT0 RGBA8
        NkTexHandle normalRoughness;  // RT1 RGBA16F
        NkTexHandle emissiveAO;       // RT2 RGBA16F
        NkTexHandle velocity;         // RT3 RG16F
        NkTexHandle depth;            // DS  D32S8
        NkTexHandle lightAccum;       // Out RGBA16F (résultat HDR)

        bool IsValid() const noexcept {
            return albedoMetallic.IsValid() && normalRoughness.IsValid()
                && emissiveAO.IsValid() && depth.IsValid() && lightAccum.IsValid();
        }
    };

    // =========================================================================
    // NkDeferredPass
    // =========================================================================
    class NkDeferredPass {
    public:
        NkDeferredPass()  = default;
        ~NkDeferredPass();

        bool Init(NkIDevice*             device,
                  NkRenderGraph*         graph,
                  NkTextureLibrary*      texLib,
                  uint32                 width,
                  uint32                 height,
                  const NkDeferredConfig& cfg = {});
        void Shutdown();

        // Inscrit les passes Geometry + Lighting dans le NkRenderGraph
        void RegisterToRenderGraph();

        // ── Par frame ─────────────────────────────────────────────────────────
        void SetCamera(const NkCamera3DData& cam, const NkMat4f& viewProj);

        void BeginGeometry(NkICommandBuffer* cmd);
        void EndGeometry  (NkICommandBuffer* cmd);

        void SubmitLight (const NkLightDesc& light);
        void SubmitLights(const NkLightDesc* lights, uint32 count);
        void ClearLights ();

        void BeginLighting(NkICommandBuffer* cmd);
        void EndLighting  (NkICommandBuffer* cmd);

        bool Resize(uint32 newWidth, uint32 newHeight);

        // ── Accès ─────────────────────────────────────────────────────────────
        const NkGBuffer&        GetGBuffer()        const { return mGBuffer; }
        NkTexHandle             GetLightAccum()     const { return mGBuffer.lightAccum; }
        const NkDeferredConfig& GetConfig()         const { return mCfg; }
        uint32                  GetWidth()          const { return mWidth; }
        uint32                  GetHeight()         const { return mHeight; }
        uint32                  GetSubmittedLights()const { return (uint32)mLights.Size(); }

    private:
        NkIDevice*        mDevice  = nullptr;
        NkRenderGraph*    mGraph   = nullptr;
        NkTextureLibrary* mTexLib  = nullptr;
        NkDeferredConfig  mCfg;
        uint32            mWidth   = 0;
        uint32            mHeight  = 0;
        bool              mReady   = false;

        NkGBuffer     mGBuffer;
        NkGraphResId  mResAlbedo   = NK_INVALID_RES_ID;
        NkGraphResId  mResNormal   = NK_INVALID_RES_ID;
        NkGraphResId  mResEmissive = NK_INVALID_RES_ID;
        NkGraphResId  mResVelocity = NK_INVALID_RES_ID;
        NkGraphResId  mResDepth    = NK_INVALID_RES_ID;
        NkGraphResId  mResAccum    = NK_INVALID_RES_ID;

        NkVector<NkLightDesc> mLights;

        NkMat4f  mView     = NkMat4f(1.f);
        NkMat4f  mProj     = NkMat4f(1.f);
        NkMat4f  mViewProj = NkMat4f(1.f);
        NkVec3f  mCamPos   = {};

        NkBufferHandle mLightDataBuf;
        NkBufferHandle mLightGridBuf;
        NkBufferHandle mLightListBuf;

        bool CreateGBufferTextures();
        void DestroyGBufferTextures();
        void BuildLightGrid(NkICommandBuffer* cmd);
        NkTextureDesc MakeGBufDesc(uint32 w, uint32 h,
                                    NkGPUFormat fmt, bool isDepth = false) const;
    };

} // namespace renderer
} // namespace nkentseu
