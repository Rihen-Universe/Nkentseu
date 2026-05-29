#pragma once
// =============================================================================
// NkPostProcessStack.h  — NKRenderer v4.0  (Tools/PostProcess/)
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRenderer/Core/NkRendererConfig.h"
#include "NKRenderer/Core/NkRenderTarget.h"   // Phase H.2 : bloom mipchain via NkRenderTarget
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKRHI/Core/NkIDevice.h"

namespace nkentseu {
    namespace renderer {
        class NkTextureLibrary;
        class NkMeshSystem;
        class NkShaderLibrary;
        class NkResources;

        class NkPostProcessStack {
            public:
                bool Init(NkIDevice* d, NkTextureLibrary* t, NkMeshSystem* m,
                          NkShaderLibrary* sl, NkResources* res,
                          uint32 w, uint32 h);
                void Shutdown();
                void OnResize(uint32 w, uint32 h);
                void SetConfig(const NkPostConfig& c) { mCfg=c; }
                NkPostConfig& GetConfig() { return mCfg; }
                NkTexHandle Execute(NkICommandBuffer* cmd,
                                    NkTexHandle hdrIn, NkTexHandle depth,
                                    NkTexHandle velocity=NkTexHandle::Null());

                // Variante consommant directement des handles RHI (utilise par
                // NkRenderGraph qui stocke des NkTextureHandle dans ses transients).
                // Ne wrappe pas dans NkTextureLibrary — le bind est fait direct
                // via mDevice->BindTextureSampler dans DrawFullscreen.
                // Phase H.2 : bloomTex optionnel — si valide, sample au binding=1
                // du tonemap (= mip 0 du bloom upsample, vrai dual-Kawase).
                // Si invalide, le shader ignore le bloom (texture noire = 0).
                void ExecuteRHI(NkICommandBuffer* cmd, NkTextureHandle hdrIn,
                                NkTextureHandle bloomTex = NkTextureHandle{},
                                NkTextureHandle ssaoTex  = NkTextureHandle{});

                // Phase H.2 : sub-passes bloom multi-pass. Le RenderGraph
                // (NkRendererImpl::BuildDefaultRenderGraph) appelle ces methodes
                // dans une pass deja ouverte (color attachment = mip cible).
                // Elles ne font QUE bind pipeline + descriptor + draw quad
                // (pas de BeginRenderPass — c'est le RG qui s'en charge).
                //
                // DrawBloomDown : extrait + downsample 2x (13-tap COD style).
                void DrawBloomDownPass(NkICommandBuffer* cmd, NkTextureHandle src,
                                       uint32 srcW, uint32 srcH, float threshold);

                // DrawBloomUp : tent filter 3x3 + blend additif sur la mip courante.
                void DrawBloomUpPass(NkICommandBuffer* cmd, NkTextureHandle src,
                                     uint32 srcW, uint32 srcH, float strength);

                // Phase H.3 : pass SSAO (depth-only). depthSrc = HDR depth
                // transient. ssaoW/ssaoH = dimensions du RT cible (typique W/2).
                void DrawSSAOPass(NkICommandBuffer* cmd, NkTextureHandle depthSrc,
                                  uint32 ssaoW, uint32 ssaoH);

                // Phase H.5b : blur cross-bilateral / gaussian apres GTAO pour
                // denoise le noise du random rotation per-pixel.
                void DrawSSAOBlurPass(NkICommandBuffer* cmd, NkTextureHandle aoSrc,
                                       uint32 ssaoW, uint32 ssaoH);
                NkTexHandle RunSSAO   (NkICommandBuffer* cmd, NkTexHandle depth, NkTexHandle normal);
                NkTexHandle RunBloom  (NkICommandBuffer* cmd, NkTexHandle hdr);
                NkTexHandle RunTonemap(NkICommandBuffer* cmd, NkTexHandle hdr);
                NkTexHandle RunFXAA   (NkICommandBuffer* cmd, NkTexHandle ldr);

                // Vrai si au moins un effet est actif dans la config — utilise par
                // BuildDefaultRenderGraph pour decider d'activer le path HDR transient.
                bool HasAnyEffect() const noexcept {
                    return mCfg.ssao || mCfg.bloom || mCfg.toneMapping || mCfg.fxaa || mCfg.aces;
                }

                // Phase L FXAA wirage : intermediate LDR target pour split
                // tonemap (→mToneTex) + FXAA (→swapchain).
                NkTexHandle GetToneTexHandle() const { return mToneTex; }
                bool        IsFXAAEnabled()    const { return mCfg.fxaa; }

                // ExecuteFXAA : pass dediee FXAA, lit ldrIn (mToneTex), draw fullscreen
                // sur le RT courant (swapchain). Appele par RenderGraph FXAA pass.
                void ExecuteFXAA(NkICommandBuffer* cmd, NkTextureHandle ldrIn);

            private:
                NkIDevice*        mDevice    = nullptr;
                NkTextureLibrary* mTex       = nullptr;
                NkMeshSystem*     mMesh      = nullptr;
                NkShaderLibrary*  mShaderLib = nullptr;
                NkResources*      mResources = nullptr;
                NkPostConfig      mCfg;
                uint32 mW=0,mH=0;
                NkTexHandle mSSAOTex, mToneTex, mFinalTex;

                // Phase L : Color grading 3D LUT. Default = identity (no
                // grading). User upload via SetColorGradingLUT(data, size).
                NkTextureHandle mLUTTex;
                uint32          mLUTSize = 16;  // sync avec mCfg.lutSize

                // Phase H.2 : bloom mipchain via NkRenderTarget (6 niveaux,
                // W/2..W/64, RGBA16F sans depth). Chaque mip a son render pass
                // pour permettre BeginRender/EndRender pendant RunBloom.
                static constexpr int kBloomMips = 6;
                NkRenderTarget mBloomRT[kBloomMips];

                // Phase H.3 : RT R8_UNORM pour le SSAO (W/2 x H/2). Sert de
                // template de render pass pour mPipeSSAO (compatible avec le
                // transient ssaoTex du RG).
                NkRenderTarget mSSAORT;

                NkPipelineHandle mPipeSSAO, mPipeTone, mPipeFXAA;
                // Phase H.5b : blur post-GTAO pour denoise.
                NkPipelineHandle mPipeSSAOBlur;
                // Phase H.2 : pipelines bloom multi-pass (downsample + upsample).
                NkPipelineHandle mPipeBloomDown;
                NkPipelineHandle mPipeBloomUp;

                // Shaders RHI (handle revoyes par NkShaderLibrary)
                ::nkentseu::NkShaderHandle mShaderTone;
                ::nkentseu::NkShaderHandle mShaderBloomDown;
                ::nkentseu::NkShaderHandle mShaderBloomUp;
                ::nkentseu::NkShaderHandle mShaderSSAO;
                ::nkentseu::NkShaderHandle mShaderSSAOBlur;
                ::nkentseu::NkShaderHandle mShaderFXAA;  // Phase L : FXAA 3.11-style

                // Descriptor set layout (1 sampler) + set alloue, refresh par Run*.
                // Utilise par ssao, fxaa, et l'ancien tonemap mono-input.
                NkDescSetHandle  mInputTexLayout;
                NkDescSetHandle  mInputTexSet;

                // Phase H.2 : pool de descriptor sets pour les sub-passes bloom.
                // Vulkan interdit d'updater un descriptor set pendant qu'un draw
                // precedent l'utilise (UB). Chaque sub-pass bloom (6 down + 5 up
                // = 11 max par frame) doit avoir son propre set. Avec triple-
                // buffering (3 frames in flight) il faut 11*3 = 33 sets pour
                // que le set qu'on reutilise apres modulo soit hors-usage GPU.
                static constexpr int kBloomDescSets = 33;
                NkDescSetHandle  mBloomSets[kBloomDescSets];
                int              mBloomSetCursor = 0;  // monotonic, modulo en runtime

                // Phase H.2 : descriptor set dedie au tonemap qui binde 2
                // textures simultanees (uHDR=0, uBloom=1). Layout separe car
                // les autres passes n'utilisent qu'un seul sampler.
                NkDescSetHandle  mToneLayout;
                NkDescSetHandle  mToneSet;

                NkMeshHandle     mQuad;

                void CreateTextures();
                void DrawFullscreen(NkICommandBuffer* cmd, NkPipelineHandle pipe,
                                    NkTexHandle src, const void* pushConst, uint32 pcSize);

                // Phase H.2 : draw fullscreen vers la mip courante d'un
                // mBloomRT[targetIdx], en samplant mBloomRT[srcIdx] (ou hdr
                // pour le premier downsample). BeginRender / EndRender autour.
                void BloomPass(NkICommandBuffer* cmd, NkPipelineHandle pipe,
                               NkTextureHandle src, int targetIdx,
                               const void* pushConst, uint32 pcSize);
        };
    }
} // namespace
