// =============================================================================
// Games/Common/MouBackground.h
// Décor de fond partagé par les jeux : ciel + sol + soleil + nuages, décliné
// par "monde" (thème). Doux et pâle pour ne pas gêner la lisibilité du jeu.
// =============================================================================
#pragma once

#ifndef MOU_BACKGROUND_H
#define MOU_BACKGROUND_H

#include "NKCore/NkTypes.h"

namespace mou {

    struct MouFrame;

    class MouBackground {
    public:
        // theme : 0 = la cour (jour), 1 = le marché (chaud), 2 = la fête (soir).
        static void Draw(const MouFrame& frame, nkentseu::int32 theme, nkentseu::float32 time) noexcept;
        // Nombre de thèmes disponibles (pour boucler par niveau/monde).
        static constexpr nkentseu::int32 ThemeCount = 3;
    };

}  // namespace mou

#endif // MOU_BACKGROUND_H
