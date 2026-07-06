// =============================================================================
// NkVoxelVAE.h — VAE 3D convolutionnel pour VOXELS (NKAI, Phase 6).
//
// Génère des FORMES 3D depuis du bruit : encodeur Conv3D (downsample par stride),
// latent (μ, logσ²), décodeur ConvTranspose3D (upsample). Tire z ~ N(0,1),
// décode -> grille de voxels -> (maillage via NkMesh). Sur des volumes 1×W³.
// Namespace nkentseu::ai::gen.
//
//   Encode : Conv3D(1->C,3,s2,p1) [C,W/2³] -> ReLU -> flatten -> μ, logσ²
//   Decode : z -> Dense -> [C,W/2³] -> ConvTranspose3D(C->1,k2,s2) [1,W³] -> sigmoid
// =============================================================================
#pragma once

#include "NKNN/NkNN.h"
#include "NKAutograd/NkVar.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
    namespace ai {
        namespace gen {

            class NkVoxelVAE {
            public:
                NkVoxelVAE() = default;
                NkVoxelVAE(uint32 gridW, uint32 channels, uint32 latent, uint32 seed = 1u);

                void  Encode(const NkVar& x, NkVar& mu, NkVar& logvar) const;  // x [B,1,W,W,W]
                NkVar Reparam(const NkVar& mu, const NkVar& logvar, const NkVar& eps) const;
                NkVar Decode(const NkVar& z) const;                            // -> [B,1,W,W,W]

                void   Parameters(NkVector<NkVar>& out) const;
                uint32 LatentDim() const { return mLatent; }

            private:
                nn::NkConv3D          mEncConv;
                nn::NkDense           mEncMu, mEncLogvar;
                nn::NkDense           mDecFc;
                nn::NkConvTranspose3D mDecDeconv;
                uint32 mGridW = 0, mChannels = 0, mLatent = 0, mPooled = 0;
            };

        } // namespace gen
    } // namespace ai
} // namespace nkentseu
