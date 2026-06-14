#pragma once
// =============================================================================
// NkVoxelBrickPool.h  — NKRenderer v5.0  (Tools/Voxel/)
//
// ⚠️⚠️ FUTUR / HORS-MVP — squelette de reflexion uniquement.
//
// Le MVP voxel utilise UNE grille dense (image3D dimX*dimY*dimZ) : simple, mais
// 512^3 * 4 octets = 512 Mo. Pour des volumes plus gros sans exploser la VRAM,
// ce fichier decrit un VOLUME EPARS en "briques" (l'equivalent 3D du
// NkSculptTileStore / de la HD Geometry de ZBrush) :
//
//   1) L'espace est decoupe en briques (ex. 16^3 voxels). Seules les briques
//      NON VIDES sont allouees dans un pool (atlas 3D ou buffer).
//   2) Une table d'indirection (page table) mappe coord-de-brique -> slot du pool
//      (ou "vide"). Resident only = working set borne par la matiere reelle.
//   3) L'edition/le raymarch consultent la page table ; les briques hors zone
//      active peuvent etre evictees (streaming).
//
// Aucune de ces classes n'est utilisee par le MVP. A reprendre plus tard.
// =============================================================================
#include "NKContainers/Sequential/NkVector.h"
#include "NKMath/NKMath.h"
#include "NKRHI/Core/NkTypes.h"
#include "NKRenderer/Tools/Voxel/NkVoxelTypes.h"

namespace nkentseu {
    namespace renderer {

        using namespace math;

        static constexpr uint32 kNkBrickSize = 16; // 16^3 voxels par brique

        // Une brique : un bloc local de voxels, potentiellement non-resident.
        struct NkVoxelBrick {
            uint32 poolSlot = 0xFFFFFFFFu; ///< Slot dans l'atlas/pool (0xFFFFFFFF = vide).
            bool   resident = false;
            bool   dirty    = false;
            uint8  _pad[2]  = {};
        };

        // Pool de briques epars. NON IMPLEMENTE.
        class NkVoxelBrickPool {
            public:
                NkVoxelBrickPool()  noexcept = default;
                ~NkVoxelBrickPool() noexcept = default;

                // bool Init(NkIDevice* device, uint32 gridBricksX, uint32 y, uint32 z,
                //           uint32 poolCapacity) noexcept;
                // void Shutdown() noexcept;

                // Garantit la residence des briques couvrant une AABB (alloue au besoin).
                // void EnsureResident(const NkVoxelBox& region) noexcept;

                // Libere les briques hors fenetre active (VRAM bornee).
                // void EvictOutside(const NkVoxelBox& keep) noexcept;

                // Resout coord-de-brique -> slot du pool (ou invalide).
                // uint32 SlotOf(int32 bx, int32 by, int32 bz) const noexcept;

            private:
                // NkVector<NkVoxelBrick> mPageTable; // indexee par coord de brique
                // NkTextureHandle        mPoolAtlas; // atlas 3D des briques residentes
                // NkTextureHandle        mPageTableTex; // indirection cote GPU
                // ... TODO
        };

    } // namespace renderer
} // namespace nkentseu
