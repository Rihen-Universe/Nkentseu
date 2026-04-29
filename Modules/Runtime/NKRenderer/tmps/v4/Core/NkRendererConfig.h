#pragma once
// =============================================================================
// NkRendererConfig.h
// Configuration d'initialisation du NkRenderer.
// Toutes les options de feature toggle sont ici.
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"

namespace nkentseu {
    namespace renderer {

        // =============================================================================
        // Qualité des ombres
        // =============================================================================
        enum class NkShadowQuality : uint32 {
            NK_SHADOW_OFF  = 0,
            NK_SHADOW_LOW,       // 512x512, 1 cascade
            NK_SHADOW_MEDIUM,    // 1024x1024, 2 cascades
            NK_SHADOW_HIGH,      // 2048x2048, 4 cascades
            NK_SHADOW_ULTRA,     // 4096x4096, 4 cascades + PCSS
        };

        // =============================================================================
        // Qualité du rendu ambient occlusion
        // =============================================================================
        enum class NkSSAOQuality : uint32 {
            NK_SSAO_OFF = 0,
            NK_SSAO_LOW,
            NK_SSAO_MEDIUM,
            NK_SSAO_HIGH,
        };

        // =============================================================================
        // Pipeline de rendu 3D
        // =============================================================================
        enum class NkRenderPipeline : uint32 {
            NK_FORWARD,          // Forward classique (simple, mobile-friendly)
            NK_DEFERRED,         // Deferred PBR (haute qualité)
            NK_FORWARD_PLUS,     // Forward+ (clustering, nombreuses lumières)
        };

        // =============================================================================
        // NkRendererConfig — configuration complète
        // =============================================================================
        struct NkRendererConfig {
            // ── Dimensions initiales ──────────────────────────────────────────────────
            uint32  width            = 1280;
            uint32  height           = 720;

            // ── Pipeline ─────────────────────────────────────────────────────────────
            NkRenderPipeline pipeline        = NkRenderPipeline::NK_DEFERRED;
            NkRenderMode     defaultMode     = NkRenderMode::NK_SOLID;

            // ── Features 3D ───────────────────────────────────────────────────────────
            NkShadowQuality shadowQuality    = NkShadowQuality::NK_SHADOW_HIGH;
            NkSSAOQuality   ssaoQuality      = NkSSAOQuality::NK_SSAO_MEDIUM;
            bool            enableIBL        = true;   // Image-Based Lighting
            bool            enableBloom      = true;
            bool            enableDOF        = false;  // Depth of Field
            bool            enableFXAA       = true;
            bool            enableTAA        = false;  // Temporal Anti-Aliasing
            bool            enableMotionBlur = false;
            bool            enableSSR        = false;  // Screen-Space Reflections
            bool            enableVolumetric = false;  // Volumetric lighting/fog
            bool            enableDecals     = true;
            bool            enableTerrain    = false;
            bool            enableVFX        = true;

            // ── Features 2D ───────────────────────────────────────────────────────────
            bool            enableText       = true;
            bool            enableUI         = true;

            // ── Limites ───────────────────────────────────────────────────────────────
            uint32  maxParticles             = 100000;
            uint32  maxLights                = 256;
            uint32  maxInstances             = 65536;
            uint32  max2DVertices            = 65536;
            uint32  max2DIndices             = 98304;
            uint32  maxShadowCasters         = 1024;

            // ── Chemins de ressources ─────────────────────────────────────────────────
            const char* fontSearchPath       = "Resources/Fonts";
            const char* textureSearchPath    = "Resources/Textures";
            const char* modelSearchPath      = "Resources/Models";
            const char* shaderCachePath      = "Build/ShaderCache";
            const char* defaultHDRI          = nullptr;

            // ── Debug ─────────────────────────────────────────────────────────────────
            bool    showStats                = false;
            bool    showGizmos               = false;
            bool    enableGPUProfiling       = false;

            // ── Presets ───────────────────────────────────────────────────────────────

            // Preset pour applications temps-réel haute-fidélité (film, archiviz)
            static NkRendererConfig HighFidelity() {
                NkRendererConfig c;
                c.pipeline         = NkRenderPipeline::NK_DEFERRED;
                c.shadowQuality    = NkShadowQuality::NK_SHADOW_ULTRA;
                c.ssaoQuality      = NkSSAOQuality::NK_SSAO_HIGH;
                c.enableIBL        = true;
                c.enableBloom      = true;
                c.enableDOF        = true;
                c.enableTAA        = true;
                c.enableSSR        = true;
                c.enableVolumetric = true;
                return c;
            }

            // Preset pour simulation temps-réel
            static NkRendererConfig Simulation() {
                NkRendererConfig c;
                c.pipeline         = NkRenderPipeline::NK_FORWARD_PLUS;
                c.shadowQuality    = NkShadowQuality::NK_SHADOW_MEDIUM;
                c.ssaoQuality      = NkSSAOQuality::NK_SSAO_LOW;
                c.enableIBL        = true;
                c.enableBloom      = false;
                c.maxLights        = 512;
                c.maxParticles     = 500000;
                return c;
            }

            // Preset pour VR/AR (performance critique)
            static NkRendererConfig VR() {
                NkRendererConfig c;
                c.pipeline         = NkRenderPipeline::NK_FORWARD;
                c.shadowQuality    = NkShadowQuality::NK_SHADOW_LOW;
                c.ssaoQuality      = NkSSAOQuality::NK_SSAO_OFF;
                c.enableIBL        = true;
                c.enableBloom      = false;
                c.enableDOF        = false;
                c.enableFXAA       = false;
                return c;
            }

            // Preset pour archiviz statique haute-qualité
            static NkRendererConfig Archviz() {
                NkRendererConfig c = HighFidelity();
                c.enableDecals     = true;
                c.enableTerrain    = false;
                c.enableVFX        = false;
                c.maxLights        = 64;
                return c;
            }

            // Preset minimal (2D seulement, ex: outils, UI)
            static NkRendererConfig Minimal2D() {
                NkRendererConfig c;
                c.pipeline         = NkRenderPipeline::NK_FORWARD;
                c.shadowQuality    = NkShadowQuality::NK_SHADOW_OFF;
                c.ssaoQuality      = NkSSAOQuality::NK_SSAO_OFF;
                c.enableIBL        = false;
                c.enableBloom      = false;
                c.enableFXAA       = false;
                c.enableVFX        = false;
                c.maxLights        = 0;
                return c;
            }
        };

    } // namespace renderer
} // namespace nkentseu
