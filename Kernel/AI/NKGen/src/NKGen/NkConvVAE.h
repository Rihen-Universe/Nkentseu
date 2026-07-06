// =============================================================================
// NkConvVAE.h — VAE CONVOLUTIONNEL (NKAI, Phase 6).
//
// Comme NkVAE, mais l'encodeur/décodeur sont CONVOLUTIONNELS (poids partagés
// spatialement) : encode la structure locale, monte en résolution via conv
// transposée. C'est l'architecture des vrais générateurs d'images/3D. Ici sur des
// images 1×8×8 : Conv->pool->latent, latent->ConvTranspose->image. Namespace gen.
//
//   Encode : Conv(1->C,3,pad1) -> ReLU -> MaxPool(2) [C,4,4] -> flatten -> μ,logσ²
//   Decode : z -> Dense -> [C,4,4] -> ConvTranspose(C->1, k2 s2) [1,8,8] -> sigmoid
// =============================================================================
#pragma once

#include "NKNN/NkNN.h"
#include "NKAutograd/NkVar.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
    namespace ai {
        namespace gen {

            class NkConvVAE {
            public:
                NkConvVAE() = default;
                // imgW = côté image (carrée, 1 canal), channels = canaux conv, latent = dim latente.
                NkConvVAE(uint32 imgW, uint32 channels, uint32 latent, uint32 seed = 1u);

                void  Encode(const NkVar& x, NkVar& mu, NkVar& logvar) const; // x [B,1,W,W]
                NkVar Reparam(const NkVar& mu, const NkVar& logvar, const NkVar& eps) const;
                NkVar DecodeLogits(const NkVar& z) const;                     // avant sigmoid (BCE)
                NkVar Decode(const NkVar& z) const;                           // -> [B,1,W,W] dans [0,1]

                void   Parameters(NkVector<NkVar>& out) const;
                uint32 LatentDim() const { return mLatent; }

            private:
                nn::NkConv2D  mEncConv;             // encodeur conv
                nn::NkDense   mEncMu, mEncLogvar;
                nn::NkDense   mDecFc;               // latent -> features spatiales
                nn::NkConv2D  mDecConv;             // resize-conv : Upsample2x puis Conv (sans artefacts)
                uint32 mImgW = 0, mChannels = 0, mLatent = 0, mPooled = 0; // mPooled = W/2
            };

        } // namespace gen
    } // namespace ai
} // namespace nkentseu
