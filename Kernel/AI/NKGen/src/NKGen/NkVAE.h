// =============================================================================
// NkVAE.h — auto-encodeur VARIATIONNEL (NKAI, Phase 6).
//
// Différence clé avec l'auto-encodeur simple : l'encodeur produit une DISTRIBUTION
// (moyenne μ + log-variance) au lieu d'un point. On échantillonne le latent
// z = μ + σ·ε (ε ~ N(0,1)), et une pénalité KL rapproche cette distribution de
// N(0,1). Résultat : on peut GÉNÉRER du neuf en tirant z ~ N(0,1) et en décodant —
// créer sans image source. Construit sur NKNN. Namespace nkentseu::ai::gen.
// =============================================================================
#pragma once

#include "NKNN/NkNN.h"
#include "NKAutograd/NkVar.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
    namespace ai {
        namespace gen {

            class NkVAE {
            public:
                NkVAE() = default;
                NkVAE(uint32 inDim, uint32 hidden, uint32 latent, uint32 seed = 1u);

                // Encode -> distribution latente (μ, logσ²), chacun [B, latent].
                void  Encode(const NkVar& x, NkVar& mu, NkVar& logvar) const;
                // Reparamétrisation : z = μ + exp(0.5·logσ²) · ε  (ε fourni, ~N(0,1)).
                NkVar Reparam(const NkVar& mu, const NkVar& logvar, const NkVar& eps) const;
                // Décode un latent -> LOGITS (avant sigmoid), pour la perte BCE.
                NkVar DecodeLogits(const NkVar& z) const;
                // Décode un latent -> reconstruction [B, inDim] dans [0,1] (sigmoid).
                NkVar Decode(const NkVar& z) const;

                void   Parameters(NkVector<NkVar>& out) const;
                uint32 LatentDim() const { return mLatent; }
                uint32 InputDim()  const { return mInDim; }

            private:
                nn::NkDense mEnc1, mEncMu, mEncLogvar;   // encodeur
                nn::NkDense mDec1, mDec2;                 // décodeur
                uint32      mInDim  = 0;
                uint32      mLatent = 0;
            };

            // Divergence KL entre N(μ, σ²) et N(0,1), sommée : renvoie un scalaire.
            //   KL = -0.5 · Σ (1 + logσ² − μ² − exp(logσ²))
            NkVar KLDivergence(const NkVar& mu, const NkVar& logvar);

        } // namespace gen
    } // namespace ai
} // namespace nkentseu
