#pragma once
// =============================================================================
// NkOffscreenTarget.h
// Cible de rendu offscreen : RTT, shadow maps, G-buffer, screen effects.
//
// Un NkOffscreenTarget encapsule un ou plusieurs render targets + depth,
// un render pass et un framebuffer. Il expose des méthodes pour :
//   - Binder / débinder comme cible de rendu
//   - Lire le résultat comme texture (NkTexId)
//   - Redimensionner dynamiquement
//   - Blitter vers un autre target ou le swapchain
//
// Usage :
//   NkOffscreenDesc desc;
//   desc.width  = 1920;
//   desc.height = 1080;
//   desc.colorFormat = NkGPUFormat::NK_RGBA16_FLOAT;
//   desc.hasDepth = true;
//   auto* target = renderer->CreateOffscreenTarget(desc);
//
//   // Pendant la frame :
//   target->Begin(cmd);
//   // ... draw calls ...
//   target->End(cmd);
//   auto colorTex = target->GetColorTexId(0); // résultat comme texture
// =============================================================================
#include "NKRHI/Core/NkTypes.h"
#include "NKRHI/Core/NkDescs.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"

#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKRenderer/Core/NkMeshCache.h"

namespace nkentseu {

    // =============================================================================
    // Descripteur de cible offscreen
    // =============================================================================
    struct NkOffscreenDesc {
        uint32          width         = 512;
        uint32          height        = 512;
        // Attachements couleur (jusqu'à 8)
        uint32          colorCount    = 1;
        NkGPUFormat     colorFormats[8] = {
            NkGPUFormat::NK_RGBA8_SRGB,
        };
        NkClearColor    clearColors[8];
        // Profondeur
        bool            hasDepth      = true;
        NkGPUFormat     depthFormat   = NkGPUFormat::NK_D32_FLOAT;
        float32         clearDepth    = 1.f;
        // MSAA (résolution automatique MSAA→non-MSAA si >1)
        NkSampleCount   msaa          = NkSampleCount::NK_S1;
        // Mip-maps pour les couleurs (pour bloom, blurs)
        bool            generateMips  = false;
        // Redimensionnable dynamiquement
        bool            resizable     = true;
        // Nommage debug
        const char*     debugName     = nullptr;
    };

    // =============================================================================
    // G-Buffer (préréglage pour le deferred rendering)
    // =============================================================================
    inline NkOffscreenDesc MakeGBufferDesc(uint32 w, uint32 h) {
        NkOffscreenDesc d;
        d.width  = w; d.height = h;
        d.colorCount = 4;
        d.colorFormats[0] = NkGPUFormat::NK_RGBA8_SRGB;     // albedo + AO
        d.colorFormats[1] = NkGPUFormat::NK_RGBA16_FLOAT;   // normal (RGB) + roughness (A)
        d.colorFormats[2] = NkGPUFormat::NK_RGBA8_UNORM;    // metallic + roughness + subsurface
        d.colorFormats[3] = NkGPUFormat::NK_RGBA16_FLOAT;   // emission + depth linear
        d.hasDepth = true;
        d.depthFormat = NkGPUFormat::NK_D32_FLOAT;
        d.debugName = "GBuffer";
        return d;
    }

    // =============================================================================
    // NkOffscreenTarget
    // =============================================================================
    class NkOffscreenTarget {
        public:
            NkOffscreenTarget(NkIDevice* device, NkTextureLibrary* texLib,
                            const NkOffscreenDesc& desc);
            ~NkOffscreenTarget();

            bool Initialize();
            void Shutdown();
            bool IsValid() const;

            // ── Rendu ─────────────────────────────────────────────────────────────────
            // Begin : transite les textures en RENDER_TARGET, commence la render pass
            void Begin(NkICommandBuffer* cmd,
                        NkLoadOp colorLoad  = NkLoadOp::NK_CLEAR,
                        NkLoadOp depthLoad  = NkLoadOp::NK_CLEAR);

            // End : termine la render pass, transite en SHADER_READ
            void End(NkICommandBuffer* cmd);

            // Begin/End sur une passe existante (pour les sous-passes)
            void BeginSubpass(NkICommandBuffer* cmd, uint32 subpassIndex = 0);

            // ── Accès aux textures résultat ───────────────────────────────────────────
            NkTexId       GetColorTexId(uint32 attachmentIndex = 0) const;
            NkTexId       GetDepthTexId()                           const;
            NkTextureHandle GetColorHandle(uint32 index = 0)        const;
            NkTextureHandle GetDepthHandle()                        const;

            // ── Opérations ────────────────────────────────────────────────────────────
            // Blit vers un autre target (copie avec filtrage)
            void BlitTo(NkICommandBuffer* cmd, NkOffscreenTarget* dst,
                        uint32 srcAttachment = 0, uint32 dstAttachment = 0,
                        NkFilter filter = NkFilter::NK_LINEAR);

            // Blit vers le swapchain
            void BlitToSwapchain(NkICommandBuffer* cmd, NkIDevice* device,
                                NkFilter filter = NkFilter::NK_LINEAR);

            // Génération des mip-maps pour un attachement (post-rendu)
            void GenerateMips(NkICommandBuffer* cmd, uint32 attachmentIndex = 0);

            // ── Resize dynamique ──────────────────────────────────────────────────────
            // Recrée tous les attachements à la nouvelle taille
            bool Resize(uint32 newWidth, uint32 newHeight);

            // ── Accès aux handles RHI (pour l'intégration pipeline) ───────────────────
            NkRenderPassHandle  GetRenderPass()  const { return mRenderPass; }
            NkFramebufferHandle GetFramebuffer() const { return mFramebuffer; }

            // ── Dimensions ────────────────────────────────────────────────────────────
            uint32 GetWidth()  const { return mDesc.width; }
            uint32 GetHeight() const { return mDesc.height; }
            NkRect2D GetFullRect() const {
                return {0, 0, (int32)mDesc.width, (int32)mDesc.height};
            }
            NkViewport GetFullViewport() const {
                return {0.f, 0.f, (float32)mDesc.width, (float32)mDesc.height};
            }

            // ── Descripteur ───────────────────────────────────────────────────────────
            const NkOffscreenDesc& GetDesc() const { return mDesc; }

        private:
            NkIDevice*          mDevice;
            NkTextureLibrary*   mTexLib;
            NkOffscreenDesc     mDesc;

            NkRenderPassHandle  mRenderPass;
            NkFramebufferHandle mFramebuffer;

            // Attachements couleur
            NkVector<NkTexId>         mColorTexIds;
            NkVector<NkTextureHandle> mColorHandles;
            // Attachements couleur MSAA resolve (si msaa > 1)
            NkVector<NkTexId>         mResolveTexIds;
            NkVector<NkTextureHandle> mResolveHandles;
            // Profondeur
            NkTexId           mDepthTexId = NK_INVALID_TEX;
            NkTextureHandle   mDepthHandle;

            bool mInsideRenderPass = false;

            bool CreateAttachments();
            bool CreateRenderPass();
            bool CreateFramebuffer();
            void DestroyAll();
    };

    // =============================================================================
    // NkOffscreenBlitter — utilitaire de blit fullscreen
    // (utilisé en interne par NkPostProcessStack et NkOffscreenTarget)
    // =============================================================================
    class NkOffscreenBlitter {
        public:
            explicit NkOffscreenBlitter(NkIDevice* device);
            ~NkOffscreenBlitter();

            bool Initialize(NkRenderPassHandle targetPass);
            void Shutdown();

            // Blit une texture vers la cible courante (fullscreen quad)
            void Blit(NkICommandBuffer* cmd,
                    NkTextureHandle src, NkSamplerHandle sampler,
                    uint32 dstWidth, uint32 dstHeight);

            void BlitWithTint(NkICommandBuffer* cmd,
                            NkTextureHandle src, NkSamplerHandle sampler,
                            math::NkVec4 tint,
                            uint32 dstWidth, uint32 dstHeight);

        private:
            NkIDevice*       mDevice;
            NkShaderHandle   mShader;
            NkPipelineHandle mPipeline;
            NkDescSetHandle  mLayout;
            NkMeshCache*     mMeshCache;  // pour le screen quad
            NkSamplerHandle  mNearestSampler, mLinearSampler;
    };

} // namespace nkentseu