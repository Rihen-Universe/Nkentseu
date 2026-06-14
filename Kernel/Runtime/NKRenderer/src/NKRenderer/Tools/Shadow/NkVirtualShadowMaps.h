#pragma once
// =============================================================================
// NkVirtualShadowMaps.h — NKRenderer Tools/Shadow (style UE5 simplifie)
//
// Systeme multi-lights de shadow maps unifie. Remplace NkShadowSystem (mono
// directional CSM) avec support de :
//   - Directional CSM (1 a kMaxCascades cascades)
//   - Spot light (1 tile par spot)
//   - Point light (6 tiles par point = cubemap virtuel unrolled)
//
// Tous les tiles partagent UN SEUL atlas D32_FLOAT (4096^2 par defaut)
// alloue dynamiquement chaque frame via NkShadowAtlasPacker (skyline rectpack).
//
// Le shader sample le tile correspondant via :
//   1. Lookup uShadows.firstSlotPerLight[lightIdx] + sub-cascade/face
//   2. Calcul shadowMatrix * worldPos -> UV dans le tile -> sample atlas
//
// Pas de page caching (UE5 vrai VSM) ni LOD adaptatif pour V0. Refactor
// possible vers vrai VSM page-based plus tard sans toucher l'API publique.
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRenderer/Core/NkCamera.h"
#include "NKRenderer/Tools/Shadow/NkShadowAtlasPacker.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"

namespace nkentseu {
    namespace renderer {
        class NkMeshSystem;
        class NkMaterialSystem;
        class NkRender3D;

        // Modes PCF/PCSS, identique a NkPCFMode dans NkShadowSystem.
        enum class NkVSMShadowQuality : uint8 {
            NONE   = 0,
            PCF3x3 = 1,
            PCF5x5 = 2,
            POISSON= 3,
            PCSS   = 4,
        };

        // Type de slot, doit matcher l'enum cote shader.
        enum class NkVSMSlotType : int32 {
            DIR_CASCADE = 0,
            SPOT        = 1,
            POINT_FACE  = 2,
        };

        // Limite hard codee, doit matcher kMaxShadowSlots cote shader.
        constexpr uint32 kMaxShadowSlots   = 256;
        constexpr uint32 kMaxLightsShadow  = 32;   // doit matcher uLights array
        constexpr uint32 kMaxCascades      = 4;
        constexpr uint32 kInvalidSlotIdx   = 0xFFFFFFFFu;

        struct NkVirtualShadowMapsConfig {
            // Taille atlas (carre, D32_FLOAT). 4096 par defaut = 64 MB VRAM.
            // Reduire a 2048 pour econome en VRAM, augmenter a 8192 pour qualite.
            uint32   atlasSize       = 4096;
            // Nb de cascades pour les directional. Max kMaxCascades.
            uint32   numCascades     = 4;
            // PCF mode pour tout le sampling.
            NkVSMShadowQuality quality = NkVSMShadowQuality::PCF5x5;
            // Cascade splits log+uniform blend factor (CSM Practical Cascaded Shadow).
            float32  cascadeLambda   = 0.75f;
            float32  cascadeNear     = 0.1f;
            float32  cascadeFar      = 200.f;
            // Bias depth en NDC space (apres mapping [0,1]). Scale par 1/(N.L)
            // cote shader (slope bias). NkVSM v1 : reduit a 0.0005 car le
            // normal bias en world units (normalBias ci-dessous) couvre la
            // majorite des cas. Depth bias = anti-acne fin uniquement.
            float32  shadowBias      = 0.0005f;
            // Normal bias en WORLD UNITS : pousse worldPos le long de N avant
            // la projection shadow. Anti peter-panning (decollement ombre au
            // pied du caster). 0.05 = 5cm, marche pour la plupart des scenes.
            // Pour les casters tres petits/larges, scale en consequence.
            float32  normalBias      = 0.05f;
            // Softness UV space pour PCF/PCSS.
            float32  softness        = 0.003f;
            // Stratification tile size pour cascades : tile[i] = baseTile / (1 << i).
            uint32   cascadeBaseTile = 1024;
            // Tile size pour spot (constant V0, pourra etre adaptatif distance V1).
            uint32   spotTile        = 512;
            // Tile size pour 1 face de cubemap point (6 tiles par point caster).
            uint32   pointFaceTile   = 256;
            // Stable shadows : snap au texel pour eviter shadow swimming.
            bool     stable          = true;
            // Mode FIXE radius par cascade (V0 stable, anti-flickering total).
            // Si true : radius par cascade = cascadeFixedRadius[i] (en world units).
            // Si false : sphere-fit du sub-frustum camera (qualite adaptive mais
            // potentiel shadow swimming si la cam bouge).
            bool     useFixedCascadeRadius = true;
            // Radius fixes par cascade (8/16/32/64 par defaut, croissant en
            // distance pour couvrir near a far autour de la cam). Ignores si
            // useFixedCascadeRadius=false.
            float32  cascadeFixedRadius[kMaxCascades] = {8.f, 16.f, 32.f, 64.f};
        };

        // Slot CPU (info de quoi rendre et ou).
        struct NkShadowSlot {
            NkMat4f         shadowMatrix  = NkMat4f::Identity();  // T * lightProj * lightView
            NkMat4f         renderMatrix  = NkMat4f::Identity();  // lightProj * lightView (pour render)
            NkShadowTileRect tileRect;                            // pixel rect dans l'atlas
            NkVec4f         tileUV        = {0,0,0,0};            // version UV [0,1]
            NkVec4f         lightPosOrDir = {0,0,0,0};            // pos (POINT/SPOT) ou dir (DIR), .w = range/split
            uint32          lightIdx      = 0;                    // index dans uLights
            NkVSMSlotType   slotType      = NkVSMSlotType::DIR_CASCADE;
            uint32          subIdx        = 0;                    // cascade idx (DIR) ou face idx (POINT)
            uint32          tileSize      = 0;                    // pixels (viewport/scissor)
            // NkVSM v1 caching : si true, le tile est deja a jour dans l'atlas
            // (rendu lors d'une frame precedente) et on peut skip son re-render
            // pendant la passe Shadow. Set par AllocSlotsForLights en comparant
            // l'etat de la light vs le cache mLightCache.
            bool            cached        = false;
        };

        // NkVSM v1 caching : etat persistant per-light (survit BeginFrame).
        // Compare avec l'etat actuel pour detecter les changements pos/dir/range.
        struct NkLightShadowCache {
            NkVec3f         lastPosition  = {0, 0, 0};
            NkVec3f         lastDirection = {0, 0, 0};
            float32         lastRange     = 0.f;
            bool            renderedOnce  = false;
            bool            wasStatic     = false;
        };

        class NkVirtualShadowMaps {
        public:
            bool Init(NkIDevice* d, NkMeshSystem* m, NkMaterialSystem* mat,
                      const NkVirtualShadowMapsConfig& cfg = {},
                      uint32 framesInFlight = 3);
            void Shutdown();

            // Setter config (live tweakable sauf atlasSize qui necessite re-init).
            void                                    SetConfig(const NkVirtualShadowMapsConfig& c) { mCfg = c; }
            NkVirtualShadowMapsConfig&              GetConfig()       noexcept { return mCfg; }
            const NkVirtualShadowMapsConfig&        GetConfig() const noexcept { return mCfg; }

            // RenderAllShadows : entry-point unique par frame. Lit le scene
            // context via mRender3D, alloue les slots, upload UBO, rend tous
            // les tiles. Appele par la passe 'Shadows' du RenderGraph.
            void RenderAllShadows(NkICommandBuffer* cmd);

            // Wirage avec NkRender3D pour iterer les drawcalls opaques.
            void SetRenderer3D(NkRender3D* r) noexcept { mRender3D = r; }

            // Accesseurs RHI pour le binding par NkRender3D :
            NkTextureHandle    GetAtlasTexture()      const { return mAtlasRhi; }
            NkSamplerHandle    GetAtlasSampler()      const { return mShadowSampler; }     // compare-mode
            NkSamplerHandle    GetAtlasRawSampler()   const { return mShadowRawSampler; }  // PCSS blocker
            // Buffer du slot courant pour le bind. NkRender3D::preBindGlobalSet
            // bind chaque slot du ring sur son descriptor set respectif (cf.
            // SetMultiFrameBuffers ci-dessous).
            NkBufferHandle     GetShadowSlotsUBO()    const {
                return (mCurFrameSlot < mUBOSlotsRing.Size())
                       ? mUBOSlotsRing[mCurFrameSlot] : NkBufferHandle{};
            }
            // Accesseurs pour le pre-bind ring (NkRender3D bind chaque buffer
            // du ring sur le descriptor set correspondant).
            uint32 GetRingSize() const { return (uint32)mUBOSlotsRing.Size(); }
            NkBufferHandle GetRingBuffer(uint32 i) const {
                return (i < mUBOSlotsRing.Size()) ? mUBOSlotsRing[i] : NkBufferHandle{};
            }
            NkRenderPassHandle GetShadowRenderPass()  const { return mShadowRP; }

            // Diagnostics.
            uint32 GetActiveSlotCount() const { return mActiveSlotCount; }
            uint32 GetAtlasSize()       const { return mCfg.atlasSize; }

        private:
            // Allocation slots pour chaque light castShadow=true.
            void   AllocSlotsForLights(const NkCamera3D& mainCam,
                                       const NkVector<NkLightDesc>& lights);
            void   AllocSlotsDirectional(const NkCamera3D& mainCam,
                                         const NkLightDesc& light,
                                         uint32 lightIdx);
            void   AllocSlotsSpot       (const NkLightDesc& light, uint32 lightIdx);
            void   AllocSlotsPoint      (const NkLightDesc& light, uint32 lightIdx);

            // Helper : calcule splits CSM log+uniform blend.
            void   ComputeCascadeSplits(float32 nearP, float32 farP,
                                        float32 lambda, uint32 numCascades,
                                        float32 outSplits[kMaxCascades]) const;

            // Helper : compute light view+proj pour CSM cascade [c].
            // Reprend le pattern de NkShadowSystem (sphere-fit + texel snap).
            void   ComputeDirectionalCascade(const NkCamera3D& mainCam,
                                              const NkLightDesc& light,
                                              uint32 cascadeIdx,
                                              float32 splitNear, float32 splitFar,
                                              uint32 tilePx,
                                              NkMat4f& outView, NkMat4f& outProj) const;

            // Upload du UBO ShadowSlots (apres BeginFrame).
            void   UploadSlotsUBO();

            // ----- members -----
            NkIDevice*               mDevice    = nullptr;
            NkMeshSystem*            mMesh      = nullptr;
            NkMaterialSystem*        mMat       = nullptr;
            NkRender3D*              mRender3D  = nullptr;
            NkVirtualShadowMapsConfig mCfg;

            NkTextureHandle          mAtlasRhi;
            NkSamplerHandle          mShadowSampler;       // compare-mode pour PCF
            NkSamplerHandle          mShadowRawSampler;    // sans compare, pour PCSS blocker
            // ShadowSlotsUBO en RING multi-frame (sinon data hazard sur 3 frames
            // in flight : CPU ecrase l'UBO pendant que GPU lit encore -> flickering).
            NkVector<NkBufferHandle> mUBOSlotsRing;
            uint32                   mFramesInFlight = 3;
            uint32                   mCurFrameSlot   = 0;
            NkFramebufferHandle      mShadowFB;            // FBO depth-only avec mAtlasRhi
            NkRenderPassHandle       mShadowRP;            // RP auto-cree par CreateFramebuffer (VK)
            NkPipelineHandle         mPipeline;            // shadow depth-only pipeline (shared)

            NkShadowAtlasPacker      mPacker;
            NkShadowSlot             mSlots[kMaxShadowSlots];
            uint32                   mActiveSlotCount = 0;
            int32                    mFirstSlotPerLight[kMaxLightsShadow];  // -1 si pas de shadow
            int32                    mSlotCountPerLight[kMaxLightsShadow];

            // NkVSM v1 : cache d'etat per-light pour shadowStatic=true.
            // Persiste a travers BeginFrame (pas reset).
            NkLightShadowCache       mLightCache[kMaxLightsShadow];
            // Diagnostics : nb de slots rendered vs cached cette frame.
            uint32                   mRenderedSlotsCount = 0;
            uint32                   mCachedSlotsCount   = 0;

        public:
            // Diag : nb de slots qui ont reellement rendered cette frame
            // (les autres ont ete cached). Indique l'efficacite du caching.
            uint32 GetRenderedSlotsCount() const { return mRenderedSlotsCount; }
            uint32 GetCachedSlotsCount()   const { return mCachedSlotsCount;   }
        };

    } // namespace renderer
} // namespace nkentseu
