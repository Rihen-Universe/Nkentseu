#pragma once
// =============================================================================
// NkRaytracingSystem.h  — NKRenderer v4.0  (Tools/Raytracing/)
//
// Raytracing hardware (DXR / Vulkan RT) avec fallback raster.
// Effets supportés :
//   • Ombres RT               (NkRtEffect::NK_RT_SHADOWS)
//   • Réflexions RT           (NkRtEffect::NK_RT_REFLECTIONS)
//   • Occlusion ambiante RT   (NkRtEffect::NK_RT_AO)
//   • Illumination globale RT (NkRtEffect::NK_RT_GI — expérimental)
//   • Caustics                (NkRtEffect::NK_RT_CAUSTICS)
//
// Architecture :
//   BLAS (Bottom-Level AS) : 1 par mesh unique
//   TLAS (Top-Level AS)    : 1 par frame (instances transforms)
//   SBT  (Shader Binding Table) : géré automatiquement
//   Fallback : si le device ne supporte pas le RT → efet ignoré silencieusement
//
// Dépendance backend :
//   Vulkan : VK_KHR_ray_tracing_pipeline + VK_KHR_acceleration_structure
//   DX12   : D3D12_RAYTRACING_TIER_1_1+
//   OpenGL / DX11 / Software : fallback raster automatique
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRenderer/Core/NkCamera.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/Associative/NkHashMap.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
namespace renderer {

    class NkTextureLibrary;
    class NkMeshSystem;

    // =========================================================================
    // Effets RT disponibles (bitmask)
    // =========================================================================
    enum class NkRtEffect : uint32 {
        NK_RT_NONE        = 0,
        NK_RT_SHADOWS     = 1 << 0,
        NK_RT_REFLECTIONS = 1 << 1,
        NK_RT_AO          = 1 << 2,
        NK_RT_GI          = 1 << 3,
        NK_RT_CAUSTICS    = 1 << 4,
    };
    inline NkRtEffect operator|(NkRtEffect a, NkRtEffect b)
        { return (NkRtEffect)((uint32)a | (uint32)b); }
    inline bool NkRtHas(NkRtEffect mask, NkRtEffect flag)
        { return ((uint32)mask & (uint32)flag) != 0; }

    // =========================================================================
    // Configuration
    // =========================================================================
    struct NkRaytracingConfig {
        NkRtEffect enabledEffects = NkRtEffect::NK_RT_SHADOWS
                                  | NkRtEffect::NK_RT_REFLECTIONS;
        uint32  shadowRays        = 1;    // 1 = hard shadow, >1 = soft (PCF-RT)
        uint32  aoSamples         = 4;
        float32 aoRadius          = 0.5f;
        uint32  reflectionBounces = 1;
        float32 reflectionFalloffDist = 50.f;
        uint32  giSamples         = 1;    // GI expérimental — coûteux
        bool    denoiseOutput     = true; // utilise NkDenoiserSystem si disponible
        bool    allowFallback     = true; // raster si RT non supporté
    };

    // =========================================================================
    // Handle acceleration structure
    // =========================================================================
    struct TagRtBLAS {};
    struct TagRtTLAS {};
    using NkBLASHandle = NkRendHandle<TagRtBLAS>;
    using NkTLASHandle = NkRendHandle<TagRtTLAS>;

    // =========================================================================
    // Instance TLAS — transformée + BLAS
    // =========================================================================
    struct NkRtInstance {
        NkBLASHandle blas;
        NkMat4f      transform;     // world-space
        uint32       instanceMask  = 0xFF;
        uint32       hitGroupIdx   = 0;
        bool         opaque        = true;
        bool         castShadow    = true;
    };

    // =========================================================================
    // NkRaytracingSystem
    // =========================================================================
    class NkRaytracingSystem {
    public:
        NkRaytracingSystem()  = default;
        ~NkRaytracingSystem();

        // ── Cycle de vie ──────────────────────────────────────────────────────
        bool Init(NkIDevice*               device,
                  NkTextureLibrary*        texLib,
                  NkMeshSystem*            meshSys,
                  const NkRaytracingConfig& cfg = {});
        void Shutdown();

        // ── Gestion des BLAS (une fois par mesh) ──────────────────────────────
        // Créer / reconstruire un BLAS pour un mesh (géométrie statique ou déformée)
        NkBLASHandle BuildBLAS(NkMeshHandle mesh, bool allowUpdate = false);
        void         UpdateBLAS(NkBLASHandle blas, NkMeshHandle mesh,
                                NkICommandBuffer* cmd);
        void         DestroyBLAS(NkBLASHandle& blas);

        // ── Gestion de la TLAS (par frame) ────────────────────────────────────
        void BeginFrame(const NkCamera3DData& cam, const NkMat4f& viewProj);

        // Soumettre les instances (remplace la TLAS)
        void SetInstances(const NkRtInstance* instances, uint32 count);

        // Reconstruire la TLAS avec les instances soumises
        void BuildTLAS(NkICommandBuffer* cmd);

        // ── Dispatching des effets RT ─────────────────────────────────────────
        // outShadow/outReflect/outAO = handles render targets de sortie
        void DispatchShadows    (NkICommandBuffer* cmd, NkTexHandle gbufDepth,
                                  NkTexHandle outShadow);
        void DispatchReflections(NkICommandBuffer* cmd, NkTexHandle gbufNormal,
                                  NkTexHandle gbufAlbedo, NkTexHandle gbufDepth,
                                  NkTexHandle outReflect);
        void DispatchAO         (NkICommandBuffer* cmd, NkTexHandle gbufNormal,
                                  NkTexHandle gbufDepth, NkTexHandle outAO);
        void DispatchGI         (NkICommandBuffer* cmd, NkTexHandle gbufNormal,
                                  NkTexHandle gbufAlbedo, NkTexHandle gbufDepth,
                                  NkTexHandle outGI);

        // ── Config dynamique ──────────────────────────────────────────────────
        void SetConfig(const NkRaytracingConfig& cfg);
        void SetEffect(NkRtEffect effect, bool enabled);

        // ── Capacités ─────────────────────────────────────────────────────────
        bool IsSupported()  const { return mSupported; }
        bool IsEnabled()    const { return mReady && mSupported; }
        bool EffectActive(NkRtEffect e) const;

        const NkRaytracingConfig& GetConfig() const { return mCfg; }

    private:
        NkIDevice*          mDevice   = nullptr;
        NkTextureLibrary*   mTexLib   = nullptr;
        NkMeshSystem*       mMesh     = nullptr;
        NkRaytracingConfig  mCfg;
        bool                mSupported= false;
        bool                mReady    = false;

        NkTLASHandle                    mTLAS;
        NkHashMap<uint64, NkBLASHandle> mBLASCache;
        NkVector<NkRtInstance>          mInstances;

        NkMat4f  mView     = NkMat4f(1.f);
        NkMat4f  mProj     = NkMat4f(1.f);
        NkVec3f  mCamPos   = {};

        bool CheckDeviceSupport();
        void CreateRTPipelines();
        void DestroyRTPipelines();
    };

} // namespace renderer
} // namespace nkentseu
