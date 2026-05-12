#pragma once
// =============================================================================
// NkEnvironmentSystem.h  — NKRenderer v5.0  (Tools/Environment/)
//
// Image-Based Lighting (IBL) ressources :
//   - Irradiance cubemap (diffuse pre-integrated)
//   - Prefilter cubemap  (specular GGX pre-integrated, plusieurs mips)
//   - BRDF LUT 2D        (split-sum)
//
// Etat actuel : D.2a stub fonctionnel.
//   - Les 3 textures sont creees a 1x1 avec valeurs neutres (irradiance gris
//     faible, prefilter noir, BRDF LUT (1,0)) -> le shader PBR peut les bind
//     et compiler sans crash, mais la contribution IBL est minimale.
//   - L'integration GGX et les loaders HDR/EXR arrivent en D.2b.
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRHI/Core/NkIDevice.h"

namespace nkentseu {
    namespace renderer {

        struct NkEnvironmentConfig {
            uint32      irradianceSize  = 32;    // taille du cubemap irradiance (D.2b : 32-64)
            uint32      prefilterSize   = 128;   // taille du cubemap specular (D.2b : 128-512)
            uint32      prefilterMips   = 5;     // niveaux de mips du specular cubemap
            uint32      brdfLUTSize     = 256;   // taille du LUT 2D (D.2b : 256x256)
            // Cache disque : active par defaut, fichiers dans le repertoire courant.
            // Mettre cacheDir = "" ou enableCache = false pour desactiver.
            bool        enableCache    = true;
            const char* cacheDir       = "";     // "" = repertoire courant
        };

        class NkEnvironmentSystem {
            public:
                NkEnvironmentSystem() = default;
                ~NkEnvironmentSystem();

                bool Init(NkIDevice* device, const NkEnvironmentConfig& cfg = {});
                void Shutdown();

                // Genere une cubemap procedurale gradient sky -> horizon -> ground
                // et l'upload dans mIrradiance + mPrefilter. Utile comme placeholder
                // tant que le compute prefilter d'une vraie HDRI n'est pas wire.
                // Default appelle avec sky=bleu clair, horizon=blanc casse, ground=marron.
                void LoadProcedural(const NkVec3f& skyTop,
                                     const NkVec3f& horizon,
                                     const NkVec3f& ground);

                // Accesseurs RHI pour Render3D / NkMaterialSystem (binding 8/9/10 du shader PBR).
                NkTextureHandle GetIrradianceCubemap() const { return mIrradiance; }
                NkTextureHandle GetPrefilterCubemap() const { return mPrefilter; }
                NkTextureHandle GetBRDFLUT()          const { return mBrdfLUT; }
                NkSamplerHandle GetEnvSampler()       const { return mEnvSampler; }
                NkSamplerHandle GetLUTSampler()       const { return mLutSampler; }

            private:
                NkIDevice*       mDevice = nullptr;
                NkEnvironmentConfig mCfg;

                NkTextureHandle  mIrradiance;   // samplerCube (binding 8)
                NkTextureHandle  mPrefilter;    // samplerCube (binding 9, mip-mapped)
                NkTextureHandle  mBrdfLUT;      // sampler2D   (binding 10)

                NkSamplerHandle  mEnvSampler;   // linear clamp pour cubemaps
                NkSamplerHandle  mLutSampler;   // linear clamp pour BRDF LUT
        };

    } // namespace renderer
} // namespace nkentseu
