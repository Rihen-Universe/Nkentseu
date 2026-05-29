#pragma once
// =============================================================================
// CameraMultiDemo.h -- Demo 2 : grille 2x2 de cameras simultanees.
//
// Ouvre jusqu'a 4 cameras via NkMultiCamera et les affiche en split-screen.
// Si moins de 4 cameras sont disponibles, les quadrants restants affichent un
// placeholder "no signal". Touche Echap : quitter.
// =============================================================================

#include "NKWindow/Core/NkEntry.h"

namespace nkentseu {
    namespace cameradem {

        int RunCameraMultiDemo(const NkEntryState& state);

    } // namespace cameradem
} // namespace nkentseu
