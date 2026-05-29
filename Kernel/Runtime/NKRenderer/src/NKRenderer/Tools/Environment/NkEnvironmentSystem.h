#pragma once
// =============================================================================
// NkEnvironmentSystem.h  — NKRenderer v5.0  (Tools/Environment/)
//
// Image-Based Lighting (IBL) ressources :
//   - Irradiance cubemap (diffuse pre-integrated, convolution Lambert)
//   - Prefilter cubemap  (specular GGX pre-integrated, mips par roughness)
//   - BRDF LUT 2D        (split-sum Karis)
//
// Etat actuel : convolution CPU complete (Phase D.2d livree). Le compute GPU
// pour les convolutions est reporté a Phase N v1 (gain init 0.5-2s -> <50ms).
//
// Source IBL parametrable via NkEnvironmentConfig::source :
//   - NK_ENV_PROCEDURAL : gradient sky (skyTop/horizon/ground), default
//   - NK_ENV_HDR_FILE   : .hdr equirect 360 charge depuis hdrPath
//   - NK_ENV_NONE       : pas d'auto-load, l'app appelle LoadProcedural()
//                          ou LoadFromHDR() explicitement plus tard
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
    namespace renderer {

        // Source de l'environnement IBL choisie au Init par l'application.
        enum class NkEnvSource : uint8 {
            NK_ENV_PROCEDURAL = 0,  // gradient sky parametrable (default)
            NK_ENV_HDR_FILE   = 1,  // charge un .hdr equirect 360 depuis hdrPath
            NK_ENV_NONE       = 2,  // pas d'auto-load (textures creées mais vides)
        };

        struct NkEnvironmentConfig {
            uint32      irradianceSize  = 32;    // taille du cubemap irradiance (D.2b : 32-64)
            uint32      prefilterSize   = 128;   // taille du cubemap specular (D.2b : 128-512)
            uint32      prefilterMips   = 5;     // niveaux de mips du specular cubemap
            uint32      brdfLUTSize     = 256;   // taille du LUT 2D (D.2b : 256x256)
            // Cache disque : active par defaut, fichiers dans le repertoire courant.
            // Mettre cacheDir = "" ou enableCache = false pour desactiver.
            bool        enableCache    = true;
            const char* cacheDir       = "";     // "" = repertoire courant

            // ── Source IBL (Phase N v0) ─────────────────────────────────────
            // L'app choisit comment Init() initialise l'IBL. Retro-compat :
            // default = PROCEDURAL avec les couleurs ci-dessous.
            NkEnvSource source         = NkEnvSource::NK_ENV_PROCEDURAL;

            // Parametres du gradient sky (utilises si source == PROCEDURAL).
            NkVec3f     skyTop         = {0.40f, 0.55f, 0.80f};
            NkVec3f     horizon        = {0.45f, 0.48f, 0.52f};
            NkVec3f     ground         = {0.10f, 0.08f, 0.06f};

            // Chemin du .hdr equirect (utilise si source == HDR_FILE).
            // Le fichier doit etre une projection equirectangulaire 360 RGB96F.
            // Ex: "Resources/HDRI/studio.hdr" (PolyHaven ou similaire).
            NkString    hdrPath        = "";
        };

        class NkEnvironmentSystem {
            public:
                NkEnvironmentSystem() = default;
                ~NkEnvironmentSystem();

                bool Init(NkIDevice* device, const NkEnvironmentConfig& cfg = {});
                void Shutdown();

                // Genere une cubemap procedurale gradient sky -> horizon -> ground
                // et l'upload dans mIrradiance + mPrefilter. Utile comme placeholder
                // ou pour des scenes stylisees sans HDR realiste.
                void LoadProcedural(const NkVec3f& skyTop,
                                     const NkVec3f& horizon,
                                     const NkVec3f& ground);

                // Phase N v0 : charge un .hdr equirectangulaire (360 RGB96F) et
                // l'utilise comme source pour les convolutions irradiance +
                // prefilter. CPU-side (tout comme LoadProcedural) ; future v1
                // portera la conversion en compute shader GPU. Retourne true
                // si le chargement et la convolution ont reussi.
                bool LoadFromHDR(const NkString& path);

                // Accesseurs RHI pour Render3D / NkMaterialSystem (binding 8/9/10 du shader PBR).
                NkTextureHandle GetIrradianceCubemap() const { return mIrradiance; }
                NkTextureHandle GetPrefilterCubemap() const { return mPrefilter; }
                NkTextureHandle GetBRDFLUT()          const { return mBrdfLUT; }
                NkSamplerHandle GetEnvSampler()       const { return mEnvSampler; }
                NkSamplerHandle GetLUTSampler()       const { return mLutSampler; }

                // Phase N v1 : cubemap dedie au skybox (RGBA32F, mip 0, sans
                // Reinhard tonemap) pour preserver le vrai dynamic range HDR
                // dans le background. Le tEnvPrefilter (binding=9) reste
                // utilise par le shader PBR pour l'IBL specular (avec tonemap
                // necessaire pour eviter le clamp blanc des reflexions).
                // En source PROCEDURAL, ce cubemap est rempli avec le gradient
                // sky pour que le skybox reste utilisable sans HDR.
                NkTextureHandle GetSkyEnvCube()       const { return mSkyEnvCube; }

            private:
                NkIDevice*       mDevice = nullptr;
                NkEnvironmentConfig mCfg;

                NkTextureHandle  mIrradiance;   // samplerCube (binding 8)
                NkTextureHandle  mPrefilter;    // samplerCube (binding 9, mip-mapped)
                NkTextureHandle  mBrdfLUT;      // sampler2D   (binding 10)
                NkTextureHandle  mSkyEnvCube;   // samplerCube (binding 11, RGBA32F, HDR brut)

                NkSamplerHandle  mEnvSampler;   // linear clamp pour cubemaps
                NkSamplerHandle  mLutSampler;   // linear clamp pour BRDF LUT
        };

    } // namespace renderer
} // namespace nkentseu
