// =============================================================================
// NkActivations.h — activations (NKAI, Phase 2).
//
// Fines enveloppes des opérations différentiables de NKAutograd, pour un usage
// « couche » lisible : nn::Relu(x), nn::Sigmoid(x), nn::Tanh(x).
// =============================================================================
#pragma once

#include "NKAutograd/NkVar.h"

namespace nkentseu {
    namespace ai {
        namespace nn {

            inline NkVar Relu   (const NkVar& x) { return autograd::Relu(x); }
            inline NkVar Sigmoid(const NkVar& x) { return autograd::Sigmoid(x); }
            inline NkVar Tanh   (const NkVar& x) { return autograd::Tanh(x); }

        } // namespace nn
    } // namespace ai
} // namespace nkentseu
