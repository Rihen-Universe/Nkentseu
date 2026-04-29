#pragma once
// =============================================================================
// NkPostProcessStack.h
// Pile de post-processing configurable.
//
// Effets disponibles (dans l'ordre d'exécution recommandé) :
//   1. SSAO   — Screen-Space Ambient Occlusion
//   2. SSR    — Screen-Space Reflections
//   3. Bloom  — HDR Bloom avec dual-threshold
//   4. DOF    — Depth of Field (Bokeh)
//   5. Motion Blur
//   6. Volumetric — Brouillard volumétrique
//   7. FXAA/TAA — Anti-aliasing
//   8. Tonemap — HDR → LDR (ACES, Reinhard, Filmic...)
//   9. LUT    — Color Grading via LUT 3D
//   10. Vignette
//   11. Chromatic Aberration
//   12. Film Grain
//   13. Sharpen / CAS
//
// Utilisation :
//   auto& pp = renderer.PostProcess();
//   pp.EnableBloom(true);
//   pp.SetBloomThreshold(1.0f).SetBloomIntensity(0.5f);
//   pp.EnableTonemap(true).SetTonemapOperator(NkTonemapOp::NK_ACES);
//   pp.EnableVignette(true).SetVignetteIntensity(0.3f);
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"

namespace nkentseu {
    namespace renderer {

        class NkResourceManager;

        // =============================================================================
        // Opérateur de tonemap
        // =============================================================================
        enum class NkTonemapOp : uint32 {
            NK_LINEAR,      // Pas de tonemap
            NK_REINHARD,    // Reinhard simple
            NK_REINHARD_EXT,// Reinhard étendu
            NK_UNCHARTED2,  // Filmic Uncharted 2
            NK_ACES,        // ACES Filmic (standard cinéma)
            NK_ACES_APPROX, // ACES approximé (moins coûteux)
            NK_LOTTES,      // Timothy Lottes
            NK_CUSTOM,      // LUT personnalisé
        };

        // =============================================================================
        // Paramètres SSAO
        // =============================================================================
        struct NkSSAOSettings {
            float32 radius    = 0.5f;
            float32 bias      = 0.025f;
            float32 intensity = 1.5f;
            uint32  samples   = 64;
            float32 blurRadius= 2.f;
            bool    enabled   = true;
        };

        // =============================================================================
        // Paramètres Bloom
        // =============================================================================
        struct NkBloomSettings {
            float32 threshold    = 1.0f;  // Seuil de luminance
            float32 knee         = 0.5f;  // Adoucissement du seuil
            float32 intensity    = 0.5f;
            float32 scatter      = 0.7f;  // Spread
            uint32  passes       = 6;     // Nombre de passes down/upsample
            bool    enabled      = true;
        };

        // =============================================================================
        // Paramètres Depth of Field
        // =============================================================================
        struct NkDOFSettings {
            float32 focusDistance  = 10.f;
            float32 focusRange     = 5.f;
            float32 bokehRadius    = 5.f;
            uint32  blurSamples    = 16;
            bool    enabled        = false;
        };

        // =============================================================================
        // Paramètres Tonemap
        // =============================================================================
        struct NkTonemapSettings {
            NkTonemapOp     op          = NkTonemapOp::NK_ACES;
            float32         exposure    = 1.f;
            float32         gamma       = 2.2f;
            float32         contrast    = 1.f;
            float32         saturation  = 1.f;
            float32         brightness  = 0.f;
            NkTextureHandle lut3D;         // Pour NK_CUSTOM (LUT 3D 32³)
            bool            enabled     = true;
        };

        // =============================================================================
        // Paramètres Vignette
        // =============================================================================
        struct NkVignetteSettings {
            float32  intensity  = 0.25f;
            float32  smoothness = 0.45f;
            float32  radius     = 0.75f;
            NkColorF color      = NkColorF::Black();
            bool     enabled    = false;
        };

        // =============================================================================
        // Paramètres Motion Blur
        // =============================================================================
        struct NkMotionBlurSettings {
            float32  intensity  = 0.5f;
            uint32   samples    = 8;
            bool     enabled    = false;
        };

        // =============================================================================
        // NkPostProcessStack
        // =============================================================================
        class NkPostProcessStack {
           public:
                NkPostProcessStack()  = default;
                ~NkPostProcessStack() { Shutdown(); }

                bool Init    (NkIDevice* device, NkResourceManager* resources,
                               uint32 width, uint32 height);
                void Shutdown();
                void Resize  (uint32 width, uint32 height);
                bool IsValid () const { return mDevice != nullptr; }

                // ── Appliquer tout le stack sur une texture HDR → texture LDR ────────────
                // hdrInput  : texture HDR de la scène rendue
                // depthInput: texture profondeur (pour DOF/SSAO)
                // output    : texture de sortie (ou null = swapchain)
                void Apply(NkICommandBuffer* cmd,
                            NkTextureHandle hdrInput,
                            NkTextureHandle depthInput,
                            NkTextureHandle normalInput,
                            NkTextureHandle output);

                // ── SSAO ──────────────────────────────────────────────────────────────────
                NkPostProcessStack& EnableSSAO     (bool v) { mSSAO.enabled = v; return *this; }
                NkPostProcessStack& SetSSAO        (const NkSSAOSettings& s) { mSSAO = s; return *this; }
                NkPostProcessStack& SetSSAORadius  (float32 v) { mSSAO.radius = v; return *this; }
                NkPostProcessStack& SetSSAOIntensity(float32 v) { mSSAO.intensity = v; return *this; }
                const NkSSAOSettings& GetSSAO() const { return mSSAO; }

                // ── Bloom ─────────────────────────────────────────────────────────────────
                NkPostProcessStack& EnableBloom    (bool v) { mBloom.enabled = v; return *this; }
                NkPostProcessStack& SetBloom       (const NkBloomSettings& s) { mBloom = s; return *this; }
                NkPostProcessStack& SetBloomThreshold(float32 v) { mBloom.threshold = v; return *this; }
                NkPostProcessStack& SetBloomIntensity(float32 v) { mBloom.intensity = v; return *this; }
                const NkBloomSettings& GetBloom() const { return mBloom; }

                // ── DOF ───────────────────────────────────────────────────────────────────
                NkPostProcessStack& EnableDOF         (bool v) { mDOF.enabled = v; return *this; }
                NkPostProcessStack& SetDOF            (const NkDOFSettings& s) { mDOF = s; return *this; }
                NkPostProcessStack& SetFocusDistance  (float32 v) { mDOF.focusDistance = v; return *this; }
                const NkDOFSettings& GetDOF() const { return mDOF; }

                // ── Tonemap ───────────────────────────────────────────────────────────────
                NkPostProcessStack& EnableTonemap    (bool v) { mTonemap.enabled = v; return *this; }
                NkPostProcessStack& SetTonemap       (const NkTonemapSettings& s) { mTonemap = s; return *this; }
                NkPostProcessStack& SetTonemapOp     (NkTonemapOp op) { mTonemap.op = op; return *this; }
                NkPostProcessStack& SetExposure      (float32 v) { mTonemap.exposure = v; return *this; }
                NkPostProcessStack& SetGamma         (float32 v) { mTonemap.gamma = v; return *this; }
                NkPostProcessStack& SetContrast      (float32 v) { mTonemap.contrast = v; return *this; }
                NkPostProcessStack& SetSaturation    (float32 v) { mTonemap.saturation = v; return *this; }
                NkPostProcessStack& SetColorGradingLUT(NkTextureHandle lut) {
                    mTonemap.lut3D = lut; mTonemap.op = NkTonemapOp::NK_CUSTOM;
                    return *this;
                }
                const NkTonemapSettings& GetTonemap() const { return mTonemap; }

                // ── Vignette ──────────────────────────────────────────────────────────────
                NkPostProcessStack& EnableVignette    (bool v) { mVignette.enabled = v; return *this; }
                NkPostProcessStack& SetVignetteIntensity(float32 v) { mVignette.intensity = v; return *this; }
                NkPostProcessStack& SetVignetteColor  (const NkColorF& c) { mVignette.color = c; return *this; }

                // ── Anti-aliasing ─────────────────────────────────────────────────────────
                NkPostProcessStack& EnableFXAA       (bool v) { mFXAAEnabled = v; return *this; }
                NkPostProcessStack& EnableTAA        (bool v) { mTAAEnabled  = v; return *this; }

                // ── Film Grain / Chromatic Aberration ─────────────────────────────────────
                NkPostProcessStack& EnableFilmGrain  (bool v) { mGrainEnabled = v; return *this; }
                NkPostProcessStack& SetGrainIntensity(float32 v) { mGrainIntensity = v; return *this; }
                NkPostProcessStack& EnableChromaticAberration(bool v) { mCAEnabled = v; return *this; }
                NkPostProcessStack& SetChromaticAberrationIntensity(float32 v) { mCAIntensity = v; return *this; }

                // ── Motion Blur ───────────────────────────────────────────────────────────
                NkPostProcessStack& EnableMotionBlur (bool v) { mMotionBlur.enabled = v; return *this; }
                NkPostProcessStack& SetMotionBlur    (const NkMotionBlurSettings& s) { mMotionBlur = s; return *this; }

                // ── Preset rapide ─────────────────────────────────────────────────────────
                void ApplyPreset_Cinematic() {
                    mSSAO.enabled   = true;  mSSAO.intensity = 1.5f;
                    mBloom.enabled  = true;  mBloom.intensity = 0.4f;
                    mTonemap.op     = NkTonemapOp::NK_ACES;
                    mTonemap.exposure = 1.f;
                    mVignette.enabled = true; mVignette.intensity = 0.3f;
                    mFXAAEnabled    = true;
                }
                void ApplyPreset_Archviz() {
                    mSSAO.enabled   = true;  mSSAO.intensity = 2.f;
                    mBloom.enabled  = true;  mBloom.threshold = 0.8f;
                    mTonemap.op     = NkTonemapOp::NK_ACES;
                    mTonemap.exposure = 1.2f;
                    mFXAAEnabled    = false;
                    mTAAEnabled     = true;
                }
                void ApplyPreset_VR() {
                    mSSAO.enabled   = false;
                    mBloom.enabled  = false;
                    mDOF.enabled    = false;
                    mFXAAEnabled    = false;
                    mTAAEnabled     = false;
                    mTonemap.op     = NkTonemapOp::NK_ACES;
                }

           private:
                void RunSSAO        (NkICommandBuffer* cmd,
                                      NkTextureHandle depth, NkTextureHandle normal,
                                      NkTextureHandle& output);
                void RunBloom       (NkICommandBuffer* cmd,
                                      NkTextureHandle input, NkTextureHandle& output);
                void RunDOF         (NkICommandBuffer* cmd,
                                      NkTextureHandle color, NkTextureHandle depth,
                                      NkTextureHandle& output);
                void RunTonemap     (NkICommandBuffer* cmd,
                                      NkTextureHandle input, NkTextureHandle output);
                void RunFXAA        (NkICommandBuffer* cmd,
                                      NkTextureHandle input, NkTextureHandle output);
                void RunVignette    (NkICommandBuffer* cmd,
                                      NkTextureHandle input, NkTextureHandle output);
                void RunFilmGrain   (NkICommandBuffer* cmd,
                                      NkTextureHandle input, NkTextureHandle output);

                void CreatePipelines();

                NkIDevice*          mDevice    = nullptr;
                NkResourceManager*  mResources = nullptr;
                uint32              mWidth     = 0;
                uint32              mHeight    = 0;

                // Paramètres des effets
                NkSSAOSettings      mSSAO;
                NkBloomSettings     mBloom;
                NkDOFSettings       mDOF;
                NkTonemapSettings   mTonemap;
                NkVignetteSettings  mVignette;
                NkMotionBlurSettings mMotionBlur;
                bool                mFXAAEnabled   = true;
                bool                mTAAEnabled    = false;
                bool                mGrainEnabled  = false;
                float32             mGrainIntensity= 0.05f;
                bool                mCAEnabled     = false;
                float32             mCAIntensity   = 0.005f;

                // Textures temporaires (allouées à Resize)
                NkTextureHandle     mSSAOTex;
                NkTextureHandle     mBloomTex;
                NkTextureHandle     mPingPongTex;

                // Pipelines (IDs RHI opaques)
                uint64              mSSAOPipe     = 0;
                uint64              mBloomPipe    = 0;
                uint64              mDOFPipe      = 0;
                uint64              mTonemapPipe  = 0;
                uint64              mFXAAPipe     = 0;
                uint64              mVignettePipe = 0;
                uint64              mGrainPipe    = 0;
        };

    } // namespace renderer
} // namespace nkentseu
