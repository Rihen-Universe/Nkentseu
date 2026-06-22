// =============================================================================
// Games/Common/MouPlant.h
// Plante porteuse de fruits (arbre ou buisson) partagée par les jeux : dessine
// la texture dans une boîte (ratio préservé) et calcule les emplacements fixes
// des fruits sur le houppier. Un type de fruit = une plante (pas de mélange).
// =============================================================================
#pragma once

#ifndef MOU_PLANT_H
#define MOU_PLANT_H

#include "NKCore/NkTypes.h"

namespace mou {

    struct MouFrame;

    class MouPlant {
    public:
        // Ratios d'aspect des assets (largeur/hauteur).
        static constexpr nkentseu::float32 TREE_AR = 360.f / 408.f;
        static constexpr nkentseu::float32 BUSH_AR = 300.f / 230.f;

        // Dessine `tex` (ratio `aspectWH`) centré dans (x,y,w,h) sans déformation.
        // Renvoie le rect réellement dessiné via out*.
        static void Draw(const MouFrame& f, nkentseu::uint32 tex, nkentseu::float32 aspectWH,
                         nkentseu::float32 x, nkentseu::float32 y, nkentseu::float32 w, nkentseu::float32 h,
                         nkentseu::float32& outX, nkentseu::float32& outY,
                         nkentseu::float32& outW, nkentseu::float32& outH) noexcept;

        // Calcule n positions de fruits (coin haut-gauche, taille fruitSz) sur le
        // houppier d'une plante dont le rect dessiné est (rx,ry,rw,rh).
        // canopyCYr/BWr/BHr : centre Y + taille de la boîte (ratios du rect).
        static void Slots(nkentseu::float32 rx, nkentseu::float32 ry, nkentseu::float32 rw, nkentseu::float32 rh,
                          nkentseu::float32 canopyCYr, nkentseu::float32 canopyBWr, nkentseu::float32 canopyBHr,
                          nkentseu::int32 n, nkentseu::float32 fruitSz,
                          nkentseu::float32* outX, nkentseu::float32* outY) noexcept;
    };

}  // namespace mou

#endif // MOU_PLANT_H
