#pragma once
// =============================================================================
// NkVoxelSystem.h  — NKRenderer v5.0  (Tools/Voxel/)
//
// Sous-systeme facade d'edition/rendu d'un volume voxel. Possede le volume, le
// trace, les pipelines. S'enregistre dans le NkRenderGraph (passe NK_COMPUTE
// "Voxel_Edit" puis passe de rendu "Voxel_Raymarch" -> G-buffer), exactement
// comme NkPixolSculptSystem mais en 3D.
//
// Deux voies de rendu (config) :
//   A) RAYMARCH (defaut) : marche le volume en compute -> ecrit dans le G-buffer
//      deferred -> la passe de lighting existante l'eclaire. Simple, "ZBrush-like".
//   B) MESHING (futur) : marching cubes / surface nets en compute -> mesh GPU ->
//      injecte dans NkRender3D (integration pipeline mesh/PBR classique).
//
// ⚠️ SQUELETTE — a implementer / tester plus tard. Voir DESIGN.md (flux + diff
//    d'integration dans NkRendererImpl). Peut partager le volume avec
//    NkVoxelAOSystem (Tools/VoxelAO) plutot que d'en allouer un second.
// =============================================================================
#include "NKMemory/NkUniquePtr.h"
#include "NKMath/NKMath.h"
#include "NKRHI/Core/NkTypes.h"
#include "NKRHI/Core/NkComputeContext.h"
#include "NKRenderer/Tools/Voxel/NkVoxelVolume.h"
#include "NKRenderer/Tools/Voxel/NkVoxelStroke.h"
#include "NKRenderer/Tools/Voxel/NkVoxelPipelines.h"
#include "NKRenderer/Tools/Voxel/NkVoxelTypes.h"

namespace nkentseu {

    class NkIDevice;        // NKRHI
    class NkICommandBuffer; // NKRHI

    namespace renderer {

        class NkRenderGraph;
        class NkTextureLibrary;
        class NkShaderLibrary;

        class NkVoxelSystem {
            public:
                NkVoxelSystem() noexcept = default;
                ~NkVoxelSystem() noexcept;

                bool Init(NkIDevice* device, NkRenderGraph* graph,
                          NkTextureLibrary* texLib, NkShaderLibrary* shaderLib,
                          const NkVoxelConfig& cfg = {}) noexcept;
                void Shutdown() noexcept;
                [[nodiscard]] bool IsValid() const noexcept { return mReady; }

                // Enregistre les passes (Edit compute + Raymarch) dans le graph.
                void RegisterToRenderGraph() noexcept;

                // Camera (pour le raymarch) : matrice inverse view-proj + position.
                void SetCamera(const NkMat4f& invViewProj, const NkVec3f& camPosWorld) noexcept;

                // ── API d'edition (monde -> voxel converti en interne) ───────────
                void SetBrush(const NkVoxelBrush& brush) noexcept;
                [[nodiscard]] const NkVoxelBrush& GetBrush() const noexcept { return mBrush; }
                void BeginStrokeWorld(const NkVec3f& posWorld, float32 pressure = 1.f) noexcept;
                void AddStrokeSampleWorld(const NkVec3f& posWorld, float32 pressure = 1.f) noexcept;
                void EndStroke() noexcept;
                void ClearVolume() noexcept;

                // Conversion monde <-> voxel (selon origin + voxelSize de la config).
                [[nodiscard]] NkVec3f WorldToVoxel(const NkVec3f& w) const noexcept;
                [[nodiscard]] NkVec3f VoxelToWorld(const NkVec3f& v) const noexcept;

                [[nodiscard]] NkVoxelVolume&      Volume()       noexcept { return mVolume; }
                [[nodiscard]] const NkVoxelStats& Stats() const  noexcept { return mStats; }

            private:
                void RecordEditPass(NkICommandBuffer* cmd) noexcept;
                void RecordRaymarchPass(NkICommandBuffer* cmd) noexcept;

                NkIDevice*        mDevice  = nullptr;
                NkRenderGraph*    mGraph   = nullptr;
                NkTextureLibrary* mTexLib  = nullptr;
                NkShaderLibrary*  mShaders = nullptr;
                NkVoxelConfig     mCfg;
                bool              mReady = false;

                // mCompute declare AVANT mPipelines : il possede le cache de
                // pipelines, donc il doit etre detruit APRES (ordre inverse de decl).
                NkComputeContext  mCompute;
                NkVoxelVolume     mVolume;
                NkVoxelStroke     mStroke;
                NkVoxelPipelines  mPipelines;
                NkVoxelBrush      mBrush;
                NkVoxelStats      mStats;

                NkMat4f           mInvViewProj = NkMat4f::Identity();
                NkVec3f           mCamPos      = {0, 0, 0};
        };

    } // namespace renderer
} // namespace nkentseu
