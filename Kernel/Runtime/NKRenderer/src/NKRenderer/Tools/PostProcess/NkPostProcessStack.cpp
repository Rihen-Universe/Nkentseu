// =============================================================================
// NkPostProcessStack.cpp  — NKRenderer v5.0
// Post-processing : ACES tonemap (D.4b), FXAA 3.11, dual-Kawase bloom, SSAO.
//
// État courant D.4b : tonemap ACES wire bout-en-bout (shader compile via
// NkShaderLibrary, pipeline avec descriptor set, RunTonemap bind input HDR
// et drawe un fullscreen quad). Bloom/SSAO/FXAA pipelines existent mais leur
// shader n'est pas encore wire — ils tomberont a no-op tant que la config
// les active.
// =============================================================================
#include "NkPostProcessStack.h"
#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKRenderer/Core/NkResources.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include "NKRenderer/Shader/NkShaderLibrary.h"

#include "NKLogger/NkLog.h"

namespace nkentseu {
namespace renderer {

    // =========================================================================
    // SHADERS GLSL EMBARQUÉS — référence complète pour intégration future
    // =========================================================================

    // Vertex shader fullscreen OpenGL-natif. NkMeshSystem::GetQuad() fournit
    // un quad NDC dans [-1,1] avec UV attribut a location 3 (NkVertex3D layout).
    // Stride 56 : pos(12) normal(12) tangent(12) uv(8) uv2(8) color(4).
    static const char* kFullscreenVS_GL = R"GLSL(
#version 460 core
layout(location=0) in vec3 aPos;
layout(location=3) in vec2 aUV;
layout(location=0) out vec2 vUV;
void main() {
    vUV = aUV;
    gl_Position = vec4(aPos.xy, 0.0, 1.0);
}
)GLSL";

    // ACES Filmic Tonemap (Krzysztof Narkowicz). Push constants emules en GL via
    // uniform vec4 _PushConstants[N] (cf. convention NkOpenglCommandBuffer).
    // Layout PC = (exposure, gamma, _, _) sur 16 bytes => N=1.
    static const char* kTonemapFS_GL = R"GLSL(
#version 460 core
layout(location=0) in vec2 vUV;
layout(location=0) out vec4 oColor;
layout(binding=0) uniform sampler2D uHDR;
uniform vec4 _PushConstants[1];   // .x=exposure, .y=gamma
vec3 ACESFilm(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}
void main() {
    float exposure = _PushConstants[0].x;
    float gamma    = _PushConstants[0].y;
    vec3 hdr    = texture(uHDR, vUV).rgb * exposure;
    vec3 mapped = ACESFilm(hdr);
    mapped      = pow(mapped, vec3(1.0/gamma));
    oColor      = vec4(mapped, 1.0);
}
)GLSL";

    // Anciens shaders Vulkan-style (kept for reference, NkShaderLibrary les
    // ignore tant que les pipelines correspondants ne sont pas wires).
    static const char* kFullscreenVS [[maybe_unused]] = "/* legacy Vulkan VS, see kFullscreenVS_GL */";
    static const char* kTonemapFS    [[maybe_unused]] = "/* legacy Vulkan FS, see kTonemapFS_GL */";

    // FXAA 3.11 simplifié
    static const char* kFXAAFS = R"GLSL(
#version 450
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 oColor;
layout(set = 1, binding = 0) uniform sampler2D uLDR;
layout(push_constant) uniform PC { vec2 invResolution; } pc;
float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }
void main() {
    vec2 px = pc.invResolution;
    vec3 c   = texture(uLDR, vUV).rgb;
    vec3 cN  = texture(uLDR, vUV + vec2(0.0,  px.y)).rgb;
    vec3 cS  = texture(uLDR, vUV - vec2(0.0,  px.y)).rgb;
    vec3 cE  = texture(uLDR, vUV + vec2(px.x, 0.0)).rgb;
    vec3 cW  = texture(uLDR, vUV - vec2(px.x, 0.0)).rgb;
    float lC = luma(c),  lN = luma(cN), lS = luma(cS);
    float lE = luma(cE), lW = luma(cW);
    float lMin = min(lC, min(min(lN, lS), min(lE, lW)));
    float lMax = max(lC, max(max(lN, lS), max(lE, lW)));
    float range = lMax - lMin;
    if (range < max(0.0312, lMax * 0.125)) { oColor = vec4(c, 1.0); return; }
    vec2 dir;
    dir.x = -((lN + lS) - (lE + lW));
    dir.y =  ((lW + lE) - (lN + lS));
    float dirReduce = max((lN + lS + lE + lW)*0.25*0.5, 1.0/128.0);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = clamp(dir * rcpDirMin, vec2(-8.0), vec2(8.0)) * px;
    vec3 r1 = 0.5 * (
        texture(uLDR, vUV + dir*(1.0/3.0 - 0.5)).rgb +
        texture(uLDR, vUV + dir*(2.0/3.0 - 0.5)).rgb);
    vec3 r2 = r1*0.5 + 0.25 * (
        texture(uLDR, vUV + dir*-0.5).rgb +
        texture(uLDR, vUV + dir* 0.5).rgb);
    float lR2 = luma(r2);
    oColor = vec4((lR2 < lMin || lR2 > lMax) ? r1 : r2, 1.0);
}
)GLSL";

    static const char* kBloomDownFS = R"GLSL(
#version 450
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 oColor;
layout(set = 1, binding = 0) uniform sampler2D uSrc;
layout(push_constant) uniform PC { vec2 srcInvSize; float threshold; float strength; } pc;
void main() {
    vec2 t = pc.srcInvSize;
    vec3 a = texture(uSrc, vUV + t*vec2(-1.0, 1.0)).rgb;
    vec3 b = texture(uSrc, vUV + t*vec2( 0.0, 1.0)).rgb;
    vec3 c = texture(uSrc, vUV + t*vec2( 1.0, 1.0)).rgb;
    vec3 d = texture(uSrc, vUV + t*vec2(-0.5, 0.5)).rgb;
    vec3 e = texture(uSrc, vUV + t*vec2( 0.5, 0.5)).rgb;
    vec3 f = texture(uSrc, vUV + t*vec2(-1.0, 0.0)).rgb;
    vec3 g = texture(uSrc, vUV).rgb;
    vec3 h = texture(uSrc, vUV + t*vec2( 1.0, 0.0)).rgb;
    vec3 i = texture(uSrc, vUV + t*vec2(-0.5,-0.5)).rgb;
    vec3 j = texture(uSrc, vUV + t*vec2( 0.5,-0.5)).rgb;
    vec3 k = texture(uSrc, vUV + t*vec2(-1.0,-1.0)).rgb;
    vec3 l = texture(uSrc, vUV + t*vec2( 0.0,-1.0)).rgb;
    vec3 m = texture(uSrc, vUV + t*vec2( 1.0,-1.0)).rgb;
    vec3 col = (d + e + i + j) * 0.5
             + (a + b + g + f) * 0.125
             + (b + c + h + g) * 0.125
             + (f + g + l + k) * 0.125
             + (g + h + m + l) * 0.125;
    col *= 1.0 / 4.0;
    float br = max(col.r, max(col.g, col.b));
    float soft = max(0.0, br - pc.threshold);
    soft = soft*soft / (4.0*pc.threshold + 1e-4);
    float contrib = max(soft, br - pc.threshold) / max(br, 1e-4);
    oColor = vec4(col * contrib, 1.0);
}
)GLSL";

    static const char* kBloomUpFS = R"GLSL(
#version 450
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 oColor;
layout(set = 1, binding = 0) uniform sampler2D uSrc;
layout(push_constant) uniform PC { vec2 srcInvSize; float strength; } pc;
void main() {
    vec2 t = pc.srcInvSize;
    vec3 col =
        texture(uSrc, vUV + t*vec2(-1.0, 1.0)).rgb*1.0 +
        texture(uSrc, vUV + t*vec2( 0.0, 1.0)).rgb*2.0 +
        texture(uSrc, vUV + t*vec2( 1.0, 1.0)).rgb*1.0 +
        texture(uSrc, vUV + t*vec2(-1.0, 0.0)).rgb*2.0 +
        texture(uSrc, vUV).rgb                          *4.0 +
        texture(uSrc, vUV + t*vec2( 1.0, 0.0)).rgb*2.0 +
        texture(uSrc, vUV + t*vec2(-1.0,-1.0)).rgb*1.0 +
        texture(uSrc, vUV + t*vec2( 0.0,-1.0)).rgb*2.0 +
        texture(uSrc, vUV + t*vec2( 1.0,-1.0)).rgb*1.0;
    col *= 1.0/16.0;
    oColor = vec4(col * pc.strength, 1.0);
}
)GLSL";

    static const char* kSSAOFS = R"GLSL(
#version 450
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 oColor;
layout(set = 1, binding = 0) uniform sampler2D uDepth;
layout(push_constant) uniform PC { vec2 invResolution; float radius; float bias; } pc;
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
void main() {
    float depth = texture(uDepth, vUV).r;
    if (depth >= 0.9999) { oColor = vec4(1.0); return; }
    float occ = 0.0;
    const int SAMPLES = 16;
    for (int i = 0; i < SAMPLES; i++) {
        vec2 off = vec2(hash(vUV + float(i)), hash(vUV - float(i)))*2.0 - 1.0;
        off *= pc.radius * pc.invResolution * 50.0;
        float d = texture(uDepth, vUV + off).r;
        float diff = depth - d;
        if (diff > pc.bias && diff < pc.radius) occ += 1.0;
    }
    occ = 1.0 - (occ / float(SAMPLES));
    oColor = vec4(vec3(occ), 1.0);
}
)GLSL";

    bool NkPostProcessStack::Init(NkIDevice* d, NkTextureLibrary* t,
                                    NkMeshSystem* m, NkShaderLibrary* sl,
                                    NkResources* res, uint32 w, uint32 h) {
        mDevice=d; mTex=t; mMesh=m; mShaderLib=sl; mResources=res; mW=w; mH=h;
        mQuad = m->GetQuad();
        CreateTextures();

        // ── Descriptor set layout : 1 sampler combine pour la texture d'entree ──
        NkDescriptorSetLayoutDesc layout;
        layout.Add(0, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER,
                   ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS);
        mInputTexLayout = mDevice->CreateDescriptorSetLayout(layout);
        mInputTexSet    = mDevice->AllocateDescriptorSet(mInputTexLayout);

        // ── Vertex layout du fullscreen quad (NkVertex3D) ─────────────────────
        // Le shader VS n'utilise que aPos (loc 0) et aUV (loc 3). On declare
        // tous les attributs pour matcher le format du mesh quad sans avertissement.
        auto buildVertexLayout = [](NkGraphicsPipelineDesc& pd) {
            pd.vertexLayout
              .AddBinding(0, sizeof(NkVertex3D), false)
              .AddAttribute(0, 0, ::nkentseu::NkVertexFormat::NK_RGB32_FLOAT, 0,  "POSITION", 0)
              .AddAttribute(1, 0, ::nkentseu::NkVertexFormat::NK_RGB32_FLOAT, 12, "NORMAL",   0)
              .AddAttribute(2, 0, ::nkentseu::NkVertexFormat::NK_RGB32_FLOAT, 24, "TANGENT",  0)
              .AddAttribute(3, 0, ::nkentseu::NkVertexFormat::NK_RG32_FLOAT,  36, "TEXCOORD", 0)
              .AddAttribute(4, 0, ::nkentseu::NkVertexFormat::NK_RG32_FLOAT,  44, "TEXCOORD", 1)
              .AddAttribute(5, 0, ::nkentseu::NkVertexFormat::NK_RGBA8_UNORM, 52, "COLOR",    0);
        };

        // ── Pipeline tonemap ─────────────────────────────────────────────────
        // LoadOrCompileVF : permet a l'utilisateur de fournir son propre tonemap
        // (ex : Reinhard, Uncharted2) en deposant un fichier dans
        // Resources/NKRenderer/Shaders/PP_Tonemap/GL/ -- fallback ACES embedded.
        if (mShaderLib) {
            auto progHandle = mShaderLib->LoadOrCompileVF("PP_Tonemap", kFullscreenVS_GL, kTonemapFS_GL);
            if (progHandle.IsValid()) {
                mShaderTone = mShaderLib->GetRHIHandle(progHandle);
            }
        }
        if (mShaderTone.IsValid()) {
            NkGraphicsPipelineDesc pd;
            pd.shader       = mShaderTone;
            pd.depthStencil = NkDepthStencilDesc::NoDepth();
            pd.rasterizer   = NkRasterizerDesc::NoCull();
            pd.blend        = NkBlendDesc::Opaque();
            pd.debugName    = "PP_Tone";
            pd.AddPushConstant(::nkentseu::NkShaderStage::NK_FRAGMENT, 0, 16);  // (exposure, gamma, _, _)
            if (mInputTexLayout.IsValid()) pd.descriptorSetLayouts.PushBack(mInputTexLayout);
            buildVertexLayout(pd);
            mPipeTone = mDevice->CreateGraphicsPipeline(pd);
        }

        // ── Pipelines bloom/ssao/fxaa : pas encore wires (shaders pas compiles) ─
        // Ces pipelines restent invalides tant que mCfg.bloom/ssao/fxaa active des
        // effets non implementes. RunBloom/RunSSAO/RunFXAA tomberont en no-op
        // (DrawFullscreen verifie pipe.IsValid()).
        (void)kFullscreenVS; (void)kTonemapFS;
        (void)kFXAAFS; (void)kBloomDownFS; (void)kBloomUpFS; (void)kSSAOFS;

        return mPipeTone.IsValid();
    }

    void NkPostProcessStack::Shutdown() {
        if (mPipeSSAO.IsValid())     mDevice->DestroyPipeline(mPipeSSAO);
        if (mPipeBloom.IsValid())    mDevice->DestroyPipeline(mPipeBloom);
        if (mPipeTone.IsValid())     mDevice->DestroyPipeline(mPipeTone);
        if (mPipeFXAA.IsValid())     mDevice->DestroyPipeline(mPipeFXAA);
        if (mInputTexSet.IsValid())    mDevice->FreeDescriptorSet(mInputTexSet);
        if (mInputTexLayout.IsValid()) mDevice->DestroyDescriptorSetLayout(mInputTexLayout);
        // Le shader handle est detenu par NkShaderLibrary, pas a detruire ici.
        mShaderTone = {};
    }

    void NkPostProcessStack::OnResize(uint32 w, uint32 h) {
        mW=w; mH=h; CreateTextures();
    }

    void NkPostProcessStack::CreateTextures() {
        if (!mTex||!mDevice) return;
        uint32 hw = mW/2 ? mW/2 : 1;
        uint32 hh = mH/2 ? mH/2 : 1;
        mSSAOTex = mTex->CreateRenderTarget(hw,hh,NkGPUFormat::NK_R8_UNORM,false,true,"SSAO");
        for(int i=0;i<6;i++){
            uint32 s=1<<i;
            uint32 bw = mW/s ? mW/s : 1;
            uint32 bh = mH/s ? mH/s : 1;
            mBloomTex[i]=mTex->CreateRenderTarget(bw,bh,NkGPUFormat::NK_RGBA16_FLOAT,false,true,"Bloom");
        }
        mToneTex  = mTex->CreateRenderTarget(mW,mH,NkGPUFormat::NK_RGBA8_UNORM,false,true,"Tone");
        mFinalTex = mTex->CreateRenderTarget(mW,mH,NkGPUFormat::NK_RGBA8_UNORM,false,true,"Final");
    }

    NkTexHandle NkPostProcessStack::Execute(NkICommandBuffer* cmd,
                                              NkTexHandle hdrIn, NkTexHandle depth,
                                              NkTexHandle velocity) {
        (void)velocity;
        NkTexHandle cur = hdrIn;
        if (mCfg.ssao)          cur = RunSSAO(cmd, depth, NkTexHandle::Null());
        if (mCfg.bloom)         cur = RunBloom(cmd, cur);
        if (mCfg.toneMapping)   cur = RunTonemap(cmd, cur);
        if (mCfg.fxaa)          cur = RunFXAA(cmd, cur);
        return cur;
    }

    // ── Helper : draw fullscreen quad sampling 'src' into current target ─────
    void NkPostProcessStack::DrawFullscreen(NkICommandBuffer* cmd,
                                            NkPipelineHandle pipe,
                                            NkTexHandle src,
                                            const void* pushConst, uint32 pcSize) {
        if (!cmd || !pipe.IsValid() || !mMesh) return;
        cmd->BindGraphicsPipeline(pipe);

        // Bind l'input texture au descriptor set (binding 0). On utilise le
        // sampler linear-clamp de NkResources (typique pour les passes PP).
        if (mInputTexSet.IsValid() && src.IsValid() && mTex && mResources) {
            NkTextureHandle rhi  = mTex->GetRHIHandle(src);
            NkSamplerHandle samp = mResources->GetSamplerLinearClamp();
            if (rhi.IsValid() && samp.IsValid()) {
                mDevice->BindTextureSampler(mInputTexSet, 0, rhi, samp);
                cmd->BindDescriptorSet(mInputTexSet, 0);
            }
        }

        if (pushConst && pcSize > 0) {
            // PP shaders : push_constant uniquement utilise par le FS (le VS
            // fullscreen quad n'a pas de PC). Pusher avec ALL_GRAPHICS declenche
            // VUID-vkCmdPushConstants-offset-01795 (range pipeline = FRAGMENT
            // strict). On reste donc en NK_FRAGMENT pour matcher.
            cmd->PushConstants(::nkentseu::NkShaderStage::NK_FRAGMENT, 0, pcSize, pushConst);
        }
        mMesh->BindMesh(cmd, mQuad);
        mMesh->DrawAll(cmd, mQuad);
    }

    NkTexHandle NkPostProcessStack::RunSSAO(NkICommandBuffer* cmd,
                                              NkTexHandle depth, NkTexHandle normal) {
        struct PC { float invResW, invResH, radius, bias; } pc;
        pc.invResW = 1.0f / (float)(mW > 0 ? mW : 1);
        pc.invResH = 1.0f / (float)(mH > 0 ? mH : 1);
        pc.radius  = mCfg.ssaoRadius;
        pc.bias    = mCfg.ssaoBias;
        DrawFullscreen(cmd, mPipeSSAO, depth, &pc, sizeof(pc));
        (void)normal;
        return mSSAOTex;
    }

    NkTexHandle NkPostProcessStack::RunBloom(NkICommandBuffer* cmd, NkTexHandle hdr) {
        struct PC { float invW, invH, threshold, strength; } pc;
        pc.invW      = 1.0f / (float)(mW > 0 ? mW : 1);
        pc.invH      = 1.0f / (float)(mH > 0 ? mH : 1);
        pc.threshold = mCfg.bloomThreshold;
        pc.strength  = mCfg.bloomStrength;
        DrawFullscreen(cmd, mPipeBloom, hdr, &pc, sizeof(pc));
        return mBloomTex[0];
    }

    NkTexHandle NkPostProcessStack::RunTonemap(NkICommandBuffer* cmd, NkTexHandle hdr) {
        struct PC { float exposure, gamma; } pc;
        pc.exposure = mCfg.exposure;
        pc.gamma    = mCfg.gamma;
        DrawFullscreen(cmd, mPipeTone, hdr, &pc, sizeof(pc));
        return mToneTex;
    }

    void NkPostProcessStack::ExecuteRHI(NkICommandBuffer* cmd, NkTextureHandle hdrIn) {
        // Variante directe pour le RenderGraph : on bind le hdrIn (RHI handle)
        // sur l'input descriptor set et on dessine le tonemap fullscreen vers le
        // FBO courant (typiquement le swapchain). Pour l'instant, seul le tonemap
        // est wire — bloom/SSAO/FXAA seront ajoutes incrementalement.
        static int sLogCount = 0;
        if (sLogCount < 3) {
            sLogCount++;
            logger.Info("[PP::ExecuteRHI] cmd={0} pipe.valid={1} set.valid={2} hdr.valid={3} mesh={4} quad.valid={5}\n",
                        (cmd!=nullptr), mPipeTone.IsValid(), mInputTexSet.IsValid(), hdrIn.IsValid(),
                        (mMesh!=nullptr), mQuad.IsValid());
        }
        if (!cmd || !mPipeTone.IsValid() || !mInputTexSet.IsValid() || !hdrIn.IsValid()) return;

        NkSamplerHandle samp = mResources ? mResources->GetSamplerLinearClamp() : NkSamplerHandle{};
        if (!samp.IsValid()) return;

        // Bind input HDR + push constants (exposure, gamma) + draw fullscreen
        mDevice->BindTextureSampler(mInputTexSet, 0, hdrIn, samp);
        cmd->BindGraphicsPipeline(mPipeTone);
        cmd->BindDescriptorSet(mInputTexSet, 0);

        // Layout PC : (exposure, gamma, _, _) sur 16 bytes (1 vec4 push slot).
        // Stage = NK_FRAGMENT pour matcher le range pipeline (le VS fullscreen
        // n'a pas de PC) : sinon VUID-vkCmdPushConstants-offset-01795.
        float32 pc[4] = { mCfg.exposure, mCfg.gamma, 0.f, 0.f };
        cmd->PushConstants(::nkentseu::NkShaderStage::NK_FRAGMENT, 0, sizeof(pc), pc);

        if (mMesh) {
            mMesh->BindMesh(cmd, mQuad);
            mMesh->DrawAll(cmd, mQuad);
        }
    }

    NkTexHandle NkPostProcessStack::RunFXAA(NkICommandBuffer* cmd, NkTexHandle ldr) {
        struct PC { float invResW, invResH; } pc;
        pc.invResW = 1.0f / (float)(mW > 0 ? mW : 1);
        pc.invResH = 1.0f / (float)(mH > 0 ? mH : 1);
        DrawFullscreen(cmd, mPipeFXAA, ldr, &pc, sizeof(pc));
        return mFinalTex;
    }

} // namespace renderer
} // namespace nkentseu
