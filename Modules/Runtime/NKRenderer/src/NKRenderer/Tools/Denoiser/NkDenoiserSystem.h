#pragma once
// =============================================================================
// NkDenoiserSystem.h  — NKRenderer v4.0  (Tools/Denoiser/)
//
// Débruitage de sorties de rendu (RT, path-tracing, AO stochastique).
//
// Backends disponibles :
//   NK_DENOISE_OIDN    : Intel Open Image Denoise (CPU/GPU, offline/interactive)
//   NK_DENOISE_NRD     : NVIDIA RealTime Denoising (ReLAX / ReBLUR — Vulkan/DX12)
//   NK_DENOISE_SPATIAL : filtre bilatéral spatial (fallback universel)
//   NK_DENOISE_TAA     : accumulation temporelle uniquement (très léger)
//
// Les signaux d'entrée optionnels (albedo + normal) améliorent grandement
// la qualité sur OIDN (mode « beauty » vs « beauty+aux »).
//
// Usage :
//   sys.Init(device, NkDenoiserBackend::NK_DENOISE_OIDN, cfg);
//   // par frame
//   sys.Denoise(cmd, {
//       .colorIn  = rtOutput,
//       .albedoIn = gbufAlbedo,  // optionnel
//       .normalIn = gbufNormal,  // optionnel
//       .depthIn  = gbufDepth,   // optionnel (reprojection temporelle)
//       .motionIn = velocity,    // optionnel (TAA)
//       .colorOut = denoisedRT,
//   });
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
namespace renderer {

    // =========================================================================
    // Backend de débruitage
    // =========================================================================
    enum class NkDenoiserBackend : uint8 {
        NK_DENOISE_OIDN    = 0,  // Intel OIDN
        NK_DENOISE_NRD     = 1,  // NVIDIA NRD (ReLAX / ReBLUR)
        NK_DENOISE_SPATIAL = 2,  // Bilatéral CPU/GPU (universel)
        NK_DENOISE_TAA     = 3,  // Accumulation temporelle seule
        NK_DENOISE_AUTO    = 4,  // Sélection automatique selon le device
    };

    // =========================================================================
    // Mode NRD (si backend = NK_DENOISE_NRD)
    // =========================================================================
    enum class NkNRDMode : uint8 {
        NK_NRD_REBLUR  = 0, // Diffuse + Specular — path-tracing stochastique
        NK_NRD_RELAX   = 1, // Spécialisé pour lumières directes/indirectes
        NK_NRD_SIGMA   = 2, // Ombres
    };

    // =========================================================================
    // Configuration
    // =========================================================================
    struct NkDenoiserConfig {
        NkDenoiserBackend backend       = NkDenoiserBackend::NK_DENOISE_AUTO;
        NkNRDMode         nrdMode       = NkNRDMode::NK_NRD_REBLUR;
        bool              temporal      = true;   // accumulation temporelle
        uint32            historyFrames = 8;       // frames à accumuler (TAA/NRD)
        float32           spatialRadius = 1.5f;    // rayon filtre bilatéral
        uint32            spatialPasses = 3;
        float32           blendFactor   = 0.1f;    // 0=full history, 1=no history
        bool              hdr           = true;    // entrée en HDR
        bool              useAlbedo     = true;    // guide albedo (qualité++)
        bool              useNormal     = true;    // guide normal (qualité++)
        bool              useMotion     = true;    // reprojection motion
    };

    // =========================================================================
    // Descripteur d'un appel Denoise()
    // =========================================================================
    struct NkDenoiseRequest {
        NkTexHandle colorIn;     // image bruitée (HDR RGBA16F)
        NkTexHandle albedoIn;    // G-buffer albedo (optionnel)
        NkTexHandle normalIn;    // G-buffer normal (optionnel)
        NkTexHandle depthIn;     // G-buffer depth  (optionnel, pour reprojection)
        NkTexHandle motionIn;    // motion vectors  (optionnel, pour TAA)
        NkTexHandle colorOut;    // résultat débruité (doit être alloué)
        float32     exposure  = 1.f;
        bool        resetHistory = false; // forcer reset (cut camera)
    };

    // =========================================================================
    // NkDenoiserSystem
    // =========================================================================
    class NkDenoiserSystem {
    public:
        NkDenoiserSystem()  = default;
        ~NkDenoiserSystem();

        // ── Cycle de vie ──────────────────────────────────────────────────────
        bool Init(NkIDevice*            device,
                  NkDenoiserBackend     preferredBackend = NkDenoiserBackend::NK_DENOISE_AUTO,
                  const NkDenoiserConfig& cfg = {});
        void Shutdown();

        // ── Débruitage ────────────────────────────────────────────────────────
        // Tous les handles doivent être valides (sauf ceux marqués optionnels).
        // Exécuté sur le command buffer fourni (compute ou transfer).
        void Denoise(NkICommandBuffer* cmd, const NkDenoiseRequest& req);

        // Version rapide sans auxiliaires (color uniquement)
        void DenoiseFast(NkICommandBuffer* cmd,
                          NkTexHandle colorIn, NkTexHandle colorOut,
                          bool resetHistory = false);

        // Invalide l'historique temporel (après cut de caméra)
        void ResetHistory();

        // ── Config ────────────────────────────────────────────────────────────
        void SetConfig(const NkDenoiserConfig& cfg);
        const NkDenoiserConfig& GetConfig() const { return mCfg; }

        // ── Capacités ─────────────────────────────────────────────────────────
        bool                IsSupported()     const { return mSupported; }
        NkDenoiserBackend   GetActiveBackend()const { return mActiveBackend; }
        const NkString&     GetBackendName()  const { return mBackendName; }
        bool                SupportsBackend(NkDenoiserBackend b) const;

    private:
        NkIDevice*        mDevice        = nullptr;
        NkDenoiserConfig  mCfg;
        NkDenoiserBackend mActiveBackend = NkDenoiserBackend::NK_DENOISE_SPATIAL;
        NkString          mBackendName;
        bool              mSupported     = false;
        bool              mReady         = false;

        // Historique temporel (ping-pong)
        NkTexHandle mHistoryBuf[2];
        uint32      mHistoryIdx  = 0;
        bool        mHistoryValid= false;

        NkDenoiserBackend SelectBackend(NkDenoiserBackend preferred);
        bool InitOIDN();
        bool InitNRD();
        bool InitSpatial();
        void RunSpatialFilter(NkICommandBuffer* cmd, const NkDenoiseRequest& req);
        void RunTemporalAccum(NkICommandBuffer* cmd, const NkDenoiseRequest& req);
    };

} // namespace renderer
} // namespace nkentseu
