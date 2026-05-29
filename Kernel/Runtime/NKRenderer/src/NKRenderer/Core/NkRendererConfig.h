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
#include "NKContainers/String/NkString.h"
#include "NKMath/NkVec.h"

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
            // Bloom (inline 13-sample cross dans tonemap ; dual-Kawase multi-pass a venir)
            bool    bloom             = true;
            float32 bloomThreshold    = 0.85f;  // pixels > 0.85 HDR recoivent du bloom
            float32 bloomStrength     = 1.5f;   // intensite de la halo
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
            // Phase L : 3D LUT cinema (16^3 par defaut identity).
            // Si lutStrength > 0, le tonemap blend la mapped color avec LUT(mapped).
            // User peut uploader son LUT custom via NkRenderer::SetColorGradingLUT().
            float32 lutStrength       = 0.f;     // 0 = no grading, 1 = full LUT applied
            uint32  lutSize           = 16;      // resolution du LUT 3D (16/32/64)
            // Phase L : Auto-exposure V0 simple — le tonemap sample le centre
            // du bloom RT (assume Dual-Kawase upsample = bonne moyenne) et adapte
            // l'exposure pour que la moyenne mappe vers `autoExposureKey`.
            // 0 = no auto, 1 = full auto-exp (override user exposure).
            // V1 future : compute reduction proper + eye adaptation temporelle.
            float32 autoExposureStrength = 0.f;
            float32 autoExposureKey      = 0.18f;  // mid-gray target (Reinhard standard)
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
            float32 iblStrength        = 0.3f;     // [0..1] multiplicateur du terme ambient IBL
            uint32  irradianceMapSize  = 32;       // diffuse env (32x32 cubemap suffit)
            uint32  specularMapSize    = 256;      // GGX prefiltered (mips = roughness)
            uint32  brdfLUTSize        = 512;      // 2D R16G16
            uint32  prefilterMipCount  = 6;        // mips de la specular map

            // ── Phase N v0 : source IBL parametrable par l'application ──────
            // useHDR=false (default) -> gradient sky procedural avec les
            //   couleurs skyTop / horizon / ground ci-dessous.
            // useHDR=true + hdrPath non vide -> charge un .hdr equirect 360
            //   et l'utilise comme source pour les convolutions IBL CPU.
            //   Cache disque automatique (cf. NkEnvironmentSystem).
            bool    useHDR             = false;
            NkString hdrPath           = "";

            // Couleurs du gradient procedural (si useHDR=false).
            math::NkVec3f skyTop       = {0.40f, 0.55f, 0.80f};
            math::NkVec3f horizon      = {0.45f, 0.48f, 0.52f};
            math::NkVec3f ground       = {0.10f, 0.08f, 0.06f};

            // Phase N v0.5 : afficher le cubemap d'environnement en arriere-plan
            // (skybox). Sinon le fond reste celui du clear color du framebuffer.
            // Recommande : true quand useHDR=true pour voir l'environnement HDR
            // entier (pas juste son reflet sur les objets).
            bool          drawSkybox   = false;
        };

        struct NkClusterConfig {
            uint32 tilesX     = 16;
            uint32 tilesY     = 9;
            uint32 sliceCount = 24;     // depth slices (logarithmique)
            uint32 maxLightsPerCluster = 256;
        };

        // =====================================================================
        // NkUnitSystem — Echelle spatiale globale (style Blender).
        //
        // Convention : 1 unite world-space = `metersPerUnit` metres reels.
        //   metersPerUnit = 1.f    -> 1 unit = 1 metre (defaut, style Blender/
        //                              Godot/Unity).
        //   metersPerUnit = 0.001f -> 1 unit = 1 millimetre (micro-scenes,
        //                              molecules, microscopie).
        //   metersPerUnit = 1000.f -> 1 unit = 1 kilometre (planetes, espace,
        //                              terrain immense).
        //
        // Cette echelle affecte les conversions "mesure physique reelle <->
        // coordonnees world" : triplanar tile size (m), distance attenuation
        // lights (m), camera near/far par defaut, vitesse caracteres, audio
        // 3D, etc.
        //
        // Le shader recoit metersPerUnit via les UBOs concernes (ex. ObjectUBO
        // .triplanarParams.y). Convertir une distance metres -> units :
        //   units = metres / metersPerUnit.
        //
        // ⚠ Unreal Engine utilise 1 unit = 1 cm (legacy Quake-era). On suit
        // Blender (1 unit = 1 m) car c'est le standard moderne et plus
        // intuitif pour les artistes.
        // =====================================================================
        struct NkUnitSystem {
            float32 metersPerUnit = 1.f;

            // m -> units : pratique pour ecrire "MetersToUnits(0.5f)" et que
            // ca donne le scale correct quelle que soit l'echelle globale.
            float32 MetersToUnits(float32 meters) const noexcept {
                return metersPerUnit > 0.f ? meters / metersPerUnit : meters;
            }
            float32 UnitsToMeters(float32 units) const noexcept {
                return units * metersPerUnit;
            }
        };

        // Accesseurs globaux. Initial : metersPerUnit = 1.0f (1 unit = 1 m).
        // Modifier via NkSetUnits() AVANT de creer la scene si tu veux
        // travailler en mm ou en km — apres coup, tous les materials, lights,
        // cameras, etc. devraient idealement etre re-scales (TODO V1).
        //
        // Inline pour zero setup : pas de .cpp dedie a NkRendererConfig.
        // Storage Meyers singleton — initialise lazy au premier appel,
        // thread-safe C++11.
        inline NkUnitSystem& NkUnitsMutable() noexcept {
            static NkUnitSystem sUnits;
            return sUnits;
        }
        inline const NkUnitSystem& NkUnits() noexcept {
            return NkUnitsMutable();
        }
        inline void NkSetUnits(const NkUnitSystem& u) noexcept {
            NkUnitsMutable() = u;
        }

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

            // ─── Frames in flight (ring buffer per-frame UBO) ───────────────
            // Combien de copies des UBOs per-frame (camera, lights, object) le
            // renderer maintient pour eviter que le CPU stalle quand il ecrit
            // un buffer encore lu par le GPU.
            //   1 = pas de ring (un seul buffer partage)
            //         -> WriteBuffer peut stall si GPU lit encore. Plus
            //            economique en VRAM mais cap typique ~60-80 fps.
            //   2 = double buffering (defaut)
            //         -> CPU ecrit slot[(N+1)%2] pendant que GPU lit slot[N%2].
            //            Coût VRAM : 2x les UBOs per-frame. Recommande.
            //   3 = triple buffering
            //         -> Marge supplementaire pour smooth-out les frames lourdes
            //            (utile en VR ou cinematic). Coût : 3x VRAM.
            // Clampe a [1,3] dans NkRendererImpl::Initialize().
            uint32             framesInFlight = 2;

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
