#pragma once
// =============================================================================
// NkRendererConfig.h  — NKRenderer v5.0  (Core/)
//
// Configuration du renderer. Trois axes orthogonaux :
//
//  1. NkSubsystemFlags  : QUELS sous-systemes activer (opt-in granulaire)
//  2. NkPipelineMode    : COMMENT le scene 3D est rasterisee (Forward / Forward+ / Deferred)
//  3. NkRenderQuality   : combien de samples/casc/passes (Mobile..Cinematic)
//
// + presets prets a l'emploi : ForGame / ForFilm / ForArchviz / For2D / ForEditor / ForMinimal
// =============================================================================
#include "NKCore/NkTypes.h"
#include "NKPlatform/NkCGXDetect.h"

namespace nkentseu {
    namespace renderer {

        using NkGraphicsApi = graphics::NkGraphicsApi;

        // =====================================================================
        // SubsystemFlags — opt-in granulaire
        // L'utilisateur peut activer SEULEMENT ce qu'il consomme :
        //   ex. un jeu 2D : NK_SS_RENDER2D | NK_SS_TEXT
        //   ex. un viewer model 3D : NK_SS_RENDER3D | NK_SS_SHADOW | NK_SS_POST_PROCESS
        //   ex. tout : NK_SS_ALL
        // =====================================================================
        enum NkSubsystemFlags : uint32 {
            NK_SS_NONE          = 0,
            NK_SS_RENDER2D      = 1u << 0,   // batched sprites/shapes/lines
            NK_SS_RENDER3D      = 1u << 1,   // mesh draw 3D
            NK_SS_TEXT          = 1u << 2,   // NKFont bridge (depend de RENDER2D)
            NK_SS_UI            = 1u << 3,   // NKUI bridge (depend de RENDER2D + TEXT)
            NK_SS_SHADOW        = 1u << 4,   // CSM + PCSS (depend de RENDER3D)
            NK_SS_POST_PROCESS  = 1u << 5,   // tonemap/bloom/fxaa/ssao
            NK_SS_VFX           = 1u << 6,   // particles + trails + decals
            NK_SS_ANIMATION     = 1u << 7,   // skeletal + skinning + blendshapes
            NK_SS_OVERLAY       = 1u << 8,   // debug overlay (stats, gizmos) (depend de RENDER2D + TEXT)
            NK_SS_SIMULATION    = 1u << 9,   // physics debug viz
            NK_SS_OFFSCREEN     = 1u << 10,  // render-to-texture targets
            NK_SS_RAYTRACING    = 1u << 11,  // RT/path tracing (require RT-capable backend)
            NK_SS_GPU_CULLING   = 1u << 12,  // compute-based frustum/occlusion culling

            // Bundles courants
            NK_SS_2D_ESSENTIALS = NK_SS_RENDER2D | NK_SS_TEXT,
            NK_SS_3D_BASE       = NK_SS_RENDER3D | NK_SS_SHADOW | NK_SS_POST_PROCESS,
            NK_SS_DEBUG         = NK_SS_OVERLAY | NK_SS_SIMULATION,
            NK_SS_ALL           = 0xFFFFFFFFu,
        };
        inline NkSubsystemFlags operator|(NkSubsystemFlags a, NkSubsystemFlags b) noexcept {
            return static_cast<NkSubsystemFlags>(static_cast<uint32>(a) | static_cast<uint32>(b));
        }
        inline NkSubsystemFlags operator&(NkSubsystemFlags a, NkSubsystemFlags b) noexcept {
            return static_cast<NkSubsystemFlags>(static_cast<uint32>(a) & static_cast<uint32>(b));
        }
        inline bool NkHasFlag(NkSubsystemFlags set, NkSubsystemFlags f) noexcept {
            return (static_cast<uint32>(set) & static_cast<uint32>(f)) != 0;
        }

        // =====================================================================
        // Quality / Pipeline
        // =====================================================================
        enum class NkRenderQuality : uint8 {
            NK_MOBILE    = 0,
            NK_LOW       = 1,
            NK_MEDIUM    = 2,
            NK_HIGH      = 3,
            NK_ULTRA     = 4,
            NK_CINEMATIC = 5,
        };

        enum class NkPipelineMode : uint8 {
            NK_FORWARD        = 0,   // simple, mobile-friendly
            NK_DEFERRED       = 1,   // many lights, no forward MSAA
            NK_FORWARD_PLUS   = 2,   // clustered light culling, MSAA-friendly (default desktop)
            NK_TILED_DEFERRED = 3,
        };

        // =====================================================================
        // Sub-configs
        // =====================================================================
        struct NkShadowConfig {
            // NB: pour ne PAS allouer le sous-systeme shadow, retirer NK_SS_SHADOW
            // des subsystems flags. Ce `enabled` est un toggle runtime (active/desactive
            // les passes shadow sans liberer les ressources).
            bool    enabled        = true;
            uint32  cascadeCount   = 4;        // CSM split count (UE5 utilise 4)
            uint32  resolution     = 2048;     // par cascade
            float32 maxDistance    = 200.f;
            float32 cascadeLambda  = 0.85f;    // 0=lineaire, 1=logarithmique
            bool    softShadows    = true;     // PCF 3x3
            bool    pcss           = false;    // PCSS (penombres realistes, ~3x cost)
            float32 normalBias     = 0.005f;
            float32 depthBias      = 1.25f;
            uint32  poissonSamples = 16;
        };

        struct NkPostConfig {
            // Tone mapping
            bool    toneMapping       = true;
            bool    aces              = true;       // ACES Filmic ; sinon Reinhard
            float32 exposure          = 1.f;
            float32 gamma             = 2.2f;
            // Bloom (dual-Kawase 6 mips)
            bool    bloom             = true;
            float32 bloomThreshold    = 1.f;
            float32 bloomStrength     = 0.04f;
            uint32  bloomPasses       = 6;
            // SSAO (ground-truth ambient occlusion)
            bool    ssao              = true;
            float32 ssaoRadius        = 0.5f;
            float32 ssaoBias          = 0.025f;
            uint32  ssaoSamples       = 32;
            bool    hbao              = false;      // upgrade vers HBAO (qualite > SSAO)
            // DOF
            bool    dof               = false;
            float32 dofFocusDist      = 10.f;
            float32 dofAperture       = 0.1f;
            // Motion blur
            bool    motionBlur        = false;
            float32 motionBlurShutter = 0.5f;
            // AA
            bool    fxaa              = true;
            bool    taa               = false;     // necessite history buffer + jittered proj
            // Color grading
            bool    colorGrading      = false;
            float32 contrast          = 1.f;
            float32 saturation        = 1.f;
            // SSR
            bool    ssr               = false;
            // Vignette / grain
            bool    vignette          = false;
            float32 vignetteIntens    = 0.4f;
            bool    filmGrain         = false;
            float32 filmGrainStr      = 0.3f;
        };

        struct NkIBLConfig {
            bool    enabled            = true;
            uint32  irradianceMapSize  = 32;       // diffuse env (32x32 cubemap suffit)
            uint32  specularMapSize    = 256;      // GGX prefiltered (mips = roughness)
            uint32  brdfLUTSize        = 512;      // 2D R16G16
            uint32  prefilterMipCount  = 6;        // mips de la specular map
        };

        struct NkClusterConfig {
            uint32 tilesX     = 16;
            uint32 tilesY     = 9;
            uint32 sliceCount = 24;     // depth slices (logarithmique)
            uint32 maxLightsPerCluster = 256;
        };

        // =====================================================================
        // NkRendererConfig — la config globale
        // =====================================================================
        struct NkRendererConfig {
            // Backend & resolution
            NkGraphicsApi      api          = NkGraphicsApi::NK_GFX_API_OPENGL;
            uint32             width        = 1280;
            uint32             height       = 720;

            // Sous-systemes actifs (opt-in granulaire)
            NkSubsystemFlags   subsystems   = NK_SS_ALL;

            // Pipeline & qualite
            NkPipelineMode     pipeline     = NkPipelineMode::NK_FORWARD_PLUS;
            NkRenderQuality    quality      = NkRenderQuality::NK_HIGH;
            bool               hdr          = true;
            bool               vsync        = true;
            uint32             msaaSamples  = 1;

            // Limites
            uint32             maxLights    = 256;
            uint32             maxParticles = 100000;
            uint32             maxMeshes    = 65536;

            // Sous-configs (consultees seulement si le sous-systeme est actif)
            NkShadowConfig     shadow;
            NkPostConfig       postProcess;
            NkIBLConfig        ibl;
            NkClusterConfig    cluster;

            // Debug
            bool               debugOverlay  = false;
            bool               wireframe     = false;
            bool               validation    = false;   // active validation layer (Vulkan)

            // ─── Helpers ────────────────────────────────────────────────────
            bool Has(NkSubsystemFlags f) const noexcept { return NkHasFlag(subsystems, f); }
            void Enable(NkSubsystemFlags f)  noexcept { subsystems = subsystems | f; }
            void Disable(NkSubsystemFlags f) noexcept {
                subsystems = static_cast<NkSubsystemFlags>(
                    static_cast<uint32>(subsystems) & ~static_cast<uint32>(f));
            }

            // =================================================================
            // Presets
            // =================================================================
            static NkRendererConfig ForGame(NkGraphicsApi api = NkGraphicsApi::NK_GFX_API_OPENGL,
                                            uint32 w = 1920, uint32 h = 1080) {
                NkRendererConfig c;
                c.api=api; c.width=w; c.height=h;
                c.pipeline=NkPipelineMode::NK_FORWARD_PLUS;
                c.quality=NkRenderQuality::NK_HIGH;
                c.subsystems = NK_SS_RENDER2D | NK_SS_RENDER3D | NK_SS_TEXT
                             | NK_SS_SHADOW | NK_SS_POST_PROCESS | NK_SS_VFX
                             | NK_SS_ANIMATION | NK_SS_OVERLAY;
                c.shadow.resolution=2048; c.shadow.cascadeCount=4;
                c.postProcess.bloom=true; c.postProcess.ssao=true;
                c.postProcess.fxaa=true;  c.postProcess.aces=true;
                return c;
            }

            static NkRendererConfig ForFilm(NkGraphicsApi api = NkGraphicsApi::NK_GFX_API_VULKAN,
                                            uint32 w = 3840, uint32 h = 2160) {
                NkRendererConfig c;
                c.api=api; c.width=w; c.height=h;
                c.pipeline=NkPipelineMode::NK_DEFERRED;
                c.quality=NkRenderQuality::NK_CINEMATIC;
                c.subsystems = NK_SS_RENDER3D | NK_SS_SHADOW | NK_SS_POST_PROCESS
                             | NK_SS_VFX | NK_SS_ANIMATION | NK_SS_OFFSCREEN;
                c.shadow.resolution=4096; c.shadow.cascadeCount=4;
                c.shadow.softShadows=true; c.shadow.pcss=true;
                c.postProcess.bloom=true;  c.postProcess.hbao=true;
                c.postProcess.dof=true;    c.postProcess.motionBlur=true;
                c.postProcess.taa=true;    c.postProcess.ssr=true;
                c.postProcess.colorGrading=true;
                c.vsync=false;
                return c;
            }

            static NkRendererConfig ForArchviz(NkGraphicsApi api = NkGraphicsApi::NK_GFX_API_VULKAN,
                                               uint32 w = 2560, uint32 h = 1440) {
                NkRendererConfig c;
                c.api=api; c.width=w; c.height=h;
                c.pipeline=NkPipelineMode::NK_DEFERRED;
                c.quality=NkRenderQuality::NK_ULTRA;
                c.subsystems = NK_SS_RENDER3D | NK_SS_SHADOW | NK_SS_POST_PROCESS
                             | NK_SS_OVERLAY;
                c.shadow.resolution=4096;
                c.postProcess.hbao=true;   c.postProcess.ssr=true;
                c.postProcess.bloom=false; c.postProcess.colorGrading=true;
                c.postProcess.taa=true;
                return c;
            }

            static NkRendererConfig ForMobile(NkGraphicsApi api = NkGraphicsApi::NK_GFX_API_OPENGLES,
                                              uint32 w = 1280, uint32 h = 720) {
                NkRendererConfig c;
                c.api=api; c.width=w; c.height=h;
                c.pipeline=NkPipelineMode::NK_FORWARD;
                c.quality=NkRenderQuality::NK_MOBILE;
                c.subsystems = NK_SS_RENDER2D | NK_SS_RENDER3D | NK_SS_TEXT;
                c.hdr=false;
                c.postProcess.bloom=false; c.postProcess.ssao=false;
                c.postProcess.fxaa=false;
                return c;
            }

            static NkRendererConfig For2D(NkGraphicsApi api = NkGraphicsApi::NK_GFX_API_OPENGL,
                                          uint32 w = 1920, uint32 h = 1080) {
                NkRendererConfig c;
                c.api=api; c.width=w; c.height=h;
                c.pipeline=NkPipelineMode::NK_FORWARD;
                c.quality=NkRenderQuality::NK_LOW;
                c.subsystems = NK_SS_RENDER2D | NK_SS_TEXT | NK_SS_UI | NK_SS_OVERLAY;
                c.hdr=false;
                return c;
            }

            static NkRendererConfig ForEditor(NkGraphicsApi api = NkGraphicsApi::NK_GFX_API_OPENGL,
                                              uint32 w = 2560, uint32 h = 1440) {
                NkRendererConfig c;
                c.api=api; c.width=w; c.height=h;
                c.pipeline=NkPipelineMode::NK_FORWARD_PLUS;
                c.quality=NkRenderQuality::NK_HIGH;
                c.subsystems = NK_SS_ALL;     // editor : tout activer
                c.debugOverlay=true;
                c.shadow.resolution=1024;
                c.postProcess.fxaa=true;
                return c;
            }

            // Minimal : juste un device + un command buffer, rien d'autre.
            // Utile pour tests ou pour batir son propre pipeline custom.
            static NkRendererConfig ForMinimal(NkGraphicsApi api = NkGraphicsApi::NK_GFX_API_OPENGL,
                                               uint32 w = 800, uint32 h = 600) {
                NkRendererConfig c;
                c.api=api; c.width=w; c.height=h;
                c.subsystems = NK_SS_NONE;
                c.hdr=false;
                return c;
            }

            static NkRendererConfig ForOffscreen(NkGraphicsApi api = NkGraphicsApi::NK_GFX_API_VULKAN,
                                                 uint32 w = 3840, uint32 h = 2160) {
                NkRendererConfig c = ForFilm(api, w, h);
                c.subsystems = c.subsystems | NK_SS_OFFSCREEN;
                c.vsync=false;
                return c;
            }
        };

    } // namespace renderer
} // namespace nkentseu
