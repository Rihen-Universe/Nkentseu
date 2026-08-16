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

		// Declarations ANTICIPEES : ces deux helpers sont definis plus bas (section
		// « Phase L V1 — AUTO-EXPOSURE ») mais utilises AVANT, dans ExecuteRHI.
		// En C++ un identifiant doit etre visible au point d'appel : sans elles le
		// fichier ne compile pas (« use of undeclared identifier »), ce qui bloque
		// TOUTES les cibles, Web comprise. On declare ici plutot que de deplacer
		// les definitions, pour garder la section auto-exposure d'un seul bloc.
		static float32 NkAutoExpStrength(float32 fromConfig);
		static float32 NkAutoExpSpeed(float32 fromConfig);

		// =========================================================================
		// SHADERS GLSL EMBARQUÉS — référence complète pour intégration future
		// =========================================================================

		// Vertex shader fullscreen OpenGL-natif. NkMeshSystem::GetQuad() fournit
		// un quad NDC dans [-1,1] avec UV attribut a location 3 (NkVertex3D layout).
		// Stride 56 : pos(12) normal(12) tangent(12) uv(8) uv2(8) color(4).
		static const char *kFullscreenVS_GL = R"GLSL(
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
		static const char *kTonemapFS_GL = R"GLSL(
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
		static const char *kFullscreenVS [[maybe_unused]] = "/* legacy Vulkan VS, see kFullscreenVS_GL */";
		static const char *kTonemapFS [[maybe_unused]] = "/* legacy Vulkan FS, see kTonemapFS_GL */";

		// FXAA 3.11 simplifié
		static const char *kFXAAFS = R"GLSL(
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

		static const char *kBloomDownFS = R"GLSL(
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

		static const char *kBloomUpFS = R"GLSL(
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

		static const char *kSSAOFS = R"GLSL(
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

		bool NkPostProcessStack::Init(NkIDevice *d, NkTextureLibrary *t, NkMeshSystem *m, NkShaderLibrary *sl,
									  NkResources *res, uint32 w, uint32 h) {
			mDevice = d;
			mTex = t;
			mMesh = m;
			mShaderLib = sl;
			mResources = res;
			mW = w;
			mH = h;
			mQuad = m->GetQuad();
			CreateTextures();

			// ── Descriptor set layout : 1 sampler combine pour la texture d'entree ──
			// Utilise par bloom_down, bloom_up, ssao, fxaa.
			NkDescriptorSetLayoutDesc layout;
			layout.Add(0, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS);
			mInputTexLayout = mDevice->CreateDescriptorSetLayout(layout);
			mInputTexSet = mDevice->AllocateDescriptorSet(mInputTexLayout);
			mBlitTexSet = mDevice->AllocateDescriptorSet(mInputTexLayout);

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
			//   - binding 4 = uAvgLuma  (Phase L V1 : luminance moyenne 1x1 mesuree)
			NkDescriptorSetLayoutDesc tonelay;
			tonelay.Add(0, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS);
			tonelay.Add(1, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS);
			tonelay.Add(2, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS);
			tonelay.Add(3, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS);
			tonelay.Add(4, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS);
			mToneLayout = mDevice->CreateDescriptorSetLayout(tonelay);
			mToneSet = mDevice->AllocateDescriptorSet(mToneLayout);

			// ── Auto-exposure V1 : layout dedie (uHDR + uPrevLuma) + pool de sets ──
			// binding 2 = uBloom : la mesure doit porter sur la COMPOSITION
			// (scene + halo), pas sur la scene seule — sinon elle ne voit jamais
			// le halo qu'elle amplifie ensuite.
			NkDescriptorSetLayoutDesc aelay;
			aelay.Add(0, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS);
			aelay.Add(1, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS);
			aelay.Add(2, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS);
			mAutoExpLayout = mDevice->CreateDescriptorSetLayout(aelay);
			for (int i = 0; i < kAutoExpDescSets; i++)
				mAutoExpSets[i] = mDevice->AllocateDescriptorSet(mAutoExpLayout);
			mAutoExpSetCursor = 0;

			// ── TAA : layout 3 samplers (courant LDR + historique + profondeur) ──
			NkDescriptorSetLayoutDesc taalay;
			taalay.Add(0, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS);
			taalay.Add(1, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS);
			taalay.Add(2, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS);
			mTAALayout = mDevice->CreateDescriptorSetLayout(taalay);
			for (int i = 0; i < kTAADescSets; i++)
				mTAASets[i] = mDevice->AllocateDescriptorSet(mTAALayout);
			mTAASetCursor = 0;

			// Phase L : create identity LUT 16^3 par defaut (no color change).
			// User upload son LUT custom via SetColorGradingLUT (accessible par
			// renderer->GetPostProcess()). Format : RGBA8 UNORM, trilinear.
			// NB : l'ancien dummy 1x1 OpenGL (crash 2026-05-23) datait du chemin
			// SPIRV-Cross sampler3D ; le tonemap est depuis genere en GLSL natif
			// par NkSL et le backend GL a le path TexStorage3D/SubImage3D complet
			// -> vraie LUT reactivee sur GL (verifie par capture 2026-07-12).
			mLUTSize = mCfg.lutSize > 0 ? mCfg.lutSize : 16;
			if (mLUTSize > 64)
				mLUTSize = 64; // hard cap pour eviter VRAM
			{
				auto td = NkTextureDesc::Tex3D(mLUTSize, mLUTSize, mLUTSize, NkGPUFormat::NK_RGBA8_UNORM);
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
					mDevice->WriteTextureRegion(mLUTTex, data.Data(), 0, 0, 0, N, N, N, 0, 0, 0);
				}
			}

			// ── Vertex layout du fullscreen quad (NkVertex3D) ─────────────────────
			// Le shader VS n'utilise que aPos (loc 0) et aUV (loc 3). On declare
			// tous les attributs pour matcher le format du mesh quad sans avertissement.
			auto buildVertexLayout = [](NkGraphicsPipelineDesc &pd) {
				pd.vertexLayout.AddBinding(0, sizeof(NkVertex3D), false)
					.AddAttribute(0, 0, ::nkentseu::NkVertexFormat::NK_RGB32_FLOAT, 0, "POSITION", 0)
					.AddAttribute(1, 0, ::nkentseu::NkVertexFormat::NK_RGB32_FLOAT, 12, "NORMAL", 0)
					.AddAttribute(2, 0, ::nkentseu::NkVertexFormat::NK_RGB32_FLOAT, 24, "TANGENT", 0)
					.AddAttribute(3, 0, ::nkentseu::NkVertexFormat::NK_RG32_FLOAT, 36, "TEXCOORD", 0)
					.AddAttribute(4, 0, ::nkentseu::NkVertexFormat::NK_RG32_FLOAT, 44, "TEXCOORD", 1)
					.AddAttribute(5, 0, ::nkentseu::NkVertexFormat::NK_RGBA8_UNORM, 52, "COLOR", 0);
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
				pd.shader = mShaderTone;
				pd.depthStencil = NkDepthStencilDesc::NoDepth();
				pd.rasterizer = NkRasterizerDesc::NoCull();
				pd.blend = NkBlendDesc::Opaque();
				pd.debugName = "PP_Tone";
				pd.AddPushConstant(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0,
								   64); // PC[0]=(exposure,gamma,vignette,sat) PC[1]=(bloomStr,lutStr,yFlipUV,lutSize)
										// PC[2]=(autoExpStr,autoExpKey,bloomYFlip,hdrClamp)
										// PC[3]=(expMin,expMax,_,_) — VS+FS
										// 64 o : sous la garantie Vulkan (128) et sous les root constants
										// DX12 (32 DWORDs depuis le fix 269207c9).
				// Phase H.2 : utilise le layout 2-bindings (uHDR + uBloom)
				if (mToneLayout.IsValid())
					pd.descriptorSetLayouts.PushBack(mToneLayout);
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
				if (progDown.IsValid())
					mShaderBloomDown = mShaderLib->GetRHIHandle(progDown);
				auto progUp = mShaderLib->LoadOrCompileVF("PP_BloomUp", "", "");
				if (progUp.IsValid())
					mShaderBloomUp = mShaderLib->GetRHIHandle(progUp);
				logger.Info("[NkPostProcessStack] Bloom shaders : down.valid={0} up.valid={1}\n",
							mShaderBloomDown.IsValid() ? 1 : 0, mShaderBloomUp.IsValid() ? 1 : 0);
			}

			if (mShaderBloomDown.IsValid() && mBloomRT[0].IsValid()) {
				NkGraphicsPipelineDesc pd;
				pd.shader = mShaderBloomDown;
				pd.depthStencil = NkDepthStencilDesc::NoDepth();
				pd.rasterizer = NkRasterizerDesc::NoCull();
				pd.blend = NkBlendDesc::Opaque();
				pd.debugName = "PP_BloomDown";
				pd.renderPass = mBloomRT[0].GetRenderPass();
				pd.AddPushConstant(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0,
								   16); // (srcInvW, srcInvH, threshold, yFlipUV) — VS+FS
				if (mInputTexLayout.IsValid())
					pd.descriptorSetLayouts.PushBack(mInputTexLayout);
				// Fullscreen triangle : pas de vertex layout.
				mPipeBloomDown = mDevice->CreateGraphicsPipeline(pd);
			}

			if (mShaderBloomUp.IsValid() && mBloomRT[0].IsValid()) {
				NkGraphicsPipelineDesc pd;
				pd.shader = mShaderBloomUp;
				pd.depthStencil = NkDepthStencilDesc::NoDepth();
				pd.rasterizer = NkRasterizerDesc::NoCull();
				// Phase H.2 : blend additif (SRC + DST = ONE * src + ONE * dst).
				// L'upsample accumule par-dessus le contenu (deja downsamples).
				pd.blend = NkBlendDesc::Additive();
				pd.debugName = "PP_BloomUp";
				pd.renderPass = mBloomRT[0].GetRenderPass();
				pd.AddPushConstant(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0,
								   16); // (srcInvW, srcInvH, strength, yFlipUV) — VS+FS
				if (mInputTexLayout.IsValid())
					pd.descriptorSetLayouts.PushBack(mInputTexLayout);
				// Fullscreen triangle : pas de vertex layout.
				mPipeBloomUp = mDevice->CreateGraphicsPipeline(pd);
			}

			// ── Phase H.3 : pipeline SSAO ───────────────────────────────────────
			if (mShaderLib && mSSAORT.IsValid()) {
				auto progSSAO = mShaderLib->LoadOrCompileVF("PP_SSAO", "", "");
				if (progSSAO.IsValid())
					mShaderSSAO = mShaderLib->GetRHIHandle(progSSAO);
			}
			if (mShaderSSAO.IsValid() && mSSAORT.IsValid()) {
				NkGraphicsPipelineDesc pd;
				pd.shader = mShaderSSAO;
				pd.depthStencil = NkDepthStencilDesc::NoDepth();
				pd.rasterizer = NkRasterizerDesc::NoCull();
				pd.blend = NkBlendDesc::Opaque();
				pd.debugName = "PP_SSAO";
				pd.renderPass = mSSAORT.GetRenderPass();
				// 128 octets : mat4 invViewProj (64) + camPos (16) + camFwd (16)
				// + p0 (16) + p1 (16). Exactement la garantie Vulkan et les 32
				// DWORDs de root constants DX12 — ne plus rien y ajouter.
				pd.AddPushConstant(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, 128);
				if (mInputTexLayout.IsValid())
					pd.descriptorSetLayouts.PushBack(mInputTexLayout);
				mPipeSSAO = mDevice->CreateGraphicsPipeline(pd);
			}

			// ── Phase H.5b : pipeline SSAO Blur (denoise) ───────────────────────
			if (mShaderLib && mSSAORT.IsValid()) {
				auto progBlur = mShaderLib->LoadOrCompileVF("PP_SSAOBlur", "", "");
				if (progBlur.IsValid())
					mShaderSSAOBlur = mShaderLib->GetRHIHandle(progBlur);
			}
			if (mShaderSSAOBlur.IsValid() && mSSAORT.IsValid()) {
				NkGraphicsPipelineDesc pd;
				pd.shader = mShaderSSAOBlur;
				pd.depthStencil = NkDepthStencilDesc::NoDepth();
				pd.rasterizer = NkRasterizerDesc::NoCull();
				pd.blend = NkBlendDesc::Opaque();
				pd.debugName = "PP_SSAOBlur";
				pd.renderPass = mSSAORT.GetRenderPass(); // meme format R8_UNORM
				pd.AddPushConstant(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0,
								   16); // (invResW, invResH, yFlipUV, _pad)
				if (mInputTexLayout.IsValid())
					pd.descriptorSetLayouts.PushBack(mInputTexLayout);
				mPipeSSAOBlur = mDevice->CreateGraphicsPipeline(pd);
			}

			// ── Phase L V1 : pipeline AUTO-EXPOSURE (mesure de luminance 1x1) ──
			// Cible 1x1 RGBA16F : le HDR peut depasser 1.0, une cible UNORM ecraserait
			// la mesure. Le render pass vient de mLumaRT[0] (meme format que [1]).
			if (mShaderLib && mLumaRT[0].IsValid()) {
				auto progAE = mShaderLib->LoadOrCompileVF("PP_AutoExposure", "", "");
				if (progAE.IsValid())
					mShaderAutoExp = mShaderLib->GetRHIHandle(progAE);
				logger.Info("[NkPostProcessStack] AutoExposure shader : valid={0}\n",
							mShaderAutoExp.IsValid() ? 1 : 0);
			}
			if (mShaderAutoExp.IsValid() && mLumaRT[0].IsValid()) {
				NkGraphicsPipelineDesc pd;
				pd.shader = mShaderAutoExp;
				pd.depthStencil = NkDepthStencilDesc::NoDepth();
				pd.rasterizer = NkRasterizerDesc::NoCull();
				pd.blend = NkBlendDesc::Opaque();
				pd.debugName = "PP_AutoExposure";
				pd.renderPass = mLumaRT[0].GetRenderPass();
				pd.AddPushConstant(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0,
								   32); // PC[0]=(dtSeconds, adaptSpeed, minLuma, maxLuma)
										// PC[1]=(bloomStrength, hasBloom, _, _) — VS+FS
				if (mAutoExpLayout.IsValid())
					pd.descriptorSetLayouts.PushBack(mAutoExpLayout);
				mPipeAutoExp = mDevice->CreateGraphicsPipeline(pd);
				logger.Info("[NkPostProcessStack] AutoExposure pipeline : valid={0}\n",
							mPipeAutoExp.IsValid() ? 1 : 0);
			}

			// ── Phase L : shader TAA (Temporal AA, post-tonemap) ───────────────
			// Le SHADER est charge ici (IsTAAEnabled en depend : le graph doit savoir
			// des sa construction s'il declare les passes TAA), mais le PIPELINE est
			// cree en lazy dans EnsureTAAPipeline : il doit etre RP-compatible avec le
			// framebuffer que le graph cree pour la passe, lequel n'existe pas encore.
			if (mShaderLib) {
				auto progTAA = mShaderLib->LoadOrCompileVF("PP_TAA", "", "");
				if (progTAA.IsValid())
					mShaderTAA = mShaderLib->GetRHIHandle(progTAA);
				logger.Info("[NkPostProcessStack] TAA shader : valid={0}\n", mShaderTAA.IsValid() ? 1 : 0);
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
				if (progFXAA.IsValid())
					mShaderFXAA = mShaderLib->GetRHIHandle(progFXAA);
			}
			if (mShaderFXAA.IsValid()) {
				NkGraphicsPipelineDesc pd;
				pd.shader = mShaderFXAA;
				pd.depthStencil = NkDepthStencilDesc::NoDepth();
				pd.rasterizer = NkRasterizerDesc::NoCull();
				pd.blend = NkBlendDesc::Opaque();
				pd.debugName = "PP_FXAA";
				// Pas de renderPass specifie : fallback sur swapchain RP (compat
				// color-only standard, comme PP_Tone).
				pd.AddPushConstant(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0,
								   16); // (invResW, invResH, yFlipUV, _pad)
				if (mInputTexLayout.IsValid())
					pd.descriptorSetLayouts.PushBack(mInputTexLayout);
				mPipeFXAA = mDevice->CreateGraphicsPipeline(pd);
			}

			// ── Pipeline Blit (MirrorPresent : recopie 1:1 vers le swapchain) ──
			// Meme interface que PP_FXAA (fullscreen triangle, PC 16 octets,
			// sampler binding 0) pour reutiliser la plomberie a l'identique.
			// Sert au "voir + enregistrer" : quand la cible finale est redirigee
			// (capture/record), cette passe garde la fenetre vivante.
			if (mShaderLib) {
				auto progBlit = mShaderLib->LoadOrCompileVF("Blit", "", "");
				if (progBlit.IsValid())
					mShaderBlit = mShaderLib->GetRHIHandle(progBlit);
			}
			if (mShaderBlit.IsValid()) {
				NkGraphicsPipelineDesc pd;
				pd.shader = mShaderBlit;
				pd.depthStencil = NkDepthStencilDesc::NoDepth();
				pd.rasterizer = NkRasterizerDesc::NoCull();
				pd.blend = NkBlendDesc::Opaque();
				pd.debugName = "Blit";
				pd.AddPushConstant(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, 16);
				if (mInputTexLayout.IsValid())
					pd.descriptorSetLayouts.PushBack(mInputTexLayout);
				mPipeBlit = mDevice->CreateGraphicsPipeline(pd);
			}

			(void)kFullscreenVS;
			(void)kTonemapFS;
			(void)kFXAAFS;
			(void)kBloomDownFS;
			(void)kBloomUpFS;
			(void)kSSAOFS;

			return mPipeTone.IsValid();
		}

		void NkPostProcessStack::Shutdown() {
			if (mPipeSSAO.IsValid())
				mDevice->DestroyPipeline(mPipeSSAO);
			if (mPipeSSAOBlur.IsValid())
				mDevice->DestroyPipeline(mPipeSSAOBlur);
			if (mPipeBloomDown.IsValid())
				mDevice->DestroyPipeline(mPipeBloomDown);
			if (mPipeBloomUp.IsValid())
				mDevice->DestroyPipeline(mPipeBloomUp);
			if (mPipeTone.IsValid())
				mDevice->DestroyPipeline(mPipeTone);
			if (mPipeFXAA.IsValid())
				mDevice->DestroyPipeline(mPipeFXAA);
			if (mPipeBlit.IsValid())
				mDevice->DestroyPipeline(mPipeBlit);
			if (mPipeBlitRT.IsValid())
				mDevice->DestroyPipeline(mPipeBlitRT);
			mPipeBlitRT = {};
			if (mBlitRTSet.IsValid())
				mDevice->FreeDescriptorSet(mBlitRTSet);
			mBlitRTSet = {};
			// TAA (les cibles d'historique appartiennent au RenderGraph)
			if (mPipeTAA.IsValid())
				mDevice->DestroyPipeline(mPipeTAA);
			mPipeTAA = {};
			for (int i = 0; i < kTAADescSets; i++) {
				if (mTAASets[i].IsValid())
					mDevice->FreeDescriptorSet(mTAASets[i]);
				mTAASets[i] = {};
			}
			if (mTAALayout.IsValid())
				mDevice->DestroyDescriptorSetLayout(mTAALayout);
			mTAALayout = {};
			// Auto-exposure V1
			if (mPipeAutoExp.IsValid())
				mDevice->DestroyPipeline(mPipeAutoExp);
			for (int i = 0; i < 2; i++) {
				if (mLumaRT[i].IsValid())
					mLumaRT[i].Shutdown();
			}
			mLumaWrite = -1;
			// Anneau de releve : les cases sont creees paresseusement, donc
			// certaines peuvent etre invalides — les detruire toutes serait faux.
			for (int i = 0; i < kLumaReadRing; i++) {
				if (mLumaReadBuf[i].IsValid())
					mDevice->DestroyBuffer(mLumaReadBuf[i]);
				mLumaReadBuf[i] = {};
			}
			mLumaReadCursor = 0;
			mLumaReadFilled = 0;
			mResolvedValid = false;
			mResolvedStaleFrames = 0;
			for (int i = 0; i < kAutoExpDescSets; i++) {
				if (mAutoExpSets[i].IsValid())
					mDevice->FreeDescriptorSet(mAutoExpSets[i]);
				mAutoExpSets[i] = {};
			}
			if (mAutoExpLayout.IsValid())
				mDevice->DestroyDescriptorSetLayout(mAutoExpLayout);
			mAutoExpLayout = {};
			if (mLUTTex.IsValid()) {
				mDevice->DestroyTexture(mLUTTex);
				mLUTTex = {};
			}
			for (int i = 0; i < kBloomMips; i++) {
				if (mBloomRT[i].IsValid())
					mBloomRT[i].Shutdown();
			}
			if (mSSAORT.IsValid())
				mSSAORT.Shutdown();
			if (mToneSet.IsValid())
				mDevice->FreeDescriptorSet(mToneSet);
			if (mToneLayout.IsValid())
				mDevice->DestroyDescriptorSetLayout(mToneLayout);
			for (int i = 0; i < kBloomDescSets; i++) {
				if (mBloomSets[i].IsValid()) {
					mDevice->FreeDescriptorSet(mBloomSets[i]);
					mBloomSets[i] = {};
				}
			}
			if (mInputTexSet.IsValid())
				mDevice->FreeDescriptorSet(mInputTexSet);
			if (mInputTexLayout.IsValid())
				mDevice->DestroyDescriptorSetLayout(mInputTexLayout);
			// Le shader handle est detenu par NkShaderLibrary, pas a detruire ici.
			mShaderTone = {};
		}

		void NkPostProcessStack::OnResize(uint32 w, uint32 h) {
			mW = w;
			mH = h;
			CreateTextures();
		}

		void NkPostProcessStack::CreateTextures() {
			if (!mTex || !mDevice)
				return;
			uint32 hw = mW / 2 ? mW / 2 : 1;
			uint32 hh = mH / 2 ? mH / 2 : 1;
			mSSAOTex = mTex->CreateRenderTarget(hw, hh, NkGPUFormat::NK_R8_UNORM, false, true, "SSAO");

			// Phase H.2 : bloom mipchain via NkRenderTarget. Chaque mip a son
			// propre render pass + framebuffer pour permettre BeginRender pendant
			// RunBloom. mBloomRT[0] est en W/2, [5] est en W/64.
			for (int i = 0; i < kBloomMips; i++) {
				uint32 div = 1u << (i + 1); // mip 0 = W/2, mip 5 = W/64
				uint32 bw = mW / div ? mW / div : 1;
				uint32 bh = mH / div ? mH / div : 1;
				NkRenderTargetDesc rtd;
				rtd.width = bw;
				rtd.height = bh;
				rtd.hdr = true;	   // RGBA16F : preserve HDR pour les bright spots
				rtd.depth = false; // pas besoin de depth pour les passes bloom
				char nameBuf[32];
				snprintf(nameBuf, sizeof(nameBuf), "BloomMip%d", i);
				rtd.name = NkString(nameBuf);
				// Note : Init() libere le RT precedent si re-init (utile pour OnResize)
				if (mBloomRT[i].IsValid())
					mBloomRT[i].Shutdown();
				mBloomRT[i].Init(mDevice, mTex, rtd);
			}

			// ── Auto-exposure V1 : deux cibles 1x1 RGBA16F, PERSISTANTES ──────────
			// Creees UNE SEULE FOIS (pas de Shutdown ici, contrairement au bloom) :
			// l'etat adapte doit survivre a un OnResize, sinon redimensionner la
			// fenetre remettrait l'exposition a zero et provoquerait un flash.
			for (int i = 0; i < 2; i++) {
				if (mLumaRT[i].IsValid())
					continue;
				NkRenderTargetDesc rtd;
				rtd.width = 1;
				rtd.height = 1;
				rtd.hdr = true; // RGBA16F : la luminance HDR depasse 1.0
				rtd.depth = false;
				rtd.name = (i == 0) ? NkString("AvgLuma0") : NkString("AvgLuma1");
				mLumaRT[i].Init(mDevice, mTex, rtd);
			}

			// NB : l'historique ping-pong du TAA n'est PAS cree ici — ce sont deux
			// transients du RenderGraph (cf. RunTAAInPass pour le pourquoi). Un
			// OnResize reconstruit le graph, donc les recree a la bonne taille et
			// repart sans historique : exactement le comportement voulu.

			mToneTex = mTex->CreateRenderTarget(mW, mH, NkGPUFormat::NK_RGBA8_UNORM, false, true, "Tone");
			mFinalTex = mTex->CreateRenderTarget(mW, mH, NkGPUFormat::NK_RGBA8_UNORM, false, true, "Final");

			// Phase H.3 : SSAO render target template (R8_UNORM, W/2 x H/2, no depth).
			// Sert juste a fournir un render pass pour la creation du pipeline SSAO ;
			// le rendu effectif passe par un transient du RenderGraph.
			{
				if (mSSAORT.IsValid())
					mSSAORT.Shutdown();
				NkRenderTargetDesc rtd;
				rtd.width = hw;
				rtd.height = hh;
				rtd.hdr = false;
				rtd.depth = false;
				rtd.colorFmt = NkGPUFormat::NK_R8_UNORM;
				rtd.name = NkString("SSAO_Template");
				mSSAORT.Init(mDevice, mTex, rtd);
			}
		}

		NkTexHandle NkPostProcessStack::Execute(NkICommandBuffer *cmd, NkTexHandle hdrIn, NkTexHandle depth,
												NkTexHandle velocity) {
			(void)velocity;
			NkTexHandle cur = hdrIn;
			if (mCfg.ssao)
				cur = RunSSAO(cmd, depth, NkTexHandle::Null());
			if (mCfg.bloom)
				cur = RunBloom(cmd, cur);
			if (mCfg.toneMapping)
				cur = RunTonemap(cmd, cur);
			if (mCfg.fxaa)
				cur = RunFXAA(cmd, cur);
			return cur;
		}

		// ── Helper : draw fullscreen quad sampling 'src' into current target ─────
		void NkPostProcessStack::DrawFullscreen(NkICommandBuffer *cmd, NkPipelineHandle pipe, NkTexHandle src,
												const void *pushConst, uint32 pcSize) {
			if (!cmd || !pipe.IsValid() || !mMesh)
				return;
			cmd->BindGraphicsPipeline(pipe);

			// Bind l'input texture au descriptor set (binding 0). On utilise le
			// sampler linear-clamp de NkResources (typique pour les passes PP).
			if (mInputTexSet.IsValid() && src.IsValid() && mTex && mResources) {
				NkTextureHandle rhi = mTex->GetRHIHandle(src);
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

		NkTexHandle NkPostProcessStack::RunSSAO(NkICommandBuffer *cmd, NkTexHandle depth, NkTexHandle normal) {
			struct PC {
					float invResW, invResH, radius, bias;
			} pc;

			pc.invResW = 1.0f / (float)(mW > 0 ? mW : 1);
			pc.invResH = 1.0f / (float)(mH > 0 ? mH : 1);
			pc.radius = mCfg.ssaoRadius;
			pc.bias = mCfg.ssaoBias;
			DrawFullscreen(cmd, mPipeSSAO, depth, &pc, sizeof(pc));
			(void)normal;
			return mSSAOTex;
		}

		NkTexHandle NkPostProcessStack::RunBloom(NkICommandBuffer *cmd, NkTexHandle hdr) {
			// Phase H.2 : RunBloom() legacy n'est plus le path actif. Le bloom
			// multi-pass est maintenant orchestre par NkRendererImpl::BuildDefault-
			// RenderGraph qui ajoute les passes Bloom_Down[0..4] + Bloom_Up[0..4]
			// au RG avec transients RGBA16F, puis appelle DrawBloomDownPass /
			// DrawBloomUpPass dans chaque pass.
			// Cette methode reste pour compat avec Execute() (legacy non-RG path).
			(void)cmd;
			(void)hdr;
			return NkTexHandle::Null();
		}

		NkTexHandle NkPostProcessStack::RunTonemap(NkICommandBuffer *cmd, NkTexHandle hdr) {
			// PC[0]=(exposure,gamma,vignette,sat) PC[1]=(bloomStr,bloomThr,invW,invH)
			struct PC {
					float exposure, gamma, vignetteIntens, saturation;
					float bloomStr, bloomThr, invW, invH;
			} pc;

			pc.exposure = mCfg.exposure;
			// Gamma basé sur le format swapchain (sRGB→1.0, UNORM→manuel), cf. ExecuteRHI.
			pc.gamma = (mDevice && mDevice->IsSwapchainSrgb()) ? 1.0f : mCfg.gamma;
			pc.vignetteIntens = mCfg.vignette ? mCfg.vignetteIntens : 0.0f;
			pc.saturation = mCfg.colorGrading ? mCfg.saturation : 1.0f;
			pc.bloomStr = mCfg.bloom ? mCfg.bloomStrength : 0.0f;
			pc.bloomThr = mCfg.bloom ? mCfg.bloomThreshold : 1.0f;
			pc.invW = mW > 0 ? 1.0f / (float)mW : 0.f;
			pc.invH = mH > 0 ? 1.0f / (float)mH : 0.f;
			DrawFullscreen(cmd, mPipeTone, hdr, &pc, sizeof(pc));
			return mToneTex;
		}

		void NkPostProcessStack::ExecuteRHI(NkICommandBuffer *cmd, NkTextureHandle hdrIn, NkTextureHandle bloomTex,
											NkTextureHandle ssaoTex) {
			if (!cmd || !mPipeTone.IsValid() || !mToneSet.IsValid() || !hdrIn.IsValid())
				return;

			NkSamplerHandle samp = mResources ? mResources->GetSamplerLinearClamp() : NkSamplerHandle{};
			if (!samp.IsValid())
				return;

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
				// Fallback : pas de SSAO — BLANC 1x1, PAS l'HDR. Le shader
				// multiplie INCONDITIONNELLEMENT par le canal rouge de ce
				// binding (`hdr *= ao`) : avec l'HDR en repli, l'image etait
				// multipliee par elle-meme — SSAO coupee ASSOMBRISSAIT quand
				// meme, et le bouton Actif du panneau semblait sans effet
				// (constate par Rihen le 9 aout). Blanc = 1.0 = vraie identite.
				NkTextureHandle white = mResources ? mResources->GetWhiteTex() : NkTextureHandle{};
				mDevice->BindTextureSampler(mToneSet, 2, white.IsValid() ? white : hdrIn, samp);
			}
			// Phase L : bind le LUT 3D (sampler3D au binding=3). Si pas alloue ou
			// strength=0 le shader skip. Fallback : bind l'HDR pour eviter undefined.
			if (mLUTTex.IsValid()) {
				mDevice->BindTextureSampler(mToneSet, 3, mLUTTex, samp);
			} else {
				mDevice->BindTextureSampler(mToneSet, 3, hdrIn, samp);
			}
			// Auto-exposure V1 : cible 1x1 de luminance moyenne au binding 4. Si la
			// passe n'a pas tourne (auto-exposure off), on binde le HDR : le shader
			// ignore ce slot des que autoExpStrength vaut 0.
			{
				NkTextureHandle lumaTex = GetAvgLumaTexRHI();
				mDevice->BindTextureSampler(mToneSet, 4, lumaTex.IsValid() ? lumaTex : hdrIn, samp);
			}
			cmd->BindGraphicsPipeline(mPipeTone);
			cmd->BindDescriptorSet(mToneSet, 0);

			// Layout PC : 32 bytes
			//   PC[0] = (exposure, gamma, vignetteIntens, saturation)
			//   PC[1] = (bloomStrength, lutStrength, yFlipUV, lutSize)
			// Phase L : lutStrength remplace bloomThreshold (inutilise apres le 1er
			// downsample). lutSize en p1.w pour le bias texel correct cote shader.
			// Gamma : si le swapchain encode déjà le sRGB (gamma auto à la présentation),
			// pc.gamma=1.0 ; sinon (UNORM) on applique le gamma manuel. Basé sur le FORMAT
			// swapchain (réglage global NkSwapchainFormat), PAS sur le backend — sinon VK en
			// UNORM resterait trop sombre (pas de gamma) vs GL.
			const bool swapchainSrgb = mDevice && mDevice->IsSwapchainSrgb();
			// Garde-fou HDR (cf. NkPostConfig::hdrSafetyClamp). Override runtime via
			// NK_HDR_CLAMP (valeur en float ; "0" ou negatif = desactive). Lu une fois.
			float32 hdrClamp = mCfg.hdrSafetyClamp;
			{
				static int sInit = 0;
				static float32 sEnv = 0.f;
				static bool sHasEnv = false;
				if (!sInit) {
					sInit = 1;
					const char *v = getenv("NK_HDR_CLAMP");
					if (v && v[0]) {
						sHasEnv = true;
						sEnv = (float32)atof(v);
					}
				}
				if (sHasEnv)
					hdrClamp = sEnv;
			}
			// <= 0 -> desactive : on passe une borne enorme que le shader traite comme "off".
			const float32 hdrClampValue = (hdrClamp > 0.f) ? hdrClamp : 0.f;
			// Exposure de base : override runtime NK_EXPOSURE (sert a prouver l'effet de
			// l'auto-exposure — une base volontairement fausse doit etre corrigee par
			// la mesure ; cf. bloc de test dans la ROADMAP Phase L).
			float32 baseExposure = mCfg.exposure;
			{
				static int sInit = 0;
				static bool sHasEnv = false;
				static float32 sEnv = 1.f;
				if (!sInit) {
					sInit = 1;
					const char *v = getenv("NK_EXPOSURE");
					if (v && v[0]) {
						sHasEnv = true;
						sEnv = (float32)atof(v);
					}
				}
				if (sHasEnv)
					baseExposure = sEnv;
			}
			float32 pc[16] = {// p0 = (exposure, gamma, vignetteIntens, saturation)
							  baseExposure, swapchainSrgb ? 1.0f : mCfg.gamma,
							  mCfg.vignette ? mCfg.vignetteIntens : 0.f, mCfg.colorGrading ? mCfg.saturation : 1.f,
							  // p1 = (bloomStrength, lutStrength, yFlipUV, lutSize)
							  mCfg.bloom ? mCfg.bloomStrength : 0.f, mCfg.colorGrading ? mCfg.lutStrength : 0.f,
							  // yFlipUV = -1 dans les deux backends : le HDR transient (FBO
							  // custom) est stocke en convention Y-down dans VK ET GL (le RHI
							  // OpenGL flip son viewport pour les FBO custom afin de matcher
							  // la convention storage Vulkan). Le top screen rendu = ligne 0
							  // du storage -> UV.y = 0 pour sampler -> flip via VS.
							  -1.f, float32(mLUTSize),
							  // p2 = (autoExposureStrength, autoExposureKey, bloomYFlip, hdrClamp)
							  // Force effective = config OU override NK_AUTOEXP, et 0 si la
							  // passe de mesure n'a pas tourne (sinon le shader lirait le HDR
							  // bindé en repli au binding 4 et calculerait une exposition folle).
							  (mLumaWrite >= 0) ? NkAutoExpStrength(mCfg.autoExposureStrength) : 0.f,
							  mCfg.autoExposureKey,
							  // bloomYFlip : 1 sur DX (bloom/SSAO stockés Y-up vs HDR Y-down → V opposé),
							  // 0 sur VK/GL (bloom/SSAO et HDR partagent la même convention V). Corrige le
							  // glow "ghost" miroir vertical sur DX.
							  ((mDevice && (mDevice->GetApi() == NkGraphicsApi::NK_GFX_API_DX11 ||
											mDevice->GetApi() == NkGraphicsApi::NK_GFX_API_DX12))
								   ? 1.f
								   : 0.f),
							  // p2.w = garde-fou HDR (0 = desactive). Le tonemap clampe le HDR avant ACES.
							  hdrClampValue,
							  // p3 = (exposureMin, exposureMax, _, _) : bornes de l'exposure
							  // calculee par l'auto-exposure (key / luminance mesuree).
							  mCfg.autoExposureMinExp, mCfg.autoExposureMaxExp, 0.f, 0.f};
			// Push avec NK_ALL_GRAPHICS pour matcher la range pipeline (cf. fix
			// VUID-vkCmdPushConstants-offset-01796 — VS lit yFlipUV au slot PC[1].z).
			cmd->PushConstants(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, sizeof(pc), pc);

			// Fullscreen triangle : 3 verts sans VBO.
			cmd->Draw(3, 1, 0, 0);
		}

		// Phase L FXAA wirage RenderGraph : version "ExecuteFXAA" sans alloc
		// texture intermediate. Le RG bind deja le swapchain comme RT, on lit
		// ldrIn (mToneTex via texture handle) au binding=0, draw fullscreen.
		void NkPostProcessStack::ExecuteFXAA(NkICommandBuffer *cmd, NkTextureHandle ldrIn) {
			if (!cmd || !mPipeFXAA.IsValid() || !ldrIn.IsValid())
				return;

			// Bind input texture (mToneTex sample) au binding=0 du PP_FXAA.
			if (mInputTexSet.IsValid() && mResources) {
				NkSamplerHandle samp = mResources->GetSamplerLinearClamp();
				mDevice->BindTextureSampler(mInputTexSet, 0, ldrIn, samp);
			}
			cmd->BindGraphicsPipeline(mPipeFXAA);
			// FXAA shader uses set=0 binding=0 (cf. pp_fxaa.frag.vk.glsl).
			if (mInputTexSet.IsValid())
				cmd->BindDescriptorSet(mInputTexSet, 0);

			// Push 16 bytes : (invResW, invResH, yFlipUV, _pad). Stage ALL_GRAPHICS
			// pour matcher la range pipeline.
			// yFlipUV : sur Vulkan le viewport est Y-flipped (storage transient
			// top-down), donc on flip l'UV cote VS pour matcher. Sur OpenGL le
			// viewport n'est pas flipped et le transient FBO est aussi top-down,
			// mais l'output vers swapchain doit etre flippe -> on garde UV direct.
			// (Convention oppose au tonemap qui ecrit direct au swapchain).
			const bool isVK = mDevice && mDevice->GetApi() == NkGraphicsApi::NK_GFX_API_VULKAN;

			struct PC {
					float invResW, invResH, yFlipUV, _pad;
			} pc;

			pc.invResW = 1.0f / (float)(mW > 0 ? mW : 1);
			pc.invResH = 1.0f / (float)(mH > 0 ? mH : 1);
			pc.yFlipUV = isVK ? -1.f : +1.f;
			pc._pad = 0.f;
			cmd->PushConstants(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, sizeof(pc), &pc);
			cmd->Draw(3, 1, 0, 0);
		}

		void NkPostProcessStack::ExecuteBlit(NkICommandBuffer *cmd, NkTextureHandle src) {
			if (!cmd || !mPipeBlit.IsValid() || !src.IsValid())
				return;

			// Bind la texture source au binding=0 sur le set DEDIE (cf. header :
			// mInputTexSet est ecrase au Submit sur les backends differes).
			if (mBlitTexSet.IsValid() && mResources) {
				NkSamplerHandle samp = mResources->GetSamplerLinearClamp();
				mDevice->BindTextureSampler(mBlitTexSet, 0, src, samp);
			}
			cmd->BindGraphicsPipeline(mPipeBlit);
			if (mBlitTexSet.IsValid())
				cmd->BindDescriptorSet(mBlitTexSet, 0);

			// Flip UV pour la sortie ECRAN : la source est un RT offscreen ecrit
			// par le pipeline 3D. Sur DX11/DX12 (RT Y-down, VS HLSL Y-negate) et
			// VK, l'affichage direct vers le swapchain est inverse -> flip.
			// Sur GL (origine bas-gauche partout) l'UV directe est correcte.
			// (Confirme a l'ecran par Rihen : DX11 inverse sans ce flip.)
			const bool isVK = mDevice && mDevice->GetApi() != NkGraphicsApi::NK_GFX_API_OPENGL;

			struct PC {
					float invResW, invResH, yFlipUV, _pad;
			} pc;

			pc.invResW = 1.0f / (float)(mW > 0 ? mW : 1);
			pc.invResH = 1.0f / (float)(mH > 0 ? mH : 1);
			pc.yFlipUV = isVK ? -1.f : +1.f;
			pc._pad = 0.f;
			cmd->PushConstants(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, sizeof(pc), &pc);
			cmd->Draw(3, 1, 0, 0);
		}

		// Copie vers une cible off-screen du graph (passe TAA_Store) : meme shader
		// et meme convention de flip qu'ExecuteBlit, mais pipeline RP-compatible
		// avec le framebuffer transient et descriptor set distinct (deux blits
		// coexistent dans la frame : ecran + historique).
		void NkPostProcessStack::ExecuteBlitToRT(NkICommandBuffer *cmd, NkTextureHandle src,
												 NkRenderPassHandle rp) {
			if (!cmd || !src.IsValid() || !mShaderBlit.IsValid() || !mDevice)
				return;
			if (!mPipeBlitRT.IsValid()) {
				NkGraphicsPipelineDesc pd;
				pd.shader = mShaderBlit;
				pd.depthStencil = NkDepthStencilDesc::NoDepth();
				pd.rasterizer = NkRasterizerDesc::NoCull();
				pd.blend = NkBlendDesc::Opaque();
				pd.debugName = "Blit_RT";
				pd.renderPass = rp;
				pd.AddPushConstant(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, 16);
				if (mInputTexLayout.IsValid())
					pd.descriptorSetLayouts.PushBack(mInputTexLayout);
				mPipeBlitRT = mDevice->CreateGraphicsPipeline(pd);
				logger.Info("[NkPostProcessStack] Blit_RT pipeline (lazy) : valid={0}\n",
							mPipeBlitRT.IsValid() ? 1 : 0);
			}
			if (!mPipeBlitRT.IsValid())
				return;
			if (!mBlitRTSet.IsValid() && mInputTexLayout.IsValid())
				mBlitRTSet = mDevice->AllocateDescriptorSet(mInputTexLayout);
			if (mBlitRTSet.IsValid() && mResources)
				mDevice->BindTextureSampler(mBlitRTSet, 0, src, mResources->GetSamplerLinearClamp());
			cmd->BindGraphicsPipeline(mPipeBlitRT);
			if (mBlitRTSet.IsValid())
				cmd->BindDescriptorSet(mBlitRTSet, 0);

			struct PC {
					float invResW, invResH, yFlipUV, _pad;
			} pc;

			pc.invResW = 1.0f / (float)(mW > 0 ? mW : 1);
			pc.invResH = 1.0f / (float)(mH > 0 ? mH : 1);
			// ⚠️ Cette copie doit PRESERVER l'orientation : l'historique est relu par
			// la passe TAA avec la meme convention qu'elle applique au LDR, donc les
			// deux doivent etre orientes pareil. La regle est celle de la LECTURE
			// d'une cible off-screen (`isVK ? -1 : +1`), et NON celle du blit vers
			// l'ECRAN (`api != GL ? -1 : +1`) : sur DX les deux divergent, et avoir
			// pris la seconde inversait l'historique. Symptome mesure, instructif car
			// silencieux : luminance correcte a 0,6 % pres (le clamp de voisinage
			// ramenait l'historique inverse dans la plage locale) mais AUCUN
			// antialiasing — l'indicateur d'escalier montait a 51,9 % au lieu de
			// descendre a ~42 %, soit exactement le niveau du jitter sans accumulation.
			pc.yFlipUV = (mDevice->GetApi() == NkGraphicsApi::NK_GFX_API_VULKAN) ? -1.f : +1.f;
			pc._pad = 0.f;
			cmd->PushConstants(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, sizeof(pc), &pc);
			cmd->Draw(3, 1, 0, 0);
		}

		// Phase L : upload LUT 3D utilisateur (cf. header pour le layout voxel).
		// Recree la texture si la taille change ; le bind au binding=3 du tonemap
		// est refait chaque ExecuteRHI -> la nouvelle texture est prise en compte
		// a la frame suivante sans autre plomberie.
		bool NkPostProcessStack::SetColorGradingLUT(const uint8 *rgba, uint32 size) {
			if (!mDevice || !rgba || size < 2 || size > 64)
				return false;
			if (mLUTTex.IsValid() && mLUTSize != size) {
				// La LUT courante peut etre referencee par une frame en vol :
				// operation de setup rare -> WaitIdle avant destruction.
				mDevice->WaitIdle();
				mDevice->DestroyTexture(mLUTTex);
				mLUTTex = {};
			}
			if (!mLUTTex.IsValid()) {
				auto td = NkTextureDesc::Tex3D(size, size, size, NkGPUFormat::NK_RGBA8_UNORM);
				td.debugName = "ColorGradingLUT_User";
				mLUTTex = mDevice->CreateTexture(td);
				if (!mLUTTex.IsValid())
					return false;
			}
			mLUTSize = size;
			mCfg.lutSize = size;
			mDevice->WriteTextureRegion(mLUTTex, rgba, 0, 0, 0, size, size, size, 0, 0, 0);
			return true;
		}

		NkTexHandle NkPostProcessStack::RunFXAA(NkICommandBuffer *cmd, NkTexHandle ldr) {
			// Phase L : push 16 bytes (invResW, invResH, yFlipUV, _pad) pour matcher
			// le pipeline range NK_ALL_GRAPHICS du VS+FS (cf. pp_fxaa.{vert,frag}.vk.glsl).
			struct PC {
					float invResW, invResH, yFlipUV, _pad;
			} pc;

			pc.invResW = 1.0f / (float)(mW > 0 ? mW : 1);
			pc.invResH = 1.0f / (float)(mH > 0 ? mH : 1);
			pc.yFlipUV = -1.f; // FBO custom : Y-flip pour matcher la convention
			pc._pad = 0.f;
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
		static NkDescSetHandle NextBloomSet(NkDescSetHandle (&pool)[33], int &cursor) {
			NkDescSetHandle h = pool[cursor % 33];
			cursor++;
			return h;
		}

		void NkPostProcessStack::DrawBloomDownPass(NkICommandBuffer *cmd, NkTextureHandle src, uint32 srcW, uint32 srcH,
												   float threshold) {
			if (!cmd || !mPipeBloomDown.IsValid() || !src.IsValid())
				return;

			NkSamplerHandle samp = mResources ? mResources->GetSamplerLinearClamp() : NkSamplerHandle{};
			if (!samp.IsValid())
				return;

			NkDescSetHandle set = NextBloomSet(mBloomSets, mBloomSetCursor);
			if (!set.IsValid())
				return;

			mDevice->BindTextureSampler(set, 0, src, samp);
			cmd->BindGraphicsPipeline(mPipeBloomDown);
			cmd->BindDescriptorSet(set, 0);

			struct PC {
					float invW, invH, threshold, yFlipUV;
			} pc;

			pc.invW = srcW > 0 ? 1.0f / (float)srcW : 0.f;
			pc.invH = srcH > 0 ? 1.0f / (float)srcH : 0.f;
			pc.threshold = threshold;
			// Sub-passes bloom : NE PAS flipper en GL (FBO custom = Y-up storage
			// natif). Sinon bloomMip storage decale par rapport au HDR storage
			// -> tonemap sample bloom et HDR a des conventions differentes ->
			// bloom mal positionne. En VK le storage est Y-down natif, flip OK.
			bool isVK = mDevice && mDevice->GetApi() == NkGraphicsApi::NK_GFX_API_VULKAN;
			pc.yFlipUV = isVK ? -1.f : +1.f;
			cmd->PushConstants(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, sizeof(pc), &pc);

			// Fullscreen triangle : 3 verts sans VBO.
			cmd->Draw(3, 1, 0, 0);
		}

		void NkPostProcessStack::DrawBloomUpPass(NkICommandBuffer *cmd, NkTextureHandle src, uint32 srcW, uint32 srcH,
												 float strength) {
			if (!cmd || !mPipeBloomUp.IsValid() || !src.IsValid())
				return;

			NkSamplerHandle samp = mResources ? mResources->GetSamplerLinearClamp() : NkSamplerHandle{};
			if (!samp.IsValid())
				return;

			NkDescSetHandle set = NextBloomSet(mBloomSets, mBloomSetCursor);
			if (!set.IsValid())
				return;

			mDevice->BindTextureSampler(set, 0, src, samp);
			cmd->BindGraphicsPipeline(mPipeBloomUp);
			cmd->BindDescriptorSet(set, 0);

			struct PC {
					float invW, invH, strength, yFlipUV;
			} pc;

			pc.invW = srcW > 0 ? 1.0f / (float)srcW : 0.f;
			pc.invH = srcH > 0 ? 1.0f / (float)srcH : 0.f;
			pc.strength = strength;
			// Sub-passes bloom : pas de flip en GL (cf. DrawBloomDownPass).
			bool isVK = mDevice && mDevice->GetApi() == NkGraphicsApi::NK_GFX_API_VULKAN;
			pc.yFlipUV = isVK ? -1.f : +1.f;
			cmd->PushConstants(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, sizeof(pc), &pc);

			// Fullscreen triangle : 3 verts sans VBO.
			cmd->Draw(3, 1, 0, 0);
		}

		// ── Phase H.3 : SSAO sub-pass (v1 « Alchemy », 2026-08-09) ──────────────
		// Le RG appelle cette methode dans une pass deja ouverte (color attachment
		// = ssaoTex transient R8_UNORM, depthSrc = mainDepth transient).
		// On bind le depth comme sampler au binding=0 et draw fullscreen triangle.
		void NkPostProcessStack::DrawSSAOPass(NkICommandBuffer *cmd, NkTextureHandle depthSrc, uint32 ssaoW,
											  uint32 ssaoH, const NkSSAOFrame &frame) {
			if (!cmd || !mPipeSSAO.IsValid() || !depthSrc.IsValid())
				return;

			// NEAREST, pas linear : un sampler lineaire sur de la profondeur
			// INVENTE des profondeurs intermediaires aux silhouettes — la
			// position monde reconstruite y devient un point qui n'existe
			// nulle part, et la normale croisee part en vrille.
			NkSamplerHandle samp = mResources ? mResources->GetSamplerNearestClamp() : NkSamplerHandle{};
			if (!samp.IsValid())
				return;

			// Reutilise le pool rotatif bloom (a renommer en pool generic PP plus
			// tard) — depth bound au binding=0 du set frais.
			NkDescSetHandle set = mBloomSets[mBloomSetCursor % kBloomDescSets];
			mBloomSetCursor++;
			if (!set.IsValid())
				return;

			mDevice->BindTextureSampler(set, 0, depthSrc, samp);
			cmd->BindGraphicsPipeline(mPipeSSAO);
			cmd->BindDescriptorSet(set, 0);

			// ── Conventions Y PAR BACKEND — les memes que le TAA, qui les a
			// MESUREES a l'ecran (cf. RunTAAInPass) :
			//   yFlipUV  (VS) : orientation de la cible lue/ecrite. VK -1, GL/DX +1.
			//   ndcYSign (FS) : signe reliant vUV.y au NDC Y pour reconstruire la
			//                   position depuis la profondeur. DX +1, GL/VK -1.
			const NkGraphicsApi api = mDevice ? mDevice->GetApi() : NkGraphicsApi::NK_GFX_API_OPENGL;
			const bool isVK = (api == NkGraphicsApi::NK_GFX_API_VULKAN);
			const bool isDX = (api == NkGraphicsApi::NK_GFX_API_DX11 || api == NkGraphicsApi::NK_GFX_API_DX12);

			// Miroir exact du bloc push_constant des shaders (128 octets).
			struct PC {
					float32 invViewProj[16];
					float32 camPos[4]; // .w = focalY
					float32 camFwd[4]; // .w = aspect
					float32 p0[4];	   // invResW, invResH, rayonMonde, intensite
					float32 p1[4];	   // yFlipUV, ndcYSign, biaisMonde, orthoFlag
			} pc;
			static_assert(sizeof(PC) == 128, "PC SSAO doit faire 128 octets (pipeline + shaders)");

			memcpy(pc.invViewProj, &frame.invViewProj, sizeof(pc.invViewProj));
			pc.camPos[0] = frame.camPos.x;
			pc.camPos[1] = frame.camPos.y;
			pc.camPos[2] = frame.camPos.z;
			pc.camPos[3] = frame.focalY;
			pc.camFwd[0] = frame.camFwd.x;
			pc.camFwd[1] = frame.camFwd.y;
			pc.camFwd[2] = frame.camFwd.z;
			pc.camFwd[3] = frame.aspect > 0.f ? frame.aspect : 1.f;
			pc.p0[0] = ssaoW > 0 ? 1.0f / (float32)ssaoW : 0.f;
			pc.p0[1] = ssaoH > 0 ? 1.0f / (float32)ssaoH : 0.f;
			// Rayon et biais en METRES (v1) — plus le rayon UV de la v0 : la
			// meme config donne la meme AO quelle que soit la distance camera.
			pc.p0[2] = mCfg.ssaoRadius > 0.f ? mCfg.ssaoRadius : 0.5f;
			pc.p0[3] = mCfg.ssaoIntensity < 0.f ? 0.f : mCfg.ssaoIntensity;
			pc.p1[0] = isVK ? -1.f : +1.f;
			pc.p1[1] = isDX ? +1.f : -1.f;
			pc.p1[2] = mCfg.ssaoBias > 0.f ? mCfg.ssaoBias : 0.025f;
			pc.p1[3] = frame.ortho ? 1.f : 0.f;
			cmd->PushConstants(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, sizeof(pc), &pc);

			cmd->Draw(3, 1, 0, 0);
		}

		// ── Phase H.5b : SSAO Blur sub-pass (denoise) ───────────────────────────
		void NkPostProcessStack::DrawSSAOBlurPass(NkICommandBuffer *cmd, NkTextureHandle aoSrc, uint32 ssaoW,
												  uint32 ssaoH) {
			if (!cmd || !mPipeSSAOBlur.IsValid() || !aoSrc.IsValid())
				return;

			NkSamplerHandle samp = mResources ? mResources->GetSamplerLinearClamp() : NkSamplerHandle{};
			if (!samp.IsValid())
				return;

			NkDescSetHandle set = mBloomSets[mBloomSetCursor % kBloomDescSets];
			mBloomSetCursor++;
			if (!set.IsValid())
				return;

			mDevice->BindTextureSampler(set, 0, aoSrc, samp);
			cmd->BindGraphicsPipeline(mPipeSSAOBlur);
			cmd->BindDescriptorSet(set, 0);

			bool isVK = mDevice && mDevice->GetApi() == NkGraphicsApi::NK_GFX_API_VULKAN;

			struct PC {
					float invW, invH, yFlipUV, _pad;
			} pc;

			pc.invW = ssaoW > 0 ? 1.0f / (float)ssaoW : 0.f;
			pc.invH = ssaoH > 0 ? 1.0f / (float)ssaoH : 0.f;
			pc.yFlipUV = isVK ? -1.f : +1.f;
			pc._pad = 0.f;
			cmd->PushConstants(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, sizeof(pc), &pc);

			cmd->Draw(3, 1, 0, 0);
		}

		// =====================================================================
		// Phase L V1 — AUTO-EXPOSURE (2026-07-30)
		// =====================================================================
		// Force d'auto-exposure effective : config, ou override d'environnement
		// NK_AUTOEXP (pratique pour un A/B sans recompiler ni modifier une app —
		// meme motif que NK_HDR_CLAMP dans ExecuteRHI). Lu une seule fois.
		static float32 NkAutoExpStrength(float32 fromConfig) {
			static int sInit = 0;
			static bool sHasEnv = false;
			static float32 sEnv = 0.f;
			if (!sInit) {
				sInit = 1;
				const char *v = getenv("NK_AUTOEXP");
				if (v && v[0]) {
					sHasEnv = true;
					sEnv = (float32)atof(v);
				}
			}
			return sHasEnv ? sEnv : fromConfig;
		}

		static float32 NkAutoExpSpeed(float32 fromConfig) {
			static int sInit = 0;
			static bool sHasEnv = false;
			static float32 sEnv = 0.f;
			if (!sInit) {
				sInit = 1;
				const char *v = getenv("NK_AUTOEXP_SPEED");
				if (v && v[0]) {
					sHasEnv = true;
					sEnv = (float32)atof(v);
				}
			}
			return sHasEnv ? sEnv : fromConfig;
		}

		bool NkPostProcessStack::IsAutoExposureEnabled() const {
			if (!mPipeAutoExp.IsValid() || !mLumaRT[0].IsValid() || !mLumaRT[1].IsValid())
				return false;
			return NkAutoExpStrength(mCfg.autoExposureStrength) > NkPostConfig::kAutoExposureOn;
		}

		NkTextureHandle NkPostProcessStack::GetAvgLumaTexRHI() const {
			if (mLumaWrite < 0 || !mTex)
				return NkTextureHandle{};
			return mTex->GetRHIHandle(mLumaRT[mLumaWrite].GetColorHandle());
		}

		void NkPostProcessStack::RunAutoExposure(NkICommandBuffer *cmd, NkTextureHandle hdrIn,
												 NkTextureHandle bloomIn, float32 bloomStrength,
												 float32 dtSeconds) {
			if (!cmd || !hdrIn.IsValid() || !IsAutoExposureEnabled())
				return;
			NkSamplerHandle samp = mResources ? mResources->GetSamplerLinearClamp() : NkSamplerHandle{};
			if (!samp.IsValid())
				return;

			// dt : mesure interne si l'appelant n'en fournit pas. La premiere frame
			// n'a pas de delta significatif -> 0 (le shader prend alors la cible
			// directement puisque l'etat precedent est nul).
			float32 dt = dtSeconds;
			if (dt < 0.f) {
				if (!mAutoExpClockStarted) {
					// Premiere frame : on ARME l'horloge, la duree rendue par
					// Reset() ne mesure encore rien -- ecartee explicitement.
					(void)mAutoExpClock.Reset();
					mAutoExpClockStarted = true;
					dt = 0.f;
				} else {
					dt = (float32)mAutoExpClock.Reset().ToSeconds();
					// Garde-fou : un hoquet (chargement, breakpoint) ne doit pas
					// teleporter l'adaptation.
					if (dt > 0.25f)
						dt = 0.25f;
				}
			}

			// Ping-pong : on ECRIT dans l'index libre, on LIT celui de la frame
			// precedente. Premiere execution : la cible de lecture n'a jamais ete
			// rendue, elle vaut 0 -> le shader detecte l'amorcage (prev <= 0).
			const int write = (mLumaWrite < 0) ? 0 : (1 - mLumaWrite);
			const int read = 1 - write;

			NkTextureHandle prevTex = mTex ? mTex->GetRHIHandle(mLumaRT[read].GetColorHandle()) : NkTextureHandle{};

			NkDescSetHandle set = mAutoExpSets[mAutoExpSetCursor % kAutoExpDescSets];
			mAutoExpSetCursor++;
			if (!set.IsValid())
				return;
			mDevice->BindTextureSampler(set, 0, hdrIn, samp);
			// prevTex invalide au premier passage : on binde le HDR pour ne pas
			// laisser un slot non initialise (UB sur certains backends). Le shader
			// ignore la valeur puisqu'elle ne sera pas <= 0 dans ce cas ; d'ou le
			// clear explicite ci-dessous a la premiere frame.
			mDevice->BindTextureSampler(set, 1, prevTex.IsValid() ? prevTex : hdrIn, samp);
			// Le halo. Absent (bloom eteint) : on rebinde le HDR pour ne pas laisser
			// un slot vide, et hasBloom=0 dit au shader de l'ignorer.
			const bool aeHasBloom = bloomIn.IsValid() && bloomStrength > 0.f;
			mDevice->BindTextureSampler(set, 2, aeHasBloom ? bloomIn : hdrIn, samp);

			// La cible 1x1 n'est PAS un transient du RenderGraph : on gere son
			// render pass ici (meme modele que la passe d'ombres).
			// clear a 0 UNIQUEMENT au premier passage : sinon on effacerait l'etat
			// precedent... qui est justement dans l'AUTRE cible, donc le clear est
			// inoffensif — on le garde a 0 pour un demarrage deterministe.
			mLumaRT[write].BeginRender(cmd, NkVec4f{0.f, 0.f, 0.f, 1.f}, false);
			cmd->BindGraphicsPipeline(mPipeAutoExp);
			cmd->BindDescriptorSet(set, 0);

			struct PC {
					float32 dt, speed, minLuma, maxLuma;
					float32 bloomStr, hasBloom, pad0, pad1;
			} pc;

			pc.dt = (mLumaWrite < 0) ? 0.f : dt; // amorcage : saut direct sur la cible
			pc.speed = NkAutoExpSpeed(mCfg.autoExposureSpeed);
			pc.minLuma = mCfg.autoExposureMinLuma > 0.f ? mCfg.autoExposureMinLuma : 0.0001f;
			pc.maxLuma = mCfg.autoExposureMaxLuma > pc.minLuma ? mCfg.autoExposureMaxLuma : 8.f;
			pc.bloomStr = aeHasBloom ? bloomStrength : 0.f;
			pc.hasBloom = aeHasBloom ? 1.f : 0.f;
			pc.pad0 = 0.f;
			pc.pad1 = 0.f;
			cmd->PushConstants(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, sizeof(pc), &pc);
			cmd->Draw(3, 1, 0, 0);
			mLumaRT[write].EndRender(cmd);

			mLumaWrite = write;

			// Le releve suit la mesure : la cible qu'on vient d'ecrire part vers
			// la case courante de l'anneau, on lira celle d'il y a deux frames.
			PumpExposureReadback(cmd);
		}

		// ── ANNEAU DE RELEVE : L'EXPOSITION REELLE REDESCEND VERS LE CPU ─────
		// Demi-flottant -> flottant. La cible de luminance est RGBA16F : la
		// valeur utile est le premier canal, sur 16 bits. Decode ici plutot que
		// d'aller chercher une dependance pour dix lignes.
		static float32 NkHalfToFloat(uint16 h) {
			const uint32 s = (uint32)(h >> 15) & 0x1u;
			const uint32 e = (uint32)(h >> 10) & 0x1Fu;
			const uint32 m = (uint32)h & 0x3FFu;
			uint32 bits;
			if (e == 0) {
				if (m == 0) {
					bits = s << 31; // +/- zero
				} else {
					// Sous-normal : on normalise a la main.
					uint32 ee = 0, mm = m;
					while ((mm & 0x400u) == 0) {
						mm <<= 1;
						ee++;
					}
					mm &= 0x3FFu;
					bits = (s << 31) | ((127 - 15 - ee + 1) << 23) | (mm << 13);
				}
			} else if (e == 31) {
				bits = (s << 31) | 0x7F800000u | (m << 13); // inf / NaN
			} else {
				bits = (s << 31) | ((e + 127 - 15) << 23) | (m << 13);
			}
			float32 f;
			memcpy(&f, &bits, sizeof(f));
			return f;
		}

		void NkPostProcessStack::PumpExposureReadback(NkICommandBuffer *cmd) {
			// CETTE FONCTION NE TOURNE QUE SOUS AUTO ACTIVE, et ce n'est pas une
			// hypothese : son unique appelant est RunAutoExposure, qui retourne
			// avant si !IsAutoExposureEnabled(). Le cas « auto eteinte » est donc
			// traite dans ResolvedExposure(), la ou la question est posee.
			//
			// IL Y AVAIT ICI UNE BRANCHE « auto eteinte », annoncee « deliberee,
			// pas un cas degrade » : elle etait INATTEIGNABLE, et son commentaire
			// est precisement ce qui a dissuade d'aller verifier qui l'appelait.
			// La garde ecrite pour poser l'exposition manuelle etait morte, donc
			// personne ne la posait -- c'est la cause du cas 4.
			if (!cmd || !mDevice || mLumaWrite < 0 || mLumaReadDisabled)
				return;
			NkTextureHandle src = GetAvgLumaTexRHI();
			if (!src.IsValid())
				return;

			// Creation paresseuse des trois cases. 256 octets : un 1x1 RGBA16F
			// tient sur 8, mais les alignements de ligne des dorsales sont plus
			// larges et une case trop juste tronquerait la copie en silence.
			for (int i = 0; i < kLumaReadRing; i++) {
				if (!mLumaReadBuf[i].IsValid()) {
					// NkBufferDesc::Staging, PAS un desc construit a la main : un
					// tampon D3D11 en USAGE_STAGING doit avoir des drapeaux de
					// liaison NULS. Un `NkBufferDesc{}` garde son bind par defaut
					// (0x1) et CreateBuffer rend E_INVALIDARG -- constate par le
					// journal, trois erreurs par frame puis plantage.
					NkBufferDesc bd = NkBufferDesc::Staging(256);
					bd.usage = NkResourceUsage::NK_READBACK;
					mLumaReadBuf[i] = mDevice->CreateBuffer(bd);
					// Une creation qui echoue ne doit pas etre retentee a chaque
					// frame : on cesse d'essayer et l'affichage reste a « — »,
					// plutot que d'inonder le journal en boucle.
					if (!mLumaReadBuf[i].IsValid()) {
						mLumaReadDisabled = true;
						return;
					}
				}
			}

			const int wr = mLumaReadCursor % kLumaReadRing;
			if (mLumaReadBuf[wr].IsValid()) {
				NkBufferTextureCopyRegion rg{};
				rg.width = 1;
				rg.height = 1;
				cmd->CopyTextureToBuffer(src, mLumaReadBuf[wr], rg);
				if (mLumaReadFilled < kLumaReadRing)
					mLumaReadFilled++;
			}
			mLumaReadCursor = (mLumaReadCursor + 1) % kLumaReadRing;

			// On lit la case ecrite il y a DEUX frames : elle est certainement
			// prete, donc aucun Map bloquant ne peut nous faire attendre. Tant que
			// l'anneau n'est pas rempli, rien a lire -- et surtout rien a inventer.
			if (mLumaReadFilled < kLumaReadRing) {
				mResolvedStaleFrames++;
				return;
			}
			const int rd = mLumaReadCursor % kLumaReadRing;
			if (!mLumaReadBuf[rd].IsValid()) {
				mResolvedStaleFrames++;
				return;
			}
			NkMappedMemory mm = mDevice->MapBuffer(mLumaReadBuf[rd], 0, 8);
			if (!mm.ptr) {
				// La copie peut echouer EN SILENCE cote RHI (garde MSAA). On ne
				// fabrique pas de valeur : on vieillit celle qu'on a.
				mResolvedStaleFrames++;
				return;
			}
			uint16 raw = 0;
			memcpy(&raw, mm.ptr, sizeof(raw));
			mDevice->UnmapBuffer(mLumaReadBuf[rd]);
			const float32 avgLuma = NkHalfToFloat(raw);

			// Meme formule que le tonemap, sinon l'affichage mentirait sur ce que
			// l'image fait reellement.
			float32 expo = mCfg.exposure;
			if (avgLuma > 0.f) {
				const float32 key = mCfg.autoExposureKey;
				float32 autoExp = key / (avgLuma > 0.0001f ? avgLuma : 0.0001f);
				const float32 lo = mCfg.autoExposureMinExp;
				const float32 hi = mCfg.autoExposureMaxExp;
				autoExp = autoExp < lo ? lo : (autoExp > hi ? hi : autoExp);
				float32 k = NkAutoExpStrength(mCfg.autoExposureStrength);
				k = k < 0.f ? 0.f : (k > 1.f ? 1.f : k);
				expo = expo + (autoExp - expo) * k;
			}
			// Jamais 0 : un seuil ancre sur cette valeur partirait a l'infini.
			mResolvedExposure = expo > 0.0001f ? expo : 0.0001f;
			mResolvedValid = true;
			mResolvedStaleFrames = 0;
		}

		bool NkPostProcessStack::ResolvedExposure(float32 *out, bool *stale) const {
			// ── HORS AUTO : LE CPU CONNAIT DEJA LA REPONSE ───────────────────
			// L'exposition effective est celle de la config, immediatement et
			// sans aucun releve. On repond ICI, au point ou la question est
			// POSEE, et non dans le pompage de l'anneau : c'est le seul endroit
			// qui vaut pour TOUS les consommateurs (bright pass, panneau) et a
			// TOUT instant -- y compris a la construction du graphe, qui a lieu
			// hors frame et ne verrait pas une valeur posee par le rendu.
			//
			// CE QUE CE COURT-CIRCUIT CORRIGE, mesure a l'appui (cas 4) : apres
			// extinction de l'auto, mResolvedExposure gardait la derniere valeur
			// relevee (5,1513 sur une scene a ambiance 0,050), mResolvedValid
			// restait vrai, et RIEN ne les rafraichissait ni ne les perimait --
			// mResolvedStaleFrames lui-meme n'est incremente que par le pompage,
			// qui ne tourne plus. Le seuil de bloom s'ancrait donc sur une
			// exposition d'un etat revolu : bloomThr 1,19 au lieu de 6,15, d'ou
			// le cube surexpose et le retour de la bouillie lumineuse.
			if (!IsAutoExposureEnabled()) {
				if (out)
					*out = mCfg.exposure > 0.f ? mCfg.exposure : 1.f;
				// Aucune mesure n'est en vol : il n'y a rien qui puisse perimer.
				if (stale)
					*stale = false;
				return true;
			}
			if (out)
				*out = mResolvedExposure;
			// Deux frames de retard sont NORMALES (c'est l'anneau) ; au-dela, les
			// releves ont cesse d'arriver et l'appelant doit le dire.
			if (stale)
				*stale = mResolvedStaleFrames > kLumaReadRing;
			return mResolvedValid;
		}

		// =====================================================================
		// Phase L — TAA (Temporal Anti-Aliasing, 2026-07-30)
		// =====================================================================
		// Active par la config (postProcess.taa) ou l'override NK_TAA (A/B sans
		// recompiler, meme motif que NK_AUTOEXP / NK_HDR_CLAMP).
		static bool NkTAAEnabledEnv(bool fromConfig) {
			static int sInit = 0;
			static bool sHasEnv = false;
			static bool sEnv = false;
			if (!sInit) {
				sInit = 1;
				const char *v = getenv("NK_TAA");
				if (v && v[0]) {
					sHasEnv = true;
					sEnv = (v[0] != '0');
				}
			}
			return sHasEnv ? sEnv : fromConfig;
		}

		// Poids de l'historique : plus il est haut, plus l'image est lisse mais
		// plus les objets mobiles trainent. 0.9 = compromis courant (10 % de
		// nouveau par frame -> convergence en ~20 frames).
		static float32 NkTAABlend() {
			static int sInit = 0;
			static float32 sVal = 0.9f;
			if (!sInit) {
				sInit = 1;
				const char *v = getenv("NK_TAA_BLEND");
				if (v && v[0])
					sVal = (float32)atof(v);
			}
			return sVal;
		}

		bool NkPostProcessStack::IsTAAEnabled() const {
			// Depend du SHADER et non du pipeline : le graph appelle cette methode au
			// moment ou il se construit, donc AVANT que le pipeline (lazy, RP-compatible
			// avec le framebuffer de la passe) ne puisse exister.
			if (!mShaderTAA.IsValid())
				return false;
			return NkTAAEnabledEnv(mCfg.taa);
		}

		bool NkPostProcessStack::EnsureTAAPipeline(NkRenderPassHandle rp) {
			if (mPipeTAA.IsValid())
				return true;
			if (!mShaderTAA.IsValid() || !mDevice)
				return false;

			NkGraphicsPipelineDesc pd;
			pd.shader = mShaderTAA;
			pd.depthStencil = NkDepthStencilDesc::NoDepth();
			pd.rasterizer = NkRasterizerDesc::NoCull();
			pd.blend = NkBlendDesc::Opaque();
			pd.debugName = "PP_TAA";
			pd.renderPass = rp; // RP de la passe du graph (cf. GetPassRenderPass)
			// 96 octets : mat4 reproj (64) + vec4 p0 (16) + vec4 p1 (16). Sous la
			// garantie Vulkan (128) et sous les root constants DX12 (32 DWORDs).
			// p1 existe pour DISSOCIER yFlipUV (VS) de ndcYSign (FS) : ces deux
			// valeurs divergent par backend, les confondre cassait GL et DX.
			pd.AddPushConstant(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, 96);
			if (mTAALayout.IsValid())
				pd.descriptorSetLayouts.PushBack(mTAALayout);
			mPipeTAA = mDevice->CreateGraphicsPipeline(pd);
			logger.Info("[NkPostProcessStack] TAA pipeline (lazy) : valid={0}\n", mPipeTAA.IsValid() ? 1 : 0);
			return mPipeTAA.IsValid();
		}

		void NkPostProcessStack::RunTAAInPass(NkICommandBuffer *cmd, NkTextureHandle ldrIn, NkTextureHandle histIn,
											  NkTextureHandle depth, const NkMat4f &reproj, bool useHistory,
											  NkRenderPassHandle rp) {
			if (!cmd || !ldrIn.IsValid() || !IsTAAEnabled())
				return;
			if (!EnsureTAAPipeline(rp))
				return;
			NkSamplerHandle samp = mResources ? mResources->GetSamplerLinearClamp() : NkSamplerHandle{};
			if (!samp.IsValid())
				return;

			NkDescSetHandle set = mTAASets[mTAASetCursor % kTAADescSets];
			mTAASetCursor++;
			if (!set.IsValid())
				return;
			const bool histOk = useHistory && histIn.IsValid();
			mDevice->BindTextureSampler(set, 0, ldrIn, samp);
			mDevice->BindTextureSampler(set, 1, histOk ? histIn : ldrIn, samp);
			// Profondeur absente (config sans depth) : on binde le LDR pour ne pas
			// laisser un slot non initialise ; le shader verra depth >= 0.9999 comme
			// du ciel et retombera sur l'image courante (donc pas d'accumulation).
			mDevice->BindTextureSampler(set, 2, depth.IsValid() ? depth : ldrIn, samp);

			cmd->BindGraphicsPipeline(mPipeTAA);
			cmd->BindDescriptorSet(set, 0);

			struct PC {
					float32 reproj[16];
					float32 blend, yFlipUV, invResW, invResH; // p0
					float32 ndcYSign, clampOn, debugMode, _pad; // p1
			} pc;

			memcpy(pc.reproj, &reproj, sizeof(pc.reproj));
			pc.blend = (histOk && depth.IsValid()) ? NkTAABlend() : 0.f;

			// ── Conventions Y PAR BACKEND ─────────────────────────────────────────
			// Ce sont DEUX quantites distinctes, et les confondre (un seul slot p0.y
			// pour les deux) etait le bug qui cassait l'echantillonnage :
			//   yFlipUV  (VS) : faut-il retourner l'UV pour echantillonner la cible
			//                   LDR ? VK oui, GL et DX non.
			//   ndcYSign (FS) : signe qui relie vUV.y au NDC Y de la frame courante,
			//                   pour reconstruire la position ecran depuis la
			//                   profondeur. DX +1, GL et VK -1.
			//
			// ⚠️ yFlipUV suit ExecuteFXAA (`isVK ? -1 : +1`) et NON le deferred
			// lighting, bien que le deferred resolve un probleme voisin : ce qui
			// compte est l'orientation de la TEXTURE LUE, et le FXAA lit exactement
			// la meme (le transient ToneLDR ecrit par la passe PostProcess). Avoir
			// extrapole depuis le deferred donnait -1 sur DX, MESURE FAUX : luminance
			// 97,8 au lieu de 89,2 sur DX11 (image retournee), contre 88,7 avec +1.
			// Verifie sur trois backends : GL +1 / VK -1 / DX +1.
			const NkGraphicsApi api = mDevice ? mDevice->GetApi() : NkGraphicsApi::NK_GFX_API_OPENGL;
			const bool isVK = (api == NkGraphicsApi::NK_GFX_API_VULKAN);
			const bool isDX = (api == NkGraphicsApi::NK_GFX_API_DX11 || api == NkGraphicsApi::NK_GFX_API_DX12);
			pc.yFlipUV = isVK ? -1.f : 1.f;
			pc.ndcYSign = isDX ? 1.f : -1.f;
			// Overrides de diagnostic : ces deux signes ne se VOIENT pas separement a
			// l'oeil (le clamp de voisinage borne l'erreur), il faut pouvoir les
			// mesurer en A/B. Servent surtout a valider un nouveau backend sans
			// recompiler. Absents = valeurs ci-dessus.
			if (const char *v = getenv("NK_TAA_YFLIP"))
				if (v[0])
					pc.yFlipUV = (float32)atof(v);
			if (const char *v = getenv("NK_TAA_NDCY"))
				if (v[0])
					pc.ndcYSign = (float32)atof(v);
			// Clamp de voisinage : actif par defaut (c'est lui qui tue le ghosting).
			// NK_TAA_CLAMP=0 le desarme, ce qui EXPOSE les erreurs de reprojection
			// qu'il masque autrement -> seul moyen de mesurer les conventions Y.
			pc.clampOn = 1.f;
			if (const char *v = getenv("NK_TAA_CLAMP"))
				if (v[0] && v[0] == '0')
					pc.clampOn = 0.f;
			// Sondes de diagnostic (cf. pp_taa.frag.nksl) : 1 = historique brut
			// reprojete, 2 = historique a l'UV courant, 3 = amplitude du decalage.
			pc.debugMode = 0.f;
			if (const char *v = getenv("NK_TAA_DEBUG"))
				if (v[0])
					pc.debugMode = (float32)atof(v);
			pc.invResW = mW > 0 ? 1.f / (float32)mW : 0.f;
			pc.invResH = mH > 0 ? 1.f / (float32)mH : 0.f;
			pc._pad = 0.f;
			cmd->PushConstants(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, sizeof(pc), &pc);
			cmd->Draw(3, 1, 0, 0);

			// Trace one-shot : de quoi verifier d'un coup d'oeil, sur un nouveau
			// backend, que l'historique est bien branche et quelles conventions Y
			// s'appliquent. Les handles distincts confirment que les trois entrees
			// ne pointent pas sur la meme cible.
			static int sDiag = 0;
			if (sDiag++ == 0)
				logger.Info("[TAA] useHistory={0} blend={1} yFlip={2} ndcY={3} | ids ldr={4} hist={5} depth={6}\n",
							histOk ? 1 : 0, pc.blend, pc.yFlipUV, pc.ndcYSign, (uint32)ldrIn.id, (uint32)histIn.id,
							(uint32)depth.id);
		}

	} // namespace renderer
} // namespace nkentseu
