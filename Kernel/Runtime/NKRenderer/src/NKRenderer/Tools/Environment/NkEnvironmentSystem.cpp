// =============================================================================
// NkEnvironmentSystem.cpp  — NKRenderer v5.0
//
// D.2a stub : creation des 3 textures IBL avec valeurs neutres.
//   - Irradiance cubemap 1x1 (6 faces) en gris foncé -> ambient minimal
//   - Prefilter  cubemap 1x1 (6 faces) noir          -> pas de spec env
//   - BRDF LUT   2D 1x1 valeur (1, 0)                 -> spec * 1 + 0 = F
//
// Note PBR : avec un BRDF LUT (1, 0), la formule split-sum donne
//   pref * (Fi * 1 + 0) = pref * Fi
// Et comme pref = 0 (cubemap noir), l'ensemble du terme spec env est 0.
// L'ambient diffus reste juste "kDi * irradiance * albedo" qui contribue
// faiblement (irradiance gris ~0.04). Suffisant pour ne pas avoir un rendu
// purement noir hors lumiere directe.
// =============================================================================
#include "NkEnvironmentSystem.h"
#include <cstdint>
#include <cstring>

namespace nkentseu {
    namespace renderer {

        NkEnvironmentSystem::~NkEnvironmentSystem() { Shutdown(); }

        bool NkEnvironmentSystem::Init(NkIDevice* device, const NkEnvironmentConfig& cfg) {
            mDevice = device;
            mCfg    = cfg;

            // Pour le stub, on cree des textures 1x1 quelle que soit la config.
            // En D.2b on respectera cfg.irradianceSize/prefilterSize/etc.

            // ── Irradiance cubemap (1x1x6, RGBA8 = ambient gris faible) ──
            {
                auto td = NkTextureDesc::Cubemap(1, NkGPUFormat::NK_RGBA8_UNORM, 1);
                td.debugName = "EnvIrradiance_Stub";
                mIrradiance = mDevice->CreateTexture(td);
                if (mIrradiance.IsValid()) {
                    // Ambient gris foncé legerement bleute (~0.04) pour eviter le noir
                    // pur hors lumiere directe. RGBA8 : (10, 10, 13, 255).
                    const uint8_t pixel[4] = {10, 10, 13, 255};
                    for (uint32 face = 0; face < 6; face++) {
                        mDevice->WriteTextureRegion(mIrradiance, pixel,
                                                     0, 0, 0, 1, 1, 1,
                                                     /*mip*/0, /*layer*/face);
                    }
                }
            }

            // ── Prefilter cubemap (1x1x6, noir) ──
            {
                auto td = NkTextureDesc::Cubemap(1, NkGPUFormat::NK_RGBA8_UNORM, 1);
                td.debugName = "EnvPrefilter_Stub";
                mPrefilter = mDevice->CreateTexture(td);
                if (mPrefilter.IsValid()) {
                    const uint8_t pixel[4] = {0, 0, 0, 255};
                    for (uint32 face = 0; face < 6; face++) {
                        mDevice->WriteTextureRegion(mPrefilter, pixel,
                                                     0, 0, 0, 1, 1, 1,
                                                     /*mip*/0, /*layer*/face);
                    }
                }
            }

            // ── BRDF LUT (2D 1x1, RG = (255, 0)) ──
            {
                auto td = NkTextureDesc::Tex2D(1, 1, NkGPUFormat::NK_RG8_UNORM, 1);
                td.debugName = "BRDFLUT_Stub";
                mBrdfLUT = mDevice->CreateTexture(td);
                if (mBrdfLUT.IsValid()) {
                    const uint8_t pixel[2] = {255, 0};
                    mDevice->WriteTexture(mBrdfLUT, pixel);
                }
            }

            // ── Samplers : linear clamp ──
            mEnvSampler = mDevice->CreateSampler(NkSamplerDesc::Clamp());
            mLutSampler = mDevice->CreateSampler(NkSamplerDesc::Clamp());

            return mIrradiance.IsValid() && mPrefilter.IsValid() && mBrdfLUT.IsValid();
        }

        void NkEnvironmentSystem::Shutdown() {
            if (mLutSampler.IsValid()) { mDevice->DestroySampler(mLutSampler); mLutSampler={}; }
            if (mEnvSampler.IsValid()) { mDevice->DestroySampler(mEnvSampler); mEnvSampler={}; }
            if (mBrdfLUT.IsValid())    { mDevice->DestroyTexture(mBrdfLUT);    mBrdfLUT={}; }
            if (mPrefilter.IsValid())  { mDevice->DestroyTexture(mPrefilter);  mPrefilter={}; }
            if (mIrradiance.IsValid()) { mDevice->DestroyTexture(mIrradiance); mIrradiance={}; }
        }

    } // namespace renderer
} // namespace nkentseu
