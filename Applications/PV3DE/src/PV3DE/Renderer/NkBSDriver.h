#pragma once
// =============================================================================
// PV3DE/Renderer/NkBSDriver.h
// =============================================================================
// Pilote GPU des Blendshapes (Morph Targets).
// Reçoit le vecteur de poids [0,1] depuis NkFaceController,
// les upload chaque frame dans un UBO/SSBO sur le GPU.
//
// Architecture :
//   NkFaceController → float32[] weights
//         ↓
//   NkBSDriver::Upload(weights, count)
//         ↓
//   GPU Buffer (UBO layout std140 ou SSBO)
//         ↓
//   Skin.vert — applique les deltas position/normal/tangent
//
// Paramètres GPU :
//   binding 3 : BS_WEIGHTS_BUFFER
//   uniform   : u_bsWeights[MAX_BLENDSHAPES]
//   uniform   : u_bsCount
//
// Optimisation :
//   - Upload uniquement si les poids ont changé (dirty tracking)
//   - Batch update : un seul appel GPU par frame même avec 64 BS
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
    namespace pv3de {

        static constexpr nk_uint32 kMaxBlendshapes = 64;

        class NkBSDriver {
        public:
            NkBSDriver() = default;
            ~NkBSDriver() noexcept { Shutdown(); }

            // ── Init / Shutdown ───────────────────────────────────────────────
            bool Init(NkIDevice* device, nk_uint32 blendshapeCount) noexcept;
            void Shutdown() noexcept;

            // ── Upload ────────────────────────────────────────────────────────
            // Copie les poids en CPU et marque dirty.
            // Le vrai upload GPU se fait dans Flush().
            void SetWeights(const nk_float32* weights, nk_uint32 count) noexcept;
            void SetWeights(const NkVector<nk_float32>& weights) noexcept {
                SetWeights(weights.Data(), (nk_uint32)weights.Size());
            }

            // Upload GPU effectif — appelé une fois par frame avant le draw.
            void Flush(NkICommandBuffer* cmd) noexcept;

            // ── Bind ──────────────────────────────────────────────────────────
            // Lie le buffer au slot de binding 3 pour le shader Skin.vert
            void Bind(NkICommandBuffer* cmd) noexcept;
            void Unbind(NkICommandBuffer* cmd) noexcept;

            // ── Accès ─────────────────────────────────────────────────────────
            nk_uint32 GetBlendshapeCount() const noexcept { return mCount; }
            bool      IsDirty()            const noexcept { return mDirty; }

        private:
            NkIDevice*       mDevice  = nullptr;
            nk_uint32        mCount   = 0;
            bool             mDirty   = false;

            nk_float32       mWeightsCPU[kMaxBlendshapes] = {};

            // Handle GPU — buffer contenant les poids (UBO ou SSBO selon le backend)
            NkBufferHandle   mGPUBuffer;

            static constexpr nk_uint32 kBindingSlot = 3;  // BS_WEIGHTS_BUFFER
        };

    } // namespace pv3de
} // namespace nkentseu
