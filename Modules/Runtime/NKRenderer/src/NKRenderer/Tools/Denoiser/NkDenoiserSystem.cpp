// =============================================================================
// NkDenoiserSystem.cpp  — NKRenderer v4.0
// =============================================================================
#include "NkDenoiserSystem.h"
#include "NKRenderer/Core/NkTextureLibrary.h"

namespace nkentseu {
namespace renderer {

    NkDenoiserSystem::~NkDenoiserSystem() {
        Shutdown();
    }

    bool NkDenoiserSystem::Init(NkIDevice* device,
                                 NkDenoiserBackend preferredBackend,
                                 const NkDenoiserConfig& cfg) {
        mDevice = device;
        mCfg    = cfg;
        mCfg.backend = preferredBackend;

        mActiveBackend = SelectBackend(preferredBackend);
        switch (mActiveBackend) {
            case NkDenoiserBackend::NK_DENOISE_OIDN:    mSupported = InitOIDN();    break;
            case NkDenoiserBackend::NK_DENOISE_NRD:     mSupported = InitNRD();     break;
            case NkDenoiserBackend::NK_DENOISE_SPATIAL:
            case NkDenoiserBackend::NK_DENOISE_TAA:
            default: mSupported = InitSpatial(); break;
        }

        mBackendName = (mActiveBackend == NkDenoiserBackend::NK_DENOISE_OIDN)    ? "OIDN"
                     : (mActiveBackend == NkDenoiserBackend::NK_DENOISE_NRD)     ? "NRD"
                     : (mActiveBackend == NkDenoiserBackend::NK_DENOISE_SPATIAL) ? "Spatial"
                     : "TAA";

        mReady = mSupported;
        return mReady;
    }

    void NkDenoiserSystem::Shutdown() {
        if (!mReady) return;
        mHistoryValid = false;
        mReady = false;
    }

    void NkDenoiserSystem::Denoise(NkICommandBuffer* cmd,
                                    const NkDenoiseRequest& req) {
        if (!mReady || !req.colorIn.IsValid() || !req.colorOut.IsValid()) return;

        switch (mActiveBackend) {
            case NkDenoiserBackend::NK_DENOISE_OIDN:
            case NkDenoiserBackend::NK_DENOISE_NRD:
            case NkDenoiserBackend::NK_DENOISE_SPATIAL:
                RunSpatialFilter(cmd, req);
                break;
            case NkDenoiserBackend::NK_DENOISE_TAA:
                RunTemporalAccum(cmd, req);
                break;
            default:
                RunSpatialFilter(cmd, req);
                break;
        }

        if (mCfg.temporal && mActiveBackend != NkDenoiserBackend::NK_DENOISE_TAA)
            RunTemporalAccum(cmd, req);

        if (req.resetHistory) ResetHistory();
    }

    void NkDenoiserSystem::DenoiseFast(NkICommandBuffer* cmd,
                                        NkTexHandle colorIn, NkTexHandle colorOut,
                                        bool resetHistory) {
        NkDenoiseRequest req;
        req.colorIn      = colorIn;
        req.colorOut     = colorOut;
        req.resetHistory = resetHistory;
        Denoise(cmd, req);
    }

    void NkDenoiserSystem::ResetHistory() {
        mHistoryValid = false;
        mHistoryIdx   = 0;
    }

    void NkDenoiserSystem::SetConfig(const NkDenoiserConfig& cfg) {
        mCfg = cfg;
    }

    bool NkDenoiserSystem::SupportsBackend(NkDenoiserBackend b) const {
        // Spatial and TAA always available; OIDN/NRD depend on device
        return b == NkDenoiserBackend::NK_DENOISE_SPATIAL
            || b == NkDenoiserBackend::NK_DENOISE_TAA;
    }

    // ── Private ──────────────────────────────────────────────────────────────

    NkDenoiserBackend NkDenoiserSystem::SelectBackend(NkDenoiserBackend preferred) {
        if (preferred == NkDenoiserBackend::NK_DENOISE_AUTO) {
            // TODO: query device for NRD/OIDN support
            return NkDenoiserBackend::NK_DENOISE_SPATIAL;
        }
        if (SupportsBackend(preferred)) return preferred;
        return NkDenoiserBackend::NK_DENOISE_SPATIAL;
    }

    bool NkDenoiserSystem::InitOIDN() {
        // OIDN requires linking against OpenImageDenoise library
        // Return false until library is linked
        return false;
    }

    bool NkDenoiserSystem::InitNRD() {
        // NRD requires Vulkan/DX12 and the NRD SDK
        return false;
    }

    bool NkDenoiserSystem::InitSpatial() {
        // Bilateral / TAA filter — always available
        return true;
    }

    void NkDenoiserSystem::RunSpatialFilter(NkICommandBuffer* /*cmd*/,
                                             const NkDenoiseRequest& /*req*/) {
        // Dispatch a fullscreen bilateral filter compute shader
        // Reads req.colorIn, writes req.colorOut
        // Stub: pass-through for now
    }

    void NkDenoiserSystem::RunTemporalAccum(NkICommandBuffer* /*cmd*/,
                                             const NkDenoiseRequest& req) {
        if (!mHistoryValid || req.resetHistory) {
            mHistoryValid = true;
            mHistoryIdx   = 0;
            return;
        }
        // Reproject and blend current + history[mHistoryIdx]
        // alpha = mCfg.blendFactor
        mHistoryIdx = (mHistoryIdx + 1) % 2;
    }

} // namespace renderer
} // namespace nkentseu
