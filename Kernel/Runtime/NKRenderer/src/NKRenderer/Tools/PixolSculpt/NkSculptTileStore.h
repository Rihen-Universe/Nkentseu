#pragma once
// =============================================================================
// NkSculptTileStore.h  — NKRenderer v5.0  (Tools/PixolSculpt/)
//
// ⚠️⚠️ FUTUR / HORS-MVP — squelette de reflexion uniquement.
//
// Le MVP PixolSculpt sculpte en ESPACE-ECRAN (cout borne par la resolution).
// Ce fichier decrit la voie "VRAIE GEOMETRIE multiresolution", calquee sur la
// strategie de ZBrush pour tenir des subdivisions enormes sans exploser la RAM :
//
//   1) On ne stocke PAS N maillages pleins, mais : cage de base + un champ de
//      DELTAS quantifies (16 bits) par niveau de subdivision.
//   2) On ne materialise en triangles que la FENETRE ACTIVE (zone sous l'outil)
//      — analogie "Sculpt HD" / HD Geometry de ZBrush. Le reste reste compact.
//   3) Working set borne par la zone de travail, pas par la taille de l'asset.
//
// Aucune de ces classes n'est utilisee par le MVP. A reprendre plus tard.
// =============================================================================
#include "NKContainers/Sequential/NkVector.h"
#include "NKMath/NKMath.h"
#include "NKRenderer/Tools/PixolSculpt/NkSculptTypes.h"

namespace nkentseu {
    namespace renderer {

        using namespace math;

        // Delta de position quantifie sur 16 bits par composante (relatif a la
        // surface subdivisee lissee). ~6 octets utiles au lieu de 12 en float32.
        struct NkSculptDelta {
            int16  dx = 0, dy = 0, dz = 0;
            uint16 _pad = 0;
        };

        // Une tuile de geometrie HD : un patch local subdivise, potentiellement
        // non-resident (streame a la demande quand l'outil l'approche).
        struct NkSculptTile {
            uint32 baseVertex = 0;     ///< Premier sommet dans le store.
            uint32 vertexCount = 0;
            uint8  lod = 0;            ///< Niveau de subdivision de la tuile.
            bool   resident = false;   ///< Triangles materialises en RAM/VRAM ?
            bool   dirty = false;      ///< Deltas modifies depuis le dernier "bake".
            uint8  _pad = 0;
        };

        // Store multiresolution streame. NON IMPLEMENTE.
        class NkSculptTileStore {
            public:
                NkSculptTileStore()  noexcept = default;
                ~NkSculptTileStore() noexcept = default;

                // bool Init(NkIDevice* device, uint32 baseVertexCount, uint8 maxLod) noexcept;
                // void Shutdown() noexcept;

                // Charge/decharge les tuiles selon la fenetre active (monde).
                // C'est l'equivalent du "Sculpt HD" localise de ZBrush.
                // void SetActiveRegion(const NkAabb& worldRegion) noexcept;

                // Applique un delta a la tuile active (mode geometrie).
                // void ApplyDelta(uint32 tileIndex, uint32 localVertex, const NkVec3f& d) noexcept;

                // "Cuit" les deltas residents en geometrie GPU et libere les
                // tuiles hors fenetre (decharge => RAM bornee).
                // void BakeAndEvict() noexcept;

            private:
                // NkVector<NkSculptTile>  mTiles;
                // NkVector<NkSculptDelta> mDeltas;  // compact, toutes tuiles confondues
                // ... TODO
        };

    } // namespace renderer
} // namespace nkentseu
