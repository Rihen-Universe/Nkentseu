#pragma once
// =============================================================================
// NkVoxelBrush.h  — NKRenderer v5.0  (Tools/Voxel/)
//
// Brosse 3D et "dab" (tampon unique le long d'un trace 3D). NkVoxelBrushGPU est
// le bloc push-constant pousse au kernel ; son layout DOIT matcher le bloc du
// shader voxel_edit (std430).
//
// ⚠️ SQUELETTE — structures de donnees uniquement.
// =============================================================================
#include "NKMath/NKMath.h"
#include "NKRenderer/Tools/Voxel/NkVoxelTypes.h"

namespace nkentseu {
    namespace renderer {

        using namespace math;

        // Parametres d'une brosse, cote CPU / outil.
        struct NkVoxelBrush {
            NkVoxelBrushMode mode      = NkVoxelBrushMode::NK_ADD;
            NkVoxelFalloff   falloff   = NkVoxelFalloff::NK_SMOOTH;
            float32          radiusVox = 8.f;        ///< Rayon en voxels.
            float32          strength  = 0.5f;       ///< Intensite [0..1].
            NkVec4f          color     = {1, 1, 1, 1};///< Pour NK_PAINT.
        };

        // Un tampon unique (centre en espace-grille = coordonnees voxel).
        struct NkVoxelDab {
            NkVec3f centerVox = {0, 0, 0}; ///< Centre en voxels (peut etre fractionnaire).
            float32 radiusVox = 8.f;
            float32 pressure  = 1.f;       ///< Pression stylet [0..1].
        };

        // ─────────────────────────────────────────────────────────────────────
        // Bloc GPU (push constants). Aligne 16 octets, layout std430.
        // ⚠️ Toute modif ici doit etre repercutee dans voxel_edit.*.
        // ─────────────────────────────────────────────────────────────────────
        struct NkVoxelBrushGPU {
            NkVec3f center;       // offset 0   — centre en voxels
            float32 radius;       // offset 12
            NkVec4f color;        // offset 16  — albedo (paint)
            uint32  mode;         // offset 32  — NkVoxelBrushMode
            uint32  falloff;      // offset 36  — NkVoxelFalloff
            float32 strength;     // offset 40
            float32 _pad0;        // offset 44
            int32   boxOffsetX;   // offset 48  — origine de la boite dispatchee
            int32   boxOffsetY;   // offset 52
            int32   boxOffsetZ;   // offset 56
            uint32  _pad1;        // offset 60  (taille 64, align 16)
        };

        // Remplit le bloc push-constant a partir de la brosse + du dab + de
        // l'origine de la boite dispatchee. La pression module l'intensite.
        inline NkVoxelBrushGPU MakeVoxelGPU(const NkVoxelBrush& b, const NkVoxelDab& dab,
                                            int32 boxX, int32 boxY, int32 boxZ) noexcept {
            NkVoxelBrushGPU g{};
            g.center     = dab.centerVox;
            g.radius     = dab.radiusVox;
            g.color      = b.color;
            g.mode       = (uint32)b.mode;
            g.falloff    = (uint32)b.falloff;
            g.strength   = b.strength * dab.pressure;
            g.boxOffsetX = boxX;
            g.boxOffsetY = boxY;
            g.boxOffsetZ = boxZ;
            return g;
        }

    } // namespace renderer
} // namespace nkentseu
