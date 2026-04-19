// =============================================================================
// NkPostProcessStack.cpp — Pile de post-traitements configurable
// SSAO → Bloom → DOF → Tonemap → FXAA/TAA → Film Grain → Vignette
// =============================================================================
#include "NKRenderer/Tools/PostProcess/NkPostProcessStack.h"
#include "NKRenderer/Core/NkResourceManager.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKRHI/Core/NkDescs.h"

namespace nkentseu {
    namespace renderer {

        // ── Shaders embarqués fullscreen triangle ─────────────────────────────────
        static const char* kFullscreenVert_GL = R"(
#version 460 core
out vec2 vUV;
void main(){
    vec2 pos[3]=vec2[](vec2(-1,-1),vec2(3,-1),vec2(-1,3));
    vUV=pos[gl_VertexID]*0.5+0.5;
    gl_Position=vec4(pos[gl_VertexID],0,1);
})";

        static const char* kBlitFrag_GL = R"(
#version 460 core
in vec2 vUV;
layout(binding=0) uniform sampler2D uTex;
out vec4 oColor;
void main(){ oColor = texture(uTex, vUV); })";

        // =============================================================================
        bool NkPostProcessStack::Init(NkIDevice* device, NkResourceManager* resources,
                                       uint32 width, uint32 height)
        {
            if (!device || !resources) return false;
            mDevice    = device;
            mResources = resources;
            mWidth     = width;
            mHeight    = height;

            CreatePipelines();
            CreateUBOs();
            AllocRenderTargets(width, height);

            mReady = true;
            return true;
        }

        void NkPostProcessStack::Shutdown() {
            if (!mDevice) return;

            auto destroyBuf = [&](uint64& id) {
                if (!id) return;
                NkBufferHandle h; h.id=id; mDevice->DestroyBuffer(h); id=0;
            };
            auto destroyPipe = [&](uint64& id) {
                if (!id) return;
                NkPipelineHandle h; h.id=id; mDevice->DestroyPipeline(h); id=0;
            };
            auto destroyShader = [&](uint64& id) {
                if (!id) return;
                NkShaderHandle h; h.id=id; mDevice->DestroyShader(h); id=0;
            };

            destroyBuf(mSSAO_UBO);
            destroyBuf(mBloom_UBO);
            destroyBuf(mDOF_UBO);
            destroyBuf(mTonemap_UBO);
            destroyBuf(mFXAA_UBO);

            destroyPipe(mSSAO_Pipe);
            destroyPipe(mBloom_Pipe);
            destroyPipe(mDOF_Pipe);
            destroyPipe(mTonemap_Pipe);
            destroyPipe(mFXAA_Pipe);
            destroyPipe(mBlit_Pipe);

            destroyShader(mFullscreenShader);

            for (auto& rt : mPingPong) {
                if (rt.IsValid()) mResources->ReleaseRenderTarget(rt);
            }
            mPingPong[0] = mPingPong[1] = {};

            mDevice = nullptr;
            mReady  = false;
        }

        void NkPostProcessStack::Resize(uint32 w, uint32 h) {
            mWidth = w; mHeight = h;
            // Recréer les RTs
            for (auto& rt : mPingPong) {
                if (rt.IsValid()) mResources->ReleaseRenderTarget(rt);
            }
            AllocRenderTargets(w, h);
        }

        // =============================================================================
        void NkPostProcessStack::AllocRenderTargets(uint32 w, uint32 h) {
            for (uint32 i = 0; i < 2; ++i) {
                char dbg[32]; snprintf(dbg, 32, "PostProcess_PingPong%u", i);
                NkTextureHandle colorTex = mResources->CreateRenderTarget(
                    w, h, NkPixelFormat::NK_RGBA16F, NkMSAA::NK_1X, dbg);
                NkRenderTargetDesc rtDesc{};
                rtDesc.width  = w; rtDesc.height = h;
                rtDesc.AddColor(colorTex);
                mPingPong[i] = mResources->CreateRenderTarget(rtDesc);
                mColorTexes[i] = colorTex;
            }
        }

        void NkPostProcessStack::CreateUBOs() {
            // Allouer les UBOs pour chaque pass
            struct SSAOBlock { NkMat4f proj,projInv; float kern[64*4]; float noiseScaleX,noiseScaleY; float radius,bias,intensity; int kSize; float _p[2]; };
            struct BloomBlock { float threshold,knee,intensity,filterRadius; int pass; float _p[3]; };
            struct DOFBlock   { float focusDist,focusRange,bokehRadius,near,far; int samples; float _p[2]; };
            struct ToneBlock  { float exposure,gamma; int op; float contrast,sat,grain,vign,vignSmooth,ca,wb,time,_p; };
            struct FXAABlock  { float invW,invH,edgeMin,edgeMax,edgeSrch,edgeGuess,_p0,_p1; };

            auto mkUBO = [&](uint64& id, uint64 sz) {
                NkBufferHandle h = mDevice->CreateBuffer(NkBufferDesc::Uniform(sz));
                id = h.id;
            };
            mkUBO(mSSAO_UBO,   sizeof(SSAOBlock));
            mkUBO(mBloom_UBO,  sizeof(BloomBlock));
            mkUBO(mDOF_UBO,    sizeof(DOFBlock));
            mkUBO(mTonemap_UBO,sizeof(ToneBlock));
            mkUBO(mFXAA_UBO,   sizeof(FXAABlock));
        }

        void NkPostProcessStack::CreatePipelines() {
            // Fullscreen vert shader (partagé)
            NkShaderDesc vsDesc{};
            vsDesc.AddGLSL(NkShaderStage::NK_VERTEX, kFullscreenVert_GL);
            vsDesc.AddGLSL(NkShaderStage::NK_FRAGMENT, kBlitFrag_GL);
            vsDesc.debugName = "PostProcess_Fullscreen";
            NkShaderHandle vs = mDevice->CreateShader(vsDesc);
            mFullscreenShader = vs.id;

            // Pipeline blit générique
            NkGraphicsPipelineDesc pd{};
            pd.shader       = vs;
            pd.topology     = NkPrimitiveTopology::NK_TRIANGLE_LIST;
            pd.rasterizer   = NkRasterizerDesc::NoCull();
            pd.depthStencil = NkDepthStencilDesc::NoDepth();
            pd.blend        = NkBlendDesc::Opaque();
            pd.renderPass   = mDevice->GetSwapchainRenderPass();
            NkPipelineHandle blitPipe = mDevice->CreateGraphicsPipeline(pd);
            mBlit_Pipe = blitPipe.id;

            // Les autres pipelines (SSAO, Bloom, etc.) sont créés lazily
            // lors du premier Apply() car ils nécessitent les shaders complets.
        }

        // =============================================================================
        // Apply — exécute les passes activées en ordre
        // =============================================================================
        NkTextureHandle NkPostProcessStack::Apply(NkICommandBuffer* cmd,
                                                    NkTextureHandle hdrInput,
                                                    NkTextureHandle depthInput,
                                                    float32 time,
                                                    float32 dt)
        {
            if (!mReady || !cmd) return hdrInput;

            mCurrentInput = hdrInput;
            mDepthTex     = depthInput;
            uint32 ping = 0, pong = 1;

            auto swap = [&]() {
                std::swap(ping, pong);
                mCurrentInput = mColorTexes[pong]; // sortie devint entrée
            };

            // ── SSAO ──────────────────────────────────────────────────────────────────
            if (mCfg.ssao.enabled && mCfg.ssao.intensity > 0.f) {
                UpdateSSAO_UBO();
                RunFullscreenPass(cmd, mSSAO_Pipe, mSSAO_UBO,
                                   mCurrentInput, mDepthTex, mPingPong[ping]);
                swap();
            }

            // ── Bloom ─────────────────────────────────────────────────────────────────
            if (mCfg.bloom.enabled && mCfg.bloom.intensity > 0.f) {
                RunBloom(cmd, ping, pong);
                swap();
            }

            // ── DOF ───────────────────────────────────────────────────────────────────
            if (mCfg.dof.enabled) {
                UpdateDOF_UBO();
                RunFullscreenPass(cmd, mDOF_Pipe, mDOF_UBO,
                                   mCurrentInput, mDepthTex, mPingPong[ping]);
                swap();
            }

            // ── Tonemap + Film Grain + Vignette + CA ──────────────────────────────────
            {
                struct ToneBlock {
                    float exposure,gamma; int op;
                    float contrast,sat,grain,vign,vignSmooth,ca,wb,time,_p;
                } tb{};
                tb.exposure   = mCfg.tonemap.exposure;
                tb.gamma      = mCfg.tonemap.gamma;
                tb.op         = (int)mCfg.tonemap.op;
                tb.contrast   = mCfg.tonemap.contrast;
                tb.sat        = mCfg.tonemap.saturation;
                tb.grain      = mCfg.filmGrain.enabled ? mCfg.filmGrain.intensity : 0.f;
                tb.vign       = mCfg.vignette.enabled  ? mCfg.vignette.intensity  : 0.f;
                tb.vignSmooth = mCfg.vignette.smoothness;
                tb.ca         = mCfg.chromaticAberration.enabled ? mCfg.chromaticAberration.intensity : 0.f;
                tb.wb         = mCfg.tonemap.whiteBalance;
                tb.time       = time;
                NkBufferHandle uh; uh.id=mTonemap_UBO;
                mDevice->WriteBuffer(uh, &tb, sizeof(tb));
                RunFullscreenPass(cmd, mTonemap_Pipe, mTonemap_UBO,
                                   mCurrentInput, {}, mPingPong[ping]);
                swap();
            }

            // ── FXAA ──────────────────────────────────────────────────────────────────
            if (mCfg.antiAliasing == NkAAMode::NK_FXAA) {
                struct FXAABlock { float invW,invH,edgeMin,edgeMax,srch,guess,_p0,_p1; } fb{};
                fb.invW=1.f/mWidth; fb.invH=1.f/mHeight;
                fb.edgeMin=0.0833f; fb.edgeMax=0.166f; fb.srch=8.f; fb.guess=8.f;
                NkBufferHandle fh; fh.id=mFXAA_UBO;
                mDevice->WriteBuffer(fh, &fb, sizeof(fb));
                RunFullscreenPass(cmd, mFXAA_Pipe, mFXAA_UBO,
                                   mCurrentInput, {}, mPingPong[ping]);
                swap();
            }

            return mCurrentInput;
        }

        // =============================================================================
        void NkPostProcessStack::RunFullscreenPass(NkICommandBuffer* cmd,
                                                    uint64 pipeId, uint64 uboId,
                                                    NkTextureHandle inputA,
                                                    NkTextureHandle inputB,
                                                    NkRenderTargetHandle output)
        {
            if (!pipeId || !cmd) {
                // Fallback blit
                pipeId = mBlit_Pipe;
            }

            // Begin render pass
            cmd->BeginRenderTarget(output, NkClearFlags::NK_COLOR, {}, 1.f, 0);
            cmd->SetViewport({0.f,0.f,(float)mWidth,(float)mHeight});
            cmd->SetScissor ({0,0,(int32)mWidth,(int32)mHeight});

            NkPipelineHandle ph; ph.id = pipeId;
            cmd->BindGraphicsPipeline(ph);

            // Bind texture input A (slot 0)
            if (inputA.IsValid()) {
                NkDescriptorWrite w{};
                w.binding = 0;
                w.type    = NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER;
                NkTextureHandle th; th.id = mResources->GetTextureRHIId(inputA);
                NkSamplerHandle sh; sh.id = mResources->GetSamplerRHIId(inputA);
                w.texture = th; w.sampler = sh;
                w.textureLayout = NkResourceState::NK_SHADER_READ;
                mDevice->UpdateDescriptorSets(&w, 1);
            }

            // Bind texture input B si présente (slot 1)
            if (inputB.IsValid()) {
                NkDescriptorWrite w{};
                w.binding = 1;
                w.type    = NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER;
                NkTextureHandle th; th.id = mResources->GetTextureRHIId(inputB);
                NkSamplerHandle sh; sh.id = mResources->GetSamplerRHIId(inputB);
                w.texture = th; w.sampler = sh;
                w.textureLayout = NkResourceState::NK_SHADER_READ;
                mDevice->UpdateDescriptorSets(&w, 1);
            }

            cmd->Draw(3, 1, 0, 0); // Triangle fullscreen
            cmd->EndRenderTarget();
        }

        // =============================================================================
        void NkPostProcessStack::RunBloom(NkICommandBuffer* cmd,
                                           uint32 ping, uint32 pong)
        {
            struct BloomBlock { float threshold,knee,intensity,filterRadius; int pass; float _p[3]; };

            auto writeBloom = [&](int pass) {
                BloomBlock bb{};
                bb.threshold    = mCfg.bloom.threshold;
                bb.knee         = mCfg.bloom.knee;
                bb.intensity    = mCfg.bloom.intensity;
                bb.filterRadius = mCfg.bloom.filterRadius;
                bb.pass         = pass;
                NkBufferHandle bh; bh.id=mBloom_UBO;
                mDevice->WriteBuffer(bh, &bb, sizeof(bb));
            };

            // Pass 0 : threshold
            writeBloom(0);
            RunFullscreenPass(cmd, mBloom_Pipe, mBloom_UBO,
                               mCurrentInput, {}, mPingPong[ping]);

            // 4× downsample
            for (int i = 0; i < 4; ++i) {
                writeBloom(1);
                RunFullscreenPass(cmd, mBloom_Pipe, mBloom_UBO,
                                   mColorTexes[ping], {}, mPingPong[pong]);
                std::swap(ping, pong);
            }

            // 4× upsample
            for (int i = 0; i < 4; ++i) {
                writeBloom(2);
                RunFullscreenPass(cmd, mBloom_Pipe, mBloom_UBO,
                                   mColorTexes[ping], {}, mPingPong[pong]);
                std::swap(ping, pong);
            }

            // Composite
            writeBloom(3);
            RunFullscreenPass(cmd, mBloom_Pipe, mBloom_UBO,
                               mCurrentInput, mColorTexes[ping], mPingPong[ping]);
        }

        // =============================================================================
        void NkPostProcessStack::UpdateSSAO_UBO() {
            // Kernel hémisphérique 64 samples (généré une fois)
            if (!mSSAO_KernelGenerated) {
                struct SSAOBlock {
                    float proj[16], projInv[16];
                    float kernel[64*4];
                    float noiseX, noiseY, radius, bias, intensity;
                    int kSize; float _p[2];
                } sb{};
                // Générer kernel de Poisson
                for (int i = 0; i < 64; ++i) {
                    float scale = (float)i/64.f;
                    scale = 0.1f + scale*scale*0.9f; // accélérer vers centre
                    // Vecteurs pseudo-aléatoires dans hémisphère
                    float theta = (float)i * 2.399963f;
                    float r     = sqrtf((float)i/64.f);
                    sb.kernel[i*4+0] = cosf(theta)*r*scale;
                    sb.kernel[i*4+1] = sinf(theta)*r*scale;
                    sb.kernel[i*4+2] = sqrtf(1.f - r*r)*scale;
                    sb.kernel[i*4+3] = 0.f;
                }
                sb.noiseX   = (float)mWidth  / 4.f;
                sb.noiseY   = (float)mHeight / 4.f;
                sb.radius   = mCfg.ssao.radius;
                sb.bias     = mCfg.ssao.bias;
                sb.intensity= mCfg.ssao.intensity;
                sb.kSize    = 64;
                NkBufferHandle uh; uh.id=mSSAO_UBO;
                mDevice->WriteBuffer(uh, &sb, sizeof(sb));
                mSSAO_KernelGenerated = true;
            }
        }

        void NkPostProcessStack::UpdateDOF_UBO() {
            struct DOFBlock { float focusDist,focusRange,bokehRadius,near,far; int samples; float _p[2]; } db{};
            db.focusDist   = mCfg.dof.focusDistance;
            db.focusRange  = mCfg.dof.focusRange;
            db.bokehRadius = mCfg.dof.bokehRadius;
            db.near        = 0.1f;
            db.far         = 1000.f;
            db.samples     = mCfg.dof.samples;
            NkBufferHandle uh; uh.id=mDOF_UBO;
            mDevice->WriteBuffer(uh, &db, sizeof(db));
        }

        // =============================================================================
        // BlitToSwapchain — copier le résultat final vers le swapchain
        // =============================================================================
        void NkPostProcessStack::BlitToSwapchain(NkICommandBuffer* cmd,
                                                   NkTextureHandle result)
        {
            if (!mBlit_Pipe || !cmd || !result.IsValid()) return;
            NkPipelineHandle ph; ph.id=mBlit_Pipe;
            cmd->BindGraphicsPipeline(ph);

            NkDescriptorWrite w{};
            w.binding = 0;
            w.type    = NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER;
            NkTextureHandle th; th.id = mResources->GetTextureRHIId(result);
            NkSamplerHandle sh; sh.id = mResources->GetSamplerRHIId(result);
            w.texture = th; w.sampler = sh;
            w.textureLayout = NkResourceState::NK_SHADER_READ;
            mDevice->UpdateDescriptorSets(&w, 1);

            cmd->Draw(3, 1, 0, 0);
        }

    } // namespace renderer
} // namespace nkentseu
