#pragma once
// =============================================================================
// NkRendererConfig.h
// Configuration complète du NkRenderer.
// =============================================================================
#include "NKRHI/Core/NkTypes.h"

namespace nkentseu {

    // =============================================================================
    // Préréglages de qualité
    // =============================================================================
    enum class NkRenderQuality : uint8 {
        NK_MOBILE        = 0,  // GLES3, pas de shadow, AO désactivé
        NK_LOW           = 1,  // Shadows 512px, pas de bloom, SSAO faible
        NK_MEDIUM        = 2,  // Shadows 1024px, bloom, SSAO
        NK_HIGH          = 3,  // Shadows 2048px, bloom+DOF, HBAO
        NK_ULTRA         = 4,  // Shadows 4096px, tout activé
        NK_CINEMATIC     = 5,  // Ultra + TAA + motion blur + high-res AO
    };

    // =============================================================================
    // Mode de rendu principal
    // =============================================================================
    enum class NkRenderMode : uint8 {
        NK_FORWARD,         // Forward rendering (mobile, transparent)
        NK_DEFERRED,        // Deferred shading (many lights, opaque)
        NK_FORWARD_PLUS,    // Forward+ (clustered, meilleur pour mixte)
        NK_TILED_DEFERRED,  // Tiled deferred (GPU uniforme)
    };

    // =============================================================================
    // Options du pipeline de shadow
    // =============================================================================
    struct NkShadowConfig {
        bool    enabled          = true;
        uint32  cascadeCount     = 4;       // CSM pour lumière directionnelle
        uint32  resolution       = 2048;    // résolution par cascade
        float32 maxDistance      = 200.f;   // distance max de shadow
        float32 cascadeSplitLambda = 0.85f; // répartition des cascades
        bool    softShadows      = true;    // PCF / PCSS
        bool    rayTracedShadows = false;   // DXR/VK si disponible
        float32 normalBias       = 0.005f;
        float32 depthBias        = 1.25f;
    };

    // =============================================================================
    // Options de post-traitement
    // =============================================================================
    struct NkPostProcessConfig {
        // Tone mapping
        bool    toneMapping     = true;
        float32 exposure        = 1.f;
        float32 gamma           = 2.2f;

        // Bloom
        bool    bloom           = true;
        float32 bloomThreshold  = 1.f;
        float32 bloomStrength   = 0.04f;
        uint32  bloomPasses     = 5;

        // Ambient Occlusion
        bool    ssao            = true;
        float32 ssaoRadius      = 0.5f;
        float32 ssaoBias        = 0.025f;
        uint32  ssaoSamples     = 32;
        bool    hbao            = false;   // HBAO+ si disponible

        // Depth of Field
        bool    dof             = false;
        float32 dofFocusDistance= 10.f;
        float32 dofAperture     = 0.1f;
        float32 dofFocalLength  = 50.f;   // mm

        // Motion blur
        bool    motionBlur      = false;
        float32 motionBlurShutter = 0.5f;

        // Anti-aliasing
        bool    fxaa            = true;
        bool    taa             = false;   // Temporal AA (requires jitter)
        bool    smaa            = false;

        // Color grading
        bool    colorGrading    = false;
        float32 saturation      = 1.f;
        float32 contrast        = 1.f;
        float32 brightness      = 0.f;
        float32 hueShift        = 0.f;

        // Chromatic aberration
        bool    chromaticAberration = false;
        float32 chromaticStrength   = 0.5f;

        // Vignette
        bool    vignette        = false;
        float32 vignetteStrength= 0.4f;
        float32 vignetteSoftness= 0.45f;

        // Film grain
        bool    filmGrain       = false;
        float32 filmGrainStrength = 0.05f;

        // Screen-Space Reflections
        bool    ssr             = false;
        float32 ssrMaxDistance  = 100.f;
        uint32  ssrSteps        = 16;

        // Screen-Space GI
        bool    ssgi            = false;
    };

    // =============================================================================
    // Options du sky
    // =============================================================================
    enum class NkSkyMode : uint8 {
        NK_NONE,
        NK_SOLID_COLOR,
        NK_GRADIENT,
        NK_HDRI,            // Image HDR cubemap
        NK_PROCEDURAL,      // Rayleigh scattering atmosphérique
    };

    struct NkSkyConfig {
        NkSkyMode mode          = NkSkyMode::NK_PROCEDURAL;
        NkClearColor solidColor = {0.15f, 0.20f, 0.30f, 1.f};
        // HDRI
        const char* hdriPath    = nullptr;
        float32 hdriIntensity   = 1.f;
        float32 hdriRotation    = 0.f;
        // Procédural
        float32 sunIntensity    = 10.f;
        float32 sunSize         = 0.5f;   // degrés
        float32 turbidity       = 2.f;
        bool    enableFog       = false;
        float32 fogStart        = 50.f;
        float32 fogEnd          = 500.f;
        NkClearColor fogColor   = {0.7f, 0.75f, 0.8f, 1.f};
    };

    // =============================================================================
    // Configuration principale
    // =============================================================================
    struct NkRendererConfig {
        // API et fenêtrage
        NkGraphicsApi   api           = NkGraphicsApi::NK_GFX_API_OPENGL;
        uint32          width         = 1280;
        uint32          height        = 720;
        bool            vsync         = true;
        bool            hdr           = true;    // framebuffer HDR (RGBA16F)
        NkSampleCount   msaa          = NkSampleCount::NK_S1;
        uint32          maxFramesInFlight = 3;

        // Mode de rendu
        NkRenderMode    renderMode    = NkRenderMode::NK_FORWARD_PLUS;
        NkRenderQuality quality       = NkRenderQuality::NK_HIGH;

        // Shadows
        NkShadowConfig  shadow;

        // Post-processing
        NkPostProcessConfig postProcess;

        // Sky / ambiance
        NkSkyConfig     sky;

        // Limites mémoire
        uint64  maxVertexBufferBytes  = 256ull * 1024 * 1024;   // 256 MB
        uint64  maxIndexBufferBytes   = 64ull  * 1024 * 1024;   // 64 MB
        uint64  maxTextureBytes       = 512ull * 1024 * 1024;   // 512 MB
        uint32  maxLights             = 256;
        uint32  maxInstances          = 8192;
        uint32  maxMaterials          = 1024;

        // Debug
        bool    debugWireframe        = false;
        bool    debugNormals          = false;
        bool    debugAO               = false;
        bool    debugShadowCascades   = false;
        bool    debugLightCount       = false;
        bool    enableGPUTimestamps   = false;

        // Préréglages rapides
        static NkRendererConfig ForGame(NkGraphicsApi api, uint32 w, uint32 h) {
            NkRendererConfig cfg;
            cfg.api = api; cfg.width = w; cfg.height = h;
            cfg.renderMode = NkRenderMode::NK_FORWARD_PLUS;
            cfg.quality = NkRenderQuality::NK_HIGH;
            cfg.postProcess.bloom = true;
            cfg.postProcess.ssao  = true;
            cfg.postProcess.toneMapping = true;
            cfg.postProcess.fxaa = true;
            return cfg;
        }

        static NkRendererConfig ForFilm(NkGraphicsApi api, uint32 w, uint32 h) {
            NkRendererConfig cfg;
            cfg.api = api; cfg.width = w; cfg.height = h;
            cfg.renderMode = NkRenderMode::NK_DEFERRED;
            cfg.quality = NkRenderQuality::NK_CINEMATIC;
            cfg.hdr = true;
            cfg.postProcess.bloom = true;
            cfg.postProcess.dof   = true;
            cfg.postProcess.motionBlur = true;
            cfg.postProcess.taa = true;
            cfg.postProcess.hbao = true;
            cfg.postProcess.colorGrading = true;
            cfg.postProcess.filmGrain = true;
            cfg.shadow.softShadows = true;
            cfg.shadow.resolution = 4096;
            return cfg;
        }

        static NkRendererConfig ForArchviz(NkGraphicsApi api, uint32 w, uint32 h) {
            NkRendererConfig cfg;
            cfg.api = api; cfg.width = w; cfg.height = h;
            cfg.renderMode = NkRenderMode::NK_DEFERRED;
            cfg.quality = NkRenderQuality::NK_ULTRA;
            cfg.postProcess.bloom = false;
            cfg.postProcess.ssao  = true;
            cfg.postProcess.hbao  = true;
            cfg.postProcess.ssr   = true;
            cfg.postProcess.colorGrading = true;
            cfg.postProcess.toneMapping  = true;
            cfg.shadow.softShadows = true;
            cfg.shadow.cascadeCount = 4;
            cfg.sky.mode = NkSkyMode::NK_HDRI;
            return cfg;
        }

        static NkRendererConfig ForMobile(NkGraphicsApi api, uint32 w, uint32 h) {
            NkRendererConfig cfg;
            cfg.api = api; cfg.width = w; cfg.height = h;
            cfg.renderMode = NkRenderMode::NK_FORWARD;
            cfg.quality = NkRenderQuality::NK_MOBILE;
            cfg.hdr = false;
            cfg.postProcess = {};
            cfg.postProcess.toneMapping = true;
            cfg.postProcess.fxaa = false;
            cfg.shadow.enabled = false;
            return cfg;
        }

        static NkRendererConfig For2D(NkGraphicsApi api, uint32 w, uint32 h) {
            NkRendererConfig cfg;
            cfg.api = api; cfg.width = w; cfg.height = h;
            cfg.renderMode = NkRenderMode::NK_FORWARD;
            cfg.quality = NkRenderQuality::NK_MEDIUM;
            cfg.shadow.enabled = false;
            cfg.postProcess.ssao = false;
            cfg.postProcess.dof  = false;
            return cfg;
        }
    };

} // namespace nkentseu

#pragma once
// =============================================================================
// NkRendererConfig.h
// Configuration complète du système NkRenderer.
// =============================================================================
#include "NKRHI/Core/NkTypes.h"

namespace nkentseu {

    enum class NkRenderQuality : uint8 {
        NK_MOBILE=0, NK_LOW=1, NK_MEDIUM=2, NK_HIGH=3, NK_ULTRA=4, NK_CINEMATIC=5
    };

    enum class NkRenderMode : uint8 {
        NK_FORWARD, NK_DEFERRED, NK_FORWARD_PLUS, NK_TILED_DEFERRED
    };

    enum class NkSkyMode : uint8 {
        NK_NONE, NK_SOLID_COLOR, NK_GRADIENT, NK_HDRI, NK_PROCEDURAL
    };

    enum class NkDOFMode : uint8 {
        NK_DOF_GAUSSIAN, NK_DOF_BOKEH, NK_DOF_TILT_SHIFT
    };

    enum class NkToneMapOp : uint8 {
        NK_TONEMAP_LINEAR=0, NK_TONEMAP_REINHARD=1, NK_TONEMAP_ACES=2,
        NK_TONEMAP_FILMIC=3, NK_TONEMAP_UNCHARTED2=4, NK_TONEMAP_AGX=5,
        NK_TONEMAP_PBR_NEUTRAL=6
    };

    struct NkShadowConfig {
        bool    enabled            = true;
        uint32  cascadeCount       = 4;
        uint32  resolution         = 2048;
        float32 maxDistance        = 200.f;
        float32 cascadeSplitLambda = 0.85f;
        bool    softShadows        = true;
        bool    rayTracedShadows   = false;
        float32 normalBias         = 0.005f;
        float32 depthBias          = 1.25f;
    };

    struct NkSSAOParams {
        bool    enabled    = true;
        bool    hbao       = false;
        uint32  samples    = 32;
        float32 radius     = 0.5f;
        float32 bias       = 0.025f;
        float32 strength   = 1.f;
        float32 power      = 1.5f;
        bool    blur       = true;
        uint32  blurRadius = 3;
    };

    struct NkBloomParams {
        bool    enabled    = true;
        float32 threshold  = 1.f;
        float32 softKnee   = 0.5f;
        float32 strength   = 0.04f;
        float32 scatter    = 0.7f;
        uint32  passes     = 5;
    };

    struct NkDOFParams {
        bool      enabled       = false;
        NkDOFMode mode          = NkDOFMode::NK_DOF_BOKEH;
        float32   focusDistance = 10.f;
        float32   focusRange    = 2.f;
        float32   aperture      = 0.1f;
        float32   focalLength   = 50.f;
        uint32    samples       = 64;
        float32   maxRadius     = 8.f;
    };

    struct NkColorGradingParams {
        bool    enabled      = false;
        float32 saturation   = 1.f;
        float32 contrast     = 1.f;
        float32 brightness   = 0.f;
        float32 hueShift     = 0.f;
        math::NkVec3 lift    = {0,0,0};
        math::NkVec3 gamma   = {1,1,1};
        math::NkVec3 gain    = {1,1,1};
        NkTexId  lut3D       = 0;
        float32  lutContrib  = 1.f;
    };

    struct NkSSRParams {
        bool    enabled     = false;
        float32 maxDistance = 100.f;
        uint32  steps       = 32;
        uint32  binarySteps = 8;
        float32 thickness   = 0.05f;
        float32 fadeStart   = 0.75f;
        float32 fadeEnd     = 0.95f;
    };

    struct NkMotionBlurParams {
        bool    enabled      = false;
        float32 shutterAngle = 180.f;
        uint32  samples      = 8;
        float32 maxVelocity  = 10.f;
    };

    struct NkPostProcessConfig {
        bool    toneMapping      = true;
        float32 exposure         = 1.f;
        float32 gamma            = 2.2f;
        NkToneMapOp tonemapOp    = NkToneMapOp::NK_TONEMAP_ACES;
        NkBloomParams bloom;
        NkSSAOParams  ssao;
        NkDOFParams   dof;
        NkSSRParams   ssr;
        NkMotionBlurParams motionBlur;
        NkColorGradingParams colorGrading;
        bool    fxaa             = true;
        bool    taa              = false;
        bool    smaa             = false;
        bool    vignette         = false;
        float32 vignetteStrength = 0.4f;
        bool    filmGrain        = false;
        float32 filmGrainStrength= 0.05f;
        bool    chromaticAberration = false;
        float32 chromaticStrength   = 0.5f;
    };

    struct NkSkyConfig {
        NkSkyMode   mode         = NkSkyMode::NK_PROCEDURAL;
        NkClearColor solidColor  = {0.15f,0.20f,0.30f,1.f};
        const char* hdriPath     = nullptr;
        float32     hdriIntensity= 1.f;
        float32     hdriRotation = 0.f;
        float32     sunIntensity = 10.f;
        float32     sunSize      = 0.5f;
        float32     turbidity    = 2.f;
        bool        enableFog    = false;
        float32     fogStart     = 50.f;
        float32     fogEnd       = 500.f;
        NkClearColor fogColor    = {0.7f,0.75f,0.8f,1.f};
    };

    struct NkRendererConfig {
        NkGraphicsApi   api           = NkGraphicsApi::NK_GFX_API_OPENGL;
        uint32          width         = 1280;
        uint32          height        = 720;
        bool            vsync         = true;
        bool            hdr           = true;
        NkSampleCount   msaa          = NkSampleCount::NK_S1;
        uint32          maxFrames     = 3;
        NkRenderMode    renderMode    = NkRenderMode::NK_FORWARD_PLUS;
        NkRenderQuality quality       = NkRenderQuality::NK_HIGH;
        NkShadowConfig  shadow;
        NkPostProcessConfig postProcess;
        NkSkyConfig     sky;
        uint64  maxVertexBufferBytes  = 256ull*1024*1024;
        uint64  maxIndexBufferBytes   = 64ull *1024*1024;
        uint32  maxLights             = 256;
        uint32  maxInstances          = 8192;
        uint32  maxMaterials          = 1024;
        bool    debugWireframe        = false;
        bool    debugNormals          = false;
        bool    enableGPUTimestamps   = false;
        const char* shaderCacheDir    = "Build/ShaderCache";

        static NkRendererConfig ForGame(NkGraphicsApi api, uint32 w, uint32 h) {
            NkRendererConfig c; c.api=api; c.width=w; c.height=h;
            c.renderMode=NkRenderMode::NK_FORWARD_PLUS;
            c.quality=NkRenderQuality::NK_HIGH;
            c.postProcess.bloom.enabled=true;
            c.postProcess.ssao.enabled=true;
            c.postProcess.fxaa=true;
            c.postProcess.toneMapping=true;
            return c;
        }
        static NkRendererConfig ForFilm(NkGraphicsApi api, uint32 w, uint32 h) {
            NkRendererConfig c; c.api=api; c.width=w; c.height=h;
            c.renderMode=NkRenderMode::NK_DEFERRED;
            c.quality=NkRenderQuality::NK_CINEMATIC;
            c.hdr=true;
            c.postProcess.bloom.enabled=true;
            c.postProcess.dof.enabled=true;
            c.postProcess.motionBlur.enabled=true;
            c.postProcess.taa=true;
            c.postProcess.ssao.hbao=true;
            c.postProcess.colorGrading.enabled=true;
            c.postProcess.filmGrain=true;
            c.shadow.softShadows=true; c.shadow.resolution=4096;
            return c;
        }
        static NkRendererConfig ForArchviz(NkGraphicsApi api, uint32 w, uint32 h) {
            NkRendererConfig c; c.api=api; c.width=w; c.height=h;
            c.renderMode=NkRenderMode::NK_DEFERRED;
            c.quality=NkRenderQuality::NK_ULTRA;
            c.postProcess.ssao.hbao=true;
            c.postProcess.ssr.enabled=true;
            c.postProcess.colorGrading.enabled=true;
            c.postProcess.toneMapping=true;
            c.shadow.softShadows=true;
            c.sky.mode=NkSkyMode::NK_HDRI;
            return c;
        }
        static NkRendererConfig ForMobile(NkGraphicsApi api, uint32 w, uint32 h) {
            NkRendererConfig c; c.api=api; c.width=w; c.height=h;
            c.renderMode=NkRenderMode::NK_FORWARD;
            c.quality=NkRenderQuality::NK_MOBILE;
            c.hdr=false; c.shadow.enabled=false;
            return c;
        }
        static NkRendererConfig For2D(NkGraphicsApi api, uint32 w, uint32 h) {
            NkRendererConfig c; c.api=api; c.width=w; c.height=h;
            c.renderMode=NkRenderMode::NK_FORWARD;
            c.quality=NkRenderQuality::NK_MEDIUM;
            c.shadow.enabled=false;
            c.postProcess.ssao.enabled=false;
            return c;
        }
    };

} // namespace nkentseu
