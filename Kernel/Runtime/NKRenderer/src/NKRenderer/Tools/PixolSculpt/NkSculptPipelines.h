#pragma once
// =============================================================================
// NkSculptPipelines.h  — NKRenderer v5.0  (Tools/PixolSculpt/)
//
// Registre des pipelines compute du sculpt pixol. S'appuie sur NkComputeContext
// (GetOrCompileGLSL) : les kernels GLSL embarques sont compiles + caches par le
// contexte. Les .comp.vk.glsl du dossier shaders/ restent la reference.
//
// ⚠️ NkSL non utilise pour le chemin actif (GetOrCompileGLSL prend du GLSL).
// =============================================================================
#include "NKRHI/Core/NkTypes.h"
#include "NKRenderer/Tools/PixolSculpt/NkSculptTypes.h"

namespace nkentseu {

    class NkComputeContext; // NKRHI/Core/NkComputeContext.h

    namespace renderer {

        class NkSculptPipelines {
            public:
                NkSculptPipelines() noexcept = default;
                ~NkSculptPipelines() noexcept = default;

                bool Init(NkComputeContext* ctx) noexcept;
                void Shutdown() noexcept;
                [[nodiscard]] bool IsValid() const noexcept { return mCtx != nullptr; }

                // Un seul kernel de brosse (mode via push-constant).
                [[nodiscard]] NkPipelineHandle Brush() noexcept;
                // Composite pixol -> G-buffer.
                [[nodiscard]] NkPipelineHandle Resolve() noexcept;

            private:
                NkComputeContext* mCtx = nullptr;
        };

    } // namespace renderer
} // namespace nkentseu
