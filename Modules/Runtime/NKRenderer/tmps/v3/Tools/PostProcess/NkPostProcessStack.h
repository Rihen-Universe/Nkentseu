#pragma once
// =============================================================================
// NkPostProcessStack.h
// Pipeline de post-traitement modulaire et configurable.
//
// Effets disponibles (activables/désactivables indépendamment) :
//   1. SSAO / HBAO         — ambient occlusion
//   2. SSR                 — screen-space reflections
//   3. SSGI                — screen-space global illumination
//   4. Bloom               — halos lumineux (threshold + multiple passes)
//   5. Depth of Field      — flou de profondeur (bokeh circulaire)
//   6. Motion Blur         — flou de mouvement (per-object ou fullscreen)
//   7. Tone Mapping        — ACES, Filmic, Reinhard, Uncharted2, PBR Neutral
//   8. Color Grading       — LUT 3D + courbes + saturation
//   9. FXAA / SMAA / TAA   — anti-aliasing post-process
//  10. Chromatic Aberration— aberration chromatique
//  11. Vignette            — vignettage
//  12. Film Grain          — grain argentique
//  13. Sharpen             — accentuation
//  14. Lens Flare          — flares de lentille
//  15. Custom pass         — effet custom (callback)
//
// Tonemapping operators :
//   NK_TONEMAP_LINEAR      — pas de tonemap (HDR direct)
//   NK_TONEMAP_REINHARD    — Reinhard simple
//   NK_TONEMAP_ACES        — ACES (cinématique, utilisé dans Unreal)
//   NK_TONEMAP_FILMIC      — Filmic Hejl-Burgess
//   NK_TONEMAP_UNCHARTED2  — Uncharted 2 (légèrement contrasté)
//   NK_TONEMAP_AGX         — AgX (Blender 4+, plus neutre en couleur)
//   NK_TONEMAP_PBR_NEUTRAL — PBR Neutral (Khronos)
// =============================================================================
#include "NKRHI/Core/NkTypes.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKRenderer/Core/NkRendererConfig.h"
#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKRenderer/Core/NkMeshCache.h"
#include "NKContainers/Functional/NkFunction.h"

namespace nkentseu {

    // =============================================================================
    // Opérateurs de tone mapping
    // =============================================================================
    enum class NkToneMapOp : uint8 {
        NK_TONEMAP_LINEAR     = 0,
        NK_TONEMAP_REINHARD   = 1,
        NK_TONEMAP_ACES       = 2,
        NK_TONEMAP_FILMIC     = 3,
        NK_TONEMAP_UNCHARTED2 = 4,
        NK_TONEMAP_AGX        = 5,   // Blender 4+
        NK_TONEMAP_PBR_NEUTRAL= 6,   // Khronos PBR Neutral
    };

    // =============================================================================
    // Paramètres SSAO
    // =============================================================================
    struct NkSSAOParams {
        bool    enabled      = true;
        bool    hbao         = false;    // HBAO+ (plus coûteux mais meilleur)
        uint32  samples      = 32;
        float32 radius       = 0.5f;
        float32 bias         = 0.025f;
        float32 strength     = 1.f;
        float32 power        = 1.5f;
        bool    blur         = true;    // blur gaussien du résultat
        uint32  blurRadius   = 3;
    };

    // =============================================================================
    // Paramètres Bloom
    // =============================================================================
    struct NkBloomParams {
        bool    enabled      = true;
        float32 threshold    = 1.f;    // luminance au-dessus de laquelle le bloom s'applique
        float32 softKnee     = 0.5f;   // zone de transition douce
        float32 strength     = 0.04f;
        float32 scatter      = 0.7f;   // diffusion (0=sharp, 1=large)
        uint32  passes       = 5;      // nombre de passes down/upsample
        math::NkVec3 tint    = {1,1,1};
    };

    // =============================================================================
    // Paramètres Depth of Field
    // =============================================================================
    enum class NkDOFMode : uint8 {
        NK_DOF_GAUSSIAN,   // flou gaussien simple (rapide)
        NK_DOF_BOKEH,      // bokeh circulaire (DX12/Vulkan compute)
        NK_DOF_TILT_SHIFT, // tilt-shift horizontal
    };

    struct NkDOFParams {
        bool      enabled        = false;
        NkDOFMode mode           = NkDOFMode::NK_DOF_BOKEH;
        float32   focusDistance  = 10.f;
        float32   focusRange     = 2.f;    // zone de netteté
        float32   aperture       = 0.1f;   // taille du bokeh
        float32   focalLength    = 50.f;   // mm (affecte le bokeh scale)
        uint32    samples        = 64;
        float32   maxRadius      = 8.f;    // rayon max du bokeh en pixels
        bool      nearBlur       = true;
        bool      farBlur        = true;
    };

    // =============================================================================
    // Paramètres Color Grading
    // =============================================================================
    struct NkColorGradingParams {
        bool    enabled      = false;
        float32 saturation   = 1.f;
        float32 contrast     = 1.f;
        float32 brightness   = 0.f;
        float32 hueShift     = 0.f;
        math::NkVec3 lift    = {0,0,0};      // shadows offset
        math::NkVec3 gamma   = {1,1,1};      // midtones power
        math::NkVec3 gain    = {1,1,1};      // highlights scale
        NkTexId      lut3D   = NK_INVALID_TEX; // LUT 3D (32x32x32 RGBA)
        float32      lutContribution = 1.f;
        // Curves (optionnel)
        bool    useCurves    = false;
    };

    // =============================================================================
    // Paramètres Motion Blur
    // =============================================================================
    struct NkMotionBlurParams {
        bool    enabled      = false;
        float32 shutterAngle = 180.f; // degrés (0..360)
        uint32  samples      = 8;
        float32 maxVelocity  = 10.f;  // pixels max de flou
        bool    perObject    = false;  // per-object (requiert motion vectors)
    };

    // =============================================================================
    // Paramètres SSR (Screen-Space Reflections)
    // =============================================================================
    struct NkSSRParams {
        bool    enabled      = false;
        float32 maxDistance  = 100.f;
        uint32  steps        = 32;
        uint32  binarySteps  = 8;
        float32 stride       = 1.f;
        float32 thickness    = 0.05f;
        float32 jitter       = 1.f;
        float32 fadeStart    = 0.75f;  // fade aux bords
        float32 fadeEnd      = 0.95f;
    };

    // =============================================================================
    // Passe custom (callback utilisateur)
    // =============================================================================
    using NkPostProcessCallback = NkFunction<void(
        NkICommandBuffer* cmd,
        NkTextureHandle   inputColor,
        NkTextureHandle   inputDepth,
        NkTextureHandle   outputColor,
        uint32            width,
        uint32            height)>;

    // =============================================================================
    // NkPostProcessStack
    // =============================================================================
    class NkPostProcessStack {
        public:
            explicit NkPostProcessStack(NkIDevice* device, NkTextureLibrary* texLib);
            ~NkPostProcessStack();

            bool Initialize(uint32 width, uint32 height,
                            NkGPUFormat hdrFormat = NkGPUFormat::NK_RGBA16_FLOAT);
            void Shutdown();
            void OnResize(uint32 width, uint32 height);

            // ── Configuration ─────────────────────────────────────────────────────────
            void SetConfig(const NkPostProcessConfig& cfg);
            void SetSSAO       (const NkSSAOParams& p)         { mSSAO = p; }
            void SetBloom      (const NkBloomParams& p)         { mBloom = p; }
            void SetDOF        (const NkDOFParams& p)           { mDOF = p; }
            void SetColorGrading(const NkColorGradingParams& p) { mColorGrading = p; }
            void SetMotionBlur (const NkMotionBlurParams& p)    { mMotionBlur = p; }
            void SetSSR        (const NkSSRParams& p)           { mSSR = p; }
            void SetToneMapping(NkToneMapOp op, float32 exposure = 1.f, float32 gamma = 2.2f);
            void SetFXAA       (bool enabled)  { mFXAA = enabled; }
            void SetTAA        (bool enabled)  { mTAA  = enabled; }
            void SetVignette   (bool enabled, float32 strength = 0.4f, float32 softness = 0.45f);
            void SetFilmGrain  (bool enabled, float32 strength = 0.05f);
            void SetChromatic  (bool enabled, float32 strength = 0.5f);

            // Ajouter un effet custom dans la chaîne
            void AddCustomPass(const NkString& name, NkPostProcessCallback callback);
            void RemoveCustomPass(const NkString& name);

            // ── Exécution ─────────────────────────────────────────────────────────────
            // Prend en entrée la texture HDR + depth.
            // Retourne l'identifiant de la texture LDR finale.
            NkTexId Execute(NkICommandBuffer* cmd,
                            NkTextureHandle hdrColor,
                            NkTextureHandle depth,
                            NkTextureHandle normalBuffer  = {},  // pour SSAO/SSR
                            NkTextureHandle motionVectors = {},  // pour TAA/MotionBlur
                            uint32 frameIndex = 0);

            // ── Accès aux résultats intermédiaires (debug) ────────────────────────────
            NkTexId GetSSAOResult()  const;
            NkTexId GetBloomResult() const;
            NkTexId GetFinalLDR()    const;

            // ── Preset rapides ────────────────────────────────────────────────────────
            void ApplyPreset(NkRenderQuality quality);
            void ApplyPresetFilm();
            void ApplyPresetGame();
            void ApplyPresetArchviz();
            void ApplyPresetAnime();

            // ── Timer ─────────────────────────────────────────────────────────────────
            float32 GetTotalGPUTimeMs() const { return mTotalGPUTimeMs; }

        private:
            NkIDevice*        mDevice;
            NkTextureLibrary* mTexLib;
            uint32            mWidth, mHeight;
            NkGPUFormat       mHDRFormat;

            // Paramètres
            NkSSAOParams        mSSAO;
            NkBloomParams       mBloom;
            NkDOFParams         mDOF;
            NkColorGradingParams mColorGrading;
            NkMotionBlurParams  mMotionBlur;
            NkSSRParams         mSSR;
            NkToneMapOp         mToneMapOp    = NkToneMapOp::NK_TONEMAP_ACES;
            float32             mExposure     = 1.f;
            float32             mGamma        = 2.2f;
            bool                mFXAA         = true;
            bool                mTAA          = false;
            bool                mVignette     = false;
            float32             mVignetteStrength = 0.4f;
            float32             mVignetteSoftness = 0.45f;
            bool                mFilmGrain    = false;
            float32             mGrainStrength= 0.05f;
            bool                mChromatic    = false;
            float32             mChromaticStr = 0.5f;

            struct CustomPass {
                NkString              name;
                NkPostProcessCallback callback;
            };
            NkVector<CustomPass>  mCustomPasses;

            // Textures intermédiaires
            NkTexId mPingPong[2] = {NK_INVALID_TEX, NK_INVALID_TEX};
            NkTexId mSSAOTex     = NK_INVALID_TEX;
            NkTexId mBloomTex    = NK_INVALID_TEX;
            NkTexId mFinalTex    = NK_INVALID_TEX;
            uint32  mPingPongIdx = 0;

            // Pipelines compute / fragment
            NkPipelineHandle mSSAOPipeline;
            NkPipelineHandle mSSAOBlurPipeline;
            NkPipelineHandle mBloomDownPipeline;
            NkPipelineHandle mBloomUpPipeline;
            NkPipelineHandle mBloomThreshPipeline;
            NkPipelineHandle mDOFCoCPipeline;
            NkPipelineHandle mDOFBokehPipeline;
            NkPipelineHandle mTAAPipeline;
            NkPipelineHandle mFXAAPipeline;
            NkPipelineHandle mTonemapPipeline;
            NkPipelineHandle mVignetteGrainPipeline;

            NkShaderHandle mSSAOShader, mBloomShader, mTonemapShader;
            NkShaderHandle mFXAAShader, mTAAShader, mDOFShader;

            NkSamplerHandle mLinearSampler, mNearestSampler;
            NkMeshCache*    mMeshCache;  // screen quad

            float32 mTotalGPUTimeMs = 0.f;
            NkTexId mTAAPrevFrame   = NK_INVALID_TEX;

            // Passes individuelles
            NkTexId PassSSAO        (NkICommandBuffer* cmd, NkTextureHandle depth, NkTextureHandle normal, NkTexId input);
            NkTexId PassBloom       (NkICommandBuffer* cmd, NkTexId input);
            NkTexId PassDOF         (NkICommandBuffer* cmd, NkTexId input, NkTextureHandle depth);
            NkTexId PassMotionBlur  (NkICommandBuffer* cmd, NkTexId input, NkTextureHandle motionVec);
            NkTexId PassSSR         (NkICommandBuffer* cmd, NkTexId input, NkTextureHandle depth, NkTextureHandle normal);
            NkTexId PassTonemap     (NkICommandBuffer* cmd, NkTexId hdr);
            NkTexId PassColorGrading(NkICommandBuffer* cmd, NkTexId input);
            NkTexId PassFXAA        (NkICommandBuffer* cmd, NkTexId input);
            NkTexId PassTAA         (NkICommandBuffer* cmd, NkTexId input, NkTextureHandle motion, uint32 frame);
            NkTexId PassVignetteGrain(NkICommandBuffer* cmd, NkTexId input, uint32 frame);

            NkTexId SwapPingPong(NkTexId src);
            void    DispatchFullscreen(NkICommandBuffer* cmd, NkPipelineHandle pipe,
                                        NkTextureHandle input, NkTextureHandle output,
                                        uint32 w, uint32 h);
            bool CreatePipelines();
            void CreateIntermediateTextures();
    };

} // namespace nkentseu