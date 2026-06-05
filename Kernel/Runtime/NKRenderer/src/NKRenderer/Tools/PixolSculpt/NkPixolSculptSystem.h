#pragma once
// =============================================================================
// NkPixolSculptSystem.h  — NKRenderer v5.0  (Tools/PixolSculpt/)
//
// Sous-systeme facade du sculpt en espace-ecran ("pixol"). Possede le canvas
// pixol, l'etat de trace, les pipelines compute. Il s'enregistre dans le
// NkRenderGraph (deux passes NK_COMPUTE : Brush puis Resolve) et expose une API
// de trace a l'application / a l'editeur.
//
// Pattern calque sur NkDeferredPass : Init() -> RegisterToRenderGraph(), puis
// le graph orchestre l'ordre et insere les barriers automatiquement.
//
// ⚠️ SQUELETTE — a implementer / tester plus tard. Voir DESIGN.md pour le flux
//    par frame et le diff d'integration dans NkRendererImpl.
// =============================================================================
#include "NKMemory/NkUniquePtr.h"
#include "NKRHI/Core/NkTypes.h"
#include "NKRHI/Core/NkComputeContext.h"
#include "NKRenderer/Tools/PixolSculpt/NkPixolBuffer.h"
#include "NKRenderer/Tools/PixolSculpt/NkSculptPipelines.h"
#include "NKRenderer/Tools/PixolSculpt/NkSculptStroke.h"
#include "NKRenderer/Tools/PixolSculpt/NkSculptTypes.h"

namespace nkentseu {

    class NkIDevice;        // NKRHI
    class NkICommandBuffer; // NKRHI

    namespace renderer {

        class NkRenderGraph;     // Core/NkRenderGraph.h
        class NkTextureLibrary;  // Core/NkTextureLibrary.h
        class NkShaderLibrary;   // Shader/NkShaderLibrary.h

        class NkPixolSculptSystem {
            public:
                NkPixolSculptSystem() noexcept = default;
                ~NkPixolSculptSystem() noexcept;

                bool Init(NkIDevice* device, NkRenderGraph* graph,
                          NkTextureLibrary* texLib, NkShaderLibrary* shaderLib,
                          uint32 width, uint32 height,
                          const NkPixolSculptConfig& cfg = {}) noexcept;
                void Shutdown() noexcept;
                bool Resize(uint32 width, uint32 height) noexcept;

                [[nodiscard]] bool IsValid() const noexcept { return mReady; }

                // Enregistre les passes compute (Brush + Resolve) dans le graph.
                // A appeler dans BuildDefaultRenderGraph(), AVANT la passe de
                // lighting deferred qui consommera le G-buffer resolu.
                void RegisterToRenderGraph() noexcept;

                // ── API de trace (appelee par l'app / l'outil d'edition) ─────
                void SetBrush(const NkSculptBrush& brush) noexcept;
                [[nodiscard]] const NkSculptBrush& GetBrush() const noexcept { return mBrush; }

                void BeginStroke(const NkVec2f& screenPos, float32 pressure = 1.f) noexcept;
                void AddStrokeSample(const NkVec2f& screenPos, float32 pressure = 1.f) noexcept;
                void EndStroke() noexcept;
                void ClearCanvas() noexcept;

                [[nodiscard]] NkPixolBuffer&       PixolBuffer()       noexcept { return mPixol; }
                [[nodiscard]] const NkSculptStats& Stats() const       noexcept { return mStats; }

            private:
                // Callbacks enregistres dans le graph (executes au moment du
                // NkRenderGraph::Execute). C'est ici que se fait le compute.
                void RecordBrushPass(NkICommandBuffer* cmd) noexcept;
                void RecordResolvePass(NkICommandBuffer* cmd) noexcept;

                NkIDevice*        mDevice  = nullptr;
                NkRenderGraph*    mGraph   = nullptr;
                NkTextureLibrary* mTexLib  = nullptr;
                NkShaderLibrary*  mShaders = nullptr;

                NkPixolSculptConfig mCfg;
                uint32              mWidth = 0, mHeight = 0;
                bool                mReady = false;

                // mCompute declare AVANT mPipelines (proprietaire du cache de
                // pipelines) -> detruit APRES (ordre inverse de declaration).
                NkComputeContext  mCompute;
                NkPixolBuffer     mPixol;
                NkSculptStroke    mStroke;
                NkSculptPipelines mPipelines;
                NkSculptBrush     mBrush;
                NkSculptStats     mStats;

                // SSBO contenant les NkSculptBrushGPU des dabs en attente (mode
                // batch) — alternative aux push constants si beaucoup de dabs.
                NkBufferHandle    mDabBuffer;
        };

    } // namespace renderer
} // namespace nkentseu
