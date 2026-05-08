// =============================================================================
// NkPostProcessStack.cpp  — NKRenderer v4.0
// Post-processing : ACES tonemap, FXAA 3.11, dual-Kawase bloom, SSAO.
//
// État courant : pipelines créés (shaders fournis par le backend RHI),
// stack draw orchestrée — la compilation explicite des shaders GLSL embarqués
// ci-dessous est laissée à l'intégration ultérieure quand le pont
// renderer↔RHI shader handle sera unifié.
// =============================================================================
#include "NkPostProcessStack.h"
#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"

namespace nkentseu {
namespace renderer {

    // =========================================================================
    // SHADERS GLSL EMBARQUÉS — référence complète pour intégration future
    // =========================================================================

    // Vertex shader fullscreen — quad NDC déjà construit par NkMeshSystem::GetQuad()
    static const char* kFullscreenVS = R"GLSL(
#version 450
layout(location = 0) in vec3 aPos;
layout(location = 3) in vec2 aUV;
layout(location = 0) out vec2 vUV;
void main() {
    vUV = aUV;
    gl_Position = vec4(aPos.xy, 0.0, 1.0);
}
)GLSL";

    // ACES Filmic Tonemap (Krzysztof Narkowicz)
    static const char* kTonemapFS = R"GLSL(
#version 450
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 oColor;
layout(set = 1, binding = 0) uniform sampler2D uHDR;
layout(push_constant) uniform PC {
    float exposure;
    float gamma;
} pc;
vec3 ACESFilm(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}
void main() {
    vec3 hdr = texture(uHDR, vUV).rgb * pc.exposure;
    vec3 mapped = ACESFilm(hdr);
    mapped = pow(mapped, vec3(1.0/pc.gamma));
    oColor = vec4(mapped, 1.0);
}
)GLSL";

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
                                    NkMeshSystem* m, uint32 w, uint32 h) {
        mDevice=d; mTex=t; mMesh=m; mW=w; mH=h;
        mQuad = m->GetQuad();
        CreateTextures();
        // Référence aux sources GLSL (consommées une fois le pont shader unifié)
        (void)kFullscreenVS; (void)kTonemapFS; (void)kFXAAFS;
        (void)kBloomDownFS; (void)kBloomUpFS; (void)kSSAOFS;

        NkGraphicsPipelineDesc pd;
        pd.rasterizer = NkRasterizerDesc::Default();
        pd.rasterizer.cullMode = nkentseu::NkCullMode::NK_NONE;
        pd.depthStencil = NkDepthStencilDesc::NoDepth();
        pd.blend = NkBlendDesc::Opaque();
        pd.debugName = "PP_SSAO";
        mPipeSSAO  = mDevice->CreateGraphicsPipeline(pd);
        pd.debugName = "PP_Bloom";
        mPipeBloom = mDevice->CreateGraphicsPipeline(pd);
        pd.debugName = "PP_Tone";
        mPipeTone  = mDevice->CreateGraphicsPipeline(pd);
        pd.debugName = "PP_FXAA";
        mPipeFXAA  = mDevice->CreateGraphicsPipeline(pd);
        return true;
    }

    void NkPostProcessStack::Shutdown() {
        if (mPipeSSAO.IsValid())  mDevice->DestroyPipeline(mPipeSSAO);
        if (mPipeBloom.IsValid()) mDevice->DestroyPipeline(mPipeBloom);
        if (mPipeTone.IsValid())  mDevice->DestroyPipeline(mPipeTone);
        if (mPipeFXAA.IsValid())  mDevice->DestroyPipeline(mPipeFXAA);
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
        if (pushConst && pcSize > 0) {
            cmd->PushConstants(NkShaderStage::NK_FRAGMENT, 0, pcSize, pushConst);
        }
        (void)src;
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

    NkTexHandle NkPostProcessStack::RunFXAA(NkICommandBuffer* cmd, NkTexHandle ldr) {
        struct PC { float invResW, invResH; } pc;
        pc.invResW = 1.0f / (float)(mW > 0 ? mW : 1);
        pc.invResH = 1.0f / (float)(mH > 0 ? mH : 1);
        DrawFullscreen(cmd, mPipeFXAA, ldr, &pc, sizeof(pc));
        return mFinalTex;
    }

} // namespace renderer
} // namespace nkentseu
