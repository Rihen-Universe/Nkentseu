#pragma once
// =============================================================================
// NkVoxelTypes.h  — NKRenderer v5.0  (Tools/Voxel/)
//
// Types fondamentaux du sous-systeme "Voxel" : edition d'un volume 3D via des
// dispatchs compute bornes a l'AABB de la brosse. C'est le pendant 3D de
// PixolSculpt : au lieu d'un canvas pixol 2D (image2D borne au dirty rect),
// on mute un volume 3D (image3D borne a une boite de voxels). Le cout est borne
// par le VOLUME TOUCHE, pas par la taille totale de la grille.
//
// ⚠️ SQUELETTE — a implementer / tester plus tard. Voir DESIGN.md.
// Depend de : NkSL image3D + local_size 3D (desormais supportes dans NKRHI/SL),
// et de NkTextureDesc::Tex3D (storage 3D, NK_UNORDERED_ACCESS) cote RHI.
// =============================================================================
#include "NKContainers/String/NkString.h"
#include "NKMath/NkMath.h"
#include "NKRHI/Core/NkTypes.h"

namespace nkentseu {
    namespace renderer {

        using namespace math;

        static constexpr uint32 kNkVoxelVersion = 1;

        // Taille de tuile compute 3D par defaut (local_size_x/y/z).
        static constexpr uint32 kNkVoxelTileSize = 4; // 4x4x4 = 64 threads/groupe

        // ─────────────────────────────────────────────────────────────────────
        // Mode de brosse 3D. Chaque mode = un kernel compute distinct.
        // ─────────────────────────────────────────────────────────────────────
        enum class NkVoxelBrushMode : uint8 {
            NK_ADD     = 0,  ///< Ajoute de la densite (depose de la matiere).
            NK_SUB     = 1,  ///< Retire de la densite (creuse).
            NK_PAINT   = 2,  ///< Peint la couleur sans changer la densite.
            NK_SMOOTH  = 3,  ///< Lisse la densite (moyenne du voisinage 3D).
            NK_FLATTEN = 4,  ///< Aplati vers un plan.
            NK_COUNT
        };

        // Profil d'attenuation radiale (centre -> bord, en 3D).
        enum class NkVoxelFalloff : uint8 {
            NK_SMOOTH   = 0,
            NK_LINEAR   = 1,
            NK_CONSTANT = 2,
            NK_SPHERE   = 3,
            NK_COUNT
        };

        // Formats des cibles du volume (storage images 3D, NK_UNORDERED_ACCESS).
        struct NkVoxelFormat {
            NkGPUFormat density  = NkGPUFormat::NK_R16_FLOAT;   ///< Densite signee (SDF-like) ou occupation.
            NkGPUFormat material = NkGPUFormat::NK_RGBA8_UNORM;  ///< id mat / roughness / metallic.
            NkGPUFormat color    = NkGPUFormat::NK_RGBA8_UNORM;  ///< Albedo peint.
        };

        // Config a la creation du sous-systeme.
        struct NkVoxelConfig {
            uint32        dimX = 256, dimY = 256, dimZ = 256; ///< Resolution de la grille.
            float32       voxelSize   = 0.05f;  ///< Unites monde par voxel.
            NkVec3f       originWorld  = {0, 0, 0}; ///< Coin (0,0,0) de la grille en monde.
            NkVoxelFormat formats      = {};
            bool          enableColor  = true;
            bool          resolveToGBuffer = true; ///< Raymarch -> G-buffer deferred.
            uint32        maxDabsPerFrame  = 128;  ///< Borne DURE du travail/frame.
            float32       dabSpacing   = 0.35f;    ///< Espacement des tampons le long du trace (x rayon).
        };

        // Statistiques d'une frame.
        struct NkVoxelStats {
            uint32  dabsDispatched   = 0;
            uint32  bricksDispatched = 0;  ///< Tuiles 3D reellement touchees.
            uint32  voxelsTouched    = 0;  ///< Estimation (bornee par dim^3).
            float32 cpuMs            = 0.f;
            void Reset() noexcept { *this = NkVoxelStats{}; }
        };

        // Boite de voxels en espace-grille : le "dirty region" 3D = working set
        // borne d'une frame. C'est ce qui garantit le cout proportionnel au volume
        // touche, et non a la grille entiere.
        struct NkVoxelBox {
            int32 minX = 0, minY = 0, minZ = 0;
            int32 maxX = 0, maxY = 0, maxZ = 0;
            [[nodiscard]] bool IsEmpty() const noexcept {
                return maxX <= minX || maxY <= minY || maxZ <= minZ;
            }
            [[nodiscard]] int32 Width()  const noexcept { return maxX - minX; }
            [[nodiscard]] int32 Height() const noexcept { return maxY - minY; }
            [[nodiscard]] int32 Depth()  const noexcept { return maxZ - minZ; }
        };

    } // namespace renderer
} // namespace nkentseu
