#pragma once
// =============================================================================
// NkVoxelStroke.h  — NKRenderer v5.0  (Tools/Voxel/)
//
// Accumule les "dabs" 3D le long d'un trace et maintient la DIRTY BOX :
// l'union des boites de voxels touchees depuis le dernier dispatch. C'est CETTE
// boite, bornee par le rayon de la brosse, qui sera dispatchee en compute (3D)
// — d'ou le cout proportionnel au volume edite, pas a la grille entiere.
//
// ⚠️ SQUELETTE — interpolation/espacement des dabs et calcul de la dirty box
//    a implementer.
// =============================================================================
#include "NKContainers/Sequential/NkVector.h"
#include "NKMath/NkMath.h"
#include "NKRenderer/Tools/Voxel/NkVoxelBrush.h"
#include "NKRenderer/Tools/Voxel/NkVoxelTypes.h"

namespace nkentseu {
    namespace renderer {

        using namespace math;

        class NkVoxelStroke {
            public:
                NkVoxelStroke() noexcept = default;
                ~NkVoxelStroke() noexcept = default;

                void Begin(const NkVoxelBrush& brush) noexcept;
                // Ajoute un echantillon en coordonnees voxel ; interpole et genere
                // les dabs espaces de brush.dabSpacing * radius.
                void AddSample(const NkVec3f& centerVox, float32 pressure) noexcept;
                void End() noexcept;

                [[nodiscard]] bool IsActive() const noexcept { return mActive; }
                [[nodiscard]] const NkVector<NkVoxelDab>& PendingDabs() const noexcept { return mPending; }
                [[nodiscard]] NkVoxelBox DirtyBox() const noexcept { return mDirty; }
                [[nodiscard]] const NkVoxelBrush& Brush() const noexcept { return mBrush; }

                void ClearPending() noexcept; // appele apres dispatch
                void Reset() noexcept;

            private:
                // Etend la dirty box pour englober un dab (aligne sur la grille de
                // tuiles kNkVoxelTileSize), clampe aux dimensions de la grille.
                void ExpandDirty(const NkVoxelDab& dab) noexcept;

                NkVoxelBrush          mBrush;
                NkVector<NkVoxelDab>  mPending;
                NkVoxelBox            mDirty;
                NkVec3f               mLastSample = {0, 0, 0};
                bool                  mActive  = false;
                bool                  mHasLast = false;
        };

    } // namespace renderer
} // namespace nkentseu
