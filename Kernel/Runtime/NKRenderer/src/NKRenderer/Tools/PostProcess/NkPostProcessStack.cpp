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
    // Layout PC = (exposure, gamma, vignetteIntens, saturation) sur 16 bytes => N=1.
    static const char* kTonemapFS_GL = R"GLSL(
#version 460 core
layout(location=0) in vec2 vUV;
layout(location=0) out vec4 oColor;
layout(binding=0) uniform sampler2D uHDR;
// PC[0] = (exposure, gamma, vignetteIntens, saturation)
// PC[1] = (bloomStrength, bloomThreshold, invWidth, invHeight)
uniform vec4 _PushConstants[2];
vec3 ACESFilm(vec3 x) {
    const float a=2.51,b=0.03,c=2.43,d=0.59,e=0.14;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e),0.0,1.0);
}
void main() {
    float exposure      = _PushConstants[0].x;
    float gamma         = _PushConstants[0].y;
    float vignetteIntens= _PushConstants[0].z;
    float saturation    = _PushConstants[0].w;
    float bloomStr      = _PushConstants[1].x;
    float bloomThr      = _PushConstants[1].y;
    float invW          = _PushConstants[1].z;
    float invH          = _PushConstants[1].w;
    vec3 hdr = texture(uHDR, vUV).rgb * exposure;
    // Bloom inline : 13-sample cross pattern sur 3 rayons (2, 8, 20 px).
    // Extrait les pixels brillants (> bloomThr) et les ajoute a hdr AVANT tonemap
    // pour que le tonemapping compresse correctement la contribution bloom.
    if (bloomStr > 0.001 && invW > 0.0 && invH > 0.0) {
        vec2 d = vec2(invW, invH);
        vec3 bloom = max(texture(uHDR, vUV).rgb - bloomThr, 0.0) * 4.0;
        float radii[3]   = float[](2.0,  8.0, 20.0);
        float weights[3] = float[](2.0,  1.0,  0.5);
        for (int r = 0; r < 3; r++) {
            vec2 off = radii[r] * d;
            bloom += max(texture(uHDR, vUV + vec2( off.x,  0.0)).rgb - bloomThr, 0.0) * weights[r];
            bloom += max(texture(uHDR, vUV + vec2(-off.x,  0.0)).rgb - bloomThr, 0.0) * weights[r];
            bloom += max(texture(uHDR, vUV + vec2( 0.0,  off.y)).rgb - bloomThr, 0.0) * weights[r];
            bloom += max(texture(uHDR, vUV + vec2( 0.0, -off.y)).rgb - bloomThr, 0.0) * weights[r];
        }
        bloom /= 18.0;  // total weight: 4 + 3*(4*weights[r]) = 4+8+4+2 = 18
        hdr += bloom * bloomStr;
    }
    vec3 mapped = ACESFilm(hdr);
    if (gamma > 1.01) mapped = pow(mapped, vec3(1.0/gamma));
    if (abs(saturation-1.0) > 0.01) {
        float lum = dot(mapped, vec3(0.2126,0.7152,0.0722));
        mapped = clamp(mix(vec3(lum), mapped, saturation), 0.0, 1.0);
    }
    if (vignetteIntens > 0.001) {
        vec2 uv = vUV * 2.0 - 1.0;
        mapped *= clamp(1.0 - dot(uv,uv) * vignetteIntens, 0.0, 1.0);
    }
    oColor = vec4(mapped, 1.0);
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
        // Utilise par bloom_down, bloom_up, ssao, fxaa.
        NkDescriptorSetLayoutDesc layout;
        layout.Add(0, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER,
                   ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS);
        mInputTexLayout = mDevice->CreateDescriptorSetLayout(layout);
        mInputTexSet    = mDevice->AllocateDescriptorSet(mInputTexLayout);

        // Phase H.2 : alloue le pool de descriptor sets pour bloom multi-pass.
        // 11 sub-passes par frame (6 down + 5 up), on alloue 16 pour marge.
        for (int i = 0; i < kBloomDescSets; i++) {
            mBloomSets[i] = mDevice->AllocateDescriptorSet(mInputTexLayout);
        }
        mBloomSetCursor = 0;

        // ── Phase H.2/H.3/L : layout du tonemap (4 samplers : uHDR + uBloom + uSSAO + uColorLUT) ──
        //   - binding 0 = uHDR     (HDR transient = sortie geometry pass)
        //   - binding 1 = uBloom   (mBloomRT[0].GetColorHandle() apres RunBloom)
        //   - binding 2 = uSSAO    (Phase H.3 : ambient occlusion factor)
        //   - binding 3 = uColorLUT (Phase L : 3D LUT cinema grading)
        NkDescriptorSetLayoutDesc tonelay;
        tonelay.Add(0, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER,
                    ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS);
        tonelay.Add(1, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER,
                    ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS);
        tonelay.Add(2, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER,
                    ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS);
        tonelay.Add(3, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER,
                    ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS);
        mToneLayout = mDevice->CreateDescriptorSetLayout(tonelay);
        mToneSet    = mDevice->AllocateDescriptorSet(mToneLayout);

        // Phase L : create identity LUT 16^3 par defaut (no color change).
        // User upload son LUT custom via NkRenderer::SetColorGradingLUT plus tard.
        // Format : RGBA8 UNORM, sampler linear-clamp (filter trilinear sur le 3D).
        //
        // TEMPORAIREMENT skip sur OpenGL backend : probable bug conversion
        // SPIRV-Cross sampler3D->GL ou binding 3D texture incorrect. Crash
        // observe 2026-05-23 sur Demo3D --bgl. Fix V1 = audit conversion +
        // GL backend 3D texture path. Sur Vulkan ca marche.
        const bool isGL = mDevice
                       && mDevice->GetApi() == NkGraphicsApi::NK_GFX_API_OPENGL;
        if (!isGL) {
            mLUTSize = mCfg.lutSize > 0 ? mCfg.lutSize : 16;
            if (mLUTSize > 64) mLUTSize = 64;  // hard cap pour eviter VRAM
            auto td = NkTextureDesc::Tex3D(mLUTSize, mLUTSize, mLUTSize,
                                            NkGPUFormat::NK_RGBA8_UNORM);
            td.debugName = "ColorGradingLUT_Identity";
            mLUTTex = mDevice->CreateTexture(td);

            // Identity LUT data : pixel (r,g,b) = (r/N-1, g/N-1, b/N-1).
            // Layout 3D : voxel (i,j,k) -> color (i, j, k) / (N-1).
            const uint32 N = mLUTSize;
            NkVector<uint8> data;
            data.Resize(N * N * N * 4);
            for (uint32 k = 0; k < N; k++)
                for (uint32 j = 0; j < N; j++)
                    for (uint32 i = 0; i < N; i++) {
                        uint32 idx = ((k * N + j) * N + i) * 4;
                        data[idx + 0] = uint8((i * 255) / (N - 1));
                        data[idx + 1] = uint8((j * 255) / (N - 1));
                        data[idx + 2] = uint8((k * 255) / (N - 1));
                        data[idx + 3] = 255;
                    }
            if (mLUTTex.IsValid()) {
                mDevice->WriteTextureRegion(mLUTTex, data.Data(),
                                             0, 0, 0,
                                             N, N, N, 0, 0, 0);
            }
        } else {
            // GL : dummy 1x1x1 3D texture (le shader skip via lutStrength=0
            // mais le binding doit avoir le target sampler3D pour eviter
            // undefined behavior si on bind un 2D au slot 3).
            mLUTSize = 1;
            auto td = NkTextureDesc::Tex3D(1, 1, 1, NkGPUFormat::NK_RGBA8_UNORM);
            td.debugName = "ColorGradingLUT_Dummy";
            mLUTTex = mDevice->CreateTexture(td);
            if (mLUTTex.IsValid()) {
                uint8 dummy[4] = {0, 0, 0, 255};
                mDevice->WriteTextureRegion(mLUTTex, dummy, 0,0,0, 1,1,1, 0,0,0);
            }
        }

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
            pd.AddPushConstant(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, 48);  // PC[0]=(exposure,gamma,vignette,sat) PC[1]=(bloomStr,lutStr,yFlipUV,lutSize) PC[2]=(autoExpStr,autoExpKey,_,_) — VS+FS
            // Phase H.2 : utilise le layout 2-bindings (uHDR + uBloom)
            if (mToneLayout.IsValid()) pd.descriptorSetLayouts.PushBack(mToneLayout);
            // Fullscreen triangle (gl_VertexIndex, pas de VBO) => pas de
            // vertex layout. Le rasterizer dessinera 3 verts via cmd->Draw(3).
            mPipeTone = mDevice->CreateGraphicsPipeline(pd);
        }

        // ── Phase H.2 : pipelines bloom downsample + upsample ──────────────
        // Le render pass utilise = celui d'un mBloomRT (tous compatibles car
        // meme format RGBA16F + pas de depth). On utilise mBloomRT[0] comme
        // template (cree par CreateTextures juste avant ce bloc).
        logger.Info("[NkPostProcessStack] Phase H.2 init : mBloomRT[0].IsValid()={0}\n",
                    mBloomRT[0].IsValid() ? 1 : 0);
        if (mShaderLib && mBloomRT[0].IsValid()) {
            auto progDown = mShaderLib->LoadOrCompileVF("PP_BloomDown", "", "");
            if (progDown.IsValid()) mShaderBloomDown = mShaderLib->GetRHIHandle(progDown);
            auto progUp = mShaderLib->LoadOrCompileVF("PP_BloomUp", "", "");
            if (progUp.IsValid())   mShaderBloomUp   = mShaderLib->GetRHIHandle(progUp);
            logger.Info("[NkPostProcessStack] Bloom shaders : down.valid={0} up.valid={1}\n",
                        mShaderBloomDown.IsValid() ? 1 : 0,
                        mShaderBloomUp.IsValid() ? 1 : 0);
        }

        if (mShaderBloomDown.IsValid() && mBloomRT[0].IsValid()) {
            NkGraphicsPipelineDesc pd;
            pd.shader       = mShaderBloomDown;
            pd.depthStencil = NkDepthStencilDesc::NoDepth();
            pd.rasterizer   = NkRasterizerDesc::NoCull();
            pd.blend        = NkBlendDesc::Opaque();
            pd.debugName    = "PP_BloomDown";
            pd.renderPass   = mBloomRT[0].GetRenderPass();
            pd.AddPushConstant(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, 16);  // (srcInvW, srcInvH, threshold, yFlipUV) — VS+FS
            if (mInputTexLayout.IsValid()) pd.descriptorSetLayouts.PushBack(mInputTexLayout);
            // Fullscreen triangle : pas de vertex layout.
            mPipeBloomDown = mDevice->CreateGraphicsPipeline(pd);
        }

        if (mShaderBloomUp.IsValid() && mBloomRT[0].IsValid()) {
            NkGraphicsPipelineDesc pd;
            pd.shader       = mShaderBloomUp;
            pd.depthStencil = NkDepthStencilDesc::NoDepth();
            pd.rasterizer   = NkRasterizerDesc::NoCull();
            // Phase H.2 : blend additif (SRC + DST = ONE * src + ONE * dst).
            // L'upsample accumule par-dessus le contenu (deja downsamples).
            pd.blend        = NkBlendDesc::Additive();
            pd.debugName    = "PP_BloomUp";
            pd.renderPass   = mBloomRT[0].GetRenderPass();
            pd.AddPushConstant(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, 16);  // (srcInvW, srcInvH, strength, yFlipUV) — VS+FS
            if (mInputTexLayout.IsValid()) pd.descriptorSetLayouts.PushBack(mInputTexLayout);
            // Fullscreen triangle : pas de vertex layout.
            mPipeBloomUp = mDevice->CreateGraphicsPipeline(pd);
        }

        // ── Phase H.3 : pipeline SSAO ───────────────────────────────────────
        if (mShaderLib && mSSAORT.IsValid()) {
            auto progSSAO = mShaderLib->LoadOrCompileVF("PP_SSAO", "", "");
            if (progSSAO.IsValid()) mShaderSSAO = mShaderLib->GetRHIHandle(progSSAO);
        }
        if (mShaderSSAO.IsValid() && mSSAORT.IsValid()) {
            NkGraphicsPipelineDesc pd;
            pd.shader       = mShaderSSAO;
            pd.depthStencil = NkDepthStencilDesc::NoDepth();
            pd.rasterizer   = NkRasterizerDesc::NoCull();
            pd.blend        = NkBlendDesc::Opaque();
            pd.debugName    = "PP_SSAO";
            pd.renderPass   = mSSAORT.GetRenderPass();
            pd.AddPushConstant(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, 16);  // (invResW, invResH, radius, yFlipUV) — VS+FS
            if (mInputTexLayout.IsValid()) pd.descriptorSetLayouts.PushBack(mInputTexLayout);
            mPipeSSAO = mDevice->CreateGraphicsPipeline(pd);
        }

        // ── Phase H.5b : pipeline SSAO Blur (denoise) ───────────────────────
        if (mShaderLib && mSSAORT.IsValid()) {
            auto progBlur = mShaderLib->LoadOrCompileVF("PP_SSAOBlur", "", "");
            if (progBlur.IsValid()) mShaderSSAOBlur = mShaderLib->GetRHIHandle(progBlur);
        }
        if (mShaderSSAOBlur.IsValid() && mSSAORT.IsValid()) {
            NkGraphicsPipelineDesc pd;
            pd.shader       = mShaderSSAOBlur;
            pd.depthStencil = NkDepthStencilDesc::NoDepth();
            pd.rasterizer   = NkRasterizerDesc::NoCull();
            pd.blend        = NkBlendDesc::Opaque();
            pd.debugName    = "PP_SSAOBlur";
            pd.renderPass   = mSSAORT.GetRenderPass();   // meme format R8_UNORM
            pd.AddPushConstant(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, 16);  // (invResW, invResH, yFlipUV, _pad)
            if (mInputTexLayout.IsValid()) pd.descriptorSetLayouts.PushBack(mInputTexLayout);
            mPipeSSAOBlur = mDevice->CreateGraphicsPipeline(pd);
        }

        // ── Phase L : pipeline FXAA (Fast Approximate AA, post-tonemap) ────
        // Shader externalise dans Resources/NKRenderer/Shaders/PP_FXAA/VK/.
        // Le wirage RenderGraph reste TODO (cf. ExecuteRHI : actuellement
        // tonemap ecrit direct au swapchain, FXAA necessiterait une pass
        // dediee). Pour l'instant le pipeline est cree et RunFXAA est appele
        // par Execute() (chemin non-RenderGraph). V1 : refactor pour split
        // tonemap -> mToneTex, FXAA -> swapchain.
        if (mShaderLib) {
            auto progFXAA = mShaderLib->LoadOrCompileVF("PP_FXAA", "", "");
            if (progFXAA.IsValid()) mShaderFXAA = mShaderLib->GetRHIHandle(progFXAA);
        }
        if (mShaderFXAA.IsValid()) {
            NkGraphicsPipelineDesc pd;
            pd.shader       = mShaderFXAA;
            pd.depthStencil = NkDepthStencilDesc::NoDepth();
            pd.rasterizer   = NkRasterizerDesc::NoCull();
            pd.blend        = NkBlendDesc::Opaque();
            pd.debugName    = "PP_FXAA";
            // Pas de renderPass specifie : fallback sur swapchain RP (compat
            // color-only standard, comme PP_Tone).
            pd.AddPushConstant(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, 16);  // (invResW, invResH, yFlipUV, _pad)
            if (mInputTexLayout.IsValid()) pd.descriptorSetLayouts.PushBack(mInputTexLayout);
            mPipeFXAA = mDevice->CreateGraphicsPipeline(pd);
        }

        (void)kFullscreenVS; (void)kTonemapFS;
        (void)kFXAAFS; (void)kBloomDownFS; (void)kBloomUpFS; (void)kSSAOFS;

        return mPipeTone.IsValid();
    }

    void NkPostProcessStack::Shutdown() {
        if (mPipeSSAO.IsValid())       mDevice->DestroyPipeline(mPipeSSAO);
        if (mPipeSSAOBlur.IsValid())   mDevice->DestroyPipeline(mPipeSSAOBlur);
        if (mPipeBloomDown.IsValid())  mDevice->DestroyPipeline(mPipeBloomDown);
        if (mPipeBloomUp.IsValid())    mDevice->DestroyPipeline(mPipeBloomUp);
        if (mPipeTone.IsValid())       mDevice->DestroyPipeline(mPipeTone);
        if (mPipeFXAA.IsValid())       mDevice->DestroyPipeline(mPipeFXAA);
        if (mLUTTex.IsValid())         { mDevice->DestroyTexture(mLUTTex); mLUTTex={}; }
        for (int i = 0; i < kBloomMips; i++) {
            if (mBloomRT[i].IsValid()) mBloomRT[i].Shutdown();
        }
        if (mSSAORT.IsValid())         mSSAORT.Shutdown();
        if (mToneSet.IsValid())        mDevice->FreeDescriptorSet(mToneSet);
        if (mToneLayout.IsValid())     mDevice->DestroyDescriptorSetLayout(mToneLayout);
        for (int i = 0; i < kBloomDescSets; i++) {
            if (mBloomSets[i].IsValid()) {
                mDevice->FreeDescriptorSet(mBloomSets[i]);
                mBloomSets[i] = {};
            }
        }
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

        // Phase H.2 : bloom mipchain via NkRenderTarget. Chaque mip a son
        // propre render pass + framebuffer pour permettre BeginRender pendant
        // RunBloom. mBloomRT[0] est en W/2, [5] est en W/64.
        for (int i = 0; i < kBloomMips; i++) {
            uint32 div = 1u << (i + 1);   // mip 0 = W/2, mip 5 = W/64
            uint32 bw = mW / div ? mW / div : 1;
            uint32 bh = mH / div ? mH / div : 1;
            NkRenderTargetDesc rtd;
            rtd.width  = bw;
            rtd.height = bh;
            rtd.hdr    = true;    // RGBA16F : preserve HDR pour les bright spots
            rtd.depth  = false;   // pas besoin de depth pour les passes bloom
            char nameBuf[32];
            snprintf(nameBuf, sizeof(nameBuf), "BloomMip%d", i);
            rtd.name   = NkString(nameBuf);
            // Note : Init() libere le RT precedent si re-init (utile pour OnResize)
            if (mBloomRT[i].IsValid()) mBloomRT[i].Shutdown();
            mBloomRT[i].Init(mDevice, mTex, rtd);
        }

        mToneTex  = mTex->CreateRenderTarget(mW,mH,NkGPUFormat::NK_RGBA8_UNORM,false,true,"Tone");
        mFinalTex = mTex->CreateRenderTarget(mW,mH,NkGPUFormat::NK_RGBA8_UNORM,false,true,"Final");

        // Phase H.3 : SSAO render target template (R8_UNORM, W/2 x H/2, no depth).
        // Sert juste a fournir un render pass pour la creation du pipeline SSAO ;
        // le rendu effectif passe par un transient du RenderGraph.
        {
            if (mSSAORT.IsValid()) mSSAORT.Shutdown();
            NkRenderTargetDesc rtd;
            rtd.width    = hw;
            rtd.height   = hh;
            rtd.hdr      = false;
            rtd.depth    = false;
            rtd.colorFmt = NkGPUFormat::NK_R8_UNORM;
            rtd.name     = NkString("SSAO_Template");
            mSSAORT.Init(mDevice, mTex, rtd);
        }
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
            // PP shaders : VS ET FS lisent le push constant (VS lit yFlipUV,
            // FS lit exposure/gamma/etc). Le pipeline declare la range avec
            // NK_ALL_GRAPHICS (cf. AddPushConstant lors de CreateGraphicsPipeline).
            // Le push DOIT matcher exactement le stageFlags de la range sinon
            // VUID-vkCmdPushConstants-offset-01796 (push stages doit inclure
            // toutes les stages de la range overlappante).
            cmd->PushConstants(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, pcSize, pushConst);
        }
        // Fullscreen triangle : 3 verts sans VBO.
        cmd->Draw(3, 1, 0, 0);
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
        // Phase H.2 : RunBloom() legacy n'est plus le path actif. Le bloom
        // multi-pass est maintenant orchestre par NkRendererImpl::BuildDefault-
        // RenderGraph qui ajoute les passes Bloom_Down[0..4] + Bloom_Up[0..4]
        // au RG avec transients RGBA16F, puis appelle DrawBloomDownPass /
        // DrawBloomUpPass dans chaque pass.
        // Cette methode reste pour compat avec Execute() (legacy non-RG path).
        (void)cmd; (void)hdr;
        return NkTexHandle::Null();
    }

    NkTexHandle NkPostProcessStack::RunTonemap(NkICommandBuffer* cmd, NkTexHandle hdr) {
        bool isVulkan = mDevice && mDevice->GetApi() == NkGraphicsApi::NK_GFX_API_VULKAN;
        // PC[0]=(exposure,gamma,vignette,sat) PC[1]=(bloomStr,bloomThr,invW,invH)
        struct PC {
            float exposure, gamma, vignetteIntens, saturation;
            float bloomStr, bloomThr, invW, invH;
        } pc;
        pc.exposure      = mCfg.exposure;
        pc.gamma         = isVulkan ? 1.0f : mCfg.gamma;
        pc.vignetteIntens= mCfg.vignette    ? mCfg.vignetteIntens : 0.0f;
        pc.saturation    = mCfg.colorGrading? mCfg.saturation     : 1.0f;
        pc.bloomStr      = mCfg.bloom       ? mCfg.bloomStrength   : 0.0f;
        pc.bloomThr      = mCfg.bloom       ? mCfg.bloomThreshold  : 1.0f;
        pc.invW          = mW > 0 ? 1.0f / (float)mW : 0.f;
        pc.invH          = mH > 0 ? 1.0f / (float)mH : 0.f;
        DrawFullscreen(cmd, mPipeTone, hdr, &pc, sizeof(pc));
        return mToneTex;
    }

    void NkPostProcessStack::ExecuteRHI(NkICommandBuffer* cmd, NkTextureHandle hdrIn,
                                          NkTextureHandle bloomTex,
                                          NkTextureHandle ssaoTex) {
        if (!cmd || !mPipeTone.IsValid() || !mToneSet.IsValid() || !hdrIn.IsValid()) return;

        NkSamplerHandle samp = mResources ? mResources->GetSamplerLinearClamp() : NkSamplerHandle{};
        if (!samp.IsValid()) return;

        // Phase H.2/H.3 : bind 3 textures au tonemap (uHDR=0 + uBloom=1 + uSSAO=2)
        mDevice->BindTextureSampler(mToneSet, 0, hdrIn, samp);
        if (bloomTex.IsValid()) {
            mDevice->BindTextureSampler(mToneSet, 1, bloomTex, samp);
        } else {
            // Fallback : pas de bloom — bind l'HDR au binding=1 (shader avec
            // bloomStrength=0 ignore le sample).
            mDevice->BindTextureSampler(mToneSet, 1, hdrIn, samp);
        }
        if (ssaoTex.IsValid()) {
            mDevice->BindTextureSampler(mToneSet, 2, ssaoTex, samp);
        } else {
            // Fallback : pas de SSAO — bind l'HDR au binding=2 (shader avec
            // ssaoEnabled=false multiplie par 1.0 = pas d'attenuation).
            mDevice->BindTextureSampler(mToneSet, 2, hdrIn, samp);
        }
        // Phase L : bind le LUT 3D (sampler3D au binding=3). Si pas alloue ou
        // strength=0 le shader skip. Fallback : bind l'HDR pour eviter undefined.
        if (mLUTTex.IsValid()) {
            mDevice->BindTextureSampler(mToneSet, 3, mLUTTex, samp);
        } else {
            mDevice->BindTextureSampler(mToneSet, 3, hdrIn, samp);
        }
        cmd->BindGraphicsPipeline(mPipeTone);
        cmd->BindDescriptorSet(mToneSet, 0);

        // Layout PC : 32 bytes
        //   PC[0] = (exposure, gamma, vignetteIntens, saturation)
        //   PC[1] = (bloomStrength, lutStrength, yFlipUV, lutSize)
        // Phase L : lutStrength remplace bloomThreshold (inutilise apres le 1er
        // downsample). lutSize en p1.w pour le bias texel correct cote shader.
        bool isVK = mDevice && mDevice->GetApi() == NkGraphicsApi::NK_GFX_API_VULKAN;
        float32 pc[12] = {
            // p0 = (exposure, gamma, vignetteIntens, saturation)
            mCfg.exposure,
            isVK ? 1.0f : mCfg.gamma,
            mCfg.vignette    ? mCfg.vignetteIntens : 0.f,
            mCfg.colorGrading? mCfg.saturation     : 1.f,
            // p1 = (bloomStrength, lutStrength, yFlipUV, lutSize)
            mCfg.bloom       ? mCfg.bloomStrength   : 0.f,
            mCfg.colorGrading? mCfg.lutStrength    : 0.f,
            // yFlipUV = -1 dans les deux backends : le HDR transient (FBO
            // custom) est stocke en convention Y-down dans VK ET GL (le RHI
            // OpenGL flip son viewport pour les FBO custom afin de matcher
            // la convention storage Vulkan). Le top screen rendu = ligne 0
            // du storage -> UV.y = 0 pour sampler -> flip via VS.
            -1.f,
            float32(mLUTSize),
            // p2 = (autoExposureStrength, autoExposureKey, _, _)
            mCfg.autoExposureStrength,
            mCfg.autoExposureKey,
            0.f, 0.f
        };
        // Push avec NK_ALL_GRAPHICS pour matcher la range pipeline (cf. fix
        // VUID-vkCmdPushConstants-offset-01796 — VS lit yFlipUV au slot PC[1].z).
        cmd->PushConstants(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, sizeof(pc), pc);

        // Fullscreen triangle : 3 verts sans VBO.
        cmd->Draw(3, 1, 0, 0);
    }

    // Phase L FXAA wirage RenderGraph : version "ExecuteFXAA" sans alloc
    // texture intermediate. Le RG bind deja le swapchain comme RT, on lit
    // ldrIn (mToneTex via texture handle) au binding=0, draw fullscreen.
    void NkPostProcessStack::ExecuteFXAA(NkICommandBuffer* cmd, NkTextureHandle ldrIn) {
        if (!cmd || !mPipeFXAA.IsValid() || !ldrIn.IsValid()) return;

        // Bind input texture (mToneTex sample) au binding=0 du PP_FXAA.
        if (mInputTexSet.IsValid() && mResources) {
            NkSamplerHandle samp = mResources->GetSamplerLinearClamp();
            mDevice->BindTextureSampler(mInputTexSet, 0, ldrIn, samp);
        }
        cmd->BindGraphicsPipeline(mPipeFXAA);
        // FXAA shader uses set=0 binding=0 (cf. pp_fxaa.frag.vk.glsl).
        if (mInputTexSet.IsValid()) cmd->BindDescriptorSet(mInputTexSet, 0);

        // Push 16 bytes : (invResW, invResH, yFlipUV, _pad). Stage ALL_GRAPHICS
        // pour matcher la range pipeline.
        // yFlipUV : sur Vulkan le viewport est Y-flipped (storage transient
        // top-down), donc on flip l'UV cote VS pour matcher. Sur OpenGL le
        // viewport n'est pas flipped et le transient FBO est aussi top-down,
        // mais l'output vers swapchain doit etre flippe -> on garde UV direct.
        // (Convention oppose au tonemap qui ecrit direct au swapchain).
        const bool isVK = mDevice
                       && mDevice->GetApi() == NkGraphicsApi::NK_GFX_API_VULKAN;
        struct PC { float invResW, invResH, yFlipUV, _pad; } pc;
        pc.invResW = 1.0f / (float)(mW > 0 ? mW : 1);
        pc.invResH = 1.0f / (float)(mH > 0 ? mH : 1);
        pc.yFlipUV = isVK ? -1.f : +1.f;
        pc._pad    = 0.f;
        cmd->PushConstants(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS,
                            0, sizeof(pc), &pc);
        cmd->Draw(3, 1, 0, 0);
    }

    NkTexHandle NkPostProcessStack::RunFXAA(NkICommandBuffer* cmd, NkTexHandle ldr) {
        // Phase L : push 16 bytes (invResW, invResH, yFlipUV, _pad) pour matcher
        // le pipeline range NK_ALL_GRAPHICS du VS+FS (cf. pp_fxaa.{vert,frag}.vk.glsl).
        struct PC { float invResW, invResH, yFlipUV, _pad; } pc;
        pc.invResW = 1.0f / (float)(mW > 0 ? mW : 1);
        pc.invResH = 1.0f / (float)(mH > 0 ? mH : 1);
        pc.yFlipUV = -1.f;   // FBO custom : Y-flip pour matcher la convention
        pc._pad    = 0.f;
        DrawFullscreen(cmd, mPipeFXAA, ldr, &pc, sizeof(pc));
        return mFinalTex;
    }

    // ── Phase H.2 : sub-passes bloom multi-pass ──────────────────────────────
    // Le RenderGraph ouvre la passe (color attachment = mip cible) avant
    // l'appel, et la ferme apres. On ne fait QUE bind pipeline + descriptor
    // + push constants + draw fullscreen quad.

    // Phase H.2 : pool rotatif de descriptor sets pour les sub-passes bloom.
    // Vulkan interdit d'updater un descriptor pendant qu'un draw precedent
    // l'utilise. Chaque sub-pass prend un set frais via ce helper.
    static NkDescSetHandle NextBloomSet(NkDescSetHandle (&pool)[33], int& cursor) {
        NkDescSetHandle h = pool[cursor % 33];
        cursor++;
        return h;
    }

    void NkPostProcessStack::DrawBloomDownPass(NkICommandBuffer* cmd, NkTextureHandle src,
                                                 uint32 srcW, uint32 srcH, float threshold) {
        if (!cmd || !mPipeBloomDown.IsValid() ||
            !src.IsValid()) return;

        NkSamplerHandle samp = mResources ? mResources->GetSamplerLinearClamp() : NkSamplerHandle{};
        if (!samp.IsValid()) return;

        NkDescSetHandle set = NextBloomSet(mBloomSets, mBloomSetCursor);
        if (!set.IsValid()) return;

        mDevice->BindTextureSampler(set, 0, src, samp);
        cmd->BindGraphicsPipeline(mPipeBloomDown);
        cmd->BindDescriptorSet(set, 0);

        struct PC { float invW, invH, threshold, yFlipUV; } pc;
        pc.invW       = srcW > 0 ? 1.0f / (float)srcW : 0.f;
        pc.invH       = srcH > 0 ? 1.0f / (float)srcH : 0.f;
        pc.threshold  = threshold;
        // Sub-passes bloom : NE PAS flipper en GL (FBO custom = Y-up storage
        // natif). Sinon bloomMip storage decale par rapport au HDR storage
        // -> tonemap sample bloom et HDR a des conventions differentes ->
        // bloom mal positionne. En VK le storage est Y-down natif, flip OK.
        bool isVK = mDevice && mDevice->GetApi() == NkGraphicsApi::NK_GFX_API_VULKAN;
        pc.yFlipUV    = isVK ? -1.f : +1.f;
        cmd->PushConstants(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, sizeof(pc), &pc);

        // Fullscreen triangle : 3 verts sans VBO.
        cmd->Draw(3, 1, 0, 0);
    }

    void NkPostProcessStack::DrawBloomUpPass(NkICommandBuffer* cmd, NkTextureHandle src,
                                               uint32 srcW, uint32 srcH, float strength) {
        if (!cmd || !mPipeBloomUp.IsValid() ||
            !src.IsValid()) return;

        NkSamplerHandle samp = mResources ? mResources->GetSamplerLinearClamp() : NkSamplerHandle{};
        if (!samp.IsValid()) return;

        NkDescSetHandle set = NextBloomSet(mBloomSets, mBloomSetCursor);
        if (!set.IsValid()) return;

        mDevice->BindTextureSampler(set, 0, src, samp);
        cmd->BindGraphicsPipeline(mPipeBloomUp);
        cmd->BindDescriptorSet(set, 0);

        struct PC { float invW, invH, strength, yFlipUV; } pc;
        pc.invW     = srcW > 0 ? 1.0f / (float)srcW : 0.f;
        pc.invH     = srcH > 0 ? 1.0f / (float)srcH : 0.f;
        pc.strength = strength;
        // Sub-passes bloom : pas de flip en GL (cf. DrawBloomDownPass).
        bool isVK = mDevice && mDevice->GetApi() == NkGraphicsApi::NK_GFX_API_VULKAN;
        pc.yFlipUV  = isVK ? -1.f : +1.f;
        cmd->PushConstants(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, sizeof(pc), &pc);

        // Fullscreen triangle : 3 verts sans VBO.
        cmd->Draw(3, 1, 0, 0);
    }

    // ── Phase H.3 : SSAO sub-pass ────────────────────────────────────────────
    // Le RG appelle cette methode dans une pass deja ouverte (color attachment
    // = ssaoTex transient R8_UNORM, depthSrc = mainDepth transient).
    // On bind le depth comme sampler au binding=0 et draw fullscreen triangle.
    void NkPostProcessStack::DrawSSAOPass(NkICommandBuffer* cmd, NkTextureHandle depthSrc,
                                            uint32 ssaoW, uint32 ssaoH) {
        if (!cmd || !mPipeSSAO.IsValid() || !depthSrc.IsValid()) return;

        NkSamplerHandle samp = mResources ? mResources->GetSamplerLinearClamp() : NkSamplerHandle{};
        if (!samp.IsValid()) return;

        // Reutilise le pool rotatif bloom (a renommer en pool generic PP plus
        // tard) — depth bound au binding=0 du set frais.
        NkDescSetHandle set = mBloomSets[mBloomSetCursor % kBloomDescSets];
        mBloomSetCursor++;
        if (!set.IsValid()) return;

        mDevice->BindTextureSampler(set, 0, depthSrc, samp);
        cmd->BindGraphicsPipeline(mPipeSSAO);
        cmd->BindDescriptorSet(set, 0);

        // Push constant : invResW, invResH, radius, yFlipUV
        // Radius en UV space (0.005 - 0.02 typique pour 720p).
        bool isVK = mDevice && mDevice->GetApi() == NkGraphicsApi::NK_GFX_API_VULKAN;
        struct PC { float invW, invH, radius, yFlipUV; } pc;
        pc.invW    = ssaoW > 0 ? 1.0f / (float)ssaoW : 0.f;
        pc.invH    = ssaoH > 0 ? 1.0f / (float)ssaoH : 0.f;
        pc.radius  = mCfg.ssaoRadius > 0.f ? mCfg.ssaoRadius * 0.01f : 0.01f;
        // SSAO sub-pass : meme convention que bloom (Y-down VK natif, Y-up GL natif).
        pc.yFlipUV = isVK ? -1.f : +1.f;
        cmd->PushConstants(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, sizeof(pc), &pc);

        cmd->Draw(3, 1, 0, 0);
    }

    // ── Phase H.5b : SSAO Blur sub-pass (denoise) ───────────────────────────
    void NkPostProcessStack::DrawSSAOBlurPass(NkICommandBuffer* cmd, NkTextureHandle aoSrc,
                                                uint32 ssaoW, uint32 ssaoH) {
        if (!cmd || !mPipeSSAOBlur.IsValid() || !aoSrc.IsValid()) return;

        NkSamplerHandle samp = mResources ? mResources->GetSamplerLinearClamp() : NkSamplerHandle{};
        if (!samp.IsValid()) return;

        NkDescSetHandle set = mBloomSets[mBloomSetCursor % kBloomDescSets];
        mBloomSetCursor++;
        if (!set.IsValid()) return;

        mDevice->BindTextureSampler(set, 0, aoSrc, samp);
        cmd->BindGraphicsPipeline(mPipeSSAOBlur);
        cmd->BindDescriptorSet(set, 0);

        bool isVK = mDevice && mDevice->GetApi() == NkGraphicsApi::NK_GFX_API_VULKAN;
        struct PC { float invW, invH, yFlipUV, _pad; } pc;
        pc.invW    = ssaoW > 0 ? 1.0f / (float)ssaoW : 0.f;
        pc.invH    = ssaoH > 0 ? 1.0f / (float)ssaoH : 0.f;
        pc.yFlipUV = isVK ? -1.f : +1.f;
        pc._pad    = 0.f;
        cmd->PushConstants(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, sizeof(pc), &pc);

        cmd->Draw(3, 1, 0, 0);
    }

} // namespace renderer
} // namespace nkentseu
