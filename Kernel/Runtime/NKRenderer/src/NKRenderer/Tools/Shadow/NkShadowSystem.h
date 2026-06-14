#pragma once
// =============================================================================
// NkShadowSystem.h  — NKRenderer v4.0  (Tools/Shadow/)
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRenderer/Core/NkCamera.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"

namespace nkentseu {
    namespace renderer {
        class NkMeshSystem; class NkMaterialSystem; class NkRender3D;

        enum class NkPCFMode : uint8 { NONE=0, PCF3x3, PCF5x5, POISSON, PCSS };

        struct NkShadowSystemConfig {
            uint32    resolution   = 2048;
            uint32    numCascades  = 3;
            // PCF5x5 par defaut : ombres adoucies + masquage du shadow aliasing.
            // Remettre a PCF3x3 si performance probleme (GPU bas de gamme).
            NkPCFMode pcfMode      = NkPCFMode::PCF5x5;
            float32   nearPlane    = 0.1f;
            float32   farPlane     = 200.f;
            float32   lambda       = 0.75f;
            // Bias depth (en NDC [0,1] post-projection) base que le shader scale
            // ensuite par 1/(N.L) pour adapter aux surfaces rasantes (slope bias).
            // Avec ce slope bias adaptatif, on peut utiliser un bias de base plus
            // petit (0.001 au lieu de 0.005) sans creer de shadow acne.
            float32   shadowBias   = 0.001f;
            float32   normalBias   = 0.005f;
            float32   depthBias    = 1.5f;     // legacy slope-scale (non utilise actuellement)
            // PCSS : taille de la lumiere en UV space. Plus grand = ombres plus
            // douces (penumbra plus large), au cout de plus de bruit Poisson
            // dans les samples. 0.02 = compromis visible-mais-clean. 0.01 =
            // subtil, 0.05+ = tres flou. Reglable live via M/N dans Demo3D.
            // Softness en UV space sur l'atlas. Pour une scene typique :
            //   0.001 = ~4 texels (PCF 5x5 equivalent, dur)
            //   0.003 = ~12 texels (defaut, moelleux)
            //   0.010 = ~40 texels (tres doux, cinematic)
            //   0.030+ = kernel plus grand que l'ombre -> ombre invisible
            // Tweak live via N/M dans Demo3D.
            float32   softness     = 0.003f;
            // Rayon de la sphere englobante pour le frustum-fitting orthographique
            // de la cascade 0 (D.3b minimal). Augmenter pour une scene plus large.
            float32   sceneRadius  = 10.f;
            bool      stable       = true;
            bool      visualize    = false;
        };

        class NkShadowSystem {
            public:
                bool Init(NkIDevice* d, NkMeshSystem* m, NkMaterialSystem* mat,
                        const NkShadowSystemConfig& cfg = {});
                void Shutdown();

                // Setter + getters de config — l'UBO shadow est re-uploade a chaque
                // RenderShadowPasses, donc les changements sont visibles en live
                // (utile pour un panneau editor ou un debug overlay : tweak du bias,
                // changement de resolution atlas, etc.).
                // NB : changer la resolution apres Init n'est pas live (atlas fixe).
                void                          SetConfig(const NkShadowSystemConfig& c) { mCfg=c; }
                NkShadowSystemConfig&         GetConfig()       noexcept { return mCfg; }
                const NkShadowSystemConfig&   GetConfig() const noexcept { return mCfg; }
                void BeginShadowPass(NkICommandBuffer* cmd, NkVec3f lightDir,
                                    const NkCamera3D& mainCam);
                void EndShadowPass(NkICommandBuffer* cmd);
                void RenderShadowPasses(NkICommandBuffer* cmd);

                // D.3b : Setter pour wirer le NkRender3D — necessaire pour iterer
                // les opaques castShadow et utiliser son pipeline shadow. Set par
                // NkRendererImpl apres init des deux systemes.
                void SetRenderer3D(NkRender3D* r) noexcept { mRender3D = r; }

                // Accesseurs RHI pour Render3D (qui binde le ShadowUBO + atlas dans son
                // descriptor set per-frame). En mode stub (D.3a), atlas = 1x1 depth et
                // mShadowUBO contient cascadeCount=0 -> le shader PBR retourne shadow=1.0.
                NkTextureHandle GetAtlasTexture() const { return mAtlasRhi; }
                NkSamplerHandle GetAtlasSampler() const { return mShadowSampler; }
                // Sampler non-compare pour le blocker search PCSS (lecture brute
                // de la profondeur stockee, sans depth-compare).
                NkSamplerHandle GetAtlasRawSampler() const { return mShadowRawSampler; }
                NkBufferHandle  GetShadowUBO()    const { return mUBOShadow; }
                // RP utilise pour rendre dans l'atlas shadow (depth-only). En VK
                // c'est le RP auto-cree par CreateFramebuffer ; en GL c'est un
                // handle null sans signification. Necessaire au pipeline Shadow
                // pour qu'il soit compatible avec le renderPass courant.
                NkRenderPassHandle GetShadowRenderPass() const { return mShadowRP; }
                const NkMat4f*  GetCascadeMats(uint32* n) const { *n=mActiveCascades; return mCascadeMats; }

            private:
                NkIDevice*          mDevice=nullptr;
                NkMeshSystem*       mMesh=nullptr;
                NkMaterialSystem*   mMat=nullptr;
                NkRender3D*         mRender3D=nullptr;   // set par SetRenderer3D
                NkShadowSystemConfig mCfg;

                NkTextureHandle     mAtlasRhi;          // depth texture (RHI)
                NkSamplerHandle     mShadowSampler;     // sampler avec compare-mode (PCF)
                NkSamplerHandle     mShadowRawSampler;  // sampler sans compare (PCSS blocker search)
                NkBufferHandle      mUBOShadow;       // ShadowUBO (cascadeMats + splits + biases + count)
                NkFramebufferHandle mShadowFB;        // FBO custom avec mAtlasRhi en depth attachment
                NkRenderPassHandle  mShadowRP;        // RP associe au fb (auto-cree par CreateFramebuffer en VK)
                uint32              mTileSize  = 1024; // taille d'une tile cascade dans l'atlas
                uint32              mAtlasW    = 1024;
                uint32              mAtlasH    = 1024;
                uint32              mCellsX    = 1;    // grille atlas : 1x1 ou 2x1 ou 2x2
                uint32              mCellsY    = 1;

                NkMat4f             mCascadeMats[4]={};   // T * lightProj * lightView (UBO -> shader sampling)
                NkMat4f             mCascadeRenderMats[4]={};  // lightProj * lightView seul (utilise pour le rendu)
                float32             mCascadeSplits[4]={};      // far plane en view-space distance
                uint32              mActiveCascades=0;

                NkPipelineHandle    mPipeline;
                bool                mInPass=false;

                void UploadShadowUBO();
        };
    }
} // namespace
