#pragma once
// =============================================================================
// NkPostProcess.h
// Système de post-traitements pour NKRenderer.
//
// Effets disponibles :
//   • Tonemapping (Reinhard, ACES, Uncharted2, Filmic)
//   • Bloom (threshold + blur gaussien + composite)
//   • SSAO (Screen-Space Ambient Occlusion)
//   • TAA (Temporal Anti-Aliasing)
//   • Vignette
//   • Color grading (LUT 3D)
//   • Chromatic aberration
//   • Film grain
//   • Depth of Field (focus + bokeh simplifié)
//   • Motion blur
//   • FXAA (Fast Approximate Anti-Aliasing)
//
// Architecture :
//   NkPostProcessStack accumule les passes dans l'ordre.
//   Chaque effet est un NkPostProcessEffect avec ses paramètres.
//   Submit() enchaîne les passes via des ping-pong render targets.
// =============================================================================
#include "NKRenderer/Core/NkIRenderer.h"
#include "NKRenderer/Command/NkRendererCommand.h"

namespace nkentseu {
namespace renderer {

    // =========================================================================
    // Effets disponibles
    // =========================================================================
    enum class NkPostFX : uint8 {
        None             = 0,
        Tonemapping      = 1,
        Bloom            = 2,
        SSAO             = 3,
        TAA              = 4,
        Vignette         = 5,
        ChromaticAberr   = 6,
        FilmGrain        = 7,
        DepthOfField     = 8,
        MotionBlur       = 9,
        FXAA             = 10,
        ColorGrading     = 11,
        Sharpen          = 12,
        Fog              = 13,
    };

    // =========================================================================
    // Paramètres par effet
    // =========================================================================
    struct NkTonemapParams {
        enum class Mode : uint8 { Reinhard, ACES, Uncharted2, Filmic, Passthrough } mode = Mode::ACES;
        float32 exposure = 1.f;
        float32 gamma    = 2.2f;
        float32 contrast = 1.f;
        float32 saturation = 1.f;
    };

    struct NkBloomParams {
        float32 threshold  = 0.8f;    // luminance minimale pour le bloom
        float32 intensity  = 0.5f;
        float32 radius     = 4.f;     // rayon du flou en pixels
        uint32  passes     = 5;       // nombre de passes de flou
        bool    highQuality= false;
    };

    struct NkSSAOParams {
        uint32  numSamples  = 16;
        float32 radius      = 0.5f;
        float32 bias        = 0.025f;
        float32 power       = 2.f;
        uint32  blurPasses  = 2;
    };

    struct NkVignetteParams {
        float32 intensity = 0.4f;
        float32 smoothness= 0.6f;
        NkVec2f center    = {0.5f, 0.5f};
        NkVec3f color     = {0.f, 0.f, 0.f};
    };

    struct NkChromaticAberrParams {
        float32 intensity = 0.005f;   // déplacement des canaux R/B en UV
        NkVec2f direction = {1.f, 0.f};
    };

    struct NkFilmGrainParams {
        float32 intensity = 0.04f;
        float32 luminanceFactor = 1.f;  // plus de grain sur les zones sombres
        bool    animated  = true;
    };

    struct NkDOFParams {
        float32 focusDistance = 5.f;
        float32 focusRange    = 2.f;    // zone nette autour de focusDistance
        float32 blurRadius    = 6.f;    // rayon du flou en pixels
        bool    autofocus     = false;
    };

    struct NkMotionBlurParams {
        float32 strength  = 0.5f;
        uint32  numSamples= 8;
    };

    struct NkFogParams {
        enum class Type : uint8 { Linear, Exponential, ExponentialSquared } type = Type::Exponential;
        NkVec3f color    = {0.7f, 0.8f, 0.9f};
        float32 density  = 0.02f;     // pour Exp/Exp²
        float32 start    = 10.f;      // pour Linear
        float32 end      = 100.f;     // pour Linear
        float32 heightFalloff = 0.f;  // brouillard de hauteur (0 = désactivé)
    };

    struct NkColorGradingParams {
        NkVec3f lift     = {0.f, 0.f, 0.f};     // shadows
        NkVec3f gamma    = {1.f, 1.f, 1.f};     // midtones
        NkVec3f gain     = {1.f, 1.f, 1.f};     // highlights
        float32 hueShift = 0.f;                  // rotation teinte [-180, 180]
        bool    useLUT   = false;
        NkTextureHandle lut3D;                   // texture 3D 16³ ou 32³
    };

    // =========================================================================
    // Post-process pass (une entrée dans la stack)
    // =========================================================================
    struct NkPostProcessPass {
        NkPostFX type    = NkPostFX::None;
        bool     enabled = true;
        float32  weight  = 1.f;

        union Params {
            NkTonemapParams    tonemap;
            NkBloomParams      bloom;
            NkSSAOParams       ssao;
            NkVignetteParams   vignette;
            NkChromaticAberrParams chromAberr;
            NkFilmGrainParams  grain;
            NkDOFParams        dof;
            NkMotionBlurParams motionBlur;
            NkFogParams        fog;
            NkColorGradingParams colorGrading;
            float32            sharpness;   // pour Sharpen
            float32            fxaaQuality; // pour FXAA
            Params() : tonemap{} {}
        } params;
    };

    // =========================================================================
    // NkPostProcessStack — chaîne de post-traitements
    // =========================================================================
    class NkPostProcessStack {
    public:
        explicit NkPostProcessStack(NkIRenderer* renderer)
            : mRenderer(renderer) {}

        ~NkPostProcessStack() { Destroy(); }

        NkPostProcessStack(const NkPostProcessStack&) = delete;
        NkPostProcessStack& operator=(const NkPostProcessStack&) = delete;

        // ── Lifecycle ────────────────────────────────────────────────────────
        bool Init(uint32 width, uint32 height, NkTextureFormat format = NkTextureFormat::RGBA16F);
        void Destroy();
        void Resize(uint32 width, uint32 height);

        // ── Configuration de la stack ─────────────────────────────────────────
        // Ajouter/modifier un effet
        void SetEffect(NkPostFX fx, const NkPostProcessPass& pass);

        // Activer/désactiver un effet
        void SetEnabled(NkPostFX fx, bool enabled);

        // Preset standard (tonemap ACES + bloom + vignette + FXAA)
        void SetPreset_Cinematic();
        // Preset jeu vidéo (tonemap + bloom + SSAO + TAA)
        void SetPreset_Game();
        // Preset archiviz (tonemap filmic + DOF + léger grain)
        void SetPreset_Archiviz();

        // ── Render ───────────────────────────────────────────────────────────
        // Applique tous les effets activés sur hdrInput
        // Résultat dans le render target courant (ou swapchain si targetOut = null)
        // hdrInput : texture HDR du rendu principal
        // depth    : depth buffer (pour SSAO, DOF, fog)
        // velocity : motion vector (pour TAA, motion blur)
        void Apply(NkICommandBuffer* cmd,
                   NkTextureHandle hdrInput,
                   NkTextureHandle depth,
                   NkTextureHandle velocity = NkTextureHandle::Null(),
                   NkRenderTargetHandle targetOut = NkRenderTargetHandle::Null());

        // ── Accesseurs ────────────────────────────────────────────────────────
        NkTextureHandle GetOutput()  const { return mPingPong[mCurrentPP]; }
        bool            IsValid()    const { return mPingPong[0].IsValid(); }

        // Temps d'application GPU (si les queries sont disponibles)
        float32 GetGPUTimeMs() const { return mLastGPUTimeMs; }

    private:
        void ApplyTonemap    (NkICommandBuffer* cmd, NkTextureHandle in, NkTextureHandle out, const NkTonemapParams&);
        void ApplyBloom      (NkICommandBuffer* cmd, NkTextureHandle in, NkTextureHandle out, const NkBloomParams&);
        void ApplySSAO       (NkICommandBuffer* cmd, NkTextureHandle in, NkTextureHandle depth, NkTextureHandle out, const NkSSAOParams&);
        void ApplyVignette   (NkICommandBuffer* cmd, NkTextureHandle in, NkTextureHandle out, const NkVignetteParams&);
        void ApplyFXAA       (NkICommandBuffer* cmd, NkTextureHandle in, NkTextureHandle out, float32 quality);
        void ApplyFog        (NkICommandBuffer* cmd, NkTextureHandle in, NkTextureHandle depth, NkTextureHandle out, const NkFogParams&);
        void ApplyFilmGrain  (NkICommandBuffer* cmd, NkTextureHandle in, NkTextureHandle out, const NkFilmGrainParams&, float32 time);
        void ApplyColorGrading(NkICommandBuffer* cmd, NkTextureHandle in, NkTextureHandle out, const NkColorGradingParams&);
        void ApplyChromaticAberr(NkICommandBuffer* cmd, NkTextureHandle in, NkTextureHandle out, const NkChromaticAberrParams&);

        // Swap des ping-pong render targets
        NkTextureHandle NextPP();

        NkIRenderer*             mRenderer = nullptr;
        uint32                   mWidth    = 0;
        uint32                   mHeight   = 0;
        NkTextureFormat          mFormat   = NkTextureFormat::RGBA16F;

        // Ping-pong render targets pour la chaîne de post-traitements
        NkTextureHandle          mPingPong[2];
        NkRenderTargetHandle     mPingPongRT[2];
        uint32                   mCurrentPP = 0;

        // Render target intermédiaire pour le bloom (demi-résolution)
        NkTextureHandle          mBloomBuffer;
        NkRenderTargetHandle     mBloomRT;

        // Passes
        NkVector<NkPostProcessPass> mPasses;

        float32 mLastGPUTimeMs = 0.f;
        float32 mTime          = 0.f;

        // Matériaux pour chaque effet (créés à la demande)
        struct EffectMaterials {
            NkMaterialHandle     mat;
            NkMaterialInstHandle inst;
        };
        NkHashMap<uint32, EffectMaterials> mEffectMaterials;
    };

    // =========================================================================
    // Implémentation inline
    // =========================================================================
    inline bool NkPostProcessStack::Init(uint32 width, uint32 height, NkTextureFormat format) {
        mWidth = width; mHeight = height; mFormat = format;

        for (int i = 0; i < 2; ++i) {
            NkTextureDesc td = NkTextureDesc::RenderTarget(width, height, format);
            char name[32]; snprintf(name, sizeof(name), "PostProcess_PingPong_%d", i);
            td.debugName = name;
            mPingPong[i] = mRenderer->CreateTexture(td);
            if (!mPingPong[i].IsValid()) return false;
        }
        return true;
    }

    inline void NkPostProcessStack::Destroy() {
        for (int i=0;i<2;++i){
            if(mPingPong[i].IsValid())   mRenderer->DestroyTexture(mPingPong[i]);
            if(mPingPongRT[i].IsValid()) mRenderer->DestroyRenderTarget(mPingPongRT[i]);
        }
        if(mBloomBuffer.IsValid()) mRenderer->DestroyTexture(mBloomBuffer);
        if(mBloomRT.IsValid())     mRenderer->DestroyRenderTarget(mBloomRT);
        mEffectMaterials.Clear();
        mPasses.Clear();
    }

    inline void NkPostProcessStack::Resize(uint32 width, uint32 height) {
        if (mWidth == width && mHeight == height) return;
        Destroy();
        Init(width, height, mFormat);
    }

    inline void NkPostProcessStack::SetEffect(NkPostFX fx, const NkPostProcessPass& pass) {
        for (usize i = 0; i < mPasses.Size(); ++i) {
            if (mPasses[i].type == fx) { mPasses[i] = pass; return; }
        }
        mPasses.PushBack(pass);
    }

    inline void NkPostProcessStack::SetEnabled(NkPostFX fx, bool enabled) {
        for (usize i = 0; i < mPasses.Size(); ++i) {
            if (mPasses[i].type == fx) { mPasses[i].enabled = enabled; return; }
        }
    }

    inline void NkPostProcessStack::SetPreset_Cinematic() {
        mPasses.Clear();
        NkPostProcessPass t{NkPostFX::Tonemapping}; t.params.tonemap.mode=NkTonemapParams::Mode::ACES; t.params.tonemap.exposure=1.1f; mPasses.PushBack(t);
        NkPostProcessPass b{NkPostFX::Bloom}; b.params.bloom.threshold=0.7f; b.params.bloom.intensity=0.4f; mPasses.PushBack(b);
        NkPostProcessPass v{NkPostFX::Vignette}; v.params.vignette.intensity=0.35f; mPasses.PushBack(v);
        NkPostProcessPass g{NkPostFX::FilmGrain}; g.params.grain.intensity=0.03f; mPasses.PushBack(g);
        NkPostProcessPass f{NkPostFX::FXAA}; mPasses.PushBack(f);
    }

    inline void NkPostProcessStack::SetPreset_Game() {
        mPasses.Clear();
        NkPostProcessPass t{NkPostFX::Tonemapping}; t.params.tonemap.mode=NkTonemapParams::Mode::ACES; mPasses.PushBack(t);
        NkPostProcessPass b{NkPostFX::Bloom}; b.params.bloom.threshold=0.9f; b.params.bloom.intensity=0.3f; mPasses.PushBack(b);
        NkPostProcessPass v{NkPostFX::Vignette}; v.params.vignette.intensity=0.2f; mPasses.PushBack(v);
        NkPostProcessPass f{NkPostFX::FXAA}; mPasses.PushBack(f);
    }

    inline void NkPostProcessStack::SetPreset_Archiviz() {
        mPasses.Clear();
        NkPostProcessPass t{NkPostFX::Tonemapping}; t.params.tonemap.mode=NkTonemapParams::Mode::Filmic; t.params.tonemap.exposure=0.9f; mPasses.PushBack(t);
        NkPostProcessPass d{NkPostFX::DepthOfField}; d.params.dof.focusDistance=5.f; d.params.dof.blurRadius=4.f; mPasses.PushBack(d);
        NkPostProcessPass g{NkPostFX::FilmGrain}; g.params.grain.intensity=0.02f; mPasses.PushBack(g);
        NkPostProcessPass v{NkPostFX::Vignette}; v.params.vignette.intensity=0.25f; mPasses.PushBack(v);
    }

    inline void NkPostProcessStack::Apply(NkICommandBuffer* /*cmd*/,
                                           NkTextureHandle hdrInput,
                                           NkTextureHandle /*depth*/,
                                           NkTextureHandle /*velocity*/,
                                           NkRenderTargetHandle /*targetOut*/)
    {
        // La chaîne de post-traitements commence sur hdrInput
        // Chaque effet lit de mPingPong[mCurrentPP] et écrit dans l'autre
        mCurrentPP = 0;
        (void)hdrInput;
        // TODO : implémenter les passes effectives en utilisant les matériaux
        // créés à la demande pour chaque effet (fullscreen quad + shader dédié)
        mTime += 0.016f;
    }

} // namespace renderer
} // namespace nkentseu