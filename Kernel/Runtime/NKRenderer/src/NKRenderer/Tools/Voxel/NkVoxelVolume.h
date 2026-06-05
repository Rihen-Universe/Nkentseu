#pragma once
// =============================================================================
// NkVoxelVolume.h  — NKRenderer v5.0  (Tools/Voxel/)
//
// Le volume voxel : un jeu de storage images 3D (NK_UNORDERED_ACCESS) —
// densite, materiau, couleur. Pendant 3D du "canvas pixol". Ecrit par les
// kernels de brosse, lu par le raymarch / le meshing.
//
// ⚠️ SQUELETTE — creation/destruction des textures 3D a implementer.
// Pour les TRES gros volumes sans exploser la RAM/VRAM : voir NkVoxelBrickPool
// (volume epars streame, analogie HD geometry de ZBrush en 3D).
// =============================================================================
#include "NKRHI/Core/NkTypes.h"
#include "NKRenderer/Tools/Voxel/NkVoxelTypes.h"

namespace nkentseu {

    class NkIDevice;          // NKRHI
    class NkICommandBuffer;   // NKRHI

    namespace renderer {

        class NkRenderGraph;  // Core/NkRenderGraph.h

        class NkVoxelVolume {
            public:
                NkVoxelVolume() noexcept = default;
                ~NkVoxelVolume() noexcept;

                bool Init(NkIDevice* device, const NkVoxelConfig& cfg) noexcept;
                void Shutdown() noexcept;

                void Clear(NkICommandBuffer* cmd) noexcept;

                [[nodiscard]] bool   IsValid() const noexcept { return mReady; }
                [[nodiscard]] uint32 DimX()    const noexcept { return mCfg.dimX; }
                [[nodiscard]] uint32 DimY()    const noexcept { return mCfg.dimY; }
                [[nodiscard]] uint32 DimZ()    const noexcept { return mCfg.dimZ; }

                // Handles RHI (storage images 3D, NK_UNORDERED_ACCESS).
                [[nodiscard]] NkTextureHandle Density()  const noexcept { return mDensity; }
                [[nodiscard]] NkTextureHandle Material() const noexcept { return mMaterial; }
                [[nodiscard]] NkTextureHandle Color()    const noexcept { return mColor; }

                // Importe les cibles dans le render graph (etat NK_UNORDERED_ACCESS).
                void ImportToGraph(NkRenderGraph* graph) noexcept;

                // NkGraphResId == uint32 (cf. NkRenderGraph.h).
                [[nodiscard]] uint32 ResDensity()  const noexcept { return mResDensity; }
                [[nodiscard]] uint32 ResMaterial() const noexcept { return mResMaterial; }
                [[nodiscard]] uint32 ResColor()    const noexcept { return mResColor; }

            private:
                bool CreateTargets() noexcept;
                void DestroyTargets() noexcept;

                NkIDevice*    mDevice = nullptr;
                NkVoxelConfig mCfg;
                bool          mReady = false;

                NkTextureHandle mDensity, mMaterial, mColor;
                uint32          mResDensity = 0, mResMaterial = 0, mResColor = 0;
        };

    } // namespace renderer
} // namespace nkentseu
