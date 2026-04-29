#pragma once
// =============================================================================
// NkRendererConfig.h  — NKRenderer v4.0
// Configuration du renderer avec presets par domaine.
// =============================================================================
#include "NKCore/NkTypes.h"

namespace nkentseu {
namespace renderer {

    enum class NkGraphicsApi : uint8 {
        NK_OPENGL    = 0,
        NK_OPENGLES  = 1,
        NK_VULKAN    = 2,
        NK_DX11      = 3,
        NK_DX12      = 4,
        NK_METAL     = 5,
        NK_WEBGL     = 6,
        NK_WEBGPU    = 7,
        NK_SOFTWARE  = 8,
    };

    enum class NkRenderQuality : uint8 {
        NK_MOBILE    = 0,
        NK_LOW       = 1,
        NK_MEDIUM    = 2,
        NK_HIGH      = 3,
        NK_ULTRA     = 4,
        NK_CINEMATIC = 5,
    };

    enum class NkPipelineMode : uint8 {
        NK_FORWARD        = 0,
        NK_DEFERRED       = 1,
        NK_FORWARD_PLUS   = 2,
        NK_TILED_DEFERRED = 3,
    };

    struct NkShadowConfig {
        bool    enabled          = true;
        uint32  cascadeCount     = 3;
        uint32  resolution       = 2048;
        float32 maxDistance      = 200.f;
        float32 cascadeLambda    = 0.85f;
        bool    softShadows      = true;
        bool    pcss             = false;
        float32 normalBias       = 0.005f;
        float32 depthBias        = 1.25f;
    };

    struct NkPostConfig {
        // Tone mapping
        bool    toneMapping    = true;
        float32 exposure       = 1.f;
        float32 gamma          = 2.2f;
        bool    aces           = true;
        // Bloom
        bool    bloom          = true;
        float32 bloomThreshold = 1.f;
        float32 bloomStrength  = 0.04f;
        uint32  bloomPasses    = 5;
        // SSAO
        bool    ssao           = true;
        float32 ssaoRadius     = 0.5f;
        float32 ssaoBias       = 0.025f;
        uint32  ssaoSamples    = 32;
        bool    hbao           = false;
        // DOF
        bool    dof            = false;
        float32 dofFocusDist   = 10.f;
        float32 dofAperture    = 0.1f;
        // Motion blur
        bool    motionBlur     = false;
        float32 motionBlurShutter = 0.5f;
        // AA
        bool    fxaa           = true;
        bool    taa            = false;
        // Color grading
        bool    colorGrading   = false;
        float32 contrast       = 1.f;
        float32 saturation     = 1.f;
        // SSR
        bool    ssr            = false;
        // Vignette / grain
        bool    vignette       = false;
        float32 vignetteIntens = 0.4f;
        bool    filmGrain      = false;
        float32 filmGrainStr   = 0.3f;
    };

    struct NkRendererConfig {
        NkGraphicsApi    api         = NkGraphicsApi::NK_OPENGL;
        uint32           width       = 1280;
        uint32           height      = 720;
        NkPipelineMode   pipeline    = NkPipelineMode::NK_FORWARD_PLUS;
        NkRenderQuality  quality     = NkRenderQuality::NK_HIGH;
        bool             hdr         = true;
        bool             vsync       = true;
        uint32           msaaSamples = 1;
        uint32           maxLights   = 256;
        uint32           maxParticles= 100000;
        bool             debugOverlay= false;
        bool             wireframe   = false;
        NkShadowConfig   shadow;
        NkPostConfig     postProcess;

        // ── Presets ───────────────────────────────────────────────────────────
        static NkRendererConfig ForGame(NkGraphicsApi api = NkGraphicsApi::NK_OPENGL,
                                         uint32 w = 1920, uint32 h = 1080) {
            NkRendererConfig c; c.api=api; c.width=w; c.height=h;
            c.pipeline=NkPipelineMode::NK_FORWARD_PLUS;
            c.quality=NkRenderQuality::NK_HIGH;
            c.shadow.resolution=2048; c.shadow.cascadeCount=3;
            c.postProcess.bloom=true; c.postProcess.ssao=true;
            c.postProcess.fxaa=true;  c.postProcess.aces=true;
            return c;
        }
        static NkRendererConfig ForFilm(NkGraphicsApi api = NkGraphicsApi::NK_VULKAN,
                                         uint32 w = 3840, uint32 h = 2160) {
            NkRendererConfig c; c.api=api; c.width=w; c.height=h;
            c.pipeline=NkPipelineMode::NK_DEFERRED;
            c.quality=NkRenderQuality::NK_CINEMATIC;
            c.shadow.resolution=4096; c.shadow.cascadeCount=4;
            c.shadow.softShadows=true; c.shadow.pcss=true;
            c.postProcess.bloom=true;  c.postProcess.hbao=true;
            c.postProcess.dof=true;    c.postProcess.motionBlur=true;
            c.postProcess.taa=true;    c.postProcess.ssr=true;
            c.postProcess.colorGrading=true;
            c.vsync=false; return c;
        }
        static NkRendererConfig ForArchviz(NkGraphicsApi api = NkGraphicsApi::NK_VULKAN,
                                            uint32 w = 2560, uint32 h = 1440) {
            NkRendererConfig c; c.api=api; c.width=w; c.height=h;
            c.pipeline=NkPipelineMode::NK_DEFERRED;
            c.quality=NkRenderQuality::NK_ULTRA;
            c.shadow.resolution=4096;
            c.postProcess.hbao=true;  c.postProcess.ssr=true;
            c.postProcess.bloom=false; c.postProcess.colorGrading=true;
            c.postProcess.taa=true; return c;
        }
        static NkRendererConfig ForMobile(NkGraphicsApi api = NkGraphicsApi::NK_OPENGLES,
                                           uint32 w = 1280, uint32 h = 720) {
            NkRendererConfig c; c.api=api; c.width=w; c.height=h;
            c.pipeline=NkPipelineMode::NK_FORWARD;
            c.quality=NkRenderQuality::NK_MOBILE;
            c.shadow.enabled=false; c.hdr=false;
            c.postProcess.bloom=false; c.postProcess.ssao=false;
            c.postProcess.fxaa=false;  return c;
        }
        static NkRendererConfig For2D(NkGraphicsApi api = NkGraphicsApi::NK_OPENGL,
                                       uint32 w = 1920, uint32 h = 1080) {
            NkRendererConfig c; c.api=api; c.width=w; c.height=h;
            c.pipeline=NkPipelineMode::NK_FORWARD;
            c.quality=NkRenderQuality::NK_LOW;
            c.shadow.enabled=false; c.hdr=false;
            c.postProcess.ssao=false; c.postProcess.dof=false;
            c.postProcess.bloom=false; c.postProcess.fxaa=false;
            return c;
        }
        static NkRendererConfig ForEditor(NkGraphicsApi api = NkGraphicsApi::NK_OPENGL,
                                           uint32 w = 2560, uint32 h = 1440) {
            NkRendererConfig c; c.api=api; c.width=w; c.height=h;
            c.pipeline=NkPipelineMode::NK_FORWARD_PLUS;
            c.quality=NkRenderQuality::NK_HIGH;
            c.debugOverlay=true;
            c.shadow.resolution=1024;
            c.postProcess.bloom=false; c.postProcess.dof=false;
            c.postProcess.fxaa=true; return c;
        }
        static NkRendererConfig ForOffscreen(NkGraphicsApi api = NkGraphicsApi::NK_VULKAN,
                                              uint32 w = 3840, uint32 h = 2160) {
            NkRendererConfig c = ForFilm(api, w, h);
            c.vsync=false; return c;
        }
    };

} // namespace renderer
} // namespace nkentseu
