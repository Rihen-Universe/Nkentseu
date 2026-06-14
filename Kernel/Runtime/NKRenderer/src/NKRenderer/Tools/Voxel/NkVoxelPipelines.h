#pragma once
// =============================================================================
// NkVoxelPipelines.h  — NKRenderer v5.0  (Tools/Voxel/)
//
// Registre des pipelines compute du voxel. S'appuie sur NkComputeContext :
// les kernels GLSL (Vulkan-style) sont compiles + mis en cache par le contexte
// (GetOrCompileGLSL), donc PAS d'I/O fichier ici. Les .comp.vk.glsl du dossier
// shaders/ restent la source de reference ; les chaines embarquees ci-dessous
// doivent rester synchronisees (ou, plus tard, chargees depuis le fichier).
//
// ⚠️ NkSL : on pourrait passer par NkSL (image3D/local_size 3D supportes), mais
//    GetOrCompileGLSL prend du GLSL inline -> on garde le GLSL canonique.
// =============================================================================
#include "NKRHI/Core/NkTypes.h"
#include "NKRenderer/Tools/Voxel/NkVoxelTypes.h"

namespace nkentseu {

    class NkComputeContext; // NKRHI/Core/NkComputeContext.h

    namespace renderer {

        class NkVoxelPipelines {
            public:
                NkVoxelPipelines() noexcept = default;
                ~NkVoxelPipelines() noexcept = default;

                // ctx : contexte compute du systeme (proprietaire du cache de pipelines).
                bool Init(NkComputeContext* ctx) noexcept;
                void Shutdown() noexcept;
                [[nodiscard]] bool IsValid() const noexcept { return mCtx != nullptr; }

                // Pipeline d'edition (un seul kernel ; le mode est en push-constant).
                [[nodiscard]] NkPipelineHandle Edit() noexcept;
                // Pipeline de raymarch (volume -> G-buffer).
                [[nodiscard]] NkPipelineHandle Raymarch() noexcept;

            private:
                NkComputeContext* mCtx = nullptr;
        };

    } // namespace renderer
} // namespace nkentseu
